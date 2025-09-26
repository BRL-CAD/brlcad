/*                   D S P _ T E S S . C P P
 * BRL-CAD
 *
 * Copyright (c) 1999-2025 United States Government as represented by
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
/** @addtogroup primitives */
/** @{ */
/** @file primitives/dsp/dsp_tess.cpp
 *
 * DSP tessellation logic
 *
 */

#include "common.h"


#include "vmath.h"
#include "raytrace.h"
#include "rt/functab.h"
#include "rt/geom.h"
#include "rt/primitives/bot.h"

#include "TerraScape.hpp"

/* private header */
#include "./dsp.h"

/**
 * Returns -
 * -1 failure
 * 0 OK.  *r points to nmgregion that holds this tessellation.
 */
extern "C" int
rt_dsp_tess(struct nmgregion **r, struct model *m, struct rt_db_internal *ip, const struct bg_tess_tol *ttol, const struct bn_tol *tol)
{
    struct rt_dsp_internal *dsp_ip;

    if (RT_G_DEBUG & RT_DEBUG_HF)
	bu_log("rt_dsp_tess()\n");

    RT_CK_DB_INTERNAL(ip);
    dsp_ip = (struct rt_dsp_internal *)ip->idb_ptr;
    RT_DSP_CK_MAGIC(dsp_ip);

    switch (dsp_ip->dsp_datasrc) {
	case RT_DSP_SRC_V4_FILE:
	case RT_DSP_SRC_FILE:
	    if (!dsp_ip->dsp_mp) {
		bu_log("WARNING: Cannot find data file for displacement map (DSP)\n");
		if (bu_vls_addr(&dsp_ip->dsp_name)) {
		    bu_log("         DSP data file [%s] not found or empty\n", bu_vls_cstr(&dsp_ip->dsp_name));
		} else {
		    bu_log("         DSP data file not found or not specified\n");
		}
		return -1;
	    }
	    break;
	case RT_DSP_SRC_OBJ:
	    if (!dsp_ip->dsp_bip) {
		bu_log("WARNING: Cannot find data object for displacement map (DSP)\n");
		if (bu_vls_addr(&dsp_ip->dsp_name)) {
		    bu_log("         DSP data object [%s] not found or empty\n", bu_vls_cstr(&dsp_ip->dsp_name));
		} else {
		    bu_log("         DSP data object not found or not specified\n");
		}
		return -1;
	    }
	    RT_CK_DB_INTERNAL(dsp_ip->dsp_bip);
	    RT_CK_BINUNIF(dsp_ip->dsp_bip->idb_ptr);
	    break;
    }

    // Step 1: Create TerraScape DSPData from rt_dsp_internal
    TerraScape::DSPData dsp;
    dsp.dsp_buf = dsp_ip->dsp_buf;           // Point to existing buffer (owned by BRL-CAD)
    dsp.dsp_xcnt = dsp_ip->dsp_xcnt;         // Copy dimensions
    dsp.dsp_ycnt = dsp_ip->dsp_ycnt;
    dsp.cell_size = 1.0;                     // Will be scaled by transformation matrix
    dsp.origin = TerraScape::Point3D(0, 0, 0);
    dsp.owns_buffer = false;                 // Don't delete BRL-CAD's buffer

    // Step 2. Convert to TerrainData
    TerraScape::TerrainData terrain;
    if (!dsp.toTerrain(terrain)) {
	bu_log("Failed to convert DSP buffer to TerrainData\n");
	return -1;
    }

    // Step 3.  Decide triangulation strategy based on tolerances -
    // we need to translate BRL-CAD's tolerances into those used by
    // the terrain algorithms.
    TerraScape::TerrainMesh mesh;

    point_t dsp_bb_min, dsp_bb_max;
    if (rt_dsp_bbox(ip, &dsp_bb_min, &dsp_bb_max, tol)) {
	/* Fallback if bbox computation fails */
	VSETALL(dsp_bb_min, 0.0);
	VSETALL(dsp_bb_max, 0.0);
    }
    double dx = dsp_bb_max[0] - dsp_bb_min[0];
    double dy = dsp_bb_max[1] - dsp_bb_min[1];
    double dz = dsp_bb_max[2] - dsp_bb_min[2];
    if (dx < 0) dx = 0;
    if (dy < 0) dy = 0;
    if (dz < 0) dz = 0;
    double diag = sqrt(dx*dx + dy*dy + dz*dz);
    double height_range = dz;

    /* Extract tessellation tolerances */
    double abs_tol = (ttol && ttol->abs > 0.0) ? ttol->abs : INFINITY;
    double rel_tol = (ttol && ttol->rel > 0.0) ? (ttol->rel * (diag > 0.0 ? diag : 1.0)) : INFINITY;

    double effective_err = INFINITY;
    if (abs_tol < INFINITY && rel_tol < INFINITY)
	effective_err = (abs_tol < rel_tol) ? abs_tol : rel_tol;
    else if (abs_tol < INFINITY)
	effective_err = abs_tol;
    else if (rel_tol < INFINITY)
	effective_err = rel_tol;

    /* Provide a fallback if neither tolerance is set */
    double base_cell = 1.0; /* original grid spacing pre-transform */
    if (!isfinite(effective_err))
	effective_err = base_cell * 0.25;

    /* Respect modeling tolerance floor */
    if (tol && tol->dist > 0.0 && effective_err < tol->dist * 0.5)
	effective_err = tol->dist * 0.5;

    /* Normal tolerance to slope threshold */
    double slope_threshold = 0.2; /* default fallback */
    if (ttol && ttol->norm > 0.0) {
	double angle_rad = 0.0;
	if (ttol->norm < 1.0) {
	    /* treat as cosine of angle */
	    if (ttol->norm > 0.0) {
		double c = ttol->norm;
		if (c > 1.0) c = 1.0;
		if (c < -1.0) c = -1.0;
		angle_rad = acos(c);
	    }
	} else {
	    /* treat as degrees */
	    angle_rad = ttol->norm * (M_PI / 180.0);
	}
	if (angle_rad > 0.0) {
	    double t = tan(angle_rad);
	    if (t < 0.0) t = -t;
	    /* clamp to avoid runaway */
	    if (t > 10.0) t = 10.0;
	    slope_threshold = t;
	}
    }

    /* Derive a heuristic reduction target */
    int min_reduction = 0;
    if (height_range > 1e-9) {
	double hscale = effective_err / height_range;
	if (hscale < 0.0) hscale = 0.0;
	if (hscale > 1.0) hscale = 1.0;
	min_reduction = (int)(hscale * 80.0); /* up to 80% if very loose */
    }
    if (min_reduction < 0) min_reduction = 0;
    if (min_reduction > 90) min_reduction = 90;

    TerraScape::SimplificationParams simp;
    simp.setErrorTol(effective_err);
    simp.setSlopeTol(slope_threshold);
    simp.setMinReduction(min_reduction);
    simp.setPreserveBounds(true);

    int use_simplified = 0;
    /* Decide whether to simplify:
       - If effective error significantly larger than base cell
       - Or slope tolerance generous
       */
    if (effective_err > base_cell * 0.6 || slope_threshold > 0.6) {
	use_simplified = 1;
    }

    if (RT_G_DEBUG & RT_DEBUG_HF) {
	bu_log("DSP tess tol mapping:\n");
	bu_log("  bbox: min=(%g %g %g) max=(%g %g %g)\n",
		V3ARGS(dsp_bb_min), V3ARGS(dsp_bb_max));
	bu_log("  abs_tol=%g rel_tol=%g -> eff=%g\n", abs_tol, rel_tol, effective_err);
	bu_log("  slope_threshold=%g min_triangle_reduction=%d (height_range=%g diag=%g)\n",
		slope_threshold, min_reduction, height_range, diag);
	bu_log("  using %s path\n", use_simplified ? "simplified" : "full");
    }

    // Step 4.  Make the TerraScape mesh (includes walls + bottom)
    if (use_simplified) {
	mesh.triangulateVolumeSimplified(terrain, simp);
    } else {
	mesh.triangulateVolume(terrain);
    }
    if (mesh.vertices.empty() || mesh.triangles.empty()) {
	bu_log("TerraScape produced empty mesh\n");
	return -1;
    }

    // Step 5.  Translate to BoT
    /* Allocate BOT internal */
    struct rt_bot_internal *bot_ip = (struct rt_bot_internal *)bu_calloc(1, sizeof(struct rt_bot_internal), "dsp bot_ip");
    bot_ip->magic = RT_BOT_INTERNAL_MAGIC;
    bot_ip->num_vertices = (int)mesh.vertices.size();
    bot_ip->num_faces = (int)mesh.triangles.size();
    bot_ip->vertices = (fastf_t *)bu_calloc(3 * bot_ip->num_vertices, sizeof(fastf_t), "bot verts");
    bot_ip->faces = (int *)bu_calloc(3 * bot_ip->num_faces, sizeof(int), "bot faces");
    bot_ip->thickness = NULL;
    bot_ip->face_mode = NULL;
    bot_ip->mode = RT_BOT_SOLID;
    bot_ip->orientation = RT_BOT_CCW;
    bot_ip->bot_flags = 0;
    bot_ip->face_normals = NULL;
    bot_ip->num_normals = 0;
    bot_ip->normals = NULL;
    bot_ip->num_face_normals = 0;
    bot_ip->face_normals = NULL;

    /* Populate vertices (apply dsp_stom) */
    for (size_t i = 0; i < bot_ip->num_vertices; ++i) {
	const TerraScape::Point3D &p = mesh.vertices[(size_t)i];
	point_t in = { p.x, p.y, p.z };
	point_t out;
	MAT4X3PNT(out, dsp_ip->dsp_stom, in);
	bot_ip->vertices[3*i+0] = out[0];
	bot_ip->vertices[3*i+1] = out[1];
	bot_ip->vertices[3*i+2] = out[2];
    }

    /* Populate faces */
    for (size_t f = 0; f < bot_ip->num_faces; ++f) {
	const TerraScape::Triangle &tri = mesh.triangles[(size_t)f];
	bot_ip->faces[3*f+0] = (int)tri.vertices[0];
	bot_ip->faces[3*f+1] = (int)tri.vertices[1];
	bot_ip->faces[3*f+2] = (int)tri.vertices[2];
    }

    /* Wrap in rt_db_internal and invoke standard BOT tessellator */
    struct rt_db_internal dsp_bot_internal;
    RT_DB_INTERNAL_INIT(&dsp_bot_internal);
    dsp_bot_internal.idb_major_type = DB5_MAJORTYPE_BRLCAD;
    dsp_bot_internal.idb_type = ID_BOT;
    dsp_bot_internal.idb_meth = &OBJ[ID_BOT];
    dsp_bot_internal.idb_ptr = (void *)bot_ip;

    int ret = rt_bot_tess(r, m, &dsp_bot_internal, NULL, tol);

    /* Free our temporary BOT internal (region already constructed) */
    rt_db_free_internal(&dsp_bot_internal);

    return ret;
}

/** @} */


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8 cino=N-s
