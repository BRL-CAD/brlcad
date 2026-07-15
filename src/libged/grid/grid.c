/*                          G R I D . C
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
/** @file libged/grid.c
 *
 * Routines that provide the basics for a snap to grid capability.
 *
 */

#include "common.h"

#include <math.h>
#include <string.h>


#include "vmath.h"
#include "bv.h"
#include "bv/snap.h"
#include "bu/cmdschema.h"
#include "bu/color.h"

#include "../ged_private.h"


static void
grid_vsnap(struct ged *gedp)
{
    point_t view_pt;
    point_t model_pt;

    MAT_DELTAS_GET_NEG(model_pt, gedp->ged_gvp->gv_center);
    MAT4X3PNT(view_pt, gedp->ged_gvp->gv_model2view, model_pt);
    bv_snap_grid_2d(gedp->ged_gvp, &view_pt[X], &view_pt[Y]);
    MAT4X3PNT(model_pt, gedp->ged_gvp->gv_view2model, view_pt);
    MAT_DELTAS_VEC_NEG(gedp->ged_gvp->gv_center, model_pt);
    bv_update(gedp->ged_gvp);
}


static void
grid_vls_print(struct ged *gedp)
{
    double blval = (gedp->dbip) ? gedp->dbip->dbi_base2local : 1.0;
    bu_vls_printf(gedp->ged_result_str, "anchor = %g %g %g\n",
		  gedp->ged_gvp->gv_s->gv_grid.anchor[0] * blval,
		  gedp->ged_gvp->gv_s->gv_grid.anchor[1] * blval,
		  gedp->ged_gvp->gv_s->gv_grid.anchor[2] * blval);
    bu_vls_printf(gedp->ged_result_str, "color = %d %d %d\n",
		  gedp->ged_gvp->gv_s->gv_grid.color[0],
		  gedp->ged_gvp->gv_s->gv_grid.color[1],
		  gedp->ged_gvp->gv_s->gv_grid.color[2]);
    bu_vls_printf(gedp->ged_result_str, "draw = %d\n", gedp->ged_gvp->gv_s->gv_grid.draw);
    bu_vls_printf(gedp->ged_result_str, "mrh = %d\n", gedp->ged_gvp->gv_s->gv_grid.res_major_h);
    bu_vls_printf(gedp->ged_result_str, "mrv = %d\n", gedp->ged_gvp->gv_s->gv_grid.res_major_v);
    bu_vls_printf(gedp->ged_result_str, "rh = %g\n", gedp->ged_gvp->gv_s->gv_grid.res_h * blval);
    bu_vls_printf(gedp->ged_result_str, "rv = %g\n", gedp->ged_gvp->gv_s->gv_grid.res_v * blval);
    bu_vls_printf(gedp->ged_result_str, "snap = %d\n", gedp->ged_gvp->gv_s->gv_grid.snap);
}


static char *grid_native_help(void);


static void
grid_usage(struct ged *gedp)
{
    char *help = grid_native_help();

    if (help) {
	bu_vls_strcat(gedp->ged_result_str, help);
	bu_free(help, "grid native tree help");
    }
}


/*
 * Note - this needs to be rewritten to accept keyword/value pairs so
 * that multiple attributes can be set with a single command call.
 */
static int
grid_execute(struct ged *gedp, int argc, const char *argv[])
{
    const char *parameter;
    const char **argp = argv;
    double blval = (gedp->dbip) ? gedp->dbip->dbi_base2local : 1.0;
    double lbval = (gedp->dbip) ? gedp->dbip->dbi_local2base : 1.0;

    GED_CHECK_VIEW(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    if (argc == 1) {
	grid_usage(gedp);
	return BRLCAD_OK;
    }

    if (argc < 2 || 5 < argc) {
	grid_usage(gedp);
	return BRLCAD_ERROR;
    }

    parameter = argv[1];
    argc -= 2;
    argp += 2;

    // TODO - need more sophisticated grid drawing - when zoomed out too far
    // grid disappears.  Need to simply draw a coarse grid that aligns with the
    // finer grid under it.
    //
    // TODO - when zoomed in so close the nearest grid points are not visible,
    // should probably disable grid snapping automatically and assume the goal
    // is free-form movement at that scale, since there are no visible snapping
    // points to target...
    if (BU_STR_EQUAL(parameter, "draw")) {
	if (argc == 0) {
	    bu_vls_printf(gedp->ged_result_str, "%d", gedp->ged_gvp->gv_s->gv_grid.draw);
	    return BRLCAD_OK;
	} else if (argc == 1) {
	    int enabled = 0;
	    if (!bu_cmd_integer_from_str(&enabled, argp[0]) || (enabled != 0 && enabled != 1))
		goto usage;
	    gedp->ged_gvp->gv_s->gv_grid.draw = enabled;

	    return BRLCAD_OK;
	}

	bu_vls_printf(gedp->ged_result_str, "The 'grid draw' command accepts 0 or 1 argument\n");
	return BRLCAD_ERROR;
    }

    if (BU_STR_EQUAL(parameter, "vsnap")) {
	if (argc == 0) {
	    grid_vsnap(gedp);
	    return BRLCAD_OK;
	}

	bu_vls_printf(gedp->ged_result_str, "The 'grid vsnap' command accepts no arguments\n");
	return BRLCAD_ERROR;
    }

    if (BU_STR_EQUAL(parameter, "snap")) {
	if (argc == 0) {
	    bu_vls_printf(gedp->ged_result_str, "%d", gedp->ged_gvp->gv_s->gv_grid.snap);
	    return BRLCAD_OK;
	} else if (argc == 1) {
	    int enabled = 0;
	    if (!bu_cmd_integer_from_str(&enabled, argp[0]) || (enabled != 0 && enabled != 1))
		goto usage;
	    gedp->ged_gvp->gv_s->gv_grid.snap = enabled;

	    return BRLCAD_OK;
	}

	bu_vls_printf(gedp->ged_result_str, "The 'grid snap' command accepts 0 or 1 argument\n");
	return BRLCAD_ERROR;
    }

    if (BU_STR_EQUAL(parameter, "rh")) {
	if (argc == 0) {
	    bu_vls_printf(gedp->ged_result_str, "%g",
			  gedp->ged_gvp->gv_s->gv_grid.res_h * blval);
	    return BRLCAD_OK;
	} else if (argc == 1) {
	    fastf_t value = 0.0;
	    if (!bu_cmd_number_from_str(&value, argp[0]))
		goto usage;
	    gedp->ged_gvp->gv_s->gv_grid.res_h = value * lbval;

	    return BRLCAD_OK;
	}

	bu_vls_printf(gedp->ged_result_str, "The 'grid rh' command accepts 0 or 1 argument\n");
	return BRLCAD_ERROR;
    }

    if (BU_STR_EQUAL(parameter, "rv")) {
	if (argc == 0) {
	    bu_vls_printf(gedp->ged_result_str, "%g",
			  gedp->ged_gvp->gv_s->gv_grid.res_v * blval);
	    return BRLCAD_OK;
	} else if (argc == 1) {
	    fastf_t value = 0.0;
	    if (!bu_cmd_number_from_str(&value, argp[0]))
		goto usage;
	    gedp->ged_gvp->gv_s->gv_grid.res_v = value * lbval;

	    return BRLCAD_OK;
	}

	bu_vls_printf(gedp->ged_result_str, "The 'grid rv' command accepts 0 or 1 argument\n");
	return BRLCAD_ERROR;
    }

    if (BU_STR_EQUAL(parameter, "mrh")) {
	if (argc == 0) {
	    bu_vls_printf(gedp->ged_result_str, "%d", gedp->ged_gvp->gv_s->gv_grid.res_major_h);
	    return BRLCAD_OK;
	} else if (argc == 1) {
	    int value = 0;
	    if (!bu_cmd_integer_from_str(&value, argp[0]))
		goto usage;
	    gedp->ged_gvp->gv_s->gv_grid.res_major_h = value;

	    return BRLCAD_OK;
	}

	bu_vls_printf(gedp->ged_result_str, "The 'grid mrh' command accepts 0 or 1 argument\n");
	return BRLCAD_ERROR;
    }

    if (BU_STR_EQUAL(parameter, "mrv")) {
	if (argc == 0) {
	    bu_vls_printf(gedp->ged_result_str, "%d", gedp->ged_gvp->gv_s->gv_grid.res_major_v);
	    return BRLCAD_OK;
	} else if (argc == 1) {
	    int value = 0;
	    if (!bu_cmd_integer_from_str(&value, argp[0]))
		goto usage;
	    gedp->ged_gvp->gv_s->gv_grid.res_major_v = value;

	    return BRLCAD_OK;
	}

	bu_vls_printf(gedp->ged_result_str, "The 'grid mrv' command accepts 0 or 1 argument\n");
	return BRLCAD_ERROR;
    }

    if (BU_STR_EQUAL(parameter, "anchor")) {
	if (argc == 0) {
	    bu_vls_printf(gedp->ged_result_str, "%g %g %g",
			  gedp->ged_gvp->gv_s->gv_grid.anchor[X] * blval,
			  gedp->ged_gvp->gv_s->gv_grid.anchor[Y] * blval,
			  gedp->ged_gvp->gv_s->gv_grid.anchor[Z] * blval);
	    return BRLCAD_OK;
	} else if (argc == 3) {
	    fastf_t value[3] = {0.0, 0.0, 0.0};
	    if (bu_cmd_vector3_from_argv(value, 3, (const char * const *)argp) != 3)
		goto usage;
	    gedp->ged_gvp->gv_s->gv_grid.anchor[0] = value[X] * lbval;
	    gedp->ged_gvp->gv_s->gv_grid.anchor[1] = value[Y] * lbval;
	    gedp->ged_gvp->gv_s->gv_grid.anchor[2] = value[Z] * lbval;

	    return BRLCAD_OK;
	}

	bu_vls_printf(gedp->ged_result_str, "The 'grid anchor' command requires 0 or 3 arguments\n");
	return BRLCAD_ERROR;
    }

    if (BU_STR_EQUAL(parameter, "color")) {
	if (argc == 0) {
	    bu_vls_printf(gedp->ged_result_str, "%d %d %d",
			  gedp->ged_gvp->gv_s->gv_grid.color[X],
			  gedp->ged_gvp->gv_s->gv_grid.color[Y],
			  gedp->ged_gvp->gv_s->gv_grid.color[Z]);
	    return BRLCAD_OK;
	} else if (argc == 1 || argc == 3) {
	    unsigned char rgb[3] = {0, 0, 0};
	    if (bu_rgb_from_argv(rgb, (size_t)argc, (const char * const *)argp) != argc)
		goto usage;
	    gedp->ged_gvp->gv_s->gv_grid.color[0] = rgb[RED];
	    gedp->ged_gvp->gv_s->gv_grid.color[1] = rgb[GRN];
	    gedp->ged_gvp->gv_s->gv_grid.color[2] = rgb[BLU];

	    return BRLCAD_OK;
	}

	bu_vls_printf(gedp->ged_result_str, "The 'grid color' command requires zero, one packed, or three RGB arguments\n");
	return BRLCAD_ERROR;
    }

    if (BU_STR_EQUAL(parameter, "vars")) {
	if (argc)
	    goto usage;
	grid_vls_print(gedp);
	return BRLCAD_OK;
    }

    if (BU_STR_EQUAL(parameter, "help")) {
	if (argc)
	    goto usage;
	grid_usage(gedp);
	return GED_HELP;
    }

    bu_vls_printf(gedp->ged_result_str, "grid: unrecognized parameter '%s'\n", parameter);

usage:
    grid_usage(gedp);

    return BRLCAD_ERROR;
}

#include "../include/plugin.h"

static const char * const grid_bool_keywords[] = {"0", "1", NULL};
static const struct bu_cmd_operand grid_bool_operands[] = {
    BU_CMD_OPERAND_KEYWORDS("enabled", BU_CMD_VALUE_KEYWORD, 0, 1,
	"Zero to disable or one to enable", NULL, grid_bool_keywords),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_operand grid_number_operands[] = {
    BU_CMD_OPERAND("value", BU_CMD_VALUE_NUMBER, 0, 1,
	"Optional finite number", NULL),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_operand grid_integer_operands[] = {
    BU_CMD_OPERAND("value", BU_CMD_VALUE_INTEGER, 0, 1,
	"Optional base-zero integer", NULL),
    BU_CMD_OPERAND_NULL
};
static int
grid_anchor_schema_validate(const struct bu_cmd_schema *UNUSED(schema), size_t argc,
	const char **argv, size_t cursor_arg, struct bu_cmd_validate_result *result)
{
    return bu_cmd_vector3_optional_validate(argc, argv, cursor_arg, result);
}
static int
grid_color_schema_validate(const struct bu_cmd_schema *UNUSED(schema), size_t argc,
	const char **argv, size_t cursor_arg, struct bu_cmd_validate_result *result)
{
    return bu_cmd_rgb_optional_validate(argc, argv, cursor_arg, result);
}
static const struct bu_cmd_operand grid_anchor_operands[] = {
    BU_CMD_OPERAND_SHAPED("anchor", BU_CMD_VALUE_VECTOR, 0, 3, NULL,
	"Optional packed x/y/z or three finite coordinates", NULL, &bu_cmd_vector3_arg_shape),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_operand grid_color_operands[] = {
    BU_CMD_OPERAND_SHAPED("color", BU_CMD_VALUE_COLOR, 0, 3, NULL,
	"Optional packed r/g/b or three RGB channels", NULL, &bu_cmd_rgb_arg_shape),
    BU_CMD_OPERAND_NULL
};
#define GRID_SCHEMA(_id, _name, _help, _operands, _validation) \
    static const struct bu_cmd_schema _id##_schema = { \
	_name, _help, NULL, _operands, BU_CMD_PARSE_STOP_AT_FIRST_OPERAND, _validation \
    }
GRID_SCHEMA(grid_root, "grid", "Query or configure the current view grid", NULL,
    BU_CMD_SCHEMA_CONSTRAINTS(NULL, NULL));
GRID_SCHEMA(grid_anchor, "anchor", "Query or set the grid anchor", grid_anchor_operands,
    BU_CMD_SCHEMA_CONSTRAINTS(grid_anchor_schema_validate, NULL));
GRID_SCHEMA(grid_color, "color", "Query or set the grid color", grid_color_operands,
    BU_CMD_SCHEMA_CONSTRAINTS(grid_color_schema_validate, NULL));
GRID_SCHEMA(grid_draw, "draw", "Query or set grid display (0 or 1)", grid_bool_operands,
    BU_CMD_SCHEMA_CONSTRAINTS(NULL, NULL));
GRID_SCHEMA(grid_snap, "snap", "Query or set grid snapping (0 or 1)", grid_bool_operands,
    BU_CMD_SCHEMA_CONSTRAINTS(NULL, NULL));
GRID_SCHEMA(grid_mrh, "mrh", "Query or set horizontal major interval", grid_integer_operands,
    BU_CMD_SCHEMA_CONSTRAINTS(NULL, NULL));
GRID_SCHEMA(grid_mrv, "mrv", "Query or set vertical major interval", grid_integer_operands,
    BU_CMD_SCHEMA_CONSTRAINTS(NULL, NULL));
GRID_SCHEMA(grid_rh, "rh", "Query or set horizontal resolution", grid_number_operands,
    BU_CMD_SCHEMA_CONSTRAINTS(NULL, NULL));
GRID_SCHEMA(grid_rv, "rv", "Query or set vertical resolution", grid_number_operands,
    BU_CMD_SCHEMA_CONSTRAINTS(NULL, NULL));
GRID_SCHEMA(grid_vars, "vars", "Print all grid values", NULL,
    BU_CMD_SCHEMA_CONSTRAINTS(NULL, NULL));
GRID_SCHEMA(grid_vsnap, "vsnap", "Snap the view center to the grid", NULL,
    BU_CMD_SCHEMA_CONSTRAINTS(NULL, NULL));
GRID_SCHEMA(grid_help, "help", "Show grid syntax", NULL,
    BU_CMD_SCHEMA_CONSTRAINTS(NULL, NULL));
#undef GRID_SCHEMA


static int
grid_tree_execute(void *data, int argc, const char *argv[])
{
    const char **full_argv = NULL;
    int ret = BRLCAD_ERROR;

    if (!data || argc < 1 || !argv)
	return BRLCAD_ERROR;
    full_argv = (const char **)bu_calloc((size_t)argc + 1, sizeof(*full_argv),
	"grid native tree argv");
    full_argv[0] = "grid";
    for (int i = 0; i < argc; i++)
	full_argv[i + 1] = argv[i];
    ret = grid_execute((struct ged *)data, argc + 1, full_argv);
    bu_free((void *)full_argv, "grid native tree argv");
    return ret;
}


static const struct bu_cmd_tree_node grid_subcommands[] = {
    BU_CMD_TREE_NODE(&grid_anchor_schema, NULL, NULL, BU_CMD_TREE_CHILD_AFTER_OPTIONS, grid_tree_execute),
    BU_CMD_TREE_NODE(&grid_color_schema, NULL, NULL, BU_CMD_TREE_CHILD_AFTER_OPTIONS, grid_tree_execute),
    BU_CMD_TREE_NODE(&grid_draw_schema, NULL, NULL, BU_CMD_TREE_CHILD_AFTER_OPTIONS, grid_tree_execute),
    BU_CMD_TREE_NODE(&grid_help_schema, NULL, NULL, BU_CMD_TREE_CHILD_AFTER_OPTIONS, grid_tree_execute),
    BU_CMD_TREE_NODE(&grid_mrh_schema, NULL, NULL, BU_CMD_TREE_CHILD_AFTER_OPTIONS, grid_tree_execute),
    BU_CMD_TREE_NODE(&grid_mrv_schema, NULL, NULL, BU_CMD_TREE_CHILD_AFTER_OPTIONS, grid_tree_execute),
    BU_CMD_TREE_NODE(&grid_rh_schema, NULL, NULL, BU_CMD_TREE_CHILD_AFTER_OPTIONS, grid_tree_execute),
    BU_CMD_TREE_NODE(&grid_rv_schema, NULL, NULL, BU_CMD_TREE_CHILD_AFTER_OPTIONS, grid_tree_execute),
    BU_CMD_TREE_NODE(&grid_snap_schema, NULL, NULL, BU_CMD_TREE_CHILD_AFTER_OPTIONS, grid_tree_execute),
    BU_CMD_TREE_NODE(&grid_vars_schema, NULL, NULL, BU_CMD_TREE_CHILD_AFTER_OPTIONS, grid_tree_execute),
    BU_CMD_TREE_NODE(&grid_vsnap_schema, NULL, NULL, BU_CMD_TREE_CHILD_AFTER_OPTIONS, grid_tree_execute),
    BU_CMD_TREE_NODE_NULL
};
static const struct bu_cmd_tree ged_grid_tree = {
    &grid_root_schema, grid_subcommands, BU_CMD_TREE_CHILD_AFTER_OPTIONS
};


static char *
grid_native_help(void)
{
    return bu_cmd_tree_describe(&ged_grid_tree);
}


int
ged_grid_core(struct ged *gedp, int argc, const char *argv[])
{
    const struct bu_cmd_tree_node *node = NULL;
    struct bu_vls msg = BU_VLS_INIT_ZERO;
    int ret = BRLCAD_ERROR;

    GED_CHECK_VIEW(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);
    bu_vls_trunc(gedp->ged_result_str, 0);
    if (argc == 1) {
	grid_usage(gedp);
	return BRLCAD_OK;
    }
    node = bu_cmd_tree_find_subcommand(&ged_grid_tree, argv[1]);
    if (!node)
	return grid_execute(gedp, argc, argv);
    if (bu_cmd_schema_parse_complete(node->schema, NULL, &msg, argc - 2, argv + 2) < 0) {
	if (bu_vls_strlen(&msg))
	    bu_vls_vlscat(gedp->ged_result_str, &msg);
	grid_usage(gedp);
	bu_vls_free(&msg);
	return BRLCAD_ERROR;
    }
    bu_vls_free(&msg);
    if (bu_cmd_tree_dispatch(&ged_grid_tree, gedp, argc - 1, argv + 1, &ret) == 0)
	return ret;
    return BRLCAD_ERROR;
}


static int
ged_grid_grammar_validate(struct ged *gedp, const char *input, size_t cursor_pos,
	struct ged_cmd_validate_result *result)
{
    return ged_cmd_tree_validate(gedp, &ged_grid_tree, input, cursor_pos, result);
}


static int
ged_grid_grammar_analyze(struct ged *gedp, const char *input,
	struct ged_cmd_analysis *analysis)
{
    return ged_cmd_tree_analyze(gedp, &ged_grid_tree, input, analysis);
}


static char *
ged_grid_grammar_json(void)
{
    return bu_cmd_tree_describe_json(&ged_grid_tree);
}


static int
ged_grid_grammar_lint(struct bu_vls *msgs)
{
    return bu_cmd_tree_lint(&ged_grid_tree, msgs);
}


static const struct ged_cmd_grammar ged_grid_grammar = {
    "grid", "Query or configure the current view grid", ged_grid_grammar_validate,
    ged_grid_grammar_analyze, ged_grid_grammar_json, ged_grid_grammar_lint
};

#define GED_GRID_COMMANDS(X, XID) \
    X(grid, ged_grid_core, GED_CMD_DEFAULT, &ged_grid_grammar) \

GED_DECLARE_COMMAND_SET_WITH_GRAMMAR(GED_GRID_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST_WITH_GRAMMAR("libged_grid", 1, GED_GRID_COMMANDS)

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
