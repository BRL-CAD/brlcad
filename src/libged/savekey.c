/*                         S A V E K E Y . C
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
/** @file libged/savekey.c
 *
 * The savekey command.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include "bio.h"

#include "./ged_private.h"


/**
 * Write out the information that RT's -M option needs to show current view.
 * Note that the model-space location of the eye is a parameter,
 * as it can be computed in different ways.
 * The is the OLD format, needed only when sending to RT on a pipe,
 * due to some oddball hackery in RT to determine old -vs- new format.
 */
static void
savekey_rt_oldwrite(struct ged *gedp, FILE *fp, fastf_t *eye_model)
{
    int i;

    (void)fprintf(fp, "%.9e\n", gedp->ged_gvp->gv_size);
    (void)fprintf(fp, "%.9e %.9e %.9e\n",
		  eye_model[X], eye_model[Y], eye_model[Z]);
    for (i=0; i < 16; i++) {
	(void)fprintf(fp, "%.9e ", gedp->ged_gvp->gv_rotation[i]);
	if ((i%4) == 3)
	    (void)fprintf(fp, "\n");
    }
    (void)fprintf(fp, "\n");
}


int
ged_savekey(struct ged *gedp, int argc, const char *argv[])
{
    FILE *fp;
    fastf_t timearg;
    vect_t eye_model;
    vect_t temp;
    static const char *usage = "file [time]";

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_VIEW(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc < 2 || 3 < argc) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    if ((fp = fopen(argv[1], "a")) == NULL) {
	perror(argv[1]);
	return TCL_ERROR;
    }
    if (argc > 2) {
	timearg = atof(argv[2]);
	(void)fprintf(fp, "%f\n", timearg);
    }
    /*
     * Eye is in conventional place.
     */
    VSET(temp, 0.0, 0.0, 1.0);
    MAT4X3PNT(eye_model, gedp->ged_gvp->gv_view2model, temp);
    savekey_rt_oldwrite(gedp, fp, eye_model);
    (void)fclose(fp);

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
