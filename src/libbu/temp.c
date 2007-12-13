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
#include <fcntl.h>
#include <string.h>

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


#ifndef HAVE_MKSTEMP
static int
mkstemp(char *file_template)
{
    int fd = -1;
    int counter = 0;
    char *filepath = NULL;
    int i;
    int start, end;

    static const char replace[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    static int replacelen = sizeof(replace) - 1;

    /* _O_TEMPORARY on Windows removes file when last descriptor is closed */
#ifndef O_TEMPORARY
#  define O_TEMPORARY 0
#endif

    if (!file_template || file_template[0] == '\0')
	return -1;

    /* identify the replacement suffix */
    start = end = strlen(file_template)-1;
    for (i=strlen(file_template)-1; i>=0; i--) {
	if (file_template[i] != 'X') {
	    break;
	}
	end = i;
    }

    do {
	/* replace the template with random chars */
	srand((unsigned)time(NULL));
	for (i=start; i>=end; i--) {
	    file_template[i] = replace[(int)(replacelen * ((double)rand() / (double)RAND_MAX))];
	}
	fd = open(file_template, O_CREAT | O_EXCL | O_TRUNC | O_RDWR | O_TEMPORARY, S_IRUSR | S_IWUSR);
    } while ((fd == -1) && (counter++ < 1000));

    return fd;
}
#endif


/**
 *  b u _ t e m p _ f i l e
 *
 * Create a temporary file.  The first readable/writable directory
 * will be used, searching TMPDIR/TEMP/TMP environment variable
 * directories followed by default system and ultimately trying the
 * current directory.
 *
 * This routine is guaranteed to return a new unique file or return
 * NULL on failure.  The temporary file will be automatically unlinked
 * on application exit.  It is the caller's responsibility to set file
 * access settings, preserve file contents, or destroy file contents
 * if the default behavior is non-optimal.
 *
 * Typical Use:
@code
 *	FILE *fp;
 *	char filename[MAXPATHLEN];
 *	fp = bu_temp_file(&filename, MAXPATHLEN); // get file name
 *	...
 *	fclose(fp); // optional, auto-closed on exit
 *
 *	...
 *
 *	fp = bu_temp_file(NULL, 0); // don't need file name
 *      fchmod(fileno(fp), 0777);
 *	...
 *	rewind(fp);
 *	while (fputc(0, fp) == 0);
 *	fclose(fp);
@endcode
 */
FILE *
bu_temp_file(char *file, int maxfilelen)
{
    FILE *fp = NULL;
    int i;
    int fd = -1;
    char *dir = NULL;
    char tempfile[MAXPATHLEN];
    const char *envdirs[] = {"TMPDIR", "TEMP", "TMP", NULL};

    if (maxfilelen > MAXPATHLEN) {
	maxfilelen = MAXPATHLEN;
    }

    /* check environment variable directories */
    for (i=0; envdirs[i]; i++) {
	dir = getenv(envdirs[i]);
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

    snprintf(tempfile, MAXPATHLEN, "%s%cBRL-CAD_temp_XXXXXXX", dir, BU_DIR_SEPARATOR);

    fd = mkstemp(tempfile);

    if (fd == -1) {
	perror("mkstemp");
	bu_log(_TF_FAIL);
	return NULL;
    }

    if (file) {
	snprintf(file, maxfilelen, "%s", tempfile);
    }

    fp = fdopen(fd, "wb+");
    if (fp == NULL) {
	perror("fdopen");
	bu_log(_TF_FAIL);
	close(fd);
	return NULL;
    }

    /* make it go away on close */
    unlink(tempfile);

    /* add the file to the atexit auto-close list */
    _bu_add_to_list(fp, fd);

    return fp;
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
