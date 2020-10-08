/*                        P I X C M P . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2020 United States Government as represented by
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
#ifdef HAVE_SYS_STAT_H
#  include <sys/stat.h>
#endif
#include "bio.h"

#include "bu/app.h"
#include "bu/getopt.h"
#include "bu/log.h"
#include "bu/file.h"
#include "bu/str.h"

/* exit codes for argument processing errors */
#define OPTS_ERROR 127
#define FILE_ERROR 126

/* exit codes for normal operation */
#define EXIT_OFF_BY_NONE 0
#define EXIT_OFF_BY_ONE  1
#define EXIT_OFF_BY_MANY 2

enum label {
    MATCHING,
    OFF_BY_ONE,
    OFF_BY_MANY,
    MISSING
};


HIDDEN void
usage(const char *name)
{
    const char *unknown = "pixcmp";
    if (!name) {
	name = unknown;
    }

    /* split to fit within ISO C90 max static string length < 509 chars */
    bu_log("Usage: %s [OPTIONS] FILE1 [FILE2 [SKIP1 [SKIP2]]]\n"
	   "\n"
	   "  Compare two PIX files pixel by pixel (or byte by byte), optionally"
	   "  skipping initial pixels (or bytes) in one or both input files.\n"
	   "\n", name);
    bu_log("Options:"
	   "  -b   Iterate and print by bytes instead of by pixels.\n"
	   "  -s   Print all that have the same matching values.\n"
	   "  -d   Print all that have different values.\n"
	   "  -q   Quiet.  Suppress printing header and summary.\n"
	   "  -i SKIP\n"
	   "       Discard initial SKIP pixels (or bytes w/ -b) in FILE1 and FILE2 input.\n"
	   "  -i SKIP1:SKIP2\n"
	   "       Skip initial SKIP1 pixels (or bytes w/ -b) in FILE1 and SKIP2 in FILE2.\n"
	   "  -n COUNT\n"
	   "       Compare COUNT pixels (or bytes w/ -b).  Stops by default at EOF.\n"
	   "\n");
    bu_log("If either FILE is `-' or is missing, then input is read from standard input.\n"
	   "If FILE1 and FILE2 are both standard input, then values must be interleaved.\n"
	   "If the `-b' option is used, SKIP and COUNT values are bytes instead of pixels.\n"
	   "Pixel numbers and bytes are indexed linearly from one.\n"
	   "\n");
    return;
}


HIDDEN void
handle_range_opt(const char *arg, size_t *skip1, size_t *skip2)
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
	if (skip1 && skip2) {
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
    const char *argv0 = argv[0];

    FILE *f1 = NULL;
    FILE *f2 = NULL;
    struct stat sf1 = {0};
    struct stat sf2 = {0};

    size_t matching = 0;
    size_t off1 = 0;
    size_t offmany = 0;
    size_t missing = 0;

    int c;
    int list_diff = 0;
    int list_same = 0;
    int print_bytes = 0;
    int quiet = 0;

    size_t bytes = 0;
    size_t f1_skip = 0;
    size_t f2_skip = 0;
    size_t stop_after = 0;

    bu_setprogname(argv[0]);

    /* process opts */
    while ((c = bu_getopt(argc, argv, "sdbi:n:q?")) != -1) {
	switch (c) {
	    case 's':
		list_same = 1;
		break;
	    case 'd':
		list_diff = 1;
		break;
	    case 'b':
		print_bytes = 1;
		break;
	    case 'i':
		handle_range_opt(bu_optarg, &f1_skip, &f2_skip);
		break;
	    case 'n':
		stop_after = (size_t)strtol(bu_optarg, NULL, 10);
		break;
	    case 'q':
		quiet = 1;
		break;
	    case '?':
	    case 'h':
		usage(argv0);
		return 0;
	    default:
		exit(OPTS_ERROR);
	}
    }
    argc -= bu_optind;
    argv += bu_optind;

    /* validate what is left over */
    if (argc < 1 || argc > 4) {
	bu_log("ERROR: incorrect number of arguments provided\n\n");
	usage(argv0);
	exit(OPTS_ERROR);
    }
    if ((argc > 0 && !argv[0]) || (argc > 1 && !argv[1])) {
	bu_log("ERROR: bad filename\n\n");
	usage(argv0);
	exit(OPTS_ERROR);
    }
    if ((argc > 2 && !argv[2]) || (argc > 3 && !argv[3])) {
	bu_log("ERROR: bad skip value\n\n");
	usage(argv0);
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
	handle_range_opt(range, &f1_skip, &f2_skip);
    }

    /* printf("Skip from FILE1: %ld and from FILE2: %ld\n", f1_skip, f2_skip); */
    if (!print_bytes) {
	f1_skip *= 3;
	f2_skip *= 3;
	stop_after *= 3;
    }

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

    if (f1_skip != f2_skip && f1 == stdin && f2 == stdin) {
	bu_log("ERROR: Cannot skip the same input stream by different amounts\n\n");
	usage(argv0);
	exit(OPTS_ERROR);
    }

    fstat(fileno(f1), &sf1);
    fstat(fileno(f2), &sf2);

    if (!quiet && ((sf1.st_size - f1_skip) != (sf2.st_size - f2_skip))) {
	bu_log("WARNING: Different image sizes detected\n");
	if (print_bytes) {
	    bu_log("\t%s: %7llu bytes (%8llu bytes, skipping %7llu)\n",
		   argv[0], (unsigned long long)(sf1.st_size - f1_skip), (unsigned long long)sf1.st_size, (unsigned long long)f1_skip);
	    bu_log("\t%s: %7llu bytes (%8llu bytes, skipping %7llu)\n",
		   argv[1], (unsigned long long)(sf2.st_size - f2_skip), (unsigned long long)sf2.st_size, (unsigned long long)f2_skip);
	} else {
	    bu_log("\t%s: %7llu pixels (%8llu bytes, skipping %7llu)\n",
		   argv[0], (unsigned long long)((sf1.st_size - f1_skip)/3), (unsigned long long)sf1.st_size, (unsigned long long)f1_skip);
	    bu_log("\t%s: %7llu pixels (%8llu bytes, skipping %7llu)\n",
		   argv[1], (unsigned long long)((sf2.st_size - f2_skip)/3), (unsigned long long)sf2.st_size, (unsigned long long)f2_skip);
	}
    }

    /*
    bu_log("!!! mode is [%d] and [%d]\n", S_ISFIFO(sf1.st_mode), S_ISFIFO(sf2.st_mode));
    bu_log("!!! tty is [%d] and [%d]\n", isatty(fileno(f1)), isatty(fileno(f2)));
    */

    /* skip requested pixels/bytes in FILE1 */
    if (f1_skip && !S_ISFIFO(sf1.st_mode) && fseek(f1, f1_skip, SEEK_SET)) {
	bu_log("ERROR: Unable to seek %zd %s%s in FILE1\n",
	       f1_skip,
	       print_bytes?"byte":"pixel",
	       f1_skip==1?"":"s");
	perror("FILE1 fseek failure");
	exit(FILE_ERROR);
    } else {
	size_t skipped = 0;
	for (skipped = 0; skipped < f1_skip; skipped++) {
	    (void)fgetc(f1);
	}
    }

    /* skip requested pixels in FILE2 */
    if (f2_skip && !S_ISFIFO(sf2.st_mode) && fseek(f2, f2_skip, SEEK_SET)) {
	bu_log("ERROR: Unable to seek %zd %s%s in FILE2\n",
	       f2_skip,
	       print_bytes?"byte":"pixel",
	       f2_skip==1?"":"s");
	perror("FILE2 fseek failure");
	exit(FILE_ERROR);
    } else {
	size_t skipped = 0;
	for (skipped = 0; skipped < f2_skip; skipped++) {
	    (void)fgetc(f2);
	}
    }

    /* print header to stderr, output to stdout */
    if (!quiet && (list_same || list_diff)) {
	if (print_bytes) {
	    bu_log("#Byte FILE1 FILE2 LABEL\n");
	} else {
	    bu_log("#Pixel\t(FILE1 R, G, B) (FILE2 R, G, B)\n");
	}
    }

    /* make sure we don't stop */
    if (stop_after == 0)
	stop_after = SIZE_MAX;

    /* iterate over the pixels/bytes in the files */
    while ((!feof(f1) || !feof(f2))
	   && (!ferror(f1) || !ferror(f2)))
    {
	enum label result;
	int r1, r2, g1, g2, b1, b2;
	r1 = r2 = g1 = g2 = b1 = b2 = -1;
	result = MISSING;

	r1 = fgetc(f1);
	r2 = fgetc(f2);
	bytes++;

	/* stop if we can't even compare a byte */
	if (r1 == r2 && r1 == -1)
	    break;

	if (print_bytes) {
	    /* replicate */
	    b1 = g1 = 0;
	    b2 = g2 = 0;
	} else {
	    g1 = fgetc(f1);
	    g2 = fgetc(f2);
	    bytes++;
	    b1 = fgetc(f1);
	    b2 = fgetc(f2);
	    bytes++;
	}

	if ((r1 == r2 && r1 != -1)
	    && (g1 == g2 && g1 != -1)
	    && (b1 == b2 && b1 != -1))
	{
	    result = MATCHING;
	    matching++;
	} else if (r1 == -1 || r2 == -1 || g1 == -1 || g2 == -1 || b1 == -1 || b2 == -1) {
	    /* image sizes don't match (or other I/O error) */
	    result = MISSING;
	    missing++;
	} else if (((r1 != r2) && (g1 == g2) && (b1 == b2))
		   || ((r1 == r2) && (g1 != g2) && (b1 == b2))
		   || ((r1 == r2) && (g1 == g2) && (b1 != b2)))
	{
	    /* off by one channel */
	    if (r1 != r2) {
		if ((r1 > r2 ? r1 - r2 : r2 - r1) > 1) {
		    result = OFF_BY_MANY;
		    offmany++;
		} else {
		    result = OFF_BY_ONE;
		    off1++;
		}
	    } else if (g1 != g2) {
		if ((g1 > g2 ? g1 - g2 : g2 - g1) > 1) {
		    result = OFF_BY_MANY;
		    offmany++;
		} else {
		    result = OFF_BY_ONE;
		    off1++;
		}
	    } else if (b1 != b2) {
		if ((b1 > b2 ? b1 - b2 : b2 - b1) > 1) {
		    result = OFF_BY_MANY;
		    offmany++;
		} else {
		    result = OFF_BY_ONE;
		    off1++;
		}
	    }
	} else {
	    result = OFF_BY_MANY;
	    offmany++;
	}

	/* print them? */
	if ((result==MATCHING && list_same)
	    || (result!=MATCHING && list_diff))
	{
	    const char *label = NULL;
	    switch (result) {
		case MATCHING:
		    label = "MATCHING";
		    break;
		case OFF_BY_MANY:
		    label = "OFF_BY_MANY";
		    break;
		case OFF_BY_ONE:
		    label = "OFF_BY_ONE";
		    break;
		case MISSING:
		default:
		    label = "MISSING";
		    break;
	    }
	    if (print_bytes) {
		printf("%ld %3d %3d %s\n", bytes, r1, r2, label);
	    } else {
		printf("%ld\t(%3d, %3d, %3d)\t(%3d, %3d, %3d) %s\n", bytes / 3, r1, g1, b1, r2, g2, b2, label);
	    }
	}

	/* see if we need to terminate prematurely */
	if ((stop_after == (size_t)-1)
	    && (feof(f1) || feof(f2))) {
	    /* bu_log("exiting because of feof\n"); */
	    break;
	} else if (bytes >= stop_after) {
	    /* bu_log("exiting because of bytes\n"); */
	    break;
	}
    }

    /* print summary */
    if (!quiet) {
	printf("pixcmp %s: %8zd matching, %8zd off by 1, %8zd off by many",
	       print_bytes?"bytes":"pixels", matching, off1, offmany);
	if (missing) {
	    printf(", %8zd missing\n", missing);
	} else {
	    printf("\n");
	}
    }

    /* check for errors */
    if (ferror(f1) || ferror(f2)) {
	perror("pixcmp file error");
	return FILE_ERROR;
    }

    /* indicate how many differences there were overall */
    if (offmany || missing) {
	return EXIT_OFF_BY_MANY;
    }
    if (off1) {
	return EXIT_OFF_BY_ONE;
    }

    /* Success! */
    return EXIT_OFF_BY_NONE;
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
