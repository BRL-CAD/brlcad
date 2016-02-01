/*                I C V _ O P E R A T I O N S . C
 * BRL-CAD
 *
 * Copyright (c) 2013-2016 United States Government as represented by
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
/** @file icv_operations.c
 *
 * Tester function for icv_operations.
 *
 */

#include "common.h"

#include <stdlib.h>

#include "bio.h"
#include "bu/log.h"
#include "bu/mime.h"
#include "bu/getopt.h"
#include "bu/str.h"
#include "icv.h"

void usage()
{
    bu_log("\
	    [ -O +|-|/|* ]\n\
	    [-b -p -d -m] \n\
	    [-o out_file]  file_1 file_2 > [out_file]\n");

    bu_log("#Image Options\n\
	    \t -b for bw image\n\
	    \t -d for dpix image\n\
	    \t -m for b image\n\
	    \t -p for pix image\n");

}

int main(int argc, char* argv[])
{
    char *out_file = NULL;
    char *in_file1 = NULL, *in_file2 = NULL;
    int c;
    int inx=0, iny=0;
    char *operation = NULL;
    icv_image_t *bif1, *bif2, *out_bif;
    mime_image_t format = MIME_IMAGE_AUTO;


    if (argc<2) {
	usage();
	return 1;
    }

    while ((c = bu_getopt(argc, argv, "O:o:bpdmh?")) != -1) {
	switch (c) {
	    case 'o':
		out_file = bu_optarg;
		break;
	    case 'O' :
		operation = bu_optarg;
		break;
	    case 'b' :
		format = MIME_IMAGE_BW;
		break;
	    case 'p' :
		format = MIME_IMAGE_PIX;
		break;
	    case 'd' :
		format = MIME_IMAGE_DPIX;
		break;
	    case 'm' :
		format = MIME_IMAGE_PPM;
		break;
	    default:
		usage();
		return 1;

	}
    }
    if (bu_optind > argc) {
	usage();
	return 1;
    }
    in_file1 = argv[bu_optind];
    bu_optind++;

    if (bu_optind > argc) {
	usage();
	return 1;
    }
    in_file2 = argv[bu_optind];

    bif1 = icv_read(in_file1, format, inx, iny);
    bif2 = icv_read(in_file2, format, inx, iny);

    if ( bif1 == NULL || bif2 == NULL ) {
	bu_log("Error loading the image.\n");
	return 1;
    }

    if (BU_STR_EQUAL(operation, "+"))
	out_bif = icv_add(bif1, bif1);
    else if (BU_STR_EQUAL(operation, "-"))
	out_bif = icv_sub(bif1, bif2);
    else if (BU_STR_EQUAL(operation, "/"))
	out_bif = icv_multiply(bif1, bif2);
    else if (BU_STR_EQUAL(operation, "*"))
	out_bif = icv_divide(bif1, bif2);
    else {
	bu_log("Using Default operation (+)");
	out_bif = icv_add(bif1, bif2);
    }

    if (!out_bif) {
	bu_log("Error in Operations\n");
    }

    icv_write(out_bif,out_file, format);
    icv_destroy(bif1);
    icv_destroy(bif2);
    icv_destroy(out_bif);

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
