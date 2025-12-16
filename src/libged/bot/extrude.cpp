/*                      E X T R U D E . C P P
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
#include <set>
#include <unordered_set>
#include <vector>

extern "C" {
#include "vmath.h"
#include "bu/time.h"
#include "bg/trimesh.h"
#include "brep.h"
#include "rt/primitives/bot.h"
#include "wdb.h"
}
#include "ged.h"
#include "./ged_bot.h"

// Default to a maximum of 1% surface area change allowed - if we get more
// change than that, don't simplify
#define MAX_AREA_DELTA_PERCENT 0.01

// Default to 20% of triangles removed as a minimum threshold - i.e. if we
// can't remove at least that many, don't simplify the mesh.
#define MIN_TRICNT_REMOVAL_THRESHOLD 0.2

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

static void
extrude_usage(struct bu_vls *str, const char *cmd, struct bu_opt_desc *d) {
    char *option_help = bu_opt_describe(d, NULL);
    bu_vls_sprintf(str, "Usage: %s [options] input_bot [output_name]\n", cmd);
    if (option_help) {
	bu_vls_printf(str, "Options:\n%s\n", option_help);
	bu_free(option_help, "help str");
    }
}

extern "C" int
_bot_cmd_extrude(void *bs, int argc, const char **argv)
{
    const char *usage_string = "bot extrude [options] <objname> [output_obj]";
    const char *purpose_string = "generate a volumetric BoT or CSG tree from the specified plate mode BoT object";
    if (_bot_cmd_msgs(bs, argc, argv, usage_string, purpose_string)) {
	return BRLCAD_OK;
    }

    struct _ged_bot_info *gb = (struct _ged_bot_info *)bs;

    argc--; argv++;

    int print_help = 0;
    int extrude_in_place = 0;
    int round_edges = 0;
    int comb_tree = 0;
    int quiet_mode = 0;
    int force_mode = 0;
    fastf_t max_area_delta = -1.0;
    fastf_t min_tri_delta = -1.0;

    struct bu_opt_desc d[9];
    BU_OPT(d[0], "h",           "help",     "",            NULL, &print_help,       "Print help");
    BU_OPT(d[1], "q",          "quiet",     "",            NULL, &quiet_mode,       "Suppress output messages");
    BU_OPT(d[2], "i",       "in-place",     "",            NULL, &extrude_in_place, "Overwrite input BoT");
    BU_OPT(d[3], "R",    "round-edges",     "",            NULL, &round_edges,      "Apply rounding to outer BoT edges");
    BU_OPT(d[4], "C",           "comb",     "",            NULL, &comb_tree,        "Write out a CSG tree rather than a volumetric BoT");
    BU_OPT(d[5], "F",          "force",     "",            NULL, &force_mode,       "Generate output even if source bot is view dependent.");
    BU_OPT(d[6], "",  "max-area-delta",     "", &bu_opt_fastf_t, &max_area_delta,   "Maximum surface area percent change allowed when simplifying (scale 0 to 1).  0 == no change allowed, 1 = any change allowed");
    BU_OPT(d[7], "",   "min-tri-delta",     "", &bu_opt_fastf_t, &min_tri_delta,    "Minimum percentage of triangles that need to be removed (scale 0 to 1) before simplified mesh is used. 0 = any triangle count change is accepted, 1 = no change allowed");
    BU_OPT_NULL(d[8]);

    int ac = bu_opt_parse(NULL, argc, argv, d);
    argc = ac;

    if (print_help || !argc) {
	extrude_usage(gb->gedp->ged_result_str, "bot extrude", d);
	return GED_HELP;
    }

    if (extrude_in_place && argc != 1) {
	bu_vls_printf(gb->gedp->ged_result_str, "%s", usage_string);
	return BRLCAD_ERROR;
    }

    if (!extrude_in_place && argc != 2) {
	bu_vls_printf(gb->gedp->ged_result_str, "%s", usage_string);
	return BRLCAD_ERROR;
    }

    if (!extrude_in_place && db_lookup(gb->gedp->dbip, argv[1], LOOKUP_QUIET) != RT_DIR_NULL) {
	bu_vls_printf(gb->gedp->ged_result_str, "Object %s already exists!\n", argv[1]);
	return BRLCAD_ERROR;
    }

    if (_bot_obj_setup(gb, argv[0]) & BRLCAD_ERROR) {
	return BRLCAD_ERROR;
    }

    struct rt_bot_internal *bot = (struct rt_bot_internal *)(gb->intern->idb_ptr);
    if (bot->mode != RT_BOT_PLATE && bot->mode != RT_BOT_PLATE_NOCOS) {
	bu_vls_printf(gb->gedp->ged_result_str, "Object %s is not a plate mode bot\n", gb->solid_name.c_str());
	return BRLCAD_ERROR;
    }

    if (bot->mode == RT_BOT_PLATE_NOCOS) {
	if (!force_mode) {
	    if (!quiet_mode)
		bu_vls_printf(gb->gedp->ged_result_str, "bot %s is using RT_BOT_PLATE_NOCOS mode.\n\nCannot be accurately represented as a volume.  To force volumetric BoT generation, use the -F flag.\n", gb->solid_name.c_str());
	    return BRLCAD_ERROR;
	} else {
	    if (!quiet_mode)
		bu_vls_printf(gb->gedp->ged_result_str, "WARNING: object %s is using RT_BOT_PLATE_NOCOS mode\n\nConversion will report different thicknesses depending on incoming ray directions.\n", gb->solid_name.c_str());
	}
    }

    if (bot->face_mode) {
	bool view_dependent = false;
	for (size_t i = 0; i < bot->num_faces; i++) {
	    if (BU_BITTEST(bot->face_mode, i)) {
		view_dependent = true;
		break;
	    }
	}
	if (view_dependent) {
	    if (!force_mode) {
		if (!quiet_mode)
		    bu_vls_printf(gb->gedp->ged_result_str, "bot %s has face_mode set (i.e. it exhibits view-dependent shotline behavior).\n\nCannot be accurately represented as a volume.  To force volumetric BoT generation, use the -F flag.\n", gb->solid_name.c_str());
		return BRLCAD_ERROR;
	    } else {
		if (!quiet_mode)
		    bu_vls_printf(gb->gedp->ged_result_str, "WARNING: object %s has one or more faces with face_mode set.\n\nVolumetric BoT will report different in/out hit points than the original.\n", gb->solid_name.c_str());
	    }
	}
    }

    // Check for at least 1 non-zero thickness, or there's no volume to define
    bool have_solid = false;
    for (size_t i = 0; i < bot->num_faces; i++) {
	if (bot->thickness[i] > VUNITIZE_TOL) {
	    have_solid = true;
	}
    }
    if (!have_solid) {
	if (!quiet_mode)
	    bu_vls_printf(gb->gedp->ged_result_str, "bot %s does not have any non-degenerate face thicknesses\n", gb->solid_name.c_str());
	return BRLCAD_OK;
    }

    if (!comb_tree) {
	if (!quiet_mode)
	    bu_log("Processing %s\n", gb->solid_name.c_str());

	int64_t start = bu_gettime();

	struct rt_bot_internal *obot = NULL;
	// Default to maximum area change of 1% of the bot surface area
	if (max_area_delta < 0 || max_area_delta > 1) {
	    fastf_t bot_area = bg_trimesh_area(bot->faces, bot->num_faces, (const point_t *)bot->vertices, bot->num_vertices);
	    max_area_delta = MAX_AREA_DELTA_PERCENT*bot_area;
	}
	if (min_tri_delta < 0 || min_tri_delta > 1)
	    min_tri_delta = MIN_TRICNT_REMOVAL_THRESHOLD;

	int ret = rt_bot_plate_to_vol(&obot, bot, round_edges, quiet_mode, max_area_delta, min_tri_delta);
	if (ret != BRLCAD_OK || !obot) {
	    if (!quiet_mode)
		bu_vls_printf(gb->gedp->ged_result_str, "Volumetric conversion failed");
	    return BRLCAD_ERROR;
	}

	fastf_t elapsed = ((fastf_t)(bu_gettime() - start)) / 1000000.0;
	bu_log("BoT %s extruded in %g seconds\n", gb->solid_name.c_str(), elapsed);

	struct rt_db_internal intern;
	RT_DB_INTERNAL_INIT(&intern);
	intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
	intern.idb_type = ID_BOT;
	intern.idb_meth = &OBJ[ID_BOT];
	intern.idb_ptr = (void *)obot;

	const char *rname;
	struct directory *dp;
	if (extrude_in_place) {
	    rname = gb->dp->d_namep;
	    dp = gb->dp;
	} else {
	    rname = argv[1];
	    dp = db_diradd(gb->gedp->dbip, rname, RT_DIR_PHONY_ADDR, 0, RT_DIR_SOLID, (void *)&intern.idb_type);
	    if (dp == RT_DIR_NULL) {
		if (!quiet_mode)
		    bu_vls_printf(gb->gedp->ged_result_str, "Failed to write out new BoT %s", rname);
		return BRLCAD_ERROR;
	    }
	}

	if (rt_db_put_internal(dp, gb->gedp->dbip, &intern, &rt_uniresource) < 0) {
	    if (!quiet_mode)
		bu_vls_printf(gb->gedp->ged_result_str, "Failed to write out new BoT %s", rname);
	    return BRLCAD_ERROR;
	}
	return BRLCAD_OK;
    }

    // We're doing a CSG tree, not a bot

    // Make a comb to hold the union of the new solid primitives
    struct wmember wcomb;
    struct bu_vls comb_name = BU_VLS_INIT_ZERO;
    if (argc > 1) {
	bu_vls_sprintf(&comb_name, "%s", argv[1]);
    } else {
	bu_vls_sprintf(&comb_name, "%s_extrusion.r", gb->dp->d_namep);
    }

    if (db_lookup(gb->gedp->dbip, bu_vls_cstr(&comb_name), LOOKUP_QUIET) != RT_DIR_NULL) {
	if (!quiet_mode)
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
    struct rt_wdb *wdbp = wdb_dbopen(gb->gedp->dbip, RT_WDB_TYPE_DB_DEFAULT);
    struct bu_vls prim_name = BU_VLS_INIT_ZERO;

    // Collect the active vertices and edges
    std::set<int> verts;
    std::map<int, double> verts_thickness;
    std::map<int, int> verts_fcnt;
    std::map<std::pair<int, int>, double> edges_thickness;
    std::map<std::pair<int, int>, int> edges_fcnt;
    std::set<std::pair<int, int>> edges;
    for (size_t i = 0; i < bot->num_faces; i++) {
	point_t eind;
	double fthickness = 0.5*bot->thickness[i];
	for (int j = 0; j < 3; j++) {
	    verts.insert(bot->faces[i*3+j]);
	    verts_thickness[bot->faces[i*3+j]] += fthickness;
	    verts_fcnt[bot->faces[i*3+j]]++;
	    eind[j] = bot->faces[i*3+j];
	}
	int e11, e12, e21, e22, e31, e32;
	e11 = (eind[0] < eind[1]) ? eind[0] : eind[1];
	e12 = (eind[1] < eind[0]) ? eind[0] : eind[1];
	e21 = (eind[1] < eind[2]) ? eind[1] : eind[2];
	e22 = (eind[2] < eind[1]) ? eind[1] : eind[2];
	e31 = (eind[2] < eind[0]) ? eind[2] : eind[0];
	e32 = (eind[0] < eind[2]) ? eind[2] : eind[0];
	edges.insert(std::make_pair(e11, e12));
	edges.insert(std::make_pair(e21, e22));
	edges.insert(std::make_pair(e31, e32));
	edges_thickness[std::make_pair(e11, e12)] += fthickness;
	edges_thickness[std::make_pair(e21, e22)] += fthickness;
	edges_thickness[std::make_pair(e31, e32)] += fthickness;
	edges_fcnt[std::make_pair(e11, e12)]++;
	edges_fcnt[std::make_pair(e21, e22)]++;
	edges_fcnt[std::make_pair(e31, e32)]++;
    }

    // Any edge with only one face associated with it is an outer
    // edge.  Vertices on those edges are exterior vertices
    std::unordered_set<int> exterior_verts;
    std::set<std::pair<int, int>>::iterator e_it;
    for (e_it = edges.begin(); e_it != edges.end(); e_it++) {
	if (edges_fcnt[*e_it] == 1) {
	    exterior_verts.insert(e_it->first);
	    exterior_verts.insert(e_it->second);
	}
    }


    std::set<int>::iterator v_it;
    for (v_it = verts.begin(); v_it != verts.end(); v_it++) {
	if (!round_edges) {
	    if (exterior_verts.find(*v_it) != exterior_verts.end())
		continue;
	}

	point_t v;
	double r = ((double)verts_thickness[*v_it]/(double)(verts_fcnt[*v_it]));
	// Make a sph at the vertex point with a radius based on the thickness
	VMOVE(v, &bot->vertices[3**v_it]);
	bu_vls_sprintf(&prim_name, "%s.sph.%d", gb->dp->d_namep, *v_it);
	mk_sph(wdbp, bu_vls_cstr(&prim_name), v, r);
	(void)mk_addmember(bu_vls_cstr(&prim_name), &(wcomb.l), NULL, DB_OP_UNION);
    }

    for (e_it = edges.begin(); e_it != edges.end(); e_it++) {
	if (!round_edges) {
	    if (edges_fcnt[*e_it] == 1)
		continue;
	}

	double r = ((double)edges_thickness[*e_it]/(double)(edges_fcnt[*e_it]));
	// Make an rcc along the edge a radius based on the thickness
	point_t b, v;
	vect_t h;
	VMOVE(b, &bot->vertices[3*(*e_it).first]);
	VMOVE(v, &bot->vertices[3*(*e_it).second]);
	VSUB2(h, v, b);
	bu_vls_sprintf(&prim_name, "%s.rcc.%d_%d", gb->dp->d_namep, (*e_it).first, (*e_it).second);
	mk_rcc(wdbp, bu_vls_cstr(&prim_name), b, h, r);
	(void)mk_addmember(bu_vls_cstr(&prim_name), &(wcomb.l), NULL, DB_OP_UNION);
    }

    // Now, handle the primary arb faces
    for (size_t i = 0; i < bot->num_faces; i++) {
	point_t pnts[6];
	point_t pf[3];
	vect_t pv1[3], pv2[3];
	vect_t n = VINIT_ZERO;
	bot_face_normal(&n, bot, i);

	for (int j = 0; j < 3; j++) {
	    VMOVE(pf[j], &bot->vertices[bot->faces[i*3+j]*3]);
	    VSCALE(pv1[j], n, 0.5*bot->thickness[i]);
	    VSCALE(pv2[j], n, -0.5*bot->thickness[i]);
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
	mk_arb6(wdbp, bu_vls_cstr(&prim_name), pnts_array);
	(void)mk_addmember(bu_vls_cstr(&prim_name), &(wcomb.l), NULL, DB_OP_UNION);
    }

    // Write the comb
    mk_lcomb(wdbp, bu_vls_addr(&comb_name), &wcomb, 1, NULL, NULL, NULL, 0);

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
