/*                        D S T A T S . C
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
/** @file dstats.c
 *
 * Compute statistics of double precision floats.
 * Gives min, max, mode, median, mean, s.d., var, and skew.
 *
 */

#include "common.h"

#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "bio.h"

#include "bu.h"

#define IBUFSIZE 1024		/* Max read size */
double buf[IBUFSIZE];		/* Input buffer */

int verbose = 0;


int main(int ac, char *av[])
{
    int i, n;
    long num_values;
    double *bp;
    double sum, sum2;
    double max, min;
    double mean, var;
    FILE *fp;

    /* check for verbose flag */
    if (ac > 1 && BU_STR_EQUAL(av[1], "-v")) {
	verbose++;
	av++;
	ac--;
    }

    /* look for optional input file */
    if (ac > 1) {
	if ((fp = fopen(av[1], "r")) == 0) {
	    bu_exit(1, "dstats: can't open \"%s\"\n", av[1]);
	}
	av++;
	ac--;
    } else
	fp = stdin;

    /* check usage */
    if (ac > 1 || isatty(fileno(fp))) {
	bu_exit(1, "Usage: dstats [-v] [file.doubles]\n");
    }

    /*
     * Find sum, min, max, mode.
     */
    num_values = 0;
    sum = sum2 = 0;
#if defined(HUGE_VAL)
    min = HUGE_VAL;
    max = -HUGE_VAL;
#else
    min = HUGE;
    max = -HUGE;
#endif
    while ((n = fread(buf, sizeof(*buf), IBUFSIZE, fp)) > 0) {
	num_values += n;
	bp = &buf[0];
	for (i = 0; i < n; i++) {
	    sum += *bp;
	    sum2 += *bp * *bp;
	    if (*bp < min)
		min = *bp;
	    if (*bp > max)
		max = *bp;
	    bp++;
	}
    }
    mean = sum/(double)num_values;
    var = sum2/(double)num_values - mean * mean;

    /*
     * Display the results.
     */
    printf("Values  %14ld (%.0f x %.0f)\n", num_values,
	   sqrt((double)num_values), sqrt((double)num_values));
    printf("Min     %14.6g\n", min);
    printf("Max     %14.6g\n", max);
    printf("Mean    %14.6g\n", mean);
    printf("s.d.    %14.6g\n", sqrt(var));
    printf("Var     %14.6g\n", var);

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
