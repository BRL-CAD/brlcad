/*                         O C E N T E R . C
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
/** @file libged/ocenter.c
 *
 * The ocenter command.
 *
 */

#include "common.h"


#include "../ged_private.h"


int
ged_ocenter_core(struct ged *gedp, int argc, const char *argv[])
{
    struct directory *dp;
    struct _ged_trace_data gtd;
    struct rt_db_internal intern;
    mat_t dmat;
    mat_t emat;
    mat_t tmpMat;
    mat_t invXform;
    point_t rpp_min;
    point_t rpp_max;
    point_t oldCenter;
    point_t center;
    point_t delta;
    double scan[3];
    static const char *usage = "object [x y z]";

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_HELP;
    }

    if (argc != 2 && argc != 5) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    /*
     * One of the get bounds routines needs to be fixed to
     * work with all cases. In the meantime...
     */
    if (_ged_get_obj_bounds2(gedp, 1, argv+1, &gtd, rpp_min, rpp_max) & BRLCAD_ERROR)
	return BRLCAD_ERROR;

    dp = gtd.gtd_obj[gtd.gtd_objpos-1];
    if (!(dp->d_flags & RT_DIR_SOLID)) {
	if (rt_obj_bounds(gedp->ged_result_str, gedp->dbip, 1, argv+1, 1, rpp_min, rpp_max) == BRLCAD_ERROR)
	    return BRLCAD_ERROR;
    }

    VADD2(oldCenter, rpp_min, rpp_max);
    VSCALE(oldCenter, oldCenter, 0.5);

    if (argc == 2) {
	VSCALE(center, oldCenter, gedp->dbip->dbi_base2local);
	bn_encode_vect(gedp->ged_result_str, center, 1);

	return BRLCAD_OK;
    }

    GED_CHECK_READ_ONLY(gedp, BRLCAD_ERROR);

    /* Read in the new center */
    if (sscanf(argv[2], "%lf", &scan[X]) != 1) {
	bu_vls_printf(gedp->ged_result_str, "%s: bad x value - %s", argv[0], argv[2]);
	return BRLCAD_ERROR;
    }

    if (sscanf(argv[3], "%lf", &scan[Y]) != 1) {
	bu_vls_printf(gedp->ged_result_str, "%s: bad y value - %s", argv[0], argv[3]);
	return BRLCAD_ERROR;
    }

    if (sscanf(argv[4], "%lf", &scan[Z]) != 1) {
	bu_vls_printf(gedp->ged_result_str, "%s: bad z value - %s", argv[0], argv[4]);
	return BRLCAD_ERROR;
    }

    VSCALE(center, scan, gedp->dbip->dbi_local2base);
    VSUB2(delta, center, oldCenter);
    MAT_IDN(dmat);
    MAT_DELTAS_VEC(dmat, delta);

    bn_mat_inv(invXform, gtd.gtd_xform);
    bn_mat_mul(tmpMat, invXform, dmat);
    bn_mat_mul(emat, tmpMat, gtd.gtd_xform);

    GED_DB_GET_INTERNAL(gedp, &intern, dp, emat, &rt_uniresource, BRLCAD_ERROR);
    RT_CK_DB_INTERNAL(&intern);
    GED_DB_PUT_INTERNAL(gedp, dp, &intern, &rt_uniresource, BRLCAD_ERROR);

    return BRLCAD_OK;
}


#ifdef GED_PLUGIN
#include "../include/plugin.h"
struct ged_cmd_impl ocenter_cmd_impl = {
    "ocenter",
    ged_ocenter_core,
    GED_CMD_DEFAULT
};

const struct ged_cmd ocenter_cmd = { &ocenter_cmd_impl };
const struct ged_cmd *ocenter_cmds[] = { &ocenter_cmd, NULL };

static const struct ged_plugin pinfo = { GED_API,  ocenter_cmds, 1 };

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
