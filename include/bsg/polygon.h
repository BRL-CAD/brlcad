/*                        P O L Y G O N . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2026 United States Government as represented by
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
/** @addtogroup bsg_polygon
 *
 *  @brief Functions for working with polygons.
 *
 *  Canonical home; bv/polygon.h is a backward-compatibility bridge.
 */
/* @file bsg/polygon.h */
/** @{ */

#ifndef BSG_POLYGON_H
#define BSG_POLYGON_H

#include "common.h"
#include "vmath.h"
#include "bu/color.h"
#include "bsg/defines.h"
#include "bsg/feature.h"
#include "bg/polygon.h"
#include "bg/polygon_types.h"

__BEGIN_DECLS

/* View polygon logic and types */

#define BSG_POLYGON_GENERAL   0
#define BSG_POLYGON_CIRCLE    1
#define BSG_POLYGON_ELLIPSE   2
#define BSG_POLYGON_RECTANGLE 3
#define BSG_POLYGON_SQUARE    4

struct bsg_polygon {
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

typedef bsg_feature_ref bsg_polygon_ref;

#define BSG_POLYGON_REF_NULL_INIT BSG_FEATURE_REF_NULL_INIT

struct bsg_polygon_record {
    bsg_polygon_ref ref;
    const char *name;
    int type;
    int fill_flag;
    vect2d_t fill_dir;
    fastf_t fill_delta;
    struct bu_color fill_color;
    unsigned char edge_color[3];
    long curr_contour_i;
    long curr_point_i;
    int first_contour_open;
    size_t contour_count;
    size_t point_count;
    point_t origin_point;
    plane_t vp;
    fastf_t vZ;
    void *user_data;
};

typedef int (*bsg_polygon_record_visit_cb)(bsg_polygon_ref ref, const struct bsg_polygon_record *record, void *data);

BSG_EXPORT extern int bsg_polygon_ref_is_null(bsg_polygon_ref ref);

BSG_EXPORT extern int bsg_polygon_record_get(bsg_polygon_ref ref, struct bsg_polygon_record *record);

BSG_EXPORT extern const struct bsg_polygon *bsg_polygon_data(bsg_polygon_ref ref);

/* Creates a view-owned polygon feature with explicit name and scope. */
BSG_EXPORT extern bsg_polygon_ref
bsg_create_polygon_ref(struct bsg_view *v, const char *name, int local, int type, point_t *fp);

/* Creates a view-owned polygon feature by copying existing polygon data. */
BSG_EXPORT extern bsg_polygon_ref
bsg_polygon_ref_create_from_data(struct bsg_view *v, const char *name, int local, const struct bsg_polygon *data);

/* Creates a view-owned polygon feature with a default polygon. */
BSG_EXPORT extern bsg_polygon_ref bsg_create_view_polygon_ref(struct bsg_view *v, int type, point_t *fp);

/* Various update modes have similar logic - we pass in the flags to the update
 * routine to enable/disable specific portions of the overall flow. */
#define BSG_POLYGON_UPDATE_DEFAULT         0
#define BSG_POLYGON_UPDATE_PROPS_ONLY      1
#define BSG_POLYGON_UPDATE_PT_SELECT       2
#define BSG_POLYGON_UPDATE_PT_SELECT_CLEAR 3
#define BSG_POLYGON_UPDATE_PT_MOVE         4
#define BSG_POLYGON_UPDATE_PT_APPEND       5
BSG_EXPORT extern int bsg_polygon_update(bsg_polygon_ref ref, struct bsg_view *v, int utype);

BSG_EXPORT extern int bsg_polygon_update_screen_pt(bsg_polygon_ref ref, struct bsg_view *v, int x, int y, int utype);

BSG_EXPORT extern bsg_polygon_ref bsg_view_select_polygon_ref(struct bsg_view *v, point_t *cp);

BSG_EXPORT extern bsg_polygon_ref bsg_view_polygon_find_ref(struct bsg_view *v, const char *name);

BSG_EXPORT extern bsg_polygon_ref bsg_view_polygon_find_scoped_ref(struct bsg_view *v, const char *name, int local_only);

BSG_EXPORT extern bsg_polygon_ref bsg_view_polygon_dup_ref(struct bsg_view *v, const char *name, const char *new_name);

BSG_EXPORT extern void bsg_view_polygon_visit_records(struct bsg_view *v, bsg_polygon_record_visit_cb cb, void *data);

BSG_EXPORT extern size_t bsg_view_polygon_snap_count(struct bsg_view *v, bsg_polygon_ref exclude);

/* Clear selected polygon points on every visible polygon. */
BSG_EXPORT extern int bsg_view_polygon_clear_point_selection(struct bsg_view *v);

BSG_EXPORT extern int bsg_polygon_move(bsg_polygon_ref ref, point_t *cp, point_t *pp);

BSG_EXPORT extern int bsg_polygon_set_current(bsg_polygon_ref ref, long contour_i, long point_i);

BSG_EXPORT extern int bsg_polygon_set_name(bsg_polygon_ref ref, const char *name);

BSG_EXPORT extern int bsg_polygon_set_view(bsg_polygon_ref ref, struct bsg_view *v);

BSG_EXPORT extern int bsg_polygon_set_visual(bsg_polygon_ref ref,
					     const struct bu_color *edge_color,
					     const struct bu_color *fill_color,
					     fastf_t fill_slope_x,
					     fastf_t fill_slope_y,
					     fastf_t fill_density,
					     fastf_t vZ,
					     int fill_flag);

BSG_EXPORT extern int bsg_polygon_set_open(bsg_polygon_ref ref, int open);

BSG_EXPORT extern int bsg_polygon_set_contour_open(bsg_polygon_ref ref, long contour_i, int open);

BSG_EXPORT extern int bsg_polygon_set_all_contours_open(bsg_polygon_ref ref, int open);

BSG_EXPORT extern int bsg_polygon_close(bsg_polygon_ref ref);

BSG_EXPORT extern int bsg_polygon_clear_selected_point(bsg_polygon_ref ref);

BSG_EXPORT extern int bsg_polygon_set_fill(bsg_polygon_ref ref, int fill_flag, fastf_t fill_slope_x, fastf_t fill_slope_y, fastf_t fill_density);

BSG_EXPORT extern int bsg_polygon_fill_color_get(bsg_polygon_ref ref, struct bu_color *fill_color);

BSG_EXPORT extern int bsg_polygon_fill_color_set(bsg_polygon_ref ref, const struct bu_color *fill_color);

BSG_EXPORT extern int bsg_polygon_remove(bsg_polygon_ref ref);

BSG_EXPORT extern void *bsg_polygon_user_data(bsg_polygon_ref ref);

BSG_EXPORT extern int bsg_polygon_user_data_set(bsg_polygon_ref ref, void *user_data);

/* Copy a bv polygon.  Note that this also performs a
 * view sync - if the user is copying the polygon into
 * another view, they will have to update the output's
 * bsg_view to match their target view. */
BSG_EXPORT extern void bsg_polygon_cpy(struct bsg_polygon *dest, const struct bsg_polygon *src);

/* Calculate a suggested default fill delta based on the polygon structure.  The
 * idea is to try and strike a balance between line count and having enough fill
 * lines to highlight interior holes. */
BSG_EXPORT extern int bsg_polygon_calc_fdelta(struct bsg_polygon *p);

BSG_EXPORT extern struct bg_polygon *
bsg_polygon_fill_segments(struct bg_polygon *poly, plane_t *vp, vect2d_t line_slope, fastf_t line_spacing);

/* For all polygon bsg_scene_objs in the objs table, apply the specified boolean
 * op using p and replace the original polygon geometry in objs with the results. */
BSG_EXPORT extern int bsg_polygon_csg_ref(bsg_polygon_ref target, bsg_polygon_ref stencil, bg_clip_t op);

__END_DECLS

#endif  /* BSG_POLYGON_H */
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
