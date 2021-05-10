/*                           O B J S . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2021 United States Government as represented by
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

#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "bu/cmd.h"
#include "bu/color.h"
#include "bu/opt.h"
#include "bu/vls.h"
#include "bv.h"

#include "../ged_private.h"
#include "./ged_view.h"

#define GET_BV_SCENE_OBJ(p, fp) { \
    if (BU_LIST_IS_EMPTY(fp)) { \
	BU_ALLOC((p), struct bv_scene_obj); \
    } else { \
	p = BU_LIST_NEXT(bv_scene_obj, fp); \
	BU_LIST_DEQUEUE(&((p)->l)); \
    } \
    BU_LIST_INIT( &((p)->s_vlist) ); }

int
_objs_cmd_draw(void *bs, int argc, const char **argv)
{
    struct _ged_view_info *gd = (struct _ged_view_info *)bs;
    struct ged *gedp = gd->gedp;
    const char *usage_string = "view obj name draw [0|1]";
    const char *purpose_string = "toggle view polygons";
    if (_view_cmd_msgs(bs, argc, argv, usage_string, purpose_string))
	return GED_OK;

    argc--; argv++;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    struct bv_scene_obj *s = gd->s;
    if (!gd->s) {
	bu_vls_printf(gedp->ged_result_str, "No view object named %s\n", gd->vobj);
	return GED_ERROR;
    }

    if (argc == 0) {
	if (s->s_flag == UP) {
	    bu_vls_printf(gedp->ged_result_str, "UP\n");
	} else {
	    bu_vls_printf(gedp->ged_result_str, "DOWN\n");
	}
	return GED_OK;
    }

    if (BU_STR_EQUAL(argv[0], "DOWN")) {
	s->s_flag = DOWN;
	return GED_OK;
    }
    if (BU_STR_EQUAL(argv[0], "UP")) {
	s->s_flag = UP;
	return GED_OK;
    }

    bu_vls_printf(gedp->ged_result_str, "Invalid argument %s\n", argv[0]);
    return GED_ERROR;
}

int
_objs_cmd_delete(void *bs, int argc, const char **argv)
{
    struct _ged_view_info *gd = (struct _ged_view_info *)bs;
    struct ged *gedp = gd->gedp;
    const char *usage_string = "view obj name delete";
    const char *purpose_string = "delete view object";
    if (_view_cmd_msgs(bs, argc, argv, usage_string, purpose_string))
	return GED_OK;

    argc--; argv++;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    struct bv_scene_obj *s = gd->s;
    if (!s) {
	bu_vls_printf(gedp->ged_result_str, "No view object named %s\n", gd->vobj);
	return GED_ERROR;
    }
    if (!(s->s_type_flags & BV_VIEWONLY)) {
	bu_vls_printf(gedp->ged_result_str, "View object %s is associated with a database object - use 'erase' cmd to clear\n", gd->vobj);
	return GED_ERROR;
    }
    bu_ptbl_rm(gedp->ged_gvp->gv_view_objs, (long *)s);
    bv_scene_obj_free(s, gedp->free_scene_obj);

    return GED_OK;
}

int
_objs_cmd_color(void *bs, int argc, const char **argv)
{
    struct _ged_view_info *gd = (struct _ged_view_info *)bs;
    struct ged *gedp = gd->gedp;
    const char *usage_string = "view obj name color [r/g/b]";
    const char *purpose_string = "show/set obj color";
    if (_view_cmd_msgs(bs, argc, argv, usage_string, purpose_string))
	return GED_OK;

    argc--; argv++;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    struct bv_scene_obj *s = gd->s;
    if (!gd->s) {
	bu_vls_printf(gedp->ged_result_str, "No view object named %s\n", gd->vobj);
	return GED_ERROR;
    }

    if (argc == 0) {
	bu_vls_printf(gedp->ged_result_str, "%d/%d/%d\n", s->s_color[0], s->s_color[1], s->s_color[2]);
	return GED_OK;
    }
    struct bu_color val;
    if (bu_opt_color(NULL, 1, (const char **)&argv[0], (void *)&val) != 1) {
	bu_vls_printf(gedp->ged_result_str, "Invalid argument %s\n", argv[0]);
	return GED_ERROR;
    }

    bu_color_to_rgb_chars(&val, s->s_color);

    return GED_OK;
}

int
_objs_cmd_arrow(void *bs, int argc, const char **argv)
{
    struct _ged_view_info *gd = (struct _ged_view_info *)bs;
    struct ged *gedp = gd->gedp;
    const char *usage_string = "view obj name arrow [0|1] [width [#]] [length [#]]";
    const char *purpose_string = "toggle arrow drawing, for those objects that support it";
    if (_view_cmd_msgs(bs, argc, argv, usage_string, purpose_string))
	return GED_OK;

    argc--; argv++;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    struct bv_scene_obj *s = gd->s;
    if (!gd->s) {
	bu_vls_printf(gedp->ged_result_str, "No view object named %s\n", gd->vobj);
	return GED_ERROR;
    }

    if (argc == 0) {
	bu_vls_printf(gedp->ged_result_str, "%d\n", s->s_arrow);
	return GED_OK;
    }

    if (BU_STR_EQUAL(argv[0], "0")) {
	s->s_arrow = 0;
	return GED_OK;
    }
    if (BU_STR_EQUAL(argv[0], "1")) {
	s->s_arrow = 1;
	return GED_OK;
    }
    if (BU_STR_EQUAL(argv[0], "width"))  {
	if (argc == 2) {
	    if (bu_opt_fastf_t(NULL, 1, (const char **)&argv[1], (void *)&s->s_os.s_arrow_tip_width) != 1) {
		bu_vls_printf(gedp->ged_result_str, "Invalid argument %s\n", argv[0]);
		return GED_ERROR;
	    }
	    return GED_OK;
	} else {
	    bu_vls_printf(gedp->ged_result_str, "%f\n", s->s_os.s_arrow_tip_width);
	    return GED_OK;
	}
    }

    if (BU_STR_EQUAL(argv[0], "length"))  {
	if (argc == 2) {
	    if (bu_opt_fastf_t(NULL, 1, (const char **)&argv[1], (void *)&s->s_os.s_arrow_tip_length) != 1) {
		bu_vls_printf(gedp->ged_result_str, "Invalid argument %s\n", argv[0]);
		return GED_ERROR;
	    }
	    return GED_OK;
	} else {
	    bu_vls_printf(gedp->ged_result_str, "%f\n", s->s_os.s_arrow_tip_length);
	    return GED_OK;
	}
    }

    bu_vls_printf(gedp->ged_result_str, "Invalid argument %s\n", argv[0]);
    return GED_ERROR;
}

int
_objs_cmd_lcnt(void *bs, int argc, const char **argv)
{
    struct _ged_view_info *gd = (struct _ged_view_info *)bs;
    struct ged *gedp = gd->gedp;
    const char *usage_string = "view obj name lcnt";
    const char *purpose_string = "print the number of vlist entities";
    if (_view_cmd_msgs(bs, argc, argv, usage_string, purpose_string))
	return GED_OK;

    argc--; argv++;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    struct bv_scene_obj *s = gd->s;
    if (!gd->s) {
	bu_vls_printf(gedp->ged_result_str, "No view object named %s\n", gd->vobj);
	return GED_ERROR;
    }
    bu_vls_printf(gedp->ged_result_str, "%d\n", bu_list_len(&s->s_vlist));
    return GED_OK;
}

int
_objs_cmd_update(void *bs, int argc, const char **argv)
{
    struct _ged_view_info *gd = (struct _ged_view_info *)bs;
    struct ged *gedp = gd->gedp;
    const char *usage_string = "view obj update name [x y]";
    const char *purpose_string = "update object";
    if (_view_cmd_msgs(bs, argc, argv, usage_string, purpose_string))
	return GED_OK;

    argc--; argv++;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    struct bv_scene_obj *s = gd->s;
    if (!gd->s) {
	bu_vls_printf(gedp->ged_result_str, "No view object named %s\n", gd->vobj);
	return GED_ERROR;
    }


    if (argc && (argc != 2)) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s\n", usage_string);
	return GED_ERROR;
    }

    struct bv *v = gedp->ged_gvp;
    if (argc) {
	int x, y;
	if (bu_opt_int(NULL, 1, (const char **)&argv[0], (void *)&x) != 1 || x < 0) {
	    bu_vls_printf(gedp->ged_result_str, "Invalid argument %s\n", argv[0]);
	    return GED_ERROR;
	}
	if (bu_opt_int(NULL, 1, (const char **)&argv[1], (void *)&y) != 1 || y < 0) {
	    bu_vls_printf(gedp->ged_result_str, "Invalid argument %s\n", argv[1]);
	    return GED_ERROR;
	}
	v->gv_mouse_x = x;
	v->gv_mouse_y = y;
    }

    s->s_changed = 0;
    s->s_v = v;
    (*s->s_update_callback)(s);

    return GED_OK;
}

const struct bu_cmdtab _obj_cmds[] = {
    { "draw",       _objs_cmd_draw},
    { "del",        _objs_cmd_delete},
    //{ "info",       _objs_cmd_info},
    { "update",     _objs_cmd_update},
    { "color",      _objs_cmd_color},
    { "axes",       _view_cmd_axes},
    { "arrow",      _objs_cmd_arrow},
    { "label",      _view_cmd_labels},
    { "lcnt",       _objs_cmd_lcnt},
    { "line",       _view_cmd_lines},
    { "polygon",    _view_cmd_polygons},
    { (char *)NULL,      NULL}
};

int
_view_cmd_objs(void *bs, int argc, const char **argv)
{
    int help = 0;
    int list_view = 0;
    int list_db = 0;
    struct _ged_view_info *gd = (struct _ged_view_info *)bs;
    struct ged *gedp = gd->gedp;

    const char *usage_string = "view [options] obj [options] [args]";
    const char *purpose_string = "manipulate view objects";
    if (_view_cmd_msgs(bs, argc, argv, usage_string, purpose_string))
	return GED_OK;

    if (!gedp->ged_gvp) {
	bu_vls_printf(gedp->ged_result_str, ": no view current in GED");
	return GED_ERROR;
    }

    // See if we have any high level options set
    struct bu_opt_desc d[4];
    BU_OPT(d[0], "h", "help",        "",  NULL,  &help,      "Print help");
    BU_OPT(d[1], "G", "gobjs",       "",  NULL,  &list_db,   "List geometry-object-based view scene objects");
    BU_OPT(d[2],  "", "view_only",   "",  NULL,  &list_view, "List view-only scene objects (default)");
    BU_OPT_NULL(d[3]);

    gd->gopts = d;

    // We know we're the obj command - start processing args
    argc--; argv++;

    // High level options are only defined prior to the subcommand
    int cmd_pos = -1;
    for (int i = 0; i < argc; i++) {
	if (bu_cmd_valid(_obj_cmds, argv[i]) == BRLCAD_OK) {
	    cmd_pos = i;
	    break;
	}
    }

    int acnt = (cmd_pos >= 0) ? cmd_pos : argc;
    int ac = bu_opt_parse(NULL, acnt, argv, d);

    if (!list_db && !list_view)
	list_view = 1;

    // If we're not wanting help and we have no subcommand, list defined view objects
    struct bv *v = gedp->ged_gvp;
    if (!ac && cmd_pos < 0 && !help) {
	if (list_db) {
	    for (size_t i = 0; i < BU_PTBL_LEN(v->gv_db_grps); i++) {
		struct bv_scene_group *cg = (struct bv_scene_group *)BU_PTBL_GET(v->gv_db_grps, i);
		if (bu_list_len(&cg->g->s_vlist)) {
		    bu_vls_printf(gd->gedp->ged_result_str, "%s\n", bu_vls_cstr(&cg->g->s_name));
		} else {
		    for (size_t j = 0; j < BU_PTBL_LEN(&cg->g->children); j++) {
			struct bv_scene_obj *s = (struct bv_scene_obj *)BU_PTBL_GET(&cg->g->children, j);
			bu_vls_printf(gd->gedp->ged_result_str, "%s\n", bu_vls_cstr(&s->s_name));
		    }
		}
	    }
	}
	if (list_view) {
	    for (size_t i = 0; i < BU_PTBL_LEN(v->gv_view_objs); i++) {
		struct bv_scene_obj *s = (struct bv_scene_obj *)BU_PTBL_GET(v->gv_view_objs, i);
		bu_vls_printf(gd->gedp->ged_result_str, "%s\n", bu_vls_cstr(&s->s_uuid));
	    }
	}
	return GED_OK;
    }

    // We need a name, even if it doesn't exist yet.  Check if it does, since subcommands
    // will react differently based on that status.
    if (ac != 1) {
	bu_vls_printf(gd->gedp->ged_result_str, "need view object name");
	return GED_ERROR;
    }
    gd->vobj = argv[0];
    gd->s = NULL;
    argc--; argv++;

    // View-only objects come first, unless we're explicitly excluding them by only specifying -G
    if (list_view) {
	for (size_t i = 0; i < BU_PTBL_LEN(v->gv_view_objs); i++) {
	    struct bv_scene_obj *s = (struct bv_scene_obj *)BU_PTBL_GET(v->gv_view_objs, i);
	    if (BU_STR_EQUAL(gd->vobj, bu_vls_cstr(&s->s_uuid))) {
		gd->s = s;
		break;
	    }
	}
    }

    if (!gd->s) {
	for (size_t i = 0; i < BU_PTBL_LEN(v->gv_db_grps); i++) {
	    struct bv_scene_group *cg = (struct bv_scene_group *)BU_PTBL_GET(v->gv_db_grps, i);
	    if (bu_list_len(&cg->g->s_vlist)) {
		if (BU_STR_EQUAL(gd->vobj, bu_vls_cstr(&cg->g->s_name))) {
		    gd->s = cg->g;
		    break;
		}
	    } else {
		for (size_t j = 0; j < BU_PTBL_LEN(&cg->g->children); j++) {
		    struct bv_scene_obj *s = (struct bv_scene_obj *)BU_PTBL_GET(&cg->g->children, j);
		    if (BU_STR_EQUAL(gd->vobj, bu_vls_cstr(&s->s_name))) {
			gd->s = s;
			break;
		    }
		}
	    }
	}
    }

    return _ged_subcmd_exec(gedp, (struct bu_opt_desc *)d, (const struct bu_cmdtab *)_obj_cmds,
	    "view obj", "[options] subcommand [args]", gd, argc, argv, help, cmd_pos);
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
