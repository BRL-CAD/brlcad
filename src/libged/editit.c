/*                        E D I T I T . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2010 United States Government as represented by
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
/** @file editit.c
 *
 * The editit function.
 *
 */

#include <stdlib.h>

#ifdef HAVE_SYS_TYPES_H
#   include <sys/types.h>
#endif

#ifdef HAVE_SYS_WAIT_H
#   include <sys/wait.h>
#endif

#include "bio.h"
#include "ged.h"


/* editors to test, in order of discovery preference (EDITOR overrides) */
#define WIN_EDITOR "c:/Program Files/Windows NT/Accessories/wordpad"
#define MAC_EDITOR "/Applications/TextEdit.app/Contents/MacOS/TextEdit"
#define	EMACS_EDITOR "/usr/bin/emacs"
#define	VIM_EDITOR "/usr/bin/vim"
#define	VI_EDITOR "/usr/bin/vi"
#define	ED_EDITOR "/bin/ed"


int
ged_editit(const char *file)
{
    int pid = 0;
    int xpid = 0;
    char buffer[RT_MAXLINE] = {0};
    const char *editor = (char *)NULL;
    int stat = 0;
#if defined(SIGINT) && defined(SIGQUIT)
    void (*s2)();
    void (*s3)();
#endif

    if (!editor || editor[0] == '\0')
	editor = getenv("EDITOR");

    /* still unset? try windows */
    if (!editor || editor[0] == '\0') {
#if defined(_WIN32) && !defined(__CYGWIN__)
	editor = WIN_EDITOR;
#else
	editor = (char *)NULL;
#endif
    }

    /* still unset? try mac os x */
    if (!editor || editor[0] == '\0') {
	if (bu_file_exists(MAC_EDITOR)) {
	    editor = MAC_EDITOR;
	}
    }

    /* still unset? try emacs */
    if (!editor || editor[0] == '\0') {
	if (bu_file_exists(EMACS_EDITOR)) {
	    editor = EMACS_EDITOR;
	}
    }

    /* still unset? try vim */
    if (!editor || editor[0] == '\0') {
	if (bu_file_exists(VIM_EDITOR)) {
	    editor = VIM_EDITOR;
	}
    }

    /* still unset? try vi */
    if (!editor || editor[0] == '\0') {
	if (bu_file_exists(VI_EDITOR)) {
	    editor = VI_EDITOR;
	}
    }

    /* still unset? try ed */
    if (!editor || editor[0] == '\0') {
	if (bu_file_exists(ED_EDITOR)) {
	    editor = ED_EDITOR;
	}
    }

    /* still unset? default to jove */
    if (!editor || editor[0] == '\0') {
	const char *binpath = bu_brlcad_root("bin", 1);
	editor = "jove";
	if (!binpath) {
	    snprintf(buffer, RT_MAXLINE, "%s/%s", binpath, editor);
	    if (bu_file_exists(buffer)) {
		editor = buffer;
	    } else {
		const char *dirn = bu_dirname(bu_argv0());
		if (dirn) {
		    snprintf(buffer, RT_MAXLINE, "%s/%s", dirn, editor);
		    if (bu_file_exists(buffer)) {
			editor = buffer;
		    }
		}
	    }
	}
    }

#if defined(SIGINT) && defined(SIGQUIT)
    s2 = signal( SIGINT, SIG_IGN );
    s3 = signal( SIGQUIT, SIG_IGN );
#endif

#ifdef HAVE_UNISTD_H
    if ((pid = fork()) < 0) {
	perror("fork");
	return (0);
    }
#endif

    if (pid == 0) {
	/* Don't call bu_log() here in the child! */

#if defined(SIGINT) && defined(SIGQUIT)
	/* deja vu */
	(void)signal( SIGINT, SIG_DFL );
	(void)signal( SIGQUIT, SIG_DFL );
#endif

	{
#if defined(_WIN32) && !defined(__CYGWIN__)
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
	    WaitForSingleObject( pi.hProcess, INFINITE );
	    return 1;
#else
	    (void)execlp(editor, editor, file, NULL);
#endif
	    /* should not reach */
	    perror(editor);
	    bu_exit(1, NULL);
	}
    }

#ifdef HAVE_UNISTD_H
    /* wait for the editor to terminate */
    while ((xpid = wait(&stat)) >= 0) {
	if (xpid == pid) {
	    break;
	}
    }
#endif

#if defined(SIGINT) && defined(SIGQUIT)
    (void)signal(SIGINT, s2);
    (void)signal(SIGQUIT, s3);
#endif

    return (!stat);
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
