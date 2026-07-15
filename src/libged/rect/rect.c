/*                          R E C T . C
 * BRL-CAD
 *
 * Copyright (c) 1998-2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file libged/rect.c
 *
 * Rubber band rectangle.
 *
 */

#include "common.h"

#include <string.h>
#include <math.h>


#include "vmath.h"
#include "bu/cmdschema.h"
#include "bu/color.h"

#include "../ged_private.h"


static char *rect_native_help(void);


static void
rect_usage(struct ged *gedp, const char *UNUSED(argv0))
{
    char *help = rect_native_help();

    if (help) {
	bu_vls_strcat(gedp->ged_result_str, help);
	bu_free(help, "rect native tree help");
    }
}


static void
rect_vls_print(struct ged *gedp)
{
    bu_vls_printf(gedp->ged_result_str, "bg = %d %d %d\n",
		  gedp->ged_gvp->gv_s->gv_rect.bg[0],
		  gedp->ged_gvp->gv_s->gv_rect.bg[1],
		  gedp->ged_gvp->gv_s->gv_rect.bg[2]);
    bu_vls_printf(gedp->ged_result_str, "cdim = %d %d\n",
		  gedp->ged_gvp->gv_s->gv_rect.cdim[X],
		  gedp->ged_gvp->gv_s->gv_rect.cdim[Y]);
    bu_vls_printf(gedp->ged_result_str, "color = %d %d %d\n",
		  gedp->ged_gvp->gv_s->gv_rect.color[0],
		  gedp->ged_gvp->gv_s->gv_rect.color[1],
		  gedp->ged_gvp->gv_s->gv_rect.color[2]);
    bu_vls_printf(gedp->ged_result_str, "dim = %d %d\n",
		  gedp->ged_gvp->gv_s->gv_rect.dim[X],
		  gedp->ged_gvp->gv_s->gv_rect.dim[Y]);
    bu_vls_printf(gedp->ged_result_str, "draw = %d\n", gedp->ged_gvp->gv_s->gv_rect.draw);
    bu_vls_printf(gedp->ged_result_str, "lstyle = %d\n", gedp->ged_gvp->gv_s->gv_rect.line_style);
    bu_vls_printf(gedp->ged_result_str, "lwidth = %d\n", gedp->ged_gvp->gv_s->gv_rect.line_width);
    bu_vls_printf(gedp->ged_result_str, "pos = %d %d\n",
		  gedp->ged_gvp->gv_s->gv_rect.pos[X],
		  gedp->ged_gvp->gv_s->gv_rect.pos[Y]);
}


/*
 * Given position and dimensions in image coordinates, calculate
 * position and dimensions in normalized view coordinates.
 */
static void
rect_image2view(struct bv_interactive_rect_state *grsp)
{
    grsp->x = (grsp->pos[X] / (fastf_t)grsp->cdim[X] - 0.5) * 2.0;
    grsp->y = ((0.5 - (grsp->cdim[Y] - grsp->pos[Y]) / (fastf_t)grsp->cdim[Y]) / grsp->aspect * 2.0);
    grsp->width = grsp->dim[X] * 2.0 / (fastf_t)grsp->cdim[X];
    grsp->height = grsp->dim[Y] * 2.0 / (fastf_t)grsp->cdim[X];
}


/*
 * Adjust the rubber band rectangle to have the same aspect ratio as the window.
 */
static void
rect_adjust_for_zoom(struct bv_interactive_rect_state *grsp)
{
    fastf_t width, height;

    if (grsp->width >= 0.0)
	width = grsp->width;
    else
	width = -grsp->width;

    if (grsp->height >= 0.0)
	height = grsp->height;
    else
	height = -grsp->height;

    if (width >= height) {
	if (grsp->height >= 0.0)
	    grsp->height = width / grsp->aspect;
	else
	    grsp->height = -width / grsp->aspect;
    } else {
	if (grsp->width >= 0.0)
	    grsp->width = height * grsp->aspect;
	else
	    grsp->width = -height * grsp->aspect;
    }
}


static int
rect_rt(struct ged *gedp, int port)
{
    int xmin, xmax;
    int ymin, ymax;

    /* initialize result in case we need to report something here */
    bu_vls_trunc(gedp->ged_result_str, 0);

    if (ZERO(gedp->ged_gvp->gv_s->gv_rect.width) &&
	ZERO(gedp->ged_gvp->gv_s->gv_rect.height))
	return BRLCAD_OK;

    if (port < 0) {
	bu_vls_printf(gedp->ged_result_str, "rect_rt: invalid port number - %d\n", port);
	return BRLCAD_ERROR;
    }

    xmin = gedp->ged_gvp->gv_s->gv_rect.pos[X];
    ymin = gedp->ged_gvp->gv_s->gv_rect.pos[Y];

    if (gedp->ged_gvp->gv_s->gv_rect.dim[X] >= 0) {
	xmax = xmin + gedp->ged_gvp->gv_s->gv_rect.dim[X];
    } else {
	xmax = xmin;
	xmin += gedp->ged_gvp->gv_s->gv_rect.dim[X];
    }

    if (gedp->ged_gvp->gv_s->gv_rect.dim[Y] >= 0) {
	ymax = ymin + gedp->ged_gvp->gv_s->gv_rect.dim[Y];
    } else {
	ymax = ymin;
	ymin += gedp->ged_gvp->gv_s->gv_rect.dim[Y];
    }

    {
	int ret;
	struct bu_vls wvls = BU_VLS_INIT_ZERO;
	struct bu_vls nvls = BU_VLS_INIT_ZERO;
	struct bu_vls vvls = BU_VLS_INIT_ZERO;
	struct bu_vls fvls = BU_VLS_INIT_ZERO;
	struct bu_vls jvls = BU_VLS_INIT_ZERO;
	struct bu_vls cvls = BU_VLS_INIT_ZERO;
	char *av[14];

	bu_vls_printf(&wvls, "%d", gedp->ged_gvp->gv_s->gv_rect.cdim[X]);
	bu_vls_printf(&nvls, "%d", gedp->ged_gvp->gv_s->gv_rect.cdim[Y]);
	bu_vls_printf(&vvls, "%lf", gedp->ged_gvp->gv_s->gv_rect.aspect);
	bu_vls_printf(&fvls, "%d", port);
	bu_vls_printf(&jvls, "%d, %d, %d, %d", xmin, ymin, xmax, ymax);
	bu_vls_printf(&cvls, "%d/%d/%d",
		      gedp->ged_gvp->gv_s->gv_rect.bg[0],
		      gedp->ged_gvp->gv_s->gv_rect.bg[1],
		      gedp->ged_gvp->gv_s->gv_rect.bg[2]);

	av[0] = "rt";
	av[1] = "-w";
	av[2] = bu_vls_addr(&wvls);
	av[3] = "-n";
	av[4] = bu_vls_addr(&nvls);
	av[5] = "-V";
	av[6] = bu_vls_addr(&vvls);
	av[7] = "-F";
	av[8] = bu_vls_addr(&fvls);
	av[9] = "-j";
	av[10] = bu_vls_addr(&jvls);
	av[11] = "-C";
	av[12] = bu_vls_addr(&cvls);
	av[13] = (char *)0;

	ret = ged_exec_rt(gedp, 13, (const char **)av);

	bu_vls_free(&wvls);
	bu_vls_free(&nvls);
	bu_vls_free(&vvls);
	bu_vls_free(&fvls);
	bu_vls_free(&jvls);
	bu_vls_free(&cvls);

	return ret;
    }
}


static int
rect_zoom(struct ged *gedp)
{
    fastf_t width, height;
    fastf_t sf;
    point_t old_model_center;
    point_t new_model_center;
    point_t old_view_center;
    point_t new_view_center;

    GED_CHECK_VIEW(gedp, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    if (ZERO(gedp->ged_gvp->gv_s->gv_rect.width) &&
	ZERO(gedp->ged_gvp->gv_s->gv_rect.height))
	return BRLCAD_OK;

    rect_adjust_for_zoom(&gedp->ged_gvp->gv_s->gv_rect);

    /* find old view center */
    MAT_DELTAS_GET_NEG(old_model_center, gedp->ged_gvp->gv_center);
    MAT4X3PNT(old_view_center, gedp->ged_gvp->gv_model2view, old_model_center);

    /* calculate new view center */
    VSET(new_view_center,
	 gedp->ged_gvp->gv_s->gv_rect.x + gedp->ged_gvp->gv_s->gv_rect.width / 2.0,
	 gedp->ged_gvp->gv_s->gv_rect.y + gedp->ged_gvp->gv_s->gv_rect.height / 2.0,
	 old_view_center[Z]);

    /* find new model center */
    MAT4X3PNT(new_model_center, gedp->ged_gvp->gv_view2model, new_view_center);

    /* zoom in to fill rectangle */
    if (gedp->ged_gvp->gv_s->gv_rect.width >= 0.0)
	width = gedp->ged_gvp->gv_s->gv_rect.width;
    else
	width = -gedp->ged_gvp->gv_s->gv_rect.width;

    if (gedp->ged_gvp->gv_s->gv_rect.height >= 0.0)
	height = gedp->ged_gvp->gv_s->gv_rect.height;
    else
	height = -gedp->ged_gvp->gv_s->gv_rect.height;

    if (width >= height)
	sf = width / 2.0;
    else
	sf = height / 2.0 * gedp->ged_gvp->gv_s->gv_rect.aspect;

    if (sf <= SMALL_FASTF || INFINITY < sf)
	return BRLCAD_OK;

    /* set the new model center */
    MAT_DELTAS_VEC_NEG(gedp->ged_gvp->gv_center, new_model_center);
    gedp->ged_gvp->gv_scale *= sf;
    bv_update(gedp->ged_gvp);

    return BRLCAD_OK;
}


/*
 * Note - this needs to be rewritten to accept keyword/value pairs so
 * that multiple attributes can be set with a single command call.
 */
static int
rect_execute(struct ged *gedp,
	 int argc,
	 const char *argv[])
{
    const char *command = NULL;
    const char *parameter;
    const char **argp = argv;

    GED_CHECK_VIEW(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    command = argv[0];

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    if (argc < 2 || 5 < argc) {
	rect_usage(gedp, argv[0]);
	return BRLCAD_ERROR;
    }

#if 0
    // Archer has at least one selection mode (Modes->Comp Select Mode->List)
    // that uses rect - uncomment this to follow along with the commands it
    // issues as the GUI works.
    struct bu_vls rcmd = BU_VLS_INIT_ZERO;
    for (i = 0; i < argc; i++) {
	bu_vls_printf(&rcmd, "%s " , argv[i]);
    }
    bu_log("%s\n", bu_vls_cstr(&rcmd));
    bu_vls_free(&rcmd);
#endif

    parameter = argv[1];
    argc -= 2;
    argp += 2;

    if (BU_STR_EQUAL(parameter, "draw")) {
	if (argc == 0) {
	    bu_vls_printf(gedp->ged_result_str, "%d", gedp->ged_gvp->gv_s->gv_rect.draw);
	    return BRLCAD_OK;
	} else if (argc == 1) {
	    int enabled = 0;
	    if (!bu_cmd_integer_from_str(&enabled, argp[0]) || (enabled != 0 && enabled != 1))
		goto usage;
	    gedp->ged_gvp->gv_s->gv_rect.draw = enabled;

	    return BRLCAD_OK;
	}

	bu_vls_printf(gedp->ged_result_str, "The '%s draw' command accepts 0 or 1 argument\n", command);
	return BRLCAD_ERROR;
    }

    if (BU_STR_EQUAL(parameter, "cdim")) {
	if (argc == 0) {
	    bu_vls_printf(gedp->ged_result_str, "%d %d",
			  gedp->ged_gvp->gv_s->gv_rect.cdim[X],
			  gedp->ged_gvp->gv_s->gv_rect.cdim[Y]);
	    return BRLCAD_OK;
	} else if (argc == 2) {
	    int value[2] = {0, 0};
	    if (bu_cmd_integer_pair_from_argv(value, 2, (const char * const *)argp) != 2 ||
		value[X] <= 0 || value[Y] <= 0)
		goto usage;
	    gedp->ged_gvp->gv_s->gv_rect.cdim[X] = value[X];
	    gedp->ged_gvp->gv_s->gv_rect.cdim[Y] = value[Y];
	    gedp->ged_gvp->gv_s->gv_rect.aspect = (fastf_t)gedp->ged_gvp->gv_s->gv_rect.cdim[X] / gedp->ged_gvp->gv_s->gv_rect.cdim[Y];

	    rect_image2view(&gedp->ged_gvp->gv_s->gv_rect);

	    return BRLCAD_OK;
	}

	bu_vls_printf(gedp->ged_result_str, "The '%s cdim' command requires 0 or 2 arguments\n", command);
	return BRLCAD_ERROR;
    }

    if (BU_STR_EQUAL(parameter, "dim")) {
	if (argc == 0) {
	    bu_vls_printf(gedp->ged_result_str, "%d %d",
			  gedp->ged_gvp->gv_s->gv_rect.dim[X],
			  gedp->ged_gvp->gv_s->gv_rect.dim[Y]);
	    return BRLCAD_OK;
	} else if (argc == 2) {
	    int value[2] = {0, 0};
	    if (bu_cmd_integer_pair_from_argv(value, 2, (const char * const *)argp) != 2)
		goto usage;
	    gedp->ged_gvp->gv_s->gv_rect.dim[X] = value[X];
	    gedp->ged_gvp->gv_s->gv_rect.dim[Y] = value[Y];

	    rect_image2view(&gedp->ged_gvp->gv_s->gv_rect);

	    return BRLCAD_OK;
	}

	bu_vls_printf(gedp->ged_result_str, "The '%s dim' command requires 0 or 2 arguments\n", command);
	return BRLCAD_ERROR;
    }

    if (BU_STR_EQUAL(parameter, "pos")) {
	if (argc == 0) {
	    bu_vls_printf(gedp->ged_result_str, "%d %d",
			  gedp->ged_gvp->gv_s->gv_rect.pos[X],
			  gedp->ged_gvp->gv_s->gv_rect.pos[Y]);
	    return BRLCAD_OK;
	} else if (argc == 2) {
	    int value[2] = {0, 0};
	    if (bu_cmd_integer_pair_from_argv(value, 2, (const char * const *)argp) != 2)
		goto usage;
	    gedp->ged_gvp->gv_s->gv_rect.pos[X] = value[X];
	    gedp->ged_gvp->gv_s->gv_rect.pos[Y] = value[Y];

	    rect_image2view(&gedp->ged_gvp->gv_s->gv_rect);

	    return BRLCAD_OK;
	}

	bu_vls_printf(gedp->ged_result_str, "The '%s pos' command requires 0 or 2 arguments\n", command);
	return BRLCAD_ERROR;
    }

    if (BU_STR_EQUAL(parameter, "bg")) {
	if (argc == 0) {
	    bu_vls_printf(gedp->ged_result_str, "%d %d %d",
			  gedp->ged_gvp->gv_s->gv_rect.bg[X],
			  gedp->ged_gvp->gv_s->gv_rect.bg[Y],
			  gedp->ged_gvp->gv_s->gv_rect.bg[Z]);
	    return BRLCAD_OK;
	} else if (argc == 1 || argc == 3) {
	    unsigned char rgb[3] = {0, 0, 0};
	    if (bu_rgb_from_argv(rgb, (size_t)argc, (const char * const *)argp) != argc)
		goto usage;
	    gedp->ged_gvp->gv_s->gv_rect.bg[0] = rgb[RED];
	    gedp->ged_gvp->gv_s->gv_rect.bg[1] = rgb[GRN];
	    gedp->ged_gvp->gv_s->gv_rect.bg[2] = rgb[BLU];

	    return BRLCAD_OK;
	}

	bu_vls_printf(gedp->ged_result_str, "The '%s bg' command requires 0 or 3 arguments\n", command);
	return BRLCAD_ERROR;
    }

    if (BU_STR_EQUAL(parameter, "color")) {
	if (argc == 0) {
	    bu_vls_printf(gedp->ged_result_str, "%d %d %d",
			  gedp->ged_gvp->gv_s->gv_rect.color[X],
			  gedp->ged_gvp->gv_s->gv_rect.color[Y],
			  gedp->ged_gvp->gv_s->gv_rect.color[Z]);
	    return BRLCAD_OK;
	} else if (argc == 1 || argc == 3) {
	    unsigned char rgb[3] = {0, 0, 0};
	    if (bu_rgb_from_argv(rgb, (size_t)argc, (const char * const *)argp) != argc)
		goto usage;
	    gedp->ged_gvp->gv_s->gv_rect.color[0] = rgb[RED];
	    gedp->ged_gvp->gv_s->gv_rect.color[1] = rgb[GRN];
	    gedp->ged_gvp->gv_s->gv_rect.color[2] = rgb[BLU];

	    return BRLCAD_OK;
	}

	bu_vls_printf(gedp->ged_result_str, "The '%s color' command requires 0 or 3 arguments\n", command);
	return BRLCAD_ERROR;
    }

    if (BU_STR_EQUAL(parameter, "lstyle")) {
	if (argc == 0) {
	    bu_vls_printf(gedp->ged_result_str, "%d", gedp->ged_gvp->gv_s->gv_rect.line_style);
	    return BRLCAD_OK;
	} else if (argc == 1) {
	    int value = 0;
	    if (!bu_cmd_integer_from_str(&value, argp[0]))
		goto usage;

	    if (value <= 0)
		gedp->ged_gvp->gv_s->gv_rect.line_style = 0;
	    else
		gedp->ged_gvp->gv_s->gv_rect.line_style = 1;

	    return BRLCAD_OK;
	}

	bu_vls_printf(gedp->ged_result_str, "The '%s lstyle' command accepts 0 or 1 argument\n", command);
	return BRLCAD_ERROR;
    }

    if (BU_STR_EQUAL(parameter, "lwidth")) {
	if (argc == 0) {
	    bu_vls_printf(gedp->ged_result_str, "%d", gedp->ged_gvp->gv_s->gv_rect.line_width);
	    return BRLCAD_OK;
	} else if (argc == 1) {
	    int value = 0;
	    if (!bu_cmd_integer_from_str(&value, argp[0]))
		goto usage;

	    if (value <= 0)
		gedp->ged_gvp->gv_s->gv_rect.line_width = 0;
	    else
		gedp->ged_gvp->gv_s->gv_rect.line_width = value;

	    return BRLCAD_OK;
	}

	bu_vls_printf(gedp->ged_result_str, "The '%s lwidth' command accepts 0 or 1 argument\n", command);
	return BRLCAD_ERROR;
    }

    if (BU_STR_EQUAL(parameter, "rt")) {
	if (argc == 1) {
	    int port = 0;
	    if (bu_cmd_integer_from_str(&port, argp[0]))
		return rect_rt(gedp, port);
	    goto usage;
	}

	bu_vls_printf(gedp->ged_result_str, "The '%s rt' command accepts 1 argument\n", command);
	return BRLCAD_ERROR;
    }

    if (BU_STR_EQUAL(parameter, "zoom")) {
	if (argc == 0)
	    return rect_zoom(gedp);

	bu_vls_printf(gedp->ged_result_str, "The '%s zoom' command accepts no arguments\n", command);
	return BRLCAD_ERROR;
    }

    if (BU_STR_EQUAL(parameter, "vars")) {
	if (argc)
	    goto usage;
	rect_vls_print(gedp);
	return BRLCAD_OK;
    }

    if (BU_STR_EQUAL(parameter, "help")) {
	if (argc)
	    goto usage;
	rect_usage(gedp, command);
	return GED_HELP;
    }

    bu_vls_printf(gedp->ged_result_str, "%s: unrecognized command '%s'\n", command, parameter);

usage:
    rect_usage(gedp, command);

    return BRLCAD_ERROR;
}


#include "../include/plugin.h"

static const char * const rect_bool_keywords[] = {"0", "1", NULL};
static const struct bu_cmd_operand rect_bool_operands[] = {
    BU_CMD_OPERAND_KEYWORDS("enabled", BU_CMD_VALUE_KEYWORD, 0, 1,
	"Zero to disable or one to enable", NULL, rect_bool_keywords),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_operand rect_integer_operands[] = {
    BU_CMD_OPERAND("value", BU_CMD_VALUE_INTEGER, 0, 1,
	"Optional base-zero integer", NULL),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_arg_shape rect_integer_pair_shape =
    BU_CMD_ARG_SHAPE(BU_CMD_ARG_SHAPE_TOKEN_SEQUENCE, 2, 2, "two integer components");
static const struct bu_cmd_operand rect_integer_pair_operands[] = {
    BU_CMD_OPERAND_SHAPED("coordinates", BU_CMD_VALUE_INTEGER, 0, 2, NULL,
	"Optional pair of integer components", NULL, &rect_integer_pair_shape),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_operand rect_color_operands[] = {
    BU_CMD_OPERAND_SHAPED("color", BU_CMD_VALUE_COLOR, 0, 3, NULL,
	"Optional packed r/g/b or three RGB channels", NULL, &bu_cmd_rgb_arg_shape),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_operand rect_port_operands[] = {
    BU_CMD_OPERAND_VALIDATE("port", BU_CMD_VALUE_INTEGER, 1, 1,
	bu_cmd_nonnegative_integer_validate, "Nonnegative framebuffer port", NULL),
    BU_CMD_OPERAND_NULL
};
static int
rect_integer_pair_schema_validate(const struct bu_cmd_schema *UNUSED(schema), size_t argc,
	const char **argv, size_t cursor_arg, struct bu_cmd_validate_result *result)
{
    return bu_cmd_integer_pair_optional_validate(argc, argv, cursor_arg, result);
}
static int
rect_cdim_schema_validate(const struct bu_cmd_schema *schema, size_t argc,
	const char **argv, size_t cursor_arg, struct bu_cmd_validate_result *result)
{
    int pair[2] = {0, 0};
    int ret = rect_integer_pair_schema_validate(schema, argc, argv, cursor_arg, result);

    if (ret || !result || result->state != BU_CMD_VALIDATE_VALID || argc != 2)
	return ret;
    if (bu_cmd_integer_pair_from_argv(pair, argc, (const char * const *)argv) != 2 ||
	pair[X] <= 0 || pair[Y] <= 0) {
	result->state = BU_CMD_VALIDATE_INVALID;
	result->hint = "positive canvas width and height required";
    }
    return 0;
}
static int
rect_color_schema_validate(const struct bu_cmd_schema *UNUSED(schema), size_t argc,
	const char **argv, size_t cursor_arg, struct bu_cmd_validate_result *result)
{
    return bu_cmd_rgb_optional_validate(argc, argv, cursor_arg, result);
}
#define RECT_SCHEMA(_id, _name, _help, _operands, _validation) \
    static const struct bu_cmd_schema _id##_schema = { \
	_name, _help, NULL, _operands, BU_CMD_PARSE_STOP_AT_FIRST_OPERAND, _validation \
    }
RECT_SCHEMA(rect_root, "rect", "Query or configure the current view rectangle", NULL,
    BU_CMD_SCHEMA_CONSTRAINTS(NULL, NULL));
RECT_SCHEMA(rect_bg, "bg", "Query or set the background color", rect_color_operands,
    BU_CMD_SCHEMA_CONSTRAINTS(rect_color_schema_validate, NULL));
RECT_SCHEMA(rect_color, "color", "Query or set the border color", rect_color_operands,
    BU_CMD_SCHEMA_CONSTRAINTS(rect_color_schema_validate, NULL));
RECT_SCHEMA(rect_cdim, "cdim", "Query or set positive canvas dimensions", rect_integer_pair_operands,
    BU_CMD_SCHEMA_CONSTRAINTS(rect_cdim_schema_validate, NULL));
RECT_SCHEMA(rect_dim, "dim", "Query or set rectangle dimensions", rect_integer_pair_operands,
    BU_CMD_SCHEMA_CONSTRAINTS(rect_integer_pair_schema_validate, NULL));
RECT_SCHEMA(rect_draw, "draw", "Query or set rectangle display (0 or 1)", rect_bool_operands,
    BU_CMD_SCHEMA_CONSTRAINTS(NULL, NULL));
RECT_SCHEMA(rect_lstyle, "lstyle", "Query or set line style", rect_integer_operands,
    BU_CMD_SCHEMA_CONSTRAINTS(NULL, NULL));
RECT_SCHEMA(rect_lwidth, "lwidth", "Query or set line width", rect_integer_operands,
    BU_CMD_SCHEMA_CONSTRAINTS(NULL, NULL));
RECT_SCHEMA(rect_pos, "pos", "Query or set rectangle position", rect_integer_pair_operands,
    BU_CMD_SCHEMA_CONSTRAINTS(rect_integer_pair_schema_validate, NULL));
RECT_SCHEMA(rect_rt, "rt", "Raytrace the rectangle to a framebuffer port", rect_port_operands,
    BU_CMD_SCHEMA_CONSTRAINTS(NULL, NULL));
RECT_SCHEMA(rect_vars, "vars", "Print all rectangle values", NULL,
    BU_CMD_SCHEMA_CONSTRAINTS(NULL, NULL));
RECT_SCHEMA(rect_zoom, "zoom", "Zoom the view to the rectangle", NULL,
    BU_CMD_SCHEMA_CONSTRAINTS(NULL, NULL));
RECT_SCHEMA(rect_help, "help", "Show rectangle syntax", NULL,
    BU_CMD_SCHEMA_CONSTRAINTS(NULL, NULL));
#undef RECT_SCHEMA


static int
rect_tree_execute(void *data, int argc, const char *argv[])
{
    const char **full_argv = NULL;
    int ret = BRLCAD_ERROR;

    if (!data || argc < 1 || !argv)
	return BRLCAD_ERROR;
    full_argv = (const char **)bu_calloc((size_t)argc + 1, sizeof(*full_argv),
	"rect native tree argv");
    full_argv[0] = "rect";
    for (int i = 0; i < argc; i++)
	full_argv[i + 1] = argv[i];
    ret = rect_execute((struct ged *)data, argc + 1, full_argv);
    bu_free((void *)full_argv, "rect native tree argv");
    return ret;
}


static const struct bu_cmd_tree_node rect_subcommands[] = {
    BU_CMD_TREE_NODE(&rect_bg_schema, NULL, NULL, BU_CMD_TREE_CHILD_AFTER_OPTIONS, rect_tree_execute),
    BU_CMD_TREE_NODE(&rect_color_schema, NULL, NULL, BU_CMD_TREE_CHILD_AFTER_OPTIONS, rect_tree_execute),
    BU_CMD_TREE_NODE(&rect_cdim_schema, NULL, NULL, BU_CMD_TREE_CHILD_AFTER_OPTIONS, rect_tree_execute),
    BU_CMD_TREE_NODE(&rect_dim_schema, NULL, NULL, BU_CMD_TREE_CHILD_AFTER_OPTIONS, rect_tree_execute),
    BU_CMD_TREE_NODE(&rect_draw_schema, NULL, NULL, BU_CMD_TREE_CHILD_AFTER_OPTIONS, rect_tree_execute),
    BU_CMD_TREE_NODE(&rect_help_schema, NULL, NULL, BU_CMD_TREE_CHILD_AFTER_OPTIONS, rect_tree_execute),
    BU_CMD_TREE_NODE(&rect_lstyle_schema, NULL, NULL, BU_CMD_TREE_CHILD_AFTER_OPTIONS, rect_tree_execute),
    BU_CMD_TREE_NODE(&rect_lwidth_schema, NULL, NULL, BU_CMD_TREE_CHILD_AFTER_OPTIONS, rect_tree_execute),
    BU_CMD_TREE_NODE(&rect_pos_schema, NULL, NULL, BU_CMD_TREE_CHILD_AFTER_OPTIONS, rect_tree_execute),
    BU_CMD_TREE_NODE(&rect_rt_schema, NULL, NULL, BU_CMD_TREE_CHILD_AFTER_OPTIONS, rect_tree_execute),
    BU_CMD_TREE_NODE(&rect_vars_schema, NULL, NULL, BU_CMD_TREE_CHILD_AFTER_OPTIONS, rect_tree_execute),
    BU_CMD_TREE_NODE(&rect_zoom_schema, NULL, NULL, BU_CMD_TREE_CHILD_AFTER_OPTIONS, rect_tree_execute),
    BU_CMD_TREE_NODE_NULL
};
static const struct bu_cmd_tree ged_rect_tree = {
    &rect_root_schema, rect_subcommands, BU_CMD_TREE_CHILD_AFTER_OPTIONS
};


static char *
rect_native_help(void)
{
    return bu_cmd_tree_describe(&ged_rect_tree);
}


int
ged_rect_core(struct ged *gedp, int argc, const char *argv[])
{
    const struct bu_cmd_tree_node *node = NULL;
    struct bu_vls msg = BU_VLS_INIT_ZERO;
    int ret = BRLCAD_ERROR;

    GED_CHECK_VIEW(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);
    if (argc < 2)
	return rect_execute(gedp, argc, argv);
    node = bu_cmd_tree_find_subcommand(&ged_rect_tree, argv[1]);
    if (!node)
	return rect_execute(gedp, argc, argv);
    bu_vls_trunc(gedp->ged_result_str, 0);
    if (bu_cmd_schema_parse_complete(node->schema, NULL, &msg, argc - 2, argv + 2) < 0) {
	if (bu_vls_strlen(&msg))
	    bu_vls_vlscat(gedp->ged_result_str, &msg);
	rect_usage(gedp, argv[0]);
	bu_vls_free(&msg);
	return BRLCAD_ERROR;
    }
    bu_vls_free(&msg);
    if (bu_cmd_tree_dispatch(&ged_rect_tree, gedp, argc - 1, argv + 1, &ret) == 0)
	return ret;
    return BRLCAD_ERROR;
}


static int
ged_rect_grammar_validate(struct ged *gedp, const char *input, size_t cursor_pos,
	struct ged_cmd_validate_result *result)
{
    return ged_cmd_tree_validate(gedp, &ged_rect_tree, input, cursor_pos, result);
}


static int
ged_rect_grammar_analyze(struct ged *gedp, const char *input,
	struct ged_cmd_analysis *analysis)
{
    return ged_cmd_tree_analyze(gedp, &ged_rect_tree, input, analysis);
}


static char *
ged_rect_grammar_json(void)
{
    return bu_cmd_tree_describe_json(&ged_rect_tree);
}


static int
ged_rect_grammar_lint(struct bu_vls *msgs)
{
    return bu_cmd_tree_lint(&ged_rect_tree, msgs);
}


static const struct ged_cmd_grammar ged_rect_grammar = {
    "rect", "Query or configure the current view rectangle", ged_rect_grammar_validate,
    ged_rect_grammar_analyze, ged_rect_grammar_json, ged_rect_grammar_lint
};

#define GED_RECT_COMMANDS(X, XID) \
    X(rect, ged_rect_core, GED_CMD_DEFAULT, &ged_rect_grammar) \

GED_DECLARE_COMMAND_SET_WITH_GRAMMAR(GED_RECT_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST_WITH_GRAMMAR("libged_rect", 1, GED_RECT_COMMANDS)

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
