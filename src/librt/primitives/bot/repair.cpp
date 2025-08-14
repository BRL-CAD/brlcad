/*                       R E P A I R . C P P
 * BRL-CAD
 *
 * Copyright (c) 2020-2025 United States Government as represented by
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
/** @file repair.cpp
 *
 * Routines related to repairing BoTs
 */

#include "common.h"

#include <stdlib.h>
#include <string.h>

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <list>
#include <map>
#include <queue>
#include <set>
#include <sstream>
#include <string>
#include <unordered_set>
#include <vector>

#include "manifold/manifold.h"

#include "geogram/basic/process.h"
#include <geogram/basic/command_line.h>
#include <geogram/basic/command_line_args.h>
#include "geogram/mesh/mesh.h"
#include "geogram/mesh/mesh_geometry.h"
#include "geogram/mesh/mesh_preprocessing.h"
#include "geogram/mesh/mesh_repair.h"
#include "geogram/mesh/mesh_fill_holes.h"
#include "geogram/mesh/mesh_remesh.h"

#include "bu/parallel.h"
#include "bg/trimesh.h"
#include "rt/defines.h"
#include "rt/application.h"
#include "rt/db_io.h"
#include "rt/geom.h"
#include "rt/primitives/bot.h"
#include "rt/rt_instance.h"
#include "rt/shoot.h"
#include "rt/wdb.h"

// The checking done with raytracing here is basically the checking done by
// libged's lint command, without the output collection done there for
// reporting purposes.  This is a yes/no decision as to whether the "repaired"
// BoT is suitable to be returned.
//
// No such automated checks can catch all cases where the result isn't what a
// user originally expected for all inputs (in the most general cases that
// question is actually not well defined) but we CAN catch a few situations
// where the result is technically manifold but the mesh still does something
// unexpected during solid raytracing.

struct lint_worker_vars {
    struct rt_i *rtip;
    struct resource *resp;
    int tri_start;
    int tri_end;
    bool reverse;
    void *ptr;
};

class lint_worker_data {
    public:
	lint_worker_data(struct rt_i *rtip, struct resource *res);
	~lint_worker_data();
	void shoot(int ind, bool reverse);

	int curr_tri = -1;
	double ttol = 0.0;

	bool error_found = false;

	struct application ap;
	struct rt_bot_internal *bot = NULL;
	const std::unordered_set<int> *bad_faces = NULL;
};

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

static int
_hit_noop(struct application *UNUSED(ap), struct partition *PartHeadp, struct seg *UNUSED(segs))
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
_overlap_noop(struct application *UNUSED(ap), struct partition *UNUSED(pp),
	struct region *UNUSED(reg1), struct region *UNUSED(reg2), struct partition *UNUSED(hp))
{
    // I don't think this is supposed to happen with a single primitive?
    return 0;
}

static int
_miss_err(struct application *ap)
{
    lint_worker_data *tinfo = (lint_worker_data *)ap->a_uptr;
    tinfo->error_found = true;
    return 0;
}

static int
_tc_hit(struct application *ap, struct partition *PartHeadp, struct seg *segs)
{
    if (PartHeadp->pt_forw == PartHeadp)
	return 1;

    lint_worker_data *tinfo = (lint_worker_data *)ap->a_uptr;

    struct seg *s = (struct seg *)segs->l.forw;
    if (s->seg_in.hit_dist > 2*SQRT_SMALL_FASTF) {
	// This is a problem (although it's not the thin volume problem.) No point in
	// continuing, flag and return.
	tinfo->error_found = true;
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

	// Error condition met - set flag
	tinfo->error_found = true;
	return 0;
    }

    return 0;
}

static int
_ck_up_hit(struct application *ap, struct partition *PartHeadp, struct seg *UNUSED(segs))
{
    if (PartHeadp->pt_forw == PartHeadp)
	return 1;

    lint_worker_data *tinfo = (lint_worker_data *)ap->a_uptr;

    // TODO - validate whether the vector between the two hit points is
    // parallel to the ray.  Saw one case where it seemed as if we were getting
    // an offset that resulted in a higher distance, but only because there was
    // a shift of one of the hit points off the ray by more than ttol
    struct partition *pp = PartHeadp->pt_forw;
    if (pp->pt_inhit->hit_dist > tinfo->ttol)
	return 0;

    // We've got something < tinfo->ttol above our triangle - too close, trouble
    tinfo->error_found = true;
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
    tinfo->error_found = true;
    return 0;
}


extern "C" void
bot_repair_lint_worker(int cpu, void *ptr)
{
    struct lint_worker_vars *state = &(((struct lint_worker_vars *)ptr)[cpu]);
    lint_worker_data *d = (lint_worker_data *)state->ptr;

    for (int i = state->tri_start; i < state->tri_end; i++) {
	d->shoot(i, state->reverse);
    }
}

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
}


typedef int (*fhit_t)(struct application *, struct partition *, struct seg *);
typedef int (*fmiss_t)(struct application *);

static bool
bot_check(struct lint_worker_vars *state, fhit_t hf, fmiss_t mf, int onehit, bool reverse, size_t ncpus)
{
    // We always need at least one worker data container to do any work at all.
    if (!ncpus)
	return false;

    // Much of the information needed for different tests is common and thus can be
    // reused, but some aspects are specific to each test - let all the worker data
    // containers know what the specifics are for this test.
    for (size_t i = 0; i < ncpus; i++) {
	lint_worker_data *d = (lint_worker_data *)state[i].ptr;
	d->ap.a_hit = hf;
	d->ap.a_miss = mf;
	d->ap.a_onehit = onehit;
	state[i].reverse = reverse;
    }

    bu_parallel(bot_repair_lint_worker, ncpus, (void *)state);

    // Check the thread results to see if any errors were reported
    for (size_t i = 0; i < ncpus; i++) {
	lint_worker_data *d = (lint_worker_data *)state[i].ptr;
	if (d->error_found)
	    return false;
    }

    return true;
}

static int
bot_repair_lint(struct rt_bot_internal *bot)
{
    // Empty BoTs are a problem
    if (!bot || !bot->num_faces)
	return -1;

    // Default to valid
    int ret = 0;

    // We need to use the raytracer to test this BoT, but it is not a database
    // entity yet.  Accordingly, we set up an in memory db_i and add this BoT
    // to it so we can raytrace it.  Any failure here means we weren't able to
    // do the test and (in the absence of confirmed testing success) we have no
    // choice but to report failure.
    struct db_i *dbip = db_open_inmem();
    if (dbip == DBI_NULL)
        return -1;
    struct rt_wdb *wdbp = wdb_dbopen(dbip, RT_WDB_TYPE_DB_INMEM);
    // wdb_export is destructive to the rt_bot_internal, so we need
    // to make a copy.
    struct rt_bot_internal *dbot = rt_bot_dup(bot);
    wdb_export(wdbp, "r.bot", (void *)dbot, ID_BOT, 1.0);
    struct directory *dp = db_lookup(wdbp->dbip, "r.bot", LOOKUP_QUIET);
    if (dp == RT_DIR_NULL) {
	db_close(dbip);
	return -1;
    }

    // Note that these tests won't work as expected if the BoT is
    // self-intersecting...
    struct rt_i *rtip = rt_new_rti(dbip);
    rt_gettree(rtip, dp->d_namep);
    rt_prep(rtip);

    // Set up memory
    //size_t ncpus = bu_avail_cpus();
    size_t ncpus = 1;
    struct lint_worker_vars *state = (struct lint_worker_vars *)bu_calloc(ncpus+1, sizeof(struct lint_worker_vars ), "state");
    struct resource *resp = (struct resource *)bu_calloc(ncpus+1, sizeof(struct resource), "resources");

    // We need to divy up the faces.  Since all triangle intersections will
    // (hopefully) take about the same length of time to run, we don't do anything
    // fancy about chunking up the work.
    int tri_step = bot->num_faces / ncpus;

    for (size_t i = 0; i < ncpus; i++) {
	state[i].rtip = rtip;
	state[i].resp = &resp[i];
	rt_init_resource(state[i].resp, (int)i, state[i].rtip);
	state[i].tri_start = i * tri_step;
	state[i].tri_end = state[i].tri_start + tri_step;
	//bu_log("%d: tri_state: %d, tri_end %d\n", (int)i, state[i].tri_start, state[i].tri_end);
	state[i].reverse = false;

	lint_worker_data *d = new lint_worker_data(rtip, state[i].resp);
	d->bot = bot;
	d->ttol = VUNITIZE_TOL;
	state[i].ptr = (void *)d;
    }

    // Make sure the last thread ends on the last face
    state[ncpus-1].tri_end = bot->num_faces - 1;
    //bu_log("%d: tri_end %d\n", (int)ncpus-1, state[ncpus-1].tri_end);

    /* Unexpected miss test.
     * Note that we are deliberately using onehit=1 for the miss test to check
     * the intersection behavior of the individual triangles */
    if (!bot_check(state, _hit_noop, _miss_err, 1, false, ncpus)) {
	//bu_log("unexpected_miss\n");
	ret = 1;
	goto bot_lint_cleanup;
    }

    /* Thin volume test.
     * Thin face pairings are a common artifact of coplanar faces in boolean
     * evaluations */
    if (!bot_check(state, _tc_hit, _miss_err, 0, false, ncpus)){
	//bu_log("thin_volume\n");
	ret = 2;
	goto bot_lint_cleanup;
    }

    /* Close face test.
     * When testing for faces that are too close to a given face, we need to
     * reverse the ray direction */
    if (!bot_check(state, _ck_up_hit, _miss_noop, 0, true, ncpus)){
	//bu_log("close_face\n");
	ret = 2;
	goto bot_lint_cleanup;
    }

    /* Unexpected hit test.
     * Checking for the case where we end up with a hit from a triangle other
     * than the one we derive the ray from. */
    if (!bot_check(state, _uh_hit, _miss_noop, 0, false, ncpus)){
	//bu_log("unexpected_hit\n");
	ret = 2;
	goto bot_lint_cleanup;
    }

bot_lint_cleanup:
    for (size_t i = 0; i < ncpus; i++) {
	lint_worker_data *d = (lint_worker_data *)state[i].ptr;
	delete d;
    }

    rt_free_rti(rtip);
    bu_free(state, "state");
    bu_free(resp, "resp");
    db_close(dbip);

    return ret;
}

static void
bot_to_geogram(GEO::Mesh *gm, struct rt_bot_internal *bot)
{
    gm->vertices.assign_points((double *)bot->vertices, 3, bot->num_vertices);
    for (size_t i = 0; i < bot->num_faces; i++) {
	GEO::index_t f = gm->facets.create_polygon(3);
	gm->facets.set_vertex(f, 0, bot->faces[3*i+0]);
	gm->facets.set_vertex(f, 1, bot->faces[3*i+1]);
	gm->facets.set_vertex(f, 2, bot->faces[3*i+2]);
    }

    // After the initial raw load, do a repair pass to set up
    // Geogram's internal mesh data
    double epsilon = 1e-6 * (0.01 * GEO::bbox_diagonal(*gm));
    GEO::mesh_repair(*gm, GEO::MeshRepairMode(GEO::MESH_REPAIR_DEFAULT), epsilon);

    // Per the geobox "mesh repair" function, we need to do some
    // small connected component removal ahead of the fill_holes
    // call  - that was the behavior difference observed between
    // the raw bot manifold run and exporting the mesh into geobox
    // for processing
    double area = GEO::Geom::mesh_area(*gm,3);
    double min_comp_area = 0.03 * area;
    if (min_comp_area > 0.0) {
	double nb_f_removed = gm->facets.nb();
	GEO::remove_small_connected_components(*gm, min_comp_area);
	nb_f_removed -= gm->facets.nb();
	if(nb_f_removed > 0 || nb_f_removed < 0) {
	    GEO::mesh_repair(*gm, GEO::MESH_REPAIR_DEFAULT, epsilon);
	}
    }
}

static void
geogram_to_manifold(manifold::MeshGL *gmm, GEO::Mesh &gm)
{
    for (GEO::index_t v = 0; v < gm.vertices.nb(); v++) {
	const double *p = gm.vertices.point_ptr(v);
	for (int i = 0; i < 3; i++)
	    gmm->vertProperties.insert(gmm->vertProperties.end(), p[i]);
    }
    for (GEO::index_t f = 0; f < gm.facets.nb(); f++) {
	for (int i = 0; i < 3; i++) {
	    // TODO - CW vs CCW orientation handling?
	    gmm->triVerts.insert(gmm->triVerts.end(), gm.facets.vertex(f, i));
	}
    }
}

static struct rt_bot_internal *
manifold_to_bot(manifold::MeshGL *omesh)
{
    struct rt_bot_internal *nbot;
    BU_GET(nbot, struct rt_bot_internal);
    nbot->magic = RT_BOT_INTERNAL_MAGIC;
    nbot->mode = RT_BOT_SOLID;
    nbot->orientation = RT_BOT_CCW;
    nbot->thickness = NULL;
    nbot->face_mode = (struct bu_bitv *)NULL;
    nbot->bot_flags = 0;
    nbot->num_vertices = (int)omesh->vertProperties.size()/3;
    nbot->num_faces = (int)omesh->triVerts.size()/3;
    nbot->vertices = (double *)calloc(omesh->vertProperties.size(), sizeof(double));;
    nbot->faces = (int *)calloc(omesh->triVerts.size(), sizeof(int));
    for (size_t j = 0; j < omesh->vertProperties.size(); j++)
	nbot->vertices[j] = omesh->vertProperties[j];
    for (size_t j = 0; j < omesh->triVerts.size(); j++)
	nbot->faces[j] = omesh->triVerts[j];

    return nbot;
}

struct rt_bot_internal *
geogram_to_bot(GEO::Mesh *gm)
{
    struct rt_bot_internal *nbot;
    BU_GET(nbot, struct rt_bot_internal);
    nbot->magic = RT_BOT_INTERNAL_MAGIC;
    nbot->mode = RT_BOT_SOLID;
    nbot->orientation = RT_BOT_CCW;
    nbot->thickness = NULL;
    nbot->face_mode = (struct bu_bitv *)NULL;
    nbot->bot_flags = 0;
    nbot->num_vertices = (int)gm->vertices.nb();
    nbot->num_faces = (int)gm->facets.nb();
    nbot->vertices = (double *)calloc(nbot->num_vertices*3, sizeof(double));
    nbot->faces = (int *)calloc(nbot->num_faces*3, sizeof(int));

    int j = 0;
    for(GEO::index_t v = 0; v < gm->vertices.nb(); v++) {
	double gm_v[3];
	const double *p = gm->vertices.point_ptr(v);
	for (int i = 0; i < 3; i++)
	    gm_v[i] = p[i];
	nbot->vertices[3*j] = gm_v[0];
	nbot->vertices[3*j+1] = gm_v[1];
	nbot->vertices[3*j+2] = gm_v[2];
	j++;
    }

    j = 0;
    for (GEO::index_t f = 0; f < gm->facets.nb(); f++) {
	double tri_verts[3];
	for (int i = 0; i < 3; i++)
	    tri_verts[i] = gm->facets.vertex(f, i);
	// TODO - CW vs CCW orientation handling?
	nbot->faces[3*j] = tri_verts[0];
	nbot->faces[3*j+1] = tri_verts[1];
	nbot->faces[3*j+2] = tri_verts[2];
	j++;
    }

    return nbot;
}

int
rt_bot_repair(struct rt_bot_internal **obot, struct rt_bot_internal *bot, struct rt_bot_repair_info *settings)
{
    if (!bot || !obot || !settings)
	return -1;

    // Unless we produce something, obot will be NULL
    *obot = NULL;

    manifold::MeshGL64 bot_mesh;
    for (size_t j = 0; j < bot->num_vertices ; j++) {
	bot_mesh.vertProperties.insert(bot_mesh.vertProperties.end(), bot->vertices[3*j+0]);
	bot_mesh.vertProperties.insert(bot_mesh.vertProperties.end(), bot->vertices[3*j+1]);
	bot_mesh.vertProperties.insert(bot_mesh.vertProperties.end(), bot->vertices[3*j+2]);
    }
    if (bot->orientation == RT_BOT_CW) {
	for (size_t j = 0; j < bot->num_faces; j++) {
	    bot_mesh.triVerts.insert(bot_mesh.triVerts.end(), bot->faces[3*j]);
	    bot_mesh.triVerts.insert(bot_mesh.triVerts.end(), bot->faces[3*j+2]);
	    bot_mesh.triVerts.insert(bot_mesh.triVerts.end(), bot->faces[3*j+1]);
	}
    } else {
	for (size_t j = 0; j < bot->num_faces; j++) {
	    bot_mesh.triVerts.insert(bot_mesh.triVerts.end(), bot->faces[3*j]);
	    bot_mesh.triVerts.insert(bot_mesh.triVerts.end(), bot->faces[3*j+1]);
	    bot_mesh.triVerts.insert(bot_mesh.triVerts.end(), bot->faces[3*j+2]);
	}
    }

    int num_vertices = (int)bot->num_vertices;
    int num_faces = (int)bot->num_faces;
    int bg_not_solid = bg_trimesh_solid2(num_vertices, num_faces, bot->vertices, bot->faces, NULL);

    if (!bot_mesh.Merge() && !bg_not_solid) {
	// BoT is already manifold
	return 1;
    }

    manifold::Manifold omanifold(bot_mesh);
    if (omanifold.Status() == manifold::Manifold::Error::NoError) {
	// MeshGL.Merge() produced a manifold mesh.  That this worked
	// essentially means the changes needed were EXTREMELY minimal, and we
	// don't bother with further processing before returning the result.
	manifold::MeshGL omesh = omanifold.GetMeshGL();
	struct rt_bot_internal *nbot = manifold_to_bot(&omesh);
	*obot = nbot;
	return 0;
    }

    // Geogram libraries like to print a lot - shut down
    // the I/O channels until we can clear the logger
    int serr = -1;
    int sout = -1;
    int stderr_stashed = -1;
    int stdout_stashed = -1;
    int fnull = open("/dev/null", O_WRONLY);
    if (fnull == -1) {
	/* https://gcc.gnu.org/ml/gcc-patches/2005-05/msg01793.html */
	fnull = open("nul", O_WRONLY);
    }
    if (fnull != -1) {
	serr = fileno(stderr);
	sout = fileno(stdout);
	stderr_stashed = dup(serr);
	stdout_stashed = dup(sout);
	dup2(fnull, serr);
	dup2(fnull, sout);
	close(fnull);
    }

    // Make sure geogram is initialized
    GEO::initialize();

     // Quell logging messages
    GEO::Logger::instance()->unregister_all_clients();

    // Put I/O channels back where they belong
    if (fnull != -1) {
	fflush(stderr);
	dup2(stderr_stashed, serr);
	close(stderr_stashed);
	fflush(stdout);
	dup2(stdout_stashed, sout);
	close(stdout_stashed);
    }

    // Use the default hole filling algorithm
    GEO::CmdLine::set_arg("algo:hole_filling", "loop_split");
    GEO::CmdLine::set_arg("algo:nn_search", "BNN");

    // Set up a Geogram mesh using the BoT data
    GEO::Mesh gm;
    bot_to_geogram(&gm, bot);

    // To try to to fill in ALL holes we default to 1e30, which is a
    // value used in the Geogram code for a large hole size.
    double hole_size = 1e30;

    // Stash the bounding box diagonal
    double bbox_diag = GEO::bbox_diagonal(gm);

    // See if the settings override the default
    double area = GEO::Geom::mesh_area(gm,3);
    if (!NEAR_ZERO(settings->max_hole_area, SMALL_FASTF)) {
	hole_size = settings->max_hole_area;
    } else if (!NEAR_ZERO(settings->max_hole_area_percent, SMALL_FASTF)) {
	hole_size = area * (settings->max_hole_area_percent/100.0);
    }

    // Do the hole filling.
    GEO::fill_holes(gm, hole_size);

    // Make sure we're still repaired post filling
    GEO::mesh_repair(gm, GEO::MeshRepairMode(GEO::MESH_REPAIR_DEFAULT));

    // Post repair, make sure mesh is still a triangle mesh
    gm.facets.triangulate();

    // Sanity check the area - shouldn't go down, and if it went up by more
    // than 3x it's concerning - that's a lot of new area even for a swiss
    // cheese mesh.  Can revisit reporting failure if we hit a legit case
    // like that, but we also want to know if something went badly wrong with
    // the hole filling itself and crazy new geometry was added...
    double new_area = GEO::Geom::mesh_area(gm,3);
    if (new_area < area) {
	bu_log("Mesh area decreased after hole filling - error\n");
	return -1;
    }
    if (new_area > 3*area) {
	bu_log("Mesh area more than tripled after hole filling.  At the moment this is considered an error - if a legitimate case exists where this is expected behavior, please report it upstream to the BRL-CAD developers.\n");
	return -1;
    }

    // Sanity check the bounding box diagonal - should be very close to the
    // original value
    double new_bbox_diag = GEO::bbox_diagonal(gm);
    if (!NEAR_EQUAL(bbox_diag, new_bbox_diag, BN_TOL_DIST)) {
	bu_log("Mesh bounding box is different after hole filling - error\n");
	return -1;
    }

    // Once Geogram is done with it, ask Manifold what it thinks
    // of the result - if Manifold doesn't think we're there, then
    // the results won't fly for boolean evaluation and we go ahead
    // and reject.
    manifold::MeshGL gmm;
    geogram_to_manifold(&gmm, gm);
    manifold::Manifold gmanifold(gmm);
    if (gmanifold.Status() != manifold::Manifold::Error::NoError) {
	// Repair failed
	return -1;
    }

    // Output is manifold, make a new bot
    manifold::MeshGL omesh = gmanifold.GetMeshGL();
    struct rt_bot_internal *nbot = manifold_to_bot(&omesh);

    // Once we have an rt_bot_internal, see what the solid raytracer's linting
    // thinks of this unless the user has explicitly told us not to do any
    // validation beyond the manifold check.  The above is enough for boolean
    // evaluation input, but won't necessarily clear all problem cases that
    // might arise in a solid raytrace.
    if (settings->strict) {
	int lint_ret = bot_repair_lint(nbot);
	if (lint_ret) {
	    bu_log("Error - new BoT does not pass lint test!\n");
	    rt_bot_internal_free(nbot);
	    BU_PUT(nbot, struct rt_bot_internal);
	    return -1;
	}

	// Note - the below attempt doesn't seem to be able to successfully
	// repair the Generic Twin failing inputs.  Not clear if it's worth
	// trying this or not...
#if 0
	// If we got an unexpected miss, try a remesh to see if it can produce more
	// acceptable triangles (at the expense of increasing mesh size.).
	if (lint_ret == 1) {

	    // Target 10 times the vertices of the original mesh to allow for
	    // flexibility introducing new triangles
	    fastf_t nb_pts = nbot->num_vertices * 10;

	    // Original nbot is the data for new Geogram input
	    GEO::Mesh remesh_src;
	    bot_to_geogram(&remesh_src, nbot);

	    // Done with original nbot
	    rt_bot_internal_free(nbot);
	    BU_PUT(nbot, struct rt_bot_internal);

	    // Set up for remeshing
	    GEO::CmdLine::import_arg_group("standard");
	    GEO::CmdLine::import_arg_group("algo");
	    GEO::CmdLine::import_arg_group("remesh");
	    std::string nbpts = std::to_string(nb_pts);
	    GEO::CmdLine::set_arg("remesh:nb_pts", nbpts.c_str());

	    // Execute remesh
	    // https://github.com/BrunoLevy/geogram/wiki/Remeshing
	    GEO::compute_normals(remesh_src);
	    set_anisotropy(remesh_src, 2*0.02);
	    GEO::Mesh remesh;
	    GEO::remesh_smooth(remesh_src, remesh, nb_pts);

	    // Make sure Manifold likes the remeshed result
	    manifold::Mesh grmm;
	    geogram_to_manifold(&grmm, remesh);
	    manifold::Manifold grmanifold(grmm);
	    if (grmanifold.Status() != manifold::Manifold::Error::NoError) {
		// Repair failed
		bu_log("Error - remeshed repair output is not Manifold!\n");
		rt_bot_internal_free(nbot);
		BU_PUT(nbot, struct rt_bot_internal);
		return -1;
	    }

	    // Output is manifold, make a new bot
	    nbot = geogram_to_bot(&remesh);

	    // Remeshing is probably denser than we want, do a decimation
	    rt_bot_decimate_gct(nbot, 0.01*bbox_diag);

	    // Try lint one more time - if that didn't do it, we're done.
	    lint_ret = bot_repair_lint(nbot);
	    if (lint_ret) {
		bu_log("Error - new BoT does not pass lint test! (remeshing attempted)\n");
		rt_bot_internal_free(nbot);
		BU_PUT(nbot, struct rt_bot_internal);
		return -1;
	    }
	}
#endif
    }

    *obot = nbot;
    return 0;
}


struct rt_bot_internal *
rt_bot_remove_faces(struct bu_ptbl *rm_face_indices, const struct rt_bot_internal *orig_bot)
{
    if (!rm_face_indices || !BU_PTBL_LEN(rm_face_indices))
	return NULL;


    std::unordered_set<size_t> rm_indices;
    for (size_t i = 0; i < BU_PTBL_LEN(rm_face_indices); i++) {
	size_t ind = (size_t)(uintptr_t)BU_PTBL_GET(rm_face_indices, i);
	rm_indices.insert(ind);
    }

    int *nfaces = (int *)bu_calloc(orig_bot->num_faces * 3, sizeof(int), "new faces array");
    size_t nfaces_ind = 0;
    for (size_t i = 0; i < orig_bot->num_faces; i++) {
	if (rm_indices.find(i) != rm_indices.end())
	    continue;
	nfaces[3*nfaces_ind + 0] = orig_bot->faces[3*i+0];
	nfaces[3*nfaces_ind + 1] = orig_bot->faces[3*i+1];
	nfaces[3*nfaces_ind + 2] = orig_bot->faces[3*i+2];
	nfaces_ind++;
    }

    // Having built a faces array with the specified triangles removed, we now
    // garbage collect to produce re-indexed face and point arrays with just the
    // active data (vertices may be no longer active in the BoT depending on
    // which faces were removed.
    int *nfacesarray = NULL;
    point_t *npointsarray = NULL;
    int npntcnt = 0;
    int new_num_faces = bg_trimesh_3d_gc(&nfacesarray, &npointsarray, &npntcnt, nfaces, nfaces_ind, (const point_t *)orig_bot->vertices);

    if (new_num_faces < 3) {
	new_num_faces = 0;
	npntcnt = 0;
	bu_free(nfacesarray, "nfacesarray");
	nfacesarray = NULL;
	bu_free(npointsarray, "npointsarray");
	npointsarray = NULL;
    }

    // Done with the nfaces array
    bu_free(nfaces, "free unmapped new faces array");

    // Make the new rt_bot_internal
    struct rt_bot_internal *bot = NULL;
    BU_GET(bot, struct rt_bot_internal);
    bot->magic = RT_BOT_INTERNAL_MAGIC;
    bot->mode = orig_bot->mode;
    bot->orientation = orig_bot->orientation;
    bot->bot_flags = orig_bot->bot_flags;
    bot->num_vertices = npntcnt;
    bot->num_faces = new_num_faces;
    bot->vertices = (fastf_t *)npointsarray;
    bot->faces = nfacesarray;

    // TODO - need to properly rebuild these arrays as well, if orig_bot has them - bg_trimesh_3d_gc only
    // handles the vertices themselves
    bot->thickness = NULL;
    bot->face_mode = NULL;
    bot->normals = NULL;
    bot->face_normals = NULL;
    bot->uvs = NULL;
    bot->face_uvs = NULL;

    return bot;
}

struct rt_bot_internal *
rt_bot_dup(const struct rt_bot_internal *obot)
{
    if (!obot)
	return NULL;

    struct rt_bot_internal *bot = NULL;
    BU_GET(bot, struct rt_bot_internal);
    bot->magic = obot->magic;
    bot->mode = obot->mode;
    bot->orientation = obot->orientation;
    bot->bot_flags = obot->bot_flags;

    bot->num_faces = obot->num_faces;
    bot->faces = (int *)bu_malloc(obot->num_faces * sizeof(int)*3, "bot faces");
    memcpy(bot->faces, obot->faces, obot->num_faces * sizeof(int)*3);

    bot->num_vertices = obot->num_vertices;
    bot->vertices = (fastf_t*)bu_malloc(obot->num_vertices * sizeof(fastf_t)*3, "bot verts");
    memcpy(bot->vertices, obot->vertices, obot->num_vertices * sizeof(fastf_t)*3);

    if (obot->thickness) {
	bot->thickness = (fastf_t*)bu_malloc(obot->num_faces * sizeof(fastf_t), "bot thicknesses");
	memcpy(bot->thickness, obot->thickness, obot->num_faces * sizeof(fastf_t));
    }

    if (obot->face_mode) {
	bot->face_mode = (struct bu_bitv *)bu_malloc(obot->num_faces * sizeof(struct bu_bitv), "bot face_mode");
	memcpy(bot->face_mode, obot->face_mode, obot->num_faces * sizeof(struct bu_bitv));
    }

    if (obot->normals && obot->num_normals) {
	bot->num_normals = obot->num_normals;
	bot->normals = (fastf_t*)bu_malloc(obot->num_normals * sizeof(fastf_t)*3, "bot normals");
	memcpy(bot->normals, obot->normals, obot->num_normals * sizeof(fastf_t)*3);
    }

    if (obot->face_normals && obot->num_face_normals) {
	bot->num_face_normals = obot->num_face_normals;
	bot->face_normals = (int*)bu_malloc(obot->num_face_normals * sizeof(int)*3, "bot face normals");
	memcpy(bot->face_normals, obot->face_normals, obot->num_face_normals * sizeof(int)*3);
    }

    if (obot->num_uvs && obot->uvs) {
	bot->num_uvs = obot->num_uvs;
	bot->uvs = (fastf_t*)bu_malloc(obot->num_uvs * sizeof(fastf_t)*3, "bot uvs");
	memcpy(bot->uvs, obot->uvs, obot->num_uvs * sizeof(fastf_t)*3);
    }

    if (obot->num_face_uvs && obot->face_uvs) {
	bot->num_face_uvs = obot->num_face_uvs;
	bot->face_uvs = (int*)bu_malloc(obot->num_face_uvs * sizeof(int)*3, "bot face_uvs");
	memcpy(bot->face_uvs, obot->face_uvs, obot->num_face_uvs * sizeof(int)*3);
    }

    return bot;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
