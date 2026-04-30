/*               T R I M E S H _ R E M E S H . C P P
 * BRL-CAD
 *
 * Copyright (c) 2025 United States Government as represented by
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
/** @file trimesh_remesh.cpp
 *
 * Implementation of bg_trimesh_remesh: remesh a triangle mesh using the
 * Geogram CVT (Centroidal Voronoi Tessellation) remeshing routines bundled
 * inside libbg.
 */

#include "common.h"

#include "geogram/basic/geogram_options.h"
#include "geogram/mesh/mesh.h"
#include "geogram/mesh/mesh_geometry.h"
#include "geogram/mesh/mesh_repair.h"
#include "geogram/mesh/mesh_remesh.h"

#include "vmath.h"
#include "bu/malloc.h"
#include "bg/trimesh.h"


/* --------------------------------------------------------------------------
 * Public API
 * -------------------------------------------------------------------------- */

extern "C" int
bg_trimesh_remesh(
	int **ofaces, int *n_ofaces,
	point_t **opnts, int *n_opnts,
	const int *ifaces, int n_ifaces,
	const point_t *ipnts, int n_ipnts,
	struct bg_trimesh_remesh_opts *opts)
{
    if (!ofaces || !n_ofaces || !opnts || !n_opnts)
	return -1;
    if (!ifaces || n_ifaces <= 0 || !ipnts || n_ipnts <= 0)
	return -1;

    /* Initialize output pointers */
    *ofaces = NULL;
    *n_ofaces = 0;
    *opnts = NULL;
    *n_opnts = 0;

    /* Use caller-supplied opts or fall back to defaults */
    struct bg_trimesh_remesh_opts default_opts = BG_TRIMESH_REMESH_OPTS_DEFAULT;
    if (!opts)
	opts = &default_opts;

    /* Determine target point count */
    GEOBRL::index_t nb_pts;
    if (opts->target_count > 0) {
	nb_pts = (GEOBRL::index_t)opts->target_count;
    } else {
	fastf_t mult = (opts->count_multiplier > 0.0) ? opts->count_multiplier : 10.0;
	nb_pts = (GEOBRL::index_t)(n_ipnts * mult);
    }
    if (nb_pts == 0)
	nb_pts = (GEOBRL::index_t)n_ipnts;

    /* Build the Geogram input mesh */
    GEOBRL::Mesh gm;
    gm.vertices.assign_points((double *)ipnts, 3, (GEOBRL::index_t)n_ipnts);
    for (int i = 0; i < n_ifaces; i++) {
	GEOBRL::index_t f = gm.facets.create_polygon(3);
	gm.facets.set_vertex(f, 0, (GEOBRL::index_t)ifaces[3*i+0]);
	gm.facets.set_vertex(f, 1, (GEOBRL::index_t)ifaces[3*i+1]);
	gm.facets.set_vertex(f, 2, (GEOBRL::index_t)ifaces[3*i+2]);
    }

    /* Geogram options – enable multi-nerve support for complex topology */
    GEOBRL::GeoOptions geo_opts;
    geo_opts.remesh_multi_nerve = true;

    /* Pre-repair: colocate near-duplicate vertices, remove degenerate faces */
    {
	double bbox_diag = GEOBRL::bbox_diagonal(gm);
	double epsilon = (bbox_diag > 0.0) ? 1e-6 * (0.01 * bbox_diag) : 0.0;
	GEOBRL::mesh_repair(gm,
			    GEOBRL::MeshRepairMode(GEOBRL::MESH_REPAIR_DEFAULT),
			    epsilon,
			    geo_opts);
    }

    if (gm.facets.nb() == 0)
	return -1;

    /* Anisotropic remesh: add normal-direction channels to the mesh point
     * storage so that the CVT solver weights the normal direction according
     * to the caller-supplied anisotropy factor.  A factor of 0.0 skips this
     * step and produces an isotropic remesh. */
    if (opts->anisotropy > 0.0) {
	GEOBRL::compute_normals(gm);
	GEOBRL::set_anisotropy(gm, (double)opts->anisotropy);
    }

    /* Remesh – dim=0 lets Geogram pick the working dimension automatically
     * (3 for isotropic, 6 for anisotropic after set_anisotropy). */
    GEOBRL::Mesh remesh;
    GEOBRL::remesh_smooth(
	gm, remesh, nb_pts, geo_opts,
	/* dim        */ 0,
	/* lloyd_iter */ (GEOBRL::index_t)((opts->lloyd_iters  >= 0) ? opts->lloyd_iters  : 5),
	/* newton_iter*/ (GEOBRL::index_t)((opts->newton_iters >= 0) ? opts->newton_iters : 30));

    if (remesh.vertices.nb() == 0 || remesh.facets.nb() == 0)
	return -1;

    /* Convert Geogram output mesh back to caller-owned arrays */
    int nv = (int)remesh.vertices.nb();
    int nf = (int)remesh.facets.nb();

    point_t *out_pts = (point_t *)bu_calloc((size_t)nv, sizeof(point_t),
					    "bg_trimesh_remesh verts");
    int *out_faces   = (int *)bu_calloc((size_t)nf * 3, sizeof(int),
					"bg_trimesh_remesh faces");

    for (int i = 0; i < nv; i++) {
	const double *p = remesh.vertices.point_ptr((GEOBRL::index_t)i);
	out_pts[i][X] = p[0];
	out_pts[i][Y] = p[1];
	out_pts[i][Z] = p[2];
    }
    for (int i = 0; i < nf; i++) {
	out_faces[3*i+0] = (int)remesh.facets.vertex((GEOBRL::index_t)i, 0);
	out_faces[3*i+1] = (int)remesh.facets.vertex((GEOBRL::index_t)i, 1);
	out_faces[3*i+2] = (int)remesh.facets.vertex((GEOBRL::index_t)i, 2);
    }

    *opnts   = out_pts;
    *n_opnts = nv;
    *ofaces   = out_faces;
    *n_ofaces = nf;

    return 0;
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
