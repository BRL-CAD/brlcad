/*                 G E D _ C O M M A N D . C P P
 * BRL-CAD
 *
 * Copyright (c) 2014-2017 United States Government as represented by
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


int
ged_simulate(ged * const gedp, const int argc, const char ** const argv)
{
    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_READ_ONLY(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    bu_vls_sprintf(gedp->ged_result_str,
		   "%s: This build of BRL-CAD was not compiled with Bullet support", argv[0]);

    return GED_ERROR;
}


#else


#include "simulation.hpp"
#include "utility.hpp"

#include "ged.h"


int
ged_simulate(ged * const gedp, const int argc, const char ** const argv)
{
    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_READ_ONLY(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    if (argc != 3) {
	bu_vls_sprintf(gedp->ged_result_str, "%s: USAGE: %s <combination> <seconds>",
		       argv[0], argv[0]);
	return GED_ERROR;
    }

    rt_wdb * const orig_wdbp = gedp->ged_wdbp->dbip->dbi_wdbp;
    gedp->ged_wdbp->dbip->dbi_wdbp = gedp->ged_wdbp;

    try {
	const fastf_t seconds = simulate::lexical_cast<fastf_t>(argv[2],
				"invalid value for 'seconds'");

	if (seconds < 0.0)
	    throw simulate::InvalidSimulationError("invalid value for 'seconds'");

	simulate::Simulation(*gedp->ged_wdbp->dbip, argv[1]).step(seconds);
    } catch (const simulate::InvalidSimulationError &exception) {
	bu_vls_sprintf(gedp->ged_result_str, "%s: %s", argv[0], exception.what());
	gedp->ged_wdbp->dbip->dbi_wdbp = orig_wdbp;
	return GED_ERROR;
    }

    gedp->ged_wdbp->dbip->dbi_wdbp = orig_wdbp;
    return GED_OK;
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
