/*                           R E U S E . H
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 */

#ifndef LIBGED_FACETIZE_REUSE_H
#define LIBGED_FACETIZE_REUSE_H

#include <map>
#include <set>
#include <string>
#include <vector>

#include "bn/tol.h"
#include "raytrace.h"

#include "../canonicalize_private.h"


struct FacetizeReuseMember {
    std::string name;
    mat_t representative_to_member = MAT_INIT_IDN;
};


struct FacetizeReuseGroup {
    std::string representative;
    std::vector<FacetizeReuseMember> members;
};


struct FacetizeReusePlan {
    std::vector<FacetizeReuseGroup> groups;
    std::map<struct directory *, GedCanonicalChildInfo> canonical_children;

    size_t reuse_count() const;
    void representatives(const std::vector<struct directory *> &inputs,
	std::vector<struct directory *> &outputs) const;
};


struct FacetizeRegionReuseMember {
    std::string name;
    mat_t representative_to_member = MAT_INIT_IDN;
};


struct FacetizeRegionReuseGroup {
    std::string representative;
    std::vector<FacetizeRegionReuseMember> members;
};


struct FacetizeRegionReusePlan {
    std::vector<FacetizeRegionReuseGroup> groups;

    size_t reuse_count() const;
    void representatives(const std::vector<struct directory *> &inputs,
	std::vector<struct directory *> &outputs) const;
};


/* Repeated non-region combinations selected for pre-evaluation.  Groups may
 * contain only a representative when one named subassembly is instanced more
 * than once. */
struct FacetizeIntermediateReusePlan {
    std::vector<FacetizeRegionReuseGroup> groups;
    size_t estimated_reuses = 0;

    size_t substitution_count() const;
    size_t reuse_count() const;
};


/* Find rigid-equivalent analytic primitives whose tessellation can be shared
 * without changing absolute tessellation error. */
int facetize_reuse_plan(FacetizeReusePlan *plan, struct db_i *dbip,
	const std::vector<struct directory *> &inputs,
	const struct bn_tol *tol);


/* Find region or implicit-root CSG trees whose Boolean result differs only by
 * a rigid transform.  Combination display and region metadata do not affect
 * the output mesh and are deliberately excluded from this identity. */
int facetize_region_reuse_plan(FacetizeRegionReusePlan *plan,
	struct db_i *dbip, const std::vector<struct directory *> &roots,
	const struct bn_tol *tol);


/* Select maximal repeated non-region subassemblies.  Evaluating only maximal
 * candidates avoids doing work for a repeated child whose repeated parent
 * result will replace it in the evaluation snapshot. */
int facetize_intermediate_reuse_plan(FacetizeIntermediateReusePlan *plan,
	struct db_i *dbip, const std::vector<struct directory *> &roots,
	const struct bn_tol *tol);


/* Replace selected combinations with leaves referencing pre-evaluated BoTs.
 * This is intended only for a disposable worker snapshot; the caller retains
 * the original working trees for validation and perturbation recovery. */
int facetize_intermediate_reuse_apply(const char *snapshot_file,
	const std::map<std::string, std::string> &substitutions,
	const char *result_file = nullptr);


/* Restore combination definitions changed by
 * facetize_intermediate_reuse_apply.  This converts the disposable snapshot
 * back into a valid recovery checkpoint before it is copied over a working
 * database after a writer failure. */
int facetize_intermediate_reuse_restore(const char *snapshot_file,
	struct db_i *source_dbip,
	const std::map<std::string, std::string> &substitutions);


/* Write transformed copies of successfully evaluated representative BoTs to
 * the preassigned output names for equivalent roots. */
int facetize_region_reuse_write_clones(const char *work_file,
	const FacetizeRegionReusePlan &plan,
	const std::map<std::string, std::string> &output_names,
	const std::set<std::string> &completed_representatives,
	std::vector<std::string> &failed_members, size_t *written_count,
	bool *write_unsafe);


/* Transform completed representative BoTs into their original member names.
 * A preparation failure reports that member for normal direct tessellation.
 * write_unsafe is set only after a working-database update has begun and
 * failed, allowing the caller's existing checkpoint recovery to take over. */
int facetize_reuse_write_clones(const char *work_file,
	const FacetizeReusePlan &plan,
	const std::set<std::string> &completed_representatives,
	std::vector<std::string> &failed_members, size_t *written_count,
	bool *write_unsafe);

#endif /* LIBGED_FACETIZE_REUSE_H */
