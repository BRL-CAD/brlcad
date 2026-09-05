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

#include <cstdlib>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "bu/app.h"
#include "bu/file.h"
#include "bu/log.h"
#include "ged.h"
#include "wdb.h"

#include "../../canonicalize_private.h"
#include "../reuse.h"


namespace {

const char *FIRST_NAME = "first.s";
const char *SECOND_NAME = "second.s";
const char *SCALED_NAME = "scaled.s";
const char *FIRST_ASSEMBLY = "first.c";
const char *SECOND_ASSEMBLY = "second.c";
const char *SCALED_ASSEMBLY = "scaled.c";
const char *FIRST_REGION = "first.r";
const char *SECOND_REGION = "second.r";
const char *SCALED_REGION = "scaled.r";
const char *FIRST_REGION_BOT = "first.r.bot";
const char *SECOND_REGION_BOT = "second.r.bot";
const char *SCALED_REGION_BOT = "scaled.r.bot";
const char *REGION_OUTPUT = "regions.bot";
const char *INTERMEDIATE_SUFFIX = ".facetize_reuse.bot";
constexpr size_t REUSE_OBJECT_COUNT = 3;
constexpr int REGION_ID_BASE = 100;


int
capture_log(void *data, void *message)
{
    if (data && message)
	static_cast<std::string *>(data)->append(
	    static_cast<const char *>(message));
    return 0;
}


int
run_region_facetize(const char *path, std::string &output)
{
    struct ged *gedp = ged_open("db", path, 1);
    if (!gedp)
	return BRLCAD_ERROR;

    struct bu_hook_list saved_hooks = BU_HOOK_LIST_INIT_ZERO;
    bu_log_hook_save_all(&saved_hooks);
    bu_log_hook_delete_all();
    bu_log_add_hook(capture_log, &output);
    const char *arguments[] = {
	"facetize", "-r", "--jobs", "2", "--methods", "NMG",
	FIRST_REGION, SECOND_REGION, SCALED_REGION, REGION_OUTPUT, nullptr
    };
    int result = ged_exec(gedp, 10, arguments);
    output.append(bu_vls_cstr(gedp->ged_result_str));
    ged_close(gedp);

    bu_log_hook_delete_all();
    bu_log_hook_restore_all(&saved_hooks);
    bu_hook_delete_all(&saved_hooks);
    return result;
}


int
write_analytic_database(const char *path)
{
    struct rt_wdb *wdbp = wdb_fopen(path);
    if (!wdbp)
	return BRLCAD_ERROR;

    const point_t origin = VINIT_ZERO;
    const point_t moved = {10.0, -3.0, 2.0};
    const vect_t a = {2.0, 0.0, 0.0};
    const vect_t b = {0.0, 3.0, 0.0};
    const vect_t c = {0.0, 0.0, 4.0};
    const vect_t rotated_a = {0.0, 2.0, 0.0};
    const vect_t rotated_b = {-3.0, 0.0, 0.0};
    const vect_t scaled_a = {4.0, 0.0, 0.0};
    const vect_t scaled_b = {0.0, 6.0, 0.0};
    const vect_t scaled_c = {0.0, 0.0, 8.0};

    int result = mk_id(wdbp, "facetize reuse planner test") ||
	mk_ell(wdbp, FIRST_NAME, origin, a, b, c) ||
	mk_ell(wdbp, SECOND_NAME, moved, rotated_a, rotated_b, c) ||
	mk_ell(wdbp, SCALED_NAME, origin, scaled_a, scaled_b, scaled_c);
    const char *solid_names[] = {FIRST_NAME, SECOND_NAME, SCALED_NAME};
    const char *assembly_names[] = {
	FIRST_ASSEMBLY, SECOND_ASSEMBLY, SCALED_ASSEMBLY
    };
    const char *region_names[] = {FIRST_REGION, SECOND_REGION, SCALED_REGION};
    for (size_t i = 0; !result && i < REUSE_OBJECT_COUNT; i++) {
	struct wmember assembly_member;
	BU_LIST_INIT(&assembly_member.l);
	mk_addmember(solid_names[i], &assembly_member.l, nullptr, WMOP_UNION);
	mk_addmember(solid_names[i], &assembly_member.l, nullptr, WMOP_UNION);
	result = mk_lcomb(wdbp, assembly_names[i], &assembly_member, 0,
	    nullptr, nullptr, nullptr, 0);
	if (result)
	    break;
	struct wmember region_member;
	BU_LIST_INIT(&region_member.l);
	mk_addmember(assembly_names[i], &region_member.l, nullptr, WMOP_UNION);
	/* Repeating the same subassembly exercises reuse of a single named DAG
	 * node, independently of equivalent-but-distinct object detection. */
	mk_addmember(assembly_names[i], &region_member.l, nullptr, WMOP_UNION);
	result = mk_lrcomb(wdbp, region_names[i], &region_member, 1, nullptr,
	    nullptr, nullptr, REGION_ID_BASE + static_cast<int>(i), 0, 0, 0,
	    0);
    }
    db_close(wdbp->dbip);
    return result ? BRLCAD_ERROR : BRLCAD_OK;
}


int
write_working_database(const char *path)
{
    struct rt_wdb *wdbp = wdb_fopen(path);
    if (!wdbp)
	return BRLCAD_ERROR;

    fastf_t vertices[] = {
	0.0, 0.0, 0.0,
	2.0, 0.0, 0.0,
	0.0, 3.0, 0.0,
	0.0, 0.0, 4.0
    };
    int faces[] = {0, 2, 1, 0, 1, 3, 1, 2, 3, 2, 0, 3};
    const point_t origin = VINIT_ZERO;
    const point_t moved = {10.0, -3.0, 2.0};
    const vect_t rotated_a = {0.0, 2.0, 0.0};
    const vect_t rotated_b = {-3.0, 0.0, 0.0};
    const vect_t c = {0.0, 0.0, 4.0};
    const vect_t scaled_a = {4.0, 0.0, 0.0};
    const vect_t scaled_b = {0.0, 6.0, 0.0};
    const vect_t scaled_c = {0.0, 0.0, 8.0};

    int result = mk_id(wdbp, "facetize reuse clone test") ||
	mk_bot(wdbp, FIRST_NAME, RT_BOT_SOLID, RT_BOT_CCW, 0, 4, 4,
	    vertices, faces, nullptr, nullptr) ||
	mk_bot(wdbp, FIRST_REGION_BOT, RT_BOT_SOLID, RT_BOT_CCW, 0, 4, 4,
	    vertices, faces, nullptr, nullptr) ||
	mk_ell(wdbp, SECOND_NAME, moved, rotated_a, rotated_b, c) ||
	mk_ell(wdbp, SCALED_NAME, origin, scaled_a, scaled_b, scaled_c);
    if (!result) {
	struct wmember assembly_member;
	BU_LIST_INIT(&assembly_member.l);
	mk_addmember(FIRST_NAME, &assembly_member.l, nullptr, WMOP_UNION);
	result = mk_lcomb(wdbp, FIRST_ASSEMBLY, &assembly_member, 0,
	    nullptr, nullptr, nullptr, 0);
    }
    db_close(wdbp->dbip);
    return result ? BRLCAD_ERROR : BRLCAD_OK;
}


size_t
matching_tree_leaves(const union tree *tree, const char *name)
{
    if (!tree)
	return 0;
    switch (tree->tr_op) {
	case OP_DB_LEAF:
	    return BU_STR_EQUAL(tree->tr_l.tl_name, name) ? 1 : 0;
	case OP_UNION:
	case OP_INTERSECT:
	case OP_SUBTRACT:
	case OP_XOR:
	    return matching_tree_leaves(tree->tr_b.tb_left, name) +
		matching_tree_leaves(tree->tr_b.tb_right, name);
	case OP_NOT:
	case OP_GUARD:
	case OP_XNOP:
	    return matching_tree_leaves(tree->tr_b.tb_left, name);
	default:
	    return 0;
    }
}


bool
combination_has_leaves(const char *path, const char *combination_name,
	const char *leaf_name, size_t expected_count)
{
    struct db_i *dbip = db_open(path, DB_OPEN_READONLY);
    if (!dbip || db_dirbuild(dbip) < 0) {
	if (dbip)
	    db_close(dbip);
	return false;
    }
    struct directory *dp = db_lookup(dbip, combination_name, LOOKUP_QUIET);
    struct rt_db_internal intern;
    RT_DB_INTERNAL_INIT(&intern);
    bool matches = dp && (dp->d_flags & RT_DIR_COMB) &&
	rt_db_get_internal(&intern, dp, dbip, nullptr) >= 0 &&
	intern.idb_minor_type == ID_COMBINATION;
    if (matches) {
	const auto *comb =
	    static_cast<const struct rt_comb_internal *>(intern.idb_ptr);
	matches = matching_tree_leaves(comb->tree, leaf_name) ==
	    expected_count;
    }
    if (intern.idb_ptr)
	rt_db_free_internal(&intern);
    db_close(dbip);
    return matches;
}


bool
region_outputs_equal(const char *path, const mat_t representative_to_member,
		     const struct bn_tol *tol)
{
    struct db_i *dbip = db_open(path, DB_OPEN_READONLY);
    if (!dbip || db_dirbuild(dbip) < 0) {
	if (dbip)
	    db_close(dbip);
	return false;
    }

    struct directory *first_dp = db_lookup(dbip, FIRST_REGION_BOT,
	LOOKUP_QUIET);
    struct directory *second_dp = db_lookup(dbip, SECOND_REGION_BOT,
	LOOKUP_QUIET);
    struct directory *scaled_dp = db_lookup(dbip, SCALED_REGION_BOT,
	LOOKUP_QUIET);
    struct directory *output_dp = db_lookup(dbip, REGION_OUTPUT,
	LOOKUP_QUIET);
    struct rt_db_internal first;
    struct rt_db_internal second;
    RT_DB_INTERNAL_INIT(&first);
    RT_DB_INTERNAL_INIT(&second);
    bool temporary_results_absent = true;
    const char *assemblies[] = {
	FIRST_ASSEMBLY, SECOND_ASSEMBLY, SCALED_ASSEMBLY
    };
    for (const char *assembly : assemblies) {
	std::string temporary_name = std::string(assembly) +
	    INTERMEDIATE_SUFFIX;
	temporary_results_absent = temporary_results_absent &&
	    !db_lookup(dbip, temporary_name.c_str(), LOOKUP_QUIET);
    }
    bool equal = temporary_results_absent && first_dp && second_dp &&
	scaled_dp && output_dp &&
	first_dp->d_minor_type == ID_BOT && second_dp->d_minor_type == ID_BOT &&
	scaled_dp->d_minor_type == ID_BOT &&
	rt_db_get_internal(&first, first_dp, dbip, nullptr) >= 0 &&
	rt_db_get_internal(&second, second_dp, dbip, nullptr) >= 0 &&
	_ged_transformed_geometry_equal(&first, representative_to_member,
	    &second, bn_mat_identity, tol);
    if (second.idb_ptr)
	rt_db_free_internal(&second);
    if (first.idb_ptr)
	rt_db_free_internal(&first);
    db_close(dbip);
    return equal;
}

} /* namespace */


int
main(int argc, const char *argv[])
{
    if (argc != 3)
	return 1;
    bu_setprogname(argv[0]);
    const char *configured_cache = std::getenv("BU_DIR_CACHE");
    if (configured_cache)
	bu_mkdir(configured_cache);
    char cache_directory[MAXPATHLEN];
    bu_dir(cache_directory, sizeof(cache_directory), BU_DIR_CACHE, nullptr);
    bu_mkdir(cache_directory);
    const char *source_file = argv[1];
    const char *work_file = argv[2];
    (void)bu_file_delete(source_file);
    (void)bu_file_delete(work_file);
    if (write_analytic_database(source_file) != BRLCAD_OK)
	return 1;

    struct db_i *source_dbip = db_open(source_file, DB_OPEN_READONLY);
    if (!source_dbip || db_dirbuild(source_dbip) < 0)
	return 1;
    std::vector<struct directory *> inputs;
    inputs.push_back(db_lookup(source_dbip, FIRST_NAME, LOOKUP_QUIET));
    inputs.push_back(db_lookup(source_dbip, SECOND_NAME, LOOKUP_QUIET));
    inputs.push_back(db_lookup(source_dbip, SCALED_NAME, LOOKUP_QUIET));

    const struct bn_tol tol = BN_TOL_INIT_TOL;
    FacetizeReusePlan plan;
    std::vector<struct directory *> representatives;
    int failed = facetize_reuse_plan(&plan, source_dbip, inputs, &tol) !=
	BRLCAD_OK || plan.groups.size() != 1 || plan.reuse_count() != 1;
    if (!failed) {
	plan.representatives(inputs, representatives);
	failed = representatives.size() != 2 ||
	    plan.groups.front().representative != FIRST_NAME ||
	    plan.groups.front().members.front().name != SECOND_NAME;
    }
    std::vector<struct directory *> roots;
    roots.push_back(db_lookup(source_dbip, FIRST_REGION, LOOKUP_QUIET));
    roots.push_back(db_lookup(source_dbip, SECOND_REGION, LOOKUP_QUIET));
    roots.push_back(db_lookup(source_dbip, SCALED_REGION, LOOKUP_QUIET));
    FacetizeRegionReusePlan region_plan;
    if (!failed)
	failed = facetize_region_reuse_plan(&region_plan, source_dbip, roots,
	    &tol) != BRLCAD_OK || region_plan.groups.size() != 1 ||
	    region_plan.reuse_count() != 1;
    if (!failed) {
	region_plan.representatives(roots, representatives);
	failed = representatives.size() != 2 ||
	    region_plan.groups.front().representative != FIRST_REGION ||
	    region_plan.groups.front().members.front().name != SECOND_REGION ||
	    !_ged_matrices_numerically_equal(
		region_plan.groups.front().members.front().representative_to_member,
		plan.groups.front().members.front().representative_to_member,
		&tol);
    }
    FacetizeIntermediateReusePlan intermediate_plan;
    if (!failed)
	failed = facetize_intermediate_reuse_plan(&intermediate_plan,
	    source_dbip, roots, &tol) != BRLCAD_OK ||
	    intermediate_plan.groups.size() != 2 ||
	    intermediate_plan.substitution_count() != 3 ||
	    intermediate_plan.reuse_count() != 4;
    db_close(source_dbip);
    if (failed || write_working_database(work_file) != BRLCAD_OK)
	return 1;

    std::map<std::string, std::string> snapshot_substitution = {
	{FIRST_ASSEMBLY, FIRST_REGION_BOT}
    };
    if (facetize_intermediate_reuse_apply(work_file, snapshot_substitution) !=
	    BRLCAD_OK || !combination_has_leaves(work_file, FIRST_ASSEMBLY,
		FIRST_REGION_BOT, 1))
	return 1;
    source_dbip = db_open(source_file, DB_OPEN_READONLY);
    if (!source_dbip || db_dirbuild(source_dbip) < 0 ||
	    facetize_intermediate_reuse_restore(work_file, source_dbip,
		snapshot_substitution) != BRLCAD_OK) {
	if (source_dbip)
	    db_close(source_dbip);
	return 1;
    }
    db_close(source_dbip);
    if (!combination_has_leaves(work_file, FIRST_ASSEMBLY, FIRST_NAME, 2))
	return 1;

    std::vector<std::string> clone_failures;
    size_t written_count = 0;
    bool write_unsafe = false;
    std::set<std::string> completed = {FIRST_NAME};
    if (facetize_reuse_write_clones(work_file, plan, completed,
	    clone_failures, &written_count, &write_unsafe) != BRLCAD_OK ||
	!clone_failures.empty() || write_unsafe || written_count != 1)
	return 1;

    std::map<std::string, std::string> region_outputs = {
	{FIRST_REGION, FIRST_REGION_BOT},
	{SECOND_REGION, SECOND_REGION_BOT},
	{SCALED_REGION, SCALED_REGION_BOT}
    };
    completed.clear();
    completed.insert(FIRST_REGION);
    if (facetize_region_reuse_write_clones(work_file, region_plan,
	    region_outputs, completed, clone_failures, &written_count,
	    &write_unsafe) != BRLCAD_OK || !clone_failures.empty() ||
	write_unsafe || written_count != 1)
	return 1;

    struct db_i *work_dbip = db_open(work_file, DB_OPEN_READONLY);
    if (!work_dbip || db_dirbuild(work_dbip) < 0)
	return 1;
    struct directory *first_dp = db_lookup(work_dbip, FIRST_NAME,
	LOOKUP_QUIET);
    struct directory *second_dp = db_lookup(work_dbip, SECOND_NAME,
	LOOKUP_QUIET);
    struct directory *scaled_dp = db_lookup(work_dbip, SCALED_NAME,
	LOOKUP_QUIET);
    struct directory *first_region_dp = db_lookup(work_dbip,
	FIRST_REGION_BOT, LOOKUP_QUIET);
    struct directory *second_region_dp = db_lookup(work_dbip,
	SECOND_REGION_BOT, LOOKUP_QUIET);
    struct rt_db_internal first;
    struct rt_db_internal second;
    struct rt_db_internal first_region;
    struct rt_db_internal second_region;
    RT_DB_INTERNAL_INIT(&first);
    RT_DB_INTERNAL_INIT(&second);
    RT_DB_INTERNAL_INIT(&first_region);
    RT_DB_INTERNAL_INIT(&second_region);
    failed = !first_dp || !second_dp || !scaled_dp || !first_region_dp ||
	!second_region_dp ||
	first_dp->d_minor_type != ID_BOT || second_dp->d_minor_type != ID_BOT ||
	scaled_dp->d_minor_type != ID_ELL ||
	rt_db_get_internal(&first, first_dp, work_dbip, nullptr) < 0 ||
	rt_db_get_internal(&second, second_dp, work_dbip, nullptr) < 0 ||
	rt_db_get_internal(&first_region, first_region_dp, work_dbip, nullptr) < 0 ||
	rt_db_get_internal(&second_region, second_region_dp, work_dbip, nullptr) < 0;
    if (!failed)
	failed = !_ged_transformed_geometry_equal(&first,
	    plan.groups.front().members.front().representative_to_member,
	    &second, bn_mat_identity, &tol);
    if (!failed)
	failed = !_ged_transformed_geometry_equal(&first_region,
	    region_plan.groups.front().members.front().representative_to_member,
	    &second_region, bn_mat_identity, &tol);
    if (second_region.idb_ptr)
	rt_db_free_internal(&second_region);
    if (first_region.idb_ptr)
	rt_db_free_internal(&first_region);
    if (second.idb_ptr)
	rt_db_free_internal(&second);
    if (first.idb_ptr)
	rt_db_free_internal(&first);
    db_close(work_dbip);

    std::string facetize_output;
    if (!failed) {
	int facetize_result = run_region_facetize(source_file, facetize_output);
	bool sharing_reported = facetize_output.find(
	    "sharing 1 rigid-equivalent region Boolean evaluation") !=
	    std::string::npos;
	bool reuse_reported = facetize_output.find(
	    "reused 1 completed region Boolean evaluation") !=
	    std::string::npos;
	bool intermediate_reported = facetize_output.find(
	    "prepared 2 reusable intermediate CSG results") !=
	    std::string::npos;
	bool outputs_equal = region_outputs_equal(source_file,
	    region_plan.groups.front().members.front().representative_to_member,
	    &tol);
	if (facetize_result != BRLCAD_OK || !sharing_reported ||
	    !reuse_reported || !intermediate_reported || !outputs_equal) {
	    bu_log("facetize region reuse integration failed: result=%d sharing=%d reuse=%d intermediate=%d geometry=%d\n%s\n",
		facetize_result, sharing_reported, reuse_reported,
		intermediate_reported, outputs_equal, facetize_output.c_str());
	    failed = true;
	}
    }

    (void)bu_file_delete(work_file);
    (void)bu_file_delete(source_file);
    return failed ? 1 : 0;
}
