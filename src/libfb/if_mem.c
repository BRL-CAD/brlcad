/*                        I F _ M E M . C
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
/** @file if_mem.c
 *
 * A Memory (virtual) Frame Buffer Interface.
 *
 */
/** @} */

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>

#include "fb.h"


/* Per connection private info */
struct meminfo {
    FBIO *fbp;		/* attached frame buffer (if any) */
    unsigned char *mem;	/* memory frame buffer */
    ColorMap cmap;		/* color map buffer */
    int mem_dirty;	/* !0 implies unflushed written data */
    int cmap_dirty;	/* !0 implies unflushed written cmap */
    int write_thru;	/* !0 implies pass-thru write mode */
};
#define MI(ptr) ((struct meminfo *)((ptr)->u1.p))
#define MIL(ptr) ((ptr)->u1.p)		/* left hand side version */

#define MODE_1MASK	(1<<1)
#define MODE_1BUFFERED	(0<<1)		/* output flushed only at close */
#define MODE_1IMMEDIATE	(1<<1)		/* pass-through writes */

#define MODE_2MASK	(1<<2)
#define MODE_2CLEAR	(0<<2)		/* assume fb opens clear */
#define MODE_2PREREAD	(1<<2)		/* pre-read data from fb */

static struct modeflags {
    char c;
    long mask;
    long value;
    char *help;
} modeflags[] = {
    { 'w',	MODE_1MASK, MODE_1IMMEDIATE,
      "Write thru mode - pass writes directly to attached frame buffer" },
    { 'r',  MODE_2MASK, MODE_2PREREAD,
      "Pre-Read attached frame buffer data - else assumes clear" },
    { '\0', 0, 0, "" }
};


HIDDEN int
mem_open(FBIO *ifp, char *file, int width, int height)
{
    int mode;
    char *cp;
    FBIO *fbp;

    FB_CK_FBIO(ifp);

    /*
     * First, attempt to determine operating mode for this open,
     * based upon the "unit number" or flags.
     * file = "/dev/mem###"
     * The default mode is zero.
     */
    mode = 0;

    if (file != NULL) {
	char modebuf[80];
	char *mp;
	int alpha;
	struct modeflags *mfp;

	if (strncmp(file, "/dev/mem", 8)) {
	    /* How did this happen?? */
	    mode = 0;
	} else {
	    /* Parse the options */
	    alpha = 0;
	    mp = &modebuf[0];
	    cp = &file[8];
	    while (*cp != '\0' && !isspace(*cp)) {
		*mp++ = *cp;	/* copy it to buffer */
		if (isdigit(*cp)) {
		    cp++;
		    continue;
		}
		alpha++;
		for (mfp = modeflags; mfp->c != '\0'; mfp++) {
		    if (mfp->c == *cp) {
			mode = (mode&~mfp->mask)|mfp->value;
			break;
		    }
		}
		if (mfp->c == '\0' && *cp != '-') {
		    fb_log("if_mem: unknown option '%c' ignored\n", *cp);
		}
		cp++;
	    }
	    *mp = '\0';
	    if (!alpha)
		mode = atoi(modebuf);
	}
    }

    /* build a local static info struct */
    if ((MIL(ifp) = (char *)calloc(1, sizeof(struct meminfo))) == NULL) {
	fb_log("mem_open:  meminfo malloc failed\n");
	return -1;
    }
    cp = &file[strlen("/dev/mem")];
    while (*cp != '\0' && *cp != ' ' && *cp != '\t')
	cp++;	/* skip suffix */
    while (*cp != '\0' && (*cp == ' ' || *cp == '\t' || *cp == ';'))
	cp++;	/* skip blanks and separators */

    if (*cp) {
	/* frame buffer device specified */
	if ((fbp = fb_open(cp, width, height)) == FBIO_NULL) {
	    free(MIL(ifp));
	    return -1;
	}
	MI(ifp)->fbp = fbp;
	ifp->if_width = fbp->if_width;
	ifp->if_height = fbp->if_height;
	ifp->if_selfd = fbp->if_selfd;
	if ((mode & MODE_1MASK) == MODE_1IMMEDIATE)
	    MI(ifp)->write_thru = 1;
    } else {
	/* no frame buffer specified */
	if (width > 0)
	    ifp->if_width = width;
	if (height > 0)
	    ifp->if_height = height;
    }
    if ((MI(ifp)->mem = (unsigned char *)calloc(ifp->if_width*ifp->if_height, 3)) == NULL) {
	fb_log("mem_open:  memory buffer malloc failed\n");
	(void)free(MIL(ifp));
	return -1;
    }
    if ((MI(ifp)->fbp != FBIO_NULL)
	&& (mode & MODE_2MASK) == MODE_2PREREAD) {
	/* Pre read all of the image data and cmap */
	int got;
	got = fb_readrect(MI(ifp)->fbp, 0, 0,
			  ifp->if_width, ifp->if_height,
			  (unsigned char *)MI(ifp)->mem);
	if (got != ifp->if_width * ifp->if_height) {
	    fb_log("if_mem:  WARNING: pre-read of %d only got %d, your image is truncated.\n",
		   ifp->if_width * ifp->if_height, got);
	}
	if (fb_rmap(MI(ifp)->fbp, &(MI(ifp)->cmap)) < 0)
	    fb_make_linear_cmap(&(MI(ifp)->cmap));
    } else {
	/* Image data begins black, colormap linear */
	fb_make_linear_cmap(&(MI(ifp)->cmap));
    }

    return 0;
}


HIDDEN int
mem_close(FBIO *ifp)
{
    /*
     * Flush memory/cmap to attached frame buffer if any
     */
    if (MI(ifp)->fbp != FBIO_NULL) {
	if (MI(ifp)->cmap_dirty) {
	    fb_wmap(MI(ifp)->fbp, &(MI(ifp)->cmap));
	}
	if (MI(ifp)->mem_dirty) {
	    fb_writerect(MI(ifp)->fbp, 0, 0,
			 ifp->if_width, ifp->if_height, (unsigned char *)MI(ifp)->mem);
	}
	fb_close(MI(ifp)->fbp);
	MI(ifp)->fbp = FBIO_NULL;
    }
    (void)free((char *)MI(ifp)->mem);
    (void)free((char *)MIL(ifp));

    return 0;
}


HIDDEN int
mem_clear(FBIO *ifp, unsigned char *pp)
{
    RGBpixel v;
    register int n;
    register unsigned char *cp;

    if (pp == RGBPIXEL_NULL) {
	v[RED] = v[GRN] = v[BLU] = 0;
    } else {
	v[RED] = (pp)[RED];
	v[GRN] = (pp)[GRN];
	v[BLU] = (pp)[BLU];
    }

    cp = MI(ifp)->mem;
    if (v[RED] == v[GRN] && v[RED] == v[BLU]) {
	int bytes = ifp->if_width*ifp->if_height*3;
	if (v[RED] == 0)
	    memset((char *)cp, 0, bytes);	/* all black */
	else
	    memset(cp, v[RED], bytes);	/* all grey */
    } else {
	for (n = ifp->if_width*ifp->if_height; n; n--) {
	    *cp++ = v[RED];
	    *cp++ = v[GRN];
	    *cp++ = v[BLU];
	}
    }
    if (MI(ifp)->write_thru) {
	return fb_clear(MI(ifp)->fbp, pp);
    } else {
	MI(ifp)->mem_dirty = 1;
    }
    return 0;
}


HIDDEN int
mem_read(FBIO *ifp, int x, int y, unsigned char *pixelp, size_t count)
{
    size_t pixels_to_end;

    if (x < 0 || x >= ifp->if_width || y < 0 || y >= ifp->if_height)
	return -1;

    /* make sure we don't run off the end of the buffer */
    pixels_to_end = ifp->if_width*ifp->if_height - (y*ifp->if_width + x);
    if (pixels_to_end < count)
	count = pixels_to_end;

    memcpy((char *)pixelp, &(MI(ifp)->mem[(y*ifp->if_width + x)*3]), count*3);

    return (int)count;
}


HIDDEN int
mem_write(FBIO *ifp, int x, int y, const unsigned char *pixelp, size_t count)
{
    size_t pixels_to_end;

    if (x < 0 || x >= ifp->if_width || y < 0 || y >= ifp->if_height)
	return -1;

    /* make sure we don't run off the end of the buffer */
    pixels_to_end = ifp->if_width*ifp->if_height - (y*ifp->if_width + x);
    if (pixels_to_end < count)
	count = pixels_to_end;

    memcpy(&(MI(ifp)->mem[(y*ifp->if_width + x)*3]), (char *)pixelp, count*3);

    if (MI(ifp)->write_thru) {
	return fb_write(MI(ifp)->fbp, x, y, pixelp, count);
    } else {
	MI(ifp)->mem_dirty = 1;
    }
    return (int)count;
}


HIDDEN int
mem_rmap(FBIO *ifp, ColorMap *cmp)
{
    *cmp = MI(ifp)->cmap;		/* struct copy */
    return 0;
}


HIDDEN int
mem_wmap(FBIO *ifp, const ColorMap *cmp)
{
    if (cmp == COLORMAP_NULL) {
	fb_make_linear_cmap(&(MI(ifp)->cmap));
    } else {
	MI(ifp)->cmap = *cmp;		/* struct copy */
    }

    if (MI(ifp)->write_thru) {
	return fb_wmap(MI(ifp)->fbp, cmp);
    } else {
	MI(ifp)->cmap_dirty = 1;
    }
    return 0;
}


HIDDEN int
mem_view(FBIO *ifp, int xcenter, int ycenter, int xzoom, int yzoom)
{
    fb_sim_view(ifp, xcenter, ycenter, xzoom, yzoom);
    if (MI(ifp)->write_thru) {
	return fb_view(MI(ifp)->fbp, xcenter, ycenter,
		       xzoom, yzoom);
    }
    return 0;
}


HIDDEN int
mem_getview(FBIO *ifp, int *xcenter, int *ycenter, int *xzoom, int *yzoom)
{
    if (MI(ifp)->write_thru) {
	return fb_getview(MI(ifp)->fbp, xcenter, ycenter,
			  xzoom, yzoom);
    }
    fb_sim_getview(ifp, xcenter, ycenter, xzoom, yzoom);
    return 0;
}


HIDDEN int
mem_setcursor(FBIO *ifp, const unsigned char *bits, int xbits, int ybits, int xorig, int yorig)
{
    if (MI(ifp)->write_thru) {
	return fb_setcursor(MI(ifp)->fbp,
			    bits, xbits, ybits, xorig, yorig);
    }
    return 0;
}


HIDDEN int
mem_cursor(FBIO *ifp, int mode, int x, int y)
{
    fb_sim_cursor(ifp, mode, x, y);
    if (MI(ifp)->write_thru) {
	return fb_cursor(MI(ifp)->fbp, mode, x, y);
    }
    return 0;
}


HIDDEN int
mem_getcursor(FBIO *ifp, int *mode, int *x, int *y)
{
    if (MI(ifp)->write_thru) {
	return fb_getcursor(MI(ifp)->fbp, mode, x, y);
    }
    fb_sim_getcursor(ifp, mode, x, y);
    return 0;
}


HIDDEN int
mem_poll(FBIO *ifp)
{
    if (MI(ifp)->write_thru) {
	return fb_poll(MI(ifp)->fbp);
    }
    return 0;
}


HIDDEN int
mem_flush(FBIO *ifp)
{
    /*
     * Flush memory/cmap to attached frame buffer if any
     */
    if (MI(ifp)->fbp != FBIO_NULL) {
	if (MI(ifp)->cmap_dirty) {
	    fb_wmap(MI(ifp)->fbp, &(MI(ifp)->cmap));
	    MI(ifp)->cmap_dirty = 0;
	}
	if (MI(ifp)->mem_dirty) {
	    fb_writerect(MI(ifp)->fbp, 0, 0,
			 ifp->if_width, ifp->if_height, (unsigned char *)MI(ifp)->mem);
	    MI(ifp)->mem_dirty = 0;
	}
	return fb_flush(MI(ifp)->fbp);
    }

    MI(ifp)->cmap_dirty = 0;
    MI(ifp)->mem_dirty = 0;
    return 0;	/* success */
}


HIDDEN int
mem_help(FBIO *ifp)
{
    struct modeflags *mfp;

    fb_log("Description: %s\n", memory_interface.if_type);
    fb_log("Device: %s\n", ifp->if_name);
    fb_log("Max width/height: %d %d\n",
	   memory_interface.if_max_width,
	   memory_interface.if_max_height);
    fb_log("Default width/height: %d %d\n",
	   memory_interface.if_width,
	   memory_interface.if_height);
    fb_log("Usage: /dev/mem[options] [attached_framebuffer]\n");
    for (mfp = modeflags; mfp->c != '\0'; mfp++) {
	fb_log("   %c   %s\n", mfp->c, mfp->help);
    }
    return 0;
}


/* This is the ONLY thing that we normally "export" */
FBIO memory_interface =  {
    0,
    mem_open,		/* device_open */
    mem_close,		/* device_close */
    mem_clear,		/* device_clear */
    mem_read,		/* buffer_read */
    mem_write,		/* buffer_write */
    mem_rmap,		/* colormap_read */
    mem_wmap,		/* colormap_write */
    mem_view,		/* set view */
    mem_getview,	/* get view */
    mem_setcursor,	/* define cursor */
    mem_cursor,		/* set cursor */
    mem_getcursor,	/* get cursor */
    fb_sim_readrect,	/* rectangle read */
    fb_sim_writerect,	/* rectangle write */
    fb_sim_bwreadrect,
    fb_sim_bwwriterect,
    mem_poll,		/* poll */
    mem_flush,		/* flush */
    mem_close,		/* free */
    mem_help,		/* help message */
    "Memory Buffer",	/* device description */
    8192,		/* max width */
    8192,		/* max height */
    "/dev/mem",		/* short device name */
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
