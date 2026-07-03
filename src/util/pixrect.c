/*                       P I X R E C T . C
 * BRL-CAD
 *
 * Copyright (c) 1986-2026 United States Government as represented by
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
/** @file util/pixrect.c
 *
 * Remove a portion of a potentially huge .pix file.
 *
 */

#include "common.h"

#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include "bio.h"

#include "bu/app.h"
#include "bu/getopt.h"
#include "bu/opt.h"
#include "bu/log.h"
#include "bu/mime.h"

#include "icv.h"

int outx=0, outy=0;		/* Number of pixels (width, height) in new map */
int xorig=0, yorig=0;		/* Bottom left corner to extract from */
int inx=512, iny=512;
char *out_file = NULL;
char *in_file = NULL;


char usage[] = "\
Usage:  pixrect [-s squaresize] [-w width] [-n height] [-S out_squaresize] [-W out_width] [-N out_height]\n\
			[-x xorig] [-y yorig] [-o out_file.pix] [file.pix] [> out_file.pix]\n";

static int
get_args(int argc, char **argv)
{
    int c;
    int remaining = 0;

    while ((c = bu_getopt(argc, argv, "s:w:n:S:W:N:x:y:o:h?")) != -1) {
	switch (c) {
	    case 's':
		if (!bu_opt_scan_int_range(bu_optarg, &inx, 1, INT_MAX, "input size"))
		    return 0;
		iny = inx;
		break;
	    case 'w':
		if (!bu_opt_scan_int_range(bu_optarg, &inx, 1, INT_MAX, "input width"))
		    return 0;
		break;
	    case 'n':
		if (!bu_opt_scan_int_range(bu_optarg, &iny, 1, INT_MAX, "input height"))
		    return 0;
		break;
	    case 'S':
		if (!bu_opt_scan_int_range(bu_optarg, &outx, 1, INT_MAX, "output size"))
		    return 0;
		outy = outx;
		break;
	    case 'W':
		if (!bu_opt_scan_int_range(bu_optarg, &outx, 1, INT_MAX, "output width"))
		    return 0;
		break;
	    case 'N':
		if (!bu_opt_scan_int_range(bu_optarg, &outy, 1, INT_MAX, "output height"))
		    return 0;
		break;
	    case 'x':
		if (!bu_opt_scan_int_range(bu_optarg, &xorig, 0, INT_MAX, "x origin"))
		    return 0;
		break;
	    case 'y':
		if (!bu_opt_scan_int_range(bu_optarg, &yorig, 0, INT_MAX, "y origin"))
		    return 0;
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
    }

    remaining = argc - bu_optind;
    if (remaining != 0) {
	bu_log("pixrect: excess argument(s) not supported\n");
	return 0;
    }

    return 1;
}


int
main(int argc, char **argv)
{
    icv_image_t *img;

    bu_setprogname(argv[0]);

    if (!get_args(argc, argv)) {
	bu_log("%s", usage);
	return 1;
    }

    img = icv_read(in_file, BU_MIME_IMAGE_PIX, inx, iny);
    if (img == NULL)
	return 1;
    if (icv_crop_rect(img, xorig, yorig, outx, outy) != 0) {
	icv_destroy(img);
	return 1;
    }
    icv_write(img, out_file, BU_MIME_IMAGE_PIX);
    if (!isatty(fileno(stdout)) && out_file != NULL) {
	icv_write(img, NULL, BU_MIME_IMAGE_PIX);
    }

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
