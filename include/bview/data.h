/*                     B V I E W / D A T A . H
 * BRL-CAD
 *
 * Copyright (c) 1993-2021 United States Government as represented by
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
/** @addtogroup bview
 *
 * @brief
 *
 * Definitions specific to the non-view-obj drawing logic in libtclcad
 */
#ifndef DM_BVIEW_DATA_H
#define DM_BVIEW_DATA_H

#include "common.h"
#include "bu/list.h"
#include "bu/vls.h"
#include "bu/ptbl.h"
#include "bg/polygon_types.h"
#include "bview/faceplate.h"
#include "vmath.h"

/** @{ */
/** @file bview/data.h */

#define BVIEW_POLY_CIRCLE_MODE 15
#define BVIEW_POLY_CONTOUR_MODE 16

// Separate these out, as we'll try not to use them in the new display work
struct bview_scene_obj_old_settings {
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

struct bview_data_axes_state {
    int       draw;
    int       color[3];
    int       line_width;          /* in pixels */
    fastf_t   size;                /* in view coordinates */
    int       num_points;
    point_t   *points;             /* in model coordinates */
};

struct bview_data_arrow_state {
    int       gdas_draw;
    int       gdas_color[3];
    int       gdas_line_width;          /* in pixels */
    int       gdas_tip_length;
    int       gdas_tip_width;
    int       gdas_num_points;
    point_t   *gdas_points;             /* in model coordinates */
};

struct bview_data_label_state {
    int         gdls_draw;
    int         gdls_color[3];
    int         gdls_num_labels;
    int         gdls_size;
    char        **gdls_labels;
    point_t     *gdls_points;
};

struct bview_data_line_state {
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
} bview_data_polygon_state;


struct bview_data_tclcad {
    struct bview_data_arrow_state       gv_data_arrows;
    struct bview_data_axes_state        gv_data_axes;
    struct bview_data_label_state       gv_data_labels;
    struct bview_data_line_state        gv_data_lines;
    bview_data_polygon_state            gv_data_polygons;
    struct bview_data_arrow_state       gv_sdata_arrows;
    struct bview_data_axes_state        gv_sdata_axes;
    struct bview_data_label_state       gv_sdata_labels;
    struct bview_data_line_state        gv_sdata_lines;
    bview_data_polygon_state            gv_sdata_polygons;
    struct bview_other_state            gv_prim_labels;
};

#endif /* DM_BVIEW_DATA_H */

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
