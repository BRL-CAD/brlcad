/*                        R A N D O M . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2021 United States Government as represented by
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
 * distribution around the center of the range (or a named center).
 *
 */

#include "common.h"

#include <stdlib.h>
#include <math.h>
#include "bio.h"

#include "vmath.h"
#include "bu/app.h"
#include "bu/getopt.h"
#include "bu/process.h"
#include "bu/log.h"
#include "bn.h"

const char *usage = "Usage: random [-u] [-g [-c center]] [-s seed] [-v] low high";

void
printusage()
{
	bu_log("%s\n", usage);
}

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

    bu_setprogname(argv[0]);

    while ((c = bu_getopt(argc, argv, "vugs:c:h?")) != -1) {
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
		break;
	    case 'u':
		uniform = 1;
		break;
	    case 'v':
		verbose = 1;
		break;
	    default:
		printusage();
		if (c == 'h' || bu_optopt == '?') {
			bu_exit(1, "\tREQUIRED: low high\n");
		}
		bu_exit(1,NULL);
	}
    }
    if (gauss && uniform) {
	printusage();
	bu_exit(1, "\tOnly one of gaussian or uniform may be used.\n");
    }
    if (argc - bu_optind != 2) {
	printusage();
	bu_exit(1, "\tREQUIRED: low high\n");
    }
    if (gauss == 0 && uniform == 0)
	uniform = 1;
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
	return 0;
    }
    if (uniform) {
	double tmp;
	up = bn_unif_init(seed, 0);
	tmp = high-low + 1.0;
	tmp*=BN_UNIF_DOUBLE(up)+0.5;
	fprintf(stdout, "%d\n", low +(int)tmp);
	return 0;
    }

    return -1;
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
