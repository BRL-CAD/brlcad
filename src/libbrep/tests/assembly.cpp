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

#include "bu/app.h"
#include "brep.h"


namespace {

constexpr double ASSEMBLY_TOLERANCE = 1.0e-8;
constexpr double PARAMETER_ENDPOINT_SHIFT = 0.125;
constexpr double PRESERVED_TOLERANCE_MARGIN = 1.0;
constexpr double TOLERANCE_COMPARISON_EPSILON = 1.0e-12;
constexpr double OVERSIZED_STITCH_TOLERANCE = 2.0;
constexpr double AMBIGUOUS_STITCH_TOLERANCE = 5.0e-3;
constexpr double FALSE_MATCH_OFFSET = 4.0e-3;

bool
test_curve_matching()
{
    /* A knot-span feature between every old fixed sample must not be
     * mistaken for a coincident boundary. */
    const double parameters[] = {0.0, 0.091, 0.14, 0.217, 0.353,
	0.5, 0.647, 0.783, 0.909, 1.0};
    constexpr int POINT_COUNT = sizeof(parameters) / sizeof(parameters[0]);
    ON_LineCurve line(ON_3dPoint(0, 0, 0), ON_3dPoint(1, 0, 0));
    line.SetDomain(0.0, 1.0);
    ON_NurbsCurve spike(3, false, 2, POINT_COUNT);
    for (int i = 0; i < POINT_COUNT; ++i) {
	spike.SetCV(i, ON_3dPoint(parameters[i], i == 2 ? 1.0 : 0.0, 0.0));
	spike.SetKnot(i, parameters[i]);
    }
    if (!spike.IsValid() || brep_curves_coincident(line, spike, ASSEMBLY_TOLERANCE))
	return false;

    ON_NurbsCurve reparameterized(3, false, 3, 3);
    reparameterized.SetCV(0, ON_3dPoint(0, 0, 0));
    reparameterized.SetCV(1, ON_3dPoint(0, 0, 0));
    reparameterized.SetCV(2, ON_3dPoint(1, 0, 0));
    reparameterized.SetKnot(0, 0.0);
    reparameterized.SetKnot(1, 0.0);
    reparameterized.SetKnot(2, 1.0);
    reparameterized.SetKnot(3, 1.0);
    bool reversed = true;
    if (!reparameterized.IsValid() ||
	!brep_curves_coincident(line, reparameterized, ASSEMBLY_TOLERANCE, &reversed) || reversed)
	return false;
    if (!reparameterized.Reverse() ||
	!brep_curves_coincident(line, reparameterized, ASSEMBLY_TOLERANCE, &reversed) || !reversed)
	return false;

    /* Equivalent rational curves need not share a knot vector, degree,
     * or parameter domain.  Their curvature must not require flattening
     * a large circle to the tiny stitching tolerance. */
    constexpr double CIRCLE_RADIUS = 1000.0;
    ON_NurbsCurve circle;
    if (!ON_Circle(ON_xy_plane, CIRCLE_RADIUS).GetNurbForm(circle) || !circle.SetDomain(0.0, 1.0))
	return false;
    ON_NurbsCurve refined(circle);
    if (!refined.InsertKnot(0.3125, 1) || !refined.IncreaseDegree(4) ||
	!refined.SetDomain(10.0, 19.0) ||
	!brep_curves_coincident(circle, refined, ASSEMBLY_TOLERANCE, &reversed) || reversed)
	return false;

    ON_NurbsCurve variable_speed(circle);
    if (!variable_speed.ChangeEndWeights(1.0, 4.0) ||
	!brep_curves_coincident(circle, variable_speed, ASSEMBLY_TOLERANCE))
	return false;
    if (!refined.Reverse() ||
	!brep_curves_coincident(circle, refined, ASSEMBLY_TOLERANCE, &reversed) || !reversed)
	return false;

    ON_NurbsCurve bowed(3, false, 3, 3);
    bowed.SetCV(0, ON_3dPoint(0, 0, 0));
    bowed.SetCV(1, ON_3dPoint(0.5, 1, 0));
    bowed.SetCV(2, ON_3dPoint(1, 0, 0));
    bowed.SetKnot(0, 0.0);
    bowed.SetKnot(1, 0.0);
    bowed.SetKnot(2, 1.0);
    bowed.SetKnot(3, 1.0);
    if (!bowed.IsValid() || brep_curves_coincident(line, bowed, ASSEMBLY_TOLERANCE))
	return false;
    /* The middle control point lies outside tolerance, but its quadratic
     * basis weight is at most one half: the entire curve is inside. */
    bowed.SetCV(1, ON_3dPoint(0.5, 1.5 * ASSEMBLY_TOLERANCE, 0));
    if (!brep_curves_coincident(line, bowed, ASSEMBLY_TOLERANCE))
	return false;
    return true;
}


bool
test_curve_ball_certificate()
{
    const ON_3dPoint center(0.0, 0.0, 0.0);
    ON_LineCurve line(ON_3dPoint(-0.5, 0.0, 0.0),
	ON_3dPoint(0.5, 0.0, 0.0));
    if (!brep_curve_locus_within_ball(line, center, 0.5) ||
	brep_curve_locus_within_ball(line, center, 0.49))
    {
	return false;
    }

    /* The original control hull is outside the accepted ball, while the
     * complete quadratic locus is inside it.  This exercises proof by
     * subdivision rather than endpoint sampling. */
    ON_NurbsCurve bowed(3, false, 3, 3);
    if (!bowed.SetCV(0, ON_3dPoint(0.0, 0.0, 0.0)) ||
	!bowed.SetCV(1, ON_3dPoint(0.0, 2.0, 0.0)) ||
	!bowed.SetCV(2, ON_3dPoint(0.0, 0.0, 0.0)) ||
	!bowed.SetKnot(0, 0.0) || !bowed.SetKnot(1, 0.0) ||
	!bowed.SetKnot(2, 1.0) || !bowed.SetKnot(3, 1.0) ||
	!bowed.IsValid() ||
	!brep_curve_locus_within_ball(bowed, center, 1.01) ||
	brep_curve_locus_within_ball(bowed, center, 0.99))
    {
	return false;
    }
    return true;
}


bool
test_stitch_scale_guard()
{
    const ON_3dPoint corners[8] = {
	ON_3dPoint(0.0, 0.0, 0.0), ON_3dPoint(1.0, 0.0, 0.0),
	ON_3dPoint(1.0, 1.0, 0.0), ON_3dPoint(0.0, 1.0, 0.0),
	ON_3dPoint(0.0, 0.0, 1.0), ON_3dPoint(1.0, 0.0, 1.0),
	ON_3dPoint(1.0, 1.0, 1.0), ON_3dPoint(0.0, 1.0, 1.0)
    };
    std::unique_ptr<ON_Brep> sheet(ON_BrepBox(corners));
    if (!sheet)
	return false;
    for (int face = sheet->m_F.Count() - 1; face > 0; --face)
	sheet->DeleteFace(sheet->m_F[face], true);
    sheet->Compact();
    if (!sheet->IsValid() || sheet->m_F.Count() != 1 ||
	sheet->m_E.Count() != 4)
    {
	return false;
    }

    brep_assembly_result result;
    const int merged = brep_stitch_naked_edges(*sheet,
	OVERSIZED_STITCH_TOLERANCE, &result);
    return merged == 0 && result.input_naked_edges == 4 &&
	result.remaining_naked_edges == 4 && result.ambiguous_edges == 0 &&
	sheet->IsValid();
}


bool
append_plane_face(ON_Brep &assembly, const ON_Plane &plane,
    double u_extent = 1.0, double v_extent = 1.0)
{
    ON_PlaneSurface surface(plane);

    if (!(u_extent > 0.0) || !(v_extent > 0.0) ||
	!std::isfinite(u_extent) || !std::isfinite(v_extent) ||
	!surface.SetDomain(0, 0.0, u_extent) ||
	!surface.SetDomain(1, 0.0, v_extent) ||
	!surface.SetExtents(0, surface.Domain(0)) ||
	!surface.SetExtents(1, surface.Domain(1)))
    {
	return false;
    }
    ON_Brep face;
    if (face.NewFace(surface) == NULL || !face.IsValid())
	return false;
    assembly.Append(face);
    return true;
}


bool
test_stitch_relative_limit_roundoff()
{
    constexpr double EDGE_LENGTH = 1.0e-2;
    constexpr double RELATIVE_LIMIT = 1.0e-4;
    const double roundoff_offset = RELATIVE_LIMIT +
	0.25 * ON_ZERO_TOLERANCE;
    ON_Brep assembly;
    const ON_Plane horizontal(ON_3dPoint::Origin,
	ON_3dVector::XAxis, ON_3dVector::YAxis);
    const ON_Plane vertical(ON_3dPoint(0.0, roundoff_offset, 0.0),
	ON_3dVector::XAxis, ON_3dVector::ZAxis);
    if (!append_plane_face(assembly, horizontal, EDGE_LENGTH, 1.0) ||
	!append_plane_face(assembly, vertical, EDGE_LENGTH, 1.0) ||
	!assembly.IsValid() || assembly.m_F.Count() != 2 ||
	assembly.m_E.Count() != 8)
    {
	return false;
    }

    /* The two short south edges are separated by the mathematical relative
     * limit plus only floating-point roundoff.  The scale guard must remain
     * inclusive instead of classifying this exact-boundary seam as a distinct
     * feature. */
    brep_assembly_result result;
    const int merged = brep_stitch_naked_edges(assembly,
	2.0 * RELATIVE_LIMIT, &result);
    const bool valid = brep_set_edge_endpoint_tolerances(assembly,
	ASSEMBLY_TOLERANCE) && assembly.IsValid();
    const bool passed = merged == 1 && result.input_naked_edges == 8 &&
	result.remaining_naked_edges == 6 && result.ambiguous_edges == 0 &&
	result.nonmanifold_edges == 0 && assembly.m_E.Count() == 7 &&
	valid;
    if (!passed) {
	bu_log("relative-limit stitch failed: merged=%d input=%d remaining=%d "
	    "ambiguous=%d nonmanifold=%d edges=%d valid=%d error=%d\n",
	    merged, result.input_naked_edges, result.remaining_naked_edges,
	    result.ambiguous_edges, result.nonmanifold_edges,
	    assembly.m_E.Count(), valid,
	    static_cast<int>(result.error));
    }
    return passed;
}


bool
test_stitch_uniquely_superior_match()
{
    ON_Brep assembly;
    const ON_Plane positive_y(ON_3dPoint::Origin,
	ON_3dVector::XAxis, ON_3dVector::YAxis);
    const ON_Plane negative_y(ON_3dPoint::Origin,
	ON_3dVector::XAxis, -ON_3dVector::YAxis);
    const ON_Plane nearby(ON_3dPoint(0.0, FALSE_MATCH_OFFSET, 0.0),
	ON_3dVector::XAxis, ON_3dVector::ZAxis);
    if (!append_plane_face(assembly, positive_y) ||
	!append_plane_face(assembly, negative_y) ||
	!append_plane_face(assembly, nearby) ||
	!assembly.IsValid() || assembly.m_F.Count() != 3 ||
	assembly.m_E.Count() != 12)
    {
	bu_log("uniquely-superior stitch setup failed: valid=%d faces=%d "
	    "edges=%d\n", assembly.IsValid(), assembly.m_F.Count(),
	    assembly.m_E.Count());
	return false;
    }

    /* All three south edges are candidates at the requested tolerance.  The
     * first two are exact duplicates, while the third is a nearby boundary
     * of a distinct face.  Only the exact mutual-best pair may be merged. */
    brep_assembly_result result;
    const int merged = brep_stitch_naked_edges(assembly,
	AMBIGUOUS_STITCH_TOLERANCE, &result);
    const bool valid = brep_set_edge_endpoint_tolerances(assembly,
	ASSEMBLY_TOLERANCE) && assembly.IsValid();
    const bool passed = merged == 1 && result.input_naked_edges == 12 &&
	result.remaining_naked_edges == 10 && result.ambiguous_edges == 1 &&
	result.nonmanifold_edges == 0 && valid;
    if (!passed) {
	ON_wString messages;
	ON_TextLog log(messages);
	assembly.IsValid(&log);
	ON_String validation(messages);
	bu_log("uniquely-superior stitch failed: merged=%d input=%d "
	    "remaining=%d ambiguous=%d nonmanifold=%d valid=%d error=%d:\n%s",
	    merged, result.input_naked_edges, result.remaining_naked_edges,
	    result.ambiguous_edges, result.nonmanifold_edges,
	    valid, static_cast<int>(result.error),
	    validation.Array());
    }
    return passed;
}

}


int
main(int UNUSED(argc), char **argv)
{
    bu_setprogname(argv[0]);
    if (!test_curve_matching())
	return 15;
    if (!test_curve_ball_certificate())
	return 17;
    if (!test_stitch_scale_guard())
	return 16;
    if (!test_stitch_relative_limit_roundoff())
	return 19;
    if (!test_stitch_uniquely_superior_match())
	return 18;
    const ON_3dPoint corners[8] = {
	ON_3dPoint(0.0, 0.0, 0.0), ON_3dPoint(1.0, 0.0, 0.0),
	ON_3dPoint(1.0, 1.0, 0.0), ON_3dPoint(0.0, 1.0, 0.0),
	ON_3dPoint(0.0, 0.0, 1.0), ON_3dPoint(1.0, 0.0, 1.0),
	ON_3dPoint(1.0, 1.0, 1.0), ON_3dPoint(0.0, 1.0, 1.0)
    };
    std::unique_ptr<ON_Brep> brep(ON_BrepBox(corners));

    if (!brep || !brep->IsSolid() || brep->m_E.Count() != 12) {
	bu_log("box assembly setup failed: brep=%d solid=%d edges=%d\n",
	    brep ? 1 : 0, brep ? brep->IsSolid() : 0,
	    brep ? brep->m_E.Count() : -1);
	return 1;
    }

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
