/*                       A T T R . C P P
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
/** @file libged/attr.cpp
 *
 * The attr command.
 *
 */

#include "common.h"

#include <utility>
#include <string>
#include <set>
#include <cstdlib>

#include "rt/cmd.h"
#include "ged/database.h"

extern "C" int
ged_attr_core(struct ged *gedp, int argc, const char *argv[])
{
    static const char *usage = "{[-c sep_char] set|get|show|rm|append|sort|list|copy} object [key [value] ... ]";
    const char *cmd_name = argv[0];

    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", cmd_name, usage);
	return BRLCAD_HELP;
    }

    /* Verify that this wdb supports lookup operations
       (non-null dbip) */
    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);

    /* The Tcl wdbp doesn't seem to work reliably for this purpose (the put
     * command has issues??) so temporarily switch the dbip->dbi_wdbp pointer
     * to the GED version. */
    struct rt_wdb *tmp_wdbp = gedp->dbip->dbi_wdbp;
    gedp->dbip->dbi_wdbp = gedp->ged_wdbp;

    int ret = rt_cmd_attr(gedp->ged_result_str, gedp->dbip, argc, argv);

    /* Restore default dbip->dbi_wdbp */
    gedp->dbip->dbi_wdbp = tmp_wdbp;

    if (ret & BRLCAD_HELP) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", cmd_name, usage);
    }

    if (ret & BRLCAD_ERROR) {
	return BRLCAD_ERROR;
    }

    return ret;
}



#ifdef GED_PLUGIN
#include "../include/plugin.h"
extern "C" {
    struct ged_cmd_impl attr_cmd_impl = { "attr", ged_attr_core, GED_CMD_DEFAULT };
    const struct ged_cmd attr_pcmd = { &attr_cmd_impl };
    const struct ged_cmd *attr_cmds[] = { &attr_pcmd,  NULL };

    static const struct ged_plugin pinfo = { GED_API,  attr_cmds, 1 };

    COMPILER_DLLEXPORT const struct ged_plugin *ged_plugin_info()
    {
	return &pinfo;
    }
}
#endif

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
