/*                         I N S T A N C E . C
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
/** @file libged/instance.c
 *
 * The instance command.
 *
 */

#include "common.h"

#include <string.h>
#include "bio.h"

#include "cmd.h"
#include "wdb.h"

#include "./ged_private.h"


int
ged_instance(struct ged *gedp, int argc, const char *argv[])
{
    struct directory *dp;
    char oper;
    static const char *usage = "obj comb [op]";

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

    if (argc < 3 || 4 < argc) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    if ((dp = db_lookup(gedp->ged_wdbp->dbip,  argv[1], LOOKUP_NOISY)) == RT_DIR_NULL)
	return GED_ERROR;

    oper = WMOP_UNION;
    if (argc == 4)
	oper = argv[3][0];

    if (oper != WMOP_UNION &&
	oper != WMOP_SUBTRACT &&
	oper != WMOP_INTERSECT) {
	bu_vls_printf(gedp->ged_result_str, "bad operation: %c\n", oper);
	return GED_ERROR;
    }

    if (_ged_combadd(gedp, dp, (char *)argv[2], 0, oper, 0, 0) == RT_DIR_NULL)
	return GED_ERROR;

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
