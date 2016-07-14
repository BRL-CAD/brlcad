/*                     P I X F I E L D S . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2016 United States Government as represented by
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
/** @file util/pixfields.c
 *
 * pixfields takes two input pictures and extracts field 1 from the
 * first pix file and field 2 comes from the second pix file.  This is
 * useful for creating field-by-field animation for NTSC video
 * display.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <string.h>
#include "bio.h"

#include "bu/getopt.h"
#include "bu/log.h"
#include "bu/malloc.h"


#define DEFAULT_WIDTH 512

size_t width = DEFAULT_WIDTH;
size_t height = DEFAULT_WIDTH;
int verbose = 0;

char *file_name;
FILE *fldonefp, *fldtwofp;

char usage[] = "\
Usage: pixfields [-v]\n\
	[-s squaresize] [-w width] [-n height]\n\
	 field1.pix field2.pix > file.pix\n";

int
get_args(int argc, char **argv)
{
    int c;

    while ((c = bu_getopt(argc, argv, "vw:n:s:h?")) != -1) {
	switch (c) {
	    case 'v':
		verbose++;
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
	    default:		/* '?' 'h' */
		return 0;
	}
    }

    if ((argc == 1) && isatty(fileno(stdout)))
	return 0;

    if (argc - bu_optind <= 1) {
	(void) fprintf(stderr,"pixfields: must supply two file names\n");
	return 0;
    }

    if ((fldonefp = fopen(argv[bu_optind], "r")) == NULL) {
	fprintf(stderr,
		"pixfields: cannot open \"%s\" for reading\n",argv[bu_optind]);
	return 0;
    }

    if ((fldtwofp = fopen(argv[++bu_optind], "r")) == NULL) {
	fprintf(stderr,
		"pixfields: cannot open \"%s\" for reading\n",argv[bu_optind]);
	return 0;
    }

    if (isatty(fileno(stdout))) {
	fprintf(stderr,"pixfields: must redirect standard output\n");
	return 0;
    }

    if (argc > ++bu_optind)
	fprintf(stderr, "pixfields: excess argument(s) ignored\n");

    return 1;		/* OK */
}


int
main(int argc, char **argv)
{
    char *line1;
    char *line2;
    int line_number;
    size_t ret;

    if (!get_args(argc, argv)) {
	fputs(usage, stderr);
	bu_exit (1, NULL);
    }

    line1 = (char *) malloc(width*3+1);
    line2 = (char *) malloc(width*3+1);

    line_number = 0;
    for (;;) {
	ret = fread(line1, 3*sizeof(char), width, fldonefp);
	if (ret != width)
	    break;
	ret = fread(line2, 3*sizeof(char), width, fldtwofp);
	if (ret != width) {
	    fprintf(stderr, "pixfields: premature EOF on 2nd file?\n");
	    bu_exit (2, NULL);
	}
	if ((line_number & 1) == 0) {
	    if (fwrite(line1, 3*sizeof(char), width, stdout) != width) {
		perror("fwrite line1");
		bu_exit (1, NULL);
	    }
	} else {
	    if (fwrite(line2, 3*sizeof(char), width, stdout) != width) {
		perror("fwrite line2");
		bu_exit (1, NULL);
	    }
	}
	line_number++;
    }

    bu_free(line2, "line2 alloc from malloc");

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
