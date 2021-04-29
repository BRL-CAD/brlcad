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
#include "bu/malloc.h"
#include "bu/ptbl.h"
#include "bu/str.h"
#include "bn/mat.h"
#include "bn/vlist.h"
#include "bview/defines.h"
#include "bview/util.h"

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

    gvp->gv_cleared = 1;

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
    gvp->gv_view_axes.label_flag = 1;
    VSET(gvp->gv_view_axes.label_color, 255, 255, 0);
    gvp->gv_view_axes.triple_color = 1;

    gvp->gv_model_axes.draw = 0;
    VSET(gvp->gv_model_axes.axes_pos, 0.0, 0.0, 0.0);
    gvp->gv_model_axes.axes_size = 2.0;
    gvp->gv_model_axes.line_width = 0;
    gvp->gv_model_axes.pos_only = 0;
    VSET(gvp->gv_model_axes.axes_color, 255, 255, 255);
    gvp->gv_model_axes.label_flag = 1;
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

    gvp->gv_fps = 0;
    gvp->gv_frametime = 1;

    gvp->gv_data_vZ = 0.0;
    gvp->gv_autoview = 1;

    /* FIXME: this causes the shaders.sh regression to fail */
    /* _ged_mat_aet(gvp); */

    // Higher values indicate more aggressive behavior (i.e. points further away will be snapped).
    gvp->gv_snap_tol_factor = 10;
    gvp->gv_snap_lines = 0;

    BU_GET(gvp->gv_scene_objs, struct bu_ptbl);
    bu_ptbl_init(gvp->gv_scene_objs, 8, "scene_objs init");
    BU_GET(gvp->gv_selected, struct bu_ptbl);
    bu_ptbl_init(gvp->gv_selected, 8, "scene_objs init");

    BU_LIST_INIT(&gvp->gv_vlfree);

    bview_update(gvp);
}

// TODO - investigate saveview/loadview logic, see if anything
// makes sense to move here
void
bview_sync(struct bview *dest, struct bview *src)
{
    if (!src || !dest)
	return;

    /* Size info */
    dest->gv_i_scale = src->gv_i_scale;
    dest->gv_a_scale = src->gv_a_scale;
    dest->gv_scale = src->gv_scale;
    dest->gv_size = src->gv_size;
    dest->gv_isize = src->gv_isize;
    dest->gv_width = src->gv_width;
    dest->gv_height = src->gv_height;
    dest->gv_base2local = src->gv_base2local;
    dest->gv_rscale = src->gv_rscale;
    dest->gv_sscale = src->gv_sscale;

    /* Camera info */
    dest->gv_perspective = src->gv_perspective;
    VMOVE(dest->gv_aet, src->gv_aet);
    VMOVE(dest->gv_eye_pos, src->gv_eye_pos);
    VMOVE(dest->gv_keypoint, src->gv_keypoint);
    dest->gv_coord = src->gv_coord;
    dest->gv_rotate_about = src->gv_rotate_about;
    MAT_COPY(dest->gv_rotation, src->gv_rotation);
    MAT_COPY(dest->gv_center, src->gv_center);
    MAT_COPY(dest->gv_model2view, src->gv_model2view);
    MAT_COPY(dest->gv_pmodel2view, src->gv_pmodel2view);
    MAT_COPY(dest->gv_view2model, src->gv_view2model);
    MAT_COPY(dest->gv_pmat, src->gv_pmat);
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

void
bview_settings_sync(struct bview_settings *dest, struct bview_settings *src)
{
    dest->mode = src->mode;
    dest->hidden = src->hidden;
    dest->s_line_width = src->s_line_width;
    dest->s_arrow_tip_length = src->s_arrow_tip_length;
    dest->s_arrow_tip_width = src->s_arrow_tip_width;
    dest->s_hiddenLine = src->s_hiddenLine;
    dest->transparency = src->transparency;
    dest->s_dmode = src->s_dmode;
    dest->color_override = src->color_override;
    VMOVE(dest->color, src->color);
    dest->dmode = src->dmode;
    dest->shaded_mode_override = src->shaded_mode_override;
    dest->hiddenLine = src->hiddenLine;
    dest->draw_wireframes = src->draw_wireframes;
    dest->draw_solid_lines_only = src->draw_solid_lines_only;
    dest->draw_non_subtract_only = src->draw_non_subtract_only;
    dest->adaptive_plot = src->adaptive_plot;
    dest->redraw_on_zoom = src->redraw_on_zoom;
    dest->x_samples = src->x_samples;
    dest->y_samples = src->y_samples;
    dest->point_scale = src->point_scale;
    dest->curve_scale = src->curve_scale;
    dest->bot_threshold = src->bot_threshold;
}

int
bview_update_selected(struct bview *gvp)
{
    int ret = 0;
    if (!gvp)
	return 0;

    for(size_t i = 0; i < BU_PTBL_LEN(gvp->gv_selected); i++) {
	struct bview_scene_obj *s = (struct bview_scene_obj *)BU_PTBL_GET(gvp->gv_selected, i);
	if (s->s_update_callback) {
	    ret += (*s->s_update_callback)(s);
	}
    }

    return (ret > 0) ? 1 : 0;
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
_bview_trans(struct bview *v, int dx, int dy, point_t UNUSED(keypoint), unsigned long long UNUSED(flags))
{
    if (!v)
	return 0;
    fastf_t aspect = (fastf_t)v->gv_width / (fastf_t)v->gv_height;
    fastf_t fx = (fastf_t)dx / (fastf_t)v->gv_width * 2.0;
    fastf_t fy = -dy / (fastf_t)v->gv_height / aspect * 2.0;

    vect_t tt;
    point_t delta;
    point_t work;
    point_t vc, nvc;

    VSET(tt, fx, fy, 0);
    MAT4X3PNT(work, v->gv_view2model, tt);
    MAT_DELTAS_GET_NEG(vc, v->gv_center);
    VSUB2(delta, work, vc);
    VSUB2(nvc, vc, delta);
    MAT_DELTAS_VEC_NEG(v->gv_center, nvc);
    bview_update(v);

    return 1;
}

int
_bview_scale(struct bview *v, int sensitivity, int factor, point_t UNUSED(keypoint), unsigned long long UNUSED(flags))
{
    double f = (double)factor/(double)sensitivity;

    v->gv_scale = v->gv_scale * (1.0 - f);

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
bview_adjust(struct bview *v, int dx, int dy, point_t keypoint, int UNUSED(mode), unsigned long long flags)
{
    if (flags == BVIEW_IDLE)
	return 0;

    // TODO - figure out why these need to be flipped for qdm to do the right thing...
    if (flags & BVIEW_ROT)
	return _bview_rot(v, dy, dx, keypoint, flags);

    if (flags & BVIEW_TRANS)
	return _bview_trans(v, dx, dy, keypoint, flags);

    if (flags & BVIEW_SCALE)
	return _bview_scale(v, dx, dy, keypoint, flags);

    return 0;
}


int
bview_screen_to_view(struct bview *v, fastf_t *fx, fastf_t *fy, fastf_t x, fastf_t y)
{
    if (!v)
	return -1;

    if (!v->gv_width || !v->gv_height)
	return -1;

    if (fx) {
	fastf_t tx = x / (fastf_t)v->gv_width * 2.0 - 1.0;
	(*fx) = tx;
    }

    if (fy) {
	fastf_t aspect = (fastf_t)v->gv_width / (fastf_t)v->gv_height;
	fastf_t ty = (y / (fastf_t)v->gv_height * -2.0 + 1.0) / aspect;
	(*fy) = ty;
    }

    return 0;
}

void
bview_scene_obj_free(struct bview_scene_obj *s)
{
    // handle children
    if (BU_PTBL_IS_INITIALIZED(&s->children)) {
	for (size_t i = 0; i < BU_PTBL_LEN(&s->children); i++) {
	    struct bview_scene_obj *s_c = (struct bview_scene_obj *)BU_PTBL_GET(&s->children, i);
	    bview_scene_obj_free(s_c);
	    BU_PUT(s_c, struct bview_scene_obj);
	}
    }

    // free vlist
    if (BU_LIST_IS_INITIALIZED(&s->s_vlist)) {
	BN_FREE_VLIST(&s->s_v->gv_vlfree, &s->s_vlist);
    }

    // free child table
    if (BU_PTBL_IS_INITIALIZED(&s->children)) {
	bu_ptbl_free(&s->children);
    }
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
