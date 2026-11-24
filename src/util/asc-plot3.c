/*                     A S C - P L O T 3 . C
 * BRL-CAD
 *
 * Copyright (c) 1990-2026 United States Government as represented by
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
/** @file util/asc-plot3.c
 *
 * Produce UNIX plot commands in PLOT3(5) format from ASCII
 * representation.
 *
 */

#include "common.h"

#include <string.h>
#include "bio.h"

#include "bv/plot3.h"

#include "bu/arg.h"
#include "bu/app.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "bu/str.h"

#define BUF_LEN 512
#define FP_IN 0
#define FP_OUT 1


struct asc_plot3_args {
    int help;
};

static const struct bu_arg_option asc_plot3_options[] = {
    BU_ARG_FLAG("h", "help", struct asc_plot3_args, help,
	"Print help and exit"),
    BU_ARG_ALIAS("?", NULL, "help", 1),
    BU_ARG_OPTION_NULL
};

static const struct bu_arg_operand asc_plot3_operands[] = {
    BU_ARG_OPERAND("input", BU_ARG_VALUE_FILE, 0, 1,
	"ASCII input file (default: standard input)", NULL),
    BU_ARG_OPERAND("output", BU_ARG_VALUE_FILE, 0, 1,
	"PLOT3 output file (default: standard output)", NULL),
    BU_ARG_OPERAND_NULL
};

static const struct bu_arg_schema asc_plot3_schema = BU_ARG_SCHEMA(
    "asc-plot3", "Convert ASCII plot commands to binary PLOT3",
    asc_plot3_options, asc_plot3_operands, BU_ARG_PARSE_OPTIONS_FIRST,
    BU_ARG_SCHEMA_META_HELP(NULL, NULL, NULL, NULL, NULL));

static const struct bu_arg_grammar asc_plot3_grammar = {
    &asc_plot3_schema, NULL, NULL
};


static int
check_syntax (char cmd, int needed, int got, int line)
{
    if (got < needed) {
	bu_exit(1, "Too few arguments for '%c' command on line %d\n", cmd, line);
    }
    return 1;
}


int
main (int argc, char **argv)
{
    char *bp = NULL;
    char buf[BUF_LEN] = {'\0'};
    char sarg[BUF_LEN] = {'\0'};
    double darg[6] = {0.0};
    FILE *fp[2] = {stdin, stdout};
    int iarg[6] = {0};
    int line_nm = 0;
    int nm_args = 0;
    struct asc_plot3_args args = {0};
    struct bu_vls msg = BU_VLS_INIT_ZERO;

    bu_setprogname(argv[0]);

    int introspection = bu_arg_introspect(&msg, &asc_plot3_grammar, argc - 1,
	(const char * const *)(argv + 1));
    if (introspection != 0) {
	bu_flog(introspection > 0 ? stdout : stderr, "%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return introspection > 0 ? 0 : 1;
    }
    int operand_index = bu_arg_schema_parse_complete(&asc_plot3_schema, &args,
	&msg, argc - 1, (const char **)(argv + 1));
    if (operand_index < 0)
	bu_exit(1, "%s", bu_vls_cstr(&msg));
    bu_vls_free(&msg);
    if (args.help) {
	char *help = bu_arg_schema_help(&asc_plot3_schema, "asc-plot3");
	bu_flog(stdout, "%s", help ? help : "");
	bu_free(help, "asc-plot3 help");
	return 0;
    }

    setmode(fileno(stdin), O_BINARY);
    setmode(fileno(stdout), O_BINARY);

    int operand_count = argc - operand_index - 1;
    const char **operands = (const char **)(argv + operand_index + 1);
    if (operand_count > 0 && !BU_STR_EQUAL(operands[0], "-")) {
	fp[FP_IN] = fopen(operands[0], "rb");
	if (!fp[FP_IN])
	    bu_exit(1, "asc-plot3: cannot open '%s' for reading\n", operands[0]);
    }
    if (operand_count > 1 && !BU_STR_EQUAL(operands[1], "-")) {
	fp[FP_OUT] = fopen(operands[1], "wb");
	if (!fp[FP_OUT])
	    bu_exit(1, "asc-plot3: cannot open '%s' for writing\n", operands[1]);
    }
    if (isatty(fileno(fp[FP_OUT]))) {
	bu_exit(1, "asc-plot3: will not write binary output to a terminal\n");
    }

    for (line_nm = 1; bu_fgets(buf, BUF_LEN - 1, fp[FP_IN]) != NULL; ++line_nm) {
	for (bp = buf; *bp != '\0'; ++bp)
	    ;
	*bp = '\n';
	*(bp + 1) = '\0';
	for (bp = buf; (*bp == ' ') || (*bp == '\t'); ++bp)
	    ;
	if (*bp == '\n')
	    continue;
	if (strchr("aclmnpsCLMNPS", *bp))
	    nm_args = sscanf(bp + 1, "%d%d%d%d%d%d",
			     &iarg[0], &iarg[1], &iarg[2],
			     &iarg[3], &iarg[4], &iarg[5]);
	else if (strchr("ioqrvwxOQVWX", *bp))
	    nm_args = sscanf(bp + 1, "%lf%lf%lf%lf%lf%lf",
			     &darg[0], &darg[1], &darg[2],
			     &darg[3], &darg[4], &darg[5]);
	else if (strchr("ft", *bp))
	    nm_args = sscanf(bp, "%*[^\"]\"%[^\"]\"", sarg);

	switch (*bp) {
	    case '#':
		break;
	    case 'a':
		if (check_syntax(*bp, 6, nm_args, line_nm))
		    pl_arc(fp[FP_OUT], iarg[0], iarg[1], iarg[2], iarg[3],
			   iarg[4], iarg[5]);
		break;
	    case 'c':
		if (check_syntax(*bp, 3, nm_args, line_nm))
		    pl_circle(fp[FP_OUT], iarg[0], iarg[1], iarg[2]);
		break;
	    case 'l':
		if (check_syntax(*bp, 4, nm_args, line_nm))
		    pl_line(fp[FP_OUT], iarg[0], iarg[1], iarg[2], iarg[3]);
		break;
	    case 'm':
		if (check_syntax(*bp, 2, nm_args, line_nm))
		    pl_move(fp[FP_OUT], iarg[0], iarg[1]);
		break;
	    case 'n':
		if (check_syntax(*bp, 2, nm_args, line_nm))
		    pl_cont(fp[FP_OUT], iarg[0], iarg[1]);
		break;
	    case 'p':
		if (check_syntax(*bp, 2, nm_args, line_nm))
		    pl_point(fp[FP_OUT], iarg[0], iarg[1]);
		break;
	    case 's':
		if (check_syntax(*bp, 4, nm_args, line_nm))
		    pl_space(fp[FP_OUT], iarg[0], iarg[1], iarg[2], iarg[3]);
		break;
	    case 'C':
		if (check_syntax(*bp, 3, nm_args, line_nm))
		    pl_color(fp[FP_OUT], iarg[0], iarg[1], iarg[2]);
		break;
	    case 'L':
		if (check_syntax(*bp, 6, nm_args, line_nm))
		    pl_3line(fp[FP_OUT], iarg[0], iarg[1], iarg[2], iarg[3],
			     iarg[4], iarg[5]);
		break;
	    case 'M':
		if (check_syntax(*bp, 3, nm_args, line_nm))
		    pl_3move(fp[FP_OUT], iarg[0], iarg[1], iarg[2]);
		break;
	    case 'N':
		if (check_syntax(*bp, 3, nm_args, line_nm))
		    pl_3cont(fp[FP_OUT], iarg[0], iarg[1], iarg[2]);
		break;
	    case 'P':
		if (check_syntax(*bp, 3, nm_args, line_nm))
		    pl_3point(fp[FP_OUT], iarg[0], iarg[1], iarg[2]);
		break;
	    case 'S':
		if (check_syntax(*bp, 6, nm_args, line_nm))
		    pl_3space(fp[FP_OUT], iarg[0], iarg[1], iarg[2], iarg[3],
			      iarg[4], iarg[5]);
		break;
	    case 'i':
		if (check_syntax(*bp, 3, nm_args, line_nm))
		    pd_circle(fp[FP_OUT], darg[0], darg[1], darg[2]);
		break;
	    case 'o':
		if (check_syntax(*bp, 2, nm_args, line_nm))
		    pd_move(fp[FP_OUT], darg[0], darg[1]);
		break;
	    case 'q':
		if (check_syntax(*bp, 2, nm_args, line_nm))
		    pd_cont(fp[FP_OUT], darg[0], darg[1]);
		break;
	    case 'r':
		if (check_syntax(*bp, 6, nm_args, line_nm))
		    pd_arc(fp[FP_OUT], darg[0], darg[1], darg[2], darg[3],
			   darg[4], darg[5]);
		break;
	    case 'v':
		if (check_syntax(*bp, 4, nm_args, line_nm))
		    pd_line(fp[FP_OUT], darg[0], darg[1], darg[2], darg[3]);
		break;
	    case 'w':
		if (check_syntax(*bp, 4, nm_args, line_nm))
		    pd_space(fp[FP_OUT], darg[0], darg[1], darg[2], darg[3]);
		break;
	    case 'x':
		if (check_syntax(*bp, 2, nm_args, line_nm))
		    pd_point(fp[FP_OUT], darg[0], darg[1]);
		break;
	    case 'O':
		if (check_syntax(*bp, 3, nm_args, line_nm))
		    pd_3move(fp[FP_OUT], darg[0], darg[1], darg[2]);
		break;
	    case 'Q':
		if (check_syntax(*bp, 3, nm_args, line_nm))
		    pd_3cont(fp[FP_OUT], darg[0], darg[1], darg[2]);
		break;
	    case 'V':
		if (check_syntax(*bp, 6, nm_args, line_nm))
		    pd_3line(fp[FP_OUT], darg[0], darg[1], darg[2], darg[3],
			     darg[4], darg[5]);
		break;
	    case 'W':
		if (check_syntax(*bp, 6, nm_args, line_nm))
		    pd_3space(fp[FP_OUT], darg[0], darg[1], darg[2], darg[3],
			      darg[4], darg[5]);
		break;
	    case 'X':
		if (check_syntax(*bp, 3, nm_args, line_nm))
		    pd_3point(fp[FP_OUT], darg[0], darg[1], darg[2]);
		break;
	    case 'F':
		pl_flush(fp[FP_OUT]);
		break;
	    case 'e':
		pl_erase(fp[FP_OUT]);
		break;
	    case 'f':
		if (check_syntax(*bp, 1, nm_args, line_nm))
		    pl_linmod(fp[FP_OUT], sarg);
		break;
	    case 't':
		if (check_syntax(*bp, 1, nm_args, line_nm))
		    pl_label(fp[FP_OUT], sarg);
		break;
	    default:
		bu_exit(1, "Unknown PLOT3 command: '%c' (o%o) on line %d\n",
			*bp, *bp, line_nm);
	}
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
