/*                        S T R D U P . C
 * BRL-CAD
 *
 * Copyright (c) 1985-2011 United States Government as represented by
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
/** @file libsysv/strdup.c
 *
 *  Duplicate a string.
 *
 *  Author -
 *	Michael John Muuss
 *
 */

#include "common.h"

/* quell empty-compilation unit warnings */
static const int unused = 0;

#ifndef HAVE_STRDUP
#include "sysv.h"

/* for malloc */
#include <stdlib.h>
#include <string.h>


/*
 *			S T R D U P
 *
 * Given a string, allocate enough memory to hold it using malloc(),
 * duplicate the strings, returns a pointer to the new string.
 */
char *
strdup(register const char *cp)
{
    register char	*base;
    register int	len;

    len = strlen( cp )+2;
    if ( (base = (char *)malloc( len )) == (char *)0 )
	return (char *)0;

    memcpy(base, cp, len);
    return base;
}

#endif

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
