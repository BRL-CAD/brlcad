/*                      I C V _ C R O P . C
 * BRL-CAD
 *
 * Copyright (c) 2016-2021 United States Government as represented by
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
/** @file icv_crop.c
 *
 * Brief description
 *
 */

#include "common.h"

#include <stdlib.h>

#include "bio.h"
#include "bu/app.h"
#include "bu/log.h"
#include "bu/mime.h"
#include "bu/getopt.h"
#include "icv.h"

void usage()
{
    bu_log("[-s squaresize] [-w width] [-n height] [-W out_width ] [-N out_height] \n\
		    [-b -p -d -m] \n\
			[-S out_squaresize] [-o out_file] [file] \n");

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
    int outx=0, outy=0;
    icv_image_t *bif;
    long format = BU_MIME_IMAGE_AUTO;
    int urx, ury, ulx, uly, llx, lly, lrx, lry;
    int ret;

    bu_setprogname(argv[0]);

    if (argc<2) {
	    usage();
	    return 1;
    }


    while ((c = bu_getopt(argc, argv, "s:W:w:N:n:s:S:o:bpdmh?")) != -1) {
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
		case 'o':
			out_file = bu_optarg;
			break;
		case 'b' :
			format = BU_MIME_IMAGE_BW;
			break;
		case 'p' :
			format = BU_MIME_IMAGE_PIX;
			break;
		case 'd' :
			format = BU_MIME_IMAGE_DPIX;
			break;
		case 'm' :
			format = BU_MIME_IMAGE_PPM;
			break;
		default: /* 'h' '?' */
			usage();
			return 1;

	    }
    }

    if (bu_optind >= argc) {
	usage();
	return 1;
    }
    in_file = argv[bu_optind];

    bu_log("\t          (ulx,uly)         (urx,ury)\n\
	    \t           __________________\n\
	    \t          /                 |\n\
	    \t         /                  |\n\
	    \t        /                   |\n\
	    \t       /                    |\n\
	    \t      /                     |\n\
	    \t     /______________________|\n\
	    \t   (llx,lly)             (lrx,lry)\n");


    bu_log("Prompting Input Parameters\n");

    bu_log("\tUpper left corner in input file (ulx, uly)?: ");
	ret = scanf("%d%d", &ulx, &uly);
	if (ret != 2)
	    perror("scanf");

	bu_log("\tUpper right corner (urx, ury)?: ");
	ret = scanf("%d%d", &urx, &ury);
	if (ret != 2)
	    perror("scanf");

	bu_log("\tLower right (lrx, lry)?: ");
	ret = scanf("%d%d", &lrx, &lry);
	if (ret != 2)
	    perror("scanf");

	bu_log("\tLower left (llx, lly)?: ");
	ret = scanf("%d%d", &llx, &lly);
	if (ret != 2)
	    perror("scanf");


    bif = icv_read(in_file, format, inx, iny);
    icv_crop(bif, ulx, uly, urx, ury, lrx, lry, llx, lly, outy, outx);
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
