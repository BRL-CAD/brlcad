/*                           C - D . C
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
/** @file c-d.c
 *
 */

#include "common.h"

#include <math.h>
#include "bio.h"

#include "bu/arg.h"
#include "bu/app.h"
#include "bu/exit.h"
#include "bu/malloc.h"
#include "bu/snooze.h"
#include "vmath.h"


#if defined(HAVE_HYPOT) && !defined(HAVE_DECL_HYPOT)
extern double hypot(double x, double y);
#endif

struct c_d_args {
    int help;
    int real;
    int imaginary;
    int magnitude;
    int phase;
    int zero;
};

static const struct bu_arg_option c_d_options[] = {
    BU_ARG_FLAG("h", "help", struct c_d_args, help, "Print help and exit"),
    BU_ARG_ALIAS("?", NULL, "help", 1),
    BU_ARG_FLAG("r", "real", struct c_d_args, real,
	"Output each real component"),
    BU_ARG_FLAG("i", "imaginary", struct c_d_args, imaginary,
	"Output each imaginary component"),
    BU_ARG_FLAG("m", "magnitude", struct c_d_args, magnitude,
	"Output each magnitude"),
    BU_ARG_FLAG("p", "phase", struct c_d_args, phase,
	"Output each phase angle"),
    BU_ARG_FLAG("z", "zero", struct c_d_args, zero,
	"Output a zero for each complex value"),
    BU_ARG_OPTION_NULL
};

static const struct bu_arg_schema c_d_schema = BU_ARG_SCHEMA(
    "c-d", "Extract components from complex double-precision data",
    c_d_options, NULL, BU_ARG_PARSE_OPTIONS_FIRST,
    BU_ARG_SCHEMA_META_HELP(NULL, NULL, NULL, NULL, NULL));

static const struct bu_arg_grammar c_d_grammar = {&c_d_schema, NULL, NULL};


int
main(int argc, char *argv[])
{
    double ibuf[512];
    double obuf[512];
    double *obp;

    size_t i, num, onum;
    size_t ret;
    struct c_d_args args = {0};
    struct bu_vls msg = BU_VLS_INIT_ZERO;

    bu_setprogname(argv[0]);

    int introspection = bu_arg_introspect(&msg, &c_d_grammar, argc - 1,
	(const char * const *)(argv + 1));
    if (introspection != 0) {
	bu_flog(introspection > 0 ? stdout : stderr, "%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return introspection > 0 ? 0 : 1;
    }
    int operand_index = bu_arg_schema_parse_complete(&c_d_schema, &args,
	&msg, argc - 1, (const char **)(argv + 1));
    if (operand_index < 0)
	bu_exit(1, "%s", bu_vls_cstr(&msg));
    bu_vls_free(&msg);
    if (args.help) {
	char *help = bu_arg_schema_help(&c_d_schema, "c-d");
	bu_flog(stdout, "%s", help ? help : "");
	bu_free(help, "c-d help");
	return 0;
    }
    if (!args.real && !args.imaginary && !args.magnitude && !args.phase &&
	!args.zero)
	bu_exit(1, "c-d requires at least one component option\n");

    fprintf(stderr,"DEPRECATION WARNING:  This command is scheduled for removal.  Please contact the developers if you use this command.\n\n");
    bu_snooze(BU_SEC2USEC(1));

    if (isatty(fileno(stdin)))
	bu_exit(1, "c-d requires redirected binary input\n");

    while ((num = fread(&ibuf[0], sizeof(ibuf[0]), 512, stdin)) > 0) {
	onum = 0;
	obp = obuf;
	for (i = 0; i < num; i += 2) {
	    if (args.real) {
		*obp++ = ibuf[i];
		onum++;
	    }
	    if (args.imaginary) {
		*obp++ = ibuf[i+1];
		onum++;
	    }
	    if (args.magnitude) {
		*obp++ = hypot(ibuf[i], ibuf[i+1]);
		onum++;
	    }
	    if (args.phase) {
		if (ZERO(ibuf[i]) && ZERO(ibuf[i+1]))
		    *obp++ = 0;
		else
		    *obp++ = atan2(ibuf[i], ibuf[i+1]);
		onum++;
	    }
	    if (args.zero) {
		*obp++ = 0.0;
		onum++;
	    }
	}
	ret = fwrite(&obuf[0], sizeof(obuf[0]), onum, stdout);
	if (ret != onum)
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
