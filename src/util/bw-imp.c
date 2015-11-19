/*                        B W - I M P . C
 * BRL-CAD
 *
 * Copyright (c) 1986-2014 United States Government as represented by
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
/** @file util/bw-imp.c
 *
 * Borrows heavily from Steve Hawley's & Geoffrey Cooper's "traceconv"
 * program.
 *
 * Notes - The image is printed upside down to simplify the
 * arithmetic, due to the organization of the input file.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <string.h>
#include "bio.h"

#include "bu/getopt.h"

#define true 1
#define false 0

static int halftone[8][8] =	/* halftone pattern */
{
    {6,   7,   8,   9,  10,  11,  12,  13},
    {40,  42,  44,  46,  49,  51,  54,  14},
    {37, 112, 118, 124, 130, 137,  57,  15},
    {36, 106, 208, 219, 230, 145,  60,  16},
    {34, 101, 197, 254, 243, 152,  63,  17},
    {32,  96, 187, 178, 169, 160,  66,  18},
    {30,  91,  86,  82,  78,  74,  70,  19},
    {29,  27,  26,  25,  23,  22,  21,  20}
};


static int dither[8][8] =		/* dither pattern */
{
    {6,  51,  14,  78,   8,  57,  16,  86},
    {118,  22, 178,  34, 130,  25, 197,  37},
    {18,  96,  10,  63,  20, 106,  12,  70},
    {219,  42, 145,  27, 243,  46, 160,  30},
    {9,  60,  17,  91,   7,  54,  15,  82},
    {137,  26, 208,  40, 124,  23, 187,  36},
    {21, 112,  13,  74,  19, 101,  11,  66},
    {254,  49, 169,  32, 230,  44, 152,  29}
};


static int (*pattern)[8] = dither;	/* -> dither or halftone */

static FILE *infp;			/* input file handle */
static const char hyphen[] = "hyphen";
static const char *file_name = hyphen;	/* name of input file, for banner */

static size_t height;			/* input height */
static size_t width;			/* input width */

static int thresh = -1;		/* Threshold */

static int page_xoff = 150;	/* 150=0.5", 192=0.75" */
static int page_yoff = 80;		/* 80=0.25", 544=1.75" */

#define MAXWIDTH 2600
long swath[32][MAXWIDTH/32];	/* assumes long has 32 bits */
unsigned char line[MAXWIDTH];		/* grey-scale input buffer */

static int im_mag;			/* magnification (1, 2 or 4) */
static size_t im_width;		/* image size (in Imagen dots) */
static size_t im_wpatches;		/* # 32-bit patches width */
static size_t im_hpatches;		/* # 32-bit patches height */

int get_args(int argc, char **argv);
int im_close(void);
int im_header(void);
void im_write(int y);

char usage[] = "\
Usage: bw-imp [-D] [-s squaresize] [-w width] [-n height]\n\
	[-X page_xoff] [-Y page_yoff] [-t thresh] [file.bw] > impress\n";

int
get_args(int argc, char **argv)
{
    int c;

    while ((c = bu_getopt(argc, argv, "Ds:n:w:t:X:Y:h?")) != -1) {
	switch (c) {
	    case 'D':
		/* halftone instead of dither */
		pattern = halftone;
		break;
	    case 's':
		/* square size */
		height = width = atoi(bu_optarg);
		break;
	    case 'n':
		height = atoi(bu_optarg);
		break;
	    case 'w':
		width = atoi(bu_optarg);
		break;
	    case 't':
		thresh = atoi(bu_optarg);
		break;
	    case 'X':
		page_xoff = atoi(bu_optarg);
		break;
	    case 'Y':
		page_yoff = atoi(bu_optarg);
		break;

	    default:		/* '?' */
		return false;
	}
    }

    if (bu_optind >= argc) {
	if (isatty(fileno(stdin)))
	    return false;
	file_name = hyphen;
	infp = stdin;
    } else {
	file_name = argv[bu_optind];
	if ((infp = fopen(file_name, "r")) == NULL) {
	    fprintf(stderr,
		    "bw-imp: cannot open \"%s\" for reading\n",
		    file_name);
	    return false;
	}
    }

    if (argc > ++bu_optind)
	fprintf(stderr, "bw-imp: excess argument(s) ignored\n");

    return true;
}


int
main(int argc, char **argv)
{
    size_t y;

    height = width = 512;		/* Defaults */

    if (!get_args(argc, argv) || isatty(fileno(stdout))) {
	(void)fputs(usage, stderr);
	return 1;
    }

    if (thresh >= 0) {
	/* Each pixel in gives one bit out, depending on thresh */
	im_mag = 1;
    } else {
	if (width > 512) im_mag = 2;
	else if (width > 256) im_mag = 4;
	else if (width > 128) im_mag = 8;
	else im_mag = 16;
    }
    im_width  = (width * im_mag) & (~31);
    im_wpatches = (im_width+31) / 32;
    im_hpatches = ((height * im_mag)+31) / 32;
    if (im_wpatches*32 > 2560) {
	fprintf(stderr, "bw-imp:  output %lu too wide, limit is 2560\n",
		(unsigned long)im_wpatches*32);
	return 1;
    }

    if (!im_header())
	return 1;

    for (y = 0; y < height; y += 32/im_mag) {
	if (feof(infp))
	    return 1;
	im_write(y);
    }

    if (!im_close())
	return 1;

    return 0;
}


int
im_header(void)
{

    printf("@document(language impress, prerasterization on, Name \"%s\")",
	   file_name
	);

    /* The margins need to be multiples of 16 (printer word align) */
    (void)putchar(205);		/* SET_HV_SYSTEM (whole page) */
    (void)putchar(0x54);
    (void)putchar(135);		/* SET_ABS_H (left margin) */
    (void)putchar((page_xoff>>8)&0xFF);
    (void)putchar(page_xoff&0xFF);
    (void)putchar(137);		/* SET_ABS_V (top margin) */
    (void)putchar((page_yoff>>8)&0xFF);
    (void)putchar(page_yoff&0xFF);
    (void)putchar(205);		/* SET_HV_SYSTEM (set origin) */
    (void)putchar(0x74);
    (void)putchar(206);		/* SET_ADV_DIRS (normal raster scan) */
    (void)putchar(0);
    (void)putchar(236);		/* SET_MAGNIFICATION */
    (void)putchar(0);		/* x 1 */
    (void)putchar(235);		/* BITMAP */
    (void)putchar(3);		/* opaque (store) */
    (void)putchar(im_wpatches);	/* hsize (# patches across) */
    (void)putchar(im_hpatches);	/* vsize (# patches down) */

    return true;
}


void
im_write(int y)
{
    size_t y1;

    /* Process one 32-bit high output swath */
    for (y1 = 0; y1 < 32; y1 += im_mag) {
	size_t x;

	/* Obtain a single line of 8-bit pixels */
	if (fread(line, 1, width, infp) != width) {
	    memset((void *)line, 0, width);
	}

	/* construct im_mag scans of Imagen swath */
	for (x = 0; x < im_width; x += 32) {
	    int my;

	    for (my = 0; my < im_mag; ++my) {
		long b = 0L;	/* image bits */
		int x1;

		for (x1 = 0; x1 < 32; x1 += im_mag) {
		    int level =
			line[width-1-((x + x1) / im_mag)];
		    int mx;

		    if (im_mag <= 1) {
			b <<= 1;
			if (level < thresh)
			    b |= 1L;
			continue;
		    }
		    for (mx = 0; mx < im_mag; ++mx) {
			int pgx, pgy;	/* page position */
			b <<= 1;

			/* Compute Dither */
			pgx = x + x1 + mx;
			pgy = y + y1 + my;
			/* ameliorate grid regularity */
			if (pattern == halftone &&
			    (pgy % 16) >= 8)
			    pgx += 4;

			if (level < pattern[pgx % 8][pgy % 8])
			    b |= 1L;
		    }
		}
		swath[y1 + my][x / 32] = b;
	    }
	}
    }

    /* output the swath */
    {
	size_t xx, yy;
	for (xx = 0; xx < im_wpatches; ++xx) {
	    for (yy = 0; yy < 32; ++yy) {
		long b = swath[yy][xx];
		int c;

		c = (int)(b >> 24) & 0xFF; (void)putchar(c);
		c = (int)(b >> 16) & 0xFF; (void)putchar(c);
		c = (int)(b >> 8) & 0xFF; (void)putchar(c);
		c = (int)(b) & 0xFF; (void)putchar(c);
	    }
	}
    }
}


int
im_close(void)
{
/* (void)putchar(219);		ENDPAGE */

    (void)fflush(stdout);
    return true;
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
