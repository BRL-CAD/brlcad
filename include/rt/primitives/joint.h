/*                         J O I N T . H
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
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
/** @file rt/primitives/joint.h
 *
 * Database joint relationship definitions and evaluation.
 */

#ifndef RT_PRIMITIVES_JOINT_H
#define RT_PRIMITIVES_JOINT_H

#include "common.h"

#include "bu/vls.h"
#include "vmath.h"
#include "rt/defines.h"

__BEGIN_DECLS

#define RT_JOINT_MAX_DOF 6

enum rt_joint_type {
    RT_JOINT_GENERAL = 0,
    RT_JOINT_FIXED,
    RT_JOINT_REVOLUTE,
    RT_JOINT_PRISMATIC,
    RT_JOINT_CYLINDRICAL,
    RT_JOINT_UNIVERSAL,
    RT_JOINT_SPHERICAL,
    RT_JOINT_PLANAR,
    RT_JOINT_FLOATING
};

enum rt_joint_motion {
    RT_JOINT_ROTATION = 1,
    RT_JOINT_TRANSLATION
};

#define RT_JOINT_DOF_LIMITED 0x1u

struct rt_joint_internal;

RT_EXPORT extern const char *rt_joint_type_name(uint32_t type);
RT_EXPORT extern const char *rt_joint_motion_name(uint32_t motion);
RT_EXPORT extern int rt_joint_type_from_name(const char *name, uint32_t *type);
RT_EXPORT extern int rt_joint_motion_from_name(const char *name, uint32_t *motion);

/** Validate a joint relationship.  Returns the number of errors found. */
RT_EXPORT extern int rt_joint_validate(const struct rt_joint_internal *joint,
                                       struct bu_vls *messages);

/** Build the ordered DOF transform in the parent attachment frame. */
RT_EXPORT extern int rt_joint_motion_mat(mat_t result,
                                         const struct rt_joint_internal *joint);

/** Build the affine pose transform as translation * rotation * scale. */
RT_EXPORT extern int rt_joint_pose_mat(mat_t result,
                                       const struct rt_joint_internal *joint);

/**
 * Build the child-to-parent relationship matrix:
 * parent_frame * motion * pose * inverse(child_frame).
 */
RT_EXPORT extern int rt_joint_relative_mat(mat_t result,
                                           const struct rt_joint_internal *joint);

__END_DECLS

#endif /* RT_PRIMITIVES_JOINT_H */
