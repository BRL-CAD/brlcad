/*                          P R N T . C
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
/** @file fbed/prnt.c
 *	Author:		Gary S. Moss
 */

#include "common.h"

#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#include "fb.h"

#include "./std.h"
#include "./ascii.h"
#include "./try.h"
#include "./extern.h"


#define PIXEL_MOVE()		MvCursor( 7, 1 )
#define PAINT_MOVE()		MvCursor( 26, 1 )
#define STRIDE_MOVE()		MvCursor( 46, 1 )
#define BRUSH_SIZE_MOVE()	MvCursor( 63, 1 )
#define CURSOR_POS_MOVE()	MvCursor( 15, 2 )
#define PROMPT_MOVE()		MvCursor( 1, PROMPT_LINE )
#define ERROR_MOVE()		MvCursor( 1, PROMPT_LINE + 1 )
#define SCROLL_PR_MOVE()	MvCursor( 1, PROMPT_LINE - 1 )
#define SCROLL_DL_MOVE()	MvCursor( 1, BOTTOM_STATUS_AREA + 1 )

static char *usage[] =
{
    "",
    "fbed (2.1)",
    "",
    "Usage: fbed [-hp]",
    "",
    "options : -h   use high resolution display",
    "          -p   open the GTCO bit pad",
    0
};

void init_Status(void);
void prnt_Status(void), prnt_Usage(void);
void prnt_Prompt(const char *msg);
void prnt_Macro(char *bufp);
void prnt_Rect2D(const char *str, Rect2D *rectp);
/**void prnt_FBC();**/

/*	p r n t _ S t a t u s ( ) */
void
prnt_Status(void)
{
    RGBpixel pixel;
    if ( ! tty )
	return;
    fb_Get_Pixel( pixel );
    PIXEL_MOVE();
    (void) printf( "%3d %3d %3d", pixel[RED], pixel[GRN], pixel[BLU] );
    PAINT_MOVE();
    (void) printf( "%3d %3d %3d", paint[RED], paint[GRN], paint[BLU] );
    STRIDE_MOVE();
    (void) printf( "%4d", gain );
    BRUSH_SIZE_MOVE();
    (void) printf( "%4d", brush_sz );
    CURSOR_POS_MOVE();
    (void) SetStandout();
    (void) printf( " line [%4d] column [%4d] ", cursor_pos.p_y, cursor_pos.p_x );
    (void) ClrStandout();
    PROMPT_MOVE();
    (void) fflush( stdout );
    return;
}

static char *screen_template[] = {
/*        1         2         3         4         5         6         7         8
	  012345678901234567890123456789012345678901234567890123456789012345678901234567890
*/
    "Pixel[           ] Paint[           ] Stride[    ] Brush Size[    ]",
    "-- FBED 2.1 -------------------------------------------------------------------",
    0
};

/*	i n i t _ S t a t u s ( ) */
void
init_Status(void)
{
    char **p = screen_template;
    int template_co;
    char buf[MAX_LN];
    template_co = Min( CO, MAX_LN );
    if ( ! tty )
	return;
    (void) ClrText();
    (void) HmCursor();
    while ( *(p+1) )
    {
	bu_strlcpy( buf, *p++, (size_t)template_co );

	(void) printf( "%s\n\r", buf );
    }
    /* Last line is reverse-video if possible. */
    (void) SetStandout();
    bu_strlcpy( buf, *p++, (size_t)template_co );

    (void) printf( "%s\n\r", buf );
    (void) ClrStandout();
    (void) fflush( stdout );
    return;
}

/*	p r n t _ U s a g e ( )
	Print usage message.
*/
void
prnt_Usage(void)
{
    char **p = usage;
    while ( *p )
	(void) fprintf( stderr, "%s\n", *p++ );
    return;
}


/*	p r n t _ P i x e l ( ) */
void
prnt_Pixel(char *msg, RGBpixel (*pixelp))
{
    prnt_Scroll(	"%s {%03d,%03d,%03d}",
			msg,
			pixelp[RED],
			pixelp[GRN],
			pixelp[BLU]
	);
    return;
}

#define Va_Decl( _func )	_func(const char *fmt, ...)
#define Va_Start()		va_list	ap; va_start( ap, fmt )
#define Va_End()		va_end( ap )
#define Va_Print( _p )		(void) vfprintf( _p, fmt, ap )

/* VARARGS */
void
fb_log (const char *fmt, ...)	/* de-macro'd due to fmt now being const */
{
    extern char *DL, *CS;
    Va_Start();
    if ( tty )
    {
	if ( DL != NULL )
	{
	    SCROLL_DL_MOVE();
	    DeleteLn();
	    SCROLL_PR_MOVE();
	    ClrEOL();
	    Va_Print( stdout );
	}
	else
	    if ( CS != NULL )
	    {
		/* This scrolling region stuff doesn't work
		   correctly (XXX). */
		SetScrlReg( TOP_SCROLL_WIN, PROMPT_LINE - 1 );
		SCROLL_PR_MOVE();
		ClrEOL();
		Va_Print( stdout );
		ResetScrlReg();
	    }
	    else
		Va_Print( stdout );
    }
    else
    {
	Va_Print( stdout );
	(void) printf( "\n" );
    }
    Va_End();
    (void) fflush( stdout );
    return;
}

/* VARARGS */
void
Va_Decl( prnt_Scroll )
{
    extern char *DL, *CS;
    Va_Start();
    if ( tty )
    {
	if ( DL != NULL )
	{
	    SCROLL_DL_MOVE();
	    DeleteLn();
	    SCROLL_PR_MOVE();
	    ClrEOL();
	    Va_Print( stdout );
	}
	else
	    if ( CS != NULL )
	    {
		/* This scrolling region stuff doesn't work
		   correctly (XXX). */
		SetScrlReg( TOP_SCROLL_WIN, PROMPT_LINE - 1 );
		SCROLL_PR_MOVE();
		ClrEOL();
		Va_Print( stdout );
		ResetScrlReg();
	    }
	    else
		Va_Print( stdout );
    }
    else
    {
	Va_Print( stdout );
	(void) printf( "\n" );
    }
    Va_End();
    return;
}

/*	p r n t _ D e b u g ( ) */
/* VARARGS */
void
Va_Decl( prnt_Debug )
{
    Va_Start();
    if ( tty )
    {
	ERROR_MOVE();
	ClrEOL();
	SetStandout();
	Va_Print( stdout );
	ClrStandout();
	(void) fflush( stdout );
    }
    else
    {
	Va_Print( stderr );
	(void) fprintf( stderr, "\n" );
    }
    Va_End();
    return;
}

/* VARARGS */
void
Va_Decl( prnt_Event )
{
    Va_Start();
    if ( tty )
    {
	ERROR_MOVE();
	ClrEOL();
	SetStandout();
	Va_Print( stdout );
	ClrStandout();
	(void) fflush( stdout );
    }
    else
    {
	Va_Print( stderr );
	(void) fprintf( stderr, "\n" );
    }
    Va_End();
    return;
}

#ifdef never
void
prnt_FBC()
{
    extern struct ik_fbc	ikfbcmem;
    prnt_Scroll(	"viewport:\t\tx [%4d]\ty [%4d]\n",
			ikfbcmem.fbc_xviewport,
			ikfbcmem.fbc_yviewport
	);
    prnt_Scroll(	"view size:\t\tx [%4d]\ty [%4d]\n",
			ikfbcmem.fbc_xsizeview,
			ikfbcmem.fbc_ysizeview
	);
    prnt_Scroll(	"window offsets:\t\tx [%4d]\ty [%4d]\n",
			ikfbcmem.fbc_xwindow,
			ikfbcmem.fbc_ywindow
	);
    prnt_Scroll(	"zoom factor:\t\tx [%4d]\ty [%4d]\n",
			ikfbcmem.fbc_xzoom,
			ikfbcmem.fbc_yzoom
	);
    prnt_Scroll(	"display rate cntrl:\th [%4d]\tv [%4d]\n",
			ikfbcmem.fbc_horiztime,
			ikfbcmem.fbc_nlines
	);
    prnt_Scroll(	"video control:\t\tL [%4d]\tH [%4d]\n",
			ikfbcmem.fbc_Lcontrol,
			ikfbcmem.fbc_Hcontrol
	);
    prnt_Scroll(	"cursor position:\tx [%4d]\ty [%4d]\n",
			ikfbcmem.fbc_xcursor,
			ikfbcmem.fbc_ycursor
	);
    return;
}
#endif

void
prnt_Prompt(const char *msg)
{
    PROMPT_MOVE();
    (void) ClrEOL();
    (void) printf( "%s", msg );
    (void) fflush( stdout );
    return;
}

void
prnt_Macro(char *bufp)
{
    char prnt_buf[BUFSIZ];
    char *p;
    for ( p = prnt_buf; *bufp != '\0'; bufp++ )
    {
	switch ( *bufp )
	{
	    case ESC :
		*p++ = 'M';
		*p++ = '-';
		break;
	    case CR :
		*p++ = '\\';
		*p++ = 'r';
		break;
	    case LF :
		*p++ = '\\';
		*p++ = 'n';
		break;
	    default :
		*p++ = *bufp;
		break;
	}
    }
    *p = NUL;
    prnt_Scroll( "Macro buffer \"%s\".", prnt_buf );
    return;
}

void
prnt_Rect2D(const char *str, Rect2D *rectp)
{
    prnt_Scroll(	"%s {<%d,%d>,<%d,%d>}\n",
			str,
			rectp->r_origin.p_x,
			rectp->r_origin.p_y,
			rectp->r_corner.p_x,
			rectp->r_corner.p_y
	);
    (void) fflush( stdout );
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
