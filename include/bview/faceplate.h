/*                    F A C E P L A T E . H
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
 * Types and definitions specific to faceplate view objects *
 */
#ifndef DM_BVIEW_FACEPLATE_H
#define DM_BVIEW_FACEPLATE_H

#include "common.h"
#include "vmath.h"

/** @{ */
/** @file bview/faceplate.h */

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

struct bview_other_state {
    int gos_draw;
    int gos_line_color[3];
    int gos_text_color[3];
};

#endif /* DM_BVIEW_FACEPLATE_H */

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
