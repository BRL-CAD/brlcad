/*                      M S R A N D O M . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2006 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this file; see the file named COPYING for more
 * information.
 */
/** @file msrandom.c
 * Generate a random number between the two values given. The number can be
 * uniform across the entire range or it can be a gaussian distrubution
 * around the center of the range (or a named center.)
 *
 *  Author -
 *	Christopher T. Johnson - 94/02/06
 *
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5068  USA
 */
#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#ifdef HAVE_UNISTD_H
#  include <unistd.h>
#endif

#include "machine.h"
#include "vmath.h"
#include "bu.h"
#include "bn.h"


struct bn_gauss *gp;
struct bn_unif *up;

int
main(int argc, char **argv)
{
	extern int bu_optind;
	extern char *bu_optarg;

	int seed = getpid();
	int high, low;
	double  center = 0;
	int verbose = 0;
	int gauss = 0;
	int uniform = 0;
	int cdone = 0;
	int c;

	while ((c = bu_getopt(argc,argv,"vugs:c:")) != EOF ) {
		switch(c) {
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
			fprintf(stderr,"msrandom [-ugv] [ -s seed] [-c center ] low high \n");
			exit(1);
		}
	}
	if (! gauss && !uniform) uniform = 1;
	if (gauss && uniform) {
		fprintf(stderr,"msrandom [-ugv] [ -s seed] [-c center ] low high \n");
		fprintf(stderr, "\tOnly one of gaussian or uniform may be used.\n");
		exit(1);
	}
	if (argc - bu_optind != 2) {
		fprintf(stderr,"msrandom [-ugv] [ -s seed] [-c center ] low high \n");
		fprintf(stderr,"\tLow High must be given.\n");
		exit(1);
	}
	low = atoi(argv[bu_optind]);
	high = atoi(argv[bu_optind+1]);
	if (!cdone) {
		center = ((double)(high + low)) / 2.0;
	}
	if (verbose) {
		fprintf(stderr,"msrandom: seed=%d %s %d %f %d\n",
		    seed, (gauss) ? "Gauss" : "Uniform", low, center, high);
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
		fprintf(stdout,"%d\n", (int)tmp);
	} else {
		double tmp;
		up = bn_unif_init(seed, 0);
		tmp = high-low + 1.0;
		tmp*=BN_UNIF_DOUBLE(up)+0.5;
		fprintf(stdout,"%d\n", low +(int)tmp);
	}
	return 0;
}


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
