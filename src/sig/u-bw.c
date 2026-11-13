/*                          U - B W . C
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
/** @file u-bw.c
 *
 * convert unsigned shorts to unsigned char.
 *
 */

#include "common.h"

#include "bio.h"

#include "bu/arg.h"
#include "bu/app.h"
#include "bu/exit.h"
#include "bu/malloc.h"

struct u_bw_args {
    int help;
    int low_byte;
};

static const struct bu_arg_option u_bw_options[] = {
    BU_ARG_FLAG("h", "help", struct u_bw_args, help, "Print help and exit"),
    BU_ARG_ALIAS("?", NULL, "help", 1),
    BU_ARG_FLAG("l", "low-byte", struct u_bw_args, low_byte,
	"Keep the low-order byte instead of the high-order byte"),
    BU_ARG_OPTION_NULL
};

static const struct bu_arg_schema u_bw_schema = BU_ARG_SCHEMA(
    "u-bw", "Convert unsigned shorts to unsigned bytes", u_bw_options, NULL,
    BU_ARG_PARSE_OPTIONS_FIRST,
    BU_ARG_SCHEMA_META_HELP(NULL, NULL, NULL, NULL, NULL));

static const struct bu_arg_grammar u_bw_grammar = {&u_bw_schema, NULL, NULL};

int
main(int argc, char *argv[])
{
    unsigned short ibuf[512];
    unsigned char obuf[512];
    struct u_bw_args args = {0};
    struct bu_vls msg = BU_VLS_INIT_ZERO;
    size_t num, i;

    bu_setprogname(argv[0]);

    int introspection = bu_arg_introspect(&msg, &u_bw_grammar, argc - 1,
	(const char * const *)(argv + 1));
    if (introspection != 0) {
	bu_flog(introspection > 0 ? stdout : stderr, "%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return introspection > 0 ? 0 : 1;
    }
    int operand_index = bu_arg_schema_parse_complete(&u_bw_schema, &args,
	&msg, argc - 1, (const char **)(argv + 1));
    if (operand_index < 0)
	bu_exit(1, "%s", bu_vls_cstr(&msg));
    bu_vls_free(&msg);
    if (args.help) {
	char *help = bu_arg_schema_help(&u_bw_schema, "u-bw");
	bu_flog(stdout, "%s", help ? help : "");
	bu_free(help, "u-bw help");
	return 0;
    }

    if (isatty(fileno(stdin)) || isatty(fileno(stdout)))
	bu_exit(1, "u-bw requires redirected binary input and output\n");
    setmode(fileno(stdin), O_BINARY);
    setmode(fileno(stdout), O_BINARY);

    while ((num = fread(ibuf, sizeof(*ibuf), 512, stdin)) > 0) {
	for (i = 0; i < num; i++)
	    obuf[i] = args.low_byte ? (unsigned char)ibuf[i] :
		(unsigned char)(ibuf[i] >> 8);

	if (fwrite(obuf, sizeof(*obuf), num, stdout) != num)
	    bu_exit(1, "u-bw: error writing output\n");
    }
    if (ferror(stdin))
	bu_exit(1, "u-bw: error reading input\n");

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
