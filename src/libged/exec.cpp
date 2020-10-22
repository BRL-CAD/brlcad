/*                        E X E C . C P P
 * BRL-CAD
 *
 * Copyright (c) 2020 United States Government as represented by
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
/** @file exec.cpp
 *
 * Brief description
 *
 */

#include "common.h"

#include <map>
#include <string>

#include "bu/time.h"
#include "ged.h"
#include "./include/plugin.h"

extern "C" void libged_init(void);
extern std::map<std::string, const struct ged_cmd *>& ged_cmd_map(void); /* from ged_init.cpp */


extern "C" int
ged_exec(struct ged *gedp, int argc, const char *argv[])
{
    if (!gedp || !argc || !argv) {
	return GED_ERROR;
    }

    double start = 0.0;
    const char *tstr = getenv("GED_EXEC_TIME");
    if (tstr) {
	start = bu_gettime();
    }

    // TODO - right now this is the map from the libged load, which we
    // shouldn't be directly accessing.
    std::map<std::string, const struct ged_cmd *>& cmap = ged_cmd_map();

    std::string key(argv[0]);
    std::map<std::string, const struct ged_cmd *>::iterator c_it = cmap.find(key);
    if (c_it == cmap.end()) {
	bu_vls_printf(gedp->ged_result_str, "unknown command: %s", argv[0]);
	return (GED_ERROR | GED_UNKNOWN);
    }

    const struct ged_cmd *cmd = c_it->second;

    // TODO - if interactive command via cmd->i->interactive, don't
    // execute unless we have the necessary callbacks defined in gedp

    int cret = (*cmd->i->cmd)(gedp, argc, argv);

    if (tstr) {
	bu_log("%s time: %g\n", argv[0], (bu_gettime() - start)/1e6);
    }

    return cret;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
