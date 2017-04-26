/*                  B R L C A D _ I D E N T . H
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
/** @file brlcad_version.h
 *
 * PRIVATE container header for determining compile-time information
 * about BRL-CAD.
 *
 * External applications should NOT use or include this file.  They
 * should use the library-specific LIBRARY_version() functions.
 * (e.g. bu_version())
 *
 */

#ifndef BRLCAD_IDENT_H
#define BRLCAD_IDENT_H

#include "common.h"

/* for snprintf */
#include <stdio.h>
/* for strlen */
#include <string.h>

#include "brlcad_version.h"
#include "bu/file.h"


/* Compilation Settings */
#if 0
/**
 * Compilation date, updated every time a build pass occurs.
 */
static const char *BRLCAD_DATE = BRLCAD_COMPILE_DATE;

/**
 * Compilation host, updated every time a build pass occurs.
 */
static const char *BRLCAD_HOST = BRLCAD_COMPILE_HOST;

/**
 * Compilation user, updated every time a build pass occurs.
 */
static const char *BRLCAD_USER = BRLCAD_COMPILE_USER;

/**
 * Compilation count, updated every time a build pass occurs.
 */
static const int BRLCAD_COUNT = BRLCAD_COMPILE_COUNT;
#endif

typedef enum {
    BRLCAD_BUILD_DATE,
    BRLCAD_BUILD_HOST,
    BRLCAD_BUILD_USER,
    BRLCAD_BUILD_COUNT
} brlcad_info_t;

static const char *
brlcad_info(brlcad_info_t info)
{
    static char data[MAXPATHLEN] = {'\0'};
    static char host[MAXPATHLEN] = {'\0'};
    static char user[MAXPATHLEN] = {'\0'};
    static char count[MAXPATHLEN] = {'\0'};
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
	default:
	    return NULL;
    }
}

/**
 * Provides the release identifier details along with basic
 * configuration and compilation information.
 */
static const char *
brlcad_ident(const char *title)
{
    static char ident[1024] = {0};
    size_t isize = 0;

    /* header */
    snprintf(ident, 1024, "BRL-CAD Release %s", brlcad_version());

    /* optional title */
    isize = strlen(ident);
    if (title)
	snprintf(ident + isize, 1024 - isize, "  %s\n", title);

    /* compile info */
    isize = strlen(ident);
    snprintf(ident + isize, 1024 - isize,
	     "    %s, Compilation %s\n"
	     "    %s@%s\n",
	     brlcad_info(BRLCAD_BUILD_DATE), brlcad_info(BRLCAD_BUILD_COUNT),
	     brlcad_info(BRLCAD_BUILD_USER), brlcad_info(BRLCAD_BUILD_HOST)
	);

    return ident;
}



#endif /* BRLCAD_IDENT_H */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
