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

	k->vs_rateflag_model_tran = 0;
	VSETALL(k->vs_rate_model_tran, 0.0);

	k->vs_rateflag_model_rotate = 0;
	VSETALL(k->vs_rate_model_rotate, 0.0);

	k->vs_rateflag_tran = 0;
	VSETALL(k->vs_rate_tran, 0.0);

	k->vs_rateflag_rotate = 0;
	VSETALL(k->vs_rate_rotate, 0.0);

	k->vs_rateflag_scale = 0;
	k->vs_rate_scale = 0.0;


    }

    /* Absolute members */
    if (!category || category == 2) {

	VSETALL(k->vs_absolute_tran, 0.0);
	VSETALL(k->vs_last_absolute_tran, 0.0);

	VSETALL(k->vs_absolute_model_tran, 0.0);
	VSETALL(k->vs_last_absolute_model_tran, 0.0);

	VSETALL(k->vs_absolute_rotate, 0.0);
	VSETALL(k->vs_last_absolute_rotate, 0.0);

	VSETALL(k->vs_absolute_model_rotate, 0.0);
	VSETALL(k->vs_last_absolute_model_rotate, 0.0);

	k->vs_absolute_scale = 0.0;

	/* Virtual trackball position */
	MAT_DELTAS_GET_NEG(v->k.vs_orig_pos, v->gv_center);

	v->gv_i_scale = v->gv_scale;
	v->gv_a_scale = 0.0;
    }

}

static void
set_absolute_view_tran(struct bview *v)
{
    /* calculate absolute_tran */
    MAT4X3PNT(v->k.vs_absolute_tran, v->gv_model2view, v->k.vs_orig_pos);
    /* Stash the current vs_absolute_tran value in case vs_absolute_tran is
     * overwritten (say by Tcl in MGED) */
    VMOVE(v->k.vs_last_absolute_tran, v->k.vs_absolute_tran);
}

static void
set_absolute_model_tran(struct bview *v)
{
    point_t new_pos;
    point_t diff;

    /* calculate absolute_model_tran */
    MAT_DELTAS_GET_NEG(new_pos, v->gv_center);
    VSUB2(diff, v->k.vs_orig_pos, new_pos);
    VSCALE(v->k.vs_absolute_model_tran, diff, 1/v->gv_scale);
    /* Stash the current vs_absolute_model_tran value in case
     * vs_absolute_model_tran is overwritten (say by Tcl in MGED) */
    VMOVE(v->k.vs_last_absolute_model_tran, v->k.vs_absolute_model_tran);
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

    if (!ZERO(v->k.vs_absolute_tran[X])
        || !ZERO(v->k.vs_absolute_tran[Y])
        || !ZERO(v->k.vs_absolute_tran[Z]))
    {
	set_absolute_view_tran(v);
	set_absolute_model_tran(v);
    }
}

int
bv_knob_cmd_process(
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
		coord = &v->k.vs_rate_model_rotate[ind];
	    } else {
		coord = &v->k.vs_rate_rotate[ind];
	    }

	    if (incr_flag) {
		*coord += f;
	    } else {
		*coord = f;
	    }

	    if (model_flag) {
		v->k.vs_rate_model_origin = origin;
	    } else {
		v->k.vs_rate_origin = origin;
	    }

	    return BRLCAD_OK;
	}

	// Translate cases
	if (cmd[0] == 'X' || cmd[0] == 'Y' || cmd[0] == 'Z') {

	    fastf_t *coord;

	    if (model_flag) {
		coord = &v->k.vs_rate_model_tran[ind];
	    } else {
		coord = &v->k.vs_rate_tran[ind];
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
		v->k.vs_rate_scale += f;
	    } else {
		v->k.vs_rate_scale = f;
	    }

	    return BRLCAD_OK;
	}

	return BRLCAD_ERROR;
    }

    // Absolute cases
    if (cmd[0] == 'a' && strlen(cmd) == 2) {

	// Rotate cases
	if (cmd[1] == 'x' || cmd[1] == 'y' || cmd[1] == 'z') {

	    fastf_t *vamr_c = &v->k.vs_absolute_model_rotate[ind];
	    fastf_t *var_c = &v->k.vs_absolute_rotate[ind];
	    fastf_t *vlamr_c = &v->k.vs_last_absolute_model_rotate[ind];
	    fastf_t *vlar_c = &v->k.vs_last_absolute_rotate[ind];
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
		arp = v->k.vs_absolute_model_rotate;
		larp = v->k.vs_last_absolute_model_rotate;
	    } else {
		arp = v->k.vs_absolute_rotate;
		larp = v->k.vs_last_absolute_rotate;
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

	    fastf_t *vamt_c = &v->k.vs_absolute_model_tran[ind];
	    fastf_t *vat_c = &v->k.vs_absolute_tran[ind];
	    fastf_t *vlamt_c = &v->k.vs_last_absolute_model_tran[ind];
	    fastf_t *vlat_c = &v->k.vs_last_absolute_tran[ind];
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
