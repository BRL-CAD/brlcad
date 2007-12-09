/*                           T E M P . C
 * BRL-CAD
 *
 * Copyright (c) 2001-2007 United States Government as represented by
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
/** @addtogroup bu_log */
/** @{ */
/** @file temp.c
 *
 * Routine to open a temporary file.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <stdio.h>

#include "machine.h"
#include "bu.h"


#define _TF_FAIL "WARNING: Unable to create a temporary file\n"


struct _bu_tf_list {
    struct bu_list l;
    FILE *fp;
    int fd;
};

static int _bu_temp_files = 0;
static struct _bu_tf_list *_bu_tf = NULL;


static void
_bu_close_files()
{
    struct _bu_tf_list *popped;
    if (!_bu_tf) {
	return;
    }

    /* close all files, free their nodes */
    while (BU_LIST_WHILE(popped, _bu_tf_list, &(_bu_tf->l))) {
	BU_LIST_DEQUEUE(&(popped->l));
	if (popped) {
	    if (popped->fp) {
		fclose(popped->fp);
		popped->fp = NULL;
	    }
	    if (popped->fd != -1) {
		close(popped->fd);
		popped->fd = -1;
	    }
	    bu_free(popped, "free bu_temp_file node");
	}
    }

    /* free the head */
    if (_bu_tf->fp) {
	if (_bu_tf->fp) {
	    fclose(_bu_tf->fp);
	    _bu_tf->fp = NULL;
	}
	if (_bu_tf->fd != -1) {
	    close(_bu_tf->fd);
	    _bu_tf->fd = -1;
	}
    }
    bu_free(_bu_tf, "free bu_temp_file head");
}


static void
_bu_add_to_list(FILE *fp, int fd)
{
    struct _bu_tf_list *newtf;

    _bu_temp_files++;

    if (_bu_temp_files == 1) {
	/* schedule files for closure on exit */
	atexit(_bu_close_files);

	BU_GETSTRUCT(_bu_tf, _bu_tf_list);
	BU_LIST_INIT(&(_bu_tf->l));
	_bu_tf->fp = fp;
	_bu_tf->fd = fd;

	return;
    }

    BU_GETSTRUCT(newtf, _bu_tf_list);
    newtf->fp = fp;
    newtf->fd = fd;
    BU_LIST_PUSH(&(_bu_tf->l), &(newtf->l));

    return;
}


/**
 *  b u _ t e m p _ f i l e
 *
 * Create a temporary file.  The first readable/writable directory
 * will be used, searching TMPDIR/TMP/TEMP environment variable
 * directories followed by default system and ultimately trying the
 * current directory.
 *
 * This routine is guaranteed to return a new unique file or return
 * NULL on failure.  The temporary file will be automatically unlinked
 * on application exit.  It is the caller's responsibility to set a
 * desired umask or to zero the file contents before closing the file.
 *
 * There is no (portable) means to get the name of the temporary file.
 *
 * Typical Use:
@code
 *	FILE *fp;
 *
 *	fp = bu_temp_file();
 *	...
 *	fclose(fp); // optional, auto-closed on exit
 *
 *	...
 *
 *	fp = bu_temp_file();
 *      fchmod(fileno(fp), 777);
 *	...
 *	rewind(fp);
 *	while (fputc(0, fp) == 0);
 *	fclose(fp);
@endcode
 */
FILE *
bu_temp_file()
{
    FILE *fp = NULL;

#ifdef HAVE_MKSTEMP

    int i;
    int fd = -1;
    char *dir = NULL;
    char file[MAXPATHLEN];
    const char *tmpdirs[] = {"TMPDIR", "TEMP", "TMP", NULL};

    /* check environment */
    for (i=0; tmpdirs[i]; i++) {
	dir = getenv(tmpdirs[i]);
	if (dir && dir[0] != '\0' && bu_file_writable(dir) && bu_file_executable(dir)) {
	    break;
	}
    }

    /* try common system default */
    if (!dir) {
#ifdef _WIN32
	dir = "C:\\TEMP";
#else
	dir = "/tmp";
#endif
	if (!bu_file_writable(dir) || !bu_file_executable(dir)) {
	    dir = NULL;
	}
    }

    /* try current dir */
    if (!dir) {
	dir = ".";
	if (!bu_file_writable(dir) || !bu_file_executable(dir)) {
	    /* give up */
	    bu_log("Unable to find a suitable temp directory\n");
	    bu_log(_TF_FAIL);
	    return NULL;
	}
    }

    snprintf(file, MAXPATHLEN, "%s%cBRL-CAD_temp_XXXXXXX", dir, BU_DIR_SEPARATOR);

    fd = mkstemp(file);
    if (fd == -1) {
	perror("mkstemp");
	bu_log(_TF_FAIL);
	return NULL;
    }

    fp = fdopen(fd, "w+");
    if (fp == NULL) {
	perror("fdopen");
	bu_log(_TF_FAIL);
	close(fd);
	return NULL;
    }

    /* make it go away on close */
    unlink(file);

    /* add the file to the atexit auto-close list */
    _bu_add_to_list(fp, fd);

    return fp;

#endif /* HAVE_MKSTEMP */

#ifdef HAVE_TMPFILE_S
 
    if (_bu_temp_files+1 > TMP_MAX_S) {
	bu_log("Exceeding temporary file creation limit (%lu > %lu)\n", _bu_temp_files+1, TMP_MAX_S);
    }

    tmpfile_s(&fp);
    if (!fp) {
	perror("tmpfile_s");
	bu_log(_TF_FAIL);
	return NULL;
    }

    /* add the file to the atexit auto-close list */
    _bu_add_to_list(fp, -1);

    return fp;

#endif /* HAVE_TMPFILE_S */

    /* shouldn't get here */
    bu_log("WARNING: Unexpectedly unable to create a temporary file\n");
    return NULL;
}

/** @} */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
