/*                     M A N I F O L D . C P P
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
/** @file libged/bot/manifold.cpp
 *
 * The LIBGED bot manifold subcommand.
 *
 * Checks if a BoT is manifold according to the Manifold library.  If
 * not, and if an output object name is specified, it will attempt
 * various repair operations to try and produce a manifold output
 * using the specified test bot as an input.
 */

#include "common.h"

#include <stdlib.h>
#include <string.h>

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

#include "manifold/manifold.h"

#include "bu/cmd.h"
#include "bu/color.h"
#include "bu/opt.h"
#include "bg/chull.h"
#include "bg/trimesh.h"
#include "rt/geom.h"
#include "wdb.h"

#include "./ged_bot.h"
#include "../ged_private.h"

static struct rt_bot_internal *
manifold_process(struct rt_bot_internal *bot, int repair)
{
    if (!bot)
	return NULL;

    manifold::Mesh bot_mesh;
    for (size_t j = 0; j < bot->num_vertices ; j++)
	bot_mesh.vertPos.push_back(glm::vec3(bot->vertices[3*j], bot->vertices[3*j+1], bot->vertices[3*j+2]));
    if (bot->orientation == RT_BOT_CW) {
	for (size_t j = 0; j < bot->num_faces; j++)
	    bot_mesh.triVerts.push_back(glm::vec3(bot->faces[3*j], bot->faces[3*j+2], bot->faces[3*j+1]));
    } else {
	for (size_t j = 0; j < bot->num_faces; j++)
	    bot_mesh.triVerts.push_back(glm::vec3(bot->faces[3*j], bot->faces[3*j+1], bot->faces[3*j+2]));
    }

    manifold::Manifold omanifold(bot_mesh);
    if (omanifold.Status() == manifold::Manifold::Error::NoError) {
	// BoT is already manifold
	return bot;
    }

    // If we're not going to try and repair it, it's just a
    // non-manifold mesh report at this point.
    if (!repair)
	return NULL;

    struct rt_bot_internal *obot = NULL;
    if (rt_bot_repair(&obot, bot)) {
	// bot repair failed
	return NULL;
    }

    // Bot repair succeeded
    return obot;
}

static void
manifold_usage(struct bu_vls *str, const char *cmd, struct bu_opt_desc *d) {
    char *option_help = bu_opt_describe(d, NULL);
    bu_vls_sprintf(str, "Usage: %s [options] input_bot [output_name]\n", cmd);
    if (option_help) {
	bu_vls_printf(str, "Options:\n%s\n", option_help);
	bu_free(option_help, "help str");
    }
}

extern "C" int
_bot_cmd_manifold(void *bs, int argc, const char **argv)
{
    const char *usage_string = "bot manifold <objname> [repaired_obj_name]";
    const char *purpose_string = "Check if Manifold thinks the BoT is a manifold mesh.  If not, and a repaired_obj_name has been supplied, attempt to produce a manifold output using objname's geometry as an input.  If successful, the resulting manifold geometry will be written to repaired_obj_name.";
    if (_bot_cmd_msgs(bs, argc, argv, usage_string, purpose_string)) {
	return BRLCAD_OK;
    }

    struct _ged_bot_info *gb = (struct _ged_bot_info *)bs;

    // We know we're the manifold command - start processing args
    argc--; argv++;

    int print_help = 0;
    int in_place_repair = 0;

    struct bu_opt_desc d[5];
    BU_OPT(d[0], "h",      "help",     "",            NULL, &print_help,      "Print help");
    BU_OPT(d[1], "i",  "in-place",     "",            NULL, &in_place_repair, "Alter BoTs (if necessary) in place");
    BU_OPT_NULL(d[2]);

    int ac = bu_opt_parse(NULL, argc, argv, d);
    argc = ac;

    if (print_help || !argc) {
	manifold_usage(gb->gedp->ged_result_str, "bot manifold", d);
	return GED_HELP;
    }

    if (in_place_repair && argc != 1) {
	bu_vls_printf(gb->gedp->ged_result_str, "%s", usage_string);
	return BRLCAD_ERROR;
    }

    if (!in_place_repair && argc != 1 && argc != 2) {
	bu_vls_printf(gb->gedp->ged_result_str, "%s", usage_string);
	return BRLCAD_ERROR;
    }

    if (!in_place_repair && db_lookup(gb->gedp->dbip, argv[1], LOOKUP_QUIET) != RT_DIR_NULL) {
	bu_vls_printf(gb->gedp->ged_result_str, "Object %s already exists!\n", argv[1]);
	return BRLCAD_ERROR;
    }

    if (_bot_obj_setup(gb, argv[0]) & BRLCAD_ERROR) {
	return BRLCAD_ERROR;
    }

    struct rt_bot_internal *bot = (struct rt_bot_internal *)(gb->intern->idb_ptr);
    if (bot->mode != RT_BOT_SOLID) {
	bu_vls_printf(gb->gedp->ged_result_str, "%s is a non-solid BoT, skipping", gb->dp->d_namep);
	return BRLCAD_ERROR;
    }

    int repair_flag = (in_place_repair || argc == 2) ?  1 : 0;

    struct rt_bot_internal *mbot = manifold_process(bot, repair_flag);

    // If we were already manifold, there's nothing to do but report it regardless of mode
    if (mbot && mbot == bot) {
	bu_vls_printf(gb->gedp->ged_result_str, "BoT %s is manifold", gb->dp->d_namep);
	return BRLCAD_OK;
    }

    // If we weren't asked to repair, just report non-manifold
    if (!mbot && !repair_flag) {
    	bu_vls_printf(gb->gedp->ged_result_str, "BoT %s is NOT manifold", gb->dp->d_namep);
	return BRLCAD_ERROR;
    }

    // If we were trying repair and couldn't, it's an error
    if (!mbot && repair_flag) {
	bu_vls_printf(gb->gedp->ged_result_str, "Unable to repair BoT %s", gb->dp->d_namep);
	return BRLCAD_ERROR;
    }

    // If we're repairing and we were able to fix it, write out the result
    struct rt_db_internal intern;
    RT_DB_INTERNAL_INIT(&intern);
    intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
    intern.idb_type = ID_BOT;
    intern.idb_meth = &OBJ[ID_BOT];
    intern.idb_ptr = (void *)mbot;

    const char *rname;
    struct directory *dp;
    if (in_place_repair) {
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

    bu_vls_printf(gb->gedp->ged_result_str, "Repair completed successfully and written to %s", rname);
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

