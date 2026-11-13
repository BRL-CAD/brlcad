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
struct rt_joint_eval;
struct rt_joint_problem;

#define RT_JOINT_EVAL_OPTIONS_VERSION 1u
#define RT_JOINT_EVAL_OPTIONS_RESERVED 4u

struct rt_joint_eval_options {
    size_t size;
    uint32_t version;
    uint64_t flags;
    const char *root_link;
    const fastf_t *root_matrix;
    struct bu_vls *messages;
    void *reserved[RT_JOINT_EVAL_OPTIONS_RESERVED];
};

#define RT_JOINT_EVAL_OPTIONS_INIT { \
    sizeof(struct rt_joint_eval_options), RT_JOINT_EVAL_OPTIONS_VERSION, 0, \
    NULL, NULL, NULL, {NULL, NULL, NULL, NULL} \
}

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

/**
 * Evaluate a connected, acyclic joint graph.  Each child link may have only
 * one incoming joint.  A NULL root_matrix selects identity.  The result owns
 * its link names and matrices and must be released with
 * rt_joint_eval_destroy().
 */
RT_EXPORT extern int rt_joint_eval_create(struct rt_joint_eval **result,
					  const struct rt_joint_internal * const *joints,
					  size_t joint_count,
					  const struct rt_joint_eval_options *options);

RT_EXPORT extern void rt_joint_eval_destroy(struct rt_joint_eval *evaluation);
RT_EXPORT extern size_t rt_joint_eval_count(const struct rt_joint_eval *evaluation);
RT_EXPORT extern const char *rt_joint_eval_link(const struct rt_joint_eval *evaluation,
						size_t index);
RT_EXPORT extern int rt_joint_eval_matrix(mat_t result,
					 const struct rt_joint_eval *evaluation,
					 size_t index);
RT_EXPORT extern int rt_joint_eval_find(mat_t result,
				       const struct rt_joint_eval *evaluation,
				       const char *link);

#define RT_JOINT_POINT_CONSTRAINT_VERSION 1u
#define RT_JOINT_POINT_CONSTRAINT_RESERVED 4u

struct rt_joint_point_constraint {
    size_t size;
    uint32_t version;
    uint64_t flags;
    const char *link;
    point_t point;
    point_t target;
    fastf_t weight;
    void *reserved[RT_JOINT_POINT_CONSTRAINT_RESERVED];
};

#define RT_JOINT_POINT_CONSTRAINT_INIT { \
    sizeof(struct rt_joint_point_constraint), RT_JOINT_POINT_CONSTRAINT_VERSION, \
    0, NULL, VINIT_ZERO, VINIT_ZERO, 1.0, {NULL, NULL, NULL, NULL} \
}

/** Create a solver-neutral problem whose variables are the ordered joint DOFs. */
RT_EXPORT extern int rt_joint_problem_create(struct rt_joint_problem **result,
				      struct rt_joint_internal * const *joints,
				      size_t joint_count,
				      const struct rt_joint_eval_options *options);
RT_EXPORT extern void rt_joint_problem_destroy(struct rt_joint_problem *problem);
RT_EXPORT extern size_t rt_joint_problem_variable_count(const struct rt_joint_problem *problem);
RT_EXPORT extern size_t rt_joint_problem_residual_count(const struct rt_joint_problem *problem);
RT_EXPORT extern int rt_joint_problem_variable(const struct rt_joint_problem *problem,
				       size_t variable, size_t *joint, size_t *dof,
				       fastf_t *lower, fastf_t *upper);
RT_EXPORT extern int rt_joint_problem_values(const struct rt_joint_problem *problem,
				     fastf_t *values, size_t count);
RT_EXPORT extern int rt_joint_problem_set_values(struct rt_joint_problem *problem,
					 const fastf_t *values, size_t count);
RT_EXPORT extern int rt_joint_problem_add_point(struct rt_joint_problem *problem,
					const struct rt_joint_point_constraint *constraint);
/** Evaluate residuals and an optional row-major residual-by-variable Jacobian. */
RT_EXPORT extern int rt_joint_problem_evaluate(const struct rt_joint_problem *problem,
				       fastf_t *residuals, size_t residual_count,
				       fastf_t *jacobian, size_t jacobian_count);

__END_DECLS

#endif /* RT_PRIMITIVES_JOINT_H */
