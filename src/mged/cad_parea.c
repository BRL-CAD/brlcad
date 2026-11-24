/*                     C A D _ P A R E A . C
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
 */
/** @file mged/cad_parea.c
 *
 * cad_parea -- area of polygon
 *
 */

#include "common.h"

#include <stdlib.h>
#include <string.h>
#include "bio.h"

#include "bu/arg.h"
#include "bu/app.h"
#include "bu/str.h"
#include "bu/exit.h"
#include "bu/log.h"


typedef struct
{
    double x;			/* X coordinate */
    double y;			/* Y coordinate */
} point;			/* polygon vertex */

static int GetArgs(int argc, const char *argv[]), Input(point *coop);
static void Output(double result);

struct cad_parea_args {
    int help;
    const char *input;
    const char *output;
};

static const struct bu_arg_option cad_parea_options[] = {
    BU_ARG_FLAG("h", "help", struct cad_parea_args, help,
	"Print help and exit"),
    BU_ARG_ALIAS("?", NULL, "help", 1),
    BU_ARG_VALUE("i", "input", struct cad_parea_args, input,
	BU_ARG_VALUE_FILE, BU_ARG_REQUIRED, "file", "Read polygon vertices from a file"),
    BU_ARG_VALUE("o", "output", struct cad_parea_args, output,
	BU_ARG_VALUE_FILE, BU_ARG_REQUIRED, "file", "Write the computed area to a file"),
    BU_ARG_OPTION_NULL
};
static const char * const cad_parea_input_option[] = {"input", NULL};
static const char * const cad_parea_output_option[] = {"output", NULL};
static const struct bu_arg_constraint cad_parea_constraints[] = {
    BU_ARG_CONSTRAINT(BU_ARG_CONSTRAINT_OPTION_OCCURRENCE_COUNT,
	BU_ARG_CONDITION_ALWAYS, cad_parea_input_option, 0, 1,
	"--input may be specified only once"),
    BU_ARG_CONSTRAINT(BU_ARG_CONSTRAINT_OPTION_OCCURRENCE_COUNT,
	BU_ARG_CONDITION_ALWAYS, cad_parea_output_option, 0, 1,
	"--output may be specified only once"),
    BU_ARG_CONSTRAINT_NULL
};

static const struct bu_arg_schema cad_parea_schema = BU_ARG_SCHEMA(
    "cad_parea", "Compute the signed area of a two-dimensional polygon",
    cad_parea_options, NULL, BU_ARG_PARSE_OPTIONS_FIRST,
    BU_ARG_SCHEMA_META_HELP(NULL, cad_parea_constraints, NULL, NULL, NULL));

static const struct bu_arg_grammar cad_parea_grammar = {
    &cad_parea_schema, NULL, NULL
};


int
main(int argc, const char *argv[])			/* "cad_parea" entry point */
    /* argument count */
    /* argument strings */
{
    point previous = {0.0, 0.0}; /* previous point */
    point current = {0.0, 0.0}; /* current point */
    point first = {0.0, 0.0}; /* saved first point */
    int saved; /* "`first' valid" flag */
    double sum; /* accumulator */
    int arg_status;

    bu_setprogname(argv[0]);

    arg_status = GetArgs(argc, argv);
    if (arg_status <= 0)
	return arg_status < 0 ? 1 : 0;

    saved = 0;
    sum = 0.0;

    while (Input(&current)) {
	/* scan input record */
	if (!saved) {
	    /* first input only */
	    first = current;
	    saved = 1;
	} else	/* accumulate cross-product */
	    sum += previous.x * current.y -
		previous.y * current.x;

	previous = current;
    }

    if (saved)			/* normally true */
	sum += previous.x * first.y - previous.y * first.x;

    Output(sum / 2.0);
    return 0;			/* success! */
}


static int
GetArgs(int argc, const char *argv[])			/* process command arguments */
    /* argument count */
    /* argument strings */
{
    struct cad_parea_args args = {0, NULL, NULL};
    struct bu_vls msg = BU_VLS_INIT_ZERO;
    int introspection;
    int operand_index;

    introspection = bu_arg_introspect(&msg, &cad_parea_grammar, argc - 1,
	argv + 1);
    if (introspection != 0) {
	bu_flog(introspection > 0 ? stdout : stderr, "%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return introspection > 0 ? 0 : -1;
    }
    operand_index = bu_arg_schema_parse_complete(&cad_parea_schema, &args,
	&msg, argc - 1, argv + 1);
    if (operand_index < 0) {
	bu_log("%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return -1;
    }
    bu_vls_free(&msg);
    if (args.help) {
	char *help = bu_arg_schema_help(&cad_parea_schema, "cad_parea");
	if (help) {
	    bu_flog(stdout, "%s", help);
	    bu_free(help, "cad_parea help");
	}
	return 0;
    }
    if (args.input && !BU_STR_EQUAL(args.input, "-") &&
	freopen(args.input, "r", stdin) == NULL) {
	bu_log("cad_parea: cannot open \"%s\" for reading\n", args.input);
	return -1;
    }
    if (args.output && !BU_STR_EQUAL(args.output, "-") &&
	freopen(args.output, "w", stdout) == NULL) {
	bu_log("cad_parea: cannot open \"%s\" for writing\n", args.output);
	return -1;
    }

    return 1;
}


static int
Input(point *coop)				/* input a coordinate record */
    /* -> input coordinates */
{
    char inbuf[82];	/* input record buffer */

    while (bu_fgets(inbuf, (int)sizeof inbuf, stdin) != NULL) {
	/* scan input record */
	int cvt;	/* # converted fields */

	cvt = sscanf(inbuf, " %le %le", &coop->x, &coop->y);

	if (cvt == 0)
	    continue;	/* skip color, comment, etc. */

	if (cvt == 2)
	    return 1;	/* successfully converted */

	printf("cad_parea: bad input:\n%s\n", inbuf);
	bu_exit(2, NULL);		/* return false insufficient */
    }

    return 0;			/* EOF */
}


static void
Output(double result)			/* output computed area */
    /* computed area */
{
    printf("%g\n", result);
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
