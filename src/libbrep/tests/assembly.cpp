/*                    A S S E M B L Y . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "common.h"

#include <algorithm>
#include <cmath>
#include <memory>

#include "brep.h"


namespace {

constexpr double ASSEMBLY_TOLERANCE = 1.0e-8;
constexpr double PARAMETER_ENDPOINT_SHIFT = 0.125;
constexpr double PRESERVED_TOLERANCE_MARGIN = 1.0;
constexpr double TOLERANCE_COMPARISON_EPSILON = 1.0e-12;

}


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
    if (!brep_assemble(*brep, ASSEMBLY_TOLERANCE, &result))
	return 4;
    if (result.input_naked_edges != 24 || result.merged_edges != 12 ||
	result.remaining_naked_edges != 0 || result.ambiguous_edges != 0 ||
	!result.oriented || !result.valid || !result.solid)
	return 5;
    if (brep->m_E.Count() != 12 || brep->m_V.Count() != 8 ||
	brep->m_F.Count() != 6 || brep->m_T.Count() != 24)
	return 6;
    ON_BrepEdge &edge = brep->m_E[0];
    if (edge.m_ti.Count() < 1)
	return 7;
    const int trim_index = edge.m_ti[0];
    ON_BrepTrim &trim = brep->m_T[trim_index];
    if (trim.m_c2i < 0 || trim.m_c2i >= brep->m_C2.Count())
	return 8;
    ON_Curve *parameter_curve = brep->m_C2[trim.m_c2i];
    if (!parameter_curve)
	return 9;
    const ON_3dPoint parameter_start = parameter_curve->PointAtStart();
    if (!parameter_curve->SetStartPoint(parameter_start +
	    ON_3dVector(PARAMETER_ENDPOINT_SHIFT, 0.0, 0.0)))
	return 10;
    trim.DestroyPspaceInformation();

    ON_3dPoint trim_start;
    ON_3dPoint trim_end;
    if (!brep->GetTrim3dStart(trim_index, trim_start) ||
	    !brep->GetTrim3dEnd(trim_index, trim_end))
	return 11;
    const ON_3dPoint expected_start = trim.m_bRev3d ?
	edge.PointAtEnd() : edge.PointAtStart();
    const ON_3dPoint expected_end = trim.m_bRev3d ?
	edge.PointAtStart() : edge.PointAtEnd();
    const double required_tolerance = std::max(
	trim_start.DistanceTo(expected_start),
	trim_end.DistanceTo(expected_end));
    if (!std::isfinite(required_tolerance) ||
	    required_tolerance <= ASSEMBLY_TOLERANCE)
	return 12;

    edge.m_tolerance = ASSEMBLY_TOLERANCE;
    if (!brep_set_edge_endpoint_tolerances(*brep, ASSEMBLY_TOLERANCE) ||
	    edge.m_tolerance < required_tolerance)
	return 13;

    const double preserved_tolerance = required_tolerance +
	PRESERVED_TOLERANCE_MARGIN;
    edge.m_tolerance = preserved_tolerance;
    if (!brep_set_edge_endpoint_tolerances(*brep, ASSEMBLY_TOLERANCE) ||
	    std::fabs(edge.m_tolerance - preserved_tolerance) >
	    TOLERANCE_COMPARISON_EPSILON)
	return 14;
    return 0;
}
