/*                       P A R S A R G . C
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
/** @file vdeck/parsarg.c
 *
 */

#include "common.h"

#include "bio.h"

#include "bu/arg.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "bu/vls.h"
#include "vmath.h"
#include "raytrace.h"

#include "./vextern.h"


struct vdeck_args {
    int debug;
    int help;
};

static const struct bu_arg_option vdeck_options[] = {
    BU_ARG_FLAG("d", "debug", struct vdeck_args, debug,
	"Enable debug output"),
    BU_ARG_FLAG("h", "help", struct vdeck_args, help,
	"Print help and exit"),
    BU_ARG_ALIAS("?", NULL, "help", 1),
    BU_ARG_OPTION_NULL
};

static const struct bu_arg_operand vdeck_operands[] = {
    BU_ARG_OPERAND("file.g", BU_ARG_VALUE_FILE, 1, 1,
	"Input BRL-CAD database", NULL),
    BU_ARG_OPERAND("object", BU_ARG_VALUE_DB_OBJECT, 0, MAXARG - 1,
	"Initial database object selection", NULL),
    BU_ARG_OPERAND_NULL
};

static const struct bu_arg_schema vdeck_schema = BU_ARG_SCHEMA(
    "vdeck", "Generate a COMGEOM card deck", vdeck_options,
    vdeck_operands, BU_ARG_PARSE_INTERSPERSED,
    BU_ARG_SCHEMA_META_HELP(NULL, NULL, NULL, NULL, NULL));

static const struct bu_arg_grammar vdeck_grammar = {
    &vdeck_schema, NULL, NULL
};


/*
 * Parse the command line arguments.
 */
int
parsArg(int argc, char **argv)
{
    struct vdeck_args args = {0};
    struct bu_vls msg = BU_VLS_INIT_ZERO;
    int arg_cnt;
    int i;
    int introspection;
    int operand_index;

    introspection = bu_arg_introspect(&msg, &vdeck_grammar, argc - 1,
	(const char * const *)(argv + 1));
    if (introspection != 0) {
	bu_flog(introspection > 0 ? stdout : stderr, "%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return introspection > 0 ? -1 : 0;
    }
    operand_index = bu_arg_schema_parse_complete(&vdeck_schema, &args,
	&msg, argc - 1, (const char **)(argv + 1));
    if (operand_index < 0) {
	bu_log("%s\n", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return 0;
    }
    bu_vls_free(&msg);
    if (args.help) {
	char *help = bu_arg_schema_help(&vdeck_schema, "vdeck");
	bu_flog(stdout, "%s", help ? help : "");
	bu_free(help, "vdeck help");
	return -1;
    }
    debug = args.debug;
    objfile = argv[operand_index + 1];
    if ((dbip = db_open(objfile, DB_OPEN_READONLY)) == DBI_NULL) {
	perror(objfile);
	return 0;		/* FAIL */
    }

    arg_list[0] = argv[0]; /* Program name goes in first. */
    for (i = operand_index + 2, arg_cnt = 1; i < argc; i++, arg_cnt++)
	/* Insert objects.	*/
	arg_list[arg_cnt] = argv[i];
    return 1;
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
