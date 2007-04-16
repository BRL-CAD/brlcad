/*                B R L C A D _ V E R S I O N . H
 * BRL-CAD
 *
 * Copyright (c) 2007 United States Government as represented by
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
 * PRIVATE container header for determining compile-time version
 * information about BRL-CAD.
 *
 * External applications should NOT use or include this file.  They
 * should use the library-specific LIBRARY_version() functions.
 * (e.g. bu_version())
 *
 * Author -
 *   Christopher Sean Morrison
 */

#include "common.h"

/* for snprintf */
#include <stdio.h>


/* FIXED VALUES */

/**
 * MAJOR.minor.patch version number
 *
 * should be an unquoted integer
 */
#ifndef BRLCAD_MAJOR
static const int BRLCAD_MAJOR =
#include "conf/MAJOR"
;
#endif

/**
 * major.MINOR.patch version number
 *
 * should be an unquoted integer
 */
#ifndef BRLCAD_MINOR
static const int BRLCAD_MINOR =
#include "conf/MINOR"
;
#endif

/**
 * major.minor.PATCH version number
 *
 * should be an unquoted integer
 */
#ifndef BRLCAD_PATCH
static const int BRLCAD_PATCH =
#include "conf/PATCH"
;
#endif


/* DYNAMIC VALUES */

/**
 * compilation count, updated every time a build pass occurs
 *
 * should be an unquoted integer
 */
#ifndef BRLCAD_COUNT
static const int BRLCAD_COUNT =
#include "conf/COUNT"
;
#endif

/**
 * compilation date, updated every time a build pass occurs
 *
 * should be a quoted value
 */
#ifndef BRLCAD_DATE
static const char BRLCAD_DATE[256] = {
#include "conf/DATE"
};
#endif

/**
 * compilation host, updated every time a build pass occurs
 *
 * should be a quoted value
 */
#ifndef BRLCAD_HOST
static const char BRLCAD_HOST[256] = {
#include "conf/HOST"
};
#endif

/**
 * configured installation path, updated every time a build pass
 * occurs
 *
 * should be a quoted value
 */
#ifndef BRLCAD_PATH
static const char BRLCAD_PATH[256] = {
#include "conf/PATH"
};
#endif

/**
 * compilation user, updated every time a build pass occurs
 *
 * should be a quoted value
 */
#ifndef BRLCAD_USER
static const char BRLCAD_USER[256] = {
#include "conf/USER"
};
#endif


/**
 * provides the version of BRL-CAD along with basic configuration and
 * compilation details.
 */
static const char *
brlcad_version(const char *title) {
    static char version[1024] = {0};
    static char label[64] = {0};
    
    if (title) {
	snprintf(label, 64, "  %s", title);
    }
	    
    if (version[0] == 0) {
	snprintf(version, 1024, 
		 "BRL-CAD Release %d.%d.%d%s\n"
		 "    %s, Compilation %d\n"
		 "    %s@%s:%s\n",
		 BRLCAD_MAJOR, BRLCAD_MINOR, BRLCAD_PATCH, label,
		 BRLCAD_DATE, BRLCAD_COUNT,
		 BRLCAD_USER, BRLCAD_HOST, BRLCAD_PATH
		 );
    }
    return version;
}



/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
