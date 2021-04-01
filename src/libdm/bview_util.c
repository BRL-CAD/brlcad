/*                    B V I E W _ U T I L . C
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
/** @file bview_util.c
 *
 * Utility functions for operating on BRL-CAD views
 *
 */

#include "common.h"
#include <string.h>
#include "vmath.h"
#include "bu/log.h"
#include "bu/str.h"
#include "bn/mat.h"
#include "dm/defines.h"
#include "dm/bview_util.h"
#define XXH_INLINE_ALL
#include "xxhash.h"

void
bview_init(struct bview *gvp)
{
    if (!gvp)
	return;

    gvp->magic = BVIEW_MAGIC;

    gvp->gv_scale = 500.0;
    gvp->gv_i_scale = gvp->gv_scale;
    gvp->gv_a_scale = 1.0 - gvp->gv_scale / gvp->gv_i_scale;
    gvp->gv_size = 2.0 * gvp->gv_scale;
    gvp->gv_isize = 1.0 / gvp->gv_size;
    VSET(gvp->gv_aet, 35.0, 25.0, 0.0);
    VSET(gvp->gv_eye_pos, 0.0, 0.0, 1.0);
    MAT_IDN(gvp->gv_rotation);
    MAT_IDN(gvp->gv_center);
    VSETALL(gvp->gv_keypoint, 0.0);
    gvp->gv_coord = 'v';
    gvp->gv_rotate_about = 'v';
    gvp->gv_minMouseDelta = -20;
    gvp->gv_maxMouseDelta = 20;
    gvp->gv_rscale = 0.4;
    gvp->gv_sscale = 2.0;

    gvp->gv_adc.a1 = 45.0;
    gvp->gv_adc.a2 = 45.0;
    VSET(gvp->gv_adc.line_color, 255, 255, 0);
    VSET(gvp->gv_adc.tick_color, 255, 255, 255);

    VSET(gvp->gv_grid.anchor, 0.0, 0.0, 0.0);
    gvp->gv_grid.res_h = 1.0;
    gvp->gv_grid.res_v = 1.0;
    gvp->gv_grid.res_major_h = 5;
    gvp->gv_grid.res_major_v = 5;
    VSET(gvp->gv_grid.color, 255, 255, 255);

    gvp->gv_rect.draw = 0;
    gvp->gv_rect.pos[0] = 128;
    gvp->gv_rect.pos[1] = 128;
    gvp->gv_rect.dim[0] = 256;
    gvp->gv_rect.dim[1] = 256;
    VSET(gvp->gv_rect.color, 255, 255, 255);

    gvp->gv_view_axes.draw = 0;
    VSET(gvp->gv_view_axes.axes_pos, 0.85, -0.85, 0.0);
    gvp->gv_view_axes.axes_size = 0.2;
    gvp->gv_view_axes.line_width = 0;
    gvp->gv_view_axes.pos_only = 1;
    VSET(gvp->gv_view_axes.axes_color, 255, 255, 255);
    VSET(gvp->gv_view_axes.label_color, 255, 255, 0);
    gvp->gv_view_axes.triple_color = 1;

    gvp->gv_model_axes.draw = 0;
    VSET(gvp->gv_model_axes.axes_pos, 0.0, 0.0, 0.0);
    gvp->gv_model_axes.axes_size = 2.0;
    gvp->gv_model_axes.line_width = 0;
    gvp->gv_model_axes.pos_only = 0;
    VSET(gvp->gv_model_axes.axes_color, 255, 255, 255);
    VSET(gvp->gv_model_axes.label_color, 255, 255, 0);
    gvp->gv_model_axes.triple_color = 0;
    gvp->gv_model_axes.tick_enabled = 1;
    gvp->gv_model_axes.tick_length = 4;
    gvp->gv_model_axes.tick_major_length = 8;
    gvp->gv_model_axes.tick_interval = 100;
    gvp->gv_model_axes.ticks_per_major = 10;
    gvp->gv_model_axes.tick_threshold = 8;
    VSET(gvp->gv_model_axes.tick_color, 255, 255, 0);
    VSET(gvp->gv_model_axes.tick_major_color, 255, 0, 0);

    gvp->gv_center_dot.gos_draw = 0;
    VSET(gvp->gv_center_dot.gos_line_color, 255, 255, 0);

    gvp->gv_prim_labels.gos_draw = 0;
    VSET(gvp->gv_prim_labels.gos_text_color, 255, 255, 0);

    gvp->gv_view_params.gos_draw = 0;
    VSET(gvp->gv_view_params.gos_text_color, 255, 255, 0);

    gvp->gv_view_scale.gos_draw = 0;
    VSET(gvp->gv_view_scale.gos_line_color, 255, 255, 0);
    VSET(gvp->gv_view_scale.gos_text_color, 255, 255, 255);

    gvp->gv_data_vZ = 1.0;

    /* FIXME: this causes the shaders.sh regression to fail */
    /* _ged_mat_aet(gvp); */

    // Higher values indicate more aggressive behavior (i.e. points further away will be snapped).
    gvp->gv_snap_tol_factor = 10;
    gvp->gv_snap_lines = 0;

    bview_update(gvp);
}

void
bview_update(struct bview *gvp)
{
    vect_t work, work1;
    vect_t temp, temp1;

    if (!gvp)
        return;

    bn_mat_mul(gvp->gv_model2view,
               gvp->gv_rotation,
               gvp->gv_center);
    gvp->gv_model2view[15] = gvp->gv_scale;
    bn_mat_inv(gvp->gv_view2model, gvp->gv_model2view);

    /* Find current azimuth, elevation, and twist angles */
    VSET(work, 0.0, 0.0, 1.0);       /* view z-direction */
    MAT4X3VEC(temp, gvp->gv_view2model, work);
    VSET(work1, 1.0, 0.0, 0.0);      /* view x-direction */
    MAT4X3VEC(temp1, gvp->gv_view2model, work1);

    /* calculate angles using accuracy of 0.005, since display
     * shows 2 digits right of decimal point */
    bn_aet_vec(&gvp->gv_aet[0],
               &gvp->gv_aet[1],
               &gvp->gv_aet[2],
               temp, temp1, (fastf_t)0.005);

    /* Force azimuth range to be [0, 360] */
    if ((NEAR_EQUAL(gvp->gv_aet[1], 90.0, (fastf_t)0.005) ||
         NEAR_EQUAL(gvp->gv_aet[1], -90.0, (fastf_t)0.005)) &&
        gvp->gv_aet[0] < 0 &&
        !NEAR_ZERO(gvp->gv_aet[0], (fastf_t)0.005))
        gvp->gv_aet[0] += 360.0;
    else if (NEAR_ZERO(gvp->gv_aet[0], (fastf_t)0.005))
        gvp->gv_aet[0] = 0.0;

    /* apply the perspective angle to model2view */
    bn_mat_mul(gvp->gv_pmodel2view, gvp->gv_pmat, gvp->gv_model2view);

    if (gvp->gv_callback) {

	if (gvp->callbacks) {
	    if (bu_ptbl_locate(gvp->callbacks, (long *)(long)gvp->gv_callback) != -1) {
		bu_log("Recursive callback (bview_update and gvp->gv_callback)");
	    }
	    bu_ptbl_ins_unique(gvp->callbacks, (long *)(long)gvp->gv_callback);
	}

	(*gvp->gv_callback)(gvp, gvp->gv_clientData);

	if (gvp->callbacks) {
	    bu_ptbl_rm(gvp->callbacks, (long *)(long)gvp->gv_callback);
	}

    }
}


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
_bview_axes_state_differ(struct bview_axes_state *v1, struct bview_axes_state *v2)
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


    BVIEW_NDIFF(1,gv_prevMouseX);
    BVIEW_NDIFF(1,gv_prevMouseY);
    BVIEW_NDIFF(1,gv_minMouseDelta);
    BVIEW_NDIFF(1,gv_maxMouseDelta);
    BVIEW_NDIFF(1,gv_rscale);
    BVIEW_NDIFF(1,gv_sscale);
    BVIEW_NDIFF(1,gv_mode);
    BVIEW_NDIFF(1,gv_zclip);

    // More complex containers have their own check routines
    BVIEW_CDIFF(1, _bview_adc_state_differ, gv_adc);
    BVIEW_CDIFF(1, _bview_axes_state_differ, gv_model_axes);
    BVIEW_CDIFF(1, _bview_axes_state_differ, gv_view_axes);
    BVIEW_CDIFF(1, _bview_data_arrow_state_differ, gv_data_arrows);
    BVIEW_CDIFF(1, _bview_data_axes_state_differ, gv_data_axes);
    BVIEW_CDIFF(1, _bview_data_label_state_differ, gv_data_labels);
    BVIEW_CDIFF(1, _bview_data_line_state_differ, gv_data_lines);
    BVIEW_CDIFF(1, _bview_data_polygon_state_differ, gv_data_polygons);
    BVIEW_CDIFF(1, _bview_data_arrow_state_differ, gv_sdata_arrows);
    BVIEW_CDIFF(1, _bview_data_axes_state_differ, gv_sdata_axes);
    BVIEW_CDIFF(1, _bview_data_label_state_differ, gv_sdata_labels);
    BVIEW_CDIFF(1, _bview_data_line_state_differ, gv_sdata_lines);
    BVIEW_NDIFF(1,gv_snap_lines);
    BVIEW_NDIFF(1,gv_snap_tol_factor);
    BVIEW_CDIFF(1, _bview_data_polygon_state_differ, gv_sdata_polygons);
    BVIEW_CDIFF(1, _bview_grid_state_differ, gv_grid);
    BVIEW_CDIFF(1, _bview_other_state_differ, gv_center_dot);
    BVIEW_CDIFF(1, _bview_other_state_differ, gv_prim_labels);
    BVIEW_CDIFF(1, _bview_other_state_differ, gv_view_params);
    BVIEW_CDIFF(1, _bview_other_state_differ, gv_view_scale);
    BVIEW_CDIFF(1, _bview_interactive_rect_state_differ, gv_rect);

    BVIEW_NDIFF(1,gv_adaptive_plot);
    BVIEW_NDIFF(2,gv_redraw_on_zoom);
    BVIEW_NDIFF(1,gv_x_samples);
    BVIEW_NDIFF(1,gv_y_samples);
    BVIEW_NDIFF(1,gv_point_scale);
    BVIEW_NDIFF(1,gv_curve_scale);
    BVIEW_NDIFF(1,gv_data_vZ);
    BVIEW_NDIFF(1,gv_bot_threshold);
    BVIEW_NDIFF(1,gv_hidden);

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



static void 
_bview_adc_state_hash(XXH64_state_t *state, struct bview_adc_state *v)
{
    /* First, do sanity checks */
    if (!v || !state)
	return;

    XXH64_update(state, &v->draw, sizeof(int));
    XXH64_update(state, &v->dv_x, sizeof(int));
    XXH64_update(state, &v->dv_y, sizeof(int));
    XXH64_update(state, &v->dv_a1, sizeof(int));
    XXH64_update(state, &v->dv_a2, sizeof(int));
    XXH64_update(state, &v->dv_dist, sizeof(int));
    XXH64_update(state, &v->pos_model, sizeof(fastf_t[3]));
    XXH64_update(state, &v->pos_view, sizeof(fastf_t[3]));
    XXH64_update(state, &v->pos_grid, sizeof(fastf_t[3]));
    XXH64_update(state, &v->a1, sizeof(fastf_t));
    XXH64_update(state, &v->a2, sizeof(fastf_t));
    XXH64_update(state, &v->dst, sizeof(fastf_t));
    XXH64_update(state, &v->anchor_pos, sizeof(int));
    XXH64_update(state, &v->anchor_a1, sizeof(int));
    XXH64_update(state, &v->anchor_a2, sizeof(int));
    XXH64_update(state, &v->anchor_dst, sizeof(int));
    XXH64_update(state, &v->anchor_pt_a1, sizeof(fastf_t[3]));
    XXH64_update(state, &v->anchor_pt_a2, sizeof(fastf_t[3]));
    XXH64_update(state, &v->anchor_pt_dst, sizeof(fastf_t[3]));
    XXH64_update(state, &v->line_color, sizeof(int[3]));
    XXH64_update(state, &v->tick_color, sizeof(int[3]));
    XXH64_update(state, &v->line_width, sizeof(int));
}

static void
_bview_axes_state_hash(XXH64_state_t *state, struct bview_axes_state *v)
{
    /* First, do sanity checks */
    if (!v || !state)
	return;

    XXH64_update(state, &v->draw, sizeof(int));
    XXH64_update(state, &v->axes_pos, sizeof(point_t));
    XXH64_update(state, &v->axes_size, sizeof(fastf_t));
    XXH64_update(state, &v->line_width, sizeof(int));
    XXH64_update(state, &v->pos_only, sizeof(int));
    XXH64_update(state, &v->axes_color, sizeof(int[3]));
    XXH64_update(state, &v->label_color, sizeof(int[3]));
    XXH64_update(state, &v->triple_color, sizeof(int));
    XXH64_update(state, &v->tick_enabled, sizeof(int));
    XXH64_update(state, &v->tick_length, sizeof(int));
    XXH64_update(state, &v->tick_major_length, sizeof(int));
    XXH64_update(state, &v->tick_interval, sizeof(fastf_t));
    XXH64_update(state, &v->ticks_per_major, sizeof(int));
    XXH64_update(state, &v->tick_threshold, sizeof(int));
    XXH64_update(state, &v->tick_color, sizeof(int[3]));
    XXH64_update(state, &v->tick_major_color, sizeof(int[3]));
}

static void
_bview_data_arrow_state_hash(XXH64_state_t *state, struct bview_data_arrow_state *v)
{
    /* First, do sanity checks */
    if (!v || !state)
	return;

    XXH64_update(state, &v->gdas_draw, sizeof(int));
    XXH64_update(state, &v->gdas_color, sizeof(int[3]));
    XXH64_update(state, &v->gdas_line_width, sizeof(int));
    XXH64_update(state, &v->gdas_tip_length, sizeof(int));
    XXH64_update(state, &v->gdas_tip_width, sizeof(int));
    XXH64_update(state, &v->gdas_num_points, sizeof(int));
    for (int i = 0; i < v->gdas_num_points; i++) {
	XXH64_update(state, &v->gdas_points[i], sizeof(point_t));
    }
}

static void 
_bview_data_axes_state_hash(XXH64_state_t *state, struct bview_data_axes_state *v)
{
    /* First, do sanity checks */
    if (!v || !state)
	return;

    XXH64_update(state, &v->draw, sizeof(int));
    XXH64_update(state, &v->color, sizeof(int[3]));
    XXH64_update(state, &v->line_width, sizeof(int));
    XXH64_update(state, &v->size, sizeof(fastf_t));
    XXH64_update(state, &v->num_points, sizeof(int));
    for (int i = 0; i < v->num_points; i++) {
	XXH64_update(state, &v->points[i], sizeof(point_t));
    }
}

static void
_bview_data_label_state_hash(XXH64_state_t *state, struct bview_data_label_state *v)
{
    /* First, do sanity checks */
    if (!v || !state)
	return;

    XXH64_update(state, &v->gdls_draw, sizeof(int));
    XXH64_update(state, &v->gdls_color, sizeof(int[3]));
    XXH64_update(state, &v->gdls_num_labels, sizeof(int));
    XXH64_update(state, &v->gdls_size, sizeof(int));
    for (int i = 0; i < v->gdls_num_labels; i++) {
	if (strlen(v->gdls_labels[i]))
	    XXH64_update(state, v->gdls_labels[i], strlen(v->gdls_labels[i]));
    }
    for (int i = 0; i < v->gdls_size; i++) {
	XXH64_update(state, &v->gdls_points[i], sizeof(point_t));
    }
}


static void
_bview_data_line_state_hash(XXH64_state_t *state, struct bview_data_line_state *v)
{
    /* First, do sanity checks */
    if (!v || !state)
	return;

    XXH64_update(state, &v->gdls_draw, sizeof(int));
    XXH64_update(state, &v->gdls_color, sizeof(int[3]));
    XXH64_update(state, &v->gdls_line_width, sizeof(int));
    XXH64_update(state, &v->gdls_num_points, sizeof(int));
    for (int i = 0; i < v->gdls_num_points; i++) {
	XXH64_update(state, &v->gdls_points[i], sizeof(point_t));
    }
}

static void
_bg_poly_contour_hash(XXH64_state_t *state, struct bg_poly_contour *v)
{
    /* First, do sanity checks */
    if (!v || !state)
	return;

    XXH64_update(state, &v->num_points, sizeof(size_t));
    for (size_t i = 0; i < v->num_points; i++) {
	XXH64_update(state, &v->point[i], sizeof(point_t));
    }
}

static void
_bg_polygon_hash(XXH64_state_t *state, struct bg_polygon *v)
{
    /* First, do sanity checks */
    if (!v || !state)
	return;

    XXH64_update(state, &v->num_contours, sizeof(size_t));
    XXH64_update(state, &v->gp_color, sizeof(int[3]));
    XXH64_update(state, &v->gp_line_width, sizeof(int));
    XXH64_update(state, &v->gp_line_style, sizeof(int));

    for (size_t i = 0; i < v->num_contours; i++) {
	_bg_poly_contour_hash(state, &v->contour[i]);
    }
    if (v->hole) {
	for (size_t i = 0; i < v->num_contours; i++) {
	    XXH64_update(state, &v->hole[i], sizeof(int));
	}
    }
}

static void
_bg_polygons_hash(XXH64_state_t *state, struct bg_polygons *v)
{
    /* First, do sanity checks */
    if (!v || !state)
	return;

    XXH64_update(state, &v->num_polygons, sizeof(size_t));
    for (size_t i = 0; i < v->num_polygons; i++) {
	_bg_polygon_hash(state, &v->polygon[i]);
    }
}

static void
_bview_data_polygon_state_hash(XXH64_state_t *state, bview_data_polygon_state *v)
{
    /* First, do sanity checks */
    if (!v || !state)
	return;

    XXH64_update(state, &v->gdps_draw, sizeof(int));
    XXH64_update(state, &v->gdps_moveAll, sizeof(int));
    XXH64_update(state, &v->gdps_color, sizeof(int[3]));
    XXH64_update(state, &v->gdps_line_width, sizeof(int));
    XXH64_update(state, &v->gdps_line_style, sizeof(int));
    XXH64_update(state, &v->gdps_cflag, sizeof(int));
    XXH64_update(state, &v->gdps_target_polygon_i, sizeof(size_t));
    XXH64_update(state, &v->gdps_curr_polygon_i, sizeof(size_t));
    XXH64_update(state, &v->gdps_curr_point_i, sizeof(size_t));
    XXH64_update(state, &v->gdps_prev_point, sizeof(point_t));
    XXH64_update(state, &v->gdps_clip_type, sizeof(bg_clip_t));
    XXH64_update(state, &v->gdps_scale, sizeof(fastf_t));
    XXH64_update(state, &v->gdps_origin, sizeof(point_t));
    XXH64_update(state, &v->gdps_rotation, sizeof(mat_t));
    XXH64_update(state, &v->gdps_view2model, sizeof(mat_t));
    XXH64_update(state, &v->gdps_model2view, sizeof(mat_t));
    _bg_polygons_hash(state, &v->gdps_polygons);
    XXH64_update(state, &v->gdps_data_vZ, sizeof(fastf_t));
}

static void
_bview_grid_state_hash(XXH64_state_t *state, struct bview_grid_state *v)
{
    /* First, do sanity checks */
    if (!v || !state)
	return;

    XXH64_update(state, &v->rc, sizeof(int));
    XXH64_update(state, &v->draw, sizeof(int));
    XXH64_update(state, &v->snap, sizeof(int));
    XXH64_update(state, &v->anchor, sizeof(fastf_t[3]));
    XXH64_update(state, &v->res_h, sizeof(fastf_t));
    XXH64_update(state, &v->res_v, sizeof(fastf_t));
    XXH64_update(state, &v->res_major_h, sizeof(int));
    XXH64_update(state, &v->res_major_v, sizeof(int));
    XXH64_update(state, &v->color, sizeof(int[3]));
}

static void
_bview_other_state_hash(XXH64_state_t *state, struct bview_other_state *v)
{
    /* First, do sanity checks */
    if (!v || !state)
	return;

    XXH64_update(state, &v->gos_draw, sizeof(int));
    XXH64_update(state, &v->gos_line_color, sizeof(int[3]));
    XXH64_update(state, &v->gos_text_color, sizeof(int[3]));
}


static void
_bview_interactive_rect_state_hash(XXH64_state_t *state, struct bview_interactive_rect_state *v)
{
    /* First, do sanity checks */
    if (!v || !state)
	return;

    XXH64_update(state, &v->active, sizeof(int));
    XXH64_update(state, &v->draw, sizeof(int));
    XXH64_update(state, &v->line_width, sizeof(int));
    XXH64_update(state, &v->line_style, sizeof(int));
    XXH64_update(state, &v->pos, sizeof(int[2]));
    XXH64_update(state, &v->dim, sizeof(int[2]));
    XXH64_update(state, &v->x, sizeof(fastf_t));
    XXH64_update(state, &v->y, sizeof(fastf_t));
    XXH64_update(state, &v->width, sizeof(fastf_t));
    XXH64_update(state, &v->height, sizeof(fastf_t));
    XXH64_update(state, &v->bg, sizeof(int[3]));
    XXH64_update(state, &v->color, sizeof(int[3]));
    XXH64_update(state, &v->cdim, sizeof(int[2]));
    XXH64_update(state, &v->aspect, sizeof(fastf_t));
}

unsigned long long
bview_hash(struct bview *v)
{
    if (!v)
	return 0;

    XXH64_hash_t hash_val;
    XXH64_state_t *state;
    state = XXH64_createState();
    if (!state)
	return 0;
    XXH64_reset(state, 0);

    XXH64_update(state, &v->gv_scale, sizeof(fastf_t));
    XXH64_update(state, &v->gv_size, sizeof(fastf_t));
    XXH64_update(state, &v->gv_isize, sizeof(fastf_t));
    XXH64_update(state, &v->gv_perspective, sizeof(fastf_t));
    XXH64_update(state, &v->gv_aet, sizeof(vect_t));
    XXH64_update(state, &v->gv_eye_pos, sizeof(vect_t));
    XXH64_update(state, &v->gv_keypoint, sizeof(vect_t));
    XXH64_update(state, &v->gv_coord, sizeof(char));
    XXH64_update(state, &v->gv_rotate_about, sizeof(char));
    XXH64_update(state, &v->gv_rotation, sizeof(mat_t));
    XXH64_update(state, &v->gv_center, sizeof(mat_t));
    XXH64_update(state, &v->gv_model2view, sizeof(mat_t));
    XXH64_update(state, &v->gv_pmodel2view, sizeof(mat_t));
    XXH64_update(state, &v->gv_view2model, sizeof(mat_t));
    XXH64_update(state, &v->gv_pmat, sizeof(mat_t));
    XXH64_update(state, &v->gv_prevMouseX, sizeof(fastf_t));
    XXH64_update(state, &v->gv_prevMouseY, sizeof(fastf_t));
    XXH64_update(state, &v->gv_minMouseDelta, sizeof(fastf_t));
    XXH64_update(state, &v->gv_maxMouseDelta, sizeof(fastf_t));
    XXH64_update(state, &v->gv_rscale, sizeof(fastf_t));
    XXH64_update(state, &v->gv_sscale, sizeof(fastf_t));
    XXH64_update(state, &v->gv_mode, sizeof(int));
    XXH64_update(state, &v->gv_zclip, sizeof(int));
    _bview_adc_state_hash(state, &v->gv_adc);
    _bview_axes_state_hash(state, &v->gv_model_axes);
    _bview_axes_state_hash(state, &v->gv_view_axes);
    _bview_data_arrow_state_hash(state, &v->gv_data_arrows);
    _bview_data_axes_state_hash(state, &v->gv_data_axes);
    _bview_data_label_state_hash(state, &v->gv_data_labels);
    _bview_data_line_state_hash(state, &v->gv_data_lines);
    _bview_data_polygon_state_hash(state, &v->gv_data_polygons);
    _bview_data_arrow_state_hash(state, &v->gv_sdata_arrows);
    _bview_data_axes_state_hash(state, &v->gv_sdata_axes);
    _bview_data_label_state_hash(state, &v->gv_sdata_labels);
    _bview_data_line_state_hash(state, &v->gv_sdata_lines);
    XXH64_update(state, &v->gv_snap_lines, sizeof(int));
    XXH64_update(state, &v->gv_snap_tol_factor, sizeof(double));
    _bview_data_polygon_state_hash(state, &v->gv_sdata_polygons);
    _bview_grid_state_hash(state, &v->gv_grid);
    _bview_other_state_hash(state, &v->gv_center_dot);
    _bview_other_state_hash(state, &v->gv_prim_labels);
    _bview_other_state_hash(state, &v->gv_view_params);
    _bview_other_state_hash(state, &v->gv_view_scale);
    _bview_interactive_rect_state_hash(state, &v->gv_rect);
    XXH64_update(state, &v->gv_adaptive_plot, sizeof(int));
    XXH64_update(state, &v->gv_redraw_on_zoom, sizeof(int));
    XXH64_update(state, &v->gv_x_samples, sizeof(int));
    XXH64_update(state, &v->gv_y_samples, sizeof(int));
    XXH64_update(state, &v->gv_point_scale, sizeof(fastf_t));
    XXH64_update(state, &v->gv_curve_scale, sizeof(fastf_t));
    XXH64_update(state, &v->gv_data_vZ, sizeof(fastf_t));
    XXH64_update(state, &v->gv_bot_threshold, sizeof(size_t));
    XXH64_update(state, &v->gv_hidden, sizeof(int));

    hash_val = XXH64_digest(state);
    XXH64_freeState(state);

    return (unsigned long long)hash_val;
}

// TODO - support constraints
int
_bview_rot(struct bview *v, int dx, int dy, point_t keypoint, unsigned long long UNUSED(flags))
{
    if (!v)
	return 0;

    point_t rot_pt;
    point_t new_origin;
    mat_t viewchg, viewchginv;
    point_t new_cent_view;
    point_t new_cent_model;

    fastf_t rdx = (fastf_t)dx * 0.25;
    fastf_t rdy = (fastf_t)dy * 0.25;
    mat_t newrot, newinv;
    bn_mat_angles(newrot, rdx, rdy, 0);
    bn_mat_inv(newinv, newrot);
    MAT4X3PNT(rot_pt, v->gv_model2view, keypoint);  /* point to rotate around */

    bn_mat_xform_about_pnt(viewchg, newrot, rot_pt);
    bn_mat_inv(viewchginv, viewchg);
    VSET(new_origin, 0.0, 0.0, 0.0);
    MAT4X3PNT(new_cent_view, viewchginv, new_origin);
    MAT4X3PNT(new_cent_model, v->gv_view2model, new_cent_view);
    MAT_DELTAS_VEC_NEG(v->gv_center, new_cent_model);

    /* Update the rotation component of the model2view matrix */
    bn_mat_mul2(newrot, v->gv_rotation); /* pure rotation */

    /* gv_rotation is updated, now sync other bview values */
    bview_update(v);

    return 1;
}

int
_bview_trans(struct bview *v, int UNUSED(dx), int UNUSED(dy), point_t UNUSED(keypoint), unsigned long long UNUSED(flags))
{
    if (!v)
	return 0;


    return 0;
}
int
_bview_scale(struct bview *v, int UNUSED(dx), int dy, point_t UNUSED(keypoint), unsigned long long UNUSED(flags))
{
    if (!v || !v->gv_height)
	return 0;

    double f = (double)dy/(double)v->gv_height;
    v->gv_a_scale += f;
    if (-SMALL_FASTF < v->gv_a_scale && v->gv_a_scale < SMALL_FASTF) {
	v->gv_scale = v->gv_i_scale;
    } else {
	if (v->gv_a_scale > 0) {
	    /* positive - scale i_Viewscale by values in [0.0, 1.0] range */
	    v->gv_scale = v->gv_i_scale * (1.0 - v->gv_a_scale);
	} else {
	    /* negative - scale i_Viewscale by values in [1.0, 10.0] range */
	    v->gv_scale = v->gv_i_scale * (1.0 + (v->gv_a_scale * -9.0));
	}
    }

    if (v->gv_scale < BVIEW_MINVIEWSIZE) {
	v->gv_scale = BVIEW_MINVIEWSIZE;
    }
    if (v->gv_scale < BVIEW_MINVIEWSCALE)
	v->gv_scale = BVIEW_MINVIEWSCALE;

    v->gv_size = 2.0 * v->gv_scale;
    v->gv_isize = 1.0 / v->gv_size;

    /* scale factors are set, now sync other bview values */
    bview_update(v);

    return 1;
}

int
bview_adjust(struct bview *v, int dx, int dy, point_t keypoint, int mode, unsigned long long flags)
{
    if (mode == BVIEW_ADC)
	return -1;

    if (flags == BVIEW_IDLE)
	return 0;

    if (flags & BVIEW_ROT)
	return _bview_rot(v, dx, dy, keypoint, flags);

    if (flags & BVIEW_TRANS)
	return _bview_trans(v, dx, dy, keypoint, flags);

    if (flags & BVIEW_SCALE)
	return _bview_scale(v, dx, dy, keypoint, flags);

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
