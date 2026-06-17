/*                    D I F F . C
 * BRL-CAD
 *
 * Copyright (c) 2020-2026 United States Government as represented by
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
#include "bsg/defines.h"
#include "bsg/util.h"
#include "view_state_private.h"

#define BSG_DIFF(_r, _var) \
    do { \
	if (v1->_var != v2->_var) \
	return _r; \
    } while (0)

#define BSG_SDIFF(_r, _var) \
    do { \
	if (!BU_STR_EQUAL(v1->_var, v2->_var)) \
	return _r; \
    } while (0)

#define BSG_MDIFF(_r, _var) \
    do { \
	struct bn_tol mtol = {BN_TOL_MAGIC, BN_TOL_DIST, BN_TOL_DIST * BN_TOL_DIST, 1.0e-6, 1.0 - 1.0e-6 }; \
	if (!bn_mat_is_equal(v1->_var, v2->_var, &mtol)) \
	return _r; \
    } while (0)

#define BSG_NDIFF(_r, _var) \
    do { \
	if (!NEAR_EQUAL((double)v1->_var, (double)v2->_var, VDIVIDE_TOL)) \
	return _r; \
    } while (0)

#define BSG_VDIFF(_r, _var) \
    do { \
	if (!VNEAR_EQUAL(v1->_var, v2->_var, VDIVIDE_TOL)) \
	return _r; \
    } while (0)

#define BSG_IVDIFF(_r, _var) \
    do { \
	if (!NEAR_EQUAL(v1->_var[0], v2->_var[0], VDIVIDE_TOL)) \
	return _r; \
	if (!NEAR_EQUAL(v1->_var[1], v2->_var[1], VDIVIDE_TOL)) \
	return _r; \
	if (!NEAR_EQUAL(v1->_var[2], v2->_var[2], VDIVIDE_TOL)) \
	return _r; \
    } while (0)

#define BSG_CDIFF(_r, _c, _var) \
    do { \
	if (_c(&v1->_var, &v2->_var)) \
	return _r; \
    } while (0)


static int
_bsg_adc_state_differ(struct bsg_adc_state *v1, struct bsg_adc_state *v2)
{
    /* First, do sanity checks */
    if (!v1 && !v2)
	return -1;
    if ((v1 && !v2) || (!v1 && v2))
	return -1;

    BSG_NDIFF(1,draw);
    BSG_NDIFF(1,dv_x);
    BSG_NDIFF(1,dv_y);
    BSG_NDIFF(1,dv_a1);
    BSG_NDIFF(1,dv_a2);
    BSG_NDIFF(1,dv_dist);
    BSG_VDIFF(1,pos_model);
    BSG_VDIFF(1,pos_view);
    BSG_VDIFF(1,pos_grid);
    BSG_NDIFF(1,a1);
    BSG_NDIFF(1,a2);
    BSG_NDIFF(1,dst);
    BSG_NDIFF(1,anchor_pos);
    BSG_NDIFF(1,anchor_a1);
    BSG_NDIFF(1,anchor_a2);
    BSG_NDIFF(1,anchor_dst);
    BSG_VDIFF(1,anchor_pt_a1);
    BSG_VDIFF(1,anchor_pt_a2);
    BSG_VDIFF(1,anchor_pt_dst);
    BSG_IVDIFF(1,line_color);
    BSG_IVDIFF(1,tick_color);
    BSG_NDIFF(1,line_width);

    return 0;
}

static int
_bsg_axes_differ(struct bsg_axes *v1, struct bsg_axes *v2)
{
    /* First, do sanity checks */
    if (!v1 && !v2)
	return -1;
    if ((v1 && !v2) || (!v1 && v2))
	return -1;

    BSG_NDIFF(1,draw);
    BSG_VDIFF(1,axes_pos);
    BSG_NDIFF(1,axes_size);
    BSG_NDIFF(1,line_width);
    BSG_NDIFF(1,pos_only);
    BSG_IVDIFF(1,axes_color);
    BSG_NDIFF(1,label_flag);
    BSG_IVDIFF(1,label_color);
    BSG_NDIFF(1,triple_color);
    BSG_NDIFF(1,tick_enabled);
    BSG_NDIFF(1,tick_length);
    BSG_NDIFF(1,tick_major_length);
    BSG_NDIFF(1,tick_interval);
    BSG_NDIFF(1,ticks_per_major);
    BSG_NDIFF(1,tick_threshold);
    BSG_IVDIFF(1,tick_color);
    BSG_IVDIFF(1,tick_major_color);

    return 0;
}

/* View-scoped feature revisions carry renderable overlay/tool state for view
 * comparisons; direct per-tool storage comparisons are not part of this path. */

static int
_bsg_grid_state_differ(struct bsg_grid_state *v1, struct bsg_grid_state *v2)
{
    /* First, do sanity checks */
    if (!v1 && !v2)
	return -1;
    if ((v1 && !v2) || (!v1 && v2))
	return -1;

    BSG_NDIFF(1,rc);
    BSG_NDIFF(1,draw);
    BSG_NDIFF(1,adaptive);
    BSG_NDIFF(1,snap);
    BSG_VDIFF(1,anchor);
    BSG_NDIFF(1,res_h);
    BSG_NDIFF(1,res_v);
    BSG_NDIFF(1,res_major_h);
    BSG_NDIFF(1,res_major_v);
    BSG_IVDIFF(1,color);
    return 0;
}

static int
_bsg_params_state_differ(struct bsg_params_state *v1, struct bsg_params_state *v2)
{
    /* First, do sanity checks */
    if (!v1 && !v2)
	return -1;
    if ((v1 && !v2) || (!v1 && v2))
	return -1;

    BSG_NDIFF(1,draw);
    BSG_NDIFF(1,draw_size);
    BSG_NDIFF(1,draw_center);
    BSG_NDIFF(1,draw_az);
    BSG_NDIFF(1,draw_el);
    BSG_NDIFF(1,draw_tw);
    BSG_NDIFF(1,draw_fps);
    BSG_IVDIFF(1,color);
    BSG_NDIFF(1,font_size);
    return 0;
}

static int
_bsg_other_state_differ(struct bsg_other_state *v1, struct bsg_other_state *v2)
{
    /* First, do sanity checks */
    if (!v1 && !v2)
	return -1;
    if ((v1 && !v2) || (!v1 && v2))
	return -1;

    BSG_NDIFF(1,gos_draw);
    BSG_IVDIFF(1,gos_line_color);
    BSG_IVDIFF(1,gos_text_color);

    return 0;
}


static int
_bsg_interactive_rect_state_differ(struct bsg_interactive_rect_state *v1, struct bsg_interactive_rect_state *v2)
{
    /* First, do sanity checks */
    if (!v1 && !v2)
	return -1;
    if ((v1 && !v2) || (!v1 && v2))
	return -1;

    BSG_NDIFF(1,active);
    BSG_NDIFF(1,draw);
    BSG_NDIFF(1,line_width);
    BSG_NDIFF(1,line_style);
    BSG_NDIFF(1,pos[0]);
    BSG_NDIFF(1,pos[1]);
    BSG_NDIFF(1,dim[0]);
    BSG_NDIFF(1,dim[1]);
    BSG_NDIFF(1,x);
    BSG_NDIFF(1,y);
    BSG_NDIFF(1,width);
    BSG_NDIFF(1,height);
    BSG_IVDIFF(1,bg);
    BSG_IVDIFF(1,color);
    BSG_NDIFF(1,cdim[0]);
    BSG_NDIFF(1,cdim[1]);
    BSG_NDIFF(1,aspect);

    return 0;
}

static int
_bsg_settings_differ(struct bsg_view_settings *v1, struct bsg_view_settings *v2)
{
    BSG_CDIFF(1, _bsg_adc_state_differ, gv_adc);
    BSG_CDIFF(1, _bsg_axes_differ, gv_model_axes);
    BSG_CDIFF(1, _bsg_axes_differ, gv_view_axes);
    BSG_CDIFF(1, _bsg_grid_state_differ, gv_grid);
    BSG_CDIFF(1, _bsg_other_state_differ, gv_center_dot);
    BSG_CDIFF(1, _bsg_params_state_differ, gv_view_params);
    BSG_CDIFF(1, _bsg_other_state_differ, gv_view_scale);
    BSG_CDIFF(1, _bsg_interactive_rect_state_differ, gv_rect);

    BSG_NDIFF(1,gv_snap_lines);
    BSG_NDIFF(1,gv_snap_tol_factor);
    BSG_NDIFF(1,gv_cleared);
    BSG_NDIFF(1,gv_zclip);

    BSG_NDIFF(1,lod_source_policy.policy);
    BSG_NDIFF(1,lod_source_policy.forced_level);
    BSG_NDIFF(1,lod_source_policy.mesh_enabled);
    BSG_NDIFF(1,lod_source_policy.csg_enabled);
    BSG_NDIFF(1,lod_source_policy.zoom_refresh);
    BSG_NDIFF(1,lod_source_policy.scale);
    BSG_NDIFF(1,lod_source_policy.point_scale);
    BSG_NDIFF(1,lod_source_policy.curve_scale);
    BSG_NDIFF(1,lod_source_policy.bot_threshold);

    return 0;
}


int
bsg_differ(struct bsg_view *v1, struct bsg_view *v2)
{
    /* First, do sanity checks */
    if (!v1 && !v2)
	return -1;
    if ((v1 && !v2) || (!v1 && v2))
	return -1;

    /* Now the real work starts - check the contents.  First up are
     * any settings which potentially impact visible data */
    BSG_NDIFF(1,gv_i_scale);
    BSG_NDIFF(1,gv_a_scale);
    BSG_NDIFF(1,gv_scale);
    BSG_NDIFF(1,gv_scale);
    BSG_NDIFF(1,gv_size);
    BSG_NDIFF(1,gv_isize);
    BSG_NDIFF(1,gv_perspective);
    BSG_VDIFF(1,gv_aet);
    BSG_VDIFF(1,gv_eye_pos);
    BSG_VDIFF(1,gv_keypoint);
    BSG_DIFF(1,gv_coord);
    BSG_DIFF(1,gv_rotate_about);
    BSG_MDIFF(1,gv_rotation);
    BSG_MDIFF(1,gv_center);
    BSG_MDIFF(1,gv_model2view);
    BSG_MDIFF(1,gv_pmodel2view);
    BSG_MDIFF(1,gv_view2model);
    BSG_MDIFF(1,gv_pmat);

    BSG_NDIFF(1,gv_width);
    BSG_NDIFF(1,gv_height);
    BSG_NDIFF(1,gv_prevMouseX);
    BSG_NDIFF(1,gv_prevMouseY);
    BSG_NDIFF(1,gv_minMouseDelta);
    BSG_NDIFF(1,gv_maxMouseDelta);
    BSG_NDIFF(1,gv_rscale);
    BSG_NDIFF(1,gv_sscale);

    /* Renderable TclCAD overlays are view-scoped BSG features whose revisions
     * drive redraws.  The view-vZ scalar is command scratch and does not
     * contribute to rendering differences. */

    if (v1->settings_active != v2->settings_active) {
	return 1;
    }

    if (v1->settings_active) {
	if (_bsg_settings_differ(v1->settings_active, v2->settings_active)) {
	    return 1;
	}
    }

    if ((v1->settings_local && !v2->settings_local) || (!v1->settings_local && v2->settings_local)) {
	return 1;
    }
    if (v1->settings_local && _bsg_settings_differ(v1->settings_local, v2->settings_local)) {
	return 1;
    }

    BSG_DIFF(3,gv_callback);
    BSG_DIFF(3,gv_clientData);
    BSG_DIFF(3,dmp);
    BSG_DIFF(3,u_data);

    BSG_DIFF(3,callbacks);
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
