/*                      P R O G N A M E . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2021 United States Government as represented by
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

#ifdef HAVE_SYS_TYPES_H
#  include <sys/types.h>
#endif
#ifdef HAVE_SYS_PARAM_H /* for MAXPATHLEN */
#  include <sys/param.h>
#endif
#ifdef HAVE_SYS_WAIT_H /* for wait */
#  include <sys/wait.h>
#endif
#if defined(HAVE_LIBPROC_H) && defined(HAVE_PROC_PIDPATH)
typedef void *rusage_info_t; /* BSD hack for POSIX-mode */
#  define SOCK_MAXADDRLEN 255 /* BSD hack for POSIX-mode */
#  include <libproc.h> /* for proc_pidpath */
#endif

#include <stdlib.h>
#include <string.h>
#include <errno.h> /* for program_invocation_name */
#include "bio.h"

#include "bu/app.h"
#include "bu/file.h"
#include "bu/malloc.h"
#include "bu/parallel.h"
#include "bu/path.h"
#include "bu/str.h"


#if !defined(HAVE_DECL_PROGRAM_INVOCATION_NAME)
extern char *program_invocation_name;
#endif

#if !defined(HAVE_DECL_GETPROGNAME) && !defined(getprogname)
extern const char *getprogname(void);
#endif

#if !defined(HAVE_DECL_WAIT) && !defined(wait)
extern pid_t wait(int *);
#endif


/* internal storage for bu_getprogname/bu_setprogname */
static char bu_progname[MAXPATHLEN] = {0};
const char *DEFAULT_PROGNAME = "(" PACKAGE_NAME ")";

const char *
_bu_progname_raw()
{
    return (const char *)bu_progname;
}

const char *
bu_argv0_full_path(void)
{
    static char buffer[MAXPATHLEN] = {0};
    char tbuf[MAXPATHLEN] = {0};
    const char *argv0 = bu_progname;
    const char *which;

#ifdef HAVE_PROGRAM_INVOCATION_NAME
    /* GLIBC provides a way */
    if (argv0[0] == '\0' && program_invocation_name) {
	argv0 = program_invocation_name;
    }
#endif

#ifdef HAVE_WINDOWS_H
    if (argv0[0] == '\0') {
	TCHAR exeFileName[MAXPATHLEN] = {0};
	GetModuleFileName(NULL, exeFileName, MAXPATHLEN);
	if (sizeof(TCHAR) == sizeof(char))
	    bu_strlcpy(tbuf, exeFileName, MAXPATHLEN);
	else
	    wcstombs(tbuf, exeFileName, wcslen(tbuf)+1);
	argv0 = tbuf;
    }
#endif

#if defined(HAVE_LIBPROC_H) && defined(HAVE_PROC_PIDPATH)
    if (argv0[0] == '\0') {
	int pid = getpid();
	(void)proc_pidpath(pid, tbuf, sizeof(tbuf));
	argv0 = tbuf;
    }
#endif

    /* still don't have a name? */
    if (argv0[0] == '\0') {
	bu_strlcat(tbuf, "(" PACKAGE_NAME ")", sizeof(tbuf));
	argv0 = tbuf;
    }

    if (argv0[0] == BU_DIR_SEPARATOR) {
	/* seems to already be a full path */
	snprintf(buffer, sizeof(buffer), "%s", argv0);
	return buffer;
    }

    /* running from PATH */
    which = bu_which(argv0);
    if (which && which[0] != '\0') {
	argv0 = which;
    }

    while (argv0[0] == '.' && argv0[1] == BU_DIR_SEPARATOR) {
	/* remove ./ if present, relative paths are appended to pwd */
	argv0 += 2;
    }

    /* running from relative dir */

    /* FIXME: this is technically wrong.  if the current working
     * directory is changed, we'll get the wrong path for argv0.
     */
    bu_getcwd(buffer, sizeof(buffer));
    snprintf(buffer+strlen(buffer), sizeof(buffer)-strlen(buffer), "%c%s", BU_DIR_SEPARATOR, argv0);
    if (bu_file_exists(buffer, NULL)) {
	return buffer;
    }

    /* give up */
    snprintf(buffer, sizeof(buffer), "%s", argv0);
    return buffer;
}


const char *
bu_getprogname(void)
{
    /* this static buffer is needed so we don't have to write into
     * bu_progname[], which potentially holds a full path.  we have to
     * free the bu_path_basename() memory, so we need to copy the
     * string somewhere before returning.
     */
    static char buffer[MAXPATHLEN] = {0};
    const char *name = bu_progname;
    char tmp_basename[MAXPATHLEN] = {0};

#ifdef HAVE_PROGRAM_INVOCATION_NAME
    /* GLIBC provides a way */
    if (name[0] == '\0' && program_invocation_name)
	name = program_invocation_name;
#endif

#ifdef HAVE_GETPROGNAME
    /* BSD's libc provides a way */
    if (name[0] == '\0') {
	name = getprogname(); /* not malloc'd memory, may return NULL */
	if (!name)
	    name = bu_progname;
    }
#endif

    /* want just the basename from paths, otherwise default result */
    bu_path_basename(name, tmp_basename);
    if (BU_STR_EQUAL(tmp_basename, ".") || BU_STR_EQUAL(tmp_basename, "/")) {
	name = DEFAULT_PROGNAME;
    } else {
	name = tmp_basename;
    }

    /* stash for return */
    bu_semaphore_acquire(BU_SEM_SYSCALL);
    bu_strlcpy(buffer, name, MAXPATHLEN);
    bu_semaphore_release(BU_SEM_SYSCALL);

    return buffer;
}


void
bu_setprogname(const char *argv0)
{
    bu_semaphore_acquire(BU_SEM_SYSCALL);
    if (argv0) {
	bu_strlcpy(bu_progname, argv0, MAXPATHLEN);
    } else {
	bu_progname[0] = '\0'; /* zap */
    }
    bu_semaphore_release(BU_SEM_SYSCALL);

    return;
}


/* DEPRECATED: Do not use. */
const char *
bu_argv0(void)
{
    return bu_getprogname();
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
