/*
  Author -
	Gary S. Moss

  Source -
	SECAD/VLD Computing Consortium, Bldg 394
	The U. S. Army Ballistic Research Laboratory
	Aberdeen Proving Ground, Maryland  21005-5066
  
  Copyright Notice -
	This software is Copyright (C) 1986 by the United States Army.
	All rights reserved.

	$Header$ (BRL)
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif
#include <stdio.h>
#if __STDC__ && !apollo
# include <stdarg.h>
#else
# include <varargs.h>
#endif
#include "machine.h"

#if __STDC__ && !apollo
/*
 *  			F B _ L O G
 *  
 *  Log a framebuffer library event in the Standard way.
 */
void
fb_log( char *fmt, ... )
{
	va_list ap;

	va_start( ap, fmt );
	(void)vfprintf( stderr, fmt, ap );
	va_end(ap);
}
#else /* __STDC__ */

#ifdef CRAY1
void
fb_log( fmt, a,b,c,d,e,f,g,h,i )
char	*fmt;
{
	fprintf( stderr, fmt, a,b,c,d,e,f,g,h,i );
}
#else
/* VARARGS */
void
fb_log( fmt, va_alist )
char	*fmt;
va_dcl
	{	va_list		ap;
	va_start( ap );
#if defined(alliant) && defined(i860)
	(void) vfprintf( stderr, fmt, ap);
#else
	(void) _doprnt( fmt, ap, stderr );
#endif
	va_end( ap );
	return;
	}
#endif
#endif /* !__STDC__ */
