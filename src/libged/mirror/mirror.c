/*                        M I R R O R . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2026 United States Government as represented by
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
/** @file libged/mirror.c
 *
 * The mirror command.
 *
 */

#include "common.h"

#include "bu/cmdschema.h"
#include "bv/util.h"
#include "ged.h"


struct mirror_args {
    point_t point;
    vect_t direction;
    fastf_t offset;
    int help;
};


static int
mirror_direction_consume(struct bu_vls *msg, size_t argc, const char **argv, void *storage)
{
    vect_t direction = VINIT_ZERO;
    int consumed = bu_cmd_vector3_from_argv(direction, argc, (const char * const *)argv);

    if (consumed == 0 || (size_t)consumed != argc) {
	if (msg)
	    bu_vls_printf(msg, "mirror direction must be x/y/z, x,y,z, x;y;z, quoted x y z, or three finite numbers\n");
	return -1;
    }
    if (MAGSQ(direction) < SMALL_FASTF * SMALL_FASTF) {
	if (msg)
	    bu_vls_printf(msg, "mirror direction must be nonzero\n");
	return -1;
    }
    if (storage)
	VMOVE((fastf_t *)storage, direction);
    return 0;
}


static int
mirror_axis_x(struct bu_vls *UNUSED(msg), const char *UNUSED(arg), void *storage)
{
    if (storage)
	VSET((fastf_t *)storage, 1.0, 0.0, 0.0);
    return 0;
}


static int
mirror_axis_y(struct bu_vls *UNUSED(msg), const char *UNUSED(arg), void *storage)
{
    if (storage)
	VSET((fastf_t *)storage, 0.0, 1.0, 0.0);
    return 0;
}


static int
mirror_axis_z(struct bu_vls *UNUSED(msg), const char *UNUSED(arg), void *storage)
{
    if (storage)
	VSET((fastf_t *)storage, 0.0, 0.0, 1.0);
    return 0;
}


static int
mirror_schema_validate(const struct bu_cmd_schema *schema, size_t argc,
	const char **argv, size_t cursor_arg, struct bu_cmd_validate_result *result)
{
    struct bu_cmd_schema flat = *schema;
    int ret;

    flat.validation.custom_validate = NULL;
    ret = bu_cmd_schema_validate(&flat, argc, argv, cursor_arg, result);
    if (ret || result->state == BU_CMD_VALIDATE_INVALID)
	return ret;
    if (bu_cmd_schema_option_present(schema, argc, argv, "h")) {
	bu_cmd_validate_result_clear(result);
	result->state = BU_CMD_VALIDATE_VALID;
	result->token_start = cursor_arg < argc ? cursor_arg : argc;
	result->token_end = result->token_start;
	result->expected = BU_CMD_EXPECT_NONE;
	result->completion_type = BU_CMD_VALUE_FLAG;
	result->hint = "command help";
    }
    return 0;
}


static const char * const mirror_axis_x_aliases[] = {"X", NULL};
static const char * const mirror_axis_y_aliases[] = {"Y", NULL};
static const char * const mirror_axis_z_aliases[] = {"Z", NULL};
static const struct bu_cmd_value_keyword mirror_axis_keywords[] = {
    {"x", mirror_axis_x_aliases, "X mirror-plane normal"},
    {"y", mirror_axis_y_aliases, "Y mirror-plane normal"},
    {"z", mirror_axis_z_aliases, "Z mirror-plane normal"},
    {NULL, NULL, NULL}
};
static const struct bu_cmd_option mirror_schema_options[] = {
    BU_CMD_FLAG("h", NULL, struct mirror_args, help, "Print command usage"),
    BU_CMD_ALIAS_SHORT("H", "h", 1),
    BU_CMD_ALIAS_SHORT("?", "h", 1),
    BU_CMD_VECTOR3("p", NULL, struct mirror_args, point, "x/y/z", "Point on mirror plane"),
    BU_CMD_ALIAS_SHORT("P", "p", 1),
    BU_CMD_OPTION_SHAPED("d", NULL, "d", struct mirror_args, direction,
	BU_CMD_VALUE_VECTOR, "x/y/z", "Nonzero mirror-plane normal",
	BU_CMD_ARG_REQUIRED, &bu_cmd_vector3_arg_shape, mirror_direction_consume),
    BU_CMD_ALIAS_SHORT("D", "d", 1),
    BU_CMD_CUSTOM_FLAG("x", NULL, "x", struct mirror_args, direction,
	mirror_axis_x, "Mirror across the X plane"),
    BU_CMD_ALIAS_SHORT("X", "x", 1),
    BU_CMD_CUSTOM_FLAG("y", NULL, "y", struct mirror_args, direction,
	mirror_axis_y, "Mirror across the Y plane"),
    BU_CMD_ALIAS_SHORT("Y", "y", 1),
    BU_CMD_CUSTOM_FLAG("z", NULL, "z", struct mirror_args, direction,
	mirror_axis_z, "Mirror across the Z plane"),
    BU_CMD_ALIAS_SHORT("Z", "z", 1),
    BU_CMD_NUMBER("o", NULL, struct mirror_args, offset, "offset", "Mirror-plane offset"),
    BU_CMD_ALIAS_SHORT("O", "o", 1),
    BU_CMD_OPTION_NULL
};
static const struct bu_cmd_operand mirror_schema_operands[] = {
    BU_CMD_OPERAND("source_object", BU_CMD_VALUE_DB_OBJECT, 1, 1,
	"Existing object to mirror", "ged.db_object"),
    BU_CMD_OPERAND("output_object", BU_CMD_VALUE_STRING, 1, 1,
	"Name for the mirrored object", NULL),
    BU_CMD_OPERAND_KEYWORD_VALUES("axis", BU_CMD_VALUE_KEYWORD, 0, 1,
	"Optional compatibility axis selector", NULL, mirror_axis_keywords),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_schema mirror_cmd_schema = {
    "mirror", "Mirror a geometry object", mirror_schema_options,
    mirror_schema_operands, BU_CMD_PARSE_OPTIONS_FIRST,
    BU_CMD_SCHEMA_CONSTRAINTS(mirror_schema_validate, NULL)
};


static void
mirror_show_help(struct ged *gedp, const char *command)
{
    char *option_help = bu_cmd_schema_describe(&mirror_cmd_schema);

    bu_vls_printf(gedp->ged_result_str,
	"Usage: %s [-p x/y/z] [-d x/y/z] [-x|-y|-z] [-o offset] source_object output_object [axis]",
	command);
    if (option_help) {
	bu_vls_printf(gedp->ged_result_str, "\nOptions:\n%s", option_help);
	bu_free(option_help, "mirror native option help");
    }
}


int
ged_mirror_core(struct ged *gedp, int argc, const char *argv[])
{
    int ret;
    int operand_index;
    int operand_count;
    struct rt_db_internal *ip;
    struct rt_db_internal internal;
    struct directory *dp;
    struct mirror_args args = {{0.0, 0.0, 0.0}, {1.0, 0.0, 0.0}, 0.0, 0};
    const char **operands;

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_READ_ONLY(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    bu_vls_trunc(gedp->ged_result_str, 0);

    if (argc == 1) {
	mirror_show_help(gedp, argv[0]);
	return GED_HELP;
    }

    operand_index = bu_cmd_schema_parse_complete(&mirror_cmd_schema, &args,
	gedp->ged_result_str, argc - 1, argv + 1);
    if (operand_index < 0) {
	mirror_show_help(gedp, argv[0]);
	return BRLCAD_ERROR;
    }
    if (args.help) {
	mirror_show_help(gedp, argv[0]);
	return GED_HELP;
    }
    operand_count = argc - 1 - operand_index;
    operands = argv + operand_index + 1;
    if (operand_count == 3) {
	if (BU_STR_EQUAL(operands[2], "x") || BU_STR_EQUAL(operands[2], "X"))
	    VSET(args.direction, 1.0, 0.0, 0.0);
	else if (BU_STR_EQUAL(operands[2], "y") || BU_STR_EQUAL(operands[2], "Y"))
	    VSET(args.direction, 0.0, 1.0, 0.0);
	else
	    VSET(args.direction, 0.0, 0.0, 1.0);
    }

    /* Make sure object mirroring to does not already exist. */
    if (db_lookup(gedp->dbip, operands[1], LOOKUP_QUIET) != RT_DIR_NULL) {
	bu_vls_printf(gedp->ged_result_str, "%s already exists\n", operands[1]);
	return BRLCAD_ERROR;
    }

    if ((dp = db_lookup(gedp->dbip, operands[0], LOOKUP_NOISY)) == RT_DIR_NULL) {
	bu_vls_printf(gedp->ged_result_str, "Unable to find solid [%s]\n", operands[0]);
	return BRLCAD_ERROR;
    }

    ret = rt_db_get_internal(&internal, dp, gedp->dbip, NULL);
    if (ret < 0) {
	bu_vls_printf(gedp->ged_result_str, "Unable to load solid [%s]\n", operands[0]);
	return BRLCAD_ERROR;
    }

    args.offset *= gedp->dbip->dbi_local2base;
    VUNITIZE(args.direction);
    VJOIN1(args.point, args.point, args.offset, args.direction);

    ip = rt_mirror(gedp->dbip, &internal, args.point, args.direction);
    if (ip == NULL) {
	bu_vls_printf(gedp->ged_result_str, "Unable to mirror [%s]", operands[0]);
	return BRLCAD_ERROR;
    }

    dp = db_diradd(gedp->dbip, operands[1], RT_DIR_PHONY_ADDR, 0, dp->d_flags,
	(void *)&ip->idb_type);
    if (dp == RT_DIR_NULL) {
	bu_vls_printf(gedp->ged_result_str, "Unable to add [%s] to the database directory", operands[1]);
	return BRLCAD_ERROR;
    }
    if (rt_db_put_internal(dp, gedp->dbip, ip) < 0) {
	bu_vls_printf(gedp->ged_result_str, "Unable to store [%s] to the database", operands[1]);
	return BRLCAD_ERROR;
    }

    {
	const char *e_argv[2] = {"draw", operands[1]};
	(void)ged_exec_draw(gedp, 2, e_argv);
	bv_update(gedp->ged_gvp);
    }

    return BRLCAD_OK;
}

#include "../include/plugin.h"

#define GED_MIRROR_COMMANDS(X, XID) \
    X(mirror, ged_mirror_core, GED_CMD_DEFAULT, &mirror_cmd_schema) \

GED_DECLARE_COMMAND_SET_WITH_NATIVE_SCHEMA(GED_MIRROR_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST_WITH_NATIVE_SCHEMA("libged_mirror", 1, GED_MIRROR_COMMANDS)

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
