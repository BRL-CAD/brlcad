/*
	Author:		Gary S. Moss
			U. S. Army Ballistic Research Laboratory
			Aberdeen Proving Ground
			Maryland 21005-5066
			(301)278-6651 or AV-298-6651
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
#include <varargs.h>
#include <Sc/Sc.h>
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
rt_log( fmt, va_alist )
char	*fmt;
va_dcl
	{	va_list		ap;
	va_start( ap );
	if( tty && (errfile[0] == '\0' || ! strcmp( errfile, "/dev/tty" )) )
		{
		if( ScSetScrlReg( SCROLL_TOP, SCROLL_BTM ) )
			{
			(void) ScMvCursor( 1, SCROLL_BTM );
			(void) ScClrEOL();
			(void) _doprnt( fmt, ap, stdout );
			(void) ScClrScrlReg();
			}
		else
		if( ScDL != NULL )
			{
			(void) ScMvCursor( 1, SCROLL_TOP );
			(void) ScDeleteLn();
			(void) ScMvCursor( 1, SCROLL_BTM );
			(void) ScClrEOL();
			(void) _doprnt( fmt, ap, stdout );
			}
		else
			{
			(void) _doprnt( fmt, ap, stdout );
			}
		(void) fflush( stdout );
		}
	else
		(void) _doprnt( fmt, ap, stderr );
	va_end( ap );
	return;
	}

/*
 *		F B _ L O G
 *  
 *  Log an FB library event
 */
/* VARARGS */
void
fb_log( fmt, va_alist )
char	*fmt;
va_dcl
	{
	(void) rt_log( fmt, va_alist );
	return;
	}
