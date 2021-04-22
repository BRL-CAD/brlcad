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
#include "bg/polygon.h"
#include "rt/geom.h"

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
	   p->polygon.contour[i].open = 0;
       }
   } else {
       p->polygon.contour[ind].open = 0;
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
	   p->polygon.contour[i].open = 1;
       }
   } else {
       p->polygon.contour[ind].open = 1;
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

int
_poly_cmd_area(void *bs, int argc, const char **argv)
{
    struct _ged_view_info *gd = (struct _ged_view_info *)bs;
    struct ged *gedp = gd->gedp;
    const char *usage_string = "view obj <objname> polygon area";
    const char *purpose_string = "report polygon area";
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

    double area = bg_find_polygon_area(&p->polygon, CLIPPER_MAX,
	                               p->v.gv_model2view, p->v.gv_scale);

    bu_vls_printf(gedp->ged_result_str, "%g", area * gedp->ged_wdbp->dbip->dbi_base2local);
    return GED_OK;
}

int
_poly_cmd_overlap(void *bs, int argc, const char **argv)
{
    struct _ged_view_info *gd = (struct _ged_view_info *)bs;
    struct ged *gedp = gd->gedp;
    const char *usage_string = "view obj <obj1> polygon overlap <obj2>";
    const char *purpose_string = "report if two polygons overlap";
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

    if (!argc) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s\n", usage_string);
	return GED_ERROR;
    }

    // Look up the polygon to check for overlaps
    struct bview *v = gedp->ged_gvp;
    struct bview_scene_obj *s2 = NULL;
    for (size_t i = 0; i < BU_PTBL_LEN(v->gv_scene_objs); i++) {
        struct bview_scene_obj *stest = (struct bview_scene_obj *)BU_PTBL_GET(v->gv_scene_objs, i);
        if (BU_STR_EQUAL(argv[0], bu_vls_cstr(&stest->s_uuid))) {
            s2 = stest;
            break;
        }
    }
    if (!s2) {
	bu_vls_printf(gedp->ged_result_str, "View object %s does not exist\n", argv[0]);
	return GED_ERROR;
    }
    if (!(s2->s_type_flags & BVIEW_VIEWONLY) || !(s2->s_type_flags & BVIEW_POLYGONS)) {
	bu_vls_printf(gedp->ged_result_str, "%s is not a view polygon.\n", argv[0]);
	return GED_ERROR;
    }

    // Have two polygons.  Check for overlaps, using the origin view of the
    // obj1 polygon.
    struct bview_polygon *polyA = (struct bview_polygon *)s->s_i_data;
    struct bview_polygon *polyB = (struct bview_polygon *)s2->s_i_data;

    int ovlp = bg_polygons_overlap(&polyA->polygon, &polyB->polygon, polyA->v.gv_model2view, &gedp->ged_wdbp->wdb_tol, polyA->v.gv_scale);

    bu_vls_printf(gedp->ged_result_str, "%d", ovlp);

    return GED_OK;
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
	return GED_OK;

    argc--; argv++;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    struct bview_scene_obj *s = gd->s;
    if (s) {
	bu_vls_printf(gedp->ged_result_str, "View object named %s already exists\n", gd->vobj);
	return GED_ERROR;
    }

    if (!argc) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s\n", usage_string);
	return GED_ERROR;
    }

    // Begin import
    size_t ncontours = 0;
    struct rt_db_internal intern;
    struct rt_sketch_internal *sketch_ip;
    struct bu_list HeadSegmentNodes;
    struct bu_list HeadContourNodes;
    struct segment_node *all_segment_nodes;
    struct segment_node *curr_snode;
    struct contour_node *curr_cnode;

    if (wdb_import_from_path(gedp->ged_result_str, &intern, argv[0], gedp->ged_wdbp) & GED_ERROR) {
	return GED_ERROR;
    }

    sketch_ip = (struct rt_sketch_internal *)intern.idb_ptr;
    if (sketch_ip->vert_count < 3 || sketch_ip->curve.count < 1) {
	rt_db_free_internal(&intern);
	return GED_ERROR;
    }

    // Have a sketch - create an empty polygon
    s = bview_create_polygon(gedp->ged_gvp, BVIEW_POLYGON_GENERAL, 0, 0);
    if (!s) {
	bu_vls_printf(gedp->ged_result_str, "Failed to create %s\n", gd->vobj);
	return GED_ERROR;
    }
    bu_vls_init(&s->s_uuid);
    bu_vls_printf(&s->s_uuid, "%s", gd->vobj);
    struct bview_polygon *p = (struct bview_polygon *)s->s_i_data;
    bg_polygon_free(&p->polygon);

    /* Start translating the sketch info into a polygon */
    all_segment_nodes = (struct segment_node *)bu_calloc(sketch_ip->curve.count, sizeof(struct segment_node), "all_segment_nodes");

    BU_LIST_INIT(&HeadSegmentNodes);
    BU_LIST_INIT(&HeadContourNodes);
    for (size_t n = 0; n < sketch_ip->curve.count; ++n) {
	all_segment_nodes[n].segment = sketch_ip->curve.segment[n];
	all_segment_nodes[n].reverse = sketch_ip->curve.reverse[n];
	BU_LIST_INSERT(&HeadSegmentNodes, &all_segment_nodes[n].l);
    }
    curr_cnode = (struct contour_node *)0;
    while (BU_LIST_NON_EMPTY(&HeadSegmentNodes)) {
	struct segment_node *unused_snode = BU_LIST_FIRST(segment_node, &HeadSegmentNodes);
	uint32_t *magic = (uint32_t *)unused_snode->segment;
	struct line_seg *unused_lsg;

	BU_LIST_DEQUEUE(&unused_snode->l);

	/* For the moment, skipping everything except line segments */
	if (*magic != CURVE_LSEG_MAGIC)
	    continue;

	unused_lsg = (struct line_seg *)unused_snode->segment;
	if (unused_snode->reverse) {
	    int tmp = unused_lsg->start;
	    unused_lsg->start = unused_lsg->end;
	    unused_lsg->end = tmp;
	}

	/* Find a contour to add the unused segment to. */
	for (BU_LIST_FOR(curr_cnode, contour_node, &HeadContourNodes)) {
	    for (BU_LIST_FOR(curr_snode, segment_node, &curr_cnode->head)) {
		struct line_seg *curr_lsg = (struct line_seg *)curr_snode->segment;

		if (unused_lsg->start == curr_lsg->end) {
		    unused_snode->used = 1;
		    BU_LIST_APPEND(&curr_snode->l, &unused_snode->l);
		    goto end;
		}

		if (unused_lsg->end == curr_lsg->start) {
		    unused_snode->used = 1;
		    BU_LIST_INSERT(&curr_snode->l, &unused_snode->l);
		    goto end;
		}
	    }
	}

end:
	if (!unused_snode->used) {
	    ++ncontours;
	    BU_ALLOC(curr_cnode, struct contour_node);
	    BU_LIST_INSERT(&HeadContourNodes, &curr_cnode->l);
	    BU_LIST_INIT(&curr_cnode->head);
	    BU_LIST_INSERT(&curr_cnode->head, &unused_snode->l);
	}
    }

    p->polygon.num_contours = ncontours;
    p->polygon.hole = (int *)bu_calloc(ncontours, sizeof(int), "gp_hole");
    p->polygon.contour = (struct bg_poly_contour *)bu_calloc(ncontours, sizeof(struct bg_poly_contour), "gp_contour");

    size_t j = 0;
    fastf_t dmax = 0.0;
    while (BU_LIST_NON_EMPTY(&HeadContourNodes)) {
	size_t k = 0;
	size_t npoints = 0;
	struct line_seg *curr_lsg = NULL;

	curr_cnode = BU_LIST_FIRST(contour_node, &HeadContourNodes);
	BU_LIST_DEQUEUE(&curr_cnode->l);

	/* Count the number of segments in this contour */
	for (BU_LIST_FOR(curr_snode, segment_node, &curr_cnode->head))
	    ++npoints;

	p->polygon.contour[j].num_points = npoints;
	p->polygon.contour[j].open = 0;
	p->polygon.contour[j].point = (point_t *)bu_calloc(npoints, sizeof(point_t), "gpc_point");

	while (BU_LIST_NON_EMPTY(&curr_cnode->head)) {
	    curr_snode = BU_LIST_FIRST(segment_node, &curr_cnode->head);
	    BU_LIST_DEQUEUE(&curr_snode->l);

	    curr_lsg = (struct line_seg *)curr_snode->segment;

	    /* Convert from UV space to model space */
	    VJOIN2(p->polygon.contour[j].point[k], sketch_ip->V,
		    sketch_ip->verts[curr_lsg->start][0], sketch_ip->u_vec,
		    sketch_ip->verts[curr_lsg->start][1], sketch_ip->v_vec);
	    fastf_t dtmp = DIST_PNT_PNT(sketch_ip->V, p->polygon.contour[j].point[k]);
	    if (dtmp > dmax)
		dmax = dtmp;
	    ++k;
	}

	/* free contour node */
	bu_free((void *)curr_cnode, "curr_cnode");

	++j;
    }

    /* Construct an appropriate bview from the sketch's
     * 3D info so we can snap to it. autoview, then dir.
     * TODO - this needs improvement... */
    struct bview *v = &p->v;
    bview_init(v);
    vect_t center = VINIT_ZERO;
    vect_t min, max;
    VSETALL(min, -dmax);
    VSETALL(max, dmax);
    vect_t radial;
    VADD2SCALE(center, max, min, 0.5);
    VSUB2(radial, max, center);
    if (VNEAR_ZERO(radial, SQRT_SMALL_FASTF))
          VSETALL(radial, 1.0);
    MAT_IDN(v->gv_center);
    MAT_DELTAS_VEC_NEG(v->gv_center, center);
    v->gv_scale = radial[X];
    V_MAX(v->gv_scale, radial[Y]);
    V_MAX(v->gv_scale, radial[Z]);
    v->gv_size = 2.0 * v->gv_scale;
    v->gv_isize = 1.0 / v->gv_size;
    bview_update(v);

    vect_t snorm;
    VCROSS(snorm, sketch_ip->u_vec, sketch_ip->v_vec);
    AZEL_FROM_V3DIR(p->v.gv_aet[0], p->v.gv_aet[1], snorm);
    _ged_mat_aet(&p->v);
    bview_update(&p->v);

    /* Clean up */
    bu_free((void *)all_segment_nodes, "all_segment_nodes");
    rt_db_free_internal(&intern);

    /* Have new polygon, now update view object vlist */
    bview_update_polygon(s);

    /* Done - add to scene objects */
    bu_ptbl_ins(gedp->ged_gvp->gv_scene_objs, (long *)s);

    return GED_OK;
}

int
_poly_cmd_export(void *bs, int argc, const char **argv)
{
    struct _ged_view_info *gd = (struct _ged_view_info *)bs;
    struct ged *gedp = gd->gedp;
    const char *usage_string = "view obj <objname> polygon export <sketchname>";
    const char *purpose_string = "export polygon to sketch";
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

    if (!argc) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s\n", usage_string);
	return GED_ERROR;
    }

    GED_CHECK_EXISTS(gedp, argv[0], LOOKUP_QUIET, GED_ERROR);

    size_t num_verts = 0;
    struct rt_db_internal internal;
    struct rt_sketch_internal *sketch_ip;
    struct line_seg *lsg;
    struct directory *dp;
    vect_t view;
    point_t vorigin;
    mat_t invRot;

    struct bview_polygon *p = (struct bview_polygon *)s->s_i_data;
    for (size_t j = 0; j < p->polygon.num_contours; ++j)
	num_verts += p->polygon.contour[j].num_points;

    if (num_verts < 3) {
	bu_vls_printf(gedp->ged_result_str, "Polygon is degenerate, not exporting.\n");
	return GED_ERROR;
    }

    RT_DB_INTERNAL_INIT(&internal);
    internal.idb_major_type = DB5_MAJORTYPE_BRLCAD;
    internal.idb_type = ID_SKETCH;
    internal.idb_meth = &OBJ[ID_SKETCH];

    BU_ALLOC(internal.idb_ptr, struct rt_sketch_internal);
    sketch_ip = (struct rt_sketch_internal *)internal.idb_ptr;
    sketch_ip->magic = RT_SKETCH_INTERNAL_MAGIC;
    sketch_ip->vert_count = num_verts;
    sketch_ip->verts = (point2d_t *)bu_calloc(sketch_ip->vert_count, sizeof(point2d_t), "sketch_ip->verts");
    sketch_ip->curve.count = num_verts;
    sketch_ip->curve.reverse = (int *)bu_calloc(sketch_ip->curve.count, sizeof(int), "sketch_ip->curve.reverse");
    sketch_ip->curve.segment = (void **)bu_calloc(sketch_ip->curve.count, sizeof(void *), "sketch_ip->curve.segment");

    bn_mat_inv(invRot, p->v.gv_rotation);
    VSET(view, 1.0, 0.0, 0.0);
    MAT4X3PNT(sketch_ip->u_vec, invRot, view);

    VSET(view, 0.0, 1.0, 0.0);
    MAT4X3PNT(sketch_ip->v_vec, invRot, view);

    /* should already be unit vectors */
    VUNITIZE(sketch_ip->u_vec);
    VUNITIZE(sketch_ip->v_vec);

    /* Project the origin onto the front of the viewing cube */
    MAT4X3PNT(vorigin, p->v.gv_model2view, p->v.gv_center);
    vorigin[Z] = p->v.gv_data_vZ;

    /* Convert back to model coordinates for storage */
    MAT4X3PNT(sketch_ip->V, p->v.gv_view2model, vorigin);

    int n = 0;
    for (size_t j = 0; j < p->polygon.num_contours; ++j) {
	size_t cstart = n;
	size_t k = 0;
	for (k = 0; k < p->polygon.contour[j].num_points; ++k) {
	    point_t vpt;
	    vect_t vdiff;

	    MAT4X3PNT(vpt, p->v.gv_model2view, p->polygon.contour[j].point[k]);
	    VSUB2(vdiff, vpt, vorigin);
	    VSCALE(vdiff, vdiff, p->v.gv_scale);
	    V2MOVE(sketch_ip->verts[n], vdiff);

	    if (k) {
		BU_ALLOC(lsg, struct line_seg);
		sketch_ip->curve.segment[n-1] = (void *)lsg;
		lsg->magic = CURVE_LSEG_MAGIC;
		lsg->start = n-1;
		lsg->end = n;
	    }

	    ++n;
	}

	if (k) {
	    BU_ALLOC(lsg, struct line_seg);
	    sketch_ip->curve.segment[n-1] = (void *)lsg;
	    lsg->magic = CURVE_LSEG_MAGIC;
	    lsg->start = n-1;
	    lsg->end = cstart;
	}
    }


    GED_DB_DIRADD(gedp, dp, argv[0], RT_DIR_PHONY_ADDR, 0, RT_DIR_SOLID, (void *)&internal.idb_type, GED_ERROR);
    GED_DB_PUT_INTERNAL(gedp, dp, &internal, &rt_uniresource, GED_ERROR);


    return GED_OK;
}

int
_poly_cmd_fill(void *bs, int argc, const char **argv)
{
    struct _ged_view_info *gd = (struct _ged_view_info *)bs;
    struct ged *gedp = gd->gedp;
    const char *usage_string = "view obj <obj1> polygon fill [dx dy spacing]";
    const char *purpose_string = "use lines to visualize polygon interior";
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

    if (argc == 1 && BU_STR_EQUAL(argv[0], "0")) {
	struct bview_polygon *p = (struct bview_polygon *)s->s_i_data;
	p->fill_flag = 0;
	bview_update_polygon(s);
	return GED_OK;
    }

    if (argc != 3) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s\n", usage_string);
	return GED_ERROR;
    }
    vect2d_t vdir;
    fastf_t vdelta;
    if (bu_opt_fastf_t(NULL, 1, (const char **)&argv[0], (void *)&vdir[0]) != 1) {
	bu_vls_printf(gedp->ged_result_str, "Invalid argument %s\n", argv[0]);
	return GED_ERROR;
    }
    if (bu_opt_fastf_t(NULL, 1, (const char **)&argv[0], (void *)&vdir[1]) != 1) {
	bu_vls_printf(gedp->ged_result_str, "Invalid argument %s\n", argv[1]);
	return GED_ERROR;
    }
    if (bu_opt_fastf_t(NULL, 1, (const char **)&argv[0], (void *)&vdelta) != 1) {
	bu_vls_printf(gedp->ged_result_str, "Invalid argument %s\n", argv[2]);
	return GED_ERROR;
    }

    struct bview_polygon *p = (struct bview_polygon *)s->s_i_data;
    p->fill_flag = 1;
    V2MOVE(p->fill_dir, vdir);
    p->fill_delta = vdelta;
    bview_update_polygon(s);

    return GED_OK;
}

int
_poly_cmd_csg(void *bs, int argc, const char **argv)
{
    struct _ged_view_info *gd = (struct _ged_view_info *)bs;
    struct ged *gedp = gd->gedp;
    const char *usage_string = "view obj <obj1> polygon csg <u|-|+> <obj2>";
    const char *purpose_string = "replace obj1's polygon with the result of obj2 u/-/+ obj1";
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
    struct bview_scene_obj *s2 = NULL;
    for (size_t i = 0; i < BU_PTBL_LEN(v->gv_scene_objs); i++) {
        struct bview_scene_obj *stest = (struct bview_scene_obj *)BU_PTBL_GET(v->gv_scene_objs, i);
        if (BU_STR_EQUAL(argv[1], bu_vls_cstr(&stest->s_uuid))) {
            s2 = stest;
            break;
        }
    }
    if (!s2) {
	bu_vls_printf(gedp->ged_result_str, "View object %s does not exist\n", argv[0]);
	return GED_ERROR;
    }
    if (!(s2->s_type_flags & BVIEW_VIEWONLY) || !(s2->s_type_flags & BVIEW_POLYGONS)) {
	bu_vls_printf(gedp->ged_result_str, "%s is not a view polygon.\n", argv[0]);
	return GED_ERROR;
    }

    // Have two polygons.  Check for overlaps, using the origin view of the
    // obj1 polygon.
    struct bview_polygon *polyA = (struct bview_polygon *)s->s_i_data;
    struct bview_polygon *polyB = (struct bview_polygon *)s2->s_i_data;

    struct bg_polygon *cp = bg_clip_polygon(op, &polyA->polygon, &polyB->polygon, CLIPPER_MAX, polyA->v.gv_model2view, polyA->v.gv_view2model);

    if (!cp)
	return GED_ERROR;

    bg_polygon_free(&polyA->polygon);
    polyA->polygon.num_contours = cp->num_contours;
    polyA->polygon.hole = cp->hole;
    polyA->polygon.contour = cp->contour;

    // clipper results are always general polygons
    polyA->type = BVIEW_POLYGON_GENERAL;

    BU_PUT(cp, struct bg_polygon);
    bview_update_polygon(s);

    return GED_OK;
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
    { "import",          _poly_cmd_import},
    { "move",            _poly_cmd_move},
    { "open",            _poly_cmd_open},
    { "overlap",         _poly_cmd_overlap},
    { "select",          _poly_cmd_select},
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

    if (help) {
	_ged_cmd_help(gedp, usage_string, d);
	return GED_HELP;
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
