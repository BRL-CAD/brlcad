/*                         M R O T . C
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
/** @file libged/mrot.c
 *
 * The mrot command.
 *
 */

#include "common.h"

#include <errno.h>
#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "bu/cmdschema.h"

#include "../ged_private.h"

static const struct bu_cmd_schema *mrot_schema(void);

static int
mrot_number_valid(const char *arg)
{
    char *end = NULL;
    double value;

    errno = 0;
    value = strtod(arg, &end);
    return arg && arg[0] && errno != ERANGE && end && !*end && isfinite(value) &&
	!(sizeof(fastf_t) == sizeof(float) && (value > FLT_MAX || value < -FLT_MAX));
}

static int
mrot_vector_valid(const char *arg)
{
    vect_t vector;

    return arg && bn_decode_vect(vector, arg) == 3 &&
	isfinite(vector[X]) && isfinite(vector[Y]) && isfinite(vector[Z]);
}

static int
mrot_validation_result(struct bu_cmd_validate_result *result,
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
ged_mrot_core(struct ged *gedp, int argc, const char *argv[])
{
    char *av[6];
    char coord;
    int ac;
    int i;
    int ret;
    mat_t rmat;
    static const char *usage = "x y z";
    int operand_index;
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


    operand_index = bu_cmd_schema_parse_complete(mrot_schema(),
	&parse_dummy, gedp->ged_result_str, argc - 1, argv + 1);
    if (operand_index < 0) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    av[0] = (char *)argv[0];
    av[1] = "-m";
    ac = argc+1;
    for (i = 1; i < argc; ++i)
	av[i+1] = (char *)argv[i];
    av[i+1] = (char *)0;

    if ((ret = ged_rot_args(gedp, ac, (const char **)av, &coord, rmat)) != BRLCAD_OK)
	return ret;

    return _ged_do_rot(gedp, coord, rmat, NULL);
}


#include "../include/plugin.h"

static int
mrot_schema_validate(const struct bu_cmd_schema *schema, size_t argc,
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
	if (mrot_vector_valid(argv[0]))
	    return mrot_validation_result(result, BU_CMD_VALIDATE_VALID, token,
		BU_CMD_VALUE_VECTOR, "packed model rotation vector", "ged.vector");
	if (mrot_number_valid(argv[0]))
	    return mrot_validation_result(result, BU_CMD_VALIDATE_INCOMPLETE, token,
		BU_CMD_VALUE_NUMBER, "two more model rotation angles expected", NULL);
	return mrot_validation_result(result, BU_CMD_VALIDATE_INVALID, token,
	    BU_CMD_VALUE_VECTOR, "packed vector or numeric angle expected", "ged.vector");
    }

    for (size_t i = 0; i < argc; i++) {
	if (!mrot_number_valid(argv[i]))
	    return mrot_validation_result(result, BU_CMD_VALIDATE_INVALID, i,
		BU_CMD_VALUE_NUMBER, "model rotation angles must be finite numbers", NULL);
    }
    if (argc < 3)
	return mrot_validation_result(result, BU_CMD_VALIDATE_INCOMPLETE, token,
	    BU_CMD_VALUE_NUMBER, "one more model rotation angle expected", NULL);
    return mrot_validation_result(result, BU_CMD_VALIDATE_VALID, token,
	BU_CMD_VALUE_VECTOR, "model rotation vector", "ged.vector");
}

static const struct bu_cmd_operand mrot_schema_operands[] = {
    BU_CMD_OPERAND("angles", BU_CMD_VALUE_RAW, 1, 3,
	"Packed vector or X Y Z model rotation angles", NULL),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_schema mrot_cmd_schema = {
    "mrot", "Rotate the model view", NULL, mrot_schema_operands,
    BU_CMD_PARSE_STOP_AT_FIRST_OPERAND, {mrot_schema_validate}
};

static const struct bu_cmd_schema *
mrot_schema(void)
{
    return &mrot_cmd_schema;
}

#define GED_MROT_COMMANDS(X, XID) \
    X(mrot, ged_mrot_core, GED_CMD_DEFAULT, &mrot_cmd_schema) \

GED_DECLARE_COMMAND_SET_WITH_NATIVE_SCHEMA(GED_MROT_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST_WITH_NATIVE_SCHEMA("libged_mrot", 1, GED_MROT_COMMANDS)

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
