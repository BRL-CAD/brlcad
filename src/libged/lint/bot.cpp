/*                        B O T . C P P
 * BRL-CAD
 *
 * Copyright (c) 2024 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file bot.cpp
 *
 * Routines specific to invalid BoTs
 */

#include "common.h"

#include <iomanip>
#include <limits>
#include <set>
#include <sstream>
#include <thread>
#include "json.hpp"
#include "concurrentqueue.h"

#include "vmath.h"
#include "rt/application.h"
#include "rt/rt_instance.h"
#include "rt/shoot.h"
#include "rt/primitives/bot.h"

#include "./ged_lint.h"


static bool
bot_face_normal(vect_t *n, struct rt_bot_internal *bot, int i)
{
    vect_t a,b;

    /* sanity */
    if (!n || !bot || i < 0 || (size_t)i > bot->num_faces ||
	    bot->faces[i*3+2] < 0 || (size_t)bot->faces[i*3+2] > bot->num_vertices) {
	return false;
    }

    VSUB2(a, &bot->vertices[bot->faces[i*3+1]*3], &bot->vertices[bot->faces[i*3]*3]);
    VSUB2(b, &bot->vertices[bot->faces[i*3+2]*3], &bot->vertices[bot->faces[i*3]*3]);
    VCROSS(*n, a, b);
    VUNITIZE(*n);
    if (bot->orientation == RT_BOT_CW) {
	VREVERSE(*n, *n);
    }

    return true;
}

static std::string
d2s(double d)
{
    size_t prec = std::numeric_limits<double>::max_digits10;
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(prec) << d;
    std::string sd = ss.str();
    return sd;
}

static void
pt_to_json(nlohmann::json *pc, const char *key, point_t pt)
{
   (*pc)[key]["X"] = d2s(pt[X]);
   (*pc)[key]["Y"] = d2s(pt[Y]);
   (*pc)[key]["Z"] = d2s(pt[Z]);
}

static void
ray_to_json(nlohmann::json *pc, struct xray *r)
{
    nlohmann::json ray;
    pt_to_json(&ray, "P", r->r_pt);
    pt_to_json(&ray, "N", r->r_dir);
    (*pc)["ray"].push_back(ray);
}

static void
tri_to_json(nlohmann::json *pc, struct rt_bot_internal *bot, int ind)
{
    nlohmann::json tri;
    point_t v[3];
    for (int i = 0; i < 3; i++)
	VMOVE(v[i], &bot->vertices[bot->faces[ind*3+i]*3]);

    tri["face_index"] = ind;
    pt_to_json(&tri, "V0", v[0]);
    pt_to_json(&tri, "V1", v[1]);
    pt_to_json(&tri, "V2", v[2]);

    vect_t n = VINIT_ZERO;
    bot_face_normal(&n, bot, ind);
    pt_to_json(&tri, "N", n);

    (*pc)["tris"].push_back(tri);
}


typedef int (*fhit_t)(struct application *, struct partition *, struct seg *);
typedef int (*fmiss_t)(struct application *);

static int
_hit_noop(struct application *UNUSED(ap),
       	struct partition *PartHeadp,
       	struct seg *UNUSED(segs))
{
    if (PartHeadp->pt_forw == PartHeadp)
	return 1;

    return 0;
}
static int
_miss_noop(struct application *UNUSED(ap))
{
    return 0;
}

static int
_overlap_noop(struct application *UNUSED(ap),
	struct partition *UNUSED(pp),
	struct region *UNUSED(reg1),
	struct region *UNUSED(reg2),
	struct partition *UNUSED(hp))
{
    /* I don't think this is supposed to happen with a single primitive, but just
     * in case we get an overlap report somehow complain about it */
    bu_log("WARNING: overlap reported in single BoT raytrace\n");
    return 0;
}

// Per thread data for lint testing
class lint_worker_data {
    public:
	lint_worker_data(struct rt_i *rtip, struct resource *res);
	~lint_worker_data();
	void shoot(int ind, bool reverse);
	void plot_bad_tris(struct bv_vlblock *vbp, struct bu_list *vhead, struct bu_list *vlfree);

	nlohmann::json tresults;
	bool condition_flag = false;
	fastf_t min_tri_area = 0.0;
	int curr_tri = -1;
	double ttol = 0.0;

	lint_data *ldata = NULL;
	struct application ap;
	struct rt_bot_internal *bot = NULL;
	const std::unordered_set<int> *bad_faces = NULL;

	std::string pname;

	// Accumulated set of all faces flagged
	// by any tests using this worker container.
	// Used for plotting, where no distinction
	// is made between various test types
	std::unordered_set<int> flagged_tris;
};

lint_worker_data::lint_worker_data(struct rt_i *rtip, struct resource *res)
{
    RT_APPLICATION_INIT(&ap);
    ap.a_onehit = 0;
    ap.a_rt_i = rtip;             /* application uses this instance */
    ap.a_hit = _hit_noop;         /* where to go on a hit */
    ap.a_miss = _miss_noop;       /* where to go on a miss */
    ap.a_overlap = _overlap_noop; /* where to go if an overlap is found */
    ap.a_onehit = 0;              /* whether to stop the raytrace on the first hit */
    ap.a_resource = res;
    ap.a_uptr = (void *)this;
}

lint_worker_data::~lint_worker_data()
{
}

void
lint_worker_data::shoot(int ind, bool reverse)
{
    if (!bot)
	return;

    // Set curr_tri so the callbacks know what our origin triangle is
    curr_tri = ind;

    // If we already know this face is no good, skip
    if (bad_faces && bad_faces->find(curr_tri) != bad_faces->end())
	return;

    // Minimum area filter check
    if (ldata && ldata->min_tri_area > 0) {
	point_t v[3];
	for (int i = 0; i < 3; i++)
	    VMOVE(v[i], &bot->vertices[bot->faces[curr_tri*3+i]*3]);
	double tri_area = bg_area_of_triangle(v[0], v[1], v[2]);
	if (tri_area < ldata->min_tri_area)
	    return;
    }

    // Triangle passes filters, continue processing
    vect_t rnorm, n, backout;
    if (!bot_face_normal(&n, bot, ind))
	return;
    // Reverse the triangle normal for a ray direction
    VREVERSE(rnorm, n);

    // We want backout to get the ray origin off the triangle surface.  If
    // we're shooting up from the triangle (reverse) we "backout" into the
    // triangle, if we're shooting into the triangle we back out above it.
    if (reverse) {
	// We're reversing for "close" testing, and a close triangle may be
	// degenerately close to our test triangle.  Hence, we back below
	// the surface to be sure.
	VMOVE(backout, rnorm);
	VMOVE(ap.a_ray.r_dir, n);
    } else {
	VMOVE(backout, n);
	VMOVE(ap.a_ray.r_dir, rnorm);
    }
    VSCALE(backout, backout, SQRT_SMALL_FASTF);

    point_t rpnts[3];
    point_t tcenter;
    VMOVE(rpnts[0], &bot->vertices[bot->faces[ind*3+0]*3]);
    VMOVE(rpnts[1], &bot->vertices[bot->faces[ind*3+1]*3]);
    VMOVE(rpnts[2], &bot->vertices[bot->faces[ind*3+2]*3]);
    VADD3(tcenter, rpnts[0], rpnts[1], rpnts[2]);
    VSCALE(tcenter, tcenter, 1.0/3.0);

    // Take the shot
    VADD2(ap.a_ray.r_pt, tcenter, backout);
    (void)rt_shootray(&ap);
};

void
lint_worker_data::plot_bad_tris(struct bv_vlblock *vbp, struct bu_list *vhead, struct bu_list *vlfree)
{
    if (!vbp || !vhead || !vlfree)
	return;

    std::unordered_set<int>::iterator tr_it;

    for (tr_it = flagged_tris.begin(); tr_it != flagged_tris.end(); tr_it++) {
	int tri_ind = *tr_it;
	point_t v[3];
	for (int i = 0; i < 3; i++)
	    VMOVE(v[i], &bot->vertices[bot->faces[tri_ind*3+i]*3]);
	BV_ADD_VLIST(vlfree, vhead, v[0], BV_VLIST_LINE_MOVE);
	BV_ADD_VLIST(vlfree, vhead, v[1], BV_VLIST_LINE_DRAW);
	BV_ADD_VLIST(vlfree, vhead, v[2], BV_VLIST_LINE_DRAW);
	BV_ADD_VLIST(vlfree, vhead, v[0], BV_VLIST_LINE_DRAW);
    }
};

static int
_tc_hit(struct application *ap, struct partition *PartHeadp, struct seg *segs)
{
    if (PartHeadp->pt_forw == PartHeadp)
	return 1;

    lint_worker_data *tinfo = (lint_worker_data *)ap->a_uptr;

    struct seg *s = (struct seg *)segs->l.forw;
    if (s->seg_in.hit_dist > 2*SQRT_SMALL_FASTF) {
	// This is a problem but it's not the thin volume problem - no point in
	// continuing
	return 0;
    }

    for (BU_LIST_FOR(s, seg, &(segs->l))) {
	// We're only interested in thin interactions centering around the
	// triangle in question - other triangles along the shotline will be
	// checked in different shots
	if (s->seg_in.hit_dist > tinfo->ttol)
	    break;

	double dist = s->seg_out.hit_dist - s->seg_in.hit_dist;
	if (dist > VUNITIZE_TOL)
	    continue;

	nlohmann::json terr;
	terr["in_index"] = s->seg_in.hit_surfno;
	terr["out_index"] = s->seg_out.hit_surfno;
	if (tinfo->ldata && tinfo->ldata->verbosity > 1) {
	    ray_to_json(&terr, &ap->a_ray);
	    tri_to_json(&terr, tinfo->bot, s->seg_in.hit_surfno);
	    tri_to_json(&terr, tinfo->bot, s->seg_out.hit_surfno);
	}
	terr["dist"] = d2s(dist);
	tinfo->tresults["errors"].push_back(terr);
	tinfo->condition_flag = true;

	// Plot both the triangles involved
	tinfo->flagged_tris.insert(s->seg_in.hit_surfno);
	tinfo->flagged_tris.insert(s->seg_out.hit_surfno);

	// Debugging - to be removed
	//bu_log("%s tri_ind %d tc hit: %g\n", tinfo->pname.c_str(), tinfo->curr_tri, dist);

    }
    return 0;
}

static int
_tc_miss(struct application *ap)
{
    // A straight-up miss is one of the possible reporting scenarios for thin
    // triangle pairs - if it happens, we need to flag what we can find
    // (unfortunately we only know which triangle prompted the report in this
    // case - hopefully the other triangle will also trigger a thin report and
    // get itself queued for removal.
    lint_worker_data *tinfo = (lint_worker_data *)ap->a_uptr;

    tinfo->condition_flag = true;
    nlohmann::json terr;
    terr["index"] = tinfo->curr_tri;
    if (tinfo->ldata && tinfo->ldata->verbosity > 1) {
	ray_to_json(&terr, &ap->a_ray);
	tri_to_json(&terr, tinfo->bot, tinfo->curr_tri);
    }
    tinfo->tresults["errors"].push_back(terr);

    // Plot the missed triangle
    tinfo->flagged_tris.insert(tinfo->curr_tri);

    // Debugging - to be removed
    //bu_log("%s tri_ind %d tc miss\n", tinfo->pname.c_str(), tinfo->curr_tri);

    return 0;
}

// TODO - A useful correctness audit for a BoT might be to shotline both the
// CSG and the BoT using the same rays constructed from the triangle centers -
// if the CSG reports a non-zero LOS but the BoT reports zero, we have a
// problem.
static int
_ck_up_hit(struct application *ap, struct partition *PartHeadp, struct seg *UNUSED(segs))
{
    if (PartHeadp->pt_forw == PartHeadp)
	return 1;

    lint_worker_data *tinfo = (lint_worker_data *)ap->a_uptr;

    // If we've got something too close above our triangle, it's trouble
    //
    // TODO - validate whether the vector between the two hit points is
    // parallel to the ray.  Saw one case where it seemed as if we were getting
    // an offset that resulted in a higher distance, but only because there was
    // a shift of one of the hit points off the ray by more than ttol
    struct partition *pp = PartHeadp->pt_forw;
    if (pp->pt_inhit->hit_dist > tinfo->ttol)
	return 0;

    nlohmann::json terr;
	terr["in_index"] = pp->pt_inhit->hit_surfno;
	terr["out_index"] = pp->pt_outhit->hit_surfno;
	if (tinfo->ldata && tinfo->ldata->verbosity > 1) {
	    ray_to_json(&terr, &ap->a_ray);
	    tri_to_json(&terr, tinfo->bot, pp->pt_inhit->hit_surfno);
	    tri_to_json(&terr, tinfo->bot, pp->pt_outhit->hit_surfno);
	}
    tinfo->tresults["errors"].push_back(terr);
    tinfo->condition_flag = true;

    // Plot both the triangles involved
    tinfo->flagged_tris.insert(pp->pt_inhit->hit_surfno);
    tinfo->flagged_tris.insert(pp->pt_outhit->hit_surfno);

    // Debugging - to be removed
    //bu_log("%s tri_ind %d close\n", tinfo->pname.c_str(), tinfo->curr_tri);

    return 0;
}

static int
_mc_miss(struct application *ap)
{
    // We are shooting directly into the center of a triangle from right above
    // it.  If we miss under those conditions, it can only happen because
    // something is wrong.
    lint_worker_data *tinfo = (lint_worker_data *)ap->a_uptr;
    nlohmann::json terr;
    terr["index"] = tinfo->curr_tri;
    if (tinfo->ldata && tinfo->ldata->verbosity > 1) {
	ray_to_json(&terr, &ap->a_ray);
	tri_to_json(&terr, tinfo->bot, tinfo->curr_tri);
    }
    tinfo->tresults["errors"].push_back(terr);
    tinfo->condition_flag = true;

    // Flag for plotting
    tinfo->flagged_tris.insert(tinfo->curr_tri);

    // Debugging - to be removed
    //bu_log("%s tri_ind %d missed\n", tinfo->pname.c_str(), tinfo->curr_tri);

    return 0;
}

static int
_uh_hit(struct application *ap, struct partition *PartHeadp, struct seg *segs)
{
    if (PartHeadp->pt_forw == PartHeadp)
	return 1;

    lint_worker_data *tinfo = (lint_worker_data *)ap->a_uptr;

    struct seg *s = (struct seg *)segs->l.forw;
    if (s->seg_in.hit_dist < 2*SQRT_SMALL_FASTF)
	return 0;

    // Segment's first hit didn't come from the expected triangle.
    nlohmann::json terr;
    terr["index"] = s->seg_in.hit_surfno;
    if (tinfo->ldata && tinfo->ldata->verbosity > 1) {
	ray_to_json(&terr, &ap->a_ray);
	tri_to_json(&terr, tinfo->bot, tinfo->curr_tri);
    }
    tinfo->tresults["errors"].push_back(terr);
    tinfo->condition_flag = true;

    // Flag for plotting
    tinfo->flagged_tris.insert(tinfo->curr_tri);

    // Debugging - to be removed
    //bu_log("%s tri_ind %d unexpected hit\n", tinfo->pname.c_str(), tinfo->curr_tri);

    return 0;
}

static int
bot_check(std::vector<lint_worker_data *> &wdata, const char *test_type, fhit_t hf, fmiss_t mf, int onehit, bool reverse)
{
    // Make a std::string of the test type for easier processing
    std::string ttype(test_type);

    // We always need at least one worker data container to do any work at all.
    if (!wdata.size())
	return 0;

    // Pull the values we need for preliminary checks from the first container
    // - the above check guarantees we have at least that one, and if properly
    // prepared all wdata worker data containers will have the same values for
    // any check we will do at this stage.
    lint_worker_data *w0 = wdata[0];
    if (!w0->bot || !w0->bot->num_faces)
	return 0;

    // If we have specific tests called out and this test hasn't been
    // specified, skip.
    const std::map<std::string, std::set<std::string>> &imt = w0->ldata->im_techniques;
    if (imt.size()) {
	std::map<std::string, std::set<std::string>>::const_iterator i_it;
	i_it = imt.find(std::string("bot"));
	if (i_it->second.find(ttype) == i_it->second.end())
	    return 0;
    }

    // Define the common identifying properties for the results container
    nlohmann::json terr;
    terr["problem_type"] = ttype;
    terr["object_type"] = "bot";
    terr["object_name"] = w0->pname;

    // Much of the information needed for different tests is common and thus can be
    // reused, but some aspects are specific to each test - let all the worker data
    // containers know what the specifics are for this test.
    for (size_t i = 0; i < wdata.size(); i++) {
	wdata[i]->ap.a_hit = hf;
	wdata[i]->ap.a_miss = mf;
	wdata[i]->ap.a_onehit = onehit;
    }

    // The concurrent queue is how we will use multiple threads to do
    // raytracing more quickly.  The advantage over bu_parallel here is it
    // should be simple to keep each thread busy - all of them will be pulling
    // their "next" work triangle from the same common source.
    moodycamel::ConcurrentQueue<size_t> q;

    // We only need one "source" of triangles to process, since the list is
    // known in advance and fixed.  Use a ProducerToken, since our use case
    // supports it.
    moodycamel::ProducerToken ptok(q);
    for (size_t i = 0; i < w0->bot->num_faces; i++)
	q.enqueue(ptok, i);

    // Do the work in multiple threads.
    std::vector<std::thread> threads;
    // https://stackoverflow.com/a/39266640
    threads.reserve(wdata.size());
    for (size_t i = 0; i < wdata.size(); i++) {
	// https://stackoverflow.com/a/25536984
	threads.emplace_back(
		std::thread(
		    // Using a lambda function to execute the work
		    [&](int ind)  // lambda scoped variable holding the passed-in wdata index
		    {
		       size_t tri_ind;
		       // As long as there's still work in q to do, have the thread keep getting
		       // more triangles to process
		       while (q.try_dequeue_from_producer(ptok, tri_ind)) {
		          // Perform an individual triangle test
		          wdata[ind]->shoot(tri_ind, reverse);
		       }
		    }
		    , i  // Value being passed into ind to id this thread's wdata container
		    )
		);
    }

    // Wait for all threads
    for (size_t i = 0; i < wdata.size(); i++) {
	threads[i].join();
    }

    // Clean up anything left (TODO - shouldn't be anything, since our producer
    // isn't making new work - this may be unnecessary.)
    size_t tri_ind;
    while (q.try_dequeue_from_producer(ptok, tri_ind)) {
	wdata[0]->shoot(tri_ind, false);
    }

    // Gather the thread results and reset the individual json containers
    bool found = false;
    for (size_t i = 0; i < wdata.size(); i++) {
	nlohmann::json &sdata = wdata[i]->tresults;
	for(nlohmann::json::const_iterator it = sdata.begin(); it != sdata.end(); ++it) {
	    found = true;
	    terr["errors"].push_back(*it);
	}
	wdata[i]->tresults.clear();
    }

    if (found)
	w0->ldata->j.push_back(terr);

    return 0;
}

void
bot_checks(lint_data *bdata, struct directory *dp, struct rt_bot_internal *bot)
{
    if (!bdata || !dp || !bot)
	return;

    std::map<std::string, std::set<std::string>> &imt = bdata->im_techniques;
    if (imt.size() && imt.find(std::string("bot")) == imt.end())
	return;

    if (!bot->num_faces) {

	if (imt.size()) {
	    std::set<std::string> &bt = imt[std::string("bot")];
	    if (bt.find(std::string("empty")) == bt.end())
		return;
	}

	nlohmann::json berr;
	berr["problem_type"] = "empty";
	berr["object_type"] = "bot";
	berr["object_name"] = dp->d_namep;
	bdata->j.push_back(berr);
	return;
    }


    // The remainder of the checks only make sense for solid BoTs.
    if (bot->mode != RT_BOT_SOLID)
	return;


    int not_solid = bg_trimesh_solid2((int)bot->num_vertices, (int)bot->num_faces, bot->vertices, bot->faces, NULL);
    if (not_solid) {
	if (imt.size()) {
	    std::set<std::string> &bt = imt[std::string("bot")];
	    if (bt.find(std::string("not_solid")) == bt.end()) {
		return;
	    }
	}

	nlohmann::json berr;
	berr["problem_type"] = "not_solid";
	berr["object_type"] = "bot";
	berr["object_name"] = dp->d_namep;
	bdata->j.push_back(berr);
	return;
    }

    // TODO check for flipped bot

    // Note that these won't work as expected if the BoT is self-intersecting.
    struct rt_i *rtip = rt_new_rti(bdata->gedp->dbip);
    rt_gettree(rtip, dp->d_namep);
    rt_prep(rtip);

    // If we can't hit a triangle face, there's no point in doing other tests
    // with it - that'll just complicate the reporting results.  If the
    // missing test isn't enabled this filter will be as well, but for the
    // most common case (multi-test) it should help.
    std::unordered_set<int> bad_faces;

    // Grr.  Would prefer to have this managed as a per-worker resource, but
    // the initialization isn't having it.
    size_t ncpus = bu_avail_cpus();
    struct resource *resp = (struct resource *)bu_calloc(ncpus+1, sizeof(struct resource), "resources");

    std::vector<lint_worker_data *> wdata;
    for (size_t i = 0; i < ncpus - 1; i++) {
	rt_init_resource(&resp[i], (int)i, rtip);
	lint_worker_data *d = new lint_worker_data(rtip, &resp[i]);
	d->ldata = bdata;
	d->pname = std::string(dp->d_namep);
	d->bot = bot;
	d->bad_faces = &bad_faces;
	d->ttol = bdata->ftol;
	d->min_tri_area = bdata->min_tri_area;
	wdata.push_back(d);
    }

    /* Note that we are deliberately using onehit=1 for the miss test to check
     * the intersection behavior of the individual triangles */
    bot_check(wdata, "unexpected_miss", _hit_noop, _mc_miss, 1, false);

    // Missed faces get flagged to avoid repeated testing, since they are known
    // not to raytrace appropriately after the above check.
    for (size_t i = 0; i < wdata.size(); i++) {
	std::unordered_set<int>::iterator f_it;
	for (f_it = wdata[i]->flagged_tris.begin(); f_it != wdata[i]->flagged_tris.end(); f_it++)
	    bad_faces.insert(*f_it);
    }

    /* Thin face pairings are a common artifact of coplanar faces in boolean
     * evaluations */
    bot_check(wdata, "thin_volume", _tc_hit, _tc_miss, 0, false);

    /* When testing for faces that are too close to a given face, we need to
     * reverse the ray direction */
    bot_check(wdata, "close_face", _ck_up_hit, _miss_noop, 0, true);

    /* Checking for the case where we end up with a hit from a triangle other
     * than the one we derive the ray from. */
    bot_check(wdata, "unexpected_hit", _uh_hit, _miss_noop, 0, false);

    if (bdata->do_plot) {
	struct bu_color *color = bdata->color;
	struct bv_vlblock *vbp = bdata->vbp;
	struct bu_list *vlfree = bdata->vlfree;
	unsigned char rgb[3] = {255, 255, 0};
	if (color)
	    bu_color_to_rgb_chars(color, rgb);
	struct bu_list *vhead = bv_vlblock_find(vbp, (int)rgb[0], (int)rgb[1], (int)rgb[2]);
	// Triangle plotting order doesn't matter particularly, just
	// iterate over all the workers
	for (size_t i = 0; i < wdata.size(); i++)
	    wdata[i]->plot_bad_tris(vbp, vhead, vlfree);
    }

    rt_free_rti(rtip);
    bu_free(resp, "resp");
}



// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
