/*                        I T E M . C
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
/** @file libged/item.c
 *
 * The item command.
 *
 */

#include <stdlib.h>
#include "ged.h"

int
ged_item(struct ged *gedp, int argc, const char *argv[])
{
    int status = GED_OK;
    struct directory *dp;
    int ident, air, GIFTmater=0, los=0;
    int GIFTmater_set, los_set;
    struct rt_db_internal intern;
    struct rt_comb_internal *comb;
    static const char *usage = "region ident [air [material [los]]]";

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

    if (argc < 3 || 6 < argc) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    GED_DB_LOOKUP(gedp, dp, argv[1], LOOKUP_NOISY, GED_ERROR);
    GED_CHECK_COMB(gedp, dp, GED_ERROR);
    GED_CHECK_REGION(gedp, dp, GED_ERROR);

    air = ident = 0;
    GIFTmater_set = los_set = 0;
    ident = atoi(argv[2]);

    /*
     * If <air> is not included, it is assumed to be zero.
     * If, on the other hand, either of <GIFTmater> and <los>
     * is not included, it is left unchanged.
     */
    if (argc > 3) {
	air = atoi(argv[3]);
    }

    if (argc > 4) {
	GIFTmater = atoi(argv[4]);
	GIFTmater_set = 1;
    }

    if (argc > 5) {
	los = atoi(argv[5]);
	los_set = 1;
    }

    GED_DB_GET_INTERNAL(gedp, &intern, dp, (fastf_t *)NULL, &rt_uniresource, GED_ERROR);

    comb = (struct rt_comb_internal *)intern.idb_ptr;
    RT_CK_COMB(comb);
    comb->region_id = ident;
    comb->aircode = air;

    if (GIFTmater_set) {
	comb->GIFTmater = GIFTmater;
    }

    if (los_set) {
	comb->los = los;
    }

    GED_DB_PUT_INTERNAL(gedp, dp, &intern, &rt_uniresource, GED_ERROR);

    return status;
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
