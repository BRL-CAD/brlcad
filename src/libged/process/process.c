/*                     P R O C E S S . C
 * BRL-CAD
 *
 * Copyright (c) 2019-2026 United States Government as represented by
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
/** @file libged/process.c
 *
 * Command for managing subprocesses launched by libged commands.
 *
 */

#include "common.h"

#include <stdlib.h>

#include "bu/cmdschema.h"
#include "bu/path.h"
#include "bu/process.h"
#include "../ged_private.h"


static char *process_native_help(void);


static void
_process_show_help(struct ged *gedp)
{
    char *help = process_native_help();

    if (help) {
	bu_vls_strcat(gedp->ged_result_str, help);
	bu_free(help, "process native tree help");
    }
}


static int
_ged_process_list(struct ged *gedp)
{
    struct bu_vls pid_str = BU_VLS_INIT_ZERO;
    struct bu_vls plist = BU_VLS_INIT_ZERO;
    struct ged_subprocess *rrp;
    unsigned int longest_pid = 0;

    /* Find the largest pid we'll have to print */
    for (size_t i = 0; i < BU_PTBL_LEN(&gedp->ged_subp); i++) {
	rrp = (struct ged_subprocess *)BU_PTBL_GET(&gedp->ged_subp, i);
	int pid = bu_process_pid(rrp->p);
	bu_vls_sprintf(&pid_str, "%d", pid);
	longest_pid = (bu_vls_strlen(&pid_str) > longest_pid) ? bu_vls_strlen(&pid_str) : longest_pid;
    }
    bu_vls_free(&pid_str);

    /* For each process print a line */
    for (size_t i = 0; i < BU_PTBL_LEN(&gedp->ged_subp); i++) {
	rrp = (struct ged_subprocess *)BU_PTBL_GET(&gedp->ged_subp, i);
	struct bu_vls pline = BU_VLS_INIT_ZERO;
	struct bu_vls cmdroot = BU_VLS_INIT_ZERO;
	const char * const *argv = NULL;
	int argc = bu_process_args_n(rrp->p, NULL, &argv);
	if (!argc || !argv) {
	    bu_log("Warning: no arguments for process!\n");
	    continue;
	}
	int pid = bu_process_pid(rrp->p);
	(void)bu_path_component(&cmdroot, argv[0], BU_PATH_BASENAME_EXTLESS);
	bu_vls_sprintf(&pline, "%*d %s", longest_pid, pid, bu_vls_cstr(&cmdroot));
	for (int j = 1; j < argc; j++) {
	    bu_vls_printf(&pline, " %s", argv[j]);
	}
	/* TODO - this should be intelligent based on current MGED/console terminal width... */
	bu_vls_trunc(&pline, 80);
	bu_vls_printf(&plist, "%s\n", bu_vls_cstr(&pline));
	bu_vls_free(&pline);
	bu_vls_free(&cmdroot);
    }

    if (bu_vls_strlen(&plist)) {
	struct bu_vls header = BU_VLS_INIT_ZERO;
	bu_vls_sprintf(&header, "%*cPID CMD", longest_pid-3, ' ');
	bu_vls_printf(gedp->ged_result_str, "%s\n%s", bu_vls_cstr(&header), bu_vls_cstr(&plist));
	bu_vls_free(&header);
    } else {
	bu_vls_printf(gedp->ged_result_str, "No currently running GED subprocesses.");
    }
    bu_vls_free(&plist);
    return BRLCAD_OK;
}

static int
_ged_process_pabort(struct ged *gedp, int argc, const char **argv)
{
    struct ged_subprocess *rrp;
    /* Note - looping over processes and then argv args to avoid trying
     * to double-terminate a process if someone provides the same pid
     * more than once.  There may be more efficient ways to do this, but
     * it shouldn't matter unless the subprocess and/or argv lists get
     * extremely long. */
    for (size_t i = 0; i < BU_PTBL_LEN(&gedp->ged_subp); i++) {
	rrp = (struct ged_subprocess *)BU_PTBL_GET(&gedp->ged_subp, i);
	int ppid = bu_process_pid(rrp->p);
	for (int j = 0; j < argc; j++) {
	    int pid;
	    if (!bu_cmd_integer_from_str(&pid, argv[j])) {
		bu_vls_printf(gedp->ged_result_str, "PID argument %s is not a valid process id.", argv[j]);
		return BRLCAD_ERROR;
	    }
	    if (ppid == pid) {
		bu_pid_terminate(bu_process_pid(rrp->p));
		rrp->aborted = 1;
		/* terminated it, no need to check other args for
		 * this process */
		break;
	    }
	}
    }
    return BRLCAD_OK;
}

static int
_ged_process_gabort(struct ged *gedp, int argc, const char **argv)
{
    struct ged_subprocess *rrp;
    struct bu_vls cmdroot = BU_VLS_INIT_ZERO;
    /* Note - looping over processes and then argv args to avoid trying
     * to double-terminate a process if someone provides args that would
     * otherwise resolve that way - with this arrangement, each process
     * can get terminated at most once. */
    for (size_t i = 0; i < BU_PTBL_LEN(&gedp->ged_subp); i++) {
	rrp = (struct ged_subprocess *)BU_PTBL_GET(&gedp->ged_subp, i);
	const char *cmd;
	int argcnt = bu_process_args_n(rrp->p, &cmd, NULL);
	bu_vls_trunc(&cmdroot, 0);
	if (argcnt > 0 && bu_path_component(&cmdroot, cmd, BU_PATH_BASENAME_EXTLESS)) {
	    for (int j = 0; j < argc; j++) {
		if (!bu_path_match(argv[j], bu_vls_cstr(&cmdroot), 0)) {
		    bu_pid_terminate(bu_process_pid(rrp->p));
		    rrp->aborted = 1;
		    /* terminated it, no need to check other args for
		     * this process */
		    break;
		}
	    }
	}
    }
    return BRLCAD_OK;
}


static const struct bu_cmd_operand process_pid_operands[] = {
    BU_CMD_OPERAND("process_ids", BU_CMD_VALUE_INTEGER, 1, BU_CMD_COUNT_UNLIMITED,
	"Process identifiers to abort", NULL),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_operand process_glob_operands[] = {
    BU_CMD_OPERAND("command_patterns", BU_CMD_VALUE_STRING, 1, BU_CMD_COUNT_UNLIMITED,
	"Command-name glob patterns", NULL),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_schema process_root_schema = {
    "process", "Manage GED subprocesses", NULL, NULL,
    BU_CMD_PARSE_OPTIONS_FIRST, BU_CMD_SCHEMA_CONSTRAINTS(NULL, NULL)
};
static const struct bu_cmd_schema process_list_schema = {
    "list", "List running GED subprocesses", NULL, NULL,
    BU_CMD_PARSE_STOP_AT_FIRST_OPERAND, BU_CMD_SCHEMA_CONSTRAINTS(NULL, NULL)
};
static const struct bu_cmd_schema process_pabort_schema = {
    "pabort", "Abort subprocesses by PID", NULL, process_pid_operands,
    BU_CMD_PARSE_STOP_AT_FIRST_OPERAND, BU_CMD_SCHEMA_CONSTRAINTS(NULL, NULL)
};
static const struct bu_cmd_schema process_gabort_schema = {
    "gabort", "Abort subprocesses by command glob", NULL, process_glob_operands,
    BU_CMD_PARSE_STOP_AT_FIRST_OPERAND, BU_CMD_SCHEMA_CONSTRAINTS(NULL, NULL)
};


static int
process_tree_parse(struct ged *gedp, const struct bu_cmd_schema *schema,
	int argc, const char *argv[])
{
    struct bu_vls msg = BU_VLS_INIT_ZERO;
    int ret = BRLCAD_ERROR;

    if (bu_cmd_schema_parse_complete(schema, NULL, &msg, argc - 1, argv + 1) >= 0)
	ret = BRLCAD_OK;
    else if (bu_vls_strlen(&msg))
	bu_vls_vlscat(gedp->ged_result_str, &msg);
    else
	bu_vls_printf(gedp->ged_result_str, "Invalid %s command arguments.", schema->name);
    bu_vls_free(&msg);
    return ret;
}


static int
process_tree_list(void *data, int argc, const char *argv[])
{
    struct ged *gedp = (struct ged *)data;

    if (process_tree_parse(gedp, &process_list_schema, argc, argv) != BRLCAD_OK)
	return BRLCAD_ERROR;
    return _ged_process_list(gedp);
}


static int
process_tree_pabort(void *data, int argc, const char *argv[])
{
    struct ged *gedp = (struct ged *)data;

    if (process_tree_parse(gedp, &process_pabort_schema, argc, argv) != BRLCAD_OK)
	return BRLCAD_ERROR;
    return _ged_process_pabort(gedp, argc - 1, argv + 1);
}


static int
process_tree_gabort(void *data, int argc, const char *argv[])
{
    struct ged *gedp = (struct ged *)data;

    if (process_tree_parse(gedp, &process_gabort_schema, argc, argv) != BRLCAD_OK)
	return BRLCAD_ERROR;
    return _ged_process_gabort(gedp, argc - 1, argv + 1);
}


static const struct bu_cmd_tree_node process_subcommands[] = {
    BU_CMD_TREE_NODE(&process_list_schema, NULL, NULL,
	BU_CMD_TREE_CHILD_AFTER_OPTIONS, process_tree_list),
    BU_CMD_TREE_NODE(&process_pabort_schema, NULL, NULL,
	BU_CMD_TREE_CHILD_AFTER_OPTIONS, process_tree_pabort),
    BU_CMD_TREE_NODE(&process_gabort_schema, NULL, NULL,
	BU_CMD_TREE_CHILD_AFTER_OPTIONS, process_tree_gabort),
    BU_CMD_TREE_NODE_NULL
};
static const struct bu_cmd_tree ged_process_tree = {
    &process_root_schema, process_subcommands, BU_CMD_TREE_CHILD_AFTER_OPTIONS
};


static char *
process_native_help(void)
{
    return bu_cmd_tree_describe(&ged_process_tree);
}


int
ged_process_core(struct ged *gedp, int argc, const char *argv[])
{
    int ret = BRLCAD_ERROR;

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc < 2) {
	_process_show_help(gedp);
	return BRLCAD_ERROR;
    }
    if (bu_cmd_tree_dispatch(&ged_process_tree, gedp, argc - 1, argv + 1, &ret) == 0)
	return ret;

    _process_show_help(gedp);
    return BRLCAD_ERROR;
}


#include "../include/plugin.h"

static int
ged_process_grammar_validate(struct ged *gedp, const char *input, size_t cursor_pos,
	struct ged_cmd_validate_result *result)
{
    return ged_cmd_tree_validate(gedp, &ged_process_tree, input, cursor_pos, result);
}


static int
ged_process_grammar_analyze(struct ged *gedp, const char *input,
	struct ged_cmd_analysis *analysis)
{
    return ged_cmd_tree_analyze(gedp, &ged_process_tree, input, analysis);
}


static char *
ged_process_grammar_json(void)
{
    return bu_cmd_tree_describe_json(&ged_process_tree);
}


static int
ged_process_grammar_lint(struct bu_vls *msgs)
{
    return bu_cmd_tree_lint(&ged_process_tree, msgs);
}


static const struct ged_cmd_grammar ged_process_grammar = {
    "process", "Manage GED subprocesses", ged_process_grammar_validate,
    ged_process_grammar_analyze, ged_process_grammar_json, ged_process_grammar_lint
};

#define GED_PROCESS_COMMANDS(X, XID) \
    X(process, ged_process_core, GED_CMD_DEFAULT, &ged_process_grammar) \

GED_DECLARE_COMMAND_SET_WITH_GRAMMAR(GED_PROCESS_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST_WITH_GRAMMAR("libged_process", 1, GED_PROCESS_COMMANDS)

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
