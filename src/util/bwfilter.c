/*                      B W F I L T E R . C
 * BRL-CAD
 *
 * Copyright (c) 1986-2014 United States Government as represented by
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
/** @file util/bwfilter.c
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
#include "icv.h"
#include "vmath.h"

/* The filter kernels */
struct kernels {
    char *name;
    char *uname;		/* What is needed to recognize it */
    int kern[9];
    int kerndiv;	/* Divisor for kernel */
    int kernoffset;	/* To be added to result */
    ICV_FILTER filter;
} kernel[] = {
    { "Low Pass", "lo", {3, 5, 3, 5, 10, 5, 3, 5, 3}, 42, 0, ICV_FILTER_LOW_PASS },
    { "Laplacian", "la", {-1, -1, -1, -1, 8, -1, -1, -1, -1}, 16, 128, ICV_FILTER_LAPLACIAN },
    { "High Pass", "hi", {-1, -2, -1, -2, 13, -2, -1, -2, -1}, 1, 0, ICV_FILTER_HIGH_PASS },
    { "Horizontal Gradient", "hg", {1, 0, -1, 1, 0, -1, 1, 0, -1}, 6, 128, ICV_FILTER_HORIZONTAL_GRAD },
    { "Vertical Gradient", "vg", {1, 1, 1, 0, 0, 0, -1, -1, -1}, 6, 128, ICV_FILTER_VERTICAL_GRAD },
    { "Boxcar Average", "b", {1, 1, 1, 1, 1, 1, 1, 1, 1}, 9, 0, ICV_FILTER_BOXCAR_AVERAGE },
    { NULL, NULL, {0, 0, 0, 0, 0, 0, 0, 0, 0}, 0, 0, ICV_FILTER_NULL },
};

int kerndiv;
int kernoffset;
int *kern;
ICV_FILTER filter_type;
int kernel_index;
int inx = 512;
int iny = 512;   /* Default Width */
int verbose = 0;
int dflag = 0;	/* Different divisor specified */
int oflag = 0;	/* Different offset specified */

void select_filter(char *str), dousage(void);

char usage[] = "\
Usage: bwfilter [-f type] [-v] [-d div] [-O offset]\n\
	[-s squaresize] [-w width] [-n height] [-o out_file.bw] [file.bw] > out_file.bw\n";

char *in_file = NULL;
char *out_file = NULL;

/*
 * S E L E C T _ F I L T E R
 *
 * Looks at the command line string and selects a filter
 * based on it.
 */
void
select_filter(char *str)
{
    kernel_index = 0;
    while (kernel[kernel_index].name != NULL) {
	if (bu_strncmp(str, kernel[kernel_index].uname, strlen(kernel[kernel_index].uname)) == 0)
	    break;
	kernel_index++;
    }

    if (kernel[kernel_index].name == NULL) {
	/* No match, output list and exit */
	fprintf(stderr, "Unrecognized filter type \"%s\"\n", str);
	dousage();
	bu_exit (3, NULL);
    }
    kern = kernel[kernel_index].kern;
    filter_type = kernel[kernel_index].filter;
}

void
dousage(void)
{
    int i;
    fputs(usage, stderr);
    fputs("Possible arguments for -f (type):\n", stderr);
    i = 0;
    while (kernel[i].name != NULL) {
	fprintf(stderr, "  %-10s%s\n", kernel[i].uname, kernel[i].name);
	i++;
    }
}

int
get_args(int argc, char **argv)
{
    int c;

    while ((c = bu_getopt(argc, argv, "vf:d:O:w:n:s:h?")) != -1) {
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
		if (ZERO(kerndiv)) {
		    bu_log("Bad argument for kerndiv\n");
		    return 1;
		}
		break;
	    case 'O':
		oflag++;
		kernoffset = atoi(bu_optarg);
		break;
	    case 'w':
		inx = atoi(bu_optarg);
		break;
	    case 'o':
		out_file = bu_optarg;
		break;
	    case 'n':
		iny = atoi(bu_optarg);
		break;
	    case 's':
		inx = iny = atoi(bu_optarg);
		break;
	    default:		/* '?' */
		return 0;
	}
    }
    if (bu_optind >= argc) {
	if (isatty(fileno(stdin))) {
	    return 0;
	}
    } else {
	in_file = argv[bu_optind];
	bu_optind++;
	return 1;
    }

    if (!isatty(fileno(stdout)) && out_file!=NULL) {
	return 0;
    }

    if (argc > ++bu_optind) {
	bu_log("bwrfilter: excess argument(s) ignored\n");
    }

    return 1;
}


int
main(int argc, char **argv)
{
    icv_image_t *img;
    double *max_d = NULL, *min_d = NULL; /* return values from min and max */
    int max, min;
    int x;
    /* Select Default Filter (low pass) */
    select_filter("low");

    if (!get_args(argc, argv)) {
	dousage();
	bu_exit (1, NULL);
    }
    img = icv_read(in_file, ICV_IMAGE_BW, inx, iny);
    if (img == NULL)
	return 1;

    icv_filter(img, filter_type);

    /* Correct the image as per the input offset and */
    if (oflag | dflag) {
        icv_add_val(img, -ICV_CONV_8BIT(kernel[kernel_index].kernoffset));

        if (dflag) {
            if (ZERO(kerndiv))
            icv_multiply_val(img, ICV_CONV_8BIT(kernel[kernel_index].kerndiv/kerndiv));
        }

        if (oflag)
            icv_add_val(img, ICV_CONV_8BIT(kernoffset));
        else
            icv_add_val(img, ICV_CONV_8BIT(kernel[kernel_index].kernoffset));
    }

    if (verbose) {
	for (x = 0; x < 11; x++)
	    bu_log("kern[%d] = %d\n", x, kern[x]);

	max_d = icv_max(img);
	min_d = icv_min(img);
	max = (int) (*max_d * 255.0);
	min = (int) (*min_d * 255.0);
	bu_log("\n\tMax = %d, Min = %d\n",max, min);
    }
    bu_free(min_d, "max value");
    bu_free(max_d, "min values");

    icv_write(img, out_file, ICV_IMAGE_BW);
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
