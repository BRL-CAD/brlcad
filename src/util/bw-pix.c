/*                        B W - P I X . C
 * BRL-CAD
 *
 * Copyright (c) 1986-2014 United States Government as represented by
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
/** @file util/bw-pix.c
 *
 * Convert an 8-bit black and white file to a 24-bit
 * color one by replicating each value three times.
 *
 */
#include "common.h"
#include <stdlib.h>
#include <stdio.h>
#include "bio.h"
#include "icv.h"
#include "bu.h"

char usage[] = "\
Usage: bw-pix [-h] [[-s squaresize] [-w width] [-n height]]\n\
[-o out_file.pix] [file.pix] > [out_file.pix]\n";

char *out_file = NULL;
char *in_file = NULL;
int inx=0, iny=0;

int
get_args(int argc, char **argv)
{
    int c;

    while ((c = bu_getopt(argc, argv, "s:w:n:o:h?")) != -1) {
	switch (c) {
	    case 'o':
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
	bu_log("bw-pix: excess argument(s) ignored\n");
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

#if defined(_WIN32) && !defined(__CYGWIN__)
    setmode(fileno(stdin), O_BINARY);
    setmode(fileno(stdout), O_BINARY);
    setmode(fileno(stderr), O_BINARY);
#endif

    img = icv_read(in_file, ICV_IMAGE_BW, inx, iny);
    if (img == NULL)
	return 1;
    icv_gray2rgb(img);
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
