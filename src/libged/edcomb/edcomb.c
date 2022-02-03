/*                        E D C O M B . C
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
/** @file libged/edcomb.c
 *
 * The edcomb command.
 *
 */

#include "ged.h"


int
ged_edcomb_core(struct ged *gedp, int argc, const char *argv[])
{
    struct directory *dp;
    int regionid, air, mat, los;
    struct rt_db_internal intern;
    struct rt_comb_internal *comb;
    static const char *usage = "combname region_flag region_id air los material_id";

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

    if (argc != 7) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    GED_DB_LOOKUP(gedp, dp, argv[1], LOOKUP_NOISY, BRLCAD_ERROR);
    GED_CHECK_COMB(gedp, dp, BRLCAD_ERROR);

    if (sscanf(argv[3], "%d", &regionid) != 1) {
	bu_vls_printf(gedp->ged_result_str, "Bad region identifier");
	return BRLCAD_ERROR;
    }
    if (sscanf(argv[4], "%d", &air) != 1) {
	bu_vls_printf(gedp->ged_result_str, "Bad air code");
	return BRLCAD_ERROR;
    }
    if (sscanf(argv[5], "%d", &los) != 1) {
	bu_vls_printf(gedp->ged_result_str, "Bad los line-of-sight equivalence factor");
	return BRLCAD_ERROR;
    }
    if (sscanf(argv[6], "%d", &mat) != 1) {
	bu_vls_printf(gedp->ged_result_str, "Bad material identifier");
	return BRLCAD_ERROR;
    }

    GED_DB_GET_INTERNAL(gedp, &intern, dp, (fastf_t *)NULL, &rt_uniresource, BRLCAD_ERROR);
    comb = (struct rt_comb_internal *)intern.idb_ptr;
    RT_CK_COMB(comb);

    if (argv[2][0] == 'R' || atoi(argv[2]))
	comb->region_flag = 1;
    else
	comb->region_flag = 0;

    comb->region_id = regionid;
    comb->aircode = air;
    comb->los = los;
    comb->GIFTmater = mat;
    GED_DB_PUT_INTERNAL(gedp, dp, &intern, &rt_uniresource, BRLCAD_ERROR);

    return BRLCAD_OK;
}


#ifdef GED_PLUGIN
#include "../include/plugin.h"
struct ged_cmd_impl edcomb_cmd_impl = {
    "edcomb",
    ged_edcomb_core,
    GED_CMD_DEFAULT
};

const struct ged_cmd edcomb_cmd = { &edcomb_cmd_impl };
const struct ged_cmd *edcomb_cmds[] = { &edcomb_cmd, NULL };

static const struct ged_plugin pinfo = { GED_API,  edcomb_cmds, 1 };

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
