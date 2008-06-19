/*                         S E T _ O U T P U T H A N D L E R . C
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
/** @file set_outputhandler.c
 *
 * The set_outputhandler command.
 *
 */

#include "ged.h"

/*
 * Get/set the output handler script
 *
 * Usage:
 *        set_outputhandler [script]
 *
 */
int
ged_set_outputHandler(struct ged *gedp, int argc, const char *argv[])
{
    static const char *usage = "[script]";

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_DRAWABLE(gedp, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(&gedp->ged_result_str, 0);
    gedp->ged_result = GED_RESULT_NULL;
    gedp->ged_result_flags = 0;

    if (argc < 1 || 2 < argc) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    /* Get the output handler script */
    if (argc == 1) {
	bu_vls_printf(&gedp->ged_result_str, "%s", gedp->ged_gdp->gd_outputHandler);
	return GED_OK;
    }

    /* We're now going to set the output handler script */
    /* First, we zap any previous script */
    if (gedp->ged_gdp->gd_outputHandler != NULL) {
	bu_free((genptr_t)gedp->ged_gdp->gd_outputHandler, "ged_set_outputHandler: zap");
	gedp->ged_gdp->gd_outputHandler = NULL;
    }

    if (argv[1] != NULL && argv[1][0] != '\0')
	gedp->ged_gdp->gd_outputHandler = bu_strdup(argv[1]);

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
