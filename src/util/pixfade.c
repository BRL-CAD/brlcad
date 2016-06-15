/*                       P I X F A D E . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2014 United States Government as represented by
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
 *	file	a picture file.
 *	STDIN	a picture file if 'file' is not given.
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

#include <stdlib.h>
#include <stdio.h>

#include "bio.h"
#include "icv.h"
#include "bu.h"

int inx=512, iny=512;
char *out_file = NULL;
char *in_file = NULL;


char usage[] = "\
Usage: pixfade [-h] [-p percentage] [-f fraction] [-s squaresize] [-w width] [-n height] \n\
                [-o out_file.pix] [file.bw] > [out_file.pix]\n";

double multiplier = 0.5;

int
get_args(int argc, char **argv)
{
    int c;

    while ((c = bu_getopt(argc, argv, "p:f:s:w:n:o:h?")) != -1) {
	switch (c) {
            case 'p':
		multiplier = atof(bu_optarg) / 100.0;
		if (multiplier < 0.0) {
		    bu_log("pixfade: percent is negative");
		    bu_exit (1, NULL);
		}
		break;
	    case 'f':
		multiplier = atof(bu_optarg);
		if (multiplier < 0.0) {
		    bu_log("pixfade: fraction is negative");
		    bu_exit (1, NULL);
		}
		break;
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
	    case 'h':
	    default:		/* '?' */
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
	bu_log("pixfade: excess argument(s) ignored\n");
    }

    return 1;		/* OK */
}


int
main(int argc, char **argv)
{
    icv_image_t *img;
    if (!get_args(argc, argv)) {
        bu_log("%s", usage);
        return 1;
    }

    img = icv_read(in_file, ICV_IMAGE_PIX, inx, iny);
    if (img == NULL)
        return 1;
    icv_fade(img, multiplier);
    icv_write(img, out_file, ICV_IMAGE_PIX);

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
