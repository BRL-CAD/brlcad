/*                         Q M A T H . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2014 United States Government as represented by
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
/** @file libbn/qmath.c
 *
 * @brief
 *  Quaternion math routines.
 *
 *  Unit Quaternions:
 *	Q = [ r, a ]	where r = cos(theta/2) = rotation amount
 *			    |a| = sin(theta/2) = rotation axis
 *
 *      If a = 0 we have the reals; if one coord is zero we have
 *	 complex numbers (2D rotations).
 *
 *  [r, a][s, b] = [rs - a.b, rb + sa + axb]
 *
 *       -1
 *  [r, a]   = (r - a) / (r^2 + a.a)
 *
 *  Powers of quaternions yield incremental rotations,
 *   e.g. Q^3 is rotated three times as far as Q.
 *
 *  Some operations on quaternions:
 *            -1
 *   [0, P'] = Q  [0, P]Q		Rotate a point P by quaternion Q
 *                     -1  a
 *   slerp(Q, R, a) = Q(Q  R)	Spherical linear interp: 0 < a < 1
 *
 *   bisect(P, Q) = (P + Q) / |P + Q|	Great circle bisector
 *
 *  Additions inspired by "Quaternion Calculus For Animation" by Ken Shoemake,
 *  SIGGRAPH '89 course notes for "Math for SIGGRAPH", May 1989.
 *
 */

#include "common.h"

#include <stdio.h>		/* DEBUG need stderr for now... */
#include <math.h>
#include "bu.h"
#include "vmath.h"
#include "bn.h"


void
quat_mat2quat(register fastf_t *quat, register const fastf_t *mat)
{
    fastf_t		tr;
    fastf_t	s;

#define XX	0
#define YY	5
#define ZZ	10
#define MMM(a, b)		mat[4*(a)+(b)]

    tr = mat[XX] + mat[YY] + mat[ZZ];
    if ( tr > 0.0 )  {
	s = sqrt( tr + 1.0 );
	quat[W] = s * 0.5;
	s = 0.5 / s;
	quat[X] = ( mat[6] - mat[9] ) * s;
	quat[Y] = ( mat[8] - mat[2] ) * s;
	quat[Z] = ( mat[1] - mat[4] ) * s;
	return;
    }

    /* Find dominant element of primary diagonal */
    if ( mat[YY] > mat[XX] )  {
	if ( mat[ZZ] > mat[YY] )  {
	    s = sqrt( MMM(Z, Z) - (MMM(X, X)+MMM(Y, Y)) + 1.0 );
	    quat[Z] = s * 0.5;
	    s = 0.5 / s;
	    quat[W] = (MMM(X, Y) - MMM(Y, X)) * s;
	    quat[X] = (MMM(Z, X) + MMM(X, Z)) * s;
	    quat[Y] = (MMM(Z, Y) + MMM(Y, Z)) * s;
	} else {
	    s = sqrt( MMM(Y, Y) - (MMM(Z, Z)+MMM(X, X)) + 1.0 );
	    quat[Y] = s * 0.5;
	    s = 0.5 / s;
	    quat[W] = (MMM(Z, X) - MMM(X, Z)) * s;
	    quat[Z] = (MMM(Y, Z) + MMM(Z, Y)) * s;
	    quat[X] = (MMM(Y, X) + MMM(X, Y)) * s;
	}
    } else {
	if ( mat[ZZ] > mat[XX] )  {
	    s = sqrt( MMM(Z, Z) - (MMM(X, X)+MMM(Y, Y)) + 1.0 );
	    quat[Z] = s * 0.5;
	    s = 0.5 / s;
	    quat[W] = (MMM(X, Y) - MMM(Y, X)) * s;
	    quat[X] = (MMM(Z, X) + MMM(X, Z)) * s;
	    quat[Y] = (MMM(Z, Y) + MMM(Y, Z)) * s;
	} else {
	    s = sqrt( MMM(X, X) - (MMM(Y, Y)+MMM(Z, Z)) + 1.0 );
	    quat[X] = s * 0.5;
	    s = 0.5 / s;
	    quat[W] = (MMM(Y, Z) - MMM(Z, Y)) * s;
	    quat[Y] = (MMM(X, Y) + MMM(Y, X)) * s;
	    quat[Z] = (MMM(X, Z) + MMM(Z, X)) * s;
	}
    }
#undef MMM
}


void
quat_quat2mat(register fastf_t *mat, register const fastf_t *quat)
{
    quat_t	q;

    QMOVE( q, quat );	/* private copy */
    QUNITIZE( q );

    mat[0] = 1.0 - 2.0*q[Y]*q[Y] - 2.0*q[Z]*q[Z];
    mat[1] = 2.0*q[X]*q[Y] + 2.0*q[W]*q[Z];
    mat[2] = 2.0*q[X]*q[Z] - 2.0*q[W]*q[Y];
    mat[3] = 0.0;
    mat[4] = 2.0*q[X]*q[Y] - 2.0*q[W]*q[Z];
    mat[5] = 1.0 - 2.0*q[X]*q[X] - 2.0*q[Z]*q[Z];
    mat[6] = 2.0*q[Y]*q[Z] + 2.0*q[W]*q[X];
    mat[7] = 0.0;
    mat[8] = 2.0*q[X]*q[Z] + 2.0*q[W]*q[Y];
    mat[9] = 2.0*q[Y]*q[Z] - 2.0*q[W]*q[X];
    mat[10] = 1.0 - 2.0*q[X]*q[X] - 2.0*q[Y]*q[Y];
    mat[11] = 0.0;
    mat[12] = 0.0;
    mat[13] = 0.0;
    mat[14] = 0.0;
    mat[15] = 1.0;
}


double
quat_distance(const fastf_t *q1, const fastf_t *q2)
{
    quat_t	qtemp;

    QSUB2( qtemp, q1, q2 );
    return QMAGNITUDE( qtemp );
}


void
quat_double(fastf_t *qout, const fastf_t *q1, const fastf_t *q2)
{
    quat_t	qtemp;
    double	scale;

    scale = 2.0 * QDOT( q1, q2 );
    QSCALE( qtemp, q2, scale );
    QSUB2( qout, qtemp, q1 );
    QUNITIZE( qout );
}


void
quat_bisect(fastf_t *qout, const fastf_t *q1, const fastf_t *q2)
{
    QADD2( qout, q1, q2 );
    QUNITIZE( qout );
}


void
quat_slerp(fastf_t *qout, const fastf_t *q1, const fastf_t *q2, double f)
{
    double		omega;
    double		cos_omega;
    double		invsin;
    register double	s1, s2;

    cos_omega = QDOT( q1, q2 );
    if ( (1.0 + cos_omega) > 1.0e-5 )  {
	/* cos_omega > -0.99999 */
	if ( (1.0 - cos_omega) > 1.0e-5 )  {
	    /* usual case */
	    omega = acos(cos_omega);	/* XXX atan2? */
	    invsin = 1.0 / sin(omega);
	    s1 = sin( (1.0-f)*omega ) * invsin;
	    s2 = sin( f*omega ) * invsin;
	} else {
	    /*
	     *  cos_omega > 0.99999
	     * The ends are very close to each other,
	     * use linear interpolation, to avoid divide-by-zero
	     */
	    s1 = 1.0 - f;
	    s2 = f;
	}
	QBLEND2( qout, s1, q1, s2, q2 );
    } else {
	/*
	 *  cos_omega == -1, omega = M_PI.
	 *  The ends are nearly opposite, 180 degrees (M_PI) apart.
	 */
	/* (I have no idea what permuting the elements accomplishes,
	 * perhaps it creates a perpendicular? */
	qout[X] = -q1[Y];
	qout[Y] =  q1[X];
	qout[Z] = -q1[W];
	s1 = sin( (0.5-f) * M_PI );
	s2 = sin( f * M_PI );
	VBLEND2( qout, s1, q1, s2, qout );
	qout[W] =  q1[Z];
    }
}


void
quat_sberp(fastf_t *qout, const fastf_t *q1, const fastf_t *qa, const fastf_t *qb, const fastf_t *q2, double f)
{
    quat_t	p1, p2, p3, p4, p5;

    /* Interp down the three segments */
    quat_slerp( p1, q1, qa, f );
    quat_slerp( p2, qa, qb, f );
    quat_slerp( p3, qb, q2, f );

    /* Interp down the resulting two */
    quat_slerp( p4, p1, p2, f );
    quat_slerp( p5, p2, p3, f );

    /* Interp this segment for final quaternion */
    quat_slerp( qout, p4, p5, f );
}


void
quat_make_nearest(fastf_t *q1, const fastf_t *q2)
{
    quat_t	qtemp;
    double	d1, d2;

    QSCALE( qtemp, q1, -1.0 );
    d1 = quat_distance( q1, q2 );
    d2 = quat_distance( qtemp, q2 );

    /* Choose smallest distance */
    if ( d2 < d1 ) {
	QMOVE( q1, qtemp );
    }
}


/* DEBUG ROUTINE */
void
quat_print(const char *title, const fastf_t *quat)
{
    int	i;
    vect_t	axis;

    fprintf( stderr, "QUATERNION: %s\n", title );
    for ( i = 0; i < 4; i++ )
	fprintf( stderr, "%8f  ", quat[i] );
    fprintf( stderr, "\n" );

    fprintf( stderr, "rot_angle = %8f deg", RAD2DEG * 2.0 * acos( quat[W] ) );
    VMOVE( axis, quat );
    VUNITIZE( axis );
    fprintf( stderr, ", Axis = (%f, %f, %f)\n",
	     axis[X], axis[Y], axis[Z] );
}


void
quat_exp(fastf_t *out, const fastf_t *in)
{
    fastf_t	theta;
    fastf_t	scale;

    if ( (theta = MAGNITUDE( in )) > VDIVIDE_TOL )
	scale = sin(theta)/theta;
    else
	scale = 1.0;

    VSCALE( out, in, scale );
    out[W] = cos(theta);
}


void
quat_log(fastf_t *out, const fastf_t *in)
{
    fastf_t	theta;
    fastf_t	scale;

    if ( (scale = MAGNITUDE(in)) > VDIVIDE_TOL )  {
	theta = atan2( scale, in[W] );
	scale = theta/scale;
    }

    VSCALE( out, in, scale );
    out[W] = 0.0;
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
