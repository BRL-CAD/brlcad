/*                         I F _ T S . C
 * BRL-CAD
 *
 * Copyright (c) 1990-2006 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */

/** \addtogroup if */
/*@{*/
/** @file if_ts.c
 *  Tech-Source GDS-3924L+8 Interface.  This is a single VME board
 *  frame buffer, most likely plugged into a Sun Microsystems computer.
 *  Features include:  1280x1024x24bit display in a 2048x1024 window, or
 *  double buffered 1024x1024x24bit.  Hardware pan and zoom, colormaps,
 *  eight bits of overlay planes.
 *
 *  To use this module you must have the GDSLIB library and header
 *  files installed, and add -lgds to the LIBFB_LIBS in Cakefile.defs.
 *
 *  Authors -
 *	Phillip Dykstra
 *
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *
 */
/*@}*/

#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "common.h"



#include <stdio.h>

#include "machine.h"
#include "fb.h"
#include "./fblocal.h"
#include <ctype.h>
#include <gdslib.h>

_LOCAL_ int	ts_open(),
		ts_close(),
		ts_clear(),
		ts_read(),
		ts_write(),
		ts_rmap(),
		ts_wmap(),
		ts_view(),
		ts_getview(),
		ts_window(),		/* OLD */
		ts_zoom(),		/* OLD */
		ts_setcursor(),
		ts_cursor(),
		ts_getcursor(),
		ts_readrect(),
		ts_writerect(),
		ts_flush(),
		ts_free(),
		ts_help();

/* This is the ONLY thing that we normally "export" */
FBIO ts_interface =  {
	0,
	ts_open,		/* open device		*/
	ts_close,		/* close device		*/
	ts_clear,		/* clear device		*/
	ts_read,		/* read	pixels		*/
	ts_write,		/* write pixels		*/
	ts_rmap,		/* read colormap	*/
	ts_wmap,		/* write colormap	*/
	ts_view,		/* set view		*/
	ts_getview,		/* get view		*/
	ts_setcursor,		/* define cursor	*/
	ts_cursor,		/* set cursor		*/
	ts_getcursor,		/* get cursor		*/
	ts_readrect,		/* read rectangle	*/
	ts_writerect,		/* write rectangle	*/
	fb_sim_bwreadrect,
	fb_sim_bwwriterect,
	fb_null,		/* handle events	*/
	ts_flush,		/* flush output		*/
	ts_free,		/* free resources	*/
	ts_help,		/* help message		*/
	"Tech-Source GDS 39XX",	/* device description	*/
	2048,			/* max width		*/
	1024,			/* max height		*/
	"/dev/ts",		/* short device name	*/
	1280,			/* default/current width  */
	1024,			/* default/current height */
	-1,			/* select file desc	*/
	-1,			/* file descriptor	*/
	1, 1,			/* zoom			*/
	640, 512,		/* window		*/
	0, 0, 0,		/* cursor		*/
	PIXEL_NULL,		/* page_base		*/
	PIXEL_NULL,		/* page_curp		*/
	PIXEL_NULL,		/* page_endp		*/
	-1,			/* page_no		*/
	0,			/* page_dirty		*/
	0L,			/* page_curpos		*/
	0L,			/* page_pixels		*/
	0			/* debug		*/
};

#define MODE_1MASK	(1<<1)
#define MODE_1NORM	(0<<1)
#define MODE_1OVERLAY	(1<<1)

#define MODE_2MASK	(1<<2)
#define MODE_2NORM	(0<<2)
#define MODE_2OVERONLY	(1<<2)

static struct modeflags {
	char	c;
	long	mask;
	long	value;
	char	*help;
} modeflags[] = {
	{ 'o',	MODE_1MASK, MODE_1OVERLAY,
		"Enable Transparent Overlay Planes - else off" },
	{ 'O',	MODE_2MASK, MODE_2OVERONLY,
		"Enable ONLY the Overlay Planes" },
	{ '\0', 0, 0, "" }
};

/*
 *  GSD Unit Numbers (bottom 3 bits is the "Monitor"):
 *	0 for Red channel	(GMA0)
 *	1 for Green channel	(GMA1)
 *	2 for Blue channel	(GMA2)
 *	3 for Overlay channel	(GMA3)
 *	4 for RGB channels	(24bit GMA0-GMA2)
 *	5 for 8bit into RGB channels (8bit GMA0-GMA2)
 *
 *  The hardware maintains 32 Graphics Units which hold a set of
 *  "Local Attributes" (drawing colors, wite mask, clip rectangle, etc.)
 *  The GDSLIB also associates a display list buffer with each open unit.
 *  The bottom 3 bits of the Unit Number (0-7) become the "Monitor"
 *  which is one of eight mappings of GMA's (Graphics Memory Arrays)
 *  maintained by the GDP (Graphics Drawing Processor).
 *  Global_Attributes[Unit] -> Local_Attributes[Monitor] -> Monitor_Def
 */
static int unit = 4;

_LOCAL_ int
ts_open( ifp, file, width, height )
FBIO	*ifp;
char	*file;
int	width, height;
{
	struct point	viewmax;
	int mode;
	char curs_bitmap[512];
	int i;

	FB_CK_FBIO(ifp);

	/*
	 *  First, attempt to determine operating mode for this open,
	 *  based upon the "unit number" or flags.
	 *  file = "/dev/X###"
	 *  The default mode is zero.
	 */
	mode = 0;
	if( file != NULL )  {
		register char *cp;
		char	modebuf[80];
		char	*mp;
		int	alpha;
		struct	modeflags *mfp;

		if( strncmp(file, "/dev/ts", 7) ) {
			/* How did this happen?? */
			mode = 0;
		}
		else {
			/* Parse the options */
			alpha = 0;
			mp = &modebuf[0];
			cp = &file[7];
			while( *cp != '\0' && !isspace(*cp) ) {
				*mp++ = *cp;	/* copy it to buffer */
				if( isdigit(*cp) ) {
					cp++;
					continue;
				}
				alpha++;
				for( mfp = modeflags; mfp->c != '\0'; mfp++ ) {
					if( mfp->c == *cp ) {
						mode = (mode&~mfp->mask)|mfp->value;
						break;
					}
				}
				if( mfp->c == '\0' && *cp != '-' ) {
					fb_log( "if_ts: unknown option '%c' ignored\n", *cp );
				}
				cp++;
			}
			*mp = '\0';
			if( !alpha )
				mode = atoi( modebuf );
			if (strlen(cp) > 0) {
				unit = atoi(cp);
				printf("Unit %d\n", unit);
			}
		}
	}

	/*printf("GSD unit = %d\n", unit);*/
	/* GDSLSIZE 16bit words in display list */
	if (open_gds(unit,GDSDLSIZE) < 0) {
		fb_log("error %d\n", f_geterror());
		exit(1);
	}
	init_gds(0);	/* if non-zero, will clear to altcolor */

#if 0
	if ((mode&MODE_2MASK) == MODE_2OVERONLY) {
printf("Overlay Only\n");
		f_ovlset(GDS_OVL_DISPLAY, GDS_OVL_ONLY, 0);
	} else if ((mode&MODE_1MASK) == MODE_1OVERLAY) {
printf("Overlay On\n");
		f_ovlset(GDS_OVL_DISPLAY, GDS_OVL_ON, 0);
	} else {
/* Seems that even doing this overlay off command leaves us with
 * only the RED channel.
 */
printf("Overlay Off\n");
		f_ovlset(GDS_OVL_DISPLAY, GDS_OVL_OFF, 0);
	}
#endif

	getviewmax(&viewmax);
	ifp->if_width = viewmax.x + 1;
	ifp->if_height = viewmax.y + 1;

	/* Initialize the hardware cursor bitmap */
	for (i = 0; i < 512; i++) {
		/* start with black */
		curs_bitmap[i] = 0;
	}
	for (i = 0; i < 8; i++) {
		/* fill in top and bottom rows */
		curs_bitmap[i] = 255;
		curs_bitmap[i+504] = 255;
	}
	for (i = 0; i < 64; i++) {
		/* then three columns */
		curs_bitmap[i*8] |= 0x80;
		curs_bitmap[i*8+7] |= 0x01;
		curs_bitmap[i*8+4] |= 0x80;
	}
	f_hwcset(GDS_HWC_BITMAP, curs_bitmap, 0);

	return(0);
}

_LOCAL_ int
ts_close( ifp )
FBIO	*ifp;
{
	close_gds(unit);
	return(0);
}

_LOCAL_ int
ts_clear( ifp, pp )
FBIO	*ifp;
RGBpixel	*pp;
{
	struct	rectangle rect;

	if (pp != RGBPIXEL_NULL) {
		setaltcolor(COLR24((*pp)[RED],(*pp)[GRN],(*pp)[BLU]));
	} else {
		setaltcolor(COLR24(0,0,0));
	}

	rect.x0 = 0;
	rect.y0 = 0;
	rect.x1 = 1279;
	rect.y1 = 1023;
	if (f_fillrect(&rect)) {
		fb_log("error 3 %d\n", f_geterror());
		exit(1);
	}
	/* XXX - Note that this may not flush the display list */

	return(0);
}

_LOCAL_ int
ts_read( ifp, x, y, pixelp, count )
FBIO	*ifp;
int	x, y;
RGBpixel	*pixelp;
int	count;
{
	struct	point dest;
	struct	size size;

	dest.x = x;
	dest.y = y;
	size.width = count;
	size.height = 1;	/*XXX*/

	f_rdpixar_f(&dest,&size,2,pixelp);
	return(count);
}

_LOCAL_ int
ts_write( ifp, x, y, pixelp, count )
FBIO	*ifp;
int	x, y;
RGBpixel	*pixelp;
int	count;
{
	struct	point dest;
	struct	size size;
	int	i;
	unsigned char lbuf[1280][4];

#if 1
	for (i = 0; i < count; i++) {
		/*lbuf[i][0] = 0;*/
		lbuf[i][1] = pixelp[i][BLU];
		lbuf[i][2] = pixelp[i][GRN];
		lbuf[i][3] = pixelp[i][RED];
	}
#endif

	dest.x = x;
	dest.y = y;
	size.width = count;
	size.height = 1;	/*XXX*/
#if 0
	f_pixar_ff(&dest,&size,2,pixelp);
#else
#	if 0
	/* This way dumps core if writes are longer than 1020 bytes -M */
	f_pixar_ff(&dest,&size,3,lbuf);
#	else
	if( count > 800 )  {
		size.width = 800;
		f_pixar_ff(&dest,&size,3,lbuf);
		size.width = count-800;
		dest.x = 800;
		f_pixar_ff(&dest,&size,3,&lbuf[800][0]);
	} else {
		f_pixar_ff(&dest,&size,3,lbuf);
	}
#	endif
#endif

	return(count);
}

_LOCAL_ int
ts_rmap( ifp, cmp )
FBIO	*ifp;
ColorMap	*cmp;
{
	int	i;
	struct color cmap[256];

	if (f_rdclut(0,256,cmap)) {
		fb_log("error 2\n");
		exit(1);
	}
	for (i = 0; i < 256; i++) {
		cmp->cm_red[i] = cmap[i].red << 8;
		cmp->cm_green[i] = cmap[i].green << 8;
		cmp->cm_blue[i] = cmap[i].blue << 8;
	}

	return(0);
}

_LOCAL_ int
ts_wmap( ifp, cmp )
FBIO	*ifp;
ColorMap	*cmp;
{
	int	i;
	struct color cmap[256];

	if (cmp != COLORMAP_NULL) {
		for (i = 0; i < 256; i++) {
			cmap[i].red = cmp->cm_red[i]>>8;
			cmap[i].green = cmp->cm_green[i]>>8;
			cmap[i].blue = cmp->cm_blue[i]>>8;
		}
	} else {
		for (i = 0; i < 256; i++)
			cmap[i].red = cmap[i].green = cmap[i].blue = i;
	}
	if (f_wrclut(0,256,cmap)) {
		fb_log("error 2\n");
		exit(1);
	}

	return(0);
}


/*
 * Note: at first I didn't have any display list flushes in here and it
 * worked fine.  Then it started messing up so I added one at the end
 * (having learned from the cursor routine).  But, that would mess up.
 * The two flush version is working again.  Strange....
 */
_LOCAL_ int
ts_view( ifp, xcenter, ycenter, xzoom, yzoom )
FBIO	*ifp;
int	xcenter, ycenter;
int	xzoom, yzoom;
{
	struct point panorigin;
	struct point factor;

	if (xzoom <= 0) xzoom = 1;
	if (yzoom <= 0) yzoom = 1;

	/* save a working copy for outselves */
	ifp->if_xcenter = xcenter;
	ifp->if_ycenter = xcenter;
	ifp->if_xzoom = xzoom;
	ifp->if_yzoom = yzoom;

	panorigin.x = xcenter - ifp->if_width/(2*xzoom);
	panorigin.y = ycenter - ifp->if_height/(2*yzoom);
	f_pan(panorigin.x, panorigin.y);
	f_exec_wr_dl(0);	/* flush display list, no wait */

	factor.x = xzoom;
	factor.y = yzoom;
	f_zoom(factor.x, factor.y);
	f_exec_wr_dl(0);	/* flush display list, no wait */

	return(0);
}

/* return base^pow */
_LOCAL_ int
ipow(base,pow)
int base;
int pow;
{
	int	i, n;

	if (pow <= 0)
		return	1;

	n = base;
	for (i = 1; i < pow; i++)
		n *= base;

	return	n;
}

/*
 *  Note: A bug in the GDSLIB causes the yzoom factor to be read
 *  back as a power of two, rather than as a pixel replication count.
 */
_LOCAL_ int
ts_getview( ifp, xcenter, ycenter, xzoom, yzoom )
FBIO	*ifp;
int	*xcenter, *ycenter;
int	*xzoom, *yzoom;
{
	struct point rpanorigin;
	struct point rfactor;

	f_rdzoom(&rfactor);
	ifp->if_xzoom = rfactor.x;
	ifp->if_yzoom = ipow(2,rfactor.y);	/* Bug fix - see above */
	*xzoom = ifp->if_xzoom;
	*yzoom = ifp->if_yzoom;

	/* read lower left pixel coordinate */
	f_rdpan(&rpanorigin);
	/* convert to center pixel coordinate */
	*xcenter = rpanorigin.x + ifp->if_width/(2*ifp->if_xzoom);
	*ycenter = rpanorigin.y + ifp->if_height/(2*ifp->if_yzoom);

#if 0
	printf("getview: hw pan %d %d\n\r", rpanorigin.x, rpanorigin.y);
	printf("getview: hw zoom %d %d\n\r", rfactor.x, rfactor.y);
	printf("getview: returning %d %d %d %d\n", *xcenter, *ycenter, *xzoom, *yzoom);
#endif

	return(0);
}

_LOCAL_ int
ts_setcursor( ifp, bits, xbits, ybits, xorig, yorig )
FBIO	*ifp;
unsigned char *bits;
int	xbits, ybits;
int	xorig, yorig;
{
	return(0);
}

_LOCAL_ int
ts_cursor( ifp, mode, x, y )
FBIO	*ifp;
int	mode;
int	x, y;
{
	struct point pos;
	struct color color;
	struct rectangle rect;

	pos.x = x;
	pos.y = y;
	color.red = 255;
	color.green = 255;
	color.blue = 255;
	rect.x0 = 0;
	rect.x1 = 1279;

	/* XXX - BUG in GDSLIB, has these two backward! */
	rect.y1 = 0;
	rect.y0 = -1023;	/* Gross... */

	if (!mode) {
		f_hwcset(GDS_HWC_STYLE, GDS_HWCS_OFF, 0);
	} else {
		f_hwcset(GDS_HWC_STYLE, GDS_HWCS_CH1|GDS_HWCS_BITMAP,
			GDS_HWC_ASSIGN, 4,
			GDS_HWC_COLOR, &color,
			GDS_HWC_WINDOW, &rect,
			GDS_HWC_POSITION, &pos, 0);
	}
	f_exec_wr_dl(0);	/* flush display list, no wait */

#if 0
{
	int	assign;
	struct point *pp;
	assign = f_hwcget(GDS_HWC_ASSIGN);
	pp = (struct point *)f_hwcget(GDS_HWC_POSITION);
	printf("cursor: %d %d %d\n", assign, pp->x, pp->y);
}
#endif

	return(0);
}

_LOCAL_ int
ts_getcursor( ifp, mode, x, y )
FBIO	*ifp;
int	*mode;
int	*x, *y;
{
	/*int	style;*/
	int	assign;
	struct point *pp;

	/*style = f_hwcget(GDS_HWC_ASSIGN);*/
	assign = f_hwcget(GDS_HWC_ASSIGN);
	pp = (struct point *)f_hwcget(GDS_HWC_POSITION);

	if (assign)
		*mode = 1;
	else
		*mode = 0;
	*x = pp->x;
	*y = pp->y;
	printf("getcursor: %d %d %d\n", *mode, *x, *y);
}

_LOCAL_ int
ts_readrect( ifp, xmin, ymin, width, height, pp )
FBIO	*ifp;
int	xmin, ymin;
int	width, height;
RGBpixel	*pp;
{
#if 0
	struct	point dest;
	struct	size size;

	dest.x = xmin;
	dest.y = ymin;
	size.width = width;
	size.height = height;
	f_rdpixar_f(&dest,&size,2,pp);

	return( width*height );
#else
	/* until we get 24bit reads that work... sigh */
	return fb_sim_readrect(ifp, xmin, ymin, width, height, pp);
#endif
}

_LOCAL_ int
ts_writerect( ifp, xmin, ymin, width, height, pp )
FBIO	*ifp;
int	xmin, ymin;
int	width, height;
RGBpixel	*pp;
{
#if 0
	struct	point dest;
	struct	size size;

	dest.x = xmin;
	dest.y = ymin;
	size.width = width;
	size.height = height;
	f_pixar_ff(&dest,&size,2,pp);

	return( width*height );
#else
	/* until we get 24bit writes that work... sigh */
	return fb_sim_writerect(ifp, xmin, ymin, width, height, pp);
#endif
}

_LOCAL_ int
ts_flush( ifp )
FBIO	*ifp;
{
	f_exec_wr_dl(0);	/* flush display list, no wait */
	return(0);
}

_LOCAL_ int
ts_free( ifp )
FBIO	*ifp;
{
	/* XXX - should reset everything to sane mode */
	return(0);
}

_LOCAL_ int
ts_help( ifp )
FBIO	*ifp;
{
	struct	modeflags *mfp;

	fb_log( "Description: %s\n", ts_interface.if_type );
	fb_log( "Device: %s\n", ifp->if_name );
	fb_log( "Max width/height: %d %d\n",
		ts_interface.if_max_width,
		ts_interface.if_max_height );
	fb_log( "Default width/height: %d %d\n",
		ts_interface.if_width,
		ts_interface.if_height );
	fb_log( "Usage: /dev/ts[options] [channel]\n" );
	for( mfp = modeflags; mfp->c != '\0'; mfp++ ) {
		fb_log( "   %c   %s\n", mfp->c, mfp->help );
	}
	fb_log( "Channel Numbers:\n");
	fb_log( " 0 for Red channel\n");
	fb_log( " 1 for Green channel\n");
	fb_log( " 2 for Blue channel\n");
	fb_log( " 3 for Overlay channel\n");
	fb_log( " 4 for RGB channels\n");
	return(0);
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
