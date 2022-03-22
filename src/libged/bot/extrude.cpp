/*                      E X T R U D E . C P P
 * BRL-CAD
 *
 * Copyright (c) 2020-2022 United States Government as represented by
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
/** @file extrude.cpp
 *
 * Given a plate mode bot, approximate it with an extrusion of the
 * individual triangles using the average of the normals of each
 * vertex for a direction.
 *
 * This method tries to produce a region comb unioning individual BoT
 * objects for each face, to avoid visual gaps between individual
 * faces.  This comes at a cost in thickness accuracy, and can produce
 * other artifacts.  It will also produce very long, thin triangles
 * along the "sides" of plate regions.
 *
 */

#include "common.h"

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
#include <vector>

#include "vmath.h"
#include "bu/avs.h"
#include "bu/path.h"
#include "brep.h"
#include "wdb.h"
#include "analyze.h"
#include "ged.h"
#include "./ged_bot.h"


bool
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

// The "correct" way to do this unfortunately requires an implementation of a
// surface offsetting algorithm, which is a hard problem:
//
// Charlie C.L. Wang, and Yong Chen, "Thickening freeform surfaces for solid
// fabrication", Rapid Prototyping Journal, vol.19, no.6, pp.395-406, November
// 2013.
//
// Chen, Zhen and Panozzo, Daniele and Dumas, Jeremie, "Half-Space Power
// Diagrams and Discrete Surface Offsets", IEEE Transactions on Visualization
// and Computer Graphics, 2019 (https://github.com/geometryprocessing/voroffset)
extern "C" int
_bot_cmd_extrude(void *bs, int argc, const char **argv)
{
    const char *usage_string = "bot [options] extrude <objname> [output_obj]";
    const char *purpose_string = "generate an ARB6 representation of the specified plate mode BoT object";
    if (_bot_cmd_msgs(bs, argc, argv, usage_string, purpose_string)) {
	return BRLCAD_OK;
    }

    struct _ged_bot_info *gb = (struct _ged_bot_info *)bs;

    argc--; argv++;

    if (_bot_obj_setup(gb, argv[0]) & BRLCAD_ERROR) {
	return BRLCAD_ERROR;
    }

    struct rt_bot_internal *bot = (struct rt_bot_internal *)(gb->intern->idb_ptr);
    if (bot->mode != RT_BOT_PLATE && bot->mode != RT_BOT_PLATE_NOCOS) {
	bu_vls_printf(gb->gedp->ged_result_str, "Object %s is not a plate mode bot\n", gb->solid_name.c_str());
	return BRLCAD_ERROR;
    }

    // Check for at least 1 non-zero thickness, or there's no volume to define
    bool have_solid = false;
    for (size_t i = 0; i < bot->num_faces; i++) {
	if (bot->thickness[i] > VUNITIZE_TOL) {
	    have_solid = true;
	}
    }
    if (!have_solid) {
	bu_vls_printf(gb->gedp->ged_result_str, "bot %s does not have any non-degenerate face thicknesses\n", gb->solid_name.c_str());
	return BRLCAD_OK;
    }

    // Make a comb to hold the union of the new solid primitives
    struct wmember wcomb;
    struct bu_vls comb_name = BU_VLS_INIT_ZERO;
    if (argc > 1) {
	bu_vls_sprintf(&comb_name, "%s", argv[1]);
    } else {
	bu_vls_sprintf(&comb_name, "%s_extrusion.r", gb->dp->d_namep);
    }

    if (db_lookup(gb->gedp->dbip, bu_vls_cstr(&comb_name), LOOKUP_QUIET) != RT_DIR_NULL) {
	bu_vls_printf(gb->gedp->ged_result_str, "Object %s already exists!\n", bu_vls_cstr(&comb_name));
	bu_vls_free(&comb_name);
	return BRLCAD_ERROR;
    }

    BU_LIST_INIT(&wcomb.l);

    // For each face, define an arb6 using shifted vertices.  For each face
    // vertex two new points will be constructed - an inner and outer - based
    // on the original point, the local face thickness, and the avg face dir
    // and its inverse.  Check the face_mode flag to know how far to shift in
    // each direction - if the faces aren't centered which way the thickness is
    // applied depends on the incoming ray direction.  Here, that is
    // interpreted as both directions of ARB6 extrusion getting the full length
    // from the surface.
    struct bu_vls prim_name = BU_VLS_INIT_ZERO;
    for (size_t i = 0; i < bot->num_faces; i++) {
	point_t pnts[6];
	point_t pf[3];
	vect_t pv1[3], pv2[3];
	vect_t n = VINIT_ZERO;
	bot_face_normal(&n, bot, i);

	for (int j = 0; j < 3; j++) {
	    VMOVE(pf[j], &bot->vertices[bot->faces[i*3+j]*3]);
	    if (BU_BITTEST(bot->face_mode, i)) {
		VSCALE(pv1[j], n, bot->thickness[i]);
		VSCALE(pv2[j], n, -1*bot->thickness[i]);
	    } else {
		/* Note - g_bot_include.c uses .51, so using the same value
		 * here for consistency. */
		VSCALE(pv1[j], n, 0.51*bot->thickness[i]);
		VSCALE(pv2[j], n, -0.51*bot->thickness[i]);
	    }
	}
	for (int j = 0; j < 3; j++) {
	    point_t npnt1;
	    point_t npnt2;
	    VADD2(npnt1, pf[j], pv1[j]);
	    VADD2(npnt2, pf[j], pv2[j]);
	    VMOVE(pnts[j], npnt1);
	    VMOVE(pnts[j+3], npnt2);
	}

	// For arb6 creation, we need a specific point order
	fastf_t pnts_array[3*6];
	/* 1 */ pnts_array[0] = pnts[4][X]; pnts_array[1] = pnts[4][Y]; pnts_array[2] = pnts[4][Z];
	/* 2 */ pnts_array[3] = pnts[3][X]; pnts_array[4] = pnts[3][Y]; pnts_array[5] = pnts[3][Z];
	/* 3 */ pnts_array[6] = pnts[0][X]; pnts_array[7] = pnts[0][Y]; pnts_array[8] = pnts[0][Z];
	/* 4 */ pnts_array[9] = pnts[1][X]; pnts_array[10] = pnts[1][Y]; pnts_array[11] = pnts[1][Z];
	/* 5 */ pnts_array[12] = pnts[5][X]; pnts_array[13] = pnts[5][Y]; pnts_array[14] = pnts[5][Z];
	/* 6 */ pnts_array[15] = pnts[2][X]; pnts_array[16] = pnts[2][Y]; pnts_array[17] = pnts[2][Z];

	bu_vls_sprintf(&prim_name, "%s.arb6.%zd", gb->dp->d_namep, i);

	// For arb6 creation we need to move a couple points the array.

	mk_arb6(gb->gedp->ged_wdbp, bu_vls_cstr(&prim_name), pnts_array);
	(void)mk_addmember(bu_vls_cstr(&prim_name), &(wcomb.l), NULL, DB_OP_UNION);
    }

    // Write the comb
    mk_lcomb(gb->gedp->ged_wdbp, bu_vls_addr(&comb_name), &wcomb, 1, NULL, NULL, NULL, 0);

    bu_vls_free(&comb_name);
    bu_vls_free(&prim_name);

    return BRLCAD_OK;
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
