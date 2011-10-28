/*                         V U T I L . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2011 United States Government as represented by
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
/** @file libged/vutil.c
 *
 * This file contains view related utility functions.
 *
 */

#include "common.h"

#include "bio.h"

#include "./ged_private.h"


void
ged_view_update(struct ged_view *gvp)
{
    vect_t work, work1;
    vect_t temp, temp1;

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

    if (gvp->gv_callback)
	(*gvp->gv_callback)(gvp, gvp->gv_clientData);
}


/**
 * FIXME: this routine is suspect and needs investigating.  if run
 * during view initialization, the shaders regression test fails.
 */
void
_ged_mat_aet(struct ged_view *gvp)
{
    mat_t tmat;
    fastf_t twist;
    fastf_t c_twist;
    fastf_t s_twist;

    bn_mat_angles(gvp->gv_rotation,
		  270.0 + gvp->gv_aet[1],
		  0.0,
		  270.0 - gvp->gv_aet[0]);

    twist = -gvp->gv_aet[2] * bn_degtorad;
    c_twist = cos(twist);
    s_twist = sin(twist);
    bn_mat_zrot(tmat, s_twist, c_twist);
    bn_mat_mul2(tmat, gvp->gv_rotation);
}


int
_ged_do_rot(struct ged *gedp,
	    char coord,
	    mat_t rmat,
	    int (*func)())
{
    mat_t temp1, temp2;

    if (func != (int (*)())0)
	return (*func)(gedp, coord, gedp->ged_gvp->gv_rotate_about, rmat);

    switch (coord) {
	case 'm':
	    /* transform model rotations into view rotations */
	    bn_mat_inv(temp1, gedp->ged_gvp->gv_rotation);
	    bn_mat_mul(temp2, gedp->ged_gvp->gv_rotation, rmat);
	    bn_mat_mul(rmat, temp2, temp1);
	    break;
	case 'v':
	default:
	    break;
    }

    /* Calculate new view center */
    if (gedp->ged_gvp->gv_rotate_about != 'v') {
	point_t rot_pt;
	point_t new_origin;
	mat_t viewchg, viewchginv;
	point_t new_cent_view;
	point_t new_cent_model;

	switch (gedp->ged_gvp->gv_rotate_about) {
	    case 'e':
		VSET(rot_pt, 0.0, 0.0, 1.0);
		break;
	    case 'k':
		MAT4X3PNT(rot_pt, gedp->ged_gvp->gv_model2view, gedp->ged_gvp->gv_keypoint);
		break;
	    case 'm':
		/* rotate around model center (0, 0, 0) */
		VSET(new_origin, 0.0, 0.0, 0.0);
		MAT4X3PNT(rot_pt, gedp->ged_gvp->gv_model2view, new_origin);
		break;
	    default:
		return GED_ERROR;
	}

	bn_mat_xform_about_pt(viewchg, rmat, rot_pt);
	bn_mat_inv(viewchginv, viewchg);

	/* Convert origin in new (viewchg) coords back to old view coords */
	VSET(new_origin, 0.0, 0.0, 0.0);
	MAT4X3PNT(new_cent_view, viewchginv, new_origin);
	MAT4X3PNT(new_cent_model, gedp->ged_gvp->gv_view2model, new_cent_view);
	MAT_DELTAS_VEC_NEG(gedp->ged_gvp->gv_center, new_cent_model);
    }

    /* pure rotation */
    bn_mat_mul2(rmat, gedp->ged_gvp->gv_rotation);
    ged_view_update(gedp->ged_gvp);

    return GED_OK;
}


int
_ged_do_slew(struct ged *gedp, vect_t svec)
{
    point_t model_center;

    MAT4X3PNT(model_center, gedp->ged_gvp->gv_view2model, svec);
    MAT_DELTAS_VEC_NEG(gedp->ged_gvp->gv_center, model_center);
    ged_view_update(gedp->ged_gvp);

    return GED_OK;
}


int
_ged_do_tra(struct ged *gedp,
	    char coord,
	    vect_t tvec,
	    int (*func)())
{
    point_t delta;
    point_t work;
    point_t vc, nvc;

    if (func != (int (*)())0)
	return (*func)(gedp, coord, tvec);

    switch (coord) {
	case 'm':
	    VSCALE(delta, tvec, -gedp->ged_wdbp->dbip->dbi_base2local);
	    MAT_DELTAS_GET_NEG(vc, gedp->ged_gvp->gv_center);
	    break;
	case 'v':
	default:
	    VSCALE(tvec, tvec, -2.0*gedp->ged_wdbp->dbip->dbi_base2local*gedp->ged_gvp->gv_isize);
	    MAT4X3PNT(work, gedp->ged_gvp->gv_view2model, tvec);
	    MAT_DELTAS_GET_NEG(vc, gedp->ged_gvp->gv_center);
	    VSUB2(delta, work, vc);
	    break;
    }

    VSUB2(nvc, vc, delta);
    MAT_DELTAS_VEC_NEG(gedp->ged_gvp->gv_center, nvc);
    ged_view_update(gedp->ged_gvp);

    return GED_OK;
}


/**
 * P E R S P _ M A T
 *
 * Compute a perspective matrix for a right-handed coordinate system.
 * Reference: SGI Graphics Reference Appendix C
 *
 * (Note: SGI is left-handed, but the fix is done in the Display
 * Manager).
 */
void
ged_persp_mat(mat_t m,
	      fastf_t fovy,
	      fastf_t aspect,
	      fastf_t near1,
	      fastf_t far1,
	      fastf_t backoff)
{
    mat_t m2, tran;

    fovy *= 3.1415926535/180.0;

    MAT_IDN(m2);
    m2[5] = cos(fovy/2.0) / sin(fovy/2.0);
    m2[0] = m2[5]/aspect;
    m2[10] = (far1+near1) / (far1-near1);
    m2[11] = 2*far1*near1 / (far1-near1);	/* This should be negative */

    m2[14] = -1;		/* XXX This should be positive */
    m2[15] = 0;

    /* Move eye to origin, then apply perspective */
    MAT_IDN(tran);
    tran[11] = -backoff;
    bn_mat_mul(m, m2, tran);
}


/**
 * Create a perspective matrix that transforms the +/1 viewing cube,
 * with the acutal eye position (not at Z=+1) specified in viewing
 * coords, into a related space where the eye has been sheared onto
 * the Z axis and repositioned at Z=(0, 0, 1), with the same
 * perspective field of view as before.
 *
 * The Zbuffer clips off stuff with negative Z values.
 *
 * pmat = persp * xlate * shear
 */
void
ged_mike_persp_mat(mat_t pmat,
		   const point_t eye)
{
    mat_t shear;
    mat_t persp;
    mat_t xlate;
    mat_t t1, t2;
    point_t sheared_eye;

    if (eye[Z] <= SMALL) {
	VPRINT("mike_persp_mat(): ERROR, z<0, eye", eye);
	return;
    }

    /* Shear "eye" to +Z axis */
    MAT_IDN(shear);
    shear[2] = -eye[X]/eye[Z];
    shear[6] = -eye[Y]/eye[Z];

    MAT4X3VEC(sheared_eye, shear, eye);
    if (!NEAR_ZERO(sheared_eye[X], .01) || !NEAR_ZERO(sheared_eye[Y], .01)) {
	VPRINT("ERROR sheared_eye", sheared_eye);
	return;
    }

    /* Translate along +Z axis to put sheared_eye at (0, 0, 1). */
    MAT_IDN(xlate);
    /* XXX should I use MAT_DELTAS_VEC_NEG()?  X and Y should be 0 now */
    MAT_DELTAS(xlate, 0, 0, 1-sheared_eye[Z]);

    /* Build perspective matrix inline, substituting fov=2*atan(1, Z) */
    MAT_IDN(persp);
    /* From page 492 of Graphics Gems */
    persp[0] = sheared_eye[Z];	/* scaling: fov aspect term */
    persp[5] = sheared_eye[Z];	/* scaling: determines fov */

    /* From page 158 of Rogers Mathematical Elements */
    /* Z center of projection at Z=+1, r=-1/1 */
    persp[14] = -1;

    bn_mat_mul(t1, xlate, shear);
    bn_mat_mul(t2, persp, t1);

    /* Now, move eye from Z=1 to Z=0, for clipping purposes */
    MAT_DELTAS(xlate, 0, 0, -1);

    bn_mat_mul(pmat, xlate, t2);
}


/*
 * Map "display plate coordinates" (which can just be the screen viewing cube),
 * into [-1, +1] coordinates, with perspective.
 * Per "High Resolution Virtual Reality" by Michael Deering,
 * Computer Graphics 26, 2, July 1992, pp 195-201.
 *
 * L is lower left corner of screen, H is upper right corner.
 * L[Z] is the front (near) clipping plane location.
 * H[Z] is the back (far) clipping plane location.
 *
 * This corresponds to the SGI "window()" routine, but taking into account
 * skew due to the eyepoint being offset parallel to the image plane.
 *
 * The gist of the algorithm is to translate the display plate to the
 * view center, shear the eye point to (0, 0, 1), translate back,
 * then apply an off-axis perspective projection.
 *
 * Another (partial) reference is "A comparison of stereoscopic cursors
 * for the interactive manipulation of B-splines" by Barham & McAllister,
 * SPIE Vol 1457 Stereoscopic Display & Applications, 1991, pg 19.
 */
void
ged_deering_persp_mat(fastf_t *m, const fastf_t *l, const fastf_t *h, const fastf_t *eye)
/* lower left corner of screen */
/* upper right (high) corner of screen */
/* eye location.  Traditionally at (0, 0, 1) */
{
    vect_t diff;	/* H - L */
    vect_t sum;	/* H + L */

    VSUB2(diff, h, l);
    VADD2(sum, h, l);

    m[0] = 2 * eye[Z] / diff[X];
    m[1] = 0;
    m[2] = (sum[X] - 2 * eye[X]) / diff[X];
    m[3] = -eye[Z] * sum[X] / diff[X];

    m[4] = 0;
    m[5] = 2 * eye[Z] / diff[Y];
    m[6] = (sum[Y] - 2 * eye[Y]) / diff[Y];
    m[7] = -eye[Z] * sum[Y] / diff[Y];

    /* Multiplied by -1, to do right-handed Z coords */
    m[8] = 0;
    m[9] = 0;
    m[10] = -(sum[Z] - 2 * eye[Z]) / diff[Z];
    m[11] = -(-eye[Z] + 2 * h[Z] * eye[Z]) / diff[Z];

    m[12] = 0;
    m[13] = 0;
    m[14] = -1;
    m[15] = eye[Z];

/* XXX May need to flip Z ? (lefthand to righthand?) */
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
