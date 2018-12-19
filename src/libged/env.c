/*                           E N V . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2018 United States Government as represented by
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
/** @file libged/env.c
 *
 * The env command.
 *
 * Interface for discovering and managing environment variables relevant
 * to the BRL-CAD libraries and programs.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <string.h>

#include "bu/env.h"
#include "bu/path.h"
#include "./ged_private.h"

static const char *
validate_env(const char *var)
{
#include "./env_vars.c"
    int i = 0;
    while (!BU_STR_EQUAL(env_vars[i], "NULL")) {
	if (BU_STR_EQUAL(env_vars[i], var)) return var;
	i++;
    }
    return NULL;
}

void
list_env(struct bu_vls *vl, const char *pattern, int list_all)
{
#include "./env_vars.c"
    int i = 0;
    while (!BU_STR_EQUAL(env_vars[i], "NULL")) {
	if (!bu_path_match(pattern, env_vars[i], 0)) {
	    char *evval = getenv(env_vars[i]);
	    if (!list_all && !evval) {
		i++;
		continue;
	    }
	    bu_vls_printf(vl, "%s=%s\n", env_vars[i], evval);
	}
	i++;
    }
}

/**
 * Reports on and manipulates environment variables relevant to BRL-CAD.
 */
int
ged_env(struct ged *gedp, int argc, const char *argv[])
{
    int print_help = 0;
    int list_all = 0;

    static const char *usage1 = "Usage: env [-hA] [pattern]\n";
    static const char *usage2 = " env get var_name\n";
    static const char *usage3 = " env set var_name var_val\n";

    struct bu_opt_desc d[3];
    BU_OPT(d[0], "h", "help", "", NULL, &print_help, "Print help and exit");
    BU_OPT(d[1], "A", "all",  "", NULL, &list_all,   "List all relevant variables, not just those currently set");
    BU_OPT_NULL(d[2]);

    /* skip command name argv[0] */
    argc-=(argc>0); argv+=(argc>0);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* parse standard options */
    argc = bu_opt_parse(NULL, argc, argv, d);

    if (print_help) {
	bu_vls_printf(gedp->ged_result_str, "%s      %s      %s", usage1, usage2, usage3);
	return GED_HELP;
    }

    if (argc) {
	if (BU_STR_EQUAL(argv[0], "get")) {
	    if (argc != 2) {
		bu_vls_printf(gedp->ged_result_str, "Usage: %s", usage2);
		return GED_ERROR;
	    }
	    if (!validate_env(argv[1])) {
		bu_vls_printf(gedp->ged_result_str, "unknown environment variable: %s", argv[1]);
		return GED_ERROR;
	    }

	    bu_vls_printf(gedp->ged_result_str, "%s", getenv(argv[1]));
	    return GED_OK;
	}

	if (BU_STR_EQUAL(argv[0], "set")) {
	    if (argc != 3) {
		bu_vls_printf(gedp->ged_result_str, "Usage: %s", usage3);
		return GED_ERROR;
	    }
	    if (!validate_env(argv[1])) {
		bu_vls_printf(gedp->ged_result_str, "unknown environment variable: %s", argv[1]);
		return GED_ERROR;
	    }

	    if (bu_setenv(argv[1], argv[2], 1)) {
		bu_vls_printf(gedp->ged_result_str, "error when setting variable %s to %s", argv[1], argv[2]);
		return GED_ERROR;
	    } else {
		bu_vls_printf(gedp->ged_result_str, "%s", argv[2]);
		return GED_OK;
	    }
	}
    }

    /* Not getting or setting, so we must be listing. */
    if (!argc) {
	list_env(gedp->ged_result_str, "*", list_all);
    } else {
	int i = 0;
	for (i = 0; i < argc; i++) {
	    list_env(gedp->ged_result_str, argv[i], list_all);
	}
    }
    return GED_OK;
}


/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
