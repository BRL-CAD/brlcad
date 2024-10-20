/*                        P O L Y G O N . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2024 United States Government as represented by
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

/*----------------------------------------------------------------------*/
/* @file polygon.h */
/** @addtogroup bv_polygon */
/** @{ */

/**
 *  @brief Functions for working with polygons
 */

#ifndef BV_POLYGON_H
#define BV_POLYGON_H

#include "common.h"
#include "vmath.h"
#include "bu/color.h"
#include "bv/defines.h"
#include "bg/polygon.h"
#include "bg/polygon_types.h"

__BEGIN_DECLS

/* View polygon logic and types */

#define BV_POLYGON_GENERAL 0
#define BV_POLYGON_CIRCLE 1
#define BV_POLYGON_ELLIPSE 2
#define BV_POLYGON_RECTANGLE 3
#define BV_POLYGON_SQUARE 4

struct bv_polygon {
    int                 type;
    int                 fill_flag;         /* set to shade the interior */
    vect2d_t            fill_dir;
    fastf_t             fill_delta;
    struct bu_color     fill_color;
    long                curr_contour_i;
    long                curr_point_i;
    point_t             origin_point;      /* For non-general polygons  */

    /* We stash the view plane creation, so we know how to return
     * to it for future 2D alterations */
    plane_t             vp;

    /* Offset of polygon plane from the view plane.  Allows for moving
     * the polygon "towards" and "away from" the viewer. */
    fastf_t vZ;

    /* Actual polygon info */
    struct bg_polygon polygon;

    /* Arbitrary data */
    void *u_data;
};

// Given a polygon, create a scene object
BV_EXPORT extern struct bv_scene_obj *bv_create_polygon_obj(struct bview *v, int flags, struct bv_polygon *p);

// Creates a scene object with a default polygon
BV_EXPORT extern struct bv_scene_obj *bv_create_polygon(struct bview *v, int flags, int type, point_t *fp);

// Various update modes have similar logic - we pass in the flags to the update
// routine to enable/disable specific portions of the overall flow.
#define BV_POLYGON_UPDATE_DEFAULT 0
#define BV_POLYGON_UPDATE_PROPS_ONLY 1
#define BV_POLYGON_UPDATE_PT_SELECT 2
#define BV_POLYGON_UPDATE_PT_SELECT_CLEAR 3
#define BV_POLYGON_UPDATE_PT_MOVE 4
#define BV_POLYGON_UPDATE_PT_APPEND 5
BV_EXPORT extern int bv_update_polygon(struct bv_scene_obj *s, struct bview *v, int utype);

// Update just the scene obj vlist, without altering the source polygon
BV_EXPORT extern void bv_polygon_vlist(struct bv_scene_obj *s);

// Find the closest polygon obj to a point
BV_EXPORT extern struct bv_scene_obj *bv_select_polygon(struct bu_ptbl *objs, point_t *cp);

BV_EXPORT extern int bv_move_polygon(struct bv_scene_obj *s, point_t *cp, point_t *pp);
BV_EXPORT extern struct bv_scene_obj *bv_dup_view_polygon(const char *nname, struct bv_scene_obj *s);

// Copy a bv polygon.  Note that this also performs a
// view sync - if the user is copying the polygon into
// another view, they will have to update the output's
// bview to match their target view.
BV_EXPORT extern void bv_polygon_cpy(struct bv_polygon *dest , struct bv_polygon *src);

// Calculate a suggested default fill delta based on the polygon structure.  The
// idea is to try and strike a balance between line count and having enough fill
// lines to highlight interior holes.
BV_EXPORT extern int bv_polygon_calc_fdelta(struct bv_polygon *p);

BV_EXPORT extern struct bg_polygon *
bv_polygon_fill_segments(struct bg_polygon *poly, plane_t *vp, vect2d_t line_slope, fastf_t line_spacing);

// For all polygon bv_scene_objs in the objs table, apply the specified boolean
// op using p and replace the original polygon geometry in objs with the
// results (NOTE:  p will not act on itself if it is in objs):
//
// u : objs[i] u p  (unions p with objs[i])
// - : objs[i] - p  (removes p from objs[i])
// + : objs[i] + p  (intersects p with objs[i])
//
// At a data structure level, what happens is the bv_polygon geometry stored in
// the bv_polygon stored as the data entry for a bv_scene_obj is replaced.  The
// bv_scene_obj and bv_polygon pointers should remain valid, but the bv_polygon
// contained in bv_polygon is replaced - calling code should not rely on the
// bv_polygon pointer remaining the same after a boolean operation.
BV_EXPORT extern int bv_polygon_csg(struct bv_scene_obj *target, struct bv_scene_obj *stencil, bg_clip_t op);

__END_DECLS

#endif  /* BV_POLYGON_H */
/** @} */
/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
