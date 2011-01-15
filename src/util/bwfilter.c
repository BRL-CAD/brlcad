/*                      B W F I L T E R . C
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
/** @file bwfilter.c
 *
 * Filters a black and white file with an arbitrary 3x3 kernel.
 * Leaves the outer rows untouched.  Allows an alternate divisor and
 * offset to be given.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <string.h>
#include "bio.h"

#include "bu.h"


#define MAXLINE (8*1024)
#define DEFAULT_WIDTH 512
unsigned char line1[MAXLINE], line2[MAXLINE], line3[MAXLINE], obuf[MAXLINE];
unsigned char *top, *middle, *bottom, *temp;

/* The filter kernels */
struct kernels {
    char *name;
    char *uname;		/* What is needed to recognize it */
    int kern[9];
    int kerndiv;	/* Divisor for kernel */
    int kernoffset;	/* To be added to result */
} kernel[] = {
    { "Low Pass", "lo", {3, 5, 3, 5, 10, 5, 3, 5, 3}, 42, 0 },
    { "Laplacian", "la", {-1, -1, -1, -1, 8, -1, -1, -1, -1}, 16, 128 },
    { "High Pass", "hi", {-1, -2, -1, -2, 13, -2, -1, -2, -1}, 1, 0 },
    { "Horizontal Gradiant", "hg", {1, 0, -1, 1, 0, -1, 1, 0, -1}, 6, 128},
    { "Vertical Gradient", "vg", {1, 1, 1, 0, 0, 0, -1, -1, -1}, 6, 128 },
    { "Boxcar Average", "b", {1, 1, 1, 1, 1, 1, 1, 1, 1}, 9, 0 },
    { NULL, NULL, {0, 0, 0, 0, 0, 0, 0, 0, 0}, 0, 0 },
};


int *kern;
int kerndiv;
int kernoffset;
int width = DEFAULT_WIDTH;
int height = DEFAULT_WIDTH;
int verbose = 0;
int dflag = 0;	/* Different divisor specified */
int oflag = 0;	/* Different offset specified */

char *file_name;
FILE *infp;

void select_filter(char *str), dousage(void);

char usage[] = "\
Usage: bwfilter [-f type] [-v] [-d div] [-o offset]\n\
	[-s squaresize] [-w width] [-n height] [file.bw] > file.bw\n";

int
get_args(int argc, char **argv)
{
    int c;

    while ((c = bu_getopt(argc, argv, "vf:d:o:w:n:s:")) != EOF) {
	switch (c) {
	    case 'v':
		verbose++;
		break;
	    case 'f':
		select_filter(bu_optarg);
		break;
	    case 'd':
		dflag++;
		kerndiv = atoi(bu_optarg);
		break;
	    case 'o':
		oflag++;
		kernoffset = atoi(bu_optarg);
		break;
	    case 'w':
		width = atoi(bu_optarg);
		break;
	    case 'n':
		height = atoi(bu_optarg);
		break;
	    case 's':
		width = height = atoi(bu_optarg);
		break;
	    default:		/* '?' */
		return 0;
	}
    }

    if (bu_optind >= argc) {
	if (isatty(fileno(stdin)))
	    return 0;
	file_name = "-";
	infp = stdin;
    } else {
	file_name = argv[bu_optind];
	if ((infp = fopen(file_name, "r")) == NULL) {
	    (void)fprintf(stderr,
			  "bwfilter: cannot open \"%s\" for reading\n",
			  file_name);
	    return 0;
	}
    }

    if (isatty(fileno(stdout)))
	return 0;

    if (argc > ++bu_optind)
	(void)fprintf(stderr, "bwfilter: excess argument(s) ignored\n");

    return 1;		/* OK */
}


int
main(int argc, char **argv)
{
    int x, y;
    int value, r1, r2, r3;
    int max, min;
    size_t ret;

    /* Select Default Filter (low pass) */
    select_filter("low");

    if (!get_args(argc, argv)) {
	dousage();
	bu_exit (1, NULL);
    }

    if (width > MAXLINE) {
	fprintf(stderr, "bwfilter:  limited to scanlines of %d\n", MAXLINE);
	bu_exit (1, NULL);
    }

    /*
     * Read in bottom and middle lines.
     * Write out bottom untouched.
     */
    bottom = &line1[0];
    middle = &line2[0];
    top    = &line3[0];
    ret = fread(bottom, sizeof(char), width, infp);
    if (ret == 0)
	perror("fread");

    ret = fread(middle, sizeof(char), width, infp);
    if (ret == 0)
	perror("fread");

    ret = fwrite(bottom, sizeof(char), width, stdout);
    if (ret == 0)
	perror("fwrite");

    if (verbose) {
	for (x = 0; x < 11; x++)
	    fprintf(stderr, "kern[%d] = %d\n", x, kern[x]);
    }

    max = 0;
    min = 255;

    for (y = 1; y < height-1; y++) {
	/* read in top line */
	ret = fread(top, sizeof(char), width, infp);
	if (ret == 0)
	    perror("fread");

	obuf[0] = middle[0];
	/* Filter a line */
	for (x = 1; x < width-1; x++) {
	    r1 = top[x-1] * kern[0] + top[x] * kern[1] + top[x+1] * kern[2];
	    r2 = middle[x-1] * kern[3] + middle[x] * kern[4] + middle[x+1] * kern[5];
	    r3 = bottom[x-1] * kern[6] + bottom[x] * kern[7] + bottom[x+1] * kern[8];
	    value = (r1+r2+r3) / kerndiv + kernoffset;
	    if (value > max) max = value;
	    if (value < min) min = value;
	    if (verbose && (value > 255 || value < 0)) {
		fprintf(stderr, "Value %d\n", value);
		fprintf(stderr, "r1=%d, r2=%d, r3=%d\n", r1, r2, r3);
	    }
	    if (value < 0)
		obuf[x] = 0;
	    else if (value > 255)
		obuf[x] = 255;
	    else
		obuf[x] = value;
	}
	obuf[width-1] = middle[width-1];
	ret = fwrite(obuf, sizeof(char), width, stdout);
	if (ret == 0)
	    perror("fwrite");

	/* Adjust row pointers */
	temp = bottom;
	bottom = middle;
	middle = top;
	top = temp;
    }
    /* write out last line untouched */
    ret = fwrite(top, sizeof(char), width, stdout);
    if (ret == 0)
	perror("fwrite");

    /* Give advise on scaling factors */
    if (verbose)
	fprintf(stderr, "Max = %d,  Min = %d\n", max, min);

    return 0;
}


/*
 * S E L E C T _ F I L T E R
 *
 * Looks at the command line string and selects a filter
 * based on it.
 */
void
select_filter(char *str)
{
    int i;

    i = 0;
    while (kernel[i].name != NULL) {
	if (strncmp(str, kernel[i].uname, strlen(kernel[i].uname)) == 0)
	    break;
	i++;
    }

    if (kernel[i].name == NULL) {
	/* No match, output list and exit */
	fprintf(stderr, "Unrecognized filter type \"%s\"\n", str);
	dousage();
	bu_exit (3, NULL);
    }

    /* Have a match, set up that kernel */
    kern = &kernel[i].kern[0];
    if (dflag == 0)
	kerndiv = kernel[i].kerndiv;
    if (oflag == 0)
	kernoffset = kernel[i].kernoffset;
}


void
dousage(void)
{
    int i;

    fputs(usage, stderr);
    i = 0;
    while (kernel[i].name != NULL) {
	fprintf(stderr, "%-10s%s\n", kernel[i].uname, kernel[i].name);
	i++;
    }
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
