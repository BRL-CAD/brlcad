/*
   Just a note:

   These particular commands should not be used in
   mixed mode programming.

   qdevice
   blkqread
   qtest
   getbutton
   getvaluator
   setvaluator
   unqdevice
   mapcolor
   gconfig
   doublebuffer
   RGBmode
   winopen
   foreground
   noborder
   keepaspect
   prefposition
*/
/*
 *			D M - G L X . C
 *
 *  This display manager started out with the guts from DM-4D which
 *  was modified to use the glx widget.
 *
 *  Authors -
 *      Robert G. Parker
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

#include "conf.h"

#include <stdio.h>
#include <math.h>
#include <termio.h>
#undef VMIN		/* is used in vmath.h, too */
#include <ctype.h>

#include <X11/X.h>
#include <gl/glws.h>
#include "tk.h"
#include <X11/extensions/XI.h>
#include <X11/extensions/XInput.h>
#include <X11/Xutil.h>

#include <gl/gl.h>		/* SGI IRIS library */
#include <gl/device.h>		/* SGI IRIS library */
#include <gl/get.h>		/* SGI IRIS library */
#include <gl/cg2vme.h>		/* SGI IRIS, for DE_R1 defn on IRIX 3 */
#include <gl/addrs.h>		/* SGI IRIS, for DER1_STEREO defn on IRIX 3 */
#include <sys/types.h>
#include <sys/time.h>
#include <sys/invent.h>

#include "machine.h"
#include "vmath.h"
#include "bu.h"
#include "raytrace.h"
#include "./ged.h"
#include "./dm.h"
#include "externs.h"
#include "./solid.h"
#include "./sedit.h"

#define YSTEREO		491	/* subfield height, in scanlines */
#define YOFFSET_LEFT	532	/* YSTEREO + YBLANK ? */

extern int dm_pipe[];

extern Tcl_Interp *interp;
extern Tk_Window tkwin;
extern inventory_t	*getinvent();

/* Display Manager package interface */

#define IRBOUND	4095.9	/* Max magnification in Rot matrix */

int	Glx_open();
void	Glx_close();
MGED_EXTERN(void	Glx_input, (fd_set *input, int noblock) );
void	Glx_prolog(), Glx_epilog();
void	Glx_normal(), Glx_newrot();
void	Glx_update();
void	Glx_puts(), Glx_2d_line(), Glx_light();
int	Glx_object();
unsigned Glx_cvtvecs(), Glx_load();
void	Glx_statechange(), Glx_viewchange(), Glx_colorchange();
void	Glx_window(), Glx_debug();
int	Glx_dm();

static int Glx_setup();
static void set_knob_offset();
static void   Glx_load_startup();
static struct dm_list *get_dm_list();
#ifdef USE_PROTOTYPES
static Tk_GenericProc Glx_doevent;
#else
static int Glx_doevent();
#endif
static void glx_var_init();

#define dpy (((struct glx_vars *)dm_vars)->_dpy)
#define win (((struct glx_vars *)dm_vars)->_win)
#define xtkwin (((struct glx_vars *)dm_vars)->_xtkwin)
#define vis (((struct glx_vars *)dm_vars)->_vis)
#define cmap (((struct glx_vars *)dm_vars)->_cmap)
#define deep (((struct glx_vars *)dm_vars)->_deep)
#define mb_mask (((struct glx_vars *)dm_vars)->_mb_mask)
#define win_l (((struct glx_vars *)dm_vars)->_win_l)
#define win_b (((struct glx_vars *)dm_vars)->_win_b)
#define win_r (((struct glx_vars *)dm_vars)->_win_r)
#define win_t (((struct glx_vars *)dm_vars)->_win_t)
#define winx_size (((struct glx_vars *)dm_vars)->_winx_size)
#define winy_size (((struct glx_vars *)dm_vars)->_winy_size)
#define omx (((struct glx_vars *)dm_vars)->_omx)
#define omy (((struct glx_vars *)dm_vars)->_omy)
#define perspective_angle (((struct glx_vars *)dm_vars)->_perspective_angle)
#define devmotionnotify (((struct glx_vars *)dm_vars)->_devmotionnotify)
#define devbuttonpress (((struct glx_vars *)dm_vars)->_devbuttonpress)
#define devbuttonrelease (((struct glx_vars *)dm_vars)->_devbuttonrelease)
#define knobs (((struct glx_vars *)dm_vars)->_knobs)
#define stereo_is_on (((struct glx_vars *)dm_vars)->_stereo_is_on)
#define glx_is_gt (((struct glx_vars *)dm_vars)->_glx_is_gt)
#define aspect (((struct glx_vars *)dm_vars)->_aspect)
#define mvars (((struct glx_vars *)dm_vars)->_mvars)

struct modifiable_glx_vars {
  int cueing_on;
  int zclipping_on;
  int zbuffer_on;
  int lighting_on;
  int perspective_mode;
  int dummy_perspective;
  int zbuf;
  int rgb;
  int doublebuffer;
  int min_scr_z;       /* based on getgdesc(GD_ZMIN) */
  int max_scr_z;       /* based on getgdesc(GD_ZMAX) */
  int debug;
  int linewidth;
};

struct glx_vars {
  struct rt_list l;
  struct dm_list *dm_list;
  Display *_dpy;
  Window _win;
  Tk_Window _xtkwin;
  Visual *_vis;
  Colormap _cmap;
  int _deep;
  unsigned int _mb_mask;
  long _win_l, _win_b, _win_r, _win_t;
  long _winx_size, _winy_size;
  int _omx, _omy;
  int _perspective_angle;
  int _devmotionnotify;
  int _devbuttonpress;
  int _devbuttonrelease;
  int _knobs[8];
  int _stereo_is_on;
  int _glx_is_gt;
  fastf_t _aspect;
  struct modifiable_glx_vars _mvars;
};

static struct glx_vars head_glx_vars;
static int GLXdoMotion = 0;
static GLXconfig glx_config_wish_list [] = {
  { GLX_NORMAL, GLX_WINDOW, GLX_NONE },
  { GLX_NORMAL, GLX_DOUBLE, TRUE },
  { GLX_NORMAL, GLX_RGB, TRUE },
  { GLX_NORMAL, GLX_ZSIZE, GLX_NOCONFIG },
  { 0, 0, 0 }
};
static int perspective_table[] = { 
	30, 45, 60, 90 };
static int ovec = -1;		/* Old color map entry number */
static int kblights();
static double	xlim_view = 1.0;	/* args for ortho() */
static double	ylim_view = 1.0;

void		glx_colorit();

#ifdef IR_BUTTONS
/*
 * Map SGI Button numbers to MGED button functions.
 * The layout of this table is suggestive of the actual button box layout.
 */
#define SW_HELP_KEY	SW0
#define SW_ZERO_KEY	SW3
#define HELP_KEY	0
#define ZERO_KNOBS	0
static unsigned char bmap[IR_BUTTONS] = {
	HELP_KEY,    BV_ADCURSOR, BV_RESET,    ZERO_KNOBS,
	BE_O_SCALE,  BE_O_XSCALE, BE_O_YSCALE, BE_O_ZSCALE, 0,           BV_VSAVE,
	BE_O_X,      BE_O_Y,      BE_O_XY,     BE_O_ROTATE, 0,           BV_VRESTORE,
	BE_S_TRANS,  BE_S_ROTATE, BE_S_SCALE,  BE_MENU,     BE_O_ILLUMINATE, BE_S_ILLUMINATE,
	BE_REJECT,   BV_BOTTOM,   BV_TOP,      BV_REAR,     BV_45_45,    BE_ACCEPT,
	BV_RIGHT,    BV_FRONT,    BV_LEFT,     BV_35_25
};
/* Inverse map for mapping MGED button functions to SGI button numbers */
static unsigned char invbmap[BV_MAXFUNC+1];

/* bit 0 == switchlight 0 */
static unsigned long lights;
#endif

#ifdef IR_KNOBS
static int irlimit();			/* provides knob dead spot */
static int Glx_add_tol();
#define NOISE 32		/* Size of dead spot on knob */
/*
 *  Labels for knobs in help mode.
 */
static char	*kn1_knobs[] = {
	/* 0 */ "adc <1",	/* 1 */ "zoom", 
	/* 2 */ "adc <2",	/* 3 */ "adc dist",
	/* 4 */ "adc y",	/* 5 */ "y slew",
	/* 6 */ "adc x",	/* 7 */	"x slew"
};
static char	*kn2_knobs[] = {
	/* 0 */ "unused",	/* 1 */	"zoom",
	/* 2 */ "z rot",	/* 3 */ "z slew",
	/* 4 */ "y rot",	/* 5 */ "y slew",
	/* 6 */ "x rot",	/* 7 */	"x slew"
};
#endif

/*
 * SGI Color Map table
 */
#define NSLOTS		4080	/* The mostest possible - may be fewer */
static int glx_nslots=0;		/* how many we have, <= NSLOTS */
static int slotsused;		/* how many actually used */
static struct rgbtab {
	unsigned char	r;
	unsigned char	g;
	unsigned char	b;
} glx_rgbtab[NSLOTS];

struct dm dm_glx = {
	Glx_open, Glx_close,
	Glx_input,
	Glx_prolog, Glx_epilog,
	Glx_normal, Glx_newrot,
	Glx_update,
	Glx_puts, Glx_2d_line,
	Glx_light,
	Glx_object,
	Glx_cvtvecs, Glx_load,
	Glx_statechange,
	Glx_viewchange,
	Glx_colorchange,
	Glx_window, Glx_debug,
	0,			/* no "displaylist", per. se. */
	0,			/* multi-window */
	IRBOUND,
	"glx", "SGI - mixed mode",
	0,			/* mem map */
	Glx_dm
};

extern struct device_values dm_values;	/* values read from devices */

static void     establish_perspective();
static void     set_perspective();
static void	establish_lighting();
static void	establish_zbuffer();
static void     establish_am();
static void set_window();
static XVisualInfo *extract_visual();
static unsigned long extract_value();

static void
refresh_hook()
{
	dmaflag = 1;
}

#define GLX_MV_O(_m) offsetof(struct modifiable_glx_vars, _m)
struct bu_structparse Glx_vparse[] = {
	{"%d",	1, "depthcue",		GLX_MV_O(cueing_on),	Glx_colorchange },
	{"%d",  1, "zclip",		GLX_MV_O(zclipping_on),	refresh_hook },
	{"%d",  1, "zbuffer",		GLX_MV_O(zbuffer_on),	establish_zbuffer },
	{"%d",  1, "lighting",		GLX_MV_O(lighting_on),	establish_lighting },
	{"%d",  1, "perspective",       GLX_MV_O(perspective_mode), establish_perspective },
	{"%d",  1, "set_perspective",GLX_MV_O(dummy_perspective),  set_perspective },
	{"%d",  1, "has_zbuf",		GLX_MV_O(zbuf),	refresh_hook },
	{"%d",  1, "has_rgb",		GLX_MV_O(rgb),	Glx_colorchange },
	{"%d",  1, "has_doublebuffer",	GLX_MV_O(doublebuffer), refresh_hook },
	{"%d",  1, "min_scr_z",		GLX_MV_O(min_scr_z),	refresh_hook },
	{"%d",  1, "max_scr_z",		GLX_MV_O(max_scr_z),	refresh_hook },
	{"%d",  1, "debug",		GLX_MV_O(debug),	FUNC_NULL },
	{"%d",  1, "linewidth",		GLX_MV_O(linewidth),	refresh_hook },
	{"",	0,  (char *)0,		0,			FUNC_NULL }
};

static int	glx_oldmonitor;		/* Old monitor type */
static long gr_id;

/* Map +/-2048 GED space into -1.0..+1.0 :: x/2048*/
#define GED2IRIS(x)	(((float)(x))*0.00048828125)

/*
 *  Mouse coordinates are in absolute screen space, not relative to
 *  the window they came from.  Convert to window-relative,
 *  then to MGED-style +/-2048 range.
 */
static int
irisX2ged(x)
register int x;
{
  return ((x/(double)winx_size - 0.5) * 4095);
}

static int
irisY2ged(y)
register int y;
{
  return ((0.5 - y/(double)winy_size) * 4095);
}

/* 
 *			I R _ C O N F I G U R E _ W I N D O W _ S H A P E
 *
 *  Either initially, or on resize/reshape of the window,
 *  sense the actual size of the window, and perform any
 *  other initializations of the window configuration.
 */
static void
Glx_configure_window_shape()
{
  int		npix;
  XWindowAttributes xwa;

  XGetWindowAttributes( dpy, win, &xwa );
  winx_size = xwa.width;
  winy_size = xwa.height;

  /* Write enable all the bloody bits after resize! */
  viewport(0, winx_size, 0, winy_size);

  if( mvars.zbuf )
    establish_zbuffer();

  establish_lighting();
	
  if( mvars.doublebuffer){
    /* Clear out image from windows underneath */
    frontbuffer(1);
    glx_clear_to_black();
    frontbuffer(0);
    glx_clear_to_black();
  } else
    glx_clear_to_black();

  ortho( -xlim_view, xlim_view, -ylim_view, ylim_view, -1.0, 1.0 );
  aspect = (fastf_t)winy_size/(fastf_t)winx_size;
}

#define CMAP_BASE	32
#define CMAP_RAMP_WIDTH	16
#define MAP_ENTRY(x)	((mvars.cueing_on) ? \
			((x) * CMAP_RAMP_WIDTH + CMAP_BASE) : \
			((x) + CMAP_BASE) )


/*
 *			I R _ O P E N
 *
 *  Fire up the display manager, and the display processor. Note that
 *  this brain-damaged version of the MEX display manager gets terribly
 *  confused if you try to close your last window.  Tough. We go ahead
 *  and close the window.  Ignore the "ERR_CLOSEDLASTWINDOW" error
 *  message. It doesn't hurt anything.  Silly MEX.
 */
Glx_open()
{
#ifdef DM_OGL
  /* This is a hack to handle the fact that the sgi attach crashes
   * if a direct OpenGL context has been previously opened in the 
   * current mged session. This stops the attach before it crashes.
   */
  if (ogl_ogl_used){
    Tcl_AppendResult(interp, "Can't attach sgi, because a direct OpenGL context has\n",
		     "previously been opened in this session. To use sgi,\n",
		     "quit this session and reopen it.\n", (char *)NULL);
    return TCL_ERROR;
  }
  ogl_sgi_used = 1;
#endif /* DM_OGL */
  glx_var_init();
  return Glx_setup(dname);
}

static int
Glx_setup( name )
char *name;
{
  register int	i;
  static int count = 0;
  Matrix		m;
  inventory_t	*inv;
  int		win_size=1000;
  int		win_o_x=272;
  int		win_o_y=12;
  struct bu_vls str;
  int j, k;
  int ndevices;
  int nclass = 0;
  XDeviceInfoPtr olist, list;
  XDevice *dev;
  XEventClass e_class[15];
  XInputClassInfo *cip;
  GLXconfig *p, *glx_config;
  XVisualInfo *visual_info;

  bu_vls_init(&str);

  /* Only need to do this once */
  if(tkwin == NULL)
    gui_setup();

  /* Only need to do this once for this display manager */
  if(!count)
    Glx_load_startup();

  if(RT_LIST_IS_EMPTY(&head_glx_vars.l))
    Tk_CreateGenericHandler(Glx_doevent, (ClientData)NULL);

  RT_LIST_APPEND(&head_glx_vars.l, &((struct glx_vars *)curr_dm_list->_dm_vars)->l);

  bu_vls_printf(&pathName, ".dm_glx%d", count++);
  xtkwin = Tk_CreateWindowFromPath(interp, tkwin, bu_vls_addr(&pathName), name);
  /*
   * Create the X drawing window by calling init_glx which
   * is defined in glxinit.tcl
   */
  bu_vls_strcpy(&str, "init_glx ");
  bu_vls_printf(&str, "%s\n", bu_vls_addr(&pathName));

  if(cmdline(&str, FALSE) == CMD_BAD){
    bu_vls_free(&str);
    return -1;
  }

  bu_vls_free(&str);

  dpy = Tk_Display(xtkwin);
  winx_size = DisplayWidth(dpy, DefaultScreen(dpy)) - 20;
  winy_size = DisplayHeight(dpy, DefaultScreen(dpy)) - 20;

  /* Make window square */
  if( winy_size < winx_size )
    winx_size = winy_size;
  else /* we have a funky shaped monitor */ 
    winy_size = winx_size;

  Tk_GeometryRequest(xtkwin, winx_size, winy_size);

  glx_is_gt = 1;
  glx_config = GLXgetconfig(dpy, Tk_ScreenNumber(xtkwin), glx_config_wish_list);
  visual_info = extract_visual(GLX_NORMAL, glx_config);
  vis = visual_info->visual;
  deep = visual_info->depth;
  cmap = extract_value(GLX_NORMAL, GLX_COLORMAP, glx_config);
  Tk_SetWindowVisual(xtkwin, vis, deep, cmap);

  Tk_MakeWindowExist(xtkwin);
  win = Tk_WindowId(xtkwin);
  set_window(GLX_NORMAL, win, glx_config);

  /* Inform the GL that you intend to render GL into an X window */
  if(GLXlink(dpy, glx_config) < 0)
    return -1;

  GLXwinset(dpy, win);

  /* set configuration variables */
  for(p = glx_config; p->buffer; ++p){
    switch(p->buffer){
    case GLX_NORMAL:
      switch(p->mode){
      case GLX_ZSIZE:
	if(p->arg)
	  mvars.zbuf = 1;
	else
	  mvars.zbuf = 0;

	break;
      case GLX_RGB:
	if(p->arg)
	  mvars.rgb = 1;
	else
	  mvars.rgb = 0;
	
	break;
      case GLX_DOUBLE:
	if(p->arg)
	  mvars.doublebuffer = 1;
	else
	  mvars.doublebuffer = 0;

	break;
      case GLX_STEREOBUF:
	stereo_is_on = 1;

	break;
      case GLX_BUFSIZE:
      case GLX_STENSIZE:
      case GLX_ACSIZE:
      case GLX_VISUAL:
      case GLX_COLORMAP:
      case GLX_WINDOW:
      case GLX_MSSAMPLE:
      case GLX_MSZSIZE:
      case GLX_MSSSIZE:
      case GLX_RGBSIZE:
      default:
	break;
      }
    case GLX_OVERLAY:
    case GLX_POPUP:
    case GLX_UNDERLAY:
    default:
      break;
    }
  }

  free((void *)glx_config);
	
  if (mged_variables.sgi_win_size > 0)
    win_size = mged_variables.sgi_win_size;

  if (mged_variables.sgi_win_origin[0] != 0)
    win_o_x = mged_variables.sgi_win_origin[0];

  if (mged_variables.sgi_win_origin[1] != 0)
    win_o_y = mged_variables.sgi_win_origin[1];

  winconstraints();	/* remove constraints on the window size */

  /*
   * Establish GL library operating modes
   */
  /* Don't draw polygon edges */
  glcompat( GLC_OLDPOLYGON, 0 );

  /* Z-range mapping */
  /* Z range from getgdesc(GD_ZMIN)
   * to getgdesc(GD_ZMAX).
   * Hardware specific.
   */
  glcompat( GLC_ZRANGEMAP, 0 );

  /* Take off a smidgeon for wraparound, as suggested by SGI manual */

  mvars.min_scr_z = getgdesc(GD_ZMIN)+15;
  mvars.max_scr_z = getgdesc(GD_ZMAX)-15;

  Glx_configure_window_shape();

  /* Line style 0 is solid.  Program line style 1 as dot-dashed */
  deflinestyle( 1, 0xCF33 );
  setlinestyle( 0 );

  /*
   * Take a look at the available input devices. We're looking
   * for "dial+buttons".
   */
  olist = list = (XDeviceInfoPtr) XListInputDevices (dpy, &ndevices);

  /* IRIX 4.0.5 bug workaround */
  if( list == (XDeviceInfoPtr)NULL ||
      list == (XDeviceInfoPtr)1 )  goto Done;

  for(j = 0; j < ndevices; ++j, list++){
    if(list->use == IsXExtensionDevice){
      if(!strcmp(list->name, "dial+buttons")){
	if((dev = XOpenDevice(dpy, list->id)) == (XDevice *)NULL){
	  Tcl_AppendResult(interp, "Glx_open: Couldn't open the dials+buttons\n",
			   (char *)NULL);
	  goto Done;
	}

	for(cip = dev->classes, k = 0; k < dev->num_classes;
	    ++k, ++cip){
	  switch(cip->input_class){
#if IR_BUTTONS
	  case ButtonClass:
	    DeviceButtonPress(dev, devbuttonpress, e_class[nclass]);
	    ++nclass;
	    DeviceButtonRelease(dev, devbuttonrelease, e_class[nclass]);
	    ++nclass;
#endif
	    break;
#if IR_KNOBS
	  case ValuatorClass:
	    DeviceMotionNotify(dev, devmotionnotify, e_class[nclass]);
	    ++nclass;
	    break;
#endif
	  default:
	    break;
	  }
	}

	XSelectExtensionEvent(dpy, win, e_class, nclass);
	goto Done;
      }
    }
  }
Done:
  XFreeDeviceList(olist);

  Tk_MapWindow(xtkwin);
  return(0);
}

/*XXX Just experimenting */
void
Glx_load_startup()
{
  char *filename;

  bzero((void *)&head_glx_vars, sizeof(struct glx_vars));
  RT_LIST_INIT( &head_glx_vars.l );

  if((filename = getenv("DM_GLX_RCFILE")) != (char *)NULL )
    Tcl_EvalFile(interp, filename);
}

/*
 *  			I R _ C L O S E
 *  
 *  Gracefully release the display.  Well, mostly gracefully -- see
 *  the comments in the open routine.
 */
void
Glx_close()
{
  if(ogl_ogl_used)
    return;

  if(xtkwin != NULL){
    if(mvars.cueing_on)
      depthcue(0);

    lampoff( 0xf );

    /* avoids error messages when reattaching */
    mmode(MVIEWING);	
    lmbind(LIGHT2,0);
    lmbind(LIGHT3,0);
    lmbind(LIGHT4,0);
    lmbind(LIGHT5,0);

    frontbuffer(1);
    glx_clear_to_black();
    frontbuffer(0);

    GLXunlink(dpy, win);
    Tk_DestroyWindow(xtkwin);
  }

  if(((struct glx_vars *)dm_vars)->l.forw != RT_LIST_NULL)
    RT_LIST_DEQUEUE(&((struct glx_vars *)dm_vars)->l);

  bu_free(dm_vars, "Glx_close: dm_vars");

  if(RT_LIST_IS_EMPTY(&head_glx_vars.l))
    Tk_DeleteGenericHandler(Glx_doevent, (ClientData)NULL);
}

/*
 *			G L X _ P R O L O G
 *
 * Define the world, and include in it instances of all the
 * important things.  Most important of all is the object "faceplate",
 * which is built between dmr_normal() and dmr_epilog()
 * by dmr_puts and dmr_2d_line calls from adcursor() and dotitles().
 */
void
Glx_prolog()
{
  GLXwinset(dpy, win);

  if (mvars.debug)
    Tcl_AppendResult(interp, "Glx_prolog\n", (char *)NULL);

  ortho( -xlim_view, xlim_view, -ylim_view, ylim_view, -1.0, 1.0 );

  if( !mvars.doublebuffer ){
    glx_clear_to_black();
    return;
  }

  linewidth(mvars.linewidth);
}

/*
 *			I R _ N O R M A L
 *
 * Restore the display processor to a normal mode of operation
 * (ie, not scaled, rotated, displaced, etc).
 * Turns off windowing.
 */
void
Glx_normal()
{
  if (mvars.debug)
    Tcl_AppendResult(interp, "Glx_normal\n", (char *)NULL);

  if( mvars.rgb )  {
    RGBcolor( (short)0, (short)0, (short)0 );
  } else {
    color(BLACK);
  }

  ortho( -xlim_view, xlim_view, -ylim_view, ylim_view, -1.0, 1.0 );
}

/*
 *			I R _ E P I L O G
 */
void
Glx_epilog()
{
  if (mvars.debug)
    Tcl_AppendResult(interp, "Glx_epilog\n", (char *)NULL);

  /*
   * A Point, in the Center of the Screen.
   * This is drawn last, to always come out on top.
   */
  Glx_2d_line( 0, 0, 0, 0, 0 );
  /* End of faceplate */

  if(mvars.doublebuffer){
    swapbuffers();
    /* give Graphics pipe time to work */
    glx_clear_to_black();
  }
}

/*
 *  			I R _ N E W R O T
 *
 *  Load a new rotation matrix.  This will be followed by
 *  many calls to Glx_object().
 *
 *  IMPORTANT COORDINATE SYSTEM NOTE:
 *
 *  MGED uses a right handed coordinate system where +Z points OUT of
 *  the screen.  The Silicon Graphics uses a left handed coordinate
 *  system where +Z points INTO the screen.
 *  This difference in convention is handled here.
 *  The conversion is accomplished by concatenating a matrix multiply
 *  by
 *            (  1    0    0   0  )
 *            (  0    1    0   0  )
 *            (  0    0   -1   0  )
 *            (  0    0    0   1  )
 *
 *  However, this is actually implemented by straight-line code which
 *  flips the sign on the entire third row.
 *
 *  Note that through BRL-CAD Release 3.7 this was handled by flipping
 *  the direction of the shade ramps.  Now, with the Z-buffer being used,
 *  the correct solution is important.
 */
void
Glx_newrot(mat, which_eye)
mat_t	mat;
{
	register fastf_t *mptr;
	Matrix	gtmat;
	mat_t	newm;
	int	i;

	if (mvars.debug)
	  Tcl_AppendResult(interp, "Glx_newrot()\n", (char *)NULL);

	switch(which_eye)  {
	case 0:
		/* Non-stereo */
		break;
	case 1:
		/* R eye */
		viewport(0, XMAXSCREEN, 0, YSTEREO);
		Glx_puts( "R", 2020, 0, 0, DM_RED );
		break;
	case 2:
		/* L eye */
		viewport(0, XMAXSCREEN, 0+YOFFSET_LEFT, YSTEREO+YOFFSET_LEFT);
		break;
	}

	if( ! mvars.zclipping_on ) {
		mat_t	nozclip;

		mat_idn( nozclip );
		nozclip[10] = 1.0e-20;
		mat_mul( newm, nozclip, mat );
		mptr = newm;
	} else {
		mptr = mat;
	}

	gtmat[0][0] = *(mptr++) * aspect;
	gtmat[1][0] = *(mptr++) * aspect;
	gtmat[2][0] = *(mptr++) * aspect;
	gtmat[3][0] = *(mptr++) * aspect;

	gtmat[0][1] = *(mptr++);
	gtmat[1][1] = *(mptr++);
	gtmat[2][1] = *(mptr++);
	gtmat[3][1] = *(mptr++);

	gtmat[0][2] = *(mptr++);
	gtmat[1][2] = *(mptr++);
	gtmat[2][2] = *(mptr++);
	gtmat[3][2] = *(mptr++);

	gtmat[0][3] = *(mptr++);
	gtmat[1][3] = *(mptr++);
	gtmat[2][3] = *(mptr++);
	gtmat[3][3] = *(mptr++);

	/*
	 *  Convert between MGED's right handed coordinate system
	 *  where +Z comes out of the screen to the Silicon Graphics's
	 *  left handed coordinate system, where +Z goes INTO the screen.
	 */
	gtmat[0][2] = -gtmat[0][2];
	gtmat[1][2] = -gtmat[1][2];
	gtmat[2][2] = -gtmat[2][2];
	gtmat[3][2] = -gtmat[3][2];

	loadmatrix( gtmat );
}

static float material_objdef[] = {
	ALPHA,		1.0,	
	AMBIENT,	0.2, 0.2, 0.2,	/* 0.4 in rt */
	DIFFUSE,	0.6, 0.6, 0.6,
	SPECULAR,	0.2, 0.2, 0.2,
	EMISSION,	0.0, 0.0, 0.0,
	SHININESS,	10.0,
	LMNULL   };

/*
 *  			I R _ O B J E C T
 *  
 *  Set up for an object, transformed as indicated, and with an
 *  object center as specified.  The ratio of object to screen size
 *  is passed in as a convienience.  Mat is model2view.
 *
 *  Returns -
 *	 0 if object could be drawn
 *	!0 if object was omitted.
 */
int
Glx_object( sp, m, ratio, white )
register struct solid *sp;
fastf_t		*m;
double		ratio;
int		white;
{
	register struct rt_vlist	*vp;
	register int nvec;
	register float	*gtvec;
	char	gtbuf[16+3*sizeof(double)];
	int first;
	int i,j;

	if (mvars.debug)
	  Tcl_AppendResult(interp, "Glx_Object()\n", (char *)NULL);

	/*
	 *  It is claimed that the "dancing vector disease" of the
	 *  4D GT processors is due to the array being passed to v3f()
	 *  not being quad-word aligned (16-byte boundary).
	 *  This hack ensures that the buffer has this alignment.
	 *  Note that this requires gtbuf to be 16 bytes longer than needed.
	 */
	gtvec = (float *)((((int)gtbuf)+15) & (~0xF));

	/*
	 * IMPORTANT DEPTHCUEING NOTE
	 *
	 * Also note that the depthcueing shaderange() routine wanders
	 * outside it's allotted range due to roundoff errors.  A buffer
	 * entry is kept on each end of the shading curves, and the
	 * highlight mode uses the *next* to the brightest entry --
	 * otherwise it can (and does) fall off the shading ramp.
	 */
	if (sp->s_soldash)
		setlinestyle( 1 );		/* set dot-dash */

	if( mvars.rgb )  {
		register short	r, g, b;
		if( white )  {
			r = g = b = 230;
		} else {
			r = (short)sp->s_color[0];
			g = (short)sp->s_color[1];
			b = (short)sp->s_color[2];
		}
		if(mvars.cueing_on)  {
			lRGBrange(
			    r/10, g/10, b/10,
			    r, g, b,
			    mvars.min_scr_z, mvars.max_scr_z );
		} else
		if(mvars.lighting_on && glx_is_gt)
		{
			/* Ambient = .2, Diffuse = .6, Specular = .2 */

			/* Ambient */
			material_objdef[3] = 	.2 * ( r / 255.0);
			material_objdef[4] = 	.2 * ( g / 255.0);
			material_objdef[5] = 	.2 * ( b / 255.0);

			/* diffuse */
			material_objdef[7] = 	.6 * ( r / 255.0);
			material_objdef[8] = 	.6 * ( g / 255.0);
			material_objdef[9] = 	.6 * ( b / 255.0);

			/* Specular */
			material_objdef[11] = 	.2 * ( r / 255.0);
			material_objdef[12] = 	.2 * ( g / 255.0);
			material_objdef[13] = 	.2 * ( b / 255.0);

			lmdef(DEFMATERIAL, 21, 0, material_objdef);
			lmbind(MATERIAL, 21);

		} else

			RGBcolor( r, g, b );
	} else {
		if( white ) {
			ovec = nvec = MAP_ENTRY(DM_WHITE);
			/* Use the *next* to the brightest white entry */
			if(mvars.cueing_on)  {
				lshaderange(nvec+1, nvec+1,
				    mvars.min_scr_z, mvars.max_scr_z );
			}
			color( nvec );
		} else {
			if( (nvec = MAP_ENTRY( sp->s_dmindex )) != ovec) {
				/* Use only the middle 14 to allow for roundoff...
				 * Pity the poor fool who has defined a black object.
				 * The code will use the "reserved" color map entries
				 * to display it when in depthcued mode.
				 */
				if(mvars.cueing_on)  {
					lshaderange(nvec+1, nvec+14,
					    mvars.min_scr_z, mvars.max_scr_z );
				}
				color( nvec );
				ovec = nvec;
			}
		}
	}

	/* Viewing region is from -1.0 to +1.0 */
	first = 1;
	for( RT_LIST_FOR( vp, rt_vlist, &(sp->s_vlist) ) )  {
		register int	i;
		register int	nused = vp->nused;
		register int	*cmd = vp->cmd;
		register point_t *pt = vp->pt;
		for( i = 0; i < nused; i++,cmd++,pt++ )  {
			switch( *cmd )  {
			case RT_VLIST_LINE_MOVE:
				/* Move, start line */
				if( first == 0 )
					endline();
				first = 0;
				bgnline();
				v3d( *pt );
				break;
			case RT_VLIST_LINE_DRAW:
				/* Draw line */
				v3d( *pt );
				break;
			case RT_VLIST_POLY_START:
				/* Start poly marker & normal */
				if( first == 0 )
					endline();
				/* concave(TRUE); */
				bgnpolygon();
				/* Set surface normal (vl_pnt points outward) */
				VMOVE( gtvec, *pt );
				n3f(gtvec);
				break;
			case RT_VLIST_POLY_MOVE:
				/* Polygon Move */
				v3d( *pt );
				break;
			case RT_VLIST_POLY_DRAW:
				/* Polygon Draw */
				v3d( *pt );
				break;
			case RT_VLIST_POLY_END:
				/* Draw, End Polygon */
				v3d( *pt );
				endpolygon();
				first = 1;
				break;
			case RT_VLIST_POLY_VERTNORM:
				/* Set per-vertex normal.  Given before vert. */
				VMOVE( gtvec, *pt );
				n3f(gtvec);
				break;
			}
		}
	}
	if( first == 0 ) endline();

	if (sp->s_soldash)
		setlinestyle(0);		/* restore solid lines */

	return(1);	/* OK */
}

/*
 *			I R _ U P D A T E
 *
 * Transmit accumulated displaylist to the display processor.
 * Last routine called in refresh cycle.
 */
void
Glx_update()
{
	if (mvars.debug)
	  Tcl_AppendResult(interp, "Glx_update()\n", (char *)NULL);

	return;
}

/*
 *			I R _ P U T S
 *
 * Output a string.
 * The starting position of the beam is as specified.
 */
void
Glx_puts( str, x, y, size, colour )
register char *str;
int x,y,size, colour;
{
	if (mvars.debug)
	  Tcl_AppendResult(interp, "Glx_puts()\n", (char *)NULL);

	cmov2( GED2IRIS(x), GED2IRIS(y));
	if( mvars.rgb )  {
		RGBcolor( (short)glx_rgbtab[colour].r,
		    (short)glx_rgbtab[colour].g,
		    (short)glx_rgbtab[colour].b );
	} else {
		color( MAP_ENTRY(colour) );
	}
	charstr( str );
}

/*
 *			I R _ 2 D _ L I N E
 *
 */
void
Glx_2d_line( x1, y1, x2, y2, dashed )
int x1, y1;
int x2, y2;
int dashed;
{
	register int nvec;

	if (mvars.debug)
	  Tcl_AppendResult(interp, "Glx_2d_line()\n", (char *)NULL);

	if( mvars.rgb )  {
		/* Yellow */
		if(mvars.cueing_on)  {
			lRGBrange(
			    255, 255, 0,
			    255, 255, 0,
			    mvars.min_scr_z, mvars.max_scr_z );
		}
		RGBcolor( (short)255, (short)255, (short) 0 );
	} else {
		if((nvec = MAP_ENTRY(DM_YELLOW)) != ovec) {
			if(mvars.cueing_on) lshaderange(nvec, nvec,
			    mvars.min_scr_z, mvars.max_scr_z );
			color( nvec );
			ovec = nvec;
		}
	}

	if( dashed )
		setlinestyle(1);	/* into dot-dash */

	move2( GED2IRIS(x1), GED2IRIS(y1));
	draw2( GED2IRIS(x2), GED2IRIS(y2));

	if( dashed )
		setlinestyle(0);	/* restore to solid */
}

/*
 *			I R _ I N P U T
 *
 * Execution must suspend in this routine until a significant event
 * has occured on either the command stream, or a device event has
 * occured, unless "noblock" is set.
 *
 * Implicit Return -
 *	If any files are ready for input, their bits will be set in 'input'.
 *	Otherwise, 'input' will be all zeros.
 */
void
Glx_input( input, noblock )
fd_set		*input;
int		noblock;
{
  return;
}


/*
   This routine does not handle mouse button or key events. The key
   events are being processed via the TCL/TK bind command or are being
   piped to ged.c/stdin_input(). Eventually, I'd also like to have the
   dials+buttons bindable. That would leave this routine to handle only
   events like Expose and ConfigureNotify.
*/
int
Glx_doevent(clientData, eventPtr)
ClientData clientData;
XEvent *eventPtr;
{
  static int button0  = 0;   /*  State of button 0 */
  static int knobs_during_help[8] = {0, 0, 0, 0, 0, 0, 0, 0};
  static int knob_values[8] = {0, 0, 0, 0, 0, 0, 0, 0};
  register struct dm_list *save_dm_list;
  register struct dm_list *p;
  struct bu_vls cmd;
  int status = CMD_OK;

  bu_vls_init(&cmd);
  save_dm_list = curr_dm_list;

  curr_dm_list = get_dm_list(eventPtr->xany.window);

  if(curr_dm_list == DM_LIST_NULL)
    goto end;

  if(mged_variables.send_key && eventPtr->type == KeyPress){
    char buffer[1];
    KeySym keysym;

    XLookupString(&(eventPtr->xkey), buffer, 1,
		  &keysym, (XComposeStatus *)NULL);

    if(keysym == mged_variables.hot_key)
      goto end;

    write(dm_pipe[1], buffer, 1);
    bu_vls_free(&cmd);
    curr_dm_list = save_dm_list;

    /* Use this so that these events won't propagate */
    return TCL_RETURN;
  }

  /* Now getting X events */
  if(eventPtr->type == Expose && eventPtr->xexpose.count == 0){
    /* Window may have moved */
    Glx_configure_window_shape();

    dirty = 1;
    refresh();
    goto end;
  }else if( eventPtr->type == ConfigureNotify ){
      /* Window may have moved */
      Glx_configure_window_shape();

      dirty = 1;
      refresh();
      goto end;
  }else if( eventPtr->type == MotionNotify ) {
    int mx, my;

    mx = eventPtr->xmotion.x;
    my = eventPtr->xmotion.y;

    switch(am_mode){
    case ALT_MOUSE_MODE_IDLE:
      if(scroll_active && eventPtr->xmotion.state & mb_mask)
	bu_vls_printf( &cmd, "M 1 %d %d\n", irisX2ged(mx), irisY2ged(my));
      else if(GLXdoMotion)
	/* do the regular thing */
	/* Constant tracking (e.g. illuminate mode) bound to M mouse */
	bu_vls_printf( &cmd, "M 0 %d %d\n", irisX2ged(mx), irisY2ged(my));
      else
	goto end;

      break;
    case ALT_MOUSE_MODE_ROTATE:
      bu_vls_printf( &cmd, "iknob ax %f ay %f\n",
		     (my - omy)/512.0, (mx - omx)/512.0 );
      break;
    case ALT_MOUSE_MODE_TRANSLATE:
      {
	fastf_t fx, fy;

	if((state == ST_S_EDIT || state == ST_O_EDIT) && !EDIT_ROTATE &&
	  (edobj || es_edflag > 0)){
	  fx = (mx/(fastf_t)winx_size - 0.5) * 2;
	  fy = (0.5 - my/(fastf_t)winy_size) * 2;
	  bu_vls_printf( &cmd, "knob aX %f aY %f\n", fx, fy );
	}else{
	  fx = (mx - omx)/(fastf_t)winx_size * 2.0;
	  fy = (omy - my)/(fastf_t)winy_size * 2.0;
	  bu_vls_printf( &cmd, "iknob aX %f aY %f\n", fx, fy );
	}
      }	     
      break;
    case ALT_MOUSE_MODE_ZOOM:
      bu_vls_printf( &cmd, "iknob aS %f\n", (omy - my)/(fastf_t)winy_size);
      break;
    }

    omx = mx;
    omy = my;
  }
#if IR_KNOBS
  else if( eventPtr->type == devmotionnotify ){
    XDeviceMotionEvent *M;
    int setting;

    M = (XDeviceMotionEvent * ) eventPtr;

    if(button0){
      glx_dbtext(
		(mged_variables.adcflag ? kn1_knobs:kn2_knobs)[M->first_axis]);
      goto end;
    }

    switch(DIAL0 + M->first_axis){
    case DIAL0:
      if(mged_variables.adcflag) {
	if(-NOISE < knobs[M->first_axis] && knobs[M->first_axis] < NOISE &&
	                !dv_1adc )
	  knobs[M->first_axis] += M->axis_data[0] - knob_values[M->first_axis];
	else
	  knobs[M->first_axis] = Glx_add_tol(dv_1adc) +
	                  M->axis_data[0] - knob_values[M->first_axis];

	setting = irlimit(knobs[M->first_axis]);
	bu_vls_printf( &cmd, "knob ang1 %d\n",
		      setting );
      }
      break;
    case DIAL1:
      if(mged_variables.rateknobs){
	if(-NOISE < knobs[M->first_axis] && knobs[M->first_axis] < NOISE &&
	   !rate_zoom )
	  knobs[M->first_axis] += M->axis_data[0] - knob_values[M->first_axis];
	else
	  knobs[M->first_axis] = Glx_add_tol((int)(512.5 * rate_zoom)) +
	    M->axis_data[0] - knob_values[M->first_axis];

	setting = irlimit(knobs[M->first_axis]);
	bu_vls_printf( &cmd , "knob S %f\n",
		       setting / 512.0 );
      }else{
	if(-NOISE < knobs[M->first_axis] && knobs[M->first_axis] < NOISE &&
	   !absolute_zoom )
	  knobs[M->first_axis] += M->axis_data[0] - knob_values[M->first_axis];
	else
	  knobs[M->first_axis] = Glx_add_tol((int)(512.5 * absolute_zoom)) +
	    M->axis_data[0] - knob_values[M->first_axis];

	setting = irlimit(knobs[M->first_axis]);
	bu_vls_printf( &cmd , "knob aS %f\n",
		       setting / 512.0 );
      }
      break;
    case DIAL2:
      if(mged_variables.adcflag){
	if(-NOISE < knobs[M->first_axis] && knobs[M->first_axis] < NOISE &&
	                !dv_2adc )
	  knobs[M->first_axis] += M->axis_data[0] - knob_values[M->first_axis];
	else
	  knobs[M->first_axis] = Glx_add_tol(dv_2adc) +
	                  M->axis_data[0] - knob_values[M->first_axis];

	setting = irlimit(knobs[M->first_axis]);
	bu_vls_printf( &cmd , "knob ang2 %d\n",
		      setting );
      }else {
	if(mged_variables.rateknobs){
	  if(-NOISE < knobs[M->first_axis] && knobs[M->first_axis] < NOISE &&
	     !rate_rotate[Z] )
	    knobs[M->first_axis] += M->axis_data[0] - knob_values[M->first_axis];
	  else
	    knobs[M->first_axis] = Glx_add_tol((int)(512.5 * rate_rotate[Z])) +
	      M->axis_data[0] - knob_values[M->first_axis];

	  setting = irlimit(knobs[M->first_axis]);
	  bu_vls_printf( &cmd , "knob z %f\n",
		      setting / 512.0 );
	}else{
	  if(-NOISE < knobs[M->first_axis] && knobs[M->first_axis] < NOISE &&
	     !absolute_rotate[Z] )
	    knobs[M->first_axis] += M->axis_data[0] - knob_values[M->first_axis];
	  else
	    knobs[M->first_axis] = Glx_add_tol((int)(512.5 * absolute_rotate[Z])) +
	      M->axis_data[0] - knob_values[M->first_axis];

	  setting = irlimit(knobs[M->first_axis]);
	  bu_vls_printf( &cmd , "knob az %f\n",
			 setting / 512.0 );
	}
      }
      break;
    case DIAL3:
      if(mged_variables.adcflag){
	if(-NOISE < knobs[M->first_axis] && knobs[M->first_axis] < NOISE &&
	   !dv_distadc)
	  knobs[M->first_axis] += M->axis_data[0] - knob_values[M->first_axis];
	else
	  knobs[M->first_axis] = Glx_add_tol(dv_distadc) +
	    M->axis_data[0] - knob_values[M->first_axis];

	setting = irlimit(knobs[M->first_axis]);
	bu_vls_printf( &cmd , "knob distadc %d\n",
		      setting );
      }else {
	if(mged_variables.rateknobs){
	  if(-NOISE < knobs[M->first_axis] && knobs[M->first_axis] < NOISE &&
	     !rate_slew[Z] )
	    knobs[M->first_axis] += M->axis_data[0] - knob_values[M->first_axis];
	  else
	    knobs[M->first_axis] = Glx_add_tol((int)(512.5 * rate_slew[Z])) +
	      M->axis_data[0] - knob_values[M->first_axis];

	  setting = irlimit(knobs[M->first_axis]);
	  bu_vls_printf( &cmd , "knob Z %f\n",
			 setting / 512.0 );
	}else{
	  if(-NOISE < knobs[M->first_axis] && knobs[M->first_axis] < NOISE &&
	     !absolute_slew[Z] )
	    knobs[M->first_axis] += M->axis_data[0] - knob_values[M->first_axis];
	  else
	    knobs[M->first_axis] = Glx_add_tol((int)(512.5 * absolute_slew[Z])) +
	      M->axis_data[0] - knob_values[M->first_axis];

	  setting = irlimit(knobs[M->first_axis]);
	  bu_vls_printf( &cmd , "knob aZ %f\n",
			 setting / 512.0 );
	}
      }
      break;
    case DIAL4:
      if(mged_variables.adcflag){
	if(-NOISE < knobs[M->first_axis] && knobs[M->first_axis] < NOISE &&
	   !dv_yadc)
	  knobs[M->first_axis] += M->axis_data[0] - knob_values[M->first_axis];
	else
	  knobs[M->first_axis] = Glx_add_tol(dv_yadc) +
	    M->axis_data[0] - knob_values[M->first_axis];

	setting = irlimit(knobs[M->first_axis]);
	bu_vls_printf( &cmd , "knob yadc %d\n",
		      setting );
      }else{
	if(mged_variables.rateknobs){
	  if(-NOISE < knobs[M->first_axis] && knobs[M->first_axis] < NOISE &&
	     !rate_rotate[Y] )
	    knobs[M->first_axis] += M->axis_data[0] - knob_values[M->first_axis];
	  else
	    knobs[M->first_axis] = Glx_add_tol((int)(512.5 * rate_rotate[Y])) +
	      M->axis_data[0] - knob_values[M->first_axis];

	  setting = irlimit(knobs[M->first_axis]);
	  bu_vls_printf( &cmd , "knob y %f\n",
			 setting / 512.0 );
	}else{
	  if(-NOISE < knobs[M->first_axis] && knobs[M->first_axis] < NOISE &&
	     !absolute_rotate[Y] )
	    knobs[M->first_axis] += M->axis_data[0] - knob_values[M->first_axis];
	  else
	    knobs[M->first_axis] = Glx_add_tol((int)(512.5 * absolute_rotate[Y])) +
	      M->axis_data[0] - knob_values[M->first_axis];

	  setting = irlimit(knobs[M->first_axis]);
	  bu_vls_printf( &cmd , "knob ay %f\n",
			 setting / 512.0 );
	}
      }
      break;
    case DIAL5:
      if(mged_variables.rateknobs){
	  if(-NOISE < knobs[M->first_axis] && knobs[M->first_axis] < NOISE &&
	     !rate_slew[Y] )
	    knobs[M->first_axis] += M->axis_data[0] - knob_values[M->first_axis];
	  else
	    knobs[M->first_axis] = Glx_add_tol((int)(512.5 * rate_slew[Y])) +
	      M->axis_data[0] - knob_values[M->first_axis];

	  setting = irlimit(knobs[M->first_axis]);
	bu_vls_printf( &cmd , "knob Y %f\n",
		       setting / 512.0 );
      }else{
	  if(-NOISE < knobs[M->first_axis] && knobs[M->first_axis] < NOISE &&
	     !absolute_slew[Y] )
	    knobs[M->first_axis] += M->axis_data[0] - knob_values[M->first_axis];
	  else
	    knobs[M->first_axis] = Glx_add_tol((int)(512.5 * absolute_slew[Y])) +
	      M->axis_data[0] - knob_values[M->first_axis];

	  setting = irlimit(knobs[M->first_axis]);
	bu_vls_printf( &cmd , "knob aY %f\n",
		       setting / 512.0 );
      }
      break;
    case DIAL6:
      if(mged_variables.adcflag){
	if(-NOISE < knobs[M->first_axis] && knobs[M->first_axis] < NOISE &&
	   !dv_xadc)
	  knobs[M->first_axis] += M->axis_data[0] - knob_values[M->first_axis];
	else
	  knobs[M->first_axis] = Glx_add_tol(dv_xadc) +
	    M->axis_data[0] - knob_values[M->first_axis];

	setting = irlimit(knobs[M->first_axis]);
	bu_vls_printf( &cmd , "knob xadc %d\n",
		      setting );
      }else{
	if(mged_variables.rateknobs){
	  if(-NOISE < knobs[M->first_axis] && knobs[M->first_axis] < NOISE &&
	     !rate_rotate[X] )
	    knobs[M->first_axis] += M->axis_data[0] - knob_values[M->first_axis];
	  else
	    knobs[M->first_axis] = Glx_add_tol((int)(512.5 * rate_rotate[X])) +
	      M->axis_data[0] - knob_values[M->first_axis];

	  setting = irlimit(knobs[M->first_axis]);
	  bu_vls_printf( &cmd , "knob x %f\n",
			 setting / 512.0 );
	}else{
	  if(-NOISE < knobs[M->first_axis] && knobs[M->first_axis] < NOISE &&
	     !absolute_rotate[X] )
	    knobs[M->first_axis] += M->axis_data[0] - knob_values[M->first_axis];
	  else
	    knobs[M->first_axis] = Glx_add_tol((int)(512.5 * absolute_rotate[X])) +
	      M->axis_data[0] - knob_values[M->first_axis];

	  setting = irlimit(knobs[M->first_axis]);
	  bu_vls_printf( &cmd , "knob ax %f\n",
			 setting / 512.0 );
	}
      }
      break;
    case DIAL7:
      if(mged_variables.rateknobs){
	if(-NOISE < knobs[M->first_axis] && knobs[M->first_axis] < NOISE &&
	   !rate_slew[X] )
	  knobs[M->first_axis] += M->axis_data[0] - knob_values[M->first_axis];
	else
	  knobs[M->first_axis] = Glx_add_tol((int)(512.5 * rate_slew[X])) +
	    M->axis_data[0] - knob_values[M->first_axis];

	setting = irlimit(knobs[M->first_axis]);
	bu_vls_printf( &cmd , "knob X %f\n",
		       setting / 512.0 );
      }else{
	if(-NOISE < knobs[M->first_axis] && knobs[M->first_axis] < NOISE &&
	   !absolute_slew[X] )
	  knobs[M->first_axis] += M->axis_data[0] - knob_values[M->first_axis];
	else
	  knobs[M->first_axis] = Glx_add_tol((int)(512.5 * absolute_slew[X])) +
	    M->axis_data[0] - knob_values[M->first_axis];

	setting = irlimit(knobs[M->first_axis]);
	bu_vls_printf( &cmd , "knob aX %f\n",
		       setting / 512.0 );
      }
      break;
    default:
      break;
    }

    /* Keep track of the knob values */
    knob_values[M->first_axis] = M->axis_data[0];
  }
#endif
#if IR_BUTTONS
  else if( eventPtr->type == devbuttonpress ){
    XDeviceButtonEvent *B;

    B = (XDeviceButtonEvent * ) eventPtr;

    if(B->button == 1){
      button0 = 1;
      goto end;
    }

    if(button0){
      glx_dbtext(label_button(bmap[B->button - 1]));
    }else if(B->button == 4){
      bu_vls_strcat(&cmd, "knob zero\n");
      set_knob_offset();
    }else
      bu_vls_printf(&cmd, "press %s\n",
		    label_button(bmap[B->button - 1]));
  }else if( eventPtr->type == devbuttonrelease ){
    XDeviceButtonEvent *B;

    B = (XDeviceButtonEvent * ) eventPtr;

    if(B->button == 1)
      button0 = 0;

    goto end;
  }
#endif
  else
    goto end;

  status = cmdline(&cmd, FALSE);
end:
  bu_vls_free(&cmd);
  curr_dm_list = save_dm_list;

  if(status == CMD_OK)
    return TCL_OK;

  return TCL_ERROR;
}


/* 
 *			I R _ L I G H T
 *
 * This function must keep both the light hardware, and the software
 * copy of the lights up to date.  Note that requests for light changes
 * may not actually cause the lights to be changed, depending on
 * whether the buttons are being used for "view" or "edit" functions
 * (although this is not done in the present code).
 */
void
Glx_light( cmd, func )
int cmd;
int func;			/* BE_ or BV_ function */
{
	register unsigned short bit;
#ifdef IR_BUTTONS
	/* Check for BE_ function not assigned to a button */
	if( (bit = invbmap[func]) == 0 && cmd != LIGHT_RESET )
		return;
	switch( cmd )  {
	case LIGHT_RESET:
		lights = 0;
		break;
	case LIGHT_ON:
		lights |= 1<<bit;
		break;
	case LIGHT_OFF:
		lights &= ~(1<<bit);
		break;
	}

	/* Update the lights box. */
#if !defined(__sgi)	/* This bombs, on early Irix 4.0 releases */
	setdblights( lights );
#endif
#endif
}

/*
 *			I R _ C V T V E C S
 *
 */
unsigned
Glx_cvtvecs( sp )
register struct solid *sp;
{
	return( 0 );	/* No "displaylist" consumed */
}

/*
 * Loads displaylist from storage[]
 */
unsigned
Glx_load( addr, count )
unsigned addr, count;
{
	return( 0 );		/* FLAG:  error */
}

void
Glx_statechange( a, b )
{
  if( mvars.debug ){
    struct bu_vls tmp_vls;

    bu_vls_init(&tmp_vls);
    bu_vls_printf(&tmp_vls, "statechange %d %d\n", a, b );
    Tcl_AppendResult(interp, bu_vls_addr(&tmp_vls), (char *)NULL);
    bu_vls_free(&tmp_vls);
  }

  /*
   *  Based upon new state, possibly do extra stuff,
   *  including enabling continuous tablet tracking,
   *  object highlighting
   */
 	switch( b )  {
	case ST_VIEW:
	  /* constant tracking OFF */
	  GLXdoMotion = 0;
	  break;
	case ST_S_PICK:
	case ST_O_PICK:
	case ST_O_PATH:
	case ST_S_VPICK:
	  /* constant tracking ON */
	  GLXdoMotion = 1;
	  break;
	case ST_O_EDIT:
	case ST_S_EDIT:
	  /* constant tracking OFF */
	  GLXdoMotion = 0;
	  break;
	default:
	  Tcl_AppendResult(interp, "Glx_statechange: unknown state ",
			   state_str[b], "\n", (char *)NULL);
	  break;
	}

	Glx_viewchange( DM_CHGV_REDO, SOLID_NULL );
}

void
Glx_viewchange( cmd, sp )
register int cmd;
register struct solid *sp;
{
  if( mvars.debug ){
    struct bu_vls tmp_vls;

    bu_vls_init(&tmp_vls);
    bu_vls_printf(&tmp_vls, "viewchange( %d, x%x )\n", cmd, sp );
    Tcl_AppendResult(interp, bu_vls_addr(&tmp_vls), (char *)NULL);
    bu_vls_free(&tmp_vls);
  }

  switch( cmd ){
  case DM_CHGV_ADD:
    break;
  case DM_CHGV_REDO:
    break;
  case DM_CHGV_DEL:
    break;
  case DM_CHGV_REPL:
    return;
  case DM_CHGV_ILLUM:
    break;
  }
}

void
Glx_debug(lvl)
{
	mvars.debug = lvl;
}

void
Glx_window(w)
int w[];
{
}


/*
 *  			I R _ C O L O R C H A N G E
 *  
 *  Go through the solid table, and allocate color map slots.
 *	8 bit system gives 4 or 8,
 *	24 bit system gives 12 or 24.
 */
void
Glx_colorchange()
{
	register int i;
	register int nramp;

	if( mvars.debug )
	  Tcl_AppendResult(interp, "colorchange\n", (char *)NULL);

	/* Program the builtin colors */
	glx_rgbtab[0].r=0; 
	glx_rgbtab[0].g=0; 
	glx_rgbtab[0].b=0;/* Black */
	glx_rgbtab[1].r=255; 
	glx_rgbtab[1].g=0; 
	glx_rgbtab[1].b=0;/* Red */
	glx_rgbtab[2].r=0; 
	glx_rgbtab[2].g=0; 
	glx_rgbtab[2].b=255;/* Blue */
	glx_rgbtab[3].r=255; 
	glx_rgbtab[3].g=255;
	glx_rgbtab[3].b=0;/*Yellow */
	glx_rgbtab[4].r = glx_rgbtab[4].g = glx_rgbtab[4].b = 255; /* White */
	slotsused = 5;

	if( mvars.rgb )  {
		if(mvars.cueing_on) {
			depthcue(1);
		} else {
			depthcue(0);
		}

		RGBcolor( (short)255, (short)255, (short)255 );

		/* apply region-id based colors to the solid table */
		color_soltab();

		return;
	}

	glx_nslots = getplanes();
	if(mvars.cueing_on && (glx_nslots < 7)) {
	  Tcl_AppendResult(interp, "Too few bitplanes: depthcueing disabled\n", (char *)NULL);
	  mvars.cueing_on = 0;
	}
	glx_nslots = 1<<glx_nslots;
	if( glx_nslots > NSLOTS )  glx_nslots = NSLOTS;
	if(mvars.cueing_on) {
		/* peel off reserved ones */
		glx_nslots = (glx_nslots - CMAP_BASE) / CMAP_RAMP_WIDTH;
		depthcue(1);
	} else {
		glx_nslots -= CMAP_BASE;	/* peel off the reserved entries */
		depthcue(0);
	}

	ovec = -1;	/* Invalidate the old colormap entry */

	/* apply region-id based colors to the solid table */
	color_soltab();

	/* Map the colors in the solid table to colormap indices */
	glx_colorit();

	for( i=0; i < slotsused; i++ )  {
		glx_gen_color( i, glx_rgbtab[i].r, glx_rgbtab[i].g, glx_rgbtab[i].b);
	}

	color(WHITE);	/* undefinied after gconfig() */
}

void
glx_colorit()
{
	register struct solid	*sp;
	register struct rgbtab *rgb;
	register int i;
	register int r,g,b;

	if( mvars.rgb )  return;

	FOR_ALL_SOLIDS( sp )  {
		r = sp->s_color[0];
		g = sp->s_color[1];
		b = sp->s_color[2];
		if( (r == 255 && g == 255 && b == 255) ||
		    (r == 0 && g == 0 && b == 0) )  {
			sp->s_dmindex = DM_WHITE;
			continue;
		}

		/* First, see if this matches an existing color map entry */
		rgb = glx_rgbtab;
		for( i = 0; i < slotsused; i++, rgb++ )  {
			if( rgb->r == r && rgb->g == g && rgb->b == b )  {
				sp->s_dmindex = i;
				goto next;
			}
		}

		/* If slots left, create a new color map entry, first-come basis */
		if( slotsused < glx_nslots )  {
			rgb = &glx_rgbtab[i=(slotsused++)];
			rgb->r = r;
			rgb->g = g;
			rgb->b = b;
			sp->s_dmindex = i;
			continue;
		}
		sp->s_dmindex = DM_YELLOW;	/* Default color */
next:		
		;
	}
}

/*
 *  I R _ D B T E X T
 *
 *  Used to call dbtext to print cute messages on the button box,
 *  if you have one.  Has to shift everythign to upper case
 *  since the box goes off the deep end with lower case.
 *
 *  Because not all SGI button boxes have text displays,
 *  this now needs to go to stdout in order to be useful.
 */

glx_dbtext(str)
register char *str;
{
#if IR_BUTTONS
	register i;
	char	buf[9];
	register char *cp;

	Tcl_AppendResult(interp, "dm-glx: You pressed Help key and '",
			 str, "'\n", (char *)NULL);
#else
	return;
#endif
}

#if IR_KNOBS
/*
 *			I R L I M I T
 *
 * Because the dials are so sensitive, setting them exactly to
 * zero is very difficult.  This function can be used to extend the
 * location of "zero" into "the dead zone".
 */
static int
irlimit(i)
int i;
{
	if( i > NOISE )
		return( i-NOISE );
	if( i < -NOISE )
		return( i+NOISE );
	return(0);
}

static int
Glx_add_tol(i)
int i;
{
  if( i > 0 )
    return( i + NOISE );
  if( i < 0 )
    return( i - NOISE );
  return(0);
}
#endif

/*			G E N _ C O L O R
 *
 *	maps a given color into the appropriate colormap entry
 *	for both depthcued and non-depthcued mode.  In depthcued mode,
 *	glx_gen_color also generates the colormap ramps.  Note that in depthcued
 *	mode, DM_BLACK uses map entry 0, and does not generate a ramp for it.
 *	Non depthcued mode skips the first CMAP_BASE colormap entries.
 *
 *	This routine is not called at all if mvars.rgb is set.
 */
glx_gen_color(c)
int c;
{
  return 0;
}

#ifdef never
/*
 *  Update the PF key lights.
 */
static int
kblights()
{
	char	lights;

	lights = (mvars.cueing_on)
	    | (mvars.zclipping_on << 1)
	    | (mvars.perspective_mode << 2)
	    | (mvars.zbuffer_on << 3);

	lampon(lights);
	lampoff(lights^0xf);
}
#endif

static void
establish_perspective()
{
  bu_vls_printf( &dm_values.dv_string,
		"set perspective %d\n",
		mvars.perspective_mode ?
		perspective_table[perspective_angle] :
		-1 );
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
  if(mvars.dummy_perspective > 0)
    perspective_angle = mvars.dummy_perspective <= 4 ? mvars.dummy_perspective - 1: 3;
  else if (--perspective_angle < 0) /* toggle perspective matrix */
    perspective_angle = 3;

  if(mvars.perspective_mode)
    bu_vls_printf( &dm_values.dv_string,
		  "set perspective %d\n",
		  perspective_table[perspective_angle] );

  /*
     Just in case the "!" is used with the set command. This
     allows us to toggle through more than two values.
   */
  mvars.dummy_perspective = 1;

  dmaflag = 1;
}

static void
establish_zbuffer()
{
	if( mvars.zbuf == 0 )  {
	  Tcl_AppendResult(interp, "dm-4d: This machine has no Zbuffer to enable\n",
			   (char *)NULL);
	  mvars.zbuffer_on = 0;
	}
	zbuffer( mvars.zbuffer_on );
	if( mvars.zbuffer_on)  {
	  /* Set screen coords of near and far clipping planes */
	  lsetdepth(mvars.min_scr_z, mvars.max_scr_z);
	}
	dmaflag = 1;
}

static void
establish_am()
{
  return;
}

glx_clear_to_black()
{
	/* Re-enable the full viewport */
	viewport(0, winx_size, 0, winy_size);

	if( mvars.zbuffer_on )  {
		zfunction( ZF_LEQUAL );
		if( mvars.rgb )  {
			czclear( 0x000000, mvars.max_scr_z );
		} else {
			czclear( BLACK, mvars.max_scr_z );
		}
	} else {
		if( mvars.rgb )  {
			RGBcolor( (short)0, (short)0, (short)0 );
		} else {
			color(BLACK);
		}
		clear();
	}
}

#if 0
/* Handy fakeouts when we don't want to link with -lmpc */
usinit()	{ 
	bu_log("usinit\n"); 
}
usnewlock()	{ 
	bu_log("usnewlock\n"); 
}
taskcreate()	{ 
	bu_log("taskcreate\n"); 
}
#endif

/*
 *  The struct_parse will change the value of the variable.
 *  Just implement it, here.
 */
static void
establish_lighting()
{
	if( !mvars.lighting_on )  {
		/* Turn it off */
		mmode(MVIEWING);
		lmbind(MATERIAL,0);
		lmbind(LMODEL,0);
		mmode(MSINGLE);
	} else {
		/* Turn it on */
		if( mvars.cueing_on )  {
			/* Has to be off for lighting */
			mvars.cueing_on = 0;
			Glx_colorchange();
		}

		mmode(MVIEWING);


		glx_make_materials();	/* Define material properties */

		lmbind(LMODEL, 2);	/* infinite */
		lmbind(LIGHT2,2);
		lmbind(LIGHT3,3);
		lmbind(LIGHT4,4);
		lmbind(LIGHT5,5);

		/* RGB color commands & lighting */
		lmcolor( LMC_COLOR );

		mmode(MSINGLE);
	}
	dmaflag = 1;
}

/*
 *  Some initial lighting model stuff
 *  Really, MGED needs to derrive it's lighting from the database,
 *  but for now, this hack will suffice.
 *
 *  For materials, the definitions are:
 *	ALPHA		opacity.  1.0=opaque
 *	AMBIENT		ambient reflectance of the material  0..1
 *	DIFFUSE		diffuse reflectance of the material  0..1
 *	SPECULAR	specular reflectance of the material  0..1
 *	EMISSION	emission color ???
 *	SHININESS	specular scattering exponent, integer 0..128
 */
static float material_default[] = {
	ALPHA,		1.0,
	AMBIENT,	0.2, 0.2, 0.2,
	DIFFUSE,	0.8, 0.8, 0.8,
	EMISSION,	0.0, 0.0, 0.0,
	SHININESS,	0.0,
	SPECULAR,	0.0, 0.0, 0.0,
	LMNULL   };

/* Something like the RT default phong material */
static float material_rtdefault[] = {
	ALPHA,		1.0,	
	AMBIENT,	0.2, 0.2, 0.2,	/* 0.4 in rt */
	DIFFUSE,	0.6, 0.6, 0.6,
	SPECULAR,	0.2, 0.2, 0.2,
	EMISSION,	0.0, 0.0, 0.0,
	SHININESS,	10.0,
	LMNULL   };

/* This was the "default" material in the demo */
static float material_xdefault[] = {
	AMBIENT, 0.35, 0.25,  0.1,
	DIFFUSE, 0.1, 0.5, 0.1,
	SPECULAR, 0.0, 0.0, 0.0,
	SHININESS, 5.0,
	LMNULL   };

static float mat_brass[] = {
	AMBIENT, 0.35, 0.25,  0.1,
	DIFFUSE, 0.65, 0.5, 0.35,
	SPECULAR, 0.0, 0.0, 0.0,
	SHININESS, 5.0,
	LMNULL   };

static float mat_shinybrass[] = {
	AMBIENT, 0.25, 0.15, 0.0,
	DIFFUSE, 0.65, 0.5, 0.35,
	SPECULAR, 0.9, 0.6, 0.0,
	SHININESS, 10.0,
	LMNULL   };

static float mat_pewter[] = {
	AMBIENT, 0.0, 0.0,  0.0,
	DIFFUSE, 0.6, 0.55 , 0.65,
	SPECULAR, 0.9, 0.9, 0.95,
	SHININESS, 10.0,
	LMNULL   };

static float mat_silver[] = {
	AMBIENT, 0.4, 0.4,  0.4,
	DIFFUSE, 0.3, 0.3, 0.3,
	SPECULAR, 0.9, 0.9, 0.95,
	SHININESS, 30.0,
	LMNULL   };

static float mat_gold[] = {
	AMBIENT, 0.4, 0.2, 0.0,
	DIFFUSE, 0.9, 0.5, 0.0,
	SPECULAR, 0.7, 0.7, 0.0,
	SHININESS, 10.0,
	LMNULL   };

static float mat_shinygold[] = {
	AMBIENT, 0.4, 0.2,  0.0,
	DIFFUSE, 0.9, 0.5, 0.0,
	SPECULAR, 0.9, 0.9, 0.0,
	SHININESS, 20.0,
	LMNULL   };

static float mat_plaster[] = {
	AMBIENT, 0.2, 0.2,  0.2,
	DIFFUSE, 0.95, 0.95, 0.95,
	SPECULAR, 0.0, 0.0, 0.0,
	SHININESS, 1.0,
	LMNULL   };

static float mat_redplastic[] = {
	AMBIENT, 0.3, 0.1, 0.1,
	DIFFUSE, 0.5, 0.1, 0.1,
	SPECULAR, 0.45, 0.45, 0.45,
	SHININESS, 30.0,
	LMNULL   };

static float mat_greenplastic[] = {
	AMBIENT, 0.1, 0.3, 0.1,
	DIFFUSE, 0.1, 0.5, 0.1,
	SPECULAR, 0.45, 0.45, 0.45,
	SHININESS, 30.0,
	LMNULL   };

static float mat_blueplastic[] = {
	AMBIENT, 0.1, 0.1, 0.3,
	DIFFUSE, 0.1, 0.1, 0.5,
	SPECULAR, 0.45, 0.45, 0.45,
	SHININESS, 30.0,
	LMNULL   };

static float mat_greenflat[] = {
	EMISSION,   0.0, 0.4, 0.0,
	AMBIENT,    0.0, 0.0, 0.0,
	DIFFUSE,    0.0, 0.0, 0.0,
	SPECULAR,   0.0, 0.6, 0.0,
	SHININESS, 10.0,
	LMNULL
};

static float mat_greenshiny[]= {
	EMISSION, 0.0, 0.4, 0.0,
	AMBIENT,  0.1, 0.25, 0.1,
	DIFFUSE,  0.5, 0.5, 0.5,
	SPECULAR,  0.25, 0.9, 0.25,
	SHININESS, 10.0,
	LMNULL
};

static float mat_blueflat[] = {
	EMISSION, 0.0, 0.0, 0.4,
	AMBIENT,  0.1, 0.25, 0.1,
	DIFFUSE,  0.0, 0.5, 0.5,
	SPECULAR,  0.0, 0.0, 0.9,
	SHININESS, 10.0,
	LMNULL
};

static float mat_blueshiny[] = {
	EMISSION, 0.0, 0.0, 0.6,
	AMBIENT,  0.1, 0.25, 0.5,
	DIFFUSE,  0.5, 0.5, 0.5,
	SPECULAR,  0.5, 0.0, 0.0,
	SHININESS, 10.0,
	LMNULL
};

static float mat_redflat[] = {
	EMISSION, 0.60, 0.0, 0.0,
	AMBIENT,  0.1, 0.25, 0.1,
	DIFFUSE,  0.5, 0.5, 0.5,
	SPECULAR,  0.5, 0.0, 0.0,
	SHININESS, 1.0,
	LMNULL
};

static float mat_redshiny[] = {
	EMISSION, 0.60, 0.0, 0.0,
	AMBIENT,  0.1, 0.25, 0.1,
	DIFFUSE,  0.5, 0.5, 0.5,
	SPECULAR,  0.5, 0.0, 0.0,
	SHININESS, 10.0,
	LMNULL
};

static float mat_beigeshiny[] = {
	EMISSION, 0.5, 0.5, 0.6,
	AMBIENT,  0.35, 0.35, 0.0,
	DIFFUSE,  0.5, 0.5, 0.0,
	SPECULAR,  0.5, 0.5, 0.0,
	SHININESS, 10.0,
	LMNULL
};

/*
 *  Meanings of the parameters:
 *	AMBIENT		ambient light associated with this source ???, 0..1
 *	LCOLOR		light color, 0..1
 *	POSITION	position of light.  w=0 for infinite lights
 */
static float default_light[] = {
	AMBIENT,	0.0, 0.0, 0.0, 
	LCOLOR,		1.0, 1.0, 1.0, 
	POSITION,	0.0, 0.0, 1.0, 0.0,
	LMNULL};


#if 1
# if 0
static float white_inf_light[] = {
	AMBIENT, 0.0, 0.0, 0.0, 
	LCOLOR,   0.70, 0.70, 0.70, 
	POSITION, 100.0, -200.0, 100.0, 0.0, 
	LMNULL};


static float red_inf_light[] = {
	AMBIENT, 0.0, 0.0, 0.0, 
	LCOLOR,   0.6, 0.1, 0.1, 
	POSITION, -100.0, -30.0, 100.0, 0.0, 
	LMNULL};

static float green_inf_light[] = {
	AMBIENT, 0.0, 0.0, 0.0, 
	LCOLOR,   0.1, 0.3, 0.1, 
	POSITION, 100.0, -20.0, 20.0, 0.0, 
	LMNULL};


static float blue_inf_light[] = {
	AMBIENT, 0.0, 0.0, 0.0, 
	LCOLOR,   0.1, 0.1, 0.3, 
	POSITION, 0.0, 100.0, -100.0, 0.0, 
	LMNULL};

static float white_local_light[] = {
	AMBIENT, 0.0, 1.0, 0.0, 
	LCOLOR,   0.75, 0.75, 0.75, 
	POSITION, 0.0, 10.0, 10.0, 5.0, 
	LMNULL};
# else
static float white_inf_light[] = {
	AMBIENT, 0.0, 0.0, 0.0, 
	LCOLOR,   0.70, 0.70, 0.70, 
	POSITION, 100.0, 200.0, 100.0, 0.0, 
	LMNULL};


static float red_inf_light[] = {
	AMBIENT, 0.0, 0.0, 0.0, 
	LCOLOR,   0.6, 0.1, 0.1, 
	POSITION, 100.0, 30.0, 100.0, 0.0, 
	LMNULL};

static float green_inf_light[] = {
	AMBIENT, 0.0, 0.0, 0.0, 
	LCOLOR,   0.1, 0.3, 0.1, 
	POSITION, -100.0, 20.0, 20.0, 0.0, 
	LMNULL};


static float blue_inf_light[] = {
	AMBIENT, 0.0, 0.0, 0.0, 
	LCOLOR,   0.1, 0.1, 0.3, 
	POSITION, 0.0, -100.0, -100.0, 0.0, 
	LMNULL};

static float white_local_light[] = {
	AMBIENT, 0.0, 1.0, 0.0, 
	LCOLOR,   0.75, 0.75, 0.75, 
	POSITION, 0.0, 10.0, 10.0, 5.0, 
	LMNULL};
# endif

#else
static float white_inf_light[] = {
	AMBIENT, 0.0, 0.0, 0.0, 
	LCOLOR,   0.70, 0.70, 0.70, 
	POSITION, 10.0, 50.0, 50.0, 0.0, 
	LMNULL};


static float red_inf_light[] = {
	AMBIENT, 0.0, 0.0, 0.0, 
	LCOLOR,   0.5, 0.1, 0.1, 
	POSITION, -100.0, 0.0, 0.0, 0.0, 
	LMNULL};

static float green_inf_light[] = {
	AMBIENT, 0.0, 0.0, 0.0, 
	LCOLOR,   0.1, 0.5, 0.1, 
	POSITION, 100.0, 50.0, 0.0, 0.0, 
	LMNULL};

static float blue_inf_light[] = {
	AMBIENT, 0.0, 0.0, 0.0, 
	LCOLOR,   0.1, 0.1, 0.5, 
	POSITION, 0.0, -50.0, 0.0, 0.0, 
	LMNULL};

static float orange_inf_light[] = {
	AMBIENT, 0.0, 0.0, 0.0, 
	LCOLOR,	0.35, 0.175, 0.0, 
	POSITION, -50.0, 50.0, 10.0, 0.0, 
	LMNULL};

static float white_local_light[] = {
	AMBIENT, 0.0, 0.0, 0.0, 
	LCOLOR,   0.75, 0.75, 0.75, 
	POSITION, 0.0, 10.0, 10.0, 5.0, 
	LMNULL};


#endif




/*
 *  Lighting model parameters
 *	AMBIENT		amount of ambient light present in the scene, 0..1
 *	ATTENUATION	fixed and variable attenuation factor, 0..1
 *	LOCALVIEWER	1=eye at (0,0,0), 0=eye at (0,0,+inf)
 */
static float	default_lmodel[] = {
	AMBIENT,	0.2,  0.2,  0.2,
	ATTENUATION,	1.0, 0.0, 
	LOCALVIEWER,	0.0, 
	LMNULL};

static float infinite[] = {
	AMBIENT, 0.3,  0.3, 0.3, 
	LOCALVIEWER, 0.0, 
	LMNULL};

static float local[] = {
	AMBIENT, 0.3,  0.3, 0.3, 
	LOCALVIEWER, 1.0, 
	ATTENUATION, 1.0, 0.0, 
	LMNULL};


glx_make_materials()
{
	/* define material properties */
	lmdef (DEFMATERIAL, 1, 0, material_default);

	lmdef (DEFMATERIAL, 2, 0, mat_brass);
	lmdef (DEFMATERIAL, 3, 0, mat_shinybrass);
	lmdef (DEFMATERIAL, 4, 0, mat_pewter);
	lmdef (DEFMATERIAL, 5, 0, mat_silver);
	lmdef (DEFMATERIAL, 6, 0, mat_gold);
	lmdef (DEFMATERIAL, 7, 0, mat_shinygold);
	lmdef (DEFMATERIAL, 8, 0, mat_plaster);
	lmdef (DEFMATERIAL, 9, 0, mat_redplastic);
	lmdef (DEFMATERIAL, 10, 0, mat_greenplastic);
	lmdef (DEFMATERIAL, 11, 0, mat_blueplastic);

	lmdef (DEFMATERIAL, 12, 0, mat_greenflat);
	lmdef (DEFMATERIAL, 13, 0, mat_greenshiny);

	lmdef (DEFMATERIAL, 14, 0, mat_blueflat);
	lmdef (DEFMATERIAL, 15, 0, mat_blueshiny);

	lmdef (DEFMATERIAL, 16, 0, mat_redflat);
	lmdef (DEFMATERIAL, 17, 0, mat_redshiny);

	lmdef (DEFMATERIAL, 18, 0, mat_beigeshiny);

	lmdef( DEFMATERIAL, 19, 0, material_xdefault );
	lmdef( DEFMATERIAL, 20, 0, material_rtdefault );

	/*    lmdef (DEFLIGHT, 1, 0, default_light); */
	lmdef (DEFLIGHT, 4, 0, green_inf_light);
	lmdef (DEFLIGHT, 2, 0, white_inf_light);
	lmdef (DEFLIGHT, 3, 0, red_inf_light);
	lmdef (DEFLIGHT, 4, 0, green_inf_light);
	lmdef (DEFLIGHT, 5, 0, blue_inf_light);
	/*    lmdef (DEFLIGHT, 6, 0, orange_inf_light); */
	lmdef (DEFLIGHT, 7, 0, white_local_light);

	lmdef (DEFLMODEL, 1, 0, default_lmodel);
	lmdef (DEFLMODEL, 2, 0, infinite);
	lmdef (DEFLMODEL, 3, 0, local);


}

/*
 *  Check to see if setmonitor(STR_RECT) will work.
 *  Returns -
 *	> 0	If stereo is available
 *	0	If not
 */
int
glx_has_stereo()
{
#if !defined(__sgi) && !defined(__mips)
	/* IRIX 3 systems, test to see if DER1_STEREO bit is
	 * read/write (no hardware underneath), or
	 * read only (hardware underneath, which can't be read back.
	 */
	int	rw_orig, rw1, rw2;

	rw_orig = getvideo(DE_R1);
	rw1 = rw_orig ^ DER1_STEREO;	/* Toggle the bit */
	setvideo(DE_R1, rw1);
	rw2 = getvideo(DE_R1);
	if( rw1 != rw2 )  {
		setvideo(DE_R1, rw_orig);/* Restore original state */
		return 1;		/* Has stereo */
	}
	rw1 = rw1 ^ DER1_STEREO;	/* Toggle the bit, again */
	setvideo(DE_R1, rw1);
	rw2 = getvideo(DE_R1);
	if( rw1 != rw2 )  {
		setvideo(DE_R1, rw_orig);/* Restore original state */
		return 1;		/* Has stereo */
	}
	setvideo(DE_R1, rw_orig);	/* Restore original state */
	return 0;			/* Does not have stereo */
#else
	/* IRIX 4 systems */
	return getgdesc(GD_STEREO);
#endif
}

/*
 *			I R _ D M
 * 
 *  Implement display-manager specific commands, from MGED "dm" command.
 */
int
Glx_dm(argc, argv)
int	argc;
char	**argv;
{
  struct bu_vls	vls;
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
      bu_struct_print("dm_4d internal variables", Glx_vparse, (CONST char *)&mvars );
    } else if( argc == 2 ) {
      bu_vls_struct_item_named( &vls, Glx_vparse, argv[1], (CONST char *)&mvars, ',');
      bu_log( "%s\n", bu_vls_addr(&vls) );
    } else {
      bu_vls_printf( &vls, "%s=\"", argv[1] );
      bu_vls_from_argv( &vls, argc-2, argv+2 );
      bu_vls_putc( &vls, '\"' );
      bu_struct_parse( &vls, Glx_vparse, (char *)&mvars);
    }

    bu_vls_free(&vls);

    stop_catching_output(&tmp_vls);
    Tcl_AppendResult(interp, bu_vls_addr(&tmp_vls), (char *)NULL);
    bu_vls_free(&tmp_vls);
    return TCL_OK;
  }

  if( !strcmp( argv[0], "m" )){
    scroll_active = 0;

    if( argc < 5){
      Tcl_AppendResult(interp, "dm m: need more parameters\n",
		       "m button 1|0 xpos ypos\n", (char *)NULL);
      return TCL_ERROR;
    }

    /* This assumes a 3-button mouse */
    switch(*argv[1]){
    case '1':
      mb_mask = Button1Mask;
      break;
    case '2':
      mb_mask = Button2Mask;
      break;
    case '3':
      mb_mask = Button3Mask;
      break;
    default:
      Tcl_AppendResult(interp, "dm m: bad button value - ", argv[1], "\n", (char *)NULL);
      return TCL_ERROR;
    }

    bu_vls_init(&vls);
    bu_vls_printf(&vls, "M %s %d %d\n", argv[2],
		  irisX2ged(atoi(argv[3])), irisY2ged(atoi(argv[4])));
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
		       "am <r|t|z> 1|0 xpos ypos\n", (char *)NULL);
      return TCL_ERROR;
    }

    buttonpress = atoi(argv[2]);
    omx = atoi(argv[3]);
    omy = atoi(argv[4]);

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
	  fx = (omx/(fastf_t)winx_size - 0.5) * 2;
	  fy = (0.5 - omy/(fastf_t)winy_size) * 2;
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
			 "am <r|t|z> 1|0 xpos ypos\n", (char *)NULL);
	return TCL_ERROR;
      }
    }else
      am_mode = ALT_MOUSE_MODE_IDLE;

    return status;
  }

  Tcl_AppendResult(interp, "dm: bad command - ", argv[0], "\n", (char *)NULL);
  return TCL_ERROR;
}

static void
set_knob_offset()
{
  int i;

  for(i = 0; i < 8; ++i)
    knobs[i] = 0;
}

static void
glx_var_init()
{
  dm_vars = bu_malloc(sizeof(struct glx_vars), "glx_var_init: glx_vars");
  bzero((void *)dm_vars, sizeof(struct glx_vars));
  devmotionnotify = LASTEvent;
  devbuttonpress = LASTEvent;
  devbuttonrelease = LASTEvent;
  ((struct glx_vars *)dm_vars)->dm_list = curr_dm_list;
  perspective_angle = 3;

  /* initialize the modifiable variables */
  mvars.cueing_on = 1;          /* Depth cueing flag - for colormap work */
  mvars.zclipping_on = 1;       /* Z Clipping flag */
  mvars.zbuffer_on = 1;         /* Hardware Z buffer is on */
  mvars.linewidth = 1;      /* Line drawing width */
  mvars.dummy_perspective = 1;
}


static struct dm_list *
get_dm_list(window)
Window window;
{
  register struct glx_vars *p;

  for( RT_LIST_FOR(p, glx_vars, &head_glx_vars.l) ){
    if(window == p->_win){
      GLXwinset(p->_dpy, p->_win);
      return p->dm_list;
    }
  }

  return DM_LIST_NULL;
}

static unsigned long
extract_value(buffer, mode, conf)
int buffer;
int mode;
GLXconfig *conf;
{
  int i;

  for (i = 0; conf[i].buffer; i++)
    if (conf[i].buffer == buffer && conf[i].mode == mode)
      return conf[i].arg;

  return 0;
}

/* Extract X visual information */
static XVisualInfo*
extract_visual(buffer, conf)
int buffer;
GLXconfig *conf;
{
  XVisualInfo template, *v;
  int n;

  template.screen = Tk_ScreenNumber(xtkwin);
  template.visualid = extract_value(buffer, GLX_VISUAL, conf);

  return XGetVisualInfo(dpy, VisualScreenMask|VisualIDMask, &template, &n);
}

/* 
 * Fill the configuration structure with the appropriately
 * created window
 */
static void
set_window(buffer, _win, conf)
int buffer;
Window _win;
GLXconfig *conf;
{
  int i;

  for (i = 0; conf[i].buffer; i++)
    if (conf[i].buffer == buffer && conf[i].mode == GLX_WINDOW)
      conf[i].arg = _win;
}
