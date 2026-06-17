/*                      P O L Y G O N S . C
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
#include "bsg.h"
#include "bsg/polygon.h"
#include "bg/polygon.h"
#include "rt/geom.h"
#include "rt/primitives/sketch.h"

#include "../ged_private.h"
#include "./ged_view.h"


static bsg_polygon_ref
_poly_ref(struct _ged_view_info *gd)
{
    if (!gd)
	return (bsg_polygon_ref)BSG_POLYGON_REF_NULL_INIT;
    if (bsg_polygon_ref_is_null(gd->polygon_ref))
	gd->polygon_ref = bsg_view_polygon_find_scoped_ref(gd->cv, gd->vobj, gd->local_obj);
    return gd->polygon_ref;
}

static int
_poly_record(struct _ged_view_info *gd, struct bsg_polygon_record *record)
{
    return bsg_polygon_record_get(_poly_ref(gd), record);
}

static int
_poly_exists(struct _ged_view_info *gd)
{
    return !bsg_polygon_ref_is_null(_poly_ref(gd));
}

static void
_poly_update(struct _ged_view_info *gd, int op)
{
    (void)bsg_polygon_update(_poly_ref(gd), gd ? gd->cv : NULL, op);
}

static int
_poly_update_screen(struct _ged_view_info *gd, int x, int y, int op)
{
    return bsg_polygon_update_screen_pt(_poly_ref(gd), gd ? gd->cv : NULL, x, y, op) ? BRLCAD_OK : BRLCAD_ERROR;
}

int
_poly_cmd_create(void *bs, int argc, const char **argv)
{
    struct _ged_view_info *gd = (struct _ged_view_info *)bs;
    struct ged *gedp = gd->gedp;
    const char *usage_string = "view obj create <objname> polygon create x y [circ|ell|rect|sq]";
    const char *purpose_string = "create polygon";
    if (_view_cmd_msgs(bs, argc, argv, usage_string, purpose_string))
	return BRLCAD_OK;

    argc--; argv++;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    if (_poly_exists(gd)) {
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

    point_t sp;
    if (bsg_screen_pt(&sp, (fastf_t)x, (fastf_t)y, gd->cv)) {
	bu_vls_printf(gedp->ged_result_str, "Failed to calculate screen point\n");
	return BRLCAD_ERROR;
    }

    int type = BSG_POLYGON_GENERAL;
    if (argc == 3) {
	if (BU_STR_EQUAL(argv[2], "circ") || BU_STR_EQUAL(argv[2], "circle"))
	    type = BSG_POLYGON_CIRCLE;
	if (BU_STR_EQUAL(argv[2], "ell") || BU_STR_EQUAL(argv[2], "ellipse"))
	    type = BSG_POLYGON_ELLIPSE;
	if (BU_STR_EQUAL(argv[2], "rect") || BU_STR_EQUAL(argv[2], "rectangle"))
	    type = BSG_POLYGON_RECTANGLE;
	if (BU_STR_EQUAL(argv[2], "sq") || BU_STR_EQUAL(argv[2], "square"))
	    type = BSG_POLYGON_SQUARE;
	if (type == BSG_POLYGON_GENERAL) {
	    bu_vls_printf(gedp->ged_result_str, "Unknown polygon type %s\n", argv[2]);
	    return BRLCAD_ERROR;
	}
    }

    gd->polygon_ref = bsg_create_polygon_ref(gd->cv, gd->vobj, gd->local_obj, type, &sp);
    if (bsg_polygon_ref_is_null(gd->polygon_ref)) {
	bu_vls_printf(gedp->ged_result_str, "Failed to create %s\n", gd->vobj);
	return BRLCAD_ERROR;
    }

    return BRLCAD_OK;
}

int
_poly_cmd_select(void *bs, int argc, const char **argv)
{
    struct _ged_view_info *gd = (struct _ged_view_info *)bs;
    struct ged *gedp = gd->gedp;
    const char *usage_string = "view obj create <objname> polygon select [contour] x y";
    const char *purpose_string = "select polygon point closest to point x,y";
    if (_view_cmd_msgs(bs, argc, argv, usage_string, purpose_string))
	return BRLCAD_OK;

    argc--; argv++;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    if (!_poly_exists(gd)) {
	bu_vls_printf(gedp->ged_result_str, "View object %s does not exist\n", gd->vobj);
	return BRLCAD_ERROR;
    }
    struct bsg_polygon_record rec;
    if (!_poly_record(gd, &rec)) {
	bu_vls_printf(gedp->ged_result_str, "Specified object is not a view polygon.\n");
	return BRLCAD_ERROR;
    }

    if (rec.type != BSG_POLYGON_GENERAL) {
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
    } else {
	contour_ind = 0;
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

    if (!bsg_polygon_set_current(_poly_ref(gd), contour_ind, -1))
	return BRLCAD_ERROR;
    if (_poly_update_screen(gd, x, y, BSG_POLYGON_UPDATE_PT_SELECT) != BRLCAD_OK)
	return BRLCAD_ERROR;

    return BRLCAD_OK;
}


int
_poly_cmd_append(void *bs, int argc, const char **argv)
{
    struct _ged_view_info *gd = (struct _ged_view_info *)bs;
    struct ged *gedp = gd->gedp;
    const char *usage_string = "view obj create <objname> polygon append [contour] x y";
    const char *purpose_string = "append point to polygon";
    if (_view_cmd_msgs(bs, argc, argv, usage_string, purpose_string))
	return BRLCAD_OK;

    argc--; argv++;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    if (!_poly_exists(gd)) {
	bu_vls_printf(gedp->ged_result_str, "View object %s does not exist\n", gd->vobj);
	return BRLCAD_ERROR;
    }
    struct bsg_polygon_record rec;
    if (!_poly_record(gd, &rec)) {
	bu_vls_printf(gedp->ged_result_str, "Specified object is not a view polygon.\n");
	return BRLCAD_ERROR;
    }

    if (rec.type != BSG_POLYGON_GENERAL) {
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
    } else {
	contour_ind = 0;
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

    if (!bsg_polygon_set_current(_poly_ref(gd), contour_ind, -1))
	return BRLCAD_ERROR;
    if (_poly_update_screen(gd, x, y, BSG_POLYGON_UPDATE_PT_APPEND) != BRLCAD_OK)
	return BRLCAD_ERROR;

    return BRLCAD_OK;
}

int
_poly_cmd_move(void *bs, int argc, const char **argv)
{
    struct _ged_view_info *gd = (struct _ged_view_info *)bs;
    struct ged *gedp = gd->gedp;
    const char *usage_string = "view obj create <objname> polygon move x y";
    const char *purpose_string = "move selected polygon point to x,y";
    if (_view_cmd_msgs(bs, argc, argv, usage_string, purpose_string))
	return BRLCAD_OK;

    argc--; argv++;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    if (!_poly_exists(gd)) {
	bu_vls_printf(gedp->ged_result_str, "View object %s does not exist\n", gd->vobj);
	return BRLCAD_ERROR;
    }
    struct bsg_polygon_record rec;
    if (!_poly_record(gd, &rec)) {
	bu_vls_printf(gedp->ged_result_str, "Specified object is not a view polygon.\n");
	return BRLCAD_ERROR;
    }

    if (rec.type != BSG_POLYGON_GENERAL) {
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

    if (_poly_update_screen(gd, x, y, BSG_POLYGON_UPDATE_PT_MOVE) != BRLCAD_OK)
	return BRLCAD_ERROR;

    return BRLCAD_OK;
}

int
_poly_cmd_clear(void *bs, int argc, const char **argv)
{
    struct _ged_view_info *gd = (struct _ged_view_info *)bs;
    struct ged *gedp = gd->gedp;
    const char *usage_string = "view obj create <objname> polygon clear";
    const char *purpose_string = "clear all modification flags";
    if (_view_cmd_msgs(bs, argc, argv, usage_string, purpose_string))
	return BRLCAD_OK;

    argc--; argv++;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    if (!_poly_exists(gd)) {
	bu_vls_printf(gedp->ged_result_str, "View object %s does not exist\n", gd->vobj);
	return BRLCAD_ERROR;
    }
    if (!bsg_polygon_set_current(_poly_ref(gd), 0, -1)) {
	bu_vls_printf(gedp->ged_result_str, "Specified object is not a view polygon.\n");
	return BRLCAD_ERROR;
    }

    _poly_update(gd, BSG_POLYGON_UPDATE_DEFAULT);

    return BRLCAD_OK;
}

int
_poly_cmd_close(void *bs, int argc, const char **argv)
{
    struct _ged_view_info *gd = (struct _ged_view_info *)bs;
    struct ged *gedp = gd->gedp;
    const char *usage_string = "view obj create <objname> polygon close [ind]";
    const char *purpose_string = "contour -> polygon";
    if (_view_cmd_msgs(bs, argc, argv, usage_string, purpose_string))
	return BRLCAD_OK;

    argc--; argv++;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    if (!_poly_exists(gd)) {
	bu_vls_printf(gedp->ged_result_str, "View object %s does not exist\n", gd->vobj);
	return BRLCAD_ERROR;
    }
    struct bsg_polygon_record rec;
    if (!_poly_record(gd, &rec)) {
	bu_vls_printf(gedp->ged_result_str, "Specified object is not a view polygon.\n");
	return BRLCAD_ERROR;
    }

    if (rec.type != BSG_POLYGON_GENERAL) {
	return BRLCAD_OK;
    }

    int ind = -1;
    if (argc == 1) {
	if (bu_opt_int(NULL, 1, (const char **)&argv[0], (void *)&ind) != 1) {
	    bu_vls_printf(gedp->ged_result_str, "Invalid argument %s\n", argv[0]);
	    return BRLCAD_ERROR;
	}
	if (ind < 0 || ind >= (int)rec.contour_count) {
	    bu_vls_printf(gedp->ged_result_str, "Invalid argument %s\n", argv[0]);
	    return BRLCAD_ERROR;
	}
    } else if (argc > 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s\n", usage_string);
	return BRLCAD_ERROR;
    }

    if (ind < 0) {
	if (!bsg_polygon_set_all_contours_open(_poly_ref(gd), 0))
	    return BRLCAD_ERROR;
    } else if (!bsg_polygon_set_contour_open(_poly_ref(gd), ind, 0)) {
	return BRLCAD_ERROR;
    }

    return BRLCAD_OK;
}

int
_poly_cmd_open(void *bs, int argc, const char **argv)
{
    struct _ged_view_info *gd = (struct _ged_view_info *)bs;
    struct ged *gedp = gd->gedp;
    const char *usage_string = "view obj create <objname> polygon open [ind]";
    const char *purpose_string = "polygon -> contour";
    if (_view_cmd_msgs(bs, argc, argv, usage_string, purpose_string))
	return BRLCAD_OK;

    argc--; argv++;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    if (!_poly_exists(gd)) {
	bu_vls_printf(gedp->ged_result_str, "View object %s does not exist\n", gd->vobj);
	return BRLCAD_ERROR;
    }
    struct bsg_polygon_record rec;
    if (!_poly_record(gd, &rec)) {
	bu_vls_printf(gedp->ged_result_str, "Specified object is not a view polygon.\n");
	return BRLCAD_ERROR;
    }

    if (rec.type != BSG_POLYGON_GENERAL) {
	bu_vls_printf(gedp->ged_result_str, "Constrained polygon shapes are always closed.\n");
	return BRLCAD_ERROR;
    }

    int ind = -1;
    if (argc == 1) {
	if (bu_opt_int(NULL, 1, (const char **)&argv[0], (void *)&ind) != 1) {
	    bu_vls_printf(gedp->ged_result_str, "Invalid argument %s\n", argv[0]);
	    return BRLCAD_ERROR;
	}
	if (ind < 0 || ind >= (int)rec.contour_count) {
	    bu_vls_printf(gedp->ged_result_str, "Invalid argument %s\n", argv[0]);
	    return BRLCAD_ERROR;
	}
    } else if (argc > 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s\n", usage_string);
	return BRLCAD_ERROR;
    }

    if (ind < 0) {
	if (!bsg_polygon_set_all_contours_open(_poly_ref(gd), 1))
	    return BRLCAD_ERROR;
    } else if (!bsg_polygon_set_contour_open(_poly_ref(gd), ind, 1)) {
	return BRLCAD_ERROR;
    }

    return BRLCAD_OK;
}

int
_poly_cmd_area(void *bs, int argc, const char **argv)
{
    struct _ged_view_info *gd = (struct _ged_view_info *)bs;
    struct ged *gedp = gd->gedp;
    const char *usage_string = "view obj create <objname> polygon area";
    const char *purpose_string = "report polygon area";
    if (_view_cmd_msgs(bs, argc, argv, usage_string, purpose_string))
	return BRLCAD_OK;

    argc--; argv++;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    if (!_poly_exists(gd)) {
	bu_vls_printf(gedp->ged_result_str, "View object %s does not exist\n", gd->vobj);
	return BRLCAD_ERROR;
    }
    const struct bsg_polygon *p = bsg_polygon_data(_poly_ref(gd));
    if (!p) {
	bu_vls_printf(gedp->ged_result_str, "Specified object is not a view polygon.\n");
	return BRLCAD_ERROR;
    }

    double area = bg_find_polygon_area((struct bg_polygon *)&p->polygon, CLIPPER_MAX, (plane_t *)&p->vp, gd->cv ? gd->cv->gv_scale : 1.0);

    if (gedp->dbip) {
	bu_vls_printf(gedp->ged_result_str, "%g", area * gedp->dbip->dbi_base2local);
    } else {
	bu_vls_printf(gedp->ged_result_str, "%g", area);
    }
    return BRLCAD_OK;
}

int
_poly_cmd_overlap(void *bs, int argc, const char **argv)
{
    struct _ged_view_info *gd = (struct _ged_view_info *)bs;
    struct ged *gedp = gd->gedp;
    struct rt_wdb *wdbp = wdb_dbopen(gedp->dbip, RT_WDB_TYPE_DB_DEFAULT);
    const char *usage_string = "view obj create <obj1> polygon overlap <obj2>";
    const char *purpose_string = "report if two polygons overlap";
    if (_view_cmd_msgs(bs, argc, argv, usage_string, purpose_string))
	return BRLCAD_OK;

    argc--; argv++;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    if (!_poly_exists(gd)) {
	bu_vls_printf(gedp->ged_result_str, "View object %s does not exist\n", gd->vobj);
	return BRLCAD_ERROR;
    }
    if (!argc) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s\n", usage_string);
	return BRLCAD_ERROR;
    }

    struct bsg_view *v = gd->cv;
    bsg_polygon_ref other_ref = bsg_view_polygon_find_ref(v, argv[0]);
    if (bsg_polygon_ref_is_null(other_ref)) {
	bu_vls_printf(gedp->ged_result_str, "View object %s does not exist\n", argv[0]);
	return BRLCAD_ERROR;
    }

    // Have two polygons.  Check for overlaps, using the origin plane of the
    // obj1 polygon.
    const struct bsg_polygon *polyA = bsg_polygon_data(_poly_ref(gd));
    const struct bsg_polygon *polyB = bsg_polygon_data(other_ref);
    if (!polyA || !polyB) {
	bu_vls_printf(gedp->ged_result_str, "%s is not a view polygon.\n", argv[0]);
	return BRLCAD_ERROR;
    }

    int ovlp = bg_polygons_overlap((struct bg_polygon *)&polyA->polygon, (struct bg_polygon *)&polyB->polygon, (plane_t *)&polyA->vp, &wdbp->wdb_tol, v->gv_scale);

    bu_vls_printf(gedp->ged_result_str, "%d", ovlp);

    return BRLCAD_OK;
}

int
_poly_cmd_import(void *bs, int argc, const char **argv)
{
    struct _ged_view_info *gd = (struct _ged_view_info *)bs;
    struct ged *gedp = gd->gedp;
    const char *usage_string = "view obj create <objname> polygon import <sketchname>";
    const char *purpose_string = "import polygon from sketch";
    if (_view_cmd_msgs(bs, argc, argv, usage_string, purpose_string))
	return BRLCAD_OK;

    argc--; argv++;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    if (_poly_exists(gd)) {
	bu_vls_printf(gedp->ged_result_str, "View object named %s already exists\n", gd->vobj);
	return BRLCAD_ERROR;
    }

    if (!argc) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s\n", usage_string);
	return BRLCAD_ERROR;
    }

    if (!gedp->dbip) {
	bu_vls_printf(gedp->ged_result_str, "no database open\n");
	return BRLCAD_ERROR;
    }

    // Begin import
    struct directory *dp = db_lookup(gedp->dbip, argv[0], LOOKUP_QUIET);
    if (dp == RT_DIR_NULL) {
	return BRLCAD_ERROR;
    }

    gd->polygon_ref = db_sketch_to_view_polygon_scoped_ref(gd->vobj, gedp->dbip, dp, gd->cv, gd->local_obj);
    if (bsg_polygon_ref_is_null(gd->polygon_ref)) {
	bu_vls_printf(gedp->ged_result_str, "Failed to create %s\n", gd->vobj);
	return BRLCAD_ERROR;
    }

    return BRLCAD_OK;
}

int
_poly_cmd_export(void *bs, int argc, const char **argv)
{
    struct _ged_view_info *gd = (struct _ged_view_info *)bs;
    struct ged *gedp = gd->gedp;
    const char *usage_string = "view obj create <objname> polygon export <sketchname>";
    const char *purpose_string = "export polygon to sketch";
    if (_view_cmd_msgs(bs, argc, argv, usage_string, purpose_string))
	return BRLCAD_OK;

    argc--; argv++;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    if (!_poly_exists(gd)) {
	bu_vls_printf(gedp->ged_result_str, "View object %s does not exist\n", gd->vobj);
	return BRLCAD_ERROR;
    }
    if (!bsg_polygon_data(_poly_ref(gd))) {
	bu_vls_printf(gedp->ged_result_str, "Specified object is not a view polygon.\n");
	return BRLCAD_ERROR;
    }

    if (!argc) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s\n", usage_string);
	return BRLCAD_ERROR;
    }

    if (!gedp->dbip) {
	bu_vls_printf(gedp->ged_result_str, "no database open\n");
	return BRLCAD_ERROR;
    }

    GED_CHECK_EXISTS(gedp, argv[0], LOOKUP_QUIET, BRLCAD_ERROR);

    if (db_view_polygon_ref_to_sketch(gedp->dbip, argv[0], _poly_ref(gd)) == RT_DIR_NULL) {
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
    const char *usage_string = "view obj create <obj1> polygon fill [dx dy spacing]";
    const char *purpose_string = "use lines to visualize polygon interior";
    if (_view_cmd_msgs(bs, argc, argv, usage_string, purpose_string))
	return BRLCAD_OK;

    argc--; argv++;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    if (!_poly_exists(gd)) {
	bu_vls_printf(gedp->ged_result_str, "View object %s does not exist\n", gd->vobj);
	return BRLCAD_ERROR;
    }
    struct bsg_polygon_record rec;
    if (!_poly_record(gd, &rec)) {
	bu_vls_printf(gedp->ged_result_str, "Specified object is not a view polygon.\n");
	return BRLCAD_ERROR;
    }

    if (argc == 1 && BU_STR_EQUAL(argv[0], "0")) {
	if (!bsg_polygon_set_fill(_poly_ref(gd), 0, rec.fill_dir[0], rec.fill_dir[1], rec.fill_delta))
	    return BRLCAD_ERROR;
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

    if (!bsg_polygon_set_fill(_poly_ref(gd), 1, vdir[0], vdir[1], vdelta))
	return BRLCAD_ERROR;

    return BRLCAD_OK;
}

int
_poly_cmd_fill_color(void *bs, int argc, const char **argv)
{
    struct _ged_view_info *gd = (struct _ged_view_info *)bs;
    struct ged *gedp = gd->gedp;
    const char *usage_string = "view obj create <obj1> polygon fill_color [r/g/b]";
    const char *purpose_string = "customize fill lines color (if fill is enabled)";
    if (_view_cmd_msgs(bs, argc, argv, usage_string, purpose_string))
	return BRLCAD_OK;

    argc--; argv++;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    if (!_poly_exists(gd)) {
	bu_vls_printf(gedp->ged_result_str, "View object %s does not exist\n", gd->vobj);
	return BRLCAD_ERROR;
    }
    if (!bsg_polygon_data(_poly_ref(gd))) {
	bu_vls_printf(gedp->ged_result_str, "Specified object is not a view polygon.\n");
	return BRLCAD_ERROR;
    }

    if (!argc) {
	unsigned char frgb[3];
	struct bu_color fill_color;
	if (!bsg_polygon_fill_color_get(_poly_ref(gd), &fill_color))
	    return BRLCAD_ERROR;
	bu_color_to_rgb_chars(&fill_color, (unsigned char *)frgb);

	bu_vls_printf(gedp->ged_result_str, "%d/%d/%d\n", frgb[0], frgb[1], frgb[2]);

	return BRLCAD_OK;
    }

    struct bu_color fill_color;
    if (bu_opt_color(NULL, 1, (const char **)&argv[0], (void *)&fill_color) != 1) {
	bu_vls_printf(gedp->ged_result_str, "Invalid argument %s\n", argv[0]);
	return BRLCAD_ERROR;
    }

    if (!bsg_polygon_fill_color_set(_poly_ref(gd), &fill_color))
	return BRLCAD_ERROR;

    return BRLCAD_OK;
}

int
_poly_cmd_csg(void *bs, int argc, const char **argv)
{
    struct _ged_view_info *gd = (struct _ged_view_info *)bs;
    struct ged *gedp = gd->gedp;
    const char *usage_string = "view obj create <obj1> polygon csg <u|-|+> <obj2>";
    const char *purpose_string = "replace obj1's polygon with the result of obj2 u/-/+ obj1";
    if (_view_cmd_msgs(bs, argc, argv, usage_string, purpose_string))
	return BRLCAD_OK;

    argc--; argv++;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    if (!_poly_exists(gd)) {
	bu_vls_printf(gedp->ged_result_str, "View object %s does not exist\n", gd->vobj);
	return BRLCAD_ERROR;
    }
    if (!bsg_polygon_data(_poly_ref(gd))) {
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

    struct bsg_view *v = gd->cv;
    bsg_polygon_ref other_ref = bsg_view_polygon_find_ref(v, argv[1]);
    if (bsg_polygon_ref_is_null(other_ref)) {
	bu_vls_printf(gedp->ged_result_str, "View object %s does not exist\n", argv[1]);
	return BRLCAD_ERROR;
    }

    bsg_polygon_ref target_ref = _poly_ref(gd);
    if (bsg_polygon_ref_is_null(target_ref) || !bsg_polygon_csg_ref(target_ref, other_ref, op))
	return BRLCAD_ERROR;

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
    { (char *)NULL,      NULL}
};

int
_view_cmd_polygons(void *bs, int argc, const char **argv)
{
    int help = 0;
    int s_version = 0;
    struct _ged_view_info *gd = (struct _ged_view_info *)bs;
    struct ged *gedp = gd->gedp;

    const char *usage_string = "view obj create <objname> polygon [options] [args]";
    const char *purpose_string = "manipulate view polygons";
    if (_view_cmd_msgs(bs, argc, argv, usage_string, purpose_string))
	return BRLCAD_OK;

    if (!gd->cv) {
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
