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
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "conf.h"

#include <stdio.h>
#ifdef HAVE_STDARG_H
#	include <stdarg.h>
#else
# if HAVE_VARARGS_H
#	include <varargs.h>
# endif
#endif

#include "machine.h"

#if defined(HAVE_STDARG_H)

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

#else  /* defined(HAVE_STDARG_H) */
# if defined(HAVE_VARARGS_H)

/* VARARGS */
void
fb_log( fmt, va_alist )
char	*fmt;
va_dcl
	{	va_list		ap;
	va_start( ap );
# ifdef HAVE_VPRINTF
	(void) vfprintf( stderr, fmt, ap);
# else
	(void) _doprnt( fmt, ap, stderr );
# endif
	va_end( ap );
	return;
	}

# else /* defined(HAVE_VARARGS_H) */

void
fb_log( fmt, a,b,c,d,e,f,g,h,i )
char	*fmt;
{
	fprintf( stderr, fmt, a,b,c,d,e,f,g,h,i );
}

# endif	/* defined(HAVE_VARARGS_H) */
#endif  /* defined(HAVE_STDARG_H) */
