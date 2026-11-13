/*                           T E M P . C
 * BRL-CAD
 *
 * Copyright (c) 2001-2026 United States Government as represented by
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

#include <errno.h>
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
#include "bu/datetime.h"
#include "bu/vls.h"
#include "bu/str.h"
#include "bu/parallel.h"
#include "bu/process.h"

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

static void
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


static void
temp_add_to_list(const char *fn, int fd)
{
    if (all_temp_files.capacity == 0) {
	all_temp_files.capacity = NUM_INITIAL_TEMP_FILES;
	all_temp_files.temp_files = initial_temp_files;
    } else if (all_temp_files.size == all_temp_files.capacity) {
	all_temp_files.capacity *= 2;
	const size_t bytes = all_temp_files.capacity * sizeof(struct temp_file);
	if (all_temp_files.temp_files == initial_temp_files) {
	    all_temp_files.temp_files = (struct temp_file *)bu_malloc(bytes, "temp file list");
	    memcpy(all_temp_files.temp_files, initial_temp_files,
		all_temp_files.size * sizeof(struct temp_file));
	} else {
	    all_temp_files.temp_files = (struct temp_file *)bu_realloc(all_temp_files.temp_files,
		bytes, "temp file list");
	}
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

#define MAX_FILELEN 25	/* arbitrary len */
const char*
bu_temp_file_name(char* filename, size_t len)
{
    static char buf[MAX_FILELEN] = {0};

    memset(buf, 0, MAX_FILELEN);

    /* create name in form of prefix_procID_threadID */
    const char* prefix = (filename && filename[0]) ? filename : PACKAGE_NAME;
    int procID = bu_pid();
    int threadID = bu_thread_id();
    snprintf(buf, MAX_FILELEN, "%s_%d_%d", prefix, procID, threadID);

    /* if user supplied buffer, copy over */
    if (len > 0 && strlen(buf) > 0) {
	int maxlen = len > MAX_FILELEN ? MAX_FILELEN : len; /* cap len at MAX_FILELEN */
	bu_strlcpy(filename, buf, maxlen);
	return filename;
    }

    return buf;
}

/* Strict C modes may omit the platform declarations. */
#ifdef HAVE_MKSTEMPS
extern int mkstemps(char *, int);
#elif defined(HAVE_MKSTEMP)
extern int mkstemp(char *);
#endif

static int
temp_open_unique(char *file_template, size_t suffix_length)
{
#ifdef HAVE_MKSTEMPS
    return mkstemps(file_template, (int)suffix_length);
#else
#  ifdef HAVE_MKSTEMP
    if (suffix_length == 0)
	return mkstemp(file_template);
#  endif
    /* Preserve exclusive creation on platforms without suffix-aware mkstemp. */
    static const char replacements[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    const size_t minimum_placeholders = 6;
    const int maximum_attempts = 1000;
    const size_t end = strlen(file_template) - suffix_length;
    size_t start = end;
    while (start > 0 && file_template[start - 1] == 'X')
	--start;
    if (end - start < minimum_placeholders) {
	errno = EINVAL;
	return -1;
    }

    srand((unsigned)(bu_gettime() % UINT_MAX));
    for (int attempt = 0; attempt < maximum_attempts; ++attempt) {
	for (size_t i = start; i < end; ++i)
	    file_template[i] = replacements[(size_t)rand() % (sizeof(replacements) - 1)];
	int fd = open(file_template, O_CREAT | O_EXCL | O_RDWR | O_BINARY, S_IRUSR | S_IWUSR);
	if (fd != -1 || errno != EEXIST)
	    return fd;
    }
    return -1;
#endif
}

FILE *
bu_temp_file_suffix(char *filepath, size_t len, const char *suffix)
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

    if (!suffix)
	suffix = "";
    if (strchr(suffix, '/') || strchr(suffix, '\\')) {
	bu_log("Temporary filename suffix must not contain directory separators\n");
	return NULL;
    }

    /* Check each candidate, retaining only a usable directory. */
    for (i = 0; envdirs[i]; ++i) {
	const char *candidate = getenv(envdirs[i]);
	if (candidate && candidate[0] && bu_file_directory(candidate) &&
	    bu_file_writable(candidate) && bu_file_executable(candidate)) {
	    dir = candidate;
	    break;
	}
    }
    for (i = 0; !dir && trydirs[i]; ++i) {
	if (bu_file_directory(trydirs[i]) && bu_file_writable(trydirs[i]) &&
	    bu_file_executable(trydirs[i]))
	    dir = trydirs[i];
    }
    if (!dir) {
	bu_log("Unable to find a suitable temp directory\n");
	bu_log(_TF_FAIL);
	return NULL;
    }

    const char native_separator[] = {BU_DIR_SEPARATOR, '\0'};
    const char *separator = dir[strlen(dir) - 1] == BU_DIR_SEPARATOR ? "" : native_separator;
    int length = snprintf(tempfile, sizeof(tempfile), "%s%s" PACKAGE_NAME "_temp_XXXXXXX%s", dir, separator, suffix);
    if (length < 0 || (size_t)length >= sizeof(tempfile) ||
	(filepath && len <= (size_t)length)) {
	bu_log("Temporary filename buffer is too small\n");
	return NULL;
    }

    /* secure the temp file with user read-write only */
    mask = umask(S_IXUSR | S_IRGRP | S_IWGRP | S_IXGRP | S_IROTH | S_IWOTH | S_IXOTH);
    fd = temp_open_unique(tempfile, strlen(suffix));
    (void)umask(mask); /* restore */

    if (UNLIKELY(fd == -1)) {
	perror("temporary file creation");
	bu_log(_TF_FAIL);
	return NULL;
    }

    /* Ensure the descriptor is binary before associating it with a stream. */
    if (UNLIKELY(setmode(fd, O_BINARY) == -1)) {
	perror("setmode");
	close(fd);
	bu_file_delete(tempfile);
	bu_log(_TF_FAIL);
	return NULL;
    }

    if (filepath)
	memcpy(filepath, tempfile, (size_t)length + 1);

    fp = fdopen(fd, "wb+");
    if (UNLIKELY(fp == NULL)) {
	perror("fdopen");
	bu_log(_TF_FAIL);
	close(fd);
	bu_file_delete(tempfile);
	return NULL;
    }

    /* add the file to the atexit auto-close list */
    temp_add_to_list(tempfile, fd);

    return fp;
}

FILE *
bu_temp_file(char *filepath, size_t len)
{
    return bu_temp_file_suffix(filepath, len, NULL);
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
