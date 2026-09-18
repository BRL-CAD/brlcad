/*                 C A N O N I C A L I Z E _ P R I V A T E . H
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 */

#ifndef LIBGED_CANONICALIZE_PRIVATE_H
#define LIBGED_CANONICALIZE_PRIVATE_H

#include "common.h"

#include "bn/tol.h"
#include "ged/defines.h"
#include "raytrace.h"

__BEGIN_DECLS

/* These helpers are private libged interfaces shared by commands that need to
 * recognize or reconstruct canonical primitive geometry. */
GED_EXPORT fastf_t _ged_matrix_roundoff_tolerance(fastf_t a, fastf_t b,
	fastf_t database_limit);

GED_EXPORT int _ged_matrices_numerically_equal(const mat_t a, const mat_t b,
	const struct bn_tol *tol);

GED_EXPORT int _ged_canonical_geometry_equal(
	const struct rt_db_internal *a, const struct rt_db_internal *b,
	const struct bn_tol *tol);

GED_EXPORT int _ged_canonical_geometry_metric(
	const struct rt_db_internal *object, const struct bn_tol *tol,
	fastf_t *metric, fastf_t *metric_tolerance);

GED_EXPORT int _ged_primitive_geometry_equal(
	const struct rt_db_internal *a, const struct rt_db_internal *b,
	const struct bn_tol *tol);

/* Allocate and populate output with input transformed by matrix.  Output must
 * be empty and remains empty on error. */
GED_EXPORT int _ged_transform_primitive(struct rt_db_internal *output,
	const mat_t matrix, const struct rt_db_internal *input);

GED_EXPORT int _ged_transformed_geometry_equal(
	const struct rt_db_internal *a, const mat_t a_matrix,
	const struct rt_db_internal *b, const mat_t b_matrix,
	const struct bn_tol *tol);

__END_DECLS

#ifdef __cplusplus

#include <map>
#include <string>
#include <vector>

struct GedCanonicalChildInfo {
    size_t identity = 0;
    mat_t placement = MAT_INIT_IDN;
};

struct GedCanonicalCombinationLeaf {
    size_t child_identity = 0;
    mat_t effective_matrix = MAT_INIT_IDN;
    mat_t relative_matrix = MAT_INIT_IDN;
};

struct GedCanonicalCombinationRecord {
    struct directory *dp = nullptr;
    mat_t placement = MAT_INIT_IDN;
    mat_t identity_placement = MAT_INIT_IDN;
    mat_t representative_to_input = MAT_INIT_IDN;
    std::string bucket;
    std::vector<GedCanonicalCombinationLeaf> leaves;
    fastf_t metric = 0.0;
    fastf_t metric_tolerance = 0.0;
    size_t identity = 0;
    bool region = false;
    bool valid = false;
};

struct GedCanonicalCombinationGroup {
    std::vector<size_t> records;
};

struct GedCanonicalCombinationAnalysis {
    std::vector<GedCanonicalCombinationRecord> records;
    std::vector<GedCanonicalCombinationGroup> groups;
    size_t failures = 0;
};

/* Identify equivalent combination DAG nodes bottom-up.  Primitive children
 * supply identities and canonical-to-input placements established by the
 * caller's selected transform mode.  Database metadata participates in
 * identity when include_metadata is true; geometry-only consumers such as
 * facetize deliberately ignore it. */
GED_EXPORT int _ged_canonical_combination_analysis(
    GedCanonicalCombinationAnalysis *analysis, struct db_i *dbip,
    const std::vector<struct directory *> &combinations,
    const std::map<struct directory *, GedCanonicalChildInfo> &primitive_children,
    const struct bn_tol *tol, bool include_metadata);

#endif

#endif /* LIBGED_CANONICALIZE_PRIVATE_H */
