/*                        P I X - B W . C
 * BRL-CAD
 *
 * Copyright (c) 1986-2016 United States Government as represented by
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
/** @file util/pix-bw.c
 *
 * Converts a RGB pix file into an 8-bit BW file.
 *
 * By default it will weight the RGB values evenly.
 * -ntsc will use weights for NTSC standard primaries and
 *       NTSC alignment white.
 * -crt  will use weights for "typical" color CRT phosphors and
 *       a D6500 alignment white.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "bio.h"

#include "vmath.h"
#include "bu/getopt.h"
#include "bu/str.h"
#include "bu/log.h"
#include "bu/mime.h"
#include "icv.h"


unsigned char ibuf[3*1024], obuf[1024];

/* flags */
int red   = 0;
int green = 0;
int blue  = 0;
double rweight = 0.0;
double gweight = 0.0;
double bweight = 0.0;
size_t inx = 0, iny = 0;
ICV_COLOR color;

char *out_file = NULL;
char *in_file = NULL;

static const char usage[] = "\
Usage: pix-bw [-s squaresize] [-w width] [-n height]\n\
              [ [-e ntsc|crt] [[-R red_weight] [-G green_weight] [-B blue_weight]] ]\n\
              [-o out_file.bw] [[<] file.pix] [> out_file.bw]\n";

int
get_args(int argc, char **argv)
{
    int c;

    bu_optind = 1;
    while ((c = bu_getopt(argc, argv, "e:s:w:n:R:G:B:o:h?")) != -1) {
	switch (c) {
	    case 'e' :
	        if (BU_STR_EQUAL(bu_optarg, "ntsc")) {
		    rweight = 0.30;
		    gweight = 0.59;
		    bweight = 0.11;
		}
		else if (BU_STR_EQUAL(bu_optarg, "crt")) {
		    rweight = 0.26;
		    gweight = 0.66;
		    bweight = 0.08;
		}
		else {
		    fprintf(stderr,"pix-bw: invalid -e argument\n");
		    return 0;
		}
		red = green = blue = 1;
		break;
	    case 'R' :
		red++;
		rweight = atof(bu_optarg);
		break;
	    case 'G' :
		green++;
		gweight = atof(bu_optarg);
		break;
	    case 'B' :
		blue++;
		bweight = atof(bu_optarg);
		break;
	    case 'o' :
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
	    default:		/* '?' 'h' */
		return 0;
	}
    }

/* Eliminate the "cannot send output to a tty" message if we
 * detect the run-with-no-arguments situation.  For an actual
 * run, we would need a least a color-scheme argument.
 */
    if (isatty(fileno(stdout)) && out_file == NULL) {
	if (argc != 1)
    	    bu_log("pix-bw: cannot send output to a tty\n");
	return 0;
    }

    if (bu_optind >= argc) {
	if (isatty(fileno(stdin))) {
	    bu_log("pix-bw: cannot receive input from a tty\n");
	    return 0;
	}
    } else {
	in_file = argv[bu_optind];
	bu_optind++;
	return 1;
    }

    if (argc > ++bu_optind) {
	bu_log("pix-bw: excess argument(s) ignored\n");
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

    setmode(fileno(stdin), O_BINARY);
    setmode(fileno(stdout), O_BINARY);
    setmode(fileno(stderr), O_BINARY);

    img = icv_read(in_file, BU_MIME_IMAGE_PIX, inx, iny);

    if (img == NULL)
	return 1;

    if (red && green && blue)
	color = ICV_COLOR_RGB;
    else if (blue && green)
	color = ICV_COLOR_BG;
    else if (red && blue)
	color = ICV_COLOR_RB;
    else if (red && green)
	color = ICV_COLOR_RG;
    else if (red)
	color = ICV_COLOR_R;
    else if (blue)
	color = ICV_COLOR_B;
    else if (green)
	color = ICV_COLOR_G;
    else
	color = ICV_COLOR_RGB;
    	/* no color scheme specified; rweight,gweight,bweight have
	 * all remained zero, so weight the 3 colors equally.
	 */

    icv_rgb2gray(img, color, rweight, gweight, bweight);

    icv_write(img, out_file, BU_MIME_IMAGE_BW);

    if (!isatty(fileno(stdout)) && out_file != NULL) {
	icv_write(img, NULL, BU_MIME_IMAGE_BW);
    }

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
