/*                       P I X - P P M . C
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
/** @file util/pix-ppm.c
 *
 * convert BRL .pix files to ppm
 *
 * Pixels run left-to-right, from the bottom of the screen to the top.
 *
 */

#include "common.h"

#include <stdlib.h>
#include "bio.h"

#include "bu.h"
#include "vmath.h"
#include "bn.h"
#include "fb.h"


static int autosize = 0;		/* !0 to autosize input */

static int fileinput = 0;		/* file of pipe on input? */
static char *file_name = (char *)NULL;
static FILE *infp = (FILE *)NULL;
static FILE *outfp = (FILE *)NULL;

static int pixbytes = 3;


int
get_args(int argc, char *argv[], long *width, long *height)
{
    int c;

    while ((c = bu_getopt(argc, argv, "a#:s:w:n:o:h?")) != -1) {
	switch (c) {
	    case '#':
		pixbytes = atoi(bu_optarg);
		break;
	    case 'a':
		autosize = 1;
		break;
	    case 's':
		/* square file size */
		*height = *width = atol(bu_optarg);
		autosize = 0;
		break;
	    case 'w':
		*width = atol(bu_optarg);
		autosize = 0;
		break;
	    case 'n':
		*height = atol(bu_optarg);
		autosize = 0;
		break;
	    case 'o': {
		outfp = fopen(bu_optarg, "w+");
		if (outfp == (FILE *)NULL) {
		    bu_exit(1, "%s: cannot open \"%s\" for writing\n", bu_getprogname(), bu_optarg);
		}
		break;
	    }

	    case '?':
	    case 'h':
	    default:		/* '?' */
		return 0;
	}
    }

    if (bu_optind >= argc) {
	if (isatty(fileno(stdin)))
	    return 0;
	file_name = "-";
    } else {
	file_name = argv[bu_optind];
	if ((infp = fopen(file_name, "r")) == NULL) {
	    perror(file_name);
	    bu_exit(1, "%s: cannot open \"%s\" for reading\n", bu_getprogname(), file_name);
	}
	fileinput++;
    }

    if (isatty(fileno(infp))) {
	bu_log("ERROR: %s will not read pix data from a tty\n", bu_getprogname());
	return 0; /* usage */
    }
    if (isatty(fileno(outfp))) {
	bu_exit(0, "ERROR: %s will not write ppm data to a tty\n", bu_getprogname());
    }
    if (argc > ++bu_optind)
	bu_log("%s: excess argument(s) ignored\n", bu_getprogname());

    return 1;		/* OK */
}


void
write_ppm(FILE *fp, char *data, long width, long height, int bytes_per_pixel)
{
    int i;
    char *row;
    size_t ret;

    if (bytes_per_pixel == 1) {
	/* PGM magic number */
	fprintf(fp, "P2\n");
    } else {
	/* PPM magic number */
	fprintf(fp, "P6\n");
    }

    /* width height */
    fprintf(fp, "%lu %lu\n", (long unsigned)width, (long unsigned)height);

    /* maximum color component value */
    fprintf(fp, "255\n");
    fflush(fp);

    /*
     * now write them out in the right order, 'cause the
     * input is upside down.
     */

    for (i = 0; i < height; i++) {
	row = data + (height-1 - i) * width * bytes_per_pixel;
	ret = fwrite(row, 1, width * bytes_per_pixel, fp);
	if (ret < (size_t)width * bytes_per_pixel)
	    perror("fwrite");
    }

}


int
main(int argc, char *argv[])
{
    char *scanbuf;

    long int file_width = 512L; /* default input width */
    long int file_height = 512L; /* default input height */

    char usage[] = "Usage: pix-ppm [-a] [-#bytes] [-w file_width] [-n file_height]\n\
	[-s square_file_size] [-o file.ppm] [file.pix] [> file.ppm]";

    long size;

    bu_setprogname(argv[0]);

    /* important to store these before calling get_args().  they're
     * also not necessarily constants so have to set here instead of
     * with the declaration.
     */
    infp = stdin;
    outfp = stdout;

    if (!get_args(argc, argv, &file_width, &file_height)) {
	bu_exit (1, "%s\n", usage);
    }

    size = file_width * pixbytes;

    /* autosize input? */
    if (fileinput && autosize) {
	size_t w, h;
	if (fb_common_file_size(&w, &h, file_name, pixbytes)) {
	    file_width = (long)w;
	    file_height = (long)h;
	} else {
	    bu_log("%s: unable to autosize\n", bu_getprogname());
	}
    }

    /*
     * gobble up the bytes
     */
    scanbuf = bu_malloc(size, "scanbuf");
    if (fread(scanbuf, 1, size, infp) == 0) {
	bu_exit (1, "%s: Short read\n", bu_getprogname());
    }

    write_ppm(outfp, scanbuf, file_width, file_height, pixbytes);

    /* user requested separate file and redirected output */
    if (!isatty(fileno(stdout)) && outfp != stdout) {
	write_ppm(stdout, scanbuf, file_width, file_height, pixbytes);
    }

    bu_free(scanbuf, "scanbuf");
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
