/*                        I F _ S G I . C
 * BRL-CAD
 *
 * Copyright (c) 1986-2007 United States Government as represented by
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
/** @addtogroup if */
/** @{ */
/** @file if_sgi.c
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
 *  In order to use this large a chunk of memory with the shared memory
 *  system, it is necessary to "poke" your kernel to authorize this.
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
 */
/** @} */

#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "common.h"



#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#include <gl.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <gl2/immed.h>
#undef RED

#include "machine.h"
#include "fb.h"
#include "./fblocal.h"

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
		PASSCMD( _p, 0, FBCcharposnabs ); \
		(_p)->s = GEpoint | GEPA_2S; \
		(_p)->s = (_x); \
		(_p)->s = (_y); \
		}

#define PASSCMD(p, n, cmd)	{(p)->l = ((GEpassthru|((n)<<8))<<16)|(cmd); }

static Cursor	cursor =
	{
#include "./sgicursor.h"
	};

HIDDEN int	sgi_open(),
		sgi_close(),
		sgi_clear(),
		sgi_read(),
		sgi_write(),
		sgi_rmap(),
		sgi_wmap(),
		sgi_view(),
		sgi_setcursor(),
		sgi_cursor(),
		sgi_poll(),
		sgi_help();

/* This is the ONLY thing that we "export" */
FBIO sgi_interface =
		{
		0,
		sgi_open,
		sgi_close,
		sgi_clear,
		sgi_read,
		sgi_write,
		sgi_rmap,
		sgi_wmap,
		sgi_view,
		fb_sim_getview,
		sgi_setcursor,		/* define cursor */
		sgi_cursor,		/* set cursor */
		fb_sim_getcursor,	/* get cursor */
		fb_sim_readrect,
		fb_sim_writerect,
		fb_sim_bwreadrect,
		fb_sim_bwwriterect,
		sgi_poll,		/* poll */
		fb_null,		/* flush */
		sgi_close,		/* free */
		sgi_help,
		"Silicon Graphics IRIS",
		1024,			/* max width */
		768,			/* max height */
		"/dev/sgi",
		1024,			/* current/default width  */
		768,			/* current/default height */
		-1,			/* select fd */
		-1,			/* file descriptor */
		1, 1,			/* zoom */
		512, 384,		/* window center */
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


/* Interface to the 12-bit window version */
int		sgw_dopen();

HIDDEN Colorindex get_Color_Index();
HIDDEN void	sgw_inqueue();
HIDDEN void	sgi_rectf_pix();
static int	is_linear_cmap();

struct sgi_cmap {
	unsigned char	cmr[256];
	unsigned char	cmg[256];
	unsigned char	cmb[256];
};

/*
 *  Per SGI (window or device) state information
 *  Too much for just the if_u[1-6] area now.
 */
struct sgiinfo {
	short	si_curs_on;
	int	si_rgb_ct;
	short	si_cmap_flag;
	int	si_shmid;
};
#define	SGI(ptr)	((struct sgiinfo *)((ptr)->u1.p))
#define	SGIL(ptr)	((ptr)->u1.p)		/* left hand side version */
#define if_mem		u2.p			/* shared memory pointer */
#define if_cmap		u3.p			/* color map in shared memory */
#define CMR(x)		((struct sgi_cmap *)((x)->if_cmap))->cmr
#define CMG(x)		((struct sgi_cmap *)((x)->if_cmap))->cmg
#define CMB(x)		((struct sgi_cmap *)((x)->if_cmap))->cmb
#define if_zoomflag	u4.l			/* zoom > 1 */
#define if_mode		u5.l			/* see MODE_* defines */

/* Define current display operating mode */
#define MODE_RGB	0		/* 24-bit mode */
#define MODE_APPROX	1		/* color cube approximation */
#define MODE_FIT	2		/* Best-fit mode */

/* Map RGB onto 10x10x10 color cube, giving index in range 0..999 */
#define COLOR_APPROX(p)	\
	(((p)[RED]/26)+ ((p)[GRN]/26)*10 + ((p)[BLU]/26)*100 + MAP_RESERVED)
/***
#define COLOR_APPROX(p)	\
	(((((((((p)[BLU])*10)+((p)[GRN]))*10)+((p)[RED]))*10)>>8)+MAP_RESERVED)
***/


#define Abs( x_ )	((x_) < 0 ? -(x_) : (x_))

#define MARGIN	4			/* # pixels margin to screen edge */
#define BANNER	18			/* Size of MEX title banner */
#define WIN_L	(1024-ifp->if_width-MARGIN)
#define WIN_R	(1024-1-MARGIN)
#define WIN_B	MARGIN
#define WIN_T	(ifp->if_height-1+MARGIN)

#define MAP_RESERVED	16		/* # slots reserved by MEX */
#define MAP_TOL		15		/* pixel delta across all channels */

static int map_size;			/* # of color map slots available */

static RGBpixel	rgb_table[4096];

/*
 *			S G I _ G E T M E M
 *
 *  Because there is no hardware zoom or pan, we need to repaint the
 *  screen (with big pixels) to implement these operations.
 *  This means that the actual "contents" of the frame buffer need
 *  to be stored somewhere else.  If possible, we allocate a shared
 *  memory segment to contain that image.  This has several advantages,
 *  the most important being that when operating the display in 12-bit
 *  output mode, pixel-readbacks still give the full 24-bits of color.
 *  System V shared memory persists until explicitly killed, so this
 *  also means that in MEX mode, the previous contents of the frame
 *  buffer still exist, and can be again accessed, even though the
 *  MEX windows are transient, per-process.
 *
 *  There are a few oddities, however.  The worst is that System V will
 *  not allow the break (see sbrk(2)) to be set above a shared memory
 *  segment, and shmat(2) does not seem to allow the selection of any
 *  reasonable memory address (like 6 Mbytes up) for the shared memory.
 *  In the initial version of this routine, that prevented subsequent
 *  calls to malloc() from succeeding, quite a drawback.  The work-around
 *  used here is to increase the current break to a large value,
 *  attach to the shared memory, and then return the break to it's
 *  original value.  This should allow most reasonable requests for
 *  memory to be satisfied.  In special cases, the values used here
 *  might need to be increased.
 */
HIDDEN int
sgi_getmem( ifp )
FBIO	*ifp;
{
#define SHMEM_KEY	42
	int	pixsize;
	int	size;
	int	i;
	char	*old_brk;
	char	*new_brk;
	char	*sp;
	int	new = 0;

	/* The shared memory section never changes size */
	errno = 0;
	pixsize = 1024 * 768 * sizeof(RGBpixel);
	size = pixsize + sizeof(struct sgi_cmap);
	size = (size + 4096-1) & ~(4096-1);
	/* First try to attach to an existing one */
	if( (SGI(ifp)->si_shmid = shmget( SHMEM_KEY, size, 0 )) < 0 )  {
		/* No existing one, create a new one */
		if( (SGI(ifp)->si_shmid = shmget(
		    SHMEM_KEY, size, IPC_CREAT|0666 )) < 0 )  {
			fb_log("if_sgi: shmget failed, errno=%d\n", errno);
			goto fail;
		}
		new = 1;
	}

	/* Move up the existing break, to leave room for later malloc()s */
	old_brk = sbrk(0);
	new_brk = (char *)(6 * 1024 * 1024);
	if( new_brk <= old_brk )
		new_brk = old_brk + 1024 * 1024;
	new_brk = (char *)((((int)new_brk) + 4096-1) & ~(4096-1));
	if( brk( new_brk ) < 0 )  {
		fb_log("new brk(x%x) failure, errno=%d\n", new_brk, errno);
		goto fail;
	}

	/* Open the segment Read/Write, near the current break */
	if( (sp = shmat( SGI(ifp)->si_shmid, 0, 0 )) == (char *)(-1) )  {
		fb_log("shmat returned x%x, errno=%d\n", sp, errno );
		goto fail;
	}

	/* Restore the old break */
	if( brk( old_brk ) < 0 )  {
		fb_log("restore brk(x%x) failure, errno=%d\n", old_brk, errno);
		/* Take the memory and run */
	}

success:
	ifp->if_mem = sp;
	ifp->if_cmap = sp + pixsize;	/* cmap at end of area */

	/* Provide non-black colormap on creation of new shared mem */
	if(new)
		sgi_wmap( ifp, COLORMAP_NULL );
	return(0);
fail:
	fb_log("sgi_getmem:  Unable to attach to shared memory.\n");
	if( (sp = malloc( size )) == NULL )  {
		fb_log("sgi_getmem:  malloc failure\n");
		return(-1);
	}
	new = 1;
	goto success;
}

/*
 *			S G I _ Z A P M E M
 */
void
sgi_zapmem()
{
	int shmid;
	int i;

	if( (shmid = shmget( SHMEM_KEY, 0, 0 )) < 0 )  {
		fb_log("sgi_zapmem shmget failed, errno=%d\n", errno);
		return;
	}

	i = shmctl( shmid, IPC_RMID, 0 );
	if( i < 0 )  {
		fb_log("sgi_zapmem shmctl failed, errno=%d\n", errno);
		return;
	}
	fb_log("if_sgi: shared memory released\n");
}

/*
 *			S G I _ R E P A I N T
 *
 *  Given the current window center, and the current zoom,
 *  repaint the screen from the shared memory buffer,
 *  which stores RGB pixels.
 */
HIDDEN int
sgi_repaint( ifp )
register FBIO	*ifp;
{
	register union gepipe *hole = GEPIPE;
	short xmin, xmax;
	short ymin, ymax;
	register short i;
	register unsigned char *ip;
	short y;
	short xscroff, yscroff, xscrpad, yscrpad;
	short xwidth;
	static RGBpixel black = { 0, 0, 0 };

	if( SGI(ifp)->si_curs_on )
		cursoff();		/* Cursor interferes with drawing */

	xscroff = yscroff = 0;
	xscrpad = yscrpad = 0;
	xwidth = ifp->if_width/ifp->if_xzoom;
	i = xwidth/2;
	xmin = ifp->if_xcenter - i;
	xmax = ifp->if_xcenter + i - 1;
	i = (ifp->if_height/2)/ifp->if_yzoom;
	ymin = ifp->if_ycenter - i;
	ymax = ifp->if_ycenter + i - 1;
	if( xmin < 0 )  {
		xscroff = -xmin * ifp->if_xzoom;
		xmin = 0;
	}
	if( ymin < 0 )  {
		yscroff = -ymin * ifp->if_yzoom;
		ymin = 0;
	}
	if( xmax > ifp->if_width-1 )  {
		xscrpad = (xmax-(ifp->if_width-1))*ifp->if_xzoom;
		xmax = ifp->if_width-1;
	}
	if( ymax > ifp->if_height-1 )  {
		yscrpad = (ymax-(ifp->if_height-1))*ifp->if_yzoom;
		ymax = ifp->if_height-1;
	}

	/* Blank out area left of image.			*/
	if( xscroff > 0 )
		sgi_rectf_pix(	ifp,
				0, 0,
				(Scoord) xscroff-1, (Scoord) ifp->if_height-1,
				(RGBpixel *) black
				);
	/* Blank out area below image.			*/
	if( yscroff > 0 )
		sgi_rectf_pix(	ifp,
				0, 0,
				(Scoord) ifp->if_width-1, (Scoord) yscroff-1,
				(RGBpixel *) black
				);
	/* Blank out area right of image.			*/
	if( xscrpad > 0 )
		sgi_rectf_pix(	ifp,
				(Scoord) ifp->if_width-xscrpad,
				0,
				(Scoord) ifp->if_width-1,
				(Scoord) ifp->if_height-1,
				(RGBpixel *) black
				);
	/* Blank out area above image.			*/
	if( yscrpad > 0 )
		sgi_rectf_pix(	ifp,
				0,
				(Scoord) ifp->if_height-yscrpad,
				(Scoord) ifp->if_width-1,
				(Scoord) ifp->if_height-1,
				(RGBpixel *) black
				);
	for( y = ymin; y <= ymax; y++ )  {

		ip = (unsigned char *)
			&ifp->if_mem[(y*1024+xmin)*sizeof(RGBpixel)];

		if( ifp->if_zoomflag )  {
			register Scoord l, b, r, t;

			l = xscroff;
			b = yscroff + (y-ymin)*ifp->if_yzoom;
			t = b + ifp->if_yzoom - 1;
			if ( SGI(ifp)->si_cmap_flag == FALSE )  {
				for( i=xwidth; i > 0; i--)  {
					switch( ifp->if_mode ) {
					case MODE_RGB:
						PASSCMD(hole,3,FBCrgbcolor);
						hole->s = (ip[RED]);
						hole->s = (ip[GRN]);
						hole->s = (ip[BLU]);
						break;
					case MODE_FIT:
						PASSCMD(hole,1,FBCcolor);
						hole->s = get_Color_Index( ifp, ip );
						break;
					case MODE_APPROX:
						PASSCMD(hole,1,FBCcolor);
						hole->s = COLOR_APPROX(ip);
						break;
					}
					r = l + ifp->if_xzoom - 1;
					/* left bottom right top: rectfs( l, b, r, t ); */
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
					l = r + 1;
					ip += sizeof(RGBpixel);
				}
				continue;
			} else {
				for( i=xwidth; i > 0; i--)  {
					static RGBpixel new;

					switch( ifp->if_mode ) {
					case MODE_RGB:
						PASSCMD(hole,3,FBCrgbcolor);
						hole->s = CMR(ifp)[ip[RED]];
						hole->s = CMG(ifp)[ip[GRN]];
						hole->s = CMB(ifp)[ip[BLU]];
						break;
					case MODE_FIT:
						new[RED] = CMR(ifp)[ip[RED]];
						new[GRN] = CMG(ifp)[ip[GRN]];
						new[BLU] = CMB(ifp)[ip[BLU]];
						PASSCMD(hole,1,FBCcolor);
						hole->s = get_Color_Index( ifp, new );
						break;
					case MODE_APPROX:
						new[RED] = CMR(ifp)[ip[RED]];
						new[GRN] = CMG(ifp)[ip[GRN]];
						new[BLU] = CMB(ifp)[ip[BLU]];
						PASSCMD(hole,1,FBCcolor);
						hole->s = COLOR_APPROX(new);
						break;
					}
					r = l + ifp->if_xzoom - 1;
					/* left bottom right top: rectfs( l, b, r, t ); */
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
					l = r + 1;
					ip += sizeof(RGBpixel);
				}
				continue;
			}
		}

		/* Non-zoomed case */
		CMOV2S( hole, xscroff, yscroff + (y-ymin) );

		switch( ifp->if_mode )  {
		case MODE_RGB:
			if ( SGI(ifp)->si_cmap_flag == FALSE )  {
				for( i=xwidth; i > 0; )  {
					register short chunk;

					if( i <= (127/3) )
						chunk = i;
					else
						chunk = 127/3;
					PASSCMD(hole, chunk*3, FBCdrawpixels);
					i -= chunk;
					for( ; chunk>0; chunk--)  {
						hole->us = *ip++;
						hole->us = *ip++;
						hole->us = *ip++;
					}
				}
			} else {
				for( i=xwidth; i > 0; )  {
					register short chunk;

					if( i <= (127/3) )
						chunk = i;
					else
						chunk = 127/3;
					PASSCMD(hole, chunk*3, FBCdrawpixels);
					i -= chunk;
					for( ; chunk>0; chunk--)  {
						hole->s = CMR(ifp)[*ip++];
						hole->s = CMG(ifp)[*ip++];
						hole->s = CMB(ifp)[*ip++];
					}
				}
			}
			break;
		case MODE_FIT:
			if ( SGI(ifp)->si_cmap_flag == FALSE )  {
				for( i=xwidth; i > 0; )  {
					register short chunk;

					if( i <= 127 )
						chunk = i;
					else
						chunk = 127;
					PASSCMD(hole, chunk, FBCdrawpixels);
					i -= chunk;
					for( ; chunk > 0; chunk--, ip += sizeof(RGBpixel) )  {
						hole->s = get_Color_Index( ifp, ip );
					}
				}
			} else {
				for( i=xwidth; i > 0; )  {
					register short chunk;

					if( i <= 127 )
						chunk = i;
					else
						chunk = 127;
					PASSCMD(hole, chunk, FBCdrawpixels);
					i -= chunk;
					for( ; chunk > 0; chunk-- )  {
						static RGBpixel new;
						new[RED] = CMR(ifp)[*ip++];
						new[GRN] = CMG(ifp)[*ip++];
						new[BLU] = CMB(ifp)[*ip++];
						hole->s = get_Color_Index( ifp, new );
					}
				}
			}
			break;
		case MODE_APPROX:
			if ( SGI(ifp)->si_cmap_flag == FALSE )  {
				for( i=xwidth; i > 0; )  {
					register short chunk;

					if( i <= 127 )
						chunk = i;
					else
						chunk = 127;
					PASSCMD(hole, chunk, FBCdrawpixels);
					i -= chunk;
					for( ; chunk > 0; chunk--, ip += sizeof(RGBpixel) )  {
						hole->s = COLOR_APPROX(ip);
					}
				}
			} else {
				for( i=xwidth; i > 0; )  {
					register short chunk;

					if( i <= 127 )
						chunk = i;
					else
						chunk = 127;
					PASSCMD(hole, chunk, FBCdrawpixels);
					i -= chunk;
					for( ; chunk > 0; chunk-- )  {
						static RGBpixel new;
						new[RED] = CMR(ifp)[*ip++];
						new[GRN] = CMG(ifp)[*ip++];
						new[BLU] = CMB(ifp)[*ip++];
						hole->s = COLOR_APPROX(new);
					}
				}
			}
			break;
		}
	}
	GEP_END(hole)->s = (0xFF<<8)|8;	/* im_last_passthru(0) */
	if( SGI(ifp)->si_curs_on )
		curson();		/* Cursor interferes with reading! */
}

/*
 *			S G I _ D O P E N
 */
HIDDEN int
sgi_open( ifp, file, width, height )
FBIO	*ifp;
char	*file;
int	width, height;
{
	int x_pos, y_pos;	/* Lower corner of viewport */
	register int i;

	FB_CK_FBIO(ifp);

	if( file != NULL )  {
		register char *cp;
		int mode;

		/* "/dev/sgi###" gives optional mode */
		for( cp = file; *cp != '\0' && !isdigit(*cp); cp++ ) ;
		mode = 0;
		if( *cp && isdigit(*cp) )
			(void)sscanf( cp, "%d", &mode );
		if( mode >= 99 )  {
			/* Attempt to release shared memory segment */
			sgi_zapmem();
			return(-1);
		}
		if( mode != 0 )
			return( sgw_dopen( ifp, file, width, height ) );
	}

	if( ismex() )  {
		return( sgw_dopen( ifp, file, width, height ) );
	}
	gbegin();		/* not ginit() */
	RGBmode();
	gconfig();
	if( getplanes() < 24 )  {
		singlebuffer();
		gconfig();
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
		fb_log("sgi_open:  sgiinfo malloc failed\n");
		return(-1);
	}
	SGI(ifp)->si_shmid = -1;	/* indicate no shared memory */

	/* Must initialize these window state variables BEFORE calling
		"sgi_getmem", because this function can indirectly trigger
		a call to "sgi_repaint" (when initializing shared memory
		after a reboot).					*/
	ifp->if_zoomflag = 0;
	ifp->if_xzoom = 1;
	ifp->if_yzoom = 1;
	ifp->if_xcenter = width/2;
	ifp->if_ycenter = height/2;
	ifp->if_mode = MODE_RGB;

	if( sgi_getmem(ifp) < 0 )
		return(-1);

	/* Must call "is_linear_cmap" AFTER "sgi_getmem" which allocates
		space for the color map.				*/
	SGI(ifp)->si_cmap_flag = !is_linear_cmap(ifp);

	/* Setup default cursor.					*/
	defcursor( 1, cursor );
	curorigin( 1, 0, 0 );

	return(0);
}

/*
 *			S G I _ C L O S E
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
HIDDEN int
sgi_close( ifp )
FBIO	*ifp;
{
	blanktime( 67 * 60 * 20L );	/* 20 minute blanking when fb closed */

	switch( ifp->if_mode )  {
	case MODE_RGB:
		gexit();
		break;
	case MODE_FIT:
	case MODE_APPROX:
		if( ismex() )  {
			winclose( ifp->if_fd );
			curson();	/* Leave mex's cursor on */
		}  else  {
			setcursor( 0, 1, 0x2000 );
			cursoff();
			greset();
			gexit();
		}
		break;
	}
	if( SGIL(ifp) != NULL ) {
		/* free up memory associated with image */
		if( SGI(ifp)->si_shmid != -1 ) {
			/* detach from shared memory */
			if( shmdt( ifp->if_mem ) == -1 ) {
				fb_log("sgi_close shmdt failed, errno=%d\n", errno);
				return -1;
			}
		} else {
			/* free private memory */
			(void)free( ifp->if_mem );
		}
		/* free state information */
		(void)free( (char *)SGIL(ifp) );
		SGIL(ifp) = NULL;
	}

	return(0);
}

/*
 *			S G I _ C L E A R
 */
HIDDEN int
sgi_clear( ifp, pp )
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

		switch( ifp->if_mode )  {
		case MODE_RGB:
			RGBcolor((short)((*pp)[RED]), (short)((*pp)[GRN]), (short)((*pp)[BLU]));
			break;
		case MODE_FIT:
			writemask( 0x3FF );
			color( get_Color_Index( ifp, pp ) );
			break;
		case MODE_APPROX:
			writemask( 0x3FF );
			color( COLOR_APPROX(*pp) );
			break;
		}
	} else {
		switch( ifp->if_mode )  {
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

HIDDEN int
sgi_poll(ifp)
FBIO	*ifp;
{
	if( qtest() )
		sgw_inqueue(ifp);
}

HIDDEN int
sgi_view( ifp, xcenter, ycenter, xzoom, yzoom )
FBIO	*ifp;
int	xcenter, ycenter;
int	xzoom, yzoom;
{
	if( qtest() )
		sgw_inqueue(ifp);

	if( xzoom < 1 ) xzoom = 1;
	if( yzoom < 1 ) yzoom = 1;
	if( ifp->if_xcenter == xcenter && ifp->if_ycenter == ycenter
	 && ifp->if_xzoom == xzoom && ifp->if_yzoom == yzoom )
		return(0);

	if( xcenter < 0 || xcenter >= ifp->if_width )
		return(-1);
	if( ycenter < 0 || ycenter >= ifp->if_height )
		return(-1);
	if( xzoom >= ifp->if_width || yzoom >= ifp->if_height )
		return(-1);

	ifp->if_xcenter = xcenter;
	ifp->if_ycenter = ycenter;
	ifp->if_xzoom = xzoom;
	ifp->if_yzoom = yzoom;

	if( ifp->if_xzoom > 1 || ifp->if_yzoom > 1 )
		ifp->if_zoomflag = 1;
	else	ifp->if_zoomflag = 0;

	sgi_repaint( ifp );
	return(0);
}

/*
 *			S G I _ R E A D
 */
HIDDEN int
sgi_read( ifp, x, y, pixelp, count )
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

	if( x < 0 || x > ifp->if_width ||
	    y < 0 || y > ifp->if_height )
		return(-1);
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
		memcpy( *pixelp, ip, scan_count*sizeof(RGBpixel) );

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

/*
 *			S G I _ B W R I T E
 */
HIDDEN int
sgi_write( ifp, xmem, ymem, pixelp, count )
FBIO	*ifp;
int	xmem, ymem;
RGBpixel *pixelp;
int	count;
{
	register union gepipe *hole = GEPIPE;
	register short scan_count;	/* # pixels on this scanline */
	int xscr, yscr;
	register int i;
	register unsigned char *cp;
	register unsigned char *op;
	int ret;
	int hfwidth = (ifp->if_width/ifp->if_xzoom)/2;
	int hfheight = (ifp->if_height/ifp->if_yzoom)/2;

	if( qtest() )
		sgw_inqueue(ifp);
	if( xmem < 0 || xmem > ifp->if_width ||
	    ymem < 0 || ymem > ifp->if_height)
		return(-1);
	if( SGI(ifp)->si_curs_on )
		cursoff();		/* Cursor interferes with writing */

	ret = 0;
	xscr = (xmem - (ifp->if_xcenter-hfwidth)) * ifp->if_xzoom;
	yscr = (ymem - (ifp->if_ycenter-hfheight)) * ifp->if_yzoom;
	cp = (unsigned char *)(*pixelp);
	while( count > 0 )  {
		if( yscr >= ifp->if_height )
			break;
		if ( count >= ifp->if_width-xmem )
			scan_count = ifp->if_width-xmem;
		else
			scan_count = count;

		op = (unsigned char *)&ifp->if_mem[(ymem*1024+xmem)*sizeof(RGBpixel)];

		if( ifp->if_zoomflag )  {
			register Scoord l, b, r, t;
			int todraw = (ifp->if_width-xscr)/ifp->if_xzoom;
			int tocopy;

			if( todraw > scan_count )  todraw = scan_count;
			tocopy = scan_count - todraw;

			l = xscr;
			b = yscr;
			t = b + ifp->if_yzoom - 1;
			for( i = todraw; i > 0; i-- )  {

				switch( ifp->if_mode )  {
				case MODE_RGB:
					PASSCMD(hole,3,FBCrgbcolor);
					if ( SGI(ifp)->si_cmap_flag == FALSE ) {
						hole->s = (cp[RED]);
						hole->s = (cp[GRN]);
						hole->s = (cp[BLU]);
					} else {
						hole->s = CMR(ifp)[cp[RED]];
						hole->s = CMG(ifp)[cp[GRN]];
						hole->s = CMB(ifp)[cp[BLU]];
					}
					break;
				case MODE_FIT:
					PASSCMD(hole,1,FBCcolor);
					if ( SGI(ifp)->si_cmap_flag == FALSE ) {
						hole->s = get_Color_Index( ifp, cp );
					} else {
						static RGBpixel new;
						new[RED] = CMR(ifp)[cp[RED]];
						new[GRN] = CMG(ifp)[cp[GRN]];
						new[BLU] = CMB(ifp)[cp[BLU]];
						hole->s = get_Color_Index( ifp, new );
					}
					break;
				case MODE_APPROX:
					PASSCMD(hole,1,FBCcolor);
					if ( SGI(ifp)->si_cmap_flag == FALSE ) {
						hole->s = COLOR_APPROX(cp);
					} else {
						static RGBpixel new;
						new[RED] = CMR(ifp)[cp[RED]];
						new[GRN] = CMG(ifp)[cp[GRN]];
						new[BLU] = CMB(ifp)[cp[BLU]];
						hole->s = COLOR_APPROX(new);
					}
					break;
				}
				r = l + ifp->if_xzoom - 1;

				/* left bottom right top: rectfs( l, b, r, t ); */
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
				l = r + 1;

				*op++ = *cp++;
				*op++ = *cp++;
				*op++ = *cp++;
			}
			for( i = tocopy; i > 0; i-- )  {
				*op++ = *cp++;
				*op++ = *cp++;
				*op++ = *cp++;
			}
			count -= scan_count;
			ret += scan_count;
			xmem = 0;
			ymem++;
			xscr = (hfwidth-ifp->if_xcenter) *
				ifp->if_xzoom;
			yscr += ifp->if_yzoom;
			continue;
		}
		/* Non-zoomed case */
		CMOV2S( hole, xscr, yscr );

		switch( ifp->if_mode )  {
		case MODE_RGB:
			if ( SGI(ifp)->si_cmap_flag == FALSE )  {
				for( i=scan_count; i > 0; )  {
					register short chunk;

					if( i <= (127/3) )
						chunk = i;
					else
						chunk = 127/3;
					PASSCMD(hole, chunk*3, FBCdrawpixels);
					i -= chunk;
					for( ; chunk>0; chunk--)  {
						hole->us = (*op++ = *cp++);
						hole->us = (*op++ = *cp++);
						hole->us = (*op++ = *cp++);
					}
				}
			} else {
				for( i=scan_count; i > 0; )  {
					register short chunk;

					if( i <= (127/3) )
						chunk = i;
					else
						chunk = 127/3;
					PASSCMD(hole, chunk*3, FBCdrawpixels);
					i -= chunk;
					for( ; chunk>0; chunk-- )  {
						hole->s = CMR(ifp)[
							*op++ = *cp++];
						hole->s = CMG(ifp)[
							*op++ = *cp++];
						hole->s = CMB(ifp)[
							*op++ = *cp++];
					}
				}
			}
			break;
		case MODE_FIT:
			if ( SGI(ifp)->si_cmap_flag == FALSE )  {
				for( i = scan_count; i > 0; )  {
					register short	chunk;
					if( i <= 127 )
						chunk = i;
					else
						chunk = 127;
					PASSCMD(hole, chunk, FBCdrawpixels);
					i -= chunk;
					for( ; chunk > 0; chunk--, pixelp++ )  {
						hole->s = get_Color_Index( ifp, pixelp );
						*op++ = (*pixelp)[RED];
						*op++ = (*pixelp)[GRN];
						*op++ = (*pixelp)[BLU];
					}
				}
			} else {
				for( i = scan_count; i > 0; )  {
					register short	chunk;
					if( i <= 127 )
						chunk = i;
					else
						chunk = 127;
					PASSCMD(hole, chunk, FBCdrawpixels);
					i -= chunk;
					for( ; chunk > 0; chunk--, pixelp++ )  {
						static RGBpixel new;
						new[RED] = CMR(ifp)[
							*op++ = (*pixelp)[RED]];
						new[GRN] = CMG(ifp)[
							*op++ = (*pixelp)[GRN]];
						new[BLU] = CMB(ifp)[
							*op++ = (*pixelp)[BLU]];
						hole->s = get_Color_Index(ifp,new);
					}
				}
			}
		case MODE_APPROX:
			if ( SGI(ifp)->si_cmap_flag == FALSE )  {
				for( i = scan_count; i > 0; )  {
					register short	chunk;
					if( i <= 127 )
						chunk = i;
					else
						chunk = 127;
					PASSCMD(hole, chunk, FBCdrawpixels);
					i -= chunk;
					for( ; chunk > 0; chunk--, pixelp++ )  {
						hole->s = COLOR_APPROX(*pixelp);
						*op++ = (*pixelp)[RED];
						*op++ = (*pixelp)[GRN];
						*op++ = (*pixelp)[BLU];
					}
				}
			} else {
				for( i = scan_count; i > 0; )  {
					register short	chunk;
					if( i <= 127 )
						chunk = i;
					else
						chunk = 127;
					PASSCMD(hole, chunk, FBCdrawpixels);
					i -= chunk;
					for( ; chunk > 0; chunk--, pixelp++ )  {
						static RGBpixel new;
						new[RED] = CMR(ifp)[
							*op++ = (*pixelp)[RED]];
						new[GRN] = CMG(ifp)[
							*op++ = (*pixelp)[GRN]];
						new[BLU] = CMB(ifp)[
							*op++ = (*pixelp)[BLU]];
						hole->s = COLOR_APPROX(new);
					}
				}
			}
		}

		count -= scan_count;
		ret += scan_count;
		xmem = 0;
		ymem++;
		xscr = ifp->if_xcenter - hfwidth;
		yscr++;
	}
	GEP_END(hole)->s = (0xFF<<8)|8;	/* im_last_passthru(0) */
	if( SGI(ifp)->si_curs_on )
		curson();		/* Cursor interferes with writing */
	return(ret);
}

/*
 *			S G I _ C M R E A D
 */
HIDDEN int
sgi_rmap( ifp, cmp )
register FBIO	*ifp;
register ColorMap	*cmp;
{
	register int i;

	if( qtest() )
		sgw_inqueue(ifp);

	/* Just parrot back the stored colormap */
	for( i = 0; i < 256; i++)
	{
		cmp->cm_red[i] = CMR(ifp)[i]<<8;
		cmp->cm_green[i] = CMG(ifp)[i]<<8;
		cmp->cm_blue[i] = CMB(ifp)[i]<<8;
	}
	return(0);
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

	for( i=0; i<256; i++ )  {
		if( CMR(ifp)[i] != i )  return(0);
		if( CMG(ifp)[i] != i )  return(0);
		if( CMB(ifp)[i] != i )  return(0);
	}
	return(1);
}

/*
 *			 S G I _ C M W R I T E
 */
HIDDEN int
sgi_wmap( ifp, cmp )
register FBIO	*ifp;
register ColorMap	*cmp;
{
	register int i;

	if( qtest() )
		sgw_inqueue(ifp);

	if ( cmp == COLORMAP_NULL)  {
		for( i = 0; i < 256; i++)  {
			CMR(ifp)[i] = i;
			CMG(ifp)[i] = i;
			CMB(ifp)[i] = i;
		}
		if( SGI(ifp)->si_cmap_flag ) {
			SGI(ifp)->si_cmap_flag = FALSE;
			sgi_repaint( ifp );
		}
		SGI(ifp)->si_cmap_flag = FALSE;
		return(0);
	}

	for(i = 0; i < 256; i++)  {
		CMR(ifp)[i] = cmp-> cm_red[i]>>8;
		CMG(ifp)[i] = cmp-> cm_green[i]>>8;
		CMB(ifp)[i] = cmp-> cm_blue[i]>>8;

	}
	SGI(ifp)->si_cmap_flag = !is_linear_cmap(ifp);
	sgi_repaint( ifp );
	return(0);
}

/*
 *			S G I _ C U R S _ S E T
 */
HIDDEN int
sgi_setcursor( ifp, bits, xbits, ybits, xorig, yorig )
FBIO	*ifp;
unsigned char	*bits;
int		xbits, ybits;
int		xorig, yorig;
{
	register int	y;
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
	for( y = 0; y < ybits; y++ )  {
		newcursor[y] = bits[(y*xbytes)+0] << 8 & 0xFF00;
		if( xbytes == 2 )
			newcursor[y] |= bits[(y*xbytes)+1] & 0x00FF;
	}
	defcursor( 1, newcursor );
	curorigin( 1, (short) xorig, (short) yorig );
	return	0;
}

/*
 *			S G I _ C M E M O R Y _ A D D R
 */
HIDDEN int
sgi_cursor( ifp, mode, x, y )
FBIO	*ifp;
int	mode;
int	x, y;
{
	short	xmin, ymin;
	register short	i;
	short	xwidth;

	if( qtest() )
		sgw_inqueue(ifp);

	fb_sim_cursor(ifp,mode,x,y);
	SGI(ifp)->si_curs_on = mode;
	if( ! mode )  {
		cursoff();
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
	curson();
	switch( ifp->if_mode )  {
	case MODE_RGB:
		RGBcursor( 1, 255, 255, 0, 0xFF, 0xFF, 0xFF );
		setvaluator( MOUSEX, x, 0, 1023 );
		setvaluator( MOUSEY, y, 0, 1023 );
		break;
	case MODE_APPROX:
	case MODE_FIT:
		{
			long	xwin, ywin;

		/* Color and bitmask ignored under MEX.	*/
		setcursor( 1, YELLOW, 0x2000 );
		getorigin( &xwin, &ywin );
		setvaluator( MOUSEX, (short)(x+xwin), 0, 1023 );
		setvaluator( MOUSEY, (short)(y+ywin), 0, 1023 );
		break;
		}
	}
	return	0;
}

/*
 *			g e t _ C o l o r _ I n d e x
 */
HIDDEN Colorindex
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
	for(	i = MAP_RESERVED, sp = (RGBpixel *)rgb_table[MAP_RESERVED];
		i < SGI(ifp)->si_rgb_ct;
		sp++, i++ ) {
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


/*
 *			S G W _ D O P E N
 */
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

	ifp->if_mode = MODE_APPROX;
	if( file != NULL )  {
		register char *cp;
		int mode;
		/* "/dev/sgi###" gives optional mode */
		for( cp = file; *cp != '\0' && !isdigit(*cp); cp++ ) ;
		mode = MODE_APPROX;
		if( isdigit(*cp) )
			(void)sscanf( cp, "%d", &mode );
		if( mode >= 99 )  {
			/* Attempt to release shared memory segment */
			sgi_zapmem();
			return(-1);
		}
		if( mode < MODE_APPROX || mode > MODE_FIT )
			mode = MODE_APPROX;
		ifp->if_mode = mode;
	}

	/* By default, pop up a 512x512 MEX window, rather than fullsize */
	if( width <= 0 )
		width = 512;
	if( height <= 0 )
		height = 512;
	if ( width > ifp->if_max_width)
		width = ifp->if_max_width;
	if ( height > ifp->if_max_height - 2 * MARGIN - BANNER)
		height = ifp->if_max_height - 2 * MARGIN - BANNER;

	ifp->if_width = width;
	ifp->if_height = height;

	if( ismex() )  {
		prefposition( WIN_L, WIN_R, WIN_B, WIN_T );
		foreground();		/* Direct focus here, don't detach */
		if( (ifp->if_fd = winopen( "Frame buffer" )) == -1 )
			{
			fb_log( "No more graphics ports available.\n" );
			return	-1;
			}
		wintitle( "BRL libfb Frame Buffer" );
		/* Free window of position constraint.		*/
		prefsize( (long)ifp->if_width, (long)ifp->if_height );
		winconstraints();
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

	/* Must initialize these window state variables BEFORE calling
		"sgi_getmem", because this function can indirectly trigger
		a call to "sgi_repaint" (when initializing shared memory
		after a reboot).					*/
	ifp->if_zoomflag = 0;
	ifp->if_xzoom = 1;	/* for zoom fakeout */
	ifp->if_yzoom = 1;	/* for zoom fakeout */
	ifp->if_xcenter = width/2;
	ifp->if_ycenter = height/2;

	if( sgi_getmem(ifp) < 0 )
		return(-1);

	/* Must call "is_linear_cmap" AFTER "sgi_getmem" which allocates
		space for the color map.				*/
	SGI(ifp)->si_cmap_flag = !is_linear_cmap(ifp);

	/*
	 * Deal with the SGI hardware color map
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
	if( ifp->if_mode == MODE_APPROX )  {
		/* Use fixed color map with 10x10x10 color cube */
		for( i = 0; i < map_size-MAP_RESERVED; i++ )
			mapcolor( 	i+MAP_RESERVED,
					(short)((i % 10) + 1) * 25,
					(short)(((i / 10) % 10) + 1) * 25,
					(short)((i / 100) + 1) * 25
					);
	}

	/* Setup default cursor.					*/
	defcursor( 1, cursor );
	curorigin( 1, 0, 0 );

	/* The screen has no useful state.  Restore it as it was before */
	/* SMART deferral logic needed */
	sgi_repaint( ifp );
	return	0;
}

/*
 *			S G W _ I N Q U E U E
 *
 *  Called when a qtest() indicates that there is a window event.
 *  Process all events, so that we don't loop on recursion to sgw_bwrite.
 */
HIDDEN void
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
		case MOUSEX :
		case MOUSEY :
		case KEYBD :
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

HIDDEN void
sgi_rectf_pix( ifp, l, b, r, t, pixelp )
FBIO		*ifp;
Scoord		l, b, r, t;
RGBpixel	*pixelp;
{
	register union gepipe *hole = GEPIPE;

	switch( ifp->if_mode ) {
	case MODE_RGB:
		PASSCMD(hole,3,FBCrgbcolor);
		hole->s = (*pixelp)[RED];
		hole->s = (*pixelp)[GRN];
		hole->s = (*pixelp)[BLU];
		break;
	case MODE_FIT:
		PASSCMD(hole,1,FBCcolor);
		hole->s = get_Color_Index( ifp, pixelp );
		break;
	case MODE_APPROX:
		PASSCMD(hole,1,FBCcolor);
		hole->s = COLOR_APPROX(*pixelp);
		break;
	}
	/* left bottom right top: rectfs( l, b, r, t ); */
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
	return;
}

HIDDEN int
sgi_help( ifp )
FBIO	*ifp;
{
	fb_log( "Description: %s\n", sgi_interface.if_type );
	fb_log( "Device: %s\n", ifp->if_name );
	fb_log( "Max width/height: %d %d\n",
		sgi_interface.if_max_width,
		sgi_interface.if_max_height );
	fb_log( "Default width/height: %d %d\n",
		sgi_interface.if_width,
		sgi_interface.if_height );
	fb_log( "\
Usage: /dev/sgi[#]\n\
  where # is a optional number from:\n\
    0    fixed color cube approximation (default)\n\
    1    dynamic colormap entry allocation (slower)\n\
   99    release shared memory\n" );

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
