/*                         V M A T H . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2011 United States Government as represented by
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
/** @file vmath.h
 *
 * @brief vector/matrix math
 *
 * This header file defines many commonly used 3D vector math macros,
 * and operates on vect_t, point_t, mat_t, and quat_t objects.
 *
 * Note that while many people in the computer graphics field use
 * post-multiplication with row vectors (ie, vector * matrix * matrix
 * ...) the BRL-CAD system uses the more traditional representation
 * of column vectors (ie, ... matrix * matrix * vector).  (The
 * matrices in these two representations are the transposes of each
 * other). Therefore, when transforming a vector by a matrix,
 * pre-multiplication is used, ie:
 *
 * view_vec = model2view_mat * model_vec
 *
 * Furthermore, additional transformations are multiplied on the left,
 * ie:
 *
 <tt> @code
 * vec'  =  T1 * vec
 * vec'' =  T2 * T1 * vec  =  T2 * vec'
 @endcode </tt>
 *
 * The most notable implication of this is the location of the "delta"
 * (translation) values in the matrix, ie:
 *
 <tt> @code
 * x'   (R0  R1  R2 Dx) x
 * y' = (R4  R5  R6 Dy) * y
 * z'   (R8  R9 R10 Dz) z
 * w'   (0   0   0  1/s) w
 @endcode </tt>
 *
 * Note -
 *@n  vect_t objects are 3-tuples
 *@n hvect_t objects are 4-tuples
 *
 * Most of these macros require that the result be in separate
 * storage, distinct from the input parameters, except where noted.
 *
 * When writing macros like this, it is very important that any
 * variables which are declared within code blocks inside a macro
 * start with an underscore.  This (hopefully) minimizes any name
 * conflicts with user-provided parameters.  For example:
 *
 * { register double _f; stuff; }
 *
 */

#ifndef __VMATH_H__
#define __VMATH_H__

#include "common.h"

/* for sqrt(), sin(), cos(), rint(), etc */
#include <math.h>

/* for floating point tolerances and other math constants */
#include <float.h>

/* for fastf_t */
#include "bu.h"


__BEGIN_DECLS

#ifndef M_
#  define M_		XXX /**< */
#endif

#ifndef M_1_PI
#  define M_1_PI	0.31830988618379067153776752675 /**< 1/pi */
#endif
#ifndef M_2_PI
#  define M_2_PI	0.63661977236758134307553505349 /**< 2/pi */
#endif
#ifndef M_2_SQRTPI
#  define M_2_SQRTPI	1.12837916709551257389615890312 /**< 2/sqrt(pi) */
#endif
#ifndef M_E
#  define M_E		2.71828182845904523536028747135 /**< e */
#endif
#ifndef M_EULER
#  define M_EULER	0.57721566490153286060651209008 /**< Euler's constant */
#endif
#ifndef M_LOG2E
#  define M_LOG2E	1.44269504088896340735992468100 /**< log_2(e) */
#endif
#ifndef M_LOG10E
#  define M_LOG10E	0.43429448190325182765112891892 /**< log_10(e) */
#endif
#ifndef M_LN10
#  define M_LN10	2.30258509299404568401799145468 /**< log_e(10) */
#endif
#ifndef M_LN2
#  define M_LN2		0.69314718055994530941723212146 /**< log_e(2) */
#endif
#ifndef M_LNPI
#  define M_LNPI	1.14472988584940017414342735135 /**< log_e(pi) */
#endif
#ifndef M_PI
#  define M_PI		3.14159265358979323846264338328 /**< pi */
#endif
#ifndef M_PI_2
#  define M_PI_2	1.57079632679489661923132169164 /**< pi/2 */
#endif
#ifndef M_PI_4
#  define M_PI_4	0.78539816339744830966156608458 /**< pi/4 */
#endif
#ifndef M_SQRT1_2
#  define M_SQRT1_2	0.70710678118654752440084436210 /**< sqrt(1/2) */
#endif
#ifndef M_SQRT2
#  define M_SQRT2	1.41421356237309504880168872421 /**< sqrt(2) */
#endif
#ifndef M_SQRT3
#  define M_SQRT3	1.73205080756887729352744634151 /**< sqrt(3) */
#endif
#ifndef M_SQRTPI
#  define M_SQRTPI	1.77245385090551602729816748334 /**< sqrt(pi) */
#endif

#ifndef DEG2RAD
#  define DEG2RAD	0.017453292519943295769236907684 /**< pi/180 */
#endif
#ifndef RAD2DEG
#  define RAD2DEG	57.295779513082320876798154814105 /**< 180/pi */
#endif


/* minimum computation tolerances */
#ifdef vax
#  define VDIVIDE_TOL		(1.0e-10)
#  define VUNITIZE_TOL		(1.0e-7)
#else
#  ifdef DBL_EPSILON
#    define VDIVIDE_TOL		(DBL_EPSILON)
#  else
#    define VDIVIDE_TOL		(1.0e-20)
#  endif
#  ifdef FLT_EPSILON
#    define VUNITIZE_TOL	(FLT_EPSILON)
#  else
#    define VUNITIZE_TOL	(1.0e-15)
#  endif
#endif

/** @brief # of fastf_t's per vect2d_t */
#define ELEMENTS_PER_VECT2D	2

/** @brief # of fastf_t's per point2d_t */
#define ELEMENTS_PER_POINT2D	2

/** @brief # of fastf_t's per vect_t */
#define ELEMENTS_PER_VECT	3

/** @brief # of fastf_t's per point_t */
#define ELEMENTS_PER_POINT	3

/** @brief # of fastf_t's per hvect_t (homogeneous vector) */
#define ELEMENTS_PER_HVECT	4

/** @brief # of fastf_t's per hpt_t (homogeneous point) */
#define ELEMENTS_PER_HPOINT	4

/** @brief # of fastf_t's per plane_t */
#define ELEMENTS_PER_PLANE	4

/** @brief # of fastf_t's per mat_t */
#define ELEMENTS_PER_MAT	(ELEMENTS_PER_PLANE*ELEMENTS_PER_PLANE)

/*
 * Types for matrixes and vectors.
 */

/** @brief 2-tuple vector */
typedef fastf_t vect2d_t[ELEMENTS_PER_VECT2D];

/** @brief pointer to a 2-tuple vector */
typedef fastf_t *vect2dp_t;

/** @brief 2-tuple point */
typedef fastf_t point2d_t[ELEMENTS_PER_POINT2D];

/** @brief pointer to a 2-tuple point */
typedef fastf_t *point2dp_t;

/** @brief 3-tuple vector */
typedef fastf_t vect_t[ELEMENTS_PER_VECT];

/** @brief pointer to a 3-tuple vector */
typedef fastf_t *vectp_t;

/** @brief 3-tuple point */
typedef fastf_t point_t[ELEMENTS_PER_POINT];

/** @brief pointer to a 3-tuple point */
typedef fastf_t *pointp_t;

/** @brief 4-tuple vector */
typedef fastf_t hvect_t[ELEMENTS_PER_HVECT];

/** @brief 4-element quaternion */
typedef hvect_t quat_t;

/** @brief 4-tuple point */
typedef fastf_t hpoint_t[ELEMENTS_PER_HPOINT];

/** @brief 4x4 matrix */
typedef fastf_t mat_t[ELEMENTS_PER_MAT];

/** @brief pointer to a 4x4 matrix */
typedef fastf_t *matp_t;

/** Vector component names for homogeneous (4-tuple) vectors */
typedef enum bn_vector_component_ {
    X = 0,
    Y = 1,
    Z = 2,
    H = 3,
    W = H
} bn_vector_component;

/**
 * Locations of deltas (MD*) and scaling values (MS*) in a 4x4
 * Homogenous Transform matrix
 */
typedef enum bn_matrix_component_ {
    MSX = 0,
    MDX = 3,
    MSY = 5,
    MDY = 7,
    MSZ = 10,
    MDZ = 11,
    MSA = 15
} bn_matrix_component;

/**
 * @brief Definition of a plane equation
 *
 * A plane is defined by a unit-length outward pointing normal vector
 * (N), and the perpendicular (shortest) distance from the origin to
 * the plane (in element N[W]).
 *
 * The plane consists of all points P=(x, y, z) such that
 *@n VDOT(P, N) - N[W] == 0
 *@n that is,
 *@n N[X]*x + N[Y]*y + N[Z]*z - N[W] == 0
 *
 * The inside of the halfspace bounded by the plane
 * consists of all points P such that
 *@n VDOT(P, N) - N[W] <= 0
 *
 * A ray with direction D is classified w.r.t. the plane by
 *
 *@n VDOT(D, N) < 0 ray enters halfspace defined by plane
 *@n VDOT(D, N) == 0 ray is parallel to plane
 *@n VDOT(D, N) > 0 ray exits halfspace defined by plane
 */
typedef fastf_t plane_t[ELEMENTS_PER_PLANE];


/**
 * Return truthfully whether a value is within a specified epsilon
 * distance from zero.
 */
#define NEAR_ZERO(val, epsilon)	(((val) > -epsilon) && ((val) < epsilon))

/**
 * Return truthfully whether all elements of a given vector are within
 * a specified epsilon distance from zero.
 */
#define VNEAR_ZERO(v, tol) \
	(NEAR_ZERO(v[X], tol) \
	 && NEAR_ZERO(v[Y], tol) \
	 && NEAR_ZERO(v[Z], tol))

/** 
 * @brief Test for all elements of `v' being smaller than `tol'. 
 * Version for degree 2 vectors.
 */
#define V2NEAR_ZERO(v, tol) (NEAR_ZERO(v[X], tol) && NEAR_ZERO(v[Y], tol))

/* FIXME: need HNEAR_ZERO */

/**
 * Return truthfully whether a value is within a minimum
 * representation tolerance from zero.
 *
 * Use not recommended due to compilation-variant tolerance.
 */
#define ZERO(_a) NEAR_ZERO((_a), SMALL_FASTF)


/**
 * Return truthfully whether two values are within a specified epsilon
 * distance from each other.
 */
#define NEAR_EQUAL(_a, _b, _tol) NEAR_ZERO((_a) - (_b), (_tol))

/**
 * Return truthfully whether two 3D vectors are approximately equal,
 * within a specified absolute tolerance.
 */
#define VNEAR_EQUAL(_a, _b, _tol) \
    (NEAR_EQUAL((_a)[X], (_b)[X], (_tol)) \
     && NEAR_EQUAL((_a)[Y], (_b)[Y], (_tol)) \
     && NEAR_EQUAL((_a)[Z], (_b)[Z], (_tol)))

/**
 * Return truthfully whether two 2D vectors are approximately equal,
 * within a specified absolute tolerance.
 */
#define V2NEAR_EQUAL(a, b, tol) \
    (NEAR_EQUAL((a)[X], (b)[X], tol) \
     && NEAR_EQUAL((a)[Y], (b)[Y], tol))

/**
 * Return truthfully whether two 4D vectors are approximately equal,
 * within a specified absolute tolerance.
 */
#define HNEAR_EQUAL(_a, _b, _tol) \
    (NEAR_EQUAL((_a)[X], (_b)[X], (_tol)) \
     && NEAR_EQUAL((_a)[Y], (_b)[Y], (_tol)) \
     && NEAR_EQUAL((_a)[Z], (_b)[Z], (_tol)) \
     && NEAR_EQUAL((_a)[W], (_b)[W], (_tol)))

/**
 * Return truthfully whether two values are within a minimum
 * representation tolerance from each other.
 *
 * Use not recommended due to compilation-variant tolerance.
 */
#define EQUAL(_a, _b) NEAR_EQUAL((_a), (_b), SMALL_FASTF)


/**
 * Return truthfully whether two vectors are equal within a minimum
 * representation tolerance.
 *
 * Use not recommended due to compilation-variant tolerance.
 */
#define VEQUAL(_a, _b) VNEAR_EQUAL((_a), (_b), SMALL_FASTF)

/**
 * @brief Compare two vectors for EXACT equality.  Use carefully. 
 * Version for degree 2 vectors.  FIXME: no such thing as exact.
 */
#define V2EQUAL(a, b)	((a)[X]==(b)[X] && (a)[Y]==(b)[Y])

/**
 * @brief Compare two vectors for EXACT equality.  Use carefully. 
 * Version for degree 4 vectors.   FIXME: no such thing as exact.
 */
#define HEQUAL(a, b)	((a)[X]==(b)[X] && (a)[Y]==(b)[Y] && (a)[Z]==(b)[Z] && (a)[W]==(b)[W])


/**
 * clamp a value to a low/high number.
 */
#define CLAMP(_v, _l, _h) if ((_v) < (_l)) _v = _l; else if ((_v) > (_h)) _v = _h


/** @brief Compute distance from a point to a plane. */
#define DIST_PT_PLANE(_pt, _pl) (VDOT(_pt, _pl) - (_pl)[W])

/** @brief Compute distance between two points. */
#define DIST_PT_PT_SQ(_a, _b) \
	((_a)[X]-(_b)[X])*((_a)[X]-(_b)[X]) + \
	((_a)[Y]-(_b)[Y])*((_a)[Y]-(_b)[Y]) + \
	((_a)[Z]-(_b)[Z])*((_a)[Z]-(_b)[Z])
#define DIST_PT_PT(_a, _b) sqrt(DIST_PT_PT_SQ(_a, _b))

/** @brief set translation values of 4x4 matrix with x, y, z values. */
#define MAT_DELTAS(_m, _x, _y, _z) { \
	(_m)[MDX] = (_x); \
	(_m)[MDY] = (_y); \
	(_m)[MDZ] = (_z); \
}

/** @brief set translation values of 4x4 matrix from a vector. */
#define MAT_DELTAS_VEC(_m, _v)	\
	MAT_DELTAS(_m, (_v)[X], (_v)[Y], (_v)[Z])

/**
 * @brief set translation values of 4x4 matrix from a reversed
 * vector.
 */
#define MAT_DELTAS_VEC_NEG(_m, _v)	\
	MAT_DELTAS(_m, -(_v)[X], -(_v)[Y], -(_v)[Z])

/** @brief get translation values of 4x4 matrix to a vector. */
#define MAT_DELTAS_GET(_v, _m) { \
	(_v)[X] = (_m)[MDX]; \
	(_v)[Y] = (_m)[MDY]; \
	(_v)[Z] = (_m)[MDZ]; \
}

/**
 * @brief get translation values of 4x4 matrix to a vector,
 * reversed.
 */
#define MAT_DELTAS_GET_NEG(_v, _m) { \
	(_v)[X] = -(_m)[MDX]; \
	(_v)[Y] = -(_m)[MDY]; \
	(_v)[Z] = -(_m)[MDZ]; \
}

/**
 * @brief increment translation elements in a 4x4 matrix with x, y, z
 * values.
 */
#define MAT_DELTAS_ADD(_m, _x, _y, _z) { \
	(_m)[MDX] += (_x); \
	(_m)[MDY] += (_y); \
	(_m)[MDZ] += (_z); \
}

/**
 * @brief increment translation elements in a 4x4 matrix from a
 * vector.
 */
#define MAT_DELTAS_ADD_VEC(_m, _v) { \
	(_m)[MDX] += (_v)[X]; \
	(_m)[MDY] += (_v)[Y]; \
	(_m)[MDZ] += (_v)[Z]; \
}

/**
 * @brief decrement translation elements in a 4x4 matrix with x, y, z
 * values.
 */
#define MAT_DELTAS_SUB(_m, _x, _y, _z) { \
	(_m)[MDX] -= (_x); \
	(_m)[MDY] -= (_y); \
	(_m)[MDZ] -= (_z); \
}

/**
 * @brief decrement translation elements in a 4x4 matrix from a
 * vector.
 */
#define MAT_DELTAS_SUB_VEC(_m, _v) { \
	(_m)[MDX] -= (_v)[X]; \
	(_m)[MDY] -= (_v)[Y]; \
	(_m)[MDZ] -= (_v)[Z]; \
}

/**
 * @brief decrement translation elements in a 4x4 matrix with x, y, z
 * values.
 */
#define MAT_DELTAS_MUL(_m, _x, _y, _z) { \
	(_m)[MDX] *= (_x); \
	(_m)[MDY] *= (_y); \
	(_m)[MDZ] *= (_z); \
}

/**
 * @brief decrement translation elements in a 4x4 matrix from a
 * vector.
 */
#define MAT_DELTAS_MUL_VEC(_m, _v) { \
	(_m)[MDX] *= (_v)[X]; \
	(_m)[MDY] *= (_v)[Y]; \
	(_m)[MDZ] *= (_v)[Z]; \
}

/** @brief set scale of 4x4 matrix from xyz. */
#define MAT_SCALE(_m, _x, _y, _z) { \
	(_m)[MSX] = _x; \
	(_m)[MSY] = _y; \
	(_m)[MSZ] = _z; \
}

/** @brief set scale of 4x4 matrix from vector. */
#define MAT_SCALE_VEC(_m, _v) { \
	(_m)[MSX] = (_v)[X]; \
	(_m)[MSY] = (_v)[Y]; \
	(_m)[MSZ] = (_v)[Z]; \
}

/** @brief set uniform scale of 4x4 matrix from scalar. */
#define MAT_SCALE_ALL(_m, _s) (_m)[MSA] = (_s)

/** @brief add to scaling elements in a 4x4 matrix from xyz. */
#define MAT_SCALE_ADD(_m, _x, _y, _z) { \
	(_m)[MSX] += _x; \
	(_m)[MSY] += _y; \
	(_m)[MSZ] += _z; \
}

/** @brief add to scaling elements in a 4x4 matrix from vector. */
#define MAT_SCALE_ADD_VEC(_m, _v) { \
	(_m)[MSX] += (_v)[X]; \
	(_m)[MSY] += (_v)[Y]; \
	(_m)[MSZ] += (_v)[Z]; \
}

/** @brief subtract from scaling elements in a 4x4 matrix from xyz. */
#define MAT_SCALE_SUB(_m, _x, _y, _z) { \
	(_m)[MSX] -= _x; \
	(_m)[MSY] -= _y; \
	(_m)[MSZ] -= _z; \
}

/**
 * @brief subtract from scaling elements in a 4x4 matrix from
 * vector.
 */
#define MAT_SCALE_SUB_VEC(_m, _v) { \
	(_m)[MSX] -= (_v)[X]; \
	(_m)[MSY] -= (_v)[Y]; \
	(_m)[MSZ] -= (_v)[Z]; \
}

/** @brief multipy scaling elements in a 4x4 matrix from xyz. */
#define MAT_SCALE_MUL(_m, _x, _y, _z) { \
	(_m)[MSX] *= _x; \
	(_m)[MSY] *= _y; \
	(_m)[MSZ] *= _z; \
}

/** @brief multiply scaling elements in a 4x4 matrix from vector. */
#define MAT_SCALE_MUL_VEC(_m, _v) { \
	(_m)[MSX] *= (_v)[X]; \
	(_m)[MSY] *= (_v)[Y]; \
	(_m)[MSZ] *= (_v)[Z]; \
}


/**
 * In following are macro versions of librt/mat.c functions for when
 * speed really matters.
 */


/** @brief Zero a matrix. */
#define MAT_ZERO(m) { \
	(m)[0] = (m)[1] = (m)[2] = (m)[3] = \
	(m)[4] = (m)[5] = (m)[6] = (m)[7] = \
	(m)[8] = (m)[9] = (m)[10] = (m)[11] = \
	(m)[12] = (m)[13] = (m)[14] = (m)[15] = 0.0; \
}

/** @brief Set matrix to identity. */
#define MAT_IDN(m) { \
	(m)[1] = (m)[2] = (m)[3] = (m)[4] = \
	(m)[6] = (m)[7] = (m)[8] = (m)[9] = \
	(m)[11] = (m)[12] = (m)[13] = (m)[14] = 0.0; \
	(m)[0] = (m)[5] = (m)[10] = (m)[15] = 1.0; \
}

/** @brief set t to the transpose of matrix m */
#define MAT_TRANSPOSE(t, m) { \
	(t)[0] = (m)[0]; \
	(t)[4] = (m)[1]; \
	(t)[8] = (m)[2]; \
	(t)[12] = (m)[3]; \
	(t)[1] = (m)[4]; \
	(t)[5] = (m)[5]; \
	(t)[9] = (m)[6]; \
	(t)[13] = (m)[7]; \
	(t)[2] = (m)[8]; \
	(t)[6] = (m)[9]; \
	(t)[10] = (m)[10]; \
	(t)[14] = (m)[11]; \
	(t)[3] = (m)[12]; \
	(t)[7] = (m)[13]; \
	(t)[11] = (m)[14]; \
	(t)[15] = (m)[15]; \
}

/** @brief Copy a matrix. */
#define MAT_COPY(d, s) { \
	(d)[0] = (s)[0]; \
	(d)[1] = (s)[1]; \
	(d)[2] = (s)[2]; \
	(d)[3] = (s)[3]; \
	(d)[4] = (s)[4]; \
	(d)[5] = (s)[5]; \
	(d)[6] = (s)[6]; \
	(d)[7] = (s)[7]; \
	(d)[8] = (s)[8]; \
	(d)[9] = (s)[9]; \
	(d)[10] = (s)[10]; \
	(d)[11] = (s)[11]; \
	(d)[12] = (s)[12]; \
	(d)[13] = (s)[13]; \
	(d)[14] = (s)[14]; \
	(d)[15] = (s)[15]; \
}

/** @brief Set 3D vector at `a' to have coordinates `b', `c', and `d'. */
#define VSET(a, b, c, d) { \
	(a)[X] = (b); \
	(a)[Y] = (c); \
	(a)[Z] = (d); \
}

/** @brief Set 2D vector at `a' to have coordinates `b' and `c'. */
#define V2SET(a, b, c) { \
	(a)[X] = (b); \
	(a)[Y] = (c); \
}

/** @brief Set 4D vector at `a' to homogenous coordinates `b', `c', `d', and `e'. */
#define HSET(a, b, c, d, e) { \
	(a)[X] = (b); \
	(a)[Y] = (c); \
	(a)[Z] = (d); \
	(a)[H] = (e); \
}


/** @brief Set all elements of 3D vector to same scalar value. */
#define VSETALL(a, s) { \
	(a)[X] = (a)[Y] = (a)[Z] = (s); \
}

/** @brief Set 2D vector elements to same scalar value. */
#define V2SETALL(a, s) { \
	(a)[X] = (a)[Y] = (s); \
}

/** @brief Set 4D vector elements to same scalar value. */
#define HSETALL(a, s) { \
	(a)[X] = (a)[Y] = (a)[Z] = (a)[H] = (s); \
}


/** @brief Set all elements of N-vector to same scalar value. */
#define VSETALLN(v, s, n) { \
	register int _j; \
	for (_j=0; _j<n; _j++) v[_j]=(s); \
}


/** @brief Transfer 3D vector at `b' to vector at `a'. */
#define VMOVE(a, b) { \
	(a)[X] = (b)[X]; \
	(a)[Y] = (b)[Y]; \
	(a)[Z] = (b)[Z]; \
}

/** @brief Move a 2D vector. */
#define V2MOVE(a, b) { \
	(a)[X] = (b)[X]; \
	(a)[Y] = (b)[Y]; \
}

/** @brief Move a homogeneous 4-tuple. */
#define HMOVE(a, b) { \
	(a)[X] = (b)[X]; \
	(a)[Y] = (b)[Y]; \
	(a)[Z] = (b)[Z]; \
	(a)[W] = (b)[W]; \
}

/** @brief Transfer vector of length `n' at `b' to vector at `a'. */
#define VMOVEN(a, b, n) { \
	register int _vmove; \
	for (_vmove = 0; _vmove < (n); _vmove++) { \
		(a)[_vmove] = (b)[_vmove]; \
	} \
}


/** @brief Reverse the direction of 3D vector `b' and store it in `a'. */
#define VREVERSE(a, b) { \
	(a)[X] = -(b)[X]; \
	(a)[Y] = -(b)[Y]; \
	(a)[Z] = -(b)[Z]; \
}

/** @brief Reverse the direction of 2D vector `b' and store it in `a'. */
#define V2REVERSE(a, b) { \
	(a)[X] = -(b)[X]; \
	(a)[Y] = -(b)[Y]; \
}

/**
 * @brief Same as VREVERSE, but for a 4-tuple.  Also useful on plane_t
 * objects.
 */
#define HREVERSE(a, b) { \
	(a)[X] = -(b)[X]; \
	(a)[Y] = -(b)[Y]; \
	(a)[Z] = -(b)[Z]; \
	(a)[W] = -(b)[W]; \
}

/** @brief Add 3D vectors at `b' and `c', store result at `a'. */
#define VADD2(a, b, c) { \
	(a)[X] = (b)[X] + (c)[X]; \
	(a)[Y] = (b)[Y] + (c)[Y]; \
	(a)[Z] = (b)[Z] + (c)[Z]; \
}

/** @brief Add 2D vectors at `b' and `c', store result at `a'. */
#define V2ADD2(a, b, c) { \
	(a)[X] = (b)[X] + (c)[X]; \
	(a)[Y] = (b)[Y] + (c)[Y]; \
}

/** @brief Add 4D vectors at `b' and `c', store result at `a'. */
#define HADD2(a, b, c) { \
	(a)[X] = (b)[X] + (c)[X]; \
	(a)[Y] = (b)[Y] + (c)[Y]; \
	(a)[Z] = (b)[Z] + (c)[Z]; \
	(a)[W] = (b)[W] + (c)[W]; \
}

/**
 * @brief Add vectors of length `n' at `b' and `c', store result at
 * `a'.
 */
#define VADD2N(a, b, c, n) { \
	register int _vadd2; \
	for (_vadd2 = 0; _vadd2 < (n); _vadd2++) { \
		(a)[_vadd2] = (b)[_vadd2] + (c)[_vadd2]; \
	} \
}


/**
 * @brief Subtract 3D vector at `c' from vector at `b', store result at
 * `a'.
 */
#define VSUB2(a, b, c) { \
	(a)[X] = (b)[X] - (c)[X]; \
	(a)[Y] = (b)[Y] - (c)[Y]; \
	(a)[Z] = (b)[Z] - (c)[Z]; \
}

/**
 * @brief Subtract 2D vector at `c' from vector at `b', store result at
 * `a'.
 */
#define V2SUB2(a, b, c) { \
	(a)[X] = (b)[X] - (c)[X]; \
	(a)[Y] = (b)[Y] - (c)[Y]; \
}

/**
 * @brief Subtract 4D vector at `c' from vector at `b', store result at
 * `a'.
 */
#define HSUB2(a, b, c) { \
	(a)[X] = (b)[X] - (c)[X]; \
	(a)[Y] = (b)[Y] - (c)[Y]; \
	(a)[Z] = (b)[Z] - (c)[Z]; \
	(a)[W] = (b)[W] - (c)[W]; \
}

/**
 * @brief Subtract `n' length vector at `c' from vector at `b', store
 * result at `a'.
 */
#define VSUB2N(a, b, c, n) { \
	register int _vsub2; \
	for (_vsub2 = 0; _vsub2 < (n); _vsub2++) { \
		(a)[_vsub2] = (b)[_vsub2] - (c)[_vsub2]; \
	} \
}


/** @brief 3D Vectors:  A = B - C - D */
#define VSUB3(a, b, c, d) { \
	(a)[X] = (b)[X] - (c)[X] - (d)[X]; \
	(a)[Y] = (b)[Y] - (c)[Y] - (d)[Y]; \
	(a)[Z] = (b)[Z] - (c)[Z] - (d)[Z]; \
}

/** @brief 2D Vectors:  A = B - C - D */
#define V2SUB3(a, b, c, d) { \
	(a)[X] = (b)[X] - (c)[X] - (d)[X]; \
	(a)[Y] = (b)[Y] - (c)[Y] - (d)[Y]; \
}

/** @brief 4D Vectors:  A = B - C - D */
#define HSUB3(a, b, c, d) { \
	(a)[X] = (b)[X] - (c)[X] - (d)[X]; \
	(a)[Y] = (b)[Y] - (c)[Y] - (d)[Y]; \
	(a)[Z] = (b)[Z] - (c)[Z] - (d)[Z]; \
	(a)[W] = (b)[W] - (c)[W] - (d)[W]; \
}

/** @brief Vectors:  A = B - C - D for vectors of length `n'. */
#define VSUB3N(a, b, c, d, n) { \
	register int _vsub3; \
	for (_vsub3 = 0; _vsub3 < (n); _vsub3++) { \
		(a)[_vsub3] = (b)[_vsub3] - (c)[_vsub3] - (d)[_vsub3]; \
	} \
}


/** @brief Add 3 3D vectors at `b', `c', and `d', store result at `a'. */
#define VADD3(a, b, c, d) { \
	(a)[X] = (b)[X] + (c)[X] + (d)[X]; \
	(a)[Y] = (b)[Y] + (c)[Y] + (d)[Y]; \
	(a)[Z] = (b)[Z] + (c)[Z] + (d)[Z]; \
}

/** @brief Add 3 2D vectors at `b', `c', and `d', store result at `a'. */
#define V2ADD3(a, b, c, d) { \
	(a)[X] = (b)[X] + (c)[X] + (d)[X]; \
	(a)[Y] = (b)[Y] + (c)[Y] + (d)[Y]; \
}

/** @brief Add 3 4D vectors at `b', `c', and `d', store result at `a'. */
#define HADD3(a, b, c, d) { \
	(a)[X] = (b)[X] + (c)[X] + (d)[X]; \
	(a)[Y] = (b)[Y] + (c)[Y] + (d)[Y]; \
	(a)[Z] = (b)[Z] + (c)[Z] + (d)[Z]; \
	(a)[W] = (b)[W] + (c)[W] + (d)[W]; \
}

/**
 * @brief Add 3 vectors of length `n' at `b', `c', and `d', store
 * result at `a'.
 */
#define VADD3N(a, b, c, d, n) { \
	register int _vadd3; \
	for (_vadd3 = 0; _vadd3 < (n); _vadd3++) { \
		(a)[_vadd3] = (b)[_vadd3] + (c)[_vadd3] + (d)[_vadd3]; \
	} \
}


/**
 * @brief Add 4 vectors at `b', `c', `d', and `e', store result at
 * `a'.
 */
#define VADD4(a, b, c, d, e) { \
	(a)[X] = (b)[X] + (c)[X] + (d)[X] + (e)[X]; \
	(a)[Y] = (b)[Y] + (c)[Y] + (d)[Y] + (e)[Y]; \
	(a)[Z] = (b)[Z] + (c)[Z] + (d)[Z] + (e)[Z]; \
}

/**
 * @brief Add 4 2D vectors at `b', `c', `d', and `e', store result at
 * `a'.
 */
#define V2ADD4(a, b, c, d, e) { \
	(a)[X] = (b)[X] + (c)[X] + (d)[X] + (e)[X]; \
	(a)[Y] = (b)[Y] + (c)[Y] + (d)[Y] + (e)[Y]; \
}

/**
 * @brief Add 4 4D vectors at `b', `c', `d', and `e', store result at
 * `a'.
 */
#define HADD4(a, b, c, d, e) { \
	(a)[X] = (b)[X] + (c)[X] + (d)[X] + (e)[X]; \
	(a)[Y] = (b)[Y] + (c)[Y] + (d)[Y] + (e)[Y]; \
	(a)[Z] = (b)[Z] + (c)[Z] + (d)[Z] + (e)[Z]; \
	(a)[W] = (b)[W] + (c)[W] + (d)[W] + (e)[W]; \
}

/**
 * @brief Add 4 `n' length vectors at `b', `c', `d', and `e', store
 * result at `a'.
 */
#define VADD4N(a, b, c, d, e, n) { \
	register int _vadd4; \
	for (_vadd4 = 0; _vadd4 < (n); _vadd4++) { \
		(a)[_vadd4] = (b)[_vadd4] + (c)[_vadd4] + (d)[_vadd4] + (e)[_vadd4]; \
	} \
}


/** @brief Scale 3D vector at `b' by scalar `c', store result at `a'. */
#define VSCALE(a, b, c) { \
	(a)[X] = (b)[X] * (c); \
	(a)[Y] = (b)[Y] * (c); \
	(a)[Z] = (b)[Z] * (c); \
}

/** @brief Scale 2D vector at `b' by scalar `c', store result at `a'. */
#define V2SCALE(a, b, c) { \
	(a)[X] = (b)[X] * (c); \
	(a)[Y] = (b)[Y] * (c); \
}

/** @brief Scale 4D vector at `b' by scalar `c', store result at `a'. */
#define HSCALE(a, b, c) { \
	(a)[X] = (b)[X] * (c); \
	(a)[Y] = (b)[Y] * (c); \
	(a)[Z] = (b)[Z] * (c); \
	(a)[W] = (b)[W] * (c); \
}

/**
 * @brief Scale vector of length `n' at `b' by scalar `c', store
 * result at `a'
 */
#define VSCALEN(a, b, c, n) { \
	register int _vscale; \
	for (_vscale = 0; _vscale < (n); _vscale++) { \
		(a)[_vscale] = (b)[_vscale] * (c); \
	} \
}

/** @brief Normalize vector `a' to be a unit vector. */
#define VUNITIZE(a) { \
	register double _f = MAGSQ(a); \
	if (! NEAR_EQUAL(_f, 1.0, VUNITIZE_TOL)) { \
		_f = sqrt(_f); \
		if (_f < VDIVIDE_TOL) { \
			VSETALL((a), 0.0); \
		} else { \
			_f = 1.0/_f; \
			(a)[X] *= _f; (a)[Y] *= _f; (a)[Z] *= _f; \
		} \
	} \
}

/** @brief If vector magnitude is too small, return an error code. */
#define VUNITIZE_RET(a, ret) { \
	register double _f; \
	_f = MAGNITUDE(a); \
	if (_f < VDIVIDE_TOL) return ret; \
	_f = 1.0/_f; \
	(a)[X] *= _f; (a)[Y] *= _f; (a)[Z] *= _f; \
}

/**
 * @brief Find the sum of two points, and scale the result.  Often
 * used to find the midpoint.
 */
#define VADD2SCALE(o, a, b, s) { \
			(o)[X] = ((a)[X] + (b)[X]) * (s); \
			(o)[Y] = ((a)[Y] + (b)[Y]) * (s); \
			(o)[Z] = ((a)[Z] + (b)[Z]) * (s); \
}

#define VADD2SCALEN(o, a, b, n) { \
	register int _vadd2scale; \
	for (_vadd2scale = 0; \
	_vadd2scale < (n); \
	_vadd2scale++) { \
		(o)[_vadd2scale] = ((a)[_vadd2scale] + (b)[_vadd2scale]) * (s); \
	} \
}

/**
 * @brief Find the difference between two points, and scale result.
 * Often used to compute bounding sphere radius given rpp points.
 */
#define VSUB2SCALE(o, a, b, s) { \
			(o)[X] = ((a)[X] - (b)[X]) * (s); \
			(o)[Y] = ((a)[Y] - (b)[Y]) * (s); \
			(o)[Z] = ((a)[Z] - (b)[Z]) * (s); \
}

#define VSUB2SCALEN(o, a, b, n) { \
	register int _vsub2scale; \
	for (_vsub2scale = 0; \
	_vsub2scale < (n); \
	_vsub2scale++) { \
		(o)[_vsub2scale] = ((a)[_vsub2scale] - (b)[_vsub2scale]) * (s); \
	} \
}


/** @brief Combine together several vectors, scaled by a scalar. */
#define VCOMB3(o, a, b, c, d, e, f) { \
	(o)[X] = (a) * (b)[X] + (c) * (d)[X] + (e) * (f)[X]; \
	(o)[Y] = (a) * (b)[Y] + (c) * (d)[Y] + (e) * (f)[Y]; \
	(o)[Z] = (a) * (b)[Z] + (c) * (d)[Z] + (e) * (f)[Z]; \
}

#define VCOMB3N(o, a, b, c, d, e, f, n) { \
	register int _vcomb3; \
	for (_vcomb3 = 0; \
	_vcomb3 < (n); \
	_vcomb3++) { \
		(o)[_vcomb3] = (a) * (b)[_vcomb3] + (c) * (d)[_vcomb3] + (e) * (f)[_vcomb3]; \
	} \
}

#define VCOMB2(o, a, b, c, d) { \
	(o)[X] = (a) * (b)[X] + (c) * (d)[X]; \
	(o)[Y] = (a) * (b)[Y] + (c) * (d)[Y]; \
	(o)[Z] = (a) * (b)[Z] + (c) * (d)[Z]; \
}

#define VCOMB2N(o, a, b, c, d, n) { \
	register int _vcomb2; \
	for (_vcomb2 = 0; \
	_vcomb2 < (n); \
	_vcomb2++) { \
		(o)[_vcomb2] = (a) * (b)[_vcomb2] + (c) * (d)[_vcomb2]; \
	} \
}

#define VJOIN4(a, b, c, d, e, f, g, h, i, j) { \
	(a)[X] = (b)[X] + (c)*(d)[X] + (e)*(f)[X] + (g)*(h)[X] + (i)*(j)[X]; \
	(a)[Y] = (b)[Y] + (c)*(d)[Y] + (e)*(f)[Y] + (g)*(h)[Y] + (i)*(j)[Y]; \
	(a)[Z] = (b)[Z] + (c)*(d)[Z] + (e)*(f)[Z] + (g)*(h)[Z] + (i)*(j)[Z]; \
}

#define VJOIN3(a, b, c, d, e, f, g, h) { \
	(a)[X] = (b)[X] + (c)*(d)[X] + (e)*(f)[X] + (g)*(h)[X]; \
	(a)[Y] = (b)[Y] + (c)*(d)[Y] + (e)*(f)[Y] + (g)*(h)[Y]; \
	(a)[Z] = (b)[Z] + (c)*(d)[Z] + (e)*(f)[Z] + (g)*(h)[Z]; \
}


/**
 * @brief Compose 3D vector at `a' of:
 * Vector at `b' plus
 * scalar `c' times vector at `d' plus
 * scalar `e' times vector at `f'
 */
#define VJOIN2(a, b, c, d, e, f) { \
	(a)[X] = (b)[X] + (c) * (d)[X] + (e) * (f)[X]; \
	(a)[Y] = (b)[Y] + (c) * (d)[Y] + (e) * (f)[Y]; \
	(a)[Z] = (b)[Z] + (c) * (d)[Z] + (e) * (f)[Z]; \
}

/**
 * @brief Compose 2D vector at `a' of:
 * Vector at `b' plus
 * scalar `c' times vector at `d' plus
 * scalar `e' times vector at `f'
 */
#define V2JOIN2(a, b, c, d, e, f) { \
	(a)[X] = (b)[X] + (c) * (d)[X] + (e) * (f)[X]; \
	(a)[Y] = (b)[Y] + (c) * (d)[Y] + (e) * (f)[Y]; \
}

/**
 * @brief Compose 4D vector at `a' of:
 * Vector at `b' plus
 * scalar `c' times vector at `d' plus
 * scalar `e' times vector at `f'
 */
#define HJOIN2(a, b, c, d, e, f) { \
	(a)[X] = (b)[X] + (c) * (d)[X] + (e) * (f)[X]; \
	(a)[Y] = (b)[Y] + (c) * (d)[Y] + (e) * (f)[Y]; \
	(a)[Z] = (b)[Z] + (c) * (d)[Z] + (e) * (f)[Z]; \
	(a)[W] = (b)[W] + (c) * (d)[W] + (e) * (f)[W]; \
}

#define VJOIN2N(a, b, c, d, e, f, n) { \
	register int _vjoin2; \
	for (_vjoin2 = 0; \
	_vjoin2 < (n); \
	_vjoin2++) { \
		(a)[_vjoin2] = (b)[_vjoin2] + (c) * (d)[_vjoin2] + (e) * (f)[_vjoin2]; \
	} \
}


#define VJOIN1(a, b, c, d) { \
	(a)[X] = (b)[X] + (c) * (d)[X]; \
	(a)[Y] = (b)[Y] + (c) * (d)[Y]; \
	(a)[Z] = (b)[Z] + (c) * (d)[Z]; \
}

#define V2JOIN1(a, b, c, d) { \
	(a)[X] = (b)[X] + (c) * (d)[X]; \
	(a)[Y] = (b)[Y] + (c) * (d)[Y]; \
}

#define HJOIN1(a, b, c, d) { \
	(a)[X] = (b)[X] + (c) * (d)[X]; \
	(a)[Y] = (b)[Y] + (c) * (d)[Y]; \
	(a)[Z] = (b)[Z] + (c) * (d)[Z]; \
	(a)[W] = (b)[W] + (c) * (d)[W]; \
}

#define VJOIN1N(a, b, c, d, n) { \
	register int _vjoin1; \
	for (_vjoin1 = 0; \
	_vjoin1 < (n); \
	_vjoin1++) { \
		(a)[_vjoin1] = (b)[_vjoin1] + (c) * (d)[_vjoin1]; \
	} \
}


/**
 * @brief Blend into vector `a'
 * scalar `b' times vector at `c' plus
 * scalar `d' times vector at `e'
 */
#define VBLEND2(a, b, c, d, e) { \
	(a)[X] = (b) * (c)[X] + (d) * (e)[X]; \
	(a)[Y] = (b) * (c)[Y] + (d) * (e)[Y]; \
	(a)[Z] = (b) * (c)[Z] + (d) * (e)[Z]; \
}

#define VBLEND2N(a, b, c, d, e, n) { \
	register int _vblend2; \
	for (_vblend2 = 0; \
	_vblend2 < (n); \
	_vblend2++) { \
		(a)[_vblend2] = (b) * (c)[_vblend2] + (d) * (e)[_vblend2]; \
	} \
}


/**
 * @brief Project vector `a' onto `b'
 * vector `c' is the component of `a' parallel to `b'
 *     "    `d' "   "     "      "   "  orthogonal "   "
 */
#define VPROJECT(a, b, c, d) { \
    VSCALE(c, b, VDOT(a, b) / VDOT(b, b)); \
    VSUB2(d, a, c); \
}

/** @brief Return scalar magnitude squared of vector at `a' */
#define MAGSQ(a)	((a)[X]*(a)[X] + (a)[Y]*(a)[Y] + (a)[Z]*(a)[Z])
#define MAG2SQ(a)	((a)[X]*(a)[X] + (a)[Y]*(a)[Y])

/** @brief Return scalar magnitude of vector at `a' */
#define MAGNITUDE(a) sqrt(MAGSQ(a))

/**
 * @brief Store cross product of vectors at `b' and `c' in vector at
 * `a'.  Note that the "right hand rule" applies: If closing your
 * right hand goes from `b' to `c', then your thumb points in the
 * direction of the cross product.
 *
 * If the angle from `b' to `c' goes clockwise, then the result vector
 * points "into" the plane (inward normal).  Example: b=(0, 1, 0),
 * c=(1, 0, 0), then bXc=(0, 0, -1).
 *
 * If the angle from `b' to `c' goes counter-clockwise, then the
 * result vector points "out" of the plane.  This outward pointing
 * normal is the BRL-CAD convention.
 */
#define VCROSS(a, b, c) { \
	(a)[X] = (b)[Y] * (c)[Z] - (b)[Z] * (c)[Y]; \
	(a)[Y] = (b)[Z] * (c)[X] - (b)[X] * (c)[Z]; \
	(a)[Z] = (b)[X] * (c)[Y] - (b)[Y] * (c)[X]; \
}

/** @brief Compute dot product of vectors at `a' and `b'. */
#define VDOT(a, b)	((a)[X]*(b)[X] + (a)[Y]*(b)[Y] + (a)[Z]*(b)[Z])

#define V2DOT(a, b)	((a)[X]*(b)[X] + (a)[Y]*(b)[Y])

#define HDOT(a, b)	((a)[X]*(b)[X] + (a)[Y]*(b)[Y] + (a)[Z]*(b)[Z] + (a)[W]*(b)[W])


/**
 * @brief Subtract two points to make a vector, dot with another
 * vector.
 */
#define VSUB2DOT(_pt2, _pt, _vec)	(\
	((_pt2)[X] - (_pt)[X]) * (_vec)[X] + \
	((_pt2)[Y] - (_pt)[Y]) * (_vec)[Y] + \
	((_pt2)[Z] - (_pt)[Z]) * (_vec)[Z])

/**
 * @brief Turn a vector into comma-separated list of elements, for
 * subroutine args.
 */
#define V2ARGS(a)	(a)[X], (a)[Y]
#define V3ARGS(a)	(a)[X], (a)[Y], (a)[Z]
#define V4ARGS(a)	(a)[X], (a)[Y], (a)[Z], (a)[W]

/**
 * if a value is within computation tolerance of an integer, clamp the
 * value to that integer.
 *
 * NOTE: should use VDIVIDE_TOL here, but cannot yet until floats are
 * replaced universally with fastf_t's since their epsilon is
 * considerably less than that of a double.
 */
#define INTCLAMP(_a) (NEAR_EQUAL((_a), rint(_a), VUNITIZE_TOL) ? (double)(long)rint(_a) : (_a))

/** @brief integer clamped versions of the previous arg macros. */
#define V2INTCLAMPARGS(a) INTCLAMP((a)[X]), INTCLAMP((a)[Y])
/** @brief integer clamped versions of the previous arg macros. */
#define V3INTCLAMPARGS(a) INTCLAMP((a)[X]), INTCLAMP((a)[Y]), INTCLAMP((a)[Z])
/** @brief integer clamped versions of the previous arg macros. */
#define V4INTCLAMPARGS(a) INTCLAMP((a)[X]), INTCLAMP((a)[Y]), INTCLAMP((a)[Z]), INTCLAMP((a)[W])

/** @brief Print vector name and components on stderr. */
#define V2PRINT(a, b)	\
	(void)fprintf(stderr, "%s (%g, %g)\n", a, V2ARGS(b));
#define VPRINT(a, b)	\
	(void)fprintf(stderr, "%s (%g, %g, %g)\n", a, V3ARGS(b));
#define HPRINT(a, b)	\
	(void)fprintf(stderr, "%s (%g, %g, %g, %g)\n", a, V4ARGS(b));

/**
 * @brief Included below are integer clamped versions of the previous
 * print macros.
 */

#define V2INTCLAMPPRINT(a, b)	\
	(void)fprintf(stderr, "%s (%g, %g)\n", a, V2INTCLAMPARGS(b));
#define VINTCLAMPPRINT(a, b)	\
	(void)fprintf(stderr, "%s (%g, %g, %g)\n", a, V3INTCLAMPARGS(b));
#define HINTCLAMPPRINT(a, b)	\
	(void)fprintf(stderr, "%s (%g, %g, %g, %g)\n", a, V4INTCLAMPARGS(b));

#ifdef __cplusplus
#define CPP_V3PRINT(_os, _title, _p)	(_os) << (_title) << "=(" << \
	(_p)[X] << ", " << (_p)[Y] << ")\n";
#define CPP_VPRINT(_os, _title, _p)	(_os) << (_title) << "=(" << \
	(_p)[X] << ", " << (_p)[Y] << ", " << (_p)[Z] << ")\n";
#define CPP_HPRINT(_os, _title, _p)	(_os) << (_title) << "=(" << \
	(_p)[X] << ", " << (_p)[Y] << ", " << (_p)[Z] << ", " << (_p)[W]<< ")\n";
#endif

/** @brief Vector element multiplication.  Really: diagonal matrix X vect. */
#define VELMUL(a, b, c) { \
	(a)[X] = (b)[X] * (c)[X]; \
	(a)[Y] = (b)[Y] * (c)[Y]; \
	(a)[Z] = (b)[Z] * (c)[Z]; \
}

#define VELMUL3(a, b, c, d) { \
	(a)[X] = (b)[X] * (c)[X] * (d)[X]; \
	(a)[Y] = (b)[Y] * (c)[Y] * (d)[Y]; \
	(a)[Z] = (b)[Z] * (c)[Z] * (d)[Z]; \
}

/** @brief Similar to VELMUL. */
#define VELDIV(a, b, c) { \
	(a)[0] = (b)[0] / (c)[0]; \
	(a)[1] = (b)[1] / (c)[1]; \
	(a)[2] = (b)[2] / (c)[2]; \
}

/**
 * @brief Given a direction vector, compute the inverses of each element.
 * When division by zero would have occured, mark inverse as INFINITY.
 */
#define VINVDIR(_inv, _dir) { \
	if ((_dir)[X] < -SQRT_SMALL_FASTF || (_dir)[X] > SQRT_SMALL_FASTF) { \
		(_inv)[X]=1.0/(_dir)[X]; \
	} else { \
		(_dir)[X] = 0.0; \
		(_inv)[X] = INFINITY; \
	} \
	if ((_dir)[Y] < -SQRT_SMALL_FASTF || (_dir)[Y] > SQRT_SMALL_FASTF) { \
		(_inv)[Y]=1.0/(_dir)[Y]; \
	} else { \
		(_dir)[Y] = 0.0; \
		(_inv)[Y] = INFINITY; \
	} \
	if ((_dir)[Z] < -SQRT_SMALL_FASTF || (_dir)[Z] > SQRT_SMALL_FASTF) { \
		(_inv)[Z]=1.0/(_dir)[Z]; \
	} else { \
		(_dir)[Z] = 0.0; \
		(_inv)[Z] = INFINITY; \
	} \
    }

/**
 * @brief Apply the 3x3 part of a mat_t to a 3-tuple.  This rotates a
 * vector without scaling it (changing its length).
 */
#define MAT3X3VEC(o, mat, vec) { \
	(o)[X] = (mat)[X]*(vec)[X]+(mat)[Y]*(vec)[Y] + (mat)[ 2]*(vec)[Z]; \
	(o)[Y] = (mat)[4]*(vec)[X]+(mat)[5]*(vec)[Y] + (mat)[ 6]*(vec)[Z]; \
	(o)[Z] = (mat)[8]*(vec)[X]+(mat)[9]*(vec)[Y] + (mat)[10]*(vec)[Z]; \
}

/** @brief Multiply a 3-tuple by the 3x3 part of a mat_t. */
#define VEC3X3MAT(o, i, m) { \
	(o)[X] = (i)[X]*(m)[X] + (i)[Y]*(m)[4] + (i)[Z]*(m)[8]; \
	(o)[Y] = (i)[X]*(m)[1] + (i)[Y]*(m)[5] + (i)[Z]*(m)[9]; \
	(o)[Z] = (i)[X]*(m)[2] + (i)[Y]*(m)[6] + (i)[Z]*(m)[10]; \
}

/** @brief Apply the 3x3 part of a mat_t to a 2-tuple (Z part=0). */
#define MAT3X2VEC(o, mat, vec) { \
	(o)[X] = (mat)[0]*(vec)[X] + (mat)[Y]*(vec)[Y]; \
	(o)[Y] = (mat)[4]*(vec)[X] + (mat)[5]*(vec)[Y]; \
	(o)[Z] = (mat)[8]*(vec)[X] + (mat)[9]*(vec)[Y]; \
}

/** @brief Multiply a 2-tuple (Z=0) by the 3x3 part of a mat_t. */
#define VEC2X3MAT(o, i, m) { \
	(o)[X] = (i)[X]*(m)[0] + (i)[Y]*(m)[4]; \
	(o)[Y] = (i)[X]*(m)[1] + (i)[Y]*(m)[5]; \
	(o)[Z] = (i)[X]*(m)[2] + (i)[Y]*(m)[6]; \
}

/**
 * @brief Apply a 4x4 matrix to a 3-tuple which is an absolute Point
 * in space.  Output and input points should be separate arrays.
 */
#define MAT4X3PNT(o, m, i) { \
	register double _f; \
	_f = 1.0/((m)[12]*(i)[X] + (m)[13]*(i)[Y] + (m)[14]*(i)[Z] + (m)[15]); \
	(o)[X]=((m)[0]*(i)[X] + (m)[1]*(i)[Y] + (m)[ 2]*(i)[Z] + (m)[3]) * _f; \
	(o)[Y]=((m)[4]*(i)[X] + (m)[5]*(i)[Y] + (m)[ 6]*(i)[Z] + (m)[7]) * _f; \
	(o)[Z]=((m)[8]*(i)[X] + (m)[9]*(i)[Y] + (m)[10]*(i)[Z] + (m)[11])* _f; \
}

/**
 * @brief Multiply an Absolute 3-Point by a full 4x4 matrix.  Output
 * and input points should be separate arrays.
 */
#define PNT3X4MAT(o, i, m) { \
	register double _f; \
	_f = 1.0/((i)[X]*(m)[3] + (i)[Y]*(m)[7] + (i)[Z]*(m)[11] + (m)[15]); \
	(o)[X]=((i)[X]*(m)[0] + (i)[Y]*(m)[4] + (i)[Z]*(m)[8] + (m)[12]) * _f; \
	(o)[Y]=((i)[X]*(m)[1] + (i)[Y]*(m)[5] + (i)[Z]*(m)[9] + (m)[13]) * _f; \
	(o)[Z]=((i)[X]*(m)[2] + (i)[Y]*(m)[6] + (i)[Z]*(m)[10] + (m)[14])* _f; \
}

/**
 * @brief Multiply an Absolute hvect_t 4-Point by a full 4x4 matrix.
 * Output and input points should be separate arrays.
 */
#define MAT4X4PNT(o, m, i) { \
	(o)[X]=(m)[ 0]*(i)[X] + (m)[ 1]*(i)[Y] + (m)[ 2]*(i)[Z] + (m)[ 3]*(i)[H]; \
	(o)[Y]=(m)[ 4]*(i)[X] + (m)[ 5]*(i)[Y] + (m)[ 6]*(i)[Z] + (m)[ 7]*(i)[H]; \
	(o)[Z]=(m)[ 8]*(i)[X] + (m)[ 9]*(i)[Y] + (m)[10]*(i)[Z] + (m)[11]*(i)[H]; \
	(o)[H]=(m)[12]*(i)[X] + (m)[13]*(i)[Y] + (m)[14]*(i)[Z] + (m)[15]*(i)[H]; \
}

/**
 * @brief Apply a 4x4 matrix to a 3-tuple which is a relative Vector
 * in space. This macro can scale the length of the vector if [15] !=
 * 1.0.  Output and input vectors should be separate arrays.
 */
#define MAT4X3VEC(o, m, i) { \
	register double _f; \
	_f = 1.0/((m)[15]); \
	(o)[X] = ((m)[0]*(i)[X] + (m)[1]*(i)[Y] + (m)[ 2]*(i)[Z]) * _f; \
	(o)[Y] = ((m)[4]*(i)[X] + (m)[5]*(i)[Y] + (m)[ 6]*(i)[Z]) * _f; \
	(o)[Z] = ((m)[8]*(i)[X] + (m)[9]*(i)[Y] + (m)[10]*(i)[Z]) * _f; \
}

#define MAT4XSCALOR(o, m, i) { \
	(o) = (i) / (m)[15]; \
}

/**
 * @brief Multiply a Relative 3-Vector by most of a 4x4 matrix.
 * Output and input vectors should be separate arrays.
 */
#define VEC3X4MAT(o, i, m) { \
	register double _f; \
	_f = 1.0/((m)[15]); \
	(o)[X] = ((i)[X]*(m)[0] + (i)[Y]*(m)[4] + (i)[Z]*(m)[8]) * _f; \
	(o)[Y] = ((i)[X]*(m)[1] + (i)[Y]*(m)[5] + (i)[Z]*(m)[9]) * _f; \
	(o)[Z] = ((i)[X]*(m)[2] + (i)[Y]*(m)[6] + (i)[Z]*(m)[10]) * _f; \
}

/** @brief Multiply a Relative 2-Vector by most of a 4x4 matrix. */
#define VEC2X4MAT(o, i, m) { \
	register double _f; \
	_f = 1.0/((m)[15]); \
	(o)[X] = ((i)[X]*(m)[0] + (i)[Y]*(m)[4]) * _f; \
	(o)[Y] = ((i)[X]*(m)[1] + (i)[Y]*(m)[5]) * _f; \
	(o)[Z] = ((i)[X]*(m)[2] + (i)[Y]*(m)[6]) * _f; \
}

/** @brief Test a vector for non-unit length. */
#define BN_VEC_NON_UNIT_LEN(_vec)	\
	(fabs(MAGSQ(_vec)) < 0.0001 || fabs(fabs(MAGSQ(_vec))-1) > 0.0001)

/**
 * @brief Included below are macros to update min and max X, Y, Z
 * values to contain a point
 */

#define V_MIN(r, s) if ((r) > (s)) r = (s)

#define V_MAX(r, s) if ((r) < (s)) r = (s)

#define VMIN(r, s) { \
	V_MIN((r)[X], (s)[X]); V_MIN((r)[Y], (s)[Y]); V_MIN((r)[Z], (s)[Z]); \
}

#define VMAX(r, s) { \
	V_MAX((r)[X], (s)[X]); V_MAX((r)[Y], (s)[Y]); V_MAX((r)[Z], (s)[Z]); \
}

#define VMINMAX(min, max, pt) { \
	VMIN((min), (pt)); VMAX((max), (pt)); \
}

/**
 * @brief Divide out homogeneous parameter from hvect_t, creating
 * vect_t.
 */
#define HDIVIDE(a, b) { \
	(a)[X] = (b)[X] / (b)[H]; \
	(a)[Y] = (b)[Y] / (b)[H]; \
	(a)[Z] = (b)[Z] / (b)[H]; \
}

/**
 * @brief Some 2-D versions of the 3-D macros given above.
 *
 * A better naming convention is V2MOVE() rather than VMOVE_2D().
 *
 * DEPRECATED: These xxx_2D names are slated to go away, use the
 * others.
 *
 * THESE ARE ALL DEPRECATED.
 */
#define VADD2_2D(a, b, c) V2ADD2(a, b, c)
#define VSUB2_2D(a, b, c) V2SUB2(a, b, c)
#define MAGSQ_2D(a) MAG2SQ(a)
#define VDOT_2D(a, b) V2DOT(a, b)
#define VMOVE_2D(a, b) V2MOVE(a, b)
#define VSCALE_2D(a, b, c) V2SCALE(a, b, c)
#define VJOIN1_2D(a, b, c, d) V2JOIN1(a, b, c, d)


/**
 * @brief Quaternion math definitions.
 *
 * Note that the [W] component will be put in the last (i.e., third)
 * place rather than the first [X] (i.e., [0]) place, so that the X,
 * Y, and Z elements will be compatible with vectors.  Only
 * QUAT_FROM_VROT macros depend on component locations, however.
 */

/**
 * @brief Create Quaternion from Vector and Rotation about vector.
 *
 * To produce a quaternion representing a rotation by PI radians about
 * X-axis:
 *
 * VSET(axis, 1, 0, 0);
 * QUAT_FROM_VROT(quat, M_PI, axis);
 * or
 * QUAT_FROM_ROT(quat, M_PI, 1.0, 0.0, 0.0, 0.0);
 *
 * Alternatively, in degrees:
 * QUAT_FROM_ROT_DEG(quat, 180.0, 1.0, 0.0, 0.0, 0.0);
 */
#define QUAT_FROM_ROT(q, r, x, y, z) { \
	register fastf_t _rot = (r) * 0.5; \
	QSET(q, x, y, z, cos(_rot)); \
	VUNITIZE(q); \
	_rot = sin(_rot); /* _rot is really just a temp variable now */ \
	VSCALE(q, q, _rot); \
}

#define QUAT_FROM_VROT(q, r, v) { \
	register fastf_t _rot = (r) * 0.5; \
	VMOVE(q, v); \
	VUNITIZE(q); \
	(q)[W] = cos(_rot); \
	_rot = sin(_rot); /* _rot is really just a temp variable now */ \
	VSCALE(q, q, _rot); \
}

#define QUAT_FROM_VROT_DEG(q, r, v) \
	QUAT_FROM_VROT(q, ((r)*(M_PI/180.0)), v)

#define QUAT_FROM_ROT_DEG(q, r, x, y, z) \
	QUAT_FROM_ROT(q, ((r)*(M_PI/180.0)), x, y, z)


/**
 * @brief Set quaternion at `a' to have coordinates `b', `c', `d', and
 * `e'.
 */
#define QSET(a, b, c, d, e) { \
	(a)[X] = (b); \
	(a)[Y] = (c); \
	(a)[Z] = (d); \
	(a)[W] = (e); \
}

/** @brief Transfer quaternion at `b' to quaternion at `a'. */
#define QMOVE(a, b) { \
	(a)[X] = (b)[X]; \
	(a)[Y] = (b)[Y]; \
	(a)[Z] = (b)[Z]; \
	(a)[W] = (b)[W]; \
}

/** @brief Add quaternions at `b' and `c', store result at `a'. */
#define QADD2(a, b, c) { \
	(a)[X] = (b)[X] + (c)[X]; \
	(a)[Y] = (b)[Y] + (c)[Y]; \
	(a)[Z] = (b)[Z] + (c)[Z]; \
	(a)[W] = (b)[W] + (c)[W]; \
}

/**
 * @brief Subtract quaternion at `c' from quaternion at `b', store
 * result at `a'.
 */
#define QSUB2(a, b, c) { \
	(a)[X] = (b)[X] - (c)[X]; \
	(a)[Y] = (b)[Y] - (c)[Y]; \
	(a)[Z] = (b)[Z] - (c)[Z]; \
	(a)[W] = (b)[W] - (c)[W]; \
}

/**
 * @brief Scale quaternion at `b' by scalar `c', store result at
 * `a'.
 */
#define QSCALE(a, b, c) { \
	(a)[X] = (b)[X] * (c); \
	(a)[Y] = (b)[Y] * (c); \
	(a)[Z] = (b)[Z] * (c); \
	(a)[W] = (b)[W] * (c); \
}

/** @brief Normalize quaternion 'a' to be a unit quaternion. */
#define QUNITIZE(a) { \
	register double _f; \
	_f = QMAGNITUDE(a); \
	if (_f < VDIVIDE_TOL) _f = 0.0; else _f = 1.0/_f; \
	(a)[X] *= _f; (a)[Y] *= _f; (a)[Z] *= _f; (a)[W] *= _f; \
}

/** @brief Return scalar magnitude squared of quaternion at `a'. */
#define QMAGSQ(a) \
	((a)[X]*(a)[X] + (a)[Y]*(a)[Y] \
	+ (a)[Z]*(a)[Z] + (a)[W]*(a)[W])

/** @brief Return scalar magnitude of quaternion at `a'. */
#define QMAGNITUDE(a) sqrt(QMAGSQ(a))

/** @brief Compute dot product of quaternions at `a' and `b'. */
#define QDOT(a, b) \
	((a)[X]*(b)[X] + (a)[Y]*(b)[Y] \
	+ (a)[Z]*(b)[Z] + (a)[W]*(b)[W])

/**
 * @brief Compute quaternion product a = b * c
 *
 * a[W] = b[W]*c[W] - VDOT(b, c);
 * VCROSS(temp, b, c);
 * VJOIN2(a, temp, b[W], c, c[W], b);
 */
#define QMUL(a, b, c) { \
	(a)[W] = (b)[W]*(c)[W] - (b)[X]*(c)[X] - (b)[Y]*(c)[Y] - (b)[Z]*(c)[Z]; \
	(a)[X] = (b)[W]*(c)[X] + (b)[X]*(c)[W] + (b)[Y]*(c)[Z] - (b)[Z]*(c)[Y]; \
	(a)[Y] = (b)[W]*(c)[Y] + (b)[Y]*(c)[W] + (b)[Z]*(c)[X] - (b)[X]*(c)[Z]; \
	(a)[Z] = (b)[W]*(c)[Z] + (b)[Z]*(c)[W] + (b)[X]*(c)[Y] - (b)[Y]*(c)[X]; \
}

/** @brief Conjugate quaternion */
#define QCONJUGATE(a, b) { \
	(a)[X] = -(b)[X]; \
	(a)[Y] = -(b)[Y]; \
	(a)[Z] = -(b)[Z]; \
	(a)[W] =  (b)[W]; \
}

/** @brief Multiplicative inverse quaternion */
#define QINVERSE(a, b) { \
	register double _f = QMAGSQ(b); \
	if (_f < VDIVIDE_TOL) _f = 0.0; else _f = 1.0/_f; \
	(a)[X] = -(b)[X] * _f; \
	(a)[Y] = -(b)[Y] * _f; \
	(a)[Z] = -(b)[Z] * _f; \
	(a)[W] =  (b)[W] * _f; \
}

/**
 * @brief Blend into quaternion `a'
 *
 * scalar `b' times quaternion at `c' plus
 * scalar `d' times quaternion at `e'
 */
#define QBLEND2(a, b, c, d, e) { \
	(a)[X] = (b) * (c)[X] + (d) * (e)[X]; \
	(a)[Y] = (b) * (c)[Y] + (d) * (e)[Y]; \
	(a)[Z] = (b) * (c)[Z] + (d) * (e)[Z]; \
	(a)[W] = (b) * (c)[W] + (d) * (e)[W]; \
}

/**
 * Macros for dealing with 3-D "extents", aka bounding boxes, that are
 * represented as axis-aligned right parallelpipeds (RPPs).  This is
 * stored as two points: a min point, and a max point.  RPP 1 is
 * defined by lo1, hi1, RPP 2 by lo2, hi2.
 */

/**
 * Compare two bounding boxes and return true if they are disjoint.
 */
#define V3RPP_DISJOINT(_l1, _h1, _l2, _h2) \
      ((_l1)[X] > (_h2)[X] || (_l1)[Y] > (_h2)[Y] || (_l1)[Z] > (_h2)[Z] || \
	(_l2)[X] > (_h1)[X] || (_l2)[Y] > (_h1)[Y] || (_l2)[Z] > (_h1)[Z])

/** Compare two bounding boxes and return true If they overlap. */
#define V3RPP_OVERLAP(_l1, _h1, _l2, _h2) \
    (! ((_l1)[X] > (_h2)[X] || (_l1)[Y] > (_h2)[Y] || (_l1)[Z] > (_h2)[Z] || \
	(_l2)[X] > (_h1)[X] || (_l2)[Y] > (_h1)[Y] || (_l2)[Z] > (_h1)[Z]))

/**
 * @brief If two extents overlap within distance tolerance, return
 * true.
 */
#define V3RPP_OVERLAP_TOL(_l1, _h1, _l2, _h2, _t) \
    (! ((_l1)[X] > (_h2)[X] + (_t)->dist || \
	(_l1)[Y] > (_h2)[Y] + (_t)->dist || \
	(_l1)[Z] > (_h2)[Z] + (_t)->dist || \
	(_l2)[X] > (_h1)[X] + (_t)->dist || \
	(_l2)[Y] > (_h1)[Y] + (_t)->dist || \
	(_l2)[Z] > (_h1)[Z] + (_t)->dist))

/** @brief Is the point within or on the boundary of the RPP? */
#define V3PT_IN_RPP(_pt, _lo, _hi)	(\
	(_pt)[X] >= (_lo)[X] && (_pt)[X] <= (_hi)[X] && \
	(_pt)[Y] >= (_lo)[Y] && (_pt)[Y] <= (_hi)[Y] && \
	(_pt)[Z] >= (_lo)[Z] && (_pt)[Z] <= (_hi)[Z])

/**
 * @brief Within the distance tolerance, is the point within the RPP?
 */
#define V3PT_IN_RPP_TOL(_pt, _lo, _hi, _t)	(\
	(_pt)[X] >= (_lo)[X]-(_t)->dist && (_pt)[X] <= (_hi)[X]+(_t)->dist && \
	(_pt)[Y] >= (_lo)[Y]-(_t)->dist && (_pt)[Y] <= (_hi)[Y]+(_t)->dist && \
	(_pt)[Z] >= (_lo)[Z]-(_t)->dist && (_pt)[Z] <= (_hi)[Z]+(_t)->dist)

/**
 * @brief Is the point outside the RPP by at least the distance tolerance?
 * This will not return true if the point is on the RPP.
 */
#define V3PT_OUT_RPP_TOL(_pt, _lo, _hi, _t)      (\
        (_pt)[X] < (_lo)[X]-(_t)->dist || (_pt)[X] > (_hi)[X]+(_t)->dist || \
        (_pt)[Y] < (_lo)[Y]-(_t)->dist || (_pt)[Y] > (_hi)[Y]+(_t)->dist || \
        (_pt)[Z] < (_lo)[Z]-(_t)->dist || (_pt)[Z] > (_hi)[Z]+(_t)->dist)

/**
 * @brief Determine if one bounding box is within another.  Also
 * returns true if the boxes are the same.
 */
#define V3RPP1_IN_RPP2(_lo1, _hi1, _lo2, _hi2)	(\
	(_lo1)[X] >= (_lo2)[X] && (_hi1)[X] <= (_hi2)[X] && \
	(_lo1)[Y] >= (_lo2)[Y] && (_hi1)[Y] <= (_hi2)[Y] && \
	(_lo1)[Z] >= (_lo2)[Z] && (_hi1)[Z] <= (_hi2)[Z])

/** Convert an azimuth/elevation to a direction vector. */
#define V3DIR_FROM_AZEL(_d, _a, _e) { \
	register fastf_t _c_e = cos(_e); \
	(_d)[X] = cos(_a) * _c_e; \
	(_d)[Y] = sin(_a) * _c_e; \
	(_d)[Z] = sin(_e); \
}

/** Convert a direction vector to azimuth/elevation (in radians). */
#define AZEL_FROM_V3DIR(_a, _e, _d) { \
	(_a) = ((NEAR_ZERO((_d)[X], SMALL_FASTF)) && (NEAR_ZERO((_d)[Y], SMALL_FASTF))) ? 0.0 : atan2(-((_d)[Y]), -((_d)[X])) * -RAD2DEG; \
	(_e) = atan2(-((_d)[Z]), sqrt((_d)[X]*(_d)[X] + (_d)[Y]*(_d)[Y])) * -RAD2DEG; \
}

/*** Macros suitable for declaration statement initialization. ***/

/**
 * 3D vector macro suitable for declaration statement initialization.
 * this sets all vector elements to the specified value similar to
 * VSETALL() but as an initializer array declaration instead of as a
 * statement.
 */
#define VINITALL(_v) {(_v), (_v), (_v)}

/**
 * 2D vector macro suitable for declaration statement initialization.
 * this sets all vector elements to the specified value similar to
 * VSETALLN(hvect_t,val,2) but as an initializer array declaration
 * instead of as a statement.
 */
#define V2INITALL(_v) {(_v), (_v), (_v)}

/**
 * 4D homogeneous vector macro suitable for declaration statement
 * initialization.  this sets all vector elements to the specified
 * value similar to VSETALLN(hvect_t,val,4) but as an initializer
 * array declaration instead of as a statement.
 */
#define HINITALL(_v) {(_v), (_v), (_v), (_v)}

/**
 * 3D vector macro suitable for declaration statement initialization.
 * this sets all vector elements to zero similar to calling
 * VSETALL(0.0) but as an initializer array declaration instead of as
 * a statement.
 */
#define VINIT_ZERO {0.0, 0.0, 0.0}

/**
 * 2D vector macro suitable for declaration statement initialization.
 * this sets all vector elements to zero similar to calling
 * V2SETALL(0.0) but as an initializer array declaration instead of as
 * a statement.
 */
#define V2INIT_ZERO {0.0, 0.0}

/**
 * 4D homogenous vector macro suitable for declaration statement
 * initialization.  this sets all vector elements to zero similar to
 * calling VSETALLN(hvect_t,0.0,4) but as an initializer array
 * declaration instead of as a statement.
 */
#define HINIT_ZERO {0.0, 0.0, 0.0, 0.0}

/**
 * matrix macro suitable for declaration statement initialization.
 * this sets up an identity matrix similar to calling MAT_IDN but as
 * an initializer array declaration instead of as a statement.
 */
#define MAT_INIT_IDN {1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0}

/**
 * matrix macro suitable for declaration statement initialization.
 * this sets up a zero matrix similar to calling MAT_ZERO but as an
 * initializer array declaration instead of as a statement.
 */
#define MAT_INIT_ZERO {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0}

__END_DECLS

#endif /* __VMATH_H__ */

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
