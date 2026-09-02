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
#include <array>
#include <climits>
#include <unordered_map>
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
    if (!n || !bot || !bot->faces || !bot->vertices || i < 0 ||
	(size_t)i >= bot->num_faces)
	return false;
    for (size_t corner = 0; corner < 3; ++corner) {
	int vertex = bot->faces[(size_t)i * 3 + corner];
	if (vertex < 0 || (size_t)vertex >= bot->num_vertices)
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
    struct rt_i *rtip = rt_i_create(dbip);
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

    rt_i_destroy(rtip);
    bu_free(state, "state");
    bu_free(resp, "resp");
    db_close(dbip);

    return ret;
}

// Helper: convert topology-changing manifold output to a plain solid BOT.
static struct rt_bot_internal *
manifold_to_bot(const manifold::MeshGL64 &mesh)
{
    if (mesh.vertProperties.size() % 3 || mesh.triVerts.size() % 3 ||
	mesh.NumVert() > INT_MAX || mesh.NumTri() > INT_MAX)
	return NULL;

    struct rt_bot_internal *nbot;
    BU_ALLOC(nbot, struct rt_bot_internal);
    nbot->magic = RT_BOT_INTERNAL_MAGIC;
    nbot->mode = RT_BOT_SOLID;
    nbot->orientation = RT_BOT_CCW;
    nbot->num_vertices = mesh.NumVert();
    nbot->num_faces = mesh.NumTri();
    nbot->vertices = (fastf_t *)bu_calloc(mesh.vertProperties.size(),
	sizeof(fastf_t), "repaired BOT vertices");
    nbot->faces = (int *)bu_calloc(mesh.triVerts.size(), sizeof(int),
	"repaired BOT faces");
    std::copy(mesh.vertProperties.begin(), mesh.vertProperties.end(),
	nbot->vertices);
    std::copy(mesh.triVerts.begin(), mesh.triVerts.end(), nbot->faces);

    return nbot;
}


// A merge-only Manifold repair retains a source face ID for every triangle.
// Reorder each output triangle back to its source corner order so indexed
// normals and UVs remain attached to the correct corners as well as faces.
static struct rt_bot_internal *
manifold_to_preserved_bot(const manifold::MeshGL64 &mesh,
	const struct rt_bot_internal *source)
{
    if (!source || mesh.vertProperties.size() % 3 ||
	mesh.triVerts.size() % 3 || mesh.NumVert() > INT_MAX ||
	mesh.NumTri() > INT_MAX || mesh.faceID.size() != mesh.NumTri())
	return NULL;

    std::vector<fastf_t> vertices(mesh.vertProperties.begin(),
	mesh.vertProperties.end());
    std::vector<int> faces(mesh.triVerts.size());
    std::vector<int> face_sources(mesh.NumTri());
    constexpr std::array<std::array<size_t, 3>, 6> corner_permutations = {{
	{{0, 1, 2}}, {{0, 2, 1}}, {{1, 0, 2}},
	{{1, 2, 0}}, {{2, 0, 1}}, {{2, 1, 0}}
    }};

    for (size_t face = 0; face < mesh.NumTri(); ++face) {
	size_t source_face = mesh.faceID[face];
	if (source_face >= source->num_faces)
	    return NULL;
	face_sources[face] = (int)source_face;

	double best_distance = INFINITY;
	const std::array<size_t, 3> *best_permutation = NULL;
	for (const auto &permutation : corner_permutations) {
	    double distance = 0.0;
	    for (size_t source_corner = 0; source_corner < 3; ++source_corner) {
		size_t output_corner = permutation[source_corner];
		size_t output_vertex = mesh.triVerts[face * 3 + output_corner];
		if (output_vertex >= mesh.NumVert())
		    return NULL;
		int source_vertex = source->faces[source_face * 3 + source_corner];
		distance += DIST_PNT_PNT_SQ(&vertices[output_vertex * 3],
		    &source->vertices[(size_t)source_vertex * 3]);
	    }
	    if (distance < best_distance) {
		best_distance = distance;
		best_permutation = &permutation;
	    }
	}
	if (!best_permutation)
	    return NULL;
	for (size_t source_corner = 0; source_corner < 3; ++source_corner) {
	    size_t output_corner = (*best_permutation)[source_corner];
	    faces[face * 3 + source_corner] =
		(int)mesh.triVerts[face * 3 + output_corner];
	}
    }

    struct rt_bot_internal geometry = *source;
    geometry.num_vertices = mesh.NumVert();
    geometry.vertices = vertices.data();
    return rt_bot_gc(&geometry, faces.data(), face_sources.data(),
	mesh.NumTri());
}

int
rt_bot_repair(struct rt_bot_internal **obot, struct rt_bot_internal *bot, struct rt_bot_repair_info *rinfo)
{
    if (!bot || !obot || !rinfo)
	return -1;

    // Unless we produce something, obot will be NULL
    *obot = NULL;

    rinfo->output_nonmanifold = 0;
    rinfo->output_lint_fail = 0;
    rinfo->output_volume = 0.0;
    rinfo->output_data_loss = 0;
    if (bot->mode != RT_BOT_SOLID || bot->num_vertices > INT_MAX ||
	bot->num_faces > INT_MAX || !bot->vertices || !bot->faces)
	return -1;
    for (size_t face_corner = 0; face_corner < bot->num_faces * 3;
	++face_corner) {
	int vertex = bot->faces[face_corner];
	if (vertex < 0 || (size_t)vertex >= bot->num_vertices)
	    return -1;
    }

    unsigned int data_loss = 0;
    if ((bot->bot_flags & RT_BOT_HAS_SURFACE_NORMALS) || bot->normals ||
	bot->num_normals || bot->face_normals || bot->num_face_normals)
	data_loss |= RT_BOT_REPAIR_LOST_NORMALS;
    if ((bot->bot_flags & RT_BOT_HAS_TEXTURE_UVS) || bot->uvs ||
	bot->num_uvs || bot->face_uvs || bot->num_face_uvs)
	data_loss |= RT_BOT_REPAIR_LOST_UVS;

    int num_vertices = (int)bot->num_vertices;
    int num_faces = (int)bot->num_faces;

    // Fast path: check if already manifold via Manifold.Merge() and bg_trimesh_solid2.
    manifold::MeshGL64 bot_mesh;
    for (size_t j = 0; j < bot->num_vertices; j++) {
	bot_mesh.vertProperties.push_back(bot->vertices[3*j+0]);
	bot_mesh.vertProperties.push_back(bot->vertices[3*j+1]);
	bot_mesh.vertProperties.push_back(bot->vertices[3*j+2]);
    }
    for (size_t j = 0; j < bot->num_faces; ++j) {
	bot_mesh.faceID.push_back((uint32_t)j);
	if (bot->orientation == RT_BOT_CW) {
	    bot_mesh.triVerts.push_back(bot->faces[3*j]);
	    bot_mesh.triVerts.push_back(bot->faces[3*j+2]);
	    bot_mesh.triVerts.push_back(bot->faces[3*j+1]);
	} else {
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
	manifold::MeshGL64 omesh = omanifold.GetMeshGL64();
	struct rt_bot_internal *nbot = manifold_to_preserved_bot(omesh, bot);
	bool data_preserved = nbot != NULL;
	if (!nbot)
	    nbot = manifold_to_bot(omesh);
	if (!nbot)
	    return -1;
	*obot = nbot;
	rinfo->output_data_loss = data_preserved ? 0 : data_loss;
	return 0;
    }

    // Call bg_trimesh_repair for the GTE-based repair.
    struct bg_trimesh_repair_opts opts;
    opts.max_hole_area         = rinfo->max_hole_area;
    opts.max_hole_area_percent = rinfo->max_hole_area_percent;

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
    manifold::MeshGL64 gmm;
    for (int i = 0; i < n_rpnts; i++) {
	gmm.vertProperties.push_back(rpnts[i][X]);
	gmm.vertProperties.push_back(rpnts[i][Y]);
	gmm.vertProperties.push_back(rpnts[i][Z]);
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
	rinfo->output_nonmanifold = 1;
	return -1;
    }
    rinfo->output_volume = gmanifold.Volume();
    if (rinfo->output_volume < 0)
	return -1;

    manifold::MeshGL64 omesh = gmanifold.GetMeshGL64();
    struct rt_bot_internal *nbot = manifold_to_bot(omesh);
    if (!nbot)
	return -1;
    if (rinfo->strict)
	rinfo->output_lint_fail = bot_repair_lint(nbot);

    *obot = nbot;
    rinfo->output_data_loss = data_loss;
    return 0;
}


static bool
copy_indexed_face_data(fastf_t **output_values, size_t *output_value_count,
	int **output_face_values, size_t *output_face_count,
	const fastf_t *input_values, size_t input_value_count,
	const int *input_face_values, size_t input_face_count,
	const int *selected_faces, size_t selected_face_count)
{
    *output_values = NULL;
    *output_value_count = 0;
    *output_face_values = NULL;
    *output_face_count = 0;
    if (!selected_face_count)
	return true;
    if (!input_values || !input_face_values || !input_value_count ||
	input_value_count > INT_MAX)
	return false;

    int *face_values = (int *)bu_calloc(selected_face_count, 3 * sizeof(int),
	"BOT subset face-indexed data");
    std::unordered_map<int, int> old_to_new;
    old_to_new.reserve(selected_face_count * 3);
    std::vector<int> active_values;
    active_values.reserve(selected_face_count * 3);

    for (size_t output_face = 0; output_face < selected_face_count; ++output_face) {
	int input_face = selected_faces ? selected_faces[output_face] :
	    (int)output_face;
	if (input_face < 0 || (size_t)input_face >= input_face_count) {
	    bu_free(face_values, "BOT subset face-indexed data");
	    return false;
	}

	for (size_t corner = 0; corner < 3; ++corner) {
	    int old_index = input_face_values[(size_t)input_face * 3 + corner];
	    if (old_index < 0 || (size_t)old_index >= input_value_count) {
		bu_free(face_values, "BOT subset face-indexed data");
		return false;
	    }

	    auto insertion = old_to_new.emplace(old_index, (int)active_values.size());
	    if (insertion.second)
		active_values.push_back(old_index);
	    face_values[output_face * 3 + corner] = insertion.first->second;
	}
    }

    fastf_t *values = (fastf_t *)bu_calloc(active_values.size(),
	3 * sizeof(fastf_t), "BOT subset indexed data");
    for (size_t output_index = 0; output_index < active_values.size(); ++output_index) {
	size_t input_index = (size_t)active_values[output_index];
	VMOVE(&values[output_index * 3], &input_values[input_index * 3]);
    }

    *output_values = values;
    *output_value_count = active_values.size();
    *output_face_values = face_values;
    *output_face_count = selected_face_count;
    return true;
}


static struct rt_bot_internal *
bot_from_faces(const struct rt_bot_internal *original,
	const int *replacement_faces, const int *source_faces, size_t face_count)
{
    if (!original || original->magic != RT_BOT_INTERNAL_MAGIC ||
	original->num_faces > INT_MAX ||
	original->num_vertices > INT_MAX || face_count > INT_MAX ||
	(face_count && !source_faces))
	return NULL;
    for (size_t face = 0; face < face_count; ++face) {
	if (source_faces[face] < 0 ||
		(size_t)source_faces[face] >= original->num_faces)
	    return NULL;
    }

    struct rt_bot_internal *result;
    BU_ALLOC(result, struct rt_bot_internal);
    result->magic = RT_BOT_INTERNAL_MAGIC;
    result->mode = original->mode;
    result->orientation = original->orientation;
    result->bot_flags = original->bot_flags;
    bool has_normal_data = false;
    bool has_uv_data = false;

    const int *geometry_faces = replacement_faces ? replacement_faces :
	original->faces;
    size_t geometry_face_count = replacement_faces ? face_count :
	original->num_faces;
    const int *geometry_face_selection = replacement_faces ? NULL :
	source_faces;
    if (!copy_indexed_face_data(&result->vertices, &result->num_vertices,
	    &result->faces, &result->num_faces,
	    original->vertices, original->num_vertices,
	    geometry_faces, geometry_face_count,
	    geometry_face_selection, face_count))
	goto fail;

    if (original->mode == RT_BOT_PLATE || original->mode == RT_BOT_PLATE_NOCOS) {
	if (face_count && (!original->thickness ||
		(original->face_mode &&
		 bu_bitv_length(original->face_mode) < original->num_faces)))
	    goto fail;
	if (face_count) {
	    result->thickness = (fastf_t *)bu_calloc(face_count,
		sizeof(fastf_t), "BOT subset thickness");
	    result->face_mode = bu_bitv_new(face_count);
	}
	for (size_t output_face = 0; output_face < face_count; ++output_face) {
	    int input_face = source_faces[output_face];
	    if (input_face < 0 || (size_t)input_face >= original->num_faces)
		goto fail;
	    result->thickness[output_face] = original->thickness[input_face];
	    if (original->face_mode && BU_BITTEST(original->face_mode, input_face))
		BU_BITSET(result->face_mode, output_face);
	}
    }

    has_normal_data = original->normals || original->num_normals ||
	original->face_normals || original->num_face_normals;
    if (face_count &&
	(original->bot_flags & RT_BOT_HAS_SURFACE_NORMALS) && !has_normal_data)
	goto fail;
    if (has_normal_data &&
	!copy_indexed_face_data(&result->normals, &result->num_normals,
	    &result->face_normals, &result->num_face_normals,
	    original->normals, original->num_normals,
	    original->face_normals, original->num_face_normals,
	    source_faces, face_count))
	goto fail;

    has_uv_data = original->uvs || original->num_uvs ||
	original->face_uvs || original->num_face_uvs;
    if (face_count &&
	(original->bot_flags & RT_BOT_HAS_TEXTURE_UVS) && !has_uv_data)
	goto fail;
    if (has_uv_data &&
	!copy_indexed_face_data(&result->uvs, &result->num_uvs,
	    &result->face_uvs, &result->num_face_uvs,
	    original->uvs, original->num_uvs,
	    original->face_uvs, original->num_face_uvs,
	    source_faces, face_count))
	goto fail;

    return result;

fail:
    rt_bot_internal_free(result);
    BU_PUT(result, struct rt_bot_internal);
    return NULL;
}


struct rt_bot_internal *
rt_bot_gc(const struct rt_bot_internal *original, const int *faces,
	const int *face_sources, size_t face_count)
{
    if (face_count && !faces)
	return NULL;
    return bot_from_faces(original, faces, face_sources, face_count);
}


struct rt_bot_internal *
rt_bot_subset(const struct rt_bot_internal *original, const int *face_indices,
	size_t face_count)
{
    return bot_from_faces(original, NULL, face_indices, face_count);
}


struct rt_bot_list *
rt_bot_split(struct rt_bot_internal *bot)
{
    RT_BOT_CK_MAGIC(bot);

    if (bot->num_faces > INT_MAX)
	return NULL;

    struct rt_bot_list *result;
    BU_ALLOC(result, struct rt_bot_list);
    BU_LIST_INIT(&result->l);
    if (bot->num_faces < 2)
	return result;

    int *face_indices = NULL;
    int *component_offsets = NULL;
    int component_count = bg_trimesh_separate(&face_indices,
	&component_offsets, bot->faces, (int)bot->num_faces);
    if (component_count < 0) {
	bu_free(result, "rt_bot_split result");
	return NULL;
    }
    if (component_count < 2) {
	bu_free(face_indices, "trimesh component face indices");
	bu_free(component_offsets, "trimesh component offsets");
	return result;
    }

    for (int component = 0; component < component_count; ++component) {
	int offset = component_offsets[component];
	size_t face_count = (size_t)(component_offsets[component + 1] - offset);
	struct rt_bot_internal *component_bot = rt_bot_subset(bot,
	    &face_indices[offset], face_count);
	if (!component_bot) {
	    bu_free(face_indices, "trimesh component face indices");
	    bu_free(component_offsets, "trimesh component offsets");
	    rt_bot_list_free(result, 1);
	    return NULL;
	}

	struct rt_bot_list *entry;
	BU_ALLOC(entry, struct rt_bot_list);
	entry->bot = component_bot;
	BU_LIST_INSERT(&result->l, &entry->l);
    }

    bu_free(face_indices, "trimesh component face indices");
    bu_free(component_offsets, "trimesh component offsets");
    return result;
}


struct rt_bot_internal *
rt_bot_remove_faces(struct bu_ptbl *rm_face_indices, const struct rt_bot_internal *orig_bot)
{
    if (!rm_face_indices || !BU_PTBL_LEN(rm_face_indices) || !orig_bot ||
	orig_bot->num_faces > INT_MAX)
	return NULL;


    std::unordered_set<size_t> rm_indices;
    for (size_t i = 0; i < BU_PTBL_LEN(rm_face_indices); i++) {
	size_t ind = (size_t)(uintptr_t)BU_PTBL_GET(rm_face_indices, i);
	rm_indices.insert(ind);
    }
    std::vector<int> selected_faces;
    selected_faces.reserve(orig_bot->num_faces);
    for (size_t i = 0; i < orig_bot->num_faces; i++) {
	if (rm_indices.find(i) != rm_indices.end())
	    continue;
	selected_faces.push_back((int)i);
    }

    // Preserve the established behavior of returning an empty BOT when fewer
    // than three faces survive the removal.
    if (selected_faces.size() < 3)
	selected_faces.clear();
    return rt_bot_subset(orig_bot, selected_faces.data(), selected_faces.size());
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
	bot->face_mode = bu_bitv_dup(obot->face_mode);
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
