/*                        L I N E S . C
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
/** @file libged/view/lines.c
 *
 * Commands for view lines.
 *
 */

#include "common.h"

#include "bu/cmdschema.h"
#include "bu/vls.h"
#include "bv.h"

#include "../ged_private.h"
#include "./ged_view.h"

struct ged_view_line_args {
    int print_help;
    int s_version;
};

static int lines_tree_create(void *bs, int argc, const char **argv);
static int lines_tree_append(void *bs, int argc, const char **argv);

static int
line_xyz_validate(const struct bu_cmd_schema *UNUSED(schema), size_t argc,
    const char **argv, size_t cursor_arg, struct bu_cmd_validate_result *result)
{
    int ret = bu_cmd_vector3_required_validate(argc, argv, cursor_arg, result);
    if (!ret && result)
	result->semantic_provider = "ged.vector";
    return ret;
}

static const struct bu_cmd_option line_root_options[] = {
    BU_CMD_FLAG("h", "help", struct ged_view_line_args, print_help,
	"Print help and exit"),
    BU_CMD_ALIAS_SHORT("?", "help", 1),
    BU_CMD_FLAG("s", NULL, struct ged_view_line_args, s_version,
	"Work with the S version of line data"),
    BU_CMD_OPTION_NULL
};
static const struct bu_cmd_operand line_xyz_operands[] = {
    BU_CMD_OPERAND_SHAPED("point", BU_CMD_VALUE_VECTOR, 0, 3, NULL,
	"Packed XYZ point or three finite coordinates", "ged.vector",
	&bu_cmd_vector3_arg_shape),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_schema line_create_schema = {
    "create", "Start a polyline at a model-space point", NULL, line_xyz_operands,
    BU_CMD_PARSE_STOP_AT_FIRST_OPERAND, BU_CMD_SCHEMA_CONSTRAINTS(line_xyz_validate, NULL)
};
static const struct bu_cmd_schema line_append_schema = {
    "append", "Append a model-space point to a polyline", NULL, line_xyz_operands,
    BU_CMD_PARSE_STOP_AT_FIRST_OPERAND, BU_CMD_SCHEMA_CONSTRAINTS(line_xyz_validate, NULL)
};
GED_EXPORT const struct bu_cmd_schema ged_view_line_schema = {
    "line", "Manage object polylines", line_root_options, NULL,
    BU_CMD_PARSE_OPTIONS_FIRST, BU_CMD_SCHEMA_CONSTRAINTS(NULL, NULL)
};
GED_EXPORT const struct bu_cmd_tree_node ged_view_line_subcommands[] = {
    BU_CMD_TREE_NODE(&line_create_schema, NULL, NULL,
	BU_CMD_TREE_CHILD_AFTER_OPTIONS, lines_tree_create),
    BU_CMD_TREE_NODE(&line_append_schema, NULL, NULL,
	BU_CMD_TREE_CHILD_AFTER_OPTIONS, lines_tree_append),
    BU_CMD_TREE_NODE_NULL
};
GED_EXPORT const struct bu_cmd_tree ged_view_line_tree = {
    &ged_view_line_schema, ged_view_line_subcommands, BU_CMD_TREE_CHILD_AFTER_OPTIONS
};

static int
line_preflight(struct ged *gedp, const struct bu_cmd_schema *schema,
    int argc, const char **argv)
{
    return bu_cmd_schema_parse_complete(schema, NULL, gedp->ged_result_str,
	argc - 1, argv + 1) < 0 ? BRLCAD_ERROR : BRLCAD_OK;
}

static int
lines_tree_create(void *bs, int argc, const char **argv)
{
    struct _ged_view_info *gd = (struct _ged_view_info *)bs;
    struct ged *gedp = gd->gedp;
    const char *usage_string = "view obj <objname> line create x y z";
    const char *purpose_string = "start a polyline at point x,y,z";
    if (_view_cmd_msgs(bs, argc, argv, usage_string, purpose_string))
	return BRLCAD_OK;

    if (line_preflight(gedp, &line_create_schema, argc, argv) != BRLCAD_OK)
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
    if (bu_cmd_vector3_from_argv(p, (size_t)argc,
	(const char * const *)argv) != argc) {
	bu_vls_printf(gedp->ged_result_str, "Invalid XYZ point\n");
	return BRLCAD_ERROR;
    }

    int flags = BV_VIEW_OBJS;
    if (gd->local_obj)
	flags |= BV_LOCAL_OBJS;

    s = bv_obj_get(gd->cv, flags);
    BU_LIST_INIT(&(s->s_vlist));

    BV_ADD_VLIST(s->vlfree, &s->s_vlist, p, BV_VLIST_LINE_MOVE);

    bu_vls_init(&s->s_name);
    bu_vls_printf(&s->s_name, "%s", gd->vobj);

    return BRLCAD_OK;
}

static int
lines_tree_append(void *bs, int argc, const char **argv)
{
    struct _ged_view_info *gd = (struct _ged_view_info *)bs;
    struct ged *gedp = gd->gedp;
    const char *usage_string = "view obj <objname> line append x y z";
    const char *purpose_string = "append point to a polyline";
    if (_view_cmd_msgs(bs, argc, argv, usage_string, purpose_string))
	return BRLCAD_OK;

    if (line_preflight(gedp, &line_append_schema, argc, argv) != BRLCAD_OK)
	return BRLCAD_ERROR;

    argc--; argv++;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    struct bv_scene_obj *s = gd->s;
    if (!s) {
        bu_vls_printf(gedp->ged_result_str, "no view object named %s\n", gd->vobj);
        return BRLCAD_ERROR;
    }

    point_t p;
    if (bu_cmd_vector3_from_argv(p, (size_t)argc,
	(const char * const *)argv) != argc) {
	bu_vls_printf(gedp->ged_result_str, "Invalid XYZ point\n");
	return BRLCAD_ERROR;
    }

    BV_ADD_VLIST(s->vlfree, &s->s_vlist, p, BV_VLIST_LINE_DRAW);

    return BRLCAD_OK;
}

int
_view_cmd_lines(void *bs, int argc, const char **argv)
{
    struct _ged_view_info *gd = (struct _ged_view_info *)bs;
    struct ged *gedp = gd->gedp;
    struct ged_view_line_args args = {0, 0};
    int child_start;
    int ret = BRLCAD_ERROR;

    const char *usage_string = "view obj [options] line [options] [args]";
    const char *purpose_string = "manipulate view lines";
    if (_view_cmd_msgs(bs, argc, argv, usage_string, purpose_string))
	return BRLCAD_OK;

    if (!gd->cv) {
	bu_vls_printf(gedp->ged_result_str, ": no view specified or current in GED");
	return BRLCAD_ERROR;
    }


    argc--; argv++;
    child_start = bu_cmd_schema_parse(&ged_view_line_schema, &args,
	gedp->ged_result_str, argc, argv);
    if (child_start < 0) {
	char *help = bu_cmd_tree_describe(&ged_view_line_tree);
	if (help) {
	    bu_vls_strcat(gedp->ged_result_str, help);
	    bu_free(help, "view line native help");
	}
	return BRLCAD_ERROR;
    }
    gd->gopts = NULL;
    if (args.print_help) {
	char *help = bu_cmd_tree_describe(&ged_view_line_tree);
	if (help) {
	    bu_vls_strcat(gedp->ged_result_str, help);
	    bu_free(help, "view line native help");
	}
	return BRLCAD_OK;
    }
    if (child_start >= argc) {
	bu_vls_strcat(gedp->ged_result_str, "line subcommand required\n");
	return BRLCAD_ERROR;
    }
    if (bu_cmd_tree_dispatch(&ged_view_line_tree, gd, argc - child_start,
	argv + child_start, &ret) == 0)
	return ret;
    bu_vls_printf(gedp->ged_result_str, "unknown line subcommand: %s\n",
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
