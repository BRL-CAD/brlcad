/*                 G E D _ C O M M A N D . C P P
 * BRL-CAD
 *
 * Copyright (c) 2014-2022 United States Government as represented by
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
/** @file ged_command.cpp
 *
 * Simulate GED command.
 *
 */


#include "common.h"


#ifndef HAVE_BULLET


#include "ged.h"


extern "C" int
ged_simulate_core(ged * const gedp, const int argc, const char ** const argv)
{
    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_READ_ONLY(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    bu_vls_sprintf(gedp->ged_result_str,
		   "%s: This build of BRL-CAD was not compiled with Bullet support", argv[0]);

    return BRLCAD_ERROR;
}


#else


#include "simulation.hpp"
#include "utility.hpp"

#include "bu/opt.h"
#include "ged.h"


namespace
{


HIDDEN simulate::Simulation::DebugMode
get_debug_mode(const std::string &debug_mode_string)
{
    simulate::Simulation::DebugMode result = simulate::Simulation::debug_none;

    if (debug_mode_string.empty())
	return result;

    std::istringstream stream(debug_mode_string);

    while (!stream.eof()) {
	std::string value;
	std::getline(stream, value, ',');

	if (value == "aabb")
	    result = result | simulate::Simulation::debug_aabb;
	else if (value == "contact")
	    result = result | simulate::Simulation::debug_contact;
	else if (value == "ray")
	    result = result | simulate::Simulation::debug_ray;
	else
	    throw simulate::InvalidSimulationError("invalid debug mode");
    }

    return result;
}


}


extern "C" int
ged_simulate_core(ged * const gedp, const int argc, const char ** const argv)
{
    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_READ_ONLY(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    const char *debug_mode_string = "";
    const bu_opt_desc options_description[] = {
	{NULL, "debug", "mode", bu_opt_str, &debug_mode_string, "set debug mode (example: --debug=aabb,contact,ray)"},
	BU_OPT_DESC_NULL
    };

    if (2 != bu_opt_parse(gedp->ged_result_str, argc - 1, &argv[1],
			  options_description)) {
	const simulate::AutoPtr<char> usage(const_cast<char *>(bu_opt_describe(
						const_cast<bu_opt_desc *>(options_description), NULL)));
	bu_vls_printf(gedp->ged_result_str,
		      "USAGE: %s [OPTIONS] path duration\nOptions:\n%s\n", argv[0], usage.ptr);
	return BRLCAD_ERROR;
    }

    rt_wdb * const orig_wdbp = gedp->dbip->dbi_wdbp;
    gedp->dbip->dbi_wdbp = gedp->ged_wdbp;

    try {
	const simulate::Simulation::DebugMode debug_mode =
	    get_debug_mode(debug_mode_string);
	const fastf_t seconds = simulate::lexical_cast<fastf_t>(argv[2],
				"invalid value for 'seconds'");

	if (seconds < 0.0)
	    throw simulate::InvalidSimulationError("invalid value for 'seconds'");

	db_full_path path;
	const simulate::AutoPtr<db_full_path, db_free_full_path> autofree_path(&path);
	db_full_path_init(&path);

	if (db_string_to_path(&path, gedp->dbip, argv[1]))
	    throw simulate::InvalidSimulationError("invalid path");

	simulate::Simulation simulation(*gedp->dbip, path);
	simulation.step(seconds, debug_mode);
    } catch (const simulate::InvalidSimulationError &exception) {
	bu_vls_sprintf(gedp->ged_result_str, "%s", exception.what());
	gedp->dbip->dbi_wdbp = orig_wdbp;
	return BRLCAD_ERROR;
    }

    gedp->dbip->dbi_wdbp = orig_wdbp;
    return BRLCAD_OK;
}

#endif

#ifdef GED_PLUGIN
#include "../include/plugin.h"
extern "C" {
    struct ged_cmd_impl simulate_cmd_impl = { "simulate", ged_simulate_core, GED_CMD_DEFAULT };
    const struct ged_cmd simulate_cmd = { &simulate_cmd_impl };
    const struct ged_cmd *simulate_cmds[] = { &simulate_cmd,  NULL };

    static const struct ged_plugin pinfo = { GED_API,  simulate_cmds, 1 };

    COMPILER_DLLEXPORT const struct ged_plugin *ged_plugin_info()
    {
	return &pinfo;
    }
}
#endif


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

