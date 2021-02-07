/*                         D S P . C
 * BRL-CAD
 *
 * Copyright (c) 2017-2021 United States Government as represented by
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
/** @file libged/dsp.c
 *
 * DSP command for displacement map operations.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "bu/opt.h"
#include "rt/geom.h"
#include "wdb.h"
#include "../ged_private.h"

/* FIXME - we want the DSP macro for convenience here - should this be in include/rt/dsp.h ? */
#include "../librt/primitives/dsp/dsp.h"

int
ged_dsp_core(struct ged *gedp, int argc, const char *argv[])
{
    struct directory *dsp_dp;
    struct rt_db_internal intern;
    struct rt_dsp_internal *dsp;
    const char *cmd = argv[0];
    const char *sub = NULL;
    const char *primitive = NULL;
    static const char *usage = "<obj> [command]\n";

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_READ_ONLY(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc < 3) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", cmd, usage);
	bu_vls_printf(gedp->ged_result_str, "commands:\n");
	bu_vls_printf(gedp->ged_result_str, "\txy x y                  - report the height value at (x,y)\n");
	bu_vls_printf(gedp->ged_result_str, "\tdiff obj [min_diff_val] - report height differences at x,y coordinates between two dsp objects\n");
	return GED_ERROR;
    }

    /* get dsp */
    primitive = argv[1];
    GED_DB_LOOKUP(gedp, dsp_dp, primitive, LOOKUP_NOISY, GED_ERROR & GED_QUIET);
    GED_DB_GET_INTERNAL(gedp, &intern, dsp_dp, bn_mat_identity, &rt_uniresource, GED_ERROR);

    if (intern.idb_major_type != DB5_MAJORTYPE_BRLCAD || intern.idb_minor_type != DB5_MINORTYPE_BRLCAD_DSP) {
	bu_vls_printf(gedp->ged_result_str, "%s: %s is not a DSP solid!", cmd, primitive);
	rt_db_free_internal(&intern);
	return GED_ERROR;
    }

    dsp = (struct rt_dsp_internal *)intern.idb_ptr;
    RT_DSP_CK_MAGIC(dsp);

    /* execute subcommand */
    sub = argv[2];
    if (BU_STR_EQUAL(sub, "xy")) {
	unsigned short elev;
	unsigned int gx = 0;
	unsigned int gy = 0;
	(void)bu_opt_int(NULL, 1, &argv[3], &gx);
	(void)bu_opt_int(NULL, 1, &argv[4], &gy);
	if (gx > dsp->dsp_xcnt || gy > dsp->dsp_ycnt) {
	    bu_vls_printf(gedp->ged_result_str, "Error - xy coordinate (%d,%d) is outside max data bounds of dsp: (%d,%d)", gx, gy, dsp->dsp_xcnt, dsp->dsp_ycnt);
	    rt_db_free_internal(&intern);
	    return GED_ERROR;
	} else {
	    elev = DSP(dsp, gx, gy);
	    bu_vls_printf(gedp->ged_result_str, "%d", elev);
	}
	rt_db_free_internal(&intern);
	return GED_OK;
    }
    if (BU_STR_EQUAL(sub, "diff")) {
	struct directory *dsp_dp2;
	struct rt_db_internal intern2;
	struct rt_dsp_internal *dsp2;
	if (argc < 4) {
	    bu_vls_printf(gedp->ged_result_str, "Error - diff subcommand specified, but not the object to diff against.");
	    rt_db_free_internal(&intern);
	    return GED_ERROR;
	}
	GED_DB_LOOKUP(gedp, dsp_dp2, argv[3], LOOKUP_NOISY, GED_ERROR & GED_QUIET);
	GED_DB_GET_INTERNAL(gedp, &intern2, dsp_dp2, bn_mat_identity, &rt_uniresource, GED_ERROR);

	if (intern2.idb_major_type != DB5_MAJORTYPE_BRLCAD || intern2.idb_minor_type != DB5_MINORTYPE_BRLCAD_DSP) {
	    bu_vls_printf(gedp->ged_result_str, "%s: %s is not a DSP solid!", cmd, argv[3]);
	    rt_db_free_internal(&intern);
	    rt_db_free_internal(&intern2);
	    return GED_ERROR;
	}

	dsp2 = (struct rt_dsp_internal *)intern.idb_ptr;
	RT_DSP_CK_MAGIC(dsp2);

	if (dsp->dsp_xcnt != dsp2->dsp_xcnt || dsp->dsp_ycnt != dsp2->dsp_ycnt) {
	    bu_vls_printf(gedp->ged_result_str, "%s xy grid size (%d,%d) differs from that of %s: (%d,%d)", dsp_dp2->d_namep, dsp2->dsp_xcnt, dsp2->dsp_ycnt, dsp_dp->d_namep, dsp->dsp_xcnt, dsp->dsp_ycnt);
	    rt_db_free_internal(&intern);
	    rt_db_free_internal(&intern2);
	    return GED_OK;
	} else {
	    uint32_t i, j;
	    for (i = 0; i < dsp->dsp_xcnt; i++) {
		for (j = 0; j < dsp->dsp_ycnt; j++) {
		    unsigned short e1 = DSP(dsp, i, j);
		    unsigned short e2 = DSP(dsp2, i, j);
		    if (e1 != e2) {
			unsigned short delta = (e1 > e2) ? e1 - e2 : e2 - e1;
			bu_vls_printf(gedp->ged_result_str, "(%d,%d): %d\n", i, j, delta);
		    }
		}
	    }
	}

	rt_db_free_internal(&intern);
	rt_db_free_internal(&intern2);
	return GED_OK;
    }

    bu_vls_printf(gedp->ged_result_str, "Error - unknown dsp subcommand: %s", sub);
    rt_db_free_internal(&intern);
    return GED_ERROR;
}


#ifdef GED_PLUGIN
#include "../include/plugin.h"
struct ged_cmd_impl dsp_cmd_impl = {
    "dsp",
    ged_dsp_core,
    GED_CMD_DEFAULT
};

const struct ged_cmd dsp_cmd = { &dsp_cmd_impl };
const struct ged_cmd *dsp_cmds[] = { &dsp_cmd, NULL };

static const struct ged_plugin pinfo = { GED_API,  dsp_cmds, 1 };

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
