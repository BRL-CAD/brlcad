/*                      A N N O T A T E . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2009 United States Government as represented by
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
/** @file annotate.c
 *
 * The annotate command.
 *
 * Examples:
 *   annotate all.g -n my.note -p 0 0 0 -m "This is a tank."
 *   annotate -p 10 0 0 -m "This geometry is unclassified."
 *   annotate sph.r -t leader -p 10 10 10
 *
 * Design options to consider:
 *
 * Types: text, leader, angular, radial, aligned, ordinate, linear
 * Modes: normal, horizontal, vertical, above, below, inline
 * Align: auto, model, view
 *
 * scale, orientation
 * fontname, fontsize
 * arrowlength, arrowwidth, arrowtype
 *
 * linearformat: "%.2f"
 * linearunits: mm, m, in, ft, etc
 *
 * angularformat: "%.2f"
 * angularunits: degrees, radians
 *
 */

#include "common.h"

#include <stdlib.h>


void
ged_annotate_help(struct bu_vls *result, const char *cmd)
{
    static const char *usage = "name [create|delete|adjust] name ";

    bu_vls_printf(result, "Usage: %s %s", cmd, usage);
}


int
ged_annotate(struct ged *gedp, int argc, const char *argv[])
{
    const char *argv0 = argv[0];

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(&gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	annotate_help(&gedp->ged_result_str, argv0);
	return GED_HELP;
    }

    /* do something */

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
