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
#include <varargs.h>

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
	(void) _doprnt( fmt, ap, stderr );
	va_end( ap );
	return;
	}
#endif
