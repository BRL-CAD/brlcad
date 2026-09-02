/*                       F B C L E A R . C
 * BRL-CAD
 *
 * Copyright (c) 1986-2026 United States Government as represented by
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
/** @file fbclear.c
 *
 * This program is intended to be used to clear a frame buffer
 * to black, or to the specified color
 *
 */

#include "common.h"

#include <errno.h>
#include <limits.h>
#include <stdlib.h>

#include "bu/app.h"
#include "bu/color.h"
#include "bu/getopt.h"
#include "bu/opt.h"
#include "bu/exit.h"
#include "dm.h"
#include "pkg.h"

#ifdef HAVE_WINSOCK_H
#  include <winsock.h>
#endif


static char *framebuffer = NULL;
static struct fb *fbp;
static int scr_width = 0;		/* use default size */
static int scr_height = 0;
static int clear_and_reset = 0;

#define u_char unsigned char

static char usage[] = "Usage: fbclear [-c] [-F framebuffer]\n\
	[-{sS} squarescrsize] [-{wW} scr_width] [-{nN} scr_height] [gray | r g b]\n";

int
get_args(int argc, char **argv)
{
    int c;

    while ((c = bu_getopt(argc, argv, "cF:s:w:n:S:W:N:h?")) != -1) {
	switch (c) {
	    case 'c':
		/* clear only, no cmap, pan, and zoom */
		clear_and_reset++;
		break;
	    case 'F':
		framebuffer = bu_optarg;
		break;
	    case 's':
	    case 'S':
		if (!bu_opt_scan_int_range(bu_optarg, &scr_width, 1, INT_MAX, "screen size"))
		    return 0;
		scr_height = scr_width;
		break;
	    case 'w':
	    case 'W':
		if (!bu_opt_scan_int_range(bu_optarg, &scr_width, 1, INT_MAX, "screen width"))
		    return 0;
		break;
	    case 'n':
	    case 'N':
		if (!bu_opt_scan_int_range(bu_optarg, &scr_height, 1, INT_MAX, "screen height"))
		    return 0;
		break;

	    default:		/* '?' 'h' */
		return 0;
	}
    }
    return 1;		/* OK */
}


int
main(int argc, char **argv)
{
    static RGBpixel pixel;
    int remaining = 0;
    int use_custom_pixel = 0;

    bu_setprogname(argv[0]);
    if (!get_args(argc, argv)) {
	(void)fputs(usage, stderr);
	bu_exit(1, NULL);
    }

    remaining = argc - bu_optind;
    if (remaining == 3) {
	if (!bu_opt_scan_uchar(argv[bu_optind+0], &pixel[RED], "red value")
	    || !bu_opt_scan_uchar(argv[bu_optind+1], &pixel[GRN], "green value")
	    || !bu_opt_scan_uchar(argv[bu_optind+2], &pixel[BLU], "blue value")) {
	    (void)fputs(usage, stderr);
	    bu_exit(1, NULL);
	}
	use_custom_pixel = 1;
    } else if (remaining == 1) {
	if (!bu_opt_scan_uchar(argv[bu_optind+0], &pixel[RED], "gray value")) {
	    (void)fputs(usage, stderr);
	    bu_exit(1, NULL);
	}
	pixel[GRN] = pixel[BLU] = pixel[RED];
	use_custom_pixel = 1;
    } else if (remaining != 0) {
	fprintf(stderr, "fbclear: expected 0, 1, or 3 color arguments, got %d\n", remaining);
	(void)fputs(usage, stderr);
	bu_exit(1, NULL);
    }

    if ((fbp = fb_open(framebuffer, scr_width, scr_height)) == NULL) {
	bu_exit(2, NULL);
    }

    /* Get the screen size we were given */
    scr_width = fb_getwidth(fbp);
    scr_height = fb_getheight(fbp);

    if (clear_and_reset) {
	if (fb_wmap(fbp, COLORMAP_NULL) < 0)
	    bu_exit(3, NULL);
	(void)fb_view(fbp, scr_width/2, scr_height/2, 1, 1);
    } else {
	ColorMap cmap;
	int xcent, ycent, xzoom, yzoom;
	if (fb_rmap(fbp, &cmap) >= 0) {
	    if (!fb_is_linear_cmap(&cmap)) {
		fprintf(stderr, "fbclear: NOTE: non-linear colormap in effect.  -c flag loads linear colormap.\n");
	    }
	}
	(void)fb_getview(fbp, &xcent, &ycent, &xzoom, &yzoom);
	if (xzoom != 1 || yzoom != 1) {
	    fprintf(stderr, "fbclear:  NOTE: framebuffer is zoomed.  -c will un-zoom.\n");
	}
    }

    if (use_custom_pixel) {
	fb_clear(fbp, pixel);
    } else {
	fb_clear(fbp, PIXEL_NULL);
    }
    (void)fb_close(fbp);
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
