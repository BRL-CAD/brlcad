/*                         O V E R L A Y . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2020 United States Government as represented by
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

#include "dm.h"

#include "./ged_private.h"

int
ged_overlay(struct ged *gedp, int argc, const char *argv[])
{
    int ret;
    int print_help = 0;
    int write_fb = 0;
    double size = 0.0;
    struct dm *dmp = NULL;
    struct fb *fbp = NULL;
    const char *name = "_PLOT_OVERLAY_";

    static char usage[] = "Usage: overlay [-h] [-F] file\n";

    struct bu_opt_desc d[4];
    BU_OPT(d[0], "h", "help",   "",   NULL,            &print_help,  "Print help and exit");
    BU_OPT(d[1], "F", "fb",     "",   NULL,            &write_fb,    "Overlay image on framebuffer");
    BU_OPT(d[2], "s", "size",   "#",  &bu_opt_fastf_t, &size,        "Character size for plot drawing");
    BU_OPT_NULL(d[3]);
    //BU_OPT(d[2], "X", "scr_xoff",       "#",    &bu_opt_int,      &scr_xoff,         "X offset");
    //BU_OPT(d[3], "Y", "scr_yoff",       "#",    &bu_opt_int,      &scr_yoff,         "Y offset");
    //BU_OPT(d[4], "w", "width",          "#",    &bu_opt_int,      &width,            "image width to grab");
    //BU_OPT(d[5], "n", "height",         "#",    &bu_opt_int,      &height,           "image height to grab");
    //BU_OPT_NULL(d[6]);

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_DRAWABLE(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    if (!gedp->ged_dmp) {
	bu_vls_printf(gedp->ged_result_str, ": no display manager currently active");
	return GED_ERROR;
    }

    dmp = (struct dm *)gedp->ged_dmp;

    /* must be wanting help */
    if (argc == 1) {
	_ged_cmd_help(gedp, usage, d);
	return GED_HELP;
    }

    argc-=(argc>0); argv+=(argc>0); /* done with command name argv[0] */

    int opt_ret = bu_opt_parse(NULL, argc, argv, d);

    if (print_help) {
	_ged_cmd_help(gedp, usage, d);
	return GED_HELP;
    }

    if (NEAR_ZERO(size, VUNITIZE_TOL)) {
	if (!gedp->ged_gvp) {
	    bu_vls_printf(gedp->ged_result_str, ": no character size specified, and could not determine default value");
	    return GED_ERROR;
	}
	size = gedp->ged_gvp->gv_scale * 0.01;
    }

    argc = opt_ret;

    if (write_fb) {
	fbp = dm_get_fb(dmp);
	if (!fbp) {
	    bu_vls_printf(gedp->ged_result_str, ": display manager does not have a framebuffer");
	    return GED_ERROR;
	}
    }

    /* must be wanting help */
    if (!argc) {
	_ged_cmd_help(gedp, usage, d);
	return GED_HELP;
    }
    /* check arg cnt */
    if (argc > 2) {
	_ged_cmd_help(gedp, usage, d);
	return GED_ERROR;
    }

    /* Second arg, if present, is view obj name */
    if (argc == 2) {
	name = argv[1];
    }

    if (!bu_file_exists(argv[0], NULL)) {
	bu_vls_printf(gedp->ged_result_str, ": file %s not found", argv[0]);
	return GED_ERROR;
    }

    if (!write_fb) {
	struct bn_vlblock*vbp;
	FILE *fp = fopen(argv[0], "rb");

	/* If we don't have an exact filename match, see if we got a pattern -
	 * it is practical to plot many plot files simultaneously, so that may
	 * be what was specified. */
	if (fp == NULL) {
	    char **files = NULL;
	    size_t count = bu_file_list(".", argv[1], &files);
	    if (count <= 0) {
		bu_vls_printf(gedp->ged_result_str, "ged_overlay: failed to open file - %s\n", argv[1]);
		return GED_ERROR;
	    }
	    vbp = rt_vlblock_init();
	    for (size_t i = 0; i < count; i++) {
		if ((fp = fopen(files[i], "rb")) == NULL) {
		    bu_vls_printf(gedp->ged_result_str, "ged_overlay: failed to open file - %s\n", files[i]);
		    bu_argv_free(count, files);
		    return GED_ERROR;
		}
		ret = rt_uplot_to_vlist(vbp, fp, size, gedp->ged_gdp->gd_uplotOutputMode);
		fclose(fp);
		if (ret < 0) {
		    bn_vlblock_free(vbp);
		    bu_argv_free(count, files);
		    return GED_ERROR;
		}
	    }
	    bu_argv_free(count, files);
	    ret = 0;
	} else {
	    vbp = rt_vlblock_init();
	    ret = rt_uplot_to_vlist(vbp, fp, size, gedp->ged_gdp->gd_uplotOutputMode);
	    fclose(fp);
	    if (ret < 0) {
		bn_vlblock_free(vbp);
		return GED_ERROR;
	    }
	}

	_ged_cvt_vlblock_to_solids(gedp, vbp, name, 0);
	bn_vlblock_free(vbp);

	return GED_OK;
    } else {
	const char *av[2];
	const char *av0 = "overlay";
	av[0] = av0;
	av[1] = argv[0];
	return ged_icv2fb(gedp, 2, (const char **)av);
    }
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
