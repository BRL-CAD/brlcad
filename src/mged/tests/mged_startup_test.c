/*                 M G E D _ S T A R T U P _ T E S T . C
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by the
 * U.S. Army Research Laboratory.
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

#include <stdio.h>
#include <stdlib.h>

#include "bu/app.h"
#include "mged_startup.h"

struct startup_case {
    const char *name;
    struct mged_startup_request request;
    enum mged_input_mode input;
    enum mged_ui_mode ui;
    enum mged_dm_mode dm;
    enum mged_rc_mode rc;
    int prompt;
    int line_editing;
    int cmd_done;
    int monitor_stdin;
    int redirect_output;
};

static int
check_case(const struct startup_case *test)
{
    struct mged_startup_plan plan;
    mged_startup_plan_resolve(&plan, &test->request);

    if (plan.input == test->input && plan.ui == test->ui &&
	plan.dm == test->dm && plan.rc == test->rc &&
	plan.show_prompt == test->prompt &&
	plan.line_editing == test->line_editing &&
	plan.emit_cmd_done == test->cmd_done &&
	plan.monitor_stdin == test->monitor_stdin &&
	plan.redirect_output == test->redirect_output)
	return 0;

    fprintf(stderr,
	"%s: got input=%s ui=%s dm=%s rc=%s prompt=%d line=%d done=%d stdin=%d redirect=%d\n",
	test->name, mged_input_mode_name(plan.input), mged_ui_mode_name(plan.ui),
	mged_dm_mode_name(plan.dm), mged_rc_mode_name(plan.rc), plan.show_prompt,
	plan.line_editing, plan.emit_cmd_done, plan.monitor_stdin,
	plan.redirect_output);
    return 1;
}

static int
check_gui_fallback(void)
{
    struct mged_startup_plan plan;
    struct mged_startup_request request = {1, 0, 1, 0, 1, 1, 0, 1, 0, 0, NULL, NULL};

    mged_startup_plan_resolve(&plan, &request);
    mged_startup_plan_gui_failed(&plan, 0);
    if (plan.ui != MGED_UI_CLASSIC || plan.dm != MGED_DM_NONE ||
	plan.show_prompt || plan.line_editing || !plan.monitor_stdin ||
	plan.redirect_output) {
	fprintf(stderr, "pipe GUI fallback enabled interactive classic behavior\n");
	return 1;
    }

    request.stdin_is_terminal = 1;
    request.stdout_is_terminal = 1;
    request.pipe_mode = 0;
    request.gui_mode = 0;
    mged_startup_plan_resolve(&plan, &request);
    mged_startup_plan_gui_failed(&plan, 1);
    if (plan.ui != MGED_UI_CLASSIC || plan.dm != MGED_DM_AUTO ||
	!plan.show_prompt || !plan.line_editing || !plan.monitor_stdin ||
	plan.redirect_output) {
	fprintf(stderr, "terminal GUI fallback did not enable classic behavior\n");
	return 1;
    }

    return 0;
}

int
main(int UNUSED(argc), char **argv)
{
    const struct startup_case tests[] = {
	{"terminal gui", {1, 1, 1, 1, 1, 0, 0, 0, 0, 0, NULL, NULL}, MGED_INPUT_TERMINAL, MGED_UI_GUI, MGED_DM_AUTO, MGED_RC_DEFAULT, 1, 0, 0, 0, 1},
	{"terminal classic", {1, 1, 1, 1, 1, 0, 1, 0, 0, 0, NULL, NULL}, MGED_INPUT_TERMINAL, MGED_UI_CLASSIC, MGED_DM_AUTO, MGED_RC_DEFAULT, 1, 1, 0, 1, 0},
	{"terminal no graphics", {1, 1, 1, 1, 0, 0, 0, 0, 0, 0, NULL, NULL}, MGED_INPUT_TERMINAL, MGED_UI_CLASSIC, MGED_DM_AUTO, MGED_RC_DEFAULT, 1, 1, 0, 1, 0},
	{"desktop gui", {1, 0, 0, 0, 1, 0, 0, 0, 0, 0, NULL, NULL}, MGED_INPUT_UNAVAILABLE, MGED_UI_GUI, MGED_DM_AUTO, MGED_RC_DEFAULT, 0, 0, 0, 0, 1},
	{"pipe headless", {1, 0, 1, 0, 1, 1, 0, 0, 0, 0, NULL, NULL}, MGED_INPUT_PIPE, MGED_UI_HEADLESS, MGED_DM_NONE, MGED_RC_SKIP, 0, 0, 1, 1, 0},
	{"pipe gui", {1, 0, 1, 0, 1, 1, 0, 1, 0, 0, NULL, NULL}, MGED_INPUT_PIPE, MGED_UI_GUI, MGED_DM_AUTO, MGED_RC_DEFAULT, 0, 0, 1, 1, 0},
	{"pipe classic", {1, 0, 1, 0, 1, 1, 1, 0, 0, 0, NULL, NULL}, MGED_INPUT_PIPE, MGED_UI_CLASSIC, MGED_DM_NONE, MGED_RC_DEFAULT, 0, 0, 1, 1, 0},
	{"stream attach", {1, 0, 1, 0, 1, 0, 0, 0, 0, 0, NULL, "wgl"}, MGED_INPUT_STREAM, MGED_UI_HEADLESS, MGED_DM_EXPLICIT, MGED_RC_SKIP, 0, 0, 0, 1, 0},
	{"stream rcfile", {1, 0, 1, 0, 1, 0, 0, 0, 0, 0, "startup.tcl", NULL}, MGED_INPUT_STREAM, MGED_UI_HEADLESS, MGED_DM_NONE, MGED_RC_EXPLICIT, 0, 0, 0, 1, 0},
	{"stream no rc", {1, 0, 1, 0, 1, 0, 0, 1, 0, 1, "startup.tcl", NULL}, MGED_INPUT_STREAM, MGED_UI_GUI, MGED_DM_AUTO, MGED_RC_SKIP, 0, 0, 0, 1, 0},
	{"command", {2, 1, 1, 1, 1, 1, 0, 1, 0, 0, NULL, NULL}, MGED_INPUT_COMMAND, MGED_UI_HEADLESS, MGED_DM_NONE, MGED_RC_SKIP, 0, 0, 0, 0, 0},
	{"terminal pipe", {1, 1, 1, 1, 1, 1, 0, 0, 0, 0, NULL, NULL}, MGED_INPUT_PIPE, MGED_UI_GUI, MGED_DM_AUTO, MGED_RC_DEFAULT, 0, 0, 1, 1, 0}
    };
    size_t i;
    int failures = 0;

    bu_setprogname(argv[0]);

    for (i = 0; i < sizeof(tests) / sizeof(tests[0]); i++)
	failures += check_case(&tests[i]);
    failures += check_gui_fallback();

    return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
