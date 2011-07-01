/*                         E R R O R . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2011 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file lgt/error.c
 *
 *  Ray Tracing library and Framebuffer library, error handling routines.
 *
 *  Functions -
 *	bu_bomb		Called upon fatal RT library error.
 *	bu_log		Called to log RT library events.
 *	fb_log		Called to log FB library events.
 *
 *	Idea originated by Mike John Muuss
 *
 *	Author:		Gary S. Moss
 */

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <assert.h>

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
 *  Abort the LIBRT library
 */
int		bu_setjmp_valid = 0;	/* !0 = bu_jmpbuf is valid */
jmp_buf		bu_jmpbuf;		/* for BU_SETJMP() */

void
bu_bomb(const char *str)
{
    bu_log( "%s (librt.a) : Fatal error, aborting!\n", str );
    (void) fflush( stdout );
    prnt_Timer( "DUMP" );
    if ( pix_buffered == B_PAGE )
	(void) fb_flush( fbiop ); /* Write out buffered image.	*/
    (void) abort();			  /* Should dump.		*/
}

void
fb_log( const char *fmt, ... )
{
    va_list ap;
    /* We use the same lock as malloc.  Sys-call or mem lock, really */
    bu_semaphore_acquire( BU_SEM_SYSCALL );		/* lock */
    va_start( ap, fmt );
    if ( tty && (err_file[0] == '\0' || BU_STR_EQUAL( err_file, "/dev/tty" )) ) {
	/* Only move cursor and scroll if newline is output.	*/
	static int	newline = 1;
	if ( CS != NULL ) {
	    (void) SetScrlReg( TOP_SCROLL_WIN, PROMPT_LINE - 1 );
	    if ( newline ) {
		SCROLL_PR_MOVE();
		(void) ClrEOL();
	    }
	    (void) vfprintf( stdout, fmt, ap );
	    (void) ResetScrlReg();
	} else
	    if ( DL != NULL ) {
		if ( newline ) {
		    SCROLL_DL_MOVE();
		    (void) DeleteLn();
		    SCROLL_PR_MOVE();
		    (void) ClrEOL();
		}
		(void) vfprintf( stdout, fmt, ap );
	    } else
		(void) vfprintf( stdout, fmt, ap );
	(void) fflush( stdout );
	/* End of line detected by existance of a newline.	*/
	newline = fmt[strlen( fmt )-1] == '\n';
	hmredraw();
    } else {
	(void) vfprintf( stderr, fmt, ap );
	(void) fflush( stderr );
    }
    va_end( ap );
    bu_semaphore_release( BU_SEM_SYSCALL );		/* unlock */
    return;
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
