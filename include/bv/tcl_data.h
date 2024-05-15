/*                     B V I E W / T C L _ D A T A . H
 * BRL-CAD
 *
 * Copyright (c) 1993-2024 United States Government as represented by
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
/** @addtogroup bv_tcl
 *
 * We're retaining these for libtclcad - don't use elsewhere
 */
#ifndef DM_BV_TCL_DATA_H
#define DM_BV_TCL_DATA_H

#include "common.h"
#include "bu/list.h"
#include "bu/vls.h"
#include "bu/ptbl.h"
#include "bg/polygon_types.h"
#include "bv/faceplate.h"
#include "vmath.h"

/** @{ */
/** @file bv/tcl_data.h */

#define BV_POLY_CIRCLE_MODE 15
#define BV_POLY_CONTOUR_MODE 16

// Separate these out, as we'll try not to use them in the new display work
struct bv_scene_obj_old_settings {
    char s_wflag;		/**< @brief  work flag - used by various libged and Tcl functions */
    char s_dflag;		/**< @brief  1 - s_basecolor is derived from the default */
    unsigned char s_basecolor[3];	/**< @brief  color from containing region */
    char s_uflag;		/**< @brief  1 - the user specified the color */
    char s_cflag;		/**< @brief  1 - use the default color */

   /* Database object related info */
    char s_Eflag;		/**< @brief  flag - not a solid but an "E'd" region (MGED ONLY)*/
    short s_regionid;		/**< @brief  region ID (MGED ONLY)*/
};

/* A display list corresponds (typically) to a database object.  It is composed of one
 * or more scene objects, which can be manipulated independently but collectively make
 * up the displayed representation of an object. */
struct display_list {
    struct bu_list      l;
    void               *dl_dp;                 /* Normally this will be a struct directory pointer */
    struct bu_vls       dl_path;
    struct bu_list      dl_head_scene_obj;          /**< @brief  head of scene obj list for this display list */
    int                 dl_wflag;
};

struct bv_data_axes_state {
    int       draw;
    int       color[3];
    int       line_width;          /* in pixels */
    fastf_t   size;                /* in view coordinates */
    int       num_points;
    point_t   *points;             /* in model coordinates */
};

struct bv_data_arrow_state {
    int       gdas_draw;
    int       gdas_color[3];
    int       gdas_line_width;          /* in pixels */
    int       gdas_tip_length;
    int       gdas_tip_width;
    int       gdas_num_points;
    point_t   *gdas_points;             /* in model coordinates */
};

struct bv_data_label_state {
    int         gdls_draw;
    int         gdls_color[3];
    int         gdls_num_labels;
    int         gdls_size;
    char        **gdls_labels;
    point_t     *gdls_points;
};

struct bv_data_line_state {
    int       gdls_draw;
    int       gdls_color[3];
    int       gdls_line_width;          /* in pixels */
    int       gdls_num_points;
    point_t   *gdls_points;             /* in model coordinates */
};

typedef struct {
    int                 gdps_draw;
    int                 gdps_moveAll;
    int                 gdps_color[3];
    int                 gdps_line_width;        /* in pixels */
    int                 gdps_line_style;
    int                 gdps_cflag;             /* contour flag */
    size_t              gdps_target_polygon_i;
    size_t              gdps_curr_polygon_i;
    size_t              gdps_curr_point_i;
    point_t             gdps_prev_point;
    bg_clip_t           gdps_clip_type;
    fastf_t             gdps_scale;
    point_t             gdps_origin;
    mat_t               gdps_rotation;
    mat_t               gdps_view2model;
    mat_t               gdps_model2view;
    struct bg_polygons  gdps_polygons;
    fastf_t             gdps_data_vZ;
} bv_data_polygon_state;


/* A note about the vZ parameter...
 *
 * gv_data_vZ is an internal parameter used by commands creating and
 * manipulating data objects.  Geometrically, it is a magnitude in the
 * direction of the Z vector of the view plane. Functionally, what it
 * allows the code to do is define a 2D plane embedded in in 3D space that
 * is offset from but parallel to the view plane - in an orthogonal view
 * this corresponds to objects drawn in that plane being "above" or "below"
 * objects defined within the view plane itself.
 *
 * Visually, objects drawn in this fashion in orthogonal views will be
 * indistinguishable regardless of their vZ offset - it is only when the
 * view is rotated that the user will be able to see the "above" and
 * "below" effect of creating view objects with differing vZ values.
 *
 * Users will generally not want to set gv_data_vZ directly, as it is a view
 * space value and may not behave intuitively.  Commands are defined to
 * calculate vZ values based on model spaces inputs, and these should be
 * used to generate the value supplied to gv_data_vZ.
 */
struct bv_data_tclcad {
    int           		gv_polygon_mode;  // libtclcad polygon modes
    int		  		gv_hide;          // libtclcad setting for hiding view - unused?
    fastf_t       		gv_data_vZ;
    struct bv_data_arrow_state	gv_data_arrows;
    struct bv_data_axes_state	gv_data_axes;
    struct bv_data_label_state	gv_data_labels;
    struct bv_data_line_state	gv_data_lines;
    bv_data_polygon_state	gv_data_polygons;
    struct bv_data_arrow_state	gv_sdata_arrows;
    struct bv_data_axes_state	gv_sdata_axes;
    struct bv_data_label_state	gv_sdata_labels;
    struct bv_data_line_state	gv_sdata_lines;
    bv_data_polygon_state	gv_sdata_polygons;
    struct bv_other_state	gv_prim_labels;
};

#endif /* DM_BV_TCL_DATA_H */

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
