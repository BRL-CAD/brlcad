/*                      P O L Y G O N S . C
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
#include "bg/polygon.h"
#include "rt/geom.h"
#include "rt/primitives/sketch.h"

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
	return BRLCAD_OK;

    argc--; argv++;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    struct bv_scene_obj *s = gd->s;
    if (s) {
	bu_vls_printf(gedp->ged_result_str, "View object named %s already exists\n", gd->vobj);
	return BRLCAD_ERROR;
    }

    if (argc != 2 && argc != 3) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s\n", usage_string);
	return BRLCAD_ERROR;
    }
    int x,y;
    if (bu_opt_int(NULL, 1, (const char **)&argv[0], (void *)&x) != 1 || x < 0) {
	bu_vls_printf(gedp->ged_result_str, "Invalid argument %s\n", argv[0]);
	return BRLCAD_ERROR;
    }
    if (bu_opt_int(NULL, 1, (const char **)&argv[1], (void *)&y) != 1 || y < 0) {
	bu_vls_printf(gedp->ged_result_str, "Invalid argument %s\n", argv[1]);
	return BRLCAD_ERROR;
    }

    int type = BV_POLYGON_GENERAL;
    if (argc == 3) {
	if (BU_STR_EQUAL(argv[2], "circ") || BU_STR_EQUAL(argv[2], "circle"))
	    type = BV_POLYGON_CIRCLE;
	if (BU_STR_EQUAL(argv[2], "ell") || BU_STR_EQUAL(argv[2], "ellipse"))
	    type = BV_POLYGON_ELLIPSE;
	if (BU_STR_EQUAL(argv[2], "rect") || BU_STR_EQUAL(argv[2], "rectangle"))
	    type = BV_POLYGON_RECTANGLE;
	if (BU_STR_EQUAL(argv[2], "sq") || BU_STR_EQUAL(argv[2], "square"))
	    type = BV_POLYGON_SQUARE;
	if (type == BV_POLYGON_GENERAL) {
	    bu_vls_printf(gedp->ged_result_str, "Unknown polygon type %s\n", argv[2]);
	    return BRLCAD_ERROR;
	}
    }

    s = bv_create_polygon(gedp->ged_gvp, type, x, y, gedp->ged_views.free_scene_obj);
    if (!s) {
	bu_vls_printf(gedp->ged_result_str, "Failed to create %s\n", gd->vobj);
	return BRLCAD_ERROR;
    }
    bu_vls_init(&s->s_uuid);
    bu_vls_printf(&s->s_uuid, "%s", gd->vobj);
    bu_ptbl_ins(gedp->ged_gvp->gv_objs.view_objs, (long *)s);

    return BRLCAD_OK;
}

int
_poly_cmd_select(void *bs, int argc, const char **argv)
{
    struct _ged_view_info *gd = (struct _ged_view_info *)bs;
    struct ged *gedp = gd->gedp;
    const char *usage_string = "view obj <objname> polygon select [contour] x y";
    const char *purpose_string = "select polygon point closest to point x,y";
    if (_view_cmd_msgs(bs, argc, argv, usage_string, purpose_string))
	return BRLCAD_OK;

    argc--; argv++;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    struct bv_scene_obj *s = gd->s;
    if (!s) {
	bu_vls_printf(gedp->ged_result_str, "View object %s does not exist\n", gd->vobj);
	return BRLCAD_ERROR;
    }
    if (!(s->s_type_flags & BV_VIEWONLY) || !(s->s_type_flags & BV_POLYGONS)) {
	bu_vls_printf(gedp->ged_result_str, "Specified object is not a view polygon.\n");
	return BRLCAD_ERROR;
    }

    struct bv_polygon *p = (struct bv_polygon *)s->s_i_data;

    if (p->type != BV_POLYGON_GENERAL) {
	bu_vls_printf(gedp->ged_result_str, "Point selection is only supported for general polygons - specified object defines a constrained shape\n");
	return BRLCAD_ERROR;
    }

    if (argc != 2 && argc != 3) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s\n", usage_string);
	return BRLCAD_ERROR;
    }
    int ioffset = 0;
    int contour_ind = -1;
    if (argc == 3) {
	ioffset++;
	if (bu_opt_int(NULL, 1, (const char **)&argv[0], (void *)&contour_ind) != 1 || contour_ind < 0) {
	    bu_vls_printf(gedp->ged_result_str, "Invalid argument %s\n", argv[0]);
	    return BRLCAD_ERROR;
	}
	p->curr_contour_i = contour_ind;
    } else {
	p->curr_contour_i = 0;
    }
    int x,y;
    if (bu_opt_int(NULL, 1, (const char **)&argv[ioffset], (void *)&x) != 1 || x < 0) {
	bu_vls_printf(gedp->ged_result_str, "Invalid argument %s\n", argv[ioffset]);
	return BRLCAD_ERROR;
    }
    if (bu_opt_int(NULL, 1, (const char **)&argv[ioffset+1], (void *)&y) != 1 || y < 0) {
	bu_vls_printf(gedp->ged_result_str, "Invalid argument %s\n", argv[ioffset+1]);
	return BRLCAD_ERROR;
    }

    p->curr_contour_i = contour_ind;
    s->s_v->gv_mouse_x = x;
    s->s_v->gv_mouse_y = y;
    bv_update_polygon(s, BV_POLYGON_UPDATE_PT_SELECT);

    return BRLCAD_OK;
}


int
_poly_cmd_append(void *bs, int argc, const char **argv)
{
    struct _ged_view_info *gd = (struct _ged_view_info *)bs;
    struct ged *gedp = gd->gedp;
    const char *usage_string = "view obj <objname> polygon append [contour] x y";
    const char *purpose_string = "append point to polygon";
    if (_view_cmd_msgs(bs, argc, argv, usage_string, purpose_string))
	return BRLCAD_OK;

    argc--; argv++;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    struct bv_scene_obj *s = gd->s;
    if (!s) {
	bu_vls_printf(gedp->ged_result_str, "View object %s does not exist\n", gd->vobj);
	return BRLCAD_ERROR;
    }
    if (!(s->s_type_flags & BV_VIEWONLY) || !(s->s_type_flags & BV_POLYGONS)) {
	bu_vls_printf(gedp->ged_result_str, "Specified object is not a view polygon.\n");
	return BRLCAD_ERROR;
    }

    struct bv_polygon *p = (struct bv_polygon *)s->s_i_data;
    if (p->type != BV_POLYGON_GENERAL) {
	bu_vls_printf(gedp->ged_result_str, "Point appending is only supported for general polygons - specified object defines a constrained shape\n");
	return BRLCAD_ERROR;
    }

    if (argc != 2 && argc != 3) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s\n", usage_string);
	return BRLCAD_ERROR;
    }
    int ioffset = 0;
    int contour_ind = -1;
    if (argc == 3) {
	ioffset++;
	if (bu_opt_int(NULL, 1, (const char **)&argv[0], (void *)&contour_ind) != 1 || contour_ind < 0) {
	    bu_vls_printf(gedp->ged_result_str, "Invalid argument %s\n", argv[0]);
	    return BRLCAD_ERROR;
	}
	p->curr_contour_i = contour_ind;
    } else {
	p->curr_contour_i = 0;
    }
    int x,y;
    if (bu_opt_int(NULL, 1, (const char **)&argv[ioffset], (void *)&x) != 1 || x < 0) {
	bu_vls_printf(gedp->ged_result_str, "Invalid argument %s\n", argv[ioffset]);
	return BRLCAD_ERROR;
    }
    if (bu_opt_int(NULL, 1, (const char **)&argv[ioffset+1], (void *)&y) != 1 || y < 0) {
	bu_vls_printf(gedp->ged_result_str, "Invalid argument %s\n", argv[ioffset+1]);
	return BRLCAD_ERROR;
    }

    s->s_v->gv_mouse_x = x;
    s->s_v->gv_mouse_y = y;
    bv_update_polygon(s, BV_POLYGON_UPDATE_PT_APPEND);

    return BRLCAD_OK;
}

int
_poly_cmd_move(void *bs, int argc, const char **argv)
{
    struct _ged_view_info *gd = (struct _ged_view_info *)bs;
    struct ged *gedp = gd->gedp;
    const char *usage_string = "view obj <objname> polygon move x y";
    const char *purpose_string = "move selected polygon point to x,y";
    if (_view_cmd_msgs(bs, argc, argv, usage_string, purpose_string))
	return BRLCAD_OK;

    argc--; argv++;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    struct bv_scene_obj *s = gd->s;
    if (!s) {
	bu_vls_printf(gedp->ged_result_str, "View object %s does not exist\n", gd->vobj);
	return BRLCAD_ERROR;
    }
    if (!(s->s_type_flags & BV_VIEWONLY) || !(s->s_type_flags & BV_POLYGONS)) {
	bu_vls_printf(gedp->ged_result_str, "Specified object is not a view polygon.\n");
	return BRLCAD_ERROR;
    }

    struct bv_polygon *p = (struct bv_polygon *)s->s_i_data;
    if (p->type != BV_POLYGON_GENERAL) {
	bu_vls_printf(gedp->ged_result_str, "Individual point movement is only supported for general polygons - specified object defines a constrained shape.  Use \"update\" to adjust constrained shapes.\n");
	return BRLCAD_ERROR;
    }

    if (argc != 2) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s\n", usage_string);
	return BRLCAD_ERROR;
    }
    int x,y;
    if (bu_opt_int(NULL, 1, (const char **)&argv[0], (void *)&x) != 1 || x < 0) {
	bu_vls_printf(gedp->ged_result_str, "Invalid argument %s\n", argv[0]);
	return BRLCAD_ERROR;
    }
    if (bu_opt_int(NULL, 1, (const char **)&argv[1], (void *)&y) != 1 || y < 0) {
	bu_vls_printf(gedp->ged_result_str, "Invalid argument %s\n", argv[1]);
	return BRLCAD_ERROR;
    }

    s->s_v->gv_mouse_x = x;
    s->s_v->gv_mouse_y = y;
    bv_update_polygon(s, BV_POLYGON_UPDATE_PT_MOVE);

    return BRLCAD_OK;
}

int
_poly_cmd_clear(void *bs, int argc, const char **argv)
{
    struct _ged_view_info *gd = (struct _ged_view_info *)bs;
    struct ged *gedp = gd->gedp;
    const char *usage_string = "view obj <objname> polygon clear";
    const char *purpose_string = "clear all modification flags";
    if (_view_cmd_msgs(bs, argc, argv, usage_string, purpose_string))
	return BRLCAD_OK;

    argc--; argv++;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    struct bv_scene_obj *s = gd->s;
    if (!s) {
	bu_vls_printf(gedp->ged_result_str, "View object %s does not exist\n", gd->vobj);
	return BRLCAD_ERROR;
    }
    if (!(s->s_type_flags & BV_VIEWONLY) || !(s->s_type_flags & BV_POLYGONS)) {
	bu_vls_printf(gedp->ged_result_str, "Specified object is not a view polygon.\n");
	return BRLCAD_ERROR;
    }

    struct bv_polygon *p = (struct bv_polygon *)s->s_i_data;
    p->curr_contour_i = 0;
    p->curr_point_i = -1;
    bv_update_polygon(s, BV_POLYGON_UPDATE_DEFAULT);

    return BRLCAD_OK;
}

int
_poly_cmd_close(void *bs, int argc, const char **argv)
{
    struct _ged_view_info *gd = (struct _ged_view_info *)bs;
    struct ged *gedp = gd->gedp;
    const char *usage_string = "view obj <objname> polygon close [ind]";
    const char *purpose_string = "contour -> polygon";
    if (_view_cmd_msgs(bs, argc, argv, usage_string, purpose_string))
	return BRLCAD_OK;

    argc--; argv++;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    struct bv_scene_obj *s = gd->s;
    if (!s) {
	bu_vls_printf(gedp->ged_result_str, "View object %s does not exist\n", gd->vobj);
	return BRLCAD_ERROR;
    }
    if (!(s->s_type_flags & BV_VIEWONLY) || !(s->s_type_flags & BV_POLYGONS)) {
	bu_vls_printf(gedp->ged_result_str, "Specified object is not a view polygon.\n");
	return BRLCAD_ERROR;
    }

    struct bv_polygon *p = (struct bv_polygon *)s->s_i_data;
    if (p->type != BV_POLYGON_GENERAL) {
	return BRLCAD_OK;
    }

    int ind = -1;
    if (argc == 2) {
	if (bu_opt_int(NULL, 1, (const char **)&argv[1], (void *)&ind) != 1) {
	    bu_vls_printf(gedp->ged_result_str, "Invalid argument %s\n", argv[1]);
	    return BRLCAD_ERROR;
	}
	if (ind < 0 || ind > 1) {
	    bu_vls_printf(gedp->ged_result_str, "Invalid argument %s\n", argv[1]);
	    return BRLCAD_ERROR;
	}
    }

   if (ind < 0) {
       // Close all contours
       for (size_t i = 0; i < p->polygon.num_contours; i++) {
	   p->polygon.contour[i].open = 0;
       }
   } else {
       p->polygon.contour[ind].open = 0;
   }

    bv_update_polygon(s, BV_POLYGON_UPDATE_DEFAULT);

    return BRLCAD_OK;
}

int
_poly_cmd_open(void *bs, int argc, const char **argv)
{
    struct _ged_view_info *gd = (struct _ged_view_info *)bs;
    struct ged *gedp = gd->gedp;
    const char *usage_string = "view obj <objname> polygon open [ind]";
    const char *purpose_string = "polygon -> contour";
    if (_view_cmd_msgs(bs, argc, argv, usage_string, purpose_string))
	return BRLCAD_OK;

    argc--; argv++;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    struct bv_scene_obj *s = gd->s;
    if (!s) {
	bu_vls_printf(gedp->ged_result_str, "View object %s does not exist\n", gd->vobj);
	return BRLCAD_ERROR;
    }
    if (!(s->s_type_flags & BV_VIEWONLY) || !(s->s_type_flags & BV_POLYGONS)) {
	bu_vls_printf(gedp->ged_result_str, "Specified object is not a view polygon.\n");
	return BRLCAD_ERROR;
    }

    struct bv_polygon *p = (struct bv_polygon *)s->s_i_data;
    if (p->type != BV_POLYGON_GENERAL) {
	bu_vls_printf(gedp->ged_result_str, "Constrained polygon shapes are always closed.\n");
	return BRLCAD_ERROR;
    }

    int ind = -1;
    if (argc == 2) {
	if (bu_opt_int(NULL, 1, (const char **)&argv[1], (void *)&ind) != 1) {
	    bu_vls_printf(gedp->ged_result_str, "Invalid argument %s\n", argv[1]);
	    return BRLCAD_ERROR;
	}
	if (ind < 0 || ind > 1) {
	    bu_vls_printf(gedp->ged_result_str, "Invalid argument %s\n", argv[1]);
	    return BRLCAD_ERROR;
	}
    }

   if (ind < 0) {
       // Open all contours
       for (size_t i = 0; i < p->polygon.num_contours; i++) {
	   p->polygon.contour[i].open = 1;
       }
   } else {
       p->polygon.contour[ind].open = 1;
   }

    bv_update_polygon(s, BV_POLYGON_UPDATE_DEFAULT);

    return BRLCAD_OK;
}

int
_poly_cmd_viewsnap(void *bs, int argc, const char **argv)
{
    struct _ged_view_info *gd = (struct _ged_view_info *)bs;
    struct ged *gedp = gd->gedp;
    const char *usage_string = "view obj <objname> polygon";
    const char *purpose_string = "set view to match that used for polygon creation";
    if (_view_cmd_msgs(bs, argc, argv, usage_string, purpose_string))
	return BRLCAD_OK;

    argc--; argv++;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    struct bv_scene_obj *s = gd->s;
    if (!s) {
	bu_vls_printf(gedp->ged_result_str, "View object %s does not exist\n", gd->vobj);
	return BRLCAD_ERROR;
    }
    if (!(s->s_type_flags & BV_VIEWONLY) || !(s->s_type_flags & BV_POLYGONS)) {
	bu_vls_printf(gedp->ged_result_str, "Specified object is not a view polygon.\n");
	return BRLCAD_ERROR;
    }

    // Set view info
    struct bv_polygon *p = (struct bv_polygon *)s->s_i_data;
    bv_sync(gedp->ged_gvp, &p->v);
    bv_update(gedp->ged_gvp);

    return BRLCAD_OK;
}

int
_poly_cmd_to_curr_view(void *bs, int argc, const char **argv)
{
    struct _ged_view_info *gd = (struct _ged_view_info *)bs;
    struct ged *gedp = gd->gedp;
    const char *usage_string = "view obj <objname> polygon";
    const char *purpose_string = "set view to match that used for polygon creation";
    if (_view_cmd_msgs(bs, argc, argv, usage_string, purpose_string))
	return BRLCAD_OK;

    argc--; argv++;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    struct bv_scene_obj *s = gd->s;
    if (!s) {
	bu_vls_printf(gedp->ged_result_str, "View object %s does not exist\n", gd->vobj);
	return BRLCAD_ERROR;
    }
    if (!(s->s_type_flags & BV_VIEWONLY) || !(s->s_type_flags & BV_POLYGONS)) {
	bu_vls_printf(gedp->ged_result_str, "Specified object is not a view polygon.\n");
	return BRLCAD_ERROR;
    }

    // Set view info
    struct bv_polygon *p = (struct bv_polygon *)s->s_i_data;
    bv_sync(&p->v, gedp->ged_gvp);
    bv_update_polygon(s, BV_POLYGON_UPDATE_DEFAULT);
    bv_update(gedp->ged_gvp);

    return BRLCAD_OK;
}

int
_poly_cmd_area(void *bs, int argc, const char **argv)
{
    struct _ged_view_info *gd = (struct _ged_view_info *)bs;
    struct ged *gedp = gd->gedp;
    const char *usage_string = "view obj <objname> polygon area";
    const char *purpose_string = "report polygon area";
    if (_view_cmd_msgs(bs, argc, argv, usage_string, purpose_string))
	return BRLCAD_OK;

    argc--; argv++;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    struct bv_scene_obj *s = gd->s;
    if (!s) {
	bu_vls_printf(gedp->ged_result_str, "View object %s does not exist\n", gd->vobj);
	return BRLCAD_ERROR;
    }
    if (!(s->s_type_flags & BV_VIEWONLY) || !(s->s_type_flags & BV_POLYGONS)) {
	bu_vls_printf(gedp->ged_result_str, "Specified object is not a view polygon.\n");
	return BRLCAD_ERROR;
    }

    struct bv_polygon *p = (struct bv_polygon *)s->s_i_data;

    double area = bg_find_polygon_area(&p->polygon, CLIPPER_MAX,
	                               p->v.gv_model2view, p->v.gv_scale);

    bu_vls_printf(gedp->ged_result_str, "%g", area * gedp->dbip->dbi_base2local);
    return BRLCAD_OK;
}

int
_poly_cmd_overlap(void *bs, int argc, const char **argv)
{
    struct _ged_view_info *gd = (struct _ged_view_info *)bs;
    struct ged *gedp = gd->gedp;
    const char *usage_string = "view obj <obj1> polygon overlap <obj2>";
    const char *purpose_string = "report if two polygons overlap";
    if (_view_cmd_msgs(bs, argc, argv, usage_string, purpose_string))
	return BRLCAD_OK;

    argc--; argv++;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    struct bv_scene_obj *s = gd->s;
    if (!s) {
	bu_vls_printf(gedp->ged_result_str, "View object %s does not exist\n", gd->vobj);
	return BRLCAD_ERROR;
    }
    if (!(s->s_type_flags & BV_VIEWONLY) || !(s->s_type_flags & BV_POLYGONS)) {
	bu_vls_printf(gedp->ged_result_str, "Specified object is not a view polygon.\n");
	return BRLCAD_ERROR;
    }

    if (!argc) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s\n", usage_string);
	return BRLCAD_ERROR;
    }

    // Look up the polygon to check for overlaps
    struct bview *v = gedp->ged_gvp;
    struct bv_scene_obj *s2 = NULL;
    for (size_t i = 0; i < BU_PTBL_LEN(v->gv_objs.view_objs); i++) {
        struct bv_scene_obj *stest = (struct bv_scene_obj *)BU_PTBL_GET(v->gv_objs.view_objs, i);
        if (BU_STR_EQUAL(argv[0], bu_vls_cstr(&stest->s_uuid))) {
            s2 = stest;
            break;
        }
    }
    if (!s2) {
	bu_vls_printf(gedp->ged_result_str, "View object %s does not exist\n", argv[0]);
	return BRLCAD_ERROR;
    }
    if (!(s2->s_type_flags & BV_VIEWONLY) || !(s2->s_type_flags & BV_POLYGONS)) {
	bu_vls_printf(gedp->ged_result_str, "%s is not a view polygon.\n", argv[0]);
	return BRLCAD_ERROR;
    }

    // Have two polygons.  Check for overlaps, using the origin view of the
    // obj1 polygon.
    struct bv_polygon *polyA = (struct bv_polygon *)s->s_i_data;
    struct bv_polygon *polyB = (struct bv_polygon *)s2->s_i_data;

    int ovlp = bg_polygons_overlap(&polyA->polygon, &polyB->polygon, polyA->v.gv_model2view, &gedp->ged_wdbp->wdb_tol, polyA->v.gv_scale);

    bu_vls_printf(gedp->ged_result_str, "%d", ovlp);

    return BRLCAD_OK;
}

struct segment_node {
    struct bu_list l;
    int reverse;
    int used;
    void *segment;
};

struct contour_node {
    struct bu_list l;
    struct bu_list head;
};

int
_poly_cmd_import(void *bs, int argc, const char **argv)
{
    struct _ged_view_info *gd = (struct _ged_view_info *)bs;
    struct ged *gedp = gd->gedp;
    const char *usage_string = "view obj <objname> polygon import <sketchname>";
    const char *purpose_string = "import polygon from sketch";
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

    if (!argc) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s\n", usage_string);
	return BRLCAD_ERROR;
    }


    // Begin import
    struct directory *dp = db_lookup(gedp->dbip, argv[0], LOOKUP_QUIET);
    if (dp == RT_DIR_NULL) {
	return BRLCAD_ERROR;
    }

    s = db_sketch_to_scene_obj(gd->vobj, gedp->dbip, dp, gedp->ged_gvp, gedp->ged_views.free_scene_obj);
    if (!s) {
	bu_vls_printf(gedp->ged_result_str, "Failed to create %s\n", gd->vobj);
	return BRLCAD_ERROR;
    }

    /* Done - add to scene objects */
    bu_ptbl_ins(gedp->ged_gvp->gv_objs.view_objs, (long *)s);

    return BRLCAD_OK;
}

int
_poly_cmd_export(void *bs, int argc, const char **argv)
{
    struct _ged_view_info *gd = (struct _ged_view_info *)bs;
    struct ged *gedp = gd->gedp;
    const char *usage_string = "view obj <objname> polygon export <sketchname>";
    const char *purpose_string = "export polygon to sketch";
    if (_view_cmd_msgs(bs, argc, argv, usage_string, purpose_string))
	return BRLCAD_OK;

    argc--; argv++;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    struct bv_scene_obj *s = gd->s;
    if (!s) {
	bu_vls_printf(gedp->ged_result_str, "View object %s does not exist\n", gd->vobj);
	return BRLCAD_ERROR;
    }
    if (!(s->s_type_flags & BV_VIEWONLY) || !(s->s_type_flags & BV_POLYGONS)) {
	bu_vls_printf(gedp->ged_result_str, "Specified object is not a view polygon.\n");
	return BRLCAD_ERROR;
    }

    if (!argc) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s\n", usage_string);
	return BRLCAD_ERROR;
    }

    GED_CHECK_EXISTS(gedp, argv[0], LOOKUP_QUIET, BRLCAD_ERROR);


    if (db_scene_obj_to_sketch(gedp->dbip, argv[0], s) != BRLCAD_OK) {
	bu_vls_printf(gedp->ged_result_str, "Failed to create sketch.\n");
	return BRLCAD_ERROR;
    }

    return BRLCAD_OK;
}

int
_poly_cmd_fill(void *bs, int argc, const char **argv)
{
    struct _ged_view_info *gd = (struct _ged_view_info *)bs;
    struct ged *gedp = gd->gedp;
    const char *usage_string = "view obj <obj1> polygon fill [dx dy spacing]";
    const char *purpose_string = "use lines to visualize polygon interior";
    if (_view_cmd_msgs(bs, argc, argv, usage_string, purpose_string))
	return BRLCAD_OK;

    argc--; argv++;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    struct bv_scene_obj *s = gd->s;
    if (!s) {
	bu_vls_printf(gedp->ged_result_str, "View object %s does not exist\n", gd->vobj);
	return BRLCAD_ERROR;
    }
    if (!(s->s_type_flags & BV_VIEWONLY) || !(s->s_type_flags & BV_POLYGONS)) {
	bu_vls_printf(gedp->ged_result_str, "Specified object is not a view polygon.\n");
	return BRLCAD_ERROR;
    }

    if (argc == 1 && BU_STR_EQUAL(argv[0], "0")) {
	struct bv_polygon *p = (struct bv_polygon *)s->s_i_data;
	p->fill_flag = 0;
	bv_update_polygon(s, BV_POLYGON_UPDATE_DEFAULT);
	return BRLCAD_OK;
    }

    if (argc != 3) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s\n", usage_string);
	return BRLCAD_ERROR;
    }
    vect2d_t vdir;
    fastf_t vdelta;
    if (bu_opt_fastf_t(NULL, 1, (const char **)&argv[0], (void *)&vdir[0]) != 1) {
	bu_vls_printf(gedp->ged_result_str, "Invalid argument %s\n", argv[0]);
	return BRLCAD_ERROR;
    }
    if (bu_opt_fastf_t(NULL, 1, (const char **)&argv[1], (void *)&vdir[1]) != 1) {
	bu_vls_printf(gedp->ged_result_str, "Invalid argument %s\n", argv[1]);
	return BRLCAD_ERROR;
    }
    if (bu_opt_fastf_t(NULL, 1, (const char **)&argv[2], (void *)&vdelta) != 1) {
	bu_vls_printf(gedp->ged_result_str, "Invalid argument %s\n", argv[2]);
	return BRLCAD_ERROR;
    }

    struct bv_polygon *p = (struct bv_polygon *)s->s_i_data;
    p->fill_flag = 1;
    V2MOVE(p->fill_dir, vdir);
    p->fill_delta = vdelta;
    bv_update_polygon(s, BV_POLYGON_UPDATE_DEFAULT);

    return BRLCAD_OK;
}

int
_poly_cmd_fill_color(void *bs, int argc, const char **argv)
{
    struct _ged_view_info *gd = (struct _ged_view_info *)bs;
    struct ged *gedp = gd->gedp;
    const char *usage_string = "view obj <obj1> polygon fill_color [r/g/b]";
    const char *purpose_string = "customize fill lines color (if fill is enabled)";
    if (_view_cmd_msgs(bs, argc, argv, usage_string, purpose_string))
	return BRLCAD_OK;

    argc--; argv++;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    struct bv_scene_obj *s = gd->s;
    if (!s) {
	bu_vls_printf(gedp->ged_result_str, "View object %s does not exist\n", gd->vobj);
	return BRLCAD_ERROR;
    }
    if (!(s->s_type_flags & BV_VIEWONLY) || !(s->s_type_flags & BV_POLYGONS)) {
	bu_vls_printf(gedp->ged_result_str, "Specified object is not a view polygon.\n");
	return BRLCAD_ERROR;
    }

    struct bv_polygon *p = (struct bv_polygon *)s->s_i_data;

    if (!argc) {
	unsigned char frgb[3];
	bu_color_to_rgb_chars(&p->fill_color, (unsigned char *)frgb);

	bu_vls_printf(gedp->ged_result_str, "%d/%d/%d\n", frgb[0], frgb[1], frgb[2]);

	return BRLCAD_OK;
    }

    if (bu_opt_color(NULL, 1, (const char **)&argv[0], (void *)&p->fill_color) != 1) {
	bu_vls_printf(gedp->ged_result_str, "Invalid argument %s\n", argv[0]);
	return BRLCAD_ERROR;
    }

    struct bv_scene_obj *fobj = NULL;
    for (size_t i = 0; i < BU_PTBL_LEN(&s->children); i++) {
	struct bv_scene_obj *s_c = (struct bv_scene_obj *)BU_PTBL_GET(&s->children, i);
	if (BU_STR_EQUAL(bu_vls_cstr(&s_c->s_uuid), "fill")) {
	    fobj = s_c;
	    break;
	}
    }

    if (fobj) {
	bu_color_to_rgb_chars(&p->fill_color, fobj->s_color);
    }

    return BRLCAD_OK;
}

int
_poly_cmd_csg(void *bs, int argc, const char **argv)
{
    struct _ged_view_info *gd = (struct _ged_view_info *)bs;
    struct ged *gedp = gd->gedp;
    const char *usage_string = "view obj <obj1> polygon csg <u|-|+> <obj2>";
    const char *purpose_string = "replace obj1's polygon with the result of obj2 u/-/+ obj1";
    if (_view_cmd_msgs(bs, argc, argv, usage_string, purpose_string))
	return BRLCAD_OK;

    argc--; argv++;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    struct bv_scene_obj *s = gd->s;
    if (!s) {
	bu_vls_printf(gedp->ged_result_str, "View object %s does not exist\n", gd->vobj);
	return BRLCAD_ERROR;
    }
    if (!(s->s_type_flags & BV_VIEWONLY) || !(s->s_type_flags & BV_POLYGONS)) {
	bu_vls_printf(gedp->ged_result_str, "Specified object is not a view polygon.\n");
	return BRLCAD_ERROR;
    }

    if (argc != 2) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s\n", usage_string);
	return BRLCAD_ERROR;
    }

    bg_clip_t op = bg_Union;
    char c = argv[0][0];
    switch (c) {
	case 'u':
	    op = bg_Union;
	    break;
	case '-':
	    op = bg_Difference;
	    break;
	case '+':
	    op = bg_Intersection;
	    break;
	default:
	    bu_vls_printf(gedp->ged_result_str, "Invalid boolean operator \"%s\"\n", argv[0]);
	    break;
    }

    // Look up the polygon to check for overlaps
    struct bview *v = gedp->ged_gvp;
    struct bv_scene_obj *s2 = NULL;
    for (size_t i = 0; i < BU_PTBL_LEN(v->gv_objs.view_objs); i++) {
        struct bv_scene_obj *stest = (struct bv_scene_obj *)BU_PTBL_GET(v->gv_objs.view_objs, i);
        if (BU_STR_EQUAL(argv[1], bu_vls_cstr(&stest->s_uuid))) {
            s2 = stest;
            break;
        }
    }
    if (!s2) {
	bu_vls_printf(gedp->ged_result_str, "View object %s does not exist\n", argv[0]);
	return BRLCAD_ERROR;
    }
    if (!(s2->s_type_flags & BV_VIEWONLY) || !(s2->s_type_flags & BV_POLYGONS)) {
	bu_vls_printf(gedp->ged_result_str, "%s is not a view polygon.\n", argv[0]);
	return BRLCAD_ERROR;
    }

    // Have two polygons.  Check for overlaps, using the origin view of the
    // obj1 polygon.
    struct bv_polygon *polyA = (struct bv_polygon *)s->s_i_data;
    struct bv_polygon *polyB = (struct bv_polygon *)s2->s_i_data;

    struct bg_polygon *cp = bg_clip_polygon(op, &polyA->polygon, &polyB->polygon, CLIPPER_MAX, polyA->v.gv_model2view, polyA->v.gv_view2model);

    if (!cp)
	return BRLCAD_ERROR;

    bg_polygon_free(&polyA->polygon);
    polyA->polygon.num_contours = cp->num_contours;
    polyA->polygon.hole = cp->hole;
    polyA->polygon.contour = cp->contour;

    // clipper results are always general polygons
    polyA->type = BV_POLYGON_GENERAL;

    BU_PUT(cp, struct bg_polygon);
    bv_update_polygon(s, BV_POLYGON_UPDATE_DEFAULT);

    return BRLCAD_OK;
}


const struct bu_cmdtab _poly_cmds[] = {
    { "append",          _poly_cmd_append},
    { "area",            _poly_cmd_area},
    { "clear",           _poly_cmd_clear},
    { "close",           _poly_cmd_close},
    { "create",          _poly_cmd_create},
    { "csg",             _poly_cmd_csg},
    { "export",          _poly_cmd_export},
    { "fill",            _poly_cmd_fill},
    { "fill_color",      _poly_cmd_fill_color},
    { "import",          _poly_cmd_import},
    { "move",            _poly_cmd_move},
    { "open",            _poly_cmd_open},
    { "overlap",         _poly_cmd_overlap},
    { "select",          _poly_cmd_select},
    { "to_curr_view",    _poly_cmd_to_curr_view},
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
	return BRLCAD_OK;

    if (!gedp->ged_gvp) {
	bu_vls_printf(gedp->ged_result_str, ": no view current in GED");
	return BRLCAD_ERROR;
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

    if (help) {
	_ged_cmd_help(gedp, usage_string, d);
	return BRLCAD_HELP;
    }

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
