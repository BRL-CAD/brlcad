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
#include "bv/vlist.h"
#include "bv/defines.h"
#include "bv/util.h"

#define BV_DIFF(_r, _var) \
    do { \
	if (v1->_var != v2->_var) \
	return _r; \
    } while (0)

#define BV_SDIFF(_r, _var) \
    do { \
	if (!BU_STR_EQUAL(v1->_var, v2->_var)) \
	return _r; \
    } while (0)

#define BV_MDIFF(_r, _var) \
    do { \
	struct bn_tol mtol = {BN_TOL_MAGIC, BN_TOL_DIST, BN_TOL_DIST * BN_TOL_DIST, 1.0e-6, 1.0 - 1.0e-6 }; \
	if (!bn_mat_is_equal(v1->_var, v2->_var, &mtol)) \
	return _r; \
    } while (0)

#define BV_NDIFF(_r, _var) \
    do { \
	if (!NEAR_EQUAL((double)v1->_var, (double)v2->_var, VDIVIDE_TOL)) \
	return _r; \
    } while (0)

#define BV_VDIFF(_r, _var) \
    do { \
	if (!VNEAR_EQUAL(v1->_var, v2->_var, VDIVIDE_TOL)) \
	return _r; \
    } while (0)

#define BV_IVDIFF(_r, _var) \
    do { \
	if (!NEAR_EQUAL(v1->_var[0], v2->_var[0], VDIVIDE_TOL)) \
	return _r; \
	if (!NEAR_EQUAL(v1->_var[1], v2->_var[1], VDIVIDE_TOL)) \
	return _r; \
	if (!NEAR_EQUAL(v1->_var[2], v2->_var[2], VDIVIDE_TOL)) \
	return _r; \
    } while (0)

#define BV_CDIFF(_r, _c, _var) \
    do { \
	if (_c(&v1->_var, &v2->_var)) \
	return _r; \
    } while (0)


static int
_bv_adc_state_differ(struct bv_adc_state *v1, struct bv_adc_state *v2)
{
    /* First, do sanity checks */
    if (!v1 && !v2)
	return -1;
    if ((v1 && !v2) || (!v1 && v2))
	return -1;

    BV_NDIFF(1,draw);
    BV_NDIFF(1,dv_x);
    BV_NDIFF(1,dv_y);
    BV_NDIFF(1,dv_a1);
    BV_NDIFF(1,dv_a2);
    BV_NDIFF(1,dv_dist);
    BV_VDIFF(1,pos_model);
    BV_VDIFF(1,pos_view);
    BV_VDIFF(1,pos_grid);
    BV_NDIFF(1,a1);
    BV_NDIFF(1,a2);
    BV_NDIFF(1,dst);
    BV_NDIFF(1,anchor_pos);
    BV_NDIFF(1,anchor_a1);
    BV_NDIFF(1,anchor_a2);
    BV_NDIFF(1,anchor_dst);
    BV_VDIFF(1,anchor_pt_a1);
    BV_VDIFF(1,anchor_pt_a2);
    BV_VDIFF(1,anchor_pt_dst);
    BV_IVDIFF(1,line_color);
    BV_IVDIFF(1,tick_color);
    BV_NDIFF(1,line_width);

    return 0;
}

static int
_bv_axes_differ(struct bv_axes *v1, struct bv_axes *v2)
{
    /* First, do sanity checks */
    if (!v1 && !v2)
	return -1;
    if ((v1 && !v2) || (!v1 && v2))
	return -1;

    BV_NDIFF(1,draw);
    BV_VDIFF(1,axes_pos);
    BV_NDIFF(1,axes_size);
    BV_NDIFF(1,line_width);
    BV_NDIFF(1,pos_only);
    BV_IVDIFF(1,axes_color);
    BV_NDIFF(1,label_flag);
    BV_IVDIFF(1,label_color);
    BV_NDIFF(1,triple_color);
    BV_NDIFF(1,tick_enabled);
    BV_NDIFF(1,tick_length);
    BV_NDIFF(1,tick_major_length);
    BV_NDIFF(1,tick_interval);
    BV_NDIFF(1,ticks_per_major);
    BV_NDIFF(1,tick_threshold);
    BV_IVDIFF(1,tick_color);
    BV_IVDIFF(1,tick_major_color);

    return 0;
}

static int
_bv_data_arrow_state_differ(struct bv_data_arrow_state *v1, struct bv_data_arrow_state *v2)
{
    /* First, do sanity checks */
    if (!v1 && !v2)
	return -1;
    if ((v1 && !v2) || (!v1 && v2))
	return -1;

    BV_NDIFF(1,gdas_draw);
    BV_IVDIFF(1,gdas_color);
    BV_NDIFF(1,gdas_line_width);
    BV_NDIFF(1,gdas_tip_length);
    BV_NDIFF(1,gdas_tip_width);
    BV_NDIFF(1,gdas_num_points);
    // If we have the same number of points, check them
    for (int i = 0; i < v1->gdas_num_points; i++) {
	BV_VDIFF(1,gdas_points[i]);
    }
    return 0;
}

static int
_bv_data_axes_state_differ(struct bv_data_axes_state *v1, struct bv_data_axes_state *v2)
{
    /* First, do sanity checks */
    if (!v1 && !v2)
	return -1;
    if ((v1 && !v2) || (!v1 && v2))
	return -1;

    BV_NDIFF(1,draw);
    BV_IVDIFF(1,color);
    BV_NDIFF(1,line_width);
    BV_NDIFF(1,size);
    BV_NDIFF(1,num_points);
    // If we have the same number of points, check them
    for (int i = 0; i < v1->num_points; i++) {
	BV_VDIFF(1,points[i]);
    }
    return 0;
}

static int
_bv_data_label_state_differ(struct bv_data_label_state *v1, struct bv_data_label_state *v2)
{
    /* First, do sanity checks */
    if (!v1 && !v2)
	return -1;
    if ((v1 && !v2) || (!v1 && v2))
	return -1;

    BV_NDIFF(1,gdls_draw);
    BV_IVDIFF(1,gdls_color);
    BV_NDIFF(1,gdls_num_labels);
    // If we have the same number of labels, check them
    for (int i = 0; i < v1->gdls_num_labels; i++) {
	BV_SDIFF(1,gdls_labels[i]);
    }
    BV_NDIFF(1,gdls_size);
    // If we have the same number of points, check them
    for (int i = 0; i < v1->gdls_size; i++) {
	BV_VDIFF(1,gdls_points[i]);
    }
    return 0;
}


static int
_bv_data_line_state_differ(struct bv_data_line_state *v1, struct bv_data_line_state *v2)
{
    /* First, do sanity checks */
    if (!v1 && !v2)
	return -1;
    if ((v1 && !v2) || (!v1 && v2))
	return -1;

    BV_NDIFF(1,gdls_draw);
    BV_IVDIFF(1,gdls_color);
    BV_NDIFF(1,gdls_line_width);
    BV_NDIFF(1,gdls_num_points);
    // If we have the same number of points, check them
    for (int i = 0; i < v1->gdls_num_points; i++) {
	BV_VDIFF(1,gdls_points[i]);
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

    BV_NDIFF(1,num_points);
    for (size_t i = 0; i < v1->num_points; i++) {
	BV_VDIFF(1,point[i]);
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

    BV_NDIFF(1,num_contours);
    BV_IVDIFF(1,gp_color);
    BV_NDIFF(1,gp_line_width);
    BV_NDIFF(1,gp_line_style);
    for (size_t i = 0; i < v1->num_contours; i++) {
	BV_CDIFF(1, _bg_poly_contour_differ, contour[i]);
    }
    if (v1->hole && v2->hole) {
	for (size_t i = 0; i < v1->num_contours; i++) {
	    BV_NDIFF(1,hole[i]);
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

    BV_NDIFF(1,num_polygons);
    for (size_t i = 0; i < v1->num_polygons; i++) {
	BV_CDIFF(1, _bg_polygon_differ, polygon[i]);
    }
    return 0;
}

static int
_bv_data_polygon_state_differ(bv_data_polygon_state *v1, bv_data_polygon_state *v2)
{
    /* First, do sanity checks */
    if (!v1 && !v2)
	return -1;
    if ((v1 && !v2) || (!v1 && v2))
	return -1;

    BV_NDIFF(1,gdps_draw);
    BV_NDIFF(1,gdps_moveAll);
    BV_IVDIFF(1,gdps_color);
    BV_NDIFF(1,gdps_line_width);
    BV_NDIFF(1,gdps_line_style);
    BV_NDIFF(1,gdps_cflag);
    BV_NDIFF(1,gdps_target_polygon_i);
    BV_NDIFF(1,gdps_curr_polygon_i);
    BV_NDIFF(1,gdps_curr_point_i);
    BV_VDIFF(1,gdps_prev_point);
    BV_DIFF(1,gdps_clip_type);
    BV_NDIFF(1,gdps_scale);
    BV_VDIFF(1,gdps_origin);
    BV_MDIFF(1,gdps_rotation);
    BV_MDIFF(1,gdps_view2model);
    BV_MDIFF(1,gdps_model2view);
    BV_NDIFF(1,gdps_data_vZ);
    BV_CDIFF(1, _bg_polygons_differ, gdps_polygons);

    return 0;
}

static int
_bv_grid_state_differ(struct bv_grid_state *v1, struct bv_grid_state *v2)
{
    /* First, do sanity checks */
    if (!v1 && !v2)
	return -1;
    if ((v1 && !v2) || (!v1 && v2))
	return -1;

    BV_NDIFF(1,rc);
    BV_NDIFF(1,draw);
    BV_NDIFF(1,snap);
    BV_VDIFF(1,anchor);
    BV_NDIFF(1,res_h);
    BV_NDIFF(1,res_v);
    BV_NDIFF(1,res_major_h);
    BV_NDIFF(1,res_major_v);
    BV_IVDIFF(1,color);
    return 0;
}

static int
_bv_other_state_differ(struct bv_other_state *v1, struct bv_other_state *v2)
{
    /* First, do sanity checks */
    if (!v1 && !v2)
	return -1;
    if ((v1 && !v2) || (!v1 && v2))
	return -1;

    BV_NDIFF(1,gos_draw);
    BV_IVDIFF(1,gos_line_color);
    BV_IVDIFF(1,gos_text_color);

    return 0;
}


static int
_bv_interactive_rect_state_differ(struct bv_interactive_rect_state *v1, struct bv_interactive_rect_state *v2)
{
    /* First, do sanity checks */
    if (!v1 && !v2)
	return -1;
    if ((v1 && !v2) || (!v1 && v2))
	return -1;

    BV_NDIFF(1,active);
    BV_NDIFF(1,draw);
    BV_NDIFF(1,line_width);
    BV_NDIFF(1,line_style);
    BV_NDIFF(1,pos[0]);
    BV_NDIFF(1,pos[1]);
    BV_NDIFF(1,dim[0]);
    BV_NDIFF(1,dim[1]);
    BV_NDIFF(1,x);
    BV_NDIFF(1,y);
    BV_NDIFF(1,width);
    BV_NDIFF(1,height);
    BV_IVDIFF(1,bg);
    BV_IVDIFF(1,color);
    BV_NDIFF(1,cdim[0]);
    BV_NDIFF(1,cdim[1]);
    BV_NDIFF(1,aspect);

    return 0;
}

static int
_bv_settings_differ(struct bv_settings *v1, struct bv_settings *v2)
{
    /* First, do sanity checks */
    if (!v1 && !v2)
	return -1;
    if ((v1 && !v2) || (!v1 && v2))
	return -1;
    return 0;
}
int
bv_differ(struct bv *v1, struct bv *v2)
{
    /* First, do sanity checks */
    if (!v1 && !v2)
	return -1;
    if ((v1 && !v2) || (!v1 && v2))
	return -1;

    /* Now the real work starts - check the contents.  First up are
     * any settings which potentially impact visible data */
    BV_NDIFF(1,gv_i_scale);
    BV_NDIFF(1,gv_a_scale);
    BV_NDIFF(1,gv_scale);
    BV_NDIFF(1,gv_scale);
    BV_NDIFF(1,gv_size);
    BV_NDIFF(1,gv_isize);
    BV_NDIFF(1,gv_perspective);
    BV_VDIFF(1,gv_aet);
    BV_VDIFF(1,gv_eye_pos);
    BV_VDIFF(1,gv_keypoint);
    BV_DIFF(1,gv_coord);
    BV_DIFF(1,gv_rotate_about);
    BV_MDIFF(1,gv_rotation);
    BV_MDIFF(1,gv_center);
    BV_MDIFF(1,gv_model2view);
    BV_MDIFF(1,gv_pmodel2view);
    BV_MDIFF(1,gv_view2model);
    BV_MDIFF(1,gv_pmat);

    BV_NDIFF(1,gv_width);
    BV_NDIFF(1,gv_height);
    BV_NDIFF(1,gv_prevMouseX);
    BV_NDIFF(1,gv_prevMouseY);
    BV_NDIFF(1,gv_minMouseDelta);
    BV_NDIFF(1,gv_maxMouseDelta);
    BV_NDIFF(1,gv_rscale);
    BV_NDIFF(1,gv_sscale);

    // More complex containers have their own check routines
    BV_CDIFF(1, _bv_settings_differ, gvs);
    BV_CDIFF(1, _bv_adc_state_differ, gv_adc);
    BV_CDIFF(1, _bv_axes_differ, gv_model_axes);
    BV_CDIFF(1, _bv_axes_differ, gv_view_axes);
    BV_CDIFF(1, _bv_data_arrow_state_differ, gv_tcl.gv_data_arrows);
    BV_CDIFF(1, _bv_data_axes_state_differ, gv_tcl.gv_data_axes);
    BV_CDIFF(1, _bv_data_label_state_differ, gv_tcl.gv_data_labels);
    BV_CDIFF(1, _bv_data_line_state_differ, gv_tcl.gv_data_lines);
    BV_CDIFF(1, _bv_data_polygon_state_differ, gv_tcl.gv_data_polygons);
    BV_CDIFF(1, _bv_data_arrow_state_differ, gv_tcl.gv_sdata_arrows);
    BV_CDIFF(1, _bv_data_axes_state_differ, gv_tcl.gv_sdata_axes);
    BV_CDIFF(1, _bv_data_label_state_differ, gv_tcl.gv_sdata_labels);
    BV_CDIFF(1, _bv_data_line_state_differ, gv_tcl.gv_sdata_lines);
    BV_CDIFF(1, _bv_data_polygon_state_differ, gv_tcl.gv_sdata_polygons);
    BV_CDIFF(1, _bv_grid_state_differ, gv_grid);
    BV_CDIFF(1, _bv_other_state_differ, gv_center_dot);
    BV_CDIFF(1, _bv_other_state_differ, gv_tcl.gv_prim_labels);
    BV_CDIFF(1, _bv_other_state_differ, gv_view_params);
    BV_CDIFF(1, _bv_other_state_differ, gv_view_scale);
    BV_CDIFF(1, _bv_interactive_rect_state_differ, gv_rect);

    BV_NDIFF(1,gv_snap_lines);
    BV_NDIFF(1,gv_snap_tol_factor);
    BV_NDIFF(1,gv_cleared);
    BV_NDIFF(1,gv_zclip);
    BV_NDIFF(1,gv_data_vZ);

    BV_NDIFF(1,adaptive_plot);
    BV_NDIFF(1,redraw_on_zoom);
    BV_NDIFF(1,point_scale);
    BV_NDIFF(1,curve_scale);
    BV_NDIFF(1,bot_threshold);

    BV_DIFF(3,gv_callback);
    BV_DIFF(3,gv_clientData);
    BV_DIFF(3,dmp);
    BV_DIFF(3,u_data);

    BV_DIFF(3,callbacks);
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
