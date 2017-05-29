/*                      V E R S I O N . C
 * BRL-CAD
 *
 * Copyright (c) 2007-2016 United States Government as represented by
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
#include <limits.h> /* for INT_MAX */
#include <string.h> /* for strlen */
#include "bu/file.h"
#include "bu/version.h"
#include "bu/vls.h"

/* The code below is an experiment to see if we can replace compile-time
 * definitions of version and build info (and its attendant re-building
 * work whenever something changes) with run-time file-based info. */

typedef enum {
    BRLCAD_MAJOR,
    BRLCAD_MINOR,
    BRLCAD_PATCH
} brlcad_version_t;

typedef enum {
    BRLCAD_BUILD_DATE,
    BRLCAD_BUILD_HOST,
    BRLCAD_BUILD_USER,
    BRLCAD_BUILD_COUNT,
    BRLCAD_VERSION_MAJOR,
    BRLCAD_VERSION_MINOR,
    BRLCAD_VERSION_PATCH,
    BRLCAD_VERSION
} brlcad_info_t;

BU_EXPORT int brlcad_version(brlcad_version_t vt);

BU_EXPORT const char *brlcad_info(brlcad_info_t infot);


int
brlcad_version(brlcad_version_t vt)
{
    static int B_MAJOR = 0;
    static int B_MINOR = 0;
    static int B_PATCH = 0;
    struct bu_vls fdata = BU_VLS_INIT_ZERO;
    const char *dpath;
    FILE *fp;
    char *endptr = NULL;
    long int l;
    int ret = 0;

    switch (vt) {
	case BRLCAD_MAJOR:
	    if (!B_MAJOR) {
		dpath = bu_brlcad_data("data/BRLCAD_VERSION_MAJOR", 1);
		if (!dpath || ((fp = fopen(dpath, "r")) == NULL)) return 0;
	    } else {
		ret = B_MAJOR;
	    }
	    break;
	case BRLCAD_MINOR:
	    if (!B_MINOR) {
		dpath = bu_brlcad_data("data/BRLCAD_VERSION_MINOR", 1);
		if (!dpath || ((fp = fopen(dpath, "r")) == NULL)) return 0;
	    } else {
		ret = B_MINOR;
	    }
	    break;
	case BRLCAD_PATCH:
	    if (!B_PATCH) {
		dpath = bu_brlcad_data("data/BRLCAD_VERSION_PATCH", 1);
		if (!dpath || ((fp = fopen(dpath, "r")) == NULL)) return 0;
	    } else {
		ret = B_PATCH;
	    }
	    break;
	default:
	    return 0;
    }

    if (dpath) {
	if (bu_vls_gets(&fdata, fp) <= 0) return 0;
	errno = 0;
	l = strtol(bu_vls_addr(&fdata), &endptr, 0);
	if ((endptr != NULL && strlen(endptr) > 0) || errno == ERANGE) return 0;

	if (l <= INT_MAX && l >= -INT_MAX) {
	    ret = (int)l;
	} else {
	    ret = 0;
	}
	switch (vt) {
	    case BRLCAD_MAJOR:
		B_MAJOR = ret;
		break;
	    case BRLCAD_MINOR:
		B_MINOR = ret;
		break;
	    case BRLCAD_PATCH:
		B_PATCH = ret;
		break;
	}
    }

    return ret;
}

const char *
brlcad_info(brlcad_info_t info)
{
    static char data[MAXPATHLEN] = {'\0'};
    static char host[MAXPATHLEN] = {'\0'};
    static char user[MAXPATHLEN] = {'\0'};
    static char count[MAXPATHLEN] = {'\0'};
    static char major[MAXPATHLEN] = {'\0'};
    static char minor[MAXPATHLEN] = {'\0'};
    static char patch[MAXPATHLEN] = {'\0'};
    static char version[MAXPATHLEN] = {'\0'};
    const char *dpath;
    FILE *fp;
    struct bu_vls fdata = BU_VLS_INIT_ZERO;
    switch (info) {
	case BRLCAD_BUILD_DATE:
	    if (data[0] == '\0') {
		dpath = bu_brlcad_data("data/BRLCAD_BUILD_DATE", 1);
		if (!dpath || ((fp = fopen(dpath, "r")) == NULL)) return NULL;
		if (bu_vls_gets(&fdata, fp) <= 0) return NULL;
		snprintf(data, MAXPATHLEN, "%s", bu_vls_addr(&fdata));
		bu_vls_free(&fdata);
	    }
	    return data;
	    break;
	case BRLCAD_BUILD_HOST:
	    if (host[0] == '\0') {
		dpath = bu_brlcad_data("data/BRLCAD_BUILD_HOST", 1);
		if (!dpath || ((fp = fopen(dpath, "r")) == NULL)) return NULL;
		if (bu_vls_gets(&fdata, fp) <= 0) return NULL;
		snprintf(host, MAXPATHLEN, "%s", bu_vls_addr(&fdata));
		bu_vls_free(&fdata);
	    }
	    return host;
	    break;
	case BRLCAD_BUILD_USER:
	    if (user[0] == '\0') {
		dpath = bu_brlcad_data("data/BRLCAD_BUILD_USER", 1);
		if (!dpath || ((fp = fopen(dpath, "r")) == NULL)) return NULL;
		if (bu_vls_gets(&fdata, fp) <= 0) return NULL;
		snprintf(user, MAXPATHLEN, "%s", bu_vls_addr(&fdata));
		bu_vls_free(&fdata);
	    }
	    return user;
	    break;
	case BRLCAD_BUILD_COUNT:
	    if (count[0] == '\0') {
		dpath = bu_brlcad_data("data/BRLCAD_BUILD_COUNT", 1);
		if (!dpath || ((fp = fopen(dpath, "r")) == NULL)) return NULL;
		if (bu_vls_gets(&fdata, fp) <= 0) return NULL;
		snprintf(count, MAXPATHLEN, "%s", bu_vls_addr(&fdata));
		bu_vls_free(&fdata);
	    }
	    return count;
	    break;
	case BRLCAD_VERSION_MAJOR:
	    if (major[0] == '\0') {
		snprintf(major, MAXPATHLEN, "%d", brlcad_get_version(BRLCAD_MAJOR));
	    }
	    return major;
	    break;
	case BRLCAD_VERSION_MINOR:
	    if (major[0] == '\0') {
		snprintf(major, MAXPATHLEN, "%d", brlcad_get_version(BRLCAD_MINOR));
	    }
	    return minor;
	    break;
	case BRLCAD_VERSION_PATCH:
	    if (major[0] == '\0') {
		snprintf(major, MAXPATHLEN, "%d", brlcad_get_version(BRLCAD_PATCH));
	    }
	    return patch;
	    break;
	case BRLCAD_VERSION:
	    if (major[0] == '\0') {
		snprintf(major, MAXPATHLEN, "%d.%d.%d", brlcad_get_version(BRLCAD_MAJOR), brlcad_get_version(BRLCAD_MINOR), brlcad_get_version(BRLCAD_PATCH));
	    }
	    return version;
	    break;
	default:
	    return NULL;
    }
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
