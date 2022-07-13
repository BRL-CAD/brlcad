/*                           D I R . C
 * BRL-CAD
 *
 * Copyright (c) 2018-2022 United States Government as represented by
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
/** @file dir.c
 *
 * Implementation of path-finding functions for general app use.
 *
 * TODO: make sure returned paths are absolute and exist
 * TODO: check for trailing path separator (e.g., temp)
 *
 */

#include "common.h"

#ifdef HAVE_SYS_PARAM_H /* for MAXPATHLEN */
#  include <sys/param.h>
#endif

#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#ifdef HAVE_SYS_TYPES_H
#  include <sys/types.h>
#endif
#ifdef HAVE_PWD_H
#  include <pwd.h>
#endif
#include "bio.h"
#ifdef HAVE_WINDOWS_H
#  include <ShlObj.h>
#endif

#include "whereami.h"

#include "bu/app.h"
#include "bu/debug.h"
#include "bu/file.h"
#include "bu/file.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "bu/path.h"
#include "bu/str.h"
#include "bu/vls.h"

#include "brlcad_version.h"

static const char DSLASH[2] = {BU_DIR_SEPARATOR, '\0'};


static void
nibble_trailing_slash(char *path)
{
    if ((strlen(path) > 1) && path[strlen(path)-1] == BU_DIR_SEPARATOR) {
	path[strlen(path)-1] = '\0'; /* nibble any trailing slash */
    }
}


static const char *
dir_temp(char *buf, size_t len)
{
    size_t i;
    const char *env;
    char path[MAXPATHLEN] = {0};

    if (!buf || !len)
	return buf;

    /* method #1: environment variable dirs */
    if (BU_STR_EMPTY(path)) {
	const char *envdirs[] = {"TMPDIR", "TEMP", "TMP", NULL};
	env = NULL;

	for (i=0; envdirs[i]; i++) {
	    env = getenv(envdirs[i]);
	    if (env && env[0] != '\0' && bu_file_writable(env) && bu_file_executable(env)) {
		break;
	    }
	}

	if (!BU_STR_EMPTY(env)) {
	    bu_strlcpy(path, env, MAXPATHLEN);
	}
    }

    /* method 2a: platform standard (macosx) */
#if defined(HAVE_CONFSTR) && defined(_CS_DARWIN_USER_TEMP_DIR)
    if (BU_STR_EMPTY(path)) {
	confstr(_CS_DARWIN_USER_TEMP_DIR, path, len);
    }
#endif

    /* method 2b: platform standard (windows */
#ifdef HAVE_WINDOWS_H
    if (BU_STR_EMPTY(path)) {
	char temp[MAXPATHLEN] = {0};
	if (GetTempPathA(MAXPATHLEN, temp) > 0) {
	    bu_strlcpy(path, temp, MAXPATHLEN);
	}
    }
#endif

    /* method #3: explicit paths */
    if (BU_STR_EMPTY(path)) {
	const char *dir = NULL;
	const char *trydirs[] = {
	    "C:\\TEMP",
	    "C:\\WINDOWS\\TEMP",
	    "/tmp",
	    "/usr/tmp",
	    "/var/tmp",
	    ".", /* last resort */
	    NULL
	};
	for (i=0; trydirs[i]; i++) {
	    dir = trydirs[i];
	    if (dir && dir[0] != '\0' && bu_file_writable(dir) && bu_file_executable(dir)) {
		break;
	    }
	}
	if (!BU_STR_EMPTY(dir)) {
	    bu_strlcpy(path, dir, MAXPATHLEN);
	}
    }

    nibble_trailing_slash(path);

    bu_strlcpy(buf, path, len);
    return buf;
}


static const char *
dir_home(char *buf, size_t len)
{
    const char *env;
    char path[MAXPATHLEN] = {0};

    if (!buf || !len)
	return buf;

    /* method #1: environment variable */
    if (BU_STR_EMPTY(path)) {
	env = getenv("HOME");
	if (env && strlen(env)) {
	    bu_strlcpy(path, env, len);
	}
    }

    /* method #2a: platform standard (linux/max) */
#ifdef HAVE_PWD_H
    if (BU_STR_EMPTY(path)) {
	struct passwd *upwd = getpwuid(getuid());
	if (upwd && upwd->pw_dir) {
	    bu_strlcpy(path, upwd->pw_dir, MAXPATHLEN);
	}
    }
#endif

    /* method #2b: platform standard (macosx) */
#if defined(HAVE_CONFSTR) && defined(_CS_DARWIN_USER_DIR)
    if (BU_STR_EMPTY(path)) {
	confstr(_CS_DARWIN_USER_DIR, path, len);
    }
#endif

    /* method #2c: platform standard (windows) */
#ifdef HAVE_WINDOWS_H
    if (BU_STR_EMPTY(path)) {
	PWSTR wpath;
	if (SHGetKnownFolderPath(&FOLDERID_Profile, 0, NULL, &wpath) == S_OK) {
	    wcstombs(path, wpath, MAXPATHLEN);
	    CoTaskMemFree(wpath);
	}
    }
#endif

    /* method #3: system root! */
    if (BU_STR_EMPTY(path)) {
	char root[2] = {BU_DIR_SEPARATOR, 0};
	bu_strlcpy(path, root, MAXPATHLEN);
    }

    nibble_trailing_slash(path);

    bu_strlcpy(buf, path, len);
    return buf;
}


static const char *
dir_cache(char *buf, size_t len)
{
    const char *env;
    char path[MAXPATHLEN] = {0};
    char temp[MAXPATHLEN] = {0};

    if (!buf || !len)
	return buf;

    /* method #1a: platform standard (linux) */
    if (BU_STR_EMPTY(path)) {
	env = getenv("XDG_CACHE_HOME");
	if (!BU_STR_EMPTY(env)) {
	    bu_strlcpy(path, env, len);
	}
    }

    /* method #1b: platform standard (macosx) */
#if defined(HAVE_CONFSTR) && defined(_CS_DARWIN_CACHE_DIR)
    if (BU_STR_EMPTY(path)) {
	confstr(_CS_DARWIN_CACHE_DIR, path, len);
    }
#endif

    /* method #1c: platform standard (windows) */
#ifdef HAVE_WINDOWS_H
    if (BU_STR_EMPTY(path)) {
	PWSTR wpath;
	if (SHGetKnownFolderPath(&FOLDERID_RoamingAppData, 0, NULL, &wpath) == S_OK) {
	    wcstombs(path, wpath, MAXPATHLEN);
	    CoTaskMemFree(wpath);
	}
    }
#endif

    /* method 2: fallback to home directory subdir */
    if (BU_STR_EMPTY(path)) {
	dir_home(temp, MAXPATHLEN);
	bu_strlcat(path, temp, MAXPATHLEN);
	bu_strlcat(path, DSLASH, MAXPATHLEN);
	bu_strlcat(path, ".cache", MAXPATHLEN);
    }

    /* method 3: fallback to temp directory subdir */
    if (BU_STR_EMPTY(path)) {
	dir_temp(temp, MAXPATHLEN);
	bu_strlcat(path, temp, MAXPATHLEN);
	bu_strlcat(path, DSLASH, MAXPATHLEN);
	bu_strlcat(path, ".cache", MAXPATHLEN);
    }

    nibble_trailing_slash(path);

    /* finally, append our application subdirectory */
    bu_strlcat(path, DSLASH, MAXPATHLEN);
    bu_strlcat(path, PACKAGE_NAME, MAXPATHLEN);

    bu_strlcpy(buf, path, len);
    return buf;
}


static const char *
dir_config(char *buf, size_t len)
{
    char home[MAXPATHLEN] = {0};

    dir_home(home, MAXPATHLEN);
    bu_strlcat(home, DSLASH, MAXPATHLEN);
    bu_strlcat(home, ".config", MAXPATHLEN);

    if (len) {
	bu_strlcpy(buf, home, len);
	return buf;
    }
    return NULL;
}


static void
path_append(struct bu_vls *vp, const char *buf)
{
    size_t len = bu_vls_strlen(vp);
    if (len && bu_vls_addr(vp)[len-1] != BU_DIR_SEPARATOR)
	bu_vls_putc(vp, BU_DIR_SEPARATOR);
    bu_vls_strcat(vp, buf);
}


/* arbitrary buffer size large enough to hold a couple paths and a label */
#define MAX_WHERE_SIZE (size_t)((MAXPATHLEN*2) + 64)

/**
 * put a left-hand and right-hand path together and test whether they
 * exist or not.
 *
 * @return boolean on whether a match was found.
 */
// TODO - this can be static once bu_brlcad_root is removed
int
_bu_dir_join_path(char result[MAXPATHLEN], const char *lhs, const char *rhs, struct bu_vls *searched, const char *where)
{
    size_t llen, rlen;
    static const char *currdir=".";

    /* swap right with left if there is no left so logic is simplified
     * later on.
     */
    if (lhs == NULL && rhs != NULL) {
	lhs = rhs;
	rhs = NULL;
    }

    if (lhs == NULL) {
	if (searched && where) {
	    bu_vls_strcat(searched, where);
	}
	return 0;
    }

    rlen = 0;
    if (rhs) {
	rlen = strlen(rhs);
    }

    /* be safe */
    if (rlen + 2 > MAXPATHLEN) {
	bu_log("Warning: path is way too long (%zu characters > %d)\n", rlen+2, MAXPATHLEN);
	if (searched && where) {
	    bu_vls_strcat(searched, where);
	}
	return 0;
    }

    /* an empty left hand implies current directory (plus a slash is appended later) */
    if (lhs[0] == '\0') {
	lhs = currdir;
    }

    /* left-hand path should exist independent of right-hand path */
    if (!bu_file_exists(lhs, NULL)) {
	if (searched && where) {
	    bu_vls_strcat(searched, where);
	}
	return 0;
    }

    /* start fresh */
    memset(result, 0, (size_t)MAXPATHLEN);
    bu_strlcpy(result, lhs, (size_t)MAXPATHLEN);

    /* nothing to add, so just return what we have */
    if (!rhs || (rlen == 0)) {
	return 1;
    }

    /* be safe again */
    llen = strlen(result);
    if (llen + rlen + 3 > MAXPATHLEN) {
	bu_log("Warning: path is way too long (%zu characters > %d)\n", llen+rlen+3, MAXPATHLEN);
	if (searched && where) {
	    bu_vls_strcat(searched, where);
	}
	return 0;
    }

    if ((*(result+llen-1) != BU_DIR_SEPARATOR) && (rhs[0] != BU_DIR_SEPARATOR)) {
	/* let the caller give "/usr/brlcad" and "bin" and get "/usr/brlcad/bin" */
	*(result+llen) = BU_DIR_SEPARATOR;
	llen++;
    } else if ((*(result+llen-1) == BU_DIR_SEPARATOR) && (rhs[0] == BU_DIR_SEPARATOR)) {
	/* let the caller give "/usr/brlcad/" and "/bin" and get "/usr/brlcad/bin"*/
	rhs++;
	rlen--;
    }

    /* found a match */
    bu_strlcpy(result+llen, rhs, (size_t)(MAXPATHLEN - llen));
    if (bu_file_exists(result, NULL)) {
	return 1;
    }

    /* close, but no match */
    if (searched && where) {
	bu_vls_strcat(searched, where);
    }
    return 0;
}


/**
 * print out an error/warning message if we cannot find the specified
 * BRLCAD_ROOT (compile-time install path)
 */
static void
_bu_dir_root_missing(const char *paths)
{
    bu_log("\
Unable to locate where BRL-CAD %s is installed while searching:\n\
%s\n\
This version of BRL-CAD was compiled to be installed at:\n\
	%s\n\n", brlcad_version(), paths, BRLCAD_ROOT);

    return;
}

static const char *
_bu_dir_brlcad_root(const char *rhs, int fail_quietly)
{
    static char result[MAXPATHLEN] = {0};
    const char *lhs;
    int length, dirname_length;
    struct bu_vls searched = BU_VLS_INIT_ZERO;
    char where[MAX_WHERE_SIZE] = {0};

    if (UNLIKELY(bu_debug & BU_DEBUG_PATHS)) {
	bu_log("_bu_dir_brlcad_root: searching for [%s]\n", rhs?rhs:"");
    }

    /* BRLCAD_ROOT environment variable if set */
    lhs = getenv("BRLCAD_ROOT");
    if (lhs) {
	snprintf(where, MAX_WHERE_SIZE, "\tBRLCAD_ROOT environment variable [%s]\n", lhs);
	if (_bu_dir_join_path(result, lhs, rhs, &searched, where)) {
	    if (UNLIKELY(bu_debug & BU_DEBUG_PATHS)) {
		bu_log("Found: BRLCAD_ROOT environment variable [%s]\n", result);
	    }
	    bu_vls_free(&searched);
	    return result;
	}
    } else {
	snprintf(where, MAX_WHERE_SIZE, "\tBRLCAD_ROOT environment variable\n");
	bu_vls_strcat(&searched, where);
    }

    /* Try run-time path identification based on libbu's location, if we can do
     * it.  This is preferable to the executable path when available, since
     * external codes may link into BRL-CAD's libraries but be installed
     * elsewhere on the filesystem. */
    length = wai_getModulePath(NULL, 0, &dirname_length);
    if (length > 0) {
	char *plhs  = (char *)bu_calloc(length+1, sizeof(char), "library path");
	wai_getModulePath(plhs, length, &dirname_length);
	plhs[length] = '\0';
	snprintf(where, MAX_WHERE_SIZE, "\trun-time path identification [UNKNOWN]\n");
	if (strlen(plhs)) {
	    char *dirpath;
	    char *real_path = bu_file_realpath(plhs, NULL);
	    dirpath = bu_path_dirname(real_path);
	    snprintf(real_path, MAXPATHLEN, "%s", dirpath);
	    bu_free(dirpath, "free bu_path_dirname");
	    dirpath = bu_path_dirname(real_path);
	    snprintf(real_path, MAXPATHLEN, "%s", dirpath);
	    bu_free(dirpath, "free bu_path_dirname");
	    if (_bu_dir_join_path(result, real_path, rhs, &searched, where)) {
		if (UNLIKELY(bu_debug & BU_DEBUG_PATHS)) {
		    bu_log("Found: Run-time path identification (lib) [%s]\n", result);
		}
		bu_vls_free(&searched);
		bu_free(real_path, "free real_path");
		bu_free(plhs, "free plhs");
		return result;
	    }
	    bu_free(real_path, "free real_path");
	} else {
	    bu_vls_strcat(&searched, where);
	}
	bu_free(plhs, "free plhs");
    }

    /* Try exec run-time path identification, if we can do it */
    length = wai_getExecutablePath(NULL, 0, &dirname_length);
    if (length > 0) {
	char *plhs  = (char *)bu_calloc(length+1, sizeof(char), "program path");
	wai_getExecutablePath(plhs, length, &dirname_length);
	plhs[length] = '\0';
	snprintf(where, MAX_WHERE_SIZE, "\trun-time path identification (exec) [UNKNOWN]\n");
	if (strlen(plhs)) {
	    char *dirpath;
	    char *real_path = bu_file_realpath(plhs, NULL);
	    dirpath = bu_path_dirname(real_path);
	    snprintf(real_path, MAXPATHLEN, "%s", dirpath);
	    bu_free(dirpath, "free bu_path_dirname");
	    dirpath = bu_path_dirname(real_path);
	    snprintf(real_path, MAXPATHLEN, "%s", dirpath);
	    bu_free(dirpath, "free bu_path_dirname");
	    if (_bu_dir_join_path(result, real_path, rhs, &searched, where)) {
		if (UNLIKELY(bu_debug & BU_DEBUG_PATHS)) {
		    bu_log("Found: Run-time path identification [%s]\n", result);
		}
		bu_vls_free(&searched);
		bu_free(real_path, "free real_path");
		bu_free(plhs, "free plhs");
		return result;
	    }
	    bu_free(real_path, "free real_path");
	} else {
	    bu_vls_strcat(&searched, where);
	}
	bu_free(plhs, "free plhs");
    }

    /* BRLCAD_ROOT compile-time path */
#ifdef BRLCAD_ROOT
    lhs = BRLCAD_ROOT;
    if (lhs) {
	snprintf(where, MAX_WHERE_SIZE, "\tBRLCAD_ROOT compile-time path [%s]\n", lhs);
	if (_bu_dir_join_path(result, lhs, rhs, &searched, where)) {
	    if (UNLIKELY(bu_debug & BU_DEBUG_PATHS)) {
		bu_log("Found: BRLCAD_ROOT compile-time path [%s]\n", result);
	    }
	    bu_vls_free(&searched);
	    return result;
	}
    }
#else
    snprintf(where, MAX_WHERE_SIZE, "\tBRLCAD_ROOT compile-time path [UNKNOWN]\n");
    bu_vls_strcat(&searched, where);
#endif

    /* current directory */
    if (_bu_dir_join_path(result, ".", rhs, &searched, "\tcurrent directory\n")) {
	if (UNLIKELY(bu_debug & BU_DEBUG_PATHS)) {
	    bu_log("Found: current directory [%s]\n", result);
	}
	bu_vls_free(&searched);
	return result;
    }

    if (!fail_quietly) {
	_bu_dir_root_missing(bu_vls_addr(&searched));
	if (rhs) {
	    bu_log("Unable to find '%s' within the BRL-CAD software installation.\nThis copy of BRL-CAD may not be properly installed.\n\n", rhs);
	} else {
	    bu_log("Unable to find the BRL-CAD software installation.\nThis copy of BRL-CAD may not be properly installed.\n\n");
	}
    }

    bu_vls_free(&searched);
    return NULL;
}

static const char *
vdir(char *result, size_t len, va_list args)
{
    struct bu_vls vls = BU_VLS_INIT_ZERO;
    static char buf[MAXPATHLEN] = {0};
    const char *cpath;
    uintptr_t arg;

    arg = va_arg(args, uintptr_t);

    memset(buf, 0, MAXPATHLEN);

    while (arg != (uintptr_t)NULL) {
	switch ((bu_dir_t)arg) {
	    case BU_DIR_CURR:
		bu_getcwd(buf, MAXPATHLEN);
		path_append(&vls, buf);
		break;
	    case BU_DIR_INIT:
		bu_getiwd(buf, MAXPATHLEN);
		path_append(&vls, buf);
		break;
	    case BU_DIR_BIN:
		cpath = _bu_dir_brlcad_root(BRLCAD_BIN_DIR, 1);
		path_append(&vls, cpath);
		break;
	    case BU_DIR_LIB:
		cpath = _bu_dir_brlcad_root(BRLCAD_LIB_DIR, 1);
		path_append(&vls, cpath);
		break;
	    case BU_DIR_LIBEXEC:
		cpath = _bu_dir_brlcad_root(BRLCAD_LIBEXEC_DIR, 1);
		path_append(&vls, cpath);
		break;
	    case BU_DIR_INCLUDE:
		cpath = _bu_dir_brlcad_root(BRLCAD_INCLUDE_DIR, 1);
		path_append(&vls, cpath);
		break;
	    case BU_DIR_DATA:
		cpath = _bu_dir_brlcad_root(BRLCAD_DATA_DIR, 1);
		path_append(&vls, cpath);
		break;
	    case BU_DIR_DOC:
		cpath = _bu_dir_brlcad_root(BRLCAD_DOC_DIR, 1);
		path_append(&vls, cpath);
		break;
	    case BU_DIR_MAN:
		cpath = _bu_dir_brlcad_root(BRLCAD_MAN_DIR, 1);
		path_append(&vls, cpath);
		break;
	    case BU_DIR_TEMP:
		cpath = dir_temp(buf, MAXPATHLEN);
		path_append(&vls, cpath);
		break;
	    case BU_DIR_HOME:
		cpath = dir_home(buf, MAXPATHLEN);
		path_append(&vls, cpath);
		break;
	    case BU_DIR_CACHE:
		cpath = dir_cache(buf, MAXPATHLEN);
		path_append(&vls, cpath);
		break;
	    case BU_DIR_CONFIG:
		cpath = dir_config(buf, MAXPATHLEN);
		path_append(&vls, cpath);
		break;
	    case BU_DIR_EXT:
		bu_vls_strcat(&vls, EXECUTABLE_SUFFIX);
		break;
	    case BU_DIR_LIBEXT:
		bu_vls_strcat(&vls, SHARED_LIBRARY_SUFFIX);
		break;
	    default:
		path_append(&vls, (const char *)arg);
		break;
	}
	arg = va_arg(args, uintptr_t);
    }

    if (len > 0 && bu_vls_strlen(&vls) > 0) {
	bu_strlcpy(result, bu_vls_cstr(&vls), len);
	bu_vls_free(&vls);
	return result;
    } else {
	memset(buf, 0, MAXPATHLEN);
	bu_strlcpy(buf, bu_vls_cstr(&vls), MAXPATHLEN);
    }
    bu_vls_free(&vls);
    return buf;
}


const char *
bu_dir(char *result, size_t len, .../*, NULL */)
{
    const char *ret;

    va_list args;

    va_start(args, len);
    ret = vdir(result, len, args);
    va_end(args);

    return ret;
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
