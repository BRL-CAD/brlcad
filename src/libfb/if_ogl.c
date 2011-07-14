/*                        I F _ O G L . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2011 United States Government as represented by
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
/** @file if_ogl.c
 *
 * Frame Buffer Library interface for OpenGL.
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

#ifdef IF_OGL

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

/* glx.h on Mac OS X (and perhaps elsewhere) defines a slew of
 * parameter names that shadow system symbols.  protect the system
 * symbols by redefining the parameters prior to header inclusion.
 */
#define j1 J1
#define y1 Y1
#define read rd
#define index idx
#define access acs
#define remainder rem
#ifdef HAVE_GL_GLX_H
#  include <GL/glx.h>
#endif
#undef remainder
#undef access
#undef index
#undef read
#undef y1
#undef j1
#ifdef HAVE_GL_GL_H
#  include <GL/gl.h>
#endif

#ifdef HAVE_UNISTD_H
#  include <unistd.h>	/* for getpagesize */
#endif

#ifdef HAVE_SYS_WAIT_H
#  include <sys/wait.h>
#endif
#include "bio.h"

#include "bu.h"
#include "fb.h"


#define CJDEBUG 0
#define DIRECT_COLOR_VISUAL_ALLOWED 0

/* XXX - arbitrary upper bound */
#define XMAXSCREEN 16383
#define YMAXSCREEN 16383

HIDDEN int ogl_nwindows = 0; 	/* number of open windows */
HIDDEN XColor color_cell[256];		/* used to set colormap */


/*
 * Structure of color map in shared memory region.  Has exactly the
 * same format as the SGI hardware "gammaramp" map Note that only the
 * lower 8 bits are significant.
 */
struct ogl_cmap {
    short cmr[256];
    short cmg[256];
    short cmb[256];
};


/*
 * This defines the format of the in-memory framebuffer copy.  The
 * alpha component and reverse order are maintained for compatibility
 * with /dev/sgi
 */
struct ogl_pixel {
    unsigned char blue;
    unsigned char green;
    unsigned char red;
    unsigned char alpha;
};


/* Clipping structure for zoom/pan operations */
struct ogl_clip {
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
    struct ogl_pixel mi_scanline[XMAXSCREEN+1];	/* one scanline */
};


/*
 * Per window state information particular to the OpenGL interface
 */
struct oglinfo {
    GLXContext glxc;
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
    struct ogl_clip clip;	/* current view clipping */
    Window cursor;
    XVisualInfo *vip;		/* pointer to info on current visual */
    Colormap xcmap;		/* xstyle color map */
    int use_ext_ctrl;		/* for controlling the Ogl graphics engine externally */
};


#define SGI(ptr) ((struct sgiinfo *)((ptr)->u1.p))
#define SGIL(ptr) ((ptr)->u1.p)	/* left hand side version */
#define OGL(ptr) ((struct oglinfo *)((ptr)->u6.p))
#define OGLL(ptr) ((ptr)->u6.p)	/* left hand side version */
#define if_mem u2.p		/* shared memory pointer */
#define if_cmap u3.p		/* color map in shared memory */
#define CMR(x) ((struct ogl_cmap *)((x)->if_cmap))->cmr
#define CMG(x) ((struct ogl_cmap *)((x)->if_cmap))->cmg
#define CMB(x) ((struct ogl_cmap *)((x)->if_cmap))->cmb
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


/*
 * S I G K I D
 */
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
    struct ogl_clip *clp;

    if (!(OGL(ifp)->front_flag)) {
	OGL(ifp)->front_flag = 1;
	glDrawBuffer(GL_FRONT);
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	glPixelZoom((float) ifp->if_xzoom, (float) ifp->if_yzoom);
    }

    clp = &(OGL(ifp)->clip);

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
	if (clp->xscrmax >= OGL(ifp)->vp_width) glRecti(ifp->if_width - CLIP_XTRA,
							clp->yscrmin - CLIP_XTRA,
							clp->xscrmax + CLIP_XTRA,
							clp->yscrmax + CLIP_XTRA);

	/* Blank out area above image */
	if (clp->yscrmax >= OGL(ifp)->vp_height) glRecti(clp->xscrmin - CLIP_XTRA,
							 OGL(ifp)->vp_height - CLIP_XTRA,
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


/* O G L _ X M I T _ S C A N L I N E S
 *
 * Note: unlike sgi_xmit_scanlines, this function updates an arbitrary
 * rectangle of the frame buffer
 */
HIDDEN void
ogl_xmit_scanlines(register FBIO *ifp, int ybase, int nlines, int xbase, int npix)
{
    register int y;
    register int n;
    int sw_cmap;	/* !0 => needs software color map */
    struct ogl_clip *clp;

    /* Caller is expected to handle attaching context, etc. */

    clp = &(OGL(ifp)->clip);

    if (OGL(ifp)->soft_cmap_flag  && SGI(ifp)->mi_cmap_flag) {
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

    if (!OGL(ifp)->use_ext_ctrl) {
	if (!OGL(ifp)->copy_flag) {
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

	} else if (OGL(ifp)->front_flag) {
	    /* in COPY mode, always draw full sized image into backbuffer.
	     * backbuffer_to_screen() is used to update the front buffer
	     */
	    glDrawBuffer(GL_BACK);
	    OGL(ifp)->front_flag = 0;
	    glMatrixMode(GL_PROJECTION);
	    glPushMatrix();	/* store current view clipping matrix*/
	    glLoadIdentity();
	    glOrtho(-0.25, ((GLdouble) OGL(ifp)->vp_width)-0.25,
		    -0.25, ((GLdouble) OGL(ifp)->vp_height)-0.25,
		    -1.0, 1.0);
	    glPixelZoom(1.0, 1.0);
	}
    }

    if (sw_cmap) {
	/* Software colormap each line as it's transmitted */
	register int x;
	register struct ogl_pixel *oglp;
	register struct ogl_pixel *op;

	y = ybase;
	if (CJDEBUG) printf("Doing sw colormap xmit\n");
	/* Perform software color mapping into temp scanline */
	op = SGI(ifp)->mi_scanline;
	for (n=nlines; n>0; n--, y++) {
	    oglp = (struct ogl_pixel *)&ifp->if_mem[
		(y*SGI(ifp)->mi_memwidth)*
		sizeof(struct ogl_pixel) ];
	    for (x=xbase+npix-1; x>=xbase; x--) {
		op[x].red   = CMR(ifp)[oglp[x].red];
		op[x].green = CMG(ifp)[oglp[x].green];
		op[x].blue  = CMB(ifp)[oglp[x].blue];
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


/**
 * O G L _ C M I N I T
 */
HIDDEN void
ogl_cminit(register FBIO *ifp)
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
 * O G L _ G E T M E M
 *
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
ogl_getmem(FBIO *ifp)
{
#define SHMEM_KEY 42
    int pixsize;
    int size;
    int i;
    char *sp;
    int new = 0;

    errno = 0;

    if ((ifp->if_mode & MODE_1MASK) == MODE_1MALLOC) {
	/*
	 * In this mode, only malloc as much memory as is needed.
	 */
	SGI(ifp)->mi_memwidth = ifp->if_width;
	pixsize = ifp->if_height * ifp->if_width * sizeof(struct ogl_pixel);
	size = pixsize + sizeof(struct ogl_cmap);

	sp = calloc(1, size);
	if (sp == 0) {
	    fb_log("ogl_getmem: frame buffer memory malloc failed\n");
	    goto fail;
	}
	new = 1;
	goto success;
    }

    /* The shared memory section never changes size */
    SGI(ifp)->mi_memwidth = ifp->if_max_width;

    /*
     * On some platforms lrectwrite() runs off the end!  So, provide a
     * pad area of 2 scanlines.  (1 line is enough, but this avoids
     * risk of damage to colormap table.)
     */
    pixsize = (ifp->if_max_height+2) * ifp->if_max_width *
	sizeof(struct ogl_pixel);

    size = pixsize + sizeof(struct ogl_cmap);
    size = (size + getpagesize()-1) & ~(getpagesize()-1);

    /* First try to attach to an existing one */
    if ((SGI(ifp)->mi_shmid = shmget(SHMEM_KEY, size, 0)) < 0) {
	/* No existing one, create a new one */
	if ((SGI(ifp)->mi_shmid = shmget(
		 SHMEM_KEY, size, IPC_CREAT|0666)) < 0) {
	    fb_log("ogl_getmem: shmget failed, errno=%d\n", errno);
	    goto fail;
	}
	new = 1;
    }

    /* WWW this is unnecessary in this version? */
    /* Open the segment Read/Write */
    /* This gets mapped to a high address on some platforms, so no problem. */
    if ((sp = shmat(SGI(ifp)->mi_shmid, 0, 0)) == (char *)(-1L)) {
	fb_log("ogl_getmem: shmat returned x%x, errno=%d\n", sp, errno);
	goto fail;
    }

success:
    ifp->if_mem = sp;
    ifp->if_cmap = sp + pixsize;	/* cmap at end of area */
    i = CMB(ifp)[255];		/* try to deref last word */
    CMB(ifp)[255] = i;

    /* Provide non-black colormap on creation of new shared mem */
    if (new)
	ogl_cminit(ifp);
    return 0;
fail:
    fb_log("ogl_getmem:  Unable to attach to shared memory.\n");
    if ((sp = calloc(1, size)) == NULL) {
	fb_log("ogl_getmem:  malloc failure\n");
	return -1;
    }
    new = 1;
    goto success;
}


/**
 * O G L _ Z A P M E M
 */
void
ogl_zapmem(void)
{
    int shmid;
    int i;

    if ((shmid = shmget(SHMEM_KEY, 0, 0)) < 0) {
	fb_log("ogl_zapmem shmget failed, errno=%d\n", errno);
	return;
    }

    i = shmctl(shmid, IPC_RMID, 0);
    if (i < 0) {
	fb_log("ogl_zapmem shmctl failed, errno=%d\n", errno);
	return;
    }
    fb_log("if_ogl: shared memory released\n");
}


/**
 * O G L _ C L I P P E R
 *
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
ogl_clipper(register FBIO *ifp)
{
    register struct ogl_clip *clp;
    register int i;
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
    if (OGL(ifp)->copy_flag) {
	if (clp->xpixmax > OGL(ifp)->vp_width-1) {
	    clp->xpixmax = OGL(ifp)->vp_width-1;
	}
	if (clp->ypixmax > OGL(ifp)->vp_height-1) {
	    clp->ypixmax = OGL(ifp)->vp_height-1;
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
    XWindowAttributes xwa;
    struct ogl_clip *clp;

    if (CJDEBUG) fb_log("entering expose_callback()\n");

    if (glXMakeCurrent(OGL(ifp)->dispp, OGL(ifp)->wind, OGL(ifp)->glxc)==False) {
	fb_log("Warning, expose_callback: glXMakeCurrent unsuccessful.\n");
    }

    if (OGL(ifp)->firstTime) {

	OGL(ifp)->firstTime = 0;

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
	    OGL(ifp)->copy_flag = 1;
	    SGI(ifp)->mi_doublebuffer = 0;
	    OGL(ifp)->front_flag = 1;
	    glDrawBuffer(GL_FRONT);
	} else {
	    OGL(ifp)->copy_flag = 0;
	}

	XGetWindowAttributes(OGL(ifp)->dispp, OGL(ifp)->wind, &xwa);
	OGL(ifp)->win_width = xwa.width;
	OGL(ifp)->win_height = xwa.height;

	/* clear entire window */
	glViewport(0, 0, OGL(ifp)->win_width, OGL(ifp)->win_height);
	glClearColor(0, 0, 0, 0);
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
	glOrtho(clp->oleft, clp->oright, clp->obottom, clp->otop,
		-1.0, 1.0);
	glPixelZoom((float) ifp->if_xzoom, (float) ifp->if_yzoom);
    } else if ((OGL(ifp)->win_width > ifp->if_width) ||
	       (OGL(ifp)->win_height > ifp->if_height)) {
	/* clear whole buffer if window larger than framebuffer */
	if (OGL(ifp)->copy_flag && !OGL(ifp)->front_flag) {
	    glDrawBuffer(GL_FRONT);
	    glViewport(0, 0, OGL(ifp)->win_width,
		       OGL(ifp)->win_height);
	    glClearColor(0, 0, 0, 0);
	    glClear(GL_COLOR_BUFFER_BIT);
	    glDrawBuffer(GL_BACK);
	} else {
	    glViewport(0, 0, OGL(ifp)->win_width,
		       OGL(ifp)->win_height);
	    glClearColor(0, 0, 0, 0);
	    glClear(GL_COLOR_BUFFER_BIT);
	}
	/* center viewport */
	glViewport(SGI(ifp)->mi_xoff,
		   SGI(ifp)->mi_yoff,
		   OGL(ifp)->vp_width,
		   OGL(ifp)->vp_height);
    }

    /* repaint entire image */
    ogl_xmit_scanlines(ifp, 0, ifp->if_height, 0, ifp->if_width);
    if (SGI(ifp)->mi_doublebuffer) {
	glXSwapBuffers(OGL(ifp)->dispp, OGL(ifp)->wind);
    } else if (OGL(ifp)->copy_flag) {
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
    glXMakeCurrent(OGL(ifp)->dispp, None, NULL);
}


void
ogl_configureWindow(FBIO *ifp, int width, int height)
{
    if (width == OGL(ifp)->win_width &&
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


HIDDEN void
ogl_do_event(FBIO *ifp)
{
    XEvent event;

    while (XCheckWindowEvent(OGL(ifp)->dispp, OGL(ifp)->wind,
			     OGL(ifp)->event_mask, &event)) {
	switch (event.type) {
	    case Expose:
		if (!OGL(ifp)->use_ext_ctrl)
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
				register struct ogl_pixel *oglp;

				x = event.xbutton.x;
				y = ifp->if_height - event.xbutton.y - 1;

				if (x < 0 || y < 0) {
				    fb_log("No RGB (outside image viewport)\n");
				    break;
				}

				oglp = (struct ogl_pixel *)&ifp->if_mem[
				    (y*SGI(ifp)->mi_memwidth)*
				    sizeof(struct ogl_pixel) ];

				fb_log("At image (%d, %d), real RGB=(%3d %3d %3d)\n",
				       x, y, (int)oglp[x].red, (int)oglp[x].green, (int)oglp[x].blue);

				break;
			    }
			case Button3:
			    OGL(ifp)->alive = 0;
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

		    if (conf->width == OGL(ifp)->win_width &&
			conf->height == OGL(ifp)->win_height)
			return;

		    ogl_configureWindow(ifp, conf->width, conf->height);
		}
	    default:
		break;
	}
    }
}


/**
 * O G L _ C H O O S E _ V I S U A L
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
 * We first try to satisfy all requirements and desires. If that
 * fails, we remove the desires one at a time until we succeed or
 * until only requirements are left. If at any stage more than one
 * visual meets the current criteria, the visual with the greatest
 * depth is chosen.
 *
 * The following flags are set:
 * SGI(ifp)->mi_doublebuffer
 * OGL(ifp)->soft_cmap_flag
 *
 * Return NULL on failure.
 */
HIDDEN XVisualInfo *
fb_ogl_choose_visual(FBIO *ifp)
{

    XVisualInfo *vip, *vibase, *maxvip, template;
#define NGOOD 200
    int good[NGOOD];
    int num, i, j;
    int m_hard_cmap, m_sing_buf, m_doub_buf;
    int use, rgba, dbfr;

    m_hard_cmap = ((ifp->if_mode & MODE_7MASK)==MODE_7NORMAL);
    m_sing_buf  = ((ifp->if_mode & MODE_9MASK)==MODE_9SINGLEBUF);
    m_doub_buf =  !m_sing_buf;

    memset((void *)&template, 0, sizeof(XVisualInfo));

    /* get a list of all visuals on this display */
    vibase = XGetVisualInfo(OGL(ifp)->dispp, 0, &template, &num);
    while (1) {

	/* search for all visuals matching current criteria */
	for (i=0, j=0, vip=vibase; i<num; i++, vip++) {
	    /* requirements */
	    glXGetConfig(OGL(ifp)->dispp, vip, GLX_USE_GL, &use);
	    if (!use)
		continue;
	    glXGetConfig(OGL(ifp)->dispp, vip, GLX_RGBA, &rgba);
	    if (!rgba)
		continue;
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
	    if ((m_hard_cmap) && (vip->class!=DirectColor))
		continue;
	    if ((m_hard_cmap) && (vip->colormap_size<256))
		continue;
	    glXGetConfig(OGL(ifp)->dispp, vip, GLX_DOUBLEBUFFER, &dbfr);
	    if ((m_doub_buf) && (!dbfr))
		continue;
	    if ((m_sing_buf) && (dbfr))
		continue;

	    /* this visual meets criteria */
	    if (j >= NGOOD-1) {
		fb_log("fb_ogl_open:  More than %d candidate visuals!\n", NGOOD);
		break;
	    }
	    good[j++] = i;
	}

	/* from list of acceptable visuals,
	 * choose the visual with the greatest depth */
	if (j>=1) {
	    maxvip = vibase + good[0];
	    for (i=1; i<j; i++) {
		vip = vibase + good[i];
		if (vip->depth > maxvip->depth) {
		    maxvip = vip;
		}
	    }
	    /* set flags and return choice */
	    OGL(ifp)->soft_cmap_flag = !m_hard_cmap;
	    SGI(ifp)->mi_doublebuffer = m_doub_buf;
	    return maxvip;
	}

	/* if no success at this point,
	 * relax one of the criteria and try again.
	 */
	if (m_hard_cmap) {
	    /* relax hardware colormap requirement */
	    m_hard_cmap = 0;
	    fb_log("fb_ogl_open: hardware colormapping not available. Using software colormap.\n");
	} else if (m_sing_buf) {
	    /* relax single buffering requirement.
	     * no need for any warning - we'll just use
	     * the front buffer
	     */
	    m_sing_buf = 0;
	} else if (m_doub_buf) {
	    /* relax double buffering requirement. */
	    m_doub_buf = 0;
	    fb_log("fb_ogl_open: double buffering not available. Using single buffer.\n");
	} else {
	    /* nothing else to relax */
	    return NULL;
	}

    }

}


/**
 * I S _ L I N E A R _ C M A P
 *
 * Check for a color map being linear in R, G, and B.  Returns 1 for
 * linear map, 0 for non-linear map (ie, non-identity map).
 */
HIDDEN int
is_linear_cmap(register FBIO *ifp)
{
    register int i;

    for (i=0; i<256; i++) {
	if (CMR(ifp)[i] != i) return 0;
	if (CMG(ifp)[i] != i) return 0;
	if (CMB(ifp)[i] != i) return 0;
    }
    return 1;
}


HIDDEN int
fb_ogl_open(FBIO *ifp, char *file, int width, int height)
{
    static char title[128];
    int mode, i, direct;
    long valuemask;
    XSetWindowAttributes swa;

    FB_CK_FBIO(ifp);

    /*
     * First, attempt to determine operating mode for this open,
     * based upon the "unit number" or flags.
     * file = "/dev/ogl###"
     */
    mode = MODE_2LINGERING;

    if (file != NULL) {
	register char *cp;
	char modebuf[80];
	char *mp;
	int alpha;
	struct modeflags *mfp;

	if (strncmp(file, ifp->if_name, strlen(ifp->if_name))) {
	    /* How did this happen? */
	    mode = 0;
	} else {
	    /* Parse the options */
	    alpha = 0;
	    mp = &modebuf[0];
	    cp = &file[8];
	    while (*cp != '\0' && !isspace(*cp)) {
		*mp++ = *cp;	/* copy it to buffer */
		if (isdigit(*cp)) {
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
	    ogl_zapmem();
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
     * addressed by SGI(ifp)->mi_xxx and OGL(ifp)->xxx
     */

    if ((SGIL(ifp) = (char *)calloc(1, sizeof(struct sgiinfo))) == NULL) {
	fb_log("fb_ogl_open:  sgiinfo malloc failed\n");
	return -1;
    }
    if ((OGLL(ifp) = (char *)calloc(1, sizeof(struct oglinfo))) == NULL) {
	fb_log("fb_ogl_open:  oglinfo malloc failed\n");
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

    if ((ifp->if_mode & MODE_2MASK) == MODE_2LINGERING) {
	/* save parent pid for later signalling */
	SGI(ifp)->mi_parent = bu_process_id();

	signal(SIGUSR1, sigkid);

#if 0
	if ((f = fork()) != 0) {
	    /* Parent process */
	    int k;
	    int f;
	    int status;

	    /* parent doesn't need these any more */
	    for (k=0; k < 20; k++) {
		close(k);
	    }

	    /*
	     * Wait until the child dies, of whatever cause,
	     * or until the child kills us.
	     * Pretty vicious, this computer society.
	     */
	    while ((k = wait(&status)) != -1 && k != f)
		; /* NULL */

	    exit(0);
	    /* NOTREACHED */
	} else if (f < 0) {
	    fb_log("fb_ogl_open:  linger-mode fork failure\n");
	    return -1;
	}
	/* Child Process falls through */
#endif
    }

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
    (void)snprintf(title, 128, "BRL-CAD /dev/ogl %s, %s",
		   ((ifp->if_mode & MODE_2MASK) == MODE_2TRANSIENT) ?
		   "Transient Win":
		   "Lingering Win",
		   ((ifp->if_mode & MODE_1MASK) == MODE_1MALLOC) ?
		   "Private Mem" :
		   "Shared Mem");

    /* initialize window state variables before calling ogl_getmem */
    ifp->if_zoomflag = 0;
    ifp->if_xzoom = 1;	/* for zoom fakeout */
    ifp->if_yzoom = 1;	/* for zoom fakeout */
    ifp->if_xcenter = width/2;
    ifp->if_ycenter = height/2;
    SGI(ifp)->mi_pid = bu_process_id();

    /* Attach to shared memory, potentially with a screen repaint */
    if (ogl_getmem(ifp) < 0)
	return -1;

    /* Open an X connection to the display.  Sending NULL to XOpenDisplay
       tells it to use the DISPLAY environment variable. */
    if ((OGL(ifp)->dispp = XOpenDisplay(NULL)) == NULL) {
	fb_log("fb_ogl_open: Failed to open display.  Check DISPLAY environment variable.\n");
	return -1;
    }
    ifp->if_selfd = ConnectionNumber(OGL(ifp)->dispp);
    if (CJDEBUG) {
	printf("Connection opened to X display on fd %d.\n",
	       ConnectionNumber(OGL(ifp)->dispp));
    }

    /* Choose an appropriate visual. */
    if ((OGL(ifp)->vip = fb_ogl_choose_visual(ifp)) == NULL) {
	fb_log("fb_ogl_open: Couldn't find an appropriate visual.  Exiting.\n");
	return -1;
    }

    /* Open an OpenGL context with this visual*/
    OGL(ifp)->glxc = glXCreateContext(OGL(ifp)->dispp, OGL(ifp)->vip, 0, GL_TRUE /* direct context */);
    if (OGL(ifp)->glxc == NULL) {
	fb_log("ERROR: Couldn't create an OpenGL context!\n");
	return -1;
    }

    direct = glXIsDirect(OGL(ifp)->dispp, OGL(ifp)->glxc);
    if (CJDEBUG) {
	fb_log("Framebuffer drawing context is %s.\n", direct ? "direct" : "indirect");
    }

    /* Create a colormap for this visual */
    SGI(ifp)->mi_cmap_flag = !is_linear_cmap(ifp);
    if (!OGL(ifp)->soft_cmap_flag) {
	OGL(ifp)->xcmap = XCreateColormap(OGL(ifp)->dispp,
					  RootWindow(OGL(ifp)->dispp,
						     OGL(ifp)->vip->screen),
					  OGL(ifp)->vip->visual,
					  AllocAll);
	/* initialize virtual colormap - it will be loaded into
	 * the hardware. This code has not yet been tested.
	 */
	if (CJDEBUG) printf("Loading read/write colormap.\n");
	for (i = 0; i < 256; i++) {
	    color_cell[i].pixel = i;
	    color_cell[i].red = CMR(ifp)[i];
	    color_cell[i].green = CMG(ifp)[i];
	    color_cell[i].blue = CMB(ifp)[i];
	    color_cell[i].flags = DoRed | DoGreen | DoBlue;
	}
	XStoreColors(OGL(ifp)->dispp, OGL(ifp)->xcmap, color_cell, 256);
    } else {
	/* read only colormap */
	if (CJDEBUG) {
	    printf("Allocating read-only colormap.");
	}
	OGL(ifp)->xcmap = XCreateColormap(OGL(ifp)->dispp,
					  RootWindow(OGL(ifp)->dispp,
						     OGL(ifp)->vip->screen),
					  OGL(ifp)->vip->visual,
					  AllocNone);
    }

    XSync(OGL(ifp)->dispp, 0);

    /* Create a window. */
    memset(&swa, 0, sizeof(swa));

    valuemask = CWBackPixel | CWBorderPixel | CWEventMask | CWColormap;

    swa.background_pixel = BlackPixel(OGL(ifp)->dispp,
				      OGL(ifp)->vip->screen);
    swa.border_pixel = BlackPixel(OGL(ifp)->dispp,
				  OGL(ifp)->vip->screen);
    swa.event_mask = OGL(ifp)->event_mask =
	ExposureMask | KeyPressMask | KeyReleaseMask |
	ButtonPressMask | ButtonReleaseMask;
    swa.colormap = OGL(ifp)->xcmap;

#define XCreateWindowDebug(display, parent, x, y, width, height,	\
			   border_width, depth, class, visual, valuemask, \
			   attributes)					\
    (printf("XCreateWindow(display = %08X, \n", (long)display),		\
     printf("                parent = %08X, \n", (long)parent),		\
     printf("                     x = %d, \n", x),			\
     printf("                     y = %d, \n", y),			\
     printf("                 width = %d, \n", width),			\
     printf("                height = %d, \n", height),			\
     printf("          border_width = %d, \n", border_width),		\
     printf("                 depth = %d, \n", depth),			\
     printf("                 class = %d, \n", class),			\
     printf("                visual = %08X, \n", (long)visual),		\
     printf("             valuemask = %08X, \n", valuemask),		\
     printf("            attributes = {"),				\
     (valuemask & CWBackPixmap) ? printf(" background_pixmap = %08X ", (long)((attributes)->background_pixmap)) : 0, \
     (valuemask & CWBackPixel) ? printf(" background_pixel = %08X ", (attributes)->background_pixel) : 0, \
     (valuemask & CWBorderPixmap) ? printf(" border_pixmap = %08X ", (long)((attributes)->border_pixmap)) : 0, \
     (valuemask & CWBorderPixel) ? printf(" border_pixel = %08X ", (attributes)->border_pixel) : 0, \
     (valuemask & CWBitGravity) ? printf(" bit_gravity = %d ", (attributes)->bit_gravity) : 0, \
     (valuemask & CWWinGravity) ? printf(" win_gravity = %d ", (attributes)->win_gravity) : 0, \
     (valuemask & CWBackingStore) ? printf(" backing_store = %d ", (attributes)->backing_store) : 0, \
     (valuemask & CWBackingPlanes) ? printf(" backing_planes = %d ", (attributes)->backing_planes) : 0, \
     (valuemask & CWBackingPixel) ? printf(" backing_pixel = %08X ", (attributes)->backing_pixel) : 0, \
     (valuemask & CWOverrideRedirect) ? printf(" override_redirect = %d ", (attributes)->override_redirect) : 0, \
     (valuemask & CWSaveUnder) ? printf(" save_under = %d ", (attributes)->save_under) : 0, \
     (valuemask & CWEventMask) ? printf(" event_mask = %08X ", (attributes)->event_mask) : 0, \
     (valuemask & CWDontPropagate) ? printf(" do_not_propagate_mask = %08X ", (attributes)->do_not_propagate_mask) : 0, \
     (valuemask & CWColormap) ? printf(" colormap = %08X ", (long)((attributes)->colormap)) : 0, \
     (valuemask & CWCursor) ? printf(" cursor = %08X ", (long)((attributes)->cursor)) : 0, \
     printf(" }\n")) > 0 ? XCreateWindow(display, parent, x, y, width, height, border_width, depth, class, visual, valuemask, attributes) : -1;

    OGL(ifp)->wind = XCreateWindow(OGL(ifp)->dispp,
				   RootWindow(OGL(ifp)->dispp,
					      OGL(ifp)->vip->screen),
				   0, 0, ifp->if_width, ifp->if_height, 0,
				   OGL(ifp)->vip->depth,
				   InputOutput,
				   OGL(ifp)->vip->visual,
				   valuemask, &swa);

    XStoreName(OGL(ifp)->dispp, OGL(ifp)->wind, title);

    /* count windows */
    ogl_nwindows++;
    XMapRaised(OGL(ifp)->dispp, OGL(ifp)->wind);

    OGL(ifp)->alive = 1;
    OGL(ifp)->firstTime = 1;

    /* Loop through events until first exposure event is processed */
    while (OGL(ifp)->firstTime == 1)
	ogl_do_event(ifp);

    return 0;
}


int
_ogl_open_existing(FBIO *ifp, Display *dpy, Window win, Colormap cmap, XVisualInfo *vip, int width, int height, GLXContext glxc, int double_buffer, int soft_cmap)
{

    /*XXX for now use private memory */
    ifp->if_mode = MODE_1MALLOC;

    /*
     * Allocate extension memory sections,
     * addressed by SGI(ifp)->mi_xxx and OGL(ifp)->xxx
     */

    if ((SGIL(ifp) = (char *)calloc(1, sizeof(struct sgiinfo))) == NULL) {
	fb_log("fb_ogl_open:  sgiinfo malloc failed\n");
	return -1;
    }
    if ((OGLL(ifp) = (char *)calloc(1, sizeof(struct oglinfo))) == NULL) {
	fb_log("fb_ogl_open:  oglinfo malloc failed\n");
	return -1;
    }

    OGL(ifp)->use_ext_ctrl = 1;

    SGI(ifp)->mi_shmid = -1;	/* indicate no shared memory */
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
    SGI(ifp)->mi_pid = bu_process_id();

    /* Attach to shared memory, potentially with a screen repaint */
    if (ogl_getmem(ifp) < 0)
	return -1;

    OGL(ifp)->dispp = dpy;
    ifp->if_selfd = ConnectionNumber(OGL(ifp)->dispp);

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


int
ogl_open_existing(FBIO *ifp, int argc, char **argv)
{
    Display *dpy;
    Window win;
    Colormap cmap;
    XVisualInfo *vip;
    int width;
    int height;
    GLXContext glxc;
    int double_buffer;
    int soft_cmap;

    if (argc != 10)
	return -1;

    if (sscanf(argv[1], "%p", (void **)&dpy) != 1)
	return -1;

    if (sscanf(argv[2], "%p", (void **)&win) != 1)
	return -1;

    if (sscanf(argv[3], "%p", (void **)&cmap) != 1)
	return -1;

    if (sscanf(argv[4], "%p", (void **)&vip) != 1)
	return -1;

    if (sscanf(argv[5], "%d", &width) != 1)
	return -1;

    if (sscanf(argv[6], "%d", &height) != 1)
	return -1;

    if (sscanf(argv[7], "%p", (void **)&glxc) != 1)
	return -1;

    if (sscanf(argv[8], "%d", &double_buffer) != 1)
	return -1;

    if (sscanf(argv[9], "%d", &soft_cmap) != 1)
	return -1;

    return _ogl_open_existing(ifp, dpy, win, cmap, vip, width, height,
			      glxc, double_buffer, soft_cmap);
}


HIDDEN int
ogl_final_close(FBIO *ifp)
{

    if (CJDEBUG) {
	printf("ogl_final_close: All done...goodbye!\n");
    }

    if (OGL(ifp)->cursor)
	XDestroyWindow(OGL(ifp)->dispp, OGL(ifp)->cursor);

    XDestroyWindow(OGL(ifp)->dispp, OGL(ifp)->wind);
    XFreeColormap(OGL(ifp)->dispp, OGL(ifp)->xcmap);

    if (SGIL(ifp) != NULL) {
	/* free up memory associated with image */
	if (SGI(ifp)->mi_shmid != -1) {
	    /* detach from shared memory */
	    if (shmdt(ifp->if_mem) == -1) {
		fb_log("fb_ogl_close shmdt failed, errno=%d\n",
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

    if (OGLL(ifp) != NULL) {
	(void)free((char *)OGLL(ifp));
	OGLL(ifp) = NULL;
    }

    ogl_nwindows--;
    return 0;
}


HIDDEN int
ogl_flush(FBIO *ifp)
{
    if ((ifp->if_mode & MODE_12MASK) == MODE_12DELAY_WRITES_TILL_FLUSH) {
	if (glXMakeCurrent(OGL(ifp)->dispp, OGL(ifp)->wind, OGL(ifp)->glxc)==False) {
	    fb_log("Warning, ogl_flush: glXMakeCurrent unsuccessful.\n");
	}

	/* Send entire in-memory buffer to the screen, all at once */
	ogl_xmit_scanlines(ifp, 0, ifp->if_height, 0, ifp->if_width);
	if (SGI(ifp)->mi_doublebuffer) {
	    glXSwapBuffers(OGL(ifp)->dispp, OGL(ifp)->wind);
	} else if (OGL(ifp)->copy_flag) {
	    backbuffer_to_screen(ifp, -1);
	}

	/* unattach context for other threads to use, also flushes */
	glXMakeCurrent(OGL(ifp)->dispp, None, NULL);
    }
    XFlush(OGL(ifp)->dispp);
    glFlush();
    return 0;
}


HIDDEN int
fb_ogl_close(FBIO *ifp)
{

    ogl_flush(ifp);

    /* only the last open window can linger -
     * call final_close if not lingering
     */
    if (ogl_nwindows > 1 ||
	(ifp->if_mode & MODE_2MASK) == MODE_2TRANSIENT)
	return ogl_final_close(ifp);

    if (CJDEBUG)
	printf("fb_ogl_close: remaining open to linger awhile.\n");

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

    while (0 < OGL(ifp)->alive) {
	ogl_do_event(ifp);
    }

    return 0;
}


int
ogl_close_existing(FBIO *ifp)
{
    if (OGL(ifp)->cursor)
	XDestroyWindow(OGL(ifp)->dispp, OGL(ifp)->cursor);

    if (SGIL(ifp) != NULL) {
	/* free up memory associated with image */
	if (SGI(ifp)->mi_shmid != -1) {
	    /* detach from shared memory */
	    if (shmdt(ifp->if_mem) == -1) {
		fb_log("fb_ogl_close: shmdt failed, errno=%d\n",
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

    if (OGLL(ifp) != NULL) {
	(void)free((char *)OGLL(ifp));
	OGLL(ifp) = NULL;
    }

    return 0;
}


/*
 * O G L _ P O L L
 *
 * Handle any pending input events
 */
HIDDEN int
ogl_poll(FBIO *ifp)
{
    ogl_do_event(ifp);

    if (OGL(ifp)->alive < 0)
	return 1;
    else
	return 0;
}


/*
 * O G L _ F R E E
 *
 * Free shared memory resources, and close.
 */
HIDDEN int
ogl_free(FBIO *ifp)
{
    int ret;

    if (CJDEBUG) printf("entering ogl_free\n");
    /* Close the framebuffer */
    ret = ogl_final_close(ifp);

    if ((ifp->if_mode & MODE_1MASK) == MODE_1SHARED) {
	/* If shared mem, release the shared memory segment */
	ogl_zapmem();
    }
    return ret;
}


HIDDEN int
ogl_clear(FBIO *ifp, unsigned char *pp)

/* pointer to beginning of memory segment*/
{
    struct ogl_pixel bg;
    register struct ogl_pixel *oglp;
    register int cnt;
    register int y;

    if (CJDEBUG) printf("entering ogl_clear\n");

    if (glXMakeCurrent(OGL(ifp)->dispp, OGL(ifp)->wind, OGL(ifp)->glxc)==False) {
	fb_log("Warning, ogl_clear: glXMakeCurrent unsuccessful.\n");
    }

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
    for (y=0; y < ifp->if_height; y++) {
	oglp = (struct ogl_pixel *)&ifp->if_mem[
	    (y*SGI(ifp)->mi_memwidth+0)*sizeof(struct ogl_pixel) ];
	for (cnt=ifp->if_width-1; cnt >= 0; cnt--) {
	    *oglp++ = bg;	/* struct copy */
	}
    }


    /* Update screen */
    if (OGL(ifp)->use_ext_ctrl) {
	glClear(GL_COLOR_BUFFER_BIT);
    } else {
	if (OGL(ifp)->copy_flag) {
	    /* COPY mode: clear both buffers */
	    if (OGL(ifp)->front_flag) {
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
		glXSwapBuffers(OGL(ifp)->dispp, OGL(ifp)->wind);
	    }
	}

    }

    /* unattach context for other threads to use */
    glXMakeCurrent(OGL(ifp)->dispp, None, NULL);

    return 0;
}


/*
 * O G L _ V I E W
 */
HIDDEN int
ogl_view(FBIO *ifp, int xcenter, int ycenter, int xzoom, int yzoom)
{
    struct ogl_clip *clp;

    if (CJDEBUG) printf("entering ogl_view\n");

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


    if (OGL(ifp)->use_ext_ctrl) {
	ogl_clipper(ifp);
    } else {
	if (glXMakeCurrent(OGL(ifp)->dispp, OGL(ifp)->wind, OGL(ifp)->glxc)==False) {
	    fb_log("Warning, ogl_view: glXMakeCurrent unsuccessful.\n");
	}

	/* Set clipping matrix and zoom level */
	glMatrixMode(GL_PROJECTION);
	if (OGL(ifp)->copy_flag && !OGL(ifp)->front_flag) {
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
	glOrtho(clp->oleft, clp->oright, clp->obottom, clp->otop, -1.0, 1.0);
	glPixelZoom((float) ifp->if_xzoom, (float) ifp->if_yzoom);

	if (OGL(ifp)->copy_flag) {
	    backbuffer_to_screen(ifp, -1);
	} else {
	    ogl_xmit_scanlines(ifp, 0, ifp->if_height, 0, ifp->if_width);
	    if (SGI(ifp)->mi_doublebuffer) {
		glXSwapBuffers(OGL(ifp)->dispp, OGL(ifp)->wind);
	    }
	}
	glFlush();

	/* unattach context for other threads to use */
	glXMakeCurrent(OGL(ifp)->dispp, None, NULL);
    }

    return 0;
}


/*
 * O G L _ G E T V I E W
 */
HIDDEN int
ogl_getview(FBIO *ifp, int *xcenter, int *ycenter, int *xzoom, int *yzoom)
{
    if (CJDEBUG) printf("entering ogl_getview\n");

    *xcenter = ifp->if_xcenter;
    *ycenter = ifp->if_ycenter;
    *xzoom = ifp->if_xzoom;
    *yzoom = ifp->if_yzoom;

    return 0;
}


/* read count pixels into pixelp starting at x, y */
HIDDEN int
ogl_read(FBIO *ifp, int x, int y, unsigned char *pixelp, size_t count)
{
    size_t n;
    size_t scan_count;	/* # pix on this scanline */
    register unsigned char *cp;
    int ret;
    register struct ogl_pixel *oglp;

    if (CJDEBUG) printf("entering ogl_read\n");

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

	oglp = (struct ogl_pixel *)&ifp->if_mem[
	    (y*SGI(ifp)->mi_memwidth+x)*sizeof(struct ogl_pixel) ];

	n = scan_count;
	while (n) {
	    cp[RED] = oglp->red;
	    cp[GRN] = oglp->green;
	    cp[BLU] = oglp->blue;
	    oglp++;
	    cp += 3;
	    n--;
	}
	ret += (int)scan_count;
	count -= scan_count;
	x = 0;
	/* Advance upwards */
	if (++y >= ifp->if_height)
	    break;
    }
    return ret;
}


/* write count pixels from pixelp starting at xstart, ystart */
HIDDEN int
ogl_write(FBIO *ifp, int xstart, int ystart, const unsigned char *pixelp, size_t count)
{
    size_t scan_count;	/* # pix on this scanline */
    register unsigned char *cp;
    int ret;
    int ybase;
    size_t pix_count;	/* # pixels to send */
    register int x;
    register int y;

    if (CJDEBUG) printf("entering ogl_write\n");

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
	register struct ogl_pixel *oglp;

	if (y >= ifp->if_height)
	    break;

	if (pix_count >= (size_t)(ifp->if_width-x))
	    scan_count = (size_t)(ifp->if_width-x);
	else
	    scan_count = pix_count;

	oglp = (struct ogl_pixel *)&ifp->if_mem[
	    (y*SGI(ifp)->mi_memwidth+x)*sizeof(struct ogl_pixel) ];

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
	if (++y >= ifp->if_height)
	    break;
    }

    if ((ifp->if_mode & MODE_12MASK) == MODE_12DELAY_WRITES_TILL_FLUSH)
	return ret;

    if (!OGL(ifp)->use_ext_ctrl) {

	if (glXMakeCurrent(OGL(ifp)->dispp, OGL(ifp)->wind, OGL(ifp)->glxc)==False) {
	    fb_log("Warning, ogl_write: glXMakeCurrent unsuccessful.\n");
	}

	if (xstart + count < (size_t)ifp->if_width) {
	    ogl_xmit_scanlines(ifp, ybase, 1, xstart, count);
	    if (OGL(ifp)->copy_flag) {
		/* repaint one scanline from backbuffer */
		backbuffer_to_screen(ifp, ybase);
	    }
	} else {
	    /* Normal case -- multi-pixel write */
	    if (SGI(ifp)->mi_doublebuffer) {
		/* refresh whole screen */
		ogl_xmit_scanlines(ifp, 0, ifp->if_height, 0, ifp->if_width);
		glXSwapBuffers(OGL(ifp)->dispp, OGL(ifp)->wind);
	    } else {
		/* just write rectangle */
		ogl_xmit_scanlines(ifp, ybase, y-ybase, 0, ifp->if_width);
		if (OGL(ifp)->copy_flag) {
		    backbuffer_to_screen(ifp, -1);
		}
	    }
	}
	glFlush();

	/* unattach context for other threads to use */
	glXMakeCurrent(OGL(ifp)->dispp, None, NULL);
    }

    return ret;

}


/*
 * O G L _ W R I T E R E C T
 *
 * The task of this routine is to reformat the pixels into SGI
 * internal form, and then arrange to have them sent to the screen
 * separately.
 */
HIDDEN int
ogl_writerect(FBIO *ifp, int xmin, int ymin, int width, int height, const unsigned char *pp)
{
    register int x;
    register int y;
    register unsigned char *cp;
    register struct ogl_pixel *oglp;

    if (CJDEBUG) printf("entering ogl_writerect\n");


    if (width <= 0 || height <= 0)
	return 0;  /* do nothing */
    if (xmin < 0 || xmin+width > ifp->if_width ||
	ymin < 0 || ymin+height > ifp->if_height)
	return -1; /* no can do */

    cp = (unsigned char *)(pp);
    for (y = ymin; y < ymin+height; y++) {
	oglp = (struct ogl_pixel *)&ifp->if_mem[
	    (y*SGI(ifp)->mi_memwidth+xmin)*sizeof(struct ogl_pixel) ];
	for (x = xmin; x < xmin+width; x++) {
	    /* alpha channel is always zero */
	    oglp->red   = cp[RED];
	    oglp->green = cp[GRN];
	    oglp->blue  = cp[BLU];
	    oglp++;
	    cp += 3;
	}
    }

    if ((ifp->if_mode & MODE_12MASK) == MODE_12DELAY_WRITES_TILL_FLUSH)
	return width*height;

    if (!OGL(ifp)->use_ext_ctrl) {
	if (glXMakeCurrent(OGL(ifp)->dispp, OGL(ifp)->wind, OGL(ifp)->glxc)==False) {
	    fb_log("Warning, ogl_writerect: glXMakeCurrent unsuccessful.\n");
	}

	if (SGI(ifp)->mi_doublebuffer) {
	    /* refresh whole screen */
	    ogl_xmit_scanlines(ifp, 0, ifp->if_height, 0, ifp->if_width);
	    glXSwapBuffers(OGL(ifp)->dispp, OGL(ifp)->wind);
	} else {
	    /* just write rectangle*/
	    ogl_xmit_scanlines(ifp, ymin, height, xmin, width);
	    if (OGL(ifp)->copy_flag) {
		backbuffer_to_screen(ifp, -1);
	    }
	}

	/* unattach context for other threads to use */
	glXMakeCurrent(OGL(ifp)->dispp, None, NULL);
    }

    return width*height;
}


/*
 * O G L _ B W W R I T E R E C T
 *
 * The task of this routine is to reformat the pixels into SGI
 * internal form, and then arrange to have them sent to the screen
 * separately.
 */
HIDDEN int
ogl_bwwriterect(FBIO *ifp, int xmin, int ymin, int width, int height, const unsigned char *pp)
{
    register int x;
    register int y;
    register unsigned char *cp;
    register struct ogl_pixel *oglp;

    if (CJDEBUG) printf("entering ogl_bwwriterect\n");


    if (width <= 0 || height <= 0)
	return 0;  /* do nothing */
    if (xmin < 0 || xmin+width > ifp->if_width ||
	ymin < 0 || ymin+height > ifp->if_height)
	return -1; /* no can do */

    cp = (unsigned char *)(pp);
    for (y = ymin; y < ymin+height; y++) {
	oglp = (struct ogl_pixel *)&ifp->if_mem[
	    (y*SGI(ifp)->mi_memwidth+xmin)*sizeof(struct ogl_pixel) ];
	for (x = xmin; x < xmin+width; x++) {
	    register int val;
	    /* alpha channel is always zero */
	    oglp->red   = (val = *cp++);
	    oglp->green = val;
	    oglp->blue  = val;
	    oglp++;
	}
    }

    if ((ifp->if_mode & MODE_12MASK) == MODE_12DELAY_WRITES_TILL_FLUSH)
	return width*height;

    if (!OGL(ifp)->use_ext_ctrl) {
	if (glXMakeCurrent(OGL(ifp)->dispp, OGL(ifp)->wind, OGL(ifp)->glxc)==False) {
	    fb_log("Warning, ogl_writerect: glXMakeCurrent unsuccessful.\n");
	}

	if (SGI(ifp)->mi_doublebuffer) {
	    /* refresh whole screen */
	    ogl_xmit_scanlines(ifp, 0, ifp->if_height, 0, ifp->if_width);
	    glXSwapBuffers(OGL(ifp)->dispp, OGL(ifp)->wind);
	} else {
	    /* just write rectangle*/
	    ogl_xmit_scanlines(ifp, ymin, height, xmin, width);
	    if (OGL(ifp)->copy_flag) {
		backbuffer_to_screen(ifp, -1);
	    }
	}

	/* unattach context for other threads to use */
	glXMakeCurrent(OGL(ifp)->dispp, None, NULL);
    }

    return width*height;
}


HIDDEN int
ogl_rmap(register FBIO *ifp, register ColorMap *cmp)
{
    register int i;

    if (CJDEBUG) printf("entering ogl_rmap\n");

    /* Just parrot back the stored colormap */
    for (i = 0; i < 256; i++) {
	cmp->cm_red[i]   = CMR(ifp)[i]<<8;
	cmp->cm_green[i] = CMG(ifp)[i]<<8;
	cmp->cm_blue[i]  = CMB(ifp)[i]<<8;
    }
    return 0;
}


/*
 * O G L _ W M A P
 */
HIDDEN int
ogl_wmap(register FBIO *ifp, register const ColorMap *cmp)
{
    register int i;
    int prev;	/* !0 = previous cmap was non-linear */

    if (CJDEBUG) printf("entering ogl_wmap\n");

    prev = SGI(ifp)->mi_cmap_flag;
    if (cmp == COLORMAP_NULL) {
	ogl_cminit(ifp);
    } else {
	for (i = 0; i < 256; i++) {
	    CMR(ifp)[i] = cmp-> cm_red[i]>>8;
	    CMG(ifp)[i] = cmp-> cm_green[i]>>8;
	    CMB(ifp)[i] = cmp-> cm_blue[i]>>8;
	}
    }
    SGI(ifp)->mi_cmap_flag = !is_linear_cmap(ifp);


    if (!OGL(ifp)->use_ext_ctrl) {
	if (OGL(ifp)->soft_cmap_flag) {
	    /* if current and previous maps are linear, return */
	    if (SGI(ifp)->mi_cmap_flag == 0 && prev == 0) return 0;

	    /* Software color mapping, trigger a repaint */

	    if (glXMakeCurrent(OGL(ifp)->dispp, OGL(ifp)->wind, OGL(ifp)->glxc)==False) {
		fb_log("Warning, ogl_wmap: glXMakeCurrent unsuccessful.\n");
	    }

	    ogl_xmit_scanlines(ifp, 0, ifp->if_height, 0, ifp->if_width);
	    if (SGI(ifp)->mi_doublebuffer) {
		glXSwapBuffers(OGL(ifp)->dispp, OGL(ifp)->wind);
	    } else if (OGL(ifp)->copy_flag) {
		backbuffer_to_screen(ifp, -1);
	    }

	    /* unattach context for other threads to use, also flushes */
	    glXMakeCurrent(OGL(ifp)->dispp, None, NULL);
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
	    XStoreColors(OGL(ifp)->dispp, OGL(ifp)->xcmap, color_cell, 256);
	}
    }

    return 0;
}


/*
 * O G L _ H E L P
 */
HIDDEN int
ogl_help(FBIO *ifp)
{
    struct modeflags *mfp;
    XVisualInfo *visual = OGL(ifp)->vip;

    fb_log("Description: %s\n", ifp->if_type);
    fb_log("Device: %s\n", ifp->if_name);
    fb_log("Max width height: %d %d\n",
	   ifp->if_max_width,
	   ifp->if_max_height);
    fb_log("Default width height: %d %d\n",
	   ifp->if_width,
	   ifp->if_height);
    fb_log("Usage: /dev/ogl[option letters]\n");
    for (mfp = modeflags; mfp->c != '\0'; mfp++) {
	fb_log("   %c   %s\n", mfp->c, mfp->help);
    }

    fb_log("\nCurrent internal state:\n");
    fb_log("	mi_doublebuffer=%d\n", SGI(ifp)->mi_doublebuffer);
    fb_log("	mi_cmap_flag=%d\n", SGI(ifp)->mi_cmap_flag);
    fb_log("	ogl_nwindows=%d\n", ogl_nwindows);

    fb_log("X11 Visual:\n");

    switch (visual->class) {
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
    if (visual->depth < 24)
	fb_log("\tWARNING: unable to obtain full 24-bits of color, image will be quantized.\n");

    return 0;
}


HIDDEN int
ogl_setcursor(FBIO *ifp, const unsigned char *UNUSED(bits), int UNUSED(xbits), int UNUSED(ybits), int UNUSED(xorig), int UNUSED(yorig))
{
    FB_CK_FBIO(ifp);

    return 0;
}


HIDDEN int
ogl_cursor(FBIO *ifp, int mode, int x, int y)
{
    if (mode) {
	register int xx, xy;
	register int delta;

	/* If we don't have a cursor, create it */
	if (!OGL(ifp)->cursor) {
	    XSetWindowAttributes xswa;
	    XColor rgb_db_def;
	    XColor bg, bd;

	    XAllocNamedColor(OGL(ifp)->dispp, OGL(ifp)->xcmap, "black",
			     &rgb_db_def, &bg);
	    XAllocNamedColor(OGL(ifp)->dispp, OGL(ifp)->xcmap, "white",
			     &rgb_db_def, &bd);
	    xswa.background_pixel = bg.pixel;
	    xswa.border_pixel = bd.pixel;
	    xswa.colormap = OGL(ifp)->xcmap;
	    xswa.save_under = True;

	    OGL(ifp)->cursor = XCreateWindow(OGL(ifp)->dispp, OGL(ifp)->wind,
					     0, 0, 4, 4, 2, OGL(ifp)->vip->depth, InputOutput,
					     OGL(ifp)->vip->visual, CWBackPixel | CWBorderPixel |
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
	xy = OGL(ifp)->win_height - xy;

	/* Move cursor into place; make it visible if it isn't */
	XMoveWindow(OGL(ifp)->dispp, OGL(ifp)->cursor, xx - 4, xy - 4);

	/* if cursor window is currently not mapped, map it */
	if (!ifp->if_cursmode)
	    XMapRaised(OGL(ifp)->dispp, OGL(ifp)->cursor);
    } else {
	/* If we have a cursor and it's mapped, unmap it */
	if (OGL(ifp)->cursor && ifp->if_cursmode)
	    XUnmapWindow(OGL(ifp)->dispp, OGL(ifp)->cursor);
    }

    /* Without this flush, cursor movement is sluggish */
    XFlush(OGL(ifp)->dispp);

    /* Update position of cursor */
    ifp->if_cursmode = mode;
    ifp->if_xcurs = x;
    ifp->if_ycurs = y;

    return 0;
}


int
ogl_refresh(FBIO *ifp, int x, int y, int w, int h)
{
    int mm;
    struct ogl_clip *clp;

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

    ogl_clipper(ifp);
    clp = &(OGL(ifp)->clip);
    glOrtho(clp->oleft, clp->oright, clp->obottom, clp->otop, -1.0, 1.0);
    glPixelZoom((float) ifp->if_xzoom, (float) ifp->if_yzoom);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    glViewport(0, 0, OGL(ifp)->win_width, OGL(ifp)->win_height);
    ogl_xmit_scanlines(ifp, y, h, x, w);
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
    glMatrixMode(mm);

    glFlush();
    return 0;
}


/* This is the ONLY thing that we normally "export" */
FBIO ogl_interface =
{
    0,			/* magic number slot */
    fb_ogl_open,	/* open device */
    fb_ogl_close,	/* close device */
    ogl_clear,		/* clear device */
    ogl_read,		/* read pixels */
    ogl_write,		/* write pixels */
    ogl_rmap,		/* read colormap */
    ogl_wmap,		/* write colormap */
    ogl_view,		/* set view */
    ogl_getview,	/* get view */
    ogl_setcursor,	/* define cursor */
    ogl_cursor,		/* set cursor */
    fb_sim_getcursor,	/* get cursor */
    fb_sim_readrect,	/* read rectangle */
    ogl_writerect,	/* write rectangle */
    fb_sim_bwreadrect,
    ogl_bwwriterect,	/* write rectangle */
    ogl_poll,		/* process events */
    ogl_flush,		/* flush output */
    ogl_free,		/* free resources */
    ogl_help,		/* help message */
    "Silicon Graphics OpenGL",	/* device description */
    XMAXSCREEN+1,	/* max width */
    YMAXSCREEN+1,	/* max height */
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
    {0}, /* u1 */
    {0}, /* u2 */
    {0}, /* u3 */
    {0}, /* u4 */
    {0}, /* u5 */
    {0}  /* u6 */
};


#else

/* quell empty-compilation unit warnings */
static const int unused = 0;

#endif /* IF_OGL */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
