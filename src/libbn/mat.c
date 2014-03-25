/*                           M A T . C
 * BRL-CAD
 *
 * Copyright (c) 1996-2014 United States Government as represented by
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

/** @addtogroup mat */
/** @{ */
/** @file libbn/mat.c
 *
 * TODO: need a better way to control tolerancing, either via
 * additional tolerance parameters or perhaps providing a global
 * tolerance state interface.
 */


#include "common.h"

#include <math.h>
#include <string.h>
#include "bio.h"

#include "bu/debug.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "bu/str.h"
#include "vmath.h"
#include "bn.h"


const mat_t bn_mat_identity = MAT_INIT_IDN;


void
bn_mat_print_guts(const char *title,
		  const mat_t m,
		  char *obuf,
		  int len)
{
    register int i;
    register char *cp;

    snprintf(obuf, len, "MATRIX %s:\n  ", title);
    cp = obuf+strlen(obuf);
    if (!m) {
	bu_strlcat(obuf, "(Identity)", len);
    } else {
	for (i=0; i<16; i++) {
	    snprintf(cp, len-(cp-obuf), " %8.3f", m[i]);
	    cp += strlen(cp);
	    if (i == 15) {
		break;
	    } else if ((i&3) == 3) {
		*cp++ = '\n';
		*cp++ = ' ';
		*cp++ = ' ';
	    }
	}
	*cp++ = '\0';
    }
}


void
bn_mat_print(const char *title,
	     const mat_t m)
{
    char obuf[1024];	/* snprintf may be non-PARALLEL */

    bn_mat_print_guts(title, m, obuf, 1024);
    bu_log("%s\n", obuf);
}


void
bn_mat_print_vls(const char *title,
		 const mat_t m,
		 struct bu_vls *vls)
{
    char obuf[1024];

    bn_mat_print_guts(title, m, obuf, 1024);
    bu_vls_printf(vls, "%s\n", obuf);
}


double
bn_atan2(double y, double x)
{
    if (x > -1.0e-20 && x < 1.0e-20) {
	/* X is equal to zero, check Y */
	if (y < -1.0e-20)
	    return -M_PI_2;
	if (y > 1.0e-20)
	    return M_PI_2;
	return 0.0;
    }
    return atan2(y, x);
}


void
bn_mat_mul(register mat_t o, register const mat_t a, register const mat_t b)
{
    o[ 0] = a[ 0]*b[ 0] + a[ 1]*b[ 4] + a[ 2]*b[ 8] + a[ 3]*b[12];
    o[ 1] = a[ 0]*b[ 1] + a[ 1]*b[ 5] + a[ 2]*b[ 9] + a[ 3]*b[13];
    o[ 2] = a[ 0]*b[ 2] + a[ 1]*b[ 6] + a[ 2]*b[10] + a[ 3]*b[14];
    o[ 3] = a[ 0]*b[ 3] + a[ 1]*b[ 7] + a[ 2]*b[11] + a[ 3]*b[15];

    o[ 4] = a[ 4]*b[ 0] + a[ 5]*b[ 4] + a[ 6]*b[ 8] + a[ 7]*b[12];
    o[ 5] = a[ 4]*b[ 1] + a[ 5]*b[ 5] + a[ 6]*b[ 9] + a[ 7]*b[13];
    o[ 6] = a[ 4]*b[ 2] + a[ 5]*b[ 6] + a[ 6]*b[10] + a[ 7]*b[14];
    o[ 7] = a[ 4]*b[ 3] + a[ 5]*b[ 7] + a[ 6]*b[11] + a[ 7]*b[15];

    o[ 8] = a[ 8]*b[ 0] + a[ 9]*b[ 4] + a[10]*b[ 8] + a[11]*b[12];
    o[ 9] = a[ 8]*b[ 1] + a[ 9]*b[ 5] + a[10]*b[ 9] + a[11]*b[13];
    o[10] = a[ 8]*b[ 2] + a[ 9]*b[ 6] + a[10]*b[10] + a[11]*b[14];
    o[11] = a[ 8]*b[ 3] + a[ 9]*b[ 7] + a[10]*b[11] + a[11]*b[15];

    o[12] = a[12]*b[ 0] + a[13]*b[ 4] + a[14]*b[ 8] + a[15]*b[12];
    o[13] = a[12]*b[ 1] + a[13]*b[ 5] + a[14]*b[ 9] + a[15]*b[13];
    o[14] = a[12]*b[ 2] + a[13]*b[ 6] + a[14]*b[10] + a[15]*b[14];
    o[15] = a[12]*b[ 3] + a[13]*b[ 7] + a[14]*b[11] + a[15]*b[15];
}


void
bn_mat_mul2(register const mat_t i, register mat_t o)
{
    mat_t temp;

    bn_mat_mul(temp, i, o);
    MAT_COPY(o, temp);
}


void
bn_mat_mul3(mat_t o, const mat_t a, const mat_t b, const mat_t c)
{
    mat_t t;

    bn_mat_mul(t, b, c);
    bn_mat_mul(o, a, t);
}


void
bn_mat_mul4(mat_t ao, const mat_t a, const mat_t b, const mat_t c, const mat_t d)
{
    mat_t t, u;

    bn_mat_mul(u, c, d);
    bn_mat_mul(t, a, b);
    bn_mat_mul(ao, t, u);
}


void
bn_matXvec(register vect_t ov, register const mat_t im, register const vect_t iv)
{
    register int eo = 0;	/* Position in output vector */
    register int em = 0;	/* Position in input matrix */
    register int ei;		/* Position in input vector */

    /* For each element in the output array... */
    for (; eo<4; eo++) {

	ov[eo] = 0;		/* Start with zero in output */

	for (ei=0; ei<4; ei++)
	    ov[eo] += im[em++] * iv[ei];
    }
}


void
bn_mat_inv(register mat_t output, const mat_t input)
{
    if (bn_mat_inverse(output, input) == 0) {

	if (bu_debug & BU_DEBUG_MATH) {
	    bu_log("bn_mat_inv:  error!");
	    bn_mat_print("singular matrix", input);
	}
	bu_bomb("bn_mat_inv: singular matrix\n");
	/* NOTREACHED */
    }
}


int
bn_mat_inverse(register mat_t output, const mat_t input)
{
    register int i, j;	/* Indices */
    int k;		/* Indices */
    int z[4];		/* Temporary */
    fastf_t b[4];	/* Temporary */
    fastf_t c[4];	/* Temporary */

    MAT_COPY(output, input);	/* Duplicate */

    /* Initialization */
    for (j = 0; j < 4; j++)
	z[j] = j;

    /* Main Loop */
    for (i = 0; i < 4; i++) {
	register fastf_t y;		/* local temporary */

	k = i;
	y = output[i*4+i];
	for (j = i+1; j < 4; j++) {
	    register fastf_t w;		/* local temporary */

	    w = output[i*4+j];
	    if (fabs(w) > fabs(y)) {
		k = j;
		y = w;
	    }
	}

	if (ZERO(y)) {
	    /* SINGULAR */
	    return 0;
	}
	y = 1.0 / y;

	for (j = 0; j < 4; j++) {
	    register fastf_t temp;	/* Local */

	    c[j] = output[j*4+k];
	    output[j*4+k] = output[j*4+i];
	    output[j*4+i] = - c[j] * y;
	    temp = output[i*4+j] * y;
	    b[j] = temp;
	    output[i*4+j] = temp;
	}

	output[i*4+i] = y;
	j = z[i];
	z[i] = z[k];
	z[k] = j;
	for (k = 0; k < 4; k++) {
	    if (k == i)
		continue;
	    for (j = 0; j < 4; j++) {
		if (j == i)
		    continue;
		output[k*4+j] = output[k*4+j] - b[j] * c[k];
	    }
	}
    }

    /* Second Loop */
    for (i = 0; i < 4; i++) {
	while ((k = z[i]) != i) {
	    int p;			/* Local temp */

	    for (j = 0; j < 4; j++) {
		register fastf_t w;	/* Local temp */

		w = output[i*4+j];
		output[i*4+j] = output[k*4+j];
		output[k*4+j] = w;
	    }
	    p = z[i];
	    z[i] = z[k];
	    z[k] = p;
	}
    }

    return 1;
}


void
bn_vtoh_move(register vect_t h, register const vect_t v)
{
    VMOVE(h, v);
    h[W] = 1.0;
}


void
bn_htov_move(register vect_t v, register const vect_t h)
{
    register fastf_t inv;

    if (ZERO(h[3] - 1.0)) {
	VMOVE(v, h);
    } else {
	if (ZERO(h[W])) {
	    bu_log("bn_htov_move: divide by %f!\n", h[W]);
	    return;
	}
	inv = 1.0 / h[W];
	VSCALE(v, h, inv);
    }
}


void
bn_mat_trn(mat_t om, register const mat_t im)
{
    MAT_TRANSPOSE(om, im);
}


void
bn_mat_ae(register fastf_t *m, double azimuth, double elev)
{
    double sin_az, sin_el;
    double cos_az, cos_el;

    azimuth *= DEG2RAD;
    elev *= DEG2RAD;

    sin_az = sin(azimuth);
    cos_az = cos(azimuth);
    sin_el = sin(elev);
    cos_el = cos(elev);

    m[0] = cos_el * cos_az;
    m[1] = -sin_az;
    m[2] = -sin_el * cos_az;
    m[3] = 0;

    m[4] = cos_el * sin_az;
    m[5] = cos_az;
    m[6] = -sin_el * sin_az;
    m[7] = 0;

    m[8] = sin_el;
    m[9] = 0;
    m[10] = cos_el;
    m[11] = 0;

    m[12] = m[13] = m[14] = 0;
    m[15] = 1.0;
}


void
bn_ae_vec(fastf_t *azp, fastf_t *elp, const vect_t v)
{
    register fastf_t az;

    if ((az = bn_atan2(v[Y], v[X]) * RAD2DEG) < 0) {
	*azp = 360 + az;
    } else if (az >= 360) {
	*azp = az - 360;
    } else {
	*azp = az;
    }
    *elp = bn_atan2(v[Z], hypot(v[X], v[Y])) * RAD2DEG;
}


void
bn_aet_vec(fastf_t *az, fastf_t *el, fastf_t *twist, fastf_t *vec_ae, fastf_t *vec_twist, fastf_t accuracy)
{
    vect_t zero_twist, ninety_twist;
    vect_t z_dir;

    /* Get az and el as usual */
    bn_ae_vec(az, el, vec_ae);

    /* stabilize fluctuation between 0 and 360
     * change azimuth near 360 to 0 */
    if (NEAR_EQUAL(*az, 360.0, accuracy))
	*az = 0.0;

    /* if elevation is +/-90 set twist to zero and calculate azimuth */
    if (NEAR_EQUAL(*el, 90.0, accuracy) || NEAR_ZERO(*el + 90.0, accuracy)) {
	*twist = 0.0;
	*az = bn_atan2(-vec_twist[X], vec_twist[Y]) * RAD2DEG;
    } else {
	/* Calculate twist from vec_twist */
	VSET(z_dir, 0, 0, 1);
	VCROSS(zero_twist, z_dir, vec_ae);
	VUNITIZE(zero_twist);
	VCROSS(ninety_twist, vec_ae, zero_twist);
	VUNITIZE(ninety_twist);

	*twist = bn_atan2(VDOT(vec_twist, ninety_twist), VDOT(vec_twist, zero_twist)) * RAD2DEG;

	/* stabilize flutter between +/- 180 */
	if (NEAR_EQUAL(*twist, -180.0, accuracy))
	    *twist = 180.0;
    }
}


void
bn_vec_ae(vect_t vect, fastf_t az, fastf_t el)
{
    fastf_t vx, vy, vz;
    vz = sin(el);
    vy = fabs(vz) * sin(az);
    vx = fabs(vz) * cos(az);
    VSET(vect, vx, vy , vz);
    VUNITIZE(vect);
}


void
bn_vec_aed(vect_t vect, fastf_t az, fastf_t el, fastf_t distance)
{
    fastf_t vx, vy, vz, rtemp;
    vz = distance * sin(el);
    rtemp = sqrt((distance * distance) - (vz * vz));
    vy = rtemp * sin(az);
    vx = rtemp * cos(az);
    VSET(vect, vx, vy , vz);
}


void
bn_mat_angles(register fastf_t *mat, double alpha_in, double beta_in, double ggamma_in)
{
    double alpha, beta, ggamma;
    double calpha, cbeta, cgamma;
    double salpha, sbeta, sgamma;

    if (NEAR_ZERO(alpha_in, 0.0) && NEAR_ZERO(beta_in, 0.0) && NEAR_ZERO(ggamma_in, 0.0)) {
	MAT_IDN(mat);
	return;
    }

    alpha = alpha_in * DEG2RAD;
    beta = beta_in * DEG2RAD;
    ggamma = ggamma_in * DEG2RAD;

    calpha = cos(alpha);
    cbeta = cos(beta);
    cgamma = cos(ggamma);

    /* sine of "180*DEG2RAD" will not be exactly zero and will
     * result in errors when some codes try to convert this back to
     * azimuth and elevation.  do_frame() uses this technique!!!
     */
    if (ZERO(alpha_in - 180.0))
	salpha = 0.0;
    else
	salpha = sin(alpha);

    if (ZERO(beta_in - 180.0))
	sbeta = 0.0;
    else
	sbeta = sin(beta);

    if (ZERO(ggamma_in - 180.0))
	sgamma = 0.0;
    else
	sgamma = sin(ggamma);

    mat[0] = cbeta * cgamma;
    mat[1] = -cbeta * sgamma;
    mat[2] = sbeta;
    mat[3] = 0.0;

    mat[4] = salpha * sbeta * cgamma + calpha * sgamma;
    mat[5] = -salpha * sbeta * sgamma + calpha * cgamma;
    mat[6] = -salpha * cbeta;
    mat[7] = 0.0;

    mat[8] = salpha * sgamma - calpha * sbeta * cgamma;
    mat[9] = salpha * cgamma + calpha * sbeta * sgamma;
    mat[10] = calpha * cbeta;
    mat[11] = 0.0;
    mat[12] = mat[13] = mat[14] = 0.0;
    mat[15] = 1.0;
}


void
bn_mat_angles_rad(register mat_t mat,
		  double alpha,
		  double beta,
		  double ggamma)
{
    double calpha, cbeta, cgamma;
    double salpha, sbeta, sgamma;

    if (ZERO(alpha) && ZERO(beta) && ZERO(ggamma)) {
	MAT_IDN(mat);
	return;
    }

    calpha = cos(alpha);
    cbeta = cos(beta);
    cgamma = cos(ggamma);

    salpha = sin(alpha);
    sbeta = sin(beta);
    sgamma = sin(ggamma);

    mat[0] = cbeta * cgamma;
    mat[1] = -cbeta * sgamma;
    mat[2] = sbeta;
    mat[3] = 0.0;

    mat[4] = salpha * sbeta * cgamma + calpha * sgamma;
    mat[5] = -salpha * sbeta * sgamma + calpha * cgamma;
    mat[6] = -salpha * cbeta;
    mat[7] = 0.0;

    mat[8] = salpha * sgamma - calpha * sbeta * cgamma;
    mat[9] = salpha * cgamma + calpha * sbeta * sgamma;
    mat[10] = calpha * cbeta;
    mat[11] = 0.0;
    mat[12] = mat[13] = mat[14] = 0.0;
    mat[15] = 1.0;
}


void
bn_eigen2x2(fastf_t *val1, fastf_t *val2, fastf_t *vec1, fastf_t *vec2, fastf_t a, fastf_t b, fastf_t c)
{
    fastf_t d, root;
    fastf_t v1, v2;

    d = 0.5 * (c - a);

    /* Check for diagonal matrix */
    if (NEAR_ZERO(b, 1.0e-10)) {
	/* smaller mag first */
	if (fabs(c) < fabs(a)) {
	    *val1 = c;
	    VSET(vec1, 0.0, 1.0, 0.0);
	    *val2 = a;
	    VSET(vec2, -1.0, 0.0, 0.0);
	} else {
	    *val1 = a;
	    VSET(vec1, 1.0, 0.0, 0.0);
	    *val2 = c;
	    VSET(vec2, 0.0, 1.0, 0.0);
	}
	return;
    }

    root = sqrt(d*d + b*b);
    v1 = 0.5 * (c + a) - root;
    v2 = 0.5 * (c + a) + root;

    /* smaller mag first */
    if (fabs(v1) < fabs(v2)) {
	*val1 = v1;
	*val2 = v2;
	VSET(vec1, b, d - root, 0.0);
    } else {
	*val1 = v2;
	*val2 = v1;
	VSET(vec1, root - d, b, 0.0);
    }
    VUNITIZE(vec1);
    VSET(vec2, -vec1[Y], vec1[X], 0.0);	/* vec1 X vec2 = +Z */
}


void
bn_vec_perp(vect_t new_vec, const vect_t old_vec)
{
    register int i;
    vect_t another_vec;	/* Another vector, different */

    i = X;
    if (fabs(old_vec[Y])<fabs(old_vec[i])) i=Y;
    if (fabs(old_vec[Z])<fabs(old_vec[i])) i=Z;
    VSETALL(another_vec, 0);
    another_vec[i] = 1.0;
    if (ZERO(old_vec[X]) && ZERO(old_vec[Y]) && ZERO(old_vec[Z])) {
	VMOVE(new_vec, another_vec);
    } else {
	VCROSS(new_vec, another_vec, old_vec);
    }
}


void
bn_mat_fromto(mat_t m, const fastf_t *from, const fastf_t *to, const struct bn_tol *tol)
{
    vect_t test_to;
    vect_t unit_from, unit_to;
    fastf_t dot;
    point_t origin = VINIT_ZERO;

    /**
     * The method used here is from Graphics Gems, A. Glassner, ed.
     * page 531, "The Use of Coordinate Frames in Computer Graphics",
     * by Ken Turkowski, Example 6.
     */
    mat_t Q, Qt;
    mat_t R;
    mat_t A;
    mat_t temp;
    vect_t N, M;
    vect_t w_prime;		/* image of "to" ("w") in Qt */

    VMOVE(unit_from, from);
    VUNITIZE(unit_from);		/* a/k/a "v" */
    VMOVE(unit_to, to);
    VUNITIZE(unit_to);		/* a/k/a "w" */

    /* If from and to are the same or opposite, special handling is
     * needed, because the cross product isn't defined.  asin(0.00001)
     * = 0.0005729 degrees (1/2000 degree)
     */
    if (bn_lseg3_lseg3_parallel(origin, from, origin, to, tol)) {
	dot = VDOT(from, to);
	if (dot > SMALL_FASTF) {
	    MAT_IDN(m);
	    return;
	} else {
	    bn_vec_perp(N, unit_from);
	}
    } else {
	VCROSS(N, unit_from, unit_to);
	VUNITIZE(N);			/* should be unnecessary */
    }
    VCROSS(M, N, unit_from);
    VUNITIZE(M);			/* should be unnecessary */

    /* Almost everything here is done with pre-multiplys:  vector * mat */
    MAT_IDN(Q);
    VMOVE(&Q[0], unit_from);
    VMOVE(&Q[4], M);
    VMOVE(&Q[8], N);
    bn_mat_trn(Qt, Q);

    /* w_prime = w * Qt */
    MAT4X3VEC(w_prime, Q, unit_to);	/* post-multiply by transpose */

    MAT_IDN(R);
    VMOVE(&R[0], w_prime);
    VSET(&R[4], -w_prime[Y], w_prime[X], w_prime[Z]);
    VSET(&R[8], 0, 0, 1);		/* is unnecessary */

    bn_mat_mul(temp, R, Q);
    bn_mat_mul(A, Qt, temp);
    bn_mat_trn(m, A);		/* back to post-multiply style */

    /* Verify that it worked */
    MAT4X3VEC(test_to, m, unit_from);
    if (UNLIKELY(!bn_lseg3_lseg3_parallel(origin, unit_to, origin, test_to, tol))) {
	dot = VDOT(unit_to, test_to);
	bu_log("bn_mat_fromto() ERROR!  from (%g, %g, %g) to (%g, %g, %g) went to (%g, %g, %g), dot=%g?\n",
	       V3ARGS(from), V3ARGS(to), V3ARGS(test_to), dot);
    }
}


void
bn_mat_xrot(fastf_t *m, double sinx, double cosx)
{
    m[0] = 1.0;
    m[1] = 0.0;
    m[2] = 0.0;
    m[3] = 0.0;

    m[4] = 0.0;
    m[5] = cosx;
    m[6] = -sinx;
    m[7] = 0.0;

    m[8] = 0.0;
    m[9] = sinx;
    m[10] = cosx;
    m[11] = 0.0;

    m[12] = m[13] = m[14] = 0.0;
    m[15] = 1.0;
}


void
bn_mat_yrot(fastf_t *m, double siny, double cosy)
{
    m[0] = cosy;
    m[1] = 0.0;
    m[2] = -siny;
    m[3] = 0.0;

    m[4] = 0.0;
    m[5] = 1.0;
    m[6] = 0.0;
    m[7] = 0.0;

    m[8] = siny;
    m[9] = 0.0;
    m[10] = cosy;

    m[11] = 0.0;
    m[12] = m[13] = m[14] = 0.0;
    m[15] = 1.0;
}


void
bn_mat_zrot(fastf_t *m, double sinz, double cosz)
{
    m[0] = cosz;
    m[1] = -sinz;
    m[2] = 0.0;
    m[3] = 0.0;

    m[4] = sinz;
    m[5] = cosz;
    m[6] = 0.0;
    m[7] = 0.0;

    m[8] = 0.0;
    m[9] = 0.0;
    m[10] = 1.0;
    m[11] = 0.0;

    m[12] = m[13] = m[14] = 0.0;
    m[15] = 1.0;
}


void
bn_mat_lookat(mat_t rot, const vect_t dir, int yflip)
{
    mat_t first;
    mat_t second;
    mat_t prod12;
    mat_t third;
    vect_t x;
    vect_t z;
    vect_t t1;
    fastf_t hypot_xy;
    vect_t xproj;
    vect_t zproj;

    /* First, rotate D around Z axis to match +X axis (azimuth) */
    hypot_xy = hypot(dir[X], dir[Y]);
    bn_mat_zrot(first, -dir[Y] / hypot_xy, dir[X] / hypot_xy);

    /* Next, rotate D around Y axis to match -Z axis (elevation) */
    bn_mat_yrot(second, -hypot_xy, -dir[Z]);
    bn_mat_mul(prod12, second, first);

    /* Produce twist correction, by re-orienting projection of X axis */
    VSET(x, 1, 0, 0);
    MAT4X3VEC(xproj, prod12, x);
    hypot_xy = hypot(xproj[X], xproj[Y]);
    if (hypot_xy < 1.0e-10) {
	bu_log("Warning: bn_mat_lookat:  unable to twist correct, hypot=%g\n", hypot_xy);
	VPRINT("xproj", xproj);
	MAT_COPY(rot, prod12);
	return;
    }
    bn_mat_zrot(third, -xproj[Y] / hypot_xy, xproj[X] / hypot_xy);
    bn_mat_mul(rot, third, prod12);

    if (yflip) {
	VSET(z, 0, 0, 1);
	MAT4X3VEC(zproj, rot, z);
	/* If original Z inverts sign, flip sign on resulting Y */
	if (zproj[Y] < 0.0) {
	    MAT_COPY(prod12, rot);
	    MAT_IDN(third);
	    third[5] = -1;
	    bn_mat_mul(rot, third, prod12);
	}
    }

    /* Check the final results */
    MAT4X3VEC(t1, rot, dir);
    if (t1[Z] > -0.98) {
	bu_log("Error:  bn_mat_lookat final= (%g, %g, %g)\n", V3ARGS(t1));
    }
}


void
bn_vec_ortho(register vect_t out, register const vect_t in)
{
    register int j, k;
    register fastf_t f;
    register int i;

    if (UNLIKELY(NEAR_ZERO(MAGSQ(in), SQRT_SMALL_FASTF))) {
	bu_log("bn_vec_ortho(): zero magnitude input vector %g %g %g\n", V3ARGS(in));
	VSETALL(out, 0);
	return;
    }

    /* Find component closest to zero */
    f = fabs(in[X]);
    i = X;
    j = Y;
    k = Z;
    if (fabs(in[Y]) < f) {
	f = fabs(in[Y]);
	i = Y;
	j = Z;
	k = X;
    }
    if (fabs(in[Z]) < f) {
	i = Z;
	j = X;
	k = Y;
    }
    f = hypot(in[j], in[k]);
    if (UNLIKELY(ZERO(f))) {
	bu_log("bn_vec_ortho(): zero hypot on %g %g %g\n", V3ARGS(in));
	VSETALL(out, 0);
	return;
    }
    f = 1.0/f;
    out[i] = 0.0;
    out[j] = -in[k]*f;
    out[k] =  in[j]*f;

    return;
}


int
bn_mat_scale_about_pt(mat_t mat, const point_t pt, const double scale)
{
    mat_t xlate;
    mat_t s;
    mat_t tmp;

    MAT_IDN(xlate);
    MAT_DELTAS_VEC_NEG(xlate, pt);

    MAT_IDN(s);
    if (ZERO(scale)) {
	MAT_ZERO(mat);
	return -1;			/* ERROR */
    }
    s[15] = 1/scale;

    bn_mat_mul(tmp, s, xlate);

    MAT_DELTAS_VEC(xlate, pt);
    bn_mat_mul(mat, xlate, tmp);
    return 0;				/* OK */
}


void
bn_mat_xform_about_pt(mat_t mat, const mat_t xform, const point_t pt)
{
    mat_t xlate;
    mat_t tmp;

    MAT_IDN(xlate);
    MAT_DELTAS_VEC_NEG(xlate, pt);

    bn_mat_mul(tmp, xform, xlate);

    MAT_DELTAS_VEC(xlate, pt);
    bn_mat_mul(mat, xlate, tmp);
}


int
bn_mat_is_equal(const mat_t a, const mat_t b, const struct bn_tol *tol)
{
    register int i;
    register double f;
    register double tdist, tperp;

    BN_CK_TOL(tol);

    tdist = tol->dist;
    tperp = tol->perp;

    /* First, check that the translation part of the matrix (dist) are
     * within the distance tolerance.  Because most instancing
     * involves translation and no rotation, doing this first should
     * detect most non-equal cases rapidly.
     */
    for (i=3; i<12; i+=4) {
	f = a[i] - b[i];
	if (!NEAR_ZERO(f, tdist)) return 0;
    }

    /* Check that the rotation part of the matrix (cosines) are within
     * the perpendicular tolerance.
     */
    for (i = 0; i < 16; i+=4) {
	f = a[i] - b[i];
	if (!NEAR_ZERO(f, tperp)) return 0;
	f = a[i+1] - b[i+1];
	if (!NEAR_ZERO(f, tperp)) return 0;
	f = a[i+2] - b[i+2];
	if (!NEAR_ZERO(f, tperp)) return 0;
    }

    /* Check that the scale part of the matrix (ratio) is within the
     * perpendicular tolerance.  There is no ratio tolerance so we use
     * the tighter of dist or perp.
     */
    f = a[15] - b[15];
    if (!NEAR_ZERO(f, tperp)) return 0;

    return 1;
}


int
bn_mat_is_identity(const mat_t m)
{
    return ! memcmp(m, bn_mat_identity, sizeof(mat_t));
}


void
bn_mat_arb_rot(mat_t m, const point_t pt, const vect_t dir, const fastf_t ang)
{
    mat_t tran1, tran2, rot;
    double cos_ang, sin_ang, one_m_cosang;
    double n1_sq, n2_sq, n3_sq;
    double n1_n2, n1_n3, n2_n3;

    if (ZERO(ang)) {
	MAT_IDN(m);
	return;
    }

    MAT_IDN(tran1);
    MAT_IDN(tran2);

    /* construct translation matrix to pt */
    tran1[MDX] = (-pt[X]);
    tran1[MDY] = (-pt[Y]);
    tran1[MDZ] = (-pt[Z]);

    /* construct translation back from pt */
    tran2[MDX] = pt[X];
    tran2[MDY] = pt[Y];
    tran2[MDZ] = pt[Z];

    /* construct rotation matrix */
    cos_ang = cos(ang);
    sin_ang = sin(ang);
    one_m_cosang = 1.0 - cos_ang;
    n1_sq = dir[X]*dir[X];
    n2_sq = dir[Y]*dir[Y];
    n3_sq = dir[Z]*dir[Z];
    n1_n2 = dir[X]*dir[Y];
    n1_n3 = dir[X]*dir[Z];
    n2_n3 = dir[Y]*dir[Z];

    MAT_IDN(rot);
    rot[0] = n1_sq + (1.0 - n1_sq)*cos_ang;
    rot[1] = n1_n2 * one_m_cosang - dir[Z]*sin_ang;
    rot[2] = n1_n3 * one_m_cosang + dir[Y]*sin_ang;

    rot[4] = n1_n2 * one_m_cosang + dir[Z]*sin_ang;
    rot[5] = n2_sq + (1.0 - n2_sq)*cos_ang;
    rot[6] = n2_n3 * one_m_cosang - dir[X]*sin_ang;

    rot[8] = n1_n3 * one_m_cosang - dir[Y]*sin_ang;
    rot[9] = n2_n3 * one_m_cosang + dir[X]*sin_ang;
    rot[10] = n3_sq + (1.0 - n3_sq) * cos_ang;

    bn_mat_mul(m, rot, tran1);
    bn_mat_mul2(tran2, m);
}


matp_t
bn_mat_dup(const mat_t in)
{
    mat_t *out;

    BU_ALLOC(out, mat_t);
    MAT_COPY(*out, in);

    return (matp_t)out;
}


int
bn_mat_ck(const char *title, const mat_t m)
{
    vect_t A, B, C;
    fastf_t fx, fy, fz;

    if (!m) return 0;		/* implies identity matrix */

    /* Validate that matrix preserves perpendicularity of axis by
     * checking that A.B == 0, B.C == 0, A.C == 0 XXX these vectors
     * should just be grabbed out of the matrix
     */
    VMOVE(A, &m[0]);
    VMOVE(B, &m[4]);
    VMOVE(C, &m[8]);

    fx = VDOT(A, B);
    fy = VDOT(B, C);
    fz = VDOT(A, C);

    /* NOTE: this tolerance cannot be any more tight than 0.00001 due
     * to default calculation tolerancing used by models.  Matrices
     * exported to disk outside of tolerance will fail import if
     * set too restrictive.
     */
    if (!NEAR_ZERO(fx, 0.00001)
	|| !NEAR_ZERO(fy, 0.00001)
	|| !NEAR_ZERO(fz, 0.00001)
	|| NEAR_ZERO(m[15], VDIVIDE_TOL))
    {
	if (bu_debug & BU_DEBUG_MATH) {
	    bu_log("bn_mat_ck(%s):  bad matrix, does not preserve axis perpendicularity.\n  X.Y=%g, Y.Z=%g, X.Z=%g, s=%g\n", title, fx, fy, fz, m[15]);
	    bn_mat_print("bn_mat_ck() bad matrix", m);
	}

	if (bu_debug & (BU_DEBUG_MATH | BU_DEBUG_COREDUMP)) {
	    bu_debug |= BU_DEBUG_COREDUMP;
	    bu_bomb("bn_mat_ck() bad matrix\n");
	}
	return -1;	/* FAIL */
    }
    return 0;		/* OK */
}


fastf_t
bn_mat_det3(const mat_t m)
{
    register fastf_t sum;

    sum = m[0] * (m[5]*m[10] - m[6]*m[9])
	-m[1] * (m[4]*m[10] - m[6]*m[8])
	+m[2] * (m[4]*m[9] - m[5]*m[8]);

    return sum;
}


fastf_t
bn_mat_determinant(const mat_t m)
{
    fastf_t det[4];
    fastf_t sum;

    det[0] = m[5] * (m[10]*m[15] - m[11]*m[14])
	-m[6] * (m[ 9]*m[15] - m[11]*m[13])
	+m[7] * (m[ 9]*m[14] - m[10]*m[13]);

    det[1] = m[4] * (m[10]*m[15] - m[11]*m[14])
	-m[6] * (m[ 8]*m[15] - m[11]*m[12])
	+m[7] * (m[ 8]*m[14] - m[10]*m[12]);

    det[2] = m[4] * (m[ 9]*m[15] - m[11]*m[13])
	-m[5] * (m[ 8]*m[15] - m[11]*m[12])
	+m[7] * (m[ 8]*m[13] - m[ 9]*m[12]);

    det[3] = m[4] * (m[ 9]*m[14] - m[10]*m[13])
	-m[5] * (m[ 8]*m[14] - m[10]*m[12])
	+m[6] * (m[ 8]*m[13] - m[ 9]*m[12]);

    sum = m[0]*det[0] - m[1]*det[1] + m[2]*det[2] - m[3]*det[3];

    return sum;

}


int
bn_mat_is_non_unif (const mat_t m)
{
    double mag[3];

    mag[0] = MAGSQ(m);
    mag[1] = MAGSQ(&m[4]);
    mag[2] = MAGSQ(&m[8]);

    if (fabs(1.0 - (mag[1]/mag[0])) > .0005 ||
	fabs(1.0 - (mag[2]/mag[0])) > .0005) {

	return 1;
    }

    if (!ZERO(m[12]) || !ZERO(m[13]) || !ZERO(m[14]))
	return 2;

    return 0;
}


void
bn_wrt_point_direc(mat_t out, const mat_t change, const mat_t in, const point_t point, const vect_t direc, const struct bn_tol *tol)
{
    static mat_t t1;
    static mat_t pt_to_origin, origin_to_pt;
    static mat_t d_to_zaxis, zaxis_to_d;
    static vect_t zaxis;

    /* build "point to origin" matrix */
    MAT_IDN(pt_to_origin);
    MAT_DELTAS_VEC_NEG(pt_to_origin, point);

    /* build "origin to point" matrix */
    MAT_IDN(origin_to_pt);
    MAT_DELTAS_VEC_NEG(origin_to_pt, point);

    /* build "direc to zaxis" matrix */
    VSET(zaxis, 0.0, 0.0, 1.0);
    bn_mat_fromto(d_to_zaxis, direc, zaxis, tol);

    /* build "zaxis to direc" matrix */
    bn_mat_inv(zaxis_to_d, d_to_zaxis);

    /* apply change matrix...
     * t1 = change * d_to_zaxis * pt_to_origin * in
     */
    bn_mat_mul4(t1, change, d_to_zaxis, pt_to_origin, in);

    /* apply origin_to_pt matrix:
     * out = origin_to_pt * zaxis_to_d *
     * change * d_to_zaxis * pt_to_origin * in
     */
    bn_mat_mul3(out, origin_to_pt, zaxis_to_d, t1);
}


/** @} */
/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
