/*                         I L L U M . C
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
/** @file libged/illum.c
 *
 * The illum command.
 *
 */

#include "common.h"
#include <string.h>

#include "ged.h"
#include "../ged_private.h"


/*
 * Illuminate/highlight database object
 *
 * Usage:
 * illum [-n] obj
 *
 */
int
ged_illum_core(struct ged *gedp, int argc, const char *argv[])
{
    struct display_list *gdlp;
    struct display_list *next_gdlp;
    int found = 0;
    int illum = 1;
    static const char *usage = "[-n] obj";

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

    if (argc == 3) {
	if (argv[1][0] == '-' && argv[1][1] == 'n')
	    illum = 0;
	else
	    goto bad;

	--argc;
	++argv;
    }

    if (argc != 2)
	goto bad;

    gdlp = BU_LIST_NEXT(display_list, gedp->ged_gdp->gd_headDisplay);
    while (BU_LIST_NOT_HEAD(gdlp, gedp->ged_gdp->gd_headDisplay)) {
	next_gdlp = BU_LIST_PNEXT(display_list, gdlp);

	found += dl_set_illum(gdlp, argv[1], illum);

	gdlp = next_gdlp;
    }


    if (!found) {
	bu_vls_printf(gedp->ged_result_str, "illum: %s not found", argv[1]);
	return GED_ERROR;
    }

    return GED_OK;

bad:
    bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
    return GED_ERROR;
}


#ifdef GED_PLUGIN
#include "../include/plugin.h"
struct ged_cmd_impl illum_cmd_impl = {
    "illum",
    ged_illum_core,
    GED_CMD_DEFAULT
};

const struct ged_cmd illum_cmd = { &illum_cmd_impl };
const struct ged_cmd *illum_cmds[] = { &illum_cmd, NULL };

static const struct ged_plugin pinfo = { GED_API,  illum_cmds, 1 };

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
