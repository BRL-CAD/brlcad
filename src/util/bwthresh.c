/*                      B W T H R E S H . C
 * BRL-CAD
 *
 * Copyright (c) 1990-2016 United States Government as represented by
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
/** @file util/bwthresh.c
 *
 * Threshold data in BW(5) format.
 *
 * Accepts n arguments, the threshold values.  The grey scale 0..255
 * is divided evenly into n+1 bins.  Each pixel of input is binned
 * according to the threshold values, and the appropriate bin color
 * is output.
 *
 */

#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bu/log.h"
#include "bu/malloc.h"
#include "bu/str.h"


#define USAGE "Usage: bwthresh values ...\n"


int
main (int argc, char **argv)
{
    int Ch;		/* The current input character */
    int *thresh_val;	/* The threshold value */
    int nm_threshs;	/* How many thresholds? */
    int i;
    unsigned char *bin_color = (unsigned char *)0;/* resultant pixel values */

    if ((BU_STR_EQUAL(argv[1],"-h") || BU_STR_EQUAL(argv[1],"-?")) && argc == 2)
	bu_exit(1, "%s", USAGE);
    if ((nm_threshs = argc - 1) < 1)
	bu_exit(1, "%s", USAGE);

    if (nm_threshs > 255) {
	bu_exit(1, "bwthresh: Too many thresholds!\n");
    }
    thresh_val = (int *)bu_malloc((unsigned) (nm_threshs * sizeof(int)), "thresh_val");
    bin_color = (unsigned char *)bu_malloc((unsigned) ((nm_threshs + 1) * sizeof(int)), "bin_color");

    for (i = 0; i < nm_threshs; ++i) {
	if (sscanf(*++argv, "%d", thresh_val + i) != 1) {
	    bu_log("bwthresh: Illegal threshold value: '%s'\n", *argv);
	    bu_exit(1, "%s", USAGE);
	}
	if ((unsigned char) thresh_val[i] != thresh_val[i]) {
	    bu_exit(1, "bwthresh: Threshold[%d] value %d out of range.  Need 0 <= value <= 255\n",
		    i, thresh_val[i]);
	}
	if (i > 0) {
	    if (thresh_val[i] <= thresh_val[i - 1]) {
		bu_exit(1, "bwthresh: Threshold values not strictly increasing\n");
	    }
	}
	bin_color[i] = 256 * i / nm_threshs;
    }
    bin_color[i] = 255;

    while ((Ch = getchar()) != EOF) {
	for (i = 0; i < nm_threshs; ++i)
	    if (Ch < thresh_val[i])
		break;
	(void) putchar(bin_color[i]);
    }

    bu_free(thresh_val, "thresh_val");
    bu_free(bin_color, "bin_color");
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
