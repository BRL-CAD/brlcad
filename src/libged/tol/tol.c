/*                         T O L . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2021 United States Government as represented by
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
/** @file libged/tol.c
 *
 * The tol command.
 *
 */

#include "common.h"

#include <string.h>

#include "bu/cmd.h"

#include "../ged_private.h"


int
ged_tol_core(struct ged *gedp, int argc, const char *argv[])
{
    double f;
    static const char *usage = "([abs|rel|norm|dist|perp] [tolerance]) ...";

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* print all tolerance settings */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Current tolerance settings are:\n");
	bu_vls_printf(gedp->ged_result_str, "Tessellation tolerances:\n");

	if (gedp->ged_wdbp->wdb_ttol.abs > 0.0) {
	    bu_vls_printf(gedp->ged_result_str, "\tabs %g mm\n", gedp->ged_wdbp->wdb_ttol.abs);
	} else {
	    bu_vls_printf(gedp->ged_result_str, "\tabs None\n");
	}

	if (gedp->ged_wdbp->wdb_ttol.rel > 0.0) {
	    bu_vls_printf(gedp->ged_result_str, "\trel %g (%g%%)\n",
			  gedp->ged_wdbp->wdb_ttol.rel, gedp->ged_wdbp->wdb_ttol.rel * 100.0);
	} else {
	    bu_vls_printf(gedp->ged_result_str, "\trel None\n");
	}

	if (gedp->ged_wdbp->wdb_ttol.norm > 0.0) {
	    int deg, min;
	    double sec;

	    sec = gedp->ged_wdbp->wdb_ttol.norm * RAD2DEG;
	    deg = (int)(sec);
	    sec = (sec - (double)deg) * 60;
	    min = (int)(sec);
	    sec = (sec - (double)min) * 60;

	    bu_vls_printf(gedp->ged_result_str, "\tnorm %g degrees (%d deg %d min %g sec)\n",
			  gedp->ged_wdbp->wdb_ttol.norm * RAD2DEG, deg, min, sec);
	} else {
	    bu_vls_printf(gedp->ged_result_str, "\tnorm None\n");
	}

	bu_vls_printf(gedp->ged_result_str, "Calculational tolerances:\n");
	bu_vls_printf(gedp->ged_result_str,
		      "\tdistance = %g mm\n\tperpendicularity = %g (cosine of %g degrees)\n",
		      gedp->ged_wdbp->wdb_tol.dist, gedp->ged_wdbp->wdb_tol.perp,
		      acos(gedp->ged_wdbp->wdb_tol.perp)*RAD2DEG);

	bu_vls_printf(gedp->ged_result_str, "BRep specific tessellation tolerances:\n");
	if (gedp->ged_wdbp->wdb_ttol.absmax > 0.0) {
	    bu_vls_printf(gedp->ged_result_str, "\tabsmax %g\n", gedp->ged_wdbp->wdb_ttol.absmax);
	} else {
	    bu_vls_printf(gedp->ged_result_str, "\tabsmax None\n");
	}

	if (gedp->ged_wdbp->wdb_ttol.absmin > 0.0) {
	    bu_vls_printf(gedp->ged_result_str, "\tabsmin %g\n", gedp->ged_wdbp->wdb_ttol.absmin);
	} else {
	    bu_vls_printf(gedp->ged_result_str, "\tabsmin None\n");
	}

	if (gedp->ged_wdbp->wdb_ttol.relmax > 0.0) {
	    bu_vls_printf(gedp->ged_result_str, "\trelmax %g\n", gedp->ged_wdbp->wdb_ttol.relmax);
	} else {
	    bu_vls_printf(gedp->ged_result_str, "\trelmax None\n");
	}

	if (gedp->ged_wdbp->wdb_ttol.relmin > 0.0) {
	    bu_vls_printf(gedp->ged_result_str, "\trelmin %g\n", gedp->ged_wdbp->wdb_ttol.relmin);
	} else {
	    bu_vls_printf(gedp->ged_result_str, "\trelmin None\n");
	}

	if (gedp->ged_wdbp->wdb_ttol.rel_lmax > 0.0) {
	    bu_vls_printf(gedp->ged_result_str, "\tlmax %g\n", gedp->ged_wdbp->wdb_ttol.rel_lmax);
	} else {
	    bu_vls_printf(gedp->ged_result_str, "\tlmax None\n");
	}

	if (gedp->ged_wdbp->wdb_ttol.rel_lmin > 0.0) {
	    bu_vls_printf(gedp->ged_result_str, "\tlmin %g\n", gedp->ged_wdbp->wdb_ttol.rel_lmin);
	} else {
	    bu_vls_printf(gedp->ged_result_str, "\tlmin None\n");
	}


	return GED_OK;
    }

    /* get the specified tolerance */
    if (argc == 2) {

	if (BU_STR_EQUAL(argv[1], "abs")) {
	    if (gedp->ged_wdbp->wdb_ttol.abs > 0.0)
		bu_vls_printf(gedp->ged_result_str, "%g", gedp->ged_wdbp->wdb_ttol.abs);
	    else {
		bu_vls_printf(gedp->ged_result_str, "None");
	    }
	    return GED_OK;
	}

	if (BU_STR_EQUAL(argv[1], "rel")) {
	    if (gedp->ged_wdbp->wdb_ttol.rel > 0.0) {
		bu_vls_printf(gedp->ged_result_str, "%g", gedp->ged_wdbp->wdb_ttol.rel);
	    } else {
		bu_vls_printf(gedp->ged_result_str, "None");
	    }
	    return GED_OK;
	}

	if (BU_STR_EQUAL(argv[1], "norm")) {
	    if (gedp->ged_wdbp->wdb_ttol.norm > 0.0) {
		bu_vls_printf(gedp->ged_result_str, "%g", gedp->ged_wdbp->wdb_ttol.norm);
	    } else {
		bu_vls_printf(gedp->ged_result_str, "None");
	    }
	    return GED_OK;
	}

	if (BU_STR_EQUAL(argv[1], "absmax")) {
	    if (gedp->ged_wdbp->wdb_ttol.absmax > 0.0) {
		bu_vls_printf(gedp->ged_result_str, "\tabsmax %g\n", gedp->ged_wdbp->wdb_ttol.absmax);
	    } else {
		bu_vls_printf(gedp->ged_result_str, "\tabsmax None\n");
	    }
	    return GED_OK;
	}

	if (BU_STR_EQUAL(argv[1], "absmin")) {
	    if (gedp->ged_wdbp->wdb_ttol.absmin > 0.0) {
		bu_vls_printf(gedp->ged_result_str, "\tabsmin %g\n", gedp->ged_wdbp->wdb_ttol.absmin);
	    } else {
		bu_vls_printf(gedp->ged_result_str, "\tabsmin None\n");
	    }
	    return GED_OK;
	}


	if (BU_STR_EQUAL(argv[1], "relmax")) {
	    if (gedp->ged_wdbp->wdb_ttol.relmax > 0.0) {
		bu_vls_printf(gedp->ged_result_str, "\trelmax %g\n", gedp->ged_wdbp->wdb_ttol.relmax);
	    } else {
		bu_vls_printf(gedp->ged_result_str, "\trelmax None\n");
	    }
	    return GED_OK;
	}


	if (BU_STR_EQUAL(argv[1], "relmin")) {
	    if (gedp->ged_wdbp->wdb_ttol.relmin > 0.0) {
		bu_vls_printf(gedp->ged_result_str, "\trelmin %g\n", gedp->ged_wdbp->wdb_ttol.relmin);
	    } else {
		bu_vls_printf(gedp->ged_result_str, "\trelmin None\n");
	    }
	    return GED_OK;
	}


	if (BU_STR_EQUAL(argv[1], "lmax")) {
	    if (gedp->ged_wdbp->wdb_ttol.rel_lmax > 0.0) {
		bu_vls_printf(gedp->ged_result_str, "\tlmax %g\n", gedp->ged_wdbp->wdb_ttol.rel_lmax);
	    } else {
		bu_vls_printf(gedp->ged_result_str, "\tlmax None\n");
	    }
	    return GED_OK;
	}


	if (BU_STR_EQUAL(argv[1], "lmin")) {
	    if (gedp->ged_wdbp->wdb_ttol.rel_lmin > 0.0) {
		bu_vls_printf(gedp->ged_result_str, "\tlmin %g\n", gedp->ged_wdbp->wdb_ttol.rel_lmin);
	    } else {
		bu_vls_printf(gedp->ged_result_str, "\tlmin None\n");
	    }
	    return GED_OK;
	}

	if (BU_STR_EQUAL(argv[1], "dist")) {
	    bu_vls_printf(gedp->ged_result_str, "%g", gedp->ged_wdbp->wdb_tol.dist);
	    return GED_OK;
	}

	if (BU_STR_EQUAL(argv[1], "perp")) {
	    bu_vls_printf(gedp->ged_result_str, "%g", gedp->ged_wdbp->wdb_tol.perp);
	    return GED_OK;
	}

	bu_vls_printf(gedp->ged_result_str, "unrecognized tolerance type - %s", argv[1]);
	return GED_ERROR;

    }

    if ((argc-1)%2 != 0) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    /* skip the command name */
    argc--;
    argv++;

    /* iterate over the pairs of tolerance values */
    while (argc > 0) {
	int valid_tol = 0;

	/* set the specified tolerance(s) */
	if (sscanf(argv[1], "%lf", &f) != 1) {
	    bu_vls_printf(gedp->ged_result_str, "bad tolerance - %s", argv[1]);
	    return GED_ERROR;
	}

	/* clamp negative to zero */
	if (f < 0.0) {
	    bu_vls_printf(gedp->ged_result_str, "negative tolerance clamped to 0.0\n");
	    f = 0.0;
	}

	if (BU_STR_EQUAL(argv[0], "abs")) {
	    /* Absolute tol */
	    if (f < gedp->ged_wdbp->wdb_tol.dist) {
		bu_vls_printf(gedp->ged_result_str,
			"absolute tolerance cannot be less than distance tolerance, clamped to %f\n", gedp->ged_wdbp->wdb_tol.dist);
	    }
	    gedp->ged_wdbp->wdb_ttol.abs = f;
	    valid_tol = 1;
	}
	if (BU_STR_EQUAL(argv[0], "rel")) {
	    if (f >= 1.0) {
		bu_vls_printf(gedp->ged_result_str,
			"relative tolerance must be between 0 and 1, not changed\n");
		return GED_ERROR;
	    }
	    /* Note that a value of 0.0 will disable relative tolerance */
	    gedp->ged_wdbp->wdb_ttol.rel = f;
	    valid_tol = 1;
	}
	if (BU_STR_EQUAL(argv[0], "norm")) {
	    /* Normal tolerance, in degrees */
	    if (f > 90.0) {
		bu_vls_printf(gedp->ged_result_str,
			"Normal tolerance must be less than 90.0 degrees\n");
		return GED_ERROR;
	    }
	    /* Note that a value of 0.0 or 360.0 will disable this tol */
	    gedp->ged_wdbp->wdb_ttol.norm = f * DEG2RAD;
	    valid_tol = 1;
	}


	if (BU_STR_EQUAL(argv[0], "absmax")) {
	    /* Absolute tol */
	    if (f < gedp->ged_wdbp->wdb_tol.dist) {
		bu_vls_printf(gedp->ged_result_str,
			"absolute tolerance cannot be less than distance tolerance, clamped to %f\n", gedp->ged_wdbp->wdb_tol.dist);
	    }
	    gedp->ged_wdbp->wdb_ttol.absmax = f;
	    valid_tol = 1;
	}

	if (BU_STR_EQUAL(argv[0], "absmin")) {
	    /* Absolute tol */
	    if (f < gedp->ged_wdbp->wdb_tol.dist) {
		bu_vls_printf(gedp->ged_result_str,
			"absolute tolerance cannot be less than distance tolerance, clamped to %f\n", gedp->ged_wdbp->wdb_tol.dist);
	    }
	    gedp->ged_wdbp->wdb_ttol.absmin = f;
	    valid_tol = 1;
	}

	if (BU_STR_EQUAL(argv[0], "relmax")) {
	    if (f >= 1.0) {
		bu_vls_printf(gedp->ged_result_str,
			"relative tolerance must be between 0 and 1, not changed\n");
		return GED_ERROR;
	    }
	    /* Note that a value of 0.0 will disable relative tolerance */
	    gedp->ged_wdbp->wdb_ttol.relmax = f;
	    valid_tol = 1;
	}

	if (BU_STR_EQUAL(argv[0], "relmin")) {
	    if (f >= 1.0) {
		bu_vls_printf(gedp->ged_result_str,
			"relative tolerance must be between 0 and 1, not changed\n");
		return GED_ERROR;
	    }
	    /* Note that a value of 0.0 will disable relative tolerance */
	    gedp->ged_wdbp->wdb_ttol.relmin = f;
	    valid_tol = 1;
	}

	if (BU_STR_EQUAL(argv[0], "lmax")) {
	    if (f >= 1.0) {
		bu_vls_printf(gedp->ged_result_str,
			"relative tolerance must be between 0 and 1, not changed\n");
		return GED_ERROR;
	    }
	    /* Note that a value of 0.0 will disable relative tolerance */
	    gedp->ged_wdbp->wdb_ttol.rel_lmax = f;
	    valid_tol = 1;
	}

	if (BU_STR_EQUAL(argv[0], "lmin")) {
	    if (f >= 1.0) {
		bu_vls_printf(gedp->ged_result_str,
			"relative tolerance must be between 0 and 1, not changed\n");
		return GED_ERROR;
	    }
	    /* Note that a value of 0.0 will disable relative tolerance */
	    gedp->ged_wdbp->wdb_ttol.rel_lmin = f;
	    valid_tol = 1;
	}

	if (BU_STR_EQUAL(argv[0], "dist")) {
	    /* Calculational distance tolerance */
	    gedp->ged_wdbp->wdb_tol.dist = f;
	    gedp->ged_wdbp->wdb_tol.dist_sq = gedp->ged_wdbp->wdb_tol.dist * gedp->ged_wdbp->wdb_tol.dist;
	    valid_tol = 1;
	}
	if (BU_STR_EQUAL(argv[0], "perp")) {
	    /* Calculational perpendicularity tolerance */
	    if (f > 1.0) {
		bu_vls_printf(gedp->ged_result_str,
			"Calculational perpendicular tolerance must be from 0 to 1\n");
		return GED_ERROR;
	    }
	    gedp->ged_wdbp->wdb_tol.perp = f;
	    gedp->ged_wdbp->wdb_tol.para = 1.0 - f;
	    valid_tol = 1;
	}

	if (!valid_tol) {
	    bu_vls_printf(gedp->ged_result_str, "unrecognized tolerance type - %s", argv[0]);
	    return GED_ERROR;
	}

	argc-=2;
	argv+=2;
    }

    return GED_OK;
}


#ifdef GED_PLUGIN
#include "../include/plugin.h"
struct ged_cmd_impl tol_cmd_impl = {
    "tol",
    ged_tol_core,
    GED_CMD_DEFAULT
};

const struct ged_cmd tol_cmd = { &tol_cmd_impl };
const struct ged_cmd *tol_cmds[] = { &tol_cmd, NULL };

static const struct ged_plugin pinfo = { GED_API,  tol_cmds, 1 };

COMPILER_DLLEXPORT const struct ged_plugin *ged_plugin_info()
{
    return &pinfo;
}
#endif /* GED_PLUGIN */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
