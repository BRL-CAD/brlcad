/*                         R O T . C
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
/** @file libged/rot.c
 *
 * The rot command.
 *
 */

#include "common.h"

#include <ctype.h>
#include <errno.h>
#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "bu/arg.h"

#include "../ged_private.h"

struct rot_args {
    int model;
    int view;
};

static const struct bu_arg_schema *rot_schema(void);


static int
rot_number_valid(const char *arg)
{
    char *end = NULL;
    double value;

    if (!arg || !arg[0])
	return 0;
    errno = 0;
    value = strtod(arg, &end);
    return errno != ERANGE && end && !*end && isfinite(value) &&
	!(sizeof(fastf_t) == sizeof(float) && (value > FLT_MAX || value < -FLT_MAX));
}


static int
rot_vector_count(const char *arg)
{
    vect_t vector;
    int count;

    if (!arg)
	return 0;
    count = bn_decode_vect(vector, arg);
    if (count < 1 || count > 3)
	return 0;
    for (int i = 0; i < count; i++)
	if (!isfinite(vector[i]))
	    return 0;
    return count;
}


static int
rot_validation_result(struct bu_arg_validate_result *result,
	bu_arg_validate_state_t state, size_t token, bu_arg_value_t type,
	const char *hint, const char *provider)
{
    bu_arg_validate_result_clear(result);
    result->state = state;
    result->token_start = token;
    result->token_end = token;
    result->expected = BU_ARG_EXPECT_OPERAND;
    result->completion_type = type;
    result->hint = hint;
    result->semantic_provider = provider;
    return 0;
}


int
ged_rot_core(struct ged *gedp, int argc, const char *argv[])
{
    char coord;
    mat_t rmat;
    vect_t rotation;
    struct rot_args args = {0, 0};
    int operand_index;
    int operand_count;

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_VIEW(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    bu_vls_trunc(gedp->ged_result_str, 0);
    if (argc == 1) {
	ged_cmd_help_append(gedp->ged_result_str, "rot", argv[0]);
	return GED_HELP;
    }
    operand_index = bu_arg_schema_parse_complete(rot_schema(), &args,
	gedp->ged_result_str, argc - 1, argv + 1);
    if (operand_index < 0)
	return BRLCAD_ERROR;
    operand_count = argc - 1 - operand_index;
    if (bu_arg_vector3_from_argv(rotation, (size_t)operand_count,
	    argv + 1 + operand_index) != operand_count) {
	bu_vls_strcat(gedp->ged_result_str, "Invalid rotation vector\n");
	return BRLCAD_ERROR;
    }
    coord = args.model ? 'm' : (args.view ? 'v' : gedp->ged_gvp->gv_coord);

    VSCALE(rotation, rotation, -1.0);
    bn_mat_angles(rmat, V3ARGS(rotation));

    return _ged_do_rot(gedp, coord, rmat, NULL);
}


#include "../include/plugin.h"

extern int ged_rotate_about_core(struct ged *gedp, int argc, const char *argv[]);
extern GED_EXPORT const struct bu_arg_schema ged_rot_about_cmd_schema;

static int
rot_schema_validate(const struct bu_arg_schema *schema, size_t argc, const char **argv,
	size_t cursor_arg, struct bu_arg_validate_result *result)
{
    struct bu_arg_schema flat = *schema;
    size_t token = cursor_arg < argc ? cursor_arg : argc;
    size_t first = 0;
    size_t count;
    int model;
    int view;
    int ret;

    ret = bu_arg_schema_validate_syntax(&flat, argc, argv, cursor_arg, result);
    if (ret || result->state == BU_ARG_VALIDATE_INVALID)
	return ret;

    model = bu_arg_schema_option_present(schema, argc, argv, "m");
    view = bu_arg_schema_option_present(schema, argc, argv, "v");
    if (model && view)
	return rot_validation_result(result, BU_ARG_VALIDATE_INVALID, token,
	    BU_ARG_VALUE_FLAG, "-m and -v are mutually exclusive", NULL);

    if (cursor_arg < argc && (result->expected & BU_ARG_EXPECT_OPTION))
	return 0;

    while (first < argc && (BU_STR_EQUAL(argv[first], "-m") ||
		BU_STR_EQUAL(argv[first], "-v")))
	first++;
    if (first < argc && BU_STR_EQUAL(argv[first], "--"))
	return rot_validation_result(result, BU_ARG_VALIDATE_INVALID, first,
	    BU_ARG_VALUE_RAW, "end-of-options marker is not supported", NULL);
    count = argc - first;
    if (count == 0)
	return rot_validation_result(result, BU_ARG_VALIDATE_INCOMPLETE, token,
	    BU_ARG_VALUE_VECTOR, "rotation vector expected", "ged.vector_group");

    if (count == 1) {
	int vector_count = rot_vector_count(argv[first]);
	if (vector_count == 3)
	    return rot_validation_result(result, BU_ARG_VALIDATE_VALID, token,
		BU_ARG_VALUE_VECTOR, "packed rotation vector", "ged.vector_group");
	if (vector_count > 0 || rot_number_valid(argv[first]))
	    return rot_validation_result(result, BU_ARG_VALIDATE_INCOMPLETE, token,
		BU_ARG_VALUE_NUMBER, "two more rotation angles expected", NULL);
	return rot_validation_result(result, BU_ARG_VALIDATE_INVALID, first,
	    BU_ARG_VALUE_VECTOR, "packed vector or numeric angle expected", "ged.vector_group");
    }

    for (size_t i = first; i < argc; i++)
	if (!rot_number_valid(argv[i]))
	    return rot_validation_result(result, BU_ARG_VALIDATE_INVALID, i,
		BU_ARG_VALUE_NUMBER, "rotation angles must be finite numbers", NULL);
    if (count < 3)
	return rot_validation_result(result, BU_ARG_VALIDATE_INCOMPLETE, token,
	    BU_ARG_VALUE_NUMBER, "one more rotation angle expected", NULL);
    return rot_validation_result(result, BU_ARG_VALIDATE_VALID, token,
	BU_ARG_VALUE_VECTOR, "rotation vector", "ged.vector_group");
}

static const struct bu_arg_option rot_schema_options[] = {
    BU_ARG_FLAG("m", NULL, struct rot_args, model, "Rotate in model coordinates"),
    BU_ARG_FLAG("v", NULL, struct rot_args, view, "Rotate in view coordinates"),
    BU_ARG_OPTION_NULL
};
static const struct bu_arg_operand rot_schema_operands[] = {
    BU_ARG_OPERAND("rotation", BU_ARG_VALUE_VECTOR, 1, 3,
	"Packed vector or X Y Z angles", "ged.vector_group"),
    BU_ARG_OPERAND_NULL
};
static const struct bu_arg_schema rot_cmd_schema =
    BU_ARG_SCHEMA_BOUND("rot", "Rotate the current view", rot_schema_options,
	rot_schema_operands, BU_ARG_PARSE_OPTIONS_FIRST, NULL, NULL, rot_schema_validate, NULL);

static const struct bu_arg_schema *
rot_schema(void)
{
    return &rot_cmd_schema;
}

#define GED_ROT_COMMANDS(X, XID) \
    X(rot, ged_rot_core, GED_CMD_DEFAULT, &rot_cmd_schema) \
    X(rot_about, ged_rotate_about_core, GED_CMD_DEFAULT, &ged_rot_about_cmd_schema) \

GED_DECLARE_COMMAND_SET_WITH_NATIVE_SCHEMA(GED_ROT_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST_WITH_NATIVE_SCHEMA("libged_rot", 1, GED_ROT_COMMANDS)

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
