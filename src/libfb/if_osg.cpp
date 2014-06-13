/*                       I F _ O S G . C P P
 * BRL-CAD
 *
 * Copyright (c) 1989-2014 United States Government as represented by
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
 * A OpenSceneGraph Frame Buffer.
 *
 */
/** @} */

#include "common.h"

#include <stdlib.h>
#include <stdio.h>

#include "bu/log.h"
#include "bu/str.h"
#include "fb.h"

#include <osg/GraphicsContext>
#include <osgViewer/Viewer>
#include <osgViewer/ViewerEventHandlers>

#include <osgGA/TrackballManipulator>
#include <osgGA/StateSetManipulator>

#include <osg/ImageUtils>
#include <osg/TextureRectangle>
#include <osg/Geode>
#include <osg/Geometry>
#include <osg/StateSet>

struct osginfo {
    osgViewer::Viewer *viewer;
    osg::Image *image;
    osg::TextureRectangle *texture;
    osg::Geometry *pictureQuad;
};

#define OSG(ptr) ((struct osginfo *)((ptr)->u6.p))

HIDDEN int
osg_open(FBIO *ifp, const char *UNUSED(file), int width, int height)
{
    FB_CK_FBIO(ifp);

    /* Get some memory for the osg specific stuff */
    if ((ifp->u6.p = (char *)calloc(1, sizeof(struct osginfo))) == NULL) {
	fb_log("fb_osg_open:  osginfo malloc failed\n");
	return -1;
    }

    if (width > 0)
	ifp->if_width = width;
    if (height > 0)
	ifp->if_height = height;

    OSG(ifp)->viewer = new osgViewer::Viewer();
    OSG(ifp)->viewer->setUpViewInWindow(0, 0, width, height);

    OSG(ifp)->image = new osg::Image;
    OSG(ifp)->image->allocateImage(width, height, 1, GL_RGB, GL_UNSIGNED_BYTE);

    OSG(ifp)->pictureQuad = osg::createTexturedQuadGeometry(osg::Vec3(0.0f,0.0f,0.0f),
	    osg::Vec3(width,0.0f,0.0f), osg::Vec3(0.0f,height,0.0f), 0.0f, 0.0, OSG(ifp)->image->s(), OSG(ifp)->image->t());
    OSG(ifp)->texture = new osg::TextureRectangle(OSG(ifp)->image);
    OSG(ifp)->pictureQuad->getOrCreateStateSet()->setTextureAttributeAndModes(0, OSG(ifp)->texture, osg::StateAttribute::ON);


    osg::ref_ptr<osg::Geode> geode = new osg::Geode;
    osg::StateSet* stateset = geode->getOrCreateStateSet();
    stateset->setMode(GL_LIGHTING,osg::StateAttribute::OFF);
    geode->addDrawable(OSG(ifp)->pictureQuad);

#if 0
    OSG(ifp)->viewer->getCamera()->setViewMatrix(osg::Matrix::identity());
    osg::Vec3 topleft(0.0f, 0.0f, 0.0f);
    osg::Vec3 bottomright(width, height, 0.0f);
    OSG(ifp)->viewer->getCamera()->setProjectionMatrixAsOrtho2D(topleft.x(),bottomright.x(),topleft.y(),bottomright.y());
    OSG(ifp)->viewer->getCamera()->setClearColor(osg::Vec4(0.0f,0.0f,0.0f,1.0f));
#endif


    OSG(ifp)->viewer->setSceneData(geode.get());
    OSG(ifp)->viewer->realize();
    OSG(ifp)->viewer->frame();

    return 0;
}

HIDDEN int
osg_close(FBIO *ifp)
{
    FB_CK_FBIO(ifp);

    return OSG(ifp)->viewer->run();

}


HIDDEN int
osg_clear(FBIO *ifp, unsigned char *UNUSED(pp))
{
    FB_CK_FBIO(ifp);

    return 0;
}


HIDDEN ssize_t
osg_read(FBIO *ifp, int UNUSED(x), int UNUSED(y), unsigned char *UNUSED(pixelp), size_t count)
{
    FB_CK_FBIO(ifp);

    return count;
}


HIDDEN ssize_t
osg_write(FBIO *ifp, int xstart, int ystart, const unsigned char *pixelp, size_t count)
{
    register int x;
    register int y;
    size_t scan_count;  /* # pix on this scanline */
    size_t pix_count;   /* # pixels to send */
    ssize_t ret;

    //fb_log("write got called!");

    FB_CK_FBIO(ifp);

    /* fast exit cases */
    pix_count = count;
    if (pix_count == 0)
	return 0;       /* OK, no pixels transferred */

    x = xstart;
    y = ystart;

    if (x < 0 || x >= ifp->if_width ||
	    y < 0 || y >= ifp->if_height)
	return -1;

    ret = 0;

    while (pix_count) {
	void *scanline;

	if (y >= ifp->if_height)
	    break;

	if (pix_count >= (size_t)(ifp->if_width-x))
	    scan_count = (size_t)(ifp->if_width-x);
	else
	    scan_count = pix_count;

	scanline = (void *)(OSG(ifp)->image->data(0,y,0));

	memcpy(scanline, pixelp, scan_count*3);

	ret += scan_count;
	pix_count -= scan_count;
	x = 0;
	if (++y >= ifp->if_height)
	    break;
    }

    OSG(ifp)->viewer->frame();

    return ret;
}


HIDDEN int
osg_rmap(FBIO *ifp, ColorMap *UNUSED(cmp))
{
    FB_CK_FBIO(ifp);

    return 0;
}


HIDDEN int
osg_wmap(FBIO *ifp, const ColorMap *UNUSED(cmp))
{
    FB_CK_FBIO(ifp);

    fb_log("wmap got called!");

    return 0;
}


HIDDEN int
osg_view(FBIO *ifp, int UNUSED(xcenter), int UNUSED(ycenter), int UNUSED(xzoom), int UNUSED(yzoom))
{
    FB_CK_FBIO(ifp);
    fb_log("view was called!\n");

    /*fb_sim_view(ifp, xcenter, ycenter, xzoom, yzoom);*/
    return 0;
}


HIDDEN int
osg_getview(FBIO *ifp, int *UNUSED(xcenter), int *UNUSED(ycenter), int *UNUSED(xzoom), int *UNUSED(yzoom))
{
    FB_CK_FBIO(ifp);

    /*fb_sim_getview(ifp, xcenter, ycenter, xzoom, yzoom);*/
    return 0;
}


HIDDEN int
osg_setcursor(FBIO *ifp, const unsigned char *UNUSED(bits), int UNUSED(xbits), int UNUSED(ybits), int UNUSED(xorig), int UNUSED(yorig))
{
    FB_CK_FBIO(ifp);

    return 0;
}


HIDDEN int
osg_cursor(FBIO *ifp, int UNUSED(mode), int UNUSED(x), int UNUSED(y))
{
    FB_CK_FBIO(ifp);

    /*fb_sim_cursor(ifp, mode, x, y);*/
    return 0;
}


HIDDEN int
osg_getcursor(FBIO *ifp, int *UNUSED(mode), int *UNUSED(x), int *UNUSED(y))
{
    FB_CK_FBIO(ifp);

    /*fb_sim_getcursor(ifp, mode, x, y);*/
    return 0;
}


HIDDEN int
osg_readrect(FBIO *ifp, int UNUSED(xmin), int UNUSED(ymin), int width, int height, unsigned char *UNUSED(pp))
{
    FB_CK_FBIO(ifp);

    return width*height;
}


HIDDEN int
osg_writerect(FBIO *ifp, int UNUSED(xmin), int UNUSED(ymin), int width, int height, const unsigned char *UNUSED(pp))
{
    FB_CK_FBIO(ifp);

    fb_log("writerect got called!");

    return width*height;
}


HIDDEN int
osg_poll(FBIO *ifp)
{
    FB_CK_FBIO(ifp);

    return 0;
}


HIDDEN int
osg_flush(FBIO *ifp)
{
    FB_CK_FBIO(ifp);

    fb_log("flush was called!\n");

    return 0;
}


HIDDEN int
osg_free(FBIO *ifp)
{
    FB_CK_FBIO(ifp);

    return 0;
}


HIDDEN int
osg_help(FBIO *ifp)
{
    FB_CK_FBIO(ifp);

    fb_log("Description: %s\n", osg_interface.if_type);
    fb_log("Device: %s\n", ifp->if_name);
    fb_log("Max width/height: %d %d\n",
	   osg_interface.if_max_width,
	   osg_interface.if_max_height);
    fb_log("Default width/height: %d %d\n",
	   osg_interface.if_width,
	   osg_interface.if_height);
    fb_log("Useful for Benchmarking/Debugging\n");
    return 0;
}

/* Functions for pre-exising windows */
extern "C" int
osg_close_existing(FBIO *ifp)
{
    FB_CK_FBIO(ifp);
    return 0;
}

extern "C" int
_osg_open_existing(FBIO *ifp, Display *dpy, Window win, Colormap cmap, int width, int height, osg::ref_ptr<osg::GraphicsContext> graphicsContext)
{
    FB_CK_FBIO(ifp);
    if (!dpy || !win || !cmap || !width || !height || !graphicsContext) return -1;
    return 0;
}

extern "C" int
osg_open_existing(FBIO *ifp, int argc, const char **argv)
{
    FB_CK_FBIO(ifp);
    if (argc != 10 || !argv) return -1;

    return 0;
}

extern "C" int
osg_refresh(FBIO *ifp, int x, int y, int w, int h){
    FB_CK_FBIO(ifp);
    if (!x || !y || !w || !h) return -1;

    fb_log("refresh got called!\n");
    return 0;
}

extern "C" void
osg_configureWindow(FBIO *ifp, int width, int height)
{
    FB_CK_FBIO(ifp);

    if (!width || !height) return;
}

/* This is the ONLY thing that we normally "export" */
FBIO osg_interface =  {
    0,
    osg_open,		/* device_open */
    osg_close,		/* device_close */
    osg_clear,		/* device_clear */
    osg_read,		/* buffer_read */
    osg_write,		/* buffer_write */
    osg_rmap,		/* colormap_read */
    osg_wmap,		/* colormap_write */
    osg_view,		/* set view */
    osg_getview,	/* get view */
    osg_setcursor,	/* define cursor */
    osg_cursor,		/* set cursor */
    osg_getcursor,	/* get cursor */
    osg_readrect,	/* rectangle read */
    osg_writerect,	/* rectangle write */
    osg_readrect,	/* bw rectangle read */
    osg_writerect,	/* bw rectangle write */
    osg_poll,		/* handle events */
    osg_flush,		/* flush output */
    osg_free,		/* free resources */
    osg_help,		/* help message */
    "OpenSceneGraph",	/* device description */
    32*1024,		/* max width */
    32*1024,		/* max height */
    bu_strdup("/dev/osg"),		/* short device name */
    512,		/* default/current width */
    512,		/* default/current height */
    -1,			/* select fd */
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


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
