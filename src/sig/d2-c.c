/*                          D 2 - C . C
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
/** @file d2-c.c
 *
 * Merge two double files into one complex file.
 *
 */

#include "common.h"

#include <string.h>
#include "bio.h"

#include "bu/arg.h"
#include "bu/app.h"
#include "bu/exit.h"
#include "bu/file.h"
#include "bu/malloc.h"
#include "bu/str.h"
#include "bu/snooze.h"

struct d2_c_args {
    int help;
};

static const struct bu_arg_option d2_c_options[] = {
    BU_ARG_FLAG("h", "help", struct d2_c_args, help, "Print help and exit"),
    BU_ARG_ALIAS("?", NULL, "help", 1),
    BU_ARG_OPTION_NULL
};

static const struct bu_arg_operand d2_c_operands[] = {
    BU_ARG_OPERAND("real-file", BU_ARG_VALUE_FILE, 1, 1,
	"Binary doubles supplying real components", NULL),
    BU_ARG_OPERAND("imaginary-file", BU_ARG_VALUE_FILE, 1, 1,
	"Binary doubles supplying imaginary components", NULL),
    BU_ARG_OPERAND_NULL
};

static const struct bu_arg_schema d2_c_schema = BU_ARG_SCHEMA(
    "d2-c", "Merge real and imaginary double streams into complex data",
    d2_c_options, d2_c_operands, BU_ARG_PARSE_OPTIONS_FIRST,
    BU_ARG_SCHEMA_META_HELP(NULL, NULL, NULL, NULL, NULL));

static const struct bu_arg_grammar d2_c_grammar = {&d2_c_schema, NULL, NULL};


static void
open_file(FILE **fp, char *name)
{
    /* check for special names */
    if (BU_STR_EQUAL(name, "-")) {
	*fp = stdin;
	return;
    } else if (BU_STR_EQUAL(name, ".")) {
	*fp = fopen(bu_file_null(), "r");
	return;
    }

    if ((*fp = fopen(name, "r")) == NULL) {
	bu_exit(2, "d2-c: Can't open \"%s\"\n", name);
    }
}


int
main(int argc, char *argv[])
{
    int i;
    int nr, ni, num;
    double obuf[2*1024];
    double *obufp;
    FILE *rfp, *ifp;
    double real[1024], imag[1024];
    size_t ret;
    struct d2_c_args args = {0};
    struct bu_vls msg = BU_VLS_INIT_ZERO;

    bu_setprogname(argv[0]);

    int introspection = bu_arg_introspect(&msg, &d2_c_grammar, argc - 1,
	(const char * const *)(argv + 1));
    if (introspection != 0) {
	bu_flog(introspection > 0 ? stdout : stderr, "%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return introspection > 0 ? 0 : 1;
    }
    argc--; argv++;
    int operand_index = bu_arg_schema_parse_complete(&d2_c_schema, &args,
	&msg, argc, (const char **)argv);
    if (operand_index < 0)
	bu_exit(1, "%s", bu_vls_cstr(&msg));
    bu_vls_free(&msg);
    if (args.help) {
	char *help = bu_arg_schema_help(&d2_c_schema, "d2-c");
	bu_flog(stdout, "%s", help ? help : "");
	bu_free(help, "d2-c help");
	return 0;
    }
    argc -= operand_index;
    argv += operand_index;

    fprintf(stderr,"DEPRECATION WARNING:  This command is scheduled for removal.  Please contact the developers if you use this command.\n\n");
    bu_snooze(BU_SEC2USEC(1));

    if (isatty(fileno(stdout)))
	bu_exit(1, "d2-c requires redirected binary output\n");

    open_file(&rfp, argv[0]);
    open_file(&ifp, argv[1]);

    while (1) {
	nr = fread(real, sizeof(double), 1024, rfp);
	ni = fread(imag, sizeof(double), 1024, ifp);
	if (nr <= 0 && ni <= 0)
	    break;

	/* find max */
	num = (nr > ni) ? nr : ni;
	if (nr < num)
	    memset(&real[nr], 0, (num-nr)*sizeof(double));
	if (ni < num)
	    memset(&imag[ni], 0, (num-ni)*sizeof(double));

	obufp = &obuf[0];
	for (i = 0; i < num; i++) {
	    *obufp++ = real[i];
	    *obufp++ = imag[i];
	}
	ret = fwrite(obuf, sizeof(double), num*2, stdout);
	if (ret != (size_t)(num*2))
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
