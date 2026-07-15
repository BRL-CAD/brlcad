/*                        O B J S . C P P
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
/** @file libged/view/objs.c
 *
 * Commands for view objects.
 *
 */

#include "common.h"

#include <ctype.h>
#include <cstdlib>
#include <cstring>
#include <queue>

extern "C" {
#include "bu/color.h"
#include "bu/cmdschema.h"
#include "bu/vls.h"
#include "bv.h"
#include "raytrace.h"
}
#include "./ged_view.h"
#include "../ged_private.h"

struct ged_view_obj_args {
    int print_help;
    int local_only;
    int geom_only;
    int view_only;
};

struct ged_view_obj_color_args {
    int recurse;
};

int _objs_cmd_draw(void *bs, int argc, const char **argv);
int _objs_cmd_delete(void *bs, int argc, const char **argv);
int _objs_cmd_color(void *bs, int argc, const char **argv);
int _objs_cmd_arrow(void *bs, int argc, const char **argv);
int _objs_cmd_lcnt(void *bs, int argc, const char **argv);
int _objs_cmd_update(void *bs, int argc, const char **argv);

static int ged_view_obj_validate(const struct bu_cmd_schema *schema, size_t argc,
    const char **argv, size_t cursor_arg, struct bu_cmd_validate_result *result);
static int ged_view_obj_color_validate(const struct bu_cmd_schema *schema, size_t argc,
    const char **argv, size_t cursor_arg, struct bu_cmd_validate_result *result);
static int ged_view_obj_arrow_validate(const struct bu_cmd_schema *schema, size_t argc,
    const char **argv, size_t cursor_arg, struct bu_cmd_validate_result *result);
static int ged_view_obj_update_validate(const struct bu_cmd_schema *schema, size_t argc,
    const char **argv, size_t cursor_arg, struct bu_cmd_validate_result *result);

static const struct bu_cmd_option ged_view_obj_options[] = {
    BU_CMD_FLAG("h", "help", struct ged_view_obj_args, print_help,
	"Print help and exit"),
    BU_CMD_ALIAS_SHORT("?", "help", 1),
    BU_CMD_FLAG("L", "local", struct ged_view_obj_args, local_only,
	"Restrict lookup or creation to the current view"),
    BU_CMD_FLAG("G", "geom_only", struct ged_view_obj_args, geom_only,
	"List or select geometry-backed scene objects"),
    BU_CMD_FLAG(NULL, "view_only", struct ged_view_obj_args, view_only,
	"List or select view-only scene objects"),
    BU_CMD_OPTION_NULL
};
static const struct bu_cmd_operand ged_view_obj_operands[] = {
    BU_CMD_OPERAND("object", BU_CMD_VALUE_STRING, 0, 1,
	"View-object name", NULL),
    BU_CMD_OPERAND("arguments", BU_CMD_VALUE_RAW, 0, BU_CMD_COUNT_UNLIMITED,
	"Object operation and arguments", NULL),
    BU_CMD_OPERAND_NULL
};
extern "C" GED_EXPORT const struct bu_cmd_schema ged_view_obj_schema = {
    "obj", "Manage view objects", ged_view_obj_options, ged_view_obj_operands,
    BU_CMD_PARSE_OPTIONS_FIRST, BU_CMD_SCHEMA_CONSTRAINTS(ged_view_obj_validate, NULL)
};

static const struct bu_cmd_operand ged_view_obj_raw_operands[] = {
    BU_CMD_OPERAND("arguments", BU_CMD_VALUE_RAW, 0, BU_CMD_COUNT_UNLIMITED,
	"Object-operation arguments", NULL),
    BU_CMD_OPERAND_NULL
};
static const char * const ged_view_obj_draw_states[] = {"UP", "DOWN", NULL};
static const char * const ged_view_obj_arrow_actions[] = {
    "0", "1", "width", "length", NULL
};
static const struct bu_cmd_operand ged_view_obj_draw_operands[] = {
    BU_CMD_OPERAND_KEYWORDS("state", BU_CMD_VALUE_KEYWORD, 0, 1,
	"UP or DOWN", NULL, ged_view_obj_draw_states),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_operand ged_view_obj_color_operands[] = {
    BU_CMD_OPERAND_SHAPED("color", BU_CMD_VALUE_RAW, 0, 3, NULL,
	"Packed color or three RGB components", "ged.color", &bu_cmd_color_arg_shape),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_operand ged_view_obj_arrow_operands[] = {
    BU_CMD_OPERAND("action", BU_CMD_VALUE_RAW, 0, 2,
	"Arrow action and optional numeric value", NULL),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_operand ged_view_obj_update_operands[] = {
    BU_CMD_OPERAND_VALIDATE("x", BU_CMD_VALUE_INTEGER, 0, 1,
	bu_cmd_nonnegative_integer_validate, "Nonnegative screen X coordinate", NULL),
    BU_CMD_OPERAND_VALIDATE("y", BU_CMD_VALUE_INTEGER, 0, 1,
	bu_cmd_nonnegative_integer_validate, "Nonnegative screen Y coordinate", NULL),
    BU_CMD_OPERAND_NULL
};
#define GED_VIEW_OBJ_RAW_SCHEMA(_name, _help) \
    static const struct bu_cmd_schema ged_view_obj_##_name##_schema = { \
	#_name, _help, NULL, ged_view_obj_raw_operands, \
	BU_CMD_PARSE_STOP_AT_FIRST_OPERAND, BU_CMD_SCHEMA_CONSTRAINTS(NULL, NULL) \
	}
static const struct bu_cmd_option ged_view_obj_color_options[] = {
    BU_CMD_FLAG("r", "recursive", struct ged_view_obj_color_args, recurse,
	"Report or set this object's child colors too"),
    BU_CMD_OPTION_NULL
};
static const struct bu_cmd_schema ged_view_obj_draw_schema = {
    "draw", "Query or set object drawing state", NULL, ged_view_obj_draw_operands,
    BU_CMD_PARSE_STOP_AT_FIRST_OPERAND, BU_CMD_SCHEMA_CONSTRAINTS(NULL, NULL)
};
static const struct bu_cmd_schema ged_view_obj_del_schema = {
    "del", "Delete a view-only object", NULL, NULL,
    BU_CMD_PARSE_STOP_AT_FIRST_OPERAND, BU_CMD_SCHEMA_CONSTRAINTS(NULL, NULL)
};
static const struct bu_cmd_schema ged_view_obj_update_schema = {
    "update", "Update a view object", NULL, ged_view_obj_update_operands,
    BU_CMD_PARSE_STOP_AT_FIRST_OPERAND, BU_CMD_SCHEMA_CONSTRAINTS(ged_view_obj_update_validate, NULL)
};
static const struct bu_cmd_schema ged_view_obj_color_schema = {
    "color", "Query or set object color", ged_view_obj_color_options,
    ged_view_obj_color_operands, BU_CMD_PARSE_OPTIONS_FIRST,
    BU_CMD_SCHEMA_CONSTRAINTS(ged_view_obj_color_validate, NULL)
};
static const struct bu_cmd_schema ged_view_obj_arrow_schema = {
    "arrow", "Query or set object arrow drawing", NULL, ged_view_obj_arrow_operands,
    BU_CMD_PARSE_STOP_AT_FIRST_OPERAND, BU_CMD_SCHEMA_CONSTRAINTS(ged_view_obj_arrow_validate, NULL)
};
static const struct bu_cmd_schema ged_view_obj_lcnt_schema = {
    "lcnt", "Report object vlist entity count", NULL, NULL,
    BU_CMD_PARSE_STOP_AT_FIRST_OPERAND, BU_CMD_SCHEMA_CONSTRAINTS(NULL, NULL)
};
#undef GED_VIEW_OBJ_RAW_SCHEMA

static const struct bu_cmd_schema ged_view_obj_child_root_schema = {
    "view_obj", "View-object operation", NULL, NULL,
    BU_CMD_PARSE_OPTIONS_FIRST, BU_CMD_SCHEMA_CONSTRAINTS(NULL, NULL)
};
static const struct bu_cmd_tree_node ged_view_obj_children[] = {
    BU_CMD_TREE_NODE(&ged_view_obj_draw_schema, NULL, NULL,
	BU_CMD_TREE_CHILD_AFTER_OPTIONS, _objs_cmd_draw),
    BU_CMD_TREE_NODE(&ged_view_obj_del_schema, NULL, NULL,
	BU_CMD_TREE_CHILD_AFTER_OPTIONS, _objs_cmd_delete),
    BU_CMD_TREE_NODE(&ged_view_obj_update_schema, NULL, NULL,
	BU_CMD_TREE_CHILD_AFTER_OPTIONS, _objs_cmd_update),
    BU_CMD_TREE_NODE(&ged_view_obj_color_schema, NULL, NULL,
	BU_CMD_TREE_CHILD_AFTER_OPTIONS, _objs_cmd_color),
	BU_CMD_TREE_NODE(&ged_view_axes_schema, NULL, ged_view_axes_subcommands,
	BU_CMD_TREE_CHILD_AFTER_OPTIONS, _view_cmd_axes),
    BU_CMD_TREE_NODE(&ged_view_obj_arrow_schema, NULL, NULL,
	BU_CMD_TREE_CHILD_AFTER_OPTIONS, _objs_cmd_arrow),
	BU_CMD_TREE_NODE(&ged_view_label_schema, NULL, ged_view_label_subcommands,
	BU_CMD_TREE_CHILD_AFTER_OPTIONS, _view_cmd_labels),
    BU_CMD_TREE_NODE(&ged_view_obj_lcnt_schema, NULL, NULL,
	BU_CMD_TREE_CHILD_AFTER_OPTIONS, _objs_cmd_lcnt),
	BU_CMD_TREE_NODE(&ged_view_line_schema, NULL, ged_view_line_subcommands,
	BU_CMD_TREE_CHILD_AFTER_OPTIONS, _view_cmd_lines),
	BU_CMD_TREE_NODE(&ged_view_polygon_schema, NULL, ged_view_polygon_subcommands,
	BU_CMD_TREE_CHILD_AFTER_OPTIONS, _view_cmd_polygons),
    BU_CMD_TREE_NODE_NULL
};
static const struct bu_cmd_tree ged_view_obj_child_tree = {
    &ged_view_obj_child_root_schema, ged_view_obj_children,
    BU_CMD_TREE_CHILD_AFTER_OPTIONS
};

static size_t
ged_view_obj_name_index(const struct bu_cmd_schema *schema, size_t argc,
    const char **argv)
{
    int options_allowed = 1;

    for (size_t i = 0; i < argc; i++) {
	int span;
	if (options_allowed && BU_STR_EQUAL(argv[i], "--")) {
	    options_allowed = 0;
	    continue;
	}
	span = options_allowed ? bu_cmd_schema_option_span(schema, argc - i, argv + i) : 0;
	if (span > 0) {
	    i += (size_t)span - 1;
	    continue;
	}
	if (options_allowed && argv[i][0] == '-' && argv[i][1])
	    return argc;
	return i;
    }
    return argc;
}

/* The compact parser deliberately owns all option spelling and option-value
 * details.  These two adapters only locate the first positional word after a
 * successful options-first phase so a parent command can delegate its
 * remaining words to a native value or child grammar. */
static size_t
ged_view_obj_operand_index(const struct bu_cmd_schema *schema, size_t argc,
    const char **argv)
{
    int options_allowed = 1;

    for (size_t i = 0; i < argc; i++) {
	int span;
	if (options_allowed && BU_STR_EQUAL(argv[i], "--")) {
	    options_allowed = 0;
	    continue;
	}
	span = options_allowed ? bu_cmd_schema_option_span(schema, argc - i, argv + i) : 0;
	if (span > 0) {
	    i += (size_t)span - 1;
	    continue;
	}
	return i;
    }
    return argc;
}

static void
ged_view_obj_set_validation(struct bu_cmd_validate_result *result,
    bu_cmd_validate_state_t state, size_t token, unsigned int expected,
    bu_cmd_value_t value_type, const char *hint)
{
    bu_cmd_validate_result_clear(result);
    result->state = state;
    result->token_start = token;
    result->token_end = token;
    result->expected = expected;
    result->completion_type = value_type;
    result->hint = hint;
}

static int
ged_view_obj_color_validate(const struct bu_cmd_schema *schema, size_t argc,
    const char **argv, size_t cursor_arg, struct bu_cmd_validate_result *result)
{
    struct bu_cmd_schema flat = *schema;
    size_t operand_start;
    int ret;

    flat.validation.custom_validate = NULL;
    ret = bu_cmd_schema_validate(&flat, argc, argv, cursor_arg, result);
    if (ret || result->state == BU_CMD_VALIDATE_INVALID)
	return ret;
    operand_start = ged_view_obj_operand_index(schema, argc, argv);
    if (cursor_arg < operand_start)
	return 0;
    ret = bu_cmd_color_optional_validate(argc - operand_start, argv + operand_start,
	cursor_arg - operand_start, result);
    if (!ret) {
	result->token_start += operand_start;
	result->token_end += operand_start;
	result->semantic_provider = "ged.color";
    }
    return ret;
}

static int
ged_view_obj_arrow_validate(const struct bu_cmd_schema *UNUSED(schema), size_t argc,
    const char **argv, size_t cursor_arg, struct bu_cmd_validate_result *result)
{
    const char *action = argc ? argv[0] : "";
    fastf_t numeric_value = 0.0;
    int action_known = 0;
    int numeric_action = 0;

    if (!result || cursor_arg > argc)
	return -1;
    for (size_t i = 0; ged_view_obj_arrow_actions[i]; i++) {
	if (BU_STR_EQUAL(action, ged_view_obj_arrow_actions[i])) {
	    action_known = 1;
	    break;
	}
    }
    numeric_action = BU_STR_EQUAL(action, "width") || BU_STR_EQUAL(action, "length");

    if (!argc) {
	ged_view_obj_set_validation(result, BU_CMD_VALIDATE_VALID, cursor_arg,
	    BU_CMD_EXPECT_OPERAND, BU_CMD_VALUE_KEYWORD, "arrow action");
	bu_cmd_keyword_candidates(result, ged_view_obj_arrow_actions, "");
	return 0;
    }
    if (!action_known) {
	ged_view_obj_set_validation(result, BU_CMD_VALIDATE_INVALID, 0,
	    BU_CMD_EXPECT_OPERAND, BU_CMD_VALUE_KEYWORD, "unknown arrow action");
	bu_cmd_keyword_candidates(result, ged_view_obj_arrow_actions, action);
	return 0;
    }
    if (cursor_arg == 0) {
	ged_view_obj_set_validation(result, BU_CMD_VALIDATE_VALID, 0,
	    BU_CMD_EXPECT_OPERAND, BU_CMD_VALUE_KEYWORD, "arrow action");
	bu_cmd_keyword_candidates(result, ged_view_obj_arrow_actions, action);
	return 0;
    }
    if (!numeric_action) {
	if (argc != 1) {
	    ged_view_obj_set_validation(result, BU_CMD_VALIDATE_INVALID, 1,
		BU_CMD_EXPECT_NONE, BU_CMD_VALUE_STRING,
		"arrow 0 and 1 do not take a value");
	    return 0;
	}
	ged_view_obj_set_validation(result, BU_CMD_VALIDATE_VALID, cursor_arg,
	    BU_CMD_EXPECT_NONE, BU_CMD_VALUE_KEYWORD, "arrow state");
	return 0;
    }
    if (argc > 2) {
	ged_view_obj_set_validation(result, BU_CMD_VALIDATE_INVALID, 2,
	    BU_CMD_EXPECT_NONE, BU_CMD_VALUE_STRING, "too many arrow arguments");
	return 0;
    }
    if (argc == 2 && !bu_cmd_number_from_str(&numeric_value, argv[1])) {
	ged_view_obj_set_validation(result, BU_CMD_VALIDATE_INVALID, 1,
	    BU_CMD_EXPECT_OPERAND, BU_CMD_VALUE_NUMBER, "invalid arrow size");
	return 0;
    }
    ged_view_obj_set_validation(result, BU_CMD_VALIDATE_VALID, cursor_arg,
	BU_CMD_EXPECT_OPERAND, BU_CMD_VALUE_NUMBER,
	argc == 1 ? "optional arrow size" : "arrow size");
    return 0;
}

static int
ged_view_obj_update_validate(const struct bu_cmd_schema *UNUSED(schema), size_t argc,
    const char **argv, size_t cursor_arg, struct bu_cmd_validate_result *result)
{
    int ret = bu_cmd_integer_pair_optional_validate(argc, argv, cursor_arg, result);

    if (ret || result->state == BU_CMD_VALIDATE_INVALID)
	return ret;
    for (size_t i = 0; i < argc; i++) {
	if (bu_cmd_nonnegative_integer_validate(NULL, argv[i]) != 0) {
	    ged_view_obj_set_validation(result, BU_CMD_VALIDATE_INVALID, i,
		BU_CMD_EXPECT_OPERAND, BU_CMD_VALUE_INTEGER,
		"screen coordinates must be nonnegative integers");
	    break;
	}
    }
    return 0;
}

static int
ged_view_obj_validate(const struct bu_cmd_schema *schema, size_t argc,
    const char **argv, size_t cursor_arg, struct bu_cmd_validate_result *result)
{
    struct bu_cmd_schema flat = *schema;
    size_t name_index;
    size_t child_start;
    int ret;

    flat.validation.custom_validate = NULL;
    ret = bu_cmd_schema_validate(&flat, argc, argv, cursor_arg, result);
    if (ret || result->state == BU_CMD_VALIDATE_INVALID)
	return ret;
    name_index = ged_view_obj_name_index(schema, argc, argv);
    if (name_index >= argc || cursor_arg <= name_index)
	return 0;
    child_start = name_index + 1;
    ret = bu_cmd_tree_validate_argv(&ged_view_obj_child_tree, argc - child_start,
	argv + child_start, cursor_arg - child_start, result);
    if (!ret) {
	result->token_start += child_start;
	result->token_end += child_start;
    }
    return ret;
}

static int
ged_view_obj_preflight(struct ged *gedp, const struct bu_cmd_schema *schema,
    void *args, int argc, const char **argv)
{
    return bu_cmd_schema_parse_complete(schema, args, gedp->ged_result_str,
	argc - 1, argv + 1) < 0 ? BRLCAD_ERROR : BRLCAD_OK;
}

int
_objs_cmd_draw(void *bs, int argc, const char **argv)
{
    struct _ged_view_info *gd = (struct _ged_view_info *)bs;
    struct ged *gedp = gd->gedp;
    const char *usage_string = "view obj name draw [0|1]";
    const char *purpose_string = "toggle view polygons";
    if (_view_cmd_msgs(bs, argc, argv, usage_string, purpose_string))
	return BRLCAD_OK;

    if (ged_view_obj_preflight(gedp, &ged_view_obj_draw_schema, NULL, argc, argv) != BRLCAD_OK)
	return BRLCAD_ERROR;

    argc--; argv++;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    struct bv_scene_obj *s = gd->s;
    if (!gd->s) {
	bu_vls_printf(gedp->ged_result_str, "No view object named %s\n", gd->vobj);
	return BRLCAD_ERROR;
    }

    if (argc == 0) {
	if (s->s_flag == UP) {
	    bu_vls_printf(gedp->ged_result_str, "UP\n");
	} else {
	    bu_vls_printf(gedp->ged_result_str, "DOWN\n");
	}
	return BRLCAD_OK;
    }

    if (BU_STR_EQUAL(argv[0], "DOWN")) {
	s->s_flag = DOWN;
	return BRLCAD_OK;
    }
    if (BU_STR_EQUAL(argv[0], "UP")) {
	s->s_flag = UP;
	return BRLCAD_OK;
    }

    bu_vls_printf(gedp->ged_result_str, "Invalid argument %s\n", argv[0]);
    return BRLCAD_ERROR;
}

int
_objs_cmd_delete(void *bs, int argc, const char **argv)
{
    struct _ged_view_info *gd = (struct _ged_view_info *)bs;
    struct ged *gedp = gd->gedp;
    const char *usage_string = "view obj name delete";
    const char *purpose_string = "delete view object";
    if (_view_cmd_msgs(bs, argc, argv, usage_string, purpose_string))
	return BRLCAD_OK;

    if (ged_view_obj_preflight(gedp, &ged_view_obj_del_schema, NULL, argc, argv) != BRLCAD_OK)
	return BRLCAD_ERROR;

    argc--; argv++;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    struct bv_scene_obj *s = gd->s;
    if (!s) {
	bu_vls_printf(gedp->ged_result_str, "No view object named %s\n", gd->vobj);
	return BRLCAD_ERROR;
    }
    if (!(s->s_type_flags & BV_VIEWONLY)) {
	bu_vls_printf(gedp->ged_result_str, "View object %s is associated with a database object - use 'erase' cmd to clear\n", gd->vobj);
	return BRLCAD_ERROR;
    }
    bv_obj_put(s);

    return BRLCAD_OK;
}

int
_objs_cmd_color(void *bs, int argc, const char **argv)
{
    struct _ged_view_info *gd = (struct _ged_view_info *)bs;
    struct ged *gedp = gd->gedp;
    const char *usage_string = "view obj name color [r/g/b]";
    const char *purpose_string = "show/set obj color";
    if (_view_cmd_msgs(bs, argc, argv, usage_string, purpose_string))
	return BRLCAD_OK;

    struct ged_view_obj_color_args args = {0};
    int color_start = bu_cmd_schema_parse_complete(&ged_view_obj_color_schema,
	&args, gedp->ged_result_str, argc - 1, argv + 1);
    if (color_start < 0)
	return BRLCAD_ERROR;

    argc--; argv++;
    int color_count = argc - color_start;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    struct bv_scene_obj *s = gd->s;
    if (!gd->s) {
	bu_vls_printf(gedp->ged_result_str, "No view object named %s\n", gd->vobj);
	return BRLCAD_ERROR;
    }

    if (color_count == 0) {
	bu_vls_printf(gedp->ged_result_str, "%d/%d/%d\n", s->s_color[0], s->s_color[1], s->s_color[2]);
	if (args.recurse) {
	    std::queue<struct bv_scene_obj *> sobjs;
	    sobjs.push(s);
	    while (!sobjs.empty()) {
		struct bv_scene_obj *sc = sobjs.front();
		sobjs.pop();
		bu_vls_printf(gedp->ged_result_str, "%s: %d/%d/%d\n", bu_vls_cstr(&sc->s_name), sc->s_color[0], sc->s_color[1], sc->s_color[2]);
		for (size_t i = 0; i < BU_PTBL_LEN(&sc->children); i++) {
		    struct bv_scene_obj *scn = (struct bv_scene_obj *)BU_PTBL_GET(&sc->children, i);
		    sobjs.push(scn);
		}
	    }
	}
	return BRLCAD_OK;
    }
    struct bu_color val = BU_COLOR_INIT_ZERO;
    if (bu_cmd_color_from_argv(&val, (size_t)color_count,
	(const char * const *)(argv + color_start)) != color_count) {
	bu_vls_printf(gedp->ged_result_str, "Invalid color argument\n");
	return BRLCAD_ERROR;
    }

    bu_color_to_rgb_chars(&val, s->s_color);
    if (args.recurse) {
	std::queue<struct bv_scene_obj *> sobjs;
	sobjs.push(s);
	while (!sobjs.empty()) {
	    struct bv_scene_obj *sc = sobjs.front();
	    sobjs.pop();
	    bu_color_to_rgb_chars(&val, sc->s_color);
	    for (size_t i = 0; i < BU_PTBL_LEN(&sc->children); i++) {
		struct bv_scene_obj *scn = (struct bv_scene_obj *)BU_PTBL_GET(&sc->children, i);
		sobjs.push(scn);
	    }
	}
    }
    return BRLCAD_OK;
}

int
_objs_cmd_arrow(void *bs, int argc, const char **argv)
{
    struct _ged_view_info *gd = (struct _ged_view_info *)bs;
    struct ged *gedp = gd->gedp;
    const char *usage_string = "view obj name arrow [0|1] [width [#]] [length [#]]";
    const char *purpose_string = "toggle arrow drawing, for those objects that support it";
    if (_view_cmd_msgs(bs, argc, argv, usage_string, purpose_string))
	return BRLCAD_OK;

    if (ged_view_obj_preflight(gedp, &ged_view_obj_arrow_schema, NULL, argc, argv) != BRLCAD_OK)
	return BRLCAD_ERROR;

    argc--; argv++;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    struct bv_scene_obj *s = gd->s;
    if (!gd->s) {
	bu_vls_printf(gedp->ged_result_str, "No view object named %s\n", gd->vobj);
	return BRLCAD_ERROR;
    }

    if (argc == 0) {
	bu_vls_printf(gedp->ged_result_str, "%d\n", s->s_arrow);
	return BRLCAD_OK;
    }

    if (BU_STR_EQUAL(argv[0], "0")) {
	s->s_arrow = 0;
	return BRLCAD_OK;
    }
    if (BU_STR_EQUAL(argv[0], "1")) {
	s->s_arrow = 1;
	return BRLCAD_OK;
    }
    if (BU_STR_EQUAL(argv[0], "width"))  {
	if (argc == 2) {
	    if (!bu_cmd_number_from_str(&s->s_os->s_arrow_tip_width, argv[1])) {
		bu_vls_printf(gedp->ged_result_str, "Invalid argument %s\n", argv[1]);
		return BRLCAD_ERROR;
	    }
	    return BRLCAD_OK;
	} else {
	    bu_vls_printf(gedp->ged_result_str, "%f\n", s->s_os->s_arrow_tip_width);
	    return BRLCAD_OK;
	}
    }

    if (BU_STR_EQUAL(argv[0], "length"))  {
	if (argc == 2) {
	    if (!bu_cmd_number_from_str(&s->s_os->s_arrow_tip_length, argv[1])) {
		bu_vls_printf(gedp->ged_result_str, "Invalid argument %s\n", argv[1]);
		return BRLCAD_ERROR;
	    }
	    return BRLCAD_OK;
	} else {
	    bu_vls_printf(gedp->ged_result_str, "%f\n", s->s_os->s_arrow_tip_length);
	    return BRLCAD_OK;
	}
    }

    bu_vls_printf(gedp->ged_result_str, "Invalid argument %s\n", argv[0]);
    return BRLCAD_ERROR;
}

int
_objs_cmd_lcnt(void *bs, int argc, const char **argv)
{
    struct _ged_view_info *gd = (struct _ged_view_info *)bs;
    struct ged *gedp = gd->gedp;
    const char *usage_string = "view obj name lcnt";
    const char *purpose_string = "print the number of vlist entities";
    if (_view_cmd_msgs(bs, argc, argv, usage_string, purpose_string))
	return BRLCAD_OK;

    if (ged_view_obj_preflight(gedp, &ged_view_obj_lcnt_schema, NULL, argc, argv) != BRLCAD_OK)
	return BRLCAD_ERROR;

    argc--; argv++;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    struct bv_scene_obj *s = gd->s;
    if (!gd->s) {
	bu_vls_printf(gedp->ged_result_str, "No view object named %s\n", gd->vobj);
	return BRLCAD_ERROR;
    }
    bu_vls_printf(gedp->ged_result_str, "%d\n", bu_list_len(&s->s_vlist));
    return BRLCAD_OK;
}

static void
update_recurse(struct bv_scene_obj *s, struct bview *v, int flags)
{
    for (size_t i = 0; i < BU_PTBL_LEN(&s->children); i++) {
	struct bv_scene_obj *sc = (struct bv_scene_obj *)BU_PTBL_GET(&s->children, i);
	update_recurse(sc, v, flags);
    }
    s->s_changed = 0;
    s->s_v = v;
    if (s->s_update_callback)
	(*s->s_update_callback)(s, v, 0);
}

int
_objs_cmd_update(void *bs, int argc, const char **argv)
{
    struct _ged_view_info *gd = (struct _ged_view_info *)bs;
    struct ged *gedp = gd->gedp;
    const char *usage_string = "view obj update name [x y]";
    const char *purpose_string = "update object";
    if (_view_cmd_msgs(bs, argc, argv, usage_string, purpose_string))
	return BRLCAD_OK;

    if (ged_view_obj_preflight(gedp, &ged_view_obj_update_schema, NULL, argc, argv) != BRLCAD_OK)
	return BRLCAD_ERROR;

    argc--; argv++;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    struct bv_scene_obj *s = gd->s;
    if (!gd->s) {
	bu_vls_printf(gedp->ged_result_str, "No view object named %s\n", gd->vobj);
	return BRLCAD_ERROR;
    }


    if (argc && (argc != 2)) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s\n", usage_string);
	return BRLCAD_ERROR;
    }

    struct bview *v = gd->cv;
    if (argc) {
	int x, y;
	if (!bu_cmd_integer_from_str(&x, argv[0]) || x < 0) {
	    bu_vls_printf(gedp->ged_result_str, "Invalid argument %s\n", argv[0]);
	    return BRLCAD_ERROR;
	}
	if (!bu_cmd_integer_from_str(&y, argv[1]) || y < 0) {
	    bu_vls_printf(gedp->ged_result_str, "Invalid argument %s\n", argv[1]);
	    return BRLCAD_ERROR;
	}
	v->gv_mouse_x = x;
	v->gv_mouse_y = y;
	bv_screen_pt(&v->gv_point, x, y, v);
    }

    update_recurse(s, v, 0);

    return BRLCAD_OK;
}

static void
ged_view_obj_usage(struct ged *gedp)
{
    char *option_help = bu_cmd_schema_describe(&ged_view_obj_schema);
    char *child_help = bu_cmd_tree_describe(&ged_view_obj_child_tree);

    bu_vls_printf(gedp->ged_result_str,
	"Usage: view obj [options] [object [subcommand [args]]]\n");
    if (option_help) {
	bu_vls_printf(gedp->ged_result_str, "Options:\n%s", option_help);
	bu_free(option_help, "view object native option help");
    }
    if (child_help) {
	bu_vls_strcat(gedp->ged_result_str, child_help);
	bu_free(child_help, "view object native child help");
    }
}

extern "C" int
_view_cmd_objs(void *bs, int argc, const char **argv)
{
    struct _ged_view_info *gd = (struct _ged_view_info *)bs;
    struct ged *gedp = gd->gedp;
    struct ged_view_obj_args args = {0, 0, 0, 0};
    size_t name_index;
    int root_argc;
    int ret = BRLCAD_ERROR;

    if (!gd->cv) {
	bu_vls_printf(gedp->ged_result_str, ": no view current in GED");
	return BRLCAD_ERROR;
    }

    /* The parent view tree has selected obj.  Its options end at the object
     * name; the native child tree owns every later operation word. */
    argc--; argv++;
    if (argc == 1 && BU_STR_EQUAL(argv[0], HELPFLAG)) {
	ged_view_obj_usage(gedp);
	return BRLCAD_OK;
    }
    name_index = ged_view_obj_name_index(&ged_view_obj_schema, (size_t)argc, argv);
    root_argc = (name_index < (size_t)argc) ? (int)name_index : argc;
    if (bu_cmd_schema_parse(&ged_view_obj_schema, &args, gedp->ged_result_str,
	root_argc, argv) != root_argc) {
	ged_view_obj_usage(gedp);
	return BRLCAD_ERROR;
    }
    gd->gopts = NULL;
    gd->local_obj = args.local_only;

    if (args.print_help) {
	ged_view_obj_usage(gedp);
	return BRLCAD_OK;
    }

    if (!args.geom_only && !args.view_only)
	args.view_only = 1;

    // If we're not wanting help and we have no subcommand, list defined view objects
    struct bview *v = gd->cv;
	if (name_index >= (size_t)argc) {
	if (args.geom_only) {
	    struct bu_ptbl *db_objs = bv_view_objs(v, BV_DB_OBJS);
	    if (db_objs) {
		for (size_t i = 0; i < BU_PTBL_LEN(db_objs); i++) {
		    struct bv_scene_group *cg = (struct bv_scene_group *)BU_PTBL_GET(db_objs, i);
		    if (bu_list_len(&cg->s_vlist)) {
			bu_vls_printf(gd->gedp->ged_result_str, "%s\n", bu_vls_cstr(&cg->s_name));
		    } else {
			for (size_t j = 0; j < BU_PTBL_LEN(&cg->children); j++) {
			    struct bv_scene_obj *s = (struct bv_scene_obj *)BU_PTBL_GET(&cg->children, j);
			    bu_vls_printf(gd->gedp->ged_result_str, "%s\n", bu_vls_cstr(&s->s_name));
			}
		    }
		}
	    }
	    struct bu_ptbl *local_db_objs = bv_view_objs(v, BV_DB_OBJS | BV_LOCAL_OBJS);
	    if (local_db_objs) {
		for (size_t i = 0; i < BU_PTBL_LEN(local_db_objs); i++) {
		    struct bv_scene_group *cg = (struct bv_scene_group *)BU_PTBL_GET(local_db_objs, i);
		    if (bu_list_len(&cg->s_vlist)) {
			bu_vls_printf(gd->gedp->ged_result_str, "%s\n", bu_vls_cstr(&cg->s_name));
		    } else {
			for (size_t j = 0; j < BU_PTBL_LEN(&cg->children); j++) {
			    struct bv_scene_obj *s = (struct bv_scene_obj *)BU_PTBL_GET(&cg->children, j);
			    bu_vls_printf(gd->gedp->ged_result_str, "%s\n", bu_vls_cstr(&s->s_name));
			}
		    }
		}
	    }
	}
	if (args.view_only) {
	    struct bu_ptbl *view_objs = bv_view_objs(v, BV_VIEW_OBJS);
	    if (view_objs) {
		for (size_t i = 0; i < BU_PTBL_LEN(view_objs); i++) {
		    struct bv_scene_obj *s = (struct bv_scene_obj *)BU_PTBL_GET(view_objs, i);
		    bu_vls_printf(gd->gedp->ged_result_str, "%s\n", bu_vls_cstr(&s->s_name));
		}
	    }

	    struct bu_ptbl *local_view_objs = bv_view_objs(v, BV_VIEW_OBJS | BV_LOCAL_OBJS);
	    if (local_view_objs) {
		for (size_t i = 0; i < BU_PTBL_LEN(local_view_objs); i++) {
		    struct bv_scene_obj *s = (struct bv_scene_obj *)BU_PTBL_GET(local_view_objs, i);
		    bu_vls_printf(gd->gedp->ged_result_str, "%s\n", bu_vls_cstr(&s->s_name));
		}
	    }
	}
	return BRLCAD_OK;
    }

    // We need a name, even if it doesn't exist yet.  Check if it does, since subcommands
    // will react differently based on that status.
    gd->vobj = argv[name_index];
    gd->s = NULL;

    // View-only objects take priority, unless we're explicitly excluding them by only specifying -G
	if (args.view_only) {
	if (!gd->local_obj) {
	    struct bu_ptbl *view_objs = bv_view_objs(v, BV_VIEW_OBJS);
	    for (size_t i = 0; i < BU_PTBL_LEN(view_objs); i++) {
		struct bv_scene_obj *s = (struct bv_scene_obj *)BU_PTBL_GET(view_objs, i);
		if (BU_STR_EQUAL(gd->vobj, bu_vls_cstr(&s->s_name))) {
		    gd->s = s;
		    break;
		}
	    }
	}
	if (!gd->s) {
	    struct bu_ptbl *view_objs = bv_view_objs(v, BV_VIEW_OBJS | BV_LOCAL_OBJS);
	    for (size_t i = 0; i < BU_PTBL_LEN(view_objs); i++) {
		struct bv_scene_obj *s = (struct bv_scene_obj *)BU_PTBL_GET(view_objs, i);
		if (BU_STR_EQUAL(gd->vobj, bu_vls_cstr(&s->s_name))) {
		    gd->s = s;
		    break;
		}
	    }
	}
    }

    if (!gd->s) {
	if (!gd->local_obj) {
	    struct bu_ptbl *db_objs = bv_view_objs(v, BV_DB_OBJS);
	    for (size_t i = 0; i < BU_PTBL_LEN(db_objs); i++) {
		struct bv_scene_group *cg = (struct bv_scene_group *)BU_PTBL_GET(db_objs, i);
		if (bu_list_len(&cg->s_vlist)) {
		    if (BU_STR_EQUAL(gd->vobj, bu_vls_cstr(&cg->s_name))) {
			gd->s = cg;
			break;
		    }
		} else {
		    for (size_t j = 0; j < BU_PTBL_LEN(&cg->children); j++) {
			struct bv_scene_obj *s = (struct bv_scene_obj *)BU_PTBL_GET(&cg->children, j);
			if (BU_STR_EQUAL(gd->vobj, bu_vls_cstr(&s->s_name))) {
			    gd->s = s;
			    break;
			}
		    }
		}
	    }
	}
	if (!gd->s) {
	    struct bu_ptbl *db_objs = bv_view_objs(v, BV_DB_OBJS | BV_LOCAL_OBJS);
	    for (size_t i = 0; i < BU_PTBL_LEN(db_objs); i++) {
		struct bv_scene_group *cg = (struct bv_scene_group *)BU_PTBL_GET(db_objs, i);
		if (bu_list_len(&cg->s_vlist)) {
		    if (BU_STR_EQUAL(gd->vobj, bu_vls_cstr(&cg->s_name))) {
			gd->s = cg;
			break;
		    }
		} else {
		    for (size_t j = 0; j < BU_PTBL_LEN(&cg->children); j++) {
			struct bv_scene_obj *s = (struct bv_scene_obj *)BU_PTBL_GET(&cg->children, j);
			if (BU_STR_EQUAL(gd->vobj, bu_vls_cstr(&s->s_name))) {
			    gd->s = s;
			    break;
			}
		    }
		}
	    }
	}
    }

    /* The object name is a parent-owned operand, not a child-tree word.  Once
     * it has been resolved, dispatch the remaining canonical operation tree
     * exactly as completion and validation do. */
    size_t child_start = name_index + 1;
    if (child_start >= (size_t)argc) {
	bu_vls_printf(gedp->ged_result_str, "view object subcommand required\n");
	ged_view_obj_usage(gedp);
	return BRLCAD_ERROR;
    }
    if (bu_cmd_tree_dispatch(&ged_view_obj_child_tree, gd,
	argc - (int)child_start, argv + child_start, &ret) == 0)
	return ret;

    bu_vls_printf(gedp->ged_result_str, "unknown view-object subcommand: %s\n",
	argv[child_start]);
    ged_view_obj_usage(gedp);
    return BRLCAD_ERROR;
}


/* Adding an old style "view obj" subcommand - will eventually need to align
 * its behavior and that of the above logic.  Probably need to fold the gobjs
 * logic in somehow as well (or move it to the edit command, since it's really
 * intended for object editing applications.) */
int
_view_cmd_old_obj(struct ged *gedp, int argc, const char *argv[])
{
    if (!gedp || argc < 4 || !argv)
	return BRLCAD_ERROR;

    // We know we're the obj command
    argc--; argv++;

    // Only have info implemented in the old mode
    if (!BU_STR_EQUAL(argv[1], "info") || !BU_STR_EQUAL(argv[2], "mode"))
	return BRLCAD_ERROR;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    // Need to see if we can match argv[0] up to a view object.  Doing this the
    // old way here - the new mode logic will have a different implementation.
    struct directory **dpp = _ged_build_dpp(gedp, argv[0]);
    if (!dpp) {
	bu_vls_printf(gedp->ged_result_str, "unknown");
	return BRLCAD_OK;
    }

    struct bu_list *hdlp = gedp->i->ged_gdp->gd_headDisplay;
    struct display_list *gdlp = BU_LIST_NEXT(display_list, hdlp);
    struct directory **tmp_dpp = NULL;
    size_t i;
    while (BU_LIST_NOT_HEAD(gdlp, hdlp)) {
	struct bv_scene_obj *sp;
	struct display_list *next_gdlp = BU_LIST_PNEXT(display_list, gdlp);
	for (BU_LIST_FOR(sp, bv_scene_obj, &gdlp->dl_head_scene_obj)) {
	    if (!sp->s_u_data)
		continue;
	    const struct ged_bv_data *bdata = (const struct ged_bv_data *)sp->s_u_data;
	    for (i = 0, tmp_dpp = dpp;
		    i < bdata->s_fullpath.fp_len && *tmp_dpp != RT_DIR_NULL;
		    ++i, ++tmp_dpp) {
		if (bdata->s_fullpath.fp_names[i] != *tmp_dpp)
		    break;
	    }

	    if (!tmp_dpp || *tmp_dpp != RT_DIR_NULL)
		continue;

	    switch (sp->s_os->s_dmode) {
		case _GED_WIREFRAME:
		    bu_vls_printf(gedp->ged_result_str, "wireframe");
		    break;
		case _GED_SHADED_MODE_BOTS:
		case _GED_SHADED_MODE_ALL:
		    bu_vls_printf(gedp->ged_result_str, "shaded");
		    break;
		case _GED_BOOL_EVAL:
		    bu_vls_printf(gedp->ged_result_str, "evaluated");
		    break;
		case _GED_HIDDEN_LINE:
		    bu_vls_printf(gedp->ged_result_str, "hidden_line");
		    break;
		case _GED_SHADED_MODE_EVAL:
		    bu_vls_printf(gedp->ged_result_str, "shaded_evaluated");
		    break;
		case _GED_WIREFRAME_EVAL:
		    bu_vls_printf(gedp->ged_result_str, "shaded_evaluated");
		    break;
		default:
		    bu_vls_printf(gedp->ged_result_str, "unknown");
		    break;
	    };

	    return BRLCAD_OK;
	}

	gdlp = next_gdlp;
    }

    // If we got this far, something wasn't right - either we couldn't find the
    // object, or the mode wasn't recognized.
    bu_vls_printf(gedp->ged_result_str, "unknown");
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
