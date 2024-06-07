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

#if 0
class lint_tri {
    public:
	point_t v[3];
	vect_t n;

	void plot(struct bview *);

	point_t center = VINIT_ZERO;
	struct bu_color c;
};

void
lint_tri::plot(struct bview *v, struct bv_vlblock *vbp, struct bu_list *vlfree)
{
    if (!v || !vbp || !vlfree)
	return;
    BV_ADD_VLIST(vlfree, vhead, v[0], BV_VLIST_LINE_MOVE);
    BV_ADD_VLIST(vlfree, vhead, v[1], BV_VLIST_LINE_DRAW);
    BV_ADD_VLIST(vlfree, vhead, v[2], BV_VLIST_LINE_DRAW);
    BV_ADD_VLIST(vlfree, vhead, v[0], BV_VLIST_LINE_DRAW);
}

static void
parse_pt(point_t *p, const nlohmann::json &sdata)
{
    if (sdata.contains("X")) {
	std::string s(sdata["X"]);
	(*p)[X] = s2d(s);
    }
    if (sdata.contains("Y")) {
	std::string s(sdata["Y"]);
	(*p)[Y] = s2d(s);
    }
    if (sdata.contains("Z")) {
	std::string s(sdata["Z"]);
	(*p)[Z] = s2d(s);
    }
}

void
json_to_tri(class lint_tri &tri, nlohmann::json &jtri)
{
    if (jtri.contains("V0")) {
	const nlohmann::json &jv = jtri["V0"];
	parse_pt(&tri.v[0], jv);
    }
    if (jtri.contains("V1")) {
	const nlohmann::json &jv = jtri["V1"];
	parse_pt(&tri.v[1], jv);
    }
    if (jtri.contains("V2")) {
	const nlohmann::json &jv = jtri["V2"];
	parse_pt(&tri.v[2], jv);
    }
    VADD3(tri.center, tri.v[0], tri.v[1], tri.v[2]);
    VSCALE(tri.center, tri.center, 1.0/3.0);

    if (jtri.contains("N")) {
	const nlohmann::json &jv = jtri["N"];
	parse_pt(&tri.n, jv);
    }
}
#endif

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

double
s2d(std::string s)
{
    double d;
    std::stringstream ss(s);
    size_t prec = std::numeric_limits<double>::max_digits10;
    ss >> std::setprecision(prec) >> std::fixed >> d;
    return d;
}

std::string
d2s(double d)
{
    size_t prec = std::numeric_limits<double>::max_digits10;
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(prec) << d;
    std::string sd = ss.str();
    return sd;
}

void
pt_to_json(nlohmann::json *pc, const char *key, point_t pt)
{
   (*pc)[key]["X"] = d2s(pt[X]);
   (*pc)[key]["Y"] = d2s(pt[Y]);
   (*pc)[key]["Z"] = d2s(pt[Z]);
}

void
ray_to_json(nlohmann::json *pc, struct xray *r)
{
    nlohmann::json ray;
    pt_to_json(&ray, "P", r->r_pt);
    pt_to_json(&ray, "N", r->r_dir);
    (*pc)["ray"].push_back(ray);
}

void
tri_to_json(nlohmann::json *pc, const char *oname, struct rt_bot_internal *bot, int ind)
{
    nlohmann::json tri;
    point_t v[3];
    for (int i = 0; i < 3; i++)
	VMOVE(v[i], &bot->vertices[bot->faces[ind*3+i]*3]);

    tri["bot_name"] = std::string(oname);
    tri["face_index"] = ind;
    pt_to_json(&tri, "V0", v[0]);
    pt_to_json(&tri, "V1", v[1]);
    pt_to_json(&tri, "V2", v[2]);

    vect_t n = VINIT_ZERO;
    bot_face_normal(&n, bot, ind);
    pt_to_json(&tri, "N", n);

    (*pc)["tris"].push_back(tri);
}




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

	const lint_data *ldata = NULL;
	struct application ap;
	struct rt_bot_internal *bot = NULL;

	std::string pname;
	std::string problem_type;

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

#if 0
static int
_tc_hit(struct application *ap, struct partition *PartHeadp, struct seg *segs)
{
    if (PartHeadp->pt_forw == PartHeadp)
	return 1;

    lint_work_data *tinfo = (lint_work_data *)ap->a_uptr;
    struct rt_bot_internal *bot = tinfo->bot;

    // If we're dealing with a triangle that is too small per the
    // user specified filter, skip it
    if (tinfo->min_tri_area > 0) {
	point_t v[3];
	for (int i = 0; i < 3; i++)
	    VMOVE(v[i], &bot->vertices[bot->faces[tinfo->curr_tri*3+i]*3]);
	double tri_area = bg_area_of_triangle(v[0], v[1], v[2]);
	if (tri_area < tinfo->min_tri_area)
	    return 0;
    }

    struct seg *s = (struct seg *)segs->l.forw;
    if (s->seg_in.hit_dist > 2*SQRT_SMALL_FASTF) {
	// This is a problem but it's not the thin volume problem - no point in
	// continuing
	return 0;
    }

    for (BU_LIST_FOR(s, seg, &(segs->l))) {
	// We're only interested in thin interactions with the triangle
	// in question - other triangles along the shotline will be checked in
	if (s->seg_in.hit_dist > tinfo->ttol)
	    break;

	nlohmann::json terr;
	terr["problem_type"] = -"unexpected_miss";
	terr["object_type"] = "bot";
	terr["object_name"] = pname;


	ray_to_json(&terr, &ap->a_ray);
	terr["indices"].push_back(s->seg_in.hit_surfno);
	terr["indices"].push_back(s->seg_out.hit_surfno);
	double dist = s->seg_out.hit_dist - s->seg_in.hit_dist;
	terr["dist"] = d2s(dist);
	tinfo->tresults["errors"].push_back(terr);
	tinfo->condition_flag = true;

	// Plot both the triangles involved with this interaction
	flagged_tris.insert(s->seg_in.hit_surfno);
	flagged_tris.insert(s->seg_out.hit_surfno);
    }

    return 0;
}

static int
_tc_miss(struct application *ap)
{
    // A straight-up miss is one of the possible reporting scenarios
    // for thin triangle pairs - if it happens, we need to flag what
    // we can find (unfortunately we only know which triangle prompted
    // the report in this case - hopefully the other triangle will also
    // trigger a thin report and get itself queued for removal.
    struct tc_info *tinfo = (struct tc_info *)ap->a_uptr;
    struct rt_bot_internal *bot = tinfo->bot;

    if (tinfo->min_tri_area > 0) {
	point_t v[3];
	for (int i = 0; i < 3; i++)
	    VMOVE(v[i], &bot->vertices[bot->faces[tinfo->curr_tri*3+i]*3]);
	double tri_area = bg_area_of_triangle(v[0], v[1], v[2]);
	if (tri_area < tinfo->min_tri_area)
	    return 0;
    }

    tinfo->condition_flag = true;
    nlohmann::json terr;
    ray_to_json(&terr, &ap->a_ray);
    terr["indices"].push_back(tinfo->curr_tri);
    tinfo->tresults["errors"].push_back(terr);

    // Plot the missed triangle
    flagged_tris.insert(tinfo->curr_tri);

    return 0;
}

static int
bot_thin_check(std::vector<lint_worker_data *> worker_data, const char *pname, struct rt_bot_internal *bot, struct rt_i *rtip, double ttol)
{
    if (!bot || bot->mode != RT_BOT_SOLID || !rtip || !bot->num_faces)
	return 0;

    std::map<std::string, std::set<std::string>> &imt = cdata->im_techniques;
    if (imt.size()) {
	std::set<std::string> &bt = imt[std::string("bot")];
	if (bt.find(std::string("thin_volume")) == bt.end())
	    return 0;
    }

    nlohmann::json terr;

    terr["problem_type"] = "thin_volume";
    terr["object_type"] = "bot";
    terr["object_name"] = pname;

    int found_thin = 0;
    struct tc_info tinfo;
    tinfo.ttol = ttol;
    tinfo.verbose = cdata->verbosity;
    tinfo.bot = bot;
    tinfo.data = &terr;
    tinfo.color = cdata->color;
    tinfo.vbp = cdata->vbp;
    tinfo.vlfree = cdata->vlfree;
    tinfo.do_plot = cdata->do_plot;
    tinfo.min_tri_area = cdata->min_tri_area;

    // Set up the raytrace
    if (!BU_LIST_IS_INITIALIZED(&rt_uniresource.re_parthead))
	rt_init_resource(&rt_uniresource, 0, rtip);
    struct application ap;
    RT_APPLICATION_INIT(&ap);
    ap.a_rt_i = rtip;     /* application uses this instance */
    ap.a_hit = _tc_hit;    /* where to go on a hit */
    ap.a_miss = _tc_miss;  /* where to go on a miss */
    ap.a_overlap = _tc_overlap;  /* where to go if an overlap is found */
    ap.a_onehit = 0;
    ap.a_resource = &rt_uniresource;
    ap.a_uptr = (void *)&tinfo;

    for (size_t i = 0; i < bot->num_faces; i++) {
	tinfo.curr_tri = (int)i;
	vect_t rnorm, n, backout;
	if (!bot_face_normal(&n, bot, i))
	    continue;

	// We want backout to get the ray origin off the triangle surface
	VMOVE(backout, n);
	VSCALE(backout, backout, SQRT_SMALL_FASTF);
	// Reverse the triangle normal for a ray direction
	VREVERSE(rnorm, n);

	point_t rpnts[3];
	point_t tcenter;
	VMOVE(rpnts[0], &bot->vertices[bot->faces[i*3+0]*3]);
	VMOVE(rpnts[1], &bot->vertices[bot->faces[i*3+1]*3]);
	VMOVE(rpnts[2], &bot->vertices[bot->faces[i*3+2]*3]);
	VADD3(tcenter, rpnts[0], rpnts[1], rpnts[2]);
	VSCALE(tcenter, tcenter, 1.0/3.0);

	// Take the shot
	tinfo.is_thin = 0;
	VMOVE(ap.a_ray.r_dir, rnorm);
	VADD2(ap.a_ray.r_pt, tcenter, backout);
	(void)rt_shootray(&ap);

	if (tinfo.is_thin) {
	    found_thin = 1;
	}
    }

    if (found_thin)
	cdata->j.push_back(terr);

    return found_thin;
}
#endif

#if 0

// TODO - A useful correctness audit for a BoT might be to shotline both the
// CSG and the BoT using the same rays constructed from the triangle centers -
// if the CSG reports a non-zero LOS but the BoT reports zero, we have a
// problem.
static int
_ck_up_hit(struct application *ap, struct partition *PartHeadp, struct seg *UNUSED(segs))
{
    if (PartHeadp->pt_forw == PartHeadp)
	return 1;

    struct ab_info *pinfo = (struct ab_info *)ap->a_uptr;
    struct rt_bot_internal *bot = pinfo->bot;

    struct partition *pp = PartHeadp->pt_forw;

    // If we've got something too close above our triangle, it's trouble
    //
    // TODO - validate whether the vector between the two hit points is
    // parallel to the ray.  Saw one case where it seemed as if we were getting
    // an offset that resulted in a higher distance, but only because there was
    // a shift of one of the hit points off the ray by more than ttol
    if (pp->pt_inhit->hit_dist < pinfo->ttol) {
	nlohmann::json terr;
	ray_to_json(&terr, &ap->a_ray);
	terr["indices"].push_back(pp->pt_inhit->hit_surfno);
	terr["indices"].push_back(pp->pt_outhit->hit_surfno);
	(*pinfo->data)["errors"].push_back(terr);
	pinfo->have_above = 1;

	if (pinfo->do_plot) {
	    struct bu_color *color = pinfo->color;
	    struct bv_vlblock *vbp = pinfo->vbp;
	    struct bu_list *vlfree = pinfo->vlfree;
	    unsigned char rgb[3] = {255, 255, 0};
	    if (color)
		bu_color_to_rgb_chars(color, rgb);
	    struct bu_list *vhead = bv_vlblock_find(vbp, (int)rgb[0], (int)rgb[1], (int)rgb[2]);

	    point_t v[3];
	    for (int i = 0; i < 3; i++)
		VMOVE(v[i], &bot->vertices[bot->faces[pp->pt_inhit->hit_surfno*3+i]*3]);
	    BV_ADD_VLIST(vlfree, vhead, v[0], BV_VLIST_LINE_MOVE);
	    BV_ADD_VLIST(vlfree, vhead, v[1], BV_VLIST_LINE_DRAW);
	    BV_ADD_VLIST(vlfree, vhead, v[2], BV_VLIST_LINE_DRAW);
	    BV_ADD_VLIST(vlfree, vhead, v[0], BV_VLIST_LINE_DRAW);
	    for (int i = 0; i < 3; i++)
		VMOVE(v[i], &bot->vertices[bot->faces[pp->pt_outhit->hit_surfno*3+i]*3]);
	    BV_ADD_VLIST(vlfree, vhead, v[0], BV_VLIST_LINE_MOVE);
	    BV_ADD_VLIST(vlfree, vhead, v[1], BV_VLIST_LINE_DRAW);
	    BV_ADD_VLIST(vlfree, vhead, v[2], BV_VLIST_LINE_DRAW);
	    BV_ADD_VLIST(vlfree, vhead, v[0], BV_VLIST_LINE_DRAW);
	}

	return 0;
    }

    return 0;
}

static int
bot_close_check(lint_data *cdata, const char *pname, struct rt_bot_internal *bot, struct rt_i *rtip, double ttol)
{
    if (!bot || bot->mode != RT_BOT_SOLID || !rtip || !bot->num_faces)
	return 0;

    std::map<std::string, std::set<std::string>> &imt = cdata->im_techniques;
    if (imt.size()) {
	std::set<std::string> &bt = imt[std::string("bot")];
	if (bt.find(std::string("close_face")) == bt.end())
	    return 0;
    }

    nlohmann::json cerr;

    cerr["problem_type"] = "close_face";
    cerr["object_type"] = "bot";
    cerr["object_name"] = pname;

    int have_above = 0;
    struct ab_info cpinfo;
    cpinfo.ttol = ttol;
    cpinfo.verbose = cdata->verbosity;
    cpinfo.bot = bot;
    cpinfo.have_above = 0;
    cpinfo.data = &cerr;
    cpinfo.color = cdata->color;
    cpinfo.vbp = cdata->vbp;
    cpinfo.vlfree = cdata->vlfree;
    cpinfo.do_plot = cdata->do_plot;


    // Set up the raytrace
    if (!BU_LIST_IS_INITIALIZED(&rt_uniresource.re_parthead))
	rt_init_resource(&rt_uniresource, 0, rtip);
    struct application ap;
    RT_APPLICATION_INIT(&ap);
    ap.a_rt_i = rtip;     /* application uses this instance */
    ap.a_hit = _ck_up_hit;    /* where to go on a hit */
    ap.a_miss = _miss_noop;   /* where to go on a miss */
    ap.a_onehit = 1;
    ap.a_resource = &rt_uniresource;
    ap.a_uptr = (void *)&cpinfo;

    for (size_t i = 0; i < bot->num_faces; i++) {
	cpinfo.curr_tri = (int)i;
	vect_t rnorm, n, backout;
	if (!bot_face_normal(&n, bot, i))
	    continue;

	point_t rpnts[3];
	point_t tcenter;
	VMOVE(rpnts[0], &bot->vertices[bot->faces[i*3+0]*3]);
	VMOVE(rpnts[1], &bot->vertices[bot->faces[i*3+1]*3]);
	VMOVE(rpnts[2], &bot->vertices[bot->faces[i*3+2]*3]);
	VADD3(tcenter, rpnts[0], rpnts[1], rpnts[2]);
	VSCALE(tcenter, tcenter, 1.0/3.0);

	VMOVE(ap.a_ray.r_dir, n);

	VREVERSE(rnorm, n);
	VMOVE(backout, rnorm);
	VSCALE(backout, backout, SMALL_FASTF);
	VADD2(ap.a_ray.r_pt, tcenter, backout);
	(void)rt_shootray(&ap);

	if (cpinfo.have_above)
	    have_above = 1;
    }

    if (have_above)
	cdata->j.push_back(cerr);

    return have_above;
}
#endif

static int
_mc_miss(struct application *ap)
{
    // We are shooting directly into the center of a triangle from right above
    // it.  If we miss under those conditions, it can only happen because
    // something is wrong.
    lint_worker_data *tinfo = (lint_worker_data *)ap->a_uptr;
    struct rt_bot_internal *bot = tinfo->bot;

    bu_log("%s tri_ind %d missed\n", tinfo->pname.c_str(), tinfo->curr_tri);

    if (tinfo->min_tri_area > 0) {
	point_t v[3];
	for (int i = 0; i < 3; i++)
	    VMOVE(v[i], &bot->vertices[bot->faces[tinfo->curr_tri*3+i]*3]);
	double tri_area = bg_area_of_triangle(v[0], v[1], v[2]);
	if (tri_area < tinfo->min_tri_area)
	    return 0;
    }

    tinfo->condition_flag = true;
    nlohmann::json terr;
    ray_to_json(&terr, &ap->a_ray);
    terr["indices"].push_back(tinfo->curr_tri);
    tinfo->tresults["errors"].push_back(terr);

    return 0;
}

static int
bot_miss_check(std::vector<lint_worker_data *> &wdata)
{
    lint_worker_data *w0 = wdata[0];
    if (!w0->bot || !w0->bot->num_faces)
	return 0;

    const std::map<std::string, std::set<std::string>> &imt = w0->ldata->im_techniques;
    if (imt.size()) {
	std::map<std::string, std::set<std::string>>::const_iterator i_it;
	i_it = imt.find(std::string("bot"));
	if (i_it->second.find(std::string("unexpected_miss")) == i_it->second.end())
	    return 0;
    }

    nlohmann::json merr;

    for (size_t i = 0; i < wdata.size(); i++) {
	wdata[i]->problem_type = std::string("unexpected_miss");
	wdata[i]->ap.a_hit = _hit_noop;
	wdata[i]->ap.a_miss = _mc_miss;
	wdata[i]->ap.a_onehit = 0; /* One hit isn't enough - looking for something solid */
    }

    moodycamel::ConcurrentQueue<size_t> q;
    moodycamel::ProducerToken ptok(q);
    std::vector<std::thread> threads;
    for (size_t i = 0; i < w0->bot->num_faces; i++)
	q.enqueue(ptok, i);

    // Do the work
    for (size_t i = 0; i < wdata.size(); i++) {
	threads.emplace_back(std::thread([&](int ind) {
		size_t tri_ind;
		while (q.try_dequeue_from_producer(ptok, tri_ind)) {
		       wdata[ind]->shoot(tri_ind, false);
		}
		}, i));
    }

    // Wait for all threads
    for (size_t i = 0; i < wdata.size(); i++) {
	threads[i].join();
    }

    // Clean up anything left
    size_t tri_ind;
    while (q.try_dequeue_from_producer(ptok, tri_ind)) {
	wdata[0]->shoot(tri_ind, false);
    }

    // TODO - to make this properly multithreaded and still report an overall
    // summary as a single json file, we'll need to  merge the results at the
    // end.  Probably will use either obj1.update(obj2), or
    // obj1.merge_patch(obj2) per https://github.com/nlohmann/json/issues/1807

    return 0;
}

#if 0
struct uh_info {
    double ttol;
    int unexpected_hit;
    struct rt_bot_internal *bot;
    const char *pname;
    nlohmann::json *data = NULL;

    struct bu_color *color = NULL;
    struct bv_vlblock *vbp = NULL;
    struct bu_list *vlfree = NULL;
    bool do_plot = false;
    fastf_t min_tri_area = 0.0;

    int verbose;
    int curr_tri;
};

static int
_uh_hit(struct application *ap, struct partition *PartHeadp, struct seg *segs)
{
    if (PartHeadp->pt_forw == PartHeadp)
	return 1;

    struct uh_info *tinfo = (struct uh_info *)ap->a_uptr;
    struct rt_bot_internal *bot = tinfo->bot;

    struct seg *s = (struct seg *)segs->l.forw;
    if (s->seg_in.hit_dist > 2*SQRT_SMALL_FASTF) {

	if (tinfo->min_tri_area > 0) {
	    point_t v[3];
	    for (int i = 0; i < 3; i++)
		VMOVE(v[i], &bot->vertices[bot->faces[tinfo->curr_tri*3+i]*3]);
	    double tri_area = bg_area_of_triangle(v[0], v[1], v[2]);
	    if (tri_area < tinfo->min_tri_area)
		return 0;
	}

	// Segment's first hit didn't come from the expected triangle.
	nlohmann::json terr;
	ray_to_json(&terr, &ap->a_ray);
	terr["indices"].push_back(tinfo->curr_tri);
	(*tinfo->data)["errors"].push_back(terr);
	tinfo->unexpected_hit = 1;
	if (tinfo->do_plot) {
	    struct bu_color *color = tinfo->color;
	    struct bv_vlblock *vbp = tinfo->vbp;
	    struct bu_list *vlfree = tinfo->vlfree;
	    unsigned char rgb[3] = {255, 255, 0};
	    if (color)
		bu_color_to_rgb_chars(color, rgb);
	    struct bu_list *vhead = bv_vlblock_find(vbp, (int)rgb[0], (int)rgb[1], (int)rgb[2]);

	    point_t v[3];
	    for (int i = 0; i < 3; i++)
		VMOVE(v[i], &bot->vertices[bot->faces[tinfo->curr_tri*3+i]*3]);
	    BV_ADD_VLIST(vlfree, vhead, v[0], BV_VLIST_LINE_MOVE);
	    BV_ADD_VLIST(vlfree, vhead, v[1], BV_VLIST_LINE_DRAW);
	    BV_ADD_VLIST(vlfree, vhead, v[2], BV_VLIST_LINE_DRAW);
	    BV_ADD_VLIST(vlfree, vhead, v[0], BV_VLIST_LINE_DRAW);
	}
	return 0;
    }

    return 0;
}

/* I don't think this is supposed to happen with a single primitive, but just
 * in case we get an overlap report somehow flag it as trouble */
static int
_uh_overlap(struct application *ap,
	struct partition *UNUSED(pp),
	struct region *UNUSED(reg1),
	struct region *UNUSED(reg2),
	struct partition *UNUSED(hp))
{
    struct uh_info *tinfo = (struct uh_info *)ap->a_uptr;
    struct rt_bot_internal *bot = tinfo->bot;

    tinfo->unexpected_hit = 1;
    nlohmann::json terr;
    ray_to_json(&terr, &ap->a_ray);
    terr["indices"].push_back(tinfo->curr_tri);
    (*tinfo->data)["errors"].push_back(terr);

    if (tinfo->do_plot) {
	struct bu_color *color = tinfo->color;
	struct bv_vlblock *vbp = tinfo->vbp;
	struct bu_list *vlfree = tinfo->vlfree;
	unsigned char rgb[3] = {255, 255, 0};
	if (color)
	    bu_color_to_rgb_chars(color, rgb);
	struct bu_list *vhead = bv_vlblock_find(vbp, (int)rgb[0], (int)rgb[1], (int)rgb[2]);

	point_t v[3];
	for (int i = 0; i < 3; i++)
	    VMOVE(v[i], &bot->vertices[bot->faces[tinfo->curr_tri*3+i]*3]);
	BV_ADD_VLIST(vlfree, vhead, v[0], BV_VLIST_LINE_MOVE);
	BV_ADD_VLIST(vlfree, vhead, v[1], BV_VLIST_LINE_DRAW);
	BV_ADD_VLIST(vlfree, vhead, v[2], BV_VLIST_LINE_DRAW);
	BV_ADD_VLIST(vlfree, vhead, v[0], BV_VLIST_LINE_DRAW);
    }

    return 0;
}

static int
bot_hit_check(lint_data *cdata, const char *pname, struct rt_bot_internal *bot, struct rt_i *rtip, double ttol)
{
    if (!bot || bot->mode != RT_BOT_SOLID || !rtip || !bot->num_faces)
	return 0;

    std::map<std::string, std::set<std::string>> &imt = cdata->im_techniques;
    if (imt.size()) {
	std::set<std::string> &bt = imt[std::string("bot")];
	if (bt.find(std::string("unexpected_hit")) == bt.end())
	    return 0;
    }

    nlohmann::json terr;

    terr["problem_type"] = "unexpected_hit";
    terr["object_type"] = "bot";
    terr["object_name"] = pname;

    int unexpected_hit = 0;
    struct uh_info tinfo;
    tinfo.ttol = ttol;
    tinfo.verbose = cdata->verbosity;
    tinfo.bot = bot;
    tinfo.data = &terr;
    tinfo.color = cdata->color;
    tinfo.vbp = cdata->vbp;
    tinfo.vlfree = cdata->vlfree;
    tinfo.do_plot = cdata->do_plot;
    tinfo.min_tri_area = cdata->min_tri_area;

    // Set up the raytrace
    if (!BU_LIST_IS_INITIALIZED(&rt_uniresource.re_parthead))
	rt_init_resource(&rt_uniresource, 0, rtip);
    struct application ap;
    RT_APPLICATION_INIT(&ap);
    ap.a_rt_i = rtip;     /* application uses this instance */
    ap.a_hit = _uh_hit;    /* where to go on a hit */
    ap.a_miss = _miss_noop;  /* where to go on a miss */
    ap.a_overlap = _uh_overlap;  /* where to go if an overlap is found */
    ap.a_onehit = 0;
    ap.a_resource = &rt_uniresource;
    ap.a_uptr = (void *)&tinfo;

    for (size_t i = 0; i < bot->num_faces; i++) {
	tinfo.curr_tri = (int)i;
	vect_t rnorm, n, backout;
	if (!bot_face_normal(&n, bot, i))
	    continue;

	// We want backout to get the ray origin off the triangle surface
	VMOVE(backout, n);
	VSCALE(backout, backout, SQRT_SMALL_FASTF);
	// Reverse the triangle normal for a ray direction
	VREVERSE(rnorm, n);

	point_t rpnts[3];
	point_t tcenter;
	VMOVE(rpnts[0], &bot->vertices[bot->faces[i*3+0]*3]);
	VMOVE(rpnts[1], &bot->vertices[bot->faces[i*3+1]*3]);
	VMOVE(rpnts[2], &bot->vertices[bot->faces[i*3+2]*3]);
	VADD3(tcenter, rpnts[0], rpnts[1], rpnts[2]);
	VSCALE(tcenter, tcenter, 1.0/3.0);

	// Take the shot
	tinfo.unexpected_hit = 0;
	VMOVE(ap.a_ray.r_dir, rnorm);
	VADD2(ap.a_ray.r_pt, tcenter, backout);
	(void)rt_shootray(&ap);

	if (tinfo.unexpected_hit) {
	    unexpected_hit = 1;
	}
    }

    if (unexpected_hit)
	cdata->j.push_back(terr);

    return unexpected_hit;
}

#endif

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
    if (bot->mode != RT_BOT_SOLID) {
	return;
    }

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
	d->ttol = bdata->ftol;
	d->min_tri_area = bdata->min_tri_area;
	wdata.push_back(d);
    }

    bot_miss_check(wdata);
    //bot_thin_check(bdata, dp->d_namep, bot, rtip, bdata->ftol);
    //bot_close_check(bdata, dp->d_namep, bot, rtip, bdata->ftol);
    //bot_hit_check(bdata, dp->d_namep, bot, rtip, bdata->ftol);

#if 0
    struct bu_color *color = bdata->color;
    struct bv_vlblock *vbp = bdata->vbp;
    struct bu_list *vlfree = bdata->vlfree;
    unsigned char rgb[3] = {255, 255, 0};
    if (color)
	bu_color_to_rgb_chars(color, rgb);
    struct bu_list *vhead = bv_vlblock_find(vbp, (int)rgb[0], (int)rgb[1], (int)rgb[2]);

#endif

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
