/*                        I F _ T K . C
 * BRL-CAD
 *
 * Copyright (c) 2007-2021 United States Government as represented by
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
/** @addtogroup libstruct fb */
/** @{ */
/** @file if_tk.c
 *
 * Frame Buffer Library interface for Tk/Mesa3d.
 *
 * There are several different Frame Buffer modes supported.  Set your
 * environment FB_FILE to the appropriate type.
 *
 * (see the modeflag definitions below).  /dev/ogl[options]
 *
 * This code is basically a port of the 4d Framebuffer interface from
 * IRIS GL to OpenGL.
 *
 */
/** @} */

#include "common.h"

#ifdef HAVE_SYS_TYPES_H
#  include <sys/types.h>
#endif
#include <sys/ipc.h>
#include <sys/shm.h>

#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <signal.h>
#include <errno.h>

#include "tk.h"

#include "bio.h"
#include "bresource.h"


#include "bu/color.h"
#include "bu/malloc.h"
#include "bu/parallel.h"
#include "bu/snooze.h"
#include "bu/str.h"
#include "../include/private.h"
#include "dm.h"
#include "./fb_tk.h"

extern struct fb tk_interface;

#define DIRECT_COLOR_VISUAL_ALLOWED 0

HIDDEN int tk_nwindows = 0; 	/* number of open windows */
HIDDEN XColor color_cell[256];		/* used to set colormap */


/*
 * Per window state information, overflow area.
 */
struct wininfo {
    short mi_curs_on;
    short mi_cmap_flag;			/* enabled when there is a non-linear map in memory */
    int mi_shmid;
    int mi_pixwidth;			/* width of scanline in if_mem (in pixels) */
    short mi_xoff;			/* X viewport offset, rel. window*/
    short mi_yoff;			/* Y viewport offset, rel. window*/
    int mi_doublebuffer;		/* 0=singlebuffer 1=doublebuffer */
};


/*
 * Per window state information particular to the OpenGL interface
 */
struct oglinfo {
    OSMesaContext glxc;
    void *buf;
    Display *dispp;		/* pointer to X display connection */
    Window wind;		/* Window identifier */
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
    struct fb_clip clip;	/* current view clipping */
    Window cursor;
    Colormap xcmap;		/* xstyle color map */
    int use_ext_ctrl;		/* for controlling the Ogl graphics engine externally */
};


#define WIN(ptr) ((struct wininfo *)((ptr)->i->u1.p))
#define WINL(ptr) ((ptr)->i->u1.p)	/* left hand side version */
#define TK(ptr) ((struct oglinfo *)((ptr)->i->u6.p))
#define TKL(ptr) ((ptr)->i->u6.p)	/* left hand side version */
#define if_mem u2.p		/* shared memory pointer */
#define if_cmap u3.p		/* color map in shared memory */
#define CMR(x) ((struct fb_cmap *)((x)->i->if_cmap))->cmr
#define CMG(x) ((struct fb_cmap *)((x)->i->if_cmap))->cmg
#define CMB(x) ((struct fb_cmap *)((x)->i->if_cmap))->cmb
#define if_zoomflag u4.l	/* zoom > 1 */
#define if_mode u5.l		/* see MODE_* defines */

#define CLIP_XTRA 1


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

HIDDEN struct modeflags {
    char c;
    long mask;
    long value;
    char *help;
} modeflags[] = {
    { 'p',	MODE_1MASK, MODE_1MALLOC,
      "Private memory - else shared" },
    { 'l',	MODE_2MASK, MODE_2LINGERING,
      "Lingering window" },
    { 't',	MODE_2MASK, MODE_2TRANSIENT,
      "Transient window" },
    { 'd',	MODE_4MASK, MODE_4NODITH,
      "Suppress dithering - else dither if not 24-bit buffer" },
    { 'c',	MODE_7MASK, MODE_7SWCMAP,
      "Perform software colormap - else use hardware colormap if possible" },
    { 's',	MODE_9MASK, MODE_9SINGLEBUF,
      "Single buffer - else double buffer if possible" },
    { 'b',	MODE_11MASK, MODE_11COPY,
      "Fast pan and zoom using backbuffer copy - else normal " },
    { 'D',	MODE_12MASK, MODE_12DELAY_WRITES_TILL_FLUSH,
      "Don't update screen until fb_flush() is called.  (Double buffer sim)" },
    { 'z',	MODE_15MASK, MODE_15ZAP,
      "Zap (free) shared memory.  Can also be done with fbfree command" },
    { '\0', 0, 0, "" }
};


/* BACKBUFFER_TO_SCREEN - copy pixels from copy on the backbuffer to
 * the front buffer. Do one scanline specified by one_y, or whole
 * screen if one_y equals -1.
 */
HIDDEN void
backbuffer_to_screen(register struct fb *ifp, int one_y)
{
    struct fb_clip *clp;

    if (!(TK(ifp)->front_flag)) {
	TK(ifp)->front_flag = 1;
	glDrawBuffer(GL_FRONT);
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	glPixelZoom((float) ifp->i->if_xzoom, (float) ifp->i->if_yzoom);
    }

    clp = &(TK(ifp)->clip);

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
	if (clp->xscrmax >= TK(ifp)->vp_width) glRecti(ifp->i->if_width - CLIP_XTRA,
							clp->yscrmin - CLIP_XTRA,
							clp->xscrmax + CLIP_XTRA,
							clp->yscrmax + CLIP_XTRA);

	/* Blank out area above image */
	if (clp->yscrmax >= TK(ifp)->vp_height) glRecti(clp->xscrmin - CLIP_XTRA,
							 TK(ifp)->vp_height - CLIP_XTRA,
							 clp->xscrmax + CLIP_XTRA,
							 clp->yscrmax + CLIP_XTRA);

	/* copy image from backbuffer */
	glRasterPos2i(clp->xpixmin, clp->ypixmin);
	glCopyPixels(WIN(ifp)->mi_xoff + clp->xpixmin,
		     WIN(ifp)->mi_yoff + clp->ypixmin,
		     clp->xpixmax - clp->xpixmin +1,
		     clp->ypixmax - clp->ypixmin +1,
		     GL_COLOR);


    } else if (one_y < clp->ypixmin) {
	return;
    } else {
	/* draw one scanline */
	glRasterPos2i(clp->xpixmin, one_y);
	glCopyPixels(WIN(ifp)->mi_xoff + clp->xpixmin,
		     WIN(ifp)->mi_yoff + one_y,
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
tk_xmit_scanlines(register struct fb *ifp, int ybase, int nlines, int xbase, int npix)
{
    register int y;
    register int n;
    int sw_cmap;	/* !0 => needs software color map */
    struct fb_clip *clp;

    /* Caller is expected to handle attaching context, etc. */

    clp = &(TK(ifp)->clip);

    if (TK(ifp)->soft_cmap_flag  && WIN(ifp)->mi_cmap_flag) {
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

    if (!TK(ifp)->use_ext_ctrl) {
	if (!TK(ifp)->copy_flag) {
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
	    if (clp->xscrmax >= ifp->i->if_width) glRecti(ifp->i->if_width - CLIP_XTRA,
						       clp->yscrmin - CLIP_XTRA,
						       clp->xscrmax + CLIP_XTRA,
						       clp->yscrmax + CLIP_XTRA);

	    /* Blank out area above image */
	    if (clp->yscrmax >= ifp->i->if_height) glRecti(clp->xscrmin - CLIP_XTRA,
							ifp->i->if_height- CLIP_XTRA,
							clp->xscrmax + CLIP_XTRA,
							clp->yscrmax + CLIP_XTRA);

	} else if (TK(ifp)->front_flag) {
	    /* in COPY mode, always draw full sized image into backbuffer.
	     * backbuffer_to_screen() is used to update the front buffer
	     */
	    glDrawBuffer(GL_BACK);
	    TK(ifp)->front_flag = 0;
	    glMatrixMode(GL_PROJECTION);
	    glPushMatrix();	/* store current view clipping matrix*/
	    glLoadIdentity();
	    glOrtho(-0.25, ((GLdouble) TK(ifp)->vp_width)-0.25,
		    -0.25, ((GLdouble) TK(ifp)->vp_height)-0.25,
		    -1.0, 1.0);
	    glPixelZoom(1.0, 1.0);
	}
    }

    if (sw_cmap) {
	/* Software colormap each line as it's transmitted */
	register int x;
	register struct fb_pixel *oglp;
	register struct fb_pixel *scanline;

	y = ybase;

	if (FB_DEBUG)
	    printf("Doing sw colormap xmit\n");

	/* Perform software color mapping into temp scanline */
	scanline = (struct fb_pixel *)calloc(ifp->i->if_width, sizeof(struct fb_pixel));
	if (scanline == NULL) {
	    fb_log("tk_getmem: scanline memory malloc failed\n");
	    return;
	}

	for (n=nlines; n>0; n--, y++) {
	    oglp = (struct fb_pixel *)&ifp->i->if_mem[(y*WIN(ifp)->mi_pixwidth) * sizeof(struct fb_pixel)];
	    for (x=xbase+npix-1; x>=xbase; x--) {
		scanline[x].red   = CMR(ifp)[oglp[x].red];
		scanline[x].green = CMG(ifp)[oglp[x].green];
		scanline[x].blue  = CMB(ifp)[oglp[x].blue];
	    }

	    glPixelStorei(GL_UNPACK_SKIP_PIXELS, xbase);
	    glRasterPos2i(xbase, y);
	    glDrawPixels(npix, 1, GL_BGRA_EXT, GL_UNSIGNED_BYTE, (const GLvoid *)scanline);
	}

	(void)free((void *)scanline);

    } else {
	/* No need for software colormapping */

	glPixelStorei(GL_UNPACK_ROW_LENGTH, WIN(ifp)->mi_pixwidth);
	glPixelStorei(GL_UNPACK_SKIP_PIXELS, xbase);
	glPixelStorei(GL_UNPACK_SKIP_ROWS, ybase);

	glRasterPos2i(xbase, ybase);
	glDrawPixels(npix, nlines, GL_BGRA_EXT, GL_UNSIGNED_BYTE, (const GLvoid *) ifp->i->if_mem);
    }
}


HIDDEN void
tk_cminit(register struct fb *ifp)
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
tk_getmem(struct fb *ifp)
{
    size_t pixsize;
    size_t size;
    int i;
    char *sp;
    int new_mem = 0;
    int shm_result = 0;

    errno = 0;

    pixsize = ifp->i->if_height * ifp->i->if_width * sizeof(struct fb_pixel);

    /* shared memory behaves badly if we try to allocate too much, so
     * arbitrarily constrain it to our default 1000x1000 size & samller.
     */
    if (pixsize > sizeof(struct fb_pixel)*1000*1000) {
	/* let the user know */
	ifp->i->if_mode |= MODE_1MALLOC;
	/* fb_log("tk_getmem: image too big for shared memory, using private\n"); */
    }

    if ((ifp->i->if_mode & MODE_1MASK) == MODE_1MALLOC) {
	/* In this mode, only malloc as much memory as is needed. */
	WIN(ifp)->mi_pixwidth = ifp->i->if_width;
	size = pixsize + sizeof(struct fb_cmap);

	sp = (char *)calloc(1, size);
	if (sp == NULL) {
	    fb_log("tk_getmem: frame buffer memory malloc failed\n");
	    return -1;
	}

	new_mem = 1;

    } else {
	/* The shared memory section never changes size */
	WIN(ifp)->mi_pixwidth = ifp->i->if_max_width;

	pixsize = ifp->i->if_max_height * ifp->i->if_max_width * sizeof(struct fb_pixel);
	size = pixsize + sizeof(struct fb_cmap);

	shm_result = bu_shmget(&(WIN(ifp)->mi_shmid), &sp, SHMEM_KEY, size);

	if (shm_result == -1) {
	    memset(sp, 0, size); /* match calloc */
	    new_mem = 1;
	} else if (shm_result == 1) {
	    ifp->i->if_mode |= MODE_1MALLOC;
	    fb_log("tk_getmem:  Unable to attach to shared memory, using private\n");
	    if ((sp = (char *)calloc(1, size)) == NULL) {
		fb_log("tk_getmem:  malloc failure\n");
		return -1;
	    }
	    new_mem = 1;
	}
    }

    ifp->i->if_mem = sp;
    ifp->i->if_cmap = sp + pixsize; /* cmap at end of area */
    i = CMB(ifp)[255];		 /* try to deref last word */
    CMB(ifp)[255] = i;

    /* Provide non-black colormap on creation of new shared mem */
    if (new_mem)
	tk_cminit(ifp);

    return 0;
}


void
tk_zapmem(void)
{
    int shmid;
    int i;

    errno = 0;

    if ((shmid = shmget(SHMEM_KEY, 0, 0)) < 0) {
	if (errno != ENOENT) {
	    fb_log("tk_zapmem shmget failed, errno=%d\n", errno);
	    perror("shmget");
	}
	return;
    }

    i = shmctl(shmid, IPC_RMID, 0);
    if (i < 0) {
	fb_log("tk_zapmem shmctl failed, errno=%d\n", errno);
	perror("shmctl");
	return;
    }
    fb_log("if_ogl: shared memory released\n");
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
fb_clipper(register struct fb *ifp)
{
    register struct fb_clip *clp;
    register int i;
    double pixels;

    clp = &(TK(ifp)->clip);

    i = TK(ifp)->vp_width/(2*ifp->i->if_xzoom);
    clp->xscrmin = ifp->i->if_xcenter - i;
    i = TK(ifp)->vp_width/ifp->i->if_xzoom;
    clp->xscrmax = clp->xscrmin + i;
    pixels = (double) i;
    clp->oleft = ((double) clp->xscrmin) - 0.25*pixels/((double) TK(ifp)->vp_width);
    clp->oright = clp->oleft + pixels;

    i = TK(ifp)->vp_height/(2*ifp->i->if_yzoom);
    clp->yscrmin = ifp->i->if_ycenter - i;
    i = TK(ifp)->vp_height/ifp->i->if_yzoom;
    clp->yscrmax = clp->yscrmin + i;
    pixels = (double) i;
    clp->obottom = ((double) clp->yscrmin) - 0.25*pixels/((double) TK(ifp)->vp_height);
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
    if (TK(ifp)->copy_flag) {
	if (clp->xpixmax > TK(ifp)->vp_width-1) {
	    clp->xpixmax = TK(ifp)->vp_width-1;
	}
	if (clp->ypixmax > TK(ifp)->vp_height-1) {
	    clp->ypixmax = TK(ifp)->vp_height-1;
	}
    } else {
	if (clp->xpixmax > ifp->i->if_width-1) {
	    clp->xpixmax = ifp->i->if_width-1;
	}
	if (clp->ypixmax > ifp->i->if_height-1) {
	    clp->ypixmax = ifp->i->if_height-1;
	}
    }

}


HIDDEN void
expose_callback(struct fb *ifp)
{
    XWindowAttributes xwa;
    struct fb_clip *clp;

    if (FB_DEBUG)
	printf("entering expose_callback()\n");

    if (OSMesaMakeCurrent(TK(ifp)->glxc, TK(ifp)->buf, GL_UNSIGNED_BYTE, TK(ifp)->win_width, TK(ifp)->win_height)) {
	fb_log("Warning, expose_callback: OSMesaMakeCurrent unsuccessful.\n");
    }

    if (TK(ifp)->firstTime) {

	TK(ifp)->firstTime = 0;

	/* just in case the configuration is double buffered but
	 * we want to pretend it's not
	 */

	if (!WIN(ifp)->mi_doublebuffer) {
	    glDrawBuffer(GL_FRONT);
	}

	if ((ifp->i->if_mode & MODE_4MASK) == MODE_4NODITH) {
	    glDisable(GL_DITHER);
	}

	/* set copy mode if possible and requested */
	if (WIN(ifp)->mi_doublebuffer &&
	    ((ifp->i->if_mode & MODE_11MASK)==MODE_11COPY)) {
	    /* Copy mode only works if there are two
	     * buffers to use. It conflicts with
	     * double buffering
	     */
	    TK(ifp)->copy_flag = 1;
	    WIN(ifp)->mi_doublebuffer = 0;
	    TK(ifp)->front_flag = 1;
	    glDrawBuffer(GL_FRONT);
	} else {
	    TK(ifp)->copy_flag = 0;
	}

	XGetWindowAttributes(TK(ifp)->dispp, TK(ifp)->wind, &xwa);
	TK(ifp)->win_width = xwa.width;
	TK(ifp)->win_height = xwa.height;

	/* clear entire window */
	glViewport(0, 0, TK(ifp)->win_width, TK(ifp)->win_height);
	glClearColor(0, 0, 0, 0);
	glClear(GL_COLOR_BUFFER_BIT);

	/* Set normal viewport size to minimum of actual window
	 * size and requested framebuffer size
	 */
	TK(ifp)->vp_width = (TK(ifp)->win_width < ifp->i->if_width) ?
	    TK(ifp)->win_width : ifp->i->if_width;
	TK(ifp)->vp_height = (TK(ifp)->win_height < ifp->i->if_height) ?
	    TK(ifp)->win_height : ifp->i->if_height;
	ifp->i->if_xcenter = TK(ifp)->vp_width/2;
	ifp->i->if_ycenter = TK(ifp)->vp_height/2;

	/* center viewport in window */
	WIN(ifp)->mi_xoff=(TK(ifp)->win_width-TK(ifp)->vp_width)/2;
	WIN(ifp)->mi_yoff=(TK(ifp)->win_height-TK(ifp)->vp_height)/2;
	glViewport(WIN(ifp)->mi_xoff,
		   WIN(ifp)->mi_yoff,
		   TK(ifp)->vp_width,
		   TK(ifp)->vp_height);
	/* initialize clipping planes and zoom */
	fb_clipper(ifp);
	clp = &(TK(ifp)->clip);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(clp->oleft, clp->oright, clp->obottom, clp->otop,
		-1.0, 1.0);
	glPixelZoom((float) ifp->i->if_xzoom, (float) ifp->i->if_yzoom);
    } else if ((TK(ifp)->win_width > ifp->i->if_width) ||
	       (TK(ifp)->win_height > ifp->i->if_height)) {
	/* clear whole buffer if window larger than framebuffer */
	if (TK(ifp)->copy_flag && !TK(ifp)->front_flag) {
	    glDrawBuffer(GL_FRONT);
	    glViewport(0, 0, TK(ifp)->win_width,
		       TK(ifp)->win_height);
	    glClearColor(0, 0, 0, 0);
	    glClear(GL_COLOR_BUFFER_BIT);
	    glDrawBuffer(GL_BACK);
	} else {
	    glViewport(0, 0, TK(ifp)->win_width,
		       TK(ifp)->win_height);
	    glClearColor(0, 0, 0, 0);
	    glClear(GL_COLOR_BUFFER_BIT);
	}
	/* center viewport */
	glViewport(WIN(ifp)->mi_xoff,
		   WIN(ifp)->mi_yoff,
		   TK(ifp)->vp_width,
		   TK(ifp)->vp_height);
    }

    /* repaint entire image */
    tk_xmit_scanlines(ifp, 0, ifp->i->if_height, 0, ifp->i->if_width);
    if (TK(ifp)->copy_flag) {
	backbuffer_to_screen(ifp, -1);
    }

    if (FB_DEBUG) {
	int dbb, db, view[4], getster, getaux;
	glGetIntegerv(GL_VIEWPORT, view);
	glGetIntegerv(GL_DOUBLEBUFFER, &dbb);
	glGetIntegerv(GL_DRAW_BUFFER, &db);
	printf("Viewport: x %d y %d width %d height %d\n", view[0],
	       view[1], view[2], view[3]);
	printf("expose: double buffered: %d, draw buffer %d\n", dbb, db);
	printf("front %d\tback%d\n", GL_FRONT, GL_BACK);
	glGetIntegerv(GL_STEREO, &getster);
	glGetIntegerv(GL_AUX_BUFFERS, &getaux);
	printf("double %d, stereo %d, aux %d\n", dbb, getster, getaux);
    }

    /* unattach context for other threads to use */
    OSMesaMakeCurrent(TK(ifp)->glxc, TK(ifp)->buf, GL_UNSIGNED_BYTE, TK(ifp)->win_width, TK(ifp)->win_height);
}


int
tk_configureWindow(struct fb *ifp, int width, int height)
{
    if (width == TK(ifp)->win_width &&
	height == TK(ifp)->win_height)
	return 1;

    ifp->i->if_width = ifp->i->if_max_width = width;
    ifp->i->if_height = ifp->i->if_max_height = height;

    TK(ifp)->win_width = TK(ifp)->vp_width = width;
    TK(ifp)->win_height = TK(ifp)->vp_height = height;

    ifp->i->if_zoomflag = 0;
    ifp->i->if_xzoom = 1;
    ifp->i->if_yzoom = 1;
    ifp->i->if_xcenter = width/2;
    ifp->i->if_ycenter = height/2;

    tk_getmem(ifp);
    fb_clipper(ifp);
    return 0;
}


HIDDEN void
tk_do_event(struct fb *ifp)
{
    XEvent event;

    while (XCheckWindowEvent(TK(ifp)->dispp, TK(ifp)->wind,
			     TK(ifp)->event_mask, &event)) {
	switch (event.type) {
	    case Expose:
		if (!TK(ifp)->use_ext_ctrl)
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
				register struct fb_pixel *oglp;

				x = event.xbutton.x;
				y = ifp->i->if_height - event.xbutton.y - 1;

				if (x < 0 || y < 0) {
				    fb_log("No RGB (outside image viewport)\n");
				    break;
				}

				oglp = (struct fb_pixel *)&ifp->i->if_mem[
				    (y*WIN(ifp)->mi_pixwidth)*
				    sizeof(struct fb_pixel) ];

				fb_log("At image (%d, %d), real RGB=(%3d %3d %3d)\n",
				       x, y, (int)oglp[x].red, (int)oglp[x].green, (int)oglp[x].blue);

				break;
			    }
			case Button3:
			    TK(ifp)->alive = 0;
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

		    if (conf->width == TK(ifp)->win_width &&
			conf->height == TK(ifp)->win_height)
			return;

		    tk_configureWindow(ifp, conf->width, conf->height);
		}
	    default:
		break;
	}
    }
}


/**
 * Check for a color map being linear in R, G, and B.  Returns 1 for
 * linear map, 0 for non-linear map (i.e., non-identity map).
 */
HIDDEN int
is_linear_cmap(register struct fb *ifp)
{
    register int i;

    for (i = 0; i < 256; i++) {
	if (CMR(ifp)[i] != i) return 0;
	if (CMG(ifp)[i] != i) return 0;
	if (CMB(ifp)[i] != i) return 0;
    }
    return 1;
}


HIDDEN int
fb_tk_open(struct fb *ifp, const char *file, int width, int height)
{
    static char title[128] = {0};
    int mode;

    FB_CK_FB(ifp->i);

    /*
     * First, attempt to determine operating mode for this open,
     * based upon the "unit number" or flags.
     * file = "/dev/ogl###"
     */
    mode = MODE_2LINGERING | MODE_12DELAY_WRITES_TILL_FLUSH;

    if (file != NULL) {
	const char *cp;
	char modebuf[80];
	char *mp;
	int alpha;
	struct modeflags *mfp;

	if (bu_strncmp(file, ifp->i->if_name, strlen(ifp->i->if_name))) {
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
		for (mfp = modeflags; mfp->c != '\0'; mfp++) {
		    if (mfp->c == *cp) {
			mode = (mode&~mfp->mask)|mfp->value;
			break;
		    }
		}
		if (mfp->c == '\0' && *cp != '-') {
		    fb_log("if_ogl: unknown option '%c' ignored\n", *cp);
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
	    tk_zapmem();
	    return -1;
	}
    }
#if DIRECT_COLOR_VISUAL_ALLOWED
    ifp->i->if_mode = mode;
#else
    ifp->i->if_mode = mode|MODE_7SWCMAP;
#endif

    /*
     * Allocate extension memory sections,
     * addressed by WIN(ifp)->mi_xxx and TK(ifp)->xxx
     */

    if ((WINL(ifp) = (char *)calloc(1, sizeof(struct wininfo))) == NULL) {
	fb_log("fb_tk_open:  wininfo malloc failed\n");
	return -1;
    }
    if ((TKL(ifp) = (char *)calloc(1, sizeof(struct oglinfo))) == NULL) {
	fb_log("fb_tk_open:  oglinfo malloc failed\n");
	return -1;
    }

    /* default to no shared memory */
    WIN(ifp)->mi_shmid = -1;

    /* use defaults if invalid width and height specified */
    if (width <= 0)
	width = ifp->i->if_width;
    if (height <= 0)
	height = ifp->i->if_height;
    /* use max values if width and height are greater */
    if (width > ifp->i->if_max_width)
	width = ifp->i->if_max_width;
    if (height > ifp->i->if_max_height)
	height = ifp->i->if_max_height;

    ifp->i->if_width = width;
    ifp->i->if_height = height;

    /* Attach to shared memory, potentially with a screen repaint */
    if (tk_getmem(ifp) < 0)
	return -1;

    WIN(ifp)->mi_curs_on = 1;

    /* Build a descriptive window title bar */
    snprintf(title, 128, "BRL-CAD /dev/ogl %s, %s",
	     ((ifp->i->if_mode & MODE_2MASK) == MODE_2TRANSIENT) ?
	     "Transient Win":
	     "Lingering Win",
	     ((ifp->i->if_mode & MODE_1MASK) == MODE_1MALLOC) ?
	     "Private Mem" :
	     "Shared Mem");

    /* initialize window state variables before calling tk_getmem */
    ifp->i->if_zoomflag = 0;
    ifp->i->if_xzoom = 1;	/* for zoom fakeout */
    ifp->i->if_yzoom = 1;	/* for zoom fakeout */
    ifp->i->if_xcenter = width/2;
    ifp->i->if_ycenter = height/2;

    TK(ifp)->alive = 1;
    TK(ifp)->firstTime = 1;

    /* Loop through events until first exposure event is processed */
    while (TK(ifp)->firstTime == 1)
	tk_do_event(ifp);

    return 0;
}


static int
open_existing(struct fb *ifp, Display *dpy, Window win, Colormap cmap, int width, int height, OSMesaContext glxc, int double_buffer, int soft_cmap)
{
    /*XXX for now use private memory */
    ifp->i->if_mode = MODE_1MALLOC;

    /*
     * Allocate extension memory sections,
     * addressed by WIN(ifp)->mi_xxx and TK(ifp)->xxx
     */

    if ((WINL(ifp) = (char *)calloc(1, sizeof(struct wininfo))) == NULL) {
	fb_log("fb_tk_open:  wininfo malloc failed\n");
	return -1;
    }
    if ((TKL(ifp) = (char *)calloc(1, sizeof(struct oglinfo))) == NULL) {
	fb_log("fb_tk_open:  oglinfo malloc failed\n");
	return -1;
    }

    TK(ifp)->use_ext_ctrl = 1;

    /* default to no shared memory */
    WIN(ifp)->mi_shmid = -1;

    ifp->i->if_width = ifp->i->if_max_width = width;
    ifp->i->if_height = ifp->i->if_max_height = height;

    TK(ifp)->win_width = TK(ifp)->vp_width = width;
    TK(ifp)->win_height = TK(ifp)->vp_height = height;

    WIN(ifp)->mi_curs_on = 1;

    /* initialize window state variables before calling tk_getmem */
    ifp->i->if_zoomflag = 0;
    ifp->i->if_xzoom = 1;	/* for zoom fakeout */
    ifp->i->if_yzoom = 1;	/* for zoom fakeout */
    ifp->i->if_xcenter = width/2;
    ifp->i->if_ycenter = height/2;

    /* Attach to shared memory, potentially with a screen repaint */
    if (tk_getmem(ifp) < 0)
	return -1;

    TK(ifp)->dispp = dpy;
    ifp->i->if_selfd = ConnectionNumber(TK(ifp)->dispp);

    TK(ifp)->glxc = glxc;
    WIN(ifp)->mi_cmap_flag = !is_linear_cmap(ifp);
    TK(ifp)->soft_cmap_flag = soft_cmap;
    WIN(ifp)->mi_doublebuffer = double_buffer;
    TK(ifp)->xcmap = cmap;

    TK(ifp)->wind = win;
    ++tk_nwindows;

    TK(ifp)->alive = 1;
    TK(ifp)->firstTime = 1;

    fb_clipper(ifp);

    return 0;
}


HIDDEN struct fb_platform_specific *
tk_get_fbps(uint32_t magic)
{
    struct fb_platform_specific *fb_ps = NULL;
    struct tk_fb_info *data = NULL;
    BU_GET(fb_ps, struct fb_platform_specific);
    BU_GET(data, struct tk_fb_info);
    fb_ps->magic = magic;
    fb_ps->data = data;
    return fb_ps;
}


HIDDEN void
tk_put_fbps(struct fb_platform_specific *fbps)
{
    BU_CKMAG(fbps, FB_OGL_MAGIC, "ogl framebuffer");
    BU_PUT(fbps->data, struct tk_fb_info);
    BU_PUT(fbps, struct fb_platform_specific);
    return;
}


HIDDEN int
tk_open_existing(struct fb *ifp, int width, int height, struct fb_platform_specific *fb_p)
{
    struct tk_fb_info *tk_internal = (struct tk_fb_info *)fb_p->data;
    BU_CKMAG(fb_p, FB_OGL_MAGIC, "ogl framebuffer");
    return open_existing(ifp, tk_internal->dpy, tk_internal->win,
			 0, width, height, tk_internal->glxc,
			 tk_internal->double_buffer, tk_internal->soft_cmap);

    return 0;
}


int
tk_close_existing(struct fb *ifp)
{
    if (TK(ifp)->cursor)
	XDestroyWindow(TK(ifp)->dispp, TK(ifp)->cursor);

    if (WINL(ifp) != NULL) {

	/* free up memory associated with image */
	if (WIN(ifp)->mi_shmid != -1) {

	    errno = 0;

	    /* detach from shared memory */
	    if (shmdt(ifp->i->if_mem) == -1) {
		fb_log("fb_tk_close: shmdt failed, errno=%d\n", errno);
		perror("shmdt");
		return -1;
	    }
	} else {
	    /* free private memory */
	    (void)free(ifp->i->if_mem);
	}

	/* free state information */
	(void)free((char *)WINL(ifp));
	WINL(ifp) = NULL;
    }

    if (TKL(ifp) != NULL) {
	(void)free((char *)TKL(ifp));
	TKL(ifp) = NULL;
    }

    return 0;
}


HIDDEN int
tk_final_close(struct fb *ifp)
{
    Display *display = TK(ifp)->dispp;
    Window window = TK(ifp)->wind;
    Colormap colormap = TK(ifp)->xcmap;

    if (FB_DEBUG)
	printf("tk_final_close: All done...goodbye!\n");

    tk_close_existing(ifp);

    XDestroyWindow(display, window);
    XFreeColormap(display, colormap);

    tk_nwindows--;
    return 0;
}


HIDDEN int
tk_flush(struct fb *ifp)
{
    if (FB_DEBUG)
	printf("flushing, copy flag is %d\n", TK(ifp)->copy_flag);

    if ((ifp->i->if_mode & MODE_12MASK) == MODE_12DELAY_WRITES_TILL_FLUSH) {
	if (OSMesaMakeCurrent(TK(ifp)->glxc, TK(ifp)->buf, GL_UNSIGNED_BYTE, TK(ifp)->win_width, TK(ifp)->win_height)) {
	    fb_log("Warning, tk_flush: OSMesaMakeCurrent unsuccessful.\n");
	}

	/* Send entire in-memory buffer to the screen, all at once */
	tk_xmit_scanlines(ifp, 0, ifp->i->if_height, 0, ifp->i->if_width);
	if (TK(ifp)->copy_flag) {
	    backbuffer_to_screen(ifp, -1);
	}

	/* unattach context for other threads to use, also flushes */
	OSMesaMakeCurrent(TK(ifp)->glxc, TK(ifp)->buf, GL_UNSIGNED_BYTE, TK(ifp)->win_width, TK(ifp)->win_height);
    }

    XFlush(TK(ifp)->dispp);
    glFlush();

    return 0;
}


/*
 * Handle any pending input events
 */
HIDDEN int
tk_poll(struct fb *ifp)
{
    tk_do_event(ifp);

    if (TK(ifp)->alive)
	return 0;
    return 1;
}


HIDDEN int
fb_tk_close(struct fb *ifp)
{
    tk_flush(ifp);

    /* only the last open window can linger -
     * call final_close if not lingering
     */
    if (tk_nwindows > 1 ||
	(ifp->i->if_mode & MODE_2MASK) == MODE_2TRANSIENT)
	return tk_final_close(ifp);

    if (FB_DEBUG)
	printf("fb_tk_close: remaining open to linger awhile.\n");

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

    while (!tk_poll(ifp)) {
	bu_snooze(fb_poll_rate(ifp));
    }

    return 0;
}


/*
 * Free shared memory resources, and close.
 */
HIDDEN int
tk_free(struct fb *ifp)
{
    int ret;

    if (FB_DEBUG)
	printf("entering tk_free\n");

    /* Close the framebuffer */
    ret = tk_final_close(ifp);

    if ((ifp->i->if_mode & MODE_1MASK) == MODE_1SHARED) {
	/* If shared mem, release the shared memory segment */
	tk_zapmem();
    }
    return ret;
}


HIDDEN int
tk_clear(struct fb *ifp, unsigned char *pp)
{
    struct fb_pixel bg;
    register struct fb_pixel *oglp;
    register int cnt;
    register int y;

    if (FB_DEBUG)
	printf("entering tk_clear\n");

    /* Set clear colors */
    if (pp != RGBPIXEL_NULL) {
	bg.alpha = 0;
	bg.red   = (pp)[RED];
	bg.green = (pp)[GRN];
	bg.blue  = (pp)[BLU];
    } else {
	bg.alpha = 0;
	bg.red   = 0;
	bg.green = 0;
	bg.blue  = 0;
    }

    /* Flood rectangle in shared memory */
    for (y = 0; y < ifp->i->if_height; y++) {
	oglp = (struct fb_pixel *)&ifp->i->if_mem[
	    (y*WIN(ifp)->mi_pixwidth)*sizeof(struct fb_pixel) ];
	for (cnt = ifp->i->if_width-1; cnt >= 0; cnt--) {
	    *oglp++ = bg;	/* struct copy */
	}
    }

    if (TK(ifp)->use_ext_ctrl) {
	return 0;
    }

    if (OSMesaMakeCurrent(TK(ifp)->glxc, TK(ifp)->buf, GL_UNSIGNED_BYTE, TK(ifp)->win_width, TK(ifp)->win_height)) {
	fb_log("Warning, tk_clear: OSMesaMakeCurrent unsuccessful.\n");
    }

    if (pp != RGBPIXEL_NULL) {
	glClearColor(pp[RED]/255.0, pp[GRN]/255.0, pp[BLU]/255.0, 0.0);
    } else {
	glClearColor(0, 0, 0, 0);
    }

    if (TK(ifp)->copy_flag) {
	/* COPY mode: clear both buffers */
	if (TK(ifp)->front_flag) {
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
    }

    /* unattach context for other threads to use */
    OSMesaMakeCurrent(TK(ifp)->glxc, TK(ifp)->buf, GL_UNSIGNED_BYTE, TK(ifp)->win_width, TK(ifp)->win_height);

    return 0;
}


HIDDEN int
tk_view(struct fb *ifp, int xcenter, int ycenter, int xzoom, int yzoom)
{
    struct fb_clip *clp;

    if (FB_DEBUG)
	printf("entering tk_view\n");

    if (xzoom < 1) xzoom = 1;
    if (yzoom < 1) yzoom = 1;
    if (ifp->i->if_xcenter == xcenter && ifp->i->if_ycenter == ycenter
	&& ifp->i->if_xzoom == xzoom && ifp->i->if_yzoom == yzoom)
	return 0;

    if (xcenter < 0 || xcenter >= ifp->i->if_width)
	return -1;
    if (ycenter < 0 || ycenter >= ifp->i->if_height)
	return -1;
    if (xzoom >= ifp->i->if_width || yzoom >= ifp->i->if_height)
	return -1;

    ifp->i->if_xcenter = xcenter;
    ifp->i->if_ycenter = ycenter;
    ifp->i->if_xzoom = xzoom;
    ifp->i->if_yzoom = yzoom;

    if (ifp->i->if_xzoom > 1 || ifp->i->if_yzoom > 1)
	ifp->i->if_zoomflag = 1;
    else ifp->i->if_zoomflag = 0;


    if (TK(ifp)->use_ext_ctrl) {
	fb_clipper(ifp);
    } else {
        if (OSMesaMakeCurrent(TK(ifp)->glxc, TK(ifp)->buf, GL_UNSIGNED_BYTE, TK(ifp)->win_width, TK(ifp)->win_height)) {
	    fb_log("Warning, tk_view: glXMakeCurrent unsuccessful.\n");
	}

	/* Set clipping matrix and zoom level */
	glMatrixMode(GL_PROJECTION);
	if (TK(ifp)->copy_flag && !TK(ifp)->front_flag) {
	    /* COPY mode - no changes to backbuffer copy - just
	     * need to update front buffer
	     */
	    glPopMatrix();
	    glDrawBuffer(GL_FRONT);
	    TK(ifp)->front_flag = 1;
	}
	glLoadIdentity();

	fb_clipper(ifp);
	clp = &(TK(ifp)->clip);
	glOrtho(clp->oleft, clp->oright, clp->obottom, clp->otop, -1.0, 1.0);
	glPixelZoom((float) ifp->i->if_xzoom, (float) ifp->i->if_yzoom);

	if (TK(ifp)->copy_flag) {
	    backbuffer_to_screen(ifp, -1);
	} else {
	    tk_xmit_scanlines(ifp, 0, ifp->i->if_height, 0, ifp->i->if_width);
	}
	glFlush();

	/* unattach context for other threads to use */
        OSMesaMakeCurrent(TK(ifp)->glxc, TK(ifp)->buf, GL_UNSIGNED_BYTE, TK(ifp)->win_width, TK(ifp)->win_height);
    }

    return 0;
}


HIDDEN int
tk_getview(struct fb *ifp, int *xcenter, int *ycenter, int *xzoom, int *yzoom)
{
    if (FB_DEBUG)
	printf("entering tk_getview\n");

    *xcenter = ifp->i->if_xcenter;
    *ycenter = ifp->i->if_ycenter;
    *xzoom = ifp->i->if_xzoom;
    *yzoom = ifp->i->if_yzoom;

    return 0;
}


/* read count pixels into pixelp starting at x, y */
HIDDEN ssize_t
tk_read(struct fb *ifp, int x, int y, unsigned char *pixelp, size_t count)
{
    size_t n;
    size_t scan_count;	/* # pix on this scanline */
    register unsigned char *cp;
    ssize_t ret;
    register struct fb_pixel *oglp;

    if (FB_DEBUG)
	printf("entering tk_read\n");

    if (x < 0 || x >= ifp->i->if_width ||
	y < 0 || y >= ifp->i->if_height)
	return -1;

    ret = 0;
    cp = (unsigned char *)(pixelp);

    while (count) {
	if (y >= ifp->i->if_height)
	    break;

	if (count >= (size_t)(ifp->i->if_width-x))
	    scan_count = ifp->i->if_width-x;
	else
	    scan_count = count;

	oglp = (struct fb_pixel *)&ifp->i->if_mem[
	    (y*WIN(ifp)->mi_pixwidth+x)*sizeof(struct fb_pixel) ];

	n = scan_count;
	while (n) {
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
	if (++y >= ifp->i->if_height)
	    break;
    }
    return ret;
}


/* write count pixels from pixelp starting at xstart, ystart */
HIDDEN ssize_t
tk_write(struct fb *ifp, int xstart, int ystart, const unsigned char *pixelp, size_t count)
{
    size_t scan_count;	/* # pix on this scanline */
    register unsigned char *cp;
    ssize_t ret;
    int ybase;
    size_t pix_count;	/* # pixels to send */
    register int x;
    register int y;

    if (FB_DEBUG)
	printf("entering tk_write\n");

    /* fast exit cases */
    pix_count = count;
    if (pix_count == 0)
	return 0;	/* OK, no pixels transferred */

    x = xstart;
    ybase = y = ystart;

    if (x < 0 || x >= ifp->i->if_width ||
	y < 0 || y >= ifp->i->if_height)
	return -1;

    ret = 0;
    cp = (unsigned char *)(pixelp);

    while (pix_count) {
	size_t n;
	register struct fb_pixel *oglp;

	if (y >= ifp->i->if_height)
	    break;

	if (pix_count >= (size_t)(ifp->i->if_width-x))
	    scan_count = (size_t)(ifp->i->if_width-x);
	else
	    scan_count = pix_count;

	oglp = (struct fb_pixel *)&ifp->i->if_mem[
	    (y*WIN(ifp)->mi_pixwidth+x)*sizeof(struct fb_pixel) ];

	n = scan_count;
	if ((n & 3) != 0) {
	    /* This code uses 60% of all CPU time */
	    while (n) {
		/* alpha channel is always zero */
		oglp->red   = cp[RED];
		oglp->green = cp[GRN];
		oglp->blue  = cp[BLU];
		oglp++;
		cp += 3;
		n--;
	    }
	} else {
	    while (n) {
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
	if (++y >= ifp->i->if_height)
	    break;
    }

    if ((ifp->i->if_mode & MODE_12MASK) == MODE_12DELAY_WRITES_TILL_FLUSH)
	return ret;

    if (!TK(ifp)->use_ext_ctrl) {

        if (OSMesaMakeCurrent(TK(ifp)->glxc, TK(ifp)->buf, GL_UNSIGNED_BYTE, TK(ifp)->win_width, TK(ifp)->win_height)) {
	    fb_log("Warning, tk_write: glXMakeCurrent unsuccessful.\n");
	}

	if (xstart + count < (size_t)ifp->i->if_width) {
	    tk_xmit_scanlines(ifp, ybase, 1, xstart, count);
	    if (TK(ifp)->copy_flag) {
		/* repaint one scanline from backbuffer */
		backbuffer_to_screen(ifp, ybase);
	    }
	} else {
	    /* Normal case -- multi-pixel write */
	    /* just write rectangle */
	    tk_xmit_scanlines(ifp, ybase, y-ybase, 0, ifp->i->if_width);
	    if (TK(ifp)->copy_flag) {
		backbuffer_to_screen(ifp, -1);
	    }
	}
	glFlush();

	/* unattach context for other threads to use */
        OSMesaMakeCurrent(TK(ifp)->glxc, TK(ifp)->buf, GL_UNSIGNED_BYTE, TK(ifp)->win_width, TK(ifp)->win_height);
    }

    return ret;

}


/*
 * The task of this routine is to reformat the pixels into WIN
 * internal form, and then arrange to have them sent to the screen
 * separately.
 */
HIDDEN int
tk_writerect(struct fb *ifp, int xmin, int ymin, int width, int height, const unsigned char *pp)
{
    register int x;
    register int y;
    register unsigned char *cp;
    register struct fb_pixel *oglp;

    if (FB_DEBUG)
	printf("entering tk_writerect\n");


    if (width <= 0 || height <= 0)
	return 0;  /* do nothing */
    if (xmin < 0 || xmin+width > ifp->i->if_width ||
	ymin < 0 || ymin+height > ifp->i->if_height)
	return -1; /* no can do */

    cp = (unsigned char *)(pp);
    for (y = ymin; y < ymin+height; y++) {
	oglp = (struct fb_pixel *)&ifp->i->if_mem[
	    (y*WIN(ifp)->mi_pixwidth+xmin)*sizeof(struct fb_pixel) ];
	for (x = xmin; x < xmin+width; x++) {
	    /* alpha channel is always zero */
	    oglp->red   = cp[RED];
	    oglp->green = cp[GRN];
	    oglp->blue  = cp[BLU];
	    oglp++;
	    cp += 3;
	}
    }

    if ((ifp->i->if_mode & MODE_12MASK) == MODE_12DELAY_WRITES_TILL_FLUSH)
	return width*height;

    if (!TK(ifp)->use_ext_ctrl) {
        if (OSMesaMakeCurrent(TK(ifp)->glxc, TK(ifp)->buf, GL_UNSIGNED_BYTE, TK(ifp)->win_width, TK(ifp)->win_height)) {
	    fb_log("Warning, tk_writerect: glXMakeCurrent unsuccessful.\n");
	}


	/* just write rectangle*/
	tk_xmit_scanlines(ifp, ymin, height, xmin, width);
	if (TK(ifp)->copy_flag) {
	    backbuffer_to_screen(ifp, -1);
	}

	/* unattach context for other threads to use */
	OSMesaMakeCurrent(TK(ifp)->glxc, TK(ifp)->buf, GL_UNSIGNED_BYTE, TK(ifp)->win_width, TK(ifp)->win_height);
    }

    return width*height;
}


/*
 * The task of this routine is to reformat the pixels into WIN
 * internal form, and then arrange to have them sent to the screen
 * separately.
 */
HIDDEN int
tk_bwwriterect(struct fb *ifp, int xmin, int ymin, int width, int height, const unsigned char *pp)
{
    register int x;
    register int y;
    register unsigned char *cp;
    register struct fb_pixel *oglp;

    if (FB_DEBUG)
	printf("entering tk_bwwriterect\n");


    if (width <= 0 || height <= 0)
	return 0;  /* do nothing */
    if (xmin < 0 || xmin+width > ifp->i->if_width ||
	ymin < 0 || ymin+height > ifp->i->if_height)
	return -1; /* no can do */

    cp = (unsigned char *)(pp);
    for (y = ymin; y < ymin+height; y++) {
	oglp = (struct fb_pixel *)&ifp->i->if_mem[
	    (y*WIN(ifp)->mi_pixwidth+xmin)*sizeof(struct fb_pixel) ];
	for (x = xmin; x < xmin+width; x++) {
	    register int val;
	    /* alpha channel is always zero */
	    oglp->red   = (val = *cp++);
	    oglp->green = val;
	    oglp->blue  = val;
	    oglp++;
	}
    }

    if ((ifp->i->if_mode & MODE_12MASK) == MODE_12DELAY_WRITES_TILL_FLUSH)
	return width*height;

    if (!TK(ifp)->use_ext_ctrl) {
        if (OSMesaMakeCurrent(TK(ifp)->glxc, TK(ifp)->buf, GL_UNSIGNED_BYTE, TK(ifp)->win_width, TK(ifp)->win_height)) {
	    fb_log("Warning, tk_writerect: glXMakeCurrent unsuccessful.\n");
	}

	/* just write rectangle*/
	tk_xmit_scanlines(ifp, ymin, height, xmin, width);
	if (TK(ifp)->copy_flag) {
	    backbuffer_to_screen(ifp, -1);
	}

	/* unattach context for other threads to use */
	OSMesaMakeCurrent(TK(ifp)->glxc, TK(ifp)->buf, GL_UNSIGNED_BYTE, TK(ifp)->win_width, TK(ifp)->win_height);
    }

    return width*height;
}


HIDDEN int
tk_rmap(register struct fb *ifp, register ColorMap *cmp)
{
    register int i;

    if (FB_DEBUG)
	printf("entering tk_rmap\n");

    /* Just parrot back the stored colormap */
    for (i = 0; i < 256; i++) {
	cmp->cm_red[i]   = CMR(ifp)[i]<<8;
	cmp->cm_green[i] = CMG(ifp)[i]<<8;
	cmp->cm_blue[i]  = CMB(ifp)[i]<<8;
    }
    return 0;
}


HIDDEN int
tk_wmap(register struct fb *ifp, register const ColorMap *cmp)
{
    register int i;
    int prev;	/* !0 = previous cmap was non-linear */

    if (FB_DEBUG)
	printf("entering tk_wmap\n");

    prev = WIN(ifp)->mi_cmap_flag;
    if (cmp == COLORMAP_NULL) {
	tk_cminit(ifp);
    } else {
	for (i = 0; i < 256; i++) {
	    CMR(ifp)[i] = cmp-> cm_red[i]>>8;
	    CMG(ifp)[i] = cmp-> cm_green[i]>>8;
	    CMB(ifp)[i] = cmp-> cm_blue[i]>>8;
	}
    }
    WIN(ifp)->mi_cmap_flag = !is_linear_cmap(ifp);


    if (!TK(ifp)->use_ext_ctrl) {
	if (TK(ifp)->soft_cmap_flag) {
	    /* if current and previous maps are linear, return */
	    if (WIN(ifp)->mi_cmap_flag == 0 && prev == 0) return 0;

	    /* Software color mapping, trigger a repaint */

	    if (OSMesaMakeCurrent(TK(ifp)->glxc, TK(ifp)->buf, GL_UNSIGNED_BYTE, TK(ifp)->win_width, TK(ifp)->win_height)) {
		fb_log("Warning, tk_wmap: glXMakeCurrent unsuccessful.\n");
	    }

	    tk_xmit_scanlines(ifp, 0, ifp->i->if_height, 0, ifp->i->if_width);
	    if (TK(ifp)->copy_flag) {
		backbuffer_to_screen(ifp, -1);
	    }

	    /* unattach context for other threads to use, also flushes */
	    OSMesaMakeCurrent(TK(ifp)->glxc, TK(ifp)->buf, GL_UNSIGNED_BYTE, TK(ifp)->win_width, TK(ifp)->win_height);
	} else {
	    /* Send color map to hardware */
	    /* This code has yet to be tested */

	    for (i = 0; i < 256; i++) {
		color_cell[i].pixel = i;
		color_cell[i].red = CMR(ifp)[i];
		color_cell[i].green = CMG(ifp)[i];
		color_cell[i].blue = CMB(ifp)[i];
		color_cell[i].flags = DoRed | DoGreen | DoBlue;
	    }
	    XStoreColors(TK(ifp)->dispp, TK(ifp)->xcmap, color_cell, 256);
	}
    }

    return 0;
}


HIDDEN int
tk_help(struct fb *ifp)
{
    struct modeflags *mfp;

    fb_log("Description: %s\n", ifp->i->if_type);
    fb_log("Device: %s\n", ifp->i->if_name);
    fb_log("Max width height: %d %d\n",
	   ifp->i->if_max_width,
	   ifp->i->if_max_height);
    fb_log("Default width height: %d %d\n",
	   ifp->i->if_width,
	   ifp->i->if_height);
    fb_log("Usage: /dev/ogl[option letters]\n");
    for (mfp = modeflags; mfp->c != '\0'; mfp++) {
	fb_log("   %c   %s\n", mfp->c, mfp->help);
    }

    fb_log("\nCurrent internal state:\n");
    fb_log("	mi_doublebuffer=%d\n", WIN(ifp)->mi_doublebuffer);
    fb_log("	mi_cmap_flag=%d\n", WIN(ifp)->mi_cmap_flag);
    fb_log("	tk_nwindows=%d\n", tk_nwindows);

    return 0;
}


HIDDEN int
tk_setcursor(struct fb *ifp, const unsigned char *UNUSED(bits), int UNUSED(xbits), int UNUSED(ybits), int UNUSED(xorig), int UNUSED(yorig))
{
    FB_CK_FB(ifp->i);

    return 0;
}


HIDDEN int
tk_cursor(struct fb *ifp, int mode, int x, int y)
{
    /* Update position of cursor */
    ifp->i->if_cursmode = mode;
    ifp->i->if_xcurs = x;
    ifp->i->if_ycurs = y;

    return 0;
}


int
tk_refresh(struct fb *ifp, int x, int y, int w, int h)
{
    int mm;
    struct fb_clip *clp;

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

    fb_clipper(ifp);
    clp = &(TK(ifp)->clip);
    glOrtho(clp->oleft, clp->oright, clp->obottom, clp->otop, -1.0, 1.0);
    glPixelZoom((float) ifp->i->if_xzoom, (float) ifp->i->if_yzoom);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    glViewport(0, 0, TK(ifp)->win_width, TK(ifp)->win_height);
    tk_xmit_scanlines(ifp, y, h, x, w);
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
    glMatrixMode(mm);

    if (!TK(ifp)->use_ext_ctrl) {
	glFlush();
    }

    return 0;
}


/* This is the ONLY thing that we normally "export" */
struct fb_impl tk_interface_impl =  {
    0,			/* magic number slot */
    FB_OGL_MAGIC,
    fb_tk_open,	/* open device */
    tk_open_existing,    /* existing device_open */
    tk_close_existing,    /* existing device_close */
    tk_get_fbps,         /* get platform specific memory */
    tk_put_fbps,         /* free platform specific memory */
    fb_tk_close,	/* close device */
    tk_clear,		/* clear device */
    tk_read,		/* read pixels */
    tk_write,		/* write pixels */
    tk_rmap,		/* read colormap */
    tk_wmap,		/* write colormap */
    tk_view,		/* set view */
    tk_getview,	/* get view */
    tk_setcursor,	/* define cursor */
    tk_cursor,		/* set cursor */
    fb_sim_getcursor,	/* get cursor */
    fb_sim_readrect,	/* read rectangle */
    tk_writerect,	/* write rectangle */
    fb_sim_bwreadrect,
    tk_bwwriterect,	/* write rectangle */
    tk_configureWindow,
    tk_refresh,
    tk_poll,		/* process events */
    tk_flush,		/* flush output */
    tk_free,		/* free resources */
    tk_help,		/* help message */
    "Silicon Graphics OpenGL",	/* device description */
    FB_XMAXSCREEN,	/* max width */
    FB_YMAXSCREEN,	/* max height */
    "/dev/ogl",		/* short device name */
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
    50000,		/* refresh rate */
    {0}, /* u1 */
    {0}, /* u2 */
    {0}, /* u3 */
    {0}, /* u4 */
    {0}, /* u5 */
    {0}  /* u6 */
};


struct fb tk_interface =  { &tk_interface_impl };

#ifdef DM_PLUGIN
static const struct fb_plugin finfo = { &tk_interface };

COMPILER_DLLEXPORT const struct fb_plugin *fb_plugin_info()
{
    return &finfo;
}
#endif

/* Because class is actually used to access a struct
 * entry in this file, preserve our redefinition
 * of class for the benefit of avoiding C++ name
 * collisions until the end of this file */
#undef class

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
