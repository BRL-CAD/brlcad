/*                         I F _ 4 D . C
 * BRL-CAD
 *
 * Copyright (C) 1989-2005 United States Government as represented by
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
/** @file if_4d.c
 *  BRL-CAD Frame Buffer Library interface for SGI Iris-4D
 *  running the IRIX32 operating system with the GL graphics library.
 *  Support for the 3030/2400 series ("Iris-3D") is in if_sgi.c
 *  However, both are called /dev/sgi
 *  Support for OpenGL is found in if_ogl.c
 *
 *  In order to use a large chunck of memory with the shared memory
 *  system it is necessary to increase the shmmax and shmall paramaters
 *  of the system. You can do this by changing the defaults in the
 *  /usr/sysgen/master.d/shm to
 *
 * 	#define SHMMAX	5131072
 *	#define SHMALL	4000
 *
 *  and rebooting your system twice.
 *
 *  Refer to the SGI Users Manuals when reconfiguring your kernel.
 *
 *  There are several different Frame Buffer modes supported.
 *  Set your environment FB_FILE to the appropriate type.
 *  (see the modeflag definitions below).
 *	/dev/sgi[options]
 *
 *  Authors -
 *	Michael John Muuss
 *	Paul R. Stay
 *	Gary S. Moss
 *
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 */
/*@}*/

#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#define class	inv_class	/* Map Irix 4 name into Irix 5 name */
#define type	inv_type	/* Map Irix 4 name into Irix 5 name */

#include "common.h"



#include <stdio.h>
#include <ctype.h>
#include <gl.h>
#include <get.h>
#include <device.h>
#include <sys/invent.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <signal.h>
#include <errno.h>
#undef RED
#include <gl/addrs.h>
#include <gl/cg2vme.h>

#include "machine.h"
#include "fb.h"
#include "./fblocal.h"

extern inventory_t	*getinvent();

static Cursor	nilcursor;	/* to make it go away -- all bits off */
static Cursor	cursor =  {
#include "./sgicursor.h"
 };

/*
 *  One global state variable to record number of different windows
 *  that *this process* has opened.
 *  Needed to prevent re-running ginit(), and avoiding prefposition().
 */
static int	sgi_nwindows = 0;

/* Internal routines */
_LOCAL_ void	sgi_cminit();
#ifdef USE_PROTOTYPES
_LOCAL_ void	gt_zbuf_to_screen(FBIO	*ifp, int one_y);
#else
_LOCAL_ void	gt_zbuf_to_screen();
#endif
_LOCAL_ void	fake_rectwrite();
_LOCAL_ void	sgi_clipper();

/* Exported routines */
_LOCAL_ int	sgi_open(),
		sgi_close(),
		sgi_clear(),
		sgi_read(),
		sgi_write(),
		sgi_rmap(),
		sgi_wmap(),
		sgi_view(),
		sgi_getview(),
		sgi_setcursor(),
		sgi_cursor(),
		sgi_writerect(),
		sgi_poll(),
		sgi_free(),
		sgi_flush(),
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
		sgi_getview,
		sgi_setcursor,
		sgi_cursor,
		fb_sim_getcursor,
		fb_sim_readrect,
		sgi_writerect,
		fb_sim_bwreadrect,
		fb_sim_bwwriterect,
		sgi_poll,		/* handle events */
		sgi_flush,		/* flush */
		sgi_free,		/* free*/
		sgi_help,
		"Silicon Graphics Iris '4D'",
		XMAXSCREEN+1,		/* max width */
		YMAXSCREEN+1,		/* max height */
		"/dev/sgi",
		512,			/* current/default width  */
		512,			/* current/default height */
		-1,			/* select fd */
		-1,			/* file descriptor */
		1, 1,			/* zoom */
		256, 256,		/* window */
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


_LOCAL_ void	sgi_inqueue();
static int	is_linear_cmap();

/*
 *  Structure of color map in shared memory region.
 *  Has exactly the same format as the SGI hardware "gammaramp" map
 *  Note that only the lower 8 bits are significant.
 */
struct sgi_cmap {
	short	cmr[256];
	short	cmg[256];
	short	cmb[256];
};

/*
 *  This is the format of the buffer for lrectwrite(),
 *  and thus defines the format of the in-memory framebuffer copy.
 */
struct sgi_pixel {
	unsigned char alpha;	/* this will always be zero */
	unsigned char blue;
	unsigned char green;
	unsigned char red;
};

/*
 *  Per window state information, overflow area.
 *  Too much for just the if_u[1-6] area now.
 */
struct sgiinfo {
	short	mi_curs_on;
	short	mi_cmap_flag;
	int	mi_shmid;
	int	mi_memwidth;		/* width of scanline in if_mem */
	long	mi_der1;		/* Saved DE_R1 */
	short	mi_xoff;		/* X viewport offset, rel. window */
	short	mi_yoff;		/* Y viewport offset, rel. window */
	short	mi_is_gt;		/* !0 when using GT hardware */
	int	mi_pid;			/* for multi-cpu check */
	int	mi_parent;		/* PID of linger-mode process */
	struct sgi_pixel mi_scanline[XMAXSCREEN+1];	/* one scanline */
	int	mi_doublebuffer;	/* 0=singlebuffer 1=doublebuffer */
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

#define MARGIN	4			/* # pixels margin to screen edge */
#define BANNER	18			/* Size of MEX title banner */
#define WIN_L	(ifp->if_max_width - ifp->if_width-MARGIN)
#define WIN_R	(ifp->if_max_width - 1 - MARGIN)
#define WIN_B	MARGIN
#define WIN_T	(ifp->if_height - 1 + MARGIN)

/*
 *  The mode has several independent bits:
 *	SHARED -vs- MALLOC'ed memory for the image
 *	TRANSIENT -vs- LINGERING windows
 *	Windowed -vs- Centered Full screen
 *	Default Hz -vs- 30hz monitor mode
 *	NTSC -vs- normal monitor mode
 */
#define MODE_1MASK	(1<<0)
#define MODE_1SHARED	(0<<0)		/* Use Shared memory */
#define MODE_1MALLOC	(1<<0)		/* Use malloc memory */

#define MODE_2MASK	(1<<1)
#define MODE_2TRANSIENT	(0<<1)
#define MODE_2LINGERING (1<<1)

#define MODE_3MASK	(1<<2)
#define MODE_3WINDOW	(0<<2)
#define MODE_3FULLSCR	(1<<2)

#define MODE_4MASK	(1<<3)
#define MODE_4HZDEF	(0<<3)
#define MODE_4HZ30	(1<<3)

#define MODE_5MASK	(1<<4)
#define MODE_5NORMAL	(0<<4)
#define MODE_5NTSC	(1<<4)

#define MODE_6MASK	(1<<5)
#define MODE_6NORMAL	(0<<5)
#define MODE_6EXTSYNC	(1<<5)

#define MODE_7MASK	(1<<6)
#define MODE_7NORMAL	(0<<6)
#define MODE_7SWCMAP	(1<<6)

#define MODE_8MASK	(1<<7)
#define MODE_8NORMAL	(0<<7)
#define MODE_8NOGT	(1<<7)

#define MODE_9MASK	(1<<8)
#define MODE_9NORMAL	(0<<8)
#define MODE_9SINGLEBUF	(1<<8)

#define MODE_10MASK	(1<<9)
#define MODE_10NORMAL	(0<<9)
#define MODE_10SYNC_ON_GREEN	(1<<9)

#define MODE_12MASK	(1<<11)
#define MODE_12NORMAL	(0<<11)
#define MODE_12DELAY_WRITES_TILL_FLUSH	(1<<11)

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
	{ 'f',	MODE_3MASK, MODE_3FULLSCR,
		"Full centered screen - else windowed" },
	{ 't',	MODE_4MASK, MODE_4HZ30,
		"Thirty Hz (e.g. Dunn) - else 60 Hz" },
	{ 'n',	MODE_5MASK, MODE_5NTSC,
		"NTSC - else normal video.  GENLOCK board activated if present." },
	{ 'g',	MODE_10MASK, MODE_10SYNC_ON_GREEN,
		"For NTSC & 30Hz, put sync on Green - else use 4th connector" },
	{ 'e',	MODE_6MASK, MODE_6EXTSYNC,
		"External sync - else internal.  Requires CG2 board." },
	{ 'c',	MODE_7MASK, MODE_7SWCMAP,
		"Perform software colormap - else use hardware colormap on whole screen" },
	{ 'G',	MODE_8MASK, MODE_8NOGT,
		"Don't use GT & Z-buffer hardware, if present (debug)" },
	{ 's',	MODE_9MASK, MODE_9SINGLEBUF,
		"On GT, single buffer, don't double buffer" },
	{ 'D',	MODE_12DELAY_WRITES_TILL_FLUSH, MODE_12DELAY_WRITES_TILL_FLUSH,
		"Don't update screen until fb_flush() is called.  (Double buffer sim)" },
	{ 'z',	MODE_15MASK, MODE_15ZAP,
		"Zap (free) shared memory.  Can also be done with fbfree command" },
	{ '\0', 0, 0, "" }
};

/* Clipping structure, for zoom/pan operations on non-GT systems */
struct sgi_clip {
	int	xmin;
	int	xmax;
	int	ymin;
	int	ymax;
	int	xscroff;
	int	yscroff;
	int	xscrpad;
	int	yscrpad;
};

/************************************************************************/
/************************************************************************/
/************************************************************************/
/******************* Shared Memory Support ******************************/
/************************************************************************/
/************************************************************************/
/************************************************************************/

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
_LOCAL_ int
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

	if( sizeof(struct sgi_pixel) != sizeof(long) )  {
		fb_log("Warning, sgi_pixel=%d, long=%d bytes\n",
			sizeof(struct sgi_pixel), sizeof(long) );
	}
	errno = 0;

	if( (ifp->if_mode & MODE_1MASK) == MODE_1MALLOC )  {
		/*
		 *  In this mode, only malloc as much memory as is needed.
		 */
		SGI(ifp)->mi_memwidth = ifp->if_width;
		pixsize = ifp->if_height * ifp->if_width * sizeof(struct sgi_pixel);
		size = pixsize + sizeof(struct sgi_cmap);

		sp = malloc( size );
		if( sp == 0 )  {
			fb_log("sgi_getmem: frame buffer memory malloc failed\n");
			goto fail;
		}
		new = 1;
		goto success;
	}

	/* The shared memory section never changes size */
	SGI(ifp)->mi_memwidth = ifp->if_max_width;

	/*
	 *  On Irix 5 with Indigo EXPRESS graphics,
	 *  lrectwrite() runs off the end!
	 *  So, provide a pad area of 2 scanlines.
	 *  (1 line is enough, but this avoids risk of damage to colormap table.)
	 */
	pixsize = (ifp->if_max_height+2) * ifp->if_max_width *
		sizeof(struct sgi_pixel);

	size = pixsize + sizeof(struct sgi_cmap);
	size = (size + getpagesize()-1) & ~(getpagesize()-1);

	/* First try to attach to an existing one */
	if( (SGI(ifp)->mi_shmid = shmget( SHMEM_KEY, size, 0 )) < 0 )  {
		/* No existing one, create a new one */
		if( (SGI(ifp)->mi_shmid = shmget(
		    SHMEM_KEY, size, IPC_CREAT|0666 )) < 0 )  {
			fb_log("sgi_getmem: shmget failed, errno=%d\n", errno);
			goto fail;
		}
		new = 1;
	}

#if defined (IRIX) && IRIX < 5
	/* Move up the existing break, to leave room for later malloc()s */
	old_brk = sbrk(0);
	new_brk = (char *)(6 * (XMAXSCREEN+1) * 1024L);
	if( new_brk <= old_brk )
		new_brk = old_brk + (XMAXSCREEN+1) * 1024;
	new_brk = (char *)((((long)new_brk) + getpagesize()-1) & ~(getpagesize()-1));
	if( brk( new_brk ) < 0 )  {
		fb_log("sgi_getmem: new brk(x%x) failure, errno=%d\n", new_brk, errno);
		goto fail;
	}

	/* Open the segment Read/Write, near the current break */
	if( (sp = shmat( SGI(ifp)->mi_shmid, 0, 0 )) == (char *)(-1L) )  {
		fb_log("sgi_getmem: shmat returned x%x, errno=%d\n", sp, errno );
		goto fail;
	}

	/* Restore the old break */
	if( brk( old_brk ) < 0 )  {
		fb_log("sgi_getmem: restore brk(x%x) failure, errno=%d\n", old_brk, errno);
		/* Take the memory and run */
	}
#else
	/* Open the segment Read/Write */
	/* On Irix 5, this gets mapped in at a high address, no problem. */
	if( (sp = shmat( SGI(ifp)->mi_shmid, 0, 0 )) == (char *)(-1L) )  {
		fb_log("sgi_getmem: shmat returned x%x, errno=%d\n", sp, errno );
		goto fail;
	}
#endif

success:
	ifp->if_mem = sp;
	ifp->if_cmap = sp + pixsize;	/* cmap at end of area */
	i = CMB(ifp)[255];		/* try to deref last word */
	CMB(ifp)[255] = i;

	/* Provide non-black colormap on creation of new shared mem */
	if(new)
		sgi_cminit( ifp );
	return(0);
fail:
	fb_log("sgi_getmem:  Unable to attach to shared memory.\nConsult comment in cad/libfb/if_4d.c for details\n");
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

/************************************************************************/

/*
 *			S G I _ X M I T _ S C A N L I N E S
 *
 *  Given the current window center, and the current zoom,
 *  transmit scanlines from the shared memory buffer.
 *
 *  On a non-GT, this routine copies scanlines from shared memory
 *  directly to the screen (window).  Zooming is done here.
 *
 *  On a GT machine, this routine copies scanlines from shared memory
 *  into the Z-buffer.
 *  The full image is stored in the Z-buffer.
 *  A separate routine is used to update some portion of the
 *  window from the Z-buffer copy, permitting high-speed pan and zoom.
 *
 *  Note that lrectwrite() addresses are relative to the pixel coordinates of
 *  the window, not the current viewport, as you might expect!
 *
 *  Normally, xbase=0 and npix = if_width.  For transmissions of
 *  less than one scanline, these may be different.
 */
_LOCAL_ void
sgi_xmit_scanlines( ifp, ybase, nlines, xbase, npix )
register FBIO	*ifp;
int		ybase;
int		nlines;
int		xbase;
int		npix;
{
	register int	y;
	register int	n;
	struct sgi_clip	clip;
	int		sw_cmap;	/* !0 => needs software color map */
	int		sw_zoom;	/* !0 => needs software zoom/pan */

	if( nlines > 1 )  {
		xbase = 0;
		npix = ifp->if_width;
	}

	winset(ifp->if_fd);

	if( SGI(ifp)->mi_is_gt )  {
		/* Enable writing pixels into the Z buffer */
		/* Always send full unzoomed image into Z buffer */
		rectzoom( 1.0, 1.0 );
		zbuffer(FALSE);
		zdraw(TRUE);
		backbuffer(FALSE);
		frontbuffer(FALSE);

		clip.xmin = clip.ymin = 0;
		clip.xscroff = clip.yscroff = 0;
		clip.xmax = ifp->if_width-1;
		clip.ymax = ifp->if_height-1;

		sw_zoom = 0;
	} else {
		/* Single buffered, direct to screen */
		if( ifp->if_zoomflag )  {
			sw_zoom = 1;
		} else {
			sw_zoom = 0;
		}
		if( ifp->if_xcenter != ifp->if_width/2 ||
		    ifp->if_ycenter != ifp->if_height/2 )  {
		    	sw_zoom = 1;
		}
		sgi_clipper( ifp, &clip );
	}

	if( (ifp->if_mode & MODE_7MASK) == MODE_7SWCMAP  &&
	    SGI(ifp)->mi_cmap_flag )  {
	    	sw_cmap = 1;
	} else {
		sw_cmap = 0;
	}

	/* Simplest case, nothing fancy */
	y = ybase;
	if( !sw_zoom && !sw_cmap )  {
		if( nlines == 1 )  {
			/*
			 *  Only a single line is being written,
			 *  and perhaps only part of the line width.
			 *  Just send it off.
			 */
			if( SGI(ifp)->mi_is_gt)
				lrectwrite(
					SGI(ifp)->mi_xoff+xbase,
					SGI(ifp)->mi_yoff+y,
					SGI(ifp)->mi_xoff+xbase+npix-1,
					SGI(ifp)->mi_yoff+y,
					(unsigned long *) &ifp->if_mem[
					    (y*SGI(ifp)->mi_memwidth+xbase)*
					    sizeof(struct sgi_pixel)] );
			else
				fake_rectwrite(
					xbase,
					y,
					xbase+npix-1,
					y,
					&ifp->if_mem[(y*SGI(ifp)->mi_memwidth+xbase)*
					    sizeof(struct sgi_pixel)] );
			return;
		}
		/*
		 *  If lrectwrite() is asked to write scanlines wider
		 *  than a certain amount, it uses a sys-write call,
		 *  otherwise, it sends the pixels via programmed I/O.
		 *  Thus, if the width being sent is more than this
		 *  threshold, it is better just to make one call to
		 *  lrectwrite, and let the DMA send more data.
		 *  But, if a thin vertical strip of pixels is being
		 *  written, it makes more sense to call lrectwrite
		 *  once for each scanline, and the the PIO zap it off.
		 *  Alas, SGI never documents the threshold value;
		 *  the one here is pure guesswork.
		 *  Also note that on a VGX machine this might be
		 *  better accomplished with pixmode(PM_STRIDE,wds/scanline),
		 *  but no earlier models have this subroutine, sigh.
		 */
		if( npix > 32 && ifp->if_width == SGI(ifp)->mi_memwidth)  {
			/* Multiple line case, wide lines, matching sizes,
			 * send full lines
			 */
			if( SGI(ifp)->mi_is_gt)
				lrectwrite(
					SGI(ifp)->mi_xoff+0,
					SGI(ifp)->mi_yoff+y,
					SGI(ifp)->mi_xoff+0+ifp->if_width-1,
					SGI(ifp)->mi_yoff+y+nlines-1,
					(unsigned long *) &ifp->if_mem[
						(y*SGI(ifp)->mi_memwidth)*
						sizeof(struct sgi_pixel)] );
			else
				fake_rectwrite(
					0,
					y,
					0+ifp->if_width-1,
					y+nlines-1,
					&ifp->if_mem[(y*SGI(ifp)->mi_memwidth)*
					    sizeof(struct sgi_pixel)] );
			return;
		}
		/* Narrow width lines case */
		for( n=nlines; n>0; n--, y++ )  {
			/*
			 *  GTX is limited to about 1000 lrectwrites/sec,
			 *  when sending wide spans,
			 *  due to the fact that 1-scanline lrectwrites
			 *  are done with DMA.  The sys-call & interrupt
			 *  processing burns 60% of the CPU in sys-time!
			 */
			if( SGI(ifp)->mi_is_gt )
				lrectwrite(
					SGI(ifp)->mi_xoff+xbase,
					SGI(ifp)->mi_yoff+y,
					SGI(ifp)->mi_xoff+xbase+npix-1,
					SGI(ifp)->mi_yoff+y,
					(unsigned long *) &ifp->if_mem[
						(y*SGI(ifp)->mi_memwidth+xbase)*
						sizeof(struct sgi_pixel)] );
			else
				fake_rectwrite(
					xbase,
					y,
					xbase+npix-1,
					y,
					&ifp->if_mem[(y*SGI(ifp)->mi_memwidth+xbase)*
					    sizeof(struct sgi_pixel)] );
			xbase = 0;
			npix = ifp->if_width;
		}
		return;
	}

	if( !sw_zoom && sw_cmap )  {
		register int	x;
		register struct sgi_pixel	*sgip;
		register struct sgi_pixel	*op;

		/* Perform software color mapping into temp scanline */
		op = SGI(ifp)->mi_scanline;
		for( n=nlines; n>0; n--, y++ )  {
			sgip = (struct sgi_pixel *)&ifp->if_mem[
				(y*SGI(ifp)->mi_memwidth)*
				sizeof(struct sgi_pixel) ];
			for( x=ifp->if_width-1; x>=0; x-- )  {
				op[x].red   = CMR(ifp)[sgip[x].red];
				op[x].green = CMG(ifp)[sgip[x].green];
				op[x].blue  = CMB(ifp)[sgip[x].blue];
			}
			if( SGI(ifp)->mi_is_gt )
				lrectwrite(
					SGI(ifp)->mi_xoff+0,
					SGI(ifp)->mi_yoff+y,
					SGI(ifp)->mi_xoff+0+ifp->if_width-1,
					SGI(ifp)->mi_yoff+y,
					(unsigned long *) op );
			else
				fake_rectwrite(
					0,
					y,
					0+ifp->if_width-1,
					y,
					op );
		}
		return;
	}

	/*
	 *  All code below is to handle software zooming on non-GT machines.
	 */

	/* Blank out area left of image */
	RGBcolor( 0, 0, 0 );
	if( clip.xscroff > 0 )  rectfs(
		0,
		0,
		(Scoord) clip.xscroff-1,
		(Scoord) ifp->if_height-1 );

	/* Blank out area below image */
	if( clip.yscroff > 0 )  rectfs(
		0,
		0,
		(Scoord) ifp->if_width-1,
		(Scoord) clip.yscroff-1 );

	/* Blank out area right of image */
	if( clip.xscrpad > 0 )  rectfs(
		(Scoord) ifp->if_width-clip.xscrpad-1,
		0,
		(Scoord) ifp->if_width-1,
		(Scoord) ifp->if_height-1 );

	/* Blank out area above image */
	if( clip.yscrpad > 0 )  rectfs(
		0,
		(Scoord) ifp->if_height-clip.yscrpad-1,
		(Scoord) ifp->if_width-1,
		(Scoord) ifp->if_height-1 );

	/* Output pixels in zoomed condition */
	{
		register int	rep;
		register int	yscr;
		register int	xscrmin, xscrmax;
		register struct sgi_pixel	*sgip, *op;

		/*
		 *  From memory starting at (xmin, ymin) to
		 *  screen starting at (xscroff, yscroff).
		 *  Memory addresses increment by 1.
		 *  Screen addresses increment by mi_?zoom.
		 */
		yscr = SGI(ifp)->mi_yoff + clip.yscroff + y * ifp->if_yzoom;
		xscrmin = SGI(ifp)->mi_xoff+clip.xscroff;
		xscrmax = SGI(ifp)->mi_xoff+ifp->if_width-1-clip.xscrpad;
		for( n=nlines; n>0; n--, y++ )  {
			register int	x;

			if( y < clip.ymin )  continue;
			if( y > clip.ymax )  continue;

			/* widen this line */
			sgip = (struct sgi_pixel *)&ifp->if_mem[
				(y*SGI(ifp)->mi_memwidth+clip.xmin)*
				sizeof(struct sgi_pixel)];
			op = SGI(ifp)->mi_scanline;
			for( x=clip.xmin; x<=clip.xmax; x++ )  {
				for( rep=0; rep<ifp->if_xzoom; rep++ )  {
					*op++ = *sgip;	/* struct copy */
				}
				sgip++;
				}
			if( sw_cmap )  {
				x=(clip.xmax-clip.xmin)*ifp->if_xzoom;
				op = SGI(ifp)->mi_scanline;
				for( ; x>=0; x-- )  {
					op[x].red   = CMR(ifp)[op[x].red];
					op[x].green = CMG(ifp)[op[x].green];
					op[x].blue  = CMB(ifp)[op[x].blue];
				}
			}
			/* X direction replication is handled above,
			 * Y direction replication is done by this loop.
			 */
			for( rep=0; rep<ifp->if_yzoom; rep++ )  {
				if( SGI(ifp)->mi_is_gt )
					lrectwrite(
						xscrmin, yscr,
						xscrmax, yscr,
						(unsigned long *)
							SGI(ifp)->mi_scanline );
				else
					fake_rectwrite(
						xscrmin - SGI(ifp)->mi_xoff,
						yscr - SGI(ifp)->mi_yoff,
						xscrmax - SGI(ifp)->mi_xoff,
						yscr - SGI(ifp)->mi_yoff,
						SGI(ifp)->mi_scanline );
				yscr++;
			}
		}
		return;
	}
}

/*
 *			S I G K I D
 */
static void sigkid()
{
	exit(0);
}

/************************************************************************/
/************************************************************************/
/************************************************************************/
/************** Routines to implement the libfb interface ***************/
/************************************************************************/
/************************************************************************/
/************************************************************************/

/*
 *			S G I _ O P E N
 */
_LOCAL_ int
sgi_open( ifp, file, width, height )
FBIO	*ifp;
char	*file;
int	width, height;
{
	int	f;
	int	status;
	static char	title[128];
	int		mode;
	inventory_t	*inv;

	FB_CK_FBIO(ifp);

	/*
	 *  First, attempt to determine operating mode for this open,
	 *  based upon the "unit number" or flags.
	 *  file = "/dev/sgi###"
	 *  The default mode is zero.
	 */
	mode = 0;

	if( file != NULL )  {
		register char *cp;
		char	modebuf[80];
		char	*mp;
		int	alpha;
		struct	modeflags *mfp;

		if( strncmp(file, "/dev/sgi", 8) ) {
			/* How did this happen?? */
			mode = 0;
		} else {
			/* Parse the options */
			alpha = 0;
			mp = &modebuf[0];
			cp = &file[8];
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
					fb_log( "if_4d: unknown option '%c' ignored\n", *cp );
				}
				cp++;
			}
			*mp = '\0';
			if( !alpha )
				mode = atoi( modebuf );
		}

		if( (mode & MODE_15MASK) == MODE_15ZAP ) {
			/* Only task: Attempt to release shared memory segment */
			sgi_zapmem();
			return(-1);
		}
	}
	ifp->if_mode = mode;

	/*
	 *  Allocate extension memory section,
	 *  addressed by SGI(ifp)->mi_xxx
	 */
	if( (SGIL(ifp) = (char *)calloc( 1, sizeof(struct sgiinfo) )) == NULL )  {
		fb_log("sgi_open:  sgiinfo malloc failed\n");
		return(-1);
	}
	SGI(ifp)->mi_shmid = -1;	/* indicate no shared memory */

	/* the Silicon Graphics Library Window management routines
	 * use shared memory. This causes lots of problems when you
	 * want to pass a window structure to a child process.
	 * One hack to get around this is to immediately fork
	 * and create a child process and sleep until the child
	 * sends a kill signal to the parent process. (in FBCLOSE)
	 * This allows us to use the traditional fb utility programs
	 * as well as allow the frame buffer window to remain around
	 * until killed by the menu subsystem.
    	 */
	if( (ifp->if_mode & MODE_2MASK) == MODE_2LINGERING )  {
		/* save parent pid for later signalling */
		SGI(ifp)->mi_parent = getpid();

		signal( SIGUSR1, sigkid);

		if( (f = fork()) > 0 )  {
			/* Parent process */
			int k;
			for(k=0; k< 20; k++)  {
				(void) close(k);
			}

			/*
			 *  Wait until the child dies, of whatever cause,
			 *  or until the child kills us.
			 *  Pretty vicious, this computer society.
			 */
			while( (k = wait(&status)) != -1 && k != f )
				/* NULL */ ;

			exit(0);
			/* NOTREACHED */
		} else if( f < 0 )  {
			fb_log("sgi_open:  linger-mode fork failure\n");
			return(-1);
		}
		/* Child Process falls through */
	}

	/*
	 *  Take inventory of the hardware
	 */
	while( (inv = getinvent()) != (inventory_t *)0 )  {
		if( inv->class != INV_GRAPHICS )  continue;
		switch( inv->type )  {
		case INV_GMDEV:
			SGI(ifp)->mi_is_gt = 1;
			break;
		case INV_GR2:
			SGI(ifp)->mi_is_gt = 1;
			break;
		}
	}
	endinvent();		/* frees internal inventory memory */
	if( (ifp->if_mode & MODE_8MASK) == MODE_8NOGT )  {
		SGI(ifp)->mi_is_gt = 0;
	}

	if( (ifp->if_mode & MODE_5MASK) == MODE_5NTSC )  {
		/* NTSC, see below */
		ifp->if_width = ifp->if_max_width = XMAX170+1;	/* 646 */
		ifp->if_height = ifp->if_max_height = YMAX170+1; /* 485 */
	} else if( (ifp->if_mode & MODE_3MASK) == MODE_3FULLSCR )  {
		/* Bump default size up to full screen, since we have it all */
		ifp->if_width = XMAXSCREEN+1;		/* 1280 */
		ifp->if_height = YMAXSCREEN+1;		/* 1024 */
	}

	if( width <= 0 )
		width = ifp->if_width;
	if( height <= 0 )
		height = ifp->if_height;
	if ( width > ifp->if_max_width )
		width = ifp->if_max_width;
	if ( height > ifp->if_max_height)
		height = ifp->if_max_height;

	ifp->if_width = width;
	ifp->if_height = height;

#if 0
	blanktime(0);
#endif
	foreground();		/* Direct focus here, don't detach */

	if( (ifp->if_mode & MODE_5MASK) == MODE_5NTSC )  {
		noborder();
#if defined(__sgi) && defined(__mips)
		/* Deal with Irix 4.0 bug:  (+2,+21) offset due to title */
		prefposition( 0-2, XMAX170-2, 0+21, YMAX170+21 );
#else
		prefposition( 0, XMAX170, 0, YMAX170 );
#endif
		SGI(ifp)->mi_curs_on = 0;	/* cursoff() happens below */
	} else if( (ifp->if_mode & MODE_3MASK) == MODE_3WINDOW )  {
		if( sgi_nwindows == 0 ) {
			prefposition( WIN_L, WIN_R, WIN_B, WIN_T );
		} else {
			prefsize( (long)width, (long)height );
		}
		SGI(ifp)->mi_curs_on = 1;	/* Mex usually has it on */
	}  else  {
		/* MODE_3MASK == MODE_3FULLSCR */
		noborder();
#if defined(__sgi) && defined(__mips)
		/* Deal with Irix 4.0 bug:  (+2,+21) offset due to title */
		prefposition( 0-2, XMAXSCREEN-2, 0+21, YMAXSCREEN+21 );
#else
		prefposition( 0, XMAXSCREEN, 0, YMAXSCREEN );
#endif
		SGI(ifp)->mi_curs_on = 0;	/* cursoff() happens below */
	}

	/*
	 *  This is where the window constraints specified above
	 *  are bound to a new window.  The return code is
	 *  the "graphics id" that identifies this window.
	 *  winset(gr_id) is used to select a window for drawing in.
	 */
	if( (ifp->if_fd = winopen( "Frame buffer" )) == -1 )  {
		fb_log( "winopen() failed, no more windows available.\n" );
		return(-1);
	}
	sgi_nwindows++;		/* track # of simultaneous windows */

	/*  Establish operating mode (Hz, GENLOCK).
	 *  The assumption is that the device is always in the
	 *  "normal" mode to start with.  The mode will only
	 *  be saved and restored when 30Hz operation is specified;
	 *  on GENLOCK operation, valid NTSC sync pulses must be present
	 *  for downstream equipment;  user should run "Set60" when done.
	 */
	if( (ifp->if_mode & MODE_4MASK) == MODE_4HZ30 )  {
		SGI(ifp)->mi_der1 = getvideo(DE_R1);
		if( (ifp->if_mode & MODE_10MASK) == MODE_10SYNC_ON_GREEN )  {
			/* 3-wire */
			setvideo( DE_R1, DER1_30HZ|DER1_UNBLANK|DER1_SYNCG );
		} else {
			/* 4-wire RS-343 */
			setvideo( DE_R1, DER1_30HZ|DER1_UNBLANK);
		}
	} else if( (ifp->if_mode & MODE_5MASK) == MODE_5NTSC )  {
		int	new_der1;

		SGI(ifp)->mi_der1 = getvideo(DE_R1);
		if( (SGI(ifp)->mi_der1 & DER1_VMASK) == DER1_170 )  {
			/*
			 *  Current mode is DE3 board internal NTSC sync.
			 *  Doing a setmonitor(NTSC) again will cause the
			 *  sync generator to drop out for a moment.
			 *  So, in this case, do nothing.
			 */
		} else if( getvideo(CG_MODE) != -1 )  {
		    	/*
			 *  Optional CG2 GENLOCK board is installed.
			 *
			 *  Mode 2:  Internal sync generator is used.
			 *
			 *  Note that the stability of the sync generator
			 *  on the GENLOCK board is *worse* than the sync
			 *  generator on the regular DE3 board.  The GENLOCK
			 *  version "twitches" every second or so.
			 *
			 *  Mode 3:  Output is locked to incoming
			 *  NTSC composite video picture
		    	 *  for sync and chroma (on "REM IN" connector).
		    	 *  Color subcarrier is phase and amplitude locked to
		    	 *  incomming color burst.
		    	 *  The blue LSB has no effect on video overlay.
			 *
			 *  Note that the generated composite NTSC output
			 *  (on "VID OUT" connector) is often a problem,
			 *  since it has a DC offset of +0.3V to the base
			 *  of the sync pulse, while other studio eqiupment
			 *  often expects the blanking level to be at
			 *  exactly 0.0V, with sync at -0.3V.
			 *  In this case, the black leves are ruined.
			 *  Also, the inboard encoder chip isn't very good.
			 *  Therefore, it is necessary to use an outboard
			 *  RS-170 to NTSC encoder to get useful results.
		    	 */
			if( (ifp->if_mode & MODE_6MASK) == MODE_6EXTSYNC )  {
				/* external sync via GENLOCK board REM IN */
			    	setvideo(CG_MODE, CG2_M_MODE3);
				new_der1 = DER1_G_170;
			} else {
				/* internal sync */
				/* Just use DE3 sync generator.
				 * For this case, GENLOCK board does nothing!
				 * Equiv to setmonitor(NTSC);
				 */
				new_der1 = DER1_170;
			}
		} else {
			/*
			 *  No genlock board is installed, produce RS-170
			 *  video at NTSC rates with separate sync,
			 *  and hope that they have an outboard NTSC
			 *  encoder device.  Equiv to setmonitor(NTSC);
			 */
			new_der1 = DER1_170;
		}
		if( (ifp->if_mode & MODE_10MASK) == MODE_10SYNC_ON_GREEN )
			new_der1 |= DER1_SYNCG;
		setvideo( DE_R1, new_der1|DER1_UNBLANK );
	}

	/* Build a descriptive window title bar */
	(void)sprintf( title, "BRL libfb /dev/sgi %s, %s",
		((ifp->if_mode & MODE_2MASK) == MODE_2TRANSIENT) ?
			"Transient Win" :
			"Lingering Win",
		((ifp->if_mode & MODE_1MASK) == MODE_1MALLOC) ?
			"Private Mem" :
			"Shared Mem" );
	wintitle( title );

	/*
	 *  Set the operating mode for the newly created window.
	 */
	if( (ifp->if_mode & MODE_12MASK) == MODE_12DELAY_WRITES_TILL_FLUSH )
		SGI(ifp)->mi_is_gt = 0;
	if( SGI(ifp)->mi_is_gt )  {
		if( (ifp->if_mode & MODE_9MASK) == MODE_9SINGLEBUF )  {
			singlebuffer();
			SGI(ifp)->mi_doublebuffer = 0;
		} else {
			doublebuffer();
			SGI(ifp)->mi_doublebuffer = 1;
		}
	} else {
		singlebuffer();
		SGI(ifp)->mi_doublebuffer = 0;
	}
	RGBmode();
	gconfig();	/* Must be called after singlebuffer() & RGBmode() */

	/* Clean out images that might remain from windows underneath */
	RGBcolor( (short)0, (short)0, (short)0 );
	clear();
	if( SGI(ifp)->mi_doublebuffer == 1 )  {
		swapbuffers();
		clear();
	}

	/*
	 *  Must initialize these window state variables BEFORE calling
	 *  "sgi_getmem", because this function can indirectly trigger
	 *  a call to "gt_zbuf_to_screen" (when initializing shared memory
	 *  after a reboot).
	 */
	ifp->if_zoomflag = 0;
	ifp->if_xzoom = 1;	/* for zoom fakeout */
	ifp->if_yzoom = 1;	/* for zoom fakeout */
	ifp->if_xcenter = width/2;
	ifp->if_ycenter = height/2;
	SGI(ifp)->mi_xoff = 0;
	SGI(ifp)->mi_yoff = 0;
	SGI(ifp)->mi_pid = getpid();

	/*
	 *  In full screen mode, center the image on the screen.
	 *  For the SGI machines, this is done via mi_xoff, rather
	 *  than with viewport/ortho2 calls, because lrectwrite()
	 *  uses window-relative, NOT viewport-relative addresses.
	 */
	if( (ifp->if_mode & MODE_3MASK) == MODE_3FULLSCR )  {
		int	xleft, ybot;

		xleft = (ifp->if_max_width)/2 - ifp->if_width/2;
		ybot = (ifp->if_max_height)/2 - ifp->if_height/2;
		/* These may be necessary for cursor aiming? */
		viewport( xleft, xleft + ifp->if_width,
			  ybot, ybot + ifp->if_height );
		ortho2( (Coord)0, (Coord)ifp->if_width,
			(Coord)0, (Coord)ifp->if_height );
		/* The real secret:  used to modify args to lrectwrite() */
		SGI(ifp)->mi_xoff = xleft;
		SGI(ifp)->mi_yoff = ybot;
		/* set input focus to current window, so that
		 * we can manipulate the cursor icon */
		winattach();
	}

	/* Attach to shared memory, potentially with a screen repaint */
	if( sgi_getmem(ifp) < 0 )
		return(-1);

	/* Must call "is_linear_cmap" AFTER "sgi_getmem" which allocates
		space for the color map.				*/
	SGI(ifp)->mi_cmap_flag = !is_linear_cmap(ifp);
	if( (ifp->if_mode & MODE_7MASK) != MODE_7SWCMAP  &&
	    SGI(ifp)->mi_cmap_flag )  {
		/* Send color map to hardware -- affects whole screen */
		gammaramp( CMR(ifp), CMG(ifp), CMB(ifp) );
	}

	/* Setup default cursor.					*/
	defcursor( 1, cursor );
	defcursor( 2, nilcursor );
	curorigin( 1, 0, 0 );
	drawmode( CURSORDRAW );
	mapcolor( 1, 255, 0, 0 );
	drawmode( NORMALDRAW );
	setcursor(1, 1, 0);

	if( SGI(ifp)->mi_curs_on == 0 )  {
		setcursor( 2, 1, 0 );		/* nilcursor */
		cursoff();
	}

	/*
	 *  Display the existing contents of the memory segment.
	 *  Make no assumptions about the state of the window.
	 */
	if( (ifp->if_mode & MODE_12MASK) != MODE_12DELAY_WRITES_TILL_FLUSH )  {
		sgi_xmit_scanlines( ifp, 0, ifp->if_height, 0, ifp->if_width );
		if( SGI(ifp)->mi_is_gt )  {
			gt_zbuf_to_screen( ifp, -1 );
		}
	}

	/* Make the file descriptor available for selecting on */
	ifp->if_selfd = qgetfd();
	if( ifp->if_selfd < 0 )
		ifp->if_selfd = -1;	/* in case of errors */

	return	0;
}

/*
 *			S G I _ F I N A L _ C L O S E
 *
 *  Handle the real work of restoring the state of the hardware and
 *  closing the window.
 */
_LOCAL_ int
sgi_final_close( ifp )
FBIO	*ifp;
{
#if 0
	/*
	 *  User is finally done with the frame buffer,
	 *  return control to our caller (who may have more to do).
	 *  set a 20 minute screensave blanking when fb is closed.
	 *  We have no way of knowing if there are other libfb windows
	 *  still open.
	 */
#if 0
	blanktime( (long) 67 * 60 * 20L );
#else
	/*
	 *  Set an 8 minute screensaver blanking, which will light up
	 *  the screen again if it was dark, and will protect it otherwise.
	 *  The 4D has a hardware botch limiting the time to 2**15 frames.
	 */
	blanktime( (long) 32767L );
#endif
#endif

	/* Restore initial operation mode, if this was 30Hz */
	if( (ifp->if_mode & MODE_4MASK) == MODE_4HZ30 )
		setvideo( DE_R1, SGI(ifp)->mi_der1 );

	/*
	 *  Note that for the MODE_5NTSC mode, the monitor will
	 *  be left in NTSC mode until the user issues a "Set60"
	 *  command.  This is vitally necessary because the Lyon-
	 *  Lamb and VTR equipment need a stedy source of NTSC sync
	 *  pulses while in the process of laying down frames.
	 */

	/* Always leave cursor on when done */
	if( SGI(ifp)->mi_curs_on == 0 )  {
		setcursor( 0, 1, 0 );		/* system default cursor */
		curson();
	}

	winclose( ifp->if_fd );		/* close window */

	if( SGIL(ifp) != NULL ) {
		/* free up memory associated with image */
		if( SGI(ifp)->mi_shmid != -1 ) {
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

	sgi_nwindows--;
	return(0);
}

/*
 *			S G I _ P O L L
 *
 *  Handle input events.
 */
_LOCAL_ int
sgi_poll( ifp )
FBIO	*ifp;
{
	while( qtest() )
		sgi_inqueue(ifp);
	return 0;
}

/*
 *			S G I _ F R E E
 *
 *  Free shared memory resources, and close.
 */
_LOCAL_ int
sgi_free( ifp )
FBIO	*ifp;
{
	int	ret;

	/* Close the framebuffer */
	ret = sgi_final_close( ifp );

	if( (ifp->if_mode & MODE_1MASK) == MODE_1SHARED ) {
		/* If shared mem, release the shared memory segment */
		sgi_zapmem();
	}
	return ret;
}

/*
 *			S G I _ C L O S E
 *
 */
_LOCAL_ int
sgi_close( ifp )
FBIO	*ifp;
{
	int menu, menuval, dev;
	short val;
	FILE *fp = NULL;

	winset(ifp->if_fd);

	sgi_flush(ifp);

	if( sgi_nwindows > 1 ||
	    (ifp->if_mode & MODE_2MASK) == MODE_2TRANSIENT )
		return sgi_final_close( ifp );

	/*
	 *  LINGER mode.  Don't return to caller until user mouses "close"
	 *  menu item.  This may delay final processing in the calling
	 *  function for some time, but the assumption is that the user
	 *  wishes to compare this image with others.
	 *
	 *  Since we plan to linger here, long after our invoker
	 *  expected us to be gone, be certain that no file descriptors
	 *  remain open to associate us with pipelines, network
	 *  connections, etc., that were ALREADY ESTABLISHED before
	 *  the point that fb_open() was called.
	 *
	 *  The simple for i=0..20 loop will not work, because that
	 *  smashes some window-manager files.  Therefore, we content
	 *  ourselves with eliminating stdin, stdout, and stderr,
	 *  (fd 0,1,2), in the hopes that this will successfully
	 *  terminate any pipes or network connections.  In the case
	 *  of calls from rfbd, in normal (non -d) mode, it gets the
	 *  network connection on stdin/stdout, so this is adequate.
	 */
	fclose( stdin );
	fclose( stdout );
	fclose( stderr );

	/* Ignore likely signals, perhaps in the background,
	 * from other typing at the keyboard
	 */
	(void)signal( SIGHUP, SIG_IGN );
	(void)signal( SIGINT, SIG_IGN );
	(void)signal( SIGQUIT, SIG_IGN );
	(void)signal( SIGALRM, SIG_IGN );

	/* Line up at the "complaints window", just in case... */
	fp = fopen("/dev/console", "w");

	kill(SGI(ifp)->mi_parent, SIGUSR1);	/* zap the lurking parent */

	menu = defpup("close");
	qdevice(RIGHTMOUSE);
	qdevice(REDRAW);

	while(1)  {
		val = 0;
		dev = qread( &val );
		switch( dev )  {

		case NULLDEV:
			/* don't process a null device */
			break;

		case RIGHTMOUSE:
			menuval = dopup( menu );
			if (menuval == 1 )
				goto out;
			break;

		case REDRAW:
			reshapeviewport();
			sgi_xmit_scanlines(ifp, 0, ifp->if_height,
				0, ifp->if_width);
			if( SGI(ifp)->mi_is_gt )  {
				gt_zbuf_to_screen(ifp, -1);
			}
			break;

		case INPUTCHANGE:
		case CURSORX:
		case CURSORY:
			/* We don't need to do anything about these */
			break;

		case QREADERROR:
			/* These are fatal errors, bail out */
			if( fp ) fprintf(fp,"libfb/sgi_close: qreaderror, aborting\n");
			goto out;

		default:
			/*
			 *  There is a tendency for infinite loops
			 *  here.  With only a few qdevice() attachments
			 *  done above, there shouldn't be too many
			 *  unexpected things.  But, lots show up.
			 *  At least this gives visibility.
			 */
			if( fp ) fprintf(fp,"libfb/sgi_close: qread %d, val %d\r\n", dev, val );
			break;
		}
	}
out:
	if( fp ) fclose(fp);		/* close our "complaints window" */

	return sgi_final_close( ifp );
}

/*
 *			S G I _ C L E A R
 */
_LOCAL_ int
sgi_clear( ifp, pp )
FBIO	*ifp;
register unsigned char	*pp;
{
	struct sgi_pixel	bg;
	register struct sgi_pixel	*sgip;
	register int	cnt;
	register int	y;

	winset(ifp->if_fd);

	if( qtest() )
		sgi_inqueue(ifp);

	if ( pp != RGBPIXEL_NULL)  {
		bg.alpha = 0;
		bg.red   = (pp)[RED];
		bg.green = (pp)[GRN];
		bg.blue  = (pp)[BLU];
		RGBcolor((short)((pp)[RED]), (short)((pp)[GRN]), (short)((pp)[BLU]));
	} else {
		bg.alpha = 0;
		bg.red   = 0;
		bg.green = 0;
		bg.blue  = 0;
		RGBcolor( (short) 0, (short) 0, (short) 0);
	}

	/* Flood rectangle in shared memory */
	for( y=0; y < ifp->if_height; y++ )  {
		sgip = (struct sgi_pixel *)&ifp->if_mem[
		    (y*SGI(ifp)->mi_memwidth+0)*sizeof(struct sgi_pixel) ];
		for( cnt=ifp->if_width-1; cnt >= 0; cnt-- )  {
			*sgip++ = bg;	/* struct copy */
		}
	}

	if( SGI(ifp)->mi_is_gt )  {
		sgi_xmit_scanlines( ifp, 0, ifp->if_height,
			0, ifp->if_width );
		gt_zbuf_to_screen(ifp, -1);
	} else {
		clear();
	}
	return(0);
}

/*
 *			S G I _ V I E W
 */
_LOCAL_ int
sgi_view( ifp, xcenter, ycenter, xzoom, yzoom )
FBIO	*ifp;
int	xcenter, ycenter;
int	xzoom, yzoom;
{

	winset(ifp->if_fd);

	if( qtest() )
		sgi_inqueue(ifp);

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

	if( SGI(ifp)->mi_is_gt )  {
		/* Transmitting the Zbuffer is all that is needed */
		gt_zbuf_to_screen( ifp, -1 );
	} else {
		sgi_xmit_scanlines( ifp, 0, ifp->if_height, 0, ifp->if_width );
	}
	return(0);
}

/*
 *			S G I _ G E T V I E W
 */
_LOCAL_ int
sgi_getview( ifp, xcenter, ycenter, xzoom, yzoom )
FBIO	*ifp;
int	*xcenter, *ycenter;
int	*xzoom, *yzoom;
{

	winset(ifp->if_fd);

	if( qtest() )
		sgi_inqueue(ifp);

	*xcenter = ifp->if_xcenter;
	*ycenter = ifp->if_ycenter;
	*xzoom = ifp->if_xzoom;
	*yzoom = ifp->if_yzoom;

	return(0);
}

/*
 *			S G I _ R E A D
 *
 *  Makes no reference to the graphics library at all.
 *  Data is copied from the memory segment.
 */
_LOCAL_ int
sgi_read( ifp, x, y, pixelp, count )
FBIO	*ifp;
int	x, y;
unsigned char	*pixelp;
int	count;
{
	register short		scan_count;	/* # pix on this scanline */
	register unsigned char	*cp;
	int			ret;
	register unsigned int	n;
	register struct sgi_pixel	*sgip;

	if( x < 0 || x >= ifp->if_width ||
	    y < 0 || y >= ifp->if_height)
		return(-1);

	ret = 0;
	cp = (unsigned char *)(pixelp);

	while( count )  {
		if( y >= ifp->if_height )
			break;

		if ( count >= ifp->if_width-x )
			scan_count = ifp->if_width-x;
		else
			scan_count = count;

		sgip = (struct sgi_pixel *)&ifp->if_mem[
		    (y*SGI(ifp)->mi_memwidth+x)*sizeof(struct sgi_pixel) ];

		n = scan_count;
		while( n )  {
			cp[RED] = sgip->red;
			cp[GRN] = sgip->green;
			cp[BLU] = sgip->blue;
			sgip++;
			cp += 3;
			n--;
		}
		ret += scan_count;
		count -= scan_count;
		x = 0;
		/* Advance upwards */
		if( ++y >= ifp->if_height )
			break;
	}
	return(ret);
}

/*
 *			S G I _ W R I T E
 *
 *  The task of this routine is to reformat the pixels into
 *  SGI internal form, and then arrange to have them sent to
 *  the screen separately.
 */
_LOCAL_ int
sgi_write( ifp, xstart, ystart, pixelp, count )
register FBIO	*ifp;
int		xstart;
int		ystart;
const unsigned char	*pixelp;
int		count;
{
	register short		scan_count;	/* # pix on this scanline */
	register unsigned char	*cp;
	int			ret;
	int			ybase;
	register int		pix_count;	/* # pixels to send */
	register int		x;
	register int		y;

	/* lrectwrite dies on 0 count */
	if( (pix_count = count) == 0 )
		return 0;	/* OK, no pixels transferred */
	if( pix_count < 0 )
		return -1;	/* ERROR */

	x = xstart;
	ybase = y = ystart;

	if( x < 0 || x >= ifp->if_width ||
	    y < 0 || y >= ifp->if_height)
		return(-1);

	ret = 0;
	cp = (unsigned char *)(pixelp);

	while( pix_count )  {
		register unsigned int n;
		register struct sgi_pixel	*sgip;

		if( y >= ifp->if_height )
			break;

		if ( pix_count >= ifp->if_width-x )
			scan_count = ifp->if_width-x;
		else
			scan_count = pix_count;

		sgip = (struct sgi_pixel *)&ifp->if_mem[
		    (y*SGI(ifp)->mi_memwidth+x)*sizeof(struct sgi_pixel) ];

		n = scan_count;
		if( (n & 3) != 0 )  {
			/* This code uses 60% of all CPU time */
			while( n )  {
				/* alpha channel is always zero */
				sgip->red   = cp[RED];
				sgip->green = cp[GRN];
				sgip->blue  = cp[BLU];
				sgip++;
				cp += 3;
				n--;
			}
		} else {
			while( n )  {
				/* alpha channel is always zero */
				sgip[0].red   = cp[RED+0*3];
				sgip[0].green = cp[GRN+0*3];
				sgip[0].blue  = cp[BLU+0*3];
				sgip[1].red   = cp[RED+1*3];
				sgip[1].green = cp[GRN+1*3];
				sgip[1].blue  = cp[BLU+1*3];
				sgip[2].red   = cp[RED+2*3];
				sgip[2].green = cp[GRN+2*3];
				sgip[2].blue  = cp[BLU+2*3];
				sgip[3].red   = cp[RED+3*3];
				sgip[3].green = cp[GRN+3*3];
				sgip[3].blue  = cp[BLU+3*3];
				sgip += 4;
				cp += 3*4;
				n -= 4;
			}
		}
		ret += scan_count;
		pix_count -= scan_count;
		x = 0;
		if( ++y >= ifp->if_height )
			break;
	}

	/*
	 * Handle events after updating the memory, and
	 * before updating the screen
	 */
	winset(ifp->if_fd);

	if( qtest() )
		sgi_inqueue(ifp);

	if( (ifp->if_mode & MODE_12MASK) == MODE_12DELAY_WRITES_TILL_FLUSH )
		return ret;

	if( xstart + count <= ifp->if_width  )  {
		/* "Fast path" case for writes of less than one scanline */
		if( SGI(ifp)->mi_doublebuffer )  {
			singlebuffer();
			SGI(ifp)->mi_doublebuffer = 0;
			gconfig();
		}
		sgi_xmit_scanlines( ifp, ybase, 1, xstart, count );
		if( SGI(ifp)->mi_is_gt )  {
			/* repaint one scanline from Z buffer */
			gt_zbuf_to_screen( ifp, ybase );
		}
	} else {
		/* Normal case -- multi-pixel write */
		sgi_xmit_scanlines( ifp, ybase, y-ybase, 0, ifp->if_width );
		if( SGI(ifp)->mi_is_gt )  {
			/* repaint whole screen from Z buffer */
			gt_zbuf_to_screen( ifp, -1 );
		}
	}
	return(ret);
}

/*
 *			S G I _ W R I T E R E C T
 *
 *  The task of this routine is to reformat the pixels into
 *  SGI internal form, and then arrange to have them sent to
 *  the screen separately.
 */
_LOCAL_ int
sgi_writerect( ifp, xmin, ymin, width, height, pp )
FBIO		*ifp;
int		xmin, ymin;
int		width, height;
const unsigned char	*pp;
{
	register int		x;
	register int		y;
	register unsigned char	*cp;
	register struct sgi_pixel	*sgip;

	if( width <= 0 || height <= 0 )
		return(0);
	if( xmin < 0 || xmin+width > ifp->if_width ||
	    ymin < 0 || ymin+height > ifp->if_height )
		return(-1);

	cp = (unsigned char *)(pp);
	for( y = ymin; y < ymin+height; y++ )  {
		sgip = (struct sgi_pixel *)&ifp->if_mem[
		    (y*SGI(ifp)->mi_memwidth+xmin)*sizeof(struct sgi_pixel) ];
		for( x = xmin; x < xmin+width; x++ )  {
			/* alpha channel is always zero */
			sgip->red   = cp[RED];
			sgip->green = cp[GRN];
			sgip->blue  = cp[BLU];
			sgip++;
			cp += 3;
		}
	}

	/*
	 * Handle events after updating the memory, and
	 * before updating the screen
	 */
	winset(ifp->if_fd);

	if( qtest() )
		sgi_inqueue(ifp);

	if( (ifp->if_mode & MODE_12MASK) != MODE_12DELAY_WRITES_TILL_FLUSH )  {
		sgi_xmit_scanlines( ifp, ymin, height, 0, ifp->if_width );
		if( SGI(ifp)->mi_is_gt )  {
			/* repaint screen from Z buffer */
			gt_zbuf_to_screen( ifp, -1 );
		}
	}
	return(width*height);
}

/*
 *			S G I _ R M A P
 *
 *  Make no access to the graphics system at all.
 */
_LOCAL_ int
sgi_rmap( ifp, cmp )
register FBIO	*ifp;
register ColorMap	*cmp;
{
	register int i;

	/* Just parrot back the stored colormap */
	for( i = 0; i < 256; i++)  {
		cmp->cm_red[i]   = CMR(ifp)[i]<<8;
		cmp->cm_green[i] = CMG(ifp)[i]<<8;
		cmp->cm_blue[i]  = CMB(ifp)[i]<<8;
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
static int
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
 *			S G I _ C M I N I T
 */
_LOCAL_ void
sgi_cminit( ifp )
register FBIO	*ifp;
{
	register int	i;

	for( i = 0; i < 256; i++)  {
		CMR(ifp)[i] = i;
		CMG(ifp)[i] = i;
		CMB(ifp)[i] = i;
	}
}

/*
 *			 S G I _ W M A P
 */
_LOCAL_ int
sgi_wmap( ifp, cmp )
register FBIO	*ifp;
register const ColorMap	*cmp;
{
	register int	i;
	int		prev;	/* !0 = previous cmap was non-linear */

	winset(ifp->if_fd);

	if( qtest() )
		sgi_inqueue(ifp);

	prev = SGI(ifp)->mi_cmap_flag;
	if ( cmp == COLORMAP_NULL)  {
		sgi_cminit( ifp );
	} else {
		for(i = 0; i < 256; i++)  {
			CMR(ifp)[i] = cmp-> cm_red[i]>>8;
			CMG(ifp)[i] = cmp-> cm_green[i]>>8;
			CMB(ifp)[i] = cmp-> cm_blue[i]>>8;
		}
	}
	SGI(ifp)->mi_cmap_flag = !is_linear_cmap(ifp);

	if( (ifp->if_mode & MODE_7MASK) == MODE_7SWCMAP )  {
		if( SGI(ifp)->mi_cmap_flag == 0 && prev == 0 )  return(0);
		/* Software color mapping, trigger a repaint */
		sgi_xmit_scanlines( ifp, 0, ifp->if_height, 0, ifp->if_width );
		if( SGI(ifp)->mi_is_gt )  {
			gt_zbuf_to_screen( ifp, -1 );
		}
	} else {
		/* Send color map to hardware -- affects whole screen */
		gammaramp( CMR(ifp), CMG(ifp), CMB(ifp) );
	}
	return(0);
}

/*
 *			S G I _ S E T C U R S O R
 */
_LOCAL_ int
sgi_setcursor( ifp, bits, xbits, ybits, xorig, yorig )
FBIO	*ifp;
const unsigned char	*bits;
int		xbits, ybits;
int		xorig, yorig;
{
	register int	y;
	register int	xbytes;
	Cursor		newcursor;

	winset(ifp->if_fd);

	if( qtest() )
		sgi_inqueue(ifp);

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
	setcursor( 1, 0, 0 );
	return	0;
}

/*
 *			S G I _ C U R S O R
 */
_LOCAL_ int
sgi_cursor( ifp, mode, x, y )
FBIO	*ifp;
int	mode;
int	x, y;
{
	short	xmin, ymin;
	register short	i;
	short	xwidth;
	long left, bottom, x_size, y_size;

	winset(ifp->if_fd);

	if( qtest() )
		sgi_inqueue(ifp);

	fb_sim_cursor(ifp, mode, x, y);

	SGI(ifp)->mi_curs_on = mode;
	if( ! mode )  {
		setcursor( 2, 1, 0 );		/* nilcursor */
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
	setcursor( 1, 1, 0 );			/* our cursor */
	curson();
	getsize(&x_size, &y_size);
	getorigin( &left, &bottom );

/*	RGBcursor( 1, 255, 255, 0, 0xFF, 0xFF, 0xFF ); */
	setvaluator( MOUSEX, x+left, 0, XMAXSCREEN );
	setvaluator( MOUSEY, y+bottom, 0, YMAXSCREEN );

	return	0;
}


/*
 *			S G I _ I N Q U E U E
 *
 *  Called when a qtest() indicates that there is a window event.
 *  Process all events, so that we don't loop on recursion to sgw_bwrite.
 */
_LOCAL_ void
sgi_inqueue(ifp)
register FBIO *ifp;
{
	short val;
	int redraw = 0;
	register int ev;

	winset(ifp->if_fd);

	while( qtest() )  {
		switch( ev = qread(&val) )  {
		case REDRAW:
			/* should check val -vs- ifp->if_fd !! */
			redraw = 1;
			break;
		case INPUTCHANGE:
			break;
		case MODECHANGE:
			/* This could be bad news.  Should we re-write
			 * the color map? */
			fb_log("sgi_inqueue:  modechange?\n");
			break;
		case MOUSEX :
		case MOUSEY :
		case KEYBD :
			break;
		default:
			fb_log("sgi_inqueue:  event %d unknown\n", ev);
			break;
		}
	}
	/*
	 * Now that all the events have been removed from the input
	 * queue, handle any actions that need to be done.
	 */
	if( redraw )  {
		reshapeviewport();
		sgi_xmit_scanlines( ifp, 0, ifp->if_height, 0, ifp->if_width );
		if( SGI(ifp)->mi_is_gt )  {
			gt_zbuf_to_screen( ifp, -1 );
		}
		redraw = 0;
	}
}

/*
 *			S G I _ H E L P
 */
_LOCAL_ int
sgi_help( ifp )
FBIO	*ifp;
{
	struct	modeflags *mfp;

	fb_log( "Description: %s\n", ifp->if_type );
	fb_log( "Device: %s\n", ifp->if_name );
	fb_log( "Max width height: %d %d\n",
		ifp->if_max_width,
		ifp->if_max_height );
	fb_log( "Default width height: %d %d\n",
		ifp->if_width,
		ifp->if_height );
	fb_log( "Usage: /dev/sgi[option letters]\n" );
	for( mfp = modeflags; mfp->c != '\0'; mfp++ ) {
		fb_log( "%c  %c   %s\n",
			((ifp->if_mode & mfp->mask) == mfp->value) ? 'Y' : ' ',
			mfp->c, mfp->help );
	}

	fb_log( "\nCurrent internal state:\n");
	fb_log( "	mi_doublebuffer=%d\n", SGI(ifp)->mi_doublebuffer );
	fb_log( "	mi_cmap_flag=%d\n", SGI(ifp)->mi_cmap_flag );
	fb_log( "	mi_is_gt=%d\n", SGI(ifp)->mi_is_gt );

	return(0);
}

/*
 *			S G I _ F L U S H
 *
 *  When simulating a double-buffered display, don't send any updated
 *  pixels to the screen until explicitly flushed.
 */
_LOCAL_ int
sgi_flush( ifp )
FBIO	*ifp;
{
	if( (ifp->if_mode & MODE_12MASK) != MODE_12DELAY_WRITES_TILL_FLUSH )
		return 0;

	winset(ifp->if_fd);

	/* Send entire in-memory buffer to the screen, all at once */
	sgi_xmit_scanlines( ifp, 0, ifp->if_height, 0, ifp->if_width );
	if( SGI(ifp)->mi_is_gt )  {
		/* repaint screen from Z buffer */
		gt_zbuf_to_screen( ifp, -1 );
	}
	return 0;
}

/***************************************************************************/
/***************************************************************************/
/***************************************************************************/
/***************** Special Support for the GT ******************************/
/***************************************************************************/
/***************************************************************************/
/***************************************************************************/

/*
 *			G T _ Z B U F _ T O _ S C R E E N
 *
 *  The parameter "one_y" is set to -1 to repaint the entire screen,
 *  or to the Y coordinate of the single scanline to be repainted.
 */
_LOCAL_ void
gt_zbuf_to_screen( ifp, one_y )
register FBIO	*ifp;
int		one_y;
{
	struct sgi_clip	clip;

	winset(ifp->if_fd);

	zbuffer(FALSE);
	readsource(SRC_ZBUFFER);	/* source for rectcopy() */
	zdraw(FALSE);
	/* dest for rectcopy() */
	if( SGI(ifp)->mi_doublebuffer )  {
 		backbuffer(TRUE);
		frontbuffer(FALSE);
	} else {
		backbuffer(FALSE);
		frontbuffer(TRUE);
	}

	sgi_clipper( ifp, &clip );
	if( SGI(ifp)->mi_doublebuffer == 0 && one_y >= 0 )  {
		/* Do only one scanline, not entire image */
		if( one_y < clip.ymin || one_y > clip.ymax )
			return;
		/* This scanline is in bounds */
		clip.yscroff += (one_y - clip.ymin) * ifp->if_yzoom;
		clip.ymin = clip.ymax = one_y;
	}

	/* rectzoom only works on GT and PI machines */
	rectzoom( (double) ifp->if_xzoom, (double) ifp->if_yzoom);

	if( SGI(ifp)->mi_doublebuffer )  {
		/*
		 *  This clear could be somewhat of a performance problem,
		 *  but it prevents having crud on the borders when
		 *  panning the image.
		 *  In singlebuffer mode, don't take the performance hit.
		 *  (Filling with black & repainting is disturbing)
		 *  (Should just black out the borders instead).
		 */
		cpack(0x00000000);	/* clear to black first */
		clear();		/* takes ~1 frame time */
	}

	/* All coordinates are window-relative, not viewport-relative */
	rectcopy(
		clip.xmin+SGI(ifp)->mi_xoff,
		clip.ymin+SGI(ifp)->mi_yoff,
		clip.xmax+SGI(ifp)->mi_xoff,
		clip.ymax+SGI(ifp)->mi_yoff,
		clip.xscroff+SGI(ifp)->mi_xoff,
		clip.yscroff+SGI(ifp)->mi_yoff );

	if( SGI(ifp)->mi_doublebuffer )  {
	 	swapbuffers();
	}
	rectzoom( 1.0, 1.0 );
}

/*
 *			S G I _ C L I P P E R
 *
 *  The image coordinates of the lower left pixel in view are:
 *	(xmin, ymin)
 *  The screen coordinates of the lower left pixle in view are:
 *	(xscroff, yscroff)
 */
_LOCAL_ void
sgi_clipper( ifp, clp )
register FBIO	*ifp;
register struct sgi_clip	*clp;
{
	register int	i;

	clp->xscroff = clp->yscroff = 0;
	clp->xscrpad = clp->yscrpad = 0;

	i = (ifp->if_width/2)/ifp->if_xzoom;
	clp->xmin = ifp->if_xcenter - i;
	clp->xmax = ifp->if_xcenter + i - 1;

	i = (ifp->if_height/2)/ifp->if_yzoom;
	clp->ymin = ifp->if_ycenter - i;
	clp->ymax = ifp->if_ycenter + i - 1;

	if( clp->xmin < 0 )  {
		clp->xscroff = -(clp->xmin * ifp->if_xzoom);
		clp->xmin = 0;
	}
	if( clp->ymin < 0 )  {
		clp->yscroff = -(clp->ymin * ifp->if_yzoom);
		clp->ymin = 0;
	}

	if( clp->xmax > ifp->if_width-1 )  {
		clp->xscrpad = (clp->xmax - (ifp->if_width-1)) * ifp->if_xzoom;
		clp->xmax = ifp->if_width-1;
	}

	if( clp->ymax > ifp->if_height-1 )  {
		clp->yscrpad = (clp->ymax - (ifp->if_height-1)) * ifp->if_yzoom;
		clp->ymax = ifp->if_height-1;
	}
}

/*
 *			F A K E _ L R E C T W R I T E
 *
 * fake_rectwrite is necessary as lrectwrite is not yet supported
 * for non-GT hardware in IRIX version 3.1 or earlier. There is
 * however, a stub in the library which states that lrectwrite is not yet
 * available for these systems. To allow us to still use
 * shared libraries and have the same executables across the
 * 4D series of workstations, if the system does not contain
 * the GT hardware upgrade then this fake routine is used.
 *
 * Note that the real lrectwrite() subroutine operates in the pixel
 * coordinates of the WINDOW, not the current viewport.
 * To simplify things, this fake_rectwrite() subroutine operates in
 * the coordinates of the VIEWPORT, because cmov2i() and writeRGB() do.
 * Having the callers convert to window, and then to convert back in here,
 * is more inefficient than is necessary.  However, this required the
 * calling sequences to be somewhat altered -vs- the lrectwrite() replaced.
 */
_LOCAL_ void
fake_rectwrite( x1, y1, x2, y2, pixels)
short	x1, y1;
short	x2, y2;
register struct sgi_pixel * pixels;
{
	register struct sgi_pixel * p;
	register short	n;
	register short	scan;
	register short	i;
	static unsigned char Red_pixels[1280];
	static unsigned char Green_pixels[1280];
	static unsigned char Blue_pixels[1280];

	p = pixels;
	n = x2  - x1 + 1;
	for( scan = y1; scan <= y2; scan++)  {
		for ( i = 0; i < n; i++)  {
			Red_pixels[i] =   p->red;
			Green_pixels[i] = p->green;
			Blue_pixels[i] =  p->blue;
			p++;
		}

		cmov2i( x1, scan);
		writeRGB( n, Red_pixels, Green_pixels, Blue_pixels);
	}
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
