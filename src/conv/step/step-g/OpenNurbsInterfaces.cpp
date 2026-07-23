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
 * reversals in the Mark V acceptance model while keeping validation linear and
 * deterministic.  This is a validation budget, not a geometric tolerance or
 * a request to approximate the edge with 1024 segments. */
constexpr int kDenseLiftValidationSegments = 1024;

/* Linear span scans are cheaper for the small curves that dominate ordinary
 * models.  At 32 spans, an R-tree is shallow enough to amortize its build cost
 * while avoiding the quadratic validation behavior seen on imported curves
 * with thousands of spans.  This is solely a search-strategy threshold: every
 * candidate returned by the tree still undergoes the same exact curve tests. */
constexpr size_t kCurveDistanceRTreeMinimumSpans = 32;

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
constexpr double kCollapsedFullPeriodMaximumRelativeSpan = 1.0e-4;
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
/* An explicit EDGE_CURVE reference supplies stronger identity than geometric
 * adjacency alone.  Safe mode may retain measured vertex drift up to 35
 * percent of the exact bounded curve length, after closest-point and endpoint
 * reevaluation proofs.  This covers observed low-precision exports whose
 * entire short edge is offset consistently (0.030 mm on a 0.090 mm edge)
 * without admitting the 40%-plus gaps and same-parameter endpoints found in
 * demonstrably broken loops.  Later edge/surface and whole-BREP validation
 * remain mandatory. */
constexpr double kMaximumRelativeEdgeVertexMismatch = 3.5e-1;
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
