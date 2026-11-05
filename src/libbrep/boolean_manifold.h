/*             B O O L E A N _ M A N I F O L D . H
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
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

#ifndef LIBBREP_BOOLEAN_MANIFOLD_H
#define LIBBREP_BOOLEAN_MANIFOLD_H

#include "common.h"

#include <cstddef>
#include <string>
#include <vector>

#include "brep/boolean.h"


enum BooleanManifoldSelection {
    BOOLEAN_MANIFOLD_ABSTAIN = -1,
    BOOLEAN_MANIFOLD_DISCARD = 0,
    BOOLEAN_MANIFOLD_KEEP = 1
};


struct BooleanManifoldDecision {
    enum BooleanManifoldSelection selection = BOOLEAN_MANIFOLD_ABSTAIN;
    /* One means source orientation, minus one means reversed, zero unknown. */
    int orientation = 0;
    double input_area = 0.0;
    double output_area = 0.0;
};


struct BooleanManifoldReport {
    std::vector<BooleanManifoldDecision> operand[2];
    size_t input_triangles = 0;
    size_t output_triangles = 0;
    size_t cdt_proxy_operands = 0;
    std::string detail;
};


/* Per-thread aggregate observations from the opt-in shadow classifier. */
struct BooleanManifoldShadowMetrics {
    size_t attempted_operations = 0;
    size_t classified_operations = 0;
    size_t abstained_operations = 0;
    size_t coplanar_operations = 0;
    size_t coplanar_classified_operations = 0;
    size_t oracle_decisions = 0;
    size_t decisions = 0;
    size_t region_abstentions = 0;
    size_t uncompared_decisions = 0;
    size_t coplanar_region_abstentions = 0;
    size_t uncertified_region_abstentions = 0;
    size_t selection_agreements = 0;
    size_t selection_disagreements = 0;
    size_t orientation_agreements = 0;
    size_t orientation_disagreements = 0;
    size_t certified_shared_boundaries = 0;
    size_t input_triangles = 0;
    size_t output_triangles = 0;
};


BREP_EXPORT void
boolean_manifold_shadow_metrics_reset();


BREP_EXPORT BooleanManifoldShadowMetrics
boolean_manifold_shadow_metrics();


/*
 * Evaluate already split, closed operand proxies.  Each proxy face is one
 * exact split-face region, so returned decisions are indexed by proxy face.
 * This is a conservative shadow classifier: partial or orientation-ambiguous
 * regions produce ABSTAIN rather than a guessed decision.
 */
BREP_EXPORT bool
boolean_manifold_classify(const ON_Brep &first, const ON_Brep &second,
	op_type operation, BooleanManifoldReport &report);

#endif /* LIBBREP_BOOLEAN_MANIFOLD_H */

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
