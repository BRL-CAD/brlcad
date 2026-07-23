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
