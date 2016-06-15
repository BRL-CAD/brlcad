/*                      I C V _ R E C T . C
 * BRL-CAD
 *
 * Copyright (c) 2013-2014 United States Government as represented by
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
/** @file icv_rect.c
 *
 * tester function for icv_rect api.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <stdio.h>

#include "bio.h"
#include "bu.h"
#include "icv.h"

void usage()
{
    bu_log("[-h] [squaresize] [-w width] [-n height] [-W out_width ] [-N out_height] \n\
		    [-b -p -d -m] \n\
			[-x xorig] [-y yorig] [-S out_squaresize] [-o out_file] [file] > [out_file]\n");

    bu_log("#Image Options\n\
	    \t -b for bw image\n\
	    \t -d for dpix image\n\
	    \t -m for b image\n\
	    \t -p for pix image\n");
}

int main(int argc, char* argv[])
{
    char *out_file = NULL;
    char *in_file = NULL;
    int c;
    int inx=0, iny=0;
    int xorig=0, yorig=0;
    int outx=0, outy=0;
    icv_image_t *bif;
    ICV_IMAGE_FORMAT format=ICV_IMAGE_AUTO;

    if (argc<2) {
	usage();
	return 1;
    }

    while ((c = bu_getopt(argc, argv, "s:W:w:N:n:S:x:y:o:bpdmh?")) != -1) {
	switch (c) {
	     case 's':
		inx = iny = atoi(bu_optarg);
		break;
	    case 'W':
		outx = atoi(bu_optarg);
		break;
	    case 'w':
		inx = atoi(bu_optarg);
		break;
	    case 'N':
		outy = atoi(bu_optarg);
		break;
	    case 'n':
		iny = atoi(bu_optarg);
		break;
	    case 'S':
		outy = outx = atoi(bu_optarg);
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
	    case 'b' :
		format = ICV_IMAGE_BW;
		break;
	    case 'p' :
		format = ICV_IMAGE_PIX;
		break;
	    case 'd' :
		format = ICV_IMAGE_DPIX;
		break;
	    case 'm' :
		format = ICV_IMAGE_PPM;
		break;
	    case 'h':
	    default:
		usage();
		return 1;

	}
    }
    if (bu_optind >= argc) {
	if (isatty(fileno(stdin))) {
	    usage();
	    return 1;
	}
    }
    else {
	in_file = argv[bu_optind];
	bu_optind++;
    }

    bif = icv_read(in_file, format, inx, iny);
    icv_rect(bif, xorig, yorig, outx, outy);
    icv_write(bif,out_file, format);
    icv_destroy(bif);

    return 0;
}

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
