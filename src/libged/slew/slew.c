/*                         S L E W . C
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
/** @file libged/slew.c
 *
 * The slew command.
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

static const struct bu_cmd_schema *slew_schema_for_command(const char *command);


static int
slew_number_parse(const char *arg, double *value)
{
    char *end = NULL;
    double parsed;

    if (!arg)
	return -1;
    errno = 0;
    parsed = strtod(arg, &end);
    if (errno == ERANGE || !end || *end || !isfinite(parsed) ||
	(sizeof(fastf_t) == sizeof(float) &&
	 (parsed > FLT_MAX || parsed < -FLT_MAX)))
	return -1;
    if (value)
	*value = parsed;
    return 0;
}


static int
slew_vector_valid(const char *arg)
{
    vect_t vector;
    int count;

    if (!arg)
	return 0;
    count = bn_decode_vect(vector, arg);
    return (count == 2 || count == 3) && isfinite(vector[X]) &&
	isfinite(vector[Y]) && (count == 2 || isfinite(vector[Z]));
}


static int
slew_validation_result(struct bu_cmd_validate_result *result,
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
ged_slew_core(struct ged *gedp, int argc, const char *argv[])
{
    vect_t svec;
    static const char *usage = "x y [z]";
    int parse_dummy = 0;

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_VIEW(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (bu_cmd_schema_parse_complete(slew_schema_for_command(argv[0]), &parse_dummy,
	gedp->ged_result_str, argc - 1, argv + 1) < 0) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
	}

    if (argc == 2) {
	int n;

	if ((n = bn_decode_vect(svec, argv[1])) != 3) {
	    if (n != 2) {
		bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
		return BRLCAD_ERROR;
	    }

	    svec[Z] = 0.0;
	}

	return _ged_do_slew(gedp, svec);
    }

    if (argc == 3 || argc == 4) {
	double scan[3];

	if (slew_number_parse(argv[1], &scan[X]) != 0) {
	    bu_vls_printf(gedp->ged_result_str, "slew: bad X value %s\n", argv[1]);
	    return BRLCAD_ERROR;
	}

	if (slew_number_parse(argv[2], &scan[Y]) != 0) {
	    bu_vls_printf(gedp->ged_result_str, "slew: bad Y value %s\n", argv[2]);
	    return BRLCAD_ERROR;
	}

	if (argc == 4) {
	    if (slew_number_parse(argv[3], &scan[Z]) != 0) {
		bu_vls_printf(gedp->ged_result_str, "slew: bad Z value %s\n", argv[3]);
		return BRLCAD_ERROR;
	    }
	} else
	    scan[Z] = 0.0;

	/* convert from double to fastf_t */
	VMOVE(svec, scan);

	return _ged_do_slew(gedp, svec);
    }

    bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
    return BRLCAD_ERROR;
}


#include "../include/plugin.h"

static int
slew_schema_validate(const struct bu_cmd_schema *schema, size_t argc,
	const char **argv, size_t cursor_arg, struct bu_cmd_validate_result *result)
{
    struct bu_cmd_schema flat = *schema;
    size_t token = cursor_arg < argc ? cursor_arg : argc;
    int ret;

    flat.validation.custom_validate = NULL;
    ret = bu_cmd_schema_validate(&flat, argc, argv, cursor_arg, result);
    if (ret || result->state == BU_CMD_VALIDATE_INVALID || argc == 0)
	return ret;

    if (argc == 1) {
	if (slew_vector_valid(argv[0]))
	    return slew_validation_result(result, BU_CMD_VALIDATE_VALID, token,
		BU_CMD_VALUE_VECTOR, "packed view slew vector", "ged.vector");
	if (slew_number_parse(argv[0], NULL) == 0)
	    return slew_validation_result(result, BU_CMD_VALIDATE_INCOMPLETE, token,
		BU_CMD_VALUE_NUMBER, "one more view slew coordinate expected", NULL);
	return slew_validation_result(result, BU_CMD_VALIDATE_INVALID, token,
	    BU_CMD_VALUE_VECTOR, "packed vector or numeric coordinate expected", "ged.vector");
    }

    for (size_t i = 0; i < argc; i++) {
	if (slew_number_parse(argv[i], NULL) != 0)
	    return slew_validation_result(result, BU_CMD_VALIDATE_INVALID, i,
		BU_CMD_VALUE_NUMBER, "view slew coordinates must be finite numbers", NULL);
    }
    if (argc < 2)
	return slew_validation_result(result, BU_CMD_VALIDATE_INCOMPLETE, token,
	    BU_CMD_VALUE_NUMBER, "one more view slew coordinate expected", NULL);
    return slew_validation_result(result, BU_CMD_VALIDATE_VALID, token,
	BU_CMD_VALUE_VECTOR, "view slew vector", "ged.vector");
}

static const struct bu_cmd_operand slew_schema_operands[] = {
    BU_CMD_OPERAND("coordinates", BU_CMD_VALUE_RAW, 1, 3,
	"Packed X Y [Z] vector or separate X Y [Z] view coordinates", NULL),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_schema slew_cmd_schema = {
    "slew", "Slew the view center", NULL, slew_schema_operands,
    BU_CMD_PARSE_STOP_AT_FIRST_OPERAND, {slew_schema_validate}
};
static const struct bu_cmd_schema sv_cmd_schema = {
    "sv", "Slew the view center", NULL, slew_schema_operands,
    BU_CMD_PARSE_STOP_AT_FIRST_OPERAND, {slew_schema_validate}
};
static const struct bu_cmd_schema vslew_cmd_schema = {
    "vslew", "Slew the view center", NULL, slew_schema_operands,
    BU_CMD_PARSE_STOP_AT_FIRST_OPERAND, {slew_schema_validate}
};

static const struct bu_cmd_schema *
slew_schema_for_command(const char *command)
{
    if (BU_STR_EQUAL(command, "sv"))
	return &sv_cmd_schema;
    if (BU_STR_EQUAL(command, "vslew"))
	return &vslew_cmd_schema;
    return &slew_cmd_schema;
}

#define GED_SLEW_COMMANDS(X, XID) \
    X(slew, ged_slew_core, GED_CMD_DEFAULT, &slew_cmd_schema) \
    X(sv, ged_slew_core, GED_CMD_DEFAULT, &sv_cmd_schema) \
    X(vslew, ged_slew_core, GED_CMD_DEFAULT, &vslew_cmd_schema) \

GED_DECLARE_COMMAND_SET_WITH_NATIVE_SCHEMA(GED_SLEW_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST_WITH_NATIVE_SCHEMA("libged_slew", 1, GED_SLEW_COMMANDS)

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
