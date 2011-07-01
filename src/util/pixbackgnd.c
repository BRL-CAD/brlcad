/*                    P I X B A C K G N D . C
 * BRL-CAD
 *
 * Copyright (c) 1986-2011 United States Government as represented by
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
/** @file util/pixbackgnd.c
 *
 * Backgound Maker
 *
 * Given Hue and Saturation for background, make top light and bottom dark.
 * Generates a pix(5) stream on stdout.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <math.h>
#include "bio.h"

#include "bu.h"
#include "vmath.h"


double RGB[3] = {128, 128, 128};	/* r, g, b */
double HSV[3] = {0, 0, 0};		/* h, s, v */

int file_width = 512;
int file_height = 512;
int invert = 0;

double deltav;
int title_height = 80;
int h_start = 240;
int h_end = 50;

char usage[] = "\
Usage:  pixbackgnd [-h -i] [-s squaresize] [-w width] [-n height]\n\
	[-t title_height] [-a top_inten] [-b bottom_inten]\n\
	hue saturation\n\
or	r g b\n\
	> file.pix";

/* rgbhsv
 *
 * convert red green blue to hue saturation value
 */
void
rgbhsv(double *rgb, double *hsv)
{
    double s, v, r, g, b, x;
    static double h;
    double dif = 0.0;

    r = rgb[0];
    g = rgb[1];
    b = rgb[2];
    v = ((r > g) ? r : g);
    v = ((v > b) ? v : b);
    x = ((r < g) ? r : g);
    x = ((x < b) ? x : b);
    if (!ZERO(v - x)) { /* v != x */
	dif = (double) (v - x);
	if (!ZERO(r - v)) /* r != v */
	    if (ZERO(g - v)) /* g == v */
		if (!ZERO(b - x)) /* b != x */
		    h = (double) (42.5 * (3. - (double)(v-b) / dif));
		else
		    h = (double) (42.5 * (1. + (double)(v-r) / dif));
	    else
		if (!ZERO(r - x)) /* r != x */
		    h = (double) (42.5 * (5. - (double)(v-r) / dif));
		else
		    h = (double) (42.5 * (3. + (double)(v-g) / dif));
	else
	    if (!ZERO(g - x)) /* g != x */
		h = (double) (42.5 * (1. - (double)(v-g) / dif));
	    else
		h = (double) (42.5 * (5. + (double)(v-b) / dif));
    }

    if (!ZERO(v))
	s = (double)(255. * dif / (double)v);
    else
	s = 0;

    hsv[0] = h;
    hsv[1] = s;
    hsv[2] = v;
}


/* hsvrgb
 *
 * convert hue saturation and value to red, green, blue
 */

void
hsvrgb(double *hsv, double *rgb)
{
    double r, g, b, m, n, k, foo;
    double h, s, v;
    double f;

    if (!ZERO(hsv[1])) {
	s = (double)hsv[1] / 255.;
	h = (double)hsv[0] / 42.666;
	f = modf(h, &foo);
	v = (double)hsv[2];
	m = (double) (v * (1. - s) + .5);
	n = (double) (v * (1. - s*f) + .5);
	k = (double) (v * (1. - (s * (1.-f))) + .5);
	switch ((int) h) {
	    case 0:
		r = hsv[2];
		g = k;
		b = m;
		break;
	    case 1:
		r = n;
		g = hsv[2];
		b = m;
		break;
	    case 2:
		r = m;
		g = hsv[2];
		b = k;
		break;
	    case 3:
		r = m;
		g = n;
		b = hsv[2];
		break;
	    case 4:
		r = k;
		g = m;
		b = hsv[2];
		break;
	    default:
	    case 5:
		r = hsv[2];
		g = m;
		b = n;
		break;
	}
    } else
	r = g = b = hsv[2];

    rgb[0] = r;
    rgb[1] = g;
    rgb[2] = b;
}


int
get_args(int argc, char **argv)
{
    int c;

    while ((c = bu_getopt(argc, argv, "his:w:n:t:a:b:")) != -1) {
	switch (c) {
	    case 'i':
		invert = 1;
		break;
	    case 'h':
		/* high-res */
		file_height = file_width = 1024;
		title_height = 90;
		break;
	    case 's':
		/* square file size */
		file_height = file_width = atoi(bu_optarg);
		break;
	    case 'w':
		file_width = atoi(bu_optarg);
		break;
	    case 'n':
		file_height = atoi(bu_optarg);
		break;
	    case 't':
		/* Title area size */
		title_height = atoi(bu_optarg);
		break;
	    case 'a':
		h_start = atoi(bu_optarg);
		break;
	    case 'b':
		h_end = atoi(bu_optarg);
		break;

	    default:		/* '?' */
		return 0;
	}
    }
    /* when bu_optind >= argc, we have run out of args */
    if (bu_optind+1 >= argc)
	return 0;		/* only 0 or 1 args */
    if (bu_optind+2 == argc) {
	/* Paramaters are H S */
	HSV[0] = atof(argv[bu_optind++]);
	HSV[1] = atof(argv[bu_optind]);
	HSV[2] = h_start;

	hsvrgb(HSV, RGB);
    } else {
	/* parameters are R G B */
	RGB[0] = atof(argv[bu_optind++]);
	RGB[1] = atof(argv[bu_optind++]);
	RGB[2] = atof(argv[bu_optind++]);

	rgbhsv(RGB, HSV);
	HSV[2] = h_start;	/* Change given RGB to starting inten */
    }
    return 1;			/* OK */
}


int
main(int argc, char **argv)
{
    int i;
    int line;
    unsigned char *horiz_buf;
    unsigned char *vert_buf;
    unsigned char *vp;
    ssize_t ret;

    if (!get_args(argc, argv) || isatty(fileno(stdout))) {
	(void)fputs(usage, stderr);
	bu_exit (1, NULL);
    }

    horiz_buf = (unsigned char *)malloc(file_width * 3);
    vert_buf = (unsigned char *)malloc(file_height * 3);

    /*
     * First stage -- prepare the vert_buf with one pixel
     * for each scanline, since each scanline has uniform color.
     * For ease of thinking about it, this is done top-to-bottom.
     */
    line = 0;
    vp = vert_buf;
    if (title_height > 0) {
	/* Make top area with initial HSV */
	for (; line<title_height; line++) {
	    *vp++ = RGB[0];
	    *vp++ = RGB[1];
	    *vp++ = RGB[2];
	}

	/* A white stripe, 4 lines high */
	for (i=0; i<4; i++, line++) {
	    *vp++ = 250;
	    *vp++ = 250;
	    *vp++ = 250;
	}
    }

    /* Do rest with V dropping from start to end values */
    if (HSV[2] > h_end) {
	/* Go from bright at the top to dim at the bottom */
	deltav = (HSV[2]-h_end) / (double)(file_height-line);

	for (; line<file_height; line++) {
	    HSV[2] -= deltav;
	    hsvrgb(HSV, RGB);
	    *vp++ = RGB[0];
	    *vp++ = RGB[1];
	    *vp++ = RGB[2];
	}
    } else {
	/* Go from dim at the top to bright at the bottom */
	deltav = (h_end-HSV[2]) / (double)(file_height-line);

	for (; line<file_height; line++) {
	    HSV[2] += deltav;
	    hsvrgb(HSV, RGB);
	    *vp++ = RGB[0];
	    *vp++ = RGB[1];
	    *vp++ = RGB[2];
	}
    }

    /*
     * Second stage -- flood each value across the horiz_buf
     * and write the scanline out.  Here we proceed bottom-to-top
     * for pix(5) format.
     */
    if (!invert) {
	for (line = file_height-1; line >= 0; line--) {
	    unsigned char *op;

	    vp = &vert_buf[line*3];
	    op = &horiz_buf[(file_width*3)-1];
	    while (op > horiz_buf) {
		*op-- = vp[2];
		*op-- = vp[1];
		*op-- = *vp;
	    }
	    ret = write(1, horiz_buf, file_width*3);
	    if (ret < 0)
		perror("write");
	}
    } else {
	/* Inverted:  top-to-bottom.  Good with cat-fb */
	for (line=0; line < file_height; line++) {
	    unsigned char *op;

	    vp = &vert_buf[line*3];
	    op = &horiz_buf[(file_width*3)-1];
	    while (op > horiz_buf) {
		*op-- = vp[2];
		*op-- = vp[1];
		*op-- = *vp;
	    }
	    ret = write(1, horiz_buf, file_width*3);
	    if (ret < 0)
		perror("write");
	}
    }

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
