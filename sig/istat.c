/*	I S T A T . C --- gather statistics on file of shorts
 *
 *	Options
 *	h	help
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif



#include <stdio.h>
#ifdef USE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

#include "machine.h"
#include "externs.h"		/* For getopt */

/* declarations to support use of getopt() system call */
char *options = "h";
char *progname = "(noname)";

/*
 *	U S A G E --- tell user how to invoke this program, then exit
 */
void usage(void)
{
	(void) fprintf(stderr, "Usage: %s [ file ]\n", progname);
	exit(1);
}

/*
 *	P A R S E _ A R G S --- Parse through command line flags
 */
int parse_args(int ac, char **av)
{
	int  c;

	if (!(progname=strrchr(*av, '/')))
		progname = *av;
	
	/* Turn off getopt's error messages */
	opterr = 0;

	/* get all the option flags from the command line */
	while ((c=getopt(ac,av,options)) != EOF)
		switch (c) {
		case '?'	:
		case 'h'	:
		default		: usage(); break;
		}

	return(optind);
}

void comp_stats(FILE *fd)
{
	short *buffer=(short *)NULL;
	short min, max;
	double stdev, sum, sum_sq, num, sqrt(double);
	int count;
	int i;


	buffer = (short *)calloc(10240, sizeof(short));
	if (buffer == (short *)NULL) {
		(void)fprintf(stderr, "%s: cannot allocate buffer\n",
			progname);
		exit(-1);
	}

	stdev = sum = sum_sq = count = num = 0.0;
	min = 32767;
	max = -32768;

	while ( (count=fread((void *)buffer, sizeof(short), 10240, fd)) ) {
		for (i=0 ; i < count ; ++i) {
			sum += (double)buffer[i];
			sum_sq += (double)(buffer[i] * buffer[i]);
			if (buffer[i] > max) max = buffer[i];
			if (buffer[i] < min) min = buffer[i];
		}
		num += (double)count;
	}

	stdev = sqrt( ((num * sum_sq) - (sum*sum)) / (num * (num-1)) );

	(void)printf("   Num: %g\n   Min: %hd\n   Max: %hd\n   Sum: %g\n  Mean: %g\nSStdev: %g\n",
		num, min, max, sum, sum/num, stdev);

}


/*
 *	M A I N
 *
 *	Call parse_args to handle command line arguments first, then
 *	process input.
 */
int main(int ac, char **av)
{
	int arg_index;

	/* parse command flags
	 */
	arg_index = parse_args(ac, av);
	if (arg_index < ac) {
		/* open file of shorts */
		if (freopen(av[arg_index], "r", stdin) == (FILE *)NULL) {
			perror(av[arg_index]);
			return(-1);
		}
	} else if (isatty((int)fileno(stdin))) {
		usage();
	}

	comp_stats(stdin);

	return 0;
}
