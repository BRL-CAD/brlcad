/*                        P I X C M P . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2007 United States Government as represented by
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
 *
 */
/** @file pixcmp.c
 *
 *  Compute the difference between two .pix files.
 *  To establish context, a monochrome image is produced when there
 *  are no differences;  otherwise the channels that differ are
 *  highlighted on differing pixels.
 *
 *  This routine operates on a pixel-by-pixel basis, and thus
 *  is independent of the resolution of the image.
 *
 *  Author -
 *	Charles M. Kennedy
 *	Michael John Muuss
 *	Christopher Sean Morrison
 */
#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef _WIN32
#  include <unistd.h>
#endif

/* exit codes for argument processing errors */
#define OPTS_ERROR 127
#define FILE_ERROR 126

/* exit codes for normal operation */
#define OFF_BY_NONE 0
#define OFF_BY_ONE  1
#define OFF_BY_MANY 2


void
usage(const char *name)
{
    fprintf(stderr, "Usage: %s [OPTIONS] FILE1 [FILE2 [SKIP1 [SKIP2]]]\n%s", name,
	    "Compare two PIX image files pixel by pixel.\n\n"
	    "  -l   List pixel numbers and values for all pixels that differ.\n"
	    "  -i SKIP\n"
	    "       Skip the first SKIP pixels of input (for FILE1 and FILE2)\n"
	    "  -i SKIP1:SKIP2\n"
	    "       Skip the first SKIP1 pixels in FILE1 and SKIP2 pixels in FILE2.\n"
	    "  -s   Silent output.  Only return an exit status.\n\n"
	    "If FILE is `-' or is missing, then input is read from standard input.\n"
	    "Pixel numbers are indexed linearly from zero.\n");
    return;
}


void
handle_i_opt(const char *arg, long *skip1, long *skip2)
{
    const char *endptr = arg;
    if ((arg == NULL) ||
	((skip1 == NULL) && (skip2 == NULL))){
	/* nothing to do */
	return;
    }
    while ((endptr != NULL) && (endptr[0] != ':') && (endptr[0] != '\0')) {
	endptr++;
    }
    if (endptr == arg) {
	/* probably empty string */
	if (skip2) {
	    endptr++;
	    *skip2 = strtol(endptr, NULL, 10);
	}
    } else if ((endptr == NULL) || (endptr[0] == '\0')) {
	/* no : found */
	if (skip2) {
	    *skip2 = strtol(arg, NULL, 10);
	}
	if (skip1) {
	    *skip1 = *skip2;
	}
    } else if (endptr[0] == ':') {
	/* found : */
	if (skip1) {
	    *skip1 = strtol(arg, NULL, 10);
	}
	if (skip2) {
	    endptr++; /* skip over : */
	    *skip2 = strtol(endptr, NULL, 10);
	}
    } else {
	fprintf(stderr, "Unexpected input processing [%s]\n", arg);
	exit(OPTS_ERROR);
    }
}

int
main(int argc, char *argv[])
{
    FILE *f1, *f2;

    long matching;
    long off1;
    long offmany;

    int c;
    int list_pixel_values = 0;
    int silent = 0;
    long f1_skip = 0;
    long f2_skip = 0;

    /* process opts */
    while ((c = getopt(argc, argv, "li:s")) != EOF) {
	switch (c) {
	    case 'l':
		list_pixel_values = 1;
		break;
	    case 'i':
		handle_i_opt(optarg, &f1_skip, &f2_skip);
		argc--; argv++;
		break;
	    case 's':
		silent = 1;
		break;
	    default:
		fprintf(stderr, "\n");
		usage(argv[0]);
		exit(OPTS_ERROR);
	}
	argc--;	argv++;
    }

    /* validate what is left over */
    if( argc < 2 || argc > 5)  {
	fprintf(stderr, "ERROR: incorrect number of arguments provided\n\n");
	usage(argv[0]);
	exit(OPTS_ERROR);
    }

    /* handle optional SKIP1 and SKIP2 following filenames */
    if (argc > 3) {
	char range[64] = {0};
	snprintf(range, 64, "%s%s%s", argc>3?argv[3]:"", argc>4?":":"", argc>4?argv[4]:"");
	handle_i_opt(range, &f1_skip, &f2_skip);
    }

    /* printf("Skip from FILE1: %ld and from FILE2: %ld\n", f1_skip, f2_skip); */

    if (strcmp( argv[1], "-" ) == 0 ) {
	f1 = stdin;
    } else if ((f1 = fopen( argv[1], "r" )) == NULL)  {
	perror(argv[1]);
	exit(FILE_ERROR);
    }
    if ((argc < 3) || (strcmp(argv[2], "-") == 0)) {
	f2 = stdin;
    } else if ((f2 = fopen( argv[2], "r" )) == NULL) {
	perror(argv[2]);
	exit(FILE_ERROR);
    }

    /* skip requested pixels in FILE1 */
    if (fseek(f1, f1_skip * 3, SEEK_SET)) {
	fprintf(stderr, "ERROR: Unable to seek %ld pixels (%ld bytes) in FILE1\n", f1_skip, f1_skip*3);
	perror("FILE1 fseek failure");
	exit(FILE_ERROR);
    }
    
    /* skip requested pixels in FILE2 */
    if (fseek(f2, f2_skip * 3, SEEK_SET)) {
	fprintf(stderr, "ERROR: Unable to seek %ld pixels (%ld bytes) in FILE2\n", f2_skip, f2_skip*3);
	perror("FILE2 fseek failure");
	exit(FILE_ERROR);
    }

    while(1)  {
	register int r1, g1, b1;
	int r2, g2, b2;

	r1 = fgetc( f1 );
	g1 = fgetc( f1 );
	b1 = fgetc( f1 );
	r2 = fgetc( f2 );
	g2 = fgetc( f2 );
	b2 = fgetc( f2 );
	if( feof(f1) || feof(f2) )  break;

	if( r1 != r2 || g1 != g2 || b1 != b2 )  {
	    register int i;

	    /* Highlight differing channels */
	    if( r1 != r2 )  {
		if( (i = r1 - r2) < 0 )  i = -i;
		if( i > 1 )  {
		    offmany++;
		} else {
		    off1++;
		}
	    } else {
		matching++;
	    }
	    if( g1 != g2 )  {
		if( (i = g1 - g2) < 0 )  i = -i;
		if( i > 1 )  {
		    offmany++;
		} else {
		    off1++;
		}
	    } else {
		matching++;
	    }
	    if( b1 != b2 )  {
		if( (i = b1 - b2) < 0 )  i = -i;
		if( i > 1 )  {
		    offmany++;
		} else {
		    off1++;
		}
	    } else {
		matching++;
	    }
	}  else  {
	    /* Common case:  equal.  Give B&W NTSC average */
	    matching += 3;
	}
    }
    if (!silent) {
	fprintf(stderr,
		"pixcmp pixels: %8ld matching, %8ld off by 1, %8ld off by many\n",
		matching, off1, offmany );
    }

    /* If files were of different lengths, that is an error */
    if (feof(f1) != feof(f2)) {
	return FILE_ERROR;
    }

    /* indicate how many differences there were overall */
    if (offmany) {
	return OFF_BY_MANY;
    }
    if (off1) {
	return OFF_BY_ONE;
    }

    /* Success! */
    return OFF_BY_NONE;
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
