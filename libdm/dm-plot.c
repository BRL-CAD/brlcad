/*
 *			D M - P L O T . C
 *
 * An unsatisfying (but useful) hack to allow GED to generate
 * UNIX-plot files that not only contain the drawn objects, but
 * also contain the faceplate display as well.
 * Mostly, a useful hack for making viewgraphs and photographs
 * of an editing session.
 * We assume that the UNIX-plot filter used can at least discard
 * the non-standard extention to specify color (a Gwyn@BRL addition).
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
#include "dm-plot.h"
#include "solid.h"

static int Plot_load_startup();

/* Display Manager package interface */

#define PLOTBOUND	1000.0	/* Max magnification in Rot matrix */
static int	Plot_init();
static int	Plot_open();
static int	Plot_close();
static int	Plot_drawBegin(), Plot_drawEnd();
static int	Plot_normal(), Plot_newrot();
static int	Plot_drawString2D(), Plot_drawLine2D();
static int      Plot_drawVertex2D();
static int	Plot_drawVList();
static int      Plot_setColor();
static int      Plot_setLineAttr();
static unsigned Plot_cvtvecs(), Plot_load();
static int	Plot_setWinBounds(), Plot_debug();

struct dm dm_Plot = {
  Plot_init,
  Plot_open,
  Plot_close,
  Plot_drawBegin,
  Plot_drawEnd,
  Plot_normal,
  Plot_newrot,
  Plot_drawString2D,
  Plot_drawLine2D,
  Plot_drawVertex2D,
  Plot_drawVList,
  Plot_setColor,
  Plot_setLineAttr,
  Plot_cvtvecs,
  Plot_load,
  Plot_setWinBounds,
  Plot_debug,
  Nu_int0,
  0,			/* no displaylist */
  PLOTBOUND,
  "plot",
  "Screen to UNIX-Plot",
  0,
  0,
  0,
  0,
  0
};

struct plot_vars head_plot_vars;

static int
Plot_init(dmp, argc, argv)
struct dm *dmp;
int argc;
char *argv[];
{
  static int count = 0;

  /* Only need to do this once for this display manager */
  if(!count)
    (void)Plot_load_startup(dmp);

  bu_vls_printf(&dmp->dm_pathName, ".dm_plot%d", count++);

  dmp->dm_vars = bu_calloc(1, sizeof(struct plot_vars), "Plot_init: plot_vars");
  bu_vls_init(&((struct plot_vars *)dmp->dm_vars)->vls);

  /* Process any options */
  ((struct plot_vars *)dmp->dm_vars)->is_3D = 1;          /* 3-D w/color, by default */
  while( argv[1] != (char *)0 && argv[1][0] == '-' )  {
    switch( argv[1][1] )  {
    case '3':
      break;
    case '2':
      ((struct plot_vars *)dmp->dm_vars)->is_3D = 0;		/* 2-D, for portability */
      break;
    case 'g':
      ((struct plot_vars *)dmp->dm_vars)->grid = 1;
      break;
#if 0
    case 'f':
      ((struct plot_vars *)dmp->dm_vars)->floating = 1;
      break;
    case 'z':
    case 'Z':
      /* Enable Z clipping */
      Tcl_AppendResult(interp, "Clipped in Z to viewing cube\n", (char *)NULL);
      ((struct plot_vars *)dmp->dm_vars)->zclip = 1;
      break;
#endif
    default:
      Tcl_AppendResult(interp, "bad PLOT option ", argv[1], "\n", (char *)NULL);
      return TCL_ERROR;
    }
    argv++;
  }
  if( argv[1] == (char *)0 )  {
    Tcl_AppendResult(interp, "no filename or filter specified\n", (char *)NULL);
    return TCL_ERROR;
  }

  if( argv[1][0] == '|' )  {
    bu_vls_strcpy(&((struct plot_vars *)dmp->dm_vars)->vls, &argv[1][1]);
    while( (++argv)[1] != (char *)0 ) {
      bu_vls_strcat( &((struct plot_vars *)dmp->dm_vars)->vls, " " );
      bu_vls_strcat( &((struct plot_vars *)dmp->dm_vars)->vls, argv[1] );
    }

    ((struct plot_vars *)dmp->dm_vars)->is_pipe = 1;
  }else{
    bu_vls_strcpy(&((struct plot_vars *)dmp->dm_vars)->vls, argv[1]);
  }

  if(dmp->dm_vars)
    return TCL_OK;

  return TCL_ERROR;
}

/*
 *			P L O T _ O P E N
 *
 * Fire up the display manager, and the display processor.
 *
 */
static int
Plot_open(dmp)
struct dm *dmp;
{
  if(((struct plot_vars *)dmp->dm_vars)->is_pipe){
    if((((struct plot_vars *)dmp->dm_vars)->up_fp =
	popen( bu_vls_addr(&((struct plot_vars *)dmp->dm_vars)->vls), "w")) == NULL){
      perror( bu_vls_addr(&((struct plot_vars *)dmp->dm_vars)->vls));
      return TCL_ERROR;
    }
    
    Tcl_AppendResult(interp, "piped to ",
		     bu_vls_addr(&((struct plot_vars *)dmp->dm_vars)->vls),
		     "\n", (char *)NULL);
  }else{
    if((((struct plot_vars *)dmp->dm_vars)->up_fp =
	fopen( bu_vls_addr(&((struct plot_vars *)dmp->dm_vars)->vls), "w" )) == NULL){
      perror(bu_vls_addr(&((struct plot_vars *)dmp->dm_vars)->vls));
      return TCL_ERROR;
    }

    Tcl_AppendResult(interp, "plot stored in ",
		     bu_vls_addr(&((struct plot_vars *)dmp->dm_vars)->vls),
		     "\n", (char *)NULL);
  }

  setbuf(((struct plot_vars *)dmp->dm_vars)->up_fp,
	  ((struct plot_vars *)dmp->dm_vars)->ttybuf);

  if(((struct plot_vars *)dmp->dm_vars)->is_3D)
    pl_3space( ((struct plot_vars *)dmp->dm_vars)->up_fp,
	       -2048, -2048, -2048, 2048, 2048, 2048 );
  else
    pl_space( ((struct plot_vars *)dmp->dm_vars)->up_fp,
	      -2048, -2048, 2048, 2048 );

  return TCL_OK;
}

/*
 *  			P L O T _ C L O S E
 *  
 *  Gracefully release the display.
 */
static int
Plot_close(dmp)
struct dm *dmp;
{
  (void)fflush(((struct plot_vars *)dmp->dm_vars)->up_fp);

  if(((struct plot_vars *)dmp->dm_vars)->is_pipe)
    pclose(((struct plot_vars *)dmp->dm_vars)->up_fp); /* close pipe, eat dead children */
  else
    fclose(((struct plot_vars *)dmp->dm_vars)->up_fp);

  if(((struct plot_vars *)dmp->dm_vars)->l.forw != BU_LIST_NULL)
    BU_LIST_DEQUEUE(&((struct plot_vars *)dmp->dm_vars)->l);

  bu_free(dmp->dm_vars, "Plot_close: plot_vars");
  return TCL_OK;
}

/*
 *			P L O T _ P R O L O G
 *
 * There are global variables which are parameters to this routine.
 */
static int
Plot_drawBegin(dmp)
struct dm *dmp;
{
  /* We expect the screen to be blank so far, from last frame flush */

  return TCL_OK;
}

/*
 *			P L O T _ E P I L O G
 */
static int
Plot_drawEnd(dmp)
struct dm *dmp;
{
  pl_flush( ((struct plot_vars *)dmp->dm_vars)->up_fp ); /* BRL-specific command */
  pl_erase( ((struct plot_vars *)dmp->dm_vars)->up_fp ); /* forces drawing */
  (void)fflush( ((struct plot_vars *)dmp->dm_vars)->up_fp );

  return TCL_OK;
}

/*
 *  			P L O T _ N E W R O T
 *  Stub.
 */
/* ARGSUSED */
static int
Plot_newrot(dmp, mat)
struct dm *dmp;
mat_t mat;
{
  return TCL_OK;
}

/*
 *  			P L O T _ O B J E C T
 *  
 *  Set up for an object, transformed as indicated, and with an
 *  object center as specified.  The ratio of object to screen size
 *  is passed in as a convienience.
 *
 *  Returns 0 if object could be drawn, !0 if object was omitted.
 */
/* ARGSUSED */
static int
Plot_drawVList( dmp, vp, mat )
struct dm *dmp;
register struct rt_vlist *vp;
mat_t mat;
{
  static vect_t			last;
  register struct rt_vlist	*tvp;
  int useful = 0;

#if 0
  if( illum )  {
    pl_linmod( ((struct plot_vars *)dmp->dm_vars)->up_fp, "longdashed" );
  } else
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
      if(vclip(start, fin, ((struct plot_vars *)dmp->dm_vars)->clipmin,
		((struct plot_vars *)dmp->dm_vars)->clipmax) == 0)
	continue;
#if 0      
      pl_color( ((struct plot_vars *)dmp->dm_vars)->up_fp, r, g, b );
#endif
      if(((struct plot_vars *)dmp->dm_vars)->is_3D)
	pl_3line( ((struct plot_vars *)dmp->dm_vars)->up_fp, 
		  (int)( start[X] * 2047 ),
		  (int)( start[Y] * 2047 ),
		  (int)( start[Z] * 2047 ),
		  (int)( fin[X] * 2047 ),
		  (int)( fin[Y] * 2047 ),
		  (int)( fin[Z] * 2047 ) );
      else
	pl_line( ((struct plot_vars *)dmp->dm_vars)->up_fp, 
		 (int)( start[X] * 2047 ),
		 (int)( start[Y] * 2047 ),
		 (int)( fin[X] * 2047 ),
		 (int)( fin[Y] * 2047 ) );

      useful = 1;
    }
  }

  return(useful);
}

/*
 *			P L O T _ N O R M A L
 *
 * Restore the display processor to a normal mode of operation
 * (ie, not scaled, rotated, displaced, etc).
 * Turns off windowing.
 */
static int
Plot_normal(dmp)
struct dm *dmp;
{
  return TCL_OK;
}


/*
 *			P L O T _ P U T S
 *
 * Output a string into the displaylist.
 * The starting position of the beam is as specified.
 */
/* ARGSUSED */
static int
Plot_drawString2D( dmp, str, x, y, size )
struct dm *dmp;
register char *str;
int x, y;
int size;
{
  pl_move( ((struct plot_vars *)dmp->dm_vars)->up_fp, x,y);
  pl_label( ((struct plot_vars *)dmp->dm_vars)->up_fp, str);

  return TCL_OK;
}

/*
 *			P L O T _ 2 D _ G O T O
 *
 */
static int
Plot_drawLine2D( dmp, x1, y1, x2, y2 )
struct dm *dmp;
int x1, y1;
int x2, y2;
{
  pl_move( ((struct plot_vars *)dmp->dm_vars)->up_fp, x1,y1);
  pl_cont( ((struct plot_vars *)dmp->dm_vars)->up_fp, x2,y2);

  return TCL_OK;
}


static int
Plot_drawVertex2D(dmp, x, y)
struct dm *dmp;
int x, y;
{
  return Plot_drawLine2D( dmp, x, y, x, y );
}


static int
Plot_setColor(dmp, r, g, b, strict)
struct dm *dmp;
register short r, g, b;
int strict;
{
  pl_color( ((struct plot_vars *)dmp->dm_vars)->up_fp,  (int)r, (int)g, (int)b );
  return TCL_OK;
}


static int
Plot_setLineAttr(dmp, width, dashed)
struct dm *dmp;
int width;
int dashed;
{
  if(dashed)
    pl_linmod( ((struct plot_vars *)dmp->dm_vars)->up_fp, "dotdashed");
  else
    pl_linmod( ((struct plot_vars *)dmp->dm_vars)->up_fp, "solid");

  return TCL_OK;
}


/* ARGSUSED */
static unsigned
Plot_cvtvecs( dmp, sp )
struct dm *dmp;
struct solid *sp;
{
  return( 0 );
}

/*
 * Loads displaylist
 */
static unsigned
Plot_load( dmp, addr, count )
struct dm *dmp;
unsigned addr, count;
{
  struct bu_vls tmp_vls;

  bu_vls_init(&tmp_vls);
  bu_vls_printf(&tmp_vls, "Plot_load(x%x, %d.)\n", addr, count);
  Tcl_AppendResult(interp, bu_vls_addr(&tmp_vls), (char *)NULL);
  bu_vls_free(&tmp_vls);
  return( 0 );
}

/* ARGSUSED */
static int
Plot_debug(dmp, lvl)
struct dm *dmp;
{
  (void)fflush(((struct plot_vars *)dmp->dm_vars)->up_fp);
  Tcl_AppendResult(interp, "flushed\n", (char *)NULL);

  return TCL_OK;
}

static int
Plot_setWinBounds(dmp, w)
struct dm *dmp;
register int w[];
{
  /* Compute the clipping bounds */
  ((struct plot_vars *)dmp->dm_vars)->clipmin[0] = w[1] / 2048.;
  ((struct plot_vars *)dmp->dm_vars)->clipmin[1] = w[3] / 2048.;
  ((struct plot_vars *)dmp->dm_vars)->clipmin[2] = w[5] / 2048.;
  ((struct plot_vars *)dmp->dm_vars)->clipmax[0] = w[0] / 2047.;
  ((struct plot_vars *)dmp->dm_vars)->clipmax[1] = w[2] / 2047.;
  ((struct plot_vars *)dmp->dm_vars)->clipmax[2] = w[4] / 2047.;

  return TCL_OK;
}

static int
Plot_load_startup(dmp)
struct dm *dmp;
{
  char *filename;

  bzero((void *)&head_plot_vars, sizeof(struct plot_vars));
  BU_LIST_INIT( &head_plot_vars.l );

  if((filename = getenv("DM_PLOT_RCFILE")) != (char *)NULL )
    return Tcl_EvalFile(interp, filename);

  return TCL_OK;
}
