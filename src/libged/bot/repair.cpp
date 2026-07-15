/*                     R E P A I R . C P P
 * BRL-CAD
 *
 * Copyright (c) 2020-2026 United States Government as represented by
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
 *
 * TODO - need to do some post repair checks to validate the repair - recent
 * use cases have shown some gnarly failure modes where the result is manifold
 * and not particularly different visually, but resulted in a massive volume
 * change and odd raytracing result due to geometry sort of turning "inside out".
 *
 * Checks we need:
 *
 * BoT raytracing sanity check - if we're manifold but can't pass the lint BoT
 * raytracing checks (observed case in the wild) then we need to reject the repair.
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
#include "bu/cmdschema.h"
#include "bu/color.h"
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

struct bot_repair_args {
    int print_help;
    fastf_t max_hole_area_percent;
    fastf_t max_hole_area;
    struct bu_vls output_name;
};

static int
bot_repair_percent_validate(struct bu_vls *msg, const char *arg)
{
    fastf_t value;

    if (bu_cmd_number_from_str(&value, arg) && value >= 0.0 && value <= 100.0)
	return 0;
    if (msg)
	bu_vls_printf(msg, "maximum hole percentage must be in [0,100]: %s\n",
		arg ? arg : "");
    return -1;
}

static int
bot_repair_schema_validate(const struct bu_cmd_schema *schema, size_t argc,
	const char **argv, size_t cursor_arg,
	struct bu_cmd_validate_result *result)
{
    struct bu_cmd_schema flat = *schema;
    int help = bu_cmd_schema_option_present(schema, argc, argv, "help");
    int ret;

    flat.validation.custom_validate = NULL;
    if (help)
	flat.operands = NULL;
    ret = bu_cmd_schema_validate(&flat, argc, argv, cursor_arg, result);
    if (ret || result->state != BU_CMD_VALIDATE_VALID || help)
	return ret;

    /* Do not replace an edited --output-name argument with completion for the
     * later input BoT.  Whole-command cardinality is evaluated once the
     * cursor has moved to the next word. */
    if (cursor_arg < argc)
	return 0;

    if (bu_cmd_schema_option_present(schema, argc, argv, "output-name") &&
	bu_cmd_schema_operand_count(schema, argc, argv) != 1) {
	size_t count = bu_cmd_schema_operand_count(schema, argc, argv);
	bu_cmd_validate_result_clear(result);
	result->state = count < 1 ? BU_CMD_VALIDATE_INCOMPLETE : BU_CMD_VALIDATE_INVALID;
	result->token_start = cursor_arg < argc ? cursor_arg : argc;
	result->token_end = result->token_start;
	result->expected = BU_CMD_EXPECT_OPERAND;
	result->completion_type = BU_CMD_VALUE_DB_OBJECT;
	result->semantic_provider = "ged.db_object";
	result->hint = "--output-name requires exactly one input BoT";
    }
    return 0;
}

static const struct bu_cmd_option bot_repair_options[] = {
    BU_CMD_FLAG("h", "help", struct bot_repair_args, print_help,
	"Print command help"),
    BU_CMD_NUMBER_VALIDATE("p", "max-hole-percent", struct bot_repair_args,
	max_hole_area_percent, bot_repair_percent_validate, "percent",
	"Maximum hole area as a percentage of mesh area"),
    BU_CMD_NONNEGATIVE_NUMBER("a", "max-hole-area", struct bot_repair_args,
	max_hole_area, "area", "Maximum hole area in mm"),
    BU_CMD_VLS_APPEND("o", "output-name", struct bot_repair_args, output_name,
	"name", "Output object name instead of overwriting the input"),
    BU_CMD_OPTION_NULL
};
static const struct bu_cmd_operand bot_repair_operands[] = {
    BU_CMD_OPERAND("input_bot", BU_CMD_VALUE_DB_OBJECT, 1, BU_CMD_COUNT_UNLIMITED,
	"Input BoT object", "ged.db_object"),
    BU_CMD_OPERAND_NULL
};
extern "C" const struct bu_cmd_schema ged_bot_repair_subcommand_schema = {
    "repair", "Repair manifold BoT geometry", bot_repair_options,
    bot_repair_operands, BU_CMD_PARSE_INTERSPERSED,
    BU_CMD_SCHEMA_CONSTRAINTS(bot_repair_schema_validate, NULL)
};

static void
repair_usage(struct bu_vls *str, const char *cmd)
{
    char *option_help = bu_cmd_schema_describe(&ged_bot_repair_subcommand_schema);
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

    struct bot_repair_args args = {0, 5.0, 0.0, BU_VLS_INIT_ZERO};
    int operand_count;
    int operand_index;
    const char **operands;
    int in_place_repair;
    struct rt_bot_repair_info settings = RT_BOT_REPAIR_INFO_INIT;

    if (!argc) {
	repair_usage(gb->gedp->ged_result_str, "bot repair");
	bu_vls_free(&args.output_name);
	return GED_HELP;
    }

    operand_index = bu_cmd_schema_parse_complete(&ged_bot_repair_subcommand_schema, &args,
	gb->gedp->ged_result_str, argc, argv);
    if (operand_index < 0) {
	bu_vls_free(&args.output_name);
	return BRLCAD_ERROR;
	}
    operand_count = argc - operand_index;
    operands = argv + operand_index;
    if (args.print_help) {
	repair_usage(gb->gedp->ged_result_str, "bot repair");
	bu_vls_free(&args.output_name);
	return GED_HELP;
	}

	settings.max_hole_area_percent = args.max_hole_area_percent;
	settings.max_hole_area = args.max_hole_area;
	in_place_repair = 1;
    if (bu_vls_strlen(&args.output_name))
	in_place_repair = 0;

    if (!in_place_repair) {
	if (operand_count != 1) {
	    bu_vls_printf(gb->gedp->ged_result_str, "When specifying an output object name, only one input at a time may be processed.");
	    bu_vls_free(&args.output_name);
	    return BRLCAD_ERROR;
	}
	if (db_lookup(gb->gedp->dbip, bu_vls_cstr(&args.output_name), LOOKUP_QUIET) != RT_DIR_NULL) {
	    bu_vls_printf(gb->gedp->ged_result_str, "Object %s already exists!\n", bu_vls_cstr(&args.output_name));
	    bu_vls_free(&args.output_name);
	    return BRLCAD_ERROR;
	}
    }

    /* Adjust settings */
    if (NEAR_EQUAL(settings.max_hole_area_percent, 100, VUNITIZE_TOL)) {
	settings.max_hole_area_percent = 0.0;
    }

    int ret = BRLCAD_OK;
    for (int i = 0; i < operand_count; i++) {

	gb->dp = db_lookup(gb->gedp->dbip, operands[i], LOOKUP_NOISY);
	if (gb->dp == RT_DIR_NULL)
	    continue;
	int real_flag = (gb->dp->d_addr == RT_DIR_PHONY_ADDR) ? 0 : 1;
	if (!real_flag)
	    continue;
	if (gb->dp->d_major_type != DB5_MAJORTYPE_BRLCAD)
	    continue;
	if (gb->dp->d_minor_type != DB5_MINORTYPE_BRLCAD_BOT)
	    continue;

	gb->solid_name = std::string(operands[i]);

	BU_GET(gb->intern, struct rt_db_internal);
	GED_DB_GET_INTERN(gb->gedp, gb->intern, gb->dp, bn_mat_identity, BRLCAD_ERROR);
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
	    rname = bu_vls_cstr(&args.output_name);
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

	if (rt_db_put_internal(dp, gb->gedp->dbip, &intern) < 0) {
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

    bu_vls_free(&args.output_name);

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
