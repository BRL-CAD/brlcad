/*
	Author:		Gary S. Moss
			U. S. Army Ballistic Research Laboratory
			Aberdeen Proving Ground
			Maryland 21005-5066
			(301)278-6647 or AV-298-6647
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

#include "conf.h"

#include <stdio.h>
#if defined(HAVE_STDARG_H)
#  include <stdarg.h>
#else
#  if defined(HAVE_VARARGS_H)
#    include <varargs.h>
#  endif
#endif
#include <assert.h>

#include "machine.h"
#include "vmath.h"
#include "bu.h"
#include "raytrace.h"
#include "fb.h"
#include "./hmenu.h"
#include "./lgt.h"
#include "./extern.h"
#include "./screen.h"
/*
 *  		B U _ B O M B
 *  
 *  Abort the RT library
 */
void
bu_bomb(str)
CONST char *str;
	{
	rt_log( "%s (librt.a) : Fatal error, aborting!\n", str );
	(void) fflush( stdout );
	prnt_Timer( "DUMP" );
	if( pix_buffered == B_PAGE )
		(void) fb_flush( fbiop ); /* Write out buffered image.	*/
	(void) abort();			  /* Should dump.		*/
#if !__STDC__
	exit(12);
#endif
	}

#if defined(HAVE_STDARG_H)
void
fb_log( char *fmt, ... )
	{
	va_list ap;
	/* We use the same lock as malloc.  Sys-call or mem lock, really */
	bu_semaphore_acquire( BU_SEM_SYSCALL );		/* lock */
	va_start( ap, fmt );
	if( tty && (err_file[0] == '\0' || ! strcmp( err_file, "/dev/tty" )) )
		{ /* Only move cursor and scroll if newline is output.	*/
			static int	newline = 1;
		if( CS != NULL )
			{
			(void) SetScrlReg( TOP_SCROLL_WIN, PROMPT_LINE - 1 );
			if( newline )
				{
				SCROLL_PR_MOVE();
				(void) ClrEOL();
				}
			(void) vfprintf( stdout, fmt, ap );
			(void) ResetScrlReg();
			}
		else
		if( DL != NULL )
			{
			if( newline )
				{
				SCROLL_DL_MOVE();
				(void) DeleteLn();
				SCROLL_PR_MOVE();
				(void) ClrEOL();
				}
			(void) vfprintf( stdout, fmt, ap );
			}
		else
			(void) vfprintf( stdout, fmt, ap );
		(void) fflush( stdout );
		/* End of line detected by existance of a newline.	*/
		newline = fmt[strlen( fmt )-1] == '\n';
		hmredraw();
		}
	else
		{
		(void) vfprintf( stderr, fmt, ap );
		(void) fflush( stderr );
		}
	va_end( ap );
	bu_semaphore_release( BU_SEM_SYSCALL );		/* unlock */
	return;
	}
#else 

#if !defined( HAVE_VARARGS_H )
/* VARARGS */
void
fb_log(fmt, a,b,c,d,e,f,g,h,i)
char *fmt;
	{
	bu_semaphore_acquire( BU_SEM_SYSCALL );		/* lock */
	if( tty && (err_file[0] == '\0' || ! strcmp( err_file, "/dev/tty" )) )
		{ /* Only move cursor and scroll if newline is output.	*/
			static int	newline = 1;
		if( CS != NULL )
			{
			(void) SetScrlReg( TOP_SCROLL_WIN, PROMPT_LINE - 1 );
			if( newline )
				{
				SCROLL_PR_MOVE();
				(void) ClrEOL();
				}
			(void) fprintf( stdout, fmt, a,b,c,d,e,f,g,h,i );
			(void) ResetScrlReg();
			}
		else
		if( DL != NULL )
			{
			if( newline )
				{
				SCROLL_DL_MOVE();
				(void) DeleteLn();
				SCROLL_PR_MOVE();
				(void) ClrEOL();
				}
			(void) fprintf( stdout, fmt, a,b,c,d,e,f,g,h,i );
			}
		else
			(void) fprintf( stdout, fmt, a,b,c,d,e,f,g,h,i );
		(void) fflush( stdout );
		/* End of line detected by existance of a newline.	*/
		newline = fmt[strlen( fmt )-1] == '\n';
		hmredraw();
		}
	else
		(void) fprintf( stderr, fmt, a,b,c,d,e,f,g,h,i );
	bu_semaphore_release( BU_SEM_SYSCALL );		/* unlock */
	}
#else 

/*
 *  		R T _  L O G
 *  
 *  Log an RT library event
 */
/* VARARGS */
void
fb_log( fmt, va_alist )
char	*fmt;
va_dcl
	{	va_list		ap;
	/* We use the same lock as malloc.  Sys-call or mem lock, really */
	bu_semaphore_acquire( BU_SEM_SYSCALL );		/* lock */
	va_start( ap );
	if( tty && (err_file[0] == '\0' || ! strcmp( err_file, "/dev/tty" )) )
		{ /* Only move cursor and scroll if newline is output.	*/
			static int	newline = 1;
		if( CS != NULL )
			{
			(void) SetScrlReg( TOP_SCROLL_WIN, PROMPT_LINE - 1 );
			if( newline )
				{
				SCROLL_PR_MOVE();
				(void) ClrEOL();
				}
			(void) _doprnt( fmt, ap, stdout );
			(void) ResetScrlReg();
			}
		else
		if( DL != NULL )
			{
			if( newline )
				{
				SCROLL_DL_MOVE();
				(void) DeleteLn();
				SCROLL_PR_MOVE();
				(void) ClrEOL();
				}
			(void) _doprnt( fmt, ap, stdout );
			}
		else
			(void) _doprnt( fmt, ap, stdout );
		(void) fflush( stdout );
		/* End of line detected by existance of a newline.	*/
		newline = fmt[strlen( fmt )-1] == '\n';
		hmredraw();
		}
	else
		{
		(void) _doprnt( fmt, ap, stderr );
		(void) fflush( stderr );
		}
	va_end( ap );
	bu_semaphore_release( BU_SEM_SYSCALL );		/* unlock */
	return;
	}
#endif
#endif /* HAVE_STDARG_H */
