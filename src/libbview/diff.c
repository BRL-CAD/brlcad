/*                    D I F F . C
 * BRL-CAD
 *
 * Copyright (c) 2020-2021 United States Government as represented by
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
/** @file diff.c
 *
 */

#include "common.h"
#include <string.h>
#include "vmath.h"
#include "bu/log.h"
#include "bu/str.h"
#include "bn/mat.h"
#include "bn/vlist.h"
#include "bview/defines.h"
#include "bview/util.h"

#define BVIEW_DIFF(_r, _var) \
    do { \
	if (v1->_var != v2->_var) \
	return _r; \
    } while (0)

#define BVIEW_SDIFF(_r, _var) \
    do { \
	if (!BU_STR_EQUAL(v1->_var, v2->_var)) \
	return _r; \
    } while (0)

#define BVIEW_MDIFF(_r, _var) \
    do { \
	struct bn_tol mtol = {BN_TOL_MAGIC, BN_TOL_DIST, BN_TOL_DIST * BN_TOL_DIST, 1.0e-6, 1.0 - 1.0e-6 }; \
	if (!bn_mat_is_equal(v1->_var, v2->_var, &mtol)) \
	return _r; \
    } while (0)

#define BVIEW_NDIFF(_r, _var) \
    do { \
	if (!NEAR_EQUAL((double)v1->_var, (double)v2->_var, VDIVIDE_TOL)) \
	return _r; \
    } while (0)

#define BVIEW_VDIFF(_r, _var) \
    do { \
	if (!VNEAR_EQUAL(v1->_var, v2->_var, VDIVIDE_TOL)) \
	return _r; \
    } while (0)

#define BVIEW_IVDIFF(_r, _var) \
    do { \
	if (!NEAR_EQUAL(v1->_var[0], v2->_var[0], VDIVIDE_TOL)) \
	return _r; \
	if (!NEAR_EQUAL(v1->_var[1], v2->_var[1], VDIVIDE_TOL)) \
	return _r; \
	if (!NEAR_EQUAL(v1->_var[2], v2->_var[2], VDIVIDE_TOL)) \
	return _r; \
    } while (0)

#define BVIEW_CDIFF(_r, _c, _var) \
    do { \
	if (_c(&v1->_var, &v2->_var)) \
	return _r; \
    } while (0)


static int
_bview_adc_state_differ(struct bview_adc_state *v1, struct bview_adc_state *v2)
{
    /* First, do sanity checks */
    if (!v1 && !v2)
	return -1;
    if ((v1 && !v2) || (!v1 && v2))
	return -1;

    BVIEW_NDIFF(1,draw);
    BVIEW_NDIFF(1,dv_x);
    BVIEW_NDIFF(1,dv_y);
    BVIEW_NDIFF(1,dv_a1);
    BVIEW_NDIFF(1,dv_a2);
    BVIEW_NDIFF(1,dv_dist);
    BVIEW_VDIFF(1,pos_model);
    BVIEW_VDIFF(1,pos_view);
    BVIEW_VDIFF(1,pos_grid);
    BVIEW_NDIFF(1,a1);
    BVIEW_NDIFF(1,a2);
    BVIEW_NDIFF(1,dst);
    BVIEW_NDIFF(1,anchor_pos);
    BVIEW_NDIFF(1,anchor_a1);
    BVIEW_NDIFF(1,anchor_a2);
    BVIEW_NDIFF(1,anchor_dst);
    BVIEW_VDIFF(1,anchor_pt_a1);
    BVIEW_VDIFF(1,anchor_pt_a2);
    BVIEW_VDIFF(1,anchor_pt_dst);
    BVIEW_IVDIFF(1,line_color);
    BVIEW_IVDIFF(1,tick_color);
    BVIEW_NDIFF(1,line_width);

    return 0;
}

static int
_bview_axes_differ(struct bview_axes *v1, struct bview_axes *v2)
{
    /* First, do sanity checks */
    if (!v1 && !v2)
	return -1;
    if ((v1 && !v2) || (!v1 && v2))
	return -1;

    BVIEW_NDIFF(1,draw);
    BVIEW_VDIFF(1,axes_pos);
    BVIEW_NDIFF(1,axes_size);
    BVIEW_NDIFF(1,line_width);
    BVIEW_NDIFF(1,pos_only);
    BVIEW_IVDIFF(1,axes_color);
    BVIEW_NDIFF(1,label_flag);
    BVIEW_IVDIFF(1,label_color);
    BVIEW_NDIFF(1,triple_color);
    BVIEW_NDIFF(1,tick_enabled);
    BVIEW_NDIFF(1,tick_length);
    BVIEW_NDIFF(1,tick_major_length);
    BVIEW_NDIFF(1,tick_interval);
    BVIEW_NDIFF(1,ticks_per_major);
    BVIEW_NDIFF(1,tick_threshold);
    BVIEW_IVDIFF(1,tick_color);
    BVIEW_IVDIFF(1,tick_major_color);

    return 0;
}

static int
_bview_data_arrow_state_differ(struct bview_data_arrow_state *v1, struct bview_data_arrow_state *v2)
{
    /* First, do sanity checks */
    if (!v1 && !v2)
	return -1;
    if ((v1 && !v2) || (!v1 && v2))
	return -1;

    BVIEW_NDIFF(1,gdas_draw);
    BVIEW_IVDIFF(1,gdas_color);
    BVIEW_NDIFF(1,gdas_line_width);
    BVIEW_NDIFF(1,gdas_tip_length);
    BVIEW_NDIFF(1,gdas_tip_width);
    BVIEW_NDIFF(1,gdas_num_points);
    // If we have the same number of points, check them
    for (int i = 0; i < v1->gdas_num_points; i++) {
	BVIEW_VDIFF(1,gdas_points[i]);
    }
    return 0;
}

static int
_bview_data_axes_state_differ(struct bview_data_axes_state *v1, struct bview_data_axes_state *v2)
{
    /* First, do sanity checks */
    if (!v1 && !v2)
	return -1;
    if ((v1 && !v2) || (!v1 && v2))
	return -1;

    BVIEW_NDIFF(1,draw);
    BVIEW_IVDIFF(1,color);
    BVIEW_NDIFF(1,line_width);
    BVIEW_NDIFF(1,size);
    BVIEW_NDIFF(1,num_points);
    // If we have the same number of points, check them
    for (int i = 0; i < v1->num_points; i++) {
	BVIEW_VDIFF(1,points[i]);
    }
    return 0;
}

static int
_bview_data_label_state_differ(struct bview_data_label_state *v1, struct bview_data_label_state *v2)
{
    /* First, do sanity checks */
    if (!v1 && !v2)
	return -1;
    if ((v1 && !v2) || (!v1 && v2))
	return -1;

    BVIEW_NDIFF(1,gdls_draw);
    BVIEW_IVDIFF(1,gdls_color);
    BVIEW_NDIFF(1,gdls_num_labels);
    // If we have the same number of labels, check them
    for (int i = 0; i < v1->gdls_num_labels; i++) {
	BVIEW_SDIFF(1,gdls_labels[i]);
    }
    BVIEW_NDIFF(1,gdls_size);
    // If we have the same number of points, check them
    for (int i = 0; i < v1->gdls_size; i++) {
	BVIEW_VDIFF(1,gdls_points[i]);
    }
    return 0;
}


static int
_bview_data_line_state_differ(struct bview_data_line_state *v1, struct bview_data_line_state *v2)
{
    /* First, do sanity checks */
    if (!v1 && !v2)
	return -1;
    if ((v1 && !v2) || (!v1 && v2))
	return -1;

    BVIEW_NDIFF(1,gdls_draw);
    BVIEW_IVDIFF(1,gdls_color);
    BVIEW_NDIFF(1,gdls_line_width);
    BVIEW_NDIFF(1,gdls_num_points);
    // If we have the same number of points, check them
    for (int i = 0; i < v1->gdls_num_points; i++) {
	BVIEW_VDIFF(1,gdls_points[i]);
    }
    return 0;
}

static int
_bg_poly_contour_differ(struct bg_poly_contour *v1, struct bg_poly_contour *v2)
{
/* First, do sanity checks */
    if (!v1 && !v2)
	return -1;
    if ((v1 && !v2) || (!v1 && v2))
	return -1;

    BVIEW_NDIFF(1,num_points);
    for (size_t i = 0; i < v1->num_points; i++) {
	BVIEW_VDIFF(1,point[i]);
    }

    return 0;
}

static int
_bg_polygon_differ(struct bg_polygon *v1, struct bg_polygon *v2)
{
/* First, do sanity checks */
    if (!v1 && !v2)
	return -1;
    if ((v1 && !v2) || (!v1 && v2))
	return -1;

    BVIEW_NDIFF(1,num_contours);
    BVIEW_IVDIFF(1,gp_color);
    BVIEW_NDIFF(1,gp_line_width);
    BVIEW_NDIFF(1,gp_line_style);
    for (size_t i = 0; i < v1->num_contours; i++) {
	BVIEW_CDIFF(1, _bg_poly_contour_differ, contour[i]);
    }
    if (v1->hole && v2->hole) {
	for (size_t i = 0; i < v1->num_contours; i++) {
	    BVIEW_NDIFF(1,hole[i]);
	}
    }
    return 0;
}

static int
_bg_polygons_differ(struct bg_polygons *v1, struct bg_polygons *v2)
{
/* First, do sanity checks */
    if (!v1 && !v2)
	return -1;
    if ((v1 && !v2) || (!v1 && v2))
	return -1;

    BVIEW_NDIFF(1,num_polygons);
    for (size_t i = 0; i < v1->num_polygons; i++) {
	BVIEW_CDIFF(1, _bg_polygon_differ, polygon[i]);
    }
    return 0;
}

static int
_bview_data_polygon_state_differ(bview_data_polygon_state *v1, bview_data_polygon_state *v2)
{
    /* First, do sanity checks */
    if (!v1 && !v2)
	return -1;
    if ((v1 && !v2) || (!v1 && v2))
	return -1;

    BVIEW_NDIFF(1,gdps_draw);
    BVIEW_NDIFF(1,gdps_moveAll);
    BVIEW_IVDIFF(1,gdps_color);
    BVIEW_NDIFF(1,gdps_line_width);
    BVIEW_NDIFF(1,gdps_line_style);
    BVIEW_NDIFF(1,gdps_cflag);
    BVIEW_NDIFF(1,gdps_target_polygon_i);
    BVIEW_NDIFF(1,gdps_curr_polygon_i);
    BVIEW_NDIFF(1,gdps_curr_point_i);
    BVIEW_VDIFF(1,gdps_prev_point);
    BVIEW_DIFF(1,gdps_clip_type);
    BVIEW_NDIFF(1,gdps_scale);
    BVIEW_VDIFF(1,gdps_origin);
    BVIEW_MDIFF(1,gdps_rotation);
    BVIEW_MDIFF(1,gdps_view2model);
    BVIEW_MDIFF(1,gdps_model2view);
    BVIEW_NDIFF(1,gdps_data_vZ);
    BVIEW_CDIFF(1, _bg_polygons_differ, gdps_polygons);

    return 0;
}

static int
_bview_grid_state_differ(struct bview_grid_state *v1, struct bview_grid_state *v2)
{
    /* First, do sanity checks */
    if (!v1 && !v2)
	return -1;
    if ((v1 && !v2) || (!v1 && v2))
	return -1;

    BVIEW_NDIFF(1,rc);
    BVIEW_NDIFF(1,draw);
    BVIEW_NDIFF(1,snap);
    BVIEW_VDIFF(1,anchor);
    BVIEW_NDIFF(1,res_h);
    BVIEW_NDIFF(1,res_v);
    BVIEW_NDIFF(1,res_major_h);
    BVIEW_NDIFF(1,res_major_v);
    BVIEW_IVDIFF(1,color);
    return 0;
}

static int
_bview_other_state_differ(struct bview_other_state *v1, struct bview_other_state *v2)
{
    /* First, do sanity checks */
    if (!v1 && !v2)
	return -1;
    if ((v1 && !v2) || (!v1 && v2))
	return -1;

    BVIEW_NDIFF(1,gos_draw);
    BVIEW_IVDIFF(1,gos_line_color);
    BVIEW_IVDIFF(1,gos_text_color);

    return 0;
}


static int
_bview_interactive_rect_state_differ(struct bview_interactive_rect_state *v1, struct bview_interactive_rect_state *v2)
{
    /* First, do sanity checks */
    if (!v1 && !v2)
	return -1;
    if ((v1 && !v2) || (!v1 && v2))
	return -1;

    BVIEW_NDIFF(1,active);
    BVIEW_NDIFF(1,draw);
    BVIEW_NDIFF(1,line_width);
    BVIEW_NDIFF(1,line_style);
    BVIEW_NDIFF(1,pos[0]);
    BVIEW_NDIFF(1,pos[1]);
    BVIEW_NDIFF(1,dim[0]);
    BVIEW_NDIFF(1,dim[1]);
    BVIEW_NDIFF(1,x);
    BVIEW_NDIFF(1,y);
    BVIEW_NDIFF(1,width);
    BVIEW_NDIFF(1,height);
    BVIEW_IVDIFF(1,bg);
    BVIEW_IVDIFF(1,color);
    BVIEW_NDIFF(1,cdim[0]);
    BVIEW_NDIFF(1,cdim[1]);
    BVIEW_NDIFF(1,aspect);

    return 0;
}

static int
_bview_settings_differ(struct bview_settings *v1, struct bview_settings *v2)
{
    /* First, do sanity checks */
    if (!v1 && !v2)
	return -1;
    if ((v1 && !v2) || (!v1 && v2))
	return -1;

    BVIEW_NDIFF(1,mode);
    BVIEW_NDIFF(1,adaptive_plot);
    BVIEW_NDIFF(1,redraw_on_zoom);
    BVIEW_NDIFF(1,x_samples);
    BVIEW_NDIFF(1,y_samples);
    BVIEW_NDIFF(1,point_scale);
    BVIEW_NDIFF(1,curve_scale);
    BVIEW_NDIFF(1,bot_threshold);
    BVIEW_NDIFF(1,hidden);

    return 0;
}
int
bview_differ(struct bview *v1, struct bview *v2)
{
    /* First, do sanity checks */
    if (!v1 && !v2)
	return -1;
    if ((v1 && !v2) || (!v1 && v2))
	return -1;

    /* Now the real work starts - check the contents.  First up are
     * any settings which potentially impact visible data */
    BVIEW_NDIFF(1,gv_i_scale);
    BVIEW_NDIFF(1,gv_a_scale);
    BVIEW_NDIFF(1,gv_scale);
    BVIEW_NDIFF(1,gv_scale);
    BVIEW_NDIFF(1,gv_size);
    BVIEW_NDIFF(1,gv_isize);
    BVIEW_NDIFF(1,gv_perspective);
    BVIEW_VDIFF(1,gv_aet);
    BVIEW_VDIFF(1,gv_eye_pos);
    BVIEW_VDIFF(1,gv_keypoint);
    BVIEW_DIFF(1,gv_coord);
    BVIEW_DIFF(1,gv_rotate_about);
    BVIEW_MDIFF(1,gv_rotation);
    BVIEW_MDIFF(1,gv_center);
    BVIEW_MDIFF(1,gv_model2view);
    BVIEW_MDIFF(1,gv_pmodel2view);
    BVIEW_MDIFF(1,gv_view2model);
    BVIEW_MDIFF(1,gv_pmat);

    BVIEW_NDIFF(1,gv_width);
    BVIEW_NDIFF(1,gv_height);
    BVIEW_NDIFF(1,gv_prevMouseX);
    BVIEW_NDIFF(1,gv_prevMouseY);
    BVIEW_NDIFF(1,gv_minMouseDelta);
    BVIEW_NDIFF(1,gv_maxMouseDelta);
    BVIEW_NDIFF(1,gv_rscale);
    BVIEW_NDIFF(1,gv_sscale);

    // More complex containers have their own check routines
    BVIEW_CDIFF(1, _bview_settings_differ, gvs);
    BVIEW_CDIFF(1, _bview_adc_state_differ, gv_adc);
    BVIEW_CDIFF(1, _bview_axes_differ, gv_model_axes);
    BVIEW_CDIFF(1, _bview_axes_differ, gv_view_axes);
    BVIEW_CDIFF(1, _bview_data_arrow_state_differ, gv_data_arrows);
    BVIEW_CDIFF(1, _bview_data_axes_state_differ, gv_data_axes);
    BVIEW_CDIFF(1, _bview_data_label_state_differ, gv_data_labels);
    BVIEW_CDIFF(1, _bview_data_line_state_differ, gv_data_lines);
    BVIEW_CDIFF(1, _bview_data_polygon_state_differ, gv_data_polygons);
    BVIEW_CDIFF(1, _bview_data_arrow_state_differ, gv_sdata_arrows);
    BVIEW_CDIFF(1, _bview_data_axes_state_differ, gv_sdata_axes);
    BVIEW_CDIFF(1, _bview_data_label_state_differ, gv_sdata_labels);
    BVIEW_CDIFF(1, _bview_data_line_state_differ, gv_sdata_lines);
    BVIEW_CDIFF(1, _bview_data_polygon_state_differ, gv_sdata_polygons);
    BVIEW_CDIFF(1, _bview_grid_state_differ, gv_grid);
    BVIEW_CDIFF(1, _bview_other_state_differ, gv_center_dot);
    BVIEW_CDIFF(1, _bview_other_state_differ, gv_prim_labels);
    BVIEW_CDIFF(1, _bview_other_state_differ, gv_view_params);
    BVIEW_CDIFF(1, _bview_other_state_differ, gv_view_scale);
    BVIEW_CDIFF(1, _bview_interactive_rect_state_differ, gv_rect);

    BVIEW_NDIFF(1,gv_snap_lines);
    BVIEW_NDIFF(1,gv_snap_tol_factor);
    BVIEW_NDIFF(1,gv_cleared);
    BVIEW_NDIFF(1,gv_zclip);
    BVIEW_NDIFF(1,gv_data_vZ);

    BVIEW_DIFF(3,gv_callback);
    BVIEW_DIFF(3,gv_clientData);
    BVIEW_DIFF(3,dmp);
    BVIEW_DIFF(3,u_data);

    BVIEW_DIFF(3,callbacks);
    // check for internal callback differences
    if (v1->callbacks && v2->callbacks) {
	if (BU_PTBL_LEN(v1->callbacks) != BU_PTBL_LEN(v2->callbacks)) {
	    return 3;
	}
	for (size_t i = 0; i < BU_PTBL_LEN(v1->callbacks); i++) {
	    if (BU_PTBL_GET(v1->callbacks, i) != BU_PTBL_GET(v2->callbacks, i)) {
		return 3;
	    }
	}
    }

    return 0;
}

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
