/*
 *			I F _ S G I . C
 *
 *  Authors -
 *	Paul R. Stay
 *	Michael John Muuss
 *
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1986 by the United States Army.
 *	All rights reserved.
 *
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include <stdio.h>
#include <gl.h>
#undef RED

#include "fb.h"
#include "./fblocal.h"


/*
 *  Defines for dealing with SGI Graphics Engine Pipeline
 */
union gepipe {
	short	s;
	long	l;
	float	f;
};

/**#define MC_68010 xx	/* not a turbo */
#ifdef MC_68010
#define GEPIPE	((union gepipe *)0X00FD5000)
#else
#define GEPIPE	((union gepipe *)0X60001000)
#endif
#define GEP_END(p)	((union gepipe *)(((char *)p)-0x1000))	/* 68000 efficient 0xFd4000 */


_LOCAL_ int	sgi_dopen(),
		sgi_dclose(),
		sgi_dreset(),
		sgi_dclear(),
		sgi_bread(),
		sgi_bwrite(),
		sgi_cmread(),
		sgi_cmwrite(),
		sgi_viewport_set(),
		sgi_window_set(),
		sgi_zoom_set(),
		sgi_cinit_bitmap(),
		sgi_cmemory_addr(),
		sgi_cscreen_addr();

/* This is the ONLY thing that we "export" */
FBIO sgi_interface =
		{
		sgi_dopen,
		sgi_dclose,
		sgi_dreset,
		sgi_dclear,
		sgi_bread,
		sgi_bwrite,
		fb_null,
		fb_null,
		sgi_viewport_set,
		sgi_window_set,
		sgi_zoom_set,
		fb_null,
		fb_null,
		fb_null,
		"Silicon Graphics IRIS",
		1024,			/* max width */
		768,			/* max height */
		"/dev/sgi",
		512,			/* current/default width  */
		512,			/* current/default height */
		-1,			/* file descriptor */
		PIXEL_NULL,		/* page_base */
		PIXEL_NULL,		/* page_curp */
		PIXEL_NULL,		/* page_endp */
		-1,			/* page_no */
		0,			/* page_ref */
		0L,			/* page_curpos */
		0L,			/* page_pixels */
		0			/* debug */
		};


_LOCAL_ int _sgi_cmap_flag;

_LOCAL_ ColorMap _sgi_cmap;

_LOCAL_ int
sgi_dopen( ifp, file, width, height )
FBIO	*ifp;
char	*file;
int	width, height;
{
	short x_pos, y_pos;	/* Lower corner of viewport */
	register int i;
	
	noport();
	gbegin();		/* not ginit() */
	if( ismex() )  {
		fb_log("libfb/if_sgi can't run under MEX\n");
		(void)sgi_dclose(ifp);
		return(-1);
	}
	RGBmode();
	gconfig();
	if( getplanes() < 24 )  {
		fb_log("Only %d bit planes?  24 required\n", getplanes());
		(void)sgi_dclose(ifp);
		return(-1);
	}
	tpoff();		/* Turn off textport */

	blanktime( 67 * 60 * 60L );	/* 1 hour blanking when fb open */

	/* Build a linear "colormap" in case he wants to read it */
	sgi_cmwrite( ifp, COLORMAP_NULL );

	if ( width > ifp->if_max_width) 
		width = ifp->if_max_width;

	if ( height > ifp->if_max_height) 
		height = ifp->if_max_height;

	ifp->if_width = width;
	ifp->if_height = height;

#define if_pixsize	u1.l
	ifp->if_pixsize = 1;	/* for zoom fakeout */
	return(0);
}

/*
 *			S G I _ D C L O S E
 *
 *  Finishing with a gexit() is mandatory.
 *  If we do a tpon() greset(), the text port pops up right
 *  away, spoiling the image.  Just doing the greset() causes
 *  the region of the text port to be disturbed, although the
 *  text port does not become readable.
 *
 *  Unfortunately, this means that the user has to run the
 *  "gclear" program before the screen can be used again,
 *  which is certainly a nuisance.  On the other hand, this
 *  means that images can be created AND READ BACK from
 *  separate programs, just like we do on the real framebuffers.
 */
_LOCAL_ int
sgi_dclose( ifp )
FBIO	*ifp;
{
	blanktime( 67 * 60 * 20L );	/* 20 minute blanking when fb closed */
#ifdef Ruins_Images
	tpon();			/* Turn on textport */
	greset();
	/* could be ginit(), gconfig() */
#endif
	gexit();
	return(0);
}

_LOCAL_ int
sgi_dreset( ifp )
FBIO	*ifp;
{
	short x_pos, y_pos;	/* Lower corner of viewport */

	
	ginit();
	RGBmode();
	gconfig();

	RGBcolor( (short) 0, (short) 0, (short) 0);
	clear();
	return(0);
}

_LOCAL_ int
sgi_dclear( ifp, pp )
FBIO	*ifp;
RGBpixel	*pp;
{
	if ( pp != NULL)
		RGBcolor((short)((*pp)[RED]), (short)((*pp)[GRN]), (short)((*pp)[BLU]));
	else
		RGBcolor( (short) 0, (short) 0, (short) 0);
	clear();
	return(0);
}

_LOCAL_ int
sgi_window_set( ifp, x, y )
FBIO	*ifp;
int	x, y;
{
	return(0);	/* Unable to do much */
}

_LOCAL_ int
sgi_zoom_set( ifp, x, y )
FBIO	*ifp;
int	x, y;
{
	int npts;
	npts = ifp->if_width;
	if( npts > 768 )  npts = 768;
	if( x < 1 )  x = 1;
	npts = npts / x;
	ifp->if_pixsize = 768/npts;
}

_LOCAL_ int
sgi_bread( ifp, x, y, pixelp, count )
FBIO	*ifp;
int	x, y;
register RGBpixel	*pixelp;
int	count;
{
	short scan_count;
	short xpos, ypos;
	RGBvalue rr[1024], gg[1024], bb[1024];
	register int i;
	int ret;

	ret = count;	/* save count */
	xpos = x;
	ypos = y;

	while( count > 0 )  {
		if ( count >= ifp->if_width )  {
			scan_count = ifp->if_width;
		} else	{
			scan_count = count;
		}

		cmov2s( xpos, ypos );		/* move to current position */
		readRGB( scan_count, rr, gg, bb );

		for( i = 0; i < scan_count; i++, pixelp++)  {
			if ( _sgi_cmap_flag == FALSE )  {
				(*pixelp)[RED] = rr[i];
				(*pixelp)[GRN] = gg[i];
				(*pixelp)[BLU] = bb[i];
			} else {
				(*pixelp)[RED] = _sgi_cmap.cm_red[ rr[i] ];
				(*pixelp)[GRN] = _sgi_cmap.cm_green[ gg[i] ];
				(*pixelp)[BLU] = _sgi_cmap.cm_blue[ bb[i] ];
			}
		}

		count -= scan_count;
		xpos = 0;
		ypos++;		/* Advance upwards */
	}
	return(ret);
}

_LOCAL_ int
sgi_bwrite( ifp, x, y, pixelp, count )
FBIO	*ifp;
short	x, y;
register RGBpixel	*pixelp;
short	count;
{
	register union gepipe *hole = GEPIPE;
	register short scan_count;
	short xpos, ypos;
	register short i;
	int ret;

	ret = count;	/* save count */
	xpos = x;
	ypos = y;
	while( count > 0 )  {
		if( ypos >= 768 )  return(0);
		if ( count >= ifp->if_width )  {
			scan_count = ifp->if_width;
		} else	{
			scan_count = count;
		}

		hole->l = 0x0008001A;	/* passthru, */
		hole->s = 0x0912;		/* cmov2s */
		hole->s = xpos;
		hole->s = ypos;
		GEP_END(hole)->s = (0xFF<<8)|8;	/* im_last_passthru(0) */

		for( i=scan_count; i > 0; )  {
			register short chunk;

			if( ifp->if_pixsize > 1 )  {
				Coord l, b;
				RGBcolor( (short)((*pixelp)[RED]),
					(short)((*pixelp)[GRN]),
					(short)((*pixelp)[BLU]) );
				l = xpos * ifp->if_pixsize;
				b = ypos * ifp->if_pixsize;
				/* left bottom right top */
				rectf( l, b,
					l+ifp->if_pixsize, b+ifp->if_pixsize);
				i--;
				xpos++;
				pixelp++;
				continue;
			}
			if( i <= (127/3) )
				chunk = i;
			else
				chunk = 127/3;
			hole->s = ((chunk*3)<<8)|8;	/* GEpassthru */
			hole->s = 0xD;		/* FBCdrawpixels */
			i -= chunk;
			if ( _sgi_cmap_flag == FALSE )  {
				for( ; chunk>0; chunk--,pixelp++ )  {
					hole->s = (*pixelp)[RED];
					hole->s = (*pixelp)[GRN];
					hole->s = (*pixelp)[BLU];
				}
			} else {
				for( ; chunk>0; chunk--,pixelp++ )  {
					hole->s = _sgi_cmap.cm_red[(*pixelp)[RED]];
					hole->s = _sgi_cmap.cm_green[(*pixelp)[GRN]];
					hole->s = _sgi_cmap.cm_blue[(*pixelp)[BLU]];
				}
			}
		}
		GEP_END(hole)->s = (0xFF<<8)|8;	/* im_last_passthru(0) */

		count -= scan_count;
		xpos = 0;
		ypos++;		/* 1st quadrant */
	}
	return(ret);
}

_LOCAL_ int
sgi_viewport_set( ifp, left, top, right, bottom )
FBIO	*ifp;
int	left, top, right, bottom;
{
	viewport( left, right, top, bottom );
	return(0);
}

_LOCAL_ int
sgi_cmread( ifp, cmp )
FBIO	*ifp;
register ColorMap	*cmp;
{
	register int i;

	/* Just parrot back the stored colormap */
	for( i = 0; i < 255; i++)
	{
		cmp->cm_red[i] = _sgi_cmap.cm_red[i]<<8;
		cmp->cm_green[i] = _sgi_cmap.cm_green[i]<<8;
		cmp->cm_blue[i] = _sgi_cmap.cm_blue[i]<<8;
	}
	return(0);
}

_LOCAL_ int
sgi_cmwrite( ifp, cmp )
FBIO	*ifp;
register ColorMap	*cmp;
{
	register int i;

	if ( cmp == COLORMAP_NULL)  {
		for( i = 0; i < 255; i++)  {
			_sgi_cmap.cm_red[i] = i;
			_sgi_cmap.cm_green[i] = i;
			_sgi_cmap.cm_blue[i] = i;
		}
		_sgi_cmap_flag = FALSE;
		return(0);
	}
	
	for(i = 0; i < 255; i++)  {
		_sgi_cmap.cm_red[i] = cmp -> cm_red[i]>>8;
		_sgi_cmap.cm_green[i] = cmp-> cm_green[i]>>8; 
		_sgi_cmap.cm_blue[i] = cmp-> cm_blue[i]>>8;

	}
	_sgi_cmap_flag = TRUE;
	return(0);
}
