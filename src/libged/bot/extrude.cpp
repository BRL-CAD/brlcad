/*                      E X T R U D E . C P P
 * BRL-CAD
 *
 * Copyright (c) 2020-2024 United States Government as represented by
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
#include <vector>

#include "vmath.h"
#include "rt/primitives/bot.h"
#include "ged.h"
#include "./ged_bot.h"

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
    const char *usage_string = "bot [options] extrude [-i] <objname> [output_obj]";
    const char *purpose_string = "generate a volumetric BoT from the specified plate mode BoT object";
    if (_bot_cmd_msgs(bs, argc, argv, usage_string, purpose_string)) {
	return BRLCAD_OK;
    }

    struct _ged_bot_info *gb = (struct _ged_bot_info *)bs;

    argc--; argv++;

    int print_help = 0;
    int extrude_in_place = 0;

    struct bu_opt_desc d[3];
    BU_OPT(d[0], "h",      "help",     "",            NULL, &print_help,      "Print help");
    BU_OPT(d[1], "i",  "in-place",     "",            NULL, &extrude_in_place, "Overwrite input BoT");
    BU_OPT_NULL(d[2]);

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

    struct bg_tess_tol ttol = BG_TESS_TOL_INIT_ZERO;
    struct bn_tol tol = BN_TOL_INIT_TOL;
    struct rt_bot_internal *obot;
    int ret = rt_bot_plate_to_vol(&obot, bot, &ttol, &tol, 1);
    if (ret != BRLCAD_OK) {
	bu_vls_printf(gb->gedp->ged_result_str, "Volumetric conversion failed");
	return BRLCAD_ERROR;
    }

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
	    bu_vls_printf(gb->gedp->ged_result_str, "Failed to write out new BoT %s", rname);
	    return BRLCAD_ERROR;
	}
    }

    if (rt_db_put_internal(dp, gb->gedp->dbip, &intern, &rt_uniresource) < 0) {
	bu_vls_printf(gb->gedp->ged_result_str, "Failed to write out new BoT %s", rname);
	return BRLCAD_ERROR;
    }

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
