/*                         P S E T . C
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
/** @file libged/pset.c
 *
 * The pset command.
 */

#include "common.h"

#include <string.h>

#include "bu/cmd.h"
#include "rt/geom.h"
#include "raytrace.h"

#include "../ged_private.h"


int
ged_pset_core(struct ged *gedp, int argc, const char *argv[])
{
    int ret;
    struct rt_db_internal intern;

    /* intentionally double for scan */
    double val;

    char *last;
    struct directory *dp;
    static const char *usage = "obj attribute val";

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

    if (argc != 4) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    if (sscanf(argv[3], "%lf", &val) != 1) {
	bu_vls_printf(gedp->ged_result_str, "%s: bad value - %s", argv[0], argv[3]);
	return BRLCAD_ERROR;
    }

    if ((last = strrchr(argv[1], '/')) == NULL)
	last = (char *)argv[1];
    else
	++last;

    if (last[0] == '\0') {
	bu_vls_printf(gedp->ged_result_str, "%s: illegal input - %s", argv[0], argv[1]);
	return BRLCAD_ERROR;
    }

    if ((dp = db_lookup(gedp->dbip, last, LOOKUP_QUIET)) == RT_DIR_NULL) {
	bu_vls_printf(gedp->ged_result_str, "%s: %s not found", argv[0], argv[1]);
	return BRLCAD_ERROR;
    }

    GED_DB_GET_INTERNAL(gedp, &intern, dp, (matp_t)NULL, &rt_uniresource, BRLCAD_ERROR);
    RT_CK_DB_INTERNAL(&intern);

    if (intern.idb_major_type != DB5_MAJORTYPE_BRLCAD) {
	bu_vls_printf(gedp->ged_result_str, "%s: Object not eligible for scaling.", argv[0]);
	rt_db_free_internal(&intern);

	return BRLCAD_ERROR;
    }

    switch (intern.idb_minor_type) {
	case DB5_MINORTYPE_BRLCAD_METABALL:
	    ret = _ged_set_metaball(gedp, (struct rt_metaball_internal *)intern.idb_ptr, argv[2], val);
	    break;
	default:
	    bu_vls_printf(gedp->ged_result_str, "%s: Object not yet supported.", argv[0]);
	    rt_db_free_internal(&intern);

	    return BRLCAD_ERROR;
    }

    if (ret == BRLCAD_OK) {
	GED_DB_PUT_INTERNAL(gedp, dp, &intern, &rt_uniresource, BRLCAD_ERROR);
    } else if (ret & BRLCAD_ERROR) {
	rt_db_free_internal(&intern);
    }

    return ret;
}


#ifdef GED_PLUGIN
#include "../include/plugin.h"
struct ged_cmd_impl pset_cmd_impl = {
    "pset",
    ged_pset_core,
    GED_CMD_DEFAULT
};

const struct ged_cmd pset_cmd = { &pset_cmd_impl };
const struct ged_cmd *pset_cmds[] = { &pset_cmd, NULL };

static const struct ged_plugin pinfo = { GED_API,  pset_cmds, 1 };

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
