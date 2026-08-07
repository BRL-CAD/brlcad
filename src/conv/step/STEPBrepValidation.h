/*                 S T E P B R E P V A L I D A T I O N . H
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 */
/** @file step/STEPBrepValidation.h
 *
 * Small, schema-neutral validation predicates used to decide whether an exact
 * STEP BREP repair transaction is safe to commit.
 */

#ifndef CONV_STEP_STEPBREPVALIDATION_H
#define CONV_STEP_STEPBREPVALIDATION_H

#include <cstddef>
#include <string>
#include <vector>

class ON_Brep;
class ON_BrepEdge;
class ON_BrepLoop;
class ON_BrepTrim;
class ON_BrepVertex;

namespace brlcad {
namespace step {

double DirectedTrimEndpointRatio(const ON_BrepTrim &trim,
	double model_tolerance);

bool DirectedTrimEndpointRegressed(const ON_BrepTrim &before,
	const ON_BrepTrim &after, double model_tolerance);

bool FaceTrimValidationRegressed(const ON_Brep &before,
	const ON_Brep &after, int face_index, double model_tolerance);

bool FaceOrientationConstraintsAreConsistent(const ON_Brep &brep);

/**
 * Count reciprocal two-use edges whose current face/trim directions agree
 * instead of opposing one another.  This is the same local orientation defect
 * counted by ON_Brep::IsSolid(), exposed here so transactional repairs can
 * prove they did not make an already-inconsistent shell worse.
 */
size_t FaceOrientationConflictCount(const ON_Brep &brep);

/**
 * Discover one coherent set of complete-loop flips which makes every
 * reciprocal two-use edge oppositely directed.  Unlike a face-only graph,
 * this includes edges shared by distinct loops on the same face.  A
 * contradictory constraint (including two agreeing uses in one loop) returns
 * false without changing the BREP.
 */
bool LoopOrientationFlipPlan(const ON_Brep &brep, std::vector<int> &flip,
	std::string *failure = NULL);

/**
 * True only when a loop has both a singular pole trim and the reciprocal,
 * importer-created seam pair which makes that pole topology complete.
 */
bool LoopHasCompletePeriodicPoleTopology(const ON_BrepLoop &loop);

/**
 * True only when two one-use boundary edges belong to distinct faces.  The
 * generic duplicate-edge repair must not pair fragments from one face:
 * singularity children can share a STEP edge identity and exact locus without
 * representing reciprocal topology.  Same-face periodic seams require their
 * separate native-side proof.
 */
bool DuplicateBoundaryEdgeUsesAreOnDistinctFaces(const ON_BrepEdge &first,
	const ON_BrepEdge &second);

/**
 * True when two independently materialized endpoint vertices are eligible
 * for a duplicate STEP-edge merge.  Anonymous and authoritative vertices
 * cannot be mixed.  Distinct positive STEP vertex identities are accepted
 * only for two copies of the same STEP edge and only inside the declared
 * model tolerance; a looser measured edge tolerance cannot authorize that
 * identity restoration.
 */
bool DuplicateStepEdgeEndpointsMatch(const ON_BrepVertex &first,
	const ON_BrepVertex &second, double model_tolerance,
	double edge_tolerance, bool same_step_edge);

/**
 * Prove that a one-use child of a positive STEP edge is a tolerance-degenerate
 * importer split, not an authoritative short source edge.  At least one
 * endpoint must be anonymous and the complete bounded curve must remain
 * inside the asserted tolerance ball.
 */
bool ImporterSplitEdgeIsToleranceDegenerate(const ON_BrepEdge &edge,
	double model_tolerance);

} // namespace step
} // namespace brlcad

#endif
