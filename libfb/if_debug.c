/*
 *		I F _ D E B U G
 *
 *  Reports all calls to fb_log().
 */
#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "conf.h"

#include <stdio.h>
#include <ctype.h>

#include "machine.h"
#include "fb.h"
#include "./fblocal.h"

_LOCAL_ int	deb_open(),
		deb_close(),
		deb_clear(),
		deb_read(),
		deb_write(),
		deb_rmap(),
		deb_wmap(),
		deb_view(),
		deb_getview(),
		deb_setcursor(),
		deb_cursor(),
		deb_getcursor(),
		deb_readrect(),
		deb_writerect(),
		deb_bwreadrect(),
		deb_bwwriterect(),
		deb_poll(),
		deb_flush(),
		deb_free(),
		deb_help();

/* This is the ONLY thing that we "export" */
FBIO debug_interface = {
	0,
	deb_open,
	deb_close,
	deb_clear,
	deb_read,
	deb_write,
	deb_rmap,
	deb_wmap,
	deb_view,
	deb_getview,
	deb_setcursor,
	deb_cursor,
	deb_getcursor,
	deb_readrect,
	deb_writerect,
	deb_bwreadrect,
	deb_bwwriterect,
	deb_poll,
	deb_flush,
	deb_free,
	deb_help,
	"Debugging Interface",
	32*1024,		/* max width */
	32*1024,		/* max height */
	"/dev/debug",
	512,			/* current/default width */
	512,			/* current/default height */
	-1,			/* select fd */
	-1,			/* file descriptor */
	1, 1,			/* zoom */
	256, 256,		/* window center */
	0, 0, 0,		/* cursor */
	PIXEL_NULL,		/* page_base */
	PIXEL_NULL,		/* page_curp */
	PIXEL_NULL,		/* page_endp */
	-1,			/* page_no */
	0,			/* page_ref */
	0L,			/* page_curpos */
	0L,			/* page_pixels */
	0			/* debug */
};

_LOCAL_ int
deb_open( ifp, file, width, height )
FBIO	*ifp;
char	*file;
int	width, height;
{
	FB_CK_FBIO(ifp);
	if( file == (char *)NULL )
		fb_log( "fb_open( 0x%lx, NULL, %d, %d )\n",
			(unsigned long)ifp, width, height );
	else
		fb_log( "fb_open( 0x%lx, \"%s\", %d, %d )\n",
			(unsigned long)ifp, file, width, height );

	/* check for default size */
	if( width <= 0 )
		width = ifp->if_width;
	if( height <= 0 )
		height = ifp->if_height;

	/* set debug bit vector */
	if( file != NULL ) {
		char *cp;
		for( cp = file; *cp != '\0' && !isdigit(*cp); cp++ ) ;
		sscanf( cp, "%d", &ifp->if_debug );
	} else {
		ifp->if_debug = 0;
	}

	/* Give the user whatever width was asked for */
	ifp->if_width = width;
	ifp->if_height = height;

	return	0;
}

_LOCAL_ int
deb_close( ifp )
FBIO	*ifp;
{
	FB_CK_FBIO(ifp);
	fb_log( "fb_close( 0x%lx )\n", (unsigned long)ifp );
	return	0;
}

_LOCAL_ int
deb_clear( ifp, pp )
FBIO	*ifp;
RGBpixel	*pp;
{
	FB_CK_FBIO(ifp);
	if( pp == 0 )
		fb_log( "fb_clear( 0x%lx, NULL )\n", (unsigned long)ifp );
	else
		fb_log( "fb_clear( 0x%lx, &[%d %d %d] )\n",
			(unsigned long)ifp,
			(int)((*pp)[RED]), (int)((*pp)[GRN]),
			(int)((*pp)[BLU]) );
	return	0;
}

_LOCAL_ int
deb_read( ifp, x, y, pixelp, count )
FBIO	*ifp;
int	x, y;
RGBpixel	*pixelp;
int	count;
{
	FB_CK_FBIO(ifp);
	fb_log( "fb_read( 0x%lx,%4d,%4d, 0x%lx, %d )\n",
		(unsigned long)ifp, x, y,
		(unsigned long)pixelp, count );
	return	count;
}

_LOCAL_ int
deb_write( ifp, x, y, pixelp, count )
FBIO	*ifp;
int	x, y;
RGBpixel	*pixelp;
int	count;
{
	int	i;

	FB_CK_FBIO(ifp);
	fb_log( "fb_write( 0x%lx,%4d,%4d, 0x%lx, %d )\n",
		(unsigned long)ifp, x, y,
		(unsigned long)pixelp, count );

	/* write them out, four per line */
	if( ifp->if_debug & FB_DEBUG_RW ) {
		for( i = 0; i < count; i++ ) {
			if( i % 4 == 0 )
				fb_log( "%4d:", i );
			fb_log( "  [%3d,%3d,%3d]", pixelp[i][RED],
				pixelp[i][GRN], pixelp[i][BLU] );
			if( i % 4 == 3 )
				fb_log( "\n" );
		}
		if( i % 4 != 0 )
			fb_log( "\n" );
	}

	return	count;
}

_LOCAL_ int
deb_rmap( ifp, cmp )
FBIO	*ifp;
ColorMap	*cmp;
{
	FB_CK_FBIO(ifp);
	fb_log( "fb_rmap( 0x%lx, 0x%lx )\n",
		(unsigned long)ifp, (unsigned long)cmp );
	return	0;
}

_LOCAL_ int
deb_wmap( ifp, cmp )
FBIO	*ifp;
ColorMap	*cmp;
{
	int	i;

	FB_CK_FBIO(ifp);
	if( cmp == NULL )
		fb_log( "fb_wmap( 0x%lx, NULL )\n",
			(unsigned long)ifp );
	else
		fb_log( "fb_wmap( 0x%lx, 0x%lx )\n",
			(unsigned long)ifp, (unsigned long)cmp );

	if( ifp->if_debug & FB_DEBUG_CMAP && cmp != NULL ) {
		for( i = 0; i < 256; i++ ) {
			fb_log( "%3d: [ 0x%4lx, 0x%4lx, 0x%4lx ]\n",
				i,
				(unsigned long)cmp->cm_red[i],
				(unsigned long)cmp->cm_green[i],
				(unsigned long)cmp->cm_blue[i] );
		}
	}

	return	0;
}

_LOCAL_ int
deb_view( ifp, xcenter, ycenter, xzoom, yzoom )
FBIO	*ifp;
int	xcenter, ycenter;
int	xzoom, yzoom;
{
	FB_CK_FBIO(ifp);
	fb_log( "fb_view( 0x%lx,%4d,%4d,%4d,%4d )\n",
		(unsigned long)ifp, xcenter, ycenter, xzoom, yzoom );
	fb_sim_view( ifp, xcenter, ycenter, xzoom, yzoom );
	return	0;
}

_LOCAL_ int
deb_getview( ifp, xcenter, ycenter, xzoom, yzoom )
FBIO	*ifp;
int	*xcenter, *ycenter;
int	*xzoom, *yzoom;
{
	FB_CK_FBIO(ifp);
	fb_log( "fb_getview( 0x%lx, 0x%x, 0x%x, 0x%x, 0x%x )\n",
		(unsigned long)ifp, xcenter, ycenter, xzoom, yzoom );
	fb_sim_getview( ifp, xcenter, ycenter, xzoom, yzoom );
	fb_log( " <= %d %d %d %d\n",
		*xcenter, *ycenter, *xzoom, *yzoom );
	return	0;
}

_LOCAL_ int
deb_setcursor( ifp, bits, xbits, ybits, xorig, yorig )
FBIO	*ifp;
unsigned char *bits;
int	xbits, ybits;
int	xorig, yorig;
{
	FB_CK_FBIO(ifp);
	fb_log( "fb_setcursor( 0x%lx, 0x%lx, %d, %d, %d, %d )\n",
		(unsigned long)ifp, bits, xbits, ybits, xorig, yorig );
	return	0;
}

_LOCAL_ int
deb_cursor( ifp, mode, x, y )
FBIO	*ifp;
int	mode;
int	x, y;
{
	fb_log( "fb_cursor( 0x%lx, %d,%4d,%4d )\n",
		(unsigned long)ifp, mode, x, y );
	fb_sim_cursor( ifp, mode, x, y );
	return	0;
}

_LOCAL_ int
deb_getcursor( ifp, mode, x, y )
FBIO	*ifp;
int	*mode;
int	*x, *y;
{
	FB_CK_FBIO(ifp);
	fb_log( "fb_getcursor( 0x%lx, 0x%x,0x%x,0x%x )\n",
		(unsigned long)ifp, mode, x, y );
	fb_sim_getcursor( ifp, mode, x, y );
	fb_log( " <= %d %d %d\n", *mode, *x, *y );
	return	0;
}

_LOCAL_ int
deb_readrect( ifp, xmin, ymin, width, height, pp )
FBIO	*ifp;
int	xmin, ymin;
int	width, height;
RGBpixel	*pp;
{
	FB_CK_FBIO(ifp);
	fb_log( "fb_readrect( 0x%lx, (%4d,%4d), %4d,%4d, 0x%lx )\n",
		(unsigned long)ifp, xmin, ymin, width, height,
		(unsigned long)pp );
	return( width*height );
}

_LOCAL_ int
deb_writerect( ifp, xmin, ymin, width, height, pp )
FBIO	*ifp;
int	xmin, ymin;
int	width, height;
RGBpixel	*pp;
{
	FB_CK_FBIO(ifp);
	fb_log( "fb_writerect( 0x%lx,%4d,%4d,%4d,%4d, 0x%lx )\n",
		(unsigned long)ifp, xmin, ymin, width, height,
		(unsigned long)pp );
	return( width*height );
}

_LOCAL_ int
deb_bwreadrect( ifp, xmin, ymin, width, height, pp )
FBIO	*ifp;
int	xmin, ymin;
int	width, height;
RGBpixel	*pp;
{
	FB_CK_FBIO(ifp);
	fb_log( "fb_bwreadrect( 0x%lx, (%4d,%4d), %4d,%4d, 0x%lx )\n",
		(unsigned long)ifp, xmin, ymin, width, height,
		(unsigned long)pp );
	return( width*height );
}

_LOCAL_ int
deb_bwwriterect( ifp, xmin, ymin, width, height, pp )
FBIO	*ifp;
int	xmin, ymin;
int	width, height;
RGBpixel	*pp;
{
	FB_CK_FBIO(ifp);
	fb_log( "fb_bwwriterect( 0x%lx,%4d,%4d,%4d,%4d, 0x%lx )\n",
		(unsigned long)ifp, xmin, ymin, width, height,
		(unsigned long)pp );
	return( width*height );
}

_LOCAL_ int
deb_poll( ifp )
FBIO	*ifp;
{
	FB_CK_FBIO(ifp);
	fb_log( "fb_poll( 0x%lx )\n", (unsigned long)ifp );
	return	0;
}

_LOCAL_ int
deb_flush( ifp )
FBIO	*ifp;
{
	FB_CK_FBIO(ifp);
	fb_log( "if_flush( 0x%lx )\n", (unsigned long)ifp );
	return	0;
}

_LOCAL_ int
deb_free( ifp )
FBIO	*ifp;
{
	FB_CK_FBIO(ifp);
	fb_log( "fb_free( 0x%lx )\n", (unsigned long)ifp );
	return	0;
}

/*ARGSUSED*/
_LOCAL_ int
deb_help( ifp )
FBIO	*ifp;
{
	FB_CK_FBIO(ifp);
	fb_log( "Description: %s\n", debug_interface.if_type );
	fb_log( "Device: %s\n", ifp->if_name );
	fb_log( "Max width/height: %d %d\n",
		debug_interface.if_max_width,
		debug_interface.if_max_height );
	fb_log( "Default width/height: %d %d\n",
		debug_interface.if_width,
		debug_interface.if_height );
	fb_log( "\
Usage: /dev/debug[#]\n\
  where # is a optional bit vector from:\n\
    1    debug buffered I/O calls\n\
    2    show colormap entries in rmap/wmap calls\n\
    4    show actual pixel values in read/write calls\n" );
	/*8    buffered read/write values - ifdef'd out*/

	return	0;
}
