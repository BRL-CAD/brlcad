/*                   F B _ P A G E D _ I O . C
 * BRL-CAD
 *
 * Copyright (c) 1986-2022 United States Government as represented by
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
/** @file fb_paged_io.c
 *
 * Buffered frame buffer IO routines.
 *
 */
/** @} */

#include "common.h"

#include <stdlib.h>
#include <stdio.h>

#include "bu/color.h"
#include "bu/log.h"
#include "./include/private.h"
#include "dm.h"


#define PAGE_BYTES (63*1024L)		/* Max # of bytes/dma. */
#define PAGE_PIXELS (((PAGE_BYTES/sizeof(RGBpixel))/ifp->i->if_width) *ifp->i->if_width)
#define PAGE_SCANS (ifp->i->if_ppixels/ifp->i->if_width)

#define Malloc_Bomb(_bytes_)					\
    fb_log("\"%s\"(%d) : allocation of %lu bytes failed.\n",	\
	   __FILE__, __LINE__, _bytes_)


static int
_fb_pgout(register struct fb *ifp)
{
    size_t scans, first_scan;

    /*fb_log("_fb_pgout(%d)\n", ifp->i->if_pno);*/

    if (ifp->i->if_pno < 0)	/* Already paged out, return 1.	*/
	return 1;

    first_scan = ifp->i->if_pno * PAGE_SCANS;
    if (first_scan + PAGE_SCANS > (size_t)ifp->i->if_height)
	scans = ifp->i->if_height - first_scan;
    else
	scans = PAGE_SCANS;

    return fb_write(ifp,
		    0,
		    first_scan,
		    ifp->i->if_pbase,
		    scans * ifp->i->if_width
	);
}


static int
_fb_pgin(register struct fb *ifp, int pageno)
{
    size_t scans, first_scan;

    /*fb_log("_fb_pgin(%d)\n", pageno);*/

    /* Set pixel pointer to beginning of page. */
    ifp->i->if_pcurp = ifp->i->if_pbase;
    ifp->i->if_pno = pageno;
    ifp->i->if_pdirty = 0;

    first_scan = ifp->i->if_pno * PAGE_SCANS;
    if (first_scan + PAGE_SCANS > (size_t)ifp->i->if_height)
	scans = ifp->i->if_height - first_scan;
    else
	scans = PAGE_SCANS;

    return fb_read(ifp,
		   0,
		   first_scan,
		   ifp->i->if_pbase,
		   scans * ifp->i->if_width
	);
}


static int
_fb_pgflush(register struct fb *ifp)
{
    if (ifp->i->if_debug & FB_DEBUG_BIO) {
	fb_log("_fb_pgflush(%p)\n", (void *)ifp);
    }

    if (ifp->i->if_pdirty) {
	if (_fb_pgout(ifp) <= -1)
	    return -1;
	ifp->i->if_pdirty = 0;
    }

    return 0;
}


/**
 * This initialization routine must be called before any buffered I/O
 * routines in this file are used.
 */
int
fb_ioinit(register struct fb *ifp)
{
    if (ifp->i->if_debug & FB_DEBUG_BIO) {
	fb_log("fb_ioinit(%p)\n", (void *)ifp);
    }

    ifp->i->if_pno = -1;		/* Force _fb_pgin() initially.	*/
    ifp->i->if_pixcur = 0L;		/* Initialize pixel number.	*/
    if (ifp->i->if_pbase == PIXEL_NULL) {
	/* Only allocate buffer once. */
	ifp->i->if_ppixels = PAGE_PIXELS;	/* Pixels/page.	*/
	if (ifp->i->if_ppixels > ifp->i->if_width * ifp->i->if_height)
	    ifp->i->if_ppixels = ifp->i->if_width * ifp->i->if_height;
	if ((ifp->i->if_pbase = (unsigned char *)malloc(ifp->i->if_ppixels * sizeof(RGBpixel)))
	    == PIXEL_NULL) {
	    Malloc_Bomb(ifp->i->if_ppixels * sizeof(RGBpixel));
	    return -1;
	}
    }
    ifp->i->if_pcurp = ifp->i->if_pbase;	/* Initialize pointer.	*/
    ifp->i->if_pendp = ifp->i->if_pbase+ifp->i->if_ppixels*sizeof(RGBpixel);
    return 0;
}


int
fb_seek(register struct fb *ifp, int x, int y)
{
    long pixelnum;
    long pagepixel;

    if (ifp->i->if_debug & FB_DEBUG_BIO) {
	fb_log("fb_seek(%p, %d, %d)\n",
	       (void *)ifp, x, y);
    }

    if (x < 0 || y < 0 || x >= ifp->i->if_width || y >= ifp->i->if_height) {
	fb_log("fb_seek: illegal address <%d, %d>.\n", x, y);
	return -1;
    }
    pixelnum = ((long) y * (long) ifp->i->if_width) + x;
    pagepixel = (long) ifp->i->if_pno * ifp->i->if_ppixels;
    if (pixelnum < pagepixel || pixelnum >= (pagepixel + ifp->i->if_ppixels)) {
	if (ifp->i->if_pdirty)
	    if (_fb_pgout(ifp) == - 1)
		return -1;
	if (_fb_pgin(ifp, (int) (pixelnum / (long) ifp->i->if_ppixels)) <= -1)
	    return -1;
    }
    ifp->i->if_pixcur = pixelnum;
    /* Compute new pixel pointer into page. */
    ifp->i->if_pcurp = ifp->i->if_pbase +
	(ifp->i->if_pixcur % ifp->i->if_ppixels)*sizeof(RGBpixel);
    return 0;
}


int
fb_tell(register struct fb *ifp, int *xp, int *yp)
{
    *yp = (int) (ifp->i->if_pixcur / ifp->i->if_width);
    *xp = (int) (ifp->i->if_pixcur % ifp->i->if_width);

    if (ifp->i->if_debug & FB_DEBUG_BIO) {
	fb_log("fb_tell(%p, %p, %p) => (%4d, %4d)\n",
	       (void *)ifp, (void *)xp, (void *)yp, *xp, *yp);
    }

    return 0;
}


int
fb_wpixel(register struct fb *ifp, unsigned char *pixelp)
{
    if (ifp->i->if_pno == -1)
	if (_fb_pgin(ifp, ifp->i->if_pixcur / ifp->i->if_ppixels) <= -1)
	    return -1;

    COPYRGB((ifp->i->if_pcurp), pixelp);
    ifp->i->if_pcurp += sizeof(RGBpixel);	/* position in page */
    ifp->i->if_pixcur++;	/* position in framebuffer */
    ifp->i->if_pdirty = 1;	/* page referenced (dirty) */
    if (ifp->i->if_pcurp >= ifp->i->if_pendp) {
	if (_fb_pgout(ifp) <= -1)
	    return -1;
	ifp->i->if_pno = -1;
    }
    return 0;
}


int
fb_rpixel(register struct fb *ifp, unsigned char *pixelp)
{
    if (ifp->i->if_pno == -1)
	if (_fb_pgin(ifp, ifp->i->if_pixcur / ifp->i->if_ppixels) <= -1)
	    return -1;

    COPYRGB(pixelp, (ifp->i->if_pcurp));
    ifp->i->if_pcurp += sizeof(RGBpixel);	/* position in page */
    ifp->i->if_pixcur++;	/* position in framebuffer */
    if (ifp->i->if_pcurp >= ifp->i->if_pendp) {
	if (_fb_pgout(ifp) <= -1)
	    return -1;
	ifp->i->if_pno = -1;
    }

    return 0;
}


int
fb_flush(register struct fb *ifp)
{
    _fb_pgflush(ifp);

    /* call device specific flush routine */
    if ((*ifp->i->if_flush)(ifp) <= -1)
	return -1;

    return 0;
}


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
