/*                       P I X F A D E . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2026 United States Government as represented by
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
/** @file util/pixfade.c
 *
 * Fade a picture
 *
 * pixfade will darken a pix by a certain percentage or do an integer
 * max pixel value.  It runs in two modes, truncate which will cut any
 * channel greater than param to param, and scale which will change
 * a channel to param percent of its original value (limited by 0-255)
 *
 * Inputs:
 *	-m	integer max value
 *	-f	fraction to fade
 *	-p	percentage of fade (fraction = percentage/100)
 *      -s      squaresize of input pixfile
 *      -w      width of input pixfile
 *      -n      height of input pixfile
 *	file	a picture file (if not given, use STDIN)
 *
 * Output:
 *	STDOUT	the faded picture.
 *
 * Calls:
 *	get_args
 *
 * Method:
 *	straight-forward.
 *
 */
#include "common.h"

#include <errno.h>
#include <stdlib.h>
#include <limits.h>
#include <float.h>

#include "bio.h"

#include "bu/app.h"
#include "bu/getopt.h"
#include "bu/opt.h"
#include "bu/log.h"
#include "bu/mime.h"

#include "icv.h"

int inx=512, iny=512;
char *out_file = NULL;
char *in_file = NULL;


char usage[] = "\
Usage: pixfade [-p percentage] [-f fraction] [-m max] [-s squaresize] [-w width] [-n height] \n\
                [-o out_file.pix] [file.pix] [> out_file.pix]\n";

double multiplier = 0.5;
int max = -1;

int
get_args(int argc, char **argv)
{
    int c;

    while ((c = bu_getopt(argc, argv, "p:f:s:w:n:o:m:h?")) != -1) {
	switch (c) {
            case 'p':
		if (!bu_opt_scan_double_range(bu_optarg, &multiplier, 0.0, DBL_MAX, "percentage")) {
		    bu_exit(1, NULL);
		}
		multiplier /= 100.0;
		break;
	    case 'f':
		if (!bu_opt_scan_double_range(bu_optarg, &multiplier, 0.0, DBL_MAX, "fraction")) {
		    bu_exit(1, NULL);
		}
		break;
    	    case 's':
		if (!bu_opt_scan_int_range(bu_optarg, &inx, 1, INT_MAX, "input size")) {
		    bu_exit(1, NULL);
		}
		iny = inx;
		break;
	    case 'w':
		if (!bu_opt_scan_int_range(bu_optarg, &inx, 1, INT_MAX, "input width")) {
		    bu_exit(1, NULL);
		}
		break;
	    case 'n':
		if (!bu_opt_scan_int_range(bu_optarg, &iny, 1, INT_MAX, "input height")) {
		    bu_exit(1, NULL);
		}
		break;
	    case 'o':
		out_file = bu_optarg;
		break;
	    case 'm':
		if (!bu_opt_scan_int(bu_optarg, &max, "max value")) {
		    bu_exit(1, NULL);
		}
		if (max < 0 )
		    max = 0;
		else if (max > 255)
		    max=255;
		break;
	    default:		/* 'h' '?' */
		return 0;
	}
    }

    if (bu_optind >= argc) {
	if (isatty(fileno(stdin))) {
	    return 0;
	}
    }

    in_file = argv[bu_optind];
    bu_optind++;
    if (bu_optind < argc) {
	bu_log("pixfade: excess argument(s) not supported\n");
	return 0;
    }
    return 1;		/* OK */

}

int
icv_ceiling(icv_image_t *img, int ceiling)
{
    size_t size;
    double *data;

    ICV_IMAGE_VAL_INT(img);

    size= img->height*img->width*img->channels;

   if (size == 0)
	return -1;

    data = img->data;

    while (size--) {
	if (*data > ceiling)
	    *data = ceiling;
	data++;
    }
    return 0;
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
    if (max < 0 )
	icv_fade(img, multiplier);
    else
	icv_ceiling(img, max);
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
