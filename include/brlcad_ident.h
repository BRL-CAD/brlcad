/*                  B R L C A D _ I D E N T . H
 * BRL-CAD
 *
 * Copyright (c) 2007-2013 United States Government as represented by
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

#ifndef __BRLCAD_IDENT_H__
#define __BRLCAD_IDENT_H__

#include "common.h"

/* for snprintf */
#include <stdio.h>
/* for strlen */
#include <string.h>

#include "brlcad_version.h"


/* Compilation Settings */

/**
 * Compilation date, updated every time a build pass occurs.
 */
static const char *BRLCAD_DATE = BRLCAD_COMPILE_DATE;

/**
 * Compilation host, updated every time a build pass occurs.
 */
static const char *BRLCAD_HOST = BRLCAD_COMPILE_HOST;

/**
 * Installation path, updated every time a build pass occurs.
 */
static const char *BRLCAD_PATH = BRLCAD_COMPILE_PATH;

/**
 * Compilation user, updated every time a build pass occurs.
 */
static const char *BRLCAD_USER = BRLCAD_COMPILE_USER;

/**
 * Compilation count, updated every time a build pass occurs.
 */
static const int BRLCAD_COUNT = BRLCAD_COMPILE_COUNT;


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
	     "    %s, Compilation %d\n"
	     "    %s@%s:%s\n",
	     BRLCAD_DATE, BRLCAD_COUNT,
	     BRLCAD_USER, BRLCAD_HOST, BRLCAD_PATH
	);

    return ident;
}


#endif /* __BRLCAD_IDENT_H__ */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
