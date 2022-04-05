/*                        L A B E L S . C
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
 * Commands for view labels.
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
_label_cmd_create(void *bs, int argc, const char **argv)
{
    struct _ged_view_info *gd = (struct _ged_view_info *)bs;
    struct ged *gedp = gd->gedp;
    const char *usage_string = "view obj <objname> label create text x y [z] [px py pz]";
    const char *purpose_string = "start a label at point x,y,[z], possibly targeting point px,py,pz";
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

    if (argc != 3 && argc != 4 && argc != 6 && argc != 7) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s\n", usage_string);
	return BRLCAD_ERROR;
    }
    point_t p;
    if (bu_opt_fastf_t(NULL, 1, (const char **)&argv[1], (void *)&(p[0])) != 1) {
	bu_vls_printf(gedp->ged_result_str, "Invalid argument %s\n", argv[1]);
	return BRLCAD_ERROR;
    }
    if (bu_opt_fastf_t(NULL, 1, (const char **)&argv[2], (void *)&(p[1])) != 1) {
	bu_vls_printf(gedp->ged_result_str, "Invalid argument %s\n", argv[2]);
	return BRLCAD_ERROR;
    }

    if (argc == 4 || argc == 7) {
	if (bu_opt_fastf_t(NULL, 1, (const char **)&argv[3], (void *)&(p[2])) != 1) {
	    bu_vls_printf(gedp->ged_result_str, "Invalid argument %s\n", argv[3]);
	    return BRLCAD_ERROR;
	}
    } else {
	fastf_t fx, fy;
	if (bv_screen_to_view(gedp->ged_gvp, &fx, &fy, (int)p[0], (int)p[1]) < 0) {
	    return BRLCAD_ERROR;
	}
	p[0] = fx;
	p[1] = fy;
	p[2] = 0;
	point_t tp;
	VMOVE(tp, p);
	MAT4X3PNT(p, gedp->ged_gvp->gv_view2model, tp);
    }
    point_t target;
    if (argc == 6) {
	if (bu_opt_fastf_t(NULL, 1, (const char **)&argv[3], (void *)&(target[0])) != 1) {
	    bu_vls_printf(gedp->ged_result_str, "Invalid argument %s\n", argv[3]);
	    return BRLCAD_ERROR;
	}
	if (bu_opt_fastf_t(NULL, 1, (const char **)&argv[4], (void *)&(target[1])) != 1) {
	    bu_vls_printf(gedp->ged_result_str, "Invalid argument %s\n", argv[4]);
	    return BRLCAD_ERROR;
	}
	if (bu_opt_fastf_t(NULL, 1, (const char **)&argv[5], (void *)&(target[2])) != 1) {
	    bu_vls_printf(gedp->ged_result_str, "Invalid argument %s\n", argv[5]);
	    return BRLCAD_ERROR;
	}
    }

    if (argc == 7) {
	if (bu_opt_fastf_t(NULL, 1, (const char **)&argv[4], (void *)&(target[0])) != 1) {
	    bu_vls_printf(gedp->ged_result_str, "Invalid argument %s\n", argv[4]);
	    return BRLCAD_ERROR;
	}
	if (bu_opt_fastf_t(NULL, 1, (const char **)&argv[5], (void *)&(target[1])) != 1) {
	    bu_vls_printf(gedp->ged_result_str, "Invalid argument %s\n", argv[5]);
	    return BRLCAD_ERROR;
	}
	if (bu_opt_fastf_t(NULL, 1, (const char **)&argv[6], (void *)&(target[2])) != 1) {
	    bu_vls_printf(gedp->ged_result_str, "Invalid argument %s\n", argv[6]);
	    return BRLCAD_ERROR;
	}
    }

    BU_GET(s, struct bv_scene_obj);
    s->s_v = gedp->ged_gvp;
    BU_LIST_INIT(&(s->s_vlist));
    BV_ADD_VLIST(&s->s_v->gv_objs.gv_vlfree, &s->s_vlist, p, BV_VLIST_LINE_MOVE);
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

    bu_vls_init(&s->s_uuid);
    bu_vls_printf(&s->s_uuid, "%s", gd->vobj);
    bu_ptbl_ins(gedp->ged_gvp->gv_objs.view_objs, (long *)s);

    return BRLCAD_OK;
}

const struct bu_cmdtab _label_cmds[] = {
    { "create",          _label_cmd_create},
    { (char *)NULL,      NULL}
};

int
_view_cmd_labels(void *bs, int argc, const char **argv)
{
    int help = 0;
    int s_version = 0;
    struct _ged_view_info *gd = (struct _ged_view_info *)bs;
    struct ged *gedp = gd->gedp;

    const char *usage_string = "view obj [options] label [options] [args]";
    const char *purpose_string = "create/manipulate view labels";
    if (_view_cmd_msgs(bs, argc, argv, usage_string, purpose_string))
	return BRLCAD_OK;

    if (!gedp->ged_gvp) {
	bu_vls_printf(gedp->ged_result_str, ": no view current in GED");
	return BRLCAD_ERROR;
    }


    // We know we're the labels command - start processing args
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
	if (bu_cmd_valid(_label_cmds, argv[i]) == BRLCAD_OK) {
	    cmd_pos = i;
	    break;
	}
    }

    int acnt = (cmd_pos >= 0) ? cmd_pos : argc;
    (void)bu_opt_parse(NULL, acnt, argv, d);

    return _ged_subcmd_exec(gedp, d, _label_cmds, "view obj label", "[options] subcommand [args]", gd, argc, argv, help, cmd_pos);
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
