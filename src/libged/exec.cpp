/*                        E X E C . C P P
 * BRL-CAD
 *
 * Copyright (c) 2020-2026 United States Government as represented by
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

#include "common.h"

#include <string>
#include <mutex>

#include "bu/time.h"
#include "bu/path.h"
#include "bu/vls.h"
#include "ged.h"
#include "./ged_private.h"
#include "./include/plugin.h"

extern "C" void libged_init(void);

extern "C" int
ged_exec(struct ged *gedp, int argc, const char *argv[])
{
    int cret = BRLCAD_OK;

    if (!gedp || !gedp->ged_results || !argc || !argv) {
	return BRLCAD_ERROR;
    }

    /* make sure argc and argv agree, no NULL args */
    for (int i = 0; i < argc; i++) {
	if (!argv[i]) {
	    if (i == (argc -1)) {
		// NULL termination, decrement argc and move on
		argc--;
	    } else {
		bu_log("INTERNAL ERROR: ged_exec() argv[%d] is NULL (argc=%d)\n", i, argc);
		return BRLCAD_ERROR;
	    }
	}
    }

    /* Ensure registry is initialized exactly once (thread-safe). */
    ged_ensure_initialized();

    double start = 0.0;
    const char *tstr = getenv("GED_EXEC_TIME");
    if (tstr) {
	start = bu_gettime();
    }

    /* Until we are successful, return an error */
    gedp->ged_results->ret = BRLCAD_ERROR;

    /* Normalize command name to basename */
    struct bu_vls cmdvls = BU_VLS_INIT_ZERO;
    bu_path_component(&cmdvls, argv[0], BU_PATH_BASENAME);
    std::string cmdname = bu_vls_cstr(&cmdvls);
    bu_vls_free(&cmdvls);

    /* Lookup command via generalized registry */
    bu_plugin_cmd_impl fn = bu_plugin_cmd_get(cmdname.c_str());
    if (!fn) {
        bu_vls_printf(gedp->ged_result_str, "unknown command: %s", cmdname.c_str());
	gedp->ged_results->ret = (BRLCAD_ERROR | GED_UNKNOWN);
	return gedp->ged_results->ret;
    }

    GED_CK_MAGIC(gedp);
    Ged_Internal *gedip = gedp->i->i;
    gedip->exec_stack.push(cmdname);
    gedip->cmd_recursion_depth_cnt[cmdname]++;

    if (gedip->cmd_recursion_depth_cnt[cmdname] > GED_CMD_RECURSION_LIMIT) {
	bu_vls_printf(gedp->ged_result_str, "Recursion limit %d exceeded for command %s - aborted.  ged_exec call stack:\n", GED_CMD_RECURSION_LIMIT, cmdname.c_str());
	std::stack<std::string> lexec_stack = gedip->exec_stack;
	while (!lexec_stack.empty()) {
	    bu_vls_printf(gedp->ged_result_str, "%s\n", lexec_stack.top().c_str());
	    lexec_stack.pop();
	}
	gedp->ged_results->ret = BRLCAD_ERROR;
	return gedp->ged_results->ret;
    }

    // Check for a pre-exec callback.
    bu_clbk_t f = NULL;
    void *d = NULL;
    if (!gedp->ged_skip_clbks && (ged_clbk_get(&f, &d, gedp, cmdname.c_str(), BU_CLBK_PRE) == BRLCAD_OK) && f) {
	cret = ged_clbk_exec(gedp->ged_result_str, gedp, GED_CMD_RECURSION_LIMIT, f, argc, argv, gedp, d);
	if (cret != BRLCAD_OK) {
	    bu_log("error running %s pre-execution callback\n", cmdname.c_str());
	    gedp->ged_results->ret = cret;
	}
    }

    // TODO - if interactive command via cmd->i->interactive, don't execute
    // unless we have the necessary callbacks defined in gedp

    // Preliminaries complete - if the setup succeeded or wasn't needed, do the
    // actual command execution call
    if (cret == BRLCAD_OK)
	gedp->ged_results->ret = fn(gedp, argc, argv);

    // Command execution complete - check for a post command callback.  Note:
    // even in an error situation, there may be cases where a caller needs to
    // execute the post run hook.  (Cleanup, etc.)  Because this is application
    // specific, the responsibility for calling or not calling based on error
    // codes rests with the application's registered callback function - it can
    // check the return code with ged_results_ret(gedp->results) to learn what
    // it is.
    if (!gedp->ged_skip_clbks && (ged_clbk_get(&f, &d, gedp, cmdname.c_str(), BU_CLBK_POST) == BRLCAD_OK) && f) {
	cret = ged_clbk_exec(gedp->ged_result_str, gedp, GED_CMD_RECURSION_LIMIT, f, argc, argv, gedp, d);
	if (cret != BRLCAD_OK) {
	    bu_log("error running %s post-execution callback\n", cmdname.c_str());
	    gedp->ged_results->ret = cret;
	}
    }

    if (tstr)
	bu_log("%s time: %g\n", cmdname.c_str(), (bu_gettime() - start)/1e6);

    gedip->cmd_recursion_depth_cnt[cmdname]--;
    gedip->exec_stack.pop();
    return gedp->ged_results->ret;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
