/*                       W A V E L E T . C
 * BRL-CAD
 *
 * Copyright (c) 1998-2011 United States Government as represented by
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
/** @file util/wavelet.c
 *
 * This program performs a wavelet transformation on data.
 * Transformations possible are decompositions and reconstructions.
 * Currently, only the Haar wavelet is supported.
 *
 * Options
 * -D decompose
 * -R reconstruct
 * -1 one-dimensional transform
 * -2 two-dimensional transform
 * -# n n-elements/channels per sample (eg 3 for a pix file)
 * -t[cdfils]	data type
 * -D level debug
 * -s squaresize of original image/dataset (power of 2)
 * -R n Restart with average image size n
 * -n number of scanlines
 * -w width of dataset
 * -S level/limit of transform ...
 * -W (size of avg img in transformed data output)
 *
 */

#include "common.h"

#include <stdlib.h>
#include "bio.h"

#include "bu.h"
#include "vmath.h"
#include "bn.h"
#include "fb.h"


#define CHAR	1
#define SHORT	2
#define INT	3
#define LONG	4
#define FLOAT	5
#define DOUBLE	6

#define DECOMPOSE 1
#define RECONSTRUCT -1

/* declarations to support use of bu_getopt() system call */
char *options = "W:S:s:w:n:t:#:D:12drR:";

char *progname = "(noname)";
int img_space=1;
int debug;
size_t width = 512;
size_t height = 512;
size_t channels = 3;
int value_type = CHAR;
int value_size = sizeof(char);
int avg_size = 0;
size_t limit = 0;
int decomp_recon;


/*
 * U S A G E --- tell user how to invoke this program, then exit
 */
void
usage(char *s)
{
    if (s) (void)fputs(s, stderr);

    bu_exit(1,
	    "Usage:\n\
	%s {-d | -r} [-2] [-t datatype] [-# channels]\n\
	[-w width] [-n scanlines] [-s number_of_samples]\n\
	< datastream > wavelets\n", progname);
}


/*
 * P A R S E _ A R G S --- Parse through command line flags
 */
int
parse_args(int ac, char **av)
{
    int c;
    char *strrchr(const char *, int);

    if ((progname=strrchr(*av, '/')))
	progname++;
    else
	progname = *av;


    /* Turn off bu_getopt's error messages */
    bu_opterr = 0;

    /* get all the option flags from the command line */
    while ((c=bu_getopt(ac, av, options)) != -1)
	switch (c) {
	    case '1': img_space=1; break;
	    case '2': img_space=2; break;
	    case 'd': decomp_recon = DECOMPOSE;
		break;
	    case 'r': decomp_recon = RECONSTRUCT;
		break;
	    case 'D': debug=atoi(bu_optarg); break;
	    case 'R': avg_size = atoi(bu_optarg); break;
	    case '#': channels = atoi(bu_optarg);
		break;
	    case 't': {
		switch (*bu_optarg) {
		    case 'c': value_type = CHAR;
			value_size = sizeof(char);
			break;
		    case 'd': value_type = DOUBLE;
			value_size = sizeof(double);
			break;
		    case 'f': value_type = FLOAT;
			value_size = sizeof(float);
			break;
		    case 'i': value_type = INT;
			value_size = sizeof(int);
			break;
		    case 'l': value_type = LONG;
			value_size = sizeof(long);
			break;
		    case 's': value_type = SHORT;
			value_size = sizeof(short);
			break;
		}
		break;
	    }
	    case 'n': height = atoi(bu_optarg); break;
	    case 'w': width = atoi(bu_optarg); break;
	    case 's': width = height = atoi(bu_optarg); break;
	    case 'W': limit = atoi(bu_optarg); break;
	    case 'S': limit = atoi(bu_optarg); break;
	    case '?':
	    case 'h':
	    default	: fprintf(stderr, "Bad or help flag specified %c\n", c);
		usage("");
		break;
	}

    return bu_optind;
}


void
wlt_decompose_1d(void)
{
    size_t ret;
    genptr_t buf, tbuf;
    size_t i, n;
    size_t sample_size;	/* size of data type x #values/sample */
    size_t scanline_size;	/* # bytes in a scanline */

    sample_size = value_size * channels;
    scanline_size = sample_size * width;

    buf = bu_malloc(scanline_size, "wavelet buf");
    tbuf = bu_malloc(scanline_size >> 1, "wavelet buf");

    if (debug)
	fprintf(stderr, "1D decompose:\n\tdatatype_size:%d channels:%lu width:%lu height:%lu limit:%lu\n", value_size, (long unsigned)channels, (long unsigned)width, (long unsigned)height, (long unsigned)limit);


    for (i=0; i < height; i++) {

	n = fread(buf, sample_size, width, stdin);
	if (n != width) {
	    bu_exit(1, "read failed line %zu got %zu not %zu\n", i, n, width);
	}

	switch (value_type) {
	    case DOUBLE:
		bn_wlt_haar_1d_double_decompose(tbuf, buf, width,
						channels, limit);
		break;
	    case FLOAT:
		bn_wlt_haar_1d_float_decompose(tbuf, buf, width,
					       channels, limit);
		break;
	    case CHAR:
		bn_wlt_haar_1d_char_decompose(tbuf, buf, width,
					      channels, limit);
		break;
	    case SHORT:
		bn_wlt_haar_1d_short_decompose(tbuf, buf, width,
					       channels, limit);
		break;
	    case INT:
		bn_wlt_haar_1d_int_decompose(tbuf, buf, width,
					     channels, limit);
		break;
	    case LONG:
		bn_wlt_haar_1d_long_decompose(tbuf, buf, width,
					      channels, limit);
		break;
	}

	ret = fwrite(buf, sample_size, width, stdout);
	if (ret == 0) {
	    perror("fwrite");
	    break;
	}
    }
}


void
wlt_decompose_2d(void)
{
    size_t ret;
    genptr_t buf, tbuf;
    size_t sample_size;
    size_t scanline_size;

    sample_size = value_size * channels;
    scanline_size = sample_size * width;

    buf = bu_malloc(scanline_size * height, "wavelet buf");
    tbuf = bu_malloc(scanline_size, "wavelet buf");

    if (debug)
	fprintf(stderr, "2D decompose:\n\tdatatype_size:%d channels:%lu width:%lu height:%lu limit:%lu\n", value_size, (long unsigned)channels, (long unsigned)width, (long unsigned)height, (long unsigned)limit);


    if (width != height) {
	fprintf(stderr, "Two dimensional decomposition requires square image\n");
	bu_exit(1, "%zu x %zu image specified\n", width, height);
    }

    if (fread(buf, scanline_size, height, stdin) != height) {
	bu_exit(1, "read error getting %zu x %zu bytes\n", scanline_size, height);
    }

    switch (value_type) {
	case DOUBLE:
	    bn_wlt_haar_2d_double_decompose(tbuf, buf, width,
					    channels, limit);
	    break;
	case FLOAT:
	    bn_wlt_haar_2d_float_decompose(tbuf, buf, width,
					   channels, limit);
	    break;
	case CHAR:
	    bn_wlt_haar_2d_char_decompose(tbuf, buf, width,
					  channels, limit);
	    break;
	case SHORT:
	    bn_wlt_haar_2d_short_decompose(tbuf, buf, width,
					   channels, limit);
	    break;
	case INT:
	    bn_wlt_haar_2d_int_decompose(tbuf, buf, width,
					 channels, limit);
	    break;
	case LONG:
	    bn_wlt_haar_2d_long_decompose(tbuf, buf, width,
					  channels, limit);
	    break;
    }
    ret = fwrite(buf, scanline_size, width, stdout);
    if (ret == 0) {
	perror("fwrite");
    }
}


void
wlt_reconstruct_1d(void)
{
    size_t ret;
    genptr_t buf, tbuf;
    size_t i, n;
    size_t sample_size;	/* size of data type x #values/sample */
    size_t scanline_size;	/* # bytes in a scanline */

    sample_size = value_size * channels;
    scanline_size = sample_size * width;

    buf = bu_malloc(scanline_size, "wavelet buf");
    tbuf = bu_malloc(scanline_size >> 1, "wavelet buf");

    if (debug)
	fprintf(stderr, "1D reconstruct:\n\tdatatype_size:%d channels:%lu width:%lu height:%lu limit:%lu\n", value_size, (long unsigned)channels, (long unsigned)width, (long unsigned)height, (long unsigned)limit);


    for (i=0; i < height; i++) {


	n = fread(buf, sample_size, width, stdin);
	if (n != width) {
	    bu_exit(-1, "read failed line %zu got %zu not %zu\n", i, n, width);
	}

	switch (value_type) {
	    case DOUBLE:
		bn_wlt_haar_1d_double_reconstruct(tbuf, buf, width,
						  channels, avg_size, limit);
		break;
	    case FLOAT:
		bn_wlt_haar_1d_float_reconstruct(tbuf, buf, width,
						 channels, avg_size, limit);
		break;
	    case CHAR:
		bn_wlt_haar_1d_char_reconstruct(tbuf, buf, width,
						channels, avg_size, limit);
		break;
	    case SHORT:
		bn_wlt_haar_1d_short_reconstruct(tbuf, buf, width,
						 channels, avg_size, limit);
		break;
	    case INT:
		bn_wlt_haar_1d_int_reconstruct(tbuf, buf, width,
					       channels, avg_size, limit);
		break;
	    case LONG:
		bn_wlt_haar_1d_long_reconstruct(tbuf, buf, width,
						channels, avg_size, limit);
		break;
	}

	ret = fwrite(buf, sample_size, width, stdout);
	if (ret == 0) {
	    perror("fwrite");
	    break;
	}
    }
}


void
wlt_reconstruct_2d(void)
{
    size_t ret;
    genptr_t buf, tbuf;
    size_t sample_size;
    size_t scanline_size;

    sample_size = value_size * channels;
    scanline_size = sample_size * width;

    buf = bu_malloc(scanline_size * height, "wavelet buf");
    tbuf = bu_malloc(scanline_size, "wavelet buf");

    if (debug)
	fprintf(stderr, "2D reconstruct:\n\tdatatype_size:%d channels:%lu width:%lu height:%lu limit:%lu\n", value_size, (long unsigned)channels, (long unsigned)width, (long unsigned)height, (long unsigned)limit);

    if (width != height) {
	fprintf(stderr, "Two dimensional decomposition requires square image\n");
	bu_exit(1, "%zu x %zu image specified\n", width, height);
    }

    if (fread(buf, scanline_size, height, stdin) != height) {
	bu_exit(1, "read error getting %zu x %zu bytes\n", scanline_size, height);
    }

    switch (value_type) {
	case DOUBLE:
	    bn_wlt_haar_2d_double_reconstruct((double *)tbuf, (double *)buf, width,
					      channels, avg_size, limit);
	    break;
	case FLOAT:
	    bn_wlt_haar_2d_float_reconstruct(tbuf, buf, width,
					     channels, avg_size, limit);
	    break;
	case CHAR:
	    bn_wlt_haar_2d_char_reconstruct(tbuf, buf, width,
					    channels, avg_size, limit);
	    break;
	case SHORT:
	    bn_wlt_haar_2d_short_reconstruct(tbuf, buf, width,
					     channels, avg_size, limit);
	    break;
	case INT:
	    bn_wlt_haar_2d_int_reconstruct(tbuf, buf, width,
					   channels, avg_size, limit);
	    break;
	case LONG:
	    bn_wlt_haar_2d_long_reconstruct(tbuf, buf, width,
					    channels, avg_size, limit);
	    break;
    }
    ret = fwrite(buf, scanline_size, width, stdout);
    if (ret == 0) {
	perror("fwrite");
    }
}


/*
 * M A I N
 *
 * Call parse_args to handle command line arguments first, then
 * process input.
 */
int
main(int ac, char **av)
{

    /* parse command flags, and make sure there are arguments
     * left over for processing.
     */
    if (parse_args(ac, av) < ac) usage("Excess arguments ignored.\n");

    if (isatty(fileno(stdout))) usage("Redirect input/output\n");

    if (!value_type)
	usage("Must specify data type\n");

    if (decomp_recon == DECOMPOSE) {
	/* set some defaults */
	if (avg_size == 0) avg_size = width;
	if (limit == 0) limit = 1;

	if (img_space == 1) wlt_decompose_1d();
	else wlt_decompose_2d();
	return 0;
    } else if (decomp_recon == RECONSTRUCT) {
	/* set some defaults */
	if (avg_size == 0) avg_size = 1;
	if (limit == 0) limit = width;

	if (img_space == 1) wlt_reconstruct_1d();
	else wlt_reconstruct_2d();
	return 0;
    }
    usage("must specify either decompose (-d) or reconstruct (-r)\n");
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
