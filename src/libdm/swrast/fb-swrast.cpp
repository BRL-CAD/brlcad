/*                   I F _ S W R A S T . C P P
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
/** @file fb-swrast.cpp
 *
 * A Software Rasterizer based OpenGL framebuffer.
 *
 * TODO - study https://stackoverflow.com/a/23230099
 *
 */
/** @} */


#include "common.h"

#include "bu/app.h"

#include "./fb-swrast.h"
extern "C" {
#include "../include/private.h"
}

extern "C" {
extern struct fb swrast_interface;
}

struct swrastinfo {
    //QOpenGLWidget *glc = NULL;
    struct fb_clip clip;        /* current view clipping */

    int win_width;              /* actual window width */
    int win_height;             /* actual window height */
    int vp_width;               /* actual viewport width */
    int vp_height;              /* actual viewport height */

    int mi_memwidth;            /* width of scanline in if_mem */

    int texid;

    int alive;
};

#define if_mem u2.p		/* shared memory pointer */
#define QTGL(ptr) ((struct swrastinfo *)((ptr)->i->u6.p))
#define QTGLL(ptr) ((ptr)->i->u6.p)     /* left hand side version */


/* If the parent application isn't supplying the OpenGL
 * Qt widget, we need to provide a window ourselves and
 * set things up. */
static int
qt_setup(struct fb *ifp, int width, int height)
{
    struct swrastinfo *qi = QTGL(ifp);
    FB_CK_FB(ifp->i);

    qi->win_width = qi->vp_width = width;
    qi->win_height = qi->vp_width = height;

    //qi->glc = new QOpenGLWidget();
    //qi->glc->resize(width, height);
    //qi->glc->show();

    return 0;
}

static void
qt_destroy(struct swrastinfo *UNUSED(qi))
{
}


HIDDEN int
swrast_getmem(struct fb *ifp)
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
	    fb_log("swrast_getmem: frame buffer memory malloc failed\n");
	    goto fail;
	}
	goto success;
    }

success:
    ifp->i->if_mem = sp;

    return 0;
fail:
    if ((sp = (char *)calloc(1, size)) == NULL) {
	fb_log("swrast_getmem:  malloc failure\n");
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
swrast_configureWindow(struct fb *ifp, int width, int height)
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

    swrast_getmem(ifp);
    fb_clipper(ifp);

    //QTGL(ifp)->glc->makeCurrent();

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
swrast_do_event(struct fb *UNUSED(ifp))
{
    //QTGL(ifp)->glc->update();
}

HIDDEN int
fb_swrast_open(struct fb *ifp, const char *UNUSED(file), int width, int height)
{
    FB_CK_FB(ifp->i);

    if ((ifp->i->u6.p = (char *)calloc(1, sizeof(struct swrastinfo))) == NULL) {
	fb_log("fb_swrast_open:  swrastinfo malloc failed\n");
	return -1;
    }

    /* use defaults if invalid width and height specified */
    if (width > 0)
	ifp->i->if_width = width;
    if (height > 0)
	ifp->i->if_height = height;

    /* use max values if width and height are greater */
    if (width > ifp->i->if_max_width)
	ifp->i->if_width = ifp->i->if_max_width;
    if (height > ifp->i->if_max_height)
	ifp->i->if_height = ifp->i->if_max_height;

    /* initialize window state variables before calling swrast_getmem */
    ifp->i->if_xzoom = 1;	/* for zoom fakeout */
    ifp->i->if_yzoom = 1;	/* for zoom fakeout */
    ifp->i->if_xcenter = width/2;
    ifp->i->if_ycenter = height/2;

    /* Allocate memory, potentially with a screen repaint */
    if (swrast_getmem(ifp) < 0)
	return -1;

    struct swrastinfo *qi = new struct swrastinfo;
    if (qi == NULL) {
	fb_log("qt_open: swrastinfo malloc failed\n");
	return -1;
    }
    QTGLL(ifp) = (char *)qi;

    /* Set up an Qt window */
    if (qt_setup(ifp, width, height) < 0) {
	qt_destroy(qi);
	return -1;
    }

    swrast_configureWindow(ifp, width, height);

    return 0;

}

#if 0

int
_swrast_open_existing(struct fb *ifp, int width, int height, void *glc, void *traits)
{
    /*
     * Allocate extension memory sections,
     * addressed by WIN(ifp)->mi_xxx and QTGL(ifp)->xxx
     */

    if ((WINL(ifp) = (char *)calloc(1, sizeof(struct wininfo))) == NULL) {
	fb_log("fb_swrast_open:  wininfo malloc failed\n");
	return -1;
    }
    if ((QTGLL(ifp) = (char *)calloc(1, sizeof(struct swrastinfo))) == NULL) {
	fb_log("fb_swrast_open:  swrastinfo malloc failed\n");
	return -1;
    }

    ifp->i->if_width = ifp->i->if_max_width = width;
    ifp->i->if_height = ifp->i->if_max_height = height;

    QTGL(ifp)->win_width = QTGL(ifp)->vp_width = width;
    QTGL(ifp)->win_height = QTGL(ifp)->vp_height = height;

    QTGL(ifp)->cursor_on = 1;

    /* initialize window state variables before calling swrast_getmem */
    ifp->i->if_xzoom = 1;	/* for zoom fakeout */
    ifp->i->if_yzoom = 1;	/* for zoom fakeout */
    ifp->i->if_xcenter = width/2;
    ifp->i->if_ycenter = height/2;

    /* Allocate memory, potentially with a screen repaint */
    if (swrast_getmem(ifp) < 0)
	return -1;


    ++swrast_nwindows;

    QTGL(ifp)->alive = 1;
    QTGL(ifp)->firstTime = 1;

    fb_clipper(ifp);

    return 0;
}

#endif

HIDDEN struct fb_platform_specific *
swrast_get_fbps(uint32_t magic)
{
    struct fb_platform_specific *fb_ps = NULL;
    struct swrast_fb_info *data = NULL;
    BU_GET(fb_ps, struct fb_platform_specific);
    BU_GET(data, struct swrast_fb_info);
    fb_ps->magic = magic;
    fb_ps->data = data;
    return fb_ps;
}


HIDDEN void
swrast_put_fbps(struct fb_platform_specific *fbps)
{
    BU_CKMAG(fbps, FB_QTGL_MAGIC, "swrast framebuffer");
    BU_PUT(fbps->data, struct swrast_fb_info);
    BU_PUT(fbps, struct fb_platform_specific);
    return;
}

HIDDEN int
swrast_open_existing(struct fb *UNUSED(ifp), int UNUSED(width), int UNUSED(height), struct fb_platform_specific *fb_p)
{
    BU_CKMAG(fb_p, FB_QTGL_MAGIC, "swrast framebuffer");
    //return _swrast_open_existing(ifp, width, height, swrast_internal->glc, swrast_internal->traits);
    return 0;
}


HIDDEN int
swrast_flush(struct fb *UNUSED(ifp))
{
    glFlush();
    return 0;
}


HIDDEN int
fb_swrast_close(struct fb *UNUSED(ifp))
{
    //struct swrastinfo *qi = QTGL(ifp);


    return 0;
}

int
swrast_close_existing(struct fb *ifp)
{
    struct swrastinfo *qi = QTGL(ifp);
    //qi->glc = NULL;
    qi->alive = 0;
    return 0;
}

/*
 * Handle any pending input events
 */
HIDDEN int
swrast_poll(struct fb *ifp)
{
    swrast_do_event(ifp);

    if (QTGL(ifp)->alive)
	return 0;
    return 1;
}


/*
 * Free memory resources and close.
 */
HIDDEN int
swrast_free(struct fb *ifp)
{
    if (FB_DEBUG)
	printf("entering swrast_free\n");

    /* Close the framebuffer */
    if (FB_DEBUG)
	printf("swrast_free: All done...goodbye!\n");

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
swrast_clear(struct fb *ifp, unsigned char *pp)
{
    struct fb_pixel bg;
    struct fb_pixel *swrastp;
    int cnt;
    int y;

    if (FB_DEBUG)
	printf("entering swrast_clear\n");

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
	swrastp = (struct fb_pixel *)&ifp->i->if_mem[(y*QTGL(ifp)->mi_memwidth+0)*sizeof(struct fb_pixel) ];
	for (cnt = ifp->i->if_width-1; cnt >= 0; cnt--) {
	    *swrastp++ = bg;	/* struct copy */
	}
    }

    //QTGL(ifp)->glc->makeCurrent();

    if (pp != RGBPIXEL_NULL) {
	glClearColor(pp[RED]/255.0, pp[GRN]/255.0, pp[BLU]/255.0, 0.0);
    } else {
	glClearColor(0, 0, 0, 0);
    }

    glClear(GL_COLOR_BUFFER_BIT);

    return 0;
}


HIDDEN int
swrast_view(struct fb *ifp, int xcenter, int ycenter, int xzoom, int yzoom)
{
    struct fb_clip *clp;

    if (FB_DEBUG)
	printf("entering swrast_view\n");

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

    //QTGL(ifp)->glc->makeCurrent();

    /* Set clipping matrix and zoom level */
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    fb_clipper(ifp);
    clp = &(QTGL(ifp)->clip);
    glOrtho(clp->oleft, clp->oright, clp->obottom, clp->otop, -1.0, 1.0);
    glPixelZoom((float) ifp->i->if_xzoom, (float) ifp->i->if_yzoom);

    glFlush();

    //QTGL(ifp)->glc->update();

    return 0;
}


HIDDEN int
swrast_getview(struct fb *ifp, int *xcenter, int *ycenter, int *xzoom, int *yzoom)
{
    if (FB_DEBUG)
	printf("entering swrast_getview\n");

    *xcenter = ifp->i->if_xcenter;
    *ycenter = ifp->i->if_ycenter;
    *xzoom = ifp->i->if_xzoom;
    *yzoom = ifp->i->if_yzoom;

    return 0;
}


/* read count pixels into pixelp starting at x, y */
HIDDEN ssize_t
swrast_read(struct fb *ifp, int x, int y, unsigned char *pixelp, size_t count)
{
    size_t n;
    size_t scan_count;	/* # pix on this scanline */
    unsigned char *cp;
    ssize_t ret;
    struct fb_pixel *swrastp;

    if (FB_DEBUG)
	printf("entering swrast_read\n");

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

	swrastp = (struct fb_pixel *)&ifp->i->if_mem[(y*QTGL(ifp)->mi_memwidth+x)*sizeof(struct fb_pixel) ];

	n = scan_count;
	while (n) {
	    cp[RED] = swrastp->red;
	    cp[GRN] = swrastp->green;
	    cp[BLU] = swrastp->blue;
	    swrastp++;
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
swrast_write(struct fb *ifp, int xstart, int ystart, const unsigned char *pixelp, size_t count)
{
    int x;
    int y;
    size_t scan_count;  /* # pix on this scanline */
    size_t pix_count;   /* # pixels to send */
    ssize_t ret;

    FB_CK_FB(ifp->i);

    if (FB_DEBUG)
	printf("entering swrast_write\n");

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
	struct fb_pixel *swrastp;

	if (y >= ifp->i->if_height)
	    break;

	if (pix_count >= (size_t)(ifp->i->if_width-x))
	    scan_count = (size_t)(ifp->i->if_width-x);
	else
	    scan_count = pix_count;

	swrastp = (struct fb_pixel *)&ifp->i->if_mem[(y*QTGL(ifp)->mi_memwidth+x)*sizeof(struct fb_pixel) ];

	n = scan_count;
	if ((n & 3) != 0) {
	    /* This code uses 60% of all CPU time */
	    while (n) {
		/* alpha channel is always zero */
		swrastp->red   = cp[RED];
		swrastp->green = cp[GRN];
		swrastp->blue  = cp[BLU];
		swrastp++;
		cp += 3;
		n--;
	    }
	} else {
	    while (n) {
		/* alpha channel is always zero */
		swrastp[0].red   = cp[RED+0*3];
		swrastp[0].green = cp[GRN+0*3];
		swrastp[0].blue  = cp[BLU+0*3];
		swrastp[1].red   = cp[RED+1*3];
		swrastp[1].green = cp[GRN+1*3];
		swrastp[1].blue  = cp[BLU+1*3];
		swrastp[2].red   = cp[RED+2*3];
		swrastp[2].green = cp[GRN+2*3];
		swrastp[2].blue  = cp[BLU+2*3];
		swrastp[3].red   = cp[RED+3*3];
		swrastp[3].green = cp[GRN+3*3];
		swrastp[3].blue  = cp[BLU+3*3];
		swrastp += 4;
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

    //QTGL(ifp)->glc->update();

    return ret;
}


/*
 * The task of this routine is to reformat the pixels into WIN
 * internal form, and then arrange to have them sent to the screen
 * separately.
 */
HIDDEN int
swrast_writerect(struct fb *ifp, int xmin, int ymin, int width, int height, const unsigned char *pp)
{
    int x;
    int y;
    unsigned char *cp;
    struct fb_pixel *swrastp;

    if (FB_DEBUG)
	printf("entering swrast_writerect\n");

    if (width <= 0 || height <= 0)
	return 0;  /* do nothing */
    if (xmin < 0 || xmin+width > ifp->i->if_width ||
	ymin < 0 || ymin+height > ifp->i->if_height)
	return -1; /* no can do */

    cp = (unsigned char *)(pp);
    for (y = ymin; y < ymin+height; y++) {
	swrastp = (struct fb_pixel *)&ifp->i->if_mem[(y*QTGL(ifp)->mi_memwidth+xmin)*sizeof(struct fb_pixel) ];
	for (x = xmin; x < xmin+width; x++) {
	    /* alpha channel is always zero */
	    swrastp->red   = cp[RED];
	    swrastp->green = cp[GRN];
	    swrastp->blue  = cp[BLU];
	    swrastp++;
	    cp += 3;
	}
    }

    //QTGL(ifp)->glc->update();

    return width*height;
}


/*
 * The task of this routine is to reformat the pixels into WIN
 * internal form, and then arrange to have them sent to the screen
 * separately.
 */
HIDDEN int
swrast_bwwriterect(struct fb *ifp, int xmin, int ymin, int width, int height, const unsigned char *pp)
{
    int x;
    int y;
    unsigned char *cp;
    struct fb_pixel *swrastp;

    if (FB_DEBUG)
	printf("entering swrast_bwwriterect\n");

    if (width <= 0 || height <= 0)
	return 0;  /* do nothing */
    if (xmin < 0 || xmin+width > ifp->i->if_width ||
	ymin < 0 || ymin+height > ifp->i->if_height)
	return -1; /* no can do */

    cp = (unsigned char *)(pp);
    for (y = ymin; y < ymin+height; y++) {
	swrastp = (struct fb_pixel *)&ifp->i->if_mem[(y*QTGL(ifp)->mi_memwidth+xmin)*sizeof(struct fb_pixel) ];
	for (x = xmin; x < xmin+width; x++) {
	    int val;
	    /* alpha channel is always zero */
	    swrastp->red   = (val = *cp++);
	    swrastp->green = val;
	    swrastp->blue  = val;
	    swrastp++;
	}
    }

    //QTGL(ifp)->glc->update();

    return width*height;
}


HIDDEN int
swrast_rmap(struct fb *UNUSED(ifp), ColorMap *UNUSED(cmp))
{
    if (FB_DEBUG)
	printf("entering swrast_rmap\n");
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
swrast_wmap(struct fb *UNUSED(ifp), const ColorMap *UNUSED(cmp))
{
    if (FB_DEBUG)
	printf("entering swrast_wmap\n");

    return 0;
}


HIDDEN int
swrast_help(struct fb *ifp)
{
    fb_log("Description: %s\n", ifp->i->if_type);
    fb_log("Device: %s\n", ifp->i->if_name);
    fb_log("Max width height: %d %d\n",
	   ifp->i->if_max_width,
	   ifp->i->if_max_height);
    fb_log("Default width height: %d %d\n",
	   ifp->i->if_width,
	   ifp->i->if_height);
    fb_log("Usage: /dev/swrast\n");

    return 0;
}


HIDDEN int
swrast_setcursor(struct fb *ifp, const unsigned char *UNUSED(bits), int UNUSED(xbits), int UNUSED(ybits), int UNUSED(xorig), int UNUSED(yorig))
{
    FB_CK_FB(ifp->i);

    // If it should ever prove desirable to alter the cursor or disable it, here's how it is done:
    // dynamic_cast<osgViewer::GraphicsWindow*>(camera->getGraphicsContext()))->setCursor(osgViewer::GraphicsWindow::NoCursor);

    return 0;
}


HIDDEN int
swrast_cursor(struct fb *UNUSED(ifp), int UNUSED(mode), int UNUSED(x), int UNUSED(y))
{

    fb_log("swrast_cursor\n");
    return 0;
}


int
swrast_refresh(struct fb *ifp, int x, int y, int w, int h)
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

    //QTGL(ifp)->glc->update();

    return 0;
}


/* This is the ONLY thing that we normally "export" */
struct fb_impl swrast_interface_impl =
{
    0,			/* magic number slot */
    FB_QTGL_MAGIC,
    fb_swrast_open,	/* open device */
    swrast_open_existing,    /* existing device_open */
    swrast_close_existing,    /* existing device_close */
    swrast_get_fbps,         /* get platform specific memory */
    swrast_put_fbps,         /* free platform specific memory */
    fb_swrast_close,	/* close device */
    swrast_clear,		/* clear device */
    swrast_read,		/* read pixels */
    swrast_write,		/* write pixels */
    swrast_rmap,		/* read colormap */
    swrast_wmap,		/* write colormap */
    swrast_view,		/* set view */
    swrast_getview,	/* get view */
    swrast_setcursor,	/* define cursor */
    swrast_cursor,		/* set cursor */
    fb_sim_getcursor,	/* get cursor */
    fb_sim_readrect,	/* read rectangle */
    swrast_writerect,	/* write rectangle */
    fb_sim_bwreadrect,
    swrast_bwwriterect,	/* write rectangle */
    swrast_configureWindow,
    swrast_refresh,
    swrast_poll,		/* process events */
    swrast_flush,		/* flush output */
    swrast_free,		/* free resources */
    swrast_help,		/* help message */
    bu_strdup("OpenSceneGraph OpenGL"),	/* device description */
    FB_XMAXSCREEN,		/* max width */
    FB_YMAXSCREEN,		/* max height */
    bu_strdup("/dev/swrast"),		/* short device name */
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
struct fb swrast_interface = { &swrast_interface_impl };

#ifdef DM_PLUGIN
static const struct fb_plugin finfo = { &swrast_interface };

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
