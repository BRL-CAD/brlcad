/*
 *			D M - P S . C
 *
 * A useful hack to allow GED to generate
 * PostScript files that not only contain the drawn objects, but
 * also contain the faceplate display as well.
 * Mostly, used for making viewgraphs and photographs
 * of an editing session.
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
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "conf.h"

#include <stdio.h>
#include <sys/time.h>		/* for struct timeval */
#include "machine.h"
#include "externs.h"
#include "bu.h"
#include "vmath.h"
#include "mater.h"
#include "raytrace.h"
#include "./ged.h"
#include "./dm.h"
#include "./solid.h"

/* Display Manager package interface */

#define PLOTBOUND	1000.0	/* Max magnification in Rot matrix */
int	PS_open();
void	PS_close();
MGED_EXTERN(void	PS_input, (fd_set *input, int noblock) );
void	PS_prolog(), PS_epilog();
void	PS_normal(), PS_newrot();
void	PS_update();
void	PS_puts(), PS_2d_line(), PS_light();
int	PS_object();
unsigned PS_cvtvecs(), PS_load();
void	PS_statechange(), PS_viewchange(), PS_colorchange();
void	PS_window(), PS_debug();

struct dm dm_PS = {
	PS_open, PS_close,
	PS_input,
	PS_prolog, PS_epilog,
	PS_normal, PS_newrot,
	PS_update,
	PS_puts, PS_2d_line,
	PS_light,
	PS_object,
	PS_cvtvecs, PS_load,
	PS_statechange,
	PS_viewchange,
	PS_colorchange,
	PS_window, PS_debug,
	0,				/* no displaylist */
	0,				/* no display to release! */
	PLOTBOUND,
	"ps", "Screen to PostScript"
};

extern struct device_values dm_values;	/* values read from devices */

static vect_t	clipmin, clipmax;	/* for vector clipping */
FILE	*ps_fp;			/* PostScript file pointer */
static char	ttybuf[BUFSIZ];
#if 0
static int	in_middle;		/* !0 when in middle of image */
#endif

/*
 * Display coordinate conversion:
 *  GED is using -2048..+2047,
 *  and we define the PLOT file to use 0..4095
 */
#define	GED_TO_PS(x)	((int)((x)+2048))

/*
 *			P S _ O P E N
 *
 * Open the output file, and output the PostScript prolog.
 *
 */
PS_open()
{
	char line[64];

	if( (ps_fp = fopen( dname, "w" )) == NULL ){
	  Tcl_AppendResult(interp, "f_ps: Error opening file - ", dname,
			   "\n", (char *)NULL);
	  return TCL_ERROR;
	}

	setbuf( ps_fp, ttybuf );
	fputs( "%!PS-Adobe-1.0\n\
%begin(plot)\n\
%%DocumentFonts:  Courier\n", ps_fp );
	fprintf(ps_fp, "%%%%Title: %s\n", line );
	fputs( "\
%%Creator: MGED dm-ps.c\n\
%%BoundingBox: 0 0 324 324	% 4.5in square, for TeX\n\
%%EndComments\n\
\n", ps_fp );

	fputs( "\
4 setlinewidth\n\
\n\
% Sizes, made functions to avoid scaling if not needed\n\
/FntH /Courier findfont 80 scalefont def\n\
/DFntL { /FntL /Courier findfont 73.4 scalefont def } def\n\
/DFntM { /FntM /Courier findfont 50.2 scalefont def } def\n\
/DFntS { /FntS /Courier findfont 44 scalefont def } def\n\
\n\
% line styles\n\
/NV { [] 0 setdash } def	% normal vectors\n\
/DV { [8] 0 setdash } def	% dotted vectors\n\
/DDV { [8 8 32 8] 0 setdash } def	% dot-dash vectors\n\
/SDV { [32 8] 0 setdash } def	% short-dash vectors\n\
/LDV { [64 8] 0 setdash } def	% long-dash vectors\n\
\n\
/NEWPG {\n\
	.0791 .0791 scale	% 0-4096 to 324 units (4.5 inches)\n\
} def\n\
\n\
FntH  setfont\n\
NEWPG\n\
", ps_fp);

#if 0
	in_middle = 0;
#endif

	return(0);			/* OK */
}

/*
 *  			P S _ C L O S E
 *  
 *  Gracefully release the display.
 */
void
PS_close()
{
  if( !ps_fp )  return;

  fputs("%end(plot)\n", ps_fp);
  (void)fclose(ps_fp);
  ps_fp = (FILE *)NULL;
}

/*
 *			P S _ P R O L O G
 *
 * There are global variables which are parameters to this routine.
 */
void
PS_prolog()
{
#if 0
  /* We expect the screen to be blank so far */
  if( in_middle )  {
    /*
     *  Force the release of this Display Manager
     *  This assures only one frame goes into the file.
     */

    release(NULL);
    return;
  }

  in_middle = 1;
#endif

  /* Put the center point up */
  PS_2d_line( 0, 0, 1, 1, 0 );
}

/*
 *			P S _ E P I L O G
 */
void
PS_epilog()
{
  if( !ps_fp )  return;

  fputs("% showpage	% uncomment to use raw file\n", ps_fp);
  (void)fflush( ps_fp );
  return;
}

/*
 *  			P S _ N E W R O T
 *  Stub.
 */
/* ARGSUSED */
void
PS_newrot(mat)
mat_t mat;
{
	return;
}

/*
 *  			P S _ O B J E C T
 *  
 *  Set up for an object, transformed as indicated, and with an
 *  object center as specified.  The ratio of object to screen size
 *  is passed in as a convienience.
 *
 *  Returns 0 if object could be drawn, !0 if object was omitted.
 */
/* ARGSUSED */
int
PS_object( sp, mat, ratio, white )
register struct solid *sp;
mat_t mat;
double ratio;
int white;
{
	static vect_t			last;
	register struct rt_vlist	*vp;
	int useful = 0;

	if( !ps_fp )  return(0);

	if( sp->s_soldash )
		fprintf(ps_fp, "DDV ");		/* Dot-dashed vectors */
	else
		fprintf(ps_fp, "NV ");		/* Normal vectors */

	for( BU_LIST_FOR( vp, rt_vlist, &(sp->s_vlist) ) )  {
		register int	i;
		register int	nused = vp->nused;
		register int	*cmd = vp->cmd;
		register point_t *pt = vp->pt;
		for( i = 0; i < nused; i++,cmd++,pt++ )  {
			static vect_t	start, fin;
			switch( *cmd )  {
			case RT_VLIST_POLY_START:
			case RT_VLIST_POLY_VERTNORM:
				continue;
			case RT_VLIST_POLY_MOVE:
			case RT_VLIST_LINE_MOVE:
				/* Move, not draw */
				MAT4X3PNT( last, model2view, *pt );
				continue;
			case RT_VLIST_POLY_DRAW:
			case RT_VLIST_POLY_END:
			case RT_VLIST_LINE_DRAW:
				/* draw */
				MAT4X3PNT( fin, model2view, *pt );
				VMOVE( start, last );
				VMOVE( last, fin );
				break;
			}

			if(
				vclip( start, fin, clipmin, clipmax ) == 0
			)  continue;

			fprintf(ps_fp,"newpath %d %d moveto %d %d lineto stroke\n",
				GED_TO_PS( start[0] * 2047 ),
				GED_TO_PS( start[1] * 2047 ),
				GED_TO_PS( fin[0] * 2047 ),
				GED_TO_PS( fin[1] * 2047 ) );
			useful = 1;
		}
	}
	return(useful);
}

/*
 *			P S _ N O R M A L
 *
 * Restore the display processor to a normal mode of operation
 * (ie, not scaled, rotated, displaced, etc).
 * Turns off windowing.
 */
void
PS_normal()
{
	return;
}

/*
 *			P S _ U P D A T E
 *
 * Transmit accumulated displaylist to the display processor.
 */
void
PS_update()
{
	if( !ps_fp )  return;

	(void)fflush(ps_fp);
}

/*
 *			P S _ P U T S
 *
 * Output a string into the displaylist.
 * The starting position of the beam is as specified.
 */
/* ARGSUSED */
void
PS_puts( str, x, y, size, color )
register char *str;
{
	if( !ps_fp )  return;

	switch( size )  {
	default:
		/* Smallest */
		fprintf(ps_fp,"DFntS ");
		break;
	case 1:
		fprintf(ps_fp,"DFntM ");
		break;
	case 2:
		fprintf(ps_fp,"DFntL ");
		break;
	case 3:
		/* Largest */
		fprintf(ps_fp,"FntH ");
		break;
	}

	fprintf(ps_fp, "(%s) %d %d moveto show\n",
		str, GED_TO_PS(x), GED_TO_PS(y) );
}

/*
 *			P S _ 2 D _ G O T O
 *
 */
void
PS_2d_line( x1, y1, x2, y2, dashed )
int x1, y1;
int x2, y2;
int dashed;
{

	if( !ps_fp )  return;

	if( dashed )
		fprintf(ps_fp, "DDV ");	/* Dot-dashed vectors */
	else
		fprintf(ps_fp, "NV ");		/* Normal vectors */
	fprintf(ps_fp,"newpath %d %d moveto %d %d lineto stroke\n",
		GED_TO_PS(x1), GED_TO_PS(y1),
		GED_TO_PS(x2), GED_TO_PS(y2) );
}

/*
 *			P S _ I N P U T
 *
 * Execution must suspend in this routine until a significant event
 * has occured on either the command stream,
 * unless "noblock" is set.
 *
 * Implicit Return -
 *	If any files are ready for input, their bits will be set in 'input'.
 *	Otherwise, 'input' will be all zeros.
 */
void
PS_input( input, noblock )
fd_set		*input;
int		noblock;
{
	struct timeval	tv;
	int		width;
	int		cnt;

#if defined(_SC_OPEN_MAX)
	if( (width = sysconf(_SC_OPEN_MAX)) <= 0 )
#endif
		width = 32;

	/*
	 * Check for input on the keyboard only.
	 *
	 * Suspend execution until either
	 *  1)  User types a full line
	 *  2)  The timelimit on SELECT has expired
	 *
	 * If a RATE operation is in progress (zoom, rotate, slew)
	 * in which we still have to update the display,
	 * do not suspend execution.
	 */
	tv.tv_sec = 0;
	if( noblock )  {
		tv.tv_usec = 0;
	}  else  {
		/* 1/20th second */
		tv.tv_usec = 50000;
	}
	cnt = select( width, input, (fd_set *)0,  (fd_set *)0, &tv );
	if( cnt < 0 )  {
		perror("dm/select");
	}
}

/* 
 *			P S _ L I G H T
 */
/* ARGSUSED */
void
PS_light( cmd, func )
int cmd;
int func;			/* BE_ or BV_ function */
{
	return;
}

/* ARGSUSED */
unsigned
PS_cvtvecs( sp )
struct solid *sp;
{
	return( 0 );
}

/*
 * Loads displaylist
 */
unsigned
PS_load( addr, count )
unsigned addr, count;
{
#if 0
	bu_log("PS_load(x%x, %d.)\n", addr, count );
#endif
	return( 0 );
}

void
PS_statechange()
{
}

void
PS_viewchange()
{
}

void
PS_colorchange()
{
	color_soltab();		/* apply colors to the solid table */
}

/* ARGSUSED */
void
PS_debug(lvl)
{
}

void
PS_window(w)
register int w[];
{
	/* Compute the clipping bounds */
	clipmin[0] = w[1] / 2048.;
	clipmin[1] = w[3] / 2048.;
	clipmin[2] = w[5] / 2048.;
	clipmax[0] = w[0] / 2047.;
	clipmax[1] = w[2] / 2047.;
	clipmax[2] = w[4] / 2047.;
}
