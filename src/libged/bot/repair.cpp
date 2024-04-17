/*                     R E P A I R . C P P
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
/** @file libged/bot/repair.cpp
 *
 * The LIBGED bot repair subcommand.
 *
 * If a BoT is not manifold according to the Manifold library attempt various
 * repair operations to try and produce a manifold output using the specified
 * bot as input.
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
bot_repair(struct rt_bot_internal *bot)
{
    struct rt_bot_internal *obot = NULL;
    int rep_ret = rt_bot_repair(&obot, bot);
    if (rep_ret < 0) {
	// bot repair failed
	return NULL;
    }
    if (rep_ret == 1) {
	// Already valid, no-op
	return bot;
    }

    // Bot repair succeeded
    return obot;
}

static void
repair_usage(struct bu_vls *str, const char *cmd, struct bu_opt_desc *d) {
    char *option_help = bu_opt_describe(d, NULL);
    bu_vls_sprintf(str, "Usage: %s [options] input_bot [output_name]\n", cmd);
    if (option_help) {
	bu_vls_printf(str, "Options:\n%s\n", option_help);
	bu_free(option_help, "help str");
    }
}

extern "C" int
_bot_cmd_repair(void *bs, int argc, const char **argv)
{
    const char *usage_string = "bot repair <objname> [repaired_obj_name]";
    const char *purpose_string = "Attempt to produce a manifold output using objname's geometry as an input.  If successful, the resulting manifold geometry will either overwrite the original objname object (if no repaired_obj_name is supplied) or be written to repaired_obj_name.  Note that in-place repair is destructive - the original BoT data is lost.  If the input is already manifold repair is a no-op.";
    if (_bot_cmd_msgs(bs, argc, argv, usage_string, purpose_string)) {
	return BRLCAD_OK;
    }

    struct _ged_bot_info *gb = (struct _ged_bot_info *)bs;

    // We know we're the manifold command - start processing args
    argc--; argv++;

    int print_help = 0;
    int in_place_repair = 0;

    struct bu_opt_desc d[2];
    BU_OPT(d[0], "h",      "help",     "",            NULL, &print_help,      "Print help");
    BU_OPT_NULL(d[1]);

    int ac = bu_opt_parse(NULL, argc, argv, d);
    argc = ac;

    if (print_help || !argc) {
	repair_usage(gb->gedp->ged_result_str, "bot manifold", d);
	return GED_HELP;
    }

    if (argc == 1)
	in_place_repair = 1;

    if (!in_place_repair && argc != 2) {
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

    struct rt_bot_internal *mbot = bot_repair(bot);

    // If we were already manifold, there's nothing to do
    if (mbot && mbot == bot) {
	bu_vls_printf(gb->gedp->ged_result_str, "BoT %s is valid", gb->dp->d_namep);
	return BRLCAD_OK;
    }

    // Trying repair and couldn't, it's an error
    if (!mbot) {
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

