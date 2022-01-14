/*                         C E N T E R . C
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
/** @file libged/center.c
 *
 * The center command.
 *
 */

#include "common.h"

#include <iomanip>
#include <limits>
#include <sstream>
#include <string>

#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "../ged_private.h"
#include "./ged_view.h"

int
ged_center_core(struct ged *gedp, int argc, const char *argv[])
{
    point_t center;
    static const char *usage = "[-v] | [x y z]";

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_VIEW(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* get view center */
    if (argc == 1) {
	MAT_DELTAS_GET_NEG(center, gedp->ged_gvp->gv_center);
	VSCALE(center, center, gedp->dbip->dbi_base2local);
	bn_encode_vect(gedp->ged_result_str, center, 1);

	return GED_OK;
    }

    if (argc == 2 && BU_STR_EQUAL(argv[1], "-v")) {
	std::ostringstream ss;
	MAT_DELTAS_GET_NEG(center, gedp->ged_gvp->gv_center);
	VSCALE(center, center, gedp->dbip->dbi_base2local);
	ss << std::fixed << std::setprecision(std::numeric_limits<fastf_t>::max_digits10) << center[X];
	ss << " ";
	ss << std::fixed << std::setprecision(std::numeric_limits<fastf_t>::max_digits10) << center[Y];
	ss << " ";
	ss << std::fixed << std::setprecision(std::numeric_limits<fastf_t>::max_digits10) << center[Z];
	bu_vls_printf(gedp->ged_result_str, "%s", ss.str().c_str());
	return GED_OK;
    }

    if (argc != 2 && argc != 4) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    /* set view center */
    if (argc == 2) {
	int success = 1;
	if (bn_decode_vect(center, argv[1]) != 3) {
	    success = 0;
	}
	if (!success) {
	    success = 1;
	    std::string vline(argv[1]);
	    size_t spos = vline.find_first_of(",:");
	    std::string xstr = vline.substr(0, spos);
	    vline.erase(0, spos+1);
	    spos = vline.find_first_of(",:");
	    std::string ystr = vline.substr(0, spos);
	    vline.erase(0, spos+1);
	    std::string zstr = vline;
	    struct bu_vls xvalstr = BU_VLS_INIT_ZERO;
	    struct bu_vls yvalstr = BU_VLS_INIT_ZERO;
	    struct bu_vls zvalstr = BU_VLS_INIT_ZERO;
	    bu_vls_sprintf(&xvalstr, "%s", xstr.c_str());
	    bu_vls_sprintf(&yvalstr, "%s", ystr.c_str());
	    bu_vls_sprintf(&zvalstr, "%s", zstr.c_str());
	    bu_vls_trimspace(&xvalstr);
	    bu_vls_trimspace(&yvalstr);
	    bu_vls_trimspace(&zvalstr);
	    fastf_t xval, yval, zval;
	    const char *xstrptr = bu_vls_cstr(&xvalstr);
	    const char *ystrptr = bu_vls_cstr(&yvalstr);
	    const char *zstrptr = bu_vls_cstr(&zvalstr);
	    if (bu_opt_fastf_t(NULL, 1, (const char **)&xstrptr, (void *)&xval) < 0) {
		success = 0;
	    }
	    if (bu_opt_fastf_t(NULL, 1, (const char **)&ystrptr, (void *)&yval) < 0) {
		success = 0;
	    }
	    if (bu_opt_fastf_t(NULL, 1, (const char **)&zstrptr, (void *)&zval) < 0) {
		success = 0;
	    }
	    VSET(center, xval, yval, zval);
	}
	if (!success) {
	    bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	    return GED_ERROR;
	}
    } else {
	double scan[3];

	if (sscanf(argv[1], "%lf", &scan[X]) != 1) {
	    bu_vls_printf(gedp->ged_result_str, "ged_center: bad X value - %s\n", argv[1]);
	    return GED_ERROR;
	}

	if (sscanf(argv[2], "%lf", &scan[Y]) != 1) {
	    bu_vls_printf(gedp->ged_result_str, "ged_center: bad Y value - %s\n", argv[2]);
	    return GED_ERROR;
	}

	if (sscanf(argv[3], "%lf", &scan[Z]) != 1) {
	    bu_vls_printf(gedp->ged_result_str, "ged_center: bad Z value - %s\n", argv[3]);
	    return GED_ERROR;
	}

	/* convert from double to fastf_t */
	VMOVE(center, scan);
    }

    VSCALE(center, center, gedp->dbip->dbi_local2base);
    MAT_DELTAS_VEC_NEG(gedp->ged_gvp->gv_center, center);
    bv_update(gedp->ged_gvp);

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
