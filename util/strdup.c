/*
 *			S T R D U P . C
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
 *  
 *  Distribution Status -
 *	Public Domain, Distribution Unlimitied.
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "conf.h"

#if !defined(HAVE_STRDUP)

#include <stdio.h>

#include "machine.h"
#include "externs.h"

/*
 *			S T R D U P
 *
 * Given a string, allocate enough memory to hold it using malloc(),
 * duplicate the strings, returns a pointer to the new string.
 */
char *
strdup( cp )
register CONST char *cp;
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
