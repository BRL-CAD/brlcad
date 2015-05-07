/*                        A N I M . H
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

/*----------------------------------------------------------------------*/
/** @addtogroup anim
 * @brief
 * Routines useful for moving geometry.
 *
 * Orientation conventions: The default object orientation is facing
 * the positive x-axis, with the positive y-axis to the object's left
 * and the positive z-axis above the object.
 *
 * The default view orientation for rt and mged is facing the negative
 * z-axis, with the negative x-axis leading to the left and the
 * positive y-axis going upwards.
 */
/** @{ */
/** @file bn/anim.h */

#ifndef BN_ANIM_H
#define BN_ANIM_H

#include "common.h"
#include <stdio.h> /* For FILE */
#include "vmath.h"
#include "bn/defines.h"

__BEGIN_DECLS

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

__END_DECLS

#endif  /* BN_ANIM_H */
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
