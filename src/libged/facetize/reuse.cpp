/*                         R E U S E . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 */

#include "common.h"

#include <algorithm>
#include <cstring>
#include <map>
#include <memory>
#include <set>

#include "rt/func.h"

#include "../canonicalize_private.h"
#include "./reuse.h"


namespace {

struct reuse_record {
    reuse_record()
    {
	RT_DB_INTERNAL_INIT(&input);
	RT_DB_INTERNAL_INIT(&canonical);
    }

    ~reuse_record()
    {
	if (canonical.idb_ptr)
	    rt_db_free_internal(&canonical);
	if (input.idb_ptr)
	    rt_db_free_internal(&input);
    }

    reuse_record(const reuse_record &) = delete;
    reuse_record &operator=(const reuse_record &) = delete;

    struct directory *dp = nullptr;
    struct rt_db_internal input;
    struct rt_db_internal canonical;
    mat_t placement = MAT_INIT_IDN;
    fastf_t metric = 0.0;
    fastf_t metric_tolerance = 0.0;
};


bool
reuse_type(int primitive_type)
{
    /* Halfspaces have no finite standalone tessellation to transform, and
     * existing BoTs already bypass analytic tessellation. */
    return primitive_type != ID_HALF && primitive_type != ID_BOT;
}


bool
make_member(FacetizeReuseMember &member, const reuse_record &representative,
	    const reuse_record &candidate, const struct bn_tol *tol)
{
    if (!_ged_canonical_geometry_equal(&representative.canonical,
	    &candidate.canonical, tol))
	return false;

    mat_t input_to_canonical;
    if (!bn_mat_inverse(input_to_canonical, representative.placement))
	return false;
    bn_mat_mul(member.representative_to_member, candidate.placement,
	input_to_canonical);
    if (!_ged_transformed_geometry_equal(&representative.input,
	    member.representative_to_member, &candidate.input,
	    bn_mat_identity, tol))
	return false;

    member.name = candidate.dp->d_namep;
    return true;
}


struct reuse_dag_state {
    struct db_i *dbip = nullptr;
    std::set<struct directory *> active;
    std::set<struct directory *> combinations;
    std::set<struct directory *> primitives;
    bool valid = true;
};


using combination_record_group =
    std::vector<const GedCanonicalCombinationRecord *>;


void collect_reuse_object(reuse_dag_state &state, struct directory *dp);


void
collect_reuse_tree(reuse_dag_state &state, const union tree *tree)
{
    if (!tree || !state.valid)
	return;

    RT_CK_TREE(tree);
    switch (tree->tr_op) {
	case OP_DB_LEAF: {
	    struct directory *child = db_lookup(state.dbip, tree->tr_l.tl_name,
		LOOKUP_QUIET);
	    if (!child) {
		state.valid = false;
		return;
	    }
	    collect_reuse_object(state, child);
	    return;
	}
	case OP_UNION:
	case OP_INTERSECT:
	case OP_SUBTRACT:
	case OP_XOR:
	    collect_reuse_tree(state, tree->tr_b.tb_left);
	    collect_reuse_tree(state, tree->tr_b.tb_right);
	    return;
	case OP_NOT:
	case OP_GUARD:
	case OP_XNOP:
	    collect_reuse_tree(state, tree->tr_b.tb_left);
	    return;
	case OP_NOP:
	    return;
	default:
	    state.valid = false;
	    return;
    }
}


void
collect_reuse_object(reuse_dag_state &state, struct directory *dp)
{
    if (!dp || !state.valid)
	return;
    if (!(dp->d_flags & RT_DIR_COMB)) {
	state.primitives.insert(dp);
	return;
    }
    if (state.active.find(dp) != state.active.end()) {
	state.valid = false;
	return;
    }
    if (state.combinations.find(dp) != state.combinations.end())
	return;
    state.active.insert(dp);

    struct rt_db_internal intern;
    RT_DB_INTERNAL_INIT(&intern);
    if (rt_db_get_internal(&intern, dp, state.dbip, nullptr) < 0 ||
	intern.idb_minor_type != ID_COMBINATION) {
	if (intern.idb_ptr)
	    rt_db_free_internal(&intern);
	state.active.erase(dp);
	state.valid = false;
	return;
    }
    state.combinations.insert(dp);
    const auto *comb = static_cast<const struct rt_comb_internal *>(intern.idb_ptr);
    RT_CK_COMB(comb);
    collect_reuse_tree(state, comb->tree);
    rt_db_free_internal(&intern);
    state.active.erase(dp);
}


bool
rigid_transform(const mat_t matrix, const struct bn_tol *tol)
{
    if (NEAR_ZERO(matrix[15], VDIVIDE_TOL) ||
	!NEAR_ZERO(matrix[12], tol->perp) ||
	!NEAR_ZERO(matrix[13], tol->perp) ||
	!NEAR_ZERO(matrix[14], tol->perp))
	return false;

    const vect_t basis[] = {
	{1.0, 0.0, 0.0},
	{0.0, 1.0, 0.0},
	{0.0, 0.0, 1.0}
    };
    vect_t transformed[3];
    for (size_t i = 0; i < 3; i++) {
	MAT4X3VEC(transformed[i], matrix, basis[i]);
	if (!NEAR_EQUAL(MAGNITUDE(transformed[i]), 1.0, tol->perp))
	    return false;
    }
    if (!NEAR_ZERO(VDOT(transformed[0], transformed[1]), tol->perp) ||
	!NEAR_ZERO(VDOT(transformed[1], transformed[2]), tol->perp) ||
	!NEAR_ZERO(VDOT(transformed[0], transformed[2]), tol->perp))
	return false;

    vect_t cross;
    VCROSS(cross, transformed[0], transformed[1]);
    return NEAR_EQUAL(VDOT(cross, transformed[2]), 1.0, tol->perp);
}


bool
record_transform(mat_t representative_to_member,
	const GedCanonicalCombinationRecord &representative,
	const GedCanonicalCombinationRecord &member, const struct bn_tol *tol)
{
    mat_t input_to_identity;
    if (!bn_mat_inverse(input_to_identity,
	    representative.representative_to_input))
	return false;
    bn_mat_mul(representative_to_member, member.representative_to_input,
	input_to_identity);
    return rigid_transform(representative_to_member, tol);
}


std::vector<combination_record_group>
rigid_record_groups(combination_record_group records,
	const struct bn_tol *tol)
{
    std::vector<combination_record_group> groups;
    std::sort(records.begin(), records.end(),
	[](const GedCanonicalCombinationRecord *a,
	   const GedCanonicalCombinationRecord *b) {
	    return std::strcmp(a->dp->d_namep, b->dp->d_namep) < 0;
	});

    while (!records.empty()) {
	combination_record_group group;
	group.push_back(records.front());
	records.erase(records.begin());
	for (auto candidate = records.begin(); candidate != records.end();) {
	    mat_t transform;
	    if (!record_transform(transform, *group.front(), **candidate,
		    tol)) {
		++candidate;
		continue;
	    }
	    group.push_back(*candidate);
	    candidate = records.erase(candidate);
	}
	groups.push_back(std::move(group));
    }
    return groups;
}


FacetizeRegionReuseGroup
make_reuse_group(const combination_record_group &records,
	const struct bn_tol *tol)
{
    FacetizeRegionReuseGroup group;
    if (records.empty())
	return group;
    group.representative = records.front()->dp->d_namep;
    for (size_t i = 1; i < records.size(); i++) {
	FacetizeRegionReuseMember member;
	member.name = records[i]->dp->d_namep;
	if (record_transform(member.representative_to_member,
		*records.front(), *records[i], tol))
	    group.members.push_back(member);
    }
    return group;
}


int
build_combination_analysis(GedCanonicalCombinationAnalysis &analysis,
	struct db_i *dbip, const std::vector<struct directory *> &roots,
	const struct bn_tol *tol)
{
    reuse_dag_state dag;
    dag.dbip = dbip;
    for (struct directory *root : roots)
	collect_reuse_object(dag, root);
    if (!dag.valid)
	return BRLCAD_ERROR;

    std::vector<struct directory *> primitives(dag.primitives.begin(),
	dag.primitives.end());
    std::sort(primitives.begin(), primitives.end(),
	[](const struct directory *a, const struct directory *b) {
	    return std::strcmp(a->d_namep, b->d_namep) < 0;
	});
    FacetizeReusePlan primitive_plan;
    if (facetize_reuse_plan(&primitive_plan, dbip, primitives, tol) !=
	    BRLCAD_OK)
	return BRLCAD_ERROR;

    return _ged_canonical_combination_analysis(&analysis, dbip, roots,
	primitive_plan.canonical_children, tol, false);
}


void count_combination_occurrences(struct db_i *dbip, struct directory *dp,
	std::map<struct directory *, size_t> &occurrences,
	std::set<struct directory *> &active);


void
count_tree_occurrences(struct db_i *dbip, const union tree *tree,
	std::map<struct directory *, size_t> &occurrences,
	std::set<struct directory *> &active)
{
    if (!tree)
	return;
    switch (tree->tr_op) {
	case OP_DB_LEAF: {
	    struct directory *child = db_lookup(dbip, tree->tr_l.tl_name,
		LOOKUP_QUIET);
	    if (child && (child->d_flags & RT_DIR_COMB))
		count_combination_occurrences(dbip, child, occurrences, active);
	    return;
	}
	case OP_UNION:
	case OP_INTERSECT:
	case OP_SUBTRACT:
	case OP_XOR:
	    count_tree_occurrences(dbip, tree->tr_b.tb_right, occurrences,
		active);
	    /* fall through */
	case OP_NOT:
	case OP_GUARD:
	case OP_XNOP:
	    count_tree_occurrences(dbip, tree->tr_b.tb_left, occurrences,
		active);
	    return;
	default:
	    return;
    }
}


void
count_combination_occurrences(struct db_i *dbip, struct directory *dp,
	std::map<struct directory *, size_t> &occurrences,
	std::set<struct directory *> &active)
{
    if (!dp || !(dp->d_flags & RT_DIR_COMB) || active.find(dp) != active.end())
	return;

    /* Counts saturate at two: candidate selection only needs to distinguish a
     * single evaluation from repeated work.  Stopping propagation after the
     * second visit also bounds traversal of heavily instanced DAGs. */
    size_t &count = occurrences[dp];
    if (count >= 2)
	return;
    count++;
    active.insert(dp);

    struct rt_db_internal intern;
    RT_DB_INTERNAL_INIT(&intern);
    if (rt_db_get_internal(&intern, dp, dbip, nullptr) >= 0 &&
	    intern.idb_minor_type == ID_COMBINATION) {
	const auto *comb =
	    static_cast<const struct rt_comb_internal *>(intern.idb_ptr);
	count_tree_occurrences(dbip, comb->tree, occurrences, active);
    }
    if (intern.idb_ptr)
	rt_db_free_internal(&intern);
    active.erase(dp);
}


void collect_maximal_candidates(struct db_i *dbip, struct directory *dp,
	const std::set<std::string> &candidates,
	std::set<std::string> &maximal, std::set<struct directory *> &active);


void
collect_maximal_tree(struct db_i *dbip, const union tree *tree,
	const std::set<std::string> &candidates,
	std::set<std::string> &maximal, std::set<struct directory *> &active)
{
    if (!tree)
	return;
    switch (tree->tr_op) {
	case OP_DB_LEAF: {
	    struct directory *child = db_lookup(dbip, tree->tr_l.tl_name,
		LOOKUP_QUIET);
	    if (!child || !(child->d_flags & RT_DIR_COMB))
		return;
	    if (candidates.find(child->d_namep) != candidates.end()) {
		maximal.insert(child->d_namep);
		return;
	    }
	    collect_maximal_candidates(dbip, child, candidates, maximal,
		active);
	    return;
	}
	case OP_UNION:
	case OP_INTERSECT:
	case OP_SUBTRACT:
	case OP_XOR:
	    collect_maximal_tree(dbip, tree->tr_b.tb_right, candidates,
		maximal, active);
	    /* fall through */
	case OP_NOT:
	case OP_GUARD:
	case OP_XNOP:
	    collect_maximal_tree(dbip, tree->tr_b.tb_left, candidates,
		maximal, active);
	    return;
	default:
	    return;
    }
}


void
collect_maximal_candidates(struct db_i *dbip, struct directory *dp,
	const std::set<std::string> &candidates,
	std::set<std::string> &maximal, std::set<struct directory *> &active)
{
    if (!dp || !(dp->d_flags & RT_DIR_COMB) || !active.insert(dp).second)
	return;
    struct rt_db_internal intern;
    RT_DB_INTERNAL_INIT(&intern);
    if (rt_db_get_internal(&intern, dp, dbip, nullptr) >= 0 &&
	    intern.idb_minor_type == ID_COMBINATION) {
	const auto *comb =
	    static_cast<const struct rt_comb_internal *>(intern.idb_ptr);
	collect_maximal_tree(dbip, comb->tree, candidates, maximal, active);
    }
    if (intern.idb_ptr)
	rt_db_free_internal(&intern);
    active.erase(dp);
}


int
replace_clone(struct db_i *dbip, const char *name,
	      struct rt_db_internal *clone, bool *write_unsafe)
{
    struct directory *old_dp = db_lookup(dbip, name, LOOKUP_QUIET);
    if (old_dp) {
	*write_unsafe = true;
	if (db_delete(dbip, old_dp) != 0 || db_dirdelete(dbip, old_dp) != 0)
	    return BRLCAD_ERROR;
    }

    *write_unsafe = true;
    struct directory *new_dp = db_diradd(dbip, name, RT_DIR_PHONY_ADDR, 0,
	RT_DIR_SOLID, (void *)&clone->idb_type);
    if (new_dp == RT_DIR_NULL)
	return BRLCAD_ERROR;
    if (rt_db_put_internal(new_dp, dbip, clone) < 0)
	return BRLCAD_ERROR;
    *write_unsafe = false;
    return BRLCAD_OK;
}


struct clone_request {
    std::string member_name;
    std::string target_name;
    mat_t transform = MAT_INIT_IDN;
};


int
write_clone_group(struct db_i *dbip, const char *representative_name,
		  const std::vector<clone_request> &requests,
		  std::vector<std::string> &failed_members,
		  size_t *written_count, bool *write_unsafe)
{
    struct directory *representative_dp = db_lookup(dbip,
	representative_name, LOOKUP_QUIET);
    struct rt_db_internal representative;
    RT_DB_INTERNAL_INIT(&representative);
    if (!representative_dp || representative_dp->d_minor_type != ID_BOT ||
	rt_db_get_internal(&representative, representative_dp, dbip,
	    nullptr) < 0) {
	for (const clone_request &request : requests)
	    failed_members.push_back(request.member_name);
	return BRLCAD_OK;
    }

    int result = BRLCAD_OK;
    for (const clone_request &request : requests) {
	struct rt_db_internal clone;
	RT_DB_INTERNAL_INIT(&clone);
	if (_ged_transform_primitive(&clone, request.transform,
		&representative) != BRLCAD_OK) {
	    failed_members.push_back(request.member_name);
	    continue;
	}
	bu_avs_merge(&clone.idb_avs, &representative.idb_avs);
	if (replace_clone(dbip, request.target_name.c_str(), &clone,
		write_unsafe) != BRLCAD_OK) {
	    if (clone.idb_ptr)
		rt_db_free_internal(&clone);
	    result = BRLCAD_ERROR;
	    break;
	}
	(*written_count)++;
    }
    if (representative.idb_ptr)
	rt_db_free_internal(&representative);
    return result;
}


template <typename Group>
size_t
reuse_count(const std::vector<Group> &groups)
{
    size_t count = 0;
    for (const Group &group : groups)
	count += group.members.size();
    return count;
}


template <typename Group>
void
representative_inputs(const std::vector<Group> &groups,
	const std::vector<struct directory *> &inputs,
	std::vector<struct directory *> &outputs)
{
    std::set<std::string> members;
    for (const Group &group : groups)
	for (const auto &member : group.members)
	    members.insert(member.name);

    outputs.clear();
    outputs.reserve(inputs.size() - std::min(inputs.size(), members.size()));
    for (struct directory *dp : inputs)
	if (dp && members.find(dp->d_namep) == members.end())
	    outputs.push_back(dp);
}

} /* namespace */


size_t
FacetizeReusePlan::reuse_count() const
{
    return ::reuse_count(groups);
}


void
FacetizeReusePlan::representatives(
	const std::vector<struct directory *> &inputs,
	std::vector<struct directory *> &outputs) const
{
    representative_inputs(groups, inputs, outputs);
}


size_t
FacetizeRegionReusePlan::reuse_count() const
{
    return ::reuse_count(groups);
}


void
FacetizeRegionReusePlan::representatives(
	const std::vector<struct directory *> &inputs,
	std::vector<struct directory *> &outputs) const
{
    representative_inputs(groups, inputs, outputs);
}


size_t
FacetizeIntermediateReusePlan::substitution_count() const
{
    size_t count = 0;
    for (const FacetizeRegionReuseGroup &group : groups)
	count += 1 + group.members.size();
    return count;
}


size_t
FacetizeIntermediateReusePlan::reuse_count() const
{
    return estimated_reuses;
}


int
facetize_reuse_plan(FacetizeReusePlan *plan, struct db_i *dbip,
	const std::vector<struct directory *> &inputs,
	const struct bn_tol *tol)
{
    if (!plan || !dbip || !tol)
	return BRLCAD_ERROR;
    plan->groups.clear();
    plan->canonical_children.clear();

    std::vector<std::unique_ptr<reuse_record>> records;
    std::map<int, std::multimap<fastf_t, size_t>> buckets;
    for (struct directory *dp : inputs) {
	if (!dp || dp->d_major_type != DB5_MAJORTYPE_BRLCAD ||
	    dp->d_minor_type <= ID_NULL || dp->d_minor_type > ID_MAXIMUM ||
	    dp->d_minor_type == ID_BOT ||
	    !OBJ[dp->d_minor_type].ft_canonicalize)
	    continue;

	auto record = std::make_unique<reuse_record>();
	record->dp = dp;
	if (rt_db_get_internal(&record->input, dp, dbip, nullptr) < 0 ||
	    rt_obj_canonicalize(&record->canonical, record->placement,
		&record->input, tol, RT_CANONICALIZE_RIGID) !=
		RT_CANONICALIZE_OK ||
	    !_ged_canonical_geometry_metric(&record->canonical, tol,
		&record->metric, &record->metric_tolerance))
	    continue;

	size_t index = records.size();
	buckets[record->canonical.idb_minor_type].emplace(record->metric,
	    index);
	records.push_back(std::move(record));
    }

    std::vector<size_t> record_identities(records.size());
    for (size_t i = 0; i < records.size(); i++)
	record_identities[i] = i + 1;

    for (auto &bucket : buckets) {
	auto &remaining = bucket.second;
	while (remaining.size() > 1) {
	    size_t representative_index = remaining.begin()->second;
	    remaining.erase(remaining.begin());
	    const reuse_record &representative = *records[representative_index];

	    FacetizeReuseGroup group;
	    group.representative = representative.dp->d_namep;
	    fastf_t maximum_metric = representative.metric +
		representative.metric_tolerance;
	    auto candidate_end = remaining.upper_bound(maximum_metric);
	    for (auto candidate_it = remaining.begin();
		    candidate_it != candidate_end;) {
		const reuse_record &candidate = *records[candidate_it->second];
		FacetizeReuseMember member;
		if (make_member(member, representative, candidate, tol)) {
		    record_identities[candidate_it->second] =
			record_identities[representative_index];
		    group.members.push_back(member);
		    candidate_it = remaining.erase(candidate_it);
		} else {
		    ++candidate_it;
		}
	    }
	    if (!group.members.empty() &&
		reuse_type(representative.canonical.idb_minor_type))
		plan->groups.push_back(group);
	}
    }

    size_t next_identity = records.size() + 1;
    for (size_t i = 0; i < records.size(); i++) {
	GedCanonicalChildInfo child;
	child.identity = record_identities[i];
	MAT_COPY(child.placement, records[i]->placement);
	plan->canonical_children[records[i]->dp] = child;
    }
    for (struct directory *dp : inputs) {
	if (!dp || plan->canonical_children.find(dp) !=
		plan->canonical_children.end())
	    continue;
	GedCanonicalChildInfo child;
	child.identity = next_identity++;
	plan->canonical_children[dp] = child;
    }

    return BRLCAD_OK;
}


int
facetize_region_reuse_plan(FacetizeRegionReusePlan *plan,
	struct db_i *dbip, const std::vector<struct directory *> &roots,
	const struct bn_tol *tol)
{
    if (!plan || !dbip || !tol)
	return BRLCAD_ERROR;
    plan->groups.clear();

    GedCanonicalCombinationAnalysis analysis;
    if (build_combination_analysis(analysis, dbip, roots, tol) != BRLCAD_OK)
	return BRLCAD_OK;

    std::set<struct directory *> root_set(roots.begin(), roots.end());
    std::map<size_t, std::vector<const GedCanonicalCombinationRecord *>>
	identity_roots;
    for (const GedCanonicalCombinationRecord &record : analysis.records) {
	if (record.valid && root_set.find(record.dp) != root_set.end())
	    identity_roots[record.identity].push_back(&record);
    }

    for (auto &identity : identity_roots) {
	if (identity.second.size() < 2)
	    continue;
	for (const combination_record_group &records :
		rigid_record_groups(identity.second, tol)) {
	    if (records.size() > 1)
		plan->groups.push_back(make_reuse_group(records, tol));
	}
    }
    return BRLCAD_OK;
}


int
facetize_intermediate_reuse_plan(FacetizeIntermediateReusePlan *plan,
	struct db_i *dbip, const std::vector<struct directory *> &roots,
	const struct bn_tol *tol)
{
    if (!plan || !dbip || !tol)
	return BRLCAD_ERROR;
    plan->groups.clear();
    plan->estimated_reuses = 0;

    GedCanonicalCombinationAnalysis analysis;
    if (build_combination_analysis(analysis, dbip, roots, tol) != BRLCAD_OK)
	return BRLCAD_OK;

    std::set<struct directory *> root_set(roots.begin(), roots.end());
    std::map<struct directory *, size_t> occurrences;
    for (struct directory *root : roots) {
	std::set<struct directory *> active;
	count_combination_occurrences(dbip, root, occurrences, active);
    }

    std::map<size_t, combination_record_group> identity_records;

    for (const GedCanonicalCombinationRecord &record : analysis.records) {
	if (record.valid && !record.region && record.leaves.size() > 1 &&
		root_set.find(record.dp) == root_set.end())
	    identity_records[record.identity].push_back(&record);
    }

    std::vector<combination_record_group> qualifying_groups;
    std::set<std::string> candidate_names;
    for (auto &identity : identity_records) {
	for (combination_record_group &records :
		rigid_record_groups(identity.second, tol)) {
	    bool repeated = records.size() > 1;
	    if (!repeated && !records.empty())
		repeated = occurrences[records.front()->dp] > 1;
	    if (!repeated)
		continue;
	    for (const GedCanonicalCombinationRecord *record : records)
		candidate_names.insert(record->dp->d_namep);
	    qualifying_groups.push_back(std::move(records));
	}
    }

    /* A selected parent result already contains all of its descendants.
     * Retain a child candidate only if it is also reached on a path without a
     * selected ancestor. */
    std::set<std::string> maximal_names;
    for (struct directory *root : roots) {
	std::set<struct directory *> active;
	collect_maximal_candidates(dbip, root, candidate_names, maximal_names,
	    active);
    }

    for (const combination_record_group &records : qualifying_groups) {
	combination_record_group maximal_records;
	for (const GedCanonicalCombinationRecord *record : records) {
	    if (maximal_names.find(record->dp->d_namep) != maximal_names.end())
		maximal_records.push_back(record);
	}
	if (maximal_records.empty())
	    continue;
	if (maximal_records.size() == 1 &&
		occurrences[maximal_records.front()->dp] < 2)
	    continue;
	size_t evaluations = 0;
	for (const GedCanonicalCombinationRecord *record : maximal_records)
	    evaluations += occurrences[record->dp];
	if (evaluations < 2)
	    continue;
	plan->groups.push_back(make_reuse_group(maximal_records, tol));
	plan->estimated_reuses += evaluations - 1;
    }
    return BRLCAD_OK;
}


int
facetize_intermediate_reuse_apply(const char *snapshot_file,
	const std::map<std::string, std::string> &substitutions,
	const char *result_file)
{
    if (!snapshot_file)
	return BRLCAD_ERROR;
    if (substitutions.empty())
	return BRLCAD_OK;

    struct db_i *dbip = db_open(snapshot_file, DB_OPEN_READWRITE);
    if (!dbip || db_dirbuild(dbip) < 0) {
	if (dbip)
	    db_close(dbip);
	return BRLCAD_ERROR;
    }

    struct db_i *result_dbip = nullptr;
    if (result_file) {
	result_dbip = db_open(result_file, DB_OPEN_READONLY);
	if (!result_dbip || db_dirbuild(result_dbip) < 0) {
	    if (result_dbip)
		db_close(result_dbip);
	    db_close(dbip);
	    return BRLCAD_ERROR;
	}
    }

    int result = BRLCAD_OK;
    for (const auto &substitution : substitutions) {
	struct directory *combination_dp = db_lookup(dbip,
	    substitution.first.c_str(), LOOKUP_QUIET);
	struct directory *result_dp = db_lookup(dbip,
	    substitution.second.c_str(), LOOKUP_QUIET);
	if (!result_dp && result_dbip) {
	    struct directory *source_dp = db_lookup(result_dbip,
		substitution.second.c_str(), LOOKUP_QUIET);
	    struct rt_db_internal result_internal;
	    RT_DB_INTERNAL_INIT(&result_internal);
	    if (!source_dp || source_dp->d_minor_type != ID_BOT ||
		    rt_db_get_internal(&result_internal, source_dp, result_dbip,
			nullptr) < 0) {
		if (result_internal.idb_ptr)
		    rt_db_free_internal(&result_internal);
		result = BRLCAD_ERROR;
		break;
	    }
	    result_dp = db_diradd(dbip, substitution.second.c_str(),
		RT_DIR_PHONY_ADDR, 0, RT_DIR_SOLID,
		(void *)&result_internal.idb_type);
	    if (!result_dp || rt_db_put_internal(result_dp, dbip,
		    &result_internal) < 0) {
		if (result_internal.idb_ptr)
		    rt_db_free_internal(&result_internal);
		result = BRLCAD_ERROR;
		break;
	    }
	}
	struct rt_db_internal intern;
	RT_DB_INTERNAL_INIT(&intern);
	if (!combination_dp || !(combination_dp->d_flags & RT_DIR_COMB) ||
		!result_dp || result_dp->d_minor_type != ID_BOT ||
		rt_db_get_internal(&intern, combination_dp, dbip, nullptr) < 0 ||
		intern.idb_minor_type != ID_COMBINATION) {
	    if (intern.idb_ptr)
		rt_db_free_internal(&intern);
	    result = BRLCAD_ERROR;
	    break;
	}

	auto *comb = static_cast<struct rt_comb_internal *>(intern.idb_ptr);
	db_free_tree(comb->tree);
	BU_GET(comb->tree, union tree);
	RT_TREE_INIT(comb->tree);
	comb->tree->tr_l.tl_op = OP_DB_LEAF;
	comb->tree->tr_l.tl_name = bu_strdup(substitution.second.c_str());
	comb->tree->tr_l.tl_mat = nullptr;
	if (rt_db_put_internal(combination_dp, dbip, &intern) < 0) {
	    if (intern.idb_ptr)
		rt_db_free_internal(&intern);
	    result = BRLCAD_ERROR;
	    break;
	}
    }
    if (result == BRLCAD_OK)
	db_update_nref(dbip);
    if (result_dbip)
	db_close(result_dbip);
    db_close(dbip);
    return result;
}


int
facetize_intermediate_reuse_restore(const char *snapshot_file,
	struct db_i *source_dbip,
	const std::map<std::string, std::string> &substitutions)
{
    if (!snapshot_file || !source_dbip)
	return BRLCAD_ERROR;
    if (substitutions.empty())
	return BRLCAD_OK;

    struct db_i *snapshot_dbip = db_open(snapshot_file, DB_OPEN_READWRITE);
    if (!snapshot_dbip || db_dirbuild(snapshot_dbip) < 0) {
	if (snapshot_dbip)
	    db_close(snapshot_dbip);
	return BRLCAD_ERROR;
    }

    int result = BRLCAD_OK;
    for (const auto &substitution : substitutions) {
	struct directory *source_dp = db_lookup(source_dbip,
	    substitution.first.c_str(), LOOKUP_QUIET);
	struct directory *snapshot_dp = db_lookup(snapshot_dbip,
	    substitution.first.c_str(), LOOKUP_QUIET);
	struct rt_db_internal intern;
	RT_DB_INTERNAL_INIT(&intern);
	if (!source_dp || !(source_dp->d_flags & RT_DIR_COMB) || !snapshot_dp ||
		rt_db_get_internal(&intern, source_dp, source_dbip, nullptr) < 0 ||
		intern.idb_minor_type != ID_COMBINATION) {
	    if (intern.idb_ptr)
		rt_db_free_internal(&intern);
	    result = BRLCAD_ERROR;
	    break;
	}
	if (rt_db_put_internal(snapshot_dp, snapshot_dbip, &intern) < 0) {
	    if (intern.idb_ptr)
		rt_db_free_internal(&intern);
	    result = BRLCAD_ERROR;
	    break;
	}
    }
    if (result == BRLCAD_OK)
	db_update_nref(snapshot_dbip);
    db_close(snapshot_dbip);
    return result;
}


int
facetize_reuse_write_clones(const char *work_file,
	const FacetizeReusePlan &plan,
	const std::set<std::string> &completed_representatives,
	std::vector<std::string> &failed_members, size_t *written_count,
	bool *write_unsafe)
{
    if (!work_file || !written_count || !write_unsafe)
	return BRLCAD_ERROR;
    failed_members.clear();
    *written_count = 0;
    *write_unsafe = false;
    if (plan.groups.empty())
	return BRLCAD_OK;

    struct db_i *dbip = db_open(work_file, DB_OPEN_READWRITE);
    if (!dbip || db_dirbuild(dbip) < 0) {
	if (dbip)
	    db_close(dbip);
	return BRLCAD_ERROR;
    }

    int result = BRLCAD_OK;
    for (const FacetizeReuseGroup &group : plan.groups) {
	if (completed_representatives.find(group.representative) ==
		completed_representatives.end())
	    continue;

	std::vector<clone_request> requests;
	for (const FacetizeReuseMember &member : group.members) {
	    clone_request request;
	    request.member_name = member.name;
	    request.target_name = member.name;
	    MAT_COPY(request.transform, member.representative_to_member);
	    requests.push_back(request);
	}
	result = write_clone_group(dbip, group.representative.c_str(),
	    requests, failed_members, written_count, write_unsafe);
	if (result != BRLCAD_OK)
	    break;
    }

    db_close(dbip);
    return result;
}


int
facetize_region_reuse_write_clones(const char *work_file,
	const FacetizeRegionReusePlan &plan,
	const std::map<std::string, std::string> &output_names,
	const std::set<std::string> &completed_representatives,
	std::vector<std::string> &failed_members, size_t *written_count,
	bool *write_unsafe)
{
    if (!work_file || !written_count || !write_unsafe)
	return BRLCAD_ERROR;
    failed_members.clear();
    *written_count = 0;
    *write_unsafe = false;
    if (plan.groups.empty())
	return BRLCAD_OK;

    struct db_i *dbip = db_open(work_file, DB_OPEN_READWRITE);
    if (!dbip || db_dirbuild(dbip) < 0) {
	if (dbip)
	    db_close(dbip);
	return BRLCAD_ERROR;
    }

    int result = BRLCAD_OK;
    for (const FacetizeRegionReuseGroup &group : plan.groups) {
	if (completed_representatives.find(group.representative) ==
		completed_representatives.end())
	    continue;
	auto representative_output = output_names.find(group.representative);
	if (representative_output == output_names.end()) {
	    for (const FacetizeRegionReuseMember &member : group.members)
		failed_members.push_back(member.name);
	    continue;
	}

	std::vector<clone_request> requests;
	for (const FacetizeRegionReuseMember &member : group.members) {
	    auto member_output = output_names.find(member.name);
	    if (member_output == output_names.end()) {
		failed_members.push_back(member.name);
		continue;
	    }
	    clone_request request;
	    request.member_name = member.name;
	    request.target_name = member_output->second;
	    MAT_COPY(request.transform, member.representative_to_member);
	    requests.push_back(request);
	}
	result = write_clone_group(dbip, representative_output->second.c_str(),
	    requests, failed_members, written_count, write_unsafe);
	if (result != BRLCAD_OK)
	    break;
    }

    db_close(dbip);
    return result;
}
