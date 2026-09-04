/*                    A S S E M B L Y . H
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 */
/** @file brep/assembly.h
 *
 * Assemble independently constructed trimmed faces into shared OpenNURBS
 * topology.  These routines do not standardize or compact a B-Rep.
 */

#ifndef BREP_ASSEMBLY_H
#define BREP_ASSEMBLY_H

#include "common.h"
#include "brep/defines.h"

__BEGIN_DECLS

#ifdef __cplusplus
extern "C++" {

enum brep_assembly_error {
    BREP_ASSEMBLY_OK = 0,
    BREP_ASSEMBLY_INVALID_TOLERANCE,
    BREP_ASSEMBLY_VERTEX_MERGE_FAILED,
    BREP_ASSEMBLY_EDGE_MERGE_FAILED,
    BREP_ASSEMBLY_CULL_FAILED,
    BREP_ASSEMBLY_ORIENTATION_FAILED,
    BREP_ASSEMBLY_VALIDATION_FAILED
};

struct brep_assembly_result {
    int input_naked_edges = 0;
    int merged_edges = 0;
    int ambiguous_edges = 0;
    int remaining_naked_edges = 0;
    int nonmanifold_edges = 0;
    bool oriented = false;
    bool valid = false;
    bool solid = false;
    enum brep_assembly_error error = BREP_ASSEMBLY_OK;
};

/** Test whether two curves trace the same locus within @p tolerance. */
extern BREP_EXPORT bool
brep_curves_coincident(const ON_Curve &first, const ON_Curve &second,
	double tolerance, bool *reversed = NULL);

/**
 * Merge unambiguous pairs of coincident naked edges in place.
 *
 * Each retained edge acquires the trims from its duplicate.  Face pcurves are
 * not changed.  Returns the number of merged edges, or -1 on an OpenNURBS
 * topology error.  Ambiguous matches are deliberately left naked.
 */
extern BREP_EXPORT int
brep_stitch_naked_edges(ON_Brep &brep, double tolerance,
	brep_assembly_result *result = NULL);

/** Make face orientations consistent in a closed two-manifold B-Rep. */
extern BREP_EXPORT bool
brep_orient_faces(ON_Brep &brep);

/**
 * Stitch, orient, derive tolerances and flags, and validate an assembled
 * B-Rep.  The return value reports whether the resulting B-Rep is valid;
 * solidity and incomplete topology are reported separately in @p result.
 */
extern BREP_EXPORT bool
brep_assemble(ON_Brep &brep, double tolerance,
	brep_assembly_result *result = NULL);

}
#endif

__END_DECLS

#endif /* BREP_ASSEMBLY_H */
