/*                      A N N O T A T E . C
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

#include <string.h>

#include "ged.h"


void
annotate_help(struct bu_vls *result, const char *cmd)
{
    static const char *usage = "[object(s)] [-n name] [-p x y z] [-t type] [-m message]";

    bu_vls_printf(result, "Usage: %s %s", cmd, usage);
}


int
ged_annotate(struct ged *gedp, int argc, const char *argv[])
{
    char **object_argv;
    const char *argv0 = argv[0];
    struct bu_vls objects;
    int object_count = 0;
    int i;

    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(&gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	annotate_help(&gedp->ged_result_str, argv0);
	return GED_HELP;
    }

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_READ_ONLY(gedp, GED_ERROR);

    bu_vls_init(&objects);

    /* stash objects, quoting them if they include spaces */
    for (i = 1; i < argc; i++) {
	bu_log("DEBUG: argv[%d] == %s\n", i, argv[i]);

	if (argv[i][0] != '-') {
	    object_count++;
	    if (i != 1)
		bu_vls_putc(&objects, ' ');
	    bu_vls_from_argv(&objects, 1, &(argv[i]));
	} else {
	    break;
	}
    }
    bu_log("DEBUG: objects: [%s]\n", bu_vls_addr(&objects));

    object_argv = (char **)bu_calloc(object_count+1, sizeof(char *), "alloc object argv");
    bu_argv_from_string(object_argv, object_count, bu_vls_addr(&objects));
    for (i = 0; i < object_count; i++) {
	bu_log("DEBUG: stashed [%s]\n", object_argv[i]);
    }

    bu_vls_free(&objects);
    bu_free_argv(object_count+1, object_argv);

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
