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

#include "bu/cmdschema.h"

#include "../ged_private.h"

static const struct bu_cmd_schema *rot_schema(void);


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
rot_validation_result(struct bu_cmd_validate_result *result,
	bu_cmd_validate_state_t state, size_t token, bu_cmd_value_t type,
	const char *hint, const char *provider)
{
    bu_cmd_validate_result_clear(result);
    result->state = state;
    result->token_start = token;
    result->token_end = token;
    result->expected = BU_CMD_EXPECT_OPERAND;
    result->completion_type = type;
    result->hint = hint;
    result->semantic_provider = provider;
    return 0;
}


int
ged_rot_core(struct ged *gedp, int argc, const char *argv[])
{
    int ret;
    char coord;
    mat_t rmat;
    int parse_dummy = 0;

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_VIEW(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    bu_vls_trunc(gedp->ged_result_str, 0);
    if (argc > 1 && bu_cmd_schema_parse_complete(rot_schema(), &parse_dummy,
	gedp->ged_result_str, argc - 1, argv + 1) < 0)
	return BRLCAD_ERROR;

    if ((ret = ged_rot_args(gedp, argc, argv, &coord, rmat)) != BRLCAD_OK)
	return ret;

    return _ged_do_rot(gedp, coord, rmat, NULL);
}


#include "../include/plugin.h"

extern int ged_rotate_about_core(struct ged *gedp, int argc, const char *argv[]);
extern GED_EXPORT const struct bu_cmd_schema ged_rot_about_cmd_schema;

static int
rot_schema_validate(const struct bu_cmd_schema *schema, size_t argc, const char **argv,
	size_t cursor_arg, struct bu_cmd_validate_result *result)
{
    struct bu_cmd_schema flat = *schema;
    size_t token = cursor_arg < argc ? cursor_arg : argc;
    size_t first = 0;
    size_t count;
    int model;
    int view;
    int ret;

    flat.validation.custom_validate = NULL;
    ret = bu_cmd_schema_validate(&flat, argc, argv, cursor_arg, result);
    if (ret || result->state == BU_CMD_VALIDATE_INVALID)
	return ret;

    model = bu_cmd_schema_option_present(schema, argc, argv, "m");
    view = bu_cmd_schema_option_present(schema, argc, argv, "v");
    if (model && view)
	return rot_validation_result(result, BU_CMD_VALIDATE_INVALID, token,
	    BU_CMD_VALUE_FLAG, "-m and -v are mutually exclusive", NULL);

    if (cursor_arg < argc && (result->expected & BU_CMD_EXPECT_OPTION))
	return 0;

    while (first < argc && (BU_STR_EQUAL(argv[first], "-m") ||
		BU_STR_EQUAL(argv[first], "-v")))
	first++;
    if (first < argc && BU_STR_EQUAL(argv[first], "--"))
	return rot_validation_result(result, BU_CMD_VALIDATE_INVALID, first,
	    BU_CMD_VALUE_RAW, "end-of-options marker is not supported", NULL);
    count = argc - first;
    if (count == 0)
	return rot_validation_result(result, BU_CMD_VALIDATE_INCOMPLETE, token,
	    BU_CMD_VALUE_VECTOR, "rotation vector expected", "ged.vector");

    if (count == 1) {
	int vector_count = rot_vector_count(argv[first]);
	if (vector_count == 3)
	    return rot_validation_result(result, BU_CMD_VALIDATE_VALID, token,
		BU_CMD_VALUE_VECTOR, "packed rotation vector", "ged.vector");
	if (vector_count > 0 || rot_number_valid(argv[first]))
	    return rot_validation_result(result, BU_CMD_VALIDATE_INCOMPLETE, token,
		BU_CMD_VALUE_NUMBER, "two more rotation angles expected", NULL);
	return rot_validation_result(result, BU_CMD_VALIDATE_INVALID, first,
	    BU_CMD_VALUE_VECTOR, "packed vector or numeric angle expected", "ged.vector");
    }

    for (size_t i = first; i < argc; i++)
	if (!rot_number_valid(argv[i]))
	    return rot_validation_result(result, BU_CMD_VALIDATE_INVALID, i,
		BU_CMD_VALUE_NUMBER, "rotation angles must be finite numbers", NULL);
    if (count < 3)
	return rot_validation_result(result, BU_CMD_VALIDATE_INCOMPLETE, token,
	    BU_CMD_VALUE_NUMBER, "one more rotation angle expected", NULL);
    return rot_validation_result(result, BU_CMD_VALIDATE_VALID, token,
	BU_CMD_VALUE_VECTOR, "rotation vector", "ged.vector");
}

static const struct bu_cmd_option rot_schema_options[] = {
    BU_CMD_FLAG_UNBOUND("m", NULL, "m", "Rotate in model coordinates"),
    BU_CMD_FLAG_UNBOUND("v", NULL, "v", "Rotate in view coordinates"),
    BU_CMD_OPTION_NULL
};
static const struct bu_cmd_operand rot_schema_operands[] = {
    BU_CMD_OPERAND("rotation", BU_CMD_VALUE_RAW, 1, 3,
	"Packed vector or X Y Z angles", NULL),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_schema rot_cmd_schema = {
    "rot", "Rotate the current view", rot_schema_options, rot_schema_operands,
    BU_CMD_PARSE_OPTIONS_FIRST, {rot_schema_validate}
};

static const struct bu_cmd_schema *
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
