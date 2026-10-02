/*                    A S S E M B L Y . H
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 */
/** @addtogroup libanalyze */
/** @{ */
/** @file analyze/assembly.h
 * Kinematic assembly bodies, joints, and inverse-kinematics solving.
 *
 * Body poses and joint frames are rigid 4x4 matrices.  Frames transform body
 * local coordinates into joint coordinates.  Joint axes use local +Z.  Angle
 * values are radians; linear values use the caller's model units.
 */

#ifndef ANALYZE_ASSEMBLY_H
#define ANALYZE_ASSEMBLY_H

#include <stddef.h>

#include "common.h"

#include "analyze/defines.h"
#include "vmath.h"

__BEGIN_DECLS

#define ANALYZE_ASSEMBLY_BODY_NONE ((size_t)-1)

enum analyze_assembly_result {
    ANALYZE_ASSEMBLY_OK = 0,
    ANALYZE_ASSEMBLY_ERROR,
    ANALYZE_ASSEMBLY_INVALID_ARGUMENT,
    ANALYZE_ASSEMBLY_NOT_FOUND,
    ANALYZE_ASSEMBLY_LIMIT,
    ANALYZE_ASSEMBLY_DISCONNECTED,
    ANALYZE_ASSEMBLY_CYCLE,
    ANALYZE_ASSEMBLY_COLLISION,
    ANALYZE_ASSEMBLY_NO_CONVERGENCE
};

enum analyze_assembly_joint_type {
    ANALYZE_ASSEMBLY_FIXED = 0,
    ANALYZE_ASSEMBLY_REVOLUTE,
    ANALYZE_ASSEMBLY_PRISMATIC,
    ANALYZE_ASSEMBLY_COAXIAL,
    ANALYZE_ASSEMBLY_SPHERICAL,
    ANALYZE_ASSEMBLY_SCREW,
    ANALYZE_ASSEMBLY_PLANAR,
    ANALYZE_ASSEMBLY_TANGENT
};

struct analyze_assembly;

struct analyze_assembly_solve_result {
    size_t reachable_body_count;
    size_t disconnected_body_count;
    size_t cycle_count;
    size_t collision_count;
    size_t iteration_count;
    fastf_t max_position_error;
    fastf_t max_angular_error;
};

struct analyze_assembly_ik_options {
    size_t max_iterations;
    fastf_t position_tolerance;
    fastf_t angular_tolerance;
    fastf_t damping;
    fastf_t orientation_weight;
    fastf_t linear_step;
    fastf_t angular_step;
    int check_collisions;
};

typedef int (*analyze_assembly_collision_callback_t)(
    void *callback_data,
    size_t body_a,
    size_t body_b,
    const mat_t pose_a,
    const mat_t pose_b);

ANALYZE_EXPORT extern struct analyze_assembly *analyze_assembly_create(void);
ANALYZE_EXPORT extern void analyze_assembly_destroy(struct analyze_assembly *assembly);
ANALYZE_EXPORT extern void analyze_assembly_ik_options_init(struct analyze_assembly_ik_options *options);
ANALYZE_EXPORT extern void analyze_assembly_solve_result_init(struct analyze_assembly_solve_result *result);
ANALYZE_EXPORT extern const char *analyze_assembly_result_message(int result);
ANALYZE_EXPORT extern const char *analyze_assembly_joint_type_name(int type);
ANALYZE_EXPORT extern int analyze_assembly_joint_type_from_name(const char *name, int *type);
ANALYZE_EXPORT extern size_t analyze_assembly_joint_dof_count(int type);

ANALYZE_EXPORT extern int analyze_assembly_add_body(
    struct analyze_assembly *assembly,
    const char *name,
    const mat_t pose,
    int fixed,
    size_t *body_index);
ANALYZE_EXPORT extern int analyze_assembly_set_body_transform(
    struct analyze_assembly *assembly,
    size_t body_index,
    const mat_t pose);
ANALYZE_EXPORT extern int analyze_assembly_get_body_transform(
    const struct analyze_assembly *assembly,
    size_t body_index,
    mat_t pose);

ANALYZE_EXPORT extern int analyze_assembly_add_joint(
    struct analyze_assembly *assembly,
    const char *name,
    int type,
    size_t body_a,
    size_t body_b,
    const mat_t frame_a,
    const mat_t frame_b,
    size_t *joint_index);
ANALYZE_EXPORT extern int analyze_assembly_set_joint_value(
    struct analyze_assembly *assembly,
    size_t joint_index,
    size_t dof,
    fastf_t value);
ANALYZE_EXPORT extern int analyze_assembly_get_joint_value(
    const struct analyze_assembly *assembly,
    size_t joint_index,
    size_t dof,
    fastf_t *value);
/**
 * Mark a degree of freedom as an explicit input (nonzero) or a passive
 * coordinate (zero).  Passive coordinates are adjusted by
 * analyze_assembly_solve_constraints to satisfy closed-loop constraints.
 */
ANALYZE_EXPORT extern int analyze_assembly_set_joint_dof_driven(
    struct analyze_assembly *assembly,
    size_t joint_index,
    size_t dof,
    int driven);
ANALYZE_EXPORT extern int analyze_assembly_get_joint_dof_driven(
    const struct analyze_assembly *assembly,
    size_t joint_index,
    size_t dof,
    int *driven);
ANALYZE_EXPORT extern int analyze_assembly_set_joint_limit(
    struct analyze_assembly *assembly,
    size_t joint_index,
    size_t dof,
    fastf_t lower,
    fastf_t upper);
ANALYZE_EXPORT extern int analyze_assembly_set_screw_pitch(
    struct analyze_assembly *assembly,
    size_t joint_index,
    fastf_t pitch);
ANALYZE_EXPORT extern int analyze_assembly_set_tangent_distance(
    struct analyze_assembly *assembly,
    size_t joint_index,
    fastf_t distance);

ANALYZE_EXPORT extern int analyze_assembly_set_collision_callback(
    struct analyze_assembly *assembly,
    analyze_assembly_collision_callback_t callback,
    void *callback_data);

/** Propagate joint coordinates from root_body through an assembly tree. */
ANALYZE_EXPORT extern int analyze_assembly_solve_forward(
    struct analyze_assembly *assembly,
    size_t root_body,
    struct analyze_assembly_solve_result *result);

/**
 * Propagate an assembly and numerically adjust passive coordinates to close
 * every kinematic loop.  This is also suitable for tree assemblies.
 */
ANALYZE_EXPORT extern int analyze_assembly_solve_constraints(
    struct analyze_assembly *assembly,
    size_t root_body,
    const struct analyze_assembly_ik_options *options,
    struct analyze_assembly_solve_result *result);

/** Solve a requested world-space pose while preserving closed-loop constraints. */
ANALYZE_EXPORT extern int analyze_assembly_solve_inverse(
    struct analyze_assembly *assembly,
    size_t root_body,
    size_t target_body,
    const mat_t target_pose,
    const struct analyze_assembly_ik_options *options,
    struct analyze_assembly_solve_result *result);

/** Measure joint residuals without changing body poses. */
ANALYZE_EXPORT extern int analyze_assembly_evaluate(
    const struct analyze_assembly *assembly,
    struct analyze_assembly_solve_result *result);

__END_DECLS

#endif /* ANALYZE_ASSEMBLY_H */

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
