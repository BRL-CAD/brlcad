/*                  I C V _ S A T U R A T E . C
 * BRL-CAD
 *
 * Copyright (c) 2013-2025 United States Government as represented by
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
/** @file icv_saturate.c
 *
 * Unit testing of saturation functionality.
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

void usage(void)
{
    bu_log("\
	    [-f fraction ]\n\
	    [-p -d -m] \n\
	    [-o out_file] [file] > [out_file]\n");

    bu_log("#Image Options\n\
	    \t -d for dpix image\n\
	    \t -m for ppm image\n\
	    \t -p for pix image\n");

}

int main(int argc, char* argv[])
{
    char *out_file = NULL;
    char *in_file = NULL;
    int c;
    int inx=0, iny=0;
    icv_image_t *bif;
    double fraction = 0;
    bu_mime_image_t format = BU_MIME_IMAGE_AUTO;

    bu_setprogname(argv[0]);

    if (argc<2) {
	usage();
	return 1;
    }

    while ((c = bu_getopt(argc, argv, "f:o:bpdmh?")) != -1) {
	switch (c) {
	    case 'o':
		out_file = bu_optarg;
		break;
	    case 'f' :
		fraction = atof(bu_optarg);
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
    if (icv_saturate(bif, fraction) < 0)
	return 1;
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
