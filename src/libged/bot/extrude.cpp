/*                      E X T R U D E . C P P
 * BRL-CAD
 *
 * Copyright (c) 2020 United States Government as represented by
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



extern "C" int
_bot_cmd_extrude(void *bs, int argc, const char **argv)
{
    const char *usage_string = "bot [options] <objname> extrude <output_obj>";
    const char *purpose_string = "generate an ARB6 representation of the specified plate mode BoT object";
    if (_bot_cmd_msgs(bs, argc, argv, usage_string, purpose_string)) {
	return GED_OK;
    }

    struct _ged_bot_info *gb = (struct _ged_bot_info *)bs;

    if (gb->intern.idb_minor_type != DB5_MINORTYPE_BRLCAD_BOT) {
        bu_vls_printf(gb->gedp->ged_result_str, ": object %s is not of type bot\n", gb->solid_name.c_str());
        return GED_ERROR;
    }

    struct rt_bot_internal *bot = (struct rt_bot_internal *)(gb->intern.idb_ptr);
    if (bot->mode != RT_BOT_PLATE && bot->mode != RT_BOT_PLATE_NOCOS) {
        bu_vls_printf(gb->gedp->ged_result_str, ": object %s is not a plate mode bot\n", gb->solid_name.c_str());
	return GED_ERROR;
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
	return GED_OK;
    }

    // Average the face normals at each vertex to get an average direction in
    // which to move each vertex for solid generation.
    vect_t *f2n = (vect_t *)bu_calloc(bot->num_faces, sizeof(vect_t), "face normals");
    vect_t *v2n = (vect_t *)bu_calloc(bot->num_vertices, sizeof(vect_t), "vert normals");
    int *v2fc = (int *)bu_calloc(bot->num_vertices, sizeof(int), "cnts");
    for (size_t i = 0; i < bot->num_faces; i++) {
	vect_t n;
	bot_face_normal(&n, bot, i);
	VMOVE(f2n[i],n);
    }
    // Add up all the face normal vectors at each vertex
    for (size_t i = 0; i < bot->num_faces; i++) {
	for (int j = 0; j < 3; j++) {
	    VADD2(v2n[bot->faces[i*3+j]], v2n[bot->faces[i*3+j]], f2n[i]);
	    v2fc[bot->faces[i*3+j]] = v2fc[bot->faces[i*3+j]] + 1;
	}
    }
    // Average based on how many faces contributed
    for (size_t i = 0; i < bot->num_vertices; i++) {
	VSCALE(v2n[i], v2n[i], (1.0/(double)v2fc[i]));
    }
    // Unitize
    for (size_t i = 0; i < bot->num_vertices; i++) {
	VUNITIZE(v2n[i]);
    }

    // Make a comb to hold the union of the new solid primitives
    struct wmember wcomb;
    struct bu_vls comb_name = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&comb_name, "%s_solid.r", gb->dp->d_namep);
    // TODO - db_lookup to make sure it doesn't already exist
    BU_LIST_INIT(&wcomb.l);

    // For each face, define an arb6 using shifted vertices.  For each face
    // vertex two new points will be constructed - an inner and outer - based
    // on the original point, the local face thickness, and the avg face dir
    // and its inverse.  Check the face_mode flag to know which points to shift
    // in which direction.
    struct bu_vls prim_name = BU_VLS_INIT_ZERO;
    for (size_t i = 0; i < bot->num_faces; i++) {
	point_t pnts[6];
	point_t pf[3];
	vect_t pv1[3], pv2[3];
	for (int j = 0; j < 3; j++) {
	    vect_t fnorm, n;
	    VMOVE(fnorm, v2n[bot->faces[i*3+j]]);
	    bot_face_normal(&n, bot, i);
	    double vdot = VDOT(fnorm, n);
	    if (vdot < 0) {
		VSCALE(fnorm, fnorm, -1);
	    }
	    VMOVE(pf[j], &bot->vertices[bot->faces[i*3+j]*3]);
	    VSCALE(pv1[j], fnorm, bot->thickness[i] * ((bot->mode == RT_BOT_CW) ? -1 : 0));
	    VSCALE(pv2[j], fnorm, -1*bot->thickness[i] * ((bot->mode == RT_BOT_CW) ? -1 : 0));
	}
	for (int j = 0; j < 3; j++) {
	    point_t npnt1;
	    point_t npnt2;
	    VADD2(npnt1, pf[j], pv1[j]);
	    VADD2(npnt2, pf[j], pv2[j]);
	    VMOVE(pnts[j], npnt1);
	    VMOVE(pnts[j+3], npnt2);
	}

	// We are in effect creating an approximation of a twisted arb6 with this
	// shape - that means we need 8 bot faces on the 6 points.
	// To make it easier to use an arb6 shape as a guide, assign the points in arb6 ordering:
	fastf_t pnts_array[3*6+3*3];
	/* 1 */ pnts_array[0] = pnts[4][X]; pnts_array[1] = pnts[4][Y]; pnts_array[2] = pnts[4][Z];
	/* 2 */ pnts_array[3] = pnts[3][X]; pnts_array[4] = pnts[3][Y]; pnts_array[5] = pnts[3][Z];
	/* 3 */ pnts_array[6] = pnts[0][X]; pnts_array[7] = pnts[0][Y]; pnts_array[8] = pnts[0][Z];
	/* 4 */ pnts_array[9] = pnts[1][X]; pnts_array[10] = pnts[1][Y]; pnts_array[11] = pnts[1][Z];
	/* 5 */ pnts_array[12] = pnts[5][X]; pnts_array[13] = pnts[5][Y]; pnts_array[14] = pnts[5][Z];
	/* 6 */ pnts_array[15] = pnts[2][X]; pnts_array[16] = pnts[2][Y]; pnts_array[17] = pnts[2][Z];

	// Three new points are needed at the centers of what would otherwise be
	// twisted faces, to define triangles that won't overlap.
	// ARB6 face 1,2,3,4
	point_t pnts_avg[3] = {VINIT_ZERO, VINIT_ZERO, VINIT_ZERO};
	VADD2(pnts_avg[0], pnts[0], pnts_avg[0]);
	VADD2(pnts_avg[0], pnts[1], pnts_avg[0]);
	VADD2(pnts_avg[0], pnts[3], pnts_avg[0]);
	VADD2(pnts_avg[0], pnts[4], pnts_avg[0]);
	VSCALE(pnts_avg[0], pnts_avg[0], 0.25);
	/* 7 */ pnts_array[18] = pnts_avg[0][X]; pnts_array[19] = pnts_avg[0][Y]; pnts_array[20] = pnts_avg[0][Z];

	// ARB6 face 2,3,5,6
	VADD2(pnts_avg[1], pnts[0], pnts_avg[1]);
	VADD2(pnts_avg[1], pnts[2], pnts_avg[1]);
	VADD2(pnts_avg[1], pnts[3], pnts_avg[1]);
	VADD2(pnts_avg[1], pnts[5], pnts_avg[1]);
	VSCALE(pnts_avg[1], pnts_avg[1], 0.25);
	/* 8 */ pnts_array[21] = pnts_avg[1][X]; pnts_array[22] = pnts_avg[1][Y]; pnts_array[23] = pnts_avg[1][Z];

	// ARB6 face 1,5,6,4
	VADD2(pnts_avg[2], pnts[1], pnts_avg[2]);
	VADD2(pnts_avg[2], pnts[2], pnts_avg[2]);
	VADD2(pnts_avg[2], pnts[4], pnts_avg[2]);
	VADD2(pnts_avg[2], pnts[5], pnts_avg[2]);
	VSCALE(pnts_avg[2], pnts_avg[2], 0.25);
	/* 9 */ pnts_array[24] = pnts_avg[2][X]; pnts_array[25] = pnts_avg[2][Y]; pnts_array[26] = pnts_avg[2][Z];

	int faces_array[3*14];
	/*  1 */ faces_array[0] = 1; faces_array[1] = 0; faces_array[2] = 4;
	/*  2 */ faces_array[3] = 3; faces_array[4] = 2; faces_array[5] = 5;

	/*  3 */ faces_array[6]  = 1; faces_array[7]  = 2; faces_array[8]  = 6;
	/*  4 */ faces_array[9]  = 2; faces_array[10] = 3; faces_array[11] = 6;
	/*  5 */ faces_array[12] = 3; faces_array[13] = 0; faces_array[14] = 6;
	/*  6 */ faces_array[15] = 0; faces_array[16] = 1; faces_array[17] = 6;
	/*  7 */ faces_array[18] = 4; faces_array[19] = 5; faces_array[20] = 7;
	/*  8 */ faces_array[21] = 5; faces_array[22] = 2; faces_array[23] = 7;
	/*  9 */ faces_array[24] = 2; faces_array[25] = 1; faces_array[26] = 7;
	/* 10 */ faces_array[27] = 1; faces_array[28] = 4; faces_array[29] = 7;
	/* 11 */ faces_array[30] = 4; faces_array[31] = 0; faces_array[32] = 8;
	/* 12 */ faces_array[33] = 5; faces_array[34] = 4; faces_array[35] = 8;
	/* 13 */ faces_array[36] = 3; faces_array[37] = 5; faces_array[38] = 8;
	/* 14 */ faces_array[39] = 0; faces_array[40] = 3; faces_array[41] = 8;

	bu_vls_sprintf(&prim_name, "%s.tarb6.%zd", gb->dp->d_namep, i);

	mk_bot(gb->gedp->ged_wdbp, bu_vls_cstr(&prim_name), RT_BOT_SOLID, RT_BOT_CCW, 0, 9, 14, (fastf_t *)pnts_array, (int *)faces_array, NULL, NULL);
	(void)mk_addmember(bu_vls_cstr(&prim_name), &(wcomb.l), NULL, DB_OP_UNION);
    }

    // Write the comb
    mk_lcomb(gb->gedp->ged_wdbp, bu_vls_addr(&comb_name), &wcomb, 1, NULL, NULL, NULL, 0);

    bu_vls_free(&comb_name);
    bu_vls_free(&prim_name);
    bu_free(f2n, "face normals");
    bu_free(v2n, "vert normals");
    bu_free(v2fc, "cnts");

    return GED_OK;
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
