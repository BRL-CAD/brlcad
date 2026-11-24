/*                        B W D I F F . C
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
/** @file util/bwdiff.c
 *
 * Take the difference between two BW files.
 * Output is: (file1-file2)/2 + 127
 * or magnitude (-m): abs(file1-file2)
 *
 */

#include "common.h"

#include <stdlib.h>
#include <string.h>
#include "bio.h"

#include "bu/arg.h"
#include "bu/app.h"
#include "bu/exit.h"
#include "bu/file.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "bu/str.h"


enum bwdiff_mode {
    BWDIFF_MODE_DIFFERENCE = 0,
    BWDIFF_MODE_MAGNITUDE,
    BWDIFF_MODE_GREATER,
    BWDIFF_MODE_LESS,
    BWDIFF_MODE_EQUAL,
    BWDIFF_MODE_NOT_EQUAL
};


struct bwdiff_args {
    int help;
    int background;
    int magnitude;
    int greater;
    int less;
    int equal;
    int not_equal;
};

static const struct bu_arg_option bwdiff_options[] = {
    BU_ARG_FLAG("h", "help", struct bwdiff_args, help,
	"Print help and exit"),
    BU_ARG_ALIAS("?", NULL, "help", 1),
    BU_ARG_FLAG("b", "background", struct bwdiff_args, background,
	"Show dimmed first-input values where relations are false"),
    BU_ARG_FLAG("m", "magnitude", struct bwdiff_args, magnitude,
	"Write absolute differences"),
    BU_ARG_FLAG("g", "greater", struct bwdiff_args, greater,
	"Mark samples where the first input is greater"),
    BU_ARG_FLAG("l", "less", struct bwdiff_args, less,
	"Mark samples where the first input is less"),
    BU_ARG_FLAG("e", "equal", struct bwdiff_args, equal,
	"Mark samples that are equal"),
    BU_ARG_FLAG("n", "not-equal", struct bwdiff_args, not_equal,
	"Mark samples that are not equal"),
    BU_ARG_OPTION_NULL
};

static const struct bu_arg_operand bwdiff_operands[] = {
    BU_ARG_OPERAND("file1", BU_ARG_VALUE_FILE, 1, 1,
	"First BW or PIX input, - for standard input, or . for null", NULL),
    BU_ARG_OPERAND("file2", BU_ARG_VALUE_FILE, 1, 1,
	"Second BW or PIX input, - for standard input, or . for null", NULL),
    BU_ARG_OPERAND_NULL
};

static const char * const bwdiff_modes[] = {
    "magnitude", "greater", "less", "equal", "not-equal", NULL
};

static const struct bu_arg_constraint bwdiff_constraints[] = {
    BU_ARG_CONSTRAINT(BU_ARG_CONSTRAINT_OPTION_COUNT, BU_ARG_CONDITION_ALWAYS,
	bwdiff_modes, 0, 1, "select at most one comparison mode"),
    BU_ARG_CONSTRAINT_NULL
};

static const struct bu_arg_schema bwdiff_schema = BU_ARG_SCHEMA(
    "bwdiff", "Compare two BW or PIX streams",
    bwdiff_options, bwdiff_operands, BU_ARG_PARSE_OPTIONS_FIRST,
    BU_ARG_SCHEMA_META_HELP(NULL, bwdiff_constraints, NULL, NULL, NULL));

static const struct bu_arg_grammar bwdiff_grammar = {
    &bwdiff_schema, NULL, NULL
};

static void
open_file(FILE **fp, const char *name)
{
    /* check for special names */
    if (BU_STR_EQUAL(name, "-")) {
	*fp = stdin;
    } else if (BU_STR_EQUAL(name, ".")) {
	*fp = fopen(bu_file_null(), "rb");
    } else {
	*fp = fopen(name, "rb");
    }
    if (!*fp)
	bu_exit(2, "bwdiff: Can't open \"%s\"\n", name);
}


int
main(int argc, char **argv)
{
    FILE *fp1, *fp2;
    unsigned char ibuf1[512], ibuf2[512], obuf[512];
    unsigned char *p1, *p2, *op;
    size_t i, n, m;
    enum bwdiff_mode mode = BWDIFF_MODE_DIFFERENCE;
    struct bwdiff_args args = {0};
    struct bu_vls msg = BU_VLS_INIT_ZERO;

    bu_setprogname(argv[0]);

    int introspection = bu_arg_introspect(&msg, &bwdiff_grammar, argc - 1,
	(const char * const *)(argv + 1));
    if (introspection != 0) {
	bu_flog(introspection > 0 ? stdout : stderr, "%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return introspection > 0 ? 0 : 1;
    }
    int operand_index = bu_arg_schema_parse_complete(&bwdiff_schema, &args,
	&msg, argc - 1, (const char **)(argv + 1));
    if (operand_index < 0)
	bu_exit(1, "%s", bu_vls_cstr(&msg));
    bu_vls_free(&msg);
    if (args.help) {
	char *help = bu_arg_schema_help(&bwdiff_schema, "bwdiff");
	bu_flog(stdout, "%s", help ? help : "");
	bu_free(help, "bwdiff help");
	return 0;
    }

    if (args.magnitude) mode = BWDIFF_MODE_MAGNITUDE;
    if (args.greater) mode = BWDIFF_MODE_GREATER;
    if (args.less) mode = BWDIFF_MODE_LESS;
    if (args.equal) mode = BWDIFF_MODE_EQUAL;
    if (args.not_equal) mode = BWDIFF_MODE_NOT_EQUAL;

    if (isatty(fileno(stdout)))
	bu_exit(1, "bwdiff: will not write binary output to a terminal\n");

    setmode(fileno(stdin), O_BINARY);
    setmode(fileno(stdout), O_BINARY);

    const char **operands = (const char **)(argv + operand_index + 1);
    open_file(&fp1, operands[0]);
    open_file(&fp2, operands[1]);

    while (1) {
	n = fread(ibuf1, 1, 512, fp1);
	m = fread(ibuf2, 1, 512, fp2);
	if (ferror(fp1) || ferror(fp2))
	    bu_exit(1, "bwdiff: error reading input\n");
	if ((n == 0) && (m == 0))
	    break;
	p1 = &ibuf1[0];
	p2 = &ibuf2[0];
	op = &obuf[0];
	if (m < n) {
	    memset((char *)(&ibuf2[m]), 0, (n - m));
	}
	if (m > n) {
	    memset((char *)(&ibuf1[n]), 0, (m - n));
	    n = m;
	}
	/* unrolled for speed */
	switch (mode) {
	    case BWDIFF_MODE_DIFFERENCE:
		for (i = 0; i < n; i++) {
		    *op++ = (((int)*p1 - (int)*p2)>>1) + 128;
		    p1++;
		    p2++;
		}
		break;
	    case BWDIFF_MODE_MAGNITUDE:
		for (i = 0; i < n; i++) {
		    *op++ = abs((int)*p1++ - (int)*p2++);
		}
		break;
	    case BWDIFF_MODE_GREATER:
		for (i = 0; i < n; i++) {
		    if (*p1 > *p2++)
			*op++ = 255;
		    else {
			if (args.background)
			    *op++ = *p1 >> 2;
			else
			    *op++ = 0;
		    }
		    p1++;
		}
		break;
	    case BWDIFF_MODE_LESS:
		for (i = 0; i < n; i++) {
		    if (*p1 < *p2++)
			*op++ = 255;
		    else {
			if (args.background)
			    *op++ = *p1 >> 2;
			else
			    *op++ = 0;
		    }
		    p1++;
		}
		break;
	    case BWDIFF_MODE_EQUAL:
		for (i = 0; i < n; i++) {
		    if (*p1 == *p2++)
			*op++ = 255;
		    else {
			if (args.background)
			    *op++ = *p1 >> 2;
			else
			    *op++ = 0;
		    }
		    p1++;
		}
		break;
	    case BWDIFF_MODE_NOT_EQUAL:
		for (i = 0; i < n; i++) {
		    if (*p1 != *p2++)
			*op++ = 255;
		    else {
			if (args.background)
			    *op++ = (*p1) >> 1;
			else
			    *op++ = 0;
		    }
		    p1++;
		}
		break;
	}
	if (fwrite(&obuf[0], 1, n, stdout) != n)
	    bu_exit(1, "bwdiff: error writing output\n");
    }
    int close_failed = 0;
    if (fp1 != stdin && fclose(fp1) == EOF)
	close_failed = 1;
    if (fp2 != stdin && fclose(fp2) == EOF)
	close_failed = 1;
    if (close_failed)
	bu_exit(1, "bwdiff: error closing input\n");
    if (fflush(stdout) == EOF)
	bu_exit(1, "bwdiff: error writing output\n");
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
