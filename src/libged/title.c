/*                         T I T L E . C
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
/** @file libged/title.c
 *
 * The title command.
 *
 */

#include "common.h"

#include <string.h>
#include "bio.h"

#include "cmd.h"

#include "./ged_private.h"


int
ged_title(struct ged *gedp, int argc, const char *argv[])
{
    struct bu_vls title;

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* get title */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "%s", gedp->ged_wdbp->dbip->dbi_title);
	return GED_OK;
    }

    GED_CHECK_READ_ONLY(gedp, GED_ERROR);

    /* set title */
    bu_vls_init(&title);
    bu_vls_from_argv(&title, argc-1, (const char **)argv+1);

    if (db_update_ident(gedp->ged_wdbp->dbip, bu_vls_addr(&title), gedp->ged_wdbp->dbip->dbi_local2base) < 0) {
	bu_vls_free(&title);
	bu_vls_printf(gedp->ged_result_str, "Error: unable to change database title");
	return GED_ERROR;
    }

    bu_vls_free(&title);
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
