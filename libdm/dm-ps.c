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
#include "dm.h"
#include "dm-ps.h"

#define EPSILON          0.0001

/*XXX This is just temporary!!! */
#include "../mged/solid.h"

static void PS_load_startup();

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

char ps_usage[] = "Usage: ps [-f font] [-t title] [-c creator] [-s size in inches]\
 [-l linewidth] file";

struct ps_vars head_ps_vars;

static int
PS_init(dmp, color_func, argc, argv)
struct dm *dmp;
void (*color_func)();
int argc;
char *argv[];
{
  static int count = 0;

  /* Only need to do this once for this display manager */
  if(!count)
    PS_load_startup(dmp);

  bu_vls_printf(&dmp->dmr_pathName, ".dm_ps%d", count++);

  dmp->dmr_vars = bu_malloc(sizeof(struct ps_vars), "PS_init: ps_vars");
  bzero((void *)dmp->dmr_vars, sizeof(struct ps_vars));
  ((struct ps_vars *)dmp->dmr_vars)->color_func = color_func;
  bu_vls_init(&((struct ps_vars *)dmp->dmr_vars)->fname);
  bu_vls_init(&((struct ps_vars *)dmp->dmr_vars)->font);
  bu_vls_init(&((struct ps_vars *)dmp->dmr_vars)->title);
  bu_vls_init(&((struct ps_vars *)dmp->dmr_vars)->creator);

  /* set defaults */
  bu_vls_strcpy(&((struct ps_vars *)dmp->dmr_vars)->font, "Courier");
  bu_vls_strcpy(&((struct ps_vars *)dmp->dmr_vars)->title, "No Title");
  bu_vls_strcpy(&((struct ps_vars *)dmp->dmr_vars)->creator, "LIBDM dm-ps");
  ((struct ps_vars *)dmp->dmr_vars)->scale = 0.0791;
  ((struct ps_vars *)dmp->dmr_vars)->linewidth = 4;
  
  /* Process any options */
  while(argv[1] != (char *)0 && argv[1][0] == '-'){
    switch(argv[1][1]){
    case 'f':               /* font */
      if(argv[1][2] != '\0')
	bu_vls_strcpy(&((struct ps_vars *)dmp->dmr_vars)->font, &argv[1][2]);
      else{
	argv++;
	if(argv[1] == (char *)0 || argv[1][0] == '-'){
	  Tcl_AppendResult(interp, ps_usage, (char *)0);
	  return TCL_ERROR;
	}else
	  bu_vls_strcpy(&((struct ps_vars *)dmp->dmr_vars)->font, &argv[1][0]);
      }
      break;
    case 't':               /* title */
      if(argv[1][2] != '\0')
	bu_vls_strcpy(&((struct ps_vars *)dmp->dmr_vars)->title, &argv[1][2]);
      else{
	argv++;
	if(argv[1] == (char *)0 || argv[1][0] == '-'){
	  Tcl_AppendResult(interp, ps_usage, (char *)0);
	  return TCL_ERROR;
	}else
	  bu_vls_strcpy(&((struct ps_vars *)dmp->dmr_vars)->title, &argv[1][0]);
      }
      break;
    case 'c':               /* creator */
      if(argv[1][2] != '\0')
	bu_vls_strcpy(&((struct ps_vars *)dmp->dmr_vars)->creator, &argv[1][2]);
      else{
	argv++;
	if(argv[1] == (char *)0 || argv[1][0] == '-'){
	  Tcl_AppendResult(interp, ps_usage, (char *)0);
	  return TCL_ERROR;
	}else
	  bu_vls_strcpy(&((struct ps_vars *)dmp->dmr_vars)->creator, &argv[1][0]);
      }
      break;
    case 's':               /* size in inches */
      {
	fastf_t size;

	if(argv[1][2] != '\0')
	  sscanf(&argv[1][2], "%lf", &size);
	else{
	  argv++;
	  if(argv[1] == (char *)0 || argv[1][0] == '-'){
	    Tcl_AppendResult(interp, ps_usage, (char *)0);
	    return TCL_ERROR;
	  }else
	    sscanf(&argv[1][0], "%lf", &size);
	}

	((struct ps_vars *)dmp->dmr_vars)->scale = size * 0.017578125;
      }
      break;
    case 'l':               /* line width */
      if(argv[1][2] != '\0')
	sscanf(&argv[1][2], "%d", &((struct ps_vars *)dmp->dmr_vars)->linewidth);
      else{
	argv++;
	if(argv[1] == (char *)0 || argv[1][0] == '-'){
	  Tcl_AppendResult(interp, ps_usage, (char *)0);
	  return TCL_ERROR;
	}else
	  sscanf(&argv[1][0], "%d", &((struct ps_vars *)dmp->dmr_vars)->linewidth);
      }
      break;
    default:
      Tcl_AppendResult(interp, ps_usage, (char *)0);
      return TCL_ERROR;
    }
    argv++;
  }

  if(argv[1] == (char *)0){
    Tcl_AppendResult(interp, "no filename specified\n", (char *)NULL);
    return TCL_ERROR;
  }

  bu_vls_strcpy(&((struct ps_vars *)dmp->dmr_vars)->fname, argv[1]);

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
  if( (((struct ps_vars *)dmp->dmr_vars)->ps_fp =
       fopen(bu_vls_addr(&((struct ps_vars *)dmp->dmr_vars)->fname), "w")) == NULL){
    Tcl_AppendResult(interp, "f_ps: Error opening file - ",
		     ((struct ps_vars *)dmp->dmr_vars)->fname,
		     "\n", (char *)NULL);
    return TCL_ERROR;
  }
  
  setbuf( ((struct ps_vars *)dmp->dmr_vars)->ps_fp,
	  ((struct ps_vars *)dmp->dmr_vars)->ttybuf );
  fprintf(((struct ps_vars *)dmp->dmr_vars)->ps_fp,"%%!PS-Adobe-1.0\n\
%%begin(plot)\n\
%%%%DocumentFonts:  %s\n",
	  bu_vls_addr(&((struct ps_vars *)dmp->dmr_vars)->font));

  fprintf(((struct ps_vars *)dmp->dmr_vars)->ps_fp, "%%%%Title: %s\n",
	  bu_vls_addr(&((struct ps_vars *)dmp->dmr_vars)->title));

  fprintf(((struct ps_vars *)dmp->dmr_vars)->ps_fp, "\
%%%%Creator: %s\n\
%%%%BoundingBox: 0 0 324 324	%% 4.5in square, for TeX\n\
%%%%EndComments\n\
\n", bu_vls_addr(&((struct ps_vars *)dmp->dmr_vars)->creator));

  fprintf(((struct ps_vars *)dmp->dmr_vars)->ps_fp, "\
%d setlinewidth\n\
\n\
%% Sizes, made functions to avoid scaling if not needed\n\
/FntH /%s findfont 80 scalefont def\n\
/DFntL { /FntL /%s findfont 73.4 scalefont def } def\n\
/DFntM { /FntM /%s findfont 50.2 scalefont def } def\n\
/DFntS { /FntS /%s findfont 44 scalefont def } def\n\
\n\
%% line styles\n\
/NV { [] 0 setdash } def	%% normal vectors\n\
/DV { [8] 0 setdash } def	%% dotted vectors\n\
/DDV { [8 8 32 8] 0 setdash } def	%% dot-dash vectors\n\
/SDV { [32 8] 0 setdash } def	%% short-dash vectors\n\
/LDV { [64 8] 0 setdash } def	%% long-dash vectors\n\
\n\
/NEWPG {\n\
	%f %f scale	%% 0-4096 to 324 units (4.5 inches)\n\
} def\n\
\n\
FntH  setfont\n\
NEWPG\n\
", ((struct ps_vars *)dmp->dmr_vars)->linewidth,
	  bu_vls_addr(&((struct ps_vars *)dmp->dmr_vars)->font),
	  bu_vls_addr(&((struct ps_vars *)dmp->dmr_vars)->font),
	  bu_vls_addr(&((struct ps_vars *)dmp->dmr_vars)->font),
	  bu_vls_addr(&((struct ps_vars *)dmp->dmr_vars)->font),
	  ((struct ps_vars *)dmp->dmr_vars)->scale,
	  ((struct ps_vars *)dmp->dmr_vars)->scale);

  return TCL_OK;
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
  if(!((struct ps_vars *)dmp->dmr_vars)->ps_fp)
    return;

  fputs("%end(plot)\n", ((struct ps_vars *)dmp->dmr_vars)->ps_fp);
  (void)fclose(((struct ps_vars *)dmp->dmr_vars)->ps_fp);

#if 0
  /* Give the application a chance to clean up */
  if(dmp->dmr_app_close)
    dmp->dmr_app_close(((struct ps_vars *)dmp->dmr_vars)->app_vars);
#endif

  if(((struct ps_vars *)dmp->dmr_vars)->l.forw != BU_LIST_NULL)
    BU_LIST_DEQUEUE(&((struct ps_vars *)dmp->dmr_vars)->l);

  bu_vls_free(&((struct ps_vars *)dmp->dmr_vars)->fname);
  bu_vls_free(&((struct ps_vars *)dmp->dmr_vars)->font);
  bu_vls_free(&((struct ps_vars *)dmp->dmr_vars)->title);
  bu_vls_free(&((struct ps_vars *)dmp->dmr_vars)->creator);
  bu_free(dmp->dmr_vars, "PS_close: ps_vars");
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
  if( !((struct ps_vars *)dmp->dmr_vars)->ps_fp )  return;

  fputs("% showpage	% uncomment to use raw file\n", ((struct ps_vars *)dmp->dmr_vars)->ps_fp);
  (void)fflush( ((struct ps_vars *)dmp->dmr_vars)->ps_fp );
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

	if( !((struct ps_vars *)dmp->dmr_vars)->ps_fp )  return(0);

	if( sp->s_soldash )
		fprintf(((struct ps_vars *)dmp->dmr_vars)->ps_fp, "DDV ");		/* Dot-dashed vectors */
	else
		fprintf(((struct ps_vars *)dmp->dmr_vars)->ps_fp, "NV ");		/* Normal vectors */

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
				vclip( start, fin, ((struct ps_vars *)dmp->dmr_vars)->clipmin, ((struct ps_vars *)dmp->dmr_vars)->clipmax ) == 0
			)  continue;

			fprintf(((struct ps_vars *)dmp->dmr_vars)->ps_fp,"newpath %d %d moveto %d %d lineto stroke\n",
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
	if( !((struct ps_vars *)dmp->dmr_vars)->ps_fp )  return;

	(void)fflush(((struct ps_vars *)dmp->dmr_vars)->ps_fp);
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
	if( !((struct ps_vars *)dmp->dmr_vars)->ps_fp )  return;

	switch( size )  {
	default:
		/* Smallest */
		fprintf(((struct ps_vars *)dmp->dmr_vars)->ps_fp,"DFntS ");
		break;
	case 1:
		fprintf(((struct ps_vars *)dmp->dmr_vars)->ps_fp,"DFntM ");
		break;
	case 2:
		fprintf(((struct ps_vars *)dmp->dmr_vars)->ps_fp,"DFntL ");
		break;
	case 3:
		/* Largest */
		fprintf(((struct ps_vars *)dmp->dmr_vars)->ps_fp,"FntH ");
		break;
	}

	fprintf(((struct ps_vars *)dmp->dmr_vars)->ps_fp, "(%s) %d %d moveto show\n",
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

	if( !((struct ps_vars *)dmp->dmr_vars)->ps_fp )  return;

	if( dashed )
		fprintf(((struct ps_vars *)dmp->dmr_vars)->ps_fp, "DDV ");	/* Dot-dashed vectors */
	else
		fprintf(((struct ps_vars *)dmp->dmr_vars)->ps_fp, "NV ");		/* Normal vectors */
	fprintf(((struct ps_vars *)dmp->dmr_vars)->ps_fp,"newpath %d %d moveto %d %d lineto stroke\n",
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
  ((struct ps_vars *)dmp->dmr_vars)->clipmin[0] = w[1] / 2048.;
  ((struct ps_vars *)dmp->dmr_vars)->clipmin[1] = w[3] / 2048.;
  ((struct ps_vars *)dmp->dmr_vars)->clipmin[2] = w[5] / 2048.;
  ((struct ps_vars *)dmp->dmr_vars)->clipmax[0] = w[0] / 2047.;
  ((struct ps_vars *)dmp->dmr_vars)->clipmax[1] = w[2] / 2047.;
  ((struct ps_vars *)dmp->dmr_vars)->clipmax[2] = w[4] / 2047.;
}

static void
PS_load_startup(dmp)
struct dm *dmp;
{
  char *filename;

  bzero((void *)&head_ps_vars, sizeof(struct ps_vars));
  BU_LIST_INIT( &head_ps_vars.l );

  if((filename = getenv("DM_PS_RCFILE")) != (char *)NULL )
    Tcl_EvalFile(interp, filename);
}
