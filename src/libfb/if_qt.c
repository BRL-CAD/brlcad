/*                       I F _ Q T . C
 * BRL-CAD
 *
 * Copyright (c) 2014 United States Government as represented by
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
/** @file if_qt.c
 *
 * A Qt Frame Buffer.
 *
 */
/** @} */

#include "common.h"

#include <stdlib.h>
#include <stdio.h>

#include "fb.h"


HIDDEN int
qt_open(FBIO *UNUSED(ifp), const char *UNUSED(file), int UNUSED(width), int UNUSED(height))
{
    fb_log("qt_open\n");

    return 0;
}


HIDDEN int
qt_close(FBIO *UNUSED(ifp))
{
    fb_log("qt_close\n");

    return 0;
}


HIDDEN int
qt_clear(FBIO *UNUSED(ifp), unsigned char *UNUSED(pp))
{
    fb_log("qt_clear\n");

    return 0;
}


HIDDEN ssize_t
qt_read(FBIO *UNUSED(ifp), int UNUSED(x), int UNUSED(y), unsigned char *UNUSED(pixelp), size_t count)
{
    fb_log("qt_read\n");

    return count;
}


HIDDEN ssize_t
qt_write(FBIO *UNUSED(ifp), int UNUSED(x), int UNUSED(y), const unsigned char *UNUSED(pixelp), size_t count)
{
    fb_log("qt_write\n");

    return count;
}


HIDDEN int
qt_rmap(FBIO *UNUSED(ifp), ColorMap *UNUSED(cmp))
{
    fb_log("qt_rmap\n");

    return 0;
}


HIDDEN int
qt_wmap(FBIO *UNUSED(ifp), const ColorMap *UNUSED(cmp))
{
    fb_log("qt_wmap\n");

    return 0;
}


HIDDEN int
qt_view(FBIO *UNUSED(ifp), int UNUSED(xcenter), int UNUSED(ycenter), int UNUSED(xzoom), int UNUSED(yzoom))
{
    fb_log("qt_view\n");

    return 0;
}


HIDDEN int
qt_getview(FBIO *UNUSED(ifp), int *UNUSED(xcenter), int *UNUSED(ycenter), int *UNUSED(xzoom), int *UNUSED(yzoom))
{
    fb_log("qt_getview\n");

    return 0;
}


HIDDEN int
qt_setcursor(FBIO *UNUSED(ifp), const unsigned char *UNUSED(bits), int UNUSED(xbits), int UNUSED(ybits), int UNUSED(xorig), int UNUSED(yorig))
{
    fb_log("qt_setcursor\n");

    return 0;
}


HIDDEN int
qt_cursor(FBIO *UNUSED(ifp), int UNUSED(mode), int UNUSED(x), int UNUSED(y))
{
    fb_log("qt_cursor\n");

    return 0;
}


HIDDEN int
qt_getcursor(FBIO *UNUSED(ifp), int *UNUSED(mode), int *UNUSED(x), int *UNUSED(y))
{
    fb_log("qt_getcursor\n");

    return 0;
}


HIDDEN int
qt_readrect(FBIO *UNUSED(ifp), int UNUSED(xmin), int UNUSED(ymin), int width, int height, unsigned char *UNUSED(pp))
{
    fb_log("qt_readrect\n");

    return width*height;
}


HIDDEN int
qt_writerect(FBIO *UNUSED(ifp), int UNUSED(xmin), int UNUSED(ymin), int width, int height, const unsigned char *UNUSED(pp))
{
    fb_log("qt_writerect\n");

    return width*height;
}


HIDDEN int
qt_poll(FBIO *UNUSED(ifp))
{
    fb_log("qt_poll\n");

    return 0;
}


HIDDEN int
qt_flush(FBIO *UNUSED(ifp))
{
    fb_log("qt_flush\n");

    return 0;
}


HIDDEN int
qt_free(FBIO *UNUSED(ifp))
{
    fb_log("qt_free\n");

    return 0;
}


HIDDEN int
qt_help(FBIO *ifp)
{
    fb_log("Description: %s\n", qt_interface.if_type);
    fb_log("Device: %s\n", ifp->if_name);
    fb_log("Max width/height: %d %d\n",
	   qt_interface.if_max_width,
	   qt_interface.if_max_height);
    fb_log("Default width/height: %d %d\n",
	   qt_interface.if_width,
	   qt_interface.if_height);
    fb_log("Useful for Benchmarking/Debugging\n");
    return 0;
}


FBIO qt_interface =  {
    0,
    qt_open,		/* device_open */
    qt_close,		/* device_close */
    qt_clear,		/* device_clear */
    qt_read,		/* buffer_read */
    qt_write,		/* buffer_write */
    qt_rmap,		/* colormap_read */
    qt_wmap,		/* colormap_write */
    qt_view,		/* set view */
    qt_getview,	/* get view */
    qt_setcursor,	/* define cursor */
    qt_cursor,	/* set cursor */
    qt_getcursor,	/* get cursor */
    qt_readrect,	/* rectangle read */
    qt_writerect,	/* rectangle write */
    qt_readrect,	/* bw rectangle read */
    qt_writerect,	/* bw rectangle write */
    qt_poll,		/* handle events */
    qt_flush,		/* flush output */
    qt_free,		/* free resources */
    qt_help,		/* help message */
    "Qt Device",	/* device description */
    32*1024,		/* max width */
    32*1024,		/* max height */
    "/dev/Qt",	/* short device name */
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
