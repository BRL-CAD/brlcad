/*                        B W S T A T . C
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
/** @file util/bwstat.c
 *
 * Compute statistics of pixels in a black and white (BW) file.
 * Gives min, max, mode, median, mean, s.d., var, and skew.
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
#define INPUT_BUFFER_SIZE 1024

struct bwstat_args {
    int help;
    int verbose;
};

static const struct bu_arg_option bwstat_options[] = {
    BU_ARG_FLAG("h", "help", struct bwstat_args, help,
	"Print help and exit"),
    BU_ARG_ALIAS("?", NULL, "help", 1),
    BU_ARG_FLAG("v", "verbose", struct bwstat_args, verbose,
	"Print the full intensity histogram"),
    BU_ARG_OPTION_NULL
};

static const struct bu_arg_operand bwstat_operands[] = {
    BU_ARG_OPERAND("input", BU_ARG_VALUE_FILE, 0, 1,
	"BW input file (default: standard input)", NULL),
    BU_ARG_OPERAND_NULL
};

static const struct bu_arg_schema bwstat_schema = BU_ARG_SCHEMA(
    "bwstat", "Compute statistics for a BW image", bwstat_options,
    bwstat_operands, BU_ARG_PARSE_OPTIONS_FIRST,
    BU_ARG_SCHEMA_META_HELP(NULL, NULL, NULL, NULL, NULL));

static const struct bu_arg_grammar bwstat_grammar = {
    &bwstat_schema, NULL, NULL
};

/*
 * Display the histogram values.
 */
static void
show_hist(const long int *histogram, long int pixel_count)
{
    int i;

    printf("Histogram:\n");

    for (i = 0; i < HISTOGRAM_BINS; i++) {
	printf("%3d: %10ld (%10f)\n", i, histogram[i],
	       (double)histogram[i] / (double)pixel_count * 100.0);
    }
}


int
main(int argc, char **argv)
{
    int i;
    size_t n;
    double d;
    unsigned char buf[INPUT_BUFFER_SIZE];
    unsigned char *bp;
    long bin[HISTOGRAM_BINS] = {0};
    long num_pixels = 0L ;
    long sum = 0L, cumulative_pixels = 0L, median_rank;
    int max = -1, min = 256 , mode = 0 , median = 0 ;
    double mean, var = 0.0 , skew = 0.0 ;
    FILE *fp = stdin;
    struct bwstat_args args = {0};
    struct bu_vls msg = BU_VLS_INIT_ZERO;

    bu_setprogname(argv[0]);

    int introspection = bu_arg_introspect(&msg, &bwstat_grammar, argc - 1,
	(const char * const *)(argv + 1));
    if (introspection != 0) {
	bu_flog(introspection > 0 ? stdout : stderr, "%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return introspection > 0 ? 0 : 1;
    }
    int operand_index = bu_arg_schema_parse_complete(&bwstat_schema, &args,
	&msg, argc - 1, (const char **)(argv + 1));
    if (operand_index < 0)
	bu_exit(1, "%s", bu_vls_cstr(&msg));
    bu_vls_free(&msg);
    if (args.help) {
	char *help = bu_arg_schema_help(&bwstat_schema, "bwstat");
	bu_flog(stdout, "%s", help ? help : "");
	bu_free(help, "bwstat help");
	return 0;
    }

    if (argc - operand_index - 1 == 1) {
	const char *path = argv[operand_index + 1];
	fp = fopen(path, "rb");
	if (!fp)
	    bu_exit(1, "bwstat: cannot open '%s' for reading\n", path);
    } else if (isatty(fileno(stdin))) {
	bu_exit(1, "bwstat requires an input file or redirected input\n");
    }

    setmode(fileno(fp), O_BINARY);
    setmode(fileno(stdout), O_BINARY);

    /*
     * Build the histogram. (num_pixels initialized to 0)
     */
    while ((n = fread(buf, sizeof(*buf), INPUT_BUFFER_SIZE, fp)) > 0) {
	num_pixels += (long)n;
	bp = &buf[0];
	for (i = 0; i < (int)n; i++)
	    bin[ *bp++ ]++;
    }
    if (ferror(fp)) {
	bu_exit(1, "bwstat: error reading input\n");
    }
    if (fp != stdin && fclose(fp) == EOF)
	bu_exit(1, "bwstat: error closing input\n");
    if (num_pixels == 0) {
	bu_exit(1, "bwstat: input contains no pixels\n");
    }
    median_rank = num_pixels / 2 + num_pixels % 2;

    /*
     * Find sum, min, max, mode. (sum and mode initialized to 0;
     * min initialized to 256; max initialized to -1)
     */
    for (i = 0; i < HISTOGRAM_BINS; i++) {
	if (bin[i] != 0) {
	    sum += i * bin[i];
	    /* no "else" between the next 2 statements,
	     * because both min and max are changed the 1st
	     * time this block runs during the present run.
	     */
	    if (i < min) min = i;
	    if (i > max) max = i;
	}
	if (bin[i] > bin[mode]) {
	    mode = i;
	}
    }
    mean = (double)sum/(double)num_pixels;

    /*
     * Now do a second pass to compute median,
     * variance and skew. (median and cumulative_pixels initialized to 0;
     * var and skew initialized to 0.0)
     */
    for (i = 0; i < HISTOGRAM_BINS; i++) {
	if (cumulative_pixels < median_rank) {
	    cumulative_pixels += bin[i];
	    if (cumulative_pixels >= median_rank)
		median = i;
	}
	d = (double)i - mean;
	var += bin[i] * d * d;
	skew += bin[i] * d * d * d;
    }
    var /= (double)num_pixels;
    skew /= (double)num_pixels;

    /*
     * Display the results.
     */
    printf("Pixels  %14ld (%.0f x %.0f)\n", num_pixels,
	   sqrt((double)num_pixels), sqrt((double)num_pixels));
    printf("Min     %14d\n", min);
    printf("Max     %14d\n", max);
    printf("Mode    %14d (%ld pixels)\n", mode, bin[mode]);
    printf("Median  %14d\n", median);
    printf("Mean    %14.3f\n", mean);
    printf("s.d.    %14.3f\n", sqrt(var));
    printf("Var     %14.3f\n", var);
    printf("Skew    %14.3f\n", skew);

    if (args.verbose)
	show_hist(bin, num_pixels);

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
