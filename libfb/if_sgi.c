#undef _LOCAL_
#define _LOCAL_ /**/
/*
 *			I F _ S G I . C
 *
 *  SGI display interface.  By default, we operate in 24-bit (RGB) mode.
 *  However, when running under MEX, 12-bit mode is utilized (actually,
 *  only 10 bits are available, thanks to MEX).  Several flavors of
 *  MEX operation are defined, either a best-fit color match, or
 *  a pre-defined colorspace.  Each has advantages and disadvantages.
 *
 *  In order to simulate the behavior of a real framebuffer, even
 *  when using the limited color map for display, an entire image
 *  worth of memory is saved using SysV shared memory.  This image
 *  exists across invocations of frame buffer utilities, and allows
 *  the full resolution of an image to be retained, and captured,
 *  even with the 10x10x10 color cube display.
 *
 *  In order to use this sized chunk of memory with the shared memory
 *  system, it is necessary to "poke" your kernel to authorized this.
 *  In the shminfo structure, change shmmax from 0x10000 to 0x250000,
 *  shmall from 0x40 to 0x258, and tcp spaces from 2000 to 4000 by running:
 *
 *	adb -w -k /vmunix
 *	shminfo?W 250000
 *	shminfo+0x14?W 258
 *	tcp_sendspace?W 4000
 *	tcp_recvspace?W 4000
 *
 *  Note that these numbers are for release 3.5;  other versions
 *  may vary.
 *
 *  Authors -
 *	Paul R. Stay
 *	Gary S. Moss
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
#include <ctype.h>
#include <gl.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <gl2/immed.h>
#undef RED

#include "fb.h"
#include "./fblocal.h"

#ifdef SYSV
#define bzero(p,cnt)	memset(p,'\0',cnt)
#endif

/* Local state variables within the FBIO structure */
extern char *malloc();

/*
 *  Defines for dealing with SGI Graphics Engine Pipeline
 */
union gepipe {
	unsigned short us;
	short	s;
	long	l;
	float	f;
};

/**#define MC_68010 xx	/* not a turbo */
#ifdef MC_68010
#define GE	(0X00FD5000)		/* Not a turbo */
#else
#define GE	(0X60001000)		/* A turbo */
#endif
#define GEPIPE	((union gepipe *)GE)
#define GEP_END(p)	((union gepipe *)(((char *)p)-0x1000))	/* 68000 efficient 0xFd4000 */

#define CMOV2S(_p,_x,_y) { \
		(_p)->l = 0x0008001A; \
		(_p)->s = 0x0912; \
		(_p)->s = (_x); \
		(_p)->s = (_y); \
		GEP_END(hole)->s = (0xFF<<8)|8; \
		}

static Cursor	cursor =
	{
#include "./sgicursor.h"
	};

_LOCAL_ int	sgi_dopen(),
		sgi_dclose(),
		sgi_dclear(),
		sgi_bread(),
		sgi_bwrite(),
		sgi_cmread(),
		sgi_cmwrite(),
		sgi_viewport_set(),
		sgi_window_set(),
		sgi_zoom_set(),
		sgi_curs_set(),
		sgi_cmemory_addr(),
		sgi_cscreen_addr();

/* This is the ONLY thing that we "export" */
FBIO sgi_interface =
		{
		sgi_dopen,
		sgi_dclose,
		fb_null,		/* reset? */
		sgi_dclear,
		sgi_bread,
		sgi_bwrite,
		sgi_cmread,
		sgi_cmwrite,
		sgi_viewport_set,
		sgi_window_set,
		sgi_zoom_set,
		sgi_curs_set,
		sgi_cmemory_addr,
		fb_null,		/* cscreen_addr */
		"Silicon Graphics IRIS",
		1024,			/* max width */
		768,			/* max height */
		"/dev/sgi",
		1024,			/* current/default width  */
		768,			/* current/default height */
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


/* Interface to the 12-bit window versions of these routines */
int		sgw_dopen();
_LOCAL_ int
		sgw_bwrite(),
		sgw_curs_set(),
		sgw_cmemory_addr(),
		sgw_cscreen_addr();
_LOCAL_ Colorindex get_Color_Index();
_LOCAL_ void	sgw_inqueue();

/* This one is not exported, but is used for roll-over to 12-bit mode */
static FBIO sgiw_interface =
		{
		sgw_dopen,
		sgi_dclose,
		fb_null,
		sgi_dclear,
		sgi_bread,
		sgw_bwrite,
		sgi_cmread,
		sgi_cmwrite,
		sgi_viewport_set,
		sgi_window_set,
		sgi_zoom_set,
		sgi_curs_set,
		sgi_cmemory_addr,
		fb_null,
		"Silicon Graphics IRIS, in 12-bit mode, for windows",
		1024,			/* max width */
		768,			/* max height */
		"/dev/sgiw",
		512,			/* current/default width  */
		512,			/* current/default height */
		-1,			/* file descriptor */
		PIXEL_NULL,		/* page_base */
		PIXEL_NULL,		/* page_curp */
		PIXEL_NULL,		/* page_endp */
		-1,			/* page_no */
		0,			/* pdirty */
		0L,			/* page_curpos */
		0L,			/* page_pixels */
		0			/* debug */
		};

/*
 *  Per SGI (window or device) state information
 *  Too much for just the if_u[1-6] area now.
 */
struct sgiinfo {
	short	si_zoomflag;
	short	si_xzoom;
	short	si_yzoom;
	short	si_xcenter;
	short	si_ycenter;
	short	si_special_zoom;
	short	si_mode;
	int	si_rgb_ct;
	short	si_curs_on;
	short	si_cmap_flag;
};
#define	SGI(ptr) ((struct sgiinfo *)((ptr)->u1.p))
#define	SGIL(ptr) ((ptr)->u1.p)		/* left hand side version */
#define if_mem	u2.p			/* shared memory pointer */

/* Define current display operating mode */
#define MODE_RGB	0		/* 24-bit mode */
#define MODE_APPROX	1		/* color cube approximation */
#define MODE_FIT	2		/* Best-fit mode */

_LOCAL_ ColorMap _sgi_cmap;

#define Abs( x_ )	((x_) < 0 ? -(x_) : (x_))

#define MARGIN	4			/* # pixels margin to screen edge */
#define BANNER	18			/* Size of MEX title banner */
#define WIN_L	(1024-ifp->if_width-MARGIN)
#define WIN_R	(1024-MARGIN)
#define WIN_B	MARGIN
#define WIN_T	(ifp->if_height+MARGIN)

#define MAP_RESERVED	16		/* # slots reserved by MEX */
#define MAP_TOL		28		/* pixel delta across all channels */
/* TOL of 28 gives good rendering of the dragon picture without running out */
static int map_size;			/* # of color map slots available */

static RGBpixel	rgb_table[4096];

_LOCAL_ int
sgi_getmem( ifp )
FBIO	*ifp;
{
	int key = 42;
	int shmid;
	int size = 1024 * 768 * sizeof(RGBpixel);
	int i;
	extern int errno;
	extern char *shmat();
	char *sp;

	errno = 0;
	if( (shmid = shmget( key, size, IPC_CREAT|0666 )) < 0 )  {
		fb_log("shmget returned %d, errno=%d\n", shmid, errno);
		goto fail;
	}
	/* Open the segment Read/Write */
	if( (sp = shmat( shmid, 0, 0 )) == (char *)(-1) )  {
		fb_log("shmat returned x%x, errno=%d\n", sp, errno );
		goto fail;
	}
	ifp->if_mem = sp;
	return(0);
fail:
	fb_log("sgi_getmem:  Unable to attach to shared memory.\nConsult comment in cad/libfb/if_sgi.c for details\n");
	if( (sp = malloc( size )) == NULL )  {
		fb_log("sgi_getmem:  malloc failure\n");
		return(-1);
	}
	ifp->if_mem = sp;
	return(0);
}

/*
 *			S G I _ R E P A I N T
 *
 *  Given the current window center, and the current zoom,
 *  repaint the screen from the shared memory buffer,
 *  which stores RGB pixels.
 */
_LOCAL_ int
sgi_repaint( ifp )
register FBIO	*ifp;
{
	register union gepipe *hole = GEPIPE;
	short xmin, xmax;
	short ymin, ymax;
	register short i;
	register unsigned char *ip;
	short y;
	short xscroff, yscroff;

	if( SGI(ifp)->si_curs_on )
		cursoff();		/* Cursor interferes with drawing */

	xscroff = yscroff = 0;
	i = (ifp->if_width/2)/SGI(ifp)->si_xzoom;
	xmin = SGI(ifp)->si_xcenter - i;
	xmax = SGI(ifp)->si_xcenter + i + 1;
	i = (ifp->if_height/2)/SGI(ifp)->si_yzoom;
	ymin = SGI(ifp)->si_ycenter - i;
	ymax = SGI(ifp)->si_ycenter + i + 1;
	if( xmin < 0 )  {
		xscroff = -xmin * SGI(ifp)->si_xzoom;
		xmin = 0;
	}
	if( ymin < 0 )  {
		yscroff = -ymin * SGI(ifp)->si_yzoom;
		ymin = 0;
	}
	if( xmax > ifp->if_width-1 )  xmax = ifp->if_width-1;
	if( ymax > ifp->if_height-1 )  ymax = ifp->if_height-1;

	for( y = ymin; y <= ymax; y++ )  {

		ip = (unsigned char *)
			&ifp->if_mem[(y*1024+xmin)*sizeof(RGBpixel)];

		if( SGI(ifp)->si_zoomflag )  {
			register Scoord l, b, r, t;

			l = xscroff;
			b = yscroff + (y-ymin)*SGI(ifp)->si_yzoom;
			t = b + SGI(ifp)->si_yzoom;
			for( i=xmax-xmin; i > 0; i--)  {
				switch( SGI(ifp)->si_mode ) {
				case MODE_RGB:
					RGBcolor( (short)(ip[RED]),
						(short)(ip[GRN]),
						(short)(ip[BLU]) );
					break;
				case MODE_FIT:
					color(get_Color_Index( ifp, ip ));
					break;
				case MODE_APPROX:
					color(MAP_RESERVED +
						(ip[RED]/26) +
						(ip[GRN]/26) * 10 +
						(ip[BLU]/26) * 100 );
					break;
				}
				r = l + SGI(ifp)->si_xzoom;
				/* left bottom right top: im_rectfs( l, b, r, t ); */
				hole->s = GEmovepoly | GEPA_2S;
				hole->s = l;
				hole->s = b;
				hole->s = GEdrawpoly | GEPA_2S;
				hole->s = r;
				hole->s = b;
				hole->s = GEdrawpoly | GEPA_2S;
				hole->s = r;
				hole->s = t;
				hole->s = GEdrawpoly | GEPA_2S;
				hole->s = l;
				hole->s = t;
				hole->s = GEclosepoly;	/* Last? */
				l = r;
				ip += sizeof(RGBpixel);
			}
			continue;
		}

		/* Non-zoomed case */
		hole->l = 0x0008001A;	/* passthru, */
		hole->s = 0x0912;		/* cmov2s */
		hole->s = xscroff + xmin;
		hole->s = yscroff + y;
		switch( SGI(ifp)->si_mode )  {
		case MODE_RGB:
			for( i=xmax-xmin; i > 0; )  {
				register short chunk;

				if( i <= (127/3) )
					chunk = i;
				else
					chunk = 127/3;
				hole->s = ((chunk*3)<<8)|8;	/* GEpassthru */
				hole->s = 0xD;		/* FBCdrawpixels */
				i -= chunk;
				for( ; chunk>0; chunk--)  {
					hole->us = *ip++;
					hole->us = *ip++;
					hole->us = *ip++;
				}
			}
			break;
		case MODE_FIT:
			for( i=xmax-xmin; i > 0; )  {
				register short chunk;

				if( i <= 127 )
					chunk = i;
				else
					chunk = 127;
				hole->s = (chunk<<8)|8; /* GEpassthru */
				hole->s = 0xD;		 /* FBCdrawpixels */
				i -= chunk;
				for( ; chunk > 0; chunk-- )  {
					hole->s = get_Color_Index( ifp, ip );
				}
			}
			break;
		case MODE_APPROX:
			for( i=xmax-xmin; i > 0; )  {
				register short chunk;

				if( i <= 127 )
					chunk = i;
				else
					chunk = 127;
				hole->s = (chunk<<8)|8; /* GEpassthru */
				hole->s = 0xD;		 /* FBCdrawpixels */
				i -= chunk;
				for( ; chunk > 0; chunk--, ip += sizeof(RGBpixel) )  {
					hole->s = MAP_RESERVED +
						(ip[RED]/26) +
						(ip[GRN]/26) * 10 +
						(ip[BLU]/26) * 100;
				}
			}
			break;
		}
	}
	GEP_END(hole)->s = (0xFF<<8)|8;	/* im_last_passthru(0) */
	if( SGI(ifp)->si_curs_on )
		curson();		/* Cursor interferes with reading! */
}

_LOCAL_ int
sgi_dopen( ifp, file, width, height )
FBIO	*ifp;
char	*file;
int	width, height;
{
	int x_pos, y_pos;	/* Lower corner of viewport */
	register int i;
	
	if( ismex() )  {
		char *name;

		name = ifp->if_name;
		*ifp = sgiw_interface;	/* struct copy */
		ifp->if_name = name;
		return( sgw_dopen( ifp, file, width, height ) );
	}
	gbegin();		/* not ginit() */
	RGBmode();
	gconfig();
	if( getplanes() < 24 )  {
		char *name;

		singlebuffer();
		gconfig();
		name = ifp->if_name;
		*ifp = sgiw_interface;	/* struct copy */
		ifp->if_name = name;
		return( sgw_dopen( ifp, file, width, height ) );
	}
	tpoff();		/* Turn off textport */
	cursoff();

	blanktime( 67 * 60 * 60L );	/* 1 hour blanking when fb open */

	if( width <= 0 )
		width = ifp->if_width;
	if( height <= 0 )
		height = ifp->if_height;
	if ( width > ifp->if_max_width) 
		width = ifp->if_max_width;

	if ( height > ifp->if_max_height) 
		height = ifp->if_max_height;

	ifp->if_width = width;
	ifp->if_height = height;

	if( (SGIL(ifp) = (char *)calloc( 1, sizeof(struct sgiinfo) )) == NULL )  {
		fb_log("sgi_dopen:  sgiinfo malloc failed\n");
		return(-1);
	}
	SGI(ifp)->si_zoomflag = 0;
	SGI(ifp)->si_xzoom = 1;
	SGI(ifp)->si_yzoom = 1;
	SGI(ifp)->si_xcenter = width/2;
	SGI(ifp)->si_ycenter = height/2;
	SGI(ifp)->si_mode = MODE_RGB;
	if( sgi_getmem(ifp) < 0 )
		return(-1);

	/* Setup default cursor.					*/
	defcursor( 1, cursor );
	curorigin( 1, 0, 0 );

	/* Build a linear "colormap" in case he wants to read it */
	sgi_cmwrite( ifp, COLORMAP_NULL );

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

	switch( SGI(ifp)->si_mode )  {
	case MODE_RGB:
		gexit();
		break;
	case MODE_FIT:
	case MODE_APPROX:
		if( ismex() )  {
			/* Leave mex's cursor on */
			/* winclose( SGI(ifp)->si_fd ); */
		}  else  {
			setcursor( 0, 1, 0x2000 );
			cursoff();
			greset();
			gexit();
		}
		break;
	}
	if( SGIL(ifp) != NULL )
		(void)free( (char *)SGIL(ifp) );
	return(0);
}

_LOCAL_ int
sgi_dclear( ifp, pp )
FBIO	*ifp;
register RGBpixel	*pp;
{

	if( qtest() )
		sgw_inqueue(ifp);

	if ( pp != RGBPIXEL_NULL)  {
		register char *op = ifp->if_mem;
		register int cnt;

		/* Slightly simplistic -- runover to right border */
		for( cnt=1024*ifp->if_height-1; cnt > 0; cnt-- )  {
			*op++ = (*pp)[RED];
			*op++ = (*pp)[GRN];
			*op++ = (*pp)[BLU];
		}

		switch( SGI(ifp)->si_mode )  {
		case MODE_RGB:
			RGBcolor((short)((*pp)[RED]), (short)((*pp)[GRN]), (short)((*pp)[BLU]));
			break;
		case MODE_FIT:
			writemask( 0x3FF );
			color( get_Color_Index( ifp, pp ) );
			break;
		case MODE_APPROX:
			writemask( 0x3FF );
			color( MAP_RESERVED +
				((*pp)[RED]/26) +
				((*pp)[GRN]/26) * 10 +
				((*pp)[BLU]/26) * 100 );
			break;
		}
	} else {
		switch( SGI(ifp)->si_mode )  {
		case MODE_RGB:
			RGBcolor( (short) 0, (short) 0, (short) 0);
			break;
		case MODE_FIT:
		case MODE_APPROX:
			color(BLACK);
			break;
		}
		bzero( ifp->if_mem, 1024*ifp->if_height*sizeof(RGBpixel) );
	}
	clear();
	return(0);
}

_LOCAL_ int
sgi_window_set( ifp, x, y )
FBIO	*ifp;
int	x, y;
{
	if( qtest() )
		sgw_inqueue(ifp);

	if( SGI(ifp)->si_xcenter == x && SGI(ifp)->si_ycenter == y )
		return(0);
	if( x < 0 || x >= ifp->if_width )
		return(-1);
	if( y < 0 || y >= ifp->if_height )
		return(-1);
	SGI(ifp)->si_xcenter = x;
	SGI(ifp)->si_ycenter = y;
	sgi_repaint( ifp );
	return(0);
}

_LOCAL_ int
sgi_zoom_set( ifp, x, y )
FBIO	*ifp;
int	x, y;
{
	int npts;

	if( qtest() )
		sgw_inqueue(ifp);

	if( x < 1 ) x = 1;
	if( y < 1 ) y = 1;
	if( SGI(ifp)->si_xzoom == x && SGI(ifp)->si_yzoom == y )
		return(0);
	if( x >= ifp->if_width || y >= ifp->if_height )
		return(-1);

	SGI(ifp)->si_xzoom = x;
	SGI(ifp)->si_yzoom = y;

	if( SGI(ifp)->si_xzoom > 1 || SGI(ifp)->si_yzoom > 1 )
		SGI(ifp)->si_zoomflag = 1;
	else	SGI(ifp)->si_zoomflag = 0;

	sgi_repaint( ifp );
	return(0);
}

_LOCAL_ int
sgi_bread( ifp, x, y, pixelp, count )
FBIO	*ifp;
int	x, y;
register RGBpixel	*pixelp;
int	count;
{
	register short scan_count;
	short xpos, ypos;
	register char *ip;
	int ret;

	if( qtest() )
		sgw_inqueue(ifp);

	ret = 0;
	xpos = x;
	ypos = y;

	while( count > 0 )  {
		ip = &ifp->if_mem[(ypos*1024+xpos)*sizeof(RGBpixel)];

		if ( count >= ifp->if_width-xpos )  {
			scan_count = ifp->if_width-xpos;
		} else	{
			scan_count = count;
		}

#ifdef BSD
		bcopy( ip, *pixelp, scan_count*sizeof(RGBpixel) );
#else
		memcpy( *pixelp, ip, scan_count*sizeof(RGBpixel) );
#endif

		pixelp += scan_count;
		count -= scan_count;
		ret += scan_count;
		xpos = 0;
		/* Advance upwards */
		if( ++ypos >= ifp->if_height )
			break;
	}
	return(ret);
}

_LOCAL_ int
sgi_bwrite( ifp, x, y, pixelp, count )
FBIO	*ifp;
int	x, y;
RGBpixel *pixelp;
int	count;
{
	register union gepipe *hole = GEPIPE;
	register int scan_count;
	int xpos, ypos;
	register int i;
	register unsigned char *cp;
	register unsigned char *op;
	int ret;

	if( SGI(ifp)->si_curs_on )
		cursoff();		/* Cursor interferes with writing */
	ret = 0;
	xpos = x;
	ypos = y;
	cp = (unsigned char *)(*pixelp);
	while( count > 0 )  {
		if( ypos >= ifp->if_height )
			break;
		scan_count = ifp->if_width / SGI(ifp)->si_xzoom;
		if( count < scan_count )
			scan_count = count;

		hole->l = 0x0008001A;	/* passthru, */
		hole->s = 0x0912;		/* cmov2s */
		hole->s = xpos;
		hole->s = ypos;
		op = (unsigned char *)&ifp->if_mem[(ypos*1024+xpos)*sizeof(RGBpixel)];

		for( i=scan_count; i > 0; )  {
			register short chunk;

			if( SGI(ifp)->si_zoomflag )  {
				Scoord l, b, r, t;

				RGBcolor( (short)(cp[RED]),
					(short)(cp[GRN]),
					(short)(cp[BLU]) );
				l = xpos * SGI(ifp)->si_xzoom;
				b = ypos * SGI(ifp)->si_yzoom;
				r = l + SGI(ifp)->si_xzoom;
				t = b + SGI(ifp)->si_yzoom;

				/* left bottom right top: im_rectfs( l, b, r, t ); */
				hole->s = GEmovepoly | GEPA_2S;
				hole->s = l;
				hole->s = b;
				hole->s = GEdrawpoly | GEPA_2S;
				hole->s = r;
				hole->s = b;
				hole->s = GEdrawpoly | GEPA_2S;
				hole->s = r;
				hole->s = t;
				hole->s = GEdrawpoly | GEPA_2S;
				hole->s = l;
				hole->s = t;
				hole->s = GEclosepoly;	/* Last? */

				*op++ = *cp++;
				*op++ = *cp++;
				*op++ = *cp++;
				i--;
				xpos++;
				continue;
			}
			if( i <= (127/3) )
				chunk = i;
			else
				chunk = 127/3;
			hole->s = ((chunk*3)<<8)|8;	/* GEpassthru */
			hole->s = 0xD;		/* FBCdrawpixels */
			i -= chunk;
			if ( SGI(ifp)->si_cmap_flag == FALSE )  {
				for( ; chunk>0; chunk--)  {
					hole->us = (*op++ = *cp++);
					hole->us = (*op++ = *cp++);
					hole->us = (*op++ = *cp++);
				}
			} else {
				for( ; chunk>0; chunk-- )  {
					hole->s = _sgi_cmap.cm_red[*op++ = *cp++];
					hole->s = _sgi_cmap.cm_green[*op++ = *cp++];
					hole->s = _sgi_cmap.cm_blue[*op++ = *cp++];
				}
			}
		}

		count -= scan_count;
		xpos = 0;
		ypos++;
	}
	GEP_END(hole)->s = (0xFF<<8)|8;	/* im_last_passthru(0) */
	if( SGI(ifp)->si_curs_on )
		curson();		/* Cursor interferes with writing */
	return(ret);
}

_LOCAL_ int
sgi_viewport_set( ifp, left, top, right, bottom )
FBIO	*ifp;
int	left, top, right, bottom;
{
	if( qtest() )
		sgw_inqueue(ifp);
#if 0
	viewport(	(Screencoord) left,
			(Screencoord) right,
			(Screencoord) top,
			(Screencoord) (bottom * fb2iris_scale)
			);
#endif
	return(0);
}

_LOCAL_ int
sgi_cmread( ifp, cmp )
FBIO	*ifp;
register ColorMap	*cmp;
{
	register int i;

	if( qtest() )
		sgw_inqueue(ifp);

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

	if( qtest() )
		sgw_inqueue(ifp);

	if ( cmp == COLORMAP_NULL)  {
		for( i = 0; i < 255; i++)  {
			_sgi_cmap.cm_red[i] = i;
			_sgi_cmap.cm_green[i] = i;
			_sgi_cmap.cm_blue[i] = i;
		}
		SGI(ifp)->si_cmap_flag = FALSE;
		return(0);
	}
	
	for(i = 0; i < 255; i++)  {
		_sgi_cmap.cm_red[i] = cmp -> cm_red[i]>>8;
		_sgi_cmap.cm_green[i] = cmp-> cm_green[i]>>8; 
		_sgi_cmap.cm_blue[i] = cmp-> cm_blue[i]>>8;

	}
	SGI(ifp)->si_cmap_flag = TRUE;
	return(0);
}

_LOCAL_ int
sgi_curs_set( ifp, bits, xbits, ybits, xorig, yorig )
FBIO	*ifp;
unsigned char	*bits;
int		xbits, ybits;
int		xorig, yorig;
	{	register int	y;
		register int	xbytes;
		Cursor		newcursor;
	/* Check size of cursor.					*/
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
	for( y = 0; y < ybits; y++ )
		{
		newcursor[y] = bits[(y*xbytes)+0] << 8 & 0xFF00;
		if( xbytes == 2 )
			newcursor[y] |= bits[(y*xbytes)+1] & 0x00FF;
		}
	defcursor( 1, newcursor );
	curorigin( 1, (short) xorig, (short) yorig );
	return	0;
	}

_LOCAL_ int
sgi_cmemory_addr( ifp, mode, x, y )
FBIO	*ifp;
int	mode;
int	x, y;
	{
	SGI(ifp)->si_curs_on = mode;
	if( ! mode )
		{
		cursoff();
		return	0;
		}
	x *= SGI(ifp)->si_xzoom;
	y *= SGI(ifp)->si_yzoom;
	curson();
	RGBcursor( 1, 255, 255, 0, 0xFF, 0xFF, 0xFF );
	setvaluator( MOUSEX, x, 0, 1023 );
	setvaluator( MOUSEY, y, 0, 1023 );
	return	0;
	}

/*
 *			g e t _ C o l o r _ I n d e x
 */
_LOCAL_ Colorindex
get_Color_Index( ifp, pixelp )
register FBIO		*ifp;
register RGBpixel	*pixelp;
{
	register int		i;
	int			best = 7;
	register RGBpixel	*sp;
	static int		groused = 0;
	register int		min_diff = 128;

	/* Find best fit in existing table */
	best = 0;
	for( i = 0, sp = rgb_table; i < SGI(ifp)->si_rgb_ct; sp++, i++ ) {
		register int	diff;
		register int	d;

		d = ((int)((*pixelp)[RED])) - ((int)(*sp)[RED]);
		if( (diff = Abs(d)) >= min_diff )
			continue;
		d = ((int)((*pixelp)[GRN])) - ((int)(*sp)[GRN]);
		if( (diff += Abs(d)) >= min_diff )
			continue;
		d = ((int)((*pixelp)[BLU])) - ((int)(*sp)[BLU]);
		if( (diff += Abs(d)) >= min_diff )
			continue;

		/* [i]'th element is the best so far... */
		if( (min_diff = diff) <= 2 )  {
			/* Great match */
			return( (Colorindex)i );
		}
		best = i;
	}

	/* Match found to within tolerance? */
	if( min_diff < MAP_TOL ) 
		return	(Colorindex)best;

	/* Allocate new entry in color table if there's room.		*/
	if( i < map_size )  {
		COPYRGB( rgb_table[SGI(ifp)->si_rgb_ct], *pixelp);
		mapcolor(	(Colorindex)SGI(ifp)->si_rgb_ct,
				(short) (*pixelp)[RED],
				(short) (*pixelp)[GRN],
				(short) (*pixelp)[BLU]
				);
		return	(Colorindex)(SGI(ifp)->si_rgb_ct++);
	}

	/* No room to add, use best we found */
	if( !groused )  {
		groused = 1;
		fb_log( "Color table now full, will use closest matches.\n" );
	}
	return	(Colorindex)best;
}

#define SET(i,r,g,b)	{ \
	rgb_table[i][RED] = r; \
	rgb_table[i][GRN] = g; \
	rgb_table[i][BLU] = b; }


int
sgw_dopen( ifp, file, width, height )
FBIO	*ifp;
char	*file;
int	width, height;
{	register Colorindex i;

	if( (SGIL(ifp) = (char *)calloc( 1, sizeof(struct sgiinfo) )) == NULL )  {
		fb_log("sgw_dopen:  sgiinfo malloc failed\n");
		return(-1);
	}

	SGI(ifp)->si_mode = MODE_APPROX;
	if( file != NULL )  {
		register char *cp;
		int mode;
		/* "/dev/sgiw###" gives optional mode */
		for( cp = file; *cp != NULL && !isdigit(*cp); cp++ ) ;
		mode = MODE_APPROX;
		(void)sscanf( cp, "%d", &mode );
		if( mode < MODE_APPROX || mode > MODE_FIT )
			mode = MODE_APPROX;
		SGI(ifp)->si_mode = mode;
	}

	if( width <= 0 )
		width = ifp->if_width;
	if( height <= 0 )
		height = ifp->if_height;
	if ( width > ifp->if_max_width)
		width = ifp->if_max_width;
	if ( height > ifp->if_max_height - 2 * MARGIN - BANNER)
		height = ifp->if_max_height - 2 * MARGIN - BANNER;

	ifp->if_width = width;
	ifp->if_height = height;

	SGI(ifp)->si_zoomflag = 0;
	SGI(ifp)->si_xzoom = 1;	/* for zoom fakeout */
	SGI(ifp)->si_yzoom = 1;	/* for zoom fakeout */
	SGI(ifp)->si_xcenter = width/2;
	SGI(ifp)->si_ycenter = height/2;
	SGI(ifp)->si_special_zoom = 0;

	if( ismex() )  {
		prefposition( WIN_L, WIN_R, WIN_B, WIN_T );
		foreground();		/* Direct focus here, don't detach */
		if( (ifp->if_fd = winopen( "Frame buffer" )) == -1 )
			{
			fb_log( "No more graphics ports available.\n" );
			return	-1;
			}
		wintitle( "BRL libfb Frame Buffer" );
		singlebuffer();
		gconfig();	/* Must be called after singlebuffer().	*/
		/* Need to clean out images from windows below */
		/* This hack is necessary until windows persist from
		 * process to process */
		color(BLACK);
		clear();
	} else {
		ginit();
		singlebuffer();
		gconfig();	/* Must be called after singlebuffer().	*/
	}

	if( sgi_getmem(ifp) < 0 )
		return(-1);

	/*
	 * Deal with the color map
	 */
	map_size = 1<<getplanes();	/* 10 or 12, depending on ismex() */

	/* The first 8 entries of the colormap are "known" colors */
	SET( 0, 000, 000, 000 );	/* BLACK */
	SET( 1, 255, 000, 000 );	/* RED */
	SET( 2, 000, 255, 000 );	/* GREEN */
	SET( 3, 255, 255, 000 );	/* YELLOW */
	SET( 4, 000, 000, 255 );	/* BLUE */
	SET( 5, 255, 000, 255 );	/* MAGENTA */
	SET( 6, 000, 255, 000 );	/* CYAN */
	SET( 7, 255, 255, 255 );	/* WHITE */

	SGI(ifp)->si_rgb_ct = MAP_RESERVED;
	if( SGI(ifp)->si_mode == MODE_APPROX )  {
		/* Use fixed color map with 10x10x10 color cube */
		for( i = 0; i < map_size-MAP_RESERVED; i++ )
			mapcolor( 	i+MAP_RESERVED,
					(short)((i % 10) + 1) * 25,
					(short)(((i / 10) % 10) + 1) * 25,
					(short)((i / 100) + 1) * 25
					);
	}


	/* Build a linear "colormap" in case he wants to read it */
	sgi_cmwrite( ifp, COLORMAP_NULL );
	/* Setup default cursor.					*/
	defcursor( 1, cursor );
	curorigin( 1, 0, 0 );

	/* The screen has no useful state.  Restore it as it was before */
	sgi_repaint( ifp );
	return	0;
}

_LOCAL_ int
sgw_bwrite( ifp, x, y, pixelp, count )
FBIO	*ifp;
int	x, y;
register RGBpixel	*pixelp;
int	count;
{	
	register union gepipe *hole = GEPIPE;
	int scan_count;
	register int i;
	register unsigned char *op;
	int ret;

	if( qtest() )
		sgw_inqueue(ifp);
	writemask( 0x3FF );
	ret = 0;
	while( count > 0 )  {
		register short	ypos = y*SGI(ifp)->si_yzoom;
		register short	xpos = x*SGI(ifp)->si_xzoom;

		if( ypos >= ifp->if_height )
			break;
		scan_count = ifp->if_width / SGI(ifp)->si_xzoom;
		if( count < scan_count )
			scan_count = count;

		op = (unsigned char *)&ifp->if_mem[(y*1024+x)*sizeof(RGBpixel)];

		if( !(SGI(ifp)->si_zoomflag) )  {	
			register Colorindex	colori;

			CMOV2S( hole, xpos, ypos );
			for( i = scan_count; i > 0; )  {	
				register short	chunk;
				if( i <= 127 )
					chunk = i;
				else
					chunk = 127;
				hole->s = (chunk<<8)|8; /* GEpassthru */
				hole->s = 0xD;		 /* FBCdrawpixels */
				i -= chunk;
				for( ; chunk > 0; chunk--, pixelp++ )  {
					if( SGI(ifp)->si_mode == MODE_FIT ) {
						colori = get_Color_Index( ifp, pixelp );
						*op++ = (*pixelp)[RED];
						*op++ = (*pixelp)[GRN];
						*op++ = (*pixelp)[BLU];
						hole->s = colori;
						continue;
					}
					colori =  MAP_RESERVED +
					    ((*op++=(*pixelp)[RED])/26);
					colori += ((*op++=(*pixelp)[GRN])/26) * 10;
					colori += ((*op++=(*pixelp)[BLU])/26) * 100;
					hole->s = colori;
				}
			}
			GEP_END(hole)->s = (0xFF<<8)|8;	/* im_last_passthru(0) */
		} else {
			for( i = 0; i < scan_count; i++, pixelp++ )  {	
				register Colorindex	col;
				register Scoord	r = xpos + SGI(ifp)->si_xzoom - 1;
				register Scoord t = ypos + SGI(ifp)->si_yzoom - 1;

				CMOV2S( hole, xpos, ypos );
				if( SGI(ifp)->si_mode == MODE_FIT ) {
					col = get_Color_Index( ifp, pixelp );
					*op++ = (*pixelp)[RED];
					*op++ = (*pixelp)[GRN];
					*op++ = (*pixelp)[BLU];
				} else {
					col =  MAP_RESERVED +
					    ((*op++=(*pixelp)[RED])/26);
					col += ((*op++=(*pixelp)[GRN])/26) * 10;
					col += ((*op++=(*pixelp)[BLU])/26) * 100;
				}

				color( col );
				im_rectfs( (Scoord)xpos, (Scoord)ypos, r, t );
				xpos += SGI(ifp)->si_xzoom;
			}
		}
		count -= scan_count;
		ret += scan_count;
		x = 0;
		y++;
	}
	return(ret);
}

_LOCAL_ int
sgw_curs_set( ifp, bits, xbits, ybits, xorig, yorig )
FBIO	*ifp;
unsigned char	*bits;
int		xbits, ybits;
int		xorig, yorig;
	{	register int	y;
		register int	xbytes;
		Cursor		newcursor;

	if( qtest() )
		sgw_inqueue(ifp);
	/* Check size of cursor.					*/
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
	for( y = 0; y < ybits; y++ )
		{
		newcursor[y] = bits[(y*xbytes)+0] << 8 & 0xFF00;
		if( xbytes == 2 )
			newcursor[y] |= bits[(y*xbytes)+1] & 0x00FF;
		}
	defcursor( 1, newcursor );
	curorigin( 1, (short) xorig, (short) yorig );
	return	0;
	}

_LOCAL_ int
sgw_cmemory_addr( ifp, mode, x, y )
FBIO	*ifp;
int	mode;
int	x, y;
	{	static Colorindex	cursor_color = YELLOW;
			/* Color and bitmask ignored under MEX.	*/
	if( qtest() )
		sgw_inqueue(ifp);
	SGI(ifp)->si_curs_on = mode;
	if( ! mode )
		{
		cursoff();
		setcursor( 0, 1, 0x2000 );
		return	0;
		}
	x *= SGI(ifp)->si_xzoom;
	y *= SGI(ifp)->si_yzoom;
	curson();
	setcursor( 1, cursor_color, 0x2000 );
	setvaluator( MOUSEX, x + WIN_L, 0, 1023 );
	setvaluator( MOUSEY, y + WIN_B, 0, 1023 );
	return	0;
	}

/*
 *			S G W _ I N Q U E U E
 *
 *  Called when a qtest() indicates that there is a window event.
 *  Process all events, so that we don't loop on recursion to sgw_bwrite.
 */
_LOCAL_ void
sgw_inqueue(ifp)
register FBIO *ifp;
{
	short val;
	int redraw = 0;
	register int ev;

	while( qtest() )  {
		switch( ev = qread(&val) )  {
		case REDRAW:
			redraw = 1;
			break;
		case INPUTCHANGE:
			break;
		case MODECHANGE:
			/* This could be bad news.  Should we re-write
			 * the color map? */
			fb_log("sgw_inqueue:  modechange?\n");
			break;
		default:
			fb_log("sgw_inqueue:  event %d unknown\n", ev);
			break;
		}
	}
	/*
	 * Now that all the events have been removed from the input
	 * queue, handle any actions that need to be done.
	 */
	if( redraw )  {
		sgi_repaint( ifp );
		redraw = 0;
	}
}
