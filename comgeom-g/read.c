/*
 *			R E A D . C
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
 *  Copyright Notice -
 *	This software is Copyright (C) 1989 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "conf.h"

#include <stdio.h>
#include <math.h>
#ifdef USE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

#include "machine.h"
#include "externs.h"

extern FILE	*infp;

extern char name_it[16];		/* argv[3] */

void	namecvt();

/*
 *			G E T L I N E
 */
int
getline( cp, buflen, title )
register char *cp;
int	buflen;
char	*title;
{
	register int	c;
	register int	count = buflen;

	while( (c = fgetc(infp)) == '\n' ) /* Skip blank lines.		*/
		;
	while( c != EOF && c != '\n' )  {
		*cp++ = c;
		count--;
		if( count <= 0 )  {
			printf("getline(x%lx, %d) input record overflows buffer for %s\n",
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
getint( cp, start, len )
char	*cp;
int	start;
int	len;
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
getdouble( cp, start, len )
char	*cp;
int	start;
int	len;
{
	char	buf[128];

	if( len >= sizeof(buf) )  len = sizeof(buf)-1;

	strncpy( buf, cp+start, len );
	buf[len] = '\0';
	return atof(buf);	
}

/*		N A M E C V T	 */
void
namecvt( n, cp, c )
register char *cp;
register int n;
{
	static char str[32];

	sprintf( str, "%c%d%s", c, n, name_it );
	strncpy( cp, str, 16 );		/* truncate str to 16 chars.*/
}
