/*                         K N O B . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2025 United States Government as represented by
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
/** @file libged/view/knob.c
 *
 * The view knob subcommand.
 *
 */

#include "common.h"

#include <string.h>
#include "../ged_private.h"
#include "./ged_view.h"

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

static void
knob_tran(struct bview *v,
	vect_t tvec,
	int model_flag)
{
    if (model_flag) {
	calc_mtran(v, tvec);
    } else if (v->gv_coord == 'o') {
	vect_t work = VINIT_ZERO;
	calc_mtran(v, work);
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
              vect_t rvec)
{
    mat_t newrot;
    mat_t temp1, temp2;

    MAT_IDN(newrot);
    bn_mat_angles(newrot, rvec[X], rvec[Y], rvec[Z]);

    if (model2view) {
        /* transform model rotations into view rotations */
        bn_mat_inv(temp1, v->gv_rotation);
        bn_mat_mul(temp2, v->gv_rotation, newrot);
        bn_mat_mul(newrot, temp2, temp1);
    }

    vrot(v, origin, newrot);
}


static void
knob_rot(struct bview *v,
	vect_t rvec,
	char origin,
	int model_flag)
{
    int model2view = 0;
    if (model_flag)
	model2view = 1;
    vrot_xyz(v, origin, model2view, rvec);
}

static void
check_nonzero_rates(struct bview *v)
{
    if (!ZERO(v->k.vs_rate_model_rotate[X])
	|| !ZERO(v->k.vs_rate_model_rotate[Y])
	|| !ZERO(v->k.vs_rate_model_rotate[Z]))
    {
	v->k.vs_rateflag_model_rotate = 1;
    } else {
	v->k.vs_rateflag_model_rotate = 0;
    }

    if (!ZERO(v->k.vs_rate_model_tran[X])
	|| !ZERO(v->k.vs_rate_model_tran[Y])
	|| !ZERO(v->k.vs_rate_model_tran[Z]))
    {
	v->k.vs_rateflag_model_tran = 1;
    } else {
	v->k.vs_rateflag_model_tran = 0;
    }

    if (!ZERO(v->k.vs_rate_rotate[X])
	|| !ZERO(v->k.vs_rate_rotate[Y])
	|| !ZERO(v->k.vs_rate_rotate[Z]))
    {
	v->k.vs_rateflag_rotate = 1;
    } else {
	v->k.vs_rateflag_rotate = 0;
    }

    if (!ZERO(v->k.vs_rate_tran[X])
	|| !ZERO(v->k.vs_rate_tran[Y])
	|| !ZERO(v->k.vs_rate_tran[Z]))
    {
	v->k.vs_rateflag_tran = 1;
    } else {
	v->k.vs_rateflag_tran = 0;
    }

    if (!ZERO(v->k.vs_rate_scale)) {
	v->k.vs_rateflag_scale = 1;
    } else {
	v->k.vs_rateflag_scale = 0;
    }
}

static void
print_knob_vals(struct bu_vls *o, struct bview *v)
{
    if (!o || !v)
	return;

    bu_vls_printf(o, "rate - rotation:\n");
    bu_vls_printf(o, " x = %f\n", v->k.vs_rate_rotate[X]);
    bu_vls_printf(o, " y = %f\n", v->k.vs_rate_rotate[Y]);
    bu_vls_printf(o, " z = %f\n", v->k.vs_rate_rotate[Z]);
    bu_vls_printf(o, "rate - translation (skew):\n");
    bu_vls_printf(o, " X = %f\n", v->k.vs_rate_tran[X]);
    bu_vls_printf(o, " Y = %f\n", v->k.vs_rate_tran[Y]);
    bu_vls_printf(o, " Z = %f\n", v->k.vs_rate_tran[Z]);
    bu_vls_printf(o, "rate - scale:\n");
    bu_vls_printf(o, " S = %f\n", v->k.vs_rate_scale);

    bu_vls_printf(o, "absolute - rotation:\n");
    bu_vls_printf(o, " ax = %f\n", v->k.vs_absolute_rotate[X]);
    bu_vls_printf(o, " ay = %f\n", v->k.vs_absolute_rotate[Y]);
    bu_vls_printf(o, " az = %f\n", v->k.vs_absolute_rotate[Z]);
    bu_vls_printf(o, "absolute - translation (skew):\n");
    bu_vls_printf(o, " aX = %f\n", v->k.vs_absolute_tran[X]);
    bu_vls_printf(o, " aY = %f\n", v->k.vs_absolute_tran[Y]);
    bu_vls_printf(o, " aZ = %f\n", v->k.vs_absolute_tran[Z]);
    bu_vls_printf(o, "absolute - scale:\n");
    bu_vls_printf(o, " aS = %f\n", v->gv_a_scale);
}

int
ged_knob_core(struct ged *gedp, int argc, const char *argv[])
{
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);
    GED_CHECK_VIEW(gedp, BRLCAD_ERROR);
    struct bview *v = gedp->ged_gvp;

    static const char *usage =
	"knob [options] x # y # z # for rotation in degrees (may specify one or more of x, y, z)\n"
	"knob [options] S #for scale\n"
       	"knob [options] X # Y # Z # for translation (a.k.a slew) (rates, range -1..+1, may specify one or more of X, Y, Z))\n"
	"knob [options] ax # ay # az # for absolute rotation in degrees (may specify one or more of ax, ay, az)\n"
	"knob [options] aS # for absolute scale.\n"
	"knob [options] aX, aY, aZ for absolute translate (a.k.a slew) (may specify one or more of ax, ay, az).\n"
	"knob calibrate to set current absolute translate (a.k.a slew) to 0\n"
	"knob zero|clear|reset to cancel motion\n";

    int print_help = 0;
    int incr_flag = 0;  /* interpret values as increments */
    int view_flag = 0;  /* manipulate view using view coords */
    int model_flag = 0; /* manipulate view using model coords */
    char origin = 'v';

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    struct bu_opt_desc d[7];
    BU_OPT(d[0], "h", "help",         "",        NULL,          &print_help,   "Print help and exit");
    BU_OPT(d[1], "?", "",             "",        NULL,          &print_help,   "");
    BU_OPT(d[2], "i", "incr",         "",        NULL,          &incr_flag,    "Interpret values as increments");
    BU_OPT(d[3], "v", "view-coords",  "",        NULL,          &view_flag,    "Manipulate view using view coordinates.  Conflicts with 'm' If neither v or m options are specified, current gv_coord setting from parent view is used.");
    BU_OPT(d[4], "m", "model-coords", "",        NULL,          &model_flag,   "Manipulate view using model coordinates.  Conflicts with 'v'");
    BU_OPT(d[5], "o", "origin",       "<char>",  &bu_opt_char,  &origin,       "Specify origin mode - may be 'e' (eye_pt), 'm' (model origin) or 'v' (view origin - default).");
    BU_OPT_NULL(d[6]);

    /* parse standard options */
    int opt_ret = bu_opt_parse(NULL, argc, argv, d);

    if (print_help) {
        _ged_cmd_help(gedp, usage, d);
        return BRLCAD_OK;
    }

    /* adjust argc to match the leftovers of the options parsing */
    argc = opt_ret;
    if (argc == 1) {
	// print current values
	print_knob_vals(gedp->ged_result_str, gedp->ged_gvp);
	return BRLCAD_OK;
    }

    // Make sure model flag is set, if that's what either the user
    // options or view settings indicate we should use
    if (model_flag && view_flag) {
        _ged_cmd_help(gedp, usage, d);
	return BRLCAD_ERROR;
    }
    if (!model_flag && !view_flag)
	model_flag = (v->gv_coord == 'm') ? 1 : 0;

    int do_tran = 0;
    int do_rot = 0;
    vect_t tvec;
    vect_t rvec;
    VSETALL(tvec, 0.0);
    VSETALL(rvec, 0.0);

    for (--argc, ++argv; argc; --argc, ++argv) {

	const char *cmd = *argv;

	if (BU_STR_EQUAL(cmd, "zap") || BU_STR_EQUAL(cmd, "zero") ||
		BU_STR_EQUAL(cmd, "clear") || BU_STR_EQUAL(cmd, "stop")) {
	    // Per MGED, this command seems to reset the rate entries
	    // but not the absolute entries.
	    bv_knobs_reset(v, 1);
	    continue;
	}
	if (BU_STR_EQUAL(cmd, "calibrate")) {
	    VSETALL(v->k.vs_absolute_tran, 0.0);
	    continue;
	}

	if (argc < 2) {
	    _ged_cmd_help(gedp, usage, d);
	    return BRLCAD_ERROR;
	}

	// Get our number
	struct bu_vls fmsg = BU_VLS_INIT_ZERO;
	fastf_t f;
	if (bu_opt_fastf_t(&fmsg, 1, (const char **)&argv[1], (void *)&f) != 1) {
	    bu_vls_printf(gedp->ged_result_str, "Unable to parse numerical argument: '%s':%s\n", argv[1], bu_vls_cstr(&fmsg));
	    bu_vls_free(&fmsg);
	    return BRLCAD_ERROR;
	}
	bu_vls_free(&fmsg);

	// Read in the numerical argument, advance the array
	--argc;	++argv;

	// Process the actual command
	int kp_ret = bv_knob_cmd_process(
		&rvec, &do_rot, &tvec, &do_tran,
		v, cmd, f,
		origin, model_flag, incr_flag);

	if (kp_ret != BRLCAD_OK) {
	    _ged_cmd_help(gedp, usage, d);
	    return BRLCAD_ERROR;
	}
    }

    if (do_tran) {
	knob_tran(v, tvec, model_flag);
    }

    if (do_rot) {
	knob_rot(v, rvec, origin, model_flag);
    }

    check_nonzero_rates(v);

    return BRLCAD_OK;
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
