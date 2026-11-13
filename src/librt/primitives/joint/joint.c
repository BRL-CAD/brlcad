/*                          J O I N T . C
 * BRL-CAD
 *
 * Copyright (c) 2014-2026 United States Government as represented by
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
/** @addtogroup primitives */
/** @{ */
/** @file primitives/joint/joint.c
 *
 * A joint is a non-geometric relationship between parent and child
 * instances.  Attachment frames are expressed in their respective body
 * coordinates.  Degrees of freedom are evaluated in order in the parent
 * attachment frame.
 */

#include "common.h"

#include <math.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "bio.h"
#include "bu/cv.h"
#include "bu/opt.h"
#include "bu/str.h"
#include "bn.h"
#include "nmg.h"
#include "raytrace.h"
#include "rt/db4.h"
#include "rt/geom.h"
#include "rt/primitives/joint.h"

#include "../../librt_private.h"

#define JOINT_EXT_MAGIC 0x4a4e5432u /* "JNT2" */
#define JOINT_EXT_VERSION 1u
#define JOINT_EXT_HEADER_LONGS 8u
#define JOINT_EXT_FRAME_DOUBLES (2u * ELEMENTS_PER_MAT)
#define JOINT_EXT_POSE_DOUBLES 10u
#define JOINT_EXT_DOF_LONGS 2u
#define JOINT_EXT_DOF_DOUBLES 6u
#define JOINT_LEGACY_DOUBLES 10u
#define JOINT_PLOT_AXIS_LENGTH 10.0

struct joint_type_info {
    const char *name;
    size_t dof_count;
    size_t rotations;
    size_t translations;
};

static const struct joint_type_info joint_types[] = {
    {"general",     0, 0, 0},
    {"fixed",       0, 0, 0},
    {"revolute",    1, 1, 0},
    {"prismatic",   1, 0, 1},
    {"cylindrical", 2, 1, 1},
    {"universal",   2, 2, 0},
    {"spherical",   3, 3, 0},
    {"planar",      3, 1, 2},
    {"floating",    6, 3, 3}
};

const char *
rt_joint_type_name(uint32_t type)
{
    if (type >= sizeof(joint_types) / sizeof(joint_types[0]))
	return NULL;
    return joint_types[type].name;
}

const char *
rt_joint_motion_name(uint32_t motion)
{
    if (motion == RT_JOINT_ROTATION)
	return "rotation";
    if (motion == RT_JOINT_TRANSLATION)
	return "translation";
    return NULL;
}

int
rt_joint_type_from_name(const char *name, uint32_t *type)
{
    uint32_t i;

    if (!name || !type)
	return BRLCAD_ERROR;
    for (i = 0; i < sizeof(joint_types) / sizeof(joint_types[0]); i++) {
	if (BU_STR_EQUAL(name, joint_types[i].name)) {
	    *type = i;
	    return BRLCAD_OK;
	}
    }
    return BRLCAD_ERROR;
}

int
rt_joint_motion_from_name(const char *name, uint32_t *motion)
{
    if (!name || !motion)
	return BRLCAD_ERROR;
    if (BU_STR_EQUAL(name, "rotation"))
	*motion = RT_JOINT_ROTATION;
    else if (BU_STR_EQUAL(name, "translation"))
	*motion = RT_JOINT_TRANSLATION;
    else
	return BRLCAD_ERROR;
    return BRLCAD_OK;
}

static void
joint_validation_message(struct bu_vls *messages, const char *format, ...)
{
    va_list args;

    if (!messages)
	return;
    if (bu_vls_strlen(messages))
	bu_vls_strcat(messages, "; ");
    va_start(args, format);
    bu_vls_vprintf(messages, format, args);
    va_end(args);
}

int
rt_joint_validate(const struct rt_joint_internal *joint, struct bu_vls *messages)
{
    struct bn_tol tol = BN_TOL_INIT_TOL;
    const struct joint_type_info *type_info = NULL;
    size_t rotations = 0;
    size_t translations = 0;
    size_t i;
    int errors = 0;

    if (!joint) {
	joint_validation_message(messages, "joint is NULL");
	return 1;
    }
    RT_JOINT_CK_MAGIC(joint);
    if (!rt_joint_type_name(joint->type)) {
	joint_validation_message(messages, "unknown joint type %u", joint->type);
	errors++;
    } else {
	type_info = &joint_types[joint->type];
    }
    if (joint->dof_count > RT_JOINT_MAX_DOF) {
	joint_validation_message(messages, "DOF count %zu exceeds %d", joint->dof_count, RT_JOINT_MAX_DOF);
	return errors + 1;
    }
    if (!bn_mat_is_rigid(joint->parent_frame, &tol)) {
	joint_validation_message(messages, "parent frame is not a rigid transform");
	errors++;
    }
    if (!bn_mat_is_rigid(joint->child_frame, &tol)) {
	joint_validation_message(messages, "child frame is not a rigid transform");
	errors++;
    }
    if (!isfinite(joint->pose_translation[X]) ||
	!isfinite(joint->pose_translation[Y]) ||
	!isfinite(joint->pose_translation[Z])) {
	joint_validation_message(messages, "pose translation is not finite");
	errors++;
    }
    if (!isfinite(joint->pose_rotation[X]) ||
	!isfinite(joint->pose_rotation[Y]) ||
	!isfinite(joint->pose_rotation[Z]) ||
	!isfinite(joint->pose_rotation[W]) ||
	!NEAR_EQUAL(QMAGNITUDE(joint->pose_rotation), 1.0, tol.perp)) {
	joint_validation_message(messages, "pose rotation is not a finite unit quaternion");
	errors++;
    }
    if (!isfinite(joint->pose_scale[X]) || !isfinite(joint->pose_scale[Y]) ||
	!isfinite(joint->pose_scale[Z])) {
	joint_validation_message(messages, "pose scale is not finite");
	errors++;
    }
    for (i = 0; i < joint->dof_count; i++) {
	const struct rt_joint_dof *dof = &joint->dofs[i];
	fastf_t axis_length = MAGNITUDE(dof->axis);

	if (dof->motion == RT_JOINT_ROTATION)
	    rotations++;
	else if (dof->motion == RT_JOINT_TRANSLATION)
	    translations++;
	else {
	    joint_validation_message(messages, "DOF %zu has unknown motion %u", i, dof->motion);
	    errors++;
	}
	if (dof->flags & ~RT_JOINT_DOF_LIMITED) {
	    joint_validation_message(messages, "DOF %zu has unknown flags 0x%x", i, dof->flags);
	    errors++;
	}
	if (!isfinite(dof->axis[X]) || !isfinite(dof->axis[Y]) ||
	    !isfinite(dof->axis[Z]) || !NEAR_EQUAL(axis_length, 1.0, tol.perp)) {
	    joint_validation_message(messages, "DOF %zu axis is not a finite unit vector", i);
	    errors++;
	}
	if (!isfinite(dof->lower) || !isfinite(dof->upper) || !isfinite(dof->current)) {
	    joint_validation_message(messages, "DOF %zu contains a non-finite value", i);
	    errors++;
	} else if (dof->flags & RT_JOINT_DOF_LIMITED) {
	    if (dof->lower > dof->upper) {
		joint_validation_message(messages, "DOF %zu lower limit exceeds upper limit", i);
		errors++;
	    } else if (dof->current < dof->lower || dof->current > dof->upper) {
		joint_validation_message(messages, "DOF %zu position is outside its limits", i);
		errors++;
	    }
	}
    }
    if (type_info && joint->type != RT_JOINT_GENERAL &&
	(joint->dof_count != type_info->dof_count ||
	 rotations != type_info->rotations || translations != type_info->translations)) {
	joint_validation_message(messages,
		"%s joint requires %zu DOFs (%zu rotational, %zu translational)",
		type_info->name, type_info->dof_count,
		type_info->rotations, type_info->translations);
	errors++;
    }
    return errors;
}

int
rt_joint_motion_mat(mat_t result, const struct rt_joint_internal *joint)
{
    size_t i;

    if (!result || !joint || rt_joint_validate(joint, NULL))
	return BRLCAD_ERROR;
    MAT_IDN(result);
    for (i = 0; i < joint->dof_count; i++) {
	const struct rt_joint_dof *dof = &joint->dofs[i];
	mat_t step;
	mat_t combined;

	MAT_IDN(step);
	if (dof->motion == RT_JOINT_ROTATION) {
	    point_t origin = VINIT_ZERO;
	    bn_mat_arb_rot(step, origin, dof->axis, dof->current);
	} else {
	    MAT_DELTAS(step, dof->axis[X] * dof->current,
		    dof->axis[Y] * dof->current,
		    dof->axis[Z] * dof->current);
	}
	bn_mat_mul(combined, result, step);
	MAT_COPY(result, combined);
    }
    return BRLCAD_OK;
}

int
rt_joint_pose_mat(mat_t result, const struct rt_joint_internal *joint)
{
    mat_t rotation;
    mat_t scale = MAT_INIT_IDN;

    if (!result || !joint || rt_joint_validate(joint, NULL))
	return BRLCAD_ERROR;
    anim_quat2mat(rotation, joint->pose_rotation);
    scale[0] = joint->pose_scale[X];
    scale[5] = joint->pose_scale[Y];
    scale[10] = joint->pose_scale[Z];
    bn_mat_mul(result, rotation, scale);
    MAT_DELTAS_VEC(result, joint->pose_translation);
    return BRLCAD_OK;
}

int
rt_joint_relative_mat(mat_t result, const struct rt_joint_internal *joint)
{
    mat_t motion;
    mat_t pose;
    mat_t child_inverse;
    mat_t combined;
    mat_t posed;

    if (!result || !joint || rt_joint_motion_mat(motion, joint) != BRLCAD_OK ||
	rt_joint_pose_mat(pose, joint) != BRLCAD_OK)
	return BRLCAD_ERROR;
    if (bn_mat_inverse(child_inverse, joint->child_frame) == 0)
	return BRLCAD_ERROR;
    bn_mat_mul(combined, joint->parent_frame, motion);
    bn_mat_mul(posed, combined, pose);
    bn_mat_mul(result, posed, child_inverse);
    return BRLCAD_OK;
}

static void
joint_init(struct rt_joint_internal *joint)
{
    memset(joint, 0, sizeof(*joint));
    joint->magic = RT_JOINT_INTERNAL_MAGIC;
    joint->type = RT_JOINT_FIXED;
    BU_VLS_INIT(&joint->parent_link);
    BU_VLS_INIT(&joint->child_link);
    BU_VLS_INIT(&joint->parent_path);
    BU_VLS_INIT(&joint->child_path);
    MAT_IDN(joint->parent_frame);
    MAT_IDN(joint->child_frame);
    QSET(joint->pose_rotation, 0.0, 0.0, 0.0, 1.0);
    VSETALL(joint->pose_scale, 1.0);
}

static void
joint_set_dof(struct rt_joint_dof *dof, uint32_t motion, fastf_t x, fastf_t y, fastf_t z)
{
    memset(dof, 0, sizeof(*dof));
    dof->motion = motion;
    VSET(dof->axis, x, y, z);
}

static void
joint_configure_type(struct rt_joint_internal *joint, uint32_t type)
{
    memset(joint->dofs, 0, sizeof(joint->dofs));
    joint->type = type;
    joint->dof_count = joint_types[type].dof_count;
    switch (type) {
	case RT_JOINT_REVOLUTE:
	    joint_set_dof(&joint->dofs[0], RT_JOINT_ROTATION, 0.0, 0.0, 1.0);
	    break;
	case RT_JOINT_PRISMATIC:
	    joint_set_dof(&joint->dofs[0], RT_JOINT_TRANSLATION, 0.0, 0.0, 1.0);
	    break;
	case RT_JOINT_CYLINDRICAL:
	    joint_set_dof(&joint->dofs[0], RT_JOINT_ROTATION, 0.0, 0.0, 1.0);
	    joint_set_dof(&joint->dofs[1], RT_JOINT_TRANSLATION, 0.0, 0.0, 1.0);
	    break;
	case RT_JOINT_UNIVERSAL:
	    joint_set_dof(&joint->dofs[0], RT_JOINT_ROTATION, 1.0, 0.0, 0.0);
	    joint_set_dof(&joint->dofs[1], RT_JOINT_ROTATION, 0.0, 1.0, 0.0);
	    break;
	case RT_JOINT_SPHERICAL:
	    joint_set_dof(&joint->dofs[0], RT_JOINT_ROTATION, 1.0, 0.0, 0.0);
	    joint_set_dof(&joint->dofs[1], RT_JOINT_ROTATION, 0.0, 1.0, 0.0);
	    joint_set_dof(&joint->dofs[2], RT_JOINT_ROTATION, 0.0, 0.0, 1.0);
	    break;
	case RT_JOINT_PLANAR:
	    joint_set_dof(&joint->dofs[0], RT_JOINT_TRANSLATION, 1.0, 0.0, 0.0);
	    joint_set_dof(&joint->dofs[1], RT_JOINT_TRANSLATION, 0.0, 1.0, 0.0);
	    joint_set_dof(&joint->dofs[2], RT_JOINT_ROTATION, 0.0, 0.0, 1.0);
	    break;
	case RT_JOINT_FLOATING:
	    joint_set_dof(&joint->dofs[0], RT_JOINT_TRANSLATION, 1.0, 0.0, 0.0);
	    joint_set_dof(&joint->dofs[1], RT_JOINT_TRANSLATION, 0.0, 1.0, 0.0);
	    joint_set_dof(&joint->dofs[2], RT_JOINT_TRANSLATION, 0.0, 0.0, 1.0);
	    joint_set_dof(&joint->dofs[3], RT_JOINT_ROTATION, 1.0, 0.0, 0.0);
	    joint_set_dof(&joint->dofs[4], RT_JOINT_ROTATION, 0.0, 1.0, 0.0);
	    joint_set_dof(&joint->dofs[5], RT_JOINT_ROTATION, 0.0, 0.0, 1.0);
	    break;
	default:
	    break;
    }
}

static void
joint_copy(struct rt_joint_internal *dest, const struct rt_joint_internal *source)
{
    joint_init(dest);
    dest->type = source->type;
    bu_vls_strcpy(&dest->parent_link, bu_vls_cstr(&source->parent_link));
    bu_vls_strcpy(&dest->child_link, bu_vls_cstr(&source->child_link));
    bu_vls_strcpy(&dest->parent_path, bu_vls_cstr(&source->parent_path));
    bu_vls_strcpy(&dest->child_path, bu_vls_cstr(&source->child_path));
    MAT_COPY(dest->parent_frame, source->parent_frame);
    MAT_COPY(dest->child_frame, source->child_frame);
    VMOVE(dest->pose_translation, source->pose_translation);
    HMOVE(dest->pose_rotation, source->pose_rotation);
    VMOVE(dest->pose_scale, source->pose_scale);
    dest->dof_count = source->dof_count;
    memcpy(dest->dofs, source->dofs, sizeof(dest->dofs));
}

static void
joint_release(struct rt_joint_internal *joint)
{
    if (BU_VLS_IS_INITIALIZED(&joint->parent_link))
	bu_vls_free(&joint->parent_link);
    if (BU_VLS_IS_INITIALIZED(&joint->child_link))
	bu_vls_free(&joint->child_link);
    if (BU_VLS_IS_INITIALIZED(&joint->parent_path))
	bu_vls_free(&joint->parent_path);
    if (BU_VLS_IS_INITIALIZED(&joint->child_path))
	bu_vls_free(&joint->child_path);
    joint->magic = 0;
}

C_DECL int
rt_joint_prep(struct soltab *stp, struct rt_db_internal *ip, struct rt_i *rtip)
{
    struct rt_joint_internal *joint = (struct rt_joint_internal *)ip->idb_ptr;
    point_t location;

    if (rtip) RT_CK_RTI(rtip);
    RT_JOINT_CK_MAGIC(joint);
    MAT_DELTAS_GET(location, joint->parent_frame);
    VMOVE(stp->st_min, location);
    VMOVE(stp->st_max, location);
    stp->st_aradius = stp->st_bradius = 0.0;
    stp->st_specific = NULL;
    return BRLCAD_OK;
}

C_DECL void
rt_joint_print(const struct soltab *stp)
{
    bu_log("joint(%s): non-geometric articulation relationship\n", stp->st_name);
}

C_DECL int
rt_joint_shot(struct soltab *stp, struct xray *rp, struct application *ap, struct seg *UNUSED(seghead))
{
    if (stp) RT_CK_SOLTAB(stp);
    if (rp) RT_CK_RAY(rp);
    if (ap) RT_CK_APPLICATION(ap);
    return 0;
}

C_DECL void
rt_joint_vshot(struct soltab **stp, struct xray **UNUSED(rp), struct seg *segp, int count, struct application *ap)
{
    int i;
    if (ap) RT_CK_APPLICATION(ap);
    for (i = 0; i < count; i++)
	if (stp[i]) segp[i].seg_stp = NULL;
}

C_DECL void
rt_joint_norm(struct hit *UNUSED(hitp), struct soltab *UNUSED(stp), struct xray *UNUSED(rp))
{
    bu_bomb("rt_joint_norm: joints cannot be ray hit\n");
}

C_DECL void
rt_joint_curve(struct curvature *UNUSED(cvp), struct hit *UNUSED(hitp), struct soltab *UNUSED(stp))
{
    bu_bomb("rt_joint_curve: joints have no curvature\n");
}

C_DECL void
rt_joint_uv(struct application *UNUSED(ap), struct soltab *UNUSED(stp), struct hit *UNUSED(hitp), struct uvcoord *UNUSED(uvp))
{
    bu_bomb("rt_joint_uv: joints have no UV coordinates\n");
}

C_DECL void
rt_joint_free(struct soltab *stp)
{
    stp->st_specific = NULL;
}

static void
joint_plot_frame(struct bu_list *vhead, const mat_t frame)
{
    struct bu_list *vlfree = &rt_vlfree;
    point_t origin;
    point_t endpoint;
    vect_t axis;
    size_t i;

    MAT_DELTAS_GET(origin, frame);
    for (i = 0; i < 3; i++) {
	VSETALL(axis, 0.0);
	axis[i] = JOINT_PLOT_AXIS_LENGTH;
	MAT4X3VEC(endpoint, frame, axis);
	VADD2(endpoint, origin, endpoint);
	BV_ADD_VLIST(vlfree, vhead, origin, BV_VLIST_LINE_MOVE);
	BV_ADD_VLIST(vlfree, vhead, endpoint, BV_VLIST_LINE_DRAW);
    }
}

C_DECL int
rt_joint_plot(struct bu_list *vhead, struct rt_db_internal *ip, const struct bg_tess_tol *UNUSED(ttol), const struct bn_tol *UNUSED(tol), const struct bview *UNUSED(info))
{
    struct rt_joint_internal *joint = (struct rt_joint_internal *)ip->idb_ptr;
    struct bu_list *vlfree = &rt_vlfree;
    point_t parent_origin;
    point_t child_origin;

    BU_CK_LIST_HEAD(vhead);
    RT_JOINT_CK_MAGIC(joint);
    joint_plot_frame(vhead, joint->parent_frame);
    joint_plot_frame(vhead, joint->child_frame);
    MAT_DELTAS_GET(parent_origin, joint->parent_frame);
    MAT_DELTAS_GET(child_origin, joint->child_frame);
    BV_ADD_VLIST(vlfree, vhead, parent_origin, BV_VLIST_LINE_MOVE);
    BV_ADD_VLIST(vlfree, vhead, child_origin, BV_VLIST_LINE_DRAW);
    return BRLCAD_OK;
}

C_DECL int
rt_joint_import4(struct rt_db_internal *UNUSED(ip), const struct bu_external *UNUSED(ep), const fastf_t *UNUSED(mat), const struct db_i *UNUSED(dbip))
{
    return BRLCAD_ERROR;
}

C_DECL int
rt_joint_export4(struct bu_external *UNUSED(ep), const struct rt_db_internal *UNUSED(ip), double UNUSED(local2mm), const struct db_i *UNUSED(dbip))
{
    return BRLCAD_ERROR;
}

C_DECL const char *
rt_joint_keypoint(point_t *point, const char *keystr, const mat_t mat, const struct rt_db_internal *ip, const struct bn_tol *UNUSED(tol))
{
    const struct rt_joint_internal *joint;
    const char *key = keystr ? keystr : "P";
    point_t local_point;

    if (!point || !ip) return NULL;
    joint = (const struct rt_joint_internal *)ip->idb_ptr;
    RT_JOINT_CK_MAGIC(joint);
    if (BU_STR_EQUAL(key, "P"))
	MAT_DELTAS_GET(local_point, joint->parent_frame);
    else if (BU_STR_EQUAL(key, "C"))
	MAT_DELTAS_GET(local_point, joint->child_frame);
    else
	return NULL;
    MAT4X3PNT(*point, mat, local_point);
    return key;
}

C_DECL int
rt_joint_mat(struct rt_db_internal *result, const mat_t transform, const struct rt_db_internal *input)
{
    struct bn_tol tol = BN_TOL_INIT_TOL;
    const struct rt_joint_internal *source;
    struct rt_joint_internal *target;
    mat_t frame;

    if (!result || !input || !transform) return BRLCAD_OK;
    if (!bn_mat_is_rigid(transform, &tol)) return BRLCAD_ERROR;
    source = (const struct rt_joint_internal *)input->idb_ptr;
    target = (struct rt_joint_internal *)result->idb_ptr;
    RT_JOINT_CK_MAGIC(source);
    RT_JOINT_CK_MAGIC(target);
    bn_mat_mul(frame, transform, source->parent_frame);
    MAT_COPY(target->parent_frame, frame);
    bn_mat_mul(frame, transform, source->child_frame);
    MAT_COPY(target->child_frame, frame);
    return BRLCAD_OK;
}

static uint32_t
joint_get_uint32(const unsigned char *buffer)
{
    uint32_t value;
    memcpy(&value, buffer, sizeof(value));
    return ntohl(value);
}

static void
joint_put_uint32(unsigned char *buffer, uint32_t value)
{
    value = htonl(value);
    memcpy(buffer, &value, sizeof(value));
}

static int
joint_import_legacy(struct rt_joint_internal *joint, const struct bu_external *external)
{
    const size_t numeric_size = JOINT_LEGACY_DOUBLES * SIZEOF_NETWORK_DOUBLE;
    const unsigned char *paths;
    const unsigned char *separator;
    size_t remaining;
    double values[JOINT_LEGACY_DOUBLES];

    if (external->ext_nbytes < numeric_size + 2) return BRLCAD_ERROR;
    bu_cv_ntohd((unsigned char *)values, external->ext_buf, JOINT_LEGACY_DOUBLES);
    paths = external->ext_buf + numeric_size;
    remaining = external->ext_nbytes - numeric_size;
    separator = (const unsigned char *)memchr(paths, '\0', remaining);
    if (!separator) return BRLCAD_ERROR;
    bu_vls_strncpy(&joint->parent_path, (const char *)paths, (size_t)(separator - paths));
    bu_vls_strcpy(&joint->parent_link, bu_vls_cstr(&joint->parent_path));
    remaining -= (size_t)(separator - paths) + 1;
    paths = separator + 1;
    separator = (const unsigned char *)memchr(paths, '\0', remaining);
    if (!separator) return BRLCAD_ERROR;
    bu_vls_strncpy(&joint->child_path, (const char *)paths, (size_t)(separator - paths));
    bu_vls_strcpy(&joint->child_link, bu_vls_cstr(&joint->child_path));
    MAT_DELTAS(joint->parent_frame, values[X], values[Y], values[Z]);
    MAT_DELTAS(joint->child_frame, values[X], values[Y], values[Z]);
    return BRLCAD_OK;
}

C_DECL int
rt_joint_import5(struct rt_db_internal *ip, const struct bu_external *external, const fastf_t *mat, const struct db_i *dbip)
{
    struct rt_joint_internal *joint;
    const unsigned char *cursor;
    size_t parent_link_length;
    size_t child_link_length;
    size_t parent_length;
    size_t child_length;
    size_t expected_size;
    size_t i;

    if (dbip) RT_CK_DBI(dbip);
    RT_CK_DB_INTERNAL(ip);
    BU_CK_EXTERNAL(external);
    ip->idb_major_type = DB5_MAJORTYPE_BRLCAD;
    ip->idb_type = ID_JOINT;
    ip->idb_meth = &OBJ[ID_JOINT];
    BU_ALLOC(joint, struct rt_joint_internal);
    ip->idb_ptr = joint;
    joint_init(joint);

    if (external->ext_nbytes < SIZEOF_NETWORK_LONG || joint_get_uint32(external->ext_buf) != JOINT_EXT_MAGIC) {
	if (joint_import_legacy(joint, external) != BRLCAD_OK) goto import_error;
    } else {
	double frames[JOINT_EXT_FRAME_DOUBLES];
	double pose[JOINT_EXT_POSE_DOUBLES];
	uint32_t version;

	if (external->ext_nbytes < JOINT_EXT_HEADER_LONGS * SIZEOF_NETWORK_LONG) goto import_error;
	cursor = external->ext_buf + SIZEOF_NETWORK_LONG;
	version = joint_get_uint32(cursor); cursor += SIZEOF_NETWORK_LONG;
	if (version != JOINT_EXT_VERSION) goto import_error;
	joint->type = joint_get_uint32(cursor); cursor += SIZEOF_NETWORK_LONG;
	joint->dof_count = joint_get_uint32(cursor); cursor += SIZEOF_NETWORK_LONG;
	parent_link_length = joint_get_uint32(cursor); cursor += SIZEOF_NETWORK_LONG;
	child_link_length = joint_get_uint32(cursor); cursor += SIZEOF_NETWORK_LONG;
	parent_length = joint_get_uint32(cursor); cursor += SIZEOF_NETWORK_LONG;
	child_length = joint_get_uint32(cursor); cursor += SIZEOF_NETWORK_LONG;
	if (joint->dof_count > RT_JOINT_MAX_DOF) goto import_error;
	expected_size = JOINT_EXT_HEADER_LONGS * SIZEOF_NETWORK_LONG +
	    JOINT_EXT_FRAME_DOUBLES * SIZEOF_NETWORK_DOUBLE +
	    JOINT_EXT_POSE_DOUBLES * SIZEOF_NETWORK_DOUBLE +
	    joint->dof_count * (JOINT_EXT_DOF_LONGS * SIZEOF_NETWORK_LONG + JOINT_EXT_DOF_DOUBLES * SIZEOF_NETWORK_DOUBLE) +
	    parent_link_length + child_link_length + parent_length + child_length;
	if (expected_size != external->ext_nbytes) goto import_error;
	bu_cv_ntohd((unsigned char *)frames, cursor, JOINT_EXT_FRAME_DOUBLES);
	cursor += JOINT_EXT_FRAME_DOUBLES * SIZEOF_NETWORK_DOUBLE;
	MAT_COPY(joint->parent_frame, frames);
	MAT_COPY(joint->child_frame, &frames[ELEMENTS_PER_MAT]);
	bu_cv_ntohd((unsigned char *)pose, cursor, JOINT_EXT_POSE_DOUBLES);
	cursor += JOINT_EXT_POSE_DOUBLES * SIZEOF_NETWORK_DOUBLE;
	VMOVE(joint->pose_translation, pose);
	HMOVE(joint->pose_rotation, &pose[3]);
	VMOVE(joint->pose_scale, &pose[7]);
	for (i = 0; i < joint->dof_count; i++) {
	    double values[JOINT_EXT_DOF_DOUBLES];
	    struct rt_joint_dof *dof = &joint->dofs[i];
	    dof->motion = joint_get_uint32(cursor); cursor += SIZEOF_NETWORK_LONG;
	    dof->flags = joint_get_uint32(cursor); cursor += SIZEOF_NETWORK_LONG;
	    bu_cv_ntohd((unsigned char *)values, cursor, JOINT_EXT_DOF_DOUBLES);
	    cursor += JOINT_EXT_DOF_DOUBLES * SIZEOF_NETWORK_DOUBLE;
	    VMOVE(dof->axis, values);
	    dof->lower = values[3]; dof->upper = values[4]; dof->current = values[5];
	}
	bu_vls_strncpy(&joint->parent_link, (const char *)cursor, parent_link_length);
	cursor += parent_link_length;
	bu_vls_strncpy(&joint->child_link, (const char *)cursor, child_link_length);
	cursor += child_link_length;
	bu_vls_strncpy(&joint->parent_path, (const char *)cursor, parent_length);
	cursor += parent_length;
	bu_vls_strncpy(&joint->child_path, (const char *)cursor, child_length);
    }
    if (rt_joint_validate(joint, NULL)) goto import_error;
    if (!mat) mat = bn_mat_identity;
    if (rt_joint_mat(ip, mat, ip) != BRLCAD_OK) goto import_error;
    return BRLCAD_OK;

import_error:
    joint_release(joint);
    BU_FREE(joint, struct rt_joint_internal);
    ip->idb_ptr = NULL;
    ip->idb_type = ID_NULL;
    return BRLCAD_ERROR;
}

C_DECL int
rt_joint_export5(struct bu_external *external, const struct rt_db_internal *ip, double local2mm, const struct db_i *dbip)
{
    const struct rt_joint_internal *joint;
    unsigned char *cursor;
    size_t parent_link_length;
    size_t child_link_length;
    size_t parent_length;
    size_t child_length;
    size_t i;
    double frames[JOINT_EXT_FRAME_DOUBLES];
    double pose[JOINT_EXT_POSE_DOUBLES];

    if (dbip) RT_CK_DBI(dbip);
    RT_CK_DB_INTERNAL(ip);
    BU_CK_EXTERNAL(external);
    if (ip->idb_type != ID_JOINT) return BRLCAD_ERROR;
    joint = (const struct rt_joint_internal *)ip->idb_ptr;
    RT_JOINT_CK_MAGIC(joint);
    if (rt_joint_validate(joint, NULL)) return BRLCAD_ERROR;
    parent_link_length = bu_vls_strlen(&joint->parent_link);
    child_link_length = bu_vls_strlen(&joint->child_link);
    parent_length = bu_vls_strlen(&joint->parent_path);
    child_length = bu_vls_strlen(&joint->child_path);
    external->ext_nbytes = JOINT_EXT_HEADER_LONGS * SIZEOF_NETWORK_LONG +
	JOINT_EXT_FRAME_DOUBLES * SIZEOF_NETWORK_DOUBLE +
	JOINT_EXT_POSE_DOUBLES * SIZEOF_NETWORK_DOUBLE +
	joint->dof_count * (JOINT_EXT_DOF_LONGS * SIZEOF_NETWORK_LONG + JOINT_EXT_DOF_DOUBLES * SIZEOF_NETWORK_DOUBLE) +
	parent_link_length + child_link_length + parent_length + child_length;
    external->ext_buf = (uint8_t *)bu_malloc(external->ext_nbytes, "joint external");
    cursor = external->ext_buf;
    joint_put_uint32(cursor, JOINT_EXT_MAGIC); cursor += SIZEOF_NETWORK_LONG;
    joint_put_uint32(cursor, JOINT_EXT_VERSION); cursor += SIZEOF_NETWORK_LONG;
    joint_put_uint32(cursor, joint->type); cursor += SIZEOF_NETWORK_LONG;
    joint_put_uint32(cursor, (uint32_t)joint->dof_count); cursor += SIZEOF_NETWORK_LONG;
    joint_put_uint32(cursor, (uint32_t)parent_link_length); cursor += SIZEOF_NETWORK_LONG;
    joint_put_uint32(cursor, (uint32_t)child_link_length); cursor += SIZEOF_NETWORK_LONG;
    joint_put_uint32(cursor, (uint32_t)parent_length); cursor += SIZEOF_NETWORK_LONG;
    joint_put_uint32(cursor, (uint32_t)child_length); cursor += SIZEOF_NETWORK_LONG;
    MAT_COPY(frames, joint->parent_frame);
    MAT_COPY(&frames[ELEMENTS_PER_MAT], joint->child_frame);
    frames[3] *= local2mm; frames[7] *= local2mm; frames[11] *= local2mm;
    frames[ELEMENTS_PER_MAT + 3] *= local2mm;
    frames[ELEMENTS_PER_MAT + 7] *= local2mm;
    frames[ELEMENTS_PER_MAT + 11] *= local2mm;
    bu_cv_htond(cursor, (const unsigned char *)frames, JOINT_EXT_FRAME_DOUBLES);
    cursor += JOINT_EXT_FRAME_DOUBLES * SIZEOF_NETWORK_DOUBLE;
    VMOVE(pose, joint->pose_translation);
    pose[X] *= local2mm;
    pose[Y] *= local2mm;
    pose[Z] *= local2mm;
    HMOVE(&pose[3], joint->pose_rotation);
    VMOVE(&pose[7], joint->pose_scale);
    bu_cv_htond(cursor, (const unsigned char *)pose, JOINT_EXT_POSE_DOUBLES);
    cursor += JOINT_EXT_POSE_DOUBLES * SIZEOF_NETWORK_DOUBLE;
    for (i = 0; i < joint->dof_count; i++) {
	const struct rt_joint_dof *dof = &joint->dofs[i];
	double values[JOINT_EXT_DOF_DOUBLES];
	fastf_t unit_scale = dof->motion == RT_JOINT_TRANSLATION ? local2mm : 1.0;
	joint_put_uint32(cursor, dof->motion); cursor += SIZEOF_NETWORK_LONG;
	joint_put_uint32(cursor, dof->flags); cursor += SIZEOF_NETWORK_LONG;
	VMOVE(values, dof->axis);
	values[3] = dof->lower * unit_scale;
	values[4] = dof->upper * unit_scale;
	values[5] = dof->current * unit_scale;
	bu_cv_htond(cursor, (const unsigned char *)values, JOINT_EXT_DOF_DOUBLES);
	cursor += JOINT_EXT_DOF_DOUBLES * SIZEOF_NETWORK_DOUBLE;
    }
    memcpy(cursor, bu_vls_cstr(&joint->parent_link), parent_link_length); cursor += parent_link_length;
    memcpy(cursor, bu_vls_cstr(&joint->child_link), child_link_length); cursor += child_link_length;
    memcpy(cursor, bu_vls_cstr(&joint->parent_path), parent_length); cursor += parent_length;
    memcpy(cursor, bu_vls_cstr(&joint->child_path), child_length);
    return BRLCAD_OK;
}

static void
joint_print_matrix(struct bu_vls *output, const mat_t matrix)
{
    size_t i;
    bu_vls_putc(output, '{');
    for (i = 0; i < ELEMENTS_PER_MAT; i++)
	bu_vls_printf(output, "%s%.17g", i ? " " : "", matrix[i]);
    bu_vls_putc(output, '}');
}

static void
joint_print_dof(struct bu_vls *output, const struct rt_joint_dof *dof)
{
    bu_vls_printf(output, "{%s %s {%g %g %g} %.17g %.17g %.17g}",
	    rt_joint_motion_name(dof->motion),
	    (dof->flags & RT_JOINT_DOF_LIMITED) ? "limited" : "unlimited",
	    V3ARGS(dof->axis), dof->lower, dof->upper, dof->current);
}

C_DECL int
rt_joint_describe(struct bu_vls *output, const struct rt_db_internal *ip, int verbose, double mm2local)
{
    const struct rt_joint_internal *joint = (const struct rt_joint_internal *)ip->idb_ptr;
    size_t i;

    RT_JOINT_CK_MAGIC(joint);
    bu_vls_printf(output, "%s joint\n", rt_joint_type_name(joint->type));
    if (!verbose) return BRLCAD_OK;
    bu_vls_printf(output, "\tparent link: %s\n\tchild link: %s\n",
	    bu_vls_cstr(&joint->parent_link), bu_vls_cstr(&joint->child_link));
    bu_vls_printf(output, "\tparent path: %s\n\tchild path: %s\n",
	    bu_vls_cstr(&joint->parent_path), bu_vls_cstr(&joint->child_path));
    bu_vls_printf(output, "\tparent attachment: (%g %g %g)\n",
	    joint->parent_frame[3] * mm2local, joint->parent_frame[7] * mm2local, joint->parent_frame[11] * mm2local);
    bu_vls_printf(output, "\tchild attachment: (%g %g %g)\n",
	    joint->child_frame[3] * mm2local, joint->child_frame[7] * mm2local, joint->child_frame[11] * mm2local);
    bu_vls_printf(output, "\tpose translation: (%g %g %g)\n",
	    joint->pose_translation[X] * mm2local, joint->pose_translation[Y] * mm2local,
	    joint->pose_translation[Z] * mm2local);
    bu_vls_printf(output, "\tpose rotation: (%g %g %g %g)\n",
	    joint->pose_rotation[X], joint->pose_rotation[Y],
	    joint->pose_rotation[Z], joint->pose_rotation[W]);
    bu_vls_printf(output, "\tpose scale: (%g %g %g)\n", V3ARGS(joint->pose_scale));
    for (i = 0; i < joint->dof_count; i++) {
	const struct rt_joint_dof *dof = &joint->dofs[i];
	fastf_t unit_scale = dof->motion == RT_JOINT_TRANSLATION ? mm2local : RAD2DEG;
	bu_vls_printf(output, "\tDOF %zu: %s axis (%g %g %g), position %g%s",
		i, rt_joint_motion_name(dof->motion), V3ARGS(dof->axis), dof->current * unit_scale,
		dof->motion == RT_JOINT_TRANSLATION ? "" : " degrees");
	if (dof->flags & RT_JOINT_DOF_LIMITED)
	    bu_vls_printf(output, ", limits [%g, %g]", dof->lower * unit_scale, dof->upper * unit_scale);
	bu_vls_putc(output, '\n');
    }
    return BRLCAD_OK;
}

C_DECL void
rt_joint_ifree(struct rt_db_internal *ip)
{
    struct rt_joint_internal *joint;

    RT_CK_DB_INTERNAL(ip);
    joint = (struct rt_joint_internal *)ip->idb_ptr;
    if (!joint) return;
    RT_JOINT_CK_MAGIC(joint);
    joint_release(joint);
    BU_FREE(joint, struct rt_joint_internal);
    ip->idb_ptr = NULL;
}

C_DECL int
rt_joint_tess(struct nmgregion **region, struct model *model, struct rt_db_internal *ip, const struct bg_tess_tol *UNUSED(ttol), const struct bn_tol *UNUSED(tol))
{
    if (region) *region = NULL;
    if (model) NMG_CK_MODEL(model);
    RT_CK_DB_INTERNAL(ip);
    return BRLCAD_ERROR;
}

C_DECL int
rt_joint_make(const struct rt_functab *ftp, struct rt_db_internal *intern, const char *UNUSED(variant), const point_t origin, double UNUSED(scale))
{
    struct rt_joint_internal *joint;

    intern->idb_type = ID_JOINT;
    intern->idb_major_type = DB5_MAJORTYPE_BRLCAD;
    BU_ASSERT(&OBJ[intern->idb_type] == ftp);
    intern->idb_meth = ftp;
    BU_ALLOC(joint, struct rt_joint_internal);
    intern->idb_ptr = joint;
    joint_init(joint);
    MAT_DELTAS_VEC(joint->parent_frame, origin);
    MAT_DELTAS_VEC(joint->child_frame, origin);
    return BRLCAD_OK;
}

C_DECL int
rt_joint_params(struct pc_pc_set *UNUSED(parameters), const struct rt_db_internal *ip)
{
    if (ip) RT_CK_DB_INTERNAL(ip);
    return BRLCAD_OK;
}

C_DECL int
rt_joint_form(struct bu_vls *output, const struct rt_functab *ftp)
{
    BU_CK_VLS(output);
    RT_CK_FUNCTAB(ftp);
    bu_vls_strcat(output,
	    "type %s parent %s child %s parent_path %s child_path %s parent_frame {%f ...} child_frame {%f ...} "
	    "pose_translation {%f %f %f} pose_rotation {%f %f %f %f} pose_scale {%f %f %f} "
	    "dof_count %d dofN {rotation|translation limited|unlimited {x y z} lower upper current}");
    return BRLCAD_OK;
}

C_DECL int
rt_joint_get(struct bu_vls *output, const struct rt_db_internal *intern, const char *attribute)
{
    const struct rt_joint_internal *joint;
    size_t index;

    BU_CK_VLS(output);
    RT_CK_DB_INTERNAL(intern);
    joint = (const struct rt_joint_internal *)intern->idb_ptr;
    RT_JOINT_CK_MAGIC(joint);
    if (!attribute) {
	bu_vls_printf(output, "joint type %s parent {%s} child {%s} parent_path {%s} child_path {%s} parent_frame ",
		rt_joint_type_name(joint->type), bu_vls_cstr(&joint->parent_link), bu_vls_cstr(&joint->child_link),
		bu_vls_cstr(&joint->parent_path), bu_vls_cstr(&joint->child_path));
	joint_print_matrix(output, joint->parent_frame);
	bu_vls_strcat(output, " child_frame ");
	joint_print_matrix(output, joint->child_frame);
	bu_vls_printf(output, " pose_translation {%.17g %.17g %.17g}", V3ARGS(joint->pose_translation));
	bu_vls_printf(output, " pose_rotation {%.17g %.17g %.17g %.17g}",
		joint->pose_rotation[X], joint->pose_rotation[Y],
		joint->pose_rotation[Z], joint->pose_rotation[W]);
	bu_vls_printf(output, " pose_scale {%.17g %.17g %.17g}", V3ARGS(joint->pose_scale));
	bu_vls_printf(output, " dof_count %zu", joint->dof_count);
	for (index = 0; index < joint->dof_count; index++) {
	    bu_vls_printf(output, " dof%zu ", index);
	    joint_print_dof(output, &joint->dofs[index]);
	}
	return BRLCAD_OK;
    }
    if (BU_STR_EQUAL(attribute, "type")) bu_vls_strcat(output, rt_joint_type_name(joint->type));
    else if (BU_STR_EQUAL(attribute, "parent")) bu_vls_strcat(output, bu_vls_cstr(&joint->parent_link));
    else if (BU_STR_EQUAL(attribute, "child")) bu_vls_strcat(output, bu_vls_cstr(&joint->child_link));
    else if (BU_STR_EQUAL(attribute, "parent_path")) bu_vls_strcat(output, bu_vls_cstr(&joint->parent_path));
    else if (BU_STR_EQUAL(attribute, "child_path")) bu_vls_strcat(output, bu_vls_cstr(&joint->child_path));
    else if (BU_STR_EQUAL(attribute, "parent_frame")) joint_print_matrix(output, joint->parent_frame);
    else if (BU_STR_EQUAL(attribute, "child_frame")) joint_print_matrix(output, joint->child_frame);
    else if (BU_STR_EQUAL(attribute, "pose_translation"))
	bu_vls_printf(output, "{%.17g %.17g %.17g}", V3ARGS(joint->pose_translation));
    else if (BU_STR_EQUAL(attribute, "pose_rotation"))
	bu_vls_printf(output, "{%.17g %.17g %.17g %.17g}",
		joint->pose_rotation[X], joint->pose_rotation[Y],
		joint->pose_rotation[Z], joint->pose_rotation[W]);
    else if (BU_STR_EQUAL(attribute, "pose_scale"))
	bu_vls_printf(output, "{%.17g %.17g %.17g}", V3ARGS(joint->pose_scale));
    else if (BU_STR_EQUAL(attribute, "dof_count")) bu_vls_printf(output, "%zu", joint->dof_count);
    else if (sscanf(attribute, "dof%zu", &index) == 1 && index < joint->dof_count) joint_print_dof(output, &joint->dofs[index]);
    else {
	bu_vls_printf(output, "ERROR: unknown joint attribute '%s'", attribute);
	return BRLCAD_ERROR;
    }
    return BRLCAD_OK;
}

static int
joint_parse_count(const char *value, size_t *count)
{
    char *end = NULL;
    unsigned long parsed = strtoul(value, &end, 10);
    if (!value[0] || !end || *end || parsed > RT_JOINT_MAX_DOF) return BRLCAD_ERROR;
    *count = (size_t)parsed;
    return BRLCAD_OK;
}

static int
joint_parse_dof(struct rt_joint_dof *dof, const char *value)
{
    const char **arguments = NULL;
    int argument_count = 0;
    int result = BRLCAD_ERROR;

    if (bu_argv_from_tcl_list(value, &argument_count, &arguments) != 0 || argument_count != 6) goto cleanup;
    if (rt_joint_motion_from_name(arguments[0], &dof->motion) != BRLCAD_OK) goto cleanup;
    if (BU_STR_EQUAL(arguments[1], "limited")) dof->flags = RT_JOINT_DOF_LIMITED;
    else if (BU_STR_EQUAL(arguments[1], "unlimited")) dof->flags = 0;
    else goto cleanup;
    if (bu_opt_vect_t(NULL, 1, &arguments[2], dof->axis) != 1 ||
	bu_opt_fastf_t(NULL, 1, &arguments[3], &dof->lower) != 1 ||
	bu_opt_fastf_t(NULL, 1, &arguments[4], &dof->upper) != 1 ||
	bu_opt_fastf_t(NULL, 1, &arguments[5], &dof->current) != 1) goto cleanup;
    result = BRLCAD_OK;
cleanup:
    if (arguments) bu_free((void *)arguments, "joint DOF arguments");
    return result;
}

C_DECL int
rt_joint_adjust(struct bu_vls *output, struct rt_db_internal *intern, int argc, const char **argv)
{
    struct rt_joint_internal candidate;
    struct rt_joint_internal *joint;
    int i;

    BU_CK_VLS(output);
    RT_CK_DB_INTERNAL(intern);
    joint = (struct rt_joint_internal *)intern->idb_ptr;
    RT_JOINT_CK_MAGIC(joint);
    if (argc % 2) {
	bu_vls_strcat(output, "ERROR: joint attributes require value pairs");
	return BRLCAD_ERROR;
    }
    joint_copy(&candidate, joint);
    for (i = 0; i < argc; i += 2) {
	const char *attribute = argv[i];
	const char *value = argv[i + 1];
	size_t index;

	if (BU_STR_EQUAL(attribute, "type")) {
	    uint32_t type;
	    if (rt_joint_type_from_name(value, &type) != BRLCAD_OK) {
		bu_vls_printf(output, "ERROR: unknown joint type '%s'", value);
		goto adjust_error;
	    }
	    if (type != candidate.type) joint_configure_type(&candidate, type);
	} else if (BU_STR_EQUAL(attribute, "parent")) bu_vls_strcpy(&candidate.parent_link, value);
	else if (BU_STR_EQUAL(attribute, "child")) bu_vls_strcpy(&candidate.child_link, value);
	else if (BU_STR_EQUAL(attribute, "parent_path")) bu_vls_strcpy(&candidate.parent_path, value);
	else if (BU_STR_EQUAL(attribute, "child_path")) bu_vls_strcpy(&candidate.child_path, value);
	else if (BU_STR_EQUAL(attribute, "parent_frame")) {
	    if (bn_opt_mat(NULL, 1, &value, candidate.parent_frame) != 1) {
		bu_vls_strcat(output, "ERROR: invalid parent frame"); goto adjust_error;
	    }
	} else if (BU_STR_EQUAL(attribute, "child_frame")) {
	    if (bn_opt_mat(NULL, 1, &value, candidate.child_frame) != 1) {
		bu_vls_strcat(output, "ERROR: invalid child frame"); goto adjust_error;
	    }
	} else if (BU_STR_EQUAL(attribute, "pose_translation")) {
	    if (bn_decode_vect(candidate.pose_translation, value) != 3) {
		bu_vls_strcat(output, "ERROR: invalid pose translation"); goto adjust_error;
	    }
	} else if (BU_STR_EQUAL(attribute, "pose_rotation")) {
	    if (bn_decode_quat(candidate.pose_rotation, value) != 4) {
		bu_vls_strcat(output, "ERROR: invalid pose rotation"); goto adjust_error;
	    }
	} else if (BU_STR_EQUAL(attribute, "pose_scale")) {
	    if (bn_decode_vect(candidate.pose_scale, value) != 3) {
		bu_vls_strcat(output, "ERROR: invalid pose scale"); goto adjust_error;
	    }
	} else if (BU_STR_EQUAL(attribute, "dof_count")) {
	    if (joint_parse_count(value, &candidate.dof_count) != BRLCAD_OK) {
		bu_vls_printf(output, "ERROR: invalid DOF count '%s'", value); goto adjust_error;
	    }
	} else if (sscanf(attribute, "dof%zu", &index) == 1 && index < RT_JOINT_MAX_DOF) {
	    if (joint_parse_dof(&candidate.dofs[index], value) != BRLCAD_OK) {
		bu_vls_printf(output, "ERROR: invalid DOF definition for %s", attribute); goto adjust_error;
	    }
	    if (candidate.dof_count <= index) candidate.dof_count = index + 1;
	} else {
	    bu_vls_printf(output, "ERROR: unknown joint attribute '%s'", attribute); goto adjust_error;
	}
    }
    if (rt_joint_validate(&candidate, output)) goto adjust_error;
    bu_vls_strcpy(&joint->parent_link, bu_vls_cstr(&candidate.parent_link));
    bu_vls_strcpy(&joint->child_link, bu_vls_cstr(&candidate.child_link));
    bu_vls_strcpy(&joint->parent_path, bu_vls_cstr(&candidate.parent_path));
    bu_vls_strcpy(&joint->child_path, bu_vls_cstr(&candidate.child_path));
    joint->type = candidate.type;
    MAT_COPY(joint->parent_frame, candidate.parent_frame);
    MAT_COPY(joint->child_frame, candidate.child_frame);
    VMOVE(joint->pose_translation, candidate.pose_translation);
    HMOVE(joint->pose_rotation, candidate.pose_rotation);
    VMOVE(joint->pose_scale, candidate.pose_scale);
    joint->dof_count = candidate.dof_count;
    memcpy(joint->dofs, candidate.dofs, sizeof(joint->dofs));
    joint_release(&candidate);
    return BRLCAD_OK;
adjust_error:
    joint_release(&candidate);
    return BRLCAD_ERROR;
}

static int
joint_data_set(struct rt_data_node *object, const char *name, struct rt_data_node *value)
{
    if (value && rt_data_object_set(object, name, value) == BRLCAD_OK)
	return BRLCAD_OK;
    rt_data_free(value);
    return BRLCAD_ERROR;
}

static struct rt_data_node *
joint_data_vector(const fastf_t *values, size_t count)
{
    struct rt_data_node *array = rt_data_array();
    size_t i;

    if (!array)
	return NULL;
    for (i = 0; i < count; i++) {
	struct rt_data_node *value = rt_data_double(values[i]);
	if (!value || rt_data_array_append(array, value) != BRLCAD_OK) {
	    rt_data_free(value);
	    rt_data_free(array);
	    return NULL;
	}
    }
    return array;
}

static int
joint_data_read_vector(fastf_t *values, size_t count, const struct rt_data_node *array)
{
    size_t i;

    if (!array || rt_data_node_type(array) != RT_DATA_ARRAY || rt_data_array_size(array) != count)
	return BRLCAD_ERROR;
    for (i = 0; i < count; i++) {
	double value;
	if (rt_data_get_double(rt_data_array_get(array, i), &value) != BRLCAD_OK || !isfinite(value))
	    return BRLCAD_ERROR;
	values[i] = value;
    }
    return BRLCAD_OK;
}

C_DECL int
rt_joint_data_export(struct rt_data_node **result, const struct rt_db_internal *intern, const struct rt_data_context *UNUSED(context))
{
    const struct rt_joint_internal *joint;
    struct rt_data_node *data = NULL;
    struct rt_data_node *links = NULL;
    struct rt_data_node *paths = NULL;
    struct rt_data_node *pose = NULL;
    struct rt_data_node *dofs = NULL;
    size_t i;

    if (!result || !intern)
	return BRLCAD_ERROR;
    joint = (const struct rt_joint_internal *)intern->idb_ptr;
    RT_JOINT_CK_MAGIC(joint);
    data = rt_data_object();
    links = rt_data_object();
    paths = rt_data_object();
    pose = rt_data_object();
    dofs = rt_data_array();
    if (!data || !links || !paths || !pose || !dofs)
	goto export_error;
    if (joint_data_set(data, "version", rt_data_uint(JOINT_EXT_VERSION)) != BRLCAD_OK ||
	joint_data_set(data, "kind", rt_data_string(rt_joint_type_name(joint->type))) != BRLCAD_OK ||
	joint_data_set(links, "parent", rt_data_string(bu_vls_cstr(&joint->parent_link))) != BRLCAD_OK ||
	joint_data_set(links, "child", rt_data_string(bu_vls_cstr(&joint->child_link))) != BRLCAD_OK ||
	joint_data_set(paths, "parent", rt_data_string(bu_vls_cstr(&joint->parent_path))) != BRLCAD_OK ||
	joint_data_set(paths, "child", rt_data_string(bu_vls_cstr(&joint->child_path))) != BRLCAD_OK ||
	joint_data_set(pose, "translation", joint_data_vector(joint->pose_translation, 3)) != BRLCAD_OK ||
	joint_data_set(pose, "rotation", joint_data_vector(joint->pose_rotation, 4)) != BRLCAD_OK ||
	joint_data_set(pose, "scale", joint_data_vector(joint->pose_scale, 3)) != BRLCAD_OK ||
	joint_data_set(data, "parent_frame", joint_data_vector(joint->parent_frame, ELEMENTS_PER_MAT)) != BRLCAD_OK ||
	joint_data_set(data, "child_frame", joint_data_vector(joint->child_frame, ELEMENTS_PER_MAT)) != BRLCAD_OK)
	goto export_error;
    for (i = 0; i < joint->dof_count; i++) {
	const struct rt_joint_dof *dof = &joint->dofs[i];
	struct rt_data_node *dof_data = rt_data_object();
	struct rt_data_node *limits = rt_data_null();
	if (dof->flags & RT_JOINT_DOF_LIMITED) {
	    fastf_t bounds[2] = {dof->lower, dof->upper};
	    rt_data_free(limits);
	    limits = joint_data_vector(bounds, 2);
	}
	if (!dof_data || !limits ||
	    joint_data_set(dof_data, "motion", rt_data_string(rt_joint_motion_name(dof->motion))) != BRLCAD_OK ||
	    joint_data_set(dof_data, "axis", joint_data_vector(dof->axis, 3)) != BRLCAD_OK ||
	    joint_data_set(dof_data, "position", rt_data_double(dof->current)) != BRLCAD_OK ||
	    joint_data_set(dof_data, "limits", limits) != BRLCAD_OK ||
	    rt_data_array_append(dofs, dof_data) != BRLCAD_OK) {
	    rt_data_free(dof_data);
	    goto export_error;
	}
    }
    if (joint_data_set(data, "links", links) != BRLCAD_OK)
	goto export_error;
    links = NULL;
    if (joint_data_set(data, "paths", paths) != BRLCAD_OK)
	goto export_error;
    paths = NULL;
    if (joint_data_set(data, "pose", pose) != BRLCAD_OK)
	goto export_error;
    pose = NULL;
    if (joint_data_set(data, "dofs", dofs) != BRLCAD_OK)
	goto export_error;
    dofs = NULL;
    *result = data;
    return BRLCAD_OK;

export_error:
    rt_data_free(links);
    rt_data_free(paths);
    rt_data_free(pose);
    rt_data_free(dofs);
    rt_data_free(data);
    return BRLCAD_ERROR;
}

C_DECL int
rt_joint_data_import(struct rt_db_internal *intern, const struct rt_data_node *data, const struct rt_data_context *UNUSED(context))
{
    struct rt_joint_internal candidate;
    struct rt_joint_internal *joint;
    const struct rt_data_node *links;
    const struct rt_data_node *paths;
    const struct rt_data_node *pose;
    const struct rt_data_node *dofs;
    const char *kind;
    const char *value;
    uint64_t version;
    size_t i;

    if (!intern || !data || rt_data_node_type(data) != RT_DATA_OBJECT)
	return BRLCAD_ERROR;
    joint = (struct rt_joint_internal *)intern->idb_ptr;
    RT_JOINT_CK_MAGIC(joint);
    if (rt_data_get_uint(rt_data_object_get(data, "version"), &version) != BRLCAD_OK || version != JOINT_EXT_VERSION)
	return BRLCAD_ERROR;
    kind = rt_data_get_string(rt_data_object_get(data, "kind"));
    links = rt_data_object_get(data, "links");
    paths = rt_data_object_get(data, "paths");
    pose = rt_data_object_get(data, "pose");
    dofs = rt_data_object_get(data, "dofs");
    if (!kind || !links || !paths || !pose || !dofs ||
	rt_data_node_type(links) != RT_DATA_OBJECT ||
	rt_data_node_type(paths) != RT_DATA_OBJECT ||
	rt_data_node_type(pose) != RT_DATA_OBJECT ||
	rt_data_node_type(dofs) != RT_DATA_ARRAY ||
	rt_data_array_size(dofs) > RT_JOINT_MAX_DOF)
	return BRLCAD_ERROR;

    joint_init(&candidate);
    if (rt_joint_type_from_name(kind, &candidate.type) != BRLCAD_OK)
	goto import_error;
    value = rt_data_get_string(rt_data_object_get(links, "parent"));
    if (!value)
	goto import_error;
    bu_vls_strcpy(&candidate.parent_link, value);
    value = rt_data_get_string(rt_data_object_get(links, "child"));
    if (!value)
	goto import_error;
    bu_vls_strcpy(&candidate.child_link, value);
    value = rt_data_get_string(rt_data_object_get(paths, "parent"));
    if (!value)
	goto import_error;
    bu_vls_strcpy(&candidate.parent_path, value);
    value = rt_data_get_string(rt_data_object_get(paths, "child"));
    if (!value)
	goto import_error;
    bu_vls_strcpy(&candidate.child_path, value);
    if (joint_data_read_vector(candidate.parent_frame, ELEMENTS_PER_MAT,
	    rt_data_object_get(data, "parent_frame")) != BRLCAD_OK ||
	joint_data_read_vector(candidate.child_frame, ELEMENTS_PER_MAT,
	    rt_data_object_get(data, "child_frame")) != BRLCAD_OK ||
	joint_data_read_vector(candidate.pose_translation, 3,
	    rt_data_object_get(pose, "translation")) != BRLCAD_OK ||
	joint_data_read_vector(candidate.pose_rotation, 4,
	    rt_data_object_get(pose, "rotation")) != BRLCAD_OK ||
	joint_data_read_vector(candidate.pose_scale, 3,
	    rt_data_object_get(pose, "scale")) != BRLCAD_OK)
	goto import_error;

    candidate.dof_count = rt_data_array_size(dofs);
    for (i = 0; i < candidate.dof_count; i++) {
	const struct rt_data_node *dof_data = rt_data_array_get(dofs, i);
	const struct rt_data_node *limits;
	const char *motion;
	double position;
	if (!dof_data || rt_data_node_type(dof_data) != RT_DATA_OBJECT)
	    goto import_error;
	motion = rt_data_get_string(rt_data_object_get(dof_data, "motion"));
	limits = rt_data_object_get(dof_data, "limits");
	if (!motion || !limits ||
	    rt_joint_motion_from_name(motion, &candidate.dofs[i].motion) != BRLCAD_OK ||
	    joint_data_read_vector(candidate.dofs[i].axis, 3,
		rt_data_object_get(dof_data, "axis")) != BRLCAD_OK ||
	    rt_data_get_double(rt_data_object_get(dof_data, "position"), &position) != BRLCAD_OK ||
	    !isfinite(position))
	    goto import_error;
	candidate.dofs[i].current = position;
	if (rt_data_node_type(limits) == RT_DATA_ARRAY) {
	    fastf_t bounds[2];
	    if (joint_data_read_vector(bounds, 2, limits) != BRLCAD_OK)
		goto import_error;
	    candidate.dofs[i].flags = RT_JOINT_DOF_LIMITED;
	    candidate.dofs[i].lower = bounds[0];
	    candidate.dofs[i].upper = bounds[1];
	} else if (rt_data_node_type(limits) != RT_DATA_NULL) {
	    goto import_error;
	}
    }
    if (rt_joint_validate(&candidate, NULL))
	goto import_error;
    joint_release(joint);
    *joint = candidate;
    return BRLCAD_OK;

import_error:
    joint_release(&candidate);
    return BRLCAD_ERROR;
}

C_DECL int
rt_joint_functab_validate(struct bu_vls *messages, const struct rt_db_internal *ip, const struct bn_tol *UNUSED(tol))
{
    const struct rt_joint_internal *joint;
    if (!ip) return BRLCAD_ERROR;
    RT_CK_DB_INTERNAL(ip);
    joint = (const struct rt_joint_internal *)ip->idb_ptr;
    return rt_joint_validate(joint, messages) ? BRLCAD_ERROR : BRLCAD_OK;
}

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
