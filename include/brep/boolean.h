/*                       B O O L E A N . H
 * BRL-CAD
 *
 * Copyright (c) 2007-2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @addtogroup brep_boolean
 * @brief
 * Boolean Operations for  Non-Uniform Rational
 * B-Spline (NURBS) Boundary Representations.
 */
#ifndef BREP_BOOLEAN_H
#define BREP_BOOLEAN_H

#include "common.h"
#include "brep/defines.h"

/** @{ */
/** @file brep/boolean.h */

/** Stable stages for machine-readable Boolean failure diagnostics. */
enum brep_boolean_failure_stage {
    BREP_BOOLEAN_FAILURE_STAGE_NONE = 0,
    BREP_BOOLEAN_FAILURE_STAGE_INPUT_VALIDATION = 1,
    BREP_BOOLEAN_FAILURE_STAGE_OPERAND_BOUNDS = 2,
    BREP_BOOLEAN_FAILURE_STAGE_SURFACE_INTERSECTION = 3,
    BREP_BOOLEAN_FAILURE_STAGE_SSI_PROCESSING = 4,
    BREP_BOOLEAN_FAILURE_STAGE_TRIM_ARRANGEMENT = 5,
    BREP_BOOLEAN_FAILURE_STAGE_FACE_SPLITTING = 6,
    BREP_BOOLEAN_FAILURE_STAGE_FACE_CLASSIFICATION = 7,
    BREP_BOOLEAN_FAILURE_STAGE_OUTPUT_ASSEMBLY = 8,
    BREP_BOOLEAN_FAILURE_STAGE_EDGE_REPAIR = 9,
    BREP_BOOLEAN_FAILURE_STAGE_OUTPUT_VALIDATION = 10,
    BREP_BOOLEAN_FAILURE_STAGE_INTERNAL = 11
};

/** Stable causes for machine-readable Boolean failure diagnostics. */
enum brep_boolean_failure_reason {
    BREP_BOOLEAN_FAILURE_REASON_NONE = 0,
    BREP_BOOLEAN_FAILURE_REASON_INVALID_INPUT = 1,
    BREP_BOOLEAN_FAILURE_REASON_UNSUPPORTED_OPERATION = 2,
    BREP_BOOLEAN_FAILURE_REASON_OPERAND_BOUNDS = 3,
    BREP_BOOLEAN_FAILURE_REASON_WORK_BUDGET = 4,
    BREP_BOOLEAN_FAILURE_REASON_SSI_PROCESSING = 5,
    BREP_BOOLEAN_FAILURE_REASON_SSI_CLIPPING = 6,
    BREP_BOOLEAN_FAILURE_REASON_CURVE_PROJECTION = 7,
    BREP_BOOLEAN_FAILURE_REASON_ARRANGEMENT_UNBALANCED = 8,
    BREP_BOOLEAN_FAILURE_REASON_ARRANGEMENT_TRANSITION = 9,
    BREP_BOOLEAN_FAILURE_REASON_ARRANGEMENT_OPEN_LOOP = 10,
    BREP_BOOLEAN_FAILURE_REASON_FACE_SPLIT = 11,
    BREP_BOOLEAN_FAILURE_REASON_FACE_CLASSIFICATION = 12,
    BREP_BOOLEAN_FAILURE_REASON_OUTPUT_ASSEMBLY = 13,
    BREP_BOOLEAN_FAILURE_REASON_PRESERVED_COMPONENT = 14,
    BREP_BOOLEAN_FAILURE_REASON_PRE_REPAIR_INVALID = 15,
    BREP_BOOLEAN_FAILURE_REASON_EDGE_MERGE = 16,
    BREP_BOOLEAN_FAILURE_REASON_OPEN_SSI_EDGE = 17,
    BREP_BOOLEAN_FAILURE_REASON_CLOSED_SSI_EDGE = 18,
    BREP_BOOLEAN_FAILURE_REASON_EDGE_STITCH = 19,
    BREP_BOOLEAN_FAILURE_REASON_OFFSET_SEAM = 20,
    BREP_BOOLEAN_FAILURE_REASON_SOURCE_EDGE_PARTITION = 21,
    BREP_BOOLEAN_FAILURE_REASON_DEGENERATE_EDGE_PAIR = 22,
    BREP_BOOLEAN_FAILURE_REASON_NAKED_EDGE_PARTITION = 23,
    BREP_BOOLEAN_FAILURE_REASON_NAKED_HAIRPIN = 24,
    BREP_BOOLEAN_FAILURE_REASON_MISSING_CAP = 25,
    BREP_BOOLEAN_FAILURE_REASON_TOLERANCE_UPDATE = 26,
    BREP_BOOLEAN_FAILURE_REASON_UNMATCHED_EDGE = 27,
    BREP_BOOLEAN_FAILURE_REASON_NONMANIFOLD_EDGE = 28,
    BREP_BOOLEAN_FAILURE_REASON_AMBIGUOUS_EDGE_MATCH = 29,
    BREP_BOOLEAN_FAILURE_REASON_OUTPUT_INVALID = 30,
    BREP_BOOLEAN_FAILURE_REASON_OUTPUT_NOT_SOLID = 31,
    BREP_BOOLEAN_FAILURE_REASON_INTERNAL_ERROR = 32,
    BREP_BOOLEAN_FAILURE_REASON_ARRANGEMENT_EXCESS_INCOMING = 33,
    BREP_BOOLEAN_FAILURE_REASON_ARRANGEMENT_EXCESS_OUTGOING = 34,
    BREP_BOOLEAN_FAILURE_REASON_OPEN_SSI_MISSING_MATE = 35,
    BREP_BOOLEAN_FAILURE_REASON_CLOSED_SSI_MISSING_MATE = 36,
    BREP_BOOLEAN_FAILURE_REASON_SSI_EDGE_PARTITION = 37
};

/**
 * Detailed evidence from one Boolean evaluation.  Face indices use the
 * concatenated operand ordering: left faces first, followed by right faces.
 * Negative face and edge indices mean that no unique item was identified.
 */
struct brep_boolean_failure_info {
    enum brep_boolean_failure_stage stage;
    enum brep_boolean_failure_reason reason;
    int face_index;
    int peer_face_index;
    int edge_index;
    int peer_edge_index;
    size_t intersection_candidates;
    size_t intersection_events;
    size_t projection_failures;
    size_t output_naked_edges;
    size_t output_nonmanifold_edges;
    size_t ambiguous_edge_matches;
    double tolerance;
};

#define BREP_BOOLEAN_FAILURE_INFO_INIT { \
    BREP_BOOLEAN_FAILURE_STAGE_NONE, BREP_BOOLEAN_FAILURE_REASON_NONE, \
    -1, -1, -1, -1, 0, 0, 0, 0, 0, 0, 0.0 \
}

#ifdef __cplusplus

extern "C++" {

enum op_type {
    BOOLEAN_UNION = 0,
    BOOLEAN_INTERSECT = 1,
    BOOLEAN_DIFF = 2,
    BOOLEAN_XOR = 3
};

enum boolean_result {
    /* Retained for callers that cannot classify an older generic failure. */
    BREP_BOOLEAN_FAILURE = -1,
    BREP_BOOLEAN_INVALID_INPUT = -2,
    BREP_BOOLEAN_INDETERMINATE = -3,
    BREP_BOOLEAN_BUDGET_EXCEEDED = -4,
    BREP_BOOLEAN_UNSUPPORTED = -5,
    BREP_BOOLEAN_INTERNAL_ERROR = -6,
    BREP_BOOLEAN_SUCCESS = 0,
    BREP_BOOLEAN_EMPTY = 1
};

/** Return a stable, human-readable description of a Boolean result code. */
extern BREP_EXPORT const char *
ON_BooleanStatusDescription(int status);

/** Return stable names for detailed Boolean diagnostic values. */
extern BREP_EXPORT const char *
ON_BooleanFailureStageDescription(enum brep_boolean_failure_stage stage);

extern BREP_EXPORT const char *
ON_BooleanFailureReasonDescription(enum brep_boolean_failure_reason reason);

/**
 * Evaluate NURBS boolean operations.
 *
 * @param brepO [out]
 * @param brepA [in]
 * @param brepB [in]
 * @param operation [in]
 * @return BREP_BOOLEAN_SUCCESS for a nonempty result,
 * BREP_BOOLEAN_EMPTY for an empty result, or a negative boolean_result code.
 * On failure, @p brepO is empty.
 */
extern BREP_EXPORT int
ON_Boolean(ON_Brep *brepO, const ON_Brep *brepA, const ON_Brep *brepB, op_type operation);

/** Evaluate a Boolean and return detailed failure evidence when requested. */
extern BREP_EXPORT int
ON_Boolean(ON_Brep *brepO, const ON_Brep *brepA, const ON_Brep *brepB,
	op_type operation, struct brep_boolean_failure_info *failure);

/**
 * Classify a point known not to lie on the boundary of a valid closed BRep.
 *
 * @return true when the point is inside, false when it is outside or the
 * input cannot be classified safely.
 */
extern BREP_EXPORT bool
ON_BrepPointInside(const ON_3dPoint &point, const ON_Brep *brep);

} /* extern C++ */
#endif

#endif  /* BREP_BOOLEAN_H */
/** @} */
/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
