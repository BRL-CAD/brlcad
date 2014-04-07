/*                      I F _ O S G . C P P
 * BRL-CAD
 *
 * Copyright (c) 2004-2014 United States Government as represented by
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
/** @file if_osg.cpp
 *
 * Frame Buffer Library interface for OpenSceneGraph.
 *
 * There are several different Frame Buffer modes supported.  Set your
 * environment FB_FILE to the appropriate type.
 *
 * (see the modeflag definitions below).  /dev/osg[options]
 *
 * This code is basically a port of the 4d Framebuffer interface from
 * IRIS GL to OpenGL.
 *
 */
/** @} */

#include "common.h"

#ifdef IF_OSG

#ifdef HAVE_SYS_TYPES_H
#  include <sys/types.h>
#endif

#ifdef HAVE_SYS_SHM_H
#  include <sys/ipc.h>
#  include <sys/shm.h>
#endif

#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <signal.h>
#include <errno.h>


#include <osg/GraphicsContext>
#include <osg/Timer>

#include <osgViewer/Viewer>
#include <osgViewer/ViewerEventHandlers>

#if defined(_WIN32)
#  include <osgViewer/api/Win32/GraphicsWindowWin32>
#else
#  include <osgViewer/api/X11/GraphicsWindowX11>
#endif

#include <osgText/Font>
#include <osgText/Text>

#ifdef HAVE_UNISTD_H
#  include <unistd.h>	/* for getpagesize and sysconf */
#endif

#ifdef HAVE_SYS_WAIT_H
#  include <sys/wait.h>
#endif
#include "tcl.h"
#include "tk.h"
#include "tkPlatDecls.h"
#include "bio.h"

#include "bu.h"
extern "C" {
#include "fb.h"
}


#define CJDEBUG 0
#define DIRECT_COLOR_VISUAL_ALLOWED 0

/* XXX - arbitrary upper bound */
#define XMAXSCREEN 16383
#define YMAXSCREEN 16383

HIDDEN int osg_nwindows = 0; 	/* number of open windows */
HIDDEN XColor osg_color_cell[256];		/* used to set colormap */


/*
 * Structure of color map in shared memory region.  Has exactly the
 * same format as the SGI hardware "gammaramp" map Note that only the
 * lower 8 bits are significant.
 */
struct osg_cmap {
    short cmr[256];
    short cmg[256];
    short cmb[256];
};


/*
 * This defines the format of the in-memory framebuffer copy.  The
 * alpha component and reverse order are maintained for compatibility
 * with /dev/sgi
 */
struct osg_pixel {
    unsigned char blue;
    unsigned char green;
    unsigned char red;
    unsigned char alpha;
};


/* Clipping structure for zoom/pan operations */
struct osg_clip {
    int xpixmin;	/* view clipping planes clipped to pixel memory space*/
    int xpixmax;
    int ypixmin;
    int ypixmax;
    int xscrmin;	/* view clipping planes */
    int xscrmax;
    int yscrmin;
    int yscrmax;
    double oleft;	/* glOrtho parameters */
    double oright;
    double otop;
    double obottom;

};


/*
 * Per window state information, overflow area.
 */
struct sgiinfo {
    short mi_curs_on;
    short mi_cmap_flag;		/* enabled when there is a non-linear map in memory */
    int mi_shmid;
    int mi_memwidth;		/* width of scanline in if_mem */
    short mi_xoff;		/* X viewport offset, rel. window*/
    short mi_yoff;		/* Y viewport offset, rel. window*/
    int mi_pid;			/* for multi-cpu check */
    int mi_parent;		/* PID of linger-mode process */
    int mi_doublebuffer;	/* 0=singlebuffer 1=doublebuffer */
    struct osg_pixel mi_scanline[XMAXSCREEN+1];	/* one scanline */
};


/*
 * Per window state information particular to the OpenGL interface
 */
struct osginfo {
    osg::ref_ptr<osg::GraphicsContext> graphicsContext;
    Display *dispp;		/* pointer to X display connection */
    Window wind;		/* Window identifier */
    Tcl_Interp *fbinterp;
    Tk_Window xtkwin;		/* Window identifier */
    int firstTime;
    int alive;
    long event_mask;		/* event types to be received */
    short front_flag;		/* front buffer being used (b-mode) */
    short copy_flag;		/* pan and zoom copied from backbuffer */
    short soft_cmap_flag;	/* use software colormapping */
    int cmap_size;		/* hardware colormap size */
    int win_width;		/* actual window width */
    int win_height;		/* actual window height */
    int vp_width;		/* actual viewport width */
    int vp_height;		/* actual viewport height */
    struct osg_clip clip;	/* current view clipping */
    Window cursor;
    Colormap xcmap;		/* xstyle color map */
    int use_ext_ctrl;		/* for controlling the Ogl graphics engine externally */
};


#define SGI(ptr) ((struct sgiinfo *)((ptr)->u1.p))
#define SGIL(ptr) ((ptr)->u1.p)	/* left hand side version */
#define OSG(ptr) ((struct osginfo *)((ptr)->u6.p))
#define OSGL(ptr) ((ptr)->u6.p)	/* left hand side version */
#define if_mem u2.p		/* shared memory pointer */
#define if_cmap u3.p		/* color map in shared memory */
#define CMR(x) ((struct osg_cmap *)((x)->if_cmap))->cmr
#define CMG(x) ((struct osg_cmap *)((x)->if_cmap))->cmg
#define CMB(x) ((struct osg_cmap *)((x)->if_cmap))->cmb
#define if_zoomflag u4.l	/* zoom > 1 */
#define if_mode u5.l		/* see MODE_* defines */

#define MARGIN 4		/* # pixels margin to screen edge */

#define CLIP_XTRA 1

#define WIN_L (ifp->if_max_width - ifp->if_width - MARGIN)
#define WIN_T (ifp->if_max_height - ifp->if_height - MARGIN)

/*
 * The mode has several independent bits:
 *
 * SHARED -vs- MALLOC'ed memory for the image
 * TRANSIENT -vs- LINGERING windows
 * Windowed -vs- Centered Full screen
 * Suppress dither -vs- dither
 * Double -vs- Single buffered
 * DrawPixels -vs- CopyPixels
 */
#define MODE_1MASK	(1<<0)
#define MODE_1SHARED	(0<<0)	/* Use Shared memory */
#define MODE_1MALLOC	(1<<0)	/* Use malloc memory */

#define MODE_2MASK	(1<<1)
#define MODE_2TRANSIENT	(0<<1)
#define MODE_2LINGERING (1<<1)	/* leave window up after closing*/

#define MODE_4MASK	(1<<3)
#define MODE_4NORMAL	(0<<3)	/* dither if it seems necessary */
#define MODE_4NODITH	(1<<3)	/* suppress any dithering */

#define MODE_7MASK	(1<<6)
#define MODE_7NORMAL	(0<<6)	/* install colormap in hardware if possible*/
#define MODE_7SWCMAP	(1<<6)	/* use software colormapping */

#define MODE_9MASK	(1<<8)
#define MODE_9NORMAL	(0<<8)	/* doublebuffer if possible */
#define MODE_9SINGLEBUF	(1<<8)	/* singlebuffer only */

#define MODE_11MASK	(1<<10)
#define MODE_11NORMAL	(0<<10)	/* always draw from mem. to window*/
#define MODE_11COPY	(1<<10)	/* keep full image on back buffer */

#define MODE_12MASK	(1<<11)
#define MODE_12NORMAL	(0<<11)
#define MODE_12DELAY_WRITES_TILL_FLUSH	(1<<11)
/* and copy current view to front */
#define MODE_15MASK	(1<<14)
#define MODE_15NORMAL	(0<<14)
#define MODE_15ZAP	(1<<14)	/* zap the shared memory segment */

HIDDEN struct osg_modeflags {
    char c;
    long mask;
    long value;
    const char *help;
} osg_modeflags[] = {
    { 'p',	MODE_1MASK, MODE_1MALLOC,
      "Private memory - else shared" },
    { 'l',	MODE_2MASK, MODE_2LINGERING,
      "Lingering window" },
    { 't',	MODE_2MASK, MODE_2TRANSIENT,
      "Transient window" },
    { 'd',  MODE_4MASK, MODE_4NODITH,
      "Suppress dithering - else dither if not 24-bit buffer" },
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


HIDDEN void
sigkid(int UNUSED(pid))
{
    exit(0);
}


/* BACKBUFFER_TO_SCREEN - copy pixels from copy on the backbuffer to
 * the front buffer. Do one scanline specified by one_y, or whole
 * screen if one_y equals -1.
 */
HIDDEN void
backbuffer_to_screen(register FBIO *ifp, int one_y)
{
    struct osg_clip *clp;

    if (!(OSG(ifp)->front_flag)) {
	OSG(ifp)->front_flag = 1;
	glDrawBuffer(GL_FRONT);
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	glPixelZoom((float) ifp->if_xzoom, (float) ifp->if_yzoom);
    }

    clp = &(OSG(ifp)->clip);

    if (one_y > clp->ypixmax) {
	return;
    } else if (one_y < 0) {
	/* do whole visible screen */

	/* Blank out area left of image */
	glColor3b(0, 0, 0);
	if (clp->xscrmin < 0) glRecti(clp->xscrmin - CLIP_XTRA,
				      clp->yscrmin - CLIP_XTRA,
				      CLIP_XTRA,
				      clp->yscrmax + CLIP_XTRA);

	/* Blank out area below image */
	if (clp->yscrmin < 0) glRecti(clp->xscrmin - CLIP_XTRA,
				      clp->yscrmin - CLIP_XTRA,
				      clp->xscrmax + CLIP_XTRA,
				      CLIP_XTRA);

	/* We are in copy mode, so we use vp_width rather
	 * than if_width
	 */
	/* Blank out area right of image */
	if (clp->xscrmax >= OSG(ifp)->vp_width) glRecti(ifp->if_width - CLIP_XTRA,
							clp->yscrmin - CLIP_XTRA,
							clp->xscrmax + CLIP_XTRA,
							clp->yscrmax + CLIP_XTRA);

	/* Blank out area above image */
	if (clp->yscrmax >= OSG(ifp)->vp_height) glRecti(clp->xscrmin - CLIP_XTRA,
							 OSG(ifp)->vp_height - CLIP_XTRA,
							 clp->xscrmax + CLIP_XTRA,
							 clp->yscrmax + CLIP_XTRA);

	/* copy image from backbuffer */
	glRasterPos2i(clp->xpixmin, clp->ypixmin);
	glCopyPixels(SGI(ifp)->mi_xoff + clp->xpixmin,
		     SGI(ifp)->mi_yoff + clp->ypixmin,
		     clp->xpixmax - clp->xpixmin +1,
		     clp->ypixmax - clp->ypixmin +1,
		     GL_COLOR);


    } else if (one_y < clp->ypixmin) {
	return;
    } else {
	/* draw one scanline */
	glRasterPos2i(clp->xpixmin, one_y);
	glCopyPixels(SGI(ifp)->mi_xoff + clp->xpixmin,
		     SGI(ifp)->mi_yoff + one_y,
		     clp->xpixmax - clp->xpixmin +1,
		     1,
		     GL_COLOR);
    }
}


/*
 * Note: unlike sgi_xmit_scanlines, this function updates an arbitrary
 * rectangle of the frame buffer
 */
HIDDEN void
osg_xmit_scanlines(register FBIO *ifp, int ybase, int nlines, int xbase, int npix)
{
    register int y;
    register int n;
    int sw_cmap;	/* !0 => needs software color map */
    struct osg_clip *clp;

    /* Caller is expected to handle attaching context, etc. */

    clp = &(OSG(ifp)->clip);

    if (OSG(ifp)->soft_cmap_flag  && SGI(ifp)->mi_cmap_flag) {
	sw_cmap = 1;
    } else {
	sw_cmap = 0;
    }

    if (xbase > clp->xpixmax || ybase > clp->ypixmax)
	return;
    if (xbase < clp->xpixmin)
	xbase = clp->xpixmin;
    if (ybase < clp->ypixmin)
	ybase = clp->ypixmin;

    if ((xbase + npix -1) > clp->xpixmax)
	npix = clp->xpixmax - xbase + 1;
    if ((ybase + nlines - 1) > clp->ypixmax)
	nlines = clp->ypixmax - ybase + 1;

    if (!OSG(ifp)->use_ext_ctrl) {
	if (!OSG(ifp)->copy_flag) {
	    /*
	     * Blank out areas of the screen around the image, if
	     * exposed.  In COPY mode, this is done in
	     * backbuffer_to_screen().
	     */

	    /* Blank out area left of image */
	    glColor3b(0, 0, 0);
	    if (clp->xscrmin < 0) glRecti(clp->xscrmin - CLIP_XTRA,
					  clp->yscrmin - CLIP_XTRA,
					  CLIP_XTRA,
					  clp->yscrmax + CLIP_XTRA);

	    /* Blank out area below image */
	    if (clp->yscrmin < 0) glRecti(clp->xscrmin - CLIP_XTRA,
					  clp->yscrmin - CLIP_XTRA,
					  clp->xscrmax + CLIP_XTRA,
					  CLIP_XTRA);

	    /* Blank out area right of image */
	    if (clp->xscrmax >= ifp->if_width) glRecti(ifp->if_width - CLIP_XTRA,
						       clp->yscrmin - CLIP_XTRA,
						       clp->xscrmax + CLIP_XTRA,
						       clp->yscrmax + CLIP_XTRA);

	    /* Blank out area above image */
	    if (clp->yscrmax >= ifp->if_height) glRecti(clp->xscrmin - CLIP_XTRA,
							ifp->if_height- CLIP_XTRA,
							clp->xscrmax + CLIP_XTRA,
							clp->yscrmax + CLIP_XTRA);

	} else if (OSG(ifp)->front_flag) {
	    /* in COPY mode, always draw full sized image into backbuffer.
	     * backbuffer_to_screen() is used to update the front buffer
	     */
	    glDrawBuffer(GL_BACK);
	    OSG(ifp)->front_flag = 0;
	    glMatrixMode(GL_PROJECTION);
	    glPushMatrix();	/* store current view clipping matrix*/
	    glLoadIdentity();
	    glOrtho(-0.25, ((GLdouble) OSG(ifp)->vp_width)-0.25,
		    -0.25, ((GLdouble) OSG(ifp)->vp_height)-0.25,
		    -1.0, 1.0);
	    glPixelZoom(1.0, 1.0);
	}
    }

    if (sw_cmap) {
	/* Software colormap each line as it's transmitted */
	register int x;
	register struct osg_pixel *osgp;
	register struct osg_pixel *op;

	y = ybase;
	if (CJDEBUG) printf("Doing sw colormap xmit\n");
	/* Perform software color mapping into temp scanline */
	op = SGI(ifp)->mi_scanline;
	for (n=nlines; n>0; n--, y++) {
	    osgp = (struct osg_pixel *)&ifp->if_mem[
		(y*SGI(ifp)->mi_memwidth)*
		sizeof(struct osg_pixel) ];
	    for (x=xbase+npix-1; x>=xbase; x--) {
		op[x].red   = CMR(ifp)[osgp[x].red];
		op[x].green = CMG(ifp)[osgp[x].green];
		op[x].blue  = CMB(ifp)[osgp[x].blue];
	    }

	    glPixelStorei(GL_UNPACK_SKIP_PIXELS, xbase);
	    glRasterPos2i(xbase, y);
	    glDrawPixels(npix, 1, GL_BGRA_EXT, GL_UNSIGNED_BYTE,
			 (const GLvoid *) op);

	}

    } else {
	/* No need for software colormapping */

	glPixelStorei(GL_UNPACK_ROW_LENGTH, SGI(ifp)->mi_memwidth);
	glPixelStorei(GL_UNPACK_SKIP_PIXELS, xbase);
	glPixelStorei(GL_UNPACK_SKIP_ROWS, ybase);

	glRasterPos2i(xbase, ybase);
	glDrawPixels(npix, nlines, GL_BGRA_EXT, GL_UNSIGNED_BYTE,
		     (const GLvoid *) ifp->if_mem);
    }
}


HIDDEN void
osg_cminit(register FBIO *ifp)
{
    register int i;

    for (i = 0; i < 256; i++) {
	CMR(ifp)[i] = i;
	CMG(ifp)[i] = i;
	CMB(ifp)[i] = i;
    }
}


/************************************************************************/
/******************* Shared Memory Support ******************************/
/************************************************************************/

/**
 * not changed from sgi_getmem.
 *
 * Because there is no hardware zoom or pan, we need to repaint the
 * screen (with big pixels) to implement these operations.  This means
 * that the actual "contents" of the frame buffer need to be stored
 * somewhere else.  If possible, we allocate a shared memory segment
 * to contain that image.  This has several advantages, the most
 * important being that when operating the display in 12-bit output
 * mode, pixel-readbacks still give the full 24-bits of color.  System
 * V shared memory persists until explicitly killed, so this also
 * means that in MEX mode, the previous contents of the frame buffer
 * still exist, and can be again accessed, even though the MEX windows
 * are transient, per-process.
 *
 * There are a few oddities, however.  The worst is that System V will
 * not allow the break (see sbrk(2)) to be set above a shared memory
 * segment, and shmat(2) does not seem to allow the selection of any
 * reasonable memory address (like 6 Mbytes up) for the shared memory.
 * In the initial version of this routine, that prevented subsequent
 * calls to malloc() from succeeding, quite a drawback.  The
 * work-around used here is to increase the current break to a large
 * value, attach to the shared memory, and then return the break to
 * its original value.  This should allow most reasonable requests for
 * memory to be satisfied.  In special cases, the values used here
 * might need to be increased.
 */
HIDDEN int
osg_getmem(FBIO *ifp)
{
#define SHMEM_KEY 42
    int pixsize;
    int size;
#if defined(HAVE_SYSCONF) && defined(HAVE_SHMDT) && defined(HAVE_SHMCTL) && defined(HAVE_SHMGET) && defined(HAVE_SHMAT)
    long psize = sysconf(_SC_PAGESIZE);
#else
    long psize = -1;
#endif
    int i;
    char *sp;
    int new_mem = 0;

    errno = 0;

    if ((ifp->if_mode & MODE_1MASK) == MODE_1MALLOC || psize == -1) {
	/*
	 * In this mode, only malloc as much memory as is needed.
	 */
	SGI(ifp)->mi_memwidth = ifp->if_width;
	pixsize = ifp->if_height * ifp->if_width * sizeof(struct osg_pixel);
	size = pixsize + sizeof(struct osg_cmap);

	sp = (char *)calloc(1, size);
	if (sp == 0) {
	    fb_log("osg_getmem: frame buffer memory malloc failed\n");
	    goto fail;
	}
	new_mem = 1;
	goto success;
    }

#if defined(HAVE_SYSCONF) && defined(HAVE_SHMDT) && defined(HAVE_SHMCTL) && defined(HAVE_SHMGET) && defined(HAVE_SHMAT)
    /* The shared memory section never changes size */
    SGI(ifp)->mi_memwidth = ifp->if_max_width;

    /*
     * On some platforms lrectwrite() runs off the end!  So, provide a
     * pad area of 2 scanlines.  (1 line is enough, but this avoids
     * risk of damage to colormap table.)
     */
    pixsize = (ifp->if_max_height+2) * ifp->if_max_width *
	sizeof(struct osg_pixel);

    size = pixsize + sizeof(struct osg_cmap);

    /* make more portable
    size = (size + getpagesize()-1) & ~(getpagesize()-1);
    */
    size = (size + psize - 1) & ~(psize - 1);

    /* First try to attach to an existing one */
    if ((SGI(ifp)->mi_shmid = shmget(SHMEM_KEY, size, 0)) < 0) {
	/* No existing one, create a new one */
	if ((SGI(ifp)->mi_shmid = shmget(
		 SHMEM_KEY, size, IPC_CREAT|0666)) < 0) {
	    fb_log("osg_getmem: shmget failed, errno=%d\n", errno);
	    goto fail;
	}
	new_mem = 1;
    }

    /* WWW this is unnecessary in this version? */
    /* Open the segment Read/Write */
    /* This gets mapped to a high address on some platforms, so no problem. */
    if ((sp = (char *)shmat(SGI(ifp)->mi_shmid, 0, 0)) == (char *)(-1L)) {
	fb_log("osg_getmem: shmat returned x%x, errno=%d\n", sp, errno);
	goto fail;
    }
#endif

success:
    ifp->if_mem = sp;
    ifp->if_cmap = sp + pixsize;	/* cmap at end of area */
    i = CMB(ifp)[255];		/* try to deref last word */
    CMB(ifp)[255] = i;

    /* Provide non-black colormap on creation of new shared mem */
    if (new_mem)
	osg_cminit(ifp);
    return 0;
fail:
    fb_log("osg_getmem:  Unable to attach to shared memory.\n");
    if ((sp = (char *)calloc(1, size)) == NULL) {
	fb_log("osg_getmem:  malloc failure\n");
	return -1;
    }
    new_mem = 1;
    goto success;
}


void
osg_zapmem(void)
{
#if defined(HAVE_SYSCONF) && defined(HAVE_SHMDT) && defined(HAVE_SHMCTL) && defined(HAVE_SHMGET) && defined(HAVE_SHMAT)
    int i;
    int shmid;
    if ((shmid = shmget(SHMEM_KEY, 0, 0)) < 0) {
	fb_log("osg_zapmem shmget failed, errno=%d\n", errno);
	return;
    }

    i = shmctl(shmid, IPC_RMID, 0);
    if (i < 0) {
	fb_log("osg_zapmem shmctl failed, errno=%d\n", errno);
	return;
    }
#endif
    fb_log("if_osg: shared memory released\n");
}


/**
 * Given:- the size of the viewport in pixels (vp_width, vp_height)
 *	 - the size of the framebuffer image (if_width, if_height)
 *	 - the current view center (if_xcenter, if_ycenter)
 * 	 - the current zoom (if_xzoom, if_yzoom)
 * Calculate:
 *	 - the position of the viewport in image space
 *		(xscrmin, xscrmax, yscrmin, yscrmax)
 *	 - the portion of the image which is visible in the viewport
 *		(xpixmin, xpixmax, ypixmin, ypixmax)
 */
void
osg_clipper(register FBIO *ifp)
{
    register struct osg_clip *clp;
    register int i;
    double pixels;

    clp = &(OSG(ifp)->clip);

    i = OSG(ifp)->vp_width/(2*ifp->if_xzoom);
    clp->xscrmin = ifp->if_xcenter - i;
    i = OSG(ifp)->vp_width/ifp->if_xzoom;
    clp->xscrmax = clp->xscrmin + i;
    pixels = (double) i;
    clp->oleft = ((double) clp->xscrmin) - 0.25*pixels/((double) OSG(ifp)->vp_width);
    clp->oright = clp->oleft + pixels;

    i = OSG(ifp)->vp_height/(2*ifp->if_yzoom);
    clp->yscrmin = ifp->if_ycenter - i;
    i = OSG(ifp)->vp_height/ifp->if_yzoom;
    clp->yscrmax = clp->yscrmin + i;
    pixels = (double) i;
    clp->obottom = ((double) clp->yscrmin) - 0.25*pixels/((double) OSG(ifp)->vp_height);
    clp->otop = clp->obottom + pixels;

    clp->xpixmin = clp->xscrmin;
    clp->xpixmax = clp->xscrmax;
    clp->ypixmin = clp->yscrmin;
    clp->ypixmax = clp->yscrmax;

    if (clp->xpixmin < 0) {
	clp->xpixmin = 0;
    }

    if (clp->ypixmin < 0) {
	clp->ypixmin = 0;
    }

    /* In copy mode, the backbuffer copy image is limited
     * to the viewport size; use that for clipping.
     * Otherwise, use size of framebuffer memory segment
     */
    if (OSG(ifp)->copy_flag) {
	if (clp->xpixmax > OSG(ifp)->vp_width-1) {
	    clp->xpixmax = OSG(ifp)->vp_width-1;
	}
	if (clp->ypixmax > OSG(ifp)->vp_height-1) {
	    clp->ypixmax = OSG(ifp)->vp_height-1;
	}
    } else {
	if (clp->xpixmax > ifp->if_width-1) {
	    clp->xpixmax = ifp->if_width-1;
	}
	if (clp->ypixmax > ifp->if_height-1) {
	    clp->ypixmax = ifp->if_height-1;
	}
    }

}


HIDDEN void
expose_callback(FBIO *ifp)
{
    struct osg_clip *clp;

    if (CJDEBUG) fb_log("entering expose_callback()\n");
    OSG(ifp)->graphicsContext->makeCurrent();
    if (OSG(ifp)->firstTime) {

	OSG(ifp)->firstTime = 0;

	/* just in case the configuration is double buffered but
	 * we want to pretend it's not
	 */

	if (!SGI(ifp)->mi_doublebuffer) {
	    glDrawBuffer(GL_FRONT);
	}

	if ((ifp->if_mode & MODE_4MASK) == MODE_4NODITH) {
	    glDisable(GL_DITHER);
	}

	/* set copy mode if possible and requested */
	if (SGI(ifp)->mi_doublebuffer &&
	    ((ifp->if_mode & MODE_11MASK)==MODE_11COPY)) {
	    /* Copy mode only works if there are two
	     * buffers to use. It conflicts with
	     * double buffering
	     */
	    OSG(ifp)->copy_flag = 1;
	    SGI(ifp)->mi_doublebuffer = 0;
	    OSG(ifp)->front_flag = 1;
	    glDrawBuffer(GL_FRONT);
	} else {
	    OSG(ifp)->copy_flag = 0;
	}

	OSG(ifp)->win_width = Tk_Width(OSG(ifp)->xtkwin);
	OSG(ifp)->win_height = Tk_Height(OSG(ifp)->xtkwin);

	/* clear entire window */
	glViewport(0, 0, OSG(ifp)->win_width, OSG(ifp)->win_height);
	glClearColor(0, 0, 0, 0);
	glClear(GL_COLOR_BUFFER_BIT);

	/* Set normal viewport size to minimum of actual window
	 * size and requested framebuffer size
	 */
	OSG(ifp)->vp_width = (OSG(ifp)->win_width < ifp->if_width) ?
	    OSG(ifp)->win_width : ifp->if_width;
	OSG(ifp)->vp_height = (OSG(ifp)->win_height < ifp->if_height) ?
	    OSG(ifp)->win_height : ifp->if_height;
	ifp->if_xcenter = OSG(ifp)->vp_width/2;
	ifp->if_ycenter = OSG(ifp)->vp_height/2;

	/* center viewport in window */
	SGI(ifp)->mi_xoff=(OSG(ifp)->win_width-OSG(ifp)->vp_width)/2;
	SGI(ifp)->mi_yoff=(OSG(ifp)->win_height-OSG(ifp)->vp_height)/2;
	glViewport(SGI(ifp)->mi_xoff,
		   SGI(ifp)->mi_yoff,
		   OSG(ifp)->vp_width,
		   OSG(ifp)->vp_height);
	/* initialize clipping planes and zoom */
	osg_clipper(ifp);
	clp = &(OSG(ifp)->clip);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(clp->oleft, clp->oright, clp->obottom, clp->otop,
		-1.0, 1.0);
	glPixelZoom((float) ifp->if_xzoom, (float) ifp->if_yzoom);
    } else if ((OSG(ifp)->win_width > ifp->if_width) ||
	       (OSG(ifp)->win_height > ifp->if_height)) {
	/* clear whole buffer if window larger than framebuffer */
	if (OSG(ifp)->copy_flag && !OSG(ifp)->front_flag) {
	    glDrawBuffer(GL_FRONT);
	    glViewport(0, 0, OSG(ifp)->win_width,
		       OSG(ifp)->win_height);
	    glClearColor(0, 0, 0, 0);
	    glClear(GL_COLOR_BUFFER_BIT);
	    glDrawBuffer(GL_BACK);
	} else {
	    glViewport(0, 0, OSG(ifp)->win_width,
		       OSG(ifp)->win_height);
	    glClearColor(0, 0, 0, 0);
	    glClear(GL_COLOR_BUFFER_BIT);
	}
	/* center viewport */
	glViewport(SGI(ifp)->mi_xoff,
		   SGI(ifp)->mi_yoff,
		   OSG(ifp)->vp_width,
		   OSG(ifp)->vp_height);
    }

    /* repaint entire image */
    osg_xmit_scanlines(ifp, 0, ifp->if_height, 0, ifp->if_width);

    if (SGI(ifp)->mi_doublebuffer) {
	OSG(ifp)->graphicsContext->swapBuffers();
    } else if (OSG(ifp)->copy_flag) {
	backbuffer_to_screen(ifp, -1);
    }

    if (CJDEBUG) {
	int dbb, db, view[4], getster, getaux;
	glGetIntegerv(GL_VIEWPORT, view);
	glGetIntegerv(GL_DOUBLEBUFFER, &dbb);
	glGetIntegerv(GL_DRAW_BUFFER, &db);
	fb_log("Viewport: x %d y %d width %d height %d\n", view[0],
	       view[1], view[2], view[3]);
	fb_log("expose: double buffered: %d, draw buffer %d\n", dbb, db);
	fb_log("front %d\tback%d\n", GL_FRONT, GL_BACK);
	glGetIntegerv(GL_STEREO, &getster);
	glGetIntegerv(GL_AUX_BUFFERS, &getaux);
	fb_log("double %d, stereo %d, aux %d\n", dbb, getster, getaux);
    }

    /* unattach context for other threads to use */
    OSG(ifp)->graphicsContext->releaseContext();
}


extern "C" void
osg_configureWindow(FBIO *ifp, int width, int height)
{
    if (width == OSG(ifp)->win_width &&
	height == OSG(ifp)->win_height)
	return;

    ifp->if_width = ifp->if_max_width = width;
    ifp->if_height = ifp->if_max_height = height;

    OSG(ifp)->win_width = OSG(ifp)->vp_width = width;
    OSG(ifp)->win_height = OSG(ifp)->vp_height = height;

    ifp->if_zoomflag = 0;
    ifp->if_xzoom = 1;
    ifp->if_yzoom = 1;
    ifp->if_xcenter = width/2;
    ifp->if_ycenter = height/2;

    osg_getmem(ifp);
    osg_clipper(ifp);
}


HIDDEN void
osg_do_event(FBIO *ifp)
{
    Tcl_DoOneEvent(TCL_ALL_EVENTS|TCL_DONT_WAIT);

    if (BU_STR_EQUAL(Tcl_GetVar(OSG(ifp)->fbinterp, "WM_DELETE_WINDOW", 0), "1")) {
	OSG(ifp)->alive = 0;
	printf("Close Window event\n");
    }

#if 0
    XEvent event;

    while (XCheckWindowEvent(OSG(ifp)->dispp, OSG(ifp)->wind,
			     OSG(ifp)->event_mask, &event)) {
	switch (event.type) {
	    case Expose:
		if (!OSG(ifp)->use_ext_ctrl)
		    expose_callback(ifp);
		break;
	    case ButtonPress:
		{
		    int button = (int) event.xbutton.button;
		    if (button == Button1) {
			/* Check for single button mouse remap.
			 * ctrl-1 => 2
			 * meta-1 => 3
			 * cmdkey => 3
			 */
			if (event.xbutton.state & ControlMask) {
			    button = Button2;
			} else if (event.xbutton.state & Mod1Mask) {
			    button = Button3;
			} else if (event.xbutton.state & Mod2Mask) {
			    button = Button3;
			}
		    }

		    switch (button) {
			case Button1:
			    break;
			case Button2:
			    {
				int x, y;
				register struct osg_pixel *osgp;

				x = event.xbutton.x;
				y = ifp->if_height - event.xbutton.y - 1;

				if (x < 0 || y < 0) {
				    fb_log("No RGB (outside image viewport)\n");
				    break;
				}

				osgp = (struct osg_pixel *)&ifp->if_mem[
				    (y*SGI(ifp)->mi_memwidth)*
				    sizeof(struct osg_pixel) ];

				fb_log("At image (%d, %d), real RGB=(%3d %3d %3d)\n",
				       x, y, (int)osgp[x].red, (int)osgp[x].green, (int)osgp[x].blue);

				break;
			    }
			case Button3:
			    OSG(ifp)->alive = 0;
			    break;
			default:
			    fb_log("unhandled mouse event\n");
			    break;
		    }
		    break;
		}
	    case ConfigureNotify:
		{
		    XConfigureEvent *conf = (XConfigureEvent *)&event;

		    if (conf->width == OSG(ifp)->win_width &&
			conf->height == OSG(ifp)->win_height)
			return;

		    osg_configureWindow(ifp, conf->width, conf->height);
		}
	    default:
		break;
	}
    }
#endif
    if (0 < OSG(ifp)->alive && BU_STR_EQUAL(Tcl_GetVar(OSG(ifp)->fbinterp, "WM_EXPOSE_EVENT", 0), "1")) {
	Tcl_SetVar(OSG(ifp)->fbinterp, "WM_EXPOSE_EVENT", "0", 0);
	expose_callback(ifp);
    }
}

#if 0
/**
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
 * We first try to satisfy all requirements and desires. If that
 * fails, we remove the desires one at a time until we succeed or
 * until only requirements are left. If at any stage more than one
 * visual meets the current criteria, the visual with the greatest
 * depth is chosen.
 *
 * The following flags are set:
 * SGI(ifp)->mi_doublebuffer
 * OSG(ifp)->soft_cmap_flag
 *
 * Return NULL on failure.
 */
HIDDEN XVisualInfo *
fb_osg_choose_visual(FBIO *ifp)
{

    XVisualInfo *vip, *vibase, *maxvip, _template;
#define NGOOD 200
    int good[NGOOD];
    int num, i, j;
    int m_hard_cmap, m_sing_buf, m_doub_buf;
    int use, rgba, dbfr;

    m_hard_cmap = ((ifp->if_mode & MODE_7MASK)==MODE_7NORMAL);
    m_sing_buf  = ((ifp->if_mode & MODE_9MASK)==MODE_9SINGLEBUF);
    m_doub_buf =  !m_sing_buf;

    memset((void *)&_template, 0, sizeof(XVisualInfo));

    /* get a list of all visuals on this display */
    vibase = XGetVisualInfo(OSG(ifp)->dispp, 0, &_template, &num);
    while (1) {

	/* search for all visuals matching current criteria */
	for (i = 0, j = 0, vip=vibase; i < num; i++, vip++) {
	    /* requirements */
	    glXGetConfig(OSG(ifp)->dispp, vip, GLX_USE_GL, &use);
	    if (!use) {
		continue;
	    }
	    glXGetConfig(OSG(ifp)->dispp, vip, GLX_RGBA, &rgba);
	    if (!rgba) {
		continue;
	    }
	    /* desires */
	    /* X_CreateColormap needs a DirectColor visual */
	    /* There should be some way of handling this with TrueColor,
	     * for example:
	     visual id:    0x50
	     class:    TrueColor
	     depth:    24 planes
	     available colormap entries:    256 per subfield
	     red, green, blue masks:    0xff0000, 0xff00, 0xff
	     significant bits in color specification:    8 bits
	    */
	    if ((m_hard_cmap) && (vip->class != DirectColor)) {
		continue;
	    }
	    if ((m_hard_cmap) && (vip->colormap_size < 256)) {
		continue;
	    }
	    glXGetConfig(OSG(ifp)->dispp, vip, GLX_DOUBLEBUFFER, &dbfr);
	    if ((m_doub_buf) && (!dbfr)) {
		continue;
	    }
	    if ((m_sing_buf) && (dbfr)) {
		continue;
	    }

	    /* this visual meets criteria */
	    if (j >= NGOOD-1) {
		fb_log("fb_osg_open:  More than %d candidate visuals!\n", NGOOD);
		break;
	    }
	    good[j++] = i;
	}

	/* from list of acceptable visuals,
	 * choose the visual with the greatest depth */
	if (j >= 1) {
	    maxvip = vibase + good[0];
	    for (i = 1; i < j; i++) {
		vip = vibase + good[i];
		if (vip->depth > maxvip->depth) {
		    maxvip = vip;
		}
	    }
	    /* set flags and return choice */
	    OSG(ifp)->soft_cmap_flag = !m_hard_cmap;
	    SGI(ifp)->mi_doublebuffer = m_doub_buf;
	    return maxvip;
	}

	/* if no success at this point,
	 * relax one of the criteria and try again.
	 */
	if (m_hard_cmap) {
	    /* relax hardware colormap requirement */
	    m_hard_cmap = 0;
	    fb_log("fb_osg_open: hardware colormapping not available. Using software colormap.\n");
	} else if (m_sing_buf) {
	    /* relax single buffering requirement.
	     * no need for any warning - we'll just use
	     * the front buffer
	     */
	    m_sing_buf = 0;
	} else if (m_doub_buf) {
	    /* relax double buffering requirement. */
	    m_doub_buf = 0;
	    fb_log("fb_osg_open: double buffering not available. Using single buffer.\n");
	} else {
	    /* nothing else to relax */
	    return NULL;
	}

    }

}
#endif

/**
 * Check for a color map being linear in R, G, and B.  Returns 1 for
 * linear map, 0 for non-linear map (i.e., non-identity map).
 */
HIDDEN int
is_linear_cmap(register FBIO *ifp)
{
    register int i;

    for (i = 0; i < 256; i++) {
	if (CMR(ifp)[i] != i) return 0;
	if (CMG(ifp)[i] != i) return 0;
	if (CMB(ifp)[i] != i) return 0;
    }
    return 1;
}


// make a stab at a framebuffer update handler... not clear
// this approach will work, as this logic is currently
// implemented.
class FramebufferEventHandler : public osgGA::GUIEventHandler
{
    public:
	FramebufferEventHandler(FBIO *ifp):
	    _ifp(ifp) {}

	virtual bool handle(const osgGA::GUIEventAdapter& ea,osgGA::GUIActionAdapter&)
	{
	    switch(ea.getEventType())
	    {
		case(osgGA::GUIEventAdapter::FRAME):
		    osg_do_event(_ifp);
		    return true;
		default:
		    return false;
	    }
	}

	FBIO *_ifp;
};


HIDDEN int
fb_osg_open(FBIO *ifp, const char *file, int width, int height)
{
    static char title[128];
    int mode;
    //int i, direct;
    //long valuemask;
    //XSetWindowAttributes swa;

    FB_CK_FBIO(ifp);

    /*
     * First, attempt to determine operating mode for this open,
     * based upon the "unit number" or flags.
     * file = "/dev/osg###"
     */
    mode = MODE_2LINGERING;

    if (file != NULL) {
	const char *cp;
	char modebuf[80];
	char *mp;
	int alpha;
	struct osg_modeflags *mfp;

	if (bu_strncmp(file, ifp->if_name, strlen(ifp->if_name))) {
	    /* How did this happen? */
	    mode = 0;
	} else {
	    /* Parse the options */
	    alpha = 0;
	    mp = &modebuf[0];
	    cp = &file[8];
	    while (*cp != '\0' && !isspace((int)(*cp))) {
		*mp++ = *cp;	/* copy it to buffer */
		if (isdigit((int)(*cp))) {
		    cp++;
		    continue;
		}
		alpha++;
		for (mfp = osg_modeflags; mfp->c != '\0'; mfp++) {
		    if (mfp->c == *cp) {
			mode = (mode&~mfp->mask)|mfp->value;
			break;
		    }
		}
		if (mfp->c == '\0' && *cp != '-') {
		    fb_log("if_osg: unknown option '%c' ignored\n", *cp);
		}
		cp++;
	    }
	    *mp = '\0';
	    if (!alpha) {
		mode |= atoi(modebuf);
	    }
	}

	if ((mode & MODE_15MASK) == MODE_15ZAP) {
	    /* Only task: Attempt to release shared memory segment */
	    osg_zapmem();
	    return -1;
	}
    }
#if DIRECT_COLOR_VISUAL_ALLOWED
    ifp->if_mode = mode;
#else
    ifp->if_mode = mode|MODE_7SWCMAP;
#endif

    /*
     * Allocate extension memory sections,
     * addressed by SGI(ifp)->mi_xxx and OSG(ifp)->xxx
     */

    if ((SGIL(ifp) = (char *)calloc(1, sizeof(struct sgiinfo))) == NULL) {
	fb_log("fb_osg_open:  sgiinfo malloc failed\n");
	return -1;
    }
    if ((OSGL(ifp) = (char *)calloc(1, sizeof(struct osginfo))) == NULL) {
	fb_log("fb_osg_open:  osginfo malloc failed\n");
	return -1;
    }

    SGI(ifp)->mi_shmid = -1;	/* indicate no shared memory */

    /* the Silicon Graphics Library Window management routines use
     * shared memory. This causes lots of problems when you want to
     * pass a window structure to a child process.  One hack to get
     * around this is to immediately fork and create a child process
     * and sleep until the child sends a kill signal to the parent
     * process. (in FBCLOSE) This allows us to use the traditional fb
     * utility programs as well as allow the frame buffer window to
     * remain around until killed by the menu subsystem.
     */

#if defined(HAVE_SYSCONF) && defined(HAVE_SHMDT) && defined(HAVE_SHMCTL) && defined(HAVE_SHMGET) && defined(HAVE_SHMAT)
    if ((ifp->if_mode & MODE_2MASK) == MODE_2LINGERING) {
	/* save parent pid for later signalling */
	SGI(ifp)->mi_parent = bu_process_id();

	signal(SIGUSR1, sigkid);
    }
#endif

    /* use defaults if invalid width and height specified */
    if (width <= 0)
	width = ifp->if_width;
    if (height <= 0)
	height = ifp->if_height;
    /* use max values if width and height are greater */
    if (width > ifp->if_max_width)
	width = ifp->if_max_width;
    if (height > ifp->if_max_height)
	height = ifp->if_max_height;

    ifp->if_width = width;
    ifp->if_height = height;

    SGI(ifp)->mi_curs_on = 1;

    /* Build a descriptive window title bar */
    (void)snprintf(title, 128, "BRL-CAD /dev/osg %s, %s",
		   ((ifp->if_mode & MODE_2MASK) == MODE_2TRANSIENT) ?
		   "Transient Win":
		   "Lingering Win",
		   ((ifp->if_mode & MODE_1MASK) == MODE_1MALLOC) ?
		   "Private Mem" :
		   "Shared Mem");

    /* initialize window state variables before calling osg_getmem */
    ifp->if_zoomflag = 0;
    ifp->if_xzoom = 1;	/* for zoom fakeout */
    ifp->if_yzoom = 1;	/* for zoom fakeout */
    ifp->if_xcenter = width/2;
    ifp->if_ycenter = height/2;
    SGI(ifp)->mi_pid = bu_process_id();

    /* Attach to shared memory, potentially with a screen repaint */
    if (osg_getmem(ifp) < 0)
	return -1;
#if 0
    /* Open an X connection to the display.  Sending NULL to XOpenDisplay
       tells it to use the DISPLAY environment variable. */
    if ((OSG(ifp)->dispp = XOpenDisplay(NULL)) == NULL) {
	fb_log("fb_osg_open: Failed to open display.  Check DISPLAY environment variable.\n");
	return -1;
    }
    ifp->if_selfd = ConnectionNumber(OSG(ifp)->dispp);
    if (CJDEBUG) {
	printf("Connection opened to X display on fd %d.\n",
	       ConnectionNumber(OSG(ifp)->dispp));
    }
#endif
    /* Choose an appropriate visual. */
    //if ((OSG(ifp)->vip = fb_osg_choose_visual(ifp)) == NULL) {
    //	fb_log("fb_osg_open: Couldn't find an appropriate visual.  Exiting.\n");
//	return -1;
    //}

#if 0
    /* Open an OpenGL context with this visual*/
    OSG(ifp)->glxc = glXCreateContext(OSG(ifp)->dispp, OSG(ifp)->vip, 0, GL_TRUE /* direct context */);
    if (OSG(ifp)->glxc == NULL) {
	fb_log("ERROR: Couldn't create an OpenGL context!\n");
	return -1;
    }

    direct = glXIsDirect(OSG(ifp)->dispp, OSG(ifp)->glxc);
    if (CJDEBUG) {
	fb_log("Framebuffer drawing context is %s.\n", direct ? "direct" : "indirect");
    }
#endif


    // TODO - replace X window creation below with Tk window creation here.  See osg_open in
    // libdm for pointers.

    struct bu_vls if_pathName;
    struct bu_vls if_tkName;
    struct bu_vls if_dName;
    bu_vls_init(&if_pathName);
    bu_vls_init(&if_tkName);
    bu_vls_init(&if_dName);
    Tk_Window tkwin = (Tk_Window)NULL;

    bu_vls_sprintf(&if_pathName, ".if_osg%d", osg_nwindows);
    osg_nwindows++;
    bu_vls_strcpy(&if_dName, ":0.0");

    OSG(ifp)->fbinterp = Tcl_CreateInterp();
    Tcl_Init(OSG(ifp)->fbinterp);
    Tcl_Eval(OSG(ifp)->fbinterp, "package require Tk");

    tkwin = Tk_MainWindow(OSG(ifp)->fbinterp);

    //OSG(ifp)->xtkwin = Tk_CreateWindowFromPath(OSG(ifp)->fbinterp, tkwin, bu_vls_addr(&if_pathName), bu_vls_addr(&if_dName));
    OSG(ifp)->xtkwin = tkwin;

    bu_vls_printf(&if_tkName, "%s", (char *)Tk_Name(OSG(ifp)->xtkwin));

    OSG(ifp)->dispp = Tk_Display(tkwin);

    Tk_GeometryRequest(OSG(ifp)->xtkwin, width, height);

    Tk_MakeWindowExist(OSG(ifp)->xtkwin);

    OSG(ifp)->wind = Tk_WindowId(OSG(ifp)->xtkwin);

    Tk_MapWindow(OSG(ifp)->xtkwin);

    /* Set Tk variables to handle Window behavior */
    Tcl_SetVar(OSG(ifp)->fbinterp, "WM_DELETE_WINDOW", "0", 0);
    Tcl_Eval(OSG(ifp)->fbinterp, "wm protocol . WM_DELETE_WINDOW {set WM_DELETE_WINDOW \"1\"}");
    Tcl_Eval(OSG(ifp)->fbinterp, "bind . <Button-3>  {set WM_DELETE_WINDOW \"1\"}");
    Tcl_Eval(OSG(ifp)->fbinterp, "bind . <Expose> {set WM_EXPOSE_EVENT \"1\"}");
    Tcl_Eval(OSG(ifp)->fbinterp, "bind . <Motion> {set WM_EXPOSE_EVENT \"1\"}");
    Tcl_Eval(OSG(ifp)->fbinterp, "bind . <Configure> {set WM_EXPOSE_EVENT \"1\"}");

    while (Tcl_DoOneEvent(TCL_ALL_EVENTS|TCL_DONT_WAIT));

    // Init the Windata Variable that holds the handle for the Window to display OSG in.
    // Check the QOSGWidget.cpp example for more logic relevant to this.  Need to find
    // something showing how to handle Cocoa for the Mac, if that's possible
    osg::ref_ptr<osg::GraphicsContext::Traits> traits = new osg::GraphicsContext::Traits;
#if defined(_WIN32)
    osg::ref_ptr<osg::Referenced> windata = new osgViewer::GraphicsWindowWin32::WindowData(Tk_GetHWND(OSG(ifp)->wind));
#else
    osg::ref_ptr<osg::Referenced> windata = new osgViewer::GraphicsWindowX11::WindowData(OSG(ifp)->wind);
#endif

    // Although we are not making direct use of osgViewer currently, we need its
    // initialization to make sure we have all the libraries we need loaded and
    // ready.
    OSG(ifp)->viewer = new osgViewer::Viewer();
    delete viewer;

    // Setup the traits parameters
    traits->x = 0;
    traits->y = 0;
    traits->width = width;
    traits->height = height;
    traits->depth = 24;
    //traits->windowDecoration = true;
    traits->windowDecoration = false;
    traits->doubleBuffer = true;
    traits->sharedContext = 0;
    traits->setInheritedWindowPixelFormat = true;

    traits->inheritedWindowData = windata;

    // Create the Graphics Context
    OSG(ifp)->graphicsContext = osg::GraphicsContext::createGraphicsContext(traits.get());

    OSG(ifp)->graphicsContext->realize();
    OSG(ifp)->graphicsContext->makeCurrent();

    OSG(ifp)->alive = 1;
    OSG(ifp)->firstTime = 1;

    /* Loop through events until first exposure event is processed */
    while (OSG(ifp)->firstTime == 1)
	osg_do_event(ifp);


    while (Tcl_DoOneEvent(TCL_ALL_EVENTS|TCL_DONT_WAIT));
    return 0;
}


extern "C" int
_osg_open_existing(FBIO *ifp, Display *dpy, Window win, Colormap cmap, int width, int height, osg::ref_ptr<osg::GraphicsContext> graphicsContext)
{

    /*XXX for now use private memory */
    ifp->if_mode = MODE_1MALLOC;

    /*
     * Allocate extension memory sections,
     * addressed by SGI(ifp)->mi_xxx and OSG(ifp)->xxx
     */

    if ((SGIL(ifp) = (char *)calloc(1, sizeof(struct sgiinfo))) == NULL) {
	fb_log("fb_osg_open:  sgiinfo malloc failed\n");
	return -1;
    }
    if ((OSGL(ifp) = (char *)calloc(1, sizeof(struct osginfo))) == NULL) {
	fb_log("fb_osg_open:  osginfo malloc failed\n");
	return -1;
    }

    OSG(ifp)->use_ext_ctrl = 1;

    SGI(ifp)->mi_shmid = -1;	/* indicate no shared memory */
    ifp->if_width = ifp->if_max_width = width;
    ifp->if_height = ifp->if_max_height = height;

    OSG(ifp)->win_width = OSG(ifp)->vp_width = width;
    OSG(ifp)->win_height = OSG(ifp)->vp_height = height;

    SGI(ifp)->mi_curs_on = 1;

    /* initialize window state variables before calling osg_getmem */
    ifp->if_zoomflag = 0;
    ifp->if_xzoom = 1;	/* for zoom fakeout */
    ifp->if_yzoom = 1;	/* for zoom fakeout */
    ifp->if_xcenter = width/2;
    ifp->if_ycenter = height/2;
    SGI(ifp)->mi_pid = bu_process_id();

    /* Attach to shared memory, potentially with a screen repaint */
    if (osg_getmem(ifp) < 0)
	return -1;

    OSG(ifp)->dispp = dpy;
    ifp->if_selfd = ConnectionNumber(OSG(ifp)->dispp);

    OSG(ifp)->graphicsContext = graphicsContext;
    SGI(ifp)->mi_cmap_flag = !is_linear_cmap(ifp);
    OSG(ifp)->xcmap = cmap;

    OSG(ifp)->wind = win;
    ++osg_nwindows;

    OSG(ifp)->alive = 1;
    OSG(ifp)->firstTime = 1;

    osg_clipper(ifp);

    return 0;
}


extern "C" int
osg_open_existing(FBIO *ifp, int argc, const char **argv)
{
    Display *dpy;
    Window win;
    Colormap cmap;
    int width;
    int height;
    //GLXContext glxc;
    osg::ref_ptr<osg::GraphicsContext> graphicsContext;

    if (argc != 10)
	return -1;

    if (sscanf(argv[1], "%p", (void **)&dpy) != 1)
	return -1;

    if (sscanf(argv[2], "%p", (void **)&win) != 1)
	return -1;

    if (sscanf(argv[3], "%p", (void **)&cmap) != 1)
	return -1;

    if (sscanf(argv[4], "%d", &width) != 1)
	return -1;

    if (sscanf(argv[5], "%d", &height) != 1)
	return -1;

    if (sscanf(argv[6], "%p", (void **)&graphicsContext) != 1)
	return -1;

    return _osg_open_existing(ifp, dpy, win, cmap, width, height,
			      graphicsContext);
}


HIDDEN int
osg_final_close(FBIO *ifp)
{

    //TODO - need name of window here
    //Tcl_Eval(OSG(ifp)->fbinterp, "destroy .");

    if (CJDEBUG) {
	printf("osg_final_close: All done...goodbye!\n");
    }

#if 0
    XDestroyWindow(OSG(ifp)->dispp, OSG(ifp)->wind);
    XFreeColormap(OSG(ifp)->dispp, OSG(ifp)->xcmap);
#endif

#if defined(HAVE_SYSCONF) && defined(HAVE_SHMDT) && defined(HAVE_SHMCTL) && defined(HAVE_SHMGET) && defined(HAVE_SHMAT)
    if (SGIL(ifp) != NULL) {
	/* free up memory associated with image */
	if (SGI(ifp)->mi_shmid != -1) {
	    /* detach from shared memory */
	    if (shmdt(ifp->if_mem) == -1) {
		fb_log("fb_osg_close shmdt failed, errno=%d\n",
		       errno);
		return -1;
	    }
	} else {
	    /* free private memory */
	    (void)free(ifp->if_mem);
	}
	/* free state information */
	(void)free((char *)SGIL(ifp));
	SGIL(ifp) = NULL;
    }
#endif

    if (OSGL(ifp) != NULL) {
	(void)free((char *)OSGL(ifp));
	OSGL(ifp) = NULL;
    }

    osg_nwindows--;
    return 0;
}


HIDDEN int
osg_flush(FBIO *ifp)
{
    if ((ifp->if_mode & MODE_12MASK) == MODE_12DELAY_WRITES_TILL_FLUSH) {
	OSG(ifp)->graphicsContext->makeCurrent();

	/* Send entire in-memory buffer to the screen, all at once */
	osg_xmit_scanlines(ifp, 0, ifp->if_height, 0, ifp->if_width);
	if (SGI(ifp)->mi_doublebuffer) {
	    OSG(ifp)->graphicsContext->swapBuffers();
	} else if (OSG(ifp)->copy_flag) {
	    backbuffer_to_screen(ifp, -1);
	}

	/* unattach context for other threads to use, also flushes */
	OSG(ifp)->graphicsContext->releaseContext();
    }
    //XFlush(OSG(ifp)->dispp);
    glFlush();
    return 0;
}


HIDDEN int
fb_osg_close(FBIO *ifp)
{

    osg_flush(ifp);

    /* only the last open window can linger -
     * call final_close if not lingering
     */
    if (osg_nwindows > 1 ||
	(ifp->if_mode & MODE_2MASK) == MODE_2TRANSIENT)
	return osg_final_close(ifp);

    if (CJDEBUG)
	printf("fb_osg_close: remaining open to linger awhile.\n");

    /*
     * else:
     *
     * LINGER mode.  Don't return to caller until user mouses "close"
     * menu item.  This may delay final processing in the calling
     * function for some time, but the assumption is that the user
     * wishes to compare this image with others.
     *
     * Since we plan to linger here, long after our invoker expected
     * us to be gone, be certain that no file descriptors remain open
     * to associate us with pipelines, network connections, etc., that
     * were ALREADY ESTABLISHED before the point that fb_open() was
     * called.
     *
     * The simple for i=0..20 loop will not work, because that smashes
     * some window-manager files.  Therefore, we content ourselves
     * with eliminating stdin, in the hopes that this will
     * successfully terminate any pipes or network connections.
     * Standard error/out may be used to print framebuffer debug
     * messages, so they're kept around.
     */
    fclose(stdin);

    while (0 < OSG(ifp)->alive) {
	osg_do_event(ifp);
    }

    {
	struct bu_vls tcl_cmd = BU_VLS_INIT_ZERO;

	OSG(ifp)->graphicsContext->makeCurrent();
	OSG(ifp)->graphicsContext->releaseContext();
	bu_vls_sprintf(&tcl_cmd, "destroy %s", (char *)Tk_Name(OSG(ifp)->xtkwin));
	Tcl_Eval(OSG(ifp)->fbinterp, bu_vls_addr(&tcl_cmd));
	bu_vls_free(&tcl_cmd);
    }

    return 0;
}


extern "C" int
osg_close_existing(FBIO *ifp)
{
#if 0
    if (OSG(ifp)->cursor)
	XDestroyWindow(OSG(ifp)->dispp, OSG(ifp)->cursor);
#endif

#if defined(HAVE_SYSCONF) && defined(HAVE_SHMDT) && defined(HAVE_SHMCTL) && defined(HAVE_SHMGET) && defined(HAVE_SHMAT)
    if (SGIL(ifp) != NULL) {
	/* free up memory associated with image */
	if (SGI(ifp)->mi_shmid != -1) {
	    /* detach from shared memory */
	    if (shmdt(ifp->if_mem) == -1) {
		fb_log("fb_osg_close: shmdt failed, errno=%d\n",
		       errno);
		return -1;
	    }
	} else {
	    /* free private memory */
	    (void)free(ifp->if_mem);
	}
	/* free state information */
	(void)free((char *)SGIL(ifp));
	SGIL(ifp) = NULL;
    }
#endif

    if (OSGL(ifp) != NULL) {
	(void)free((char *)OSGL(ifp));
	OSGL(ifp) = NULL;
    }

    return 0;
}


/*
 * Handle any pending input events
 */
HIDDEN int
osg_poll(FBIO *ifp)
{
    osg_do_event(ifp);

    if (OSG(ifp)->alive < 0)
	return 1;
    else
	return 0;
}


/*
 * Free shared memory resources, and close.
 */
HIDDEN int
osg_free(FBIO *ifp)
{
    int ret;

    if (CJDEBUG) printf("entering osg_free\n");
    /* Close the framebuffer */
    ret = osg_final_close(ifp);

    if ((ifp->if_mode & MODE_1MASK) == MODE_1SHARED) {
	/* If shared mem, release the shared memory segment */
	osg_zapmem();
    }
    return ret;
}


HIDDEN int
osg_clear(FBIO *ifp, unsigned char *pp)

/* pointer to beginning of memory segment*/
{
    struct osg_pixel bg;
    register struct osg_pixel *osgp;
    register int cnt;
    register int y;

    if (CJDEBUG) printf("entering osg_clear\n");

    OSG(ifp)->graphicsContext->makeCurrent();

    /* Set clear colors */
    if (pp != RGBPIXEL_NULL) {
	bg.alpha = 0;
	bg.red   = (pp)[RED];
	bg.green = (pp)[GRN];
	bg.blue  = (pp)[BLU];
	glClearColor(pp[RED]/255.0, pp[GRN]/255.0, pp[BLU]/255.0, 0.0);
    } else {
	bg.alpha = 0;
	bg.red   = 0;
	bg.green = 0;
	bg.blue  = 0;
	glClearColor(0, 0, 0, 0);
    }

    /* Flood rectangle in shared memory */
    for (y = 0; y < ifp->if_height; y++) {
	osgp = (struct osg_pixel *)&ifp->if_mem[
	    (y*SGI(ifp)->mi_memwidth+0)*sizeof(struct osg_pixel) ];
	for (cnt = ifp->if_width-1; cnt >= 0; cnt--) {
	    *osgp++ = bg;	/* struct copy */
	}
    }


    /* Update screen */
    if (OSG(ifp)->use_ext_ctrl) {
	glClear(GL_COLOR_BUFFER_BIT);
    } else {
	if (OSG(ifp)->copy_flag) {
	    /* COPY mode: clear both buffers */
	    if (OSG(ifp)->front_flag) {
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
	    if (SGI(ifp)->mi_doublebuffer) {
		OSG(ifp)->graphicsContext->swapBuffers();
	    }
	}

    }

    /* unattach context for other threads to use */
    OSG(ifp)->graphicsContext->releaseContext();

    return 0;
}


HIDDEN int
osg_view(FBIO *ifp, int xcenter, int ycenter, int xzoom, int yzoom)
{
    struct osg_clip *clp;

    if (CJDEBUG) printf("entering osg_view\n");

    if (xzoom < 1) xzoom = 1;
    if (yzoom < 1) yzoom = 1;
    if (ifp->if_xcenter == xcenter && ifp->if_ycenter == ycenter
	&& ifp->if_xzoom == xzoom && ifp->if_yzoom == yzoom)
	return 0;

    if (xcenter < 0 || xcenter >= ifp->if_width)
	return -1;
    if (ycenter < 0 || ycenter >= ifp->if_height)
	return -1;
    if (xzoom >= ifp->if_width || yzoom >= ifp->if_height)
	return -1;

    ifp->if_xcenter = xcenter;
    ifp->if_ycenter = ycenter;
    ifp->if_xzoom = xzoom;
    ifp->if_yzoom = yzoom;

    if (ifp->if_xzoom > 1 || ifp->if_yzoom > 1)
	ifp->if_zoomflag = 1;
    else ifp->if_zoomflag = 0;


    if (OSG(ifp)->use_ext_ctrl) {
	osg_clipper(ifp);
    } else {
	OSG(ifp)->graphicsContext->makeCurrent();

	/* Set clipping matrix and zoom level */
	glMatrixMode(GL_PROJECTION);
	if (OSG(ifp)->copy_flag && !OSG(ifp)->front_flag) {
	    /* COPY mode - no changes to backbuffer copy - just
	     * need to update front buffer
	     */
	    glPopMatrix();
	    glDrawBuffer(GL_FRONT);
	    OSG(ifp)->front_flag = 1;
	}
	glLoadIdentity();

	osg_clipper(ifp);
	clp = &(OSG(ifp)->clip);
	glOrtho(clp->oleft, clp->oright, clp->obottom, clp->otop, -1.0, 1.0);
	glPixelZoom((float) ifp->if_xzoom, (float) ifp->if_yzoom);

	if (OSG(ifp)->copy_flag) {
	    backbuffer_to_screen(ifp, -1);
	} else {
	    osg_xmit_scanlines(ifp, 0, ifp->if_height, 0, ifp->if_width);
	    if (SGI(ifp)->mi_doublebuffer) {
		OSG(ifp)->graphicsContext->swapBuffers();
	    }
	}
	glFlush();

	/* unattach context for other threads to use */
	OSG(ifp)->graphicsContext->releaseContext();
    }

    return 0;
}


HIDDEN int
osg_getview(FBIO *ifp, int *xcenter, int *ycenter, int *xzoom, int *yzoom)
{
    if (CJDEBUG) printf("entering osg_getview\n");

    *xcenter = ifp->if_xcenter;
    *ycenter = ifp->if_ycenter;
    *xzoom = ifp->if_xzoom;
    *yzoom = ifp->if_yzoom;

    return 0;
}


/* read count pixels into pixelp starting at x, y */
HIDDEN ssize_t
osg_read(FBIO *ifp, int x, int y, unsigned char *pixelp, size_t count)
{
    size_t n;
    size_t scan_count;	/* # pix on this scanline */
    register unsigned char *cp;
    ssize_t ret;
    register struct osg_pixel *osgp;

    if (CJDEBUG) printf("entering osg_read\n");

    if (x < 0 || x >= ifp->if_width ||
	y < 0 || y >= ifp->if_height)
	return -1;

    ret = 0;
    cp = (unsigned char *)(pixelp);

    while (count) {
	if (y >= ifp->if_height)
	    break;

	if (count >= (size_t)(ifp->if_width-x))
	    scan_count = ifp->if_width-x;
	else
	    scan_count = count;

	osgp = (struct osg_pixel *)&ifp->if_mem[
	    (y*SGI(ifp)->mi_memwidth+x)*sizeof(struct osg_pixel) ];

	n = scan_count;
	while (n) {
	    cp[RED] = osgp->red;
	    cp[GRN] = osgp->green;
	    cp[BLU] = osgp->blue;
	    osgp++;
	    cp += 3;
	    n--;
	}
	ret += scan_count;
	count -= scan_count;
	x = 0;
	/* Advance upwards */
	if (++y >= ifp->if_height)
	    break;
    }
    return ret;
}


/* write count pixels from pixelp starting at xstart, ystart */
HIDDEN ssize_t
osg_write(FBIO *ifp, int xstart, int ystart, const unsigned char *pixelp, size_t count)
{
    size_t scan_count;	/* # pix on this scanline */
    register unsigned char *cp;
    ssize_t ret;
    int ybase;
    size_t pix_count;	/* # pixels to send */
    register int x;
    register int y;

    if (CJDEBUG) printf("entering osg_write\n");

    /* fast exit cases */
    pix_count = count;
    if (pix_count == 0)
	return 0;	/* OK, no pixels transferred */

    x = xstart;
    ybase = y = ystart;

    if (x < 0 || x >= ifp->if_width ||
	y < 0 || y >= ifp->if_height)
	return -1;

    ret = 0;
    cp = (unsigned char *)(pixelp);

    while (pix_count) {
	size_t n;
	register struct osg_pixel *osgp;

	if (y >= ifp->if_height)
	    break;

	if (pix_count >= (size_t)(ifp->if_width-x))
	    scan_count = (size_t)(ifp->if_width-x);
	else
	    scan_count = pix_count;

	osgp = (struct osg_pixel *)&ifp->if_mem[
	    (y*SGI(ifp)->mi_memwidth+x)*sizeof(struct osg_pixel) ];

	n = scan_count;
	if ((n & 3) != 0) {
	    /* This code uses 60% of all CPU time */
	    while (n) {
		/* alpha channel is always zero */
		osgp->red   = cp[RED];
		osgp->green = cp[GRN];
		osgp->blue  = cp[BLU];
		osgp++;
		cp += 3;
		n--;
	    }
	} else {
	    while (n) {
		/* alpha channel is always zero */
		osgp[0].red   = cp[RED+0*3];
		osgp[0].green = cp[GRN+0*3];
		osgp[0].blue  = cp[BLU+0*3];
		osgp[1].red   = cp[RED+1*3];
		osgp[1].green = cp[GRN+1*3];
		osgp[1].blue  = cp[BLU+1*3];
		osgp[2].red   = cp[RED+2*3];
		osgp[2].green = cp[GRN+2*3];
		osgp[2].blue  = cp[BLU+2*3];
		osgp[3].red   = cp[RED+3*3];
		osgp[3].green = cp[GRN+3*3];
		osgp[3].blue  = cp[BLU+3*3];
		osgp += 4;
		cp += 3*4;
		n -= 4;
	    }
	}
	ret += scan_count;
	pix_count -= scan_count;
	x = 0;
	if (++y >= ifp->if_height)
	    break;
    }

    if ((ifp->if_mode & MODE_12MASK) == MODE_12DELAY_WRITES_TILL_FLUSH)
	return ret;

    if (!OSG(ifp)->use_ext_ctrl) {

	OSG(ifp)->graphicsContext->makeCurrent();

	if (xstart + count < (size_t)ifp->if_width) {
	    osg_xmit_scanlines(ifp, ybase, 1, xstart, count);
	    if (SGI(ifp)->mi_doublebuffer) {
		OSG(ifp)->graphicsContext->swapBuffers();
	    } else if (OSG(ifp)->copy_flag) {
		/* repaint one scanline from backbuffer */
		backbuffer_to_screen(ifp, ybase);
	    }
	} else {
	    /* Normal case -- multi-pixel write */
	    if (SGI(ifp)->mi_doublebuffer) {
		/* refresh whole screen */
		osg_xmit_scanlines(ifp, 0, ifp->if_height, 0, ifp->if_width);
		OSG(ifp)->graphicsContext->swapBuffers();
	    } else {
		/* just write rectangle */
		osg_xmit_scanlines(ifp, ybase, y-ybase, 0, ifp->if_width);
		if (OSG(ifp)->copy_flag) {
		    backbuffer_to_screen(ifp, -1);
		}
	    }
	}
	glFlush();

	/* unattach context for other threads to use */
	OSG(ifp)->graphicsContext->releaseContext();
    }

    return ret;

}


/*
 * The task of this routine is to reformat the pixels into SGI
 * internal form, and then arrange to have them sent to the screen
 * separately.
 */
HIDDEN int
osg_writerect(FBIO *ifp, int xmin, int ymin, int width, int height, const unsigned char *pp)
{
    register int x;
    register int y;
    register unsigned char *cp;
    register struct osg_pixel *osgp;

    if (CJDEBUG) printf("entering osg_writerect\n");


    if (width <= 0 || height <= 0)
	return 0;  /* do nothing */
    if (xmin < 0 || xmin+width > ifp->if_width ||
	ymin < 0 || ymin+height > ifp->if_height)
	return -1; /* no can do */

    cp = (unsigned char *)(pp);
    for (y = ymin; y < ymin+height; y++) {
	osgp = (struct osg_pixel *)&ifp->if_mem[
	    (y*SGI(ifp)->mi_memwidth+xmin)*sizeof(struct osg_pixel) ];
	for (x = xmin; x < xmin+width; x++) {
	    /* alpha channel is always zero */
	    osgp->red   = cp[RED];
	    osgp->green = cp[GRN];
	    osgp->blue  = cp[BLU];
	    osgp++;
	    cp += 3;
	}
    }

    if ((ifp->if_mode & MODE_12MASK) == MODE_12DELAY_WRITES_TILL_FLUSH)
	return width*height;

    if (!OSG(ifp)->use_ext_ctrl) {
	OSG(ifp)->graphicsContext->makeCurrent();

	if (SGI(ifp)->mi_doublebuffer) {
	    /* refresh whole screen */
	    osg_xmit_scanlines(ifp, 0, ifp->if_height, 0, ifp->if_width);
	    OSG(ifp)->graphicsContext->swapBuffers();
	} else {
	    /* just write rectangle*/
	    osg_xmit_scanlines(ifp, ymin, height, xmin, width);
	    if (OSG(ifp)->copy_flag) {
		backbuffer_to_screen(ifp, -1);
	    }
	}

	/* unattach context for other threads to use */
	OSG(ifp)->graphicsContext->releaseContext();
    }

    return width*height;
}


/*
 * The task of this routine is to reformat the pixels into SGI
 * internal form, and then arrange to have them sent to the screen
 * separately.
 */
HIDDEN int
osg_bwwriterect(FBIO *ifp, int xmin, int ymin, int width, int height, const unsigned char *pp)
{
    register int x;
    register int y;
    register unsigned char *cp;
    register struct osg_pixel *osgp;

    if (CJDEBUG) printf("entering osg_bwwriterect\n");


    if (width <= 0 || height <= 0)
	return 0;  /* do nothing */
    if (xmin < 0 || xmin+width > ifp->if_width ||
	ymin < 0 || ymin+height > ifp->if_height)
	return -1; /* no can do */

    cp = (unsigned char *)(pp);
    for (y = ymin; y < ymin+height; y++) {
	osgp = (struct osg_pixel *)&ifp->if_mem[
	    (y*SGI(ifp)->mi_memwidth+xmin)*sizeof(struct osg_pixel) ];
	for (x = xmin; x < xmin+width; x++) {
	    register int val;
	    /* alpha channel is always zero */
	    osgp->red   = (val = *cp++);
	    osgp->green = val;
	    osgp->blue  = val;
	    osgp++;
	}
    }

    if ((ifp->if_mode & MODE_12MASK) == MODE_12DELAY_WRITES_TILL_FLUSH)
	return width*height;

    if (!OSG(ifp)->use_ext_ctrl) {
	OSG(ifp)->graphicsContext->makeCurrent();

	if (SGI(ifp)->mi_doublebuffer) {
	    /* refresh whole screen */
	    osg_xmit_scanlines(ifp, 0, ifp->if_height, 0, ifp->if_width);
	    OSG(ifp)->graphicsContext->swapBuffers();
	} else {
	    /* just write rectangle*/
	    osg_xmit_scanlines(ifp, ymin, height, xmin, width);
	    if (OSG(ifp)->copy_flag) {
		backbuffer_to_screen(ifp, -1);
	    }
	}

	/* unattach context for other threads to use */
	OSG(ifp)->graphicsContext->releaseContext();
    }

    return width*height;
}


HIDDEN int
osg_rmap(register FBIO *ifp, register ColorMap *cmp)
{
    register int i;

    if (CJDEBUG) printf("entering osg_rmap\n");

    /* Just parrot back the stored colormap */
    for (i = 0; i < 256; i++) {
	cmp->cm_red[i]   = CMR(ifp)[i]<<8;
	cmp->cm_green[i] = CMG(ifp)[i]<<8;
	cmp->cm_blue[i]  = CMB(ifp)[i]<<8;
    }
    return 0;
}


HIDDEN int
osg_wmap(register FBIO *ifp, register const ColorMap *cmp)
{
    register int i;
    int prev;	/* !0 = previous cmap was non-linear */

    if (CJDEBUG) printf("entering osg_wmap\n");

    prev = SGI(ifp)->mi_cmap_flag;
    if (cmp == COLORMAP_NULL) {
	osg_cminit(ifp);
    } else {
	for (i = 0; i < 256; i++) {
	    CMR(ifp)[i] = cmp-> cm_red[i]>>8;
	    CMG(ifp)[i] = cmp-> cm_green[i]>>8;
	    CMB(ifp)[i] = cmp-> cm_blue[i]>>8;
	}
    }
    SGI(ifp)->mi_cmap_flag = !is_linear_cmap(ifp);


    if (!OSG(ifp)->use_ext_ctrl) {
	if (OSG(ifp)->soft_cmap_flag) {
	    /* if current and previous maps are linear, return */
	    if (SGI(ifp)->mi_cmap_flag == 0 && prev == 0) return 0;

	    /* Software color mapping, trigger a repaint */

	    OSG(ifp)->graphicsContext->makeCurrent();

	    osg_xmit_scanlines(ifp, 0, ifp->if_height, 0, ifp->if_width);
	    if (SGI(ifp)->mi_doublebuffer) {
		OSG(ifp)->graphicsContext->swapBuffers();
	    } else if (OSG(ifp)->copy_flag) {
		backbuffer_to_screen(ifp, -1);
	    }

	    /* unattach context for other threads to use, also flushes */
	    OSG(ifp)->graphicsContext->releaseContext();
	} else {
	    /* Send color map to hardware */
	    /* This code has yet to be tested */

	    for (i = 0; i < 256; i++) {
		osg_color_cell[i].pixel = i;
		osg_color_cell[i].red = CMR(ifp)[i];
		osg_color_cell[i].green = CMG(ifp)[i];
		osg_color_cell[i].blue = CMB(ifp)[i];
		osg_color_cell[i].flags = DoRed | DoGreen | DoBlue;
	    }
	    //XStoreColors(OSG(ifp)->dispp, OSG(ifp)->xcmap, osg_color_cell, 256);
	}
    }

    return 0;
}


HIDDEN int
osg_help(FBIO *ifp)
{
    struct osg_modeflags *mfp;

    fb_log("Description: %s\n", ifp->if_type);
    fb_log("Device: %s\n", ifp->if_name);
    fb_log("Max width height: %d %d\n",
	   ifp->if_max_width,
	   ifp->if_max_height);
    fb_log("Default width height: %d %d\n",
	   ifp->if_width,
	   ifp->if_height);
    fb_log("Usage: /dev/osg[option letters]\n");
    for (mfp = osg_modeflags; mfp->c != '\0'; mfp++) {
	fb_log("   %c   %s\n", mfp->c, mfp->help);
    }

    fb_log("\nCurrent internal state:\n");
    fb_log("	mi_doublebuffer=%d\n", SGI(ifp)->mi_doublebuffer);
    fb_log("	mi_cmap_flag=%d\n", SGI(ifp)->mi_cmap_flag);
    fb_log("	osg_nwindows=%d\n", osg_nwindows);
#if 0
    fb_log("X11 Visual:\n");

    switch (visual->class) {
	case DirectColor:
	    fb_log("\tDirectColor: Alterable RGB maps, pixel RGB subfield indices\n");
	    fb_log("\tRGB Masks: 0x%x 0x%x 0x%x\n", visual->red_mask,
		   visual->green_mask, visual->blue_mask);
	    break;
	case TrueColor:
	    fb_log("\tTrueColor: Fixed RGB maps, pixel RGB subfield indices\n");
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
	    fb_log("\tUnknown visual class %d\n", visual->class);
	    break;
    }
#endif
    return 0;
}


HIDDEN int
osg_setcursor(FBIO *ifp, const unsigned char *UNUSED(bits), int UNUSED(xbits), int UNUSED(ybits), int UNUSED(xorig), int UNUSED(yorig))
{
    FB_CK_FBIO(ifp);

    return 0;
}


HIDDEN int
osg_cursor(FBIO *ifp, int mode, int x, int y)
{
#if 0
    if (mode) {
	register int xx, xy;
	register int delta;

	/* If we don't have a cursor, create it */
	if (!OSG(ifp)->cursor) {
	    XSetWindowAttributes xswa;
	    XColor rgb_db_def;
	    XColor bg, bd;

	    XAllocNamedColor(OSG(ifp)->dispp, OSG(ifp)->xcmap, "black",
			     &rgb_db_def, &bg);
	    XAllocNamedColor(OSG(ifp)->dispp, OSG(ifp)->xcmap, "white",
			     &rgb_db_def, &bd);
	    xswa.background_pixel = bg.pixel;
	    xswa.border_pixel = bd.pixel;
	    xswa.colormap = OSG(ifp)->xcmap;
	    xswa.save_under = True;

	    OSG(ifp)->cursor = XCreateWindow(OSG(ifp)->dispp, OSG(ifp)->wind,
					     0, 0, 4, 4, 2, OSG(ifp)->vip->depth, InputOutput,
					     OSG(ifp)->vip->visual, CWBackPixel | CWBorderPixel |
					     CWSaveUnder | CWColormap, &xswa);
	}

	delta = ifp->if_width/ifp->if_xzoom/2;
	xx = x - (ifp->if_xcenter - delta);
	xx *= ifp->if_xzoom;
	xx += ifp->if_xzoom/2;  /* center cursor */

	delta = ifp->if_height/ifp->if_yzoom/2;
	xy = y - (ifp->if_ycenter - delta);
	xy *= ifp->if_yzoom;
	xy += ifp->if_yzoom/2;  /* center cursor */
	xy = OSG(ifp)->win_height - xy;

	/* Move cursor into place; make it visible if it isn't */
	XMoveWindow(OSG(ifp)->dispp, OSG(ifp)->cursor, xx - 4, xy - 4);

	/* if cursor window is currently not mapped, map it */
	if (!ifp->if_cursmode)
	    XMapRaised(OSG(ifp)->dispp, OSG(ifp)->cursor);
    } else {
	/* If we have a cursor and it's mapped, unmap it */
	if (OSG(ifp)->cursor && ifp->if_cursmode)
	    XUnmapWindow(OSG(ifp)->dispp, OSG(ifp)->cursor);
    }

    /* Without this flush, cursor movement is sluggish */
    XFlush(OSG(ifp)->dispp);
#endif
    /* Update position of cursor */
    ifp->if_cursmode = mode;
    ifp->if_xcurs = x;
    ifp->if_ycurs = y;

    return 0;
}


extern "C" int
osg_refresh(FBIO *ifp, int x, int y, int w, int h)
{
    int mm;
    struct osg_clip *clp;

    if (w < 0) {
	w = -w;
	x -= w;
    }

    if (h < 0) {
	h = -h;
	y -= h;
    }


    glGetIntegerv(GL_MATRIX_MODE, &mm);
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();

    osg_clipper(ifp);
    clp = &(OSG(ifp)->clip);
    glOrtho(clp->oleft, clp->oright, clp->obottom, clp->otop, -1.0, 1.0);
    glPixelZoom((float) ifp->if_xzoom, (float) ifp->if_yzoom);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    glViewport(0, 0, OSG(ifp)->win_width, OSG(ifp)->win_height);
    osg_xmit_scanlines(ifp, y, h, x, w);
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
    glMatrixMode(mm);

    glFlush();
    return 0;
}

extern "C" {
/* This is the ONLY thing that we normally "export" */
FBIO osg_interface =
{
    0,			/* magic number slot */
    fb_osg_open,	/* open device */
    fb_osg_close,	/* close device */
    osg_clear,		/* clear device */
    osg_read,		/* read pixels */
    osg_write,		/* write pixels */
    osg_rmap,		/* read colormap */
    osg_wmap,		/* write colormap */
    osg_view,		/* set view */
    osg_getview,	/* get view */
    osg_setcursor,	/* define cursor */
    osg_cursor,		/* set cursor */
    fb_sim_getcursor,	/* get cursor */
    fb_sim_readrect,	/* read rectangle */
    osg_writerect,	/* write rectangle */
    fb_sim_bwreadrect,
    osg_bwwriterect,	/* write rectangle */
    osg_poll,		/* process events */
    osg_flush,		/* flush output */
    osg_free,		/* free resources */
    osg_help,		/* help message */
    "OpenSceneGraph",	/* device description */
    XMAXSCREEN+1,	/* max width */
    YMAXSCREEN+1,	/* max height */
    bu_strdup("/dev/osg"),		/* short device name */
    512,		/* default/current width */
    512,		/* default/current height */
    -1,			/* select file desc */
    -1,			/* file descriptor */
    1, 1,		/* zoom */
    256, 256,		/* window center */
    0, 0, 0,		/* cursor */
    PIXEL_NULL,		/* page_base */
    PIXEL_NULL,		/* page_curp */
    PIXEL_NULL,		/* page_endp */
    -1,			/* page_no */
    0,			/* page_dirty */
    0L,			/* page_curpos */
    0L,			/* page_pixels */
    0,			/* debug */
    {0}, /* u1 */
    {0}, /* u2 */
    {0}, /* u3 */
    {0}, /* u4 */
    {0}, /* u5 */
    {0}  /* u6 */
};
}
#else

/* quell empty-compilation unit warnings */
static const int unused = 0;

#endif /* IF_OSG */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
