/*                      I F _ S T A C K . C
 * BRL-CAD
 *
 * Copyright (c) 1986-2016 United States Government as represented by
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
/** @addtogroup libfb */
/** @{ */
/** @file if_stack.c
 *
 * Allows multiple frame buffers to be ganged together.
 *
 */
/** @} */

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "bu/log.h"
#include "bu/str.h"
#include "fb_private.h"
#include "fb.h"


/* List of interface struct pointers, one per dev */
#define MAXIF 32
struct stkinfo {
    fb *if_list[MAXIF];
};
#define SI(ptr) ((struct stkinfo *)((ptr)->u1.p))
#define SIL(ptr) ((ptr)->u1.p)		/* left hand side version */

HIDDEN int
stk_open(fb *ifp, const char *file, int width, int height)
{
    int i;
    const char *cp;
    char devbuf[80];

    FB_CK_FB(ifp);

    /* Check for /dev/stack */
    if (bu_strncmp(file, ifp->if_name, strlen("/dev/stack")) != 0) {
	fb_log("stack_dopen: Bad device %s\n", file);
	return -1;
    }

    if ((SIL(ifp) = (char *)calloc(1, sizeof(struct stkinfo))) == NULL) {
	fb_log("stack_dopen:  stkinfo malloc failed\n");
	return -1;
    }

    cp = &file[strlen("/dev/stack")];
    while (*cp != '\0' && *cp != ' ' && *cp != '\t')
	cp++;	/* skip suffix */

    /* special check for a possibly user confusing case */
    if (*cp == '\0') {
	fb_log("stack_dopen: No devices specified\n");
	fb_log("Usage: /dev/stack device_one; device_two; ...\n");
	return -1;
    }

    ifp->if_width = ifp->if_max_width;
    ifp->if_height = ifp->if_max_height;
    i = 0;
    while (i < MAXIF && *cp != '\0') {
	register char *dp;
	register fb *fbp;

	while (*cp != '\0' && (*cp == ' ' || *cp == '\t' || *cp == ';'))
	    cp++;	/* skip blanks and separators */
	if (*cp == '\0')
	    break;
	dp = devbuf;
	while (*cp != '\0' && *cp != ';')
	    *dp++ = *cp++;
	*dp = '\0';
	if ((fbp = fb_open(devbuf, width, height)) != FB_NULL) {
	    FB_CK_FB(fbp);
	    /* Track the minimum of all the actual sizes */
	    if (fbp->if_width < ifp->if_width)
		ifp->if_width = fbp->if_width;
	    if (fbp->if_height < ifp->if_height)
		ifp->if_height = fbp->if_height;
	    if (fbp->if_max_width < ifp->if_max_width)
		ifp->if_max_width = fbp->if_max_width;
	    if (fbp->if_max_height < ifp->if_max_height)
		ifp->if_max_height = fbp->if_max_height;
	    SI(ifp)->if_list[i++] = fbp;
	}
    }
    if (i > 0)
	return 0;
    else
	return -1;
}

HIDDEN struct fb_platform_specific *
stk_get_fbps(uint32_t UNUSED(magic))
{
        return NULL;
}


HIDDEN void
stk_put_fbps(struct fb_platform_specific *UNUSED(fbps))
{
        return;
}

HIDDEN int
stk_open_existing(fb *UNUSED(ifp), int UNUSED(width), int UNUSED(height), struct fb_platform_specific *UNUSED(fb_p))
{
        return 0;
}


HIDDEN int
stk_close_existing(fb *UNUSED(ifp))
{
        return 0;
}

HIDDEN int
stk_configure_window(fb *UNUSED(ifp), int UNUSED(width), int UNUSED(height))
{
        return 0;
}

HIDDEN int
stk_refresh(fb *UNUSED(ifp), int UNUSED(x), int UNUSED(y), int UNUSED(w), int UNUSED(h))
{
        return 0;
}


HIDDEN int
stk_close(fb *ifp)
{
    register fb **ip = SI(ifp)->if_list;

    FB_CK_FB(ifp);
    while (*ip != (fb *)NULL) {
	FB_CK_FB((*ip));
	fb_close((*ip));
	ip++;
    }

    return 0;
}


HIDDEN int
stk_clear(fb *ifp, unsigned char *pp)
{
    register fb **ip = SI(ifp)->if_list;

    while (*ip != (fb *)NULL) {
	fb_clear((*ip), pp);
	ip++;
    }

    return 0;
}


HIDDEN ssize_t
stk_read(fb *ifp, int x, int y, unsigned char *pixelp, size_t count)
{
    register fb **ip = SI(ifp)->if_list;

    if (*ip != (fb *)NULL) {
	fb_read((*ip), x, y, pixelp, count);
    }

    return count;
}


HIDDEN ssize_t
stk_write(fb *ifp, int x, int y, const unsigned char *pixelp, size_t count)
{
    register fb **ip = SI(ifp)->if_list;

    while (*ip != (fb *)NULL) {
	fb_write((*ip), x, y, pixelp, count);
	ip++;
    }

    return count;
}


/*
 * Read only from the first source on the stack.
 */
HIDDEN int
stk_readrect(fb *ifp, int xmin, int ymin, int width, int height, unsigned char *pp)
{
    register fb **ip = SI(ifp)->if_list;

    if (*ip != (fb *)NULL) {
	(void)fb_readrect((*ip), xmin, ymin, width, height, pp);
    }

    return width*height;
}


/*
 * Write to all destinations on the stack
 */
HIDDEN int
stk_writerect(fb *ifp, int xmin, int ymin, int width, int height, const unsigned char *pp)
{
    register fb **ip = SI(ifp)->if_list;

    while (*ip != (fb *)NULL) {
	(void)fb_writerect((*ip), xmin, ymin, width, height, pp);
	ip++;
    }

    return width*height;
}


/*
 * Read only from the first source on the stack.
 */
HIDDEN int
stk_bwreadrect(fb *ifp, int xmin, int ymin, int width, int height, unsigned char *pp)
{
    register fb **ip = SI(ifp)->if_list;

    if (*ip != (fb *)NULL) {
	(void)fb_bwreadrect((*ip), xmin, ymin, width, height, pp);
    }

    return width*height;
}


/*
 * Write to all destinations on the stack
 */
HIDDEN int
stk_bwwriterect(fb *ifp, int xmin, int ymin, int width, int height, const unsigned char *pp)
{
    register fb **ip = SI(ifp)->if_list;

    while (*ip != (fb *)NULL) {
	(void)fb_bwwriterect((*ip), xmin, ymin, width, height, pp);
	ip++;
    }

    return width*height;
}


HIDDEN int
stk_rmap(fb *ifp, ColorMap *cmp)
{
    register fb **ip = SI(ifp)->if_list;

    if (*ip != (fb *)NULL) {
	fb_rmap((*ip), cmp);
    }

    return 0;
}


HIDDEN int
stk_wmap(fb *ifp, const ColorMap *cmp)
{
    register fb **ip = SI(ifp)->if_list;

    while (*ip != (fb *)NULL) {
	fb_wmap((*ip), cmp);
	ip++;
    }

    return 0;
}


HIDDEN int
stk_view(fb *ifp, int xcenter, int ycenter, int xzoom, int yzoom)
{
    register fb **ip = SI(ifp)->if_list;

    while (*ip != (fb *)NULL) {
	fb_view((*ip), xcenter, ycenter, xzoom, yzoom);
	ip++;
    }

    return 0;
}


HIDDEN int
stk_getview(fb *ifp, int *xcenter, int *ycenter, int *xzoom, int *yzoom)
{
    register fb **ip = SI(ifp)->if_list;

    if (*ip != (fb *)NULL) {
	fb_getview((*ip), xcenter, ycenter, xzoom, yzoom);
    }

    return 0;
}


HIDDEN int
stk_setcursor(fb *ifp, const unsigned char *bits, int xbits, int ybits, int xorig, int yorig)
{
    register fb **ip = SI(ifp)->if_list;

    while (*ip != (fb *)NULL) {
	fb_setcursor((*ip), bits, xbits, ybits, xorig, yorig);
	ip++;
    }

    return 0;
}


HIDDEN int
stk_cursor(fb *ifp, int mode, int x, int y)
{
    register fb **ip = SI(ifp)->if_list;

    while (*ip != (fb *)NULL) {
	fb_cursor((*ip), mode, x, y);
	ip++;
    }

    return 0;
}


HIDDEN int
stk_getcursor(fb *ifp, int *mode, int *x, int *y)
{
    register fb **ip = SI(ifp)->if_list;

    if (*ip != (fb *)NULL) {
	fb_getcursor((*ip), mode, x, y);
    }

    return 0;
}


HIDDEN int
stk_poll(fb *ifp)
{
    register fb **ip = SI(ifp)->if_list;

    while (*ip != (fb *)NULL) {
	fb_poll((*ip));
	ip++;
    }

    return 0;
}


HIDDEN int
stk_flush(fb *ifp)
{
    register fb **ip = SI(ifp)->if_list;

    while (*ip != (fb *)NULL) {
	fb_flush((*ip));
	ip++;
    }

    return 0;
}


HIDDEN int
stk_free(fb *ifp)
{
    register fb **ip = SI(ifp)->if_list;

    while (*ip != (fb *)NULL) {
	fb_free((*ip));
	ip++;
    }

    return 0;
}


HIDDEN int
stk_help(fb *ifp)
{
    register fb **ip = SI(ifp)->if_list;
    int i;

    fb_log("Device: /dev/stack\n");
    fb_log("Usage: /dev/stack device_one; device_two; ...\n");

    i = 0;
    while (*ip != (fb *)NULL) {
	fb_log("=== Current stack device #%d ===\n", i++);
	fb_help((*ip));
	ip++;
    }

    return 0;
}


/* This is the ONLY thing that we normally "export" */
fb stk_interface =  {
    0,
    FB_STK_MAGIC,
    stk_open,		/* device_open */
    stk_open_existing,	/* device_open */
    stk_close_existing,
    stk_get_fbps,
    stk_put_fbps,
    stk_close,		/* device_close */
    stk_clear,		/* device_clear */
    stk_read,		/* buffer_read */
    stk_write,		/* buffer_write */
    stk_rmap,		/* colormap_read */
    stk_wmap,		/* colormap_write */
    stk_view,		/* set view */
    stk_getview,		/* get view */
    stk_setcursor,		/* define cursor */
    stk_cursor,		/* set cursor */
    stk_getcursor,		/* get cursor */
    stk_readrect,		/* read rectangle */
    stk_writerect,		/* write rectangle */
    stk_bwreadrect,		/* read bw rectangle */
    stk_bwwriterect,	/* write bw rectangle */
    stk_configure_window,
    stk_refresh,
    stk_poll,		/* handle events */
    stk_flush,		/* flush output */
    stk_free,		/* free resources */
    stk_help,		/* help function */
    "Multiple Device Stacker", /* device description */
    32*1024,		/* max width */
    32*1024,		/* max height */
    "/dev/stack",		/* short device name */
    4,			/* default/current width */
    4,			/* default/current height */
    -1,			/* select fd */
    -1,			/* file descriptor */
    1, 1,			/* zoom */
    2, 2,			/* window center */
    0, 0, 0,		/* cursor */
    PIXEL_NULL,		/* page_base */
    PIXEL_NULL,		/* page_curp */
    PIXEL_NULL,		/* page_endp */
    -1,			/* page_no */
    0,			/* page_dirty */
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
