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
/** @file libged/annotate.c
 *
 * The annotate command.
 *
 * Examples:
 *
 *   annotate
 *            [text {string}]
 *            [as label|leader|angular|radial|dimension|table|note|box|axes|plane [named {name}]]
 *            [on|for {object1} [and {object2}] [and {...}]]
 *            [to|thru|at {point|plane} [offset {distance|vector}]]
 *            [align auto|model|view]
 *            [position auto|fixed|absolute|relative]
 *            [visible always|never|rendering|wireframe]
 *
 *            [help]
 *            [list {name}]
 *            [get {key} from {name}]
 *            [set {key=value} on {name}]
 *
 *   annotate help
 *   annotate text "Hello, World!"
 *   annotate for all.g
 *   annotate as label named my.note for all.g text "This is a tank."
 *   annotate as box text "This geometry is unclassified."
 *
 * DESIGN OPTIONS TO CONSIDER:
 *
 * Types: text (see Text), leader, angular, radial, aligned, ordinate, linear
 *
 * Extended types: note, label, table, dimension, tolerance, box (see Box), axes, plane
 *
 * Orientation: auto, horizontal, vertical, above, below, inline
 *
 * Align: auto, model, view
 *
 * Text: fontname, fontsize, fontstyle (regular, italic, bold), linespacing, justification
 *   Justification:
 *     undefined
 *     left
 *     center
 *     right
 *     bottom
 *     middle
 *     top
 *     bottomleft
 *     bottomcenter
 *     bottomright
 *     middleleft
 *     middlecenter
 *     middleright
 *     topleft
 *     topcenter
 *     topright
 *
 * Decoration: color, leader line, box (see Box) around target, box around annotation
 *
 * Box: empty, hatch, gradient, solid
 *
 * Placement: position (auto/fixed/relative/absolute), scale, orientation/rotation/twist, head/tail
 *   auto is automatic static placement
 *   fixed is 2d coordinate relative to view
 *   absolute is 2d coordinate relative to world center or global 3d position
 *   relative is 2d coordinate relative to view center or 3d offset distance from target center
 *
 * Leader: linelength, linewidth, type (no head, arrow head, round head, square head)
 *
 * Visibility: auto, wireframe, render, both
 *
 * linearformat: "%.2f"
 * linearunits: mm, m, in, ft, etc
 *
 * angularformat: "%.2f"
 * angularunits: degrees, radians
 *
 * struct parameters:
 * type
 * plane
 * point list
 * text?
 * color?
 * visibility?
 *
 */

#include "common.h"

#include <string.h>

#include "ged.h"


void
annotate_help(struct bu_vls *result, const char *cmd)
{
    static const char *usage = "[object(s)] [-n name] [-p x y z]";

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
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	annotate_help(gedp->ged_result_str, argv0);
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
