/*
	Author:		Gary S. Moss
			U. S. Army Ballistic Research Laboratory
			Aberdeen Proving Ground
			Maryland 21005-5066
*/
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif
/*
 *			E R R O R
 *
 *  Ray Tracing library and Framebuffer library, error handling routines.
 *
 *  Functions -
 *	rt_bomb		Called upon fatal RT library error.
 *	rt_log		Called to log RT library events.
 *	fb_log		Called to log FB library events.
 *
 *	Idea originated by Mike John Muuss
 */

#include <stdio.h>
#if __STDC__
#include <stdarg.h>
#else
#include <varargs.h>
#endif
#include "./Sc.h"
#include "./extern.h"

/*
 *  		R T _ B O M B
 *  
 *  Abort the RT library
 */
void
rt_bomb(str)
char *str;
	{
	rt_log( "%s (librt.a) : Fatal error, aborting!\n", str );
	(void) fflush( stdout );
	(void) abort();			  /* Should dump.		*/
	exit(12);
	}

/*
 *  		R T _  L O G
 *  
 *  Log an RT library event
 */
/* VARARGS */
void
#if __STDC__
rt_log( char *fmt, ... )
#else
rt_log( va_alist )
va_dcl
#endif
	{
#if ! __STDC__
	register char *fmt; /* picked up by va_arg() */
#endif
	va_list	ap;
#if __STDC__
	va_start( ap, fmt );
#else
	va_start( ap );
	fmt  = va_arg( ap, char * );
#endif
	if( tty && (errfile[0] == '\0' || ! strcmp( errfile, "/dev/tty" )) )
		{
		TcClrTabs( HmTtyFd );
		if( ScDL != NULL )
			{
			(void) ScMvCursor( 1, SCROLL_TOP );
			(void) ScDeleteLn();
			(void) ScMvCursor( 1, SCROLL_BTM );
			(void) ScClrEOL();
			(void) vprintf( fmt, ap );
			}			
		else
		if( ScSetScrlReg( SCROLL_TOP, SCROLL_BTM+1 ) )
			{	char buf[LNBUFSZ];
				char *p;
			(void) ScMvCursor( 1, SCROLL_BTM+1 );
			(void) ScClrEOL();
			/* Work around for problem with vprintf(): it doesn't
				cause the screen to scroll, don't know why. */
			(void) vsprintf( buf, fmt, ap );
			/* Newline will cause double scroll. */
			p = buf+strlen(buf)-1;
			if( *p == '\n' )
				*p = '\0'; /* clobber newline */
			(void) puts( buf );
			/*(void) vprintf( fmt, ap );*/
			(void) ScMvCursor( 1, SCROLL_BTM+1 );
			(void) ScClrScrlReg();
			}
		else
			{
			(void) fprintf( stderr,
					"%s %s %s %s!\n",
					"This terminal has no delete line",
					"or scrolling region capability,",
					"please dump it somewhere and get",
					"a real terminal"
					);
			exit( 1 );
			}
		(void) fflush( stdout );
		}
	else
		{
		(void) vfprintf( stderr, fmt, ap );
		}
	va_end( ap );
	}

/*
 *		F B _ L O G
 *  
 *  Log an FB library event
 */
/*VARARGS*/
void
#if __STDC__
fb_log(char *fmt, ...)
#else
fb_log( va_alist )
va_dcl
#endif
	{
#if ! __STDC__
	register char *fmt;
#endif
	va_list ap;
#if __STDC__
	va_start( ap, fmt );
#else
	va_start( ap );
	fmt = va_arg( ap, char * );
#endif
	(void) rt_log( fmt, ap );
	va_end( ap );
	}
