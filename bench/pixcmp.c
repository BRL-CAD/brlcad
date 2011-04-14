/*                        P I X C M P . C
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
 *
 */
/** @file pixcmp.c
 *
 * Compute and summarily report differences between two .pix files.
 *
 * This routine operates on a pixel-by-pixel basis, and thus
 * is independent of the resolution of the image.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <string.h>
#ifdef HAVE_UNISTD_H
#  include <unistd.h>
#endif
#include "bio.h"

#include "bu.h"


/* exit codes for argument processing errors */
#define OPTS_ERROR 127
#define FILE_ERROR 126

/* exit codes for normal operation */
#define OFF_BY_NONE 0
#define OFF_BY_ONE  1
#define OFF_BY_MANY 2


HIDDEN void
usage(const char *name)
{
    const char *unknown = "pixcmp";
    if (!name) {
	name = unknown;
    }
    /* this must be split, ISO C90 has a max static string length of 509 */
    bu_log("Usage: %s [OPTIONS] FILE1 [FILE2 [SKIP1 [SKIP2]]]\n%s", name,
	   "Compare two PIX image files pixel by pixel.\n\n"
	   "  -l   List pixel numbers and values for all pixels that differ.\n"
	   "  -b   Output and process by bytes instead of pixels.\n"
	   "  -i SKIP\n"
	   "       Skip the first SKIP pixels of input (for FILE1 and FILE2)\n");
    bu_log("  -i SKIP1:SKIP2\n"
	   "       Skip the first SKIP1 pixels in FILE1 and SKIP2 pixels in FILE2.\n"
	   "  -s   Silent output.  Only return an exit status.\n\n"
	   "If FILE is `-' or is missing, then input is read from standard input.\n"
	   "If the `-b' option is used, SKIP values are bytes instead of pixels.\n"
	   "Pixel numbers and bytes are indexed linearly from one.\n");
    return;
}


HIDDEN void
handle_i_opt(const char *arg, long *skip1, long *skip2)
{
    const char *endptr = arg;
    if ((arg == NULL) || ((skip1 == NULL) && (skip2 == NULL))) {
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
	bu_exit(OPTS_ERROR, "Unexpected input processing [%s]\n", arg);
    }
}


int
main(int argc, char *argv[])
{
    FILE *f1 = NULL;
    FILE *f2 = NULL;

    long matching = 0;
    long off1 = 0;
    long offmany = 0;

    int c;
    int list_pixel_values = 0;
    int print_bytes = 0;
    int silent = 0;
    long f1_skip = 0;
    long f2_skip = 0;

    long int bytes = 0;

    /* process opts */
    while ((c = bu_getopt(argc, argv, "lbi:s")) != -1) {
	switch (c) {
	    case 'l':
		list_pixel_values = 1;
		break;
	    case 'b':
		print_bytes = 1;
		break;
	    case 'i':
		handle_i_opt(bu_optarg, &f1_skip, &f2_skip);
		break;
	    case 's':
		silent = 1;
		break;
	    default:
		bu_log("\n");
		usage(argv[0]);
		exit(OPTS_ERROR);
	}
    }
    argc -= bu_optind;
    argv += bu_optind;

    /* validate what is left over */
    if (argc < 1 || argc > 4) {
	bu_log("ERROR: incorrect number of arguments provided\n\n");
	usage(argv[0]);
	exit(OPTS_ERROR);
    }
    if ((argc > 0 && !argv[0]) || (argc > 1 && !argv[1])) {
	bu_log("ERROR: bad filename\n\n");
	usage(argv[0]);
	exit(OPTS_ERROR);
    }
    if ((argc > 2 && !argv[2]) || (argc > 3 && !argv[3])) {
	bu_log("ERROR: bad skip value\n\n");
	usage(argv[0]);
	exit(OPTS_ERROR);
    }

    /* handle optional SKIP1 and SKIP2 following filenames */
    if (argc > 2) {
	char range[64] = {0};
	if (argc > 3) {
	    snprintf(range, 64, "%s:%s", argv[2], argv[3]);
	} else {
	    snprintf(range, 64, "%s", argv[2]);
	}
	handle_i_opt(range, &f1_skip, &f2_skip);
    }

    /* printf("Skip from FILE1: %ld and from FILE2: %ld\n", f1_skip, f2_skip); */

    if (BU_STR_EQUAL(argv[0], "-")) {
	f1 = stdin;
    } else if ((f1 = fopen(argv[0], "rb")) == NULL) {
	perror(argv[0]);
	exit(FILE_ERROR);
    }
    if ((argc < 2) || BU_STR_EQUAL(argv[1], "-")) {
	f2 = stdin;
    } else if ((f2 = fopen(argv[1], "rb")) == NULL) {
	perror(argv[1]);
	exit(FILE_ERROR);
    }

    if (!print_bytes) {
	f1_skip *= 3;
	f2_skip *= 3;
    }

    /* skip requested pixels/bytes in FILE1 */
    if (f1_skip && fseek(f1, f1_skip, SEEK_SET)) {
	bu_log("ERROR: Unable to seek %ld %s%s in FILE1\n",
	       f1_skip,
	       print_bytes?"byte":"pixel",
	       f1_skip==1?"":"s");
	perror("FILE1 fseek failure");
	exit(FILE_ERROR);
    }

    /* skip requested pixels in FILE2 */
    if (f2_skip && fseek(f2, f2_skip, SEEK_SET)) {
	bu_log("ERROR: Unable to seek %ld %s%s in FILE2\n",
	       f1_skip,
	       print_bytes?"byte":"pixel",
	       f1_skip==1?"":"s");
	perror("FILE2 fseek failure");
	exit(FILE_ERROR);
    }

    /* iterate over the pixels/bytes in the files */
    while ((!feof(f1) && !feof(f2)) &&
	   (!ferror(f1) && !ferror(f2))) {
	register int r1, r2, g1, g2, b1, b2;
	r1 = r2 = g1 = g2 = b1 = b2 = -1;

	r1 = fgetc(f1);
	r2 = fgetc(f2);
	if (feof(f1) || feof(f2)) break;
	bytes++;
	if (!print_bytes) {
	    g1 = fgetc(f1);
	    g2 = fgetc(f2);
	    if (feof(f1) || feof(f2)) break;
	    bytes++;
	    b1 = fgetc(f1);
	    b2 = fgetc(f2);
	    if (feof(f1) || feof(f2)) break;
	    bytes++;
	}

	if ((r1 == r2) && (g1 == g2) && (b1 == b2)) {
	    matching++;
	    continue;
	}

	/* tabulate differing pixels */
	if (((r1 != r2) && (g1 == g2) && (b1 == b2)) ||
	    ((r1 == r2) && (g1 != g2) && (b1 == b2)) ||
	    ((r1 == r2) && (g1 == g2) && (b1 != b2))) {
	    /* off by one channel */
	    if (r1 != r2) {
		if ((r1 > r2 ? r1 - r2 : r2 - r1) > 1) {
		    offmany++;
		} else {
		    off1++;
		}
	    } else if (g1 != g2) {
		if ((g1 > g2 ? g1 - g2 : g2 - g1) > 1) {
		    offmany++;
		} else {
		    off1++;
		}
	    } else if (b1 != b2) {
		if ((b1 > b2 ? b1 - b2 : b2 - b1) > 1) {
		    offmany++;
		} else {
		    off1++;
		}
	    }
	} else {
	    /* off by many */
	    offmany++;
	}

	/* they're different, so print something */
	if (list_pixel_values) {
	    if (print_bytes) {
		printf("%ld %3d %3d\n", bytes, r1, r2);
	    } else {
		printf("%ld\t(%3d, %3d, %3d)\t(%3d, %3d, %3d)\n", bytes / 3, r1, g1, b1, r2, g2, b2);
	    }
	}
    }

    /* print summary */
    if (!silent) {
	printf("pixcmp %s: %8ld matching, %8ld off by 1, %8ld off by many\n",
	       print_bytes?"bytes":"pixels",
	       matching, off1, offmany);
    }

    /* check for errors */
    if (ferror(f1) || ferror(f2)) {
	perror("pixcmp file error");
	return FILE_ERROR;
    }

    /* if files were of different lengths, consider it an error */
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
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
