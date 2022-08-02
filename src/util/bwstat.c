/*                        B W S T A T . C
 * BRL-CAD
 *
 * Copyright (c) 1986-2022 United States Government as represented by
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
/** @file util/bwstat.c
 *
 * Compute statistics of pixels in a black and white (BW) file.
 * Gives min, max, mode, median, mean, s.d., var, and skew.
 * Will optionally dump the histogram of values.
 *
 * We compute these from a histogram which is a real win for
 * discrete values of small range.  Statistics can then be
 * calculated on the lump sums in the bins rather than individual
 * samples.  Also only one pass is required over the input file.
 * (lastly, some statistics such as mode and median, require the
 * histogram)
 *
 */

#include "common.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "bio.h"

#include "bu/app.h"
#include "bu/str.h"
#include "bu/exit.h"


#define IBUFSIZE 1024		/* Max read size in pixels */
unsigned char buf[IBUFSIZE];	/* Input buffer */

int verbose = 0;
long bin[256];		/* Histogram bins */


static const char *Usage = "Usage: bwstat [-v] [file.bw]\n";

/*
 * Display the histogram values.
 */
void
show_hist(long int *histogram, int sum)
{
    int i;

    printf("Histogram:\n");

    for (i = 0; i < 256; i++) {
	printf("%3d: %10ld (%10f)\n", i, histogram[i], (float)histogram[i]/sum * 100.0);
    }
}


int
main(int argc, char **argv)
{
    int i, n;
    double d;
    unsigned char *bp;
    long num_pixels = 0L ;
    long sum = 0L , partial_sum = 0L ;
    int max = -1, min = 256 , mode = 0 , median = 0 ;
    double mean, var = 0.0 , skew = 0.0 ;
    FILE *fp;

    bu_setprogname(argv[0]);

    setmode(fileno(stdin), O_BINARY);
    setmode(fileno(stdout), O_BINARY);

    if (BU_STR_EQUAL(argv[1], "-h") || BU_STR_EQUAL(argv[1], "-?"))
	bu_exit(1, "%s", Usage);

    /* check for verbose flag */
    if (argc > 1 && BU_STR_EQUAL(argv[1], "-v")) {
	verbose++;
	argv++;
	argc--;
    }

    /* check for optional input file */
    if (argc > 1) {
	if ((fp = fopen(argv[1], "rb")) == 0) {
	    bu_exit(1, "bwstat: can't open \"%s\"\n", argv[1]);
	}
	argv++;
	argc--;
    } else
	fp = stdin;

    /* check usage */
    if (argc > 1 || isatty(fileno(fp))) {
	bu_exit(1, "%s", Usage);
    }

    /*
     * Build the histogram. (num_pixels initialized to 0)
     */
    while ((n = fread(buf, sizeof(*buf), IBUFSIZE, fp)) > 0) {
	num_pixels += n;
	bp = &buf[0];
	for (i = 0; i < n; i++)
	    bin[ *bp++ ]++;
    }

    /*
     * Find sum, min, max, mode. (sum and mode initialized to 0;
     * min initialized to 256; max initialized to -1)
     */
    for (i = 0; i < 256; i++) {
	if (bin[i] != 0) {
	    sum += i * bin[i];
	    /* no "else" between the next 2 statements,
	     * because both min and max are changed the 1st
	     * time this block runs during the present run.
	     */
	    if (i < min) min = i;
	    if (i > max) max = i;
	}
	if (bin[i] > bin[mode]) {
	    mode = i;
	}
    }
    mean = (double)sum/(double)num_pixels;

    /*
     * Now do a second pass to compute median,
     * variance and skew. (median and partial_sum initialized to 0;
     * var and skew initialized to 0.0)
     */
    for (i = 0; i < 256; i++) {
	if (partial_sum < sum/2.0) {
	    partial_sum += i * bin[i];
	    median = i;
	}
	d = (double)i - mean;
	var += bin[i] * d * d;
	skew += bin[i] * d * d * d;
    }
    var /= (double)num_pixels;
    skew /= (double)num_pixels;

    /*
     * Display the results.
     */
    printf("Pixels  %14ld (%.0f x %.0f)\n", num_pixels,
	   sqrt((double)num_pixels), sqrt((double)num_pixels));
    printf("Min     %14d\n", min);
    printf("Max     %14d\n", max);
    printf("Mode    %14d (%ld pixels)\n", mode, bin[mode]);
    printf("Median  %14d\n", median);
    printf("Mean    %14.3f\n", mean);
    printf("s.d.    %14.3f\n", sqrt(var));
    printf("Var     %14.3f\n", var);
    printf("Skew    %14.3f\n", skew);

    if (verbose)
	show_hist(bin, sum);

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
