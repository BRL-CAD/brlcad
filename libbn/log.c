/*
 *			L O G . C
 *
 *  A version of rt_log() for ordinary users of routines written
 *  originally for linking with librt.
 *
 *  Functions -
 *	rt_log		Called to log library events.
 *
 *  Author -
 *	Michael John Muuss
 *  
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1985 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static char RCSerror[] = "@(#)$Header$ (BRL)";
#endif

#include <stdio.h>
#include <varargs.h>
#include "machine.h"
#include "vmath.h"

#if defined( cray ) || defined( mips )
/* VARARGS */
void
rt_log(fmt, a,b,c,d,e,f,g,h,i)
char *fmt;
{
	fprintf(stderr, fmt, a,b,c,d,e,f,g,h,i );
}
#else
#ifdef BSD
/*
 *  			R T _ L O G
 *  
 *  Log an RT library event
 */
/* VARARGS */
void
rt_log(va_alist)
va_dcl
{
	va_list ap;
	char *fmt;

	va_start(ap);
	fmt = va_arg(ap,char *);

	(void) _doprnt( fmt, ap, stderr );
	va_end(ap);
}
#else
/*
 *  			R T _ L O G
 *  
 *  Log an RT library event
 */
/* VARARGS */
void
rt_log(va_alist)
va_dcl
{
	va_list ap;
	char *fmt;
	int a=0,b=0,c=0,d=0,e=0,f=0,g=0,h=0,i=0,j=0,k=0;
	int cnt=0;
	register char *cp;

	va_start(ap);
	fmt = va_arg(ap,char *);
	cp = fmt;
	while( *cp )  if( *cp++ == '%' )  {
		cnt++;
		if( sizeof(double) == sizeof(float) )
			continue;
		/* WARNING:  This assumes no fancy format specs like %.1f */
		switch( *cp )  {
		case 'e':
		case 'f':
		case 'g':
			cnt++;		/* Doubles are bigger! */
		}
	}

	if( cnt > 0 )  {
		a = va_arg(ap,int);
	}
	if( cnt > 1 )  {
		b = va_arg(ap,int);
	}
	if( cnt > 2 )  {
		c = va_arg(ap,int);
	}
	if( cnt > 3 )  {
		d = va_arg(ap,int);
	}
	if( cnt > 4 )  {
		e = va_arg(ap,int);
	}
	if( cnt > 5 )  {
		f = va_arg(ap,int);
	}
	if( cnt > 6 )  {
		g = va_arg(ap,int);
	}
	if( cnt > 7 )  {
		h = va_arg(ap,int);
	}
	if( cnt > 8 )  {
		i = va_arg(ap,int);
	}
	if( cnt > 9 )  {
		j = va_arg(ap,int);
	}
	if( cnt > 10 )  {
		k = va_arg(ap,int);
	}
	if( cnt > 11 )  {
		a = (int)fmt;
		fmt = "rt_log: Max args exceeded on: %s\n";
	}
	va_end(ap);

	fprintf(stderr, fmt, a,b,c,d,e,f,g,h,i,j,k );
}
#endif
#endif
