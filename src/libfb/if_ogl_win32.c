/*                  I F _ O G L _ W I N 3 2 . C
 * BRL-CAD
 *
 * Copyright (C) 2004-2005 United States Government as represented by
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
/** @file if_ogl_win32.c
 *  Frame Buffer Library interface for OpenGL.
 *
 *  There are several different Frame Buffer modes supported.
 *  Set your environment FB_FILE to the appropriate type.
 *  Note that some of the /dev/sgi modes are not supported, and there are
 *  some new modes.
 *  (see the modeflag definitions below).
 *	/dev/ogl[options]
 *
 *  This code is basically a port of if_4d.c from IRIS GL to OpenGL.
 *
 *  Authors -
 *	Carl Nuzman
 *	Bob Parker
 *
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5068  USA
 *
 */
/*@}*/

#ifndef lint
static char RCSid[] = "@(#)$Header$ (ARL)";
#endif

#include "common.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>
#ifdef HAVE_GL_GL_H
#  include <GL/gl.h>
#endif
#undef RED

#include "tk.h"
#include "tkPlatDecls.h"
#include "machine.h"
#include "bu.h"
#include "fb.h"
#include "./fblocal.h"

#define CJDEBUG 0

/*WWW these come from Iris gl gl.h*/
#define XMAXSCREEN	1279
#define YMAXSCREEN	1023
#define XMAXMEDIUM	1023
#define YMAXMEDIUM	767
#define XMAX170		645
#define YMAX170		484


/* Internal callbacks etc.*/
_LOCAL_ void		do_event();
_LOCAL_ void		expose_callback();
void ogl_configureWindow FB_ARGS((FBIO *ifp, int width, int height));

/* Other Internal routines */
_LOCAL_ void		ogl_clipper();
_LOCAL_ int		ogl_getmem();
_LOCAL_ void		backbuffer_to_screen();
_LOCAL_ void		ogl_cminit();
#if 0
_LOCAL_ void		reorder_cursor();
#endif
_LOCAL_ PIXELFORMATDESCRIPTOR *	ogl_choose_visual();
_LOCAL_ int		is_linear_cmap();

_LOCAL_ int	ogl_nwindows = 0; 	/* number of open windows */
_LOCAL_ int	multiple_windows = 0;	/* someone wants to be ready
					 * for multiple windows, at the
					 * expense of speed.
					 */
/*_LOCAL_	XColor	color_cell[256];*/		/* used to set colormap */

int ogl_refresh();
int ogl_open_existing();
int ogl_close_existing();
int _ogl_open_existing();

_LOCAL_ int	ogl_open(),
		ogl_close(),
		ogl_clear(),
		ogl_read(),
		ogl_write(),
		ogl_rmap(),
		ogl_wmap(),
		ogl_view(),
		ogl_getview(),
		ogl_setcursor(),
		ogl_cursor(),
#if 0
		ogl_getcursor(),
		ogl_readrect(),
		fb_cnull(),
#endif
		ogl_writerect(),
		ogl_bwwriterect(),
		ogl_poll(),
		ogl_flush(),
		ogl_free(),
		ogl_help();

/* This is the ONLY thing that we normally "export" */
FBIO ogl_interface =
	{
	0,			/* magic number slot	*/
	ogl_open,		/* open device		*/
	ogl_close,		/* close device		*/
	ogl_clear,		/* clear device		*/
	ogl_read,		/* read	pixels		*/
	ogl_write,		/* write pixels		*/
	ogl_rmap,		/* read colormap	*/
	ogl_wmap,		/* write colormap	*/
	ogl_view,		/* set view		*/
	ogl_getview,		/* get view		*/
	ogl_setcursor,		/* define cursor	*/
	ogl_cursor,		/* set cursor		*/
	fb_sim_getcursor,	/* get cursor		*/
	fb_sim_readrect,	/* read rectangle	*/
	ogl_writerect,		/* write rectangle	*/
	fb_sim_bwreadrect,
	ogl_bwwriterect,	/* write rectangle	*/
	ogl_poll,		/* process events	*/
	ogl_flush,		/* flush output		*/
	ogl_free,		/* free resources	*/
	ogl_help,		/* help message		*/
	"Microsoft Windows OpenGL",	/* device description	*/
	XMAXSCREEN+1,			/* max width		*/
	YMAXSCREEN+1,			/* max height		*/
	"/dev/ogl",		/* short device name	*/
	512,			/* default/current width  */
	512,			/* default/current height */
	-1,			/* select file desc	*/
	-1,			/* file descriptor	*/
	1, 1,			/* zoom			*/
	256, 256,		/* window center	*/
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

FBIO	*saveifp;

/*
 *  Structure of color map in shared memory region.
 *  Has exactly the same format as the SGI hardware "gammaramp" map
 *  Note that only the lower 8 bits are significant.
 */
struct ogl_cmap {
	short	cmr[256];
	short	cmg[256];
	short	cmb[256];
};

/*
 *  This defines the format of the in-memory framebuffer copy.
 *  The alpha component and reverse order are maintained for
 *  compatibility with /dev/sgi
 */
struct ogl_pixel {
	unsigned char blue;
	unsigned char green;
	unsigned char red;
	unsigned char alpha;

};

/* Clipping structure for zoom/pan operations */
struct ogl_clip {
	int	xpixmin;	/* view clipping planes clipped to pixel memory space*/
	int	xpixmax;
	int	ypixmin;
	int	ypixmax;
	int	xscrmin;	/* view clipping planes */
	int	xscrmax;
	int	yscrmin;
	int	yscrmax;
	double	oleft;		/* glOrtho parameters */
	double	oright;
	double	otop;
	double	obottom;

};

/*
 *  Per window state information, overflow area.
 *  Structure members have the same meaning as in the if_4d.c code.
 */
struct sgiinfo {
	short	mi_curs_on;
	short	mi_cmap_flag;		/* enabled when there is a non-linear map in memory */
	int	mi_shmid;
	int	mi_memwidth;		/* width of scanline in if_mem */
	short	mi_xoff;		/* X viewport offset, rel. window*/
	short	mi_yoff;		/* Y viewport offset, rel. window*/
	int	mi_pid;			/* for multi-cpu check */
	int	mi_parent;		/* PID of linger-mode process */
	int	mi_doublebuffer;	/* 0=singlebuffer 1=doublebuffer */
	struct ogl_pixel mi_scanline[XMAXSCREEN+1];	/* one scanline */
};

/*
 *  Per window state information particular to the OpenGL interface
 */
struct oglinfo {
	HGLRC	glxc;
	Display	       *dispp;		/* pointer to X display connection */
	Window		wind;		/* Window identifier */
	int		firstTime;
	int		alive;
	long		event_mask;	/* event types to be received */
	short		front_flag;	/* front buffer being used (b-mode) */
	short		copy_flag;	/* pan and zoom copied from backbuffer */
	short		soft_cmap_flag;	/* use software colormapping */
	int		cmap_size;	/* hardware colormap size */
	int 		win_width;	/* actual window width */
	int		win_height;	/* actual window height */
	int		vp_width;	/* actual viewport width */
	int		vp_height;	/* actual viewport height */
	struct ogl_clip	clip;		/* current view clipping */
	Window		cursor;
	PIXELFORMATDESCRIPTOR    *vip;		/* pointer to info on current visual */
	Colormap	xcmap;		/* xstyle color map */
	int		use_ext_ctrl;	/* for controlling the Ogl graphics engine externally */
	HDC		hdc;
	HWND	hwnd;

};

#define	SGI(ptr)	((struct sgiinfo *)((ptr)->u1.p))
#define	SGIL(ptr)	((ptr)->u1.p)		/* left hand side version */
#define	OGL(ptr)	((struct oglinfo *)((ptr)->u6.p))
#define	OGLL(ptr)	((ptr)->u6.p)		/* left hand side version */
#define if_mem		u2.p			/* shared memory pointer */
#define if_cmap		u3.p			/* color map in shared memory */
#define CMR(x)		((struct ogl_cmap *)((x)->if_cmap))->cmr
#define CMG(x)		((struct ogl_cmap *)((x)->if_cmap))->cmg
#define CMB(x)		((struct ogl_cmap *)((x)->if_cmap))->cmb
#define if_zoomflag	u4.l			/* zoom > 1 */
#define if_mode		u5.l			/* see MODE_* defines */

#define MARGIN	4			/* # pixels margin to screen edge */

#define CLIP_XTRA 1

#define WIN_L (ifp->if_max_width - ifp->if_width - MARGIN)
#define WIN_T (ifp->if_max_height - ifp->if_height - MARGIN)

/*
 *  The mode has several independent bits:
 *	SHARED -vs- MALLOC'ed memory for the image
 *	TRANSIENT -vs- LINGERING windows
 *	Windowed -vs- Centered Full screen
 *	Suppress dither -vs- dither
 *	Double -vs- Single buffered
 *	DrawPixels -vs- CopyPixels
 */
#define MODE_1MASK	(1<<0)
#define MODE_1SHARED	(0<<0)		/* Use Shared memory */
#define MODE_1MALLOC	(1<<0)		/* Use malloc memory */

#define MODE_2MASK	(1<<1)
#define MODE_2TRANSIENT	(0<<1)
#define MODE_2LINGERING (1<<1)		/* leave window up after closing*/

#define MODE_3MASK	(1<<2)
#define MODE_3WINDOW	(0<<2)		/* window mode */
#define MODE_3FULLSCR	(1<<2)		/* full screen mode */

#define MODE_4MASK	(1<<3)
#define MODE_4NORMAL	(0<<3)		/* dither if it seems necessary */
#define MODE_4NODITH	(1<<3)		/* suppress any dithering */

#define MODE_5MASK	(1<<4)
#define MODE_5NORMAL	(0<<4)	 	/* fast - assume no multiple windows */
#define MODE_5MULTI	(1<<4)		/* be ready for multiple windows */

#define MODE_7MASK	(1<<6)
#define MODE_7NORMAL	(0<<6)		/* install colormap in hardware if possible*/
#define MODE_7SWCMAP	(1<<6)		/* use software colormapping */

#define MODE_9MASK	(1<<8)
#define MODE_9NORMAL	(0<<8)		/* doublebuffer if possible */
#define MODE_9SINGLEBUF	(1<<8)		/* singlebuffer only */

#define MODE_11MASK	(1<<10)
#define MODE_11NORMAL	(0<<10)		/* always draw from mem. to window*/
#define MODE_11COPY	(1<<10)		/* keep full image on back buffer,*/

#define MODE_12MASK	(1<<11)
#define MODE_12NORMAL	(0<<11)
#define MODE_12DELAY_WRITES_TILL_FLUSH	(1<<11)
					/* and copy current view to front */
#define MODE_15MASK	(1<<14)
#define MODE_15NORMAL	(0<<14)
#define MODE_15ZAP	(1<<14)		/* zap the shared memory segment */

_LOCAL_ struct modeflags {
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
	{ 'd',  MODE_4MASK, MODE_4NODITH,
		"Suppress dithering - else dither if not 24-bit buffer" },
	{ 'm',  MODE_5MASK, MODE_5MULTI,
		"Be ready for multiple windows - else optimize for single windows" },
	{ 'c',	MODE_7MASK, MODE_7SWCMAP,
		"Perform software colormap - else use hardware colormap if possible" },
	{ 's',	MODE_9MASK, MODE_9SINGLEBUF,
		"Single buffer -  else double buffer if possible" },
	{ 'b',	MODE_11MASK, MODE_11COPY,
		"Fast pan and zoom using backbuffer copy -  else normal " },
	{ 'D',	MODE_12DELAY_WRITES_TILL_FLUSH, MODE_12DELAY_WRITES_TILL_FLUSH,
		"Don't update screen until fb_flush() is called.  (Double buffer sim)" },
	{ 'z',	MODE_15MASK, MODE_15ZAP,
		"Zap (free) shared memory.  Can also be done with fbfree command" },
	{ '\0', 0, 0, "" }
};


/************************************************************************/
/************************************************************************/
/************************************************************************/
/******************* Shared Memory Support ******************************/
/************************************************************************/
/************************************************************************/
/************************************************************************/

/*
 *			O G L _ G E T M E M
 *
 *			not changed from
 *
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
 *  attach to the shared memory, and then return the break to its
 *  original value.  This should allow most reasonable requests for
 *  memory to be satisfied.  In special cases, the values used here
 *  might need to be increased.
 */
_LOCAL_ int
ogl_getmem( ifp )
FBIO	*ifp;
{
	int	pixsize;
	int	size;
	int	i;
	char	*sp;
	int	new = 0;

	errno = 0;

/*	if( (ifp->if_mode & MODE_1MASK) == MODE_1MALLOC )  */{
		/*
		 *  In this mode, only malloc as much memory as is needed.
		 */
		SGI(ifp)->mi_memwidth = ifp->if_width;
		pixsize = ifp->if_height * ifp->if_width * sizeof(struct ogl_pixel);
		size = pixsize + sizeof(struct ogl_cmap);

		sp = calloc( 1, size );
		if( sp == 0 )  {
			fb_log("ogl_getmem: frame buffer memory malloc failed\n");
			goto fail;
		}
		new = 1;
		goto success;
	}

success:
	ifp->if_mem = sp;
	ifp->if_cmap = sp + pixsize;	/* cmap at end of area */
	i = CMB(ifp)[255];		/* try to deref last word */
	CMB(ifp)[255] = i;

	/* Provide non-black colormap on creation of new shared mem */
	if(new)
		ogl_cminit( ifp );
	return(0);
fail:
	fb_log("ogl_getmem:  Unable to attach to shared memory.\nConsult comment in cad/libfb/if_4d.c for details\n");
	if( (sp = calloc( 1, size )) == NULL )  {
		fb_log("ogl_getmem:  malloc failure\n");
		return(-1);
	}
	new = 1;
	goto success;
}

/*
 *			O G L _ Z A P M E M
 */
void
ogl_zapmem()
{
}

/*
 *			S I G K I D
 */
static void
#if _XOPEN_SOURCE
sigkid( pid )
int pid;
#else
sigkid()
#endif
{
	exit(0);
}



/* 			O G L _ X M I T _ S C A N L I N E S
 *
 * Note: unlike sgi_xmit_scanlines, this function updates an arbitrary
 * rectangle of the frame buffer
 */
_LOCAL_ void
ogl_xmit_scanlines( ifp, ybase, nlines, xbase, npix )
register FBIO	*ifp;
int		ybase;
int		nlines;
int		xbase;
int		npix;
{
	register int	y;
	register int	n;
	int		sw_cmap;	/* !0 => needs software color map */
	struct ogl_clip	*clp;

	/* Caller is expected to handle attaching context, etc. */

	clp = &(OGL(ifp)->clip);

	if( OGL(ifp)->soft_cmap_flag  && SGI(ifp)->mi_cmap_flag )  {
	    	sw_cmap = 1;
	} else {
		sw_cmap = 0;
	}

	if(xbase > clp->xpixmax || ybase > clp->ypixmax)
		return;
	if(xbase < clp->xpixmin)
		xbase = clp->xpixmin;
	if(ybase < clp->ypixmin)
		ybase = clp->ypixmin;

	if((xbase + npix -1 ) > clp->xpixmax)
		npix = clp->xpixmax - xbase + 1;
	if((ybase + nlines - 1) > clp->ypixmax)
		nlines = clp->ypixmax - ybase + 1;

	if(!OGL(ifp)->use_ext_ctrl){
	if (!OGL(ifp)->copy_flag){
		/*
		 * Blank out areas of the screen around the image, if exposed.
		 * In COPY mode, this is done in backbuffer_to_screen().
		 */

		/* Blank out area left of image */
		glColor3b( 0, 0, 0 );
		if( clp->xscrmin < 0 )  glRecti(
			clp->xscrmin - CLIP_XTRA,
			clp->yscrmin - CLIP_XTRA,
			CLIP_XTRA,
			clp->yscrmax + CLIP_XTRA);

		/* Blank out area below image */
		if( clp->yscrmin < 0 )  glRecti(
			clp->xscrmin - CLIP_XTRA,
			clp->yscrmin - CLIP_XTRA,
			clp->xscrmax + CLIP_XTRA,
			CLIP_XTRA);

		/* Blank out area right of image */
		if( clp->xscrmax >= ifp->if_width )  glRecti(
			ifp->if_width - CLIP_XTRA,
			clp->yscrmin - CLIP_XTRA,
			clp->xscrmax + CLIP_XTRA,
			clp->yscrmax + CLIP_XTRA);

		/* Blank out area above image */
		if( clp->yscrmax >= ifp->if_height )  glRecti(
			clp->xscrmin - CLIP_XTRA,
			ifp->if_height- CLIP_XTRA,
			clp->xscrmax + CLIP_XTRA,
			clp->yscrmax + CLIP_XTRA);

	} else if (OGL(ifp)->front_flag) {
		/* in COPY mode, always draw full sized image into backbuffer.
		 * backbuffer_to_screen() is used to update the front buffer
		 */
		glDrawBuffer(GL_BACK);
		OGL(ifp)->front_flag = 0;
		glMatrixMode(GL_PROJECTION);
		glPushMatrix();	/* store current view clipping matrix*/
		glLoadIdentity();
		glOrtho( -0.25, ((GLdouble) OGL(ifp)->vp_width)-0.25,
			-0.25, ((GLdouble) OGL(ifp)->vp_height)-0.25,
			-1.0, 1.0);
		glPixelZoom( 1.0, 1.0);
	}
	}

	if( sw_cmap ) {
		/* Software colormap each line as it's transmitted */
		register int	x;
		register struct ogl_pixel	*oglp;
		register struct ogl_pixel	*op;

		y = ybase;
		if(CJDEBUG) printf("Doing sw colormap xmit\n");
		/* Perform software color mapping into temp scanline */
		op = SGI(ifp)->mi_scanline;
		for( n=nlines; n>0; n--, y++ )  {
			oglp = (struct ogl_pixel *)&ifp->if_mem[
				(y*SGI(ifp)->mi_memwidth)*
				sizeof(struct ogl_pixel) ];
			for( x=xbase+npix-1; x>=xbase; x-- )  {
				op[x].red   = CMR(ifp)[oglp[x].red];
				op[x].green = CMG(ifp)[oglp[x].green];
				op[x].blue  = CMB(ifp)[oglp[x].blue];
			}

			glPixelStorei(GL_UNPACK_SKIP_PIXELS,xbase);
			glRasterPos2i(xbase,y);
			glDrawPixels(npix,1,GL_BGRA_EXT,GL_UNSIGNED_BYTE,
					(const GLvoid *) op);

		}

	} else  {
		/* No need for software colormapping */

		glPixelStorei(GL_UNPACK_ROW_LENGTH,SGI(ifp)->mi_memwidth);
		glPixelStorei(GL_UNPACK_SKIP_PIXELS,xbase);
		glPixelStorei(GL_UNPACK_SKIP_ROWS,ybase);

		glRasterPos2i(xbase,ybase);
		glDrawPixels(npix,nlines,GL_BGRA_EXT,GL_UNSIGNED_BYTE,
				(const GLvoid *) ifp->if_mem);
	}
}

LONG WINAPI MainWndProc (
    HWND    hWnd,
    UINT    uMsg,
    WPARAM  wParam,
    LPARAM  lParam)
{
    switch (uMsg) {
    case WM_PAINT:
	if(!OGL(saveifp)->use_ext_ctrl)
	    expose_callback(saveifp,0);
	break;
    case WM_LBUTTONDOWN:
	break;
    case WM_RBUTTONDOWN:
	break;
    case WM_MBUTTONDOWN:
	break;
    case WM_CLOSE:
	OGL(saveifp)->alive = 0;
	break;
    case WM_LBUTTONUP:
	OGL(saveifp)->alive = 0;
	break;
    case WM_RBUTTONUP:
	OGL(saveifp)->alive = 0;
	break;
    case WM_MBUTTONUP:
	OGL(saveifp)->alive = 0;
	break;
    case WM_KEYDOWN:
	break;
    case WM_KEYUP:
	OGL(saveifp)->alive = 0;
	break;
    case WM_SIZE:
	{
#if 0
	    if(conf->width == OGL(ifp)->win_width &&
	       conf->height == OGL(ifp)->win_height)
		return;

	    ogl_configureWindow(ifp, conf->width, conf->height);
#endif
	}
    default:
      return DefWindowProc (hWnd, uMsg, wParam, lParam);
    }

    return 1;
}
_LOCAL_ int
ogl_open( ifp, file, width, height )
FBIO	*ifp;
char	*file;
int	width, height;
{
	static char	title[128];
	int		mode,  ret;
	HWND hwnd;
	HDC hdc;
	HGLRC glxc;
	HINSTANCE hinstance;
	DWORD Dword;
	WNDCLASS   wndclass;

	FB_CK_FBIO(ifp);

	saveifp = ifp;


	/*
	 *  First, attempt to determine operating mode for this open,
	 *  based upon the "unit number" or flags.
	 *  file = "/dev/ogl###"
	 *  The default mode is zero.
	 */
	mode = 0;

	if( file != NULL )  {
		register char *cp;
		char	modebuf[80];
		char	*mp;
		int	alpha;
		struct	modeflags *mfp;

		if( strncmp(file, "/dev/ogl", 8) ) {
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
			ogl_zapmem();
			return(-1);
		}
	}
	ifp->if_mode = mode;

	/*
	 *  Allocate extension memory sections,
	 *  addressed by SGI(ifp)->mi_xxx and OGL(ifp)->xxx
	 */

	if( (SGIL(ifp) = (char *)calloc( 1, sizeof(struct sgiinfo) )) == NULL )  {
		fb_log("ogl_open:  sgiinfo malloc failed\n");
		return(-1);
	}
	if( (OGLL(ifp) = (char *)calloc( 1, sizeof(struct oglinfo) )) == NULL )  {
		fb_log("ogl_open:  oglinfo malloc failed\n");
		return(-1);
	}

	SGI(ifp)->mi_shmid = -1;	/* indicate no shared memory */

	if (ogl_nwindows && !multiple_windows){
		fb_log("Warning - ogl_open: Multiple windows opened. Use /dev/oglm for first window!");
	}

	/* Anyone can turn this on; no one can turn it off */
	if( (ifp->if_mode & MODE_5MASK) == MODE_5MULTI )
		multiple_windows = 1;

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

	if( (ifp->if_mode & MODE_3MASK) == MODE_3FULLSCR )  {
		/* Bump default size up to full screen, since we have it all */
		ifp->if_width = XMAXSCREEN+1;		/* 1280 */
		ifp->if_height = YMAXSCREEN+1;		/* 1024 */
	}

	/* use defaults if invalid width and height specified */
	if( width <= 0 )
		width = ifp->if_width;
	if( height <= 0 )
		height = ifp->if_height;
	/* use max values if width and height are greater */
	if ( width > ifp->if_max_width )
		width = ifp->if_max_width;
	if ( height > ifp->if_max_height)
		height = ifp->if_max_height;

	ifp->if_width = width;
	ifp->if_height = height;


	if( (ifp->if_mode & MODE_3MASK) == MODE_3WINDOW )  {
		SGI(ifp)->mi_curs_on = 1;
	}  else  {
		/* MODE_3MASK == MODE_3FULLSCR */
		SGI(ifp)->mi_curs_on = 0;
	}

	/* Build a descriptive window title bar */
	(void)sprintf( title, "BRL-CAD /dev/ogl %s, %s",
		((ifp->if_mode & MODE_2MASK) == MODE_2TRANSIENT) ?
			"Transient Win":
			"Lingering Win",
		((ifp->if_mode & MODE_1MASK) == MODE_1MALLOC) ?
			"Private Mem" :
			"Shared Mem" );


	/* initialize window state variables before calling ogl_getmem */
	ifp->if_zoomflag = 0;
	ifp->if_xzoom = 1;	/* for zoom fakeout */
	ifp->if_yzoom = 1;	/* for zoom fakeout */
	ifp->if_xcenter = width/2;
	ifp->if_ycenter = height/2;
	/* SGI(ifp)->mi_pid = getpid(); */

	/* Attach to shared memory, potentially with a screen repaint */
	if( ogl_getmem(ifp) < 0 )
		return(-1);

	/* Register the frame class */
    wndclass.style         = 0;
    wndclass.lpfnWndProc   = (WNDPROC)MainWndProc;
    wndclass.cbClsExtra    = 0;
    wndclass.cbWndExtra    = 0;
    wndclass.hInstance     = Tk_GetHINSTANCE();
    wndclass.hIcon         = LoadIcon (Tk_GetHINSTANCE(), "Win OpenGL");
    wndclass.hCursor       = LoadCursor (NULL,IDC_ARROW);
    wndclass.hbrBackground = (HBRUSH)(COLOR_WINDOW+1);
    wndclass.lpszMenuName  = "Win OpenGL";
    wndclass.lpszClassName = "Win OpenGL";

	ret = RegisterClass (&wndclass);

	OGL(ifp)->hwnd =  CreateWindow(
  "Win OpenGL",  /* pointer to registered class name */
  title, /* pointer to window name */
  WS_OVERLAPPEDWINDOW | WS_CLIPSIBLINGS | WS_CLIPCHILDREN,        /* window style */
  CW_USEDEFAULT,                /* horizontal position of window */
  CW_USEDEFAULT,                /* vertical position of window */
  ifp->if_width,           /* window width */
  ifp->if_height,          /* window height */
  NULL,      /* handle to parent or owner window */
  NULL,          /* handle to menu or child-window identifier */
  Tk_GetHINSTANCE(),     /* handle to application instance */
  NULL        /* pointer to window-creation data */
);

	Dword = GetLastError();

	hinstance=Tk_GetHINSTANCE();
	hwnd = OGL(ifp)->hwnd;
	OGL(ifp)->hdc = GetDC(OGL(ifp)->hwnd);
	hdc = OGL(ifp)->hdc;



	/* Choose an appropriate visual. */
	if( (OGL(ifp)->vip = ogl_choose_visual(ifp)) == NULL ) {
		fb_log("ogl_open: Couldn't find an appropriate visual.  Exiting.\n");
		return (-1);
	}

	OGL(ifp)->glxc = wglCreateContext(OGL(ifp)->hdc);
	glxc = OGL(ifp)->glxc;

	/* count windows */
	ogl_nwindows++;

	OGL(ifp)->alive = 1;
	OGL(ifp)->firstTime = 1;

	ShowWindow(OGL(ifp)->hwnd,SW_SHOW);
	UpdateWindow(OGL(ifp)->hwnd);


	/* Loop through events until first exposure event is processed */
	/*	while (OGL(ifp)->firstTime == 1)
		do_event(ifp);
	*/

	return 0;

}


int
ogl_open_existing(ifp, argc, argv)
FBIO *ifp;
int argc;
char **argv;
{
  Display *dpy;
  Window win;
  Colormap cmap;
  PIXELFORMATDESCRIPTOR *vip;
  int width;
  int height;
  HGLRC glxc;
  int double_buffer;
  int soft_cmap;

  if(argc != 10)
    return -1;

  if(sscanf(argv[1], "%lu", (unsigned long *)&dpy) != 1)
   return -1;

  if(sscanf(argv[2], "%lu", (unsigned long *)&win) != 1)
    return -1;

  if(sscanf(argv[3], "%lu", (unsigned long *)&cmap) != 1)
    return -1;

  if(sscanf(argv[4], "%lu", (unsigned long *)&vip) != 1)
    return -1;

  if(sscanf(argv[5], "%d", &width) != 1)
    return -1;

  if(sscanf(argv[6], "%d", &height) != 1)
    return -1;

  if(sscanf(argv[7], "%lu", (unsigned long *)&glxc) != 1)
    return -1;

  if(sscanf(argv[8], "%d", &double_buffer) != 1)
    return -1;

  if(sscanf(argv[9], "%d", &soft_cmap) != 1)
    return -1;

  return _ogl_open_existing(ifp, dpy, win, cmap, vip, width, height,
			    glxc, double_buffer, soft_cmap);
}

int
_ogl_open_existing(ifp, dpy, win, cmap, vip, width, height, glxc, double_buffer, soft_cmap)
FBIO *ifp;
Display *dpy;
Window win;
Colormap cmap;
PIXELFORMATDESCRIPTOR *vip;
int width;
int height;
HGLRC glxc;
int double_buffer;
int soft_cmap;
{

  /*XXX for now use private memory */
  ifp->if_mode = MODE_1MALLOC;

  /*
   *  Allocate extension memory sections,
   *  addressed by SGI(ifp)->mi_xxx and OGL(ifp)->xxx
   */

  if( (SGIL(ifp) = (char *)calloc( 1, sizeof(struct sgiinfo) )) == NULL )  {
    fb_log("ogl_open:  sgiinfo malloc failed\n");
    return -1;
  }
  if( (OGLL(ifp) = (char *)calloc( 1, sizeof(struct oglinfo) )) == NULL )  {
    fb_log("ogl_open:  oglinfo malloc failed\n");
    return -1;
  }

  OGL(ifp)->use_ext_ctrl = 1;

  SGI(ifp)->mi_shmid = -1;	/* indicate no shared memory */
  multiple_windows = 1;
  ifp->if_width = ifp->if_max_width = width;
  ifp->if_height = ifp->if_max_height = height;

  OGL(ifp)->win_width = OGL(ifp)->vp_width = width;
  OGL(ifp)->win_height = OGL(ifp)->vp_height = height;

  SGI(ifp)->mi_curs_on = 1;

  /* initialize window state variables before calling ogl_getmem */
  ifp->if_zoomflag = 0;
  ifp->if_xzoom = 1;	/* for zoom fakeout */
  ifp->if_yzoom = 1;	/* for zoom fakeout */
  ifp->if_xcenter = width/2;
  ifp->if_ycenter = height/2;
  /* SGI(ifp)->mi_pid = getpid(); */

  /* Attach to shared memory, potentially with a screen repaint */
  if(ogl_getmem(ifp) < 0)
    return -1;

  OGL(ifp)->dispp = dpy;
  /* ifp->if_selfd = ConnectionNumber(OGL(ifp)->dispp); */

  OGL(ifp)->vip = vip;
  OGL(ifp)->glxc = glxc;
  SGI(ifp)->mi_cmap_flag = !is_linear_cmap(ifp);
  OGL(ifp)->soft_cmap_flag = soft_cmap;
  SGI(ifp)->mi_doublebuffer = double_buffer;
  OGL(ifp)->xcmap = cmap;

  OGL(ifp)->wind = win;
  ++ogl_nwindows;

  OGL(ifp)->alive = 1;
  OGL(ifp)->firstTime = 1;

  ogl_clipper(ifp);

  return 0;
}

_LOCAL_ int
ogl_final_close( ifp )
FBIO	*ifp;
{

  if( CJDEBUG ) {
    printf("ogl_final_close: All done...goodbye!\n");
  }

  /* if(OGL(ifp)->cursor)
     XDestroyWindow(OGL(ifp)->dispp, OGL(ifp)->cursor);
  */

  /* XDestroyWindow(OGL(ifp)->dispp, OGL(ifp)->wind);
     XFreeColormap(OGL(ifp)->dispp, OGL(ifp)->xcmap);
  */

  if (OGL(ifp)->glxc)
	  wglDeleteContext(OGL(ifp)->glxc);
  if (OGL(ifp)->hdc)
	  ReleaseDC(OGL(ifp)->hwnd, OGL(ifp)->hdc);
	DestroyWindow(OGL(ifp)->hwnd);


  if( SGIL(ifp) != NULL ) {
    /* free up memory associated with image */

      /* free private memory */
      (void)free( ifp->if_mem );
    /* free state information */
    (void)free( (char *)SGIL(ifp) );
    SGIL(ifp) = NULL;
  }

  if( OGLL(ifp) != NULL) {
    (void) free( (char *)OGLL(ifp) );
    OGLL(ifp) = NULL;
  }

  ogl_nwindows--;
  return(0);
}


_LOCAL_ int
ogl_close( ifp )
FBIO	*ifp;
{

	ogl_flush( ifp );

	/* only the last open window can linger -
	 * call final_close if not lingering
	 */
	if( ogl_nwindows > 1 ||
	    (ifp->if_mode & MODE_2MASK) == MODE_2TRANSIENT )
		return ogl_final_close( ifp );

	if( CJDEBUG )
		printf("ogl_close: remaining open to linger awhile.\n");

	/*
	 *  else:
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
	 *  terminate any pipes or network connections.
	 */
	fclose( stdin );
	fclose( stdout );
	fclose( stderr );

	/* Ignore likely signals, perhaps in the background,
	 * from other typing at the keyboard
	 */
	/*
	  (void)signal( SIGHUP, SIG_IGN );
	  (void)signal( SIGINT, SIG_IGN );
	  (void)signal( SIGQUIT, SIG_IGN );
	  (void)signal( SIGALRM, SIG_IGN );
	*/

	while( OGL(ifp)->alive )
	    do_event(ifp);

	return 0;
}

int
ogl_close_existing(ifp)
FBIO *ifp;
{
    /*
      if(OGL(ifp)->cursor)
          XDestroyWindow(OGL(ifp)->dispp, OGL(ifp)->cursor);
    */

  if( SGIL(ifp) != NULL ) {
    /* free up memory associated with image */
      /* free private memory */
      (void)free( ifp->if_mem );
    /* free state information */
    (void)free( (char *)SGIL(ifp) );
    SGIL(ifp) = NULL;
  }

  if( OGLL(ifp) != NULL) {
    (void) free( (char *)OGLL(ifp) );
    OGLL(ifp) = NULL;
  }

  return 0;
}

/*
 *			O G L _ P O L L
 *
 *	Handle any pending input events
 */
_LOCAL_ int
ogl_poll(ifp)
FBIO	*ifp;
{
	do_event(ifp);
	return(0);
}

/*
 *			O G L _ F R E E
 *
 *  Free shared memory resources, and close.
 */
_LOCAL_ int
ogl_free( ifp )
FBIO	*ifp;
{
	int	ret;

	if(CJDEBUG) printf("entering  ogl_free\n");
	/* Close the framebuffer */
	ret = ogl_final_close( ifp );

	if( (ifp->if_mode & MODE_1MASK) == MODE_1SHARED ) {
		/* If shared mem, release the shared memory segment */
		ogl_zapmem();
	}
	return ret;
}


_LOCAL_ int
ogl_clear( ifp, pp )
FBIO	*ifp;
unsigned char	*pp;		/* pointer to beginning of memory segment*/
{
	struct ogl_pixel		bg;
	register struct ogl_pixel      *oglp;
	register int			cnt;
	register int			y;

	if( CJDEBUG ) printf("entering ogl_clear\n");

	if (multiple_windows) {
		if (wglMakeCurrent(OGL(ifp)->hdc,OGL(ifp)->glxc)==False){
		fb_log("Warning, ogl_clear: wglMakeCurrent unsuccessful.\n");
		}
	}

	/* Set clear colors */
	if ( pp != RGBPIXEL_NULL)  {
		bg.alpha = 0;
		bg.red   = (pp)[RED];
		bg.green = (pp)[GRN];
		bg.blue  = (pp)[BLU];
		glClearColor( pp[RED]/255.0, pp[GRN]/255.0, pp[BLU]/255.0, 0.0 );
	} else {
		bg.alpha = 0;
		bg.red   = 0;
		bg.green = 0;
		bg.blue  = 0;
		glClearColor( 0, 0, 0, 0 );
	}

	/* Flood rectangle in shared memory */
	for( y=0; y < ifp->if_height; y++ )  {
		oglp = (struct ogl_pixel *)&ifp->if_mem[
		    (y*SGI(ifp)->mi_memwidth+0)*sizeof(struct ogl_pixel) ];
		for( cnt=ifp->if_width-1; cnt >= 0; cnt-- )  {
			*oglp++ = bg;	/* struct copy */
		}
	}


	/* Update screen */
	if(OGL(ifp)->use_ext_ctrl){
		glClear(GL_COLOR_BUFFER_BIT);
	}else{
	if ( OGL(ifp)->copy_flag){
		/* COPY mode: clear both buffers */
		if (OGL(ifp)->front_flag){
			glDrawBuffer(GL_BACK);
			glClear(GL_COLOR_BUFFER_BIT);
			glDrawBuffer(GL_FRONT);
			glClear(GL_COLOR_BUFFER_BIT);
		} else {
			glDrawBuffer(GL_FRONT);
			glClear(GL_COLOR_BUFFER_BIT);
			glDrawBuffer(GL_BACK);
			glClear(GL_COLOR_BUFFER_BIT);
		}
	} else {
		glClear(GL_COLOR_BUFFER_BIT);
		if(SGI(ifp)->mi_doublebuffer){
			SwapBuffers( OGL(ifp)->hdc);
		}
	}

	if (multiple_windows) {
		/* unattach context for other threads to use */
		wglMakeCurrent(OGL(ifp)->hdc,OGL(ifp)->glxc);
	}
	}

	return(0);
}


/*
 *			O G L _ V I E W
 */
_LOCAL_ int
ogl_view( ifp, xcenter, ycenter, xzoom, yzoom )
FBIO	*ifp;
int	xcenter, ycenter;
int	xzoom, yzoom;
{
	struct ogl_clip *clp;

	if(CJDEBUG) printf("entering ogl_view\n");

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


	if(OGL(ifp)->use_ext_ctrl){
		ogl_clipper(ifp);
	}else{
	if (multiple_windows) {
		if (wglMakeCurrent(OGL(ifp)->hdc,OGL(ifp)->glxc)==False){
			fb_log("Warning, ogl_view: wglMakeCurrent unsuccessful.\n");
		}
	}

	/* Set clipping matrix  and zoom level */
	glMatrixMode(GL_PROJECTION);
	if (OGL(ifp)->copy_flag && !OGL(ifp)->front_flag){
		/* COPY mode - no changes to backbuffer copy - just
		 * need to update front buffer
		 */
		glPopMatrix();
		glDrawBuffer(GL_FRONT);
		OGL(ifp)->front_flag = 1;
	}
	glLoadIdentity();

	ogl_clipper(ifp);
	clp = &(OGL(ifp)->clip);
	glOrtho( clp->oleft, clp->oright, clp->obottom, clp->otop, -1.0, 1.0);
	glPixelZoom((float) ifp->if_xzoom,(float) ifp->if_yzoom);

	if (OGL(ifp)->copy_flag){
		backbuffer_to_screen(ifp,-1);
	} else {
		ogl_xmit_scanlines( ifp, 0, ifp->if_height, 0, ifp->if_width );
		if(SGI(ifp)->mi_doublebuffer){
			SwapBuffers( OGL(ifp)->hdc);
		}
	}

	if (multiple_windows) {
		/* unattach context for other threads to use */
		wglMakeCurrent(OGL(ifp)->hdc,OGL(ifp)->glxc);
	}
	}

	return(0);
}

/*
 *			O G L _ G E T V I E W
 */
_LOCAL_ int
ogl_getview( ifp, xcenter, ycenter, xzoom, yzoom )
FBIO	*ifp;
int	*xcenter, *ycenter;
int	*xzoom, *yzoom;
{
	if(CJDEBUG) printf("entering ogl_getview\n");

	*xcenter = ifp->if_xcenter;
	*ycenter = ifp->if_ycenter;
	*xzoom = ifp->if_xzoom;
	*yzoom = ifp->if_yzoom;

	return(0);
}


_LOCAL_ int
ogl_read( ifp, x, y, pixelp, count ) /*read count pixels into pixelp starting at x,y*/
FBIO	*ifp;
int	x, y;
unsigned char	*pixelp;
int	count;
{
	register short		scan_count;	/* # pix on this scanline */
	register unsigned char	*cp;
	int			ret;
	register unsigned int	n;
	register struct ogl_pixel	*oglp;

	if(CJDEBUG) printf("entering ogl_read\n");

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

		oglp = (struct ogl_pixel *)&ifp->if_mem[
		    (y*SGI(ifp)->mi_memwidth+x)*sizeof(struct ogl_pixel) ];

		n = scan_count;
		while( n )  {
			cp[RED] = oglp->red;
			cp[GRN] = oglp->green;
			cp[BLU] = oglp->blue;
			oglp++;
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



_LOCAL_ int
ogl_write( ifp, xstart, ystart, pixelp, count ) /*write count pixels from pixelp starting at xstart,ystart*/
FBIO	*ifp;
int	xstart, ystart;
const unsigned char	*pixelp;
int	count;
{
	register short		scan_count;	/* # pix on this scanline */
	register unsigned char	*cp;
	int			ret;
	int			ybase;
	register int		pix_count;	/* # pixels to send */
	register int		x;
	register int		y;

	if(CJDEBUG) printf("entering ogl_write\n");

	/* fast exit cases */
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
		register struct ogl_pixel	*oglp;

		if( y >= ifp->if_height )
			break;

		if ( pix_count >= ifp->if_width-x )
			scan_count = ifp->if_width-x;
		else
			scan_count = pix_count;

		oglp = (struct ogl_pixel *)&ifp->if_mem[
		    (y*SGI(ifp)->mi_memwidth+x)*sizeof(struct ogl_pixel) ];

		n = scan_count;
		if( (n & 3) != 0 )  {
			/* This code uses 60% of all CPU time */
			while( n )  {
				/* alpha channel is always zero */
				oglp->red   = cp[RED];
				oglp->green = cp[GRN];
				oglp->blue  = cp[BLU];
				oglp++;
				cp += 3;
				n--;
			}
		} else {
			while( n )  {
				/* alpha channel is always zero */
				oglp[0].red   = cp[RED+0*3];
				oglp[0].green = cp[GRN+0*3];
				oglp[0].blue  = cp[BLU+0*3];
				oglp[1].red   = cp[RED+1*3];
				oglp[1].green = cp[GRN+1*3];
				oglp[1].blue  = cp[BLU+1*3];
				oglp[2].red   = cp[RED+2*3];
				oglp[2].green = cp[GRN+2*3];
				oglp[2].blue  = cp[BLU+2*3];
				oglp[3].red   = cp[RED+3*3];
				oglp[3].green = cp[GRN+3*3];
				oglp[3].blue  = cp[BLU+3*3];
				oglp += 4;
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

	if( (ifp->if_mode & MODE_12MASK) == MODE_12DELAY_WRITES_TILL_FLUSH )
		return ret;

	if (multiple_windows) {
		if (wglMakeCurrent(OGL(ifp)->hdc,OGL(ifp)->glxc)==False){
			fb_log("Warning, ogl_write: wglMakeCurrent unsuccessful.\n");
		}
	}

	if(!OGL(ifp)->use_ext_ctrl){
	if( xstart + count <= ifp->if_width  )  {
		/* "Fast path" case for writes of less than one scanline.
		 * The assumption is that there will be a lot of short
		 * writes, and it's best just to ignore the backbuffer
		 */
#if 0
		if ( SGI(ifp)->mi_doublebuffer ) {
			/* "turn off" doublebuffering*/
			SGI(ifp)->mi_doublebuffer = 0;
			glDrawBuffer(GL_FRONT);
		}
#endif
		ogl_xmit_scanlines( ifp, ybase, 1, xstart, count );
		if (OGL(ifp)->copy_flag){
			/* repaint one scanline from backbuffer */
			backbuffer_to_screen(ifp,ybase);
		}
	} else {
		/* Normal case -- multi-pixel write */
		if ( SGI(ifp)->mi_doublebuffer) { /* refresh whole screen */
			ogl_xmit_scanlines( ifp, 0, ifp->if_height, 0, ifp->if_width );
			SwapBuffers( OGL(ifp)->hdc);
		}
		else { /* just write rectangle */
			ogl_xmit_scanlines( ifp, ybase, y-ybase, 0, ifp->if_width );
			if (OGL(ifp)->copy_flag){
				backbuffer_to_screen(ifp,-1);
			}
		}
	}

	if (multiple_windows) {
		/* unattach context for other threads to use */
		wglMakeCurrent(OGL(ifp)->hdc,OGL(ifp)->glxc);
	}
	}

	return(ret);

}

/*
 *			O G L _ W R I T E R E C T
 *
 *  The task of this routine is to reformat the pixels into
 *  SGI internal form, and then arrange to have them sent to
 *  the screen separately.
 */
_LOCAL_ int
ogl_writerect(FBIO *ifp,
	      int xmin,
	      int ymin,
	      int width,
	      int height,
	      const unsigned char *pp)
{
	register int		x;
	register int		y;
	register unsigned char	*cp;
	register struct ogl_pixel	*oglp;

	if(CJDEBUG) printf("entering ogl_writerect\n");


	if( width <= 0 || height <= 0 )
		return(0);  /* do nothing */
	if( xmin < 0 || xmin+width > ifp->if_width ||
	    ymin < 0 || ymin+height > ifp->if_height )
		return(-1); /* no can do */

	cp = (unsigned char *)(pp);
	for( y = ymin; y < ymin+height; y++ )  {
		oglp = (struct ogl_pixel *)&ifp->if_mem[
		    (y*SGI(ifp)->mi_memwidth+xmin)*sizeof(struct ogl_pixel) ];
		for( x = xmin; x < xmin+width; x++ )  {
			/* alpha channel is always zero */
			oglp->red   = cp[RED];
			oglp->green = cp[GRN];
			oglp->blue  = cp[BLU];
			oglp++;
			cp += 3;
		}
	}

	if( (ifp->if_mode & MODE_12MASK) == MODE_12DELAY_WRITES_TILL_FLUSH )
		return width*height;

	if(!OGL(ifp)->use_ext_ctrl){
	if (multiple_windows) {
	if (wglMakeCurrent(OGL(ifp)->hdc,OGL(ifp)->glxc)==False){
		fb_log("Warning, ogl_writerect: wglMakeCurrent unsuccessful.\n");
	}
	}

	if ( SGI(ifp)->mi_doublebuffer) { /* refresh whole screen */
		ogl_xmit_scanlines( ifp, 0, ifp->if_height, 0, ifp->if_width );
		SwapBuffers( OGL(ifp)->hdc);
	}
	else { /* just write rectangle*/
		ogl_xmit_scanlines( ifp, ymin, height, xmin, width );
		if (OGL(ifp)->copy_flag){
			backbuffer_to_screen(ifp,-1);
		}
	}

	if (multiple_windows) {
	/* unattach context for other threads to use */
	wglMakeCurrent(OGL(ifp)->hdc,OGL(ifp)->glxc);
	}
	}

	return(width*height);
}


/*
 *			O G L _ B W W R I T E R E C T
 *
 *  The task of this routine is to reformat the pixels into
 *  SGI internal form, and then arrange to have them sent to
 *  the screen separately.
 */
_LOCAL_ int
ogl_bwwriterect(FBIO *ifp,
		int xmin,
		int ymin,
		int width,
		int height,
		const unsigned char *pp)
{
	register int		x;
	register int		y;
	register unsigned char	*cp;
	register struct ogl_pixel	*oglp;

	if(CJDEBUG) printf("entering ogl_bwwriterect\n");


	if( width <= 0 || height <= 0 )
		return(0);  /* do nothing */
	if( xmin < 0 || xmin+width > ifp->if_width ||
	    ymin < 0 || ymin+height > ifp->if_height )
		return(-1); /* no can do */

	cp = (unsigned char *)(pp);
	for( y = ymin; y < ymin+height; y++ )  {
		oglp = (struct ogl_pixel *)&ifp->if_mem[
		    (y*SGI(ifp)->mi_memwidth+xmin)*sizeof(struct ogl_pixel) ];
		for( x = xmin; x < xmin+width; x++ )  {
			register int	val;
			/* alpha channel is always zero */
			oglp->red   = (val = *cp++);
			oglp->green = val;
			oglp->blue  = val;
			oglp++;
		}
	}

	if( (ifp->if_mode & MODE_12MASK) == MODE_12DELAY_WRITES_TILL_FLUSH )
		return width*height;

	if(!OGL(ifp)->use_ext_ctrl){
	if (multiple_windows) {
	if (wglMakeCurrent(OGL(ifp)->hdc,OGL(ifp)->glxc)==False){
		fb_log("Warning, ogl_writerect: wglMakeCurrent unsuccessful.\n");
	}
	}

	if ( SGI(ifp)->mi_doublebuffer) { /* refresh whole screen */
		ogl_xmit_scanlines( ifp, 0, ifp->if_height, 0, ifp->if_width );
		SwapBuffers( OGL(ifp)->hdc);
	}
	else { /* just write rectangle*/
		ogl_xmit_scanlines( ifp, ymin, height, xmin, width );
		if (OGL(ifp)->copy_flag){
			backbuffer_to_screen(ifp,-1);
		}
	}

	if (multiple_windows) {
	/* unattach context for other threads to use */
	wglMakeCurrent(OGL(ifp)->hdc,OGL(ifp)->glxc);
	}
	}

	return(width*height);
}



_LOCAL_ int
ogl_rmap(register FBIO *ifp,
	 register ColorMap *cmp)
{
	register int i;

	if(CJDEBUG) printf("entering ogl_rmap\n");

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
_LOCAL_ int
is_linear_cmap(register FBIO *ifp)
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
 *			O G L _ C M I N I T
 */
_LOCAL_ void
ogl_cminit(register FBIO *ifp)
{
	register int	i;

	for( i = 0; i < 256; i++)  {
		CMR(ifp)[i] = i;
		CMG(ifp)[i] = i;
		CMB(ifp)[i] = i;
	}
}

/*
 *			 O G L _ W M A P
 */
_LOCAL_ int
ogl_wmap(register FBIO *ifp,
	 register const ColorMap *cmp)
{
	register int	i;
	int		prev;	/* !0 = previous cmap was non-linear */

	if(CJDEBUG) printf("entering ogl_wmap\n");

	prev = SGI(ifp)->mi_cmap_flag;
	if ( cmp == COLORMAP_NULL)  {
		ogl_cminit( ifp );
	} else {
		for(i = 0; i < 256; i++)  {
			CMR(ifp)[i] = cmp-> cm_red[i]>>8;
			CMG(ifp)[i] = cmp-> cm_green[i]>>8;
			CMB(ifp)[i] = cmp-> cm_blue[i]>>8;
		}
	}
	SGI(ifp)->mi_cmap_flag = !is_linear_cmap(ifp);


	if(!OGL(ifp)->use_ext_ctrl){
	if( OGL(ifp)->soft_cmap_flag )  {
		/* if current and previous maps are linear, return */
		if( SGI(ifp)->mi_cmap_flag == 0 && prev == 0 )  return(0);

		/* Software color mapping, trigger a repaint */

		if (multiple_windows) {
		if (wglMakeCurrent(OGL(ifp)->hdc,OGL(ifp)->glxc)==False){
			fb_log("Warning, ogl_wmap: wglMakeCurrent unsuccessful.\n");
		}
		}

		ogl_xmit_scanlines( ifp, 0, ifp->if_height, 0, ifp->if_width );
		if(SGI(ifp)->mi_doublebuffer){
			SwapBuffers( OGL(ifp)->hdc);
		} else if (OGL(ifp)->copy_flag) {
			backbuffer_to_screen(ifp,-1);
		}
		if (multiple_windows) {
		/* unattach context for other threads to use, also flushes */
		wglMakeCurrent(OGL(ifp)->hdc,OGL(ifp)->glxc);
		}
	} else {
		/* Send color map to hardware */
		/* This code has yet to be tested */

/*	    	for (i = 0; i < 256; i++) {
	    		color_cell[i].pixel = i;
	    		color_cell[i].red = CMR(ifp)[i];
	    		color_cell[i].green = CMG(ifp)[i];
	    		color_cell[i].blue = CMB(ifp)[i];
	    		color_cell[i].flags = DoRed | DoGreen | DoBlue;
	    	}
    		XStoreColors(OGL(ifp)->dispp, OGL(ifp)->xcmap, color_cell, 256);*/
	}
	}

	return(0);
}

/*
 *			O G L _ H E L P
 */
_LOCAL_ int
ogl_help(FBIO *ifp)
{
	struct	modeflags *mfp;
	PIXELFORMATDESCRIPTOR *visual = OGL(ifp)->vip;

	fb_log( "Description: %s\n", ifp->if_type );
	fb_log( "Device: %s\n", ifp->if_name );
	fb_log( "Max width height: %d %d\n",
		ifp->if_max_width,
		ifp->if_max_height );
	fb_log( "Default width height: %d %d\n",
		ifp->if_width,
		ifp->if_height );
	fb_log( "Usage: /dev/ogl[option letters]\n" );
	for( mfp = modeflags; mfp->c != '\0'; mfp++ ) {
		fb_log( "   %c   %s\n", mfp->c, mfp->help );
	}

	fb_log( "\nCurrent internal state:\n");
	fb_log( "	mi_doublebuffer=%d\n", SGI(ifp)->mi_doublebuffer );
	fb_log( "	mi_cmap_flag=%d\n", SGI(ifp)->mi_cmap_flag );
	fb_log( "	ogl_nwindows=%d\n", ogl_nwindows );
	fb_log( "	multiple_windows=%d\n", multiple_windows );

	fb_log("X11 Visual:\n");

/*	switch(visual->class) {
	case DirectColor:
		fb_log("\tDirectColor: Alterable RGB maps, pixel RGB subfield indicies\n");
		fb_log("\tRGB Masks: 0x%x 0x%x 0x%x\n", visual->red_mask,
		       visual->green_mask, visual->blue_mask);
		break;
	case TrueColor:
		fb_log("\tTrueColor: Fixed RGB maps, pixel RGB subfield indicies\n");
		fb_log("\tRGB Masks: 0x%x 0x%x 0x%x\n", visual->red_mask,
		       visual->green_mask, visual->blue_mask);
		break;
	case PseudoColor:
		fb_log("\tPseudoColor: Alterable RGB maps, single index\n");
		break;
	case StaticColor:
		fb_log("\tStaticColor: Fixed RGB maps, single index\n");
		break;
	case GrayScale:
		fb_log("\tGrayScale: Alterable map (R=G=B), single index\n");
		break;
	case StaticGray:
		fb_log("\tStaticGray: Fixed map (R=G=B), single index\n");
		break;
	default:
		fb_log("\tUnknown visual class %d\n",
		       visual->class);
		break;
	}
	fb_log("\tColormap Size: %d\n", visual->colormap_size);
	fb_log("\tBits per RGB: %d\n", visual->bits_per_rgb);
	fb_log("\tscreen: %d\n", visual->screen);
	fb_log("\tdepth (total bits per pixel): %d\n", visual->depth);
	if( visual->depth < 24 )
		fb_log("\tWARNING: unable to obtain full 24-bits of color, image will be quantized.\n");
*/
	return 0;
}


_LOCAL_ int
ogl_setcursor(FBIO *ifp,
	      const unsigned char *bits,
	      int xbits,
	      int ybits,
	      int xorig,
	      int yorig)
{
  return 0;
}


_LOCAL_ int
ogl_cursor(FBIO *ifp, int mode, int x, int y)
{
return 0;
}



_LOCAL_ int
ogl_flush(FBIO *ifp)
{
	if( (ifp->if_mode & MODE_12MASK) == MODE_12DELAY_WRITES_TILL_FLUSH )  {
		if (multiple_windows) {
			if (wglMakeCurrent(OGL(ifp)->hdc,OGL(ifp)->glxc)==False){
				fb_log("Warning, ogl_flush: wglMakeCurrent unsuccessful.\n");
			}
		}
		/* Send entire in-memory buffer to the screen, all at once */
		ogl_xmit_scanlines( ifp, 0, ifp->if_height, 0, ifp->if_width );
		if ( SGI(ifp)->mi_doublebuffer) {
			SwapBuffers( OGL(ifp)->hdc);
		} else {
			if (OGL(ifp)->copy_flag){
				backbuffer_to_screen(ifp,-1);
			}
		}
	}
	/* XFlush(OGL(ifp)->dispp); */
	glFlush();
	return(0);
}

#if 0
_LOCAL_ int
fb_cnull(FBIO *ifp)
{
	return(0);
}
#endif

/*
 * O G L _ C L I P P E R ( )
 *
 * Given:- the size of the viewport in pixels (vp_width, vp_height)
 *	 - the size of the framebuffer image (if_width, if_height)
 *	 - the current view center (if_xcenter, if_ycenter)
 * 	 - the current zoom (if_xzoom, if_yzoom)
 * Calculate:
 *	 - the position of the viewport in image space
 *		(xscrmin,xscrmax,yscrmin,yscrmax)
 *	 - the portion of the image which is visible in the viewport
 *		(xpixmin,xpixmax,ypixmin,ypixmax)
 */
void
ogl_clipper(register FBIO *ifp)
{
	register struct ogl_clip *clp;
	register int	i;
	double pixels;

	clp = &(OGL(ifp)->clip);

	i = OGL(ifp)->vp_width/(2*ifp->if_xzoom);
	clp->xscrmin = ifp->if_xcenter - i;
	i = OGL(ifp)->vp_width/ifp->if_xzoom;
	clp->xscrmax = clp->xscrmin + i;
	pixels = (double) i;
	clp->oleft = ((double) clp->xscrmin) - 0.25*pixels/((double) OGL(ifp)->vp_width);
	clp->oright = clp->oleft + pixels;

	i = OGL(ifp)->vp_height/(2*ifp->if_yzoom);
	clp->yscrmin = ifp->if_ycenter - i;
	i = OGL(ifp)->vp_height/ifp->if_yzoom;
	clp->yscrmax = clp->yscrmin + i;
	pixels = (double) i;
	clp->obottom = ((double) clp->yscrmin) - 0.25*pixels/((double) OGL(ifp)->vp_height);
	clp->otop = clp->obottom + pixels;

	clp->xpixmin = clp->xscrmin;
	clp->xpixmax = clp->xscrmax;
	clp->ypixmin = clp->yscrmin;
	clp->ypixmax = clp->yscrmax;

	if( clp->xpixmin < 0 )  {
		clp->xpixmin = 0;
	}

	if( clp->ypixmin < 0 )  {
		clp->ypixmin = 0;
	}

	/* In copy mode, the backbuffer copy image is limited
	 * to the viewport size; use that for clipping.
	 * Otherwise, use size of framebuffer memory segment
	 */
	if (OGL(ifp)->copy_flag) {
		if( clp->xpixmax > OGL(ifp)->vp_width-1 )  {
			clp->xpixmax = OGL(ifp)->vp_width-1;
		}
		if( clp->ypixmax > OGL(ifp)->vp_height-1 )  {
			clp->ypixmax = OGL(ifp)->vp_height-1;
		}
	} else {
		if( clp->xpixmax > ifp->if_width-1 )  {
			clp->xpixmax = ifp->if_width-1;
		}
		if( clp->ypixmax > ifp->if_height-1 )  {
			clp->ypixmax = ifp->if_height-1;
		}
	}

}


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *	Call back routines and so on				   *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 */

_LOCAL_ void
do_event(FBIO *ifp)
{
	MSG msg;
	BOOL bRet;
	/* Check and Dispatch any messages. */

    if( (bRet = GetMessage( &msg, NULL, 0, 0 )) != 0)
    {
        if (bRet == -1)
        {
            /* handle the error and possibly exit */
        }
        else
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }
    /* let's not starve the processor */
    Sleep( 250 );
}

_LOCAL_ void
expose_callback(FBIO *ifp,
		int eventPtr)
{
    /*	XWindowAttributes xwa; */
	struct ogl_clip *clp;

	if( CJDEBUG ) fb_log("entering expose_callback()\n");


	if( multiple_windows || OGL(ifp)->firstTime ) {
		if( wglMakeCurrent(OGL(ifp)->hdc,OGL(ifp)->glxc) == False) {
			fb_log("Warning, libfb/expose_callback: wglMakeCurrent unsuccessful.\n");
		}
	}

	if( OGL(ifp)->firstTime ) {
		OGL(ifp)->firstTime = 0;

		/* just in case the configuration is double buffered but
		 * we want to pretend it's not
		 */

		if( !SGI(ifp)->mi_doublebuffer ) {
			glDrawBuffer(GL_FRONT);
		}

		if( (ifp->if_mode & MODE_4MASK) == MODE_4NODITH ) {
			glDisable(GL_DITHER);
		}

		/* set copy mode if possible and requested */
		if( SGI(ifp)->mi_doublebuffer &&
		    ((ifp->if_mode & MODE_11MASK)==MODE_11COPY) ) {
			/* Copy mode only works if there are two
			 * buffers to use. It conflicts with
			 * double buffering
			 */
			OGL(ifp)->copy_flag = 1;
			SGI(ifp)->mi_doublebuffer = 0;
			OGL(ifp)->front_flag = 1;
			glDrawBuffer(GL_FRONT);
		} else {
			OGL(ifp)->copy_flag = 0;
		}

		OGL(ifp)->win_width=ifp->if_width;
		OGL(ifp)->win_height=ifp->if_height;

		/* clear entire window */
		glViewport(0, 0, OGL(ifp)->win_width, OGL(ifp)->win_height);
		glClearColor(0,0,0,0);
		glClear(GL_COLOR_BUFFER_BIT);

		/* Set normal viewport size to minimum of actual window
		 * size and requested framebuffer size
		 */
		OGL(ifp)->vp_width = (OGL(ifp)->win_width < ifp->if_width) ?
				OGL(ifp)->win_width : ifp->if_width;
		OGL(ifp)->vp_height = (OGL(ifp)->win_height < ifp->if_height) ?
				OGL(ifp)->win_height : ifp->if_height;
		ifp->if_xcenter = OGL(ifp)->vp_width/2;
		ifp->if_ycenter = OGL(ifp)->vp_height/2;

		/* center viewport in window */
		SGI(ifp)->mi_xoff=(OGL(ifp)->win_width-OGL(ifp)->vp_width)/2;
		SGI(ifp)->mi_yoff=(OGL(ifp)->win_height-OGL(ifp)->vp_height)/2;
		glViewport(SGI(ifp)->mi_xoff,
			   SGI(ifp)->mi_yoff,
			   OGL(ifp)->vp_width,
			   OGL(ifp)->vp_height);
		/* initialize clipping planes and zoom */
		ogl_clipper(ifp);
		clp = &(OGL(ifp)->clip);
		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
		glOrtho( clp->oleft, clp->oright, clp->obottom, clp->otop,
				-1.0,1.0);
		glPixelZoom((float) ifp->if_xzoom,(float) ifp->if_yzoom);
	} else if( (OGL(ifp)->win_width > ifp->if_width) ||
		   (OGL(ifp)->win_height > ifp->if_height) ) {
		/* clear whole buffer if window larger than framebuffer */
	 	if( OGL(ifp)->copy_flag && !OGL(ifp)->front_flag ) {
	 		glDrawBuffer(GL_FRONT);
			glViewport(0, 0, OGL(ifp)->win_width,
				   OGL(ifp)->win_height);
			glClearColor(0,0,0,0);
			glClear(GL_COLOR_BUFFER_BIT);
	 		glDrawBuffer(GL_BACK);
	 	} else {
			glViewport(0, 0, OGL(ifp)->win_width,
				   OGL(ifp)->win_height);
			glClearColor(0,0,0,0);
			glClear(GL_COLOR_BUFFER_BIT);
	 	}
	 	/* center viewport */
		glViewport(SGI(ifp)->mi_xoff,
			   SGI(ifp)->mi_yoff,
			   OGL(ifp)->vp_width,
			   OGL(ifp)->vp_height);
	}

	/* repaint entire image */
	ogl_xmit_scanlines( ifp, 0, ifp->if_height, 0, ifp->if_width );
	if( SGI(ifp)->mi_doublebuffer ) {
		SwapBuffers( OGL(ifp)->hdc);
	} else if( OGL(ifp)->copy_flag ) {
		backbuffer_to_screen(ifp,-1);
	}

	if( CJDEBUG ) {
		int dbb,db,view[4],getster,getaux;
		glGetIntegerv(GL_VIEWPORT, view);
		glGetIntegerv(GL_DOUBLEBUFFER,&dbb);
		glGetIntegerv(GL_DRAW_BUFFER,&db);
		fb_log("Viewport: x %d y %d width %d height %d\n",view[0],
		       view[1],view[2],view[3]);
		fb_log("expose: double buffered: %d, draw buffer %d\n",dbb,db);
		fb_log("front %d\tback%d\n",GL_FRONT,GL_BACK);
		glGetIntegerv(GL_STEREO,&getster);
		glGetIntegerv(GL_AUX_BUFFERS,&getaux);
		fb_log("double %d, stereo %d, aux %d\n",dbb,getster,getaux);
	}

	if( multiple_windows ) {
		/* unattach context for other threads to use */
		wglMakeCurrent(OGL(ifp)->hdc,OGL(ifp)->glxc);
	}

#if 0
	XFlush(OGL(ifp)->dispp);
	glFlush();
#endif
}

void
ogl_configureWindow(FBIO *ifp,
		    int width,
		    int height)
{
  if(width == OGL(ifp)->win_width &&
     height == OGL(ifp)->win_height)
    return;

  ifp->if_width = ifp->if_max_width = width;
  ifp->if_height = ifp->if_max_height = height;

  OGL(ifp)->win_width = OGL(ifp)->vp_width = width;
  OGL(ifp)->win_height = OGL(ifp)->vp_height = height;

  ifp->if_zoomflag = 0;
  ifp->if_xzoom = 1;
  ifp->if_yzoom = 1;
  ifp->if_xcenter = width/2;
  ifp->if_ycenter = height/2;

  ogl_getmem(ifp);
  ogl_clipper(ifp);
}

#if 0
/* reorder_cursor - reverses the order of the scanlines.
 * scanlines are byte aligned, the specified cursor is xbits
 * by ybits bits in size.
 *
 */
_LOCAL_ void
reorder_cursor(char *dst,
	       char *src,
	       int xbits,
	       int ybits)
{
	int xbytes;
	int i,j,k;

	if( (xbytes = xbits /8) * 8 != xbits)
		xbytes++;

	for (j=0, k=(ybits-1)*xbytes; j < ybits*xbytes; j+=xbytes, k-=xbytes){
		for (i=0; i < xbytes; i++){
			dst[j+i] = src[k+i];
		}
	}

}
#endif

/* BACKBUFFER_TO_SCREEN - copy pixels from copy on the backbuffer
 * to the front buffer. Do one scanline specified by one_y, or whole
 * screen if one_y equals -1.
 */
_LOCAL_ void
backbuffer_to_screen(register FBIO *ifp,
		     int one_y)
{
	struct ogl_clip *clp;

	if (!(OGL(ifp)->front_flag)){
		OGL(ifp)->front_flag = 1;
		glDrawBuffer(GL_FRONT);
		glMatrixMode(GL_PROJECTION);
		glPopMatrix();
		glPixelZoom((float) ifp->if_xzoom,(float) ifp->if_yzoom);
	}

	clp = &(OGL(ifp)->clip);

	if (one_y > clp->ypixmax) {
		return;
	} else if (one_y < 0) { /* do whole visible screen */

		/* Blank out area left of image */
		glColor3b( 0, 0, 0 );
		if( clp->xscrmin < 0 )  glRecti(
			clp->xscrmin - CLIP_XTRA,
			clp->yscrmin - CLIP_XTRA,
			CLIP_XTRA,
			clp->yscrmax + CLIP_XTRA);

		/* Blank out area below image */
		if( clp->yscrmin < 0 )  glRecti(
			clp->xscrmin - CLIP_XTRA,
			clp->yscrmin - CLIP_XTRA,
			clp->xscrmax + CLIP_XTRA,
			CLIP_XTRA);

		/* We are in copy mode, so we use vp_width rather
		 * than if_width
		 */
		/* Blank out area right of image */
		if( clp->xscrmax >= OGL(ifp)->vp_width )  glRecti(
			ifp->if_width - CLIP_XTRA,
			clp->yscrmin - CLIP_XTRA,
			clp->xscrmax + CLIP_XTRA,
			clp->yscrmax + CLIP_XTRA);

		/* Blank out area above image */
		if( clp->yscrmax >= OGL(ifp)->vp_height )  glRecti(
			clp->xscrmin - CLIP_XTRA,
			OGL(ifp)->vp_height - CLIP_XTRA,
			clp->xscrmax + CLIP_XTRA,
			clp->yscrmax + CLIP_XTRA);

		/* copy image from backbuffer */
		glRasterPos2i(clp->xpixmin,clp->ypixmin);
		glCopyPixels(SGI(ifp)->mi_xoff + clp->xpixmin,
			SGI(ifp)->mi_yoff + clp->ypixmin,
			clp->xpixmax - clp->xpixmin +1,
			clp->ypixmax - clp->ypixmin +1,
			GL_COLOR);


	} else if (one_y < clp->ypixmin) {
		return;
	} else { /* draw one scanline */
		glRasterPos2i(clp->xpixmin,one_y);
		glCopyPixels(SGI(ifp)->mi_xoff + clp->xpixmin,
			SGI(ifp)->mi_yoff + one_y,
			clp->xpixmax - clp->xpixmin +1,
			1,
			GL_COLOR);
	}
}

/* 		O G L _ C H O O S E _ V I S U A L
 *
 * Select an appropriate visual, and set flags.
 *
 * The user requires support for:
 *    	-OpenGL rendering in RGBA mode
 *
 * The user may desire support for:
 *	-a single-buffered OpenGL context
 *	-a double-buffered OpenGL context
 *	-hardware colormapping (DirectColor)
 *
 * We first try to satisfy all requirements and desires. If that fails,
 * we remove the desires one at a time until we succeed or until only
 * requirements are left. If at any stage more than one visual meets the
 * current criteria, the visual with the greatest depth is chosen.
 *
 * The following flags are set:
 * 	SGI(ifp)->mi_doublebuffer
 *	OGL(ifp)->soft_cmap_flag
 *
 * Return NULL on failure.
 */
_LOCAL_ PIXELFORMATDESCRIPTOR *
ogl_choose_visual(FBIO *ifp)
{
	int iPixelFormat ;
    PIXELFORMATDESCRIPTOR pfd, *ppfd;
	BOOL good;

	ppfd = &pfd;

/*  ppfd = (PIXELFORMATDESCRIPTOR *)malloc(sizeof(PIXELFORMATDESCRIPTOR)); */
  iPixelFormat  = GetPixelFormat(OGL(ifp)->hdc);
  ppfd->nSize = sizeof(PIXELFORMATDESCRIPTOR);
  ppfd->nVersion = 1;
  ppfd->dwFlags =  PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER | PFD_TYPE_RGBA | PFD_STEREO;
  ppfd->iPixelType = PFD_TYPE_RGBA;
  ppfd->cColorBits = 24;
  ppfd->cRedBits = 0;
  ppfd->cRedShift = 0;
  ppfd->cGreenBits = 0;
  ppfd->cGreenShift = 0;
  ppfd->cBlueBits = 0;
  ppfd->cBlueShift = 0;
  ppfd->cAlphaBits = 0;
  ppfd->cAlphaShift = 0;
  ppfd->cAccumBits = 0;
  ppfd->cAccumRedBits = 0;
  ppfd->cAccumGreenBits = 0;
  ppfd->cAccumBlueBits = 0;
  ppfd->cAccumAlphaBits = 0;
  ppfd->cDepthBits = 32;
  ppfd->cStencilBits = 0;
  ppfd->cAuxBuffers = 0 ;
  ppfd->iLayerType = PFD_MAIN_PLANE;
  ppfd->bReserved = 0;
  ppfd->dwLayerMask = 0;
  ppfd->dwVisibleMask = 0;
  ppfd->dwDamageMask = 0;

  iPixelFormat = ChoosePixelFormat(OGL(ifp)->hdc, ppfd);
  good = SetPixelFormat(OGL(ifp)->hdc, iPixelFormat, ppfd);


  SGI(ifp)->mi_doublebuffer = 1;
  OGL(ifp)->soft_cmap_flag = 0;

  if(good) return ppfd;
  else return (PIXELFORMATDESCRIPTOR *)NULL;
}

int
ogl_refresh(FBIO *ifp,
	    int x,
	    int y,
	    int w,
	    int h)
{
  int mm;
  struct ogl_clip *clp;

  if(w < 0){
    w = -w;
    x -= w;
  }

  if(h < 0){
    h = -h;
    y -= h;
  }

#if 0
  if(glIsEnabled(GL_DEPTH_TEST)){
    glDisable(GL_DEPTH_TEST);
    dflag = 1;
  }
#endif

  glGetIntegerv(GL_MATRIX_MODE, &mm);
  glMatrixMode(GL_PROJECTION);
  glPushMatrix();
  glLoadIdentity();

#if 0
  glOrtho(0.0, OGL(ifp)->win_width, 0.0, OGL(ifp)->win_height, -1.0, 1.0);
#else
  ogl_clipper(ifp);
  clp = &(OGL(ifp)->clip);
  glOrtho( clp->oleft, clp->oright, clp->obottom, clp->otop, -1.0, 1.0);
  glPixelZoom((float) ifp->if_xzoom,(float) ifp->if_yzoom);
#endif
  glMatrixMode(GL_MODELVIEW);
  glPushMatrix();
  glLoadIdentity();
#if 0
  glTranslatef(0.0, 0.0, -1.0);
#endif
  glViewport(0, 0, OGL(ifp)->win_width, OGL(ifp)->win_height);
  ogl_xmit_scanlines(ifp, y, h, x, w);
  glMatrixMode(GL_PROJECTION);
  glPopMatrix();
  glMatrixMode(GL_MODELVIEW);
  glPopMatrix();
  glMatrixMode(mm);

#if 0
  if(dflag)
    glEnable(GL_DEPTH_TEST);
#endif

  glFlush();
  return 0;
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
