/*                           E N V . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2019 United States Government as represented by
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

#include "./env_vars.c"

static int
env_cmd(struct bu_vls *s_out, int argc, const char **argv)
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

    /* parse standard options */
    argc = bu_opt_parse(NULL, argc, argv, d);

    if (print_help) {
	bu_vls_printf(s_out, "%s      %s      %s", usage1, usage2, usage3);
	return 2;
    }

    if (argc) {
	if (BU_STR_EQUAL(argv[0], "get")) {
	    if (argc != 2) {
		bu_vls_printf(s_out, "Usage: %s", usage2);
		return -1;
	    }
	    if (!validate_env(argv[1])) {
		bu_vls_printf(s_out, "unknown environment variable: %s", argv[1]);
		return -1;
	    }

	    bu_vls_printf(s_out, "%s", getenv(argv[1]));
	    return 0;
	}

	if (BU_STR_EQUAL(argv[0], "set")) {
	    if (argc != 3) {
		bu_vls_printf(s_out, "Usage: %s", usage3);
		return -1;
	    }
	    if (!validate_env(argv[1])) {
		bu_vls_printf(s_out, "unknown environment variable: %s", argv[1]);
		return -1;
	    }

	    if (bu_setenv(argv[1], argv[2], 1)) {
		bu_vls_printf(s_out, "error when setting variable %s to %s", argv[1], argv[2]);
		return -1;
	    } else {
		bu_vls_printf(s_out, "%s", argv[2]);
		return 0;
	    }
	}
    }

    /* Not getting or setting, so we must be listing. */
    if (!argc) {
	list_env(s_out, "*", list_all);
    } else {
	int i = 0;
	for (i = 0; i < argc; i++) {
	    list_env(s_out, argv[i], list_all);
	}
    }
    return 0;
}

/**
 * Reports on and manipulates environment variables relevant to BRL-CAD.
 */
int
ged_env(struct ged *gedp, int argc, const char *argv[])
{
    int ret = GED_OK;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    ret = env_cmd(gedp->ged_result_str, argc, argv);

    if (ret == 2) {
	return GED_HELP;
    }

    ret = (!ret) ? GED_OK : GED_ERROR;

    return ret;
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
