/*                  J O I N T _ M I R R O R . C
 * BRL-CAD
 *
 * Copyright (c) 2009-2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 */
/** @file primitives/joint/joint_mirror.c */

#include "common.h"

#include "raytrace.h"
#include "rt/geom.h"
#include "rt/primitives/joint.h"

int
rt_joint_mirror(struct rt_db_internal *ip, const plane_t plane)
{
    struct rt_joint_internal *joint;
    mat_t world_mirror = MAT_INIT_IDN;
    mat_t local_mirror = MAT_INIT_IDN;
    mat_t combined;
    mat_t result;
    vect_t normal;
    fastf_t magnitude;
    size_t row;
    size_t column;
    size_t i;

    RT_CK_DB_INTERNAL(ip);
    joint = (struct rt_joint_internal *)ip->idb_ptr;
    RT_JOINT_CK_MAGIC(joint);
    VMOVE(normal, plane);
    magnitude = MAGNITUDE(normal);
    if (magnitude <= SQRT_SMALL_FASTF)
	return BRLCAD_ERROR;
    VSCALE(normal, normal, 1.0 / magnitude);

    for (row = 0; row < 3; row++) {
	for (column = 0; column < 3; column++)
	    world_mirror[row * 4 + column] -= 2.0 * normal[row] * normal[column];
	world_mirror[row * 4 + 3] = 2.0 * (plane[W] / magnitude) * normal[row];
    }

    /* Restore a right-handed attachment coordinate system after reflection. */
    local_mirror[0] = -1.0;
    bn_mat_mul(combined, world_mirror, joint->parent_frame);
    bn_mat_mul(result, combined, local_mirror);
    MAT_COPY(joint->parent_frame, result);
    bn_mat_mul(combined, world_mirror, joint->child_frame);
    bn_mat_mul(result, combined, local_mirror);
    MAT_COPY(joint->child_frame, result);
    for (i = 0; i < joint->dof_count; i++)
	joint->dofs[i].axis[X] = -joint->dofs[i].axis[X];
    joint->pose_translation[X] = -joint->pose_translation[X];
    joint->pose_rotation[Y] = -joint->pose_rotation[Y];
    joint->pose_rotation[Z] = -joint->pose_rotation[Z];

    return rt_joint_validate(joint, NULL) ? BRLCAD_ERROR : BRLCAD_OK;
}

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
