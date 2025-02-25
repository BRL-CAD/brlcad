/*                     R E P A I R . C P P
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
bot_repair(struct rt_bot_internal *bot, struct rt_bot_repair_info *i)
{
    struct rt_bot_internal *obot = NULL;
    int rep_ret = rt_bot_repair(&obot, bot, i);
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
    bu_vls_sprintf(str, "Usage: %s [options] input_bot [output_name]\n\n", cmd);
    bu_vls_printf(str, "Attempts to produce a manifold output using objname's geometry as an input.  If\n");
    bu_vls_printf(str, "successful, the resulting manifold geometry will either overwrite the original\n");
    bu_vls_printf(str, "objname object (if no repaired_obj_name is supplied) or be written to\n");
    bu_vls_printf(str, "repaired_obj_name.  Note that in-place repair is destructive - the original BoT\n");
    bu_vls_printf(str, "data is lost.  If the input is already manifold repair is a no-op.\n\n");
    if (option_help) {
	bu_vls_printf(str, "Options:\n%s\n", option_help);
	bu_free(option_help, "help str");
    }
}

extern "C" int
_bot_cmd_repair(void *bs, int argc, const char **argv)
{
    const char *usage_string = "bot repair [options] <namepattern1> [namepattern2 ...]";
    const char *purpose_string = "Attempt to convert objname to a manifold BoT.";
    if (_bot_cmd_msgs(bs, argc, argv, usage_string, purpose_string)) {
	return BRLCAD_OK;
    }

    struct _ged_bot_info *gb = (struct _ged_bot_info *)bs;

    // We know we're the manifold command - start processing args
    argc--; argv++;

    int print_help = 0;
    int in_place_repair = 1;
    struct rt_bot_repair_info settings = RT_BOT_REPAIR_INFO_INIT;
    struct bu_vls obot_name = BU_VLS_INIT_ZERO;

    struct bu_opt_desc d[5];
    BU_OPT(d[0], "h",  "help",                "",             NULL,                     &print_help,  "Print help");
    BU_OPT(d[1], "p",  "max-hole-percent",   "#",   bu_opt_fastf_t, &settings.max_hole_area_percent,  "Maximum hole area to repair (percentage of mesh surface area, range 0-100.) 0 and 100 mean always attempt filling operations. Overridden by -a option.");
    BU_OPT(d[2], "a",  "max-hole-area",     " #",   bu_opt_fastf_t,         &settings.max_hole_area,  "Maximum hole area to repair in mm (Hard upper limit regardless of mesh size, overrides -p option.)");
    BU_OPT(d[3], "o",  "output-name",   "<name>",       bu_opt_vls,                      &obot_name,  "Output object name (write repaired BoT to this name - avoids overwriting input BoT)");
    BU_OPT_NULL(d[4]);

    int ac = bu_opt_parse(NULL, argc, argv, d);
    argc = ac;

    if (print_help || !argc) {
	repair_usage(gb->gedp->ged_result_str, "bot manifold", d);
	bu_vls_free(&obot_name);
	return GED_HELP;
    }

    if (bu_vls_strlen(&obot_name))
	in_place_repair = 0;

    if (!in_place_repair) {
	if (argc != 1) {
	    bu_vls_printf(gb->gedp->ged_result_str, "When specifying an output object name, only one input at a time may be processed.");
	    bu_vls_free(&obot_name);
	    return BRLCAD_ERROR;
	}
	if (db_lookup(gb->gedp->dbip, bu_vls_cstr(&obot_name), LOOKUP_QUIET) != RT_DIR_NULL) {
	    bu_vls_printf(gb->gedp->ged_result_str, "Object %s already exists!\n", bu_vls_cstr(&obot_name));
	    bu_vls_free(&obot_name);
	    return BRLCAD_ERROR;
	}
    }

    /* Adjust settings */
    if (NEAR_EQUAL(settings.max_hole_area_percent, 100, VUNITIZE_TOL)) {
	settings.max_hole_area_percent = 0.0;
    }

    int ret = BRLCAD_OK;
    for (int i = 0; i < argc; i++) {

	gb->dp = db_lookup(gb->gedp->dbip, argv[i], LOOKUP_NOISY);
	if (gb->dp == RT_DIR_NULL)
	    continue;
	int real_flag = (gb->dp->d_addr == RT_DIR_PHONY_ADDR) ? 0 : 1;
	if (!real_flag)
	    continue;
	if (gb->dp->d_major_type != DB5_MAJORTYPE_BRLCAD)
	    continue;
	if (gb->dp->d_minor_type != DB5_MINORTYPE_BRLCAD_BOT)
	    continue;

	gb->solid_name = std::string(argv[i]);

	BU_GET(gb->intern, struct rt_db_internal);
	GED_DB_GET_INTERNAL(gb->gedp, gb->intern, gb->dp, bn_mat_identity, &rt_uniresource, BRLCAD_ERROR);
	RT_CK_DB_INTERNAL(gb->intern);

	struct rt_bot_internal *bot = (struct rt_bot_internal *)(gb->intern->idb_ptr);
	if (bot->mode != RT_BOT_SOLID) {
	    rt_db_free_internal(gb->intern);
	    BU_PUT(gb->intern, struct rt_db_internal);
	    gb->intern = NULL;
	    continue;
	}

	struct rt_bot_internal *mbot = bot_repair(bot, &settings);

	// If we were already manifold, there's nothing to do
	if (mbot && mbot == bot) {
	    rt_db_free_internal(gb->intern);
	    BU_PUT(gb->intern, struct rt_db_internal);
	    gb->intern = NULL;
	    continue;
	}

	// Trying repair and couldn't, it's an error
	if (!mbot) {
	    bu_vls_printf(gb->gedp->ged_result_str, "Unable to repair BoT %s\n", gb->dp->d_namep);
	    rt_db_free_internal(gb->intern);
	    BU_PUT(gb->intern, struct rt_db_internal);
	    gb->intern = NULL;
	    ret = BRLCAD_ERROR;
	    continue;
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
	    rname = bu_vls_cstr(&obot_name);
	    dp = db_diradd(gb->gedp->dbip, rname, RT_DIR_PHONY_ADDR, 0, RT_DIR_SOLID, (void *)&intern.idb_type);
	    if (dp == RT_DIR_NULL) {
		bu_vls_printf(gb->gedp->ged_result_str, "Failed to write out new BoT %s\n", rname);
		rt_db_free_internal(gb->intern);
		BU_PUT(gb->intern, struct rt_db_internal);
		gb->intern = NULL;
		ret = BRLCAD_ERROR;
		continue;
	    }
	}

	if (rt_db_put_internal(dp, gb->gedp->dbip, &intern, &rt_uniresource) < 0) {
	    bu_vls_printf(gb->gedp->ged_result_str, "Failed to write out new BoT %s\n", rname);
	    rt_db_free_internal(gb->intern);
	    BU_PUT(gb->intern, struct rt_db_internal);
	    gb->intern = NULL;
	    ret = BRLCAD_ERROR;
	    continue;
	}

	bu_vls_printf(gb->gedp->ged_result_str, "Repair completed successfully and written to %s\n", rname);

	rt_db_free_internal(gb->intern);
	BU_PUT(gb->intern, struct rt_db_internal);
	gb->intern = NULL;
    }

    bu_vls_free(&obot_name);

    return ret;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

