/*			M S R A N D O M . C
 *
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
 *
 *  Distribution Status -
 *	Public Domain, Distribution Unlimitied.
 */
#include "conf.h"

#include <stdio.h>
#include <math.h>

#include "machine.h"
#include "externs.h"		/* For atoi */
#include "vmath.h"
#include "bu.h"
#include "bn.h"

struct bn_gauss *gp;
struct bn_unif *up;

main(argc,argv)
int argc;
char **argv;
{
	extern int optind;
	extern char *optarg;

	int seed = getpid();
	int high, low;
	double  center;
	int verbose = 0;
	int gauss = 0;
	int uniform = 0;
	int cdone = 0;
	int c;

	while ((c = getopt(argc,argv,"vugs:c:")) != EOF ) {
		switch(c) {
		case 's':
			seed = atoi(optarg);
			break;
		case 'c':
			center = atoi(optarg);
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
	if (argc - optind != 2) {
		fprintf(stderr,"msrandom [-ugv] [ -s seed] [-c center ] low high \n");
		fprintf(stderr,"\tLow High must be given.\n");
		exit(1);
	}
	low = atoi(argv[optind]);
	high = atoi(argv[optind+1]);
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
		int bins[21];
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
}
