/*                         D D I S P . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2013 United States Government as represented by
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
 */
/** @file ddisp.c
 *
 * Data Display - shows doubles on a framebuffer in various ways.
 *
 */

#include "common.h"

#include <string.h>
#include <stdlib.h>
#include "bio.h"

#include "fb.h"

#define MAXPTS 4096

int Clear = 0;
int pause_time = 0;
int mode = 0;
#define VERT 1
#define BARS 2

FBIO *fbp;
int fbsize = 512;


static void
lineout(double *dat, int n)
{
    static int y = 0;
    int i, value;
    RGBpixel lbuf[1024*4];

    if (n > fb_getwidth(fbp)) n = fb_getwidth(fbp);

    for (i = 0; i < n; i++) {
	/* Magnitude version */
	value = dat[i] * 255.9;
	if (value < 0) value = 0;
	else if (value > 255) value = 255;
	lbuf[i][RED] = lbuf[i][GRN] = lbuf[i][BLU] = value;
    }
    fb_write(fbp, 0, y, (unsigned char *)lbuf, n);

    /* Next screen position */
    y = (y + 1) % fb_getheight(fbp);
}


/*
 * Display doubles.
 * +/- 1.0 in, becomes +/- 128 from center Y.
 */
static void
disp_inten(double *buf, int size)
{
    int x, y;
    RGBpixel color;

/* color.red = color.green = color.blue = 255;*/

    if (size > fb_getwidth(fbp)) size = fb_getwidth(fbp);

    for (x = 0; x < size; x++) {
	y = buf[x] * 128;
#ifdef OVERLAY
	fb_read(fbp, x, y+255, color, 1);
#else
	color[RED] = color[BLU] = 0;
#endif
	color[GRN] = 255;
	fb_write(fbp, x, y+255, color, 1);
    }
}


/*
 * Display doubles.
 * +/- 1.0 in, becomes +/- 128 from center Y.
 */
static void
disp_bars(double *buf, int size)
{
    int x, y;
    RGBpixel color;

/* color.red = color.green = color.blue = 255;*/

    if (size > fb_getwidth(fbp)) size = fb_getwidth(fbp);

    for (x = 0; x < size; x++) {
	if (buf[x] > 1.0) {
	    y = 128;
	} else if (buf[x] < -1.0) {
	    y = -128;
	} else {
	    y = buf[x] * 128;
	}
#ifdef OVERLAY
	fb_read(fbp, x, y+255, color, 1);
#else
	color[RED] = color[BLU] = 0;
#endif
	color[GRN] = 255;
	if (y > 0) {
	    while (y >= 0) {
		fb_write(fbp, x, y+255, color, 1);
		y--;
	    }
	} else {
	    while (y <= 0) {
		fb_write(fbp, x, y+255, color, 1);
		y++;
	    }
	}
    }
}


int
main(int argc, char **argv)
{
    static const char usage[] = "Usage: ddisp [-v -b -p -c -H] [width (512)] < inputfile\n";

    double buf[MAXPTS];

    int n, L;

    if (isatty(fileno(stdin))) {
	bu_exit(1, "%s", usage);
    }

    while (argc > 1) {
	if (BU_STR_EQUAL(argv[1], "-v")) {
	    mode = VERT;
	    pause_time = 0;
	    Clear = 0;
	} else if (BU_STR_EQUAL(argv[1], "-b")) {
	    mode = BARS;
	} else if (BU_STR_EQUAL(argv[1], "-p")) {
	    pause_time = 3;
	} else if (BU_STR_EQUAL(argv[1], "-c")) {
	    Clear++;
	} else if (BU_STR_EQUAL(argv[1], "-H")) {
	    fbsize = 1024;
	    bu_exit(1, "%s", usage);
	} else {
	    if (! BU_STR_EQUAL(argv[1], "-h") && ! BU_STR_EQUAL(argv[1], "-?"))
		fprintf(stderr, "Illegal option -- %s\n", argv[1]);
	    bu_exit(1, "%s", usage);
	}
	argc--;
	argv++;
    }

    if ((fbp = fb_open(NULL, fbsize, fbsize)) == FBIO_NULL) {
	bu_exit(2, "Unable to open framebuffer\n");
    }

    L = (argc > 1) ? atoi(argv[1]) : 512;

    while ((n = fread(buf, sizeof(*buf), L, stdin)) > 0) {
	/* XXX - width hack */
	if (n > fb_getwidth(fbp))
	    n = fb_getwidth(fbp);

	if (Clear)
	    fb_clear(fbp, PIXEL_NULL);
	if (mode == VERT)
	    disp_inten(buf, n);
	else if (mode == BARS)
	    disp_bars(buf, n);
	else
	    lineout(buf, n);
	if (pause_time)
	    sleep(pause_time);
    }
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
