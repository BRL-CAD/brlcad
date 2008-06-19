/*                         S E T _ T R A N S P A R E N C Y . C
 * BRL-CAD
 *
 * Copyright (c) 2008 United States Government as represented by
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
/** @file set_transparency.c
 *
 * The set_transparency command.
 *
 */

#include "ged.h"
#include "solid.h"


/*
 * Set the transparency of the specified object
 *
 * Usage:
 *        set_transparency obj tr
 *
 */
int
ged_set_transparency(struct ged *gedp, int argc, const char *argv[])
{
    register struct solid *sp;
    int i;
    struct directory **dpp;
    register struct directory **tmp_dpp;
    fastf_t transparency;
    static const char *usage = "tr";

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_DRAWABLE(gedp, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(&gedp->ged_result_str, 0);
    gedp->ged_result = GED_RESULT_NULL;
    gedp->ged_result_flags = 0;

    /* must be wanting help */
    if (argc == 1) {
	gedp->ged_result_flags |= GED_RESULT_FLAGS_HELP_BIT;
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_OK;
    }


    if (argc != 3) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    if (sscanf(argv[2], "%lf", &transparency) != 1) {
	bu_vls_printf(&gedp->ged_result_str, "dgo_set_transparency: bad transparency - %s\n", argv[2]);
	return GED_ERROR;
    }

    if ((dpp = ged_build_dpp(gedp, argv[1])) == NULL) {
	return GED_OK;
    }

    FOR_ALL_SOLIDS(sp, &gedp->ged_gdp->gd_headSolid) {
	for (i = 0, tmp_dpp = dpp;
	     i < sp->s_fullpath.fp_len && *tmp_dpp != DIR_NULL;
	     ++i, ++tmp_dpp) {
	    if (sp->s_fullpath.fp_names[i] != *tmp_dpp)
		break;
	}

	if (*tmp_dpp != DIR_NULL)
	    continue;

	/* found a match */
	sp->s_transparency = transparency;
    }

    if (dpp != (struct directory **)NULL)
	bu_free((genptr_t)dpp, "ged_set_transparency: directory pointers");

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
