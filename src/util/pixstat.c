/*                       P I X S T A T . C
 * BRL-CAD
 *
 * Copyright (c) 1986-2026 United States Government as represented by
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
/** @file util/pixstat.c
 *
 * Compute statistics of pixels in a PIX file.
 * Gives min, max, mode, median, mean, s.d., var, and skew
 * for each color.
 * Will optionally dump the histogram of values.
 *
 * We compute these from a histogram which is a real win for
 * discrete values of small range.  Statistics can then be
 * calculated on the lump sums in the bins rather than individual
 * samples.  Also only one pass is required over the input file.
 * (lastly, some statistics such as mode and median, require the
 * histogram)
 *
 */

#include "common.h"

#include <math.h>
#include "bio.h"

#include "bu/arg.h"
#include "bu/app.h"
#include "bu/exit.h"
#include "bu/malloc.h"


#define HISTOGRAM_BINS 256
#define INPUT_BUFFER_SIZE (3 * 1024)

/* our statistics pool */
struct rgb_statistics {
    long bin[HISTOGRAM_BINS];
    int max, min, mode, median;
    double mean, var, skew;
    long sum, partial_sum;
};

struct pixstat_args {
    int help;
    int verbose;
};

static const struct bu_arg_option pixstat_options[] = {
    BU_ARG_FLAG("h", "help", struct pixstat_args, help,
	"Print help and exit"),
    BU_ARG_ALIAS("?", NULL, "help", 1),
    BU_ARG_FLAG("v", "verbose", struct pixstat_args, verbose,
	"Print the full per-channel histogram"),
    BU_ARG_OPTION_NULL
};

static const struct bu_arg_operand pixstat_operands[] = {
    BU_ARG_OPERAND("input", BU_ARG_VALUE_FILE, 0, 1,
	"PIX input file (default: standard input)", NULL),
    BU_ARG_OPERAND_NULL
};

static const struct bu_arg_schema pixstat_schema = BU_ARG_SCHEMA(
    "pixstat", "Compute per-channel statistics for a PIX image",
    pixstat_options, pixstat_operands, BU_ARG_PARSE_OPTIONS_FIRST,
    BU_ARG_SCHEMA_META_HELP(NULL, NULL, NULL, NULL, NULL));

static const struct bu_arg_grammar pixstat_grammar = {
    &pixstat_schema, NULL, NULL
};


/*
 * Display the histogram values.
 */
static void
show_hist(const struct rgb_statistics *red,
    const struct rgb_statistics *green,
    const struct rgb_statistics *blue)
{
    int i;

    printf("Histogram:\n");

    for (i = 0; i < HISTOGRAM_BINS; i++) {
	printf("%3d: %10ld %10ld %10ld\n",
	       i, red->bin[i], green->bin[i], blue->bin[i]);
    }
}


int
main(int argc, char **argv)
{
    int i, n, num;
    double d;
    long num_pixels = 0L ;
    long median_rank;
    unsigned char buf[INPUT_BUFFER_SIZE];
    unsigned char *bp;
    FILE *fp = stdin;
    struct rgb_statistics r = {0};
    struct rgb_statistics g = {0};
    struct rgb_statistics b = {0};
    struct pixstat_args args = {0};
    struct bu_vls msg = BU_VLS_INIT_ZERO;

    bu_setprogname(argv[0]);

    int introspection = bu_arg_introspect(&msg, &pixstat_grammar, argc - 1,
	(const char * const *)(argv + 1));
    if (introspection != 0) {
	bu_flog(introspection > 0 ? stdout : stderr, "%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return introspection > 0 ? 0 : 1;
    }
    int operand_index = bu_arg_schema_parse_complete(&pixstat_schema, &args,
	&msg, argc - 1, (const char **)(argv + 1));
    if (operand_index < 0)
	bu_exit(1, "%s", bu_vls_cstr(&msg));
    bu_vls_free(&msg);
    if (args.help) {
	char *help = bu_arg_schema_help(&pixstat_schema, "pixstat");
	bu_flog(stdout, "%s", help ? help : "");
	bu_free(help, "pixstat help");
	return 0;
    }

    if (argc - operand_index - 1 == 1) {
	const char *path = argv[operand_index + 1];
	fp = fopen(path, "rb");
	if (!fp)
	    bu_exit(1, "pixstat: cannot open '%s' for reading\n", path);
    } else if (isatty(fileno(stdin))) {
	bu_exit(1, "pixstat requires an input file or redirected input\n");
    }

    setmode(fileno(fp), O_BINARY);
    setmode(fileno(stdout), O_BINARY);

    /*
     * Build the histogram. (num_pixels initialized to 0)
     */
    while ((n = fread(&buf[0], sizeof(*buf), INPUT_BUFFER_SIZE, fp)) > 0) {
	if (n % 3)
	    bu_exit(1, "pixstat: input ends with an incomplete pixel\n");
	num = n/3;
	num_pixels += num;
	bp = &buf[0];
	for (i = 0; i < num; i++) {
	    r.bin[ *bp++ ]++;
	    g.bin[ *bp++ ]++;
	    b.bin[ *bp++ ]++;
	}
    }
    if (ferror(fp))
	bu_exit(1, "pixstat: error reading input\n");
    if (fp != stdin && fclose(fp) == EOF)
	bu_exit(1, "pixstat: error closing input\n");
    if (num_pixels == 0)
	bu_exit(1, "pixstat: input contains no pixels\n");
    median_rank = num_pixels / 2 + num_pixels % 2;

    /*
     * Find sum, min, max, mode.
     */
    r.sum = g.sum = b.sum = 0;
    r.min = g.min = b.min = HISTOGRAM_BINS;
    r.max = g.max = b.max = -1;
    r.mode = g.mode = b.mode = 0;
    for (i = 0; i < HISTOGRAM_BINS; i++) {
	/* sum */
	r.sum += i * r.bin[i];
	g.sum += i * g.bin[i];
	b.sum += i * b.bin[i];
	/* min */
	if (i < r.min && r.bin[i] != 0)
	    r.min = i;
	if (i < g.min && g.bin[i] != 0)
	    g.min = i;
	if (i < b.min && b.bin[i] != 0)
	    b.min = i;
	/* max */
	if (i > r.max && r.bin[i] != 0)
	    r.max = i;
	if (i > g.max && g.bin[i] != 0)
	    g.max = i;
	if (i > b.max && b.bin[i] != 0)
	    b.max = i;
	/* mode */
	if (r.bin[i] > r.bin[r.mode])
	    r.mode = i;
	if (g.bin[i] > g.bin[g.mode])
	    g.mode = i;
	if (b.bin[i] > b.bin[b.mode])
	    b.mode = i;
    }
    r.mean = (double)r.sum/(double)num_pixels;
    g.mean = (double)g.sum/(double)num_pixels;
    b.mean = (double)b.sum/(double)num_pixels;

    /*
     * Now do a second pass to compute median,
     * variance and skew.
     */
    for (i = 0; i < HISTOGRAM_BINS; i++) {
	/* median */
	if (r.partial_sum < median_rank) {
	    r.partial_sum += r.bin[i];
	    if (r.partial_sum >= median_rank)
		r.median = i;
	}
	if (g.partial_sum < median_rank) {
	    g.partial_sum += g.bin[i];
	    if (g.partial_sum >= median_rank)
		g.median = i;
	}
	if (b.partial_sum < median_rank) {
	    b.partial_sum += b.bin[i];
	    if (b.partial_sum >= median_rank)
		b.median = i;
	}
	/* var and skew */
	d = (double)i - r.mean;
	r.var += r.bin[i] * d * d;
	r.skew += r.bin[i] * d * d * d;
	d = (double)i - g.mean;
	g.var += g.bin[i] * d * d;
	g.skew += g.bin[i] * d * d * d;
	d = (double)i - b.mean;
	b.var += b.bin[i] * d * d;
	b.skew += b.bin[i] * d * d * d;
    }
    r.var /= (double)num_pixels;
    g.var /= (double)num_pixels;
    b.var /= (double)num_pixels;
    r.skew /= (double)num_pixels;
    g.skew /= (double)num_pixels;
    b.skew /= (double)num_pixels;

    /*
     * Display the results.
     */
    printf("Pixels  %14ld (%.0f x %.0f)\n", num_pixels,
	   sqrt((double)num_pixels), sqrt((double)num_pixels));
    printf("Min     %14d%14d%14d\n", r.min, g.min, b.min);
    printf("Max     %14d%14d%14d\n", r.max, g.max, b.max);
    printf("Mode    %14d%14d%14d\n", r.mode, g.mode, b.mode);
    printf("Num@mode%14ld%14ld%14ld\n", r.bin[r.mode], g.bin[g.mode], b.bin[b.mode]);
    printf("Median  %14d%14d%14d\n", r.median, g.median, b.median);
    printf("Mean    %14.3f%14.3f%14.3f\n", r.mean, g.mean, b.mean);
    printf("s.d.    %14.3f%14.3f%14.3f\n", sqrt(r.var), sqrt(g.var), sqrt(b.var));
    printf("Var     %14.3f%14.3f%14.3f\n", r.var, g.var, b.var);
    printf("Skew    %14.3f%14.3f%14.3f\n", r.skew, g.skew, b.skew);

    if (args.verbose)
	show_hist(&r, &g, &b);
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
