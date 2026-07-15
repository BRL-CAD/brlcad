/*                        E D I T I T . C
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
/** @file libged/editit.c
 *
 * The editit function.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <string.h>

#ifdef HAVE_SYS_TYPES_H
#  include <sys/types.h>
#endif

#include "bio.h"
#include "bresource.h"

#include "bu/app.h"
#include "bu/cmdschema.h"
#include "bu/file.h"
#include "bu/path.h"
#include "ged.h"
#include "../ged_private.h"

struct editit_args {
    int print_help;
    const char *editstring;
    const char *filename;
};

static const struct bu_cmd_option editit_schema_options[] = {
    BU_CMD_FLAG("h", "help", struct editit_args, print_help, "Print help and exit"),
    BU_CMD_ALIAS_SHORT("?", "help", 1),
    BU_CMD_STRING("e", NULL, struct editit_args, editstring, "editstring",
	"Specify edit string (deprecated)"),
    BU_CMD_FILE("f", NULL, struct editit_args, filename, "file", "Specify file to edit"),
    BU_CMD_OPTION_NULL
};
static const struct bu_cmd_operand editit_schema_operands[] = {
    BU_CMD_OPERAND("file", BU_CMD_VALUE_FILE, 0, 1, "File to edit", "ged.file_path"),
    BU_CMD_OPERAND_NULL
};

static int
editit_schema_validate(const struct bu_cmd_schema *schema, size_t argc, const char **argv,
	size_t cursor_arg, struct bu_cmd_validate_result *result)
{
    struct bu_cmd_schema flat = *schema;
    size_t operand_count;
    int has_help;
    int has_file;
    int ret;

    flat.validation.custom_validate = NULL;
    ret = bu_cmd_schema_validate(&flat, argc, argv, cursor_arg, result);
    if (ret || result->state != BU_CMD_VALIDATE_VALID)
	return ret;

    has_help = bu_cmd_schema_option_present(schema, argc, argv, "help");
    if (has_help)
	return 0;

    has_file = bu_cmd_schema_option_present(schema, argc, argv, "f");
    operand_count = bu_cmd_schema_operand_count(schema, argc, argv);
    if ((has_file && operand_count == 0) || (!has_file && operand_count == 1))
	return 0;

    if (!has_file && operand_count == 0) {
	result->state = BU_CMD_VALIDATE_INCOMPLETE;
	result->expected |= BU_CMD_EXPECT_OPERAND;
	result->hint = "file required";
	return 0;
    }

    bu_cmd_validate_result_clear(result);
    result->state = BU_CMD_VALIDATE_INVALID;
    result->token_start = cursor_arg < argc ? cursor_arg : argc - 1;
    result->token_end = result->token_start;
    result->expected = BU_CMD_EXPECT_NONE;
    result->completion_type = BU_CMD_VALUE_FILE;
    result->hint = "file specified both by -f and as an operand";
    return 0;
}

static const struct bu_cmd_schema editit_cmd_schema = {
    "editit", "Edit a file with the configured editor", editit_schema_options,
    editit_schema_operands, BU_CMD_PARSE_INTERSPERSED, {editit_schema_validate}
};

static void
editit_show_help(struct ged *gedp, const char *usage)
{
    char *option_help = bu_cmd_schema_describe(&editit_cmd_schema);

    bu_vls_printf(gedp->ged_result_str, "Usage: %s\n", usage);
    if (option_help) {
	bu_vls_printf(gedp->ged_result_str, "Options:\n%s", option_help);
	bu_free(option_help, "editit native option help");
    }
}


int
ged_editit_core(struct ged *gedp, int argc, const char *argv[])
{
    const char *usage = "editit [opts] <filename>";
    struct editit_args args = {0, NULL, NULL};
    const char *filename;
    int operand_index;
    int ret = 0;

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);


    if (argc == 1) {
	/* must be wanting help */
	editit_show_help(gedp, usage);
	return GED_HELP;
    }


    operand_index = bu_cmd_schema_parse_complete(&editit_cmd_schema, &args,
	gedp->ged_result_str, argc - 1, argv + 1);
    if (operand_index < 0) {
	editit_show_help(gedp, usage);
	return BRLCAD_ERROR;
    }

    if (args.print_help) {
	editit_show_help(gedp, usage);
	return BRLCAD_OK;
    }

    filename = args.filename ? args.filename : argv[operand_index + 1];

    ret = _ged_editit(gedp, args.editstring, filename);
    return ret;
}

#include "../include/plugin.h"

#define GED_EDITIT_COMMANDS(X, XID) \
    X(editit, ged_editit_core, GED_CMD_DEFAULT, &editit_cmd_schema) \

GED_DECLARE_COMMAND_SET_WITH_NATIVE_SCHEMA(GED_EDITIT_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST_WITH_NATIVE_SCHEMA("libged_editit", 1, GED_EDITIT_COMMANDS)

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
