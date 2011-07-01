/*                         T O L . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2011 United States Government as represented by
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
#include "bio.h"

#include "cmd.h"

#include "./ged_private.h"


int
ged_tol(struct ged *gedp, int argc, const char *argv[])
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
	bu_vls_printf(gedp->ged_result_str, "Tesselation tolerances:\n");

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

	    sec = gedp->ged_wdbp->wdb_ttol.norm * bn_radtodeg;
	    deg = (int)(sec);
	    sec = (sec - (double)deg) * 60;
	    min = (int)(sec);
	    sec = (sec - (double)min) * 60;

	    bu_vls_printf(gedp->ged_result_str, "\tnorm %g degrees (%d deg %d min %g sec)\n",
			  gedp->ged_wdbp->wdb_ttol.norm * bn_radtodeg, deg, min, sec);
	} else {
	    bu_vls_printf(gedp->ged_result_str, "\tnorm None\n");
	}

	bu_vls_printf(gedp->ged_result_str, "Calculational tolerances:\n");
	bu_vls_printf(gedp->ged_result_str,
		      "\tdistance = %g mm\n\tperpendicularity = %g (cosine of %g degrees)",
		      gedp->ged_wdbp->wdb_tol.dist, gedp->ged_wdbp->wdb_tol.perp,
		      acos(gedp->ged_wdbp->wdb_tol.perp)*bn_radtodeg);

	return GED_OK;
    }

    /* get the specified tolerance */
    if (argc == 2) {
	int status = GED_OK;

	switch (argv[1][0]) {
	    case 'a':
		if (gedp->ged_wdbp->wdb_ttol.abs > 0.0)
		    bu_vls_printf(gedp->ged_result_str, "%g", gedp->ged_wdbp->wdb_ttol.abs);
		else
		    bu_vls_printf(gedp->ged_result_str, "None");
		break;
	    case 'r':
		if (gedp->ged_wdbp->wdb_ttol.rel > 0.0)
		    bu_vls_printf(gedp->ged_result_str, "%g", gedp->ged_wdbp->wdb_ttol.rel);
		else
		    bu_vls_printf(gedp->ged_result_str, "None");
		break;
	    case 'n':
		if (gedp->ged_wdbp->wdb_ttol.norm > 0.0)
		    bu_vls_printf(gedp->ged_result_str, "%g", gedp->ged_wdbp->wdb_ttol.norm);
		else
		    bu_vls_printf(gedp->ged_result_str, "None");
		break;
	    case 'd':
		bu_vls_printf(gedp->ged_result_str, "%g", gedp->ged_wdbp->wdb_tol.dist);
		break;
	    case 'p':
		bu_vls_printf(gedp->ged_result_str, "%g", gedp->ged_wdbp->wdb_tol.perp);
		break;
	    default:
		bu_vls_printf(gedp->ged_result_str, "unrecognized tolerance type - %s", argv[1]);
		status = GED_ERROR;
		break;
	}

	return status;
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

	switch (argv[0][0]) {
	    case 'a':
		/* Absolute tol */
		if (f < gedp->ged_wdbp->wdb_tol.dist) {
		    bu_vls_printf(gedp->ged_result_str,
				  "absolute tolerance cannot be less than distance tolerance, clamped to %f\n", gedp->ged_wdbp->wdb_tol.dist);
		}
		gedp->ged_wdbp->wdb_ttol.abs = f;
		break;
	    case 'r':
		if (f >= 1.0) {
		    bu_vls_printf(gedp->ged_result_str,
				  "relative tolerance must be between 0 and 1, not changed\n");
		    return GED_ERROR;
		}
		/* Note that a value of 0.0 will disable relative tolerance */
		gedp->ged_wdbp->wdb_ttol.rel = f;
		break;
	    case 'n':
		/* Normal tolerance, in degrees */
		if (f > 90.0) {
		    bu_vls_printf(gedp->ged_result_str,
				  "Normal tolerance must be less than 90.0 degrees\n");
		    return GED_ERROR;
		}
		/* Note that a value of 0.0 or 360.0 will disable this tol */
		gedp->ged_wdbp->wdb_ttol.norm = f * bn_degtorad;
		break;
	    case 'd':
		/* Calculational distance tolerance */
		gedp->ged_wdbp->wdb_tol.dist = f;
		gedp->ged_wdbp->wdb_tol.dist_sq = gedp->ged_wdbp->wdb_tol.dist * gedp->ged_wdbp->wdb_tol.dist;
		break;
	    case 'p':
		/* Calculational perpendicularity tolerance */
		if (f > 1.0) {
		    bu_vls_printf(gedp->ged_result_str,
				  "Calculational perpendicular tolerance must be from 0 to 1\n");
		    return GED_ERROR;
		}
		gedp->ged_wdbp->wdb_tol.perp = f;
		gedp->ged_wdbp->wdb_tol.para = 1.0 - f;
		break;
	    default:
		bu_vls_printf(gedp->ged_result_str, "unrecognized tolerance type - %s", argv[0]);

		return GED_ERROR;
	}

	argc-=2;
	argv+=2;
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
