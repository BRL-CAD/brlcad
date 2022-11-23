/*                        A X E S . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2022 United States Government as represented by
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
 * Commands for scene data axes (the faceplate axes for showing view XYZ
 * coordinate systems is handled separately - these are axes defined as data in
 * the 3D scene.)
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

int
_axes_cmd_create(void *bs, int argc, const char **argv)
{
    struct _ged_view_info *gd = (struct _ged_view_info *)bs;
    struct ged *gedp = gd->gedp;
    const char *usage_string = "view obj <objname> axes create x y z";
    const char *purpose_string = "define data axes at point x,y,z";
    if (_view_cmd_msgs(bs, argc, argv, usage_string, purpose_string))
	return BRLCAD_OK;

    argc--; argv++;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    struct bv_scene_obj *s = gd->s;
    if (s) {
        bu_vls_printf(gedp->ged_result_str, "View object named %s already exists\n", gd->vobj);
        return BRLCAD_ERROR;
    }

    if (argc != 3) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s\n", usage_string);
	return BRLCAD_ERROR;
    }
    point_t p;
    for (int i = 0; i < 3; i++) {
	if (bu_opt_fastf_t(NULL, 1, (const char **)&argv[i], (void *)&(p[i])) != 1) {
	    bu_vls_printf(gedp->ged_result_str, "Invalid argument %s\n", argv[i]);
	    return BRLCAD_ERROR;
	}
    }

    s = bv_obj_get(gd->cv, BV_VIEW_OBJS);
    BU_LIST_INIT(&(s->s_vlist));
    BV_ADD_VLIST(&s->s_v->gv_objs.gv_vlfree, &s->s_vlist, p, BV_VLIST_LINE_MOVE);
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

    bu_vls_init(&s->s_uuid);
    bu_vls_printf(&s->s_uuid, "%s", gd->vobj);

    return BRLCAD_OK;
}

int
_axes_cmd_pos(void *bs, int argc, const char **argv)
{
    struct _ged_view_info *gd = (struct _ged_view_info *)bs;
    struct ged *gedp = gd->gedp;
    const char *usage_string = "view obj <objname> axes pos [x y z]";
    const char *purpose_string = "adjust axes position";
    if (_view_cmd_msgs(bs, argc, argv, usage_string, purpose_string))
	return BRLCAD_OK;

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
    if (argc != 3) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s\n", usage_string);
	return BRLCAD_ERROR;
    }
    point_t p;
    for (int i = 0; i < 3; i++) {
	if (bu_opt_fastf_t(NULL, 1, (const char **)&argv[i], (void *)&(p[i])) != 1) {
	    bu_vls_printf(gedp->ged_result_str, "Invalid argument %s\n", argv[i]);
	    return BRLCAD_ERROR;
	}
    }

    VMOVE(a->axes_pos, p);
    s->s_changed++;

    return BRLCAD_OK;
}

int
_axes_cmd_size(void *bs, int argc, const char **argv)
{
    struct _ged_view_info *gd = (struct _ged_view_info *)bs;
    struct ged *gedp = gd->gedp;
    const char *usage_string = "view obj <objname> axes size [#]";
    const char *purpose_string = "adjust axes size";
    if (_view_cmd_msgs(bs, argc, argv, usage_string, purpose_string))
	return BRLCAD_OK;

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

    if (argc != 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s\n", usage_string);
	return BRLCAD_ERROR;
    }
    fastf_t val;
    if (bu_opt_fastf_t(NULL, 1, (const char **)&argv[0], (void *)&val) != 1) {
	bu_vls_printf(gedp->ged_result_str, "Invalid argument %s\n", argv[0]);
	return BRLCAD_ERROR;
    }

    a->axes_size = val;
    s->s_changed++;

    return BRLCAD_OK;
}

int
_axes_cmd_linewidth(void *bs, int argc, const char **argv)
{
    struct _ged_view_info *gd = (struct _ged_view_info *)bs;
    struct ged *gedp = gd->gedp;
    const char *usage_string = "view obj <objname> axes linewidth [#]";
    const char *purpose_string = "adjust axes line width";
    if (_view_cmd_msgs(bs, argc, argv, usage_string, purpose_string))
	return BRLCAD_OK;

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

    if (argc != 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s\n", usage_string);
	return BRLCAD_ERROR;
    }
    int val;
    if (bu_opt_int(NULL, 1, (const char **)&argv[0], (void *)&val) != 1) {
	bu_vls_printf(gedp->ged_result_str, "Invalid argument %s\n", argv[0]);
	return BRLCAD_ERROR;
    }

    if (val < 1) {
	bu_vls_printf(gedp->ged_result_str, "Smallest supported value is 1\n");
	return BRLCAD_ERROR;
    }

    a->line_width = val;
    s->s_changed++;

    return BRLCAD_OK;
}

int
_axes_cmd_axes_color(void *bs, int argc, const char **argv)
{
    struct _ged_view_info *gd = (struct _ged_view_info *)bs;
    struct ged *gedp = gd->gedp;
    const char *usage_string = "view obj <objname> axes color [r/g/b]";
    const char *purpose_string = "get/set color of axes";
    if (_view_cmd_msgs(bs, argc, argv, usage_string, purpose_string))
	return BRLCAD_OK;

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

    // For color need either 1 or 3 non-subcommand args
    if (argc != 1 && argc != 3) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s\n", usage_string);
	return BRLCAD_ERROR;
    }

    struct bu_color c;
    int opt_ret = bu_opt_color(NULL, argc, (const char **)argv, (void *)&c);
    if (opt_ret != 1 && opt_ret != 3) {
	bu_vls_printf(gedp->ged_result_str, "Invalid color specifier\n");
	return BRLCAD_ERROR;
    }

    bu_color_to_rgb_ints(&c, &a->axes_color[0], &a->axes_color[1], &a->axes_color[2]);
    s->s_changed++;

    return BRLCAD_OK;
}


const struct bu_cmdtab _axes_cmds[] = {
    { "create",            _axes_cmd_create},
    { "pos",               _axes_cmd_pos},
    { "size",              _axes_cmd_size},
    { "line_width",        _axes_cmd_linewidth},
    { "axes_color",        _axes_cmd_axes_color},
    { (char *)NULL,      NULL}
};

int
_view_cmd_axes(void *bs, int argc, const char **argv)
{
    int help = 0;
    int s_version = 0;
    struct _ged_view_info *gd = (struct _ged_view_info *)bs;
    struct ged *gedp = gd->gedp;

    const char *usage_string = "view obj [options] axes [options] [args]";
    const char *purpose_string = "create/manipulate view axes";
    if (_view_cmd_msgs(bs, argc, argv, usage_string, purpose_string))
	return BRLCAD_OK;

    if (!gd->cv) {
	bu_vls_printf(gedp->ged_result_str, ": no view specified or current");
	return BRLCAD_ERROR;
    }


    // We know we're the axes command - start processing args
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
	if (bu_cmd_valid(_axes_cmds, argv[i]) == BRLCAD_OK) {
	    cmd_pos = i;
	    break;
	}
    }

    int acnt = (cmd_pos >= 0) ? cmd_pos : argc;
    (void)bu_opt_parse(NULL, acnt, argv, d);

    return _ged_subcmd_exec(gedp, d, _axes_cmds, "view obj axes", "[options] subcommand [args]", gd, argc, argv, help, cmd_pos);
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
