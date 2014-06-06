/*                 I C V _ S I Z E _ D O W N . C
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
/** @file icv_size_down.c
 *
 * tester api for icv_resize api. This utility tests for the methods
 * dealing in to decrease the size of the image.
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
    bu_log("\
	    [-h] [-s squaresize] [-w width] [-n height] \n\
	    [-f under_sample|shrink]\n\
	    [-b -p -d -m] \n\
	    [-o out_file] [file] > [out_file]\n");

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
    int factor=2;
    icv_image_t *bif;
    ICV_IMAGE_FORMAT format=ICV_IMAGE_AUTO;
    ICV_RESIZE_METHOD method = ICV_RESIZE_SHRINK;
    size_t index;

    if (argc<2) {
	usage();
	return 1;
    }

    while ((c = bu_getopt(argc, argv, "s:w:n:M:f:o:bpdmh?")) != -1) {
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
	    case 'o':
		out_file = bu_optarg;
		break;
	    case 'M':
		if (BU_STR_EQUAL(bu_optarg, "under_sample"))
		    method = ICV_RESIZE_UNDERSAMPLE;
		else if (BU_STR_EQUAL(bu_optarg, "shrink"))
		    method = ICV_RESIZE_SHRINK;
		else {
		    usage();
		    bu_exit(1, "Wrong Input Argument\n");
		}
	    break;
	    case 'f':
		factor = atoi(bu_optarg);
	    break;
	    case 'b':
		format = ICV_IMAGE_BW;
		break;
	    case 'p':
		format = ICV_IMAGE_PIX;
		break;
	    case 'd':
		format = ICV_IMAGE_DPIX;
		break;
	    case 'm':
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
    icv_resize(bif, method, 0, 0, (unsigned int) factor);
    bu_log("File information width %d height%d\n", bif->width, bif->height);

    for (index = 0; index < 65536; index++)
	bu_log("index = %ld, data = %f\n", index, bif->data[index]);


    icv_write(bif,out_file, format);
    bu_log("File information width %d height%d channels = %d\n", bif->width, bif->height, bif->channels);
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
