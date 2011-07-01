/*                          R E A D . C
 * BRL-CAD
 *
 * Copyright (c) 1989-2011 United States Government as represented by
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
/** @file comgeom/read.c
 *
 * This module contains all of the routines necessary to read in
 * a COMGEOM input file, and put the information into internal form.
 *
 *  Author -
 *	Michael John Muuss
 *
 *  Original Version -
 *	March 17, 1980
 */

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>

#include "bu.h"

extern FILE	*infp;

extern char name_it[16];		/* argv[3] */

void	namecvt(int n, char **cp, int c);

/*
 *			G E T L I N E
 */
int
get_line(char *cp, int buflen, char *title)
{
    int	c;
    int	count = buflen;

    while ( (c = fgetc(infp)) == '\n' ) /* Skip blank lines.		*/
	;
    while ( c != EOF && c != '\n' )  {
	*cp++ = c;
	count--;
	if ( count <= 0 )  {
	    printf("get_line(x%lx, %d) input record overflows buffer for %s\n",
		   (unsigned long)cp, buflen, title);
	    break;
	}
	c = fgetc(infp);
    }
    if ( c == EOF )
	return	EOF;
    while ( count-- > 0 )
	*cp++ = 0;
    return	c;
}

/*
 *			G E T I N T
 */
int
getint(char *cp, int start, size_t len)
{
    char	buf[128];

    if (len > sizeof(buf))
	len = sizeof(buf);

    bu_strlcpy( buf, cp+start, len );
    return atoi(buf);
}

/*
 *			G E T D O U B L E
 */
double
getdouble(char *cp, int start, size_t len)
{
    char	buf[128];

    if (len > sizeof(buf))
	len = sizeof(buf);

    bu_strlcpy( buf, cp+start, len );
    return atof(buf);
}

/*		N A M E C V T	 */
void
namecvt(int n, char **cp, int c)
{
    char str[16];

    sprintf( str, "%c%d%.13s", (char)c, n, name_it );
    *cp = bu_strdup( str );
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
