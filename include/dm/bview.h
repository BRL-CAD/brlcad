/*                        B V I E W . H
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
 * Types and definitions related to display lists, angle distance cursor,
 * and other generic view constructs.
 *
 * This header is intended to be independent of any one BRL-CAD library and is
 * specifically intended to allow the easy definition of common display list
 * types between otherwise independent libraries (libdm and libged, for
 * example).
 *
 */
#ifndef DM_BVIEW_H
#define DM_BVIEW_H

#include "common.h"
#include "bu/list.h"
#include "bu/vls.h"
#include "bu/observer.h"
#include "bu/ptbl.h"
#include "bg/polygon_types.h"
#include "vmath.h"

/** @{ */
/** @file bview.h */

#define BVIEW_MINVIEWSIZE 0.0001
#define BVIEW_MINVIEWSCALE 0.00005

#ifndef UP
#  define UP 0
#endif
#ifndef DOWN
#  define DOWN 1
#endif

struct display_list {
    struct bu_list      l;
    void               *dl_dp;                 /* Normally this will be a struct directory pointer */
    struct bu_vls       dl_path;
    struct bu_list      dl_head_scene_obj;          /**< @brief  head of scene obj list for this display list */
    int                 dl_wflag;
};

struct bview_adc_state {
    int         draw;
    int         dv_x;
    int         dv_y;
    int         dv_a1;
    int         dv_a2;
    int         dv_dist;
    fastf_t     pos_model[3];
    fastf_t     pos_view[3];
    fastf_t     pos_grid[3];
    fastf_t     a1;
    fastf_t     a2;
    fastf_t     dst;
    int         anchor_pos;
    int         anchor_a1;
    int         anchor_a2;
    int         anchor_dst;
    fastf_t     anchor_pt_a1[3];
    fastf_t     anchor_pt_a2[3];
    fastf_t     anchor_pt_dst[3];
    int         line_color[3];
    int         tick_color[3];
    int         line_width;
};

struct bview_axes_state {
    int       draw;
    point_t   axes_pos;             /* in model coordinates */
    fastf_t   axes_size;            /* in view coordinates */
    int       line_width;           /* in pixels */
    int       pos_only;
    int       axes_color[3];
    int       label_color[3];
    int       triple_color;
    int       tick_enabled;
    int       tick_length;          /* in pixels */
    int       tick_major_length;    /* in pixels */
    fastf_t   tick_interval;        /* in mm */
    int       ticks_per_major;
    int       tick_threshold;
    int       tick_color[3];
    int       tick_major_color[3];
};

struct bview_data_axes_state {
    int       draw;
    int       color[3];
    int       line_width;          /* in pixels */
    fastf_t   size;                /* in view coordinates */
    int       num_points;
    point_t   *points;             /* in model coordinates */
};

struct bview_grid_state {
    int       rc;
    int       draw;               /* draw grid */
    int       snap;               /* snap to grid */
    fastf_t   anchor[3];
    fastf_t   res_h;              /* grid resolution in h */
    fastf_t   res_v;              /* grid resolution in v */
    int       res_major_h;        /* major grid resolution in h */
    int       res_major_v;        /* major grid resolution in v */
    int       color[3];
};

struct bview_interactive_rect_state {
    int        active;     /* 1 - actively drawing a rectangle */
    int        draw;       /* draw rubber band rectangle */
    int        line_width;
    int        line_style;  /* 0 - solid, 1 - dashed */
    int        pos[2];     /* Position in image coordinates */
    int        dim[2];     /* Rectangle dimension in image coordinates */
    fastf_t    x;          /* Corner of rectangle in normalized     */
    fastf_t    y;          /* ------ view coordinates (i.e. +-1.0). */
    fastf_t    width;      /* Width and height of rectangle in      */
    fastf_t    height;     /* ------ normalized view coordinates.   */
    int        bg[3];      /* Background color */
    int        color[3];   /* Rectangle color */
    int        cdim[2];    /* Canvas dimension in pixels */
    fastf_t    aspect;     /* Canvas aspect ratio */
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

struct bview_other_state {
    int gos_draw;
    int gos_line_color[3];
    int gos_text_color[3];
};

struct bview {
    uint32_t			magic;         /**< @brief magic number */
    struct bu_vls               gv_name;
    fastf_t                     gv_i_scale;
    fastf_t                     gv_a_scale;    /**< @brief absolute scale */
    fastf_t                     gv_scale;
    fastf_t                     gv_size;                /**< @brief  2.0 * scale */
    fastf_t                     gv_isize;               /**< @brief  1.0 / size */
    fastf_t                     gv_perspective;         /**< @brief  perspective angle */
    vect_t                      gv_aet;
    vect_t                      gv_eye_pos;             /**< @brief  eye position */
    vect_t                      gv_keypoint;
    char                        gv_coord;               /**< @brief  coordinate system */
    char                        gv_rotate_about;        /**< @brief  indicates what point rotations are about */
    mat_t                       gv_rotation;
    mat_t                       gv_center;
    mat_t                       gv_model2view;
    mat_t                       gv_pmodel2view;
    mat_t                       gv_view2model;
    mat_t                       gv_pmat;                /**< @brief  perspective matrix */
    void                        (*gv_callback)();       /**< @brief  called in ged_view_update with gvp and gv_clientData */
    void *                      gv_clientData;          /**< @brief  passed to gv_callback */
    int				gv_width;
    int				gv_height;
    fastf_t                     gv_prevMouseX;
    fastf_t                     gv_prevMouseY;
    fastf_t                     gv_minMouseDelta;
    fastf_t                     gv_maxMouseDelta;
    fastf_t                     gv_rscale;
    fastf_t                     gv_sscale;
    int                         gv_mode;
    int                         gv_zclip;
    struct bview_adc_state      gv_adc;
    struct bview_axes_state     gv_model_axes;
    struct bview_axes_state     gv_view_axes;
    struct bview_data_arrow_state gv_data_arrows;
    struct bview_data_axes_state        gv_data_axes;
    struct bview_data_label_state gv_data_labels;
    struct bview_data_line_state  gv_data_lines;
    bview_data_polygon_state      gv_data_polygons;
    struct bview_data_arrow_state gv_sdata_arrows;
    struct bview_data_axes_state        gv_sdata_axes;
    struct bview_data_label_state gv_sdata_labels;
    struct bview_data_line_state  gv_sdata_lines;
    int                           gv_snap_lines;
    double 			  gv_snap_tol_factor;
    bview_data_polygon_state      gv_sdata_polygons;
    struct bview_grid_state     gv_grid;
    struct bview_other_state      gv_center_dot;
    struct bview_other_state      gv_prim_labels;
    struct bview_other_state      gv_view_params;
    struct bview_other_state      gv_view_scale;
    struct bview_interactive_rect_state         gv_rect;
    int                         gv_adaptive_plot;
    int                         gv_redraw_on_zoom;
    int                         gv_x_samples;
    int                         gv_y_samples;
    fastf_t                     gv_point_scale;
    fastf_t                     gv_curve_scale;
    fastf_t                     gv_data_vZ;
    size_t                      gv_bot_threshold;
    int			        gv_hidden;
    void                        *dmp;  /* Display manager pointer, if one is associated with this view */
    void                        *u_data; /* Caller data associated with this view */
    struct bu_ptbl *callbacks;
};


struct bview_solid_data {
    struct display_list *gdlp;
    int draw_solid_lines_only;
    int wireframe_color_override;
    int wireframe_color[3];
    fastf_t transparency;
    int dmode;
    int hiddenLine;
    void *free_scene_obj;
};

#endif /* DM_BVIEW_H */

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
