/*                        B W R E C T . C
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
/** @file util/bwrect.c
 *
 * Remove a portion of a potentially huge .bw file.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <stdio.h>

#include "bio.h"
#include "bu.h"
#include "icv.h"

int outx=0, outy=0;		/* Number of pixels in new map */
int xorig=0, yorig=0;		/* Bottom left corner to extract from */
int inx=512, iny=512;
char *out_file = NULL;
char *in_file = NULL;


char usage[] = "\
Usage:  bwrect [-s squaresize] [-w width] [-n height] [-S out_squaresize] [-W out_width ] [-N out_height]\n\
			[-x xorig] [-y yorig] [-o out_file.bw] [file.bw] > [out_file.bw]\n";


static int
get_args(int argc, char **argv)
{
    int c;

    while ((c = bu_getopt(argc, argv, "s:w:n:S:W:N:x:y:o:h?")) != -1) {
	switch (c) {
	    case 's':
		inx = iny = atoi(bu_optarg);
		break;
	    case 'w':
		inx = atoi(bu_optarg);
		break;
	    case 'n':
		iny = atoi(bu_optarg);
		break;
	    case 'S':
		outy = outx = atoi(bu_optarg);
		break;
	    case 'W':
		outx = atoi(bu_optarg);
		break;
	    case 'N':
		outy = atoi(bu_optarg);
		break;
	    case 'x':
		xorig = atoi(bu_optarg);
		break;
	    case 'y':
		yorig = atoi(bu_optarg);
		break;
	    case 'o':
		out_file = bu_optarg;
		break;
	    default : /* '?' , 'h' */
		return 0;
	}
    }
    if (bu_optind >= argc) {
	if (isatty(fileno(stdin))) {
	    return 0;
	}
    } else {
        in_file = argv[bu_optind];
        bu_optind++;
        return 1;
    }


    if (!isatty(fileno(stdout)) && out_file!=NULL) {
	return 0;
    }

    if (argc > ++bu_optind) {
	bu_log("bwrect: excess argument(s) ignored\n");
    }

    return 1;
}


int
main(int argc, char **argv)
{
    icv_image_t *img;
    if (!get_args(argc, argv)) {
        bu_log("%s", usage);
        return 1;
    }

    img = icv_read(in_file, ICV_IMAGE_BW, inx, iny);
    if (img == NULL)
        return 1;
    icv_rect(img, xorig, yorig, outx, outy);
    icv_write(img, out_file, ICV_IMAGE_BW);

    icv_destroy(img);
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
