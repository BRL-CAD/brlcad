/*                        R A N D O M . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2011 United States Government as represented by
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
/** @file util/random.c
 *
 * Generate a random number between the two values given. The number
 * can be uniform across the entire range or it can be a gaussian
 * distrubution around the center of the range (or a named center.)
 *
 */

#include "common.h"

#include <stdlib.h>
#include <math.h>
#include "bio.h"

#include "vmath.h"
#include "bu.h"
#include "bn.h"


int
main(int argc, char *argv[])
{
    struct bn_gauss *gp;
    struct bn_unif *up;

    int seed = bu_process_id();
    int high, low;
    double center = 0;
    int verbose = 0;
    int gauss = 0;
    int uniform = 0;
    int cdone = 0;
    int c;
    const char *usage = "[-u] [-g [-c center]] [-s seed] [-v] low high";

    while ((c = bu_getopt(argc, argv, "vugs:c:")) != -1) {
	switch (c) {
	    case 's':
		seed = atoi(bu_optarg);
		break;
	    case 'c':
		center = atoi(bu_optarg);
		cdone = 1;
		break;
	    case 'g':
		gauss = 1;
		uniform = 0;
		break;
	    case 'u':
		uniform = 1;
		gauss = 0;
		break;
	    case 'v':
		verbose = 1;
		break;
	    case '?':
		bu_exit(1, "%s %s\n", argv[0], usage);
	}
    }
    if (! gauss && !uniform) uniform = 1;
    if (gauss && uniform) {
	bu_log("%s %s\n", argv[0], usage);
	bu_exit(1, "\tOnly one of gaussian or uniform may be used.\n");
    }
    if (argc - bu_optind != 2) {
	bu_log("%s %s\n", argv[0], usage);
	bu_exit(1, "\tLow High must be given.\n");
    }
    low = atoi(argv[bu_optind]);
    high = atoi(argv[bu_optind+1]);
    if (!cdone) {
	center = ((double)(high + low)) / 2.0;
    }
    if (verbose) {
	bu_log("%s: seed=%d %s %d %f %d\n", argv[0], seed, (gauss) ? "Gauss" : "Uniform", low, center, high);
    }
    if (gauss) {
	double tmp;
	double max;
	max = center-(double)low;
	if (max < 0) max = -max;
	tmp = (double)high - center;
	if (tmp<0) tmp = -tmp;
	if (tmp > max) max = tmp;
	gp = bn_gauss_init(seed, 0);

	tmp = BN_GAUSS_DOUBLE(gp)/3.0;
	tmp = 0.5 + center + max*tmp;
	if (tmp < low) tmp = low;
	if (tmp > high) tmp = high;
	fprintf(stdout, "%d\n", (int)tmp);
    } else {
	double tmp;
	up = bn_unif_init(seed, 0);
	tmp = high-low + 1.0;
	tmp*=BN_UNIF_DOUBLE(up)+0.5;
	fprintf(stdout, "%d\n", low +(int)tmp);
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
