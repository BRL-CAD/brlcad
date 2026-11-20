/*                          G _ Q A . C
 * BRL-CAD
 *
 * Copyright (c) 2005-2026 United States Government as represented by
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
/** @file g_qa.c
 *
 * Perform quantitative analysis checks on geometry.
 *
 * XXX need to look at gap computation
 *
 * plot the points where overlaps start/stop
 *
 * Designed to be a framework for 3d sampling of the geometry volume.
 */

#include "common.h"

#include "bu/arg.h"
#include "bu/app.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "bu/vls.h"
#include "ged.h"

#define GQA_SCHEMA_STANDALONE
#include "../libged/gqa/gqa_schema.h"

static const struct bu_arg_grammar gqa_grammar = {
    &gqa_schema, NULL, NULL
};


static int
gqa_help(FILE *stream, int status)
{
    char *help = bu_arg_schema_help(&gqa_schema, "gqa");

    if (help) {
	bu_flog(stream, "%s", help);
	bu_free(help, "gqa help");
    }
    return status;
}

int
main(int argc, char *argv[])
{
    int i, j;
    int db_index;
    int introspection;
    int operand_index;
    int ret;
    const char **av;
    struct ged *gedp;
    struct gqa_args args = {0};
    struct bu_vls msg = BU_VLS_INIT_ZERO;

    bu_setprogname(argv[0]);

    introspection = bu_arg_introspect(&msg, &gqa_grammar, argc - 1,
	(const char * const *)(argv + 1));
    if (introspection != 0) {
	bu_flog(introspection > 0 ? stdout : stderr, "%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return introspection > 0 ? 0 : 1;
    }

    args.views = INT_MIN;
    operand_index = bu_arg_schema_parse_complete(&gqa_schema, &args, &msg,
	argc - 1, (const char **)(argv + 1));
    if (operand_index < 0) {
	bu_flog(stderr, "%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return 1;
    }
    bu_vls_free(&msg);
    if (args.help)
	return gqa_help(stdout, 0);

    av = (const char **)bu_calloc(argc, sizeof(char *), "av");

    db_index = operand_index + 1;
    for (i = j = 0; i < argc; ++i) {
	if (i == db_index)
	    continue;

	av[j] = argv[i];
	++j;
    }
    av[0] = "gqa";
    av[j] = (char *)0;

    if ((gedp = ged_open("db", argv[db_index], 1)) == GED_NULL) {
	bu_free((void *)av, "av");
	bu_flog(stderr, "%s: unable to open database %s\n", argv[0],
	    argv[db_index]);
	return 1;
    }

    ret = ged_exec_gqa(gedp, j, av);
    if (bu_vls_strlen(gedp->ged_result_str) > 0)
	bu_log("%s", bu_vls_addr(gedp->ged_result_str));
    ged_close(gedp);

    bu_free((void *)av, "av");

    return ret;
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
