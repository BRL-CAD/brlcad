/*                            B N . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2013 United States Government as represented by
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

/** @addtogroup libbn */
/** @{ */
/** @file bn.h
 *
 * Header file for the BRL-CAD Numerical Computation Library, LIBBN.
 *
 * The library provides a broad assortment of numerical algorithms and
 * computational routines, including random number generation, vector
 * math, matrix math, quaternion math, complex math, synthetic
 * division, root finding, etc.
 *
 */

#ifndef __BN_H__
#define __BN_H__

#include "common.h"

__BEGIN_DECLS

/* interface headers */
#include "bu.h"		/* required for BU_CKMAG */
#include "vmath.h"	/* required for mat_t, vect_t */

#ifndef BN_EXPORT
#  if defined(BN_DLL_EXPORTS) && defined(BN_DLL_IMPORTS)
#    error "Only BN_DLL_EXPORTS or BN_DLL_IMPORTS can be defined, not both."
#  elif defined(BN_DLL_EXPORTS)
#    define BN_EXPORT __declspec(dllexport)
#  elif defined(BN_DLL_IMPORTS)
#    define BN_EXPORT __declspec(dllimport)
#  else
#    define BN_EXPORT
#  endif
#endif

#define BN_AZIMUTH   0
#define BN_ELEVATION 1
#define BN_TWIST     2


/** @} */
/*----------------------------------------------------------------------*/
/** @addtogroup tol */
/** @{ */
/**
 * B N _ T O L
 *
 * @brief Support for uniform tolerances
 *
 * A handy way of passing around the tolerance information needed to
 * perform approximate floating-point calculations on geometry.
 *
 * dist & dist_sq establish the distance tolerance.
 *
 * If two points are closer together than dist, then they are to be
 * considered the same point.
 *
 * For example:
 @code
 point_t a, b;
 vect_t diff;
 VSUB2(diff, a, b);
 if (MAGNITUDE(diff) < tol->dist)	a & b are the same.
 or, more efficiently:
 if (MAQSQ(diff) < tol->dist_sq)
 @endcode
 * perp & para establish the angular tolerance.
 *
 * If two rays emanate from the same point, and their dot product is
 * nearly one, then the two rays are the same, while if their dot
 * product is nearly zero, then they are perpendicular.
 *
 * For example:
 @code
 vect_t a, b;
 if (fabs(VDOT(a, b)) >= tol->para)	a & b are parallel
 if (fabs(VDOT(a, b)) <= tol->perp)	a & b are perpendicular
 @endcode
 *
 *@note
 * tol->dist_sq = tol->dist * tol->dist;
 *@n tol->para = 1 - tol->perp;
 */
struct bn_tol {
    uint32_t magic;
    double dist;		/**< @brief >= 0 */
    double dist_sq;		/**< @brief dist * dist */
    double perp;		/**< @brief nearly 0 */
    double para;		/**< @brief nearly 1 */
};

/**
 * asserts the validity of a bn_tol struct.
 */
#define BN_CK_TOL(_p)	BU_CKMAG(_p, BN_TOL_MAGIC, "bn_tol")

/**
 * initializes a bn_tol struct to zero without allocating any memory.
 */
#define BN_TOL_INIT(_p) { \
	(_p)->magic = BN_TOL_MAGIC; \
	(_p)->dist = 0.0; \
	(_p)->dist_sq = 0.0; \
	(_p)->perp = 0.0; \
	(_p)->para = 1.0; \
    }

/**
 * macro suitable for declaration statement zero-initialization of a
 * bn_tol struct.
 */
#define BN_TOL_INIT_ZERO { BN_TOL_MAGIC, 0.0, 0.0, 0.0, 1.0 }

/**
 * returns truthfully whether a bn_tol struct has been initialized.
 */
#define BN_TOL_IS_INITIALIZED(_p) (((struct bn_tol *)(_p) != (struct bn_tol *)0) && LIKELY((_p)->magic == BN_TOL_MAGIC))

/**
 * replaces the hard coded tolerance value
 */
#define BN_TOL_DIST 0.0005

/**
 * returns truthfully whether a given dot-product of two unspecified
 * vectors are within a specified parallel tolerance.
 */
#define BN_VECT_ARE_PARALLEL(_dot, _tol)				\
    (((_dot) <= -SMALL_FASTF) ? (NEAR_EQUAL((_dot), -1.0, (_tol)->perp)) : (NEAR_EQUAL((_dot), 1.0, (_tol)->perp)))

/**
 * returns truthfully whether a given dot-product of two unspecified
 * vectors are within a specified perpendicularity tolerance.
 */
#define BN_VECT_ARE_PERP(_dot, _tol)					\
    (((_dot) < 0) ? ((-(_dot))<=(_tol)->perp) : ((_dot) <= (_tol)->perp))


/** @} */


/*----------------------------------------------------------------------*/


/* anim.c */

/** @addtogroup anim */
/** @{ */

/**
 * Routines useful in animation programs.
 *
 * Orientation conventions: The default object orientation is facing
 * the positive x-axis, with the positive y-axis to the object's left
 * and the positive z-axis above the object.
 *
 * The default view orientation for rt and mged is facing the negative
 * z-axis, with the negative x-axis leading to the left and the
 * positive y-axis going upwards.
 */

/* FIXME: These should all have bn_ prefixes */

/**
 * @brief Pre-multiply a rotation matrix by a matrix which maps the
 * z-axis to the negative x-axis, the y-axis to the z-axis and the
 * x-axis to the negative y-axis.
 *
 * This has the effect of twisting an object in the default view
 * orientation into the default object orientation before applying the
 * matrix.  Given a matrix designed to operate on an object, yield a
 * matrix which operates on the view.
 */
BN_EXPORT extern void anim_v_permute(mat_t m);

/**
 * @brief Undo the mapping done by anim_v_permute().
 *
 * This has the effect of twisting an object in the default object
 * orientation into the default view orientation before applying the
 * matrix.  Given a matrix designed to operate on the view, yield a
 * matrix which operates on an object.
 */
BN_EXPORT extern void anim_v_unpermute(mat_t m);

/**
 * @brief Transpose matrix in place
 */
BN_EXPORT extern void anim_tran(mat_t m);

/**
 * @brief
 * Convert the rotational part of a 4x4 transformation matrix to zyx
 * form, that is to say, rotations carried out in the order z, y, and
 * then x. The angles are stored in radians as a vector in the order
 * x, y, z. A return value of ERROR1 means that arbitrary assumptions
 * were necessary. ERROR2 means that the conversion failed.
 */
BN_EXPORT extern int anim_mat2zyx(const mat_t viewrot,
				  vect_t angle);

/**
 * @brief
 * Convert the rotational part of a 4x4 transformation matrix to
 * yaw-pitch-roll form, that is to say, +roll degrees about the
 * x-axis, -pitch degrees about the y-axis, and +yaw degrees about the
 * z-axis. The angles are stored in radians as a vector in the order
 * y, p, r.  A return of ERROR1 means that arbitrary assumptions were
 * necessary.  ERROR2 means that the conversion failed.
 */
BN_EXPORT extern int anim_mat2ypr(mat_t viewrot,
				  vect_t angle);

/**
 * @brief
 * This interprets the rotational part of a 4x4 transformation matrix
 * in terms of unit quaternions. The result is stored as a vector in
 * the order x, y, z, w.  The algorithm is from Ken Shoemake,
 * Animating Rotation with Quaternion Curves, 1985 SIGGraph Conference
 * Proceeding, p.245.
 */
BN_EXPORT extern int anim_mat2quat(quat_t quat,
				   const mat_t viewrot);

/**
 * @brief Create a premultiplication rotation matrix to turn the front
 * of an object (its x-axis) to the given yaw, pitch, and roll, which
 * is stored in radians in the vector a.
 */
BN_EXPORT extern void anim_ypr2mat(mat_t m,
				   const vect_t a);

/**
 * @brief Create a post-multiplication rotation matrix, which could be
 * used to move the virtual camera to the given yaw, pitch, and roll,
 * which are stored in radians in the given vector a. The following
 * are equivalent sets of commands:
 *
 * ypr2vmat(matrix, a);
 * -- or --
 * ypr2mat(matrix, a);
 * v_permute(matrix);
 * transpose(matrix;
 */
BN_EXPORT extern void anim_ypr2vmat(mat_t m,
				    const vect_t a);

/**
 * @brief
 * Make matrix to rotate an object to the given yaw, pitch, and
 * roll. (Specified in radians.)
 */
BN_EXPORT extern void anim_y_p_r2mat(mat_t m,
				     double y,
				     double p,
				     double r);

/**
 * @brief Make matrix to rotate an object to the given yaw, pitch, and
 * roll. (Specified in degrees.)
 */
BN_EXPORT extern void anim_dy_p_r2mat(mat_t m,
				      double y,
				      double p,
				      double r);

/**
 * @brief Make a view rotation matrix, given desired yaw, pitch and
 * roll. (Note that the matrix is a permutation of the object rotation
 * matrix).
 */
BN_EXPORT extern void anim_dy_p_r2vmat(mat_t m,
				       double yaw,
				       double pch,
				       double rll);

/**
 * @brief Make a rotation matrix corresponding to a rotation of "x"
 * radians about the x-axis, "y" radians about the y-axis, and then
 * "z" radians about the z-axis.
 */
BN_EXPORT extern void anim_x_y_z2mat(mat_t m,
				     double x,
				     double y,
				     double z);

/**
 * @brief Make a rotation matrix corresponding to a rotation of "x"
 * degrees about the x-axis, "y" degrees about the y-axis, and then
 * "z" degrees about the z-axis.
 */
BN_EXPORT extern void anim_dx_y_z2mat(mat_t m,
				      double x,
				      double y,
				      double z);

/**
 * @brief Make a rotation matrix corresponding to a rotation of "z"
 * radians about the z-axis, "y" radians about the y-axis, and then
 * "x" radians about the x-axis.
 */
BN_EXPORT extern void anim_zyx2mat(mat_t m,
				   const vect_t a);

/**
 * @brief Make a rotation matrix corresponding to a rotation of "z"
 * radians about the z-axis, "y" radians about the y-axis, and then
 * "x" radians about the x-axis.
 */
BN_EXPORT extern void anim_z_y_x2mat(mat_t m,
				     double x,
				     double y,
				     double z);

/**
 * @brief
 * Make a rotation matrix corresponding to a rotation of "z" degrees
 * about the z-axis, "y" degrees about the y-axis, and then "x"
 * degrees about the x-axis.
 */
BN_EXPORT extern void anim_dz_y_x2mat(mat_t m,
				      double x,
				      double y,
				      double z);

/**
 * @brief
 * Make 4x4 matrix from the given quaternion Note: these quaternions
 * are the conjugates of the quaternions used in the librt/qmath.c
 * quat_quat2mat()
 */
BN_EXPORT extern void anim_quat2mat(mat_t m,
				    const quat_t qq);

/**
 * @brief
 * make a matrix which turns a vehicle from the x-axis to point in the
 * desired direction, staying "right-side up" (i.e. the y-axis never has
 * a z-component). A second direction vector is consulted when the
 * given direction is vertical. This is intended to represent the
 * direction from a previous frame.
 */
BN_EXPORT extern void anim_dir2mat(mat_t m,
				   const vect_t d,
				   const vect_t d2);

/**
 * @brief make a matrix which turns a vehicle from the x-axis to point
 * in the desired direction, staying "right-side up". In cases where
 * the direction is vertical, the second vector is consulted. The
 * second vector defines a normal to the vertical plane into which
 * the vehicle's x and z axes should be put. A good choice to put here
 * is the direction of the vehicle's y-axis in the previous frame.
 */
BN_EXPORT extern void anim_dirn2mat(mat_t m,
				    const vect_t dx,
				    const vect_t dn);

/**
 * @brief given the next frame's position, remember the value of the
 * previous frame's position and calculate a matrix which points the
 * x-axis in the direction defined by those two positions. Return new
 * matrix, and the remembered value of the current position, as
 * arguments; return 1 as the normal value, and 0 when there is not
 * yet information to remember.
 */
BN_EXPORT extern int anim_steer_mat(mat_t mat,
				    vect_t point,
				    int end);

/**
 * @brief Add pre- and post- translation to a rotation matrix.  The
 * resulting matrix has the effect of performing the first
 * translation, followed by the rotation, followed by the second
 * translation.
 */
BN_EXPORT extern void anim_add_trans(mat_t m,
				     const vect_t post,
				     const vect_t pre);

/**
 * @brief Rotate the vector "d" through "a" radians about the z-axis.
 */
BN_EXPORT extern void anim_rotatez(fastf_t a,
				   vect_t d);

/**
 * @brief print out 4X4 matrix, with optional colon
 */
BN_EXPORT extern void anim_mat_print(FILE *fp,
				     const mat_t m,
				     int s_colon);

/**
 * @brief print out 4X4 matrix.  formstr must be less than twenty
 * chars
 */
BN_EXPORT extern void anim_mat_printf(FILE *fp,
				      const mat_t m,
				      const char *formstr,
				      const char *linestr,
				      const char *endstr);

/**
 * @brief Reverse the direction of a view matrix, keeping it
 * right-side up
 */
BN_EXPORT extern void anim_view_rev(mat_t m);


/*----------------------------------------------------------------------*/


/* bn_tcl.c */

/**
 * @brief
 * Tcl interfaces to all the LIBBN math routines.
 *
 */


/* Support routines for the math functions */

/* XXX Really need a decode_array function that uses atof(),
 * XXX so that junk like leading { and commas between inputs
 * XXX don't spoil the conversion.
 */

BN_EXPORT extern int bn_decode_mat(mat_t m,
				   const char *str);
BN_EXPORT extern int bn_decode_quat(quat_t q,
				    const char *str);
BN_EXPORT extern int bn_decode_vect(vect_t v,
				    const char *str);
BN_EXPORT extern int bn_decode_hvect(hvect_t v,
				     const char *str);
BN_EXPORT extern void bn_encode_mat(struct bu_vls *vp,
				    const mat_t m);
BN_EXPORT extern void bn_encode_quat(struct bu_vls *vp,
				     const quat_t q);
BN_EXPORT extern void bn_encode_vect(struct bu_vls *vp,
				     const vect_t v);
BN_EXPORT extern void bn_encode_hvect(struct bu_vls *vp,
				      const hvect_t v);

/* The presence of Tcl_Interp as an arg prevents giving arg list */

/**
 * B N _ T C L _ S E T U P
 *@brief
 * Add all the supported Tcl interfaces to LIBBN routines to
 * the list of commands known by the given interpreter.
 */
BN_EXPORT extern void bn_tcl_setup();

/**
 * B N _ I N I T
 *@brief
 * Allows LIBBN to be dynamically loade to a vanilla tclsh/wish with
 * "load /usr/brlcad/lib/libbn.so"
 *
 * The name of this function is specified by TCL.
 */
BN_EXPORT extern int Bn_Init();

/**
 * B N _ M A T _ P R I N T
 */
BN_EXPORT extern void bn_tcl_mat_print();

/** @} */

/*----------------------------------------------------------------------*/


/* chull.c */
/*
 * Routines for the computation of convex hulls in 2D and 3D
 */

/**
 * @brief
 * Melkman's 2D simple polyline O(n) convex hull algorithm
 *
 * On-line construction of the convex hull of a simple polyline
 * Melkman, Avraham A. Information Processing Letters 25.1 (1987): 11-12.
 *
 * See also <a href="http://geomalgorithms.com/a12-_hull-3.html">http://geomalgorithms.com/a12-_hull-3.html</a>
 *
 * @param[out]	hull convex hull array vertices in ccw orientation (max is n)
 * @param	polyline The points defining the input polyline, stored with ccw orientation
 * @param	n the number of points in polyline
 * @return the number of points in the output hull array
 */
BN_EXPORT int bn_polyline_2d_chull(point2d_t** hull, const point2d_t* polyline, int n);

/**
 * @brief
 * Find 2D convex hull for unordered co-planar point sets
 *
 * The monotone chain algorithm's sorting approach is used to do
 * the initial ordering of the points:
 *
 * Another efficient algorithm for convex hulls in two dimensions.
 * Andrew, A. M. Information Processing Letters 9.5 (1979): 216-219.
 *
 * See also <a href="http://geomalgorithms.com/a10-_hull-1.html">http://geomalgorithms.com/a10-_hull-1.html</a>
 *
 * From there, instead of using the monotonic chain hull assembly
 * step, recognize that the points thus ordered can be viewed as
 * defining a simple polyline and use Melkman's algorithm for the
 * hull building.
 *
 * The input point array currently uses type point_t, but all Z
 * values should be zero.
 *
 * @param[out]	hull 2D convex hull array vertices in ccw orientation (max is n)
 * @param	points_2d The input 2d points for which a convex hull will be built
 * @param	n the number of points in the input set
 * @return the number of points in the output hull array or zero if error.
 */
BN_EXPORT int bn_2d_chull(point2d_t** hull, const point2d_t* points_2d, int n);

/**
 * @brief
 * Find 3D coplanar point convex hull for unordered co-planar point sets
 *
 * This function assumes an input an array of 3D points which are coplanar
 * in some arbitrary plane.  This function:
 *
 * 1. Finds the plane that fits the points and picks an origin, x-axis and y-axis
 *    which allow 2D coordinates for all points to be calculated.
 * 2. Calls 2D routines on the array found by step 1 to get a 2D convex hull
 * 3. Translates the resultant 2D hull points back into 3D points so the hull array
 *    contains the bounding hull expressed in the 3D coordinate space of the
 *    original points.
 *
 * @param[out]	hull_3d convex hull array vertices using 3-space coordinates in ccw orientation (max is n)
 * @param	points_3d The input points for which a convex hull will be built
 * @param	n the number of points in the input set
 * @return the number of points in the output hull array
 */
BN_EXPORT int bn_3d_coplanar_chull(point_t** hull, const point_t* points_3d, int n);




/*----------------------------------------------------------------------*/


/* complex.c */

/** @addtogroup complex */
/** @{ */

/*
 * Complex numbers
 */

/* "complex number" data type: */
typedef struct bn_complex {
    double re;		/**< @brief real part */
    double im;		/**< @brief imaginary part */
}  bn_complex_t;

/* functions that are efficiently done as macros: */

#define bn_cx_copy(ap, bp) {*(ap) = *(bp);}
#define bn_cx_neg(cp) { (cp)->re = -((cp)->re);(cp)->im = -((cp)->im);}
#define bn_cx_real(cp)		(cp)->re
#define bn_cx_imag(cp)		(cp)->im

#define bn_cx_add(ap, bp) { (ap)->re += (bp)->re; (ap)->im += (bp)->im;}
#define bn_cx_ampl(cp)		hypot((cp)->re, (cp)->im)
#define bn_cx_amplsq(cp)		((cp)->re * (cp)->re + (cp)->im * (cp)->im)
#define bn_cx_conj(cp) { (cp)->im = -(cp)->im; }
#define bn_cx_cons(cp, r, i) { (cp)->re = r; (cp)->im = i; }
#define bn_cx_phas(cp)		atan2((cp)->im, (cp)->re)
#define bn_cx_scal(cp, s) { (cp)->re *= (s); (cp)->im *= (s); }
#define bn_cx_sub(ap, bp) { (ap)->re -= (bp)->re; (ap)->im -= (bp)->im;}

#define bn_cx_mul(ap, bp)	 	\
    { register fastf_t a__re, b__re; \
	(ap)->re = ((a__re=(ap)->re)*(b__re=(bp)->re)) - (ap)->im*(bp)->im; \
	(ap)->im = a__re*(bp)->im + (ap)->im*b__re; }

/* Output variable "ap" is different from input variables "bp" or "cp" */
#define bn_cx_mul2(ap, bp, cp) { \
	(ap)->re = (cp)->re * (bp)->re - (cp)->im * (bp)->im; \
	(ap)->im = (cp)->re * (bp)->im + (cp)->im * (bp)->re; }

/**
 * B N _ C X _ D I V
 *@brief
 * Divide one complex by another
 *
 * bn_cx_div(&a, &b).  divides a by b.  Zero divisor fails.  a and b
 * may coincide.  Result stored in a.
 */
BN_EXPORT extern void bn_cx_div(bn_complex_t *ap,
				const bn_complex_t *bp);

/**
 * B N _ C X _ S Q R T
 *@brief
 * Compute square root of complex number
 *
 * bn_cx_sqrt(&out, &c) replaces out by sqrt(c)
 *
 * Note: This is a double-valued function; the result of bn_cx_sqrt()
 * always has nonnegative imaginary part.
 */
BN_EXPORT extern void bn_cx_sqrt(bn_complex_t *op,
				 const bn_complex_t *ip);


/*----------------------------------------------------------------------*/


/* mat.c */
/*
 * 4x4 Matrix math
 */

BN_EXPORT extern const mat_t bn_mat_identity;

/**
 * B N _ M A T _ P R I N T
 */
BN_EXPORT extern void bn_mat_print(const char *title,
				   const mat_t m);

/**
 *
 */
BN_EXPORT extern void bn_mat_print_guts(const char *title,
					const mat_t m,
					char *buf,
					int buflen);

/**
 * B N _ M A T _ P R I N T _ V L S
 */
BN_EXPORT extern void bn_mat_print_vls(const char *title,
				       const mat_t m,
				       struct bu_vls *vls);

/**
 * B N _ A T A N 2
 *
 * A wrapper for the system atan2().  On the Silicon Graphics, and
 * perhaps on others, x==0 incorrectly returns infinity.
 */
BN_EXPORT extern double bn_atan2(double x, double y);

#define bn_mat_zero(_m) { \
	bu_log("%s:%d bn_mat_zero() is deprecated, use MAT_ZERO()\n", \
	       __FILE__, __LINE__); \
	(_m)[0] = (_m)[1] = (_m)[2] = (_m)[3] = \
	    (_m)[4] = (_m)[5] = (_m)[6] = (_m)[7] = \
	    (_m)[8] = (_m)[9] = (_m)[10] = (_m)[11] = \
	    (_m)[12] = (_m)[13] = (_m)[14] = (_m)[15] = 0.0; }
/*
  #define bn_mat_zero(_m)	(void)memset((void *)_m, 0, sizeof(mat_t))
*/
#define bn_mat_idn(_m) { \
	bu_log("%s:%d bn_mat_idn() is deprecated, use MAT_IDN()\n", \
	       __FILE__, __LINE__); \
	(_m)[1] = (_m)[2] = (_m)[3] = (_m)[4] = \
	    (_m)[6] = (_m)[7] = (_m)[8] = (_m)[9] = \
	    (_m)[11] = (_m)[12] = (_m)[13] = (_m)[14] = 0.0; \
	(_m)[0] = (_m)[5] = (_m)[10] = (_m)[15] = 1.0; }
/*
  #define bn_mat_idn(_m)	(void)memcpy((void *)_m, (const void *)bn_mat_identity, sizeof(mat_t))
*/

#define bn_mat_copy(_d, _s) { \
	bu_log("%s:%d bn_mat_copy() is deprecated, use MAT_COPY()\n", \
	       __FILE__, __LINE__); \
	(_d)[0] = (_s)[0];\
	(_d)[1] = (_s)[1];\
	(_d)[2] = (_s)[2];\
	(_d)[3] = (_s)[3];\
	(_d)[4] = (_s)[4];\
	(_d)[5] = (_s)[5];\
	(_d)[6] = (_s)[6];\
	(_d)[7] = (_s)[7];\
	(_d)[8] = (_s)[8];\
	(_d)[9] = (_s)[9];\
	(_d)[10] = (_s)[10];\
	(_d)[11] = (_s)[11];\
	(_d)[12] = (_s)[12];\
	(_d)[13] = (_s)[13];\
	(_d)[14] = (_s)[14];\
	(_d)[15] = (_s)[15]; }
/*
  #define bn_mat_copy(_d, _s)	(void)memcpy((void *)_d, (const void *)(_s), sizeof(mat_t))
*/


/**
 * B N _ M A T _ M U L
 *
 * Multiply matrix "a" by "b" and store the result in "o".
 *
 * This is different from multiplying "b" by "a" (most of the time!)
 * Also, "o" must not be the same as either of the inputs.
 */
BN_EXPORT extern void bn_mat_mul(mat_t o,
				 const mat_t a,
				 const mat_t b);

/**
 * B N _ M A T _ M U L 2
 *
 * o = i * o
 *
 * A convenience wrapper for bn_mat_mul() to update a matrix in place.
 * The argument ordering is confusing either way.
 */
BN_EXPORT extern void bn_mat_mul2(const mat_t i,
				  mat_t o);

/**
 * B N _ M A T _ M U L 3
 *
 * o = a * b * c
 *
 * The output matrix may be the same as 'b' or 'c', but may not be
 * 'a'.
 */
BN_EXPORT extern void bn_mat_mul3(mat_t o,
				  const mat_t a,
				  const mat_t b,
				  const mat_t c);

/**
 * B N _ M A T _ M U L 4
 *
 * o = a * b * c * d
 *
 * The output matrix may be the same as any input matrix.
 */
BN_EXPORT extern void bn_mat_mul4(mat_t o,
				  const mat_t a,
				  const mat_t b,
				  const mat_t c,
				  const mat_t d);

/**
 * B N _ M A T X V E C
 *
 * Multiply the matrix "im" by the vector "iv" and store the result in
 * the vector "ov".  Note this is post-multiply, and operates on
 * 4-tuples.  Use MAT4X3VEC() to operate on 3-tuples.
 */
BN_EXPORT extern void bn_matXvec(hvect_t ov,
				 const mat_t im,
				 const hvect_t iv);

/**
 * B N _ M A T _ I N V
 *
 * The matrix pointed at by "input" is inverted and stored in the area
 * pointed at by "output".
 *
 * Calls bu_bomb if matrix is singular.
 */
BN_EXPORT extern void bn_mat_inv(mat_t output,
				 const mat_t input);

/**
 * B N _ M A T _ I N V E R S E
 *
 * The matrix pointed at by "input" is inverted and stored in the area
 * pointed at by "output".
 *
 * Invert a 4-by-4 matrix using Algorithm 120 from ACM.  This is a
 * modified Gauss-Jordan algorithm. Note: Inversion is done in place,
 * with 3 work vectors.
 *
 * @return 1 if OK.
 * @return 0 if matrix is singular.
 */
BN_EXPORT extern int bn_mat_inverse(mat_t output,
				    const mat_t input);

/**
 * B N _ V T O H _ M O V E
 *
 * Takes a pointer to a [x, y, z] vector, and a pointer to space for a
 * homogeneous vector [x, y, z, w], and builds [x, y, z, 1].
 */
BN_EXPORT extern void bn_vtoh_move(vect_t h,
				   const vect_t v);

/**
 * B N _ H T O V _ M O V E
 *
 * Takes a pointer to [x, y, z, w], and converts it to an ordinary
 * vector [x/w, y/w, z/w].  Optimization for the case of w==1 is
 * performed.
 *
 * FIXME: make tolerance configurable
 */
BN_EXPORT extern void bn_htov_move(vect_t v,
				   const vect_t h);

/**
 * B N _ M A T _ T R N
 */
BN_EXPORT extern void bn_mat_trn(mat_t om,
				 const mat_t im);

/**
 * B N _ M A T _ A E
 *
 * Compute a 4x4 rotation matrix given Azimuth and Elevation.
 *
 * Azimuth is +X, Elevation is +Z, both in degrees.
 *
 * Formula due to Doug Gwyn, BRL.
 */
BN_EXPORT extern void bn_mat_ae(mat_t m,
				double azimuth,
				double elev);

/**
 * B N _ A E _ V E C
 *
 * Find the azimuth and elevation angles that correspond to the
 * direction (not including twist) given by a direction vector.
 */
BN_EXPORT extern void bn_ae_vec(fastf_t *azp,
				fastf_t *elp,
				const vect_t v);

/**
 * B N _ A E T _ V E C
 *
 * Find the azimuth, elevation, and twist from two vectors.  Vec_ae is
 * in the direction of view (+z in mged view) and vec_twist points to
 * the viewers right (+x in mged view).  Accuracy (degrees) is used to
 * stabilize flutter between equivalent extremes of atan2(), and to
 * snap twist to zero when elevation is near +/- 90
 */
BN_EXPORT extern void bn_aet_vec(fastf_t *az,
				 fastf_t *el,
				 fastf_t *twist,
				 vect_t vec_ae,
				 vect_t vec_twist,
				 fastf_t accuracy);

/**
 * B N _ V E C _ A E
 *
 * Find a unit vector from the origin given azimuth and elevation.
 */
BN_EXPORT extern void bn_vec_ae(vect_t vec,
				fastf_t az,
				fastf_t el);

/**
 * B N _ V E C _ A E D
 *
 * Find a vector from the origin given azimuth, elevation, and distance.
 */
BN_EXPORT extern void bn_vec_aed(vect_t vec,
				 fastf_t az,
				 fastf_t el,
				 fastf_t dist);

/**
 * B N _ M A T _ A N G L E S
 *
 * This routine builds a Homogeneous rotation matrix, given alpha,
 * beta, and gamma as angles of rotation, in degrees.
 *
 * Alpha is angle of rotation about the X axis, and is done third.
 * Beta is angle of rotation about the Y axis, and is done second.
 * Gamma is angle of rotation about Z axis, and is done first.
 *
 * FIXME: make tolerance configurable
 */
BN_EXPORT extern void bn_mat_angles(mat_t mat,
				    double alpha,
				    double beta, double ggamma);

/**
 * B N _ M A T _ A N G L E S _ R A D
 *
 * This routine builds a Homogeneous rotation matrix, given alpha,
 * beta, and gamma as angles of rotation, in radians.
 *
 * Alpha is angle of rotation about the X axis, and is done third.
 * Beta is angle of rotation about the Y axis, and is done second.
 * Gamma is angle of rotation about Z axis, and is done first.
 *
 * FIXME: make tolerance configurable
 */
BN_EXPORT extern void bn_mat_angles_rad(mat_t mat,
					double alpha,
					double beta,
					double ggamma);

/**
 * B N _ E I G E N 2 X 2
 *
 * Find the eigenvalues and eigenvectors of a symmetric 2x2 matrix.
 * (a b)
 * (b c)
 *
 * The eigenvalue with the smallest absolute value is returned in
 * val1, with its eigenvector in vec1.
 */
BN_EXPORT extern void bn_eigen2x2(fastf_t *val1,
				  fastf_t *val2,
				  vect_t vec1,
				  vect_t vec2,
				  fastf_t a,
				  fastf_t b,
				  fastf_t c);

/**
 * B N _ V E C _ P E R P
 *
 * Given a vector, create another vector which is perpendicular to it.
 * The output vector will have unit length only if the input vector
 * did.
 *
 * FIXME: make tolerance configurable
 */
BN_EXPORT extern void bn_vec_perp(vect_t new_vec,
				  const vect_t old_vec);

/**
 * B N _ M A T _ F R O M T O
 *
 * Given two vectors, compute a rotation matrix that will transform
 * space by the angle between the two.  There are many candidate
 * matrices.
 *
 * The input 'from' and 'to' vectors need not be unit length.
 * MAT4X3VEC(to, m, from) is the identity that is created.
 *
 */
BN_EXPORT extern void bn_mat_fromto(mat_t m,
				    const fastf_t *from,
				    const fastf_t *to,
				    const struct bn_tol *tol);

/**
 * B N _ M A T _ X R O T
 *
 * Given the sin and cos of an X rotation angle, produce the rotation
 * matrix.
 */
BN_EXPORT extern void bn_mat_xrot(mat_t m,
				  double sinx,
				  double cosx);

/**
 * B N _ M A T _ Y R O T
 *
 * Given the sin and cos of a Y rotation angle, produce the rotation
 * matrix.
 */
BN_EXPORT extern void bn_mat_yrot(mat_t m,
				  double siny,
				  double cosy);

/**
 * B N _ M A T _ Z R O T
 *
 * Given the sin and cos of a Z rotation angle, produce the rotation
 * matrix.
 */
BN_EXPORT extern void bn_mat_zrot(mat_t m,
				  double sinz,
				  double cosz);

/**
 * B N _ M A T _ L O O K A T
 *
 * Given a direction vector D of unit length, product a matrix which
 * rotates that vector D onto the -Z axis.  This matrix will be
 * suitable for use as a "model2view" matrix.
 *
 * XXX This routine will fail if the vector is already more or less
 * aligned with the Z axis.
 *
 * This is done in several steps.
 *
 @code
 1) Rotate D about Z to match +X axis.  Azimuth adjustment.
 2) Rotate D about Y to match -Y axis.  Elevation adjustment.
 3) Rotate D about Z to make projection of X axis again point
 in the +X direction.  Twist adjustment.
 4) Optionally, flip sign on Y axis if original Z becomes inverted.
 This can be nice for static frames, but is astonishing when
 used in animation.
 @endcode
*/
BN_EXPORT extern void bn_mat_lookat(mat_t rot,
				    const vect_t dir,
				    int yflip);

/**
 * B N _ V E C _ O R T H O
 *
 * Given a vector, create another vector which is perpendicular to it,
 * and with unit length.  This algorithm taken from Gift's arvec.f; a
 * faster algorithm may be possible.
 *
 * FIXME: make tolerance configurable
 */
BN_EXPORT extern void bn_vec_ortho(vect_t out,
				   const vect_t in);

/**
 * B N _ M A T _ S C A L E _ A B O U T _ P T
 *
 * Build a matrix to scale uniformly around a given point.
 *
 * @return -1 if scale is too small.
 * @return  0 if OK.
 *
 * FIXME: make tolerance configurable
 */
BN_EXPORT extern int bn_mat_scale_about_pt(mat_t mat,
					   const point_t pt,
					   const double scale);

/**
 * B N _ M A T _ X F O R M _ A B O U T _ P T
 *
 * Build a matrix to apply arbitrary 4x4 transformation around a given
 * point.
 */
BN_EXPORT extern void bn_mat_xform_about_pt(mat_t mat,
					    const mat_t xform,
					    const point_t pt);

/**
 * B N _ M A T _ I S _ E Q U A L
 *
 * @return 0 When matrices are not equal
 * @return 1 When matrices are equal
 */
BN_EXPORT extern int bn_mat_is_equal(const mat_t a,
				     const mat_t b,
				     const struct bn_tol *tol);

/**
 * B N _ M A T _ I S _ I D E N T I T Y
 *
 * This routine is intended for detecting identity matrices read in
 * from ascii or binary files, where the numbers are pure ones or
 * zeros.  This routine is *not* intended for tolerance-based
 * "near-zero" comparisons; as such, it shouldn't be used on matrices
 * which are the result of calculation.
 *
 *
 * @return 0 non-identity
 * @return 1 a perfect identity matrix
 */
BN_EXPORT extern int bn_mat_is_identity(const mat_t m);

/**
 * B N _ M A T _ A R B _ R O T
 *
 * Construct a transformation matrix for rotation about an arbitrary
 * axis.  The axis is defined by a point (pt) and a unit direction
 * vector (dir).  The angle of rotation is "ang"
 *
 * FIXME: make tolerance configurable
 */
BN_EXPORT extern void bn_mat_arb_rot(mat_t m,
				     const point_t pt,
				     const vect_t dir,
				     const fastf_t ang);

/**
 * B N _ M A T _ D U P
 *
 * Return a pointer to a copy of the matrix in dynamically allocated
 * memory.
 */
BN_EXPORT extern matp_t bn_mat_dup(const mat_t in);

/**
 * B N _ M A T _ C K
 *
 * Check to ensure that a rotation matrix preserves axis
 * perpendicularity.  Note that not all matrices are rotation
 * matrices.
 *
 *
 * @return -1 FAIL
 * @return  0 OK
 */
BN_EXPORT extern int bn_mat_ck(const char *title,
			       const mat_t m);

/**
 * B N _ M A T _ D E T 3
 *
 * Calculates the determinant of the 3X3 "rotation" part of the passed
 * matrix.
 */
BN_EXPORT extern fastf_t bn_mat_det3(const mat_t m);

/**
 * B N _ M A T _ D E T E R M I N A N T
 *
 * Calculates the determinant of the 4X4 matrix
 */
BN_EXPORT extern fastf_t bn_mat_determinant(const mat_t m);

/**
 * B N _ M A T _ I S _ N O N _ U N I F
 *
 * FIXME: make tolerance configurable
 */
BN_EXPORT extern int bn_mat_is_non_unif(const mat_t m);

/**
 * B N _ W R T _ P O I N T _ D I R E C
 *
 * Given a model-space transformation matrix "change",
 * return a matrix which applies the change with-respect-to
 * given "point" and "direc".
 */
BN_EXPORT extern void bn_wrt_point_direc(mat_t out,
					 const mat_t change,
					 const mat_t in,
					 const point_t point,
					 const vect_t direc,
					 const struct bn_tol *tol);

/** @} */


/*----------------------------------------------------------------------*/


/* msr.c */

/** @addtogroup msr */
/** @{ */

/** @brief
 *
 * Minimal Standard RANdom number generator
 *
 * @par From:
 *	Stephen K. Park and Keith W. Miller
 * @n	"Random number generators: good ones are hard to find"
 * @n	CACM vol 31 no 10, Oct 88
 *
 */

/*
 * Define data structures and constants for the "MSR" random number
 * package.
 *
 * Also define a set of macros to access the random number tables and
 * to limit the area/volume that a set of random numbers inhabit.
 */

struct bn_unif {
    uint32_t magic;
    long msr_seed;
    int msr_double_ptr;
    double *msr_doubles;
    int msr_long_ptr;
    long *msr_longs;
};

#define BN_CK_UNIF(_p)  BU_CKMAG(_p, BN_UNIF_MAGIC, "bn_unif")
#define BN_CK_GAUSS(_p) BU_CKMAG(_p, BN_GAUSS_MAGIC, "bn_gauss")


/**
 * NOTE!!! The order of msr_gauss and msr_unif MUST match in the first
 * three entries as msr_gauss is passed as a msr_unif in
 * msr_gauss_fill.
 */
struct bn_gauss {
    uint32_t magic;
    long msr_gauss_seed;
    int msr_gauss_dbl_ptr;
    double *msr_gauss_doubles;
    int msr_gauss_ptr;
    double *msr_gausses;
};


/*	bn_unif_init	Initialize a random number structure.
 *
 * @par Entry
 *	setseed	seed to use
 *	method  method to use to generate numbers;
 *
 * @par Exit
 *	returns	a pointer to a bn_unif structure.
 *	returns 0 on error.
 *
 * @par Uses
 *	None.
 *
 * @par Calls
 *	bu_malloc
 *
 * @par Method @code
 *	malloc up a structure with pointers to the numbers
 *	get space for the integer table.
 *	get space for the floating table.
 *	set pointer counters
 *	set seed if one was given and setseed != 1
 @endcode
 *
 */
BN_EXPORT extern struct bn_unif *bn_unif_init(long setseed,
					      int method);

/*
 * bn_unif_free	  free random number table
 *
 */
BN_EXPORT extern void bn_unif_free(struct bn_unif *p);

/*	bn_unif_long_fill	fill a random number table.
 *
 * Use the msrad algorithm to fill a random number table
 * with values from 1 to 2^31-1.  These numbers can (and are) extracted from
 * the random number table via high speed macros and bn_unif_long_fill called
 * when the table is exhausted.
 *
 * @par Entry
 *	p	pointer to a bn_unif structure.
 *
 * @par Exit
 *	if (!p) returns 1 else returns a value between 1 and 2^31-1
 *
 * @par Calls
 *	None.  msran is inlined for speed reasons.
 *
 * @par Uses
 *	None.
 *
 * @par Method @code
 *	if (!p) return 1;
 *	if p->msr_longs != NULL
 *		msr_longs is reloaded with random numbers;
 *		msr_long_ptr is set to BN_MSR_MAXTBL
 *	endif
 *	msr_seed is updated.
 @endcode
*/
BN_EXPORT extern long bn_unif_long_fill(struct bn_unif *p);

/*	bn_unif_double_fill	fill a random number table.
 *
 * Use the msrad algorithm to fill a random number table
 * with values from -0.5 to 0.5.  These numbers can (and are) extracted from
 * the random number table via high speed macros and bn_unif_double_fill
 * called when the table is exhausted.
 *
 * @par Entry
 *	p	pointer to a bn_unif structure.
 *
 * @par Exit
 *	if (!p) returns 0.0 else returns a value between -0.5 and 0.5
 *
 * @par Calls
 *	None.  msran is inlined for speed reasons.
 *
 * @par Uses
 *	None.
 *
 * @par Method @code
 *	if (!p) return (0.0)
 *	if p->msr_longs != NULL
 *		msr_longs is reloaded with random numbers;
 *		msr_long_ptr is set to BN_MSR_MAXTBL
 *	endif
 *	msr_seed is updated.
 @endcode
*/
BN_EXPORT extern double bn_unif_double_fill(struct bn_unif *p);

/*	bn_gauss_init	Initialize a random number struct for gaussian
 *	numbers.
 *
 * @par Entry
 *	setseed		Seed to use.
 *	method		method to use to generate numbers (not used)
 *
 * @par Exit
 *	Returns a pointer toa bn_msr_gauss structure.
 *	returns 0 on error.
 *
 * @par Calls
 *	bu_malloc
 *
 * @par Uses
 *	None.
 *
 * @par Method @code
 *	malloc up a structure
 *	get table space
 *	set seed and pointer.
 *	if setseed != 0 then seed = setseed
 @endcode
*/
BN_EXPORT extern struct bn_gauss *bn_gauss_init(long setseed,
						int method);

/*
 * bn_gauss_free	free random number table
 *
 */
BN_EXPORT extern void bn_gauss_free(struct bn_gauss *p);

/*	bn_gauss_fill	fill a random number table.
 *
 * Use the msrad algorithm to fill a random number table.
 * hese numbers can (and are) extracted from
 * the random number table via high speed macros and bn_msr_gauss_fill
 * called when the table is exhausted.
 *
 * @par Entry
 *	p	pointer to a bn_msr_gauss structure.
 *
 * @par Exit
 *	if (!p) returns 0.0 else returns a value with a mean of 0 and
 *	    a variance of 1.0.
 *
 * @par Calls
 *	BN_UNIF_CIRCLE to get to uniform random number whos radius is
 *	<= 1.0. I.e. sqrt(v1*v1 + v2*v2) <= 1.0
 *	BN_UNIF_CIRCLE is a macro which can call bn_unif_double_fill.
 *
 * @par Uses
 *	None.
 *
 * @par Method @code
 *	if (!p) return (0.0)
 *	if p->msr_longs != NULL
 *		msr_longs is reloaded with random numbers;
 *		msr_long_ptr is set to BN_MSR_MAXTBL
 *	endif
 *	msr_seed is updated.
 @endcode
*/
BN_EXPORT extern double bn_gauss_fill(struct bn_gauss *p);

#define BN_UNIF_LONG(_p)	\
    (((_p)->msr_long_ptr) ? \
     (_p)->msr_longs[--(_p)->msr_long_ptr] : \
     bn_unif_long_fill(_p))
#define BN_UNIF_DOUBLE(_p)	\
    (((_p)->msr_double_ptr) ? \
     (_p)->msr_doubles[--(_p)->msr_double_ptr] : \
     bn_unif_double_fill(_p))

#define BN_UNIF_CIRCLE(_p, _x, _y, _r) { \
	do { \
	    (_x) = 2.0*BN_UNIF_DOUBLE((_p)); \
	    (_y) = 2.0*BN_UNIF_DOUBLE((_p)); \
	    (_r) = (_x)*(_x)+(_y)*(_y); \
	} while ((_r) >= 1.0);  }

#define BN_UNIF_SPHERE(_p, _x, _y, _z, _r) { \
	do { \
	    (_x) = 2.0*BN_UNIF_DOUBLE(_p); \
	    (_y) = 2.0*BN_UNIF_DOUBLE(_p); \
	    (_z) = 2.0*BN_UNIF_DOUBLE(_p); \
	    (_r) = (_x)*(_x)+(_y)*(_y)+(_z)*(_z);\
	} while ((_r) >= 1.0) }

#define BN_GAUSS_DOUBLE(_p)	\
    (((_p)->msr_gauss_ptr) ? \
     (_p)->msr_gausses[--(_p)->msr_gauss_ptr] : \
     bn_gauss_fill(_p))

/** @} */


/*----------------------------------------------------------------------*/


/* noise.c */

/** @addtogroup noise */
/** @{ */

/** @brief
 *
 * These noise functions provide mostly random noise at the integer
 * lattice points.  The functions should be evaluated at non-integer
 * locations for their nature to be realized.
 *
 * Contains contributed code from:
 * F. Kenton Musgrave
 * Robert Skinner
 *
 */

/*
 * fractal noise support
 */

BN_EXPORT extern void bn_noise_init();

/**
 *@brief
 * Robert Skinner's Perlin-style "Noise" function
 *
 * Results are in the range [-0.5 .. 0.5].  Unlike many
 * implementations, this function provides random noise at the integer
 * lattice values.  However this produces much poorer quality and
 * should be avoided if possible.
 *
 * The power distribution of the result has no particular shape,
 * though it isn't as flat as the literature would have one believe.
 */
BN_EXPORT extern double bn_noise_perlin(point_t pt);

/* FIXME: Why isn't the result listed first? */

/**
 * Vector-valued "Noise"
 */
BN_EXPORT extern void bn_noise_vec(point_t point,
				   point_t result);

/**
 * @brief
 * Procedural fBm evaluated at "point"; returns value stored in
 * "value".
 *
 * @param point          location to sample noise
 * @param ``h_val''      fractal increment parameter
 * @param ``lacunarity'' gap between successive frequencies
 * @param ``octaves''  	 number of frequencies in the fBm
 *
 * The spectral properties of the result are in the APPROXIMATE range
 * [-1..1] Depending upon the number of octaves computed, this range
 * may be exceeded.  Applications should clamp or scale the result to
 * their needs.  The results have a more-or-less gaussian
 * distribution.  Typical results for 1M samples include:
 *
 * @li Min -1.15246
 * @li Max  1.23146
 * @li Mean -0.0138744
 * @li s.d.  0.306642
 * @li Var 0.0940295
 *
 * The function call pow() is relatively expensive.  Therfore, this
 * function pre-computes and saves the spectral weights in a table for
 * re-use in successive invocations.
 */
BN_EXPORT extern double bn_noise_fbm(point_t point,
				     double h_val,
				     double lacunarity,
				     double octaves);

/**
 * @brief
 * Procedural turbulence evaluated at "point";
 *
 * @return turbulence value for point
 *
 * @param point          location to sample noise at
 * @param ``h_val''      fractal increment parameter
 * @param ``lacunarity'' gap between successive frequencies
 * @param ``octaves''    number of frequencies in the fBm
 *
 * The result is characterized by sharp, narrow trenches in low values
 * and a more fbm-like quality in the mid-high values.  Values are in
 * the APPROXIMATE range [0 .. 1] depending upon the number of octaves
 * evaluated.  Typical results:
 @code
 * Min  0.00857137
 * Max  1.26712
 * Mean 0.395122
 * s.d. 0.174796
 * Var  0.0305536
 @endcode
 * The function call pow() is relatively expensive.  Therfore, this
 * function pre-computes and saves the spectral weights in a table for
 * re-use in successive invocations.
 */
BN_EXPORT extern double bn_noise_turb(point_t point,
				      double h_val,
				      double lacunarity,
				      double octaves);

/**
 *
 * From "Texturing and Modeling, A Procedural Approach" 2nd ed
 */
BN_EXPORT extern double bn_noise_mf(point_t point,
				    double h_val,
				    double lacunarity,
				    double octaves,
				    double offset);

/**
 *@brief
 * A ridged noise pattern
 *
 * From "Texturing and Modeling, A Procedural Approach" 2nd ed p338
 */
BN_EXPORT extern double bn_noise_ridged(point_t point,
					double h_val,
					double lacunarity,
					double octaves,
					double offset);

/*----------------------------------------------------------------------*/


/* obr.c */
/*
 * Routines for the computation of oriented bounding rectangles 2D and 3D
 */


/**
 *@brief
 * Uses the Rotating Calipers algorithm to find the
 * minimum oriented bounding rectangle for a set of 2D
 * points.  Returns 0 on success.
 *
 * The box will be described by a center point and 2
 * vectors:
 *
 * \verbatim
 * ----------------------------
 * |            ^             |
 * |            |             |
 * |         v  |             |
 * |            |             |
 * |            *------------>|
 * |         center     u     |
 * |                          |
 * |                          |
 * ----------------------------
 * \endverbatim
 *
 * Note that the box is oriented, and thus not necessarily axis
 * aligned (u and v are perpendicular, but not necessarily parallel
 * with the coordinate space V=0 and U=0 axis vectors.)
 *
 * @param[out] center	center of oriented bounding rectangle
 * @param[out] u	vector in the direction of obr x with
 * 			vector length of 0.5 * obr length
 * @param[out] v	vector in the obr y direction with vector
 * 			length of 0.5 * obr width
 * @param points_2d	array of 2D points
 * @param pnt_cnt	number of points in pnts array
 */
BN_EXPORT extern int bn_2d_obr(point2d_t *center,
			       vect2d_t *u,
			       vect2d_t *v,
			       const point2d_t *points_2d,
			       int pnt_cnt);

/**
 *@brief
 * Uses the Rotating Calipers algorithm to find the
 * minimum oriented bounding rectangle for a set of coplanar 3D
 * points.  Returns 0 on success.
 *
 * @param[out] center	center of oriented bounding rectangle
 * @param[out] v1	vector in the direction of obr x with
 * 			vector length of 0.5 * obr length
 * @param[out] v2	vector in the obr y direction with vector
 * 			length of 0.5 * obr width
 * @param points_3d	array of coplanar 3D points
 * @param pnt_cnt	number of points in pnts array
 */
BN_EXPORT extern int bn_3d_coplanar_obr(point_t *center,
			       vect_t *v1,
			       vect_t *v2,
			       const point_t *points_3d,
			       int pnt_cnt);



/*----------------------------------------------------------------------*/


/* plane.c */
/*
 * Plane/line/point calculations
 */

/** @brief
 *
 * Some useful routines for dealing with planes and lines.
 *
 */


/** B N _ D I S T S Q _ L I N E 3 _ L I N E 3
 *@brief
 * Calculate the square of the distance of closest approach for two
 * lines.
 *
 * The lines are specified as a point and a vector each.  The vectors
 * need not be unit length.  P and d define one line; Q and e define
 * the other.
 *
 * @return 0 - normal return
 * @return 1 - lines are parallel, dist[0] is set to 0.0
 *
 * Output values:
 * dist[0] is the parametric distance along the first line P + dist[0] * d of the PCA
 * dist[1] is the parametric distance along the second line Q + dist[1] * e of the PCA
 * dist[3] is the square of the distance between the points of closest approach
 * pt1 is the point of closest approach on the first line
 * pt2 is the point of closest approach on the second line
 *
 * This algorithm is based on expressing the distance squared, taking
 * partials with respect to the two unknown parameters (dist[0] and
 * dist[1]), setting the two partials equal to 0, and solving the two
 * simultaneous equations
 */
BN_EXPORT extern int bn_distsq_line3_line3(fastf_t dist[3],
					   point_t P,
					   vect_t d,
					   point_t Q,
					   vect_t e,
					   point_t pt1,
					   point_t pt2);

/**
 * Find the distance from a point P to a line described by the
 * endpoint A and direction dir, and the point of closest approach
 * (PCA).
 *
 @code
			P
		       *
		      /.
		     / .
		    /  .
		   /   . (dist)
		  /    .
		 /     .
		*------*-------->
		A      PCA    dir
 @endcode
 * There are three distinct cases, with these return codes -
 *   0 => P is within tolerance of point A.  *dist = 0, pca=A.
 *   1 => P is within tolerance of line.  *dist = 0, pca=computed.
 *   2 => P is "above/below" line.  *dist=|PCA-P|, pca=computed.
 *
 * TODO: For efficiency, a version of this routine that provides the
 * distance squared would be faster.
 */
BN_EXPORT extern int bn_dist_pt3_line3(fastf_t *dist,
				       point_t pca,
				       const point_t a,
				       const point_t p,
				       const vect_t dir,
				       const struct bn_tol *tol);

/**
 * calculate intersection or closest approach of a line and a line
 * segment.
 *
 * returns:
 *	-2 -> line and line segment are parallel and collinear.
 *	-1 -> line and line segment are parallel, not collinear.
 *	 0 -> intersection between points a and b.
 *	 1 -> intersection outside a and b.
 *	 2 -> closest approach is between a and b.
 *	 3 -> closest approach is outside a and b.
 *
 * dist[0] is actual distance from p in d direction to
 * closest portion of segment.
 * dist[1] is ratio of distance from a to b (0.0 at a, and 1.0 at b),
 * dist[1] may be less than 0 or greater than 1.
 * For return values less than 0, closest approach is defined as
 * closest to point p.
 * Direction vector, d, must be unit length.
 *
 */
BN_EXPORT extern int bn_dist_line3_lseg3(fastf_t *dist,
					 const fastf_t *p,
					 const fastf_t *d,
					 const fastf_t *a,
					 const fastf_t *b,
					 const struct bn_tol *tol);

/**
 * Calculate closest approach of two lines
 *
 * returns:
 *	-2 -> lines are parallel and do not intersect
 *	-1 -> lines are parallel and collinear
 *	 0 -> lines intersect
 *	 1 -> lines do not intersect
 *
 * For return values less than zero, dist is not set.  For return
 * values of 0 or 1, dist[0] is the distance from p1 in the d1
 * direction to the point of closest approach for that line.  Similar
 * for the second line.  d1 and d2 must be unit direction vectors.
 *
 * XXX How is this different from bn_isect_line3_line3() ?
 * XXX Why are the calling sequences just slightly different?
 * XXX Can we pick the better one, and get rid of the other one?
 * XXX If not, can we document how they differ?
 */
BN_EXPORT extern int bn_dist_line3_line3(fastf_t dist[2],
					 const point_t p1,
					 const point_t p2,
					 const vect_t d1,
					 const vect_t d2,
					 const struct bn_tol *tol);

/**
 * B N _ D I S T _ P T 3 _ L S E G 3
 *@brief
 * Find the distance from a point P to a line segment described by the
 * two endpoints A and B, and the point of closest approach (PCA).
 @verbatim
 *
 *			P
 *		       *
 *		      /.
 *		     / .
 *		    /  .
 *		   /   . (dist)
 *		  /    .
 *		 /     .
 *		*------*--------*
 *		A      PCA	B
 @endverbatim
 *
 * @return 0	P is within tolerance of lseg AB.  *dist isn't 0: (SPECIAL!!!)
 *		  *dist = parametric dist = |PCA-A| / |B-A|.  pca=computed.
 * @return 1	P is within tolerance of point A.  *dist = 0, pca=A.
 * @return 2	P is within tolerance of point B.  *dist = 0, pca=B.
 * @return 3	P is to the "left" of point A.  *dist=|P-A|, pca=A.
 * @return 4	P is to the "right" of point B.  *dist=|P-B|, pca=B.
 * @return 5	P is "above/below" lseg AB.  *dist=|PCA-P|, pca=computed.
 *
 * This routine was formerly called bn_dist_pt_lseg().
 *
 * XXX For efficiency, a version of this routine that provides the
 * XXX distance squared would be faster.
 */
BN_EXPORT extern int bn_dist_pt3_lseg3(fastf_t *dist,
				       point_t pca,
				       const point_t a,
				       const point_t b,
				       const point_t p,
				       const struct bn_tol *tol);

/**
 * PRIVATE: This is a new API and should be considered unpublished.
 *
 * B N _ D I S T S Q _ P T 3 _ L S E G 3 _ v 2
 *
 * Find the square of the distance from a point P to a line segment described
 * by the two endpoints A and B.
 *
 *			P
 *		       *
 *		      /.
 *		     / .
 *		    /  .
 *		   /   . (dist)
 *		  /    .
 *		 /     .
 *		*------*--------*
 *		A      PCA	B
 *
 * There are six distinct cases, with these return codes -
 * Return code precedence: 1, 2, 0, 3, 4, 5
 *
 *	0	P is within tolerance of lseg AB.  *dist =  0.
 *	1	P is within tolerance of point A.  *dist = 0.
 *	2	P is within tolerance of point B.  *dist = 0.
 *	3	PCA is within tolerance of A. *dist = |P-A|**2.
 *	4	PCA is within tolerance of B. *dist = |P-B|**2.
 *	5	P is "above/below" lseg AB.   *dist=|PCA-P|**2.
 *
 * If both P and PCA and not within tolerance of lseg AB use
 * these return codes -
 *
 *	3	PCA is to the left of A.  *dist = |P-A|**2.
 *	4	PCA is to the right of B. *dist = |P-B|**2.
 *
 * This function is a test version of "bn_distsq_pt3_lseg3".
 *
 */
BN_EXPORT extern int bn_distsq_pt3_lseg3_v2(fastf_t *distsq,
					    const fastf_t *a,
					    const fastf_t *b,
					    const fastf_t *p,
					    const struct bn_tol *tol);

/**
 * B N _ 3 P T S _ C O L L I N E A R
 * @brief
 * Check to see if three points are collinear.
 *
 * The algorithm is designed to work properly regardless of the order
 * in which the points are provided.
 *
 * @return 1	If 3 points are collinear
 * @return 0	If they are not
 */
BN_EXPORT extern int bn_3pts_collinear(point_t a,
				       point_t b,
				       point_t c,
				       const struct bn_tol *tol);

/**
 * B N _ P T 3 _ P T 3 _ E Q U A L
 *
 * @return 1	if the two points are equal, within the tolerance
 * @return 0	if the two points are not "the same"
 */
BN_EXPORT extern int bn_pt3_pt3_equal(const point_t a,
				      const point_t b,
				      const struct bn_tol *tol);

/**
 * B N _ D I S T _ P T 2 _ L S E G 2
 *@brief
 * Find the distance from a point P to a line segment described by the
 * two endpoints A and B, and the point of closest approach (PCA).
 @verbatim
 *			P
 *		       *
 *		      /.
 *		     / .
 *		    /  .
 *		   /   . (dist)
 *		  /    .
 *		 /     .
 *		*------*--------*
 *		A      PCA	B
 @endverbatim
 * There are six distinct cases, with these return codes -
 * @return 0	P is within tolerance of lseg AB.  *dist isn't 0: (SPECIAL!!!)
 *		  *dist = parametric dist = |PCA-A| / |B-A|.  pca=computed.
 * @return 1	P is within tolerance of point A.  *dist = 0, pca=A.
 * @return 2	P is within tolerance of point B.  *dist = 0, pca=B.
 * @return 3	P is to the "left" of point A.  *dist=|P-A|**2, pca=A.
 * @return 4	P is to the "right" of point B.  *dist=|P-B|**2, pca=B.
 * @return 5	P is "above/below" lseg AB.  *dist=|PCA-P|**2, pca=computed.
 *
 *
 * Patterned after bn_dist_pt3_lseg3().
 */
BN_EXPORT extern int bn_dist_pt2_lseg2(fastf_t *dist_sq,
				       fastf_t pca[2],
				       const point_t a,
				       const point_t b,
				       const point_t p,
				       const struct bn_tol *tol);

/**
 * B N _ I S E C T _ L S E G 3 _ L S E G 3
 *@brief
 * Intersect two 3D line segments, defined by two points and two
 * vectors.  The vectors are unlikely to be unit length.
 *
 *
 * @return -3	missed
 * @return -2	missed (line segments are parallel)
 * @return -1	missed (colinear and non-overlapping)
 * @return 0	hit (line segments colinear and overlapping)
 * @return 1	hit (normal intersection)
 *
 * @param[out] dist
 *	The value at dist[] is set to the parametric distance of the
 *	intercept dist[0] is parameter along p, range 0 to 1, to
 *	intercept.  dist[1] is parameter along q, range 0 to 1, to
 *	intercept.  If within distance tolerance of the endpoints,
 *	these will be exactly 0.0 or 1.0, to ease the job of caller.
 *
 *      CLARIFICATION: This function 'bn_isect_lseg3_lseg3'
 *      returns distance values scaled where an intersect at the start
 *      point of the line segment (within tol->dist) results in 0.0
 *      and when the intersect is at the end point of the line
 *      segment (within tol->dist), the result is 1.0.  Intersects
 *      before the start point return a negative distance.  Intersects
 *      after the end point result in a return value > 1.0.
 *
 * Special note: when return code is "0" for co-linearity, dist[1] has
 * an alternate interpretation: it's the parameter along p (not q)
 * which takes you from point p to the point (q + qdir), i.e., it's
 * the endpoint of the q linesegment, since in this case there may be
 * *two* intersections, if q is contained within span p to (p + pdir).
 *
 * @param p	point 1
 * @param pdir	direction-1
 * @param q	point 2
 * @param qdir	direction-2
 * @param tol	tolerance values
 */
BN_EXPORT extern int bn_isect_lseg3_lseg3(fastf_t *dist,
					      const point_t p, const vect_t pdir,
					      const point_t q, const vect_t qdir,
					      const struct bn_tol *tol);

BN_EXPORT extern int bn_lseg3_lseg3_parallel(const point_t sg1pt1, const point_t sg1pt2,
					     const point_t sg2pt1, const point_t sg2pt2,
					     const struct bn_tol *tol);

/**
 * B N _ I S E C T _ L I N E 3 _ L I N E 3
 *
 * Intersect two line segments, each in given in parametric form:
 *
 * X = p0 + pdist * pdir_i   (i.e. line p0->p1)
 * and
 * X = q0 + qdist * qdir_i   (i.e. line q0->q1)
 *
 * The input vectors 'pdir_i' and 'qdir_i' must NOT be unit vectors
 * for this function to work correctly. The magnitude of the direction
 * vectors indicates line segment length.
 *
 * The 'pdist' and 'qdist' values returned from this function are the
 * actual distance to the intersect (i.e. not scaled). Distances may
 * be negative, see below.
 *
 * @return -2	no intersection, lines are parallel.
 * @return -1	no intersection
 * @return 0	lines are co-linear (pdist and qdist returned) (see below)
 * @return 1	intersection found  (pdist and qdist returned) (see below)
 *
 * @param	p0	point 1
 * @param	pdir_i 	direction 1
 * @param	q0	point 2
 * @param	qdir_i 	direction 2
 * @param tol	tolerance values
 * @param[out]	pdist, qdist (distances to intersection) (see below)
 *
 *		When return = 1, pdist is the distance along line p0->p1 to the
 *		intersect with line q0->q1. If the intersect is along p0->p1 but
 *		in the opposite direction of vector pdir_i (i.e. occurring before
 *		p0 on line p0->p1) then the distance will be negative. The value
 *		if qdist is the same as pdist except it is the distance along line
 *		q0->q1 to the intersect with line p0->p1.
 *
 *		When return code = 0 for co-linearity, pdist and qdist have a
 *		different meaning. pdist is the distance from point p0 to point q0
 *		and qdist is the distance from point p0 to point q1. If point q0
 *		occurs before point p0 on line segment p0->p1 then pdist will be
 *		negative. The same occurs for the distance to point q1.
 */
BN_EXPORT extern int bn_isect_line3_line3(fastf_t *s, fastf_t *t,
					      const point_t p0,
					      const vect_t u,
					      const point_t q0,
					      const vect_t v,
					      const struct bn_tol *tol);

/**
 * B N _ 2 L I N E 3 _ C O L I N E A R
 * @brief
 * Returns non-zero if the 3 lines are colinear to within tol->dist
 * over the given distance range.
 *
 * Range should be at least one model diameter for most applications.
 * 1e5 might be OK for a default for "vehicle sized" models.
 *
 * The direction vectors do not need to be unit length.
 */
BN_EXPORT extern int bn_2line3_colinear(const point_t p1,
					const vect_t d1,
					const point_t p2,
					const vect_t d2,
					double range,
					const struct bn_tol *tol);

/**
 * B N _ I S E C T _ P T 2 _ L S E G 2
 * @brief
 * Intersect a point P with the line segment defined by two distinct
 * points A and B.
 *
 * @return -2	P on line AB but outside range of AB,
 *			dist = distance from A to P on line.
 * @return -1	P not on line of AB within tolerance
 * @return 1	P is at A
 * @return 2	P is at B
 * @return 3	P is on AB, dist = distance from A to P on line.
 @verbatim
 B *
 |
 P'*-tol-*P
 |    /  _
 dist  /   /|
 |  /   /
 | /   / AtoP
 |/   /
 A *   /

 tol = distance limit from line to pt P;
 dist = distance from A to P'
 @endverbatim
*/
BN_EXPORT extern int bn_isect_pt2_lseg2(fastf_t *dist,
					const point_t a,
					const point_t b,
					const point_t p,
					const struct bn_tol *tol);

/**
 * B N _ I S E C T _ L I N E 2 _ L S E G 2
 *@brief
 * Intersect a line in parametric form:
 *
 * X = P + t * D
 *
 * with a line segment defined by two distinct points A and B=(A+C).
 *
 * XXX probably should take point B, not vector C.  Sigh.
 *
 * @return -4	A and B are not distinct points
 * @return -3	Lines do not intersect
 * @return -2	Intersection exists, but outside segment, < A
 * @return -1	Intersection exists, but outside segment, > B
 * @return 0	Lines are co-linear (special meaning of dist[1])
 * @return 1	Intersection at vertex A
 * @return 2	Intersection at vertex B (A+C)
 * @return 3	Intersection between A and B
 *
 * Implicit Returns -
 * @param dist	When explicit return >= 0, t is the parameter that describes
 *		the intersection of the line and the line segment.
 *		The actual intersection coordinates can be found by
 *		solving P + t * D.  However, note that for return codes
 *		1 and 2 (intersection exactly at a vertex), it is
 *		strongly recommended that the original values passed in
 *		A or B are used instead of solving P + t * D, to prevent
 *		numeric error from creeping into the position of
 *		the endpoints.
 *
 * @param p	point of first line
 * @param d	direction of first line
 * @param a	point of second line
 * @param c	direction of second line
 * @param tol	tolerance values
 */
BN_EXPORT extern int bn_isect_line2_lseg2(fastf_t *dist,
					  const point_t p,
					  const vect_t d,
					  const point_t a,
					  const vect_t c,
					  const struct bn_tol *tol);

/**
 * B N _ I S E C T _ L S E G 2 _ L S E G 2
 *@brief
 * Intersect two 2D line segments, defined by two points and two
 * vectors.  The vectors are unlikely to be unit length.
 *
 * @return -2	missed (line segments are parallel)
 * @return -1	missed (colinear and non-overlapping)
 * @return 0	hit (line segments colinear and overlapping)
 * @return 1	hit (normal intersection)
 *
 * @param dist  The value at dist[] is set to the parametric distance of the
 *		intercept.
 *@n	dist[0] is parameter along p, range 0 to 1, to intercept.
 *@n	dist[1] is parameter along q, range 0 to 1, to intercept.
 *@n	If within distance tolerance of the endpoints, these will be
 *	exactly 0.0 or 1.0, to ease the job of caller.
 *
 * Special note: when return code is "0" for co-linearity, dist[1] has
 * an alternate interpretation: it's the parameter along p (not q)
 * which takes you from point p to the point (q + qdir), i.e., its
 * the endpoint of the q linesegment, since in this case there may be
 * *two* intersections, if q is contained within span p to (p + pdir).
 * And either may be -10 if the point is outside the span.
 *
 * @param p	point 1
 * @param pdir	direction1
 * @param q	point 2
 * @param qdir	direction2
 * @param tol	tolerance values
 */
BN_EXPORT extern int bn_isect_lseg2_lseg2(fastf_t *dist,
					  const point_t p,
					  const vect_t pdir,
					  const point_t q,
					  const vect_t qdir,
					  const struct bn_tol *tol);

/**
 * B N _ I S E C T _ L I N E 2 _ L I N E 2
 *
 * Intersect two lines, each in given in parametric form:
 @verbatim

 X = P + t * D
 and
 X = A + u * C

 @endverbatim
 *
 * While the parametric form is usually used to denote a ray (i.e.,
 * positive values of the parameter only), in this case the full line
 * is considered.
 *
 * The direction vectors C and D need not have unit length.
 *
 * @return -1	no intersection, lines are parallel.
 * @return 0	lines are co-linear
 *@n			dist[0] gives distance from P to A,
 *@n			dist[1] gives distance from P to (A+C) [not same as below]
 * @return 1	intersection found (t and u returned)
 *@n			dist[0] gives distance from P to isect,
 *@n			dist[1] gives distance from A to isect.
 *
 * @param dist When explicit return > 0, dist[0] and dist[1] are the
 * line parameters of the intersection point on the 2 rays.  The
 * actual intersection coordinates can be found by substituting either
 * of these into the original ray equations.
 *
 * @param p	point of first line
 * @param d	direction of first line
 * @param a	point of second line
 * @param c	direction of second line
 * @param tol	tolerance values
 *
 * Note that for lines which are very nearly parallel, but not quite
 * parallel enough to have the determinant go to "zero", the
 * intersection can turn up in surprising places.  (e.g. when
 * det=1e-15 and det1=5.5e-17, t=0.5)
 */
BN_EXPORT extern int bn_isect_line2_line2(fastf_t *dist,
					  const point_t p,
					  const vect_t d,
					  const point_t a,
					  const vect_t c,
					  const struct bn_tol *tol);

/**
 * B N _ D I S T _ P T 3 _ P T 3
 * @brief
 * Returns distance between two points.
 */
BN_EXPORT extern double bn_dist_pt3_pt3(const point_t a,
					const point_t b);

/**
 * B N _ 3 P T S _ D I S T I N C T
 *
 * Check to see if three points are all distinct, i.e., ensure that
 * there is at least sqrt(dist_tol_sq) distance between every pair of
 * points.
 *
 * @return 1 If all three points are distinct
 * @return 0 If two or more points are closer together than dist_tol_sq
 */
BN_EXPORT extern int bn_3pts_distinct(const point_t a,
				      const point_t b,
				      const point_t c,
				      const struct bn_tol *tol);

/**
 * B N _ N P T S _ D I S T I N C T
 *
 * Check to see if the points are all distinct, i.e., ensure that
 * there is at least sqrt(dist_tol_sq) distance between every pair of
 * points.
 *
 * @return 1 If all the points are distinct
 * @return 0 If two or more points are closer together than dist_tol_sq
 */
BN_EXPORT extern int bn_npts_distinct(const int npts,
				      const point_t *pts,
				      const struct bn_tol *tol);

/**
 * B N _ M K _ P L A N E _ 3 P T S
 *
 * Find the equation of a plane that contains three points.  Note that
 * normal vector created is expected to point out (see vmath.h), so
 * the vector from A to C had better be counter-clockwise (about the
 * point A) from the vector from A to B.  This follows the BRL-CAD
 * outward-pointing normal convention, and the right-hand rule for
 * cross products.
 *
 @verbatim
 *
 *			C
 *	                *
 *	                |\
 *	                | \
 *	   ^     N      |  \
 *	   |      \     |   \
 *	   |       \    |    \
 *	   |C-A     \   |     \
 *	   |         \  |      \
 *	   |          \ |       \
 *	               \|        \
 *	                *---------*
 *	                A         B
 *			   ----->
 *		            B-A
 @endverbatim
 *
 * If the points are given in the order A B C (e.g.,
 * *counter*-clockwise), then the outward pointing surface normal:
 *
 * N = (B-A) x (C-A).
 *
 *  @return 0	OK
 *  @return -1	Failure.  At least two of the points were not distinct,
 *		or all three were colinear.
 *
 * @param[out]	plane	The plane equation is stored here.
 * @param[in]	a	point 1
 * @param[in]	b	point 2
 * @param[in]	c	point 3
 * @param[in]	tol	Tolerance values for doing calculation
 */
BN_EXPORT extern int bn_mk_plane_3pts(plane_t plane,
				      const point_t a,
				      const point_t b,
				      const point_t c,
				      const struct bn_tol *tol);

/**
 * B N _ M K P O I N T _ 3 P L A N E S
 *@brief
 * Given the description of three planes, compute the point of intersection, if
 * any. The direction vectors of the planes need not be of unit length.
 *
 * Find the solution to a system of three equations in three unknowns:
 @verbatim
 * Px * Ax + Py * Ay + Pz * Az = -A3;
 * Px * Bx + Py * By + Pz * Bz = -B3;
 * Px * Cx + Py * Cy + Pz * Cz = -C3;
 *
 * OR
 *
 * [ Ax  Ay  Az ]   [ Px ]   [ -A3 ]
 * [ Bx  By  Bz ] * [ Py ] = [ -B3 ]
 * [ Cx  Cy  Cz ]   [ Pz ]   [ -C3 ]
 *
 @endverbatim
 *
 * @return 0	OK
 * @return -1	Failure.  Intersection is a line or plane.
 *
 * @param[out] pt	The point of intersection is stored here.
 * @param	a	plane 1
 * @param	b	plane 2
 * @param	c	plane 3
 */

BN_EXPORT extern int bn_mkpoint_3planes(point_t pt,
					const plane_t a,
					const plane_t b,
					const plane_t c);

/**
 * B N _ I S E C T _ L I N E 3 _ P L A N E
 *
 * Intersect an infinite line (specified in point and direction vector
 * form) with a plane that has an outward pointing normal.  The
 * direction vector need not have unit length.  The first three
 * elements of the plane equation must form a unit length vector.
 *
 * @return -2	missed (ray is outside halfspace)
 * @return -1	missed (ray is inside)
 * @return 0	line lies on plane
 * @return 1	hit (ray is entering halfspace)
 * @return 2	hit (ray is leaving)
 *
 * @param[out]	dist	set to the parametric distance of the intercept
 * @param[in]	pt	origin of ray
 * @param[in]	dir	direction of ray
 * @param[in]	plane	equation of plane
 * @param[in]	tol	tolerance values
 */
BN_EXPORT extern int bn_isect_line3_plane(fastf_t *dist,
					  const point_t pt,
					  const vect_t dir,
					  const plane_t plane,
					  const struct bn_tol *tol);

/**
 * B N _ I S E C T _ 2 P L A N E S
 *@brief
 * Given two planes, find the line of intersection between them, if
 * one exists.  The line of intersection is returned in parametric
 * line (point & direction vector) form.
 *
 * In order that all the geometry under consideration be in "front" of
 * the ray, it is necessary to pass the minimum point of the model
 * RPP.  If this convention is unnecessary, just pass (0, 0, 0) as
 * rpp_min.
 *
 * @return 0	success, line of intersection stored in 'pt' and 'dir'
 * @return -1	planes are coplanar
 * @return -2	planes are parallel but not coplanar
 * @return -3	error, should be intersection but unable to find
 *
 * @param[out]	pt	Starting point of line of intersection
 * @param[out]	dir	Direction vector of line of intersection (unit length)
 * @param[in]	a	plane 1 (normal must be unit length)
 * @param[in]	b	plane 2 (normal must be unit length)
 * @param[in]	rpp_min	minimum point of model RPP
 * @param[in]	tol	tolerance values
 */
BN_EXPORT extern int bn_isect_2planes(point_t pt,
				      vect_t dir,
				      const plane_t a,
				      const plane_t b,
				      const vect_t rpp_min,
				      const struct bn_tol *tol);
BN_EXPORT extern int bn_isect_2lines(fastf_t *t,
				     fastf_t *u,
				     const point_t p,
				     const vect_t d,
				     const point_t a,
				     const vect_t c,
				     const struct bn_tol *tol);

/**
 * B N _ I S E C T _ L I N E _ L S E G
 *@brief
 * Intersect a line in parametric form:
 *
 * X = P + t * D
 *
 * with a line segment defined by two distinct points A and B.
 *
 *
 * @return -4	A and B are not distinct points
 * @return -3	Intersection exists, < A (t is returned)
 * @return -2	Intersection exists, > B (t is returned)
 * @return -1	Lines do not intersect
 * @return 0	Lines are co-linear (t for A is returned)
 * @return 1	Intersection at vertex A
 * @return 2	Intersection at vertex B
 * @return 3	Intersection between A and B
 *
 * @par Implicit Returns -
 *
 * t When explicit return >= 0, t is the parameter that describes the
 * intersection of the line and the line segment.  The actual
 * intersection coordinates can be found by solving P + t * D.
 * However, note that for return codes 1 and 2 (intersection exactly
 * at a vertex), it is strongly recommended that the original values
 * passed in A or B are used instead of solving P + t * D, to prevent
 * numeric error from creeping into the position of the endpoints.
 *
 * XXX should probably be called bn_isect_line3_lseg3()
 * XXX should probably be changed to return dist[2]
 */
BN_EXPORT extern int bn_isect_line_lseg(fastf_t *t, const point_t p,
					const vect_t d,
					const point_t a,
					const point_t b,
					const struct bn_tol *tol);

/**
 * B N _ D I S T _ L I N E 3_ P T 3
 *@brief
 * Given a parametric line defined by PT + t * DIR and a point A,
 * return the closest distance between the line and the point.
 *
 *  'dir' need not have unit length.
 *
 * Find parameter for PCA along line with unitized DIR:
 * d = VDOT(f, dir) / MAGNITUDE(dir);
 * Find distance g from PCA to A using Pythagoras:
 * g = sqrt(MAGSQ(f) - d**2)
 *
 * Return -
 * Distance
 */
BN_EXPORT extern double bn_dist_line3_pt3(const point_t pt,
					  const vect_t dir,
					  const point_t a);

/**
 * B N _ D I S T S Q _ L I N E 3 _ P T 3
 *
 * Given a parametric line defined by PT + t * DIR and a point A,
 * return the square of the closest distance between the line and the
 * point.
 *
 * 'dir' need not have unit length.
 *
 * Return -
 * Distance squared
 */
BN_EXPORT extern double bn_distsq_line3_pt3(const point_t pt,
					    const vect_t dir,
					    const point_t a);

/**
 * B N _ D I S T _ L I N E _ O R I G I N
 *@brief
 * Given a parametric line defined by PT + t * DIR, return the closest
 * distance between the line and the origin.
 *
 * 'dir' need not have unit length.
 *
 * @return Distance
 */
BN_EXPORT extern double bn_dist_line_origin(const point_t pt,
					    const vect_t dir);

/**
 * B N _ D I S T _ L I N E 2 _ P O I N T 2
 *@brief
 * Given a parametric line defined by PT + t * DIR and a point A,
 * return the closest distance between the line and the point.
 *
 * 'dir' need not have unit length.
 *
 * @return Distance
 */
BN_EXPORT extern double bn_dist_line2_point2(const point_t pt,
					     const vect_t dir,
					     const point_t a);

/**
 * B N _ D I S T S Q _ L I N E 2 _ P O I N T 2
 *@brief
 * Given a parametric line defined by PT + t * DIR and a point A,
 * return the closest distance between the line and the point,
 * squared.
 *
 * 'dir' need not have unit length.
 *
 * @return
 * Distance squared
 */
BN_EXPORT extern double bn_distsq_line2_point2(const point_t pt,
					       const vect_t dir,
					       const point_t a);

/**
 * B N _ A R E A _ O F _ T R I A N G L E
 *@brief
 * Returns the area of a triangle. Algorithm by Jon Leech 3/24/89.
 */
BN_EXPORT extern double bn_area_of_triangle(const point_t a,
					    const point_t b,
					    const point_t c);

/**
 * B N _ I S E C T _ P T _ L S E G
 *@brief
 * Intersect a point P with the line segment defined by two distinct
 * points A and B.
 *
 * @return -2	P on line AB but outside range of AB,
 * 			dist = distance from A to P on line.
 * @return -1	P not on line of AB within tolerance
 * @return 1	P is at A
 * @return 2	P is at B
 * @return 3	P is on AB, dist = distance from A to P on line.
 @verbatim
 B *
 |
 P'*-tol-*P
 |    /   _
 dist  /   /|
 |  /   /
 | /   / AtoP
 |/   /
 A *   /

 tol = distance limit from line to pt P;
 dist = parametric distance from A to P' (in terms of A to B)
 @endverbatim
 *
 * @param p	point
 * @param a	start of lseg
 * @param b	end of lseg
 * @param tol	tolerance values
 * @param[out] dist	parametric distance from A to P' (in terms of A to B)
 */
BN_EXPORT extern int bn_isect_pt_lseg(fastf_t *dist,
				      const point_t a,
				      const point_t b,
				      const point_t p,
				      const struct bn_tol *tol);

BN_EXPORT extern double bn_dist_pt_lseg(point_t pca,
					const point_t a,
					const point_t b,
					const point_t p,
					const struct bn_tol *tol);

/**
 * B N _ R O T A T E _ B B O X
 *@brief
 * Transform a bounding box (RPP) by the given 4x4 matrix.  There are
 * 8 corners to the bounding RPP.  Each one needs to be transformed
 * and min/max'ed.  This is not minimal, but does fully contain any
 * internal object, using an axis-aligned RPP.
 */
BN_EXPORT extern void bn_rotate_bbox(point_t omin,
				     point_t omax,
				     const mat_t mat,
				     const point_t imin,
				     const point_t imax);

/**
 * B N _ R O T A T E _ P L A N E
 *@brief
 * Transform a plane equation by the given 4x4 matrix.
 */
BN_EXPORT extern void bn_rotate_plane(plane_t oplane,
				      const mat_t mat,
				      const plane_t iplane);

/**
 * B N _ C O P L A N A R
 *@brief
 * Test if two planes are identical.  If so, their dot products will
 * be either +1 or -1, with the distance from the origin equal in
 * magnitude.
 *
 * @return -1	not coplanar, parallel but distinct
 * @return 0	not coplanar, not parallel.  Planes intersect.
 * @return 1	coplanar, same normal direction
 * @return 2	coplanar, opposite normal direction
 */
BN_EXPORT extern int bn_coplanar(const plane_t a,
				 const plane_t b,
				 const struct bn_tol *tol);

/**
 * B N _ A N G L E _ M E A S U R E
 *
 * Using two perpendicular vectors (x_dir and y_dir) which lie in the
 * same plane as 'vec', return the angle (in radians) of 'vec' from
 * x_dir, going CCW around the perpendicular x_dir CROSS y_dir.
 *
 * Trig note -
 *
 * theta = atan2(x, y) returns an angle in the range -pi to +pi.
 * Here, we need an angle in the range of 0 to 2pi.  This could be
 * implemented by adding 2pi to theta when theta is negative, but this
 * could have nasty numeric ambiguity right in the vicinity of theta =
 * +pi, which is a very critical angle for the applications using this
 * routine.
 *
 * So, an alternative formulation is to compute gamma = atan2(-x, -y),
 * and then theta = gamma + pi.  Now, any error will occur in the
 * vicinity of theta = 0, which can be handled much more readily.
 *
 * If theta is negative, or greater than two pi, wrap it around.
 * These conditions only occur if there are problems in atan2().
 *
 * @return vec == x_dir returns 0,
 * @return vec == y_dir returns pi/2,
 * @return vec == -x_dir returns pi,
 * @return vec == -y_dir returns 3*pi/2.
 *
 * In all cases, the returned value is between 0 and bn_twopi.
 */
BN_EXPORT extern double bn_angle_measure(vect_t vec,
					 const vect_t x_dir,
					 const vect_t y_dir);

/**
 * B N _ D I S T _ P T 3 _ A L O N G _ L I N E 3
 *@brief
 * Return the parametric distance t of a point X along a line defined
 * as a ray, i.e. solve X = P + t * D.  If the point X does not lie on
 * the line, then t is the distance of the perpendicular projection of
 * point X onto the line.
 */
BN_EXPORT extern double bn_dist_pt3_along_line3(const point_t p,
						const vect_t d,
						const point_t x);

/**
 * B N _ D I S T _ P T 2 _ A L O N G _ L I N E 2
 *@brief
 * Return the parametric distance t of a point X along a line defined
 * as a ray, i.e. solve X = P + t * D.  If the point X does not lie on
 * the line, then t is the distance of the perpendicular projection of
 * point X onto the line.
 */
BN_EXPORT extern double bn_dist_pt2_along_line2(const point_t p,
						const vect_t d,
						const point_t x);

/**
 *
 * @return 1	if left <= mid <= right
 * @return 0	if mid is not in the range.
 */
BN_EXPORT extern int bn_between(double left,
				double mid,
				double right,
				const struct bn_tol *tol);

/**
 * B N _ D O E S _ R A Y _ I S E C T _ T R I
 *
 * @return 0	No intersection
 * @return 1	Intersection, 'inter' has intersect point.
 */
BN_EXPORT extern int bn_does_ray_isect_tri(const point_t pt,
					   const vect_t dir,
					   const point_t V,
					   const point_t A,
					   const point_t B,
					   point_t inter);

/**
 * B N _ H L F _ C L A S S
 *@brief
 * Classify a halfspace, specified by its plane equation, against a
 * bounding RPP.
 *
 * @return BN_CLASSIFY_INSIDE
 * @return BN_CLASSIFY_OVERLAPPING
 * @return BN_CLASSIFY_OUTSIDE
 */
BN_EXPORT extern int bn_hlf_class(const plane_t half_eqn,
				  const vect_t min, const vect_t max,
				  const struct bn_tol *tol);


#define BN_CLASSIFY_UNIMPLEMENTED 0x0000
#define BN_CLASSIFY_INSIDE        0x0001
#define BN_CLASSIFY_OVERLAPPING   0x0002
#define BN_CLASSIFY_OUTSIDE       0x0003


/**
 * B N _ I S E C T _ P L A N E S
 *@brief
 * Calculates the point that is the minimum distance from all the
 * planes in the "planes" array.  If the planes intersect at a single
 * point, that point is the solution.
 *
 * The method used here is based on:

 * An expression for the distance from a point to a plane is:
 * VDOT(pt, plane)-plane[H].
 * Square that distance and sum for all planes to get the "total"
 * distance.
 * For minimum total distance, the partial derivatives of this
 * expression (with respect to x, y, and z) must all be zero.
 * This produces a set of three equations in three unknowns (x, y, z).

 * This routine sets up the three equations as [matrix][pt] = [hpq]
 * and solves by inverting "matrix" into "inverse" and
 * [pt] = [inverse][hpq].
 *
 * There is likely a more economical solution rather than matrix
 * inversion, but bn_mat_inv was handy at the time.
 *
 * Checks if these planes form a singular matrix and returns.
 *
 * @return 0 - all is well
 * @return 1 - planes form a singular matrix (no solution)
 */
BN_EXPORT extern int bn_isect_planes(point_t pt,
				     const plane_t planes[],
				     const size_t pl_count);

/** @} */


/*----------------------------------------------------------------------*/


/* poly.c */

/** @addtogroup poly */
/** @{ */

/**
 * Library for dealing with polynomials.
 *
 */


/* This could be larger, or even dynamic... */
#define BN_MAX_POLY_DEGREE 6	/* Maximum Poly Order */

/**
 * Polynomial data type
 * bn_poly->cf[n] corresponds to X^n
 */
typedef struct bn_poly {
    uint32_t magic;
    size_t dgr;
    fastf_t cf[BN_MAX_POLY_DEGREE+1];
}  bn_poly_t;

#define BN_CK_POLY(_p) BU_CKMAG(_p, BN_POLY_MAGIC, "struct bn_poly")
#define BN_POLY_NULL   ((struct bn_poly *)NULL)
#define BN_POLY_INIT_ZERO { BN_POLY_MAGIC, 0, {0.0} }


/**
 * bn_poly_mul
 *
 * @brief
 * multiply two polynomials
 */
BN_EXPORT extern struct bn_poly *bn_poly_mul(struct bn_poly *product,
					     const struct bn_poly *m1,
					     const struct bn_poly *m2);

/**
 * bn_poly_scale
 *
 * @brief
 * scale a polynomial
 */
BN_EXPORT extern struct bn_poly *bn_poly_scale(struct bn_poly *eqn,
					       double factor);

/**
 * bn_poly_add
 * @brief
 * add two polynomials
 */
BN_EXPORT extern struct bn_poly *bn_poly_add(struct bn_poly *sum,
					     const struct bn_poly *poly1,
					     const struct bn_poly *poly2);

/**
 * bn_poly_sub
 * @brief
 * subtract two polynomials
 */
BN_EXPORT extern struct bn_poly *bn_poly_sub(struct bn_poly *diff,
					     const struct bn_poly *poly1,
					     const struct bn_poly *poly2);

/**
 * s y n D i v
 * @brief
 * Divides any polynomial into any other polynomial using synthetic
 * division.  Both polynomials must have real coefficients.
 */
BN_EXPORT extern void bn_poly_synthetic_division(struct bn_poly *quo,
						 struct bn_poly *rem,
						 const struct bn_poly *dvdend,
						 const struct bn_poly *dvsor);

/**
 * b n _ p o l y _ q u a d r a t i c _ r o o t s
 *@brief
 * Uses the quadratic formula to find the roots (in `complex' form) of
 * any quadratic equation with real coefficients.
 *
 *	@return 1 for success
 *	@return 0 for fail.
 */
BN_EXPORT extern int bn_poly_quadratic_roots(struct bn_complex roots[],
					     const struct bn_poly *quadrat);

/**
 * b n _ p o l y _ c u b i c _ r o o t s
 *@brief
 * Uses the cubic formula to find the roots (in `complex' form)
 * of any cubic equation with real coefficients.
 *
 * to solve a polynomial of the form:
 *
 * X**3 + c1*X**2 + c2*X + c3 = 0,
 *
 * first reduce it to the form:
 *
 * Y**3 + a*Y + b = 0,
 *
 * where
 * Y = X + c1/3,
 * and
 * a = c2 - c1**2/3,
 * b = (2*c1**3 - 9*c1*c2 + 27*c3)/27.
 *
 * Then we define the value delta,   D = b**2/4 + a**3/27.
 *
 * If D > 0, there will be one real root and two conjugate
 * complex roots.
 * If D = 0, there will be three real roots at least two of
 * which are equal.
 * If D < 0, there will be three unequal real roots.
 *
 * Returns 1 for success, 0 for fail.
 */
BN_EXPORT extern int bn_poly_cubic_roots(struct bn_complex roots[],
					 const struct bn_poly *eqn);

/**
 * b n _ p o l y _ q u a r t i c _ r o o t s
 *@brief
 * Uses the quartic formula to find the roots (in `complex' form)
 * of any quartic equation with real coefficients.
 *
 *	@return 1 for success
 *	@return 0 for fail.
 */
BN_EXPORT extern int bn_poly_quartic_roots(struct bn_complex roots[],
					   const struct bn_poly *eqn);

BN_EXPORT extern int bn_poly_findroot(bn_poly_t *eqn,
				      bn_complex_t *nxZ,
				      const char *str);

BN_EXPORT extern void bn_poly_eval_w_2derivatives(bn_complex_t *cZ,
						  bn_poly_t *eqn,
						  bn_complex_t *b,
						  bn_complex_t *c,
						  bn_complex_t *d);

BN_EXPORT extern int bn_poly_checkroots(bn_poly_t *eqn,
					bn_complex_t *roots,
					int nroots);

BN_EXPORT extern void bn_poly_deflate(bn_poly_t *oldP,
				      bn_complex_t *root);

BN_EXPORT extern int bn_poly_roots(bn_poly_t *eqn,
				   bn_complex_t roots[],
				   const char *name);

/**
 * b n _ p r _ p o l y
 *
 * Print out the polynomial.
 */
BN_EXPORT extern void bn_pr_poly(const char *title,
				 const struct bn_poly *eqn);

/**
 * b n _ p r _ r o o t s
 *
 * Print out the roots of a given polynomial (complex numbers)
 */
BN_EXPORT extern void bn_pr_roots(const char *title,
				  const struct bn_complex roots[],
				  int n);

/** @} */


/*----------------------------------------------------------------------*/


/* multipoly.c */

/** @addtogroup multipoly */
/** @{ */

/**
 * Polynomial data type
 */
typedef struct bn_multipoly {
    uint32_t magic;
    int dgrs;
    int dgrt;
    double **cf;
}  bn_multipoly_t;

/** @} */


/*----------------------------------------------------------------------*/


/* qmath.c */

/** @addtogroup mat */
/** @{ */

/** @brief
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

/*
 * Quaternion support
 */


/**
 *			Q U A T _ M A T 2 Q U A T
 *@brief
 *
 *  Convert Matrix to Quaternion.
 */
BN_EXPORT extern void quat_mat2quat(quat_t quat,
				    const mat_t mat);

/**
 *			Q U A T _ Q U A T 2 M A T
 *@brief
 *
 *  Convert Quaternion to Matrix.
 *
 * NB: This only works for UNIT quaternions.  We may get imaginary results
 *   otherwise.  We should normalize first (still yields same rotation).
 */
BN_EXPORT extern void quat_quat2mat(mat_t mat,
				    const quat_t quat);

/**
 *			Q U A T _ D I S T A N C E
 *@brief
 *
 * Gives the euclidean distance between two quaternions.
 *
 */
BN_EXPORT extern double quat_distance(const quat_t q1,
				      const quat_t q2);

/**
 *			Q U A T _ D O U B L E
 *@brief
 * Gives the quaternion point representing twice the rotation
 *   from q1 to q2.
 *   Needed for patching Bezier curves together.
 *   A rather poor name admittedly.
 */
BN_EXPORT extern void quat_double(quat_t qout,
				  const quat_t q1,
				  const quat_t q2);

/**
 *			Q U A T _ B I S E C T
 *@brief
 * Gives the bisector of quaternions q1 and q2.
 * (Could be done with quat_slerp and factor 0.5)
 * [I believe they must be unit quaternions this to work]
 */
BN_EXPORT extern void quat_bisect(quat_t qout,
				  const quat_t q1,
				  const quat_t q2);

/**
 *			Q U A T _ S L E R P
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
 *			Q U A T _ S B E R P
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
 *			Q U A T _ M A K E _ N E A R E S T
 *@brief
 *  Set the quaternion q1 to the quaternion which yields the
 *   smallest rotation from q2 (of the two versions of q1 which
 *   produce the same orientation).
 *
 * Note that smallest euclidian distance implies smallest great
 *   circle distance as well (since surface is convex).
 */
BN_EXPORT extern void quat_make_nearest(quat_t q1,
					const quat_t q2);

/**
 *			Q U A T _ P R I N T
 */
BN_EXPORT extern void quat_print(const char *title,
				 const quat_t quat);

/**
 *			Q U A T _ E X P
 *@brief
 *  Exponentiate a quaternion, assuming that the scalar part is 0.
 *  Code by Ken Shoemake.
 */
BN_EXPORT extern void quat_exp(quat_t out,
			       const quat_t in);

/**
 *			Q U A T _ L O G
 *@brief
 *  Take the natural logarithm of a unit quaternion.
 *  Code by Ken Shoemake.
 */
BN_EXPORT extern void quat_log(quat_t out,
			       const quat_t in);
/** @} */


/*----------------------------------------------------------------------*/


/* rand.c */

/** @addtogroup rnd */
/** @{ */
/**
 * A supply of fast pseudo-random numbers from table in bn/rand.c.
 * The values are in the open interval (i.e. exclusive) of 0.0 to 1.0
 * range with a period of 4096.
 *
 * @par Usage:
 @code
 unsigned idx;
 float f;

 BN_RANDSEED(idx, integer_seed);

 while (NEED_MORE_RAND_NUMBERS) {
 f = BN_RANDOM(idx);
 }
 @endcode
 *
 * Note that the values from bn_rand_half() become all 0.0 when the
 * benchmark flag is set (bn_rand_halftab is set to all 0's).  The
 * numbers from bn_rand_table do not change, because the procedural
 * noise would cease to exist.
 */


#define BN_RAND_TABSIZE 4096
#define BN_RAND_TABMASK 0xfff
#define BN_RANDSEED(_i, _seed)  _i = ((unsigned)_seed) % BN_RAND_TABSIZE

/**
 * This is our table of random numbers.  Rather than calling drand48()
 * or random() or rand() we just pick numbers out of this table.  This
 * table has 4096 unique entries with floating point values ranging
 * from the open interval (i.e. exclusive) 0.0 to 1.0 range.
 *
 * There are convenience macros for access in the bn.h header.
 */
BN_EXPORT extern const float bn_rand_table[BN_RAND_TABSIZE];

/** BN_RANDOM always gives numbers between the open interval 0.0 to 1.0 */
#define BN_RANDOM(_i)	bn_rand_table[ _i = (_i+1) % BN_RAND_TABSIZE ]

/** BN_RANDHALF always gives numbers between the open interval -0.5 and 0.5 */
#define BN_RANDHALF(_i) (bn_rand_table[ _i = (_i+1) % BN_RAND_TABSIZE ]-0.5)
#define BN_RANDHALF_INIT(_p) _p = bn_rand_table

#define BN_RANDHALFTABSIZE 16535	/* Powers of two give streaking */
BN_EXPORT extern int bn_randhalftabsize;

/**
 *  The actual table of random floating point numbers with values in
 *  the closed interval (i.e. inclusive) -0.5 to +0.5 range.
 *
 *  For benchmarking purposes, this table is zeroed.
 */
BN_EXPORT extern float bn_rand_halftab[BN_RANDHALFTABSIZE];

/**
 * random numbers between the closed interval -0.5 to 0.5 inclusive,
 * except when benchmark flag is set, when this becomes a constant 0.0
 *
 * @param _p float pointer type initialized by bn_rand_init()
 *
 */
#define bn_rand_half(_p)	\
    ((++(_p) >= &bn_rand_halftab[bn_randhalftabsize] || \
      (_p) < bn_rand_halftab) ? \
     *((_p) = bn_rand_halftab) : *(_p))

/**
 * initialize the seed for the large random number table (halftab)
 *
 * @param _p float pointer to be initialized, used for bn_rand0to1()
 * and bn_rand_half()
 * @param _seed Integer SEED for offset in the table.
 *
 */
#define bn_rand_init(_p, _seed)	\
    (_p) = &bn_rand_halftab[ \
	(int)(\
	    (bn_rand_halftab[(_seed)%bn_randhalftabsize] + 0.5) * \
	    (bn_randhalftabsize-1)) ]

/**
 * random numbers in the closed interval 0.0 to 1.0 range (inclusive)
 * except when benchmarking, when this is always 0.5
 *
 * @param _q float pointer type initialized by bn_rand_init()
 *
 */
#define bn_rand0to1(_q)	(bn_rand_half(_q)+0.5)

#define BN_SINTABSIZE 2048

BN_EXPORT extern double bn_sin_scale;

#define bn_tab_sin(_a)	(((_a) > 0) ? \
			 (bn_sin_table[(int)((0.5+ (_a)*bn_sin_scale))&(BN_SINTABSIZE-1)]) :\
			 (-bn_sin_table[(int)((0.5- (_a)*bn_sin_scale))&(BN_SINTABSIZE-1)]))

/**
 * table of floating point sine values in the closed (i.e. inclusive)
 * interval -1.0 to 1.0 range.
 */
BN_EXPORT extern const float bn_sin_table[BN_SINTABSIZE];

/**
 *			M A T H T A B _ C O N S T A N T
 *@brief
 *  For benchmarking purposes, make the random number table predictable.
 *  Setting to all zeros keeps dithered values at their original values.
 */
BN_EXPORT extern void bn_mathtab_constant();

/** @} */


/*----------------------------------------------------------------------*/


/* randmt.c */

/** @addtogroup rnd */
/** @{ */
/**
 * Mersenne Twister random number generation as defined by MT19937.
 *
 * Generates one pseudorandom real number (double) which is uniformly
 * distributed on [0, 1]-interval, for each call.
 *
 * @par Usage:
 @code
 double d;

 bn_randmt_seed(integer_seed);

 while (NEED_MORE_RAND_NUMBERS) {
 d = bn_randmt();
 }
 @endcode
 *
 */

BN_EXPORT extern double bn_randmt();
BN_EXPORT extern void bn_randmt_seed(unsigned long seed);

/** @} */


/*----------------------------------------------------------------------*/


/* wavelet.c */


/** @addtogroup wavelet */
/** @{ */
/** @brief
 *  This is a standard wavelet library that takes a given data buffer of some data
 *  type and then performs a wavelet transform on that data.
 *
 * The transform
 *  operations available are to either decompose or reconstruct a signal into its
 *  corresponding wavelet form based on the haar wavelet.
 *
 *  Wavelet decompose/reconstruct operations
 *
 *	- bn_wlt_haar_1d_double_decompose(tbuffer, buffer, dimen, channels, limit)
 *	- bn_wlt_haar_1d_float_decompose (tbuffer, buffer, dimen, channels, limit)
 *	- bn_wlt_haar_1d_char_decompose  (tbuffer, buffer, dimen, channels, limit)
 *	- bn_wlt_haar_1d_short_decompose (tbuffer, buffer, dimen, channels, limit)
 *	- bn_wlt_haar_1d_int_decompose   (tbuffer, buffer, dimen, channels, limit)
 *	- bn_wlt_haar_1d_long_decompose  (tbuffer, buffer, dimen, channels, limit)
 *
 *	- bn_wlt_haar_1d_double_reconstruct(tbuffer, buffer, dimen, channels, sub_sz, limit)
 *	- bn_wlt_haar_1d_float_reconstruct (tbuffer, buffer, dimen, channels, sub_sz, limit)
 *	- bn_wlt_haar_1d_char_reconstruct  (tbuffer, buffer, dimen, channels, sub_sz, limit)
 *	- bn_wlt_haar_1d_short_reconstruct (tbuffer, buffer, dimen, channels, sub_sz, limit)
 *	- bn_wlt_haar_1d_int_reconstruct   (tbuffer, buffer, dimen, channels, sub_sz, limit)
 *	- bn_wlt_haar_1d_long_reconstruct  (tbuffer, buffer, dimen, channels, sub_sz, limit)
 *
 *	- bn_wlt_haar_2d_double_decompose(tbuffer, buffer, dimen, channels, limit)
 *	- bn_wlt_haar_2d_float_decompose (tbuffer, buffer, dimen, channels, limit)
 *	- bn_wlt_haar_2d_char_decompose  (tbuffer, buffer, dimen, channels, limit)
 *	- bn_wlt_haar_2d_short_decompose (tbuffer, buffer, dimen, channels, limit)
 *	- bn_wlt_haar_2d_int_decompose   (tbuffer, buffer, dimen, channels, limit)
 *	- bn_wlt_haar_2d_long_decompose  (tbuffer, buffer, dimen, channels, limit)
 *
 *	- bn_wlt_haar_2d_double_reconstruct(tbuffer, buffer, dimen, channels, sub_sz, limit)
 *	- bn_wlt_haar_2d_float_reconstruct (tbuffer, buffer, dimen, channels, sub_sz, limit)
 *	- bn_wlt_haar_2d_char_reconstruct  (tbuffer, buffer, dimen, channels, sub_sz, limit)
 *	- bn_wlt_haar_2d_short_reconstruct (tbuffer, buffer, dimen, channels, sub_sz, limit)
 *	- bn_wlt_haar_2d_int_reconstruct   (tbuffer, buffer, dimen, channels, sub_sz, limit)
 *	- bn_wlt_haar_2d_long_reconstruct  (tbuffer, buffer, dimen, channels, sub_sz, limit)
 *
 *	- bn_wlt_haar_2d_double_decompose2(tbuffer, buffer, width, height, channels, limit)
 *	- bn_wlt_haar_2d_float_decompose2 (tbuffer, buffer, width, height, channels, limit)
 *	- bn_wlt_haar_2d_char_decompose2  (tbuffer, buffer, width, height, channels, limit)
 *	- bn_wlt_haar_2d_short_decompose2 (tbuffer, buffer, width, height, channels, limit)
 *	- bn_wlt_haar_2d_int_decompose2   (tbuffer, buffer, width, height, channels, limit)
 *	- bn_wlt_haar_2d_long_decompose2  (tbuffer, buffer, width, height, channels, limit)
 *
 *	- bn_wlt_haar_2d_double_reconstruct2(tbuffer, buffer, width, height, channels, sub_sz, limit)
 *	- bn_wlt_haar_2d_float_reconstruct2 (tbuffer, buffer, width, height, channels, sub_sz, limit)
 *	- bn_wlt_haar_2d_char_reconstruct2  (tbuffer, buffer, width, height, channels, sub_sz, limit)
 *	- bn_wlt_haar_2d_short_reconstruct2 (tbuffer, buffer, width, height, channels, sub_sz, limit)
 *	- bn_wlt_haar_2d_int_reconstruct2   (tbuffer, buffer, width, height, channels, sub_sz, limit)
 *	- bn_wlt_haar_2d_long_reconstruct2  (tbuffer, buffer, width, height, channels, sub_sz, limit)
 *
 *
 *  For greatest accuracy, it is preferable to convert everything to "double"
 *  and decompose/reconstruct with that.  However, there are useful
 *  properties to performing the decomposition and/or reconstruction in
 *  various data types (most notably char).
 *
 *  Rather than define all of these routines explicitly, we define
 *  2 macros "decompose" and "reconstruct" which embody the structure of
 *  the function (which is common to all of them).  We then instatiate
 *  these macros once for each of the data types.  It's ugly, but it
 *  assures that a change to the structure of one operation type
 *  (decompose or reconstruct) occurs for all data types.
 *
 *
 *
 *
 *  bn_wlt_haar_1d_*_decompose(tbuffer, buffer, dimen, channels, limit)
 *  @par Parameters:
 *	- tbuffer     a temporary data buffer 1/2 as large as "buffer". See (1) below.
 *	- buffer      pointer to the data to be decomposed
 *	- dimen    the number of samples in the data buffer
 *	- channels the number of values per sample
 *	- limit    the extent of the decomposition
 *
 *  Perform a Haar wavelet decomposition on the data in buffer "buffer".  The
 *  decomposition is done "in place" on the data, hence the values in "buffer"
 *  are not preserved, but rather replaced by their decomposition.
 *  The number of original samples in the buffer (parameter "dimen") and the
 *  decomposition limit ("limit") must both be a power of 2 (e.g. 512, 1024).
 *  The buffer is decomposed into "average" and "detail" halves until the
 *  size of the "average" portion reaches "limit".  Simultaneous
 *  decomposition of multi-plane (e.g. pixel) data, can be performed by
 *  indicating the number of planes in the "channels" parameter.
 *
 *  (1) The process requires a temporary buffer which is 1/2 the size of the
 *  longest span to be decomposed.  If the "tbuffer" argument is non-null then
 *  it is a pointer to a temporary buffer.  If the pointer is NULL, then a
 *  local temporary buffer will be allocated (and freed).
 *
 *  @par Examples:
 @code
 double dbuffer[512], cbuffer[256];
 ...
 bn_wlt_haar_1d_double_decompose(cbuffer, dbuffer, 512, 1, 1);
 @endcode
 *
 *    performs complete decomposition on the data in array "dbuffer".
 *
 @code
 double buffer[3][512];	 /_* 512 samples, 3 values/sample (e.g. RGB?)*_/
 double tbuffer[3][256];	 /_* the temporary buffer *_/
 ...
 bn_wlt_haar_1d_double_decompose(tbuffer, buffer, 512, 3, 1);
 @endcode
 *
 *    This will completely decompose the data in buffer.  The first sample will
 *    be the average of all the samples.  Alternatively:
 *
 * bn_wlt_haar_1d_double_decompose(tbuffer, buffer, 512, 3, 64);
 *
 *    decomposes buffer into a 64-sample "average image" and 3 "detail" sets.
 *
 *  bn_wlt_haar_1d_*_reconstruct(tbuffer, buffer, dimen, channels, sub_sz, limit)
 *
 */
/** @} */


#define CK_POW_2(dimen) { \
	register unsigned long j; \
	register int ok; \
	for (ok=0, j=0; j < sizeof(unsigned long) * 8; j++) { \
	    if ((unsigned long)(1<<j) == dimen) { ok = 1;  break; } \
	} \
	if (! ok) { \
	    bu_log("%s:%d value %ld should be power of 2 (2^%ld)\n", \
		   __FILE__, __LINE__, (long)dimen, (long)j); \
	    bu_bomb("CK_POW_2"); \
	} \
    }

BN_EXPORT extern void bn_wlt_haar_1d_double_decompose(double *tbuf,
						      double *buf,
						      unsigned long dimen,
						      unsigned long depth,
						      unsigned long limit);
BN_EXPORT extern void bn_wlt_haar_1d_double_reconstruct(double *tbuf,
							double *buf,
							unsigned long dimen,
							unsigned long depth,
							unsigned long subimage_size,
							unsigned long limit);

BN_EXPORT extern void bn_wlt_haar_1d_float_decompose(float *tbuf,
						     float *buf,
						     unsigned long dimen,
						     unsigned long depth,
						     unsigned long limit);
BN_EXPORT extern void bn_wlt_haar_1d_float_reconstruct(float *tbuf,
						       float *buf,
						       unsigned long dimen,
						       unsigned long depth,
						       unsigned long subimage_size,
						       unsigned long limit);

BN_EXPORT extern void bn_wlt_haar_1d_char_decompose(char *tbuf,
						    char *buf,
						    unsigned long dimen,
						    unsigned long depth,
						    unsigned long limit);
BN_EXPORT extern void bn_wlt_haar_1d_char_reconstruct(char *tbuf, char *buf,
						      unsigned long dimen,
						      unsigned long depth,
						      unsigned long subimage_size,
						      unsigned long limit);

BN_EXPORT extern void bn_wlt_haar_1d_short_decompose(short *tbuf, short *buf,
						     unsigned long dimen,
						     unsigned long depth,
						     unsigned long limit);
BN_EXPORT extern void bn_wlt_haar_1d_short_reconstruct(short *tbuf, short *buf,
						       unsigned long dimen,
						       unsigned long depth,
						       unsigned long subimage_size,
						       unsigned long limit);

BN_EXPORT extern void bn_wlt_haar_1d_int_decompose(int *tbuf, int *buf,
						   unsigned long dimen,
						   unsigned long depth,
						   unsigned long limit);
BN_EXPORT extern void bn_wlt_haar_1d_int_reconstruct(int *tbuf,
						     int *buf,
						     unsigned long dimen,
						     unsigned long depth,
						     unsigned long subimage_size,
						     unsigned long limit);

BN_EXPORT extern void bn_wlt_haar_1d_long_decompose(long *tbuf, long *buf,
						    unsigned long dimen,
						    unsigned long depth,
						    unsigned long limit);
BN_EXPORT extern void bn_wlt_haar_1d_long_reconstruct(long *tbuf, long *buf,
						      unsigned long dimen,
						      unsigned long depth,
						      unsigned long subimage_size,
						      unsigned long limit);


BN_EXPORT extern void bn_wlt_haar_2d_double_decompose(double *tbuf,
						      double *buf,
						      unsigned long dimen,
						      unsigned long depth,
						      unsigned long limit);
BN_EXPORT extern void bn_wlt_haar_2d_double_reconstruct(double *tbuf,
							double *buf,
							unsigned long dimen,
							unsigned long depth,
							unsigned long subimage_size,
							unsigned long limit);

BN_EXPORT extern void bn_wlt_haar_2d_float_decompose(float *tbuf,
						     float *buf,
						     unsigned long dimen,
						     unsigned long depth,
						     unsigned long limit);
BN_EXPORT extern void bn_wlt_haar_2d_float_reconstruct(float *tbuf,
						       float *buf,
						       unsigned long dimen,
						       unsigned long depth,
						       unsigned long subimage_size,
						       unsigned long limit);

BN_EXPORT extern void bn_wlt_haar_2d_char_decompose(char *tbuf,
						    char *buf,
						    unsigned long dimen,
						    unsigned long depth,
						    unsigned long limit);
BN_EXPORT extern void bn_wlt_haar_2d_char_reconstruct(char *tbuf,
						      char *buf,
						      unsigned long dimen,
						      unsigned long depth,
						      unsigned long subimage_size,
						      unsigned long limit);

BN_EXPORT extern void bn_wlt_haar_2d_short_decompose(short *tbuf,
						     short *buf,
						     unsigned long dimen,
						     unsigned long depth,
						     unsigned long limit);
BN_EXPORT extern void bn_wlt_haar_2d_short_reconstruct(short *tbuf,
						       short *buf,
						       unsigned long dimen,
						       unsigned long depth,
						       unsigned long subimage_size,
						       unsigned long limit);

BN_EXPORT extern void bn_wlt_haar_2d_int_decompose(int *tbuf,
						   int *buf,
						   unsigned long dimen,
						   unsigned long depth,
						   unsigned long limit);
BN_EXPORT extern void bn_wlt_haar_2d_int_reconstruct(int *tbuf,
						     int *buf,
						     unsigned long dimen,
						     unsigned long depth,
						     unsigned long subimage_size,
						     unsigned long limit);

BN_EXPORT extern void bn_wlt_haar_2d_long_decompose(long *tbuf,
						    long *buf,
						    unsigned long dimen,
						    unsigned long depth,
						    unsigned long limit);
BN_EXPORT extern void bn_wlt_haar_2d_long_reconstruct(long *tbuf,
						      long *buf,
						      unsigned long dimen,
						      unsigned long depth,
						      unsigned long subimage_size,
						      unsigned long limit);


BN_EXPORT extern void bn_wlt_haar_2d_double_decompose2(double *tbuf,
						       double *buf,
						       unsigned long dimen,
						       unsigned long width,
						       unsigned long height,
						       unsigned long limit);
BN_EXPORT extern void bn_wlt_haar_2d_double_reconstruct2(double *tbuf,
							 double *buf,
							 unsigned long dimen,
							 unsigned long width,
							 unsigned long height,
							 unsigned long subimage_size,
							 unsigned long limit);

BN_EXPORT extern void bn_wlt_haar_2d_float_decompose2(float *tbuf,
						      float *buf,
						      unsigned long dimen,
						      unsigned long width,
						      unsigned long height,
						      unsigned long limit);
BN_EXPORT extern void bn_wlt_haar_2d_float_reconstruct2(float *tbuf,
							float *buf,
							unsigned long dimen,
							unsigned long width,
							unsigned long height,
							unsigned long subimage_size,
							unsigned long limit);

BN_EXPORT extern void bn_wlt_haar_2d_char_decompose2(char *tbuf,
						     char *buf,
						     unsigned long dimen,
						     unsigned long width,
						     unsigned long height,
						     unsigned long limit);
BN_EXPORT extern void bn_wlt_haar_2d_char_reconstruct2(char *tbuf,
						       char *buf,
						       unsigned long dimen,
						       unsigned long width,
						       unsigned long height,
						       unsigned long subimage_size,
						       unsigned long limit);

BN_EXPORT extern void bn_wlt_haar_2d_short_decompose2(short *tbuf,
						      short *buf,
						      unsigned long dimen,
						      unsigned long width,
						      unsigned long height,
						      unsigned long limit);
BN_EXPORT extern void bn_wlt_haar_2d_short_reconstruct2(short *tbuf,
							short *buf,
							unsigned long dimen,
							unsigned long width,
							unsigned long height,
							unsigned long subimage_size,
							unsigned long limit);

BN_EXPORT extern void bn_wlt_haar_2d_int_decompose2(int *tbuf,
						    int *buf,
						    unsigned long dimen,
						    unsigned long width,
						    unsigned long height,
						    unsigned long limit);
BN_EXPORT extern void bn_wlt_haar_2d_int_reconstruct2(int *tbuf,
						      int *buf,
						      unsigned long dimen,
						      unsigned long width,
						      unsigned long height,
						      unsigned long subimage_size,
						      unsigned long limit);

BN_EXPORT extern void bn_wlt_haar_2d_long_decompose2(long *tbuf,
						     long *buf,
						     unsigned long dimen,
						     unsigned long width,
						     unsigned long height,
						     unsigned long limit);
BN_EXPORT extern void bn_wlt_haar_2d_long_reconstruct2(long *tbuf,
						       long *buf,
						       unsigned long dimen,
						       unsigned long width,
						       unsigned long height,
						       unsigned long subimage_size,
						       unsigned long limit);


/*----------------------------------------------------------------------*/


/** @file libbn/globals.c
 *
 * Global variables in LIBBN.
 *
 * New global variables are discouraged and refactoring in ways that
 * eliminates existing global variables without reducing functionality
 * is always encouraged.
 *
 */

/**
 * pi
 */
BN_EXPORT extern const fastf_t bn_pi;

/**
 * pi*2
 */
BN_EXPORT extern const fastf_t bn_twopi;

/**
 * pi/2
 */
BN_EXPORT extern const fastf_t bn_halfpi;

/**
 * pi/4
 */
BN_EXPORT extern const fastf_t bn_quarterpi;

/**
 * 1/pi
 */
BN_EXPORT extern const fastf_t bn_invpi;

/**
 * 1/(pi*2)
 */
BN_EXPORT extern const fastf_t bn_inv2pi;

/**
 * 1/(pi*4)
 */
BN_EXPORT extern const fastf_t bn_inv4pi;

/**
 * 1.0/255.0
 */
BN_EXPORT extern const fastf_t bn_inv255;

/**
 * (pi*2)/360
 */
BN_EXPORT extern const fastf_t bn_degtorad;

/**
 * 360/(pi*2)
 */
BN_EXPORT extern const fastf_t bn_radtodeg;


/*----------------------------------------------------------------------*/


/* tabdata.c */

/**
 * T A B D A T A
 *
 * Data structures to assist with recording many sets of data sampled
 * along the same set of independent variables.
 *
 * The overall notion is that each sample should be as compact as
 * possible (an array of measurements), with all the context stored in
 * one place.
 *
 * These structures and support routines apply to any measured "curve"
 * or "function" or "table" with one independent variable and one or
 * more scalar dependent variable(s).
 *
 * The context is kept in an 'bn_table' structure, and the data for
 * one particular sample are kept in an 'bn_tabdata' structure.
 *
 * The contents of the sample in val[j] are interpreted in the
 * interval (wavel[j]..wavel[j+1]).  This value could be power,
 * albedo, absorption, refractive index, or any other
 * wavelength-specific parameter.
 *
 * For example, if the val[] array contains power values, then val[j]
 * contains the integral of the power from wavel[j] to wavel[j+1]
 *
 * As an example, assume nwave=2, wavel[0]=500, wavel[1]=600, wavel[2]=700.
 * Then val[0] would contain data for the 500 to 600nm interval,
 * and val[1] would contain data for the 600 to 700nm interval.
 * There would be no storage allocated for val[2] -- don't use it!
 * There are several interpretations of this:
 *	1)  val[j] stores the total (integral, area) value for the interval, or
 *	2)  val[j] stores the average value across the interval.
 *
 * The intervals need not be uniformly spaced; it is acceptable to
 * increase wavelength sampling density around "important"
 * frequencies.
 *
 */

struct bn_table {
    uint32_t magic;
    size_t nx;
    fastf_t x[1];	/**< @brief array of nx+1 wavelengths, dynamically sized */
};

#define BN_CK_TABLE(_p)	BU_CKMAG(_p, BN_TABLE_MAGIC, "bn_table")
#define BN_TABLE_NULL	((struct bn_table *)NULL)

/* Gets an bn_table, with x[] having size _nx+1 */
#ifndef NO_BOMBING_MACROS
#  define BN_GET_TABLE(_table, _nx) { \
	if ((_nx) < 1)  bu_bomb("RT_GET_TABLE() _nx < 1\n"); \
	_table = (struct bn_table *)bu_calloc(1, \
					      sizeof(struct bn_table) + sizeof(fastf_t)*(_nx), \
					      "struct bn_table"); \
	_table->magic = BN_TABLE_MAGIC; \
	_table->nx = (_nx);  }
#else
#  define BN_GET_TABLE(_table, _nx) { \
	_table = (struct bn_table *)bu_calloc(1, \
					      sizeof(struct bn_table) + sizeof(fastf_t)*(_nx), \
					      "struct bn_table"); \
	_table->magic = BN_TABLE_MAGIC; \
	_table->nx = (_nx);  }
#endif

struct bn_tabdata {
    uint32_t magic;
    size_t ny;
    const struct bn_table *table;	/**< @brief Up pointer to definition of X axis */
    fastf_t y[1];			/**< @brief array of ny samples, dynamically sized */
};
#define BN_CK_TABDATA(_p)	BU_CKMAG(_p, BN_TABDATA_MAGIC, "bn_tabdata")
#define BN_TABDATA_NULL		((struct bn_tabdata *)NULL)

#define BN_SIZEOF_TABDATA_Y(_tabdata)	sizeof(fastf_t)*((_tabdata)->ny)
#define BN_SIZEOF_TABDATA(_table)	(sizeof(struct bn_tabdata) + \
					 sizeof(fastf_t)*((_table)->nx-1))

/* Gets an bn_tabdata, with y[] having size _ny */
#define BN_GET_TABDATA(_data, _table) { \
	BN_CK_TABLE(_table);\
	_data = (struct bn_tabdata *)bu_calloc(1, \
					       BN_SIZEOF_TABDATA(_table), "struct bn_tabdata"); \
	_data->magic = BN_TABDATA_MAGIC; \
	_data->ny = (_table)->nx; \
	_data->table = (_table); }

/*
 * Routines
 */


/*
 *			B N _ T A B L E _ F R E E
 */
BN_EXPORT extern void bn_table_free(struct bn_table *tabp);

/*
 *			B N _ T A B D A T A _ F R E E
 */
BN_EXPORT extern void bn_tabdata_free(struct bn_tabdata *data);

/*
 *			B N _ C K _ T A B L E
 */
BN_EXPORT extern void bn_ck_table(const struct bn_table *tabp);

/*
 *			B N _ T A B L E _ M A K E _ U N I F O R M
 *@brief
 *  Set up an independent "table margin" from 'first' to 'last',
 *  inclusive, using 'num' uniformly spaced samples.  Num >= 1.
 */
BN_EXPORT extern struct bn_table *bn_table_make_uniform(size_t num,
							double first,
							double last);

/*
 *			B N _ T A B D A T A _ A D D
 *@brief
 *  Sum the values from two data tables.
 */
BN_EXPORT extern void bn_tabdata_add(struct bn_tabdata *out,
				     const struct bn_tabdata *in1,
				     const struct bn_tabdata *in2);

/*
 *			B N _ T A B D A T A _ M U L
 *@brief
 *  Element-by-element multiply the values from two data tables.
 */
BN_EXPORT extern void bn_tabdata_mul(struct bn_tabdata *out,
				     const struct bn_tabdata *in1,
				     const struct bn_tabdata *in2);

/*
 *			B N _ T A B D A T A _ M U L 3
 *@brief
 *  Element-by-element multiply the values from three data tables.
 */
BN_EXPORT extern void bn_tabdata_mul3(struct bn_tabdata *out,
				      const struct bn_tabdata *in1,
				      const struct bn_tabdata *in2,
				      const struct bn_tabdata *in3);

/*
 *			B N _ T A B D A T A _ I N C R _ M U L 3 _ S C A L E
 *@brief
 *  Element-by-element multiply the values from three data tables and a scalor.
 *
 *	out += in1 * in2 * in3 * scale
 */
BN_EXPORT extern void bn_tabdata_incr_mul3_scale(struct bn_tabdata *out,
						 const struct bn_tabdata *in1,
						 const struct bn_tabdata *in2,
						 const struct bn_tabdata *in3,
						 double scale);

/*
 *			B N _ T A B D A T A _ I N C R _ M U L 2 _ S C A L E
 *@brief
 *  Element-by-element multiply the values from two data tables and a scalor.
 *
 *	out += in1 * in2 * scale
 */
BN_EXPORT extern void bn_tabdata_incr_mul2_scale(struct bn_tabdata *out,
						 const struct bn_tabdata *in1,
						 const struct bn_tabdata *in2,
						 double scale);

/*
 *			B N _ T A B D A T A _ S C A L E
 *@brief
 *  Multiply every element in a data table by a scalar value 'scale'.
 */
BN_EXPORT extern void bn_tabdata_scale(struct bn_tabdata *out,
				       const struct bn_tabdata *in1,
				       double scale);

/*
 *			B N _ T A B L E _ S C A L E
 *@brief
 *  Scale the independent axis of a table by 'scale'.
 */
BN_EXPORT extern void bn_table_scale(struct bn_table *tabp,
				     double scale);

/*
 *			B N _ T A B D A T A _ J O I N 1
 *@brief
 *  Multiply every element in data table in2 by a scalar value 'scale',
 *  add it to the element in in1, and store in 'out'.
 *  'out' may overlap in1 or in2.
 */
BN_EXPORT extern void bn_tabdata_join1(struct bn_tabdata *out,
				       const struct bn_tabdata *in1,
				       double scale,
				       const struct bn_tabdata *in2);

/*
 *			B N _ T A B D A T A _ J O I N 2
 *@brief
 *  Multiply every element in data table in2 by a scalar value 'scale2',
 *  plus in3 * scale3, and
 *  add it to the element in in1, and store in 'out'.
 *  'out' may overlap in1 or in2.
 */
BN_EXPORT extern void bn_tabdata_join2(struct bn_tabdata *out,
				       const struct bn_tabdata *in1,
				       double scale2,
				       const struct bn_tabdata *in2,
				       double scale3,
				       const struct bn_tabdata *in3);

/*
 *			B N _ T A B D A T A _ B L E N D 2
 */
BN_EXPORT extern void bn_tabdata_blend2(struct bn_tabdata *out,
					double scale1,
					const struct bn_tabdata *in1,
					double scale2,
					const struct bn_tabdata *in2);

/*
 *			B N _ T A B D A T A _ B L E N D 3
 */
BN_EXPORT extern void bn_tabdata_blend3(struct bn_tabdata *out,
					double scale1,
					const struct bn_tabdata *in1,
					double scale2,
					const struct bn_tabdata *in2,
					double scale3,
					const struct bn_tabdata *in3);

/*
 *			B N _ T A B D A T A _ A R E A 1
 *@brief
 *  Following interpretation #1, where y[j] stores the total (integral
 *  or area) value within the interval, return the area under the whole curve.
 *  This is simply totaling up the areas from each of the intervals.
 */
BN_EXPORT extern double bn_tabdata_area1(const struct bn_tabdata *in);

/*
 *			B N _ T A B D A T A _ A R E A 2
 *@brief
 *  Following interpretation #2, where y[j] stores the average
 *  value for the interval, return the area under
 *  the whole curve.  Since the interval spacing need not be uniform,
 *  sum the areas of the rectangles.
 */
BN_EXPORT extern double bn_tabdata_area2(const struct bn_tabdata *in);

/*
 *			B N _ T A B D A T A _ M U L _ A R E A 1
 *@brief
 *  Following interpretation #1, where y[j] stores the total (integral
 *  or area) value within the interval, return the area under the whole curve.
 *  This is simply totaling up the areas from each of the intervals.
 *  The curve value is found by multiplying corresponding entries from
 *  in1 and in2.
 */
BN_EXPORT extern double bn_tabdata_mul_area1(const struct bn_tabdata *in1,
					     const struct bn_tabdata *in2);

/*
 *			B N _ T A B D A T A _ M U L _ A R E A 2
 *@brief
 *  Following interpretation #2,
 *  return the area under the whole curve.
 *  The curve value is found by multiplying corresponding entries from
 *  in1 and in2.
 */
BN_EXPORT extern double bn_tabdata_mul_area2(const struct bn_tabdata *in1,
					     const struct bn_tabdata *in2);

/*
 *			B N _ T A B L E _ L I N _ I N T E R P
 *@brief
 *  Return the value of the curve at independent parameter value 'wl'.
 *  Linearly interpolate between values in the input table.
 *  Zero is returned for values outside the sampled range.
 */
BN_EXPORT extern fastf_t bn_table_lin_interp(const struct bn_tabdata *samp,
					     double wl);

/*
 *			B N _ T A B D A T A _ R E S A M P L E _ M A X
 *@brief
 *  Given a set of sampled data 'olddata', resample it for different
 *  spacing, by linearly interpolating the values when an output span
 *  is entirely contained within an input span, and by taking the
 *  maximum when an output span covers more than one input span.
 *
 *  This assumes interpretation (2) of the data, i.e. that the values
 *  are the average value across the interval.
 */
BN_EXPORT extern struct bn_tabdata *bn_tabdata_resample_max(const struct bn_table *newtable,
							    const struct bn_tabdata *olddata);

/*
 *			B N _ T A B D A T A _ R E S A M P L E _ A V G
 *@brief
 *  Given a set of sampled data 'olddata', resample it for different
 *  spacing, by linearly interpolating the values when an output span
 *  is entirely contained within an input span, and by taking the
 *  average when an output span covers more than one input span.
 *
 *  This assumes interpretation (2) of the data, i.e. that the values
 *  are the average value across the interval.
 */
BN_EXPORT extern struct bn_tabdata *bn_tabdata_resample_avg(const struct bn_table *newtable,
							    const struct bn_tabdata *olddata);

/*
 *			B N _ T A B L E _ W R I T E
 *@brief
 *  Write out the table structure in an ASCII file,
 *  giving the number of values (minus 1), and the
 *  actual values.
 */
BN_EXPORT extern int bn_table_write(const char *filename,
				    const struct bn_table *tabp);

/*
 *			B N _ T A B L E _ R E A D
 *@brief
 *  Allocate and read in the independent variable values from an ASCII file,
 *  giving the number of samples (minus 1), and the
 *  actual values.
 */
BN_EXPORT extern struct bn_table *bn_table_read(const char *filename);

/*
 *			B N _ P R _ T A B L E
 */
BN_EXPORT extern void bn_pr_table(const char *title,
				  const struct bn_table *tabp);

/*
 *			B N _ P R _ T A B D A T A
 */
BN_EXPORT extern void bn_pr_tabdata(const char *title,
				    const struct bn_tabdata *data);

/*
 *			B N _ P R I N T _ T A B L E _ A N D _ T A B D A T A
 *@brief
 *  Write out a given data table into an ASCII file,
 *  suitable for input to GNUPLOT.
 *
 *	(set term postscript)
 *	(set output "|print-postscript")
 *	(plot "filename" with lines)
 */
BN_EXPORT extern int bn_print_table_and_tabdata(const char *filename,
						const struct bn_tabdata *data);

/*
 *			B N _ R E A D _ T A B L E _ A N D _ T A B D A T A
 *@brief
 *  Read in a file which contains two columns of numbers, the first
 *  column being the wavelength, the second column being the sample value
 *  at that wavelength.
 *
 *  A new bn_table structure and one bn_tabdata structure
 *  are created, a pointer to the bn_tabdata structure is returned.
 *  The final wavelength is guessed at.
 */
BN_EXPORT extern struct bn_tabdata *bn_read_table_and_tabdata(const char *filename);

/*
 *			B N _ T A B D A T A _ B I N A R Y _ R E A D
 */
BN_EXPORT extern struct bn_tabdata *bn_tabdata_binary_read(const char *filename,
							   size_t num,
							   const struct bn_table *tabp);

/*
 *			B N _ T A B D A T A _ M A L L O C _ A R R A Y
 *@brief
 *  Allocate storage for, and initialize, an array of 'num' data table
 *  structures.
 *  This subroutine is provided because the bn_tabdata structures
 *  are variable length.
 */
BN_EXPORT extern struct bn_tabdata *bn_tabdata_malloc_array(const struct bn_table *tabp,
							    size_t num);

/*
 *			B N _ T A B D A T A _ C O P Y
 */
BN_EXPORT extern void bn_tabdata_copy(struct bn_tabdata *out,
				      const struct bn_tabdata *in);

/*
 *			B N _ T A B D A T A _ D U P
 */
BN_EXPORT extern struct bn_tabdata *bn_tabdata_dup(const struct bn_tabdata *in);

/*
 *			B N _ T A B D A T A _ G E T _ C O N S T V A L
 *@brief
 *  For a given table, allocate and return a tabdata structure
 *  with all elements initialized to 'val'.
 */
BN_EXPORT extern struct bn_tabdata *bn_tabdata_get_constval(double val,
							    const struct bn_table *tabp);

/*
 *			B N _ T A B D A T A _ C O N S T V A L
 *@brief
 *  Set all the tabdata elements to 'val'
 */
BN_EXPORT extern void bn_tabdata_constval(struct bn_tabdata *data,
					  double val);

/*
 *			B N _ T A B D A T A _ T O _ T C L
 *@brief
 *  Convert an bn_tabdata/bn_table pair into a Tcl compatible string
 *  appended to a VLS.  It will have form:
 *	x {...} y {...} nx # ymin # ymax #
 */
BN_EXPORT extern void bn_tabdata_to_tcl(struct bu_vls *vp,
					const struct bn_tabdata *data);

/*
 *			B N _ T A B D A T A _ F R O M _ A R R A Y
 *@brief
 *  Given an array of (x, y) pairs, build the relevant bn_table and
 *  bn_tabdata structures.
 *
 *  The table is terminated by an x value <= 0.
 *  Consistent with the interpretation of the spans,
 *  invent a final span ending x value.
 */
BN_EXPORT extern struct bn_tabdata *bn_tabdata_from_array(const double *array);

/*
 *			B N _ T A B D A T A _ F R E Q _ S H I F T
 *@brief
 *  Shift the data by a constant offset in the independent variable
 *  (often frequency), interpolating new sample values.
 */
BN_EXPORT extern void bn_tabdata_freq_shift(struct bn_tabdata *out,
					    const struct bn_tabdata *in,
					    double offset);

/*
 *			B N _ T A B L E _ I N T E R V A L _ N U M _ S A M P L E S
 *@brief
 *  Returns number of sample points between 'low' and 'hi', inclusive.
 */
BN_EXPORT extern int bn_table_interval_num_samples(const struct bn_table *tabp,
						   double low,
						   double hi);

/*
 *			B N _ T A B L E _ D E L E T E _ S A M P L E _ P T S
 *@brief
 *  Remove all sampling points between subscripts i and j, inclusive.
 *  Don't bother freeing the tiny bit of storage at the end of the array.
 *  Returns number of points removed.
 */
BN_EXPORT extern int bn_table_delete_sample_pts(struct bn_table *tabp,
						unsigned int i,
						unsigned int j);

/*
 *			B N _ T A B L E _ M E R G E 2
 *@brief
 *  A new table is returned which has sample points at places from
 *  each of the input tables.
 */
BN_EXPORT extern struct bn_table *bn_table_merge2(const struct bn_table *a,
						  const struct bn_table *b);

/*
 *		B N _ T A B D A T A _ M K _ L I N E A R _ F I L T E R
 *@brief
 *  Create a filter to accept power in a given band.
 *  The first and last filter values will be in the range 0..1,
 *  while all the internal filter values will be 1.0,
 *  and all samples outside the given band will be 0.0.
 *
 *
 *  @return	NULL	if given band does not overlap input spectrum
 *  @return	tabdata*
 */
BN_EXPORT extern struct bn_tabdata *bn_tabdata_mk_linear_filter(const struct bn_table *spectrum,
								double lower_wavelen,
								double upper_wavelen);


/*----------------------------------------------------------------------*/


/* tri_tri.c */
/*
 * Tomas Möller's triangle/triangle intersection routines from the article
 *
 * "A Fast Triangle-Triangle Intersection Test",
 * Journal of Graphics Tools, 2(2), 1997
 */

BN_EXPORT extern int bn_tri_tri_isect_coplanar(point_t V0,
					       point_t V1,
					       point_t V2,
					       point_t U0,
					       point_t U1,
					       point_t U2,
					       int area_flag);

BN_EXPORT extern int bn_tri_tri_isect(point_t V0,
				      point_t V1,
				      point_t V2,
				      point_t U0,
				      point_t U1,
				      point_t U2);

BN_EXPORT extern int bn_tri_tri_isect_with_line(point_t V0,
						point_t V1,
						point_t V2,
						point_t U0,
						point_t U1,
						point_t U2,
						int *coplanar,
						point_t *isectpt1,
						point_t *isectp2);


/*----------------------------------------------------------------------*/

/* util.c */

/**
 * @brief
 * Find a 2D coordinate system for a set of co-planar 3D points
 *
 * Based on the planar normal and the vector from the center point to the
 * point furthest from that center, find vectors describing a 2D coordinate system.
 *
 * @param[out]  origin_pnt Origin of 2D coordinate system in 3 space
 * @param[out]  u_axis 3D vector describing the U axis of the 2D coordinate system in 3 space
 * @param[out]  v_axis 3D vector describing the V axis of the 2D coordinate system in 3 space
 * @param       points_3d Array of 3D points
 * @param       n the number of points in the input set
 * @return 0 if successful
 */
int
bn_coplanar_2d_coord_sys(point_t *origin_pnt, vect_t *u_axis, vect_t *v_axis, const point_t *points_3d, int n);

/**
 * @brief
 * Find 2D coordinates for a set of co-planar 3D points
 *
 * @param[out]  points_2d Array of parameterized 2D points
 * @param       origin_pnt Origin of 2D coordinate system in 3 space
 * @param       u_axis 3D vector describing the U axis of the 2D coordinate system in 3 space
 * @param       v_axis 3D vector describing the V axis of the 2D coordinate system in 3 space
 * @param       points_3d 3D input points
 * @param       n the number of points in the input set
 * @return 0 if successful
 */
int
bn_coplanar_3d_to_2d(point2d_t **points_2d, const point_t *origin_pnt,
                     const vect_t *u_axis, const vect_t *v_axis,
                     const point_t *points_3d, int n);

/**
 * @brief
 * Find 3D coordinates for a set of 2D points given a coordinate system
 *
 * @param[out]  points_3d Array of 3D points
 * @param       origin_pnt Origin of 2D coordinate system in 3 space
 * @param       u_axis 3D vector describing the U axis of the 2D coordinate system in 3 space
 * @param       v_axis 3D vector describing the V axis of the 2D coordinate system in 3 space
 * @param       points_2d 2D input points
 * @param       n the number of points in the input set
 * @return 0 if successful
 */
int
bn_coplanar_2d_to_3d(point_t **points_3d, const point_t *origin_pnt,
                     const vect_t *u_axis, const vect_t *v_axis,
                     const point2d_t *points_2d, int n);




/*----------------------------------------------------------------------*/


/* vlist.c */

#define BN_VLIST_CHUNK 35		/**< @brief 32-bit mach => just less than 1k */

/**
 * B N _ V L I S T
 *
 * Definitions for handling lists of vectors (really vertices, or
 * points) and polygons in 3-space.  Intended for common handling of
 * wireframe display information, in the full resolution that is
 * calculated in.
 *
 * On 32-bit machines, BN_VLIST_CHUNK of 35 results in bn_vlist
 * structures just less than 1k bytes.
 *
 * The head of the doubly linked list can be just a "struct bu_list"
 * head.
 *
 * To visit all the elements in the vlist:
 *	for (BU_LIST_FOR(vp, bn_vlist, hp)) {
 *		register int	i;
 *		register int	nused = vp->nused;
 *		register int	*cmd = vp->cmd;
 *		register point_t *pt = vp->pt;
 *		for (i = 0; i < nused; i++, cmd++, pt++) {
 *			access(*cmd, *pt);
 *			access(vp->cmd[i], vp->pt[i]);
 *		}
 *	}
 */
struct bn_vlist  {
    struct bu_list l;		/**< @brief magic, forw, back */
    size_t nused;		/**< @brief elements 0..nused active */
    int cmd[BN_VLIST_CHUNK];	/**< @brief VL_CMD_* */
    point_t pt[BN_VLIST_CHUNK];	/**< @brief associated 3-point/vect */
};
#define BN_VLIST_NULL	((struct bn_vlist *)0)
#define BN_CK_VLIST(_p) BU_CKMAG((_p), BN_VLIST_MAGIC, "bn_vlist")

/* should these be enum? -Erik */
/* Values for cmd[] */
#define BN_VLIST_LINE_MOVE	0
#define BN_VLIST_LINE_DRAW	1
#define BN_VLIST_POLY_START	2	/**< @brief pt[] has surface normal */
#define BN_VLIST_POLY_MOVE	3	/**< @brief move to first poly vertex */
#define BN_VLIST_POLY_DRAW	4	/**< @brief subsequent poly vertex */
#define BN_VLIST_POLY_END	5	/**< @brief last vert (repeats 1st), draw poly */
#define BN_VLIST_POLY_VERTNORM	6	/**< @brief per-vertex normal, for interpolation */
#define BN_VLIST_TRI_START	7	/**< @brief pt[] has surface normal */
#define BN_VLIST_TRI_MOVE	8	/**< @brief move to first triangle vertex */
#define BN_VLIST_TRI_DRAW	9	/**< @brief subsequent triangle vertex */
#define BN_VLIST_TRI_END	10	/**< @brief last vert (repeats 1st), draw poly */
#define BN_VLIST_TRI_VERTNORM	11	/**< @brief per-vertex normal, for interpolation */
#define BN_VLIST_POINT_DRAW	12	/**< @brief Draw a single point */
#define BN_VLIST_POINT_SIZE	13	/**< @brief specify point pixel size */
#define BN_VLIST_LINE_WIDTH	14	/**< @brief specify line pixel width */
#define BN_VLIST_CMD_MAX	14	/**< @brief Max command number */

/**
 * Applications that are going to use BN_ADD_VLIST and BN_GET_VLIST
 * are required to execute this macro once, on their _free_hd:
 * BU_LIST_INIT(&_free_hd);
 *
 * Note that BN_GET_VLIST and BN_FREE_VLIST are non-PARALLEL.
 */
#define BN_GET_VLIST(_free_hd, p) {\
	(p) = BU_LIST_FIRST(bn_vlist, (_free_hd)); \
	if (BU_LIST_IS_HEAD((p), (_free_hd))) { \
	    BU_ALLOC((p), struct bn_vlist); \
	    (p)->l.magic = BN_VLIST_MAGIC; \
	} else { \
	    BU_LIST_DEQUEUE(&((p)->l)); \
	} \
	(p)->nused = 0; \
    }

/** Place an entire chain of bn_vlist structs on the freelist _free_hd */
#define BN_FREE_VLIST(_free_hd, hd) { \
	BU_CK_LIST_HEAD((hd)); \
	BU_LIST_APPEND_LIST((_free_hd), (hd)); \
    }

#define BN_ADD_VLIST(_free_hd, _dest_hd, pnt, draw) { \
	register struct bn_vlist *_vp; \
	BU_CK_LIST_HEAD(_dest_hd); \
	_vp = BU_LIST_LAST(bn_vlist, (_dest_hd)); \
	if (BU_LIST_IS_HEAD(_vp, (_dest_hd)) || _vp->nused >= BN_VLIST_CHUNK) { \
	    BN_GET_VLIST(_free_hd, _vp); \
	    BU_LIST_INSERT((_dest_hd), &(_vp->l)); \
	} \
	VMOVE(_vp->pt[_vp->nused], (pnt)); \
	_vp->cmd[_vp->nused++] = (draw); \
    }

/** Set a point size to apply to the vlist elements that follow. */
#define BN_VLIST_SET_POINT_SIZE(_free_hd, _dest_hd, _size) { \
	struct bn_vlist *_vp; \
	BU_CK_LIST_HEAD(_dest_hd); \
	_vp = BU_LIST_LAST(bn_vlist, (_dest_hd)); \
	if (BU_LIST_IS_HEAD(_vp, (_dest_hd)) || _vp->nused >= BN_VLIST_CHUNK) { \
	    BN_GET_VLIST(_free_hd, _vp); \
	    BU_LIST_INSERT((_dest_hd), &(_vp->l)); \
	} \
	_vp->pt[_vp->nused][0] = (_size); \
	_vp->cmd[_vp->nused++] = BN_VLIST_POINT_SIZE; \
}

/** Set a line width to apply to the vlist elements that follow. */
#define BN_VLIST_SET_LINE_WIDTH(_free_hd, _dest_hd, _width) { \
	struct bn_vlist *_vp; \
	BU_CK_LIST_HEAD(_dest_hd); \
	_vp = BU_LIST_LAST(bn_vlist, (_dest_hd)); \
	if (BU_LIST_IS_HEAD(_vp, (_dest_hd)) || _vp->nused >= BN_VLIST_CHUNK) { \
	    BN_GET_VLIST(_free_hd, _vp); \
	    BU_LIST_INSERT((_dest_hd), &(_vp->l)); \
	} \
	_vp->pt[_vp->nused][0] = (_width); \
	_vp->cmd[_vp->nused++] = BN_VLIST_LINE_WIDTH; \
}

/**
 * B N _ V L B L O C K
 *
 * For plotting, a way of separating plots into separate color vlists:
 * blocks of vlists, each with an associated color.
 */
struct bn_vlblock {
    uint32_t magic;
    size_t nused;
    size_t max;
    long *rgb;		/**< @brief rgb[max] variable size array */
    struct bu_list *head;		/**< @brief head[max] variable size array */
    struct bu_list *free_vlist_hd;	/**< @brief where to get/put free vlists */
};
#define BN_CK_VLBLOCK(_p)	BU_CKMAG((_p), BN_VLBLOCK_MAGIC, "bn_vlblock")


/** @addtogroup vlist */
/** @{ */
/** @file libbn/font.c
 *
 */

/**
 * Convert a string to a vlist.
 *
 * 'scale' is the width, in mm, of one character.
 *
 * @param vhead
 * @param free_hd source of free vlists
 * @param string  string of chars to be plotted
 * @param origin	 lower left corner of 1st char
 * @param rot	 Transform matrix (WARNING: may xlate)
 * @param scale    scale factor to change 1x1 char sz
 *
 */
BN_EXPORT extern void bn_vlist_3string(struct bu_list *vhead,
				       struct bu_list *free_hd,
				       const char *string,
				       const point_t origin,
				       const mat_t rot,
				       double scale);

/**
 * Convert string to vlist in 2D
 *
 * A simpler interface, for those cases where the text lies in the X-Y
 * plane.
 *
 * @param vhead
 * @param free_hd	source of free vlists
 * @param string	string of chars to be plotted
 * @param x		lower left corner of 1st char
 * @param y		lower left corner of 1st char
 * @param scale		scale factor to change 1x1 char sz
 * @param theta 	degrees ccw from X-axis
 *
 */
BN_EXPORT extern void bn_vlist_2string(struct bu_list *vhead,
				       struct bu_list *free_hd,
				       const char *string,
				       double x,
				       double y,
				       double scale,
				       double theta);

/** @} */


/*----------------------------------------------------------------------*/


/* vert_tree.c */
/*
 * vertex tree support
 */

/** @addtogroup vtree */
/** @{ */
/**
 * @brief
 *
 * Routines to manage a binary search tree of vertices.
 *
 * The actual vertices are stored in an array
 * for convenient use by routines such as "mk_bot".
 * The binary search tree stores indices into the array.
 *
 */


/**
 * packaging structure
 * holds all the required info for a single vertex tree
 */
struct vert_root {
    uint32_t magic;
    int tree_type;		/**< @brief vertices or vertices with normals */
    union vert_tree *the_tree;	/**< @brief the actual vertex tree */
    fastf_t *the_array;		/**< @brief the array of vertices */
    size_t curr_vert;		/**< @brief the number of vertices currently in the array */
    size_t max_vert;		/**< @brief the current maximum capacity of the array */
};

#define TREE_TYPE_VERTS 1
#define TREE_TYPE_VERTS_AND_NORMS 2

#define VERT_BLOCK 512			/**< @brief number of vertices to malloc per call when building the array */

#define BN_CK_VERT_TREE(_p) BU_CKMAG(_p, VERT_TREE_MAGIC, "vert_tree")


/**		C R E A T E _ V E R T _ T R E E
 *@brief
 *	routine to create a vertex tree.
 *
 *	Possible refinements include specifying an initial size
 */
BN_EXPORT extern struct vert_root *create_vert_tree();

/**		C R E A T E _ V E R T _ T R E E _ W _ N O R M S
 *@brief
 *	routine to create a vertex tree.
 *
 *	Possible refinements include specifying an initial size
 */
BN_EXPORT extern struct vert_root *create_vert_tree_w_norms();

/**		F R E E _ V E R T_ T R E E
 *@brief
 *	Routine to free a vertex tree and all associated dynamic memory
 */
BN_EXPORT extern void free_vert_tree(struct vert_root *tree_root);

/**		A D D _ V E R T
 *@brief
 *	Routine to add a vertex to the current list of part vertices.
 *	The array is re-alloc'd if needed.
 *	Returns index into the array of vertices where this vertex is stored
 */
BN_EXPORT extern int Add_vert(double x,
			      double y,
			      double z,
			      struct vert_root *tree_root,
			      fastf_t local_tol_sq);

/**		A D D _ V E R T _ A N D _ N O R M
 *@brief
 *	Routine to add a vertex and a normal to the current list of part vertices.
 *	The array is re-alloc'd if needed.
 *	Returns index into the array of vertices where this vertex and normal is stored
 */
BN_EXPORT extern int Add_vert_and_norm(double x,
				       double y,
				       double z,
				       double nx,
				       double ny,
				       double nz,
				       struct vert_root *tree_root,
				       fastf_t local_tol_sq);

/**		C L E A N _ V E R T _ T R E E
 *@brief
 *	Routine to free the binary search tree and reset the current number of vertices.
 *	The vertex array is left untouched, for re-use later.
 */
BN_EXPORT extern void clean_vert_tree(struct vert_root *tree_root);


/*----------------------------------------------------------------------*/


/* vectfont.c */

/** @addtogroup plot */
/** @{ */
/**
 *
 *	Terminal Independent Graphics Display Package.
 *		Mike Muuss  July 31, 1978
 *
 *	This routine is used to plot a string of ASCII symbols
 *  on the plot being generated, using a built-in set of fonts
 *  drawn as vector lists.
 *
 *	Internally, the basic font resides in a 10x10 unit square.
 *  Externally, each character can be thought to occupy one square
 *  plotting unit;  the 'scale'
 *  parameter allows this to be changed as desired, although scale
 *  factors less than 10.0 are unlikely to be legible.
 *
 *  The vector font table here was provided courtesy of Dr. Bruce
 *  Henrikson and Dr. Stephen Wolff, US Army Ballistic Research
 *  Laboratory, Summer of 1978.  They had developed it for their
 *  remote Houston Instruments pen plotter package for the
 *  GE Tymeshare system.
 *
 */
/** @} */


/**
 * @brief
 *  Once-only setup routine
 *  Used by libplot3/symbol.c, so it can't be static.
 */
BN_EXPORT extern void tp_setup();

/**
 * report version information about LIBBN
 */
BN_EXPORT extern const char *bn_version(void);

__END_DECLS


/*----------------------------------------------------------------------*/


/*
 * plane structures (from src/librt/plane.h)
 */

/**
 * Plane structures
 * holds all the required info for geometric planes.
 */

/* from src/librt/plane.h. */

#define MAXPTS 4			/* All we need are 4 points */
#define pl_A pl_points[0]		/* Synonym for A point */

struct plane_specific  {
    size_t pl_npts;			/* number of points on plane */
    point_t pl_points[MAXPTS];		/* Actual points on plane */
    vect_t pl_Xbasis;			/* X (B-A) vector (for 2d coords) */
    vect_t pl_Ybasis;			/* Y (C-A) vector (for 2d coords) */
    vect_t pl_N;			/* Unit-length Normal (outward) */
    fastf_t pl_NdotA;			/* Normal dot A */
    fastf_t pl_2d_x[MAXPTS];		/* X 2d-projection of points */
    fastf_t pl_2d_y[MAXPTS];		/* Y 2d-projection of points */
    fastf_t pl_2d_com[MAXPTS];		/* pre-computed common-term */
    struct plane_specific *pl_forw;	/* Forward link */
    char pl_code[MAXPTS+1];		/* Face code string.  Decorative. */
};

/*
 * Describe the tri_specific structure.
 */
struct tri_specific  {
    point_t tri_A;			/* triangle vertex (A) */
    vect_t tri_BA;			/* B - A (second point) */
    vect_t tri_CA;			/* C - A (third point) */
    vect_t tri_wn;			/* facet normal (non-unit) */
    vect_t tri_N;			/* unit normal vector */
    fastf_t *tri_normals;		/* unit vertex normals A, B, C  (this is malloced storage) */
    int tri_surfno;			/* solid specific surface number */
    struct tri_specific *tri_forw;	/* Next facet */
};

typedef struct tri_specific tri_specific_double;

/*
 * A more memory conservative version
 */
struct tri_float_specific  {
    float tri_A[3];			/* triangle vertex (A) */
    float tri_BA[3];			/* B - A (second point) */
    float tri_CA[3];			/* C - A (third point) */
    float tri_wn[3];			/* facet normal (non-unit) */
    float tri_N[3];			/* unit normal vector */
    signed char *tri_normals;		/* unit vertex normals A, B, C  (this is malloced storage) */
    int tri_surfno;			/* solid specific surface number */
    struct tri_float_specific *tri_forw;/* Next facet */
};

typedef struct tri_float_specific tri_specific_float;

#endif /* __BN_H__ */

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
