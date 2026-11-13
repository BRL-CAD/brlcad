/*                   J O I N T _ O B J E C T . C
 * BRL-CAD
 *
 * Copyright (c) 2014-2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 */
/** @file libged/joint_object.c
 *
 * Shared command handling for database-backed articulation relationships.
 */

#include "common.h"

#include "bu/opt.h"
#include "raytrace.h"
#include "rt/geom.h"
#include "rt/primitives/joint.h"

#include "ged_private.h"

static const char *joint_object_usage =
    "joint object name show\n"
    "joint object name validate\n"
    "joint object name matrix\n"
    "joint object name apply\n"
    "joint object name pose [translation|rotation|scale [values...]]\n"
    "joint object name position index [value]";

static void
joint_object_print_matrix(struct bu_vls *output, const mat_t matrix)
{
    size_t i;

    for (i = 0; i < ELEMENTS_PER_MAT; i++)
	bu_vls_printf(output, "%s%.17g", i ? " " : "", matrix[i]);
}

static int
joint_object_validate_path(struct ged *gedp, const char *label, const struct bu_vls *path)
{
    struct db_full_path full_path;

    if (!bu_vls_strlen(path))
	return BRLCAD_OK;
    db_full_path_init(&full_path);
    if (db_string_to_path(&full_path, gedp->dbip, bu_vls_cstr(path)) != 0 || full_path.fp_len == 0) {
	bu_vls_printf(gedp->ged_result_str, "%s path does not resolve: %s", label, bu_vls_cstr(path));
	db_free_full_path(&full_path);
	return BRLCAD_ERROR;
    }
    db_free_full_path(&full_path);
    return BRLCAD_OK;
}

static int
joint_object_apply(struct ged *gedp, const struct rt_joint_internal *joint)
{
    struct db_full_path parent_path = DB_FULL_PATH_INIT_ZERO;
    struct db_full_path child_path = DB_FULL_PATH_INIT_ZERO;
    struct directory *parent_directory;
    struct rt_db_internal parent_internal;
    struct rt_comb_internal *combination;
    union tree *child_leaf;
    mat_t relative;
    int result = BRLCAD_ERROR;

    GED_CHECK_READ_ONLY(gedp, BRLCAD_ERROR);
    if (!bu_vls_strlen(&joint->parent_path) || !bu_vls_strlen(&joint->child_path)) {
	bu_vls_strcat(gedp->ged_result_str, "apply requires parent and child path bindings");
	return BRLCAD_ERROR;
    }
    db_full_path_init(&parent_path);
    db_full_path_init(&child_path);
    if (db_string_to_path(&parent_path, gedp->dbip,
	    bu_vls_cstr(&joint->parent_path)) != 0 ||
	db_string_to_path(&child_path, gedp->dbip,
	    bu_vls_cstr(&joint->child_path)) != 0) {
	bu_vls_strcat(gedp->ged_result_str, "joint path binding does not resolve");
	goto cleanup;
    }
    if (child_path.fp_len != parent_path.fp_len + 1 ||
	!db_full_path_match_top(&parent_path, &child_path)) {
	bu_vls_strcat(gedp->ged_result_str,
		"child path must be a direct descendant of parent path");
	goto cleanup;
    }
    parent_directory = DB_FULL_PATH_CUR_DIR(&parent_path);
    if (!parent_directory || !(parent_directory->d_flags & RT_DIR_COMB)) {
	bu_vls_strcat(gedp->ged_result_str, "bound parent is not a combination");
	goto cleanup;
    }
    if (rt_joint_relative_mat(relative, joint) != BRLCAD_OK) {
	bu_vls_strcat(gedp->ged_result_str, "unable to evaluate joint matrix");
	goto cleanup;
    }

    RT_DB_INTERNAL_INIT(&parent_internal);
    if (rt_db_get_internal(&parent_internal, parent_directory, gedp->dbip,
	    NULL) < 0) {
	bu_vls_strcat(gedp->ged_result_str, "unable to read bound parent combination");
	goto cleanup;
    }
    combination = (struct rt_comb_internal *)parent_internal.idb_ptr;
    RT_CK_COMB(combination);
    child_leaf = db_find_named_leaf_instance(combination->tree,
	    DB_FULL_PATH_CUR_DIR(&child_path)->d_namep,
	    (size_t)DB_FULL_PATH_CUR_COMB_INST(&child_path));
    if (!child_leaf) {
	bu_vls_strcat(gedp->ged_result_str, "bound child instance is not present in parent");
	rt_db_free_internal(&parent_internal);
	goto cleanup;
    }
    if (bn_mat_is_identity(relative)) {
	if (child_leaf->tr_l.tl_mat) {
	    bu_free(child_leaf->tr_l.tl_mat, "joint child instance matrix");
	    child_leaf->tr_l.tl_mat = NULL;
	}
    } else {
	if (!child_leaf->tr_l.tl_mat)
	    child_leaf->tr_l.tl_mat = (matp_t)bu_malloc(sizeof(mat_t),
		    "joint child instance matrix");
	MAT_COPY(child_leaf->tr_l.tl_mat, relative);
    }
    if (rt_db_put_internal(parent_directory, gedp->dbip, &parent_internal) < 0) {
	bu_vls_strcat(gedp->ged_result_str, "unable to update bound child instance");
	rt_db_free_internal(&parent_internal);
	goto cleanup;
    }
    bu_vls_strcat(gedp->ged_result_str, "applied");
    result = BRLCAD_OK;

cleanup:
    db_free_full_path(&child_path);
    db_free_full_path(&parent_path);
    return result;
}

static int
joint_object_position(struct ged *gedp, struct directory *directory, struct rt_db_internal *intern, int argc, const char *argv[])
{
    struct rt_joint_internal *joint = (struct rt_joint_internal *)intern->idb_ptr;
    struct bu_vls parse_message = BU_VLS_INIT_ZERO;
    char *end = NULL;
    unsigned long parsed_index;
    size_t index;
    fastf_t unit_scale;

    if (argc != 3 && argc != 4) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s", joint_object_usage);
	return BRLCAD_ERROR;
    }
    parsed_index = strtoul(argv[2], &end, 10);
    if (!argv[2][0] || !end || *end || parsed_index >= joint->dof_count) {
	bu_vls_printf(gedp->ged_result_str, "invalid DOF index: %s", argv[2]);
	return BRLCAD_ERROR;
    }
    index = (size_t)parsed_index;
    unit_scale = joint->dofs[index].motion == RT_JOINT_ROTATION ? DEG2RAD : gedp->dbip->dbi_local2base;
    if (argc == 3) {
	fastf_t display_scale = joint->dofs[index].motion == RT_JOINT_ROTATION ? RAD2DEG : gedp->dbip->dbi_base2local;
	bu_vls_printf(gedp->ged_result_str, "%.17g", joint->dofs[index].current * display_scale);
	return BRLCAD_OK;
    }
    GED_CHECK_READ_ONLY(gedp, BRLCAD_ERROR);

    fastf_t value;
    if (bu_opt_fastf_t(&parse_message, 1, &argv[3], &value) != 1 || !isfinite(value)) {
	bu_vls_printf(gedp->ged_result_str, "invalid position '%s': %s", argv[3], bu_vls_cstr(&parse_message));
	bu_vls_free(&parse_message);
	return BRLCAD_ERROR;
    }
    bu_vls_free(&parse_message);
    fastf_t previous = joint->dofs[index].current;
    joint->dofs[index].current = value * unit_scale;
    if (rt_joint_validate(joint, gedp->ged_result_str)) {
	joint->dofs[index].current = previous;
	return BRLCAD_ERROR;
    }
    GED_DB_PUT_INTERN(gedp, directory, intern, BRLCAD_ERROR);
    return BRLCAD_OK;
}

static int
joint_object_parse_values(struct bu_vls *messages, fastf_t *values, size_t count, const char *argv[])
{
    size_t i;

    for (i = 0; i < count; i++) {
	if (bu_opt_fastf_t(messages, 1, &argv[i], &values[i]) != 1 || !isfinite(values[i]))
	    return BRLCAD_ERROR;
    }
    return BRLCAD_OK;
}

static int
joint_object_pose(struct ged *gedp, struct directory *directory, struct rt_db_internal *intern, int argc, const char *argv[])
{
    struct rt_joint_internal *joint = (struct rt_joint_internal *)intern->idb_ptr;
    struct bu_vls parse_message = BU_VLS_INIT_ZERO;
    fastf_t values[4];
    size_t value_count;
    size_t i;

    if (argc == 2) {
	bu_vls_printf(gedp->ged_result_str,
		"translation {%.17g %.17g %.17g} rotation {%.17g %.17g %.17g %.17g} scale {%.17g %.17g %.17g}",
		joint->pose_translation[X] * gedp->dbip->dbi_base2local,
		joint->pose_translation[Y] * gedp->dbip->dbi_base2local,
		joint->pose_translation[Z] * gedp->dbip->dbi_base2local,
		joint->pose_rotation[X], joint->pose_rotation[Y],
		joint->pose_rotation[Z], joint->pose_rotation[W], V3ARGS(joint->pose_scale));
	return BRLCAD_OK;
    }
    if (argc == 3) {
	if (BU_STR_EQUAL(argv[2], "translation"))
	    bu_vls_printf(gedp->ged_result_str, "%.17g %.17g %.17g",
		    joint->pose_translation[X] * gedp->dbip->dbi_base2local,
		    joint->pose_translation[Y] * gedp->dbip->dbi_base2local,
		    joint->pose_translation[Z] * gedp->dbip->dbi_base2local);
	else if (BU_STR_EQUAL(argv[2], "rotation"))
	    bu_vls_printf(gedp->ged_result_str, "%.17g %.17g %.17g %.17g",
		    joint->pose_rotation[X], joint->pose_rotation[Y],
		    joint->pose_rotation[Z], joint->pose_rotation[W]);
	else if (BU_STR_EQUAL(argv[2], "scale"))
	    bu_vls_printf(gedp->ged_result_str, "%.17g %.17g %.17g", V3ARGS(joint->pose_scale));
	else
	    return BRLCAD_ERROR;
	return BRLCAD_OK;
    }
    GED_CHECK_READ_ONLY(gedp, BRLCAD_ERROR);

    value_count = BU_STR_EQUAL(argv[2], "rotation") ? 4 : 3;
    if (argc != (int)(3 + value_count) ||
	(!BU_STR_EQUAL(argv[2], "translation") &&
	 !BU_STR_EQUAL(argv[2], "rotation") &&
	 !BU_STR_EQUAL(argv[2], "scale")) ||
	joint_object_parse_values(&parse_message, values, value_count, &argv[3]) != BRLCAD_OK) {
	bu_vls_printf(gedp->ged_result_str, "invalid pose values: %s", bu_vls_cstr(&parse_message));
	bu_vls_free(&parse_message);
	return BRLCAD_ERROR;
    }
    bu_vls_free(&parse_message);

    if (BU_STR_EQUAL(argv[2], "translation")) {
	for (i = 0; i < value_count; i++)
	    values[i] *= gedp->dbip->dbi_local2base;
	VMOVE(joint->pose_translation, values);
    } else if (BU_STR_EQUAL(argv[2], "rotation")) {
	HMOVE(joint->pose_rotation, values);
    } else {
	VMOVE(joint->pose_scale, values);
    }
    if (rt_joint_validate(joint, gedp->ged_result_str))
	return BRLCAD_ERROR;
    GED_DB_PUT_INTERN(gedp, directory, intern, BRLCAD_ERROR);
    return BRLCAD_OK;
}

int
_ged_joint_object_core(struct ged *gedp, int argc, const char *argv[])
{
    struct directory *directory;
    struct rt_db_internal intern;
    struct rt_joint_internal *joint;
    const char *subcommand;
    int result = BRLCAD_OK;

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    bu_vls_trunc(gedp->ged_result_str, 0);
    if (argc < 2) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s", joint_object_usage);
	return argc == 0 ? GED_HELP : BRLCAD_ERROR;
    }
    directory = db_lookup(gedp->dbip, argv[0], LOOKUP_NOISY);
    if (directory == RT_DIR_NULL) {
	bu_vls_printf(gedp->ged_result_str, "joint object not found: %s", argv[0]);
	return BRLCAD_ERROR;
    }
    GED_DB_GET_INTERN(gedp, &intern, directory, bn_mat_identity, BRLCAD_ERROR);
    if (intern.idb_type != ID_JOINT) {
	bu_vls_printf(gedp->ged_result_str, "%s is not a joint", argv[0]);
	rt_db_free_internal(&intern);
	return BRLCAD_ERROR;
    }
    joint = (struct rt_joint_internal *)intern.idb_ptr;
    RT_JOINT_CK_MAGIC(joint);
    subcommand = argv[1];

    if (BU_STR_EQUAL(subcommand, "show")) {
	if (argc != 2)
	    result = BRLCAD_ERROR;
	else
	    result = intern.idb_meth->ft_get(gedp->ged_result_str, &intern, NULL);
    } else if (BU_STR_EQUAL(subcommand, "validate")) {
	if (argc != 2)
	    result = BRLCAD_ERROR;
	else if (rt_joint_validate(joint, gedp->ged_result_str))
	    result = BRLCAD_ERROR;
	else if (joint_object_validate_path(gedp, "parent", &joint->parent_path) != BRLCAD_OK ||
		 joint_object_validate_path(gedp, "child", &joint->child_path) != BRLCAD_OK)
	    result = BRLCAD_ERROR;
	else
	    bu_vls_strcat(gedp->ged_result_str, "valid");
    } else if (BU_STR_EQUAL(subcommand, "matrix")) {
	mat_t relative;
	if (argc != 2 || rt_joint_relative_mat(relative, joint) != BRLCAD_OK)
	    result = BRLCAD_ERROR;
	else
	    joint_object_print_matrix(gedp->ged_result_str, relative);
    } else if (BU_STR_EQUAL(subcommand, "apply")) {
	if (argc != 2)
	    result = BRLCAD_ERROR;
	else
	    result = joint_object_apply(gedp, joint);
    } else if (BU_STR_EQUAL(subcommand, "position")) {
	result = joint_object_position(gedp, directory, &intern, argc, argv);
    } else if (BU_STR_EQUAL(subcommand, "pose")) {
	result = joint_object_pose(gedp, directory, &intern, argc, argv);
    } else {
	result = BRLCAD_ERROR;
    }

    if (result != BRLCAD_OK && !bu_vls_strlen(gedp->ged_result_str))
	bu_vls_printf(gedp->ged_result_str, "Usage: %s", joint_object_usage);
    rt_db_free_internal(&intern);
    return result;
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
