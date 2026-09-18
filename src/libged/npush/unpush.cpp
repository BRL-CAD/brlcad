/*                      U N P U S H . C P P
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
/** @file libged/npush/unpush.cpp
 *
 * Restore shared primitive definitions from pushed geometry.  Analysis and
 * reference accounting are completed before recoverable database rewrites.
 */

#include "common.h"

#include <algorithm>
#include <cstring>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "bu/opt.h"
#include "rt/func.h"

#include "../canonicalize_private.h"
#include "../ged_private.h"


namespace {

constexpr const char *CANONICAL_NAME_PREFIX = "unpush_";


struct unpush_reference {
    struct directory *parent = nullptr;
    struct directory *child = nullptr;
    mat_t matrix = MAT_INIT_IDN;
};


struct unpush_walk_state {
    struct db_i *dbip = nullptr;
    std::set<struct directory *> active_combinations;
    std::set<struct directory *> visited_combinations;
    std::set<struct directory *> primitives;
    std::set<struct directory *> root_primitives;
    std::set<struct directory *> root_combinations;
    std::map<struct directory *, size_t> selected_references;
    std::map<struct directory *, std::vector<struct directory *>> children;
    std::vector<unpush_reference> references;
    size_t missing_references = 0;
    bool cycle_found = false;
    bool read_error = false;
};


struct unpush_primitive_record {
    struct directory *dp = nullptr;
    mat_t canonical_to_input = MAT_INIT_IDN;
    std::string bucket;
    fastf_t metric = 0.0;
    fastf_t metric_tolerance = 0.0;
};


struct unpush_group {
    std::vector<size_t> records;
};


using canonical_combination_record = GedCanonicalCombinationRecord;
using combination_group = GedCanonicalCombinationGroup;
using combination_analysis_result = GedCanonicalCombinationAnalysis;


struct indexed_reference {
    struct directory *parent = nullptr;
    mat_t matrix = MAT_INIT_IDN;
};


struct database_reference_index {
    const std::set<struct directory *> *targets = nullptr;
    std::map<struct directory *, std::vector<indexed_reference>> references;
};


struct rewrite_target {
    std::string canonical_name;
    mat_t canonical_to_input = MAT_INIT_IDN;
};


struct rewritten_leaf {
    std::string path;
    std::string name;
    mat_t matrix = MAT_INIT_IDN;
};


struct planned_group {
    std::vector<size_t> records;
    std::string canonical_name;
};


struct planned_combination_group {
    size_t representative = 0;
    std::vector<size_t> records;
};


class canonical_object {
public:
    canonical_object(struct db_i *dbip, struct directory *dp,
		     const struct bn_tol *tol, enum rt_canonicalize_mode mode)
    {
	RT_DB_INTERNAL_INIT(&input);
	RT_DB_INTERNAL_INIT(&canonical);
	if (rt_db_get_internal(&input, dp, dbip, nullptr) < 0) {
	    status = RT_CANONICALIZE_ERROR;
	    return;
	}
	input_loaded = true;
	status = rt_obj_canonicalize(&canonical, placement, &input, tol, mode);
    }

    canonical_object(const canonical_object &) = delete;
    canonical_object &operator=(const canonical_object &) = delete;

    ~canonical_object()
    {
	if (canonical.idb_ptr)
	    rt_db_free_internal(&canonical);
	if (input_loaded)
	    rt_db_free_internal(&input);
    }

    struct rt_db_internal input;
    struct rt_db_internal canonical;
    mat_t placement = MAT_INIT_IDN;
    int status = RT_CANONICALIZE_ERROR;

private:
    bool input_loaded = false;
};


void
collect_database_reference(struct db_i *UNUSED(dbip), struct directory *parent,
			   struct directory *child, const char *UNUSED(child_name),
			   db_op_t UNUSED(operation), matp_t matrix,
			   void *data)
{
    auto *index = static_cast<database_reference_index *>(data);

    if (!index || !index->targets || !parent || !child ||
	index->targets->find(child) == index->targets->end())
	return;

    if (parent->d_flags & RT_DIR_COMB) {
	indexed_reference reference;
	reference.parent = parent;
	if (matrix)
	    MAT_COPY(reference.matrix, matrix);
	index->references[child].push_back(reference);
    }
}


void collect_object(unpush_walk_state &state, struct directory *dp);


void
collect_tree(unpush_walk_state &state, union tree *tree, struct directory *parent)
{
    if (!tree || state.cycle_found || state.read_error)
	return;

    RT_CK_TREE(tree);
    switch (tree->tr_op) {
	case OP_DB_LEAF: {
	    struct directory *child = db_lookup(state.dbip, tree->tr_l.tl_name, LOOKUP_QUIET);
	    if (child == RT_DIR_NULL) {
		state.missing_references++;
		return;
	    }
	    state.selected_references[child]++;
	    state.children[parent].push_back(child);
	    unpush_reference reference;
	    reference.parent = parent;
	    reference.child = child;
	    if (tree->tr_l.tl_mat)
		MAT_COPY(reference.matrix, tree->tr_l.tl_mat);
	    state.references.push_back(reference);
	    collect_object(state, child);
	    return;
	}
	case OP_UNION:
	case OP_INTERSECT:
	case OP_SUBTRACT:
	case OP_XOR:
	    collect_tree(state, tree->tr_b.tb_left, parent);
	    collect_tree(state, tree->tr_b.tb_right, parent);
	    return;
	case OP_NOT:
	case OP_GUARD:
	case OP_XNOP:
	    collect_tree(state, tree->tr_b.tb_left, parent);
	    return;
	case OP_NOP:
	    return;
	default:
	    state.read_error = true;
	    return;
    }
}


void
collect_object(unpush_walk_state &state, struct directory *dp)
{
    if (!(dp->d_flags & RT_DIR_COMB)) {
	state.primitives.insert(dp);
	return;
    }
    if (state.active_combinations.find(dp) != state.active_combinations.end()) {
	state.cycle_found = true;
	return;
    }
    if (state.visited_combinations.find(dp) != state.visited_combinations.end())
	return;

    state.active_combinations.insert(dp);
    state.visited_combinations.insert(dp);

    struct rt_db_internal intern;
    RT_DB_INTERNAL_INIT(&intern);
    if (rt_db_get_internal(&intern, dp, state.dbip, nullptr) < 0) {
	state.read_error = true;
	state.active_combinations.erase(dp);
	return;
    }
    const struct rt_comb_internal *comb = static_cast<const struct rt_comb_internal *>(intern.idb_ptr);
    RT_CK_COMB(comb);
    collect_tree(state, comb->tree, dp);
    rt_db_free_internal(&intern);

    state.active_combinations.erase(dp);
}


std::string
canonical_bucket(const canonical_object &object)
{
    std::ostringstream key;
    key << object.canonical.idb_minor_type << ':' << object.input.idb_avs.count;

    switch (object.canonical.idb_minor_type) {
	case ID_BOT: {
	    const auto *bot = static_cast<const struct rt_bot_internal *>(object.canonical.idb_ptr);
	    key << ':' << static_cast<unsigned int>(bot->mode)
		<< ':' << static_cast<unsigned int>(bot->orientation)
		<< ':' << static_cast<unsigned int>(bot->bot_flags)
		<< ':' << bot->num_vertices << ':' << bot->num_faces
		<< ':' << bot->num_normals << ':' << bot->num_face_normals
		<< ':' << bot->num_uvs << ':' << bot->num_face_uvs;
	    break;
	}
	default:
	    break;
    }

    return key.str();
}

void
canonical_metric(const canonical_object &object, const struct bn_tol *tol,
		 fastf_t &metric, fastf_t &metric_tolerance)
{
    (void)_ged_canonical_geometry_metric(&object.canonical, tol, &metric,
	&metric_tolerance);
}


const struct bu_attribute_value_pair *
find_attribute(const struct bu_attribute_value_set *attributes, const char *name)
{
    for (size_t i = 0; i < attributes->count; i++) {
	if (BU_STR_EQUAL(attributes->avp[i].name, name))
	    return &attributes->avp[i];
    }
    return nullptr;
}


bool
attributes_equal(const struct bu_attribute_value_set *a,
		 const struct bu_attribute_value_set *b)
{
    if (a->count != b->count)
	return false;

    for (size_t i = 0; i < a->count; i++) {
	const struct bu_attribute_value_pair *other = find_attribute(b, a->avp[i].name);
	if (!other)
	    return false;
	if ((a->avp[i].value == nullptr) != (other->value == nullptr))
	    return false;
	if (a->avp[i].value && !BU_STR_EQUAL(a->avp[i].value, other->value))
	    return false;
#if defined(USE_BINARY_ATTRIBUTES)
	if (a->avp[i].binvaluelen != other->binvaluelen)
	    return false;
	if (a->avp[i].binvaluelen &&
	    std::memcmp(a->avp[i].binvalue, other->binvalue, a->avp[i].binvaluelen) != 0)
	    return false;
#endif
    }

    return true;
}


bool
canonical_reconstructs_references(const canonical_object &representative,
				  const canonical_object &candidate,
				  const std::vector<indexed_reference> &references,
				  bool root_primitive,
				  const struct bn_tol *tol)
{
    if (!_ged_canonical_geometry_equal(&representative.canonical,
	    &candidate.canonical, tol))
	return false;

    /* Validate in the direct parent's coordinate system.  This catches the
     * common case where an existing leaf scale would magnify an otherwise
     * tolerable difference between canonical objects. */
    auto reference_is_preserved = [&](const mat_t input_matrix) {
	mat_t replacement_matrix;
	bn_mat_mul(replacement_matrix, input_matrix, candidate.placement);
	return _ged_transformed_geometry_equal(&representative.canonical,
	    replacement_matrix, &candidate.input, input_matrix, tol);
    };

    if (root_primitive && !reference_is_preserved(bn_mat_identity))
	return false;
    if (!root_primitive && references.empty())
	return false;
    for (const indexed_reference &reference : references) {
	if (!reference_is_preserved(reference.matrix))
	    return false;
    }
    return true;
}


std::vector<unpush_group>
verify_groups(struct db_i *dbip, const std::vector<unpush_primitive_record> &records,
	      const std::map<std::string, std::vector<size_t>> &buckets,
	      const std::map<struct directory *, std::vector<indexed_reference>> &references,
	      const std::set<struct directory *> &root_primitives,
	      const struct bn_tol *tol, enum rt_canonicalize_mode mode)
{
    std::vector<unpush_group> groups;

    for (const auto &bucket : buckets) {
	if (bucket.second.size() < 2)
	    continue;

	std::multimap<fastf_t, size_t> remaining;
	for (size_t record_index : bucket.second)
	    remaining.emplace(records[record_index].metric, record_index);
	while (remaining.size() > 1) {
	    size_t representative_index = remaining.begin()->second;
	    remaining.erase(remaining.begin());
	    canonical_object representative(dbip, records[representative_index].dp, tol, mode);
	    auto representative_references = references.find(records[representative_index].dp);
	    const std::vector<indexed_reference> no_references;
	    const auto &representative_arcs = representative_references == references.end() ?
		no_references : representative_references->second;
	    if (representative.status != RT_CANONICALIZE_OK ||
		!canonical_reconstructs_references(representative, representative,
		    representative_arcs,
		    root_primitives.find(records[representative_index].dp) !=
			root_primitives.end(), tol))
		continue;

	    unpush_group group;
	    group.records.push_back(representative_index);
	    fastf_t maximum_metric = records[representative_index].metric +
		records[representative_index].metric_tolerance;
	    auto candidate_end = remaining.upper_bound(maximum_metric);
	    for (auto candidate_it = remaining.begin(); candidate_it != candidate_end;) {
		canonical_object candidate(dbip, records[candidate_it->second].dp, tol, mode);
		auto candidate_references = references.find(records[candidate_it->second].dp);
		const auto &candidate_arcs = candidate_references == references.end() ?
		    no_references : candidate_references->second;
		if (candidate.status == RT_CANONICALIZE_OK &&
		    attributes_equal(&representative.input.idb_avs, &candidate.input.idb_avs) &&
		    canonical_reconstructs_references(representative, candidate,
			candidate_arcs,
			root_primitives.find(records[candidate_it->second].dp) !=
			    root_primitives.end(), tol)) {
		    group.records.push_back(candidate_it->second);
		    candidate_it = remaining.erase(candidate_it);
		} else {
		    ++candidate_it;
		}
	    }
	    if (group.records.size() > 1)
		groups.push_back(group);
	}
    }

    return groups;
}

combination_analysis_result
analyze_combinations(struct db_i *dbip,
		     const std::set<struct directory *> &combinations,
		     const std::vector<unpush_primitive_record> &primitive_records,
		     const std::vector<unpush_group> &primitive_groups,
		     const struct bn_tol *tol)
{
    std::map<struct directory *, size_t> primitive_identities;
    size_t next_primitive_identity = 1;
    for (const unpush_group &group : primitive_groups) {
	size_t identity = next_primitive_identity++;
	for (size_t record_index : group.records)
	    primitive_identities[primitive_records[record_index].dp] = identity;
    }

    std::map<struct directory *, GedCanonicalChildInfo> primitive_children;
    for (const unpush_primitive_record &primitive : primitive_records) {
	auto identity = primitive_identities.find(primitive.dp);
	if (identity == primitive_identities.end()) {
	    size_t new_identity = next_primitive_identity++;
	    identity = primitive_identities.emplace(primitive.dp,
		new_identity).first;
	}
	GedCanonicalChildInfo child;
	child.identity = identity->second;
	MAT_COPY(child.placement, primitive.canonical_to_input);
	primitive_children[primitive.dp] = child;
    }

    std::vector<struct directory *> ordered_combinations(combinations.begin(),
	combinations.end());
    combination_analysis_result result;
    if (_ged_canonical_combination_analysis(&result, dbip,
	    ordered_combinations, primitive_children, tol, true) !=
	BRLCAD_OK)
	result.failures++;
    return result;
}


void
mark_exposed_descendants(struct directory *dp,
			 const std::map<struct directory *, std::vector<struct directory *>> &children,
			 std::set<struct directory *> &exposed)
{
    auto child_it = children.find(dp);
    if (child_it == children.end())
	return;
    for (struct directory *child : child_it->second) {
	if (exposed.insert(child).second)
	    mark_exposed_descendants(child, children, exposed);
    }
}


bool
rewrite_tree_at_path(union tree *tree,
		     const std::map<std::string, rewrite_target> &targets,
		     const struct bn_tol *tol,
		     std::vector<rewritten_leaf> &rewritten,
		     std::string &path)
{
    if (!tree)
	return true;

    RT_CK_TREE(tree);
    switch (tree->tr_op) {
	case OP_DB_LEAF: {
	    auto target_it = targets.find(tree->tr_l.tl_name);
	    if (target_it == targets.end())
		return true;

	    mat_t input_matrix;
	    mat_t replacement;
	    if (tree->tr_l.tl_mat)
		MAT_COPY(input_matrix, tree->tr_l.tl_mat);
	    else
		MAT_IDN(input_matrix);
	    bn_mat_mul(replacement, input_matrix,
		target_it->second.canonical_to_input);

	    bu_free(tree->tr_l.tl_name, "unpush old leaf name");
	    tree->tr_l.tl_name = bu_strdup(target_it->second.canonical_name.c_str());
	    if (tree->tr_l.tl_mat) {
		bu_free(tree->tr_l.tl_mat, "unpush old leaf matrix");
		tree->tr_l.tl_mat = nullptr;
	    }
	    if (!bn_mat_is_equal(replacement, bn_mat_identity, tol))
		tree->tr_l.tl_mat = bn_mat_dup(replacement);

	    rewritten_leaf leaf;
	    leaf.path = path;
	    leaf.name = target_it->second.canonical_name;
	    MAT_COPY(leaf.matrix, replacement);
	    rewritten.push_back(leaf);
	    return true;
	}
	case OP_UNION:
	case OP_INTERSECT:
	case OP_SUBTRACT:
	case OP_XOR:
	    path.push_back('L');
	    if (!rewrite_tree_at_path(tree->tr_b.tb_left, targets, tol,
		    rewritten, path)) {
		path.pop_back();
		return false;
	    }
	    path.back() = 'R';
	    if (!rewrite_tree_at_path(tree->tr_b.tb_right, targets, tol,
		    rewritten, path)) {
		path.pop_back();
		return false;
	    }
	    path.pop_back();
	    return true;
	case OP_NOT:
	case OP_GUARD:
	case OP_XNOP: {
	    path.push_back('L');
	    bool rewritten_child = rewrite_tree_at_path(tree->tr_b.tb_left,
		targets, tol, rewritten, path);
	    path.pop_back();
	    return rewritten_child;
	}
	case OP_NOP:
	    return true;
	default:
	    return false;
    }
}


bool
rewrite_tree(union tree *tree,
	     const std::map<std::string, rewrite_target> &targets,
	     const struct bn_tol *tol, std::vector<rewritten_leaf> &rewritten)
{
    std::string path;
    return rewrite_tree_at_path(tree, targets, tol, rewritten, path);
}


const union tree *
tree_at_path(const union tree *tree, const std::string &path)
{
    for (char direction : path) {
	if (!tree)
	    return nullptr;
	RT_CK_TREE(tree);
	if (direction == 'L')
	    tree = tree->tr_b.tb_left;
	else if (direction == 'R')
	    tree = tree->tr_b.tb_right;
	else
	    return nullptr;
    }
    return tree;
}


void
free_internals(std::map<struct directory *, struct rt_db_internal *> &internals)
{
    for (auto &entry : internals) {
	rt_db_free_internal(entry.second);
	BU_PUT(entry.second, struct rt_db_internal);
    }
    internals.clear();
}


bool
remove_object(struct db_i *dbip, const std::string &name)
{
    struct directory *dp = db_lookup(dbip, name.c_str(), LOOKUP_QUIET);

    if (dp == RT_DIR_NULL)
	return true;
    if (dp->d_addr != RT_DIR_PHONY_ADDR && db_delete(dbip, dp) != 0)
	return false;
    return db_dirdelete(dbip, dp) == 0;
}


int
put_internal_retyped(struct rt_wdb *wdbp, struct directory *dp,
		     struct rt_db_internal *intern)
{
    const int major_type = intern->idb_major_type;
    const int minor_type = intern->idb_minor_type;
    const unsigned char previous_major_type = dp->d_major_type;
    const unsigned char previous_minor_type = dp->d_minor_type;
    const int previous_flags = dp->d_flags;

    /* wdb_put_internal updates an existing directory's flags, but assumes its
     * type already agrees with the new internal.  Set both before the write so
     * database-change callbacks observe a consistent directory entry. */
    dp->d_major_type = static_cast<unsigned char>(major_type);
    dp->d_minor_type = static_cast<unsigned char>(minor_type);
    int result = wdb_put_internal(wdbp, dp->d_namep, intern, 1.0);

    if (result < 0) {
	dp->d_major_type = previous_major_type;
	dp->d_minor_type = previous_minor_type;
	dp->d_flags = previous_flags;
    }
    return result;
}


bool
rollback_objects(struct rt_wdb *wdbp,
		 const std::vector<struct directory *> &attempted,
		 const std::set<struct directory *> &retyped,
		 std::map<struct directory *, struct rt_db_internal *> &originals)
{
    bool restored = true;

    for (auto parent_it = attempted.rbegin(); parent_it != attempted.rend(); ++parent_it) {
	auto original_it = originals.find(*parent_it);
	if (original_it == originals.end()) {
	    restored = false;
	    continue;
	}
	int result = retyped.find(*parent_it) != retyped.end() ?
	    put_internal_retyped(wdbp, *parent_it, original_it->second) :
	    rt_db_put_internal(*parent_it, wdbp->dbip, original_it->second);
	if (result < 0)
	    restored = false;
    }
    return restored;
}


bool
validate_parent_rewrite(struct db_i *dbip, struct directory *parent,
			const std::vector<rewritten_leaf> &expected,
			const struct bn_tol *tol)
{
    struct rt_db_internal intern;

    RT_DB_INTERNAL_INIT(&intern);
    if (rt_db_get_internal(&intern, parent, dbip, nullptr) < 0)
	return false;
    if (intern.idb_minor_type != ID_COMBINATION || !intern.idb_ptr) {
	rt_db_free_internal(&intern);
	return false;
    }
    const auto *comb = static_cast<const struct rt_comb_internal *>(intern.idb_ptr);
    RT_CK_COMB(comb);
    bool valid = true;
    for (const rewritten_leaf &expected_leaf : expected) {
	const union tree *actual = tree_at_path(comb->tree, expected_leaf.path);
	if (!actual || actual->tr_op != OP_DB_LEAF ||
	    !BU_STR_EQUAL(actual->tr_l.tl_name, expected_leaf.name.c_str())) {
	    valid = false;
	    break;
	}
	mat_t actual_matrix;
	if (actual->tr_l.tl_mat)
	    MAT_COPY(actual_matrix, actual->tr_l.tl_mat);
	else
	    MAT_IDN(actual_matrix);
	if (!bn_mat_is_equal(actual_matrix, expected_leaf.matrix, tol)) {
	    valid = false;
	    break;
	}
    }
    rt_db_free_internal(&intern);
    return valid;
}


union tree *
make_leaf_tree(const std::string &child_name, const mat_t matrix,
	       const struct bn_tol *tol)
{
    union tree *leaf;
    BU_GET(leaf, union tree);
    RT_TREE_INIT(leaf);
    leaf->tr_l.tl_op = OP_DB_LEAF;
    leaf->tr_l.tl_name = bu_strdup(child_name.c_str());
    if (!bn_mat_is_equal(matrix, bn_mat_identity, tol))
	leaf->tr_l.tl_mat = bn_mat_dup(matrix);
    return leaf;
}


void
make_wrapper_internal(struct rt_db_internal *intern, const std::string &child_name,
		      const mat_t matrix,
		      const struct bu_attribute_value_set *attributes,
		      const struct bn_tol *tol)
{
    auto *comb = static_cast<struct rt_comb_internal *>(
	bu_calloc(1, sizeof(struct rt_comb_internal), "unpush wrapper combination"));
    RT_COMB_INTERNAL_INIT(comb);
    comb->tree = make_leaf_tree(child_name, matrix, tol);

    intern->idb_major_type = DB5_MAJORTYPE_BRLCAD;
    intern->idb_minor_type = ID_COMBINATION;
    intern->idb_meth = &OBJ[ID_COMBINATION];
    intern->idb_ptr = comb;
    bu_avs_merge(&intern->idb_avs, attributes);
}


bool
replacement_matrix_is_writable(const mat_t input_matrix,
			       const mat_t canonical_to_input)
{
    mat_t replacement;

    bn_mat_mul(replacement, input_matrix, canonical_to_input);
    return bn_mat_ck("unpush replacement", replacement) == 0;
}


bool
combination_reconstructs_reference(
    const canonical_combination_record &representative,
    const canonical_combination_record &candidate,
    const mat_t reference_matrix, const struct bn_tol *tol,
    mat_t representative_to_candidate)
{
    if (representative.leaves.size() != candidate.leaves.size())
	return false;
    mat_t representative_to_canonical;
    if (!bn_mat_inverse(representative_to_canonical,
	    representative.placement))
	return false;
    bn_mat_mul(representative_to_candidate, candidate.placement,
	representative_to_canonical);
    if (!replacement_matrix_is_writable(reference_matrix,
	    representative_to_candidate))
	return false;

    for (size_t i = 0; i < representative.leaves.size(); i++) {
	mat_t old_leaf_matrix;
	mat_t candidate_leaf_matrix;
	mat_t replacement_leaf_matrix;
	bn_mat_mul(old_leaf_matrix, reference_matrix,
	    candidate.leaves[i].effective_matrix);
	bn_mat_mul(candidate_leaf_matrix, representative_to_candidate,
	    representative.leaves[i].effective_matrix);
	bn_mat_mul(replacement_leaf_matrix, reference_matrix,
	    candidate_leaf_matrix);
	if (!_ged_matrices_numerically_equal(old_leaf_matrix, replacement_leaf_matrix,
		tol))
	    return false;
    }
    return true;
}


const char *
mode_name(enum rt_canonicalize_mode mode)
{
    switch (mode) {
	case RT_CANONICALIZE_RIGID:
	    return "rigid";
	case RT_CANONICALIZE_SIMILARITY:
	    return "similarity";
	case RT_CANONICALIZE_AFFINE:
	    return "affine";
    }
    return "unknown";
}


int
apply_groups(struct ged *gedp, struct rt_wdb *wdbp,
	     const unpush_walk_state &walk,
	     const std::vector<unpush_primitive_record> &records,
	     const std::vector<unpush_group> &groups,
	     const combination_analysis_result &combination_analysis,
	     const std::map<struct directory *, std::vector<indexed_reference>> &references,
	     const struct bn_tol *tol, enum rt_canonicalize_mode mode,
	     bool local_changes_only, int verbosity)
{
    std::set<std::string> database_names;
    std::vector<planned_group> planned_groups;
    std::vector<planned_combination_group> planned_combination_groups;
    std::map<std::string, rewrite_target> targets;
    std::map<struct directory *, rewrite_target> wrapper_targets;
    std::map<struct directory *, rewrite_target> combination_wrapper_targets;
    std::set<struct directory *> parents;
    size_t next_name_id = 1;
    size_t deferred_groups = 0;
    size_t deferred_combination_groups = 0;

    struct directory *directory_entry;
    FOR_ALL_DIRECTORY_START(directory_entry, gedp->dbip)
	if (directory_entry->d_namep)
	    database_names.insert(directory_entry->d_namep);
    FOR_ALL_DIRECTORY_END;

    for (const unpush_group &group : groups) {
	planned_group plan;
	for (size_t record_index : group.records) {
	    struct directory *dp = records[record_index].dp;
	    bool root_primitive = walk.root_primitives.find(dp) !=
		walk.root_primitives.end();
	    auto references_it = references.find(dp);
	    size_t reference_count = references_it == references.end() ? 0 :
		references_it->second.size();
	    bool eligible = root_primitive ||
		(dp->d_nref >= 0 && reference_count > 0 &&
		    (local_changes_only ||
		    reference_count == static_cast<size_t>(dp->d_nref)));
	    if (eligible && root_primitive) {
		eligible = replacement_matrix_is_writable(bn_mat_identity,
		    records[record_index].canonical_to_input);
	    }
	    if (eligible && !root_primitive) {
		for (const indexed_reference &reference : references_it->second) {
		    if (!replacement_matrix_is_writable(reference.matrix,
			    records[record_index].canonical_to_input)) {
			eligible = false;
			break;
		    }
		}
	    }
	    if (eligible) {
		plan.records.push_back(record_index);
	    } else if (!local_changes_only) {
		plan.records.clear();
		break;
	    }
	}
	if (plan.records.size() < 2) {
	    deferred_groups++;
	    continue;
	}

	std::string canonical_name;
	do {
	    canonical_name = std::string(CANONICAL_NAME_PREFIX) +
		std::to_string(next_name_id++);
	} while (database_names.find(canonical_name) != database_names.end());
	database_names.insert(canonical_name);

	plan.canonical_name = canonical_name;
	planned_groups.push_back(plan);
	for (size_t record_index : plan.records) {
	    rewrite_target target;
	    target.canonical_name = canonical_name;
	    MAT_COPY(target.canonical_to_input,
		records[record_index].canonical_to_input);
	    struct directory *dp = records[record_index].dp;
	    if (walk.root_primitives.find(dp) != walk.root_primitives.end())
		wrapper_targets[dp] = target;
	    else
		targets[dp->d_namep] = target;
	}
    }

    for (const combination_group &group : combination_analysis.groups) {
	std::vector<size_t> eligible_records;
	for (size_t record_index : group.records) {
	    struct directory *dp = combination_analysis.records[record_index].dp;
	    bool root_combination = walk.root_combinations.find(dp) !=
		walk.root_combinations.end();
	    auto references_it = references.find(dp);
	    size_t reference_count = references_it == references.end() ? 0 :
		references_it->second.size();
	    bool eligible = root_combination ||
		(dp->d_nref >= 0 && reference_count > 0 &&
		    (local_changes_only ||
		    reference_count == static_cast<size_t>(dp->d_nref)));
	    if (eligible)
		eligible_records.push_back(record_index);
	}
	if (eligible_records.size() < 2) {
	    deferred_combination_groups++;
	    continue;
	}

	planned_combination_group plan;
	plan.representative = eligible_records.front();
	plan.records.push_back(plan.representative);
	const canonical_combination_record &representative =
	    combination_analysis.records[plan.representative];
	for (size_t record_index : eligible_records) {
	    if (record_index == plan.representative)
		continue;
	    const canonical_combination_record &candidate =
		combination_analysis.records[record_index];
	    struct directory *dp = candidate.dp;
	    bool root_combination = walk.root_combinations.find(dp) !=
		walk.root_combinations.end();

	    mat_t representative_to_candidate;
	    bool safe = true;
	    if (root_combination) {
		/* A combination wrapper can retain a selected assembly name, but
		 * wrapping a region in another region changes region traversal. */
		safe = !candidate.region && combination_reconstructs_reference(
		    representative, candidate, bn_mat_identity, tol,
		    representative_to_candidate);
	    } else {
		auto references_it = references.find(dp);
		if (references_it == references.end() || references_it->second.empty()) {
		    safe = false;
		} else {
		    for (const indexed_reference &reference : references_it->second) {
			if (!combination_reconstructs_reference(representative,
				candidate, reference.matrix, tol,
				representative_to_candidate)) {
			    safe = false;
			    break;
			}
		    }
		}
	    }
	    if (!safe)
		continue;

	    rewrite_target target;
	    target.canonical_name = representative.dp->d_namep;
	    MAT_COPY(target.canonical_to_input, representative_to_candidate);
	    if (root_combination)
		combination_wrapper_targets[dp] = target;
	    else
		targets[dp->d_namep] = target;
	    plan.records.push_back(record_index);
	}
	if (plan.records.size() < 2) {
	    deferred_combination_groups++;
	    continue;
	}
	planned_combination_groups.push_back(plan);
    }

    if (planned_groups.empty() && planned_combination_groups.empty()) {
	bu_vls_printf(gedp->ged_result_str,
	    "unpush: no fully-contained groups are currently safe to rewrite"
	    " (%zu deferred)\n", deferred_groups +
		deferred_combination_groups);
	return BRLCAD_OK;
    }

    for (const planned_group &plan : planned_groups) {
	for (size_t record_index : plan.records) {
	    if (walk.root_primitives.find(records[record_index].dp) !=
		    walk.root_primitives.end())
		continue;
	    auto references_it = references.find(records[record_index].dp);
	    if (references_it == references.end())
		continue;
	    for (const indexed_reference &reference : references_it->second)
		parents.insert(reference.parent);
	}
    }
    for (const planned_combination_group &plan : planned_combination_groups) {
	for (size_t record_index : plan.records) {
	    if (record_index == plan.representative)
		continue;
	    struct directory *dp = combination_analysis.records[record_index].dp;
	    if (combination_wrapper_targets.find(dp) !=
		    combination_wrapper_targets.end())
		continue;
	    auto references_it = references.find(dp);
	    if (references_it == references.end())
		continue;
	    for (const indexed_reference &reference : references_it->second)
		parents.insert(reference.parent);
	}
    }
    /* Replacing a top-level combination's whole tree supersedes any child
     * rewrites that would otherwise have been prepared for that combination. */
    for (const auto &wrapper : combination_wrapper_targets)
	parents.erase(wrapper.first);

    std::vector<struct directory *> ordered_parents(parents.begin(), parents.end());
    std::sort(ordered_parents.begin(), ordered_parents.end(),
	[](const struct directory *a, const struct directory *b) {
	    return std::strcmp(a->d_namep, b->d_namep) < 0;
	});
    std::vector<struct directory *> ordered_wrappers;
    for (const auto &wrapper : wrapper_targets)
	ordered_wrappers.push_back(wrapper.first);
    std::sort(ordered_wrappers.begin(), ordered_wrappers.end(),
	[](const struct directory *a, const struct directory *b) {
	    return std::strcmp(a->d_namep, b->d_namep) < 0;
	});
    std::vector<struct directory *> ordered_combination_wrappers;
    for (const auto &wrapper : combination_wrapper_targets)
	ordered_combination_wrappers.push_back(wrapper.first);
    std::sort(ordered_combination_wrappers.begin(),
	ordered_combination_wrappers.end(),
	[](const struct directory *a, const struct directory *b) {
	    return std::strcmp(a->d_namep, b->d_namep) < 0;
	});

    std::map<struct directory *, struct rt_db_internal *> originals;
    std::map<struct directory *, struct rt_db_internal *> updates;
    std::map<struct directory *, std::vector<rewritten_leaf>> expectations;
    bool preparation_failed = false;
    for (struct directory *parent : ordered_parents) {
	struct rt_db_internal *original;
	struct rt_db_internal *update;
	BU_GET(original, struct rt_db_internal);
	BU_GET(update, struct rt_db_internal);
	RT_DB_INTERNAL_INIT(original);
	RT_DB_INTERNAL_INIT(update);
	originals[parent] = original;
	updates[parent] = update;
	if (!(parent->d_flags & RT_DIR_COMB) ||
	    rt_db_get_internal(original, parent, gedp->dbip, nullptr) < 0 ||
	    rt_db_get_internal(update, parent, gedp->dbip, nullptr) < 0) {
	    preparation_failed = true;
	    break;
	}
	auto *comb = static_cast<struct rt_comb_internal *>(update->idb_ptr);
	RT_CK_COMB(comb);
	if (!rewrite_tree(comb->tree, targets, tol, expectations[parent]) ||
	    expectations[parent].empty()) {
	    preparation_failed = true;
	    break;
	}
    }
    for (struct directory *wrapper : ordered_wrappers) {
	if (preparation_failed)
	    break;
	struct rt_db_internal *original;
	struct rt_db_internal *update;
	BU_GET(original, struct rt_db_internal);
	BU_GET(update, struct rt_db_internal);
	RT_DB_INTERNAL_INIT(original);
	RT_DB_INTERNAL_INIT(update);
	originals[wrapper] = original;
	updates[wrapper] = update;
	if ((wrapper->d_flags & RT_DIR_COMB) ||
	    rt_db_get_internal(original, wrapper, gedp->dbip, nullptr) < 0) {
	    preparation_failed = true;
	    break;
	}
	const rewrite_target &target = wrapper_targets.at(wrapper);
	make_wrapper_internal(update, target.canonical_name,
	    target.canonical_to_input, &original->idb_avs, tol);
	rewritten_leaf leaf;
	leaf.name = target.canonical_name;
	MAT_COPY(leaf.matrix, target.canonical_to_input);
	expectations[wrapper].push_back(leaf);
    }
    for (struct directory *wrapper : ordered_combination_wrappers) {
	if (preparation_failed)
	    break;
	struct rt_db_internal *original;
	struct rt_db_internal *update;
	BU_GET(original, struct rt_db_internal);
	BU_GET(update, struct rt_db_internal);
	RT_DB_INTERNAL_INIT(original);
	RT_DB_INTERNAL_INIT(update);
	originals[wrapper] = original;
	updates[wrapper] = update;
	if (!(wrapper->d_flags & RT_DIR_COMB) ||
	    rt_db_get_internal(original, wrapper, gedp->dbip, nullptr) < 0 ||
	    rt_db_get_internal(update, wrapper, gedp->dbip, nullptr) < 0 ||
	    update->idb_minor_type != ID_COMBINATION) {
	    preparation_failed = true;
	    break;
	}
	auto *comb = static_cast<struct rt_comb_internal *>(update->idb_ptr);
	RT_CK_COMB(comb);
	db_free_tree(comb->tree);
	const rewrite_target &target = combination_wrapper_targets.at(wrapper);
	comb->tree = make_leaf_tree(target.canonical_name,
	    target.canonical_to_input, tol);
	rewritten_leaf leaf;
	leaf.name = target.canonical_name;
	MAT_COPY(leaf.matrix, target.canonical_to_input);
	expectations[wrapper].push_back(leaf);
    }
    if (preparation_failed) {
	free_internals(updates);
	free_internals(originals);
	bu_vls_printf(gedp->ged_result_str,
	    "unpush: unable to prepare object rewrites; database unchanged\n");
	return BRLCAD_ERROR;
    }

    std::vector<std::string> written_canonical_names;
    bool canonical_write_failed = false;
    for (const planned_group &plan : planned_groups) {
	struct directory *representative =
	    records[plan.records.front()].dp;
	canonical_object object(gedp->dbip, representative, tol, mode);
	if (object.status != RT_CANONICALIZE_OK) {
	    canonical_write_failed = true;
	    break;
	}
	bu_avs_merge(&object.canonical.idb_avs, &object.input.idb_avs);
	written_canonical_names.push_back(plan.canonical_name);
	if (wdb_put_internal(wdbp, plan.canonical_name.c_str(),
		&object.canonical, 1.0) < 0) {
	    canonical_write_failed = true;
	    break;
	}
    }
    if (canonical_write_failed) {
	bool cleanup_ok = true;
	for (const std::string &name : written_canonical_names)
	    cleanup_ok = remove_object(gedp->dbip, name) && cleanup_ok;
	db_sync(gedp->dbip);
	free_internals(updates);
	free_internals(originals);
	bu_vls_printf(gedp->ged_result_str,
	    "unpush: unable to write canonical primitives; database references "
	    "were not changed%s\n", cleanup_ok ? "" :
	    "; an unreferenced canonical object could not be removed");
	return BRLCAD_ERROR;
    }

    std::vector<struct directory *> attempted_objects;
    bool object_write_failed = false;
    for (struct directory *parent : ordered_parents) {
	attempted_objects.push_back(parent);
	if (rt_db_put_internal(parent, gedp->dbip, updates[parent]) < 0) {
	    object_write_failed = true;
	    break;
	}
    }
    if (!object_write_failed) {
	for (struct directory *wrapper : ordered_wrappers) {
	    attempted_objects.push_back(wrapper);
	    if (put_internal_retyped(wdbp, wrapper, updates[wrapper]) < 0) {
		object_write_failed = true;
		break;
	    }
	}
    }
    if (!object_write_failed) {
	for (struct directory *wrapper : ordered_combination_wrappers) {
	    attempted_objects.push_back(wrapper);
	    if (rt_db_put_internal(wrapper, gedp->dbip, updates[wrapper]) < 0) {
		object_write_failed = true;
		break;
	    }
	}
    }

    bool validation_failed = false;
    if (!object_write_failed) {
	for (struct directory *parent : ordered_parents) {
	    if (!validate_parent_rewrite(gedp->dbip, parent,
		    expectations[parent], tol)) {
		validation_failed = true;
		break;
	    }
	}
    }
    if (!object_write_failed && !validation_failed) {
	for (struct directory *wrapper : ordered_wrappers) {
	    if (!validate_parent_rewrite(gedp->dbip, wrapper,
		    expectations[wrapper], tol)) {
		validation_failed = true;
		break;
	    }
	}
    }
    if (!object_write_failed && !validation_failed) {
	for (struct directory *wrapper : ordered_combination_wrappers) {
	    if (!validate_parent_rewrite(gedp->dbip, wrapper,
		    expectations[wrapper], tol)) {
		validation_failed = true;
		break;
	    }
	}
    }

    if (object_write_failed || validation_failed) {
	if (!object_write_failed) {
	attempted_objects = ordered_parents;
	attempted_objects.insert(attempted_objects.end(), ordered_wrappers.begin(),
	    ordered_wrappers.end());
	attempted_objects.insert(attempted_objects.end(),
	    ordered_combination_wrappers.begin(),
	    ordered_combination_wrappers.end());
	}
	std::set<struct directory *> retyped(ordered_wrappers.begin(),
	    ordered_wrappers.end());
	bool rollback_ok = rollback_objects(wdbp, attempted_objects, retyped, originals);
	bool cleanup_ok = rollback_ok;
	if (rollback_ok) {
	    for (const std::string &name : written_canonical_names)
		cleanup_ok = remove_object(gedp->dbip, name) && cleanup_ok;
	}
	db_sync(gedp->dbip);
	free_internals(updates);
	free_internals(originals);
	bu_vls_printf(gedp->ged_result_str,
	    "unpush: %s; %s\n",
	    object_write_failed ? "an object write failed" :
	    "post-write validation failed",
	    rollback_ok ? (cleanup_ok ? "original objects restored" :
		"original objects restored, but redundant canonical objects remain") :
		"rollback was incomplete; canonical objects were retained to avoid missing references");
	return BRLCAD_ERROR;
    }

    free_internals(updates);
    free_internals(originals);

    db_update_nref(gedp->dbip);
    size_t removed_combinations = 0;
    size_t retained_combinations = 0;
    bool deletion_failed = false;
    for (const planned_combination_group &plan : planned_combination_groups) {
	for (size_t record_index : plan.records) {
	    if (record_index == plan.representative)
		continue;
	    struct directory *old_object =
		combination_analysis.records[record_index].dp;
	    if (combination_wrapper_targets.find(old_object) !=
		    combination_wrapper_targets.end())
		continue;
	    if (old_object->d_nref != 0) {
		retained_combinations++;
		if (!local_changes_only)
		    deletion_failed = true;
		continue;
	    }
	    std::string old_name = old_object->d_namep;
	    if (remove_object(gedp->dbip, old_name))
		removed_combinations++;
	    else {
		retained_combinations++;
		deletion_failed = true;
	    }
	}
    }
    db_update_nref(gedp->dbip);

    size_t removed_objects = 0;
    size_t retained_objects = 0;
    for (const planned_group &plan : planned_groups) {
	for (size_t record_index : plan.records) {
	    struct directory *old_object = records[record_index].dp;
	    if (wrapper_targets.find(old_object) != wrapper_targets.end())
		continue;
	    if (old_object->d_nref != 0) {
		retained_objects++;
		if (!local_changes_only)
		    deletion_failed = true;
		continue;
	    }
	    std::string old_name = old_object->d_namep;
	    if (remove_object(gedp->dbip, old_name))
		removed_objects++;
	    else {
		retained_objects++;
		deletion_failed = true;
	    }
	}
    }
    db_update_nref(gedp->dbip);
    db_sync(gedp->dbip);

    bu_vls_printf(gedp->ged_result_str,
	"unpush rewrite complete\n"
	"  canonical objects written: %zu\n"
	"  parent combinations rewritten: %zu\n"
	"  top-level wrappers written: %zu\n"
	"  top-level combination wrappers written: %zu\n"
	"  original objects removed: %zu\n"
	"  original objects retained: %zu\n"
	"  groups deferred: %zu\n"
	"  combination groups consolidated: %zu\n"
	"  combination objects removed: %zu\n"
	"  combination objects retained: %zu\n"
	"  combination groups deferred: %zu\n",
	planned_groups.size(), ordered_parents.size(), ordered_wrappers.size(),
	ordered_combination_wrappers.size(), removed_objects, retained_objects,
	deferred_groups, planned_combination_groups.size(),
	removed_combinations, retained_combinations,
	deferred_combination_groups);
    if (verbosity > 0) {
	for (const planned_group &plan : planned_groups)
	    bu_vls_printf(gedp->ged_result_str, "  wrote %s\n",
		plan.canonical_name.c_str());
    }
    if (deletion_failed) {
	bu_vls_printf(gedp->ged_result_str,
	    "unpush: one or more superseded objects could not be removed; "
	    "the rewritten trees remain valid\n");
	return BRLCAD_ERROR;
    }

    return BRLCAD_OK;
}


void
unpush_usage(struct bu_vls *output, struct bu_opt_desc *options)
{
    char *option_help = bu_opt_describe(options, nullptr);
    bu_vls_sprintf(output, "Usage: unpush [options] object...\n\n");
    bu_vls_printf(output,
	"Restores shared primitive objects and reference matrices inferred from "
	"pushed geometry.  Use -D to analyze without modifying the database.  "
	"Writes constrain affine requests to similarity transforms because stored "
	"combination matrices cannot represent general shear.\n\n");
    if (option_help) {
	bu_vls_printf(output, "Options:\n%s\n", option_help);
	bu_free(option_help, "unpush option help");
    }
}

}


extern "C" int
ged_unpush_core(struct ged *gedp, int argc, const char *argv[])
{
    int print_help = 0;
    int dry_run = 0;
    int local_changes_only = 0;
    int verbosity = 0;
    bool write_mode_constrained = false;
    const char *mode_argument = "affine";
    struct bu_opt_desc options[7];
    BU_OPT(options[0], "h", "help", "", nullptr, &print_help, "Print help and exit");
    BU_OPT(options[1], "?", "", "", nullptr, &print_help, "");
    BU_OPT(options[2], "D", "dry-run", "", nullptr, &dry_run, "Analyze without modifying the database");
    BU_OPT(options[3], "L", "local", "", nullptr, &local_changes_only, "Preserve uses outside the selected trees");
    BU_OPT(options[4], "v", "verbosity", "", &bu_opt_incr_long, &verbosity, "Increase reporting verbosity");
    BU_OPT(options[5], "m", "mode", "rigid|similarity|affine", &bu_opt_str, &mode_argument, "Maximum transform class to factor out");
    BU_OPT_NULL(options[6]);

    argc--;
    argv++;
    int option_result = bu_opt_parse(nullptr, argc, argv, options);
    argc = option_result;

    if (print_help || argc == 0) {
	unpush_usage(gedp->ged_result_str, options);
	return BRLCAD_OK;
    }

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);
    bu_vls_trunc(gedp->ged_result_str, 0);
    if (!dry_run)
	GED_CHECK_READ_ONLY(gedp, BRLCAD_ERROR);

    enum rt_canonicalize_mode mode = RT_CANONICALIZE_AFFINE;
    if (BU_STR_EQUAL(mode_argument, "rigid"))
	mode = RT_CANONICALIZE_RIGID;
    else if (BU_STR_EQUAL(mode_argument, "similarity"))
	mode = RT_CANONICALIZE_SIMILARITY;
    else if (!BU_STR_EQUAL(mode_argument, "affine")) {
	bu_vls_printf(gedp->ged_result_str, "unpush: unknown mode '%s'\n", mode_argument);
	return BRLCAD_ERROR;
    }
    if (!dry_run && mode == RT_CANONICALIZE_AFFINE) {
	/* General affine canonical placements may contain shear, which librt
	 * deliberately rejects in stored combination matrices.  Similarity is
	 * the strongest transform class accepted for every orientation. */
	mode = RT_CANONICALIZE_SIMILARITY;
	write_mode_constrained = true;
    }

    struct rt_wdb *wdbp = wdb_dbopen(gedp->dbip, RT_WDB_TYPE_DB_DEFAULT);
    if (!wdbp) {
	bu_vls_printf(gedp->ged_result_str, "unpush: unable to access database tolerances\n");
	return BRLCAD_ERROR;
    }
    const struct bn_tol *tol = &wdbp->wdb_tol;

    unpush_walk_state walk;
    walk.dbip = gedp->dbip;
    for (int i = 0; i < argc; i++) {
	struct directory *root = db_lookup(gedp->dbip, argv[i], LOOKUP_QUIET);
	if (root == RT_DIR_NULL) {
	    bu_vls_printf(gedp->ged_result_str, "unpush: object '%s' does not exist\n", argv[i]);
	    return BRLCAD_ERROR;
	}
	if (root->d_flags & RT_DIR_COMB)
	    walk.root_combinations.insert(root);
	else
	    walk.root_primitives.insert(root);
	collect_object(walk, root);
    }

    if (walk.cycle_found || walk.read_error) {
	bu_vls_printf(gedp->ged_result_str, "unpush: unable to analyze selected trees (%s)\n",
	    walk.cycle_found ? "cyclic combination reference" : "database read or tree error");
	return BRLCAD_ERROR;
    }

    std::vector<unpush_primitive_record> records;
    std::map<std::string, std::vector<size_t>> buckets;
    std::map<int, size_t> unsupported_types;
    std::map<int, size_t> failed_types;
    std::map<int, std::vector<std::string>> failed_objects;
    size_t unsupported = 0;
    size_t failures = 0;
    std::vector<struct directory *> primitive_objects(walk.primitives.begin(), walk.primitives.end());
    std::sort(primitive_objects.begin(), primitive_objects.end(),
	[](const struct directory *a, const struct directory *b) {
	    return std::strcmp(a->d_namep, b->d_namep) < 0;
	});
    for (struct directory *primitive : primitive_objects) {
	canonical_object object(gedp->dbip, primitive, tol, mode);
	if (object.status == RT_CANONICALIZE_UNSUPPORTED) {
	    unsupported++;
	    unsupported_types[primitive->d_minor_type]++;
	    continue;
	}
	if (object.status != RT_CANONICALIZE_OK) {
	    failures++;
	    failed_types[primitive->d_minor_type]++;
	    failed_objects[primitive->d_minor_type].push_back(primitive->d_namep);
	    continue;
	}

	unpush_primitive_record record;
	record.dp = primitive;
	MAT_COPY(record.canonical_to_input, object.placement);
	record.bucket = canonical_bucket(object);
	canonical_metric(object, tol, record.metric, record.metric_tolerance);
	if (!std::isfinite(record.metric) ||
	    !std::isfinite(record.metric_tolerance)) {
	    failures++;
	    failed_types[primitive->d_minor_type]++;
	    failed_objects[primitive->d_minor_type].push_back(primitive->d_namep);
	    continue;
	}
	records.push_back(record);
	buckets[record.bucket].push_back(records.size() - 1);
    }
    std::set<struct directory *> reference_targets = walk.primitives;
    reference_targets.insert(walk.visited_combinations.begin(),
	walk.visited_combinations.end());
    database_reference_index database_references;
    database_references.targets = &reference_targets;
    if (db_add_update_nref_clbk(gedp->dbip, collect_database_reference,
	    &database_references) != 0) {
	bu_vls_printf(gedp->ged_result_str,
	    "unpush: unable to register reference accounting\n");
	return BRLCAD_ERROR;
    }
    db_update_nref(gedp->dbip);
    if (db_rm_update_nref_clbk(gedp->dbip, collect_database_reference,
	    &database_references) != 1) {
	bu_vls_printf(gedp->ged_result_str,
	    "unpush: unable to unregister reference accounting\n");
	return BRLCAD_ERROR;
    }

    std::set<struct directory *> exposed;
    for (struct directory *combination : walk.visited_combinations) {
	size_t selected = walk.selected_references[combination];
	if (combination->d_nref > static_cast<long>(selected)) {
	    exposed.insert(combination);
	    mark_exposed_descendants(combination, walk.children, exposed);
	}
    }

    std::map<struct directory *, std::vector<indexed_reference>> local_references;
    const auto *rewrite_references = &database_references.references;
    if (local_changes_only) {
	for (const unpush_reference &reference : walk.references) {
	    if (exposed.find(reference.parent) != exposed.end())
		continue;
	    indexed_reference indexed;
	    indexed.parent = reference.parent;
	    MAT_COPY(indexed.matrix, reference.matrix);
	    local_references[reference.child].push_back(indexed);
	}
	rewrite_references = &local_references;
    }

    std::vector<unpush_group> groups = verify_groups(gedp->dbip, records,
	buckets, *rewrite_references, walk.root_primitives, tol, mode);
    combination_analysis_result combination_analysis = analyze_combinations(
	gedp->dbip, walk.visited_combinations, records, groups, tol);

    size_t grouped_objects = 0;
    size_t duplicate_objects = 0;
    size_t rewritable_references = 0;
    size_t potential_reuse = 0;
    size_t estimated_granule_reduction = 0;
    std::set<struct directory *> grouped_exposed;
    std::map<struct directory *, size_t> grouped_record_indices;
    for (const unpush_group &group : groups) {
	grouped_objects += group.records.size();
	duplicate_objects += group.records.size() - 1;
	size_t group_instances = 0;
	size_t group_storage = 0;
	size_t largest_object = 0;
	for (size_t record_index : group.records) {
	    struct directory *dp = records[record_index].dp;
	    grouped_record_indices[dp] = record_index;
	    size_t selected = walk.selected_references[dp];
	    if (walk.root_primitives.find(dp) != walk.root_primitives.end())
		selected++;
	    rewritable_references += selected;
	    group_instances += selected;
	    size_t object_size = dp->d_len;
	    group_storage += object_size;
	    largest_object = std::max(largest_object, object_size);
	    if (exposed.find(dp) != exposed.end() ||
		dp->d_nref > static_cast<long>(walk.selected_references[dp]))
		grouped_exposed.insert(dp);
	}
	if (group_instances > 1)
	    potential_reuse += group_instances - 1;
	estimated_granule_reduction += group_storage - largest_object;
    }

    size_t grouped_combinations = 0;
    size_t duplicate_combinations = 0;
    size_t potential_boolean_reuse = 0;
    for (const combination_group &group : combination_analysis.groups) {
	grouped_combinations += group.records.size();
	duplicate_combinations += group.records.size() - 1;
	size_t group_instances = 0;
	for (size_t record_index : group.records) {
	    struct directory *dp = combination_analysis.records[record_index].dp;
	    group_instances += walk.selected_references[dp];
	    if (walk.root_combinations.find(dp) != walk.root_combinations.end())
		group_instances++;
	}
	if (group_instances > 1)
	    potential_boolean_reuse += group_instances - 1;
    }

    bu_vls_printf(gedp->ged_result_str,
	"unpush %s analysis\n"
	"  mode: %s\n"
	"%s"
	"  local preservation: %s\n"
	"  primitive objects: %zu\n"
	"  canonicalized: %zu\n"
	"  unsupported: %zu\n"
	"  failed: %zu\n"
	"  verified groups: %zu\n"
	"  grouped objects: %zu\n"
	"  duplicate objects: %zu\n"
	"  rewritable selected references: %zu\n"
	"  top-level primitive wrappers: %zu\n"
	"  externally exposed grouped objects: %zu\n"
	"  estimated removable database granules: %zu\n"
	"  potential facetize primitive reuses: %zu\n"
	"  combination objects: %zu\n"
	"  canonicalized combinations: %zu\n"
	"  combination failures: %zu\n"
	"  verified combination groups: %zu\n"
	"  grouped combination objects: %zu\n"
	"  duplicate combination objects: %zu\n"
	"  potential facetize boolean reuses: %zu\n",
	dry_run ? "dry-run" : "rewrite", mode_name(mode),
	write_mode_constrained ?
	    "  requested affine mode constrained to similarity for database writes\n" : "",
	local_changes_only ? "yes" : "no", walk.primitives.size(),
	records.size(), unsupported, failures, groups.size(), grouped_objects,
	duplicate_objects, rewritable_references, walk.root_primitives.size(),
	grouped_exposed.size(), estimated_granule_reduction, potential_reuse,
	walk.visited_combinations.size(), combination_analysis.records.size() -
	    combination_analysis.failures, combination_analysis.failures,
	combination_analysis.groups.size(), grouped_combinations,
	duplicate_combinations, potential_boolean_reuse);

    if (walk.missing_references)
	bu_vls_printf(gedp->ged_result_str, "  pre-existing missing references: %zu\n",
	    walk.missing_references);

    if (verbosity > 0) {
	for (const auto &type_count : unsupported_types) {
	    const char *label = (type_count.first >= 0 && type_count.first <= ID_MAX_SOLID) ?
		OBJ[type_count.first].ft_label : "unknown";
	    bu_vls_printf(gedp->ged_result_str, "  unsupported %s: %zu\n",
		label, type_count.second);
	}
	for (const auto &type_count : failed_types) {
	    const char *label = (type_count.first >= 0 && type_count.first <= ID_MAX_SOLID) ?
		OBJ[type_count.first].ft_label : "unknown";
	    bu_vls_printf(gedp->ged_result_str, "  failed %s: %zu\n",
		label, type_count.second);
	    bu_vls_printf(gedp->ged_result_str, "    objects:");
	    for (const std::string &name : failed_objects[type_count.first])
		bu_vls_printf(gedp->ged_result_str, " %s", name.c_str());
	    bu_vls_putc(gedp->ged_result_str, '\n');
	}
	for (size_t group_index = 0; group_index < groups.size(); group_index++) {
	    bu_vls_printf(gedp->ged_result_str, "  group %zu:", group_index + 1);
	    for (size_t record_index : groups[group_index].records)
		bu_vls_printf(gedp->ged_result_str, " %s", records[record_index].dp->d_namep);
	    bu_vls_putc(gedp->ged_result_str, '\n');
	}
	for (size_t group_index = 0;
		group_index < combination_analysis.groups.size(); group_index++) {
	    bu_vls_printf(gedp->ged_result_str, "  combination group %zu:",
		group_index + 1);
	    std::vector<const char *> names;
	    for (size_t record_index : combination_analysis.groups[group_index].records)
		names.push_back(
		    combination_analysis.records[record_index].dp->d_namep);
	    std::sort(names.begin(), names.end(),
		[](const char *a, const char *b) {
		    return std::strcmp(a, b) < 0;
		});
	    for (const char *name : names)
		bu_vls_printf(gedp->ged_result_str, " %s", name);
	    bu_vls_putc(gedp->ged_result_str, '\n');
	}
    }

    if (verbosity > 1) {
	for (const unpush_reference &reference : walk.references) {
	    auto record_it = grouped_record_indices.find(reference.child);
	    if (record_it == grouped_record_indices.end())
		continue;

	    mat_t replacement;
	    bn_mat_mul(replacement, reference.matrix,
		records[record_it->second].canonical_to_input);
	    struct bu_vls title = BU_VLS_INIT_ZERO;
	    bu_vls_sprintf(&title, "  %s/%s replacement matrix",
		reference.parent->d_namep, reference.child->d_namep);
	    bn_mat_print_vls(bu_vls_cstr(&title), replacement, gedp->ged_result_str);
	    bu_vls_free(&title);
	}
	std::vector<struct directory *> root_objects(walk.root_primitives.begin(), walk.root_primitives.end());
	std::sort(root_objects.begin(), root_objects.end(),
	    [](const struct directory *a, const struct directory *b) {
		return std::strcmp(a->d_namep, b->d_namep) < 0;
	    });
	for (struct directory *root : root_objects) {
	    auto record_it = grouped_record_indices.find(root);
	    if (record_it == grouped_record_indices.end())
		continue;
	    struct bu_vls title = BU_VLS_INIT_ZERO;
	    bu_vls_sprintf(&title, "  %s wrapper matrix", root->d_namep);
	    bn_mat_print_vls(bu_vls_cstr(&title),
		records[record_it->second].canonical_to_input, gedp->ged_result_str);
	    bu_vls_free(&title);
	}
    }

    if (!dry_run) {
	if (walk.missing_references) {
	    bu_vls_printf(gedp->ged_result_str,
		"unpush: refusing to rewrite a tree with pre-existing missing references\n");
	    return BRLCAD_ERROR;
	}
	return apply_groups(gedp, wdbp, walk, records, groups,
	    combination_analysis, *rewrite_references, tol, mode,
	    local_changes_only != 0, verbosity);
    }

    return BRLCAD_OK;
}

/*
 * Local Variables:
 * mode: C++
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
