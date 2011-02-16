/*                        S E A R C H . C
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
/** @file search.c
 *
 * Brief description
 *
 */

#include "ged.h"

int
ged_search(struct ged *gedp, int argc, const char *argv_orig[])
{
    struct rt_search_dbinfo dbinfo;
    struct rt_search_results results;
    if (argc < 2) {
	bu_vls_printf(&gedp->ged_result_str, " [path] [expressions...]\n");
    } else {
	dbinfo.dbip = gedp->ged_wdbp->dbip;
	dbinfo.wdbp = gedp->ged_wdbp;

	/* initialize result */
	bu_vls_trunc(&gedp->ged_result_str, 0);
	bu_vls_init(&results.result_str);

	/* call rt_search */
	rt_search(&dbinfo, &results, argc, argv_orig);
	bu_vls_printf(&gedp->ged_result_str,  "%s", bu_vls_addr(&results.result_str));
	bu_vls_free(&results.result_str);
	return TCL_OK;
    }
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
