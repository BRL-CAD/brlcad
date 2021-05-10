/*                        L I N E S . C
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
/** @file libged/view/lines.c
 *
 * Commands for view lines.
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
_line_cmd_create(void *bs, int argc, const char **argv)
{
    struct _ged_view_info *gd = (struct _ged_view_info *)bs;
    struct ged *gedp = gd->gedp;
    const char *usage_string = "view obj <objname> line create x y z";
    const char *purpose_string = "start a polyline at point x,y,z";
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

    if (argc != 3) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s\n", usage_string);
	return GED_ERROR;
    }
    point_t p;
    if (bu_opt_fastf_t(NULL, 1, (const char **)&argv[0], (void *)&(p[0])) != 1) {
	bu_vls_printf(gedp->ged_result_str, "Invalid argument %s\n", argv[0]);
	return GED_ERROR;
    }
    if (bu_opt_fastf_t(NULL, 1, (const char **)&argv[1], (void *)&(p[1])) != 1) {
	bu_vls_printf(gedp->ged_result_str, "Invalid argument %s\n", argv[1]);
	return GED_ERROR;
    }
    if (bu_opt_fastf_t(NULL, 1, (const char **)&argv[2], (void *)&(p[2])) != 1) {
	bu_vls_printf(gedp->ged_result_str, "Invalid argument %s\n", argv[2]);
	return GED_ERROR;
    }

    BU_GET(s, struct bview_scene_obj);
    s->s_v = gedp->ged_gvp;
    BU_LIST_INIT(&(s->s_vlist));

    BV_ADD_VLIST(&s->s_v->gv_vlfree, &s->s_vlist, p, BV_VLIST_LINE_MOVE);

    bu_vls_init(&s->s_uuid);
    bu_vls_printf(&s->s_uuid, "%s", gd->vobj);
    bu_ptbl_ins(gedp->ged_gvp->gv_view_objs, (long *)s);

    return GED_OK;
}

int
_line_cmd_append(void *bs, int argc, const char **argv)
{
    struct _ged_view_info *gd = (struct _ged_view_info *)bs;
    struct ged *gedp = gd->gedp;
    const char *usage_string = "view obj <objname> line append x y z";
    const char *purpose_string = "append point to a polyline";
    if (_view_cmd_msgs(bs, argc, argv, usage_string, purpose_string))
	return GED_OK;

    argc--; argv++;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    struct bview_scene_obj *s = gd->s;
    if (!s) {
        bu_vls_printf(gedp->ged_result_str, "no view object named %s\n", gd->vobj);
        return GED_ERROR;
    }

    if (argc != 3) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s\n", usage_string);
	return GED_ERROR;
    }

    point_t p;
    if (bu_opt_fastf_t(NULL, 1, (const char **)&argv[0], (void *)&(p[0])) != 1) {
	bu_vls_printf(gedp->ged_result_str, "Invalid argument %s\n", argv[0]);
	return GED_ERROR;
    }
    if (bu_opt_fastf_t(NULL, 1, (const char **)&argv[1], (void *)&(p[1])) != 1) {
	bu_vls_printf(gedp->ged_result_str, "Invalid argument %s\n", argv[1]);
	return GED_ERROR;
    }
    if (bu_opt_fastf_t(NULL, 1, (const char **)&argv[2], (void *)&(p[2])) != 1) {
	bu_vls_printf(gedp->ged_result_str, "Invalid argument %s\n", argv[2]);
	return GED_ERROR;
    }

    BV_ADD_VLIST(&s->s_v->gv_vlfree, &s->s_vlist, p, BV_VLIST_LINE_DRAW);

    return GED_OK;
}

const struct bu_cmdtab _line_cmds[] = {
    { "create",          _line_cmd_create},
    { "append",          _line_cmd_append},
    { (char *)NULL,      NULL}
};

int
_view_cmd_lines(void *bs, int argc, const char **argv)
{
    int help = 0;
    int s_version = 0;
    struct _ged_view_info *gd = (struct _ged_view_info *)bs;
    struct ged *gedp = gd->gedp;

    const char *usage_string = "view obj [options] line [options] [args]";
    const char *purpose_string = "manipulate view lines";
    if (_view_cmd_msgs(bs, argc, argv, usage_string, purpose_string))
	return GED_OK;

    if (!gedp->ged_gvp) {
	bu_vls_printf(gedp->ged_result_str, ": no view current in GED");
	return GED_ERROR;
    }


    // We know we're the lines command - start processing args
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
	if (bu_cmd_valid(_line_cmds, argv[i]) == BRLCAD_OK) {
	    cmd_pos = i;
	    break;
	}
    }

    int acnt = (cmd_pos >= 0) ? cmd_pos : argc;
    (void)bu_opt_parse(NULL, acnt, argv, d);

    return _ged_subcmd_exec(gedp, d, _line_cmds, "view obj line", "[options] subcommand [args]", gd, argc, argv, help, cmd_pos);
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
