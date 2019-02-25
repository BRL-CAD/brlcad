/*                           D I R . C
 * BRL-CAD
 *
 * Copyright (c) 2018-2019 United States Government as represented by
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

#include "bu/app.h"
#include "bu/vls.h"
#include "bu/log.h"
#include "bu/str.h"
#include "bu/file.h"


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
#ifdef HAVE_FOUNDATION_FOUNDATION_H
    if (BU_STR_EMPTY(path)) {
	extern void dir_cache_macosx(char *buf, size_t len);
	dir_cache_macosx(path, MAXPATHLEN);
    }
#endif

    /* method #1c: platform standard (macosx) */
#if defined(HAVE_CONFSTR) && defined(_CS_DARWIN_CACHE_DIR)
    if (BU_STR_EMPTY(path)) {
	confstr(_CS_DARWIN_CACHE_DIR, path, len);
    }
#endif

    /* method #1d: platform standard (windows) */
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
append(struct bu_vls *vp, const char *buf)
{
    size_t len = bu_vls_strlen(vp);
    if (len && bu_vls_addr(vp)[len-1] != BU_DIR_SEPARATOR)
	bu_vls_putc(vp, BU_DIR_SEPARATOR);
    bu_vls_strcat(vp, buf);
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
		append(&vls, buf);
		break;
	    case BU_DIR_INIT:
		bu_log("UNIMPLEMENTED\n");
		break;
	    case BU_DIR_BIN:
		cpath = bu_brlcad_root(BRLCAD_BIN_DIR, 1);
		append(&vls, cpath);
		break;
	    case BU_DIR_LIB:
		cpath = bu_brlcad_root(BRLCAD_LIB_DIR, 1);
		append(&vls, cpath);
		break;
	    case BU_DIR_LIBEXEC:
		cpath = bu_brlcad_root(BRLCAD_LIBEXEC_DIR, 1);
		append(&vls, cpath);
		break;
	    case BU_DIR_INCLUDE:
		cpath = bu_brlcad_root(BRLCAD_INCLUDE_DIR, 1);
		append(&vls, cpath);
		break;
	    case BU_DIR_DATA:
		cpath = bu_brlcad_root(BRLCAD_DATA_DIR, 1);
		append(&vls, cpath);
		break;
	    case BU_DIR_DOC:
		cpath = bu_brlcad_root(BRLCAD_DOC_DIR, 1);
		append(&vls, cpath);
		break;
	    case BU_DIR_MAN:
		cpath = bu_brlcad_root(BRLCAD_MAN_DIR, 1);
		append(&vls, cpath);
		break;
	    case BU_DIR_TEMP:
		cpath = dir_temp(buf, MAXPATHLEN);
		append(&vls, cpath);
		break;
	    case BU_DIR_HOME:
		cpath = dir_home(buf, MAXPATHLEN);
		append(&vls, cpath);
		break;
	    case BU_DIR_CACHE:
		cpath = dir_cache(buf, MAXPATHLEN);
		append(&vls, cpath);
		break;
	    case BU_DIR_CONFIG:
		cpath = dir_config(buf, MAXPATHLEN);
		append(&vls, cpath);
		break;
	    case BU_DIR_EXT:
		bu_log("UNIMPLEMENTED\n");
		break;
	    case BU_DIR_LIBEXT:
		bu_log("UNIMPLEMENTED\n");
		break;
	    default:
		append(&vls, (const char *)arg);
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
