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

#define EDITIT_OPTIONS(args) \
    BU_OPT_FLAG(args, "h", "help", print_help, "Print help and exit"), \
    BU_OPT_FLAG(args, "?", NULL, print_help, ""), \
    BU_OPT_STR(args, "e", NULL, editstring, "editstring", \
	"Specify edit string (deprecated)"), \
    BU_OPT_STR(args, "f", NULL, filename, "file", "Specify file to edit"),

BU_OPT_DESC_BUILDER(editit_options, struct editit_args, EDITIT_OPTIONS);

static const ged_opt_rule editit_opt_rules[] = {
    GED_RULE_ALIAS("?", "help"),
    GED_RULE_SEMANTIC("f", BU_CMD_VALUE_FILE, "ged.file_path", "file"),
    GED_RULE_WHEN_HELP("help", "Display command help", "file:file?"),
    GED_RULE_WHEN_HELP("f", "Take the file from the -f option", ""),
    GED_RULE_OTHERWISE_HELP("Take the file from the positional operand", "file:file"),
    GED_RULE_NULL
};

static const ged_opt_spec editit_opt_spec =
    GED_OPT_FORMS("editit", "Edit a file with the configured editor",
	editit_options, editit_opt_rules);

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
    int ret = 0;

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);


    if (argc == 1) {
	/* must be wanting help */
	editit_show_help(gedp, usage);
	return GED_HELP;
    }


    argc--; argv++;
    operand_count = bu_opt_parse_build(gedp->ged_result_str, argc, argv,
	editit_options, &args);
    if (operand_count < 0) {
	editit_show_help(gedp, usage);
	return BRLCAD_ERROR;
    }

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
    X(editit, ged_editit_core, GED_CMD_DEFAULT, &editit_opt_spec) \

GED_DECLARE_COMMAND_SET_WITH_OPT_SPEC(GED_EDITIT_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST_WITH_OPT_SPEC("libged_editit", 1, GED_EDITIT_COMMANDS)

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
