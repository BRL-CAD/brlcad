/*                          R E A D . C
 * BRL-CAD
 *
 * Copyright (C) 1989-2005 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this file; see the file named COPYING for more
 * information.
 *
 */
/** @file read.c
 *
 * This module contains all of the routines necessary to read in
 * a COMGEOM input file, and put the information into internal form.
 *
 *  Author -
 *	Michael John Muuss
 *
 *  Original Version -
 *	March 17, 1980
 *
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *
 */
#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "common.h"

#include <stdio.h>
#include <math.h>
#ifdef HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

#include "machine.h"
#include "bu.h"

extern FILE	*infp;

extern char name_it[16];		/* argv[3] */

void	namecvt(register int n, register char **cp, int c);

/*
 *			G E T L I N E
 */
int
get_line(register char *cp, int buflen, char *title)
{
	register int	c;
	register int	count = buflen;

	while( (c = fgetc(infp)) == '\n' ) /* Skip blank lines.		*/
		;
	while( c != EOF && c != '\n' )  {
		*cp++ = c;
		count--;
		if( count <= 0 )  {
			printf("get_line(x%lx, %d) input record overflows buffer for %s\n",
			       (unsigned long)cp, buflen, title);
			break;
		}
		c = fgetc(infp);
	}
	if( c == EOF )
		return	EOF;
	while( count-- > 0 )
		*cp++ = 0;
	return	c;
}

/*
 *			G E T I N T
 */
int
getint(char *cp, int start, int len)
{
	char	buf[128];

	if( len >= sizeof(buf) )  len = sizeof(buf)-1;

	strncpy( buf, cp+start, len );
	buf[len] = '\0';
	return atoi(buf);
}

/*
 *			G E T D O U B L E
 */
double
getdouble(char *cp, int start, int len)
{
	char	buf[128];

	if( len >= sizeof(buf) )  len = sizeof(buf)-1;

	strncpy( buf, cp+start, len );
	buf[len] = '\0';
	return atof(buf);
}

/*		N A M E C V T	 */
void
namecvt(register int n, register char **cp, int c)
{
	char str[16];

	sprintf( str, "%c%d%.13s", (char)c, n, name_it );
	*cp = bu_strdup( str );
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
