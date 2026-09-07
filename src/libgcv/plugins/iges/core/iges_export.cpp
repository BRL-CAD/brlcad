/* BRL-CAD
 * Copyright (c) 1993-2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include "common.h"
#include "iges_export.h"

#include <cmath>
#include <exception>
#include <map>
#include <set>
#include <stdexcept>

#include "bu/debug.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "nmg.h"
#include "rt/geom.h"

namespace brlcad {
namespace iges {
namespace {

void
report_progress(const ExportOptions &options, const char *stage, const char *activity,
    size_t completed = 0, size_t total = 0)
{
    if (options.progress)
	options.progress(stage, activity, completed, total);
}

constexpr size_t MAX_HIERARCHY_DEPTH = 4096;

struct FacetContext {
    Writer &writer;
    const std::string &name;
    const struct rt_comb_internal *properties = nullptr;
    ExportedEntity result;
    std::exception_ptr exception;
    bool failed = false;
};

// Keep non-local library error handling inside functions with no automatic
// C++ owners.  The caller retains sole ownership of the Boolean tree.
union tree *
evaluate_boolean(struct db_tree_state *state, union tree *tree, int *failed)
{
    union tree *result = nullptr;
    const uint32_t debug = nmg_debug;
    if (!BU_SETJUMP) {
	nmg_model_fuse(*state->ts_m, &rt_vlfree, state->ts_tol);
	result = nmg_booltree_evaluate(tree, &rt_vlfree, state->ts_tol);
    } else {
	*failed = 1;
	nmg_debug = debug;
    }
    BU_UNSETJUMP;
    return *failed ? nullptr : result;
}

void
release_tree(union tree *tree)
{
    if (!BU_SETJUMP)
	db_free_tree(tree);
    else
	bu_log("IGES: damaged NMG tree could not be fully released\n");
    BU_UNSETJUMP;
}

void
release_model(struct model *model)
{
    if (!model)
	return;
    if (!BU_SETJUMP)
	nmg_km(model);
    else
	bu_log("IGES: damaged NMG model could not be fully released\n");
    BU_UNSETJUMP;
}

union tree *
empty_tree()
{
    union tree *tree;
    BU_ALLOC(tree, union tree);
    RT_TREE_INIT(tree);
    tree->tr_op = OP_NOP;
    return tree;
}

union tree *
facet_leaf(struct db_tree_state *state, const struct db_full_path *path,
    struct rt_db_internal *internal, void *data)
{
    auto *context = static_cast<FacetContext *>(data);
    if (internal->idb_major_type != DB5_MAJORTYPE_BRLCAD || !supported_primitive(internal->idb_type)) {
	context->failed = true;
	return empty_tree();
    }
    union tree *result = nullptr;
    const uint32_t debug = nmg_debug;
    if (!BU_SETJUMP)
	result = rt_booltree_leaf_tess(state, path, internal, &rt_vlfree);
    else {
	context->failed = true;
	nmg_debug = debug;
    }
    BU_UNSETJUMP;
    if (!result) {
	context->failed = true;
	return empty_tree();
    }
    return result;
}

union tree *
facet_region(struct db_tree_state *state, const struct db_full_path *,
    union tree *tree, void *data)
{
    auto &context = *static_cast<FacetContext *>(data);
    int failed = 0;
    union tree *evaluated = context.failed || tree->tr_op == OP_NOP ? nullptr :
	evaluate_boolean(state, tree, &failed);
    context.failed = context.failed || failed != 0 || !evaluated;
    if (!context.failed) {
	try {
	    report_progress(context.writer.options(), "NMG export", context.name.c_str());
	    const auto geometry = export_nmg_region(context.writer, *evaluated->tr_d.td_r,
		context.name + ".shape");
	    if (geometry) {
		ParameterWriter parameters(430);
		parameters.integer(geometry.directory);
		EntitySpec specification(430);
		specification.form = 1;
		context.result = {context.writer.named_entity(specification, parameters,
		    context.name, context.properties), true};
	    }
	} catch (...) {
	    context.exception = std::current_exception();
	}
    }
    release_tree(tree);
    return empty_tree();
}

ExportedEntity
facetize(Writer &writer, struct db_i *database, const std::string &name,
    const struct rt_comb_internal &properties)
{
    FacetContext context{writer, name, &properties, {}, {}, false};
    struct model *model = nmg_mm();
    struct db_tree_state state;
    RT_DBTS_INIT(&state);
    state.ts_dbip = database;
    state.ts_tol = &writer.options().tolerance;
    state.ts_ttol = &writer.options().tessellation;
    state.ts_m = &model;
    const char *root = name.c_str();
    const int status = db_walk_tree(database, 1, &root, 1, &state,
	nullptr, facet_region, facet_leaf, &context);
    release_model(model);
    db_free_db_tree_state(&state);
    if (context.exception)
	std::rethrow_exception(context.exception);
    return status || context.failed ? ExportedEntity{} : context.result;
}

class Exporter {
public:
    Exporter(struct db_i *database, Writer &writer) :
	database_(database), writer_(writer) {}

    ExportedEntity object(const std::string &name)
    {
	const auto found = objects_.find(name);
	if (found != objects_.end())
	    return found->second;
	if (active_.size() >= MAX_HIERARCHY_DEPTH || !active_.insert(name).second) {
	    writer_.omission("cyclic or excessively deep combination: " + name);
	    return {};
	}
	report_progress(writer_.options(), "geometry export", name.c_str(), completed_roots_, total_roots_);
	ExportedEntity result;
	try {
	    struct directory *entry = db_lookup(database_, name.c_str(), LOOKUP_QUIET);
	    if (!entry)
		throw std::domain_error("requested object was not found");
	    Internal internal;
	    if (rt_db_get_internal(&internal.value, entry, database_, nullptr) < 0)
		throw std::domain_error("could not read geometry");
	    if (internal.value.idb_major_type != DB5_MAJORTYPE_BRLCAD)
		throw std::domain_error("is non-geometric data and cannot be exported");
	    if (internal.value.idb_type != ID_COMBINATION && !supported_primitive(internal.value.idb_type))
		throw std::domain_error("primitive type is not supported by the IGES exporter");
	    if (internal.value.idb_type == ID_COMBINATION) {
		const auto &combination = *static_cast<const struct rt_comb_internal *>(internal.value.idb_ptr);
		if (writer_.options().mode != ExportOptions::Mode::Csg && combination.region_flag)
		    result = facetize(writer_, database_, name, combination);
		else
		    result = combination_entity(combination, name);
	    } else if (writer_.options().mode == ExportOptions::Mode::Csg) {
		result = export_primitive(writer_, internal.value, name);
	    } else {
		result = export_nmg(writer_, internal.value, name);
	    }
	    if (!result)
		throw std::domain_error("failed to translate geometry to IGES format");
	} catch (const std::domain_error &error) {
	    writer_.omission(name + ": " + error.what());
	}
	active_.erase(name);
	objects_[name] = result;
	return result;
    }

    bool run(const std::vector<std::string> &roots)
    {
	total_roots_ = roots.size();
	bool wrote = false;
	for (const auto &root : roots) {
	    wrote = static_cast<bool>(object(root)) || wrote;
	    ++completed_roots_;
	}
	// A selected definition may also occur beneath another selected root.
	// Give its unplaced selection a separate instance so root reconciliation
	// cannot discard it as merely a dependency of the placed occurrence.
	for (const auto &root : roots) {
	    if (!referenced_.erase(root))
		continue;
	    const auto geometry = objects_.at(root);
	    if (!geometry)
		continue;
	    ParameterWriter parameters(430);
	    parameters.integer(geometry.directory);
	    EntitySpec specification(430);
	    specification.form = geometry.brep ? 1 : 0;
	    writer_.named_entity(specification, parameters, root);
	}
	return wrote;
    }

private:
    bool tokens(const union tree *tree, std::vector<int> &postfix, bool &brep, size_t depth = 0)
    {
	if (!tree || depth >= MAX_HIERARCHY_DEPTH)
	    return false;
	if (tree->tr_op == OP_DB_LEAF) {
	    auto geometry = object(tree->tr_l.tl_name);
	    if (!geometry)
		return false;
	    referenced_.insert(tree->tr_l.tl_name);
	    if (tree->tr_l.tl_mat && !bn_mat_is_identity(tree->tr_l.tl_mat))
		geometry.directory = writer_.instance(geometry, tree->tr_l.tl_mat);
	    brep = brep || geometry.brep;
	    postfix.push_back(-geometry.directory);
	    return geometry.directory != 0;
	}
	int operation;
	switch (tree->tr_op) {
	    case OP_UNION: operation = 1; break;
	    case OP_INTERSECT: operation = 2; break;
	    case OP_SUBTRACT: operation = 3; break;
	    default: return false;
	}
	// Visit both branches even if one fails so useful sibling geometry is
	// retained in a readable partial export.
	const bool left = tokens(tree->tr_b.tb_left, postfix, brep, depth + 1);
	const bool right = tokens(tree->tr_b.tb_right, postfix, brep, depth + 1);
	postfix.push_back(operation);
	return left && right;
    }

    ExportedEntity combination_entity(const struct rt_comb_internal &combination,
	const std::string &name)
    {
	std::vector<int> postfix;
	bool brep = false;
	if (!tokens(combination.tree, postfix, brep) || postfix.empty())
	    return {};
	const int type = postfix.size() == 1 ? 430 : 180;
	ParameterWriter parameters(type);
	if (type == 430)
	    parameters.integer(-postfix.front());
	else {
	    parameters.integer(postfix.size());
	    for (int token : postfix)
		parameters.integer(token);
	}
	EntitySpec specification(type);
	specification.form = brep ? 1 : 0;
	return {writer_.named_entity(specification, parameters, name, &combination), brep};
    }

    struct db_i *database_;
    Writer &writer_;
    std::map<std::string, ExportedEntity> objects_;
    std::set<std::string> active_;
    std::set<std::string> referenced_;
    size_t completed_roots_ = 0;
    size_t total_roots_ = 0;
};

void
collect_region_occurrences(struct db_i *database, const RegionOccurrence &occurrence,
    std::set<std::string> &active, std::vector<RegionOccurrence> &regions)
{
    if (active.size() >= MAX_HIERARCHY_DEPTH || !active.insert(occurrence.name).second)
	throw std::domain_error("cyclic or excessively deep combination: " + occurrence.path);
    const auto *entry = db_lookup(database, occurrence.name.c_str(), LOOKUP_QUIET);
    if (!entry)
	throw std::domain_error("requested object was not found: " + occurrence.path);
    if ((entry->d_flags & RT_DIR_REGION) || !(entry->d_flags & RT_DIR_COMB)) {
	regions.push_back(occurrence);
	active.erase(occurrence.name);
	return;
    }
    Internal internal;
    if (rt_db_get_internal(&internal.value, entry, database, nullptr) < 0)
	throw std::domain_error("cannot inspect combination: " + occurrence.path);
    std::vector<const union tree *> pending = {
	static_cast<const struct rt_comb_internal *>(internal.value.idb_ptr)->tree};
    while (!pending.empty()) {
	const auto *tree = pending.back();
	pending.pop_back();
	if (!tree)
	    continue;
	if (tree->tr_op == OP_DB_LEAF) {
	    RegionOccurrence child{tree->tr_l.tl_name, occurrence.path + '/' + tree->tr_l.tl_name,
		occurrence.matrix};
	    if (tree->tr_l.tl_mat)
		bn_mat_mul(child.matrix.data(), occurrence.matrix.data(), tree->tr_l.tl_mat);
	    collect_region_occurrences(database, child, active, regions);
	}
	else if (tree->tr_op == OP_UNION || tree->tr_op == OP_SUBTRACT || tree->tr_op == OP_INTERSECT) {
	    pending.push_back(tree->tr_b.tb_right);
	    pending.push_back(tree->tr_b.tb_left);
	} else {
	    throw std::domain_error("unsupported combination operator: " + occurrence.path);
	}
    }
    active.erase(occurrence.name);
}

} // namespace

void
validate_export_options(ExportOptions &options)
{
    if (!std::isfinite(options.tolerance.dist) || options.tolerance.dist <= 0.0 ||
	!std::isfinite(options.tessellation.abs) || options.tessellation.abs < 0.0 ||
	!std::isfinite(options.tessellation.rel) || options.tessellation.rel < 0.0 ||
	!std::isfinite(options.tessellation.norm) || options.tessellation.norm < 0.0 || options.tessellation.norm > M_PI)
	throw std::invalid_argument("invalid export tolerance");
    options.tolerance.dist_sq = options.tolerance.dist * options.tolerance.dist;
}

bool
export_objects(Writer &writer, struct db_i *database, const std::vector<std::string> &roots)
{
    Exporter exporter(database, writer);
    return exporter.run(roots);
}

std::vector<RegionOccurrence>
collect_regions(struct db_i *database, const std::vector<std::string> &roots)
{
    std::vector<RegionOccurrence> regions;
    std::set<std::string> active;
    for (const auto &root : roots) {
	RegionOccurrence occurrence{root, root, {}};
	MAT_IDN(occurrence.matrix.data());
	collect_region_occurrences(database, occurrence, active, regions);
    }
    return regions;
}

bool
export_occurrence(Writer &writer, struct db_i *database, const RegionOccurrence &occurrence)
{
    Exporter exporter(database, writer);
    const auto geometry = exporter.object(occurrence.name);
    if (!geometry)
	return false;
    // Definitions stay local; each occurrence contributes its placement once.
    if (occurrence.path != occurrence.name || !bn_mat_is_identity(occurrence.matrix.data())) {
	ParameterWriter parameters(430);
	parameters.integer(geometry.directory);
	EntitySpec specification(430);
	specification.form = geometry.brep ? 1 : 0;
	specification.transform = writer.transform(occurrence.matrix.data());
	writer.named_entity(specification, parameters, occurrence.path);
    }
    return true;
}

} // namespace iges
} // namespace brlcad

/*
 * Local Variables:
 * mode: C++
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
