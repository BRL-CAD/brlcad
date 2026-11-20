/*                    M G E D _ S T A R T U P . H
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

#ifndef MGED_STARTUP_H
#define MGED_STARTUP_H

#include "common.h"

enum mged_input_mode {
    MGED_INPUT_TERMINAL = 0,
    MGED_INPUT_UNAVAILABLE,
    MGED_INPUT_PIPE,
    MGED_INPUT_STREAM,
    MGED_INPUT_COMMAND
};

enum mged_ui_mode {
    MGED_UI_HEADLESS = 0,
    MGED_UI_CLASSIC,
    MGED_UI_GUI
};

enum mged_dm_mode {
    MGED_DM_NONE = 0,
    MGED_DM_AUTO,
    MGED_DM_EXPLICIT
};

enum mged_rc_mode {
    MGED_RC_SKIP = 0,
    MGED_RC_DEFAULT,
    MGED_RC_EXPLICIT
};

struct mged_startup_request {
    int positional_argc;
    int stdin_is_terminal;
    int stdin_is_open;
    int stdout_is_terminal;
    int graphics_available;
    int pipe_mode;
    int classic_mode;
    int gui_mode;
    int background;
    int skip_rc;
    const char *rcfile;
    const char *attach;
};

struct mged_startup_plan {
    enum mged_input_mode input;
    enum mged_ui_mode ui;
    enum mged_dm_mode dm;
    enum mged_rc_mode rc;
    const char *dm_type;
    const char *rcfile;
    int run_in_foreground;
    int show_prompt;
    int line_editing;
    int emit_cmd_done;
    int monitor_stdin;
    int redirect_output;
};

__BEGIN_DECLS

void mged_startup_plan_resolve(struct mged_startup_plan *, const struct mged_startup_request *);
void mged_startup_plan_gui_failed(struct mged_startup_plan *, int);
const char *mged_input_mode_name(enum mged_input_mode);
const char *mged_ui_mode_name(enum mged_ui_mode);
const char *mged_dm_mode_name(enum mged_dm_mode);
const char *mged_rc_mode_name(enum mged_rc_mode);

__END_DECLS

#endif /* MGED_STARTUP_H */
