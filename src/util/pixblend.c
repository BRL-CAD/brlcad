/*                      P I X B L E N D . C
 * BRL-CAD
 *
 * Copyright (c) 1995-2026 United States Government as represented by
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
/** @file util/pixblend.c
 *
 * Given two streams of data, typically pix(5) or bw(5) images,
 * generate an output stream of the same size, where the value of
 * each pixel in the output is determined by either:
 * 1) a linear interpolation between the two corresponding pixels in the
 * input streams; or,
 * 2) the pixel of either the first stream or the second stream, chosen
 * randomly.
 *
 * This routine operates on a pixel-by-pixel basis, and thus
 * is independent of the resolution of the image.
 *
 */

#include "common.h"

#include <errno.h>
#include <stdlib.h>
#ifdef HAVE_SYS_TIME_H
#  include <sys/time.h>
#endif
#include <string.h>
#include <time.h>
#include "bio.h"

#include "bu/app.h"
#include "bu/arg.h"
#include "bu/exit.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "bu/str.h"

#if defined(HAVE_SRAND48) && !defined(HAVE_DECL_SRAND48)
extern void srand48(long int seedval);
#endif

/* CHUNK should be a multiple of 3 (the pixel size) */
#define CHUNK (3 * 1024)

static int
timeseed(void)
{
    return time(0);
}

struct pixblend_args {
    int help;
    fastf_t random_value;
    fastf_t interpolate_value;
    int seed;
    fastf_t glitter_value;
};

static int
pixblend_time_seed(struct bu_vls *UNUSED(msg), const char *UNUSED(arg),
	void *storage)
{
    if (!storage)
	return -1;
    *(int *)storage = timeseed();
    return 0;
}

static const struct bu_arg_option pixblend_options[] = {
    BU_ARG_FLAG("h", "help", struct pixblend_args, help,
	"Print help and exit"),
    BU_ARG_ALIAS("?", NULL, "help", 1),
    BU_ARG_NUMBER_RANGE("r", "random", struct pixblend_args, random_value,
	1, 0.0, 1, 1.0, "value", "Randomly select pixels from the two inputs"),
    BU_ARG_NUMBER_RANGE("i", "interpolate", struct pixblend_args,
	interpolate_value, 1, 0.0, 1, 1.0, "value",
	"Linearly interpolate corresponding input values"),
    BU_ARG_VALUE("s", "seed", struct pixblend_args, seed,
	BU_ARG_VALUE_INTEGER, BU_ARG_REQUIRED, "seed", "Set the random seed"),
    BU_ARG_CUSTOM_FLAG("S", "time-seed", "time-seed", struct pixblend_args,
	seed, pixblend_time_seed, "Seed the random generator from the system clock"),
    BU_ARG_NUMBER_RANGE("g", "glitter", struct pixblend_args, glitter_value,
	1, 0.0, 1, 1.0, "gvalue", "Set the fraction of differing pixels to white"),
    BU_ARG_OPTION_NULL
};

static const struct bu_arg_operand pixblend_operands[] = {
    BU_ARG_OPERAND("first-input", BU_ARG_VALUE_FILE, 1, 1,
	"First PIX or BW input; - reads standard input", NULL),
    BU_ARG_OPERAND("second-input", BU_ARG_VALUE_FILE, 1, 1,
	"Second PIX or BW input; - reads standard input", NULL),
    BU_ARG_OPERAND_NULL
};

static const char * const pixblend_blend_conflicts[] = {
    "random", "interpolate", NULL
};
static const char * const pixblend_glitter_conflicts[] = {
    "glitter", "interpolate", NULL
};

static const struct bu_arg_constraint pixblend_constraints[] = {
    BU_ARG_CONSTRAINT(BU_ARG_CONSTRAINT_OPTION_CONFLICTS,
	BU_ARG_CONDITION_ALWAYS, pixblend_blend_conflicts, 0, 0,
	"--random cannot be combined with --interpolate"),
    BU_ARG_CONSTRAINT(BU_ARG_CONSTRAINT_OPTION_CONFLICTS,
	BU_ARG_CONDITION_ALWAYS, pixblend_glitter_conflicts, 0, 0,
	"--glitter cannot be combined with --interpolate"),
    BU_ARG_CONSTRAINT_NULL
};

static const struct bu_arg_schema pixblend_schema = BU_ARG_SCHEMA(
    "pixblend", "Blend two PIX or BW image streams",
    pixblend_options, pixblend_operands, BU_ARG_PARSE_OPTIONS_FIRST,
    BU_ARG_SCHEMA_META_HELP(NULL, pixblend_constraints, NULL, NULL, NULL));

static const struct bu_arg_grammar pixblend_grammar = {
    &pixblend_schema, NULL, NULL
};


int
main(int argc, char **argv)
{
#ifndef HAVE_DRAND48
    int threshold = 0;
    int gthreshold = 0;
#endif
    size_t ret;
    const char *f1_name;
    const char *f2_name;
    FILE *f1;
    FILE *f2;
    char *b1;
    char *b2;
    char *b3;
    struct pixblend_args args = {0, -1.0, -1.0, 0, -1.0};
    struct bu_vls msg = BU_VLS_INIT_ZERO;

    bu_setprogname(argv[0]);

    int introspection = bu_arg_introspect(&msg, &pixblend_grammar, argc - 1,
	(const char * const *)(argv + 1));
    if (introspection != 0) {
	bu_flog(introspection > 0 ? stdout : stderr, "%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return introspection > 0 ? 0 : 1;
    }
    int operand_index = bu_arg_schema_parse_complete(&pixblend_schema, &args,
	&msg, argc - 1, (const char **)(argv + 1));
    if (operand_index < 0)
	bu_exit(1, "%s", bu_vls_cstr(&msg));
    bu_vls_free(&msg);
    if (args.help) {
	char *help = bu_arg_schema_help(&pixblend_schema, "pixblend");
	bu_flog(stdout, "%s", help ? help : "");
	bu_free(help, "pixblend help");
	return 0;
    }
    if (isatty(fileno(stdout)))
	bu_exit(1, "pixblend requires redirected standard output\n");

    setmode(fileno(stdin), O_BINARY);
    setmode(fileno(stdout), O_BINARY);

    f1_name = argv[operand_index + 1];
    if (BU_STR_EQUAL(f1_name, "-")) {
	f1 = stdin;
    } else if ((f1 = fopen(f1_name, "rb")) == NULL) {
	bu_exit(1, "pixblend: cannot open \"%s\" for reading: %s\n",
	    f1_name, strerror(errno));
    }
    f2_name = argv[operand_index + 2];
    if (BU_STR_EQUAL(f2_name, "-")) {
	f2 = stdin;
    } else if ((f2 = fopen(f2_name, "rb")) == NULL) {
	bu_exit(1, "pixblend: cannot open \"%s\" for reading: %s\n",
	    f2_name, strerror(errno));
    }

    int interpolate = args.interpolate_value >= 0.0;
    int random_blend = args.random_value >= 0.0 || args.glitter_value >= 0.0;
    double value = args.random_value >= 0.0 ? args.random_value : 0.0;
    double gvalue = args.glitter_value >= 0.0 ? args.glitter_value : 0.0;
    if (!interpolate && !random_blend) {
	interpolate = 1;
	value = 0.5;
    } else if (interpolate) {
	value = args.interpolate_value;
    } else {
	value += gvalue * (1.0 - value);
    }

    if (random_blend) {
#ifdef HAVE_DRAND48
	srand48((long)args.seed);
#else
	threshold = (int) (value * 65536.0);
	gthreshold = (int) (gvalue * 65536.0);
	srandom(args.seed);
#endif
    }

    if ((b1 = (char *)malloc(CHUNK)) == (char *)0 ||
	(b2 = (char *)malloc(CHUNK)) == (char *)0 ||
	(b3 = (char *)malloc(CHUNK)) == (char *)0) {
	fprintf(stderr, "pixblend:  malloc failure\n");
	bu_exit (3, NULL);
    }

    while (1) {
	unsigned char *cb1, *cb2;	/* current input buf ptrs */
	unsigned char *cb3; 	/* current output buf ptr */
	size_t r1, r2, len, todo;

	r1 = fread(b1, 1, CHUNK, f1);
	r2 = fread(b2, 1, CHUNK, f2);
	len = r1;
	if (r2 < len)
	    len = r2;
	if (len <= 0)
	    break;

	cb1 = (unsigned char *)b1;
	cb2 = (unsigned char *)b2;
	cb3 = (unsigned char *)b3;
	todo = len;
	if (interpolate) {
	    while (todo--) {
		*cb3++ = (char) ((1.0 - value) * (*cb1++) +
				 value * (*cb2++));
	    }
	} else {
	    while (todo > 0) {
		if (cb1[0] == cb2[0] &&
		    cb1[1] == cb2[1] &&
		    cb1[2] == cb2[2]) {
		    cb3[0] = cb1[0];
		    cb3[1] = cb1[1];
		    cb3[2] = cb1[2];
		    cb1 += 3;
		    cb2 += 3;
		    cb3 += 3;
		    todo -= 3;
		} else {
#ifdef HAVE_DRAND48
		    double d;
		    d = drand48();
		    if (d >= value)
#else
			int r;
		    r = random() & 0xffff;
		    if (r >= threshold)
#endif
		    {
			cb3[0] = cb1[0];
			cb3[1] = cb1[1];
			cb3[2] = cb1[2];
		    } else {
			if (
#ifdef HAVE_DRAND48
			    d >= gvalue
#else
			    r >= gthreshold
#endif
			    ) {
			    cb3[0] = cb2[0];
			    cb3[1] = cb2[1];
			    cb3[2] = cb2[2];
			} else {
			    cb3[0] = cb3[1] = cb3[2] = 255;
			}
		    }
		    cb1 += 3;
		    cb2 += 3;
		    cb3 += 3;
		    todo -= 3;
		}
	    }
	}
	ret = fwrite(b3, 1, len, stdout);
	if (ret < len)
	    perror("fwrite");
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
