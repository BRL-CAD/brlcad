/*                        P I X - B W . C
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

#include "bu.h"
#include "vmath.h"
#include "icv.h"


unsigned char ibuf[3*1024], obuf[1024];

/* flags */
int red   = 0;
int green = 0;
int blue  = 0;
double rweight = 0.0;
double gweight = 0.0;
double bweight = 0.0;
int inx = 0, iny = 0;
ICV_COLOR color;

char *out_file = NULL;
char *in_file = NULL;

static const char usage[] = "\
pix-bw [-h] [-s squaresize] [-w width] [-n height] \n\
            [ [-e ntsc|crt] [[-R red_weight] [-G green_weight] [-B blue_weight]] ]\n\
	    [-o out_file.bw] [file.bw] > [out_file.bw] \n";

double multiplier = 0.5;

int
get_args(int argc, char **argv)
{
    int c;

    bu_optind = 1;
    while ((c = bu_getopt(argc, argv, "e:s:w:n:R:G:B:o:h?NC")) != -1) {
	switch (c) {
	    case 'e' :
	        if (BU_STR_EQUAL(bu_optarg, "ntsc")) {
		    rweight = 0.30;
		    gweight = 0.59;
		    bweight = 0.11;
		    red = green = blue = 1;
		    color = ICV_COLOR_RGB;
		}
		else if (BU_STR_EQUAL(bu_optarg, "crt")) {
		    rweight = 0.26;
		    gweight = 0.66;
		    bweight = 0.08;
		    red = green = blue = 1;
		    color = ICV_COLOR_RGB;
		}
		else
		    return 0;
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
	    case 'h' :
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

    setmode(fileno(stdin), O_BINARY);
    setmode(fileno(stdout), O_BINARY);
    setmode(fileno(stderr), O_BINARY);

    img = icv_read(in_file, ICV_IMAGE_PIX, inx, iny);

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
    else bu_exit(1, "%s",usage);

    icv_rgb2gray(img, color, rweight, gweight, bweight);

    icv_write(img, out_file, ICV_IMAGE_BW);

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
