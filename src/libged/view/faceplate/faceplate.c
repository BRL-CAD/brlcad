/*                      F A C E P L A T E . C
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
/** @file libged/faceplate/faceplate.c
 *
 * Controls for view elements (center dot, model axes, view axes,
 * etc.) that are built into BRL-CAD views.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "bu/cmd.h"
#include "bu/cmdschema.h"
#include "bu/color.h"
#include "bu/malloc.h"
#include "bu/vls.h"
#include "bn/tol.h"
#include "bv.h"

#include "../../ged_private.h"
#include "../ged_view.h"
#include "./faceplate.h"

#ifndef COMMA
#  define COMMA ','
#endif

#define HELPFLAG "--print-help"
#define PURPOSEFLAG "--print-purpose"

struct _ged_fp_info {
    struct ged *gedp;
    long verbosity;
    const struct bu_cmdtab *cmds;
    void *gopts;
};

struct fp_root_args {
    int help;
    long verbosity;
};

static const struct bu_cmd_option fp_root_options[] = {
    BU_CMD_FLAG("h", "help", struct fp_root_args, help, "Print help"),
    BU_CMD_COUNTING_LONG_FLAG("v", "verbose", struct fp_root_args, verbosity,
	"Increase output detail"),
    BU_CMD_OPTION_NULL
};

const struct bu_cmd_schema ged_faceplate_native_schema = {
    "faceplate", "Manage faceplate view elements", fp_root_options, NULL,
    BU_CMD_PARSE_OPTIONS_FIRST, BU_CMD_SCHEMA_CONSTRAINTS(NULL, NULL)
};

static const struct bu_cmd_option fp_subcommand_options[] = {
    BU_CMD_FLAG("h", "help", struct ged_faceplate_subcommand_args, help, "Print help"),
    BU_CMD_OPTION_NULL
};

const struct bu_cmd_schema ged_faceplate_subcommand_schema = {
    "subcommand", "Faceplate subcommand options", fp_subcommand_options, NULL,
    BU_CMD_PARSE_OPTIONS_FIRST, BU_CMD_SCHEMA_CONSTRAINTS(NULL, NULL)
};

static int
_fp_cmd_msgs(void *bs, int argc, const char **argv, const char *UNUSED(us),
	const char *UNUSED(ps))
{
    struct _ged_fp_info *gd = (struct _ged_fp_info *)bs;
    if (argc == 2 && BU_STR_EQUAL(argv[1], HELPFLAG)) {
	const char *path[] = {argv[0]};
	char *help = bu_cmd_tree_help_path(&ged_faceplate_tree,
	    "view faceplate", 1, path);
	if (help) {
	    bu_vls_strcat(gd->gedp->ged_result_str, help);
	    bu_free(help, "faceplate leaf help");
	}
	return 1;
    }
    if (argc == 2 && BU_STR_EQUAL(argv[1], PURPOSEFLAG)) {
	const struct bu_cmd_tree_node *node = bu_cmd_tree_find_subcommand(
	    &ged_faceplate_tree, argv[0]);
	bu_vls_printf(gd->gedp->ged_result_str, "%s\n",
	    (node && node->schema && node->schema->help) ?
	    node->schema->help : "");
	return 1;
    }
    return 0;
}


static int
_fp_subcmd_help(struct ged *gedp, const struct bu_cmd_schema *schema,
	const struct bu_cmdtab *cmds, const char *cmdname, const char *cmdargs,
	void *context, int argc, const char **argv)
{
    const char *path[2] = {NULL, NULL};
    size_t path_argc = 0;
    const char *branch;
    char *help;

    if (!gedp || !schema || !cmds || !cmdname)
	return BRLCAD_ERROR;
    (void)schema;
    (void)cmdargs;
    (void)context;

    branch = cmdname + strlen("view faceplate");
    while (*branch == ' ')
	branch++;
    if (*branch)
	path[path_argc++] = branch;
    if (argc && argv && !BU_STR_EQUAL(argv[0], "help"))
	path[path_argc++] = argv[0];

    help = bu_cmd_tree_help_path(&ged_faceplate_tree, "view faceplate",
	path_argc, path);
    if (!help)
	return BRLCAD_ERROR;
    bu_vls_strcat(gedp->ged_result_str, help);
    bu_free(help, "faceplate tree path help");
    return BRLCAD_OK;
}


int
_fp_subcmd_exec(struct ged *gedp, const struct bu_cmd_schema *schema,
	const struct bu_cmdtab *cmds, const char *cmdname, const char *cmdargs,
	void *context, int argc, const char **argv, int help)
{
    if (help)
	return _fp_subcmd_help(gedp, schema, cmds, cmdname, cmdargs, context, argc, argv);

    if (!argc) {
	bu_vls_printf(gedp->ged_result_str, ": no valid subcommand specified\n");
	_fp_subcmd_help(gedp, schema, cmds, cmdname, cmdargs, context, 0, NULL);
	return BRLCAD_ERROR;
    }

    int ret = BRLCAD_ERROR;
    if (bu_cmd(cmds, argc, argv, 0, context, &ret) == BRLCAD_OK)
	return ret;

    bu_vls_printf(gedp->ged_result_str, "subcommand %s not defined", argv[0]);
    return BRLCAD_ERROR;
}


int
_fp_cmd_center_dot(void *ds, int argc, const char **argv)
{
    if (_fp_cmd_msgs(ds, argc, argv, NULL, NULL)) {
	return BRLCAD_OK;
    }

    argc--; argv++;

    struct _ged_fp_info *gd = (struct _ged_fp_info *)ds;
    struct ged *gedp = gd->gedp;
    struct bview *v = gedp->ged_gvp;

    if (!argc) {
	if (gd->verbosity) {
	    bu_vls_printf(gedp->ged_result_str, "%d (%d/%d/%d)", v->gv_s->gv_center_dot.gos_draw,
		    v->gv_s->gv_center_dot.gos_line_color[0], v->gv_s->gv_center_dot.gos_line_color[1],
		    v->gv_s->gv_center_dot.gos_line_color[2]);
	} else {
	    bu_vls_printf(gedp->ged_result_str, "%d", v->gv_s->gv_center_dot.gos_draw);
	}
	return BRLCAD_OK;
    }

    if (argc == 1) {
	if (BU_STR_EQUAL("1", argv[0])) {
	    v->gv_s->gv_center_dot.gos_draw = 1;
	    return BRLCAD_OK;
	}
	if (BU_STR_EQUAL("0", argv[0])) {
	    v->gv_s->gv_center_dot.gos_draw = 0;
	    return BRLCAD_OK;
	}
	bu_vls_printf(gedp->ged_result_str, "value %s is invalid - valid values are 0 or 1\n", argv[0]);
	return BRLCAD_ERROR;
    }

    if (argc > 1) {
	if (!BU_STR_EQUAL("color", argv[0])) {
	    bu_vls_printf(gedp->ged_result_str, "unknown subcommand %s\n", argv[0]);
	    return BRLCAD_ERROR;
	}
	argc--; argv++;
	struct bu_color c;
	if (bu_cmd_color_from_argv(&c, (size_t)argc,
		(const char * const *)argv) != argc) {
	    bu_vls_printf(gedp->ged_result_str, "invalid color specification\n");
	    return BRLCAD_ERROR;
	}
	int *cls = (int *)(v->gv_s->gv_center_dot.gos_line_color);
	bu_color_to_rgb_ints(&c, &cls[0], &cls[1], &cls[2]);
	return BRLCAD_OK;
    }

    bu_vls_printf(gedp->ged_result_str, "invalid command\n");

    return BRLCAD_OK;
}

int
_fp_cmd_fb(void *ds, int argc, const char **argv)
{
    if (_fp_cmd_msgs(ds, argc, argv, NULL, NULL)) {
	return BRLCAD_OK;
    }

    argc--; argv++;

    struct _ged_fp_info *gd = (struct _ged_fp_info *)ds;
    struct ged *gedp = gd->gedp;
    struct bview *v = gedp->ged_gvp;

    if (!argc) {
	bu_vls_printf(gedp->ged_result_str, "%d", v->gv_s->gv_fb_mode);
	return BRLCAD_OK;
    }

    if (argc == 1) {
	if (BU_STR_EQUAL("2", argv[0])) {
	    v->gv_s->gv_fb_mode = 2;
	    return BRLCAD_OK;
	}
	if (BU_STR_EQUAL("1", argv[0])) {
	    v->gv_s->gv_fb_mode = 1;
	    return BRLCAD_OK;
	}
	if (BU_STR_EQUAL("0", argv[0])) {
	    v->gv_s->gv_fb_mode = 0;
	    return BRLCAD_OK;
	}
	bu_vls_printf(gedp->ged_result_str, "value %s is invalid - valid values are 0, 1 and 2\n", argv[0]);
	return BRLCAD_ERROR;
    }

    bu_vls_printf(gedp->ged_result_str, "invalid command\n");

    return BRLCAD_OK;
}

int
_fp_cmd_scale(void *ds, int argc, const char **argv)
{
    if (_fp_cmd_msgs(ds, argc, argv, NULL, NULL)) {
	return BRLCAD_OK;
    }

    argc--; argv++;

    struct _ged_fp_info *gd = (struct _ged_fp_info *)ds;
    struct ged *gedp = gd->gedp;
    struct bview *v = gedp->ged_gvp;

    if (!argc) {
	if (gd->verbosity) {
	    bu_vls_printf(gedp->ged_result_str, "%d (%d/%d/%d)", v->gv_s->gv_view_scale.gos_draw,
		    v->gv_s->gv_view_scale.gos_line_color[0], v->gv_s->gv_view_scale.gos_line_color[1],
		    v->gv_s->gv_view_scale.gos_line_color[2]);
	} else {
	    bu_vls_printf(gedp->ged_result_str, "%d", v->gv_s->gv_view_scale.gos_draw);
	}
	return BRLCAD_OK;
    }

    if (argc == 1) {
	if (BU_STR_EQUAL("1", argv[0])) {
	    v->gv_s->gv_view_scale.gos_draw = 1;
	    return BRLCAD_OK;
	}
	if (BU_STR_EQUAL("0", argv[0])) {
	    v->gv_s->gv_view_scale.gos_draw = 0;
	    return BRLCAD_OK;
	}
	bu_vls_printf(gedp->ged_result_str, "value %s is invalid - valid values are 0 or 1\n", argv[0]);
	return BRLCAD_ERROR;
    }

    if (argc > 1) {
	if (!BU_STR_EQUAL("color", argv[0])) {
	    bu_vls_printf(gedp->ged_result_str, "unknown subcommand %s\n", argv[0]);
	    return BRLCAD_ERROR;
	}
	argc--; argv++;
	struct bu_color c;
	if (bu_cmd_color_from_argv(&c, (size_t)argc,
		(const char * const *)argv) != argc) {
	    bu_vls_printf(gedp->ged_result_str, "invalid color specification\n");
	    return BRLCAD_ERROR;
	}
	int *cls = (int *)(v->gv_s->gv_view_scale.gos_line_color);
	bu_color_to_rgb_ints(&c, &cls[0], &cls[1], &cls[2]);
	return BRLCAD_OK;
    }

    bu_vls_printf(gedp->ged_result_str, "invalid command\n");

    return BRLCAD_OK;
}

int
_fp_cmd_params(void *ds, int argc, const char **argv)
{
    if (_fp_cmd_msgs(ds, argc, argv, NULL, NULL)) {
	return BRLCAD_OK;
    }

    argc--; argv++;

    struct _ged_fp_info *gd = (struct _ged_fp_info *)ds;
    struct ged *gedp = gd->gedp;
    struct bview *v = gedp->ged_gvp;

    if (!argc) {
	if (gd->verbosity) {
	    bu_vls_printf(gedp->ged_result_str, "%d[%d] (%d/%d/%d) [%d %d %d %d %d %d]",
		    v->gv_s->gv_view_params.draw,
		    v->gv_s->gv_view_params.font_size,
		    V3ARGS(v->gv_s->gv_view_params.color),
		    v->gv_s->gv_view_params.draw_size,
		    v->gv_s->gv_view_params.draw_center,
		    v->gv_s->gv_view_params.draw_az,
		    v->gv_s->gv_view_params.draw_el,
		    v->gv_s->gv_view_params.draw_tw,
		    v->gv_s->gv_view_params.draw_fps
		    );
	} else {
	    bu_vls_printf(gedp->ged_result_str, "%d", v->gv_s->gv_view_params.draw);
	}
	return BRLCAD_OK;
    }

    if (argc == 1) {
	if (BU_STR_EQUAL("1", argv[0])) {
	    v->gv_s->gv_view_params.draw = 1;
	    return BRLCAD_OK;
	}
	if (BU_STR_EQUAL("0", argv[0])) {
	    v->gv_s->gv_view_params.draw = 0;
	    return BRLCAD_OK;
	}
	if (BU_STR_EQUAL("size", argv[0])) {
	    bu_vls_printf(gedp->ged_result_str, "%d", v->gv_s->gv_view_params.draw_size);
	    return BRLCAD_OK;
	}
	if (BU_STR_EQUAL("center", argv[0])) {
	    bu_vls_printf(gedp->ged_result_str, "%d", v->gv_s->gv_view_params.draw_center);
	    return BRLCAD_OK;
	}
	if (BU_STR_EQUAL("az", argv[0])) {
	    bu_vls_printf(gedp->ged_result_str, "%d", v->gv_s->gv_view_params.draw_az);
	    return BRLCAD_OK;
	}
	if (BU_STR_EQUAL("el", argv[0])) {
	    bu_vls_printf(gedp->ged_result_str, "%d", v->gv_s->gv_view_params.draw_el);
	    return BRLCAD_OK;
	}
	if (BU_STR_EQUAL("tw", argv[0])) {
	    bu_vls_printf(gedp->ged_result_str, "%d", v->gv_s->gv_view_params.draw_tw);
	    return BRLCAD_OK;
	}
	if (BU_STR_EQUAL("fps", argv[0])) {
	    bu_vls_printf(gedp->ged_result_str, "%d", v->gv_s->gv_view_params.draw_fps);
	    return BRLCAD_OK;
	}
	if (BU_STR_EQUAL("font_size", argv[0])) {
	    bu_vls_printf(gedp->ged_result_str, "%d", v->gv_s->gv_view_params.font_size);
	    return BRLCAD_OK;
	}
	bu_vls_printf(gedp->ged_result_str, "input %s is invalid\n", argv[0]);
	return BRLCAD_ERROR;
    }

    if (argc > 1) {
	if (BU_STR_EQUAL("color", argv[0])) {
	    argc--; argv++;
	    struct bu_color c;
	    if (bu_cmd_color_from_argv(&c, (size_t)argc,
		    (const char * const *)argv) != argc) {
		bu_vls_printf(gedp->ged_result_str, "invalid color specification\n");
		return BRLCAD_ERROR;
	    }
	    int *cls = (int *)(v->gv_s->gv_view_params.color);
	    bu_color_to_rgb_ints(&c, &cls[0], &cls[1], &cls[2]);
	    return BRLCAD_OK;
	}
	if (argc == 2 && BU_STR_EQUAL("size", argv[0]))  {
	    if (BU_STR_EQUAL("0", argv[1])) {
		v->gv_s->gv_view_params.draw_size = 0;
	    } else if (BU_STR_EQUAL("1", argv[1])) {
		v->gv_s->gv_view_params.draw_size = 1;
	    } else {
		bu_vls_printf(gedp->ged_result_str,
		    "value %s is invalid - valid values are 0 or 1\n", argv[1]);
		return BRLCAD_ERROR;
	    }
	    return BRLCAD_OK;
	}
	if (argc == 2 && BU_STR_EQUAL("center", argv[0]))  {
	    if (BU_STR_EQUAL("0", argv[1])) {
		v->gv_s->gv_view_params.draw_center = 0;
	    } else if (BU_STR_EQUAL("1", argv[1])) {
		v->gv_s->gv_view_params.draw_center = 1;
	    } else {
		bu_vls_printf(gedp->ged_result_str,
		    "value %s is invalid - valid values are 0 or 1\n", argv[1]);
		return BRLCAD_ERROR;
	    }
	    return BRLCAD_OK;
	}
	if (argc == 2 && BU_STR_EQUAL("az", argv[0]))  {
	    if (BU_STR_EQUAL("0", argv[1])) {
		v->gv_s->gv_view_params.draw_az = 0;
	    } else if (BU_STR_EQUAL("1", argv[1])) {
		v->gv_s->gv_view_params.draw_az = 1;
	    } else {
		bu_vls_printf(gedp->ged_result_str,
		    "value %s is invalid - valid values are 0 or 1\n", argv[1]);
		return BRLCAD_ERROR;
	    }
	    return BRLCAD_OK;
	}
	if (argc == 2 && BU_STR_EQUAL("el", argv[0]))  {
	    if (BU_STR_EQUAL("0", argv[1])) {
		v->gv_s->gv_view_params.draw_el = 0;
	    } else if (BU_STR_EQUAL("1", argv[1])) {
		v->gv_s->gv_view_params.draw_el = 1;
	    } else {
		bu_vls_printf(gedp->ged_result_str,
		    "value %s is invalid - valid values are 0 or 1\n", argv[1]);
		return BRLCAD_ERROR;
	    }
	    return BRLCAD_OK;
	}
	if (argc == 2 && BU_STR_EQUAL("tw", argv[0]))  {
	    if (BU_STR_EQUAL("0", argv[1])) {
		v->gv_s->gv_view_params.draw_tw = 0;
	    } else if (BU_STR_EQUAL("1", argv[1])) {
		v->gv_s->gv_view_params.draw_tw = 1;
	    } else {
		bu_vls_printf(gedp->ged_result_str,
		    "value %s is invalid - valid values are 0 or 1\n", argv[1]);
		return BRLCAD_ERROR;
	    }
	    return BRLCAD_OK;
	}
	if (argc == 2 && BU_STR_EQUAL("fps", argv[0]))  {
	    if (BU_STR_EQUAL("0", argv[1])) {
		v->gv_s->gv_view_params.draw_fps = 0;
	    } else if (BU_STR_EQUAL("1", argv[1])) {
		v->gv_s->gv_view_params.draw_fps = 1;
	    } else {
		bu_vls_printf(gedp->ged_result_str,
		    "value %s is invalid - valid values are 0 or 1\n", argv[1]);
		return BRLCAD_ERROR;
	    }
	    return BRLCAD_OK;
	}
	if (argc == 2 && BU_STR_EQUAL("font_size", argv[0])) {
	    int fsize;
	    if (!bu_cmd_integer_from_str(&fsize, argv[1])) {
		bu_vls_printf(gedp->ged_result_str, "invalid font size specification\n");
		return BRLCAD_ERROR;
	    }
	    v->gv_s->gv_view_params.font_size = fsize;
	    return BRLCAD_OK;
	}
	bu_vls_printf(gedp->ged_result_str, "unknown subcommand %s\n", argv[0]);
	return BRLCAD_ERROR;
    }

    bu_vls_printf(gedp->ged_result_str, "invalid command\n");

    return BRLCAD_OK;
}

const struct bu_cmdtab _fp_cmds[] = {
    { "center_dot",      _fp_cmd_center_dot},
    { "fb",              _fp_cmd_fb},
    { "grid",            _fp_cmd_grid},
    { "irect",           _fp_cmd_irect},
    { "model_axes",      _fp_cmd_model_axes},
    { "params",          _fp_cmd_params},
    { "scale",           _fp_cmd_scale},
    { "view_axes",       _fp_cmd_view_axes},
    { (char *)NULL,      NULL}
};

static const struct bu_cmd_operand fp_optional_bool_operands[] = {
    BU_CMD_OPERAND_INTEGER_RANGE("state", 0, 1, 0, 1,
	"New state; omit to query", NULL),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_operand fp_optional_fb_operands[] = {
    BU_CMD_OPERAND_INTEGER_RANGE("mode", 0, 1, 0, 2,
	"Framebuffer mode; omit to query", NULL),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_operand fp_optional_number_operands[] = {
    BU_CMD_OPERAND("value", BU_CMD_VALUE_NUMBER, 0, 1,
	"New numeric value; omit to query", NULL),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_operand fp_optional_integer_operands[] = {
    BU_CMD_OPERAND("value", BU_CMD_VALUE_INTEGER, 0, 1,
	"New integer value; omit to query", NULL),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_operand fp_optional_positive_integer_operands[] = {
    BU_CMD_OPERAND_INTEGER_MIN("value", 0, 1, 1,
	"New positive integer; omit to query", NULL),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_operand fp_optional_nonnegative_integer_operands[] = {
    BU_CMD_OPERAND_INTEGER_MIN("value", 0, 1, 0,
	"New nonnegative integer; omit to query", NULL),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_operand fp_optional_resolution_operands[] = {
    BU_CMD_OPERAND_NUMBER_MIN("value", 0, 1, BN_TOL_DIST,
	"New positive resolution; omit to query", NULL),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_operand fp_optional_vector3_operands[] = {
    BU_CMD_OPERAND_SHAPED("point", BU_CMD_VALUE_VECTOR, 0, 3, NULL,
	"New XYZ point; omit to query", NULL, &bu_cmd_vector3_arg_shape),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_arg_variant fp_vector2_forms[] = {
    BU_CMD_ARG_VARIANT("query", "", 0, "Omit coordinates to query"),
    BU_CMD_ARG_VARIANT("components", "x y", 2,
	"Two separate integer components"),
    BU_CMD_ARG_VARIANT_NULL
};
static const struct bu_cmd_arg_shape fp_vector2_shape =
    BU_CMD_ARG_SHAPE_FORMS(BU_CMD_ARG_SHAPE_TOKEN_SEQUENCE, 0, 2,
	"omitted query or two integer components", NULL, fp_vector2_forms);
static const struct bu_cmd_operand fp_optional_vector2_operands[] = {
    BU_CMD_OPERAND_SHAPED("position", BU_CMD_VALUE_INTEGER, 0, 2, NULL,
	"New two-dimensional value; omit to query", NULL, &fp_vector2_shape),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_operand fp_optional_color_operands[] = {
    BU_CMD_OPERAND_SHAPED("color", BU_CMD_VALUE_RAW, 0, 3, NULL,
	"New color; omit to query", NULL, &bu_cmd_color_arg_shape),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_arg_variant fp_overlay_forms[] = {
    BU_CMD_ARG_VARIANT("query", "", 0, "Report the current state"),
    BU_CMD_ARG_VARIANT("state", "0|1", 1, "Disable or enable the overlay"),
    BU_CMD_ARG_VARIANT("packed-color", "color color", 2,
	"Set a named, hexadecimal, or packed RGB color"),
    BU_CMD_ARG_VARIANT("component-color", "color r g b", 4,
	"Set three separate RGB components"),
    BU_CMD_ARG_VARIANT_NULL
};
static const struct bu_cmd_arg_shape fp_overlay_shape =
    BU_CMD_ARG_SHAPE_FORMS(BU_CMD_ARG_SHAPE_CUSTOM, 0, 4,
	"query, state, or color operation", NULL, fp_overlay_forms);
static const struct bu_cmd_operand fp_overlay_operands[] = {
    BU_CMD_OPERAND_SHAPED("operation", BU_CMD_VALUE_RAW, 0, 4, NULL,
	"Overlay query or update", NULL, &fp_overlay_shape),
    BU_CMD_OPERAND_NULL
};

static int
fp_discrete_count_validate(const struct bu_cmd_schema *schema, size_t argc,
	const char **argv, size_t cursor_arg, struct bu_cmd_validate_result *result,
	size_t count1, size_t count2, size_t count3)
{
    struct bu_cmd_schema flat = *schema;
    int ret;

    flat.validation.custom_validate = NULL;
    ret = bu_cmd_schema_validate_syntax(&flat, argc, argv, cursor_arg, result);
    if (ret || result->state == BU_CMD_VALIDATE_INVALID || cursor_arg < argc)
	return ret;
    if (argc != count1 && argc != count2 && argc != count3) {
	bu_cmd_validate_result_clear(result);
	result->state = BU_CMD_VALIDATE_INCOMPLETE;
	result->token_start = argc;
	result->token_end = argc;
	result->expected = BU_CMD_EXPECT_OPERAND;
	result->completion_type = BU_CMD_VALUE_UNKNOWN;
	result->hint = "additional operands required";
    }
    return 0;
}

static int
fp_vector2_validate(const struct bu_cmd_schema *schema, size_t argc,
	const char **argv, size_t cursor_arg, struct bu_cmd_validate_result *result)
{
    return fp_discrete_count_validate(schema, argc, argv, cursor_arg, result,
	0, 2, BU_CMD_COUNT_UNLIMITED);
}

static int
fp_vector3_validate(const struct bu_cmd_schema *schema, size_t argc,
	const char **argv, size_t cursor_arg, struct bu_cmd_validate_result *result)
{
    int ret = fp_discrete_count_validate(schema, argc, argv, cursor_arg, result,
	0, 1, 3);

    if (!ret && cursor_arg == argc && result->state == BU_CMD_VALIDATE_VALID &&
	argc) {
	vect_t value;
	if (bu_cmd_vector3_from_argv(value, argc,
		(const char * const *)argv) != (int)argc) {
	    bu_cmd_validate_result_clear(result);
	    result->state = BU_CMD_VALIDATE_INVALID;
	    result->token_start = argc - 1;
	    result->token_end = argc - 1;
	    result->expected = BU_CMD_EXPECT_OPERAND;
	    result->completion_type = BU_CMD_VALUE_VECTOR;
	    result->hint = "invalid vector";
	}
    }
    return ret;
}

static int
fp_color_validate(const struct bu_cmd_schema *schema, size_t argc,
	const char **argv, size_t cursor_arg, struct bu_cmd_validate_result *result)
{
    int ret = fp_discrete_count_validate(schema, argc, argv, cursor_arg, result,
	0, 1, 3);

    if (!ret && cursor_arg == argc && result->state == BU_CMD_VALIDATE_VALID &&
	argc) {
	struct bu_color color;
	if (bu_cmd_color_from_argv(&color, argc,
		(const char * const *)argv) != (int)argc) {
	    bu_cmd_validate_result_clear(result);
	    result->state = BU_CMD_VALIDATE_INVALID;
	    result->token_start = argc - 1;
	    result->token_end = argc - 1;
	    result->expected = BU_CMD_EXPECT_OPERAND;
	    result->completion_type = BU_CMD_VALUE_COLOR;
	    result->hint = "invalid color";
	}
    }
    return ret;
}

static int
fp_overlay_validate(const struct bu_cmd_schema *schema, size_t argc,
	const char **argv, size_t cursor_arg, struct bu_cmd_validate_result *result)
{
    struct bu_cmd_schema flat = *schema;
    int valid = 0;
    int ret;

    flat.validation.custom_validate = NULL;
    ret = bu_cmd_schema_validate_syntax(&flat, argc, argv, cursor_arg, result);
    if (ret || result->state == BU_CMD_VALIDATE_INVALID || cursor_arg < argc)
	return ret;
    if (!argc)
	return 0;
    if (argc == 1)
	valid = BU_STR_EQUAL(argv[0], "0") || BU_STR_EQUAL(argv[0], "1");
    if ((argc == 2 || argc == 4) && BU_STR_EQUAL(argv[0], "color")) {
	struct bu_color color;
	valid = bu_cmd_color_from_argv(&color, argc - 1,
	    (const char * const *)(argv + 1)) == (int)argc - 1;
    }
    if (!valid) {
	bu_cmd_validate_result_clear(result);
	result->state = (argc == 3) ? BU_CMD_VALIDATE_INCOMPLETE :
	    BU_CMD_VALIDATE_INVALID;
	result->token_start = argc ? argc - 1 : 0;
	result->token_end = result->token_start;
	result->expected = BU_CMD_EXPECT_OPERAND;
	result->completion_type = BU_CMD_VALUE_UNKNOWN;
	result->hint = (argc == 3) ? "one additional color component required" :
	    "expected 0, 1, or color followed by a valid color";
    }
    return 0;
}

#define FP_SCHEMA(_id, _name, _help, _operands, _validator) \
    static const struct bu_cmd_schema fp_##_id##_schema = { \
	_name, _help, NULL, _operands, BU_CMD_PARSE_STOP_AT_FIRST_OPERAND, \
	BU_CMD_SCHEMA_CONSTRAINTS(_validator, NULL) \
    }

#define FP_BRANCH_SCHEMA(_id, _name, _help) \
    static const struct bu_cmd_schema fp_##_id##_schema = { \
	_name, _help, fp_subcommand_options, NULL, BU_CMD_PARSE_OPTIONS_FIRST, \
	BU_CMD_SCHEMA_CONSTRAINTS(NULL, NULL) \
    }

FP_SCHEMA(center_dot, "center_dot", "Query or set the center dot",
    fp_overlay_operands, fp_overlay_validate);
FP_SCHEMA(fb, "fb", "Query or set framebuffer overlay behavior",
    fp_optional_fb_operands, NULL);
FP_SCHEMA(scale, "scale", "Query or set the scale overlay",
    fp_overlay_operands, fp_overlay_validate);
FP_BRANCH_SCHEMA(grid, "grid", "Manage the faceplate grid");
FP_BRANCH_SCHEMA(irect, "irect", "Manage the interactive rectangle");
FP_BRANCH_SCHEMA(model_axes, "model_axes", "Manage model axes");
FP_BRANCH_SCHEMA(view_axes, "view_axes", "Manage view axes");
FP_BRANCH_SCHEMA(params, "params", "Manage view parameter overlay");

FP_SCHEMA(grid_draw, "draw", "Query or set grid drawing", fp_optional_bool_operands, NULL);
FP_SCHEMA(grid_snap, "snap", "Query or set grid snapping", fp_optional_bool_operands, NULL);
FP_SCHEMA(grid_anchor, "anchor", "Query or set the grid anchor", fp_optional_vector3_operands, fp_vector3_validate);
FP_SCHEMA(grid_res_h, "res_h", "Query or set horizontal grid resolution",
    fp_optional_resolution_operands, NULL);
FP_SCHEMA(grid_res_v, "res_v", "Query or set vertical grid resolution",
    fp_optional_resolution_operands, NULL);
FP_SCHEMA(grid_res_major_h, "res_major_h",
    "Query or set major horizontal grid resolution",
    fp_optional_positive_integer_operands, NULL);
FP_SCHEMA(grid_res_major_v, "res_major_v",
    "Query or set major vertical grid resolution",
    fp_optional_positive_integer_operands, NULL);
FP_SCHEMA(grid_color, "color", "Query or set grid color", fp_optional_color_operands, fp_color_validate);

FP_SCHEMA(irect_draw, "draw", "Query or set rectangle drawing", fp_optional_bool_operands, NULL);
FP_SCHEMA(irect_line_width, "line_width", "Query or set rectangle line width",
    fp_optional_positive_integer_operands, NULL);
FP_SCHEMA(irect_line_style, "line_style", "Query or set rectangle line style",
    fp_optional_positive_integer_operands, NULL);
FP_SCHEMA(irect_pos, "pos", "Query or set rectangle position", fp_optional_vector2_operands, fp_vector2_validate);
FP_SCHEMA(irect_dim, "dim", "Query or set rectangle dimensions", fp_optional_vector2_operands, fp_vector2_validate);
FP_SCHEMA(irect_x, "x", "Query rectangle X position", NULL, NULL);
FP_SCHEMA(irect_y, "y", "Query rectangle Y position", NULL, NULL);
FP_SCHEMA(irect_width, "width", "Query rectangle width", NULL, NULL);
FP_SCHEMA(irect_height, "height", "Query rectangle height", NULL, NULL);
FP_SCHEMA(irect_bg, "bg", "Query or set rectangle background color",
    fp_optional_color_operands, fp_color_validate);
FP_SCHEMA(irect_color, "color", "Query or set rectangle color", fp_optional_color_operands, fp_color_validate);
FP_SCHEMA(irect_cdim, "cdim", "Query or set rectangle center dimensions", fp_optional_vector2_operands, fp_vector2_validate);

FP_SCHEMA(axes_size, "size", "Query or set axes size", fp_optional_number_operands, NULL);
FP_SCHEMA(axes_line_width, "line_width", "Query or set axes line width",
    fp_optional_positive_integer_operands, NULL);
FP_SCHEMA(axes_pos_only, "pos_only", "Query or set axes position-only mode", fp_optional_bool_operands, NULL);
FP_SCHEMA(axes_color, "axes_color", "Query or set axes color",
    fp_optional_color_operands, fp_color_validate);
FP_SCHEMA(axes_label, "label", "Query or set axes labels", fp_optional_bool_operands, NULL);
FP_SCHEMA(axes_label_color, "label_color", "Query or set axes label color", fp_optional_color_operands, fp_color_validate);
FP_SCHEMA(axes_triple_color, "triple_color", "Query or set axes triple-color mode", fp_optional_bool_operands, NULL);
FP_SCHEMA(axes_tick, "tick", "Query or set axes ticks", fp_optional_bool_operands, NULL);
FP_SCHEMA(axes_tick_length, "tick_length", "Query or set tick length",
    fp_optional_positive_integer_operands, NULL);
FP_SCHEMA(axes_tick_major_length, "tick_major_length",
    "Query or set major tick length", fp_optional_positive_integer_operands,
    NULL);
FP_SCHEMA(axes_tick_interval, "tick_interval", "Query or set tick interval", fp_optional_number_operands, NULL);
FP_SCHEMA(axes_ticks_per_major, "ticks_per_major",
    "Query or set ticks per major interval",
    fp_optional_nonnegative_integer_operands, NULL);
FP_SCHEMA(axes_tick_threshold, "tick_threshold", "Query or set tick threshold",
    fp_optional_nonnegative_integer_operands, NULL);
FP_SCHEMA(axes_tick_color, "tick_color", "Query or set tick color", fp_optional_color_operands, fp_color_validate);
FP_SCHEMA(axes_tick_major_color, "tick_major_color", "Query or set major tick color", fp_optional_color_operands, fp_color_validate);

FP_SCHEMA(params_color, "color", "Query or set parameter overlay color", fp_optional_color_operands, fp_color_validate);
FP_SCHEMA(params_size, "size", "Query or set size display", fp_optional_bool_operands, NULL);
FP_SCHEMA(params_center, "center", "Query or set center display", fp_optional_bool_operands, NULL);
FP_SCHEMA(params_az, "az", "Query or set azimuth display", fp_optional_bool_operands, NULL);
FP_SCHEMA(params_el, "el", "Query or set elevation display", fp_optional_bool_operands, NULL);
FP_SCHEMA(params_tw, "tw", "Query or set twist display", fp_optional_bool_operands, NULL);
FP_SCHEMA(params_fps, "fps", "Query or set frames-per-second display", fp_optional_bool_operands, NULL);
FP_SCHEMA(params_font_size, "font_size", "Query or set parameter font size", fp_optional_integer_operands, NULL);

#undef FP_SCHEMA
#undef FP_BRANCH_SCHEMA

#define FP_TREE_LEAF(_id) \
    BU_CMD_TREE_NODE(&fp_##_id##_schema, NULL, NULL, \
	BU_CMD_TREE_CHILD_AFTER_OPTIONS, NULL)

static const struct bu_cmd_tree_node fp_grid_subcommands[] = {
    FP_TREE_LEAF(grid_draw), FP_TREE_LEAF(grid_snap), FP_TREE_LEAF(grid_anchor),
    FP_TREE_LEAF(grid_res_h), FP_TREE_LEAF(grid_res_v), FP_TREE_LEAF(grid_res_major_h),
    FP_TREE_LEAF(grid_res_major_v), FP_TREE_LEAF(grid_color), BU_CMD_TREE_NODE_NULL
};
static const struct bu_cmd_tree_node fp_irect_subcommands[] = {
    FP_TREE_LEAF(irect_draw), FP_TREE_LEAF(irect_line_width), FP_TREE_LEAF(irect_line_style),
    FP_TREE_LEAF(irect_pos), FP_TREE_LEAF(irect_dim), FP_TREE_LEAF(irect_x),
    FP_TREE_LEAF(irect_y), FP_TREE_LEAF(irect_width), FP_TREE_LEAF(irect_height),
    FP_TREE_LEAF(irect_bg), FP_TREE_LEAF(irect_color), FP_TREE_LEAF(irect_cdim),
    BU_CMD_TREE_NODE_NULL
};
static const struct bu_cmd_tree_node fp_axes_subcommands[] = {
    FP_TREE_LEAF(axes_size), FP_TREE_LEAF(axes_line_width), FP_TREE_LEAF(axes_pos_only),
    FP_TREE_LEAF(axes_color), FP_TREE_LEAF(axes_label), FP_TREE_LEAF(axes_label_color),
    FP_TREE_LEAF(axes_triple_color), FP_TREE_LEAF(axes_tick), FP_TREE_LEAF(axes_tick_length),
    FP_TREE_LEAF(axes_tick_major_length), FP_TREE_LEAF(axes_tick_interval),
    FP_TREE_LEAF(axes_ticks_per_major), FP_TREE_LEAF(axes_tick_threshold),
    FP_TREE_LEAF(axes_tick_color), FP_TREE_LEAF(axes_tick_major_color), BU_CMD_TREE_NODE_NULL
};
static const struct bu_cmd_tree_node fp_params_subcommands[] = {
    FP_TREE_LEAF(params_color), FP_TREE_LEAF(params_size), FP_TREE_LEAF(params_center),
    FP_TREE_LEAF(params_az), FP_TREE_LEAF(params_el), FP_TREE_LEAF(params_tw),
    FP_TREE_LEAF(params_fps), FP_TREE_LEAF(params_font_size), BU_CMD_TREE_NODE_NULL
};

const struct bu_cmd_tree_node ged_faceplate_subcommands[] = {
    FP_TREE_LEAF(center_dot), FP_TREE_LEAF(fb),
    BU_CMD_TREE_NODE(&fp_grid_schema, NULL, fp_grid_subcommands,
	BU_CMD_TREE_CHILD_AFTER_OPTIONS, NULL),
    BU_CMD_TREE_NODE(&fp_irect_schema, NULL, fp_irect_subcommands,
	BU_CMD_TREE_CHILD_AFTER_OPTIONS, NULL),
    BU_CMD_TREE_NODE(&fp_model_axes_schema, NULL, fp_axes_subcommands,
	BU_CMD_TREE_CHILD_AFTER_OPTIONS, NULL),
    BU_CMD_TREE_NODE(&fp_params_schema, NULL, fp_params_subcommands,
	BU_CMD_TREE_CHILD_AFTER_OPTIONS, NULL),
    FP_TREE_LEAF(scale),
    BU_CMD_TREE_NODE(&fp_view_axes_schema, NULL, fp_axes_subcommands,
	BU_CMD_TREE_CHILD_AFTER_OPTIONS, NULL),
    BU_CMD_TREE_NODE_NULL
};

const struct bu_cmd_tree ged_faceplate_tree = {
    &ged_faceplate_native_schema, ged_faceplate_subcommands,
    BU_CMD_TREE_CHILD_AFTER_OPTIONS
};

#undef FP_TREE_LEAF

static const struct bu_cmd_tree_node *
fp_cmd_schema_node(const char *branch, const char *leaf)
{
    const struct bu_cmd_tree_node *node = NULL;

    if (branch)
	node = bu_cmd_tree_find_subcommand(&ged_faceplate_tree, branch);
    else if (leaf)
	node = bu_cmd_tree_find_subcommand(&ged_faceplate_tree, leaf);
    if (!node || !branch || !leaf)
	return node;

    for (const struct bu_cmd_tree_node *child = node->subcommands;
	    child && child->schema; child++) {
	if (BU_STR_EQUAL(child->schema->name, leaf))
	    return child;
	for (size_t i = 0; child->aliases && child->aliases[i]; i++)
	    if (BU_STR_EQUAL(child->aliases[i], leaf))
		return child;
    }
    return NULL;
}

void
_fp_cmd_schema_help(struct ged *gedp, const char *branch, const char *leaf)
{
    const char *path[2] = {branch ? branch : leaf, branch ? leaf : NULL};
    size_t path_argc = branch ? 2 : 1;
    char *help;

    if (!gedp || !leaf)
	return;
    help = bu_cmd_tree_help_path(&ged_faceplate_tree, "view faceplate",
	path_argc, path);
    if (!help)
	return;
    bu_vls_strcat(gedp->ged_result_str, help);
    bu_free(help, "faceplate schema help");
}

int
_fp_cmd_schema_msgs(struct ged *gedp, int argc, const char **argv,
	const char *branch)
{
    const struct bu_cmd_tree_node *node;

    if (!gedp || argc != 2 || !argv)
	return 0;
    if (!BU_STR_EQUAL(argv[1], HELPFLAG) &&
	!BU_STR_EQUAL(argv[1], PURPOSEFLAG))
	return 0;

    node = fp_cmd_schema_node(branch, argv[0]);
    if (BU_STR_EQUAL(argv[1], HELPFLAG))
	_fp_cmd_schema_help(gedp, branch, argv[0]);
    else
	bu_vls_printf(gedp->ged_result_str, "%s\n",
	    (node && node->schema && node->schema->help) ?
	    node->schema->help : "");
    return 1;
}

int
ged_faceplate_core(struct ged *gedp, int argc, const char *argv[])
{
    struct fp_root_args args = {0};
    struct _ged_fp_info gd;
    gd.gedp = gedp;
    gd.cmds = _fp_cmds;
    gd.gopts = NULL;
    gd.verbosity = 0;

    // Sanity
    if (UNLIKELY(!gedp || !argc || !argv)) {
	return BRLCAD_ERROR;
    }

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    // We know we're the faceplate command - start processing args
    argc--; argv++;

    struct bu_vls parse_msgs = BU_VLS_INIT_ZERO;
    int subcommand_index = bu_cmd_schema_parse(&ged_faceplate_native_schema, &args,
	&parse_msgs, argc, argv);
    if (subcommand_index < 0) {
	bu_vls_printf(gedp->ged_result_str, "%s", bu_vls_addr(&parse_msgs));
	bu_vls_free(&parse_msgs);
	return BRLCAD_ERROR;
    }
    bu_vls_free(&parse_msgs);
    gd.verbosity = args.verbosity;

    argc -= subcommand_index;
    argv += subcommand_index;

    if (args.help || !argc) {
	return _fp_subcmd_exec(gedp, &ged_faceplate_native_schema, _fp_cmds,
	    "view faceplate", "[options] subcommand [args]", &gd, argc, argv, args.help);
    }

    if (!gedp->ged_gvp) {
	bu_vls_printf(gedp->ged_result_str, ": no current view set");
	return BRLCAD_ERROR;
    }

    return _fp_subcmd_exec(gedp, &ged_faceplate_native_schema, _fp_cmds,
	"view faceplate", "[options] subcommand [args]", &gd, argc, argv, 0);
}

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
