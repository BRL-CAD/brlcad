/* Experimental. */
/*
 *			D M - P E X . C
 *
 *  An X Window System interface for MGED
 *  that uses PEX. This display manager started
 *  out with the innards from DM-X.C
 *
 *  Author -
 *      Robert Parker
 *
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1988 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#define DO_XSELECTINPUT 0
#define SET_COLOR( r, g, b, c ) { \
	(c).rgb.red = (r); \
	(c).rgb.green = (g); \
	(c).rgb.blue = (b);}

#include "conf.h"
#include "tk.h"

#ifdef HAVE_XOSDEFS_H
#include <X11/Xfuncproto.h>
#include <X11/Xosdefs.h>
#endif
#if defined(linux)
#	undef   X_NOT_STDC_ENV
#	undef   X_NOT_POSIX
#endif
#define XLIB_ILLEGAL_ACCESS	/* necessary on facist SGI 5.0.1 */

#include <X11/PEX5/PEXlib.h>
#include <X11/Xutil.h>

#include "machine.h"
#include "externs.h"
#include "bu.h"
#include "vmath.h"
#if 0
#include "mater.h"
#endif
#include "raytrace.h"
#include "dm.h"
#include "dm-pex.h"
#if 0
#include "solid.h"
#endif

#define IMMED_MODE_SPT(info) (((info)->subset_info & 0xffff) ==\
			      PEXCompleteImplementation ||\
			      (info)->subset_info & PEXImmediateMode)
#define Pex_VMOVE(a,b) {\
			(a).x = (b)[X];\
			(a).y = (b)[Y];\
			(a).z = (b)[Z]; }

struct pex_vars head_pex_vars;
void     Pex_establish_perspective();
void     Pex_set_perspective();

static void     Pex_var_init();
static void     Pex_setup_renderer();
static void     Pex_mat_copy();
static void     Pex_load_startup();

static void	label();
static void	draw();

/* Display Manager package interface */

#define PLOTBOUND	1000.0	/* Max magnification in Rot matrix */
static int      Pex_init();
static int	Pex_open();
static void	Pex_close();
static void	Pex_input();
static void	Pex_prolog(), Pex_epilog();
static void	Pex_normal(), Pex_newrot();
static void	Pex_update();
static void	Pex_puts(), Pex_2d_line(), Pex_light();
static int	Pex_object();
static unsigned Pex_cvtvecs(), Pex_load();
static void	Pex_viewchange(), Pex_colorchange();
static void	Pex_window(), Pex_debug(), Pex_selectargs();

struct dm dm_pex = {
  Pex_init,
  Pex_open, Pex_close,
  Pex_input,
  Pex_prolog, Pex_epilog,
  Pex_normal, Pex_newrot,
  Pex_update,
  Pex_puts, Pex_2d_line,
  Pex_light,
  Pex_object,	Pex_cvtvecs, Pex_load,
  0,
  Pex_viewchange,
  Pex_colorchange,
  Pex_window, Pex_debug, 0, 0,
  0,				/* no displaylist */
  PLOTBOUND,
  "pex", "X Window System (X11)",
  0,
  0,
  0,
  0,
  0,
  0
};

extern Tcl_Interp *interp;
extern Tk_Window tkwin;

static int view_num = 0;
#if TRY_DEPTHCUE
static int cue_num = 1;
#endif

static int perspective_table[] = { 30, 45, 60, 90 };

static int
Pex_init(dmp, argc, argv)
struct dm *dmp;
int argc;
char *argv[];
{
  static int count = 0;

  /* Only need to do this once for this display manager */
  if(!count)
    Pex_load_startup(dmp);

  bu_vls_init(&dmp->dm_pathName);
  bu_vls_printf(&dmp->dmr_pathName, ".dm_pex%d", count++);

  dmp->dmr_vars = bu_calloc(1, sizeof(struct pex_vars), "Pex_init: pex_vars");
  ((struct pex_vars *)dmp->dmr_vars)->perspective_angle = 3;

  /* initialize the modifiable variables */
  ((struct pex_vars *)dmp->dmr_vars)->mvars.dummy_perspective = 1;

  if(BU_LIST_IS_EMPTY(&head_pex_vars.l))
    Tk_CreateGenericHandler(dmp->dmr_eventhandler, (ClientData)DM_TYPE_PEX);

  BU_LIST_APPEND(&head_pex_vars.l, &((struct pex_vars *)dmp->dmr_vars)->l);

  if(dmp->dmr_vars)
        return TCL_OK;

  return TCL_ERROR;
}


/*
 *			P E X _ O P E N
 *
 * Fire up the display manager, and the display processor.
 *
 */
static int
Pex_open(dmp)
struct dm *dmp;
{
  int first_event, first_error;
  char *cp;
  XGCValues gcv;
  XColor a_color;
  Visual *a_visual;
  int a_screen;
  Colormap  a_cmap;
  struct bu_vls str;
  Display *tmp_dpy;
  char pex_err[80];
  PEXExtensionInfo *pex_info;

  bu_vls_init(&str);

  ((struct pex_vars *)dmp->dmr_vars)->fontstruct = NULL;

  /* Make xtkwin a toplevel window */
  ((struct pex_vars *)dmp->dmr_vars)->xtkwin = Tk_CreateWindowFromPath(interp, tkwin,
		       bu_vls_addr(&dmp->dmr_pathName), dmp->dmr_dname);

  /*
   * Create the X drawing window by calling create_x which
   * is defined in xinit.tk
   */
  bu_vls_strcpy(&str, "init_x ");
  bu_vls_printf(&str, "%s\n", bu_vls_addr(&dmp->dmr_pathName));

  if(Tcl_Eval(interp, bu_vls_addr(&str)) == TCL_ERROR){
    bu_vls_free(&str);
    return TCL_ERROR;
  }

  bu_vls_free(&str);
  ((struct pex_vars *)dmp->dmr_vars)->dpy = Tk_Display(((struct pex_vars *)dmp->dmr_vars)->xtkwin);
  ((struct pex_vars *)dmp->dmr_vars)->width =
    DisplayWidth(((struct pex_vars *)dmp->dmr_vars)->dpy,
		 DefaultScreen(((struct pex_vars *)dmp->dmr_vars)->dpy)) - 20;
  ((struct pex_vars *)dmp->dmr_vars)->height =
    DisplayHeight(((struct pex_vars *)dmp->dmr_vars)->dpy,
		  DefaultScreen(((struct pex_vars *)dmp->dmr_vars)->dpy)) - 20;

  /* Make window square */
  if(((struct pex_vars *)dmp->dmr_vars)->height < ((struct pex_vars *)dmp->dmr_vars)->width)
    ((struct pex_vars *)dmp->dmr_vars)->width = ((struct pex_vars *)dmp->dmr_vars)->height;
  else
    ((struct pex_vars *)dmp->dmr_vars)->height = ((struct pex_vars *)dmp->dmr_vars)->width;

  Tk_GeometryRequest(((struct pex_vars *)dmp->dmr_vars)->xtkwin,
		     ((struct pex_vars *)dmp->dmr_vars)->width, 
		     ((struct pex_vars *)dmp->dmr_vars)->height);
#if 0
  /*XXX*/
  XSynchronize(((struct pex_vars *)dmp->dmr_vars)->dpy, TRUE);
#endif

  Tk_MakeWindowExist(((struct pex_vars *)dmp->dmr_vars)->xtkwin);
  ((struct pex_vars *)dmp->dmr_vars)->win =
      Tk_WindowId(((struct pex_vars *)dmp->dmr_vars)->xtkwin);

#ifdef DOUBLE_BUFFERING_WITH_PIXMAPS
  ((struct pex_vars *)dmp->dmr_vars)->pix_width = ((struct pex_vars *)dmp->dmr_vars)->width;
  ((struct pex_vars *)dmp->dmr_vars)->pix_height = ((struct pex_vars *)dmp->dmr_vars)->height;
  ((struct pex_vars *)dmp->dmr_vars)->pix =
    Tk_GetPixmap(((struct pex_vars *)dmp->dmr_vars)->dpy,
		 DefaultRootWindow(((struct pex_vars *)dmp->dmr_vars)->dpy),
		 ((struct pex_vars *)dmp->dmr_vars)->width,
		 ((struct pex_vars *)dmp->dmr_vars)->height,
		 Tk_Depth(((struct pex_vars *)dmp->dmr_vars)->xtkwin));
#endif

  a_screen = Tk_ScreenNumber(((struct pex_vars *)dmp->dmr_vars)->xtkwin);
  a_visual = Tk_Visual(((struct pex_vars *)dmp->dmr_vars)->xtkwin);

  /* Get color map indices for the colors we use. */
  ((struct pex_vars *)dmp->dmr_vars)->black = BlackPixel( ((struct pex_vars *)dmp->dmr_vars)->dpy, a_screen );
  ((struct pex_vars *)dmp->dmr_vars)->white = WhitePixel( ((struct pex_vars *)dmp->dmr_vars)->dpy, a_screen );

  a_cmap = Tk_Colormap(((struct pex_vars *)dmp->dmr_vars)->xtkwin);
  a_color.red = 255<<8;
  a_color.green=0;
  a_color.blue=0;
  a_color.flags = DoRed | DoGreen| DoBlue;
  if ( ! XAllocColor(((struct pex_vars *)dmp->dmr_vars)->dpy, a_cmap, &a_color)) {
    bu_log( "dm-X: Can't Allocate red\n");
    return TCL_ERROR;
  }
  ((struct pex_vars *)dmp->dmr_vars)->red = a_color.pixel;
    if ( ((struct pex_vars *)dmp->dmr_vars)->red == ((struct pex_vars *)dmp->dmr_vars)->white )
      ((struct pex_vars *)dmp->dmr_vars)->red = ((struct pex_vars *)dmp->dmr_vars)->black;

    a_color.red = 200<<8;
    a_color.green=200<<8;
    a_color.blue=0<<8;
    a_color.flags = DoRed | DoGreen| DoBlue;
    if ( ! XAllocColor(((struct pex_vars *)dmp->dmr_vars)->dpy, a_cmap, &a_color)) {
	bu_log( "dm-X: Can't Allocate yellow\n");
	return TCL_ERROR;
    }
    ((struct pex_vars *)dmp->dmr_vars)->yellow = a_color.pixel;
    if ( ((struct pex_vars *)dmp->dmr_vars)->yellow == ((struct pex_vars *)dmp->dmr_vars)->white )
      ((struct pex_vars *)dmp->dmr_vars)->yellow = ((struct pex_vars *)dmp->dmr_vars)->black;
    
    a_color.red = 0;
    a_color.green=0;
    a_color.blue=255<<8;
    a_color.flags = DoRed | DoGreen| DoBlue;
    if ( ! XAllocColor(((struct pex_vars *)dmp->dmr_vars)->dpy, a_cmap, &a_color)) {
	bu_log( "dm-X: Can't Allocate blue\n");
	return TCL_ERROR;
    }
    ((struct pex_vars *)dmp->dmr_vars)->blue = a_color.pixel;
    if ( ((struct pex_vars *)dmp->dmr_vars)->blue == ((struct pex_vars *)dmp->dmr_vars)->white )
      ((struct pex_vars *)dmp->dmr_vars)->blue = ((struct pex_vars *)dmp->dmr_vars)->black;

    a_color.red = 128<<8;
    a_color.green=128<<8;
    a_color.blue= 128<<8;
    a_color.flags = DoRed | DoGreen| DoBlue;
    if ( ! XAllocColor(((struct pex_vars *)dmp->dmr_vars)->dpy, a_cmap, &a_color)) {
	bu_log( "dm-X: Can't Allocate gray\n");
	return TCL_ERROR;
    }
    ((struct pex_vars *)dmp->dmr_vars)->gray = a_color.pixel;
    if ( ((struct pex_vars *)dmp->dmr_vars)->gray == ((struct pex_vars *)dmp->dmr_vars)->white )
      ((struct pex_vars *)dmp->dmr_vars)->gray = ((struct pex_vars *)dmp->dmr_vars)->black;

    /* Select border, background, foreground colors,
     * and border width.
     */
    if( a_visual->class == GrayScale || a_visual->class == StaticGray ) {
	((struct pex_vars *)dmp->dmr_vars)->is_monochrome = 1;
	((struct pex_vars *)dmp->dmr_vars)->bd = BlackPixel( ((struct pex_vars *)dmp->dmr_vars)->dpy, a_screen );
	((struct pex_vars *)dmp->dmr_vars)->bg = WhitePixel( ((struct pex_vars *)dmp->dmr_vars)->dpy, a_screen );
	((struct pex_vars *)dmp->dmr_vars)->fg = BlackPixel( ((struct pex_vars *)dmp->dmr_vars)->dpy, a_screen );
    } else {
	/* Hey, it's a color server.  Ought to use 'em! */
	((struct pex_vars *)dmp->dmr_vars)->is_monochrome = 0;
	((struct pex_vars *)dmp->dmr_vars)->bd = WhitePixel( ((struct pex_vars *)dmp->dmr_vars)->dpy, a_screen );
	((struct pex_vars *)dmp->dmr_vars)->bg = BlackPixel( ((struct pex_vars *)dmp->dmr_vars)->dpy, a_screen );
	((struct pex_vars *)dmp->dmr_vars)->fg = WhitePixel( ((struct pex_vars *)dmp->dmr_vars)->dpy, a_screen );
    }

    if( !((struct pex_vars *)dmp->dmr_vars)->is_monochrome &&
	((struct pex_vars *)dmp->dmr_vars)->fg != ((struct pex_vars *)dmp->dmr_vars)->red &&
	((struct pex_vars *)dmp->dmr_vars)->red != ((struct pex_vars *)dmp->dmr_vars)->black )
      ((struct pex_vars *)dmp->dmr_vars)->fg = ((struct pex_vars *)dmp->dmr_vars)->red;

    gcv.foreground = ((struct pex_vars *)dmp->dmr_vars)->fg;
    gcv.background = ((struct pex_vars *)dmp->dmr_vars)->bg;

#ifndef CRAY2
#if 1
    cp = FONT6;
    if ( (((struct pex_vars *)dmp->dmr_vars)->fontstruct =
	 XLoadQueryFont(((struct pex_vars *)dmp->dmr_vars)->dpy, cp)) == NULL ) {
      /* Try hardcoded backup font */
      if ( (((struct pex_vars *)dmp->dmr_vars)->fontstruct =
	    XLoadQueryFont(((struct pex_vars *)dmp->dmr_vars)->dpy, FONTBACK)) == NULL) {
	bu_log( "dm-X: Can't open font '%s' or '%s'\n", cp, FONTBACK );
	return TCL_ERROR;
      }
    }
    gcv.font = ((struct pex_vars *)dmp->dmr_vars)->fontstruct->fid;
#endif
    ((struct pex_vars *)dmp->dmr_vars)->gc = XCreateGC(((struct pex_vars *)dmp->dmr_vars)->dpy,
					       ((struct pex_vars *)dmp->dmr_vars)->win,
					       (GCFont|GCForeground|GCBackground),
						&gcv);
#else
    ((struct pex_vars *)dmp->dmr_vars)->gc = XCreateGC(((struct pex_vars *)dmp->dmr_vars)->dpy,
					       ((struct pex_vars *)dmp->dmr_vars)->win,
					       (GCForeground|GCBackground),
					       &gcv);
#endif

/* Begin PEX stuff. */
    if(PEXInitialize(((struct pex_vars *)dmp->dmr_vars)->dpy,
		     &pex_info, 80, pex_err) != 0){
      bu_vls_free(&str);
      bu_log("Pex_setup: %s\n", pex_err);
      return TCL_ERROR;
    }

    if(!IMMED_MODE_SPT(pex_info)){
      bu_vls_free(&str);
      bu_log("Pex_setup: Immediate mode is not supported.\n");
      return TCL_ERROR;
    }

    Pex_setup_renderer(dmp);

#if DO_XSELECTINPUT
    /* start with constant tracking OFF */
    XSelectInput(((struct pex_vars *)dmp->dmr_vars)->dpy,
		 ((struct pex_vars *)dmp->dmr_vars)->win,
		 ExposureMask|ButtonPressMask|KeyPressMask|StructureNotifyMask);
#endif

#if 1
    Pex_configure_window_shape(dmp);
#endif

    Tk_SetWindowBackground(((struct pex_vars *)dmp->dmr_vars)->xtkwin, ((struct pex_vars *)dmp->dmr_vars)->bg);
    Tk_MapWindow(((struct pex_vars *)dmp->dmr_vars)->xtkwin);

    return TCL_OK;
}

/*
 *  			P E X _ C L O S E
 *  
 *  Gracefully release the display.
 */
static void
Pex_close(dmp)
struct dm *dmp;
{
  if(((struct pex_vars *)dmp->dmr_vars)->gc != 0)
    XFreeGC(((struct pex_vars *)dmp->dmr_vars)->dpy, ((struct pex_vars *)dmp->dmr_vars)->gc);

#ifdef DOUBLE_BUFFERING_WITH_PIXMAPS
  if(((struct pex_vars *)dmp->dmr_vars)->pix != 0)
     Tk_FreePixmap(((struct pex_vars *)dmp->dmr_vars)->dpy, ((struct pex_vars *)dmp->dmr_vars)->pix);
#endif

  if(((struct pex_vars *)dmp->dmr_vars)->xtkwin != 0)
    Tk_DestroyWindow(((struct pex_vars *)dmp->dmr_vars)->xtkwin);

  if(((struct pex_vars *)dmp->dmr_vars)->l.forw != BU_LIST_NULL)
    BU_LIST_DEQUEUE(&((struct pex_vars *)dmp->dmr_vars)->l);

  bu_free(dmp->dmr_vars, "Pex_close: dmp->dmr_vars");

  if(BU_LIST_IS_EMPTY(&head_pex_vars.l))
    Tk_DeleteGenericHandler(dmp->dmr_eventhandler, (ClientData)DM_TYPE_PEX);
}

/*
 *			P E X _ P R O L O G
 *
 * There are global variables which are parameters to this routine.
 */
static void
Pex_prolog(dmp)
struct dm *dmp;
{
#ifdef DOUBLE_BUFFERING_WITH_PIXMAPS
  XGCValues       gcv;

  gcv.foreground = ((struct pex_vars *)dmp->dmr_vars)->bg;
  XChangeGC(((struct pex_vars *)dmp->dmr_vars)->dpy, ((struct pex_vars *)dmp->dmr_vars)->gc,
	    GCForeground, &gcv);
  XFillRectangle(((struct pex_vars *)dmp->dmr_vars)->dpy, ((struct pex_vars *)dmp->dmr_vars)->pix,
		 ((struct pex_vars *)dmp->dmr_vars)->gc, 0, 0,
		 ((struct pex_vars *)dmp->dmr_vars)->width + 1,
		 ((struct pex_vars *)dmp->dmr_vars)->height + 1);
#else
  XClearWindow(((struct pex_vars *)dmp->dmr_vars)->dpy, ((struct pex_vars *)dmp->dmr_vars)->win);
#endif
}

/*
 *			P E X _ E P I L O G
 */
static void
Pex_epilog(dmp)
struct dm *dmp;
{
  /* Put the center point up last */
  draw( dmp, 0, 0, 0, 0 );

#ifdef DOUBLE_BUFFERING_WITH_PIXMAPS
  XCopyArea(((struct pex_vars *)dmp->dmr_vars)->dpy, ((struct pex_vars *)dmp->dmr_vars)->pix,
	    ((struct pex_vars *)dmp->dmr_vars)->win, ((struct pex_vars *)dmp->dmr_vars)->gc, 0, 0,
	    ((struct pex_vars *)dmp->dmr_vars)->width, ((struct pex_vars *)dmp->dmr_vars)->height,
	    0, 0);
#endif

  /* Prevent lag between events and updates */
  XSync(((struct pex_vars *)dmp->dmr_vars)->dpy, 0);
}

/*
 *  			P E X _ N E W R O T
 */
/* ARGSUSED */
static void
Pex_newrot(dmp, mat)
struct dm *dmp;
mat_t mat;
{
  float *mptr;
  int perspective;
  int err;
  PEXViewEntry view;

#if 0
  static PEXNPCSubVolume viewport = { { 0.0, 0.0, 0.0 }, { 1.0, 1.0, 1.0 } };

  /* specified in view coordinates */
  static PEXCoord prp = { 0.0, 0.0, 2.0 };
  static PEXCoord2D view_win[] = { {-2.0, -2.0}, {2.0, 2.0} };
  static double front = 2.0;
  static double view_plane = 0.0;
  static double back = -2.0;

  /* specified in world coordinates */
  static PEXVector vpn = { 0.5, 0.5, 1.0 };
  static PEXVector vuv = { 0.0, 1.0, 0.0 };
  static PEXCoord vrp = { 0.0, 0.0, 0.0 };
  
  if((err = PEXViewOrientationMatrix(&vrp, &vpn, &vuv, view.orientation)) != 0){
    bu_log("Pex_newrot: bad PEXViewOrientationMatrix return - %d\n", err);
    return;
  }
#else
  static PEXNPCSubVolume viewport = { { 0.0, 0.0, 0.0 }, { 1.0, 1.0, 1.0 } };
  static PEXCoord2D view_win[] = { { -1.0, -1.0 }, { 1.0, 1.0 } };
  static PEXCoord prp = { 0.0, 0.0, 1.0 };
  static double front = 1.0;
  static double view_plane = 0.0;
  static double back = -1.0;

  Pex_mat_copy(view.orientation, mat);
#endif

  perspective = 0;
  if((err = PEXViewMappingMatrix(view_win, &viewport, perspective,
				 &prp, view_plane, back, front,
				 view.mapping)) != 0){
    bu_log("Pex_newrot: bad PEXViewMappingMatrix return - %d\n", err);
    return;
  }

#if 1
  view.clip_flags = 0;
#else
  view.clip_flags = PEXClippingAll;
#endif
  view.clip_limits = viewport;

  PEXSetTableEntries(((struct pex_vars *)dmp->dmr_vars)->dpy,
		     ((struct pex_vars *)dmp->dmr_vars)->rattrs.view_table,
		     view_num, 1, PEXLUTView, (PEXPointer)&view);
#if 0
  PEXSetViewIndex(((struct pex_vars *)dmp->dmr_vars)->dpy,
		  ((struct pex_vars *)dmp->dmr_vars)->renderer,
		  PEXOCRender, view_num);
#endif
}

/*
 *  			P E X _ O B J E C T
 *  
 *  Set up for an object, transformed as indicated, and with an
 *  object center as specified.  The ratio of object to screen size
 *  is passed in as a convienience.
 *
 *  Returns 0 if object could be drawn, !0 if object was omitted.
 */

/* ARGSUSED */
static int
Pex_object( dmp, vp, m, linestyle, r, g, b )
struct dm *dmp;
register struct rt_vlist *vp;
fastf_t *m;
int linestyle;
register short r, g, b;
{
  register struct rt_vlist    *tvp;
  PEXCoord coord_buf[1024];
  PEXCoord *cp;                /* current coordinate */
  int first;
  int ncoord;                  /* number of coordinates */
  PEXColor color;

  PEXBeginRendering(((struct pex_vars *)dmp->dmr_vars)->dpy,
#ifdef DOUBLE_BUFFERING_WITH_PIXMAPS
		    ((struct pex_vars *)dmp->dmr_vars)->pix,
#else
		    ((struct pex_vars *)dmp->dmr_vars)->win,
#endif
		    ((struct pex_vars *)dmp->dmr_vars)->renderer);
  {
#if 0
    if( illum ){
      SET_COLOR( 0.9, 0.9, 0.9, color );
    }else{
      SET_COLOR( r / 255.0, g / 255.0,
		 b / 255.0, color );
    }
#else
    SET_COLOR( r / 255.0, g / 255.0, b / 255.0, color );
#endif

    PEXSetLineColor(((struct pex_vars *)dmp->dmr_vars)->dpy,
		    ((struct pex_vars *)dmp->dmr_vars)->renderer,
		    PEXOCRender, PEXColorTypeRGB, &color);

    PEXSetLineWidth(((struct pex_vars *)dmp->dmr_vars)->dpy,
		    ((struct pex_vars *)dmp->dmr_vars)->renderer,
		    PEXOCRender, 1.0);

    if( linestyle )
      PEXSetLineType(((struct pex_vars *)dmp->dmr_vars)->dpy,
		     ((struct pex_vars *)dmp->dmr_vars)->renderer,
		     PEXOCRender, PEXLineTypeDashed);
    else
      PEXSetLineType(((struct pex_vars *)dmp->dmr_vars)->dpy,
		     ((struct pex_vars *)dmp->dmr_vars)->renderer,
		     PEXOCRender, PEXLineTypeSolid);

    ncoord = 0;
    cp = coord_buf;
    first = 1;
    for( BU_LIST_FOR( tvp, rt_vlist, &vp->l ) )  {
      register int	i;
      register int	nused = tvp->nused;
      register int	*cmd = tvp->cmd;
      register point_t *pt = tvp->pt;

      /* Viewing region is from -1.0 to +1.0 */
      /* 2^31 ~= 2e9 -- dynamic range of a long int */
      /* 2^(31-11) = 2^20 ~= 1e6 */
      for( i = 0; i < nused; i++,cmd++,pt++ )  {
	switch( *cmd )  {
	case RT_VLIST_POLY_START:
	case RT_VLIST_POLY_VERTNORM:
	  break;
	case RT_VLIST_LINE_MOVE:
	  /* Move, start line */
	  if( first == 0 ){
	    PEXPolyline(((struct pex_vars *)dmp->dmr_vars)->dpy,
			((struct pex_vars *)dmp->dmr_vars)->renderer,
			PEXOCRender, ncoord, coord_buf);

	    ncoord = 0;
	    cp = coord_buf;
	  }

	  first = 0;
	  Pex_VMOVE(*cp, *pt);
	  ++cp;
	  ++ncoord;
	  break;
	case RT_VLIST_LINE_DRAW:
	case RT_VLIST_POLY_MOVE:
	case RT_VLIST_POLY_DRAW:
	  Pex_VMOVE(*cp, *pt);
	  ++cp;
	  ++ncoord;
	  break;
	case RT_VLIST_POLY_END:
	  Pex_VMOVE(*cp, *pt);
	  ++ncoord;

	  PEXPolyline(((struct pex_vars *)dmp->dmr_vars)->dpy,
		      ((struct pex_vars *)dmp->dmr_vars)->renderer,
		      PEXOCRender, ncoord, coord_buf);

	  ncoord = 0;
	  cp = coord_buf;
	  first = 1;
	  break;
	}
      }
    }

    if( first == 0)
      PEXPolyline(((struct pex_vars *)dmp->dmr_vars)->dpy,
		  ((struct pex_vars *)dmp->dmr_vars)->renderer,
		  PEXOCRender, ncoord, coord_buf);

    if (linestyle) /* restore solid lines */
      PEXSetLineType(((struct pex_vars *)dmp->dmr_vars)->dpy,
		     ((struct pex_vars *)dmp->dmr_vars)->renderer,
		     PEXOCRender, PEXLineTypeSolid);
  }
  PEXEndRendering(((struct pex_vars *)dmp->dmr_vars)->dpy,
		  ((struct pex_vars *)dmp->dmr_vars)->renderer, True);

  return 1;        /* OK */
}

/*
 *			P E X _ N O R M A L
 *
 * Restore the display processor to a normal mode of operation
 * (ie, not scaled, rotated, displaced, etc).
 * Turns off windowing.
 */
static void
Pex_normal(dmp)
struct dm *dmp;
{
	return;
}

/*
 *			P E X _ U P D A T E
 *
 * Transmit accumulated displaylist to the display processor.
 */
static void
Pex_update(dmp)
struct dm *dmp;
{
    XFlush(((struct pex_vars *)dmp->dmr_vars)->dpy);
}

/*
 *			P E X _ P U T S
 *
 * Output a string into the displaylist.
 * The starting position of the beam is as specified.
 */
/* ARGSUSED */
static void
Pex_puts( dmp, str, x, y, size, r, g, b )
struct dm *dmp;
register char *str;
int x, y;
int size;
register short r, g, b;
{
	XGCValues gcv;
#if 0
	unsigned long fg;

	switch( color )  {
	case DM_BLACK:
		fg = ((struct pex_vars *)dmp->dmr_vars)->black;
		break;
	case DM_RED:
		fg = ((struct pex_vars *)dmp->dmr_vars)->red;
		break;
	case DM_BLUE:
		fg = ((struct pex_vars *)dmp->dmr_vars)->blue;
		break;
	default:
	case DM_YELLOW:
		fg = ((struct pex_vars *)dmp->dmr_vars)->yellow;
		break;
	case DM_WHITE:
		fg = ((struct pex_vars *)dmp->dmr_vars)->gray;
		break;
	}
	gcv.foreground = fg;
	XChangeGC( ((struct pex_vars *)dmp->dmr_vars)->dpy,
		   ((struct pex_vars *)dmp->dmr_vars)->gc, GCForeground, &gcv );
#else
	PEXColor color;

	PEXSetTextColor(((struct pex_vars *)dmp->dmr_vars)->dpy,
			((struct pex_vars *)dmp->dmr_vars)->renderer,
			PEXOCRender, PEXColorTypeRGB, &color);
#endif
	label( dmp, (double)x, (double)y, str );
}

/*
 *			P E X _ 2 D _ G O T O
 *
 */
static void
Pex_2d_line( dmp, x1, y1, x2, y2, dashed, r, g, b )
struct dm *dmp;
int x1, y1;
int x2, y2;
int dashed;
register short r, g, b;
{
#if 0
  PEXCoord2D coord_buf_2D[2];

  PEXBeginRendering(((struct pex_vars *)dmp->dmr_vars)->dpy,
#ifdef DOUBLE_BUFFERING_WITH_PIXMAPS
		    ((struct pex_vars *)dmp->dmr_vars)->pix,
#else
		    ((struct pex_vars *)dmp->dmr_vars)->win,
#endif
		    ((struct pex_vars *)dmp->dmr_vars)->renderer);
  {

    coord_buf_2D[0].x = x1;
    coord_buf_2D[0].y = y1;
    coord_buf_2D[1].x = x2;
    coord_buf_2D[1].y = y2;
    
    PEXSetLineWidth(((struct pex_vars *)dmp->dmr_vars)->dpy,
		    ((struct pex_vars *)dmp->dmr_vars)->renderer,
		    PEXOCRender, 1.0);
		                        
    if( dashed ){
      PEXSetLineType(((struct pex_vars *)dmp->dmr_vars)->dpy,
		     ((struct pex_vars *)dmp->dmr_vars)->renderer,
		     PEXOCRender, PEXLineTypeDashed);
    }else{
      PEXSetLineType(((struct pex_vars *)dmp->dmr_vars)->dpy,
		                          ((struct pex_vars *)dmp->dmr_vars)->renderer,
		                          PEXOCRender, PEXLineTypeSolid);
    }

    
    PEXPolyline2D(((struct pex_vars *)dmp->dmr_vars)->dpy,
		((struct pex_vars *)dmp->dmr_vars)->renderer,
		PEXOCRender, 2, coord_buf_2D);

  }
  PEXEndRendering(((struct pex_vars *)dmp->dmr_vars)->dpy,
		  ((struct pex_vars *)dmp->dmr_vars)->renderer, True);
#else
    XGCValues gcv;

    gcv.foreground = ((struct pex_vars *)dmp->dmr_vars)->yellow;
    XChangeGC( ((struct pex_vars *)dmp->dmr_vars)->dpy, ((struct pex_vars *)dmp->dmr_vars)->gc, GCForeground, &gcv );
    if( dashed ) {
	XSetLineAttributes(((struct pex_vars *)dmp->dmr_vars)->dpy, ((struct pex_vars *)dmp->dmr_vars)->gc, 1, LineOnOffDash, CapButt, JoinMiter );
    } else {
	XSetLineAttributes(((struct pex_vars *)dmp->dmr_vars)->dpy, ((struct pex_vars *)dmp->dmr_vars)->gc, 1, LineSolid, CapButt, JoinMiter );
    }
    draw( dmp, x1, y1, x2, y2 );
#endif
}

/*
 *			P E X _ I N P U T
 *
 * Execution must suspend in this routine until a significant event
 * has occured on either the command stream,
 * unless "noblock" is set.
 *
 *
 * Implicit Return -
 *	If any files are ready for input, their bits will be set in 'input'.
 *	Otherwise, 'input' will be all zeros.
 *
 * DEPRECATED 
 *
 */
/* ARGSUSED */
static void
Pex_input( dmp, input, noblock )
struct dm *dmp;
fd_set		*input;
int		noblock;
{
    return;
}

/* 
 *			P E X _ L I G H T
 */
/* ARGSUSED */
static void
Pex_light( dmp, cmd, func )
struct dm *dmp;
int cmd;
int func;			/* BE_ or BV_ function */
{
	return;
}

/* ARGSUSED */
static unsigned
Pex_cvtvecs( dmp, sp )
struct dm *dmp;
struct solid *sp;
{
	return( 0 );
}

/*
 * Loads displaylist
 */
static unsigned
Pex_load( dmp, addr, count )
struct dm *dmp;
unsigned addr, count;
{
	bu_log("Pex_load(x%x, %d.)\n", addr, count );
	return( 0 );
}

static void
Pex_viewchange(dmp)
struct dm *dmp;
{
  return;
}

static void
Pex_colorchange(dmp)
struct dm *dmp;
{
#if TRY_DEPTHCUE
  PEXDepthCueEntry cue_entry;

  if(((struct pex_vars *)dmp->dmr_vars)->mvars.cue)
    cue_entry.mode = PEXOn;
  else
    cue_entry.mode = PEXOff;

  cue_entry.front_plane = 0.9;
  cue_entry.back_plane = 0.1;
  cue_entry.front_scaling = 1;
  cue_entry.back_scaling = 0.5;
  SET_COLOR( 0, 0, 0, cue_entry.color.value );

  PEXSetTableEntries( ((struct pex_vars *)dmp->dmr_vars)->dpy,
		      ((struct pex_vars *)dmp->dmr_vars)->rattrs.depth_cue_table,
		      cue_num, 1, PEXLUTDepthCue, &cue_entry );
#endif
}

/* ARGSUSED */
static void
Pex_debug(dmp, lvl)
struct dm *dmp;
{
	XFlush(((struct pex_vars *)dmp->dmr_vars)->dpy);
	bu_log("flushed\n");
}

static void
Pex_window(dmp, w)
struct dm *dmp;
register int w[];
{
#if 0
	/* Compute the clipping bounds */
	clipmin[0] = w[1] / 2048.;
	clipmin[1] = w[3] / 2048.;
	clipmin[2] = w[5] / 2048.;
	clipmax[0] = w[0] / 2047.;
	clipmax[1] = w[2] / 2047.;
	clipmax[2] = w[4] / 2047.;
#endif
}

/*********XXX**********/
/*
 *  Called for 2d_line, and dot at center of screen.
 */
static void
draw( dmp, x1, y1, x2, y2 )
struct dm *dmp;
int	x1, y1;		/* from point */
int	x2, y2;		/* to point */
{
  int	sx1, sy1, sx2, sy2;

  sx1 = GED_TO_Xx( dmp, x1 );
  sy1 = GED_TO_Xy( dmp, y1 );
  sx2 = GED_TO_Xx( dmp, x2 );
  sy2 = GED_TO_Xy( dmp, y2 );

  if( sx1 == sx2 && sy1 == sy2 )
    XDrawPoint( ((struct pex_vars *)dmp->dmr_vars)->dpy,
#ifdef DOUBLE_BUFFERING_WITH_PIXMAPS
		((struct pex_vars *)dmp->dmr_vars)->pix,
#else
		((struct pex_vars *)dmp->dmr_vars)->win,
#endif
		((struct pex_vars *)dmp->dmr_vars)->gc, sx1, sy1 );
  else
    XDrawLine( ((struct pex_vars *)dmp->dmr_vars)->dpy,
#ifdef DOUBLE_BUFFERING_WITH_PIXMAPS
	       ((struct pex_vars *)dmp->dmr_vars)->pix,
#else
	       ((struct pex_vars *)dmp->dmr_vars)->win,
#endif
	       ((struct pex_vars *)dmp->dmr_vars)->gc, sx1, sy1, sx2, sy2 );
}

static void
label( dmp, x, y, str )
struct dm *dmp;
double	x, y;
char	*str;
{
  int	sx, sy;

  sx = GED_TO_Xx( dmp, x );
  sy = GED_TO_Xy( dmp, y );
  /* point is center of text? - seems like what MGED wants... */
  /* The following makes the menu look good, the rest bad */
  /*sy += ((struct pex_vars *)dmp->dmr_vars)->fontstruct->maPex_bounds.ascent + ((struct pex_vars *)dmp->dmr_vars)->fontstruct->maPex_bounds.descent/2);*/

  XDrawString( ((struct pex_vars *)dmp->dmr_vars)->dpy,
#ifdef DOUBLE_BUFFERING_WITH_PIXMAPS
	       ((struct pex_vars *)dmp->dmr_vars)->pix,
#else
	       ((struct pex_vars *)dmp->dmr_vars)->win,
#endif
	       ((struct pex_vars *)dmp->dmr_vars)->gc, sx, sy, str, strlen(str) );
}

static XWMHints xwmh = {
        StateHint,		        /* flags */
	0,				/* input */
	NormalState,			/* initial_state */
	0,				/* icon pixmap */
	0,				/* icon window */
	0, 0,				/* icon location */
	0,				/* icon mask */
	0				/* Window group */
};

void
Pex_configure_window_shape(dmp)
struct dm *dmp;
{
  XWindowAttributes xwa;
  XFontStruct     *newfontstruct;
  XGCValues       gcv;

  XGetWindowAttributes( ((struct pex_vars *)dmp->dmr_vars)->dpy,
			((struct pex_vars *)dmp->dmr_vars)->win, &xwa );
  ((struct pex_vars *)dmp->dmr_vars)->height = xwa.height;
  ((struct pex_vars *)dmp->dmr_vars)->width = xwa.width;

#ifdef DOUBLE_BUFFERING_WITH_PIXMAPS
    if(((struct pex_vars *)dmp->dmr_vars)->height != ((struct pex_vars *)dmp->dmr_vars)->pix_height ||
       ((struct pex_vars *)dmp->dmr_vars)->width != ((struct pex_vars *)dmp->dmr_vars)->pix_width){
      Tk_FreePixmap( ((struct pex_vars *)dmp->dmr_vars)->dpy, ((struct pex_vars *)dmp->dmr_vars)->pix );

      ((struct pex_vars *)dmp->dmr_vars)->pix_width = ((struct pex_vars *)dmp->dmr_vars)->width;
      ((struct pex_vars *)dmp->dmr_vars)->pix_height = ((struct pex_vars *)dmp->dmr_vars)->height;
      ((struct pex_vars *)dmp->dmr_vars)->pix =
	Tk_GetPixmap(((struct pex_vars *)dmp->dmr_vars)->dpy,
		     DefaultRootWindow(((struct pex_vars *)dmp->dmr_vars)->dpy),
		     ((struct pex_vars *)dmp->dmr_vars)->pix_width,
		     ((struct pex_vars *)dmp->dmr_vars)->pix_height,
		     Tk_Depth(((struct pex_vars *)dmp->dmr_vars)->xtkwin));
    }
#endif

  /* First time through, load a font or quit */
  if (((struct pex_vars *)dmp->dmr_vars)->fontstruct == NULL) {
    if ((((struct pex_vars *)dmp->dmr_vars)->fontstruct = XLoadQueryFont(((struct pex_vars *)dmp->dmr_vars)->dpy, FONT9)) == NULL ) {
      /* Try hardcoded backup font */
      if ((((struct pex_vars *)dmp->dmr_vars)->fontstruct = XLoadQueryFont(((struct pex_vars *)dmp->dmr_vars)->dpy, FONTBACK)) == NULL) {
	Tcl_AppendResult(interp, "dm-X: Can't open font '", FONT9,
			 "' or '", FONTBACK, "'\n", (char *)NULL);
	return;
      }
    }

    gcv.font = ((struct pex_vars *)dmp->dmr_vars)->fontstruct->fid;
    XChangeGC(((struct pex_vars *)dmp->dmr_vars)->dpy, ((struct pex_vars *)dmp->dmr_vars)->gc,
	      GCFont, &gcv);
  }

  /* Always try to choose a the font that best fits the window size.
   */

  if (((struct pex_vars *)dmp->dmr_vars)->width < 582) {
    if (((struct pex_vars *)dmp->dmr_vars)->fontstruct->per_char->width != 5) {
      if ((newfontstruct = XLoadQueryFont(((struct pex_vars *)dmp->dmr_vars)->dpy, FONT5)) != NULL ) {
	XFreeFont(((struct pex_vars *)dmp->dmr_vars)->dpy, ((struct pex_vars *)dmp->dmr_vars)->fontstruct);
	((struct pex_vars *)dmp->dmr_vars)->fontstruct = newfontstruct;
	gcv.font = ((struct pex_vars *)dmp->dmr_vars)->fontstruct->fid;
	XChangeGC(((struct pex_vars *)dmp->dmr_vars)->dpy, ((struct pex_vars *)dmp->dmr_vars)->gc,
		  GCFont, &gcv);
      }
    }
  } else if (((struct pex_vars *)dmp->dmr_vars)->width < 679) {
    if (((struct pex_vars *)dmp->dmr_vars)->fontstruct->per_char->width != 6){
      if ((newfontstruct = XLoadQueryFont(((struct pex_vars *)dmp->dmr_vars)->dpy, FONT6)) != NULL ) {
	XFreeFont(((struct pex_vars *)dmp->dmr_vars)->dpy, ((struct pex_vars *)dmp->dmr_vars)->fontstruct);
	((struct pex_vars *)dmp->dmr_vars)->fontstruct = newfontstruct;
	gcv.font = ((struct pex_vars *)dmp->dmr_vars)->fontstruct->fid;
	XChangeGC(((struct pex_vars *)dmp->dmr_vars)->dpy, ((struct pex_vars *)dmp->dmr_vars)->gc,
		  GCFont, &gcv);
      }
    }
  } else if (((struct pex_vars *)dmp->dmr_vars)->width < 776) {
    if (((struct pex_vars *)dmp->dmr_vars)->fontstruct->per_char->width != 7){
      if ((newfontstruct = XLoadQueryFont(((struct pex_vars *)dmp->dmr_vars)->dpy, FONT7)) != NULL ) {
	XFreeFont(((struct pex_vars *)dmp->dmr_vars)->dpy, ((struct pex_vars *)dmp->dmr_vars)->fontstruct);
	((struct pex_vars *)dmp->dmr_vars)->fontstruct = newfontstruct;
	gcv.font = ((struct pex_vars *)dmp->dmr_vars)->fontstruct->fid;
	XChangeGC(((struct pex_vars *)dmp->dmr_vars)->dpy, ((struct pex_vars *)dmp->dmr_vars)->gc,
		  GCFont, &gcv);
      }
    }
  } else if (((struct pex_vars *)dmp->dmr_vars)->width < 873) {
    if (((struct pex_vars *)dmp->dmr_vars)->fontstruct->per_char->width != 8){
      if ((newfontstruct = XLoadQueryFont(((struct pex_vars *)dmp->dmr_vars)->dpy, FONT8)) != NULL ) {
	XFreeFont(((struct pex_vars *)dmp->dmr_vars)->dpy, ((struct pex_vars *)dmp->dmr_vars)->fontstruct);
	((struct pex_vars *)dmp->dmr_vars)->fontstruct = newfontstruct;
	gcv.font = ((struct pex_vars *)dmp->dmr_vars)->fontstruct->fid;
	XChangeGC(((struct pex_vars *)dmp->dmr_vars)->dpy, ((struct pex_vars *)dmp->dmr_vars)->gc,
		  GCFont, &gcv);
      }
    }
  } else {
    if (((struct pex_vars *)dmp->dmr_vars)->fontstruct->per_char->width != 9){
      if ((newfontstruct = XLoadQueryFont(((struct pex_vars *)dmp->dmr_vars)->dpy, FONT9)) != NULL ) {
	XFreeFont(((struct pex_vars *)dmp->dmr_vars)->dpy, ((struct pex_vars *)dmp->dmr_vars)->fontstruct);
	((struct pex_vars *)dmp->dmr_vars)->fontstruct = newfontstruct;
	gcv.font = ((struct pex_vars *)dmp->dmr_vars)->fontstruct->fid;
	XChangeGC(((struct pex_vars *)dmp->dmr_vars)->dpy, ((struct pex_vars *)dmp->dmr_vars)->gc,
		  GCFont, &gcv);
      }
    }
  }
}

void
Pex_establish_perspective(dmp)
struct dm *dmp;
{
  struct bu_vls vls;

  bu_vls_init(&vls);
  bu_vls_printf( &vls, "set perspective %d\n",
		 ((struct pex_vars *)dmp->dmr_vars)->mvars.perspective_mode ?
		 perspective_table[((struct pex_vars *)dmp->dmr_vars)->perspective_angle] : -1 );

  Tcl_Eval(interp, bu_vls_addr(&vls));
  bu_vls_free(&vls);

#if 0
  dmaflag = 1;
#endif
}

/*
   This routine will toggle the perspective_angle if the
   dummy_perspective value is 0 or less. Otherwise, the
   perspective_angle is set to the value of (dummy_perspective - 1).
*/
void
Pex_set_perspective(dmp)
struct dm *dmp;
{
  /* set perspective matrix */
  if(((struct pex_vars *)dmp->dmr_vars)->mvars.dummy_perspective > 0)
    ((struct pex_vars *)dmp->dmr_vars)->perspective_angle = ((struct pex_vars *)dmp->dmr_vars)->mvars.dummy_perspective <= 4 ?
    ((struct pex_vars *)dmp->dmr_vars)->mvars.dummy_perspective - 1: 3;
  else if (--((struct pex_vars *)dmp->dmr_vars)->perspective_angle < 0) /* toggle perspective matrix */
    ((struct pex_vars *)dmp->dmr_vars)->perspective_angle = 3;

  if(((struct pex_vars *)dmp->dmr_vars)->mvars.perspective_mode){
    struct bu_vls vls;

    bu_vls_init(&vls);
    bu_vls_printf( &vls, "set perspective %d\n",
		   perspective_table[((struct pex_vars *)dmp->dmr_vars)->perspective_angle] );

    Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);
  }

  /*
    Just in case the "!" is used with the set command. This
    allows us to toggle through more than two values.
   */
  ((struct pex_vars *)dmp->dmr_vars)->mvars.dummy_perspective = 1;

#if 0
  dmaflag = 1;
#endif
}

static void
Pex_load_startup(dmp)
struct dm *dmp;
{
  char *filename;

  bzero((void *)&head_pex_vars, sizeof(struct pex_vars));
  BU_LIST_INIT( &head_pex_vars.l );

  if((filename = getenv("DM_PEX_RCFILE")) != (char *)NULL )
    Tcl_EvalFile(interp, filename);
}

static void
Pex_setup_renderer(dmp)
struct dm *dmp;
{
  unsigned long mask = 0;

/*XXX Slowly adding more stuff */

#if TRY_DEPTHCUE
  mask |= PEXRADepthCueTable;
  ((struct pex_vars *)dmp->dmr_vars)->rattrs.view_table =
    PEXCreateLookupTable(((struct pex_vars *)dmp->dmr_vars)->dpy,
			 ((struct pex_vars *)dmp->dmr_vars)->win, PEXLUTDepthCue);
#endif

  mask |= PEXRAViewTable;
  ((struct pex_vars *)dmp->dmr_vars)->rattrs.view_table =
    PEXCreateLookupTable(((struct pex_vars *)dmp->dmr_vars)->dpy,
			 ((struct pex_vars *)dmp->dmr_vars)->win, PEXLUTView);

  ((struct pex_vars *)dmp->dmr_vars)->renderer =
    PEXCreateRenderer(((struct pex_vars *)dmp->dmr_vars)->dpy,
		      ((struct pex_vars *)dmp->dmr_vars)->win,
		      mask, &((struct pex_vars *)dmp->dmr_vars)->rattrs);

#if TRY_DEPTHCUE
  PEXSetDepthCueIndex(((struct pex_vars *)dmp->dmr_vars)->dpy,
		  ((struct pex_vars *)dmp->dmr_vars)->renderer,
		  PEXOCRender, cue_num);
#endif

  PEXSetViewIndex(((struct pex_vars *)dmp->dmr_vars)->dpy,
		  ((struct pex_vars *)dmp->dmr_vars)->renderer,
		  PEXOCRender, view_num);
}

/* P E X _ M A T _ C O P Y
 *
 * Copy the Brlcad matrix to a PEX matrix
 */
static void
Pex_mat_copy( dest, src )
register PEXMatrix	dest;
register CONST mat_t	src;
{
  register int i, j, k;

  k = 0;

  /* Copy all elements */
#include "noalias.h"
#if 1
  /* regular copy */
  for( i=0; i<4; ++i)
    for( j=0; j<4; ++j)
      dest[i][j] = src[k++];
#else
  /* transpose copy */
  for( i=0; i<4; ++i)
    for( j=0; j<4; ++j)
      dest[j][i] = src[k++];
#endif
}
