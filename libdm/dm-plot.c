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
#include "bn.h"
#include "mater.h"
#include "raytrace.h"
#include "dm.h"
#include "dm-plot.h"
#include "solid.h"

/* Display Manager package interface */

#define PLOTBOUND	1000.0	/* Max magnification in Rot matrix */
struct dm	*plot_open();
static int	plot_close();
static int	plot_drawBegin(), plot_drawEnd();
static int	plot_normal(), plot_loadMatrix();
static int	plot_drawString2D(), plot_drawLine2D();
static int      plot_drawPoint2D();
static int	plot_drawVList();
static int      plot_setColor();
static int      plot_setLineAttr();
static int	plot_setWinBounds(), plot_debug();

struct dm dm_plot = {
  plot_close,
  plot_drawBegin,
  plot_drawEnd,
  plot_normal,
  plot_loadMatrix,
  plot_drawString2D,
  plot_drawLine2D,
  plot_drawPoint2D,
  plot_drawVList,
  plot_setColor,
  plot_setLineAttr,
  plot_setWinBounds,
  plot_debug,
  Nu_int0,
  Nu_int0,
  Nu_int0,
  Nu_int0,
  0,
  0,			/* no displaylist */
  0,                    /* no stereo */
  PLOTBOUND,
  "plot",
  "Screen to UNIX-Plot",
  DM_TYPE_PLOT,
  0,
  0,
  0,
  0,
  0,
  1.0, /* aspect ratio */
  0,
  {0, 0},
  0,
  0,
  0
};

struct plot_vars head_plot_vars;
static mat_t plotmat;

/*
 *			P L O T _ O P E N
 *
 * Fire up the display manager, and the display processor.
 *
 */
struct dm *
plot_open(argc, argv)
int argc;
char *argv[];
{
  static int count = 0;
  struct dm *dmp;

  BU_GETSTRUCT(dmp, dm);
  if(dmp == DM_NULL)
    return DM_NULL;

  *dmp = dm_plot; /* struct copy */

  dmp->dm_vars.priv_vars = (genptr_t)bu_calloc(1, sizeof(struct plot_vars), "plot_open: plot_vars");
  BU_GETSTRUCT(dmp->dm_vars.priv_vars, plot_vars);
  if(dmp->dm_vars.priv_vars == (genptr_t)NULL){
    bu_free(dmp, "plot_open: dmp");
    return DM_NULL;
  }

  bu_vls_init(&((struct plot_vars *)dmp->dm_vars.priv_vars)->vls);
  bu_vls_init(&dmp->dm_pathName);
  bu_vls_init(&dmp->dm_tkName);
  bu_vls_printf(&dmp->dm_pathName, ".dm_plot%d", count++);
  bu_vls_printf(&dmp->dm_tkName, "dm_plot%d", count++);

  /* skip first argument */
  --argc; ++argv;

  /* Process any options */
  ((struct plot_vars *)dmp->dm_vars.priv_vars)->is_3D = 1;          /* 3-D w/color, by default */
  while( argv[0] != (char *)0 && argv[0][0] == '-' )  {
    switch( argv[0][1] )  {
    case '3':
      break;
    case '2':
      ((struct plot_vars *)dmp->dm_vars.priv_vars)->is_3D = 0;		/* 2-D, for portability */
      break;
    case 'g':
      ((struct plot_vars *)dmp->dm_vars.priv_vars)->grid = 1;
      break;
    case 'f':
      ((struct plot_vars *)dmp->dm_vars.priv_vars)->floating = 1;
      break;
    case 'z':
    case 'Z':
      /* Enable Z clipping */
      Tcl_AppendResult(interp, "Clipped in Z to viewing cube\n", (char *)NULL);
      ((struct plot_vars *)dmp->dm_vars.priv_vars)->zclip = 1;
      break;
    default:
      Tcl_AppendResult(interp, "bad PLOT option ", argv[0], "\n", (char *)NULL);
      (void)plot_close(dmp);
      return DM_NULL;
    }
    argv++;
  }
  if( argv[0] == (char *)0 )  {
    Tcl_AppendResult(interp, "no filename or filter specified\n", (char *)NULL);
    (void)plot_close(dmp);
    return DM_NULL;
  }

  if( argv[0][0] == '|' )  {
    bu_vls_strcpy(&((struct plot_vars *)dmp->dm_vars.priv_vars)->vls, &argv[0][1]);
    while( (++argv)[0] != (char *)0 ) {
      bu_vls_strcat( &((struct plot_vars *)dmp->dm_vars.priv_vars)->vls, " " );
      bu_vls_strcat( &((struct plot_vars *)dmp->dm_vars.priv_vars)->vls, argv[0] );
    }

    ((struct plot_vars *)dmp->dm_vars.priv_vars)->is_pipe = 1;
  }else{
    bu_vls_strcpy(&((struct plot_vars *)dmp->dm_vars.priv_vars)->vls, argv[0]);
  }

  if(((struct plot_vars *)dmp->dm_vars.priv_vars)->is_pipe){
    if((((struct plot_vars *)dmp->dm_vars.priv_vars)->up_fp =
	popen( bu_vls_addr(&((struct plot_vars *)dmp->dm_vars.priv_vars)->vls), "w")) == NULL){
      perror( bu_vls_addr(&((struct plot_vars *)dmp->dm_vars.priv_vars)->vls));
      (void)plot_close(dmp);
      return DM_NULL;
    }
    
    Tcl_AppendResult(interp, "piped to ",
		     bu_vls_addr(&((struct plot_vars *)dmp->dm_vars.priv_vars)->vls),
		     "\n", (char *)NULL);
  }else{
    if((((struct plot_vars *)dmp->dm_vars.priv_vars)->up_fp =
	fopen( bu_vls_addr(&((struct plot_vars *)dmp->dm_vars.priv_vars)->vls), "w" )) == NULL){
      perror(bu_vls_addr(&((struct plot_vars *)dmp->dm_vars.priv_vars)->vls));
      (void)plot_close(dmp);
      return DM_NULL;
    }

    Tcl_AppendResult(interp, "plot stored in ",
		     bu_vls_addr(&((struct plot_vars *)dmp->dm_vars.priv_vars)->vls),
		     "\n", (char *)NULL);
  }

  setbuf(((struct plot_vars *)dmp->dm_vars.priv_vars)->up_fp,
	  ((struct plot_vars *)dmp->dm_vars.priv_vars)->ttybuf);

  if(((struct plot_vars *)dmp->dm_vars.priv_vars)->is_3D)
    pl_3space( ((struct plot_vars *)dmp->dm_vars.priv_vars)->up_fp,
	       -2048, -2048, -2048, 2048, 2048, 2048 );
  else
    pl_space( ((struct plot_vars *)dmp->dm_vars.priv_vars)->up_fp,
	      -2048, -2048, 2048, 2048 );

  bn_mat_idn(plotmat);

  return dmp;
}

/*
 *  			P L O T _ C L O S E
 *  
 *  Gracefully release the display.
 */
static int
plot_close(dmp)
struct dm *dmp;
{
  (void)fflush(((struct plot_vars *)dmp->dm_vars.priv_vars)->up_fp);

  if(((struct plot_vars *)dmp->dm_vars.priv_vars)->is_pipe)
    pclose(((struct plot_vars *)dmp->dm_vars.priv_vars)->up_fp); /* close pipe, eat dead children */
  else
    fclose(((struct plot_vars *)dmp->dm_vars.priv_vars)->up_fp);

  bu_vls_free(&dmp->dm_pathName);
  bu_free(dmp->dm_vars.priv_vars, "plot_close: plot_vars");
  bu_free(dmp, "plot_close: dmp");
  return TCL_OK;
}

/*
 *			P L O T _ P R O L O G
 *
 * There are global variables which are parameters to this routine.
 */
static int
plot_drawBegin(dmp)
struct dm *dmp;
{
  /* We expect the screen to be blank so far, from last frame flush */

  return TCL_OK;
}

/*
 *			P L O T _ E P I L O G
 */
static int
plot_drawEnd(dmp)
struct dm *dmp;
{
  pl_flush( ((struct plot_vars *)dmp->dm_vars.priv_vars)->up_fp ); /* BRL-specific command */
  pl_erase( ((struct plot_vars *)dmp->dm_vars.priv_vars)->up_fp ); /* forces drawing */
  (void)fflush( ((struct plot_vars *)dmp->dm_vars.priv_vars)->up_fp );

  return TCL_OK;
}

/*
 *  			P L O T _ L O A D M A T R I X
 *
 *  Load a new transformation matrix.  This will be followed by
 *  many calls to plot_drawVList().
 */
static int
plot_loadMatrix(dmp, mat, which_eye)
struct dm *dmp;
mat_t mat;
int which_eye;
{
  if(((struct plot_vars *)dmp->dm_vars.priv_vars)->debug){
    struct bu_vls tmp_vls;

    Tcl_AppendResult(interp, "plot_loadMatrix()\n", (char *)NULL);

    bu_vls_init(&tmp_vls);
    bu_vls_printf(&tmp_vls, "which eye = %d\t", which_eye);
    bu_vls_printf(&tmp_vls, "transformation matrix = \n");
    bu_vls_printf(&tmp_vls, "%g %g %g %g\n", mat[0], mat[4], mat[8],mat[12]);
    bu_vls_printf(&tmp_vls, "%g %g %g %g\n", mat[1], mat[5], mat[9],mat[13]);
    bu_vls_printf(&tmp_vls, "%g %g %g %g\n", mat[2], mat[6], mat[10],mat[14]);
    bu_vls_printf(&tmp_vls, "%g %g %g %g\n", mat[3], mat[7], mat[11],mat[15]);

    Tcl_AppendResult(interp, bu_vls_addr(&tmp_vls), (char *)NULL);
    bu_vls_free(&tmp_vls);
  }

  bn_mat_copy(plotmat, mat);
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
static int
plot_drawVList( dmp, vp )
struct dm *dmp;
register struct rt_vlist *vp;
{
  static vect_t			last;
  register struct rt_vlist	*tvp;
  int useful = 0;

  if(((struct plot_vars *)dmp->dm_vars.priv_vars)->floating){
    rt_vlist_to_uplot(((struct plot_vars *)dmp->dm_vars.priv_vars)->up_fp, &vp->l);
		      
    return TCL_OK;
  }

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
	MAT4X3PNT( last, plotmat, *pt );
	continue;
      case RT_VLIST_POLY_DRAW:
      case RT_VLIST_POLY_END:
      case RT_VLIST_LINE_DRAW:
	/* draw */
	MAT4X3PNT( fin, plotmat, *pt );
	VMOVE( start, last );
	VMOVE( last, fin );
	break;
      }
      if(vclip(start, fin, ((struct plot_vars *)dmp->dm_vars.priv_vars)->clipmin,
		((struct plot_vars *)dmp->dm_vars.priv_vars)->clipmax) == 0)
	continue;

      if(((struct plot_vars *)dmp->dm_vars.priv_vars)->is_3D)
	pl_3line( ((struct plot_vars *)dmp->dm_vars.priv_vars)->up_fp, 
		  (int)( start[X] * 2047 ),
		  (int)( start[Y] * 2047 ),
		  (int)( start[Z] * 2047 ),
		  (int)( fin[X] * 2047 ),
		  (int)( fin[Y] * 2047 ),
		  (int)( fin[Z] * 2047 ) );
      else
	pl_line( ((struct plot_vars *)dmp->dm_vars.priv_vars)->up_fp, 
		 (int)( start[X] * 2047 ),
		 (int)( start[Y] * 2047 ),
		 (int)( fin[X] * 2047 ),
		 (int)( fin[Y] * 2047 ) );

      useful = 1;
    }
  }

  if(useful)
    return TCL_OK;

  return TCL_ERROR;
}

/*
 *			P L O T _ N O R M A L
 *
 * Restore the display processor to a normal mode of operation
 * (ie, not scaled, rotated, displaced, etc).
 * Turns off windowing.
 */
static int
plot_normal(dmp)
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
plot_drawString2D( dmp, str, x, y, size, use_aspect )
struct dm *dmp;
register char *str;
fastf_t x, y;
int size;
int use_aspect;
{
  int sx, sy;

  sx = x * 2047;
  sy = y + 2047;
  pl_move( ((struct plot_vars *)dmp->dm_vars.priv_vars)->up_fp, sx, sy);
  pl_label( ((struct plot_vars *)dmp->dm_vars.priv_vars)->up_fp, str);

  return TCL_OK;
}

/*
 *			P L O T _ 2 D _ G O T O
 *
 */
static int
plot_drawLine2D( dmp, x1, y1, x2, y2 )
struct dm *dmp;
fastf_t x1, y1;
fastf_t x2, y2;
{
  int sx1, sy1;
  int sx2, sy2;

  sx1 = x1 * 2047;
  sx2 = x2 * 2047;
  sy1 = y1 + 2047;
  sy2 = y2 + 2047;
  pl_move( ((struct plot_vars *)dmp->dm_vars.priv_vars)->up_fp, sx1, sy1);
  pl_cont( ((struct plot_vars *)dmp->dm_vars.priv_vars)->up_fp, sx2, sy2);

  return TCL_OK;
}


static int
plot_drawPoint2D(dmp, x, y)
struct dm *dmp;
fastf_t x, y;
{
  return plot_drawLine2D( dmp, x, y, x, y );
}


static int
plot_setColor(dmp, r, g, b, strict)
struct dm *dmp;
register short r, g, b;
int strict;
{
  pl_color( ((struct plot_vars *)dmp->dm_vars.priv_vars)->up_fp,  (int)r, (int)g, (int)b );
  return TCL_OK;
}


static int
plot_setLineAttr(dmp, width, style)
struct dm *dmp;
int width;
int style;
{
  dmp->dm_lineWidth = width;
  dmp->dm_lineStyle = style;

  if(style == DM_DASHED_LINE)
    pl_linmod( ((struct plot_vars *)dmp->dm_vars.priv_vars)->up_fp, "dotdashed");
  else
    pl_linmod( ((struct plot_vars *)dmp->dm_vars.priv_vars)->up_fp, "solid");

  return TCL_OK;
}

/* ARGSUSED */
static int
plot_debug(dmp, lvl)
struct dm *dmp;
{
  (void)fflush(((struct plot_vars *)dmp->dm_vars.priv_vars)->up_fp);
  Tcl_AppendResult(interp, "flushed\n", (char *)NULL);

  return TCL_OK;
}

static int
plot_setWinBounds(dmp, w)
struct dm *dmp;
register int w[];
{
  /* Compute the clipping bounds */
  ((struct plot_vars *)dmp->dm_vars.priv_vars)->clipmin[0] = w[0] / 2048.;
  ((struct plot_vars *)dmp->dm_vars.priv_vars)->clipmax[0] = w[1] / 2047.;
  ((struct plot_vars *)dmp->dm_vars.priv_vars)->clipmin[1] = w[2] / 2048.;
  ((struct plot_vars *)dmp->dm_vars.priv_vars)->clipmax[1] = w[3] / 2047.;

  if(((struct plot_vars *)dmp->dm_vars.priv_vars)->zclip){
    ((struct plot_vars *)dmp->dm_vars.priv_vars)->clipmin[2] = w[4] / 2048.;
    ((struct plot_vars *)dmp->dm_vars.priv_vars)->clipmax[2] = w[5] / 2047.;
  }else{
    ((struct plot_vars *)dmp->dm_vars.priv_vars)->clipmin[2] = -1.0e20;
    ((struct plot_vars *)dmp->dm_vars.priv_vars)->clipmax[2] = 1.0e20;
  }

  return TCL_OK;
}
