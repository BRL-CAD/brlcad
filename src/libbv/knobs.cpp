/*                      K N O B S . C P P
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
/** @file knobs.cpp
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
bv_knobs_reset(struct bview_knobs *k, int category)
{

    if (!k)
	return;

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

    }

}

unsigned long long
bv_knobs_hash(struct bview_knobs *k, struct bu_data_hash_state *state)
{
    if (!k)
	return 0ULL;

    int own_state = 0;
    if (!state) {
	state = bu_data_hash_create();
	if (!state)
	    return 0ULL;
	own_state = 1;
    }

    /* Rate rotations */
    bu_data_hash_update(state, &k->rot_m, sizeof(k->rot_m));
    bu_data_hash_update(state, &k->rot_m_flag, sizeof(k->rot_m_flag));
    bu_data_hash_update(state, &k->origin_m, sizeof(k->origin_m));
    bu_data_hash_update(state, &k->rot_o, sizeof(k->rot_o));
    bu_data_hash_update(state, &k->rot_o_flag, sizeof(k->rot_o_flag));
    bu_data_hash_update(state, &k->origin_o, sizeof(k->origin_o));
    bu_data_hash_update(state, &k->rot_v, sizeof(k->rot_v));
    bu_data_hash_update(state, &k->rot_v_flag, sizeof(k->rot_v_flag));
    bu_data_hash_update(state, &k->origin_v, sizeof(k->origin_v));

    /* Rate scale */
    bu_data_hash_update(state, &k->sca, sizeof(k->sca));
    bu_data_hash_update(state, &k->sca_flag, sizeof(k->sca_flag));

    /* Rate translations */
    bu_data_hash_update(state, &k->tra_m, sizeof(k->tra_m));
    bu_data_hash_update(state, &k->tra_m_flag, sizeof(k->tra_m_flag));
    bu_data_hash_update(state, &k->tra_v, sizeof(k->tra_v));
    bu_data_hash_update(state, &k->tra_v_flag, sizeof(k->tra_v_flag));

    /* Absolute rotations */
    bu_data_hash_update(state, &k->rot_m_abs, sizeof(k->rot_m_abs));
    bu_data_hash_update(state, &k->rot_m_abs_last, sizeof(k->rot_m_abs_last));
    bu_data_hash_update(state, &k->rot_o_abs, sizeof(k->rot_o_abs));
    bu_data_hash_update(state, &k->rot_o_abs_last, sizeof(k->rot_o_abs_last));
    bu_data_hash_update(state, &k->rot_v_abs, sizeof(k->rot_v_abs));
    bu_data_hash_update(state, &k->rot_v_abs_last, sizeof(k->rot_v_abs_last));

    /* Absolute scale */
    bu_data_hash_update(state, &k->sca_abs, sizeof(k->sca_abs));

    /* Absolute translations */
    bu_data_hash_update(state, &k->tra_m_abs, sizeof(k->tra_m_abs));
    bu_data_hash_update(state, &k->tra_m_abs_last, sizeof(k->tra_m_abs_last));
    bu_data_hash_update(state, &k->tra_v_abs, sizeof(k->tra_v_abs));
    bu_data_hash_update(state, &k->tra_v_abs_last, sizeof(k->tra_v_abs_last));

    if (!own_state)
	return 0ULL;

    unsigned long long hv = bu_data_hash_val(state);
    bu_data_hash_destroy(state);
    return hv;
}


static void
set_absolute_view_tran(struct bview *v)
{
    /* calculate absolute_tran */
    MAT4X3PNT(v->k.tra_v_abs, v->gv_model2view, v->orig_pos);
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
    VSUB2(diff, v->orig_pos, new_pos);
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
calc_mtran(struct bview *v, const vect_t tvec)
{
    point_t delta;
    point_t vc, nvc;

    VSCALE(delta, tvec, v->gv_local2base);
    MAT_DELTAS_GET_NEG(vc, v->gv_center);
    VSUB2(nvc, vc, delta);
    MAT_DELTAS_VEC_NEG(v->gv_center, nvc);
    bv_update(v);

    /* calculate absolute_tran */
    set_absolute_view_tran(v);
}

static void
calc_vtran(struct bview *v, const vect_t tvec)
{
    vect_t tt;
    point_t delta;
    point_t work;
    point_t vc, nvc;

    VSCALE(tt, tvec, v->gv_local2base / v->gv_scale);
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
	const vect_t tvec,
	int model_flag)
{
    if (!v || !tvec)
	return;
    if (model_flag) {
	calc_mtran(v, tvec);
    } else if (v->gv_coord == 'o') {
	vect_t work = VINIT_ZERO;
	calc_mtran(v, work);
    } else {
	calc_vtran(v, tvec);
    }
}

void
bv_knobs_rot(struct bview *v,
    const vect_t rvec,
    char origin,
    char coords,
    const matp_t obj_rot,
    const pointp_t pvt_pt)
{
    if (!v || !rvec)
        return;

    /* 1. Build base rotation from Euler angles (view-space candidate) */
    mat_t base;
    MAT_IDN(base);
    bn_mat_angles(base, rvec[X], rvec[Y], rvec[Z]);

    /* 2. Convert to final view-space rotation based on coords */
    mat_t view_rot;
    MAT_IDN(view_rot);

    if (coords == 'm') {
        mat_t t1, rvinv;
        bn_mat_inv(rvinv, v->gv_rotation);
        bn_mat_mul(t1, v->gv_rotation, base);
        bn_mat_mul(view_rot, t1, rvinv);
    } else if (coords == 'o') {
        if (obj_rot) {
            mat_t obj_inv, model_rot, t2, rvinv, t1;
            bn_mat_inv(obj_inv, obj_rot);
            bn_mat_mul(t2, obj_rot, base);
            bn_mat_mul(model_rot, t2, obj_inv);
            bn_mat_inv(rvinv, v->gv_rotation);
            bn_mat_mul(t1, v->gv_rotation, model_rot);
            bn_mat_mul(view_rot, t1, rvinv);
        } else {
            /* Fallback: treat as view-space rotation */
            MAT_COPY(view_rot, base);
        }
    } else {
        /* 'v' or unrecognized => direct application */
        MAT_COPY(view_rot, base);
    }

    /* 3. Determine pivot in view space if origin requires recentering */
    int recenter = 0;
    point_t pivot_view;

    switch (origin) {
        case 'v':
            VSET(pivot_view, 0.0, 0.0, 0.0);
            break;
        case 'e':
            VSET(pivot_view, 0.0, 0.0, 1.0);
            recenter = 1;
            break;
        case 'm': {
            point_t mzero = {0.0, 0.0, 0.0};
            MAT4X3PNT(pivot_view, v->gv_model2view, mzero);
            recenter = 1;
            break;
        }
        case 'k': {
            point_t mp;
            if (pvt_pt) {
                VMOVE(mp, pvt_pt);
            } else {
                VSET(mp, 0.0, 0.0, 0.0);
            }
            MAT4X3PNT(pivot_view, v->gv_model2view, mp);
            recenter = 1;
            break;
        }
        default:
            /* Fallback to view center */
            VSET(pivot_view, 0.0, 0.0, 0.0);
            break;
    }

    if (recenter) {
        mat_t about, about_inv;
        bn_mat_xform_about_pnt(about, view_rot, pivot_view);
        bn_mat_inv(about_inv, about);

        point_t new_origin = {0.0, 0.0, 0.0};
        point_t new_cent_view, new_cent_model;
        MAT4X3PNT(new_cent_view, about_inv, new_origin);
        MAT4X3PNT(new_cent_model, v->gv_view2model, new_cent_view);
        MAT_DELTAS_VEC_NEG(v->gv_center, new_cent_model);
    }

    /* 4. Apply rotation */
    bn_mat_mul2(view_rot, v->gv_rotation);
    bv_update(v);

    /* 5. Refresh absolute translations  */
    set_absolute_view_tran(v);
    set_absolute_model_tran(v);
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
