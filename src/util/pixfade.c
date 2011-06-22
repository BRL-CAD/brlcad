/*                       P I X F A D E . C
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
/** @file util/pixfade.c
 *
 * Fade a pixture
 *
 * pixfade will darken a pix by a certen percentage or do an integer
 * max pixel value.  It runs in two modes, truncate which will cut any
 * channel greater than param to param, and scale which will change
 * a channel to param percent of its orignal value (limited by 0-255)
 *
 * Inputs:
 *	-m	integer max value
 *	-f	fraction to fade
 *	-p	percentage of fade (fraction = percentage/100)
 *	file	a pixture file.
 *	STDIN	a pixture file if 'file' is not given.
 *
 * Output:
 *	STDOUT	the faded pixture.
 *
 * Calls:
 *	get_args
 *
 * Method:
 *	straight-forward.
 *
 */

#include "common.h"

#include <stdlib.h> /* for atof() */
#include <math.h>
#include "bio.h"

#include "vmath.h"
#include "bu.h"
#include "bn.h"


int
get_args(int argc, char **argv, FILE **inpp, int *max, double *multiplier)
{
    int c;

    while ((c = bu_getopt(argc, argv, "m:p:f:")) != -1) {
	switch (c) {
	    case 'm':
		*max = atoi(bu_optarg);
		if ((*max < 0) || (*max > 255)) {
		    fprintf(stderr, "pixfade: max out of range");
		    bu_exit (1, NULL);
		}
		break;
	    case 'p':
		*multiplier = atof(bu_optarg) / 100.0;
		if (*multiplier < 0.0) {
		    fprintf(stderr, "pixfade: percent is negitive");
		    bu_exit (1, NULL);
		}
		break;
	    case 'f':
		*multiplier = atof(bu_optarg);
		if (*multiplier < 0.0) {
		    fprintf(stderr, "pixfade: fraction is negitive");
		    bu_exit (1, NULL);
		}
		break;

	    default:		/* '?' */
		return 0;
	}
    }

    if (bu_optind >= argc) {
	if (isatty(fileno(stdin))) {
	    fprintf(stderr, "pixfade: stdin is a tty\n");
	    return 0;
	}
	*inpp = stdin;
    } else {
	*inpp = fopen(argv[bu_optind], "r");
	if (*inpp == NULL) {
	    (void)fprintf(stderr,
			  "pixfade: cannot open \"%s\" for reading\n",
			  argv[bu_optind]);
	    return 0;
	}
    }

    if (argc > ++bu_optind)
	(void)fprintf(stderr, "pixfade: excess argument(s) ignored\n");

    if (isatty(fileno(stdout))) {
	fprintf(stderr, "pixfade: stdout is a tty\n");
	return 0;
    }

    return 1;		/* OK */
}


int
main(int argc, char **argv)
{
    static char usage[] = "\
Usage: pixfade [-m max] [-p percent] [-f fraction] [pix-file]\n";

    FILE *inp = NULL;
    int max = 255;
    double multiplier = 1.0;

    float *randp;
    struct color_rec {
	unsigned char red, green, blue;
    } cur_color;

    bn_rand_init(randp, 0);

    if (!get_args(argc, argv, &inp, &max, &multiplier)) {
	(void)fputs(usage, stderr);
	bu_exit (1, NULL);
    }

    /* fprintf(stderr, "pixfade: max = %d, multiplier = %f\n", max, multiplier); */

    for (;;) {
	double t;
	size_t ret;

	ret = fread(&cur_color, 1, 3, inp);
	if (ret != 3) break;
	if (feof(inp)) break;

	t = cur_color.red * multiplier + bn_rand_half(randp);
	if (t > max)
	    cur_color.red   = max;
	else
	    cur_color.red = t;

	t = cur_color.green * multiplier + bn_rand_half(randp);
	if (t > max)
	    cur_color.green = max;
	else
	    cur_color.green = t;

	t = cur_color.blue * multiplier + bn_rand_half(randp);
	if (t > max)
	    cur_color.blue  = max;
	else
	    cur_color.blue = t;

	ret = fwrite(&cur_color, 1, 3, stdout);
	if (ret < 3)
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
