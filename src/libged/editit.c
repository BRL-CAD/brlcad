/*                        E D I T I T . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2011 United States Government as represented by
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
/** @file libged/editit.c
 *
 * The editit function.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <string.h>

#ifdef HAVE_SYS_TYPES_H
#  include <sys/types.h>
#endif

#ifdef HAVE_SYS_WAIT_H
#  include <sys/wait.h>
#endif

#include "bio.h"
#include "ged.h"

int
_ged_editit(char *editstring, const char *filename)
{
#ifdef HAVE_UNISTD_H
    int xpid = 0;
    int status = 0;
#endif
    int pid = 0;
    char **avtmp;
    const char *terminal = (char *)NULL;
    const char *terminal_opt = (char *)NULL;
    const char *editor = (char *)NULL;
    const char *editor_opt = (char *)NULL;
    const char *file = (const char *)filename;

#if defined(SIGINT) && defined(SIGQUIT)
    void (*s2)();
    void (*s3)();
#endif

    if (!file) {
	bu_log("INTERNAL ERROR: editit filename missing\n");
	return 0;
    }

    /* convert the edit string into pieces suitable for arguments to execlp */

    avtmp = (char **)bu_malloc(sizeof(char *)*5, "ged_editit: editstring args");
    bu_argv_from_string(avtmp, 4, editstring);

    if (avtmp[0] && !BU_STR_EQUAL(avtmp[0], "(null)"))
	terminal = avtmp[0];
    if (avtmp[1] && !BU_STR_EQUAL(avtmp[1], "(null)"))
	terminal_opt = avtmp[1];
    if (avtmp[2] && !BU_STR_EQUAL(avtmp[2], "(null)"))
	editor = avtmp[2];
    if (avtmp[3] && !BU_STR_EQUAL(avtmp[3], "(null)"))
	editor_opt = avtmp[3];

    if (!editor) {
	bu_log("INTERNAL ERROR: editit editor missing\n");
	return 0;
    }

    /* print a message to let the user know they need to quit their
     * editor before the application will come back to life.
     */
    {
	int length;
	struct bu_vls str;
	struct bu_vls sep;
	char *editor_basename;

	bu_vls_init(&str);
	bu_vls_init(&sep);
	if (terminal && editor_opt) {
	    bu_log("Invoking [%s %s %s] via %s\n\n", editor, editor_opt, file, terminal);
	} else if (terminal) {
	    bu_log("Invoking [%s %s] via %s\n\n", editor, file, terminal);
	} else if (editor_opt) {
	    bu_log("Invoking [%s %s %s]\n\n", editor, editor_opt, file);
	} else {
	    bu_log("Invoking [%s %s]\n\n", editor, file);
	}
	editor_basename = bu_basename(editor);
	bu_vls_sprintf(&str, "\nNOTE: You must QUIT %s before %s will respond and continue.\n", editor_basename, bu_getprogname());
	for (length = bu_vls_strlen(&str) - 2; length > 0; length--) {
	    bu_vls_putc(&sep, '*');
	}
	bu_log("%V%V%V\n\n", &sep, &str, &sep);
	bu_vls_free(&str);
	bu_vls_free(&sep);
	bu_free(editor_basename, "editor_basename free");
    }

#if defined(SIGINT) && defined(SIGQUIT)
    s2 = signal(SIGINT, SIG_IGN);
    s3 = signal(SIGQUIT, SIG_IGN);
#endif

#ifdef HAVE_UNISTD_H
    if ((pid = fork()) < 0) {
	perror("fork");
	return 0;
    }
#endif

    if (pid == 0) {
	/* Don't call bu_log() here in the child! */

#if defined(SIGINT) && defined(SIGQUIT)
	/* deja vu */
	(void)signal(SIGINT, SIG_DFL);
	(void)signal(SIGQUIT, SIG_DFL);
#endif

	{

	    char *editor_basename;

#if defined(_WIN32) && !defined(__CYGWIN__)
	    char buffer[RT_MAXLINE + 1] = {0};
	    STARTUPINFO si = {0};
	    PROCESS_INFORMATION pi = {0};
	    si.cb = sizeof(STARTUPINFO);
	    si.lpReserved = NULL;
	    si.lpReserved2 = NULL;
	    si.cbReserved2 = 0;
	    si.lpDesktop = NULL;
	    si.dwFlags = 0;

	    snprintf(buffer, RT_MAXLINE, "%s %s", editor, file);

	    CreateProcess(NULL, buffer, NULL, NULL, TRUE, NORMAL_PRIORITY_CLASS, NULL, NULL, &si, &pi);
	    WaitForSingleObject(pi.hProcess, INFINITE);
	    return 1;
#else

	    editor_basename = bu_basename(editor);
	    if (BU_STR_EQUAL(editor_basename, "TextEdit")) {
		/* close stdout/stderr so we don't get blather from TextEdit about service registration failure */
		close(fileno(stdout));
		close(fileno(stderr));
	    }
	    bu_free(editor_basename, "editor_basename free");

	    if (!terminal && !editor_opt) {
		(void)execlp(editor, editor, file, NULL);
	    } else if (!terminal) {
		(void)execlp(editor, editor, editor_opt, file, NULL);
	    } else if (terminal && !terminal_opt) {
		(void)execlp(terminal, terminal, editor, file, NULL);
	    } else if (terminal && !editor_opt) {
		(void)execlp(terminal, terminal, terminal_opt, editor, file, NULL);
	    } else {
		(void)execlp(terminal, terminal, terminal_opt, editor, editor_opt, file, NULL);
	    }
#endif
	    /* should not reach */
	    perror(editor);
	    bu_exit(1, NULL);
	}
    }

#ifdef HAVE_UNISTD_H
    /* wait for the editor to terminate */
    while ((xpid = wait(&status)) >= 0) {
	if (xpid == pid) {
	    break;
	}
    }
#endif

#if defined(SIGINT) && defined(SIGQUIT)
    (void)signal(SIGINT, s2);
    (void)signal(SIGQUIT, s3);
#endif

    bu_free((genptr_t)avtmp, "ged_editit: avtmp");
    return 1;
}


int
ged_editit(struct ged *gedp, int argc, const char *argv[])
{
    int ret = 0;
    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* FIXME: this should NOT assume that argv[2] and argv[4] are the
     * edit string and temp file.  should use bu_getopt().
     */
    if (argc != 5) {
	bu_vls_printf(gedp->ged_result_str, "Internal Error: \"%s -e editstring -f tmpfile\" is malformed (argc == %d)", argv[0], argc);
	return TCL_ERROR;
    } else {
	char *edstr = bu_strdup((char *)argv[2]);
	ret = _ged_editit(edstr, argv[4]);
	bu_free(edstr, "free tmp editstring copy");
	return ret;
    }
}


/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
