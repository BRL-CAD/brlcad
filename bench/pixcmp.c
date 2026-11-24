/*                        P I X C M P . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2026 United States Government as represented by
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

// TODO - replace with std::filesystem::is_fifo
#ifndef S_ISFIFO
#  ifdef S_IFIFO
#    define S_ISFIFO(mode) (((mode) & S_IFMT) == S_IFIFO)
#  else
#    define S_ISFIFO(mode) (0)
#  endif
#endif

#include <errno.h>
#include "bu/arg.h"
#include "bu/app.h"
#include "bu/log.h"
#include "bu/file.h"
#include "bu/malloc.h"
#include "bu/str.h"

/* exit codes for argument processing errors */
#define OPTS_ERROR 127
#define FILE_ERROR 126

/* exit codes for normal operation */
#define EXIT_OFF_BY_NONE 0
#define EXIT_OFF_BY_ONE  1
#define EXIT_OFF_BY_MANY 2

enum diff {
    MATCHING,
    OFF_BY_ONE,
    OFF_BY_MANY,
    MISSING
};


struct pixcmp_skip {
    size_t first;
    size_t second;
};

struct pixcmp_args {
    int help;
    int list_same;
    int list_diff;
    int print_bytes;
    int quiet;
    struct pixcmp_skip skip;
    size_t count;
};

static int
pixcmp_size_parse(struct bu_vls *msg, const char *arg, void *storage)
{
    size_t value;

    if (!bu_arg_size_t_from_str(msg, &value, arg, 10))
	return -1;
    if (storage)
	*((size_t *)storage) = value;
    return 0;
}

static int
pixcmp_size_validate(struct bu_vls *msg, const char *arg)
{
    return pixcmp_size_parse(msg, arg, NULL);
}

static int
pixcmp_skip_parse(struct bu_vls *msg, const char *arg, void *storage)
{
    struct pixcmp_skip value = {0, 0};
    const char *separator;

    if (!arg || !arg[0]) {
	if (msg)
	    bu_vls_printf(msg, "skip value must not be empty\n");
	return -1;
    }

    separator = strchr(arg, ':');
    if (!separator) {
	if (!bu_arg_size_t_from_str(msg, &value.first, arg, 10))
	    return -1;
	value.second = value.first;
    } else {
	size_t first_length = (size_t)(separator - arg);
	char *first;

	if (first_length == 0 || !separator[1] || strchr(separator + 1, ':')) {
	    if (msg)
		bu_vls_printf(msg, "skip range must be SKIP or SKIP1:SKIP2\n");
	    return -1;
	}
	first = (char *)bu_malloc(first_length + 1, "pixcmp first skip");
	memcpy(first, arg, first_length);
	first[first_length] = '\0';
	if (!bu_arg_size_t_from_str(msg, &value.first, first, 10) ||
	    !bu_arg_size_t_from_str(msg, &value.second, separator + 1, 10)) {
	    bu_free(first, "pixcmp first skip");
	    return -1;
	}
	bu_free(first, "pixcmp first skip");
    }

    if (storage)
	*((struct pixcmp_skip *)storage) = value;
    return 0;
}

static const struct bu_arg_option pixcmp_options[] = {
    BU_ARG_FLAG("h", "help", struct pixcmp_args, help,
	"Print help and exit"),
    BU_ARG_ALIAS("?", NULL, "help", 1),
    BU_ARG_FLAG("b", "bytes", struct pixcmp_args, print_bytes,
	"Compare and report bytes instead of pixels; skip and count values are bytes"),
    BU_ARG_FLAG("s", "same", struct pixcmp_args, list_same,
	"Print every matching pixel or byte"),
    BU_ARG_FLAG("d", "different", struct pixcmp_args, list_diff,
	"Print every differing pixel or byte"),
    BU_ARG_FLAG("q", "quiet", struct pixcmp_args, quiet,
	"Suppress the header, size warning, and summary"),
    BU_ARG_CUSTOM("i", "skip", struct pixcmp_args, skip,
	pixcmp_skip_parse, "skip|skip1:skip2",
	"Discard one initial count from both inputs, or skip1 and skip2 from each input"),
    BU_ARG_CUSTOM("n", "count", struct pixcmp_args, count,
	pixcmp_size_parse, "count",
	"Compare at most this many pixels or bytes; otherwise continue to end of file"),
    BU_ARG_OPTION_NULL
};

static const struct bu_arg_operand pixcmp_operands[] = {
    BU_ARG_OPERAND("file1", BU_ARG_VALUE_FILE, 1, 1,
	"First input file, or - for standard input", NULL),
    BU_ARG_OPERAND("file2", BU_ARG_VALUE_FILE, 0, 1,
	"Second input file, or - or omission for standard input", NULL),
    BU_ARG_OPERAND_VALIDATE("skip1", BU_ARG_VALUE_STRING, 0, 1,
	pixcmp_size_validate, "Initial pixels or bytes to discard from file1", NULL),
    BU_ARG_OPERAND_VALIDATE("skip2", BU_ARG_VALUE_STRING, 0, 1,
	pixcmp_size_validate, "Initial pixels or bytes to discard from file2", NULL),
    BU_ARG_OPERAND_NULL
};

static const struct bu_arg_schema pixcmp_schema = BU_ARG_SCHEMA(
    "pixcmp", "Compare two PIX images pixel by pixel or byte by byte",
    pixcmp_options, pixcmp_operands, BU_ARG_PARSE_INTERSPERSED,
    BU_ARG_SCHEMA_META_HELP(NULL, NULL, NULL, NULL, NULL));

static const struct bu_arg_grammar pixcmp_grammar = {
    &pixcmp_schema, NULL, NULL
};

static int
pixcmp_help(FILE *stream, int status)
{
    char *help = bu_arg_schema_help(&pixcmp_schema, "pixcmp");
    bu_flog(stream, "%s", help ? help : "");
    bu_free(help, "pixcmp help");
    return status;
}


static enum diff
compare_rgb(int r1, int g1, int b1, int r2, int g2, int b2, size_t *matching, size_t *off1, size_t *offmany, size_t *missing)
{
    enum diff result = MISSING;

    if ((r1 == r2 && r1 != -1)
	&& (g1 == g2 && g1 != -1)
	&& (b1 == b2 && b1 != -1))
    {
	result = MATCHING;
    } else if (r1 == -1 || r2 == -1 || g1 == -1 || g2 == -1 || b1 == -1 || b2 == -1) {
	/* image sizes don't match (or other I/O error) */
	result = MISSING;
    } else {
	/* at least this */
	result = OFF_BY_ONE;

	/* but maybe more */
	if (r1 != r2) {
	    if ((r1 > r2 ? r1 - r2 : r2 - r1) > 1) {
		result = OFF_BY_MANY;
	    }
	}
	if (g1 != g2) {
	    if ((g1 > g2 ? g1 - g2 : g2 - g1) > 1) {
		result = OFF_BY_MANY;
	    }
	}
	if (b1 != b2) {
	    if ((b1 > b2 ? b1 - b2 : b2 - b1) > 1) {
		result = OFF_BY_MANY;
	    }
	}
    }
    if (result == OFF_BY_ONE) {
	(*off1)++;
    } else if (result == OFF_BY_MANY) {
	(*offmany)++;
    } else if (result == MATCHING) {
	(*matching)++;
    } else {
	(*missing)++;
    }

    return result;
}


int
main(int argc, char *argv[])
{
    struct pixcmp_args args = {0};
    struct bu_vls msg = BU_VLS_INIT_ZERO;
    FILE *f1 = NULL;
    FILE *f2 = NULL;
    struct stat sf1;
    struct stat sf2;
    int introspection;
    int operand_index;

    memset(&sf1, 0, sizeof(struct stat));
    memset(&sf2, 0, sizeof(struct stat));

    size_t matching = 0;
    size_t off1 = 0;
    size_t offmany = 0;
    size_t missing = 0;

    size_t bytes = 0;
    size_t f1_skip = 0;
    size_t f2_skip = 0;
    size_t stop_after = 0;

    bu_setprogname(argv[0]);

    introspection = bu_arg_introspect(&msg, &pixcmp_grammar, argc - 1,
	(const char * const *)(argv + 1));
    if (introspection != 0) {
	bu_flog(introspection > 0 ? stdout : stderr, "%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return introspection > 0 ? 0 : OPTS_ERROR;
    }
    operand_index = bu_arg_schema_parse_complete(&pixcmp_schema, &args, &msg,
	argc - 1, (const char **)(argv + 1));
    if (operand_index < 0) {
	bu_flog(stderr, "%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return OPTS_ERROR;
    }
    bu_vls_free(&msg);
    if (args.help)
	return pixcmp_help(stdout, 0);

    argc = argc - 1 - operand_index;
    argv = argv + 1 + operand_index;
    f1_skip = args.skip.first;
    f2_skip = args.skip.second;
    stop_after = args.count;

    /* Trailing compatibility values override -i, as they did historically. */
    if (argc > 2) {
	(void)bu_arg_size_t_from_str(NULL, &f1_skip, argv[2], 10);
	if (argc > 3)
	    (void)bu_arg_size_t_from_str(NULL, &f2_skip, argv[3], 10);
	else
	    f2_skip = f1_skip;
    }

    if (!args.print_bytes) {
	if ((f1_skip > ((size_t)-1)/3)
	    || (f2_skip > ((size_t)-1)/3))
	{
	    bu_log("ERROR: overflow, -i skip value(s) are too big {%zu:%zu}\n", f1_skip, f2_skip);
	    exit(OPTS_ERROR);
	}
	f1_skip *= 3;
	f2_skip *= 3;
	if (stop_after > ((size_t)-1)/3)
	{
	    bu_log("ERROR: overflow, -n number is too big {%zu}\n", stop_after);
	    exit(OPTS_ERROR);
	}
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

    setmode(fileno(stdin), O_BINARY);
    /* we're writing out ascii */
    /* setmode(fileno(stdout), O_BINARY); */

    if (f1_skip != f2_skip && f1 == stdin && f2 == stdin) {
	bu_exit(OPTS_ERROR, "ERROR: cannot skip the same input stream by different amounts\n");
    }

    errno = 0;
    if (fstat(fileno(f1), &sf1)) {
	bu_log("Warning - f1 fstat failed: %s\n", strerror(errno));
    }
    errno = 0;
    if (fstat(fileno(f2), &sf2)) {
	bu_log("Warning - f2 fstat failed: %s\n", strerror(errno));
    }

    /* bu_log("FILE1_size(%zu) FILE1_skip(%zu) FILE2_size(%zu) FILE2_skip(%zu)\n", (size_t)sf1.st_size, f1_skip, (size_t)sf2.st_size, f2_skip); */

    if (!args.quiet && ((sf1.st_size - f1_skip) != (sf2.st_size - f2_skip))) {
	bu_log("WARNING: Different image sizes detected\n");
	if (args.print_bytes) {
	    bu_log("\t%s: %7zu bytes (%8zu bytes, skipping %7zu)\n",
		   argv[0], (size_t)sf1.st_size - f1_skip, (size_t)sf1.st_size, f1_skip);
	    bu_log("\t%s: %7zu bytes (%8zu bytes, skipping %7zu)\n",
		   argv[1], (size_t)sf2.st_size - f2_skip, (size_t)sf2.st_size, f2_skip);
	} else {
	    bu_log("\t%s: %7zu pixels (%8zu bytes, skipping %7zu)\n",
		   argv[0], ((size_t)sf1.st_size - f1_skip)/3, (size_t)sf1.st_size, f1_skip);
	    bu_log("\t%s: %7zu pixels (%8zu bytes, skipping %7zu)\n",
		   argv[1], ((size_t)sf2.st_size - f2_skip)/3, (size_t)sf2.st_size, f2_skip);
	}
    }

    /* make sure we read all bytes */
    if (stop_after == 0) {
	stop_after = FMAX((sf1.st_size - f1_skip), (sf2.st_size - f2_skip));
	if (f1 == stdin && f2 == stdin) {
	    /* dual stdin is interleaved */
	    stop_after = (stop_after+1)/2;
	}
    }

    /*
    bu_log("mode is [%d] and [%d]\n", S_ISFIFO(sf1.st_mode), S_ISFIFO(sf2.st_mode));
    bu_log("tty is [%d] and [%d]\n", isatty(fileno(f1)), isatty(fileno(f2)));
    */

    /* skip requested pixels/bytes in FILE1 */
    if (f1_skip) {
	if (S_ISFIFO(sf1.st_mode)) {
	    size_t skipped = 0;
	    for (skipped = 0; skipped < f1_skip; skipped++) {
		(void)fgetc(f1);
	    }
	} else if (fseek(f1, (long)f1_skip, SEEK_SET)) {
	    bu_log("ERROR: Unable to seek %zd %s%s in FILE1\n",
		   f1_skip,
		   args.print_bytes?"byte":"pixel",
		   f1_skip==1?"":"s");
	    perror("FILE1 fseek failure");
	    exit(FILE_ERROR);
	}
    }

    /* skip requested pixels in FILE2 */
    if (f2_skip) {
	if (S_ISFIFO(sf2.st_mode)) {
	    size_t skipped = 0;
	    for (skipped = 0; skipped < f2_skip; skipped++) {
		(void)fgetc(f2);
	    }
	} else if (fseek(f2, (long)f2_skip, SEEK_SET)) {
	    bu_log("ERROR: Unable to seek %zd %s%s in FILE2\n",
		   f2_skip,
		   args.print_bytes?"byte":"pixel",
		   f2_skip==1?"":"s");
	    perror("FILE2 fseek failure");
	    exit(FILE_ERROR);
	}
    }

    /* print header to stderr, output to stdout */
    if (!args.quiet && (args.list_same || args.list_diff)) {
	if (args.print_bytes) {
	    bu_log("#Byte FILE1 FILE2 LABEL\n");
	} else {
	    bu_log("#Pixel\t(FILE1 R, G, B) (FILE2 R, G, B)\n");
	}
    }

    /* iterate over the pixels/bytes in the files */
    while (bytes < stop_after) {
	enum diff result;
	int r1 = -1;
	int r2 = -1;
	int g1 = -1;
	int g2 = -1;
	int b1 = -1;
	int b2 = -1;

	/* bu_log("\tbytes[%zu] < stop[%zu\n", bytes, stop_after); */

	r1 = fgetc(f1);
	r2 = fgetc(f2);
	bytes++;

	if (args.print_bytes) {
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

	result = compare_rgb(r1, g1, b1, r2, g2, b2, &matching, &off1, &offmany, &missing);

	/* print them? */
	if ((result==MATCHING && args.list_same)
	    || (result!=MATCHING && args.list_diff))
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
	    if (args.print_bytes) {
		printf("%zu %3d %3d %s\n", bytes, r1, r2, label);
	    } else {
		printf("%zu\t(%3d, %3d, %3d)\t(%3d, %3d, %3d) %s\n", bytes / 3, r1, g1, b1, r2, g2, b2, label);
	    }
	}
    }

    /* print summary */
    if (!args.quiet) {
	printf("pixcmp %s: %8zd matching, %8zd off by 1, %8zd off by many",
	       args.print_bytes?"bytes":"pixels", matching, off1, offmany);
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
