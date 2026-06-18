/*              T E S T _ S T A T E _ S E R V I C E S . C P P
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
/** @file test_state_services.cpp
 *
 * Public GED state-service coverage.  This test deliberately avoids removed
 * migration-internal APIs, which must not be preserved by test contracts.
 *
 * Usage: ged_test_state_services <dir-containing-moss.g>
 */

#include "common.h"

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <limits>
#include <string>
#include <vector>

#include "bu/app.h"
#include "bu/avs.h"
#include "bu/env.h"
#include "bu/file.h"
#include "bu/hash.h"
#include "bu/str.h"
#include "bu/vls.h"
#include "ged.h"
#include "ged/bsg_ged_draw.h"
#include "ged/db_index.h"
#include "ged/event_txn.h"
#include "ged/selection_state.h"
#include "nmg.h"
#include "rt/db_attr.h"
#include "wdb.h"

static int g_failures = 0;
static const size_t default_index_scale_fanout = 8192;
static const size_t slow_index_scale_fanout = 100000;

#define CHECK(_cond, _msg) do { \
	if (!(_cond)) { \
	    bu_log("FAIL [%s:%d] %s\n", __FILE__, __LINE__, (_msg)); \
	    g_failures++; \
	} \
    } while (0)

static ged_db_index_id
name_hash(const char *name)
{
    return bu_data_hash(name, strlen(name) * sizeof(char));
}

static std::string
scale_parent_name(size_t i)
{
    char name[64] = {0};
    std::snprintf(name, sizeof(name), "_ged_index_scale_parent_%05zu.c", i);
    return std::string(name);
}

static std::string
state_services_tmp_name(size_t fanout)
{
    if (fanout == default_index_scale_fanout)
	return std::string("ged_state_services_tmp.g");

    char name[128] = {0};
    std::snprintf(name, sizeof(name), "ged_state_services_%zu_tmp.g", fanout);
    return std::string(name);
}

static std::string
state_services_cache_name(size_t fanout)
{
    if (fanout == default_index_scale_fanout)
	return std::string("ged_state_services_test_cache");

    char name[128] = {0};
    std::snprintf(name, sizeof(name), "ged_state_services_test_cache_%zu", fanout);
    return std::string(name);
}

static struct ged *
open_service_ged(const char *path)
{
    struct ged *gedp = ged_open("db", path, 1);
    CHECK(gedp != NULL, "ged_open must open service test database");
    if (!gedp)
	return NULL;

    CHECK(ged_event_txn_available(gedp) == 1,
	    "GedEventTxn service must be available");
    CHECK(ged_db_index_available(gedp) == 1,
	    "GedDbIndex service must be available");
    CHECK(ged_selection_state_available(gedp) == 1,
	    "GedSelectionState service must be available");

    return gedp;
}

static std::vector<std::string>
affected_path_strings(struct ged *gedp, ged_db_index_id object_id,
		      size_t max_depth)
{
    std::vector<std::string> paths;
    size_t path_count =
	ged_db_index_affected_path_count(gedp, object_id, max_depth);
    for (size_t row = 0; row < path_count; row++) {
	size_t path_len = ged_db_index_affected_path_at(gedp, object_id, row,
		NULL, 0, max_depth);
	CHECK(path_len > 0,
		"affected_path_at must report affected path depth");
	if (!path_len)
	    continue;

	std::vector<ged_db_index_id> ids(path_len);
	CHECK(ged_db_index_affected_path_at(gedp, object_id, row,
		    ids.data(), ids.size(), max_depth) == path_len,
		"affected_path_at must fill affected path ids");

	struct bu_vls printed = BU_VLS_INIT_ZERO;
	CHECK(ged_db_index_path_print(gedp, &printed, ids.data(), ids.size(),
		    0, 0) == 1,
		"affected path ids must be printable");
	paths.push_back(std::string(bu_vls_cstr(&printed)));
	bu_vls_free(&printed);
    }
    std::sort(paths.begin(), paths.end());
    return paths;
}

static int
test_db_index(struct ged *gedp, size_t idx_scale_fanout)
{
    bu_log("=== GedDbIndex service queries ===\n");

    CHECK(ged_db_index_refresh(gedp) > 0,
	    "ged_db_index_refresh must initialize service state");

    size_t top_count = ged_db_index_tops(gedp, 0, NULL, 0);
    CHECK(top_count > 0, "ged_db_index_tops must report top entries");
    std::vector<ged_db_index_id> tops(top_count);
    if (top_count)
	CHECK(ged_db_index_tops(gedp, 0, tops.data(), tops.size()) == top_count,
		"ged_db_index_tops must fill all requested top ids");

    ged_db_index_id allg_id = name_hash("all.g");
    CHECK(std::find(tops.begin(), tops.end(), allg_id) != tops.end(),
	    "ged_db_index_tops must include all.g");

    struct ged_db_index_record allg_rec = {};
    CHECK(ged_db_index_record_get(gedp, allg_id, &allg_rec) == 1,
	    "ged_db_index_record_get must resolve all.g");
    CHECK(allg_rec.valid == 1, "all.g record must be valid");
    CHECK(allg_rec.dp != NULL, "all.g record must expose a directory pointer");
    CHECK(allg_rec.is_comb == 1, "all.g record must identify a comb");
    CHECK(allg_rec.child_count > 0, "all.g record must expose child count");
    CHECK(allg_rec.name && BU_STR_EQUAL(allg_rec.name, "all.g"),
	    "all.g record must expose its name");
    CHECK(ged_db_index_child_count(gedp, allg_id) == allg_rec.child_count,
	    "ged_db_index_child_count must match record child_count");

    ged_db_index_id platform_id = name_hash("platform.r");
    int found_platform = 0;
    for (size_t row = 0; row < allg_rec.child_count; row++) {
	struct ged_db_index_child child = {};
	CHECK(ged_db_index_child_at(gedp, allg_id, row, &child) == 1,
		"ged_db_index_child_at must resolve all.g child rows");
	if (child.record.id == platform_id) {
	    found_platform = 1;
	    CHECK(child.bool_op == DB_OP_UNION,
		    "platform.r child row must report union bool op");
	    CHECK(child.record.dp != NULL,
		    "platform.r child row must expose a directory pointer");
	}
    }
    CHECK(found_platform, "all.g children must include platform.r");

    ged_db_index_id path_ids[2] = {allg_id, platform_id};
    CHECK(ged_db_index_path_resolve(gedp, "all.g/platform.r", NULL, 0) == 2,
	    "path_resolve must report all.g/platform.r depth");
    ged_db_index_id resolved_ids[2] = {0, 0};
    CHECK(ged_db_index_path_resolve(gedp, "all.g/platform.r",
		resolved_ids, 2) == 2,
	    "path_resolve must resolve all.g/platform.r");
    CHECK(resolved_ids[0] == allg_id && resolved_ids[1] == platform_id,
	    "path_resolve ids must match expected objects");
    CHECK(ged_db_index_path_hash(gedp, path_ids, 2, 0) != 0,
	    "path_hash must produce non-zero hash for valid paths");
    CHECK(ged_db_index_valid_id(gedp, 0) == 0,
	    "valid_id must reject id zero");

    struct bu_vls printed = BU_VLS_INIT_ZERO;
    CHECK(ged_db_index_path_print(gedp, &printed, path_ids, 2, 0, 0) == 1,
	    "path_print must print all.g/platform.r");
    CHECK(BU_STR_EQUAL(bu_vls_cstr(&printed), "all.g/platform.r"),
	    "path_print output must match expected string");
    bu_vls_free(&printed);

    struct directory *all_dp = db_lookup(gedp->dbip, "all.g", LOOKUP_QUIET);
    CHECK(all_dp != RT_DIR_NULL, "db_lookup must find all.g");
    if (all_dp) {
	CHECK(ged_db_index_note_object_change(gedp, all_dp,
		    GED_DB_INDEX_OBJECT_CHANGED) == 1,
		"note_object_change must accept known objects");
	CHECK((ged_db_index_refresh_flags(gedp) &
		    GED_DB_INDEX_REFRESH_DB_CHANGE) != 0,
		"refresh_flags must report queued DB changes");
	CHECK(ged_db_index_refresh_flags(gedp) == 0,
		"refresh_flags must clear consumed changes");
	CHECK(ged_db_index_note_object_change(gedp, all_dp, 999) == 0,
		"note_object_change must reject unknown change kinds");
    }

    const char *idx_parent = "_ged_index_parent.c";
    const char *idx_child = "_ged_index_child.s";
    const char *idx_missing = "_ged_index_missing.s";
    struct rt_wdb *wdbp = wdb_dbopen(gedp->dbip, RT_WDB_TYPE_DB_DEFAULT);
    CHECK(wdbp != RT_WDB_NULL, "wdb_dbopen must create writer for index fixture");
    if (wdbp) {
	struct wmember wm;
	point_t center = {210.0, 0.0, 0.0};
	BU_LIST_INIT(&wm.l);
	CHECK(mk_sph(wdbp, idx_child, center, 1.0) == 0,
		"index fixture child sphere must be created");
	CHECK(mk_addmember(idx_child, &wm.l, NULL, WMOP_UNION) != NULL,
		"index fixture must add first child instance");
	CHECK(mk_addmember(idx_child, &wm.l, NULL, WMOP_UNION) != NULL,
		"index fixture must add duplicate child instance");
	CHECK(mk_addmember(idx_missing, &wm.l, NULL, WMOP_UNION) != NULL,
		"index fixture must add invalid reference child");
	CHECK(mk_comb(wdbp, idx_parent, &wm.l, 0, NULL, NULL, NULL,
		    0, 0, 0, 0, 0, 0, 0) == 0,
		"index fixture parent comb must be created");
    }

    struct directory *idx_child_dp = db_lookup(gedp->dbip, idx_child, LOOKUP_QUIET);
    struct directory *idx_parent_dp = db_lookup(gedp->dbip, idx_parent, LOOKUP_QUIET);
    CHECK(idx_child_dp != RT_DIR_NULL,
	    "index fixture child must be present in directory");
    CHECK(idx_parent_dp != RT_DIR_NULL,
	    "index fixture parent must be present in directory");
    if (idx_child_dp)
	CHECK(ged_db_index_note_object_change(gedp, idx_child_dp,
		    GED_DB_INDEX_OBJECT_ADDED) == 1,
		"child addition must be queued");
    if (idx_parent_dp)
	CHECK(ged_db_index_note_object_change(gedp, idx_parent_dp,
		    GED_DB_INDEX_OBJECT_ADDED) == 1,
		"parent addition must be queued");
    CHECK(ged_db_index_refresh(gedp) > 0,
	    "refresh must index synthetic child instances");

    ged_db_index_id idx_parent_id = name_hash(idx_parent);
    ged_db_index_id idx_child_id = name_hash(idx_child);
    ged_db_index_id idx_missing_id = name_hash(idx_missing);
    struct ged_db_index_record idx_parent_rec = {};
    CHECK(ged_db_index_record_get(gedp, idx_parent_id, &idx_parent_rec) == 1,
	    "record_get must resolve synthetic parent");
    CHECK(idx_parent_rec.child_count == 3,
	    "synthetic parent must report three direct children");

    int saw_canonical_child = 0;
    int saw_duplicate_child = 0;
    int saw_invalid_child = 0;
    ged_db_index_id duplicate_child_id = 0;
    for (size_t row = 0; row < idx_parent_rec.child_count; row++) {
	struct ged_db_index_child child = {};
	CHECK(ged_db_index_child_at(gedp, idx_parent_id, row, &child) == 1,
		"child_at must resolve synthetic child row");
	if (child.record.id == idx_child_id) {
	    saw_canonical_child = 1;
	    CHECK(child.record.is_instance == 0,
		    "first duplicate object use must retain canonical child id");
	    CHECK(child.record.object_id == idx_child_id,
		    "canonical child object_id must equal child id");
	} else if (child.record.is_instance &&
		child.record.object_id == idx_child_id) {
	    saw_duplicate_child = 1;
	    duplicate_child_id = child.record.id;
	    CHECK(child.record.id != idx_child_id,
		    "duplicate child instance id must differ from object id");
	    CHECK(child.record.name &&
		    BU_STR_EQUAL(child.record.name, "_ged_index_child.s@1"),
		    "duplicate child instance must expose printable discriminator");
	} else if (child.record.id == idx_missing_id) {
	    saw_invalid_child = 1;
	    CHECK(child.record.valid == 1,
		    "invalid reference must still be an index record");
	    CHECK(child.record.dp == NULL,
		    "invalid reference must not expose a directory pointer");
	    CHECK(child.record.name && BU_STR_EQUAL(child.record.name, idx_missing),
		    "invalid reference must preserve source child name");
	}
    }
    CHECK(saw_canonical_child, "synthetic hierarchy must expose canonical child");
    CHECK(saw_duplicate_child, "synthetic hierarchy must expose duplicate child");
    CHECK(saw_invalid_child, "synthetic hierarchy must expose invalid reference");

    CHECK(ged_db_index_object_use_count(gedp, idx_parent_id) == 0,
	    "synthetic parent must not report direct parents");
    CHECK(ged_db_index_object_use_count(gedp, idx_child_id) == 2,
	    "synthetic child reverse index must report both direct uses");
    if (duplicate_child_id)
	CHECK(ged_db_index_object_use_count(gedp, duplicate_child_id) == 2,
		"duplicate instance id reverse query must canonicalize to object uses");

    int saw_canonical_use = 0;
    int saw_duplicate_use = 0;
    size_t child_use_count = ged_db_index_object_use_count(gedp, idx_child_id);
    for (size_t row = 0; row < child_use_count; row++) {
	struct ged_db_index_use use = {};
	CHECK(ged_db_index_object_use_at(gedp, idx_child_id, row, &use) == 1,
		"object_use_at must resolve synthetic child direct uses");
	CHECK(use.parent.id == idx_parent_id,
		"synthetic child direct use parent must be synthetic parent");
	CHECK(use.parent.child_count == idx_parent_rec.child_count,
		"direct-use parent record must expose child count");
	CHECK(use.bool_op == DB_OP_UNION,
		"synthetic child direct use must report union bool op");

	struct ged_db_index_child forward_child = {};
	CHECK(ged_db_index_child_at(gedp, use.parent.id, use.child_row,
		    &forward_child) == 1,
		"direct-use row must resolve through forward child API");
	CHECK(forward_child.record.id == use.child.id,
		"direct-use child record must match forward child row");
	if (use.child.id == idx_child_id && use.child.is_instance == 0)
	    saw_canonical_use = 1;
	if (duplicate_child_id && use.child.id == duplicate_child_id &&
		use.child.is_instance == 1)
	    saw_duplicate_use = 1;
    }
    CHECK(saw_canonical_use,
	    "reverse index must expose canonical direct child use");
    CHECK(saw_duplicate_use,
	    "reverse index must expose duplicate direct child use");

    CHECK(ged_db_index_object_use_count(gedp, idx_missing_id) == 1,
	    "invalid reference reverse index must report direct parent use");
    struct ged_db_index_use missing_use = {};
    CHECK(ged_db_index_object_use_at(gedp, idx_missing_id, 0,
		&missing_use) == 1,
	    "object_use_at must resolve invalid reference direct use");
    CHECK(missing_use.parent.id == idx_parent_id,
	    "invalid reference use parent must be synthetic parent");
    CHECK(missing_use.child.id == idx_missing_id &&
	    missing_use.child.dp == NULL,
	    "invalid reference use child must preserve invalid child record");
    struct ged_db_index_use out_of_range_use = {};
    CHECK(ged_db_index_object_use_at(gedp, idx_child_id, child_use_count,
		&out_of_range_use) == 0,
	    "object_use_at must reject out-of-range rows");
    CHECK(ged_db_index_object_use_at(gedp, idx_child_id, 0, NULL) == 0,
	    "object_use_at must reject null output");

    if (duplicate_child_id) {
	ged_db_index_id dup_path[2] = {idx_parent_id, duplicate_child_id};
	struct bu_vls dup_print = BU_VLS_INIT_ZERO;
	CHECK(ged_db_index_path_print(gedp, &dup_print, dup_path, 2, 0, 0) == 1,
		"path_print must print duplicate instance path");
	CHECK(BU_STR_EQUAL(bu_vls_cstr(&dup_print),
		    "_ged_index_parent.c/_ged_index_child.s@1"),
		"duplicate path print must include instance discriminator");
	bu_vls_free(&dup_print);
	CHECK(ged_db_index_path_resolve(gedp,
		    "_ged_index_parent.c/_ged_index_child.s@1",
		    NULL, 0) == 2,
		"path_resolve must resolve duplicate instance path");
    }

    ged_db_index_id invalid_path[2] = {idx_parent_id, idx_missing_id};
    struct bu_vls invalid_print = BU_VLS_INIT_ZERO;
    CHECK(ged_db_index_path_print(gedp, &invalid_print,
		invalid_path, 2, 0, 0) == 1,
	    "path_print must print invalid reference path");
    CHECK(BU_STR_EQUAL(bu_vls_cstr(&invalid_print),
		"_ged_index_parent.c/_ged_index_missing.s"),
	    "invalid path print must preserve missing child name");
    bu_vls_free(&invalid_print);
    CHECK(ged_db_index_path_resolve(gedp,
		"_ged_index_parent.c/_ged_index_missing.s", NULL, 0) == 2,
	    "path_resolve must resolve known invalid reference path");

    const char *idx_root_a = "_ged_index_root_a.c";
    const char *idx_root_b = "_ged_index_root_b.c";
    if (wdbp) {
	struct wmember root_a;
	struct wmember root_b;
	BU_LIST_INIT(&root_a.l);
	BU_LIST_INIT(&root_b.l);
	CHECK(mk_addmember(idx_parent, &root_a.l, NULL, WMOP_UNION) != NULL,
		"affected-path fixture must add parent under root A");
	CHECK(mk_comb(wdbp, idx_root_a, &root_a.l, 0, NULL, NULL, NULL,
		    0, 0, 0, 0, 0, 0, 0) == 0,
		"affected-path fixture root A must be created");
	CHECK(mk_addmember(idx_parent, &root_b.l, NULL, WMOP_UNION) != NULL,
		"affected-path fixture must add parent under root B");
	CHECK(mk_comb(wdbp, idx_root_b, &root_b.l, 0, NULL, NULL, NULL,
		    0, 0, 0, 0, 0, 0, 0) == 0,
		"affected-path fixture root B must be created");
    }
    CHECK(ged_db_index_note_object_name_change(gedp, idx_root_a,
		GED_DB_INDEX_OBJECT_ADDED) == 1,
	    "affected-path root A addition must be queued");
    CHECK(ged_db_index_note_object_name_change(gedp, idx_root_b,
		GED_DB_INDEX_OBJECT_ADDED) == 1,
	    "affected-path root B addition must be queued");
    CHECK(ged_db_index_refresh(gedp) > 0,
	    "refresh must index affected-path reused-comb fixture");

    std::vector<std::string> child_affected =
	affected_path_strings(gedp, idx_child_id, 0);
    std::vector<std::string> expected_child_affected = {
	"_ged_index_root_a.c/_ged_index_parent.c/_ged_index_child.s",
	"_ged_index_root_a.c/_ged_index_parent.c/_ged_index_child.s@1",
	"_ged_index_root_b.c/_ged_index_parent.c/_ged_index_child.s",
	"_ged_index_root_b.c/_ged_index_parent.c/_ged_index_child.s@1"
    };
    CHECK(child_affected == expected_child_affected,
	    "affected primitive paths must follow all reused parent paths");
    CHECK(ged_db_index_affected_path_count(gedp, duplicate_child_id, 0) ==
	    child_affected.size(),
	    "affected paths queried by duplicate instance id must canonicalize");

    std::vector<std::string> parent_affected =
	affected_path_strings(gedp, idx_parent_id, 0);
    std::vector<std::string> expected_parent_affected = {
	"_ged_index_root_a.c/_ged_index_parent.c",
	"_ged_index_root_b.c/_ged_index_parent.c"
    };
    CHECK(parent_affected == expected_parent_affected,
	    "affected comb paths must include reused parent instances");

    std::vector<std::string> depth_limited =
	affected_path_strings(gedp, idx_child_id, 2);
    CHECK(depth_limited.empty(),
	    "affected path max_depth must suppress deeper paths");
    std::vector<std::string> depth_three =
	affected_path_strings(gedp, idx_child_id, 3);
    CHECK(depth_three == expected_child_affected,
	    "affected path max_depth must include paths within depth");

    const char *idx_scale_child = "_ged_index_scale_child.s";
    if (wdbp) {
	point_t scale_center = {220.0, 0.0, 0.0};
	CHECK(mk_sph(wdbp, idx_scale_child, scale_center, 1.0) == 0,
		"scale index fixture child sphere must be created");
	for (size_t i = 0; i < idx_scale_fanout; i++) {
	    struct wmember wm;
	    std::string parent_name = scale_parent_name(i);
	    BU_LIST_INIT(&wm.l);
	    CHECK(mk_addmember(idx_scale_child, &wm.l, NULL,
			WMOP_UNION) != NULL,
		    "scale index fixture must add shared child");
	    CHECK(mk_comb(wdbp, parent_name.c_str(), &wm.l, 0,
			NULL, NULL, NULL, 0, 0, 0, 0, 0, 0, 0) == 0,
		    "scale index fixture parent comb must be created");
	}
    }

    auto scale_refresh_start = std::chrono::steady_clock::now();
    CHECK(ged_db_index_refresh(gedp) > 0,
	    "scale refresh must index large affected-path fan-out fixture");
    auto scale_refresh_end = std::chrono::steady_clock::now();
    ged_db_index_id idx_scale_child_id = name_hash(idx_scale_child);

    auto scale_query_start = std::chrono::steady_clock::now();
    size_t scale_use_count =
	ged_db_index_object_use_count(gedp, idx_scale_child_id);
    size_t scale_path_count =
	ged_db_index_affected_path_count(gedp, idx_scale_child_id, 0);
    size_t scale_limited_count =
	ged_db_index_affected_path_count(gedp, idx_scale_child_id, 1);
    CHECK(scale_use_count == idx_scale_fanout,
	    "scale reverse-use query must return every direct parent use");
    CHECK(scale_path_count == idx_scale_fanout,
	    "scale affected-path query must return every parent path");
    CHECK(scale_limited_count == 0,
	    "scale affected-path max_depth must suppress longer paths");

    size_t scale_sample_step = idx_scale_fanout / 16;
    if (!scale_sample_step)
	scale_sample_step = 1;
    for (size_t row = 0; row < scale_path_count; row += scale_sample_step) {
	ged_db_index_id ids[2] = {0, 0};
	struct ged_db_index_record parent_rec = {};
	CHECK(ged_db_index_affected_path_at(gedp, idx_scale_child_id, row,
		    ids, 2, 0) == 2,
		"scale affected_path_at must fill sampled parent/child ids");
	CHECK(ids[1] == idx_scale_child_id,
		"scale sampled affected path must end in shared child id");
	CHECK(ged_db_index_record_get(gedp, ids[0], &parent_rec) == 1 &&
		parent_rec.name &&
		std::string(parent_rec.name).find("_ged_index_scale_parent_") == 0,
		"scale sampled affected path must begin with generated parent");
	CHECK(ged_db_index_path_hash(gedp, ids, 2, 0) != 0,
		"scale sampled affected path must have a stable nonzero hash");
    }
    auto scale_query_end = std::chrono::steady_clock::now();

    long long scale_refresh_ms =
	std::chrono::duration_cast<std::chrono::milliseconds>(
		scale_refresh_end - scale_refresh_start).count();
    long long scale_query_ms =
	std::chrono::duration_cast<std::chrono::milliseconds>(
		scale_query_end - scale_query_start).count();
    long long scale_query_limit_ms =
	(idx_scale_fanout >= slow_index_scale_fanout) ? 30000 : 5000;
    bu_log("GedDbIndex scale: fanout=%zu refresh=%lld ms query=%lld ms limit=%lld ms\n",
	    idx_scale_fanout, scale_refresh_ms, scale_query_ms,
	    scale_query_limit_ms);
    CHECK(scale_query_ms < scale_query_limit_ms,
	    "scale affected-path/reverse-use query should remain bounded");

    const char *idx_late_child = "_ged_index_late_child.s";
    if (wdbp) {
	point_t late_center = {215.0, 0.0, 0.0};
	CHECK(mk_sph(wdbp, idx_late_child, late_center, 1.0) == 0,
		"late index fixture child sphere must be created");
    }
    CHECK(ged_db_index_note_object_name_change(gedp, idx_late_child,
		GED_DB_INDEX_OBJECT_ADDED) == 1,
	    "object-added notification by name must queue late child");
    CHECK((ged_db_index_refresh_flags(gedp) &
		GED_DB_INDEX_REFRESH_DB_CHANGE) != 0,
	    "object-added notification must report DB-index refresh flags");
    ged_db_index_id idx_late_child_id = name_hash(idx_late_child);
    struct ged_db_index_record late_child_rec = {};
    CHECK(ged_db_index_record_get(gedp, idx_late_child_id,
		&late_child_rec) == 1,
	    "incremental object add must expose late child record");
    CHECK(late_child_rec.dp != NULL && late_child_rec.name &&
	    BU_STR_EQUAL(late_child_rec.name, idx_late_child),
	    "incremental object add record must have live directory pointer");
    CHECK(ged_db_index_object_use_count(gedp, idx_late_child_id) == 0,
	    "incremental object add must not invent parent uses");
    CHECK(ged_db_index_refresh(gedp) > 0,
	    "full refresh after object add must succeed");
    CHECK(ged_db_index_record_get(gedp, idx_late_child_id,
		&late_child_rec) == 1 && late_child_rec.dp != NULL,
	    "full refresh must preserve incremental object add record");

    CHECK(ged_db_index_note_object_name_change(gedp, idx_parent,
		GED_DB_INDEX_OBJECT_CHANGED) == 1,
	    "object-changed notification by name must queue synthetic parent");
    CHECK((ged_db_index_refresh_flags(gedp) &
		GED_DB_INDEX_REFRESH_DB_CHANGE) != 0,
	    "object-changed notification must report DB-index refresh flags");
    size_t changed_parent_child_count =
	ged_db_index_child_count(gedp, idx_parent_id);
    CHECK(changed_parent_child_count == idx_parent_rec.child_count,
	    "incremental no-topology object change must preserve child count");
    CHECK(ged_db_index_refresh(gedp) > 0,
	    "full refresh after no-topology object change must succeed");
    CHECK(ged_db_index_child_count(gedp, idx_parent_id) ==
	    changed_parent_child_count,
	    "full refresh must preserve no-topology object change child count");

    const char *rm_missing_av[2] = {"rm",
	"_ged_index_parent.c/_ged_index_missing.s"};
    CHECK(ged_exec_rm(gedp, 2, rm_missing_av) == BRLCAD_OK,
	    "rm must remove invalid child instance from synthetic parent");
    CHECK((ged_db_index_refresh_flags(gedp) &
		GED_DB_INDEX_REFRESH_DB_CHANGE) != 0,
	    "comb-child removal must queue DB-index refresh flags");
    size_t removed_child_count = ged_db_index_child_count(gedp, idx_parent_id);
    size_t removed_missing_use_count =
	ged_db_index_object_use_count(gedp, idx_missing_id);
    CHECK(removed_child_count == 2,
	    "incremental comb-child removal must update child count");
    CHECK(removed_missing_use_count == 0,
	    "incremental comb-child removal must clear invalid reference uses");
    CHECK(ged_db_index_path_resolve(gedp,
		"_ged_index_parent.c/_ged_index_missing.s", NULL, 0) == 0,
	    "incremental comb-child removal must clear removed child path");
    CHECK(ged_db_index_refresh(gedp) > 0,
	    "full refresh after comb-child removal must succeed");
    CHECK(ged_db_index_child_count(gedp, idx_parent_id) == removed_child_count,
	    "full refresh must preserve incremental removal child count");
    CHECK(ged_db_index_object_use_count(gedp, idx_missing_id) ==
	    removed_missing_use_count,
	    "full refresh must preserve incremental removal reverse-use count");

    const char *idx_child_renamed = "_ged_index_child_renamed.s";
    const char *mv_child_av[3] = {"mv", idx_child, idx_child_renamed};
    CHECK(ged_exec_mv(gedp, 3, mv_child_av) == BRLCAD_OK,
	    "mv must rename synthetic child object");
    CHECK((ged_db_index_refresh_flags(gedp) &
		GED_DB_INDEX_REFRESH_DB_CHANGE) != 0,
	    "object rename must queue DB-index refresh flags");
    ged_db_index_id idx_child_renamed_id = name_hash(idx_child_renamed);
    struct ged_db_index_record renamed_rec = {};
    struct ged_db_index_record old_child_rec = {};
    CHECK(ged_db_index_record_get(gedp, idx_child_renamed_id,
		&renamed_rec) == 1,
	    "incremental rename must expose new object record");
    CHECK(renamed_rec.dp != NULL && renamed_rec.name &&
	    BU_STR_EQUAL(renamed_rec.name, idx_child_renamed),
	    "incremental rename new record must have live directory pointer");
    CHECK(ged_db_index_record_get(gedp, idx_child_id, &old_child_rec) == 1,
	    "incremental rename must preserve old referenced child record");
    CHECK(old_child_rec.dp == NULL && old_child_rec.name &&
	    BU_STR_EQUAL(old_child_rec.name, idx_child),
	    "incremental rename old child record must become invalid reference");
    size_t renamed_new_use_count =
	ged_db_index_object_use_count(gedp, idx_child_renamed_id);
    size_t renamed_old_use_count =
	ged_db_index_object_use_count(gedp, idx_child_id);
    CHECK(renamed_new_use_count == 0,
	    "incremental rename must not invent parent uses for new name");
    CHECK(renamed_old_use_count == 2,
	    "incremental rename must keep old-name parent uses as invalid refs");
    CHECK(ged_db_index_path_resolve(gedp,
		"_ged_index_parent.c/_ged_index_child_renamed.s",
		NULL, 0) == 0,
	    "incremental rename must not rewrite comb child paths");
    CHECK(ged_db_index_path_resolve(gedp,
		"_ged_index_parent.c/_ged_index_child.s", NULL, 0) == 2,
	    "incremental rename must keep old-name child path resolvable");
    CHECK(ged_db_index_refresh(gedp) > 0,
	    "full refresh after rename must succeed");
    CHECK(ged_db_index_object_use_count(gedp, idx_child_renamed_id) ==
	    renamed_new_use_count,
	    "full refresh must preserve incremental rename new-use count");
    CHECK(ged_db_index_object_use_count(gedp, idx_child_id) ==
	    renamed_old_use_count,
	    "full refresh must preserve incremental rename old-use count");

    const char *kill_child_av[2] = {"kill", idx_child_renamed};
    CHECK(ged_exec_kill(gedp, 2, kill_child_av) == BRLCAD_OK,
	    "kill must remove renamed synthetic child object");
    CHECK((ged_db_index_refresh_flags(gedp) &
		GED_DB_INDEX_REFRESH_DB_CHANGE) != 0,
	    "object removal must queue DB-index refresh flags");
    struct ged_db_index_record killed_rec = {};
    CHECK(ged_db_index_record_get(gedp, idx_child_renamed_id,
		&killed_rec) == 1,
	    "incremental removal must preserve removed object name record");
    CHECK(killed_rec.dp == NULL && killed_rec.name &&
	    BU_STR_EQUAL(killed_rec.name, idx_child_renamed),
	    "incremental removal record must have no live directory pointer");
    size_t killed_use_count =
	ged_db_index_object_use_count(gedp, idx_child_renamed_id);
    CHECK(killed_use_count == 0,
	    "incremental removal must keep removed unreferenced object unused");
    CHECK(ged_db_index_refresh(gedp) > 0,
	    "full refresh after removal must succeed");
    CHECK(ged_db_index_object_use_count(gedp, idx_child_renamed_id) ==
	    killed_use_count,
	    "full refresh must preserve incremental removal use count");

    const char *cycle_a = "_ged_index_cycle_a.c";
    const char *cycle_b = "_ged_index_cycle_b.c";
    if (wdbp) {
	struct wmember wm_a;
	struct wmember wm_b;
	BU_LIST_INIT(&wm_a.l);
	BU_LIST_INIT(&wm_b.l);
	CHECK(mk_addmember(cycle_b, &wm_a.l, NULL, WMOP_UNION) != NULL,
		"cycle fixture must add B under A");
	CHECK(mk_comb(wdbp, cycle_a, &wm_a.l, 0, NULL, NULL, NULL,
		    0, 0, 0, 0, 0, 0, 0) == 0,
		"cycle fixture comb A must be created");
	CHECK(mk_addmember(cycle_a, &wm_b.l, NULL, WMOP_UNION) != NULL,
		"cycle fixture must add A under B");
	CHECK(mk_comb(wdbp, cycle_b, &wm_b.l, 0, NULL, NULL, NULL,
		    0, 0, 0, 0, 0, 0, 0) == 0,
		"cycle fixture comb B must be created");
    }

    struct directory *cycle_a_dp = db_lookup(gedp->dbip, cycle_a, LOOKUP_QUIET);
    struct directory *cycle_b_dp = db_lookup(gedp->dbip, cycle_b, LOOKUP_QUIET);
    CHECK(cycle_a_dp != RT_DIR_NULL, "cycle fixture A must exist");
    CHECK(cycle_b_dp != RT_DIR_NULL, "cycle fixture B must exist");
    if (cycle_a_dp)
	CHECK(ged_db_index_note_object_change(gedp, cycle_a_dp,
		    GED_DB_INDEX_OBJECT_ADDED) == 1,
		"cycle fixture A addition must be queued");
    if (cycle_b_dp)
	CHECK(ged_db_index_note_object_change(gedp, cycle_b_dp,
		    GED_DB_INDEX_OBJECT_ADDED) == 1,
		"cycle fixture B addition must be queued");
    CHECK(ged_db_index_refresh(gedp) > 0,
	    "refresh must index cyclic comb fixture");

    ged_db_index_id cycle_a_id = name_hash(cycle_a);
    ged_db_index_id cycle_b_id = name_hash(cycle_b);
    size_t tops_without_cycle_count = ged_db_index_tops(gedp, 0, NULL, 0);
    std::vector<ged_db_index_id> tops_without_cycle(tops_without_cycle_count);
    if (tops_without_cycle_count)
	(void)ged_db_index_tops(gedp, 0, tops_without_cycle.data(),
		tops_without_cycle.size());
    CHECK(std::find(tops_without_cycle.begin(), tops_without_cycle.end(),
		cycle_a_id) == tops_without_cycle.end() &&
	    std::find(tops_without_cycle.begin(), tops_without_cycle.end(),
		cycle_b_id) == tops_without_cycle.end(),
	    "cyclic fixture must not appear in non-cyclic tops");

    size_t tops_with_cycle_count = ged_db_index_tops(gedp, 1, NULL, 0);
    std::vector<ged_db_index_id> tops_with_cycle(tops_with_cycle_count);
    if (tops_with_cycle_count)
	(void)ged_db_index_tops(gedp, 1, tops_with_cycle.data(),
		tops_with_cycle.size());
    CHECK(std::find(tops_with_cycle.begin(), tops_with_cycle.end(),
		cycle_a_id) != tops_with_cycle.end(),
	    "include_cyclic tops must expose cyclic comb A");
    CHECK(std::find(tops_with_cycle.begin(), tops_with_cycle.end(),
		cycle_b_id) != tops_with_cycle.end(),
	    "include_cyclic tops must expose cyclic comb B");

    struct ged_db_index_child cycle_child = {};
    CHECK(ged_db_index_child_at(gedp, cycle_a_id, 0, &cycle_child) == 1 &&
	    cycle_child.record.object_id == cycle_b_id,
	    "cyclic comb A child row must expose comb B");
    CHECK(ged_db_index_child_at(gedp, cycle_b_id, 0, &cycle_child) == 1 &&
	    cycle_child.record.object_id == cycle_a_id,
	    "cyclic comb B child row must expose comb A");

    ged_db_index_id cycle_path[3] = {cycle_a_id, cycle_b_id, cycle_a_id};
    struct bu_vls cycle_print = BU_VLS_INIT_ZERO;
    CHECK(ged_db_index_path_print(gedp, &cycle_print,
		cycle_path, 3, 0, 0) == 1,
	    "path_print must print explicit cyclic path vectors");
    CHECK(BU_STR_EQUAL(bu_vls_cstr(&cycle_print),
		"_ged_index_cycle_a.c/_ged_index_cycle_b.c/_ged_index_cycle_a.c"),
	    "cyclic path print must preserve explicit path components");
    bu_vls_free(&cycle_print);
    CHECK(ged_db_index_path_resolve(gedp,
		"_ged_index_cycle_a.c/_ged_index_cycle_b.c/_ged_index_cycle_a.c",
		NULL, 0) == 0,
	    "path_resolve must reject cyclic string paths instead of recursing");

    return 0;
}

static int
test_selection(struct ged *gedp)
{
    bu_log("=== GedSelectionState service queries ===\n");

    CHECK(ged_selection_clear(gedp, NULL) >= 0,
	    "selection clear must accept default set");

    ged_db_index_id path_ids[2] = {
	name_hash("all.g"),
	name_hash("platform.r")
    };
    ged_db_index_id path_hash = ged_db_index_path_hash(gedp, path_ids, 2, 0);
    ged_db_index_id parent_path_hash = ged_db_index_path_hash(gedp, path_ids, 2, 1);

    CHECK(ged_selection_select_path_ids(gedp, NULL, path_ids, 2, 1) == 1,
	    "select_path_ids must select all.g/platform.r");
    CHECK(ged_selection_count(gedp, NULL) == 1,
	    "selection_count must report one selected path");
    CHECK(ged_selection_is_path_selected(gedp, NULL, path_hash) == 1,
	    "is_path_selected must find all.g/platform.r");
    CHECK(ged_selection_is_path_active_parent(gedp, NULL, parent_path_hash) == 1,
	    "is_path_active_parent must identify all.g");
    CHECK(ged_selection_is_object_immediate_parent(gedp, NULL, path_ids[0]) == 1,
	    "is_object_immediate_parent must identify all.g");
    CHECK(ged_selection_is_object_parent(gedp, NULL, path_ids[0]) == 1,
	    "is_object_parent must identify all.g");

    CHECK(ged_selection_deselect_path_ids(gedp, NULL, path_ids, 2, 1) == 1,
	    "deselect_path_ids must deselect all.g/platform.r");
    CHECK(ged_selection_count(gedp, NULL) == 0,
	    "selection_count must report zero after deselect");
    CHECK(ged_selection_is_path_selected(gedp, NULL, path_hash) == 0,
	    "is_path_selected must clear deselected path");

    CHECK(ged_selection_select_path(gedp, NULL, "all.g/platform.r", 1) == 1,
	    "select_path must select string paths");
    struct bu_vls selected_paths = BU_VLS_INIT_ZERO;
    CHECK(ged_selection_list_paths(gedp, NULL, &selected_paths) == 1,
	    "list_paths must list the default set");
    CHECK(std::string(bu_vls_cstr(&selected_paths)).find("all.g/platform.r") !=
	    std::string::npos,
	    "list_paths must print selected default-set path");
    bu_vls_free(&selected_paths);
    CHECK(ged_selection_state_hash(gedp, NULL) != 0,
	    "selection_state_hash must report non-empty state");
    CHECK(ged_selection_deselect_path(gedp, NULL, "/all.g/platform.r", 1) == 1,
	    "deselect_path must accept slash-prefixed paths");

    ged_db_index_id cone_child_ids[3] = {0, 0, 0};
    CHECK(ged_db_index_path_resolve(gedp, "all.g/cone.r/cone.s",
	    cone_child_ids, 3) == 3,
	    "path_resolve must find all.g/cone.r/cone.s");
    ged_db_index_id cone_child_hash =
	ged_db_index_path_hash(gedp, cone_child_ids, 3, 0);
    CHECK(ged_selection_select_path(gedp, NULL, "all.g/cone.r", 1) == 1,
	    "select_path must select parent paths");
    CHECK(ged_selection_is_path_selected(gedp, NULL, cone_child_hash) == 0,
	    "is_path_selected must not report expanded child as explicitly selected");
    CHECK(ged_selection_is_path_active(gedp, NULL, cone_child_hash) == 1,
	    "is_path_active must find child paths expanded from a selected parent");
    CHECK(ged_selection_recompute(gedp, NULL) == 1,
	    "selection recompute must invalidate derived selection query state");
    CHECK(ged_selection_is_path_active(gedp, NULL, cone_child_hash) == 1,
	    "is_path_active must rebuild after recompute invalidation");
    CHECK(ged_selection_clear(gedp, NULL) >= 0,
	    "selection clear must remove parent-path selection");

    CHECK(ged_selection_select_path_matching(gedp, "qa_sel",
		"all.g/cone.r/cone.s", 0) == 1,
	    "select_path_matching must create/use named sets");
    CHECK(ged_selection_count(gedp, "qa_sel") == 1,
	    "selection_count must report named-set selection");
    CHECK(ged_selection_set_match_count(gedp, "qa_*") == 1,
	    "set_match_count must match named selection sets");
    CHECK(ged_selection_recompute_matching(gedp, "qa_*") == 1,
	    "recompute_matching must update matched sets");
    struct bu_vls named_paths = BU_VLS_INIT_ZERO;
    CHECK(ged_selection_list_paths(gedp, "qa_*", &named_paths) == 1,
	    "list_paths must list matched named sets");
    CHECK(std::string(bu_vls_cstr(&named_paths)).find("all.g/cone.r/cone.s") !=
	    std::string::npos,
	    "list_paths must print matched named-set path");
    bu_vls_free(&named_paths);
    CHECK(ged_selection_clear_matching(gedp, "qa_*") == 1,
	    "clear_matching must clear matched named sets");
    CHECK(ged_selection_clear(gedp, NULL) >= 0,
	    "selection clear must reset default set");

    return 0;
}

struct event_counts {
    size_t calls = 0;
    size_t event_count = 0;
    size_t coalesced_count = 0;
};

static void
event_count_cb(struct ged *UNUSED(gedp),
	       const struct ged_event *UNUSED(events),
	       size_t event_count,
	       const struct ged_event_txn_result *result,
	       void *client_data)
{
    struct event_counts *counts = (struct event_counts *)client_data;
    counts->calls++;
    counts->event_count += event_count;
    if (result)
	counts->coalesced_count += result->coalesced_event_count;
}

struct event_order_shared {
    std::vector<char> phases;
};

struct event_order_observer {
    event_order_shared *shared = nullptr;
    char phase = '?';
    size_t calls = 0;
    size_t event_count = 0;
    size_t coalesced_count = 0;
    std::vector<ged_event_kind> kinds;
    std::vector<ged_event_kind> all_kinds;
    std::vector<ged_event_kind> all_named_kinds;
    std::vector<std::string> all_named_names;
    std::vector<std::string> all_parent_names;
    std::vector<std::string> all_child_names;
    std::vector<std::string> all_paths;
    std::vector<int> redraws;
    uint64_t current_draw_revision = 0;
    uint64_t result_draw_before = 0;
    uint64_t result_draw_after = 0;
    std::string affected_names;
    std::string all_affected_names;
};

static void
event_order_cb(struct ged *gedp,
	       const struct ged_event *events,
	       size_t event_count,
	       const struct ged_event_txn_result *result,
	       void *client_data)
{
    struct event_order_observer *obs =
	(struct event_order_observer *)client_data;
    if (!obs)
	return;

    obs->calls++;
    obs->event_count = event_count;
    obs->kinds.clear();
    obs->redraws.clear();
    for (size_t i = 0; i < event_count; i++) {
	obs->kinds.push_back(events[i].kind);
	obs->all_kinds.push_back(events[i].kind);
	obs->all_named_kinds.push_back(events[i].kind);
	obs->all_named_names.push_back(events[i].name ? events[i].name : "");
	obs->all_parent_names.push_back(events[i].parent_name ?
		events[i].parent_name : "");
	obs->all_child_names.push_back(events[i].child_name ?
		events[i].child_name : "");
	obs->all_paths.push_back(events[i].path ? events[i].path : "");
	obs->redraws.push_back(events[i].redraw);
    }
    obs->current_draw_revision = ged_draw_scene_revision(gedp);
    if (result) {
	obs->coalesced_count = result->coalesced_event_count;
	obs->result_draw_before = result->draw_scene_revision_before;
	obs->result_draw_after = result->draw_scene_revision_after;
	obs->affected_names = bu_vls_cstr(&result->affected_names);
	if (bu_vls_strlen(&result->affected_names)) {
	    if (!obs->all_affected_names.empty())
		obs->all_affected_names.append(" ");
	    obs->all_affected_names.append(bu_vls_cstr(
		    &result->affected_names));
	}
    }
    if (obs->shared)
	obs->shared->phases.push_back(obs->phase);
}

static bool
observed_scoped_event(const event_order_observer &obs, ged_event_kind kind,
		     const char *name, const char *parent_name,
		     const char *path)
{
    for (size_t i = 0; i < obs.all_named_kinds.size() &&
	    i < obs.all_named_names.size() &&
	    i < obs.all_parent_names.size() &&
	    i < obs.all_paths.size(); i++) {
	if (obs.all_named_kinds[i] == kind &&
		BU_STR_EQUAL(obs.all_named_names[i].c_str(), name) &&
		BU_STR_EQUAL(obs.all_parent_names[i].c_str(), parent_name) &&
		BU_STR_EQUAL(obs.all_paths[i].c_str(), path))
	    return true;
    }
    return false;
}

static bool
observed_named_event(const event_order_observer &obs, ged_event_kind kind,
		     const char *name)
{
    for (size_t i = 0; i < obs.all_named_kinds.size() &&
	    i < obs.all_named_names.size(); i++) {
	if (obs.all_named_kinds[i] == kind &&
		BU_STR_EQUAL(obs.all_named_names[i].c_str(), name))
	    return true;
    }
    return false;
}

static bool
observed_named_structural_fallback(const event_order_observer &obs,
				   const char *name)
{
    return observed_named_event(obs, GED_EVENT_COMB_TREE_CHANGED, name) ||
	observed_named_event(obs, GED_EVENT_OBJECT_MODIFIED, name);
}

static int
make_event_nmg_tet(struct rt_wdb *wdbp, const char *name)
{
    struct bn_tol tol = BN_TOL_INIT_TOL;
    struct model *m = nmg_mm();
    struct nmgregion *r = nmg_mrsv(m);
    struct shell *s = BU_LIST_FIRST(shell, &r->s_hd);
    struct vertex *verts[4] = {NULL, NULL, NULL, NULL};
    point_t pts[4] = {
	{0.0, 0.0, 0.0},
	{1.0, 0.0, 0.0},
	{0.0, 1.0, 0.0},
	{0.0, 0.0, 1.0},
    };
    struct vertex **fv0[3] = {&verts[0], &verts[1], &verts[2]};
    struct vertex **fv1[3] = {&verts[0], &verts[1], &verts[3]};
    struct vertex **fv2[3] = {&verts[0], &verts[2], &verts[3]};
    struct vertex **fv3[3] = {&verts[1], &verts[2], &verts[3]};
    struct faceuse *fu = NULL;

    if (!wdbp || !name || !m || !r || !s) {
	if (m)
	    nmg_km(m);
	return -1;
    }

    fu = nmg_cmface(s, fv0, 3);
    nmg_vertex_gv(verts[0], pts[0]);
    nmg_vertex_gv(verts[1], pts[1]);
    nmg_vertex_gv(verts[2], pts[2]);
    if (!fu || nmg_fu_planeeqn(fu, &tol) != 0) {
	nmg_km(m);
	return -1;
    }

    fu = nmg_cmface(s, fv1, 3);
    nmg_vertex_gv(verts[3], pts[3]);
    if (!fu || nmg_fu_planeeqn(fu, &tol) != 0) {
	nmg_km(m);
	return -1;
    }

    fu = nmg_cmface(s, fv2, 3);
    if (!fu || nmg_fu_planeeqn(fu, &tol) != 0) {
	nmg_km(m);
	return -1;
    }

    fu = nmg_cmface(s, fv3, 3);
    if (!fu || nmg_fu_planeeqn(fu, &tol) != 0) {
	nmg_km(m);
	return -1;
    }

    if (mk_nmg(wdbp, name, m) != 0) {
	nmg_km(m);
	return -1;
    }
    return 0;
}

static int
test_events(struct ged *gedp)
{
    bu_log("=== GedEventTxn service batching/observers ===\n");

    struct event_counts internal_counts;
    struct event_counts post_counts;
    ged_event_observer_token internal_token =
	ged_event_observer_add(gedp, GED_EVENT_OBSERVER_INTERNAL,
		event_count_cb, &internal_counts);
    ged_event_observer_token post_token =
	ged_event_observer_add(gedp, GED_EVENT_OBSERVER_POST_RECONCILE,
		event_count_cb, &post_counts);
    CHECK(internal_token != 0, "internal event observer must register");
    CHECK(post_token != 0, "post-reconcile event observer must register");

    struct ged_event_txn_result result;
    ged_event_txn_result_init(&result);
    CHECK(ged_event_batch_begin(gedp) == 1,
	    "event batch begin must succeed");
    CHECK(ged_event_notify_object_modified(gedp, "all.g", 1, NULL) == 0,
	    "batched notify must queue first object-modified event");
    CHECK(ged_event_notify_object_modified(gedp, "all.g", 1, NULL) == 0,
	    "batched notify must queue duplicate object-modified event");
    CHECK(ged_event_batch_end(gedp, &result) >= 0,
	    "event batch end must reconcile queued events");

    CHECK(result.event_count == 2,
	    "event result must report two queued input events");
    CHECK(result.coalesced_event_count == 1,
	    "event result must coalesce duplicate events");
    CHECK(internal_counts.calls == 1 && internal_counts.event_count == 1,
	    "internal observer must see one coalesced event batch");
    CHECK(post_counts.calls == 1 && post_counts.event_count == 1,
	    "post observer must see one coalesced event batch");
    CHECK(std::string(bu_vls_cstr(&result.affected_names)).find("all.g") !=
	    std::string::npos,
	    "event result must identify affected names");
    ged_event_txn_result_free(&result);

    CHECK(ged_event_observer_remove(gedp, post_token) == 1,
	    "post observer removal must succeed");
    post_counts = event_counts();
    CHECK(ged_event_notify_object_added(gedp, "all.g", NULL) >= 0,
	    "direct event publish must succeed after observer removal");
    CHECK(post_counts.calls == 0,
	    "removed post observer must not receive later events");
    CHECK(ged_event_observer_remove(gedp, internal_token) == 1,
	    "internal observer removal must succeed");

    event_order_observer librt_add_post;
    ged_event_observer_token librt_add_post_token =
	ged_event_observer_add(gedp, GED_EVENT_OBSERVER_POST_RECONCILE,
		event_order_cb, &librt_add_post);
    CHECK(librt_add_post_token != 0,
	    "librt add coalescing observer must register");
    struct rt_wdb *event_wdbp =
	wdb_dbopen(gedp->dbip, RT_WDB_TYPE_DB_DEFAULT);
    const char *direct_add_name = "_ged_event_direct_add.s";
    point_t direct_add_center = {520.0, 0.0, 0.0};
    ged_event_txn_result_init(&result);
    CHECK(event_wdbp != RT_WDB_NULL,
	    "librt add coalescing fixture must open writer");
    CHECK(ged_event_batch_begin(gedp) == 1,
	    "librt add coalescing batch begin must succeed");
    CHECK(event_wdbp && mk_sph(event_wdbp, direct_add_name,
	    direct_add_center, 1.0) == 0,
	    "librt add coalescing fixture must create object");
    CHECK(ged_event_batch_end(gedp, &result) >= 0,
	    "librt add coalescing batch end must reconcile");
    CHECK(result.event_count >= 2,
	    "librt add coalescing test must queue raw add and put callbacks");
    CHECK(result.coalesced_event_count == 1,
	    "librt add coalescing must reduce add plus put to one event");
    CHECK(librt_add_post.kinds.size() == 1 &&
	    librt_add_post.kinds[0] == GED_EVENT_OBJECT_ADDED,
	    "librt add coalescing observer must see only object-added");
    CHECK(std::find(librt_add_post.all_kinds.begin(),
	    librt_add_post.all_kinds.end(),
	    GED_EVENT_OBJECT_MODIFIED) == librt_add_post.all_kinds.end() &&
	    std::find(librt_add_post.all_kinds.begin(),
	    librt_add_post.all_kinds.end(),
	    GED_EVENT_COMB_TREE_CHANGED) == librt_add_post.all_kinds.end(),
	    "librt add coalescing must suppress redundant modified fallback");
    CHECK(std::string(bu_vls_cstr(&result.affected_names)).
	    find(direct_add_name) != std::string::npos,
	    "librt add coalescing result must identify added object");
    ged_event_txn_result_free(&result);
    CHECK(ged_event_observer_remove(gedp, librt_add_post_token) == 1,
	    "librt add coalescing observer removal must succeed");

    const char *visibility_event_name = "_ged_event_visibility.s";
    if (event_wdbp) {
	point_t visibility_center = {530.0, 0.0, 0.0};
	CHECK(mk_sph(event_wdbp, visibility_event_name,
		    visibility_center, 1.0) == 0,
		"visibility event fixture must create object");
    }
    struct directory *visibility_dp =
	db_lookup(gedp->dbip, visibility_event_name, LOOKUP_QUIET);
    CHECK(visibility_dp != RT_DIR_NULL &&
	    !(visibility_dp->d_flags & RT_DIR_HIDDEN),
	    "visibility event fixture must start unhidden");

    event_order_observer hide_post;
    ged_event_observer_token hide_post_token =
	ged_event_observer_add(gedp, GED_EVENT_OBSERVER_POST_RECONCILE,
		event_order_cb, &hide_post);
    CHECK(hide_post_token != 0,
	    "hide command observer must register");
    const char *hide_av[3] = {"hide", visibility_event_name, NULL};
    CHECK(ged_exec(gedp, 2, hide_av) == BRLCAD_OK,
	    "hide command must publish object-visibility event");
    CHECK(hide_post.calls == 1,
	    "hide command must publish one event transaction");
    CHECK(observed_named_event(hide_post,
		GED_EVENT_OBJECT_VISIBILITY_CHANGED, visibility_event_name),
	    "hide command must emit object-visibility event");
    CHECK(!observed_named_structural_fallback(hide_post,
		visibility_event_name),
	    "hide visibility event must cover raw structural fallback");
    CHECK(hide_post.all_affected_names.find(visibility_event_name) !=
	    std::string::npos,
	    "hide visibility event must identify affected object");
    CHECK(visibility_dp != RT_DIR_NULL &&
	    (visibility_dp->d_flags & RT_DIR_HIDDEN),
	    "hide command must set the directory hidden flag");
    CHECK(ged_event_observer_remove(gedp, hide_post_token) == 1,
	    "hide command observer removal must succeed");

    event_order_observer hide_noop_post;
    ged_event_observer_token hide_noop_post_token =
	ged_event_observer_add(gedp, GED_EVENT_OBSERVER_POST_RECONCILE,
		event_order_cb, &hide_noop_post);
    CHECK(hide_noop_post_token != 0,
	    "repeated hide observer must register");
    CHECK(ged_exec(gedp, 2, hide_av) == BRLCAD_OK,
	    "repeated hide command must remain successful");
    CHECK(hide_noop_post.calls == 0,
	    "repeated hide command must not publish a no-op database event");
    CHECK(ged_event_observer_remove(gedp, hide_noop_post_token) == 1,
	    "repeated hide observer removal must succeed");

    event_order_observer unhide_post;
    ged_event_observer_token unhide_post_token =
	ged_event_observer_add(gedp, GED_EVENT_OBSERVER_POST_RECONCILE,
		event_order_cb, &unhide_post);
    CHECK(unhide_post_token != 0,
	    "unhide command observer must register");
    const char *unhide_av[3] = {"unhide", visibility_event_name, NULL};
    CHECK(ged_exec(gedp, 2, unhide_av) == BRLCAD_OK,
	    "unhide command must publish object-visibility event");
    CHECK(unhide_post.calls == 1,
	    "unhide command must publish one event transaction");
    CHECK(observed_named_event(unhide_post,
		GED_EVENT_OBJECT_VISIBILITY_CHANGED, visibility_event_name),
	    "unhide command must emit object-visibility event");
    CHECK(!observed_named_structural_fallback(unhide_post,
		visibility_event_name),
	    "unhide visibility event must cover raw structural fallback");
    CHECK(unhide_post.all_affected_names.find(visibility_event_name) !=
	    std::string::npos,
	    "unhide visibility event must identify affected object");
    CHECK(visibility_dp != RT_DIR_NULL &&
	    !(visibility_dp->d_flags & RT_DIR_HIDDEN),
	    "unhide command must clear the directory hidden flag");
    CHECK(ged_event_observer_remove(gedp, unhide_post_token) == 1,
	    "unhide command observer removal must succeed");

    event_order_observer unhide_noop_post;
    ged_event_observer_token unhide_noop_post_token =
	ged_event_observer_add(gedp, GED_EVENT_OBSERVER_POST_RECONCILE,
		event_order_cb, &unhide_noop_post);
    CHECK(unhide_noop_post_token != 0,
	    "repeated unhide observer must register");
    CHECK(ged_exec(gedp, 2, unhide_av) == BRLCAD_OK,
	    "repeated unhide command must remain successful");
    CHECK(unhide_noop_post.calls == 0,
	    "repeated unhide command must not publish a no-op database event");
    CHECK(ged_event_observer_remove(gedp, unhide_noop_post_token) == 1,
	    "repeated unhide observer removal must succeed");

    const char *draw_av[2] = {"draw", "all.g"};
    CHECK(ged_exec_draw(gedp, 2, draw_av) == BRLCAD_OK,
	    "event ordering setup draw all.g must succeed");
    uint64_t drawn_revision = ged_draw_scene_revision(gedp);
    CHECK(drawn_revision > 0,
	    "event ordering setup must create a drawn-scene revision");

    event_order_shared order_shared;
    event_order_observer internal_order;
    event_order_observer post_order;
    internal_order.shared = &order_shared;
    internal_order.phase = 'I';
    post_order.shared = &order_shared;
    post_order.phase = 'P';
    ged_event_observer_token internal_order_token =
	ged_event_observer_add(gedp, GED_EVENT_OBSERVER_INTERNAL,
		event_order_cb, &internal_order);
    ged_event_observer_token post_order_token =
	ged_event_observer_add(gedp, GED_EVENT_OBSERVER_POST_RECONCILE,
		event_order_cb, &post_order);
    CHECK(internal_order_token != 0,
	    "internal ordering observer must register");
    CHECK(post_order_token != 0,
	    "post ordering observer must register");

    ged_event_txn_result_init(&result);
    CHECK(ged_event_batch_begin(gedp) == 1,
	    "event ordering batch begin must succeed");
    CHECK(ged_event_notify_object_modified(gedp, "all.g", 0, NULL) == 0,
	    "event ordering must queue first modified event");
    CHECK(ged_event_notify_object_modified(gedp, "all.g", 1, NULL) == 0,
	    "event ordering must queue redraw modified event");
    CHECK(ged_event_notify_attribute_changed(gedp, "platform.r", 0, NULL) == 0,
	    "event ordering must queue first attribute event");
    CHECK(ged_event_notify_attribute_changed(gedp, "platform.r", 1, NULL) == 0,
	    "event ordering must queue redraw attribute event");
    CHECK(ged_event_batch_end(gedp, &result) >= 0,
	    "event ordering batch end must reconcile queued events");

    CHECK(result.event_count == 4,
	    "event ordering result must report all queued inputs");
    CHECK(result.coalesced_event_count == 2,
	    "event ordering result must coalesce mixed duplicate events");
    CHECK(order_shared.phases.size() == 2 &&
	    order_shared.phases[0] == 'I' &&
	    order_shared.phases[1] == 'P',
	    "event observers must run internal before post-reconcile");
    CHECK(internal_order.calls == 1 && post_order.calls == 1,
	    "event ordering observers must each see one batch");
    CHECK(internal_order.event_count == 2 && post_order.event_count == 2,
	    "event ordering observers must see coalesced event count");
    CHECK(internal_order.coalesced_count == 2 &&
	    post_order.coalesced_count == 2,
	    "event ordering observers must see result coalesced count");
    CHECK(internal_order.kinds.size() == 2 &&
	    internal_order.kinds[0] == GED_EVENT_OBJECT_MODIFIED &&
	    internal_order.kinds[1] == GED_EVENT_ATTRIBUTE_CHANGED,
	    "event coalescing must preserve first-seen kind order");
    CHECK(post_order.kinds == internal_order.kinds,
	    "post observer must see the same coalesced event order");
    CHECK(internal_order.redraws.size() == 2 &&
	    internal_order.redraws[0] == 1 &&
	    internal_order.redraws[1] == 1,
	    "event coalescing must OR redraw flags for modified events");
    CHECK(internal_order.current_draw_revision == drawn_revision,
	    "internal observer must run before draw reconciliation");
    CHECK(internal_order.result_draw_after ==
	    internal_order.result_draw_before,
	    "internal observer must see pre-reconcile result revisions");
    CHECK(post_order.current_draw_revision ==
	    result.draw_scene_revision_after,
	    "post observer must see reconciled draw scene revision");
    CHECK(post_order.result_draw_after == result.draw_scene_revision_after,
	    "post observer result must report reconciled draw revision");
    CHECK(post_order.result_draw_after >= post_order.result_draw_before,
	    "post observer draw revision must not move backward");
    CHECK(post_order.affected_names.find("all.g") != std::string::npos &&
	    post_order.affected_names.find("platform.r") != std::string::npos,
	    "post observer result must include mixed affected names");

    ged_event_txn_result_free(&result);
    CHECK(ged_event_observer_remove(gedp, post_order_token) == 1,
	    "post ordering observer removal must succeed");
    CHECK(ged_event_observer_remove(gedp, internal_order_token) == 1,
	    "internal ordering observer removal must succeed");

    event_order_observer metadata_direct_post;
    ged_event_observer_token metadata_direct_post_token =
	ged_event_observer_add(gedp, GED_EVENT_OBSERVER_POST_RECONCILE,
		event_order_cb, &metadata_direct_post);
    CHECK(metadata_direct_post_token != 0,
	    "direct metadata observer must register");
    uint64_t metadata_draw_before = ged_draw_scene_revision(gedp);
    ged_event_txn_result_init(&result);
    CHECK(ged_event_notify_database_metadata_changed(gedp, &result) >= 0,
	    "direct metadata event publish must succeed");
    CHECK(result.event_count == 1 &&
	    result.coalesced_event_count == 1,
	    "direct metadata event result must report one coalesced event");
    CHECK(result.draw_status == 0 && result.db_index_status == 0 &&
	    result.selection_status == 0,
	    "direct metadata event must not redraw, reindex, or recompute selection");
    CHECK(result.draw_scene_revision_after == metadata_draw_before,
	    "direct metadata event must not advance draw scene revision");
    CHECK(metadata_direct_post.kinds.size() == 1 &&
	    metadata_direct_post.kinds[0] ==
	    GED_EVENT_DATABASE_METADATA_CHANGED,
	    "direct metadata observer must see database-metadata event");
    CHECK(bu_vls_strlen(&result.affected_names) == 0,
	    "direct metadata event must not report object paths");
    ged_event_txn_result_free(&result);
    CHECK(ged_event_observer_remove(gedp, metadata_direct_post_token) == 1,
	    "direct metadata observer removal must succeed");

    event_order_observer title_post;
    ged_event_observer_token title_post_token =
	ged_event_observer_add(gedp, GED_EVENT_OBSERVER_POST_RECONCILE,
		event_order_cb, &title_post);
    CHECK(title_post_token != 0,
	    "title metadata observer must register");
    const char *title_av[3] = {"title", "modern metadata title", NULL};
    CHECK(ged_exec(gedp, 2, title_av) == BRLCAD_OK,
	    "title command must publish database metadata event");
    CHECK(std::find(title_post.all_kinds.begin(),
	    title_post.all_kinds.end(),
	    GED_EVENT_DATABASE_METADATA_CHANGED) !=
	    title_post.all_kinds.end(),
	    "title command must emit database-metadata event");
    CHECK(std::find(title_post.all_kinds.begin(),
	    title_post.all_kinds.end(),
	    GED_EVENT_OBJECT_MODIFIED) == title_post.all_kinds.end(),
	    "title metadata event must cover _GLOBAL librt fallback event");
    CHECK(title_post.all_affected_names.empty(),
	    "title metadata event must not report object paths");
    CHECK(ged_event_observer_remove(gedp, title_post_token) == 1,
	    "title metadata observer removal must succeed");

    event_order_observer units_post;
    ged_event_observer_token units_post_token =
	ged_event_observer_add(gedp, GED_EVENT_OBSERVER_POST_RECONCILE,
		event_order_cb, &units_post);
    CHECK(units_post_token != 0,
	    "units metadata observer must register");
    const char *units_av[3] = {"units", "mm", NULL};
    CHECK(ged_exec(gedp, 2, units_av) == BRLCAD_OK,
	    "units command must publish database metadata event");
    CHECK(std::find(units_post.all_kinds.begin(),
	    units_post.all_kinds.end(),
	    GED_EVENT_DATABASE_METADATA_CHANGED) !=
	    units_post.all_kinds.end(),
	    "units command must emit database-metadata event");
    CHECK(std::find(units_post.all_kinds.begin(),
	    units_post.all_kinds.end(),
	    GED_EVENT_OBJECT_MODIFIED) == units_post.all_kinds.end(),
	    "units metadata event must cover _GLOBAL librt fallback event");
    CHECK(units_post.all_affected_names.empty(),
	    "units metadata event must not report object paths");
    CHECK(ged_event_observer_remove(gedp, units_post_token) == 1,
	    "units metadata observer removal must succeed");

    event_order_observer attr_post;
    ged_event_observer_token attr_post_token =
	ged_event_observer_add(gedp, GED_EVENT_OBSERVER_POST_RECONCILE,
		event_order_cb, &attr_post);
    CHECK(attr_post_token != 0,
	    "attr command observer must register");
    const char *attr_av[6] = {"attr", "set", "box.r",
	"modern_test_attr", "1", NULL};
    CHECK(ged_exec(gedp, 5, attr_av) == BRLCAD_OK,
	    "attr set command must publish named attribute event");
    CHECK(std::find(attr_post.all_kinds.begin(), attr_post.all_kinds.end(),
	    GED_EVENT_ATTRIBUTE_CHANGED) != attr_post.all_kinds.end(),
	    "attr set command must emit attribute-changed event");
    CHECK(std::find(attr_post.all_kinds.begin(), attr_post.all_kinds.end(),
	    GED_EVENT_COMB_TREE_CHANGED) == attr_post.all_kinds.end() &&
	    std::find(attr_post.all_kinds.begin(), attr_post.all_kinds.end(),
	    GED_EVENT_OBJECT_MODIFIED) == attr_post.all_kinds.end(),
	    "attr set command semantic event must cover librt fallback events");
    CHECK(attr_post.all_affected_names.find("box.r") != std::string::npos,
	    "attr set command attribute event must identify affected paths");
    CHECK(ged_event_observer_remove(gedp, attr_post_token) == 1,
	    "attr command observer removal must succeed");

    event_order_observer constraint_post;
    ged_event_observer_token constraint_post_token =
	ged_event_observer_add(gedp, GED_EVENT_OBSERVER_POST_RECONCILE,
		event_order_cb, &constraint_post);
    CHECK(constraint_post_token != 0,
	    "constraint command observer must register");
    const char *constraint_av[6] = {"constraint", "set", "box.r",
	"modern_test_constraint", "1", NULL};
    CHECK(ged_exec(gedp, 5, constraint_av) == BRLCAD_OK,
	    "constraint set command must publish named attribute event");
    CHECK(std::find(constraint_post.all_kinds.begin(),
	    constraint_post.all_kinds.end(),
	    GED_EVENT_ATTRIBUTE_CHANGED) != constraint_post.all_kinds.end(),
	    "constraint set command must emit attribute-changed event");
    CHECK(std::find(constraint_post.all_kinds.begin(),
	    constraint_post.all_kinds.end(),
	    GED_EVENT_COMB_TREE_CHANGED) == constraint_post.all_kinds.end() &&
	    std::find(constraint_post.all_kinds.begin(),
	    constraint_post.all_kinds.end(),
	    GED_EVENT_OBJECT_MODIFIED) == constraint_post.all_kinds.end(),
	    "constraint set semantic event must cover librt fallback events");
    CHECK(constraint_post.all_affected_names.find("box.r") !=
	    std::string::npos,
	    "constraint set command attribute event must identify affected paths");
    CHECK(ged_event_observer_remove(gedp, constraint_post_token) == 1,
	    "constraint command observer removal must succeed");

    const char *brep_source_solid = "_ged_event_brep_source.s";
    const char *brep_source = "_ged_event_brep_source.s.brep";
    const char *brep_split = "_ged_event_brep_split";
    const char *brep_make_av[4] = {"make", brep_source_solid, "ell",
	NULL};
    CHECK(ged_exec(gedp, 3, brep_make_av) == BRLCAD_OK,
	    "brep split fixture must create source ellipsoid");
    const char *brep_convert_av[4] = {"brep", brep_source_solid, "brep",
	NULL};
    CHECK(ged_exec(gedp, 3, brep_convert_av) == BRLCAD_OK,
	    "brep split fixture must convert ellipsoid to brep");
    event_order_observer brep_split_post;
    ged_event_observer_token brep_split_post_token =
	ged_event_observer_add(gedp, GED_EVENT_OBSERVER_POST_RECONCILE,
		event_order_cb, &brep_split_post);
    CHECK(brep_split_post_token != 0,
	    "brep split observer must register");
    const char *brep_split_av[8] = {"brep", brep_source, "split", "-t",
	"1", "-o", brep_split, NULL};
    CHECK(ged_exec(gedp, 7, brep_split_av) == BRLCAD_OK,
	    "brep split -t command must create plate-mode brep output");
    CHECK(observed_named_event(brep_split_post, GED_EVENT_OBJECT_ADDED,
		brep_split),
	    "brep split output must emit final object-added event");
    CHECK(!observed_named_event(brep_split_post,
		GED_EVENT_OBJECT_MODIFIED, brep_split),
	    "brep split object-added event must cover raw attribute fallback");
    struct directory *brep_split_dp = db_lookup(gedp->dbip, brep_split,
	    LOOKUP_QUIET);
    CHECK(brep_split_dp != RT_DIR_NULL,
	    "brep split output must be present");
    if (brep_split_dp != RT_DIR_NULL) {
	struct bu_attribute_value_set brep_split_avs;
	bu_avs_init_empty(&brep_split_avs);
	CHECK(db5_get_attributes(gedp->dbip, &brep_split_avs,
		    brep_split_dp) == 0,
		"brep split output attrs must be readable");
	const char *split_thickness =
	    bu_avs_get(&brep_split_avs, "_plate_mode_thickness");
	char *thickness_end = NULL;
	double split_thickness_val = split_thickness ?
	    std::strtod(split_thickness, &thickness_end) : 0.0;
	CHECK(split_thickness && thickness_end != split_thickness &&
		split_thickness_val > 0.9 && split_thickness_val < 1.1,
		"brep split -t output must preserve plate-mode thickness");
	bu_avs_free(&brep_split_avs);
    }
    CHECK(ged_event_observer_remove(gedp, brep_split_post_token) == 1,
	    "brep split observer removal must succeed");

    uint64_t material_revision_before = ged_draw_material_revision(gedp);
    ged_event_txn_result_init(&result);
    CHECK(ged_event_notify_object_material_changed(gedp, "box.r",
	    &result) >= 0,
	    "named material event publish must succeed");
    CHECK(result.event_count == 1 &&
	    result.coalesced_event_count == 1,
	    "named material event result must report one coalesced event");
    CHECK(ged_draw_material_revision(gedp) > material_revision_before,
	    "named material event must bump draw material revision");
    CHECK(std::string(bu_vls_cstr(&result.affected_names)).find("box.r") !=
	    std::string::npos,
	    "named material event result must identify affected paths");
    ged_event_txn_result_free(&result);

    event_order_observer rmater_post;
    ged_event_observer_token rmater_post_token =
	ged_event_observer_add(gedp, GED_EVENT_OBSERVER_POST_RECONCILE,
		event_order_cb, &rmater_post);
    CHECK(rmater_post_token != 0,
	    "rmater material observer must register");
    char rmater_file[MAXPATHLEN] = {0};
    FILE *rmater_fp = bu_temp_file(rmater_file, MAXPATHLEN);
    CHECK(rmater_fp != NULL,
	    "rmater material test must create temp input file");
    if (rmater_fp) {
	std::fprintf(rmater_fp, "box.r plastic 21 43 65 1 0\n");
	std::fclose(rmater_fp);
	uint64_t rmater_revision_before = ged_draw_material_revision(gedp);
	const char *rmater_av[3] = {"rmater", rmater_file, NULL};
	CHECK(ged_exec(gedp, 2, rmater_av) == BRLCAD_OK,
		"rmater command must publish named material event");
	CHECK(ged_draw_material_revision(gedp) > rmater_revision_before,
		"rmater command must bump draw material revision");
	CHECK(std::find(rmater_post.all_kinds.begin(),
		rmater_post.all_kinds.end(),
		GED_EVENT_MATERIAL_CHANGED) != rmater_post.all_kinds.end(),
		"rmater command must emit material-changed event");
	CHECK(rmater_post.all_affected_names.find("box.r") !=
		std::string::npos,
		"rmater command material event must identify affected paths");
	bu_file_delete(rmater_file);
    }
    CHECK(ged_event_observer_remove(gedp, rmater_post_token) == 1,
	    "rmater material observer removal must succeed");

    event_order_observer material_assign_post;
    ged_event_observer_token material_assign_post_token =
	ged_event_observer_add(gedp, GED_EVENT_OBSERVER_POST_RECONCILE,
		event_order_cb, &material_assign_post);
    CHECK(material_assign_post_token != 0,
	    "material assign observer must register");
    uint64_t material_assign_revision_before =
	ged_draw_material_revision(gedp);
    const char *material_assign_av[5] = {"material", "assign", "box.r",
	"modern_test_material", NULL};
    CHECK(ged_exec(gedp, 4, material_assign_av) == BRLCAD_OK,
	    "material assign command must publish named material event");
    CHECK(ged_draw_material_revision(gedp) > material_assign_revision_before,
	    "material assign command must bump draw material revision");
    CHECK(std::find(material_assign_post.all_kinds.begin(),
	    material_assign_post.all_kinds.end(),
	    GED_EVENT_MATERIAL_CHANGED) != material_assign_post.all_kinds.end(),
	    "material assign command must emit material-changed event");
    CHECK(std::find(material_assign_post.all_kinds.begin(),
	    material_assign_post.all_kinds.end(),
	    GED_EVENT_COMB_TREE_CHANGED) == material_assign_post.all_kinds.end() &&
	    std::find(material_assign_post.all_kinds.begin(),
	    material_assign_post.all_kinds.end(),
	    GED_EVENT_OBJECT_MODIFIED) == material_assign_post.all_kinds.end(),
	    "material assign semantic event must cover librt fallback events");
    CHECK(material_assign_post.all_affected_names.find("box.r") !=
	    std::string::npos,
	    "material assign command material event must identify affected paths");
    CHECK(ged_event_observer_remove(gedp, material_assign_post_token) == 1,
	    "material assign observer removal must succeed");

    const char *mater_density_set_av[5] = {"mater", "-d", "set",
	"42,1.0,ModernMapMaterial", NULL};
    CHECK(ged_exec(gedp, 4, mater_density_set_av) == BRLCAD_OK,
	    "mater density setup must create in-database density data");
    const char *mater_map_id_av[6] = {"attr", "set", "box.r",
	"material_id", "42", NULL};
    CHECK(ged_exec(gedp, 5, mater_map_id_av) == BRLCAD_OK,
	    "mater map setup must assign source material_id");
    const char *mater_map_old_name_av[6] = {"attr", "set", "box.r",
	"material_name", "OldMapMaterial", NULL};
    CHECK(ged_exec(gedp, 5, mater_map_old_name_av) == BRLCAD_OK,
	    "mater map setup must assign stale material_name");
    event_order_observer mater_map_post;
    ged_event_observer_token mater_map_post_token =
	ged_event_observer_add(gedp, GED_EVENT_OBSERVER_POST_RECONCILE,
		event_order_cb, &mater_map_post);
    CHECK(mater_map_post_token != 0,
	    "mater map observer must register");
    const char *mater_map_av[5] = {"mater", "-d", "map",
	"--names-from-ids", NULL};
    CHECK(ged_exec(gedp, 4, mater_map_av) == BRLCAD_OK,
	    "mater map command must publish named attribute event");
    CHECK(observed_named_event(mater_map_post, GED_EVENT_ATTRIBUTE_CHANGED,
		"box.r"),
	    "mater map command must emit named attribute-changed event");
    CHECK(!observed_named_event(mater_map_post, GED_EVENT_COMB_TREE_CHANGED,
		"box.r") &&
	    !observed_named_event(mater_map_post, GED_EVENT_OBJECT_MODIFIED,
		"box.r"),
	    "mater map semantic event must cover librt fallback events");
    CHECK(mater_map_post.all_affected_names.find("box.r") !=
	    std::string::npos,
	    "mater map command attribute event must identify affected paths");
    struct directory *mater_map_dp = db_lookup(gedp->dbip, "box.r",
	    LOOKUP_QUIET);
    CHECK(mater_map_dp != RT_DIR_NULL,
	    "mater map test object must still exist");
    if (mater_map_dp != RT_DIR_NULL) {
	struct bu_attribute_value_set mater_map_avs;
	bu_avs_init_empty(&mater_map_avs);
	CHECK(db5_get_attributes(gedp->dbip, &mater_map_avs,
		    mater_map_dp) == 0,
		"mater map test must read updated attributes");
	const char *material_name = bu_avs_get(&mater_map_avs,
		"material_name");
	CHECK(material_name &&
		BU_STR_EQUAL(material_name, "ModernMapMaterial"),
		"mater map command must update material_name");
	bu_avs_free(&mater_map_avs);
    }
    CHECK(ged_event_observer_remove(gedp, mater_map_post_token) == 1,
	    "mater map observer removal must succeed");

    event_order_observer color_post;
    ged_event_observer_token color_post_token =
	ged_event_observer_add(gedp, GED_EVENT_OBSERVER_POST_RECONCILE,
		event_order_cb, &color_post);
    CHECK(color_post_token != 0,
	    "color command observer must register");
    uint64_t color_revision_before = ged_draw_material_revision(gedp);
    const char *color_av[7] = {"color", "902", "903",
	"44", "55", "66", NULL};
    CHECK(ged_exec(gedp, 6, color_av) == BRLCAD_OK,
	    "color command must publish global material event");
    CHECK(ged_draw_material_revision(gedp) > color_revision_before,
	    "color command must bump draw material revision");
    CHECK(std::find(color_post.all_kinds.begin(),
	    color_post.all_kinds.end(),
	    GED_EVENT_MATERIAL_CHANGED) != color_post.all_kinds.end(),
	    "color command must emit material-changed event");
    CHECK(std::find(color_post.all_kinds.begin(),
	    color_post.all_kinds.end(),
	    GED_EVENT_OBJECT_MODIFIED) == color_post.all_kinds.end(),
	    "color command material event must cover _GLOBAL librt fallback event");
    CHECK(color_post.all_affected_names.find(DB5_GLOBAL_OBJECT_NAME) !=
	    std::string::npos,
	    "color command material event must identify _GLOBAL metadata");
    CHECK(ged_event_observer_remove(gedp, color_post_token) == 1,
	    "color command observer removal must succeed");

    char concat_src_g[MAXPATHLEN] = {0};
    bu_dir(concat_src_g, MAXPATHLEN, BU_DIR_CURR,
	    "ged_state_services_concat_source.g", NULL);
    bu_file_delete(concat_src_g);
    struct ged *concat_src = ged_open("db", concat_src_g, 0);
    CHECK(concat_src != NULL,
	    "concat color-table source database must be created");
    if (concat_src) {
	const char *source_color_av[7] = {"color", "900", "901",
	    "11", "22", "33", NULL};
	CHECK(ged_exec(concat_src, 6, source_color_av) == BRLCAD_OK,
		"concat color-table source must define region color table");
	db_sync(concat_src->dbip);
	ged_close(concat_src);
    }

    event_order_observer concat_post;
    ged_event_observer_token concat_post_token =
	ged_event_observer_add(gedp, GED_EVENT_OBSERVER_POST_RECONCILE,
		event_order_cb, &concat_post);
    CHECK(concat_post_token != 0,
	    "concat color-table observer must register");
    uint64_t concat_revision_before = ged_draw_material_revision(gedp);
    const char *concat_av[5] = {"concat", "-c", concat_src_g,
	"_ged_concat_", NULL};
    CHECK(concat_src && ged_exec(gedp, 4, concat_av) == BRLCAD_OK,
	    "concat -c command must publish global material event");
    CHECK(ged_draw_material_revision(gedp) > concat_revision_before,
	    "concat -c command must bump draw material revision");
    CHECK(std::find(concat_post.all_kinds.begin(),
	    concat_post.all_kinds.end(),
	    GED_EVENT_MATERIAL_CHANGED) != concat_post.all_kinds.end(),
	    "concat -c command must emit material-changed event");
    CHECK(std::find(concat_post.all_kinds.begin(),
	    concat_post.all_kinds.end(),
	    GED_EVENT_OBJECT_MODIFIED) == concat_post.all_kinds.end(),
	    "concat -c material event must cover _GLOBAL librt fallback event");
    CHECK(concat_post.all_affected_names.find(DB5_GLOBAL_OBJECT_NAME) !=
	    std::string::npos,
	    "concat -c material event must identify _GLOBAL metadata");
    CHECK(ged_event_observer_remove(gedp, concat_post_token) == 1,
	    "concat color-table observer removal must succeed");
    bu_file_delete(concat_src_g);

    struct rt_wdb *wdbp = wdb_dbopen(gedp->dbip, RT_WDB_TYPE_DB_DEFAULT);
    CHECK(wdbp != RT_WDB_NULL,
	    "comb command event fixture must create writer");
    const char *comb_event_child = "_ged_event_comb_child.s";
    const char *comb_event_region = "_ged_event_comb_region.c";
    const char *comb_event_region_b = "_ged_event_comb_region_b.c";
    const char *adjust_event_comb = "_ged_event_adjust_comb.c";
    const char *adjust_event_prim = "_ged_event_adjust_prim.s";
    const char *put_comb_event_comb = "_ged_event_put_comb.c";
    const char *put_event_dst = "_ged_event_put.s";
    const char *in_direct_event_dst = "_ged_event_in_direct.s";
    const char *in_common_event_dst = "_ged_event_in_common.s";
    const char *heal_event_bot = "_ged_event_heal.bot";
    const char *matrix_event_child = "_ged_event_matrix_child.s";
    const char *matrix_event_source = "_ged_event_matrix_source.c";
    const char *matrix_event_dest = "_ged_event_matrix_dest.c";
    const char *push_event_leaf_a = "_ged_event_push_leaf_a.s";
    const char *push_event_leaf_b = "_ged_event_push_leaf_b.s";
    const char *push_event_mid = "_ged_event_push_mid.c";
    const char *push_event_top = "_ged_event_push_top.c";
    const char *pull_event_leaf = "_ged_event_pull_leaf.s";
    const char *pull_event_mid = "_ged_event_pull_mid.c";
    const char *pull_event_top = "_ged_event_pull_top.c";
    const char *xpush_event_leaf = "_ged_event_xpush_leaf.s";
    const char *xpush_event_a = "_ged_event_xpush_a.c";
    const char *xpush_event_b = "_ged_event_xpush_b.c";
    const char *xpush_event_top = "_ged_event_xpush_top.c";
    const char *npush_event_leaf = "_ged_event_npush_leaf.s";
    const char *npush_event_a = "_ged_event_npush_a.c";
    const char *npush_event_b = "_ged_event_npush_b.c";
    const char *npush_event_top = "_ged_event_npush_top.c";
    const char *clone_event_leaf = "_ged_event_clone_leaf.s";
    const char *clone_xpush_event_leaf = "_ged_event_clone_xpush_leaf.s";
    const char *clone_xpush_event_top = "_ged_event_clone_xpush_top.c";
    const char *mirror_event_src = "_ged_event_mirror_src.s";
    const char *mirror_event_dst = "_ged_event_mirror_dst.s";
    const char *cpi_event_src = "_ged_event_cpi_src.s";
    const char *cpi_event_dst = "_ged_event_cpi_dst.s";
    const char *copyeval_event_src = "_ged_event_copyeval_src.s";
    const char *copyeval_event_dst = "_ged_event_copyeval_dst.s";
    const char *bb_event_dst = "_ged_event_bb_bbox.s";
    const char *rfarb_event_dst = "_ged_event_rfarb.s";
    const char *bot_split_event_src = "_ged_event_bot_split.bot";
    const char *bot_split_event_dst_a = "_ged_event_bot_split.bot.0";
    const char *bot_split_event_dst_b = "_ged_event_bot_split.bot.1";
    const char *shells_event_nmg = "_ged_event_shells.nmg";
    const char *decompose_event_nmg = "_ged_event_decompose.nmg";
    const char *fracture_event_nmg = "_ged_event_fracture.nmg";
    const char *prefix_event_src = "_ged_event_prefix_src.s";
    const char *prefix_event_dst = "pre__ged_event_prefix_src.s";
    const char *prefix_event_parent = "_ged_event_prefix_parent.c";
    if (wdbp) {
	point_t child_center = {340.0, 0.0, 0.0};
	struct wmember empty_wm;
	BU_LIST_INIT(&empty_wm.l);
	CHECK(mk_sph(wdbp, comb_event_child, child_center, 1.0) == 0,
		"comb command event fixture child must be created");
	CHECK(mk_comb(wdbp, comb_event_region, &empty_wm.l, 0, NULL, NULL,
		    NULL, 0, 0, 0, 0, 0, 0, 0) == 0,
		"comb command event fixture comb must be created");
	CHECK(mk_comb(wdbp, comb_event_region_b, &empty_wm.l, 1, NULL, NULL,
		    NULL, 8200, 0, 50, 100, 0, 0, 0) == 0,
		"rcodes command event fixture region must be created");
	CHECK(mk_comb(wdbp, adjust_event_comb, &empty_wm.l, 0, NULL, NULL,
		    NULL, 0, 0, 0, 0, 0, 0, 0) == 0,
		"adjust command event fixture comb must be created");
	CHECK(mk_comb(wdbp, put_comb_event_comb, &empty_wm.l, 0, NULL,
		    NULL, NULL, 0, 0, 0, 0, 0, 0, 0) == 0,
		"put_comb command event fixture comb must be created");
	point_t adjust_prim_v = {345.0, 0.0, 0.0};
	vect_t adjust_prim_h = {0.0, 0.0, 4.0};
	CHECK(mk_cline(wdbp, adjust_event_prim, adjust_prim_v,
		    adjust_prim_h, 1.0, 0.0) == 0,
		"adjust command event fixture primitive must be created");

	point_t matrix_center = {350.0, 0.0, 0.0};
	CHECK(mk_sph(wdbp, matrix_event_child, matrix_center, 1.0) == 0,
		"matrix command event fixture child must be created");
	struct wmember matrix_source_wm;
	BU_LIST_INIT(&matrix_source_wm.l);
	CHECK(mk_addmember(matrix_event_child, &matrix_source_wm.l, NULL,
		    WMOP_UNION) != NULL,
		"matrix command event fixture must add source child");
	CHECK(mk_comb(wdbp, matrix_event_source, &matrix_source_wm.l, 0,
		    NULL, NULL, NULL, 0, 0, 0, 0, 0, 0, 0) == 0,
		"matrix command event fixture source comb must be created");
	struct wmember matrix_dest_wm;
	BU_LIST_INIT(&matrix_dest_wm.l);
	CHECK(mk_addmember(matrix_event_child, &matrix_dest_wm.l, NULL,
		    WMOP_UNION) != NULL,
		"matrix command event fixture must add destination child");
	CHECK(mk_comb(wdbp, matrix_event_dest, &matrix_dest_wm.l, 0,
		    NULL, NULL, NULL, 0, 0, 0, 0, 0, 0, 0) == 0,
		"matrix command event fixture destination comb must be created");

	point_t push_a_center = {355.0, 0.0, 0.0};
	point_t push_b_center = {358.0, 0.0, 0.0};
	struct wmember push_mid_wm;
	BU_LIST_INIT(&push_mid_wm.l);
	CHECK(mk_sph(wdbp, push_event_leaf_a, push_a_center, 1.0) == 0,
		"push command event fixture first leaf must be created");
	CHECK(mk_sph(wdbp, push_event_leaf_b, push_b_center, 1.0) == 0,
		"push command event fixture second leaf must be created");
	CHECK(mk_addmember(push_event_leaf_a, &push_mid_wm.l, NULL,
		    WMOP_UNION) != NULL,
		"push command event fixture must add first leaf");
	CHECK(mk_addmember(push_event_leaf_b, &push_mid_wm.l, NULL,
		    WMOP_UNION) != NULL,
		"push command event fixture must add second leaf");
	CHECK(mk_comb(wdbp, push_event_mid, &push_mid_wm.l, 0, NULL,
		    NULL, NULL, 0, 0, 0, 0, 0, 0, 0) == 0,
		"push command event fixture middle comb must be created");
	mat_t push_mat;
	MAT_IDN(push_mat);
	MAT_DELTAS(push_mat, 4.0, 5.0, 6.0);
	struct wmember push_top_wm;
	BU_LIST_INIT(&push_top_wm.l);
	CHECK(mk_addmember(push_event_mid, &push_top_wm.l, push_mat,
		    WMOP_UNION) != NULL,
		"push command event fixture must add transformed middle comb");
	CHECK(mk_comb(wdbp, push_event_top, &push_top_wm.l, 0, NULL,
		    NULL, NULL, 0, 0, 0, 0, 0, 0, 0) == 0,
		"push command event fixture top comb must be created");

	point_t pull_center = {365.0, 7.0, 0.0};
	struct wmember pull_mid_wm;
	BU_LIST_INIT(&pull_mid_wm.l);
	CHECK(mk_sph(wdbp, pull_event_leaf, pull_center, 1.0) == 0,
		"pull command event fixture leaf must be created");
	CHECK(mk_addmember(pull_event_leaf, &pull_mid_wm.l, NULL,
		    WMOP_UNION) != NULL,
		"pull command event fixture must add leaf");
	CHECK(mk_comb(wdbp, pull_event_mid, &pull_mid_wm.l, 0, NULL,
		    NULL, NULL, 0, 0, 0, 0, 0, 0, 0) == 0,
		"pull command event fixture middle comb must be created");
	struct wmember pull_top_wm;
	BU_LIST_INIT(&pull_top_wm.l);
	CHECK(mk_addmember(pull_event_mid, &pull_top_wm.l, NULL,
		    WMOP_UNION) != NULL,
		"pull command event fixture must add middle comb");
	CHECK(mk_comb(wdbp, pull_event_top, &pull_top_wm.l, 0, NULL,
		    NULL, NULL, 0, 0, 0, 0, 0, 0, 0) == 0,
		"pull command event fixture top comb must be created");

	point_t xpush_center = {372.0, 0.0, 0.0};
	CHECK(mk_sph(wdbp, xpush_event_leaf, xpush_center, 1.0) == 0,
		"xpush command event fixture leaf must be created");
	mat_t xpush_mat_a;
	MAT_IDN(xpush_mat_a);
	MAT_DELTAS(xpush_mat_a, 2.0, 0.0, 0.0);
	struct wmember xpush_a_wm;
	BU_LIST_INIT(&xpush_a_wm.l);
	CHECK(mk_addmember(xpush_event_leaf, &xpush_a_wm.l,
		    xpush_mat_a, WMOP_UNION) != NULL,
		"xpush command event fixture must add first transformed leaf");
	CHECK(mk_comb(wdbp, xpush_event_a, &xpush_a_wm.l, 0, NULL,
		    NULL, NULL, 0, 0, 0, 0, 0, 0, 0) == 0,
		"xpush command event fixture first comb must be created");
	mat_t xpush_mat_b;
	MAT_IDN(xpush_mat_b);
	MAT_DELTAS(xpush_mat_b, 0.0, 3.0, 0.0);
	struct wmember xpush_b_wm;
	BU_LIST_INIT(&xpush_b_wm.l);
	CHECK(mk_addmember(xpush_event_leaf, &xpush_b_wm.l,
		    xpush_mat_b, WMOP_UNION) != NULL,
		"xpush command event fixture must add second transformed leaf");
	CHECK(mk_comb(wdbp, xpush_event_b, &xpush_b_wm.l, 0, NULL,
		    NULL, NULL, 0, 0, 0, 0, 0, 0, 0) == 0,
		"xpush command event fixture second comb must be created");
	struct wmember xpush_top_wm;
	BU_LIST_INIT(&xpush_top_wm.l);
	CHECK(mk_addmember(xpush_event_a, &xpush_top_wm.l, NULL,
		    WMOP_UNION) != NULL,
		"xpush command event fixture must add first comb");
	CHECK(mk_addmember(xpush_event_b, &xpush_top_wm.l, NULL,
		    WMOP_UNION) != NULL,
		"xpush command event fixture must add second comb");
	CHECK(mk_comb(wdbp, xpush_event_top, &xpush_top_wm.l, 0, NULL,
		    NULL, NULL, 0, 0, 0, 0, 0, 0, 0) == 0,
		"xpush command event fixture top comb must be created");

	point_t npush_center = {380.0, 0.0, 0.0};
	CHECK(mk_sph(wdbp, npush_event_leaf, npush_center, 1.0) == 0,
		"npush command event fixture leaf must be created");
	mat_t npush_mat_a;
	MAT_IDN(npush_mat_a);
	MAT_DELTAS(npush_mat_a, 2.0, 0.0, 0.0);
	struct wmember npush_a_wm;
	BU_LIST_INIT(&npush_a_wm.l);
	CHECK(mk_addmember(npush_event_leaf, &npush_a_wm.l,
		    npush_mat_a, WMOP_UNION) != NULL,
		"npush command event fixture must add first transformed leaf");
	CHECK(mk_comb(wdbp, npush_event_a, &npush_a_wm.l, 0, NULL,
		    NULL, NULL, 0, 0, 0, 0, 0, 0, 0) == 0,
		"npush command event fixture first comb must be created");
	mat_t npush_mat_b;
	MAT_IDN(npush_mat_b);
	MAT_DELTAS(npush_mat_b, 0.0, 3.0, 0.0);
	struct wmember npush_b_wm;
	BU_LIST_INIT(&npush_b_wm.l);
	CHECK(mk_addmember(npush_event_leaf, &npush_b_wm.l,
		    npush_mat_b, WMOP_UNION) != NULL,
		"npush command event fixture must add second transformed leaf");
	CHECK(mk_comb(wdbp, npush_event_b, &npush_b_wm.l, 0, NULL,
		    NULL, NULL, 0, 0, 0, 0, 0, 0, 0) == 0,
		"npush command event fixture second comb must be created");
	struct wmember npush_top_wm;
	BU_LIST_INIT(&npush_top_wm.l);
	CHECK(mk_addmember(npush_event_a, &npush_top_wm.l, NULL,
		    WMOP_UNION) != NULL,
		"npush command event fixture must add first comb");
	CHECK(mk_addmember(npush_event_b, &npush_top_wm.l, NULL,
		    WMOP_UNION) != NULL,
		"npush command event fixture must add second comb");
	CHECK(mk_comb(wdbp, npush_event_top, &npush_top_wm.l, 0, NULL,
		    NULL, NULL, 0, 0, 0, 0, 0, 0, 0) == 0,
		"npush command event fixture top comb must be created");

	point_t clone_center = {390.0, 0.0, 0.0};
	CHECK(mk_sph(wdbp, clone_event_leaf, clone_center, 1.0) == 0,
		"clone command event fixture leaf must be created");

	point_t clone_xpush_center = {395.0, 0.0, 0.0};
	CHECK(mk_sph(wdbp, clone_xpush_event_leaf, clone_xpush_center,
		    1.0) == 0,
		"clone --xpush event fixture leaf must be created");
	mat_t clone_xpush_mat;
	MAT_IDN(clone_xpush_mat);
	MAT_DELTAS(clone_xpush_mat, 2.0, 3.0, 4.0);
	struct wmember clone_xpush_wm;
	BU_LIST_INIT(&clone_xpush_wm.l);
	CHECK(mk_addmember(clone_xpush_event_leaf, &clone_xpush_wm.l,
		    clone_xpush_mat, WMOP_UNION) != NULL,
		"clone --xpush event fixture must add transformed leaf");
	CHECK(mk_comb(wdbp, clone_xpush_event_top, &clone_xpush_wm.l, 0,
		    NULL, NULL, NULL, 0, 0, 0, 0, 0, 0, 0) == 0,
		"clone --xpush event fixture top comb must be created");

	point_t mirror_src_center = {405.0, 0.0, 0.0};
	CHECK(mk_sph(wdbp, mirror_event_src, mirror_src_center, 1.0) == 0,
		"mirror command event fixture source must be created");
	point_t cpi_base = {410.0, 0.0, 0.0};
	vect_t cpi_height = {0.0, 0.0, 5.0};
	CHECK(mk_rcc(wdbp, cpi_event_src, cpi_base, cpi_height, 1.0) == 0,
		"cpi command event fixture source must be created");
	point_t copyeval_src_center = {415.0, 0.0, 0.0};
	CHECK(mk_sph(wdbp, copyeval_event_src, copyeval_src_center, 1.0) == 0,
		"copyeval command event fixture source must be created");

	point_t prefix_src_center = {420.0, 0.0, 0.0};
	CHECK(mk_sph(wdbp, prefix_event_src, prefix_src_center, 1.0) == 0,
		"prefix command event fixture source must be created");
	struct wmember prefix_wm;
	BU_LIST_INIT(&prefix_wm.l);
	CHECK(mk_addmember(prefix_event_src, &prefix_wm.l, NULL,
		    WMOP_UNION) != NULL,
		"prefix command event fixture must add source child");
	CHECK(mk_comb(wdbp, prefix_event_parent, &prefix_wm.l, 0,
		    NULL, NULL, NULL, 0, 0, 0, 0, 0, 0, 0) == 0,
		"prefix command event fixture parent comb must be created");

	fastf_t heal_vertices[12] = {
	    0.0, 0.0, 0.0,
	    1.0, 0.0, 0.0,
	    0.0, 1.0, 0.0,
	    0.0, 0.0, 1.0
	};
	int heal_faces[12] = {
	    0, 2, 1,
	    0, 3, 2,
	    0, 1, 3,
	    1, 2, 3
	};
	CHECK(mk_bot(wdbp, heal_event_bot, RT_BOT_SOLID, RT_BOT_CCW, 0,
		    4, 4, heal_vertices, heal_faces, NULL, NULL) == 0,
		"heal command event fixture BoT must be created");

	fastf_t bot_split_vertices[18] = {
	    0.0, 0.0, 0.0,
	    1.0, 0.0, 0.0,
	    0.0, 1.0, 0.0,
	    10.0, 0.0, 0.0,
	    11.0, 0.0, 0.0,
	    10.0, 1.0, 0.0
	};
	int bot_split_faces[6] = {
	    0, 1, 2,
	    3, 4, 5
	};
	CHECK(mk_bot(wdbp, bot_split_event_src, RT_BOT_SURFACE,
		    RT_BOT_UNORIENTED, 0, 6, 2, bot_split_vertices,
		    bot_split_faces, NULL, NULL) == 0,
		"bot split command event fixture BoT must be created");

	CHECK(make_event_nmg_tet(wdbp, fracture_event_nmg) == 0,
		"fracture command event fixture NMG must be created");
	CHECK(make_event_nmg_tet(wdbp, shells_event_nmg) == 0,
		"shells command event fixture NMG must be created");
	CHECK(make_event_nmg_tet(wdbp, decompose_event_nmg) == 0,
		"decompose command event fixture NMG must be created");
    }

    event_order_observer arced_post;
    ged_event_observer_token arced_post_token =
	ged_event_observer_add(gedp, GED_EVENT_OBSERVER_POST_RECONCILE,
		event_order_cb, &arced_post);
    CHECK(arced_post_token != 0,
	    "arced observer must register");
    std::string arced_path =
	std::string(matrix_event_source) + "/" + matrix_event_child;
    const char *arced_av[9] = {"arced", arced_path.c_str(), "matrix",
	"rarc", "xlate", "3", "4", "5", NULL};
    CHECK(ged_exec(gedp, 8, arced_av) == BRLCAD_OK,
	    "arced command must publish comb-tree event");
    CHECK(arced_post.calls == 1,
	    "arced command must publish one event transaction");
    CHECK(observed_named_event(arced_post, GED_EVENT_COMB_TREE_CHANGED,
		matrix_event_source),
	    "arced command must emit comb-tree event for edited parent");
    CHECK(!observed_named_event(arced_post, GED_EVENT_OBJECT_MODIFIED,
		matrix_event_source),
	    "arced semantic event must cover raw parent fallback");
    CHECK(arced_post.all_affected_names.find(matrix_event_source) !=
	    std::string::npos,
	    "arced command must identify affected parent comb");
    CHECK(ged_event_observer_remove(gedp, arced_post_token) == 1,
	    "arced observer removal must succeed");

    event_order_observer copymat_post;
    ged_event_observer_token copymat_post_token =
	ged_event_observer_add(gedp, GED_EVENT_OBSERVER_POST_RECONCILE,
		event_order_cb, &copymat_post);
    CHECK(copymat_post_token != 0,
	    "copymat observer must register");
    std::string copymat_src_path =
	std::string(matrix_event_source) + "/" + matrix_event_child;
    std::string copymat_dst_path =
	std::string(matrix_event_dest) + "/" + matrix_event_child;
    const char *copymat_av[4] = {"copymat", copymat_src_path.c_str(),
	copymat_dst_path.c_str(), NULL};
    CHECK(ged_exec(gedp, 3, copymat_av) == BRLCAD_OK,
	    "copymat command must publish comb-tree event");
    CHECK(copymat_post.calls == 1,
	    "copymat command must publish one event transaction");
    CHECK(observed_named_event(copymat_post, GED_EVENT_COMB_TREE_CHANGED,
		matrix_event_dest),
	    "copymat command must emit comb-tree event for destination parent");
    CHECK(!observed_named_event(copymat_post, GED_EVENT_OBJECT_MODIFIED,
		matrix_event_dest),
	    "copymat semantic event must cover raw parent fallback");
    CHECK(copymat_post.all_affected_names.find(matrix_event_dest) !=
	    std::string::npos,
	    "copymat command must identify affected destination comb");
    CHECK(ged_event_observer_remove(gedp, copymat_post_token) == 1,
	    "copymat observer removal must succeed");

    event_order_observer adjust_prim_post;
    ged_event_observer_token adjust_prim_post_token =
	ged_event_observer_add(gedp, GED_EVENT_OBSERVER_POST_RECONCILE,
		event_order_cb, &adjust_prim_post);
    CHECK(adjust_prim_post_token != 0,
	    "adjust primitive observer must register");
    const char *adjust_prim_av[5] = {"adjust", adjust_event_prim, "H",
	"0 0 16", NULL};
    CHECK(ged_exec(gedp, 4, adjust_prim_av) == BRLCAD_OK,
	    "adjust primitive edit must publish object-modified event");
    CHECK(adjust_prim_post.calls == 1,
	    "adjust primitive edit must publish one event transaction");
    CHECK(observed_named_event(adjust_prim_post,
		GED_EVENT_OBJECT_MODIFIED, adjust_event_prim),
	    "adjust primitive edit must emit named object-modified event");
    CHECK(!observed_named_event(adjust_prim_post,
		GED_EVENT_COMB_TREE_CHANGED, adjust_event_prim),
	    "adjust primitive edit must not emit comb-tree event");
    CHECK(adjust_prim_post.all_affected_names.find(adjust_event_prim) !=
	    std::string::npos,
	    "adjust primitive edit must identify affected primitive");
    CHECK(ged_event_observer_remove(gedp, adjust_prim_post_token) == 1,
	    "adjust primitive observer removal must succeed");

    event_order_observer adjust_attr_post;
    ged_event_observer_token adjust_attr_post_token =
	ged_event_observer_add(gedp, GED_EVENT_OBSERVER_POST_RECONCILE,
		event_order_cb, &adjust_attr_post);
    CHECK(adjust_attr_post_token != 0,
	    "adjust comb attr observer must register");
    uint64_t adjust_attr_revision_before = ged_draw_material_revision(gedp);
    const char *adjust_attr_av[7] = {"adjust", adjust_event_comb, "region",
	"1", "id", "8110", NULL};
    CHECK(ged_exec(gedp, 6, adjust_attr_av) == BRLCAD_OK,
	    "adjust comb metadata edit must publish semantic events");
    CHECK(adjust_attr_post.calls == 1,
	    "adjust comb metadata edit must publish one event transaction");
    CHECK(observed_named_event(adjust_attr_post,
		GED_EVENT_ATTRIBUTE_CHANGED, adjust_event_comb),
	    "adjust comb metadata edit must emit attribute-changed event");
    CHECK(observed_named_event(adjust_attr_post,
		GED_EVENT_MATERIAL_CHANGED, adjust_event_comb),
	    "adjust comb id edit must emit material-changed event");
    CHECK(!observed_named_structural_fallback(adjust_attr_post,
		adjust_event_comb),
	    "adjust comb metadata semantic events must cover raw fallback");
    CHECK(ged_draw_material_revision(gedp) > adjust_attr_revision_before,
	    "adjust comb id edit must bump draw material revision");
    CHECK(ged_event_observer_remove(gedp, adjust_attr_post_token) == 1,
	    "adjust comb attr observer removal must succeed");

    event_order_observer adjust_material_post;
    ged_event_observer_token adjust_material_post_token =
	ged_event_observer_add(gedp, GED_EVENT_OBSERVER_POST_RECONCILE,
		event_order_cb, &adjust_material_post);
    CHECK(adjust_material_post_token != 0,
	    "adjust comb material observer must register");
    uint64_t adjust_material_revision_before =
	ged_draw_material_revision(gedp);
    const char *adjust_material_av[5] = {"adjust", adjust_event_comb,
	"shader", "plastic", NULL};
    CHECK(ged_exec(gedp, 4, adjust_material_av) == BRLCAD_OK,
	    "adjust comb material edit must publish material event");
    CHECK(adjust_material_post.calls == 1,
	    "adjust comb material edit must publish one event transaction");
    CHECK(observed_named_event(adjust_material_post,
		GED_EVENT_MATERIAL_CHANGED, adjust_event_comb),
	    "adjust comb material edit must emit material-changed event");
    CHECK(!observed_named_structural_fallback(adjust_material_post,
		adjust_event_comb),
	    "adjust comb material event must cover raw fallback");
    CHECK(ged_draw_material_revision(gedp) >
	    adjust_material_revision_before,
	    "adjust comb material edit must bump draw material revision");
    CHECK(ged_event_observer_remove(gedp, adjust_material_post_token) == 1,
	    "adjust comb material observer removal must succeed");

    event_order_observer adjust_tree_post;
    ged_event_observer_token adjust_tree_post_token =
	ged_event_observer_add(gedp, GED_EVENT_OBSERVER_POST_RECONCILE,
		event_order_cb, &adjust_tree_post);
    CHECK(adjust_tree_post_token != 0,
	    "adjust comb tree observer must register");
    std::string adjust_tree = std::string("l ") + comb_event_child;
    const char *adjust_tree_av[5] = {"adjust", adjust_event_comb, "tree",
	adjust_tree.c_str(), NULL};
    CHECK(ged_exec(gedp, 4, adjust_tree_av) == BRLCAD_OK,
	    "adjust comb tree edit must publish comb-tree event");
    CHECK(adjust_tree_post.calls == 1,
	    "adjust comb tree edit must publish one event transaction");
    CHECK(observed_named_event(adjust_tree_post,
		GED_EVENT_COMB_TREE_CHANGED, adjust_event_comb),
	    "adjust comb tree edit must emit comb-tree event");
    CHECK(!observed_named_event(adjust_tree_post,
		GED_EVENT_OBJECT_MODIFIED, adjust_event_comb),
	    "adjust comb tree semantic event must cover raw object fallback");
    CHECK(adjust_tree_post.all_affected_names.find(adjust_event_comb) !=
	    std::string::npos,
	    "adjust comb tree edit must identify affected comb");
    CHECK(ged_event_observer_remove(gedp, adjust_tree_post_token) == 1,
	    "adjust comb tree observer removal must succeed");

    event_order_observer put_comb_post;
    ged_event_observer_token put_comb_post_token =
	ged_event_observer_add(gedp, GED_EVENT_OBSERVER_POST_RECONCILE,
		event_order_cb, &put_comb_post);
    CHECK(put_comb_post_token != 0,
	    "put_comb observer must register");
    std::string put_comb_expr = std::string("u ") + comb_event_child;
    const char *put_comb_av[12] = {"put_comb", put_comb_event_comb,
	"12 34 56", "plastic", "1", put_comb_expr.c_str(), "y",
	"8120", "0", "44", "100", NULL};
    CHECK(ged_exec(gedp, 11, put_comb_av) == BRLCAD_OK,
	    "put_comb existing comb edit must publish semantic events");
    CHECK(put_comb_post.calls == 1,
	    "put_comb existing comb edit must publish one event transaction");
    CHECK(observed_named_event(put_comb_post,
		GED_EVENT_COMB_TREE_CHANGED, put_comb_event_comb),
	    "put_comb existing comb edit must emit comb-tree event");
    CHECK(observed_named_event(put_comb_post,
		GED_EVENT_ATTRIBUTE_CHANGED, put_comb_event_comb),
	    "put_comb existing comb edit must emit attribute event");
    CHECK(observed_named_event(put_comb_post,
		GED_EVENT_MATERIAL_CHANGED, put_comb_event_comb),
	    "put_comb existing comb edit must emit material event");
    CHECK(std::find(put_comb_post.all_kinds.begin(),
	    put_comb_post.all_kinds.end(),
	    GED_EVENT_OBJECT_ADDED) == put_comb_post.all_kinds.end() &&
	    std::find(put_comb_post.all_kinds.begin(),
	    put_comb_post.all_kinds.end(),
	    GED_EVENT_OBJECT_REMOVED) == put_comb_post.all_kinds.end(),
	    "put_comb existing comb edit must not leak temp add/remove events");
    bool put_comb_temp_event = false;
    for (const std::string &name : put_comb_post.all_named_names) {
	if (name.find("ged_tmp.a") == 0)
	    put_comb_temp_event = true;
    }
    CHECK(!put_comb_temp_event,
	    "put_comb existing comb edit must hide saved temporary comb events");
    CHECK(ged_event_observer_remove(gedp, put_comb_post_token) == 1,
	    "put_comb observer removal must succeed");

    event_order_observer put_post;
    ged_event_observer_token put_post_token =
	ged_event_observer_add(gedp, GED_EVENT_OBSERVER_POST_RECONCILE,
		event_order_cb, &put_post);
    CHECK(put_post_token != 0,
	    "put observer must register");
    const char *put_av[12] = {"put", put_event_dst, "ell", "V",
	"0 0 0", "A", "1 0 0", "B", "0 1 0", "C", "0 0 1", NULL};
    CHECK(ged_exec(gedp, 11, put_av) == BRLCAD_OK,
	    "put command must publish object creation event");
    CHECK(put_post.calls == 1,
	    "put command must publish one event transaction");
    CHECK(observed_named_event(put_post, GED_EVENT_OBJECT_ADDED,
		put_event_dst),
	    "put command must emit object-added event for created object");
    CHECK(std::find(put_post.all_kinds.begin(),
	    put_post.all_kinds.end(), GED_EVENT_OBJECT_REMOVED) ==
	    put_post.all_kinds.end(),
	    "put command creation fixture must not publish removals");
    CHECK(ged_event_observer_remove(gedp, put_post_token) == 1,
	    "put observer removal must succeed");

    event_order_observer in_direct_post;
    ged_event_observer_token in_direct_post_token =
	ged_event_observer_add(gedp, GED_EVENT_OBSERVER_POST_RECONCILE,
		event_order_cb, &in_direct_post);
    CHECK(in_direct_post_token != 0,
	    "in direct-write observer must register");
    const char *in_direct_av[8] = {"in", in_direct_event_dst, "sph",
	"0", "0", "0", "1", NULL};
    CHECK(ged_exec(gedp, 7, in_direct_av) == BRLCAD_OK,
	    "in sph direct-write helper must publish object creation event");
    CHECK(in_direct_post.calls == 1,
	    "in sph direct-write helper must publish one event transaction");
    CHECK(observed_named_event(in_direct_post, GED_EVENT_OBJECT_ADDED,
		in_direct_event_dst),
	    "in sph direct-write helper must emit object-added event");
    CHECK(std::find(in_direct_post.all_kinds.begin(),
	    in_direct_post.all_kinds.end(), GED_EVENT_OBJECT_REMOVED) ==
	    in_direct_post.all_kinds.end(),
	    "in sph creation fixture must not publish removals");
    CHECK(ged_event_observer_remove(gedp, in_direct_post_token) == 1,
	    "in direct-write observer removal must succeed");

    event_order_observer in_common_post;
    ged_event_observer_token in_common_post_token =
	ged_event_observer_add(gedp, GED_EVENT_OBSERVER_POST_RECONCILE,
		event_order_cb, &in_common_post);
    CHECK(in_common_post_token != 0,
	    "in common-write observer must register");
    const char *in_common_av[16] = {"in", in_common_event_dst, "ell",
	"0", "0", "0", "1", "0", "0", "0", "1", "0", "0", "0", "1",
	NULL};
    CHECK(ged_exec(gedp, 15, in_common_av) == BRLCAD_OK,
	    "in ell common writer must publish object creation event");
    CHECK(in_common_post.calls == 1,
	    "in ell common writer must publish one event transaction");
    CHECK(observed_named_event(in_common_post, GED_EVENT_OBJECT_ADDED,
		in_common_event_dst),
	    "in ell common writer must emit object-added event");
    CHECK(std::find(in_common_post.all_kinds.begin(),
	    in_common_post.all_kinds.end(), GED_EVENT_OBJECT_REMOVED) ==
	    in_common_post.all_kinds.end(),
	    "in ell creation fixture must not publish removals");
    CHECK(ged_event_observer_remove(gedp, in_common_post_token) == 1,
	    "in common-write observer removal must succeed");

    event_order_observer heal_post;
    ged_event_observer_token heal_post_token =
	ged_event_observer_add(gedp, GED_EVENT_OBSERVER_POST_RECONCILE,
		event_order_cb, &heal_post);
    CHECK(heal_post_token != 0,
	    "heal observer must register");
    const char *heal_av[3] = {"heal", heal_event_bot, NULL};
    CHECK(ged_exec(gedp, 2, heal_av) == BRLCAD_OK,
	    "heal command must publish object-modified event");
    CHECK(heal_post.calls == 1,
	    "heal command must publish one event transaction");
    CHECK(observed_named_event(heal_post, GED_EVENT_OBJECT_MODIFIED,
		heal_event_bot),
	    "heal command must emit object-modified event for healed BoT");
    CHECK(std::find(heal_post.all_kinds.begin(),
	    heal_post.all_kinds.end(), GED_EVENT_OBJECT_ADDED) ==
	    heal_post.all_kinds.end() &&
	    std::find(heal_post.all_kinds.begin(),
	    heal_post.all_kinds.end(), GED_EVENT_OBJECT_REMOVED) ==
	    heal_post.all_kinds.end(),
	    "heal command mutation fixture must not publish add/remove events");
    CHECK(ged_event_observer_remove(gedp, heal_post_token) == 1,
	    "heal observer removal must succeed");

    event_order_observer push_post;
    ged_event_observer_token push_post_token =
	ged_event_observer_add(gedp, GED_EVENT_OBSERVER_POST_RECONCILE,
		event_order_cb, &push_post);
    CHECK(push_post_token != 0,
	    "push observer must register");
    const char *push_av[3] = {"push", push_event_top, NULL};
    CHECK(ged_exec(gedp, 2, push_av) == BRLCAD_OK,
	    "push command must publish grouped rewrite events");
    CHECK(push_post.calls == 1,
	    "push command must publish one event transaction");
    CHECK(observed_named_event(push_post, GED_EVENT_OBJECT_MODIFIED,
		push_event_leaf_a) &&
	    observed_named_event(push_post, GED_EVENT_OBJECT_MODIFIED,
		push_event_leaf_b),
	    "push command must emit modified events for rewritten leaves");
    CHECK(observed_named_event(push_post, GED_EVENT_COMB_TREE_CHANGED,
		push_event_mid) &&
	    observed_named_event(push_post, GED_EVENT_COMB_TREE_CHANGED,
		push_event_top),
	    "push command must emit comb-tree events for rewritten combinations");
    CHECK(std::find(push_post.all_kinds.begin(), push_post.all_kinds.end(),
	    GED_EVENT_OBJECT_ADDED) == push_post.all_kinds.end() &&
	    std::find(push_post.all_kinds.begin(), push_post.all_kinds.end(),
	    GED_EVENT_OBJECT_REMOVED) == push_post.all_kinds.end(),
	    "push command rewrite must not publish add/remove events");
    CHECK(ged_event_observer_remove(gedp, push_post_token) == 1,
	    "push observer removal must succeed");

    event_order_observer pull_post;
    ged_event_observer_token pull_post_token =
	ged_event_observer_add(gedp, GED_EVENT_OBSERVER_POST_RECONCILE,
		event_order_cb, &pull_post);
    CHECK(pull_post_token != 0,
	    "pull observer must register");
    const char *pull_av[3] = {"pull", pull_event_top, NULL};
    CHECK(ged_exec(gedp, 2, pull_av) == BRLCAD_OK,
	    "pull command must publish grouped rewrite events");
    CHECK(pull_post.calls == 1,
	    "pull command must publish one event transaction");
    CHECK(observed_named_event(pull_post, GED_EVENT_COMB_TREE_CHANGED,
		pull_event_mid) ||
	    observed_named_event(pull_post, GED_EVENT_COMB_TREE_CHANGED,
		pull_event_top),
	    "pull command must emit comb-tree events for rewritten combinations");
    CHECK(std::find(pull_post.all_kinds.begin(), pull_post.all_kinds.end(),
	    GED_EVENT_OBJECT_ADDED) == pull_post.all_kinds.end() &&
	    std::find(pull_post.all_kinds.begin(), pull_post.all_kinds.end(),
	    GED_EVENT_OBJECT_REMOVED) == pull_post.all_kinds.end(),
	    "pull command rewrite must not publish add/remove events");
    CHECK(ged_event_observer_remove(gedp, pull_post_token) == 1,
	    "pull observer removal must succeed");

    event_order_observer xpush_post;
    ged_event_observer_token xpush_post_token =
	ged_event_observer_add(gedp, GED_EVENT_OBSERVER_POST_RECONCILE,
		event_order_cb, &xpush_post);
    CHECK(xpush_post_token != 0,
	    "xpush observer must register");
    const char *xpush_av[3] = {"xpush", xpush_event_top, NULL};
    CHECK(ged_exec(gedp, 2, xpush_av) == BRLCAD_OK,
	    "xpush command must publish grouped rewrite events");
    CHECK(xpush_post.calls == 1,
	    "xpush command must publish one event transaction");
    CHECK(std::find(xpush_post.all_kinds.begin(), xpush_post.all_kinds.end(),
	    GED_EVENT_OBJECT_ADDED) != xpush_post.all_kinds.end(),
	    "xpush command must emit object-added events for copied objects");
    CHECK(observed_named_event(xpush_post, GED_EVENT_COMB_TREE_CHANGED,
		xpush_event_a) &&
	    observed_named_event(xpush_post, GED_EVENT_COMB_TREE_CHANGED,
		xpush_event_b) &&
	    observed_named_event(xpush_post, GED_EVENT_COMB_TREE_CHANGED,
		xpush_event_top),
	    "xpush command must emit comb-tree events for rewritten combinations");
    CHECK(std::find(xpush_post.all_kinds.begin(), xpush_post.all_kinds.end(),
	    GED_EVENT_OBJECT_REMOVED) == xpush_post.all_kinds.end(),
	    "xpush command rewrite fixture must not publish unused-copy removals");
    CHECK(ged_event_observer_remove(gedp, xpush_post_token) == 1,
	    "xpush observer removal must succeed");

    event_order_observer npush_dry_post;
    ged_event_observer_token npush_dry_post_token =
	ged_event_observer_add(gedp, GED_EVENT_OBSERVER_POST_RECONCILE,
		event_order_cb, &npush_dry_post);
    CHECK(npush_dry_post_token != 0,
	    "npush dry-run observer must register");
    const char *npush_dry_av[5] = {"npush", "-f", "-D",
	npush_event_top, NULL};
    CHECK(ged_exec(gedp, 4, npush_dry_av) == BRLCAD_OK,
	    "npush force dry-run must not publish durable events");
    CHECK(npush_dry_post.calls == 0,
	    "npush force dry-run must not notify observers");
    CHECK(ged_event_observer_remove(gedp, npush_dry_post_token) == 1,
	    "npush dry-run observer removal must succeed");

    event_order_observer npush_post;
    ged_event_observer_token npush_post_token =
	ged_event_observer_add(gedp, GED_EVENT_OBSERVER_POST_RECONCILE,
		event_order_cb, &npush_post);
    CHECK(npush_post_token != 0,
	    "npush observer must register");
    const char *npush_av[4] = {"npush", "-f", npush_event_top, NULL};
    CHECK(ged_exec(gedp, 3, npush_av) == BRLCAD_OK,
	    "npush command must publish grouped rewrite events");
    CHECK(npush_post.calls == 1,
	    "npush command must publish one event transaction");
    CHECK(std::find(npush_post.all_kinds.begin(), npush_post.all_kinds.end(),
	    GED_EVENT_OBJECT_ADDED) != npush_post.all_kinds.end(),
	    "npush command must emit object-added events for copied objects");
    bool npush_comb_events =
	observed_named_event(npush_post, GED_EVENT_COMB_TREE_CHANGED,
		npush_event_a) &&
	observed_named_event(npush_post, GED_EVENT_COMB_TREE_CHANGED,
		npush_event_b);
    if (!npush_comb_events) {
	bu_log("npush affected names: %s\n",
		npush_post.all_affected_names.c_str());
	for (size_t i = 0; i < npush_post.all_named_kinds.size() &&
		i < npush_post.all_named_names.size(); i++) {
	    bu_log("npush event kind=%d name=%s\n",
		    (int)npush_post.all_named_kinds[i],
		    npush_post.all_named_names[i].c_str());
	}
    }
    CHECK(npush_comb_events,
	    "npush command must emit comb-tree events for rewritten combinations");
    CHECK(observed_named_event(npush_post, GED_EVENT_OBJECT_MODIFIED,
		npush_event_leaf),
	    "npush command must emit modified event for rewritten shared leaf");
    CHECK(std::find(npush_post.all_kinds.begin(), npush_post.all_kinds.end(),
	    GED_EVENT_OBJECT_REMOVED) == npush_post.all_kinds.end(),
	    "npush command rewrite fixture must not publish removals");
    CHECK(ged_event_observer_remove(gedp, npush_post_token) == 1,
	    "npush observer removal must succeed");

    event_order_observer clone_post;
    ged_event_observer_token clone_post_token =
	ged_event_observer_add(gedp, GED_EVENT_OBSERVER_POST_RECONCILE,
		event_order_cb, &clone_post);
    CHECK(clone_post_token != 0,
	    "clone observer must register");
    const char *clone_av[9] = {"clone", "-n", "2", "-t", "10", "0", "0",
	clone_event_leaf, NULL};
    CHECK(ged_exec(gedp, 8, clone_av) == BRLCAD_OK,
	    "clone command must publish grouped copy events");
    CHECK(clone_post.calls == 1,
	    "clone command must publish one event transaction");
    CHECK(std::count(clone_post.all_kinds.begin(),
	    clone_post.all_kinds.end(), GED_EVENT_OBJECT_ADDED) >= 2,
	    "clone command must emit object-added events for created copies");
    CHECK(std::find(clone_post.all_kinds.begin(), clone_post.all_kinds.end(),
	    GED_EVENT_OBJECT_REMOVED) == clone_post.all_kinds.end(),
	    "clone command copy fixture must not publish removals");
    CHECK(ged_event_observer_remove(gedp, clone_post_token) == 1,
	    "clone observer removal must succeed");

    event_order_observer clone_xpush_post;
    ged_event_observer_token clone_xpush_post_token =
	ged_event_observer_add(gedp, GED_EVENT_OBSERVER_POST_RECONCILE,
		event_order_cb, &clone_xpush_post);
    CHECK(clone_xpush_post_token != 0,
	    "clone --xpush observer must register");
    const char *clone_xpush_av[4] = {"clone", "--xpush",
	clone_xpush_event_top, NULL};
    CHECK(ged_exec(gedp, 3, clone_xpush_av) == BRLCAD_OK,
	    "clone --xpush command must publish grouped copy events");
    CHECK(clone_xpush_post.calls == 1,
	    "clone --xpush command must publish one event transaction");
    CHECK(std::find(clone_xpush_post.all_kinds.begin(),
	    clone_xpush_post.all_kinds.end(), GED_EVENT_OBJECT_ADDED) !=
	    clone_xpush_post.all_kinds.end(),
	    "clone --xpush command must emit object-added events for final copies");
    const char *clone_xpush_temp_top =
	"_ged_event_clone_xpush_top.c1";
    const char *clone_xpush_temp_leaf =
	"_ged_event_clone_xpush_leaf.s100";
    CHECK(!observed_named_event(clone_xpush_post, GED_EVENT_OBJECT_ADDED,
		clone_xpush_temp_top) &&
	    !observed_named_event(clone_xpush_post, GED_EVENT_OBJECT_ADDED,
		clone_xpush_temp_leaf) &&
	    !observed_named_event(clone_xpush_post, GED_EVENT_OBJECT_REMOVED,
		clone_xpush_temp_top) &&
	    !observed_named_event(clone_xpush_post, GED_EVENT_OBJECT_REMOVED,
		clone_xpush_temp_leaf),
	    "clone --xpush command must hide temporary source add/remove events");
    CHECK(std::find(clone_xpush_post.all_kinds.begin(),
	    clone_xpush_post.all_kinds.end(), GED_EVENT_OBJECT_REMOVED) ==
	    clone_xpush_post.all_kinds.end(),
	    "clone --xpush command must not publish temporary cleanup removals");
    CHECK(ged_event_observer_remove(gedp, clone_xpush_post_token) == 1,
	    "clone --xpush observer removal must succeed");

    event_order_observer mirror_post;
    ged_event_observer_token mirror_post_token =
	ged_event_observer_add(gedp, GED_EVENT_OBSERVER_POST_RECONCILE,
		event_order_cb, &mirror_post);
    CHECK(mirror_post_token != 0,
	    "mirror observer must register");
    const char *mirror_av[5] = {"mirror", "-x", mirror_event_src,
	mirror_event_dst, NULL};
    CHECK(ged_exec(gedp, 4, mirror_av) == BRLCAD_OK,
	    "mirror command must publish object creation event");
    CHECK(mirror_post.calls == 1,
	    "mirror command must publish one event transaction");
    CHECK(observed_named_event(mirror_post, GED_EVENT_OBJECT_ADDED,
		mirror_event_dst),
	    "mirror command must emit object-added event for mirrored object");
    CHECK(std::find(mirror_post.all_kinds.begin(),
	    mirror_post.all_kinds.end(), GED_EVENT_OBJECT_REMOVED) ==
	    mirror_post.all_kinds.end(),
	    "mirror command creation fixture must not publish removals");
    CHECK(ged_event_observer_remove(gedp, mirror_post_token) == 1,
	    "mirror observer removal must succeed");

    event_order_observer cpi_post;
    ged_event_observer_token cpi_post_token =
	ged_event_observer_add(gedp, GED_EVENT_OBSERVER_POST_RECONCILE,
		event_order_cb, &cpi_post);
    CHECK(cpi_post_token != 0,
	    "cpi observer must register");
    const char *cpi_av[4] = {"cpi", cpi_event_src, cpi_event_dst, NULL};
    CHECK(ged_exec(gedp, 3, cpi_av) == BRLCAD_OK,
	    "cpi command must publish object creation event");
    CHECK(cpi_post.calls == 1,
	    "cpi command must publish one event transaction");
    CHECK(observed_named_event(cpi_post, GED_EVENT_OBJECT_ADDED,
		cpi_event_dst),
	    "cpi command must emit object-added event for copied cylinder");
    CHECK(std::find(cpi_post.all_kinds.begin(),
	    cpi_post.all_kinds.end(), GED_EVENT_OBJECT_REMOVED) ==
	    cpi_post.all_kinds.end(),
	    "cpi command creation fixture must not publish removals");
    CHECK(ged_event_observer_remove(gedp, cpi_post_token) == 1,
	    "cpi observer removal must succeed");

    event_order_observer copyeval_post;
    ged_event_observer_token copyeval_post_token =
	ged_event_observer_add(gedp, GED_EVENT_OBSERVER_POST_RECONCILE,
		event_order_cb, &copyeval_post);
    CHECK(copyeval_post_token != 0,
	    "copyeval observer must register");
    const char *copyeval_av[4] = {"copyeval", copyeval_event_src,
	copyeval_event_dst, NULL};
    CHECK(ged_exec(gedp, 3, copyeval_av) == BRLCAD_OK,
	    "copyeval command must publish object creation event");
    CHECK(copyeval_post.calls == 1,
	    "copyeval command must publish one event transaction");
    CHECK(observed_named_event(copyeval_post, GED_EVENT_OBJECT_ADDED,
		copyeval_event_dst),
	    "copyeval command must emit object-added event for copied primitive");
    CHECK(std::find(copyeval_post.all_kinds.begin(),
	    copyeval_post.all_kinds.end(), GED_EVENT_OBJECT_REMOVED) ==
	    copyeval_post.all_kinds.end(),
	    "copyeval command creation fixture must not publish removals");
    CHECK(ged_event_observer_remove(gedp, copyeval_post_token) == 1,
	    "copyeval observer removal must succeed");

    event_order_observer bb_post;
    ged_event_observer_token bb_post_token =
	ged_event_observer_add(gedp, GED_EVENT_OBSERVER_POST_RECONCILE,
		event_order_cb, &bb_post);
    CHECK(bb_post_token != 0,
	    "bb observer must register");
    const char *bb_av[5] = {"bb", "-c", bb_event_dst, mirror_event_src, NULL};
    CHECK(ged_exec(gedp, 4, bb_av) == BRLCAD_OK,
	    "bb -c command must publish object creation event");
    CHECK(bb_post.calls == 1,
	    "bb -c command must publish one event transaction");
    CHECK(observed_named_event(bb_post, GED_EVENT_OBJECT_ADDED,
		bb_event_dst),
	    "bb -c command must emit object-added event for bounding box");
    CHECK(std::find(bb_post.all_kinds.begin(),
	    bb_post.all_kinds.end(), GED_EVENT_OBJECT_REMOVED) ==
	    bb_post.all_kinds.end(),
	    "bb -c command creation fixture must not publish removals");
    CHECK(ged_event_observer_remove(gedp, bb_post_token) == 1,
	    "bb observer removal must succeed");

    event_order_observer rfarb_post;
    ged_event_observer_token rfarb_post_token =
	ged_event_observer_add(gedp, GED_EVENT_OBSERVER_POST_RECONCILE,
		event_order_cb, &rfarb_post);
    CHECK(rfarb_post_token != 0,
	    "rfarb observer must register");
    const char *rfarb_av[18] = {"rfarb", rfarb_event_dst, "0", "0", "0",
	"0", "90", "z", "1", "0", "z", "0", "1", "z", "1", "1", "1",
	NULL};
    CHECK(ged_exec(gedp, 17, rfarb_av) == BRLCAD_OK,
	    "rfarb command must publish object creation event");
    CHECK(rfarb_post.calls == 1,
	    "rfarb command must publish one event transaction");
    CHECK(observed_named_event(rfarb_post, GED_EVENT_OBJECT_ADDED,
		rfarb_event_dst),
	    "rfarb command must emit object-added event for created arb");
    CHECK(std::find(rfarb_post.all_kinds.begin(),
	    rfarb_post.all_kinds.end(), GED_EVENT_OBJECT_REMOVED) ==
	    rfarb_post.all_kinds.end(),
	    "rfarb command creation fixture must not publish removals");
    CHECK(ged_event_observer_remove(gedp, rfarb_post_token) == 1,
	    "rfarb observer removal must succeed");

    event_order_observer bot_split_post;
    ged_event_observer_token bot_split_post_token =
	ged_event_observer_add(gedp, GED_EVENT_OBSERVER_POST_RECONCILE,
		event_order_cb, &bot_split_post);
    CHECK(bot_split_post_token != 0,
	    "bot split observer must register");
    const char *bot_split_av[4] = {"bot", "split", bot_split_event_src,
	NULL};
    CHECK(ged_exec(gedp, 3, bot_split_av) == BRLCAD_OK,
	    "bot split command must publish grouped object creation events");
    CHECK(bot_split_post.calls == 1,
	    "bot split command must publish one event transaction");
    CHECK(observed_named_event(bot_split_post, GED_EVENT_OBJECT_ADDED,
		bot_split_event_dst_a),
	    "bot split command must emit object-added event for first output BoT");
    CHECK(observed_named_event(bot_split_post, GED_EVENT_OBJECT_ADDED,
		bot_split_event_dst_b),
	    "bot split command must emit object-added event for second output BoT");
    CHECK(std::find(bot_split_post.all_kinds.begin(),
	    bot_split_post.all_kinds.end(), GED_EVENT_OBJECT_REMOVED) ==
	    bot_split_post.all_kinds.end(),
	    "bot split creation fixture must not publish removals");
    CHECK(ged_event_observer_remove(gedp, bot_split_post_token) == 1,
	    "bot split observer removal must succeed");

    event_order_observer shells_post;
    ged_event_observer_token shells_post_token =
	ged_event_observer_add(gedp, GED_EVENT_OBSERVER_POST_RECONCILE,
		event_order_cb, &shells_post);
    CHECK(shells_post_token != 0,
	    "shells observer must register");
    const char *shells_av[3] = {"shells", shells_event_nmg, NULL};
    CHECK(ged_exec(gedp, 2, shells_av) == BRLCAD_OK,
	    "shells command must publish grouped object creation events");
    CHECK(shells_post.calls == 1,
	    "shells command must publish one event transaction");
    CHECK(std::find(shells_post.all_kinds.begin(),
	    shells_post.all_kinds.end(), GED_EVENT_OBJECT_ADDED) !=
	    shells_post.all_kinds.end(),
	    "shells command must emit added events for created shell objects");
    CHECK(std::find(shells_post.all_kinds.begin(),
	    shells_post.all_kinds.end(), GED_EVENT_OBJECT_REMOVED) ==
	    shells_post.all_kinds.end(),
	    "shells command creation fixture must not publish removals");
    CHECK(ged_event_observer_remove(gedp, shells_post_token) == 1,
	    "shells observer removal must succeed");

    event_order_observer decompose_post;
    ged_event_observer_token decompose_post_token =
	ged_event_observer_add(gedp, GED_EVENT_OBSERVER_POST_RECONCILE,
		event_order_cb, &decompose_post);
    CHECK(decompose_post_token != 0,
	    "decompose observer must register");
    const char *decompose_av[4] = {"decompose", decompose_event_nmg,
	"_ged_event_decompose_part", NULL};
    CHECK(ged_exec(gedp, 3, decompose_av) == BRLCAD_OK,
	    "decompose command must publish grouped object creation events");
    CHECK(decompose_post.calls == 1,
	    "decompose command must publish one event transaction");
    CHECK(std::find(decompose_post.all_kinds.begin(),
	    decompose_post.all_kinds.end(), GED_EVENT_OBJECT_ADDED) !=
	    decompose_post.all_kinds.end(),
	    "decompose command must emit added events for decomposed objects");
    CHECK(std::find(decompose_post.all_kinds.begin(),
	    decompose_post.all_kinds.end(), GED_EVENT_OBJECT_REMOVED) ==
	    decompose_post.all_kinds.end(),
	    "decompose command creation fixture must not publish removals");
    CHECK(ged_event_observer_remove(gedp, decompose_post_token) == 1,
	    "decompose observer removal must succeed");

    event_order_observer fracture_post;
    ged_event_observer_token fracture_post_token =
	ged_event_observer_add(gedp, GED_EVENT_OBSERVER_POST_RECONCILE,
		event_order_cb, &fracture_post);
    CHECK(fracture_post_token != 0,
	    "fracture observer must register");
    const char *fracture_av[4] = {"fracture", fracture_event_nmg,
	"_ged_event_fracture_part", NULL};
    CHECK(ged_exec(gedp, 3, fracture_av) == BRLCAD_OK,
	    "fracture command must publish grouped multi-object events");
    CHECK(fracture_post.calls == 1,
	    "fracture command must publish one event transaction");
    CHECK(std::count(fracture_post.all_kinds.begin(),
	    fracture_post.all_kinds.end(), GED_EVENT_OBJECT_ADDED) >= 2,
	    "fracture command must emit added events for created NMG parts");
    CHECK(std::find(fracture_post.all_kinds.begin(),
	    fracture_post.all_kinds.end(), GED_EVENT_OBJECT_REMOVED) ==
	    fracture_post.all_kinds.end(),
	    "fracture command creation fixture must not publish removals");
    CHECK(ged_event_observer_remove(gedp, fracture_post_token) == 1,
	    "fracture observer removal must succeed");

    event_order_observer comb_add_post;
    ged_event_observer_token comb_add_post_token =
	ged_event_observer_add(gedp, GED_EVENT_OBSERVER_POST_RECONCILE,
		event_order_cb, &comb_add_post);
    CHECK(comb_add_post_token != 0,
	    "comb add observer must register");
    const char *comb_add_av[5] = {"comb", "all.g", "u",
	comb_event_child, NULL};
    CHECK(ged_exec(gedp, 4, comb_add_av) == BRLCAD_OK,
	    "comb command child insertion must publish comb-tree event");
    CHECK(std::find(comb_add_post.all_kinds.begin(),
	    comb_add_post.all_kinds.end(),
	    GED_EVENT_COMB_TREE_CHANGED) != comb_add_post.all_kinds.end(),
	    "comb command child insertion must emit comb-tree event");
    CHECK(std::find(comb_add_post.all_kinds.begin(),
	    comb_add_post.all_kinds.end(),
	    GED_EVENT_OBJECT_MODIFIED) == comb_add_post.all_kinds.end(),
	    "comb command child insertion must not emit object-modified event");
    CHECK(comb_add_post.all_affected_names.find("all.g") !=
	    std::string::npos,
	    "comb command child insertion must identify affected comb paths");
    CHECK(ged_event_observer_remove(gedp, comb_add_post_token) == 1,
	    "comb add observer removal must succeed");

    event_order_observer remove_child_post;
    ged_event_observer_token remove_child_post_token =
	ged_event_observer_add(gedp, GED_EVENT_OBSERVER_POST_RECONCILE,
		event_order_cb, &remove_child_post);
    CHECK(remove_child_post_token != 0,
	    "remove child observer must register");
    const char *remove_child_av[4] = {"remove", "all.g",
	comb_event_child, NULL};
    CHECK(ged_exec(gedp, 3, remove_child_av) == BRLCAD_OK,
	    "remove command child deletion must publish grouped semantic event");
    CHECK(remove_child_post.calls == 1,
	    "remove command child deletion must publish one event transaction");
    CHECK(remove_child_post.kinds.size() == 1 &&
	    remove_child_post.kinds[0] == GED_EVENT_COMB_INSTANCE_REMOVED,
	    "remove command child deletion must suppress raw parent fallback");
    CHECK(std::find(remove_child_post.all_kinds.begin(),
	    remove_child_post.all_kinds.end(),
	    GED_EVENT_COMB_TREE_CHANGED) == remove_child_post.all_kinds.end() &&
	    std::find(remove_child_post.all_kinds.begin(),
	    remove_child_post.all_kinds.end(),
	    GED_EVENT_OBJECT_MODIFIED) == remove_child_post.all_kinds.end(),
	    "remove command comb-instance semantic event must cover librt fallback");
    CHECK(ged_event_observer_remove(gedp, remove_child_post_token) == 1,
	    "remove child observer removal must succeed");

    const char *move_event_old = "_ged_event_move_old.s";
    const char *move_event_new = "_ged_event_move_new.s";
    if (wdbp) {
	point_t move_center = {360.0, 0.0, 0.0};
	CHECK(mk_sph(wdbp, move_event_old, move_center, 1.0) == 0,
		"move command event fixture source must be created");
    }
    event_order_observer move_post;
    ged_event_observer_token move_post_token =
	ged_event_observer_add(gedp, GED_EVENT_OBSERVER_POST_RECONCILE,
		event_order_cb, &move_post);
    CHECK(move_post_token != 0,
	    "move command observer must register");
    const char *move_av[4] = {"move", move_event_old, move_event_new, NULL};
    CHECK(ged_exec(gedp, 3, move_av) == BRLCAD_OK,
	    "move command must publish grouped rename event");
    CHECK(move_post.calls == 1,
	    "move command must publish one event transaction");
    CHECK(move_post.kinds.size() == 1 &&
	    move_post.kinds[0] == GED_EVENT_OBJECT_RENAMED,
	    "move command rename must suppress raw modified fallback");
    CHECK(observed_named_event(move_post, GED_EVENT_OBJECT_RENAMED,
		move_event_old),
	    "move command must emit old-name rename event");
    CHECK(!observed_named_event(move_post, GED_EVENT_OBJECT_MODIFIED,
		move_event_new) &&
	    !observed_named_event(move_post, GED_EVENT_COMB_TREE_CHANGED,
		move_event_new),
	    "move command semantic rename event must cover librt fallback");
    CHECK(ged_event_observer_remove(gedp, move_post_token) == 1,
	    "move command observer removal must succeed");

    const char *move_all_file_old_a = "_ged_event_mvall_file_old_a.s";
    const char *move_all_file_old_b = "_ged_event_mvall_file_old_b.s";
    const char *move_all_file_new_a = "_ged_event_mvall_file_new_a.s";
    const char *move_all_file_new_b = "_ged_event_mvall_file_new_b.s";
    if (wdbp) {
	point_t move_all_file_center_a = {362.0, 0.0, 0.0};
	point_t move_all_file_center_b = {364.0, 0.0, 0.0};
	CHECK(mk_sph(wdbp, move_all_file_old_a,
		    move_all_file_center_a, 1.0) == 0,
		"move_all -f event fixture first source must be created");
	CHECK(mk_sph(wdbp, move_all_file_old_b,
		    move_all_file_center_b, 1.0) == 0,
		"move_all -f event fixture second source must be created");
    }
    event_order_observer move_all_file_post;
    ged_event_observer_token move_all_file_post_token =
	ged_event_observer_add(gedp, GED_EVENT_OBSERVER_POST_RECONCILE,
		event_order_cb, &move_all_file_post);
    CHECK(move_all_file_post_token != 0,
	    "move_all -f observer must register");
    char move_all_file[MAXPATHLEN] = {0};
    FILE *move_all_fp = bu_temp_file(move_all_file, MAXPATHLEN);
    CHECK(move_all_fp != NULL,
	    "move_all -f command test must create input file");
    if (move_all_fp) {
	std::fprintf(move_all_fp, "%s %s\n", move_all_file_old_a,
		move_all_file_new_a);
	std::fprintf(move_all_fp, "%s %s\n", move_all_file_old_b,
		move_all_file_new_b);
	std::fclose(move_all_fp);
	const char *move_all_file_av[4] = {"move_all", "-f",
	    move_all_file, NULL};
	CHECK(ged_exec(gedp, 3, move_all_file_av) == BRLCAD_OK,
		"move_all -f command must publish grouped rename events");
	CHECK(move_all_file_post.calls == 1,
		"move_all -f command must publish one event transaction");
	CHECK(observed_named_event(move_all_file_post,
		    GED_EVENT_OBJECT_RENAMED, move_all_file_old_a),
		"move_all -f command must emit first old-name rename event");
	CHECK(observed_named_event(move_all_file_post,
		    GED_EVENT_OBJECT_RENAMED, move_all_file_old_b),
		"move_all -f command must emit second old-name rename event");
	CHECK(!observed_named_structural_fallback(move_all_file_post,
		    move_all_file_new_a) &&
		!observed_named_structural_fallback(move_all_file_post,
		    move_all_file_new_b),
		"move_all -f rename events must cover raw fallbacks");
	bu_file_delete(move_all_file);
    }
    CHECK(ged_event_observer_remove(gedp, move_all_file_post_token) == 1,
	    "move_all -f observer removal must succeed");

    event_order_observer prefix_post;
    ged_event_observer_token prefix_post_token =
	ged_event_observer_add(gedp, GED_EVENT_OBSERVER_POST_RECONCILE,
		event_order_cb, &prefix_post);
    CHECK(prefix_post_token != 0,
	    "prefix command observer must register");
    const char *prefix_av[4] = {"prefix", "pre_", prefix_event_src, NULL};
    CHECK(ged_exec(gedp, 3, prefix_av) == BRLCAD_OK,
	    "prefix command must publish grouped rename/tree event");
    CHECK(prefix_post.calls == 1,
	    "prefix command must publish one event transaction");
    CHECK(observed_named_event(prefix_post, GED_EVENT_OBJECT_RENAMED,
		prefix_event_src),
	    "prefix command must emit old-name rename event");
    CHECK(observed_named_event(prefix_post, GED_EVENT_COMB_TREE_CHANGED,
		prefix_event_parent),
	    "prefix command must emit comb-tree event for updated parent");
    CHECK(!observed_named_structural_fallback(prefix_post,
		prefix_event_dst),
	    "prefix command rename event must cover raw fallback for renamed object");
    CHECK(prefix_post.all_affected_names.find(prefix_event_parent) !=
	    std::string::npos,
	    "prefix command must identify affected parent comb");
    CHECK(ged_event_observer_remove(gedp, prefix_post_token) == 1,
	    "prefix command observer removal must succeed");

    const char *killrefs_child = "_ged_event_killrefs_child.s";
    const char *killrefs_parent_a = "_ged_event_killrefs_parent_a.c";
    const char *killrefs_parent_b = "_ged_event_killrefs_parent_b.c";
    if (wdbp) {
	point_t killrefs_center = {380.0, 0.0, 0.0};
	struct wmember killrefs_wm;
	CHECK(mk_sph(wdbp, killrefs_child, killrefs_center, 1.0) == 0,
		"killrefs event fixture child must be created");
	BU_LIST_INIT(&killrefs_wm.l);
	CHECK(mk_addmember(killrefs_child, &killrefs_wm.l, NULL,
		    WMOP_UNION) != NULL,
		"killrefs event fixture must add first parent child");
	CHECK(mk_comb(wdbp, killrefs_parent_a, &killrefs_wm.l, 0, NULL,
		    NULL, NULL, 0, 0, 0, 0, 0, 0, 0) == 0,
		"killrefs event fixture first parent must be created");
	BU_LIST_INIT(&killrefs_wm.l);
	CHECK(mk_addmember(killrefs_child, &killrefs_wm.l, NULL,
		    WMOP_UNION) != NULL,
		"killrefs event fixture must add second parent child");
	CHECK(mk_comb(wdbp, killrefs_parent_b, &killrefs_wm.l, 0, NULL,
		    NULL, NULL, 0, 0, 0, 0, 0, 0, 0) == 0,
		"killrefs event fixture second parent must be created");
    }
    event_order_observer killrefs_post;
    ged_event_observer_token killrefs_post_token =
	ged_event_observer_add(gedp, GED_EVENT_OBSERVER_POST_RECONCILE,
		event_order_cb, &killrefs_post);
    CHECK(killrefs_post_token != 0,
	    "killrefs observer must register");
    const char *killrefs_av[3] = {"killrefs", killrefs_child, NULL};
    CHECK(ged_exec(gedp, 2, killrefs_av) == BRLCAD_OK,
	    "killrefs command must publish grouped reference-removal event");
    CHECK(killrefs_post.calls == 1,
	    "killrefs command must publish one grouped event transaction");
    CHECK(observed_named_event(killrefs_post,
		GED_EVENT_OBJECT_REFERENCES_REMOVED, killrefs_child),
	    "killrefs command must emit object-references-removed event");
    std::string killrefs_path_a =
	std::string(killrefs_parent_a) + "/" + killrefs_child;
    std::string killrefs_path_b =
	std::string(killrefs_parent_b) + "/" + killrefs_child;
    CHECK(observed_scoped_event(killrefs_post,
		GED_EVENT_OBJECT_REFERENCES_REMOVED, killrefs_child,
		killrefs_parent_a, killrefs_path_a.c_str()) &&
	    observed_scoped_event(killrefs_post,
		GED_EVENT_OBJECT_REFERENCES_REMOVED, killrefs_child,
		killrefs_parent_b, killrefs_path_b.c_str()),
	    "killrefs command must identify each parent reference removed");
    CHECK(std::find(killrefs_post.all_kinds.begin(),
	    killrefs_post.all_kinds.end(),
	    GED_EVENT_COMB_TREE_CHANGED) == killrefs_post.all_kinds.end() &&
	    std::find(killrefs_post.all_kinds.begin(),
	    killrefs_post.all_kinds.end(),
	    GED_EVENT_OBJECT_MODIFIED) == killrefs_post.all_kinds.end(),
	    "killrefs command semantic parent events must cover raw comb fallback");
    CHECK(ged_db_index_path_resolve(gedp, killrefs_path_a.c_str(),
		NULL, 0) == 0 &&
	    ged_db_index_path_resolve(gedp, killrefs_path_b.c_str(), NULL, 0)
	    == 0,
	    "killrefs command semantic parent events must update DB index paths");
    CHECK(ged_event_observer_remove(gedp, killrefs_post_token) == 1,
	    "killrefs observer removal must succeed");

    event_order_observer comb_region_post;
    ged_event_observer_token comb_region_post_token =
	ged_event_observer_add(gedp, GED_EVENT_OBSERVER_POST_RECONCILE,
		event_order_cb, &comb_region_post);
    CHECK(comb_region_post_token != 0,
	    "comb region observer must register");
    const char *comb_region_av[4] = {"comb", "-r", comb_event_region, NULL};
    CHECK(ged_exec(gedp, 3, comb_region_av) == BRLCAD_OK,
	    "comb -r command must publish attribute event");
    CHECK(std::find(comb_region_post.all_kinds.begin(),
	    comb_region_post.all_kinds.end(),
	    GED_EVENT_ATTRIBUTE_CHANGED) != comb_region_post.all_kinds.end(),
	    "comb -r command must emit attribute-changed event");
    CHECK(std::find(comb_region_post.all_kinds.begin(),
	    comb_region_post.all_kinds.end(),
	    GED_EVENT_COMB_TREE_CHANGED) == comb_region_post.all_kinds.end() &&
	    std::find(comb_region_post.all_kinds.begin(),
	    comb_region_post.all_kinds.end(),
	    GED_EVENT_OBJECT_MODIFIED) == comb_region_post.all_kinds.end(),
	    "comb -r command semantic event must cover librt fallback events");
    CHECK(comb_region_post.all_affected_names.find(comb_event_region) !=
	    std::string::npos,
	    "comb -r command attribute event must identify affected comb");
    CHECK(ged_event_observer_remove(gedp, comb_region_post_token) == 1,
	    "comb region observer removal must succeed");

    event_order_observer comb_color_post;
    ged_event_observer_token comb_color_post_token =
	ged_event_observer_add(gedp, GED_EVENT_OBSERVER_POST_RECONCILE,
		event_order_cb, &comb_color_post);
    CHECK(comb_color_post_token != 0,
	    "comb_color observer must register");
    uint64_t comb_color_revision_before = ged_draw_material_revision(gedp);
    const char *comb_color_av[6] = {"comb_color", comb_event_region, "11",
	"22", "33", NULL};
    CHECK(ged_exec(gedp, 5, comb_color_av) == BRLCAD_OK,
	    "comb_color command must publish named material event");
    CHECK(comb_color_post.calls == 1,
	    "comb_color command must publish one event transaction");
    CHECK(observed_named_event(comb_color_post,
		GED_EVENT_MATERIAL_CHANGED, comb_event_region),
	    "comb_color command must emit material-changed event");
    CHECK(!observed_named_structural_fallback(comb_color_post,
		comb_event_region),
	    "comb_color semantic event must cover raw comb fallback");
    CHECK(ged_draw_material_revision(gedp) > comb_color_revision_before,
	    "comb_color command must bump draw material revision");
    CHECK(ged_event_observer_remove(gedp, comb_color_post_token) == 1,
	    "comb_color observer removal must succeed");

    event_order_observer shader_post;
    ged_event_observer_token shader_post_token =
	ged_event_observer_add(gedp, GED_EVENT_OBSERVER_POST_RECONCILE,
		event_order_cb, &shader_post);
    CHECK(shader_post_token != 0,
	    "shader observer must register");
    uint64_t shader_revision_before = ged_draw_material_revision(gedp);
    const char *shader_av[5] = {"shader", comb_event_region, "plastic",
	"di=0.5", NULL};
    CHECK(ged_exec(gedp, 4, shader_av) == BRLCAD_OK,
	    "shader command must publish named material event");
    CHECK(shader_post.calls == 1,
	    "shader command must publish one event transaction");
    CHECK(observed_named_event(shader_post,
		GED_EVENT_MATERIAL_CHANGED, comb_event_region),
	    "shader command must emit material-changed event");
    CHECK(!observed_named_structural_fallback(shader_post,
		comb_event_region),
	    "shader semantic event must cover raw comb fallback");
    CHECK(ged_draw_material_revision(gedp) > shader_revision_before,
	    "shader command must bump draw material revision");
    CHECK(ged_event_observer_remove(gedp, shader_post_token) == 1,
	    "shader observer removal must succeed");

    event_order_observer shader_noop_post;
    ged_event_observer_token shader_noop_post_token =
	ged_event_observer_add(gedp, GED_EVENT_OBSERVER_POST_RECONCILE,
		event_order_cb, &shader_noop_post);
    CHECK(shader_noop_post_token != 0,
	    "shader no-op observer must register");
    uint64_t shader_revision_after = ged_draw_material_revision(gedp);
    CHECK(ged_exec(gedp, 4, shader_av) == BRLCAD_OK,
	    "shader no-op command must succeed");
    CHECK(shader_noop_post.calls == 0,
	    "shader no-op command must not publish an event transaction");
    CHECK(ged_draw_material_revision(gedp) == shader_revision_after,
	    "shader no-op command must not bump draw material revision");
    CHECK(ged_event_observer_remove(gedp, shader_noop_post_token) == 1,
	    "shader no-op observer removal must succeed");

    event_order_observer item_post;
    ged_event_observer_token item_post_token =
	ged_event_observer_add(gedp, GED_EVENT_OBSERVER_POST_RECONCILE,
		event_order_cb, &item_post);
    CHECK(item_post_token != 0,
	    "item observer must register");
    uint64_t item_revision_before = ged_draw_material_revision(gedp);
    const char *item_av[7] = {"item", comb_event_region, "8101", "0",
	"41", "100", NULL};
    CHECK(ged_exec(gedp, 6, item_av) == BRLCAD_OK,
	    "item command must publish region metadata events");
    CHECK(item_post.calls == 1,
	    "item command must publish one event transaction");
    CHECK(observed_named_event(item_post, GED_EVENT_ATTRIBUTE_CHANGED,
		comb_event_region),
	    "item command must emit attribute-changed event");
    CHECK(observed_named_event(item_post, GED_EVENT_MATERIAL_CHANGED,
		comb_event_region),
	    "item command must emit material-changed event for region/material ids");
    CHECK(!observed_named_structural_fallback(item_post, comb_event_region),
	    "item semantic events must cover raw comb fallback");
    CHECK(ged_draw_material_revision(gedp) > item_revision_before,
	    "item command must bump draw material revision");
    CHECK(ged_event_observer_remove(gedp, item_post_token) == 1,
	    "item observer removal must succeed");

    event_order_observer edcomb_post;
    ged_event_observer_token edcomb_post_token =
	ged_event_observer_add(gedp, GED_EVENT_OBSERVER_POST_RECONCILE,
		event_order_cb, &edcomb_post);
    CHECK(edcomb_post_token != 0,
	    "edcomb observer must register");
    uint64_t edcomb_revision_before = ged_draw_material_revision(gedp);
    const char *edcomb_av[8] = {"edcomb", comb_event_region, "R", "8102",
	"0", "100", "42", NULL};
    CHECK(ged_exec(gedp, 7, edcomb_av) == BRLCAD_OK,
	    "edcomb command must publish region metadata events");
    CHECK(edcomb_post.calls == 1,
	    "edcomb command must publish one event transaction");
    CHECK(observed_named_event(edcomb_post, GED_EVENT_ATTRIBUTE_CHANGED,
		comb_event_region),
	    "edcomb command must emit attribute-changed event");
    CHECK(observed_named_event(edcomb_post, GED_EVENT_MATERIAL_CHANGED,
		comb_event_region),
	    "edcomb command must emit material-changed event for region/material ids");
    CHECK(!observed_named_structural_fallback(edcomb_post, comb_event_region),
	    "edcomb semantic events must cover raw comb fallback");
    CHECK(ged_draw_material_revision(gedp) > edcomb_revision_before,
	    "edcomb command must bump draw material revision");
    CHECK(ged_event_observer_remove(gedp, edcomb_post_token) == 1,
	    "edcomb observer removal must succeed");

    event_order_observer rcodes_post;
    ged_event_observer_token rcodes_post_token =
	ged_event_observer_add(gedp, GED_EVENT_OBSERVER_POST_RECONCILE,
		event_order_cb, &rcodes_post);
    CHECK(rcodes_post_token != 0,
	    "rcodes observer must register");
    char rcodes_file[MAXPATHLEN] = {0};
    FILE *rcodes_fp = bu_temp_file(rcodes_file, MAXPATHLEN);
    CHECK(rcodes_fp != NULL,
	    "rcodes command test must create input file");
    if (rcodes_fp) {
	std::fprintf(rcodes_fp, "8103 0 43 100 %s\n", comb_event_region);
	std::fprintf(rcodes_fp, "8201 0 51 100 %s\n", comb_event_region_b);
	std::fclose(rcodes_fp);
	uint64_t rcodes_revision_before = ged_draw_material_revision(gedp);
	const char *rcodes_av[3] = {"rcodes", rcodes_file, NULL};
	CHECK(ged_exec(gedp, 2, rcodes_av) == BRLCAD_OK,
		"rcodes command must publish region metadata events");
	CHECK(rcodes_post.calls == 1,
		"rcodes command must publish one event transaction");
	CHECK(observed_named_event(rcodes_post, GED_EVENT_ATTRIBUTE_CHANGED,
		    comb_event_region),
		"rcodes command must emit attribute-changed event");
	CHECK(observed_named_event(rcodes_post, GED_EVENT_MATERIAL_CHANGED,
		    comb_event_region),
		"rcodes command must emit material-changed event for region/material ids");
	CHECK(observed_named_event(rcodes_post, GED_EVENT_ATTRIBUTE_CHANGED,
		    comb_event_region_b),
		"rcodes command must emit attribute-changed event for every changed region");
	CHECK(observed_named_event(rcodes_post, GED_EVENT_MATERIAL_CHANGED,
		    comb_event_region_b),
		"rcodes command must emit material-changed event for every changed region/material id");
	CHECK(!observed_named_structural_fallback(rcodes_post,
		    comb_event_region) &&
		!observed_named_structural_fallback(rcodes_post,
		    comb_event_region_b),
		"rcodes semantic events must cover raw comb fallback");
	CHECK(ged_draw_material_revision(gedp) > rcodes_revision_before,
		"rcodes command must bump draw material revision");
	bu_file_delete(rcodes_file);
    }
    CHECK(ged_event_observer_remove(gedp, rcodes_post_token) == 1,
	    "rcodes observer removal must succeed");

    event_order_observer red_post;
    ged_event_observer_token red_post_token =
	ged_event_observer_add(gedp, GED_EVENT_OBSERVER_POST_RECONCILE,
		event_order_cb, &red_post);
    CHECK(red_post_token != 0,
	    "red command observer must register");

    char red_editor_file[MAXPATHLEN] = {0};
    FILE *red_editor_fp = bu_temp_file(red_editor_file, MAXPATHLEN);
    CHECK(red_editor_fp != NULL,
	    "red command test must create scripted editor");
    if (red_editor_fp) {
	std::fprintf(red_editor_fp, "#!/bin/sh\n");
	std::fprintf(red_editor_fp, "tmp=\"$1\"\n");
	std::fprintf(red_editor_fp, "out=\"${tmp}.redtest\"\n");
	std::fprintf(red_editor_fp,
		"awk '\n"
		"/^---------- Combination Tree ----------/ {\n"
		"  print \"modern_red_attr = 1\"\n"
		"  print\n"
		"  print \"\"\n"
		"  print \" u %s\"\n"
		"  next\n"
		"}\n"
		"{ print }\n"
		"' \"$tmp\" > \"$out\" && mv \"$out\" \"$tmp\"\n",
		comb_event_child);
	std::fclose(red_editor_fp);

	struct bu_vls red_editstring = BU_VLS_INIT_ZERO;
	bu_vls_sprintf(&red_editstring, "(null) (null) /bin/sh %s",
		red_editor_file);
	const char *red_av[5] = {"red", "-E", bu_vls_cstr(&red_editstring),
	    comb_event_region, NULL};
	int red_ret = ged_exec(gedp, 4, red_av);
	if (red_ret != BRLCAD_OK)
	    bu_log("red command result: %s\n",
		    bu_vls_cstr(gedp->ged_result_str));
	CHECK(red_ret == BRLCAD_OK,
		"red command scripted edit must publish semantic events");
	CHECK(observed_named_event(red_post, GED_EVENT_COMB_TREE_CHANGED,
		comb_event_region),
		"red command tree edit must emit final comb-tree event");
	CHECK(observed_named_event(red_post, GED_EVENT_ATTRIBUTE_CHANGED,
		comb_event_region),
		"red command attr edit must emit final attribute-changed event");
	CHECK(red_post.all_affected_names.find(comb_event_region) !=
		std::string::npos,
		"red command semantic events must identify final comb");

	struct directory *red_dp =
	    db_lookup(gedp->dbip, comb_event_region, LOOKUP_QUIET);
	CHECK(red_dp != RT_DIR_NULL,
		"red command final comb must remain present");
	if (red_dp != RT_DIR_NULL) {
	    struct bu_attribute_value_set red_avs;
	    bu_avs_init_empty(&red_avs);
	    CHECK(db5_get_attributes(gedp->dbip, &red_avs, red_dp) == 0,
		    "red command final comb attrs must be readable");
	    const char *red_attr = bu_avs_get(&red_avs, "modern_red_attr");
	    CHECK(red_attr && BU_STR_EQUAL(red_attr, "1"),
		    "red command scripted attribute must persist");
	    bu_avs_free(&red_avs);
	}
	std::string red_child_path =
	    std::string(comb_event_region) + "/" + comb_event_child;
	CHECK(ged_db_index_path_resolve(gedp, red_child_path.c_str(),
		NULL, 0) == 2,
		"red command scripted tree child must update DB index");

	bu_vls_free(&red_editstring);
	bu_file_delete(red_editor_file);
    }
    CHECK(ged_event_observer_remove(gedp, red_post_token) == 1,
	    "red command observer removal must succeed");

    event_order_observer red_attr_post;
    ged_event_observer_token red_attr_post_token =
	ged_event_observer_add(gedp, GED_EVENT_OBSERVER_POST_RECONCILE,
		event_order_cb, &red_attr_post);
    CHECK(red_attr_post_token != 0,
	    "red attr-only observer must register");

    char red_attr_editor_file[MAXPATHLEN] = {0};
    FILE *red_attr_editor_fp =
	bu_temp_file(red_attr_editor_file, MAXPATHLEN);
    CHECK(red_attr_editor_fp != NULL,
	    "red attr-only test must create scripted editor");
    if (red_attr_editor_fp) {
	std::fprintf(red_attr_editor_fp, "#!/bin/sh\n");
	std::fprintf(red_attr_editor_fp, "tmp=\"$1\"\n");
	std::fprintf(red_attr_editor_fp, "out=\"${tmp}.redattrtest\"\n");
	std::fprintf(red_attr_editor_fp,
		"awk '\n"
		"/^modern_red_attr[[:space:]]*=/ {\n"
		"  print \"modern_red_attr = 2\"\n"
		"  next\n"
		"}\n"
		"{ print }\n"
		"' \"$tmp\" > \"$out\" && mv \"$out\" \"$tmp\"\n");
	std::fclose(red_attr_editor_fp);

	struct bu_vls red_attr_editstring = BU_VLS_INIT_ZERO;
	bu_vls_sprintf(&red_attr_editstring, "(null) (null) /bin/sh %s",
		red_attr_editor_file);
	const char *red_attr_av[5] = {"red", "-E",
	    bu_vls_cstr(&red_attr_editstring), comb_event_region, NULL};
	int red_attr_ret = ged_exec(gedp, 4, red_attr_av);
	if (red_attr_ret != BRLCAD_OK)
	    bu_log("red attr-only result: %s\n",
		    bu_vls_cstr(gedp->ged_result_str));
	CHECK(red_attr_ret == BRLCAD_OK,
		"red attr-only scripted edit must succeed");
	CHECK(observed_named_event(red_attr_post, GED_EVENT_ATTRIBUTE_CHANGED,
		comb_event_region),
		"red attr-only edit must emit final attribute-changed event");
	CHECK(!observed_named_event(red_attr_post, GED_EVENT_COMB_TREE_CHANGED,
		comb_event_region),
		"red attr-only edit must not emit final comb-tree event");

	struct directory *red_attr_dp =
	    db_lookup(gedp->dbip, comb_event_region, LOOKUP_QUIET);
	CHECK(red_attr_dp != RT_DIR_NULL,
		"red attr-only final comb must remain present");
	if (red_attr_dp != RT_DIR_NULL) {
	    struct bu_attribute_value_set red_attr_avs;
	    bu_avs_init_empty(&red_attr_avs);
	    CHECK(db5_get_attributes(gedp->dbip, &red_attr_avs,
			red_attr_dp) == 0,
		    "red attr-only final comb attrs must be readable");
	    const char *red_attr_value =
		bu_avs_get(&red_attr_avs, "modern_red_attr");
	    CHECK(red_attr_value && BU_STR_EQUAL(red_attr_value, "2"),
		    "red attr-only scripted attribute must persist");
	    bu_avs_free(&red_attr_avs);
	}

	bu_vls_free(&red_attr_editstring);
	bu_file_delete(red_attr_editor_file);
    }
    CHECK(ged_event_observer_remove(gedp, red_attr_post_token) == 1,
	    "red attr-only observer removal must succeed");

    const char *facet_child = "_ged_event_facetize_child.s";
    const char *facet_region = "_ged_event_facetize_region.r";
    const char *facet_output = "_ged_event_facetize_output.c";
    if (wdbp) {
	point_t facet_center = {620.0, 0.0, 0.0};
	struct wmember facet_wm;
	BU_LIST_INIT(&facet_wm.l);
	CHECK(mk_sph(wdbp, facet_child, facet_center, 1.0) == 0,
		"facetize old event fixture child must be created");
	CHECK(mk_addmember(facet_child, &facet_wm.l, NULL, WMOP_UNION) !=
		NULL,
		"facetize old event fixture must add child");
	CHECK(mk_comb(wdbp, facet_region, &facet_wm.l, 1, NULL, NULL,
		    NULL, 0, 0, 0, 0, 0, 0, 0) == 0,
		"facetize old event fixture region must be created");
    }
    const char *facet_attr_av[6] = {"attr", "set", facet_region,
	"modern_facetize_attr", "1", NULL};
    CHECK(ged_exec(gedp, 5, facet_attr_av) == BRLCAD_OK,
	    "facetize old event fixture attr must be set");

    event_order_observer facet_old_post;
    ged_event_observer_token facet_old_post_token =
	ged_event_observer_add(gedp, GED_EVENT_OBSERVER_POST_RECONCILE,
		event_order_cb, &facet_old_post);
    CHECK(facet_old_post_token != 0,
	    "facetize old observer must register");
    const char *facet_av[7] = {"facetize_old", "-q", "-r", "--NMG",
	facet_region, facet_output, NULL};
    int facet_ret = ged_exec(gedp, 6, facet_av);
    if (facet_ret != BRLCAD_OK)
	bu_log("facetize_old result: %s\n",
		bu_vls_cstr(gedp->ged_result_str));
    CHECK(facet_ret == BRLCAD_OK,
	    "facetize_old region copy must succeed");
    std::string facet_copy;
    for (size_t i = 0; i < facet_old_post.all_named_kinds.size() &&
	    i < facet_old_post.all_named_names.size(); i++) {
	if (facet_old_post.all_named_kinds[i] == GED_EVENT_ATTRIBUTE_CHANGED &&
		facet_old_post.all_named_names[i].find(facet_region) == 0 &&
		!BU_STR_EQUAL(facet_old_post.all_named_names[i].c_str(),
		    facet_region)) {
	    facet_copy = facet_old_post.all_named_names[i];
	    break;
	}
    }
    if (facet_copy.empty() || db_lookup(gedp->dbip, facet_copy.c_str(),
		LOOKUP_QUIET) == RT_DIR_NULL) {
	bu_log("facetize_old affected names: %s\n",
		facet_old_post.all_affected_names.c_str());
	for (size_t i = 0; i < facet_old_post.all_named_kinds.size() &&
		i < facet_old_post.all_named_names.size(); i++) {
	    bu_log("facetize_old event kind=%d name=%s\n",
		    (int)facet_old_post.all_named_kinds[i],
		    facet_old_post.all_named_names[i].c_str());
	}
	struct directory *diag_dp = RT_DIR_NULL;
	FOR_ALL_DIRECTORY_START(diag_dp, gedp->dbip) {
	    if (diag_dp->d_namep &&
		    std::strstr(diag_dp->d_namep, "_ged_event_facetize"))
		bu_log("facetize_old db entry: %s flags=0x%x\n",
			diag_dp->d_namep, diag_dp->d_flags);
	} FOR_ALL_DIRECTORY_END;
    }
    CHECK(!facet_copy.empty(),
	    "facetize_old copied comb must emit final attribute event");
    CHECK(facet_old_post.all_affected_names.find(facet_copy) !=
	    std::string::npos,
	    "facetize_old copied comb event must identify copied comb");

    struct directory *facet_copy_dp =
	db_lookup(gedp->dbip, facet_copy.c_str(), LOOKUP_QUIET);
    CHECK(facet_copy_dp != RT_DIR_NULL,
	    "facetize_old copied comb must be present");
    if (facet_copy_dp != RT_DIR_NULL) {
	struct bu_attribute_value_set facet_avs;
	bu_avs_init_empty(&facet_avs);
	CHECK(db5_get_attributes(gedp->dbip, &facet_avs,
		    facet_copy_dp) == 0,
		"facetize_old copied comb attrs must be readable");
	const char *facet_attr =
	    bu_avs_get(&facet_avs, "modern_facetize_attr");
	CHECK(facet_attr && BU_STR_EQUAL(facet_attr, "1"),
		"facetize_old copied comb must preserve source attr");
	bu_avs_free(&facet_avs);
    }
    std::string facet_copy_path =
	std::string(facet_output) + "/" + facet_copy;
    CHECK(ged_db_index_path_resolve(gedp, facet_copy_path.c_str(),
	    NULL, 0) == 2,
	    "facetize_old copied comb must update DB index path");
    CHECK(ged_event_observer_remove(gedp, facet_old_post_token) == 1,
	    "facetize old observer removal must succeed");

    const char *erase_av[2] = {"erase", "all.g"};
    CHECK(ged_exec_erase(gedp, 2, erase_av) == BRLCAD_OK,
	    "event ordering cleanup erase all.g must succeed");

    return 0;
}

static int
test_draw(struct ged *gedp)
{
    bu_log("=== GedDrawState retained path queries ===\n");

    const char *draw_av[2] = {"draw", "all.g"};
    CHECK(ged_exec_draw(gedp, 2, draw_av) == BRLCAD_OK,
	    "draw all.g must succeed");
    CHECK(ged_draw_has_shapes(gedp) == 1,
	    "retained draw state must contain shapes after draw");
    CHECK(ged_draw_scene_revision(gedp) > 0,
	    "draw scene revision must advance after draw");
    CHECK(ged_draw_path_state(gedp, gedp->ged_gvp, "all.g", -1) == 1,
	    "draw path_state must report all.g fully drawn");
    CHECK(ged_draw_path_state(gedp, gedp->ged_gvp,
		"all.g/platform.r", -1) == 1,
	    "draw path_state must report child path drawn");

    struct bu_vls group_paths = BU_VLS_INIT_ZERO;
    CHECK(ged_draw_list_paths(gedp, gedp->ged_gvp, -1, 0, &group_paths) > 0,
	    "draw list must report command-level paths");
    CHECK(std::string(bu_vls_cstr(&group_paths)).find("all.g") !=
	    std::string::npos,
	    "draw list must include all.g");
    bu_vls_free(&group_paths);

    struct bu_vls leaf_paths = BU_VLS_INIT_ZERO;
    CHECK(ged_draw_list_paths(gedp, gedp->ged_gvp, -1, 1, &leaf_paths) > 0,
	    "expanded draw list must report realized leaf paths");
    CHECK(std::string(bu_vls_cstr(&leaf_paths)).find("platform.r") !=
	    std::string::npos,
	    "expanded draw list must include realized child paths");
    bu_vls_free(&leaf_paths);

    const char *erase_av[2] = {"erase", "all.g"};
    CHECK(ged_exec_erase(gedp, 2, erase_av) == BRLCAD_OK,
	    "erase all.g must succeed");
    CHECK(ged_draw_path_state(gedp, gedp->ged_gvp, "all.g", -1) == 0,
	    "draw path_state must clear after erase");

    return 0;
}

static int
copy_fixture(const char *src_dir, const char *dst_name)
{
    struct bu_vls src_path = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&src_path, "%s/moss.g", src_dir);
    if (!bu_file_exists(bu_vls_cstr(&src_path), NULL)) {
	bu_log("ERROR: [%s] does not exist\n", bu_vls_cstr(&src_path));
	bu_vls_free(&src_path);
	return 0;
    }

    std::ifstream src(bu_vls_cstr(&src_path), std::ios::binary);
    std::ofstream dst(dst_name, std::ios::binary);
    dst << src.rdbuf();
    int ok = src.good() && dst.good();
    bu_vls_free(&src_path);
    return ok;
}

int
main(int ac, char *av[])
{
    bu_setprogname(av[0]);

    if (ac != 2 && ac != 3) {
	bu_log("Usage: %s <dir-containing-moss.g> [index-scale-fanout]\n", av[0]);
	return 1;
    }

    size_t index_scale_fanout = default_index_scale_fanout;
    if (ac == 3) {
	char *end = NULL;
	errno = 0;
	unsigned long long parsed = std::strtoull(av[2], &end, 10);
	unsigned long long max_fanout =
	    static_cast<unsigned long long>((std::numeric_limits<size_t>::max)());
	if (errno == ERANGE || !parsed || !end || *end != '\0' ||
		parsed > max_fanout) {
	    bu_log("ERROR: invalid index-scale-fanout '%s'\n", av[2]);
	    return 1;
	}
	index_scale_fanout = (size_t)parsed;
    }

    if (index_scale_fanout > default_index_scale_fanout) {
	const char *run_slow = std::getenv("BRLCAD_RUN_SLOW_TESTS");
	if (!run_slow || !run_slow[0] || BU_STR_EQUAL(run_slow, "0")) {
	    bu_log("Skipping slow GedDbIndex fanout=%zu test; set BRLCAD_RUN_SLOW_TESTS=1 to run it.\n",
		    index_scale_fanout);
	    return 123;
	}
    }

    if (!bu_file_directory(av[1])) {
	bu_log("ERROR: [%s] is not a directory\n", av[1]);
	return 2;
    }

    char lcache[MAXPATHLEN] = {0};
    std::string cache_name = state_services_cache_name(index_scale_fanout);
    bu_dir(lcache, MAXPATHLEN, BU_DIR_CURR,
	    cache_name.c_str(), NULL);
    bu_mkdir(lcache);
    bu_setenv("BU_DIR_CACHE", lcache, 1);
    bu_setenv("LIBRT_USE_COMB_INSTANCE_SPECIFIERS", "1", 1);

    std::string tmp_g = state_services_tmp_name(index_scale_fanout);
    if (!copy_fixture(av[1], tmp_g.c_str()))
	return 3;

    struct ged *gedp = open_service_ged(tmp_g.c_str());
    if (!gedp) {
	bu_file_delete(tmp_g.c_str());
	return 4;
    }

    test_db_index(gedp, index_scale_fanout);
    test_selection(gedp);
    test_events(gedp);
    test_draw(gedp);

    ged_close(gedp);
    bu_file_delete(tmp_g.c_str());

    if (g_failures) {
	bu_log("%d GED state-service check(s) FAILED.\n", g_failures);
	return 1;
    }

    bu_log("All GED state-service tests PASSED.\n");
    return 0;
}
