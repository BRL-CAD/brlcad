/*                         C P I . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2022 United States Government as represented by
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

#include "rt/geom.h"

#include "../ged_private.h"


int
ged_cpi_core(struct ged *gedp, int argc, const char *argv[])
{
    struct directory *proto;
    struct directory *dp;
    struct rt_db_internal internal;
    struct rt_tgc_internal *tgc_ip;
    int id;
    static const char *usage = "from to";

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_READ_ONLY(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_HELP;
    }

    if (argc != 3) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    if ((proto = db_lookup(gedp->dbip,  argv[1], LOOKUP_NOISY)) == RT_DIR_NULL) {
	bu_vls_printf(gedp->ged_result_str, "%s: %s does not exist!!\n", argv[0], argv[1]);
	return BRLCAD_ERROR;
    }

    if (db_lookup(gedp->dbip,  argv[2], LOOKUP_QUIET) != RT_DIR_NULL) {
	bu_vls_printf(gedp->ged_result_str, "%s: %s already exists!!\n", argv[0], argv[2]);
	return BRLCAD_ERROR;
    }

    if ((id = rt_db_get_internal(&internal, proto, gedp->dbip, (fastf_t *)NULL, &rt_uniresource)) < 0) {
	bu_vls_printf(gedp->ged_result_str, "%s: Database read error, aborting\n", argv[0]);
	return BRLCAD_ERROR;
    }
    /* make sure it is a TGC */
    if (id != ID_TGC) {
	bu_vls_printf(gedp->ged_result_str, "%s: %s is not a cylinder\n", argv[0], argv[1]);
	rt_db_free_internal(&internal);
	return BRLCAD_ERROR;
    }
    tgc_ip = (struct rt_tgc_internal *)internal.idb_ptr;

    /* translate to end of "original" cylinder */
    VADD2(tgc_ip->v, tgc_ip->v, tgc_ip->h);

    dp = db_diradd(gedp->dbip, argv[2], RT_DIR_PHONY_ADDR, 0, proto->d_flags, &proto->d_minor_type);
    if (dp == RT_DIR_NULL) {
	bu_vls_printf(gedp->ged_result_str, "%s: An error has occurred while adding a new object to the database.\n", argv[0]);
	return BRLCAD_ERROR;
    }

    if (rt_db_put_internal(dp, gedp->dbip, &internal, &rt_uniresource) < 0) {
	bu_vls_printf(gedp->ged_result_str, "%s: Database write error, aborting\n", argv[0]);
	return BRLCAD_ERROR;
    }

    return BRLCAD_OK;
}


#ifdef GED_PLUGIN
#include "../include/plugin.h"
struct ged_cmd_impl cpi_cmd_impl = {
    "cpi",
    ged_cpi_core,
    GED_CMD_DEFAULT
};

const struct ged_cmd cpi_cmd = { &cpi_cmd_impl };
const struct ged_cmd *cpi_cmds[] = { &cpi_cmd, NULL };

static const struct ged_plugin pinfo = { GED_API,  cpi_cmds, 1 };

COMPILER_DLLEXPORT const struct ged_plugin *ged_plugin_info()
{
    return &pinfo;
}
#endif /* GED_PLUGIN */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
