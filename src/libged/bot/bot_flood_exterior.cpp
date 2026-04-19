/*              B O T _ F L O O D _ E X T E R I O R . C P P
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
/** @file libged/bot/bot_flood_exterior.cpp
 *
 * Tier-2 exterior face classification: "would get wet if submerged."
 *
 * A face is exterior when there exists a path of empty (non-solid)
 * voxels connecting the face to the unbounded exterior space. This
 * correctly handles narrow channels, small holes, and nested shells
 * that the tier-1 ray-sampling approach may miss.
 *
 * Algorithm
 * ---------
 *  1. Build an occupancy grid (solid / air) via rt_rtip_to_occupancy_grid
 *     (implemented in bot_openvdb.cpp).  Rays are shot in +X, +Y, +Z.
 *     A voxel is marked solid when any hit segment encloses its centre.
 *
 *  2. BFS flood-fill starting from the corner of the padded grid boundary,
 *     expanding through air voxels that have not been visited yet.
 *
 *  3. A face is exterior if any of its six face-adjacent voxels was reached
 *     by the flood fill.
 */

#include "common.h"

#include <cmath>
#include <algorithm>
#include <queue>

#include "bot_openvdb.h"

#include "vmath.h"
#include "rt/geom.h"
#include "raytrace.h"
#include "bu/log.h"


/* -----------------------------------------------------------------------
 * BFS flood fill from a corner of the padded region.
 *
 * Returns a BoolGrid whose active voxels are all empty-space cells
 * reachable from the exterior without crossing a solid voxel.
 * On return, *n_exterior is set to the number of exterior voxels found.
 * ---------------------------------------------------------------------- */
static openvdb::BoolGrid::Ptr
flood_fill_exterior(openvdb::BoolGrid::Ptr solid, int nx, int ny, int nz,
		    long long *n_exterior)
{
    openvdb::BoolGrid::Ptr exterior = openvdb::BoolGrid::create(false);
    openvdb::BoolGrid::Accessor        ext_acc   = exterior->getAccessor();
    openvdb::BoolGrid::ConstAccessor   solid_acc = solid->getConstAccessor();

    /* Seed: (0, 0, 0) is always in the padding layer, outside the model. */
    std::queue<openvdb::Coord> q;
    openvdb::Coord seed(0, 0, 0);
    ext_acc.setValue(seed, true);
    q.push(seed);
    long long count = 1;

    /* 6-connected face neighbors. */
    static const int D[6][3] = {
	{ 1,0,0}, {-1,0,0},
	{ 0,1,0}, { 0,-1,0},
	{ 0,0,1}, { 0,0,-1}
    };

    while (!q.empty()) {
	openvdb::Coord c = q.front();
	q.pop();

	for (int d = 0; d < 6; d++) {
	    openvdb::Coord nbr(c[0]+D[d][0], c[1]+D[d][1], c[2]+D[d][2]);

	    /* Bounds check against grid extents. */
	    if (nbr[0] < 0 || nbr[0] >= nx ||
		nbr[1] < 0 || nbr[1] >= ny ||
		nbr[2] < 0 || nbr[2] >= nz)
		continue;

	    /* Stop at solid walls. */
	    if (solid_acc.getValue(nbr))
		continue;

	    /* Skip already-visited exterior voxels. */
	    if (ext_acc.getValue(nbr))
		continue;

	    ext_acc.setValue(nbr, true);
	    q.push(nbr);
	    count++;
	}
    }

    *n_exterior = count;
    return exterior;
}


/* -----------------------------------------------------------------------
 * C-callable entry point
 * ---------------------------------------------------------------------- */

extern "C" int
bot_flood_exterior_classify(struct rt_i *rtip,
			    struct rt_bot_internal *bot,
			    int *face_exterior,
			    double voxel_size)
{
    if (!rtip || !bot || !face_exterior)
	return -1;

    /* Auto-size: aim for ~100 voxels along the shortest dimension. */
    if (voxel_size <= 0.0) {
	double dx = rtip->mdl_max[X] - rtip->mdl_min[X];
	double dy = rtip->mdl_max[Y] - rtip->mdl_min[Y];
	double dz = rtip->mdl_max[Z] - rtip->mdl_min[Z];
	double min_dim = std::min({dx, dy, dz});
	voxel_size = (min_dim > 0.0) ? (min_dim / 100.0) : 1.0;
    }

    int nx = 0, ny = 0, nz = 0;

    /* Must call openvdb::initialize() before using any OpenVDB type. */
    openvdb::initialize();

    /* Step 1: voxelise the model into a solid-occupancy grid. */
    bu_log("bot flood exterior: voxel_size=%.4g\n", voxel_size);
    openvdb::BoolGrid::Ptr solid = rt_rtip_to_occupancy_grid(rtip, voxel_size, &nx, &ny, &nz);
    bu_log("bot flood exterior: grid %dx%dx%d\n", nx, ny, nz);

    /* Step 2: BFS flood fill to find exterior (water-reachable) voxels. */
    long long n_ext_vox = 0;
    openvdb::BoolGrid::Ptr exterior = flood_fill_exterior(solid, nx, ny, nz, &n_ext_vox);
    bu_log("bot flood exterior: %lld exterior voxels\n", n_ext_vox);

    /* Step 3: For each face, mark it exterior if any face-adjacent voxel
     * (in the 6-connected sense) is in the exterior set. */
    openvdb::BoolGrid::ConstAccessor ext_acc = exterior->getConstAccessor();

    static const int D[6][3] = {
	{ 1,0,0}, {-1,0,0},
	{ 0,1,0}, { 0,-1,0},
	{ 0,0,1}, { 0,0,-1}
    };

    int num_exterior = 0;
    for (size_t fi = 0; fi < bot->num_faces; fi++) {
	int vi0 = bot->faces[fi*3+0];
	int vi1 = bot->faces[fi*3+1];
	int vi2 = bot->faces[fi*3+2];

	/* Face centroid. */
	double cx = (bot->vertices[vi0*3+X] + bot->vertices[vi1*3+X] + bot->vertices[vi2*3+X]) / 3.0;
	double cy = (bot->vertices[vi0*3+Y] + bot->vertices[vi1*3+Y] + bot->vertices[vi2*3+Y]) / 3.0;
	double cz = (bot->vertices[vi0*3+Z] + bot->vertices[vi1*3+Z] + bot->vertices[vi2*3+Z]) / 3.0;

	/* Padded voxel index of centroid (origin offset by 1 for padding).
	 * Use floor() so that negative relative coordinates map correctly
	 * (C's cast-to-int truncates toward zero, not toward -inf). */
	int ix = (int)floor((cx - rtip->mdl_min[X]) / voxel_size) + 1;
	int iy = (int)floor((cy - rtip->mdl_min[Y]) / voxel_size) + 1;
	int iz = (int)floor((cz - rtip->mdl_min[Z]) / voxel_size) + 1;

	/* Check all six face-adjacent voxels. */
	face_exterior[fi] = 0;
	for (int d = 0; d < 6 && !face_exterior[fi]; d++) {
	    openvdb::Coord nbr(ix+D[d][0], iy+D[d][1], iz+D[d][2]);
	    if (nbr[0] >= 0 && nbr[0] < nx &&
		nbr[1] >= 0 && nbr[1] < ny &&
		nbr[2] >= 0 && nbr[2] < nz &&
		ext_acc.getValue(nbr)) {
		face_exterior[fi] = 1;
	    }
	}

	if (face_exterior[fi])
	    num_exterior++;
    }

    return num_exterior;
}


/*
 * Local Variables:
 * mode: C++
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
