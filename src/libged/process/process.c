/*                     P R O C E S S . C
 * BRL-CAD
 *
 * Copyright (c) 2019-2022 United States Government as represented by
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

#include "bu/path.h"
#include "bu/process.h"
#include "../ged_private.h"


HIDDEN void
_process_show_help(struct ged *gedp)
{
    struct bu_vls str = BU_VLS_INIT_ZERO;

    bu_vls_sprintf(&str, "Usage: process [subcommand]\n\n");
    bu_vls_printf(&str, "Subcommands:\n\n");
    bu_vls_printf(&str, "  list          - List all currently running subprocesses.\n");
    bu_vls_printf(&str, "  pabort <pid>  - Abort the specified subprocess.\n\n");
    bu_vls_printf(&str, "  gabort <glob> - Abort all subprocesses whose command matches the glob expression.\n\n");

    bu_vls_vlscat(gedp->ged_result_str, &str);
    bu_vls_free(&str);
}


HIDDEN int
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
	const char * const *argv;
	int argc = bu_process_args(NULL, &argv, rrp->p);
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

HIDDEN int
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
	    if (bu_opt_int(NULL, 1, (const char **)&argv[j], (void *)&pid) < 0) {
		bu_vls_printf(gedp->ged_result_str, "PID argument %s is not a valid process id.", argv[j]);
		return BRLCAD_ERROR;
	    }
	    if (ppid == pid) {
		bu_terminate(bu_process_pid(rrp->p));
		rrp->aborted = 1;
		/* terminated it, no need to check other args for
		 * this process */
		break;
	    }
	}
    }
    return BRLCAD_OK;
}

HIDDEN int
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
	int argcnt = bu_process_args(&cmd, NULL, rrp->p);
	bu_vls_trunc(&cmdroot, 0);
	if (argcnt > 0 && bu_path_component(&cmdroot, cmd, BU_PATH_BASENAME_EXTLESS)) {
	    for (int j = 0; j < argc; j++) {
		if (!bu_path_match(argv[j], bu_vls_cstr(&cmdroot), 0)) {
		    bu_terminate(bu_process_pid(rrp->p));
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


int
ged_process_core(struct ged *gedp, int argc, const char *argv[])
{
    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_READ_ONLY(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc < 2) {
	_process_show_help(gedp);
	return BRLCAD_ERROR;
    }

    if (argc == 2 && !BU_STR_EQUAL(argv[1], "list")) {
	_process_show_help(gedp);
	return BRLCAD_ERROR;
    }


    if (argc == 3 && (!BU_STR_EQUAL(argv[1], "gabort") && !BU_STR_EQUAL(argv[1], "pabort"))) {
	_process_show_help(gedp);
	return BRLCAD_ERROR;
    }

    if (argc == 2) {
	return _ged_process_list(gedp);
    }

    if (BU_STR_EQUAL(argv[1], "gabort")) {
	return _ged_process_gabort(gedp, argc - 2, (const char **)&argv[2]);
    }

    if (BU_STR_EQUAL(argv[1], "pabort")) {
	return _ged_process_pabort(gedp, argc - 2, (const char **)&argv[2]);
    }

    return BRLCAD_ERROR;
}


#ifdef GED_PLUGIN
#include "../include/plugin.h"
struct ged_cmd_impl process_cmd_impl = {
    "process",
    ged_process_core,
    GED_CMD_DEFAULT
};

const struct ged_cmd process_cmd = { &process_cmd_impl };
const struct ged_cmd *process_cmds[] = { &process_cmd, NULL };

static const struct ged_plugin pinfo = { GED_API,  process_cmds, 1 };

COMPILER_DLLEXPORT const struct ged_plugin *ged_plugin_info()
{
    return &pinfo;
}
#endif /* GED_PLUGIN */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
