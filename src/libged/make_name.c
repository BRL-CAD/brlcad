/*                        M A K E _ N A M E . C
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
/** @file libged/make_name.c
 *
 * The make_name command.
 *
 */

#include <string.h>
#include "ged.h"

int
ged_make_name(struct ged *gedp, int argc, const char *argv[])
{
    struct bu_vls obj_name;
    char *cp, *tp;
    static int i = 0;
    int len;
    static const char *usage = "template | -s [num]";

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_READ_ONLY(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    switch (argc) {
	case 2:
	    if (!BU_STR_EQUAL(argv[1], "-s"))
		break;
	    else {
		i = 0;
		return GED_OK;
	    }
	case 3:
	    {
		int new_i;

		if ((BU_STR_EQUAL(argv[1], "-s"))
		    && (sscanf(argv[2], "%d", &new_i) == 1)) {
		    i = new_i;
		    return GED_OK;
		}
	    }
	default:
	    bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	    return GED_ERROR;
    }

    bu_vls_init(&obj_name);
    for (cp = (char *)argv[1], len = 0; *cp != '\0'; ++cp, ++len) {
	if (*cp == '@') {
	    if (*(cp + 1) == '@')
		++cp;
	    else
		break;
	}
	bu_vls_putc(&obj_name, *cp);
    }
    bu_vls_putc(&obj_name, '\0');
    tp = (*cp == '\0') ? "" : cp + 1;

    do {
	bu_vls_trunc(&obj_name, len);
	bu_vls_printf(&obj_name, "%d", i++);
	bu_vls_strcat(&obj_name, tp);
    }
    while (db_lookup(gedp->ged_wdbp->dbip, bu_vls_addr(&obj_name), LOOKUP_QUIET) != RT_DIR_NULL);

    bu_vls_printf(gedp->ged_result_str, "%s", bu_vls_addr(&obj_name));
    bu_vls_free(&obj_name);

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
