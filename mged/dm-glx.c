/*
   Just a note:

   DM-4D.C currently uses the commands below. These particular commands
   should not be used in mixed mode programming.

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
 *  This version for the SGI 4-D Iris, both regular and GT versions.
 *
 *  Uses library -lgl
 *
 *  Authors -
 *	Paul R. Stay
 *	Michael John Muuss
 *	Robert J. Reschly
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

/* Experimental */
#define MIXED_MODE 1
#define TRY_PIPES 1

#include "conf.h"

/* Forwards compat with IRIX 5.0.1 */
#define class	inv_class	/* Map Irix 4 name into Irix 5 name */
#define type	inv_type	/* Map Irix 4 name into Irix 5 name */
#define state	inv_state	/* Map Irix 4 name into Irix 5 name */

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
#include "tkGLX.h"

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
#include "raytrace.h"
#include "./ged.h"
#include "./dm.h"
#include "externs.h"
#include "./solid.h"

#define YSTEREO		491	/* subfield height, in scanlines */
#define YOFFSET_LEFT	532	/* YSTEREO + YBLANK ? */

#if TRY_PIPES
extern int ged_pipe[];
#endif

extern Tcl_Interp *interp;
extern Tk_Window tkwin;
extern inventory_t	*getinvent();
extern void (*knob_offset_hook)();
void set_knob_offset();

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
#if MIXED_MODE
int   Glx_loadGLX();
#ifdef USE_PROTOTYPES
Tk_GenericProc Glx_checkevents;
#else
int Glxcheckevents();
#endif
#else
void    Glx_checkevents();
#endif

/*
 * These variables are visible and modifiable via a "dm set" command.
 */
static int	cueing_on = 1;		/* Depth cueing flag - for colormap work */
static int	zclipping_on = 1;	/* Z Clipping flag */
static int	zbuffer_on = 1;		/* Hardware Z buffer is on */
static int	lighting_on = 0;	/* Lighting model on */
static int	glx_debug;		/* 2 for basic, 3 for full */
static int	no_faceplate = 0;	/* Don't draw faceplate */
static int	glx_linewidth = 1;	/* Line drawing width */
static int      dummy_perspective = 1;
static int      perspective_mode = 0;	/* Perspective flag */
/*
 * These are derived from the hardware inventory -- user can change them,
 * but the results may not be pleasing.  Mostly, this allows them to be seen.
 */
static int	glx_is_gt;		/* 0 for non-GT machines */
static int	glx_has_zbuf;		/* 0 if no Z buffer */
static int	glx_has_rgb;		/* 0 if mapped mode must be used */
static int	glx_has_doublebuffer;	/* 0 if singlebuffer mode must be used */
static int	min_scr_z;		/* based on getgdesc(GD_ZMIN) */
static int	max_scr_z;		/* based on getgdesc(GD_ZMAX) */
/* End modifiable variables */

static int irsetup();
GLXconfig glxConfig [] = {
  { GLX_NORMAL, GLX_DOUBLE, TRUE },
  { GLX_NORMAL, GLX_RGB, TRUE },
  { GLX_NORMAL, GLX_ZSIZE, GLX_NOCONFIG },
  { 0, 0, 0 }
};

#ifdef MULTI_ATTACH
#define dpy (((struct glx_vars *)dm_vars)->_dpy)
#define win (((struct glx_vars *)dm_vars)->_win)
#define xtkwin (((struct glx_vars *)dm_vars)->_xtkwin)
#define win_l (((struct glx_vars *)dm_vars)->_win_l)
#define win_b (((struct glx_vars *)dm_vars)->_win_b)
#define win_r (((struct glx_vars *)dm_vars)->_win_r)
#define win_t (((struct glx_vars *)dm_vars)->_win_t)
#define winx_size (((struct glx_vars *)dm_vars)->_winx_size)
#define winy_size (((struct glx_vars *)dm_vars)->_winy_size)
#define perspective_angle (((struct glx_vars *)dm_vars)->_perspective_angle)
#define devmotionnotify (((struct glx_vars *)dm_vars)->_devmotionnotify)
#define devbuttonpress (((struct glx_vars *)dm_vars)->_devbuttonpress)
#define devbuttonrelease (((struct glx_vars *)dm_vars)->_devbuttonrelease)
#define knobs (((struct glx_vars *)dm_vars)->_knobs)
#define knobs_offset (((struct glx_vars *)dm_vars)->_knobs_offset)
#define stereo_is_on (((struct glx_vars *)dm_vars)->_stereo_is_on)
#define ref (((struct glx_vars *)dm_vars)->_ref)

struct glx_vars {
  Display *_dpy;
  Window _win;
  Tk_Window _xtkwin;
  long _win_l, _win_b, _wind_r, _win_t;
  long _winx_size, _winy_size;
  int _perspective_angle;
  int _devmotionnotify;
  int _devbuttonpress;
  int _devbuttonrelease;
  int _knobs[8];
  int _knobs_offset[8];
  int _stereo_is_on;
  char *_ref;
};

void glx_var_init();
static struct dm_list *get_dm_list();
#else
static Tk_Window xtkwin;
static Display  *dpy;
static Window   win;
static long win_l, win_b, win_r, win_t;
static long winx_size, winy_size;
static int perspective_angle =3;	/* Angle of perspective */
static int devmotionnotify = LASTEvent;
static int devbuttonpress = LASTEvent;
static int devbuttonrelease = LASTEvent;
static int knobs[8] = {0, 0, 0, 0, 0, 0, 0, 0};
static int knobs_offset[8] = {0, 0, 0, 0, 0, 0, 0, 0};
static int	stereo_is_on = 0;
#endif

static int	glx_fd;			/* GL file descriptor to select() on */
static CONST char glx_title[] = "BRL MGED";
static int perspective_table[] = { 
	30, 45, 60, 90 };
static int ovec = -1;		/* Old color map entry number */
static int kblights();
static double	xlim_view = 1.0;	/* args for ortho() */
static double	ylim_view = 1.0;
static mat_t	aspect_corr;

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
	"glx", "SGI 4d",
	0,			/* mem map */
	Glx_dm
};

extern struct device_values dm_values;	/* values read from devices */

static void     establish_perspective();
static void     set_perspective();
static void	establish_lighting();
static void	establish_zbuffer();


static void
refresh_hook()
{
	dmaflag = 1;
}
struct structparse Glx_vparse[] = {
	{"%d",  1, "depthcue",		(int)&cueing_on,	Glx_colorchange },
	{"%d",  1, "zclip",		(int)&zclipping_on,	refresh_hook },
	{"%d",  1, "zbuffer",		(int)&zbuffer_on,	establish_zbuffer },
	{"%d",  1, "lighting",		(int)&lighting_on,	establish_lighting },
	{"%d",  1, "perspective",       (int)&perspective_mode, establish_perspective },
	{"%d",  1, "set_perspective",(int)&dummy_perspective,  set_perspective },
	{"%d",  1, "no_faceplate",	(int)&no_faceplate,	refresh_hook },
	{"%d",  1, "has_zbuf",		(int)&glx_has_zbuf,	refresh_hook },
	{"%d",  1, "has_rgb",		(int)&glx_has_rgb,	Glx_colorchange },
	{"%d",  1, "has_doublebuffer",	(int)&glx_has_doublebuffer, refresh_hook },
	{"%d",  1, "min_scr_z",		(int)&min_scr_z,	refresh_hook },
	{"%d",  1, "max_scr_z",		(int)&max_scr_z,	refresh_hook },
	{"%d",  1, "debug",		(int)&glx_debug,		FUNC_NULL },
	{"%d",  1, "linewidth",		(int)&glx_linewidth,	refresh_hook },
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
#if MIXED_MODE
	x = (x/(double)winx_size - 0.5) * 4095;
#else
	if( x <= win_l )  return(-2048);
	if( x >= win_r )  return(2047);

	x -= win_l;
	x = ( x / (double)winx_size)*4096.0;
	x -= 2048;
#endif

	return(x);
}

static int
irisY2ged(y)
register int y;
{
#if MIXED_MODE
	y = (0.5 - y/(double)winy_size) * 4095;
#else
	if( y <= win_b )  return(-2048);
	if( y >= win_t )  return(2047);

	y -= win_b;
	if( stereo_is_on )  y = (y%512)<<1;
	y = ( y / (double)winy_size)*4096.0;
	y -= 2048;
#endif

	return(y);
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
#if MIXED_MODE
	XWindowAttributes xwa;
#endif

	xlim_view = 1.0;
	ylim_view = 1.0;
	mat_idn(aspect_corr);

#if MIXED_MODE
	XGetWindowAttributes( dpy, win, &xwa );
	winx_size = xwa.width;
	winy_size = xwa.height;
#else
	getsize( &winx_size, &winy_size);
	getorigin( &win_l, & win_b );

	win_r = win_l + winx_size;
	win_t = win_b + winy_size;
#endif
	/* Write enable all the bloody bits after resize! */
	viewport(0, winx_size, 0, winy_size);

	if( glx_has_zbuf ) establish_zbuffer();
	establish_lighting();
	
	if( glx_has_doublebuffer)
	{
		/* Clear out image from windows underneath */
		frontbuffer(1);
		glx_clear_to_black();
		frontbuffer(0);
		glx_clear_to_black();
	} else
		glx_clear_to_black();

#if MIXED_MODE
#else
	switch( getmonitor() )  {
	default:
		break;
	case NTSC:
		/* Only use the central square part, due to overscan */
		npix = YMAX170-30;
		winx_size = npix * 4 / 3;	/* NTSC aspect ratio */
		winy_size = npix;
		win_l = (XMAX170 - winx_size)/2;
		win_r = win_l + winx_size;
		win_b = (YMAX170-winy_size)/2;
		win_t = win_b + winy_size;

		if( no_faceplate )  {
			/* Use the whole screen, for VR & visualization */
			viewport( 0, XMAX170, 0, YMAX170 );
		} else {
			/* Only use the central (square) faceplate area */
			/*
			 * XXX Does this viewport() call do anything?  (Write enable pixels maybe?)
			 * XXX (1) the first frame (only) is shrunken oddly in X, and
			 * XXX (2) the drawing overflows the boundaries.
			 * XXX Perhaps XY clipping could be used?
			 * At least the aspect ratio is right!
			 */
			/* XXXXX See page 8-9 in the manual.
			 * XXX The best way to do the masking in NTSC mode is to
			 * XXX just set a Z-buffer write mask of 0 on the parts we don't
			 * XXX want to have written.
			 */
			viewport( (XMAX170 - npix)/2, npix + (XMAX170 - npix)/2,
			    (YMAX170-npix)/2, npix + (YMAX170-npix)/2 );
		}
		/* Aspect ratio correction is needed either way */
		xlim_view = XMAX170 / (double)YMAX170;
		ylim_view = 1;	/* YMAX170 / YMAX170 */

		glx_linewidth = 3;
		blanktime(0);	/* don't screensave while recording video! */
		break;
	case PAL:
		/* Only use the central square part */
		npix = YMAXPAL-30;
		winx_size = npix;	/* What is PAL aspect ratio? */
		winy_size = npix;
		win_l = (XMAXPAL - winx_size)/2;
		win_r = win_l + winx_size;
		;
		win_b = (YMAXPAL-winy_size)/2;
		win_t = win_b + winy_size;
		viewport( (XMAXPAL - npix)/2, npix + (XMAXPAL - npix)/2,
		    (YMAXPAL-npix)/2, npix + (YMAXPAL-npix)/2 );
		glx_linewidth = 3;
		blanktime(0);	/* don't screensave while recording video! */
		break;
	}
#endif

	ortho( -xlim_view, xlim_view, -ylim_view, ylim_view, -1.0, 1.0 );
	/* The ortho() call really just makes this matrix: */
	aspect_corr[0] = 1/xlim_view;
	aspect_corr[5] = 1/ylim_view;
}

#define CMAP_BASE	32
#define CMAP_RAMP_WIDTH	16
#define MAP_ENTRY(x)	((cueing_on) ? \
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
        char	line[82];
        char	hostname[80];
	char	display[82];
	char	*envp;

	glx_var_init();
	/* get or create the default display */
	if( (envp = getenv("DISPLAY")) == NULL ) {
		/* Env not set, use local host */
		gethostname( hostname, 80 );
		hostname[79] = '\0';
		(void)sprintf( display, "%s:0", hostname );
		envp = display;
	}

	rt_log("X Display [%s]? ", envp );
	(void)fgets( line, sizeof(line), stdin );
	line[strlen(line)-1] = '\0';		/* remove newline */
	if( feof(stdin) )  quit();
	if( line[0] != '\0' ) {
		if( irsetup(line) ) {
			return(1);		/* BAD */
		}
	} else {
		if( irsetup(envp) ) {
			return(1);	/* BAD */
		}
	}

	/* Ignore the old scrollbars and menus */
	ignore_scroll_and_menu = 1;

	knob_offset_hook = set_knob_offset;

	return(0);			/* OK */
}

static int
irsetup( name )
char *name;
{
  register int	i;
  static int ref_count = 0;
  char tmp_str[64];
  Matrix		m;
  inventory_t	*inv;
  int		win_size=1000;
  int		win_o_x=272;
  int		win_o_y=12;
  struct rt_vls str;
  int j, k;
  int ndevices;
  int nclass = 0;
  XDeviceInfoPtr olist, list;
  XDevice *dev;
  XEventClass e_class[15];
  XInputClassInfo *cip;
  XAnyClassPtr any;
  Display *tmp_dpy;

  sprintf(tmp_str, "%s%d", "mged", ref_count++);
  ref = strdup(tmp_str);

  rt_vls_init(&pathName);
  rt_vls_printf(&pathName, ".%s.glx", ref);

	  
	rt_vls_init(&str);
	rt_vls_printf(&str, "loadtk %s\n", name);

	if(tkwin == NULL){
	  if(cmdline(&str, FALSE) == CMD_BAD){
	    rt_vls_free(&str);
	    return -1;
	  }
	}
	
	(void)TkGLX_Init(interp, tkwin);

	/* Invoke script to create button and key bindings */
	if( Glx_loadGLX() )
	  return -1;

#if 0
	dpy = Tk_Display(tkwin);
	winx_size = DisplayWidth(dpy, Tk_ScreenNumber(tkwin)) - 20;
	winy_size = DisplayHeight(dpy, Tk_ScreenNumber(tkwin)) - 20;
#else
	if((tmp_dpy = XOpenDisplay(name)) == NULL)
	  return -1;

	winx_size = DisplayWidth(tmp_dpy, DefaultScreen(tmp_dpy)) - 20;
	winy_size = DisplayHeight(tmp_dpy, DefaultScreen(tmp_dpy)) - 20;

	XCloseDisplay(tmp_dpy);
#endif
	if(winx_size > winy_size)
	  winx_size = winy_size;
	else
	  winy_size = winx_size;

	rt_vls_strcpy(&str, "create_glx ");
	rt_vls_printf(&str, "%s .%s glx %s %d %d true true\n", name,
		      ref, ref, winx_size, winy_size);
	rt_vls_printf(&str, "bindem .%s\n", ref);
#if 0
	rt_vls_printf(&str, "pack .%s -expand 1 -fill both\n", ref);
#endif
	if(cmdline(&str, FALSE) == CMD_BAD){
	  rt_vls_free(&str);
	  return -1;
	}

	rt_vls_free(&str);

	if(TkGLXwin_RefExists(ref)){
	  xtkwin = TkGLXwin_RefGetTkwin(ref);
	  if(xtkwin == NULL)
	    return -1;

	  dpy = Tk_Display(xtkwin);
	}else{
	  rt_log("Glx_open: ref - %s doesn't exist!!!\n", ref);
	  return -1;
	}

	/* Do this now to force a GLXlink */
	Tk_MapWindow(xtkwin);

	Tk_MakeWindowExist(xtkwin);
	win = Tk_WindowId(xtkwin);

	glx_is_gt = 1;

	{
	  GLXconfig *glx_config, *p;

	  glx_config = TkGLXwin_RefGetConfig(ref);
	
	  for(p = glx_config; p->buffer; ++p){
	    switch(p->buffer){
	    case GLX_NORMAL:
	      switch(p->mode){
	      case GLX_ZSIZE:
		if(p->arg)
		  glx_has_zbuf = 1;
		else
		  glx_has_zbuf = 0;

		break;
	      case GLX_RGB:
		if(p->arg)
		  glx_has_rgb = 1;
		else
		  glx_has_rgb = 0;

		break;
	      case GLX_DOUBLE:
		if(p->arg)
		  glx_has_doublebuffer = 1;
		else
		  glx_has_doublebuffer = 0;

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
	}

#if 0
	/* Start out with the usual window */
/*XXX Not supposed to be using this guy in mixed mode. */
	foreground();
#endif
	
	if (mged_variables.sgi_win_size > 0)
		win_size = mged_variables.sgi_win_size;

	if (mged_variables.sgi_win_origin[0] != 0)
		win_o_x = mged_variables.sgi_win_origin[0];

	if (mged_variables.sgi_win_origin[1] != 0)
		win_o_y = mged_variables.sgi_win_origin[1];
#if 0
	prefposition( win_o_x, win_o_x+win_size, win_o_y, win_o_y+win_size);
#else
#endif
#if 0
	keepaspect(1,1);	/* enforce 1:1 aspect ratio */
#endif
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
#if 1
	min_scr_z = getgdesc(GD_ZMIN)+15;
	max_scr_z = getgdesc(GD_ZMAX)-15;
#else
	min_scr_z = 0;
	max_scr_z = 0;
#endif

	Glx_configure_window_shape();

	/* Line style 0 is solid.  Program line style 1 as dot-dashed */
	deflinestyle( 1, 0xCF33 );
	setlinestyle( 0 );

/* Take a look at the available input devices */
	olist = list = (XDeviceInfoPtr) XListInputDevices (dpy, &ndevices);

	/* IRIX 4.0.5 bug workaround */
	if( list == (XDeviceInfoPtr)NULL ||
	   list == (XDeviceInfoPtr)1 )  goto Done;

	for(j = 0; j < ndevices; ++j, list++){
	  if(list->use == IsXExtensionDevice){
	    if(!strcmp(list->name, "dial+buttons")){
	      if((dev = XOpenDevice(dpy, list->id)) == (XDevice *)NULL){
		rt_log("Glx_open: Couldn't open the dials+buttons\n");
		goto Done;
	      }

	      for(cip = dev->classes, k = 0; k < dev->num_classes;
		  ++k, ++cip){
		switch(cip->input_class){
		case ButtonClass:
		  DeviceButtonPress(dev, devbuttonpress, e_class[nclass]);
		  ++nclass;
		  DeviceButtonRelease(dev, devbuttonrelease, e_class[nclass]);
		  ++nclass;
		  break;
		case ValuatorClass:
		  DeviceMotionNotify(dev, devmotionnotify, e_class[nclass]);
		  ++nclass;
		  break;
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
	Tk_CreateGenericHandler(Glx_checkevents, (ClientData)NULL);
	XSelectInput(dpy, win, ExposureMask|ButtonPressMask|
		     KeyPressMask|StructureNotifyMask);

	return(0);
}

/*XXX Just experimenting */
int
Glx_loadGLX()
{
  FILE    *fp;
  struct rt_vls str;
  char *path;
  int     found;
  int bogus;

#define DM_GLX_ENVRC "MGED_DM_GLX_RCFILE"
#define DM_GLX_RCFILE "glxinit2.tk"

  found = 0;
  rt_vls_init( &str );

  if((path = getenv(DM_GLX_ENVRC)) != (char *)NULL ){
    if ((fp = fopen(path, "r")) != NULL ) {
      rt_vls_strcpy( &str, path );
      found = 1;
    }
  }

  if(!found){
    if( (path = getenv("HOME")) != (char *)NULL )  {
      rt_vls_strcpy( &str, path );
      rt_vls_strcat( &str, "/" );
      rt_vls_strcat( &str, DM_GLX_RCFILE );

      if( (fp = fopen(rt_vls_addr(&str), "r")) != NULL )
	found = 1;
    }
  }

  if( !found ) {
    if( (fp = fopen( DM_GLX_RCFILE, "r" )) != NULL )  {
      rt_vls_strcpy( &str, DM_GLX_RCFILE );
      found = 1;
    }
  }

/*XXX Temporary, so things will work without knowledge of the new environment
      variables */
  if( !found ) {
    rt_vls_strcpy( &str, "/m/cad/mged/");
    rt_vls_strcat( &str, DM_GLX_RCFILE);

    if( (fp = fopen(rt_vls_addr(&str), "r")) != NULL )
      found = 1;
  }

  if(!found){
    rt_vls_free(&str);
    return -1;
  }

  fclose( fp );

  if (Tcl_EvalFile( interp, rt_vls_addr(&str) ) == TCL_ERROR) {
    rt_log("Error reading %s: %s\n", DM_GLX_RCFILE, interp->result);
    rt_vls_free(&str);
    return -1;
  }

  rt_vls_free(&str);
  return 0;
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
	if(cueing_on) depthcue(0);

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

#if MIXED_MODE
	Tk_DestroyWindow(xtkwin);
#else
	if( getmonitor() != glx_oldmonitor )
		setmonitor(glx_oldmonitor);

	winclose(gr_id);
#endif

	/* Stop ignoring the old scrollbars and menus */
	ignore_scroll_and_menu = 0;

	knob_offset_hook = NULL;
	rt_free(dm_vars, "dm_vars");
	rt_vls_free(&pathName);
	free(ref);
}

/*
 *			I R _ P R O L O G
 *
 * Define the world, and include in it instances of all the
 * important things.  Most important of all is the object "faceplate",
 * which is built between dmr_normal() and dmr_epilog()
 * by dmr_puts and dmr_2d_line calls from adcursor() and dotitles().
 */
void
Glx_prolog()
{
  struct rt_vls str;

  rt_vls_init(&str);
  rt_vls_printf(&str, "%s winset\n", rt_vls_addr(&pathName));

  if(cmdline(&str, FALSE) == CMD_BAD){
    rt_vls_free(&str);
    rt_log("Glx_prolog: winset failed\n");
    return;
  }

  rt_vls_free(&str);

	if (glx_debug)
		rt_log( "Glx_prolog\n");
#if 0
	ortho2( -1.0,1.0, -1.0,1.0);	/* L R Bot Top */
#else
	ortho( -xlim_view, xlim_view, -ylim_view, ylim_view, -1.0, 1.0 );
#endif

	if( dmaflag && !glx_has_doublebuffer )
	{
		glx_clear_to_black();
		return;
	}
	linewidth(glx_linewidth);
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
	if (glx_debug)
		rt_log( "Glx_normal\n");

	if( glx_has_rgb )  {
		RGBcolor( (short)0, (short)0, (short)0 );
	} else {
		color(BLACK);
	}

#if 0
	ortho2( -1.0,1.0, -1.0,1.0);	/* L R Bot Top */
#else
	ortho( -xlim_view, xlim_view, -ylim_view, ylim_view, -1.0, 1.0 );
#endif
}

/*
 *			I R _ E P I L O G
 */
void
Glx_epilog()
{
	if (glx_debug)
		rt_log( "Glx_epilog\n");
	/*
	 * A Point, in the Center of the Screen.
	 * This is drawn last, to always come out on top.
	 */
	Glx_2d_line( 0, 0, 0, 0, 0 );
	/* End of faceplate */

	if(glx_has_doublebuffer )
	{
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

	if (glx_debug)
		rt_log( "Glx_newrot()\n");

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

	if( ! zclipping_on ) {
		mat_t	nozclip;

		mat_idn( nozclip );
		nozclip[10] = 1.0e-20;
		mat_mul( newm, nozclip, mat );
		mptr = newm;
	} else {
		mptr = mat;
	}
#if 0
	for(i= 0; i < 4; i++) {
		gtmat[0][i] = *(mptr++);
		gtmat[1][i] = *(mptr++);
		gtmat[2][i] = *(mptr++);
		gtmat[3][i] = *(mptr++);
	}
#else
	gtmat[0][0] = *(mptr++) * aspect_corr[0];
	gtmat[1][0] = *(mptr++) * aspect_corr[0];
	gtmat[2][0] = *(mptr++) * aspect_corr[0];
	gtmat[3][0] = *(mptr++) * aspect_corr[0];

	gtmat[0][1] = *(mptr++) * aspect_corr[5];
	gtmat[1][1] = *(mptr++) * aspect_corr[5];
	gtmat[2][1] = *(mptr++) * aspect_corr[5];
	gtmat[3][1] = *(mptr++) * aspect_corr[5];

	gtmat[0][2] = *(mptr++);
	gtmat[1][2] = *(mptr++);
	gtmat[2][2] = *(mptr++);
	gtmat[3][2] = *(mptr++);

	gtmat[0][3] = *(mptr++);
	gtmat[1][3] = *(mptr++);
	gtmat[2][3] = *(mptr++);
	gtmat[3][3] = *(mptr++);
#endif

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

	if (glx_debug)
		rt_log( "Glx_Object()\n");

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

	if( glx_has_rgb )  {
		register short	r, g, b;
		if( white )  {
			r = g = b = 230;
		} else {
			r = (short)sp->s_color[0];
			g = (short)sp->s_color[1];
			b = (short)sp->s_color[2];
		}
		if(cueing_on)  {
			lRGBrange(
			    r/10, g/10, b/10,
			    r, g, b,
			    min_scr_z, max_scr_z );
		} else
		if(lighting_on && glx_is_gt)
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
			if(cueing_on)  {
				lshaderange(nvec+1, nvec+1,
				    min_scr_z, max_scr_z );
			}
			color( nvec );
		} else {
			if( (nvec = MAP_ENTRY( sp->s_dmindex )) != ovec) {
				/* Use only the middle 14 to allow for roundoff...
				 * Pity the poor fool who has defined a black object.
				 * The code will use the "reserved" color map entries
				 * to display it when in depthcued mode.
				 */
				if(cueing_on)  {
					lshaderange(nvec+1, nvec+14,
					    min_scr_z, max_scr_z );
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
	if (glx_debug)
		rt_log( "Glx_update()\n");
	if( !dmaflag )
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
	if (glx_debug)
		rt_log( "Glx_puts()\n");
	cmov2( GED2IRIS(x), GED2IRIS(y));
	if( glx_has_rgb )  {
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

	if (glx_debug)
		rt_log( "Glx_2d_line()\n");
	if( glx_has_rgb )  {
		/* Yellow */
		if(cueing_on)  {
			lRGBrange(
			    255, 255, 0,
			    255, 255, 0,
			    min_scr_z, max_scr_z );
		}
		RGBcolor( (short)255, (short)255, (short) 0 );
	} else {
		if((nvec = MAP_ENTRY(DM_YELLOW)) != ovec) {
			if(cueing_on) lshaderange(nvec, nvec,
			    min_scr_z, max_scr_z );
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
	static int cnt;
	register int i;
	struct timeval	tv;
	fd_set		files;
	int		width;

#if MIXED_MODE
/* Don't need to do this because Glx_input never gets called anyway */
#else
	if (glx_debug)
		rt_log( "Glx_input()\n");
	if( (width = sysconf(_SC_OPEN_MAX)) <= 0 )
		width = 32;
	files = *input;		/* save, for restore on each loop */
	FD_SET( glx_fd, &files );

	/*
	 * Check for input on the keyboard or on the polled registers.
	 *
	 * Suspend execution until either
	 *  1)  User types a full line
	 *  2)  A change in peripheral status occurs
	 *  3)  The timelimit on SELECT has expired
	 *
	 * If a RATE operation is in progress (zoom, rotate, slew)
	 * in which the peripherals (rate setting) may not be changed,
	 * but we still have to update the display,
	 * do not suspend execution.
	 */
	do  {
		cnt = 0;
		i = qtest();
		if( i != 0 )  {
			FD_ZERO( input );
			FD_SET( glx_fd, input );
			break;		/* There is device input */
		}
		*input = files;

		tv.tv_sec = 0;
		if( noblock )  {
			tv.tv_usec = 0;
		}  else {
			/* 1/20th second */
			tv.tv_usec = 50000;
		}
		cnt = select( width, input, (fd_set *)0,  (fd_set *)0, &tv );
		if( cnt < 0 )  {
			perror("dm-4d.c/select");
			break;
		}
		if( noblock )  break;
		for( i=0; i<width; i++ )  {
			if( FD_ISSET(i, input) )  goto input_waiting;
		}
	} while( noblock == 0 );

input_waiting:
	/*
	 * Set device interface structure for GED to "rest" state.
	 * First, process any messages that came in.
	 */

	if( FD_ISSET( glx_fd, input ) )
		Glx_checkevents();
#endif
	return;
}


/*
   This routine does not get key events. The key events are
   being processed via the TCL/TK bind command. Eventually, I'd also
   like to do the dials+buttons that way. That would leave this
   routine to handle only events like Expose and ConfigureNotify.
*/
int
Glx_checkevents(clientData, eventPtr)
ClientData clientData;
XEvent *eventPtr;
{
  XEvent event;
  static int button0  = 0;   /*  State of button 0 */
  static int knobs_during_help[8] = {0, 0, 0, 0, 0, 0, 0, 0};
#ifdef MULTI_ATTACH
  register struct dm_list *save_dm_list;
  struct rt_vls cmd;

/*XXX still drawing too much!!!
i.e. drawing 2 or more times when resizing the window to a larger size.
once for the Configure and once for the expose. This is especially
annoying when running remotely. */

  rt_vls_init(&cmd);
  save_dm_list = curr_dm_list;
  curr_dm_list = get_dm_list(eventPtr->xany.window);

  if(curr_dm_list == DM_LIST_NULL)
    goto end;

#if TRY_PIPES
  if(mged_variables.focus && eventPtr->type == KeyPress){
    char buffer[1];

    XLookupString(&(eventPtr->xkey), buffer, 1,
		  (KeySym *)NULL, (XComposeStatus *)NULL);

    write(ged_pipe[1], buffer, 1);
    rt_vls_free(&cmd);
    curr_dm_list = save_dm_list;

    /* Use this so that these events won't propagate */
    return TCL_RETURN;
  }
#endif


  /* Now getting X events */
  if(eventPtr->type == Expose && eventPtr->xexpose.count == 0){
    /* Window may have moved */
    Glx_configure_window_shape();

    dmaflag = 1;
    if( glx_has_doublebuffer) /* to fix back buffer */
      refresh();
    dmaflag = 1;
  }else if( eventPtr->type == ConfigureNotify ){
      /* Window may have moved */
      Glx_configure_window_shape();

      if (eventPtr->xany.window != win)
	goto end;

      dmaflag = 1;
      if( glx_has_doublebuffer) /* to fix back buffer */
	refresh();
      dmaflag = 1;
  }else if( eventPtr->type == MotionNotify ) {
    int x, y;

    x = (eventPtr->xmotion.x/(double)winx_size - 0.5) * 4095;
    y = (0.5 - eventPtr->xmotion.y/(double)winy_size) * 4095;
    /* Constant tracking (e.g. illuminate mode) bound to M mouse */
    rt_vls_printf( &cmd, "M 0 %d %d\n", x, y );
  }else if( eventPtr->type == devmotionnotify ){
    XDeviceMotionEvent *M;
    int setting;

    M = (XDeviceMotionEvent * ) eventPtr;

    if(button0){
      knobs_during_help[M->first_axis] = M->axis_data[0];
      glx_dbtext(
		(adcflag ? kn1_knobs:kn2_knobs)[M->first_axis]);
      goto end;
    }else{
      knobs[M->first_axis] = M->axis_data[0];
      setting = irlimit(knobs[M->first_axis] - knobs_offset[M->first_axis]);
    }

    switch(DIAL0 + M->first_axis){
    case DIAL0:
      if(adcflag) {
	rt_vls_printf( &cmd, "knob ang1 %d\n",
		      setting );
      }
      break;
    case DIAL1:
      rt_vls_printf( &cmd , "knob S %f\n",
		    setting / 2048.0 );
      break;
    case DIAL2:
      if(adcflag)
	rt_vls_printf( &cmd , "knob ang2 %d\n",
		      setting );
      else
	rt_vls_printf( &cmd , "knob z %f\n",
		      setting / 2048.0 );
      break;
    case DIAL3:
      if(adcflag)
	rt_vls_printf( &cmd , "knob distadc %d\n",
		      setting );
      else
	rt_vls_printf( &cmd , "knob Z %f\n",
		      setting / 2048.0 );
      break;
    case DIAL4:
      if(adcflag)
	rt_vls_printf( &cmd , "knob yadc %d\n",
		      setting );
      else
	rt_vls_printf( &cmd , "knob y %f\n",
		      setting / 2048.0 );
      break;
    case DIAL5:
      rt_vls_printf( &cmd , "knob Y %f\n",
		    setting / 2048.0 );
      break;
    case DIAL6:
      if(adcflag)
	rt_vls_printf( &cmd , "knob xadc %d\n",
		      setting );
      else
	rt_vls_printf( &cmd , "knob x %f\n",
		      setting / 2048.0 );
      break;
    case DIAL7:
      rt_vls_printf( &cmd , "knob X %f\n",
		    setting / 2048.0 );
      break;
    default:
      break;
    }

  }else if( eventPtr->type == devbuttonpress ){
    XDeviceButtonEvent *B;

    B = (XDeviceButtonEvent * ) eventPtr;

    if(B->button == 1){
      button0 = 1;
      goto end;
    }

    if(button0){
      glx_dbtext(label_button(bmap[B->button - 1]));
    }else if(B->button == 4){
      rt_vls_strcat(&cmd, "knob zero\n");
      set_knob_offset();
    }else
      rt_vls_printf(&cmd, "press %s\n",
		    label_button(bmap[B->button - 1]));
  }else if( eventPtr->type == devbuttonrelease ){
    XDeviceButtonEvent *B;

    B = (XDeviceButtonEvent * ) eventPtr;

    if(B->button == 1){
      int i;

      button0 = 0;

      /* update the offset */
      for(i = 0; i < 8; ++i)
	knobs_offset[i] += knobs_during_help[i] - knobs[i];
    }
  }

end:
  (void)cmdline(&cmd, FALSE);
  rt_vls_free(&cmd);
  curr_dm_list = save_dm_list;
#else

/*XXX still drawing too much!!!
i.e. drawing 2 or more times when resizing the window to a larger size.
once for the Configure and once for the expose. This is especially
annoying when running remotely. */

  if(eventPtr->xany.window != win)
    goto end;

#if TRY_PIPES
  if(mged_variables.focus && eventPtr->type == KeyPress){
    char buffer[1];

    XLookupString(&(eventPtr->xkey), buffer, 1,
		  (KeySym *)NULL, (XComposeStatus *)NULL);

    write(ged_pipe[1], buffer, 1);
    goto end;
  }
#endif


  /* Now getting X events */
  if(eventPtr->type == Expose && eventPtr->xexpose.count == 0){
    /* Window may have moved */
    Glx_configure_window_shape();

    dmaflag = 1;
    if( glx_has_doublebuffer) /* to fix back buffer */
      refresh();
    dmaflag = 1;
  }else if( eventPtr->type == ConfigureNotify ){
      /* Window may have moved */
      Glx_configure_window_shape();

      dmaflag = 1;
      if( glx_has_doublebuffer) /* to fix back buffer */
	refresh();
      dmaflag = 1;
  }else if( eventPtr->type == MotionNotify ) {
    int x, y;

    x = (eventPtr->xmotion.x/(double)winx_size - 0.5) * 4095;
    y = (0.5 - eventPtr->xmotion.y/(double)winy_size) * 4095;
    /* Constant tracking (e.g. illuminate mode) bound to M mouse */
    rt_vls_printf( &dm_values.dv_string, "M 0 %d %d\n", x, y );
  }else if( eventPtr->type == devmotionnotify ){
    XDeviceMotionEvent *M;
    int setting;

    M = (XDeviceMotionEvent * ) eventPtr;

    if(button0){
      knobs_during_help[M->first_axis] = M->axis_data[0];
      glx_dbtext(
		(adcflag ? kn1_knobs:kn2_knobs)[M->first_axis]);
      goto end;
    }else{
      knobs[M->first_axis] = M->axis_data[0];
      setting = irlimit(knobs[M->first_axis] - knobs_offset[M->first_axis]);
    }

    switch(DIAL0 + M->first_axis){
    case DIAL0:
      if(adcflag) {
	rt_vls_printf( &dm_values.dv_string, "knob ang1 %d\n",
		      setting );
      }
      break;
    case DIAL1:
      rt_vls_printf( &dm_values.dv_string , "knob S %f\n",
		    setting / 2048.0 );
      break;
    case DIAL2:
      if(adcflag)
	rt_vls_printf( &dm_values.dv_string , "knob ang2 %d\n",
		      setting );
      else
	rt_vls_printf( &dm_values.dv_string , "knob z %f\n",
		      setting / 2048.0 );
      break;
    case DIAL3:
      if(adcflag)
	rt_vls_printf( &dm_values.dv_string , "knob distadc %d\n",
		      setting );
      else
	rt_vls_printf( &dm_values.dv_string , "knob Z %f\n",
		      setting / 2048.0 );
      break;
    case DIAL4:
      if(adcflag)
	rt_vls_printf( &dm_values.dv_string , "knob yadc %d\n",
		      setting );
      else
	rt_vls_printf( &dm_values.dv_string , "knob y %f\n",
		      setting / 2048.0 );
      break;
    case DIAL5:
      rt_vls_printf( &dm_values.dv_string , "knob Y %f\n",
		    setting / 2048.0 );
      break;
    case DIAL6:
      if(adcflag)
	rt_vls_printf( &dm_values.dv_string , "knob xadc %d\n",
		      setting );
      else
	rt_vls_printf( &dm_values.dv_string , "knob x %f\n",
		      setting / 2048.0 );
      break;
    case DIAL7:
      rt_vls_printf( &dm_values.dv_string , "knob X %f\n",
		    setting / 2048.0 );
      break;
    default:
      break;
    }

  }else if( eventPtr->type == devbuttonpress ){
    XDeviceButtonEvent *B;

    B = (XDeviceButtonEvent * ) eventPtr;

    if(B->button == 1){
      button0 = 1;
      goto end;
    }

    if(button0){
      glx_dbtext(label_button(bmap[B->button - 1]));
    }else if(B->button == 4){
      rt_vls_strcat(&dm_values.dv_string, "knob zero\n");
      set_knob_offset();
    }else
      rt_vls_printf(&dm_values.dv_string, "press %s\n",
		    label_button(bmap[B->button - 1]));
  }else if( eventPtr->type == devbuttonrelease ){
    XDeviceButtonEvent *B;

    B = (XDeviceButtonEvent * ) eventPtr;

    if(B->button == 1){
      int i;

      button0 = 0;

      /* update the offset */
      for(i = 0; i < 8; ++i)
	knobs_offset[i] += knobs_during_help[i] - knobs[i];
    }
  }
end:
#endif

  return TCL_OK;
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
	if( glx_debug ) rt_log("statechange %d %d\n", a, b );
	/*
	 *  Based upon new state, possibly do extra stuff,
	 *  including enabling continuous tablet tracking,
	 *  object highlighting
	 */
#if MIXED_MODE
 	switch( b )  {
	case ST_VIEW:
	  /* constant tracking OFF */
	  XSelectInput(dpy, win, ExposureMask|ButtonPressMask|
		       KeyPressMask|StructureNotifyMask);
	  break;
	case ST_S_PICK:
	case ST_O_PICK:
	case ST_O_PATH:
	  /* constant tracking ON */
	  XSelectInput(dpy, win, ExposureMask|ButtonPressMask|
		       KeyPressMask|StructureNotifyMask|PointerMotionMask);
	  break;
	case ST_O_EDIT:
	case ST_S_EDIT:
	  /* constant tracking OFF */
	  XSelectInput(dpy, win, ExposureMask|ButtonPressMask|
		       KeyPressMask|StructureNotifyMask);
	  break;
#else
	switch( b )  {
	case ST_VIEW:
		unqdevice( MOUSEY );	/* constant tracking OFF */
		/* This should not affect the tie()'d MOUSEY events */
		break;

	case ST_S_PICK:
	case ST_O_PICK:
	case ST_O_PATH:
		/*  Have all changes of MOUSEY generate an event */
		qdevice( MOUSEY );	/* constant tracking ON */
		break;
	case ST_O_EDIT:
	case ST_S_EDIT:
	case ST_S_VPICK:
		unqdevice( MOUSEY );	/* constant tracking OFF */
		break;
#endif
	default:
		rt_log("Glx_statechange: unknown state %s\n", state_str[b]);
		break;
	}

	Glx_viewchange( DM_CHGV_REDO, SOLID_NULL );
}

void
Glx_viewchange( cmd, sp )
register int cmd;
register struct solid *sp;
{
	if( glx_debug ) rt_log("viewchange( %d, x%x )\n", cmd, sp );
	switch( cmd )  {
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
	glx_debug = lvl;
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

	if( glx_debug )  rt_log("colorchange\n");

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

	if( glx_has_rgb )  {
		if(cueing_on) {
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
	if(cueing_on && (glx_nslots < 7)) {
		rt_log("Too few bitplanes: depthcueing disabled\n");
		cueing_on = 0;
	}
	glx_nslots = 1<<glx_nslots;
	if( glx_nslots > NSLOTS )  glx_nslots = NSLOTS;
	if(cueing_on) {
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

	if( glx_has_rgb )  return;

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

# if 0
	for(i = 0, cp = buf; i < 8 && *str; i++, cp++, str++)
		*cp = islower(*str) ?  toupper(*str) : *str;
	*cp = 0;
	dbtext(buf);
# else
	rt_log("dm-4d: You pressed Help key and '%s'\n", str);
# endif
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
static 
int irlimit(i)
int i;
{
	if( i > NOISE )
		return( i-NOISE );
	if( i < -NOISE )
		return( i+NOISE );
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
 *	This routine is not called at all if glx_has_rgb is set.
 */
glx_gen_color(c)
int c;
{
#if MIXED_MODE
#else
	if(cueing_on) {

		/*  Not much sense in making a ramp for DM_BLACK.  Besides
		 *  which, doing so, would overwrite the bottom color
		 *  map entries, which is a no-no.
		 */
		if( c != DM_BLACK) {
			register int i;
			fastf_t r_inc, g_inc, b_inc;
			fastf_t red, green, blue;

			r_inc = glx_rgbtab[c].r/16;
			g_inc = glx_rgbtab[c].g/16;
			b_inc = glx_rgbtab[c].b/16;

			red = glx_rgbtab[c].r;
			green = glx_rgbtab[c].g;
			blue = glx_rgbtab[c].b;

			for(i = 15; i >= 0;
			    i--, red -= r_inc, green -= g_inc, blue -= b_inc)
				mapcolor( MAP_ENTRY(c) + i,
				    (short)red,
				    (short)green,
				    (short)blue );
		}
	} else {
		mapcolor(c+CMAP_BASE,
		    glx_rgbtab[c].r, glx_rgbtab[c].g, glx_rgbtab[c].b);
	}
#endif
}

#ifdef never
/*
 *  Update the PF key lights.
 */
static int
kblights()
{
	char	lights;

	lights = (cueing_on)
	    | (zclipping_on << 1)
	    | (perspective_mode << 2)
	    | (zbuffer_on << 3);

	lampon(lights);
	lampoff(lights^0xf);
}
#endif

static void
establish_perspective()
{
  rt_vls_printf( &dm_values.dv_string,
		"set perspective %d\n",
		perspective_mode ?
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
  if(dummy_perspective > 0)
    perspective_angle = dummy_perspective <= 4 ? dummy_perspective - 1: 3;
  else if (--perspective_angle < 0) /* toggle perspective matrix */
    perspective_angle = 3;

  if(perspective_mode)
    rt_vls_printf( &dm_values.dv_string,
		  "set perspective %d\n",
		  perspective_table[perspective_angle] );

  /*
     Just in case the "!" is used with the set command. This
     allows us to toggle through more than two values.
   */
  dummy_perspective = 1;

  dmaflag = 1;
}

static void
establish_zbuffer()
{
	if( glx_has_zbuf == 0 )  {
		rt_log("dm-4d: This machine has no Zbuffer to enable\n");
		zbuffer_on = 0;
	}
	zbuffer( zbuffer_on );
	if( zbuffer_on)  {
		/* Set screen coords of near and far clipping planes */
		lsetdepth(min_scr_z, max_scr_z);
	}
	dmaflag = 1;
}

glx_clear_to_black()
{
	/* Re-enable the full viewport */
	viewport(0, winx_size, 0, winy_size);

	if( zbuffer_on )  {
		zfunction( ZF_LEQUAL );
		if( glx_has_rgb )  {
			czclear( 0x000000, max_scr_z );
		} else {
			czclear( BLACK, max_scr_z );
		}
	} else {
		if( glx_has_rgb )  {
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
	rt_log("usinit\n"); 
}
usnewlock()	{ 
	rt_log("usnewlock\n"); 
}
taskcreate()	{ 
	rt_log("taskcreate\n"); 
}
#endif

/*
 *  The structparse will change the value of the variable.
 *  Just implement it, here.
 */
static void
establish_lighting()
{
	if( !lighting_on )  {
		/* Turn it off */
		mmode(MVIEWING);
		lmbind(MATERIAL,0);
		lmbind(LMODEL,0);
		mmode(MSINGLE);
	} else {
		/* Turn it on */
		if( cueing_on )  {
			/* Has to be off for lighting */
			cueing_on = 0;
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
	struct rt_vls	vls;

	if( !strcmp( argv[0], "set" )){
	  rt_vls_init(&vls);
	  if( argc < 2 )  {
	    /* Bare set command, print out current settings */
	    rt_structprint("dm_4d internal variables", Glx_vparse, (char *)0 );
	    rt_log("%s", rt_vls_addr(&vls) );
	  } else if( argc == 2 ) {
	    rt_vls_name_print( &vls, Glx_vparse, argv[1], (char *)0 );
	    rt_log( "%s\n", rt_vls_addr(&vls) );
	  } else {
	    rt_vls_printf( &vls, "%s=\"", argv[1] );
	    rt_vls_from_argv( &vls, argc-2, argv+2 );
	    rt_vls_putc( &vls, '\"' );
	    rt_structparse( &vls, Glx_vparse, (char *)0 );
	  }
	  rt_vls_free(&vls);
	  return CMD_OK;
	}

	if( !strcmp( argv[0], "mouse" )){
	  int up;
	  int xpos;
	  int ypos;

	  if( argc < 4){
	    rt_log("dm: need more parameters\n");
	    rt_log("mouse 1|0 xpos ypos\n");
	    return CMD_BAD;
	  }

	  up = atoi(argv[1]);
	  xpos = atoi(argv[2]);
	  ypos = atoi(argv[3]);
	  rt_vls_printf(&dm_values.dv_string, "M %d %d %d\n",
			up, irisX2ged(xpos), irisY2ged(ypos));
	  return CMD_OK;
	}

	rt_log("dm: bad command - %s\n", argv[0]);
	return CMD_BAD;
}

void
set_knob_offset()
{
  int i;

  for(i = 0; i < 8; ++i){
    knobs_offset[i] = knobs[i];
  }
}

#ifdef MULTI_ATTACH
void
glx_var_init()
{
  dm_vars = (char *)rt_malloc(sizeof(struct glx_vars),
					    "glx_vars");
  devmotionnotify = LASTEvent;
  devbuttonpress = LASTEvent;
  devbuttonrelease = LASTEvent;
}

struct dm_list *
get_dm_list(window)
Window window;
{
  register struct dm_list *p;
  struct rt_vls str;

  for( RT_LIST_FOR(p, dm_list, &head_dm_list.l) ){
    if(window == ((struct glx_vars *)p->_dm_vars)->_win){
      rt_vls_init(&str);
      rt_vls_printf(&str, "%s winset\n", rt_vls_addr(&pathName));
      cmdline(&str, FALSE);
      rt_vls_free(&str);
      return p;
    }
  }

  return DM_LIST_NULL;
}
#endif
