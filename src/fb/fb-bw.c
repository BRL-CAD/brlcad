/*                         F B - B W . C
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
/** @file fb-bw.c
 *
 * Read a Black and White image from the framebuffer and output
 * it in 8-bit black and white form in pix order,
 * i.e. Bottom UP, left to right.
 *
 */

#include "common.h"

#include <stdlib.h>

#include "bu/color.h"
#include "bu/getopt.h"
#include "bu/log.h"
#include "fb.h"


#define LINELEN 8192

static unsigned char inbuf[LINELEN*3];
static unsigned char obuf[LINELEN];

int height;
int width;
int inverse;
int scr_xoff, scr_yoff;

char *framebuffer = NULL;
char *file_name;
FILE *outfp;


int
get_args(int argc, char **argv)
{
    int c;

    while ((c = bu_getopt(argc, argv, "iF:X:Y:s:w:n:h?")) != -1) {
	switch (c) {
	    case 'i':
		inverse = 1;
		break;
	    case 'F':
		framebuffer = bu_optarg;
		break;
	    case 'X':
		scr_xoff = atoi(bu_optarg);
		break;
	    case 'Y':
		scr_yoff = atoi(bu_optarg);
		break;
	    case 's':
		/* square size */
		height = width = atoi(bu_optarg);
		break;
	    case 'w':
		width = atoi(bu_optarg);
		break;
	    case 'n':
		height = atoi(bu_optarg);
		break;

	    default:		/* '?' 'h' */
		return 0;
	}
    }

    if (bu_optind >= argc) {
	if (isatty(fileno(stdout)))
	    return 0;
	file_name = "-";
	outfp = stdout;
    } else {
	file_name = argv[bu_optind];
	if ((outfp = fopen(file_name, "wb")) == NULL) {
	    fprintf(stderr,
		    "fb-bw: cannot open \"%s\" for writing\n",
		    file_name);
	    return 0;
	}
    }

    if (argc > ++bu_optind)
	fprintf(stderr, "fb-bw: excess argument(s) ignored\n");

    return 1;		/* OK */
}


int
main(int argc, char **argv)
{
    fb *fbp;

    int x, y;
    int xin, yin;		/* number of sceen output lines */

    char usage[] = "Usage: fb-bw [-i] [-F framebuffer]\n\
	[-X scr_xoff] [-Y scr_yoff]\n\
	[-s squaresize] [-w width] [-n height] [file.bw]\n";

    height = width = 512;		/* Defaults */

    if (!get_args(argc, argv)) {
	(void)fputs(usage, stderr);
	bu_exit(1, NULL);
    }

    /* Open Display Device */
    if ((fbp = fb_open(framebuffer, width, height)) == NULL) {
	fprintf(stderr, "fb_open failed\n");
	bu_exit(1, NULL);
    }

    /* determine "reasonable" behavior */
    xin = fb_getwidth(fbp) - scr_xoff;
    if (xin < 0) xin = 0;
    if (xin > width) xin = width;
    yin = fb_getheight(fbp) - scr_yoff;
    if (yin < 0) yin = 0;
    if (yin > height) yin = height;

    for (y = scr_yoff; y < scr_yoff + yin; y++) {
	size_t ret;
	if (inverse) {
	    (void)fb_read(fbp, scr_xoff, fb_getheight(fbp)-1-y, inbuf, xin);
	} else {
	    (void)fb_read(fbp, scr_xoff, y, inbuf, xin);
	}
	for (x = 0; x < xin; x++) {
	    obuf[x] = (((int)inbuf[3*x+RED]) + ((int)inbuf[3*x+GRN])
		       + ((int)inbuf[3*x+BLU])) / 3;
	}
	ret = fwrite(&obuf[0], sizeof(char), xin, outfp);
	if (ret != (size_t)xin)
	    perror("fwrite");
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
