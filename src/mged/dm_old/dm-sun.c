/*                        D M - S U N . C
 * BRL-CAD
 *
 * Copyright (C) 1988-2005 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this file; see the file named COPYING for more
 * information.
 */
/** @file dm-sun.c
 *
 *  Driver for SUN using SunView pixwins library.
 *
 *  Prerequisites -
 *	SUN 3.2.  Will not compile on 3.0 and below.
 *
 *  Author -
 *	Bill Lindemann, SUN - original pixwin/pixrect version.
 *	Phillip Dykstra, BRL - SunView version.
 *
 *  Source -
 *	SUN Microsystems Inc.
 *	2550 Garcia Ave
 *	Mountain View, Ca 94043
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "common.h"



#include <stdio.h>
#include "machine.h"
#include "bu.h"
#include "vmath.h"
#include "mater.h"
#include "raytrace.h"
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

/*
 * Max number of color map slots to use (don't consume all 256)
 */
#define	CMS_MGED	"mged"
#define	CMS_MGEDSIZE	128

/* Display Manager package interface */

#define SUNPWBOUND	1000.0	/* Max magnification in Rot matrix */
int             SunPw_open();
void            SunPw_close();
MGED_EXTERN(void	SunPw_input, (fd_set *input, int noblock) );
void            SunPw_prolog(), SunPw_epilog();
void            SunPw_normal(), SunPw_newrot();
void            SunPw_update();
void            SunPw_puts(), SunPw_2d_line(), SunPw_light();
int             SunPw_object();
unsigned        SunPw_cvtvecs(), SunPw_load();
void            SunPw_statechange(), SunPw_viewchange(), SunPw_colorchange();
void            SunPw_window(), SunPw_debug();

struct dm dm_SunPw = {
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
	0,			/* no displaylist */
	0,			/* Multiwindow */
	SUNPWBOUND,
	"sun", "SunView Release 3.x pixwin library"
};

void		input_eater();
extern struct device_values dm_values;	/* values read from devices */
static int	peripheral_input;	/* !0 => unprocessed input */
static int	mouse_motion;		/* !0 => MGED pen tracking mode */

static vect_t   clipmin, clipmax;	/* for vector clipping */
static int	height, width;

Window          frame;
Canvas		canvas;
Pixwin		*sun_pw;		/* sub-area of screen being used */
int		sun_depth;
int             sun_cmap_color = DM_WHITE+1;	/* for default colormap */
struct pixfont *sun_font_ptr;
Pr_texture     *sun_get_texP();
Rect            sun_win_rect;
int             sun_win_fd;
int             sun_debug = 0;

static short icon_image[] = {
#include "./sunicon.h"
};
DEFINE_ICON_FROM_IMAGE(icon, icon_image);

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
	 */
	if( we_getgfxwindow(gfxwinname) ) {
		bu_log( "dm-sun: Must be running suntools\n" );
		return(-1);
	}

	/* Make a frame and a canvas to go in it */
	frame = window_create(NULL, FRAME,
			      FRAME_ICON, &icon,
			      FRAME_LABEL, "MGED Display", 0);
	/* XXX - "command line" args? pg.51 */
	canvas = window_create(frame, CANVAS,
			      WIN_WIDTH, width,
			      WIN_HEIGHT, height, 0);
	/* Fit window to canvas (width+10, height+20) */
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
	window_set( canvas,
		WIN_CONSUME_PICK_EVENT, WIN_MOUSE_BUTTONS,
		WIN_CONSUME_KBD_EVENT, WIN_TOP_KEYS,
		WIN_CONSUME_KBD_EVENT, WIN_UP_EVENTS,
		WIN_EVENT_PROC, input_eater, 0 );

	/* Set up our Colormap */
	if( sun_depth >= 8 )  {
		/* set a colormap segment; initialize it */
		SunPw_colorchange();
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
 *
 *  Sets the global "sun_cmap_color" to the index of the requested color.
 */
sun_color(color)
int	color;
{
	/* Note that DM_BLACK is only used for clearing to background */
	if( sun_depth < 8 )  {
		/*
		 * Sun Monochrome:  Following normal Sun usage,
		 * "black" means "background" which is the lowest entry
		 * in your colormap segment.
		 * "white" means "foreground" which is the hightest entry.
		 * Which one is actually black or white depends on whether
		 * you are in "inverse video" or not.
		 *
		 * Note: foreground would have been index CMS_MGEDSIZE-2,
		 *  but this is the monochrome case so that does't work.
		 */
		if( color == DM_BLACK )
			sun_cmap_color = 0;	/* background */
		else
			sun_cmap_color = 1;	/* foreground */
		return;
	}
	switch (color) {
	case DM_BLACK:
		sun_cmap_color = 1;
		break;
	case DM_RED:
		sun_cmap_color = 2;
		break;
	case DM_BLUE:
		sun_cmap_color = 3;
		break;
	case DM_YELLOW:
		sun_cmap_color = 4;
		break;
	case DM_WHITE:
		sun_cmap_color = 5;
		break;
	default:
		bu_log("sun_color:  mged color %d not known\n", color);
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

	/* draw in the memory pixrect, not the window */
	pw_batch_on(sun_pw);

	/* clear the screen for a redraw */
	sun_clear_win();

	/* Put the center point up, in foreground */
	sun_color(DM_WHITE);
	pw_put(sun_pw, GED_TO_SUNPWx(0), GED_TO_SUNPWy(0),
		sun_cmap_color);
}

/*
 *			S U N P W _ E P I L O G
 */
void
SunPw_epilog()
{
	/* flush everything to the window */
	pw_batch_off(sun_pw);
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
#define	MAXVEC	2048
/* ARGSUSED */
int
SunPw_object(sp, mat, ratio, white)
struct solid	*sp;
mat_t	mat;
double	ratio;
{
	register struct rt_vlist *vp;
	register struct pr_pos	*ptP;		/* Sun point list */
	register u_char		*mvP;		/* Sun move/draw list */
	struct pr_pos   ptlist[MAXVEC];		/* Sun point buffer */
	u_char          mvlist[MAXVEC];		/* Sun move/draw buffer */
	int             numvec;			/* number of points */
	static vect_t   pnt;			/* working point */
	Pr_texture	*texP;			/* line style/color */
	Pr_brush	*brush;
	int		color;

	ptP = ptlist;
	mvP = mvlist;
	numvec = 0;
	for( BU_LIST_FOR( vp, rt_vlist, &(sp->s_vlist) ) )  {
		register int	i;
		register int	nused = vp->nused;
		register int	*cmd = vp->cmd;
		register point_t *pt = vp->pt;

		/* Viewing region is from -1.0 to +1.0 */
		/* 2^31 ~= 2e9 -- dynamic range of a long int */
		/* 2^(31-11) = 2^20 ~= 1e6 */
		/* Integerize and let Sun do the clipping */
		for( i = 0; i < nused; i++,cmd++,pt++ )  {
			static vect_t	start, fin;
			switch( *cmd )  {
			case RT_VLIST_POLY_START:
			case RT_VLIST_POLY_VERTNORM:
				continue;
			case RT_VLIST_POLY_MOVE:
			case RT_VLIST_LINE_MOVE:
				/* Move, not draw */
				MAT4X3PNT( pnt, mat, *pt );
				if( pnt[0] < -1e6 || pnt[0] > 1e6 ||
				    pnt[1] < -1e6 || pnt[1] > 1e6 )
					continue; /* omit this point (ugh) */
				/* Integerize and let the Sun library do the clipping */
				ptP->x = GED_TO_SUNPWx(2048*pnt[0]);
				ptP->y = GED_TO_SUNPWy(2048*pnt[1]);
				ptP++;
				*mvP++ = 1;
				break;
			case RT_VLIST_POLY_DRAW:
			case RT_VLIST_POLY_END:
			case RT_VLIST_LINE_DRAW:
				/* draw */
				MAT4X3PNT( pnt, mat, *pt );
				if( pnt[0] < -1e6 || pnt[0] > 1e6 ||
				    pnt[1] < -1e6 || pnt[1] > 1e6 )
					continue; /* omit this point (ugh) */
				/* Integerize and let the Sun library do the clipping */
				ptP->x = GED_TO_SUNPWx(2048*pnt[0]);
				ptP->y = GED_TO_SUNPWy(2048*pnt[1]);
				ptP++;
				*mvP++ = 0;
				break;
			}
			if( ++numvec >= MAXVEC ) {
				(void)bu_log(
					"SunPw_object: nvec %d clipped to %d\n",
					sp->s_vlen, numvec );
				break;
			}
		}
	}

	/* get line style/color etc */
	if( white )
		brush = &sun_whitebrush;
	else
		brush = (Pr_brush *)0;
	texP = sun_get_texP(sp->s_soldash);
	if( sun_depth < 8 ) {
		sun_color( DM_WHITE );
		color = PIX_COLOR(sun_cmap_color);
	} else {
		color = PIX_COLOR(sp->s_dmindex);
	}

	pw_polyline( sun_pw, 0, 0, numvec, ptlist, mvlist,
		brush, texP, PIX_SRC|color );

	return(numvec);		/* non-zero means it did something */
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
 *  Output a string into the displaylist.
 *  The starting position of the beam is as specified.
 *
 *  MGED starts edit messages with a TAB.  We get back at it
 *  here.  It's yucky, but better than printing a blotch.
 */
/* ARGSUSED */
void
SunPw_puts(str, x, y, size, color)
register u_char *str;
{
	if( str[0] == 9 ) {
		str[0] = ' ';
		x += 10;
	}

	sun_color(color);
	if( sun_depth == 1 ) {
		pw_text(sun_pw, GED_TO_SUNPWx(x), GED_TO_SUNPWy(y),
			PIX_SRC,
			sun_font_ptr, str);
	} else {
		pw_ttext(sun_pw, GED_TO_SUNPWx(x), GED_TO_SUNPWy(y),
			PIX_SRC | PIX_COLOR(sun_cmap_color),
			sun_font_ptr, str);
	}
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
 * has occured on either the command stream, or a device event has
 * occured, unless "noblock" is set.
 *
 * We need to call notify_dispatch() regularly so that window events
 * such as say title-bar actions can be processed.  Button and mouse
 * events actually get sent to input_eater() via the notifier.  We
 * use a flag peripheral_input, for input_eater() to tell this module
 * when there had been window input to return.
 *
 * I used to use a long select timeout (60 sec) after a notify_do_dispatch()
 * call (to dispatch events inside of the select) and counted on a
 * notify_stop() in the input_eater() routine to wake the select up.
 * [This is the way we *should* do it, right? (See pg 266)]
 * It appears though that some input events get lost this way.  Perhaps
 * notify_stop() may be intended more as an abort and thus isn't careful.
 * The final straw was SIGINT on command input was causing window fd
 * input that notify_dispatch was never dispatching!  So, I finally
 * went back to a short timeout select loop and avoided the do_dispatch
 * and stop business.  It works better this way.
 *
 *
 * Implicit Return -
 *	If any files are ready for input, their bits will be set in 'input'.
 *	Otherwise, 'input' will be all zeros.
 */
void
SunPw_input( input, noblock)
fd_set		*input;
int		noblock;	/* !0 => poll */
{
	struct timeval  tv;
	fd_set		files;
	int		width;
	int		cnt;
	int		i;

	if( (width = getdtablesize()) <= 0 )
		width = 32;
	files = *input;		/* save, for restore on each loop */

	FD_SET( sun_win_fd, &files );

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
		tv.tv_sec = 0;
		tv.tv_usec = 0;
	} else {
		tv.tv_sec = 0;
		tv.tv_usec = 200000;
	}
	for( ;; ) {
		*input = files;
		cnt = select( width, input, (fd_set *)0,  (fd_set *)0, &tv );
		if( cnt < 0 )  {
			perror("dm-sun.c/select");
			break;
		}

		(void) notify_dispatch();
		if( peripheral_input ) {
			/*printf("Returning peripherals\n");*/
			peripheral_input = 0;
			return;		/* just peripheral stuff */
		}
		if( FD_SET(sun_win_fd, input) ) {
			/* check for more input events before redisplay */
			/*printf("Loop d'loop\n");*/
			continue;
		}
		if( noblock ) {
			/*printf("Returning noblock\n");*/
			return;
		}
		for( i=0; i<width; i++ )  {
			if( FD_ISSET(i, input) )  return;
		}
	}
}

#define	NBUTTONS	10
#define	ZOOM_BUTTON	1	/* F1 */
#define	Z_SLEW_BUTTON	2
#define	X_SLEW_BUTTON	3
#define	Y_SLEW_BUTTON	4
#define	X_ROT_BUTTON	5
#define	Y_ROT_BUTTON	6
#define	Z_ROT_BUTTON	7
int	sun_buttons[NBUTTONS];

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
	int		xpen, ypen;

	id = event_id(event);
	xpen = SUNPWx_TO_GED(event_x(event));
	ypen = -SUNPWy_TO_GED(event_y(event));
	if( sun_debug )
		bu_log("Event %d at (%d %d)\n",id,event_x(event),event_y(event));
	switch(id) {
	case MS_LEFT:
		if (event_is_down(event)) {
			bu_vls_strcat( &dm_values.dv_string , "zoom 2\n" );
			peripheral_input++;
		}
		break;
	case MS_MIDDLE:
		if (event_is_down(event)) {
			bu_vls_printf( &dm_values.dv_string , "M 1 %d %d\n" , xpen, ypen );
			peripheral_input++;
		}
		break;
	case MS_RIGHT:
		if (event_is_down(event)) {
			bu_vls_strcat( &dm_values.dv_string , "zoom 0.5\n" );
			peripheral_input++;
		}
		break;
	case LOC_DRAG:
		break;
	case LOC_MOVE:
		xval = (float) xpen / 2048.;
		if (xval < -1.0)
			xval = -1.0;
		if (xval > 1.0)
			xval = 1.0;
		yval = (float) ypen / 2048.;
		if (yval < -1.0)
			yval = -1.0;
	    	if (yval > 1.0)
			yval = 1.0;
		for( button = 0; button < NBUTTONS; button++ ) {
			char str_buf[128];

			if( sun_buttons[button] ) {
				peripheral_input++;
				switch(button) {
				case ZOOM_BUTTON:
					{
						float zoom;

						zoom = (yval*yval) / 2;
						if (yval < 0)
						    zoom = -zoom;
						sprintf( str_buf , &dm_values.dv_string, "zoom %f\n", zoom );
						bu_vls_strcat( &dm_values.dv_string, str_buf );
					}
					break;
				case X_SLEW_BUTTON:
					sprintf( str_buf, "knob X %f\n", xval );
					bu_vls_strcat( &dm_values.dv_string, str_buf );
					break;
				case Y_SLEW_BUTTON:
					sprintf( str_buf, "knob Y %f\n", yval );
					bu_vls_strcat( &dm_values.dv_string, str_buf );
					break;
				case Z_SLEW_BUTTON:
					sprintf( str_buf, "knob Z %f\n", yval );
					bu_vls_strcat( &dm_values.dv_string, str_buf );
					break;
				case X_ROT_BUTTON:
					sprintf( str_buf, "knob x %f\n", xval );
					bu_vls_strcat( &dm_values.dv_string, str_buf );
					break;
				case Y_ROT_BUTTON:
					sprintf( str_buf, "knob y %f\n", yval );
					bu_vls_strcat( &dm_values.dv_string, str_buf );
					break;
				case Z_ROT_BUTTON:
					sprintf( str_buf, "knob z %f\n", yval );
					bu_vls_strcat( &dm_values.dv_string, str_buf );
					break;
				}
			}
		}
		if( mouse_motion )
			peripheral_input++;	/* MGED wants to know */
		break;
	case KEY_TOP(ZOOM_BUTTON):
		sun_key(event, ZOOM_BUTTON);
		break;
	case KEY_TOP(X_SLEW_BUTTON):
		sun_key(event, X_SLEW_BUTTON);
		break;
	case KEY_TOP(Y_SLEW_BUTTON):
		sun_key(event, Y_SLEW_BUTTON);
		break;
	case KEY_TOP(Z_SLEW_BUTTON):
		sun_key(event, Z_SLEW_BUTTON);
		break;
	case KEY_TOP(X_ROT_BUTTON):
		sun_key(event, X_ROT_BUTTON);
		break;
	case KEY_TOP(Y_ROT_BUTTON):
		sun_key(event, Y_ROT_BUTTON);
		break;
	case KEY_TOP(Z_ROT_BUTTON):
		sun_key(event, Z_ROT_BUTTON);
		break;
	/*
	 * Gratuitous Input Events - supposed to be good for you
	 */
	case WIN_REPAINT:
		/* The canvas does the repaint for us */
		window_default_event_proc( win, event, arg );
		break;
	case WIN_RESIZE:
		dmaflag = 1;	/* causes refresh */
		height = (int)window_get(canvas,CANVAS_HEIGHT);
		width = (int)window_get(canvas,CANVAS_WIDTH);
		sun_pw = canvas_pixwin(canvas);
		sun_win_rect = *((Rect *)window_get(canvas, WIN_RECT));
		window_default_event_proc( win, event, arg );
		peripheral_input++;
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
		/*printf("*** Default event ***\n");*/
		window_default_event_proc( win, event, arg );
		break;
	}

	/*
	 * Used to do a notify_stop() here to wake up the
	 * bsdselect() if blocked, but this seems to be unsafe
	 * (some input events were lost).  So, we just use a
	 * short select timeout now and wait for it.
	 */
}

sun_key(eventP, button)
Event	*eventP;
int	button;
{
	if (event_is_down(eventP)) {
		sun_buttons[button] = 1;
		/*window_set(canvas,WIN_CONSUME_PICK_EVENT,LOC_MOVE,0);*/
	} else {
		sun_buttons[button] = 0;
		/*window_set(canvas,WIN_IGNORE_PICK_EVENT,LOC_MOVE,0);*/
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
	bu_log("SunPw_load(x%x, %d.)\n", addr, count);
	return (0);
}

void
SunPw_statechange( a, b )
int	a, b;
{
	/*
	 *  Based upon new state, possibly do extra stuff,
	 *  including enabling continuous tablet tracking,
	 *  object highlighting
	 */
	switch( b )  {
	case ST_VIEW:
		/* constant tracking OFF */
		mouse_motion = 0;
		break;
	case ST_S_PICK:
	case ST_O_PICK:
	case ST_O_PATH:
		/* constant tracking ON */
		mouse_motion = 1;
		break;
	case ST_O_EDIT:
	case ST_S_EDIT:
		/* constant tracking OFF */
		mouse_motion = 0;
		break;
	default:
		bu_log("SunPw_statechange: unknown state %s\n", state_str[b]);
		break;
	}
	/*SunPw_viewchange( DM_CHGV_REDO, SOLID_NULL );*/
}

void
SunPw_viewchange()
{
}

/*
 * Color Map table
 */
static int sun_nslots = CMS_MGEDSIZE-2;	/* how many we have (static) */
static int slotsused = 0;		/* how many actually used */
static struct cmap {
	unsigned char   r;
	unsigned char   g;
	unsigned char   b;
} sun_cmap[CMS_MGEDSIZE];

/*
 *  			S U N P W _ C O L O R C H A N G E
 *  
 *  Go through the mater table, and allocate color map slots.
 */
void
SunPw_colorchange()
{
	register struct mater *mp;
	register int    i;
	char unsigned   red[256], grn[256], blu[256];

	if( slotsused == 0 ) {
		/* create a colormap segment */
		pw_setcmsname(sun_pw, CMS_MGED);
	}

	sun_cmap[0].r = 0;
	sun_cmap[0].g = 0;
	sun_cmap[0].b = 0;	/* Suntools Background */

	sun_cmap[1].r = 0;
	sun_cmap[1].g = 0;
	sun_cmap[1].b = 0;	/* DM_BLACK */
	sun_cmap[2].r = 255;
	sun_cmap[2].g = 0;
	sun_cmap[2].b = 0;	/* DM_RED */
	sun_cmap[3].r = 0;
	sun_cmap[3].g = 0;
	sun_cmap[3].b = 255;	/* DM_BLUE */
	sun_cmap[4].r = 255;
	sun_cmap[4].g = 255;
	sun_cmap[4].b = 0;	/* DM_YELLOW */
	sun_cmap[5].r = 255;
	sun_cmap[5].g = 255;
	sun_cmap[5].b = 255;	/* DM_WHITE */
	sun_cmap[6].r = 0;
	sun_cmap[6].g = 255;
	sun_cmap[6].b = 0;	/* Green */

	slotsused = 7;

	/* apply region-id based colors to the solid table */
	color_soltab();

	/* Map the colors in the solid table to colormap indices */
	sun_colorit();

	for( i = 0; i < slotsused; i++ ) {
		red[i] = sun_cmap[i].r;
		grn[i] = sun_cmap[i].g;
		blu[i] = sun_cmap[i].b;
	}
	/* fill in remainder with a "distinctive" color */
	while( i < CMS_MGEDSIZE ) {
		red[i] = 200;
		grn[i] = 200;
		blu[i] = 200;
		i++;
	}
	/*
	 * Set bottom (background) and top (foreground) equal so
	 * that suntools will fill in the "correct" values for us.
	 */
	red[CMS_MGEDSIZE-1] = red[0];
	grn[CMS_MGEDSIZE-1] = grn[0];
	blu[CMS_MGEDSIZE-1] = blu[0];
	pw_putcolormap(sun_pw, 0, CMS_MGEDSIZE, red, grn, blu);
	/* read back the corrected colors */
	pw_getcolormap(sun_pw, 0, CMS_MGEDSIZE, red, grn, blu);
	/* save returned foreground color at size-2 */
	red[CMS_MGEDSIZE-2] = red[CMS_MGEDSIZE-1];
	grn[CMS_MGEDSIZE-2] = grn[CMS_MGEDSIZE-1];
	blu[CMS_MGEDSIZE-2] = blu[CMS_MGEDSIZE-1];
	/* Set cursor (forground!) color */
	red[CMS_MGEDSIZE-1] = 255;
	grn[CMS_MGEDSIZE-1] =  80;
	blu[CMS_MGEDSIZE-1] =   0;
	pw_putcolormap(sun_pw, 0, CMS_MGEDSIZE, red, grn, blu);
}

int
sun_colorit()
{
	register struct solid	*sp;
	register struct cmap *rgb;
	register int    i;
	register int    r, g, b;

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
		rgb = sun_cmap;
		for( i = 0; i < slotsused; i++, rgb++ )  {
			if( rgb->r == r && rgb->g == g && rgb->b == b )  {
				sp->s_dmindex = i;
				goto next;
			}
		}

		/* If slots left, create a new color map entry, first-come basis */
		if( slotsused < sun_nslots )  {
			rgb = &sun_cmap[i=(slotsused++)];
			rgb->r = r;
			rgb->g = g;
			rgb->b = b;
			sp->s_dmindex = i;
			continue;
		}
		sp->s_dmindex = DM_YELLOW;	/* Default color */
next:		;
	}
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

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
