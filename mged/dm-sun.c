/*
 *			D M - S U N . C
 *
 * Driver for SUN using SunView pixwins library.
 * This will use a GP if present, but will not do shaded stuff.
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
#include <sunwindow/window_hs.h>
#include <sunwindow/cms.h>
#include <sunwindow/cms_rgb.h>
#include <suntool/sunview.h>
#include <suntool/gfxsw.h>

extern void     perror();

/* typedef unsigned char u_char; */

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

extern struct device_values dm_values;	/* values read from devices */

static vect_t   clipmin, clipmax;	/* for vector clipping */
/* static char     ttybuf[BUFSIZ]; */

Pixwin         *sun_master_pw;		/* area of screen available */
Pixwin		*sun_pw;		/* sub-area of screen being used */
int		sun_depth;
int             sun_cmap_color = 7;	/* for default colormap */
struct pixfont *sun_font_ptr;
Pr_texture     *sun_get_texP();
Rect            sun_master_rect;
Rect            sun_win_rect;
int             sun_win_fd;
int             sun_damaged;
int             sun_debug = 0;
Inputmask       sun_input_mask;

/*
 * Display coordinate conversion:
 *  GED is using -2048..+2048,
 *  and we define (for the moment) window coordinates to be 0...512
 *
 *  NOTE that the Sun is 4th quadrant, so the user of these macros
 *  has to flip his Y values.
 */
#define	GED_TO_SUNPW(x)	((((int)(x))+2048)>>3)
#define SUNPW_TO_GED(x)	((((int)(x))<<3)-2048)
#define LABELING_FONT   "/usr/lib/fonts/fixedwidthfonts/sail.r.6"

Notify_value    sun_sigwinch()
{
                    pw_damaged(sun_pw);
    pw_donedamaged(sun_pw);
    sun_damaged = 1;
}

/*
 *			S U N P W _ O P E N
 *
 * Fire up the display manager, and the display processor.
 *
 */
SunPw_open()
{
	Rect            rect;
	char unsigned   red[256], grn[256], blu[256];
	struct pr_subregion bound;

#ifdef	DISABLED
	struct gfxsubwindow *mine;

	/*
	 * Get graphics subwindow, get its size, and trap alarm clock and interrupt
	 * signals. 
	 */
	mine = gfxsw_init(0, 0);
	sun_pw = mine->gfx_pixwin;
	win_getsize(sun_pw->pw_clipdata->pwcd_windowfd, &rect);
	sun_win_rect = rect;
#endif	DISABLED
#ifdef	DISABLED
	Window          frame;

	frame = window_create(NULL, FRAME,
	    WIN_WIDTH, 600,
	    WIN_HEIGHT, 600,
	    0);
	sun_pw = window_get(frame, WIN_PIXWIN);
#endif	DISABLED

	{
		int             gfxfd;
		char            gfxwinname[128];
		int		width, height;
		int		size;

		sun_win_fd = win_getnewwindow();
		we_getgfxwindow(gfxwinname);
		gfxfd = open(gfxwinname, 2);
		win_insertblanket(sun_win_fd, gfxfd);

		if( (sun_master_pw = pw_open(sun_win_fd)) == (Pixwin *)0 )  {
			fprintf(stderr,"sun: pw_open failed\n");
			return(-1);
		}
		win_getsize(sun_win_fd, &sun_master_rect);
		width = sun_master_rect.r_width;
		height = sun_master_rect.r_height;
		if( width > height )  size = height;
		else  size = width;

		if( size < 512 ) printf("screen is not 512x512 pixels!\n");
		/* XXX should be size, not 512 */
		sun_pw = pw_region(sun_master_pw, 0, 0, 512, 512);
		win_getsize(sun_win_fd, &rect);
		sun_win_rect = rect;
		sun_depth = sun_pw->pw_pixrect->pr_depth;

		input_imnull(&sun_input_mask);
		sun_input_mask.im_flags |= IM_NEGEVENT;
		win_setinputcodebit(&sun_input_mask, MS_LEFT);
		win_setinputcodebit(&sun_input_mask, MS_MIDDLE);
		win_setinputcodebit(&sun_input_mask, MS_RIGHT);
		win_setinputcodebit(&sun_input_mask, KEY_TOP(1));
		win_setinputcodebit(&sun_input_mask, KEY_TOP(2));
		win_setinputcodebit(&sun_input_mask, KEY_TOP(3));
		win_setinputcodebit(&sun_input_mask, KEY_TOP(4));
		win_setinputcodebit(&sun_input_mask, KEY_TOP(5));
		win_setinputcodebit(&sun_input_mask, KEY_TOP(6));
		win_setinputcodebit(&sun_input_mask, KEY_TOP(7));
		win_setinputcodebit(&sun_input_mask, KEY_TOP(8));
		win_setinputcodebit(&sun_input_mask, KEY_TOP(9));
		win_set_pick_mask(sun_win_fd, &sun_input_mask);
		fcntl(sun_win_fd, F_SETFL, FNDELAY);
	}

	if( sun_depth >= 8 )  {
		/* set a new cms name; initialize it */
		pw_setcmsname(sun_pw, "mged");
		/* default colormap, real one is set by SunPw_colorchange */
		red[0] = 0;
		grn[0] = 0;
		blu[0] = 0;
		red[1] = 255;
		grn[1] = 0;
		blu[1] = 0;
		red[2] = 0;
		grn[2] = 255;
		blu[2] = 0;
		red[3] = 255;
		grn[3] = 255;
		blu[3] = 0;
		red[4] = 0;
		grn[4] = 0;
		blu[4] = 255;
		red[5] = 255;
		grn[5] = 0;
		blu[5] = 255;
		red[6] = 0;
		grn[6] = 255;
		blu[6] = 255;
		red[7] = 255;
		grn[7] = 255;
		blu[7] = 255;
		pw_putcolormap(sun_pw, 0, 8, red, grn, blu);
	} else {
		pr_blackonwhite( sun_pw->pw_pixrect, 0, 1 );
	}

	sun_clear_win();
	sun_font_ptr = pf_open(LABELING_FONT);
	if (sun_font_ptr == (struct pixfont *) 0)
	{
		perror(LABELING_FONT);
		return(-1);
	}
	pf_textbound(&bound, 1, sun_font_ptr, "X");
	notify_set_signal_func(SunPw_open, sun_sigwinch, SIGWINCH, NOTIFY_ASYNC);
	return (0);			/* OK */
}

/*
 *  			S U N P W _ C L O S E
 *  
 *  Gracefully release the display.
 */
void	SunPw_close()
{
	pw_close(sun_pw);
	pw_close(sun_master_pw);
	(void)close(sun_win_fd);
	sun_win_fd = -1;
}

/*
 *			S U N _ C O L O R
 */
sun_color(color)
int             color;
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
	switch (color)
	{
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
void            SunPw_prolog()
{
	if(!dmaflag)
		return;

	/* clear the screen for a redraw */
	sun_clear_win();

	/* Put the center point up, in foreground */
	sun_color(DM_WHITE);
	pw_put(sun_pw, GED_TO_SUNPW(0), GED_TO_SUNPW(0), 1);
}

/*
 *			S U N P W _ E P I L O G
 */
void            SunPw_epilog()
{
                    return;
}

/*
 *  			S U N P W _ N E W R O T
 *  Stub.
 */
/* ARGSUSED */
void            SunPw_newrot(mat)
mat_t           mat;
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
int             SunPw_object(sp, mat, ratio, white)
register struct solid *sp;
mat_t           mat;
double          ratio;
{
	register struct pr_pos *ptP;
	u_char         *mvP;
	static vect_t   pt;
	register struct vlist *vp;
	struct mater   *mp;
	int             nvec, totalvec;
	int             useful = 0;
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
	totalvec = nvec;
	for( vp = sp->s_vlist; vp != VL_NULL; vp = vp->vl_forw )  {
		MAT4X3PNT(pt, mat, vp->vl_pnt);
		/* Visible range is +/- 1.0 */
		/* 2^31 ~= 2e9 -- dynamic range of a long int */
		/* 2^(31-11) = 2^20 ~= 1e6 */
		if( pt[0] < -1e6 || pt[0] > 1e6 ||
		    pt[1] < -1e6 || pt[1] > 1e6 )
			continue;		/* omit this point (ugh) */
		/* Integerize and let the Sun library do the clipping */
		ptP->x = GED_TO_SUNPW( 2048*pt[0]);
		ptP->y = GED_TO_SUNPW(-2048*pt[1]);
		ptP++;
		if (vp->vl_draw == 0)
			*mvP++ = 1;
		else
			*mvP++ = 0;
		useful++;
	}
	mvlist[0] = 0;
	if( sun_depth < 8 )  {
		sun_color( DM_WHITE );
		color = PIX_COLOR(sun_cmap_color);
	} else
		color = PIX_COLOR(mp->mt_dm_int);
	pw_polyline(sun_pw, 0, 0, totalvec, polylist, mvlist, brush,
	    texP, PIX_SRC | color );
	return(useful);
}

/*
 *			S U N P W _ N O R M A L
 *
 * Restore the display processor to a normal mode of operation
 * (ie, not scaled, rotated, displaced, etc).
 * Turns off windowing.
 */
void            SunPw_normal()
{
                    return;
}

/*
 *			S U N P W _ U P D A T E
 *
 * Transmit accumulated displaylist to the display processor.
 */
void            SunPw_update()
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
void            SunPw_puts(str, x, y, size, color)
register u_char *str;
{
	sun_color(color);
	pw_text(sun_pw, GED_TO_SUNPW(x), GED_TO_SUNPW(-y),
		(PIX_NOT(PIX_SRC)) | PIX_COLOR(sun_cmap_color),
		sun_font_ptr, str);
}

/*
 *			S U N P W _ 2 D _ L I N E
 *
 */
void            SunPw_2d_line(x1, y1, x2, y2, dashed)
int             x1, y1;
int             x2, y2;
int             dashed;
{
    Pr_texture     *texP;

    sun_color(DM_YELLOW);
    texP = sun_get_texP(dashed);
    pw_line(sun_pw, GED_TO_SUNPW(x1), GED_TO_SUNPW(-y1),
	    GED_TO_SUNPW(x2), GED_TO_SUNPW(-y2), (Pr_brush *) 0, texP,
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

SunPw_input(cmd_fd, noblock)
{
    long            readfds;
    struct timeval  timeout;
    int             numfds;
    int             ret;
    int             id;
    int             button;
    float           xval;
    float           yval;
    Event           event;

    /*
     * Check for input on the keyboard or on the polled registers. 
     *
     * Suspend execution until either 1)  User types a full line 2)  The timelimit
     * on SELECT has expired 
     *
     * If a RATE operation is in progress (zoom, rotate, slew) in which we still
     * have to update the display, do not suspend execution. 
     */
    readfds = (1 << cmd_fd) | (1 << sun_win_fd);
    if (noblock)
    {
	timeout.tv_sec = 0;
	timeout.tv_usec = 0;
	numfds = select(32, &readfds, (long *) 0, (long *) 0, &timeout);
    }
    else
    {
	timeout.tv_sec = 30 * 60;	/* 30 mins */
	timeout.tv_usec = 0;
	numfds = select(32, &readfds, (long *) 0, (long *) 0, &timeout);
    }

    dm_values.dv_penpress = 0;
    if ((ret = input_readevent(sun_win_fd, &event)) != -1)
    {
	id = event_id(&event);
	dm_values.dv_xpen = SUNPW_TO_GED(event_x(&event));
	dm_values.dv_ypen = -SUNPW_TO_GED(event_y(&event));
	switch(id)
	{
	case MS_LEFT:
	    if (event_is_down(&event))
		dm_values.dv_penpress = DV_OUTZOOM;
	    break;
	case MS_MIDDLE:
	    if (event_is_down(&event))
		dm_values.dv_penpress = DV_PICK;
	    break;
	case MS_RIGHT:
	    if (event_is_down(&event))
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
	    for (button = 0; button < NBUTTONS; button++)
	    {
		if (sun_buttons[button])
		{
		    switch(button)
		    {
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
	    sun_key(&event, ZOOM_BUTTON);
	    dm_values.dv_zoom = 0.0;
	    break;
	case KEY_TOP(SLEW_BUTTON):
	    sun_key(&event, SLEW_BUTTON);
	    dm_values.dv_xslew = 0.0;
	    dm_values.dv_yslew = 0.0;
	    break;
	case KEY_TOP(XY_ROT_BUTTON):
	    sun_key(&event, XY_ROT_BUTTON);
	    dm_values.dv_xjoy = 0.0;
	    dm_values.dv_yjoy = 0.0;
	    dm_values.dv_zjoy = 0.0;
	    break;
	case KEY_TOP(YZ_ROT_BUTTON):
	    sun_key(&event, YZ_ROT_BUTTON);
	    dm_values.dv_xjoy = 0.0;
	    dm_values.dv_yjoy = 0.0;
	    dm_values.dv_zjoy = 0.0;
	    break;
	case KEY_TOP(XZ_ROT_BUTTON):
	    sun_key(&event, XZ_ROT_BUTTON);
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
	}
    }

	if( sun_damaged )  {
		dmaflag = 1;	/* causes refresh */
		sun_damaged = 0;
	}

    if (readfds & (1 << cmd_fd))
	return (1);		/* command awaits */
    else
	return (0);		/* just peripheral stuff */
}

sun_key(eventP, button)
Event	*eventP;
int	button;
{
    if (event_is_down(eventP))
    {
	sun_buttons[button] = 1;
	win_setinputcodebit(&sun_input_mask, LOC_MOVE);
	win_set_pick_mask(sun_win_fd, &sun_input_mask);
    }
    else
    {
	sun_buttons[button] = 0;
	win_unsetinputcodebit(&sun_input_mask, LOC_MOVE);
	win_set_pick_mask(sun_win_fd, &sun_input_mask);
    }
}

/* 
 *			S U N P W _ L I G H T
 */
/* ARGSUSED */
void            SunPw_light(cmd, func)
int             cmd;
int             func;		/* BE_ or BV_ function */
{
    return;
}

/* ARGSUSED */
unsigned        SunPw_cvtvecs(sp)
struct solid   *sp;
{
    return (0);
}

/*
 * Loads displaylist
 */
unsigned        SunPw_load(addr, count)
unsigned        addr, count;
{
    (void) printf("SunPw_load(x%x, %d.)\n", addr, count);
    return (0);
}

void            SunPw_statechange()
{
}

void            SunPw_viewchange()
{
}


/*
 * Color Map table
 */
#define NSLOTS		256
static int      sun_nslots = 0;	/* how many we have, <= NSLOTS */
static int      slotsused;	/* how many actually used */
static struct rgbtab
{
    unsigned char   r;
    unsigned char   g;
    unsigned char   b;
}               sun_rgbtab[NSLOTS];

/*
 *  			S U N P W _ C O L O R C H A N G E
 *  
 *  Go through the mater table, and allocate color map slots.
 *	8 bit system gives 4 or 8,
 *	24 bit system gives 12 or 24.
 */
void            SunPw_colorchange()
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
void            SunPw_debug(lvl)
{
	sun_debug = lvl;
	return;
}

void            SunPw_window(w)
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
	/* XXX oddly, using sun_master_pw here does not work! */
	pw_rop(sun_pw, 0, 0,
		sun_master_rect.r_width, sun_master_rect.r_height,
		PIX_SRC | PIX_COLOR(sun_cmap_color), (Pixrect *) 0, 0, 0);
}

Pr_texture     *sun_get_texP(dashed)
int             dashed;
{
    static Pr_texture dotdashed = {
				   pr_tex_dashdot,
				   0,
				   {
				    1,
				    1,
				    0,
				    0,
				    }
    };

    if (dashed)
	return (&dotdashed);
    else
	return ((Pr_texture *) 0);
}
