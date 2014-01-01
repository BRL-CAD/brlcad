/*                 I C V _ R E A D _ W R I T E . C
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
/** @file icv_read_write.c
 *
 * This testing function is to test the time required in loading and
 * image to icv container and saving it again.
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
    bu_log("tester_icv_read_write [-h] [-s squaresize] [-w width] [-n height]]\n\
				  [-b -p -d -m]\n\
				  [-o out_file] [in_file] > [out_file]\n");

    bu_log("#Options\n\
	    \t -b for bw images\n\
	    \t -d for dpix images\n\
	    \t -m for b images\n\
	    \t -p for pix images\n");
}

int main(int argc, char* argv[])
{
    char *out_file = NULL;
    char *in_file = NULL;
    int inx=0, iny=0;
    icv_image_t *bif;
    ICV_IMAGE_FORMAT format=ICV_IMAGE_AUTO;
    int c;

    if (argc<2) {
	usage();
	return 1;
    }

    while ((c = bu_getopt(argc, argv, "s:o:w:nbpdmh?")) != -1) {
	switch (c) {
	    case 'o' :
		bu_log("out_file = %s\n", bu_optarg);
		out_file = bu_optarg;
		break;
	    case 's' :
	       inx = iny = atoi(bu_optarg);
	       break;
	    case 'w' :
	       inx = atoi(bu_optarg);
	       break;
	    case 'n' :
	       iny = atoi(bu_optarg);
	       break;
	    case 'b' :
		bu_log("There was in bw\n");
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

	bu_log("C= %c, optind = %d\n", c, bu_optind);
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

    bu_log("in_file = %s, out_file = %s\n", in_file, out_file);

    bif = icv_read(in_file, format, inx, iny);
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
