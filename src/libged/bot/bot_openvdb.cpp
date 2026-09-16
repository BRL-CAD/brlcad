/*                  B O T _ O P E N V D B . C P P
 * BRL-CAD
 *
 * Copyright (c) 2024-2025 United States Government as represented by
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
/** @file libged/bot/bot_openvdb.cpp
 *
 * Shared OpenVDB utilities for the bot plugin.
 *
 * Three building blocks are implemented here:
 *
 *  bot_to_sdf()                — BoT → OpenVDB signed-distance field
 *  sdf_to_bot()                — OpenVDB SDF → rt_bot_internal
 *  rt_rtip_to_occupancy_grid() — raytrace a prepped rt_i into a BoolGrid
 *
 * See bot_openvdb.h for the public API documentation.
 */

#include "common.h"

#ifdef BRLCAD_OPENVDB

#  include <cmath>
#  include <algorithm>
#  include <limits>

#  include <openvdb/openvdb.h>
#  include <openvdb/tools/MeshToVolume.h>
#  include <openvdb/tools/VolumeToMesh.h>

#  include "manifold/manifold.h"

#  include "vmath.h"
#  include "bu/malloc.h"
#  include "bu/log.h"
#  include "rt/application.h"
#  include "rt/ray_partition.h"
#  include "rt/seg.h"
#  include "rt/geom.h"
#  include "raytrace.h"

#  include "bot_openvdb.h"


/* -----------------------------------------------------------------------
 * botDataAdapter
 *
 * Adapts an rt_bot_internal for use with openvdb::tools::meshToVolume.
 * meshToVolume expects world-space coordinates from getIndexSpacePoint
 * when a transform is supplied (the name "IndexSpace" in the OpenVDB
 * template interface is misleading — values are converted by the caller
 * using the provided transform).
 * ---------------------------------------------------------------------- */
struct botDataAdapter {
    struct rt_bot_internal *bot;

    size_t polygonCount() const { return bot->num_faces; }
    size_t pointCount()   const { return bot->num_vertices; }
    size_t vertexCount(size_t /*polygon*/) const { return 3; }

    void getIndexSpacePoint(size_t n, size_t v, openvdb::Vec3d &pos) const {
	int idx = bot->faces[(n * 3) + v];
	pos[X] = bot->vertices[(idx * 3) + X];
	pos[Y] = bot->vertices[(idx * 3) + Y];
	pos[Z] = bot->vertices[(idx * 3) + Z];
    }

    explicit botDataAdapter(struct rt_bot_internal *bip) : bot(bip) {}
};


/* -----------------------------------------------------------------------
 * bot_to_sdf
 * ---------------------------------------------------------------------- */
openvdb::FloatGrid::Ptr
bot_to_sdf(struct rt_bot_internal *bot, double voxel_size)
{
    if (!bot || !bot->num_vertices || !bot->num_faces)
	return openvdb::FloatGrid::Ptr();

    /* Auto-size: aim for ~100 voxels along the longest bbox dimension. */
    if (voxel_size <= 0.0) {
	double xmin = bot->vertices[0], xmax = bot->vertices[0];
	double ymin = bot->vertices[1], ymax = bot->vertices[1];
	double zmin = bot->vertices[2], zmax = bot->vertices[2];
	for (size_t i = 1; i < bot->num_vertices; i++) {
	    double x = bot->vertices[i * 3 + X];
	    double y = bot->vertices[i * 3 + Y];
	    double z = bot->vertices[i * 3 + Z];
	    if (x < xmin) xmin = x; if (x > xmax) xmax = x;
	    if (y < ymin) ymin = y; if (y > ymax) ymax = y;
	    if (z < zmin) zmin = z; if (z > zmax) zmax = z;
	}
	double dx = xmax - xmin, dy = ymax - ymin, dz = zmax - zmin;
	double diag = std::sqrt(dx*dx + dy*dy + dz*dz);
	voxel_size = (diag > 0.0) ? (diag / 100.0) : 1.0;
    }

    /* Use a narrow band (3 voxels) for both sides.  Deep interior voxels
     * default to the background value (positive = exterior), which is
     * correct for surface extraction via volumeToMesh at isoValue 0. */
    const float exteriorBandWidth = 3.0f;
    const float interiorBandWidth = 3.0f;

    openvdb::initialize();

    struct botDataAdapter bda(bot);
    openvdb::math::Transform::Ptr xform =
	openvdb::math::Transform::createLinearTransform(voxel_size);

    return openvdb::tools::meshToVolume<openvdb::FloatGrid>(
	bda, *xform, exteriorBandWidth, interiorBandWidth);
}


/* -----------------------------------------------------------------------
 * sdf_to_bot
 *
 * Uses the class-based VolumeToMesh (rather than the volumeToMesh free
 * function) so that relaxDisorientedTriangles=true is in effect.  That
 * flag lets the mesher flip locally mis-oriented triangles at concave
 * surface pockets, giving marginally cleaner output at essentially zero
 * extra cost.  The output is otherwise identical to the free function at
 * the same isovalue/adaptivity settings.
 * ---------------------------------------------------------------------- */
struct rt_bot_internal *
sdf_to_bot(openvdb::FloatGrid::Ptr grid, double adaptivity)
{
    if (!grid)
	return NULL;

    if (adaptivity < 0.0) adaptivity = 0.0;
    if (adaptivity > 1.0) adaptivity = 1.0;

    openvdb::tools::VolumeToMesh mesher(/*isovalue=*/0.0, adaptivity,
					/*relaxDisorientedTriangles=*/true);
    mesher(*grid);

    size_t n_points = mesher.pointListSize();
    size_t n_pools  = mesher.polygonPoolListSize();

    if (n_points == 0 || n_pools == 0)
	return NULL;

    const openvdb::tools::PointList       &points = mesher.pointList();
    const openvdb::tools::PolygonPoolList &pools  = mesher.polygonPoolList();

    /* Count total faces across all polygon pools.
     * Each quad is split into two triangles. */
    size_t n_tris = 0;
    for (size_t p = 0; p < n_pools; p++)
	n_tris += pools[p].numTriangles() + pools[p].numQuads() * 2;

    if (n_tris == 0)
	return NULL;

    struct rt_bot_internal *obot;
    BU_GET(obot, struct rt_bot_internal);
    obot->magic        = RT_BOT_INTERNAL_MAGIC;
    obot->mode         = RT_BOT_SOLID;
    obot->orientation  = RT_BOT_CCW;
    obot->thickness    = NULL;
    obot->face_mode    = (struct bu_bitv *)NULL;
    obot->bot_flags    = 0;
    obot->normals      = NULL;
    obot->face_normals = NULL;
    obot->num_normals  = 0;
    obot->num_face_normals = 0;

    obot->num_vertices = n_points;
    obot->vertices = (fastf_t *)bu_malloc(n_points * 3 * sizeof(fastf_t),
					  "sdf_to_bot vertices");
    for (size_t i = 0; i < n_points; i++) {
	obot->vertices[i * 3 + X] = points[i].x();
	obot->vertices[i * 3 + Y] = points[i].y();
	obot->vertices[i * 3 + Z] = points[i].z();
    }

    obot->num_faces = n_tris;
    obot->faces = (int *)bu_malloc(n_tris * 3 * sizeof(int), "sdf_to_bot faces");

    /* Walk each polygon pool, emitting triangles then splitting quads. */
    size_t fidx = 0;
    for (size_t p = 0; p < n_pools; p++) {
	const openvdb::tools::PolygonPool &pool = pools[p];

	for (size_t i = 0; i < pool.numTriangles(); i++) {
	    const openvdb::Vec3I &tri = pool.triangle(i);
	    obot->faces[fidx * 3 + X] = tri.x();
	    obot->faces[fidx * 3 + Y] = tri.y();
	    obot->faces[fidx * 3 + Z] = tri.z();
	    fidx++;
	}

	/* Split each quad into two triangles: (q0,q1,q2) and (q0,q2,q3). */
	for (size_t i = 0; i < pool.numQuads(); i++) {
	    const openvdb::Vec4I &q = pool.quad(i);
	    obot->faces[fidx * 3 + X] = q[0];
	    obot->faces[fidx * 3 + Y] = q[1];
	    obot->faces[fidx * 3 + Z] = q[2];
	    fidx++;
	    obot->faces[fidx * 3 + X] = q[0];
	    obot->faces[fidx * 3 + Y] = q[2];
	    obot->faces[fidx * 3 + Z] = q[3];
	    fidx++;
	}
    }

    return obot;
}


/* -----------------------------------------------------------------------
 * bot_openvdb_repair
 *
 * Attempt to repair a non-manifold solid BoT using the OpenVDB level-set
 * pipeline.  Converts the mesh to a signed-distance field and extracts a
 * guaranteed-manifold mesh from the 0-isovalue surface.
 *
 * Validates the result: checks that it is manifold and has positive volume
 * (negative volume would indicate an inside-out reconstruction).
 *
 * Returns a new rt_bot_internal on success, NULL on failure.
 * Caller is responsible for freeing the returned bot.
 * ---------------------------------------------------------------------- */
struct rt_bot_internal *
bot_openvdb_repair(struct rt_bot_internal *bot, double voxel_size)
{
    openvdb::FloatGrid::Ptr grid = bot_to_sdf(bot, voxel_size);
    if (!grid) {
	bu_log("bot_openvdb_repair: meshToVolume failed\n");
	return NULL;
    }

    /* Full voxel resolution (adaptivity=0) preserves shape best for repair. */
    struct rt_bot_internal *cand = sdf_to_bot(grid, 0.0);
    if (!cand || cand->num_faces == 0) {
	bu_log("bot_openvdb_repair: volumeToMesh produced no faces\n");
	if (cand) { bu_free(cand->vertices, "verts"); bu_free(cand->faces, "faces"); BU_PUT(cand, struct rt_bot_internal); }
	return NULL;
    }

    /* Validate: the result must be manifold. */
    manifold::MeshGL64 vcheck;
    vcheck.numProp = 3;
    vcheck.vertProperties.resize(cand->num_vertices * 3);
    for (size_t i = 0; i < cand->num_vertices; i++) {
	vcheck.vertProperties[i * 3 + 0] = cand->vertices[i * 3 + X];
	vcheck.vertProperties[i * 3 + 1] = cand->vertices[i * 3 + Y];
	vcheck.vertProperties[i * 3 + 2] = cand->vertices[i * 3 + Z];
    }
    vcheck.triVerts.resize(cand->num_faces * 3);
    for (size_t i = 0; i < cand->num_faces; i++) {
	vcheck.triVerts[i * 3 + 0] = cand->faces[i * 3 + X];
	vcheck.triVerts[i * 3 + 1] = cand->faces[i * 3 + Y];
	vcheck.triVerts[i * 3 + 2] = cand->faces[i * 3 + Z];
    }
    manifold::Manifold mcheck(vcheck);
    if (mcheck.Status() != manifold::Manifold::Error::NoError) {
	bu_log("bot_openvdb_repair: result is not manifold (unexpected)\n");
	bu_free(cand->vertices, "verts"); bu_free(cand->faces, "faces");
	BU_PUT(cand, struct rt_bot_internal);
	return NULL;
    }

    /* Validate: positive volume means outward-facing normals (correct).
     * If the volume is negative the mesh is topologically manifold but
     * inside-out (all face normals point inward).  Swapping vertices 1
     * and 2 of every triangle reverses the winding order globally and
     * turns it into a valid outward-facing solid without touching the
     * vertex data or rebuilding the mesh. */
    if (mcheck.Volume() <= 0.0) {
	bu_log("bot_openvdb_repair: result is inside-out; flipping winding order\n");
	for (size_t i = 0; i < cand->num_faces; i++) {
	    int tmp = cand->faces[i * 3 + 1];
	    cand->faces[i * 3 + 1] = cand->faces[i * 3 + 2];
	    cand->faces[i * 3 + 2] = tmp;
	}
    }

    return cand;
}


/* -----------------------------------------------------------------------
 * rt_rtip_to_occupancy_grid internals
 *
 * These hit/miss callbacks and the per-ray context are used only by
 * rt_rtip_to_occupancy_grid below.
 * ---------------------------------------------------------------------- */

struct OccRayCtx {
    openvdb::BoolGrid::Accessor *acc;
    double ray_origin_axis;
    double mdl_min_axis;
    double voxel_size;
    int idx_a_padded;
    int idx_b_padded;
    int n_axis;
    int axis; /* 0=+X, 1=+Y, 2=+Z */
};


static int
occ_miss(struct application *UNUSED(ap))
{
    return 0;
}


static int
occ_hit(struct application *ap, struct partition *PartHeadp, struct seg *UNUSED(seg))
{
    struct OccRayCtx *ctx = (struct OccRayCtx *)ap->a_uptr;
    openvdb::BoolGrid::Accessor &acc = *ctx->acc;

    for (struct partition *pp = PartHeadp->pt_forw;
	 pp != PartHeadp;
	 pp = pp->pt_forw) {
	double in_world  = ctx->ray_origin_axis + pp->pt_inhit->hit_dist;
	double out_world = ctx->ray_origin_axis + pp->pt_outhit->hit_dist;

	/* Use floor() — C cast truncates toward zero, not -inf. */
	int v_in  = (int)floor((in_world  - ctx->mdl_min_axis) / ctx->voxel_size) + 1;
	int v_out = (int)floor((out_world - ctx->mdl_min_axis) / ctx->voxel_size) + 1;

	if (v_in  < 1)                v_in  = 1;
	if (v_out >= ctx->n_axis - 1) v_out = ctx->n_axis - 2;
	if (v_in  > v_out)            continue;

	for (int v = v_in; v <= v_out; v++) {
	    openvdb::Coord c;
	    switch (ctx->axis) {
		case 0:  c = openvdb::Coord(v, ctx->idx_a_padded, ctx->idx_b_padded); break;
		case 1:  c = openvdb::Coord(ctx->idx_b_padded, v, ctx->idx_a_padded); break;
		default: c = openvdb::Coord(ctx->idx_a_padded, ctx->idx_b_padded, v); break;
	    }
	    acc.setValue(c, true);
	}
    }
    return 1;
}


/* -----------------------------------------------------------------------
 * rt_rtip_to_occupancy_grid
 * ---------------------------------------------------------------------- */
openvdb::BoolGrid::Ptr
rt_rtip_to_occupancy_grid(struct rt_i *rtip, double voxel_size,
			  int *nx, int *ny, int *nz)
{
    int nx_m = (int)std::ceil((rtip->mdl_max[X] - rtip->mdl_min[X]) / voxel_size);
    int ny_m = (int)std::ceil((rtip->mdl_max[Y] - rtip->mdl_min[Y]) / voxel_size);
    int nz_m = (int)std::ceil((rtip->mdl_max[Z] - rtip->mdl_min[Z]) / voxel_size);

    if (nx_m < 1) nx_m = 1;
    if (ny_m < 1) ny_m = 1;
    if (nz_m < 1) nz_m = 1;

    *nx = nx_m + 2;
    *ny = ny_m + 2;
    *nz = nz_m + 2;

    openvdb::BoolGrid::Ptr solid = openvdb::BoolGrid::create(false);
    openvdb::BoolGrid::Accessor acc = solid->getAccessor();

    struct OccRayCtx ctx;
    ctx.acc        = &acc;
    ctx.voxel_size = voxel_size;

    struct application ap;
    RT_APPLICATION_INIT(&ap);
    ap.a_rt_i   = rtip;
    ap.a_hit    = occ_hit;
    ap.a_miss   = occ_miss;
    ap.a_onehit = 0;
    ap.a_uptr   = &ctx;

    /* Pass 1: +X rays (one per Y-Z column). */
    ctx.axis            = 0;
    ctx.mdl_min_axis    = rtip->mdl_min[X];
    ctx.n_axis          = *nx;
    ctx.ray_origin_axis = rtip->mdl_min[X] - voxel_size;

    for (int k = 0; k < nz_m; k++) {
	ctx.idx_b_padded = k + 1;
	for (int j = 0; j < ny_m; j++) {
	    ctx.idx_a_padded = j + 1;
	    VSET(ap.a_ray.r_pt,
		 ctx.ray_origin_axis,
		 rtip->mdl_min[Y] + (j + 0.5) * voxel_size,
		 rtip->mdl_min[Z] + (k + 0.5) * voxel_size);
	    VSET(ap.a_ray.r_dir, 1.0, 0.0, 0.0);
	    rt_shootray(&ap);
	}
    }

    /* Pass 2: +Y rays (one per X-Z column). */
    ctx.axis            = 1;
    ctx.mdl_min_axis    = rtip->mdl_min[Y];
    ctx.n_axis          = *ny;
    ctx.ray_origin_axis = rtip->mdl_min[Y] - voxel_size;

    for (int k = 0; k < nz_m; k++) {
	ctx.idx_a_padded = k + 1;
	for (int i = 0; i < nx_m; i++) {
	    ctx.idx_b_padded = i + 1;
	    VSET(ap.a_ray.r_pt,
		 rtip->mdl_min[X] + (i + 0.5) * voxel_size,
		 ctx.ray_origin_axis,
		 rtip->mdl_min[Z] + (k + 0.5) * voxel_size);
	    VSET(ap.a_ray.r_dir, 0.0, 1.0, 0.0);
	    rt_shootray(&ap);
	}
    }

    /* Pass 3: +Z rays (one per X-Y column). */
    ctx.axis            = 2;
    ctx.mdl_min_axis    = rtip->mdl_min[Z];
    ctx.n_axis          = *nz;
    ctx.ray_origin_axis = rtip->mdl_min[Z] - voxel_size;

    for (int j = 0; j < ny_m; j++) {
	ctx.idx_b_padded = j + 1;
	for (int i = 0; i < nx_m; i++) {
	    ctx.idx_a_padded = i + 1;
	    VSET(ap.a_ray.r_pt,
		 rtip->mdl_min[X] + (i + 0.5) * voxel_size,
		 rtip->mdl_min[Y] + (j + 0.5) * voxel_size,
		 ctx.ray_origin_axis);
	    VSET(ap.a_ray.r_dir, 0.0, 0.0, 1.0);
	    rt_shootray(&ap);
	}
    }

    return solid;
}

#endif /* BRLCAD_OPENVDB */


/*
 * Local Variables:
 * mode: C++
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
