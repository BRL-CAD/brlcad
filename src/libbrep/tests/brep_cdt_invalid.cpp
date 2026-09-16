/*                 B R E P _ C D T _ I N V A L I D . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 */
/** @file brep_cdt_invalid.cpp
 *
 * Verify that watertight CDT rejects an invalid BRep before entering the
 * triangulation machinery.  Continuing after IsValid() fails can send
 * malformed topology into refinement loops that do not terminate.
 */

#include "common.h"

#include "bu/app.h"
#include "bu/log.h"
#include "brep.h"
#include "brep/cdt.h"


int
main(int argc, const char **argv)
{
    bu_setprogname(argv[0]);
    if (argc != 1)
	return 1;

    ON_Brep brep;
    ON_BrepVertex &vertex = brep.NewVertex(ON_3dPoint(0.0, 0.0, 0.0), 0.0);

    /* Deliberately violate the component-index invariant so IsValid() is
     * guaranteed to reject this otherwise minimal BRep. */
    vertex.m_vertex_index = 7;
    ON_wString messages;
    ON_TextLog log(messages);
    if (brep.IsValid(&log)) {
	bu_log("ERROR: malformed test BRep unexpectedly passed IsValid()\n");
	return 1;
    }

    struct ON_Brep_CDT_State *state = ON_Brep_CDT_Create(&brep, "invalid-test");
    if (!state) {
	bu_log("ERROR: failed to create CDT state\n");
	return 1;
    }

    int result = ON_Brep_CDT_Tessellate(state, 0, NULL);
    ON_Brep_CDT_Destroy(state);
    if (result != -1) {
	bu_log("ERROR: invalid BRep tessellation returned %d, expected -1\n", result);
	return 1;
    }

    return 0;
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
