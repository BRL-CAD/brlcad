/*                         O S C A L E . C
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
/** @file libged/oscale.c
 *
 * The oscale command.
 *
 */

#include "common.h"


#include "../ged_private.h"


/*
 * Scale obj about the keypoint by sf.
 */
int
ged_oscale_core(struct ged *gedp, int argc, const char *argv[])
{
    struct directory *dp;
    struct _ged_trace_data gtd;
    struct rt_db_internal intern;
    mat_t smat;
    mat_t emat;
    mat_t tmpMat;
    mat_t invXform;
    point_t rpp_min;
    point_t rpp_max;
    point_t keypoint;

    /* intentionally double for scan */
    double sf;

    static const char *usage = "obj sf [kX kY kZ]";

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

    if (argc != 3 && argc != 6) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    if (sscanf(argv[2], "%lf", &sf) != 1) {
	bu_vls_printf(gedp->ged_result_str, "%s: bad sf value - %s", argv[0], argv[2]);
	return BRLCAD_ERROR;
    }

    if (argc == 3) {
	if (_ged_get_obj_bounds2(gedp, 1, argv+1, &gtd, rpp_min, rpp_max) & BRLCAD_ERROR)
	    return BRLCAD_ERROR;

	dp = gtd.gtd_obj[gtd.gtd_objpos-1];
	if (!(dp->d_flags & RT_DIR_SOLID)) {
	    if (rt_obj_bounds(gedp->ged_result_str, gedp->dbip, 1, argv+1, 1, rpp_min, rpp_max) == BRLCAD_ERROR)
		return BRLCAD_ERROR;
	}

	VADD2(keypoint, rpp_min, rpp_max);
	VSCALE(keypoint, keypoint, 0.5);
    } else {
	double scan[3];

	/* The user has provided the keypoint. */
	MAT_IDN(gtd.gtd_xform);

	if (sscanf(argv[3], "%lf", &scan[X]) != 1) {
	    bu_vls_printf(gedp->ged_result_str, "%s: bad kx value - %s", argv[0], argv[3]);
	    return BRLCAD_ERROR;
	}

	if (sscanf(argv[4], "%lf", &scan[Y]) != 1) {
	    bu_vls_printf(gedp->ged_result_str, "%s: bad ky value - %s", argv[0], argv[4]);
	    return BRLCAD_ERROR;
	}

	if (sscanf(argv[5], "%lf", &scan[Z]) != 1) {
	    bu_vls_printf(gedp->ged_result_str, "%s: bad kz value - %s", argv[0], argv[5]);
	    return BRLCAD_ERROR;
	}

	VSCALE(keypoint, scan, gedp->dbip->dbi_local2base);

	if ((dp = db_lookup(gedp->dbip, argv[1], LOOKUP_QUIET)) == RT_DIR_NULL) {
	    bu_vls_printf(gedp->ged_result_str, "%s: %s not found", argv[0], argv[1]);
	    return BRLCAD_ERROR;
	}
    }

    MAT_IDN(smat);
    bn_mat_scale_about_pnt(smat, keypoint, sf);

    bn_mat_inv(invXform, gtd.gtd_xform);
    bn_mat_mul(tmpMat, invXform, smat);
    bn_mat_mul(emat, tmpMat, gtd.gtd_xform);

    GED_DB_GET_INTERNAL(gedp, &intern, dp, emat, &rt_uniresource, BRLCAD_ERROR);
    RT_CK_DB_INTERNAL(&intern);
    GED_DB_PUT_INTERNAL(gedp, dp, &intern, &rt_uniresource, BRLCAD_ERROR);

    return BRLCAD_OK;
}


#ifdef GED_PLUGIN
#include "../include/plugin.h"
struct ged_cmd_impl oscale_cmd_impl = {
    "oscale",
    ged_oscale_core,
    GED_CMD_DEFAULT
};

const struct ged_cmd oscale_cmd = { &oscale_cmd_impl };
const struct ged_cmd *oscale_cmds[] = { &oscale_cmd, NULL };

static const struct ged_plugin pinfo = { GED_API,  oscale_cmds, 1 };

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
