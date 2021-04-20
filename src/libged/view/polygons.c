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
_poly_cmd_create(void *bs, int argc, const char **argv)
{
    struct _ged_view_info *gd = (struct _ged_view_info *)bs;
    struct ged *gedp = gd->gedp;
    const char *usage_string = "view obj <objname> polygon create x y [circ|ell|rect|sq]";
    const char *purpose_string = "create polygon";
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

    if (argc != 2 && argc != 3) {
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

    int type = BVIEW_POLYGON_GENERAL;
    if (argc == 3) {
	if (BU_STR_EQUAL(argv[2], "circ") || BU_STR_EQUAL(argv[2], "circle"))
	    type = BVIEW_POLYGON_CIRCLE;
	if (BU_STR_EQUAL(argv[2], "ell") || BU_STR_EQUAL(argv[2], "ellipse"))
	    type = BVIEW_POLYGON_ELLIPSE;
	if (BU_STR_EQUAL(argv[2], "rect") || BU_STR_EQUAL(argv[2], "rectangle"))
	    type = BVIEW_POLYGON_RECTANGLE;
	if (BU_STR_EQUAL(argv[2], "sq") || BU_STR_EQUAL(argv[2], "square"))
	    type = BVIEW_POLYGON_SQUARE;
	if (type == BVIEW_POLYGON_GENERAL) {
	    bu_vls_printf(gedp->ged_result_str, "Unknown polygon type %s\n", argv[2]);
	    return GED_ERROR;
	}
    }

    s = bview_create_polygon(gedp->ged_gvp, type, x, y);
    if (!s) {
	bu_vls_printf(gedp->ged_result_str, "Failed to create %s\n", gd->vobj);
	return GED_ERROR;
    }
    bu_vls_init(&s->s_uuid);
    bu_vls_printf(&s->s_uuid, "%s", gd->vobj);
    bu_ptbl_ins(gedp->ged_gvp->gv_scene_objs, (long *)s);

    // Stash view info
    struct bview_polygon *p = (struct bview_polygon *)s->s_i_data;
    bview_sync(&p->v, gedp->ged_gvp);

    return GED_OK;
}

int
_poly_cmd_update(void *bs, int argc, const char **argv)
{
    struct _ged_view_info *gd = (struct _ged_view_info *)bs;
    struct ged *gedp = gd->gedp;
    const char *usage_string = "view obj <objname> polygon update x y";
    const char *purpose_string = "update shape constrained polygon types";
    if (_view_cmd_msgs(bs, argc, argv, usage_string, purpose_string))
	return GED_OK;

    argc--; argv++;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    struct bview_scene_obj *s = gd->s;
    if (!s) {
	bu_vls_printf(gedp->ged_result_str, "View object %s does not exist\n", gd->vobj);
	return GED_ERROR;
    }
    if (!(s->s_type_flags & BVIEW_VIEWONLY) || !(s->s_type_flags & BVIEW_POLYGONS)) {
	bu_vls_printf(gedp->ged_result_str, "Specified object is not a view polygon.\n");
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

    struct bview_polygon *p = (struct bview_polygon *)s->s_i_data;
    if (p->type == BVIEW_POLYGON_GENERAL) {
	return GED_OK;
    }

    s->s_v->gv_mouse_x = x;
    s->s_v->gv_mouse_y = y;

    (*s->s_update_callback)(s);

    return GED_OK;
}


int
_poly_cmd_select(void *bs, int argc, const char **argv)
{
    struct _ged_view_info *gd = (struct _ged_view_info *)bs;
    struct ged *gedp = gd->gedp;
    const char *usage_string = "view obj <objname> polygon select [contour] x y";
    const char *purpose_string = "select polygon point closest to point x,y";
    if (_view_cmd_msgs(bs, argc, argv, usage_string, purpose_string))
	return GED_OK;

    argc--; argv++;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    struct bview_scene_obj *s = gd->s;
    if (!s) {
	bu_vls_printf(gedp->ged_result_str, "View object %s does not exist\n", gd->vobj);
	return GED_ERROR;
    }
    if (!(s->s_type_flags & BVIEW_VIEWONLY) || !(s->s_type_flags & BVIEW_POLYGONS)) {
	bu_vls_printf(gedp->ged_result_str, "Specified object is not a view polygon.\n");
	return GED_ERROR;
    }

    struct bview_polygon *p = (struct bview_polygon *)s->s_i_data;

    if (p->type != BVIEW_POLYGON_GENERAL) {
	bu_vls_printf(gedp->ged_result_str, "Point selection is only supported for general polygons - specified object defines a constrained shape\n");
	return GED_ERROR;
    }

    if (argc != 2 && argc != 3) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s\n", usage_string);
	return GED_ERROR;
    }
    int ioffset = 0;
    int contour_ind = -1;
    if (argc == 3) {
	ioffset++;
	if (bu_opt_int(NULL, 1, (const char **)&argv[0], (void *)&contour_ind) != 1 || contour_ind < 0) {
	    bu_vls_printf(gedp->ged_result_str, "Invalid argument %s\n", argv[0]);
	    return GED_ERROR;
	}
	p->curr_contour_i = contour_ind;
    } else {
	p->curr_contour_i = 0;
    }
    int x,y;
    if (bu_opt_int(NULL, 1, (const char **)&argv[ioffset], (void *)&x) != 1 || x < 0) {
	bu_vls_printf(gedp->ged_result_str, "Invalid argument %s\n", argv[ioffset]);
	return GED_ERROR;
    }
    if (bu_opt_int(NULL, 1, (const char **)&argv[ioffset+1], (void *)&y) != 1 || y < 0) {
	bu_vls_printf(gedp->ged_result_str, "Invalid argument %s\n", argv[ioffset+1]);
	return GED_ERROR;
    }

    p->curr_contour_i = contour_ind;
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
    const char *usage_string = "view obj <objname> polygon append [contour] x y";
    const char *purpose_string = "append point to polygon";
    if (_view_cmd_msgs(bs, argc, argv, usage_string, purpose_string))
	return GED_OK;

    argc--; argv++;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    struct bview_scene_obj *s = gd->s;
    if (!s) {
	bu_vls_printf(gedp->ged_result_str, "View object %s does not exist\n", gd->vobj);
	return GED_ERROR;
    }
    if (!(s->s_type_flags & BVIEW_VIEWONLY) || !(s->s_type_flags & BVIEW_POLYGONS)) {
	bu_vls_printf(gedp->ged_result_str, "Specified object is not a view polygon.\n");
	return GED_ERROR;
    }

    struct bview_polygon *p = (struct bview_polygon *)s->s_i_data;
    if (p->type != BVIEW_POLYGON_GENERAL) {
	bu_vls_printf(gedp->ged_result_str, "Point appending is only supported for general polygons - specified object defines a constrained shape\n");
	return GED_ERROR;
    }

    if (argc != 2 && argc != 3) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s\n", usage_string);
	return GED_ERROR;
    }
    int ioffset = 0;
    int contour_ind = -1;
    if (argc == 3) {
	ioffset++;
	if (bu_opt_int(NULL, 1, (const char **)&argv[0], (void *)&contour_ind) != 1 || contour_ind < 0) {
	    bu_vls_printf(gedp->ged_result_str, "Invalid argument %s\n", argv[0]);
	    return GED_ERROR;
	}
	p->curr_contour_i = contour_ind;
    } else {
	p->curr_contour_i = 0;
    }
    int x,y;
    if (bu_opt_int(NULL, 1, (const char **)&argv[ioffset], (void *)&x) != 1 || x < 0) {
	bu_vls_printf(gedp->ged_result_str, "Invalid argument %s\n", argv[ioffset]);
	return GED_ERROR;
    }
    if (bu_opt_int(NULL, 1, (const char **)&argv[ioffset+1], (void *)&y) != 1 || y < 0) {
	bu_vls_printf(gedp->ged_result_str, "Invalid argument %s\n", argv[ioffset+1]);
	return GED_ERROR;
    }

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
    if (!s) {
	bu_vls_printf(gedp->ged_result_str, "View object %s does not exist\n", gd->vobj);
	return GED_ERROR;
    }
    if (!(s->s_type_flags & BVIEW_VIEWONLY) || !(s->s_type_flags & BVIEW_POLYGONS)) {
	bu_vls_printf(gedp->ged_result_str, "Specified object is not a view polygon.\n");
	return GED_ERROR;
    }

    struct bview_polygon *p = (struct bview_polygon *)s->s_i_data;
    if (p->type != BVIEW_POLYGON_GENERAL) {
	bu_vls_printf(gedp->ged_result_str, "Individual point movement is only supported for general polygons - specified object defines a constrained shape.  Use \"update\" to adjust constrained shapes.\n");
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
    if (!s) {
	bu_vls_printf(gedp->ged_result_str, "View object %s does not exist\n", gd->vobj);
	return GED_ERROR;
    }
    if (!(s->s_type_flags & BVIEW_VIEWONLY) || !(s->s_type_flags & BVIEW_POLYGONS)) {
	bu_vls_printf(gedp->ged_result_str, "Specified object is not a view polygon.\n");
	return GED_ERROR;
    }

    struct bview_polygon *p = (struct bview_polygon *)s->s_i_data;
    p->sflag = 0;
    p->mflag = 0;
    p->aflag = 0;
    p->curr_contour_i = 0;
    p->curr_point_i = -1;

    bview_update_polygon(s);

    return GED_OK;
}

int
_poly_cmd_close(void *bs, int argc, const char **argv)
{
    struct _ged_view_info *gd = (struct _ged_view_info *)bs;
    struct ged *gedp = gd->gedp;
    const char *usage_string = "view obj <objname> polygon close [ind]";
    const char *purpose_string = "contour -> polygon";
    if (_view_cmd_msgs(bs, argc, argv, usage_string, purpose_string))
	return GED_OK;

    argc--; argv++;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    struct bview_scene_obj *s = gd->s;
    if (!s) {
	bu_vls_printf(gedp->ged_result_str, "View object %s does not exist\n", gd->vobj);
	return GED_ERROR;
    }
    if (!(s->s_type_flags & BVIEW_VIEWONLY) || !(s->s_type_flags & BVIEW_POLYGONS)) {
	bu_vls_printf(gedp->ged_result_str, "Specified object is not a view polygon.\n");
	return GED_ERROR;
    }

    struct bview_polygon *p = (struct bview_polygon *)s->s_i_data;
    if (p->type != BVIEW_POLYGON_GENERAL) {
	return GED_OK;
    }

    int ind = -1;
    if (argc == 2) {
	if (bu_opt_int(NULL, 1, (const char **)&argv[1], (void *)&ind) != 1) {
	    bu_vls_printf(gedp->ged_result_str, "Invalid argument %s\n", argv[1]);
	    return GED_ERROR;
	}
	if (ind < 0 || ind > 1) {
	    bu_vls_printf(gedp->ged_result_str, "Invalid argument %s\n", argv[1]);
	    return GED_ERROR;
	}
    }

   if (ind < 0) {
       // Close all contours
       for (size_t i = 0; i < p->polygon.num_contours; i++) {
	   p->polygon.contour[i].closed = 1;
       }
   } else {
       p->polygon.contour[ind].closed = 1;
   }

    p->sflag = 0;
    p->mflag = 0;
    p->aflag = 0;

    bview_update_polygon(s);

    return GED_OK;
}

int
_poly_cmd_open(void *bs, int argc, const char **argv)
{
    struct _ged_view_info *gd = (struct _ged_view_info *)bs;
    struct ged *gedp = gd->gedp;
    const char *usage_string = "view obj <objname> polygon open [ind]";
    const char *purpose_string = "polygon -> contour";
    if (_view_cmd_msgs(bs, argc, argv, usage_string, purpose_string))
	return GED_OK;

    argc--; argv++;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    struct bview_scene_obj *s = gd->s;
    if (!s) {
	bu_vls_printf(gedp->ged_result_str, "View object %s does not exist\n", gd->vobj);
	return GED_ERROR;
    }
    if (!(s->s_type_flags & BVIEW_VIEWONLY) || !(s->s_type_flags & BVIEW_POLYGONS)) {
	bu_vls_printf(gedp->ged_result_str, "Specified object is not a view polygon.\n");
	return GED_ERROR;
    }

    struct bview_polygon *p = (struct bview_polygon *)s->s_i_data;
    if (p->type != BVIEW_POLYGON_GENERAL) {
	bu_vls_printf(gedp->ged_result_str, "Constrained polygon shapes are always closed.\n");
	return GED_ERROR;
    }

    int ind = -1;
    if (argc == 2) {
	if (bu_opt_int(NULL, 1, (const char **)&argv[1], (void *)&ind) != 1) {
	    bu_vls_printf(gedp->ged_result_str, "Invalid argument %s\n", argv[1]);
	    return GED_ERROR;
	}
	if (ind < 0 || ind > 1) {
	    bu_vls_printf(gedp->ged_result_str, "Invalid argument %s\n", argv[1]);
	    return GED_ERROR;
	}
    }

   if (ind < 0) {
       // Open all contours
       for (size_t i = 0; i < p->polygon.num_contours; i++) {
	   p->polygon.contour[i].closed = 0;
       }
   } else {
       p->polygon.contour[ind].closed = 0;
   }

    p->sflag = 0;
    p->mflag = 0;
    p->aflag = 0;

    bview_update_polygon(s);

    return GED_OK;
}

int
_poly_cmd_viewsnap(void *bs, int argc, const char **argv)
{
    struct _ged_view_info *gd = (struct _ged_view_info *)bs;
    struct ged *gedp = gd->gedp;
    const char *usage_string = "view obj <objname> polygon";
    const char *purpose_string = "set view to match that used for polygon creation";
    if (_view_cmd_msgs(bs, argc, argv, usage_string, purpose_string))
	return GED_OK;

    argc--; argv++;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    struct bview_scene_obj *s = gd->s;
    if (!s) {
	bu_vls_printf(gedp->ged_result_str, "View object %s does not exist\n", gd->vobj);
	return GED_ERROR;
    }
    if (!(s->s_type_flags & BVIEW_VIEWONLY) || !(s->s_type_flags & BVIEW_POLYGONS)) {
	bu_vls_printf(gedp->ged_result_str, "Specified object is not a view polygon.\n");
	return GED_ERROR;
    }

    // Set view info
    struct bview_polygon *p = (struct bview_polygon *)s->s_i_data;
    bview_sync(gedp->ged_gvp, &p->v);
    bview_update(gedp->ged_gvp);

    return GED_OK;
}

const struct bu_cmdtab _poly_cmds[] = {
    { "create",          _poly_cmd_create},
    { "update",          _poly_cmd_update},
    { "select",          _poly_cmd_select},
    { "move",            _poly_cmd_move},
    { "append",          _poly_cmd_append},
    { "clear",           _poly_cmd_clear},
    { "close",           _poly_cmd_close},
    { "open",            _poly_cmd_open},
    { "viewsnap",        _poly_cmd_viewsnap},
    { (char *)NULL,      NULL}
};

int
_view_cmd_polygons(void *bs, int argc, const char **argv)
{
    int help = 0;
    int s_version = 0;
    struct _ged_view_info *gd = (struct _ged_view_info *)bs;
    struct ged *gedp = gd->gedp;

    const char *usage_string = "view obj <objname> polygon [options] [args]";
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

    return _ged_subcmd_exec(gedp, d, _poly_cmds, "view obj <objname>", "[options] subcommand [args]", gd, argc, argv, help, cmd_pos);
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
