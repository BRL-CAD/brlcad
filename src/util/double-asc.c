/*                    D O U B L E - A S C . C
 * BRL-CAD
 *
 * Copyright (c) 1996-2011 United States Government as represented by
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
/** @file util/double-asc.c
 *
 * Take a stream of IEEE doubles and make them ASCII
 *
 */

#include "common.h"

#include <stdlib.h>
#include <string.h>
#include "bio.h"

#include "vmath.h"
#include "bu.h"
#include "bn.h"
#include "fb.h"


#define OPT_STRING "acf:hs:n:w#:?"


static char *file_name;
static char *format = 0;
static int infd;

static int fileinput = 0;		/* Input from file (vs. pipe)? */
static int autosize = 0;		/* Try to autosize input? */

static long int file_width = 512L;	/* default input width */
static long int file_height = 512L;	/* default input height */

static int make_cells = 0;		/* Insert cell coords in output? */
static int d_per_l = 1;		/* doubles per line of output */


void print_usage (void)
{
    bu_exit(1, "Usage: 'double-asc %s\n%s [file.d]'\n",
	    "[-{ah}] [-s squaresize] [-w width] [-n height]",
	    "                   [-c] [-f format] [-# depth]");
}


int
get_args(int argc, char **argv)
{
    int ch;

    while ((ch = bu_getopt(argc, argv, OPT_STRING)) != -1) {
	switch (ch) {
	    /*
	     * BRL-CAD image-size options
	     */
	    case 'a':
		autosize = 1;
		break;
	    case 'h':
		/* high-res */
		file_height = file_width = 1024L;
		autosize = 0;
		break;
	    case 's':
		/* square file size */
		file_height = file_width = atol(bu_optarg);
		autosize = 0;
		break;
	    case 'n':
		file_height = atol(bu_optarg);
		autosize = 0;
		break;
	    case 'w':
		file_width = atol(bu_optarg);
		autosize = 0;
		break;
		/*
		 * application-specific options
		 */
	    case 'c':
		make_cells = 1;
		break;
	    case 'f':
		if (format != 0)
		    bu_free(format, "format_string");
		format = (char *)bu_malloc(strlen(bu_optarg)+1, "format string");
		bu_strlcpy(format, bu_optarg, strlen(bu_optarg)+1);
		break;
	    case '#':
		d_per_l = atoi(bu_optarg);
		break;
	    case '?':
	    default:
		print_usage();
	}
    }
    if (format == 0)
	format = " %g";

    /*
     * Establish the input stream
     */
    switch (argc - bu_optind) {
	case 0:
	    file_name = "stdin";
	    infd = 0;
	    break;
	case 1:
	    file_name = argv[bu_optind++];
	    if ((infd = open(file_name, O_RDONLY)) == -1)
		bu_exit (1, "Cannot open file '%s'\n", file_name);
	    fileinput = 1;
	    break;
	default:
	    print_usage();
    }

    if (argc > ++bu_optind) {
	print_usage();
    }

    return 1;		/* OK */
}


int
main (int argc, char **argv)
{
    unsigned char *buffer;
    unsigned char *bp;
    double *value;
    int bufsiz;		/* buffer size (in bytes) */
    int l_per_b;	/* buffer size (in output lines) */
    int line_nm;	/* number of current line */
    int num;		/* number of bytes read */
    int i;
    int row, col;	/* coords within input stream */

    if (!get_args(argc, argv)) {
	print_usage();
    }

    /* autosize input? */
    if (fileinput && autosize) {
	size_t w, h;

	if (fb_common_file_size(&w, &h, file_name, d_per_l * 8)) {
	    file_width = (long)w;
	    file_height = (long)h;
	} else
	    bu_log("double-asc: unable to autosize\n");
    }
    bu_log("OK, file is %ld wide and %ld high\n", file_width, file_height);

    /*
     * Choose an input-buffer size as close as possible to 64 kbytes,
     * while still an integral multiple of the size of an output line.
     */
    l_per_b = ((1 << 16) / (d_per_l * 8));
    bufsiz = l_per_b * (d_per_l * 8);

    buffer = (unsigned char *) bu_malloc(bufsiz, "char buffer");
    value = (double *) bu_malloc(d_per_l * 8, "doubles");
    col = row = 0;
    while ((num = read(infd, buffer, bufsiz)) > 0) {
	bp = buffer;
	l_per_b = num / (d_per_l * 8);
	for (line_nm = 0; line_nm < l_per_b; ++line_nm) {
	    if (make_cells)
		printf("%d %d", col, row);
	    ntohd((unsigned char *)value, bp, d_per_l);
	    bp += d_per_l * 8;
	    for (i = 0; i < d_per_l; ++i)
		printf(format, value[i]);
	    printf("\n");
	    if (++col % file_width == 0) {
		col = 0;
		++row;
	    }
	}
    }
    if (num < 0) {
	perror("double-asc");
	bu_exit (1, NULL);
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
