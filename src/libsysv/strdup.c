/*                        S T R D U P . C
 * BRL-CAD
 *
 * Copyright (C) 1985-2005 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file strdup.c
 *
 *  Duplicate a string.
 *
 *  Author -
 *	Michael John Muuss
 *
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 */

#include "common.h"


/* quell empty-compilation unit warnings */
static const int unused = 0;

#ifndef HAVE_STRDUP

/* for malloc */
#include <stdlib.h>

#ifdef HAVE_STRING_H
#  include <string.h>
#else
#  ifdef HAVE_STRINGS_H
#    include <strings.h>
#  endif
#endif


/*
 *			S T R D U P
 *
 * Given a string, allocate enough memory to hold it using malloc(),
 * duplicate the strings, returns a pointer to the new string.
 */
char *
strdup( cp )
register const char *cp;
{
	register char	*base;
	register int	len;

	len = strlen( cp )+2;
	if( (base = (char *)malloc( len )) == (char *)0 )
		return( (char *)0 );

	bcopy( cp, base, len );
	return(base);
}

#endif

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
