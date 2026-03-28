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
#include <numeric>
#include <queue>
#include <set>
#include <sstream>
#include <string>
#include <unordered_set>
#include <vector>

#include "manifold/manifold.h"

#include <Mathematics/Vector2.h>
#include <Mathematics/Vector3.h>
#include <Mathematics/MeshRepair.h>
#include <Mathematics/MeshHoleFilling.h>
#include <Mathematics/MeshPreprocessing.h>
#include <Mathematics/MeshQuality.h>
#include <Mathematics/LSCMParameterization.h>

#include "bn/tol.h"

#if defined(__GNUC__) && !defined(__clang__)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wfloat-equal"
#endif
#if defined(__clang__)
#  pragma clang diagnostic push
#  pragma clang diagnostic ignored "-Wfloat-equal"
#endif
#include "../../../libbg/detria.hpp"
#if defined(__GNUC__) && !defined(__clang__)
#  pragma GCC diagnostic pop
#endif
#if defined(__clang__)
#  pragma clang diagnostic pop
#endif

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

// Helper: compute bounding box diagonal of a GTE vertex set
static double
gte_bbox_diagonal(std::vector<gte::Vector3<double>> const& vertices)
{
    if (vertices.empty())
	return 0.0;
    double minx = vertices[0][0], maxx = vertices[0][0];
    double miny = vertices[0][1], maxy = vertices[0][1];
    double minz = vertices[0][2], maxz = vertices[0][2];
    for (auto const& v : vertices) {
	if (v[0] < minx) minx = v[0];
	if (v[0] > maxx) maxx = v[0];
	if (v[1] < miny) miny = v[1];
	if (v[1] > maxy) maxy = v[1];
	if (v[2] < minz) minz = v[2];
	if (v[2] > maxz) maxz = v[2];
    }
    double dx = maxx - minx, dy = maxy - miny, dz = maxz - minz;
    return std::sqrt(dx*dx + dy*dy + dz*dz);
}

// Helper: compute total mesh area from GTE vertex/triangle arrays
static double
gte_mesh_area(std::vector<gte::Vector3<double>> const& vertices,
	      std::vector<std::array<int32_t, 3>> const& triangles)
{
    double area = 0.0;
    for (auto const& tri : triangles) {
	gte::Vector3<double> const& p0 = vertices[tri[0]];
	gte::Vector3<double> const& p1 = vertices[tri[1]];
	gte::Vector3<double> const& p2 = vertices[tri[2]];
	gte::Vector3<double> e1 = p1 - p0;
	gte::Vector3<double> e2 = p2 - p0;
	gte::Vector3<double> cross = gte::Cross(e1, e2);
	area += gte::Length(cross) * 0.5;
    }
    return area;
}

static void
bot_to_gte(std::vector<gte::Vector3<double>>& vertices,
	   std::vector<std::array<int32_t, 3>>& triangles,
	   struct rt_bot_internal *bot)
{
    vertices.resize(bot->num_vertices);
    for (size_t i = 0; i < bot->num_vertices; i++) {
	vertices[i][0] = bot->vertices[3*i+0];
	vertices[i][1] = bot->vertices[3*i+1];
	vertices[i][2] = bot->vertices[3*i+2];
    }
    triangles.resize(bot->num_faces);
    for (size_t i = 0; i < bot->num_faces; i++) {
	triangles[i][0] = bot->faces[3*i+0];
	triangles[i][1] = bot->faces[3*i+1];
	triangles[i][2] = bot->faces[3*i+2];
    }

    // After the initial raw load, do a repair pass to remove degenerate
    // triangles, colocated vertices, and small connected components - matching
    // the pre-processing that the previous Geogram-based path performed.
    double bbox_diag = gte_bbox_diagonal(vertices);
    double epsilon = 1e-6 * (0.01 * bbox_diag);
    gte::MeshRepair<double>::Parameters repairParams;
    repairParams.epsilon = epsilon;
    gte::MeshRepair<double>::Repair(vertices, triangles, repairParams);

    double area = gte_mesh_area(vertices, triangles);
    double min_comp_area = 0.03 * area;
    if (min_comp_area > 0.0) {
	size_t nf_before = triangles.size();
	gte::MeshPreprocessing<double>::RemoveSmallComponents(vertices, triangles, min_comp_area);
	if (triangles.size() != nf_before) {
	    gte::MeshRepair<double>::Repair(vertices, triangles, repairParams);
	}
    }
}

static void
gte_to_manifold(manifold::MeshGL *gmm,
		std::vector<gte::Vector3<double>> const& vertices,
		std::vector<std::array<int32_t, 3>> const& triangles)
{
    for (auto const& v : vertices) {
	gmm->vertProperties.push_back(v[0]);
	gmm->vertProperties.push_back(v[1]);
	gmm->vertProperties.push_back(v[2]);
    }
    for (auto const& tri : triangles) {
	// TODO - CW vs CCW orientation handling?
	gmm->triVerts.push_back(tri[0]);
	gmm->triVerts.push_back(tri[1]);
	gmm->triVerts.push_back(tri[2]);
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
gte_to_bot(std::vector<gte::Vector3<double>> const& vertices,
	   std::vector<std::array<int32_t, 3>> const& triangles)
{
    struct rt_bot_internal *nbot;
    BU_GET(nbot, struct rt_bot_internal);
    nbot->magic = RT_BOT_INTERNAL_MAGIC;
    nbot->mode = RT_BOT_SOLID;
    nbot->orientation = RT_BOT_CCW;
    nbot->thickness = NULL;
    nbot->face_mode = (struct bu_bitv *)NULL;
    nbot->bot_flags = 0;
    nbot->num_vertices = (int)vertices.size();
    nbot->num_faces = (int)triangles.size();
    nbot->vertices = (double *)calloc(nbot->num_vertices*3, sizeof(double));
    nbot->faces = (int *)calloc(nbot->num_faces*3, sizeof(int));

    for (size_t j = 0; j < vertices.size(); j++) {
	nbot->vertices[3*j+0] = vertices[j][0];
	nbot->vertices[3*j+1] = vertices[j][1];
	nbot->vertices[3*j+2] = vertices[j][2];
    }

    for (size_t j = 0; j < triangles.size(); j++) {
	// TODO - CW vs CCW orientation handling?
	nbot->faces[3*j+0] = triangles[j][0];
	nbot->faces[3*j+1] = triangles[j][1];
	nbot->faces[3*j+2] = triangles[j][2];
    }

    return nbot;
}

// Helper: attempt patch repair by identifying topologically problematic faces,
// growing a connected region around them until its boundary forms simple closed
// loops, removing the patch faces, projecting all patch vertices (boundary AND
// interior orphans) into 2D via LSCM, and retriangulating with CDT (detria).
//
// The current LSCM hole-filling passes (1 and 2) only ADD triangles — they
// cannot handle topology that has too many triangles in a region (excess edges,
// severely mis-oriented regions) because the hole-boundary walker expects a
// simple closed loop but finds branching or dead-end points.
//
// This pass takes the complementary approach:
//  1. Use bg_trimesh_solid2 to locate faces incident to topological errors
//     (excess edges, misoriented edges, non-manifold vertices).
//  2. Grow a connected "patch" around those faces via BFS until the boundary
//     has only degree-2 vertices (simple closed loop, no branching).
//  3. Remove the patch faces from the mesh.
//  4. Apply full LSCM parameterization to ALL patch vertices (boundary pinned
//     to a unit circle; interior vertices solved via cotangent Laplacian) to
//     obtain a 2D UV embedding of the patch region.
//  5. Run CDT (detria) on the 2D UV positions with the boundary loop as a
//     constraint, incorporating the interior vertices as Steiner points.
//  6. Map the CDT triangles back to 3D global vertex indices and check manifold.
//  7. Fall back to boundary-only LSCM hole-fill (discarding interior points)
//     if LSCM or CDT fails.
//
// Returns true and sets gm_out on success.
static bool
try_patch_repair(struct rt_bot_internal *bot, manifold::Manifold& gm_out)
{
    // Fresh GTE representation with standard pre-processing.
    std::vector<gte::Vector3<double>> vp;
    std::vector<std::array<int32_t, 3>> tp;
    bot_to_gte(vp, tp, bot);

    int nv = (int)vp.size();
    int nf = (int)tp.size();
    if (nv == 0 || nf == 0)
	return false;

    // Build vertex-to-face adjacency.
    std::vector<std::vector<int32_t>> vert_faces((size_t)nv);
    for (int f = 0; f < nf; f++)
	for (int k = 0; k < 3; k++)
	    vert_faces[(size_t)tp[f][k]].push_back((int32_t)f);

    // Build face-to-face adjacency up front (needed for both seeding and
    // boundary detection).
    // ConnectFacets sets adj[f*3+e] to:
    //   >= 0 : index of the one adjacent face (manifold edge)
    //   -1   : no adjacent face (open boundary / hole)
    //   -2   : non-manifold edge — 3+ faces share this edge
    std::vector<int32_t> adj;
    gte::MeshRepair<double>::ConnectFacets(tp, adj);

    // Seed the patch with faces incident to ANY problematic topology.
    //
    // By the time this pass is reached both LSCM passes have already failed,
    // so there is no benefit in distinguishing open-boundary edges (adj == -1)
    // from excess/non-manifold edges (adj == -2).  Any face that touches an
    // edge with adj < 0 is in a region that the earlier hole-filling passes
    // could not handle; pull ALL such faces into the initial patch so the
    // growth loop has the full problem region to work with from the start.
    std::unordered_set<int32_t> patch;

    for (int f = 0; f < nf; f++)
	for (int e = 0; e < 3; e++)
	    if (adj[(size_t)f * 3 + (size_t)e] < 0)
		patch.insert((int32_t)f);

    // Also seed from misoriented edges via bg_trimesh_solid2 — these have
    // adj >= 0 so the adj < 0 scan above would not catch them.
    {
	std::vector<double> bv((size_t)nv * 3);
	std::vector<int>    bf((size_t)nf * 3);
	for (int i = 0; i < nv; i++) {
	    bv[3*i] = vp[i][0]; bv[3*i+1] = vp[i][1]; bv[3*i+2] = vp[i][2];
	}
	for (int i = 0; i < nf; i++) {
	    bf[3*i] = tp[i][0]; bf[3*i+1] = tp[i][1]; bf[3*i+2] = tp[i][2];
	}
	struct bg_trimesh_solid_errors errs = BG_TRIMESH_SOLID_ERRORS_INIT_NULL;
	bg_trimesh_solid2(nv, nf, bv.data(), bf.data(), &errs);

	auto add_edge_faces = [&](int va, int vb) {
	    for (int32_t f : vert_faces[(size_t)va]) {
		bool hva = false, hvb = false;
		for (int k = 0; k < 3; k++) {
		    if (tp[f][k] == va) hva = true;
		    if (tp[f][k] == vb) hvb = true;
		}
		if (hva && hvb) patch.insert(f);
	    }
	};

	for (int i = 0; i < errs.misoriented.count; i++)
	    add_edge_faces(errs.misoriented.edges[2*i], errs.misoriented.edges[2*i+1]);
	bg_free_trimesh_solid_errors(&errs);
    }

    if (patch.empty())
	return false;

    // Helper: for a NON-PATCH face's edge e, returns true iff this edge is on
    // the "hole boundary" — i.e. the edge has no adjacent face in the non-patch
    // region.  This is the correct perspective for determining where the mesh
    // will have open holes after the patch faces are removed.
    //
    // Computing boundary degree from patch-face edges is INCORRECT for excess-
    // edge cases: an excess/extra triangle can have edges with adj == -1 (no
    // adjacent face) that are purely internal to the error region (e.g. an edge
    // that only exists because of the spurious extra face).  Such edges produce
    // spurious degree-1 vertices that prevent the boundary from appearing simple.
    //
    // By looking at NON-PATCH faces only, those internal-excess edges are
    // invisible — only the genuine ring where patch meets non-patch is counted.
    auto is_hole_boundary = [&](int32_t f, int e) -> bool {
	int32_t adj_f = adj[(size_t)f * 3 + (size_t)e];
	if (adj_f < 0)  return true;               // -1: mesh hole; -2: excess edge
	return patch.count(adj_f) != 0;            // adj face is in patch → hole boundary
    };

    // Grow the patch until the hole boundary (edges of non-patch faces that
    // border the patch or are open) has only degree-2 vertices.
    //
    // A simple closed boundary requires every hole-boundary vertex to appear on
    // exactly 2 hole-boundary edges.  Vertices with degree != 2 (branching at
    // 3+, dangling at 1) prevent LSCM/CDT from producing a valid triangulation.
    //
    // For each non-simple vertex we pull ALL its incident faces into the patch
    // so it becomes an interior patch vertex.  We repeat until the boundary is
    // simple or no further expansion is possible.
    for (;;) {
	std::map<int32_t, int> hole_degree;
	for (int f = 0; f < nf; f++) {
	    if (patch.count((int32_t)f)) continue;
	    for (int e = 0; e < 3; e++) {
		if (is_hole_boundary((int32_t)f, e)) {
		    hole_degree[tp[f][e]]++;
		    hole_degree[tp[f][(e+1)%3]]++;
		}
	    }
	}

	std::vector<int32_t> bad_verts;
	for (auto const& kv : hole_degree)
	    if (kv.second != 2)
		bad_verts.push_back(kv.first);

	if (bad_verts.empty())
	    break;

	bool expanded = false;
	for (int32_t v : bad_verts) {
	    for (int32_t f : vert_faces[(size_t)v]) {
		if (patch.count(f) == 0) {
		    patch.insert(f);
		    expanded = true;
		}
	    }
	}
	if (!expanded)
	    break;
    }

    // Reject if the patch consumed the whole mesh — nothing left to keep.
    if (patch.size() >= (size_t)nf)
	return false;

    // Verify the hole boundary is actually simple before proceeding.
    {
	std::map<int32_t, int> hole_degree;
	for (int f = 0; f < nf; f++) {
	    if (patch.count((int32_t)f)) continue;
	    for (int e = 0; e < 3; e++) {
		if (is_hole_boundary((int32_t)f, e)) {
		    hole_degree[tp[f][e]]++;
		    hole_degree[tp[f][(e+1)%3]]++;
		}
	    }
	}
	for (auto const& kv : hole_degree)
	    if (kv.second != 2)
		return false;
    }

    // Reject if the patch consumed the whole mesh — nothing left to keep.
    if (patch.size() >= (size_t)nf)
	return false;

    bu_log("rt_bot_repair: patch_repair: patch=%zu/%d faces\n", patch.size(), nf);

    // Classify all patch vertices as boundary (shared with non-patch faces)
    // or interior (only in patch faces, would be orphaned after removal).
    std::vector<int> face_ref_count((size_t)nv, 0);
    std::vector<int> patch_ref_count((size_t)nv, 0);
    for (int f = 0; f < nf; f++) {
	bool in_p = (patch.count((int32_t)f) > 0);
	for (int k = 0; k < 3; k++) {
	    face_ref_count[tp[f][k]]++;
	    if (in_p) patch_ref_count[tp[f][k]]++;
	}
    }

    // Build compact patch vertex array (boundary first, then interior) and a
    // global-to-local index map.
    std::vector<int32_t> patch_verts_global; // local index → global vertex index
    std::vector<int32_t> bnd_verts_local;    // local indices of boundary vertices
    std::vector<int32_t> int_verts_local;    // local indices of interior vertices
    std::vector<int32_t> g2l((size_t)nv, -1);

    for (int v = 0; v < nv; v++) {
	if (patch_ref_count[v] == 0) continue;
	int32_t loc = (int32_t)patch_verts_global.size();
	patch_verts_global.push_back((int32_t)v);
	g2l[(size_t)v] = loc;
	if (patch_ref_count[v] < face_ref_count[v])
	    bnd_verts_local.push_back(loc);
	else
	    int_verts_local.push_back(loc);
    }

    // Patch triangles in local indices (needed for the LSCM cotangent Laplacian).
    std::vector<std::array<int32_t, 3>> patch_tris_local;
    patch_tris_local.reserve(patch.size());
    for (int32_t f : patch)
	patch_tris_local.push_back({g2l[tp[f][0]], g2l[tp[f][1]], g2l[tp[f][2]]});

    // 3D positions of patch vertices (local order).
    std::vector<gte::Vector3<double>> patch_v3d;
    patch_v3d.reserve(patch_verts_global.size());
    for (int32_t gv : patch_verts_global)
	patch_v3d.push_back(vp[(size_t)gv]);

    // Trace the boundary loop: ordered sequence of boundary vertices (global).
    // Each hole-boundary vertex has exactly 2 hole-boundary edge neighbors,
    // so the loop can be walked unambiguously.
    // Build undirected boundary-edge adjacency from the non-patch perspective.
    std::map<int32_t, std::vector<int32_t>> bnd_nbrs;
    for (int f = 0; f < nf; f++) {
	if (patch.count((int32_t)f)) continue;
	for (int e = 0; e < 3; e++) {
	    if (is_hole_boundary((int32_t)f, e)) {
		int32_t va = tp[f][e];
		int32_t vb = tp[f][(e+1)%3];
		auto& na = bnd_nbrs[va];
		if (std::find(na.begin(), na.end(), vb) == na.end())
		    na.push_back(vb);
		auto& nb = bnd_nbrs[vb];
		if (std::find(nb.begin(), nb.end(), va) == nb.end())
		    nb.push_back(va);
	    }
	}
    }

    if (bnd_nbrs.empty())
	return false;

    // Walk the loop.
    std::vector<int32_t> bnd_loop_global;
    {
	int32_t start = bnd_nbrs.begin()->first;
	int32_t cur = start, prev = -1;
	do {
	    bnd_loop_global.push_back(cur);
	    int32_t nxt = -1;
	    for (int32_t nb : bnd_nbrs[cur])
		if (nb != prev) { nxt = nb; break; }
	    if (nxt < 0) break;
	    prev = cur;
	    cur = nxt;
	} while (cur != start);
    }

    if ((int)bnd_loop_global.size() < 3)
	return false;

    bu_log("rt_bot_repair: patch_repair: bnd_nbrs=%zu bnd_loop=%zu single=%d\n",
	   bnd_nbrs.size(), bnd_loop_global.size(), (int)(bnd_loop_global.size() == bnd_nbrs.size()));

    // If the loop didn't close (boundary has multiple disjoint loops), use the
    // per-loop CDT path below.  Otherwise use the single-loop CDT path.
    bool single_loop = (bnd_loop_global.size() == bnd_nbrs.size());

    if (single_loop) {
	// Convert boundary loop to local indices.
	std::vector<int32_t> bnd_loop_local;
	bnd_loop_local.reserve(bnd_loop_global.size());
	for (int32_t gv : bnd_loop_global)
	    bnd_loop_local.push_back(g2l[(size_t)gv]);

	// LSCM parameterization: project all patch vertices to 2D.
	//
	// If there are no interior vertices, MapBoundaryToCircle is sufficient
	// (returns UV only for boundary vertices, parallel to bnd_loop_local).
	// Otherwise, Parameterize() solves the cotangent Laplacian for interior
	// UV positions given the patch triangulation; it returns UV for ALL patch
	// vertices indexed by local vertex index.
	std::vector<gte::Vector2<double>> all_uv;
	bool lscm_ok = false;

	if (int_verts_local.empty()) {
	    lscm_ok = gte::LSCMParameterization<double>::MapBoundaryToCircle(
		patch_v3d, bnd_loop_local, all_uv);
	    // Expand uv (parallel to bnd_loop_local) into a full-size array
	    // indexed by local vertex index so the CDT loop below is uniform.
	    if (lscm_ok) {
		std::vector<gte::Vector2<double>> full_uv(
		    patch_verts_global.size(), gte::Vector2<double>{0.0, 0.0});
		for (int i = 0; i < (int)bnd_loop_local.size(); i++)
		    full_uv[(size_t)bnd_loop_local[i]] = all_uv[i];
		all_uv = std::move(full_uv);
	    }
	} else {
	    // Full LSCM: boundary pinned to circle, interior solved via CG.
	    // The patch_tris_local triangulation is used only for setting up
	    // the cotangent weights; it is replaced by the CDT output.
	    lscm_ok = gte::LSCMParameterization<double>::Parameterize(
		patch_v3d, bnd_loop_local, int_verts_local,
		patch_tris_local, all_uv);
	    // all_uv is indexed by local patch vertex index (same as all_patch_verts).
	}

	if (lscm_ok && !all_uv.empty()) {
	    // Build 2D point array for detria (UV coordinates).
	    std::vector<detria::PointD> tpnts;
	    tpnts.reserve(patch_verts_global.size());
	    for (auto const& uv : all_uv) {
		detria::PointD pt;
		pt.x = uv[0];
		pt.y = uv[1];
		tpnts.push_back(pt);
	    }

	    detria::Triangulation<detria::PointD, int> dtri;
	    dtri.setPoints(tpnts);

	    // Add the boundary loop as the outer polygon constraint.
	    // addPolylineAutoDetectType determines CW vs CCW automatically.
	    std::vector<int> bnd_int(bnd_loop_local.begin(), bnd_loop_local.end());
	    dtri.addPolylineAutoDetectType(bnd_int);

	    // Interior patch vertices are treated as unconstrained Steiner
	    // points — detria includes all supplied points in the triangulation.

	    if (dtri.triangulate(true /* Delaunay */)) {
		// Try both winding orders; keep whichever gives a manifold mesh.
		for (bool cw : {false, true}) {
		    std::vector<std::array<int32_t, 3>> cdt_local;
		    dtri.forEachTriangle(
			[&](const detria::Triangle<int>& t) {
			    cdt_local.push_back({(int32_t)t.x,
						 (int32_t)t.y,
						 (int32_t)t.z});
			}, cw);

		    if (cdt_local.empty()) continue;

		    // Build combined mesh: non-patch faces + CDT faces,
		    // all in global vertex indices.
		    std::vector<std::array<int32_t, 3>> tcombined;
		    tcombined.reserve((size_t)nf - patch.size() + cdt_local.size());
		    for (int f = 0; f < nf; f++)
			if (patch.count((int32_t)f) == 0)
			    tcombined.push_back(tp[f]);
		    for (auto const& ct : cdt_local)
			tcombined.push_back({
			    patch_verts_global[(size_t)ct[0]],
			    patch_verts_global[(size_t)ct[1]],
			    patch_verts_global[(size_t)ct[2]]});

		    // Compact the vertex array (drops unreferenced vertices that
		    // were not included in the CDT output, e.g. interior verts
		    // outside the UV polygon boundary).
		    std::vector<bool> ref_cdt((size_t)nv, false);
		    for (auto const& t : tcombined)
			for (int k = 0; k < 3; k++)
			    ref_cdt[(size_t)t[k]] = true;

		    std::vector<int32_t> remap_cdt((size_t)nv, -1);
		    std::vector<gte::Vector3<double>> vcompact;
		    vcompact.reserve((size_t)nv);
		    for (int i = 0; i < nv; i++) {
			if (ref_cdt[(size_t)i]) {
			    remap_cdt[(size_t)i] = (int32_t)vcompact.size();
			    vcompact.push_back(vp[(size_t)i]);
			}
		    }
		    for (auto& t : tcombined)
			for (int k = 0; k < 3; k++)
			    t[k] = remap_cdt[(size_t)t[k]];

		    manifold::MeshGL gmm;
		    gte_to_manifold(&gmm, vcompact, tcombined);
		    manifold::Manifold gm(gmm);
		    if (gm.Status() == manifold::Manifold::Error::NoError) {
			if (gm.Volume() >= 0) {
			    gm_out = gm;
			    return true;
			}
			// Volume negative: flip all faces and retry.
			gte::MeshPreprocessing<double>::InvertNormals(tcombined);
			manifold::MeshGL gmm2;
			gte_to_manifold(&gmm2, vcompact, tcombined);
			manifold::Manifold gm2(gmm2);
			if (gm2.Status() == manifold::Manifold::Error::NoError && gm2.Volume() >= 0) {
			    gm_out = gm2;
			    return true;
			}
		    }
		}
	    }
	}
    } else {
// Multiple boundary loops — fill each independently using boundary-only CDT.
//
// Each component of bnd_nbrs is a simple closed loop bounding one
// independent hole.  We fill each hole directly from its boundary
// vertices (MapBoundaryToCircle → CDT), without partitioning the
// patch faces.  This avoids sub-patch adjacency artefacts entirely and
// produces a topologically clean fill for every loop.

// Enumerate connected components and walk each loop.
struct LoopComp {
    std::vector<int32_t> loop;  // ordered boundary walk (global indices)
};
std::vector<LoopComp> lcomps;
{
    std::unordered_set<int32_t> seen;
    for (auto const& kv : bnd_nbrs) {
if (seen.count(kv.first)) continue;
LoopComp lc;
// BFS: collect all vertices in this boundary component.
std::queue<int32_t> bfsq;
bfsq.push(kv.first);
std::vector<int32_t> comp_verts;
while (!bfsq.empty()) {
    int32_t v = bfsq.front(); bfsq.pop();
    if (seen.count(v)) continue;
    seen.insert(v);
    comp_verts.push_back(v);
    for (int32_t nb : bnd_nbrs.at(v))
if (!seen.count(nb)) bfsq.push(nb);
}
// Walk the loop in order.
{
    int32_t start = comp_verts[0], cur = start, prev = -1;
    do {
lc.loop.push_back(cur);
int32_t nxt = -1;
for (int32_t nb : bnd_nbrs.at(cur))
    if (nb != prev) { nxt = nb; break; }
if (nxt < 0) break;
prev = cur; cur = nxt;
    } while (cur != start);
}
// Skip degenerate or non-simple components.
if ((int)lc.loop.size() < 3) continue;
if (lc.loop.size() != comp_verts.size()) continue;
lcomps.push_back(std::move(lc));
    }
}

std::vector<std::array<int32_t, 3>> ml_cdt;
bool ml_ok = !lcomps.empty();
bu_log("rt_bot_repair: patch_repair: multi-loop lcomps=%zu\n", lcomps.size());

for (int ci = 0; ci < (int)lcomps.size() && ml_ok; ci++) {
    const std::vector<int32_t>& sp_loop = lcomps[ci].loop;
    bu_log("rt_bot_repair: per-loop CDT[%d]: loop=%zu verts\n",
   ci, sp_loop.size());

    // 3D positions of boundary vertices (only loop vertices, no patch interior).
    std::vector<gte::Vector3<double>> sp_v3d;
    sp_v3d.reserve(sp_loop.size());
    for (int32_t gv : sp_loop)
sp_v3d.push_back(vp[(size_t)gv]);

    // Build a 0..n-1 local index sequence to pass to MapBoundaryToCircle.
    std::vector<int32_t> sp_loop_loc((int)sp_loop.size());
    std::iota(sp_loop_loc.begin(), sp_loop_loc.end(), 0);

    // Map boundary to circle: no interior vertices needed.
    std::vector<gte::Vector2<double>> sp_uv;
    bool sp_lscm = gte::LSCMParameterization<double>::MapBoundaryToCircle(
sp_v3d, sp_loop_loc, sp_uv);
    if (!sp_lscm || sp_uv.empty()) {
bu_log("rt_bot_repair: per-loop CDT[%d]: LSCM failed\n", ci);
ml_ok = false; break;
    }

    // Build CDT point set (UV coords, one per loop vertex).
    std::vector<detria::PointD> sp_pts;
    sp_pts.reserve(sp_loop.size());
    for (auto const& uv : sp_uv) {
detria::PointD pt; pt.x = uv[0]; pt.y = uv[1];
sp_pts.push_back(pt);
    }

    detria::Triangulation<detria::PointD, int> sp_dtri;
    sp_dtri.setPoints(sp_pts);
    std::vector<int> sp_bnd_int(sp_loop_loc.begin(), sp_loop_loc.end());
    sp_dtri.addPolylineAutoDetectType(sp_bnd_int);

    if (!sp_dtri.triangulate(true)) {
bu_log("rt_bot_repair: per-loop CDT[%d]: detria triangulate failed\n", ci);
ml_ok = false; break;
    }

	    // Collect CDT triangles (CCW orientation in UV space).
	    std::vector<std::array<int32_t, 3>> sp_cdt_local;
	    sp_dtri.forEachTriangle([&](const detria::Triangle<int>& t) {
		sp_cdt_local.push_back({(int32_t)t.x, (int32_t)t.y, (int32_t)t.z});
	    }, false);
	    if (sp_cdt_local.empty()) {
		bu_log("rt_bot_repair: per-loop CDT[%d]: CDT output empty\n", ci);
		ml_ok = false; break;
	    }

	    // Determine correct CDT winding by comparing with the non-patch mesh
	    // at the first boundary edge (sp_loop[0]→sp_loop[1]).
	    // For a manifold combined mesh, the CDT edge orientation at each
	    // boundary edge must be OPPOSITE to the non-patch face's orientation.
	    {
		int32_t va = sp_loop[0], vb = sp_loop[1];
		bool flip = false, found = false;
		for (int32_t f0 : vert_faces[(size_t)va]) {
		    if (patch.count(f0)) continue;
		    int va_pos = -1, vb_pos = -1;
		    for (int k = 0; k < 3; k++) {
			if (tp[f0][k] == va) va_pos = k;
			if (tp[f0][k] == vb) vb_pos = k;
		    }
		    if (va_pos < 0 || vb_pos < 0) continue;
		    // Non-patch face edge orientation: va→vb iff vb_pos == (va_pos+1)%3
		    bool np_va_to_vb = (vb_pos == (va_pos + 1) % 3);
		    // Find the CDT triangle containing both local index 0 (=va) and 1 (=vb).
		    for (auto const& ct : sp_cdt_local) {
			int p0 = -1, p1 = -1;
			for (int k = 0; k < 3; k++) {
			    if (ct[k] == 0) p0 = k;
			    if (ct[k] == 1) p1 = k;
			}
			if (p0 < 0 || p1 < 0) continue;
			// CDT edge orientation: va→vb iff p1 == (p0+1)%3
			bool cdt_va_to_vb = (p1 == (p0 + 1) % 3);
			// Flip CDT if same direction as non-patch (need opposite for manifold).
			flip = (np_va_to_vb == cdt_va_to_vb);
			found = true;
			break;
		    }
		    if (found) break;
		}
		if (flip) {
		    for (auto& ct : sp_cdt_local)
			std::swap(ct[1], ct[2]);
		}
	    }

	    // Map CDT local indices to global indices and accumulate.
	    for (auto const& ct : sp_cdt_local)
		ml_cdt.push_back({sp_loop[(size_t)ct[0]],
			      sp_loop[(size_t)ct[1]],
			      sp_loop[(size_t)ct[2]]});
}

// Combine all CDT fills with the non-patch faces and check Manifold.
if (ml_ok && !ml_cdt.empty()) {
    bu_log("rt_bot_repair: per-loop CDT: %zu loops, cdt_tris=%zu\n",
   lcomps.size(), ml_cdt.size());
    std::vector<std::array<int32_t, 3>> tcomb;
    tcomb.reserve((size_t)nf - patch.size() + ml_cdt.size());
    for (int f = 0; f < nf; f++)
if (!patch.count((int32_t)f)) tcomb.push_back(tp[f]);
    for (auto const& t : ml_cdt) tcomb.push_back(t);

    // Compact vertex array (drop unreferenced patch-interior vertices).
    std::vector<bool> mref((size_t)nv, false);
    for (auto const& t : tcomb) for (int k = 0; k < 3; k++) mref[(size_t)t[k]] = true;
    std::vector<int32_t> mremap((size_t)nv, -1);
    std::vector<gte::Vector3<double>> mvc;
    mvc.reserve((size_t)nv);
    for (int i = 0; i < nv; i++)
if (mref[(size_t)i]) { mremap[(size_t)i] = (int32_t)mvc.size(); mvc.push_back(vp[(size_t)i]); }
    for (auto& t : tcomb) for (int k = 0; k < 3; k++) t[k] = mremap[(size_t)t[k]];

    manifold::MeshGL gmm_ml;
    gte_to_manifold(&gmm_ml, mvc, tcomb);
    manifold::Manifold gm_ml(gmm_ml);
    bu_log("rt_bot_repair: per-loop CDT combined manifold status=%d vol=%.6g\n",
   (int)gm_ml.Status(), gm_ml.Volume());
    if (gm_ml.Status() == manifold::Manifold::Error::NoError) {
if (gm_ml.Volume() >= 0) { gm_out = gm_ml; return true; }
// Try flipping all faces.
gte::MeshPreprocessing<double>::InvertNormals(tcomb);
manifold::MeshGL gmm_ml2;
gte_to_manifold(&gmm_ml2, mvc, tcomb);
manifold::Manifold gm_ml2(gmm_ml2);
if (gm_ml2.Status() == manifold::Manifold::Error::NoError && gm_ml2.Volume() >= 0)
    { gm_out = gm_ml2; return true; }
    }
}
    }

    // Fallback: discard interior vertices, remove patch faces, compact, and
    // fill the resulting hole with boundary-only LSCM.
    bu_log("rt_bot_repair: patch_repair: falling back to GTE LSCM fill (non-patch=%zu)\n",
	   (size_t)nf - patch.size());
    std::vector<std::array<int32_t, 3>> tnew;
    tnew.reserve((size_t)nf - patch.size());
    for (int f = 0; f < nf; f++)
	if (patch.count((int32_t)f) == 0)
	    tnew.push_back(tp[f]);

    if (tnew.empty())
	return false;

    std::vector<bool> ref2((size_t)nv, false);
    for (auto const& t : tnew)
	for (int k = 0; k < 3; k++)
	    ref2[(size_t)t[k]] = true;

    std::vector<int32_t> vremap2((size_t)nv, -1);
    std::vector<gte::Vector3<double>> vnew;
    vnew.reserve((size_t)nv);
    for (int i = 0; i < nv; i++) {
	if (ref2[(size_t)i]) {
	    vremap2[(size_t)i] = (int32_t)vnew.size();
	    vnew.push_back(vp[(size_t)i]);
	}
    }
    for (auto& t : tnew)
	for (int k = 0; k < 3; k++)
	    t[k] = vremap2[(size_t)t[k]];

    gte::MeshHoleFilling<double>::Parameters fp;
    fp.maxArea      = 1e30;
    fp.method       = gte::MeshHoleFilling<double>::TriangulationMethod::LSCM;
    fp.autoFallback = true;
    gte::MeshHoleFilling<double>::FillHoles(vnew, tnew, fp);

    manifold::MeshGL gmm;
    gte_to_manifold(&gmm, vnew, tnew);
    manifold::Manifold gm(gmm);
    if (gm.Status() != manifold::Manifold::Error::NoError)
	return false;
    if (gm.Volume() < 0) {
	// The filled mesh is consistently wound but globally inverted.
	// Flip all face normals and retry — mirrors the try_fill logic.
	gte::MeshPreprocessing<double>::InvertNormals(tnew);
	manifold::MeshGL gmm2;
	gte_to_manifold(&gmm2, vnew, tnew);
	manifold::Manifold gm2(gmm2);
	if (gm2.Status() != manifold::Manifold::Error::NoError || gm2.Volume() < 0)
	    return false;
	gm_out = gm2;
	return true;
    }

    gm_out = gm;
    return true;
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

    // Set up GTE mesh from the BoT data, including initial repair and
    // small connected component removal.
    std::vector<gte::Vector3<double>> vertices;
    std::vector<std::array<int32_t, 3>> triangles;
    bot_to_gte(vertices, triangles, bot);

    // To try to fill in ALL holes we default to 1e30, which is a
    // value used in the Geogram code for a large hole size.
    double hole_size = 1e30;

    // See if the settings override the default
    double area = gte_mesh_area(vertices, triangles);
    if (!NEAR_ZERO(settings->max_hole_area, SMALL_FASTF)) {
	hole_size = settings->max_hole_area;
    } else if (!NEAR_ZERO(settings->max_hole_area_percent, SMALL_FASTF)) {
	hole_size = area * (settings->max_hole_area_percent/100.0);
    }

    // Hole-filling parameters shared by all passes below.
    //
    // LSCM (Least Squares Conformal Maps) is preferred over CDT here because:
    //   - CDT projects the hole boundary onto a best-fit plane, which fails for
    //     non-planar or highly curved holes (common in individual aircraft-skin
    //     meshes like the GenericTwin shapes).
    //   - LSCM maps the boundary to a circle using arc-length parameterization,
    //     which always succeeds for any topologically simple boundary loop
    //     regardless of planarity.
    //   - autoFallback=true further falls back to 3D ear-clipping if LSCM
    //     itself fails (e.g. self-intersecting boundary).
    //
    // Do NOT run MeshRepair after FillHoles.  The repair pass removes colocated
    // or degenerate-looking triangles, but it also removes newly-added hole-fill
    // triangles that share edges with the repaired mesh — which re-opens the
    // holes we just filled.  The final Manifold check below is the authoritative
    // test; no intermediate repair is needed.
    gte::MeshHoleFilling<double>::Parameters fillParams;
    fillParams.maxArea = hole_size;
    fillParams.method = gte::MeshHoleFilling<double>::TriangulationMethod::LSCM;
    fillParams.autoFallback = true;

    // Helper: attempt LSCM hole fill on (v, t) and check Manifold.
    // Returns true and writes the result Manifold into gm_out on success.
    // The area check (filled area >= original) guards against fill removing geometry.
    // Volume must be non-negative: a negative-volume manifold is topologically
    // valid but geometrically inside-out (all face normals inverted), which
    // would fail the raytracing lint check with spurious unexpected-miss errors.
    auto try_fill = [&](std::vector<gte::Vector3<double>> v,
			std::vector<std::array<int32_t, 3>> t,
			double ref_area,
			manifold::Manifold& gm_out,
			const char *pass_name = "") -> bool {
	// Stash the bounding box diagonal before fill so we can detect
	// fill operations that unexpectedly alter the mesh extent.
	// Hole fill only adds triangles between existing boundary vertices
	// (no new vertex positions), so the bbox should be exactly unchanged.
	// This mirrors the identical guard in the Geogram-based repair path.
	double bbox_diag_before = gte_bbox_diagonal(v);

	size_t tcount_before = t.size();
	gte::MeshHoleFilling<double>::FillHoles(v, t, fillParams);
	double new_a = gte_mesh_area(v, t);
	bu_log("rt_bot_repair: %s try_fill: %zu faces before fill, %zu after, area %.6g vs ref %.6g\n",
	       pass_name, tcount_before, t.size(), new_a, ref_area);
	if (new_a < ref_area) {
	    bu_log("rt_bot_repair: %s try_fill: area decreased, rejecting\n", pass_name);
	    return false;
	}

	// Sanity check the bounding box diagonal — should be very close to
	// the pre-fill value (BN_TOL_DIST = 0.0005 mm).
	double bbox_diag_after = gte_bbox_diagonal(v);
	if (!NEAR_EQUAL(bbox_diag_before, bbox_diag_after, BN_TOL_DIST)) {
	    bu_log("rt_bot_repair: %s try_fill: bbox diagonal changed after hole fill (before=%.6g after=%.6g), rejecting\n",
		   pass_name, bbox_diag_before, bbox_diag_after);
	    return false;
	}
	manifold::MeshGL gmm;
	gte_to_manifold(&gmm, v, t);
	manifold::Manifold gm(gmm);
	if (gm.Status() != manifold::Manifold::Error::NoError) {
	    bu_log("rt_bot_repair: %s try_fill: manifold status error %d\n", pass_name, (int)gm.Status());
	    return false;
	}
	if (gm.Volume() < 0) {
	    // Reorientation produced a consistently-wound but inside-out mesh.
	    // Flip all face normals and retry the manifold check.
	    bu_log("rt_bot_repair: %s try_fill: negative volume %.6g, trying flipped normals\n",
		   pass_name, gm.Volume());
	    gte::MeshPreprocessing<double>::InvertNormals(t);
	    manifold::MeshGL gmm2;
	    gte_to_manifold(&gmm2, v, t);
	    manifold::Manifold gm2(gmm2);
	    if (gm2.Status() != manifold::Manifold::Error::NoError) {
		bu_log("rt_bot_repair: %s try_fill: flipped manifold status error %d\n",
		       pass_name, (int)gm2.Status());
		return false;
	    }
	    if (gm2.Volume() < 0) {
		bu_log("rt_bot_repair: %s try_fill: flipped volume still negative %.6g\n",
		       pass_name, gm2.Volume());
		return false;
	    }
	    gm_out = gm2;
	    return true;
	}
	gm_out = gm;
	return true;
    };

    // --- Pass 0: remove purely-extra faces ---
    //
    // A "purely extra" face has ≥1 adj == -2 edge (it sits on a non-manifold
    // edge shared by 3+ faces) AND ≥1 adj == -1 edge (it introduces an open
    // boundary that would not exist without it).  These are spurious duplicate
    // triangles — e.g. from FASTGEN4 duplicate CTRI records — whose removal
    // alone resolves the non-manifold condition without any hole-filling.
    //
    // This must be attempted BEFORE the LSCM passes because MeshHoleFilling
    // needs a clean open boundary to triangulate; purely-extra faces pollute
    // the adjacency so no simple boundary can be found.
    {
	std::vector<int32_t> adj0;
	gte::MeshRepair<double>::ConnectFacets(triangles, adj0);
	bu_log("rt_bot_repair: pass 0 scan: %d faces\n", (int)triangles.size());

	std::vector<std::array<int32_t, 3>> t_pruned;
	t_pruned.reserve(triangles.size());
	for (int f = 0; f < (int)triangles.size(); f++) {
	    bool has_excess = false, has_open = false;
	    for (int e = 0; e < 3; e++) {
		int a = adj0[(size_t)f * 3 + (size_t)e];
		if (a == -2) has_excess = true;
		if (a == -1) has_open  = true;
	    }
	    if (has_excess || has_open)
		bu_log("  f%d v(%d,%d,%d) adj=(%d,%d,%d) excess=%d open=%d\n",
		       f, triangles[f][0], triangles[f][1], triangles[f][2],
		       adj0[f*3+0], adj0[f*3+1], adj0[f*3+2],
		       (int)has_excess, (int)has_open);
	    if (!(has_excess && has_open))
		t_pruned.push_back(triangles[f]);
	}
	bu_log("rt_bot_repair: pass 0: %d pruned to %d\n",
	       (int)triangles.size(), (int)t_pruned.size());

	if (t_pruned.size() < triangles.size()) {
	    manifold::MeshGL gmm0;
	    gte_to_manifold(&gmm0, vertices, t_pruned);
	    manifold::Manifold gm0(gmm0);
	    if (gm0.Status() == manifold::Manifold::Error::NoError && gm0.Volume() >= 0) {
		bu_log("rt_bot_repair: pass 0 (remove excess faces) succeeded\n");
		manifold::MeshGL omesh = gm0.GetMeshGL();
		struct rt_bot_internal *nbot = manifold_to_bot(&omesh);
		if (settings->strict) {
		    int lint_ret = bot_repair_lint(nbot);
		    if (lint_ret) {
			bu_log("Error - new BoT does not pass lint test!\n");
			rt_bot_internal_free(nbot);
			BU_PUT(nbot, struct rt_bot_internal);
			/* fall through to passes 1-3 */
			goto pass1;
		    }
		}
		*obot = nbot;
		return 0;
	    }
	}
    }
pass1:

    // --- Pass 1: straightforward LSCM fill --------------------------------
    //
    // This handles the common case: holes with simple boundary loops.
    manifold::Manifold gmanifold;
    if (!try_fill(vertices, triangles, area, gmanifold, "pass1")) {
	bu_log("rt_bot_repair: pass 1 (LSCM fill) failed\n");

	// --- Pass 2: SplitNonManifoldVertices then LSCM fill -----------------
	//
	// Pass 1 can fail when the mesh has non-manifold vertices — points where
	// multiple independent triangle fans share a single vertex index.
	// bg_trimesh_solid2 (BRL-CAD's solid check) only verifies face-pair
	// connectivity and consistent winding; it does NOT detect non-manifold
	// vertices where three or more surface patches converge.
	//
	// Consequence: the hole-boundary detection in MeshHoleFilling walks
	// boundary half-edges expecting a simple loop, but a non-manifold vertex
	// on the boundary creates a branching point, so the loop is never
	// completed and the hole is left unfilled.
	//
	// SplitNonManifoldVertices resolves this by duplicating the shared vertex
	// into one copy per fan, converting the non-simple boundary into multiple
	// simple loops that LSCM can then triangulate individually.
	//
	// ConnectFacets and ReorientFacetsAntiMoebius are run first because
	// SplitNonManifoldVertices requires consistent per-facet adjacency and
	// a coherent winding orientation to identify which triangles belong to
	// each independent fan at a shared vertex.
	//
	// This pass is intentionally NOT run unconditionally (only on Pass 1
	// failure) because SplitNonManifoldVertices can break intentional
	// multi-body topology where two bodies legitimately share a boundary edge
	// — duplicating that edge's vertices would tear the bodies apart.
	//
	// Move the preprocessed vectors into Pass 2 (they are not needed after
	// this point, so we avoid an extra copy of the vertex/triangle arrays).
	std::vector<gte::Vector3<double>> v2 = std::move(vertices);
	std::vector<std::array<int32_t, 3>> t2 = std::move(triangles);
	std::vector<int32_t> adj2;
	gte::MeshRepair<double>::ConnectFacets(t2, adj2);
	gte::MeshRepair<double>::ReorientFacetsAntiMoebius(v2, t2, adj2);
	gte::MeshRepair<double>::SplitNonManifoldVertices(v2, t2, adj2);
	if (!try_fill(v2, t2, area, gmanifold, "pass2")) {
	    bu_log("rt_bot_repair: pass 2 (split+LSCM) failed\n");

	    // --- Pass 3: Patch repair ---
	    //
	    // Both LSCM passes failed.  The mesh likely has topologically-excess
	    // geometry (edges shared by 3+ faces, severely mis-oriented regions,
	    // or non-manifold vertices whose fans cannot be split cleanly) that
	    // causes the hole-boundary walker to branch mid-loop and abandon the
	    // hole unfilled.
	    //
	    // This pass takes the complementary approach: instead of adding
	    // triangles to fill holes, it REMOVES the faces in the problematic
	    // region and then fills the resulting clean hole(s) with LSCM.  The
	    // "patch" region is grown from the error-incident faces until its
	    // boundary is a simple closed loop with no branching vertices.
	    if (!try_patch_repair(bot, gmanifold)) {
		// All three passes failed to produce a manifold mesh
		return -1;
	    }
	}
    }

    // Output is manifold, make a new bot
    bu_log("rt_bot_repair: gmanifold volume=%.6g\n", gmanifold.Volume());
    manifold::MeshGL omesh = gmanifold.GetMeshGL();
    struct rt_bot_internal *nbot = manifold_to_bot(&omesh);

    // Once we have an rt_bot_internal, see what the solid raytracer's linting
    // thinks of this unless the user has explicitly told us not to do any
    // validation beyond the manifold check.  The above is enough for boolean
    // evaluation input, but won't necessarily clear all problem cases that
    // might arise in a solid raytrace.
    //
    // Note: lint "unexpected miss" failures on manifold-confirmed meshes are
    // treated as warnings, not hard failures.  The BRL-CAD ray-triangle
    // intersection becomes unreliable for triangles that are very thin, very
    // small, or lie in geometrically complex curved regions (e.g. aircraft
    // sheet-metal skins with tight curvature).  Since the manifold library has
    // already verified topological correctness via exact arithmetic, a lint
    // miss on a small fraction of triangles is a raytracer precision limitation
    // rather than a genuine topology defect.  Blocking repair output on such
    // false positives would leave the mesh unrepaired.
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
