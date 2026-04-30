/*                T R I M E S H _ R E P A I R . C P P
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
/** @file trimesh_repair.cpp
 *
 * Implementation of bg_trimesh_repair: repair a non-manifold triangle mesh
 * using the Geometric Tools Engine (GTE) mesh repair and hole-filling routines
 * bundled inside libbg.
 */

#include "common.h"

#include <array>
#include <cstdint>
#include <cstring>
#include <vector>

#include <Mathematics/Vector3.h>
#include <Mathematics/MeshRepair.h>
#include <Mathematics/MeshHoleFilling.h>
#include <Mathematics/MeshPreprocessing.h>

#include "vmath.h"
#include "bu/malloc.h"
#include "bg/trimesh.h"

/* --------------------------------------------------------------------------
 * Internal helpers (mirrors analogous helpers in librt/primitives/bot/repair.cpp
 * but operating on bare GTE types rather than rt_bot_internal).
 * -------------------------------------------------------------------------- */

static double
trimesh_gte_bbox_diag(std::vector<gte::Vector3<double>> const& verts)
{
    if (verts.empty())
	return 0.0;

    double mn[3] = { verts[0][0], verts[0][1], verts[0][2] };
    double mx[3] = { verts[0][0], verts[0][1], verts[0][2] };
    for (auto const& v : verts) {
	for (int c = 0; c < 3; c++) {
	    if (v[c] < mn[c]) mn[c] = v[c];
	    if (v[c] > mx[c]) mx[c] = v[c];
	}
    }
    double dx = mx[0]-mn[0], dy = mx[1]-mn[1], dz = mx[2]-mn[2];
    return std::sqrt(dx*dx + dy*dy + dz*dz);
}

static double
trimesh_gte_area(std::vector<gte::Vector3<double>> const& verts,
		 std::vector<std::array<int32_t, 3>> const& tris)
{
    double area = 0.0;
    for (auto const& tri : tris) {
	gte::Vector3<double> const& p0 = verts[tri[0]];
	gte::Vector3<double> const& p1 = verts[tri[1]];
	gte::Vector3<double> const& p2 = verts[tri[2]];
	gte::Vector3<double> e1 = p1 - p0;
	gte::Vector3<double> e2 = p2 - p0;
	gte::Vector3<double> cr = gte::Cross(e1, e2);
	area += gte::Length(cr) * 0.5;
    }
    return area;
}


/* --------------------------------------------------------------------------
 * Public API
 * -------------------------------------------------------------------------- */

extern "C" int
bg_trimesh_repair(
	int **ofaces, int *n_ofaces,
	point_t **opnts, int *n_opnts,
	const int *ifaces, int n_ifaces,
	const point_t *ipnts, int n_ipnts,
	struct bg_trimesh_repair_opts *opts)
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
    struct bg_trimesh_repair_opts default_opts = BG_TRIMESH_REPAIR_OPTS_DEFAULT;
    if (!opts)
	opts = &default_opts;

    /* Quick check: is the mesh already solid?  Return 1 if so. */
    int not_solid = bg_trimesh_solid2(n_ipnts, n_ifaces,
				      (fastf_t *)ipnts, (int *)ifaces,
				      NULL);
    if (!not_solid)
	return 1;

    /* Convert input arrays to GTE types. */
    std::vector<gte::Vector3<double>> verts((size_t)n_ipnts);
    for (int i = 0; i < n_ipnts; i++) {
	verts[i][0] = ipnts[i][X];
	verts[i][1] = ipnts[i][Y];
	verts[i][2] = ipnts[i][Z];
    }
    std::vector<std::array<int32_t, 3>> tris((size_t)n_ifaces);
    for (int i = 0; i < n_ifaces; i++) {
	tris[i][0] = ifaces[3*i+0];
	tris[i][1] = ifaces[3*i+1];
	tris[i][2] = ifaces[3*i+2];
    }

    /* --- Pass 1: initial colocate + degenerate removal ------------------- */
    {
	double bbox_diag = trimesh_gte_bbox_diag(verts);
	gte::MeshRepair<double>::Parameters rp;
	rp.epsilon = (bbox_diag > 0.0) ? 1e-6 * (0.01 * bbox_diag) : 0.0;
	gte::MeshRepair<double>::Repair(verts, tris, rp);
    }

    if (tris.empty())
	return -1;

    /* --- Pass 2: remove small disconnected components -------------------- */
    {
	double area = trimesh_gte_area(verts, tris);
	double min_comp_area = 0.03 * area;
	if (min_comp_area > 0.0) {
	    size_t nf_before = tris.size();
	    gte::MeshPreprocessing<double>::RemoveSmallComponents(verts, tris, min_comp_area);
	    if (tris.size() != nf_before) {
		/* Re-run basic repair after component removal. */
		double bbox_diag = trimesh_gte_bbox_diag(verts);
		gte::MeshRepair<double>::Parameters rp;
		rp.epsilon = (bbox_diag > 0.0) ? 1e-6 * (0.01 * bbox_diag) : 0.0;
		gte::MeshRepair<double>::Repair(verts, tris, rp);
	    }
	}
    }

    if (tris.empty())
	return -1;

    /* --- Pass 2b: pre-fill topology normalisation -----------------------
     * The old Geogram-based repair called mesh_repair(MESH_REPAIR_DEFAULT)
     * which includes G4 (connect) + G5 (reorient) + G6 (split non-manifold)
     * BEFORE hole filling.  Without this, GTE's hole-boundary tracer can
     * encounter non-simple (self-touching) boundary loops that it aborts on,
     * leaving those holes unfilled.  Calling the full G4-G6 sequence here
     * fixes non-manifold vertices first so hole detection sees clean,
     * simple boundary loops. */
    {
	std::vector<int32_t> adj;
	gte::MeshRepair<double>::ConnectFacets(tris, adj);
	gte::MeshRepair<double>::ReorientFacetsAntiMoebius(verts, tris, adj);

	gte::MeshRepair<double>::ConnectFacets(tris, adj);
	gte::MeshRepair<double>::SplitNonManifoldVertices(verts, tris, adj);
    }

    /* --- Pass 3 + 4: iterative hole-fill + topology-repair loop -----------
     * Run fill-then-repair repeatedly.  Each post-fill round of G4-G6 may
     * expose previously blocked hole boundaries (non-simple loops caused by
     * non-manifold vertices) so subsequent FillHoles passes can fill them.
     * Stop when no new faces are added (convergence) or after a safety limit.
     *
     * This mirrors the old Geogram approach:
     *   GEO::fill_holes(gm, hole_size);
     *   GEO::mesh_repair(gm, GEO::MESH_REPAIR_DEFAULT);  // G1-G8 incl. G4-G6
     * where a single pass was sufficient because Geogram's fill_holes handles
     * complex boundary loops natively.  GTE's FillHoles requires the extra
     * G4-G6 steps to untangle the topology between iterations. */
    for (int iter = 0; iter < 10; ++iter) {
	size_t nf_before = tris.size();

	/* Pass 3: hole filling */
	{
	    double area = trimesh_gte_area(verts, tris);

	    double hole_limit = 1e30; /* default: attempt to fill all holes */
	    if (opts->max_hole_area > SMALL_FASTF) {
		hole_limit = (double)opts->max_hole_area;
	    } else if (opts->max_hole_area_percent > SMALL_FASTF) {
		hole_limit = area * ((double)opts->max_hole_area_percent / 100.0);
	    }

	    gte::MeshHoleFilling<double>::Parameters fp;
	    fp.maxArea      = hole_limit;
	    fp.method       = gte::MeshHoleFilling<double>::TriangulationMethod::LSCM;
	    fp.autoFallback = true;
	    gte::MeshHoleFilling<double>::FillHoles(verts, tris, fp);
	}

	if (tris.empty())
	    return -1;

	/* Pass 4: post-fill topology repair (G4+G5+G6+G8) */
	{
	    std::vector<int32_t> adj;
	    gte::MeshRepair<double>::ConnectFacets(tris, adj);
	    gte::MeshRepair<double>::ReorientFacetsAntiMoebius(verts, tris, adj);
	    gte::MeshRepair<double>::ConnectFacets(tris, adj);
	    gte::MeshRepair<double>::SplitNonManifoldVertices(verts, tris, adj);
	}
	gte::MeshPreprocessing<double>::OrientNormals(verts, tris);

	/* Convergence: stop when no new faces were added */
	if (tris.size() == nf_before)
	    break;
    }

    /* --- Build output arrays --------------------------------------------- */
    int nv = (int)verts.size();
    int nf = (int)tris.size();

    point_t *out_pts = (point_t *)bu_calloc((size_t)nv, sizeof(point_t),
					    "bg_trimesh_repair verts");
    int *out_faces   = (int *)bu_calloc((size_t)nf * 3, sizeof(int),
					"bg_trimesh_repair faces");

    for (int i = 0; i < nv; i++) {
	out_pts[i][X] = verts[i][0];
	out_pts[i][Y] = verts[i][1];
	out_pts[i][Z] = verts[i][2];
    }
    for (int i = 0; i < nf; i++) {
	out_faces[3*i+0] = tris[i][0];
	out_faces[3*i+1] = tris[i][1];
	out_faces[3*i+2] = tris[i][2];
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
