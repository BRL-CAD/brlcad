/*                        P I X - B W . C
 * BRL-CAD
 *
 * Copyright (c) 1986-2011 United States Government as represented by
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


unsigned char ibuf[3*1024], obuf[1024];

/* flags */
int red   = 0;
int green = 0;
int blue  = 0;
double rweight = 0.0;
double gweight = 0.0;
double bweight = 0.0;

static const char usage[] = "\
Usage: pix-bw [-ntsc -crt -R[#] -G[#] -B[#]] [in.pix] > out.bw\n";

int
main(int argc, char **argv)
{
    size_t in, out, num;
    int multiple_colors, num_color_planes;
    int clip_high, clip_low;
    double value;
    FILE *finp, *foutp;
    size_t ret;

    while (argc > 1 && argv[1][0] == '-') {
	if (BU_STR_EQUAL(argv[1], "-ntsc")) {
	    /* NTSC weights */
	    rweight = 0.30;
	    gweight = 0.59;
	    bweight = 0.11;
	    red = green = blue = 1;
	} else if (BU_STR_EQUAL(argv[1], "-crt")) {
	    /* CRT weights */
	    rweight = 0.26;
	    gweight = 0.66;
	    bweight = 0.08;
	    red = green = blue = 1;
	} else switch (argv[1][1]) {
	    case 'R':
		red++;
		if (argv[1][2] != '\0')
		    rweight = atof(&argv[1][2]);
		break;
	    case 'G':
		green++;
		if (argv[1][2] != '\0')
		    gweight = atof(&argv[1][2]);
		break;
	    case 'B':
		blue++;
		if (argv[1][2] != '\0')
		    bweight = atof(&argv[1][2]);
		break;
	    default:
		fprintf(stderr, "pix-bw: bad flag \"%s\"\n", argv[1]);
		bu_exit(1, "%s", usage);
	}
	argc--;
	argv++;
    }

    if (argc > 1) {
	if ((finp = fopen(argv[1], "rb")) == NULL) {
	    bu_exit(2, "pix-bw: can't open \"%s\"\n", argv[1]);
	}
    } else
	finp = stdin;

#if defined(_WIN32) && !defined(__CYGWIN__)
    setmode(fileno(stdin), O_BINARY);
    setmode(fileno(stdout), O_BINARY);
    setmode(fileno(stderr), O_BINARY);
#endif

    foutp = stdout;
    
    if (isatty(fileno(finp)) || isatty(fileno(foutp))) {
	bu_exit(2, "%s", usage);
    }

    /* Hack for multiple color planes */
    if (red + green + blue > 1 || !ZERO(rweight) || !ZERO(gweight) || !ZERO(bweight))
	multiple_colors = 1;
    else
	multiple_colors = 0;

    num_color_planes = red + green + blue;
    if (red != 0 && ZERO(rweight))
	rweight = 1.0 / (double)num_color_planes;
    if (green != 0 && ZERO(gweight))
	gweight = 1.0 / (double)num_color_planes;
    if (blue != 0 && ZERO(bweight))
	bweight = 1.0 / (double)num_color_planes;

    clip_high = clip_low = 0;
    while ((num = fread(ibuf, sizeof(char), 3*1024, finp)) > 0) {
	/*
	 * The loops are repeated for efficiency...
	 */
	if (multiple_colors) {
	    for (in = out = 0; out < num/3; out++, in += 3) {
		value = rweight*ibuf[in] + gweight*ibuf[in+1] + bweight*ibuf[in+2];
		if (value > 255.0) {
		    obuf[out] = 255;
		    clip_high++;
		} else if (value < 0.0) {
		    obuf[out] = 0;
		    clip_low++;
		} else
		    obuf[out] = value;
	    }
	} else if (red) {
	    for (in = out = 0; out < num/3; out++, in += 3)
		obuf[out] = ibuf[in];
	} else if (green) {
	    for (in = out = 0; out < num/3; out++, in += 3)
		obuf[out] = ibuf[in+1];
	} else if (blue) {
	    for (in = out = 0; out < num/3; out++, in += 3)
		obuf[out] = ibuf[in+2];
	} else {
	    /* uniform weight */
	    for (in = out = 0; out < num/3; out++, in += 3)
		obuf[out] = ((int)ibuf[in] + (int)ibuf[in+1] +
			     (int)ibuf[in+2]) / 3;
	}
	ret = fwrite(obuf, sizeof(char), num/3, foutp);
	if (ret != (size_t)num/3)
	    perror("fwrite");
    }

    if (clip_high != 0 || clip_low != 0) {
	fprintf(stderr, "pix-bw: clipped %d high, %d, low\n",
		clip_high, clip_low);
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
