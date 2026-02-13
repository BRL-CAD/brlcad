/*                         G E T _ A U T O V I E W . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2025 United States Government as represented by
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
/** @file libged/get_autoview.c
 *
 * The get_autoview command.
 *
 */

#include "bu/getopt.h"
#include "dm.h"
#include "ged.h"


#include "../ged_private.h"

/*
 * Get the view size and center such that all displayed solids would be in view
 *
 * Usage:
 * get_autoview
 *
 */
int
ged_get_autoview_core(struct ged *gedp, int argc, const char *argv[])
{
    int is_empty = 1;
    vect_t min, max;
    vect_t center = VINIT_ZERO;
    vect_t radial;
    fastf_t size;
    int pflag = 0;
    int c;
    double sval = (gedp->dbip) ? gedp->dbip->dbi_base2local : 1.0;

    GED_CHECK_DRAWABLE(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc != 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s", argv[0]);
	return GED_HELP;
    }

    /* Parse options. */
    bu_optind = 1;
    while ((c = bu_getopt(argc, (char * const *)argv, "p")) != -1) {
	switch (c) {
	    case 'p':
		pflag = 1;
		break;
	    default: {
		bu_vls_printf(gedp->ged_result_str, "Usage: %s", argv[0]);
		return BRLCAD_ERROR;
	    }
	}
    }

    is_empty = dl_bounding_sph(gedp->i->ged_gdp->gd_headDisplay, &min, &max, pflag);

    if (is_empty) {
	/* Nothing is in view */
	VSETALL(radial, 1000.0);	/* 1 meter */
    } else {
	VADD2SCALE(center, max, min, 0.5);
	VSUB2(radial, max, center);
    }

    if (VNEAR_ZERO(radial, SQRT_SMALL_FASTF))
	VSETALL(radial, 1.0);

    VSCALE(center, center, sval);
    VSCALE(radial, radial, sval * 2.0);
    size = radial[X];
    V_MAX(size, radial[Y]);
    V_MAX(size, radial[Z]);
    bu_vls_printf(gedp->ged_result_str, "center {%g %g %g} size %g", V3ARGS(center), size);

    return BRLCAD_OK;
}

#include "../include/plugin.h"

#define GED_GET_AUTOVIEW_COMMANDS(X, XID) \
    X(get_autoview, ged_get_autoview_core, GED_CMD_DEFAULT) \

GED_DECLARE_COMMAND_SET(GED_GET_AUTOVIEW_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST("libged_get_autoview", 1, GED_GET_AUTOVIEW_COMMANDS)

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
