/*                      B W H I S T E Q . C
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
/** @file util/bwhisteq.c
 *
 * Build up the histogram of a picture and output the "equalized"
 * version of it on stdout.
 *
 */

#include "common.h"

#include "bio.h"

#include "bu/arg.h"
#include "bu/app.h"
#include "bu/file.h"
#include "bu/exit.h"
#include "bu/malloc.h"


struct bwhisteq_args {
    int help;
    int verbose;
};

static const struct bu_arg_option bwhisteq_options[] = {
    BU_ARG_FLAG("h", "help", struct bwhisteq_args, help,
	"Print help and exit"),
    BU_ARG_ALIAS("?", NULL, "help", 1),
    BU_ARG_FLAG("v", "verbose", struct bwhisteq_args, verbose,
	"Print the pixel-value mapping"),
    BU_ARG_OPTION_NULL
};

static const struct bu_arg_operand bwhisteq_operands[] = {
    BU_ARG_OPERAND("input", BU_ARG_VALUE_FILE, 1, 1, "BW input file", NULL),
    BU_ARG_OPERAND_NULL
};

static const struct bu_arg_schema bwhisteq_schema = BU_ARG_SCHEMA(
    "bwhisteq", "Equalize the histogram of a BW image",
    bwhisteq_options, bwhisteq_operands, BU_ARG_PARSE_OPTIONS_FIRST,
    BU_ARG_SCHEMA_META_HELP(NULL, NULL, NULL, NULL, NULL));

static const struct bu_arg_grammar bwhisteq_grammar = {
    &bwhisteq_schema, NULL, NULL
};

#define HISTOGRAM_BINS 256
#define IO_BUFFER_SIZE 512


int
main(int argc, char **argv)
{
    FILE *fp = NULL;
    struct bwhisteq_args args = {0};
    struct bu_vls msg = BU_VLS_INIT_ZERO;

    double havg = 0.0;
    double hint = 0.0;
    size_t i = 0;
    size_t n = 0;
    int r = 0;
    unsigned char *bp = NULL;

    unsigned char buf[IO_BUFFER_SIZE] = {0};
    unsigned char obuf[IO_BUFFER_SIZE] = {0};
    long bin[HISTOGRAM_BINS] = {0};
    unsigned char result[HISTOGRAM_BINS] = {0};
    int left[HISTOGRAM_BINS] = {0};
    int right[HISTOGRAM_BINS] = {0};

    bu_setprogname(argv[0]);
    setmode(fileno(stdout), O_BINARY);

    int introspection = bu_arg_introspect(&msg, &bwhisteq_grammar, argc - 1,
	(const char * const *)(argv + 1));
    if (introspection != 0) {
	bu_flog(introspection > 0 ? stdout : stderr, "%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return introspection > 0 ? 0 : 1;
    }
    int operand_index = bu_arg_schema_parse_complete(&bwhisteq_schema, &args,
	&msg, argc - 1, (const char **)(argv + 1));
    if (operand_index < 0)
	bu_exit(1, "%s", bu_vls_cstr(&msg));
    bu_vls_free(&msg);
    if (args.help) {
	char *help = bu_arg_schema_help(&bwhisteq_schema, "bwhisteq");
	bu_flog(stdout, "%s", help ? help : "");
	bu_free(help, "bwhisteq help");
	return 0;
    }

    if (isatty(fileno(stdout)))
	bu_exit(1, "bwhisteq requires redirected binary output\n");

    const char *path = argv[operand_index + 1];
    if ((fp = fopen(path, "rb")) == NULL)
	bu_exit(2, "bwhisteq: cannot open '%s' for reading\n", path);

    /* Tally up the intensities */
    havg = 0.0;
    while ((n = fread(buf, sizeof(*buf), IO_BUFFER_SIZE, fp)) > 0) {
	bp = &buf[0];
	for (i = 0; i < n; i++)
	    bin[ *bp++ ]++;
	havg += n;
    }
    if (ferror(fp))
	bu_exit(1, "bwhisteq: error reading input\n");
    havg /= (double)HISTOGRAM_BINS;

    r = 0;
    hint = 0;
    for (i = 0; i < HISTOGRAM_BINS; i++) {
	left[i] = r;
	hint += bin[i];
	while (hint > havg) {
	    hint -= havg;
	    r++;
	}
	right[i] = r;
	result[i] = (left[i] + right[i]) / 2;
    }

    if (args.verbose) {
	for (i = 0; i < HISTOGRAM_BINS; i++)
	    fprintf(stderr, "result[%zu] = %d\n", i, result[i]);
    }

    if (bu_fseek(fp, 0, SEEK_SET) != 0)
	bu_exit(1, "bwhisteq: error rewinding input\n");
    while ((n = fread(buf, 1, IO_BUFFER_SIZE, fp)) > 0) {
	for (i = 0; i < n; i++) {
	    unsigned char idx = buf[i];

	    if (left[idx] == right[idx])
		obuf[i] = left[idx];
	    else {
		obuf[i] = result[idx];
	    }
	}
	if (fwrite(obuf, 1, n, stdout) != (size_t)n)
	    bu_exit(1, "bwhisteq: error writing output\n");
    }
    if (ferror(fp))
	bu_exit(1, "bwhisteq: error rereading input\n");
    if (fclose(fp) == EOF)
	bu_exit(1, "bwhisteq: error closing input\n");
    if (ferror(stdout) || fflush(stdout) == EOF)
	bu_exit(1, "bwhisteq: error writing output\n");
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
