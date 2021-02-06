/*                         V M A T H . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2021 United States Government as represented by
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
/** @addtogroup vmath */
/** @{ */
/** @file vmath.h
 *
 * @brief fundamental vector, matrix, quaternion math macros
 *
 * VMATH defines commonly needed macros for 2D/3D/4D math involving:
 *
 *   points (point2d_t, point_t, and hpoint_t),
 *   vectors (vect2d_t, vect_t, and hvect_t),
 *   quaternions (quat_t),
 *   planes (plane_t), and
 *   4x4 matrices (mat_t).
 *
 * By default, all floating point numbers are stored in arrays using
 * the 'fastf_t' type definition.  It should be manually typedef'd to
 * the "fastest" 64-bit floating point type available on the current
 * hardware with at least 64 bits of precision.  On 16 and 32 bit
 * machines, this is typically "double", but on 64 bit machines, it
 * could be "float".
 *
 * Matrix array elements have the following positions in the matrix:
 * @code
 *			|  0  1  2  3 |		| 0 |
 *	[ 0 1 2 3 ]	|  4  5  6  7 |		| 1 |
 *			|  8  9 10 11 |		| 2 |
 *			| 12 13 14 15 |		| 3 |
 *
 * preVector (vect_t) Matrix (mat_t) postVector (vect_t)
 * @endcode
 *
 * Note that while many people in the computer graphics field use
 * post-multiplication with row vectors (i.e., vector * matrix *
 * matrix ...) VMATH uses the more traditional representation of
 * column vectors (i.e., ... matrix * matrix * vector).  (The matrices
 * in these two representations are the transposes of each
 * other). Therefore, when transforming a vector by a matrix,
 * pre-multiplication is used, i.e.:
 *
 * view_vec = model2view_mat * model_vec
 *
 * Furthermore, additional transformations are multiplied on the left,
 * i.e.:
 *
 * @code
 * vec'  =  T1 * vec
 * vec'' =  T2 * T1 * vec  =  T2 * vec'
 * @endcode
 *
 * The most notable implication of this is the location of the "delta"
 * (translation) values in the matrix, i.e.:
 *
 * @code
 * x'   (R0  R1  R2 Dx) x
 * y' = (R4  R5  R6 Dy) * y
 * z'   (R8  R9 R10 Dz) z
 * w'   (0   0   0  1/s) w
 * @endcode
 *
 * Note -
 *@n  vect_t objects are 3-tuples
 *@n hvect_t objects are 4-tuples
 *
 * Most of these macros require that the result be in separate
 * storage, distinct from the input parameters, except where noted.
 *
 * IMPLEMENTOR NOTES
 *
 * When writing macros like this, it is very important that any
 * variables declared within a macro code blocks start with an
 * underscore in order to (hopefully) minimize any name conflicts with
 * user-provided parameters, such as _f in the following example:
 *
 * @code
 * #define ABC() do { double _f; do stuff; } while (0)
 * @endcode
 *
 * All of the macros that introduce a scope like the preceding
 * example are written as do { } while (0) loops in order to require
 * callers provide a trailing semicolon (e.g., ABC();).  This helps
 * preserve source code formatting.
 */

#ifndef VMATH_H
#define VMATH_H

#include "common.h"

/* needed for additional math defines on Windows when including math.h */
#define _USE_MATH_DEFINES 1

/* for sqrt(), sin(), cos(), rint(), M_PI, INFINITY (HUGE_VAL), and more */
#include <math.h>

/* for floating point tolerances and other math constants */
#include <float.h>


#ifdef __cplusplus
extern "C" {
#endif


#ifndef M_
#  define M_		XXX /**< all with 36-digits of precision */
#endif

#ifndef M_1_2PI
#  define M_1_2PI	0.159154943091895335768883763372514362  /**< 1/(2*pi) */
#endif
#ifndef M_1_PI
#  define M_1_PI	0.318309886183790671537767526745028724  /**< 1/pi */
#endif
#ifndef M_2_PI
#  define M_2_PI	0.636619772367581343075535053490057448  /**< 2/pi */
#endif
#ifndef M_2_SQRTPI
#  define M_2_SQRTPI	1.12837916709551257389615890312154517   /**< 2/sqrt(pi) */
#endif
#ifndef M_E
#  define M_E		2.71828182845904523536028747135266250   /**< e */
#endif
#ifndef M_EULER
#  define M_EULER	0.577215664901532860606512090082402431  /**< Euler's constant */
#endif
#ifndef M_LOG2E
#  define M_LOG2E	1.44269504088896340735992468100189214   /**< log_2(e) */
#endif
#ifndef M_LOG10E
#  define M_LOG10E	0.434294481903251827651128918916605082  /**< log_10(e) */
#endif
#ifndef M_LN2
#  define M_LN2		0.693147180559945309417232121458176568  /**< log_e(2) */
#endif
#ifndef M_LN10
#  define M_LN10	2.30258509299404568401799145468436421   /**< log_e(10) */
#endif
#ifndef M_LNPI
#  define M_LNPI	1.14472988584940017414342735135305871   /** log_e(pi) */
#endif
#ifndef M_PI
#  define M_PI		3.14159265358979323846264338327950288   /**< pi */
#endif
#ifndef M_2PI
#  define M_2PI		6.28318530717958647692528676655900576   /**< 2*pi */
#endif
#ifndef M_PI_2
#  define M_PI_2	1.57079632679489661923132169163975144   /**< pi/2 */
#endif
#ifndef M_PI_3
#  define M_PI_3	1.04719755119659774615421446109316763   /**< pi/3 */
#endif
#ifndef M_PI_4
#  define M_PI_4	0.785398163397448309615660845819875721  /**< pi/4 */
#endif
#ifndef M_SQRT1_2
#  define M_SQRT1_2	0.707106781186547524400844362104849039  /**< sqrt(1/2) */
#endif
#ifndef M_SQRT2
#  define M_SQRT2	1.41421356237309504880168872420969808   /**< sqrt(2) */
#endif
#ifndef M_SQRT3
#  define M_SQRT3	1.73205080756887729352744634150587237   /**< sqrt(3) */
#endif
#ifndef M_SQRTPI
#  define M_SQRTPI	1.77245385090551602729816748334114518   /**< sqrt(pi) */
#endif

#ifndef DEG2RAD
#  define DEG2RAD	0.0174532925199432957692369076848861271 /**< pi/180 */
#endif
#ifndef RAD2DEG
#  define RAD2DEG	57.2957795130823208767981548141051703   /**< 180/pi */
#endif


/**
 * Definitions about limits of floating point representation
 * Eventually, should be tied to type of hardware (IEEE, IBM, Cray)
 * used to implement the fastf_t type.
 *
 * MAX_FASTF - Very close to the largest value that can be held by a
 * fastf_t without overflow.  Typically specified as an integer power
 * of ten, to make the value easy to spot when printed.  TODO: macro
 * function syntax instead of constant (DEPRECATED)
 *
 * SQRT_MAX_FASTF - sqrt(MAX_FASTF), or slightly smaller.  Any number
 * larger than this, if squared, can be expected to * produce an
 * overflow.  TODO: macro function syntax instead of constant
 * (DEPRECATED)
 *
 * SMALL_FASTF - Very close to the smallest value that can be
 * represented while still being greater than zero.  Any number
 * smaller than this (and non-negative) can be considered to be
 * zero; dividing by such a number can be expected to produce a
 * divide-by-zero error.  All divisors should be checked against
 * this value before actual division is performed.  TODO: macro
 * function syntax instead of constant (DEPRECATED)
 *
 * SQRT_SMALL_FASTF - sqrt(SMALL_FASTF), or slightly larger.  The
 * value of this is quite a lot larger than that of SMALL_FASTF.  Any
 * number smaller than this, when squared, can be expected to produce
 * a zero result.  TODO: macro function syntax instead of constant
 * (DEPRECATED)
 *
 */
#if defined(vax)
/* DEC VAX "D" format, the most restrictive */
#  define MAX_FASTF		1.0e37	/* Very close to the largest number */
#  define SQRT_MAX_FASTF	1.0e18	/* This squared just avoids overflow */
#  define SMALL_FASTF		1.0e-37	/* Anything smaller is zero */
#  define SQRT_SMALL_FASTF	1.0e-18	/* This squared gives zero */
#else
/* IBM format, being the next most restrictive format */
#  define MAX_FASTF		1.0e73	/* Very close to the largest number */
#  define SQRT_MAX_FASTF	1.0e36	/* This squared just avoids overflow */
#  define SMALL_FASTF		1.0e-77	/* Anything smaller is zero */
#  if defined(aux)
#    define SQRT_SMALL_FASTF	1.0e-40 /* _doprnt error in libc */
#  else
#    define SQRT_SMALL_FASTF	1.0e-39	/* This squared gives zero */
#  endif
#endif

/** DEPRECATED, do not use */
#define SMALL SQRT_SMALL_FASTF

/**
 * It is necessary to have a representation of 1.0/0.0 or log(0),
 * i.e., "infinity" that fits within the dynamic range of the machine
 * being used.  This constant places an upper bound on the size object
 * which can be represented in the model.  With IEEE 754 floating
 * point, this may print as 'inf' and is represented with all 1 bits
 * in the biased-exponent field and all 0 bits in the fraction with
 * the sign indicating positive (0) or negative (1) infinity.
 * However, we do not assume or rely on IEEE 754 floating point.
 */
#ifndef INFINITY
#  if defined(DBL_MAX)
#    define INFINITY ((fastf_t)DBL_MAX)
#  elif defined(HUGE_VAL)
#    define INFINITY ((fastf_t)HUGE_VAL)
#  elif defined(MAXDOUBLE)
#    define INFINITY ((fastf_t)MAXDOUBLE)
#  elif defined(HUGE)
#    define INFINITY ((fastf_t)HUGE)
/* fall back to a single-precision limit */
#  elif defined(FLT_MAX)
#    define INFINITY ((fastf_t)FLT_MAX)
#  elif defined(HUGE_VALF)
#    define INFINITY ((fastf_t)HUGE_VALF)
#  elif defined(MAXFLOAT)
#    define INFINITY ((fastf_t)MAXFLOAT)
#  else
     /* all else fails, just pick something big slightly under the
      * 32-bit single-precision floating point limit for IEEE 754.
      */
#    define INFINITY ((fastf_t)1.0e38)
#  endif
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


/** @brief number of fastf_t's per vect2d_t */
#define ELEMENTS_PER_VECT2D	2

/** @brief number of fastf_t's per point2d_t */
#define ELEMENTS_PER_POINT2D	2

/** @brief number of fastf_t's per vect_t */
#define ELEMENTS_PER_VECT	3

/** @brief number of fastf_t's per point_t */
#define ELEMENTS_PER_POINT	3

/** @brief number of fastf_t's per hvect_t (homogeneous vector) */
#define ELEMENTS_PER_HVECT	4

/** @brief number of fastf_t's per hpt_t (homogeneous point) */
#define ELEMENTS_PER_HPOINT	4

/** @brief number of fastf_t's per plane_t */
#define ELEMENTS_PER_PLANE	4

/** @brief number of fastf_t's per mat_t */
#define ELEMENTS_PER_MAT	(ELEMENTS_PER_PLANE*ELEMENTS_PER_PLANE)


/*
 * Fundamental types
 */

/** @brief fastest 64-bit (or larger) floating point type */
typedef double fastf_t;

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

/** Vector component names for homogeneous (4-tuple) vectors */
typedef enum vmath_vector_component_ {
    X = 0,
    Y = 1,
    Z = 2,
    W = 3,
    H = W
} vmath_vector_component;

/**
 * Locations of deltas (MD*) and scaling values (MS*) in a 4x4
 * Homogeneous Transform matrix
 */
typedef enum vmath_matrix_component_ {
    MSX = 0,
    MDX = 3,
    MSY = 5,
    MDY = 7,
    MSZ = 10,
    MDZ = 11,
    MSA = 15
} vmath_matrix_component;

/**
 * Evaluates truthfully whether a number is not within valid range of
 * INFINITY to -INFINITY exclusive (open set).
 */
#define INVALID(n) (!((n) > -INFINITY && (n) < INFINITY))

/**
 * Evaluates truthfully whether any components of a vector are not
 * within a valid range.
 */
#define VINVALID(v) (INVALID((v)[X]) || INVALID((v)[Y]) || INVALID((v)[Z]))

/**
 * Evaluates truthfully whether any components of a 2D vector are not
 * within a valid range.
 */
#define V2INVALID(v) (INVALID((v)[X]) || INVALID((v)[Y]))

/**
 * Evaluates truthfully whether any components of a 4D vector are not
 * within a valid range.
 */
#define HINVALID(v) (INVALID((v)[X]) || INVALID((v)[Y]) || INVALID((v)[Z]) || INVALID((v)[W]))

/**
 * Return truthfully whether a value is within a specified epsilon
 * distance from zero.
 */
#ifdef KEITH_WANTS_THIS
/* this is a proposed change to equality/zero testing.  prior behavior
 * evaluated as an open set.  this would change the behavior to that
 * of a closed set so that you can perform exact comparisons against
 * the tolerance and get a match.  examples that fail with the current
 * macro: tol=0.1; 1.1 == 1.0 or tol=0; 1==1
 *
 * these need to be tested carefully to make sure we pass ALL
 * regression and integration tests, which will require some
 * concerted effort to coordinate prior to a release.  first step is
 * to evaluate impact on performance and behavior of our tests.
 */
#  define NEAR_ZERO(val, epsilon)	(!(((val) < -epsilon) || ((val) > epsilon)))
#  define NEAR_ZERO(val, epsilon)	(!(((val) < -epsilon)) && !(((val) > epsilon)))
#else
#  define NEAR_ZERO(val, epsilon)	(((val) > -epsilon) && ((val) < epsilon))
#endif

/**
 * Return truthfully whether all elements of a given vector are within
 * a specified epsilon distance from zero.
 */
#define VNEAR_ZERO(v, tol) \
	(NEAR_ZERO(v[X], tol) \
	 && NEAR_ZERO(v[Y], tol) \
	 && NEAR_ZERO(v[Z], tol))

/**
 * Test for all elements of `v' being smaller than `tol'.
 * Version for degree 2 vectors.
 */
#define V2NEAR_ZERO(v, tol) (NEAR_ZERO(v[X], tol) && NEAR_ZERO(v[Y], tol))

/**
 * Test for all elements of `v' being smaller than `tol'.
 * Version for degree 2 vectors.
 */
#define HNEAR_ZERO(v, tol) \
    (NEAR_ZERO(v[X], tol) \
     && NEAR_ZERO(v[Y], tol) \
     && NEAR_ZERO(v[Z], tol) \
     && NEAR_ZERO(h[W], tol))


/**
 * Return truthfully whether a value is within a minimum
 * representation tolerance from zero.
 */
#define ZERO(_a) NEAR_ZERO((_a), SMALL_FASTF)

/**
 * Return truthfully whether a vector is within a minimum
 * representation tolerance from zero.
 */
#define VZERO(_a) VNEAR_ZERO((_a), SMALL_FASTF)

/**
 * Return truthfully whether a 2d vector is within a minimum
 * representation tolerance from zero.
 */
#define V2ZERO(_a) V2NEAR_ZERO((_a), SMALL_FASTF)

/**
 * Return truthfully whether a homogenized 4-element vector is within
 * a minimum representation tolerance from zero.
 */
#define HZERO(_a) HNEAR_ZERO((_a), SMALL_FASTF)


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
 */
#define EQUAL(_a, _b) NEAR_EQUAL((_a), (_b), SMALL_FASTF)


/**
 * Return truthfully whether two vectors are equal within a minimum
 * representation tolerance.
 */
#define VEQUAL(_a, _b) VNEAR_EQUAL((_a), (_b), SMALL_FASTF)

/**
 * @brief Return truthfully whether two 2D vectors are equal within
 * a minimum representation tolerance.
 */
#define V2EQUAL(_a, _b) V2NEAR_EQUAL((_a), (_b), SMALL_FASTF)

/**
 * @brief Return truthfully whether two higher degree vectors are
 * equal within a minimum representation tolerance.
 */
#define HEQUAL(_a, _b)  HNEAR_EQUAL((_a), (_b), SMALL_FASTF)


/** @brief Compute distance from a point to a plane. */
#define DIST_PNT_PLANE(_pt, _pl) (VDOT(_pt, _pl) - (_pl)[W])

/** @brief Compute distance between two points. */
#define DIST_PNT_PNT_SQ(_a, _b) \
	((_a)[X]-(_b)[X])*((_a)[X]-(_b)[X]) + \
	((_a)[Y]-(_b)[Y])*((_a)[Y]-(_b)[Y]) + \
	((_a)[Z]-(_b)[Z])*((_a)[Z]-(_b)[Z])
#define DIST_PNT_PNT(_a, _b) sqrt(DIST_PNT_PNT_SQ(_a, _b))

/** @brief Compute distance between two 2D points. */
#define DIST_PNT2_PNT2_SQ(_a, _b) \
	((_a)[X]-(_b)[X])*((_a)[X]-(_b)[X]) + \
	((_a)[Y]-(_b)[Y])*((_a)[Y]-(_b)[Y])
#define DIST_PNT2_PNT2(_a, _b) sqrt(DIST_PNT2_PNT2_SQ(_a, _b))

/** @brief set translation values of 4x4 matrix with x, y, z values. */
#define MAT_DELTAS(_m, _x, _y, _z) do { \
	(_m)[MDX] = (_x); \
	(_m)[MDY] = (_y); \
	(_m)[MDZ] = (_z); \
    } while (0)

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
#define MAT_DELTAS_GET(_v, _m) do { \
	(_v)[X] = (_m)[MDX]; \
	(_v)[Y] = (_m)[MDY]; \
	(_v)[Z] = (_m)[MDZ]; \
    } while (0)

/**
 * @brief get translation values of 4x4 matrix to a vector,
 * reversed.
 */
#define MAT_DELTAS_GET_NEG(_v, _m) do { \
	(_v)[X] = -(_m)[MDX]; \
	(_v)[Y] = -(_m)[MDY]; \
	(_v)[Z] = -(_m)[MDZ]; \
    } while (0)

/**
 * @brief increment translation elements in a 4x4 matrix with x, y, z
 * values.
 */
#define MAT_DELTAS_ADD(_m, _x, _y, _z) do { \
	(_m)[MDX] += (_x); \
	(_m)[MDY] += (_y); \
	(_m)[MDZ] += (_z); \
    } while (0)

/**
 * @brief increment translation elements in a 4x4 matrix from a
 * vector.
 */
#define MAT_DELTAS_ADD_VEC(_m, _v) do { \
	(_m)[MDX] += (_v)[X]; \
	(_m)[MDY] += (_v)[Y]; \
	(_m)[MDZ] += (_v)[Z]; \
    } while (0)

/**
 * @brief decrement translation elements in a 4x4 matrix with x, y, z
 * values.
 */
#define MAT_DELTAS_SUB(_m, _x, _y, _z) do { \
	(_m)[MDX] -= (_x); \
	(_m)[MDY] -= (_y); \
	(_m)[MDZ] -= (_z); \
    } while (0)

/**
 * @brief decrement translation elements in a 4x4 matrix from a
 * vector.
 */
#define MAT_DELTAS_SUB_VEC(_m, _v) do { \
	(_m)[MDX] -= (_v)[X]; \
	(_m)[MDY] -= (_v)[Y]; \
	(_m)[MDZ] -= (_v)[Z]; \
    } while (0)

/**
 * @brief decrement translation elements in a 4x4 matrix with x, y, z
 * values.
 */
#define MAT_DELTAS_MUL(_m, _x, _y, _z) do { \
	(_m)[MDX] *= (_x); \
	(_m)[MDY] *= (_y); \
	(_m)[MDZ] *= (_z); \
    } while (0)

/**
 * @brief decrement translation elements in a 4x4 matrix from a
 * vector.
 */
#define MAT_DELTAS_MUL_VEC(_m, _v) do { \
	(_m)[MDX] *= (_v)[X]; \
	(_m)[MDY] *= (_v)[Y]; \
	(_m)[MDZ] *= (_v)[Z]; \
    } while (0)

/** @brief set scale of 4x4 matrix from xyz. */
#define MAT_SCALE(_m, _x, _y, _z) do { \
	(_m)[MSX] = _x; \
	(_m)[MSY] = _y; \
	(_m)[MSZ] = _z; \
    } while (0)

/** @brief set scale of 4x4 matrix from vector. */
#define MAT_SCALE_VEC(_m, _v) do { \
	(_m)[MSX] = (_v)[X]; \
	(_m)[MSY] = (_v)[Y]; \
	(_m)[MSZ] = (_v)[Z]; \
    } while (0)

/** @brief set uniform scale of 4x4 matrix from scalar. */
#define MAT_SCALE_ALL(_m, _s) (_m)[MSA] = (_s)

/** @brief add to scaling elements in a 4x4 matrix from xyz. */
#define MAT_SCALE_ADD(_m, _x, _y, _z) do { \
	(_m)[MSX] += _x; \
	(_m)[MSY] += _y; \
	(_m)[MSZ] += _z; \
    } while (0)

/** @brief add to scaling elements in a 4x4 matrix from vector. */
#define MAT_SCALE_ADD_VEC(_m, _v) do { \
	(_m)[MSX] += (_v)[X]; \
	(_m)[MSY] += (_v)[Y]; \
	(_m)[MSZ] += (_v)[Z]; \
    } while (0)

/** @brief subtract from scaling elements in a 4x4 matrix from xyz. */
#define MAT_SCALE_SUB(_m, _x, _y, _z) do { \
	(_m)[MSX] -= _x; \
	(_m)[MSY] -= _y; \
	(_m)[MSZ] -= _z; \
    } while (0)

/**
 * @brief subtract from scaling elements in a 4x4 matrix from
 * vector.
 */
#define MAT_SCALE_SUB_VEC(_m, _v) do { \
	(_m)[MSX] -= (_v)[X]; \
	(_m)[MSY] -= (_v)[Y]; \
	(_m)[MSZ] -= (_v)[Z]; \
    } while (0)

/** @brief multiply scaling elements in a 4x4 matrix from xyz. */
#define MAT_SCALE_MUL(_m, _x, _y, _z) do { \
	(_m)[MSX] *= _x; \
	(_m)[MSY] *= _y; \
	(_m)[MSZ] *= _z; \
    } while (0)

/** @brief multiply scaling elements in a 4x4 matrix from vector. */
#define MAT_SCALE_MUL_VEC(_m, _v) do { \
	(_m)[MSX] *= (_v)[X]; \
	(_m)[MSY] *= (_v)[Y]; \
	(_m)[MSZ] *= (_v)[Z]; \
    } while (0)


/**
 * In following are macro versions of librt/mat.c functions for when
 * speed really matters.
 */


/** @brief Zero a matrix. */
#define MAT_ZERO(m) do { \
	(m)[0] = (m)[1] = (m)[2] = (m)[3] = \
	(m)[4] = (m)[5] = (m)[6] = (m)[7] = \
	(m)[8] = (m)[9] = (m)[10] = (m)[11] = \
	(m)[12] = (m)[13] = (m)[14] = (m)[15] = 0.0; \
    } while (0)

/** @brief Set matrix to identity. */
#define MAT_IDN(m) do { \
	(m)[1] = (m)[2] = (m)[3] = (m)[4] = \
	(m)[6] = (m)[7] = (m)[8] = (m)[9] = \
	(m)[11] = (m)[12] = (m)[13] = (m)[14] = 0.0; \
	(m)[0] = (m)[5] = (m)[10] = (m)[15] = 1.0; \
    } while (0)

/**
 * @brief set t to the transpose of matrix m
 *
 * NOTE: This implementation will not transpose in-place or
 * overlapping matrices (e.g., MAT_TRANSPOSE(m, m) will be wrong).
 */
#define MAT_TRANSPOSE(t, m) do { \
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
    } while (0)

/** @brief Copy a matrix `m' into `c'. */
#define MAT_COPY(c, m) do { \
	(c)[0] = (m)[0]; \
	(c)[1] = (m)[1]; \
	(c)[2] = (m)[2]; \
	(c)[3] = (m)[3]; \
	(c)[4] = (m)[4]; \
	(c)[5] = (m)[5]; \
	(c)[6] = (m)[6]; \
	(c)[7] = (m)[7]; \
	(c)[8] = (m)[8]; \
	(c)[9] = (m)[9]; \
	(c)[10] = (m)[10]; \
	(c)[11] = (m)[11]; \
	(c)[12] = (m)[12]; \
	(c)[13] = (m)[13]; \
	(c)[14] = (m)[14]; \
	(c)[15] = (m)[15]; \
    } while (0)

/** @brief Set 3D vector at `o' to have coordinates `a', `b', and `c'. */
#define VSET(o, a, b, c) do { \
	(o)[X] = (a); \
	(o)[Y] = (b); \
	(o)[Z] = (c); \
    } while (0)

/** @brief Set 2D vector at `o' to have coordinates `a' and `b'. */
#define V2SET(o, a, b) do { \
	(o)[X] = (a); \
	(o)[Y] = (b); \
    } while (0)

/** @brief Set 4D vector at `o' to homogeneous coordinates `a', `b', `c', and `d'. */
#define HSET(o, a, b, c, d) do { \
	(o)[X] = (a); \
	(o)[Y] = (b); \
	(o)[Z] = (c); \
	(o)[W] = (d); \
    } while (0)


/** @brief Set all elements of 3D vector to same scalar value. */
#define VSETALL(v, s) do { \
	(v)[X] = (v)[Y] = (v)[Z] = (s); \
    } while (0)

/** @brief Set 2D vector elements to same scalar value. */
#define V2SETALL(v, s) do { \
	(v)[X] = (v)[Y] = (s); \
    } while (0)

/** @brief Set 4D vector elements to same scalar value. */
#define HSETALL(v, s) do { \
	(v)[X] = (v)[Y] = (v)[Z] = (v)[W] = (s); \
    } while (0)


/** @brief Set all elements of N-vector to same scalar value. */
#define VSETALLN(v, s, n) do { \
	size_t _j; \
	for (_j=0; _j < (size_t)(n); _j++) v[_j]=(s); \
    } while (0)


/** @brief Transfer 3D vector at `v' to vector at `o'. */
#define VMOVE(o, v) do { \
	(o)[X] = (v)[X]; \
	(o)[Y] = (v)[Y]; \
	(o)[Z] = (v)[Z]; \
    } while (0)

/** @brief Move a 2D vector at `v' to vector at `o'. */
#define V2MOVE(o, v) do { \
	(o)[X] = (v)[X]; \
	(o)[Y] = (v)[Y]; \
    } while (0)

/** @brief Move a homogeneous 4-tuple at `v' to `o'. */
#define HMOVE(o, v) do { \
	(o)[X] = (v)[X]; \
	(o)[Y] = (v)[Y]; \
	(o)[Z] = (v)[Z]; \
	(o)[W] = (v)[W]; \
    } while (0)

/** @brief Transfer vector of length `n' at `v' to vector at `o'. */
#define VMOVEN(o, v, n) do { \
	size_t _vmove; \
	for (_vmove = 0; _vmove < (size_t)(n); _vmove++) { \
	    (o)[_vmove] = (v)[_vmove]; \
	} \
    } while (0)


/**
 * @brief Reverse the direction of 3D vector `v' and store it in `o'.
 *
 * NOTE: Reversing in place works (i.e., VREVERSE(v, v))
 */
#define VREVERSE(o, v) do { \
	(o)[X] = -(v)[X]; \
	(o)[Y] = -(v)[Y]; \
	(o)[Z] = -(v)[Z]; \
    } while (0)

/**
 * @brief Reverse the direction of 2D vector `v' and store it in `o'.
 *
 * NOTE: Reversing in place works (i.e., V2REVERSE(v, v))
 */
#define V2REVERSE(o, v) do { \
	(o)[X] = -(v)[X]; \
	(o)[Y] = -(v)[Y]; \
    } while (0)

/**
 * @brief Same as VREVERSE, but for a 4-tuple.  Also useful on plane_t
 * objects.
 *
 * NOTE: Reversing in place works (i.e., HREVERSE(v, v))
 */
#define HREVERSE(o, v) do { \
	(o)[X] = -(v)[X]; \
	(o)[Y] = -(v)[Y]; \
	(o)[Z] = -(v)[Z]; \
	(o)[W] = -(v)[W]; \
    } while (0)

/** @brief Add 3D vectors at `a' and `b', store result at `o'. */
#define VADD2(o, a, b) do { \
	(o)[X] = (a)[X] + (b)[X]; \
	(o)[Y] = (a)[Y] + (b)[Y]; \
	(o)[Z] = (a)[Z] + (b)[Z]; \
    } while (0)

/** @brief Add 2D vectors at `a' and `b', store result at `o'. */
#define V2ADD2(o, a, b) do { \
	(o)[X] = (a)[X] + (b)[X]; \
	(o)[Y] = (a)[Y] + (b)[Y]; \
    } while (0)

/** @brief Add 4D vectors at `a' and `b', store result at `o'. */
#define HADD2(o, a, b) do { \
	(o)[X] = (a)[X] + (b)[X]; \
	(o)[Y] = (a)[Y] + (b)[Y]; \
	(o)[Z] = (a)[Z] + (b)[Z]; \
	(o)[W] = (a)[W] + (b)[W]; \
    } while (0)

/**
 * @brief Add vectors of length `n' at `a' and `b', store result at
 * `o'.
 */
#define VADD2N(o, a, b, n) do { \
	size_t _vadd2; \
	for (_vadd2 = 0; _vadd2 < (size_t)(n); _vadd2++) { \
		(o)[_vadd2] = (a)[_vadd2] + (b)[_vadd2]; \
	} \
    } while (0)


/**
 * @brief Subtract 3D vector at `b' from vector at `a', store result at
 * `o'.
 */
#define VSUB2(o, a, b) do { \
	(o)[X] = (a)[X] - (b)[X]; \
	(o)[Y] = (a)[Y] - (b)[Y]; \
	(o)[Z] = (a)[Z] - (b)[Z]; \
    } while (0)

/**
 * @brief Subtract 2D vector at `b' from vector at `a', store result at
 * `o'.
 */
#define V2SUB2(o, a, b) do { \
	(o)[X] = (a)[X] - (b)[X]; \
	(o)[Y] = (a)[Y] - (b)[Y]; \
    } while (0)

/**
 * @brief Subtract 4D vector at `b' from vector at `a', store result at
 * `o'.
 */
#define HSUB2(o, a, b) do { \
	(o)[X] = (a)[X] - (b)[X]; \
	(o)[Y] = (a)[Y] - (b)[Y]; \
	(o)[Z] = (a)[Z] - (b)[Z]; \
	(o)[W] = (a)[W] - (b)[W]; \
    } while (0)

/**
 * @brief Subtract `n' length vector at `b' from `n' length vector at
 * `a', store result at `o'.
 */
#define VSUB2N(o, a, b, n) do { \
	size_t _vsub2; \
	for (_vsub2 = 0; _vsub2 < (size_t)(n); _vsub2++) { \
		(o)[_vsub2] = (a)[_vsub2] - (b)[_vsub2]; \
	} \
    } while (0)


/** @brief 3D Vectors:  O = A - B - C */
#define VSUB3(o, a, b, c) do { \
	(o)[X] = (a)[X] - (b)[X] - (c)[X]; \
	(o)[Y] = (a)[Y] - (b)[Y] - (c)[Y]; \
	(o)[Z] = (a)[Z] - (b)[Z] - (c)[Z]; \
    } while (0)

/** @brief 2D Vectors:  O = A - B - C */
#define V2SUB3(o, a, b, c) do { \
	(o)[X] = (a)[X] - (b)[X] - (c)[X]; \
	(o)[Y] = (a)[Y] - (b)[Y] - (c)[Y]; \
    } while (0)

/** @brief 4D Vectors:  O = A - B - C */
#define HSUB3(o, a, b, c) do { \
	(o)[X] = (a)[X] - (b)[X] - (c)[X]; \
	(o)[Y] = (a)[Y] - (b)[Y] - (c)[Y]; \
	(o)[Z] = (a)[Z] - (b)[Z] - (c)[Z]; \
	(o)[W] = (a)[W] - (b)[W] - (c)[W]; \
    } while (0)

/** @brief Vectors:  O = A - B - C for vectors of length `n'. */
#define VSUB3N(o, a, b, c, n) do { \
	size_t _vsub3; \
	for (_vsub3 = 0; _vsub3 < (size_t)(n); _vsub3++) { \
		(o)[_vsub3] = (a)[_vsub3] - (b)[_vsub3] - (c)[_vsub3]; \
	} \
    } while (0)


/** @brief Add 3 3D vectors at `a', `b', and `c', store result at `o'. */
#define VADD3(o, a, b, c) do { \
	(o)[X] = (a)[X] + (b)[X] + (c)[X]; \
	(o)[Y] = (a)[Y] + (b)[Y] + (c)[Y]; \
	(o)[Z] = (a)[Z] + (b)[Z] + (c)[Z]; \
    } while (0)

/** @brief Add 3 2D vectors at `a', `b', and `c', store result at `o'. */
#define V2ADD3(o, a, b, c) do { \
	(o)[X] = (a)[X] + (b)[X] + (c)[X]; \
	(o)[Y] = (a)[Y] + (b)[Y] + (c)[Y]; \
    } while (0)

/** @brief Add 3 4D vectors at `a', `b', and `c', store result at `o'. */
#define HADD3(o, a, b, c) do { \
	(o)[X] = (a)[X] + (b)[X] + (c)[X]; \
	(o)[Y] = (a)[Y] + (b)[Y] + (c)[Y]; \
	(o)[Z] = (a)[Z] + (b)[Z] + (c)[Z]; \
	(o)[W] = (a)[W] + (b)[W] + (c)[W]; \
    } while (0)

/**
 * @brief Add 3 vectors of length `n' at `a', `b', and `c', store
 * result at `o'.
 */
#define VADD3N(o, a, b, c, n) do { \
	size_t _vadd3; \
	for (_vadd3 = 0; _vadd3 < (size_t)(n); _vadd3++) { \
		(o)[_vadd3] = (a)[_vadd3] + (b)[_vadd3] + (c)[_vadd3]; \
	} \
    } while (0)


/**
 * @brief Add 4 vectors at `a', `b', `c', and `d', store result at
 * `o'.
 */
#define VADD4(o, a, b, c, d) do { \
	(o)[X] = (a)[X] + (b)[X] + (c)[X] + (d)[X]; \
	(o)[Y] = (a)[Y] + (b)[Y] + (c)[Y] + (d)[Y]; \
	(o)[Z] = (a)[Z] + (b)[Z] + (c)[Z] + (d)[Z]; \
    } while (0)

/**
 * @brief Add 4 2D vectors at `a', `b', `c', and `d', store result at
 * `o'.
 */
#define V2ADD4(o, a, b, c, d) do { \
	(o)[X] = (a)[X] + (b)[X] + (c)[X] + (d)[X]; \
	(o)[Y] = (a)[Y] + (b)[Y] + (c)[Y] + (d)[Y]; \
    } while (0)

/**
 * @brief Add 4 4D vectors at `a', `b', `c', and `d', store result at
 * `o'.
 */
#define HADD4(o, a, b, c, d) do { \
	(o)[X] = (a)[X] + (b)[X] + (c)[X] + (d)[X]; \
	(o)[Y] = (a)[Y] + (b)[Y] + (c)[Y] + (d)[Y]; \
	(o)[Z] = (a)[Z] + (b)[Z] + (c)[Z] + (d)[Z]; \
	(o)[W] = (a)[W] + (b)[W] + (c)[W] + (d)[W]; \
    } while (0)

/**
 * @brief Add 4 `n' length vectors at `a', `b', `c', and `d', store
 * result at `o'.
 */
#define VADD4N(o, a, b, c, d, n) do { \
	size_t _vadd4;		   \
	for (_vadd4 = 0; _vadd4 < (size_t)(n); _vadd4++) { \
		(o)[_vadd4] = (a)[_vadd4] + (b)[_vadd4] + (c)[_vadd4] + (d)[_vadd4]; \
	} \
    } while (0)


/** @brief Scale 3D vector at `v' by scalar `s', store result at `o'. */
#define VSCALE(o, v, s) do { \
	(o)[X] = (v)[X] * (s); \
	(o)[Y] = (v)[Y] * (s); \
	(o)[Z] = (v)[Z] * (s); \
    } while (0)

/** @brief Scale 2D vector at `v' by scalar `s', store result at `o'. */
#define V2SCALE(o, v, s) do { \
	(o)[X] = (v)[X] * (s); \
	(o)[Y] = (v)[Y] * (s); \
    } while (0)

/** @brief Scale 4D vector at `v' by scalar `s', store result at `o'. */
#define HSCALE(o, v, s) do { \
	(o)[X] = (v)[X] * (s); \
	(o)[Y] = (v)[Y] * (s); \
	(o)[Z] = (v)[Z] * (s); \
	(o)[W] = (v)[W] * (s); \
    } while (0)

/**
 * @brief Scale vector of length `n' at `v' by scalar `s', store
 * result at `o'
 */
#define VSCALEN(o, v, s, n) do { \
	size_t _vscale; \
	for (_vscale = 0; _vscale < (size_t)(n); _vscale++) { \
		(o)[_vscale] = (v)[_vscale] * (s); \
	} \
    } while (0)

/** @brief Normalize vector `v' to be a unit vector. */
#define VUNITIZE(v) do { \
	double _f = MAGSQ(v); \
	if (! NEAR_EQUAL(_f, 1.0, VUNITIZE_TOL)) { \
		_f = sqrt(_f); \
		if (_f < VDIVIDE_TOL) { \
			VSETALL((v), 0.0); \
		} else { \
			_f = 1.0/_f; \
			(v)[X] *= _f; (v)[Y] *= _f; (v)[Z] *= _f; \
		} \
	} \
    } while (0)

/** @brief Normalize 2D vector `v' to be a unit vector. */
#define V2UNITIZE(v) do { \
	double _f = MAG2SQ(v); \
	if (! NEAR_EQUAL(_f, 1.0, VUNITIZE_TOL)) { \
		_f = sqrt(_f); \
		if (_f < VDIVIDE_TOL) { \
			V2SETALL((v), 0.0); \
		} else { \
			_f = 1.0/_f; \
			(v)[X] *= _f; (v)[Y] *= _f; \
		} \
	} \
    } while (0)

/**
 * @brief Find the sum of two points, and scale the result.  Often
 * used to find the midpoint.
 */
#define VADD2SCALE(o, a, b, s) do { \
			(o)[X] = ((a)[X] + (b)[X]) * (s); \
			(o)[Y] = ((a)[Y] + (b)[Y]) * (s); \
			(o)[Z] = ((a)[Z] + (b)[Z]) * (s); \
    } while (0)

/**
 * @brief Find the sum of two vectors of length `n', and scale the
 * result by `s'.  Often used to find the midpoint.
 */
#define VADD2SCALEN(o, a, b, s, n) do { \
	size_t _vadd2scale; \
	for (_vadd2scale = 0; \
	     _vadd2scale < (size_t)(n); \
	     _vadd2scale++) { \
	    (o)[_vadd2scale] = ((a)[_vadd2scale] + (b)[_vadd2scale]) * (s); \
	} \
    } while (0)

/**
 * @brief Find the difference between two points, and scale result.
 * Often used to compute bounding sphere radius given rpp points.
 */
#define VSUB2SCALE(o, a, b, s) do { \
			(o)[X] = ((a)[X] - (b)[X]) * (s); \
			(o)[Y] = ((a)[Y] - (b)[Y]) * (s); \
			(o)[Z] = ((a)[Z] - (b)[Z]) * (s); \
    } while (0)

/**
 * @brief Find the difference between two vectors of length `n', and
 * scale result by `s'.
 */
#define VSUB2SCALEN(o, a, b, s, n) do { \
	size_t _vsub2scale; \
	for (_vsub2scale = 0; \
	     _vsub2scale < (size_t)(n); \
	     _vsub2scale++) { \
	    (o)[_vsub2scale] = ((a)[_vsub2scale] - (b)[_vsub2scale]) * (s); \
	} \
    } while (0)


/**
 * @brief Combine together three vectors, all scaled by scalars.
 *
 * DEPRECATED: API inconsistent, use combo of other macros.
 */
#define VCOMB3(o, a, b, c, d, e, f) do { \
	(o)[X] = (a) * (b)[X] + (c) * (d)[X] + (e) * (f)[X]; \
	(o)[Y] = (a) * (b)[Y] + (c) * (d)[Y] + (e) * (f)[Y]; \
	(o)[Z] = (a) * (b)[Z] + (c) * (d)[Z] + (e) * (f)[Z]; \
    } while (0)

/**
 * @brief Combine together three vectors of length `n', all scaled by
 * scalars.
 *
 * DEPRECATED: API inconsistent, use combo of other macros.
 */
#define VCOMB3N(o, a, b, c, d, e, f, n) do { \
	size_t _vcomb3; \
	for (_vcomb3 = 0; \
	     _vcomb3 < (size_t)(n); \
	     _vcomb3++) { \
	    (o)[_vcomb3] = (a) * (b)[_vcomb3] + (c) * (d)[_vcomb3] + (e) * (f)[_vcomb3]; \
	} \
    } while (0)

/**
 * @brief Combine together 2 vectors, both scaled by scalars.
 */
#define VCOMB2(o, sa, va, sb, vb) do { \
	(o)[X] = (sa) * (va)[X] + (sb) * (vb)[X]; \
	(o)[Y] = (sa) * (va)[Y] + (sb) * (vb)[Y]; \
	(o)[Z] = (sa) * (va)[Z] + (sb) * (vb)[Z]; \
    } while (0)

/**
 * @brief Combine together 2 vectors of length `n', both scaled by
 * scalars.
 */
#define VCOMB2N(o, sa, a, sb, b, n) do { \
	size_t _vcomb2; \
	for (_vcomb2 = 0; \
	     _vcomb2 < (size_t)(n); \
	     _vcomb2++) { \
	    (o)[_vcomb2] = (sa) * (va)[_vcomb2] + (sb) * (vb)[_vcomb2]; \
	} \
    } while (0)

/**
 * DEPRECATED.
 */
#define VJOIN4(a, b, c, d, e, f, g, h, i, j) do { \
	(a)[X] = (b)[X] + (c)*(d)[X] + (e)*(f)[X] + (g)*(h)[X] + (i)*(j)[X]; \
	(a)[Y] = (b)[Y] + (c)*(d)[Y] + (e)*(f)[Y] + (g)*(h)[Y] + (i)*(j)[Y]; \
	(a)[Z] = (b)[Z] + (c)*(d)[Z] + (e)*(f)[Z] + (g)*(h)[Z] + (i)*(j)[Z]; \
    } while (0)

/**
 * Join three scaled vectors to a base `a', storing the result in `o'.
 */
#define VJOIN3(o, a, sb, b, sc, c, sd, d) do { \
	(o)[X] = (a)[X] + (sb)*(b)[X] + (sc)*(c)[X] + (sd)*(d)[X]; \
	(o)[Y] = (a)[Y] + (sb)*(b)[Y] + (sc)*(c)[Y] + (sd)*(d)[Y]; \
	(o)[Z] = (a)[Z] + (sb)*(b)[Z] + (sc)*(c)[Z] + (sd)*(d)[Z]; \
    } while (0)


/**
 * @brief Compose 3D vector at `o' of:
 * Vector at `a' plus
 * scalar `sb' times vector at `b' plus
 * scalar `sc' times vector at `c'
 */
#define VJOIN2(o, a, sb, b, sc, c) do { \
	(o)[X] = (a)[X] + (sb) * (b)[X] + (sc) * (c)[X]; \
	(o)[Y] = (a)[Y] + (sb) * (b)[Y] + (sc) * (c)[Y]; \
	(o)[Z] = (a)[Z] + (sb) * (b)[Z] + (sc) * (c)[Z]; \
    } while (0)

/**
 * @brief Compose 2D vector at `o' of:
 * Vector at `a' plus
 * scalar `sb' times vector at `b' plus
 * scalar `sc' times vector at `c'
 */
#define V2JOIN2(o, a, sb, b, sc, c) do { \
	(o)[X] = (a)[X] + (sb) * (b)[X] + (sc) * (c)[X]; \
	(o)[Y] = (a)[Y] + (sb) * (b)[Y] + (sc) * (c)[Y]; \
    } while (0)

/**
 * @brief Compose 4D vector at `o' of:
 * Vector at `a' plus
 * scalar `sb' times vector at `b' plus
 * scalar `sc' times vector at `c'
 */
#define HJOIN2(o, a, sb, b, sc, c) do { \
	(o)[X] = (a)[X] + (sb) * (b)[X] + (sc) * (c)[X]; \
	(o)[Y] = (a)[Y] + (sb) * (b)[Y] + (sc) * (c)[Y]; \
	(o)[Z] = (a)[Z] + (sb) * (b)[Z] + (sc) * (c)[Z]; \
	(o)[W] = (a)[W] + (sb) * (b)[W] + (sc) * (c)[W]; \
    } while (0)

#define VJOIN2N(o, a, sb, b, sc, c, n) do { \
	size_t _vjoin2; \
	for (_vjoin2 = 0; \
	     _vjoin2 < (size_t)(n); \
	     _vjoin2++) { \
	    (o)[_vjoin2] = (a)[_vjoin2] + (sb) * (b)[_vjoin2] + (sc) * (c)[_vjoin2]; \
	} \
    } while (0)


/**
 * Compose 3D vector at `o' of:
 * vector at `a' plus
 * scalar `sb' times vector at `b'
 *
 * This is basically a shorthand for VSCALE();VADD2();.
 */
#define VJOIN1(o, a, sb, b) do { \
	(o)[X] = (a)[X] + (sb) * (b)[X]; \
	(o)[Y] = (a)[Y] + (sb) * (b)[Y]; \
	(o)[Z] = (a)[Z] + (sb) * (b)[Z]; \
    } while (0)

/**
 * Compose 2D vector at `o' of:
 * vector at `a' plus
 * scalar `sb' times vector at `b'
 *
 * This is basically a shorthand for V2SCALE();V2ADD2();.
 */
#define V2JOIN1(o, a, sb, b) do { \
	(o)[X] = (a)[X] + (sb) * (b)[X]; \
	(o)[Y] = (a)[Y] + (sb) * (b)[Y]; \
    } while (0)

/**
 * Compose 4D vector at `o' of:
 * vector at `a' plus
 * scalar `sb' times vector at `b'
 *
 * This is basically a shorthand for HSCALE();HADD2();.
 */
#define HJOIN1(o, a, sb, b) do { \
	(o)[X] = (a)[X] + (sb) * (b)[X]; \
	(o)[Y] = (a)[Y] + (sb) * (b)[Y]; \
	(o)[Z] = (a)[Z] + (sb) * (b)[Z]; \
	(o)[W] = (a)[W] + (sb) * (b)[W]; \
    } while (0)

/**
 * Compose `n'-D vector at `o' of:
 * vector at `a' plus
 * scalar `sb' times vector at `b'
 *
 * This is basically a shorthand for VSCALEN();VADD2N();.
 */
#define VJOIN1N(o, a, sb, b, n) do { \
	size_t _vjoin1; \
	for (_vjoin1 = 0; \
	     _vjoin1 < (size_t)(n); \
	     _vjoin1++) { \
	    (o)[_vjoin1] = (a)[_vjoin1] + (sb) * (b)[_vjoin1]; \
	} \
    } while (0)


/**
 * @brief Blend into vector `o'
 * scalar `sa' times vector at `a' plus
 * scalar `sb' times vector at `b'
 */
#define VBLEND2(o, sa, a, sb, b) do { \
	(o)[X] = (sa) * (a)[X] + (sb) * (b)[X]; \
	(o)[Y] = (sa) * (a)[Y] + (sb) * (b)[Y]; \
	(o)[Z] = (sa) * (a)[Z] + (sb) * (b)[Z]; \
    } while (0)

/**
 * @brief Blend into vector `o'
 * scalar `sa' times vector at `a' plus
 * scalar `sb' times vector at `b'
 */
#define VBLEND2N(o, sa, a, sb, b, n) do { \
	size_t _vblend2; \
	for (_vblend2 = 0; \
	     _vblend2 < (size_t)(n); \
	     _vblend2++) { \
	    (b)[_vblend2] = (sa) * (a)[_vblend2] + (sb) * (b)[_vblend2]; \
	} \
    } while (0)


/**
 * @brief Project vector `a' onto `b'
 * vector `c' is the component of `a' parallel to `b'
 *     "    `d' "   "     "      "   "  orthogonal "   "
 *
 * FIXME: consistency, the result should come first
 */
#define VPROJECT(a, b, c, d) do { \
    VSCALE(c, b, VDOT(a, b) / VDOT(b, b)); \
    VSUB2(d, a, c); \
    } while (0)

/** @brief Return scalar magnitude squared of vector at `v' */
#define MAGSQ(v)	((v)[X]*(v)[X] + (v)[Y]*(v)[Y] + (v)[Z]*(v)[Z])
#define MAG2SQ(v)	((v)[X]*(v)[X] + (v)[Y]*(v)[Y])


/**
 * @brief Return scalar magnitude of the 3D vector `a'.  This is
 * otherwise known as the Euclidean norm of the provided vector..
 */
#define MAGNITUDE(v) sqrt(MAGSQ(v))

/**
 * @brief Return scalar magnitude of the 2D vector at `a'.  This is
 * otherwise known as the Euclidean norm of the provided vector..
 */
#define MAGNITUDE2(v) sqrt(MAG2SQ(v))

/**
 * @brief Store cross product of 3D vectors at `a' and `b' in vector at `o'.
 *
 * NOTE: The "right hand rule" applies. If closing your right hand
 * goes from `a' to `b', then your thumb points in the direction of
 * the cross product.
 *
 * If the angle from `a' to `b' goes clockwise, then the result vector
 * points "into" the plane (inward normal).  Example: a=(0, 1, 0),
 * b=(1, 0, 0), then aXb=(0, 0, -1).
 *
 * If the angle from `a' to `b' goes counter-clockwise, then the
 * result vector points "out" of the plane.  This outward pointing
 * normal is the BRL-CAD convention.
 */
#define VCROSS(o, a, b) do { \
	(o)[X] = (a)[Y] * (b)[Z] - (a)[Z] * (b)[Y]; \
	(o)[Y] = (a)[Z] * (b)[X] - (a)[X] * (b)[Z]; \
	(o)[Z] = (a)[X] * (b)[Y] - (a)[Y] * (b)[X]; \
    } while (0)

/**
 * Return the analog of a cross product for 2D vectors `a' and `b'
 * as a scalar value.  If a = (ax, ay) and b = (bx, by), then the analog
 * of a x b is det(a*b) = ax*by - ay*bx
 */
#define V2CROSS(a, b) ((a)[X] * (b)[Y] - (a)[Y] * (b)[X])

/**
 * TODO: implement me
 */
#define HCROSS(a, b, c)


/** @brief Compute dot product of vectors at `a' and `b'. */
#define VDOT(a, b)	((a)[X]*(b)[X] + (a)[Y]*(b)[Y] + (a)[Z]*(b)[Z])

#define V2DOT(a, b)	((a)[X]*(b)[X] + (a)[Y]*(b)[Y])

#define HDOT(a, b)	((a)[X]*(b)[X] + (a)[Y]*(b)[Y] + (a)[Z]*(b)[Z] + (a)[W]*(b)[W])


/**
 * @brief Linearly interpolate between two 3D vectors `a' and `b' by
 * interpolant `t', expected in the range [0,1], with result in `o'.
 *
 * NOTE: We intentionally use the form "o = a*(1-t) + b*t" which is
 * mathematically equivalent to "o = a + (b-a)*t".  The latter might
 * result in fewer math operations but cannot guarantee o==v1 when
 * t==1 due to floating-point arithmetic error.
 */
#define VLERP(o, a, b, t) do { \
	(o)[X] = (a)[X] * (1 - (t)) + (b)[X] * (t); \
	(o)[Y] = (a)[Y] * (1 - (t)) + (b)[Y] * (t); \
	(o)[Z] = (a)[Z] * (1 - (t)) + (b)[Z] * (t); \
    } while (0)

/**
 * @brief Linearly interpolate between two 2D vectors `a' and `b' by
 * interpolant `t', expected in the range [0,1], with result in `o'.
 *
 * NOTE: We intentionally use the form "o = a*(1-t) + b*t" which is
 * mathematically equivalent to "o = a + (b-a)*t".  The latter might
 * result in fewer math operations but cannot guarantee o==v1 when
 * t==1 due to floating-point arithmetic error.
 */
#define V2LERP(o, a, b, t) do { \
	(o)[X] = (a)[X] * (1 - (t)) + (b)[X] * (t); \
	(o)[Y] = (a)[Y] * (1 - (t)) + (b)[Y] * (t); \
    } while (0)

/**
 * @brief Linearly interpolate between two 4D vectors `a' and `b' by
 * interpolant `t', expected in the range [0,1], with result in `o'.
 *
 * NOTE: We intentionally use the form "o = a*(1-t) + b*t" which is
 * mathematically equivalent to "o = a + (b-a)*t".  The latter might
 * result in fewer math operations but cannot guarantee o==v1 when
 * t==1 due to floating-point arithmetic error.
 */
#define HLERP(o, a, b, t) do { \
	(o)[X] = (a)[X] * (1 - (t)) + (b)[X] * (t); \
	(o)[Y] = (a)[Y] * (1 - (t)) + (b)[Y] * (t); \
	(o)[Z] = (a)[Z] * (1 - (t)) + (b)[Z] * (t); \
	(o)[W] = (a)[W] * (1 - (t)) + (b)[W] * (t); \
    } while (0)


/**
 * @brief Subtract two points to make a vector, dot with another
 * vector.  Returns the dot product scalar value.
 */
#define VSUB2DOT(_pt2, _pt, _vec)	(\
	((_pt2)[X] - (_pt)[X]) * (_vec)[X] + \
	((_pt2)[Y] - (_pt)[Y]) * (_vec)[Y] + \
	((_pt2)[Z] - (_pt)[Z]) * (_vec)[Z])

/**
 * @brief Turn a vector into comma-separated list of elements, for
 * variable argument subroutines (e.g. printf()).
 */
#define V2ARGS(a)	(a)[X], (a)[Y]
#define V3ARGS(a)	(a)[X], (a)[Y], (a)[Z]
#define V4ARGS(a)	(a)[X], (a)[Y], (a)[Z], (a)[W]

/**
 * Clamp values within tolerance of an integer to that value.
 *
 * For example, INTCLAMP(10.0000123123) evaluates to 10.0
 *
 * NOTE: should use VDIVIDE_TOL here, but cannot yet.  we use
 * VUINITIZE_TOL until floats are replaced universally with fastf_t's
 * since their epsilon is considerably less than that of a double.
 */
#define INTCLAMP(_a) (NEAR_EQUAL((_a), rint(_a), VUNITIZE_TOL) ? rint(_a) : (_a))

/** Clamp a 3D vector's elements to nearby integer values. */
#define VINTCLAMP(_v) do { \
	(_v)[X] = INTCLAMP((_v)[X]); \
	(_v)[Y] = INTCLAMP((_v)[Y]); \
	(_v)[Z] = INTCLAMP((_v)[Z]); \
    } while (0)

/** Clamp a 2D vector's elements to nearby integer values. */
#define V2INTCLAMP(_v) do { \
	(_v)[X] = INTCLAMP((_v)[X]); \
	(_v)[Y] = INTCLAMP((_v)[Y]); \
    } while (0)

/** Clamp a 4D vector's elements to nearby integer values. */
#define HINTCLAMP(_v) do { \
	VINTCLAMP(_v); \
	(_v)[W] = INTCLAMP((_v)[W]); \
    } while (0)


/** @brief integer clamped versions of the previous arg macros. */
#define V2INTCLAMPARGS(a) INTCLAMP((a)[X]), INTCLAMP((a)[Y])
/** @brief integer clamped versions of the previous arg macros. */
#define V3INTCLAMPARGS(a) INTCLAMP((a)[X]), INTCLAMP((a)[Y]), INTCLAMP((a)[Z])
/** @brief integer clamped versions of the previous arg macros. */
#define V4INTCLAMPARGS(a) INTCLAMP((a)[X]), INTCLAMP((a)[Y]), INTCLAMP((a)[Z]), INTCLAMP((a)[W])

/** @brief Print vector name and components on stderr. */
#define V2PRINT(a, b)	\
	fprintf(stderr, "%s (%.6f, %.6g)\n", a, V2ARGS(b));
#define VPRINT(a, b)	\
	fprintf(stderr, "%s (%.6f, %.6f, %.6f)\n", a, V3ARGS(b));
#define HPRINT(a, b)	\
	fprintf(stderr, "%s (%.6f, %.6f, %.6f, %.6f)\n", a, V4ARGS(b));

/**
 * @brief Included below are integer clamped versions of the previous
 * print macros.
 */

#define V2INTCLAMPPRINT(a, b)	\
	fprintf(stderr, "%s (%g, %g)\n", a, V2INTCLAMPARGS(b));
#define VINTCLAMPPRINT(a, b)	\
	fprintf(stderr, "%s (%g, %g, %g)\n", a, V3INTCLAMPARGS(b));
#define HINTCLAMPPRINT(a, b)	\
	fprintf(stderr, "%s (%g, %g, %g, %g)\n", a, V4INTCLAMPARGS(b));


/** @brief Vector element multiplication.  Really: diagonal matrix X vect. */
#define VELMUL(o, a, b) do { \
	(o)[X] = (a)[X] * (b)[X]; \
	(o)[Y] = (a)[Y] * (b)[Y]; \
	(o)[Z] = (a)[Z] * (b)[Z]; \
    } while (0)

#define VELMUL3(o, a, b, c) do { \
	(o)[X] = (a)[X] * (b)[X] * (c)[X]; \
	(o)[Y] = (a)[Y] * (b)[Y] * (c)[Y]; \
	(o)[Z] = (a)[Z] * (b)[Z] * (c)[Z]; \
    } while (0)

/** @brief Similar to VELMUL. */
#define VELDIV(o, a, b) do { \
	(o)[X] = (a)[X] / (b)[X]; \
	(o)[Y] = (a)[Y] / (b)[Y]; \
	(o)[Z] = (a)[Z] / (b)[Z]; \
    } while (0)

/**
 * @brief Given a direction vector, compute the inverses of each element.
 * When division by zero would have occurred, mark inverse as INFINITY.
 */
#define VINVDIR(_inv, _dir) do { \
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
    } while (0)

/**
 * @brief Apply the 3x3 part of a mat_t to a 3-tuple.  This rotates a
 * vector without scaling it (changing its length).
 */
#define MAT3X3VEC(o, mat, vec) do { \
	(o)[X] = (mat)[X]*(vec)[X]+(mat)[Y]*(vec)[Y] + (mat)[ 2]*(vec)[Z]; \
	(o)[Y] = (mat)[4]*(vec)[X]+(mat)[5]*(vec)[Y] + (mat)[ 6]*(vec)[Z]; \
	(o)[Z] = (mat)[8]*(vec)[X]+(mat)[9]*(vec)[Y] + (mat)[10]*(vec)[Z]; \
    } while (0)

/** @brief Multiply a 3-tuple by the 3x3 part of a mat_t. */
#define VEC3X3MAT(o, i, m) do { \
	(o)[X] = (i)[X]*(m)[X] + (i)[Y]*(m)[4] + (i)[Z]*(m)[8]; \
	(o)[Y] = (i)[X]*(m)[1] + (i)[Y]*(m)[5] + (i)[Z]*(m)[9]; \
	(o)[Z] = (i)[X]*(m)[2] + (i)[Y]*(m)[6] + (i)[Z]*(m)[10]; \
    } while (0)

/** @brief Apply the 3x3 part of a mat_t to a 2-tuple (Z part=0). */
#define MAT3X2VEC(o, mat, vec) do { \
	(o)[X] = (mat)[0]*(vec)[X] + (mat)[Y]*(vec)[Y]; \
	(o)[Y] = (mat)[4]*(vec)[X] + (mat)[5]*(vec)[Y]; \
	(o)[Z] = (mat)[8]*(vec)[X] + (mat)[9]*(vec)[Y]; \
    } while (0)

/** @brief Multiply a 2-tuple (Z=0) by the 3x3 part of a mat_t. */
#define VEC2X3MAT(o, i, m) do { \
	(o)[X] = (i)[X]*(m)[0] + (i)[Y]*(m)[4]; \
	(o)[Y] = (i)[X]*(m)[1] + (i)[Y]*(m)[5]; \
	(o)[Z] = (i)[X]*(m)[2] + (i)[Y]*(m)[6]; \
    } while (0)

/**
 * @brief Apply a 4x4 matrix to a 3-tuple which is an absolute Point
 * in space.  Output and input points should be separate arrays.
 */
#define MAT4X3PNT(o, m, i) do { \
	double _f; \
	_f = 1.0/((m)[12]*(i)[X] + (m)[13]*(i)[Y] + (m)[14]*(i)[Z] + (m)[15]); \
	(o)[X]=((m)[0]*(i)[X] + (m)[1]*(i)[Y] + (m)[ 2]*(i)[Z] + (m)[3]) * _f; \
	(o)[Y]=((m)[4]*(i)[X] + (m)[5]*(i)[Y] + (m)[ 6]*(i)[Z] + (m)[7]) * _f; \
	(o)[Z]=((m)[8]*(i)[X] + (m)[9]*(i)[Y] + (m)[10]*(i)[Z] + (m)[11])* _f; \
    } while (0)

/**
 * @brief Multiply an Absolute 3-Point by a full 4x4 matrix.  Output
 * and input points should be separate arrays.
 */
#define PNT3X4MAT(o, i, m) do { \
	double _f; \
	_f = 1.0/((i)[X]*(m)[3] + (i)[Y]*(m)[7] + (i)[Z]*(m)[11] + (m)[15]); \
	(o)[X]=((i)[X]*(m)[0] + (i)[Y]*(m)[4] + (i)[Z]*(m)[8] + (m)[12]) * _f; \
	(o)[Y]=((i)[X]*(m)[1] + (i)[Y]*(m)[5] + (i)[Z]*(m)[9] + (m)[13]) * _f; \
	(o)[Z]=((i)[X]*(m)[2] + (i)[Y]*(m)[6] + (i)[Z]*(m)[10] + (m)[14])* _f; \
    } while (0)

/**
 * @brief Multiply an Absolute hvect_t 4-Point by a full 4x4 matrix.
 * Output and input points should be separate arrays.
 */
#define MAT4X4PNT(o, m, i) do { \
	(o)[X]=(m)[ 0]*(i)[X] + (m)[ 1]*(i)[Y] + (m)[ 2]*(i)[Z] + (m)[ 3]*(i)[W]; \
	(o)[Y]=(m)[ 4]*(i)[X] + (m)[ 5]*(i)[Y] + (m)[ 6]*(i)[Z] + (m)[ 7]*(i)[W]; \
	(o)[Z]=(m)[ 8]*(i)[X] + (m)[ 9]*(i)[Y] + (m)[10]*(i)[Z] + (m)[11]*(i)[W]; \
	(o)[W]=(m)[12]*(i)[X] + (m)[13]*(i)[Y] + (m)[14]*(i)[Z] + (m)[15]*(i)[W]; \
    } while (0)

/**
 * @brief Apply a 4x4 matrix to a 3-tuple which is a relative Vector
 * in space. This macro can scale the length of the vector if [15] !=
 * 1.0.  Output and input vectors should be separate arrays.
 */
#define MAT4X3VEC(o, m, i) do { \
	double _f; \
	_f = 1.0/((m)[15]); \
	(o)[X] = ((m)[0]*(i)[X] + (m)[1]*(i)[Y] + (m)[ 2]*(i)[Z]) * _f; \
	(o)[Y] = ((m)[4]*(i)[X] + (m)[5]*(i)[Y] + (m)[ 6]*(i)[Z]) * _f; \
	(o)[Z] = ((m)[8]*(i)[X] + (m)[9]*(i)[Y] + (m)[10]*(i)[Z]) * _f; \
    } while (0)

#define MAT4XSCALOR(o, m, i) do { \
	(o) = (i) / (m)[15]; \
    } while (0)

/**
 * @brief Multiply a Relative 3-Vector by most of a 4x4 matrix.
 * Output and input vectors should be separate arrays.
 */
#define VEC3X4MAT(o, i, m) do { \
	double _f; \
	_f = 1.0/((m)[15]); \
	(o)[X] = ((i)[X]*(m)[0] + (i)[Y]*(m)[4] + (i)[Z]*(m)[8]) * _f; \
	(o)[Y] = ((i)[X]*(m)[1] + (i)[Y]*(m)[5] + (i)[Z]*(m)[9]) * _f; \
	(o)[Z] = ((i)[X]*(m)[2] + (i)[Y]*(m)[6] + (i)[Z]*(m)[10]) * _f; \
    } while (0)

/** @brief Multiply a Relative 2-Vector by most of a 4x4 matrix. */
#define VEC2X4MAT(o, i, m) do { \
	double _f; \
	_f = 1.0/((m)[15]); \
	(o)[X] = ((i)[X]*(m)[0] + (i)[Y]*(m)[4]) * _f; \
	(o)[Y] = ((i)[X]*(m)[1] + (i)[Y]*(m)[5]) * _f; \
	(o)[Z] = ((i)[X]*(m)[2] + (i)[Y]*(m)[6]) * _f; \
    } while (0)

/**
 * @brief Included below are macros to update min and max X, Y, Z
 * values to contain a point
 */

#define V_MIN(r, s) if ((r) > (s)) r = (s)

#define V_MAX(r, s) if ((r) < (s)) r = (s)

#ifdef VMIN
#  undef VMIN
#endif
#define VMIN(r, s) do { \
	V_MIN((r)[X], (s)[X]); V_MIN((r)[Y], (s)[Y]); V_MIN((r)[Z], (s)[Z]); \
    } while (0)

#ifdef VMAX
#  undef VMAX
#endif
#define VMAX(r, s) do { \
	V_MAX((r)[X], (s)[X]); V_MAX((r)[Y], (s)[Y]); V_MAX((r)[Z], (s)[Z]); \
    } while (0)

#ifdef VMINMAX
#  undef VMINMAX
#endif
#define VMINMAX(min, max, pt) do { \
	VMIN((min), (pt)); VMAX((max), (pt)); \
    } while (0)

/**
 * @brief Included below are macros to update min and max X, Y
 * values to contain a point
 */

#define V2MIN(r, s) do { \
	V_MIN((r)[X], (s)[X]); V_MIN((r)[Y], (s)[Y]); \
    } while (0)

#define V2MAX(r, s) do { \
	V_MAX((r)[X], (s)[X]); V_MAX((r)[Y], (s)[Y]); \
    } while (0)

#define V2MINMAX(min, max, pt) do { \
	V2MIN((min), (pt)); V2MAX((max), (pt)); \
    } while (0)

/**
 * clamp a value to a low/high number.
 */
#define CLAMP(_v, _l, _h) V_MAX((_v), (_l)); else V_MIN((_v), (_h))


/**
 * @brief Divide out homogeneous parameter from hvect_t, creating
 * vect_t.
 */
#define HDIVIDE(o, v) do { \
	(o)[X] = (v)[X] / (v)[W]; \
	(o)[Y] = (v)[Y] / (v)[W]; \
	(o)[Z] = (v)[Z] / (v)[W]; \
    } while (0)

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
#define QUAT_FROM_ROT(q, r, x, y, z) do { \
	fastf_t _rot = (r) * 0.5; \
	QSET(q, x, y, z, cos(_rot)); \
	VUNITIZE(q); \
	_rot = sin(_rot); /* _rot is really just a temp variable now */ \
	VSCALE(q, q, _rot); \
    } while (0)

#define QUAT_FROM_VROT(q, r, v) do { \
	fastf_t _rot = (r) * 0.5; \
	VMOVE(q, v); \
	VUNITIZE(q); \
	(q)[W] = cos(_rot); \
	_rot = sin(_rot); /* _rot is really just a temp variable now */ \
	VSCALE(q, q, _rot); \
    } while (0)

#define QUAT_FROM_VROT_DEG(q, r, v) \
	QUAT_FROM_VROT(q, ((r)*DEG2RAD), v)

#define QUAT_FROM_ROT_DEG(q, r, x, y, z) \
	QUAT_FROM_ROT(q, ((r)*DEG2RAD), x, y, z)


/**
 * @brief Set quaternion at `a' to have coordinates `b', `c', `d', and
 * `e'.
 */
#define QSET(a, b, c, d, e) do { \
	(a)[X] = (b); \
	(a)[Y] = (c); \
	(a)[Z] = (d); \
	(a)[W] = (e); \
    } while (0)

/** @brief Transfer quaternion at `b' to quaternion at `a'. */
#define QMOVE(a, b) do { \
	(a)[X] = (b)[X]; \
	(a)[Y] = (b)[Y]; \
	(a)[Z] = (b)[Z]; \
	(a)[W] = (b)[W]; \
    } while (0)

/** @brief Add quaternions at `b' and `c', store result at `a'. */
#define QADD2(a, b, c) do { \
	(a)[X] = (b)[X] + (c)[X]; \
	(a)[Y] = (b)[Y] + (c)[Y]; \
	(a)[Z] = (b)[Z] + (c)[Z]; \
	(a)[W] = (b)[W] + (c)[W]; \
    } while (0)

/**
 * @brief Subtract quaternion at `c' from quaternion at `b', store
 * result at `a'.
 */
#define QSUB2(a, b, c) do { \
	(a)[X] = (b)[X] - (c)[X]; \
	(a)[Y] = (b)[Y] - (c)[Y]; \
	(a)[Z] = (b)[Z] - (c)[Z]; \
	(a)[W] = (b)[W] - (c)[W]; \
    } while (0)

/**
 * @brief Scale quaternion at `b' by scalar `c', store result at
 * `a'.
 */
#define QSCALE(a, b, c) do { \
	(a)[X] = (b)[X] * (c); \
	(a)[Y] = (b)[Y] * (c); \
	(a)[Z] = (b)[Z] * (c); \
	(a)[W] = (b)[W] * (c); \
    } while (0)

/** @brief Normalize quaternion 'a' to be a unit quaternion. */
#define QUNITIZE(a) do { \
	double _f; \
	_f = QMAGNITUDE(a); \
	if (_f < VDIVIDE_TOL) _f = 0.0; else _f = 1.0/_f; \
	(a)[X] *= _f; (a)[Y] *= _f; (a)[Z] *= _f; (a)[W] *= _f; \
    } while (0)

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
#define QMUL(a, b, c) do { \
	(a)[W] = (b)[W]*(c)[W] - (b)[X]*(c)[X] - (b)[Y]*(c)[Y] - (b)[Z]*(c)[Z]; \
	(a)[X] = (b)[W]*(c)[X] + (b)[X]*(c)[W] + (b)[Y]*(c)[Z] - (b)[Z]*(c)[Y]; \
	(a)[Y] = (b)[W]*(c)[Y] + (b)[Y]*(c)[W] + (b)[Z]*(c)[X] - (b)[X]*(c)[Z]; \
	(a)[Z] = (b)[W]*(c)[Z] + (b)[Z]*(c)[W] + (b)[X]*(c)[Y] - (b)[Y]*(c)[X]; \
    } while (0)

/** @brief Conjugate quaternion */
#define QCONJUGATE(a, b) do { \
	(a)[X] = -(b)[X]; \
	(a)[Y] = -(b)[Y]; \
	(a)[Z] = -(b)[Z]; \
	(a)[W] =  (b)[W]; \
    } while (0)

/** @brief Multiplicative inverse quaternion */
#define QINVERSE(a, b) do { \
	double _f = QMAGSQ(b); \
	if (_f < VDIVIDE_TOL) _f = 0.0; else _f = 1.0/_f; \
	(a)[X] = -(b)[X] * _f; \
	(a)[Y] = -(b)[Y] * _f; \
	(a)[Z] = -(b)[Z] * _f; \
	(a)[W] =  (b)[W] * _f; \
    } while (0)

/**
 * @brief Blend into quaternion `a'
 *
 * scalar `b' times quaternion at `c' plus
 * scalar `d' times quaternion at `e'
 */
#define QBLEND2(a, b, c, d, e) do { \
	(a)[X] = (b) * (c)[X] + (d) * (e)[X]; \
	(a)[Y] = (b) * (c)[Y] + (d) * (e)[Y]; \
	(a)[Z] = (b) * (c)[Z] + (d) * (e)[Z]; \
	(a)[W] = (b) * (c)[W] + (d) * (e)[W]; \
    } while (0)

/**
 * Macros for dealing with 3-D "extents", aka bounding boxes, that are
 * represented as axis-aligned right parallelepipeds (RPPs).  This is
 * stored as two points: a min point, and a max point.  RPP 1 is
 * defined by lo1, hi1, RPP 2 by lo2, hi2.
 */

/**
 * Compare two bounding boxes and return true if they are disjoint.
 */
#define V3RPP_DISJOINT(_l1, _h1, _l2, _h2) \
      ((_l1)[X] > (_h2)[X] || (_l1)[Y] > (_h2)[Y] || (_l1)[Z] > (_h2)[Z] || \
	(_l2)[X] > (_h1)[X] || (_l2)[Y] > (_h1)[Y] || (_l2)[Z] > (_h1)[Z])

/**
 * Compare two bounding boxes and return true if they are disjoint
 * by at least distance tolerance.
 */
#define V3RPP_DISJOINT_TOL(_l1, _h1, _l2, _h2, _t) \
       ((_l1)[X] > (_h2)[X] + (_t) || \
	(_l1)[Y] > (_h2)[Y] + (_t) || \
	(_l1)[Z] > (_h2)[Z] + (_t) || \
	(_l2)[X] > (_h1)[X] + (_t) || \
	(_l2)[Y] > (_h1)[Y] + (_t) || \
	(_l2)[Z] > (_h1)[Z] + (_t))

/** Compare two bounding boxes and return true If they overlap. */
#define V3RPP_OVERLAP(_l1, _h1, _l2, _h2) \
    (! ((_l1)[X] > (_h2)[X] || (_l1)[Y] > (_h2)[Y] || (_l1)[Z] > (_h2)[Z] || \
	(_l2)[X] > (_h1)[X] || (_l2)[Y] > (_h1)[Y] || (_l2)[Z] > (_h1)[Z]))

/**
 * @brief If two extents overlap within distance tolerance, return
 * true.
 */
#define V3RPP_OVERLAP_TOL(_l1, _h1, _l2, _h2, _t) \
    (! ((_l1)[X] > (_h2)[X] + (_t) || \
	(_l1)[Y] > (_h2)[Y] + (_t) || \
	(_l1)[Z] > (_h2)[Z] + (_t) || \
	(_l2)[X] > (_h1)[X] + (_t) || \
	(_l2)[Y] > (_h1)[Y] + (_t) || \
	(_l2)[Z] > (_h1)[Z] + (_t)))

/**
 * @brief Is the point within or on the boundary of the RPP?
 *
 * FIXME: should not be using >= <=, '=' case is unreliable
 */
#define V3PNT_IN_RPP(_pt, _lo, _hi)	(\
	(_pt)[X] >= (_lo)[X] && (_pt)[X] <= (_hi)[X] && \
	(_pt)[Y] >= (_lo)[Y] && (_pt)[Y] <= (_hi)[Y] && \
	(_pt)[Z] >= (_lo)[Z] && (_pt)[Z] <= (_hi)[Z])

/**
 * @brief Within the distance tolerance, is the point within the RPP?
 *
 * FIXME: should not be using >= <=, '=' case is unreliable
 */
#define V3PNT_IN_RPP_TOL(_pt, _lo, _hi, _t)	(\
	(_pt)[X] >= (_lo)[X]-(_t) && (_pt)[X] <= (_hi)[X]+(_t) && \
	(_pt)[Y] >= (_lo)[Y]-(_t) && (_pt)[Y] <= (_hi)[Y]+(_t) && \
	(_pt)[Z] >= (_lo)[Z]-(_t) && (_pt)[Z] <= (_hi)[Z]+(_t))

/**
 * @brief Is the point outside the RPP by at least the distance tolerance?
 * This will not return true if the point is on the RPP.
 */
#define V3PNT_OUT_RPP_TOL(_pt, _lo, _hi, _t)      (\
	(_pt)[X] < (_lo)[X]-(_t) || (_pt)[X] > (_hi)[X]+(_t) || \
	(_pt)[Y] < (_lo)[Y]-(_t) || (_pt)[Y] > (_hi)[Y]+(_t) || \
	(_pt)[Z] < (_lo)[Z]-(_t) || (_pt)[Z] > (_hi)[Z]+(_t))

/**
 * @brief Determine if one bounding box is within another.  Also
 * returns true if the boxes are the same.
 *
 * FIXME: should not be using >= <=, '=' case is unreliable
 */
#define V3RPP1_IN_RPP2(_lo1, _hi1, _lo2, _hi2)	(\
	(_lo1)[X] >= (_lo2)[X] && (_hi1)[X] <= (_hi2)[X] && \
	(_lo1)[Y] >= (_lo2)[Y] && (_hi1)[Y] <= (_hi2)[Y] && \
	(_lo1)[Z] >= (_lo2)[Z] && (_hi1)[Z] <= (_hi2)[Z])

/** Convert an azimuth/elevation to a direction vector. */
#define V3DIR_FROM_AZEL(_d, _a, _e) do { \
	fastf_t _c_e = cos(_e); \
	(_d)[X] = cos(_a) * _c_e; \
	(_d)[Y] = sin(_a) * _c_e; \
	(_d)[Z] = sin(_e); \
    } while (0)

/** Convert a direction vector to azimuth/elevation (in radians). */
#define AZEL_FROM_V3DIR(_a, _e, _d) do { \
	(_a) = ((NEAR_ZERO((_d)[X], SMALL_FASTF)) && (NEAR_ZERO((_d)[Y], SMALL_FASTF))) ? 0.0 : atan2(-((_d)[Y]), -((_d)[X])) * -RAD2DEG; \
	(_e) = atan2(-((_d)[Z]), sqrt((_d)[X]*(_d)[X] + (_d)[Y]*(_d)[Y])) * -RAD2DEG; \
    } while (0)


/** Swap two 3D vectors */
#define VSWAP(_a, _b) do { \
	fastf_t _t; \
	_t = (_a)[X]; \
	(_a)[X] = (_b)[X]; \
	(_b)[X] = _t; \
	_t = (_a)[Y]; \
	(_a)[Y] = (_b)[Y]; \
	(_b)[Y] = _t; \
	_t = (_a)[Z]; \
	(_a)[Z] = (_b)[Z]; \
	(_b)[Z] = _t; \
    } while (0)

/** Swap two 2D vectors */
#define V2SWAP(_a, _b) do { \
	fastf_t _t; \
	_t = (_a)[X]; \
	(_a)[X] = (_b)[X]; \
	(_b)[X] = _t; \
	_t = (_a)[Y]; \
	(_a)[Y] = (_b)[Y]; \
	(_b)[Y] = _t; \
    } while (0)

/** Swap two 4D vectors */
#define HSWAP(_a, _b) do { \
	fastf_t _t; \
	_t = (_a)[X]; \
	(_a)[X] = (_b)[X]; \
	(_b)[X] = _t; \
	_t = (_a)[Y]; \
	(_a)[Y] = (_b)[Y]; \
	(_b)[Y] = _t; \
	_t = (_a)[Z]; \
	(_a)[Z] = (_b)[Z]; \
	(_b)[Z] = _t; \
	_t = (_a)[W]; \
	(_a)[W] = (_b)[W]; \
	(_b)[W] = _t; \
    } while (0)

/** Swap two 4x4 matrices */
#define MAT_SWAP(_a, _b) do { \
	mat_t _t; \
	MAT_COPY(_t, (_a)); \
	MAT_COPY((_a), (_b)); \
	MAT_COPY((_b), _t); \
    } while (0)

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
 * 4D homogeneous vector macro suitable for declaration statement
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


#ifdef __cplusplus
} /* end extern "C" */
#endif

#endif /* VMATH_H */

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
