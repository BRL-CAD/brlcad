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
#include "bu/arg.h"
#include "bu/file.h"
#include "bu/path.h"
#include "ged.h"
#include "../ged_private.h"

struct editit_args {
    int print_help;
    const char *editstring;
    const char *filename;
};

static const struct bu_arg_option editit_options[] = {
    BU_ARG_FLAG("h", "help", struct editit_args, print_help,
	"Print help and exit"),
    BU_ARG_ALIAS("?", NULL, "help", 0),
    BU_ARG_VALUE("e", NULL, struct editit_args, editstring,
	BU_ARG_VALUE_STRING, BU_ARG_REQUIRED, "editstring",
	"Specify edit string (deprecated)"),
    BU_ARG_VALUE("f", NULL, struct editit_args, filename,
	BU_ARG_VALUE_FILE, BU_ARG_REQUIRED, "file", "Specify file to edit"),
    BU_ARG_OPTION_NULL
};

static const struct bu_arg_operand editit_help_operands[] = {
    BU_ARG_OPERAND("file", BU_ARG_VALUE_FILE, 0, 1,
	"File named while requesting help", "ged.file_path"),
    BU_ARG_OPERAND_NULL
};
static const struct bu_arg_operand editit_file_operands[] = {
    BU_ARG_OPERAND("file", BU_ARG_VALUE_FILE, 1, 1,
	"File to edit", "ged.file_path"),
    BU_ARG_OPERAND_NULL
};
static const char * const editit_help_options[] = {"help", NULL};
static const char * const editit_option_file[] = {"f", NULL};
static const struct bu_arg_schema_case editit_cases[] = {
    BU_ARG_SCHEMA_CASE("help", "Display command help",
	BU_ARG_CONDITION_ANY_OPTION_PRESENT, editit_help_options,
	editit_help_operands, NULL),
    BU_ARG_SCHEMA_CASE("option-file", "Take the file from the -f option",
	BU_ARG_CONDITION_ANY_OPTION_PRESENT, editit_option_file, NULL, NULL),
    BU_ARG_SCHEMA_CASE_DEFAULT("operand-file",
	"Take the file from the positional operand", editit_file_operands, NULL),
    BU_ARG_SCHEMA_CASE_NULL
};

static const struct bu_arg_schema editit_cmd_schema =
    BU_ARG_SCHEMA_CASED("editit", "Edit a file with the configured editor",
	editit_options, BU_ARG_PARSE_INTERSPERSED, editit_cases);

static void
editit_show_help(struct ged *gedp, const char *UNUSED(usage))
{
    char *help = ged_cmd_help("editit", "editit");

    if (help) {
	bu_vls_strcat(gedp->ged_result_str, help);
	bu_free(help, "editit standard help");
    }
}


int
ged_editit_core(struct ged *gedp, int argc, const char *argv[])
{
    const char *usage = "editit [opts] <filename>";
    struct editit_args args = {0, NULL, NULL};
    const char *filename;
    int operand_count;
    int operand_index;
    int ret = 0;

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);


    if (argc == 1) {
	/* must be wanting help */
	editit_show_help(gedp, usage);
	return GED_HELP;
    }


    argc--; argv++;
    operand_index = bu_arg_schema_parse_complete(&editit_cmd_schema, &args,
	gedp->ged_result_str, argc, argv);
    if (operand_index < 0) {
	editit_show_help(gedp, usage);
	return BRLCAD_ERROR;
    }
    argv += operand_index;
    operand_count = argc - operand_index;

    if (args.print_help) {
	if (operand_count > 1) {
	    editit_show_help(gedp, usage);
	    return BRLCAD_ERROR;
	}
	editit_show_help(gedp, usage);
	return BRLCAD_OK;
    }

    if ((args.filename && operand_count) || (!args.filename && operand_count != 1)) {
	bu_vls_printf(gedp->ged_result_str, "file required exactly once\n");
	editit_show_help(gedp, usage);
	return BRLCAD_ERROR;
    }
    filename = args.filename ? args.filename : argv[0];

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
