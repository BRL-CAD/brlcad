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
#define TRY_DEPTHCUE 0
#define SET_COLOR( r, g, b, c ) { \
	(c).rgb.red = (r); \
	(c).rgb.green = (g); \
	(c).rgb.blue = (b);}

#include "conf.h"

#include <sys/time.h>		/* for struct timeval */
#include <X11/X.h>
#ifdef HAVE_XOSDEFS_H
#include <X11/Xfuncproto.h>
#include <X11/Xosdefs.h>
#endif
#if defined(linux)
#	undef   X_NOT_STDC_ENV
#	undef   X_NOT_POSIX
#endif
#define XLIB_ILLEGAL_ACCESS	/* necessary on facist SGI 5.0.1 */
#include "tk.h"
#include <X11/PEX5/PEXlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <X11/extensions/multibuf.h>

#include "machine.h"
#include "externs.h"
#include "vmath.h"
#include "mater.h"
#include "bu.h"
#include "raytrace.h"
#include "./ged.h"
#include "./dm.h"
#include "./solid.h"
#include "./sedit.h"

#define IMMED_MODE_SPT(info) (((info)->subset_info & 0xffff) ==\
			      PEXCompleteImplementation ||\
			      (info)->subset_info & PEXImmediateMode)
#define Pex_VMOVE(a,b) {\
			(a).x = (b)[X];\
			(a).y = (b)[Y];\
			(a).z = (b)[Z]; }

extern int dm_pipe[];

static void     Pex_var_init();
static void     Pex_setup_renderer();
static void     Pex_mat_copy();
static void     Pex_load_startup();
static int	Pex_setup();

static void	label();
static void	draw();
static void     establish_perspective();
static void     set_perspective();
static void     establish_am();

/* Display Manager package interface */

#define PLOTBOUND	1000.0	/* Max magnification in Rot matrix */
int	Pex_open();
void	Pex_close();
MGED_EXTERN(void	Pex_input, (fd_set *input, int noblock) );
void	Pex_prolog(), Pex_epilog();
void	Pex_normal(), Pex_newrot();
void	Pex_update();
void	Pex_puts(), Pex_2d_line(), Pex_light();
int	Pex_object();
unsigned Pex_cvtvecs(), Pex_load();
void	Pex_statechange(), Pex_viewchange(), Pex_colorchange();
void	Pex_window(), Pex_debug(), Pex_selectargs();
int     Pex_dm();

static struct dm_list *get_dm_list();
#ifdef USE_PROTOTYPES
static Tk_GenericProc Pex_doevent;
#else
static int Pex_doevent();
#endif

struct dm dm_pex = {
	Pex_open, Pex_close,
	Pex_input,
	Pex_prolog, Pex_epilog,
	Pex_normal, Pex_newrot,
	Pex_update,
	Pex_puts, Pex_2d_line,
	Pex_light,
	Pex_object,	Pex_cvtvecs, Pex_load,
	Pex_statechange,
	Pex_viewchange,
	Pex_colorchange,
	Pex_window, Pex_debug,
	0,				/* no displaylist */
	0,				/* multi-window */
	PLOTBOUND,
	"pex", "X Window System (X11)",
	0,
	Pex_dm
};

extern struct device_values dm_values;	/* values read from devices */
extern Tcl_Interp *interp;
extern Tk_Window tkwin;

struct modifiable_pex_vars {
#if TRY_DEPTHCUE
  int cue;
#endif
  int perspective_mode;
  int dummy_perspective;
};

struct pex_vars {
  struct rt_list l;
  struct dm_list *dm_list;
  Display *dpy;
  Tk_Window xtkwin;
  Window win;
#ifdef DOUBLE_BUFFERING_WITH_PIXMAPS
  Pixmap pix;
  int pix_width, pix_height;
#endif
  PEXRenderer renderer;
  PEXRendererAttributes rattrs;
  unsigned int mb_mask;
  int width;
  int height;
  int omx, omy;
  int perspective_angle;
  GC gc;
  XFontStruct *fontstruct;
  int is_monochrome;
  unsigned long black,gray,white,yellow,red,blue;
  unsigned long bd, bg, fg;   /* color of border, background, foreground */
  struct modifiable_pex_vars mvars;
};

#define Pex_MV_O(_m) offsetof(struct modifiable_pex_vars, _m)
struct bu_structparse Pex_vparse[] = {
#if TRY_DEPTHCUE
  {"%d",  1, "depthcue",	Pex_MV_O(cue),	Pex_colorchange },
#endif
  {"%d",  1, "perspective",     Pex_MV_O(perspective_mode), establish_perspective },
  {"%d",  1, "set_perspective", Pex_MV_O(dummy_perspective),  set_perspective },
  {"",    0,  (char *)0,          0,                      FUNC_NULL }
};

static int perspective_table[] = { 
	30, 45, 60, 90 };
static int view_num = 0;
#if TRY_DEPTHCUE
static int cue_num = 1;
#endif

static struct pex_vars head_pex_vars;
#if !DO_XSELECTINPUT
static int XdoMotion = 0;
#endif

/*
 * Display coordinate conversion:
 *  GED is using -2048..+2048,
 *  X is 0..width,0..height
 */
#define	GED_TO_Xx(x)	((int)((x/4096.0+0.5)*((struct pex_vars *)dm_vars)->width))
#define	GED_TO_Xy(x)	((int)((0.5-x/4096.0)*((struct pex_vars *)dm_vars)->height))
#define Xx_TO_GED(x)    ((int)((x/(double)((struct pex_vars *)dm_vars)->width - 0.5) * 4095))
#define Xy_TO_GED(x)    ((int)((0.5 - x/(double)((struct pex_vars *)dm_vars)->height) * 4095))

/*
 *			P E X _ O P E N
 *
 * Fire up the display manager, and the display processor.
 *
 */
Pex_open()
{
  Pex_var_init();

  return Pex_setup(dname);
}

/*
 *  			P E X _ C L O S E
 *  
 *  Gracefully release the display.
 */
void
Pex_close()
{
  if(((struct pex_vars *)dm_vars)->gc != 0)
    XFreeGC(((struct pex_vars *)dm_vars)->dpy, ((struct pex_vars *)dm_vars)->gc);

#ifdef DOUBLE_BUFFERING_WITH_PIXMAPS
  if(((struct pex_vars *)dm_vars)->pix != 0)
     Tk_FreePixmap(((struct pex_vars *)dm_vars)->dpy, ((struct pex_vars *)dm_vars)->pix);
#endif

  if(((struct pex_vars *)dm_vars)->xtkwin != 0)
    Tk_DestroyWindow(((struct pex_vars *)dm_vars)->xtkwin);

  if(((struct pex_vars *)dm_vars)->l.forw != RT_LIST_NULL)
    RT_LIST_DEQUEUE(&((struct pex_vars *)dm_vars)->l);

  bu_free(dm_vars, "Pex_close: dm_vars");

  if(RT_LIST_IS_EMPTY(&head_pex_vars.l))
    Tk_DeleteGenericHandler(Pex_doevent, (ClientData)NULL);
}

/*
 *			P E X _ P R O L O G
 *
 * There are global variables which are parameters to this routine.
 */
void
Pex_prolog()
{
#ifdef DOUBLE_BUFFERING_WITH_PIXMAPS
  XGCValues       gcv;

  gcv.foreground = ((struct pex_vars *)dm_vars)->bg;
  XChangeGC(((struct pex_vars *)dm_vars)->dpy, ((struct pex_vars *)dm_vars)->gc,
	    GCForeground, &gcv);
  XFillRectangle(((struct pex_vars *)dm_vars)->dpy, ((struct pex_vars *)dm_vars)->pix,
		 ((struct pex_vars *)dm_vars)->gc, 0, 0,
		 ((struct pex_vars *)dm_vars)->width + 1,
		 ((struct pex_vars *)dm_vars)->height + 1);
#else
  XClearWindow(((struct pex_vars *)dm_vars)->dpy, ((struct pex_vars *)dm_vars)->win);
#endif
}

/*
 *			P E X _ E P I L O G
 */
void
Pex_epilog()
{
  /* Put the center point up last */
  draw( 0, 0, 0, 0 );

#ifdef DOUBLE_BUFFERING_WITH_PIXMAPS
  XCopyArea(((struct pex_vars *)dm_vars)->dpy, ((struct pex_vars *)dm_vars)->pix,
	    ((struct pex_vars *)dm_vars)->win, ((struct pex_vars *)dm_vars)->gc, 0, 0,
	    ((struct pex_vars *)dm_vars)->width, ((struct pex_vars *)dm_vars)->height,
	    0, 0);
#endif

  /* Prevent lag between events and updates */
  XSync(((struct pex_vars *)dm_vars)->dpy, 0);
}

/*
 *  			P E X _ N E W R O T
 */
/* ARGSUSED */
void
Pex_newrot(mat)
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

  PEXSetTableEntries(((struct pex_vars *)dm_vars)->dpy,
		     ((struct pex_vars *)dm_vars)->rattrs.view_table,
		     view_num, 1, PEXLUTView, (PEXPointer)&view);
#if 0
  PEXSetViewIndex(((struct pex_vars *)dm_vars)->dpy,
		  ((struct pex_vars *)dm_vars)->renderer,
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
int
Pex_object( sp, mat, ratio, white_flag )
register struct solid *sp;
mat_t mat;
double ratio;
int white_flag;
{
  register struct rt_vlist    *vp;
  PEXCoord coord_buf[1024];
  PEXCoord *cp;                /* current coordinate */
  int first;
  int ncoord;                  /* number of coordinates */
  PEXColor color;

  PEXBeginRendering(((struct pex_vars *)dm_vars)->dpy,
#ifdef DOUBLE_BUFFERING_WITH_PIXMAPS
		    ((struct pex_vars *)dm_vars)->pix,
#else
		    ((struct pex_vars *)dm_vars)->win,
#endif
		    ((struct pex_vars *)dm_vars)->renderer);
  {
    if( white_flag ){
      SET_COLOR( 0.9, 0.9, 0.9, color );
    }else{
      SET_COLOR( (short)sp->s_color[0] / 255.0, (short)sp->s_color[1] / 255.0,
		 (short)sp->s_color[2] / 255.0, color );
    }

    PEXSetLineColor(((struct pex_vars *)dm_vars)->dpy,
		    ((struct pex_vars *)dm_vars)->renderer,
		    PEXOCRender, PEXColorTypeRGB, &color);

    PEXSetLineWidth(((struct pex_vars *)dm_vars)->dpy,
		    ((struct pex_vars *)dm_vars)->renderer,
		    PEXOCRender, 1.0);

    if( sp->s_soldash )
      PEXSetLineType(((struct pex_vars *)dm_vars)->dpy,
		     ((struct pex_vars *)dm_vars)->renderer,
		     PEXOCRender, PEXLineTypeDashed);
    else
      PEXSetLineType(((struct pex_vars *)dm_vars)->dpy,
		     ((struct pex_vars *)dm_vars)->renderer,
		     PEXOCRender, PEXLineTypeSolid);

    ncoord = 0;
    cp = coord_buf;
    first = 1;
    for( RT_LIST_FOR( vp, rt_vlist, &(sp->s_vlist) ) )  {
      register int	i;
      register int	nused = vp->nused;
      register int	*cmd = vp->cmd;
      register point_t *pt = vp->pt;

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
	    PEXPolyline(((struct pex_vars *)dm_vars)->dpy,
			((struct pex_vars *)dm_vars)->renderer,
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

	  PEXPolyline(((struct pex_vars *)dm_vars)->dpy,
		      ((struct pex_vars *)dm_vars)->renderer,
		      PEXOCRender, ncoord, coord_buf);

	  ncoord = 0;
	  cp = coord_buf;
	  first = 1;
	  break;
	}
      }
    }

    if( first == 0)
      PEXPolyline(((struct pex_vars *)dm_vars)->dpy,
		  ((struct pex_vars *)dm_vars)->renderer,
		  PEXOCRender, ncoord, coord_buf);

    if (sp->s_soldash) /* restore solid lines */
      PEXSetLineType(((struct pex_vars *)dm_vars)->dpy,
		     ((struct pex_vars *)dm_vars)->renderer,
		     PEXOCRender, PEXLineTypeSolid);
  }
  PEXEndRendering(((struct pex_vars *)dm_vars)->dpy,
		  ((struct pex_vars *)dm_vars)->renderer, True);
}

/*
 *			P E X _ N O R M A L
 *
 * Restore the display processor to a normal mode of operation
 * (ie, not scaled, rotated, displaced, etc).
 * Turns off windowing.
 */
void
Pex_normal()
{
	return;
}

/*
 *			P E X _ U P D A T E
 *
 * Transmit accumulated displaylist to the display processor.
 */
void
Pex_update()
{
    XFlush(((struct pex_vars *)dm_vars)->dpy);
}

/*
 *			P E X _ P U T S
 *
 * Output a string into the displaylist.
 * The starting position of the beam is as specified.
 */
/* ARGSUSED */
void
Pex_puts( str, x, y, size, color )
register char *str;
{
	XGCValues gcv;
	unsigned long fg;

	switch( color )  {
	case DM_BLACK:
		fg = ((struct pex_vars *)dm_vars)->black;
		break;
	case DM_RED:
		fg = ((struct pex_vars *)dm_vars)->red;
		break;
	case DM_BLUE:
		fg = ((struct pex_vars *)dm_vars)->blue;
		break;
	default:
	case DM_YELLOW:
		fg = ((struct pex_vars *)dm_vars)->yellow;
		break;
	case DM_WHITE:
		fg = ((struct pex_vars *)dm_vars)->gray;
		break;
	}
	gcv.foreground = fg;
	XChangeGC( ((struct pex_vars *)dm_vars)->dpy, ((struct pex_vars *)dm_vars)->gc, GCForeground, &gcv );
	label( (double)x, (double)y, str );
}

/*
 *			P E X _ 2 D _ G O T O
 *
 */
void
Pex_2d_line( x1, y1, x2, y2, dashed )
int x1, y1;
int x2, y2;
int dashed;
{
#if 0
  PEXCoord2D coord_buf_2D[2];

  PEXBeginRendering(((struct pex_vars *)dm_vars)->dpy,
#ifdef DOUBLE_BUFFERING_WITH_PIXMAPS
		    ((struct pex_vars *)dm_vars)->pix,
#else
		    ((struct pex_vars *)dm_vars)->win,
#endif
		    ((struct pex_vars *)dm_vars)->renderer);
  {

    coord_buf_2D[0].x = x1;
    coord_buf_2D[0].y = y1;
    coord_buf_2D[1].x = x2;
    coord_buf_2D[1].y = y2;
    
    PEXSetLineWidth(((struct pex_vars *)dm_vars)->dpy,
		    ((struct pex_vars *)dm_vars)->renderer,
		    PEXOCRender, 1.0);
		                        
    if( dashed ){
      PEXSetLineType(((struct pex_vars *)dm_vars)->dpy,
		     ((struct pex_vars *)dm_vars)->renderer,
		     PEXOCRender, PEXLineTypeDashed);
    }else{
      PEXSetLineType(((struct pex_vars *)dm_vars)->dpy,
		                          ((struct pex_vars *)dm_vars)->renderer,
		                          PEXOCRender, PEXLineTypeSolid);
    }

    
    PEXPolyline2D(((struct pex_vars *)dm_vars)->dpy,
		((struct pex_vars *)dm_vars)->renderer,
		PEXOCRender, 2, coord_buf_2D);

  }
  PEXEndRendering(((struct pex_vars *)dm_vars)->dpy,
		  ((struct pex_vars *)dm_vars)->renderer, True);
#else
    XGCValues gcv;

    gcv.foreground = ((struct pex_vars *)dm_vars)->yellow;
    XChangeGC( ((struct pex_vars *)dm_vars)->dpy, ((struct pex_vars *)dm_vars)->gc, GCForeground, &gcv );
    if( dashed ) {
	XSetLineAttributes(((struct pex_vars *)dm_vars)->dpy, ((struct pex_vars *)dm_vars)->gc, 1, LineOnOffDash, CapButt, JoinMiter );
    } else {
	XSetLineAttributes(((struct pex_vars *)dm_vars)->dpy, ((struct pex_vars *)dm_vars)->gc, 1, LineSolid, CapButt, JoinMiter );
    }
    draw( x1, y1, x2, y2 );
#endif
}

static int
Pex_doevent(clientData, eventPtr)
ClientData clientData;
XEvent *eventPtr;
{
  KeySym key;
  char keybuf[4];
  int cnt;
  XComposeStatus compose_stat;
  XWindowAttributes xwa;
  struct bu_vls cmd;
  register struct dm_list *save_dm_list;
  int status = CMD_OK;

  save_dm_list = curr_dm_list;

  curr_dm_list = get_dm_list(eventPtr->xany.window);

  if(curr_dm_list == DM_LIST_NULL)
    goto end;

#if 0
  /* Not interested */
  if (eventPtr->xany.window != ((struct pex_vars *)dm_vars)->win){
    curr_dm_list = save_dm_list;
    return TCL_OK;
  }
#endif

  if(mged_variables.send_key && eventPtr->type == KeyPress){
    char buffer[1];
    KeySym keysym;

    XLookupString(&(eventPtr->xkey), buffer, 1,
		  &keysym, (XComposeStatus *)NULL);

    if(keysym == mged_variables.hot_key)
      goto end;

    write(dm_pipe[1], buffer, 1);

    curr_dm_list = save_dm_list;

    /* Use this so that these events won't propagate */
    return TCL_RETURN;
  }

  if (eventPtr->type == Expose && eventPtr->xexpose.count == 0){
    dirty = 1;
    refresh();
    goto end;
  }else if(eventPtr->type == ConfigureNotify){
    ((struct pex_vars *)dm_vars)->height = eventPtr->xconfigure.height;
    ((struct pex_vars *)dm_vars)->width = eventPtr->xconfigure.width;

/*XXX This seems to be causing an broken X connection */
#ifdef DOUBLE_BUFFERING_WITH_PIXMAPS
    if(((struct pex_vars *)dm_vars)->height != ((struct pex_vars *)dm_vars)->pix_height ||
       ((struct pex_vars *)dm_vars)->width != ((struct pex_vars *)dm_vars)->pix_width){
      Tk_FreePixmap( ((struct pex_vars *)dm_vars)->dpy, ((struct pex_vars *)dm_vars)->pix );

      ((struct pex_vars *)dm_vars)->pix_width = ((struct pex_vars *)dm_vars)->width;
      ((struct pex_vars *)dm_vars)->pix_height = ((struct pex_vars *)dm_vars)->height;
      ((struct pex_vars *)dm_vars)->pix =
	Tk_GetPixmap(((struct pex_vars *)dm_vars)->dpy,
		     DefaultRootWindow(((struct pex_vars *)dm_vars)->dpy),
		     ((struct pex_vars *)dm_vars)->pix_width,
		     ((struct pex_vars *)dm_vars)->pix_height,
		     Tk_Depth(((struct pex_vars *)dm_vars)->xtkwin));
    }
#endif

    dirty = 1;
    refresh();
    goto end;
  } else if( eventPtr->type == MotionNotify ) {
    int mx, my;

    bu_vls_init(&cmd);
    mx = eventPtr->xmotion.x;
    my = eventPtr->xmotion.y;

    switch(am_mode){
    case ALT_MOUSE_MODE_IDLE:
      if(scroll_active && eventPtr->xmotion.state & ((struct pex_vars *)dm_vars)->mb_mask)
	bu_vls_printf( &cmd, "M 1 %d %d\n", Xx_TO_GED(mx), Xy_TO_GED(my));
      else if(XdoMotion)
	/* trackball not active so do the regular thing */
	/* Constant tracking (e.g. illuminate mode) bound to M mouse */
	bu_vls_printf( &cmd, "M 0 %d %d\n", Xx_TO_GED(mx), Xy_TO_GED(my));
      else
	goto end;

      break;
    case ALT_MOUSE_MODE_ROTATE:
      bu_vls_printf( &cmd, "iknob ax %f ay %f\n",
		     (my - ((struct pex_vars *)dm_vars)->omy)/512.0,
		     (mx - ((struct pex_vars *)dm_vars)->omx)/512.0);
      break;
    case ALT_MOUSE_MODE_TRANSLATE:
      {
	fastf_t fx, fy;

	if((state == ST_S_EDIT || state == ST_O_EDIT) && !EDIT_ROTATE &&
	  (edobj || es_edflag > 0)){
	  fx = (mx/(fastf_t)((struct pex_vars *)dm_vars)->width - 0.5) * 2;
	  fy = (0.5 - my/(fastf_t)((struct pex_vars *)dm_vars)->height) * 2;
	  bu_vls_printf( &cmd, "knob aX %f aY %f\n", fx, fy );
	}else{
	  fx = (mx - ((struct pex_vars *)dm_vars)->omx) /
	    (fastf_t)((struct pex_vars *)dm_vars)->width * 2.0;
	  fy = (((struct pex_vars *)dm_vars)->omy - my) /
	    (fastf_t)((struct pex_vars *)dm_vars)->height * 2.0;
	  bu_vls_printf( &cmd, "iknob aX %f aY %f\n", fx, fy );
	}
      }	     
      break;
    case ALT_MOUSE_MODE_ZOOM:
      bu_vls_printf( &cmd, "iknob aS %f\n",
		     (((struct pex_vars *)dm_vars)->omy - my)/
		     (fastf_t)((struct pex_vars *)dm_vars)->height);
      break;
    }

    ((struct pex_vars *)dm_vars)->omx = mx;
    ((struct pex_vars *)dm_vars)->omy = my;
  } else {
    XGetWindowAttributes( ((struct pex_vars *)dm_vars)->dpy, ((struct pex_vars *)dm_vars)->win, &xwa);
    ((struct pex_vars *)dm_vars)->height = xwa.height;
    ((struct pex_vars *)dm_vars)->width = xwa.width;

    goto end;
  }

  status = cmdline(&cmd, FALSE);
  bu_vls_free(&cmd);
end:
  curr_dm_list = save_dm_list;

  if(status == CMD_OK)
    return TCL_OK;

  return TCL_ERROR;
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
void
Pex_input( input, noblock )
fd_set		*input;
int		noblock;
{
    return;
}

/* 
 *			P E X _ L I G H T
 */
/* ARGSUSED */
void
Pex_light( cmd, func )
int cmd;
int func;			/* BE_ or BV_ function */
{
	return;
}

/* ARGSUSED */
unsigned
Pex_cvtvecs( sp )
struct solid *sp;
{
	return( 0 );
}

/*
 * Loads displaylist
 */
unsigned
Pex_load( addr, count )
unsigned addr, count;
{
	bu_log("Pex_load(x%x, %d.)\n", addr, count );
	return( 0 );
}

void
Pex_statechange( a, b )
int	a, b;
{
	/*
	 *  Based upon new state, possibly do extra stuff,
	 *  including enabling continuous tablet tracking,
	 *  object highlighting
	 */
#if DO_XSELECTINPUT
 	switch( b )  {
	case ST_VIEW:
	  /* constant tracking OFF */
	  XSelectInput(((struct pex_vars *)dm_vars)->dpy, ((struct pex_vars *)dm_vars)->win, ExposureMask|ButtonPressMask|
		       KeyPressMask|StructureNotifyMask);
	  break;
	case ST_S_PICK:
	case ST_O_PICK:
	case ST_O_PATH:
	  /* constant tracking ON */
	  XSelectInput(((struct pex_vars *)dm_vars)->dpy, ((struct pex_vars *)dm_vars)->win, ExposureMask|ButtonPressMask|
		       KeyPressMask|StructureNotifyMask|PointerMotionMask);
	  break;
	case ST_O_EDIT:
	case ST_S_EDIT:
	  /* constant tracking OFF */
	  XSelectInput(((struct pex_vars *)dm_vars)->dpy, ((struct pex_vars *)dm_vars)->win, ExposureMask|ButtonPressMask|
		       KeyPressMask|StructureNotifyMask);
	  break;
#else
 	switch( b )  {
	case ST_VIEW:
	    /* constant tracking OFF */
	    XdoMotion = 0;
	    break;
	case ST_S_PICK:
	case ST_O_PICK:
	case ST_O_PATH:
	case ST_S_VPICK:
	    /* constant tracking ON */
	    XdoMotion = 1;
	    break;
	case ST_O_EDIT:
	case ST_S_EDIT:
	    /* constant tracking OFF */
	    XdoMotion = 0;
	    break;
#endif
	default:
	    bu_log("Pex_statechange: unknown state %s\n", state_str[b]);
	    break;
	}

	/*Pex_viewchange( DM_CHGV_REDO, SOLID_NULL );*/
}

void
Pex_viewchange()
{
}

void
Pex_colorchange()
{
#if TRY_DEPTHCUE
  PEXDepthCueEntry cue_entry;

  if(((struct pex_vars *)dm_vars)->mvars.cue)
    cue_entry.mode = PEXOn;
  else
    cue_entry.mode = PEXOff;

  cue_entry.front_plane = 0.9;
  cue_entry.back_plane = 0.1;
  cue_entry.front_scaling = 1;
  cue_entry.back_scaling = 0.5;
  SET_COLOR( 0, 0, 0, cue_entry.color.value );

  PEXSetTableEntries( ((struct pex_vars *)dm_vars)->dpy,
		      ((struct pex_vars *)dm_vars)->rattrs.depth_cue_table,
		      cue_num, 1, PEXLUTDepthCue, &cue_entry );
#endif
  color_soltab();		/* apply colors to the solid table */
}

/* ARGSUSED */
void
Pex_debug(lvl)
{
	XFlush(((struct pex_vars *)dm_vars)->dpy);
	bu_log("flushed\n");
}

void
Pex_window(w)
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
draw( x1, y1, x2, y2 )
int	x1, y1;		/* from point */
int	x2, y2;		/* to point */
{
  int	sx1, sy1, sx2, sy2;

  sx1 = GED_TO_Xx( x1 );
  sy1 = GED_TO_Xy( y1 );
  sx2 = GED_TO_Xx( x2 );
  sy2 = GED_TO_Xy( y2 );

  if( sx1 == sx2 && sy1 == sy2 )
    XDrawPoint( ((struct pex_vars *)dm_vars)->dpy,
#ifdef DOUBLE_BUFFERING_WITH_PIXMAPS
		((struct pex_vars *)dm_vars)->pix,
#else
		((struct pex_vars *)dm_vars)->win,
#endif
		((struct pex_vars *)dm_vars)->gc, sx1, sy1 );
  else
    XDrawLine( ((struct pex_vars *)dm_vars)->dpy,
#ifdef DOUBLE_BUFFERING_WITH_PIXMAPS
	       ((struct pex_vars *)dm_vars)->pix,
#else
	       ((struct pex_vars *)dm_vars)->win,
#endif
	       ((struct pex_vars *)dm_vars)->gc, sx1, sy1, sx2, sy2 );
}

static void
label( x, y, str )
double	x, y;
char	*str;
{
  int	sx, sy;

  sx = GED_TO_Xx( x );
  sy = GED_TO_Xy( y );
  /* point is center of text? - seems like what MGED wants... */
  /* The following makes the menu look good, the rest bad */
  /*sy += ((struct pex_vars *)dm_vars)->fontstruct->maPex_bounds.ascent + ((struct pex_vars *)dm_vars)->fontstruct->maPex_bounds.descent/2);*/

  XDrawString( ((struct pex_vars *)dm_vars)->dpy,
#ifdef DOUBLE_BUFFERING_WITH_PIXMAPS
	       ((struct pex_vars *)dm_vars)->pix,
#else
	       ((struct pex_vars *)dm_vars)->win,
#endif
	       ((struct pex_vars *)dm_vars)->gc, sx, sy, str, strlen(str) );
}

#define	FONT	"6x10"
#define FONT2	"-adobe-courier-medium-r-normal--10-100-75-75-m-60-iso8859-1"

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

static int
Pex_setup( name )
char	*name;
{
  static int count = 0;
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

  /* Only need to do this once */
  if(tkwin == NULL){
#if 1
    gui_setup();
#else
    bu_vls_printf(&str, "loadtk %s\n", name);

    if(cmdline(&str, FALSE) == CMD_BAD){
      bu_vls_free(&str);
      return -1;
    }
#endif
  }

  /* Only need to do this once for this display manager */
  if(!count)
    Pex_load_startup();

  if(RT_LIST_IS_EMPTY(&head_pex_vars.l))
    Tk_CreateGenericHandler(Pex_doevent, (ClientData)NULL);

  RT_LIST_APPEND(&head_pex_vars.l, &((struct pex_vars *)curr_dm_list->_dm_vars)->l);

  bu_vls_printf(&pathName, ".dm_pex%d", count);

  /* Make xtkwin a toplevel window */
  ((struct pex_vars *)dm_vars)->xtkwin = Tk_CreateWindowFromPath(interp, tkwin,
						       bu_vls_addr(&pathName), name);

  /*
   * Create the X drawing window by calling create_x which
   * is defined in xinit.tk
   */
  bu_vls_strcpy(&str, "init_x ");
  bu_vls_printf(&str, "%s\n", bu_vls_addr(&pathName));

  if(cmdline(&str, FALSE) == CMD_BAD){
    bu_vls_free(&str);
    return -1;
  }

  bu_vls_free(&str);
  ((struct pex_vars *)dm_vars)->dpy = Tk_Display(((struct pex_vars *)dm_vars)->xtkwin);
  ((struct pex_vars *)dm_vars)->width =
    DisplayWidth(((struct pex_vars *)dm_vars)->dpy,
		 DefaultScreen(((struct pex_vars *)dm_vars)->dpy)) - 20;
  ((struct pex_vars *)dm_vars)->height =
    DisplayHeight(((struct pex_vars *)dm_vars)->dpy,
		  DefaultScreen(((struct pex_vars *)dm_vars)->dpy)) - 20;

  /* Make window square */
  if(((struct pex_vars *)dm_vars)->height < ((struct pex_vars *)dm_vars)->width)
    ((struct pex_vars *)dm_vars)->width = ((struct pex_vars *)dm_vars)->height;
  else
    ((struct pex_vars *)dm_vars)->height = ((struct pex_vars *)dm_vars)->width;

  Tk_GeometryRequest(((struct pex_vars *)dm_vars)->xtkwin,
		     ((struct pex_vars *)dm_vars)->width, 
		     ((struct pex_vars *)dm_vars)->height);
#if 0
  /*XXX*/
  XSynchronize(((struct pex_vars *)dm_vars)->dpy, TRUE);
#endif

  Tk_MakeWindowExist(((struct pex_vars *)dm_vars)->xtkwin);
  ((struct pex_vars *)dm_vars)->win =
      Tk_WindowId(((struct pex_vars *)dm_vars)->xtkwin);

#ifdef DOUBLE_BUFFERING_WITH_PIXMAPS
  ((struct pex_vars *)dm_vars)->pix_width = ((struct pex_vars *)dm_vars)->width;
  ((struct pex_vars *)dm_vars)->pix_height = ((struct pex_vars *)dm_vars)->height;
  ((struct pex_vars *)dm_vars)->pix =
    Tk_GetPixmap(((struct pex_vars *)dm_vars)->dpy,
		 DefaultRootWindow(((struct pex_vars *)dm_vars)->dpy),
		 ((struct pex_vars *)dm_vars)->width,
		 ((struct pex_vars *)dm_vars)->height,
		 Tk_Depth(((struct pex_vars *)dm_vars)->xtkwin));
#endif

  a_screen = Tk_ScreenNumber(((struct pex_vars *)dm_vars)->xtkwin);
  a_visual = Tk_Visual(((struct pex_vars *)dm_vars)->xtkwin);

  /* Get color map indices for the colors we use. */
  ((struct pex_vars *)dm_vars)->black = BlackPixel( ((struct pex_vars *)dm_vars)->dpy, a_screen );
  ((struct pex_vars *)dm_vars)->white = WhitePixel( ((struct pex_vars *)dm_vars)->dpy, a_screen );

  a_cmap = Tk_Colormap(((struct pex_vars *)dm_vars)->xtkwin);
  a_color.red = 255<<8;
  a_color.green=0;
  a_color.blue=0;
  a_color.flags = DoRed | DoGreen| DoBlue;
  if ( ! XAllocColor(((struct pex_vars *)dm_vars)->dpy, a_cmap, &a_color)) {
    bu_log( "dm-X: Can't Allocate red\n");
    return -1;
  }
  ((struct pex_vars *)dm_vars)->red = a_color.pixel;
    if ( ((struct pex_vars *)dm_vars)->red == ((struct pex_vars *)dm_vars)->white )
      ((struct pex_vars *)dm_vars)->red = ((struct pex_vars *)dm_vars)->black;

    a_color.red = 200<<8;
    a_color.green=200<<8;
    a_color.blue=0<<8;
    a_color.flags = DoRed | DoGreen| DoBlue;
    if ( ! XAllocColor(((struct pex_vars *)dm_vars)->dpy, a_cmap, &a_color)) {
	bu_log( "dm-X: Can't Allocate yellow\n");
	return -1;
    }
    ((struct pex_vars *)dm_vars)->yellow = a_color.pixel;
    if ( ((struct pex_vars *)dm_vars)->yellow == ((struct pex_vars *)dm_vars)->white )
      ((struct pex_vars *)dm_vars)->yellow = ((struct pex_vars *)dm_vars)->black;
    
    a_color.red = 0;
    a_color.green=0;
    a_color.blue=255<<8;
    a_color.flags = DoRed | DoGreen| DoBlue;
    if ( ! XAllocColor(((struct pex_vars *)dm_vars)->dpy, a_cmap, &a_color)) {
	bu_log( "dm-X: Can't Allocate blue\n");
	return -1;
    }
    ((struct pex_vars *)dm_vars)->blue = a_color.pixel;
    if ( ((struct pex_vars *)dm_vars)->blue == ((struct pex_vars *)dm_vars)->white )
      ((struct pex_vars *)dm_vars)->blue = ((struct pex_vars *)dm_vars)->black;

    a_color.red = 128<<8;
    a_color.green=128<<8;
    a_color.blue= 128<<8;
    a_color.flags = DoRed | DoGreen| DoBlue;
    if ( ! XAllocColor(((struct pex_vars *)dm_vars)->dpy, a_cmap, &a_color)) {
	bu_log( "dm-X: Can't Allocate gray\n");
	return -1;
    }
    ((struct pex_vars *)dm_vars)->gray = a_color.pixel;
    if ( ((struct pex_vars *)dm_vars)->gray == ((struct pex_vars *)dm_vars)->white )
      ((struct pex_vars *)dm_vars)->gray = ((struct pex_vars *)dm_vars)->black;

    /* Select border, background, foreground colors,
     * and border width.
     */
    if( a_visual->class == GrayScale || a_visual->class == StaticGray ) {
	((struct pex_vars *)dm_vars)->is_monochrome = 1;
	((struct pex_vars *)dm_vars)->bd = BlackPixel( ((struct pex_vars *)dm_vars)->dpy, a_screen );
	((struct pex_vars *)dm_vars)->bg = WhitePixel( ((struct pex_vars *)dm_vars)->dpy, a_screen );
	((struct pex_vars *)dm_vars)->fg = BlackPixel( ((struct pex_vars *)dm_vars)->dpy, a_screen );
    } else {
	/* Hey, it's a color server.  Ought to use 'em! */
	((struct pex_vars *)dm_vars)->is_monochrome = 0;
	((struct pex_vars *)dm_vars)->bd = WhitePixel( ((struct pex_vars *)dm_vars)->dpy, a_screen );
	((struct pex_vars *)dm_vars)->bg = BlackPixel( ((struct pex_vars *)dm_vars)->dpy, a_screen );
	((struct pex_vars *)dm_vars)->fg = WhitePixel( ((struct pex_vars *)dm_vars)->dpy, a_screen );
    }

    if( !((struct pex_vars *)dm_vars)->is_monochrome &&
	((struct pex_vars *)dm_vars)->fg != ((struct pex_vars *)dm_vars)->red &&
	((struct pex_vars *)dm_vars)->red != ((struct pex_vars *)dm_vars)->black )
      ((struct pex_vars *)dm_vars)->fg = ((struct pex_vars *)dm_vars)->red;

    gcv.foreground = ((struct pex_vars *)dm_vars)->fg;
    gcv.background = ((struct pex_vars *)dm_vars)->bg;

#ifndef CRAY2
    cp = FONT;
    if ( (((struct pex_vars *)dm_vars)->fontstruct =
	 XLoadQueryFont(((struct pex_vars *)dm_vars)->dpy, cp)) == NULL ) {
      /* Try hardcoded backup font */
      if ( (((struct pex_vars *)dm_vars)->fontstruct =
	    XLoadQueryFont(((struct pex_vars *)dm_vars)->dpy, FONT2)) == NULL) {
	bu_log( "dm-X: Can't open font '%s' or '%s'\n", cp, FONT2 );
	return -1;
      }
    }
    gcv.font = ((struct pex_vars *)dm_vars)->fontstruct->fid;
    ((struct pex_vars *)dm_vars)->gc = XCreateGC(((struct pex_vars *)dm_vars)->dpy,
					       ((struct pex_vars *)dm_vars)->win,
					       (GCFont|GCForeground|GCBackground),
						&gcv);
#else
    ((struct pex_vars *)dm_vars)->gc = XCreateGC(((struct pex_vars *)dm_vars)->dpy,
					       ((struct pex_vars *)dm_vars)->win,
					       (GCForeground|GCBackground),
					       &gcv);
#endif

/* Begin PEX stuff. */
    if(!count){
      if(PEXInitialize(((struct pex_vars *)dm_vars)->dpy,
		       &pex_info, 80, pex_err) != 0){
	bu_vls_free(&str);
	bu_log("Pex_setup: %s\n", pex_err);
	return -1;
      }

      if(!IMMED_MODE_SPT(pex_info)){
	bu_vls_free(&str);
	bu_log("Pex_setup: Immediate mode is not supported.\n");
	return -1;
      }
    }


    Pex_setup_renderer();

#if 0
    /* Register the file descriptor with the Tk event handler */
    Tk_CreateGenericHandler(Pex_doevent, (ClientData)NULL);
#endif

#if DO_XSELECTINPUT
    /* start with constant tracking OFF */
    XSelectInput(((struct pex_vars *)dm_vars)->dpy,
		 ((struct pex_vars *)dm_vars)->win,
		 ExposureMask|ButtonPressMask|KeyPressMask|StructureNotifyMask);
#endif

    Tk_SetWindowBackground(((struct pex_vars *)dm_vars)->xtkwin, ((struct pex_vars *)dm_vars)->bg);
    Tk_MapWindow(((struct pex_vars *)dm_vars)->xtkwin);

    ++count;
    return 0;
}

static void
establish_perspective()
{
  bu_vls_printf( &dm_values.dv_string,
		"set perspective %d\n",
		((struct pex_vars *)dm_vars)->mvars.perspective_mode ?
		perspective_table[((struct pex_vars *)dm_vars)->perspective_angle] : -1 );
  dmaflag = 1;
}

/*
   This routine will toggle the perspective_angle if the
   dummy_perspective value is 0 or less. Otherwise, the
   perspective_angle is set to the value of (dummy_perspective - 1).
*/
static void
set_perspective()
{
  /* set perspective matrix */
  if(((struct pex_vars *)dm_vars)->mvars.dummy_perspective > 0)
    ((struct pex_vars *)dm_vars)->perspective_angle = ((struct pex_vars *)dm_vars)->mvars.dummy_perspective <= 4 ?
    ((struct pex_vars *)dm_vars)->mvars.dummy_perspective - 1: 3;
  else if (--((struct pex_vars *)dm_vars)->perspective_angle < 0) /* toggle perspective matrix */
    ((struct pex_vars *)dm_vars)->perspective_angle = 3;

  if(((struct pex_vars *)dm_vars)->mvars.perspective_mode)
    bu_vls_printf( &dm_values.dv_string,
		  "set perspective %d\n",
		  perspective_table[((struct pex_vars *)dm_vars)->perspective_angle] );

  /*
     Just in case the "!" is used with the set command. This
     allows us to toggle through more than two values.
   */
  ((struct pex_vars *)dm_vars)->mvars.dummy_perspective = 1;

  dmaflag = 1;
}

static void
establish_am()
{
  if(am_mode){
    if(state != ST_S_PICK && state != ST_O_PICK &&
       state != ST_O_PATH && state != ST_S_VPICK){

      /* turn constant tracking ON */
      XdoMotion = 1;
    }
  }else{
    if(state != ST_S_PICK && state != ST_O_PICK &&
       state != ST_O_PATH && state != ST_S_VPICK){

      /* turn constant tracking OFF */
      XdoMotion = 0;
    }
  }
}

int
Pex_dm(argc, argv)
int argc;
char *argv[];
{
  struct bu_vls   vls;
  int status;
  char *av[4];
  char xstr[32];
  char ystr[32];
  char zstr[32];

  if( !strcmp( argv[0], "set" )){
    struct bu_vls tmp_vls;

    bu_vls_init(&vls);
    bu_vls_init(&tmp_vls);
    start_catching_output(&tmp_vls);

    if( argc < 2 )  {
      /* Bare set command, print out current settings */
      bu_struct_print("dm_X internal variables", Pex_vparse,
		     (CONST char *)&((struct pex_vars *)dm_vars)->mvars );
    } else if( argc == 2 ) {
      bu_vls_struct_item_named( &vls, Pex_vparse, argv[1],
			 (CONST char *)&((struct pex_vars *)dm_vars)->mvars, ',');
      bu_log( "%s\n", bu_vls_addr(&vls) );
    } else {
      bu_vls_printf( &vls, "%s=\"", argv[1] );
      bu_vls_from_argv( &vls, argc-2, argv+2 );
      bu_vls_putc( &vls, '\"' );
      bu_struct_parse( &vls, Pex_vparse, (char *)&((struct pex_vars *)dm_vars)->mvars);
    }

    bu_vls_free(&vls);

    stop_catching_output(&tmp_vls);
    Tcl_AppendResult(interp, bu_vls_addr(&tmp_vls), (char *)NULL);
    bu_vls_free(&tmp_vls);
    return TCL_OK;
  }

  if( !strcmp( argv[0], "m")){
    int up;
    int xpos, ypos;

    scroll_active = 0;

    if( argc < 5){
      Tcl_AppendResult(interp, "dm m: need more parameters\n",
		       "dm m button 1|0 xpos ypos\n", (char *)NULL);
      return TCL_ERROR;
    }

    /* This assumes a 3-button mouse */
    switch(*argv[1]){
    case '1':
      ((struct pex_vars *)dm_vars)->mb_mask = Button1Mask;
      break;
    case '2':
      ((struct pex_vars *)dm_vars)->mb_mask = Button2Mask;
      break;
    case '3':
      ((struct pex_vars *)dm_vars)->mb_mask = Button3Mask;
      break;
    default:
      Tcl_AppendResult(interp, "dm m: bad button value - ", argv[1], "\n", (char *)NULL);
      return TCL_ERROR;
    }

    bu_vls_init(&vls);
    bu_vls_printf(&vls, "M %s %d %d\n", argv[2],
		  Xx_TO_GED(atoi(argv[3])), Xy_TO_GED(atoi(argv[4])));
    status = cmdline(&vls, FALSE);
    bu_vls_free(&vls);

    if(status == CMD_OK)
      return TCL_OK;

    return TCL_ERROR;
  }

  status = TCL_OK;
  if( !strcmp( argv[0], "am" )){
    int buttonpress;

    scroll_active = 0;

    if( argc < 5){
      Tcl_AppendResult(interp, "dm am: need more parameters\n",
		       "dm am <r|t|z> 1|0 xpos ypos\n", (char *)NULL);
      return TCL_ERROR;
    }

    buttonpress = atoi(argv[2]);
    ((struct pex_vars *)dm_vars)->omx = atoi(argv[3]);
    ((struct pex_vars *)dm_vars)->omy = atoi(argv[4]);

    if(buttonpress){
      switch(*argv[1]){
      case 'r':
	am_mode = ALT_MOUSE_MODE_ROTATE;
	break;
      case 't':
	am_mode = ALT_MOUSE_MODE_TRANSLATE;
	if((state == ST_S_EDIT || state == ST_O_EDIT) && !EDIT_ROTATE &&
	              (edobj || es_edflag > 0)){
	  fastf_t fx, fy;

	  bu_vls_init(&vls);
	  fx = (((struct pex_vars *)dm_vars)->omx/
		(fastf_t)((struct pex_vars *)dm_vars)->width - 0.5) * 2;
	  fy = (0.5 - ((struct pex_vars *)dm_vars)->omy/
		(fastf_t)((struct pex_vars *)dm_vars)->height) * 2;

	  bu_vls_printf( &vls, "knob aX %f aY %f\n", fx, fy);
	  (void)cmdline(&vls, FALSE);
	  bu_vls_free(&vls);
	}

	break;
      case 'z':
	am_mode = ALT_MOUSE_MODE_ZOOM;
	break;
      default:
	Tcl_AppendResult(interp, "dm am: need more parameters\n",
			 "dm am <r|t|z> 1|0 xpos ypos\n", (char *)NULL);
	return TCL_ERROR;
      }
    }else{
      am_mode = ALT_MOUSE_MODE_IDLE;
    }

    return status;
  }

  Tcl_AppendResult(interp, "dm: bad command - ", argv[0], "\n", (char *)NULL);
  return TCL_ERROR;
}

static void
Pex_var_init()
{
  dm_vars = (char *)bu_malloc(sizeof(struct pex_vars), "Pex_var_init: pex_vars");
  bzero((void *)dm_vars, sizeof(struct pex_vars));
  ((struct pex_vars *)dm_vars)->dm_list = curr_dm_list;
  ((struct pex_vars *)dm_vars)->perspective_angle = 3;

  /* initialize the modifiable variables */

  ((struct pex_vars *)dm_vars)->mvars.dummy_perspective = 1;
}


static void
Pex_load_startup()
{
  char *filename;

  bzero((void *)&head_pex_vars, sizeof(struct pex_vars));
  RT_LIST_INIT( &head_pex_vars.l );

  if((filename = getenv("DM_PEX_RCFILE")) != (char *)NULL )
    Tcl_EvalFile(interp, filename);
}


static struct dm_list *
get_dm_list(window)
Window window;
{
  register struct pex_vars *p;

  for( RT_LIST_FOR(p, pex_vars, &head_pex_vars.l) ){
    if(window == p->win)
      return p->dm_list;
  }

  return DM_LIST_NULL;
}

static void
Pex_setup_renderer()
{
  unsigned long mask = 0;

/*XXX Slowly adding more stuff */

#if TRY_DEPTHCUE
  mask |= PEXRADepthCueTable;
  ((struct pex_vars *)dm_vars)->rattrs.view_table =
    PEXCreateLookupTable(((struct pex_vars *)dm_vars)->dpy,
			 ((struct pex_vars *)dm_vars)->win, PEXLUTDepthCue);
#endif

  mask |= PEXRAViewTable;
  ((struct pex_vars *)dm_vars)->rattrs.view_table =
    PEXCreateLookupTable(((struct pex_vars *)dm_vars)->dpy,
			 ((struct pex_vars *)dm_vars)->win, PEXLUTView);

  ((struct pex_vars *)dm_vars)->renderer =
    PEXCreateRenderer(((struct pex_vars *)dm_vars)->dpy,
		      ((struct pex_vars *)dm_vars)->win,
		      mask, &((struct pex_vars *)dm_vars)->rattrs);

#if TRY_DEPTHCUE
  PEXSetDepthCueIndex(((struct pex_vars *)dm_vars)->dpy,
		  ((struct pex_vars *)dm_vars)->renderer,
		  PEXOCRender, cue_num);
#endif

  PEXSetViewIndex(((struct pex_vars *)dm_vars)->dpy,
		  ((struct pex_vars *)dm_vars)->renderer,
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
