/*                        L A B E L S . C
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
/** @file libged/view/labels.c
 *
 * Commands for view labels.
 *
 */

#include "common.h"

#include "bu/cmdschema.h"
#include "bu/vls.h"
#include "bv.h"

#include "../ged_private.h"
#include "./ged_view.h"

struct ged_view_label_args {
    int print_help;
    int s_version;
};

static int labels_tree_create(void *bs, int argc, const char **argv);

static int
label_create_validate(const struct bu_cmd_schema *UNUSED(schema), size_t argc,
    const char **argv, size_t cursor_arg, struct bu_cmd_validate_result *result)
{
    bu_cmd_validate_state_t state = BU_CMD_VALIDATE_VALID;
    const char *hint = "label arguments";
    bu_cmd_value_t type = cursor_arg == 0 ? BU_CMD_VALUE_STRING : BU_CMD_VALUE_NUMBER;

    if (!result || cursor_arg > argc)
	return -1;
    if (argc > 7) {
	state = BU_CMD_VALIDATE_INVALID;
	hint = "label accepts text, X/Y[/Z], and an optional target XYZ";
    } else {
	for (size_t i = 1; i < argc; i++) {
	    fastf_t value = 0.0;
	    if (!bu_cmd_number_from_str(&value, argv[i])) {
		state = BU_CMD_VALIDATE_INVALID;
		hint = "invalid label coordinate";
		break;
	    }
	}
	if (state == BU_CMD_VALIDATE_VALID && argc < 3) {
	    state = BU_CMD_VALIDATE_INCOMPLETE;
	    hint = argc ? "label X and Y coordinates required" : "label text required";
	} else if (state == BU_CMD_VALIDATE_VALID && argc == 5) {
	    state = BU_CMD_VALIDATE_INCOMPLETE;
	    hint = "label target requires three coordinates";
	}
    }
    bu_cmd_validate_result_clear(result);
    result->state = state;
    result->token_start = cursor_arg;
    result->token_end = cursor_arg;
    result->expected = BU_CMD_EXPECT_OPERAND;
    result->completion_type = type;
    result->hint = hint;
    result->semantic_provider = NULL;
    return 0;
}

static const struct bu_cmd_option label_root_options[] = {
    BU_CMD_FLAG("h", "help", struct ged_view_label_args, print_help,
	"Print help and exit"),
    BU_CMD_ALIAS_SHORT("?", "help", 1),
    BU_CMD_FLAG("s", NULL, struct ged_view_label_args, s_version,
	"Work with the S version of label data"),
    BU_CMD_OPTION_NULL
};
static const struct bu_cmd_operand label_create_operands[] = {
    BU_CMD_OPERAND("arguments", BU_CMD_VALUE_RAW, 0, 7,
	"Text, position, and optional target", NULL),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_schema label_create_schema = {
    "create", "Create a label at a position, optionally with a target", NULL,
    label_create_operands, BU_CMD_PARSE_STOP_AT_FIRST_OPERAND,
    BU_CMD_SCHEMA_CONSTRAINTS(label_create_validate, NULL)
};
GED_EXPORT const struct bu_cmd_schema ged_view_label_schema = {
    "label", "Manage object labels", label_root_options, NULL,
    BU_CMD_PARSE_OPTIONS_FIRST, BU_CMD_SCHEMA_CONSTRAINTS(NULL, NULL)
};
GED_EXPORT const struct bu_cmd_tree_node ged_view_label_subcommands[] = {
    BU_CMD_TREE_NODE(&label_create_schema, NULL, NULL,
	BU_CMD_TREE_CHILD_AFTER_OPTIONS, labels_tree_create),
    BU_CMD_TREE_NODE_NULL
};
GED_EXPORT const struct bu_cmd_tree ged_view_label_tree = {
    &ged_view_label_schema, ged_view_label_subcommands, BU_CMD_TREE_CHILD_AFTER_OPTIONS
};

static int
label_preflight(struct ged *gedp, int argc, const char **argv)
{
    return bu_cmd_schema_parse_complete(&label_create_schema, NULL,
	gedp->ged_result_str, argc - 1, argv + 1) < 0 ? BRLCAD_ERROR : BRLCAD_OK;
}

static int
labels_tree_create(void *bs, int argc, const char **argv)
{
    struct _ged_view_info *gd = (struct _ged_view_info *)bs;
    struct ged *gedp = gd->gedp;
    const char *usage_string = "view obj <objname> label create text x y [z] [px py pz]";
    const char *purpose_string = "start a label at point x,y,[z], possibly targeting point px,py,pz";
    if (_view_cmd_msgs(bs, argc, argv, usage_string, purpose_string))
	return BRLCAD_OK;

    if (label_preflight(gedp, argc, argv) != BRLCAD_OK)
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
    if (!bu_cmd_number_from_str(&p[0], argv[1])) {
	bu_vls_printf(gedp->ged_result_str, "Invalid argument %s\n", argv[1]);
	return BRLCAD_ERROR;
    }
    if (!bu_cmd_number_from_str(&p[1], argv[2])) {
	bu_vls_printf(gedp->ged_result_str, "Invalid argument %s\n", argv[2]);
	return BRLCAD_ERROR;
    }

    if (argc == 4 || argc == 7) {
	if (!bu_cmd_number_from_str(&p[2], argv[3])) {
	    bu_vls_printf(gedp->ged_result_str, "Invalid argument %s\n", argv[3]);
	    return BRLCAD_ERROR;
	}
    } else {
	fastf_t fx, fy;
	if (bv_screen_to_view(gd->cv, &fx, &fy, (int)p[0], (int)p[1]) < 0) {
	    return BRLCAD_ERROR;
	}
	p[0] = fx;
	p[1] = fy;
	p[2] = 0;
	point_t tp;
	VMOVE(tp, p);
	MAT4X3PNT(p, gd->cv->gv_view2model, tp);
    }
    point_t target;
    if (argc == 6) {
	if (!bu_cmd_number_from_str(&target[0], argv[3])) {
	    bu_vls_printf(gedp->ged_result_str, "Invalid argument %s\n", argv[3]);
	    return BRLCAD_ERROR;
	}
	if (!bu_cmd_number_from_str(&target[1], argv[4])) {
	    bu_vls_printf(gedp->ged_result_str, "Invalid argument %s\n", argv[4]);
	    return BRLCAD_ERROR;
	}
	if (!bu_cmd_number_from_str(&target[2], argv[5])) {
	    bu_vls_printf(gedp->ged_result_str, "Invalid argument %s\n", argv[5]);
	    return BRLCAD_ERROR;
	}
    }

    if (argc == 7) {
	if (!bu_cmd_number_from_str(&target[0], argv[4])) {
	    bu_vls_printf(gedp->ged_result_str, "Invalid argument %s\n", argv[4]);
	    return BRLCAD_ERROR;
	}
	if (!bu_cmd_number_from_str(&target[1], argv[5])) {
	    bu_vls_printf(gedp->ged_result_str, "Invalid argument %s\n", argv[5]);
	    return BRLCAD_ERROR;
	}
	if (!bu_cmd_number_from_str(&target[2], argv[6])) {
	    bu_vls_printf(gedp->ged_result_str, "Invalid argument %s\n", argv[6]);
	    return BRLCAD_ERROR;
	}
    }

    int flags = BV_VIEW_OBJS;
    if (gd->local_obj)
	flags |= BV_LOCAL_OBJS;
    s = bv_obj_get(gd->cv, flags);
    s->s_v = gd->cv;
    BU_LIST_INIT(&(s->s_vlist));
    BV_ADD_VLIST(s->vlfree, &s->s_vlist, p, BV_VLIST_LINE_MOVE);
    VSET(s->s_color, 255, 255, 0);

    struct bv_label *l;
    BU_GET(l, struct bv_label);
    BU_VLS_INIT(&l->label);
    bu_vls_sprintf(&l->label, "%s", argv[0]);
    VMOVE(l->p, p);
    if (argc == 6 || argc == 7) {
	VMOVE(l->target, target);
	l->line_flag = 1;
    }
    s->s_i_data = (void *)l;

    s->s_type_flags |= BV_VIEWONLY;
    s->s_type_flags |= BV_LABELS;

    bu_vls_init(&s->s_name);
    bu_vls_printf(&s->s_name, "%s", gd->vobj);

    return BRLCAD_OK;
}

int
_view_cmd_labels(void *bs, int argc, const char **argv)
{
    struct _ged_view_info *gd = (struct _ged_view_info *)bs;
    struct ged *gedp = gd->gedp;
    struct ged_view_label_args args = {0, 0};
    int child_start;
    int ret = BRLCAD_ERROR;

    const char *usage_string = "view obj [options] label [options] [args]";
    const char *purpose_string = "create/manipulate view labels";
    if (_view_cmd_msgs(bs, argc, argv, usage_string, purpose_string))
	return BRLCAD_OK;

    if (!gd->cv) {
	bu_vls_printf(gedp->ged_result_str, ": no view specified or current in GED");
	return BRLCAD_ERROR;
    }


    argc--; argv++;
    child_start = bu_cmd_schema_parse(&ged_view_label_schema, &args,
	gedp->ged_result_str, argc, argv);
    if (child_start < 0) {
	char *help = bu_cmd_tree_describe(&ged_view_label_tree);
	if (help) {
	    bu_vls_strcat(gedp->ged_result_str, help);
	    bu_free(help, "view label native help");
	}
	return BRLCAD_ERROR;
    }
    gd->gopts = NULL;
    if (args.print_help) {
	char *help = bu_cmd_tree_describe(&ged_view_label_tree);
	if (help) {
	    bu_vls_strcat(gedp->ged_result_str, help);
	    bu_free(help, "view label native help");
	}
	return BRLCAD_OK;
    }
    if (child_start >= argc) {
	bu_vls_strcat(gedp->ged_result_str, "label subcommand required\n");
	return BRLCAD_ERROR;
    }
    if (bu_cmd_tree_dispatch(&ged_view_label_tree, gd, argc - child_start,
	argv + child_start, &ret) == 0)
	return ret;
    bu_vls_printf(gedp->ged_result_str, "unknown label subcommand: %s\n",
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
