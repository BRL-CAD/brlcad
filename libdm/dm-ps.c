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
#include "tcl.h"

#include <stdio.h>
#include <sys/time.h>		/* for struct timeval */
#include "machine.h"
#include "externs.h"
#include "bu.h"
#include "vmath.h"
#include "mater.h"
#include "raytrace.h"
#include "_dm.h"
#include "dm-ps.h"

#define EPSILON          0.0001

/*XXX This is just temporary!!! */
#include "../mged/solid.h"

static int vclip();

/* Display Manager package interface */

#define PLOTBOUND	1000.0	/* Max magnification in Rot matrix */
static int	PS_init();
static int	PS_open();
static void	PS_close();
static void	PS_input();
static void	PS_prolog(), PS_epilog();
static void	PS_normal(), PS_newrot();
static void	PS_update();
static void	PS_puts(), PS_2d_line(), PS_light();
static int	PS_object();
static unsigned PS_cvtvecs(), PS_load();
static void	PS_viewchange(), PS_colorchange();
static void	PS_window(), PS_debug();

struct dm dm_PS = {
  PS_init,
  PS_open, PS_close,
  PS_input,
  PS_prolog, PS_epilog,
  PS_normal, PS_newrot,
  PS_update,
  PS_puts, PS_2d_line,
  PS_light,
  PS_object,
  PS_cvtvecs, PS_load,
  0,
  PS_viewchange,
  PS_colorchange,
  PS_window, PS_debug, 0, 0,
  0,				/* no displaylist */
  0,				/* no display to release! */
  PLOTBOUND,
  "ps", "Screen to PostScript",
  0,
  0,
  0,
  0
};

static vect_t	clipmin, clipmax;	/* for vector clipping */
FILE	*ps_fp;			/* PostScript file pointer */
static char	ttybuf[BUFSIZ];

/*
 * Display coordinate conversion:
 *  GED is using -2048..+2047,
 *  and we define the PLOT file to use 0..4095
 */
static int
PS_init(dmp, color_func)
struct dm *dmp;
void (*color_func)();
{

  dmp->dmr_vars = bu_malloc(sizeof(struct ps_vars), "PS_init: ps_vars");
  bzero((void *)dmp->dmr_vars, sizeof(struct ps_vars));
  ((struct ps_vars *)dmp->dmr_vars)->color_func = color_func;

  if(dmp->dmr_vars)
    return TCL_OK;

  return TCL_ERROR;
}

/*
 *			P S _ O P E N
 *
 * Open the output file, and output the PostScript prolog.
 *
 */
static int
PS_open(dmp)
struct dm *dmp;
{
	char line[64];

	if( (ps_fp = fopen( dmp->dmr_dname, "w" )) == NULL ){
	  Tcl_AppendResult(interp, "f_ps: Error opening file - ", dmp->dmr_dname,
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

	return(0);			/* OK */
}

/*
 *  			P S _ C L O S E
 *  
 *  Gracefully release the display.
 */
static void
PS_close(dmp)
struct dm *dmp;
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
static void
PS_prolog(dmp)
struct dm *dmp;
{
  /* Put the center point up */
  PS_2d_line( dmp, 0, 0, 1, 1, 0 );
}

/*
 *			P S _ E P I L O G
 */
static void
PS_epilog(dmp)
struct dm *dmp;
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
static void
PS_newrot(dmp, mat)
struct dm *dmp;
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
static int
PS_object( dmp, sp, mat, ratio, white )
struct dm *dmp;
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
				MAT4X3PNT( last, mat, *pt );
				continue;
			case RT_VLIST_POLY_DRAW:
			case RT_VLIST_POLY_END:
			case RT_VLIST_LINE_DRAW:
				/* draw */
				MAT4X3PNT( fin, mat, *pt );
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
static void
PS_normal(dmp)
struct dm *dmp;
{
	return;
}

/*
 *			P S _ U P D A T E
 *
 * Transmit accumulated displaylist to the display processor.
 */
static void
PS_update(dmp)
struct dm *dmp;
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
static void
PS_puts( dmp, str, x, y, size, color )
struct dm *dmp;
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
static void
PS_2d_line( dmp, x1, y1, x2, y2, dashed )
struct dm *dmp;
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
static void
PS_input( dmp, input, noblock )
struct dm *dmp;
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
static void
PS_light( dmp, cmd, func )
struct dm *dmp;
int cmd;
int func;			/* BE_ or BV_ function */
{
	return;
}

/* ARGSUSED */
static unsigned
PS_cvtvecs( dmp, sp )
struct dm *dmp;
struct solid *sp;
{
	return( 0 );
}

/*
 * Loads displaylist
 */
static unsigned
PS_load( dmp, addr, count )
struct dm *dmp;
unsigned addr, count;
{
#if 0
	bu_log("PS_load(x%x, %d.)\n", addr, count );
#endif
	return( 0 );
}

static void
PS_viewchange(dmp)
struct dm *dmp;
{
}

static void
PS_colorchange(dmp)
struct dm *dmp;
{
  ((struct ps_vars *)dmp->dmr_vars)->color_func();
}

/* ARGSUSED */
static void
PS_debug(dmp, lvl)
struct dm *dmp;
{
}

static void
PS_window(dmp, w)
struct dm *dmp;
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

/*                      V C L I P
 *
 *  Clip a ray against a rectangular parallelpiped (RPP)
 *  that has faces parallel to the coordinate planes (a clipping RPP).
 *  The RPP is defined by a minimum point and a maximum point.
 *
 *  Returns -
 *	 0  if ray does not hit RPP,
 *	!0  if ray hits RPP.
 *
 *  Implicit Return -
 *	if !0 was returned, "a" and "b" have been clipped to the RPP.
 */
static int
vclip( a, b, min, max )
vect_t a, b;
register fastf_t *min, *max;
{
	static vect_t diff;
	static double sv;
	static double st;
	static double mindist, maxdist;
	register fastf_t *pt = &a[0];
	register fastf_t *dir = &diff[0];
	register int i;

	mindist = -INFINITY;
	maxdist = INFINITY;
	VSUB2( diff, b, a );

	for( i=0; i < 3; i++, pt++, dir++, max++, min++ )  {
		if( *dir < -EPSILON )  {
			if( (sv = (*min - *pt) / *dir) < 0.0 )
				return(0);	/* MISS */
			if(maxdist > sv)
				maxdist = sv;
			if( mindist < (st = (*max - *pt) / *dir) )
				mindist = st;
		}  else if( *dir > EPSILON )  {
			if( (st = (*max - *pt) / *dir) < 0.0 )
				return(0);	/* MISS */
			if(maxdist > st)
				maxdist = st;
			if( mindist < ((sv = (*min - *pt) / *dir)) )
				mindist = sv;
		}  else  {
			/*
			 *  If direction component along this axis is NEAR 0,
			 *  (ie, this ray is aligned with this axis),
			 *  merely check against the boundaries.
			 */
			if( (*min > *pt) || (*max < *pt) )
				return(0);	/* MISS */;
		}
	}
	if( mindist >= maxdist )
		return(0);	/* MISS */

	if( mindist > 1 || maxdist < 0 )
		return(0);	/* MISS */

	if( mindist <= 0 && maxdist >= 1 )
		return(1);	/* HIT, no clipping needed */

	/* Don't grow one end of a contained segment */
	if( mindist < 0 )
		mindist = 0;
	if( maxdist > 1 )
		maxdist = 1;

	/* Compute actual intercept points */
	VJOIN1( b, a, maxdist, diff );		/* b must go first */
	VJOIN1( a, a, mindist, diff );
	return(1);		/* HIT */
}
