/*                        I S T A T S . C
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
/** @file istats.c
 *
 * gather statistics on a set of short integers.
 *
 * Options
 * h help
 */

#include "common.h"

#include <stdlib.h>
#include <string.h>
#include "bio.h"

#include "bu.h"


/*
 * U S A G E --- tell user how to invoke this program, then exit
 */
void usage(const char *progname)
{
    bu_exit(1, "Usage: %s [ file ]\n", progname);
}

/*
 * P A R S E _ A R G S --- Parse through command line flags
 */
int parse_args(int ac, char **av, char *progname)
{
    int c;

    if (!(progname=strrchr(*av, '/')))
	progname = *av;

    /* Turn off bu_getopt's error messages */
    bu_opterr = 0;

    /* get all the option flags from the command line */
    while ((c=bu_getopt(ac, av, "h")) != -1)
	switch (c) {
	    case '?'	:
	    case 'h'	:
	    default		: usage(progname); break;
	}

    return bu_optind;
}

void comp_stats(FILE *fd)
{
    short *buffer=(short *)NULL;
    short min, max;
    double stdev, sum, sum_sq, num, sqrt(double);
    int count;
    int i;


    buffer = (short *)bu_calloc(10240, sizeof(short), "buffer");

    stdev = sum = sum_sq = count = num = 0.0;
    min = 32767;
    max = -32768;

    while ((count=fread((void *)buffer, sizeof(short), 10240, fd))) {
	for (i=0; i < count; ++i) {
	    sum += (double)buffer[i];
	    sum_sq += (double)(buffer[i] * buffer[i]);
	    if (buffer[i] > max) max = buffer[i];
	    if (buffer[i] < min) min = buffer[i];
	}
	num += (double)count;
    }

    stdev = sqrt(((num * sum_sq) - (sum*sum)) / (num * (num-1)));

    (void)printf("   Num: %g\n   Min: %hd\n   Max: %hd\n   Sum: %g\n  Mean: %g\nSStdev: %g\n",
		 num, min, max, sum, sum/num, stdev);

    bu_free(buffer, "buffer");
}


/*
 * M A I N
 *
 * Call parse_args to handle command line arguments first, then
 * process input.
 */
int main(int ac, char *av[])
{
    char *progname = "(noname)";
    int arg_index;

    /* parse command flags
     */
    arg_index = parse_args(ac, av, progname);
    if (arg_index < ac) {
	/* open file of shorts */
	if (freopen(av[arg_index], "r", stdin) == (FILE *)NULL) {
	    perror(av[arg_index]);
	    return -1;
	}
    } else if (isatty((int)fileno(stdin))) {
	usage(progname);
    }

    comp_stats(stdin);

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
