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
#include "solid.h"

#define EPSILON          0.0001

/* Display Manager package interface */

#define PLOTBOUND	1000.0	/* Max magnification in Rot matrix */
struct dm	*ps_open();
static int	ps_close();
static int	ps_drawBegin(), ps_drawEnd();
static int	ps_normal(), ps_newrot();
static int	ps_drawString2D(), ps_drawLine2D();
static int      ps_drawVertex2D();
static int	ps_drawVList();
static int      ps_setColor();
static int      ps_setLineAttr();
static unsigned ps_cvtvecs(), ps_load();
static int	ps_setWinBounds(), ps_debug();

struct dm dm_ps = {
  ps_close,
  ps_drawBegin,
  ps_drawEnd,
  ps_normal,
  ps_newrot,
  ps_drawString2D,
  ps_drawLine2D,
  ps_drawVertex2D,
  ps_drawVList,
  ps_setColor,
  ps_setLineAttr,
  ps_cvtvecs,
  ps_load,
  ps_setWinBounds,
  ps_debug,
  Nu_int0,
  0,				/* no displaylist */
  PLOTBOUND,
  "ps",
  "Screen to PostScript",
  DM_TYPE_PS,
  0,
  0,
  0,
  0,
  0,
  1.0, /* aspect ratio */
  0,
  0,
  0,
  0,
  0,
  0
};

char ps_usage[] = "Usage: ps [-f font] [-t title] [-c creator] [-s size in inches]\
 [-l linewidth] file";

struct ps_vars head_ps_vars;

/*
 *			P S _ O P E N
 *
 * Open the output file, and output the PostScript prolog.
 *
 */
struct dm *
ps_open(eventHandler, argc, argv)
int (*eventHandler)();
int argc;
char *argv[];
{
  static int count = 0;
  struct dm *dmp;

  BU_GETSTRUCT(dmp, dm);
  if(dmp == DM_NULL)
    return DM_NULL;

  *dmp = dm_ps;  /* struct copy */
  dmp->dm_eventHandler = eventHandler;

  dmp->dm_vars = (genptr_t)bu_calloc(1, sizeof(struct ps_vars), "ps_open: ps_vars");
  if(dmp->dm_vars == (genptr_t)NULL){
    bu_free(dmp, "ps_open: dmp");
    return DM_NULL;
  }

  bu_vls_init(&dmp->dm_pathName);
  bu_vls_init(&dmp->dm_tkName);
  bu_vls_printf(&dmp->dm_pathName, ".dm_ps%d", count++);
  bu_vls_printf(&dmp->dm_tkName, "dm_ps%d", count++);

  bu_vls_init(&((struct ps_vars *)dmp->dm_vars)->fname);
  bu_vls_init(&((struct ps_vars *)dmp->dm_vars)->font);
  bu_vls_init(&((struct ps_vars *)dmp->dm_vars)->title);
  bu_vls_init(&((struct ps_vars *)dmp->dm_vars)->creator);

  /* set defaults */
  bu_vls_strcpy(&((struct ps_vars *)dmp->dm_vars)->font, "Courier");
  bu_vls_strcpy(&((struct ps_vars *)dmp->dm_vars)->title, "No Title");
  bu_vls_strcpy(&((struct ps_vars *)dmp->dm_vars)->creator, "LIBDM dm-ps");
  ((struct ps_vars *)dmp->dm_vars)->scale = 0.0791;
  ((struct ps_vars *)dmp->dm_vars)->linewidth = 4;

  /* skip first argument */
  --argc; ++argv;

  /* Process any options */
  while(argv[0] != (char *)0 && argv[0][0] == '-'){
    switch(argv[0][1]){
    case 'f':               /* font */
      if(argv[0][2] != '\0')
	bu_vls_strcpy(&((struct ps_vars *)dmp->dm_vars)->font, &argv[0][2]);
      else{
	argv++;
	if(argv[0] == (char *)0 || argv[0][0] == '-'){
	  Tcl_AppendResult(interp, ps_usage, (char *)0);
	  (void)ps_close(dmp);
	  return DM_NULL;
	}else
	  bu_vls_strcpy(&((struct ps_vars *)dmp->dm_vars)->font, &argv[0][0]);
      }
      break;
    case 't':               /* title */
      if(argv[0][2] != '\0')
	bu_vls_strcpy(&((struct ps_vars *)dmp->dm_vars)->title, &argv[0][2]);
      else{
	argv++;
	if(argv[0] == (char *)0 || argv[0][0] == '-'){
	  Tcl_AppendResult(interp, ps_usage, (char *)0);
	  (void)ps_close(dmp);
	  return DM_NULL;
	}else
	  bu_vls_strcpy(&((struct ps_vars *)dmp->dm_vars)->title, &argv[0][0]);
      }
      break;
    case 'c':               /* creator */
      if(argv[0][2] != '\0')
	bu_vls_strcpy(&((struct ps_vars *)dmp->dm_vars)->creator, &argv[0][2]);
      else{
	argv++;
	if(argv[0] == (char *)0 || argv[0][0] == '-'){
	  Tcl_AppendResult(interp, ps_usage, (char *)0);
	  (void)ps_close(dmp);
	  return DM_NULL;
	}else
	  bu_vls_strcpy(&((struct ps_vars *)dmp->dm_vars)->creator, &argv[0][0]);
      }
      break;
    case 's':               /* size in inches */
      {
	fastf_t size;

	if(argv[0][2] != '\0')
	  sscanf(&argv[0][2], "%lf", &size);
	else{
	  argv++;
	  if(argv[0] == (char *)0 || argv[0][0] == '-'){
	    Tcl_AppendResult(interp, ps_usage, (char *)0);
	    (void)ps_close(dmp);
	    return DM_NULL;
	  }else
	    sscanf(&argv[0][0], "%lf", &size);
	}

	((struct ps_vars *)dmp->dm_vars)->scale = size * 0.017578125;
      }
      break;
    case 'l':               /* line width */
      if(argv[0][2] != '\0')
	sscanf(&argv[0][2], "%d", &((struct ps_vars *)dmp->dm_vars)->linewidth);
      else{
	argv++;
	if(argv[0] == (char *)0 || argv[0][0] == '-'){
	  Tcl_AppendResult(interp, ps_usage, (char *)0);
	  (void)ps_close(dmp);
	  return DM_NULL;
	}else
	  sscanf(&argv[0][0], "%d", &((struct ps_vars *)dmp->dm_vars)->linewidth);
      }
      break;
    default:
      Tcl_AppendResult(interp, ps_usage, (char *)0);
      (void)ps_close(dmp);
      return DM_NULL;
    }
    argv++;
  }

  if(argv[0] == (char *)0){
    Tcl_AppendResult(interp, "no filename specified\n", (char *)NULL);
    (void)ps_close(dmp);
    return DM_NULL;
  }

  bu_vls_strcpy(&((struct ps_vars *)dmp->dm_vars)->fname, argv[0]);

  if( (((struct ps_vars *)dmp->dm_vars)->ps_fp =
       fopen(bu_vls_addr(&((struct ps_vars *)dmp->dm_vars)->fname), "w")) == NULL){
    Tcl_AppendResult(interp, "f_ps: Error opening file - ",
		     ((struct ps_vars *)dmp->dm_vars)->fname,
		     "\n", (char *)NULL);
    (void)ps_close(dmp);
    return DM_NULL;
  }
  
  setbuf( ((struct ps_vars *)dmp->dm_vars)->ps_fp,
	  ((struct ps_vars *)dmp->dm_vars)->ttybuf );
  fprintf(((struct ps_vars *)dmp->dm_vars)->ps_fp,"%%!PS-Adobe-1.0\n\
%%begin(plot)\n\
%%%%DocumentFonts:  %s\n",
	  bu_vls_addr(&((struct ps_vars *)dmp->dm_vars)->font));

  fprintf(((struct ps_vars *)dmp->dm_vars)->ps_fp, "%%%%Title: %s\n",
	  bu_vls_addr(&((struct ps_vars *)dmp->dm_vars)->title));

  fprintf(((struct ps_vars *)dmp->dm_vars)->ps_fp, "\
%%%%Creator: %s\n\
%%%%BoundingBox: 0 0 324 324	%% 4.5in square, for TeX\n\
%%%%EndComments\n\
\n", bu_vls_addr(&((struct ps_vars *)dmp->dm_vars)->creator));

  fprintf(((struct ps_vars *)dmp->dm_vars)->ps_fp, "\
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
", ((struct ps_vars *)dmp->dm_vars)->linewidth,
	  bu_vls_addr(&((struct ps_vars *)dmp->dm_vars)->font),
	  bu_vls_addr(&((struct ps_vars *)dmp->dm_vars)->font),
	  bu_vls_addr(&((struct ps_vars *)dmp->dm_vars)->font),
	  bu_vls_addr(&((struct ps_vars *)dmp->dm_vars)->font),
	  ((struct ps_vars *)dmp->dm_vars)->scale,
	  ((struct ps_vars *)dmp->dm_vars)->scale);

  return dmp;
}

/*
 *  			P S _ C L O S E
 *  
 *  Gracefully release the display.
 */
static int
ps_close(dmp)
struct dm *dmp;
{
  if(!((struct ps_vars *)dmp->dm_vars)->ps_fp)
    return TCL_ERROR;

  fputs("%end(plot)\n", ((struct ps_vars *)dmp->dm_vars)->ps_fp);
  (void)fclose(((struct ps_vars *)dmp->dm_vars)->ps_fp);

  bu_vls_free(&dmp->dm_pathName);
  bu_vls_free(&dmp->dm_tkName);
  bu_vls_free(&((struct ps_vars *)dmp->dm_vars)->fname);
  bu_vls_free(&((struct ps_vars *)dmp->dm_vars)->font);
  bu_vls_free(&((struct ps_vars *)dmp->dm_vars)->title);
  bu_vls_free(&((struct ps_vars *)dmp->dm_vars)->creator);
  bu_free(dmp->dm_vars, "ps_close: ps_vars");
  bu_free(dmp, "ps_close: dmp");

  return TCL_OK;
}

/*
 *			P S _ P R O L O G
 *
 * There are global variables which are parameters to this routine.
 */
static int
ps_drawBegin(dmp)
struct dm *dmp;
{
  return TCL_OK;
}

/*
 *			P S _ E P I L O G
 */
static int
ps_drawEnd(dmp)
struct dm *dmp;
{
  if( !((struct ps_vars *)dmp->dm_vars)->ps_fp )
    return TCL_ERROR;

  fputs("% showpage	% uncomment to use raw file\n",
	((struct ps_vars *)dmp->dm_vars)->ps_fp);
  (void)fflush( ((struct ps_vars *)dmp->dm_vars)->ps_fp );

  return TCL_OK;
}

/*
 *  			P S _ N E W R O T
 *  Stub.
 */
/* ARGSUSED */
static int
ps_newrot(dmp, mat)
struct dm *dmp;
mat_t mat;
{
  return TCL_OK;
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
ps_drawVList( dmp, vp, mat )
struct dm *dmp;
register struct rt_vlist *vp;
mat_t mat;
{
  static vect_t			last;
  register struct rt_vlist	*tvp;
  int useful = 0;

  if( !((struct ps_vars *)dmp->dm_vars)->ps_fp )
    return TCL_ERROR;

#if 0
  if( linestyle )
    fprintf(((struct ps_vars *)dmp->dm_vars)->ps_fp, "DDV ");		/* Dot-dashed vectors */
  else
    fprintf(((struct ps_vars *)dmp->dm_vars)->ps_fp, "NV ");		/* Normal vectors */
#endif

  for( BU_LIST_FOR( tvp, rt_vlist, &vp->l ) )  {
    register int	i;
    register int	nused = tvp->nused;
    register int	*cmd = tvp->cmd;
    register point_t *pt = tvp->pt;
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

      if(vclip( start, fin, ((struct ps_vars *)dmp->dm_vars)->clipmin,
		((struct ps_vars *)dmp->dm_vars)->clipmax ) == 0)
	continue;

      fprintf(((struct ps_vars *)dmp->dm_vars)->ps_fp,
	      "newpath %d %d moveto %d %d lineto stroke\n",
	      GED_TO_PS( start[0] * 2047 ),
	      GED_TO_PS( start[1] * 2047 ),
	      GED_TO_PS( fin[0] * 2047 ),
	      GED_TO_PS( fin[1] * 2047 ) );
      useful = 1;
    }
  }

  if(useful)
    return TCL_OK;

  return TCL_ERROR;
}

/*
 *			P S _ N O R M A L
 *
 * Restore the display processor to a normal mode of operation
 * (ie, not scaled, rotated, displaced, etc).
 * Turns off windowing.
 */
static int
ps_normal(dmp)
struct dm *dmp;
{
  return TCL_OK;
}

/*
 *			P S _ P U T S
 *
 * Output a string into the displaylist.
 * The starting position of the beam is as specified.
 */
/* ARGSUSED */
static int
ps_drawString2D( dmp, str, x, y, size, use_aspect )
struct dm *dmp;
register char *str;
int x, y;
int size;
int use_aspect;
{
  if( !((struct ps_vars *)dmp->dm_vars)->ps_fp )
    return TCL_ERROR;

  switch( size )  {
  default:
    /* Smallest */
    fprintf(((struct ps_vars *)dmp->dm_vars)->ps_fp,"DFntS ");
    break;
  case 1:
    fprintf(((struct ps_vars *)dmp->dm_vars)->ps_fp,"DFntM ");
    break;
  case 2:
    fprintf(((struct ps_vars *)dmp->dm_vars)->ps_fp,"DFntL ");
    break;
  case 3:
    /* Largest */
    fprintf(((struct ps_vars *)dmp->dm_vars)->ps_fp,"FntH ");
    break;
  }

  fprintf(((struct ps_vars *)dmp->dm_vars)->ps_fp,
	  "(%s) %d %d moveto show\n", str, GED_TO_PS(x), GED_TO_PS(y) );

  return TCL_OK;
}

/*
 *			P S _ 2 D _ G O T O
 *
 */
static int
ps_drawLine2D( dmp, x1, y1, x2, y2 )
struct dm *dmp;
int x1, y1;
int x2, y2;
{
  if( !((struct ps_vars *)dmp->dm_vars)->ps_fp )
    return TCL_ERROR;

#if 0
  if( dashed )
    fprintf(((struct ps_vars *)dmp->dm_vars)->ps_fp, "DDV ");	/* Dot-dashed vectors */
  else
    fprintf(((struct ps_vars *)dmp->dm_vars)->ps_fp, "NV ");		/* Normal vectors */
#endif

  fprintf(((struct ps_vars *)dmp->dm_vars)->ps_fp,
	  "newpath %d %d moveto %d %d lineto stroke\n",
	  GED_TO_PS(x1), GED_TO_PS(y1),
	  GED_TO_PS(x2), GED_TO_PS(y2) );

  return TCL_OK;
}

static int
ps_drawVertex2D(dmp, x, y)
struct dm *dmp;
int x, y;
{
  return ps_drawLine2D(dmp, x, y, x, y);
}

static int
ps_setColor(dmp, r, g, b, strict)
struct dm *dmp;
register short r, g, b;
int strict;
{
  return TCL_OK;
}

static int
ps_setLineAttr(dmp, width, style)
struct dm *dmp;
int width;
int style;
{
  dmp->dm_lineWidth = width;
  dmp->dm_lineStyle = style;

  if(style == DM_DASHED_LINE)
    fprintf(((struct ps_vars *)dmp->dm_vars)->ps_fp, "DDV "); /* Dot-dashed vectors */
  else
    fprintf(((struct ps_vars *)dmp->dm_vars)->ps_fp, "NV "); /* Normal vectors */

  return TCL_OK;
}

/* ARGSUSED */
static unsigned
ps_cvtvecs( dmp, sp )
struct dm *dmp;
struct solid *sp;
{
	return( 0 );
}

/*
 * Loads displaylist
 */
static unsigned
ps_load( dmp, addr, count )
struct dm *dmp;
unsigned addr, count;
{
#if 0
	bu_log("ps_load(x%x, %d.)\n", addr, count );
#endif
	return( 0 );
}

/* ARGSUSED */
static int
ps_debug(dmp, lvl)
struct dm *dmp;
{
  return TCL_OK;
}

static int
ps_setWinBounds(dmp, w)
struct dm *dmp;
register int w[];
{
  /* Compute the clipping bounds */
  ((struct ps_vars *)dmp->dm_vars)->clipmin[0] = w[1] / 2048.;
  ((struct ps_vars *)dmp->dm_vars)->clipmin[1] = w[3] / 2048.;
  ((struct ps_vars *)dmp->dm_vars)->clipmin[2] = w[5] / 2048.;
  ((struct ps_vars *)dmp->dm_vars)->clipmax[0] = w[0] / 2047.;
  ((struct ps_vars *)dmp->dm_vars)->clipmax[1] = w[2] / 2047.;
  ((struct ps_vars *)dmp->dm_vars)->clipmax[2] = w[4] / 2047.;

  return TCL_OK;
}
