/*
 *  How to add a new device interface:
 *
 *  Copy this file to if_devname.c
 *  Do a global replace of DEVNAME with your devname.
 *   (In the interest of non-flexnames, DEVNAME should be no more
 *   than three characters; except perhaps for DEVNAME_interface)
 *  Fill in the device description, max width and height,
 *   default width and height, and shortname (what you will
 *   look it up as).
 *  Set the unimplemented functions to "fb_null"
 *   (and remove the skeletons if you're tidy)
 *  Set DEVNAME_readrect to fb_sim_readrect, and DEVNAME_writerect
 *   to fb_sim_writerect, if not implemented.
 *  Make DEVNAME_free call DEVNAME_close if not implemented.
 *  Go add an "ifdef IF_DEVNAME" to fb_generic.c (two places).
 *  Add defines to ../Cakefile.defs.
 *  Replace this header.
 */

#include "fb.h"
#include "./fblocal.h"
#include <ctype.h>
#include <gdslib.h>

_LOCAL_ int	ts_open(),
		ts_close(),
		ts_reset(),
		ts_clear(),
		ts_read(),
		ts_write(),
		ts_rmap(),
		ts_wmap(),
		ts_viewport(),
		ts_window(),
		ts_zoom(),
		ts_setcursor(),
		ts_cursor(),
		ts_scursor(),
		ts_readrect(),
		ts_writerect(),
		ts_flush(),
		ts_free(),
		ts_help();

/* This is the ONLY thing that we normally "export" */
FBIO ts_interface =  {
	ts_open,		/* open device	*/
	ts_close,		/* close device	*/
	ts_reset,		/* reset device	*/
	ts_clear,		/* clear device	*/
	ts_read,		/* read	pixels	*/
	ts_write,		/* write pixels */
	ts_rmap,		/* rmap - read colormap	*/
	ts_wmap,		/* wmap - write colormap */
	ts_viewport,	/* viewport set	*/
	ts_window,		/* window set	*/
	ts_zoom,		/* zoom set	*/
	ts_setcursor,	/* setcursor - define cursor	*/
	ts_cursor,		/* cursor - memory address	*/
	ts_scursor,	/* scursor - screen address	*/
	ts_readrect,	/* readrect - read rectangle	*/
	ts_writerect,	/* writerect - write rectangle	*/
	ts_flush,		/* flush output	*/
	ts_free,		/* free resources */
	ts_help,		/* help message	*/
	"Tech-Source GDS 39xx",	/* device description	*/
	2048,			/* max width		*/
	1024,			/* max height		*/
	"/dev/ts",		/* short device name	*/
	1280,			/* default/current width  */
	1024,			/* default/current height */
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

static int unit = 4;

_LOCAL_ int
ts_open( ifp, file, width, height )
FBIO	*ifp;
char	*file;
int	width, height;
{
	struct point	viewmax;
	int mode;

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

	if (open_gds(unit,GDSDLSIZE) < 0) {
		fb_log("error %d\n", f_geterror());
		exit(1);
	}
	init_gds(0);

#if 0
	if ((mode&MODE_2MASK) == MODE_2OVERONLY) {
printf("Overlay Only\n");
		f_ovlset(GDS_OVL_DISPLAY, GDS_OVL_ONLY, 0);
	} else if ((mode&MODE_1MASK) == MODE_1OVERLAY) {
printf("Overlay On\n");
		f_ovlset(GDS_OVL_DISPLAY, GDS_OVL_ON, 0);
	} else {
printf("Overlay Off\n");
		f_ovlset(GDS_OVL_DISPLAY, GDS_OVL_OFF, 0);
	}
#endif

	getviewmax(&viewmax);
	ifp->if_width = viewmax.x;
	ifp->if_height = viewmax.y;

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
ts_reset( ifp )
FBIO	*ifp;
{
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

	return(0);
}

_LOCAL_ int
ts_read( ifp, x, y, pixelp, count )
FBIO	*ifp;
int	x, y;
RGBpixel	*pixelp;
int	count;
{
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
	f_pixar_ff(&dest,&size,3,lbuf);
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

_LOCAL_ int
ts_viewport( ifp, left, top, right, bottom )
FBIO	*ifp;
int	left, top, right, bottom;
{
	return(0);
}

_LOCAL_ int
ts_window( ifp, x, y )
FBIO	*ifp;
int	x, y;
{
	struct point panorigin;
	struct point rpanorigin;

	panorigin.x = x - ifp->if_width/2;
	panorigin.y = y - ifp->if_height/2;
	f_pan(panorigin.x, panorigin.y);
printf("pan %d %d -> %d %d\n\r", x, y, panorigin.x, panorigin.y);
	f_rdpan(&rpanorigin);
printf("hw pan %d %d\n\r", rpanorigin.x, rpanorigin.y);

	return(0);
}

_LOCAL_ int
ts_zoom( ifp, x, y )
FBIO	*ifp;
int	x, y;
{
	struct point factor;
	struct point rfactor;

	factor.x = x;
	factor.y = y;
	f_zoom(factor.x, factor.y);
printf("zoom %d %d\n\r", factor.x, factor.y);
	f_rdzoom(&rfactor);
printf("hw zoom %d %d\n\r", rfactor.x, rfactor.y);

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

	pos.x = x;
	pos.y = y;
	color.red = 255;
	color.green = 255;
	color.blue = 255;

	if (!mode) {
		f_hwcset(GDS_HWC_STYLE, GDS_HWCS_OFF, 0);
	} else {
		f_hwcset(GDS_HWC_STYLE, GDS_HWCS_CH1,
			GDS_HWC_COLOR, &color,
			GDS_HWC_POSITION, &pos, 0);
	}

	return(0);
}

_LOCAL_ int
ts_scursor( ifp, mode, x, y )
FBIO	*ifp;
int	mode;
int	x, y;
{
	struct point pos;
	struct color color;

	pos.x = x;
	pos.y = y;
	color.red = 255;
	color.green = 255;
	color.blue = 255;

	if (!mode) {
		f_hwcset(GDS_HWC_STYLE, GDS_HWCS_OFF, 0);
	} else {
		f_hwcset(GDS_HWC_STYLE, GDS_HWCS_CH1,
			GDS_HWC_COLOR, &color,
			GDS_HWC_POSITION, &pos, 0);
	}
	return(0);
}

_LOCAL_ int
ts_readrect( ifp, xmin, ymin, width, height, pp )
FBIO	*ifp;
int	xmin, ymin;
int	width, height;
RGBpixel	*pp;
{
	return( width*height );
}

_LOCAL_ int
ts_writerect( ifp, xmin, ymin, width, height, pp )
FBIO	*ifp;
int	xmin, ymin;
int	width, height;
RGBpixel	*pp;
{
	struct	point dest;
	struct	size size;

	dest.x = xmin;
	dest.y = ymin;
	size.width = width;
	size.height = height;
	f_pixar_ff(&dest,&size,2,pp);

	return( width*height );
}

_LOCAL_ int
ts_flush( ifp )
FBIO	*ifp;
{
	return(0);
}

_LOCAL_ int
ts_free( ifp )
FBIO	*ifp;
{
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
