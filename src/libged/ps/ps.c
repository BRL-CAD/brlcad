/*                         P S . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2021 United States Government as represented by
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
/** @file libged/ps.c
 *
 * The ps command.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>


#include "bu/getopt.h"
#include "vmath.h"
#include "bn.h"


#include "../ged_private.h"


static fastf_t ps_default_ppi = 72.0;
static fastf_t ps_default_scale = 4.5 * 72.0 / 4096.0;

int
ged_ps_core(struct ged *gedp, int argc, const char *argv[])
{
    FILE *fp;
    struct bu_vls creator = BU_VLS_INIT_ZERO;
    struct bu_vls title = BU_VLS_INIT_ZERO;
    struct bu_vls font = BU_VLS_INIT_ZERO;
    fastf_t scale = ps_default_scale;
    int linewidth = 4;
    int xoffset = 0;
    int yoffset = 0;
    int border = 0;
    int k;
    int r, g, b;

    float border_red = 0.0;
    float border_green = 0.0;
    float border_blue = 0.0;

    static const char *usage = "[-a author] [-b] [-c r/g/b] [-f font] [-s size] [-t title] [-x offset] [-y offset] file";

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_VIEW(gedp, GED_ERROR);
    GED_CHECK_DRAWABLE(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    /* Initialize var defaults */
    bu_vls_printf(&font, "Courier");
    bu_vls_printf(&title, "No Title");
    bu_vls_printf(&creator, "LIBGED ps");

    /* Process options */
    bu_optind = 1;
    while ((k = bu_getopt(argc, (char * const *)argv, "a:bc:f:s:t:x:y:")) != -1) {
	double tmp_f;

	switch (k) {
	    case 'a':
		bu_vls_trunc(&creator, 0);
		bu_vls_printf(&creator, "%s", bu_optarg);

		break;
	    case 'b':
		border = 1;
		break;
	    case 'c':
		if (sscanf(bu_optarg, "%d%*c%d%*c%d", &r, &g, &b) != 3) {
		    bu_vls_printf(gedp->ged_result_str, "%s: bad color - %s", argv[0], bu_optarg);
		    return GED_ERROR;
		}

		/* Clamp color values */
		if (r < 0)
		    r = 0;
		else if (r > 255)
		    r = 255;

		if (g < 0)
		    g = 0;
		else if (g > 255)
		    g = 255;

		if (b < 0)
		    b = 0;
		else if (b > 255)
		    b = 255;

		border_red = PS_COLOR(r);
		border_green = PS_COLOR(g);
		border_blue = PS_COLOR(b);

		break;
	    case 'f':
		bu_vls_trunc(&font, 0);
		bu_vls_printf(&font, "%s", bu_optarg);

		break;
	    case 's':
		if (sscanf(bu_optarg, "%lf", &tmp_f) != 1) {
		    bu_vls_printf(gedp->ged_result_str, "%s: bad size - %s", argv[0], bu_optarg);
		    goto bad;
		}

		if (tmp_f < 0.0 || NEAR_ZERO(tmp_f, 0.1)) {
		    bu_vls_printf(gedp->ged_result_str, "%s: bad size - %s, must be greater than 0.1 inches\n", argv[0], bu_optarg);
		    goto bad;
		}

		scale = tmp_f * ps_default_ppi / 4096.0;

		break;
	    case 't':
		bu_vls_trunc(&title, 0);
		bu_vls_printf(&title, "%s", bu_optarg);

		break;
	    case 'x':
		if (sscanf(bu_optarg, "%lf", &tmp_f) != 1) {
		    bu_vls_printf(gedp->ged_result_str, "%s: bad x offset - %s", argv[0], bu_optarg);
		    goto bad;
		}
		xoffset = (int)(tmp_f * ps_default_ppi);

		break;
	    case 'y':
		if (sscanf(bu_optarg, "%lf", &tmp_f) != 1) {
		    bu_vls_printf(gedp->ged_result_str, "%s: bad y offset - %s", argv[0], bu_optarg);
		    goto bad;
		}
		yoffset = (int)(tmp_f * ps_default_ppi);

		break;
	    default:
		bu_vls_printf(gedp->ged_result_str, "%s: Unrecognized option - %s", argv[0], argv[bu_optind-1]);
		goto bad;
	}
    }

    if ((argc - bu_optind) != 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	goto bad;
    }

    fp = fopen(argv[bu_optind], "wb");
    if (fp == NULL) {
	bu_vls_printf(gedp->ged_result_str, "%s: Error opening file - %s\n", argv[0], argv[bu_optind]);
	goto bad;
    }

    dl_ps(gedp->ged_gdp->gd_headDisplay, fp, border, bu_vls_addr(&font), bu_vls_addr(&title), bu_vls_addr(&creator), linewidth, scale, xoffset, yoffset, gedp->ged_gvp->gv_model2view, gedp->ged_gvp->gv_perspective, gedp->ged_gvp->gv_eye_pos, border_red, border_green, border_blue);

    fclose(fp);

    bu_vls_free(&font);
    bu_vls_free(&title);
    bu_vls_free(&creator);

    return GED_OK;

bad:
    bu_vls_free(&font);
    bu_vls_free(&title);
    bu_vls_free(&creator);

    return GED_ERROR;
}


#ifdef GED_PLUGIN
#include "../include/plugin.h"
struct ged_cmd_impl ps_cmd_impl = {"ps", ged_ps_core, GED_CMD_DEFAULT};
const struct ged_cmd ps_cmd = { &ps_cmd_impl };

struct ged_cmd_impl postscript_cmd_impl = {"postscript", ged_ps_core, GED_CMD_DEFAULT};
const struct ged_cmd postscript_cmd = { &postscript_cmd_impl };

const struct ged_cmd *ps_cmds[] = { &ps_cmd, &postscript_cmd, NULL };

static const struct ged_plugin pinfo = { GED_API,  ps_cmds, 2 };

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
