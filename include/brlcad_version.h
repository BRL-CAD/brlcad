/*                B R L C A D _ V E R S I O N . H
 * BRL-CAD
 *
 * Copyright (c) 2007-2014 United States Government as represented by
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
 */

#ifndef BRLCAD_VERSION_H
#define BRLCAD_VERSION_H

#include "common.h"


/**************************************************/
/* Compile-time version information (discouraged) */
/**************************************************/

/**
 * Compile-time macro for testing whether the current compilation
 * matches or precedes a given version triplet.  Returns false if the
 * triplet come after the current compilation version, true otherwise.
 * Conventional use allows calling code to support multiple API
 * versions simultaneously, particularly with respect to deprecation
 * changes (see CHANGES).
 *
 @code
#if BRLCAD_API(7,24,6)
... code that worked in 7.24.6 API and prior ...
#else
... current API ...
#endif
 @endcode
 */
#define BRLCAD_API(_major, _minor, _patch) \
    (((_major) < BRLCAD_VERSION_MAJOR) || \
     ((_major) == BRLCAD_VERSION_MAJOR && (_minor) < BRLCAD_VERSION_MINOR) || \
     ((_major) == BRLCAD_VERSION_MAJOR && (_minor) == BRLCAD_VERSION_MINOR && (_patch) <= BRLCAD_VERSION_PATCH))


/*********************************************/
/* Run-time version information (encouraged) */
/*********************************************/

/**
 * Run-time integer of the MAJOR.minor.patch version number.
 */
static const int BRLCAD_MAJOR = BRLCAD_VERSION_MAJOR;


/**
 * Run-time integer of the major.MINOR.patch version number.
 */
static const int BRLCAD_MINOR = BRLCAD_VERSION_MINOR;


/**
 * Run-time integer of the major.minor.PATCH version number.
 */
static const int BRLCAD_PATCH = BRLCAD_VERSION_PATCH;


/* helper macros to turn a preprocessor value into a string */

#define NUMHASH(x) #x
#define NUM2STR(x) NUMHASH(x)

/**
 * Run-time string of the "MAJOR.MINOR.PATCH" version number.
 */
static char BRLCAD_VERSION[32] = NUM2STR(BRLCAD_VERSION_MAJOR) "." NUM2STR(BRLCAD_VERSION_MINOR) "." NUM2STR(BRLCAD_VERSION_PATCH);


__BEGIN_DECLS

/**
 * Provides the version string in MAJOR.MINOR.PATCH triplet form.
 */
static inline const char *
brlcad_version(void)
{
    return BRLCAD_VERSION;
}

__END_DECLS


#endif /* BRLCAD_VERSION_H */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
