/*            A P 2 1 4 _ B R E P _ T R A N S A C T I O N _ T E S T . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 */

#include "common.h"

#include <iostream>

#include "bu/app.h"
#include "raytrace.h"

#include "STEPBrepValidation.h"

namespace {

int failures = 0;

void
expect(bool condition, const char *message)
{
    if (condition) return;
    std::cerr << "FAIL: " << message << '\n';
    ++failures;
}

ON_Brep
one_trim_face()
{
    ON_Brep brep;
    ON_PlaneSurface *surface = new ON_PlaneSurface(ON_xy_plane);
    surface->SetDomain(0, 0.0, 1.0);
    surface->SetDomain(1, -1.0, 1.0);
    surface->SetExtents(0, surface->Domain(0));
    surface->SetExtents(1, surface->Domain(1));
    const int si = brep.AddSurface(surface);

    const int start_index = brep.NewVertex(
	ON_3dPoint(0.0, 0.0, 0.0)).m_vertex_index;
    const int end_index = brep.NewVertex(
	ON_3dPoint(1.0, 0.0, 0.0)).m_vertex_index;
    ON_Curve *edge_curve = new ON_LineCurve(
	brep.m_V[start_index].Point(), brep.m_V[end_index].Point());
    edge_curve->SetDomain(0.0, 1.0);
    const int c3i = brep.AddEdgeCurve(edge_curve);
    ON_BrepEdge &edge = brep.NewEdge(
	brep.m_V[start_index], brep.m_V[end_index], c3i);
    edge.m_tolerance = 5.13e-5;

    ON_BrepFace &face = brep.NewFace(si);
    ON_BrepLoop &loop = brep.NewLoop(ON_BrepLoop::outer, face);
    ON_Curve *trim_curve = new ON_LineCurve(
	ON_2dPoint(0.0, 0.0), ON_2dPoint(1.0, 0.0));
    trim_curve->SetDomain(0.0, 1.0);
    const int c2i = brep.AddTrimCurve(trim_curve);
    ON_BrepTrim &trim = brep.NewTrim(edge, false, loop, c2i);
    trim.m_type = ON_BrepTrim::boundary;
    trim.m_tolerance[0] = 5.13e-5;
    trim.m_tolerance[1] = 5.13e-5;
    return brep;
}

ON_Brep
three_face_orientation_cycle(bool contradictory)
{
    ON_Brep brep;
    int loops[3];
    for (int fi = 0; fi < 3; ++fi) {
	ON_PlaneSurface *surface = new ON_PlaneSurface(ON_xy_plane);
	surface->SetDomain(0, 0.0, 1.0);
	surface->SetDomain(1, 0.0, 1.0);
	surface->SetExtents(0, surface->Domain(0));
	surface->SetExtents(1, surface->Domain(1));
	const int si = brep.AddSurface(surface);
	const int face_index = brep.NewFace(si).m_face_index;
	loops[fi] = brep.NewLoop(ON_BrepLoop::outer,
	    brep.m_F[face_index]).m_loop_index;
    }

    for (int ei = 0; ei < 3; ++ei) {
	const int vi0 = brep.NewVertex(
	    ON_3dPoint(0.0, static_cast<double>(ei), 0.0)).m_vertex_index;
	const int vi1 = brep.NewVertex(
	    ON_3dPoint(1.0, static_cast<double>(ei), 0.0)).m_vertex_index;
	ON_Curve *edge_curve = new ON_LineCurve(
	    brep.m_V[vi0].Point(), brep.m_V[vi1].Point());
	edge_curve->SetDomain(0.0, 1.0);
	const int c3i = brep.AddEdgeCurve(edge_curve);
	const int edge_index = brep.NewEdge(
	    brep.m_V[vi0], brep.m_V[vi1], c3i).m_edge_index;

	for (int use = 0; use < 2; ++use) {
	    ON_Curve *trim_curve = new ON_LineCurve(
		ON_2dPoint(0.0, 0.0), ON_2dPoint(1.0, 0.0));
	    trim_curve->SetDomain(0.0, 1.0);
	    const int c2i = brep.AddTrimCurve(trim_curve);
	    const bool reverse = ei == 2 && use == 1 && !contradictory;
	    ON_BrepTrim &trim = brep.NewTrim(brep.m_E[edge_index],
		reverse, brep.m_L[loops[(ei + use) % 3]], c2i);
	    trim.m_type = ON_BrepTrim::mated;
	}
    }
    return brep;
}

ON_Brep
same_face_reciprocal_loops(bool same_loop)
{
    ON_Brep brep;
    ON_PlaneSurface *surface = new ON_PlaneSurface(ON_xy_plane);
    surface->SetDomain(0, 0.0, 1.0);
    surface->SetDomain(1, 0.0, 1.0);
    surface->SetExtents(0, surface->Domain(0));
    surface->SetExtents(1, surface->Domain(1));
    const int si = brep.AddSurface(surface);
    ON_BrepFace &face = brep.NewFace(si);
    const int first_loop =
	brep.NewLoop(ON_BrepLoop::outer, face).m_loop_index;
    const int second_loop = same_loop ? first_loop :
	brep.NewLoop(ON_BrepLoop::inner, face).m_loop_index;

    const int vi0 =
	brep.NewVertex(ON_3dPoint(0.0, 0.0, 0.0)).m_vertex_index;
    const int vi1 =
	brep.NewVertex(ON_3dPoint(1.0, 0.0, 0.0)).m_vertex_index;
    ON_Curve *edge_curve = new ON_LineCurve(
	brep.m_V[vi0].Point(), brep.m_V[vi1].Point());
    edge_curve->SetDomain(0.0, 1.0);
    const int c3i = brep.AddEdgeCurve(edge_curve);
    ON_BrepEdge &edge = brep.NewEdge(
	brep.m_V[vi0], brep.m_V[vi1], c3i);
    edge.m_edge_user.i = 650;
    for (int use = 0; use < 2; ++use) {
	ON_Curve *trim_curve = new ON_LineCurve(
	    ON_2dPoint(0.0, 0.0), ON_2dPoint(1.0, 0.0));
	trim_curve->SetDomain(0.0, 1.0);
	const int c2i = brep.AddTrimCurve(trim_curve);
	ON_BrepTrim &trim = brep.NewTrim(edge, false,
	    brep.m_L[use ? second_loop : first_loop], c2i);
	trim.m_type = ON_BrepTrim::mated;
    }
    return brep;
}

ON_Brep
duplicate_boundary_pair(bool same_face)
{
    ON_Brep brep;
    int loops[2];
    for (int use = 0; use < 2; ++use) {
	if (use && same_face) {
	    loops[use] = brep.NewLoop(ON_BrepLoop::inner,
		brep.m_F[0]).m_loop_index;
	    continue;
	}
	ON_PlaneSurface *surface = new ON_PlaneSurface(ON_xy_plane);
	surface->SetDomain(0, 0.0, 1.0);
	surface->SetDomain(1, 0.0, 1.0);
	surface->SetExtents(0, surface->Domain(0));
	surface->SetExtents(1, surface->Domain(1));
	const int si = brep.AddSurface(surface);
	ON_BrepFace &face = brep.NewFace(si);
	loops[use] = brep.NewLoop(ON_BrepLoop::outer,
	    face).m_loop_index;
    }

    for (int use = 0; use < 2; ++use) {
	const int vi0 = brep.NewVertex(
	    ON_3dPoint(0.0, 0.0, 0.0)).m_vertex_index;
	const int vi1 = brep.NewVertex(
	    ON_3dPoint(1.0, 0.0, 0.0)).m_vertex_index;
	ON_Curve *edge_curve = new ON_LineCurve(
	    brep.m_V[vi0].Point(), brep.m_V[vi1].Point());
	edge_curve->SetDomain(0.0, 1.0);
	const int c3i = brep.AddEdgeCurve(edge_curve);
	ON_BrepEdge &edge = brep.NewEdge(
	    brep.m_V[vi0], brep.m_V[vi1], c3i);
	edge.m_edge_user.i = 650;
	ON_Curve *trim_curve = new ON_LineCurve(
	    ON_2dPoint(0.0, 0.0), ON_2dPoint(1.0, 0.0));
	trim_curve->SetDomain(0.0, 1.0);
	const int c2i = brep.AddTrimCurve(trim_curve);
	ON_BrepTrim &trim = brep.NewTrim(edge, false,
	    brep.m_L[loops[use]], c2i);
	trim.m_type = ON_BrepTrim::boundary;
    }
    return brep;
}

ON_Brep
periodic_pole_topology(bool paired_artificial_seam)
{
    ON_Brep brep = same_face_reciprocal_loops(true);
    brep.m_E[0].m_edge_user.i = paired_artificial_seam ? 0 : 650;
    ON_Curve *singular_curve = new ON_LineCurve(
	ON_2dPoint(0.0, 1.0), ON_2dPoint(1.0, 1.0));
    singular_curve->SetDomain(0.0, 1.0);
    const int c2i = brep.AddTrimCurve(singular_curve);
    brep.NewSingularTrim(brep.m_V[0], brep.m_L[0],
	ON_Surface::S_iso, c2i);
    return brep;
}

} // namespace

int
main(int, char **argv)
{
    bu_setprogname(argv[0]);
    const double model_tolerance = 1.0e-6;
    ON_Brep before = one_trim_face();
    if (brlcad::step::DirectedTrimEndpointRatio(before.m_T[0],
	    model_tolerance) > 1.0) {
	const ON_3dPoint uv0 = before.m_T[0].PointAtStart();
	const ON_3dPoint uv1 = before.m_T[0].PointAtEnd();
	const ON_3dPoint lift0 = before.m_F[0].SurfaceOf()->PointAt(uv0.x, uv0.y);
	const ON_3dPoint lift1 = before.m_F[0].SurfaceOf()->PointAt(uv1.x, uv1.y);
	const ON_Interval domain = before.m_E[0].Domain();
	const ON_3dPoint edge0 = before.m_E[0].PointAt(domain[0]);
	const ON_3dPoint edge1 = before.m_E[0].PointAt(domain[1]);
	std::cerr << "fixture endpoint lifts/edges: "
	    << lift0.x << ',' << lift0.y << ',' << lift0.z << " / "
	    << edge0.x << ',' << edge0.y << ',' << edge0.z << " ; "
	    << lift1.x << ',' << lift1.y << ',' << lift1.z << " / "
	    << edge1.x << ',' << edge1.y << ',' << edge1.z << '\n';
    }
    ON_Brep unchanged(before);
    expect(!brlcad::step::FaceTrimValidationRegressed(before, unchanged, 0,
	    model_tolerance),
	"an unchanged exact trim is accepted");

    ON_Brep displaced(before);
    ON_Curve *bad_curve = new ON_LineCurve(
	ON_2dPoint(0.0, 0.0), ON_2dPoint(1.0, 0.171));
    bad_curve->SetDomain(0.0, 1.0);
    const int bad_c2i = displaced.AddTrimCurve(bad_curve);
    expect(displaced.SetTrimCurve(displaced.m_T[0], bad_c2i),
	"the representative displaced pcurve was installed");
    const bool displaced_regressed =
	brlcad::step::FaceTrimValidationRegressed(before, displaced, 0,
	    model_tolerance);
    if (!displaced_regressed)
	std::cerr << "directed endpoint ratios before/after: "
	    << brlcad::step::DirectedTrimEndpointRatio(before.m_T[0],
		model_tolerance) << '/'
	    << brlcad::step::DirectedTrimEndpointRatio(displaced.m_T[0],
		model_tolerance) << '\n';
    expect(displaced_regressed,
	"a Panzer-style directed endpoint displacement rejects the transaction");

    ON_Brep within_tolerance(before);
    ON_Curve *noise_curve = new ON_LineCurve(
	ON_2dPoint(0.0, 0.0), ON_2dPoint(1.0, 1.0e-6));
    noise_curve->SetDomain(0.0, 1.0);
    const int noise_c2i = within_tolerance.AddTrimCurve(noise_curve);
    expect(within_tolerance.SetTrimCurve(within_tolerance.m_T[0], noise_c2i),
	"the within-tolerance pcurve was installed");
    expect(!brlcad::step::FaceTrimValidationRegressed(before,
	    within_tolerance, 0, model_tolerance),
	"a displacement inside the trim tolerance remains accepted");

    const ON_Brep consistent = three_face_orientation_cycle(false);
    expect(brlcad::step::FaceOrientationConstraintsAreConsistent(consistent),
	"a solvable three-face orientation cycle is accepted");
    expect(brlcad::step::FaceOrientationConflictCount(consistent) == 2,
	"the representative unresolved shell begins with two local conflicts");
    const ON_Brep contradictory = three_face_orientation_cycle(true);
    expect(!brlcad::step::FaceOrientationConstraintsAreConsistent(
	    contradictory),
	"an odd contradictory face-orientation cycle is rejected");
    expect(brlcad::step::FaceOrientationConflictCount(contradictory) == 3,
	"the XS650-style wrong closure exposes a measurable 2-to-3 regression");

    std::vector<int> loop_flip;
    std::string loop_failure;
    const ON_Brep same_face = same_face_reciprocal_loops(false);
    expect(brlcad::step::LoopOrientationFlipPlan(same_face, loop_flip,
	    &loop_failure),
	"reciprocal loops on one face participate in the orientation plan");
    expect(loop_flip.size() == 2 && loop_flip[0] != loop_flip[1],
	"the XS650-style same-face pair schedules exactly one loop reversal");
    const ON_Brep impossible_same_loop = same_face_reciprocal_loops(true);
    expect(!brlcad::step::LoopOrientationFlipPlan(impossible_same_loop,
	    loop_flip, &loop_failure),
	"two agreeing reciprocal uses in one loop are reported as contradictory");

    const ON_Brep same_face_duplicates = duplicate_boundary_pair(true);
    expect(!brlcad::step::DuplicateBoundaryEdgeUsesAreOnDistinctFaces(
	    same_face_duplicates.m_E[0], same_face_duplicates.m_E[1]),
	"XS650-style same-face singularity children cannot be merged as "
	"reciprocal edges");
    ON_Brep adjacent_face_duplicates = duplicate_boundary_pair(false);
    expect(brlcad::step::DuplicateBoundaryEdgeUsesAreOnDistinctFaces(
	    adjacent_face_duplicates.m_E[0], adjacent_face_duplicates.m_E[1]),
	"duplicate fragments on adjacent faces remain eligible for exact "
	"locus validation");
    adjacent_face_duplicates.m_V[0].m_vertex_user.i = 1129660;
    adjacent_face_duplicates.m_V[2].m_vertex_user.i = 1129662;
    adjacent_face_duplicates.m_V[2].point.x = 2.2e-10;
    adjacent_face_duplicates.m_V[0].m_tolerance = model_tolerance;
    adjacent_face_duplicates.m_V[2].m_tolerance = model_tolerance;
    expect(brlcad::step::DuplicateStepEdgeEndpointsMatch(
	    adjacent_face_duplicates.m_V[0],
	    adjacent_face_duplicates.m_V[2], model_tolerance,
	    0.175, true),
	"a collapsed sub-tolerance STEP edge can leave equivalent positive "
	"endpoint identities on two uses of one source edge");
    expect(!brlcad::step::DuplicateStepEdgeEndpointsMatch(
	    adjacent_face_duplicates.m_V[0],
	    adjacent_face_duplicates.m_V[2], model_tolerance,
	    0.175, false),
	"distinct source edges cannot infer endpoint identity from proximity");
    adjacent_face_duplicates.m_V[2].point.x = 2.0 * model_tolerance;
    expect(!brlcad::step::DuplicateStepEdgeEndpointsMatch(
	    adjacent_face_duplicates.m_V[0],
	    adjacent_face_duplicates.m_V[2], model_tolerance,
	    0.175, true),
	"a loose measured edge tolerance cannot merge distinct positive STEP "
	"vertex identities outside the declared model uncertainty");
    adjacent_face_duplicates.m_V[2].point.x = 0.0;
    adjacent_face_duplicates.m_V[2].m_vertex_user.i = 0;
    expect(!brlcad::step::DuplicateStepEdgeEndpointsMatch(
	    adjacent_face_duplicates.m_V[0],
	    adjacent_face_duplicates.m_V[2], model_tolerance,
	    model_tolerance, true),
	"an authoritative STEP endpoint cannot merge with an anonymous split "
	"vertex based only on proximity");

    const ON_Brep partial_pole = periodic_pole_topology(false);
    expect(!brlcad::step::LoopHasCompletePeriodicPoleTopology(
	    partial_pole.m_L[0]),
	"a singular trim alone does not suppress remaining cap reconstruction");
    const ON_Brep complete_pole = periodic_pole_topology(true);
    expect(brlcad::step::LoopHasCompletePeriodicPoleTopology(
	    complete_pole.m_L[0]),
	"a singular trim plus its paired artificial seam is complete pole topology");

    ON_Brep anonymous_child = one_trim_face();
    anonymous_child.m_E[0].m_edge_user.i = 650;
    anonymous_child.m_V[0].m_vertex_user.i = 123;
    expect(brlcad::step::ImporterSplitEdgeIsToleranceDegenerate(
	    anonymous_child.m_E[0], 2.0),
	"an anonymous importer split wholly inside tolerance is removable");
    expect(!brlcad::step::ImporterSplitEdgeIsToleranceDegenerate(
	    anonymous_child.m_E[0], 0.5),
	"an importer split extending beyond tolerance is retained");
    anonymous_child.m_V[1].m_vertex_user.i = 456;
    expect(!brlcad::step::ImporterSplitEdgeIsToleranceDegenerate(
	    anonymous_child.m_E[0], 2.0),
	"an authoritative STEP vertex pair is retained at any repair tolerance");

    if (failures)
	std::cerr << failures << " BREP transaction validation test(s) failed\n";
    return failures ? 1 : 0;
}

/*
 * Local Variables:
 * tab-width: 8
 * mode: C++
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 */
