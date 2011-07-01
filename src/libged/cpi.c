/*                         C P I . C
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
/** @file libged/cpi.c
 *
 * The cpi command.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include "bio.h"

#include "rtgeom.h"

#include "./ged_private.h"


int
ged_cpi(struct ged *gedp, int argc, const char *argv[])
{
    struct directory *proto;
    struct directory *dp;
    struct rt_db_internal internal;
    struct rt_tgc_internal *tgc_ip;
    int id;
    static const char *usage = "from to";

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

    if (argc != 3) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    if ((proto = db_lookup(gedp->ged_wdbp->dbip,  argv[1], LOOKUP_NOISY)) == RT_DIR_NULL) {
	bu_vls_printf(gedp->ged_result_str, "%s: %s does not exist!!\n", argv[0], argv[1]);
	return GED_ERROR;
    }

    if (db_lookup(gedp->ged_wdbp->dbip,  argv[2], LOOKUP_QUIET) != RT_DIR_NULL) {
	bu_vls_printf(gedp->ged_result_str, "%s: %s already exists!!\n", argv[0], argv[2]);
	return GED_ERROR;
    }

    if ((id = rt_db_get_internal(&internal, proto, gedp->ged_wdbp->dbip, (fastf_t *)NULL, &rt_uniresource)) < 0) {
	bu_vls_printf(gedp->ged_result_str, "%s: Database read error, aborting\n", argv[0]);
	return GED_ERROR;
    }
    /* make sure it is a TGC */
    if (id != ID_TGC) {
	bu_vls_printf(gedp->ged_result_str, "%s: %s is not a cylinder\n", argv[0], argv[1]);
	rt_db_free_internal(&internal);
	return GED_ERROR;
    }
    tgc_ip = (struct rt_tgc_internal *)internal.idb_ptr;

    /* translate to end of "original" cylinder */
    VADD2(tgc_ip->v, tgc_ip->v, tgc_ip->h);

    dp = db_diradd(gedp->ged_wdbp->dbip, argv[2], RT_DIR_PHONY_ADDR, 0, proto->d_flags, &proto->d_minor_type);
    if (dp == RT_DIR_NULL) {
	bu_vls_printf(gedp->ged_result_str, "%s: An error has occured while adding a new object to the database.\n", argv[0]);
	return GED_ERROR;
    }

    if (rt_db_put_internal(dp, gedp->ged_wdbp->dbip, &internal, &rt_uniresource) < 0) {
	bu_vls_printf(gedp->ged_result_str, "%s: Database write error, aborting\n", argv[0]);
	return GED_ERROR;
    }

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
