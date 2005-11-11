/*                          P R N T . C
 * BRL-CAD
 *
 * Copyright (C) 2004-2005 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this file; see the file named COPYING for more
 * information.
 */
/** @file prnt.c
	Author:		Gary S. Moss
			U. S. Army Ballistic Research Laboratory
			Aberdeen Proving Ground
			Maryland 21005-5066
			(301)278-6647 or AV-298-6647
*/
#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "common.h"



#include <stdio.h>
#include <math.h>
#include <assert.h>
#if defined(HAVE_STDARG_H)
#include <stdarg.h>
#elif defined(HAVE_VARARGS_H)
#include <varargs.h>
#endif

#include "machine.h"
#include "vmath.h"
#include "raytrace.h"
#include "fb.h"
#include "./hmenu.h"
#include "./lgt.h"
#include "./extern.h"
#include "./vecmath.h"
#include "./tree.h"
#include "./screen.h"
#include "./ascii.h"

static char	*usage[] =
	{
"",
"Usage:",
"",
"lgt [-IOjovw file][-AGKTXacefiknps n][-G \"s c g v\"][-b \"R G B\"][-dtD \"x y\"][-xy \"a b\"] file.g object...",
"",
"The options may appear in any order; however, their parameters must",
"be present, are positional, and if there is more than one parameter",
"for an option, they must be supplied as a single argument (e.g.,",
"inside double-quotes as shown).",
"",
0
	};
static char	*lgt_menu[] =
	{
"                BRL Lighting Model (LGT) : global command set",
"",
"A factor             anti-aliasing thru over-sampling by factor (i.e. 2)",
"a roll               specify roll (angle around viewing axis) rotation to grid",
"B                    submit batch run using current context",
"b R G B              specify background-color",
"C                    use cursor input module",
"c flag               enable or disable tracking cursor",
"D x y                translate image when raytracing (WRT viewport)",
"E                    clear display",
"e bitmask            set debug flag (hexidecimal bitmask)",
"F                    animate",
"f distance           specify distance from origin of grid to model centroid",
"G size cflag gflag view_size    grid configuration",
"        if cflag == 0, size refers to no. of rays across image (default)",
"        otherwise it refers to cell size (ray separation) in millimeters.",
"        if gflag == 0, grid origin will be aligned WRT model RPP (default)",
"        otherwise it will be aligned WRT model origin",
"        if viewsize > 0.0 the field of view will be set accordingly",
"        otherwise it will be set relative to the model RPP (default)",
"H file               save frame buffer image",
"h file               read frame buffer image",
"J                    make a movie (prompts for parameters)",
"j file               input key-frame from file (as output by mged(1))",
"K bounces            maximum level of recursion in raytracing",
"k flag               enable or disable hidden line drawing",
"        if flag == 0, a lighting model image is produced (default)",
"        otherwise a hidden line drawing is produced",
"        if flag == 2, a reverse video (white-on-black) drawing is generated",
"L id                 modify light source entry id (0 to 10)",
"l id                 print light source entry id (0 to 10) or all",
"M id                 modify material data base entry id (0 to 99)",
"m id                 print material data base entry id (0 to 99) or all",
"n processors         number of processors to use (parallel environment)",
"O file               re-direct errors to specified output file",
"o file               image (output file for picture)",
"p factor             adjust perspective (0.0 to infinity)",
"q or ^D              quit",
"R                    raytrace (generate image) within current rectangle",
"r                    redraw screen",
"S file               script (save current option settings in file)",
"s                    enter infrared module",
"T size               size of frame buffer display (pixels across)",
"        By default the display window fits the grid size.  If zooming",
"        is desired, size should be a multiple of the grid size. To turn",
"        off manual sizing, set size to zero.",
"t x y                translate grid when raytracing (WRT model)",
"V file               write light source data base",
"v file               read light source data base",
"W file               write material attribute data base",
"w file               read material attribute data base",
"X flag               enable or disable reporting of overlaps",
"x start finish       set left and right border of current rectangle",
"y start finish       set bottom and top border of current rectangle",
"z ulen vlen width    size of texture map (plus width of padded lines)",
"?                    print this menu",
"! arg(s)             feed arg(s) to /bin/sh or $SHELL if set",
". flag               set buffered pixel I/O flag",
"# anything           comment or NOP (useful in preparing input files)",
	NULL
	};
static char	*ir_menu[] =
	{
"",
"                       Infrared Module: local commands",
"",
"d x y                specify automatic IR mapping offsets",
"I file               read and display IR data",
"i noise              specify noise threshold for IR data",
"N temperature        specify temperature for IR painting",
"P                    print GED regions and associated IR mappings",
"Q                    enter temperature for GED region or group",
"s                    exit IR module",
"U file               write IR data base file",
"u file               read IR data base file",
"Z                    display pseudo-color IR mapping scale",
	NULL
	};

char	screen[TOP_SCROLL_WIN+1][TEMPLATE_COLS+1];

/*	pad_Strcpy -- WARNING: this routine does NOT nul-terminate the
	destination buffer, but pads it with blanks.
 */
static void
pad_Strcpy(register char *des, register char *src, register int len)
{
	while( len > 0 && *src != '\0' )
		{
		*des++ = *src++;
		len--;
		}
	while( len-- > 0 )
		*des++ = ' ';
	return;
	}

/*	i n i t _ S t a t u s ( )					*/
void
init_Status(void)
{	register int	row, col;
	for( row = 0; row <= TOP_SCROLL_WIN; row++ )
		for( col = 0; col <= TEMPLATE_COLS; col++ )
			screen[row][col] = '\0';
	return;
	}

/*	p r n t _ S t a t u s ( )					*/
void
prnt_Status(void)
{	static char	scratchbuf[TEMPLATE_COLS+1];
	pad_Strcpy( TITLE_PTR, title, TITLE_LEN - 1 );
	pad_Strcpy( TIMER_PTR, timer, TIMER_LEN - 1 );
	pad_Strcpy( F_SCRIPT_PTR, script_file, 32 );
	(void) sprintf( scratchbuf, "%11.4f", view_size );
	(void) strncpy( VU_SIZE_PTR, scratchbuf, strlen( scratchbuf ) );
	pad_Strcpy( F_ERRORS_PTR, err_file, 32 );
	(void) sprintf( scratchbuf, "%11.4f", grid_dist );
	(void) strncpy( GRID_DIS_PTR, scratchbuf, strlen( scratchbuf ) );
	pad_Strcpy( F_MAT_DB_PTR, mat_db_file, 32 );
	(void) sprintf( scratchbuf, "%11.4f", x_grid_offset );
	(void) strncpy( GRID_XOF_PTR, scratchbuf, strlen( scratchbuf ) );
	pad_Strcpy( F_LGT_DB_PTR, lgt_db_file, 32 );
	(void) sprintf( scratchbuf, "%11.4f", y_grid_offset );
	(void) strncpy( GRID_YOF_PTR, scratchbuf, strlen( scratchbuf ) );
	pad_Strcpy( F_RASTER_PTR, fb_file, 32 );
	(void) sprintf( scratchbuf, "%11.4f", modl_radius );
	(void) strncpy( MODEL_RA_PTR, scratchbuf, strlen( scratchbuf ) );
	(void) sprintf( scratchbuf, "%3d %3d %3d",
			background[0], background[1], background[2] );
	(void) strncpy( BACKGROU_PTR, scratchbuf, strlen( scratchbuf ) );
	(void) sprintf( scratchbuf,
			"%4s",	pix_buffered == B_PAGE ? "PAGE" :
				pix_buffered == B_PIO ? "PIO" :
				pix_buffered == B_LINE ? "LINE" : "?"
				);
	(void) strncpy( BUFFERED_PTR, scratchbuf, strlen( scratchbuf ) );
	(void) sprintf( scratchbuf, "0x%06x", RT_G_DEBUG );
	(void) strncpy( DEBUGGER_PTR, scratchbuf, strlen( scratchbuf ) );
	(void) sprintf( scratchbuf, "%-2d", max_bounce );
	(void) strncpy( MAX_BOUN_PTR, scratchbuf, strlen( scratchbuf ) );
	(void) sprintf( scratchbuf, " LGT %s", version );
	(void) strncpy( PROGRAM_NM_PTR, scratchbuf, strlen( scratchbuf ) );
	(void) sprintf( scratchbuf, " %s ",
			ged_file == NULL ? "(null)" : ged_file );
	(void) strncpy( F_GED_DB_PTR, scratchbuf,
			Min( strlen( scratchbuf ), 26 ) );
	(void) sprintf( scratchbuf, " [%04d-", grid_x_org );
	(void) strncpy( GRID_PIX_PTR, scratchbuf, strlen( scratchbuf ) );
	(void) sprintf( scratchbuf, "%04d,", grid_x_fin );
	(void) strncpy( GRID_SIZ_PTR, scratchbuf, strlen( scratchbuf ) );
	(void) sprintf( scratchbuf, "%04d-", grid_y_org );
	(void) strncpy( GRID_SCN_PTR, scratchbuf, strlen( scratchbuf ) );
	(void) sprintf( scratchbuf, "%04d:", grid_y_fin );
	(void) strncpy( GRID_FIN_PTR, scratchbuf, strlen( scratchbuf ) );
	(void) sprintf( scratchbuf, "%04d] ", frame_no );
	(void) strncpy( FRAME_NO_PTR, scratchbuf, strlen( scratchbuf ) );
	update_Screen();
	return;
	}

void
update_Screen(void)
{	register int	tem_co, row, col;
	tem_co = Min( co, TEMPLATE_COLS );
	for( row = 0; template[row][0] != '\0'; row++ )
		{	register int	lastcol = -2;
		if( template[row+1] == NULL )
			(void) SetStandout();
		for( col = 0; col < tem_co; col++ )
			if( screen[row][col] != template[row][col] )
				{
				if( col != lastcol+1 )
					MvCursor( col+1, row+1 );
				lastcol = col;
				(void) putchar( template[row][col] );
				screen[row][col] = template[row][col];
				}
		}
	(void) ClrStandout();
	EVENT_MOVE();
	(void) fflush( stdout );
	return;
	}

/*	p r n t _ P a g e d _ M e n u ( )				*/
void
prnt_Paged_Menu(register char **menu)
{	register int	done = FALSE;
		int		lines =	(PROMPT_LINE-TOP_SCROLL_WIN);
	if( ! tty )
		{
		for( ; *menu != NULL; menu++ )
			bu_log( "%s\n", *menu );
		return;
		}
	for( ; *menu != NULL && ! done;  )
		{
		for( ; lines > 0 && *menu != NULL; menu++, --lines )
			prnt_Scroll( "%-*s\n", co, *menu );
		if( *menu != NULL )
			done = ! do_More( &lines );
		prnt_Prompt( "" );
		}
	(void) fflush( stdout );
	return;
	}

int
do_More(int *linesp)
{	register int	ret = TRUE;
	if( ! tty )
		return	TRUE;
	save_Tty( 0 );
	set_Raw( 0 );
	clr_Echo( 0 );
	SetStandout();
	prnt_Prompt( "More ? [n|<return>|<space>] " );
	ClrStandout();
	(void) fflush( stdout );
	switch( hm_getchar() )
		{
	case 'Q' :
	case 'q' :
	case 'N' :
	case 'n' :
		ret = FALSE;
		break;
	case LF :
	case CR :
		*linesp = 1;
		break;
	default :
		*linesp = (PROMPT_LINE-TOP_SCROLL_WIN);
		break;
		}
	reset_Tty( 0 );
	return	ret;
	}


/*	p r n t _ M e n u ( )						*/
void
prnt_Menu(void)
{
	prnt_Paged_Menu( lgt_menu );
	if( ir_mapping )
		prnt_Paged_Menu( ir_menu );
	hmredraw();
	return;
	}

/*	p r n t _ P r o m p t ( )					*/
void
prnt_Prompt(char *prompt)
{
	if( tty )
		{
		PROMPT_MOVE();
		(void) ClrEOL();
		(void) SetStandout();
		(void) printf( "%s", prompt );
		(void) ClrStandout();
		(void) fflush( stdout );
		}
	return;
	}

/*	p r n t _ T i m e r ( )						*/
void
prnt_Timer(char *eventstr)
{
	(void) rt_read_timer( timer, TIMER_LEN-1 );
	if( tty )
		{
		pad_Strcpy( TIMER_PTR, timer, TIMER_LEN-1 );
		update_Screen();
		}
	else
		bu_log( "(%s) %s\n", eventstr == NULL ? "(null)" : eventstr, timer );
	return;
	}

/*	p r n t _ E v e n t ( )						*/
void
prnt_Event(char *s)
{	static int	lastlen = 0;
		register int	i;
	if( ! tty )
		return;
	EVENT_MOVE();
	if( s != NULL )
		{	register int len = strlen( s );
		(void) fputs( s, stdout );
		/* Erase last message. */
		for( i = len; i < lastlen; i++ )
			(void) putchar( ' ' );
		lastlen = len;
		}
	else
		{
		/* Erase last message. */
		for( i = 0; i < lastlen; i++ )
			(void) putchar( ' ' );
		lastlen = 0;
		}
	IDLE_MOVE();
	(void) fflush( stdout );
	return;
	}

/*	p r n t _ T i t l e ( )						*/
void
prnt_Title(char *titleptr)
{
	if( ! tty || RT_G_DEBUG )
		bu_log( "%s\n", titleptr == NULL ? "(null)" : titleptr );
	return;
	}

/*	p r n t _ U s a g e ( )
	Print usage message.
 */
void
prnt_Usage(void)
{	register char	**p = usage;
	while( *p != NULL )
		(void) fprintf( stderr, "%s\n", *p++ );
	return;
	}

#if defined(HAVE_STDARG_H)
/*	p r n t _ S c r o l l ( )					*/
/* VARARGS */
void
prnt_Scroll( char *fmt, ... )
	{	va_list		ap;
	/* We use the same lock as malloc.  Sys-call or mem lock, really */
	bu_semaphore_acquire( BU_SEM_SYSCALL );		/* lock */
	va_start( ap, fmt );
	if( tty )
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
			(void)vfprintf( stdout, fmt, ap );
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
			(void)vfprintf( stdout, fmt, ap );
			}
		else
			(void)vfprintf( stdout, fmt, ap );
		/* End of line detected by existance of a newline.	*/
		newline = fmt[strlen( fmt )-1] == '\n';
		hmredraw();
		}
	else
		(void)vfprintf( stderr, fmt, ap );
	va_end( ap );
	bu_semaphore_release( BU_SEM_SYSCALL );		/* unlock */
	return;
	}

#elif !defined(HAVE_VARARGS_H)

void
prnt_Scroll(fmt, a,b,c,d,e,f,g,h,i)
char *fmt;
{
	bu_semaphore_acquire( BU_SEM_SYSCALL );		/* lock */
	if( tty )
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
		/* End of line detected by existance of a newline.	*/
		newline = fmt[strlen( fmt )-1] == '\n';
		hmredraw();
		}
	else
		(void) fprintf( stdout, fmt, a,b,c,d,e,f,g,h,i );
	bu_semaphore_release( BU_SEM_SYSCALL );		/* unlock */
}
#else
/*	p r n t _ S c r o l l ( )					*/
/* VARARGS */
void
prnt_Scroll( fmt, va_alist )
char	*fmt;
va_dcl
	{	va_list		ap;
	/* We use the same lock as malloc.  Sys-call or mem lock, really */
	bu_semaphore_acquire( BU_SEM_SYSCALL );		/* lock */
	va_start( ap );
	if( tty )
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
		/* End of line detected by existance of a newline.	*/
		newline = fmt[strlen( fmt )-1] == '\n';
		hmredraw();
		}
	else
		(void) _doprnt( fmt, ap, stderr );
	va_end( ap );
	bu_semaphore_release( BU_SEM_SYSCALL );		/* unlock */
	return;
	}
#endif /* HAVE_STDARG_H */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
