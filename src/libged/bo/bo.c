/*                             B O . C
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
/** @file libged/bo.c
 *
 * The 'bo' binary object command, used for importing and exporting
 * between files and binary objects stored in a geometry database.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "bu/cmdschema.h"
#include "ged.h"


struct bo_args {
    int input_mode;
    int output_mode;
};

static const char * const bo_major_types[] = {"u", NULL};
static const char * const bo_minor_types[] = {
    "f", "d", "c", "s", "i", "l", "C", "S", "I", "L", NULL
};

static int
bo_minor_type(const char *minor, unsigned int *minor_type)
{
    unsigned int type = 0;

    if (!minor || minor[1] != '\0')
	return -1;
    switch (minor[0]) {
	case 'f': type = DB5_MINORTYPE_BINU_FLOAT; break;
	case 'd': type = DB5_MINORTYPE_BINU_DOUBLE; break;
	case 'c': type = DB5_MINORTYPE_BINU_8BITINT; break;
	case 's': type = DB5_MINORTYPE_BINU_16BITINT; break;
	case 'i': type = DB5_MINORTYPE_BINU_32BITINT; break;
	case 'l': type = DB5_MINORTYPE_BINU_64BITINT; break;
	case 'C': type = DB5_MINORTYPE_BINU_8BITINT_U; break;
	case 'S': type = DB5_MINORTYPE_BINU_16BITINT_U; break;
	case 'I': type = DB5_MINORTYPE_BINU_32BITINT_U; break;
	case 'L': type = DB5_MINORTYPE_BINU_64BITINT_U; break;
	default: return -1;
    }
    if (minor_type)
	*minor_type = type;
    return 0;
}

static int
bo_keyword_valid(const char * const *values, const char *value)
{
    if (!value)
	return 0;
    for (size_t i = 0; values[i]; i++)
	if (BU_STR_EQUAL(values[i], value))
	    return 1;
    return 0;
}

static void
bo_keyword_candidates(struct bu_cmd_validate_result *result,
	const char * const *values, const char *prefix)
{
    size_t count = 0;

    for (size_t i = 0; values[i]; i++)
	if (!prefix || !prefix[0] || bu_strncmp(values[i], prefix, strlen(prefix)) == 0)
	    count++;
    if (!count)
	return;
    result->completion_candidates = (const char **)bu_calloc(count + 1,
	sizeof(char *), "bo keyword candidates");
    for (size_t i = 0, oi = 0; values[i]; i++)
	if (!prefix || !prefix[0] || bu_strncmp(values[i], prefix, strlen(prefix)) == 0)
	    result->completion_candidates[oi++] = bu_strdup(values[i]);
    result->completion_count = count;
}

static int
bo_validation_result(struct bu_cmd_validate_result *result,
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

static void
bo_operand_role(int input, size_t role, bu_cmd_value_t *type,
	const char **hint, const char **provider, const char * const **keywords)
{
    *provider = NULL;
    *keywords = NULL;
    if (input) {
	switch (role) {
	    case 0: *type = BU_CMD_VALUE_KEYWORD; *hint = "major type"; *keywords = bo_major_types; break;
	    case 1: *type = BU_CMD_VALUE_KEYWORD; *hint = "minor binary type"; *keywords = bo_minor_types; break;
	    case 2: *type = BU_CMD_VALUE_STRING; *hint = "destination object name"; break;
	    default: *type = BU_CMD_VALUE_FILE; *hint = "source file"; *provider = "ged.file_path"; break;
	}
	return;
    }
    if (role == 0) {
	*type = BU_CMD_VALUE_FILE;
	*hint = "destination file";
	*provider = "ged.file_path";
    } else {
	*type = BU_CMD_VALUE_DB_OBJECT;
	*hint = "source binary object";
	*provider = "ged.db_object";
    }
}

static int
bo_mode_option(const char *arg)
{
    if (!arg || arg[0] != '-' || !arg[1])
	return 0;
    for (size_t i = 1; arg[i]; i++)
	if (arg[i] != 'i' && arg[i] != 'o')
	    return 0;
    return 1;
}

static size_t
bo_first_operand(size_t argc, const char **argv)
{
    size_t i = 0;

    while (i < argc && bo_mode_option(argv[i]))
	i++;
    if (i < argc && BU_STR_EQUAL(argv[i], "--"))
	i++;
    return i;
}

static int
bo_schema_validate(const struct bu_cmd_schema *schema, size_t argc,
	const char **argv, size_t cursor_arg, struct bu_cmd_validate_result *result)
{
    struct bu_cmd_schema flat = *schema;
    size_t operands;
    size_t first;
    int input;
    int output;
    int ret;

    flat.validation.custom_validate = NULL;
    ret = bu_cmd_schema_validate(&flat, argc, argv, cursor_arg, result);
    if (ret || result->state == BU_CMD_VALIDATE_INVALID)
	return ret;

    input = bu_cmd_schema_option_present(schema, argc, argv, "i");
    output = bu_cmd_schema_option_present(schema, argc, argv, "o");
    if (cursor_arg < argc && argv[cursor_arg] && argv[cursor_arg][0] == '-' &&
	argv[cursor_arg][1] && (result->expected & BU_CMD_EXPECT_OPTION))
	return 0;
    if (input && output)
	return bo_validation_result(result, BU_CMD_VALIDATE_INVALID,
	    cursor_arg < argc ? cursor_arg : argc, BU_CMD_VALUE_FLAG,
	    "exactly one of -i or -o is required", NULL);
    if (!input && !output) {
	/* Retain the flat schema's -i/-o candidates while specifying that a
	 * mode, rather than an arbitrary raw operand, is required here. */
	result->state = argc ? BU_CMD_VALIDATE_INVALID : BU_CMD_VALIDATE_INCOMPLETE;
	result->token_start = cursor_arg < argc ? cursor_arg : argc;
	result->token_end = result->token_start;
	result->expected = BU_CMD_EXPECT_OPTION;
	result->completion_type = BU_CMD_VALUE_FLAG;
	result->hint = "-i or -o mode required";
	return 0;
    }

    operands = bu_cmd_schema_operand_count(schema, argc, argv);
    first = bo_first_operand(argc, argv);
    size_t required = input ? 4 : 2;
    if (operands > required)
	return bo_validation_result(result, BU_CMD_VALIDATE_INVALID,
	    cursor_arg < argc ? cursor_arg : argc, BU_CMD_VALUE_STRING,
	    "too many operands for selected mode", NULL);

    if (cursor_arg < argc && cursor_arg >= first) {
	size_t role = cursor_arg - first;
	bu_cmd_value_t type;
	const char *hint;
	const char *provider;
	const char * const *keywords;
	bu_cmd_validate_state_t state = BU_CMD_VALIDATE_VALID;

	bo_operand_role(input, role, &type, &hint, &provider, &keywords);
	if (keywords && !bo_keyword_valid(keywords, argv[cursor_arg]))
	    state = BU_CMD_VALIDATE_INVALID;
	bo_validation_result(result, state, cursor_arg, type,
	    state == BU_CMD_VALIDATE_VALID ? hint : "invalid binary type", provider);
	if (keywords)
	    bo_keyword_candidates(result, keywords, argv[cursor_arg]);
	return 0;
    }

    /* Validate all confirmed type tokens before an execution parse can reach
     * the file or database operations. */
    if (input && operands > 0 && first < argc &&
	!bo_keyword_valid(bo_major_types, argv[first]))
	return bo_validation_result(result, BU_CMD_VALIDATE_INVALID, first,
	    BU_CMD_VALUE_KEYWORD, "invalid major type", NULL);
    if (input && operands > 1 && first + 1 < argc &&
	bo_minor_type(argv[first + 1], NULL) != 0)
	return bo_validation_result(result, BU_CMD_VALIDATE_INVALID, first + 1,
	    BU_CMD_VALUE_KEYWORD, "invalid minor binary type", NULL);

    if (operands < required) {
	bu_cmd_value_t type;
	const char *hint;
	const char *provider;
	const char * const *keywords;
	bo_operand_role(input, operands, &type, &hint, &provider, &keywords);
	bo_validation_result(result, BU_CMD_VALIDATE_INCOMPLETE, argc, type, hint, provider);
	if (keywords)
	    bo_keyword_candidates(result, keywords, "");
    }
    return 0;
}

static const struct bu_cmd_option bo_schema_options[] = {
    BU_CMD_FLAG("i", NULL, struct bo_args, input_mode, "Import a file as a binary object"),
    BU_CMD_FLAG("o", NULL, struct bo_args, output_mode, "Export a binary object to a file"),
    BU_CMD_OPTION_NULL
};
static const struct bu_cmd_operand bo_schema_operands[] = {
    BU_CMD_OPERAND("mode_operands", BU_CMD_VALUE_RAW, 0, 4,
	"Type, object, and file operands selected by mode", NULL),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_schema bo_cmd_schema = {
    "bo", "Import or export uniform binary objects", bo_schema_options,
    bo_schema_operands, BU_CMD_PARSE_OPTIONS_FIRST, {bo_schema_validate}
};


int
ged_bo_core(struct ged *gedp, int argc, const char *argv[])
{
    unsigned int minor_type=0;
    char *obj_name;
    char *file_name;
    struct bo_args args = {0, 0};
    struct rt_binunif_internal *bip;
    struct rt_db_internal intern;
    struct directory *dp;
    const char *argv0;
    static const char *usage = "{-i major_type minor_type | -o} dest source";
    int operand_index;

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_READ_ONLY(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    argv0 = argv[0];

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv0, usage);
	return GED_HELP;
    }

    /* check that we are using a version 5 database */
    if (db_version(gedp->dbip) < 5) {
	bu_vls_printf(gedp->ged_result_str, "This is an older database version.\nIt does not support binary objects.Use \"dbupgrade\" to upgrade this database to the current version.\n");
	return BRLCAD_ERROR;
    }

    operand_index = bu_cmd_schema_parse_complete(&bo_cmd_schema, &args,
	gedp->ged_result_str, argc - 1, argv + 1);
    if (operand_index < 0) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv0, usage);
	return BRLCAD_ERROR;
    }
    argc -= operand_index + 1;
    argv += operand_index + 1;


    if (args.input_mode) {
	(void)bo_minor_type(argv[1], &minor_type);
	obj_name = (char *)argv[2];
	GED_CHECK_EXISTS(gedp, obj_name, LOOKUP_QUIET, BRLCAD_ERROR);
	file_name = (char *)argv[3];

	/* make a binunif of the entire file */
	struct rt_wdb *wdbp = wdb_dbopen(gedp->dbip, RT_WDB_TYPE_DB_DEFAULT);
	if (rt_mk_binunif (wdbp, obj_name, file_name, minor_type, 0)) {
	    bu_vls_printf(gedp->ged_result_str, "Error creating %s", obj_name);
	    return BRLCAD_ERROR;
	}

    } else if (args.output_mode) {
	FILE *fp;

	file_name = (char *)argv[0];
	obj_name = (char *)argv[1];

	dp = db_lookup(gedp->dbip, obj_name, LOOKUP_NOISY);
	if (dp == RT_DIR_NULL) {
	    return BRLCAD_ERROR;
	}
	if (!(dp->d_major_type & DB5_MAJORTYPE_BINARY_MASK)) {
	    bu_vls_printf(gedp->ged_result_str, "%s is not a binary object", obj_name);
	    return BRLCAD_ERROR;
	}

	if (dp->d_major_type != DB5_MAJORTYPE_BINARY_UNIF) {
	    bu_vls_printf(gedp->ged_result_str, "source must be a uniform binary object");
	    return BRLCAD_ERROR;
	}

	fp = fopen(file_name, "w+b");
	if (fp == NULL) {
	    bu_vls_printf(gedp->ged_result_str, "Error: cannot open file %s for writing", file_name);
	    return BRLCAD_ERROR;
	}

	if (rt_db_get_internal(&intern, dp, gedp->dbip, NULL) < 0) {
	    bu_vls_printf(gedp->ged_result_str, "Error reading %s from database", dp->d_namep);
	    fclose(fp);
	    return BRLCAD_ERROR;
	}

	RT_CK_DB_INTERNAL(&intern);

	bip = (struct rt_binunif_internal *)intern.idb_ptr;
	if (bip->count < 1) {
	    bu_vls_printf(gedp->ged_result_str, "%s has no contents", obj_name);
	    fclose(fp);
	    rt_db_free_internal(&intern);
	    return BRLCAD_ERROR;
	}

	if (fwrite(bip->u.int8, bip->count * db5_type_sizeof_h_binu(bip->type),
		   1, fp) != 1) {
	    bu_vls_printf(gedp->ged_result_str, "Error writing contents to file");
	    fclose(fp);
	    rt_db_free_internal(&intern);
	    return BRLCAD_ERROR;
	}

	fclose(fp);
	rt_db_free_internal(&intern);

    } else {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv0, usage);
	return BRLCAD_ERROR;
    }

    return BRLCAD_OK;
}

#include "../include/plugin.h"

#define GED_BO_COMMANDS(X, XID) \
    X(bo, ged_bo_core, GED_CMD_DEFAULT, &bo_cmd_schema) \

GED_DECLARE_COMMAND_SET_WITH_NATIVE_SCHEMA(GED_BO_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST_WITH_NATIVE_SCHEMA("libged_bo", 1, GED_BO_COMMANDS)

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
