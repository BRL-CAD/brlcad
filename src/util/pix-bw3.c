/*                       P I X - B W 3 . C
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
/** @file util/pix-bw3.c
 *
 * Converts a RGB pix file into 3 8-bit BW files.
 * (i.e. seperates the colors)
 *
 */

#include "common.h"

#include <stdlib.h>
#include <string.h>
#include "bio.h"

#include "bu.h"


unsigned char ibuf[3*1024];
unsigned char red[1024], green[1024], blue[1024];


int
main(int argc, char **argv)
{
    int i, num;
    FILE *rfp, *bfp, *gfp;
    unsigned char *ibufp;
    size_t ret;

    if (argc != 4 || isatty(fileno(stdin))) {
	bu_exit(1, "usage: pix-bw3 redout greenout blueout < file.pix\n");
    }

    rfp = fopen(argv[1], "w");
    gfp = fopen(argv[2], "w");
    bfp = fopen(argv[3], "w");

    if (rfp == NULL || gfp == NULL || bfp == NULL) {
	bu_exit(2, "pix-bw3: Can't open output files\n");
    }

    while ((num = fread(ibuf, sizeof(char), 3*1024, stdin)) > 0) {
	ibufp = &ibuf[0];
	for (i = 0; i < num/3; i++) {
	    red[i] = *ibufp++;
	    green[i] = *ibufp++;
	    blue[i] = *ibufp++;
	}
	ret = fwrite(red, sizeof(*red), num/3, rfp);
	if (ret < (size_t)num/3)
	    perror("fwrite");
	ret = fwrite(green, sizeof(*green), num/3, gfp);
	if (ret < (size_t)num/3)
	    perror("fwrite");
	ret = fwrite(blue, sizeof(*blue), num/3, bfp);
	if (ret < (size_t)num/3)
	    perror("fwrite");
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
