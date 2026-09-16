/*                        L A U N C H . C
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file launch.c
 *
 * Spawn a selected application as an independent sub-process using libbu's
 * cross-platform process API.  The BRL-CAD bin directory is prepended to the
 * child's PATH so that a launched shell (and any tool it starts) finds the
 * BRL-CAD executables.
 *
 */

#include "common.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "bu.h"
#include "bu/app.h"
#include "bu/env.h"
#include "bu/process.h"
#include "bu/vls.h"

#include "launcher.h"


/* Prepend the BRL-CAD bin directory to PATH (idempotent enough for our use --
 * the child inherits the environment we set here). */
static void
prepend_bindir_to_path(void)
{
    char bindir[MAXPATHLEN] = {0};
    const char *oldpath;
    struct bu_vls newpath = BU_VLS_INIT_ZERO;
    /* PATH list separator: ';' on Windows, ':' elsewhere.  This is the one
     * unavoidable platform difference in the launcher proper. */
#if defined(_WIN32) && !defined(__CYGWIN__)
    const char sep = ';';
#else
    const char sep = ':';
#endif

    bu_dir(bindir, sizeof(bindir), BU_DIR_BIN, NULL);
    if (bindir[0] == '\0')
	return;

    oldpath = getenv("PATH");
    if (oldpath && oldpath[0] != '\0')
	bu_vls_sprintf(&newpath, "%s%c%s", bindir, sep, oldpath);
    else
	bu_vls_sprintf(&newpath, "%s", bindir);

    (void)bu_setenv("PATH", bu_vls_cstr(&newpath), 1);
    bu_vls_free(&newpath);
}


static int
spawn(const char **argv)
{
    struct bu_process *p = NULL;

    if (!argv || !argv[0])
	return -1;

    bu_process_create(&p, argv, BU_PROCESS_DEFAULT);
    if (!p)
	return -1;

    /* Deliberately do not wait: launched programs run independently so the
     * user can start several from the launcher and keep it open. */
    return 0;
}


/*
 * Open an interactive command shell with the BRL-CAD tools available.  This is
 * the only entry that needs to know about the host platform's terminal, so the
 * platform-specific selection is confined here.
 */
static int
launch_shell(void)
{
    const char *argv[8];

#if defined(_WIN32) && !defined(__CYGWIN__)
    /* Windows: launch the command interpreter.  When the launcher is a GUI
     * (windowed) process the OS allocates a fresh console for it. */
    const char *comspec = getenv("COMSPEC");
    argv[0] = (comspec && comspec[0]) ? comspec : "cmd.exe";
    argv[1] = "/K";
    argv[2] = "title BRL-CAD Command Shell";
    argv[3] = NULL;
    return spawn(argv);
#else
    /* POSIX: try a graphical terminal emulator running the user's shell so the
     * launcher works when started by a double-click (no controlling tty).
     * Fall back to running the shell directly if none is found. */
    const char *shell = getenv("SHELL");
    const char *terms[] = { "x-terminal-emulator", "xterm", "gnome-terminal", "konsole", NULL };
    int i;

    if (!shell || !shell[0])
	shell = "/bin/sh";

    for (i = 0; terms[i]; i++) {
	const char *term = bu_which(terms[i]);
	if (!term)
	    continue;
	argv[0] = term;
	argv[1] = "-e";
	argv[2] = shell;
	argv[3] = NULL;
	return spawn(argv);
    }

    argv[0] = shell;
    argv[1] = NULL;
    return spawn(argv);
#endif
}


int
app_launch(const struct app_entry *e)
{
    const char *argv[2];

    if (!e || !e->available)
	return -1;

    prepend_bindir_to_path();

    if (e->exec[0] == '@') {
	if (BU_STR_EQUAL(e->exec, "@shell"))
	    return launch_shell();
	return -1;
    }

    argv[0] = e->exec_path;
    argv[1] = NULL;
    return spawn(argv);
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
