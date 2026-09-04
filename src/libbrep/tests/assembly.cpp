/*                    A S S E M B L Y . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "common.h"

#include <memory>

#include "brep.h"


int
main()
{
    const ON_3dPoint corners[8] = {
	ON_3dPoint(0.0, 0.0, 0.0), ON_3dPoint(1.0, 0.0, 0.0),
	ON_3dPoint(1.0, 1.0, 0.0), ON_3dPoint(0.0, 1.0, 0.0),
	ON_3dPoint(0.0, 0.0, 1.0), ON_3dPoint(1.0, 0.0, 1.0),
	ON_3dPoint(1.0, 1.0, 1.0), ON_3dPoint(0.0, 1.0, 1.0)
    };
    std::unique_ptr<ON_Brep> brep(ON_BrepBox(corners));
    if (!brep || !brep->IsSolid() || brep->m_E.Count() != 12)
	return 1;

    const int original_edge_count = brep->m_E.Count();
    for (int edge = 0; edge < original_edge_count; ++edge)
	if (!brep->DisconnectEdgeFaces(edge))
	    return 2;
    brep->CullUnusedEdges();
    brep->CullUnused3dCurves();
    if (brep->m_E.Count() != 24)
	return 3;

    brep_assembly_result result;
    if (!brep_assemble(*brep, 1.0e-8, &result))
	return 4;
    if (result.input_naked_edges != 24 || result.merged_edges != 12 ||
	result.remaining_naked_edges != 0 || result.ambiguous_edges != 0 ||
	!result.oriented || !result.valid || !result.solid)
	return 5;
    if (brep->m_E.Count() != 12 || brep->m_V.Count() != 8 ||
	brep->m_F.Count() != 6 || brep->m_T.Count() != 24)
	return 6;
    return 0;
}
