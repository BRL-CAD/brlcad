/*                           T E M P . C
 * BRL-CAD
 *
 * Copyright (c) 2001-2022 United States Government as represented by
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
#ifdef HAVE_WINDOWS_H
#  include<crtdbg.h> /* For _CrtSetReportMode */
#endif

#include "bu/app.h"
#include "bu/file.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "bu/time.h"
#include "bu/vls.h"

#define _TF_FAIL "WARNING: Unable to create a temporary file\n"

/* c99 doesn't declare these */
#if !defined(_WIN32) || defined(__CYGWIN__)
#  if !defined(__cplusplus)
extern FILE *fdopen(int, const char *);
#  endif
#endif

#define NUM_INITIAL_TEMP_FILES 8

struct temp_file {
    struct bu_vls fn;
    int fd;
};

struct temp_file initial_temp_files[NUM_INITIAL_TEMP_FILES];

static struct temp_file_list {
    size_t size, capacity;
    struct temp_file *temp_files;
} all_temp_files;

/* Get Windows to shut up about trying to close an already closed file -
 * the libbu temp_close_files routine is designed to make sure that any
 * files the user may not have closed get closed.  The way the MSVC
 * assertion was working, it would *preclude* the user from using close
 * on temp files. Disable the parameter handler for just this function
 * to avoid the issue. */
#if defined(HAVE_WINDOWS_H)
static void disable_duplicate_close_check(const wchar_t* UNUSED(expression),
					  const wchar_t* UNUSED(function),
					  const wchar_t* UNUSED(file),
					  unsigned int UNUSED(line),
					  uintptr_t UNUSED(pReserved)) {}
#endif

HIDDEN void
temp_close_files(void)
{
    size_t i;

#if defined(HAVE_WINDOWS_H)
    _invalid_parameter_handler stdhandler, nhandler;
#endif
    if (all_temp_files.size == 0) {
	return;
    }

#if defined(HAVE_WINDOWS_H)
    nhandler = disable_duplicate_close_check;
    stdhandler = _set_invalid_parameter_handler(nhandler);
    /* Turn off the message box as well. */
    (void)_CrtSetReportMode(_CRT_ASSERT, 0);
#endif

    /* close all files, free their nodes, and unlink */
    for (i = 0; i < all_temp_files.size; i++) {

	/* close up shop */
	if (all_temp_files.temp_files[i].fd != -1) {
	    close(all_temp_files.temp_files[i].fd);
	    all_temp_files.temp_files[i].fd = -1;
	}

	/* burn the house down */
	if (BU_VLS_IS_INITIALIZED(&all_temp_files.temp_files[i].fn) && bu_vls_addr(&all_temp_files.temp_files[i].fn)) {
	    bu_file_delete(bu_vls_addr(&all_temp_files.temp_files[i].fn));
	    bu_vls_free(&all_temp_files.temp_files[i].fn);
	}
    }

#if defined(HAVE_WINDOWS_H)
    /* Now that we're done, restore default behavior */
    (void)_set_invalid_parameter_handler(stdhandler);
#endif

}


HIDDEN void
temp_add_to_list(const char *fn, int fd)
{
    if (all_temp_files.capacity == 0) {
	all_temp_files.capacity = NUM_INITIAL_TEMP_FILES;
	all_temp_files.temp_files = initial_temp_files;
    } else if (all_temp_files.size == NUM_INITIAL_TEMP_FILES) {
	all_temp_files.capacity *= 2;
	all_temp_files.temp_files = (struct temp_file *)bu_malloc(all_temp_files.capacity * sizeof(struct temp_file), "initial resize of temp file list");
    } else if (all_temp_files.size == all_temp_files.capacity) {
	all_temp_files.capacity *= 2;
	all_temp_files.temp_files = (struct temp_file *)bu_realloc(all_temp_files.temp_files, all_temp_files.capacity * sizeof(struct temp_file), "additional resize of temp file list");
    }

    if (all_temp_files.size == 0) {
	/* schedule files for closure on exit */
	atexit(temp_close_files);
    }

    bu_vls_init(&all_temp_files.temp_files[all_temp_files.size].fn);
    bu_vls_strcpy(&all_temp_files.temp_files[all_temp_files.size].fn, fn);
    all_temp_files.temp_files[all_temp_files.size].fd = fd;

    all_temp_files.size++;
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
	srand((unsigned)(bu_gettime() % UINT_MAX));
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
	"C:\\TEMP",
	"C:\\WINDOWS\\TEMP",
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

    if (strlen(dir) > 1 && dir[strlen(dir)-1] == BU_DIR_SEPARATOR)
	snprintf(tempfile, MAXPATHLEN, "%s" PACKAGE_NAME "_temp_XXXXXXX", dir);
    else
	snprintf(tempfile, MAXPATHLEN, "%s%c" PACKAGE_NAME "_temp_XXXXXXX", dir, BU_DIR_SEPARATOR);

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
