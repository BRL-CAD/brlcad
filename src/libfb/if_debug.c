/*                      I F _ D E B U G . C
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
/** @file if_debug.c
 *
 * Reports all calls to fb_log().
 */
/** @} */

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

#include "bu/color.h"
#include "bu/log.h"
#include "fb_private.h"
#include "fb.h"

HIDDEN int
deb_open(fb *ifp, const char *file, int width, int height)
{
    FB_CK_FB(ifp);
    if (file == (char *)NULL)
	fb_log("fb_open(%p, NULL, %d, %d)\n",
	       (void *)ifp, width, height);
    else
	fb_log("fb_open(%p, \"%s\", %d, %d)\n",
	       (void *)ifp, file, width, height);

    /* check for default size */
    if (width <= 0)
	width = ifp->if_width;
    if (height <= 0)
	height = ifp->if_height;

    /* set debug bit vector */
    if (file != NULL) {
	const char *cp;
	for (cp = file; *cp != '\0' && !isdigit((int)*cp); cp++)
	    ;
	sscanf(cp, "%d", &ifp->if_debug);
    } else {
	ifp->if_debug = 0;
    }

    /* Give the user whatever width was asked for */
    ifp->if_width = width;
    ifp->if_height = height;

    return 0;
}

HIDDEN struct fb_platform_specific *
deb_get_fbps(uint32_t UNUSED(magic))
{
        return NULL;
}


HIDDEN void
deb_put_fbps(struct fb_platform_specific *UNUSED(fbps))
{
        return;
}

HIDDEN int
deb_open_existing(fb *UNUSED(ifp), int UNUSED(width), int UNUSED(height), struct fb_platform_specific *UNUSED(fb_p))
{
        return 0;
}

HIDDEN int
deb_close_existing(fb *UNUSED(ifp))
{
        return 0;
}

HIDDEN int
deb_close(fb *ifp)
{
    FB_CK_FB(ifp);
    fb_log("fb_close(%p)\n", (void *)ifp);
    return 0;
}

HIDDEN int
deb_configure_window(fb *UNUSED(ifp), int UNUSED(width), int UNUSED(height))
{
    return 0;
}

HIDDEN int
deb_refresh(fb *UNUSED(ifp), int UNUSED(x), int UNUSED(y), int UNUSED(w), int UNUSED(h))
{
    return 0;
}

HIDDEN int
deb_clear(fb *ifp, unsigned char *pp)
{
    FB_CK_FB(ifp);
    if (pp == 0)
	fb_log("fb_clear(%p, NULL)\n", (void *)ifp);
    else
	fb_log("fb_clear(x%p, &[%d %d %d])\n",
	       (void *)ifp,
	       (int)(pp[RED]), (int)(pp[GRN]),
	       (int)(pp[BLU]));
    return 0;
}


HIDDEN ssize_t
deb_read(fb *ifp, int x, int y, unsigned char *pixelp, size_t count)
{
    FB_CK_FB(ifp);
    fb_log("fb_read(%p, %4d, %4d, %p, %lu)\n",
	   (void *)ifp, x, y,
	   (void *)pixelp, count);
    return count;
}


HIDDEN ssize_t
deb_write(fb *ifp, int x, int y, const unsigned char *pixelp, size_t count)
{
    size_t i;

    FB_CK_FB(ifp);
    fb_log("fb_write(%p, %4d, %4d, %p, %ld)\n",
	   (void *)ifp, x, y,
	   (void *)pixelp, (long)count);

    /* write them out, four per line */
    if (ifp->if_debug & FB_DEBUG_RW) {
	for (i = 0; i < count; i++) {
	    if (i % 4 == 0)
		fb_log("%4zu:", i);
	    fb_log("  [%3d, %3d, %3d]", *(pixelp+(i*3)+RED),
		   *(pixelp+(i*3)+GRN), *(pixelp+(i*3)+BLU));
	    if (i % 4 == 3)
		fb_log("\n");
	}
	if (i % 4 != 0)
	    fb_log("\n");
    }

    return count;
}


HIDDEN int
deb_rmap(fb *ifp, ColorMap *cmp)
{
    FB_CK_FB(ifp);
    fb_log("fb_rmap(%p, %p)\n",
	   (void *)ifp, (void *)cmp);
    return 0;
}


HIDDEN int
deb_wmap(fb *ifp, const ColorMap *cmp)
{
    int i;

    FB_CK_FB(ifp);
    if (cmp == NULL)
	fb_log("fb_wmap(%p, NULL)\n",
	       (void *)ifp);
    else
	fb_log("fb_wmap(%p, %p)\n",
	       (void *)ifp, (void *)cmp);

    if (ifp->if_debug & FB_DEBUG_CMAP && cmp != NULL) {
	for (i = 0; i < 256; i++) {
	    fb_log("%3d: [ 0x%4lx, 0x%4lx, 0x%4lx ]\n",
		   i,
		   (unsigned long)cmp->cm_red[i],
		   (unsigned long)cmp->cm_green[i],
		   (unsigned long)cmp->cm_blue[i]);
	}
    }

    return 0;
}


HIDDEN int
deb_view(fb *ifp, int xcenter, int ycenter, int xzoom, int yzoom)
{
    FB_CK_FB(ifp);
    fb_log("fb_view(%p, %4d, %4d, %4d, %4d)\n",
	   (void *)ifp, xcenter, ycenter, xzoom, yzoom);
    fb_sim_view(ifp, xcenter, ycenter, xzoom, yzoom);
    return 0;
}


HIDDEN int
deb_getview(fb *ifp, int *xcenter, int *ycenter, int *xzoom, int *yzoom)
{
    FB_CK_FB(ifp);
    fb_log("fb_getview(%p, %p, %p, %p, %p)\n",
	   (void *)ifp, (void *)xcenter, (void *)ycenter, (void *)xzoom, (void *)yzoom);
    fb_sim_getview(ifp, xcenter, ycenter, xzoom, yzoom);
    fb_log(" <= %d %d %d %d\n",
	   *xcenter, *ycenter, *xzoom, *yzoom);
    return 0;
}


HIDDEN int
deb_setcursor(fb *ifp, const unsigned char *bits, int xbits, int ybits, int xorig, int yorig)
{
    FB_CK_FB(ifp);
    fb_log("fb_setcursor(%p, %p, %d, %d, %d, %d)\n",
	   (void *)ifp, (void *)bits, xbits, ybits, xorig, yorig);
    return 0;
}


HIDDEN int
deb_cursor(fb *ifp, int mode, int x, int y)
{
    fb_log("fb_cursor(%p, %d, %4d, %4d)\n",
	   (void *)ifp, mode, x, y);
    fb_sim_cursor(ifp, mode, x, y);
    return 0;
}


HIDDEN int
deb_getcursor(fb *ifp, int *mode, int *x, int *y)
{
    FB_CK_FB(ifp);
    fb_log("fb_getcursor(%p, %p, %p, %p)\n",
	   (void *)ifp, (void *)mode, (void *)x, (void *)y);
    fb_sim_getcursor(ifp, mode, x, y);
    fb_log(" <= %d %d %d\n", *mode, *x, *y);
    return 0;
}


HIDDEN int
deb_readrect(fb *ifp, int xmin, int ymin, int width, int height, unsigned char *pp)
{
    FB_CK_FB(ifp);
    fb_log("fb_readrect(%p, (%4d, %4d), %4d, %4d, %p)\n",
	   (void *)ifp, xmin, ymin, width, height,
	   (void *)pp);
    return width*height;
}


HIDDEN int
deb_writerect(fb *ifp, int xmin, int ymin, int width, int height, const unsigned char *pp)
{
    FB_CK_FB(ifp);
    fb_log("fb_writerect(%p, %4d, %4d, %4d, %4d, %p)\n",
	   (void *)ifp, xmin, ymin, width, height,
	   (void *)pp);
    return width*height;
}


HIDDEN int
deb_bwreadrect(fb *ifp, int xmin, int ymin, int width, int height, unsigned char *pp)
{
    FB_CK_FB(ifp);
    fb_log("fb_bwreadrect(%p, (%4d, %4d), %4d, %4d, %p)\n",
	   (void *)ifp, xmin, ymin, width, height,
	   (void *)pp);
    return width*height;
}


HIDDEN int
deb_bwwriterect(fb *ifp, int xmin, int ymin, int width, int height, const unsigned char *pp)
{
    FB_CK_FB(ifp);
    fb_log("fb_bwwriterect(%p, %4d, %4d, %4d, %4d, %p)\n",
	   (void *)ifp, xmin, ymin, width, height,
	   (void *)pp);
    return width*height;
}


HIDDEN int
deb_poll(fb *ifp)
{
    FB_CK_FB(ifp);
    fb_log("fb_poll(%p)\n", (void *)ifp);
    return 0;
}


HIDDEN int
deb_flush(fb *ifp)
{
    FB_CK_FB(ifp);
    fb_log("if_flush(%p)\n", (void *)ifp);
    return 0;
}


HIDDEN int
deb_free(fb *ifp)
{
    FB_CK_FB(ifp);
    fb_log("fb_free(%p)\n", (void *)ifp);
    return 0;
}


/*ARGSUSED*/
HIDDEN int
deb_help(fb *ifp)
{
    FB_CK_FB(ifp);
    fb_log("Description: %s\n", debug_interface.if_type);
    fb_log("Device: %s\n", ifp->if_name);
    fb_log("Max width/height: %d %d\n",
	   debug_interface.if_max_width,
	   debug_interface.if_max_height);
    fb_log("Default width/height: %d %d\n",
	   debug_interface.if_width,
	   debug_interface.if_height);
    fb_log("\
Usage: /dev/debug[#]\n\
  where # is a optional bit vector from:\n\
    1    debug buffered I/O calls\n\
    2    show colormap entries in rmap/wmap calls\n\
    4    show actual pixel values in read/write calls\n");
    /*8    buffered read/write values - ifdef'd out*/

    return 0;
}


/* This is the ONLY thing that we "export" */
fb debug_interface = {
    0,
    FB_DEBUG_MAGIC,
    deb_open,
    deb_open_existing,
    deb_close_existing,
    deb_get_fbps,
    deb_put_fbps,
    deb_close,
    deb_clear,
    deb_read,
    deb_write,
    deb_rmap,
    deb_wmap,
    deb_view,
    deb_getview,
    deb_setcursor,
    deb_cursor,
    deb_getcursor,
    deb_readrect,
    deb_writerect,
    deb_bwreadrect,
    deb_bwwriterect,
    deb_configure_window,
    deb_refresh,
    deb_poll,
    deb_flush,
    deb_free,
    deb_help,
    "Debugging Interface",
    32*1024,		/* max width */
    32*1024,		/* max height */
    "/dev/debug",
    512,			/* current/default width */
    512,			/* current/default height */
    -1,			/* select fd */
    -1,			/* file descriptor */
    1, 1,			/* zoom */
    256, 256,		/* window center */
    0, 0, 0,		/* cursor */
    PIXEL_NULL,		/* page_base */
    PIXEL_NULL,		/* page_curp */
    PIXEL_NULL,		/* page_endp */
    -1,			/* page_no */
    0,			/* page_ref */
    0L,			/* page_curpos */
    0L,			/* page_pixels */
    0,			/* debug */
    0,			/* refresh rate */
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
