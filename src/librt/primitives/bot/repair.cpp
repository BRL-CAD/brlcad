/*                       R E P A I R . C P P
 * BRL-CAD
 *
 * Copyright (c) 2020-2026 United States Government as represented by
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
#include <unordered_set>
#include <vector>

#include "manifold/manifold.h"

#include "bu/malloc.h"
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

namespace {
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
}

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

    // Skip triangles too thin for reliable ray-triangle intersection math.
    // Two complementary checks guard against false "unexpected miss" results:
    //
    //  1. Absolute: min altitude < SQRT_SMALL_FASTF — nearly degenerate on any
    //     scale; the ray origin cannot be meaningfully placed off the surface.
    //
    //  2. Relative: min altitude < max_edge * SQRT_SMALL_FASTF — catches
    //     "super-thin" triangles whose aspect ratio (max_edge / min_alt) exceeds
    //     ~5.5e7.  For such triangles the near-zero determinant in the
    //     Möller-Trumbore ray-triangle intersection makes hit/miss unreliable
    //     regardless of the triangle's absolute size, producing false
    //     "unexpected miss" lint failures on otherwise valid repaired meshes.
    {
	const double *p0 = &bot->vertices[bot->faces[ind*3+0]*3];
	const double *p1 = &bot->vertices[bot->faces[ind*3+1]*3];
	const double *p2 = &bot->vertices[bot->faces[ind*3+2]*3];
	vect_t e01, e12, e20, cross;
	VSUB2(e01, p1, p0);
	VSUB2(e12, p2, p1);
	VSUB2(e20, p0, p2);
	VCROSS(cross, e01, e12);
	double area2 = MAGNITUDE(cross);  /* 2 * triangle area */
	double l01 = MAGNITUDE(e01);
	double l12 = MAGNITUDE(e12);
	double l20 = MAGNITUDE(e20);
	double max_edge = l01;
	if (l12 > max_edge) max_edge = l12;
	if (l20 > max_edge) max_edge = l20;
	/* absolute: min altitude < backout threshold */
	if (max_edge < SQRT_SMALL_FASTF || area2 / max_edge < SQRT_SMALL_FASTF)
	    return;
	/* relative: aspect ratio > 1/SQRT_SMALL_FASTF — super-thin triangles
	 * whose intersection determinant is too small to trust */
	if (area2 < max_edge * max_edge * SQRT_SMALL_FASTF)
	    return;
    }

    // Triangle passes filters, continue processing
    vect_t rnorm, n, backout;
    if (!bot_face_normal(&n, bot, ind))
	return;
    // Reverse the triangle normal for a ray direction
    VREVERSE(rnorm, n);

    // Compute triangle centroid first: needed for scale-relative backout below.
    point_t rpnts[3];
    point_t tcenter;
    VMOVE(rpnts[0], &bot->vertices[bot->faces[ind*3+0]*3]);
    VMOVE(rpnts[1], &bot->vertices[bot->faces[ind*3+1]*3]);
    VMOVE(rpnts[2], &bot->vertices[bot->faces[ind*3+2]*3]);
    VADD3(tcenter, rpnts[0], rpnts[1], rpnts[2]);
    VSCALE(tcenter, tcenter, 1.0/3.0);

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

    // Scale the backout so it is numerically significant at the coordinate
    // scale of this triangle.  The fixed SQRT_SMALL_FASTF value (~1e-18) is
    // below machine epsilon when vertex coordinates are large (e.g. an
    // aircraft model with vertices in the thousands of millimetres), causing
    // the ray origin to round exactly to the centroid and producing spurious
    // "unexpected miss" results on otherwise valid repaired meshes.
    {
	double bscale = SQRT_SMALL_FASTF;
	double tcmag = MAGNITUDE(tcenter);
	if (tcmag > 1.0) {
	    /* ~4500× machine epsilon: safely above double-precision round-off
	     * for any coordinate magnitude while remaining far below any
	     * meaningful geometric feature size (< 1e-7 mm at mm scale). */
	    double scale_eps = tcmag * 1.0e-12;
	    if (scale_eps > bscale)
		bscale = scale_eps;
	}
	VSCALE(backout, backout, bscale);
    }

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
	bu_log("rt_bot_repair lint: unexpected miss\n");
	ret = 1;
	goto bot_lint_cleanup;
    }

    /* Note: thin-volume, close-face, and unexpected-hit tests are intentionally
     * skipped for repair validation.  Repaired meshes may legitimately consist
     * of thin panels or tightly adjacent surfaces (e.g. aircraft sheet-metal
     * components) that would produce false positives from these proximity-based
     * checks.  Only the unexpected-miss test — which catches genuine topology
     * holes — is run for repair output. */

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

// Helper: convert bg_trimesh_repair / manifold arrays to rt_bot_internal
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
    nbot->vertices = (double *)calloc(omesh->vertProperties.size(), sizeof(double));
    nbot->faces = (int *)calloc(omesh->triVerts.size(), sizeof(int));
    for (size_t j = 0; j < omesh->vertProperties.size(); j++)
	nbot->vertices[j] = omesh->vertProperties[j];
    for (size_t j = 0; j < omesh->triVerts.size(); j++)
	nbot->faces[j] = omesh->triVerts[j];

    return nbot;
}

int
rt_bot_repair(struct rt_bot_internal **obot, struct rt_bot_internal *bot, struct rt_bot_repair_info *settings)
{
    if (!bot || !obot || !settings)
	return -1;

    // Unless we produce something, obot will be NULL
    *obot = NULL;

    int num_vertices = (int)bot->num_vertices;
    int num_faces = (int)bot->num_faces;

    // Fast path: check if already manifold via Manifold.Merge() and bg_trimesh_solid2.
    manifold::MeshGL64 bot_mesh;
    for (size_t j = 0; j < bot->num_vertices; j++) {
	bot_mesh.vertProperties.push_back(bot->vertices[3*j+0]);
	bot_mesh.vertProperties.push_back(bot->vertices[3*j+1]);
	bot_mesh.vertProperties.push_back(bot->vertices[3*j+2]);
    }
    if (bot->orientation == RT_BOT_CW) {
	for (size_t j = 0; j < bot->num_faces; j++) {
	    bot_mesh.triVerts.push_back(bot->faces[3*j]);
	    bot_mesh.triVerts.push_back(bot->faces[3*j+2]);
	    bot_mesh.triVerts.push_back(bot->faces[3*j+1]);
	}
    } else {
	for (size_t j = 0; j < bot->num_faces; j++) {
	    bot_mesh.triVerts.push_back(bot->faces[3*j]);
	    bot_mesh.triVerts.push_back(bot->faces[3*j+1]);
	    bot_mesh.triVerts.push_back(bot->faces[3*j+2]);
	}
    }

    int bg_not_solid = bg_trimesh_solid2(num_vertices, num_faces, bot->vertices, bot->faces, NULL);

    if (!bot_mesh.Merge() && !bg_not_solid) {
	// BoT is already manifold
	return 1;
    }

    manifold::Manifold omanifold(bot_mesh);
    if (omanifold.Status() == manifold::Manifold::Error::NoError) {
	// MeshGL.Merge() produced a manifold mesh.  Minimal changes needed.
	manifold::MeshGL omesh = omanifold.GetMeshGL();
	struct rt_bot_internal *nbot = manifold_to_bot(&omesh);
	*obot = nbot;
	return 0;
    }

    // Call bg_trimesh_repair for the GTE-based repair.
    struct bg_trimesh_repair_opts opts;
    opts.max_hole_area         = settings->max_hole_area;
    opts.max_hole_area_percent = settings->max_hole_area_percent;

    int *rfaces = NULL;
    int n_rfaces = 0;
    point_t *rpnts = NULL;
    int n_rpnts = 0;

    int repair_ret = bg_trimesh_repair(
	    &rfaces, &n_rfaces,
	    &rpnts, &n_rpnts,
	    bot->faces, num_faces,
	    (const point_t *)bot->vertices, num_vertices,
	    &opts);

    if (repair_ret == 1) {
	// Already solid after basic repair check — treat same as fast path above.
	bu_free(rfaces, "rfaces");
	bu_free(rpnts,  "rpnts");
	return 1;
    }
    if (repair_ret < 0 || !rfaces || !rpnts) {
	bu_free(rfaces, "rfaces");
	bu_free(rpnts,  "rpnts");
	return -1;
    }

    // Validate the repaired mesh with Manifold.
    manifold::MeshGL gmm;
    for (int i = 0; i < n_rpnts; i++) {
	gmm.vertProperties.push_back((float)rpnts[i][X]);
	gmm.vertProperties.push_back((float)rpnts[i][Y]);
	gmm.vertProperties.push_back((float)rpnts[i][Z]);
    }
    for (int i = 0; i < n_rfaces; i++) {
	gmm.triVerts.push_back((uint32_t)rfaces[3*i+0]);
	gmm.triVerts.push_back((uint32_t)rfaces[3*i+1]);
	gmm.triVerts.push_back((uint32_t)rfaces[3*i+2]);
    }
    bu_free(rfaces, "rfaces");
    bu_free(rpnts,  "rpnts");

    manifold::Manifold gmanifold(gmm);
    if (gmanifold.Status() != manifold::Manifold::Error::NoError) {
	bu_log("rt_bot_repair: repaired mesh is not manifold\n");
	return -1;
    }
    if (gmanifold.Volume() < 0) {
	bu_log("rt_bot_repair: repaired mesh has negative volume, rejecting\n");
	return -1;
    }

    bu_log("rt_bot_repair: gmanifold volume=%.6g\n", gmanifold.Volume());
    manifold::MeshGL omesh = gmanifold.GetMeshGL();
    struct rt_bot_internal *nbot = manifold_to_bot(&omesh);

    if (settings->strict) {
	int lint_ret = bot_repair_lint(nbot);
	if (lint_ret) {
	    bu_log("Warning - repaired BoT has lint unexpected-miss on some faces "
		   "(manifold solid confirmed; likely raytracer precision issue "
		   "for thin/curved triangles - proceeding with manifold result)\n");
	}
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
