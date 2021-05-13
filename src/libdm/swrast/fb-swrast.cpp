/*                   F B - S W R A S T . C P P
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
 * An OpenGL framebuffer using OSMesa software rasterization.
 */
/** @} */


#include "common.h"

#include "bu/app.h"

#define USE_MGL_NAMESPACE 1
#include "OSMesa/gl.h"
#include "OSMesa/osmesa.h"

extern "C" {
#include "../include/private.h"
}

extern "C" {
extern struct fb swrast_interface;
}

// NOTE - Qt is used here because we need a dm window to display the fb in
// stand-alone mode.  There should be nothing specific to Qt in most of the
// core swrast logic, and we should always be able to replace the Qt dm here
// with any other dm backend to achieve the same results.
#include <QApplication>
#include "swrastwin.h"

struct swrastinfo {
    int ac;
    char **av;
    QApplication *qapp = NULL;
    QtSWWin *mw = NULL;

    struct dm *dmp = NULL;

    int cmap_size;		/* hardware colormap size */
    int win_width;              /* actual window width */
    int win_height;             /* actual window height */
    int vp_width;               /* actual viewport width */
    int vp_height;              /* actual viewport height */
    struct fb_clip clip;        /* current view clipping */
    short mi_cmap_flag;		/* enabled when there is a non-linear map in memory */

    int mi_memwidth;            /* width of scanline in if_mem */

    int alive;
};

#define SWRAST(ptr) ((struct swrastinfo *)((ptr)->i->pp))
#define SWRASTL(ptr) ((ptr)->i->pp)     /* left hand side version */
#define if_cmap u3.p		/* color map memory */
#define CMR(x) ((struct fb_cmap *)((x)->i->if_cmap))->cmr
#define CMG(x) ((struct fb_cmap *)((x)->i->if_cmap))->cmg
#define CMB(x) ((struct fb_cmap *)((x)->i->if_cmap))->cmb


static void
swrast_xmit_scanlines(struct fb *ifp, int ybase, int nlines, int xbase, int npix)
{
    int y;
    int n;
    struct fb_clip *clp = &(SWRAST(ifp)->clip);

    int sw_cmap;	/* !0 => needs software color map */
    if (SWRAST(ifp)->mi_cmap_flag) {
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

    if (sw_cmap) {
	/* Software colormap each line as it's transmitted */
	int x;
	struct fb_pixel *swrastp;
	struct fb_pixel *scanline;

	y = ybase;

	if (FB_DEBUG)
	    printf("Doing sw colormap xmit\n");

	/* Perform software color mapping into temp scanline */
	scanline = (struct fb_pixel *)calloc(ifp->i->if_width, sizeof(struct fb_pixel));
	if (scanline == NULL) {
	    fb_log("swrast_getmem: scanline memory malloc failed\n");
	    return;
	}

	for (n=nlines; n>0; n--, y++) {
	    swrastp = (struct fb_pixel *)&ifp->i->if_mem[(y*SWRAST(ifp)->mi_memwidth) * sizeof(struct fb_pixel)];
	    for (x=xbase+npix-1; x>=xbase; x--) {
		scanline[x].red   = CMR(ifp)[swrastp[x].red];
		scanline[x].green = CMG(ifp)[swrastp[x].green];
		scanline[x].blue  = CMB(ifp)[swrastp[x].blue];
	    }

	    glPixelStorei(GL_UNPACK_SKIP_PIXELS, xbase);
	    glRasterPos2i(xbase, y);
	    glDrawPixels(npix, 1, GL_BGRA_EXT, GL_UNSIGNED_BYTE, (const GLvoid *)scanline);
	}

	(void)free((void *)scanline);

    } else {
	/* No need for software colormapping */
	glPixelStorei(GL_UNPACK_ROW_LENGTH, SWRAST(ifp)->mi_memwidth);
	glPixelStorei(GL_UNPACK_SKIP_PIXELS, xbase);
	glPixelStorei(GL_UNPACK_SKIP_ROWS, ybase);

	glRasterPos2i(xbase, ybase);
	glDrawPixels(npix, nlines, GL_BGRA_EXT, GL_UNSIGNED_BYTE, (const GLvoid *) ifp->i->if_mem);
    }
}




static void
qt_destroy(struct swrastinfo *qi)
{
    delete qi->mw;
    delete qi->qapp;
    free(qi->av[0]);
    free(qi->av);
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
	SWRAST(ifp)->mi_memwidth = ifp->i->if_width;
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

    clp = &(SWRAST(ifp)->clip);

    i = SWRAST(ifp)->vp_width/(2*ifp->i->if_xzoom);
    clp->xscrmin = ifp->i->if_xcenter - i;
    i = SWRAST(ifp)->vp_width/ifp->i->if_xzoom;
    clp->xscrmax = clp->xscrmin + i;
    pixels = (double) i;
    clp->oleft = ((double) clp->xscrmin) - 0.25*pixels/((double) SWRAST(ifp)->vp_width);
    clp->oright = clp->oleft + pixels;

    i = SWRAST(ifp)->vp_height/(2*ifp->i->if_yzoom);
    clp->yscrmin = ifp->i->if_ycenter - i;
    i = SWRAST(ifp)->vp_height/ifp->i->if_yzoom;
    clp->yscrmax = clp->yscrmin + i;
    pixels = (double) i;
    clp->obottom = ((double) clp->yscrmin) - 0.25*pixels/((double) SWRAST(ifp)->vp_height);
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

    if (!SWRAST(ifp)->mi_memwidth)
	getmem = 1;

    ifp->i->if_width = ifp->i->if_max_width = width;
    ifp->i->if_height = ifp->i->if_max_height = height;

    SWRAST(ifp)->win_width = SWRAST(ifp)->vp_width = width;
    SWRAST(ifp)->win_height = SWRAST(ifp)->vp_height = height;

    ifp->i->if_xzoom = 1;
    ifp->i->if_yzoom = 1;
    ifp->i->if_xcenter = width/2;
    ifp->i->if_ycenter = height/2;

    if (!getmem && width == SWRAST(ifp)->win_width &&
	height == SWRAST(ifp)->win_height)
	return 1;

    swrast_getmem(ifp);
    fb_clipper(ifp);

    dm_make_current(SWRAST(ifp)->dmp);

    return 0;
}


HIDDEN void
swrast_do_event(struct fb *ifp)
{
    SWRAST(ifp)->mw->update();
}

static int
swrast_open_existing(struct fb *ifp, int width, int height, struct fb_platform_specific *fb_p)
{
    BU_CKMAG(fb_p, FB_SWFB_MAGIC, "swrast framebuffer");

    // If this really is an existing ifp, may not need to create this container
    // - swrast_open may already have allocated it to store Qt window info.
    if (!ifp->i->pp) {
	if ((ifp->i->pp = (char *)calloc(1, sizeof(struct swrastinfo))) == NULL) {
	    fb_log("fb_swrast:  swrastinfo malloc failed\n");
	    return -1;
	}
    }

    SWRAST(ifp)->dmp = (struct dm *)fb_p->data;
    SWRAST(ifp)->dmp->i->fbp = ifp;

    ifp->i->if_width = ifp->i->if_max_width = width;
    ifp->i->if_height = ifp->i->if_max_height = height;

    SWRAST(ifp)->win_width = SWRAST(ifp)->vp_width = width;
    SWRAST(ifp)->win_height = SWRAST(ifp)->vp_height = height;

    /* initialize window state variables before calling swrast_getmem */
    ifp->i->if_xzoom = 1;	/* for zoom fakeout */
    ifp->i->if_yzoom = 1;	/* for zoom fakeout */
    ifp->i->if_xcenter = width/2;
    ifp->i->if_ycenter = height/2;

    /* Allocate memory */
    if (!SWRAST(ifp)->mi_memwidth) {
	if (swrast_getmem(ifp) < 0)
	    return -1;
    }

    fb_clipper(ifp);

    swrast_configureWindow(ifp, width, height);

    return 0;
}

static int
fb_swrast_open(struct fb *ifp, const char *UNUSED(file), int width, int height)
{
    FB_CK_FB(ifp->i);

    if ((ifp->i->pp = (char *)calloc(1, sizeof(struct swrastinfo))) == NULL) {
	fb_log("fb_swrast:  swrastinfo malloc failed\n");
	return -1;
    }

    ifp->i->stand_alone = 1;

    struct swrastinfo *qi = SWRAST(ifp);
    qi->av = (char **)calloc(2, sizeof(char *));
    qi->ac = 1;
    qi->av[0] = bu_strdup("Frame buffer");
    qi->av[1] = NULL;
    FB_CK_FB(ifp->i);

    qi->win_width = qi->vp_width = width;
    qi->win_height = qi->vp_width = height;

    qi->qapp = new QApplication(qi->ac, qi->av);

    qi->mw = new QtSWWin(ifp);

    BU_GET(qi->mw->canvas->v, struct bview);
    bv_init(qi->mw->canvas->v);
    qi->mw->canvas->v->gv_fb_mode = 1;
    qi->mw->canvas->v->gv_width = width;
    qi->mw->canvas->v->gv_height = height;


    qi->mw->canvas->setFixedSize(width, height);
    qi->mw->adjustSize();
    qi->mw->setFixedSize(qi->mw->size());
    qi->mw->show();

    // Do the standard libdm attach to get our rendering backend.
    const char *acmd = "attach";
    struct dm *dmp = dm_open((void *)qi->mw->canvas->v, NULL, "swrast", 1, &acmd);
    if (!dmp)
	return -1;

    struct fb_platform_specific fbps;
    fbps.magic = FB_SWFB_MAGIC;
    fbps.data = (void *)dmp;

    return swrast_open_existing(ifp, width, height, &fbps);
}

HIDDEN struct fb_platform_specific *
swrast_get_fbps(uint32_t magic)
{
    struct fb_platform_specific *fb_ps = NULL;
    BU_GET(fb_ps, struct fb_platform_specific);
    fb_ps->magic = magic;
    fb_ps->data = NULL;
    return fb_ps;
}


HIDDEN void
swrast_put_fbps(struct fb_platform_specific *fbps)
{
    BU_CKMAG(fbps, FB_SWFB_MAGIC, "swrast framebuffer");
    BU_PUT(fbps, struct fb_platform_specific);
    return;
}


HIDDEN int
swrast_flush(struct fb *UNUSED(ifp))
{
    glFlush();
    return 0;
}


HIDDEN int
fb_swrast_close(struct fb *ifp)
{
    struct swrastinfo *qi = SWRAST(ifp);

    /* if a window was created wait for user input and process events */
    if (qi->qapp) {
	return qi->qapp->exec();
	qt_destroy(qi);
    }

    return 0;
}

int
swrast_close_existing(struct fb *ifp)
{
    struct swrastinfo *qi = SWRAST(ifp);
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

    if (SWRAST(ifp)->alive)
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

    if (SWRASTL(ifp) != NULL) {
	(void)free((char *)SWRASTL(ifp));
	SWRASTL(ifp) = NULL;
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
	swrastp = (struct fb_pixel *)&ifp->i->if_mem[(y*SWRAST(ifp)->mi_memwidth+0)*sizeof(struct fb_pixel) ];
	for (cnt = ifp->i->if_width-1; cnt >= 0; cnt--) {
	    *swrastp++ = bg;	/* struct copy */
	}
    }

    dm_make_current(SWRAST(ifp)->dmp);

    if (pp != RGBPIXEL_NULL) {
	glClearColor(pp[RED]/255.0, pp[GRN]/255.0, pp[BLU]/255.0, 0.0);
    } else {
	glClearColor(0, 0, 0, 0);
    }

    glClear(GL_COLOR_BUFFER_BIT);

    return 0;
}


static int
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

    dm_make_current(SWRAST(ifp)->dmp);

    /* Set clipping matrix and zoom level */
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    fb_clipper(ifp);
    clp = &(SWRAST(ifp)->clip);
    glOrtho(clp->oleft, clp->oright, clp->obottom, clp->otop, -1.0, 1.0);
    glPixelZoom((float) ifp->i->if_xzoom, (float) ifp->i->if_yzoom);

    glFlush();

    SWRAST(ifp)->mw->update();

    return 0;
}


static int
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
static ssize_t
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

	swrastp = (struct fb_pixel *)&ifp->i->if_mem[(y*SWRAST(ifp)->mi_memwidth+x)*sizeof(struct fb_pixel) ];

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
static ssize_t
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

	swrastp = (struct fb_pixel *)&ifp->i->if_mem[(y*SWRAST(ifp)->mi_memwidth+x)*sizeof(struct fb_pixel) ];

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
	swrastp = (struct fb_pixel *)&ifp->i->if_mem[(y*SWRAST(ifp)->mi_memwidth+xmin)*sizeof(struct fb_pixel) ];
	for (x = xmin; x < xmin+width; x++) {
	    /* alpha channel is always zero */
	    swrastp->red   = cp[RED];
	    swrastp->green = cp[GRN];
	    swrastp->blue  = cp[BLU];
	    swrastp++;
	    cp += 3;
	}
    }

    SWRAST(ifp)->mw->update();

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
	swrastp = (struct fb_pixel *)&ifp->i->if_mem[(y*SWRAST(ifp)->mi_memwidth+xmin)*sizeof(struct fb_pixel) ];
	for (x = xmin; x < xmin+width; x++) {
	    int val;
	    /* alpha channel is always zero */
	    swrastp->red   = (val = *cp++);
	    swrastp->green = val;
	    swrastp->blue  = val;
	    swrastp++;
	}
    }

    SWRAST(ifp)->mw->update();

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

    if (w < 0) {
	w = -w;
	x -= w;
    }

    if (h < 0) {
	h = -h;
	y -= h;
    }

    int mm;
    struct fb_clip *clp;

    glGetIntegerv(GL_MATRIX_MODE, &mm);
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();

    fb_clipper(ifp);
    clp = &(SWRAST(ifp)->clip);
    glOrtho(clp->oleft, clp->oright, clp->obottom, clp->otop, -1.0, 1.0);
    glPixelZoom((float) ifp->i->if_xzoom, (float) ifp->i->if_yzoom);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    glViewport(0, 0, SWRAST(ifp)->win_width, SWRAST(ifp)->win_height);
    swrast_xmit_scanlines(ifp, y, h, x, w);
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
    glMatrixMode(mm);

    dm_set_dirty(SWRAST(ifp)->dmp, 1);
    return 0;
}


/* This is the ONLY thing that we normally "export" */
struct fb_impl swrast_interface_impl =
{
    0,			/* magic number slot */
    FB_SWFB_MAGIC,
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
    bu_strdup("OSMesa swrast OpenGL"),	/* device description */
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
