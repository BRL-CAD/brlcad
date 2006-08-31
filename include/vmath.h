/*                         V M A T H . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2006 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file vmath.h
 *
 *  This header file defines many commonly used 3D vector math macros,
 *  and operates on vect_t, point_t, mat_t, and quat_t objects.
 *
 *  Note that while many people in the computer graphics field use
 *  post-multiplication with row vectors (ie, vector * matrix * matrix ...)
 *  the BRL-CAD system uses the more traditional representation of
 *  column vectors (ie, ... matrix * matrix * vector).  (The matrices
 *  in these two representations are the transposes of each other). Therefore,
 *  when transforming a vector by a matrix, pre-multiplication is used, ie:
 *
 *		view_vec = model2view_mat * model_vec
 *
 *  Furthermore, additional transformations are multiplied on the left, ie:
 *
 *		vec'  =  T1 * vec
 *		vec'' =  T2 * T1 * vec  =  T2 * vec'
 *
 *  The most notable implication of this is the location of the
 *  "delta" (translation) values in the matrix, ie:
 *
 *        x'     ( R0   R1   R2   Dx )      x
 *        y' =   ( R4   R5   R6   Dy )   *  y
 *        z'     ( R8   R9   R10  Dz )      z
 *        w'     (  0    0    0   1/s)      w
 *
 *  Note -
 *	vect_t objects are 3-tuples
 *	hvect_t objects are 4-tuples
 *
 *  Most of these macros require that the result be in
 *  separate storage, distinct from the input parameters,
 *  except where noted.
 *
 *  When writing macros like this, it is very important that any
 *  variables which are declared within code blocks inside a macro
 *  start with an underscore.  This prevents any name conflicts with
 *  user-provided parameters.  For example:
 *	{ register double _f; stuff; }
 *
 *  Author -
 *	Michael John Muuss
 *
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005
 *
 *  Include Sequencing -
@code
	#include <stdio.h>
	#include <math.h>
	#include "machine.h"	/_* For fastf_t definition on this machine *_/
	#include "vmath.h"
@endcode
 *
 *  Libraries Used -
 *	-lm -lc
 *
 *  $Header$
 */

#ifndef VMATH_H
#define VMATH_H seen

#include "common.h"

/* for sqrt(), sin(), cos(), rint(), etc */
#ifdef HAVE_MATH_H
#  include <math.h>
#endif

/* for floating point tolerances and other math constants */
#ifdef HAVE_FLOAT_H
#  include <float.h>
#endif

__BEGIN_DECLS

#ifndef M_PI
#  define M_E		2.7182818284590452354	/* e */
#  define M_LOG2E	1.4426950408889634074	/* log_2 e */
#  define M_LOG10E	0.43429448190325182765	/* log_10 e */
#  define M_LN2		0.69314718055994530942	/* log_e 2 */
#  define M_LN10	2.30258509299404568402	/* log_e 10 */
#  define M_PI		3.14159265358979323846	/* pi */
#  define M_PI_2	1.57079632679489661923	/* pi/2 */
#  define M_PI_4	0.78539816339744830962	/* pi/4 */
#  define M_1_PI	0.31830988618379067154	/* 1/pi */
#  define M_2_PI	0.63661977236758134308	/* 2/pi */
#  define M_2_SQRTPI	1.12837916709551257390	/* 2/sqrt(pi) */
#  define M_SQRT2	1.41421356237309504880	/* sqrt(2) */
#  define M_SQRT1_2	0.70710678118654752440	/* 1/sqrt(2) */
#  define M_SQRT2_DIV2	0.70710678118654752440  /* 1/sqrt(2) */
#endif
#ifndef PI
#  define PI M_PI
#endif

/* minimum computation tolerances */
#ifdef vax
#  define VDIVIDE_TOL	( 1.0e-10 )
#  define VUNITIZE_TOL	( 1.0e-7 )
#else
#  ifdef DBL_EPSILON
#    define VDIVIDE_TOL		( DBL_EPSILON )
#  else
#    define VDIVIDE_TOL		( 1.0e-20 )
#  endif
#  ifdef FLT_EPSILON
#    define VUNITIZE_TOL	( FLT_EPSILON )
#  else
#    define VUNITIZE_TOL	( 1.0e-15 )
#  endif
#endif

#define ELEMENTS_PER_VECT	3	/* # of fastf_t's per vect_t */
#define ELEMENTS_PER_PT         3
#define HVECT_LEN		4	/* # of fastf_t's per hvect_t */
#define HPT_LEN			4
#define ELEMENTS_PER_PLANE	4
#define ELEMENTS_PER_MAT	(4*4)

/*
 * Types for matrixes and vectors.
 */
typedef	fastf_t	mat_t[ELEMENTS_PER_MAT];
typedef	fastf_t	*matp_t;

typedef	fastf_t	vect_t[ELEMENTS_PER_VECT];
typedef	fastf_t	*vectp_t;

typedef fastf_t	point_t[ELEMENTS_PER_PT];
typedef fastf_t	*pointp_t;

typedef fastf_t point2d_t[2];

typedef fastf_t hvect_t[HVECT_LEN];
typedef fastf_t hpoint_t[HPT_LEN];

#define quat_t	hvect_t		/* 4-element quaternion */

/**
 * return truthfully whether a value is within some epsilon from zero
 */
#define NEAR_ZERO(val,epsilon)	( ((val) > -epsilon) && ((val) < epsilon) )

/**
 * clamp a value to a low/high number
 */
#define CLAMP(_v, _l, _h) if ((_v) < (_l)) _v = _l; else if ((_v) > (_h)) _v = _h

/*
 *  Definition of a plane equation:
 *  A plane is defined by a unit-length outward pointing normal vector (N),
 *  and the perpendicular (shortest) distance from the origin to the plane
 *  (in element N[3]).
 *
 *  The plane consists of all points P=(x,y,z) such that
 *	VDOT(P,N) - N[3] == 0
 *  that is,
 *	N[X]*x + N[Y]*y + N[Z]*z - N[3] == 0
 *
 *  The inside of the halfspace bounded by the plane
 *  consists of all points P such that
 *	VDOT(P,N) - N[3] <= 0
 *
 *  A ray with direction D is classified w.r.t. the plane by
 *
 *	VDOT(D,N) < 0	ray enters halfspace defined by plane
 *	VDOT(D,N) == 0	ray is parallel to plane
 *	VDOT(D,N) > 0	ray exits halfspace defined by plane
 */
typedef fastf_t	plane_t[ELEMENTS_PER_PLANE];

/* Compute distance from a point to a plane */
#define DIST_PT_PLANE(_pt, _pl) (VDOT(_pt, _pl) - (_pl)[H])

/* Compute distance between two points */
#define DIST_PT_PT(a,b)		sqrt( \
	((a)[X]-(b)[X])*((a)[X]-(b)[X]) + \
	((a)[Y]-(b)[Y])*((a)[Y]-(b)[Y]) + \
	((a)[Z]-(b)[Z])*((a)[Z]-(b)[Z]) )

/* Element names in homogeneous vector (4-tuple) */
#define	X	0
#define	Y	1
#define Z	2
#define H	3
#define W	H

/* Locations of deltas in 4x4 Homogenous Transform matrix */
#define MDX	3
#define MDY	7
#define MDZ	11
#define MAT_DELTAS(m,x,y,z)	{ \
			(m)[MDX] = (x); \
			(m)[MDY] = (y); \
			(m)[MDZ] = (z); }

#define MAT_DELTAS_VEC(_m,_v)	\
			MAT_DELTAS(_m, (_v)[X], (_v)[Y], (_v)[Z] )
#define MAT_DELTAS_VEC_NEG(_m,_v)	\
			MAT_DELTAS(_m,-(_v)[X],-(_v)[Y],-(_v)[Z] )
#define MAT_DELTAS_GET(_v,_m)	{ \
			(_v)[X] = (_m)[MDX]; \
			(_v)[Y] = (_m)[MDY]; \
			(_v)[Z] = (_m)[MDZ]; }
#define MAT_DELTAS_GET_NEG(_v,_m)	{ \
			(_v)[X] = -(_m)[MDX]; \
			(_v)[Y] = -(_m)[MDY]; \
			(_v)[Z] = -(_m)[MDZ]; }

/* Locations of scaling values in 4x4 Homogenous Transform matrix */
#define MSX	0
#define MSY	5
#define MSZ	10
#define MSA	15

#define MAT_SCALE(_m, _x, _y, _z) { \
	(_m)[MSX] = _x; \
	(_m)[MSY] = _y; \
	(_m)[MSZ] = _z; }

#define MAT_SCALE_VEC(_m, _v) {\
	(_m)[MSX] = (_v)[X]; \
	(_m)[MSY] = (_v)[Y]; \
	(_m)[MSZ] = (_v)[Z]; }

#define MAT_SCALE_ALL(_m, _s) (_m)[MSA] = (_s)


/* Macro versions of librt/mat.c functions, for when speed really matters */
#define MAT_ZERO(m)	{ \
	(m)[0] = (m)[1] = (m)[2] = (m)[3] = \
	(m)[4] = (m)[5] = (m)[6] = (m)[7] = \
	(m)[8] = (m)[9] = (m)[10] = (m)[11] = \
	(m)[12] = (m)[13] = (m)[14] = (m)[15] = 0.0;}

/* #define MAT_ZERO(m)	{\
	register int _j; \
	for(_j=0; _j<16; _j++) (m)[_j]=0.0; }
  */

#define MAT_IDN(m)	{\
	(m)[1] = (m)[2] = (m)[3] = (m)[4] =\
	(m)[6] = (m)[7] = (m)[8] = (m)[9] = \
	(m)[11] = (m)[12] = (m)[13] = (m)[14] = 0.0;\
	(m)[0] = (m)[5] = (m)[10] = (m)[15] = 1.0;}

/* #define MAT_IDN(m)	{\
	int _j;	for(_j=0;_j<16;_j++) (m)[_j]=0.0;\
	(m)[0] = (m)[5] = (m)[10] = (m)[15] = 1.0;}
  */

#define MAT_COPY( d, s )	{ \
	(d)[0] = (s)[0];\
	(d)[1] = (s)[1];\
	(d)[2] = (s)[2];\
	(d)[3] = (s)[3];\
	(d)[4] = (s)[4];\
	(d)[5] = (s)[5];\
	(d)[6] = (s)[6];\
	(d)[7] = (s)[7];\
	(d)[8] = (s)[8];\
	(d)[9] = (s)[9];\
	(d)[10] = (s)[10];\
	(d)[11] = (s)[11];\
	(d)[12] = (s)[12];\
	(d)[13] = (s)[13];\
	(d)[14] = (s)[14];\
	(d)[15] = (s)[15]; }

/* #define MAT_COPY(o,m)   VMOVEN(o,m,16)  */

/* Set vector at `a' to have coordinates `b', `c', `d' */
#define VSET(a,b,c,d)	{ \
			(a)[X] = (b);\
			(a)[Y] = (c);\
			(a)[Z] = (d); }

/* Set all elements of vector to same scalar value */
#define VSETALL(a,s)	{ (a)[X] = (a)[Y] = (a)[Z] = (s); }

#define VSETALLN(v,s,n)  {\
	register int _j;\
	for (_j=0; _j<n; _j++) v[_j]=(s);}

/* Transfer vector at `b' to vector at `a' */
#define VMOVE(a,b)	{ \
			(a)[X] = (b)[X];\
			(a)[Y] = (b)[Y];\
			(a)[Z] = (b)[Z]; }

/* Transfer vector of length `n' at `b' to vector at `a' */
#define VMOVEN(a,b,n) \
	{ register int _vmove; \
	for(_vmove = 0; _vmove < (n); _vmove++) \
		(a)[_vmove] = (b)[_vmove]; \
	}

/* Move a homogeneous 4-tuple */
#define HMOVE(a,b)	{ \
			(a)[X] = (b)[X];\
			(a)[Y] = (b)[Y];\
			(a)[Z] = (b)[Z];\
			(a)[W] = (b)[W]; }

/* This naming convention seems better than the VMOVE_2D version below */
#define V2MOVE(a,b)	{ \
			(a)[X] = (b)[X];\
			(a)[Y] = (b)[Y]; }

/* Reverse the direction of b and store it in a */
#define VREVERSE(a,b)	{ \
			(a)[X] = -(b)[X]; \
			(a)[Y] = -(b)[Y]; \
			(a)[Z] = -(b)[Z]; }

/* Same as VREVERSE, but for a 4-tuple.  Also useful on plane_t objects */
#define HREVERSE(a,b)	{ \
			(a)[X] = -(b)[X]; \
			(a)[Y] = -(b)[Y]; \
			(a)[Z] = -(b)[Z]; \
			(a)[W] = -(b)[W]; }

/* Add vectors at `b' and `c', store result at `a' */
#ifdef SHORT_VECTORS
#define VADD2(a,b,c) VADD2N(a,b,c, 3)
#else
#define VADD2(a,b,c)	{ \
			(a)[X] = (b)[X] + (c)[X];\
			(a)[Y] = (b)[Y] + (c)[Y];\
			(a)[Z] = (b)[Z] + (c)[Z]; }
#endif /* SHORT_VECTORS */

/* Add vectors of length `n' at `b' and `c', store result at `a' */
#define VADD2N(a,b,c,n) \
	{ register int _vadd2; \
	for(_vadd2 = 0; _vadd2 < (n); _vadd2++) \
		(a)[_vadd2] = (b)[_vadd2] + (c)[_vadd2]; \
	}

#define V2ADD2(a,b,c)	{ \
			(a)[X] = (b)[X] + (c)[X];\
			(a)[Y] = (b)[Y] + (c)[Y];}

/* Subtract vector at `c' from vector at `b', store result at `a' */
#ifdef SHORT_VECTORS
#define VSUB2(a,b,c) 	VSUB2N(a,b,c, 3)
#else
#define VSUB2(a,b,c)	{ \
			(a)[X] = (b)[X] - (c)[X];\
			(a)[Y] = (b)[Y] - (c)[Y];\
			(a)[Z] = (b)[Z] - (c)[Z]; }
#endif /* SHORT_VECTORS */

/* Subtract `n' length vector at `c' from vector at `b', store result at `a' */
#define VSUB2N(a,b,c,n) \
	{ register int _vsub2; \
	for(_vsub2 = 0; _vsub2 < (n); _vsub2++) \
		(a)[_vsub2] = (b)[_vsub2] - (c)[_vsub2]; \
	}

#define V2SUB2(a,b,c)	{ \
			(a)[X] = (b)[X] - (c)[X];\
			(a)[Y] = (b)[Y] - (c)[Y];}

/* Vectors:  A = B - C - D */
#ifdef SHORT_VECTORS
#define VSUB3(a,b,c,d) VSUB3(a,b,c,d, 3)
#else
#define VSUB3(a,b,c,d)	{ \
			(a)[X] = (b)[X] - (c)[X] - (d)[X];\
			(a)[Y] = (b)[Y] - (c)[Y] - (d)[Y];\
			(a)[Z] = (b)[Z] - (c)[Z] - (d)[Z]; }
#endif /* SHORT_VECTORS */

/* Vectors:  A = B - C - D for vectors of length `n' */
#define VSUB3N(a,b,c,d,n) \
	{ register int _vsub3; \
	for(_vsub3 = 0; _vsub3 < (n); _vsub3++) \
		(a)[_vsub3] = (b)[_vsub3] - (c)[_vsub3] - (d)[_vsub3]; \
	}

/* Add 3 vectors at `b', `c', and `d', store result at `a' */
#ifdef SHORT_VECTORS
#define VADD3(a,b,c,d) VADD3N(a,b,c,d, 3)
#else
#define VADD3(a,b,c,d)	{ \
			(a)[X] = (b)[X] + (c)[X] + (d)[X];\
			(a)[Y] = (b)[Y] + (c)[Y] + (d)[Y];\
			(a)[Z] = (b)[Z] + (c)[Z] + (d)[Z]; }
#endif /* SHORT_VECTORS */

/* Add 3 vectors of length `n' at `b', `c', and `d', store result at `a' */
#define VADD3N(a,b,c,d,n) \
	{ register int _vadd3; \
	for(_vadd3 = 0; _vadd3 < (n); _vadd3++) \
		(a)[_vadd3] = (b)[_vadd3] + (c)[_vadd3] + (d)[_vadd3]; \
	}

/* Add 4 vectors at `b', `c', `d', and `e', store result at `a' */
#ifdef SHORT_VECTORS
#define VADD4(a,b,c,d,e) VADD4N(a,b,c,d,e, 3)
#else
#define VADD4(a,b,c,d,e) { \
			(a)[X] = (b)[X] + (c)[X] + (d)[X] + (e)[X];\
			(a)[Y] = (b)[Y] + (c)[Y] + (d)[Y] + (e)[Y];\
			(a)[Z] = (b)[Z] + (c)[Z] + (d)[Z] + (e)[Z]; }
#endif /* SHORT_VECTORS */

/* Add 4 `n' length vectors at `b', `c', `d', and `e', store result at `a' */
#define VADD4N(a,b,c,d,e,n) \
	{ register int _vadd4; \
	for(_vadd4 = 0; _vadd4 < (n); _vadd4++) \
		(a)[_vadd4] = (b)[_vadd4] + (c)[_vadd4] + (d)[_vadd4] + (e)[_vadd4];\
	}

/* Scale vector at `b' by scalar `c', store result at `a' */
#ifdef SHORT_VECTORS
#define VSCALE(a,b,c) VSCALEN(a,b,c, 3)
#else
#define VSCALE(a,b,c)	{ \
			(a)[X] = (b)[X] * (c);\
			(a)[Y] = (b)[Y] * (c);\
			(a)[Z] = (b)[Z] * (c); }
#endif /* SHORT_VECTORS */

#define HSCALE(a,b,c)	{ \
			(a)[X] = (b)[X] * (c);\
			(a)[Y] = (b)[Y] * (c);\
			(a)[Z] = (b)[Z] * (c);\
			(a)[W] = (b)[W] * (c); }

/* Scale vector of length `n' at `b' by scalar `c', store result at `a' */
#define VSCALEN(a,b,c,n) \
	{ register int _vscale; \
	for(_vscale = 0; _vscale < (n); _vscale++) \
		(a)[_vscale] = (b)[_vscale] * (c); \
	}

#define V2SCALE(a,b,c)	{ \
			(a)[X] = (b)[X] * (c);\
			(a)[Y] = (b)[Y] * (c); }

/* Normalize vector `a' to be a unit vector */
#ifdef SHORT_VECTORS
#define VUNITIZE(a) { \
	register double _f = MAGSQ(a); \
	register int _vunitize; \
	if ( ! NEAR_ZERO( _f-1.0, VUNITIZE_TOL ) ) { \
		_f = sqrt( _f ); \
		if( _f < VDIVIDE_TOL ) { VSETALL( (a), 0.0 ); } else { \
			_f = 1.0/_f; \
			for(_vunitize = 0; _vunitize < 3; _vunitize++) \
				(a)[_vunitize] *= _f; \
		} \
	} \
}
#else
#define VUNITIZE(a)	{ \
	register double _f = MAGSQ(a); \
	if ( ! NEAR_ZERO( _f-1.0, VUNITIZE_TOL ) ) { \
		_f = sqrt( _f ); \
		if( _f < VDIVIDE_TOL ) { VSETALL( (a), 0.0 ); } else { \
			_f = 1.0/_f; \
			(a)[X] *= _f; (a)[Y] *= _f; (a)[Z] *= _f; \
		} \
	} \
}
#endif /* SHORT_VECTORS */

/* If vector magnitude is too small, return an error code */
#define VUNITIZE_RET(a,ret)	{ \
			register double _f; _f = MAGNITUDE(a); \
			if( _f < VDIVIDE_TOL ) return(ret); \
			_f = 1.0/_f; \
			(a)[X] *= _f; (a)[Y] *= _f; (a)[Z] *= _f; }

/*
 *  Find the sum of two points, and scale the result.
 *  Often used to find the midpoint.
 */
#ifdef SHORT_VECTORS
#define VADD2SCALE( o, a, b, s )	VADD2SCALEN( o, a, b, s, 3 )
#else
#define VADD2SCALE( o, a, b, s )	{ \
					(o)[X] = ((a)[X] + (b)[X]) * (s); \
					(o)[Y] = ((a)[Y] + (b)[Y]) * (s); \
					(o)[Z] = ((a)[Z] + (b)[Z]) * (s); }
#endif

#define VADD2SCALEN( o, a, b, n ) \
	{ register int _vadd2scale; \
	for( _vadd2scale = 0; _vadd2scale < (n); _vadd2scale++ ) \
		(o)[_vadd2scale] = ((a)[_vadd2scale] + (b)[_vadd2scale]) * (s); \
	}

/*
 *  Find the difference between two points, and scale result.
 *  Often used to compute bounding sphere radius given rpp points.
 */
#ifdef SHORT_VECTORS
#define VSUB2SCALE( o, a, b, s )	VSUB2SCALEN( o, a, b, s, 3 )
#else
#define VSUB2SCALE( o, a, b, s )	{ \
					(o)[X] = ((a)[X] - (b)[X]) * (s); \
					(o)[Y] = ((a)[Y] - (b)[Y]) * (s); \
					(o)[Z] = ((a)[Z] - (b)[Z]) * (s); }
#endif

#define VSUB2SCALEN( o, a, b, n ) \
	{ register int _vsub2scale; \
	for( _vsub2scale = 0; _vsub2scale < (n); _vsub2scale++ ) \
		(o)[_vsub2scale] = ((a)[_vsub2scale] - (b)[_vsub2scale]) * (s); \
	}


/*
 *  Combine together several vectors, scaled by a scalar
 */
#ifdef SHORT_VECTORS
#define VCOMB3(o, a,b, c,d, e,f)	VCOMB3N(o, a,b, c,d, e,f, 3)
#else
#define VCOMB3(o, a,b, c,d, e,f)	{\
	(o)[X] = (a) * (b)[X] + (c) * (d)[X] + (e) * (f)[X];\
	(o)[Y] = (a) * (b)[Y] + (c) * (d)[Y] + (e) * (f)[Y];\
	(o)[Z] = (a) * (b)[Z] + (c) * (d)[Z] + (e) * (f)[Z];}
#endif /* SHORT_VECTORS */

#define VCOMB3N(o, a,b, c,d, e,f, n)	{\
	{ register int _vcomb3; \
	for(_vcomb3 = 0; _vcomb3 < (n); _vcomb3++) \
		(o)[_vcomb3] = (a) * (b)[_vcomb3] + (c) * (d)[_vcomb3] + (e) * (f)[_vcomb3]; \
	} }

#ifdef SHORT_VECTORS
#define VCOMB2(o, a,b, c,d)	VCOMB2N(o, a,b, c,d, 3)
#else
#define VCOMB2(o, a,b, c,d)	{\
	(o)[X] = (a) * (b)[X] + (c) * (d)[X];\
	(o)[Y] = (a) * (b)[Y] + (c) * (d)[Y];\
	(o)[Z] = (a) * (b)[Z] + (c) * (d)[Z];}
#endif /* SHORT_VECTORS */

#define VCOMB2N(o, a,b, c,d, n)	{\
	{ register int _vcomb2; \
	for(_vcomb2 = 0; _vcomb2 < (n); _vcomb2++) \
		(o)[_vcomb2] = (a) * (b)[_vcomb2] + (c) * (d)[_vcomb2]; \
	} }

#define VJOIN4(a,b,c,d,e,f,g,h,i,j)	{ \
	(a)[X] = (b)[X] + (c)*(d)[X] + (e)*(f)[X] + (g)*(h)[X] + (i)*(j)[X];\
	(a)[Y] = (b)[Y] + (c)*(d)[Y] + (e)*(f)[Y] + (g)*(h)[Y] + (i)*(j)[Y];\
	(a)[Z] = (b)[Z] + (c)*(d)[Z] + (e)*(f)[Z] + (g)*(h)[Z] + (i)*(j)[Z]; }

#define VJOIN3(a,b,c,d,e,f,g,h)		{ \
	(a)[X] = (b)[X] + (c)*(d)[X] + (e)*(f)[X] + (g)*(h)[X];\
	(a)[Y] = (b)[Y] + (c)*(d)[Y] + (e)*(f)[Y] + (g)*(h)[Y];\
	(a)[Z] = (b)[Z] + (c)*(d)[Z] + (e)*(f)[Z] + (g)*(h)[Z]; }

/* Compose vector at `a' of:
 *	Vector at `b' plus
 *	scalar `c' times vector at `d' plus
 *	scalar `e' times vector at `f'
 */
#ifdef SHORT_VECTORS
#define VJOIN2(a,b,c,d,e,f)	VJOIN2N(a,b,c,d,e,f,3)
#else
#define VJOIN2(a,b,c,d,e,f)	{ \
	(a)[X] = (b)[X] + (c) * (d)[X] + (e) * (f)[X];\
	(a)[Y] = (b)[Y] + (c) * (d)[Y] + (e) * (f)[Y];\
	(a)[Z] = (b)[Z] + (c) * (d)[Z] + (e) * (f)[Z]; }
#endif /* SHORT_VECTORS */

#define VJOIN2N(a,b,c,d,e,f,n)	\
	{ register int _vjoin2; \
	for(_vjoin2 = 0; _vjoin2 < (n); _vjoin2++) \
		(a)[_vjoin2] = (b)[_vjoin2] + (c) * (d)[_vjoin2] + (e) * (f)[_vjoin2]; \
	}

#ifdef SHORT_VECTORS
#define VJOIN1(a,b,c,d)		VJOIN1N(a,b,c,d,3)
#else
#define VJOIN1(a,b,c,d) 	{ \
	(a)[X] = (b)[X] + (c) * (d)[X];\
	(a)[Y] = (b)[Y] + (c) * (d)[Y];\
	(a)[Z] = (b)[Z] + (c) * (d)[Z]; }
#endif /* SHORT_VECTORS */

#define VJOIN1N(a,b,c,d,n) \
	{ register int _vjoin1; \
	for(_vjoin1 = 0; _vjoin1 < (n); _vjoin1++) \
		(a)[_vjoin1] = (b)[_vjoin1] + (c) * (d)[_vjoin1]; \
	}

#define HJOIN1(a,b,c,d)	{ \
			(a)[X] = (b)[X] + (c) * (d)[X]; \
			(a)[Y] = (b)[Y] + (c) * (d)[Y]; \
			(a)[Z] = (b)[Z] + (c) * (d)[Z]; \
			(a)[W] = (b)[W] + (c) * (d)[W]; }

#define V2JOIN1(a,b,c,d) 	{ \
			(a)[X] = (b)[X] + (c) * (d)[X];\
			(a)[Y] = (b)[Y] + (c) * (d)[Y]; }

/*
 *  Blend into vector `a'
 *	scalar `b' times vector at `c' plus
 *	scalar `d' times vector at `e'
 */
#ifdef SHORT_VECTORS
#define VBLEND2(a,b,c,d,e)	VBLEND2N(a,b,c,d,e,3)
#else
#define VBLEND2(a,b,c,d,e)	{ \
	(a)[X] = (b) * (c)[X] + (d) * (e)[X];\
	(a)[Y] = (b) * (c)[Y] + (d) * (e)[Y];\
	(a)[Z] = (b) * (c)[Z] + (d) * (e)[Z]; }
#endif /* SHORT_VECTORS */

#define VBLEND2N(a,b,c,d,e,n)	\
	{ register int _vblend2; \
	for(_vblend2 = 0; _vblend2 < (n); _vblend2++) \
		(a)[_vblend2] = (b) * (c)[_vblend2] + (d) * (e)[_vblend2]; \
	}

/* Return scalar magnitude squared of vector at `a' */
#define MAGSQ(a)	( (a)[X]*(a)[X] + (a)[Y]*(a)[Y] + (a)[Z]*(a)[Z] )
#define MAG2SQ(a)	( (a)[X]*(a)[X] + (a)[Y]*(a)[Y] )

/* Return scalar magnitude of vector at `a' */
#define MAGNITUDE(a)	sqrt( MAGSQ( a ) )

/*
 *  Store cross product of vectors at `b' and `c' in vector at `a'.
 *  Note that the "right hand rule" applies:
 *  If closing your right hand goes from `b' to `c', then your
 *  thumb points in the direction of the cross product.
 *
 *  If the angle from `b' to `c' goes clockwise, then
 *  the result vector points "into" the plane (inward normal).
 *  Example:  b=(0,1,0), c=(1,0,0), then bXc=(0,0,-1).
 *
 *  If the angle from `b' to `c' goes counter-clockwise, then
 *  the result vector points "out" of the plane.
 *  This outward pointing normal is the BRL convention.
 */
#define VCROSS(a,b,c)	{ \
			(a)[X] = (b)[Y] * (c)[Z] - (b)[Z] * (c)[Y];\
			(a)[Y] = (b)[Z] * (c)[X] - (b)[X] * (c)[Z];\
			(a)[Z] = (b)[X] * (c)[Y] - (b)[Y] * (c)[X]; }

/* Compute dot product of vectors at `a' and `b' */
#define VDOT(a,b)	( (a)[X]*(b)[X] + (a)[Y]*(b)[Y] + (a)[Z]*(b)[Z] )

#define V2DOT(a,b)	( (a)[X]*(b)[X] + (a)[Y]*(b)[Y] )

/* Subtract two points to make a vector, dot with another vector */
#define VSUB2DOT(_pt2, _pt, _vec)	( \
	((_pt2)[X] - (_pt)[X]) * (_vec)[X] + \
	((_pt2)[Y] - (_pt)[Y]) * (_vec)[Y] + \
	((_pt2)[Z] - (_pt)[Z]) * (_vec)[Z] )

/* Turn a vector into comma-separated list of elements, for subroutine args */
#define V2ARGS(a)	(a)[X], (a)[Y]
#define V3ARGS(a)	(a)[X], (a)[Y], (a)[Z]
#define V4ARGS(a)	(a)[X], (a)[Y], (a)[Z], (a)[W]

/* integer clamped versions of the previous arg macros */
#define V2INTCLAMPARGS(a)	INTCLAMP((a)[X]), INTCLAMP((a)[Y])
#define V3INTCLAMPARGS(a)	INTCLAMP((a)[X]), INTCLAMP((a)[Y]), INTCLAMP((a)[Z])
#define V4INTCLAMPARGS(a)	INTCLAMP((a)[X]), INTCLAMP((a)[Y]), INTCLAMP((a)[Z]), INTCLAMP((a)[W])

/* Print vector name and components on stderr */
#define V2PRINT(a,b)	\
	(void)fprintf(stderr,"%s (%g, %g)\n", a, V2ARGS(b) );
#define VPRINT(a,b)	\
	(void)fprintf(stderr,"%s (%g, %g, %g)\n", a, V3ARGS(b) );
#define HPRINT(a,b)	\
	(void)fprintf(stderr,"%s (%g, %g, %g, %g)\n", a, V4ARGS(b) );

/* integer clamped versions of the previous print macros */
#define V2INTCLAMPPRINT(a,b)	\
	(void)fprintf(stderr,"%s (%g, %g)\n", a, V2INTCLAMPARGS(b) );
#define VINTCLAMPPRINT(a,b)	\
	(void)fprintf(stderr,"%s (%g, %g, %g)\n", a, V3INTCLAMPARGS(b) );
#define HINTCLAMPPRINT(a,b)	\
	(void)fprintf(stderr,"%s (%g, %g, %g, %g)\n", a, V4INTCLAMPARGS(b) );

#ifdef __cplusplus
#define CPP_V3PRINT( _os, _title, _p )	(_os) << (_title) << "=(" << \
	(_p)[X] << ", " << (_p)[Y] << ")\n";
#define CPP_VPRINT( _os, _title, _p )	(_os) << (_title) << "=(" << \
	(_p)[X] << ", " << (_p)[Y] << ", " << (_p)[Z] << ")\n";
#define CPP_HPRINT( _os, _title, _p )	(_os) << (_title) << "=(" << \
	(_p)[X] << ", " << (_p)[Y] << ", " << (_p)[Z] << "," << (_p)[W]<< ")\n";
#endif

/**
 * if a value is within computation tolerance of an integer, clamp the
 * value to that integer.  XXX - should use VDIVIDE_TOL here, but
 * cannot yet until floats are replaced universally with fastf_t's
 * since their epsilon is considerably less than that of a double.
 */
#define INTCLAMP(_a)	( NEAR_ZERO((_a) - rint(_a), VUNITIZE_TOL) ? rint(_a) : (_a) )

/* Vector element multiplication.  Really: diagonal matrix X vect */
#ifdef SHORT_VECTORS
#define VELMUL(a,b,c) \
	{ register int _velmul; \
	for(_velmul = 0; _velmul < 3; _velmul++) \
		(a)[_velmul] = (b)[_velmul] * (c)[_velmul]; \
	}
#else
#define VELMUL(a,b,c) 	{ \
	(a)[X] = (b)[X] * (c)[X];\
	(a)[Y] = (b)[Y] * (c)[Y];\
	(a)[Z] = (b)[Z] * (c)[Z]; }
#endif /* SHORT_VECTORS */

#ifdef SHORT_VECTORS
#define VELMUL3(a,b,c,d) \
	{ register int _velmul; \
	for(_velmul = 0; _velmul < 3; _velmul++) \
		(a)[_velmul] = (b)[_velmul] * (c)[_velmul] * (d)[_velmul]; \
	}
#else
#define VELMUL3(a,b,c,d) 	{ \
	(a)[X] = (b)[X] * (c)[X] * (d)[X];\
	(a)[Y] = (b)[Y] * (c)[Y] * (d)[Y];\
	(a)[Z] = (b)[Z] * (c)[Z] * (d)[Z]; }
#endif /* SHORT_VECTORS */

/* Similar to VELMUL */
#define VELDIV(a,b,c)	{ \
	(a)[0] = (b)[0] / (c)[0];\
	(a)[1] = (b)[1] / (c)[1];\
	(a)[2] = (b)[2] / (c)[2]; }

/* Given a direction vector, compute the inverses of each element. */
/* When division by zero would have occured, mark inverse as INFINITY. */
#define VINVDIR( _inv, _dir )	{ \
	if( (_dir)[X] < -SQRT_SMALL_FASTF || (_dir)[X] > SQRT_SMALL_FASTF )  { \
		(_inv)[X]=1.0/(_dir)[X]; \
	} else { \
		(_dir)[X] = 0.0; \
		(_inv)[X] = INFINITY; \
	} \
	if( (_dir)[Y] < -SQRT_SMALL_FASTF || (_dir)[Y] > SQRT_SMALL_FASTF )  { \
		(_inv)[Y]=1.0/(_dir)[Y]; \
	} else { \
		(_dir)[Y] = 0.0; \
		(_inv)[Y] = INFINITY; \
	} \
	if( (_dir)[Z] < -SQRT_SMALL_FASTF || (_dir)[Z] > SQRT_SMALL_FASTF )  { \
		(_inv)[Z]=1.0/(_dir)[Z]; \
	} else { \
		(_dir)[Z] = 0.0; \
		(_inv)[Z] = INFINITY; \
	} \
    }

/* Apply the 3x3 part of a mat_t to a 3-tuple. */
/* This rotates a vector without scaling it (changing its length) */
#ifdef SHORT_VECTORS
#define MAT3X3VEC(o,mat,vec) \
	{ register int _m3x3v; \
	for(_m3x3v = 0; _m3x3v < 3; _m3x3v++) \
		(o)[_m3x3v] = (mat)[4*_m3x3v+0]*(vec)[X] + \
			  (mat)[4*_m3x3v+1]*(vec)[Y] + \
			  (mat)[4*_m3x3v+2]*(vec)[Z]; \
	}
#else
#define MAT3X3VEC(o,mat,vec) 	{ \
	(o)[X] = (mat)[X]*(vec)[X]+(mat)[Y]*(vec)[Y] + (mat)[ 2]*(vec)[Z]; \
	(o)[Y] = (mat)[4]*(vec)[X]+(mat)[5]*(vec)[Y] + (mat)[ 6]*(vec)[Z]; \
	(o)[Z] = (mat)[8]*(vec)[X]+(mat)[9]*(vec)[Y] + (mat)[10]*(vec)[Z]; }
#endif /* SHORT_VECTORS */

/* Multiply a 3-tuple by the 3x3 part of a mat_t. */
#ifdef SHORT_VECTORS
#define VEC3X3MAT(o,i,m) \
	{ register int _v3x3m; \
	for(_v3x3m = 0; _v3x3m < 3; _v3x3m++) \
		(o)[_v3x3m] = (i)[X]*(m)[_v3x3m] + \
			(i)[Y]*(m)[_v3x3m+4] + \
			(i)[Z]*(m)[_v3x3m+8]; \
	}
#else
#define VEC3X3MAT(o,i,m) 	{ \
	(o)[X] = (i)[X]*(m)[X] + (i)[Y]*(m)[4] + (i)[Z]*(m)[8]; \
	(o)[Y] = (i)[X]*(m)[1] + (i)[Y]*(m)[5] + (i)[Z]*(m)[9]; \
	(o)[Z] = (i)[X]*(m)[2] + (i)[Y]*(m)[6] + (i)[Z]*(m)[10]; }
#endif /* SHORT_VECTORS */

/* Apply the 3x3 part of a mat_t to a 2-tuple (Z part=0). */
#ifdef SHORT_VECTORS
#define MAT3X2VEC(o,mat,vec) \
	{ register int _m3x2v; \
	for(_m3x2v = 0; _m3x2v < 3; _m3x2v++) \
		(o)[_m3x2v] = (mat)[4*_m3x2v]*(vec)[X] + \
			(mat)[4*_m3x2v+1]*(vec)[Y]; \
	}
#else
#define MAT3X2VEC(o,mat,vec) 	{ \
	(o)[X] = (mat)[0]*(vec)[X] + (mat)[Y]*(vec)[Y]; \
	(o)[Y] = (mat)[4]*(vec)[X] + (mat)[5]*(vec)[Y]; \
	(o)[Z] = (mat)[8]*(vec)[X] + (mat)[9]*(vec)[Y]; }
#endif /* SHORT_VECTORS */

/* Multiply a 2-tuple (Z=0) by the 3x3 part of a mat_t. */
#ifdef SHORT_VECTORS
#define VEC2X3MAT(o,i,m) \
	{ register int _v2x3m; \
	for(_v2x3m = 0; _v2x3m < 3; _v2x3m++) \
		(o)[_v2x3m] = (i)[X]*(m)[_v2x3m] + (i)[Y]*(m)[2*_v2x3m]; \
	}
#else
#define VEC2X3MAT(o,i,m) 	{ \
	(o)[X] = (i)[X]*(m)[0] + (i)[Y]*(m)[4]; \
	(o)[Y] = (i)[X]*(m)[1] + (i)[Y]*(m)[5]; \
	(o)[Z] = (i)[X]*(m)[2] + (i)[Y]*(m)[6]; }
#endif /* SHORT_VECTORS */

/* Apply a 4x4 matrix to a 3-tuple which is an absolute Point in space */
#ifdef SHORT_VECTORS
#define MAT4X3PNT(o,m,i) \
	{ register double _f; \
	register int _i_m4x3p, _j_m4x3p; \
	_f = 0.0; \
	for(_j_m4x3p = 0; _j_m4x3p < 3; _j_m4x3p++)  \
		_f += (m)[_j_m4x3p+12] * (i)[_j_m4x3p]; \
	_f = 1.0/(_f + (m)[15]); \
	for(_i_m4x3p = 0; _i_m4x3p < 3; _i_m4x3p++) \
		(o)[_i_m4x3p] = 0.0; \
	for(_i_m4x3p = 0; _i_m4x3p < 3; _i_m4x3p++)  { \
		for(_j_m4x3p = 0; _j_m4x3p < 3; _j_m4x3p++) \
			(o)[_i_m4x3p] += (m)[_j_m4x3p+4*_i_m4x3p] * (i)[_j_m4x3p]; \
	} \
	for(_i_m4x3p = 0; _i_m4x3p < 3; _i_m4x3p++)  { \
		(o)[_i_m4x3p] = ((o)[_i_m4x3p] + (m)[4*_i_m4x3p+3]) * _f; \
	} }
#else
#define MAT4X3PNT(o,m,i) \
	{ register double _f; \
	_f = 1.0/((m)[12]*(i)[X] + (m)[13]*(i)[Y] + (m)[14]*(i)[Z] + (m)[15]);\
	(o)[X]=((m)[0]*(i)[X] + (m)[1]*(i)[Y] + (m)[ 2]*(i)[Z] + (m)[3]) * _f;\
	(o)[Y]=((m)[4]*(i)[X] + (m)[5]*(i)[Y] + (m)[ 6]*(i)[Z] + (m)[7]) * _f;\
	(o)[Z]=((m)[8]*(i)[X] + (m)[9]*(i)[Y] + (m)[10]*(i)[Z] + (m)[11])* _f;}
#endif /* SHORT_VECTORS */

/* Multiply an Absolute 3-Point by a full 4x4 matrix. */
#define PNT3X4MAT(o,i,m) \
	{ register double _f; \
	_f = 1.0/((i)[X]*(m)[3] + (i)[Y]*(m)[7] + (i)[Z]*(m)[11] + (m)[15]);\
	(o)[X]=((i)[X]*(m)[0] + (i)[Y]*(m)[4] + (i)[Z]*(m)[8] + (m)[12]) * _f;\
	(o)[Y]=((i)[X]*(m)[1] + (i)[Y]*(m)[5] + (i)[Z]*(m)[9] + (m)[13]) * _f;\
	(o)[Z]=((i)[X]*(m)[2] + (i)[Y]*(m)[6] + (i)[Z]*(m)[10] + (m)[14])* _f;}

/* Multiply an Absolute hvect_t 4-Point by a full 4x4 matrix. */
#ifdef SHORT_VECTORS
#define MAT4X4PNT(o,m,i) \
	{ register int _i_m4x4p, _j_m4x4p; \
	for(_i_m4x4p = 0; _i_m4x4p < 4; _i_m4x4p++) \
		(o)[_i_m4x4p] = 0.0; \
	for(_i_m4x4p = 0; _i_m4x4p < 4; _i_m4x4p++) \
		for(_j_m4x4p = 0; _j_m4x4p < 4; _j_m4x4p++) \
			(o)[_i_m4x4p] += (m)[_j_m4x4p+4*_i_m4x4p] * (i)[_j_m4x4p]; \
	}
#else
#define MAT4X4PNT(o,m,i) 	{ \
	(o)[X]=(m)[ 0]*(i)[X] + (m)[ 1]*(i)[Y] + (m)[ 2]*(i)[Z] + (m)[ 3]*(i)[H];\
	(o)[Y]=(m)[ 4]*(i)[X] + (m)[ 5]*(i)[Y] + (m)[ 6]*(i)[Z] + (m)[ 7]*(i)[H];\
	(o)[Z]=(m)[ 8]*(i)[X] + (m)[ 9]*(i)[Y] + (m)[10]*(i)[Z] + (m)[11]*(i)[H];\
	(o)[H]=(m)[12]*(i)[X] + (m)[13]*(i)[Y] + (m)[14]*(i)[Z] + (m)[15]*(i)[H]; }
#endif /* SHORT_VECTORS */

/* Apply a 4x4 matrix to a 3-tuple which is a relative Vector in space */
/* This macro can scale the length of the vector if [15] != 1.0 */
#ifdef SHORT_VECTORS
#define MAT4X3VEC(o,m,i) \
	{ register double _f; \
	register int _i_m4x3v, _j_m4x3v; \
	_f = 1.0/((m)[15]); \
	for(_i_m4x3v = 0; _i_m4x3v < 3; _i_m4x3v++) \
		(o)[_i_m4x3v] = 0.0; \
	for(_i_m4x3v = 0; _i_m4x3v < 3; _i_m4x3v++) { \
		for(_j_m4x3v = 0; _j_m4x3v < 3; _j_m4x3v++) \
			(o)[_i_m4x3v] += (m)[_j_m4x3v+4*_i_m4x3v] * (i)[_j_m4x3v]; \
	} \
	for(_i_m4x3v = 0; _i_m4x3v < 3; _i_m4x3v++) { \
		(o)[_i_m4x3v] *= _f; \
	} }
#else
#define MAT4X3VEC(o,m,i) \
	{ register double _f;	_f = 1.0/((m)[15]);\
	(o)[X] = ((m)[0]*(i)[X] + (m)[1]*(i)[Y] + (m)[ 2]*(i)[Z]) * _f; \
	(o)[Y] = ((m)[4]*(i)[X] + (m)[5]*(i)[Y] + (m)[ 6]*(i)[Z]) * _f; \
	(o)[Z] = ((m)[8]*(i)[X] + (m)[9]*(i)[Y] + (m)[10]*(i)[Z]) * _f; }
#endif /* SHORT_VECTORS */

#define MAT4XSCALOR(o,m,i) \
	{(o) = (i) / (m)[15];}

/* Multiply a Relative 3-Vector by most of a 4x4 matrix */
#define VEC3X4MAT(o,i,m) \
	{ register double _f; 	_f = 1.0/((m)[15]); \
	(o)[X] = ((i)[X]*(m)[0] + (i)[Y]*(m)[4] + (i)[Z]*(m)[8]) * _f; \
	(o)[Y] = ((i)[X]*(m)[1] + (i)[Y]*(m)[5] + (i)[Z]*(m)[9]) * _f; \
	(o)[Z] = ((i)[X]*(m)[2] + (i)[Y]*(m)[6] + (i)[Z]*(m)[10]) * _f; }

/* Multiply a Relative 2-Vector by most of a 4x4 matrix */
#define VEC2X4MAT(o,i,m) \
	{ register double _f; 	_f = 1.0/((m)[15]); \
	(o)[X] = ((i)[X]*(m)[0] + (i)[Y]*(m)[4]) * _f; \
	(o)[Y] = ((i)[X]*(m)[1] + (i)[Y]*(m)[5]) * _f; \
	(o)[Z] = ((i)[X]*(m)[2] + (i)[Y]*(m)[6]) * _f; }

/* Test a vector for non-unit length */
#define BN_VEC_NON_UNIT_LEN(_vec)	\
	(fabs(MAGSQ(_vec)) < 0.0001 || fabs(fabs(MAGSQ(_vec))-1) > 0.0001)

/* Compare two vectors for EXACT equality.  Use carefully. */
#define VEQUAL(a,b)	((a)[X]==(b)[X] && (a)[Y]==(b)[Y] && (a)[Z]==(b)[Z])

/*
 *  Compare two vectors for approximate equality,
 *  within the specified absolute tolerance.
 */
#define VAPPROXEQUAL(a,b,tol)	( \
	NEAR_ZERO( (a)[X]-(b)[X], tol ) && \
	NEAR_ZERO( (a)[Y]-(b)[Y], tol ) && \
	NEAR_ZERO( (a)[Z]-(b)[Z], tol ) )

/* Test for all elements of `v' being smaller than `tol' */
#define VNEAR_ZERO(v, tol)	( \
	NEAR_ZERO(v[X],tol) && NEAR_ZERO(v[Y],tol) && NEAR_ZERO(v[Z],tol)  )

/* Macros to update min and max X,Y,Z values to contain a point */
#define V_MIN(r,s)	if( (s) < (r) ) r = (s)
#define V_MAX(r,s)	if( (s) > (r) ) r = (s)
#define VMIN(r,s)	{ V_MIN((r)[X],(s)[X]); V_MIN((r)[Y],(s)[Y]); V_MIN((r)[Z],(s)[Z]); }
#define VMAX(r,s)	{ V_MAX((r)[X],(s)[X]); V_MAX((r)[Y],(s)[Y]); V_MAX((r)[Z],(s)[Z]); }
#define VMINMAX( min, max, pt )	{ VMIN( (min), (pt) ); VMAX( (max), (pt) ); }

/* Divide out homogeneous parameter from hvect_t, creating vect_t */
#ifdef SHORT_VECTORS
#define HDIVIDE(a,b)  \
	{ register int _hdivide; \
	for(_hdivide = 0; _hdivide < 3; _hdivide++) \
		(a)[_hdivide] = (b)[_hdivide] / (b)[H]; \
	}
#else
#define HDIVIDE(a,b)  { \
	(a)[X] = (b)[X] / (b)[H];\
	(a)[Y] = (b)[Y] / (b)[H];\
	(a)[Z] = (b)[Z] / (b)[H]; }
#endif /* SHORT_VECTORS */

/*
 *  Some 2-D versions of the 3-D macros given above.
 *
 *  A better naming convention is V2MOVE() rather than VMOVE_2D().
 *  XXX These xxx_2D names are slated to go away, use the others.
 */
#define VADD2_2D(a,b,c)	V2ADD2(a,b,c)
#define VSUB2_2D(a,b,c)	V2SUB2(a,b,c)
#define MAGSQ_2D(a)	MAG2SQ(a)
#define VDOT_2D(a,b)	V2DOT(a,b)
#define VMOVE_2D(a,b)	V2MOVE(a,b)
#define VSCALE_2D(a,b,c)	V2SCALE(a,b,c)
#define VJOIN1_2D(a,b,c,d) 	V2JOIN1(a,b,c,d)

/*
 *  Quaternion math definitions.
 *
 *  Note that the W component will be put in the last [3] place
 *  rather than the first [0] place,
 *  so that the X, Y, Z elements will be compatible with vectors.
 *  Only QUAT_FROM_VROT macros depend on component locations, however.
 *
 *  Phillip Dykstra, 26 Sep 1985.
 *  Lee A. Butler, 14 March 1996.
 */

/* Create Quaternion from Vector and Rotation about vector.
 *
 * To produce a quaternion representing a rotation by PI radians about X-axis:
 *
 *	VSET(axis, 1, 0, 0);
 *	QUAT_FROM_VROT( quat, M_PI, axis);
 *		or
 *	QUAT_FROM_ROT( quat, M_PI, 1.0, 0.0, 0.0, 0.0 );
 *
 *  Alternatively, in degrees:
 *	QUAT_FROM_ROT_DEG( quat, 180.0, 1.0, 0.0, 0.0, 0.0 );
 *
 */
#define QUAT_FROM_ROT(q, r, x, y, z){ \
	register fastf_t _rot = (r) * 0.5; \
	QSET(q, x, y, z, cos(_rot)); \
	VUNITIZE(q); \
	_rot = sin(_rot); /* _rot is really just a temp variable now */ \
	VSCALE(q, q, _rot ); }

#define QUAT_FROM_VROT(q, r, v) { \
	register fastf_t _rot = (r) * 0.5; \
	VMOVE(q, v); \
	VUNITIZE(q); \
	(q)[W] = cos(_rot); \
	_rot = sin(_rot); /* _rot is really just a temp variable now */ \
	VSCALE(q, q, _rot ); }

#define QUAT_FROM_VROT_DEG(q, r, v) \
	QUAT_FROM_VROT(q, ((r)*(M_PI/180.0)), v)

#define QUAT_FROM_ROT_DEG(q, r, x, y, z) \
	QUAT_FROM_ROT(q, ((r)*(M_PI/180.0)), x, y, z)



/* Set quaternion at `a' to have coordinates `b', `c', `d', `e' */
#define QSET(a,b,c,d,e)	{ \
			(a)[X] = (b);\
			(a)[Y] = (c);\
			(a)[Z] = (d);\
			(a)[W] = (e); }

/* Transfer quaternion at `b' to quaternion at `a' */
#define QMOVE(a,b)	{ \
			(a)[X] = (b)[X];\
			(a)[Y] = (b)[Y];\
			(a)[Z] = (b)[Z];\
			(a)[W] = (b)[W]; }

/* Add quaternions at `b' and `c', store result at `a' */
#define QADD2(a,b,c)	{ \
			(a)[X] = (b)[X] + (c)[X];\
			(a)[Y] = (b)[Y] + (c)[Y];\
			(a)[Z] = (b)[Z] + (c)[Z];\
			(a)[W] = (b)[W] + (c)[W]; }

/* Subtract quaternion at `c' from quaternion at `b', store result at `a' */
#define QSUB2(a,b,c)	{ \
			(a)[X] = (b)[X] - (c)[X];\
			(a)[Y] = (b)[Y] - (c)[Y];\
			(a)[Z] = (b)[Z] - (c)[Z];\
			(a)[W] = (b)[W] - (c)[W]; }

/* Scale quaternion at `b' by scalar `c', store result at `a' */
#define QSCALE(a,b,c)	{ \
			(a)[X] = (b)[X] * (c);\
			(a)[Y] = (b)[Y] * (c);\
			(a)[Z] = (b)[Z] * (c);\
			(a)[W] = (b)[W] * (c); }

/* Normalize quaternion 'a' to be a unit quaternion */
#define QUNITIZE(a)	{register double _f; _f = QMAGNITUDE(a); \
			if( _f < VDIVIDE_TOL ) _f = 0.0; else _f = 1.0/_f; \
			(a)[X] *= _f; (a)[Y] *= _f; (a)[Z] *= _f; (a)[W] *= _f; }

/* Return scalar magnitude squared of quaternion at `a' */
#define QMAGSQ(a)	( (a)[X]*(a)[X] + (a)[Y]*(a)[Y] \
			+ (a)[Z]*(a)[Z] + (a)[W]*(a)[W] )

/* Return scalar magnitude of quaternion at `a' */
#define QMAGNITUDE(a)	sqrt( QMAGSQ( a ) )

/* Compute dot product of quaternions at `a' and `b' */
#define QDOT(a,b)	( (a)[X]*(b)[X] + (a)[Y]*(b)[Y] \
			+ (a)[Z]*(b)[Z] + (a)[W]*(b)[W] )

/*
 *  Compute quaternion product a = b * c
 *	a[W] = b[W]*c[W] - VDOT(b,c);
	VCROSS( temp, b, c );
 *	VJOIN2( a, temp, b[W], c, c[W], b );
 */
#define QMUL(a,b,c)	{ \
    (a)[W] = (b)[W]*(c)[W] - (b)[X]*(c)[X] - (b)[Y]*(c)[Y] - (b)[Z]*(c)[Z]; \
    (a)[X] = (b)[W]*(c)[X] + (b)[X]*(c)[W] + (b)[Y]*(c)[Z] - (b)[Z]*(c)[Y]; \
    (a)[Y] = (b)[W]*(c)[Y] + (b)[Y]*(c)[W] + (b)[Z]*(c)[X] - (b)[X]*(c)[Z]; \
    (a)[Z] = (b)[W]*(c)[Z] + (b)[Z]*(c)[W] + (b)[X]*(c)[Y] - (b)[Y]*(c)[X]; }

/* Conjugate quaternion */
#define QCONJUGATE(a,b)	{ \
	(a)[X] = -(b)[X]; \
	(a)[Y] = -(b)[Y]; \
	(a)[Z] = -(b)[Z]; \
	(a)[W] =  (b)[W]; }

/* Multiplicative inverse quaternion */
#define QINVERSE(a,b)	{ register double _f = QMAGSQ(b); \
	if( _f < VDIVIDE_TOL ) _f = 0.0; else _f = 1.0/_f; \
	(a)[X] = -(b)[X] * _f; \
	(a)[Y] = -(b)[Y] * _f; \
	(a)[Z] = -(b)[Z] * _f; \
	(a)[W] =  (b)[W] * _f; }

/*
 *  Blend into quaternion `a'
 *	scalar `b' times quaternion at `c' plus
 *	scalar `d' times quaternion at `e'
 */
#ifdef SHORT_VECTORS
#define QBLEND2(a,b,c,d,e)	VBLEND2N(a,b,c,d,e,4)
#else
#define QBLEND2(a,b,c,d,e)	{ \
	(a)[X] = (b) * (c)[X] + (d) * (e)[X];\
	(a)[Y] = (b) * (c)[Y] + (d) * (e)[Y];\
	(a)[Z] = (b) * (c)[Z] + (d) * (e)[Z];\
	(a)[W] = (b) * (c)[W] + (d) * (e)[W]; }
#endif /* SHORT_VECTORS */

/*
 *  Macros for dealing with 3-D "extents" represented as axis-aligned
 *  right parallelpipeds (RPPs).
 *  This is stored as two points:  a min point, and a max point.
 *  RPP 1 is defined by lo1, hi1, RPP 2 by lo2, hi2.
 */

/* Compare two extents represented as RPPs. If they are disjoint, return true */
#define V3RPP_DISJOINT(_l1, _h1, _l2, _h2) \
      ( (_l1)[X] > (_h2)[X] || (_l1)[Y] > (_h2)[Y] || (_l1)[Z] > (_h2)[Z] || \
	(_l2)[X] > (_h1)[X] || (_l2)[Y] > (_h1)[Y] || (_l2)[Z] > (_h1)[Z] )

/* Compare two extents represented as RPPs. If they overlap, return true */
#define V3RPP_OVERLAP(_l1, _h1, _l2, _h2) \
    (! ((_l1)[X] > (_h2)[X] || (_l1)[Y] > (_h2)[Y] || (_l1)[Z] > (_h2)[Z] || \
	(_l2)[X] > (_h1)[X] || (_l2)[Y] > (_h1)[Y] || (_l2)[Z] > (_h1)[Z]) )

/* If two extents overlap within distance tolerance, return true. */
#define V3RPP_OVERLAP_TOL(_l1, _h1, _l2, _h2, _t) \
    (! ((_l1)[X] > (_h2)[X] + (_t)->dist || \
	(_l1)[Y] > (_h2)[Y] + (_t)->dist || \
	(_l1)[Z] > (_h2)[Z] + (_t)->dist || \
	(_l2)[X] > (_h1)[X] + (_t)->dist || \
	(_l2)[Y] > (_h1)[Y] + (_t)->dist || \
	(_l2)[Z] > (_h1)[Z] + (_t)->dist ) )

/* Is the point within or on the boundary of the RPP? */
#define V3PT_IN_RPP(_pt, _lo, _hi)	( \
	(_pt)[X] >= (_lo)[X] && (_pt)[X] <= (_hi)[X] && \
	(_pt)[Y] >= (_lo)[Y] && (_pt)[Y] <= (_hi)[Y] && \
	(_pt)[Z] >= (_lo)[Z] && (_pt)[Z] <= (_hi)[Z]  )

/* Within the distance tolerance, is the point within the RPP? */
#define V3PT_IN_RPP_TOL(_pt, _lo, _hi, _t)	( \
	(_pt)[X] >= (_lo)[X]-(_t)->dist && (_pt)[X] <= (_hi)[X]+(_t)->dist && \
	(_pt)[Y] >= (_lo)[Y]-(_t)->dist && (_pt)[Y] <= (_hi)[Y]+(_t)->dist && \
	(_pt)[Z] >= (_lo)[Z]-(_t)->dist && (_pt)[Z] <= (_hi)[Z]+(_t)->dist  )

/*
 * Determine if one bounding box is within another.
 * Also returns true if the boxes are the same.
 */
#define V3RPP1_IN_RPP2( _lo1 , _hi1 , _lo2 , _hi2 )	( \
	(_lo1)[X] >= (_lo2)[X] && (_hi1)[X] <= (_hi2)[X] && \
	(_lo1)[Y] >= (_lo2)[Y] && (_hi1)[Y] <= (_hi2)[Y] && \
	(_lo1)[Z] >= (_lo2)[Z] && (_hi1)[Z] <= (_hi2)[Z] )

__END_DECLS

#endif /* VMATH_H */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
