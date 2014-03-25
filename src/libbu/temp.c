/*                           T E M P . C
 * BRL-CAD
 *
 * Copyright (c) 2001-2014 United States Government as represented by
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

#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef HAVE_SYS_TYPES_H
#  include <sys/types.h>
#endif
#ifdef HAVE_SYS_STAT_H
#  include <sys/stat.h>
#endif
#include "bio.h"

#include "bu/file.h"
#include "bu/log.h"
#include "bu/list.h"
#include "bu/malloc.h"
#include "bu/vls.h"

#define _TF_FAIL "WARNING: Unable to create a temporary file\n"


/* c99 doesn't declare these */
#if !defined(_WIN32) || defined(__CYGWIN__)
#  if !defined(__cplusplus)
extern FILE *fdopen(int, const char *);
#  endif
#endif


struct temp_file_list {
    struct bu_list l;
    struct bu_vls fn;
    int fd;
};

static int temp_files = 0;
static struct temp_file_list *TF = NULL;


HIDDEN void
temp_close_files(void)
{
    struct temp_file_list *popped;
    if (!TF) {
	return;
    }

    /* close all files, free their nodes, and unlink */
    while (BU_LIST_WHILE(popped, temp_file_list, &(TF->l))) {
	if (!popped)
	    break;

	/* take it off the list */
	BU_LIST_DEQUEUE(&(popped->l));

	/* close up shop */
	if (popped->fd != -1) {
	    close(popped->fd);
	    popped->fd = -1;
	}

	/* burn the house down */
	if (BU_VLS_IS_INITIALIZED(&popped->fn) && bu_vls_addr(&popped->fn)) {
	    bu_file_delete(bu_vls_addr(&popped->fn));
	    bu_vls_free(&popped->fn);
	}
	BU_PUT(popped, struct temp_file_list);
    }

    /* free the head */
    if (TF->fd != -1) {
	close(TF->fd);
	TF->fd = -1;
    }
    if (BU_VLS_IS_INITIALIZED(&TF->fn) && bu_vls_addr(&TF->fn)) {
	bu_file_delete(bu_vls_addr(&TF->fn));
	bu_vls_free(&TF->fn);
    }
    BU_PUT(TF, struct temp_file_list);
}


HIDDEN void
temp_add_to_list(const char *fn, int fd)
{
    struct temp_file_list *newtf;

    temp_files++;

    if (temp_files == 1) {
	/* schedule files for closure on exit */
	atexit(temp_close_files);

	BU_GET(TF, struct temp_file_list);
	BU_LIST_INIT(&(TF->l));
	bu_vls_init(&TF->fn);

	bu_vls_strcpy(&TF->fn, fn);
	TF->fd = fd;

	return;
    }

    BU_GET(newtf, struct temp_file_list);
    BU_LIST_INIT(&(TF->l));
    bu_vls_init(&TF->fn);

    bu_vls_strcpy(&TF->fn, fn);
    newtf->fd = fd;

    BU_LIST_PUSH(&(TF->l), &(newtf->l));

    return;
}


#ifndef HAVE_MKSTEMP
HIDDEN int
mkstemp(char *file_template)
{
    int fd = -1;
    int counter = 0;
    size_t i;
    size_t start, end;

    static const char replace[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    static int replacelen = sizeof(replace) - 1;

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
#else
/* for c99 strict, doesn't declare */
extern int mkstemp(char *);
#endif


FILE *
bu_temp_file(char *filepath, size_t len)
{
    FILE *fp = NULL;
    int i;
    int fd = -1;
    char tempfile[MAXPATHLEN];
    mode_t mask;
    const char *dir = NULL;
    const char *envdirs[] = {"TMPDIR", "TEMP", "TMP", NULL};
    const char *trydirs[] = {
#ifdef _WIN32
	"C:\\TEMP",
	"C:\\WINDOWS\\TEMP",
#endif
	"/tmp",
	"/usr/tmp",
	"/var/tmp",
	".", /* last resort */
	NULL
    };

    if (len > MAXPATHLEN) {
	len = MAXPATHLEN;
    }

    /* check environment variable directories */
    for (i=0; envdirs[i]; i++) {
	dir = getenv(envdirs[i]);
	if (dir && dir[0] != '\0' && bu_file_writable(dir) && bu_file_executable(dir)) {
	    break;
	}
    }

    if (!dir) {
	/* try various directories */
	for (i=0; trydirs[i]; i++) {
	    dir = trydirs[i];
	    if (dir && dir[0] != '\0' && bu_file_writable(dir) && bu_file_executable(dir)) {
		break;
	    }
	}
    }

    if (UNLIKELY(!dir)) {
	/* give up */
	bu_log("Unable to find a suitable temp directory\n");
	bu_log(_TF_FAIL);
	return NULL;
    }

    snprintf(tempfile, MAXPATHLEN, "%s%cBRL-CAD_temp_XXXXXXX", dir, BU_DIR_SEPARATOR);

    /* secure the temp file with user read-write only */
    mask = umask(S_IXUSR | S_IRGRP | S_IWGRP | S_IXGRP | S_IROTH | S_IWOTH | S_IXOTH);
    fd = mkstemp(tempfile);
    (void)umask(mask); /* restore */

    if (UNLIKELY(fd == -1)) {
	perror("mkstemp");
	bu_log(_TF_FAIL);
	return NULL;
    }

    if (filepath) {
	if (UNLIKELY(len < strlen(tempfile))) {
	    bu_log("INTERNAL ERROR: bu_temp_file filepath buffer size is insufficient (%zu < %zu)\n", len, strlen(tempfile));
	}
	snprintf(filepath, len, "%s", tempfile);
    }

    fp = fdopen(fd, "wb+");
    if (UNLIKELY(fp == NULL)) {
	perror("fdopen");
	bu_log(_TF_FAIL);
	close(fd);
	return NULL;
    }

    /* add the file to the atexit auto-close list */
    temp_add_to_list(tempfile, fd);

    return fp;
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
