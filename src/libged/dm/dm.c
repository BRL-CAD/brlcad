/*                            D M . C
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
/** @file libged/dm.c
 *
 * The dm and screengrab commands.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include "bu/cmdschema.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "bu/vls.h"
#include "dm.h"

#include "../ged_private.h"

#define DM_MAX_TRIES 100

#ifndef COMMA
#  define COMMA ','
#endif

struct _ged_dm_info {
    struct ged *gedp;
    long verbosity;
};

struct dm_root_args {
    int print_help;
    long verbosity;
};

struct dm_target_args {
    struct bu_vls dm_name;
};

struct dm_attach_args {
    struct bu_vls dm_name;
    struct bu_vls view_name;
};

static int dm_bg_schema_validate(const struct bu_cmd_schema *schema,
    size_t argc, const char **argv, size_t cursor_arg,
    struct bu_cmd_validate_result *result);

static const struct bu_cmd_option dm_root_options[] = {
    BU_CMD_FLAG("h", "help", struct dm_root_args, print_help,
	"Print command help"),
    BU_CMD_ALIAS_SHORT("?", "help", 1),
    BU_CMD_COUNTING_LONG_FLAG("v", "verbose", struct dm_root_args, verbosity,
	"Increase output detail"),
    BU_CMD_OPTION_NULL
};
static const struct bu_cmd_option dm_target_options[] = {
    BU_CMD_VLS_APPEND("d", "dm", struct dm_target_args, dm_name, "name",
	"Display manager instance"),
    BU_CMD_OPTION_NULL
};
static const struct bu_cmd_option dm_attach_options[] = {
    BU_CMD_VLS_APPEND("d", "dm", struct dm_attach_args, dm_name, "name",
	"New display manager name"),
    BU_CMD_VLS_APPEND("V", "view", struct dm_attach_args, view_name, "name",
	"View to associate with the display manager"),
    BU_CMD_OPTION_NULL
};
static const struct bu_cmd_operand dm_bg_operands[] = {
    BU_CMD_OPERAND("color", BU_CMD_VALUE_RAW, 0, 6,
	"One color, or two colors for a gradient", NULL),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_operand dm_debug_operands[] = {
    BU_CMD_OPERAND("level", BU_CMD_VALUE_INTEGER, 0, 1,
	"Optional debug level", NULL),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_operand dm_name_operands[] = {
    BU_CMD_OPERAND("name", BU_CMD_VALUE_STRING, 0, 1,
	"Optional display manager name", NULL),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_operand dm_vars_operands[] = {
    BU_CMD_OPERAND("variable", BU_CMD_VALUE_STRING, 0, BU_CMD_COUNT_UNLIMITED,
	"Display manager variable name", NULL),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_operand dm_set_operands[] = {
    BU_CMD_OPERAND("key_value", BU_CMD_VALUE_STRING, 0, 2,
	"Optional variable name and value", NULL),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_operand dm_attach_operands[] = {
    BU_CMD_OPERAND("type", BU_CMD_VALUE_STRING, 1, 1,
	"Display manager type", NULL),
    BU_CMD_OPERAND("name", BU_CMD_VALUE_STRING, 0, 1,
	"Optional display manager name", NULL),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_schema dm_root_schema = {
    "dm", "Manage display manager instances", dm_root_options, NULL,
    BU_CMD_PARSE_OPTIONS_FIRST, BU_CMD_SCHEMA_CONSTRAINTS(NULL, NULL)
};
static const struct bu_cmd_schema dm_attach_schema = {
    "attach", "Create a display manager", dm_attach_options, dm_attach_operands,
    BU_CMD_PARSE_INTERSPERSED, BU_CMD_SCHEMA_CONSTRAINTS(NULL, NULL)
};
static const struct bu_cmd_schema dm_bg_schema = {
    "bg", "Query or set the background color", NULL, dm_bg_operands,
    BU_CMD_PARSE_STOP_AT_FIRST_OPERAND, BU_CMD_SCHEMA_CONSTRAINTS(dm_bg_schema_validate, NULL)
};
static const struct bu_cmd_schema dm_debug_schema = {
    "debug", "Query or set the debug level", NULL, dm_debug_operands,
    BU_CMD_PARSE_STOP_AT_FIRST_OPERAND, BU_CMD_SCHEMA_CONSTRAINTS(NULL, NULL)
};
static const struct bu_cmd_schema dm_get_schema = {
    "get", "Query display manager variables", dm_target_options, dm_vars_operands,
    BU_CMD_PARSE_INTERSPERSED, BU_CMD_SCHEMA_CONSTRAINTS(NULL, NULL)
};
static const struct bu_cmd_schema dm_height_schema = {
    "height", "Report display manager height", NULL, dm_name_operands,
    BU_CMD_PARSE_STOP_AT_FIRST_OPERAND, BU_CMD_SCHEMA_CONSTRAINTS(NULL, NULL)
};
static const struct bu_cmd_schema dm_initmsg_schema = {
    "initmsg", "Report display manager initialization messages", NULL, NULL,
    BU_CMD_PARSE_STOP_AT_FIRST_OPERAND, BU_CMD_SCHEMA_CONSTRAINTS(NULL, NULL)
};
static const struct bu_cmd_schema dm_list_schema = {
    "list", "List display manager instances", NULL, NULL,
    BU_CMD_PARSE_STOP_AT_FIRST_OPERAND, BU_CMD_SCHEMA_CONSTRAINTS(NULL, NULL)
};
static const struct bu_cmd_schema dm_set_schema = {
    "set", "Query or set display manager variables", dm_target_options, dm_set_operands,
    BU_CMD_PARSE_INTERSPERSED, BU_CMD_SCHEMA_CONSTRAINTS(NULL, NULL)
};
static const struct bu_cmd_schema dm_type_schema = {
    "type", "Report a display manager type", NULL, dm_name_operands,
    BU_CMD_PARSE_STOP_AT_FIRST_OPERAND, BU_CMD_SCHEMA_CONSTRAINTS(NULL, NULL)
};
static const struct bu_cmd_schema dm_types_schema = {
    "types", "List available display manager types", NULL, NULL,
    BU_CMD_PARSE_STOP_AT_FIRST_OPERAND, BU_CMD_SCHEMA_CONSTRAINTS(NULL, NULL)
};
static const struct bu_cmd_schema dm_width_schema = {
    "width", "Report display manager width", NULL, dm_name_operands,
    BU_CMD_PARSE_STOP_AT_FIRST_OPERAND, BU_CMD_SCHEMA_CONSTRAINTS(NULL, NULL)
};

static void
_dm_cmd_during_clbk(struct _ged_dm_info *gd, int argc, const char **argv)
{
    if (!gd || !gd->gedp || argc < 1 || !argv)
        return;

    bu_clbk_t clbk = NULL;
    void *u2 = NULL;
    if (ged_clbk_get(&clbk, &u2, gd->gedp, "dm", BU_CLBK_DURING) != BRLCAD_OK || !clbk)
        return;

    const char *dbg = getenv("GED_DM_DURING_DEBUG");
    if (dbg && BU_STR_EQUAL(dbg, "1")) {
        bu_log("ged dm during callback: ");
        for (int i = 0; i < argc; i++) {
            bu_log("%s%s", argv[i], (i + 1 < argc) ? " " : "\n");
        }
    }

    int cbret = ged_clbk_exec(gd->gedp->ged_result_str, gd->gedp, GED_CMD_RECURSION_LIMIT, clbk, argc, argv, (void *)gd->gedp, u2);
    if (cbret != BRLCAD_OK) {
        bu_vls_printf(gd->gedp->ged_result_str, "\nwarning: dm during callback returned %d", cbret);
    }
}

static int
_ged_dm_log_to_vls(void *data, void *str)
{
    struct bu_vls *v = (struct bu_vls *)data;
    if (!v || !str)
        return 0;
    bu_vls_printf(v, "%s", (const char *)str);
    return 0;
}

static int
dm_bg_colors_from_argv(int argc, const char **argv, struct bu_color *first,
	struct bu_color *second)
{
    int first_count;
    int second_count;

    if (!argc)
	return 0;
    first_count = bu_cmd_color_from_argv(first, (size_t)argc,
	(const char * const *)argv);
    if (first_count <= 0)
	return -1;
    if (first_count == argc) {
	if (second)
	    *second = *first;
	return 1;
    }
    second_count = bu_cmd_color_from_argv(second, (size_t)(argc - first_count),
	(const char * const *)(argv + first_count));
    if (second_count <= 0 || first_count + second_count != argc)
	return -1;
    return 2;
}

static void
dm_bg_validation_set(struct bu_cmd_validate_result *result,
	bu_cmd_validate_state_t state, size_t token, const char *hint)
{
    bu_cmd_validate_result_clear(result);
    result->state = state;
    result->token_start = token;
    result->token_end = token;
    result->expected = BU_CMD_EXPECT_OPERAND;
    result->completion_type = BU_CMD_VALUE_COLOR;
    result->hint = hint;
}

static int
dm_bg_schema_validate(const struct bu_cmd_schema *schema, size_t argc,
	const char **argv, size_t cursor_arg, struct bu_cmd_validate_result *result)
{
    struct bu_cmd_schema flat = *schema;
    struct bu_color first = BU_COLOR_INIT_ZERO;
    struct bu_color second = BU_COLOR_INIT_ZERO;
    int first_count;
    int second_count;
    int ret;

    flat.validation.custom_validate = NULL;
    ret = bu_cmd_schema_validate(&flat, argc, argv, cursor_arg, result);
    if (ret || result->state == BU_CMD_VALIDATE_INVALID || !argc)
	return ret;

    first_count = bu_cmd_color_from_argv(&first, argc,
	(const char * const *)argv);
    if (first_count <= 0) {
	ret = bu_cmd_color_optional_validate(argc, argv, cursor_arg, result);
	return ret;
    }
    if ((size_t)first_count == argc) {
	dm_bg_validation_set(result, BU_CMD_VALIDATE_VALID, cursor_arg,
	    "background color");
	return 0;
    }
    second_count = bu_cmd_color_from_argv(&second, argc - (size_t)first_count,
	(const char * const *)(argv + first_count));
    if (second_count <= 0) {
	ret = bu_cmd_color_optional_validate(argc - (size_t)first_count,
	    argv + first_count, cursor_arg >= (size_t)first_count ?
	    cursor_arg - (size_t)first_count : 0, result);
	if (!ret) {
	    result->token_start += (size_t)first_count;
	    result->token_end += (size_t)first_count;
	}
	return ret;
    }
    if ((size_t)(first_count + second_count) != argc) {
	dm_bg_validation_set(result, BU_CMD_VALIDATE_INVALID,
	    (size_t)(first_count + second_count), "at most two background colors");
	return 0;
    }
    dm_bg_validation_set(result, BU_CMD_VALIDATE_VALID, cursor_arg,
	"background color");
    return 0;
}

struct dm *
_dm_name_lookup(struct _ged_dm_info *gd, const char *dm_name)
{
    struct dm *cdmp = NULL;
    struct bview *gdvp = NULL;
    struct dm *ndmp = NULL;

    if (!gd) {
	return NULL;
    }
    if (!dm_name || !strlen(dm_name)) {
	bu_vls_printf(gd->gedp->ged_result_str, ": no DM specified and no current DM set in GED\n");
	return NULL;
    }

    struct ged *gedp = gd->gedp;
    struct bu_ptbl *views = bv_set_views(&gedp->ged_views);
    if (!BU_PTBL_LEN(views)) {
	bu_vls_printf(gedp->ged_result_str, ": no views defined in GED\n");
	return NULL;
    }
    int dm_cnt = 0;
    for (size_t i = 0; i < BU_PTBL_LEN(views); i++) {
	gdvp = (struct bview *)BU_PTBL_GET(views, i);
	if (!gdvp->dmp)
	    continue;
	dm_cnt++;
    }
    if (!dm_cnt) {
	bu_vls_printf(gedp->ged_result_str, ": no views have associated DMs defined\n");
	return NULL;
    }

    for (size_t i = 0; i < BU_PTBL_LEN(views); i++) {
	gdvp = (struct bview *)BU_PTBL_GET(views, i);
	ndmp = (struct dm *)gdvp->dmp;
	if (ndmp && BU_STR_EQUAL(dm_name, bu_vls_cstr(dm_get_pathname(ndmp)))) {
	    cdmp = ndmp;
	    break;
	}
    }
    if (!cdmp) {
	bu_vls_printf(gd->gedp->ged_result_str, ": no DM with name %s found\n", dm_name);
    }

    return cdmp;
}

static struct dm *
_dm_find(struct _ged_dm_info *gd, struct bu_vls *name)
{
    if (!gd)
	return NULL;

    struct ged *gedp = gd->gedp;
    if (!name || !bu_vls_strlen(name)) {
	if (!gedp->ged_gvp) {
	    bu_vls_printf(gedp->ged_result_str, ": no current view is set in GED\n");
	    return NULL;
	}
	if (!gedp->ged_gvp->dmp) {
	    bu_vls_printf(gedp->ged_result_str, ": no current DM is set in GED's current view\n");
	    return NULL;
	}
	return (struct dm *)gedp->ged_gvp->dmp;
    }
    if (name && gedp->ged_gvp && gedp->ged_gvp->dmp) {
	struct dm *cdmp = (struct dm *)gedp->ged_gvp->dmp;
	if (BU_STR_EQUAL(bu_vls_cstr(name), bu_vls_cstr(dm_get_pathname(cdmp))))
	    return cdmp;
    }

    return _dm_name_lookup(gd, bu_vls_cstr(name));
}

int
_dm_cmd_bg(void *ds, int argc, const char **argv)
{
    struct _ged_dm_info *gd = (struct _ged_dm_info *)ds;
    struct ged *gedp = gd->gedp;
    struct dm *cdmp = _dm_find(gd, NULL);
    struct bu_color first = BU_COLOR_INIT_ZERO;
    struct bu_color second = BU_COLOR_INIT_ZERO;
    int color_count;

    if (bu_cmd_schema_parse_complete(&dm_bg_schema, NULL,
	gedp->ged_result_str, argc - 1, argv + 1) < 0) {
	return BRLCAD_ERROR;
    }
    if (!cdmp) {
	return BRLCAD_ERROR;
    }

    argc--;
    argv++;
    if (!argc) {
	unsigned char *dm_bg1 = NULL;
	unsigned char *dm_bg2 = NULL;
	int dm_bg_type = dm_get_bg(&dm_bg1, &dm_bg2, cdmp);
	if (dm_bg_type == 1) {
	    bu_vls_printf(gedp->ged_result_str, "%d/%d/%d->%d/%d/%d\n", (short)dm_bg1[0], (short)dm_bg1[1], (short)dm_bg1[2], (short)dm_bg2[0], (short)dm_bg2[1], (short)dm_bg2[2]);
	} else {
	    bu_vls_printf(gedp->ged_result_str, "%d/%d/%d\n", (short)dm_bg1[0], (short)dm_bg1[1], (short)dm_bg1[2]);
	}
	return BRLCAD_OK;
    }

    color_count = dm_bg_colors_from_argv(argc, argv, &first, &second);
    if (color_count < 0) {
	bu_vls_printf(gedp->ged_result_str, "invalid color specification\n");
	return BRLCAD_ERROR;
    }
    unsigned char n_bg1[3], n_bg2[3];
    bu_color_to_rgb_chars(&first, n_bg1);
    bu_color_to_rgb_chars(&second, n_bg2);

    dm_set_bg(cdmp, n_bg1[0], n_bg1[1], n_bg1[2], n_bg2[0], n_bg2[1], n_bg2[2]);

    const char *cbav[4] = {"dm", "bg", argv[0], NULL};
    _dm_cmd_during_clbk(gd, 3, cbav);

    return BRLCAD_OK;
}

int
_dm_cmd_debug(void *ds, int argc, const char **argv)
{
    struct _ged_dm_info *gd = (struct _ged_dm_info *)ds;
    struct ged *gedp = gd->gedp;
    struct dm *cdmp = _dm_find(gd, NULL);

    if (bu_cmd_schema_parse_complete(&dm_debug_schema, NULL,
	gedp->ged_result_str, argc - 1, argv + 1) < 0) {
	return BRLCAD_ERROR;
    }
    if (!cdmp) {
	return BRLCAD_ERROR;
    }

    argc--;
    argv++;
    if (!argc) {
	bu_vls_printf(gedp->ged_result_str, "%d\n", dm_get_debug(cdmp));
	return BRLCAD_OK;
    }

    int lvl;
    if (!bu_cmd_integer_from_str(&lvl, argv[0]))
	return BRLCAD_ERROR;
    dm_set_debug(cdmp, lvl);
    return BRLCAD_OK;
}

int
_dm_cmd_type(void *ds, int argc, const char **argv)
{
    struct _ged_dm_info *gd = (struct _ged_dm_info *)ds;
    struct bu_vls dm_name = BU_VLS_INIT_ZERO;
    struct dm *cdmp;

    if (bu_cmd_schema_parse_complete(&dm_type_schema, NULL,
	gd->gedp->ged_result_str, argc - 1, argv + 1) < 0) {
	return BRLCAD_ERROR;
    }
    if (argc == 2)
	bu_vls_strcpy(&dm_name, argv[1]);
    cdmp = _dm_find(gd, &dm_name);
    bu_vls_free(&dm_name);
    if (!cdmp) {
	return BRLCAD_ERROR;
    }

    bu_vls_printf(gd->gedp->ged_result_str, "%s\n", dm_get_type(cdmp));
    return BRLCAD_OK;
}

int
_dm_cmd_types(void *ds, int argc, const char **argv)
{
    struct _ged_dm_info *gd = (struct _ged_dm_info *)ds;

    if (bu_cmd_schema_parse_complete(&dm_types_schema, NULL,
	gd->gedp->ged_result_str, argc - 1, argv + 1) < 0)
	return BRLCAD_ERROR;

    struct bu_vls list = BU_VLS_INIT_ZERO;
    dm_list_types(&list, "\n");
    bu_vls_printf(gd->gedp->ged_result_str, "%s\n", bu_vls_cstr(&list));
    bu_vls_free(&list);

    return BRLCAD_OK;
}

int
_dm_cmd_initmsg(void *ds, int argc, const char **argv)
{
    struct _ged_dm_info *gd = (struct _ged_dm_info *)ds;

    if (bu_cmd_schema_parse_complete(&dm_initmsg_schema, NULL,
	gd->gedp->ged_result_str, argc - 1, argv + 1) < 0)
	return BRLCAD_ERROR;

    bu_vls_printf(gd->gedp->ged_result_str, "%s\n", dm_init_msgs());
    return BRLCAD_OK;
}

int
_dm_cmd_list(void *ds, int argc, const char **argv)
{
    struct _ged_dm_info *gd = (struct _ged_dm_info *)ds;
    struct ged *gedp = gd->gedp;

    if (bu_cmd_schema_parse_complete(&dm_list_schema, NULL,
	gedp->ged_result_str, argc - 1, argv + 1) < 0)
	return BRLCAD_ERROR;

    struct bview *cv = gedp->ged_gvp;
    struct dm *cdmp = cv ? (struct dm *)cv->dmp : NULL;
    if (cdmp) {
	// Current dmp first, if we have a current instance
	if (gd->verbosity) {
	    bu_vls_printf(gedp->ged_result_str, " %s (%s)\n", bu_vls_cstr(dm_get_pathname(cdmp)), dm_get_type(cdmp));
	} else {
	    bu_vls_printf(gedp->ged_result_str, "%s\n", bu_vls_cstr(dm_get_pathname(cdmp)));
	}
    }

    struct bu_ptbl *views = bv_set_views(&gedp->ged_views);
    if (!BU_PTBL_LEN(views) && !cdmp) {
	bu_vls_printf(gedp->ged_result_str, ": no views defined in GED\n");
	return BRLCAD_ERROR;
    }
    int dm_cnt = 0;
    for (size_t i = 0; i < BU_PTBL_LEN(views); i++) {
	struct bview *gdvp = (struct bview *)BU_PTBL_GET(views, i);
	if (!gdvp->dmp)
	    continue;
	dm_cnt++;
    }
    if (!dm_cnt && !cdmp) {
	bu_vls_printf(gedp->ged_result_str, ": no views have associated DMs defined\n");
	return BRLCAD_ERROR;
    }

    for (size_t i = 0; i < BU_PTBL_LEN(views); i++) {
	struct bview *gdvp = (struct bview *)BU_PTBL_GET(views, i);
	struct dm *ndmp = (struct dm *)gdvp->dmp;
	if (!ndmp || ndmp == cdmp)
	    continue;
	if (gd->verbosity) {
	    bu_vls_printf(gedp->ged_result_str, " %s (%s)\n", bu_vls_cstr(dm_get_pathname(ndmp)), dm_get_type(ndmp));
	} else {
	    bu_vls_printf(gedp->ged_result_str, "%s\n", bu_vls_cstr(dm_get_pathname(ndmp)));
	}
    }

    return BRLCAD_OK;
}

int
_dm_cmd_get(void *ds, int argc, const char **argv)
{
    struct _ged_dm_info *gd = (struct _ged_dm_info *)ds;
    struct dm_target_args args = {BU_VLS_INIT_ZERO};
    int operand_index;
    int operand_count;
    const char **operands;
    struct dm *cdmp;

    operand_index = bu_cmd_schema_parse_complete(&dm_get_schema, &args,
	gd->gedp->ged_result_str, argc - 1, argv + 1);
    if (operand_index < 0) {
	bu_vls_free(&args.dm_name);
	return BRLCAD_ERROR;
    }
    operand_count = argc - 1 - operand_index;
    operands = argv + 1 + operand_index;
    cdmp = _dm_find(gd, &args.dm_name);
    if (!cdmp) {
	bu_vls_free(&args.dm_name);
	return BRLCAD_ERROR;
    }

    if (!operand_count) {
	// Report all vars
	struct bu_vls rstr = BU_VLS_INIT_ZERO;
	bu_vls_sprintf(&rstr, "Display Manager %s (type %s) internal variables", bu_vls_cstr(dm_get_pathname(cdmp)), dm_get_dm_name(cdmp));
	struct bu_structparse *dmparse = dm_get_vparse(cdmp);
	void *mvars = dm_get_mvars(cdmp);
	if (dmparse && mvars) {
	    bu_vls_struct_print2(gd->gedp->ged_result_str, bu_vls_addr(&rstr), dmparse, (const char *)mvars);
	}
	bu_vls_free(&rstr);
	bu_vls_free(&args.dm_name);
	return BRLCAD_OK;
    }

    struct bu_structparse *dmparse = dm_get_vparse(cdmp);
    void *mvars = dm_get_mvars(cdmp);
    if (!dmparse || !mvars) {
	// No variables to report
	bu_vls_free(&args.dm_name);
	return BRLCAD_OK;
    }

    for (int i = 0; i < operand_count; i++) {
	if (gd->verbosity) {
	    bu_vls_printf(gd->gedp->ged_result_str, "%s=", operands[i]);
	    bu_vls_struct_item_named(gd->gedp->ged_result_str, dmparse, operands[i], (const char *)mvars, COMMA);
	} else {
	    bu_vls_struct_item_named(gd->gedp->ged_result_str, dmparse, operands[i], (const char *)mvars, COMMA);
	}
	if (i < operand_count - 1) {
	    bu_vls_printf(gd->gedp->ged_result_str, "\n");
	}
    }

    bu_vls_free(&args.dm_name);
    return BRLCAD_OK;
}

int
_dm_cmd_set(void *ds, int argc, const char **argv)
{
    struct _ged_dm_info *gd = (struct _ged_dm_info *)ds;
    struct dm_target_args args = {BU_VLS_INIT_ZERO};
    int operand_index;
    int operand_count;
    const char **operands;
    struct dm *cdmp;

    operand_index = bu_cmd_schema_parse_complete(&dm_set_schema, &args,
	gd->gedp->ged_result_str, argc - 1, argv + 1);
    if (operand_index < 0) {
	bu_vls_free(&args.dm_name);
	return BRLCAD_ERROR;
    }
    operand_count = argc - 1 - operand_index;
    operands = argv + 1 + operand_index;
    cdmp = _dm_find(gd, &args.dm_name);
    if (!cdmp) {
	bu_vls_free(&args.dm_name);
	return BRLCAD_ERROR;
    }

    struct bu_structparse *dmparse = dm_get_vparse(cdmp);
    void *mvars = dm_get_mvars(cdmp);
    if (!dmparse || !mvars) {
	// No variables to set
	bu_vls_printf(gd->gedp->ged_result_str, "display manager has not associated variables\n");
	bu_vls_free(&args.dm_name);
	return BRLCAD_ERROR;
    }

    if (!operand_count) {
	/* MGED-compatible behavior: "dm set" reports all current values. */
	struct bu_vls rstr = BU_VLS_INIT_ZERO;
	bu_vls_sprintf(&rstr, "Display Manager %s (type %s) internal variables", bu_vls_cstr(dm_get_pathname(cdmp)), dm_get_dm_name(cdmp));
	bu_vls_struct_print2(gd->gedp->ged_result_str, bu_vls_addr(&rstr), dmparse, (const char *)mvars);
	bu_vls_free(&rstr);
	bu_vls_free(&args.dm_name);
	return BRLCAD_OK;
    }

    if (operand_count == 1) {
	/* MGED-compatible behavior: "dm set <key>" reports current value. */
	bu_vls_struct_item_named(gd->gedp->ged_result_str, dmparse, operands[0], (const char *)mvars, COMMA);
	bu_vls_free(&args.dm_name);
	return BRLCAD_OK;
    }

    if (operand_count != 2) {
	bu_vls_printf(gd->gedp->ged_result_str, ": invalid argument count - need key and value");
	bu_vls_free(&args.dm_name);
	return BRLCAD_ERROR;
    }

    struct bu_vls tmp_vls = BU_VLS_INIT_ZERO;
    struct bu_vls parse_msgs = BU_VLS_INIT_ZERO;
    struct bu_hook_list saved_log_hooks = BU_HOOK_LIST_INIT_ZERO;
    int ret;
    bu_vls_printf(&tmp_vls, "%s=\"%s\"", operands[0], operands[1]);

    bu_log_hook_save_all(&saved_log_hooks);
    bu_log_hook_delete_all();
    bu_log_add_hook(_ged_dm_log_to_vls, (void *)&parse_msgs);

    ret = bu_struct_parse(&tmp_vls, dmparse, (char *)mvars, NULL);

    bu_log_hook_delete_all();
    bu_log_hook_restore_all(&saved_log_hooks);

    bu_vls_free(&tmp_vls);
    if (ret < 0) {
	if (bu_vls_strlen(&parse_msgs)) {
	    bu_vls_printf(gd->gedp->ged_result_str, "%s", bu_vls_cstr(&parse_msgs));
	}
	bu_vls_printf(gd->gedp->ged_result_str, ": unable to set %s", operands[0]);
	bu_vls_free(&parse_msgs);
	bu_vls_free(&args.dm_name);
	return BRLCAD_ERROR;
    }
    bu_vls_free(&parse_msgs);

    if (gd->verbosity) {
	if (gd->verbosity > 1) {
	    bu_vls_printf(gd->gedp->ged_result_str, "%s=", operands[0]);
	    bu_vls_struct_item_named(gd->gedp->ged_result_str, dmparse, operands[0], (const char *)mvars, COMMA);
	} else {
	    bu_vls_struct_item_named(gd->gedp->ged_result_str, dmparse, operands[0], (const char *)mvars, COMMA);
	}
    }

    const char *cbav[4] = {"dm", "set", operands[0], operands[1]};
    _dm_cmd_during_clbk(gd, 4, cbav);

    bu_vls_free(&args.dm_name);
    return BRLCAD_OK;
}

int
_dm_cmd_attach(void *ds, int argc, const char **argv)
{
    struct _ged_dm_info *gd = (struct _ged_dm_info *)ds;
    struct ged *gedp = gd->gedp;
    struct dm_attach_args args = {BU_VLS_INIT_ZERO, BU_VLS_INIT_ZERO};
    struct bview *target_view = NULL;
    const char *dm_type = NULL;
    const char **operands = NULL;
    int operand_index;
    int operand_count;
    int ret = BRLCAD_ERROR;

    operand_index = bu_cmd_schema_parse_complete(&dm_attach_schema, &args,
	gedp->ged_result_str, argc - 1, argv + 1);
    if (operand_index < 0)
	goto done;
    operand_count = argc - 1 - operand_index;
    operands = argv + 1 + operand_index;
    dm_type = operands[0];

    if (operand_count == 2) {
	if (bu_vls_strlen(&args.dm_name) &&
	    !BU_STR_EQUAL(operands[1], bu_vls_cstr(&args.dm_name))) {
	    bu_vls_printf(gedp->ged_result_str,
		"Two different dm names specified: %s and %s\n", operands[1],
		bu_vls_cstr(&args.dm_name));
	    goto done;
	}
	bu_vls_strcpy(&args.dm_name, operands[1]);
    }

    struct bu_ptbl *views = bv_set_views(&gedp->ged_views);
    if (!bu_vls_strlen(&args.dm_name)) {
	int tries = 0;
	bu_vls_sprintf(&args.dm_name, "%s-0", dm_type);
	for (;;) {
	    int exists = 0;
	    for (size_t i = 0; i < BU_PTBL_LEN(views); i++) {
		struct bview *view = (struct bview *)BU_PTBL_GET(views, i);
		struct dm *dmp = view ? (struct dm *)view->dmp : NULL;
		if (dmp && BU_STR_EQUAL(bu_vls_cstr(dm_get_pathname(dmp)),
			bu_vls_cstr(&args.dm_name))) {
		    exists = 1;
		    break;
		}
	    }
	    if (!exists)
		break;
	    if (++tries == DM_MAX_TRIES) {
		bu_vls_printf(gedp->ged_result_str, "unable to generate DM name");
		goto done;
	    }
	    bu_vls_incr(&args.dm_name, NULL, "0:0:0:0:-", NULL, NULL);
	}
    } else {
	for (size_t i = 0; i < BU_PTBL_LEN(views); i++) {
	    struct bview *view = (struct bview *)BU_PTBL_GET(views, i);
	    struct dm *dmp = view ? (struct dm *)view->dmp : NULL;
	    if (dmp && BU_STR_EQUAL(bu_vls_cstr(dm_get_pathname(dmp)),
		    bu_vls_cstr(&args.dm_name))) {
		bu_vls_printf(gedp->ged_result_str, "DM %s already exists",
		    bu_vls_cstr(&args.dm_name));
		goto done;
	    }
	}
    }

    if (bu_vls_strlen(&args.view_name)) {
	for (size_t i = 0; i < BU_PTBL_LEN(views); i++) {
	    struct bview *view = (struct bview *)BU_PTBL_GET(views, i);
	    if (view && !bu_vls_strcmp(&args.view_name, &view->gv_name)) {
		target_view = view;
		break;
	    }
	}
	if (!target_view) {
	    bu_vls_printf(gedp->ged_result_str, "View %s not found",
		bu_vls_cstr(&args.view_name));
	    goto done;
	}
    } else if (gedp->ged_gvp && !gedp->ged_gvp->dmp) {
	target_view = gedp->ged_gvp;
    }

    if (!target_view) {
	BU_GET(target_view, struct bview);
	bv_init(target_view, &gedp->ged_views);
	bv_set_add_view(&gedp->ged_views, target_view);
	bu_ptbl_ins(&gedp->ged_free_views, (long *)target_view);
    }

    if (target_view->dmp) {
	bu_vls_printf(gedp->ged_result_str,
	    "Target view %s of dm attach already has an associated dm\n",
	    bu_vls_cstr(&target_view->gv_name));
	goto done;
    }

    if (!target_view->gv_width)
	target_view->gv_width = 512;
    if (!target_view->gv_height)
	target_view->gv_height = 512;

    void *ctx = ged_dm_ctx_get(gedp, dm_type);
    if (!ctx)
	ctx = (void *)target_view;

    const char *acmd = "attach";
    struct dm *dmp = dm_open(ctx, gedp->ged_interp, dm_type, 1, &acmd);
    if (!dmp) {
	bu_vls_printf(gedp->ged_result_str, "failed to create DM %s",
	    bu_vls_cstr(&args.dm_name));
	goto done;
    }

    dm_set_vp(dmp, &target_view->gv_scale);
    dm_configure_win(dmp, 0);
    dm_set_pathname(dmp, bu_vls_cstr(&args.dm_name));
    dm_set_zbuffer(dmp, 1);
    {
	fastf_t windowbounds[6] = { -1, 1, -1, 1, (int)BV_MIN, (int)BV_MAX };
	dm_set_win_bounds(dmp, windowbounds);
    }
    target_view->dmp = dmp;

    {
	const char *cbav[4] = {"dm", "attach", dm_type, bu_vls_cstr(&args.dm_name)};
	_dm_cmd_during_clbk(gd, 4, cbav);
    }
    ret = BRLCAD_OK;
done:
    bu_vls_free(&args.dm_name);
    bu_vls_free(&args.view_name);
    return ret;
}

int
_dm_cmd_width(void *ds, int argc, const char **argv)
{
    struct bu_vls tmpname = BU_VLS_INIT_ZERO;
    struct _ged_dm_info *gd = (struct _ged_dm_info *)ds;
    struct dm *cdmp;

    if (bu_cmd_schema_parse_complete(&dm_width_schema, NULL,
	gd->gedp->ged_result_str, argc - 1, argv + 1) < 0) {
	return BRLCAD_ERROR;
    }
    if (argc == 2)
	bu_vls_strcpy(&tmpname, argv[1]);
    cdmp = _dm_find(gd, &tmpname);
    bu_vls_free(&tmpname);
    if (!cdmp) {
	return BRLCAD_ERROR;
    }
    bu_vls_printf(gd->gedp->ged_result_str, "%d\n", dm_get_width(cdmp));
    return BRLCAD_OK;
}

int
_dm_cmd_height(void *ds, int argc, const char **argv)
{
    struct bu_vls tmpname = BU_VLS_INIT_ZERO;
    struct _ged_dm_info *gd = (struct _ged_dm_info *)ds;
    struct dm *cdmp;

    if (bu_cmd_schema_parse_complete(&dm_height_schema, NULL,
	gd->gedp->ged_result_str, argc - 1, argv + 1) < 0) {
	return BRLCAD_ERROR;
    }
    if (argc == 2)
	bu_vls_strcpy(&tmpname, argv[1]);
    cdmp = _dm_find(gd, &tmpname);
    bu_vls_free(&tmpname);
    if (!cdmp) {
	return BRLCAD_ERROR;
    }

    bu_vls_printf(gd->gedp->ged_result_str, "%d\n", dm_get_height(cdmp));
    return BRLCAD_OK;
}

static const struct bu_cmd_tree_node dm_subcommands[] = {
    BU_CMD_TREE_NODE(&dm_attach_schema, NULL, NULL,
	BU_CMD_TREE_CHILD_AFTER_OPTIONS, _dm_cmd_attach),
    BU_CMD_TREE_NODE(&dm_bg_schema, NULL, NULL,
	BU_CMD_TREE_CHILD_AFTER_OPTIONS, _dm_cmd_bg),
    BU_CMD_TREE_NODE(&dm_debug_schema, NULL, NULL,
	BU_CMD_TREE_CHILD_AFTER_OPTIONS, _dm_cmd_debug),
    BU_CMD_TREE_NODE(&dm_get_schema, NULL, NULL,
	BU_CMD_TREE_CHILD_AFTER_OPTIONS, _dm_cmd_get),
    BU_CMD_TREE_NODE(&dm_height_schema, NULL, NULL,
	BU_CMD_TREE_CHILD_AFTER_OPTIONS, _dm_cmd_height),
    BU_CMD_TREE_NODE(&dm_initmsg_schema, NULL, NULL,
	BU_CMD_TREE_CHILD_AFTER_OPTIONS, _dm_cmd_initmsg),
    BU_CMD_TREE_NODE(&dm_list_schema, NULL, NULL,
	BU_CMD_TREE_CHILD_AFTER_OPTIONS, _dm_cmd_list),
    BU_CMD_TREE_NODE(&dm_set_schema, NULL, NULL,
	BU_CMD_TREE_CHILD_AFTER_OPTIONS, _dm_cmd_set),
    BU_CMD_TREE_NODE(&dm_type_schema, NULL, NULL,
	BU_CMD_TREE_CHILD_AFTER_OPTIONS, _dm_cmd_type),
    BU_CMD_TREE_NODE(&dm_types_schema, NULL, NULL,
	BU_CMD_TREE_CHILD_AFTER_OPTIONS, _dm_cmd_types),
    BU_CMD_TREE_NODE(&dm_width_schema, NULL, NULL,
	BU_CMD_TREE_CHILD_AFTER_OPTIONS, _dm_cmd_width),
    BU_CMD_TREE_NODE_NULL
};
static const struct bu_cmd_tree dm_tree = {
    &dm_root_schema, dm_subcommands, BU_CMD_TREE_CHILD_AFTER_OPTIONS
};

static void
dm_tree_show_help(struct ged *gedp, const struct bu_cmd_schema *child)
{
    char *help;

    if (!gedp)
	return;
    if (!child) {
	help = bu_cmd_tree_describe(&dm_tree);
	if (help) {
	    bu_vls_printf(gedp->ged_result_str, "%s", help);
	    bu_free(help, "native dm tree help");
	}
	return;
    }
    help = bu_cmd_schema_describe(child);
    bu_vls_printf(gedp->ged_result_str, "Usage: dm [options] %s [args]\n",
	child->name);
    if (help) {
	bu_vls_printf(gedp->ged_result_str, "\n%s", help);
	bu_free(help, "native dm child help");
    }
}

int
ged_dm_core(struct ged *gedp, int argc, const char *argv[])
{
    struct _ged_dm_info gd = {0};
    struct dm_root_args args = {0, 0};
    const struct bu_cmd_tree_node *node = NULL;
    int cmd_pos = -1;
    int root_argc;
    int ret;

    // Sanity
    if (UNLIKELY(!gedp || !argc || !argv)) {
	return BRLCAD_ERROR;
    }

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    // We know we're the dm command - start processing args
    argc--; argv++;

    for (int i = 0; i < argc; i++) {
	int option_span = bu_cmd_schema_option_span(&dm_root_schema,
	    (size_t)(argc - i), argv + i);
	if (option_span > 0) {
	    i += option_span - 1;
	    continue;
	}
	if (option_span < 0 || (argv[i][0] == '-' && argv[i][1]))
	    break;
	cmd_pos = i;
	break;
    }


    root_argc = (cmd_pos >= 0) ? cmd_pos : argc;
    if (bu_cmd_schema_parse_complete(&dm_root_schema, &args,
	gedp->ged_result_str, root_argc, argv) < 0) {
	dm_tree_show_help(gedp, NULL);
	return BRLCAD_ERROR;
    }
    argc -= root_argc;
    argv += root_argc;
    gd.gedp = gedp;
    gd.verbosity = args.verbosity;

    if (args.print_help) {
	if (argc)
	    node = bu_cmd_tree_find_subcommand(&dm_tree, argv[0]);
	dm_tree_show_help(gedp, node ? node->schema : NULL);
	return GED_HELP;
    }
    if (!argc) {
	dm_tree_show_help(gedp, NULL);
	return GED_HELP;
    }

    if (bu_cmd_tree_dispatch(&dm_tree, &gd, argc, argv, &ret) == 0) {
	return ret;
    }

	bu_vls_printf(gedp->ged_result_str, "subcommand %s not defined", argv[0]);

    return BRLCAD_ERROR;
}


#include "../include/plugin.h"

extern int ged_ert_core(struct ged *gedp, int argc, const char *argv[]);
extern int ged_screen_grab_core(struct ged *gedp, int argc, const char *argv[]);
extern const struct bu_cmd_schema ged_screengrab_schema;
extern const struct bu_cmd_schema ged_screen_grab_schema;

static const struct bu_cmd_operand ert_operands[] = {
    BU_CMD_OPERAND("rt_arguments", BU_CMD_VALUE_RAW, 0, BU_CMD_COUNT_UNLIMITED,
	"Arguments forwarded to rt; use -- before process arguments", NULL),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_schema ert_cmd_schema = {
    "ert", "Raytrace the current view into its embedded framebuffer", NULL,
    ert_operands, BU_CMD_PARSE_STOP_AT_FIRST_OPERAND,
    BU_CMD_SCHEMA_CONSTRAINTS(NULL, NULL)
};

static int
ged_dm_grammar_validate(struct ged *gedp, const char *input, size_t cursor_pos,
	struct ged_cmd_validate_result *result)
{
    return ged_cmd_tree_validate(gedp, &dm_tree, input, cursor_pos, result);
}

static int
ged_dm_grammar_analyze(struct ged *gedp, const char *input,
	struct ged_cmd_analysis *analysis)
{
    return ged_cmd_tree_analyze(gedp, &dm_tree, input, analysis);
}

static char *
ged_dm_grammar_json(void)
{
    return bu_cmd_tree_describe_json(&dm_tree);
}

static int
ged_dm_grammar_lint(struct bu_vls *msgs)
{
    return bu_cmd_tree_lint(&dm_tree, msgs);
}

static const struct ged_cmd_grammar ged_dm_grammar = {
    "dm", "Manage display manager instances", ged_dm_grammar_validate,
    ged_dm_grammar_analyze, ged_dm_grammar_json, ged_dm_grammar_lint
};

#define GED_DM_COMMANDS(X, XID, N, NID, G, GID) \
    N(ert, ged_ert_core, GED_CMD_DEFAULT, &ert_cmd_schema) \
    G(dm, ged_dm_core, GED_CMD_DEFAULT, &ged_dm_grammar) \
    N(screen_grab, ged_screen_grab_core, GED_CMD_DEFAULT, &ged_screen_grab_schema) \
    N(screengrab, ged_screen_grab_core, GED_CMD_DEFAULT, &ged_screengrab_schema) \

GED_DECLARE_COMMAND_SET_WITH_MIXED_SCHEMA(GED_DM_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST_WITH_MIXED_SCHEMA("libged_dm", 1, GED_DM_COMMANDS)

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
