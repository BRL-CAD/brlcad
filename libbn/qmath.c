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
 *
 *  Author -
 *	Phil Dykstra, 25 Sep 1985
 *
 *  Additions inspired by "Quaternion Calculus For Animation" by Ken Shoemake,
 *  SIGGRAPH '89 course notes for "Math for SIGGRAPH", May 1989.
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

#include "conf.h"

#include <stdio.h>		/* DEBUG need stderr for now... */
#include <math.h>
#include "machine.h"
#include "bu.h"
#include "vmath.h"
#include "bn.h"

#ifdef M_PI
#define PI M_PI
#else
#define PI	3.14159265358979323264
#endif
#define	RTODEG	(180.0/PI)

/*
 *			Q U A T _ M A T 2 Q U A T
 *
 *  Convert Matrix to Quaternion.
 */
void
quat_mat2quat( quat, mat )
register quat_t	quat;
register mat_t	mat;
{
	fastf_t		tr;
	FAST fastf_t	s;

#define XX	0
#define YY	5
#define ZZ	10
#define M(a,b)		mat[4*(a)+(b)]

	tr = mat[XX] + mat[YY] + mat[ZZ];
	if( tr > 0.0 )  {
		s = sqrt( tr + 1.0 );
		quat[W] = s * 0.5;
		s = 0.5 / s;
		quat[X] = ( mat[6] - mat[9] ) * s;
		quat[Y] = ( mat[8] - mat[2] ) * s;
		quat[Z] = ( mat[1] - mat[4] ) * s;
		return;
	}

	/* Find dominant element of primary diagonal */
	if( mat[YY] > mat[XX] )  {
		if( mat[ZZ] > mat[YY] )  {
			s = sqrt( M(Z,Z) - (M(X,X)+M(Y,Y)) + 1.0 );
			quat[Z] = s * 0.5;
			s = 0.5 / s;
			quat[W] = (M(X,Y) - M(Y,X)) * s;
			quat[X] = (M(Z,X) + M(X,Z)) * s;
			quat[Y] = (M(Z,Y) + M(Y,Z)) * s;
		} else {
			s = sqrt( M(Y,Y) - (M(Z,Z)+M(X,X)) + 1.0 );
			quat[Y] = s * 0.5;
			s = 0.5 / s;
			quat[W] = (M(Z,X) - M(X,Z)) * s;
			quat[Z] = (M(Y,Z) + M(Z,Y)) * s;
			quat[X] = (M(Y,X) + M(X,Y)) * s;
		}
	} else {
		if( mat[ZZ] > mat[XX] )  {
			s = sqrt( M(Z,Z) - (M(X,X)+M(Y,Y)) + 1.0 );
			quat[Z] = s * 0.5;
			s = 0.5 / s;
			quat[W] = (M(X,Y) - M(Y,X)) * s;
			quat[X] = (M(Z,X) + M(X,Z)) * s;
			quat[Y] = (M(Z,Y) + M(Y,Z)) * s;
		} else {
			s = sqrt( M(X,X) - (M(Y,Y)+M(Z,Z)) + 1.0 );
			quat[X] = s * 0.5;
			s = 0.5 / s;
			quat[W] = (M(Y,Z) - M(Z,Y)) * s;
			quat[Y] = (M(X,Y) + M(Y,X)) * s;
			quat[Z] = (M(X,Z) + M(Z,X)) * s;
		}
	}
#undef M
}

/*
 *			Q U A T _ Q U A T 2 M A T
 *
 *  Convert Quaternion to Matrix.
 *
 * NB: This only works for UNIT quaternions.  We may get imaginary results
 *   otherwise.  We should normalize first (still yields same rotation).
 */
void
quat_quat2mat( mat, quat )
register mat_t	mat;
register quat_t	quat;
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
void
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
 * (Could be done with quat_slerp and factor 0.5)
 * [I believe they must be unit quaternions this to work]
 */
void
quat_bisect( qout, q1, q2 )
quat_t	qout, q1, q2;
{
	QADD2( qout, q1, q2 );
	QUNITIZE( qout );
}

/*
 *			Q U A T _ S L E R P
 *
 * Do Spherical Linear Interpolation between two unit quaternions
 *  by the given factor.
 *
 * As f goes from 0 to 1, qout goes from q1 to q2.
 * Code based on code by Ken Shoemake
 */
void
quat_slerp( qout, q1, q2, f )
quat_t	qout, q1, q2;
double	f;
{
	double		omega;
	double		cos_omega;
	double		invsin;
	register double	s1, s2;

	cos_omega = QDOT( q1, q2 );
	if( (1.0 + cos_omega) > 1.0e-5 )  {
		/* cos_omega > -0.99999 */
		if( (1.0 - cos_omega) > 1.0e-5 )  {
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
		 *  cos_omega == -1, omega = PI.
		 *  The ends are nearly opposite, 180 degrees (PI) apart.
		 */
		/* (I have no idea what permuting the elements accomplishes,
		 * perhaps it creates a perpendicular? */
		qout[X] = -q1[Y];
		qout[Y] =  q1[X];
		qout[Z] = -q1[W];
		s1 = sin( (0.5-f) * PI );
		s2 = sin( f * PI );
		VBLEND2( qout, s1, q1, s2, qout );
		qout[W] =  q1[Z];
	}
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
void
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
 *  Set the quaternion q1 to the quaternion which yields the
 *   smallest rotation from q2 (of the two versions of q1 which
 *   produce the same orientation).
 *
 * Note that smallest euclidian distance implies smallest great
 *   circle distance as well (since surface is convex).
 */
void
quat_make_nearest( q1, q2 )
quat_t	q1, q2;
{
	quat_t	qtemp;
	double	d1, d2;

	QSCALE( qtemp, q1, -1.0 );
	d1 = quat_distance( q1, q2 );
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
void
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

	fprintf( stderr, "rot_angle = %8f deg", RTODEG * 2.0 * acos( quat[W] ) );
	VMOVE( axis, quat );
	VUNITIZE( axis );
	fprintf( stderr, ", Axis = (%f, %f, %f)\n",
		axis[X], axis[Y], axis[Z] );
}

/*
 *			Q U A T _ E X P
 *
 *  Exponentiate a quaternion, assuming that the scalar part is 0.
 *  Code by Ken Shoemake.
 */
void
quat_exp( out, in )
quat_t	out, in;
{
	FAST fastf_t	theta;
	FAST fastf_t	scale;

	if( (theta = MAGNITUDE( in )) > VDIVIDE_TOL )
		scale = sin(theta)/theta;
	else
		scale = 1.0;

	VSCALE( out, in, scale );
	out[W] = cos(theta);
}

/*
 *			Q U A T _ L O G
 *
 *  Take the natural logarithm of a unit quaternion.
 *  Code by Ken Shoemake.
 */
void
quat_log( out, in )
quat_t	out, in;
{
	FAST fastf_t	theta;
	FAST fastf_t	scale;

	if( (scale = MAGNITUDE(in)) > VDIVIDE_TOL )  {
		theta = atan2( scale, in[W] );
		scale = theta/scale;
	}

	VSCALE( out, in, scale );
	out[W] = 0.0;
}
