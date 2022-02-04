/*                         U N I T S . C
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
/** @file libged/units.c
 *
 * The units command.
 *
 */

#include "common.h"

#include <string.h>

#include "bu/cmd.h"
#include "bu/units.h"

#include "../ged_private.h"


int
ged_units_core(struct ged *gedp, int argc, const char *argv[])
{
    double loc2mm;
    const char *str;
    int sflag = 0;
    static const char *usage = "[-s] [mm|cm|m|in|ft|...]";

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    if (argc > 2) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    if (argc == 2) {
	if (BU_STR_EQUAL(argv[1], "-s")) {
	    --argc;
	    ++argv;

	    sflag = 1;
	} else if (BU_STR_EQUAL(argv[1], "-t")) {
	    struct bu_vls *vlsp = bu_units_strings_vls();

	    bu_vls_printf(gedp->ged_result_str, "%s", bu_vls_addr(vlsp));
	    bu_vls_free(vlsp);
	    bu_free(vlsp, "ged_units_core: vlsp");

	    return BRLCAD_OK;
	}
    }

    /* Get units */
    if (argc == 1) {
	str = bu_units_string(gedp->dbip->dbi_local2base);
	if (!str) str = "Unknown_unit";

	if (sflag)
	    bu_vls_printf(gedp->ged_result_str, "%s", str);
	else
	    bu_vls_printf(gedp->ged_result_str, "You are editing in '%s'.  1 %s = %g mm \n",
			  str, str, gedp->dbip->dbi_local2base);

	return BRLCAD_OK;
    }

    /* Set units */
    /* Allow inputs of the form "25cm" or "3ft" */
    if ((loc2mm = bu_mm_value(argv[1])) <= 0) {
	bu_vls_printf(gedp->ged_result_str,
		      "%s: unrecognized unit\nvalid units: <um|mm|cm|m|km|in|ft|yd|mi>\n",
		      argv[1]);
	return BRLCAD_ERROR;
    }

    if (db_update_ident(gedp->dbip, gedp->dbip->dbi_title, loc2mm) < 0) {
	bu_vls_printf(gedp->ged_result_str, "Warning: unable to stash working units into database\n");
    }

    gedp->dbip->dbi_local2base = loc2mm;
    gedp->dbip->dbi_base2local = 1.0 / loc2mm;

    str = bu_units_string(gedp->dbip->dbi_local2base);
    if (!str) str = "Unknown_unit";
    bu_vls_printf(gedp->ged_result_str, "You are now editing in '%s'.  1 %s = %g mm \n",
		  str, str, gedp->dbip->dbi_local2base);

    return BRLCAD_OK;
}


#ifdef GED_PLUGIN
#include "../include/plugin.h"
struct ged_cmd_impl units_cmd_impl = {
    "units",
    ged_units_core,
    GED_CMD_DEFAULT
};

const struct ged_cmd units_cmd = { &units_cmd_impl };
const struct ged_cmd *units_cmds[] = { &units_cmd, NULL };

static const struct ged_plugin pinfo = { GED_API,  units_cmds, 1 };

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
