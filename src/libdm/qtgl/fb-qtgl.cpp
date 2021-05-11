/*                     F B - Q T G L . C P P
 * BRL-CAD
 *
 * Copyright (c) 1989-2021 United States Government as represented by
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
/** @file fb-qtgl.cpp
 *
 * An OpenGL framebuffer using Qt.
 *
 * TODO:  I think the way to go about this is probably going to be to have the
 * fb ALWAYS be an internal fb in a dm.  If we don't get the open existing
 * call, we'll create a dm, store the dm pointer internally, and use that dm's
 * fb.  That way all the implementation logic is the same for both modes and
 * it's just the open and close for fb itself that will need to do more.  That
 * was one of the motivations for libqtcad - so the Qt dm display widget can
 * be re-used for scenarios like this one.
 *
 * Unless it proves impractical for some reason, go with the ISST approach of
 * using and displaying a texture.  The only practical scenario I know of where
 * that was a problem during the osgl work was a default VirtualBox Windows
 * environment where there wasn't any OpenGL support higher than 1.2, and if
 * swrast can handle the isst approach we can use that to guarantee at least
 * some form of working functionality (however slow) even in that scenario.
 *
 * Initially colormap is probably going to be a no-op - I need some practical
 * examples where we need that to figure out how to use it.
 *
 * Not going to worry about shared memory, lingering mode, etc. (again, unless
 * we turn out to need one of them for application support.)
 *
 * If it proves practical we should switch ogl/wgl to common functions like we
 * did for the dm, but the implementation logic looks more convoluted and I'm
 * not sure yet how practical that will prove (particularly since we're not
 * looking to reproduce all the extra modes defined by ogl.)
 */
/** @} */


#include "common.h"

#include "bu/app.h"

#include "./fb-qtgl.h"
extern "C" {
#include "../include/private.h"
}

extern "C" {
extern struct fb qtgl_interface;
}

#include "qtglwin.h"

struct qtglinfo {
    int ac;
    char **av;
    QApplication *qapp = NULL;
    QtGLWin *mw = NULL;
    QOpenGLWidget *glc = NULL;

    struct dm *dmp = NULL;

    struct fb_clip clip;        /* current view clipping */

    int win_width;              /* actual window width */
    int win_height;             /* actual window height */
    int vp_width;               /* actual viewport width */
    int vp_height;              /* actual viewport height */

    int mi_memwidth;            /* width of scanline in if_mem */

    int texid;

    int alive;
};

#define QTGL(ptr) ((struct qtglinfo *)((ptr)->i->pp))
#define QTGLL(ptr) ((ptr)->i->pp)     /* left hand side version */



static void
qt_destroy(struct qtglinfo *qi)
{
    delete qi->mw;
    delete qi->qapp;
    free(qi->av[0]);
    free(qi->av);
}


HIDDEN int
qtgl_getmem(struct fb *ifp)
{
    int pixsize;
    int size;
    char *sp;

    errno = 0;

    {
	/*
	 * only malloc as much memory as is needed.
	 */
	QTGL(ifp)->mi_memwidth = ifp->i->if_width;
	pixsize = ifp->i->if_height * ifp->i->if_width * sizeof(struct fb_pixel);
	size = pixsize + sizeof(struct fb_cmap);

	sp = (char *)calloc(1, size);
	if (sp == 0) {
	    fb_log("qtgl_getmem: frame buffer memory malloc failed\n");
	    goto fail;
	}
	goto success;
    }

success:
    ifp->i->if_mem = sp;

    return 0;
fail:
    if ((sp = (char *)calloc(1, size)) == NULL) {
	fb_log("qtgl_getmem:  malloc failure\n");
	return -1;
    }
    goto success;
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
fb_clipper(struct fb *ifp)
{
    struct fb_clip *clp;
    int i;
    double pixels;

    clp = &(QTGL(ifp)->clip);

    i = QTGL(ifp)->vp_width/(2*ifp->i->if_xzoom);
    clp->xscrmin = ifp->i->if_xcenter - i;
    i = QTGL(ifp)->vp_width/ifp->i->if_xzoom;
    clp->xscrmax = clp->xscrmin + i;
    pixels = (double) i;
    clp->oleft = ((double) clp->xscrmin) - 0.25*pixels/((double) QTGL(ifp)->vp_width);
    clp->oright = clp->oleft + pixels;

    i = QTGL(ifp)->vp_height/(2*ifp->i->if_yzoom);
    clp->yscrmin = ifp->i->if_ycenter - i;
    i = QTGL(ifp)->vp_height/ifp->i->if_yzoom;
    clp->yscrmax = clp->yscrmin + i;
    pixels = (double) i;
    clp->obottom = ((double) clp->yscrmin) - 0.25*pixels/((double) QTGL(ifp)->vp_height);
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

	if (clp->xpixmax > ifp->i->if_width-1) {
	    clp->xpixmax = ifp->i->if_width-1;
	}
	if (clp->ypixmax > ifp->i->if_height-1) {
	    clp->ypixmax = ifp->i->if_height-1;
	}
    }

int
qtgl_configureWindow(struct fb *ifp, int width, int height)
{
    int getmem = 0;

    if (!QTGL(ifp)->mi_memwidth)
	getmem = 1;

    ifp->i->if_width = ifp->i->if_max_width = width;
    ifp->i->if_height = ifp->i->if_max_height = height;

    QTGL(ifp)->win_width = QTGL(ifp)->vp_width = width;
    QTGL(ifp)->win_height = QTGL(ifp)->vp_height = height;

    ifp->i->if_xzoom = 1;
    ifp->i->if_yzoom = 1;
    ifp->i->if_xcenter = width/2;
    ifp->i->if_ycenter = height/2;

    if (!getmem && width == QTGL(ifp)->win_width &&
	height == QTGL(ifp)->win_height)
	return 1;

    qtgl_getmem(ifp);
    fb_clipper(ifp);

    QTGL(ifp)->glc->makeCurrent();

    glBindTexture (GL_TEXTURE_2D, QTGL(ifp)->texid);
    glPixelStorei (GL_UNPACK_ALIGNMENT, 1);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    glTexImage2D (GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, ifp->i->if_mem);

    glDisable(GL_LIGHTING);

    glViewport(0, 0, QTGL(ifp)->win_width, QTGL(ifp)->win_height);
    glMatrixMode (GL_PROJECTION);
    glLoadIdentity ();
    glOrtho(0, QTGL(ifp)->win_width, QTGL(ifp)->win_height, 0, -1, 1);
    glMatrixMode (GL_MODELVIEW);

    glClear(GL_COLOR_BUFFER_BIT);

    glLoadIdentity();
    glColor3f(1,1,1);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, QTGL(ifp)->texid);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, QTGL(ifp)->win_width, QTGL(ifp)->win_height, GL_RGB, GL_UNSIGNED_BYTE, ifp->i->if_mem);
    glBegin(GL_TRIANGLE_STRIP);

    glTexCoord2d(0, 0); glVertex3f(0, 0, 0);
    glTexCoord2d(0, 1); glVertex3f(0, QTGL(ifp)->win_height, 0);
    glTexCoord2d(1, 0); glVertex3f(QTGL(ifp)->win_width, 0, 0);
    glTexCoord2d(1, 1); glVertex3f(QTGL(ifp)->win_width, QTGL(ifp)->win_height, 0);

    glEnd();


    return 0;
}


HIDDEN void
qtgl_do_event(struct fb *ifp)
{
    QTGL(ifp)->glc->update();
}

static int
qtgl_open_existing(struct fb *ifp, int width, int height, struct fb_platform_specific *fb_p)
{
    BU_CKMAG(fb_p, FB_QTGL_MAGIC, "qtgl framebuffer");

    // If this really is an existing ifp, we need to create this container - qtgl_open
    // may already have allocated it to store Qt window info.
    if (!ifp->i->pp) {
	if ((ifp->i->pp = (char *)calloc(1, sizeof(struct qtglinfo))) == NULL) {
	    fb_log("fb_qtgl:  qtglinfo malloc failed\n");
	    return -1;
	}
    }

    QTGL(ifp)->dmp = (struct dm *)fb_p->data;

    /* Allocate memory */
    if (qtgl_getmem(ifp) < 0)
	return -1;

    ifp->i->if_width = ifp->i->if_max_width = width;
    ifp->i->if_height = ifp->i->if_max_height = height;

    QTGL(ifp)->win_width = QTGL(ifp)->vp_width = width;
    QTGL(ifp)->win_height = QTGL(ifp)->vp_height = height;

    /* initialize window state variables before calling qtgl_getmem */
    ifp->i->if_xzoom = 1;	/* for zoom fakeout */
    ifp->i->if_yzoom = 1;	/* for zoom fakeout */
    ifp->i->if_xcenter = width/2;
    ifp->i->if_ycenter = height/2;

    /* Allocate memory, potentially with a screen repaint */
    if (qtgl_getmem(ifp) < 0)
	return -1;

    fb_clipper(ifp);

    qtgl_configureWindow(ifp, width, height);

    return 0;
}

static int
fb_qtgl_open(struct fb *ifp, const char *UNUSED(file), int width, int height)
{
    FB_CK_FB(ifp->i);

    if ((ifp->i->pp = (char *)calloc(1, sizeof(struct qtglinfo))) == NULL) {
	fb_log("fb_qtgl:  qtglinfo malloc failed\n");
	return -1;
    }

    ifp->i->stand_alone = 1;

    struct qtglinfo *qi = QTGL(ifp);
    qi->av = (char **)calloc(2, sizeof(char *));
    qi->ac = 1;
    qi->av[0] = bu_strdup("Frame buffer");
    qi->av[1] = NULL;
    FB_CK_FB(ifp->i);

    qi->win_width = qi->vp_width = width;
    qi->win_height = qi->vp_width = height;

    qi->qapp = new QApplication(qi->ac, qi->av);

    QSurfaceFormat fmt;
    fmt.setDepthBufferSize(24);
    QSurfaceFormat::setDefaultFormat(fmt);

    qi->mw = new QtGLWin(ifp);
    qi->mw->canvas->setFixedSize(width, height);
    qi->mw->adjustSize();
    qi->mw->setFixedSize(qi->mw->size());
    qi->mw->show();
    qi->glc = qi->mw->canvas;

    // Do the standard libdm attach to get our rendering backend.
    const char *acmd = "attach";
    struct dm *dmp = dm_open((void *)qi->mw->canvas, NULL, "qtgl", 1, &acmd);
    if (!dmp)
	return -1;
    BU_GET(qi->mw->canvas->v, struct bview);
    bv_init(qi->mw->canvas->v);

    struct fb_platform_specific fbps;
    fbps.magic = FB_QTGL_MAGIC;
    fbps.data = (void *)dmp;

    return qtgl_open_existing(ifp, width, height, &fbps);
}

HIDDEN struct fb_platform_specific *
qtgl_get_fbps(uint32_t magic)
{
    struct fb_platform_specific *fb_ps = NULL;
    struct qtgl_fb_info *data = NULL;
    BU_GET(fb_ps, struct fb_platform_specific);
    BU_GET(data, struct qtgl_fb_info);
    fb_ps->magic = magic;
    fb_ps->data = data;
    return fb_ps;
}


HIDDEN void
qtgl_put_fbps(struct fb_platform_specific *fbps)
{
    BU_CKMAG(fbps, FB_QTGL_MAGIC, "qtgl framebuffer");
    BU_PUT(fbps->data, struct qtgl_fb_info);
    BU_PUT(fbps, struct fb_platform_specific);
    return;
}


HIDDEN int
qtgl_flush(struct fb *UNUSED(ifp))
{
    glFlush();
    return 0;
}


HIDDEN int
fb_qtgl_close(struct fb *ifp)
{
    struct qtglinfo *qi = QTGL(ifp);

    /* if a window was created wait for user input and process events */
    if (qi->qapp) {
	return qi->qapp->exec();
	qt_destroy(qi);
    }

    return 0;
}

int
qtgl_close_existing(struct fb *ifp)
{
    struct qtglinfo *qi = QTGL(ifp);
    qi->glc = NULL;
    qi->alive = 0;
    return 0;
}

/*
 * Handle any pending input events
 */
HIDDEN int
qtgl_poll(struct fb *ifp)
{
    qtgl_do_event(ifp);

    if (QTGL(ifp)->alive)
	return 0;
    return 1;
}


/*
 * Free memory resources and close.
 */
HIDDEN int
qtgl_free(struct fb *ifp)
{
    if (FB_DEBUG)
	printf("entering qtgl_free\n");

    /* Close the framebuffer */
    if (FB_DEBUG)
	printf("qtgl_free: All done...goodbye!\n");

    if (ifp->i->if_mem != NULL) {
	/* free up memory associated with image */
	(void)free(ifp->i->if_mem);
    }

    if (QTGLL(ifp) != NULL) {
	(void)free((char *)QTGLL(ifp));
	QTGLL(ifp) = NULL;
    }

    return 0;
}


HIDDEN int
qtgl_clear(struct fb *ifp, unsigned char *pp)
{
    struct fb_pixel bg;
    struct fb_pixel *qtglp;
    int cnt;
    int y;

    if (FB_DEBUG)
	printf("entering qtgl_clear\n");

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

    /* Flood rectangle in memory */
    for (y = 0; y < ifp->i->if_height; y++) {
	qtglp = (struct fb_pixel *)&ifp->i->if_mem[(y*QTGL(ifp)->mi_memwidth+0)*sizeof(struct fb_pixel) ];
	for (cnt = ifp->i->if_width-1; cnt >= 0; cnt--) {
	    *qtglp++ = bg;	/* struct copy */
	}
    }

    QTGL(ifp)->glc->makeCurrent();

    if (pp != RGBPIXEL_NULL) {
	glClearColor(pp[RED]/255.0, pp[GRN]/255.0, pp[BLU]/255.0, 0.0);
    } else {
	glClearColor(0, 0, 0, 0);
    }

    glClear(GL_COLOR_BUFFER_BIT);

    return 0;
}


HIDDEN int
qtgl_view(struct fb *ifp, int xcenter, int ycenter, int xzoom, int yzoom)
{
    struct fb_clip *clp;

    if (FB_DEBUG)
	printf("entering qtgl_view\n");

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

    QTGL(ifp)->glc->makeCurrent();

    /* Set clipping matrix and zoom level */
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    fb_clipper(ifp);
    clp = &(QTGL(ifp)->clip);
    glOrtho(clp->oleft, clp->oright, clp->obottom, clp->otop, -1.0, 1.0);
    glPixelZoom((float) ifp->i->if_xzoom, (float) ifp->i->if_yzoom);

    glFlush();

    QTGL(ifp)->glc->update();

    return 0;
}


HIDDEN int
qtgl_getview(struct fb *ifp, int *xcenter, int *ycenter, int *xzoom, int *yzoom)
{
    if (FB_DEBUG)
	printf("entering qtgl_getview\n");

    *xcenter = ifp->i->if_xcenter;
    *ycenter = ifp->i->if_ycenter;
    *xzoom = ifp->i->if_xzoom;
    *yzoom = ifp->i->if_yzoom;

    return 0;
}


/* read count pixels into pixelp starting at x, y */
HIDDEN ssize_t
qtgl_read(struct fb *ifp, int x, int y, unsigned char *pixelp, size_t count)
{
    size_t n;
    size_t scan_count;	/* # pix on this scanline */
    unsigned char *cp;
    ssize_t ret;
    struct fb_pixel *qtglp;

    if (FB_DEBUG)
	printf("entering qtgl_read\n");

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

	qtglp = (struct fb_pixel *)&ifp->i->if_mem[(y*QTGL(ifp)->mi_memwidth+x)*sizeof(struct fb_pixel) ];

	n = scan_count;
	while (n) {
	    cp[RED] = qtglp->red;
	    cp[GRN] = qtglp->green;
	    cp[BLU] = qtglp->blue;
	    qtglp++;
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
qtgl_write(struct fb *ifp, int xstart, int ystart, const unsigned char *pixelp, size_t count)
{
    int x;
    int y;
    size_t scan_count;  /* # pix on this scanline */
    size_t pix_count;   /* # pixels to send */
    ssize_t ret;

    FB_CK_FB(ifp->i);

    if (FB_DEBUG)
	printf("entering qtgl_write\n");

    /* fast exit cases */
    pix_count = count;
    if (pix_count == 0)
	return 0;	/* OK, no pixels transferred */

    x = xstart;
    y = ystart;

    if (x < 0 || x >= ifp->i->if_width ||
	    y < 0 || y >= ifp->i->if_height)
	return -1;

    ret = 0;

    unsigned char *cp;
    //int ybase;

    //ybase = ystart;
    cp = (unsigned char *)(pixelp);

    while (pix_count) {
	size_t n;
	struct fb_pixel *qtglp;

	if (y >= ifp->i->if_height)
	    break;

	if (pix_count >= (size_t)(ifp->i->if_width-x))
	    scan_count = (size_t)(ifp->i->if_width-x);
	else
	    scan_count = pix_count;

	qtglp = (struct fb_pixel *)&ifp->i->if_mem[(y*QTGL(ifp)->mi_memwidth+x)*sizeof(struct fb_pixel) ];

	n = scan_count;
	if ((n & 3) != 0) {
	    /* This code uses 60% of all CPU time */
	    while (n) {
		/* alpha channel is always zero */
		qtglp->red   = cp[RED];
		qtglp->green = cp[GRN];
		qtglp->blue  = cp[BLU];
		qtglp++;
		cp += 3;
		n--;
	    }
	} else {
	    while (n) {
		/* alpha channel is always zero */
		qtglp[0].red   = cp[RED+0*3];
		qtglp[0].green = cp[GRN+0*3];
		qtglp[0].blue  = cp[BLU+0*3];
		qtglp[1].red   = cp[RED+1*3];
		qtglp[1].green = cp[GRN+1*3];
		qtglp[1].blue  = cp[BLU+1*3];
		qtglp[2].red   = cp[RED+2*3];
		qtglp[2].green = cp[GRN+2*3];
		qtglp[2].blue  = cp[BLU+2*3];
		qtglp[3].red   = cp[RED+3*3];
		qtglp[3].green = cp[GRN+3*3];
		qtglp[3].blue  = cp[BLU+3*3];
		qtglp += 4;
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

    QTGL(ifp)->glc->update();

    return ret;
}


/*
 * The task of this routine is to reformat the pixels into WIN
 * internal form, and then arrange to have them sent to the screen
 * separately.
 */
HIDDEN int
qtgl_writerect(struct fb *ifp, int xmin, int ymin, int width, int height, const unsigned char *pp)
{
    int x;
    int y;
    unsigned char *cp;
    struct fb_pixel *qtglp;

    if (FB_DEBUG)
	printf("entering qtgl_writerect\n");

    if (width <= 0 || height <= 0)
	return 0;  /* do nothing */
    if (xmin < 0 || xmin+width > ifp->i->if_width ||
	ymin < 0 || ymin+height > ifp->i->if_height)
	return -1; /* no can do */

    cp = (unsigned char *)(pp);
    for (y = ymin; y < ymin+height; y++) {
	qtglp = (struct fb_pixel *)&ifp->i->if_mem[(y*QTGL(ifp)->mi_memwidth+xmin)*sizeof(struct fb_pixel) ];
	for (x = xmin; x < xmin+width; x++) {
	    /* alpha channel is always zero */
	    qtglp->red   = cp[RED];
	    qtglp->green = cp[GRN];
	    qtglp->blue  = cp[BLU];
	    qtglp++;
	    cp += 3;
	}
    }

    QTGL(ifp)->glc->update();

    return width*height;
}


/*
 * The task of this routine is to reformat the pixels into WIN
 * internal form, and then arrange to have them sent to the screen
 * separately.
 */
HIDDEN int
qtgl_bwwriterect(struct fb *ifp, int xmin, int ymin, int width, int height, const unsigned char *pp)
{
    int x;
    int y;
    unsigned char *cp;
    struct fb_pixel *qtglp;

    if (FB_DEBUG)
	printf("entering qtgl_bwwriterect\n");

    if (width <= 0 || height <= 0)
	return 0;  /* do nothing */
    if (xmin < 0 || xmin+width > ifp->i->if_width ||
	ymin < 0 || ymin+height > ifp->i->if_height)
	return -1; /* no can do */

    cp = (unsigned char *)(pp);
    for (y = ymin; y < ymin+height; y++) {
	qtglp = (struct fb_pixel *)&ifp->i->if_mem[(y*QTGL(ifp)->mi_memwidth+xmin)*sizeof(struct fb_pixel) ];
	for (x = xmin; x < xmin+width; x++) {
	    int val;
	    /* alpha channel is always zero */
	    qtglp->red   = (val = *cp++);
	    qtglp->green = val;
	    qtglp->blue  = val;
	    qtglp++;
	}
    }

    QTGL(ifp)->glc->update();

    return width*height;
}


HIDDEN int
qtgl_rmap(struct fb *UNUSED(ifp), ColorMap *UNUSED(cmp))
{
    if (FB_DEBUG)
	printf("entering qtgl_rmap\n");
#if 0
    /* Just parrot back the stored colormap */
    for (i = 0; i < 256; i++) {
	cmp->cm_red[i]   = CMR(ifp)[i]<<8;
	cmp->cm_green[i] = CMG(ifp)[i]<<8;
	cmp->cm_blue[i]  = CMB(ifp)[i]<<8;
    }
#endif
    return 0;
}


HIDDEN int
qtgl_wmap(struct fb *UNUSED(ifp), const ColorMap *UNUSED(cmp))
{
    if (FB_DEBUG)
	printf("entering qtgl_wmap\n");

    return 0;
}


HIDDEN int
qtgl_help(struct fb *ifp)
{
    fb_log("Description: %s\n", ifp->i->if_type);
    fb_log("Device: %s\n", ifp->i->if_name);
    fb_log("Max width height: %d %d\n",
	   ifp->i->if_max_width,
	   ifp->i->if_max_height);
    fb_log("Default width height: %d %d\n",
	   ifp->i->if_width,
	   ifp->i->if_height);
    fb_log("Usage: /dev/qtgl\n");

    return 0;
}


HIDDEN int
qtgl_setcursor(struct fb *ifp, const unsigned char *UNUSED(bits), int UNUSED(xbits), int UNUSED(ybits), int UNUSED(xorig), int UNUSED(yorig))
{
    FB_CK_FB(ifp->i);

    // If it should ever prove desirable to alter the cursor or disable it, here's how it is done:
    // dynamic_cast<osgViewer::GraphicsWindow*>(camera->getGraphicsContext()))->setCursor(osgViewer::GraphicsWindow::NoCursor);

    return 0;
}


HIDDEN int
qtgl_cursor(struct fb *UNUSED(ifp), int UNUSED(mode), int UNUSED(x), int UNUSED(y))
{

    fb_log("qtgl_cursor\n");
    return 0;
}


int
qtgl_refresh(struct fb *ifp, int x, int y, int w, int h)
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
    clp = &(QTGL(ifp)->clip);
    glOrtho(clp->oleft, clp->oright, clp->obottom, clp->otop, -1.0, 1.0);
    glPixelZoom((float) ifp->i->if_xzoom, (float) ifp->i->if_yzoom);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    glViewport(0, 0, QTGL(ifp)->win_width, QTGL(ifp)->win_height);

    glBindTexture (GL_TEXTURE_2D, QTGL(ifp)->texid);
    glPixelStorei (GL_UNPACK_ALIGNMENT, 1);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    glTexImage2D (GL_TEXTURE_2D, 0, GL_RGB, QTGL(ifp)->win_width, QTGL(ifp)->win_height, 0, GL_RGB, GL_UNSIGNED_BYTE, ifp->i->if_mem);

    glDisable(GL_LIGHTING);

    glViewport(0, 0, QTGL(ifp)->win_width, QTGL(ifp)->win_height);

    glMatrixMode (GL_PROJECTION);
    glLoadIdentity ();
    glOrtho(0, w, h, 0, -1, 1);
    glPopMatrix();

    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
    glMatrixMode(mm);

    QTGL(ifp)->glc->update();

    return 0;
}


/* This is the ONLY thing that we normally "export" */
struct fb_impl qtgl_interface_impl =
{
    0,			/* magic number slot */
    FB_QTGL_MAGIC,
    fb_qtgl_open,	/* open device */
    qtgl_open_existing,    /* existing device_open */
    qtgl_close_existing,    /* existing device_close */
    qtgl_get_fbps,         /* get platform specific memory */
    qtgl_put_fbps,         /* free platform specific memory */
    fb_qtgl_close,	/* close device */
    qtgl_clear,		/* clear device */
    qtgl_read,		/* read pixels */
    qtgl_write,		/* write pixels */
    qtgl_rmap,		/* read colormap */
    qtgl_wmap,		/* write colormap */
    qtgl_view,		/* set view */
    qtgl_getview,	/* get view */
    qtgl_setcursor,	/* define cursor */
    qtgl_cursor,		/* set cursor */
    fb_sim_getcursor,	/* get cursor */
    fb_sim_readrect,	/* read rectangle */
    qtgl_writerect,	/* write rectangle */
    fb_sim_bwreadrect,
    qtgl_bwwriterect,	/* write rectangle */
    qtgl_configureWindow,
    qtgl_refresh,
    qtgl_poll,		/* process events */
    qtgl_flush,		/* flush output */
    qtgl_free,		/* free resources */
    qtgl_help,		/* help message */
    bu_strdup("OpenSceneGraph OpenGL"),	/* device description */
    FB_XMAXSCREEN,		/* max width */
    FB_YMAXSCREEN,		/* max height */
    bu_strdup("/dev/qtgl"),		/* short device name */
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
    NULL,
    NULL,
    0,
    {0}, /* u1 */
    {0}, /* u2 */
    {0}, /* u3 */
    {0}, /* u4 */
    {0}, /* u5 */
    {0}  /* u6 */
};

extern "C" {
struct fb qtgl_interface = { &qtgl_interface_impl };

#ifdef DM_PLUGIN
static const struct fb_plugin finfo = { &qtgl_interface };

extern "C" {
COMPILER_DLLEXPORT const struct fb_plugin *fb_plugin_info()
{
    return &finfo;
}
}
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
