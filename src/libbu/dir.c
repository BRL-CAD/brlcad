/*                           D I R . C
 * BRL-CAD
 *
 * Copyright (c) 2018 United States Government as represented by
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
 */

#include "common.h"

#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#include "bu/app.h"
#include "bu/vls.h"
#include "bu/log.h"
#include "bu/str.h"


static const char *
dir_temp(char *buf, size_t len)
{
    const char *temp = getenv("TEMP");

    if (len && temp && strlen(temp)) {
	bu_strlcpy(buf, temp, len);
	return buf;
    }
    return NULL;
}


static const char *
dir_home(char *buf, size_t len)
{
    const char *home = getenv("HOME");

    if (len && home && strlen(home)) {
	bu_strlcpy(buf, home, len);
	return buf;
    }
    return NULL;
}


static const char *
dir_cache(char *buf, size_t len)
{
    char home[MAXPATHLEN] = {0};

    dir_home(home, MAXPATHLEN);
    bu_strlcat(home, "/.cache", MAXPATHLEN);

    if (len) {
	bu_strlcpy(buf, home, len);
	return buf;
    }
    return NULL;
}


static const char *
dir_config(char *buf, size_t len)
{
    char home[MAXPATHLEN] = {0};

    dir_home(home, MAXPATHLEN);
    bu_strlcat(home, "/.config", MAXPATHLEN);

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
    char buf[MAXPATHLEN] = {0};
    const char *cpath;
    uintptr_t arg;

    arg = va_arg(args, uintptr_t);

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
		cpath = bu_brlcad_root("bin", 1);
		append(&vls, cpath);
		break;
	    case BU_DIR_LIB:
		cpath = bu_brlcad_root("lib", 1);
		append(&vls, cpath);
		break;
	    case BU_DIR_LIBEXEC:
		cpath = bu_brlcad_root("libexec", 1);
		append(&vls, cpath);
		break;
	    case BU_DIR_INCLUDE:
		cpath = bu_brlcad_root("include", 1);
		append(&vls, cpath);
		break;
	    case BU_DIR_DATA:
		cpath = bu_brlcad_root("data", 1);
		append(&vls, cpath);
		break;
	    case BU_DIR_DOC:
		cpath = bu_brlcad_root("doc", 1);
		append(&vls, cpath);
		break;
	    case BU_DIR_MAN:
		cpath = bu_brlcad_root("man", 1);
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
    }
    bu_vls_free(&vls);
    return NULL;
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
