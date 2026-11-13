/*                        J O I N T 2 . C
 * BRL-CAD
 *
 * Copyright (c) 2014-2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 */
/** @file libged/joint2/joint2.c
 *
 * Staging command for database-backed articulation relationships.
 */

#include "common.h"

#include "bu/opt.h"
#include "raytrace.h"
#include "rt/geom.h"
#include "rt/primitives/joint.h"

#include "../ged_private.h"

static const char *joint2_usage =
    "joint2 object show\n"
    "joint2 object validate\n"
    "joint2 object matrix\n"
    "joint2 object pose [translation|rotation|scale [values...]]\n"
    "joint2 object position index [value]";

static void
joint2_print_matrix(struct bu_vls *output, const mat_t matrix)
{
    size_t i;

    for (i = 0; i < ELEMENTS_PER_MAT; i++)
	bu_vls_printf(output, "%s%.17g", i ? " " : "", matrix[i]);
}

static int
joint2_validate_path(struct ged *gedp, const char *label, const struct bu_vls *path)
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
joint2_position(struct ged *gedp, struct directory *directory, struct rt_db_internal *intern, int argc, const char *argv[])
{
    struct rt_joint_internal *joint = (struct rt_joint_internal *)intern->idb_ptr;
    struct bu_vls parse_message = BU_VLS_INIT_ZERO;
    char *end = NULL;
    unsigned long parsed_index;
    size_t index;
    fastf_t unit_scale;

    if (argc != 4 && argc != 5) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s", joint2_usage);
	return BRLCAD_ERROR;
    }
    parsed_index = strtoul(argv[3], &end, 10);
    if (!argv[3][0] || !end || *end || parsed_index >= joint->dof_count) {
	bu_vls_printf(gedp->ged_result_str, "invalid DOF index: %s", argv[3]);
	return BRLCAD_ERROR;
    }
    index = (size_t)parsed_index;
    unit_scale = joint->dofs[index].motion == RT_JOINT_ROTATION ? DEG2RAD : gedp->dbip->dbi_local2base;
    if (argc == 4) {
	fastf_t display_scale = joint->dofs[index].motion == RT_JOINT_ROTATION ? RAD2DEG : gedp->dbip->dbi_base2local;
	bu_vls_printf(gedp->ged_result_str, "%.17g", joint->dofs[index].current * display_scale);
	return BRLCAD_OK;
    }

    fastf_t value;
    if (bu_opt_fastf_t(&parse_message, 1, &argv[4], &value) != 1 || !isfinite(value)) {
	bu_vls_printf(gedp->ged_result_str, "invalid position '%s': %s", argv[4], bu_vls_cstr(&parse_message));
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
joint2_parse_values(struct bu_vls *messages, fastf_t *values, size_t count, const char *argv[])
{
    size_t i;

    for (i = 0; i < count; i++) {
	if (bu_opt_fastf_t(messages, 1, &argv[i], &values[i]) != 1 || !isfinite(values[i]))
	    return BRLCAD_ERROR;
    }
    return BRLCAD_OK;
}

static int
joint2_pose(struct ged *gedp, struct directory *directory, struct rt_db_internal *intern, int argc, const char *argv[])
{
    struct rt_joint_internal *joint = (struct rt_joint_internal *)intern->idb_ptr;
    struct bu_vls parse_message = BU_VLS_INIT_ZERO;
    fastf_t values[4];
    size_t value_count;
    size_t i;

    if (argc == 3) {
	bu_vls_printf(gedp->ged_result_str,
		"translation {%.17g %.17g %.17g} rotation {%.17g %.17g %.17g %.17g} scale {%.17g %.17g %.17g}",
		joint->pose_translation[X] * gedp->dbip->dbi_base2local,
		joint->pose_translation[Y] * gedp->dbip->dbi_base2local,
		joint->pose_translation[Z] * gedp->dbip->dbi_base2local,
		joint->pose_rotation[X], joint->pose_rotation[Y],
		joint->pose_rotation[Z], joint->pose_rotation[W], V3ARGS(joint->pose_scale));
	return BRLCAD_OK;
    }
    if (argc == 4) {
	if (BU_STR_EQUAL(argv[3], "translation"))
	    bu_vls_printf(gedp->ged_result_str, "%.17g %.17g %.17g",
		    joint->pose_translation[X] * gedp->dbip->dbi_base2local,
		    joint->pose_translation[Y] * gedp->dbip->dbi_base2local,
		    joint->pose_translation[Z] * gedp->dbip->dbi_base2local);
	else if (BU_STR_EQUAL(argv[3], "rotation"))
	    bu_vls_printf(gedp->ged_result_str, "%.17g %.17g %.17g %.17g",
		    joint->pose_rotation[X], joint->pose_rotation[Y],
		    joint->pose_rotation[Z], joint->pose_rotation[W]);
	else if (BU_STR_EQUAL(argv[3], "scale"))
	    bu_vls_printf(gedp->ged_result_str, "%.17g %.17g %.17g", V3ARGS(joint->pose_scale));
	else
	    return BRLCAD_ERROR;
	return BRLCAD_OK;
    }

    value_count = BU_STR_EQUAL(argv[3], "rotation") ? 4 : 3;
    if (argc != (int)(4 + value_count) ||
	(!BU_STR_EQUAL(argv[3], "translation") &&
	 !BU_STR_EQUAL(argv[3], "rotation") &&
	 !BU_STR_EQUAL(argv[3], "scale")) ||
	joint2_parse_values(&parse_message, values, value_count, &argv[4]) != BRLCAD_OK) {
	bu_vls_printf(gedp->ged_result_str, "invalid pose values: %s", bu_vls_cstr(&parse_message));
	bu_vls_free(&parse_message);
	return BRLCAD_ERROR;
    }
    bu_vls_free(&parse_message);

    if (BU_STR_EQUAL(argv[3], "translation")) {
	for (i = 0; i < value_count; i++)
	    values[i] *= gedp->dbip->dbi_local2base;
	VMOVE(joint->pose_translation, values);
    } else if (BU_STR_EQUAL(argv[3], "rotation")) {
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
ged_joint2_core(struct ged *gedp, int argc, const char *argv[])
{
    struct directory *directory;
    struct rt_db_internal intern;
    struct rt_joint_internal *joint;
    const char *subcommand;
    int result = BRLCAD_OK;

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);
    bu_vls_trunc(gedp->ged_result_str, 0);
    if (argc < 3) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s", joint2_usage);
	return argc == 1 ? GED_HELP : BRLCAD_ERROR;
    }
    directory = db_lookup(gedp->dbip, argv[1], LOOKUP_NOISY);
    if (directory == RT_DIR_NULL) {
	bu_vls_printf(gedp->ged_result_str, "joint object not found: %s", argv[1]);
	return BRLCAD_ERROR;
    }
    GED_DB_GET_INTERN(gedp, &intern, directory, bn_mat_identity, BRLCAD_ERROR);
    if (intern.idb_type != ID_JOINT) {
	bu_vls_printf(gedp->ged_result_str, "%s is not a joint", argv[1]);
	rt_db_free_internal(&intern);
	return BRLCAD_ERROR;
    }
    joint = (struct rt_joint_internal *)intern.idb_ptr;
    RT_JOINT_CK_MAGIC(joint);
    subcommand = argv[2];

    if (BU_STR_EQUAL(subcommand, "show")) {
	if (argc != 3)
	    result = BRLCAD_ERROR;
	else
	    result = intern.idb_meth->ft_get(gedp->ged_result_str, &intern, NULL);
    } else if (BU_STR_EQUAL(subcommand, "validate")) {
	if (argc != 3)
	    result = BRLCAD_ERROR;
	else if (rt_joint_validate(joint, gedp->ged_result_str))
	    result = BRLCAD_ERROR;
	else if (joint2_validate_path(gedp, "parent", &joint->parent_path) != BRLCAD_OK ||
		 joint2_validate_path(gedp, "child", &joint->child_path) != BRLCAD_OK)
	    result = BRLCAD_ERROR;
	else
	    bu_vls_strcat(gedp->ged_result_str, "valid");
    } else if (BU_STR_EQUAL(subcommand, "matrix")) {
	mat_t relative;
	if (argc != 3 || rt_joint_relative_mat(relative, joint) != BRLCAD_OK)
	    result = BRLCAD_ERROR;
	else
	    joint2_print_matrix(gedp->ged_result_str, relative);
    } else if (BU_STR_EQUAL(subcommand, "position")) {
	result = joint2_position(gedp, directory, &intern, argc, argv);
    } else if (BU_STR_EQUAL(subcommand, "pose")) {
	result = joint2_pose(gedp, directory, &intern, argc, argv);
    } else {
	result = BRLCAD_ERROR;
    }

    if (result != BRLCAD_OK && !bu_vls_strlen(gedp->ged_result_str))
	bu_vls_printf(gedp->ged_result_str, "Usage: %s", joint2_usage);
    rt_db_free_internal(&intern);
    return result;
}

#include "../include/plugin.h"

#define GED_JOINT2_COMMANDS(X, XID) \
    X(joint2, ged_joint2_core, GED_CMD_DEFAULT) \

GED_DECLARE_COMMAND_SET(GED_JOINT2_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST("libged_joint2", 1, GED_JOINT2_COMMANDS)

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
