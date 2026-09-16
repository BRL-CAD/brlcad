/*                 OpenNurbsInterfaces.cpp
 * BRL-CAD
 *
 * Copyright (c) 1994-2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file step/OpenNurbsInterfaces.cpp
 *
 * Routines to convert STEP "OpenNurbsInterfaces" to BRL-CAD BREP
 * structures.
 *
 */

#include "common.h"

#include "brep/defines.h"
#include "brep/pullback.h"

#include "sdai.h"
class SDAI_Application_instance;

/* must come after nist step headers */
#include "brep.h"

#include <algorithm>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <vector>
#include "nmg.h"

#include "STEPEntity.h"
#include "Axis1Placement.h"
#include "Factory.h"
#include "LocalUnits.h"
#include "PullbackCurve.h"
#include "Point.h"
#include "CartesianPoint.h"
#include "VertexPoint.h"
#include "Vector.h"
#include "EdgeCurve.h"
#include "OrientedEdge.h"
#include "OpenNurbsInterfaces.h"

// Curve includes
#include "BezierCurve.h"
#include "BSplineCurve.h"
#include "BSplineCurveWithKnots.h"
#include "QuasiUniformCurve.h"
#include "RationalBezierCurve.h"
#include "RationalBSplineCurve.h"
#include "RationalBSplineCurveWithKnots.h"
#include "RationalQuasiUniformCurve.h"
#include "RationalUniformCurve.h"
#include "UniformCurve.h"

// Surface includes
#include "Line.h"
#include "Circle.h"
#include "Ellipse.h"
#include "Hyperbola.h"
#include "Parabola.h"
#include "CylindricalSurface.h"
#include "ConicalSurface.h"
#include "SweptSurface.h"
#include "SurfaceOfLinearExtrusion.h"
#include "SurfaceOfRevolution.h"
#include "Path.h"
#include "Plane.h"
#include "Loop.h"
#include "VertexLoop.h"
#include "Face.h"
#include "OpenShell.h"
#include "OrientedFace.h"
#include "FaceBound.h"
#include "FaceOuterBound.h"
#include "FaceSurface.h"
#include "BezierSurface.h"
#include "BSplineSurface.h"
#include "BSplineSurfaceWithKnots.h"
#include "QuasiUniformSurface.h"
#include "RationalBezierSurface.h"
#include "RationalBSplineSurface.h"
#include "RationalBSplineSurfaceWithKnots.h"
#include "RationalQuasiUniformSurface.h"
#include "RationalUniformSurface.h"
#include "SphericalSurface.h"
#include "ToroidalSurface.h"
#include "UniformSurface.h"

#include "AdvancedBrepShapeRepresentation.h"
#include "ShellBasedSurfaceModel.h"
#include "PullbackCurve.h"

#include "brep.h"

//#define _DEBUG_TESTING_
#ifdef _DEBUG_TESTING_
extern void print_pullback_data(std::string str, std::list<PBCData*> &pbcs, bool justendpoints);
#endif

namespace {

/* Dense validation is the final proof that a repaired or reused pcurve lifts
 * to its exact STEP edge within the file uncertainty.  1024 uniform segments
 * were selected as a bounded empirical budget: they resolve the short seam
 * reversals in the periodic-seam fixtures while keeping validation linear and
 * deterministic.  This is a validation budget, not a geometric tolerance or
 * a request to approximate the edge with 1024 segments. */
constexpr int kDenseLiftValidationSegments = 1024;

/* A triangular spline patch can express its apex by omitting the collapsed
 * parameter-boundary trim.  Require the two adjacent exact STEP edges to span
 * at least three quarters of that boundary before considering a missing-pole
 * repair.  Candidate surface edits receive a bounded 64-by-64 interior audit;
 * every retained edge is then regenerated and receives the ordinary
 * 1024-segment exact-locus validation. */
constexpr int kDegenerateBoundarySurfaceGridSegments = 64;
constexpr double kDegenerateBoundaryMinimumParameterSpanFraction = 0.75;
constexpr double kDegenerateBoundaryMinimumNormalAlignment = 0.5;

/* Linear span scans are cheaper for the small curves that dominate ordinary
 * models.  At 32 spans, an R-tree is shallow enough to amortize its build cost
 * while avoiding the quadratic validation behavior seen on imported curves
 * with thousands of spans.  This is solely a search-strategy threshold: every
 * candidate returned by the tree still undergoes the same exact curve tests. */
constexpr size_t kCurveDistanceRTreeMinimumSpans = 32;

/* A two-dimensional closest-point solve visits the tensor product of a
 * surface's spans.  For a non-linear edge which can be accepted here only as
 * an exact knot-boundary isocurve, that global solve is merely a candidate
 * filter: searching every knot boundary in one dimension is complete and is
 * cheaper for a surface with more than 32 spans in either direction or more
 * than 64 tensor-product patches.  The threshold changes only the search
 * strategy; every candidate receives the same dense 3-D validation. */
constexpr size_t kIsoparametricWitnessMaximumDirectionalSpans = 32;
constexpr size_t kIsoparametricWitnessMaximumSurfacePatches = 64;

/* Pullback fragments are polylines.  When deciding whether an extra fragment
 * duplicates a complete authoritative STEP edge, test the represented line
 * between every pair of stored UV samples, not just the stored endpoints.
 * Eight subdivisions per interval is a bounded rejection test; accepted
 * pcurves still undergo the 1024-segment final validation above. */
constexpr int kFragmentIntervalValidationSubdivisions = 8;

/* Closed-edge isocurve recognition is only a candidate generator; every
 * accepted trim is subsequently subjected to the 1024-segment validation
 * above.  Sixty-four segments reliably distinguishes traversal direction and
 * supplies the locus matcher, while eight source samples provide a cheap
 * one-way rejection proof before a parameterization-independent curve search.
 * The rejection test cannot accept geometry and therefore does not weaken the
 * authoritative dense validation. */
constexpr int kClosedIsoCandidateValidationSegments = 64;
constexpr int kClosedIsoCandidateRejectionSegments = 8;

/* A closed pullback's declared STEP topology vertex may fall between two
 * adaptive UV samples after a legacy periodic seam rotation.  Search each
 * already validated polyline segment at a small deterministic resolution,
 * then refine only the best bracket.  This locates a cyclic cut; it does not
 * alter the pullback locus, and the accepted point must still lift to the
 * topology vertex within the measured local tolerance. */
constexpr int kTopologyVertexSegmentSearchSubdivisions = 16;
constexpr int kTopologyVertexSegmentRefinementIterations = 32;
/* Both a validated pullback sample and an exact candidate isocurve may sit one
 * model tolerance from the authoritative 3-D edge.  Four tolerances therefore
 * leave the full triangle-inequality allowance plus numerical headroom for a
 * rejection-only lift test.  Passing this gate never accepts a trim. */
constexpr double kClosedIsoCandidateLiftGateToleranceMultiplier = 4.0;
/* Pre-closure full-period restoration is needed only when closest-point
 * pullback has erased essentially the entire UV winding.  One ten-thousandth
 * of each native parameter domain distinguishes that collapsed cloud from an
 * already usable closed-edge path without acting as a geometric tolerance;
 * the reconstructed candidate is still densely validated in model space. */
/* A relocated periodic seam must lie in an interval containing no sampled
 * boundary data.  Require that empty interval to span at least one thousandth
 * of the complete parameter period so the new seam is not numerically
 * indistinguishable from the exact boundary it is intended to avoid.  The
 * complete remapped boundary is still validated in 3-D before acceptance. */
constexpr double kMinimumSafeSeamGapFraction = 1.0e-3;

static void
record_pullback_context_statistics(STEPWrapper *step,
    const std::list<PBCData *> &samples)
{
    if (!step) return;
    std::set<const brlcad::PullbackContext *> recorded;
    for (std::list<PBCData *>::const_iterator sample = samples.begin();
	    sample != samples.end(); ++sample) {
	if (!*sample || !(*sample)->context) continue;
	const brlcad::PullbackContext *context = (*sample)->context.get();
	if (!recorded.insert(context).second) continue;
	step->RecordPullbackStatistics(context->Statistics());
    }
}

/* If dense validation finds a missed bow between already projected UV
 * samples, refine only that exceptional polyline.  These are work ceilings,
 * not approximation settings: every inserted point and the completed curve
 * are still checked against the exact 3-D STEP edge. */
constexpr int kMaximumAdjustedPolylineRefinementDepth = 12;
constexpr size_t kMaximumAdjustedPolylineSamples = 16384;

/* STEP pcurves at a periodic join can differ by a few measured microradians
 * even when their lifted 3-D endpoints and topology vertex coincide.  This
 * scale-relative bound only recognizes and snaps parameters already proven
 * coincident in model space; it is never used to accept geometric error. */
constexpr double kPeriodicParameterSnapFraction = 1.0e-5;

/* Directed winding accumulates projection error from every independently
 * supplied edge, so its residual is not an endpoint snap tolerance.  Permit
 * at most one thousandth of a turn for cap classification; acceptance still
 * requires a topology join separated by one exact period, coincident 3-D
 * lifts, and dense validation of the reparameterized surface.  This bound
 * covers the 1.2e-5-turn residual in the compact HUMVEE spherical-cap guard
 * without treating a materially partial winding as a pole boundary. */
constexpr double kPeriodicWindingResidualFraction = 1.0e-3;

/* Evaluating a rational periodic surface at parameters separated by one exact
 * period can differ slightly because its independently supplied control points
 * and knots are finite-precision decimal data.  Treat the two parameter images
 * as equivalent only when their lifts agree within two percent of the model
 * uncertainty.  This bound is used solely to rejoin an internal UV split; every
 * merged sample must still lie on the original STEP edge within its full
 * model-derived tolerance. */
constexpr double kPeriodicLiftEquivalenceToleranceFraction = 2.0e-2;

/* Keep numerical solver floors comfortably above floating-point zero without
 * replacing the model-derived tolerance used for acceptance. */
constexpr double kNumericalToleranceScale = 1024.0;

/* A proposed seam relocation must preserve the spatial coverage of its STEP
 * edge.  Locus-only checks can otherwise collapse a closed circle to its
 * shared vertex and still report zero point-to-curve distance. */
constexpr double kMinimumSeamCoverageFraction = 0.8;

/* Real exchange files occasionally contain an edge curve, its asserted
 * vertices, and its supporting surface at separations larger than the
 * declared uncertainty.  Non-exact mode may reflect that measured reality in
 * the one affected BREP edge, but only below one percent of the edge scale.
 * This is a guardrail against accepting an unrelated surface or topology-
 * scale gap, not an approximation allowance: source curves and vertices stay
 * unchanged and dense lift validation still applies. */
constexpr double kMaximumRelativeEdgeMismatch = 1.0e-2;
constexpr double kMaximumRelativeItemMismatch = 1.0e-3;
/* An explicitly associated boundary curve whose two exact endpoints both lie
 * on the supplied surface at the declared uncertainty has stronger evidence
 * of modeling intent than an arbitrary nearby curve.  In safe mode, complete
 * dense projection may reflect interior approximation drift up to one eighth
 * of one percent of the item scale.  This is only a 25 percent extension of
 * the ordinary item-relative ceiling, is unavailable unless both endpoint
 * anchors pass independently, and never moves either source object. */
constexpr double kMaximumRelativeEndpointAnchoredItemMismatch = 1.25e-3;
/* An open, non-periodic NURBS surface has no equivalent wrapped parameter
 * branch.  After strict projection has identified a continuous closest locus,
 * safe mode may measure a slightly wider source curve/surface disagreement:
 * at most two percent of the bounded edge or two tenths of one percent of the
 * complete item.  This second ceiling is deliberately unavailable to planes,
 * analytic/periodic surfaces, vertices, and --exact.  It does not move either
 * source object; the candidate must still pass complete 1024-segment
 * curve-locus validation and whole-BREP validation. */
constexpr double kMaximumRelativeOpenNurbsEdgeMismatch = 2.0e-2;
constexpr double kMaximumRelativeOpenNurbsItemMismatch = 2.0e-3;
/* Once the source graph has been proven contradictory, permissive import may
 * construct a projected trim rather than omit the complete solid.  These are
 * catastrophe guards, not claims that the inference is a safe repair: the
 * result is tagged, and the complete B-rep must still validate.  Keeping the
 * search below both one quarter of the affected edge scale and five percent
 * of the item scale prevents either a very large item or a very long edge from
 * authorizing an accidentally associated distant surface. */
constexpr double kMaximumPermissiveRelativeEdgeMismatch = 2.5e-1;
constexpr double kMaximumPermissiveRelativeItemMismatch = 5.0e-2;
/* A separate whole-solid inference transaction may substitute geometry whose
 * local discrepancy exceeds the ordinary feature-relative guard, but the
 * measured displacement must still be small compared with the complete item.
 * One percent is intentionally much tighter than the ordinary permissive
 * item's catastrophe guard.  The transaction is accepted only after the
 * complete result proves to be a closed, oriented manifold solid, and every
 * substituted edge is retained in the inference provenance. */
constexpr double kMaximumWholeItemInferenceRelativeMismatch = 1.0e-2;
/* An explicit EDGE_CURVE reference supplies stronger identity than geometric
 * adjacency alone.  Safe mode may retain measured vertex drift up to 35
 * percent of the exact bounded curve length, after closest-point and endpoint
 * reevaluation proofs.  This covers observed low-precision exports whose
 * entire short edge is offset consistently (0.030 mm on a 0.090 mm edge)
 * without admitting the 40%-plus gaps and same-parameter endpoints found in
 * demonstrably broken loops.  Later edge/surface and whole-BREP validation
 * remain mandatory. */
constexpr double kMaximumRelativeEdgeVertexMismatch = 3.5e-1;
/* A STEP EDGE_CURVE explicitly associates its curve and topology vertices.
 * If that association is contradictory beyond the safe limit, permissive
 * import may retain a vertex-bounded interval with an inferred tolerance up
 * to two complete traversals of that interval.  This is deliberately much
 * wider than safe repair and is always tagged as inferred geometry. */
constexpr double kMaximumPermissiveEdgeVertexTraversalFraction = 2.0;
/* The special one-vertex/open-curve contradiction is resolved only by a
 * whole-item topology repair.  Screen inference retries with the same tight
 * item-relative ceiling applied by that later transaction; the complete
 * locus and neighboring-edge limit are still proved there. */
constexpr double kZeroLengthTopologyMaximumRelativeItemMismatch = 5.0e-4;
/* A contradictory EDGE_CURVE may contain only a terminal fragment of the edge
 * named by its topology vertices.  Permissive import may prepend or append the
 * missing straight segment only when one fragment endpoint is already exact.
 * A nearly straight fragment must remain inside a narrow chord tube with a
 * combined path no longer than 125 percent of the asserted edge.  A curved
 * fragment may instead traverse at most twice that edge and the minimal
 * connector at most 125 percent of the edge.  The latter accommodates a
 * terminal fragment which extends slightly beyond its exact topology endpoint
 * without letting a remote curve become a candidate.  These bounds preserve
 * the complete authored locus; the result is tagged as inferred and the
 * complete BREP must validate. */
constexpr double kMaximumPermissiveTopologyBridgeFraction = 1.25;
constexpr double kMaximumPermissiveTopologyBridgeTubeFraction = 1.25e-1;
constexpr double kMaximumPermissiveTopologyBridgePathFraction = 1.25;
constexpr double kMaximumPermissiveTopologyFragmentTraversalFraction = 2.0;
constexpr double kMaximumPermissiveTopologyCurvedPathFraction = 3.0;
/* A bounded EDGE_CURVE can name two vertices just beyond one end of an open
 * NURBS curve even though the terminal polynomial span continues through the
 * missing point.  Safe repair may use that exact continuation only when the
 * omitted tail is no more than two tenths of one percent of the complete curve
 * scale and no more than eight declared uncertainties.  The extended span must
 * hit the asserted vertex at the declared tolerance; --exact never extends
 * source domains. */
constexpr double kMaximumRelativeNurbsEndpointExtension = 2.0e-3;
constexpr double kMaximumDeclaredNurbsEndpointExtensionFactor = 8.0;
/* A declared uncertainty rounded below the source data's measured separation
 * needs an absolute-tolerance allowance even when the affected feature is
 * shorter than that uncertainty.  Safe mode may reflect at most twice the
 * declaration in one affected OpenNURBS edge.  Closest-point endpoint proofs,
 * dense curve/surface validation, the independent relative feature/item
 * ceilings, and whole-BREP validation still apply; source geometry is not
 * moved.  Scale-relative allowances continue to handle larger features, and
 * --exact never enters the adjustment paths which consume either allowance.
 */
constexpr double kMaximumDeclaredToleranceAdjustmentFactor = 2.0;
constexpr double kMeasuredToleranceSafetyFactor = 1.05;
/* Do not use an enlarged first projection search when the declared output-
 * space uncertainty is below ten nanometres.  At that scale, decimal exchange
 * noise and competing periodic branches dominate the claimed tolerance; a
 * strict first pass is required to establish the branch before any densely
 * measured retry.  LocalUnits converts STEP lengths to millimetres. */
constexpr double kMinimumBoundedNurbsFirstPassToleranceMillimeters = 1.0e-5;
/* Conversely, ordinary CAD tolerances at or above a tenth of a micrometre do
 * not need the enlarged first search: the strict pass can establish a stable
 * branch directly, and doing so avoids selecting a different periodic image.
 * The bounded first pass is therefore reserved for the narrow 10--100 nm
 * range where exchange precision can defeat strict seeding without making
 * periodic branch identity numerically ambiguous. */
constexpr double kMaximumBoundedNurbsFirstPassToleranceMillimeters = 1.0e-4;
/* Adaptive UV refinement can discover a larger source mismatch between the
 * original knot samples.  Re-measure and retry a small bounded number of
 * times; double the search tolerance when successive refinement levels expose
 * the mismatch only incrementally.  The accepted BREP tolerance is reset to
 * the largest actually measured error (and later densely validated), so this
 * exponential search does not itself loosen output geometry. */
constexpr int kMaximumMeasuredToleranceRetries = 6;

/* Curve-locus validation brackets the closest point independently in every
 * exact NURBS knot span, then refines the best interval.  Sixty-four brackets
 * are sufficient for a cubic span while keeping exceptional adjusted-edge
 * validation deterministic and bounded. */
constexpr int kCurveClosestBracketsPerSpan = 64;
/* A chord projection normally starts within the local convergence basin for
 * CAD edge curves.  Eight safeguarded Newton steps are enough to test that
 * fast path; failure retains the bounded golden-section fallback below. */
constexpr int kCurveClosestNewtonIterations = 8;
constexpr int kCurveClosestRefinementIterations = 64;

} // namespace



#include "OpenNurbsPullback.inc"
#include "OpenNurbsTopology.inc"


ON_Curve *
step_exact_planar_pcurve(const ON_Surface *surface, const ON_Curve *curve,
    double tolerance)
{
    ON_Curve *result = NULL;
    PBCData *data = exact_planar_pullback(surface, curve, tolerance,
	tolerance, &result);
    if (!data) {
	delete result;
	return NULL;
    }
    destroy_pullback_data(data);
    return result;
}


ON_Curve *
step_curve_surface_pcurve(const ON_Surface *surface, const ON_Curve *curve,
    double tolerance, std::string *failure_reason)
{
    if (failure_reason)
	failure_reason->clear();
    if (!surface || !curve || !surface->IsValid() || !curve->IsValid() ||
	    !(tolerance > 0.0)) {
	if (failure_reason)
	    *failure_reason = "the surface, curve, or model tolerance was invalid";
	return NULL;
    }

    /* A finite plane has an exact affine inverse and should not pay for (or
     * inherit the branch ambiguity of) a numerical closest-point solve. */
    if (ON_PlaneSurface::Cast(surface)) {
	ON_Curve *plane = step_exact_planar_pcurve(surface, curve, tolerance);
	if (plane)
	    return plane;
    }

    std::shared_ptr<brlcad::PullbackContext> context(
	new brlcad::PullbackContext());
    PBCData *data = pullback_samples(surface, curve, tolerance, tolerance,
	tolerance, tolerance, context);
    if (!data || data->rejected_projection_samples != 0 ||
	    !data->samples_source_validated ||
	    pullback_sample_count(data) < 2) {
	if (failure_reason) {
	    std::ostringstream reason;
	    reason << "bounded curve-to-surface projection did not produce a "
		"complete pcurve";
	    if (data)
		reason << " (" << data->rejected_projection_samples << '/'
		    << data->projection_samples << " samples rejected, maximum "
		    << "distance " << data->maximum_projection_distance << ')';
	    *failure_reason = reason.str();
	}
	destroy_pullback_data(data);
	return NULL;
    }

    ON_3dPointArray points;
    ON_2dPoint previous = ON_2dPoint::UnsetPoint;
    for (std::list<ON_2dPointArray *>::const_iterator segment =
	    data->segments->begin(); segment != data->segments->end(); ++segment) {
	if (!*segment)
	    continue;
	for (int sample = 0; sample < (*segment)->Count(); ++sample) {
	    ON_2dPoint uv = (**segment)[sample];
	    if (!uv.IsValid()) {
		destroy_pullback_data(data);
		if (failure_reason)
		    *failure_reason = "curve-to-surface projection produced a "
			"non-finite parameter";
		return NULL;
	    }
	    /* Independently sampled fragments may choose neighboring images of
	     * a periodic surface.  Translate whole periods to retain the nearest
	     * continuous branch; this changes neither the surface locus nor the
	     * authored 3-D curve. */
	    if (previous.IsValid()) {
		for (int direction = 0; direction < 2; ++direction) {
		    if (!surface->IsClosed(direction))
			continue;
		    const double period = surface->Domain(direction).Length();
		    if (period > ON_ZERO_TOLERANCE)
			uv[direction] += round((previous[direction] -
			    uv[direction]) / period) * period;
		}
		if (uv.DistanceTo(previous) <= ON_ZERO_TOLERANCE)
		    continue;
	    }
	    points.Append(ON_3dPoint(uv.x, uv.y, 0.0));
	    previous = uv;
	}
    }
    destroy_pullback_data(data);
    if (points.Count() < 2) {
	if (failure_reason)
	    *failure_reason = "curve-to-surface projection collapsed to one "
		"parameter point";
	return NULL;
    }

    std::unique_ptr<ON_PolylineCurve> candidate(new ON_PolylineCurve(points));
    if (!candidate->ChangeDimension(2) || !candidate->IsValid()) {
	if (failure_reason)
	    *failure_reason = "the projected parameter polyline was invalid";
	return NULL;
    }

    std::vector<ON_3dPoint> lifted(kDenseLiftValidationSegments + 1,
	ON_3dPoint::UnsetPoint);
    const ON_Interval domain = candidate->Domain();
    bool finite = domain.IsIncreasing();
    for (int sample = 0; finite && sample <= kDenseLiftValidationSegments;
	    ++sample) {
	const ON_3dPoint uv = candidate->PointAt(domain.ParameterAt(
	    static_cast<double>(sample) / kDenseLiftValidationSegments));
	lifted[sample] = uv.IsValid() ? surface->PointAt(uv.x, uv.y) :
	    ON_3dPoint::UnsetPoint;
	finite = lifted[sample].IsValid();
    }
    std::size_t rejected_index = 0;
    double rejected_distance = DBL_MAX;
    if (!finite || !step_curve_locus_contains_points(curve, lifted.data(),
	    lifted.size(), tolerance, &rejected_index, &rejected_distance)) {
	if (failure_reason) {
	    std::ostringstream reason;
	    reason << "the projected pcurve did not preserve the complete 3-D "
		"curve locus";
	    if (finite)
		reason << " at sample " << rejected_index << '/'
		    << kDenseLiftValidationSegments << " (distance "
		    << rejected_distance << ", tolerance " << tolerance << ')';
	    *failure_reason = reason.str();
	}
	return NULL;
    }
    return candidate.release();
}


ON_Curve *
step_closed_curve_with_seam_at(const ON_Curve *curve,
    const ON_3dPoint &point, double tolerance)
{
    if (!curve || !curve->IsClosed() || !point.IsValid() ||
	    !(tolerance > 0.0))
	return NULL;
    CurveDistanceEvaluator evaluator(curve);
    double parameter = 0.0;
    double distance = DBL_MAX;
    if (!evaluator.ClosestParameter(point, &parameter, &distance) ||
	    distance > tolerance)
	return NULL;
    std::unique_ptr<ON_NurbsCurve> result(new ON_NurbsCurve());
    if (!curve->GetNurbForm(*result) || !result->IsClosed() ||
	    !result->ChangeClosedCurveSeam(parameter) || !result->IsValid() ||
	    result->PointAtStart().DistanceTo(point) > tolerance ||
	    result->PointAtEnd().DistanceTo(point) > tolerance)
	return NULL;
    return result.release();
}
