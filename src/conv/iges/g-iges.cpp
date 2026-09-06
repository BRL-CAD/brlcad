/* BRL-CAD
 * Copyright (c) 1993-2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include "common.h"
#include "iges_writer.h"

#include <exception>
#include <filesystem>
#include <map>
#include <set>
#include <stdexcept>

#include "bu/app.h"
#include "bu/debug.h"
#include "bu/exit.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "bu/opt.h"
#include "nmg.h"
#include "rt/geom.h"

namespace {
using namespace brlcad::iges;
constexpr size_t MAX_HIERARCHY_DEPTH = 4096;

struct FacetContext {
    Writer &writer;
    ProgressReporter &progress;
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
	bu_log("g-iges: damaged NMG tree could not be fully released\n");
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
	bu_log("g-iges: damaged NMG model could not be fully released\n");
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
	    context.progress.update("NMG export", context.name.c_str());
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
    const struct rt_comb_internal &properties, ProgressReporter &progress)
{
    FacetContext context{writer, progress, name, &properties, {}, {}, false};
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
    Exporter(struct db_i *database, Writer &writer, ProgressReporter &progress) :
	database_(database), writer_(writer), progress_(progress) {}

    ExportedEntity object(const std::string &name)
    {
	const auto found = objects_.find(name);
	if (found != objects_.end())
	    return found->second;
	if (active_.size() >= MAX_HIERARCHY_DEPTH || !active_.insert(name).second) {
	    writer_.omission("cyclic or excessively deep combination: " + name);
	    return {};
	}
	progress_.update("geometry export", name.c_str(), completed_roots_, total_roots_);
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
		    result = facetize(writer_, database_, name, combination, progress_);
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
    ProgressReporter &progress_;
    std::map<std::string, ExportedEntity> objects_;
    std::set<std::string> active_;
    std::set<std::string> referenced_;
    size_t completed_roots_ = 0;
    size_t total_roots_ = 0;
};

struct RegionOccurrence {
    std::string name;
    std::string path;
    std::array<fastf_t, ELEMENTS_PER_MAT> matrix;
};

void
collect_regions(struct db_i *database, const RegionOccurrence &occurrence,
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
	    collect_regions(database, child, active, regions);
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

std::filesystem::path
multi_file_path(const std::filesystem::path &directory, const std::string &name)
{
    Vls sanitized;
    db_sanitize_name(&sanitized.value, name.c_str());
    const std::string stem = bu_vls_strlen(&sanitized.value) ? bu_vls_cstr(&sanitized.value) : "region";
    auto candidate = directory / (stem + ".igs");
    for (size_t suffix = 1; std::filesystem::exists(candidate); ++suffix)
	candidate = directory / (stem + '-' + std::to_string(suffix) + ".igs");
    return candidate;
}

int
convert(int argc, const char **argv)
{
    const char *program = argv[0];
    bu_setprogname(program);
    ExportOptions settings;
    int help = 0, faceted = 0, trimmed = 0, multi_file = 0, nurbs = 0, flatten = 0, verbose = 0;
    const char *output_path = nullptr;
    struct bu_opt_desc options[] = {
	{"h", "help", "", nullptr, &help, "print help and exit"},
	{"?", "", "", nullptr, &help, ""},
	{"f", "faceted", "", nullptr, &faceted, "tessellate each region to a faceted B-Rep"},
	{"t", "trimmed-surfaces", "", nullptr, &trimmed, "tessellate regions to trimmed surfaces"},
	{"m", "multi-file", "", nullptr, &multi_file, "write one trimmed-surface file per region"},
	{"s", "nurbs", "", nullptr, &nurbs, "write NMG facets using NURBS geometry"},
	{"", "flatten-brep", "", nullptr, &flatten, "write exact B-Reps as IGES 144 surfaces"},
	{"v", "verbose", "", nullptr, &verbose, "enable verbose diagnostics"},
	{"a", "absolute-tolerance", "MM", bu_opt_fastf_t, &settings.tessellation.abs, "absolute tessellation tolerance"},
	{"r", "relative-tolerance", "FRACTION", bu_opt_fastf_t, &settings.tessellation.rel, "relative tessellation tolerance"},
	{"n", "normal-tolerance", "RADIANS", bu_opt_fastf_t, &settings.tessellation.norm, "surface-normal tessellation tolerance"},
	{"d", "distance-tolerance", "MM", bu_opt_fastf_t, &settings.tolerance.dist, "minimum distance between distinct points"},
	{"x", "rt-debug", "HEX", parse_debug_mask, &rt_debug, "librt hexadecimal debug mask"},
	{"X", "nmg-debug", "HEX", parse_debug_mask, &nmg_debug, "NMG hexadecimal debug mask"},
	{"o", "output", "FILE", bu_opt_str, &output_path, "output file, or directory in multi-file mode"},
	{"P", "processors", "COUNT", bu_opt_int, &settings.processors, "maximum worker count (IGES conversion is serialized)"},
	BU_OPT_DESC_NULL
    };
    Vls messages;
    argc = bu_opt_parse(&messages.value, argc - 1, argv + 1, options);
    ++argv;
    if (help || argc < 2 || bu_vls_strlen(&messages.value)) {
	char *description = bu_opt_describe(options, nullptr);
	bu_log("Usage: %s [options] database.g object [object ...]\n%s", program, description ? description : "");
	bu_free(description, "IGES option description");
	if (bu_vls_strlen(&messages.value))
	    bu_log("%s\n", bu_vls_cstr(&messages.value));
	return help ? BRLCAD_OK : BRLCAD_ERROR;
    }
    if (faceted + trimmed + multi_file > 1 || (multi_file && !output_path) || settings.processors < 1 ||
	!std::isfinite(settings.tolerance.dist) || settings.tolerance.dist <= 0.0 ||
	!std::isfinite(settings.tessellation.abs) || settings.tessellation.abs < 0.0 ||
	!std::isfinite(settings.tessellation.rel) || settings.tessellation.rel < 0.0 ||
	!std::isfinite(settings.tessellation.norm) || settings.tessellation.norm < 0.0 || settings.tessellation.norm > M_PI)
	throw std::invalid_argument("invalid output mode, worker count, or tolerance");
    settings.mode = faceted ? ExportOptions::Mode::Faceted :
	trimmed || multi_file ? ExportOptions::Mode::Trimmed : ExportOptions::Mode::Csg;
    settings.nurbs_facets = nurbs != 0;
    settings.flatten_brep = flatten != 0;
    settings.verbose = verbose != 0;
    settings.tolerance.dist_sq = settings.tolerance.dist * settings.tolerance.dist;
    if (settings.processors > 1)
	bu_log("IGES: serializing conversion to protect NMG recovery and record ordering\n");
    validate_paths(argv[0], !multi_file && output_path ? std::vector<std::string>{output_path} : std::vector<std::string>{});
    if (multi_file && !std::filesystem::is_directory(output_path))
	throw std::invalid_argument("multi-file output must be an existing directory");
    ProgressReporter progress;
    std::unique_ptr<struct db_i, decltype(&db_close)> database(db_open(argv[0], DB_OPEN_READONLY), db_close);
    if (!database || db_dirbuild(database.get()) < 0)
	throw std::runtime_error("cannot open geometry database");
    std::vector<std::string> roots(argv + 1, argv + argc);
    if (multi_file) {
	std::vector<RegionOccurrence> regions;
	std::set<std::string> active;
	for (const auto &root : roots) {
	    RegionOccurrence occurrence{root, root, {}};
	    MAT_IDN(occurrence.matrix.data());
	    collect_regions(database.get(), occurrence, active, regions);
	}
	bool success = !regions.empty();
	size_t completed = 0;
	for (const auto &region : regions) {
	    const auto destination = multi_file_path(output_path, region.name);
	    validate_paths(argv[0], {destination.string()});
	    Writer writer(settings, {region.path});
	    Exporter exporter(database.get(), writer, progress);
	    const auto geometry = exporter.object(region.name);
	    progress.update("multi-file export", region.path.c_str(), ++completed, regions.size());
	    if (!geometry) {
		success = false;
		continue;
	    }
	    // Definitions stay in local coordinates.  Preserve each occurrence's
	    // accumulated placement exactly once, including repeated regions.
	    if (region.path != region.name || !bn_mat_is_identity(region.matrix.data())) {
		ParameterWriter parameters(430);
		parameters.integer(geometry.directory);
		EntitySpec specification(430);
		specification.form = geometry.brep ? 1 : 0;
		specification.transform = writer.transform(region.matrix.data());
		writer.named_entity(specification, parameters, region.path);
	    }
	    StagedFile staged(destination);
	    File output(std::fopen(staged.path().c_str(), "wb"));
	    if (!output)
		throw std::runtime_error("cannot create IGES output");
	    writer.finish(output.get(), argv[0], destination.string());
	    close_file(output);
	    staged.publish();
	    success = success && writer.omissions() == 0;
	}
	return success ? BRLCAD_OK : BRLCAD_ERROR;
    }
    Writer writer(settings, std::set<std::string>(roots.begin(), roots.end()));
    Exporter exporter(database.get(), writer, progress);
    const bool wrote = exporter.run(roots);
    progress.update("finalization", "writing IGES sections");
    if (output_path) {
	StagedFile staged(output_path);
	File output(std::fopen(staged.path().c_str(), "wb"));
	if (!output)
	    throw std::runtime_error("cannot create IGES output");
	writer.finish(output.get(), argv[0], output_path);
	close_file(output);
	staged.publish();
    } else {
	writer.finish(stdout, argv[0], "");
    }
    writer.print_statistics();
    return wrote && !writer.omissions() ? BRLCAD_OK : BRLCAD_ERROR;
}
} // namespace

int
main(int argc, const char **argv)
{
    try {
	return convert(argc, argv);
    } catch (const std::exception &error) {
	bu_log("g-iges: %s\n", error.what());
    } catch (...) {
	bu_log("g-iges: unexpected conversion failure\n");
    }
    return BRLCAD_ERROR;
}

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
