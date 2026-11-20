/*                    M G E D _ S T A R T U P . C
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

#include <string.h>

#include "mged_startup.h"

void
mged_startup_plan_resolve(struct mged_startup_plan *plan, const struct mged_startup_request *request)
{
    memset(plan, 0, sizeof(*plan));

    if (request->positional_argc > 1)
	plan->input = MGED_INPUT_COMMAND;
    else if (request->pipe_mode)
	plan->input = MGED_INPUT_PIPE;
    else if (request->stdin_is_terminal)
	plan->input = MGED_INPUT_TERMINAL;
    else if (!request->stdin_is_open)
	plan->input = MGED_INPUT_UNAVAILABLE;
    else
	plan->input = MGED_INPUT_STREAM;

    if (plan->input == MGED_INPUT_COMMAND)
	plan->ui = MGED_UI_HEADLESS;
    else if (request->gui_mode)
	plan->ui = MGED_UI_GUI;
    else if (request->classic_mode)
	plan->ui = MGED_UI_CLASSIC;
    else if (request->stdin_is_terminal)
	plan->ui = request->graphics_available ? MGED_UI_GUI : MGED_UI_CLASSIC;
    else if (plan->input == MGED_INPUT_UNAVAILABLE && request->graphics_available)
	plan->ui = MGED_UI_GUI;
    else
	plan->ui = MGED_UI_HEADLESS;

    if (request->attach) {
	plan->dm = MGED_DM_EXPLICIT;
	plan->dm_type = request->attach;
    } else if (plan->ui == MGED_UI_HEADLESS ||
	(plan->ui == MGED_UI_CLASSIC && plan->input != MGED_INPUT_TERMINAL)) {
	plan->dm = MGED_DM_NONE;
    } else {
	plan->dm = MGED_DM_AUTO;
    }

    if (request->skip_rc) {
	plan->rc = MGED_RC_SKIP;
    } else if (request->rcfile) {
	plan->rc = MGED_RC_EXPLICIT;
	plan->rcfile = request->rcfile;
    } else if (plan->ui == MGED_UI_HEADLESS) {
	plan->rc = MGED_RC_SKIP;
    } else {
	plan->rc = MGED_RC_DEFAULT;
    }

    plan->run_in_foreground = !request->background;
    plan->show_prompt = plan->input == MGED_INPUT_TERMINAL;
    plan->line_editing = plan->input == MGED_INPUT_TERMINAL &&
	plan->ui == MGED_UI_CLASSIC && request->stdout_is_terminal;
    plan->emit_cmd_done = plan->input == MGED_INPUT_PIPE;
    plan->monitor_stdin = plan->input != MGED_INPUT_COMMAND &&
	plan->input != MGED_INPUT_UNAVAILABLE &&
	(plan->ui != MGED_UI_GUI || plan->input != MGED_INPUT_TERMINAL);
    plan->redirect_output = plan->ui == MGED_UI_GUI &&
	(plan->input == MGED_INPUT_TERMINAL ||
	 plan->input == MGED_INPUT_UNAVAILABLE);
}

void
mged_startup_plan_gui_failed(struct mged_startup_plan *plan, int stdout_is_terminal)
{
    plan->ui = MGED_UI_CLASSIC;
    if (plan->dm != MGED_DM_EXPLICIT) {
	plan->dm = plan->input == MGED_INPUT_TERMINAL ? MGED_DM_AUTO : MGED_DM_NONE;
	plan->dm_type = NULL;
    }
    plan->show_prompt = plan->input == MGED_INPUT_TERMINAL;
    plan->line_editing = plan->show_prompt && stdout_is_terminal;
    plan->monitor_stdin = plan->input != MGED_INPUT_COMMAND &&
	plan->input != MGED_INPUT_UNAVAILABLE;
    plan->redirect_output = 0;
}

const char *
mged_input_mode_name(enum mged_input_mode mode)
{
    switch (mode) {
	case MGED_INPUT_TERMINAL: return "terminal";
	case MGED_INPUT_UNAVAILABLE: return "unavailable";
	case MGED_INPUT_PIPE: return "pipe";
	case MGED_INPUT_STREAM: return "stream";
	case MGED_INPUT_COMMAND: return "command";
    }
    return "unknown";
}

const char *
mged_ui_mode_name(enum mged_ui_mode mode)
{
    switch (mode) {
	case MGED_UI_HEADLESS: return "headless";
	case MGED_UI_CLASSIC: return "classic";
	case MGED_UI_GUI: return "gui";
    }
    return "unknown";
}

const char *
mged_dm_mode_name(enum mged_dm_mode mode)
{
    switch (mode) {
	case MGED_DM_NONE: return "none";
	case MGED_DM_AUTO: return "auto";
	case MGED_DM_EXPLICIT: return "explicit";
    }
    return "unknown";
}

const char *
mged_rc_mode_name(enum mged_rc_mode mode)
{
    switch (mode) {
	case MGED_RC_SKIP: return "skip";
	case MGED_RC_DEFAULT: return "default";
	case MGED_RC_EXPLICIT: return "explicit";
    }
    return "unknown";
}
