/*                         P L O T . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2016 United States Government as represented by
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
/** @file libged/plot.c
 *
 * The plot command.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "bn.h"
#include "bn/plot3.h"


#include "./ged_private.h"


/*
 * plot file [opts]
 * potential options might include:
 * grid, 3d w/color, |filter, infinite Z
 */
int
ged_plot(struct ged *gedp, int argc, const char *argv[])
{
    FILE *fp;
    int Three_D;			/* 0=2-D -vs- 1=3-D */
    int Z_clip;			/* Z clipping */
    int floating;			/* 3-D floating point plot */
    int is_pipe = 0;
    static const char *usage = "file [2|3] [f] [g] [z]";

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_DRAWABLE(gedp, GED_ERROR);
    GED_CHECK_VIEW(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    /* Process any options */
    Three_D = 1;				/* 3-D w/color, by default */
    Z_clip = 0;				/* NO Z clipping, by default*/
    floating = 0;
    while (argv[1] != (char *)0 && argv[1][0] == '-') {
	switch (argv[1][1]) {
	    case 'f':
		floating = 1;
		break;
	    case '3':
		Three_D = 1;
		break;
	    case '2':
		Three_D = 0;		/* 2-D, for portability */
		break;
	    case 'g':
		/* do grid */
		bu_vls_printf(gedp->ged_result_str, "%s: grid unimplemented\n", argv[0]);
		break;
	    case 'z':
	    case 'Z':
		/* Enable Z clipping */
		bu_vls_printf(gedp->ged_result_str, "%s: Clipped in Z to viewing cube\n", argv[0]);
		Z_clip = 1;
		break;
	    default:
		bu_vls_printf(gedp->ged_result_str, "%s: bad PLOT option %s\n", argv[0], argv[1]);
		break;
	}
	argv++;
    }
    if (argv[1] == (char *)0) {
	bu_vls_printf(gedp->ged_result_str, "%s: no filename or filter specified\n", argv[0]);
	return GED_ERROR;
    }
    if (argv[1][0] == '|') {
	struct bu_vls str = BU_VLS_INIT_ZERO;
	bu_vls_strcpy(&str, &argv[1][1]);
	while ((++argv)[1] != (char *)0) {
	    bu_vls_strcat(&str, " ");
	    bu_vls_strcat(&str, argv[1]);
	}
	if ((fp = popen(bu_vls_addr(&str), "w")) == NULL) {
	    perror(bu_vls_addr(&str));
	    return GED_ERROR;
	}

	bu_vls_printf(gedp->ged_result_str, "piped to %s\n", bu_vls_addr(&str));
	bu_vls_free(&str);
	is_pipe = 1;
    } else {
	if ((fp = fopen(argv[1], "w")) == NULL) {
	    perror(argv[1]);
	    return GED_ERROR;
	}

	bu_vls_printf(gedp->ged_result_str, "plot stored in %s\n", argv[1]);
	is_pipe = 0;
    }

    dl_plot(gedp->ged_gdp->gd_headDisplay, fp, gedp->ged_gvp->gv_model2view, floating, gedp->ged_gvp->gv_center, gedp->ged_gvp->gv_scale, Three_D, Z_clip);

    if (is_pipe)
	(void)pclose(fp);
    else
	(void)fclose(fp);

    return GED_ERROR;
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
