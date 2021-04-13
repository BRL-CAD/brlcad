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
#include "bview.h"

#include "../ged_private.h"
#include "./ged_view.h"

int
_objs_cmd_draw(void *bs, int argc, const char **argv)
{
    struct _ged_view_info *gd = (struct _ged_view_info *)bs;
    struct ged *gedp = gd->gedp;
    const char *usage_string = "view objs draw name [0|1]";
    const char *purpose_string = "toggle view polygons";
    if (_view_cmd_msgs(bs, argc, argv, usage_string, purpose_string))
	return GED_OK;

    argc--; argv++;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    struct bview_scene_obj *s = gd->s;
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
	return GED_ERROR;
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
_objs_cmd_color(void *bs, int argc, const char **argv)
{
    struct _ged_view_info *gd = (struct _ged_view_info *)bs;
    struct ged *gedp = gd->gedp;
    const char *usage_string = "view objs color name [r/g/b]";
    const char *purpose_string = "show/set obj color";
    if (_view_cmd_msgs(bs, argc, argv, usage_string, purpose_string))
	return GED_OK;

    argc--; argv++;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    struct bview_scene_obj *s = gd->s;
    if (!gd->s) {
	bu_vls_printf(gedp->ged_result_str, "No view object named %s\n", gd->vobj);
	return GED_ERROR;
    }

    if (argc == 0) {
	bu_vls_printf(gedp->ged_result_str, "%d/%d/%d\n", s->s_color[0], s->s_color[1], s->s_color[2]);
	return GED_ERROR;
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
_objs_cmd_update(void *bs, int argc, const char **argv)
{
    struct _ged_view_info *gd = (struct _ged_view_info *)bs;
    struct ged *gedp = gd->gedp;
    const char *usage_string = "view objs update name [x y]";
    const char *purpose_string = "update object";
    if (_view_cmd_msgs(bs, argc, argv, usage_string, purpose_string))
	return GED_OK;

    argc--; argv++;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    struct bview_scene_obj *s = gd->s;
    if (!gd->s) {
	bu_vls_printf(gedp->ged_result_str, "No view object named %s\n", gd->vobj);
	return GED_ERROR;
    }


    if (argc && (argc != 2)) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s\n", usage_string);
	return GED_ERROR;
    }

    struct bview *v = gedp->ged_gvp;
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
    //{ "info",       _objs_cmd_info},
    { "update",     _objs_cmd_update},
    { "color",      _objs_cmd_color},
    { "polygon",    _view_cmd_polygons},
    { (char *)NULL,      NULL}
};

int
_view_cmd_objs(void *bs, int argc, const char **argv)
{
    int help = 0;
    struct _ged_view_info *gd = (struct _ged_view_info *)bs;
    struct ged *gedp = gd->gedp;

    const char *usage_string = "view [options] objs [options] [args]";
    const char *purpose_string = "manipulate view polygons";
    if (_view_cmd_msgs(bs, argc, argv, usage_string, purpose_string))
	return GED_OK;

    if (!gedp->ged_gvp) {
	bu_vls_printf(gedp->ged_result_str, ": no view current in GED");
	return GED_ERROR;
    }

    // See if we have any high level options set
    struct bu_opt_desc d[2];
    BU_OPT(d[0], "h", "help",  "",  NULL,  &help,      "Print help");
    BU_OPT_NULL(d[1]);

    gd->gopts = d;

    // We know we're the objs command - start processing args
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

    // If we're not wanting help and we have no subcommand, list defined view objects
    struct bview *v = gedp->ged_gvp;
    if (!ac && cmd_pos < 0 && !help) {
	for (size_t i = 0; i < BU_PTBL_LEN(v->gv_scene_objs); i++) {
	    struct bview_scene_obj *s = (struct bview_scene_obj *)BU_PTBL_GET(v->gv_scene_objs, i);
	    bu_vls_printf(gd->gedp->ged_result_str, "%s", bu_vls_cstr(&s->s_uuid));
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
    for (size_t i = 0; i < BU_PTBL_LEN(v->gv_scene_objs); i++) {
	struct bview_scene_obj *s = (struct bview_scene_obj *)BU_PTBL_GET(v->gv_scene_objs, i);
	if (BU_STR_EQUAL(gd->vobj, bu_vls_cstr(&s->s_uuid))) {
	    gd->s = s;
	    break;
	}
    }

    return _ged_subcmd_exec(gedp, (struct bu_opt_desc *)d, (const struct bu_cmdtab *)_obj_cmds,
	    "view objs", "[options] subcommand [args]", gd, argc, argv, help, cmd_pos);
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
