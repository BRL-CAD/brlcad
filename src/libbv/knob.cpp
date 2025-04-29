/*                      K N O B . C P P
 * BRL-CAD
 *
 * Copyright (c) 2020-2025 United States Government as represented by
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
/** @file knob.cpp
 *
 * Utility functions for working with BRL-CAD view knob functionality
 *
 */

#include "common.h"
#include <string.h>
#include "vmath.h"
#include "bn/mat.h"
#include "bv/defines.h"
#include "bv/util.h"
#include "./bv_private.h"

void
bv_knobs_reset(struct bview *v, int category)
{

    if (!v)
	return;

    struct bview_knob *k = &v->k;

    /* Rate members */
    if (!category || category == 1) {

	k->rot_m_flag = 0;
	VSETALL(k->rot_m, 0.0);

	k->rot_o_flag = 0;
	VSETALL(k->rot_o, 0.0);

	k->rot_v_flag = 0;
	VSETALL(k->rot_v, 0.0);

	k->tra_m_flag = 0;
	VSETALL(k->tra_m, 0.0);

	k->tra_v_flag = 0;
	VSETALL(k->tra_v, 0.0);

	k->sca_flag = 0;
	k->sca = 0.0;


    }

    /* Absolute members */
    if (!category || category == 2) {

	VSETALL(k->rot_m_abs, 0.0);
	VSETALL(k->rot_m_abs_last, 0.0);

	VSETALL(k->rot_o_abs, 0.0);
	VSETALL(k->rot_o_abs_last, 0.0);

	VSETALL(k->rot_v_abs, 0.0);
	VSETALL(k->rot_v_abs_last, 0.0);

	VSETALL(k->tra_m_abs, 0.0);
	VSETALL(k->tra_m_abs_last, 0.0);

	VSETALL(k->tra_v_abs, 0.0);
	VSETALL(k->tra_v_abs_last, 0.0);

	k->sca_abs = 0.0;

	/* Virtual trackball position */
	MAT_DELTAS_GET_NEG(v->k.orig_pos, v->gv_center);

	v->gv_i_scale = v->gv_scale;
	v->gv_a_scale = 0.0;
    }

}

static void
set_absolute_view_tran(struct bview *v)
{
    /* calculate absolute_tran */
    MAT4X3PNT(v->k.tra_v_abs, v->gv_model2view, v->k.orig_pos);
    /* Stash the current tra_v_abs value in case tra_v_abs is
     * overwritten (say by Tcl in MGED) */
    VMOVE(v->k.tra_v_abs_last, v->k.tra_v_abs);
}

static void
set_absolute_model_tran(struct bview *v)
{
    point_t new_pos;
    point_t diff;

    /* calculate absolute_model_tran */
    MAT_DELTAS_GET_NEG(new_pos, v->gv_center);
    VSUB2(diff, v->k.orig_pos, new_pos);
    VSCALE(v->k.tra_m_abs, diff, 1/v->gv_scale);
    /* Stash the current tra_m_abs value in case
     * tra_m_abs is overwritten (say by Tcl in MGED) */
    VMOVE(v->k.tra_m_abs_last, v->k.tra_m_abs);
}

static void
abs_zoom(struct bview *v)
{
    /* Use initial Viewscale */
    if (-SMALL_FASTF < v->gv_a_scale && v->gv_a_scale < SMALL_FASTF) {
        v->gv_scale = v->gv_i_scale;
    } else {
        /* if positive */
        if (v->gv_a_scale > 0) {
            /* scale i_Viewscale by values in [0.0, 1.0] range */
            v->gv_scale = v->gv_i_scale * (1.0 - v->gv_a_scale);
        } else/* negative */
            /* scale i_Viewscale by values in [1.0, 10.0] range */
        {
            v->gv_scale = v->gv_i_scale * (1.0 + (v->gv_a_scale * -9.0));
        }
    }

    if (v->gv_scale < BV_MINVIEWSCALE)
	v->gv_scale = BV_MINVIEWSCALE;

    v->gv_size = 2.0 * v->gv_scale;
    v->gv_isize = 1.0 / v->gv_size;
    bv_update(v);

    if (!ZERO(v->k.tra_v_abs[X])
        || !ZERO(v->k.tra_v_abs[Y])
        || !ZERO(v->k.tra_v_abs[Z]))
    {
	set_absolute_view_tran(v);
	set_absolute_model_tran(v);
    }
}

static void
calc_mtran(struct bview *v, const vect_t *tvec)
{
    point_t delta;
    point_t vc, nvc;

    VSCALE(delta, *tvec, v->gv_local2base);
    MAT_DELTAS_GET_NEG(vc, v->gv_center);
    VSUB2(nvc, vc, delta);
    MAT_DELTAS_VEC_NEG(v->gv_center, nvc);
    bv_update(v);

    /* calculate absolute_tran */
    set_absolute_view_tran(v);
}

static void
calc_vtran(struct bview *v, const vect_t *tvec)
{
    vect_t tt;
    point_t delta;
    point_t work;
    point_t vc, nvc;

    VSCALE(tt, *tvec, v->gv_local2base / v->gv_scale);
    MAT4X3PNT(work, v->gv_view2model, tt);
    MAT_DELTAS_GET_NEG(vc, v->gv_center);
    VSUB2(delta, work, vc);
    VSUB2(nvc, vc, delta);
    MAT_DELTAS_VEC_NEG(v->gv_center, nvc);

    bv_update(v);

    /* calculate absolute_model_tran */
    set_absolute_model_tran(v);
}

void
bv_knobs_tran(struct bview *v,
	vect_t *tvec,
	int model_flag)
{
    if (!v || !tvec)
	return;
    if (model_flag) {
	calc_mtran(v, tvec);
    } else if (v->gv_coord == 'o') {
	vect_t work = VINIT_ZERO;
	calc_mtran(v, &work);
    } else {
	calc_vtran(v, tvec);
    }
}

static void
vrot(struct bview *v, char origin, fastf_t *newrot)
{
    mat_t newinv;

    bn_mat_inv(newinv, newrot);

    if (origin != 'v') {
        point_t rot_pt;
        point_t new_origin;
        mat_t viewchg, viewchginv;
        point_t new_cent_view;
        point_t new_cent_model;

        if (origin == 'e') {
            /* "VR driver" method: rotate around "eye" point (0, 0, 1) viewspace */
            VSET(rot_pt, 0.0, 0.0, 1.0);                /* point to rotate around */
        } else {
            /* rotate around model center (0, 0, 0) */
            VSET(new_origin, 0.0, 0.0, 0.0);
            MAT4X3PNT(rot_pt, v->gv_model2view, new_origin);  /* point to rotate around */
        }

        bn_mat_xform_about_pnt(viewchg, newrot, rot_pt);
        bn_mat_inv(viewchginv, viewchg);

        /* Convert origin in new (viewchg) coords back to old view coords */
        VSET(new_origin, 0.0, 0.0, 0.0);
        MAT4X3PNT(new_cent_view, viewchginv, new_origin);
        MAT4X3PNT(new_cent_model, v->gv_view2model, new_cent_view);
        MAT_DELTAS_VEC_NEG(v->gv_center, new_cent_model);
    }

    /* Update the rotation component of the model2view matrix */
    bn_mat_mul2(newrot, v->gv_rotation); /* pure rotation */
    bv_update(v);

    set_absolute_view_tran(v);
    set_absolute_model_tran(v);
}


static void
vrot_xyz(struct bview *v,
              char origin,
              int model2view,
              vect_t *rvec)
{
    mat_t newrot;
    mat_t temp1, temp2;

    MAT_IDN(newrot);
    bn_mat_angles(newrot, (*rvec)[X], (*rvec)[Y], (*rvec)[Z]);

    if (model2view) {
        /* transform model rotations into view rotations */
        bn_mat_inv(temp1, v->gv_rotation);
        bn_mat_mul(temp2, v->gv_rotation, newrot);
        bn_mat_mul(newrot, temp2, temp1);
    }

    vrot(v, origin, newrot);
}


void
bv_knobs_rot(struct bview *v,
	vect_t *rvec,
	char origin,
	int model_flag)
{
    if (!v || !rvec)
	return;
    int model2view = 0;
    if (model_flag)
	model2view = 1;
    vrot_xyz(v, origin, model2view, rvec);
}

void
bv_update_rate_flags(struct bview *v)
{
    if (!ZERO(v->k.rot_m[X])
	|| !ZERO(v->k.rot_m[Y])
	|| !ZERO(v->k.rot_m[Z]))
    {
	v->k.rot_m_flag = 1;
    } else {
	v->k.rot_m_flag = 0;
    }

    if (!ZERO(v->k.tra_m[X])
	|| !ZERO(v->k.tra_m[Y])
	|| !ZERO(v->k.tra_m[Z]))
    {
	v->k.tra_m_flag = 1;
    } else {
	v->k.tra_m_flag = 0;
    }

    if (!ZERO(v->k.rot_v[X])
	|| !ZERO(v->k.rot_v[Y])
	|| !ZERO(v->k.rot_v[Z]))
    {
	v->k.rot_v_flag = 1;
    } else {
	v->k.rot_v_flag = 0;
    }

    if (!ZERO(v->k.tra_v[X])
	|| !ZERO(v->k.tra_v[Y])
	|| !ZERO(v->k.tra_v[Z]))
    {
	v->k.tra_v_flag = 1;
    } else {
	v->k.tra_v_flag = 0;
    }

    if (!ZERO(v->k.sca)) {
	v->k.sca_flag = 1;
    } else {
	v->k.sca_flag = 0;
    }
}

int
bv_knobs_cmd_process(
	vect_t *rvec, int *do_rot, vect_t *tvec, int *do_tran,
	struct bview *v, const char *cmd, fastf_t f,
	char origin, int model_flag, int incr_flag)
{
    if (!v || !cmd || !rvec || !do_rot || !tvec || !do_tran)
	return BRLCAD_ERROR;

    // Get the index we'll be working with
    int ind = -1;
    char c = (cmd[1] == '\0') ? cmd[0] : cmd[1];
    switch (c) {
	case 'x':
	case 'X':
	    ind = X;
	    break;
	case 'y':
	case 'Y':
	    ind = Y;
	    break;
	case 'z':
	case 'Z':
	    ind = Z;
	    break;
	case 'S':
	    ind = 0;
	    break;
    }

    // If we couldn't get a valid index, this isn't a valid command
    if (ind < 0)
	return BRLCAD_ERROR;

    // Handle the various rate and abs commands
    if (cmd[1] == '\0') {

	// Rotate cases
	if (cmd[0] == 'x' || cmd[0] == 'y' || cmd[0] == 'z') {

	    fastf_t *coord;

	    if (model_flag) {
		coord = &v->k.rot_m[ind];
	    } else {
		coord = &v->k.rot_v[ind];
	    }

	    if (incr_flag) {
		*coord += f;
	    } else {
		*coord = f;
	    }

	    if (model_flag) {
		v->k.origin_m = origin;
	    } else {
		v->k.origin_v = origin;
	    }

	    return BRLCAD_OK;
	}

	// Translate cases
	if (cmd[0] == 'X' || cmd[0] == 'Y' || cmd[0] == 'Z') {

	    fastf_t *coord;

	    if (model_flag) {
		coord = &v->k.tra_m[ind];
	    } else {
		coord = &v->k.tra_v[ind];
	    }

	    if (incr_flag) {
		*coord += f;
	    } else {
		*coord = f;
	    }

	    return BRLCAD_OK;
	}

	// Scale case
	if (cmd[0] == 'S') {

	    if (incr_flag) {
		v->k.sca += f;
	    } else {
		v->k.sca = f;
	    }

	    return BRLCAD_OK;
	}

	return BRLCAD_ERROR;
    }

    // Absolute cases
    if (cmd[0] == 'a' && strlen(cmd) == 2) {

	// Rotate cases
	if (cmd[1] == 'x' || cmd[1] == 'y' || cmd[1] == 'z') {

	    fastf_t *vamr_c = &v->k.rot_m_abs[ind];
	    fastf_t *var_c = &v->k.rot_v_abs[ind];
	    fastf_t *vlamr_c = &v->k.rot_m_abs_last[ind];
	    fastf_t *vlar_c = &v->k.rot_v_abs_last[ind];
	    fastf_t *rvec_c = &(*rvec)[ind];

	    if (incr_flag) {
		if (model_flag) {
		    *vamr_c += f;
		} else {
		    *var_c += f;
		}
		*rvec_c = f;
	    } else {
		if (model_flag) {
		    *rvec_c = f - *vlamr_c;
		    *vamr_c = f;
		} else {
		    *rvec_c = f - *vlar_c;
		    *var_c = f;
		}
	    }

	    /* wrap around */
	    fastf_t *arp;
	    fastf_t *larp;

	    if (model_flag) {
		arp = v->k.rot_m_abs;
		larp = v->k.rot_m_abs_last;
	    } else {
		arp = v->k.rot_v_abs;
		larp = v->k.rot_v_abs_last;
	    }

	    if (arp[ind] < -180.0) {
		arp[ind] = arp[ind] + 360.0;
	    } else if (arp[ind] > 180.0) {
		arp[ind] = arp[ind] - 360.0;
	    }
	    larp[ind] = arp[ind];

	    *do_rot = 1;

	    return BRLCAD_OK;
	}

	// Translate cases
	if (cmd[1] == 'X' || cmd[1] == 'Y' || cmd[1] == 'Z') {

	    fastf_t *vamt_c = &v->k.tra_m_abs[ind];
	    fastf_t *vat_c = &v->k.tra_v_abs[ind];
	    fastf_t *vlamt_c = &v->k.tra_m_abs_last[ind];
	    fastf_t *vlat_c = &v->k.tra_v_abs_last[ind];
	    fastf_t *tvec_c = &(*tvec)[ind];
	    fastf_t sf = f * v->gv_local2base / v->gv_scale;

	    if (incr_flag) {
		if (model_flag) {
		    *vamt_c += sf;
		    *vlamt_c = *vamt_c;
		} else {
		    *vat_c += sf;
		    *vlat_c = *vat_c;
		}

		*tvec_c = f;
	    } else {
		if (model_flag) {
		    *tvec_c = f - *vlamt_c * v->gv_scale * v->gv_base2local;
		    *vamt_c = sf;
		    *vlamt_c = *vamt_c;
		} else {
		    *tvec_c = f - *vlat_c * v->gv_scale * v->gv_base2local;
		    *vat_c = sf;
		    *vlat_c = *vat_c;
		}

	    }

	    *do_tran = 1;

	    return BRLCAD_OK;
	}

	// Scale case
	if (cmd[1] == 'S') {
	    if (incr_flag) {
		v->gv_a_scale += f;
	    } else {
		v->gv_a_scale = f;
	    }
	    abs_zoom(v);
	    return BRLCAD_OK;
	}

	return BRLCAD_ERROR;
    }

    // Invalid command form
    return BRLCAD_ERROR;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
