/*
 *			D M - 4 D . C
 *
 *  This version for the SGI 4-D Iris, both regular and GT versions.
 *  
 *
 *  Uses library -lgl
 *
 *  Authors -
 *	Paul R. Stay
 *	Michael John Muuss
 *	Robert J. Reschly
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
#include <gl/gl.h>		/* SGI IRIS library */
#include <gl/device.h>		/* SGI IRIS library */
#include <gl/get.h>		/* SGI IRIS library */
#include <gl/cg2vme.h>		/* SGI IRIS, for DE_R1 defn on IRIX 3 */
#include <gl/addrs.h>		/* SGI IRIS, for DER1_STEREO defn on IRIX 3 */
#include <sys/types.h>
#include <sys/time.h>
#include <sys/invent.h>

#include "machine.h"
#include "bu.h"
#include "vmath.h"
#include "raytrace.h"
#include "./ged.h"
#include "./mged_dm.h"
#include "dm.h"
#include "externs.h"
#include "solid.h"
#include "tcl.h"

#define YSTEREO		491	/* subfield height, in scanlines */
#define YOFFSET_LEFT	532	/* YSTEREO + YBLANK ? */

extern inventory_t	*getinvent();

/* Display Manager package interface */

#define IRBOUND	4095.9	/* Max magnification in Rot matrix */

int	Ir_open();
void	Ir_close();
MGED_EXTERN(void	Ir_input, (fd_set *input, int noblock) );
void	Ir_prolog(), Ir_epilog();
void	Ir_normal(), Ir_newrot();
void	Ir_update();
void	Ir_puts(), Ir_2d_line(), Ir_light();
int	Ir_object();
unsigned Ir_cvtvecs(), Ir_load();
void	Ir_statechange(), Ir_viewchange(), Ir_colorchange();
void	Ir_window(), Ir_debug();
int	Ir_dm();
static void    Ircheckevents();
static void    ir_var_init();

#define ir_fd (((struct ir_vars *)dm_vars)->_ir_fd)
#define gr_id (((struct ir_vars *)dm_vars)->_gr_id)
#define win (((struct ir_vars *)dm_vars)->_win)
#define win_l (((struct ir_vars *)dm_vars)->_win_l)
#define win_b (((struct ir_vars *)dm_vars)->_win_b)
#define win_r (((struct ir_vars *)dm_vars)->_win_r)
#define win_t (((struct ir_vars *)dm_vars)->_win_t)
#define winx_size (((struct ir_vars *)dm_vars)->_winx_size)
#define winy_size (((struct ir_vars *)dm_vars)->_winy_size)
#define perspective_mode (((struct ir_vars *)dm_vars)->_perspective_mode)
#define perspective_angle (((struct ir_vars *)dm_vars)->_perspective_angle)
#define knobs (((struct ir_vars *)dm_vars)->_knobs)
#define stereo_is_on (((struct ir_vars *)dm_vars)->_stereo_is_on)
#define ir_is_gt (((struct ir_vars *)dm_vars)->_ir_is_gt)
#define aspect_corr (((struct ir_vars *)dm_vars)->_aspect_corr)
#define ir_oldmonitor (((struct ir_vars *)dm_vars)->_ir_oldmonitor)
#define mvars (((struct ir_vars *)dm_vars)->_mvars)

struct modifiable_ir_vars {
  int cueing_on;
  int zclipping_on;
  int zbuffer_on;
  int lighting_on;
  int zbuf;
  int rgb;
  int doublebuffer;
  int min_scr_z;       /* based on getgdesc(GD_ZMIN) */
  int max_scr_z;       /* based on getgdesc(GD_ZMAX) */
  int debug;
  int linewidth;
};

struct ir_vars {
  int _ir_fd;
  long _gr_id;
  Window _win;
  long _win_l, _win_b, _win_r, _win_t;
  long _winx_size, _winy_size;
  int _perspective_mode;
  int _perspective_angle;
  int _knobs[8];
  int _stereo_is_on;
  int _ir_is_gt;
  mat_t _aspect_corr;
  int _ir_oldmonitor;
  struct modifiable_ir_vars _mvars;
};

static int ir_count = 0;
static int perspective_table[] = { 
	30, 45, 60, 90 };
static int ovec = -1;		/* Old color map entry number */
static int kblights();
static double	xlim_view = 1.0;	/* args for ortho() */
static double	ylim_view = 1.0;
static CONST char ir_title[] = "BRL MGED";


void		ir_colorit();

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
char	*kn1_knobs[] = {
	/* 0 */ "adc <1",	/* 1 */ "zoom", 
	/* 2 */ "adc <2",	/* 3 */ "adc dist",
	/* 4 */ "adc y",	/* 5 */ "y slew",
	/* 6 */ "adc x",	/* 7 */	"x slew"
};
char	*kn2_knobs[] = {
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
static int ir_nslots=0;		/* how many we have, <= NSLOTS */
static int slotsused;		/* how many actually used */
static struct rgbtab {
	unsigned char	r;
	unsigned char	g;
	unsigned char	b;
} ir_rgbtab[NSLOTS];

struct dm dm_4d = {
	Ir_open, Ir_close,
	Ir_input,
	Ir_prolog, Ir_epilog,
	Ir_normal, Ir_newrot,
	Ir_update,
	Ir_puts, Ir_2d_line,
	Ir_light,
	Ir_object,
	Ir_cvtvecs, Ir_load,
	Ir_statechange,
	Ir_viewchange,
	Ir_colorchange,
	Ir_window, Ir_debug,
	0,			/* no "displaylist", per. se. */
	0,			/* multi-window */
	IRBOUND,
	"sgi", "SGI 4d",
	0,			/* mem map */
	Ir_dm
};

extern struct device_values dm_values;	/* values read from devices */

static void	establish_lighting();
static void	establish_zbuffer();


static void
refresh_hook()
{
	dmaflag = 1;
}

#define IR_MV_O(_m) offsetof(struct modifiable_ir_vars, _m)
struct bu_structparse Ir_vparse[] = {
	{"%d",  1, "depthcue",		IR_MV_O(cueing_on),	Ir_colorchange },
	{"%d",  1, "zclip",		IR_MV_O(zclipping_on),	refresh_hook },
	{"%d",  1, "zbuffer",		IR_MV_O(zbuffer_on),	establish_zbuffer },
	{"%d",  1, "lighting",		IR_MV_O(lighting_on),	establish_lighting },
	{"%d",  1, "has_zbuf",		IR_MV_O(zbuf),	refresh_hook },
	{"%d",  1, "has_rgb",		IR_MV_O(rgb),	Ir_colorchange },
	{"%d",  1, "has_doublebuffer",	IR_MV_O(doublebuffer), refresh_hook },
	{"%d",  1, "min_scr_z",		IR_MV_O(min_scr_z),	refresh_hook },
	{"%d",  1, "max_scr_z",		IR_MV_O(max_scr_z),	refresh_hook },
	{"%d",  1, "debug",		IR_MV_O(debug),		FUNC_NULL },
	{"%d",  1, "linewidth",		IR_MV_O(linewidth),	refresh_hook },
	{"",	0,  (char *)0,		0,			FUNC_NULL }
};

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
	if( x <= win_l )  return(-2048);
	if( x >= win_r )  return(2047);
	x -= win_l;
	x = ( x / (double)winx_size)*4096.0;
	x -= 2048;
	return(x);
}

static int
irisY2ged(y)
register int y;
{
	if( y <= win_b )  return(-2048);
	if( y >= win_t )  return(2047);
	y -= win_b;
	if( stereo_is_on )  y = (y%512)<<1;
	y = ( y / (double)winy_size)*4096.0;
	y -= 2048;
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
Ir_configure_window_shape()
{
	int		npix;
	int		monitor;

	xlim_view = 1.0;
	ylim_view = 1.0;
	mat_idn(aspect_corr);

	getsize( &winx_size, &winy_size);
	getorigin( &win_l, & win_b );
	win_r = win_l + winx_size;
	win_t = win_b + winy_size;

	/* Write enable all the bloody bits after resize! */
	viewport(0, winx_size, 0, winy_size);

	if( mvars.zbuf ) establish_zbuffer();
	establish_lighting();
	
	if( mvars.doublebuffer)
	{
		/* Clear out image from windows underneath */
		frontbuffer(1);
		ir_clear_to_black();
		frontbuffer(0);
		ir_clear_to_black();
	} else
		ir_clear_to_black();

	monitor = getmonitor();
	{
		extern Tcl_Interp *interp;	/* cmd.c */
		char	*val;

		val = Tcl_GetVar2(interp, "sgi_ntsc", NULL, TCL_GLOBAL_ONLY);
		if( val && atoi(val) != 0 )  {
		  Tcl_AppendResult(interp, "dm-4d: setting NTSC size window\n", (char *)NULL);
		  monitor = NTSC;
		}
	}
	switch( monitor )  {
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

		if( !mged_variables.faceplate )  {
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

		mvars.linewidth = 3;
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
		mvars.linewidth = 3;
		blanktime(0);	/* don't screensave while recording video! */
		break;
	}

	ortho( -xlim_view, xlim_view, -ylim_view, ylim_view, -1.0, 1.0 );
	/* The ortho() call really just makes this matrix: */
	aspect_corr[0] = 1/xlim_view;
	aspect_corr[5] = 1/ylim_view;
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
int
Ir_open()
{
	register int	i;
	Matrix		m;
	inventory_t	*inv;
	int		win_size=1000;
	int		win_o_x=272;
	int		win_o_y=12;

	if(ir_count){
	  ++ir_count;
	  Tcl_AppendResult(interp, "Ir_open: sgi is already open\n", (char *)NULL);
	  return TCL_ERROR;
	}

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

	ir_var_init();
	bu_vls_printf(&pathName, ".dm_4d");

#if 1
	{
	  static char dm_4d_dstr[80];

	  sprintf(dm_4d_dstr, "DISPLAY=%s", dname);
	  putenv(dm_4d_dstr);
	}

	if(getgdesc(GD_BITS_NORM_DBL_RED) > 0){
	  mvars.rgb = 1;
	  mvars.doublebuffer = 1;
	  ir_is_gt = 1;
	}

	if(getgdesc(GD_BITS_NORM_ZBUFFER) > 0)
	  mvars.zbuf = 1;
#else
	/*
	 *  Take inventory of the hardware.
	 *  See "types for class graphics" in /usr/include/sys/invent.h
	 */
	while( (inv = getinvent()) != (inventory_t *)0 )  {
		if( inv->class != INV_GRAPHICS )  continue;
		switch( inv->type )  {
		default:
#if 0
			bu_log("mged/dm-4d.c: getinvent() INV_GRAPHICS type=%d not recognized, you need to modify the source code\n",
			    inv->type);
#endif
			/* Since we recognize all the old devices, be
			 * optimistic and assume that new devices are plush.
			 * Or at least that GL can simulate everything adequately.
			 */
			ir_is_gt = 1;
			mvars.zbuf = 1;
			mvars.rgb = 1;
			mvars.doublebuffer = 1;
			break;
		case INV_GRODEV:			 /* 4D/60 machines */
			mvars.doublebuffer = 1;
			if( inv->state & INV_GR1ZBUF24 )
				mvars.zbuf = 1;
			break;
		case INV_VGX:
		case INV_VGXT:			/* VGX Turbo and SkyWriter */
		case INV_GMDEV:			/* GT graphics */
		case INV_CG2:
			ir_is_gt = 1;
			mvars.zbuf = 1;
			mvars.rgb = 1;
			mvars.doublebuffer = 1;
			break;
		case INV_GR1BP:
			mvars.rgb = 1;
			break;
		case INV_GR1ZBUFFER:
			mvars.zbuf = 1;
			break;
		case INV_GR1BOARD:	/* Personal Iris */
			if ( inv->state & INV_GR1RE2 )
				ir_is_gt = 1;
			if(inv->state & INV_GR1ZBUF24 )
				mvars.zbuf = 1;
			if(inv->state & INV_GR1BIT24 )
				mvars.rgb = 1;
			mvars.doublebuffer = 1;
			break;
#if defined(INV_LIGHT)
		case INV_LIGHT:		/* Entry Level Indigo */
			ir_is_gt = 0;
			mvars.zbuf = 1;
			mvars.doublebuffer = 0;
			mvars.rgb = 1;
			break;
#endif

#if defined(INV_GR2)
		case INV_GR2:		/* Elan EXPRESS Graphics */
			/* if(inv->state & INV_GR2_ELAN) */
			/* Just let GL simulate it */
			mvars.rgb = 1;
			mvars.doublebuffer = 1;
			mvars.zbuf = 1;
			ir_is_gt = 1;
			/* if(inv->state & INV_GR2_EXTREME) */
			break;
#endif
#if defined(INV_NEWPORT)
		case INV_NEWPORT:
			/* if(inv->state & INV_NEWPORT_XL) */
			/* Just let GL simulate it */
			mvars.rgb = 1;
			mvars.doublebuffer = 1;
			mvars.zbuf = 1;
			ir_is_gt = 1;
#			if 0
				if(inv->state & INV_NEWPORT_24)
					mvars.rgb = 1;
#			endif
			break;
#endif
		}
	}
	endinvent();		/* frees internal inventory memory */
#endif

#if 0
	bu_log("4D: gt=%d, zbuf=%d, rgb=%d\n", ir_is_gt, mvars.zbuf, mvars.rgb);
#endif

	/* Start out with the usual window */
	foreground();
#if 1
	if (mged_variables.sgi_win_size > 0)
		win_size = mged_variables.sgi_win_size;

	if (mged_variables.sgi_win_origin[0] != 0)
		win_o_x = mged_variables.sgi_win_origin[0];

	if (mged_variables.sgi_win_origin[1] != 0)
		win_o_y = mged_variables.sgi_win_origin[1];

	prefposition( win_o_x, win_o_x+win_size, win_o_y, win_o_y+win_size);
#else
	prefposition( 376, 1276, 12, 912 );		/* Old, larger size */
	prefposition( 376, 376+900, 112, 112+900 );	/* new, smaller size */
#endif
	if( (gr_id = winopen( "BRL MGED" )) == -1 )  {
	  Tcl_AppendResult(interp, "No more graphics ports available.\n", (char *)NULL);
	  return TCL_ERROR;
	}
	keepaspect(1,1);	/* enforce 1:1 aspect ratio */
	winconstraints();	/* remove constraints on the window size */

	ir_oldmonitor = getmonitor();
	if( mged_variables.eye_sep_dist )  {
		if( sgi_has_stereo() )  {
			setmonitor(STR_RECT);
			stereo_is_on = 1;
		} else {
		  Tcl_AppendResult(interp, "NOTICE: This SGI does not have stereo display capability\n", (char *)NULL);
		  stereo_is_on = 0;
		}
	}

	/*
	 *  If monitor is in special mode, close window and re-open.
	 *  winconstraints() does not work, and getmonitor() can't
	 *  be called before a window is open.
	 */
	switch( getmonitor() )  {
	case HZ30:
	case HZ30_SG:
		/* Dunn camera, etc. */
		/* Use already established prefposition */
		break;
	case STR_RECT:
		/* Hi-res monitor in stereo mode, take over whole screen */
		winclose(gr_id);
		noborder();
		foreground();
#if defined(__sgi) && defined(__mips)
		/* Deal with Irix 4.0 bug:  (+2,+0) offset due to border */
		prefposition( 0-2, XMAXSCREEN-2, 0, YMAXSCREEN );
#else
		prefposition( 0, XMAXSCREEN, 0, YMAXSCREEN );
#endif
		if( (gr_id = winopen( "BRL MGED" )) == -1 )  {
		  Tcl_AppendResult(interp, "No more graphics ports available.\n", (char *)NULL);
		  return TCL_ERROR;
		}
		break;
	default:
	case HZ60:
		/* Regular hi-res monitor */
		/* Use already established prefposition */
		break;
	case NTSC:
		/* Television */
		winclose(gr_id);
		prefposition( 0, XMAX170, 0, YMAX170 );
		foreground();
		if( (gr_id = winopen( "BRL MGED" )) == -1 )  {
		  Tcl_AppendResult(interp, "No more graphics ports available.\n", (char *)NULL);
		  return TCL_ERROR;
		}
		break;
	case PAL:
		/* Television */
		winclose(gr_id);
		prefposition( 0, XMAXPAL, 0, YMAXPAL );
		foreground();
		if( (gr_id = winopen( "BRL MGED" )) == -1 )  {
		  Tcl_AppendResult(interp, "No more graphics ports available.\n", (char *)NULL);
		  return TCL_ERROR;
		}
		break;
	}

	/*
	 *  Configure the operating mode of the pixels in this window.
	 *  Do not output graphics, clear screen, etc, until *after*
	 *  the call to gconfig().
	 */
	if( mvars.rgb )  {
		RGBmode();
	} else {
		/* one indexed color map of 4096 entries */
		onemap();
	}
	if ( mvars.doublebuffer)
		doublebuffer();

	gconfig();

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

	Ir_configure_window_shape();

	/* Enable qdev() input from various devices */
	qdevice(LEFTMOUSE);
	qdevice(MIDDLEMOUSE);
	qdevice(RIGHTMOUSE);
	tie(LEFTMOUSE, MOUSEX, MOUSEY);
	tie(MIDDLEMOUSE, MOUSEX, MOUSEY);
	tie(RIGHTMOUSE, MOUSEX, MOUSEY);

#if IR_KNOBS
	/*
	 *  Turn on the dials and initialize them for -2048 to 2047
	 *  range with a dead spot at zero (Iris knobs are 1024 units
	 *  per rotation).
	 */
	for(i = DIAL0; i < DIAL8; i++)
		setvaluator(i, 0, -2048-NOISE, 2047+NOISE);
	for(i = DIAL0; i < DIAL8; i++)
		qdevice(i);
#endif
#if IR_BUTTONS
	/*
	 *  Enable all the buttons in the button table.
	 */
	for(i = 0; i < IR_BUTTONS; i++)
		qdevice(i+SWBASE);
	/*
	 *  For all possible button presses, build a table
	 *  of MGED function to SGI button/light mappings.
	 */
	for( i=0; i < IR_BUTTONS; i++ )  {
		register int j;
		if( (j = bmap[i]) != 0 )
			invbmap[j] = i;
	}
# if 0
	ir_dbtext(ir_title);
# else
	dbtext("");
# endif
#endif
        qdevice(THREEKEY); /* press 35,25 */
        qdevice(FOURKEY); /* press 45,45 */
	qdevice(AKEY); /* adc */
        qdevice(BKEY); /* press bottom */
        qdevice(EKEY); /* set e_axes ! */
        qdevice(FKEY); /* press front */
        qdevice(IKEY); /* aip f */
        qdevice(LKEY); /* press left */
        qdevice(OKEY); /* press oill */
	qdevice(PKEY); /* M 1 0 0 */
        qdevice(QKEY); /* press reject */
        qdevice(RKEY); /* press right */
        qdevice(SKEY); /* press sill */
        qdevice(TKEY); /* press top */
        qdevice(UKEY); /* aip b */
        qdevice(VKEY); /* set v_axes ! */
        qdevice(WKEY); /* set w_axes ! */

	qdevice(F1KEY);	/* pf1 key for depthcue switching */
	qdevice(F2KEY);	/* pf2 for Z clipping */
	qdevice(F3KEY);	/* pf3 for perspective */
	qdevice(F4KEY);	/* pf4 for Z buffering */
	qdevice(F5KEY);	/* pf5 for lighting */
	qdevice(F6KEY);	/* pf6 for changing perspective */
	qdevice(F7KEY);	/* pf7 toggles faceplate */
	qdevice(F8KEY);	/* pf8 toggles menu */
	qdevice(F12KEY);/* pf12 toggles focus */
	while( getbutton(LEFTMOUSE)||getbutton(MIDDLEMOUSE)||getbutton(RIGHTMOUSE) )  {
	  Tcl_AppendResult(interp, "IRIS_open:  mouse button stuck\n", (char *)NULL);
	  sleep(1);
	}

	/* Line style 0 is solid.  Program line style 1 as dot-dashed */
	deflinestyle( 1, 0xCF33 );
	setlinestyle( 0 );

	ir_fd = qgetfd();

	Tk_CreateFileHandler(ir_fd, 1, Ircheckevents, (ClientData)curr_dm_list);
	ir_count = 1;
	return(0);
}

/*
 *  			I R _ C L O S E
 *  
 *  Gracefully release the display.  Well, mostly gracefully -- see
 *  the comments in the open routine.
 */
void
Ir_close()
{
#ifdef DM_OGL
  if (ogl_ogl_used)
    return;
#endif

  if(ir_count > 1){
    --ir_count;
    return;
  }

  if(mvars.cueing_on) depthcue(0);

  lampoff( 0xf );

  /* avoids error messages when reattaching */
  mmode(MVIEWING);	
  lmbind(LIGHT2,0);
  lmbind(LIGHT3,0);
  lmbind(LIGHT4,0);
  lmbind(LIGHT5,0);

  frontbuffer(1);
  ir_clear_to_black();
  frontbuffer(0);

  if( getmonitor() != ir_oldmonitor )
    setmonitor(ir_oldmonitor);

  winclose(gr_id);

  if(ir_fd)
    Tk_DeleteFileHandler(ir_fd);

  if(dm_vars)
    bu_free(dm_vars, "Ir_close: dm_vars");

  ir_count = 0;
  return;
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
Ir_prolog()
{
	if (mvars.debug)
	  Tcl_AppendResult(interp, "Ir_prolog\n", (char *)NULL);
#if 0
	ortho2( -1.0,1.0, -1.0,1.0);	/* L R Bot Top */
#else
	ortho( -xlim_view, xlim_view, -ylim_view, ylim_view, -1.0, 1.0 );
#endif

	if( !mvars.doublebuffer )
	{
		ir_clear_to_black();
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
Ir_normal()
{
	if (mvars.debug)
	  Tcl_AppendResult(interp, "Ir_normal\n", (char *)NULL);

	if( mvars.rgb )  {
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
Ir_epilog()
{
	if (mvars.debug)
	  Tcl_AppendResult(interp, "Ir_epilog\n", (char *)NULL);

	/*
	 * A Point, in the Center of the Screen.
	 * This is drawn last, to always come out on top.
	 */
	Ir_2d_line( 0, 0, 0, 0, 0 );
	/* End of faceplate */

	if(mvars.doublebuffer )
	{
		swapbuffers();
		/* give Graphics pipe time to work */
		ir_clear_to_black();
	}
}

/*
 *  			I R _ N E W R O T
 *
 *  Load a new rotation matrix.  This will be followed by
 *  many calls to Ir_object().
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
Ir_newrot(mat, which_eye)
mat_t	mat;
{
	register fastf_t *mptr;
	Matrix	gtmat;
	mat_t	newm;
	int	i;

	if (mvars.debug)
	  Tcl_AppendResult(interp, "Ir_newrot()\n", (char *)NULL);

	switch(which_eye)  {
	case 0:
		/* Non-stereo */
		break;
	case 1:
		/* R eye */
		viewport(0, XMAXSCREEN, 0, YSTEREO);
		Ir_puts( "R", 2020, 0, 0, DM_RED );
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
Ir_object( sp, m, ratio, white )
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
	  Tcl_AppendResult(interp, "Ir_Object()\n", (char *)NULL);

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
		if(mvars.lighting_on && ir_is_gt)
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
	for( BU_LIST_FOR( vp, rt_vlist, &(sp->s_vlist) ) )  {
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
Ir_update()
{
	if (mvars.debug)
	  Tcl_AppendResult(interp, "Ir_update()\n", (char *)NULL);

	return;
}

/*
 *			I R _ P U T S
 *
 * Output a string.
 * The starting position of the beam is as specified.
 */
void
Ir_puts( str, x, y, size, colour )
register char *str;
int x,y,size, colour;
{
	if (mvars.debug)
	  Tcl_AppendResult(interp, "Ir_puts()\n", (char *)NULL);

	cmov2( GED2IRIS(x), GED2IRIS(y));
	if( mvars.rgb )  {
		RGBcolor( (short)ir_rgbtab[colour].r,
		    (short)ir_rgbtab[colour].g,
		    (short)ir_rgbtab[colour].b );
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
Ir_2d_line( x1, y1, x2, y2, dashed )
int x1, y1;
int x2, y2;
int dashed;
{
	register int nvec;

	if (mvars.debug)
	  Tcl_AppendResult(interp, "Ir_2d_line()\n", (char *)NULL);

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
Ir_input( input, noblock )
fd_set		*input;
int		noblock;
{
	static int cnt;
	register int i;
	struct timeval	tv;
	fd_set		files;
	int		width;

	if (mvars.debug)
	  Tcl_AppendResult(interp, "Ir_input()\n", (char *)NULL);

	if( (width = sysconf(_SC_OPEN_MAX)) <= 0 )
		width = 32;
	files = *input;		/* save, for restore on each loop */
	FD_SET( ir_fd, &files );

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
			FD_SET( ir_fd, input );
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

	if( FD_ISSET( ir_fd, input ) )
		Ircheckevents();

	return;
}
/*
 *  C H E C K E V E N T S
 *
 *  Look at events to check for button, dial, and mouse movement.
 */
static void
Ircheckevents(clientData, mask)
ClientData clientData;
int mask;
{
#define NVAL 48
  short values[NVAL];
  register short *valp;
  register int ret;
  register int n;
  static	button0  = 0;	/*  State of button 0 */
  static char	pending_button = 'M';
  static int	pending_val = 0;
  static	pending_x = 0;
  static	pending_y = 0;
  register struct dm_list *save_dm_list;
  struct bu_vls cmd;

/*
  if (qtest() == 0)
  return;
*/

  bu_vls_init(&cmd);

  save_dm_list = curr_dm_list;
  curr_dm_list = (struct dm_list *)clientData;

  n = blkqread( values, NVAL );	/* n is # of shorts returned */
  if( mvars.debug ){
    struct bu_vls tmp_vls;

    bu_vls_init(&tmp_vls);
    bu_vls_printf(&tmp_vls, "blkqread gave %d\n", n);
    Tcl_AppendResult(interp, bu_vls_addr(&tmp_vls), (char *)NULL);
    bu_vls_free(&tmp_vls);
  }
  for( valp = values; n > 0; n -= 2, valp += 2 )  {

    ret = *valp;
    if( mvars.debug ){
      struct bu_vls tmp_vls;

      bu_vls_init(&tmp_vls);
      bu_vls_printf(&tmp_vls, "qread ret=%d, val=%d\n", ret, valp[1]);
      Tcl_AppendResult(interp, bu_vls_addr(&tmp_vls), (char *)NULL);
      bu_vls_free(&tmp_vls);
    }
#if IR_BUTTONS
    if((ret >= SWBASE && ret < SWBASE+IR_BUTTONS)
       || (ret >= F1KEY && ret <= F12KEY)
       ) {
      register int	i;

      /*
       *  Switch 0 is help key.
       *  Holding down key 0 and pushing another
       *  button or turning the knob will display
       *  what it is in the button box display.
       */
			if(ret == SW_HELP_KEY)  {
				button0 = valp[1];
#if IR_KNOBS
				/*
				 *  Save and restore dial settings
				 *  so that when the user twists them
				 *  while holding down button 0 he
				 *  really isn't changing them.
				 */
				for(i = 0; i < 8; i++)
					if(button0)
						knobs[i] =
						    getvaluator(DIAL0+i);
					else setvaluator(DIAL0+i,knobs[i],
					    -2048-NOISE, 2047+NOISE);
#endif
				if(button0)
					ir_dbtext("Help Key");
# if 0
else
	ir_dbtext(ir_title);
# endif
continue;

			}

#if IR_KNOBS
			/*
			 *  If button 1 is pressed, reset run away knobs.
			 */
			if(ret == SW_ZERO_KEY || ret == F12KEY)  {
				if(!valp[1]) continue; /* Ignore release */
				/*  Help mode */
				if(button0)  {
					ir_dbtext("ZeroKnob");
					continue;
				}
#if IR_KNOBS
				/* zap the knobs */
				for(i = 0; i < 8; i++)  {
					setvaluator(DIAL0+i, 0,
					    -2048-NOISE, 2047+NOISE);
					knobs[i] = 0;
				}
#endif
				bu_vls_printf(&cmd,
				    "knob zero\n");
				continue;
			}
#endif
			/*
			 *  If PFkey 1 is pressed, toggle depthcueing.
			 */
			if(ret == F1KEY)  {
				if(!valp[1]) continue; /* Ignore release */
				/*  Help mode */
				if(button0)  {
					ir_dbtext("Depthcue");
					continue;
				}
				/* toggle depthcueing and remake colormap */
				bu_vls_printf(&cmd,
				    "dm set depthcue !\n");
				continue;
			} else if(ret == F2KEY)  {
				if(!valp[1]) continue; /* Ignore release */
				/*  Help mode */
				if(button0)  {
					ir_dbtext("Z clip");
					continue;
				}
				/* toggle zclipping */
				bu_vls_printf(&cmd,
				    "dm set zclip !\n");
				continue;
			} else if(ret == F3KEY)  {
				if(!valp[1]) continue; /* Ignore release */
				/*  Help mode */
				if(button0)  {
					ir_dbtext("Perspect");
					continue;
				}
				perspective_mode = 1-perspective_mode;
				bu_vls_printf( &cmd,
				    "set perspective %d\n",
				    perspective_mode ?
				    perspective_table[perspective_angle] :
				    -1 );
				dmaflag = 1;
				continue;
			} else if(ret == F4KEY)  {
				if(!valp[1]) continue; /* Ignore release */
				/*  Help mode */
				if(button0)  {
					ir_dbtext("Zbuffing");
					continue;
				}
				/* toggle zbuffer status */
				bu_vls_printf(&cmd,
				    "dm set zbuffer !\n");
				continue;
			} else if(ret == F5KEY)  {
				if(!valp[1]) continue; /* Ignore release */
				/*  Help mode */
				if(button0)  {
					ir_dbtext("Lighting");
					continue;
				}
				/* toggle status */
				bu_vls_printf(&cmd,
				    "dm set lighting !\n");
				continue;
			} else if (ret == F6KEY) {
				if (!valp[1]) continue; /* Ignore release */
				/* Help mode */
				if (button0) {
					ir_dbtext("P-Angle");
					continue;
				}
				/* toggle perspective matrix */
				if (--perspective_angle < 0) perspective_angle = 3;
				if(perspective_mode) bu_vls_printf( &cmd,
				    "set perspective %d\n",
				    perspective_table[perspective_angle] );
				dmaflag = 1;
				continue;
			} else if (ret == F7KEY) {
				if (!valp[1]) continue; /* Ignore release */
				/* Help mode */
				if (button0) {
					ir_dbtext("NoFace");
					continue;
				}
				/* Toggle faceplate on/off */
#if 0
				no_faceplate = !no_faceplate;

				bu_vls_strcat( &cmd,
				    no_faceplate ?
				    "set faceplate 0\n" :
				    "set faceplate 1\n" );
#else
				bu_vls_strcat( &cmd,
					       "set faceplate !\n" );
#endif
				Ir_configure_window_shape();
				dmaflag = 1;
				continue;
			} else if (ret == F8KEY) {
				if (!valp[1]) continue; /* Ignore release */
				/* Help mode */
				if (button0) {
					ir_dbtext("NoMenu");
					continue;
				}

				bu_vls_strcat( &cmd,
					       "set show_menu !\n" );

				Ir_configure_window_shape();
				dmaflag = 1;
				continue;
			}
			/*
			 * If button being depressed (as opposed to
			 * being released) either display the cute
			 * message or process the button depending
			 * on whether button0 is also being held down.
			 */
			i = bmap[ret - SWBASE];



			if(!valp[1]) continue;
			if(button0) {
				ir_dbtext(label_button(i));
			} else {
				/* An actual button press */
#				if 0
				/* old way -- an illegal upcall */
				button(i);
#				else
				/* better way -- queue a string command */
				bu_vls_strcat( &cmd,
				    "press " );
				bu_vls_strcat( &cmd,
				    label_button(i) );
				bu_vls_strcat( &cmd,
				    "\n" );
#				endif
			}
			continue;
    }
#endif
    if( ret >= BUT0 && ret <= MAXKBDBUT ) {
      if( !valp[1] )
	continue; /* Ignore release */

      switch( ret ) {
      case THREEKEY:
	/*  Help mode */
	if(button0)  {
	  ir_dbtext("press 35,25");
	  break;
	}

	bu_vls_strcat(&cmd, "press 35,25\n");
	break;
      case FOURKEY:
	/*  Help mode */
	if(button0)  {
	  ir_dbtext("press 45,45");
	  break;
	}

	bu_vls_strcat(&cmd, "press 45,45\n");
	break;
      case AKEY:
	/*  Help mode */
	if(button0)  {
	  ir_dbtext("adc");
	  break;
	}

	bu_vls_strcat(&cmd, "adc\n");
	break;
      case BKEY:
	/*  Help mode */
	if(button0)  {
	  ir_dbtext("press bottom");
	  break;
	}

	bu_vls_strcat(&cmd, "press bottom\n");
	break;
      case EKEY:
	/*  Help mode */
	if(button0)  {
	  ir_dbtext("set e_axes !");
	  break;
	}

	bu_vls_strcat(&cmd, "set e_axes !\n");
	break;
      case FKEY:
	/*  Help mode */
	if(button0)  {
	  ir_dbtext("press front");
	  break;
	}

	bu_vls_strcat(&cmd, "press front\n");
	break;
      case IKEY:
	/*  Help mode */
	if(button0)  {
	  ir_dbtext("aip f");
	  break;
	}

	bu_vls_strcat(&cmd, "aip f\n");
	break;
      case LKEY:
	/*  Help mode */
	if(button0)  {
	  ir_dbtext("press left");
	  break;
	}

	bu_vls_strcat(&cmd, "press left\n");
	break;
      case OKEY:
	/*  Help mode */
	if(button0)  {
	  ir_dbtext("press oill");
	  break;
	}

	bu_vls_strcat(&cmd, "press oill\n");
	break;
      case PKEY:
	/*  Help mode */
	if(button0)  {
	  ir_dbtext("M 1 0 0");
	  break;
	}

	bu_vls_strcat(&cmd, "M 1 0 0\n");
	break;
      case QKEY:
	/*  Help mode */
	if(button0)  {
	  ir_dbtext("press reject");
	  break;
	}

	bu_vls_strcat(&cmd, "press reject\n");
	break;
      case RKEY:
	/*  Help mode */
	if(button0)  {
	  ir_dbtext("press right");
	  break;
	}

	bu_vls_strcat(&cmd, "press right\n");
	break;
      case SKEY:
	/*  Help mode */
	if(button0)  {
	  ir_dbtext("press sill");
	  break;
	}

	bu_vls_strcat(&cmd, "press sill\n");
	break;
      case TKEY:
	/*  Help mode */
	if(button0)  {
	  ir_dbtext("press top");
	  break;
	}

	bu_vls_strcat(&cmd, "press top\n");
	break;
      case UKEY:
	/*  Help mode */
	if(button0)  {
	  ir_dbtext("aip b");
	  break;
	}

	bu_vls_strcat(&cmd, "aip b\n");
	break;
      case VKEY:
	/*  Help mode */
	if(button0)  {
	  ir_dbtext("set v_axes !");
	  break;
	}

	bu_vls_strcat(&cmd, "set v_axes !\n");
	break;
      case WKEY:
	/*  Help mode */
	if(button0)  {
	  ir_dbtext("set w_axes");
	  break;
	}

	bu_vls_strcat(&cmd, "set w_axes !\n");
	break;
      default:
	break;
      }

      continue;
    }
#if IR_KNOBS
		/*  KNOBS, uh...er...DIALS  */
		/*	6	7
		 *	4	5
		 *	2	3
		 *	0	1
		 */
		if(ret >= DIAL0 && ret <= DIAL8)  {
			int	setting;
			/*  Help MODE */
			if(button0)  {
				ir_dbtext(
				    (mged_variables.adcflag ? kn1_knobs:kn2_knobs)[ret-DIAL0]);
				continue;
			}
			/* Make a dead zone around 0 */
			setting = irlimit(valp[1]);
			switch(ret)  {
			case DIAL0:
				if(mged_variables.adcflag) {
					bu_vls_printf( &cmd, "knob ang1 %d\n",
							setting );
				}
				break;
			case DIAL1:
				bu_vls_printf( &cmd , "knob S %f\n",
							setting/2048.0 );
				break;
			case DIAL2:
				if(mged_variables.adcflag)
					bu_vls_printf( &cmd , "knob ang2 %d\n",
							setting );
				else {
				  if(mged_variables.rateknobs)
				    bu_vls_printf( &cmd , "knob z %f\n",
						   setting/2048.0 );
				  else
				    bu_vls_printf( &cmd , "knob az %f\n",
						   setting/512.0 );
				}
				break;
			case DIAL3:
				if(mged_variables.adcflag)
					bu_vls_printf( &cmd , "knob distadc %d\n",
							setting );
				else {
				  if(mged_variables.rateknobs)
				    bu_vls_printf( &cmd , "knob Z %f\n",
						   setting/2048.0 );
				  else
				    bu_vls_printf( &cmd , "knob aZ %f\n",
						   setting/512.0 );
				}
				break;
			case DIAL4:
				if(mged_variables.adcflag)
					bu_vls_printf( &cmd , "knob yadc %d\n",
							setting );
				else {
				  if(mged_variables.rateknobs)
				    bu_vls_printf( &cmd , "knob y %f\n",
						   setting/2048.0 );
				  else
				    bu_vls_printf( &cmd , "knob ay %f\n",
						   setting/512.0 );
				}
				break;
			case DIAL5:
			        if(mged_variables.rateknobs)
				  bu_vls_printf( &cmd , "knob Y %f\n",
						 setting/2048.0 );
				else
				  bu_vls_printf( &cmd , "knob aY %f\n",
						 setting/512.0 );
				break;
			case DIAL6:
				if(mged_variables.adcflag)
					bu_vls_printf( &cmd , "knob xadc %d\n",
							setting );
				else {
				  if(mged_variables.rateknobs)
				    bu_vls_printf( &cmd , "knob x %f\n",
						   setting/2048.0 );
				  else
				    bu_vls_printf( &cmd , "knob ax %f\n",
						   setting/512.0 );
				}
				break;
			case DIAL7:
			        if(mged_variables.rateknobs)
				  bu_vls_printf( &cmd , "knob X %f\n",
						 setting/2048.0 );
				else
				  bu_vls_printf( &cmd , "knob aX %f\n",
						 setting/512.0 );
				break;
			}
			continue;
		}
#endif
		switch( ret )  {
		case LEFTMOUSE:
			pending_button = 'L';
			pending_val = (int)valp[1] ? 1 : 0;
			break;
		case RIGHTMOUSE:
			pending_button = 'R';
			pending_val = (int)valp[1] ? 1 : 0;
			break;
		case MIDDLEMOUSE:
			/* Will be followed by MOUSEX and MOUSEY hits */
			pending_button = 'M';
			pending_val = (int)valp[1] ? 1 : 0;
			/* Don't signal DV_PICK until MOUSEY event */
			break;
		case MOUSEX:
			pending_x = irisX2ged( (int)valp[1] );
			if(mvars.debug>1)printf("mousex %d\n", pending_x);
			break;
		case MOUSEY:
			pending_y = irisY2ged( (int)valp[1] );
			if(mvars.debug>1)printf("mousey %d\n", pending_y);
			/*
			 *  Thanks to the tie() call, when a MIDDLEMOUSE
			 *  event is signalled, it will be (eventually)
			 *  followed by a MOUSEX and MOUSEY event.
			 *  The MOUSEY event may not be in the same
			 *  blkqread() buffer, owing to time delays in
			 *  the outputting of the tie()'ed events.
			 *  So, don't report the mouse event on MIDDLEMOUSE;
			 *  instead, the flag pending_val flag is set,
			 *  and the mouse event is signaled here, which
			 *  may need multiple trips through this subroutine.
			 *
			 *  MOUSEY may be queued all by itself to support
			 *  illuminate mode;  in those cases, pending_val
			 *  will be zero already, and we just parrot the last
			 *  X value.
			 */
			bu_vls_printf( &cmd, "%c %d %d %d\n",
			    pending_button,
			    pending_val,
			    pending_x, pending_y);
			pending_button = 'M';
			pending_val = 0;
			break;
		case REDRAW:
			/* Window may have moved */
			Ir_configure_window_shape();
			dmaflag = 1;
			if( mvars.doublebuffer) /* to fix back buffer */
				refresh();
			dmaflag = 1;
			break;
		case INPUTCHANGE:
			/* Means we got or lost the keyboard.  Ignore */
			break;
		case WMREPLY:
			/* This guy speaks, but has nothing to say */
			break;
		case 0:
			/* These show up as a consequence of using qgetfd().  Most regrettable.  Ignore. */
			break;
		default:
		  {
		    struct bu_vls tmp_vls;

		    bu_vls_init(&tmp_vls);
		    bu_vls_printf(&tmp_vls, "IRIS device %d gave %d?\n", ret, valp[1]);
		    Tcl_AppendResult(interp, bu_vls_addr(&tmp_vls), (char *)NULL);
		    bu_vls_free(&tmp_vls);
		  }
		  break;
		}
  }

  scroll_active = 0;
  (void)cmdline(&cmd, FALSE);
  bu_vls_free(&cmd);
  curr_dm_list = save_dm_list;
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
Ir_light( cmd, func )
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
Ir_cvtvecs( sp )
register struct solid *sp;
{
	return( 0 );	/* No "displaylist" consumed */
}

/*
 * Loads displaylist from storage[]
 */
unsigned
Ir_load( addr, count )
unsigned addr, count;
{
	return( 0 );		/* FLAG:  error */
}

void
Ir_statechange( a, b )
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
	default:
	  {
	    struct bu_vls tmp_vls;

	    bu_vls_init(&tmp_vls);
	    bu_vls_printf(&tmp_vls, "Ir_statechange: unknown state %s\n", state_str[b]);
	    Tcl_AppendResult(interp, bu_vls_addr(&tmp_vls), (char *)NULL);
	    bu_vls_free(&tmp_vls);
	  }
	  break;
	}
	Ir_viewchange( DM_CHGV_REDO, SOLID_NULL );
}

void
Ir_viewchange( cmd, sp )
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
Ir_debug(lvl)
{
	mvars.debug = lvl;
}

void
Ir_window(w)
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
Ir_colorchange()
{
	register int i;
	register int nramp;

	if( mvars.debug )
	  Tcl_AppendResult(interp, "colorchange\n", (char *)NULL);

	/* Program the builtin colors */
	ir_rgbtab[0].r=0; 
	ir_rgbtab[0].g=0; 
	ir_rgbtab[0].b=0;/* Black */
	ir_rgbtab[1].r=255; 
	ir_rgbtab[1].g=0; 
	ir_rgbtab[1].b=0;/* Red */
	ir_rgbtab[2].r=0; 
	ir_rgbtab[2].g=0; 
	ir_rgbtab[2].b=255;/* Blue */
	ir_rgbtab[3].r=255; 
	ir_rgbtab[3].g=255;
	ir_rgbtab[3].b=0;/*Yellow */
	ir_rgbtab[4].r = ir_rgbtab[4].g = ir_rgbtab[4].b = 255; /* White */
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

	ir_nslots = getplanes();
	if(mvars.cueing_on && (ir_nslots < 7)) {
	  Tcl_AppendResult(interp, "Too few bitplanes: depthcueing disabled\n");
	  mvars.cueing_on = 0;
	}
	ir_nslots = 1<<ir_nslots;
	if( ir_nslots > NSLOTS )  ir_nslots = NSLOTS;
	if(mvars.cueing_on) {
		/* peel off reserved ones */
		ir_nslots = (ir_nslots - CMAP_BASE) / CMAP_RAMP_WIDTH;
		depthcue(1);
	} else {
		ir_nslots -= CMAP_BASE;	/* peel off the reserved entries */
		depthcue(0);
	}

	ovec = -1;	/* Invalidate the old colormap entry */

	/* apply region-id based colors to the solid table */
	color_soltab();

	/* Map the colors in the solid table to colormap indices */
	ir_colorit();

	for( i=0; i < slotsused; i++ )  {
		gen_color( i, ir_rgbtab[i].r, ir_rgbtab[i].g, ir_rgbtab[i].b);
	}

	color(WHITE);	/* undefinied after gconfig() */
}

void
ir_colorit()
{
	register struct solid	*sp;
	register struct rgbtab *rgb;
	register int i;
	register int r,g,b;

	if( mvars.rgb )  return;

	FOR_ALL_SOLIDS (sp, &HeadSolid.l) {
		r = sp->s_color[0];
		g = sp->s_color[1];
		b = sp->s_color[2];
		if( (r == 255 && g == 255 && b == 255) ||
		    (r == 0 && g == 0 && b == 0) )  {
			sp->s_dmindex = DM_WHITE;
			continue;
		}

		/* First, see if this matches an existing color map entry */
		rgb = ir_rgbtab;
		for( i = 0; i < slotsused; i++, rgb++ )  {
			if( rgb->r == r && rgb->g == g && rgb->b == b )  {
				sp->s_dmindex = i;
				goto next;
			}
		}

		/* If slots left, create a new color map entry, first-come basis */
		if( slotsused < ir_nslots )  {
			rgb = &ir_rgbtab[i=(slotsused++)];
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

ir_dbtext(str)
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
	Tcl_AppendResult(interp, "dm-4d: You pressed Help key and '", str,
			 "'\n", (char *)NULL);
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
 *	gen_color also generates the colormap ramps.  Note that in depthcued
 *	mode, DM_BLACK uses map entry 0, and does not generate a ramp for it.
 *	Non depthcued mode skips the first CMAP_BASE colormap entries.
 *
 *	This routine is not called at all if ir_has_rgb is set.
 */
gen_color(c)
int c;
{
	if(mvars.cueing_on) {

		/*  Not much sense in making a ramp for DM_BLACK.  Besides
		 *  which, doing so, would overwrite the bottom color
		 *  map entries, which is a no-no.
		 */
		if( c != DM_BLACK) {
			register int i;
			fastf_t r_inc, g_inc, b_inc;
			fastf_t red, green, blue;

			r_inc = ir_rgbtab[c].r/16;
			g_inc = ir_rgbtab[c].g/16;
			b_inc = ir_rgbtab[c].b/16;

			red = ir_rgbtab[c].r;
			green = ir_rgbtab[c].g;
			blue = ir_rgbtab[c].b;

			for(i = 15; i >= 0;
			    i--, red -= r_inc, green -= g_inc, blue -= b_inc)
				mapcolor( MAP_ENTRY(c) + i,
				    (short)red,
				    (short)green,
				    (short)blue );
		}
	} else {
		mapcolor(c+CMAP_BASE,
		    ir_rgbtab[c].r, ir_rgbtab[c].g, ir_rgbtab[c].b);
	}
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
	    | (perspective_mode << 2)
	    | (mvars.zbuffer_on << 3);

	lampon(lights);
	lampoff(lights^0xf);
}
#endif

static void
establish_zbuffer()
{
	if( mvars.zbuf == 0 )  {
	  Tcl_AppendResult(interp, "dm-4d: This machine has no Zbuffer to enable\n", (char *)NULL);
		mvars.zbuffer_on = 0;
	}
	zbuffer( mvars.zbuffer_on );
	if( mvars.zbuffer_on)  {
		/* Set screen coords of near and far clipping planes */
		lsetdepth(mvars.min_scr_z, mvars.max_scr_z);
	}
	dmaflag = 1;
}

ir_clear_to_black()
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
 *  The structparse will change the value of the variable.
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
			Ir_colorchange();
		}

		mmode(MVIEWING);


		make_materials();	/* Define material properties */

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


make_materials()
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
sgi_has_stereo()
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
Ir_dm(argc, argv)
int	argc;
char	**argv;
{
	struct bu_vls	vls;
	struct bu_vls tmp_vls;

	if( argc < 1 )  return -1;

	/* For now, only "set" command is implemented */
	if( strcmp( argv[0], "set" ) != 0 )  {
	  Tcl_AppendResult(interp, "dm: command is not 'set'\n", (char *)NULL);
	  return TCL_ERROR;
	}

	bu_vls_init(&vls);
	bu_vls_init(&tmp_vls);
	start_catching_output(&tmp_vls);

	if( argc < 2 )  {
		/* Bare set command, print out current settings */
	  bu_struct_print( "dm_4d internal variables", Ir_vparse, (char *)&mvars );
		bu_log("%s", bu_vls_addr(&vls) );
	} else if( argc == 2 ) {
	  bu_vls_struct_item_named( &vls, Ir_vparse, argv[1], (char *)&mvars, ',');
		bu_log( "%s\n", bu_vls_addr(&vls) );
  	} else {
	        bu_vls_printf( &vls, "%s=\"", argv[1] );
	        bu_vls_from_argv( &vls, argc-2, argv+2 );
		bu_vls_putc( &vls, '\"' );
		bu_struct_parse( &vls, Ir_vparse, (char *)&mvars );
	}

	bu_vls_free(&vls);

	stop_catching_output(&tmp_vls);
	Tcl_AppendResult(interp, bu_vls_addr(&tmp_vls), (char *)NULL);
	bu_vls_free(&tmp_vls);
	return TCL_OK;
}

static void
ir_var_init()
{
  dm_vars = (char *)bu_malloc(sizeof(struct ir_vars), "ir_var_init: ir_vars");
  bzero((void *)dm_vars, sizeof(struct ir_vars));
  mvars.cueing_on = 1;
  mvars.zclipping_on = 1;
  mvars.zbuffer_on = 1;
  mvars.linewidth = 1;
  win = -1;
}
