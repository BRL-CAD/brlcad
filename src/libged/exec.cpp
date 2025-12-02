/*                        E X E C . C P P
 * BRL-CAD
 *
 * Copyright (c) 2020-2025 United States Government as represented by
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
#include "./include/plugin.h"

extern "C" void libged_init(void);
extern "C" void ged_force_static_registration();

extern "C" int
ged_exec(struct ged *gedp, int argc, const char *argv[])
{
    int cret = BRLCAD_OK;

    if (!gedp || !argc || !argv) {
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
    static std::once_flag _ged_registry_once;
    std::call_once(_ged_registry_once, []() {
        #ifdef LIBGED_STATIC_CORE
            ged_force_static_registration();
        #endif
        libged_init();
    });

    if (ged_registered_count() == 0) {
        // Fallback path if something went wrong with one-time init:
        libged_init();
    }

    double start = 0.0;
    const char *tstr = getenv("GED_EXEC_TIME");
    if (tstr) {
	start = bu_gettime();
    }

    /* Normalize command name to basename */
    struct bu_vls cmdvls = BU_VLS_INIT_ZERO;
    bu_path_component(&cmdvls, argv[0], BU_PATH_BASENAME);
    std::string cmdname = bu_vls_cstr(&cmdvls);
    bu_vls_free(&cmdvls);

    /* Lookup command */
    const struct ged_cmd *cmd = ged_get_command(cmdname.c_str());
    if (!cmd) {
        bu_vls_printf(gedp->ged_result_str, "unknown command: %s", cmdname.c_str());
        return (BRLCAD_ERROR | GED_UNKNOWN);
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
	return BRLCAD_ERROR;
    }

    // Check for a pre-exec callback.
    bu_clbk_t f = NULL;
    void *d = NULL;
    if ((ged_clbk_get(&f, &d, gedp, cmdname.c_str(), BU_CLBK_PRE) == BRLCAD_OK) && f) {
	cret = ged_clbk_exec(gedp->ged_result_str, gedp, GED_CMD_RECURSION_LIMIT, f, argc, argv, gedp, d);
	if (cret != BRLCAD_OK)
	    bu_log("error running %s pre-execution callback\n", cmdname.c_str());
    }

    // TODO - if interactive command via cmd->i->interactive, don't execute
    // unless we have the necessary callbacks defined in gedp

    // Preliminaries complete - do the actual command execution call
    cret = (*cmd->i->cmd)(gedp, argc, argv);

    // If we didn't execute successfully, don't execute the post run hook.  (If
    // a specific command wants to anyway, it can do so in its own
    // implementation.)
    if (cret != BRLCAD_OK) {
	if (tstr)
	    bu_log("%s time: %g\n", cmdname.c_str(), (bu_gettime() - start)/1e6);

	gedip->cmd_recursion_depth_cnt[cmdname]--;
	gedip->exec_stack.pop();
	return cret;
    }

    // Command execution complete - check for a post command callback.
    if ((ged_clbk_get(&f, &d, gedp, cmdname.c_str(), BU_CLBK_POST) == BRLCAD_OK) && f) {
	cret = ged_clbk_exec(gedp->ged_result_str, gedp, GED_CMD_RECURSION_LIMIT, f, argc, argv, gedp, d);
	if (cret != BRLCAD_OK)
	    bu_log("error running %s post-execution callback\n", cmdname.c_str());
    }

    if (tstr)
	bu_log("%s time: %g\n", cmdname.c_str(), (bu_gettime() - start)/1e6);

    gedip->cmd_recursion_depth_cnt[cmdname]--;
    gedip->exec_stack.pop();
    return cret;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
