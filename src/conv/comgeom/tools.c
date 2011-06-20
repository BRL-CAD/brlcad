/*                         T O O L S . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2011 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 *
 */
/** @file comgeom/tools.c
 *
 */

#include "common.h"

#include <stdlib.h>

#define PADCHR		~(1<<15)		/* non data value.*/

char *
endstr(char *str)
{
    if (!str)
	return NULL;

    while ( *str != '\0' ) {
	str++;
    }

    return str;
}


void
strappend(char *s, char *t)	/* === */
{
    s = endstr( s );
    while ( (*s++ = *t++) != '\0' );
    *s = '\0';
}

void
maxmin(int *l, int n, int *max, int *min)	/*  === */
{
    *max = -PADCHR;
    *min =  PADCHR;
    while ( --n>0 )
    {
	if ( *l > *max )	*max = *l;
	if ( *l < *min )	*min = *l;
	++l;
    }
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
