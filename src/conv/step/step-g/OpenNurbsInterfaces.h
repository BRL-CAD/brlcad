/*                 O P E N N U R B S I N T E R F A C E S . H
 * BRL-CAD
 *
 * Copyright (c) 1994-2026 United States Government as represented by the
 * U.S. Army Research Laboratory.
 *
 * Internal interfaces shared by the STEP topology construction and bounded
 * post-construction repair passes.
 */

#ifndef CONV_STEP_OPENNURBSINTERFACES_H
#define CONV_STEP_OPENNURBSINTERFACES_H

#include <cstddef>
#include <functional>
#include <string>

class ON_Brep;
class ON_BrepLoop;
class ON_Curve;
class ON_Surface;
class ON_BrepTrim;
class ON_2dPoint;
class ON_3dPoint;

/* Deterministically prove that every supplied point lies on a bounded curve
 * within the caller's model-space tolerance.  The implementation brackets
 * every exact NURBS knot span and reuses one evaluator across the batch;
 * unlike ON_NurbsCurve_GetClosestPoint, it cannot silently reject a valid
 * point after converging to the wrong local branch of a closed curve. */
bool step_curve_locus_contains_points(const ON_Curve *curve,
    const ON_3dPoint *points, std::size_t point_count, double tolerance,
    std::size_t *rejected_index = NULL,
    double *rejected_distance = NULL,
    const std::function<void(std::size_t, std::size_t,
	const std::string &)> &progress =
	std::function<void(std::size_t, std::size_t, const std::string &)>());

/** Express a 3-D curve on a finite OpenNURBS plane in that plane surface's
 * private parameter domain.  The returned exact curve is caller-owned; NULL
 * means the complete lift could not be proven within tolerance. */
ON_Curve *step_exact_planar_pcurve(const ON_Surface *surface,
    const ON_Curve *curve, double tolerance);

/** Express a 3-D boundary curve in an OpenNURBS surface's private parameter
 * domain.  This is the schema-neutral, topology-free subset of the STEP face
 * pullback machinery: the returned caller-owned pcurve is accepted only when
 * its complete lifted locus agrees with the immutable 3-D curve within the
 * supplied model tolerance. */
ON_Curve *step_curve_surface_pcurve(const ON_Surface *surface,
    const ON_Curve *curve, double tolerance,
    std::string *failure_reason = NULL);

/** Return a caller-owned exact NURBS copy of a closed curve whose arbitrary
 * parameter seam is relocated to the supplied point.  The point must lie on
 * the complete curve locus within tolerance. */
ON_Curve *step_closed_curve_with_seam_at(const ON_Curve *curve,
    const ON_3dPoint &point, double tolerance);

/** Stitch independently constructed STEP faces using authoritative source
 * vertex/edge identities.  Edges are combined only after their endpoints and
 * both complete curve loci pass bounded dense validation; this deliberately
 * leaves unequal seam fragments for the whole-solid reconciliation pass. */
bool step_stitch_face_breps(ON_Brep *brep, double tolerance,
    std::string *failure_reason = NULL);

bool step_insert_periodic_pole_cut(ON_Brep *brep, ON_BrepLoop &loop,
    const ON_Surface *surface, const ON_BrepTrim &boundary_trim,
    const ON_2dPoint &boundary_end, const ON_2dPoint &boundary_start,
    double tolerance, std::string *failure_reason = NULL);

#endif /* CONV_STEP_OPENNURBSINTERFACES_H */
