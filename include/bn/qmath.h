/*                        Q M A T H . H
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

/*----------------------------------------------------------------------*/
/** @addtogroup bn_quat
 *
 *  @brief
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
/** @{ */
/* @file qmath.h */

#ifndef BN_QMATH_H
#define BN_QMATH_H

#include "common.h"
#include "vmath.h"
#include "bn/defines.h"

__BEGIN_DECLS

/*
 * Quaternion support
 */

/**
 *@brief  Convert Matrix to Quaternion.
 */
BN_EXPORT extern void quat_mat2quat(quat_t quat,
				    const mat_t mat);

/**
 *@brief  Convert Quaternion to Matrix.
 */
BN_EXPORT extern void quat_quat2mat(mat_t mat,
				    const quat_t quat);

/**
 *@brief Gives the euclidean distance between two quaternions.
 */
BN_EXPORT extern double quat_distance(const quat_t q1,
				      const quat_t q2);

/**
 *@brief
 *   Gives the quaternion point representing twice the rotation
 *   from q1 to q2.
 *
 *   Needed for patching Bezier curves together.
 *   A rather poor name admittedly.
 */
BN_EXPORT extern void quat_double(quat_t qout,
				  const quat_t q1,
				  const quat_t q2);

/**
 *@brief Gives the bisector of quaternions q1 and q2.
 *
 * (Could be done with quat_slerp and factor 0.5)
 * [I believe they must be unit quaternions this to work]
 */
BN_EXPORT extern void quat_bisect(quat_t qout,
				  const quat_t q1,
				  const quat_t q2);

/**
 *@brief
 * Do Spherical Linear Interpolation between two unit quaternions
 *  by the given factor.
 *
 * As f goes from 0 to 1, qout goes from q1 to q2.
 * Code based on code by Ken Shoemake
 */
BN_EXPORT extern void quat_slerp(quat_t qout,
				 const quat_t q1,
				 const quat_t q2,
				 double f);

/**
 *@brief
 * Spherical Bezier Interpolate between four quaternions by amount f.
 * These are intended to be used as start and stop quaternions along
 *   with two control quaternions chosen to match spline segments with
 *   first order continuity.
 *
 *  Uses the method of successive bisection.
 */
BN_EXPORT extern void quat_sberp(quat_t qout,
				 const quat_t q1,
				 const quat_t qa,
				 const quat_t qb,
				 const quat_t q2,
				 double f);

/**
 *@brief
 *  Set the quaternion q1 to the quaternion which yields the
 *   smallest rotation from q2 (of the two versions of q1 which
 *   produce the same orientation).
 *
 * Note that smallest euclidean distance implies smallest great
 *   circle distance as well (since surface is convex).
 */
BN_EXPORT extern void quat_make_nearest(quat_t q1,
					const quat_t q2);

BN_EXPORT extern void quat_print(const char *title,
				 const quat_t quat);

/**
 *@brief
 *  Exponentiate a quaternion, assuming that the scalar part is 0.
 *  Code by Ken Shoemake.
 */
BN_EXPORT extern void quat_exp(quat_t out,
			       const quat_t in);

/**
 *@brief
 *  Take the natural logarithm of a unit quaternion.
 *  Code by Ken Shoemake.
 */
BN_EXPORT extern void quat_log(quat_t out,
			       const quat_t in);

__END_DECLS

#endif  /* BN_QMATH_H */
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
