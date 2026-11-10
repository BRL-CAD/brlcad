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

#include "bu/app.h"
#include "bu/cmd.h"
#include "bu/env.h"
#include "bu/file.h"
#include "bu/arg.h"
#include "bu/process.h"

#include "../ged_private.h"

enum { MAN_DEFAULT_TEXT_WIDTH = 80 };

struct man_args {
    int help;
    int enable_gui;
    int disable_gui;
    struct bu_vls language;
    char section;
    int width;
};

static int
man_width_validate(const struct bu_opt_desc *UNUSED(option), size_t argc,
	const char **argv, size_t cursor_arg, void *UNUSED(context),
	void *UNUSED(data), struct bu_opt_validate_result *result)
{
    if (argv && cursor_arg < argc &&
	bu_arg_positive_integer_validate(NULL, argv[cursor_arg]) != 0) {
	result->state = BU_OPT_VALIDATE_INVALID;
	result->hint = "width must be a positive integer";
    }
    return 0;
}


#define MAN_OPTIONS(args) \
    BU_OPT_FLAG(args, "h", "help", help, "Print help and exit"), \
    BU_OPT_FLAG(args, "H", NULL, help, ""), \
    BU_OPT_FLAG(args, "?", NULL, help, ""), \
    BU_OPT_FLAG(args, "g", "gui", enable_gui, \
	"Force use of the graphical manual-page viewer"), \
    BU_OPT_FLAG(args, NULL, "no-gui", disable_gui, \
	"Force use of the command-line manual-page viewer"), \
    BU_OPT_FLAG(args, NULL, "dump", disable_gui, \
	"Return formatted text in the command result"), \
    BU_OPT_INT(args, NULL, "width", width, "columns", \
	"Set formatted text width (default: 80)"), \
    BU_OPT_LANG(args, "L", "language", language, "code", \
	"Select a lower-case ISO 639-1 manual language"), \
    BU_OPT_SECTION(args, "S", "section", section, "section", \
	"Select manual section 1, 3, 5, or n"),

BU_OPT_DESC_BUILDER(man_options, struct man_args, MAN_OPTIONS);

static const ged_opt_rule man_opt_rules[] = {
    GED_RULE_ALIAS("H ?", "help"),
    GED_RULE_ALIAS("dump", "no-gui"),
    GED_RULE_VALUE_VALIDATE("width", BU_OPT_VALUE_INTEGER, "positive integer",
	man_width_validate, NULL),
    GED_RULE_CONFLICTS("gui no-gui", "GUI and command line modes conflict"),
    GED_RULE_OPERANDS(BU_ARG_CONDITION_ANY_OPTION_PRESENT, "no-gui", 1, 2,
	"Command line mode requires a manual page"),
    GED_RULE_NULL
};
static const ged_opt_spec man_opt_spec =
    GED_OPT_WITH("man", "Open a BRL-CAD manual page", man_options,
	"interspersed manual_page:string? trailing_section:string?", man_opt_rules);


static void
man_show_help(struct ged *gedp, const char *command)
{
    char *help = ged_cmd_help(command, command);

    if (help) {
	bu_vls_strcat(gedp->ged_result_str, help);
	bu_free(help, "man standard help");
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
    int operand_count;
    int next_arg = 6;
    int process_options = BU_PROCESS_HIDE_WINDOW;
    struct bu_process *process = NULL;
    const char *command = argv[0];
    const char *man_name = NULL;
    const char *brlman = NULL;
    char brlmancmd[MAXPATHLEN] = {0};
    const char *brlman_exec_cmd[11] = {NULL};
    struct bu_vls man_sect = BU_VLS_INIT_ZERO;
    struct bu_vls man_width = BU_VLS_INIT_ZERO;
    struct man_args args = {0, 0, 0, BU_VLS_INIT_ZERO, 'n', MAN_DEFAULT_TEXT_WIDTH};

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    argc--; argv++;
    operand_count = bu_opt_parse_build_with_policy(gedp->ged_result_str, argc, argv,
	man_options, &args, BU_OPT_PARSE_INTERSPERSED_END_MARKER);
    if (operand_count < 0) {
	man_show_help(gedp, command);
	ret = BRLCAD_ERROR;
	goto cleanup;
    }
    if (args.help) {
	man_show_help(gedp, command);
	ret = GED_HELP;
	goto cleanup;
    }

    if (operand_count == 1) {
	man_name = argv[0];
    } else if (operand_count == 2) {
	int section_index = -1;
	for (int i = 0; i < operand_count; i++) {
	    if (bu_arg_man_section_validate(NULL, argv[i]) == 0) {
		section_index = i;
		args.section = argv[i][0];
		break;
	    }
	}
	if (section_index < 0) {
	    bu_vls_printf(gedp->ged_result_str,
		"Error - need a single manual page name and an optional section");
	    ret = BRLCAD_ERROR;
	    goto cleanup;
	}
	man_name = argv[section_index ? 0 : 1];
    } else if (operand_count > 2) {
	bu_vls_printf(gedp->ged_result_str,
	    "Error - need a single manual page name and an optional section");
	ret = BRLCAD_ERROR;
	goto cleanup;
    }

    /* Explicit options take precedence over environment defaults. */
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
	    if (bu_arg_iso639_1_validate(&langparse_msg, lang_flags) != 0) {
		bu_vls_printf(gedp->ged_result_str, "getenv(GED_MAN_LANG_MODE) failure: %s", bu_vls_cstr(&langparse_msg));
		bu_vls_free(&langparse_msg);
		ret = BRLCAD_ERROR;
		goto cleanup;
	    }
	    bu_vls_strcat(&args.language, lang_flags);
	    bu_vls_free(&langparse_msg);
	}
    }

    if (args.width <= 0) {
	bu_vls_printf(gedp->ged_result_str, "Error - width must be positive.\n");
	ret = BRLCAD_ERROR;
	goto cleanup;
    }
    if (args.disable_gui && !man_name) {
	bu_vls_printf(gedp->ged_result_str, "Error - please specify a man page.\n");
	ret = BRLCAD_ERROR;
	goto cleanup;
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
    bu_vls_sprintf(&man_width, "%d", args.width);
    brlman_exec_cmd[0] = brlmancmd;
    brlman_exec_cmd[1] = "-L";
    brlman_exec_cmd[2] = bu_vls_strlen(&args.language) ? bu_vls_cstr(&args.language) : "en";
    brlman_exec_cmd[3] = "-S";
    brlman_exec_cmd[4] = bu_vls_cstr(&man_sect);
    brlman_exec_cmd[5] = args.disable_gui ? "--dump" : "--gui";
    if (args.disable_gui) {
	brlman_exec_cmd[next_arg++] = "--width";
	brlman_exec_cmd[next_arg++] = bu_vls_cstr(&man_width);
    }
    if (man_name) {
	brlman_exec_cmd[next_arg++] = "--";
	brlman_exec_cmd[next_arg++] = man_name;
    }

    if (args.disable_gui)
	process_options |= BU_PROCESS_OUT_EQ_ERR;
    bu_process_create(&process, brlman_exec_cmd, process_options);
    if (!process || bu_process_pid(process) == -1) {
	bu_vls_printf(gedp->ged_result_str, "Unable to launch brlman viewer.\n");
	if (process)
	    bu_process_wait_n(&process, 0);
	ret = BRLCAD_ERROR;
    } else if (args.disable_gui) {
	char buffer[BUFSIZ];
	int count;
	while ((count = bu_process_read_n(process, BU_PROCESS_STDOUT,
		(int)sizeof(buffer), buffer)) > 0)
	    bu_vls_strncat(gedp->ged_result_str, buffer, (size_t)count);
	if (count < 0) {
	    bu_vls_printf(gedp->ged_result_str, "Unable to read brlman output.\n");
	    bu_process_terminate(process);
	}
	int exit_status = bu_process_wait_n(&process, 0);
	if (count < 0 || exit_status != 0)
	    ret = BRLCAD_ERROR;
    }

cleanup:
    bu_vls_free(&man_sect);
    bu_vls_free(&man_width);
    bu_vls_free(&args.language);
    return ret;
}


#include "../include/plugin.h"

#define GED_MAN_COMMANDS(X, XID) \
    X(man, ged_man_core, GED_CMD_DEFAULT, &man_opt_spec) \

GED_DECLARE_COMMAND_SET_WITH_OPT_SPEC(GED_MAN_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST_WITH_OPT_SPEC("libged_man", 1, GED_MAN_COMMANDS)

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
