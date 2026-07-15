/*                         L O D . C P P
 * BRL-CAD
 *
 * Copyright (c) 2008-2026 United States Government as represented by
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
/** @file libged/view/lod.cpp
 *
 * The view lod subcommand.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include <vector>

#include "bu/cmd.h"
#include "bu/cmdschema.h"
#include "bu/hash.h"
#include "bu/str.h"
#include "bu/time.h"
#include "bu/vls.h"
#include "bv/lod.h"

#include "../ged_private.h"
#include "./ged_view.h"

struct ged_view_lod_args {
    int print_help;
};

static const char * const ged_view_lod_actions[] = {
    "0", "1", "csg", "mesh", "cache", "scale", "point_scale",
    "curve_scale", "bot_threshold", NULL
};
static const char * const ged_view_lod_binary_values[] = {"0", "1", NULL};
static const char * const ged_view_lod_cache_actions[] = {"clear", "exists", NULL};
static const char * const ged_view_lod_cache_clear_actions[] = {"all_files", NULL};


static int
ged_view_lod_keyword(const char * const *values, const char *value)
{
    if (!value)
	return 0;
    for (size_t i = 0; values[i]; i++)
	if (BU_STR_EQUAL(values[i], value))
	    return 1;
    return 0;
}


static void
ged_view_lod_validation_result(struct bu_cmd_validate_result *result,
	bu_cmd_validate_state_t state, size_t token, bu_cmd_value_t value_type,
	const char *hint)
{
    bu_cmd_validate_result_clear(result);
    result->state = state;
    result->token_start = token;
    result->token_end = token;
    result->expected = BU_CMD_EXPECT_OPERAND;
    result->completion_type = value_type;
    result->hint = hint;
}


static int
ged_view_lod_prefix_state(const char * const *operands, size_t count,
	struct bu_cmd_validate_result *result, size_t token, int at_end)
{
    const char *action = count ? operands[0] : NULL;

    if (!count) {
	ged_view_lod_validation_result(result, BU_CMD_VALIDATE_VALID, token,
	    BU_CMD_VALUE_KEYWORD, "LoD action may follow");
	bu_cmd_keyword_candidates(result, ged_view_lod_actions, "");
	return 0;
    }
    if (!ged_view_lod_keyword(ged_view_lod_actions, action)) {
	ged_view_lod_validation_result(result, BU_CMD_VALIDATE_INVALID, token,
	    BU_CMD_VALUE_KEYWORD, "known LoD action required");
	bu_cmd_keyword_candidates(result, ged_view_lod_actions, action);
	return 0;
    }
    if (BU_STR_EQUAL(action, "0") || BU_STR_EQUAL(action, "1")) {
	if (count > 1)
	    ged_view_lod_validation_result(result, BU_CMD_VALIDATE_INVALID, token,
		BU_CMD_VALUE_STRING, "LoD enable/disable action takes no operands");
	else
	    ged_view_lod_validation_result(result, BU_CMD_VALIDATE_VALID, token,
		BU_CMD_VALUE_KEYWORD, "LoD enable/disable action");
	return 0;
    }
    if (BU_STR_EQUAL(action, "csg") || BU_STR_EQUAL(action, "mesh")) {
	if (count == 1) {
	    ged_view_lod_validation_result(result, BU_CMD_VALIDATE_VALID, token,
		BU_CMD_VALUE_KEYWORD, "optional LoD enable value");
	    if (at_end)
		bu_cmd_keyword_candidates(result, ged_view_lod_binary_values, "");
	    return 0;
	}
	if (count == 2 && ged_view_lod_keyword(ged_view_lod_binary_values, operands[1])) {
	    ged_view_lod_validation_result(result, BU_CMD_VALIDATE_VALID, token,
		BU_CMD_VALUE_KEYWORD, "LoD enable value");
	    return 0;
	}
	ged_view_lod_validation_result(result, BU_CMD_VALIDATE_INVALID, token,
	    BU_CMD_VALUE_KEYWORD, "LoD enable value must be 0 or 1");
	if (count == 2)
	    bu_cmd_keyword_candidates(result, ged_view_lod_binary_values, operands[1]);
	return 0;
    }
    if (BU_STR_EQUAL(action, "cache")) {
	if (count == 1) {
	    ged_view_lod_validation_result(result, BU_CMD_VALIDATE_VALID, token,
		BU_CMD_VALUE_KEYWORD, "optional cache action");
	    if (at_end)
		bu_cmd_keyword_candidates(result, ged_view_lod_cache_actions, "");
	    return 0;
	}
	if (!ged_view_lod_keyword(ged_view_lod_cache_actions, operands[1])) {
	    ged_view_lod_validation_result(result, BU_CMD_VALIDATE_INVALID, token,
		BU_CMD_VALUE_KEYWORD, "cache action must be clear or exists");
	    bu_cmd_keyword_candidates(result, ged_view_lod_cache_actions, operands[1]);
	    return 0;
	}
	if (BU_STR_EQUAL(operands[1], "exists")) {
	    ged_view_lod_validation_result(result,
		count == 2 ? BU_CMD_VALIDATE_VALID : BU_CMD_VALIDATE_INVALID, token,
		BU_CMD_VALUE_KEYWORD, "cache exists takes no operand");
	    return 0;
	}
	if (count == 2) {
	    ged_view_lod_validation_result(result, BU_CMD_VALIDATE_VALID, token,
		BU_CMD_VALUE_KEYWORD, "optional cache clear scope");
	    if (at_end)
		bu_cmd_keyword_candidates(result, ged_view_lod_cache_clear_actions, "");
	    return 0;
	}
	if (count == 3 && BU_STR_EQUAL(operands[2], "all_files")) {
	    ged_view_lod_validation_result(result, BU_CMD_VALIDATE_VALID, token,
		BU_CMD_VALUE_KEYWORD, "cache clear scope");
	    return 0;
	}
	ged_view_lod_validation_result(result, BU_CMD_VALIDATE_INVALID, token,
	    BU_CMD_VALUE_KEYWORD, "cache clear scope must be all_files");
	if (count == 3)
	    bu_cmd_keyword_candidates(result, ged_view_lod_cache_clear_actions, operands[2]);
	return 0;
    }

    if (count == 1) {
	ged_view_lod_validation_result(result, BU_CMD_VALIDATE_VALID, token,
	    BU_CMD_VALUE_NUMBER, "optional LoD scale value");
	return 0;
    }
    if (BU_STR_EQUAL(action, "bot_threshold")) {
	int threshold = 0;
	if (count == 2 && bu_cmd_integer_from_str(&threshold, operands[1]) && threshold >= 0) {
	    ged_view_lod_validation_result(result, BU_CMD_VALIDATE_VALID, token,
		BU_CMD_VALUE_INTEGER, "nonnegative BoT face threshold");
	    return 0;
	}
	ged_view_lod_validation_result(result, BU_CMD_VALIDATE_INVALID, token,
	    BU_CMD_VALUE_INTEGER, "nonnegative BoT face threshold required");
	return 0;
    }
    fastf_t factor = 0.0;
    if (count == 2 && bu_cmd_number_from_str(&factor, operands[1])) {
	ged_view_lod_validation_result(result, BU_CMD_VALIDATE_VALID, token,
	    BU_CMD_VALUE_NUMBER, "finite LoD scale value");
	return 0;
    }
    ged_view_lod_validation_result(result, BU_CMD_VALIDATE_INVALID, token,
	BU_CMD_VALUE_NUMBER, "finite LoD scale value required");
    return 0;
}


static int
ged_view_lod_validate(const struct bu_cmd_schema *schema, size_t argc,
	const char **argv, size_t cursor_arg, struct bu_cmd_validate_result *result)
{
    struct bu_cmd_schema flat = *schema;
    const char *operands[3] = {NULL, NULL, NULL};
    size_t count = 0;
    size_t i = 0;
    int option_phase = 1;

    flat.validation.custom_validate = NULL;
    if (bu_cmd_schema_validate(&flat, argc, argv, cursor_arg, result) != 0 ||
	result->state == BU_CMD_VALIDATE_INVALID)
	return 0;
    if (bu_cmd_schema_option_present(schema, argc, argv, "help") ||
	(result->expected & BU_CMD_EXPECT_OPTION_ARG) ||
	(cursor_arg < argc && (result->expected & BU_CMD_EXPECT_OPTION)))
	return 0;

    while (i < cursor_arg) {
	int span = 0;
	if (option_phase && BU_STR_EQUAL(argv[i], "--")) {
	    option_phase = 0;
	    i++;
	    continue;
	}
	if (option_phase)
	    span = bu_cmd_schema_option_span(schema, argc - i, argv + i);
	if (span > 0) {
	    i += (size_t)span;
	    continue;
	}
	if (count < 3)
	    operands[count++] = argv[i];
	i++;
    }

    if (cursor_arg < argc) {
	const char *current = argv[cursor_arg];
	if (count < 3)
	    operands[count] = current;
	if (!count) {
	    ged_view_lod_validation_result(result,
		ged_view_lod_keyword(ged_view_lod_actions, current) ? BU_CMD_VALIDATE_VALID :
		BU_CMD_VALIDATE_INVALID, cursor_arg, BU_CMD_VALUE_KEYWORD,
		"LoD action required");
	    bu_cmd_keyword_candidates(result, ged_view_lod_actions, current);
	    return 0;
	}
	const char *action = operands[0];
	if (count == 1 && (BU_STR_EQUAL(action, "csg") || BU_STR_EQUAL(action, "mesh"))) {
	    ged_view_lod_validation_result(result,
		ged_view_lod_keyword(ged_view_lod_binary_values, current) ? BU_CMD_VALIDATE_VALID :
		BU_CMD_VALIDATE_INVALID, cursor_arg, BU_CMD_VALUE_KEYWORD,
		"LoD enable value must be 0 or 1");
	    bu_cmd_keyword_candidates(result, ged_view_lod_binary_values, current);
	    return 0;
	}
	if (count == 1 && BU_STR_EQUAL(action, "cache")) {
	    ged_view_lod_validation_result(result,
		ged_view_lod_keyword(ged_view_lod_cache_actions, current) ? BU_CMD_VALIDATE_VALID :
		BU_CMD_VALIDATE_INVALID, cursor_arg, BU_CMD_VALUE_KEYWORD,
		"cache action must be clear or exists");
	    bu_cmd_keyword_candidates(result, ged_view_lod_cache_actions, current);
	    return 0;
	}
	if (count == 2 && BU_STR_EQUAL(action, "cache") &&
	    BU_STR_EQUAL(operands[1], "clear")) {
	    ged_view_lod_validation_result(result,
		BU_STR_EQUAL(current, "all_files") ? BU_CMD_VALIDATE_VALID : BU_CMD_VALIDATE_INVALID,
		cursor_arg, BU_CMD_VALUE_KEYWORD, "cache clear scope must be all_files");
	    bu_cmd_keyword_candidates(result, ged_view_lod_cache_clear_actions, current);
	    return 0;
	}
	if (count == 1 && (BU_STR_EQUAL(action, "scale") ||
	    BU_STR_EQUAL(action, "point_scale") || BU_STR_EQUAL(action, "curve_scale"))) {
	    fastf_t factor = 0.0;
	    ged_view_lod_validation_result(result,
		bu_cmd_number_from_str(&factor, current) ? BU_CMD_VALIDATE_VALID : BU_CMD_VALIDATE_INVALID,
		cursor_arg, BU_CMD_VALUE_NUMBER, "finite LoD scale value required");
	    return 0;
	}
	if (count == 1 && BU_STR_EQUAL(action, "bot_threshold")) {
	    int threshold = 0;
	    ged_view_lod_validation_result(result,
		bu_cmd_integer_from_str(&threshold, current) && threshold >= 0 ?
		BU_CMD_VALIDATE_VALID : BU_CMD_VALIDATE_INVALID, cursor_arg,
		BU_CMD_VALUE_INTEGER, "nonnegative BoT face threshold required");
	    return 0;
	}
	ged_view_lod_validation_result(result, BU_CMD_VALIDATE_INVALID, cursor_arg,
	    BU_CMD_VALUE_STRING, "too many LoD operands");
	return 0;
    }

    return ged_view_lod_prefix_state(operands, count, result, cursor_arg, 1);
}


static const struct bu_cmd_option ged_view_lod_options[] = {
    BU_CMD_FLAG("h", "help", struct ged_view_lod_args, print_help, "Print help and exit"),
    BU_CMD_OPTION_NULL
};
static const struct bu_cmd_operand ged_view_lod_operands[] = {
    BU_CMD_OPERAND("lod_action", BU_CMD_VALUE_RAW, 0, 3,
	"LoD action and optional values", NULL),
    BU_CMD_OPERAND_NULL
};
extern "C" GED_EXPORT const struct bu_cmd_schema ged_view_lod_schema = {
    "lod", "Manage level-of-detail drawing settings", ged_view_lod_options,
    ged_view_lod_operands, BU_CMD_PARSE_INTERSPERSED,
    BU_CMD_SCHEMA_CONSTRAINTS(ged_view_lod_validate, NULL)
};


extern "C" GED_EXPORT char *
ged_view_lod_grammar_json(void)
{
    struct bu_vls out = BU_VLS_INIT_ZERO;

    /* The action vocabulary is owned by the same static sets used by the
     * executable validator.  The generic flat-schema JSON can describe the
     * raw tail, but it cannot express this conditional mini-language. */
    bu_vls_strcat(&out,
	"{\"name\":\"lod\",\"kind\":\"native_action_grammar\","
	"\"help\":\"Manage level-of-detail drawing settings\",\"actions\":[");
    for (size_t i = 0; ged_view_lod_actions[i]; i++) {
	if (i)
	    bu_vls_putc(&out, ',');
	bu_vls_printf(&out, "{\"name\":\"%s\"", ged_view_lod_actions[i]);
	if (BU_STR_EQUAL(ged_view_lod_actions[i], "csg") ||
	    BU_STR_EQUAL(ged_view_lod_actions[i], "mesh")) {
	    bu_vls_strcat(&out, ",\"values\":[\"0\",\"1\"]");
	} else if (BU_STR_EQUAL(ged_view_lod_actions[i], "cache")) {
	    bu_vls_strcat(&out,
		",\"actions\":[{\"name\":\"clear\",\"values\":[\"all_files\"]},"
		"{\"name\":\"exists\"}]");
	} else if (BU_STR_EQUAL(ged_view_lod_actions[i], "scale") ||
	    BU_STR_EQUAL(ged_view_lod_actions[i], "point_scale") ||
	    BU_STR_EQUAL(ged_view_lod_actions[i], "curve_scale")) {
	    bu_vls_strcat(&out, ",\"value_type\":\"number\"");
	} else if (BU_STR_EQUAL(ged_view_lod_actions[i], "bot_threshold")) {
	    bu_vls_strcat(&out, ",\"value_type\":\"nonnegative_integer\"");
	}
	bu_vls_putc(&out, '}');
    }
    bu_vls_strcat(&out, "]}");
    char *json = bu_vls_strdup(&out);
    bu_vls_free(&out);
    return json;
}


static void
ged_view_lod_usage(struct ged *gedp)
{
    char *option_help = bu_cmd_schema_describe(&ged_view_lod_schema);

    bu_vls_printf(gedp->ged_result_str,
	"Usage:\n"
	"view lod [0|1]\n"
	"view lod csg|mesh [0|1]\n"
	"view lod cache [clear [all_files]|exists]\n"
	"view lod scale|point_scale|curve_scale [factor]\n"
	"view lod bot_threshold [face_count]\n");
    if (option_help) {
	bu_vls_printf(gedp->ged_result_str, "Options:\n%s\n", option_help);
	bu_free(option_help, "view lod native option help");
    }
}

extern "C" GED_EXPORT int
ged_view_lod_core(struct ged *gedp, struct bview *view, int argc, const char **argv)
{
    struct ged_view_lod_args args = {0};
    std::vector<const char *> pargv;
    int operand_index;
    int action_argc;
    struct bview *gvp;

    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    pargv.assign(argv, argv + argc);
    operand_index = bu_cmd_schema_parse_complete(&ged_view_lod_schema, &args,
	gedp->ged_result_str, argc - 1, pargv.data() + 1);
    if (operand_index < 0) {
	ged_view_lod_usage(gedp);
	return BRLCAD_ERROR;
    }
    if (args.print_help) {
	ged_view_lod_usage(gedp);
	return GED_HELP;
    }
    action_argc = argc - 1 - operand_index;
    argc = action_argc;
    argv = pargv.data() + 1 + operand_index;

    gvp = view;
    if (gvp == NULL) {
	bu_vls_printf(gedp->ged_result_str, "no current view defined\n");
	return BRLCAD_ERROR;
    }

    /* Print current state if no args are supplied */
    if (argc == 0) {
	bu_vls_printf(gedp->ged_result_str, "enabled(mesh/csg): %d/%d\n", gvp->gv_s->adaptive_plot_mesh, gvp->gv_s->adaptive_plot_csg);
	bu_vls_printf(gedp->ged_result_str, "scale: %g\n", gvp->gv_s->lod_scale);
	bu_vls_printf(gedp->ged_result_str, "point_scale: %g\n", gvp->gv_s->point_scale);
	bu_vls_printf(gedp->ged_result_str, "curve_scale: %g\n", gvp->gv_s->curve_scale);
	bu_vls_printf(gedp->ged_result_str, "bot_threshold: %zd\n", gvp->gv_s->bot_threshold);
	return BRLCAD_OK;
    }

    if (BU_STR_EQUIV(argv[0], "1")) {
	if (!gvp->gv_s->adaptive_plot_mesh || !gvp->gv_s->adaptive_plot_csg) {
	    gvp->gv_s->adaptive_plot_csg = 1;
	    gvp->gv_s->adaptive_plot_mesh = 1;
	    int rac = 1;
	    const char *rav[1] = {"redraw"};
	    ged_exec_redraw(gedp, rac, (const char **)rav);
	}
	return BRLCAD_OK;
    }
    if (BU_STR_EQUIV(argv[0], "0")) {
	if (gvp->gv_s->adaptive_plot_mesh || gvp->gv_s->adaptive_plot_csg) {
	    gvp->gv_s->adaptive_plot_csg = 0;
	    gvp->gv_s->adaptive_plot_mesh = 0;
	    int rac = 1;
	    const char *rav[1] = {"redraw"};
	    ged_exec_redraw(gedp, rac, (const char **)rav);
	}
	return BRLCAD_OK;
    }

    if (BU_STR_EQUAL(argv[0], "csg")) {
	if (argc == 1) {
	    bu_vls_printf(gedp->ged_result_str, "%d\n", gvp->gv_s->adaptive_plot_csg);
	    return BRLCAD_OK;
	}
	if (BU_STR_EQUAL(argv[1], "1")) {
	    if (!gvp->gv_s->adaptive_plot_csg) {
		gvp->gv_s->adaptive_plot_csg = 1;
		int rac = 1;
		const char *rav[1] = {"redraw"};
		ged_exec_redraw(gedp, rac, (const char **)rav);
	    }
	    return BRLCAD_OK;
	}
	if (BU_STR_EQUAL(argv[1], "0")) {
	    if (gvp->gv_s->adaptive_plot_csg) {
		gvp->gv_s->adaptive_plot_csg = 0;
		int rac = 1;
		const char *rav[1] = {"redraw"};
		ged_exec_redraw(gedp, rac, (const char **)rav);
	    }
	    return BRLCAD_OK;
	}
	bu_vls_printf(gedp->ged_result_str, "Error - invalid arg: \"%s\".  Valid args are 0 or 1", argv[1]);
	return BRLCAD_ERROR;
    }

    if (BU_STR_EQUAL(argv[0], "mesh")) {
	if (argc == 1) {
	    bu_vls_printf(gedp->ged_result_str, "%d\n", gvp->gv_s->adaptive_plot_mesh);
	    return BRLCAD_OK;
	}
	if (BU_STR_EQUAL(argv[1], "1")) {
	    if (!gvp->gv_s->adaptive_plot_mesh) {
		gvp->gv_s->adaptive_plot_mesh = 1;
		int rac = 1;
		const char *rav[1] = {"redraw"};
		ged_exec_redraw(gedp, rac, (const char **)rav);
	    }
	    return BRLCAD_OK;
	}
	if (BU_STR_EQUAL(argv[1], "0")) {
	    if (gvp->gv_s->adaptive_plot_mesh) {
		gvp->gv_s->adaptive_plot_mesh = 0;
		int rac = 1;
		const char *rav[1] = {"redraw"};
		ged_exec_redraw(gedp, rac, (const char **)rav);
	    }
	    return BRLCAD_OK;
	}
	bu_vls_printf(gedp->ged_result_str, "Error - invalid arg: \"%s\".  Valid args are 0 or 1", argv[1]);
	return BRLCAD_ERROR;
    }

    if (BU_STR_EQUAL(argv[0], "cache")) {
	if (argc == 1) {
	    int64_t elapsedtime = bu_gettime();

	    if (!gedp || !gedp->dbip)
		return BRLCAD_ERROR;

	    struct rt_wdb *wdbp = wdb_dbopen(gedp->dbip, RT_WDB_TYPE_DB_DEFAULT);

	    // Clear any old cache in memory
	    bv_mesh_lod_clear_cache(gedp->ged_lod, 0);

	    int done = 0;
	    int total = 0;
	    struct directory *dp;
	    FOR_ALL_DIRECTORY_START(dp, gedp->dbip)
		if (dp->d_addr == RT_DIR_PHONY_ADDR)
		    continue;
		if (dp->d_minor_type == DB5_MINORTYPE_BRLCAD_BOT)
		    total++;
		if (dp->d_minor_type == DB5_MINORTYPE_BRLCAD_BREP)
		    total++;
	    FOR_ALL_DIRECTORY_END;

	    FOR_ALL_DIRECTORY_START(dp, gedp->dbip)
		    if (dp->d_addr == RT_DIR_PHONY_ADDR)
			continue;

		    unsigned long long key = 0;

		    // No need to open up the internal unless it's a BoT or a BRep
		    if (dp->d_minor_type == DB5_MINORTYPE_BRLCAD_BOT) {
			struct rt_db_internal dbintern;
			RT_DB_INTERNAL_INIT(&dbintern);
			struct rt_db_internal *ip = &dbintern;
			int ret = rt_db_get_internal(ip, dp, gedp->dbip, NULL);
			if (ret < 0)
			    continue;

			if (ip->idb_minor_type != DB5_MINORTYPE_BRLCAD_BOT) {
			    rt_db_free_internal(&dbintern);
			    continue;
			}
			done++;
			struct bu_vls pname = BU_VLS_INIT_ZERO;
			bu_log("Caching BoT %s (%d of %d)\n", dp->d_namep, done, total);
			bu_vls_free(&pname);
			struct rt_bot_internal *bot = (struct rt_bot_internal *)ip->idb_ptr;
			RT_BOT_CK_MAGIC(bot);
			key = bv_mesh_lod_cache(gedp->ged_lod, (const point_t *)bot->vertices, bot->num_vertices, NULL, bot->faces, bot->num_faces, 0, 0.66);
			if (key)
			    bv_mesh_lod_key_put(gedp->ged_lod, dp->d_namep, key);
			rt_db_free_internal(&dbintern);
		    }

		    if (dp->d_minor_type == DB5_MINORTYPE_BRLCAD_BREP) {
			struct bu_external ext = BU_EXTERNAL_INIT_ZERO;
			if (db_get_external(&ext, dp, gedp->dbip))
			    continue;
			key = bu_data_hash((void *)ext.ext_buf,  ext.ext_nbytes);
			bu_free_external(&ext);
			if (!key)
			    continue;

			struct rt_db_internal dbintern;
			RT_DB_INTERNAL_INIT(&dbintern);
			struct rt_db_internal *ip = &dbintern;
			int ret = rt_db_get_internal(ip, dp, gedp->dbip, NULL);
			if (ret < 0)
			    continue;

			if (ip->idb_minor_type != DB5_MINORTYPE_BRLCAD_BREP) {
			    rt_db_free_internal(&dbintern);
			    continue;
			}
			done++;
			struct bu_vls pname = BU_VLS_INIT_ZERO;
			bu_log("Caching BRep %s (%d of %d)\n", dp->d_namep, done, total);
			bu_vls_free(&pname);
			struct rt_brep_internal *bi = (struct rt_brep_internal *)ip->idb_ptr;
			RT_BREP_CK_MAGIC(bi);

			// Unlike a BoT, which has the mesh data already, we need to generate the
			// mesh from the brep
			int *faces = NULL;
			int face_cnt = 0;
			vect_t *normals = NULL;
			point_t *pnts = NULL;
			int pnt_cnt = 0;
			struct bn_tol *tol = &wdbp->wdb_tol;
			struct bg_tess_tol *ttol = &wdbp->wdb_ttol;

			int bret = brep_cdt_fast(&faces, &face_cnt, &normals, &pnts, &pnt_cnt, bi->brep, -1, ttol, tol);
			if (bret != BRLCAD_OK) {
			    bu_free(faces, "faces");
			    bu_free(normals, "normals");
			    bu_free(pnts, "pnts");
			    rt_db_free_internal(&dbintern);
			    continue;
			}

			// Because we won't have the internal data to use for a full detail scenario, we set the ratio
			// to 1 rather than .66 for breps...
			key = bv_mesh_lod_cache(gedp->ged_lod, (const point_t *)pnts, pnt_cnt, normals, faces, face_cnt, key, 1);

			if (key)
			    bv_mesh_lod_key_put(gedp->ged_lod, dp->d_namep, key);

			rt_db_free_internal(&dbintern);
			bu_free(faces, "faces");
			bu_free(normals, "normals");
			bu_free(pnts, "pnts");
		    }
	    FOR_ALL_DIRECTORY_END;

	    elapsedtime = bu_gettime() - elapsedtime;
	    {
		int seconds = elapsedtime / 1000000;
		int minutes = seconds / 60;
		int hours = minutes / 60;

		minutes = minutes % 60;
		seconds = seconds %60;
		bu_vls_printf(gedp->ged_result_str, "Caching complete (Elapsed time: %02d:%02d:%02d)\n", hours, minutes, seconds);
	    }
	    return BRLCAD_OK;
	}
	if (argc == 2) {
	    if (BU_STR_EQUAL(argv[1], "clear")) {
		bv_mesh_lod_clear_cache(gedp->ged_lod, 0);
		return BRLCAD_OK;
	    } else if (BU_STR_EQUAL(argv[1], "exists")) {
		struct directory *dp;
		FOR_ALL_DIRECTORY_START(dp, gedp->dbip)
		    if (dp->d_addr == RT_DIR_PHONY_ADDR)
			continue;
		    // checking both BoTs and BREPs
		    if ((dp->d_minor_type == DB5_MINORTYPE_BRLCAD_BOT) ||
			(dp->d_minor_type == DB5_MINORTYPE_BRLCAD_BREP)) {
			unsigned long long key = bv_mesh_lod_key_get(gedp->ged_lod, dp->d_namep);
			if (!key) {
			    return BRLCAD_ERROR;
			}
		    }
		FOR_ALL_DIRECTORY_END;
		return BRLCAD_OK;
	    }
	}
	if (argc == 3) {
	    if (BU_STR_EQUAL(argv[1], "clear") && BU_STR_EQUAL(argv[2], "all_files")) {
		bv_mesh_lod_clear_cache(NULL, 0);
		return BRLCAD_OK;
	    }
	}
	bu_vls_printf(gedp->ged_result_str, "unknown argument to cache: %s\n", argv[1]);
	return BRLCAD_ERROR;
    }

    if (BU_STR_EQUAL(argv[0], "scale")) {
	if (argc == 1) {
	    bu_vls_printf(gedp->ged_result_str, "%g\n", gvp->gv_s->lod_scale);
	    return BRLCAD_OK;
	}
	fastf_t scale = 1.0;
	if (!bu_cmd_number_from_str(&scale, argv[1])) {
	    bu_vls_printf(gedp->ged_result_str, "unknown argument to point_scale: %s\n", argv[1]);
	    return BRLCAD_ERROR;
	}
	gvp->gv_s->lod_scale = scale;
	return BRLCAD_OK;
    }

    if (BU_STR_EQUAL(argv[0], "point_scale")) {
	if (argc == 1) {
	    bu_vls_printf(gedp->ged_result_str, "%g\n", gvp->gv_s->point_scale);
	    return BRLCAD_OK;
	}
	fastf_t scale = 1.0;
	if (!bu_cmd_number_from_str(&scale, argv[1])) {
	    bu_vls_printf(gedp->ged_result_str, "unknown argument to point_scale: %s\n", argv[1]);
	    return BRLCAD_ERROR;
	}
	gvp->gv_s->point_scale = scale;
	return BRLCAD_OK;
    }

    if (BU_STR_EQUAL(argv[0], "curve_scale")) {
	if (argc == 1) {
	    bu_vls_printf(gedp->ged_result_str, "%g\n", gvp->gv_s->curve_scale);
	    return BRLCAD_OK;
	}
	fastf_t scale = 1.0;
	if (!bu_cmd_number_from_str(&scale, argv[1])) {
	    bu_vls_printf(gedp->ged_result_str, "unknown argument to curve_scale: %s\n", argv[1]);
	    return BRLCAD_ERROR;
	}
	gvp->gv_s->curve_scale = scale;
	return BRLCAD_OK;
    }

    if (BU_STR_EQUAL(argv[0], "bot_threshold")) {
	if (argc == 1) {
	    bu_vls_printf(gedp->ged_result_str, "%zd\n", gvp->gv_s->bot_threshold);
	    return BRLCAD_OK;
	}
	int bcnt = 0;
	if (!bu_cmd_integer_from_str(&bcnt, argv[1]) || bcnt < 0) {
	    bu_vls_printf(gedp->ged_result_str, "unknown argument to bot_threshold: %s\n", argv[1]);
	    return BRLCAD_ERROR;
	}
	gvp->gv_s->bot_threshold = bcnt;
	return BRLCAD_OK;

    }

    bu_vls_printf(gedp->ged_result_str, "unknown subcommand: %s\n", argv[0]);
    return BRLCAD_ERROR;
}


int
_view_cmd_lod(void *bs, int argc, const char **argv)
{
    struct _ged_view_info *gd = (struct _ged_view_info *)bs;
    const char *usage_string = "view [options] lod [subcommand] [vals]";
    const char *purpose_string = "manage Level of Detail drawing settings";

    if (_view_cmd_msgs(bs, argc, argv, usage_string, purpose_string))
	return BRLCAD_OK;
    return ged_view_lod_core(gd->gedp, gd->cv, argc, argv);
}



// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
