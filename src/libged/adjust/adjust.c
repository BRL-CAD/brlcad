/*                         A D J U S T . C
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
/** @file libged/adjust.c
 *
 * The adjust command.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "ged.h"


int
ged_adjust_core(struct ged *gedp, int argc, const char *argv[])
{
    int status;
    struct directory *dp;
    char *name;
    struct rt_db_internal intern;
    static const char *usage = "object attr value ?attr value?";

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_READ_ONLY(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_HELP;
    }

    if (argc < 4) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    name = (char *)argv[1];

    GED_DB_LOOKUP(gedp, dp, name, LOOKUP_QUIET, BRLCAD_ERROR);

    GED_DB_GET_INTERNAL(gedp, &intern, dp, (matp_t)NULL, &rt_uniresource, BRLCAD_ERROR);
    RT_CK_DB_INTERNAL(&intern);

    /* Find out what type of object we are dealing with and tweak it. */
    RT_CK_FUNCTAB(intern.idb_meth);

    if (!intern.idb_meth->ft_adjust) {
	bu_vls_printf(gedp->ged_result_str, "wdb_export(%s) adjust failure", name);
	return BRLCAD_ERROR;
    }

    status = intern.idb_meth->ft_adjust(gedp->ged_result_str, &intern, argc-2, argv+2);
    if (status == BRLCAD_OK && wdb_put_internal(gedp->ged_wdbp, name, &intern, 1.0) < 0) {
	bu_vls_printf(gedp->ged_result_str, "wdb_export(%s) failure", name);
	rt_db_free_internal(&intern);
	return BRLCAD_ERROR;
    }

    return BRLCAD_OK;
}


#ifdef GED_PLUGIN
#include "../include/plugin.h"
struct ged_cmd_impl adjust_cmd_impl = {
    "adjust",
    ged_adjust_core,
    GED_CMD_DEFAULT
};

const struct ged_cmd adjust_cmd = { &adjust_cmd_impl };
const struct ged_cmd *adjust_cmds[] = { &adjust_cmd, NULL };

static const struct ged_plugin pinfo = { GED_API,  adjust_cmds, 1 };

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
