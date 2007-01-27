/*                        I F _ S U N . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2007 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @addtogroup if */
/** @{ */
/** @file if_sun.c
 *
 *  SUN display interface.  In order to simulate the behavior of a real
 *  framebuffer, an entire image worth of memory is saved using SysV
 *  shared memory.  This image exists across invocations of frame buffer
 *  utilities, and allows the full resolution of an image to be retained,
 *  and captured, even with the 1-bit deep 3/50 displays.
 *
 *  In order to use this large a chunk of memory with the shared memory
 *  system, it is necessary to reconfigure your kernel to authorize this.
 *  If you have already reconfigured your kernel and your hostname is
 *  "PICKLE", you should have a "/sys/conf/PICKLE" file, otherwise you should
 *  read up on kernel reconfiguration in your owners manual, and then
 *  copy /sys/conf/GENERIC to /sys/conf/PICKLE and make the necessary
 *  deletions.  Then add the following line to the other "options" lines
 *  near the top of "PICKLE".
 *
 *  options	SHMPOOL=1024	# Increase for BRL-CAD libfb
 *
 *  Once you have modified "PICKLE" finish the procedure by typing:
 *
 *  # /etc/config PICKLE
 *  # cd ../PICKLE
 *  # make && mv /vmunix /vmunix.bak && mv vmunix /vmunix
 *
 *  Then reboot your system and you should be all set.
 *
 *  Note that this has been tested on release 3.2 of the 3/50 kernel.
 *  If you have more than 4 Megs of memory on your system, you may want
 *  to increase [XY]MAXWINDOW to be as big as [XY]MAXSCREEN (see CON-
 *  FIGURATION NOTES below) and increase SHMPOOL appropriately.  If you
 *  do not reconfigure your kernel, the attempt to acquire shared memory
 *  will fail, and the image will be stored in the process's address space
 *  instead, with the only penalty being lack of persistance of the image
 *  across processes.
 *
 *  Authors -
 *	Bill Lindemann
 *	Michael John Muuss
 *	Phil Dykstra
 *	Gary S. Moss
 *	Lee A. Butler
 *
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *
 */
/** @} */

#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "common.h"


#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#include <signal.h>
#include <sys/file.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <suntool/sunview.h>
#include <suntool/canvas.h>

#include "machine.h"
#include "fb.h"
#include "./fblocal.h"

extern char *shmat();

HIDDEN int	linger();

/* CONFIGURATION NOTES:

	If you have 4 Megabytes of memory, best limit the window sizes
	to 512-by-512 (whether or not your using shared memory) because a
	24-bit image is stored in memory and we don't want our systems to
	thrash do we).
 */
#define XMAXSCREEN	1152	/* Physical screen width in pixels. */
#define YMAXSCREEN	896	/* Physical screen height in pixels. */
#define XMAXWINDOW	640	/* Max. width of frame buffer in pixels. */
#define YMAXWINDOW	512	/* Max. height of frame buffer in pixels. */
#define BANNER		18	/* Height of title bar at top of window. */
#define BORDER		2	/* Border thickness of window. */
#define PARENTBORDER	4	/* Border thickness of parent window. */
#define TITLEXOFFSET	20	/* Indentation for text within title bar. */
#define TITLEYOFFSET	4	/* Vertical padding of text within title bar. */
#define TIMEOUT		15	/* Seconds between check for window repaint. */
#define MAXDITHERSZ	8	/* Dimensions of dithering threshold matrix. */
#define DITHERSZ	8	/* Size of dither pattern used. */
#define DITHMASK(ii) ((ii)&07)	/* Masks of DITHERSZ bits. */

#define EQUALRGB(aa,bb) \
	((aa)[RED]==(bb)[RED]&&(aa)[GRN]==(bb)[GRN]&&(aa)[BLU]==(bb)[BLU])
#define XIMAGE2SCR( x ) \
 ((x)*ifp->if_xzoom-(ifp->if_xcenter*ifp->if_xzoom-ifp->if_width/2))
#define YIMAGE2SCR( y ) \
 ((ifp->if_height-1)-((y)*ifp->if_yzoom-(ifp->if_ycenter*ifp->if_yzoom - ifp->if_height/2)))
HIDDEN int	sun_open(),
		sun_close(),
		sun_clear(),
		sun_read(),
		sun_write(),
		sun_rmap(),
		sun_wmap(),
		sun_view(),
		sun_setcursor(),
		sun_cursor(),
		sun_getcursor(),
		sun_poll(),
		sun_free(),
		sun_help();

/* This is the ONLY thing that we "export" */
FBIO sun_interface = {
		0,
		sun_open,
		sun_close,
		sun_clear,
		sun_read,
		sun_write,
		sun_rmap,
		sun_wmap,
		sun_view,
		fb_sim_getview,
		sun_setcursor,
		sun_cursor,
		sun_getcursor,
		fb_sim_readrect,
		fb_sim_writerect,
		fb_sim_bwreadrect,
		fb_sim_bwwriterect,
		sun_poll,
		fb_null,		/* flush */
		sun_free,		/* free */
		sun_help,
		"SUN SunView or raw Pixwin",
		XMAXWINDOW,	/* max width */
		YMAXWINDOW,	/* max height */
		"/dev/sun",
		512,		/* current/default width  */
		512,		/* current/default height */
		-1,		/* select fd */
		-1,		/* file descriptor */
		1, 1,		/* zoom */
		256, 256,	/* window center */
		0, 0, 0,	/* cursor */
		PIXEL_NULL,	/* page_base */
		PIXEL_NULL,	/* page_curp */
		PIXEL_NULL,	/* page_endp */
		-1,		/* page_no */
		0,		/* page_ref */
		0L,		/* page_curpos */
		0L,		/* page_pixels */
		0		/* debug */
};

HIDDEN int	is_linear_cmap();

/*
 * Our image (window) pixwin
 * XXX WARNING: this should be in suninfo but isn't there yet
 * because of signal routines that need to find it.
 */
static Pixwin	*imagepw;

/*
 *  Per SUN (window or device) state information
 *  Too much for just the if_u[1-6] area now.
 */
struct suninfo
	{
	Window	frame;
	Window	canvas;
	short	su_curs_on;
	short	su_cmap_flag;
	short	su_xcursor;
	short	su_ycursor;
	short	su_depth;
	int	su_shmid;	/* shared memory ID */
	int	su_mode;
	};
#define if_mem		u2.p	/* shared memory pointer */
#define if_cmap		u3.p	/* color map in shared memory */
#define if_windowfd	u4.l	/* f. b. window file descriptor under SUNTOOLS */
#define sunrop(dx,dy,w,h,op,pix,sx,sy) \
		if( sun_pixwin ) \
			pw_rop(SUNPW(ifp),dx,dy,w,h,op,pix,sx,sy); \
		else \
			pr_rop(SUNPR(ifp),dx,dy,w,h,PIX_DONTCLIP|op,pix,sx,sy)

#define sunput(dx,dy,v) \
		if( sun_pixwin ) \
			pw_put(SUNPW(ifp),dx,dy,v); \
		else \
			pr_put(SUNPR(ifp),dx,dy,v);

#define sunreplrop(dx,dy,dw,dh,op,spr,sx,sy) \
		if( sun_pixwin ) \
			pw_replrop(SUNPW(ifp),dx,dy,dw,dh,op,spr,sx,sy); \
		else \
			pr_replrop(SUNPR(ifp),dx,dy,dw,dh,op,spr,sx,sy);

#define SUNPW(ptr)	((Pixwin *)((ptr)->u5.p))
#define SUNPWL(ptr)	((ptr)->u5.p)	/* left hand side version. */
#define SUNPR(ptr)	((Pixrect *)((ptr)->u5.p))
#define SUNPRL(ptr)	((ptr)->u5.p)	/* left hand side version. */
#define	SUN(ptr)	((struct suninfo *)((ptr)->u1.p))
#define SUNL(ptr)	((ptr)->u1.p)	/* left hand side version. */
#define CMR(x)		((struct sun_cmap *)((x)->if_cmap))->cmr
#define CMG(x)		((struct sun_cmap *)((x)->if_cmap))->cmg
#define CMB(x)		((struct sun_cmap *)((x)->if_cmap))->cmb

static int	sun_damaged = FALSE; /* SIGWINCH fired, need to repair damage. */
static int	sun_pixwin = FALSE;  /* Running under SUNTOOLS. */
static RGBpixel black = { 0, 0, 0 };
struct sun_cmap {
	unsigned char	cmr[256];
	unsigned char	cmg[256];
	unsigned char	cmb[256];
};

/* dither pattern (threshold level) for Black & white dithering */
static short	dither[MAXDITHERSZ][MAXDITHERSZ] = {
	  6,  51,  14,  78,   8,  57,  16,  86,
	118,  22, 178,  34, 130,  25, 197,  37,
	 18,  96,  10,  63,  20, 106,  12,  70,
	219,  42, 145,  27, 243,  46, 160,  30,
	  9,  60,  17,  91,   7,  54,  15,  82,
	137,  26, 208,  40, 124,  23, 187,  36,
	 21, 112,  13,  74,  19, 101,  11,  66,
	254,  49, 169,  32, 230,  44, 152,  29
};

/*
 *	Sun Hardware colormap support
 *
 *	The color map is organized as a 6x6x6 colorcube, with 10 extra
 *	entries each for the primary colors and grey values.
 *
 *	entries 0 -> 215 are the color cube
 *	entries 216 -> 225 are extra "red" values
 *	entries 226 -> 235 are extra "green" values
 *	entries 236 -> 245 are extra "blue" values
 *	entries 246 -> 255 are extra "grey" values
 *
 */
/* Our copy of the *hardware* colormap */
static unsigned char redmap[256], grnmap[256], blumap[256];

/* values for color cube entries */
static unsigned char cubevec[6] = { 0, 51, 102, 153, 204, 255 };

/* additional values for primaries */
static unsigned char primary[10] = {
	17, 34, 68, 85, 119, 136, 170, 187, 221, 238
};

/* Arrays containing the indicies of the primary colors and grey values
 * in the color map
 */
static unsigned char redvec[16] = {
0,  216, 217, 1, 218, 219, 2, 220, 221, 3, 222, 223, 4, 224, 225, 5
};
static unsigned char grnvec[16] = {
0, 226, 227, 6, 228, 229, 12, 230, 231, 18, 232, 233, 24, 234, 235, 30
};
static unsigned char bluvec[16] = {
0, 236, 237, 36, 238, 239, 72, 240, 241, 108, 242, 243, 144, 244, 245, 180
};
static unsigned char greyvec[16] = {
0, 246, 247, 43, 248, 249, 86, 250, 251, 129, 252, 253, 172, 254, 255, 215
};

static
double table[10] = {0.0, 0.1, -0.4, 0.2, -0.3, 0.3, -0.2, 0.4, -0.1, 0.5};
static double *noise_ptr = table, *end_table = &table[10];
#define NOISE() (noise_ptr < end_table ? *noise_ptr++ : *(noise_ptr=table) )
#define MAG1 51.0
#define MAG2 17.0
#define DITHER(d, v, noise, mag) { \
	if ( (d = v + noise * mag) > 255 ) d = 255; \
	else if (d < 0) d = 0; }

/*	c o n v D I T H E R G B
 *
 *	Convert a single RGBpixel to its corresponding entry in the Sun
 *	colormap, dithering as we go.
 */
HIDDEN unsigned char convDITHERGB(v)
register RGBpixel *v;
{
	register int r, g, b;
	double dr, dg, db;
	dr = NOISE(); DITHER(r, (*v)[RED], dr, MAG1); r = (r+26) / 51;
	dg = NOISE(); DITHER(g, (*v)[GRN], dg, MAG1); g = (g+26) / 51;
	db = NOISE(); DITHER(b, (*v)[BLU], db, MAG1); b = (b+26) / 51;
	if (r == g) {
		if (r == b) {
			DITHER(r, (*v)[RED], dr, MAG2);
			DITHER(g, (*v)[GRN], dg, MAG2);
			DITHER(b, (*v)[BLU], db, MAG2);
			return (greyvec[ ( (r + g + b)/3) >> 4]);
		}else if (r == 0) {
			DITHER(r, (*v)[RED], dr, MAG2);
			DITHER(g, (*v)[GRN], dg, MAG2);
			DITHER(b, (*v)[BLU], db, MAG2);
			return (bluvec[ b >> 4]);
		}else return ( r + g * 6 + b * 36 );
	}else if (g == b && g == 0) {
		DITHER(r, (*v)[RED], dr, MAG2);
		DITHER(g, (*v)[GRN], dg, MAG2);
		DITHER(b, (*v)[BLU], db, MAG2);
		return (redvec [ r >> 4]);
	}else if (r == b && r == 0) {
		DITHER(r, (*v)[RED], dr, MAG2);
		DITHER(g, (*v)[GRN], dg, MAG2);
		DITHER(b, (*v)[BLU], db, MAG2);
		return (grnvec[ g >> 4]);
	}else return ( r + g * 6 + b * 36 );
}
/*
 *	c o n v R G B
 *
 *	convert a single RGBpixel to its corresponding entry in the Sun
 *	colormap.
 */
unsigned char convRGB(v)
register RGBpixel *v;
{
	register int r, g, b;

	r = ( (*v)[RED]+26 ) / 51;
	g = ( (*v)[GRN]+26 ) / 51;
	b = ( (*v)[BLU]+26 ) / 51;

	if ( r == g )  {
		if( r == b )  {
			/* all grey, take average */
			return greyvec[( ((*v)[RED]+(*v)[GRN]+(*v)[BLU]) / 3 ) /16];
		}
		else if (r == 0)  {
			/* r=g=0, all blue */
			return bluvec[((*v)[BLU])/16];
		}
		else	return r + g * 6 + b * 36;
	}
	else if (g == b && g == 0)  {
		/* all red */
		return redvec[((*v)[RED])/16];
	}
	else if (r == b && r == 0)  {
		/* all green */
		return grnvec[((*v)[GRN])/16];
	}
	else	return r + g * 6 + b * 36;
}


/*
 *	G E N M A P
 *
 *	initialize the Sun harware colormap
 */
void genmap(rmap, gmap, bmap)
unsigned char rmap[], gmap[], bmap[];
{
	register r, g, b;

	/* build the basic color cube */
	for (r=0 ; r < 6 ; r++)
		for (g=0 ; g < 6 ; g ++)
			for (b=0 ; b < 6 ; b++) {
				rmap[r + g * 6 + b * 36] = cubevec[r];
				gmap[r + g * 6 + b * 36] = cubevec[g];
				bmap[r + g * 6 + b * 36] = cubevec[b];
			}

	/* put in the linear sections */
	for (r=216 ; r < 226 ; ++r) {
		rmap[r] = primary[r-216];	/* red */
		gmap[r] = bmap[r] = 0;
	}

	for (g=226 ; g < 236 ; ++g) {
		gmap[g] = primary[g-226];	/* green */
		rmap[g] = bmap[g] = 0;
	}

	for (b=236 ; b < 246 ; ++b) {
		bmap[b] = primary[b-236];	/* blue */
		rmap[b] = gmap[b] = 0;
	}

	for (r=246 ; r < 256 ; ++r) {		/* grey */
		rmap[r] = gmap[r] =
		bmap[r] = primary[r-246];
	}
}


#define SUN_CMAPVAL( p, o )\
	if( SUN(ifp)->su_cmap_flag )\
		{\
		(o)[RED] = CMR(ifp)[(*p)[RED]];\
		(o)[GRN] = CMG(ifp)[(*p)[GRN]];\
		(o)[BLU] = CMB(ifp)[(*p)[BLU]];\
		}\
	else	COPYRGB( o, *p );

/*
 *  The mode has several independent bits:
 *	SHARED -vs- MALLOC'ed memory for the image
 *	TRANSIENT -vs- LINGERING windows
 *	DitheredColor -vs- UnDitheredColor
 */
#define MODE_1MASK	(1<<0)
#define MODE_1SHARED	(0<<0)		/* Use Shared memory */
#define MODE_1MALLOC	(1<<0)		/* Use malloc memory */

#define MODE_2MASK	(1<<1)
#define MODE_2TRANSIENT	(0<<1)
#define MODE_2LINGERING (1<<1)

#define MODE_3MASK	(1<<2)
#define MODE_3UNDITHERGB (0<<2)
#define MODE_3DITHERGB	(1<<2)		/* dithered colors on color display */

#define MODE_15MASK	(1<<14)
#define MODE_15NORMAL	(0<<14)
#define MODE_15ZAP	(1<<14)

static struct modeflags {
	char	c;
	long	mask;
	long	value;
	char	*help;
} modeflags[] = {
	{ 'p',	MODE_1MASK, MODE_1MALLOC,
		"Private memory - else shared" },
	{ 'l',	MODE_2MASK, MODE_2LINGERING,
		"Lingering window - else transient" },
	{ 'd',  MODE_3MASK, MODE_3DITHERGB,
		"Color dithering - else undithered colors" },
	{ 'z',	MODE_15MASK, MODE_15ZAP,
		"Zap (free) shared memory" },
	{ '\0', 0, 0, "" }
};

HIDDEN int
sun_sigwinch( sig )
{
	sun_damaged = TRUE;
	return	sig;
}

HIDDEN int
sun_sigalarm( sig )
{
	if( imagepw == (Pixwin *) NULL )
		return	sig;
	(void) signal( SIGALRM, sun_sigalarm );
	if( sun_damaged ) {
		pw_damaged( imagepw );
		pw_repairretained( imagepw );
		pw_donedamaged( imagepw );
		sun_damaged = FALSE;
	}
	alarm( TIMEOUT );
	return	sig;
}

#ifdef INEFFICIENT
HIDDEN void
sun_storepixel( ifp, x, y, p, count )
FBIO			*ifp;
int			x, y;
register RGBpixel	*p;
register int		count;
{
	register RGBpixel	*memp;
	for(	memp =	(RGBpixel *)
			(&ifp->if_mem[(y*ifp->if_width+x)*sizeof(RGBpixel)]);
		count > 0;
		memp++, p++, count--
		)
		{
			COPYRGB( *memp, *p );
		}
	return;
}
#endif

HIDDEN void
sun_storebackground( ifp, x, y, p, count )
FBIO			*ifp;
int			x, y;
register RGBpixel	*p;
register int		count;
{
	register RGBpixel	*memp;

	memp = (RGBpixel *)
		(&ifp->if_mem[(y*ifp->if_width+x)*sizeof(RGBpixel)]);

	if( (p == (RGBpixel *)0) ||
	    ((*p)[RED] == 0 && (*p)[GRN] == 0 && (*p)[BLU] == 0) ) {
		/* black */
		bzero( memp, count*sizeof(RGBpixel) );
	} else if( ((*p)[RED] == (*p)[GRN]) && ((*p)[GRN] == (*p)[BLU]) ) {
		/* R, G, and B are equal */
		memset( memp, (*p)[RED] );
	} else {
		while( count-- > 0 ) {
			COPYRGB( *memp, *p );
			memp++;
		}
	}
	return;
}

/* These lock routines are unused.  They do not seem to provide any
speedup when bracketing raster op routines.  This may pan out
differently when other processes are actively competing for raster ops.
*/
HIDDEN void sun_lock( ifp ) FBIO *ifp;
{
	if( sun_pixwin ) {
		 /* Lock the display and get the cursor out of the way. */
		pw_lock( SUNPW(ifp), SUNPW(ifp)->pw_pixrect );
	}
	return;
}

HIDDEN void
sun_unlock( ifp )
FBIO	*ifp;
{
	if( sun_pixwin ) {
		/* Release display lock and restore cursor. */
		pw_unlock( SUNPW(ifp) );
	}
	return;
}

/* Dither pattern pixrect for fast raster ops.  (1bit displays) */
static char	dither_mpr_buf[8];
mpr_static( dither_mpr, DITHERSZ, DITHERSZ, 1, (short *)dither_mpr_buf );

/* Straight pixrect for grey-scale and color devices. (8bit) */
static char	pixel_mpr_buf[1];
mpr_static( pixel_mpr, 1, 1, 8, (short *)pixel_mpr_buf );

HIDDEN void
sun_rectclear( ifp, xlft, ytop, width, height, pp )
FBIO		*ifp;
int		xlft, ytop, width, height;
RGBpixel	*pp;
{
	static int		lastvalue = 0;
	register int		value;
	RGBpixel		v;

	/*fb_log( "sun_rectclear(%d,%d,%d,%d)\n", xlft, ytop, width, height ); /* XXX-debug */
	/* Get color map value for pixel. */
	SUN_CMAPVAL( pp, v );

	if( SUN(ifp)->su_depth == 1 ) {
		/* Construct dither pattern in memory pixrect. */
		value = (v[RED] + v[GRN] + v[BLU]);
		if( value == 0 ) {
			sunrop( xlft, ytop, width, height,
				PIX_SET, (Pixrect *) 0, 0, 0
				);
			return;
		}
		if( value != lastvalue ) {
			register int	i, j;
			for( i = 0; i < DITHERSZ; i++ )
				for( j = 0; j < DITHERSZ; j++ )
					{	register int	op;
					op = (value < dither[j][i]*3) ?
						PIX_SET : PIX_CLR;
					pr_rop( &dither_mpr, j, i, 1, 1, op,
						(Pixrect *) NULL, 0, 0 );
					}
			lastvalue = value;
		}
		/* Blat out dither pattern. */
		sunreplrop( xlft, ytop, width, height,
			PIX_SRC, &dither_mpr, DITHMASK(xlft), DITHMASK(ytop) );
	}
	else {
		/* Grey-scale or color image. */
		pixel_mpr_buf[0] = convRGB(v);

		sunreplrop(xlft, ytop, width, height,
			PIX_SRC, &pixel_mpr, 0, 0);
	}
	return;
}

/*
 * Scanline dither pattern pixrect.  One each for 1bit, and 8bit
 * deep displays.  If DITHERSZ or BITSDEEP > sizeof(char) you must
 * extend the length of scan_mpr_buf accordingly.
 */
static char	scan_mpr_buf[XMAXWINDOW];
mpr_static( scan1_mpr, XMAXWINDOW, DITHERSZ, 1, (short *)scan_mpr_buf );
mpr_static( scan8_mpr, XMAXWINDOW, DITHERSZ, 8, (short *)scan_mpr_buf );

/*
 * XXX - 512 calls to scanwrite consumed 10.8 seconds or 78% of
 * the runtime of pix-fb dragon.pix on a monochrome Sun.
 */
HIDDEN void
sun_scanwrite( ifp, xlft, ybtm, xrgt, pp )
FBIO		*ifp;
int		xlft;
int		ybtm;
register int	xrgt;
RGBpixel	*pp;
{
	register int	sy = YIMAGE2SCR( ybtm+1 ) + 1;
	register int	xzoom = ifp->if_xzoom;
	int		xl = XIMAGE2SCR( xlft );

	/*fb_log( "sun_scanwrite(%d,%d,%d,0x%x)\n", xlft, ybtm, xrgt, pp );
		/* XXX-debug */

	if( SUN(ifp)->su_depth == 1 ) {
		register int	x;
		register int	sx = xl;
		int		yzoom = ifp->if_yzoom;

		/* Clear buffer to black. */
		(void) memset( scan_mpr_buf, 0xff, XMAXWINDOW );

		for( x = xlft; x <= xrgt; x++, sx += xzoom, pp++ ) {
			register int	value;
			RGBpixel	v;

			/* Get color map value for pixel. */
			SUN_CMAPVAL( pp, v );
			value = v[RED] + v[GRN] + v[BLU];

			if( value ) {
				register int	maxdy = yzoom < DITHERSZ ?
							yzoom : DITHERSZ;
				register int	dy;
				register int	yoffset = sx;

				/* Construct dither pattern. */
				value /= 3;
				for( dy = 0; dy < maxdy; dy++, yoffset += XMAXWINDOW ) {
					register int dx;
					register int ydit = DITHMASK(sy+dy);
					for( dx = 0; dx < xzoom; dx++ ) {
						register int xdit =
							DITHMASK(sx+dx);
						if( value >= dither[xdit][ydit] )
							scan_mpr_buf[(yoffset+dx)>>3] &= ~(0x80>>xdit);
					}
				}
			}
		}
		sunreplrop( xl, sy,
			(xrgt-xlft+1)*xzoom, yzoom,
			PIX_SRC, &scan1_mpr,
			xl, 0 );
	}
	else {
		/* Grey-scale or color image. */
		register int	x;
		register int	sx = xl;
		unsigned char	(*convf)();
		if( (SUN(ifp)->su_mode & MODE_3MASK) == MODE_3DITHERGB )
			convf = convDITHERGB;
		else
			convf = convRGB;

		for( x = xlft; x <= xrgt; x++, pp++, sx += xzoom ) {
			register int	dx, value, r, g, b;
			RGBpixel	v;


			/* Get Software color map value for pixel. */
			SUN_CMAPVAL( pp, v );
			/* Convert RGBpixel to 8 bit Sun pixel */
			value = (*convf)(v);

			for( dx = 0; dx < xzoom; dx++ )
				scan_mpr_buf[sx+dx] = value;
		}
		sunreplrop( xl, sy,
			(xrgt-xlft+1)*xzoom, ifp->if_yzoom,
			PIX_SRC, &scan8_mpr,
			xl, 0 );
	}
	return;
}

HIDDEN void
sun_rectwrite( ifp, xmin, ymin, xmax, ymax, buf, offset )
register FBIO		*ifp;
int			xmin, ymin;
int			xmax;
register int		ymax;
RGBpixel		*buf;
register int		offset;
{
	register int		y;
	register RGBpixel	*p;
	/*fb_log( "sun_rectwrite(xmin=%d,ymin=%d,xmax=%d,ymax=%d,buf=0x%x,offset=%d)\n",
		xmin, ymin, xmax, ymax, buf, offset ); /* XXX--debug */
	p = buf-offset+ymin*ifp->if_width+xmin;
	for( y = ymin; y <= ymax; y++, p += ifp->if_width )
		sun_scanwrite(	ifp, xmin, y, xmax, p );
	return;
}

HIDDEN void
sun_repaint( ifp )
register FBIO	*ifp;
{
	register int	i;
	register int	ymin, ymax;
	int		xmin, xmax;
	int		xwidth;
	int		xscroff, yscroff;
	int		xscrpad, yscrpad;
	/*fb_log( "sun_repaint: xzoom=%d yzoom=%d xcenter=%d ycenter=%d\n",
		ifp->if_xzoom, ifp->if_yzoom,
		ifp->if_xcenter, ifp->if_ycenter ); /* XXX-debug */
	xscroff = yscroff = 0;
	xscrpad = yscrpad = 0;
	xwidth = ifp->if_width/ifp->if_xzoom;
	i = xwidth/2;
	xmin = ifp->if_xcenter - i;
	xmax = ifp->if_xcenter + i - 1;
	i = (ifp->if_height/2)/ifp->if_yzoom;
	ymin = ifp->if_ycenter - i;
	ymax = ifp->if_ycenter + i - 1;
	if( xmin < 0 ) {
		xscroff = -xmin * ifp->if_xzoom;
		xmin = 0;
	}
	if( ymin < 0 ) {
		yscroff = -ymin * ifp->if_yzoom;
		ymin = 0;
	}
	if( xmax > ifp->if_width-1 ) {
		xscrpad = (xmax-(ifp->if_width-1))*ifp->if_xzoom;
		xmax = ifp->if_width-1;
	}
	if( ymax > ifp->if_height-1 ) {
		yscrpad = (ymax-(ifp->if_height-1))*ifp->if_yzoom;
		ymax = ifp->if_height-1;
	}
	/* Blank out area left of image.			*/
	if( xscroff > 0 )
		sun_rectclear(	ifp, 0, 0, xscroff, ifp->if_height,
				(RGBpixel *) black );
	/* Blank out area below image.			*/
	if( yscroff > 0 )
		sun_rectclear(	ifp, 0, ifp->if_height-yscroff,
				ifp->if_width, yscroff,
				(RGBpixel *) black );
	/* Blank out area right of image.			*/
	if( xscrpad > 0 )
		sun_rectclear(	ifp, ifp->if_width-xscrpad, 0,
				xscrpad, ifp->if_height,
				(RGBpixel *) black );
	/* Blank out area above image.			*/
	if( yscrpad > 0 )
		sun_rectclear(	ifp, 0, 0,
				ifp->if_width, yscrpad,
				(RGBpixel *) black );
	sun_rectwrite( ifp, xmin, ymin, xmax, ymax, (RGBpixel *)ifp->if_mem, 0 );
	return;
}

/*
 *			S U N _ G E T M E M
 *
 *  Because there is no hardware zoom or pan, we need to repaint the
 *  screen (with big pixels) to implement these operations.
 *  This means that the actual "contents" of the frame buffer need
 *  to be stored somewhere else.  If possible, we allocate a shared
 *  memory segment to contain that image.  This has several advantages,
 *  the most important being that although output mode requires monochrome,
 *  dithering, or up to 8-bit color on the Suns, pixel-readbacks still
 *  give the full 24-bits of color.  System V shared memory persists
 *  until explicitly killed, so this also means that under SUNTOOLS, the
 *  previous contents of the frame buffer still exist, and can be again
 *  accessed, even though the windows are transient, per-process.
 */
HIDDEN int
sun_getmem( ifp )
FBIO	*ifp;
{
	int	pixsize;
	int	size;
	int	new = 0;
	static char	*sp = NULL;
	extern caddr_t	sbrk();

	errno = 0;
	pixsize = ifp->if_max_height * ifp->if_max_width * sizeof(RGBpixel);
	size = pixsize + sizeof(struct sun_cmap);
	size = (size + 4096-1) & ~(4096-1);
#define SHMEM_KEY	42
	/* Do once per process. */
	if( sp == (char *) NULL ) {
		if( (SUN(ifp)->su_mode & MODE_1MASK) == MODE_1MALLOC )
			goto localmem;

		/* First try to attach to an existing shared memory */
		if( (SUN(ifp)->su_shmid = shmget(SHMEM_KEY, size, 0)) < 0 ) {
			/* No existing one, create a new one */
			SUN(ifp)->su_shmid =
				shmget( SHMEM_KEY, size, IPC_CREAT|0666 );
			if( SUN(ifp)->su_shmid < 0 )
				goto fail;
			new = 1;
		}
		/* Open the segment Read/Write, near the current break */
		if( (sp = shmat( SUN(ifp)->su_shmid, 0, 0 )) < 0 ) {
			fb_log("shmat returned x%x, errno=%d\n", sp, errno );
			goto fail;
		}
		goto	common;
fail:
		fb_log("sun_getmem:  Unable to attach to shared memory, using private.\n");
		/* Change it to local */
		SUN(ifp)->su_mode = (SUN(ifp)->su_mode & ~MODE_1MASK)
			| MODE_1MALLOC;
localmem:
		if( (sp = malloc( size )) == NULL ) {
			fb_log( "sun_getmem:  malloc failure, couldn't allocate %d bytes\n", size );
			return	-1;
		}
		new = 1;
	}
common:
	ifp->if_mem = sp;
	ifp->if_cmap = sp + pixsize;	/* Color map at end of area */

	/* Initialize the colormap and clear memory frame buffer to black */
	if( new ) {
		sun_wmap( ifp, COLORMAP_NULL );
		sun_storebackground( ifp, 0, 0, black, ifp->if_max_width*ifp->if_max_height );
	}
	return	0;
}

/*
 *			S U N _ Z A P M E M
 */
HIDDEN void
sun_zapmem()
{
	int	shmid;
	int	i;
	if( (shmid = shmget( SHMEM_KEY, 0, 0 )) < 0 ) {
		fb_log( "sun_zapmem shmget failed, errno=%d\n", errno );
		return;
	}

	i = shmctl( shmid, IPC_RMID, 0 );
	if( i < 0 ) {
		fb_log( "sun_zapmem shmctl failed, errno=%d\n", errno );
		return;
	}
	fb_log( "if_sun: shared memory released\n" );
	return;
}

/*
 *			S U N _ O P E N
 */
HIDDEN int
sun_open(ifp, file, width, height)
FBIO	*ifp;
char	*file;
int	width, height;
{
	char		sun_parentwinname[WIN_NAMESIZE];
	Rect		winrect;
	int		x;
	struct pr_prpos	where;
	struct pixfont	*myfont;
	int	mode;

	FB_CK_FBIO(ifp);

	/*
	 *  First, attempt to determine operating mode for this open,
	 *  based upon the "unit number" or flags.
	 *  file = "/dev/sun###"
	 *  The default mode is zero.
	 */
	mode = 0;

	if( file != NULL )  {
		register char *cp;
		struct	modeflags *mfp;

		if( strncmp(file, "/dev/sun", 8) ) {
			/* How did this happen?? */
			mode = 0;
		}
		else {
			/* Parse the options as either ascii mnemonics or
			 * as decimal bit representation
			 */
			if ( isdigit(*(cp= &file[8])) || *cp == '-') mode = atoi(cp);
			else {
				for ( ;*cp != '\0' && !isspace(*cp) ; ++cp) {
					/* look for character in mode list */
					for( mfp = modeflags; mfp->c != '\0'; mfp++ )
						if( mfp->c == *cp ) {
							mode = (mode&~mfp->mask)|mfp->value;
							break;
						}
					if( mfp->c == '\0' && *cp != '-' )
						fb_log( "if_sun: unknown option '%c' ignored\n", *cp );
				}
			}
		}

		if( (mode & MODE_15MASK) == MODE_15ZAP ) {
			/* Only task: Attempt to release shared memory segment */
			sun_zapmem();
			return(-1);
		}

		/* Pick off just the mode bits of interest here */
		mode &= (MODE_1MASK | MODE_2MASK | MODE_3MASK);
	}

	if( width <= 0 )
		width = ifp->if_width;
	if( height <= 0 )
		height = ifp->if_height;
	if ( width > ifp->if_max_width)
		width = ifp->if_max_width;
	if ( height > ifp->if_max_height)
		height = ifp->if_max_height;

	if( SUN(ifp) != (struct suninfo *) NULL ) {
		fb_log( "sun_open, already open\n" );
		return	-1;	/* FAIL */
	}
	if( (SUNL(ifp) = calloc( 1, sizeof(struct suninfo) )) == NULL ) {
		fb_log( "sun_open:  suninfo calloc failed\n" );
		return	-1;
	}
	SUN(ifp)->su_mode = mode;
	SUN(ifp)->su_shmid = -1;
#define WHICH_FONT	"/usr/lib/fonts/fixedwidthfonts/screen.b.14"
	myfont = pf_open( WHICH_FONT );
	if( myfont == 0 )  {
		fb_log("sun_open: pf_open %s failure\n", WHICH_FONT);
		return(-1);
	}

	/*
	 * Initialize what we want an 8bit *hardware* colormap to
	 * look like.  Note that our software colormap is totally
	 * separate from this.
	 */

	genmap(redmap, grnmap, blumap);

	/* Create window. */
	if( sun_pixwin = (we_getgfxwindow(sun_parentwinname) == 0) ) {
		/************** SunView Open **************/
		Window	frame;
		Canvas	canvas;

		/* Make a frame and a canvas to go in it */
		frame = window_create(NULL, FRAME,
			      FRAME_LABEL, "Frame Buffer", 0);
		if( frame == 0 )  {
			fb_log("sun_open: window_create frame failure\n");
			return(-1);
		}
		/* XXX - "command line" args? pg.51 */
		canvas = window_create(frame, CANVAS,
			      WIN_WIDTH, width,
			      WIN_HEIGHT, height, 0);
		if( canvas == 0 )  {
			fb_log("sun_open: window_create canvas failure\n");
			return(-1);
		}
		/* Fit window to canvas (width+10, height+20) */
		window_fit(frame);

		/* get the canvas pixwin to draw in */
		imagepw = canvas_pixwin(canvas);
		SUNPWL(ifp) = (char *) imagepw;

		winrect = *((Rect *)window_get(canvas, WIN_RECT));
		if( width > winrect.r_width )
			width = winrect.r_width;
		if( height > winrect.r_height )
			height = winrect.r_height;

		ifp->if_windowfd = imagepw->pw_clipdata->pwcd_windowfd;
		SUN(ifp)->su_depth = SUNPW(ifp)->pw_pixrect->pr_depth;
		SUN(ifp)->frame = frame;
		SUN(ifp)->canvas = canvas;

		/* set up window input */
		/*window_set(canvas, WIN_EVENT_PROC, input_eater, 0);*/

		window_set(frame, WIN_SHOW, TRUE, 0);	/* display it! */
		(void) notify_dispatch();		/* process event */

		if( SUN(ifp)->su_depth == 8 ) {
			/* set a new cms name; initialize it */
			x = pw_setcmsname(imagepw, "libfb");
			x = pw_putcolormap(imagepw, 0, 256,
					redmap, grnmap, blumap);
		}
	}
	else {
		/************ Raw Screen Open ************/
		Pixrect	*screenpr;
		Pixrect	*windowpr;

		screenpr = pr_open( "/dev/fb" );
		if( screenpr == (Pixrect *) NULL ) {
			fb_log("sun_open: pr_open /dev/fb failure\n");
			return(-1);
		}
		windowpr = pr_region(	screenpr,
			XMAXSCREEN-width-BORDER*2,
			YMAXSCREEN-(height+BANNER+BORDER*3),
			width+BORDER*2, height+BANNER+BORDER*3
			);
		if( windowpr == 0 )  {
			fb_log("sun_open: pr_region failure\n");
			return(-1);
		}

		SUNPRL(ifp) = (char *)
			pr_region(	windowpr,
					BORDER, BANNER+BORDER*2,
					width, height
					);
		/* Outer border is black. */
		pr_rop( windowpr, 0, 0,
			width+BORDER*2, height+BANNER+BORDER*3,
			PIX_DONTCLIP | PIX_SET,
			(Pixrect *) NULL, 0, 0 );
		/* Inner border is white. */
		pr_rop( windowpr, 1, 1,
			width+2, height+BANNER+BORDER*2,
			PIX_DONTCLIP | PIX_CLR,
			(Pixrect *) NULL, 0, 0 );
		/* Black out title bar. */
		pr_rop( windowpr, BORDER, BORDER,
			width, BANNER,
			PIX_DONTCLIP | PIX_SET,
			(Pixrect *) NULL, 0, 0 );
		/* Draw title in title bar (banner). */
		where.pr = windowpr;
		where.pos.x = TITLEXOFFSET;
		where.pos.y = BANNER - TITLEYOFFSET;
		pf_ttext(	where,
				PIX_CLR,
				myfont, "BRL libfb Frame Buffer" );
		SUN(ifp)->su_depth = SUNPR(ifp)->pr_depth;
		if( SUN(ifp)->su_depth == 8 ) {
			pr_putcolormap(windowpr, 0, 256,
				redmap, grnmap, blumap );
		}
	}
	pf_close( myfont );

	if( (SUN(ifp)->su_depth != 1) && (SUN(ifp)->su_depth != 8) ) {
		fb_log( "if_sun: Can only handle 1bit and 8bit deep displays (not %d)\n",
			SUN(ifp)->su_depth );
		return	-1;
	}

	ifp->if_width = width;
	ifp->if_height = height;
	ifp->if_xzoom = 1;
	ifp->if_yzoom = 1;
	ifp->if_xcenter = width/2;
	ifp->if_ycenter = height/2;
	sun_getmem( ifp );

	/* Must call "is_linear_cmap" AFTER "sun_getmem" which allocates
		space for the color map.				*/
	SUN(ifp)->su_cmap_flag = !is_linear_cmap(ifp);

	if( (SUN(ifp)->su_mode & MODE_1MASK) == MODE_1SHARED ) {
		/* Redraw 24-bit image from shared memory */
		sun_repaint( ifp );
	}

	return	0;		/* "Success" */
}

/*
 *			S U N _ C L O S E
 */
HIDDEN int
sun_close(ifp)
FBIO	*ifp;
{
	register Pixrect *p;
	register int i;

	if( SUNL(ifp) == (char *) NULL ) {
		fb_log( "sun_close: frame buffer not open.\n" );
		return	-1;
	}
	if( sun_pixwin ) {
		if( (SUN(ifp)->su_mode & MODE_2MASK) == MODE_2LINGERING ) {
			if( linger(ifp) )
				return	0;  /* parent leaves the display */
		}
		/* child or single process */
		alarm( 0 ); /* Turn off window redraw daemon. */
		window_set(SUN(ifp)->frame, FRAME_NO_CONFIRM, TRUE, 0);
		window_destroy(SUN(ifp)->frame);
		imagepw = NULL;
	}
	else {
		/* if the user hasn't requested a lingering buffer, clear the
		 * colormap so that we can see to log in on the console again
		 */
		if( (SUN(ifp)->su_mode & MODE_2MASK) != MODE_2LINGERING) {
			p = SUNPR(ifp);
			redmap[0] = grnmap[0] = blumap[0] = 255;
			for (i=1 ; i < 256 ; ++i)
				redmap[i] = grnmap[i] = blumap[i] = 0;
			pr_putcolormap(p, 0, 256, redmap, grnmap, blumap);
		}
		pr_close( SUNPR(ifp) );
	}
	/* free up memory associated with image */
	if( SUN(ifp)->su_shmid >= 0 ) {
		/* detach from shared memory */
		if( shmdt( ifp->if_mem ) == -1 ) {
			fb_log("sun_close shmdt failed, errno=%d\n", errno);
			return -1;
		}
	} else {
		/* free private memory */
		(void)free( ifp->if_mem );
	}
	(void) free( (char *) SUNL(ifp) );
	SUNL(ifp) = NULL;
	return	0;
}

/*
 *			S U N _ P O L L
 */
HIDDEN int
sun_poll(ifp)
FBIO	*ifp;
{
	/* XXX - Need to empty event queue here, not just one event */
	notify_dispatch();
}

/*
 *			S U N _ F R E E
 *
 *  Like close, but also releases shared memory if any.
 */
HIDDEN int
sun_free(ifp)
FBIO	*ifp;
{
	register Pixrect *p;
	register int i;

	if( SUNL(ifp) == (char *) NULL ) {
		fb_log( "sun_free: frame buffer not open.\n" );
		return	-1;
	}
	if( sun_pixwin ) {
		/* child or single process */
		alarm( 0 ); /* Turn off window redraw daemon. */
		window_set(SUN(ifp)->frame, FRAME_NO_CONFIRM, TRUE, 0);
		window_destroy(SUN(ifp)->frame);
		imagepw = NULL;
	}
	else {
		/* if the user hasn't requested a lingering buffer, clear the
		 * colormap so that we can see to log in on the console again
		 */
		if( (SUN(ifp)->su_mode & MODE_2MASK) != MODE_2LINGERING) {
			p = SUNPR(ifp);
			redmap[0] = grnmap[0] = blumap[0] = 255;
			for (i=1 ; i < 256 ; ++i)
				redmap[i] = grnmap[i] = blumap[i] = 0;
			pr_putcolormap(p, 0, 256, redmap, grnmap, blumap);
		}
		pr_close( SUNPR(ifp) );
	}
	/* free up memory associated with image */
	if( SUN(ifp)->su_shmid >= 0 ) {
		/* release shared memory */
		sun_zapmem();
	} else {
		/* free private memory */
		(void)free( ifp->if_mem );
	}
	(void) free( (char *) SUNL(ifp) );
	SUNL(ifp) = NULL;
	return	0;
}

/*
 *			S U N _ C L E A R
 */
HIDDEN int
sun_clear(ifp, pp)
FBIO			*ifp;
register RGBpixel	*pp;
{
	if( pp == (RGBpixel *) NULL )
		pp = (RGBpixel *) black;

	/* Clear 24-bit image in shared memory. */
	sun_storebackground( ifp, 0, 0, pp, ifp->if_width*ifp->if_height );

	/* Clear entire screen. */
	sun_rectclear( ifp, 0, 0, ifp->if_width, ifp->if_height, pp );
	return	0;
}

/*
 *			S U N _ V I E W
 */
HIDDEN int
sun_view(ifp, xcenter, ycenter, xzoom, yzoom)
FBIO	*ifp;
int     xcenter, ycenter;
int	xzoom, yzoom;
{
	/*fb_log( "sun_window_set(0x%x,%d,%d)\n", ifp, xcenter , ycenter );*/
	if( ifp->if_xcenter == xcenter && ifp->if_ycenter == ycenter
	 && ifp->if_xzoom == xzoom && ifp->if_yzoom == yzoom )
		return	0;
	if( xcenter < 0 || xcenter >= ifp->if_width )
		return	-1;
	if( ycenter < 0 || ycenter >= ifp->if_height )
		return	-1;
	if( xzoom >= ifp->if_width || yzoom >= ifp->if_height )
		return	-1;

	ifp->if_xcenter = xcenter;
	ifp->if_ycenter = ycenter;
	ifp->if_xzoom = xzoom;
	ifp->if_yzoom = yzoom;

	/* Redraw 24-bit image from memory. */
	sun_repaint(ifp);
	return	0;
}

/*
 *			S U N _ S E T C U R S O R
 */
HIDDEN int
sun_setcursor(ifp, bits, xbits, ybits, xorig, yorig )
FBIO		*ifp;
unsigned char	*bits;
int		xbits, ybits;
int		xorig, yorig;
{
	register int	xbytes;
	register int	y;
	Cursor		newcursor;
	/* Check size of cursor. */
	if( xbits < 0 )
		return	-1;
	if( xbits > 16 )
		xbits = 16;
	if( ybits < 0 )
		return	-1;
	if( ybits > 16 )
		ybits = 16;
	if( (xbytes = xbits / 8) * 8 != xbits )
		xbytes++;
#if 0
	for( y = 0; y < ybits; y++ ) {
		newcursor[y] = bits[(y*xbytes)+0] << 8 & 0xFF00;
		if( xbytes == 2 )
			newcursor[y] |= bits[(y*xbytes)+1] & 0x00FF;
	}
#endif
	return	0;
}

/*
 *			S U N _ C U R S O R
 */
HIDDEN int
sun_cursor( ifp, mode, x, y )
FBIO	*ifp;
int	mode;
int	x, y;
{
	short	xmin, ymin;
	register short	i;
	short	xwidth;

	fb_sim_cursor(ifp, mode, x, y);
	if( ! sun_pixwin )
		return	0; /* No cursor outside of suntools yet. */
	SUN(ifp)->su_curs_on = mode;
	if( ! mode ) {
		/* XXX turn off cursor. */
		return	0;
	}
	xwidth = ifp->if_width/ifp->if_xzoom;
	i = xwidth/2;
	xmin = ifp->if_xcenter - i;
	i = (ifp->if_height/2)/ifp->if_yzoom;
	ymin = ifp->if_ycenter - i;
	x -= xmin;
	y -= ymin;
	x *= ifp->if_xzoom;
	y *= ifp->if_yzoom;
	y = ifp->if_height - y;
	/* Move cursor/mouse to <x,y>. */
	if(	x < 1 || x > ifp->if_width
	    ||	y < 1 || y > ifp->if_height
		)
		return	-1;
	/* Translate address from window to tile space. */
	x += BORDER;
	y += BORDER*2 + BANNER;
	SUN(ifp)->su_xcursor = x;
	SUN(ifp)->su_ycursor = y;
	win_setmouseposition( ifp->if_windowfd, x, y );
	return	0;
}

/*
 *			S U N _ G E T C U R S O R
 */
HIDDEN int
sun_getcursor(ifp, mode, x, y)
FBIO	*ifp;
int	*mode;
int	*x, *y;
{
	fb_sim_getcursor(ifp, mode, x, y);	/*XXX*/
	return	0;
}

/*
 *			S U N _ R E A D
 */
HIDDEN int
sun_read(ifp, x, y, p, count)
FBIO			*ifp;
int			x, y;
register RGBpixel	*p;
register int		count;
{
	register RGBpixel	*memp;

	for(	memp =	(RGBpixel *)
			(&ifp->if_mem[(y*ifp->if_width+x)*sizeof(RGBpixel)]);
		count > 0;
		memp++, p++, count--
		)
		{
			COPYRGB( *p, *memp );
		}
	return	count;
}

/*
 *			S U N _ W R I T E
 */
HIDDEN int
sun_write(ifp, x, y, p, count)
register FBIO	*ifp;
int		x, y;
RGBpixel	*p;
register int	count;
{
	int		xmax, ymax;
	register int	xwidth;

	/*fb_log( "sun_write(0x%x,%d,%d,0x%x,%d)\n", ifp, x, y, p, count );
		/* XXX--debug */
	/* Store pixels in memory. */
	/*sun_storepixel( ifp, x, y, p, count );*/
	bcopy( p, &ifp->if_mem[(y*ifp->if_width+x)*sizeof(RGBpixel)],
		count*sizeof(RGBpixel) );

	xwidth = ifp->if_width/ifp->if_xzoom;
	xmax = count >= xwidth-x ? xwidth-1 : x+count-1;
	ymax = y + (count-1)/ ifp->if_width;
	sun_rectwrite( ifp, x, y, xmax, ymax, p, y*ifp->if_width+x );
	return	count;
}

/*
 *			S U N _ R M A P
 */
HIDDEN int
sun_rmap( ifp, cmp )
register FBIO		*ifp;
register ColorMap	*cmp;
{
	register int i;

	/* Just parrot back the stored colormap */
	for( i = 0; i < 256; i++)
		{
		cmp->cm_red[i] = CMR(ifp)[i]<<8;
		cmp->cm_green[i] = CMG(ifp)[i]<<8;
		cmp->cm_blue[i] = CMB(ifp)[i]<<8;
		}
	return	0;
}

/*
 *			I S _ L I N E A R _ C M A P
 *
 *  Check for a color map being linear in R, G, and B.
 *  Returns 1 for linear map, 0 for non-linear map
 *  (ie, non-identity map).
 */
HIDDEN int
is_linear_cmap(ifp)
register FBIO	*ifp;
{
	register int i;
	for( i=0; i<256; i++ )
		{
		if( CMR(ifp)[i] != i )  return(0);
		if( CMG(ifp)[i] != i )  return(0);
		if( CMB(ifp)[i] != i )  return(0);
		}
	return	1;
}

/*
 *			S U N _ W M A P
 */
HIDDEN int
sun_wmap(ifp, cmp)
register FBIO		*ifp;
register ColorMap	*cmp;
{
	register int i;
	if( cmp == COLORMAP_NULL ) {
		for( i = 0; i < 256; i++) {
			CMR(ifp)[i] = i;
			CMG(ifp)[i] = i;
			CMB(ifp)[i] = i;
		}
		if( SUN(ifp)->su_cmap_flag ) {
			SUN(ifp)->su_cmap_flag = FALSE;
			sun_repaint( ifp );
		}
		SUN(ifp)->su_cmap_flag = FALSE;
		return	0;
	}

	for( i = 0; i < 256; i++ ) {
		CMR(ifp)[i] = cmp-> cm_red[i]>>8;
		CMG(ifp)[i] = cmp-> cm_green[i]>>8;
		CMB(ifp)[i] = cmp-> cm_blue[i]>>8;
	}
	SUN(ifp)->su_cmap_flag = !is_linear_cmap(ifp);
	sun_repaint( ifp );
	return	0;
}

HIDDEN int
sun_help( ifp )
FBIO	*ifp;
{
	struct	modeflags *mfp;

	fb_log( "Description: %s\n", sun_interface.if_type );
	fb_log( "Device: %s\n", ifp->if_name );
	fb_log( "Max width/height: %d %d\n",
		sun_interface.if_max_width,
		sun_interface.if_max_height );
	fb_log( "Default width/height: %d %d\n",
		sun_interface.if_width,
		sun_interface.if_height );
	fb_log( "Usage: /dev/sun[options]\n" );
	for( mfp = modeflags; mfp->c != '\0'; mfp++ ) {
		fb_log( "   %c   %s\n", mfp->c, mfp->help );
	}

	return(0);
}

static int window_not_destroyed = 1;

static Notify_value
my_real_destroy(frame, status)
Frame	frame;
Destroy_status	status;
{
	if( status != DESTROY_CHECKING ) {
		window_not_destroyed = 0;
	}
	/* Let frame get destroy event */
	return( notify_next_destroy_func(frame, status) );
}

/*
 *  Lingering Window.
 *  Wait around until a "quit" is selected.  Ideally this would
 *  happen in a child process with a fork done to free the original
 *  program and return control to the shell.  However, something
 *  in Suntools seems to stop notifying this window of repaints, etc.
 *  as soon as the PID changes.  Until we figure out how to get around
 *  this the parent will have to keep hanging on.
 */
HIDDEN int
linger( ifp )
FBIO	*ifp;
{
	struct timeval  timeout;

#ifdef DOFORK
	/* allow child to inherit input events */
	window_release_event_lock(SUN(ifp)->frame);
	window_release_event_lock(SUN(ifp)->canvas);
	pw_unlock(SUNPW(ifp));

	notify_dispatch();

	printf("forking\n");
	if( fork() )
		return 1;	/* release the parent */
#endif

	notify_interpose_destroy_func(SUN(ifp)->frame,my_real_destroy);

	timeout.tv_sec = 0;
	timeout.tv_usec = 250000;

	while( window_not_destroyed ) {
		(void) select(0, (fd_set *)0, (fd_set *)0, (fd_set *)0, &timeout);
		notify_dispatch();
	}
	return	0;
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
