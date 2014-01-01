/*                         O P E N . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2014 United States Government as represented by
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
/** @file libged/open.c
 *
 * The open command.
 *
 */

#include "common.h"

#include <string.h>
#include "bio.h"

#include "cmd.h"

#include "./ged_private.h"


int
ged_reopen(struct ged *gedp, int argc, const char *argv[])
{
    struct db_i *new_dbip;
    static const char *usage = "[filename]";

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* get database filename */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "%s", gedp->ged_wdbp->dbip->dbi_filename);
	return GED_OK;
    }

    /* set database filename */
    if (argc == 2) {
	char *av[2];
	struct db_i *old_dbip = gedp->ged_wdbp->dbip;
	struct mater *old_materp = rt_material_head();
        struct mater *new_materp;

	rt_new_material_head(MATER_NULL);

	if ((new_dbip = _ged_open_dbip(argv[1], 0)) == DBI_NULL) {
	    /* Restore RT's material head */
	    rt_new_material_head(old_materp);

	    bu_vls_printf(gedp->ged_result_str, "ged_reopen: failed to open %s\n", argv[1]);
	    return GED_ERROR;
	}

	new_materp = rt_material_head();

	gedp->ged_wdbp->dbip = old_dbip;
        rt_new_material_head(old_materp);

	av[0] = "zap";
	av[1] = (char *)0;
	ged_zap(gedp, 1, (const char **)av);

	/* close current database */
	db_close(gedp->ged_wdbp->dbip);

	gedp->ged_wdbp->dbip = new_dbip;
	rt_new_material_head(new_materp);

	bu_vls_printf(gedp->ged_result_str, "%s", gedp->ged_wdbp->dbip->dbi_filename);
	return GED_OK;
    }

    bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
    return GED_ERROR;
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
