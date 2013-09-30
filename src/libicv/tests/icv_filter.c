/*                    I C V _ F I L T E R . C
 * BRL-CAD
 *
 * Copyright (c) 2013 United States Government as represented by
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
/** @file icv_filter.c
 *
 * tester function for icv_api.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <stdio.h>

#include "bio.h"
#include "bu.h"
#include "icv.h"

#define TOTAL_FILTERS 6

struct kernels_info {
    char *name;
    char *uname;		/* What is needed to recognize it */
    ICV_FILTER filter;
} kernel[] = {
    { "LOW PASS", "lo", ICV_FILTER_LOW_PASS },
    { "LAPLACIAN", "la", ICV_FILTER_LAPLACIAN },
    { "HIGH PASS", "hi", ICV_FILTER_HIGH_PASS },
    { "HORIZONTAL GRAD", "hg", ICV_FILTER_HORIZONTAL_GRAD },
    { "VERTICAL GRAD", "vg", ICV_FILTER_VERTICAL_GRAD },
    { "BOXCAR AVERAGE", "b",  ICV_FILTER_BOXCAR_AVERAGE },
    { NULL, NULL, ICV_FILTER_NULL },
};


void usage()
{
    int i;

    bu_log("[-h] [-s squaresize] [-w width] [-n height] \n\
	    [-f lo|la|hi|hg|vg|b]\n\
	    [-b -p -d -m] \n\
	    [-o out_file] [file] > [out_file]\n");

    bu_log("#Filter Options\n");

    for (i = 0; i<TOTAL_FILTERS-1; i++)
	bu_log("\t %s for %s\n", kernel[i].uname, kernel[i].name);

    bu_log("#Image Options\n\
	    \t -b for bw image\n\
	    \t -d for dpix image\n\
	    \t -m for b image\n\
	    \t -p for pix image\n");

}

ICV_FILTER select_filter(char* uname)
{
    int i;
    for (i = 0; i<TOTAL_FILTERS-1; i++)
	if(BU_STR_EQUAL(kernel[i].uname,uname))
	    return kernel[i].filter;

    bu_log("Using Low Pass Filter\n");
    return ICV_FILTER_LOW_PASS; /* default */
}

int main(int argc, char* argv[])
{
    char *out_file = NULL;
    char *in_file = NULL;
    int c;
    int inx=0, iny=0;
    icv_image_t *bif;
    ICV_IMAGE_FORMAT format=ICV_IMAGE_AUTO;
    ICV_FILTER filter = ICV_FILTER_LOW_PASS;
    if (argc<2) {
	usage();
	return 1;
    }

    while ((c = bu_getopt(argc, argv, "s:w:n:f:o:bpdmh?")) != -1) {
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
	    case 'f' :
		filter = select_filter(bu_optarg);
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
    icv_filter(bif, filter);
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
