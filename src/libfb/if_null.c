/*                       I F _ N U L L . C
 * BRL-CAD
 *
 * Copyright (c) 1989-2011 United States Government as represented by
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
/** @file if_null.c
 *
 * A Null Frame Buffer.
 * Useful for benchmarking or debugging.
 *
 */
/** @} */

#include "common.h"

#include <stdlib.h>
#include <stdio.h>

#include "fb.h"


HIDDEN int
null_open(FBIO *ifp, const char *UNUSED(file), int width, int height)
{
    FB_CK_FBIO(ifp);

    if (width > 0)
	ifp->if_width = width;
    if (height > 0)
	ifp->if_height = height;

    return 0;
}


HIDDEN int
null_close(FBIO *ifp)
{
    FB_CK_FBIO(ifp);

    return 0;
}


HIDDEN int
null_clear(FBIO *ifp, unsigned char *UNUSED(pp))
{
    FB_CK_FBIO(ifp);

    return 0;
}


HIDDEN int
null_read(FBIO *ifp, int UNUSED(x), int UNUSED(y), unsigned char *UNUSED(pixelp), size_t count)
{
    FB_CK_FBIO(ifp);

    return (int)count;
}


HIDDEN int
null_write(FBIO *ifp, int UNUSED(x), int UNUSED(y), const unsigned char *UNUSED(pixelp), size_t count)
{
    FB_CK_FBIO(ifp);

    return (int)count;
}


HIDDEN int
null_rmap(FBIO *ifp, ColorMap *UNUSED(cmp))
{
    FB_CK_FBIO(ifp);

    return 0;
}


HIDDEN int
null_wmap(FBIO *ifp, const ColorMap *UNUSED(cmp))
{
    FB_CK_FBIO(ifp);

    return 0;
}


HIDDEN int
null_view(FBIO *ifp, int UNUSED(xcenter), int UNUSED(ycenter), int UNUSED(xzoom), int UNUSED(yzoom))
{
    FB_CK_FBIO(ifp);

    /*fb_sim_view(ifp, xcenter, ycenter, xzoom, yzoom);*/
    return 0;
}


HIDDEN int
null_getview(FBIO *ifp, int *UNUSED(xcenter), int *UNUSED(ycenter), int *UNUSED(xzoom), int *UNUSED(yzoom))
{
    FB_CK_FBIO(ifp);

    /*fb_sim_getview(ifp, xcenter, ycenter, xzoom, yzoom);*/
    return 0;
}


HIDDEN int
null_setcursor(FBIO *ifp, const unsigned char *UNUSED(bits), int UNUSED(xbits), int UNUSED(ybits), int UNUSED(xorig), int UNUSED(yorig))
{
    FB_CK_FBIO(ifp);

    return 0;
}


HIDDEN int
null_cursor(FBIO *ifp, int UNUSED(mode), int UNUSED(x), int UNUSED(y))
{
    FB_CK_FBIO(ifp);

    /*fb_sim_cursor(ifp, mode, x, y);*/
    return 0;
}


HIDDEN int
null_getcursor(FBIO *ifp, int *UNUSED(mode), int *UNUSED(x), int *UNUSED(y))
{
    FB_CK_FBIO(ifp);

    /*fb_sim_getcursor(ifp, mode, x, y);*/
    return 0;
}


HIDDEN int
null_readrect(FBIO *ifp, int UNUSED(xmin), int UNUSED(ymin), int width, int height, unsigned char *UNUSED(pp))
{
    FB_CK_FBIO(ifp);

    return width*height;
}


HIDDEN int
null_writerect(FBIO *ifp, int UNUSED(xmin), int UNUSED(ymin), int width, int height, const unsigned char *UNUSED(pp))
{
    FB_CK_FBIO(ifp);

    return width*height;
}


HIDDEN int
null_poll(FBIO *ifp)
{
    FB_CK_FBIO(ifp);

    return 0;
}


HIDDEN int
null_flush(FBIO *ifp)
{
    FB_CK_FBIO(ifp);

    return 0;
}


HIDDEN int
null_free(FBIO *ifp)
{
    FB_CK_FBIO(ifp);

    return 0;
}


HIDDEN int
null_help(FBIO *ifp)
{
    FB_CK_FBIO(ifp);

    fb_log("Description: %s\n", null_interface.if_type);
    fb_log("Device: %s\n", ifp->if_name);
    fb_log("Max width/height: %d %d\n",
	   null_interface.if_max_width,
	   null_interface.if_max_height);
    fb_log("Default width/height: %d %d\n",
	   null_interface.if_width,
	   null_interface.if_height);
    fb_log("Useful for Benchmarking/Debugging\n");
    return 0;
}


/* This is the ONLY thing that we normally "export" */
FBIO null_interface =  {
    0,
    null_open,		/* device_open */
    null_close,		/* device_close */
    null_clear,		/* device_clear */
    null_read,		/* buffer_read */
    null_write,		/* buffer_write */
    null_rmap,		/* colormap_read */
    null_wmap,		/* colormap_write */
    null_view,		/* set view */
    null_getview,	/* get view */
    null_setcursor,	/* define cursor */
    null_cursor,	/* set cursor */
    null_getcursor,	/* get cursor */
    null_readrect,	/* rectangle read */
    null_writerect,	/* rectangle write */
    null_readrect,	/* bw rectangle read */
    null_writerect,	/* bw rectangle write */
    null_poll,		/* handle events */
    null_flush,		/* flush output */
    null_free,		/* free resources */
    null_help,		/* help message */
    "Null Device",	/* device description */
    32*1024,		/* max width */
    32*1024,		/* max height */
    "/dev/null",	/* short device name */
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
