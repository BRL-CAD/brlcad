/*                      T G F - G . C P P
 * BRL-CAD
 *
 * Copyright (c) 2008-2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file tgf-g.cpp
 *
 * INTAVAL Target Geometry File to BRL-CAD converter:
 * main function
 *
 *  Origin -
 *	TNO (Netherlands)
 *	IABG mbH (Germany)
 */

#include "common.h"

#include <cstdio>

#ifndef HAVE_DECL_FSEEKO
#include "bio.h" /* for b_off_t */
extern "C" int fseeko(FILE *, b_off_t, int);
extern "C" b_off_t ftello(FILE *);
#endif
#include <fstream>

#include "bu/app.h"
#include "bu/arg.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "bu/vls.h"

#include "regtab.h"
#include "read_dra.h"


struct tgf_g_args {
    int help;
};

static const struct bu_arg_option tgf_g_options[] = {
    BU_ARG_FLAG("h", "help", struct tgf_g_args, help,
	"Print help and exit"),
    BU_ARG_ALIAS("?", NULL, "help", 1),
    BU_ARG_OPTION_NULL
};

static const struct bu_arg_operand tgf_g_operands[] = {
    BU_ARG_OPERAND("material_file", BU_ARG_VALUE_FILE, 1, 1,
	"Input INTAVAL material file", NULL),
    BU_ARG_OPERAND("geometry_file", BU_ARG_VALUE_FILE, 1, 1,
	"Input INTAVAL geometry file", NULL),
    BU_ARG_OPERAND("output.g", BU_ARG_VALUE_FILE, 1, 1,
	"Output BRL-CAD database", NULL),
    BU_ARG_OPERAND_NULL
};

static const struct bu_arg_schema tgf_g_schema = BU_ARG_SCHEMA(
    "tgf-g", "Convert INTAVAL target geometry to BRL-CAD",
    tgf_g_options, tgf_g_operands, BU_ARG_PARSE_OPTIONS_FIRST,
    BU_ARG_SCHEMA_META_HELP(NULL, NULL, NULL, NULL, NULL));

static const struct bu_arg_grammar tgf_g_grammar = {
    &tgf_g_schema, NULL, NULL
};


int main
(
    int   argc,
    char* argv[]
) {
    struct tgf_g_args args = {0};
    struct bu_vls msg = BU_VLS_INIT_ZERO;
    int ret = 0;
    int introspection;
    int operand_index;

    bu_setprogname(argv[0]);

    introspection = bu_arg_introspect(&msg, &tgf_g_grammar, argc - 1,
	(const char * const *)(argv + 1));
    if (introspection != 0) {
	bu_flog(introspection > 0 ? stdout : stderr, "%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return introspection > 0 ? 0 : 1;
    }
    operand_index = bu_arg_schema_parse_complete(&tgf_g_schema, &args,
	&msg, argc - 1, (const char **)(argv + 1));
    if (operand_index < 0) {
	bu_log("%s\n", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return 1;
    }
    bu_vls_free(&msg);
    if (args.help) {
	char *help = bu_arg_schema_help(&tgf_g_schema, "tgf-g");
	bu_flog(stdout, "%s", help ? help : "");
	bu_free(help, "tgf-g help");
	return 0;
    }
    argv += operand_index;

    FILE *in = fopen(argv[1], "r");
    if (!in) {
	bu_log("Error reading INTAVAL material file '%s'\n", argv[1]);
	return 1;
    }
    ret = readMaterials(in);
    fclose(in);
    if (ret != 0)
	return ret;

    std::ifstream is(argv[2]);
    if (!is.is_open()) {
	bu_log("Error reading INTAVAL geometry file '%s'\n", argv[2]);
	return 1;
    }
    struct rt_wdb *wdbp = wdb_fopen(argv[3]);
    if (!wdbp) {
	bu_log("Error creating BRL-CAD database '%s'\n", argv[3]);
	return 1;
    }

    conv(is, wdbp);
    createRegions(wdbp);
    db_close(wdbp->dbip);

    return 0;
}
