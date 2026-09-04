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
#include <cmath>
#include <cstring>
#include <limits>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "bg/trimesh.h"
#include "bu/opt.h"
#include "rt/func.h"

#include "../ged_private.h"


namespace {

constexpr fastf_t BUCKET_TOLERANCE_MULTIPLIER = 8.0;
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
};


struct unpush_group {
    std::vector<size_t> records;
};


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
    std::string name;
    mat_t matrix = MAT_INIT_IDN;
};


struct planned_group {
    const unpush_group *group = nullptr;
    std::string canonical_name;
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


long long
bucket_value(fastf_t value, const struct bn_tol *tol)
{
    fastf_t resolution = std::max(tol->dist * BUCKET_TOLERANCE_MULTIPLIER,
	static_cast<fastf_t>(SMALL_FASTF));
    long double scaled = static_cast<long double>(value) / resolution;
    long double maximum = static_cast<long double>((std::numeric_limits<long long>::max)());

    if (scaled >= maximum)
	return (std::numeric_limits<long long>::max)();
    if (scaled <= -maximum)
	return -(std::numeric_limits<long long>::max)();
    return std::llround(scaled);
}


std::string
canonical_bucket(const canonical_object &object, const struct bn_tol *tol)
{
    std::ostringstream key;
    key << object.canonical.idb_minor_type << ':' << object.input.idb_avs.count;

    switch (object.canonical.idb_minor_type) {
	case ID_HALF:
	    break;
	case ID_ELL:
	case ID_SPH: {
	    const auto *ell = static_cast<const struct rt_ell_internal *>(object.canonical.idb_ptr);
	    key << ':' << bucket_value(MAGNITUDE(ell->a), tol)
		<< ':' << bucket_value(MAGNITUDE(ell->b), tol)
		<< ':' << bucket_value(MAGNITUDE(ell->c), tol);
	    break;
	}
	case ID_TOR: {
	    const auto *tor = static_cast<const struct rt_tor_internal *>(object.canonical.idb_ptr);
	    key << ':' << bucket_value(tor->r_a, tol)
		<< ':' << bucket_value(tor->r_h, tol);
	    break;
	}
	case ID_ETO: {
	    const auto *eto = static_cast<const struct rt_eto_internal *>(object.canonical.idb_ptr);
	    key << ':' << bucket_value(eto->eto_C[X], tol)
		<< ':' << bucket_value(eto->eto_C[Z], tol)
		<< ':' << bucket_value(eto->eto_r, tol)
		<< ':' << bucket_value(eto->eto_rd, tol);
	    break;
	}
	case ID_TGC:
	case ID_REC: {
	    const auto *tgc = static_cast<const struct rt_tgc_internal *>(object.canonical.idb_ptr);
	    const fastf_t *vectors[] = {tgc->h, tgc->a, tgc->b, tgc->c, tgc->d};
	    for (const fastf_t *vector : vectors) {
		key << ':' << bucket_value(vector[X], tol)
		    << ':' << bucket_value(vector[Y], tol)
		    << ':' << bucket_value(vector[Z], tol);
	    }
	    break;
	}
	case ID_ARB8: {
	    const auto *arb = static_cast<const struct rt_arb_internal *>(object.canonical.idb_ptr);
	    for (const point_t &point : arb->pt) {
		key << ':' << bucket_value(point[X], tol)
		    << ':' << bucket_value(point[Y], tol)
		    << ':' << bucket_value(point[Z], tol);
	    }
	    break;
	}
	case ID_BOT: {
	    const auto *bot = static_cast<const struct rt_bot_internal *>(object.canonical.idb_ptr);
	    unsigned long long hash = bg_trimesh_hash(
		bot->faces, bot->num_faces,
		reinterpret_cast<const point_t *>(bot->vertices), bot->num_vertices,
		tol->dist);
	    key << ':' << static_cast<unsigned int>(bot->mode)
		<< ':' << static_cast<unsigned int>(bot->orientation)
		<< ':' << static_cast<unsigned int>(bot->bot_flags)
		<< ':' << bot->num_vertices << ':' << bot->num_faces << ':' << hash;
	    break;
	}
	default:
	    break;
    }

    return key.str();
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
near_value(fastf_t a, fastf_t b, fastf_t tolerance)
{
    return std::fabs(a - b) <= tolerance;
}


bool
bot_payload_equal(const struct rt_bot_internal *a, const struct rt_bot_internal *b,
		  const struct bn_tol *tol)
{
    if (a->mode != b->mode || a->orientation != b->orientation ||
	a->bot_flags != b->bot_flags || a->num_vertices != b->num_vertices ||
	a->num_faces != b->num_faces || a->num_normals != b->num_normals ||
	a->num_face_normals != b->num_face_normals || a->num_uvs != b->num_uvs ||
	a->num_face_uvs != b->num_face_uvs)
	return false;

    if (bg_trimesh_diff(a->faces, a->num_faces,
	reinterpret_cast<const point_t *>(a->vertices), a->num_vertices,
	b->faces, b->num_faces, reinterpret_cast<const point_t *>(b->vertices),
	b->num_vertices, tol->dist))
	return false;

    if ((a->thickness == nullptr) != (b->thickness == nullptr) ||
	(a->face_mode == nullptr) != (b->face_mode == nullptr) ||
	(a->normals == nullptr) != (b->normals == nullptr) ||
	(a->face_normals == nullptr) != (b->face_normals == nullptr) ||
	(a->uvs == nullptr) != (b->uvs == nullptr) ||
	(a->face_uvs == nullptr) != (b->face_uvs == nullptr))
	return false;

    for (size_t i = 0; a->thickness && i < a->num_faces; i++) {
	if (!near_value(a->thickness[i], b->thickness[i], tol->dist))
	    return false;
    }
    for (size_t i = 0; a->face_mode && i < a->num_faces; i++) {
	if (BU_BITTEST(a->face_mode, i) != BU_BITTEST(b->face_mode, i))
	    return false;
    }
    for (size_t i = 0; a->normals && i < 3 * a->num_normals; i++) {
	if (!near_value(a->normals[i], b->normals[i], tol->perp))
	    return false;
    }
    for (size_t i = 0; a->face_normals && i < 3 * a->num_face_normals; i++) {
	if (a->face_normals[i] != b->face_normals[i])
	    return false;
    }
    for (size_t i = 0; a->uvs && i < 3 * a->num_uvs; i++) {
	if (!near_value(a->uvs[i], b->uvs[i], tol->dist))
	    return false;
    }
    for (size_t i = 0; a->face_uvs && i < 3 * a->num_face_uvs; i++) {
	if (a->face_uvs[i] != b->face_uvs[i])
	    return false;
    }

    return true;
}


bool
canonical_geometry_equal(const struct rt_db_internal *a,
			 const struct rt_db_internal *b,
			 const struct bn_tol *tol)
{
    if (a->idb_minor_type != b->idb_minor_type)
	return false;

    switch (a->idb_minor_type) {
	case ID_HALF:
	    return true;
	case ID_ELL:
	case ID_SPH: {
	    const auto *aell = static_cast<const struct rt_ell_internal *>(a->idb_ptr);
	    const auto *bell = static_cast<const struct rt_ell_internal *>(b->idb_ptr);
	    return VNEAR_EQUAL(aell->v, bell->v, tol->dist) &&
		VNEAR_EQUAL(aell->a, bell->a, tol->dist) &&
		VNEAR_EQUAL(aell->b, bell->b, tol->dist) &&
		VNEAR_EQUAL(aell->c, bell->c, tol->dist);
	}
	case ID_TOR: {
	    const auto *ator = static_cast<const struct rt_tor_internal *>(a->idb_ptr);
	    const auto *btor = static_cast<const struct rt_tor_internal *>(b->idb_ptr);
	    return VNEAR_EQUAL(ator->v, btor->v, tol->dist) &&
		VNEAR_EQUAL(ator->h, btor->h, tol->perp) &&
		near_value(ator->r_a, btor->r_a, tol->dist) &&
		near_value(ator->r_h, btor->r_h, tol->dist);
	}
	case ID_ETO: {
	    const auto *aeto = static_cast<const struct rt_eto_internal *>(a->idb_ptr);
	    const auto *beto = static_cast<const struct rt_eto_internal *>(b->idb_ptr);
	    return VNEAR_EQUAL(aeto->eto_V, beto->eto_V, tol->dist) &&
		VNEAR_EQUAL(aeto->eto_N, beto->eto_N, tol->perp) &&
		VNEAR_EQUAL(aeto->eto_C, beto->eto_C, tol->dist) &&
		near_value(aeto->eto_r, beto->eto_r, tol->dist) &&
		near_value(aeto->eto_rd, beto->eto_rd, tol->dist);
	}
	case ID_TGC:
	case ID_REC: {
	    const auto *atgc = static_cast<const struct rt_tgc_internal *>(a->idb_ptr);
	    const auto *btgc = static_cast<const struct rt_tgc_internal *>(b->idb_ptr);
	    return VNEAR_EQUAL(atgc->v, btgc->v, tol->dist) &&
		VNEAR_EQUAL(atgc->h, btgc->h, tol->dist) &&
		VNEAR_EQUAL(atgc->a, btgc->a, tol->dist) &&
		VNEAR_EQUAL(atgc->b, btgc->b, tol->dist) &&
		VNEAR_EQUAL(atgc->c, btgc->c, tol->dist) &&
		VNEAR_EQUAL(atgc->d, btgc->d, tol->dist);
	}
	case ID_ARB8: {
	    const auto *aarb = static_cast<const struct rt_arb_internal *>(a->idb_ptr);
	    const auto *barb = static_cast<const struct rt_arb_internal *>(b->idb_ptr);
	    for (size_t i = 0; i < 8; i++) {
		if (!VNEAR_EQUAL(aarb->pt[i], barb->pt[i], tol->dist))
		    return false;
	    }
	    return true;
	}
	case ID_BOT:
	    return bot_payload_equal(static_cast<const struct rt_bot_internal *>(a->idb_ptr),
		static_cast<const struct rt_bot_internal *>(b->idb_ptr), tol);
	default:
	    return false;
    }
}


std::vector<unpush_group>
verify_groups(struct db_i *dbip, const std::vector<unpush_primitive_record> &records,
	      const std::map<std::string, std::vector<size_t>> &buckets,
	      const struct bn_tol *tol, enum rt_canonicalize_mode mode)
{
    std::vector<unpush_group> groups;

    for (const auto &bucket : buckets) {
	if (bucket.second.size() < 2)
	    continue;

	std::set<size_t> remaining(bucket.second.begin(), bucket.second.end());
	while (remaining.size() > 1) {
	    size_t representative_index = *remaining.begin();
	    remaining.erase(remaining.begin());
	    canonical_object representative(dbip, records[representative_index].dp, tol, mode);
	    if (representative.status != RT_CANONICALIZE_OK)
		continue;

	    unpush_group group;
	    group.records.push_back(representative_index);
	    for (auto candidate_it = remaining.begin(); candidate_it != remaining.end();) {
		canonical_object candidate(dbip, records[*candidate_it].dp, tol, mode);
		if (candidate.status == RT_CANONICALIZE_OK &&
		    attributes_equal(&representative.input.idb_avs, &candidate.input.idb_avs) &&
		    canonical_geometry_equal(&representative.canonical, &candidate.canonical, tol)) {
		    group.records.push_back(*candidate_it);
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
rewrite_tree(union tree *tree, const std::map<std::string, rewrite_target> &targets,
	     const struct bn_tol *tol, std::vector<rewritten_leaf> &rewritten)
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
	    leaf.name = target_it->second.canonical_name;
	    MAT_COPY(leaf.matrix, replacement);
	    rewritten.push_back(leaf);
	    return true;
	}
	case OP_UNION:
	case OP_INTERSECT:
	case OP_SUBTRACT:
	case OP_XOR:
	    return rewrite_tree(tree->tr_b.tb_left, targets, tol, rewritten) &&
		rewrite_tree(tree->tr_b.tb_right, targets, tol, rewritten);
	case OP_NOT:
	case OP_GUARD:
	case OP_XNOP:
	    return rewrite_tree(tree->tr_b.tb_left, targets, tol, rewritten);
	case OP_NOP:
	    return true;
	default:
	    return false;
    }
}


bool
collect_rewritten_leaves(const union tree *tree,
			 const std::set<std::string> &canonical_names,
			 std::vector<rewritten_leaf> &rewritten)
{
    if (!tree)
	return true;

    RT_CK_TREE(tree);
    switch (tree->tr_op) {
	case OP_DB_LEAF:
	    if (canonical_names.find(tree->tr_l.tl_name) != canonical_names.end()) {
		rewritten_leaf leaf;
		leaf.name = tree->tr_l.tl_name;
		if (tree->tr_l.tl_mat)
		    MAT_COPY(leaf.matrix, tree->tr_l.tl_mat);
		rewritten.push_back(leaf);
	    }
	    return true;
	case OP_UNION:
	case OP_INTERSECT:
	case OP_SUBTRACT:
	case OP_XOR:
	    return collect_rewritten_leaves(tree->tr_b.tb_left, canonical_names,
		rewritten) &&
		collect_rewritten_leaves(tree->tr_b.tb_right, canonical_names,
		    rewritten);
	case OP_NOT:
	case OP_GUARD:
	case OP_XNOP:
	    return collect_rewritten_leaves(tree->tr_b.tb_left, canonical_names,
		rewritten);
	case OP_NOP:
	    return true;
	default:
	    return false;
    }
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


bool
rollback_parents(struct db_i *dbip,
		 const std::vector<struct directory *> &attempted,
		 std::map<struct directory *, struct rt_db_internal *> &originals)
{
    bool restored = true;

    for (auto parent_it = attempted.rbegin(); parent_it != attempted.rend(); ++parent_it) {
	auto original_it = originals.find(*parent_it);
	if (original_it == originals.end() ||
	    rt_db_put_internal(*parent_it, dbip, original_it->second) < 0)
	    restored = false;
    }
    return restored;
}


bool
validate_parent_rewrite(struct db_i *dbip, struct directory *parent,
			const std::set<std::string> &canonical_names,
			const std::vector<rewritten_leaf> &expected,
			const struct bn_tol *tol)
{
    struct rt_db_internal intern;
    std::vector<rewritten_leaf> actual;

    RT_DB_INTERNAL_INIT(&intern);
    if (rt_db_get_internal(&intern, parent, dbip, nullptr) < 0)
	return false;
    const auto *comb = static_cast<const struct rt_comb_internal *>(intern.idb_ptr);
    RT_CK_COMB(comb);
    bool valid = collect_rewritten_leaves(comb->tree, canonical_names, actual) &&
	actual.size() == expected.size();
    for (size_t i = 0; valid && i < expected.size(); i++) {
	valid = actual[i].name == expected[i].name &&
	    bn_mat_is_equal(actual[i].matrix, expected[i].matrix, tol);
    }
    rt_db_free_internal(&intern);
    return valid;
}


bool
replacement_matrix_is_writable(const mat_t input_matrix,
			       const mat_t canonical_to_input)
{
    mat_t replacement;

    bn_mat_mul(replacement, input_matrix, canonical_to_input);
    return bn_mat_ck("unpush replacement", replacement) == 0;
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
	     const database_reference_index &database_references,
	     const struct bn_tol *tol, enum rt_canonicalize_mode mode,
	     bool local_changes_only, int verbosity)
{
    std::set<std::string> database_names;
    std::vector<planned_group> planned_groups;
    std::map<std::string, rewrite_target> targets;
    std::set<struct directory *> parents;
    std::map<struct directory *, std::vector<indexed_reference>> local_references;
    size_t next_name_id = 1;
    size_t deferred_groups = 0;

    struct directory *directory_entry;
    FOR_ALL_DIRECTORY_START(directory_entry, gedp->dbip)
	if (directory_entry->d_namep)
	    database_names.insert(directory_entry->d_namep);
    FOR_ALL_DIRECTORY_END;

    if (local_changes_only) {
	for (const unpush_reference &reference : walk.references) {
	    indexed_reference indexed;
	    indexed.parent = reference.parent;
	    MAT_COPY(indexed.matrix, reference.matrix);
	    local_references[reference.child].push_back(indexed);
	}
    }
    const auto &references = local_changes_only ? local_references :
	database_references.references;

    for (const unpush_group &group : groups) {
	bool eligible = true;
	for (size_t record_index : group.records) {
	    struct directory *dp = records[record_index].dp;
	    auto references_it = references.find(dp);
	    size_t reference_count = references_it == references.end() ? 0 :
		references_it->second.size();
	    if (walk.root_primitives.find(dp) != walk.root_primitives.end() ||
		dp->d_nref < 0 || reference_count == 0 ||
		reference_count != static_cast<size_t>(dp->d_nref)) {
		eligible = false;
		break;
	    }

	    for (const indexed_reference &reference : references_it->second) {
		if (!replacement_matrix_is_writable(reference.matrix,
			records[record_index].canonical_to_input)) {
		    eligible = false;
		    break;
		}
	    }
	    if (!eligible)
		break;
	}
	if (!eligible) {
	    deferred_groups++;
	    continue;
	}

	std::string canonical_name;
	do {
	    canonical_name = std::string(CANONICAL_NAME_PREFIX) +
		std::to_string(next_name_id++);
	} while (database_names.find(canonical_name) != database_names.end());
	database_names.insert(canonical_name);

	planned_group plan;
	plan.group = &group;
	plan.canonical_name = canonical_name;
	planned_groups.push_back(plan);
	for (size_t record_index : group.records) {
	    rewrite_target target;
	    target.canonical_name = canonical_name;
	    MAT_COPY(target.canonical_to_input,
		records[record_index].canonical_to_input);
	    targets[records[record_index].dp->d_namep] = target;
	}
    }

    if (planned_groups.empty()) {
	bu_vls_printf(gedp->ged_result_str,
	    "unpush: no fully-contained groups are currently safe to rewrite"
	    " (%zu deferred)\n", deferred_groups);
	return BRLCAD_OK;
    }

    for (const planned_group &plan : planned_groups) {
	for (size_t record_index : plan.group->records) {
	    auto references_it = references.find(records[record_index].dp);
	    if (references_it == references.end())
		continue;
	    for (const indexed_reference &reference : references_it->second)
		parents.insert(reference.parent);
	}
    }

    std::vector<struct directory *> ordered_parents(parents.begin(), parents.end());
    std::sort(ordered_parents.begin(), ordered_parents.end(),
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
    if (preparation_failed) {
	free_internals(updates);
	free_internals(originals);
	bu_vls_printf(gedp->ged_result_str,
	    "unpush: unable to prepare parent rewrites; database unchanged\n");
	return BRLCAD_ERROR;
    }

    std::vector<std::string> written_canonical_names;
    bool canonical_write_failed = false;
    for (const planned_group &plan : planned_groups) {
	struct directory *representative =
	    records[plan.group->records.front()].dp;
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

    std::vector<struct directory *> attempted_parents;
    bool parent_write_failed = false;
    for (struct directory *parent : ordered_parents) {
	attempted_parents.push_back(parent);
	if (rt_db_put_internal(parent, gedp->dbip, updates[parent]) < 0) {
	    parent_write_failed = true;
	    break;
	}
    }

    std::set<std::string> canonical_names(written_canonical_names.begin(),
	written_canonical_names.end());
    bool validation_failed = false;
    if (!parent_write_failed) {
	for (struct directory *parent : ordered_parents) {
	    if (!validate_parent_rewrite(gedp->dbip, parent, canonical_names,
		    expectations[parent], tol)) {
		validation_failed = true;
		break;
	    }
	}
    }

    if (parent_write_failed || validation_failed) {
	if (!parent_write_failed)
	    attempted_parents = ordered_parents;
	bool rollback_ok = rollback_parents(gedp->dbip, attempted_parents, originals);
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
	    parent_write_failed ? "a parent write failed" :
	    "post-write validation failed",
	    rollback_ok ? (cleanup_ok ? "original parents restored" :
		"original parents restored, but redundant canonical objects remain") :
		"rollback was incomplete; canonical objects were retained to avoid missing references");
	return BRLCAD_ERROR;
    }

    free_internals(updates);
    free_internals(originals);

    db_update_nref(gedp->dbip);
    size_t removed_objects = 0;
    size_t retained_objects = 0;
    bool deletion_failed = false;
    for (const planned_group &plan : planned_groups) {
	for (size_t record_index : plan.group->records) {
	    struct directory *old_object = records[record_index].dp;
	    if (old_object->d_nref != 0) {
		retained_objects++;
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
	"  original objects removed: %zu\n"
	"  original objects retained: %zu\n"
	"  groups deferred: %zu\n",
	planned_groups.size(), ordered_parents.size(), removed_objects,
	retained_objects, deferred_groups);
    if (verbosity > 0) {
	for (const planned_group &plan : planned_groups)
	    bu_vls_printf(gedp->ged_result_str, "  wrote %s\n",
		plan.canonical_name.c_str());
    }
    if (deletion_failed) {
	bu_vls_printf(gedp->ged_result_str,
	    "unpush: one or more unreferenced originals could not be removed; "
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
	if (!(root->d_flags & RT_DIR_COMB))
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
	record.bucket = canonical_bucket(object, tol);
	records.push_back(record);
	buckets[record.bucket].push_back(records.size() - 1);
    }

    std::vector<unpush_group> groups = verify_groups(gedp->dbip, records, buckets, tol, mode);

    std::set<struct directory *> grouped_primitives;
    for (const unpush_group &group : groups) {
	for (size_t record_index : group.records)
	    grouped_primitives.insert(records[record_index].dp);
    }
    database_reference_index database_references;
    database_references.targets = &grouped_primitives;
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
	"  potential facetize primitive reuses: %zu\n",
	dry_run ? "dry-run" : "rewrite", mode_name(mode),
	write_mode_constrained ?
	    "  requested affine mode constrained to similarity for database writes\n" : "",
	local_changes_only ? "yes" : "no", walk.primitives.size(),
	records.size(), unsupported, failures, groups.size(), grouped_objects,
	duplicate_objects, rewritable_references, walk.root_primitives.size(),
	grouped_exposed.size(), estimated_granule_reduction, potential_reuse);

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
	    database_references, tol, mode, local_changes_only != 0, verbosity);
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
