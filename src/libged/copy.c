/*                         C O P Y . C
 * BRL-CAD
 *
 * Copyright (c) 2008 United States Government as represented by
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
/** @file copy.c
 *
 * The copy command.
 *
 */

#include "common.h"

#include <string.h>

#include "bio.h"
#include "cmd.h"
#include "ged_private.h"

int
ged_copy(struct ged *gedp, int argc, const char *argv[])
{
    register struct directory *proto;
    register struct directory *dp;
    struct bu_external external;
    static const char *usage = "from to";

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_READ_ONLY(gedp, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(&gedp->ged_result_str, 0);
    gedp->ged_result = GED_RESULT_NULL;
    gedp->ged_result_flags = 0;

    /* must be wanting help */
    if (argc == 1) {
	gedp->ged_result_flags |= GED_RESULT_FLAGS_HELP_BIT;
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_OK;
    }

    if (argc != 3) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }


    if ((proto = db_lookup(gedp->ged_wdbp->dbip,  argv[1], LOOKUP_NOISY)) == DIR_NULL)
	return BRLCAD_ERROR;

    if (db_lookup(gedp->ged_wdbp->dbip, argv[2], LOOKUP_QUIET) != DIR_NULL) {
	bu_vls_printf(&gedp->ged_result_str, "%s: already exists", argv[2]);
	return BRLCAD_ERROR;
    }

    if (db_get_external(&external, proto, gedp->ged_wdbp->dbip)) {
	bu_vls_printf(&gedp->ged_result_str, "Database read error, aborting\n");
	return BRLCAD_ERROR;
    }

    if ((dp=db_diradd(gedp->ged_wdbp->dbip, argv[2], -1, 0, proto->d_flags, (genptr_t)&proto->d_minor_type)) == DIR_NULL ) {
	bu_vls_printf(&gedp->ged_result_str, "An error has occured while adding a new object to the database.");
	return BRLCAD_ERROR;
    }

    if (db_put_external(&external, dp, gedp->ged_wdbp->dbip) < 0) {
	bu_free_external(&external);
	bu_vls_printf(&gedp->ged_result_str, "Database write error, aborting\n");
	return BRLCAD_ERROR;
    }
    bu_free_external(&external);

    return BRLCAD_OK;
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
