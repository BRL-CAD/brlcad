/*                        M A T . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2022 United States Government as represented by
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

/** @addtogroup bn_mat
 *
 *  @brief
 *  Matrix and vector functionality
 */
/** @{ */
/* @file mat.h */

#ifndef BN_MAT_H
#define BN_MAT_H

#include "common.h"
#include <stdio.h> /* For FILE */
#include "vmath.h"
#include "bu/vls.h"
#include "bn/defines.h"
#include "bn/tol.h"

__BEGIN_DECLS

/*
 * 4x4 Matrix math
 */

BN_EXPORT extern const mat_t bn_mat_identity;

BN_EXPORT extern void bn_mat_print(const char *title,
				   const mat_t m);

BN_EXPORT extern void bn_mat_print_guts(const char *title,
					const mat_t m,
					char *buf,
					int buflen);

BN_EXPORT extern void bn_mat_print_vls(const char *title,
				       const mat_t m,
				       struct bu_vls *vls);

/**
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
 * Multiply matrix "a" by "b" and store the result in "o".
 *
 * This is different from multiplying "b" by "a" (most of the time!)
 * Also, "o" must not be the same as either of the inputs.
 */
BN_EXPORT extern void bn_mat_mul(mat_t o,
				 const mat_t a,
				 const mat_t b);

/**
 * o = i * o
 *
 * A convenience wrapper for bn_mat_mul() to update a matrix in place.
 * The argument ordering is confusing either way.
 */
BN_EXPORT extern void bn_mat_mul2(const mat_t i,
				  mat_t o);

/**
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
 * Multiply the matrix "im" by the vector "iv" and store the result in
 * the vector "ov".  Note this is post-multiply, and operates on
 * 4-tuples.  Use MAT4X3VEC() to operate on 3-tuples.
 */
BN_EXPORT extern void bn_matXvec(hvect_t ov,
				 const mat_t im,
				 const hvect_t iv);

/**
 * The matrix pointed at by "input" is inverted and stored in the area
 * pointed at by "output".
 *
 * Calls bu_bomb if matrix is singular.
 */
BN_EXPORT extern void bn_mat_inv(mat_t output,
				 const mat_t input);

/**
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
 * Takes a pointer to a [x, y, z] vector, and a pointer to space for a
 * homogeneous vector [x, y, z, w], and builds [x, y, z, 1].
 */
BN_EXPORT extern void bn_vtoh_move(hvect_t h,
				   const vect_t v);

/**
 * Takes a pointer to [x, y, z, w], and converts it to an ordinary
 * vector [x/w, y/w, z/w].  Optimization for the case of w==1 is
 * performed.
 *
 * FIXME: make tolerance configurable
 */
BN_EXPORT extern void bn_htov_move(vect_t v,
				   const hvect_t h);

BN_EXPORT extern void bn_mat_trn(mat_t om,
				 const mat_t im);

/**
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
 * Find the azimuth and elevation angles that correspond to the
 * direction (not including twist) given by a direction vector.
 */
BN_EXPORT extern void bn_ae_vec(fastf_t *azp,
				fastf_t *elp,
				const vect_t v);

/**
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
 * Find a unit vector from the origin given azimuth and elevation.
 */
BN_EXPORT extern void bn_vec_ae(vect_t vec,
				fastf_t az,
				fastf_t el);

/**
 * Find a vector from the origin given azimuth, elevation, and distance.
 */
BN_EXPORT extern void bn_vec_aed(vect_t vec,
				 fastf_t az,
				 fastf_t el,
				 fastf_t dist);

/**
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
 * Given a vector, create another vector which is perpendicular to it.
 * The output vector will have unit length only if the input vector
 * did.
 *
 * FIXME: make tolerance configurable
 */
BN_EXPORT extern void bn_vec_perp(vect_t new_vec,
				  const vect_t old_vec);

/**
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
 * Given the sin and cos of an X rotation angle, produce the rotation
 * matrix.
 */
BN_EXPORT extern void bn_mat_xrot(mat_t m,
				  double sinx,
				  double cosx);

/**
 * Given the sin and cos of a Y rotation angle, produce the rotation
 * matrix.
 */
BN_EXPORT extern void bn_mat_yrot(mat_t m,
				  double siny,
				  double cosy);

/**
 * Given the sin and cos of a Z rotation angle, produce the rotation
 * matrix.
 */
BN_EXPORT extern void bn_mat_zrot(mat_t m,
				  double sinz,
				  double cosz);

/**
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
 * Given a vector, create another vector which is perpendicular to it,
 * and with unit length.  This algorithm taken from Gift's arvec.f; a
 * faster algorithm may be possible.
 *
 * FIXME: make tolerance configurable
 */
BN_EXPORT extern void bn_vec_ortho(vect_t out,
				   const vect_t in);

/**
 * Build a matrix to scale uniformly around a given point.
 *
 * @return -1 if scale is too small.
 * @return  0 if OK.
 *
 * FIXME: make tolerance configurable
 */
BN_EXPORT extern int bn_mat_scale_about_pnt(mat_t mat,
					    const point_t pnt,
					    const double scale);

/**
 * Build a matrix to apply arbitrary 4x4 transformation around a given
 * point.
 */
BN_EXPORT extern void bn_mat_xform_about_pnt(mat_t mat,
					    const mat_t xform,
					    const point_t pnt);

/**
 * @return 0 When matrices are not equal
 * @return 1 When matrices are equal
 */
BN_EXPORT extern int bn_mat_is_equal(const mat_t a,
				     const mat_t b,
				     const struct bn_tol *tol);

/**
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
 * Return a pointer to a copy of the matrix in dynamically allocated
 * memory.
 */
BN_EXPORT extern matp_t bn_mat_dup(const mat_t in);

/**
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
 * Calculates the determinant of the 3X3 "rotation" part of the passed
 * matrix.
 */
BN_EXPORT extern fastf_t bn_mat_det3(const mat_t m);

/**
 * Calculates the determinant of the 4X4 matrix
 */
BN_EXPORT extern fastf_t bn_mat_determinant(const mat_t m);

/**
 * FIXME: make tolerance configurable
 */
BN_EXPORT extern int bn_mat_is_non_unif(const mat_t m);

/**
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


/* Matrix stuff from MGED's dozoom.c - somewhat unfinished,
 * judging by the comments.  Can probably be simplified/combined */
BN_EXPORT extern void persp_mat(mat_t m, fastf_t fovy, fastf_t aspect, fastf_t near1, fastf_t far1, fastf_t backoff);
BN_EXPORT extern void mike_persp_mat(fastf_t *pmat, const fastf_t *eye);
BN_EXPORT extern void deering_persp_mat(fastf_t *m, const fastf_t *l, const fastf_t *h, const fastf_t *eye);


/* Option parser/validator for libbu's option system that will read in a matrix
 * from the argv entries.  set_var should be a matp_t
 *
 * NOTE:  In one sense this could be defined with the other option parsers in
 * bu/opt.h, as vmath.h (which defines the matrix type) is stand-alone, but
 * since it is defining a matrix it's a better overall conceptual fit with
 * libbn - hence it is defined as part of that API. */
BN_EXPORT extern int bn_opt_mat(struct bu_vls *msg, size_t argc, const char **argv, void *set_var);

__END_DECLS

#endif  /* BN_MAT_H */
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
