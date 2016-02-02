/*                    S I M U L A T E . C P P
 * BRL-CAD
 *
 * Copyright (c) 2014-2016 United States Government as represented by
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
/** @file simulate.cpp
 *
 * Brief description
 *
 */


#ifdef HAVE_BULLET


#include "common.h"

#include "simulation.hpp"

#include <sstream>
#include <stdexcept>

#include "ged.h"


namespace
{


template<typename Target, typename Source>
Target lexical_cast(Source arg,
		    const std::string &description = "bad lexical_cast")
{
    std::stringstream interpreter;
    Target result;

    if (!(interpreter << arg) ||
	!(interpreter >> result) ||
	!(interpreter >> std::ws).eof())
	throw std::invalid_argument(description);

    return result;
}


}


int
ged_simulate(ged *gedp, int argc, const char **argv)
{
    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    if (argc != 3) {
	bu_vls_sprintf(gedp->ged_result_str, "%s: USAGE: %s <scene_comb> <seconds>",
		       argv[0], argv[0]);
	return GED_ERROR;
    }

    directory *dir = db_lookup(gedp->ged_wdbp->dbip, argv[1], LOOKUP_NOISY);

    if (!dir) return GED_ERROR;

    try {
	btScalar seconds = lexical_cast<btScalar>(argv[2],
			   "invalid value for 'seconds'");

	if (seconds < 0.0) throw std::runtime_error("invalid value for 'seconds'");

	simulate::Simulation simulation(*gedp->ged_wdbp->dbip, *dir);
	simulation.step(seconds);
    } catch (const std::logic_error &e) {
	bu_vls_sprintf(gedp->ged_result_str, "%s: %s", argv[0], e.what());
	return GED_ERROR;
    } catch (const std::runtime_error &e) {
	bu_vls_sprintf(gedp->ged_result_str, "%s: %s", argv[0], e.what());
	return GED_ERROR;
    }

    return GED_OK;
}


#else


#include "common.h"

#include "ged.h"


int
ged_simulate(ged *gedp, int argc, const char **argv)
{
    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    bu_vls_sprintf(gedp->ged_result_str,
		   "%s: This build of BRL-CAD was not compiled with Bullet support", argv[0]);

    return GED_ERROR;
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
