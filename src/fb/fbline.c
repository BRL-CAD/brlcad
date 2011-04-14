/*                        F B L I N E . C
 * BRL-CAD
 *
 * Copyright (c) 1988-2011 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 *
 */
/** @file fbline.c
 *
 * Draw a single 2-D line on the framebuffer, in a given color
 *
 * Based rather heavily on plot-fb.c
 *
 */

#include "common.h"

#include <stdlib.h>
#include <ctype.h>
#include "bio.h"

#include "bu.h"
#include "fb.h"
#include "pkg.h"


struct coords {
    short x;
    short y;
}; 		/* Cartesian coordinates */

struct stroke {
    struct stroke *next;	/* next in list, or NULL */
    struct coords pixel;	/* starting scan, nib */
    short xsign;		/* 0 or +1 */
    short ysign;		/* -1, 0, or +1 */
    int ymajor; 		/* true iff Y is major dir. */
#undef major
#undef minor
    short major;		/* major dir delta (nonneg) */
    short minor;		/* minor dir delta (nonneg) */
    short e;			/* DDA error accumulator */
    short de;			/* increment for `e' */
}; /* rasterization descriptor */

static char *framebuffer = NULL;
FBIO *fbp;			/* Current framebuffer */

static int screen_width = 512;	/* default input width */
static int screen_height = 512;	/* default input height */
static int clear = 0;

RGBpixel pixcolor = { 255, 255, 255 };

static int fbx1, fby1, fbx2, fby2;


static char usage[] = "\
Usage: fbline [-h -c ] [-F framebuffer]\n\
	[-W screen_width] [-N screen_height]\n\
	[-r red] [-g green] [-b blue] x1 y1 x2 y2\n";


/*
 * E D G E L I M I T
 *
 * Limit generated positions to edges of screen
 * Really should clip to screen, instead.
 */
void
edgelimit(struct coords *ppos)
{
    if (ppos->x >= screen_width)
	ppos->x = screen_width -1;

    if (ppos->y >= screen_height)
	ppos->y = screen_height -1;
}


/*
 * R A S T E R
 *
 * Raster - rasterize stroke.
 *
 * Method:
 * Modified Bresenham algorithm.  Guaranteed to mark a dot for
 * a zero-length stroke.
 */
void
Raster(struct stroke *vp)
{
    short dy;		/* raster within active band */

    /*
     * Write the color of this vector on all pixels.
     */
    for (dy = vp->pixel.y; dy < screen_height;) {

	/* set the appropriate pixel in the buffer */
	fb_write(fbp, vp->pixel.x, dy, pixcolor, 1);

	if (vp->major-- == 0)
	    return;		/* Done */

	if (vp->e < 0) {
	    /* advance major & minor */
	    dy += vp->ysign;
	    vp->pixel.x += vp->xsign;
	    vp->e += vp->de;
	} else {
	    /* advance major only */
	    if (vp->ymajor)	/* Y is major dir */
		++dy;
	    else			/* X is major dir */
		vp->pixel.x += vp->xsign;
	    vp->e -= vp->minor;
	}
    }
    fprintf(stderr, "line left screen?\n");
}


/*
 * B U I L D S T R
 *
 * set up DDA parameters
 */
void
BuildStr(struct coords *pt1, struct coords *pt2)
{
    struct stroke cur_stroke;
    struct stroke *vp = &cur_stroke;

    /* arrange for pt1 to have the smaller Y-coordinate: */
    if (pt1->y > pt2->y) {
	struct coords *temp;	/* temporary for swap */

	temp = pt1;		/* swap pointers */
	pt1 = pt2;
	pt2 = temp;
    }

    /* set up DDA parameters for stroke */
    vp->pixel = *pt1;		/* initial pixel */
    vp->major = pt2->y - vp->pixel.y;	/* always nonnegative */
    vp->ysign = vp->major ? 1 : 0;
    vp->minor = pt2->x - vp->pixel.x;
    if ((vp->xsign = vp->minor ? (vp->minor > 0 ? 1 : -1) : 0) < 0)
	vp->minor = -vp->minor;

    /* if Y is not really major, correct the assignments */
    if (!(vp->ymajor = vp->minor <= vp->major)) {
	short temp;	/* temporary for swap */

	temp = vp->minor;
	vp->minor = vp->major;
	vp->major = temp;
    }

    vp->e = vp->major / 2 - vp->minor;	/* initial DDA error */
    vp->de = vp->major - vp->minor;

    Raster(vp);
}


/*
 * G E T_ A R G S
 */
int
get_args(int argc, char **argv)
{

    int c;

    while ((c = bu_getopt(argc, argv, "hW:w:N:n:cF:r:g:b:")) != -1) {
	switch (c) {
	    case 'h':
		/* high-res */
		screen_height = screen_width = 1024;
		break;
	    case 'W':
	    case 'w':
		screen_width = atoi(bu_optarg);
		break;
	    case 'N':
	    case 'n':
		screen_height = atoi(bu_optarg);
		break;
	    case 'c':
		clear = 1;
		break;
	    case 'F':
		framebuffer = bu_optarg;
		break;
	    case 'r':
		pixcolor[RED] = atoi(bu_optarg);
		break;
	    case 'g':
		pixcolor[GRN] = atoi(bu_optarg);
		break;
	    case 'b':
		pixcolor[BLU] = atoi(bu_optarg);
		break;
	    default:		/* '?' */
		return 0;
	}
    }

    if (bu_optind+4 > argc)
	return 0;		/* BAD */
    fbx1 = atoi(argv[bu_optind++]);
    fby1 = atoi(argv[bu_optind++]);
    fbx2 = atoi(argv[bu_optind++]);
    fby2 = atoi(argv[bu_optind++]);

    if (argc > bu_optind)
	(void)fprintf(stderr, "fbline: excess argument(s) ignored\n");

    return 1;		/* OK */
}


/*
 * M A I N
 */
int
main(int argc, char **argv)
{
    struct coords start, end;

    if (!get_args(argc, argv)) {
	fputs(usage, stderr);
	bu_exit(1, NULL);
    }

    if ((fbp = fb_open(framebuffer, screen_width, screen_height)) == NULL)
	bu_exit(12, NULL);

    if (clear) {
	fb_clear(fbp, PIXEL_NULL);
    }
    screen_width = fb_getwidth(fbp);
    screen_height = fb_getheight(fbp);

    start.x = fbx1;
    start.y = fby1;
    end.x = fbx2;
    end.y = fby2;
    edgelimit(&start);
    edgelimit(&end);
    BuildStr(&start, &end);	/* pixels */

    fb_close(fbp);
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
