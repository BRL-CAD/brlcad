/*                      P O L Y G O N S . C
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
_poly_cmd_circle(void *bs, int argc, const char **argv)
{
    struct _ged_view_info *gd = (struct _ged_view_info *)bs;
    struct ged *gedp = gd->gedp;
    const char *usage_string = "view obj <objname> polygon circle [create|update] x y";
    const char *purpose_string = "create circular view polygon";
    if (_view_cmd_msgs(bs, argc, argv, usage_string, purpose_string))
	return GED_OK;

    argc--; argv++;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    struct bview_scene_obj *s = gd->s;

    if (argc != 3) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s\n", usage_string);
	return GED_ERROR;
    }
    int x,y;
    if (bu_opt_int(NULL, 1, (const char **)&argv[1], (void *)&x) != 1 || x < 0) {
	bu_vls_printf(gedp->ged_result_str, "Invalid argument %s\n", argv[0]);
	return GED_ERROR;
    }
    if (bu_opt_int(NULL, 1, (const char **)&argv[2], (void *)&y) != 1 || y < 0) {
	bu_vls_printf(gedp->ged_result_str, "Invalid argument %s\n", argv[1]);
	return GED_ERROR;
    }

    if (BU_STR_EQUAL(argv[0], "create")) {
	if (s) {
	    bu_vls_printf(gedp->ged_result_str, "View object named %s already exists\n", gd->vobj);
	    return GED_ERROR;
	}
	s = bview_create_polygon(gedp->ged_gvp, BVIEW_POLYGON_CIRCLE, x, y);
	if (!s) {
	    bu_vls_printf(gedp->ged_result_str, "Failed to create %s\n", gd->vobj);
	    return GED_ERROR;
	}
	bu_vls_init(&s->s_uuid);
	bu_vls_printf(&s->s_uuid, "%s", gd->vobj);
	bu_ptbl_ins(gedp->ged_gvp->gv_scene_objs, (long *)s);

	return GED_OK;
    }

    if (BU_STR_EQUAL(argv[0], "update")) {
	if (!s) {
	    bu_vls_printf(gedp->ged_result_str, "View object named %s does not exist\n", gd->vobj);
	    return GED_ERROR;
	}

	s->s_v->gv_mouse_x = x;
	s->s_v->gv_mouse_y = y;

	(*s->s_update_callback)(s);
	return GED_OK;
    }

    bu_vls_printf(gedp->ged_result_str, "unknown subcommand: %s\n", argv[0]);
    return GED_ERROR;
}

int
_poly_cmd_ellipse(void *bs, int argc, const char **argv)
{
    struct _ged_view_info *gd = (struct _ged_view_info *)bs;
    struct ged *gedp = gd->gedp;
    const char *usage_string = "view obj <objname> polygon ellipse [create|update] x y";
    const char *purpose_string = "create circular view polygon";
    if (_view_cmd_msgs(bs, argc, argv, usage_string, purpose_string))
	return GED_OK;

    argc--; argv++;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    struct bview_scene_obj *s = gd->s;

    if (argc != 3) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s\n", usage_string);
	return GED_ERROR;
    }
    int x,y;
    if (bu_opt_int(NULL, 1, (const char **)&argv[1], (void *)&x) != 1 || x < 0) {
	bu_vls_printf(gedp->ged_result_str, "Invalid argument %s\n", argv[0]);
	return GED_ERROR;
    }
    if (bu_opt_int(NULL, 1, (const char **)&argv[2], (void *)&y) != 1 || y < 0) {
	bu_vls_printf(gedp->ged_result_str, "Invalid argument %s\n", argv[1]);
	return GED_ERROR;
    }

    if (BU_STR_EQUAL(argv[0], "create")) {
	if (s) {
	    bu_vls_printf(gedp->ged_result_str, "View object named %s already exists\n", gd->vobj);
	    return GED_ERROR;
	}
	s = bview_create_polygon(gedp->ged_gvp, BVIEW_POLYGON_ELLIPSE, x, y);
	if (!s) {
	    bu_vls_printf(gedp->ged_result_str, "Failed to create %s\n", gd->vobj);
	    return GED_ERROR;
	}
	bu_vls_init(&s->s_uuid);
	bu_vls_printf(&s->s_uuid, "%s", gd->vobj);
	bu_ptbl_ins(gedp->ged_gvp->gv_scene_objs, (long *)s);

	return GED_OK;
    }

    if (BU_STR_EQUAL(argv[0], "update")) {
	if (!s) {
	    bu_vls_printf(gedp->ged_result_str, "View object named %s does not exist\n", gd->vobj);
	    return GED_ERROR;
	}

	s->s_v->gv_mouse_x = x;
	s->s_v->gv_mouse_y = y;

	(*s->s_update_callback)(s);
	return GED_OK;
    }

    bu_vls_printf(gedp->ged_result_str, "unknown subcommand: %s\n", argv[0]);
    return GED_ERROR;
}

int
_poly_cmd_rectangle(void *bs, int argc, const char **argv)
{
    struct _ged_view_info *gd = (struct _ged_view_info *)bs;
    struct ged *gedp = gd->gedp;
    const char *usage_string = "view obj <objname> polygon rectangle [create|update] x y";
    const char *purpose_string = "create circular view polygon";
    if (_view_cmd_msgs(bs, argc, argv, usage_string, purpose_string))
	return GED_OK;

    argc--; argv++;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    struct bview_scene_obj *s = gd->s;

    if (argc != 3) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s\n", usage_string);
	return GED_ERROR;
    }
    int x,y;
    if (bu_opt_int(NULL, 1, (const char **)&argv[1], (void *)&x) != 1 || x < 0) {
	bu_vls_printf(gedp->ged_result_str, "Invalid argument %s\n", argv[0]);
	return GED_ERROR;
    }
    if (bu_opt_int(NULL, 1, (const char **)&argv[2], (void *)&y) != 1 || y < 0) {
	bu_vls_printf(gedp->ged_result_str, "Invalid argument %s\n", argv[1]);
	return GED_ERROR;
    }

    if (BU_STR_EQUAL(argv[0], "create")) {
	if (s) {
	    bu_vls_printf(gedp->ged_result_str, "View object named %s already exists\n", gd->vobj);
	    return GED_ERROR;
	}
	s = bview_create_polygon(gedp->ged_gvp, BVIEW_POLYGON_RECTANGLE, x, y);
	if (!s) {
	    bu_vls_printf(gedp->ged_result_str, "Failed to create %s\n", gd->vobj);
	    return GED_ERROR;
	}
	bu_vls_init(&s->s_uuid);
	bu_vls_printf(&s->s_uuid, "%s", gd->vobj);
	bu_ptbl_ins(gedp->ged_gvp->gv_scene_objs, (long *)s);

	return GED_OK;
    }

    if (BU_STR_EQUAL(argv[0], "update")) {
	if (!s) {
	    bu_vls_printf(gedp->ged_result_str, "View object named %s does not exist\n", gd->vobj);
	    return GED_ERROR;
	}

	s->s_v->gv_mouse_x = x;
	s->s_v->gv_mouse_y = y;

	(*s->s_update_callback)(s);
	return GED_OK;
    }

    bu_vls_printf(gedp->ged_result_str, "unknown subcommand: %s\n", argv[0]);
    return GED_ERROR;
}

int
_poly_cmd_square(void *bs, int argc, const char **argv)
{
    struct _ged_view_info *gd = (struct _ged_view_info *)bs;
    struct ged *gedp = gd->gedp;
    const char *usage_string = "view obj <objname> polygon square [create|update] x y";
    const char *purpose_string = "create circular view polygon";
    if (_view_cmd_msgs(bs, argc, argv, usage_string, purpose_string))
	return GED_OK;

    argc--; argv++;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    struct bview_scene_obj *s = gd->s;

    if (argc != 3) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s\n", usage_string);
	return GED_ERROR;
    }
    int x,y;
    if (bu_opt_int(NULL, 1, (const char **)&argv[1], (void *)&x) != 1 || x < 0) {
	bu_vls_printf(gedp->ged_result_str, "Invalid argument %s\n", argv[0]);
	return GED_ERROR;
    }
    if (bu_opt_int(NULL, 1, (const char **)&argv[2], (void *)&y) != 1 || y < 0) {
	bu_vls_printf(gedp->ged_result_str, "Invalid argument %s\n", argv[1]);
	return GED_ERROR;
    }

    if (BU_STR_EQUAL(argv[0], "create")) {
	if (s) {
	    bu_vls_printf(gedp->ged_result_str, "View object named %s already exists\n", gd->vobj);
	    return GED_ERROR;
	}
	s = bview_create_polygon(gedp->ged_gvp, BVIEW_POLYGON_SQUARE, x, y);
	if (!s) {
	    bu_vls_printf(gedp->ged_result_str, "Failed to create %s\n", gd->vobj);
	    return GED_ERROR;
	}
	bu_vls_init(&s->s_uuid);
	bu_vls_printf(&s->s_uuid, "%s", gd->vobj);
	bu_ptbl_ins(gedp->ged_gvp->gv_scene_objs, (long *)s);

	return GED_OK;
    }

    if (BU_STR_EQUAL(argv[0], "update")) {
	if (!s) {
	    bu_vls_printf(gedp->ged_result_str, "View object named %s does not exist\n", gd->vobj);
	    return GED_ERROR;
	}

	s->s_v->gv_mouse_x = x;
	s->s_v->gv_mouse_y = y;

	(*s->s_update_callback)(s);
	return GED_OK;
    }

    bu_vls_printf(gedp->ged_result_str, "unknown subcommand: %s\n", argv[0]);
    return GED_ERROR;
}

int
_poly_cmd_create(void *bs, int argc, const char **argv)
{
    struct _ged_view_info *gd = (struct _ged_view_info *)bs;
    struct ged *gedp = gd->gedp;
    const char *usage_string = "view obj <objname> polygon create x y";
    const char *purpose_string = "create general view polygon";
    if (_view_cmd_msgs(bs, argc, argv, usage_string, purpose_string))
	return GED_OK;

    argc--; argv++;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    struct bview_scene_obj *s = gd->s;
    if (s) {
	bu_vls_printf(gedp->ged_result_str, "View object named %s already exists\n", gd->vobj);
	return GED_ERROR;
    }

    if (argc != 2) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s\n", usage_string);
	return GED_ERROR;
    }
    int x,y;
    if (bu_opt_int(NULL, 1, (const char **)&argv[0], (void *)&x) != 1 || x < 0) {
	bu_vls_printf(gedp->ged_result_str, "Invalid argument %s\n", argv[0]);
	return GED_ERROR;
    }
    if (bu_opt_int(NULL, 1, (const char **)&argv[1], (void *)&y) != 1 || y < 0) {
	bu_vls_printf(gedp->ged_result_str, "Invalid argument %s\n", argv[1]);
	return GED_ERROR;
    }

    s = bview_create_polygon(gedp->ged_gvp, BVIEW_POLYGON_GENERAL, x, y);
    if (!s) {
	bu_vls_printf(gedp->ged_result_str, "Failed to create %s\n", gd->vobj);
	return GED_ERROR;
    }
    bu_vls_init(&s->s_uuid);
    bu_vls_printf(&s->s_uuid, "%s", gd->vobj);
    bu_ptbl_ins(gedp->ged_gvp->gv_scene_objs, (long *)s);

    return GED_OK;
}


int
_poly_cmd_select(void *bs, int argc, const char **argv)
{
    struct _ged_view_info *gd = (struct _ged_view_info *)bs;
    struct ged *gedp = gd->gedp;
    const char *usage_string = "view obj <objname> polygon select x y";
    const char *purpose_string = "select polygon point closest to point x,y";
    if (_view_cmd_msgs(bs, argc, argv, usage_string, purpose_string))
	return GED_OK;

    argc--; argv++;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    struct bview_scene_obj *s = gd->s;

    if (argc != 2) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s\n", usage_string);
	return GED_ERROR;
    }
    int x,y;
    if (bu_opt_int(NULL, 1, (const char **)&argv[0], (void *)&x) != 1 || x < 0) {
	bu_vls_printf(gedp->ged_result_str, "Invalid argument %s\n", argv[0]);
	return GED_ERROR;
    }
    if (bu_opt_int(NULL, 1, (const char **)&argv[1], (void *)&y) != 1 || y < 0) {
	bu_vls_printf(gedp->ged_result_str, "Invalid argument %s\n", argv[1]);
	return GED_ERROR;
    }

    struct bview_polygon *p = (struct bview_polygon *)s->s_i_data;
    p->sflag = 1;
    p->mflag = 0;
    p->aflag = 0;

    s->s_v->gv_mouse_x = x;
    s->s_v->gv_mouse_y = y;
    bview_update_polygon(s);

    return GED_OK;
}


int
_poly_cmd_append(void *bs, int argc, const char **argv)
{
    struct _ged_view_info *gd = (struct _ged_view_info *)bs;
    struct ged *gedp = gd->gedp;
    const char *usage_string = "view obj <objname> polygon append x y";
    const char *purpose_string = "append point to polygon";
    if (_view_cmd_msgs(bs, argc, argv, usage_string, purpose_string))
	return GED_OK;

    argc--; argv++;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    struct bview_scene_obj *s = gd->s;

    if (argc != 2) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s\n", usage_string);
	return GED_ERROR;
    }
    int x,y;
    if (bu_opt_int(NULL, 1, (const char **)&argv[0], (void *)&x) != 1 || x < 0) {
	bu_vls_printf(gedp->ged_result_str, "Invalid argument %s\n", argv[0]);
	return GED_ERROR;
    }
    if (bu_opt_int(NULL, 1, (const char **)&argv[1], (void *)&y) != 1 || y < 0) {
	bu_vls_printf(gedp->ged_result_str, "Invalid argument %s\n", argv[1]);
	return GED_ERROR;
    }

    struct bview_polygon *p = (struct bview_polygon *)s->s_i_data;
    p->sflag = 0;
    p->mflag = 0;
    p->aflag = 1;

    s->s_v->gv_mouse_x = x;
    s->s_v->gv_mouse_y = y;
    bview_update_polygon(s);

    return GED_OK;
}

int
_poly_cmd_move(void *bs, int argc, const char **argv)
{
    struct _ged_view_info *gd = (struct _ged_view_info *)bs;
    struct ged *gedp = gd->gedp;
    const char *usage_string = "view obj <objname> polygon move x y";
    const char *purpose_string = "move selected polygon point to x,y";
    if (_view_cmd_msgs(bs, argc, argv, usage_string, purpose_string))
	return GED_OK;

    argc--; argv++;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    struct bview_scene_obj *s = gd->s;

    if (argc != 2) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s\n", usage_string);
	return GED_ERROR;
    }
    int x,y;
    if (bu_opt_int(NULL, 1, (const char **)&argv[0], (void *)&x) != 1 || x < 0) {
	bu_vls_printf(gedp->ged_result_str, "Invalid argument %s\n", argv[0]);
	return GED_ERROR;
    }
    if (bu_opt_int(NULL, 1, (const char **)&argv[1], (void *)&y) != 1 || y < 0) {
	bu_vls_printf(gedp->ged_result_str, "Invalid argument %s\n", argv[1]);
	return GED_ERROR;
    }

    struct bview_polygon *p = (struct bview_polygon *)s->s_i_data;
    p->sflag = 0;
    p->mflag = 1;
    p->aflag = 0;

    s->s_v->gv_mouse_x = x;
    s->s_v->gv_mouse_y = y;
    bview_update_polygon(s);

    return GED_OK;
}

int
_poly_cmd_clear(void *bs, int argc, const char **argv)
{
    struct _ged_view_info *gd = (struct _ged_view_info *)bs;
    struct ged *gedp = gd->gedp;
    const char *usage_string = "view obj <objname> polygon clear";
    const char *purpose_string = "clear all modification flags";
    if (_view_cmd_msgs(bs, argc, argv, usage_string, purpose_string))
	return GED_OK;

    argc--; argv++;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    struct bview_scene_obj *s = gd->s;
    struct bview_polygon *p = (struct bview_polygon *)s->s_i_data;
    p->sflag = 0;
    p->mflag = 0;
    p->aflag = 0;

    bview_update_polygon(s);

    return GED_OK;
}

const struct bu_cmdtab _poly_cmds[] = {
    { "circle",          _poly_cmd_circle},
    { "ellipse",         _poly_cmd_ellipse},
    { "rectangle",       _poly_cmd_rectangle},
    { "square",          _poly_cmd_square},
    { "create",          _poly_cmd_create},
    { "select",          _poly_cmd_select},
    { "move",            _poly_cmd_move},
    { "append",          _poly_cmd_append},
    { "clear",           _poly_cmd_clear},
    { (char *)NULL,      NULL}
};

int
_view_cmd_polygons(void *bs, int argc, const char **argv)
{
    int help = 0;
    int s_version = 0;
    struct _ged_view_info *gd = (struct _ged_view_info *)bs;
    struct ged *gedp = gd->gedp;

    const char *usage_string = "view [options] polygons [options] [args]";
    const char *purpose_string = "manipulate view polygons";
    if (_view_cmd_msgs(bs, argc, argv, usage_string, purpose_string))
	return GED_OK;

    if (!gedp->ged_gvp) {
	bu_vls_printf(gedp->ged_result_str, ": no view current in GED");
	return GED_ERROR;
    }


    // We know we're the polygons command - start processing args
    argc--; argv++;

    // See if we have any high level options set
    struct bu_opt_desc d[3];
    BU_OPT(d[0], "h", "help",  "",  NULL,  &help,      "Print help");
    BU_OPT(d[1], "s", "",      "",  NULL,  &s_version, "Work with S version of data");
    BU_OPT_NULL(d[2]);

    gd->gopts = d;

    // High level options are only defined prior to the subcommand
    int cmd_pos = -1;
    for (int i = 0; i < argc; i++) {
	if (bu_cmd_valid(_poly_cmds, argv[i]) == BRLCAD_OK) {
	    cmd_pos = i;
	    break;
	}
    }

    int acnt = (cmd_pos >= 0) ? cmd_pos : argc;
    (void)bu_opt_parse(NULL, acnt, argv, d);

    return _ged_subcmd_exec(gedp, d, _poly_cmds, "view obj polygons", "[options] subcommand [args]", gd, argc, argv, help, cmd_pos);
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
