/*
 *		Q M A T H . C
 *
 *  Quaternion math routines.
 *
 *  Unit Quaternions:
 *	Q = [ r, a ]	where r = cos(theta/2) = rotation amount
 *			    |a| = sin(theta/2) = rotation axis
 *
 *      If a = 0 we have the reals; if one coord is zero we have
 *	 complex numbers (2D rotations).
 *
 *  [r,a][s,b] = [rs - a.b, rb + sa + axb]
 *
 *       -1
 *  [r,a]   = (r - a) / (r^2 + a.a)
 *
 *  Powers of quaternions yield incremental rotations,
 *   e.g. Q^3 is rotated three times as far as Q.
 *
 *  Some operations on quaternions:
 *            -1
 *   [0,P'] = Q  [0,P]Q		Rotate a point P by quaternion Q
 *                     -1  a
 *   slerp(Q,R,a) = Q(Q  R)	Spherical linear interp: 0 < a < 1
 *
 *   bisect(P,Q) = (P + Q) / |P + Q|	Great circle bisector
 *
 *  Note: We may wish to make a quat_t synonymous with hvect_t for clarity.
 *
 *  Author -
 *	Phil Dykstra, 25 Sep 1985
 *  
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *  
 *  Distribution Status -
 *	Public Domain, Distribution Unlimitied.
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include <stdio.h>		/* DEBUG need stderr for now... */
#include <math.h>
#include "machine.h"
#include "vmath.h"

#define	RTODEG	(180.0/3.1415925635898)

/*
 *			Q U A T _ M A T 2 Q U A T
 *
 *  Convert Matrix to Quaternion.
 */
quat_mat2quat( quat, mat )
quat_t	quat;
mat_t	mat;
{
	fastf_t	w2, x2, y2;
	FAST fastf_t	s;

	w2 = 0.25 * (1.0 + mat[0] + mat[5] + mat[10]);
	if( w2 > VDIVIDE_TOL ) {
		quat[W] = sqrt( w2 );
		s = 0.25 / quat[W];
		quat[X] = ( mat[6] - mat[9] ) * s;
		quat[Y] = ( mat[8] - mat[2] ) * s;
		quat[Z] = ( mat[1] - mat[4] ) * s;
		return;
	}

	quat[W] = 0.0;
	x2 = (-1.0 / 2.0) * (mat[5] + mat[10]);
	if( x2 > VDIVIDE_TOL ) {
		quat[X] = sqrt( x2 );
		quat[Y] = mat[1] / (2.0 * quat[X]);
		quat[Z] = mat[2] / (2.0 * quat[X]);
		return;
	}
	quat[X] = 0.0;
	y2 = (1.0 / 2.0) * (1.0 - mat[10]);
	if( y2 > VDIVIDE_TOL ) {
		quat[Y] = sqrt( y2 );
		quat[Z] = mat[6] / (2.0 * quat[Y]);
		return;
	}
	quat[Y] = 0.0;
	quat[Z] = 1.0;
	return;
}

/*
 *			Q U A T _ Q U A T 2 M A T
 *
 *  Convert Quaternion to Matrix.
 *
 * NB: This only works for UNIT quaternions.  We may get imaginary results
 *   otherwise.  We should normalize first (still yields same rotation).
 */
quat_quat2mat( mat, q )
mat_t	mat;
quat_t	q;
{
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

/*
 *			Q U A T _ D I S T A N C E
 *
 * Gives the euclidean distance between two quaternions.
 */
double
quat_distance( q1, q2 )
quat_t	q1, q2;
{
	quat_t	qtemp;

	QSUB2( qtemp, q1, q2 );
	return( QMAGNITUDE( qtemp ) );
}

/*
 *			Q U A T _ D O U B L E
 *
 * Gives the quaternion point representing twice the rotation
 *   from q1 to q2.
 *   Needed for patching Bezier curves together.
 *   A rather poor name admittedly.
 */
quat_double( qout, q1, q2 )
quat_t	qout, q1, q2;
{
	quat_t	qtemp;
	double	scale;

	scale = 2.0 * QDOT( q1, q2 );
	QSCALE( qtemp, q2, scale );
	QSUB2( qout, qtemp, q1 );
	QUNITIZE( qout );
}

/*
 *			Q U A T _ B I S E C T
 *
 * Gives the bisector of quaternions q1 and q2.
 * (Could be done with qslerp and factor 0.5)
 * [I believe they must be unit quaternions this to work]
 */
quat_bisect( qout, q1, q2 )
quat_t	qout, q1, q2;
{
	quat_t	qtemp;
	double	scale;

	QADD2( qtemp, q1, q2 );
	scale = QMAGNITUDE( qtemp );
	QSCALE( qout, qtemp, scale );
	QUNITIZE( qout );
}

/*
 *			Q U A T _ S L E R P
 *
 * Do Spherical Linear Interpolation between two quaternion
 *  by the given factor.
 *
 * I'm SURE this can be done better.
 */
void
quat_slerp( qout, q1, q2, f )
quat_t	qout, q1, q2;
double	f;
{
	double	theta, factor;
	quat_t	qtemp1, qtemp2;

	QUNITIZE( q1 );
	QUNITIZE( q2 );
	theta = acos( QDOT( q1, q2 ) );

	if( fabs(theta) < VDIVIDE_TOL )
		factor = 1.0;
	else
		factor = sin( theta * (1.0 - f) ) / sin( theta );
	QSCALE( qtemp1, q1, factor );

	if( fabs(theta) < VDIVIDE_TOL )
		factor = 0.0;
	else
		factor = sin( theta * f ) / sin( theta );
	QSCALE( qtemp2, q2, factor );

	QADD2( qout, qtemp1, qtemp2 );
}

/*
 *			Q U A T _ S B E R P
 *
 * Spherical Bezier Interpolate between four quaternions by amount f.
 * These are intended to be used as start and stop quaternions along
 *   with two control quaternions chosen to match spline segments with
 *   first order continuity.
 *
 *  Uses the method of successive bisection.
 */
quat_sberp( qout, q1, qa, qb, q2, f )
quat_t	qout, q1, qa, qb, q2;
double	f;
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

/*
 *			Q U A T _ M A K E _ N E A R E S T
 *
 * Test routine to set the quaternion q1 to that which yields the
 *   smallest rotation from q2 (of the two versions of q1 which
 *   produce the same orientation).
 *
 * Note that smallest euclidian distance implies smallest great
 *   circle distance as well (since surface is convex).
 */
quat_make_nearest( q1, q2 )
quat_t	q1, q2;
{
	quat_t	qtemp;
	double	d1, d2;

	d1 = quat_distance( q1, q2 );
	QSCALE( qtemp, q1, -1.0 );
	d2 = quat_distance( qtemp, q2 );

	/* Choose smallest distance */
	if( d2 < d1 ) {
		QMOVE( q1, qtemp );
	}
}

/*
 *			Q U A T _ P R I N T
 */
/* DEBUG ROUTINE */
quat_print( title, quat )
char	*title;
quat_t	quat;
{
	int	i;
	vect_t	axis;

	fprintf( stderr, "QUATERNION: %s\n", title );
	for( i = 0; i < 4; i++ )
		fprintf( stderr, "%8f  ", quat[i] );
	fprintf( stderr, "\n" );

	fprintf( stderr, "rot_angle = %8f deg", RTODEG * 2.0 * acos( quat[3] ) );
	VMOVE( axis, quat );
	VUNITIZE( axis );
	fprintf( stderr, ", Axis = (%f, %f, %f)\n",
		axis[X], axis[Y], axis[Z] );
}
