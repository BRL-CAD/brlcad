/*                          M A N . C
 * BRL-CAD
 *
 * Copyright (c) 1988-2026 United States Government as represented by
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
/** @addtogroup libged */
/** @{ */
/** @file libged/man.c
 *
 * Define a command for launching the man page viewer.
 */
/** @} */

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#ifdef HAVE_SYS_TYPES_H
#  include <sys/types.h>
#endif
#include "bresource.h"

#include "bn.h"
#include "bu/app.h"
#include "bu/cmd.h"
#include "bu/env.h"
#include "bu/file.h"
#include "bu/interrupt.h"
#include "bu/cmdschema.h"
#include "bu/process.h"
#include "vmath.h"

#include "../ged_private.h"

struct man_args {
    int help;
    int enable_gui;
    int disable_gui;
    struct bu_vls language;
    char section;
};

static const char * const man_language_option[] = {"language", NULL};
static const struct bu_cmd_constraint man_schema_constraints[] = {
    BU_CMD_CONSTRAINT_OPTIONS(man_language_option, 0, 1,
	"--language may be specified only once"),
    BU_CMD_CONSTRAINT_NULL
};
static const struct bu_cmd_option man_schema_options[] = {
    BU_CMD_FLAG("h", "help", struct man_args, help, "Print help and exit"),
    BU_CMD_ALIAS_SHORT("H", "help", 1),
    BU_CMD_ALIAS_SHORT("?", "help", 1),
    BU_CMD_FLAG("g", "gui", struct man_args, enable_gui,
	"Force use of the graphical manual-page viewer"),
    BU_CMD_FLAG(NULL, "no-gui", struct man_args, disable_gui,
	"Force use of the command-line manual-page viewer"),
    BU_CMD_ISO639_1("L", "language", struct man_args, language, "code",
	"Select a lower-case ISO 639-1 manual language"),
    BU_CMD_MAN_SECTION("S", "section", struct man_args, section, "section",
	"Select manual section 1, 3, 5, or n"),
    BU_CMD_OPTION_NULL
};
static const struct bu_cmd_operand man_schema_operands[] = {
    BU_CMD_OPERAND("manual_page", BU_CMD_VALUE_STRING, 0, 1,
	"Manual page name", NULL),
    BU_CMD_OPERAND("trailing_section", BU_CMD_VALUE_STRING, 0, 1,
	"Optional compatibility manual section", NULL),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_schema man_cmd_schema = {
    "man", "Open a BRL-CAD manual page", man_schema_options,
    man_schema_operands, BU_CMD_PARSE_INTERSPERSED,
    BU_CMD_SCHEMA_CONSTRAINTS(NULL, man_schema_constraints)
};


static void
man_show_help(struct ged *gedp, const char *command)
{
    char *option_help = bu_cmd_schema_describe(&man_cmd_schema);

    bu_vls_printf(gedp->ged_result_str,
	"Usage: %s [--gui|--no-gui] [-L language] [-S section] [manual_page [section]]",
	command);
    if (option_help) {
	bu_vls_printf(gedp->ged_result_str, "\nOptions:\n%s", option_help);
	bu_free(option_help, "man native option help");
    }
}


/**
 * Invoke brlman with the current environmental options, or
 * (if supplied) user defined options
 */
int
ged_man_core(struct ged *gedp, int argc, const char *argv[])
{
    int ret = BRLCAD_OK;
    int operand_index;
    int operand_count;
    const char **operands;
    const char *man_name = NULL;
    const char *brlman = NULL;
    char brlmancmd[MAXPATHLEN] = {0};
    const char *brlman_exec_cmd[8];
    struct bu_vls man_sect = BU_VLS_INIT_ZERO;
    struct man_args args = {0, 0, 0, BU_VLS_INIT_ZERO, 'n'};

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    operand_index = bu_cmd_schema_parse_complete(&man_cmd_schema, &args,
	gedp->ged_result_str, argc - 1, argv + 1);
    if (operand_index < 0) {
	man_show_help(gedp, argv[0]);
	ret = BRLCAD_ERROR;
	goto cleanup;
    }
    if (args.help) {
	man_show_help(gedp, argv[0]);
	ret = GED_HELP;
	goto cleanup;
    }

    operand_count = argc - 1 - operand_index;
    operands = argv + 1 + operand_index;
    if (operand_count == 1) {
	man_name = operands[0];
    } else if (operand_count == 2) {
	int section_index = -1;
	for (int i = 0; i < operand_count; i++) {
	    if (bu_cmd_man_section_validate(NULL, operands[i]) == 0) {
		section_index = i;
		args.section = operands[i][0];
		break;
	    }
	}
	if (section_index < 0) {
	    bu_vls_printf(gedp->ged_result_str,
		"Error - need a single manual page name and an optional section");
	    ret = BRLCAD_ERROR;
	    goto cleanup;
	}
	man_name = operands[section_index ? 0 : 1];
    }

    // Next, we check environment variables for any options.  If command line values are
    // set they override them, but if not let the environment tell us what we need
    if (!args.enable_gui && !args.disable_gui) {
	char *cmd_flags;
	cmd_flags = getenv("GED_MAN_CMDLINE_MODE");
	if (cmd_flags && bu_str_true(cmd_flags)) {
	    args.disable_gui = 1;
	}
    }
    if (!args.enable_gui && !args.disable_gui)
	args.enable_gui = 1;

    if (!bu_vls_strlen(&args.language)) {
	char *lang_flags;
	lang_flags = getenv("GED_MAN_LANG_MODE");
	if (lang_flags) {
	    struct bu_vls langparse_msg = BU_VLS_INIT_ZERO;
	    if (bu_cmd_iso639_1_validate(&langparse_msg, lang_flags) != 0) {
		bu_vls_printf(gedp->ged_result_str, "getenv(GED_MAN_LANG_MODE) failure: %s", bu_vls_cstr(&langparse_msg));
		bu_vls_free(&langparse_msg);
		ret = BRLCAD_ERROR;
		goto cleanup;
	    }
	    bu_vls_strcat(&args.language, lang_flags);
	    bu_vls_free(&langparse_msg);
	}
    }

    if (args.enable_gui && args.disable_gui) {
	bu_vls_printf(gedp->ged_result_str, "Error - both GUI and command line man viewer modes specified.");
	ret = BRLCAD_ERROR;
	goto cleanup;
    }

    brlman = bu_dir(brlmancmd, MAXPATHLEN, BU_DIR_BIN, "brlman", BU_DIR_EXT, NULL);
    if (!bu_file_exists(brlman, NULL)) {
	bu_vls_printf(gedp->ged_result_str, "ERROR: Unable to find 'brlman' viewer: %s.\n", brlman);
	ret = BRLCAD_ERROR;
	goto cleanup;
    }

    bu_vls_sprintf(&man_sect, "%c", args.section);
    brlman_exec_cmd[0] = (const char *)brlmancmd;
    brlman_exec_cmd[1] = "-L";
    brlman_exec_cmd[2] = (bu_vls_strlen(&args.language)) ? bu_vls_cstr(&args.language) : "en";
    brlman_exec_cmd[3] = "-S";
    brlman_exec_cmd[4] = bu_vls_cstr(&man_sect);
	if (args.enable_gui || args.disable_gui) {
	brlman_exec_cmd[5] = (args.disable_gui) ? "--no-gui" : "--gui";
	if (operand_count) {
	    brlman_exec_cmd[6] = man_name;
	    brlman_exec_cmd[7] = NULL;
	} else {
	    brlman_exec_cmd[6] = NULL;
	}
    } else {
	if (operand_count) {
	    brlman_exec_cmd[5] = man_name;
	    brlman_exec_cmd[6] = NULL;
	} else {
	    brlman_exec_cmd[5] = NULL;
	}
    }

    struct bu_process *p;
    bu_process_create(&p, (const char **)brlman_exec_cmd, BU_PROCESS_HIDE_WINDOW);
    if (bu_process_pid(p) == -1) {
	bu_vls_printf(gedp->ged_result_str, "\nunable to successfully launch subprocess: ");
	ret = BRLCAD_ERROR;
    }
cleanup:
    bu_vls_free(&man_sect);
    bu_vls_free(&args.language);
    return ret;
}


#include "../include/plugin.h"

#define GED_MAN_COMMANDS(X, XID) \
    X(man, ged_man_core, GED_CMD_DEFAULT, &man_cmd_schema) \

GED_DECLARE_COMMAND_SET_WITH_NATIVE_SCHEMA(GED_MAN_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST_WITH_NATIVE_SCHEMA("libged_man", 1, GED_MAN_COMMANDS)

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
