/*
 *			I F _ X . C
 *
 *  X Window System (X11) libfb interface.
 *
 *  Authors -
 *	Phillip Dykstra
 *
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1988 by the United States Army.
 *	All rights reserved.
 *
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include <stdio.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/cursorfont.h>
#include <X11/Xatom.h>		/* for XA_RGB_BEST_MAP */
#include "fb.h"
#include "./fblocal.h"

extern int	fb_sim_readrect(), fb_sim_writerect();

static	int	linger();

_LOCAL_ int	X_dopen(),
		X_dclose(),
		X_dreset(),
		X_dclear(),
		X_bread(),
		X_bwrite(),
		X_cmread(),
		X_cmwrite(),
		X_viewport_set(),
		X_window_set(),
		X_zoom_set(),
		X_curs_set(),
		X_cmemory_addr(),
		X_cscreen_addr(),
		X_help();

/* This is the ONLY thing that we normally "export" */
FBIO X_interface =  {
	X_dopen,		/* device_open		*/
	X_dclose,		/* device_close		*/
	X_dreset,		/* device_reset		*/
	X_dclear,		/* device_clear		*/
	X_bread,		/* buffer_read		*/
	X_bwrite,		/* buffer_write		*/
	X_cmread,		/* colormap_read	*/
	X_cmwrite,		/* colormap_write	*/
	X_viewport_set,		/* viewport_set		*/
	X_window_set,		/* window_set		*/
	X_zoom_set,		/* zoom_set		*/
	X_curs_set,		/* curs_set		*/
	X_cmemory_addr,		/* cursor_move_memory_addr */
	X_cscreen_addr,		/* cursor_move_screen_addr */
	fb_sim_readrect,
	fb_sim_writerect,
	X_help,			/* help message		*/
	"X Window System (X11)",/* device description	*/
	1024,			/* max width		*/
	1024,			/* max height		*/
	"/dev/X",		/* short device name	*/
	512,			/* default/current width  */
	512,			/* default/current height */
	-1,			/* file descriptor	*/
	PIXEL_NULL,		/* page_base		*/
	PIXEL_NULL,		/* page_curp		*/
	PIXEL_NULL,		/* page_endp		*/
	-1,			/* page_no		*/
	0,			/* page_dirty		*/
	0L,			/* page_curpos		*/
	0L,			/* page_pixels		*/
	0			/* debug		*/
};

/*
 * Per window state information.
 */
struct	xinfo {
	Display	*dpy;			/* Display and Screen(s) info */
	Window	win;			/* Window ID */
	int	screen;			/* Our screen selection */
	Visual	*visual;		/* Our visual selection */
	GC	gc;			/* current graphics context */
	XStandardColormap cmap;		/* 8bit colormap */
	XImage	*image;

	int	depth;			/* 1, 8, or 24bit */
	int	mode;			/* 0,1,2 */
	int	x_zoom, y_zoom;
	int	x_window, y_window;	/**/
	int	x_corig, y_corig;	/* cursor origin offsets */
};
#define	XI(ptr) ((struct xinfo *)((ptr)->u1.p))
#define	XIL(ptr) ((ptr)->u1.p)		/* left hand side version */

unsigned char	*bytebuf = NULL;	/*XXXXXX*/
char	*bitbuf = NULL;		/*XXXXXX*/

_LOCAL_ int
X_dopen( ifp, file, width, height )
FBIO	*ifp;
char	*file;
int	width, height;
{
	char	*display_name = NULL;

	if( width <= 0 )
		width = ifp->if_width;
	if( height <= 0 )
		height = ifp->if_height;
	if ( width > ifp->if_max_width) 
		width = ifp->if_max_width;
	if ( height > ifp->if_max_height) 
		height = ifp->if_max_height;

	/* round width up to a multiple of eight bits */
	if( (width%8) != 0 )
		width = (width + 7)/8;
	ifp->if_width = width;
	ifp->if_height = height;

	if( (XIL(ifp) = (char *)calloc( 1, sizeof(struct xinfo) )) == NULL ) {
		fb_log("X_dopen: xinfo malloc failed\n");
		return(-1);
	}

	if( file[strlen("/dev/X")] != NULL )
		XI(ifp)->mode = 1;

	/* set up an X window, graphics context, etc. */
	if( xsetup( ifp, width, height ) < 0 ) {
		return(-1);
	}

#ifdef notes
	/* monochrome */
        colormap = XDefaultColormap(dpy,screen);
        image = XCreateImage(dpy, visual, 1, XYBitmap, 0,
                (char *)buffer, buf_width, buf_height, 8, 0);
	/* color */
        colormap = GetColormap(colors, ncolors, &newmap_flag, buffer,
            buffer_size);
        image = XCreateImage(dpy, visual, 8, ZPixmap, 0, (char *)buffer,
            buf_width, buf_height, 8, 0);
	/* expose event */
	XPutImage(dpy, image_win, gc, image,
		expose->x, expose->y, expose->x, expose->y,
		expose->width, expose->height);
	/* normal draw */
	XPutImage(dpy, image_win, gc, image, 0, 0, 0, 0,
		image->width, image->height);
#endif

	/* an image data buffer */
	if( (bytebuf = (unsigned char *)calloc( 1, width*height )) == NULL ) {
		fb_log("X_dopen: bytebuf malloc failed\n");
		return(-1);
	}
	if( (bitbuf = (char *)calloc( 1, (width*height)/8 )) == NULL ) {
		fb_log("X_dopen: bitbuf malloc failed\n");
		return(-1);
	}

	/*
	 *  Create an Image structure.
	 *  The image is our client resident copy which we
	 *  can get/put from/to a server resident Pixmap or
	 *  Window (i.e. a "Drawable").
	 */
	if( XI(ifp)->depth == 1 ) {
		XI(ifp)->image = XCreateImage( XI(ifp)->dpy,
			XI(ifp)->visual, 1, XYBitmap, 0,
			(char *)bitbuf, width, height, 8, 0);
	} else if( XI(ifp)->depth == 8 ) {
		XI(ifp)->image = XCreateImage( XI(ifp)->dpy,
			XI(ifp)->visual, 8, ZPixmap, 0,
			(char *)bytebuf, width, height, 8, 0);
	} else {
		fprintf( stderr, "if_X: can't handle a depth of %d\n",
			XI(ifp)->depth );
		return(-1);
	}

	return(0);
}

_LOCAL_ int
X_dclose( ifp )
FBIO	*ifp;
{
	XFlush( XI(ifp)->dpy );
	if( XI(ifp)->mode ) {
		if( linger(ifp) )
			return(0);	/* parent leaves the display */
	}
	if( XIL(ifp) != NULL ) {
		XCloseDisplay( XI(ifp)->dpy );
		(void)free( (char *)XIL(ifp) );
	}
	return(0);
}

_LOCAL_ int
X_dreset( ifp )
FBIO	*ifp;
{
}

_LOCAL_ int
X_dclear( ifp, pp )
FBIO	*ifp;
RGBpixel	*pp;
{
	XClearWindow( XI(ifp)->dpy, XI(ifp)->win );
	return(0);
}

_LOCAL_ int
X_bread( ifp, x, y, pixelp, count )
FBIO	*ifp;
int	x, y;
RGBpixel	*pixelp;
int	count;
{
}

/*************************
* code for dithering     *
*************************/
int dm4[4][4] = {
     0,  8,  2, 10,
    12,  4, 14,  6,
     3, 11,  1,  9,
    15,  7, 13,  5
};
int dm8[8][8] = {
     0, 32,  8, 40,  2, 34, 10, 42,
    48, 16, 56, 24, 50, 18, 58, 26,
    12, 44,  4, 36, 14, 46,  6, 38,
    60, 28, 52, 20, 62, 30, 54, 22,
     3, 35, 11, 43,  1, 33,  9, 41,
    51, 19, 59, 27, 49, 17, 57, 25,
    15, 47,  7, 39, 13, 45,  5, 37,
    63, 31, 55, 23, 61, 29, 53, 21
};
int ditherPeriod = 8;
int *dm = &(dm8[0][0]);
int *error1, *error2;

int dither_bw(pixel,count,line)
unsigned int pixel;
register count, line;
{   
    if( pixel > dm[((line%ditherPeriod)*ditherPeriod) +
            (count%ditherPeriod)])
        return(1);
    else
        return(0);
}
/***************************************
* code for modified floyd steinberg    *
****************************************/
int mfs_bw(pixel,count,line)
unsigned int pixel;
register count, line;
{
    int  onoff, *te;
    long  intensity, pixerr;

    if (count == 0) {
        te = error1;
        error1 = error2;
        error2 = te;
        error2[0] = 0;
    }  
    intensity = pixel + error1[count];
    if (intensity > 255)
        intensity = 255;
    else if (intensity < 0)
        intensity = 0;

    if (intensity < 128) {
        onoff = 0;
        pixerr = 128 - intensity;
    }
    else {
        onoff = 1;
        pixerr = 128 - intensity;
    }
    error1[count+1] += (int)(3*pixerr)/8;
    error2[count+1] = (int)pixerr/4;
    error2[count] += (int)(3*pixerr)/8;
    return(onoff);
}
/*****************************
* code for floyd steinberg   *
*****************************/
int fs_bw(pixel, count, line)
unsigned int pixel;
register count, line;
{
    int  onoff, *te; 
    long  intensity, pixerr;

    if (count == 0) {
        te = error1;
        error1 = error2;
        error2 = te;
        error2[0] = 0;
    }  
    intensity = pixel + error1[count];
    if (intensity > 255)
        intensity = 255;
    else if (intensity < 0)
            intensity = 0;

    if (intensity < 128) {
        onoff = 0;
        pixerr = intensity;
    } else {
        onoff = 1;
        pixerr = intensity - 255;
    }
    error1[count+1] += (int)(3*pixerr)/8;
    error2[count+1] = (int)pixerr/4;
    error2[count] += (int)(3*pixerr)/8;
    return(onoff);
}

_LOCAL_ int
X_bwrite( ifp, x, y, pixelp, count )
FBIO	*ifp;
int	x, y;
RGBpixel	*pixelp;
int	count;
{
	register unsigned char	*cp;
	register int	i;

	/* 1st -> 4th quadrant */
	y = ifp->if_height - 1 - y;

	if( XI(ifp)->depth == 8 ) {
		/* assuming SGI 4Ds strange form of pseudocolor XXX */
		cp = &bytebuf[y*ifp->if_width + x];
		for( i = 0; i < count; i++ ) {
			cp[i] = (int)(pixelp[i][RED]/52)
				+ 25*(int)(pixelp[i][GRN]/28.45)
				+ 5*(int)(pixelp[i][BLU]/52) + 31;
		}
		goto done;
	}

	/* save the 8bit black and white version of it */
	cp = &bytebuf[y*ifp->if_width + x];
	for( i = 0; i < count; i++ ) {
		cp[i] = (30*pixelp[i][RED] + 59*pixelp[i][GRN]
			+ 11*pixelp[i][BLU]) / 100;
	}

	/* Convert the monochrome data to a bitmap */
    if (XBitmapBitOrder(XI(ifp)->dpy) == LSBFirst) {
#ifdef LATER
        for (row=0; row < height; row++)
            for (col=0; col < width; ) {
                mvalue = 0x00;
                for (bit=0; (bit < 8) && (col < width); bit++,col++)
                    if( dither_bw(*mpbuffer++, col, row) )
                        mvalue |= (0x01 << bit);    /*  for Vax */
                *mbuffer++ = mvalue;
            }
#endif
    } else {
	int	row, col, bit, val;
    	int	byte, rem;
	unsigned char	mvalue;
	unsigned char	*mbuffer;	/* = &buffer[(y*ifp->if_width + x)/8]; */
    	byte = y * ifp->if_width + x;
    	rem = byte % 8;
    	byte /= 8;
	mbuffer = (unsigned char *)&bitbuf[byte];

	for( row = y; row < y+1; row++ ) {
		for( col=x; col < x+count; ) {
			/*mvalue = 0x00;*/
			mvalue = *mbuffer;
			for( bit=rem; (bit < 8) && (col < x+count); bit++,col++ ) {
				/*val = (30*(*pixelp)[RED] + 59*(*pixelp)[GRN] + 11*(*pixelp)[BLU] + 200) / 400;*/
				/*pixelp++;*/
				/*if( dither_bw(val, col, row) ) {*/
				if( *cp++ > (dm[((row&7)<<3)+(col&7)]<<2)+1 ) {
					mvalue |= (0x80 >> bit);    /*  for RT, Sun  */
				} else {
					mvalue &= ~(0x80 >> bit);    /*  for RT, Sun  */
				}
			}
			rem = 0;
			*mbuffer++ = mvalue;
		}
	}
    }

done:
	XPutImage(XI(ifp)->dpy, XI(ifp)->win, XI(ifp)->gc, XI(ifp)->image,
		x, y, x, y,
		count, 1 );
}

_LOCAL_ int
X_cmread( ifp, cmp )
FBIO	*ifp;
ColorMap	*cmp;
{
}

_LOCAL_ int
X_cmwrite( ifp, cmp )
FBIO	*ifp;
ColorMap	*cmp;
{
}

_LOCAL_ int
X_viewport_set( ifp, left, top, right, bottom )
FBIO	*ifp;
int	left, top, right, bottom;
{
}

_LOCAL_ int
X_window_set( ifp, x, y )
FBIO	*ifp;
int	x, y;
{
}

_LOCAL_ int
X_zoom_set( ifp, x, y )
FBIO	*ifp;
int	x, y;
{
}

_LOCAL_ int
X_curs_set( ifp, bits, xbits, ybits, xorig, yorig )
FBIO	*ifp;
unsigned char *bits;
int	xbits, ybits;
int	xorig, yorig;
{
}

_LOCAL_ int
X_cmemory_addr( ifp, mode, x, y )
FBIO	*ifp;
int	mode;
int	x, y;
{
}

_LOCAL_ int
X_cscreen_addr( ifp, mode, x, y )
FBIO	*ifp;
int	mode;
int	x, y;
{
}

XWMHints xwmh = {
	(InputHint|StateHint),		/* flags */
	False,				/* input */
	NormalState,			/* initial_state */
	0,				/* icon pixmap */
	0,				/* icon window */
	0, 0,				/* icon location */
	0,				/* icon mask */
	0				/* Window group */
};

xsetup( ifp, width, height )
FBIO	*ifp;
int	width, height;
{
	char	hostname[80];
	char	display[80];
	char	*envp;
	unsigned long	bd, bg, fg, bw;
	XSizeHints xsh;
	XEvent	event;
	XGCValues gcv;
	Display	*dpy;			/* local copy */
	Window	win;			/* local copy */
	int	screen;			/* local copy */
	Visual	*visual;		/* local copy */
	GC	gc;			/* local copy */
	XSetWindowAttributes	xswa;

	width = height = 512;

	/* Open the display - use the env variable DISPLAY */
	if( (dpy = XOpenDisplay(NULL)) == NULL ) {
		fb_log( "if_X: Can't open X display \"%s\"\n",
			XDisplayName(NULL) );
		return	-1;
	}
	screen = DefaultScreen(dpy);
	visual = DefaultVisual(dpy,screen);
XI(ifp)->dpy = dpy;
XI(ifp)->screen = screen;
XI(ifp)->visual = visual;
XI(ifp)->depth = DisplayPlanes(dpy,screen);

#ifdef DEBUGX
printf("%d DisplayPlanes\n", DisplayPlanes(dpy,screen) );
switch(DefaultVisual(dpy,screen)->class) {
case DirectColor:
	printf("DirectColor: Full Color changeable map\n");
	break;
case TrueColor:
	printf("TrueColor: Full Color, no map\n");
	break;
case PseudoColor:
	printf("PseudoColor: Some Color, changeable map\n");
	break;
case StaticColor:
	printf("StaticColor: Some Color, fixed map\n");
	break;
case GrayScale:
	printf("GrayScale: gray scale, changeable map\n");
	break;
case StaticGray:
	printf("StaticGray: gray scale, fixed map\n");
	break;
default:
	printf("Unknown visual class %d\n",
		DefaultVisual(dpy,screen)->class);
	break;
}
#endif

	/*
	 * Select border, background, foreground colors,
	 * and border width.
	 */
	bd = WhitePixel( dpy, screen );
	bg = BlackPixel( dpy, screen );
	fg = WhitePixel( dpy, screen );
	bw = 1;

	/*
	 * Fill in XSizeHints struct to inform window
	 * manager about initial size and location.
	 */
	xsh.flags = PPosition | PSize | PMinSize | PMaxSize;
	xsh.width = xsh.max_width = xsh.min_width = width;
	xsh.height = xsh.max_height = xsh.min_height = height;
	xsh.x = xsh.y = 0;

	/*
	 * Fill in XSetWindowAttributes struct for XCreateWindow.
	 */
	xswa.event_mask = ExposureMask;
		/* |ButtonPressMask |LeaveWindowMask | EnterWindowMask; */
		/* |ColormapChangeMask */
	/*xswa.colormap = colormap;*/
	xswa.background_pixel = BlackPixel(dpy, screen);
	xswa.border_pixel = WhitePixel(dpy, screen);
#ifdef CURSOR
	xswa.cursor = XCreateFontCursor(dpy, XC_gumby);
#endif

#ifdef OLD
	win = XCreateSimpleWindow( dpy, DefaultRootWindow(dpy),
		xsh.x, xsh.y, xsh.width, xsh.height,
		bw, bd, bg );
#endif /* OLD */

	/*
	 *  Note: all Windows, Colormaps and XImages have
	 *  a Visual attribute which determines how pixel
	 *  values are mapped to displayed colors.
	 */
#ifdef DEBUGX
printf("Creating window\n");
#endif
	win = XCreateWindow( dpy, DefaultRootWindow(dpy),
		0, 0, xsh.width, xsh.height,
		3, XDefaultDepth(dpy, screen),
		InputOutput, visual,
/*CURSOR	CWBackPixel |CWEventMask |CWCursor |CWBorderPixel,*/
		CWBackPixel |CWEventMask |CWBorderPixel,
		/* |CWColormap */
		&xswa );

XI(ifp)->win = win;
	if( win == 0 ) {
		fb_log( "if_X: Can't create window\n" );
		return	-1;
	}

	/* Set standard properties for Window Managers */
#ifdef DEBUGX
printf("Setting properties\n");
#endif
	XSetStandardProperties( dpy, win, "Frame buffer", "Frame buffer",
		None, NULL, 0, &xsh );
#ifdef DEBUGX
printf("Setting Hints\n");
#endif
	XSetWMHints( dpy, win, &xwmh );

	/* Create a Graphics Context for drawing */
	/*gcv.font = NULL;*/
	gcv.foreground = fg;
	gcv.background = bg;
#ifdef DEBUGX
printf("Making graphics context\n");
#endif
	gc = XCreateGC( dpy, win, (GCForeground|GCBackground), &gcv );
XI(ifp)->gc = gc;

	if( XI(ifp)->depth == 8 ) {
		int	ret;
		ret = XGetStandardColormap( dpy, RootWindow(dpy,screen),
			&(XI(ifp)->cmap), XA_RGB_BEST_MAP );
		if( !ret ) {
			fb_log("if_X: can't get colormap\n");
		} else {
#ifdef DEBUGX
printf("R G B Max: %d %d %d\n",
XI(ifp)->cmap.red_max, XI(ifp)->cmap.green_max, XI(ifp)->cmap.blue_max);
printf("R G B Mult: %d %d %d\n",
XI(ifp)->cmap.red_mult, XI(ifp)->cmap.green_mult, XI(ifp)->cmap.blue_mult);
printf("Base: %d\n",
XI(ifp)->cmap.base_pixel);
#endif
			;
		}
	}

	XSelectInput( dpy, win, ExposureMask );
	XMapWindow( dpy, win );

	while( 1 ) {
		XNextEvent( dpy, &event );
		if( event.type == Expose && event.xexpose.count == 0 ) {
			XWindowAttributes xwa;
			int	x, y;

			/* remove other exposure events */
			while( XCheckTypedEvent(dpy, Expose, &event) ) ;

			if( XGetWindowAttributes( dpy, win, &xwa ) == 0 )
				break;

			width = xwa.width;
			height = xwa.height;
			break;
		}
	}

	return	0;
}

static int
linger( ifp )
FBIO	*ifp;
{
	XEvent	event;
	XExposeEvent	*expose;
	XCrossingEvent	*xcrossing;
	int	alive = 1;

	if( fork() != 0 )
		return 1;	/* release the parent */

	XSelectInput( XI(ifp)->dpy, XI(ifp)->win,
		ExposureMask|ButtonPressMask );
	expose = (XExposeEvent *)&event;
	while( alive ) {
		XNextEvent( XI(ifp)->dpy, &event );
		switch( (int)event.type ) {
		case Expose:
			/*XXXfprintf(stderr,
			"expose event x= %d y= %d width= %d height= %d\n",
			expose->x, expose->y, expose->width, expose->height);*/
			XPutImage(XI(ifp)->dpy, XI(ifp)->win, XI(ifp)->gc, XI(ifp)->image,
				expose->x, expose->y, expose->x, expose->y,
				expose->width, expose->height );
			break;
		case ButtonPress:
			switch( (int)event.xbutton.button ) {
			case Button1:
				Monochrome(bitbuf,bytebuf,512,512);
				XPutImage(XI(ifp)->dpy, XI(ifp)->win, XI(ifp)->gc, XI(ifp)->image,
					0, 0, 0, 0,
					512, 512 );
				break;
			case Button2:
				fb_log("(%d,%d)\n", event.xbutton.x, event.xbutton.y );
				break;
			case Button3:
				alive = 0;
				break;
			}
			break;
		default:
			fb_log("Bad X event.\n");
			break;
		}
	}

	return 0;
}

static int method = 0;

/* Monochrome to Bitmap conversion */
Monochrome(bitbuf,bytebuf,width,height)
char *bitbuf;
unsigned char *bytebuf;
int width, height;
{
	register unsigned char *mbuffer, mvalue;   /* monochrome bitmap buffer */
	register unsigned char *mpbuffer;          /* monochrome byte buffer */
	register row, col, bit;
	char  *malloc(), *calloc();

	error1 = (int *)malloc((unsigned)(width+1) * sizeof(int));
	error2 = (int *)malloc((unsigned)(width+1) * sizeof(int));

	mpbuffer = bytebuf;
	mbuffer = (unsigned char *)bitbuf;

	for( row = 0; row < height; row++ ) {
		for( col=0; col < width; ) {
			mvalue = 0x00;
			for( bit=0; (bit < 8) && (col < width); bit++,col++ ) {
				/*if( dither_bw(val, col, row) ) {*/
				if( method == 0 ) {
					if( *mpbuffer > (dm[((row&7)<<3)+(col&7)]<<2)+1 ) {
						mvalue |= (0x80 >> bit);    /*  for RT, Sun  */
					}
				} else if( method == 1 ) {
					if( fs_bw(*mpbuffer, col, row) ) {
						mvalue |= (0x80 >> bit);    /*  for RT, Sun  */
					}
				} else if( method == 2 ) {
					if( mfs_bw(*mpbuffer, col, row) ) {
						mvalue |= (0x80 >> bit);    /*  for RT, Sun  */
					}
				} else if( method == 3 ) {
					if( *mpbuffer > (dm4[(row&3)][(col&3)]<<4)+7 ) {
						mvalue |= (0x80 >> bit);    /*  for RT, Sun  */
					}
				}
				mpbuffer++;
			}
			*mbuffer++ = mvalue;
		}
	}

	if( method == 0 )
		method = 1;
	else if( method == 1 )
		method = 2;
	else if( method == 2 )
		method = 3;
	else if( method == 3 )
		method = 0;

	free((char *)error1);
	free((char *)error2);
}

_LOCAL_ int
X_help( ifp )
FBIO	*ifp;
{
	fb_log( "Description: %s\n", X_interface.if_type );
	fb_log( "Device: %s\n", ifp->if_name );
	fb_log( "Max width/height: %d %d\n",
		X_interface.if_max_width,
		X_interface.if_max_height );
	fb_log( "Default width/height: %d %d\n",
		X_interface.if_width,
		X_interface.if_height );
	return(0);
}
