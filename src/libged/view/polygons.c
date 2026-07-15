/*                      P O L Y G O N S . C
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
/** @file libged/view/polygons.c
 *
 * Commands for view polygons.
 *
 */

#include "common.h"

#include "bu/color.h"
#include "bu/cmdschema.h"
#include "bu/vls.h"
#include "bv.h"
#include "bg/polygon.h"
#include "rt/geom.h"
#include "rt/primitives/sketch.h"

#include "../ged_private.h"
#include "./ged_view.h"

struct ged_view_polygon_args {
    int print_help;
    int s_version;
};

int _poly_cmd_append(void *bs, int argc, const char **argv);
int _poly_cmd_area(void *bs, int argc, const char **argv);
int _poly_cmd_clear(void *bs, int argc, const char **argv);
int _poly_cmd_close(void *bs, int argc, const char **argv);
int _poly_cmd_create(void *bs, int argc, const char **argv);
int _poly_cmd_csg(void *bs, int argc, const char **argv);
int _poly_cmd_export(void *bs, int argc, const char **argv);
int _poly_cmd_fill(void *bs, int argc, const char **argv);
int _poly_cmd_fill_color(void *bs, int argc, const char **argv);
int _poly_cmd_import(void *bs, int argc, const char **argv);
int _poly_cmd_move(void *bs, int argc, const char **argv);
int _poly_cmd_open(void *bs, int argc, const char **argv);
int _poly_cmd_overlap(void *bs, int argc, const char **argv);
int _poly_cmd_select(void *bs, int argc, const char **argv);

static void
polygon_set_validation(struct bu_cmd_validate_result *result,
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
polygon_nonnegative_pair_validate(const struct bu_cmd_schema *UNUSED(schema),
    size_t argc, const char **argv, size_t cursor_arg,
    struct bu_cmd_validate_result *result)
{
    bu_cmd_validate_state_t state = BU_CMD_VALIDATE_VALID;
    const char *hint = "two nonnegative screen coordinates required";

    if (!result || cursor_arg > argc)
	return -1;
    if (argc < 2) {
	state = BU_CMD_VALIDATE_INCOMPLETE;
    } else if (argc > 2) {
	state = BU_CMD_VALIDATE_INVALID;
	hint = "exactly two screen coordinates required";
    } else {
	for (size_t i = 0; i < argc; i++) {
	    if (bu_cmd_nonnegative_integer_validate(NULL, argv[i]) != 0) {
		state = BU_CMD_VALIDATE_INVALID;
		hint = "screen coordinates must be nonnegative integers";
		break;
	    }
	}
    }
    polygon_set_validation(result, state, cursor_arg, BU_CMD_VALUE_INTEGER, hint);
    return 0;
}

static int
polygon_contour_pair_validate(const struct bu_cmd_schema *UNUSED(schema),
    size_t argc, const char **argv, size_t cursor_arg,
    struct bu_cmd_validate_result *result)
{
    bu_cmd_validate_state_t state = BU_CMD_VALIDATE_VALID;
    const char *hint = "screen coordinates, optionally preceded by a contour index";

    if (!result || cursor_arg > argc)
	return -1;
    if (argc < 2) {
	state = BU_CMD_VALIDATE_INCOMPLETE;
    } else if (argc > 3) {
	state = BU_CMD_VALIDATE_INVALID;
	hint = "accepts X Y or contour X Y";
    } else {
	for (size_t i = 0; i < argc; i++) {
	    if (bu_cmd_nonnegative_integer_validate(NULL, argv[i]) != 0) {
		state = BU_CMD_VALIDATE_INVALID;
		hint = "contour and screen coordinates must be nonnegative integers";
		break;
	    }
	}
    }
    polygon_set_validation(result, state, cursor_arg, BU_CMD_VALUE_INTEGER, hint);
    return 0;
}

static int
polygon_open_close_validate(const struct bu_cmd_schema *UNUSED(schema), size_t argc,
    const char **argv, size_t cursor_arg, struct bu_cmd_validate_result *result)
{
    int index = 0;

    if (!result || cursor_arg > argc)
	return -1;
    if (argc > 1) {
	polygon_set_validation(result, BU_CMD_VALIDATE_INVALID, 1, BU_CMD_VALUE_INTEGER,
	    "at most one contour index is accepted");
	return 0;
    }
	if (argc && (!bu_cmd_integer_from_str(&index, argv[0]) || index < 0)) {
	polygon_set_validation(result, BU_CMD_VALIDATE_INVALID, 0, BU_CMD_VALUE_INTEGER,
	    "contour index must be a nonnegative integer");
	return 0;
    }
    polygon_set_validation(result, BU_CMD_VALIDATE_VALID, cursor_arg,
	BU_CMD_VALUE_INTEGER, "optional contour index");
    return 0;
}

static const char * const polygon_fill_disable[] = {"0", NULL};

static int
polygon_fill_validate(const struct bu_cmd_schema *UNUSED(schema), size_t argc,
    const char **argv, size_t cursor_arg, struct bu_cmd_validate_result *result)
{
    bu_cmd_validate_state_t state = BU_CMD_VALIDATE_VALID;
    const char *hint = "0 or three finite fill parameters required";
    bu_cmd_value_t type = BU_CMD_VALUE_NUMBER;

    if (!result || cursor_arg > argc)
	return -1;
    if (argc == 1 && BU_STR_EQUAL(argv[0], "0")) {
	type = BU_CMD_VALUE_KEYWORD;
    } else if (argc > 3) {
	state = BU_CMD_VALIDATE_INVALID;
    } else {
	for (size_t i = 0; i < argc; i++) {
	    fastf_t value = 0.0;
	    if (!bu_cmd_number_from_str(&value, argv[i])) {
		state = BU_CMD_VALIDATE_INVALID;
		hint = "invalid fill parameter";
		break;
	    }
	}
	if (state == BU_CMD_VALIDATE_VALID && argc < 3) {
	    state = BU_CMD_VALIDATE_INCOMPLETE;
	    hint = "three fill parameters required";
	}
    }
    polygon_set_validation(result, state, cursor_arg, type, hint);
    if (cursor_arg == 0 && (argc == 0 || (argc && !BU_STR_EQUAL(argv[0], "0"))))
	bu_cmd_keyword_candidates(result, polygon_fill_disable, argc ? argv[0] : "");
    return 0;
}

static int
polygon_fill_color_validate(const struct bu_cmd_schema *UNUSED(schema), size_t argc,
    const char **argv, size_t cursor_arg, struct bu_cmd_validate_result *result)
{
    int ret = bu_cmd_color_optional_validate(argc, argv, cursor_arg, result);
    if (!ret && result)
	result->semantic_provider = "ged.color";
    return ret;
}

static const char * const polygon_circle_aliases[] = {"circle", NULL};
static const char * const polygon_ellipse_aliases[] = {"ellipse", NULL};
static const char * const polygon_rectangle_aliases[] = {"rectangle", NULL};
static const char * const polygon_square_aliases[] = {"square", NULL};
static const struct bu_cmd_value_keyword polygon_shape_values[] = {
    {"circ", polygon_circle_aliases, "Circle"},
    {"ell", polygon_ellipse_aliases, "Ellipse"},
    {"rect", polygon_rectangle_aliases, "Rectangle"},
    {"sq", polygon_square_aliases, "Square"},
    {NULL, NULL, NULL}
};
static const char * const polygon_csg_values[] = {"u", "-", "+", NULL};

static const struct bu_cmd_option polygon_root_options[] = {
    BU_CMD_FLAG("h", "help", struct ged_view_polygon_args, print_help,
	"Print help and exit"),
    BU_CMD_ALIAS_SHORT("?", "help", 1),
    BU_CMD_FLAG("s", NULL, struct ged_view_polygon_args, s_version,
	"Work with the S version of polygon data"),
    BU_CMD_OPTION_NULL
};
static const struct bu_cmd_operand polygon_create_operands[] = {
    BU_CMD_OPERAND_VALIDATE("x", BU_CMD_VALUE_INTEGER, 1, 1,
	bu_cmd_nonnegative_integer_validate, "Nonnegative screen X coordinate", NULL),
    BU_CMD_OPERAND_VALIDATE("y", BU_CMD_VALUE_INTEGER, 1, 1,
	bu_cmd_nonnegative_integer_validate, "Nonnegative screen Y coordinate", NULL),
    BU_CMD_OPERAND_KEYWORD_VALUES("shape", BU_CMD_VALUE_KEYWORD, 0, 1,
	"Optional constrained polygon shape", NULL, polygon_shape_values),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_operand polygon_raw_three_operands[] = {
    BU_CMD_OPERAND("arguments", BU_CMD_VALUE_RAW, 0, 3, "Polygon arguments", NULL),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_operand polygon_optional_index_operands[] = {
    BU_CMD_OPERAND("index", BU_CMD_VALUE_RAW, 0, 1, "Optional contour index", NULL),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_operand polygon_one_string_operands[] = {
    BU_CMD_OPERAND("name", BU_CMD_VALUE_STRING, 1, 1, "Name", NULL),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_operand polygon_csg_operands[] = {
    BU_CMD_OPERAND_KEYWORDS("operation", BU_CMD_VALUE_KEYWORD, 1, 1,
	"u, -, or +", NULL, polygon_csg_values),
    BU_CMD_OPERAND("object", BU_CMD_VALUE_STRING, 1, 1, "Other polygon name", NULL),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_schema polygon_create_schema = {
    "create", "Create a general or constrained polygon", NULL, polygon_create_operands,
    BU_CMD_PARSE_STOP_AT_FIRST_OPERAND, BU_CMD_SCHEMA_CONSTRAINTS(NULL, NULL)
};
static const struct bu_cmd_schema polygon_select_schema = {
    "select", "Select the nearest general-polygon point", NULL, polygon_raw_three_operands,
    BU_CMD_PARSE_STOP_AT_FIRST_OPERAND, BU_CMD_SCHEMA_CONSTRAINTS(polygon_contour_pair_validate, NULL)
};
static const struct bu_cmd_schema polygon_append_schema = {
    "append", "Append a point to a general polygon", NULL, polygon_raw_three_operands,
    BU_CMD_PARSE_STOP_AT_FIRST_OPERAND, BU_CMD_SCHEMA_CONSTRAINTS(polygon_contour_pair_validate, NULL)
};
static const struct bu_cmd_schema polygon_move_schema = {
    "move", "Move the selected general-polygon point", NULL, polygon_raw_three_operands,
    BU_CMD_PARSE_STOP_AT_FIRST_OPERAND, BU_CMD_SCHEMA_CONSTRAINTS(polygon_nonnegative_pair_validate, NULL)
};
static const struct bu_cmd_schema polygon_clear_schema = {
    "clear", "Clear polygon selection state", NULL, NULL,
    BU_CMD_PARSE_STOP_AT_FIRST_OPERAND, BU_CMD_SCHEMA_CONSTRAINTS(NULL, NULL)
};
static const struct bu_cmd_schema polygon_close_schema = {
    "close", "Close all or one contour", NULL, polygon_optional_index_operands,
    BU_CMD_PARSE_STOP_AT_FIRST_OPERAND, BU_CMD_SCHEMA_CONSTRAINTS(polygon_open_close_validate, NULL)
};
static const struct bu_cmd_schema polygon_open_schema = {
    "open", "Open all or one contour", NULL, polygon_optional_index_operands,
    BU_CMD_PARSE_STOP_AT_FIRST_OPERAND, BU_CMD_SCHEMA_CONSTRAINTS(polygon_open_close_validate, NULL)
};
static const struct bu_cmd_schema polygon_area_schema = {
    "area", "Report polygon area", NULL, NULL,
    BU_CMD_PARSE_STOP_AT_FIRST_OPERAND, BU_CMD_SCHEMA_CONSTRAINTS(NULL, NULL)
};
static const struct bu_cmd_schema polygon_overlap_schema = {
    "overlap", "Report overlap with another polygon", NULL, polygon_one_string_operands,
    BU_CMD_PARSE_STOP_AT_FIRST_OPERAND, BU_CMD_SCHEMA_CONSTRAINTS(NULL, NULL)
};
static const struct bu_cmd_schema polygon_import_schema = {
    "import", "Import a polygon from a sketch", NULL, polygon_one_string_operands,
    BU_CMD_PARSE_STOP_AT_FIRST_OPERAND, BU_CMD_SCHEMA_CONSTRAINTS(NULL, NULL)
};
static const struct bu_cmd_schema polygon_export_schema = {
    "export", "Export a polygon to a sketch", NULL, polygon_one_string_operands,
    BU_CMD_PARSE_STOP_AT_FIRST_OPERAND, BU_CMD_SCHEMA_CONSTRAINTS(NULL, NULL)
};
static const struct bu_cmd_schema polygon_fill_schema = {
    "fill", "Disable fill or set fill direction and spacing", NULL, polygon_raw_three_operands,
    BU_CMD_PARSE_STOP_AT_FIRST_OPERAND, BU_CMD_SCHEMA_CONSTRAINTS(polygon_fill_validate, NULL)
};
static const struct bu_cmd_schema polygon_fill_color_schema = {
    "fill_color", "Query or set fill-line color", NULL, polygon_raw_three_operands,
    BU_CMD_PARSE_STOP_AT_FIRST_OPERAND, BU_CMD_SCHEMA_CONSTRAINTS(polygon_fill_color_validate, NULL)
};
static const struct bu_cmd_schema polygon_csg_schema = {
    "csg", "Replace this polygon with a boolean result", NULL, polygon_csg_operands,
    BU_CMD_PARSE_STOP_AT_FIRST_OPERAND, BU_CMD_SCHEMA_CONSTRAINTS(NULL, NULL)
};
GED_EXPORT const struct bu_cmd_schema ged_view_polygon_schema = {
    "polygon", "Manage object polygons", polygon_root_options, NULL,
    BU_CMD_PARSE_OPTIONS_FIRST, BU_CMD_SCHEMA_CONSTRAINTS(NULL, NULL)
};
GED_EXPORT const struct bu_cmd_tree_node ged_view_polygon_subcommands[] = {
    BU_CMD_TREE_NODE(&polygon_append_schema, NULL, NULL, BU_CMD_TREE_CHILD_AFTER_OPTIONS, _poly_cmd_append),
    BU_CMD_TREE_NODE(&polygon_area_schema, NULL, NULL, BU_CMD_TREE_CHILD_AFTER_OPTIONS, _poly_cmd_area),
    BU_CMD_TREE_NODE(&polygon_clear_schema, NULL, NULL, BU_CMD_TREE_CHILD_AFTER_OPTIONS, _poly_cmd_clear),
    BU_CMD_TREE_NODE(&polygon_close_schema, NULL, NULL, BU_CMD_TREE_CHILD_AFTER_OPTIONS, _poly_cmd_close),
    BU_CMD_TREE_NODE(&polygon_create_schema, NULL, NULL, BU_CMD_TREE_CHILD_AFTER_OPTIONS, _poly_cmd_create),
    BU_CMD_TREE_NODE(&polygon_csg_schema, NULL, NULL, BU_CMD_TREE_CHILD_AFTER_OPTIONS, _poly_cmd_csg),
    BU_CMD_TREE_NODE(&polygon_export_schema, NULL, NULL, BU_CMD_TREE_CHILD_AFTER_OPTIONS, _poly_cmd_export),
    BU_CMD_TREE_NODE(&polygon_fill_schema, NULL, NULL, BU_CMD_TREE_CHILD_AFTER_OPTIONS, _poly_cmd_fill),
    BU_CMD_TREE_NODE(&polygon_fill_color_schema, NULL, NULL, BU_CMD_TREE_CHILD_AFTER_OPTIONS, _poly_cmd_fill_color),
    BU_CMD_TREE_NODE(&polygon_import_schema, NULL, NULL, BU_CMD_TREE_CHILD_AFTER_OPTIONS, _poly_cmd_import),
    BU_CMD_TREE_NODE(&polygon_move_schema, NULL, NULL, BU_CMD_TREE_CHILD_AFTER_OPTIONS, _poly_cmd_move),
    BU_CMD_TREE_NODE(&polygon_open_schema, NULL, NULL, BU_CMD_TREE_CHILD_AFTER_OPTIONS, _poly_cmd_open),
    BU_CMD_TREE_NODE(&polygon_overlap_schema, NULL, NULL, BU_CMD_TREE_CHILD_AFTER_OPTIONS, _poly_cmd_overlap),
    BU_CMD_TREE_NODE(&polygon_select_schema, NULL, NULL, BU_CMD_TREE_CHILD_AFTER_OPTIONS, _poly_cmd_select),
    BU_CMD_TREE_NODE_NULL
};
GED_EXPORT const struct bu_cmd_tree ged_view_polygon_tree = {
    &ged_view_polygon_schema, ged_view_polygon_subcommands, BU_CMD_TREE_CHILD_AFTER_OPTIONS
};

static int
polygon_preflight(struct ged *gedp, const struct bu_cmd_schema *schema,
    int argc, const char **argv)
{
    return bu_cmd_schema_parse_complete(schema, NULL, gedp->ged_result_str,
	argc - 1, argv + 1) < 0 ? BRLCAD_ERROR : BRLCAD_OK;
}

int
_poly_cmd_create(void *bs, int argc, const char **argv)
{
    struct _ged_view_info *gd = (struct _ged_view_info *)bs;
    struct ged *gedp = gd->gedp;
    const char *usage_string = "view obj <objname> polygon create x y [circ|ell|rect|sq]";
    const char *purpose_string = "create polygon";
    if (_view_cmd_msgs(bs, argc, argv, usage_string, purpose_string))
	return BRLCAD_OK;

    if (polygon_preflight(gedp, &polygon_create_schema, argc, argv) != BRLCAD_OK)
	return BRLCAD_ERROR;

    argc--; argv++;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    struct bv_scene_obj *s = gd->s;
    if (s) {
	bu_vls_printf(gedp->ged_result_str, "View object named %s already exists\n", gd->vobj);
	return BRLCAD_ERROR;
    }

    int x,y;
	if (!bu_cmd_integer_from_str(&x, argv[0]) || x < 0) {
	bu_vls_printf(gedp->ged_result_str, "Invalid argument %s\n", argv[0]);
	return BRLCAD_ERROR;
    }
	if (!bu_cmd_integer_from_str(&y, argv[1]) || y < 0) {
	bu_vls_printf(gedp->ged_result_str, "Invalid argument %s\n", argv[1]);
	return BRLCAD_ERROR;
    }

    point_t sp;
    bv_screen_pt(&sp, (fastf_t)x, (fastf_t)y, gedp->ged_gvp);

    int type = BV_POLYGON_GENERAL;
    if (argc == 3) {
	if (BU_STR_EQUAL(argv[2], "circ") || BU_STR_EQUAL(argv[2], "circle"))
	    type = BV_POLYGON_CIRCLE;
	if (BU_STR_EQUAL(argv[2], "ell") || BU_STR_EQUAL(argv[2], "ellipse"))
	    type = BV_POLYGON_ELLIPSE;
	if (BU_STR_EQUAL(argv[2], "rect") || BU_STR_EQUAL(argv[2], "rectangle"))
	    type = BV_POLYGON_RECTANGLE;
	if (BU_STR_EQUAL(argv[2], "sq") || BU_STR_EQUAL(argv[2], "square"))
	    type = BV_POLYGON_SQUARE;
	if (type == BV_POLYGON_GENERAL) {
	    bu_vls_printf(gedp->ged_result_str, "Unknown polygon type %s\n", argv[2]);
	    return BRLCAD_ERROR;
	}
    }

    int flags = BV_VIEW_OBJS;
    if (gd->local_obj)
	flags |= BV_LOCAL_OBJS;
    s = bv_create_polygon(gd->cv, flags, type, &sp);
    if (!s) {
	bu_vls_printf(gedp->ged_result_str, "Failed to create %s\n", gd->vobj);
	return BRLCAD_ERROR;
    }
    bu_vls_init(&s->s_name);
    bu_vls_printf(&s->s_name, "%s", gd->vobj);

    return BRLCAD_OK;
}

int
_poly_cmd_select(void *bs, int argc, const char **argv)
{
    struct _ged_view_info *gd = (struct _ged_view_info *)bs;
    struct ged *gedp = gd->gedp;
    const char *usage_string = "view obj <objname> polygon select [contour] x y";
    const char *purpose_string = "select polygon point closest to point x,y";
    if (_view_cmd_msgs(bs, argc, argv, usage_string, purpose_string))
	return BRLCAD_OK;

    if (polygon_preflight(gedp, &polygon_select_schema, argc, argv) != BRLCAD_OK)
	return BRLCAD_ERROR;

    argc--; argv++;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    struct bv_scene_obj *s = gd->s;
    if (!s) {
	bu_vls_printf(gedp->ged_result_str, "View object %s does not exist\n", gd->vobj);
	return BRLCAD_ERROR;
    }
    if (!(s->s_type_flags & BV_VIEWONLY) || !(s->s_type_flags & BV_POLYGONS)) {
	bu_vls_printf(gedp->ged_result_str, "Specified object is not a view polygon.\n");
	return BRLCAD_ERROR;
    }

    struct bv_polygon *p = (struct bv_polygon *)s->s_i_data;

    if (p->type != BV_POLYGON_GENERAL) {
	bu_vls_printf(gedp->ged_result_str, "Point selection is only supported for general polygons - specified object defines a constrained shape\n");
	return BRLCAD_ERROR;
    }

    int ioffset = 0;
    int contour_ind = -1;
    if (argc == 3) {
	ioffset++;
	if (!bu_cmd_integer_from_str(&contour_ind, argv[0]) || contour_ind < 0) {
	    bu_vls_printf(gedp->ged_result_str, "Invalid argument %s\n", argv[0]);
	    return BRLCAD_ERROR;
	}
	p->curr_contour_i = contour_ind;
    } else {
	contour_ind = 0;
	p->curr_contour_i = contour_ind;
    }
    int x,y;
	if (!bu_cmd_integer_from_str(&x, argv[ioffset]) || x < 0) {
	bu_vls_printf(gedp->ged_result_str, "Invalid argument %s\n", argv[ioffset]);
	return BRLCAD_ERROR;
    }
	if (!bu_cmd_integer_from_str(&y, argv[ioffset+1]) || y < 0) {
	bu_vls_printf(gedp->ged_result_str, "Invalid argument %s\n", argv[ioffset+1]);
	return BRLCAD_ERROR;
    }

    p->curr_contour_i = contour_ind;
    s->s_v->gv_mouse_x = x;
    s->s_v->gv_mouse_y = y;
    bv_screen_pt(&s->s_v->gv_point, (fastf_t)x, (fastf_t)y, gedp->ged_gvp);

    bv_update_polygon(s, s->s_v, BV_POLYGON_UPDATE_PT_SELECT);

    return BRLCAD_OK;
}


int
_poly_cmd_append(void *bs, int argc, const char **argv)
{
    struct _ged_view_info *gd = (struct _ged_view_info *)bs;
    struct ged *gedp = gd->gedp;
    const char *usage_string = "view obj <objname> polygon append [contour] x y";
    const char *purpose_string = "append point to polygon";
    if (_view_cmd_msgs(bs, argc, argv, usage_string, purpose_string))
	return BRLCAD_OK;

    if (polygon_preflight(gedp, &polygon_append_schema, argc, argv) != BRLCAD_OK)
	return BRLCAD_ERROR;

    argc--; argv++;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    struct bv_scene_obj *s = gd->s;
    if (!s) {
	bu_vls_printf(gedp->ged_result_str, "View object %s does not exist\n", gd->vobj);
	return BRLCAD_ERROR;
    }
    if (!(s->s_type_flags & BV_VIEWONLY) || !(s->s_type_flags & BV_POLYGONS)) {
	bu_vls_printf(gedp->ged_result_str, "Specified object is not a view polygon.\n");
	return BRLCAD_ERROR;
    }

    struct bv_polygon *p = (struct bv_polygon *)s->s_i_data;
    if (p->type != BV_POLYGON_GENERAL) {
	bu_vls_printf(gedp->ged_result_str, "Point appending is only supported for general polygons - specified object defines a constrained shape\n");
	return BRLCAD_ERROR;
    }

    int ioffset = 0;
    int contour_ind = -1;
    if (argc == 3) {
	ioffset++;
	if (!bu_cmd_integer_from_str(&contour_ind, argv[0]) || contour_ind < 0) {
	    bu_vls_printf(gedp->ged_result_str, "Invalid argument %s\n", argv[0]);
	    return BRLCAD_ERROR;
	}
	p->curr_contour_i = contour_ind;
    } else {
	p->curr_contour_i = 0;
    }
    int x,y;
	if (!bu_cmd_integer_from_str(&x, argv[ioffset]) || x < 0) {
	bu_vls_printf(gedp->ged_result_str, "Invalid argument %s\n", argv[ioffset]);
	return BRLCAD_ERROR;
    }
	if (!bu_cmd_integer_from_str(&y, argv[ioffset+1]) || y < 0) {
	bu_vls_printf(gedp->ged_result_str, "Invalid argument %s\n", argv[ioffset+1]);
	return BRLCAD_ERROR;
    }

    s->s_v->gv_mouse_x = x;
    s->s_v->gv_mouse_y = y;
    bv_screen_pt(&s->s_v->gv_point, (fastf_t)x, (fastf_t)y, gedp->ged_gvp);
    bv_update_polygon(s, s->s_v, BV_POLYGON_UPDATE_PT_APPEND);

    return BRLCAD_OK;
}

int
_poly_cmd_move(void *bs, int argc, const char **argv)
{
    struct _ged_view_info *gd = (struct _ged_view_info *)bs;
    struct ged *gedp = gd->gedp;
    const char *usage_string = "view obj <objname> polygon move x y";
    const char *purpose_string = "move selected polygon point to x,y";
    if (_view_cmd_msgs(bs, argc, argv, usage_string, purpose_string))
	return BRLCAD_OK;

    if (polygon_preflight(gedp, &polygon_move_schema, argc, argv) != BRLCAD_OK)
	return BRLCAD_ERROR;

    argc--; argv++;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    struct bv_scene_obj *s = gd->s;
    if (!s) {
	bu_vls_printf(gedp->ged_result_str, "View object %s does not exist\n", gd->vobj);
	return BRLCAD_ERROR;
    }
    if (!(s->s_type_flags & BV_VIEWONLY) || !(s->s_type_flags & BV_POLYGONS)) {
	bu_vls_printf(gedp->ged_result_str, "Specified object is not a view polygon.\n");
	return BRLCAD_ERROR;
    }

    struct bv_polygon *p = (struct bv_polygon *)s->s_i_data;
    if (p->type != BV_POLYGON_GENERAL) {
	bu_vls_printf(gedp->ged_result_str, "Individual point movement is only supported for general polygons - specified object defines a constrained shape.  Use \"update\" to adjust constrained shapes.\n");
	return BRLCAD_ERROR;
    }

    int x,y;
	if (!bu_cmd_integer_from_str(&x, argv[0]) || x < 0) {
	bu_vls_printf(gedp->ged_result_str, "Invalid argument %s\n", argv[0]);
	return BRLCAD_ERROR;
    }
	if (!bu_cmd_integer_from_str(&y, argv[1]) || y < 0) {
	bu_vls_printf(gedp->ged_result_str, "Invalid argument %s\n", argv[1]);
	return BRLCAD_ERROR;
    }

    s->s_v->gv_mouse_x = x;
    s->s_v->gv_mouse_y = y;
    bv_screen_pt(&s->s_v->gv_point, (fastf_t)x, (fastf_t)y, gedp->ged_gvp);
    bv_update_polygon(s, s->s_v, BV_POLYGON_UPDATE_PT_MOVE);

    return BRLCAD_OK;
}

int
_poly_cmd_clear(void *bs, int argc, const char **argv)
{
    struct _ged_view_info *gd = (struct _ged_view_info *)bs;
    struct ged *gedp = gd->gedp;
    const char *usage_string = "view obj <objname> polygon clear";
    const char *purpose_string = "clear all modification flags";
    if (_view_cmd_msgs(bs, argc, argv, usage_string, purpose_string))
	return BRLCAD_OK;

    if (polygon_preflight(gedp, &polygon_clear_schema, argc, argv) != BRLCAD_OK)
	return BRLCAD_ERROR;

    argc--; argv++;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    struct bv_scene_obj *s = gd->s;
    if (!s) {
	bu_vls_printf(gedp->ged_result_str, "View object %s does not exist\n", gd->vobj);
	return BRLCAD_ERROR;
    }
    if (!(s->s_type_flags & BV_VIEWONLY) || !(s->s_type_flags & BV_POLYGONS)) {
	bu_vls_printf(gedp->ged_result_str, "Specified object is not a view polygon.\n");
	return BRLCAD_ERROR;
    }

    struct bv_polygon *p = (struct bv_polygon *)s->s_i_data;
    p->curr_contour_i = 0;
    p->curr_point_i = -1;
    bv_update_polygon(s, s->s_v, BV_POLYGON_UPDATE_DEFAULT);

    return BRLCAD_OK;
}

int
_poly_cmd_close(void *bs, int argc, const char **argv)
{
    struct _ged_view_info *gd = (struct _ged_view_info *)bs;
    struct ged *gedp = gd->gedp;
    const char *usage_string = "view obj <objname> polygon close [ind]";
    const char *purpose_string = "contour -> polygon";
    if (_view_cmd_msgs(bs, argc, argv, usage_string, purpose_string))
	return BRLCAD_OK;

    if (polygon_preflight(gedp, &polygon_close_schema, argc, argv) != BRLCAD_OK)
	return BRLCAD_ERROR;

    argc--; argv++;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    struct bv_scene_obj *s = gd->s;
    if (!s) {
	bu_vls_printf(gedp->ged_result_str, "View object %s does not exist\n", gd->vobj);
	return BRLCAD_ERROR;
    }
    if (!(s->s_type_flags & BV_VIEWONLY) || !(s->s_type_flags & BV_POLYGONS)) {
	bu_vls_printf(gedp->ged_result_str, "Specified object is not a view polygon.\n");
	return BRLCAD_ERROR;
    }

    struct bv_polygon *p = (struct bv_polygon *)s->s_i_data;
    if (p->type != BV_POLYGON_GENERAL) {
	return BRLCAD_OK;
    }

    int ind = -1;
    if (argc && !bu_cmd_integer_from_str(&ind, argv[0]))
	return BRLCAD_ERROR;
    if (ind >= 0 && (size_t)ind >= p->polygon.num_contours) {
	bu_vls_printf(gedp->ged_result_str, "Invalid contour index %d\n", ind);
	return BRLCAD_ERROR;
    }

   if (ind < 0) {
       // Close all contours
       for (size_t i = 0; i < p->polygon.num_contours; i++) {
	   p->polygon.contour[i].open = 0;
       }
   } else {
       p->polygon.contour[ind].open = 0;
   }

    bv_update_polygon(s, s->s_v, BV_POLYGON_UPDATE_DEFAULT);

    return BRLCAD_OK;
}

int
_poly_cmd_open(void *bs, int argc, const char **argv)
{
    struct _ged_view_info *gd = (struct _ged_view_info *)bs;
    struct ged *gedp = gd->gedp;
    const char *usage_string = "view obj <objname> polygon open [ind]";
    const char *purpose_string = "polygon -> contour";
    if (_view_cmd_msgs(bs, argc, argv, usage_string, purpose_string))
	return BRLCAD_OK;

    if (polygon_preflight(gedp, &polygon_open_schema, argc, argv) != BRLCAD_OK)
	return BRLCAD_ERROR;

    argc--; argv++;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    struct bv_scene_obj *s = gd->s;
    if (!s) {
	bu_vls_printf(gedp->ged_result_str, "View object %s does not exist\n", gd->vobj);
	return BRLCAD_ERROR;
    }
    if (!(s->s_type_flags & BV_VIEWONLY) || !(s->s_type_flags & BV_POLYGONS)) {
	bu_vls_printf(gedp->ged_result_str, "Specified object is not a view polygon.\n");
	return BRLCAD_ERROR;
    }

    struct bv_polygon *p = (struct bv_polygon *)s->s_i_data;
    if (p->type != BV_POLYGON_GENERAL) {
	bu_vls_printf(gedp->ged_result_str, "Constrained polygon shapes are always closed.\n");
	return BRLCAD_ERROR;
    }

    int ind = -1;
    if (argc && !bu_cmd_integer_from_str(&ind, argv[0]))
	return BRLCAD_ERROR;
    if (ind >= 0 && (size_t)ind >= p->polygon.num_contours) {
	bu_vls_printf(gedp->ged_result_str, "Invalid contour index %d\n", ind);
	return BRLCAD_ERROR;
    }

   if (ind < 0) {
       // Open all contours
       for (size_t i = 0; i < p->polygon.num_contours; i++) {
	   p->polygon.contour[i].open = 1;
       }
   } else {
       p->polygon.contour[ind].open = 1;
   }

    bv_update_polygon(s, s->s_v, BV_POLYGON_UPDATE_DEFAULT);

    return BRLCAD_OK;
}

int
_poly_cmd_area(void *bs, int argc, const char **argv)
{
    struct _ged_view_info *gd = (struct _ged_view_info *)bs;
    struct ged *gedp = gd->gedp;
    const char *usage_string = "view obj <objname> polygon area";
    const char *purpose_string = "report polygon area";
    if (_view_cmd_msgs(bs, argc, argv, usage_string, purpose_string))
	return BRLCAD_OK;

    if (polygon_preflight(gedp, &polygon_area_schema, argc, argv) != BRLCAD_OK)
	return BRLCAD_ERROR;

    argc--; argv++;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    struct bv_scene_obj *s = gd->s;
    if (!s) {
	bu_vls_printf(gedp->ged_result_str, "View object %s does not exist\n", gd->vobj);
	return BRLCAD_ERROR;
    }
    if (!(s->s_type_flags & BV_VIEWONLY) || !(s->s_type_flags & BV_POLYGONS)) {
	bu_vls_printf(gedp->ged_result_str, "Specified object is not a view polygon.\n");
	return BRLCAD_ERROR;
    }

    struct bv_polygon *p = (struct bv_polygon *)s->s_i_data;

    double area = bg_find_polygon_area(&p->polygon, CLIPPER_MAX, &p->vp, s->s_v->gv_scale);

    if (gedp->dbip) {
	bu_vls_printf(gedp->ged_result_str, "%g", area * gedp->dbip->dbi_base2local);
    } else {
	bu_vls_printf(gedp->ged_result_str, "%g", area);
    }
    return BRLCAD_OK;
}

int
_poly_cmd_overlap(void *bs, int argc, const char **argv)
{
    struct _ged_view_info *gd = (struct _ged_view_info *)bs;
    struct ged *gedp = gd->gedp;
    struct rt_wdb *wdbp = NULL;
    const char *usage_string = "view obj <obj1> polygon overlap <obj2>";
    const char *purpose_string = "report if two polygons overlap";
    if (_view_cmd_msgs(bs, argc, argv, usage_string, purpose_string))
	return BRLCAD_OK;

    if (polygon_preflight(gedp, &polygon_overlap_schema, argc, argv) != BRLCAD_OK)
	return BRLCAD_ERROR;

    argc--; argv++;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    struct bv_scene_obj *s = gd->s;
    if (!s) {
	bu_vls_printf(gedp->ged_result_str, "View object %s does not exist\n", gd->vobj);
	return BRLCAD_ERROR;
    }
    if (!(s->s_type_flags & BV_VIEWONLY) || !(s->s_type_flags & BV_POLYGONS)) {
	bu_vls_printf(gedp->ged_result_str, "Specified object is not a view polygon.\n");
	return BRLCAD_ERROR;
    }

    if (!gedp->dbip) {
	bu_vls_printf(gedp->ged_result_str, "no database open\n");
	return BRLCAD_ERROR;
    }
    wdbp = wdb_dbopen(gedp->dbip, RT_WDB_TYPE_DB_DEFAULT);
    if (!wdbp) {
	bu_vls_printf(gedp->ged_result_str, "unable to open database context\n");
	return BRLCAD_ERROR;
    }

    if (!argc) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s\n", usage_string);
	return BRLCAD_ERROR;
    }

    // Look up the polygon to check for overlaps
    struct bview *v = gd->cv;
    struct bv_scene_obj *s2 = NULL;
    struct bu_ptbl *view_objs = bv_view_objs(v, BV_VIEW_OBJS);
    if (view_objs) {
	for (size_t i = 0; i < BU_PTBL_LEN(view_objs); i++) {
	    struct bv_scene_obj *stest = (struct bv_scene_obj *)BU_PTBL_GET(view_objs, i);
	    if (BU_STR_EQUAL(argv[0], bu_vls_cstr(&stest->s_name))) {
		s2 = stest;
		break;
	    }
	}
    }
    if (!s2) {
	struct bu_ptbl *local_view_objs = bv_view_objs(v, BV_VIEW_OBJS | BV_LOCAL_OBJS);
	if (local_view_objs) {
	    for (size_t i = 0; i < BU_PTBL_LEN(local_view_objs); i++) {
		struct bv_scene_obj *stest = (struct bv_scene_obj *)BU_PTBL_GET(local_view_objs, i);
		if (BU_STR_EQUAL(argv[0], bu_vls_cstr(&stest->s_name))) {
		    s2 = stest;
		    break;
		}
	    }
	}
    }
    if (!s2) {
	bu_vls_printf(gedp->ged_result_str, "View object %s does not exist\n", argv[0]);
	return BRLCAD_ERROR;
    }
    if (!(s2->s_type_flags & BV_VIEWONLY) || !(s2->s_type_flags & BV_POLYGONS)) {
	bu_vls_printf(gedp->ged_result_str, "%s is not a view polygon.\n", argv[0]);
	return BRLCAD_ERROR;
    }

    // Have two polygons.  Check for overlaps, using the origin plane of the
    // obj1 polygon.
    struct bv_polygon *polyA = (struct bv_polygon *)s->s_i_data;
    struct bv_polygon *polyB = (struct bv_polygon *)s2->s_i_data;

    int ovlp = bg_polygons_overlap(&polyA->polygon, &polyB->polygon, &polyA->vp, &wdbp->wdb_tol, v->gv_scale);

    bu_vls_printf(gedp->ged_result_str, "%d", ovlp);

    return BRLCAD_OK;
}

struct segment_node {
    struct bu_list l;
    int reverse;
    int used;
    void *segment;
};

struct contour_node {
    struct bu_list l;
    struct bu_list head;
};

int
_poly_cmd_import(void *bs, int argc, const char **argv)
{
    struct _ged_view_info *gd = (struct _ged_view_info *)bs;
    struct ged *gedp = gd->gedp;
    const char *usage_string = "view obj <objname> polygon import <sketchname>";
    const char *purpose_string = "import polygon from sketch";
    if (_view_cmd_msgs(bs, argc, argv, usage_string, purpose_string))
	return BRLCAD_OK;

    if (polygon_preflight(gedp, &polygon_import_schema, argc, argv) != BRLCAD_OK)
	return BRLCAD_ERROR;

    argc--; argv++;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    struct bv_scene_obj *s = gd->s;
    if (s) {
	bu_vls_printf(gedp->ged_result_str, "View object named %s already exists\n", gd->vobj);
	return BRLCAD_ERROR;
    }

    if (!argc) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s\n", usage_string);
	return BRLCAD_ERROR;
    }

    if (!gedp->dbip) {
	bu_vls_printf(gedp->ged_result_str, "no database open\n");
	return BRLCAD_ERROR;
    }

    // Begin import
    struct directory *dp = db_lookup(gedp->dbip, argv[0], LOOKUP_QUIET);
    if (dp == RT_DIR_NULL) {
	return BRLCAD_ERROR;
    }

    int flags = BV_VIEW_OBJS;
    if (gd->local_obj)
	flags |= BV_LOCAL_OBJS;
    s = db_sketch_to_scene_obj(gd->vobj, gedp->dbip, dp, gd->cv, flags);
    if (!s) {
	bu_vls_printf(gedp->ged_result_str, "Failed to create %s\n", gd->vobj);
	return BRLCAD_ERROR;
    }

    return BRLCAD_OK;
}

int
_poly_cmd_export(void *bs, int argc, const char **argv)
{
    struct _ged_view_info *gd = (struct _ged_view_info *)bs;
    struct ged *gedp = gd->gedp;
    const char *usage_string = "view obj <objname> polygon export <sketchname>";
    const char *purpose_string = "export polygon to sketch";
    if (_view_cmd_msgs(bs, argc, argv, usage_string, purpose_string))
	return BRLCAD_OK;

    if (polygon_preflight(gedp, &polygon_export_schema, argc, argv) != BRLCAD_OK)
	return BRLCAD_ERROR;

    argc--; argv++;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    struct bv_scene_obj *s = gd->s;
    if (!s) {
	bu_vls_printf(gedp->ged_result_str, "View object %s does not exist\n", gd->vobj);
	return BRLCAD_ERROR;
    }
    if (!(s->s_type_flags & BV_VIEWONLY) || !(s->s_type_flags & BV_POLYGONS)) {
	bu_vls_printf(gedp->ged_result_str, "Specified object is not a view polygon.\n");
	return BRLCAD_ERROR;
    }

    if (!argc) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s\n", usage_string);
	return BRLCAD_ERROR;
    }

    if (!gedp->dbip) {
	bu_vls_printf(gedp->ged_result_str, "no database open\n");
	return BRLCAD_ERROR;
    }

    GED_CHECK_EXISTS(gedp, argv[0], LOOKUP_QUIET, BRLCAD_ERROR);

    if (db_scene_obj_to_sketch(gedp->dbip, argv[0], s) != BRLCAD_OK) {
	bu_vls_printf(gedp->ged_result_str, "Failed to create sketch.\n");
	return BRLCAD_ERROR;
    }

    return BRLCAD_OK;
}

int
_poly_cmd_fill(void *bs, int argc, const char **argv)
{
    struct _ged_view_info *gd = (struct _ged_view_info *)bs;
    struct ged *gedp = gd->gedp;
    const char *usage_string = "view obj <obj1> polygon fill [dx dy spacing]";
    const char *purpose_string = "use lines to visualize polygon interior";
    if (_view_cmd_msgs(bs, argc, argv, usage_string, purpose_string))
	return BRLCAD_OK;

    if (polygon_preflight(gedp, &polygon_fill_schema, argc, argv) != BRLCAD_OK)
	return BRLCAD_ERROR;

    argc--; argv++;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    struct bv_scene_obj *s = gd->s;
    if (!s) {
	bu_vls_printf(gedp->ged_result_str, "View object %s does not exist\n", gd->vobj);
	return BRLCAD_ERROR;
    }
    if (!(s->s_type_flags & BV_VIEWONLY) || !(s->s_type_flags & BV_POLYGONS)) {
	bu_vls_printf(gedp->ged_result_str, "Specified object is not a view polygon.\n");
	return BRLCAD_ERROR;
    }

    if (argc == 1 && BU_STR_EQUAL(argv[0], "0")) {
	struct bv_polygon *p = (struct bv_polygon *)s->s_i_data;
	p->fill_flag = 0;
	bv_update_polygon(s, s->s_v, BV_POLYGON_UPDATE_DEFAULT);
	return BRLCAD_OK;
    }

    vect2d_t vdir;
    fastf_t vdelta;
	if (!bu_cmd_number_from_str(&vdir[0], argv[0])) {
	bu_vls_printf(gedp->ged_result_str, "Invalid argument %s\n", argv[0]);
	return BRLCAD_ERROR;
    }
	if (!bu_cmd_number_from_str(&vdir[1], argv[1])) {
	bu_vls_printf(gedp->ged_result_str, "Invalid argument %s\n", argv[1]);
	return BRLCAD_ERROR;
    }
	if (!bu_cmd_number_from_str(&vdelta, argv[2])) {
	bu_vls_printf(gedp->ged_result_str, "Invalid argument %s\n", argv[2]);
	return BRLCAD_ERROR;
    }

    struct bv_polygon *p = (struct bv_polygon *)s->s_i_data;
    p->fill_flag = 1;
    V2MOVE(p->fill_dir, vdir);
    p->fill_delta = vdelta;
    bv_update_polygon(s, s->s_v, BV_POLYGON_UPDATE_DEFAULT);

    return BRLCAD_OK;
}

int
_poly_cmd_fill_color(void *bs, int argc, const char **argv)
{
    struct _ged_view_info *gd = (struct _ged_view_info *)bs;
    struct ged *gedp = gd->gedp;
    const char *usage_string = "view obj <obj1> polygon fill_color [r/g/b]";
    const char *purpose_string = "customize fill lines color (if fill is enabled)";
    if (_view_cmd_msgs(bs, argc, argv, usage_string, purpose_string))
	return BRLCAD_OK;

    if (polygon_preflight(gedp, &polygon_fill_color_schema, argc, argv) != BRLCAD_OK)
	return BRLCAD_ERROR;

    argc--; argv++;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    struct bv_scene_obj *s = gd->s;
    if (!s) {
	bu_vls_printf(gedp->ged_result_str, "View object %s does not exist\n", gd->vobj);
	return BRLCAD_ERROR;
    }
    if (!(s->s_type_flags & BV_VIEWONLY) || !(s->s_type_flags & BV_POLYGONS)) {
	bu_vls_printf(gedp->ged_result_str, "Specified object is not a view polygon.\n");
	return BRLCAD_ERROR;
    }

    struct bv_polygon *p = (struct bv_polygon *)s->s_i_data;

    if (!argc) {
	unsigned char frgb[3];
	bu_color_to_rgb_chars(&p->fill_color, (unsigned char *)frgb);

	bu_vls_printf(gedp->ged_result_str, "%d/%d/%d\n", frgb[0], frgb[1], frgb[2]);

	return BRLCAD_OK;
    }

	if (bu_cmd_color_from_argv(&p->fill_color, (size_t)argc,
	(const char * const *)argv) != argc) {
	bu_vls_printf(gedp->ged_result_str, "Invalid fill color\n");
	return BRLCAD_ERROR;
    }

    struct bv_scene_obj *fobj = bv_find_child(s, "*fill*");
    if (fobj) {
	bu_color_to_rgb_chars(&p->fill_color, fobj->s_color);
    }

    return BRLCAD_OK;
}

int
_poly_cmd_csg(void *bs, int argc, const char **argv)
{
    struct _ged_view_info *gd = (struct _ged_view_info *)bs;
    struct ged *gedp = gd->gedp;
    const char *usage_string = "view obj <obj1> polygon csg <u|-|+> <obj2>";
    const char *purpose_string = "replace obj1's polygon with the result of obj2 u/-/+ obj1";
    if (_view_cmd_msgs(bs, argc, argv, usage_string, purpose_string))
	return BRLCAD_OK;

    if (polygon_preflight(gedp, &polygon_csg_schema, argc, argv) != BRLCAD_OK)
	return BRLCAD_ERROR;

    argc--; argv++;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    struct bv_scene_obj *s = gd->s;
    if (!s) {
	bu_vls_printf(gedp->ged_result_str, "View object %s does not exist\n", gd->vobj);
	return BRLCAD_ERROR;
    }
    if (!(s->s_type_flags & BV_VIEWONLY) || !(s->s_type_flags & BV_POLYGONS)) {
	bu_vls_printf(gedp->ged_result_str, "Specified object is not a view polygon.\n");
	return BRLCAD_ERROR;
    }

    bg_clip_t op = bg_Union;
    char c = argv[0][0];
    switch (c) {
	case 'u':
	    op = bg_Union;
	    break;
	case '-':
	    op = bg_Difference;
	    break;
	case '+':
	    op = bg_Intersection;
	    break;
	default:
	    bu_vls_printf(gedp->ged_result_str, "Invalid boolean operator \"%s\"\n", argv[0]);
	    return BRLCAD_ERROR;
    }

    // Look up the polygon to check for overlaps
    struct bview *v = gd->cv;
    struct bv_scene_obj *s2 = NULL;
    struct bu_ptbl *view_objs = bv_view_objs(v, BV_VIEW_OBJS);
    if (view_objs) {
	for (size_t i = 0; i < BU_PTBL_LEN(view_objs); i++) {
	    struct bv_scene_obj *stest = (struct bv_scene_obj *)BU_PTBL_GET(view_objs, i);
		if (BU_STR_EQUAL(argv[1], bu_vls_cstr(&stest->s_name))) {
		s2 = stest;
		break;
	    }
	}
    }
    if (!s2) {
	struct bu_ptbl *local_view_objs = bv_view_objs(v, BV_VIEW_OBJS | BV_LOCAL_OBJS);
	if (local_view_objs) {
	    for (size_t i = 0; i < BU_PTBL_LEN(local_view_objs); i++) {
		struct bv_scene_obj *stest = (struct bv_scene_obj *)BU_PTBL_GET(local_view_objs, i);
		if (BU_STR_EQUAL(argv[1], bu_vls_cstr(&stest->s_name))) {
		    s2 = stest;
		    break;
		}
	    }
	}
    }
    if (!s2) {
	bu_vls_printf(gedp->ged_result_str, "View object %s does not exist\n", argv[1]);
	return BRLCAD_ERROR;
    }
    if (!(s2->s_type_flags & BV_VIEWONLY) || !(s2->s_type_flags & BV_POLYGONS)) {
	bu_vls_printf(gedp->ged_result_str, "%s is not a view polygon.\n", argv[1]);
	return BRLCAD_ERROR;
    }

    // Have two polygons.  Check for overlaps, using the origin view of the
    // obj1 polygon.
    struct bv_polygon *polyA = (struct bv_polygon *)s->s_i_data;
    struct bv_polygon *polyB = (struct bv_polygon *)s2->s_i_data;

    struct bg_polygon *cp = bg_clip_polygon(op, &polyA->polygon, &polyB->polygon, CLIPPER_MAX, &polyA->vp);

    if (!cp)
	return BRLCAD_ERROR;

    bg_polygon_free(&polyA->polygon);
    polyA->polygon.num_contours = cp->num_contours;
    polyA->polygon.hole = cp->hole;
    polyA->polygon.contour = cp->contour;

    // clipper results are always general polygons
    polyA->type = BV_POLYGON_GENERAL;

    BU_PUT(cp, struct bg_polygon);
    bv_update_polygon(s, s->s_v, BV_POLYGON_UPDATE_DEFAULT);

    return BRLCAD_OK;
}

int
_view_cmd_polygons(void *bs, int argc, const char **argv)
{
    struct _ged_view_info *gd = (struct _ged_view_info *)bs;
    struct ged *gedp = gd->gedp;
    struct ged_view_polygon_args args = {0, 0};
    int child_start;
    int ret = BRLCAD_ERROR;

    const char *usage_string = "view obj <objname> polygon [options] [args]";
    const char *purpose_string = "manipulate view polygons";
    if (_view_cmd_msgs(bs, argc, argv, usage_string, purpose_string))
	return BRLCAD_OK;

    if (!gd->cv) {
	bu_vls_printf(gedp->ged_result_str, ": no view current in GED");
	return BRLCAD_ERROR;
    }


    argc--; argv++;
    child_start = bu_cmd_schema_parse(&ged_view_polygon_schema, &args,
	gedp->ged_result_str, argc, argv);
    if (child_start < 0) {
	char *help = bu_cmd_tree_describe(&ged_view_polygon_tree);
	if (help) {
	    bu_vls_strcat(gedp->ged_result_str, help);
	    bu_free(help, "view polygon native help");
	}
	return BRLCAD_ERROR;
    }
    gd->gopts = NULL;
    if (args.print_help) {
	char *help = bu_cmd_tree_describe(&ged_view_polygon_tree);
	if (help) {
	    bu_vls_strcat(gedp->ged_result_str, help);
	    bu_free(help, "view polygon native help");
	}
	return GED_HELP;
    }
    if (child_start >= argc) {
	bu_vls_strcat(gedp->ged_result_str, "polygon subcommand required\n");
	return BRLCAD_ERROR;
    }
    if (bu_cmd_tree_dispatch(&ged_view_polygon_tree, gd, argc - child_start,
	argv + child_start, &ret) == 0)
	return ret;
    bu_vls_printf(gedp->ged_result_str, "unknown polygon subcommand: %s\n",
	argv[child_start]);
    return BRLCAD_ERROR;
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
