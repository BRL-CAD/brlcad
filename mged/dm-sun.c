/*
 *			D M - S U N . C
 *
 *  Driver for SUN using SunView pixwins library.
 *
 *  Prerequisites -
 *	SUN 3.2.  Will not compile on 3.0 and below.
 *
 *  Author -
 *	Bill Lindemann
 *  
 *  Source -
 *	SUN Microsystems Inc.
 *	2550 Garcia Ave
 *	Mountain View, Ca 94043
 *  
 *  Copyright Notice -
 *	You're welcome to it.
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include <stdio.h>
#include "machine.h"
#include "vmath.h"
#include "mater.h"
#include "./ged.h"
#include "./dm.h"
#include "./solid.h"

#include <fcntl.h>
#include <signal.h>
#include <sunwindow/cms.h>
#include <sunwindow/cms_rgb.h>
#include <suntool/sunview.h>
#include <suntool/canvas.h>
#include <suntool/gfxsw.h>

extern void     perror();

/* Display Manager package interface */

#define SUNPWBOUND	1000.0	/* Max magnification in Rot matrix */
int             SunPw_open();
void            SunPw_close();
int             SunPw_input();
void            SunPw_prolog(), SunPw_epilog();
void            SunPw_normal(), SunPw_newrot();
void            SunPw_update();
void            SunPw_puts(), SunPw_2d_line(), SunPw_light();
int             SunPw_object();
unsigned        SunPw_cvtvecs(), SunPw_load();
void            SunPw_statechange(), SunPw_viewchange(), SunPw_colorchange();
void            SunPw_window(), SunPw_debug();

struct dm       dm_SunPw = {
			    SunPw_open, SunPw_close,
			    SunPw_input,
			    SunPw_prolog, SunPw_epilog,
			    SunPw_normal, SunPw_newrot,
			    SunPw_update,
			    SunPw_puts, SunPw_2d_line,
			    SunPw_light,
			    SunPw_object,
			    SunPw_cvtvecs, SunPw_load,
			    SunPw_statechange,
			    SunPw_viewchange,
			    SunPw_colorchange,
			    SunPw_window, SunPw_debug,
			    0,	/* no displaylist */
			    SUNPWBOUND,
			 "sun", "SunView 1.0/Sun release 3.x pixwin library"
};

void		input_eater();
extern struct device_values dm_values;	/* values read from devices */
static int	peripheral_input;	/* !0 => unprocessed input */

static vect_t   clipmin, clipmax;	/* for vector clipping */
static int	height, width;

Window          frame;
Canvas		canvas;
Pixwin		*sun_pw;		/* sub-area of screen being used */
int		sun_depth;
int             sun_cmap_color = 7;	/* for default colormap */
struct pixfont *sun_font_ptr;
Pr_texture     *sun_get_texP();
Rect            sun_win_rect;
int             sun_win_fd;
int             sun_debug = 0;

/*
 * Display coordinate conversion:
 *  GED is using -2048..+2048,
 *  We use 0..width, height..0.
 */
#define	GED_TO_SUNPWx(x) ((int)(((x)/4096.0+0.5)*width))
#define	GED_TO_SUNPWy(x) ((int)((0.5-(x)/4096.0)*height))
#define	SUNPWx_TO_GED(x) (((x)/(double)width-0.5)*4095)
#define	SUNPWy_TO_GED(x) (((x)/(double)height-0.5)*4095)

#define LABELING_FONT   "/usr/lib/fonts/fixedwidthfonts/sail.r.6"

static int doing_close;

static Notify_value
my_real_destroy(frame, status)
Frame	frame;
Destroy_status	status;
{
	if( status != DESTROY_CHECKING && !doing_close ) {
		doing_close++;
		quit();		/* Exit MGED */
	}
	/* Let frame get destroy event */
	return( notify_next_destroy_func(frame, status) );
}

/*
 *			S U N P W _ O P E N
 *
 * Fire up the display manager, and the display processor.
 *
 */
SunPw_open()
{
	char unsigned   red[256], grn[256], blu[256];
	struct pr_subregion bound;
	char	gfxwinname[128];

	width = height = 512;
	doing_close = 0;

	/*
	 * Make sure we are running under suntools.
	 * Gets the name of our parent.
	 * We could then open it, size it, create a subwindow within it,
	 * etc. but who cares.  We will make our own window.
	 */
	if( we_getgfxwindow(gfxwinname) ) {
		fprintf( stderr, "dm-sun: Must be running suntools\n" );
		return(-1);
	}

	/* Make a frame and a canvas to go in it */
	frame = window_create(NULL, FRAME,
			      FRAME_LABEL, "MGED Display", 0);
	/* XXX - "command line" args? pg.51 */
	canvas = window_create(frame, CANVAS,
			      WIN_WIDTH, width,
			      WIN_HEIGHT, height, 0);
	/* Fit window to canvas (width+20, height+20) */
	window_fit(frame);

	/* get the canvas pixwin to draw in */
	sun_pw = canvas_pixwin(canvas);
	sun_win_rect = *((Rect *)window_get(canvas, WIN_RECT));
	sun_depth = sun_pw->pw_pixrect->pr_depth;
	sun_win_fd = sun_pw->pw_clipdata->pwcd_windowfd;
/*printf("left,top,width,height=(%d %d %d %d)\n",
sun_win_rect.r_left, sun_win_rect.r_top,
sun_win_rect.r_width, sun_win_rect.r_height );*/

	/* Set up Canvas input */
	window_set( canvas, WIN_CONSUME_KBD_EVENT,
		WIN_TOP_KEYS, 0 );
	window_set( canvas, WIN_CONSUME_PICK_EVENT,
		WIN_MOUSE_BUTTONS, 0 );
	window_set( canvas, WIN_EVENT_PROC, input_eater, 0 );

	/*fcntl(sun_win_fd, F_SETFL, FNDELAY);*/

	/* Set up our Colormap */
	if( sun_depth >= 8 )  {
		/* set a new cms name; initialize it */
		pw_setcmsname(sun_pw, "mged");
		/* default colormap, real one is set by SunPw_colorchange */
		red[0] =   0; grn[0] =   0; blu[0] =   0;	/* Black */
		red[1] = 255; grn[1] =   0; blu[1] =   0;	/* Red */
		red[2] =   0; grn[2] = 255; blu[2] =   0;	/* Green */
		red[3] = 255; grn[3] = 255; blu[3] =   0;	/* Yellow */
		red[4] =   0; grn[4] =   0; blu[4] = 255;	/* Blue */
		red[5] = 255; grn[5] =   0; blu[5] = 255;	/* Magenta */
		red[6] =   0; grn[6] = 255; blu[6] = 255;	/* Cyan */
		red[7] = 255; grn[7] = 255; blu[7] = 255;	/* White */
		pw_putcolormap(sun_pw, 0, 8, red, grn, blu);
	} else {
		pw_blackonwhite( sun_pw, 0, 1 );
	}

	sun_clear_win();
	sun_font_ptr = pf_open(LABELING_FONT);
	if (sun_font_ptr == (struct pixfont *) 0) {
		perror(LABELING_FONT);
		return(-1);
	}
	pf_textbound(&bound, 1, sun_font_ptr, "X");

	notify_interpose_destroy_func(frame,my_real_destroy);

	window_set(frame, WIN_SHOW, TRUE, 0);	/* display it! */

	return (0);			/* OK */
}

/*
 *  			S U N P W _ C L O S E
 *  
 *  Gracefully release the display.
 *  Called on either a quit or release.
 */
void
SunPw_close()
{
	/*
	 * The window_destroy() call posts a destroy event, the same
	 * as if the user selected quit from the title bar.  We can
	 * interpose a destroy function to do an MGED release() or
	 * quit() in response to a title bar Quit, but both will
	 * result in calling this function, which will cause another
	 * destroy event, etc.!  There doesn't appear to be any way
	 * to REMOVE an interposed function.  So, we make this function
	 * do nothing if we have already been here or in destroy.
	 */
	if( !doing_close ) {
		doing_close++;
		window_set(frame, FRAME_NO_CONFIRM, TRUE, 0);
		window_destroy(frame);
	}
}

/*
 *			S U N _ C O L O R
 */
sun_color(color)
int	color;
{
	/* Note that DM_BLACK is only used for clearing to background */
	if( sun_depth < 8 )  {
		/*
		 * Sun Monochrome:  Following normal Sun usage,
		 * background (DM_BLACK) is white, from Sun color value 0,
		 * foreground (DM_WHITE) is black, from Sun color value 1.
		 * HOWEVER, in a graphics window, somehow this seems reversed.
		 */
		if( color == DM_BLACK )
			sun_cmap_color = 1;
		else
			sun_cmap_color = 0;
		/* Note that color value of 0 becomes -1 (foreground) (=1),
		 * according to pg 14 of Pixrect Ref Man (V.A or 17-Feb-86) */
		return;
	}
	switch (color) {
	case DM_BLACK:
		sun_cmap_color = 0;
		break;
	case DM_RED:
		sun_cmap_color = 1;
		break;
	case DM_BLUE:
		sun_cmap_color = 4;
		break;
	case DM_YELLOW:
		sun_cmap_color = 3;
		break;
	case DM_WHITE:
		sun_cmap_color = 7;
		break;
	default:
		printf("sun_color:  mged color %d not known\n", color);
		break;
	}
}

/*
 *			S U N P W _ P R O L O G
 *
 * There are global variables which are parameters to this routine.
 */
void
SunPw_prolog()
{
	if(!dmaflag)
		return;

	/* clear the screen for a redraw */
	sun_clear_win();

	/* Put the center point up, in foreground */
	sun_color(DM_WHITE);
	pw_put(sun_pw, GED_TO_SUNPWx(0), GED_TO_SUNPWy(0), 1);
}

/*
 *			S U N P W _ E P I L O G
 */
void
SunPw_epilog()
{
	return;
}

/*
 *  			S U N P W _ N E W R O T
 *  Stub.
 */
/* ARGSUSED */
void
SunPw_newrot(mat)
mat_t	mat;
{
	return;
}

static Pr_brush sun_whitebrush = { 4 };

/*
 *  			S U N P W _ O B J E C T
 *  
 *  Set up for an object, transformed as indicated, and with an
 *  object center as specified.  The ratio of object to screen size
 *  is passed in as a convienience.
 *
 *  Returns 0 if object could be drawn, !0 if object was omitted.
 */
/* ARGSUSED */
int
SunPw_object(sp, mat, ratio, white)
register struct solid *sp;
mat_t	mat;
double	ratio;
{
	register struct pr_pos *ptP;
	u_char         *mvP;
	static vect_t   pt;
	register struct vlist *vp;
	struct mater   *mp;
	int             nvec, totalvec;
	struct pr_pos   polylist[1024+1];
	u_char          mvlist[1024+1];
	Pr_texture     *texP;
	Pr_brush	*brush;
	int		color;

	if( white )  brush = &sun_whitebrush;
	else brush = (Pr_brush *)0;

	texP = sun_get_texP(sp->s_soldash);
	mp = (struct mater *) sp->s_materp;

	nvec = sp->s_vlen;
	ptP = polylist;
	mvP = mvlist;
	if (nvec > 1024)  {
		(void) fprintf(stderr, "SunPw_object: nvec %d clipped to 1024\n", nvec);
		nvec = 1024;
	}
	totalvec = 0;
	for( vp = sp->s_vlist; vp != VL_NULL; vp = vp->vl_forw )  {
		MAT4X3PNT(pt, mat, vp->vl_pnt);
		/* Visible range is +/- 1.0 */
		/* 2^31 ~= 2e9 -- dynamic range of a long int */
		/* 2^(31-11) = 2^20 ~= 1e6 */
		if( pt[0] < -1e6 || pt[0] > 1e6 ||
		    pt[1] < -1e6 || pt[1] > 1e6 )
			continue;		/* omit this point (ugh) */
		/* Integerize and let the Sun library do the clipping */
		ptP->x = GED_TO_SUNPWx(2048*pt[0]);
		ptP->y = GED_TO_SUNPWy(2048*pt[1]);
		ptP++;
		if (vp->vl_draw == 0)
			*mvP++ = 1;
		else
			*mvP++ = 0;
		totalvec++;
	}
	mvlist[0] = 0;
	if( sun_depth < 8 )  {
		sun_color( DM_WHITE );
		color = PIX_COLOR(sun_cmap_color);
	} else
		color = PIX_COLOR(mp->mt_dm_int);
	pw_polyline(sun_pw, 0, 0, totalvec, polylist, mvlist, brush,
	    texP, PIX_SRC | color );
	return(totalvec);	/* non-zero means it did something */
}

/*
 *			S U N P W _ N O R M A L
 *
 * Restore the display processor to a normal mode of operation
 * (ie, not scaled, rotated, displaced, etc).
 * Turns off windowing.
 */
void
SunPw_normal()
{
	return;
}

/*
 *			S U N P W _ U P D A T E
 *
 * Transmit accumulated displaylist to the display processor.
 */
void
SunPw_update()
{
	return;
}

/*
 *			S U N P W _ P U T S
 *
 * Output a string into the displaylist.
 * The starting position of the beam is as specified.
 */
/* ARGSUSED */
void
SunPw_puts(str, x, y, size, color)
register u_char *str;
{
	sun_color(color);
	pw_text(sun_pw, GED_TO_SUNPWx(x), GED_TO_SUNPWy(y),
		(PIX_NOT(PIX_SRC)) | PIX_COLOR(sun_cmap_color),
		sun_font_ptr, str);
}

/*
 *			S U N P W _ 2 D _ L I N E
 *
 */
void
SunPw_2d_line(x1, y1, x2, y2, dashed)
int	x1, y1;
int	x2, y2;
int	dashed;
{
	Pr_texture     *texP;

	sun_color(DM_YELLOW);
	texP = sun_get_texP(dashed);
	pw_line(sun_pw, GED_TO_SUNPWx(x1), GED_TO_SUNPWy(y1),
	    GED_TO_SUNPWx(x2), GED_TO_SUNPWy(y2), (Pr_brush *) 0, texP,
	    PIX_SRC | PIX_COLOR(sun_cmap_color));
}

/*
 *			S U N P W _ I N P U T
 *
 * Execution must suspend in this routine until a significant event
 * has occured on either the command stream,
 * unless "noblock" is set.
 *
 * Returns:
 *	0 if no command waiting to be read,
 *	1 if command is waiting to be read.
 */
#define	NBUTTONS	10
#define	ZOOM_BUTTON	1
#define	SLEW_BUTTON	2
#define	XY_ROT_BUTTON	3
#define	YZ_ROT_BUTTON	4
#define	XZ_ROT_BUTTON	5
int	sun_buttons[NBUTTONS];

/*
 *			S U N P W _ I N P U T
 *
 * Execution must suspend in this routine until a significant event
 * has occured on either the command stream, or a device event has
 * occured, unless "noblock" is set.
 *
 * We need to call notify_dispatch() regularly so that window events
 * such as say title-bar actions can be processed.  Button and mouse
 * events actually get sent to input_eater() via the notifier.  We
 * use a flag peripheral_input, for input_eater() to tell this module
 * when there had been window input to return.
 *
 * We might be able to replace the short select timeout polling by
 * notify_do_dispatch() coupled with notify_stop() in input_eater().
 * XXX Try this. See pg 266.
 *
 * Returns:
 *	0 if no command waiting to be read,
 *	1 if command is waiting to be read.
 */
SunPw_input(cmd_fd, noblock)
int	cmd_fd;
int	noblock;	/* !0 => poll */
{
	long            readfds;
	struct timeval  timeout;
	int             numfds;

	/*
	 * Check for input on the keyboard or on the polled registers. 
	 *
	 * Suspend execution until either
	 *  1)  User types a full line
	 *  2)	A change in peripheral status occurs
	 *  3)  The timelimit on SELECT has expired 
	 *
	 * If a RATE operation is in progress (zoom, rotate, slew)
	 * in which the peripherals (rate setting) may not have changed,
	 * but we still have to update the display,
	 * do not suspend execution.
	 */
	if (noblock) {
		timeout.tv_sec = 0;
		timeout.tv_usec = 0;
	} else {
		timeout.tv_sec = 0;
		timeout.tv_usec = 250000;	/* 1/4 second */
	}
	for( ;; ) {
		readfds = (1 << cmd_fd) | (1 << sun_win_fd);
		numfds = select(32, &readfds, (long *)0, (long *)0, &timeout);
/*printf("num = %d, val = %d (cmd %d, win %d)\n",
	numfds, readfds, 1<<cmd_fd, 1<<sun_win_fd );*/
		(void) notify_dispatch();
		if( readfds & (1 << cmd_fd) ) {
/*printf("Returning command\n");*/
			return (1);		/* command awaits */
		} else if( peripheral_input ) {
			peripheral_input = 0;
/*printf("Returning peripherals\n");*/
			return (0);		/* just peripheral stuff */
		}
		if( noblock )
			return (0);
	}
}

/*
 *  This gets called by the notifier whenever device input that
 *  we asked for comes in (and some that we didn't ask for).
 */
void
input_eater( win, event, arg )
Window	win;
Event	*event;
caddr_t	*arg;
{
	int             id;
	int             button;
	float           xval;
	float           yval;

	dm_values.dv_penpress = 0;
	id = event_id(event);
	dm_values.dv_xpen = SUNPWx_TO_GED(event_x(event));
	dm_values.dv_ypen = -SUNPWy_TO_GED(event_y(event));
/*printf( "Event %d at (%d %d)\n", id, event_x(event), event_y(event));*/
	switch(id) {
	case MS_LEFT:
		if (event_is_down(event))
			dm_values.dv_penpress = DV_OUTZOOM;
		break;
	case MS_MIDDLE:
		if (event_is_down(event))
			dm_values.dv_penpress = DV_PICK;
		break;
	case MS_RIGHT:
		if (event_is_down(event))
			dm_values.dv_penpress = DV_INZOOM;
		break;
	case LOC_DRAG:
		break;
	case LOC_MOVE:
		xval = (float) dm_values.dv_xpen / 2048.;
		if (xval < -1.0)
			xval = -1.0;
		if (xval > 1.0)
			xval = 1.0;
		yval = (float) dm_values.dv_ypen / 2048.;
		if (yval < -1.0)
			yval = -1.0;
	    	if (yval > 1.0)
			yval = 1.0;
		for (button = 0; button < NBUTTONS; button++) {
			if (sun_buttons[button]) {
				switch(button) {
				case ZOOM_BUTTON:
					dm_values.dv_zoom = ((xval*xval) + (yval*yval)) / 2;
					if (xval < 0)
					    dm_values.dv_zoom = -dm_values.dv_zoom;
					break;
				case SLEW_BUTTON:
					dm_values.dv_xslew = xval;
					dm_values.dv_yslew = yval;
					break;
				case XY_ROT_BUTTON:
					dm_values.dv_xjoy = xval;
					dm_values.dv_yjoy = yval;
					break;
				case YZ_ROT_BUTTON:
					dm_values.dv_yjoy = yval;
					dm_values.dv_zjoy = xval;
					break;
				case XZ_ROT_BUTTON:
					dm_values.dv_xjoy = xval;
					dm_values.dv_zjoy = yval;
					break;
				}
			}
		}
		break;
	case KEY_TOP(ZOOM_BUTTON):
		sun_key(event, ZOOM_BUTTON);
		dm_values.dv_zoom = 0.0;
		break;
	case KEY_TOP(SLEW_BUTTON):
		sun_key(event, SLEW_BUTTON);
		dm_values.dv_xslew = 0.0;
		dm_values.dv_yslew = 0.0;
		break;
	case KEY_TOP(XY_ROT_BUTTON):
		sun_key(event, XY_ROT_BUTTON);
		dm_values.dv_xjoy = 0.0;
		dm_values.dv_yjoy = 0.0;
		dm_values.dv_zjoy = 0.0;
		break;
	case KEY_TOP(YZ_ROT_BUTTON):
		sun_key(event, YZ_ROT_BUTTON);
		dm_values.dv_xjoy = 0.0;
		dm_values.dv_yjoy = 0.0;
		dm_values.dv_zjoy = 0.0;
		break;
	case KEY_TOP(XZ_ROT_BUTTON):
		sun_key(event, XZ_ROT_BUTTON);
		dm_values.dv_xjoy = 0.0;
		dm_values.dv_yjoy = 0.0;
		dm_values.dv_zjoy = 0.0;
		break;
	case KEY_TOP(6):
		break;
	case KEY_TOP(7):
		break;
	case KEY_TOP(8):
		break;
	case KEY_TOP(9):
		break;
	/*
	 * Gratuitous Input Events - supposed to be good for you
	 */
	case WIN_REPAINT:
		dmaflag = 1;	/* causes refresh */
		break;
	case WIN_RESIZE:
		height = (int)window_get(canvas,CANVAS_HEIGHT);
		width = (int)window_get(canvas,CANVAS_WIDTH);
		sun_pw = canvas_pixwin(canvas);
		sun_win_rect = *((Rect *)window_get(canvas, WIN_RECT));
		window_default_event_proc( win, event, arg );
		break;
	case LOC_WINENTER:
	case LOC_WINEXIT:
	case LOC_RGNENTER:
	case LOC_RGNEXIT:
	case KBD_USE:
	case KBD_DONE:
		/* pass them on */
		/*printf("*** Gratuity ***\n");*/
		window_default_event_proc( win, event, arg );
		break;
	default:
		printf("*** Default event ***\n");
		window_default_event_proc( win, event, arg );
		break;
	}

	peripheral_input++;
}

sun_key(eventP, button)
Event	*eventP;
int	button;
{
	if (event_is_down(eventP)) {
		sun_buttons[button] = 1;
		window_set(frame,WIN_CONSUME_PICK_EVENT,LOC_MOVE,0);
	} else {
		sun_buttons[button] = 0;
		/*window_set(frame,WIN_IGNORE_PICK_EVENT,LOC_MOVE,0);*/
	}
}

/* 
 *			S U N P W _ L I G H T
 */
/* ARGSUSED */
void
SunPw_light(cmd, func)
int	cmd;
int	func;		/* BE_ or BV_ function */
{
	return;
}

/* ARGSUSED */
unsigned
SunPw_cvtvecs(sp)
struct solid   *sp;
{
	return (0);
}

/*
 * Loads displaylist
 */
unsigned
SunPw_load(addr, count)
unsigned	addr, count;
{
	(void) printf("SunPw_load(x%x, %d.)\n", addr, count);
	return (0);
}

void
SunPw_statechange()
{
}

void
SunPw_viewchange()
{
}

/*
 * Color Map table
 */
#define NSLOTS	256
static int      sun_nslots = 0;	/* how many we have, <= NSLOTS */
static int      slotsused;	/* how many actually used */
static struct rgbtab {
	unsigned char   r;
	unsigned char   g;
	unsigned char   b;
} sun_rgbtab[NSLOTS];

/*
 *  			S U N P W _ C O L O R C H A N G E
 *  
 *  Go through the mater table, and allocate color map slots.
 *	8 bit system gives 4 or 8,
 *	24 bit system gives 12 or 24.
 */
void
SunPw_colorchange()
{
    register struct mater *mp;
    register int    i;
    char unsigned   red[256], grn[256], blu[256];

    if (!sun_nslots)
    {
	sun_nslots = 256;	/* ## hardwire cg2, for now */
	if (sun_nslots > NSLOTS)
	    sun_nslots = NSLOTS;
    }
    sun_rgbtab[0].r = 0;
    sun_rgbtab[0].g = 0;
    sun_rgbtab[0].b = 0;	/* Black */
    sun_rgbtab[1].r = 255;
    sun_rgbtab[1].g = 0;
    sun_rgbtab[1].b = 0;	/* Red */
    sun_rgbtab[2].r = 0;
    sun_rgbtab[2].g = 0;
    sun_rgbtab[2].b = 255;	/* Blue */
    sun_rgbtab[3].r = 255;
    sun_rgbtab[3].g = 255;
    sun_rgbtab[3].b = 0;	/* Yellow */
    sun_rgbtab[4].r = sun_rgbtab[4].g = sun_rgbtab[4].b = 255;	/* White */
    sun_rgbtab[5].g = 255;	/* Green */
    sun_rgbtab[6].r = sun_rgbtab[6].g = sun_rgbtab[6].b = 128;	/* Grey */
    sun_rgbtab[7].r = sun_rgbtab[7].g = sun_rgbtab[7].b = 255;	/* White TEXT */

    slotsused = 8;
    for (mp = MaterHead; mp != MATER_NULL; mp = mp->mt_forw)
	sun_colorit(mp);
    if (sun_debug)
	printf("colorchange, %d slots used\n", slotsused);

    color_soltab();		/* apply colors to the solid table */

    for (i = 0; i < slotsused; i++)
    {
	red[i] = sun_rgbtab[i].r;
	grn[i] = sun_rgbtab[i].g;
	blu[i] = sun_rgbtab[i].b;
    }
    pw_putcolormap(sun_pw, 0, slotsused, red, grn, blu);
}

int             sun_colorit(mp)
struct mater   *mp;
{
    register struct rgbtab *rgb;
    register int    i;
    register int    r, g, b;

    r = mp->mt_r;
    g = mp->mt_g;
    b = mp->mt_b;
    if ((r == 255 && g == 255 && b == 255) ||
	(r == 0 && g == 0 && b == 0))
    {
	mp->mt_dm_int = DM_WHITE;
	return;
    }

    /* First, see if this matches an existing color map entry */
    rgb = sun_rgbtab;
    for (i = 0; i < slotsused; i++, rgb++)
    {
	if (rgb->r == r && rgb->g == g && rgb->b == b)
	{
	    mp->mt_dm_int = i;
	    return;
	}
    }

    /* If slots left, create a new color map entry, first-come basis */
    if (slotsused < sun_nslots)
    {
	rgb = &sun_rgbtab[i = (slotsused++)];
	rgb->r = r;
	rgb->g = g;
	rgb->b = b;
	mp->mt_dm_int = i;
	return;
    }
    mp->mt_dm_int = DM_YELLOW;	/* Default color */
}

/* ARGSUSED */
void
SunPw_debug(lvl)
{
	sun_debug = lvl;
	return;
}

void
SunPw_window(w)
register int    w[];
{
	/* Compute the clipping bounds */
	clipmin[0] = w[1] / 2048.;
	clipmin[1] = w[3] / 2048.;
	clipmin[2] = w[5] / 2048.;
	clipmax[0] = w[0] / 2047.;
	clipmax[1] = w[2] / 2047.;
	clipmax[2] = w[4] / 2047.;
}

sun_clear_win()
{
	sun_color(DM_BLACK);
	/* Clear entire window, regardless of how much is being used */
	pw_writebackground(sun_pw, 0, 0,
		sun_win_rect.r_width, sun_win_rect.r_height,
		PIX_SRC | PIX_COLOR(sun_cmap_color) );
}

Pr_texture
*sun_get_texP(dashed)
int	dashed;
{
	static Pr_texture dotdashed = {
		pr_tex_dashdot, 0, { 1, 1, 0, 0, } };

	if (dashed)
		return (&dotdashed);
	else
		return ((Pr_texture *) 0);
}
