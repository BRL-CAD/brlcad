/*                    D O _ O P T I O N S . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2007 United States Government as represented by
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
/** @file do_options.c
	Author:		Gary S. Moss
			U. S. Army Ballistic Research Laboratory
			Aberdeen Proving Ground
			Maryland 21005-5066
*/
#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <math.h>
#include <signal.h>

#include "machine.h"
#include "vmath.h"
#include "raytrace.h"
#include "fb.h"

#include "./hmenu.h"
#include "./lgt.h"
#include "./extern.h"
#include "./mat_db.h"
#include "./vecmath.h"
#include "./ascii.h"
#include "./tree.h"
#include "./screen.h"
#include "./cursors.h"

#define SGI_USEKEYBD	(tty && sgi_usemouse && sgi_console)

#define MAX_ARGS	81
#define MAX_COLS	128
static char	*prog_id;
static char	**objects;
static char	svkey_file[MAX_LN] = { 0 };
static char	scratchbuf[TEMPLATE_COLS+1];

/* The strings in this array will be modified as the program runs,
 * so don't point to const strings, initialize as character arrays here.
 */
char	template[][TEMPLATE_COLS] = {
/*         1         2         3         4         5         6         7         8
 012345678901234567890123456789012345678901234567890123456789012345678901234567890*/
"TITLE [                                                                       ]",
"TIMER [                                                                       ]",
"FILES : script [                                ] GRID : x-trans. [           ]",
"      : errors [                                ]      : y-trans. [           ]",
"      : mat db [                                ]      : distance [           ]",
"      : lgt db [                                ]       view size [           ]",
"      : device [                                ]    model-radius [           ]",
"                                                       background [           ]",
"FLAGS : buffer [    ] debug [        ] max-bounce [   ]                        ",
"-------------------------------------------------------------------------------",
""
};

static int	noframes = 1;

static int	make_Script(char *file);
static int	f_Nop(HMitem *itemp, char **ar);		/* default */
static int	f_Exec_Shell(HMitem *itemp, char **args);		/* ! */
static int	f_Comment(HMitem *itemp, char **ar);		/* # */
static int	f_Menu(HMitem *itemp, char **ar);		/* ? */
static int	f_Buffer(HMitem *itemp, char **args);		/* . */
static int	f_Anti_Aliasing(HMitem *itemp, char **args);	/* A */
static int	f_Batch(HMitem *itemp, char **args);		/* B */
static int	f_Cursor_Module(HMitem *itemp, char **args);	/* C */
static int	f_Display_Origin(HMitem *itemp, char **args);	/* D */
static int	f_Fbclear(HMitem *itemp, char **args);		/* E */
static int	f_Animate(HMitem *itemp, char **args);		/* F */
static int	f_GridConfig(HMitem *itemp, char **args);		/* G */
static int	f_Wrt_Fb(HMitem *itemp, char **args);		/* H */
static int	f_Rd_Raw_IR(HMitem *itemp, char **args);		/* I */
static int	f_Movie(HMitem *itemp, char **args);		/* J */
static int	f_Max_Bounce(HMitem *itemp, char **args);		/* K */
static int	f_Entr_Lgt_Db(HMitem *itemp, char **args);	/* L */
static int	f_Entr_Mat_Db(HMitem *itemp, char **args);	/* M */
static int	f_Set_IR_Paint(HMitem *itemp, char **args);	/* N */
static int	f_Err_File(HMitem *itemp, char **args);		/* O */
static int	f_Prnt_Regions(HMitem *itemp, char **args);	/* P */
static int	f_Set_Region_IR(HMitem *itemp, char **args);	/* Q */
static int	f_Raytrace(HMitem *itemp, char **ar);		/* R */
static int	f_Script(HMitem *itemp, char **args);		/* S */
static int	f_SetFbSize(HMitem *itemp, char **args);		/* T */
static int	f_Wrt_IR_Db(HMitem *itemp, char **args);		/* U */
static int	f_Wrt_Lgt_Db(HMitem *itemp, char **args);		/* V */
static int	f_Wrt_Mat_Db(HMitem *itemp, char **args);		/* W */
static int	f_Overlaps(HMitem *itemp, char **args);		/* X */
static int	f_Show_IR(HMitem *itemp, char **args);		/* Z */
static int	f_Grid_Roll(HMitem *itemp, char **args);		/* a */
static int	f_Background(HMitem *itemp, char **args);		/* b */
static int	f_Tracking_Cursor(HMitem *itemp, char **args);	/* c */
static int	f_IR_Offset(HMitem *itemp, char **args);		/* d */
static int	f_Debug(HMitem *itemp, char **args);		/* e */
static int	f_Dist_Grid(HMitem *itemp, char **args);		/* f */
static int	f_Scale_Grid(HMitem *itemp, char **args);		/* g */
static int	f_Rd_Fb(HMitem *itemp, char **args);		/* h */
static int	f_IR_Noise(HMitem *itemp, char **args);		/* i */
static int	f_Key_Frame(HMitem *itemp, char **args);		/* j */
static int	f_Hidden_Ln_Draw(HMitem *itemp, char **args);	/* k */
static int	f_Prnt_Lgt_Db(HMitem *itemp, char **args);	/* l */
static int	f_Prnt_Mat_Db(HMitem *itemp, char **args);	/* m */
static int	f_Parallel(HMitem *itemp, char **args);		/* n */
static int	f_Raster_File(HMitem *itemp, char **args);	/* o */
static int	f_Perspective(HMitem *itemp, char **args);	/* p */
static int	f_Quit(HMitem *itemp, int args);		/* q */
static int	f_Redraw(HMitem *itemp, char **args);		/* r */
static int	f_IRmodule(HMitem *itemp, char **args);		/* s */
static int	f_Grid_Translate(HMitem *itemp, char **args);	/* t */
static int	f_Rd_IR_Db(HMitem *itemp, char **args);		/* u */
static int	f_Rd_Lgt_Db(HMitem *itemp, char **args);		/* v */
static int	f_Rd_Mat_Db(HMitem *itemp, char **args);		/* w */
int		f_Grid_X_Pos(HMitem *itemp, char **args);		/* x */
int		f_Grid_Y_Pos(HMitem *itemp, char **args);		/* y */
static int	f_Shadows(HMitem *itemp, char **args);		/* z */

typedef struct
	{
	int	(*func)();
	} Key_Bindings;

static Key_Bindings func_tab[] =
	{
{/* NUL */ f_Nop},
{/* ^A  */ f_Nop},
{/* ^B  */ f_Nop},
{/* ^C  */ f_Nop},
{/* ^D  */ f_Nop},
{/* ^E  */ f_Nop},
{/* ^F  */ f_Nop},
{/* ^G  */ f_Nop},
{/* ^H  */ f_Nop},
{/* ^I  */ f_Nop},
{/* ^J  */ f_Nop},
{/* ^K  */ f_Nop},
{/* ^L  */ f_Redraw},
{/* ^M  */ f_Nop},
{/* ^N  */ f_Nop},
{/* ^O  */ f_Nop},
{/* ^P  */ f_Nop},
{/* ^Q  */ f_Nop},
{/* ^R  */ f_Nop},
{/* ^S  */ f_Nop},
{/* ^T  */ f_Nop},
{/* ^U  */ f_Nop},
{/* ^V  */ f_Nop},
{/* ^W  */ f_Nop},
{/* ^X  */ f_Nop},
{/* ^Y  */ f_Nop},
{/* ^Z  */ f_Nop},
{/* ESC */ f_Nop},
{/* FS  */ f_Nop},
{/* GS  */ f_Nop},
{/* RS  */ f_Nop},
{/* US  */ f_Nop},
{/* SP  */ f_Nop},
{/* !   */ f_Exec_Shell},
{/* "   */ f_Nop},
{/* #   */ f_Comment},
{/* $   */ f_Nop},
{/* %   */ f_Nop},
{/* &   */ f_Nop},
{/* `   */ f_Nop},
{/* (   */ f_Nop},
{/* )   */ f_Nop},
{/* *   */ f_Nop},
{/* +   */ f_Nop},
{/* ,   */ f_Nop},
{/* -   */ f_Nop},
{/* .   */ f_Buffer},
{/* /   */ f_Nop},
{/* 0   */ f_Nop},
{/* 1   */ f_Nop},
{/* 2   */ f_Nop},
{/* 3   */ f_Nop},
{/* 4   */ f_Nop},
{/* 5   */ f_Nop},
{/* 6   */ f_Nop},
{/* 7   */ f_Nop},
{/* 8   */ f_Nop},
{/* 9   */ f_Nop},
{/* :   */ f_Nop},
{/* ;   */ f_Nop},
{/* <   */ f_Nop},
{/* =   */ f_Nop},
{/* >   */ f_Nop},
{/* ?   */ f_Menu},
{/* @   */ f_Nop},
{/* A   */ f_Anti_Aliasing},
{/* B   */ f_Batch},
{/* C   */ f_Cursor_Module},
{/* D   */ f_Display_Origin},
{/* E   */ f_Fbclear},
{/* F   */ f_Animate},
{/* G   */ f_GridConfig},
{/* H   */ f_Wrt_Fb},
{/* I   */ f_Rd_Raw_IR},
{/* J   */ f_Movie},
{/* K   */ f_Max_Bounce},
{/* L   */ f_Entr_Lgt_Db},
{/* M   */ f_Entr_Mat_Db},
{/* N   */ f_Set_IR_Paint},
{/* O   */ f_Err_File},
{/* P   */ f_Prnt_Regions},
{/* Q   */ f_Set_Region_IR},
{/* R   */ f_Raytrace},
{/* S   */ f_Script},
{/* T   */ f_SetFbSize},
{/* U   */ f_Wrt_IR_Db},
{/* V   */ f_Wrt_Lgt_Db},
{/* W   */ f_Wrt_Mat_Db},
{/* X   */ f_Overlaps},
{/* Y   */ f_Nop},
{/* Z   */ f_Show_IR},
{/* [   */ f_Nop},
{/* \   */ f_Nop},
{/* ]   */ f_Nop},
{/* ^   */ f_Nop},
{/* _   */ f_Nop},
{/* `   */ f_Nop},
{/* a   */ f_Grid_Roll},
{/* b   */ f_Background},
{/* c   */ f_Tracking_Cursor},
{/* d   */ f_IR_Offset},
{/* e   */ f_Debug},
{/* f   */ f_Dist_Grid},
{/* g   */ f_Scale_Grid},
{/* h   */ f_Rd_Fb},
{/* i   */ f_IR_Noise},
{/* j   */ f_Key_Frame},
{/* k   */ f_Hidden_Ln_Draw},
{/* l   */ f_Prnt_Lgt_Db},
{/* m   */ f_Prnt_Mat_Db},
{/* n   */ f_Parallel},
{/* o   */ f_Raster_File},
{/* p   */ f_Perspective},
{/* q   */ f_Quit},
{/* r   */ f_Redraw},
{/* s   */ f_IRmodule},
{/* t   */ f_Grid_Translate},
{/* u   */ f_Rd_IR_Db},
{/* v   */ f_Rd_Lgt_Db},
{/* w   */ f_Rd_Mat_Db},
{/* x   */ f_Grid_X_Pos},
{/* y   */ f_Grid_Y_Pos},
{/* z   */ f_Shadows},
{/* {   */ f_Nop},
{/* |   */ f_Nop},
{/* }   */ f_Nop},
{/* ~   */ f_Nop},
{/* DEL */ f_Nop}
};


HMitem	ray_items[] =
	{
	{ "run now", "generate image while you wait", 0, 0, 0, f_Raytrace, 0 },
	{ "submit batch run", "generate image using job queueing facility", 0, 0, 0, f_Batch, 0 },
	{ 0, 0, 0, 0, 0, 0, 0 }
	};
HMenu	ray_trace_hmenu = { ray_items, 0, 0, 0, 0, 0 };

HMitem	rect_items[] =
	{
	{ "horizontal", "left and right indices", 0, 0, 0, f_Grid_X_Pos, 0 },
	{ "vertical", "bottom and top indices", 0, 0, 0, f_Grid_Y_Pos, 0 },
	{ 0, 0, 0, 0, 0, 0, 0 }
	};
HMenu	rect_hmenu = { rect_items, 0, 0, 0, 1, prnt_Status };

HMitem	grid_items[] =
	{
	{ "resolution and centering",
		"select grid resolution and image centering options",
		0, 0, 0, f_GridConfig, 0 },
	{ "distance to model", "grid origin to centroid of model RPP", 0, 0, 0, f_Dist_Grid, 0 },
	{ "perspective", "0 = none, -1 = manual, > 0 increases", 0, 0, 0, f_Perspective, 0 },
	{ "roll", "rotate grid about viewing axis", 0, 0, 0, f_Grid_Roll, 0 },
	{ "viewport", "rectangular portion of grid to ray trace", &rect_hmenu, 0, 0, 0, 0 },
	{ "image translation", "move image WRT screen", 0, 0, 0, f_Display_Origin, 0 },
	{ "grid translation", "move grid WRT model", 0, 0, 0, f_Grid_Translate, 0 },
	{ "anti-aliasing", "over-sampling factor", 0, 0, 0, f_Anti_Aliasing, 0 },
	{ "key-frame input", "read MGED(1B) key frames", 0, 0, 0, f_Key_Frame, 0 },
	{ 0, 0, 0, 0, 0, 0, 0 }
	};
HMenu	grid_hmenu = { grid_items, 0, 0, 0, 1, prnt_Status };

HMitem	light_items[] =
	{
	{ "print light source", "no help", 0, 0, 0, f_Prnt_Lgt_Db, 0 },
	{ "modify light source", "no help", 0, 0, 0, f_Entr_Lgt_Db, 0 },
	{ "read light source data base", "no help", 0, 0, 0, f_Rd_Lgt_Db, 0 },
	{ "save light source data base", "no help", 0, 0, 0, f_Wrt_Lgt_Db, 0 },
	{ 0, 0, 0, 0, 0, 0, 0 }
	};
HMenu	light_src_hmenu = { light_items, 0, 0, 0, 1, prnt_Status };

HMitem	mater_items[] =
	{
	{ "print material", "no help", 0, 0, 0, f_Prnt_Mat_Db, 0 },
	{ "modify material", "no help", 0, 0, 0, f_Entr_Mat_Db, 0 },
	{ "read material data base", "no help", 0, 0, 0, f_Rd_Mat_Db, 0 },
	{ "save material data base", "no help", 0, 0, 0, f_Wrt_Mat_Db, 0 },
	{ 0, 0, 0, 0, 0, 0, 0 }
	};
HMenu	materials_hmenu = { mater_items, 0, 0, 0, 1, prnt_Status };

HMitem	file_items[] =
	{
	{ "frame buffer", "Graphics display or file for image.", 0, 0, 0, f_Raster_File, 0 },
	{ "error messages", "Redirect errors and diagnostics to a log file.", 0, 0, 0, f_Err_File, 0 },
	{ "write script", "Save current context as an executable script.", 0, 0, 0, f_Script, 0 },
	{ "save image", "Copy frame buffer image to file.", 0, 0, 0, f_Wrt_Fb, 0 },
	{ "read image", "Restore saved frame buffer image.", 0, 0, 0, f_Rd_Fb, 0 },
	{ 0, 0, 0, 0, 0, 0, 0 }
	};
HMenu	file_hmenu = { file_items, 0, 0, 0, 0, prnt_Status };

HMitem	ir_items[] =
	{
	{ "set flags", "no help", 0, 0, 0, f_IRmodule, 0 },
	{ "read real IR data", "no help", 0, 0, 0, f_Rd_Raw_IR, 0 },
	{ "read IR data base", "restore saved 3-d data base", 0, 0, 0, f_Rd_IR_Db, 0 },
	{ "write IR data base", "save 3-d data base", 0, 0, 0, f_Wrt_IR_Db, 0 },
	{ "automatic mapping offsets", "no help", 0, 0, 0, f_IR_Offset, 0 },
	{ "specify noise threshold", "tolerance for sub-division by value", 0, 0, 0, f_IR_Noise, 0 },
	{ "set temperature", "no help", 0, 0, 0, f_Set_IR_Paint, 0 },
	{ "assign temperature by region", "manual assignment of data to sub-tree", 0, 0, 0, f_Set_Region_IR, 0 },
	{ "print temperatures by region", "no help", 0, 0, 0, f_Prnt_Regions, 0  },
	{ "display color assignment legend", "no help", 0, 0, 0, f_Show_IR
, 0 },
	{ 0, 0, 0, 0, 0, 0, 0 }
	};
HMenu	ir_hmenu = { ir_items, 0, 0, 0, 1, 0 };

HMitem	special_items[] =
	{
	{ "infrared modeling", "3-d mapping of infrared data", &ir_hmenu, 0, 0, 0 },
	{ "hidden-line drawing", "Image produced will be a line drawing.", 0, 0, 0, f_Hidden_Ln_Draw, 0 },
	{ 0, 0, 0, 0, 0, 0, 0 }
	};
HMenu	special_hmenu = { special_items, 0, 0, 0, 0, 0 };

HMitem	quit_items[] =
	{
	{ "Really quit", "confirmed exit", 0, 0, 0, f_Quit, 0 },
	{ "Not yet", "no exit", 0, 0, 0, 0, 0 },
	{ 0, 0, 0, 0, 0, 0, 0 }
	};
HMenu	quit_hmenu = { quit_items, 0, 0, 0, 0, 0 };

int
user_Cmd(char **ar)
{	register int ret = (*func_tab[(int)*ar[0]].func)( (HMitem *) 0, ar );
	return	ret;
	}

static int
user_Opt(char opt, char *arg)
{	char	buf[2], *local_argv[MAX_ARGS];
		register int	i;
	buf[0] = opt;
	buf[1] = '\0';
	/* Set up argument vector for function. */
	local_argv[0] = buf;
	local_argv[1] = strtok( arg, " \t" );	/* Arguments to option. */
	/* Look for multiple tokens in argument. */
	for(	i = 2;
		i < 5
	    &&	(local_argv[i] = strtok( (char *) NULL, " \t" )) != NULL;
		i++
		)
		;
	return (*func_tab[(int)*local_argv[0]].func)( (HMitem *) 0, local_argv );
	}

int
user_Pop(int i)
{	char	buf[2], *local_argv[MAX_ARGS];
	if( i < 0 )
		return	1;
	buf[0] = i;
	buf[1] = '\0';
	/* Set up argument vector for function. */
	local_argv[0] = buf;
	local_argv[1] = NULL;
	return	(*func_tab[i].func)( (HMitem *) 0, local_argv );
	}

static int
f_Nop(HMitem *itemp, char **ar)
{
	if( *ar[0] > ' ' && *ar[0] <= '~' )
		bu_log( "Unbound (%c)\n", *ar[0] );
	else
		bu_log( "Unbound (0%o)\n", *ar[0] );
	ring_Bell();
	return	-2;
	}

/*ARGSUSED*/
static int
f_Comment(HMitem *itemp, char **ar)
{
	bu_log( "%s\n", ar[0] );
	return	1;
	}

/*ARGSUSED*/
static int
f_Menu(HMitem *itemp, char **ar)
{
	prnt_Menu();
	return 1;
	}

/*ARGSUSED*/
static int
f_Script(HMitem *itemp, char **args)
{
	return	make_Script( args != (char **) NULL ? args[1] : (char *) NULL );
	}

/*ARGSUSED*/
static int
f_SetFbSize(HMitem *itemp, char **args)
{
	if( args != NULL && args[1] != NULL )
		{
		if( sscanf( args[1], "%d", &fb_size ) != 1 )
			{
			force_fbsz = FALSE;
			return	-1;
			}
		force_fbsz = fb_size > 0 ? TRUE : FALSE;
		}
	else
		{
		(void) sprintf( prompt, "Set frame buffer size ? [y|n](%c) ",
				force_fbsz ? 'y' : 'n'
				);
		if( get_Input( input_ln, MAX_LN, prompt ) != NULL )
			force_fbsz = input_ln[0] != 'n';
		if( force_fbsz )
			{
			(void) sprintf( prompt,
					"Frame buffer size ? (%d) ", fb_size );
			if(	get_Input( input_ln, MAX_LN, prompt ) != NULL
	 		    &&	sscanf( input_ln, "%d", &fb_size ) != 1
				)
				{
				force_fbsz = FALSE;
				return	-1;
				}
			}
		}
	return	1;
	}

/*ARGSUSED*/
static int
f_Raytrace(HMitem *itemp, char **ar)
{
	if( rt_g.rtg_parallel )
		bu_log( "Will use %d processors.\n", npsw );

	user_interrupt = FALSE;	/* Set by interrupt handler. */
	for(	frame_no = movie.m_curframe;
		frame_no <= movie.m_endframe
	     &&	! user_interrupt
	     &&	interpolate_Frame( frame_no )
	     &&	ready_Output_Device( frame_no );
		close_Output_Device( frame_no ),
		frame_no++
		)
		render_Model( frame_no );

	/* Reset movie control structure. */
	movie.m_curframe = 0;
	movie.m_endframe = 0;
	movie.m_noframes = 1;
	if( movie.m_keys && movie.m_keys_fp )
		(void) fclose( movie.m_keys_fp );
	return	1;
	}

HMitem	buffer_items[] =
	{
	{ "unbuffered", "single-pixel I/O", 0, 0, 0, 0, B_PIO },
	{ "page buffered", "demand-paged I/O", 0, 0, 0, 0, B_PAGE },
	{ "line buffered", "scan-line I/O", 0, 0, 0, 0, B_LINE },
	{ 0, 0, 0, 0, 0, 0, 0 }
	};
HMenu	buffer_hmenu = { buffer_items, 0, 0, 0, 0, prnt_Status };

/*
	int f_Buffer( HMItem *itemp, char **args )
 */
/*ARGSUSED*/
static int
f_Buffer(HMitem *itemp, char **args)
{
#if HAS_SGIGL
	if( (args == NULL || args[1] == NULL) && sgi_usemouse )
		{	extern long	buffering_menu;
			int		rc;
		if( (rc = dopup( buffering_menu )) >= 0 )
			pix_buffered = rc;
		}
	else
#endif
	if(	args == NULL || args[1] == NULL
	    ||	sscanf( args[1], "%d", &pix_buffered ) != 1
		)
		{	HMitem	*itemptr;
		if( ! tty )
			{
			bu_log( "pixel buffering command (%c): missing arg.\n",
				*args[0] );
			return	-1;
			}
		if( (itemptr = hmenuhit( &buffer_hmenu, 1, TOP_SCROLL_WIN ))
			!= (HMitem *) 0
			)
			pix_buffered = itemptr->data;
		else
			Rotate( pix_buffered );
		}
	return	1;
	}

HMitem	debug_items[] =
	{
	{ "reset all flags", "debugging off", 0, 0, 0, 0, DEBUG_OFF },
	{ "all rays", "calls to shootray()", 0, 0, 0, 0, DEBUG_ALLRAYS },
	{ "shoot", "info about shootray() processing", 0, 0, 0, 0, DEBUG_SHOOT },
	{ "data base", "data base debugging", 0, 0, 0, 0, DEBUG_DB },
	{ "solids", "prep'ed solids", 0, 0, 0, 0, DEBUG_SOLIDS },
	{ "regions", "regions and boolean trees", 0, 0, 0, 0, DEBUG_REGIONS },
	{ "arb8", "ARB8 details", 0, 0, 0, 0, DEBUG_ARB8 },
	{ "spline", "no help", 0, 0, 0, 0, DEBUG_SPLINE },
	{ "roots", "root finder details", 0, 0, 0, 0, DEBUG_ROOTS },
	{ "partition", "info about boolweave()", 0, 0, 0, 0, DEBUG_PARTITION },
	{ "cut", "space subdivision details", 0, 0, 0, 0, DEBUG_CUT },
	{ "boxing", "object/box checking details", 0, 0, 0, 0, DEBUG_BOXING },
	{ "memory", "dynamic memory operations", 0, 0, 0, 0, DEBUG_MEM },
	{ "testing", "one-shot debugging flag", 0, 0, 0, 0, DEBUG_TESTING },
	{ "fdiff", "bool/fdiff debugging", 0, 0, 0, 0, DEBUG_FDIFF },
	{ "RGB", "trace color computation", 0, 0, 0, 0, DEBUG_RGB },
	{ "refraction", "stats on refracted rays", 0, 0, 0, 0, DEBUG_REFRACT },
	{ "normals", "info on surface normal checking", 0, 0, 0, 0, DEBUG_NORML },
	{ "shadows", "stats on shadowed vs. illumated pixels", 0, 0, 0, 0, DEBUG_SHADOW },
	{ "cell size", "no help", 0, 0, 0, 0, DEBUG_CELLSIZE },
	{ "octree", "dumps octree data structure", 0, 0, 0, 0, DEBUG_OCTREE },
	{ 0, 0, 0, 0, 0, 0, 0 }
	};
HMenu	debug_hmenu = { debug_items, 0, 0, 0, 0, prnt_Status };

/*
	int f_Debug( HMItem *itemp, char **args )
 */
/*ARGSUSED*/
static int
f_Debug(HMitem *itemp, char **args)
{
#if HAS_SGIGL
	if( (args == NULL || args[1] == NULL) && sgi_usemouse )
		{	extern long	debugging_menu;
			int		flag;
		if( (flag = dopup( debugging_menu )) > 0 )
			rt_g.debug |= flag;
		else
		if( flag == 0 )
			rt_g.debug = 0;
		}
	else
#endif
	if(	args == NULL || args[0] == NULL
	    ||	args[1] == NULL || sscanf( args[1], "%x", (unsigned int *)&rt_g.debug ) != 1
		)
		{	HMitem	*itemptr;
		if( ! tty )
			{
			bu_log( "debug command (%c): missing argument.\n",
				*args[0] );
			return	-1;
			}
		if( (itemptr = hmenuhit( &debug_hmenu, 1, TOP_SCROLL_WIN ))
			!= (HMitem *) 0
			)
			{
			if( itemptr->data > 0 )
				rt_g.debug |= itemptr->data;
			else
				rt_g.debug = 0;
			}
		}
	if( RT_G_DEBUG & DEBUG_OCTREE )
		prnt_Octree( &ir_octree, 0 );
	return	1;
	}

/*
	int f_Grid_Roll( HMItem *itemp, char **args )
 */
static int
f_Grid_Roll(HMitem *itemp, char **args)
{
	if( args == NULL || args[1] == NULL || sscanf( args[1], "%lf", &grid_roll ) != 1 )
		{
		if( tty )
			{
			(void) sprintf( prompt, "Grid roll ? (%g) ", grid_roll*DEGRAD );
			if( get_Input( input_ln, MAX_LN, prompt ) != NULL )
				{
			   	if( sscanf( input_ln, "%lf", &grid_roll ) != 1 )
					return	-1;
				else
					grid_roll /= DEGRAD;
				}
			}
		else
			return	-1;
		}
	else
		grid_roll /= DEGRAD;
	return	1;
	}

/*	f _ A n t i _ A l i a s i n g ( ) */
static int
f_Anti_Aliasing(HMitem *itemp, char **args)
{
	if(	args == NULL || args[1] == NULL
	     ||	sscanf( args[1], "%d", &aperture_sz ) != 1
		)
		{
		if( tty )
			{
			(void) sprintf( prompt,
					"Anti-aliasing factor ? (%d) ",
					aperture_sz
					);
			if(	get_Input( input_ln, MAX_LN, prompt ) != NULL
			    &&	sscanf( input_ln, "%d", &aperture_sz ) != 1
				)
				return	-1;
			}
		else
			return	-1;
		}
	if( aperture_sz > 1 )
		{
		anti_aliasing = TRUE;
		sample_sz = Sqr( aperture_sz );
		}
	else
		anti_aliasing = FALSE;
	return	1;
	}
/* Disgusting hack to avoid the compiler warnings about tmpnam(),
 * and yet not rewrite lgt.
 */
char *
lgt_tmp_name(void)
{
	char *template;
	int tmp_fd;

	template = bu_strdup( "/tmp/lgt_tmp.XXXXXX" );
	if( (tmp_fd=mkstemp( template ) ) == -1 ) {
		bu_bomb( "Unable to open tmp file!!!\n" );
	}

	close( tmp_fd );
	return( template );
}

#ifndef L_tmpnam /* Alliant has bogus "tmpnam" implementation. */
#define L_tmpnam	20
#endif
/*	f _ B a t c h ( ) */
/*ARGSUSED*/
static int
f_Batch(HMitem *itemp, char **args)
{
		static char	*batch_com[8];
		char		*script;
#ifdef cray
	bu_log( "Sorry, no batch queue on the Cray yet.\n" );
	return	1;
#else
	script = lgt_tmp_name();
	batch_com[0] = "batch";
	batch_com[1] = "-m";
	batch_com[2] = "-t";
	batch_com[3] = prog_id;
	batch_com[4] = "-s";
	batch_com[5] = "/bin/sh";
	batch_com[6] = script;
	batch_com[7] = NULL;
	if( make_Script( script ) == -1 )
		return	-1;
	(void) exec_Shell( batch_com );
	(void) unlink( script );
	bu_free( (char *)script, "lgt temporary file name" );
	return	1;
#endif
	}

#ifdef UP
#undef UP
#endif

#ifdef DOWN
#undef DOWN
#endif

#define UP	'k'
#define DOWN	'j'
#define LEFT	'h'
#define RIGHT	'l'
#define JUMP	(32/zoom)
#define JUP	'K'
#define JDOWN	'J'
#define JLEFT	'H'
#define JRIGHT	'L'
#define IN	'i'
#define OUT	'o'
#define CENTER	'c'
#define READ	'r'
#define MAPFROM	'f'
#define MAPTO	't'
#define SELECT	' '
#define ENTER	'\n'
#define RESET	'\r'
#define QUIT	'q'

void
pt_Select(register int x, register int y, register int *xp, register int *yp, register int *originp)
{	char		*args[3];
		char		buf1[5], buf2[5];

	args[1] = buf1;
	args[2] = buf2;
	if( *originp )
		{
		*xp = x;
		*yp = y;
		(void) sprintf( args[1], "%d", x - x_fb_origin );
		(void) sprintf( args[2], "%d", grid_x_fin );
		(void) f_Grid_X_Pos( (HMitem *) 0, args );
		(void) sprintf( args[1], "%d", y - y_fb_origin );
		(void) sprintf( args[2], "%d", grid_y_fin );
		(void) f_Grid_Y_Pos( (HMitem *) 0, args );
		}
	else
		{	int	x_fin, y_fin;
		x_fin = x - x_fb_origin;
		y_fin = y - y_fb_origin;
		if( x_fin < grid_x_org )
			{
			Swap_Integers( x_fin, grid_x_org );
			}
		if( y_fin < grid_y_org )
			{
			Swap_Integers( y_fin, grid_y_org );
			}
		(void) sprintf( args[1], "%d", grid_x_org );
		(void) sprintf( args[2], "%d", x_fin );
		(void) f_Grid_X_Pos( (HMitem *) 0, args );
		(void) sprintf( args[1], "%d", grid_y_org );
		(void) sprintf( args[2], "%d", y_fin );
		(void) f_Grid_Y_Pos( (HMitem *) 0, args );
		}
	Toggle( *originp );
	return;
	}


/*
	setCellSize( void )

 */
static int
setCellSize(void)
{
	(void) sprintf( prompt, "Cell size (mm) ? (%g) ", cell_sz );
	if(	get_Input( input_ln, MAX_LN, prompt ) != NULL
	   &&	sscanf( input_ln, "%lf", &cell_sz ) != 1
		)
		return	FALSE;
	return	TRUE;
	}

void
setGridSize(int size)
{	char	buf[10];
		char	*ar[3];
	ar[1] = "0";
	ar[2] = buf;
	(void) sprintf( ar[2], "%d", grid_sz-1 );
	(void) f_Grid_X_Pos( (HMitem *) 0, ar );
	(void) f_Grid_Y_Pos( (HMitem *) 0, ar );
	return;
	}

/*
	setViewSize( void )

 */
static int
setViewSize(void)
{
	(void) sprintf( prompt, "View size (mm) ? (%g) ", view_size );
	if(	get_Input( input_ln, MAX_LN, prompt ) != NULL
	   &&	sscanf( input_ln, "%lf", &view_size ) != 1
		)
		return	FALSE;
	return	TRUE;
	}

HMitem	cursor_items[] =
	{
	{ "tag pixel", "no help", 0, 0, 0, 0, C_TAGPIXEL },
	{ "sweep rectangle", "no help", 0, 0, 0, 0, C_SWEEPREC },
	{ "window in", "no help", 0, 0, 0, 0, C_I_WINDOW },
	{ "window out", "no help", 0, 0, 0, 0, C_O_WINDOW },
	{ "query region", "no help", 0, 0, 0, 0, C_QUERYREG },
	{ 0, 0, 0, 0, 0, 0, 0 }
	};
HMenu	cursor_hmenu = { cursor_items, 0, 0, 0, 0, 0 };

static int	popups = FALSE;	/* Are pop-up menus being used? */

/*	f_Cursor_Module() */
/*ARGSUSED*/
static int
f_Cursor_Module(HMitem *itemp, char **args)
{	static int	x = -1, y = -1;
		register int	c;
		int		origin = TRUE;
		int		xx0, yy0;

	if( ! tty )
		return	-1;

	if( ! ready_Output_Device( 0 ) )
		return	-1;

	/* Initialize x and y only once so we can continue where we were.*/
	if( x < 0 || y < 0 )
		{
		x = grid_sz / 2;
		y = grid_sz / 2;
		}
	xx0 = x;
	yy0 = y;
	(void) fb_cursor( fbiop, 1, x, y );

	for( ; ; )
		{	int		fx=0, fy=0, tx=0, ty=0;
			int		cx = x, cy = y;
			int		mx, my;
			int		mapfromflag = FALSE;
			int		maptoflag = FALSE;
			int		tagging_pixel = FALSE;
			int		sweeping_rect = FALSE;
			int		in_windowing = FALSE;
			int		out_windowing = FALSE;
			HMitem		*itemptr;
#if HAS_SGIGL
		if( sgi_usemouse )
			{
			switch( sgi_Cursor_Input( x, y, &mx, &my, xx0, yy0, origin ) )
				{
			default :
				break;
			case 2 :
				goto	keybd_input;
			case 3 :
				goto	exit_cm;
				}
			}
#endif /* ifdef sgi */
		if( (itemptr = hmenuhit( &cursor_hmenu, 1, TOP_SCROLL_WIN ))
			== (HMitem *) 0
			)
			{
			prnt_Prompt( "" );
			(void) fb_setcursor( fbiop, arrowcursor, 16, 16, 0, 0 );
			(void) fb_cursor( fbiop, tracking_cursor, x, y );
			goto	exit_cm;
			}
		switch( (int) itemptr->data )
			{
		case C_TAGPIXEL : /* Tag pixel. */
			tagging_pixel = TRUE;
			break;
		case C_SWEEPREC : /* Sweep rectangle. */
			sweeping_rect = TRUE;
			break;
		case C_I_WINDOW : /* Window in. */
			if( rel_perspective > 0.0 )
				{
				prnt_Scroll( "Windowing does not yet work WITH perspective.\n" );
				prnt_Scroll( "Set perspective to zero or negative.\n" );
				goto	exit_cm;
				}
			in_windowing = TRUE;
			break;
		case C_O_WINDOW : /* Window out. */
			if( rel_perspective > 0.0 )
				{
				prnt_Scroll( "Windowing does not yet work WITH perspective.\n" );
				prnt_Scroll( "Set perspective to zero or negative.\n" );
				goto	exit_cm;
				}
			out_windowing = TRUE;
			break;
		case C_QUERYREG : /* Query region. */
			tagging_pixel = TRUE;
			Toggle( query_region );
			break;
			}
#if HAS_SGIGL
keybd_input :
#endif
		prnt_Event( "" );
		prnt_Prompt( "Cursor input module (type '?' for help) : " );
		set_Cbreak( 0 );
		clr_Echo( 0 );
		(void) fb_setcursor( fbiop, target1, 16, 16, 8, 8 );
		(void) fb_cursor( fbiop, 1, x, y );
		while( (c = hm_getchar()) != QUIT )
			{
			switch( c )
				{
			case DOWN :
				if( y > 0 )
					y--;
				break;
			case UP :
				if( y < grid_sz - 1 )
					y++;
				break;
			case LEFT :
				if( x > 0 )
					x--;
				break;
			case RIGHT :
				if( x < grid_sz - 1 )
					x++;
				break;
			case JDOWN :
				y -= JUMP;
				if( y < 0 )
					y = 0;
				break;
			case JUP :
				y += JUMP;
				if( y > grid_sz - 1 )
					y = (grid_sz - 1);
				break;
			case JLEFT :
				x -= JUMP;
				if( x < 0 )
					x = 0;
				break;
			case JRIGHT :
				x += JUMP;
				if( x > grid_sz - 1 )
					x = (grid_sz - 1);
				break;
			case CENTER :
				cx = x;
				cy = y;
				(void) fb_window( fbiop, cx, cy );
				break;
			case ENTER :
			case RESET :
				cx = cy = x = y = grid_sz / 2;
				zoom = fb_getwidth( fbiop ) / grid_sz;
				(void) fb_zoom( fbiop, zoom, zoom );
				(void) fb_window( fbiop, cx, cy );
				break;
			case IN :
				if( zoom <= fbiop->if_width/2 )
					{
					zoom *= 2;
					cx = x;
					cy = y;
					(void) fb_zoom( fbiop, zoom, zoom );
					(void) fb_window( fbiop, cx, cy );
					}
				else
					ring_Bell();
				break;
			case OUT :
				if( zoom > 1 )
					{
					zoom /= 2;
					cx = x;
					cy = y;
					(void) fb_zoom( fbiop, zoom, zoom );
					(void) fb_window( fbiop, cx, cy );
					}
				else
					ring_Bell();
				break;
	 		case READ :
				{	RGBpixel	pixel;
				mx = XSCR2MEM( x );
				my = YSCR2MEM( y );
				if(	fb_seek( fbiop, mx, my ) == -1
				    ||	fb_rpixel( fbiop, (unsigned char *) pixel )
						== -1
					)
					bu_log( "Read from <%d,%d> failed.", mx, my );
				else
					prnt_Scroll( "Temperature is %d degrees fahrenheit.\n",
						pixel_To_Temp( (RGBpixel *)pixel )
						);
				prnt_Prompt( "Cursor input module (type '?' for help) : " );
				break;
				}
			case MAPFROM :
				mx = XSCR2MEM( x );
				my = YSCR2MEM( y );
				mapfromflag = TRUE;
				fx = mx - x_fb_origin;
				fy = my - y_fb_origin;
				if( maptoflag )
					{
					ir_mapx = tx - fx;
					ir_mapy = ty - fy;
					bu_log( "to<%d,%d> from<%d,%d> delta<%d,%d>\n",
						tx, ty, fx, fy, ir_mapx, ir_mapy );
					ir_offset = TRUE;
					(void) sprintf( IR_AUTO_MAP_PTR, "%4s", "ON" );
					update_Screen();
					prnt_Prompt( "Cursor input module (type '?' for help) : " );
					}
				break;
			case MAPTO :
				mx = XSCR2MEM( x );
				my = YSCR2MEM( y );
				maptoflag = TRUE;
				tx = mx - x_fb_origin;
				ty = my - y_fb_origin;
				if( mapfromflag )
					{
					ir_mapx = tx - fx;
					ir_mapy = ty - fy;
					bu_log( "to<%d,%d> from<%d,%d> delta<%d,%d>\n",
						tx, ty, fx, fy, ir_mapx, ir_mapy );
					ir_offset = TRUE;
					(void) sprintf( IR_AUTO_MAP_PTR, "%4s", "ON" );
					update_Screen();
					prnt_Prompt( "Cursor input module (type '?' for help) : " );
					}
				break;
			case SELECT :
				mx = XSCR2MEM( x );
				my = YSCR2MEM( y );
				pt_Select( mx, my, &xx0, &yy0, &origin );
				if( tagging_pixel )
					{
					pt_Select( mx, my, &xx0, &yy0, &origin );
					ir_doing_paint = TRUE;
					(void) sprintf( IR_PAINT_PTR, "%4d", ir_paint );
					update_Screen();
					}
				else
				if( ! origin )
					break;
				else
				if( sweeping_rect )
					{
					ir_doing_paint = TRUE;
					(void) sprintf( IR_PAINT_PTR, "%4d", ir_paint );
					update_Screen();
					}
				else
				if( in_windowing || out_windowing )
					{	register int	dx, dy, dw;
						fastf_t		scale;
						fastf_t		x_translate, y_translate;
						static char	buf[5];
						static char	*ar[3];
					dx = mx - xx0;
					dx = Abs( dx );
					dy = my - yy0;
					dy = Abs( dy );
					dw = Max( dx, dy );
#define Pixel2Grid(x_) ((x_)/((fastf_t)fbiop->if_width/grid_sz))
#define Grid2Model(x_) ((x_)*cell_sz)
					scale = dw / (fastf_t)(grid_sz);
					if( out_windowing )
						{
						scale = 1.0 / scale;
						x_translate = (xx0+mx)/2.0 - grid_sz/2.0;
						x_translate = Grid2Model( x_translate );
						y_translate = (yy0+my)/2.0 - grid_sz/2.0;
						y_translate = Grid2Model( y_translate );
						x_grid_offset -= x_translate * scale;
						y_grid_offset -= y_translate * scale;
						}
					else
						{
						x_translate = (xx0+mx)/2.0 - grid_sz/2.0;
						x_translate = Grid2Model( x_translate );
						y_translate = (yy0+my)/2.0 - grid_sz/2.0;
						y_translate = Grid2Model( y_translate );
						x_grid_offset += x_translate;
						y_grid_offset += y_translate;
						}
						view_size *= scale;
						/* Scale down view size. */
					ar[0] = "G";
					ar[1] = buf;
					ar[2] = NULL;
					(void) sprintf( ar[1], "%d", grid_sz );
					(void) f_GridConfig( (HMitem *) 0, ar );
					}
				reset_Tty( 0 );
				user_interrupt = FALSE;
				render_Model( 0 );
				set_Cbreak( 0 );
				clr_Echo( 0 );
				prnt_Prompt( "Cursor input module (type '?' for help) : " );
				break;
			case '?' :
				prnt_Scroll( "KEY\tFUNCTION\n" );
				prnt_Scroll( " '%c'\tmove left\n", LEFT );
				prnt_Scroll( " '%c'\tmove right\n", RIGHT );
				prnt_Scroll( " '%c'\tmove up\n", UP );
				prnt_Scroll( " '%c'\tmove down\n", DOWN );
				prnt_Scroll( " '%c'\tjump left\n", JLEFT );
				prnt_Scroll( " '%c'\tjump right\n", JRIGHT );
				prnt_Scroll( " '%c'\tjump up\n", JUP );
				prnt_Scroll( " '%c'\tjump down\n", JDOWN );
				prnt_Scroll( " '%c'\tzoom in\n", IN );
				prnt_Scroll( " '%c'\tzoom out\n", OUT );
				prnt_Scroll( " '%c'\tcenter window about cursor\n", CENTER );
				prnt_Scroll( " %s\treset window\n", "RETURN" );
				prnt_Scroll( " '%c'\tread current pixel's temperature\n", READ );
				prnt_Scroll( " '%c'\tselect mapping pixel on IR image\n", MAPFROM );
				prnt_Scroll( " '%c'\tselect mapping pixel on raytrace image\n", MAPTO );
				prnt_Scroll( " '%c'\tselect corner of current rectangle\n", SELECT );
				prnt_Scroll( " '%c'\tquit\n", QUIT );
				prnt_Prompt( "Cursor input module (type '?' for help) : " );
				break;
			default :
				ring_Bell();
				bu_log( "Illegal key '%c:0%o'.\n", c, c );
				prnt_Prompt( "Cursor input module (type '?' for help) : " );
				break;
				}
			(void) fb_cursor( fbiop, 1, x, y );
			} /* end while */
		(void) fb_setcursor( fbiop, arrowcursor, 16, 16, 0, 0 );
		prnt_Prompt( "" );
		reset_Tty( 0 );
		} /* end for */
exit_cm :	;
	query_region = FALSE;
	close_Output_Device( 0 );
	return	1;
	}

/*	f _ D i s p l a y _ O r i g i n ( ) */
/*ARGSUSED*/
static int
f_Display_Origin(HMitem *itemp, char **args)
{
	if(	args == NULL ||
		args[1] == NULL || sscanf( args[1], "%d", &x_fb_origin ) != 1
	    ||	args[2] == NULL || sscanf( args[2], "%d", &y_fb_origin ) != 1
		)
		{
		if( tty )
			{
			(void) sprintf( prompt,
					"Image offset ? [x y](%d %d) ",
					x_fb_origin, y_fb_origin
					);
			if(	get_Input( input_ln, MAX_LN, prompt ) != NULL
			   &&	sscanf( input_ln, "%d %d", &x_fb_origin, &y_fb_origin ) != 2
				)
				return	-1;
			}
		else
			return	-1;
		}
	return	1;
	}

/*	f _ A n i m a t e ( ) */
/*ARGSUSED*/
static int
f_Animate(HMitem *itemp, char **args)
{
	(void) sprintf( prompt, "Full-screen animation ? [y|n](%c) ", movie.m_fullscreen ? 'y' : 'n' );
	if( get_Input( input_ln, MAX_LN, prompt ) != NULL )
		movie.m_fullscreen = input_ln[0] != 'n';
	if( movie.m_fullscreen )
		{	char		movie_file[MAX_LN];
			char		*suffixptr;
		(void) sprintf( prompt, "Movie file prefix ? (%s) ",
				prefix );
		if( get_Input( input_ln, MAX_LN, prompt ) != NULL )
			(void) strcpy( prefix, input_ln );
		(void) strcpy( movie_file, prefix );
		suffixptr = movie_file + strlen( movie_file );
		(void) sprintf( prompt, "Number of frames ? (%d) ", noframes );
		if(	get_Input( input_ln, MAX_LN, prompt ) != NULL
		    &&	sscanf( input_ln, "%d", &noframes ) != 1
			)
			{
			bu_log( "Illegal input (%s).\n", input_ln );
			return	-1;
			}
		ready_Output_Device( 0 );
		(void) signal( SIGINT, abort_RT );
		for( frame_no = 0; ! user_interrupt; frame_no++ )
			{	FBIO	*movie_fbiop;
				register int	y;
			if( frame_no == noframes )
				frame_no = 0;
			(void) sprintf( suffixptr, ".%04d", frame_no );
			if( access( movie_file, 0 ) == -1 )
				continue;
			if( (movie_fbiop = fb_open( movie_file, grid_sz, grid_sz )) == FBIO_NULL )
				{
				(void) signal( SIGINT, norml_sig );
				close_Output_Device( 0 );
				return	-1;
				}
			if( fb_ioinit( movie_fbiop ) == -1 )
				{
				(void) signal( SIGINT, norml_sig );
				close_Output_Device( 0 );
				(void) fb_close( movie_fbiop );
				return	-1;
				}
			prnt_Status();
			for( y = 0; y < grid_sz; y++ )
				{	RGBpixel	pixels[1024];
				if( fb_read( movie_fbiop, 0, y, (unsigned char *)pixels, grid_sz ) == -1 )
					{
					fb_log( "Read from <%d,%d> failed on file \"%s\".\n", 0, y, movie_file );
					(void) signal( SIGINT, norml_sig );
					close_Output_Device( 0 );
					(void) fb_close( movie_fbiop );
					return	-1;
					}
				if( fb_write( fbiop, 0, y, (unsigned char *)pixels, grid_sz ) == -1 )
					{
					fb_log( "Write to <%d,%d> failed on \"%s\".\n", 0, y, fb_file );
					(void) signal( SIGINT, norml_sig );
					close_Output_Device( 0 );
					(void) fb_close( movie_fbiop );
					return	-1;
					}
				}
			if( fb_close( movie_fbiop ) == -1 )
				{
				fb_log( "Can't close \"%s\".\n", movie_file );
				(void) signal( SIGINT, norml_sig );
				return	-1;
				}
			}
		user_interrupt = FALSE;
		(void) signal( SIGINT, norml_sig );
		close_Output_Device( 0 );
		return	1;
		}
#if HAS_SGIGL
	{	extern void	sgi_Animate();
		static int	fps = 30;
	(void) sprintf( prompt, "Frame size ? (%d) ", movie.m_frame_sz );
	if(	get_Input( input_ln, MAX_LN, prompt ) != NULL
	    &&	sscanf( input_ln, "%d", &movie.m_frame_sz ) != 1 )
		{
		bu_log( "Illegal input (%s)\n", input_ln );
		return	-1;
		}
	(void) sprintf( prompt, "Number of frames ? (%d) ", noframes );
	if(	get_Input( input_ln, MAX_LN, prompt ) != NULL
	    &&	sscanf( input_ln, "%d", &noframes ) != 1 )
		{
		bu_log( "Illegal input (%s)\n", input_ln );
		return	-1;
		}
	(void) sprintf( prompt, "Frames per second ? (%d) ", fps );
	if(	get_Input( input_ln, MAX_LN, prompt ) != NULL
	    &&	sscanf( input_ln, "%d", &fps ) != 1
		)
		{
		bu_log( "Illegal input (%s)\n", input_ln );
		return	-1;
		}
	x_fb_origin = 0;
	y_fb_origin = 0;
	if(	fbiop == NULL
	     && ! fb_Setup( fb_file, MovieSize( movie.m_frame_sz, noframes ) ) )
		return	-1;
	sgi_Animate( movie.m_frame_sz, fps < 0 ? 1 : (fps > 64 ? 64 : fps) );
	close_Output_Device( 0 );
	}
#endif
	return	1;
	}

/*
	int f_GridConfig( HMItem *itemp, char **args )
 */
/*ARGSUSED*/
static int
f_GridConfig(HMitem *itemp, char **args)
{
	if( args != NULL && args[1] != NULL && args[2] == NULL )
		{ /* Old style 'G' command for upward compatibility of
			'lgt' scripts. */
		if( sscanf( args[1], "%d", &grid_sz ) != 1 )
			return	-1;
		force_cellsz = FALSE;
		type_grid = GT_RPP_CENTERED;
		}
	else
	if( args != NULL && args[1] != NULL
		&& args[2] != NULL && args[3] != NULL && args[4] != NULL )
		{	fastf_t	vsize;
		if( sscanf( args[2], "%d", &force_cellsz ) != 1 )
			{
			bu_log( "Can't read size flag!\n" );
			return	-1;
			}
		if( force_cellsz )
			{
#if SINGLE_PRECISION
			if( sscanf( args[1], "%f", &cell_sz ) != 1 )
#else
			if( sscanf( args[1], "%lf", &cell_sz ) != 1 )
#endif
				{
				bu_log( "Can't read cell size!\n" );
				return	-1;
				}
			}
		else
			if( sscanf( args[1], "%d", &grid_sz ) != 1 )
				{
				bu_log( "Can't read grid size!\n" );
				return	-1;
				}
		if( sscanf( args[3], "%d", &type_grid ) != 1 )
			{
			bu_log( "Can't read grid type!\n" );
			return	-1;
			}

#if SINGLE_PRECISION
		if( sscanf( args[4], "%f", &vsize ) != 1 )
#else
		if( sscanf( args[4], "%lf", &vsize ) != 1 )
#endif
			{
			bu_log( "Can't read view size!\n" );
			return	-1;
			}
		if( vsize > EPSILON )
			{
			view_size = vsize;
			force_viewsz = TRUE;
			}
		else
			force_viewsz = FALSE;
		}
	else
	if( args == NULL )
		{
		(void) sprintf( prompt,
				"Specify cell size ? [y|n](%c) ",
				force_cellsz ? 'y' : 'n'
				);
		if( get_Input( input_ln, MAX_LN, prompt ) != NULL )
			force_cellsz = input_ln[0] != 'n';
		if( force_cellsz )
			{
			if( ! setCellSize() )
				{
				force_cellsz = FALSE;
				return	-1;
				}
			}
		else
			{
			(void) sprintf( prompt, "Grid size ? (%d) ", grid_sz );
			if(	get_Input( input_ln, MAX_LN, prompt ) != NULL
		 	   &&	sscanf( input_ln, "%d", &grid_sz ) != 1
				)
				return	-1;
			}
		(void) sprintf( prompt,
				"Center image WRT model centroid ? [y|n](%c) ",
				type_grid == GT_RPP_CENTERED ? 'y' : 'n'
				);
		if( get_Input( input_ln, MAX_LN, prompt ) != NULL )
			type_grid = input_ln[0] != 'n' ?
					GT_RPP_CENTERED : GT_ORG_CENTERED;
		(void) sprintf( prompt,
				"Specify view size ? [y|n](%c) ",
				force_viewsz ? 'y' : 'n'
				);
		if( get_Input( input_ln, MAX_LN, prompt ) != NULL )
			force_viewsz = input_ln[0] != 'n';
		if( force_viewsz && ! setViewSize() )
			{
			force_viewsz = FALSE;
			return	-1;
			}
		}
	setGridSize( grid_sz );
	if( ! force_viewsz )
		view_size = modl_radius * 2.0;
	return	1;
	}

/*	f _ W r t _ F b ( ) */
/*ARGSUSED*/
static int
f_Wrt_Fb(HMitem *itemp, char **args)
{	register int y;
		static char save_fb_file[MAX_LN] = { 0 };
		FBIO *save_fbiop;
	if( args != NULL && args[1] != NULL )
		(void) strncpy( save_fb_file, args[1], MAX_LN );
	else
	if( tty )
		{
		(void) sprintf( prompt,
				"File name for saved image ? (%s) ",
				save_fb_file
				);
		if( get_Input( input_ln, MAX_LN, prompt ) != NULL )
			(void) strcpy( save_fb_file, input_ln );
		}
	else
		return	-1;
	if( save_fb_file[0] == '\0' )
		{
		bu_log( "No default, must specify file name!\n" );
		return	-1;
		}
	if( strcmp( save_fb_file, fb_file ) == 0 )
		{
		bu_log( "Frame buffer is same as saved image!\n" );
		return	-1;
		}
	if( ! ready_Output_Device( 0 ) )
		return	-1;
	prnt_Event( "Saving image..." );
	if( (save_fbiop =
		fb_open( save_fb_file, grid_sz, grid_sz ))== FBIO_NULL )
		{
		close_Output_Device( 0 );
		return	-1;
		}
	if( fb_ioinit( save_fbiop ) == -1 )
		{
		close_Output_Device( 0 );
		(void) fb_close( save_fbiop );
		return	-1;
		}
	for( y = 0; y < grid_sz; y++ )
		{	RGBpixel	pixels[1024];
		if( fb_read( fbiop, 0, y, (unsigned char *)pixels, grid_sz ) == -1 )
			{
			fb_log( "Read from <%d,%d> failed.\n", 0, y );
			close_Output_Device( 0 );
			(void) fb_close( save_fbiop );
			return	-1;
			}
		if( fb_write( save_fbiop, 0, y, (unsigned char *)pixels, grid_sz ) == -1 )
			{
			fb_log( "Write to <%d,%d> failed.\n", 0, y );
			close_Output_Device( 0 );
			(void) fb_close( save_fbiop );
			return	-1;
			}
		}
	close_Output_Device( 0 );
	if( fb_close( save_fbiop ) == -1 )
		{
		fb_log( "Can't close \"%s\".\n", save_fb_file );
		return	-1;
		}
	return	1;
	}

/*	f _ R d _ F b ( ) */
/*ARGSUSED*/
static int
f_Rd_Fb(HMitem *itemp, char **args)
{	register int y;
		static char save_fb_file[MAX_LN] = { 0 };
		FBIO *save_fbiop;
	if( args != NULL && args[1] != NULL )
		(void) strncpy( save_fb_file, args[1], MAX_LN );
	else
	if( tty )
		{
		(void) sprintf( prompt,
				"File name of image ? (%s) ",
				save_fb_file
				);
		if( get_Input( input_ln, MAX_LN, prompt ) != NULL )
			(void) strcpy( save_fb_file, input_ln );
		}
	else
		return	-1;
	if( save_fb_file[0] == '\0' )
		{
		bu_log( "No default, must specify file name!\n" );
		return	-1;
		}
	if( strcmp( save_fb_file, fb_file ) == 0 )
		{
		bu_log( "Frame buffer is same as saved image!\n" );
		return	-1;
		}
	if( ! ready_Output_Device( 0 ) )
		return	-1;
	prnt_Event( "Reading saved image..." );
	if( (save_fbiop =
		fb_open( save_fb_file, grid_sz, grid_sz )) == FBIO_NULL )
		{
		close_Output_Device( 0 );
		return	-1;
		}
	if( fb_ioinit( save_fbiop ) == -1 )
		{
		close_Output_Device( 0 );
		(void) fb_close( save_fbiop );
		return	-1;
		}
	for( y = 0; y < grid_sz; y++ )
		{	RGBpixel	pixels[1024];
		if( fb_read( save_fbiop, 0, y, (unsigned char *)pixels, grid_sz ) == -1 )
			{
			fb_log( "Read from <%d,%d> failed.\n", 0, y );
			close_Output_Device( 0 );
			(void) fb_close( save_fbiop );
			return	-1;
			}
		if( fb_write( fbiop, 0, y, (unsigned char *)pixels, grid_sz ) == -1 )
			{
			fb_log( "Write to <%d,%d> failed.\n", 0, y );
			close_Output_Device( 0 );
			(void) fb_close( save_fbiop );
			return	-1;
			}
		}
	close_Output_Device( 0 );
	if( fb_close( save_fbiop ) == -1 )
		{
		fb_log( "Can't close \"%s\".\n", save_fb_file );
		return	-1;
		}
	return	1;
	}

static void
wait_For_User(void)
{
	(void) get_Input( input_ln, MAX_LN, "Continue ? [<return>] " );
	(void) f_Redraw( (HMitem *) 0, (char **) 0 );
	return;
	}

/*	f _ P r n t _ L g t _ D b ( ) */
/*ARGSUSED*/
static int
f_Prnt_Lgt_Db(HMitem *itemp, char **args)
{	static int	light_id = -1;
#if HAS_SGIGL
	if( (args == NULL || args[1] == NULL) && sgi_usemouse )
		{	extern long	lgts_prnt_menu;
		light_id = dopup( lgts_prnt_menu );
		}
	else
#endif
	if( (args == NULL || args[1] == NULL ) && tty )
		{
		(void) sprintf( input_ln, "%d", light_id );
		(void) sprintf( prompt,
				"Light source id ? [0-%d or * for all](%s) ",
				MAX_LGTS-1,
				input_ln[0] == '-' ? "*" : input_ln
				);
		if(	get_Input( input_ln, MAX_LN, prompt ) != NULL
		    &&	sscanf( input_ln, "%d", &light_id ) != 1
			)
			{
			if( input_ln[0] == '*' )
				light_id = -1;
			}
		}
	else
	if( 	args != NULL && args[1] != NULL
	    &&	sscanf( args[1], "%d", &light_id ) != 1
		)
		{
		bu_log( "Illegal input (%s)\n", args[1] );
		return	-1;
		}
	if( lgt_Print_Db( light_id ) && popups )
		wait_For_User();
	return	1;
	}

/*	f _ P r n t _ M a t _ D b ( ) */
/*ARGSUSED*/
static int
f_Prnt_Mat_Db(HMitem *itemp, char **args)
{	static int	material_id = -1;
	if( (args == NULL || args[1] == NULL) && tty )
		{
		(void) sprintf( input_ln, "%d", material_id );
		(void) sprintf( prompt,
				"Material id ? [0-99 or * for all](%s) ",
				input_ln[0] == '-' ? "*" : input_ln
				);
		if(	get_Input( input_ln, MAX_LN, prompt ) != NULL
		    &&	sscanf( input_ln, "%d", &material_id ) != 1
			)
			{
			if( input_ln[0] == '*' )
				material_id = -1;
			}

		}
	else
	if(	args != NULL && args[1] != NULL
	    &&	sscanf( args[1], "%d", &material_id ) != 1 )
		{
		bu_log( "Illegal input (%s)\n", args[1] );
		return	-1;
		}
	if( mat_Print_Db( material_id ) && popups )
		wait_For_User();
	return	1;
	}

/*	f _ R d _ R a w _ I R ( ) */
/*ARGSUSED*/
static int
f_Rd_Raw_IR(HMitem *itemp, char **args)
{	FILE	*ir_fp;
	if( args != NULL && args[1] != NULL )
		(void) strncpy( ir_file, args[1], MAX_LN );
	else
	if( tty )
		{
		(void) sprintf( prompt,
				"Name of IR data file ? (%s) ",
				ir_file
				);
		if( get_Input( input_ln, MAX_LN, prompt ) != NULL )
			(void) strcpy( ir_file, input_ln );
		}
	else
		return	-1;
	if( ir_file[0] == '\0' )
		{
		bu_log( "No default, must specify file name!\n" );
		return	-1;
		}
	if( (ir_fp = fopen( ir_file, "r" )) == NULL )
		{
		bu_log( "Can not open \"%s\" for reading.\n", ir_file );
		return	-1;
		}
	if( ! ready_Output_Device( 0 ) )
		return	0;
	prnt_Event( "Reading IR data..." );
	if( ! read_IR( ir_fp ) )
		{
		bu_log(	"Read of IR data from file \"%s\" failed.\n",
			ir_file
			);
		(void) fclose( ir_fp );
		close_Output_Device( 0 );
		return	-1;
		}
	prnt_Timer( "IRREAD" );
	(void) fclose( ir_fp );
	close_Output_Device( 0 );
	return	1;
	}

static int
read_Frame(FILE *fp)
{	register int	i;
#if SINGLE_PRECISION
	if( fscanf( fp, "%e", &view_size ) != 1 )
#else
	if( fscanf( fp, "%le", &view_size ) != 1 )
#endif
		{
		bu_log( "Failed to read view size.\n" );
		if( ! movie.m_keys && fp != stdin )
			(void) fclose( fp );
		return	FALSE;
		}
#if SINGLE_PRECISION
	if( fscanf( fp, "%e %e %e", &lgts[0].loc[X], &lgts[0].loc[Y], &lgts[0].loc[Z] )
#else
	if( fscanf( fp, "%le %le %le", &lgts[0].loc[X], &lgts[0].loc[Y], &lgts[0].loc[Z] )
#endif
		!= 3
		)
		{
		bu_log( "Failed to read eye position.\n" );
		if( ! movie.m_keys && fp != stdin )
			(void) fclose( fp );
		return	FALSE;
		}
	for( i = 0; i < 16; i++ )
#if SINGLE_PRECISION
		if( fscanf( fp, "%e", &view_rots[i] ) != 1 )
#else
		if( fscanf( fp, "%le", &view_rots[i] ) != 1 )
#endif
			{
			bu_log( "Failed to read view matrix.\n" );
			save_view_flag = FALSE;
			if( ! movie.m_keys && fp != stdin )
				(void) fclose( fp );
			return	FALSE;
			}
	return	TRUE;
	}

/*	f _ M o v i e ( ) */
/*ARGSUSED*/
static int
f_Movie(HMitem *itemp, char **args)
{	int		ret = TRUE;
		char		buf[10];
		char		*locargs[3];
	/* Will use 'grid_sz' to control resolution. */
	force_cellsz = FALSE;
	(void) sprintf( prompt,
			"Full-screen frames ? [y|n](%c) ",
			movie.m_fullscreen ? 'y' : 'n'
			);
	if( get_Input( input_ln, MAX_LN, prompt ) != NULL )
		movie.m_fullscreen = input_ln[0] != 'n';
	if( movie.m_fullscreen )
		{
		(void) sprintf( prompt,
				"Output file prefix for frames ? (%s) ",
				prefix
				);
		if( get_Input( input_ln, MAX_LN, prompt ) != NULL )
			(void) strcpy( prefix, input_ln );
		}
	/* Setup default frame size to match grid size. */
	if( movie.m_frame_sz < 0 )
		movie.m_frame_sz = grid_sz;
	(void) sprintf( prompt, "Frame size ? (%d) ", movie.m_frame_sz );
	if(	get_Input( input_ln, MAX_LN, prompt ) != NULL
	    &&	sscanf( input_ln, "%d", &movie.m_frame_sz ) != 1
		)
		{
		ret = -1;
		goto	error_exit;
		}
	grid_sz = movie.m_frame_sz; /* Grid size must match. */
	setGridSize( grid_sz );

	locargs[1] = buf;
	locargs[1] = strtok( input_ln, " \t\n\r" );
	locargs[2] = NULL; /* XXX old-style grid configuration. */
	(void) f_GridConfig( (HMitem *) 0, locargs );
	(void) sprintf( prompt, "Number of frames ? (%d) ", noframes );
	if( get_Input( input_ln, MAX_LN, prompt ) != NULL )
		{
		if( sscanf( input_ln, "%d", &noframes ) != 1 )
			{
			ret = -1;
			goto	error_exit;
			}
		}
	movie.m_endframe = noframes-1; /* set default */
	movie.m_noframes = noframes;
	if( movie.m_noframes < 1 )
		{
		bu_log( "It doesn't make sense not to have atleast 2 frames.\n" );
		ret = -1;
		goto	error_exit;
		}
	(void) sprintf( prompt, "Starting frame ? (%d) ", movie.m_curframe );
	if(	get_Input( input_ln, MAX_LN, prompt ) != NULL
	    &&	sscanf( input_ln, "%d", &movie.m_curframe ) != 1
		)
		{
		ret = -1;
		goto	error_exit;
		}
	(void) sprintf( prompt, "Ending frame ? (%d) ", movie.m_endframe );
	if(	get_Input( input_ln, MAX_LN, prompt ) != NULL
	    &&	sscanf( input_ln, "%d", &movie.m_endframe ) != 1
		)
		{
		ret = -1;
		goto	error_exit;
		}
	(void) sprintf( prompt,
			"Key frame animation ? [y|n](%c) ",
			movie.m_keys ? 'y' : 'n'
			);
	if( get_Input( input_ln, MAX_LN, prompt ) != NULL )
		movie.m_keys = input_ln[0] != 'n';
	if( movie.m_keys )
		{	register int	i;
		(void) sprintf( prompt,
				"Name of key frame file ? (%s) ",
				svkey_file[0] == '\0' ? "STDIN" : svkey_file
				);
		if( get_Input( input_ln, MAX_LN, prompt) != NULL )
			(void) strcpy( svkey_file, input_ln );
		if( svkey_file[0] == '\0' )
			movie.m_keys_fp = NULL; /* Will use STDIN. */
		else
		if( (movie.m_keys_fp = fopen( svkey_file, "r" )) == NULL )
			{
			bu_log( "Can't open \"%s\" for reading.", svkey_file );
			ret = -1;
			goto	error_exit;
			}
		for( i = 0; i < movie.m_curframe; i++ )
			read_Frame( movie.m_keys_fp );
		}
	(void) sprintf( prompt,
			"Light sources tracking viewer ? [y|n](%c) ",
			movie.m_lgts ? 'y' : 'n'
			);
	if( get_Input( input_ln, MAX_LN, prompt ) != NULL )
		movie.m_lgts = input_ln[0] != 'n';
	if( movie.m_keys )
		return	TRUE;
	(void) sprintf( prompt,
			"Starting and ending view azimuth ? (%g %g) ",
			movie.m_azim_beg*DEGRAD, movie.m_azim_end*DEGRAD
			);
	if( get_Input( input_ln, MAX_LN, prompt) != NULL )
		{
#if SINGLE_PRECISION
		if( sscanf( input_ln, "%f %f", &movie.m_azim_beg, &movie.m_azim_end ) != 2 )
#else
		if( sscanf( input_ln, "%lf %lf", &movie.m_azim_beg, &movie.m_azim_end ) != 2 )
#endif
			{
			ret = -1;
			goto	error_exit;
			}
		else
			{
			movie.m_azim_beg /= DEGRAD;
			movie.m_azim_end /= DEGRAD;
			}
		}
	(void) sprintf( prompt,
			"Starting and ending view elevation ? (%g %g) ",
			movie.m_elev_beg*DEGRAD, movie.m_elev_end*DEGRAD
			);
	if( get_Input( input_ln, MAX_LN, prompt ) != NULL )
		{
#if SINGLE_PRECISION
		if( sscanf( input_ln, "%f %f", &movie.m_elev_beg, &movie.m_elev_end ) != 2 )
#else
		if( sscanf( input_ln, "%lf %lf", &movie.m_elev_beg, &movie.m_elev_end ) != 2 )
#endif
			{
			ret = -1;
			goto	error_exit;
			}
		else
			{
			movie.m_elev_beg /= DEGRAD;
			movie.m_elev_end /= DEGRAD;
			}
		}
	(void) sprintf( prompt,
			"Starting and ending view roll ? (%g %g) ",
			movie.m_roll_beg*DEGRAD, movie.m_roll_end*DEGRAD
			);
	if( get_Input( input_ln, MAX_LN, prompt ) != NULL )
		{
#if SINGLE_PRECISION
		if( sscanf( input_ln, "%f %f", &movie.m_roll_beg, &movie.m_roll_end ) != 2 )
#else
		if( sscanf( input_ln, "%lf %lf", &movie.m_roll_beg, &movie.m_roll_end ) != 2 )
#endif
			{
			ret = -1;
			goto	error_exit;
			}
		else
			{
			movie.m_roll_beg /= DEGRAD;
			movie.m_roll_end /= DEGRAD;
			}
		}
	(void) sprintf( prompt, "Manual viewer positioning ? [y|n](%c) ", movie.m_over ? 'y' : 'n' );
	if( get_Input( input_ln, MAX_LN, prompt ) != NULL )
		movie.m_over = input_ln[0] != 'n';
	if( movie.m_over )
		{
		(void) sprintf( prompt,
				"Starting and ending observer distance ? (%g %g) ",
				movie.m_dist_beg, movie.m_dist_end
				);
		if( get_Input( input_ln, MAX_LN, prompt ) != NULL )
			{
#if SINGLE_PRECISION
			if( sscanf( input_ln, "%f %f", &movie.m_dist_beg, &movie.m_dist_end ) != 2 )
#else
			if( sscanf( input_ln, "%lf %lf", &movie.m_dist_beg, &movie.m_dist_end ) != 2 )
#endif
				{
				ret = -1;
				goto	error_exit;
				}
			}
		(void) sprintf( prompt,
				"Starting and ending grid distance ? (%g %g) ",
				movie.m_grid_beg, movie.m_grid_end
				);
		if(	get_Input( input_ln, MAX_LN, prompt ) != NULL
#if SINGLE_PRECISION
		    &&	sscanf( input_ln, "%f %f", &movie.m_grid_beg, &movie.m_grid_end ) != 2 )
#else
		    &&	sscanf( input_ln, "%lf %lf", &movie.m_grid_beg, &movie.m_grid_end ) != 2 )
#endif
			{
			ret = -1;
			goto	error_exit;
			}
		}
	else
		{
		(void) sprintf( prompt,
				"Starting and ending perspective ? (%g %g) ",
				movie.m_pers_beg, movie.m_pers_end
				);
		if(	get_Input( input_ln, MAX_LN, prompt ) != NULL
#if SINGLE_PRECISION
		    &&	sscanf( input_ln, "%f %f", &movie.m_pers_beg, &movie.m_pers_end ) != 2
#else
		    &&	sscanf( input_ln, "%lf %lf", &movie.m_pers_beg, &movie.m_pers_end ) != 2
#endif
			)
			{
			ret = -1;
			goto	error_exit;
			}
		if( movie.m_pers_beg == 0.0 && movie.m_pers_end == 0.0 )
			{
			(void) sprintf( prompt,
				"Starting and ending grid distance ? (%g %g) ",
					movie.m_grid_beg, movie.m_grid_end
					);
			if(	get_Input( input_ln, MAX_LN, prompt ) != NULL
#if SINGLE_PRECISION
			    &&	sscanf( input_ln, "%f %f",
				&movie.m_grid_beg, &movie.m_grid_end ) != 2 )
#else
			    &&	sscanf( input_ln, "%lf %lf",
				&movie.m_grid_beg, &movie.m_grid_end ) != 2 )
#endif
				{
				ret = -1;
				goto	error_exit;
				}
			}
		}
error_exit :
	if( ret == -1 )
		{
		bu_log( "Illegal input (%s).\n", input_ln );
		movie.m_noframes = 1;
		}
	return	ret;
	}

/*	f _ M a x _ B o u n c e ( ) */
/*ARGSUSED*/
static int
f_Max_Bounce(HMitem *itemp, char **args)
{
	if( ir_mapping )
		{
		bu_log( "Multiple bounces disallowed during IR mapping.\n" );
		return	-1;
		}
	if( args == NULL )
		{
		(void) sprintf( prompt,
				"Maximum ray bounces ? (%d) ",
				max_bounce
				);
		if(	get_Input( input_ln, MAX_LN, prompt ) != NULL
		    &&	sscanf( input_ln, "%d", &max_bounce ) != 1
			)
			{
			bu_log( "f_Max_Bounce: Illegal input (%s)\n",
				input_ln
				);
			return	-1;
			}
		}
	else
	if( args[1] == NULL || sscanf( args[1], "%d", &max_bounce ) != 1 )
		return	-1;
	return	1;
	}

/*	f _ P r n t _ R e g i o n s ( )  */
/*ARGSUSED*/
static int
f_Prnt_Regions(HMitem *itemp, char **args)
{
	prnt_Trie( reg_triep, 0 );
	if( popups )
		wait_For_User();
	return	1;
	}

/*	f _ S e t _ R e g i o n _ I R ( ) */
/*ARGSUSED*/
static int
f_Set_Region_IR(HMitem *itemp, char **args)
{	register OcList	*ocp;
		OcList		*oclist;
		static char	reg_name[MAX_LN];
		static int	temperature = 0;
	oclist = get_Region_Name( reg_name, MAX_LN, "Region name ? " );
	(void) sprintf( prompt,
			"Temperature for region(s) ? (%d) ",
			temperature
			);
	if(	get_Input( input_ln, 12, prompt ) != NULL
	     &&	sscanf( input_ln, "%d", &temperature ) != 1
		)
		return	-1;
	for( ocp = oclist; ocp != OCLIST_NULL; ocp = ocp->p_next )
		ocp->p_octp->o_temp = temperature;
	delete_OcList( &oclist );
	return	1;
	}

/*	f _ E r r _ F i l e ( ) */
/*ARGSUSED*/
static int
f_Err_File(HMitem *itemp, char **args)
{	static int	err_fd = -1;
	if( args != NULL && args[1] != NULL )
		(void) strncpy( err_file, args[1], MAX_LN );
	else
	if( tty )
		{
		(void) sprintf( prompt, "Name of log file ? (%s) ", err_file );
		if( get_Input( input_ln, MAX_LN, prompt ) != NULL )
			(void) strcpy( err_file, input_ln );
		}
	else
		(void) strncpy( err_file, "/dev/tty", MAX_LN );

	if( err_file[0] == '\0' )
		{
		bu_log( "No default, must specify file name!\n" );
		return	-1;
		}
	/* Insure that error log is truncated. */
	if(	(err_fd =
		open( err_file, O_TRUNC|O_CREAT|O_WRONLY, 0644 )) == -1
		)
		{
		loc_Perror( err_file );
		return	-1;
		}
	(void) close( 2 );
	if( fcntl( err_fd, F_DUPFD, 2 ) == -1 )
		{
		loc_Perror( "fcntl" );
		return	-1;
		}
	return	1;
	}

/*	f _ S h a d o w s ( )
 */
/*ARGSUSED*/
static int
f_Shadows(HMitem *itemp, char **args)
{
	if( args == NULL )
		{
		(void) sprintf( prompt,
				"Shadows on ? [y|n](%c) ",
				shadowing ? 'y' : 'n'
				);
		if( get_Input( input_ln, MAX_LN, prompt ) != NULL )
			shadowing = input_ln[0] != 'n';
		}
	else
	if( args[1] == NULL || sscanf( args[1], "%d", &shadowing ) != 1 )
		{
		Toggle( shadowing );
		}
	return	1;
	}

/*	f _ T r a c k i n g _ C u r s o r ( ) */
/*ARGSUSED*/
static int
f_Tracking_Cursor(HMitem *itemp, char **args)
{
	if( args == NULL )
		{
		(void) sprintf( prompt,
				"Tracking cursor on ? [y|n](%c) ",
				tracking_cursor ? 'y' : 'n'
				);
		if( get_Input( input_ln, MAX_LN, prompt ) != NULL )
			tracking_cursor = input_ln[0] != 'n';
		}
	else
	if( args[1] == NULL || sscanf( args[1], "%d", &tracking_cursor ) != 1 )
		{
		Toggle( tracking_cursor );
		}
	return	1;
	}

/*	f _ P a r a l l e l ( ) */
/*ARGSUSED*/
static int
f_Parallel(HMitem *itemp, char **args)
{
#ifdef PARALLEL
		int maxpsw = bu_avail_cpus();
	if( maxpsw > MAX_PSW )
		maxpsw = MAX_PSW;
	if( args == NULL )
		{
		(void) sprintf( prompt,
				"Number of parallel processors ? [1 to %d](%d) ",
				maxpsw,
				npsw
				);
		if(	get_Input( input_ln, MAX_LN, prompt ) != NULL
		    &&	sscanf( input_ln, "%d", &npsw ) != 1
			)
			return	-1;
		}
	else
	if( args[1] == NULL || sscanf( args[1], "%d", &npsw ) != 1 )
		return	-1;
	npsw = npsw > maxpsw ? maxpsw : npsw;
	if( npsw > 1 )
		rt_g.rtg_parallel = 1;
	else
		rt_g.rtg_parallel = 0;

	bu_semaphore_init( RT_SEM_LAST );
#endif
	return	1;
	}

/*	f _ W r t _ I R _ D b ( ) */
/*ARGSUSED*/
static int
f_Wrt_IR_Db(HMitem *itemp, char **args)
{	FILE		*ir_fp;
	if( args != NULL && args[1] != NULL )
		(void) strncpy( ir_db_file, args[1], MAX_LN );
	else
	if( tty )
		{
		(void) sprintf( prompt,
				"Name for IR data base file ? (%s) ",
				ir_db_file
				);
		if( get_Input( input_ln, MAX_LN, prompt ) != NULL )
			(void) strcpy( ir_db_file, input_ln );
		}
	else
		return	-1;
	if( ir_db_file[0] == '\0' )
		{
		bu_log( "No default, must specify file name!\n" );
		return	-1;
		}
	if( (ir_fp = fopen( ir_db_file, "w" )) == NULL )
		{
		bu_log( "Can not open \"%s\" for writing.\n", ir_db_file );
		return	-1;
		}
	prnt_Event( "Writing IR data base..." );
	if( ! write_Trie( reg_triep, 0, ir_fp ) )
		{
		bu_log(	"Write of IR data base (%s) failed.\n",
			ir_db_file
			);
		(void) fclose( ir_fp );
		return	-1;
		}
	(void) fclose( ir_fp );
	return	1;
	}

/*	f _ W r t _ L g t _ D b ( ) */
/*ARGSUSED*/
static int
f_Wrt_Lgt_Db(HMitem *itemp, char **args)
{
 	if( args != NULL && args[1] != NULL )
		(void) strncpy( lgt_db_file, args[1], MAX_LN );
	else
	if( tty )
		{
		(void) sprintf( prompt,
				"Name for light source data base file ? (%s) ",
				lgt_db_file
				);
		if( get_Input( input_ln, MAX_LN, prompt ) != NULL )
			(void) strcpy( lgt_db_file, input_ln );
		}
	else
		return	-1;
	if( lgt_db_file[0] == '\0' )
		{
		bu_log( "No default, must specify file name!\n" );
		return	-1;
		}
	if( ! lgt_Save_Db( lgt_db_file ) )
		{
		bu_log( "Write of light source data base (%s) failed.\n", lgt_db_file );
		return	-1;
		}
	return	1;
	}

/*	f _ W r t _ M a t _ D b ( ) */
/*ARGSUSED*/
static int
f_Wrt_Mat_Db(HMitem *itemp, char **args)
{
	if( args != NULL && args[1] != NULL )
		(void) strncpy( mat_db_file, args[1], MAX_LN );
	else
	if( tty )
		{
		(void) sprintf( prompt,
				"Name for material data base file ? (%s) ",
				mat_db_file
				);
		if( get_Input( input_ln, MAX_LN, prompt ) != NULL )
			(void) strcpy( mat_db_file, input_ln );
		}
	else
		return	-1;
	if( mat_db_file[0] == '\0' )
		{
		bu_log( "No default, must specify file name!\n" );
		return	-1;
		}
	return	mat_Save_Db( mat_db_file ) ? 1 : -1;
	}

/*	f _ G r i d _ T r a n s l a t e ( ) */
/*ARGSUSED*/
static int
f_Grid_Translate(HMitem *itemp, char **args)
{
#if SINGLE_PRECISION
	if(	args == NULL || args[1] == NULL || args[2] == NULL
	    ||	sscanf( args[1], "%f", &x_grid_offset ) != 1
	    ||	sscanf( args[2], "%f", &y_grid_offset ) != 1
#else
	if(	args == NULL || args[1] == NULL || args[2] == NULL
	    ||	sscanf( args[1], "%lf", &x_grid_offset ) != 1
	    ||	sscanf( args[2], "%lf", &y_grid_offset ) != 1
#endif
		)
		{
		if( tty )
			{
			(void) sprintf( prompt,
					"Grid translation ? [x y](%g %g)",
					x_grid_offset, y_grid_offset
					);
			if(	get_Input( input_ln, MAX_LN, prompt ) != NULL
#if SINGLE_PRECISION
			   &&	sscanf( input_ln, "%f %f", &x_grid_offset, &y_grid_offset ) != 2
#else
			   &&	sscanf( input_ln, "%lf %lf", &x_grid_offset, &y_grid_offset ) != 2
#endif
				)
				return	-1;
			}
		else
			return	-1;
		}
	return	1;
	}

/*ARGSUSED*/
static int
f_Overlaps(HMitem *itemp, char **args)
{
	if( args == NULL )
		{
		(void) sprintf( prompt,
				"Overlap reporting on ? [y|n](%c) ",
				report_overlaps ? 'y' : 'n'
				);
		if( get_Input( input_ln, MAX_LN, prompt ) != NULL )
			report_overlaps = input_ln[0] != 'n';
		}
	else
	if( args[1] == NULL || sscanf( args[1], "%d", &report_overlaps ) != 1 )
		{
		Toggle( report_overlaps );
		}
	if( ! tty )
		bu_log(	"Overlaps will%sbe reported.\n",
			report_overlaps ? " " : " NOT "
			);
	return	1;
	}

/*	f _ S h o w _ I R ( ) */
/*ARGSUSED*/
static int
f_Show_IR(HMitem *itemp, char **args)
{	int	x, y;
	if( args == NULL || args[1] == NULL )
		{
		x = grid_sz / 8;
		y = 0;
		}
	else
	if( sscanf( args[1], "%d", &x ) != 1 )
		return	-1;
	else
	if( args[2] == NULL )
		y = 0;
	else
	if( sscanf( args[2], "%d", &y ) != 1 )
		return	-1;
	/* Display IR temperature scale for pseudo coloring. */
	if( ! ready_Output_Device( 0 ) )
		return	-1;
	display_Temps( x, y );
	close_Output_Device( 0 );
	return	1;
	}

/*	f _ B a c k g r o u n d ( ) */
/*ARGSUSED*/
static int
f_Background(HMitem *itemp, char **args)
{
	if(	args == NULL || args[1] == NULL
	     ||	args[2] == NULL
	     ||	args[3] == NULL
	     ||	sscanf( args[1], "%d", &background[0] ) != 1
	     ||	sscanf( args[2], "%d", &background[1] ) != 1
	     || sscanf( args[3], "%d", &background[2] ) != 1
		)
		{
		if( tty )
			{
			(void) sprintf( prompt,
					"Background color ? [R G B](%d %d %d) ? ",
					background[RED],
					background[GRN],
					background[BLU]
					);
			if(	get_Input( input_ln, MAX_LN, prompt ) != NULL
			   &&	sscanf( input_ln,
					"%d %d %d",
					&background[0],
					&background[1],
					&background[2]
					) != 3 )
				return	-1;
			}
		else
			return	-1;
		}
	bg_coefs[0] = (fastf_t) background[0] / 255.0;
	bg_coefs[1] = (fastf_t) background[1] / 255.0;
	bg_coefs[2] = (fastf_t) background[2] / 255.0;
	COPYRGB( bgpixel, background );
	return	1;
	}

HMitem	irflags_items[] =
	{
	{ "read only", "data base will NOT be modified by raytracing", 0, 0, 0, 0, IR_READONLY },
	{ "edit", "data base will be modified by raytracing", 0, 0, 0, 0, IR_EDIT },
	{ "octree rendering", "DEBUG: image of octree data structure itself", 0, 0, 0, 0, IR_OCTREE },
	{ "reset all flags", "leave infrared module", 0, 0, 0, 0, IR_OFF },
	{ 0, 0, 0, 0, 0, 0, 0 }
	};
HMenu	irflags_hmenu = { irflags_items, 0, 0, 0, 0, 0 };

void
set_IRmapping(int bitflag)
{
	switch( bitflag )
		{
	case IR_OFF :
		ir_mapping = 0;
		break;
	case IR_READONLY :
		ir_mapping |= IR_READONLY;
		ir_mapping &= ~IR_EDIT;
		break;
	case IR_EDIT :
		ir_mapping |= IR_EDIT;
		ir_mapping &= ~IR_READONLY;
		break;
	case IR_OCTREE :
		ir_mapping |= IR_OCTREE;
		break;
		}
	return;
	}

void
note_IRmapping(void)
{
	if( ir_mapping == IR_OFF )
		{
		prnt_Scroll( "Leaving IR module.\n" );
		return;
		}
	max_bounce = 0;
	if( ir_mapping & IR_READONLY )
		prnt_Scroll( "IR module: READONLY mode.\n" );
	if( ir_mapping & IR_EDIT )
		prnt_Scroll( "IR module: EDIT mode.\n" );
	if( ir_mapping & IR_OCTREE )
		prnt_Scroll( "IR module: picture will be OCTREE rendering.\n" );
	return;
	}

/*	f _ I R m o d u l e ( ) */
/*ARGSUSED*/
static int
f_IRmodule(HMitem *itemp, char **args)
{	int	menuval = -1;
	if( args == NULL || args[1] == NULL )
		{	HMitem	*itemptr;
		if( ! tty )
			{
			bu_log( "IR module command (%c): missing argument\n",
				*args[0] );
			return	-1;
			}
#if HAS_SGIGL
		if( sgi_usemouse )
			{	extern long	irflags_menu;
			menuval = dopup( irflags_menu );
			}
		else
#endif
		if( (itemptr = hmenuhit( &irflags_hmenu, 1, TOP_SCROLL_WIN ))
			!= (HMitem *) 0
			)
			menuval = itemptr->data;
		if( menuval < 0 )
			return	-1;
		set_IRmapping( menuval );
		}
	else
	if( sscanf( args[1], "%d", &ir_mapping ) != 1 )
		return	-1;
	note_IRmapping();
	return	1;
	}

/*	f _ R d _ I R _ D b ( ) */
/*ARGSUSED*/
static int
f_Rd_IR_Db(HMitem *itemp, char **args)
{	FILE	*ir_fp;
	if( args != NULL && args[1] != NULL )
		(void) strncpy( ir_db_file, args[1], MAX_LN );
	else
	if( tty )
		{
		(void) sprintf( prompt,
				"Name of IR data base file ? (%s) ",
				ir_db_file
				);
		if( get_Input( input_ln, MAX_LN, prompt ) != NULL )
			(void) strcpy( ir_db_file, input_ln );
		}
	if( ir_db_file[0] == '\0' )
		{
		bu_log( "No default, must specify file name!\n" );
		return	-1;
		}
	if( (ir_fp = fopen( ir_db_file, "r" )) == NULL )
		{
		bu_log( "Can not open \"%s\" for reading.\n", ir_db_file );
		return	-1;
		}
	if( ! ready_Output_Device( 0 ) )
		return	-1;
	prnt_Event( "Reading IR data base..." );
	if( ! read_Trie( ir_fp ) )
		{
		bu_log(	"Read of IR data base (%s) failed.\n",
			ir_db_file
			);
		(void) fclose( ir_fp );
		close_Output_Device( 0 );
		return	-1;
		}
	(void) fclose( ir_fp );
	close_Output_Device( 0 );
	return	1;
	}

/*	f _ R d _ L g t _ D b ( ) */
/*ARGSUSED*/
static int
f_Rd_Lgt_Db(HMitem *itemp, char **args)
{
	if( args != NULL && args[1] != NULL )
		(void) strncpy( lgt_db_file, args[1], MAX_LN );
	else
	if( tty )
		{
		(void) sprintf( prompt,
				"Name of light source data base file ? (%s) ",
				lgt_db_file
				);
		if( get_Input( input_ln, MAX_LN, prompt ) != NULL )
			(void) strcpy( lgt_db_file, input_ln );
		}
	if( lgt_db_file[0] == '\0' )
		{
		bu_log( "No default, must specify file name!\n" );
		return	-1;
		}
	if( ! lgt_Rd_Db( lgt_db_file ) )
		{
		bu_log( "Read of light data base (%s) failed.\n", lgt_db_file );
		return	-1;
		}
	return	1;
	}

/*	f _ R d _ M a t _ D b ( ) */
/*ARGSUSED*/
static int
f_Rd_Mat_Db(HMitem *itemp, char **args)
{
	if( args != NULL && args[1] != NULL )
		(void) strncpy( mat_db_file, args[1], MAX_LN );
	else
	if( tty )
		{
		(void) sprintf( prompt,
				"Name of material data base file ? (%s) ",
				mat_db_file
				);
		if( get_Input( input_ln, MAX_LN, prompt ) != NULL )
			(void) strcpy( mat_db_file, input_ln );
		}
	if( mat_db_file[0] == '\0' )
		{
		bu_log( "No default, must specify file name!\n" );
		return	-1;
		}
	if( ! mat_Rd_Db( mat_db_file ) )
		{
		bu_log( "Read of material data base (%s) failed.\n", mat_db_file );
		return	-1;
		}
	return	1;
	}

/*	f _ I R _ O f f s e t ( ) */
/*ARGSUSED*/
static int
f_IR_Offset(HMitem *itemp, char **args)
{
	if(	args == NULL || args[1] == NULL || sscanf( args[1], "%d", &ir_mapx ) != 1
	     ||	args[2] == NULL || sscanf( args[2], "%d", &ir_mapy ) != 1
		)
		ir_offset = FALSE;
	else
		ir_offset = TRUE;
	return	1;
	}

/*	f _ D i s t _ G r i d ( ) */
/*ARGSUSED*/
static int
f_Dist_Grid(HMitem *itemp, char **args)
{
	if(	args != NULL && args[1] != NULL
#if SINGLE_PRECISION
	    &&	sscanf( args[1], "%f", &grid_dist ) != 1
#else
	    &&	sscanf( args[1], "%lf", &grid_dist ) != 1
#endif
		)
		{
		bu_log( "f_Dist_Grid : Illegal input.\n" );
		return	-1;
		}
	else
	if( args == NULL )
		{
		if( tty )
			{
			(void) sprintf( prompt,
					"Distance of grid from model centroid ? (%g) ",
					grid_dist
					);
			if(	get_Input( input_ln, MAX_LN, prompt ) != NULL
#if SINGLE_PRECISION
			    &&	sscanf( input_ln, "%f", &grid_dist ) != 1
#else
			    &&	sscanf( input_ln, "%lf", &grid_dist ) != 1
#endif
				)
				{
				bu_log( "f_Dist_Grid : Illegal input (%s).\n",
					input_ln );
				return	-1;
				}
			}
		else
			return	-1;
		}
	grid_position = TRUE;
	return	1;
	}

/*	f _ S c a l e _ G r i d ( ) */
/*ARGSUSED*/
static int
f_Scale_Grid(HMitem *itemp, char **args)
{
	bu_log( "The -g option is obsolete, use -G to set view size.\n" );
	return	1;
	}

/*	f _ I R _ N o i s e ( ) */
/*ARGSUSED*/
static int
f_IR_Noise(HMitem *itemp, char **args)
{
	if(	args == NULL || args[1] == NULL
	    ||	sscanf( args[1], "%d", &ir_noise ) != 1
		)
		{
		if( tty )
			{
			(void) sprintf( prompt,
					"Temperature threshold for region sub-division ? (%d) ",
					ir_noise
					);
			if(	get_Input( input_ln, MAX_LN, prompt ) != NULL
			   &&	sscanf( input_ln, "%d", &ir_noise ) != 1
				)
				return	-1;
			}
		else
			return	-1;
		}
	return	1;
	}

/*	f _ K e y _ F r a m e ( )
	Apply saved view output by MGED(1B) "saveview" and "svkey" commands.
	Some of this code was originally written by Mike J. Muuss for
	his RT(1B) ray-tracing front-end.
 */
/*ARGSUSED*/
static int
f_Key_Frame(HMitem *itemp, char **args)
{	fastf_t		model2view[16], to_eye[16];
		FILE		*svkey_fp=NULL;
	if( args != NULL && args[1] != NULL )
		{
		(void) strncpy( svkey_file, args[1], MAX_LN );
		if( (svkey_fp = fopen( svkey_file, "r" )) == NULL )
			{
			bu_log( "Can't open \"%s\" for reading.", svkey_file );
			return	-1;
			}
		}
	else
	if( tty && ! movie.m_keys )
		{	int abortsvkey;
		(void) sprintf( prompt, "Use key frame file ? [y|n](%c) ",
				svkey_file[0] == '\0' ? 'n' : 'y' );
		if( get_Input( input_ln, MAX_LN, prompt ) != NULL )
			abortsvkey = input_ln[0] == 'n';
		else
			abortsvkey = svkey_file[0] == '\0';
		if( abortsvkey )
			{
			svkey_file[0] = '\0'; /* disable reading of key frame */
			save_view_flag = FALSE;
			return	1;
			}
		(void) sprintf( prompt,
				"Name of key frame file ? (%s) ",
				svkey_file
				);
		if( get_Input( input_ln, MAX_LN, prompt ) != NULL )
			(void) strcpy( svkey_file, input_ln );
		if( svkey_file[0] == '\0' )
			{
			bu_log( "No default, must specify file name!\n" );
			return	-1;
			}
		if( (svkey_fp = fopen( svkey_file, "r" )) == NULL )
			{
			bu_log( "Can't open \"%s\" for reading.", svkey_file );
			return	-1;
			}
		}
	else
	if( movie.m_keys )
		{
		if( movie.m_keys_fp == NULL )
			svkey_fp = stdin;
		else
			svkey_fp = movie.m_keys_fp;
		}
	else
	if( args != NULL && args[1] == NULL && ! tty )
		{
		svkey_file[0] = '\0'; /* disable reading of key frame */
		save_view_flag = FALSE;
		return	1;
		}
	if( ! read_Frame( svkey_fp ) )
		return	-1;

	/* Compute view-to-model rotation matrix. */
	MAT_IDN( to_eye );
	to_eye[MDX] = -lgts[0].loc[X];
	to_eye[MDY] = -lgts[0].loc[Y];
	to_eye[MDZ] = -lgts[0].loc[Z];
	bn_mat_mul( model2view, view_rots, to_eye );
	bn_mat_inv( view2model, model2view );
	save_view_flag = TRUE;
	force_viewsz = TRUE;
	if( ! movie.m_keys && svkey_fp != stdin )
		(void) fclose( svkey_fp );
	return	1;
	}

/*ARGSUSED*/
static int
f_Hidden_Ln_Draw(HMitem *itemp, char **args)
{
	if( args == NULL )
		{
		(void) sprintf( prompt,
				"Hidden line drawing ? [y|n](%c) ",
				hiddenln_draw ? 'y' : 'n'
				);
		if( get_Input( input_ln, MAX_LN, prompt ) != NULL )
			hiddenln_draw = input_ln[0] != 'n';
		if( hiddenln_draw == HL_ENABLED )
			{
			(void) sprintf( prompt,
					"Reverse video ? [y|n](%c) ",
					reverse_video ? 'y' : 'n'
					);
			if( get_Input( input_ln, MAX_LN, prompt ) != NULL )
				reverse_video = input_ln[0] != 'n';
			}
		}
	else
	if( args[1] == NULL || sscanf( args[1], "%d", &hiddenln_draw ) != 1 )
		{
		switch( hiddenln_draw )
			{
		case HL_ENABLED :
		case HL_REVERSE_VIDEO :
			hiddenln_draw = HL_DISABLED;
			break;
		case HL_DISABLED :
			hiddenln_draw = reverse_video ?
				HL_REVERSE_VIDEO : HL_ENABLED;
			break;
		default :
			bu_log( "Illegal value (%d) for hidden line flag.\n",
				hiddenln_draw );
			return	-1;
			}
		}
	if( hiddenln_draw == HL_REVERSE_VIDEO )
		reverse_video = TRUE;
	return	1;
	}

/*	f _ E n t r _ L g t _ D b ( ) */
/*ARGSUSED*/
static int
f_Entr_Lgt_Db(HMitem *itemp, char **args)
{	static int	light_id = 0;
#if HAS_SGIGL
	if( (args == NULL || args[1] == NULL) && sgi_usemouse )
		{	extern long	lgts_edit_menu;
			int		rc;
		if( (rc = dopup( lgts_edit_menu )) >= 0 )
			light_id = rc;
		}
	else
#endif
	if( (args == NULL || args[1] == NULL ) && tty )
		{
		(void) sprintf( input_ln, "%d", light_id );
		(void) sprintf( prompt,
				"Light source id ? [0 to %d](%s) ",
				MAX_LGTS-1,
				input_ln
				);
		if(	(get_Input( input_ln, MAX_LN, prompt ) != NULL
		&&	sscanf( input_ln, "%d", &light_id ) != 1)
		     ||	light_id < 0 || light_id >= MAX_LGTS
			)
			{
			bu_log( "Illegal input (%s)\n", input_ln );
			return	-1;
			}
		}
	else
	if(	args != NULL && args[1] != NULL
	    &&	sscanf( args[1], "%d", &light_id ) != 1
		)
		{
		bu_log( "Illegal input (%s)\n", args[1] );
		return	-1;
		}
	return	lgt_Edit_Db_Entry( light_id );
	}

/*	f _ E n t r _ M a t _ D b ( ) */
/*ARGSUSED*/
static int
f_Entr_Mat_Db(HMitem *itemp, char **args)
{	static int	material_id = 0;
	if( (args == NULL || args[1] == NULL) && tty )
		{
		(void) sprintf( input_ln, "%d", material_id );
		(void) sprintf( prompt,
				"Material id ? [0 to 99](%s) ",
				input_ln
				);
		if(	get_Input( input_ln, MAX_LN, prompt ) != NULL
		    &&	sscanf( input_ln, "%d", &material_id ) != 1
			)
			{
			bu_log( "Illegal input (%s)\n", input_ln );
			return	-1;
			}

		}
	else
	if(	args != NULL && args[1] != NULL
	    &&	sscanf( args[1], "%d", &material_id ) != 1
		)
		{
		bu_log( "Illegal input (%s)\n", args[1] );
		return	-1;
		}
	return	mat_Edit_Db_Entry( material_id ) ? 1 : -1;
	}

/*	f _ S e t _ I R _ P a i n t ( ) */
/*ARGSUSED*/
static int
f_Set_IR_Paint(HMitem *itemp, char **args)
{
#if HAS_SGIGL
	if( (args == NULL || args[1] == NULL) && sgi_usemouse )
		{	extern long	irpaint_menu;
		ir_doing_paint = dopup( irpaint_menu );
		if( ir_doing_paint && tty )
			{
			(void) sprintf( prompt,
					"Temperature ? (%d) ",
					ir_paint
					);
			if(	get_Input( input_ln, MAX_LN, prompt ) != NULL
			   &&	sscanf( input_ln, "%d", &ir_paint ) != 1
				)
				{
				ir_doing_paint = FALSE;
				return	-1;
				}
			}
		else
			return	-1;

		}
	else
#endif
	if( args == NULL || args[1] == NULL || sscanf( args[1], "%d", &ir_paint ) != 1 )
		ir_doing_paint = FALSE;
	else
		ir_doing_paint = TRUE;
	return	1;
	}

/*	f _ R a s t e r _ F i l e ( ) */
/*ARGSUSED*/
static int
f_Raster_File(HMitem *itemp, char **args)
{
	if( args != NULL && args[1] != NULL )
		(void) strncpy( fb_file, args[1], MAX_LN );
	else
	if( tty )
		{
		(void) sprintf( prompt,
				"Frame buffer name ? (%s) ",
				fb_file
				);
		if( get_Input( input_ln, MAX_LN, prompt ) != NULL )
			(void) strcpy( fb_file, input_ln );
		}
	else
		fb_file[0] = '\0';
	return	1;
	}

/*	f _ P e r s p e c t i v e ( ) */
/*ARGSUSED*/
static int
f_Perspective(HMitem *itemp, char **args)
{
#if SINGLE_PRECISION
	if( args == NULL || args[1] == NULL || sscanf( args[1], "%f", &rel_perspective ) != 1 )
#else
	if( args == NULL || args[1] == NULL || sscanf( args[1], "%lf", &rel_perspective ) != 1 )
#endif
		{
		if( tty )
			{
			(void) sprintf( prompt,
					"Perspective factor ? [-1.0 to disable](%g) ",
					rel_perspective
					);
			if(	get_Input( input_ln, MAX_LN, prompt ) != NULL
#if SINGLE_PRECISION
			    &&	sscanf( input_ln, "%f", &rel_perspective ) != 1
#else
			    &&	sscanf( input_ln, "%lf", &rel_perspective ) != 1
#endif
				)
				return	-1;
			}
		else
			return	-1;
		}
	return	1;
	}

/*	f _ Q u i t ( ) */
/*ARGSUSED*/
static int
f_Quit(HMitem *itemp, int args)
{
	exit_Neatly( 0 );
	/*NOTREACHED*/
	return( 0 ); /* for the compiler */
	}

/*	f _ R e d r a w ( ) */
/*ARGSUSED*/
static int
f_Redraw(HMitem *itemp, char **args)
{
	if( tty )
		{
		(void) ClrText();
		init_Status();
		prnt_Status();
		hmredraw();
		}
	return	1;
	}

/*	f _ G r i d _ X _ P o s  ( ) */
/*ARGSUSED*/
int
f_Grid_X_Pos(HMitem *itemp, char **args)
{
	if(	args != NULL && args[1] != NULL && args[2] != NULL
	    &&	(	sscanf( args[1], "%d", &grid_x_org ) != 1
		    ||	sscanf( args[2], "%d", &grid_x_fin ) != 1
		)
		)
		{
		bu_log( "f_Grid_X_Pos : Illegal input.\n" );
		return	-1;
		}
	else
	if( args == NULL )
		{
		(void) sprintf( prompt,
				"Horizontal grid indices ? [begin end](%d %d) ",
				grid_x_org, grid_x_fin
				);
		if(	get_Input( input_ln, MAX_LN, prompt ) != NULL
		    &&	sscanf( input_ln, "%d %d", &grid_x_org, &grid_x_fin )
			!= 2
			)
			{
			bu_log( "Illegal input (%s).\n", input_ln );
			return	-1;
			}
		}
	if( tty )
		{
		(void) sprintf( scratchbuf, " [%04d-", grid_x_org );
		(void) strncpy( GRID_PIX_PTR,
				scratchbuf, strlen( scratchbuf ) );
		(void) sprintf( scratchbuf, "%04d,", grid_x_fin );
		(void) strncpy( GRID_SIZ_PTR,
				scratchbuf, strlen( scratchbuf ) );
		}
	grid_x_cur = grid_x_org;
	return	1;
	}

/*	f _ G r i d _ Y _ P o s  ( ) */
/*ARGSUSED*/
int
f_Grid_Y_Pos(HMitem *itemp, char **args)
{
	if(	args != NULL && args[1] != NULL && args[2] != NULL
	    &&	(	sscanf( args[1], "%d", &grid_y_org ) != 1
		    ||	sscanf( args[2], "%d", &grid_y_fin ) != 1
		)
		)
		{
		bu_log( "f_Grid_Y_Pos : Illegal input.\n" );
		return	-1;
		}
	else
	if( args == NULL )
		{
		(void) sprintf( prompt,
				"Vertical grid indices ? [begin end](%d %d) ",
				grid_y_org, grid_y_fin
				);
		if(	get_Input( input_ln, MAX_LN, prompt ) != NULL
		   &&	sscanf( input_ln, "%d %d", &grid_y_org, &grid_y_fin )
			!= 2
			)
			{
			bu_log( "Illegal input (%s).\n", input_ln );
			return	-1;
			}
		}
	if( tty )
		{
		(void) sprintf( scratchbuf, "%04d-", grid_y_org );
		(void) strncpy( GRID_SCN_PTR,
				scratchbuf, strlen( scratchbuf ) );
		(void) sprintf( scratchbuf, "%04d:", grid_y_fin );
		(void) strncpy( GRID_FIN_PTR,
				scratchbuf, strlen( scratchbuf ) );
		}
	grid_y_cur = grid_y_org;
	return	1;
	}

/*	f _ F b c l e a r ( ) */
/*ARGSUSED*/
static int
f_Fbclear(HMitem *itemp, char **args)
{	static RGBpixel	black = { 0, 0, 0 };
	if( ! ready_Output_Device( 0 ) )
		return	-1;
	(void) fb_clear( fbiop, black );
	close_Output_Device( 0 );
	return	1;
	}

/*	f _ E x e c _ S h e l l ( ) */
/*ARGSUSED*/
static int
f_Exec_Shell(HMitem *itemp, char **args)
{	register int	i;
		int	exit_status;
	if( args == NULL )
		{
		if( (args = (char **) malloc( sizeof(char *)*2 )) == (char **) 0 )
			{
			Malloc_Bomb( sizeof(char *)*2 );
			return	-1;
			}
		args[0] = "!";
		args[1] = NULL;
		}
	/* Skip leading blanks. */
	while( *args[0] == ' ' )
		++(args[0]);
exec_start :
	if( (args[0])[1] == '\0' && args[1] == NULL )
		{
		if( tty )
			{
			if(	get_Input( input_ln, BUFSIZ, "Command line : " ) == NULL
			    ||	(args[0] = strtok( input_ln, " \t" )) == NULL
				)
				return	-1;
			for( i = 1; args[i-1] != NULL ; ++i )
				args[i] = strtok( (char *) NULL, " \t" );
			}
		else
			return	-1;
		}
	else
	if( (args[0])[1] != '\0' )
		/* Command immediately follows the '!'. */
		++(args[0]);
	else
		++args;
	if( tty )
		{
		EVENT_MOVE();
		(void) fflush( stdout );
		}
	exit_status = exec_Shell( args );
	if( tty )
		{
		prnt_Scroll( "\n" );
		prnt_Scroll( "\n" );
		EVENT_MOVE();
		(void) fputs( "\"", stdout );
		for( i = 0; args[i] != NULL; ++i )
			(void) printf( "%s ", args[i] );
		(void) fputs( "\" ", stdout );
		if( exit_status )
			(void) printf( "failed (exit status %d).", exit_status );
		else
			(void) fputs( "completed successfully.", stdout );
		(void) get_Input( input_ln, BUFSIZ, "Continue ? [y|n](y) " );
		if( input_ln[0] == 'n' )
			{
			args[0] = "!";
			args[1] = NULL;
			goto	exec_start;
			}
		(void) f_Redraw( (HMitem *) 0, (char **) 0 );
		}
#if HAS_SGIGL
	if( SGI_USEKEYBD )
		{
		qreset();
		winattach();
		}
#endif
	return	1;
	}

HMitem	pref_items[] =
	{
	{ "frame buffer size",
		"Set size of display window, should be multiple of grid size.",
		0, 0, 0, f_SetFbSize, 0 },
	{ "background color", "no help", 0, 0, 0, f_Background, 0 },
	{ "buffering", "Select mode of buffering of pixels I/O.",
		0, 0, 0, f_Buffer, 0 },
	{ "debugging", "Not for general use.", 0, 0, 0, f_Debug, 0 },
	{ "maximum ray bounces", "Set recursion level.",
		0, 0, 0, f_Max_Bounce, 0 },
	{ "overlap reporting", "Disable or enable overlap diagnostics.",
		0, 0, 0, f_Overlaps, 0 },
	{ "parallel processors",
		"Limit the number of processors used on a parallel machine",
		0, 0, 0, f_Parallel, 0 },
	{ "tracking cursor", "Points to scan line being ray traced.",
		0, 0, 0, f_Tracking_Cursor, 0 },
	{ "shadows", "Disable or enable shadows.", 0, 0, 0, f_Shadows, 0 },
	{ 0, 0, 0, 0, 0, 0, 0 }
	};
HMenu	pref_hmenu = { pref_items, 0, 0, 0, 1, 0 };

HMitem	anim_items[] =
	{
	{ "generate movie", "specify animation sequence", 0, 0, 0, f_Movie, 0 },
	{ "display movie", "watch pre-computed movie", 0, 0, 0, f_Animate, 0 },
	{ 0, 0, 0, 0, 0, 0, 0 }
	};
HMenu	anim_hmenu = { anim_items, 0, 0, 0, 0, 0 };

HMitem	main_items[] =
	{
	{ "quit", "Exit program", &quit_hmenu, 0, 0, 0, 0 },
	{ "ray trace", "no help", &ray_trace_hmenu, 0, 0, 0, 0 },
	{ "gridding parameters", "no help", &grid_hmenu, 0, 0, 0, 0 },
	{ "clear frame buffer", "Erase graphics.", 0, 0, 0, f_Fbclear, 0 },
	{ "redraw text", "Repaint text screen.", 0, 0, 0, f_Redraw, 0 },
	{ "light sources", "Edit or print light source data base.", &light_src_hmenu, 0, 0, 0, 0 },
	{ "materials", "Edit or print material reflectance properties data base.", &materials_hmenu, 0, 0, 0, 0 },
	{ "files", "no help", &file_hmenu, 0, 0, 0, 0 },
	{ "preferences", "Set options relating to program behavior", &pref_hmenu, 0, 0, 0, 0 },
	{ "cursor input", "Interact with graphics display directly.", 0, 0, 0, f_Cursor_Module, 0 },
	{ "animation", "Generate or display a movie.", &anim_hmenu,  0, 0, 0, 0 },
	{ "special applications", "Non-lighting model images", &special_hmenu, 0, 0, 0, 0 },
	{ "shell escape", "Execute a UNIX shell-level command.", 0, 0, 0, f_Exec_Shell, 0 },
	{ 0, 0, 0, 0, 0, 0, 0 }
	};
HMenu	main_hmenu = { main_items, 0, 0, 0, 1, prnt_Status };
void
user_Interaction(void)
{	static char	*local_argv[MAX_ARGS];
		register int	ret;
	local_argv[1] = NULL; /* Truncate argument list initially. */
	/* Set up user options. */
#if HAS_SGIGL
	if( sgi_usemouse )
		sgi_User_Input( local_argv );
	else
#endif
menuinput :
	if( tty )
		{
		prnt_Status();
		popups = TRUE;
		prnt_Event( "Type 'q' to leave popup menu." );
		(void) hmenuhit( &main_hmenu, 1, TOP_SCROLL_WIN );
		}
	do
		{
		if( tty )
			prnt_Status();
		popups = FALSE;
		user_Input( local_argv );
		}
	while( (ret = user_Cmd( local_argv )) && ret != -2 );
	if( ret == -2 )
		goto	menuinput;
	return;
	}

static fastf_t	x_axis[3] = { 1.0, 0.0, 0.0 };
static fastf_t	neg_x_axis[3] = { -1.0, 0.0, 0.0 };
static fastf_t	y_axis[3] = { 0.0, 1.0, 0.0 };
static fastf_t	z_axis[3] = { 0.0, 0.0, 1.0 };
static fastf_t	neg_z_axis[3] = { 0.0, 0.0, -1.0 };

/*
	int setup_Lgts( int frame )

	Compute locations of light sources, observer, and grid.
 */
int
setup_Lgts(int frame)
{	register int i;
		struct soltab *eye_stp = NULL;  /* Initialize to shut up the */
		struct soltab *grid_stp = NULL; /* stupid Cray compiler. */
	prnt_Event( "Setting up light sources..." );
	lgts[0].stp = NULL; /* flags use of eye solid */

	/* Compute observer (ambient light) position in model space. */
	if( save_view_flag )
		{
		/* Use view matrix to specify gridding. */
		/* Unit vector from center of model to eye. */
		MAT4X3VEC( lgts[0].dir, view2model, z_axis );
		VUNITIZE( lgts[0].dir );
		/* Eye location specified by saved view, grid location
			depends on whether we want perspective or not.
		 */
		if( rel_perspective != 0.0 )
			{ /* Perspective used in animation. */
				fastf_t	grid_delta[3], f;
			f = EYE_SIZE * rel_perspective;
			Scale2Vec( lgts[0].dir, -f, grid_delta );
			Add2Vec( lgts[0].loc, grid_delta, grid_loc );
			}
		else
			{ /* No perspective, eye at center of grid. */
			VMOVE( grid_loc, lgts[0].loc );
			}
		/* Horizontal and vertical grid unit vectors. */
		MAT4X3VEC( grid_hor, view2model, x_axis );
		MAT4X3VEC( grid_ver, view2model, y_axis );
		bu_log( "\tview size\t%12.6f\n", view_size );
		bu_log( "\tview direction\t<%12.6f,%12.6f,%12.6f>\n",
			-lgts[0].dir[X],
			-lgts[0].dir[Y],
			-lgts[0].dir[Z]
			);
		V_Print( "\teye location", lgts[0].loc, bu_log );
		V_Print( "\tgrid location", grid_loc, bu_log );
		V_Print( "\tgrid horizontal", grid_hor, bu_log );
		V_Print( "\tgrid vertical", grid_ver, bu_log );
		}
	else
	if(   !	lgts[0].over
	    &&	(eye_stp = rt_find_solid( rt_ip, lgts[0].name )) != SOLTAB_NULL
	    &&	(grid_stp = rt_find_solid( rt_ip, "GRID" )) != SOLTAB_NULL
		) /* Eye and grid are modeled explicitly. */
		{	fastf_t	mag;
		lgts[0].stp = eye_stp;
		VMOVE( lgts[0].loc, eye_stp->st_center );
		VMOVE( grid_loc, grid_stp->st_center );
		/* Observer positioned by actual solid in data base. */
		Diff2Vec( lgts[0].loc, grid_loc, lgts[0].dir );
		if( MAGNITUDE( lgts[0].dir ) < EPSILON  )
			{
			bu_log( "The GRID is on top of your EYE!\n" );
			prnt_Timer( "LGTS" );
			return	0;
			}
		VUNITIZE( lgts[0].dir );
		bu_log( "Using eye solid (%s) and grid solid \"GRID\".\n",
			lgts[0].name );
		bu_log( "\teye center = <%g,%g,%g>\n",
			eye_stp->st_center[X],
			eye_stp->st_center[Y],
			eye_stp->st_center[Z] );
		bu_log( "\tgrid center = <%g,%g,%g>\n",
			grid_stp->st_center[X],
			grid_stp->st_center[Y],
			grid_stp->st_center[Z] );
		bu_log( "\tviewing direction = <%g,%g,%g>\n",
			lgts[0].dir[X],
			lgts[0].dir[Y],
			lgts[0].dir[Z] );
		VCROSS( grid_hor, lgts[0].dir, neg_z_axis );
		if( (mag = MAGNITUDE( grid_hor )) < EPSILON )
			{ /* Must be top view. */
			VMOVE( grid_hor, y_axis );
			VMOVE( grid_ver, neg_x_axis );
			}
		else
			{
			/* Unitize first. */
			VSCALE( grid_hor, grid_hor, 1.0/mag );
			VCROSS( grid_ver, lgts[0].dir, grid_hor );
			if( (mag = MAGNITUDE( grid_ver )) < EPSILON )
				{ /* Must be top ([+/-]90,90) view. */
				VMOVE( grid_ver, y_axis );
				}
			else
				{
				/* Unitize first. */
				VSCALE( grid_ver, grid_ver, 1.0/mag );
				/* Check for inverted image. */
				if(	Dot( grid_hor, y_axis ) < 0
				    &&	Dot( grid_ver, neg_z_axis ) > 0
					)
					{
					VSCALE( grid_ver, grid_ver, -1.0 );
					}
				}
			}
#ifdef VDEBUG
		bu_log( "\tgrid vectors: hor=<%g,%g,%g> ver=<%g,%g,%g>\n",
			grid_hor[X], grid_hor[Y], grid_hor[Z],
			grid_ver[X], grid_ver[Y], grid_ver[Z] );
#endif
		}
	else	/* Automatic positioning of grid based on perspective. */
	if( rel_perspective >= 0.0 )
		{	fastf_t	hor_adjust[3], ver_adjust[3];
		/* Compute horizontal and vertical grid unit vectors. */
		grid_Rotate( lgts[0].azim, lgts[0].elev, grid_roll,
			     grid_hor, grid_ver );
		/* Unit vector from center of model toward eye. */
		cons_Vector( lgts[0].dir, lgts[0].azim, lgts[0].elev );
		if( rel_perspective == 0.0 )
			{
			VJOIN1( grid_loc, modl_cntr, grid_dist, lgts[0].dir );
			VMOVE( lgts[0].loc, grid_loc );
			}
		else
			{
			/* Compute grid position in model space. */
			grid_dist = modl_radius * (1.0/rel_perspective);
			VJOIN1( grid_loc, modl_cntr, grid_dist, lgts[0].dir );
			/* Compute location of eye in model space. */
			lgts[0].dist = grid_dist * 2.0;
			VJOIN1( lgts[0].loc, modl_cntr, lgts[0].dist,
				lgts[0].dir );
			}
		/* Apply grid translation. */
		Scale2Vec( grid_hor, x_grid_offset, hor_adjust );
		Scale2Vec( grid_ver, y_grid_offset, ver_adjust );
		AddVec( grid_loc, hor_adjust );
		AddVec( grid_loc, ver_adjust );
		DiffVec( lgts[0].loc, hor_adjust );
		DiffVec( lgts[0].loc, ver_adjust );
		}
	else	/* Position grid using user options. */
		{	fastf_t	hor_adjust[3], ver_adjust[3];
		/* Compute horizontal and vertical grid unit vectors. */
		grid_Rotate( lgts[0].azim, lgts[0].elev, grid_roll, grid_hor, grid_ver );
		/* Unit vector from center of model to eye. */
		cons_Vector( lgts[0].dir, lgts[0].azim, lgts[0].elev );
		/* Compute location of eye. */
		VJOIN1( lgts[0].loc, modl_cntr, lgts[0].dist, lgts[0].dir );
		/* Compute grid position in model space. */
		VJOIN1( grid_loc, modl_cntr, grid_dist, lgts[0].dir );
		/* Apply grid translation. */
		Scale2Vec( grid_hor, x_grid_offset, hor_adjust );
		Scale2Vec( grid_ver, y_grid_offset, ver_adjust );
		AddVec( grid_loc, hor_adjust );
		AddVec( grid_loc, ver_adjust );
		DiffVec( lgts[0].loc, hor_adjust );
		DiffVec( lgts[0].loc, ver_adjust );
		}
	/* Get light source positions in model space. */
	if( movie.m_noframes > 1  && movie.m_lgts )
		{ 	fastf_t	rel_frame = (fastf_t) frame / movie.m_noframes;
		/* Lights must track view, explicit light source positions
			not supported. */
		for( i = 1; i < lgt_db_size; i++ )
			{	fastf_t	azim, elev;
			if(   ! lgts[i].over
				/* Fill in ptr to solid table for quick checking. */
			    &&	(lgts[i].stp = rt_find_solid( rt_ip, lgts[i].name )) != SOLTAB_NULL
				)
				{
				bu_log( "Explicitly positioned light sources can't track view.\n" );
				bu_log( "Light source [%d]:\"%s\" ignored.\n", i, lgts[i].name );
				continue;
				}
			azim = lgts[i].azim + rel_frame *
				(movie.m_azim_end - movie.m_azim_beg);
			elev = lgts[i].elev + rel_frame *
				(movie.m_elev_end - movie.m_elev_beg);
			/* Unit vector - center of model to light src. */
			cons_Vector( lgts[i].dir, azim, elev );
			/* Compute location of light source. */
			VJOIN1( lgts[i].loc, modl_cntr, lgts[i].dist, lgts[i].dir );
			}
		}
	else
	for( i = 1; i < lgt_db_size; i++ )
		{
		if(   ! lgts[i].over
			/* Fill in ptr to solid table for quick checking.*/
		    &&	(lgts[i].stp = rt_find_solid( rt_ip, lgts[i].name )) != SOLTAB_NULL
			)
			{
			VMOVE( lgts[i].loc, lgts[i].stp->st_center );
			lgts[i].dist = Dist3d( lgts[i].loc, modl_cntr );
			}
		else
			{
			/* Unit vector - center of model to light src. */
			cons_Vector( lgts[i].dir, lgts[i].azim, lgts[i].elev );
			/* Compute location of light source. */
			VJOIN1( lgts[i].loc, modl_cntr, lgts[i].dist, lgts[i].dir );
			}
		}
	/* Compute intensity coefficients based on RGB values. */
	for( i = 0; i < lgt_db_size; i++ )
		{
		if(	lgts[i].rgb[0] == 0
		    &&	lgts[i].rgb[1] == 0
		    &&	lgts[i].rgb[2] == 0
			)
			bu_log( "Warning: light source %d is black.\n", i );
		Scale2Vec( lgts[i].rgb, RGB_INVERSE, lgts[i].coef );
		}
	prnt_Timer( "LGTS" );
	return	1;
	}

/*	u s e r _ I n p u t ( ) */
void
user_Input(char **args)
{	static int first_time_through = TRUE;
		register int i;
	if( get_Input( input_ln, BUFSIZ, ": " ) == NULL )
		{
		/* If detached, and at EOF, do atleast 1 picture. */
		if( ! tty && first_time_through )
			{
			first_time_through = FALSE;
			args[0] = "R";
			args[1] = NULL;
			return;
			}
		else
		if( feof( stdin ) )
			{ /* User types interrupt, EOT, or EOF encountered
				from detached process.
			   */
			args[0] = "q";
			args[1] = NULL;
			return;
			}
		/* else, probably user interrupt, ignore it. */
		}
	first_time_through = FALSE;
	/* special code to process comments */
	if( func_tab[(int)input_ln[0]].func == f_Comment )
		{
		args[0] = input_ln;
		args[1] = NULL;
		return;
		}
	if( (args[0] = strtok( input_ln, " \t" )) == NULL )
		{
		if( ! tty )
			bu_log( "Blank line input.\n" );
		args[0] = "#";
		args[1] = NULL;
		return;
		}
	for( i = 1; (args[i] = strtok( (char *) NULL, " \t" )) != NULL; ++i )
		;
	return;
	}

/*	m a k e _ S c r i p t ( ) */
static int
make_Script(char *file)
{	FILE	*run_fp;
		char	**obj_p = objects;
	if( file != NULL )
		(void) strncpy( script_file, file, MAX_LN );
	else
	if( tty )
		{
		(void) sprintf( prompt,
				"Name of script file ? (%s) ",
				script_file
				);
		if( get_Input( input_ln, MAX_LN, prompt ) != NULL )
			(void) strcpy( script_file, input_ln );
		}
	if( script_file[0] == '\0' )
		{
		bu_log( "No default, must specify file name!\n" );
		return	-1;
		}
	{	int	tmp_fd;
	if( (tmp_fd = open( script_file, O_CREAT, 0755 )) == -1 )
		{
		loc_Perror( script_file );
		return	-1;
		}
	(void) close( tmp_fd );
	}
	if( (run_fp = fopen( script_file, "w" )) == NULL )
		{
		bu_log( "Can't open '%s' for writing.\n", script_file );
		return	-1;
		}
	(void) fprintf( run_fp, "#!/bin/sh\n" );
	(void) fprintf( run_fp,
			"# This script created with 'w' command of 'lgt'.\n"
			);
	(void) fprintf( run_fp,
			"exec %s -f%g -x\"%d %d\" -y\"%d %d\"",
			prog_id,
			grid_dist,
			grid_x_org,
			grid_x_fin,
			grid_y_org,
			grid_y_fin
			);
	if( save_view_flag )
		(void) fprintf( run_fp, " -j%s", svkey_file );
	(void) fprintf( run_fp, " -G\"%g %d %d %g\"",
			force_cellsz ? cell_sz : (double) grid_sz,
			force_cellsz,
			type_grid,
			force_viewsz ? view_size : 0.0 );

	if( err_file[0] != '\0' && strcmp( err_file, "/dev/tty" ) )
		(void) fprintf( run_fp,	" -O%s", err_file );
	if( aperture_sz > 1 )
		(void) fprintf( run_fp,	" -A%d", aperture_sz );
	if( x_fb_origin != 0 || y_fb_origin != 0 )
		(void) fprintf( run_fp,	" -D\"%d %d\"",
				x_fb_origin, y_fb_origin );
	if( max_bounce )
		(void) fprintf( run_fp,	" -K%d", max_bounce );
	if( grid_roll != 0.0 )
		(void) fprintf( run_fp,	" -a%g", grid_roll*DEGRAD );
	if( background[0] || background[1] || background[2] )
		(void) fprintf( run_fp,
				" -b\"%d %d %d\"",
				background[0],
				background[1],
				background[2]
				);
	if( RT_G_DEBUG )
		(void) fprintf( run_fp,	" -e%d", RT_G_DEBUG );
	if( ir_noise != 2 )
		(void) fprintf( run_fp,	" -i%d", ir_noise );
	if( hiddenln_draw )
		(void) fprintf( run_fp,	" -k%d",
				hiddenln_draw ? (reverse_video ? 2 : 1) : 0
			 	);
	if( ! report_overlaps )
		(void) fprintf( run_fp, " -X%d", report_overlaps );
	if( ! shadowing )
		(void) fprintf( run_fp, " -z%d", shadowing );
	if( force_fbsz )
		(void) fprintf( run_fp, " -T%d", fb_size );

	if( x_grid_offset != 0.0 || y_grid_offset != 0.0 )
		(void) fprintf( run_fp,	" -t\"%g %g\"", x_grid_offset, y_grid_offset );
	if( mat_db_file[0] != '\0' )
		(void) fprintf( run_fp,	" -w%s", mat_db_file );
	if( lgt_db_file[0] != '\0' )
		(void) fprintf( run_fp,	" -v%s", lgt_db_file );
	/*if( npsw != DEFAULT_PSW )
		(void) fprintf( run_fp, " -n%d", npsw );  (XXX) */
	if( fb_file[0] != '\0' )
		(void) fprintf( run_fp,	" -o%s", fb_file );
	if( ir_offset )
		(void) fprintf( run_fp, " -d\"%d %d\"", ir_mapx, ir_mapy );
	if( rel_perspective != 0.25 )
		(void) fprintf( run_fp, " -p%g", rel_perspective );
	if( ir_mapping ) /* MUST precede -I option. */
		(void) fprintf( run_fp, " -s%d", ir_mapping );
	if( ir_file[0] != '\0' )
		(void) fprintf( run_fp,	" -I%s", ir_file );
	/* Can't do this before GED file is read....
	if( ir_db_file[0] != '\0' )
		(void) fprintf( run_fp,	" -u%s", ir_db_file ); */

	(void) fprintf( run_fp,	" %s", ged_file	);
	while( *obj_p != NULL )
		(void) fprintf( run_fp, " %s", *obj_p++ );
	(void) fprintf( run_fp, "\n" );
	(void) fclose( run_fp );
	return	1;
	}

/*	p a r s _ A r g v ( ) */
int
pars_Argv(int argc, register char **argv)
{	register int	c;
		extern int	optind;
		extern char	*optarg;
	prog_id = argv[0];

	/* Initialize terminal I/O. */
	if( (tty = isatty( 0 )) )
		{
		setbuf( stdout, malloc( BUFSIZ ) );
		InitTermCap( stdout );
		li = LI;	/* Default window size from termcap. */
		co = CO;
		(void) f_Redraw( (HMitem *) 0, (char **) 0 );
		(void) fflush( stdout );
		}
	rt_prep_timer();
	prnt_Event( "Parsing options..." );

	/* Parse options. */
	while(	(c =
		getopt( argc, argv, "A:D:I:G:K:O:S:T:X:a:b:c:d:e:f:g:i:j:k:n:o:p:s:t:v:w:x:y:z:" )
		)
		!= EOF
		)
		{
		switch( c )
			{
		default :
			if( ! user_Opt(	c, optarg ) )
				{
				(void) printf( "Failure of user_Opt(%c)", c );
				return	0;
				}
			break;
		case '?' :
			return	0;
			}
		}
	prnt_Timer( "OPT" );
	prnt_Event( "Reading data base..." );

	/* Check for data base name. */
	if( (ged_file = argv[optind++]) == NULL )
		{
		bu_log( "No GED file or objects!\n" );
		return	0;
		}

	/* Check for objects to raytrace. */
	if( argv[optind] == NULL )
		{
		bu_log( "Must specify GED objects!\n" );
		return	0;
		}

	/* Build the directory. */
	if( (rt_ip = rt_dirbuild( ged_file, title, TITLE_LEN )) == RTI_NULL )
		{
		prnt_Event( "Exiting...\n" );
		exit( 1 );
		}
	prnt_Title( title );
	prnt_Timer( "DIR" );

	/* Load the desired portion of the model. */
	objects = &argv[optind];
	{	register int	ok = 0;
	while( argv[optind] != NULL )
		{
		if( tty )
			{	char	prnt_buf[MAX_LN];
			(void) sprintf(	prnt_buf,
					"Loading \"%s\"...",
					argv[optind]
					);
			prnt_Event( prnt_buf );
			}
		if( rt_gettree( rt_ip, argv[optind++] ) == 0 )
			ok++;
		}
	prnt_Timer( "LOAD" );
	if( ! ok )
		{
		bu_log( "No valid objects specified!\n" );
		exit( 1 );
		}
	}
	prnt_Event( "Prepping solids..." );
	rt_prep( rt_ip );
	prnt_Timer( "PREP" );

	/* Compute model radius. */
	modl_radius = Dist3d( rt_ip->mdl_min, rt_ip->mdl_max ) / 2.0;
	if( ! force_viewsz )
		view_size = modl_radius * 2.0;
	bu_log( "Model minimum [%11.4f,%11.4f,%11.4f]\n",
		rt_ip->mdl_min[X],
		rt_ip->mdl_min[Y],
		rt_ip->mdl_min[Z]
		);
	bu_log( "Model maximum [%11.4f,%11.4f,%11.4f]\n",
		rt_ip->mdl_max[X],
		rt_ip->mdl_max[Y],
		rt_ip->mdl_max[Z]
		);

	if( ! grid_position )
		grid_dist = view_size;
	return	1;
	}

/*ARGSUSED*/
void
stop_sig(int sig)
{	int	pid = getpid();
	EVENT_MOVE();
	(void) signal( sig, SIG_DFL );
#if defined( SIGSTOP )
	(void) kill( pid, SIGSTOP );
#else
	(void) kill( pid, sig );
#endif
	(void) signal( sig, stop_sig );
	(void) f_Redraw( (HMitem *) 0, (char **) 0 );
	return;
	}

/* Below are global entry points for getting to function table from
	other modules.
 */
int
key_Frame(void)
{	static char	*local_argv[2];
	local_argv[0] = local_argv[1] = NULL;
	return	f_Key_Frame( (HMitem *) 0, local_argv );
	}

/*	g e t _ I n p u t ( )
	Get a line of input.
 */
char	*
get_Input(char *inbuf, int bufsz, char *msg)
{
#if HAS_SGIGL
		extern char	*sgi_GetInput();
	if( sgi_console && sgi_usemouse )
		return	sgi_GetInput( inbuf, bufsz, msg );
	else
#endif
		{
		prnt_Prompt( msg );
		if( fgets( inbuf, bufsz, stdin ) == NULL )
			return	NULL;
		else	/* Clobber new-line. */
			inbuf[strlen(inbuf)-1] = NUL;
		prnt_Prompt( "" );
		if( inbuf[0] == '\0' )
			return	NULL;
		else
			return	inbuf;
		}
	}

int
hm_getchar(void)
{
#if HAS_SGIGL
	if( SGI_USEKEYBD )
		return	sgi_Getchar();
#endif
	{	register int	c;
	while( (c = getchar()) == EOF )
		;
	return	c;
	}
	}

int
hm_ungetchar(int c)
{
#if HAS_SGIGL
	if( SGI_USEKEYBD )
		return	sgi_Ungetchar();
#endif
	return	ungetc( c, stdin );
	}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
