/*                        A X E S . C
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
/** @file libged/view/axes.c
 *
 * Commands for scene data axes (the faceplate axes for showing view XYZ
 * coordinate systems is handled separately - these are axes defined as data in
 * the 3D scene.)
 *
 */

#include "common.h"

#include "bu/color.h"
#include "bu/cmdschema.h"
#include "bu/vls.h"
#include "bv.h"

#include "../ged_private.h"
#include "./ged_view.h"

struct ged_view_axes_args {
    int print_help;
    int s_version;
};

static int axes_tree_create(void *bs, int argc, const char **argv);
static int axes_tree_pos(void *bs, int argc, const char **argv);
static int axes_tree_size(void *bs, int argc, const char **argv);
static int axes_tree_linewidth(void *bs, int argc, const char **argv);
static int axes_tree_color(void *bs, int argc, const char **argv);

static int
axes_optional_xyz_validate(const struct bu_cmd_schema *schema, size_t argc,
    const char **argv, size_t cursor_arg, struct bu_cmd_validate_result *result)
{
    struct bu_cmd_schema flat = *schema;
    int ret;

    flat.validation.custom_validate = NULL;
    ret = bu_cmd_schema_validate(&flat, argc, argv, cursor_arg, result);
    if (ret || result->state == BU_CMD_VALIDATE_INVALID || !argc || argc == 3)
	return ret;
    bu_cmd_validate_result_clear(result);
    result->state = argc < 3 ? BU_CMD_VALIDATE_INCOMPLETE : BU_CMD_VALIDATE_INVALID;
    result->token_start = cursor_arg < argc ? cursor_arg : argc;
    result->token_end = result->token_start;
    result->expected = BU_CMD_EXPECT_OPERAND;
    result->completion_type = BU_CMD_VALUE_NUMBER;
    result->hint = "zero or three finite XYZ components required";
    return 0;
}

static int
axes_color_validate(const struct bu_cmd_schema *UNUSED(schema), size_t argc,
    const char **argv, size_t cursor_arg, struct bu_cmd_validate_result *result)
{
    int ret = bu_cmd_color_optional_validate(argc, argv, cursor_arg, result);
    if (!ret && result)
	result->semantic_provider = "ged.color";
    return ret;
}

static const struct bu_cmd_option axes_root_options[] = {
    BU_CMD_FLAG("h", "help", struct ged_view_axes_args, print_help,
	"Print help and exit"),
    BU_CMD_ALIAS_SHORT("?", "help", 1),
    BU_CMD_FLAG("s", NULL, struct ged_view_axes_args, s_version,
	"Work with the S version of axes data"),
    BU_CMD_OPTION_NULL
};
static const struct bu_cmd_operand axes_xyz_required_operands[] = {
    BU_CMD_OPERAND("x", BU_CMD_VALUE_NUMBER, 1, 1, "X coordinate", NULL),
    BU_CMD_OPERAND("y", BU_CMD_VALUE_NUMBER, 1, 1, "Y coordinate", NULL),
    BU_CMD_OPERAND("z", BU_CMD_VALUE_NUMBER, 1, 1, "Z coordinate", NULL),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_operand axes_xyz_optional_operands[] = {
    BU_CMD_OPERAND("x", BU_CMD_VALUE_NUMBER, 0, 1, "X coordinate", NULL),
    BU_CMD_OPERAND("y", BU_CMD_VALUE_NUMBER, 0, 1, "Y coordinate", NULL),
    BU_CMD_OPERAND("z", BU_CMD_VALUE_NUMBER, 0, 1, "Z coordinate", NULL),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_operand axes_size_operands[] = {
    BU_CMD_OPERAND("size", BU_CMD_VALUE_NUMBER, 0, 1, "Axes size", NULL),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_operand axes_linewidth_operands[] = {
    BU_CMD_OPERAND_VALIDATE("width", BU_CMD_VALUE_INTEGER, 0, 1,
	bu_cmd_positive_integer_validate, "Positive line width", NULL),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_operand axes_color_operands[] = {
    BU_CMD_OPERAND_SHAPED("color", BU_CMD_VALUE_RAW, 0, 3, NULL,
	"Packed color or three RGB components", "ged.color", &bu_cmd_color_arg_shape),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_schema axes_create_schema = {
    "create", "Define data axes at a model-space point", NULL,
    axes_xyz_required_operands, BU_CMD_PARSE_STOP_AT_FIRST_OPERAND,
    BU_CMD_SCHEMA_CONSTRAINTS(NULL, NULL)
};
static const struct bu_cmd_schema axes_pos_schema = {
    "pos", "Query or set axes position", NULL,
    axes_xyz_optional_operands, BU_CMD_PARSE_STOP_AT_FIRST_OPERAND,
    BU_CMD_SCHEMA_CONSTRAINTS(axes_optional_xyz_validate, NULL)
};
static const struct bu_cmd_schema axes_size_schema = {
    "size", "Query or set axes size", NULL, axes_size_operands,
    BU_CMD_PARSE_STOP_AT_FIRST_OPERAND, BU_CMD_SCHEMA_CONSTRAINTS(NULL, NULL)
};
static const struct bu_cmd_schema axes_linewidth_schema = {
    "line_width", "Query or set axes line width", NULL, axes_linewidth_operands,
    BU_CMD_PARSE_STOP_AT_FIRST_OPERAND, BU_CMD_SCHEMA_CONSTRAINTS(NULL, NULL)
};
static const struct bu_cmd_schema axes_color_schema = {
    "axes_color", "Query or set axes color", NULL, axes_color_operands,
    BU_CMD_PARSE_STOP_AT_FIRST_OPERAND,
    BU_CMD_SCHEMA_CONSTRAINTS(axes_color_validate, NULL)
};
GED_EXPORT const struct bu_cmd_schema ged_view_axes_schema = {
    "axes", "Create or manipulate data axes", axes_root_options, NULL,
    BU_CMD_PARSE_OPTIONS_FIRST, BU_CMD_SCHEMA_CONSTRAINTS(NULL, NULL)
};
GED_EXPORT const struct bu_cmd_tree_node ged_view_axes_subcommands[] = {
    BU_CMD_TREE_NODE(&axes_create_schema, NULL, NULL,
	BU_CMD_TREE_CHILD_AFTER_OPTIONS, axes_tree_create),
    BU_CMD_TREE_NODE(&axes_pos_schema, NULL, NULL,
	BU_CMD_TREE_CHILD_AFTER_OPTIONS, axes_tree_pos),
    BU_CMD_TREE_NODE(&axes_size_schema, NULL, NULL,
	BU_CMD_TREE_CHILD_AFTER_OPTIONS, axes_tree_size),
    BU_CMD_TREE_NODE(&axes_linewidth_schema, NULL, NULL,
	BU_CMD_TREE_CHILD_AFTER_OPTIONS, axes_tree_linewidth),
    BU_CMD_TREE_NODE(&axes_color_schema, NULL, NULL,
	BU_CMD_TREE_CHILD_AFTER_OPTIONS, axes_tree_color),
    BU_CMD_TREE_NODE_NULL
};
GED_EXPORT const struct bu_cmd_tree ged_view_axes_tree = {
    &ged_view_axes_schema, ged_view_axes_subcommands, BU_CMD_TREE_CHILD_AFTER_OPTIONS
};

static int
axes_schema_preflight(struct ged *gedp, const struct bu_cmd_schema *schema,
    int argc, const char **argv)
{
    if (bu_cmd_schema_parse_complete(schema, NULL, gedp->ged_result_str,
	argc - 1, argv + 1) < 0)
	return BRLCAD_ERROR;
    return BRLCAD_OK;
}

static int
axes_tree_create(void *bs, int argc, const char **argv)
{
    struct _ged_view_info *gd = (struct _ged_view_info *)bs;
    struct ged *gedp = gd->gedp;
    const char *usage_string = "view obj <objname> axes create x y z";
    const char *purpose_string = "define data axes at point x,y,z";
    if (_view_cmd_msgs(bs, argc, argv, usage_string, purpose_string))
	return BRLCAD_OK;

    if (axes_schema_preflight(gedp, &axes_create_schema, argc, argv) != BRLCAD_OK)
	return BRLCAD_ERROR;

    argc--; argv++;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    struct bv_scene_obj *s = gd->s;
    if (s) {
        bu_vls_printf(gedp->ged_result_str, "View object named %s already exists\n", gd->vobj);
        return BRLCAD_ERROR;
    }

    point_t p;
    for (int i = 0; i < 3; i++) {
	if (!bu_cmd_number_from_str(&p[i], argv[i]))
	    return BRLCAD_ERROR;
    }

    int flags = BV_VIEW_OBJS;
    if (gd->local_obj)
	flags |= BV_LOCAL_OBJS;
    s = bv_obj_get(gd->cv, flags);

    BU_LIST_INIT(&(s->s_vlist));
    BV_ADD_VLIST(s->vlfree, &s->s_vlist, p, BV_VLIST_LINE_MOVE);
    VSET(s->s_color, 255, 255, 0);

    struct bv_axes *l;
    BU_GET(l, struct bv_axes);
    VMOVE(l->axes_pos, p);
    l->line_width = 1;
    l->axes_size = 10;
    VSET(l->axes_color, 255, 255, 0);
    s->s_i_data = (void *)l;

    s->s_type_flags |= BV_VIEWONLY;
    s->s_type_flags |= BV_AXES;

    bu_vls_init(&s->s_name);
    bu_vls_printf(&s->s_name, "%s", gd->vobj);

    return BRLCAD_OK;
}

static int
axes_tree_pos(void *bs, int argc, const char **argv)
{
    struct _ged_view_info *gd = (struct _ged_view_info *)bs;
    struct ged *gedp = gd->gedp;
    const char *usage_string = "view obj <objname> axes pos [x y z]";
    const char *purpose_string = "adjust axes position";
    if (_view_cmd_msgs(bs, argc, argv, usage_string, purpose_string))
	return BRLCAD_OK;

    if (axes_schema_preflight(gedp, &axes_pos_schema, argc, argv) != BRLCAD_OK)
	return BRLCAD_ERROR;

    argc--; argv++;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    struct bv_scene_obj *s = gd->s;
    if (!s) {
        bu_vls_printf(gedp->ged_result_str, "View object named %s does not exist\n", gd->vobj);
        return BRLCAD_ERROR;
    }
    if (!(s->s_type_flags & BV_AXES)) {
        bu_vls_printf(gedp->ged_result_str, "View object %s is not an axes object\n", gd->vobj);
        return BRLCAD_ERROR;
    }
    struct bv_axes *a = (struct bv_axes *)s->s_i_data;
    if (argc == 0) {
	bu_vls_printf(gedp->ged_result_str, "%f %f %f\n", V3ARGS(a->axes_pos));
	return BRLCAD_OK;
    }
    point_t p;
    for (int i = 0; i < 3; i++) {
	if (!bu_cmd_number_from_str(&p[i], argv[i]))
	    return BRLCAD_ERROR;
    }

    VMOVE(a->axes_pos, p);
    s->s_changed++;

    return BRLCAD_OK;
}

static int
axes_tree_size(void *bs, int argc, const char **argv)
{
    struct _ged_view_info *gd = (struct _ged_view_info *)bs;
    struct ged *gedp = gd->gedp;
    const char *usage_string = "view obj <objname> axes size [#]";
    const char *purpose_string = "adjust axes size";
    if (_view_cmd_msgs(bs, argc, argv, usage_string, purpose_string))
	return BRLCAD_OK;

    if (axes_schema_preflight(gedp, &axes_size_schema, argc, argv) != BRLCAD_OK)
	return BRLCAD_ERROR;

    argc--; argv++;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    struct bv_scene_obj *s = gd->s;
    if (!s) {
        bu_vls_printf(gedp->ged_result_str, "View object named %s does not exist\n", gd->vobj);
        return BRLCAD_ERROR;
    }
    if (!(s->s_type_flags & BV_AXES)) {
        bu_vls_printf(gedp->ged_result_str, "View object %s is not an axes object\n", gd->vobj);
        return BRLCAD_ERROR;
    }
    struct bv_axes *a = (struct bv_axes *)s->s_i_data;
     if (argc == 0) {
	bu_vls_printf(gedp->ged_result_str, "%f\n", a->axes_size);
	return BRLCAD_OK;
    }

    fastf_t val;
    if (!bu_cmd_number_from_str(&val, argv[0]))
	return BRLCAD_ERROR;

    a->axes_size = val;
    s->s_changed++;

    return BRLCAD_OK;
}

static int
axes_tree_linewidth(void *bs, int argc, const char **argv)
{
    struct _ged_view_info *gd = (struct _ged_view_info *)bs;
    struct ged *gedp = gd->gedp;
    const char *usage_string = "view obj <objname> axes linewidth [#]";
    const char *purpose_string = "adjust axes line width";
    if (_view_cmd_msgs(bs, argc, argv, usage_string, purpose_string))
	return BRLCAD_OK;

    if (axes_schema_preflight(gedp, &axes_linewidth_schema, argc, argv) != BRLCAD_OK)
	return BRLCAD_ERROR;

    argc--; argv++;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    struct bv_scene_obj *s = gd->s;
    if (!s) {
        bu_vls_printf(gedp->ged_result_str, "View object named %s does not exist\n", gd->vobj);
        return BRLCAD_ERROR;
    }
    if (!(s->s_type_flags & BV_AXES)) {
        bu_vls_printf(gedp->ged_result_str, "View object %s is not an axes object\n", gd->vobj);
        return BRLCAD_ERROR;
    }
    struct bv_axes *a = (struct bv_axes *)s->s_i_data;
     if (argc == 0) {
	bu_vls_printf(gedp->ged_result_str, "%d\n", a->line_width);
	return BRLCAD_OK;
    }

    int val;
    if (!bu_cmd_integer_from_str(&val, argv[0]))
	return BRLCAD_ERROR;

    a->line_width = val;
    s->s_changed++;

    return BRLCAD_OK;
}

static int
axes_tree_color(void *bs, int argc, const char **argv)
{
    struct _ged_view_info *gd = (struct _ged_view_info *)bs;
    struct ged *gedp = gd->gedp;
    const char *usage_string = "view obj <objname> axes color [r/g/b]";
    const char *purpose_string = "get/set color of axes";
    if (_view_cmd_msgs(bs, argc, argv, usage_string, purpose_string))
	return BRLCAD_OK;

    if (axes_schema_preflight(gedp, &axes_color_schema, argc, argv) != BRLCAD_OK)
	return BRLCAD_ERROR;

    argc--; argv++;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    struct bv_scene_obj *s = gd->s;
    if (!s) {
        bu_vls_printf(gedp->ged_result_str, "View object named %s does not exist\n", gd->vobj);
        return BRLCAD_ERROR;
    }
    if (!(s->s_type_flags & BV_AXES)) {
        bu_vls_printf(gedp->ged_result_str, "View object %s is not an axes object\n", gd->vobj);
        return BRLCAD_ERROR;
    }
    struct bv_axes *a = (struct bv_axes *)s->s_i_data;
     if (argc == 0) {
	bu_vls_printf(gedp->ged_result_str, "%d %d %d\n", a->axes_color[0], a->axes_color[1], a->axes_color[2]);
	return BRLCAD_OK;
    }

    struct bu_color c;
    if (bu_cmd_color_from_argv(&c, (size_t)argc, (const char * const *)argv) != argc)
	return BRLCAD_ERROR;

    bu_color_to_rgb_ints(&c, &a->axes_color[0], &a->axes_color[1], &a->axes_color[2]);
    s->s_changed++;

    return BRLCAD_OK;
}


static void
axes_usage(struct ged *gedp)
{
    char *help = bu_cmd_tree_describe(&ged_view_axes_tree);

    bu_vls_printf(gedp->ged_result_str,
	"Usage: view obj <object> axes [options] subcommand [args]\n");
    if (help) {
	bu_vls_strcat(gedp->ged_result_str, help);
	bu_free(help, "view axes native tree help");
    }
}


int
_view_cmd_axes(void *bs, int argc, const char **argv)
{
    struct _ged_view_info *gd = (struct _ged_view_info *)bs;
    struct ged *gedp = gd->gedp;
    struct ged_view_axes_args args = {0, 0};
    int subcommand_pos = -1;
    int ret = BRLCAD_ERROR;

    if (argc == 2 && BU_STR_EQUAL(argv[1], HELPFLAG)) {
	axes_usage(gedp);
	return BRLCAD_OK;
    }
    if (!gd->cv) {
	bu_vls_printf(gedp->ged_result_str, ": no view specified or current");
	return BRLCAD_ERROR;
    }

    /* The parent object handler has selected axes; root options belong only
     * before the axes child command, just as the native view root's options
     * belong before its child. */
    argc--; argv++;
    for (int i = 0; i < argc; i++) {
	int span = bu_cmd_schema_option_span(&ged_view_axes_schema,
	    (size_t)(argc - i), argv + i);
	if (span > 0) {
	    i += span - 1;
	    continue;
	}
	/* Leave malformed options in the root phase so the native parser reports
	 * the option error rather than labelling it an unknown subcommand. */
	if (span < 0 || (argv[i][0] == '-' && argv[i][1]))
	    break;
	subcommand_pos = i;
	break;
    }

    int root_argc = subcommand_pos >= 0 ? subcommand_pos : argc;
    if (bu_cmd_schema_parse_complete(&ged_view_axes_schema, &args,
	gedp->ged_result_str, root_argc, argv) < 0) {
	axes_usage(gedp);
	return BRLCAD_ERROR;
    }
    (void)args.s_version; /* Accepted historic spelling; behavior is unchanged. */

    if (args.print_help) {
	if (subcommand_pos >= 0) {
	    const char *sub_help[] = {argv[subcommand_pos], HELPFLAG};
	    (void)bu_cmd_tree_dispatch(&ged_view_axes_tree, gd, 2, sub_help, &ret);
	} else {
	    axes_usage(gedp);
	}
	return BRLCAD_OK;
    }
    if (subcommand_pos < 0) {
	bu_vls_printf(gedp->ged_result_str, "subcommand required\n");
	axes_usage(gedp);
	return BRLCAD_ERROR;
    }
    if (bu_cmd_tree_dispatch(&ged_view_axes_tree, gd, argc - root_argc,
	argv + root_argc, &ret) == 0)
	return ret;

    bu_vls_printf(gedp->ged_result_str, "unknown axes subcommand: %s\n",
	argv[subcommand_pos]);
	axes_usage(gedp);
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
