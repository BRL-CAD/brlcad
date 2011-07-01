/*                         O V E R L A Y . C
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
/** @file libged/overlay.c
 *
 * The overlay command.
 *
 */

#include "common.h"

#include "bio.h"

#include "./ged_private.h"


int
ged_overlay(struct ged *gedp, int argc, const char *argv[])
{
    int ret;
    struct bn_vlblock*vbp;
    FILE *fp;
    double char_size;
    char *name;
    static const char *usage = "file.pl char_size [name]";

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_DRAWABLE(gedp, GED_ERROR);
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

    if (sscanf(argv[2], "%lf", &char_size) != 1) {
	bu_vls_printf(gedp->ged_result_str, "ged_overlay: bad character size - %s\n", argv[2]);
	return GED_ERROR;
    }

    if (argc == 3)
	name = "_PLOT_OVERLAY_";
    else
	name = (char *)argv[3];

    if ((fp = fopen(argv[1], "rb")) == NULL) {
	bu_vls_printf(gedp->ged_result_str, "ged_overlay: failed to open file - %s\n", argv[1]);
	return GED_ERROR;
    }

    vbp = rt_vlblock_init();
    ret = rt_uplot_to_vlist(vbp, fp, char_size, gedp->ged_gdp->gd_uplotOutputMode);
    fclose(fp);

    if (ret < 0) {
	rt_vlblock_free(vbp);
	return GED_ERROR;
    }

    _ged_cvt_vlblock_to_solids(gedp, vbp, name, 0);
    rt_vlblock_free(vbp);

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
