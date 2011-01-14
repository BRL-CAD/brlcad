/*                   F B _ P A G E D _ I O . C
 * BRL-CAD
 *
 * Copyright (c) 1986-2011 United States Government as represented by
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
/** @addtogroup fb */
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

#include "fb.h"


#define PAGE_BYTES (63*1024L)		/* Max # of bytes/dma. */
#define PAGE_PIXELS (((PAGE_BYTES/sizeof(RGBpixel))/ifp->if_width) *ifp->if_width)
#define PAGE_SCANS (ifp->if_ppixels/ifp->if_width)

#define Malloc_Bomb(_bytes_)					\
    fb_log("\"%s\"(%d) : allocation of %d bytes failed.\n",	\
	   __FILE__, __LINE__, _bytes_)


static int
_fb_pgout(register FBIO *ifp)
{
    size_t scans, first_scan;

    /*fb_log("_fb_pgout(%d)\n", ifp->if_pno);*/

    if (ifp->if_pno < 0)	/* Already paged out, return 1.	*/
	return 1;

    first_scan = ifp->if_pno * PAGE_SCANS;
    if (first_scan + PAGE_SCANS > (size_t)ifp->if_height)
	scans = ifp->if_height - first_scan;
    else
	scans = PAGE_SCANS;

    return fb_write(ifp,
		    0,
		    first_scan,
		    ifp->if_pbase,
		    scans * ifp->if_width
	);
}


static int
_fb_pgin(register FBIO *ifp, int pageno)
{
    size_t scans, first_scan;

    /*fb_log("_fb_pgin(%d)\n", pageno);*/

    /* Set pixel pointer to beginning of page. */
    ifp->if_pcurp = ifp->if_pbase;
    ifp->if_pno = pageno;
    ifp->if_pdirty = 0;

    first_scan = ifp->if_pno * PAGE_SCANS;
    if (first_scan + PAGE_SCANS > (size_t)ifp->if_height)
	scans = ifp->if_height - first_scan;
    else
	scans = PAGE_SCANS;

    return fb_read(ifp,
		   0,
		   first_scan,
		   ifp->if_pbase,
		   scans * ifp->if_width
	);
}


static int
_fb_pgflush(register FBIO *ifp)
{
    if (ifp->if_debug & FB_DEBUG_BIO) {
	fb_log("_fb_pgflush(%p)\n", (void *)ifp);
    }

    if (ifp->if_pdirty) {
	if (_fb_pgout(ifp) <= -1)
	    return -1;
	ifp->if_pdirty = 0;
    }

    return 0;
}


/**
 * F B _ I O I N I T
 *
 * This initialization routine must be called before any buffered I/O
 * routines in this file are used.
 */
int
fb_ioinit(register FBIO *ifp)
{
    if (ifp->if_debug & FB_DEBUG_BIO) {
	fb_log("fb_ioinit(%p)\n", (void *)ifp);
    }

    ifp->if_pno = -1;		/* Force _fb_pgin() initially.	*/
    ifp->if_pixcur = 0L;		/* Initialize pixel number.	*/
    if (ifp->if_pbase == PIXEL_NULL) {
	/* Only allocate buffer once. */
	ifp->if_ppixels = PAGE_PIXELS;	/* Pixels/page.	*/
	if (ifp->if_ppixels > ifp->if_width * ifp->if_height)
	    ifp->if_ppixels = ifp->if_width * ifp->if_height;
	if ((ifp->if_pbase = (unsigned char *)malloc(ifp->if_ppixels * sizeof(RGBpixel)))
	    == PIXEL_NULL) {
	    Malloc_Bomb(ifp->if_ppixels * sizeof(RGBpixel));
	    return -1;
	}
    }
    ifp->if_pcurp = ifp->if_pbase;	/* Initialize pointer.	*/
    ifp->if_pendp = ifp->if_pbase+ifp->if_ppixels*sizeof(RGBpixel);
    return 0;
}


int
fb_seek(register FBIO *ifp, int x, int y)
{
    long pixelnum;
    long pagepixel;

    if (ifp->if_debug & FB_DEBUG_BIO) {
	fb_log("fb_seek(%p, %d, %d)\n",
	       (void *)ifp, x, y);
    }

    if (x < 0 || y < 0 || x >= ifp->if_width || y >= ifp->if_height) {
	fb_log("fb_seek: illegal address <%d, %d>.\n", x, y);
	return -1;
    }
    pixelnum = ((long) y * (long) ifp->if_width) + x;
    pagepixel = (long) ifp->if_pno * ifp->if_ppixels;
    if (pixelnum < pagepixel || pixelnum >= (pagepixel + ifp->if_ppixels)) {
	if (ifp->if_pdirty)
	    if (_fb_pgout(ifp) == - 1)
		return -1;
	if (_fb_pgin(ifp, (int) (pixelnum / (long) ifp->if_ppixels)) <= -1)
	    return -1;
    }
    ifp->if_pixcur = pixelnum;
    /* Compute new pixel pointer into page. */
    ifp->if_pcurp = ifp->if_pbase +
	(ifp->if_pixcur % ifp->if_ppixels)*sizeof(RGBpixel);
    return 0;
}


int
fb_tell(register FBIO *ifp, int *xp, int *yp)
{
    *yp = (int) (ifp->if_pixcur / ifp->if_width);
    *xp = (int) (ifp->if_pixcur % ifp->if_width);

    if (ifp->if_debug & FB_DEBUG_BIO) {
	fb_log("fb_tell(%p, 0x%x, 0x%x) => (%4d, %4d)\n",
	       (void *)ifp, xp, yp, *xp, *yp);
    }

    return 0;
}


int
fb_wpixel(register FBIO *ifp, unsigned char *pixelp)
{
    if (ifp->if_pno == -1)
	if (_fb_pgin(ifp, ifp->if_pixcur / ifp->if_ppixels) <= -1)
	    return -1;

    COPYRGB((ifp->if_pcurp), pixelp);
    ifp->if_pcurp += sizeof(RGBpixel);	/* position in page */
    ifp->if_pixcur++;	/* position in framebuffer */
    ifp->if_pdirty = 1;	/* page referenced (dirty) */
    if (ifp->if_pcurp >= ifp->if_pendp) {
	if (_fb_pgout(ifp) <= -1)
	    return -1;
	ifp->if_pno = -1;
    }
    return 0;
}


int
fb_rpixel(register FBIO *ifp, unsigned char *pixelp)
{
    if (ifp->if_pno == -1)
	if (_fb_pgin(ifp, ifp->if_pixcur / ifp->if_ppixels) <= -1)
	    return -1;

    COPYRGB(pixelp, (ifp->if_pcurp));
    ifp->if_pcurp += sizeof(RGBpixel);	/* position in page */
    ifp->if_pixcur++;	/* position in framebuffer */
    if (ifp->if_pcurp >= ifp->if_pendp) {
	if (_fb_pgout(ifp) <= -1)
	    return -1;
	ifp->if_pno = -1;
    }

    return 0;
}


int
fb_flush(register FBIO *ifp)
{
    _fb_pgflush(ifp);

    /* call device specific flush routine */
    if ((*ifp->if_flush)(ifp) <= -1)
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
