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
#include <stdio.h>
#include "machine.h"
#include "vmath.h"
#include "raytrace.h"
#include "./lgt.h"
#include "./tree.h"
#include "./screen.h"
#include "./extern.h"
static char	*usage[] =
	{
"",
"l g t (2.3)",
"",
"Usage:",
"",
"lgt [-IOTovw file][-AGKacefginps n][-b \"R G B\"][-dtD \"x y\"][-xy \"a b\"] file.g object...",
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
"A factor             anti-aliasing thru over-sampling by factor",
"a roll               specify roll (angle around viewing axis) rotation to grid",
"B                    submit batch run using current context",
"b R G B              specify background-color",
"C                    use cursor input module",
"c [flag]             enable or disable tracking cursor",
"D x y                translate image when raytracing (WRT viewport)",
"E                    clear display",
"e bitmask            set debug flag (hexidecimal bitmask)",
"F                    animate",
"f distance           specify distance from origin of grid to model centroid",
"G size               grid resolution (# of rays along edge of square grid)",
"g fov                field of view (1.0 = entire model)",
"H [file]             save frame buffer image",
"h [file]             read frame buffer image",
"J                    make a movie (prompts for parameters)",
"j [file]             input key-frame from file (as output by mged(1B))",
"K bounces            maximum level of recursion in raytracing",
"L id                 modify light source entry for id (0 to 10)",
"l id                 print light source entry for id (0 to 10)",
"M id                 modify material data base entry for id (0 to 99)",
"m id                 print material data base entry for id (0 to 99)",
"n [processors]       number of processors to use (parallel environment)",
"O [file]             re-direct errors to specified output file",
"o [file]             image (output file for picture)",
"p factor             adjust perspective (0.0 to infinity)",
"q or ^D              quit",
"R                    raytrace (generate image) within current rectangle",
"r                    redraw screen",
"S [file]             script (save current option settings in file) ",
"s                    enter infrared module",
"T [file]             read texture map file",
"t x y                translate grid when raytracing (WRT model)",
"V [file]             write light source data base",
"v [file]             read light source data base",
"W [file]             write material attribute data base",
"w [file]             read material attribute data base",
"x start [finish]     set left and right border of current rectangle",
"y start [finish]     set top and bottom border of current rectangle",
"z ulen vlen [width]  size of texture map (plus width of padded lines)",
"?                    print this menu",
"! [arg(s)]           feed arg(s) to /bin/sh or $SHELL if set",
". [flag]             set buffered pixel I/O flag",
"# [anything]         comment or NOP (useful in preparing input files)",
	NULL
	};
static char	*ir_menu[] =
	{
"",
"                       Infrared Module: local commands",
"",
"d x y                specify automatic IR mapping offsets",
"I [file]             read and display IR data",
"i noise              specify noise threshold for IR data",
"N [temperature]      specify temperature for IR painting",
"P                    print GED regions and associated IR mappings",
"Q                    enter temperature for GED region or group",
"s                    exit IR module",
"U [file]             write IR data base file",
"u [file]             read IR data base file",
"Z                    display pseudo-color IR mapping scale",
	NULL
	};

/*	p r n t _ S t a t u s ( )					*/
void
prnt_Status()
	{
	if( ir_mapping )
		prnt_IR_Status();
	else
		prnt_Lgt_Status();
	return;
	}

/*	p r n t _ L g t _ S t a t u s ( )				*/
void
prnt_Lgt_Status()
	{
	TITLE_MOVE();
	(void) fputs( title, stdout );
	TIMER_MOVE();
	(void) fputs( timer, stdout );
	F_SCRIPT_MOVE();
	(void) fputs( script_file, stdout );
	FIELD_OF_VU_MOVE();
	(void) printf( "%6.4f", grid_scale );
	if( err_file[0] != '\0' )
		{
		F_ERRORS_MOVE();
		(void) printf( "%-32s", err_file );
		}
	GRID_DIS_MOVE();
	(void) printf( "%11.4f", grid_dist );
	if( mat_db_file[0] != '\0' )
		{
		F_MAT_DB_MOVE();
		(void) printf( "%-32s", mat_db_file );
		}
	GRID_XOF_MOVE();
	(void) printf( "%11.4f", x_grid_offset );
	if( lgt_db_file[0] != '\0' )
		{
		F_LGT_DB_MOVE();
		(void) printf( "%-32s", lgt_db_file );
		}
	GRID_YOF_MOVE();
	(void) printf( "%11.4f", y_grid_offset );
	if( fb_file[0] != '\0' )
		{
		F_RASTER_MOVE();
		(void) printf( "%-32s", fb_file );
		}
	MODEL_RA_MOVE();
	(void) printf( "%11.4f", modl_radius );
	F_TEXTURE_MOVE();
	(void) printf( "%-32s", texture_file );
	BACKGROU_MOVE();
	(void) printf( "%3d %3d %3d", background[0], background[1], background[2] );
	BUFFERED_MOVE();
	(void) printf( "%4s",	pix_buffered == B_PAGE ? "PAGE" :
				pix_buffered == B_PIO ? "PIO" :
				pix_buffered == B_LINE ? "LINE" : "?"
				);
	DEBUGGER_MOVE();
	(void) printf( "0x%06x", rt_g.debug );
	MAX_BOUN_MOVE();
	(void) printf( "%-2d", max_bounce );
	(void) SetStandout();
	PROGRAM_NM_MOVE();
	(void) printf( " LGT (%s) ",  *version == '%' ? "EXP" : version );
	if( ged_file != NULL )
		{
		F_GED_DB_MOVE();
		(void) printf( " %s ", ged_file );
		}
	GRID_PIX_MOVE();
	(void) printf( " [%04d-", grid_x_org );
	GRID_SIZ_MOVE();
	(void) printf( "%04d,", grid_x_fin );
	GRID_SCN_MOVE();
	(void) printf( "%04d-", grid_y_org );
	GRID_FIN_MOVE();
	(void) printf( "%04d] ", grid_y_fin );
	(void) ClrStandout();
	return;
	}

/*	p r n t _ I R _ S t a t u s ( )					*/
void
prnt_IR_Status()
	{
	TITLE_MOVE();
	(void) fputs( title, stdout );
	TIMER_MOVE();
	(void) fputs( timer, stdout );
	F_SCRIPT_MOVE();
	(void) fputs( script_file, stdout );
	FIELD_OF_VU_MOVE();
	(void) printf( "%6.4f", grid_scale );
	if( err_file[0] != '\0' )
		{
		F_ERRORS_MOVE();
		(void) printf( "%-32s", err_file );
		}
	GRID_DIS_MOVE();
	(void) printf( "%11.4f", grid_dist );
	if( ir_file[0] != '\0' )
		{
		F_MAT_DB_MOVE();
		(void) printf( "%-32s", ir_file );
		}
	GRID_XOF_MOVE();
	(void) printf( "%11.4f", x_grid_offset );
	if( lgt_db_file[0] != '\0' )
		{
		F_LGT_DB_MOVE();
		(void) printf( "%-32s", lgt_db_file );
		}
	GRID_YOF_MOVE();
	(void) printf( "%11.4f", y_grid_offset );
	if( fb_file[0] != '\0' )
		{
		F_RASTER_MOVE();
		(void) printf( "%-32s", fb_file );
		}
	MODEL_RA_MOVE();
	(void) printf( "%11.4f", modl_radius );
	BACKGROU_MOVE();
	(void) printf( "%3d %3d %3d", background[0], background[1], background[2] );
	BUFFERED_MOVE();
	(void) printf( "%4s",	pix_buffered == B_PAGE ? "PAGE" :
				pix_buffered == B_PIO ? "PIO" :
				pix_buffered == B_LINE ? "LINE" : "?"
				);
	DEBUGGER_MOVE();
	(void) printf( "0x%06x", rt_g.debug );
	IR_AUTO_MAP_MOVE();
	(void) printf( "%4s", 	ir_offset == 1 ? "ON" : "OFF" );
	IR_PAINT_MOVE();
	if( ir_paint_flag )
		(void) printf( "%4d", ir_paint );
	else
		(void) printf( "none" );
	(void) SetStandout();
	PROGRAM_NM_MOVE();
	(void) printf(	" LGT (%s) IR Module [%s] ",
			*version == '%' ? "EXP" : version,
			ir_mapping & IR_READONLY ? "readonly" : "editmode"
			);
	GRID_PIX_MOVE();
	(void) printf( " [%04d-", grid_x_org );
	GRID_SIZ_MOVE();
	(void) printf( "%04d,", grid_x_fin );
	GRID_SCN_MOVE();
	(void) printf( "%04d-", grid_y_org );
	GRID_FIN_MOVE();
	(void) printf( "%04d] ", grid_y_fin );
	(void) ClrStandout();
	return;
	}

/*	p r n t _ M e n u ( )						*/
void
prnt_Menu()
	{	register char	**p;
	for( p = lgt_menu; *p != NULL; p++ )
		prnt_Scroll( "%-*s\n", co, *p );
	if( ir_mapping )
		for( p = ir_menu; *p != NULL; p++ )
			prnt_Scroll( "%-*s\n", co, *p );
	(void) fflush( stdout );
	return;
	}

/*	p r n t _ P r o m p t ( )					*/
void
prnt_Prompt( prompt )
char	*prompt;
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
prnt_Timer( s )
char	*s;
	{
	(void) rt_read_timer( timer, TIMER_LEN-1 );
	if( tty )
		{	register int	nblanks;
		nblanks = (TIMER_LEN-1) - strlen( timer );
		TIMER_MOVE();
		(void) fputs( timer, stdout );
		while( nblanks-- > 0 )
			(void) putc( ' ', stdout );
		}
	else
		rt_log( "(%s) %s\n", s == NULL ? "(null)" : s, timer );
	return;
	}

/*	p r n t _ E v e n t ( )						*/
void

prnt_Event( s )
char	*s;
	{
	if( ! tty )
		return;
	EVENT_MOVE();
	(void) ClrEOL();
	if( s != NULL )
		(void) fputs( s, stdout );
	IDLE_MOVE();
	(void) fflush( stdout );
	return;
	}

/*	p r n t _ T i t l e ( )						*/
void
prnt_Title( s )
char	*s;
	{
	if( tty && ! rt_g.debug )
		{
		TITLE_MOVE();
		(void) fputs( s, stdout );
		}
	else
		rt_log( "%s\n", s == NULL ? "(null)" : s );
	return;
	}

/*	p r n t _ U s a g e ( )
	Print usage message.
 */
void
prnt_Usage()
	{	register char	**p = usage;
	while( *p != NULL )
		(void) fprintf( stderr, "%s\n", *p++ );
	return;
	}

#include <varargs.h>
/*	p r n t _ S c r o l l ( )					*/
/* VARARGS */
void
prnt_Scroll( fmt, va_alist )
char	*fmt;
va_dcl
	{	va_list		ap;
	/* We use the same lock as malloc.  Sys-call or mem lock, really */
	RES_ACQUIRE( &rt_g.res_malloc );		/* lock */
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
		}
	va_end( ap );
	RES_RELEASE( &rt_g.res_malloc );		/* unlock */
	return;
	}
