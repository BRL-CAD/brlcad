/*
 *			L I N E B U F . C
 *
 *	A portable way of doing setlinebuf().
 *
 *  Author -
 *	D. A. Gwyn
 *	Michael John Muuss
 *  
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5068  USA
 *  
 *  Distribution Status -
 *	Public Domain, Distribution Unlimited.
 */
#ifndef lint
static const char libbu_linebuf_RCSid[] = "@(#)$Header$ (ARL)";
#endif

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif



#include <stdio.h>

#include "machine.h"
#include "bu.h"

void
bu_setlinebuf(FILE *fp)
{
#ifdef WIN32
	(void) setvbuf( fp, (char *) NULL, _IOLBF, BUFSIZ );
#else
#ifdef BSD
	setlinebuf( fp );
#else
#	if defined( SYSV ) && !defined( sgi ) && !defined(CRAY2) && \
	 !defined(n16)
		(void) setvbuf( fp, (char *) NULL, _IOLBF, BUFSIZ );
#	endif
#	if defined(sgi) && defined(mips)
		if( setlinebuf( fp ) != 0 )
			perror("setlinebuf(fp)");
#	endif
#endif
#endif
}
