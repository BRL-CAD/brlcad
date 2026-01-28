/*                       A T T R . C P P
 * BRL-CAD
 *
 * Copyright (c) 2008-2025 United States Government as represented by
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
    static const char *usage = "{[-c sep_char] set|get|show|rm|append|sort|list|copy|standardize} object [key [value] ... ]";
    const char *cmd_name = argv[0];

    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", cmd_name, usage);
	return GED_HELP;
    }

    /* Verify that this wdb supports lookup operations
       (non-null dbip) */
    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);

    int ret = rt_cmd_attr(gedp->ged_result_str, gedp->dbip, argc, argv);

    if (ret & GED_HELP) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", cmd_name, usage);
    }

    if (ret & BRLCAD_ERROR) {
	return BRLCAD_ERROR;
    }

    return ret;
}

#include "../include/plugin.h"

struct ged_cmd_impl attr_impl = {"attr", ged_attr_core, GED_CMD_DEFAULT};
REGISTER_GED_COMMAND(attr);

#ifdef GED_PLUGIN
extern "C" {
    static bu_plugin_cmd pcommands[] = {
	{ "attr",            ged_attr_core }
    };
    static bu_plugin_manifest pinfo = {
	"libged_attr",
	1,
	(unsigned int)(sizeof(pcommands)/sizeof(pcommands[0])),
	pcommands,
	BU_PLUGIN_ABI_VERSION,
	sizeof(bu_plugin_manifest)
    };
    BU_PLUGIN_DECLARE_MANIFEST(pinfo)
}
#endif /* GED_PLUGIN */

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

