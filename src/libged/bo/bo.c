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
#include "ged/commands.h"


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

static const struct bu_cmd_option bo_schema_options[] = {
    BU_CMD_FLAG("i", NULL, struct bo_args, input_mode, "Import a file as a binary object"),
    BU_CMD_FLAG("o", NULL, struct bo_args, output_mode, "Export a binary object to a file"),
    BU_CMD_OPTION_NULL
};
static const struct bu_cmd_operand bo_input_operands[] = {
    BU_CMD_OPERAND_KEYWORDS("major_type", BU_CMD_VALUE_KEYWORD, 1, 1,
	"Uniform binary major type", NULL, bo_major_types),
    BU_CMD_OPERAND_KEYWORDS("minor_type", BU_CMD_VALUE_KEYWORD, 1, 1,
	"Uniform binary element type", NULL, bo_minor_types),
    BU_CMD_OPERAND("output_object", BU_CMD_VALUE_STRING, 1, 1,
	"Destination object name", NULL),
    GED_CMD_OPERAND_FILE("input_file", 1, 1, "Source binary file"),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_operand bo_output_operands[] = {
    GED_CMD_OPERAND_FILE("output_file", 1, 1, "Destination binary file"),
    BU_CMD_OPERAND("input_object", BU_CMD_VALUE_DB_OBJECT, 1, 1,
	"Source uniform binary object", GED_CMD_PROVIDER_DB_OBJECT_BINARY),
    BU_CMD_OPERAND_NULL
};
static const char * const bo_input_case[] = {"i", NULL};
static const char * const bo_output_case[] = {"o", NULL};
static const char * const bo_mode_options[] = {"i", "o", NULL};
static const struct bu_cmd_schema_case bo_schema_cases[] = {
    BU_CMD_SCHEMA_CASE("input", "Import a file as a binary object",
	BU_CMD_CONDITION_ALL_OPTIONS_PRESENT, bo_input_case, bo_input_operands, NULL),
    BU_CMD_SCHEMA_CASE("output", "Export a binary object to a file",
	BU_CMD_CONDITION_ALL_OPTIONS_PRESENT, bo_output_case, bo_output_operands, NULL),
    BU_CMD_SCHEMA_CASE_DEFAULT("mode", "Select -i or -o", NULL, NULL),
    BU_CMD_SCHEMA_CASE_NULL
};
static const struct bu_cmd_constraint bo_schema_constraints[] = {
    BU_CMD_CONSTRAINT_OPTIONS(bo_mode_options, 1, 1,
	"exactly one of -i or -o is required"),
    BU_CMD_CONSTRAINT_NULL
};
static const struct bu_cmd_schema bo_cmd_schema = {
    "bo", "Import or export uniform binary objects", bo_schema_options,
	NULL, BU_CMD_PARSE_OPTIONS_FIRST,
	BU_CMD_SCHEMA_META_CASES(NULL, bo_schema_constraints, NULL, NULL,
	    bo_schema_cases)
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
    int operand_index;

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_READ_ONLY(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    argv0 = argv[0];

    /* must be wanting help */
    if (argc == 1) {
	ged_cmd_help_append(gedp->ged_result_str, argv0, argv0);
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
	ged_cmd_help_append(gedp->ged_result_str, argv0, argv0);
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
	ged_cmd_help_append(gedp->ged_result_str, argv0, argv0);
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
