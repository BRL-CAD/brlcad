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

struct event_refresh_counts {
    size_t calls = 0;
};

static void
event_refresh_cb(void *client_data)
{
    struct event_refresh_counts *counts =
	(struct event_refresh_counts *)client_data;
    if (counts)
	counts->calls++;
}

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
make_event_nmg_box(struct rt_wdb *wdbp, const char *name)
{
    struct bn_tol tol = BN_TOL_INIT_TOL;
    struct model *m = nmg_mm();
    struct nmgregion *r = nmg_mrsv(m);
    struct shell *s = BU_LIST_FIRST(shell, &r->s_hd);
    struct vertex *verts[8] = {NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL};
    point_t pts[8] = {
	{0.0, 0.0, 0.0},
	{1.0, 0.0, 0.0},
	{1.0, 1.0, 0.0},
	{0.0, 1.0, 0.0},
	{0.0, 0.0, 1.0},
	{1.0, 0.0, 1.0},
	{1.0, 1.0, 1.0},
	{0.0, 1.0, 1.0}
    };
    int face_ids[6][4] = {
	{0, 3, 2, 1},
	{4, 5, 6, 7},
	{0, 1, 5, 4},
	{3, 7, 6, 2},
	{0, 4, 7, 3},
	{1, 2, 6, 5}
    };

    if (!wdbp || !name || !m || !r || !s) {
	if (m)
	    nmg_km(m);
	return -1;
    }

    for (size_t i = 0; i < 6; i++) {
	struct vertex **fv[4] = {
	    &verts[face_ids[i][0]],
	    &verts[face_ids[i][1]],
	    &verts[face_ids[i][2]],
	    &verts[face_ids[i][3]]
	};
	struct faceuse *fu = nmg_cmface(s, fv, 4);
	for (size_t j = 0; j < 4; j++) {
	    int vindex = face_ids[i][j];
	    if (verts[vindex])
		nmg_vertex_gv(verts[vindex], pts[vindex]);
	}
	if (!fu || nmg_fu_planeeqn(fu, &tol) != 0) {
	    nmg_km(m);
	    return -1;
	}
    }

    if (mk_nmg(wdbp, name, m) != 0) {
	nmg_km(m);
	return -1;
    }
    return 0;
}

static long
first_nmg_face_index(struct ged *gedp, const char *name)
{
    struct directory *dp = db_lookup(gedp->dbip, name, LOOKUP_QUIET);
    if (dp == RT_DIR_NULL)
	return -1;

    struct rt_db_internal intern;
    if (rt_db_get_internal(&intern, dp, gedp->dbip, bn_mat_identity) < 0)
	return -1;

    long face_index = -1;
    if (intern.idb_type == ID_NMG) {
	struct model *m = (struct model *)intern.idb_ptr;
	struct nmgregion *r = NULL;
	struct shell *s = NULL;
	struct faceuse *fu = NULL;

	for (BU_LIST_FOR(r, nmgregion, &m->r_hd)) {
	    for (BU_LIST_FOR(s, shell, &r->s_hd)) {
		for (BU_LIST_FOR(fu, faceuse, &s->fu_hd)) {
		    if (fu->orientation != OT_SAME)
			continue;
		    face_index = fu->f_p->index;
		    goto done;
		}
	    }
	}
    }

done:
    rt_db_free_internal(&intern);
    return face_index;
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

    void (*old_refresh_handler)(void *) = gedp->ged_refresh_handler;
    void *old_refresh_clientdata = gedp->ged_refresh_clientdata;
    event_refresh_counts refresh_counts;
    gedp->ged_refresh_handler = event_refresh_cb;
    gedp->ged_refresh_clientdata = &refresh_counts;

    event_order_observer bulk_post;
    ged_event_observer_token bulk_post_token =
	ged_event_observer_add(gedp, GED_EVENT_OBSERVER_POST_RECONCILE,
		event_order_cb, &bulk_post);
    CHECK(bulk_post_token != 0,
	    "bulk transaction observer must register");
    ged_event_txn_result_init(&result);
    CHECK(ged_event_bulk_begin(gedp) == 1,
	    "bulk transaction begin must succeed");
    CHECK(ged_event_bulk_active(gedp) == 1,
	    "bulk transaction must report active");
    CHECK(ged_event_batch_begin(gedp) == 1,
	    "bulk transaction must allow nested command batches to pair");
    CHECK(ged_event_notify_object_modified(gedp, "all.g", 1, NULL) == 0,
	    "bulk transaction must defer published mutation events");
    CHECK(ged_event_batch_end(gedp, NULL) == 0,
	    "bulk transaction nested command batch end must not reconcile");
    CHECK(bulk_post.calls == 0,
	    "bulk transaction must not dispatch before final refresh");
    CHECK(ged_event_bulk_end(gedp, &result) >= 0,
	    "bulk transaction end must perform final refresh");
    CHECK(ged_event_bulk_active(gedp) == 0,
	    "bulk transaction must report inactive after end");
    CHECK(bulk_post.calls == 1 && bulk_post.event_count == 1,
	    "bulk transaction observer must see one final event");
    CHECK(bulk_post.kinds.size() == 1 &&
	    bulk_post.kinds[0] == GED_EVENT_BATCH_REBUILD,
	    "bulk transaction final event must be a batch rebuild");
    CHECK(result.event_count == 1 && result.coalesced_event_count == 1,
	    "bulk transaction result must report one final rebuild event");
    CHECK(refresh_counts.calls == 1,
	    "bulk transaction must run one final refresh callback");
    ged_event_txn_result_free(&result);
    CHECK(ged_event_observer_remove(gedp, bulk_post_token) == 1,
	    "bulk transaction observer removal must succeed");

    refresh_counts = event_refresh_counts();
    event_order_observer outer_bulk_post;
    ged_event_observer_token outer_bulk_post_token =
	ged_event_observer_add(gedp, GED_EVENT_OBSERVER_POST_RECONCILE,
		event_order_cb, &outer_bulk_post);
    CHECK(outer_bulk_post_token != 0,
	    "outer bulk transaction observer must register");
    ged_event_txn_result_init(&result);
    CHECK(ged_event_batch_begin(gedp) == 1,
	    "outer bulk command batch begin must succeed");
    CHECK(ged_event_bulk_begin(gedp) == 1,
	    "outer bulk transaction begin must succeed");
    CHECK(ged_event_batch_begin(gedp) == 2,
	    "outer bulk nested command batch must increment depth");
    CHECK(ged_event_notify_object_modified(gedp, "all.g", 1, NULL) == 0,
	    "outer bulk nested event must be deferred");
    CHECK(ged_event_batch_end(gedp, NULL) == 1,
	    "outer bulk nested command batch must leave outer batch open");
    CHECK(outer_bulk_post.calls == 0,
	    "outer bulk nested command batch must not dispatch");
    CHECK(ged_event_bulk_end(gedp, NULL) == 0,
	    "outer bulk transaction end must queue final refresh in outer batch");
    CHECK(outer_bulk_post.calls == 0,
	    "outer bulk final refresh must wait for outer batch end");
    CHECK(refresh_counts.calls == 0,
	    "outer bulk final refresh callback must wait for outer batch end");
    CHECK(ged_event_batch_end(gedp, &result) >= 0,
	    "outer bulk command batch end must dispatch final refresh");
    CHECK(outer_bulk_post.calls == 1 && outer_bulk_post.event_count == 1,
	    "outer bulk observer must see one final event");
    CHECK(outer_bulk_post.kinds.size() == 1 &&
	    outer_bulk_post.kinds[0] == GED_EVENT_BATCH_REBUILD,
	    "outer bulk final event must be a batch rebuild");
    CHECK(result.event_count == 1 && result.coalesced_event_count == 1,
	    "outer bulk result must report one final rebuild event");
    CHECK(refresh_counts.calls == 1,
	    "outer bulk must run one final refresh callback");
    ged_event_txn_result_free(&result);
    CHECK(ged_event_observer_remove(gedp, outer_bulk_post_token) == 1,
	    "outer bulk transaction observer removal must succeed");

    gedp->ged_refresh_handler = old_refresh_handler;
    gedp->ged_refresh_clientdata = old_refresh_clientdata;

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

    const char *brep_convert_solid = "_ged_event_brep_convert_source.s";
    const char *brep_convert_out = "_ged_event_brep_convert_out.brep";
    const char *brep_convert_make_av[4] = {"make", brep_convert_solid,
	"ell", NULL};
    CHECK(ged_exec(gedp, 3, brep_convert_make_av) == BRLCAD_OK,
	    "brep conversion fixture must create source ellipsoid");
    event_order_observer brep_convert_post;
    ged_event_observer_token brep_convert_post_token =
	ged_event_observer_add(gedp, GED_EVENT_OBSERVER_POST_RECONCILE,
		event_order_cb, &brep_convert_post);
    CHECK(brep_convert_post_token != 0,
	    "brep conversion observer must register");
    const char *brep_conversion_av[5] = {"brep", brep_convert_solid, "brep",
	brep_convert_out, NULL};
    CHECK(ged_exec(gedp, 4, brep_conversion_av) == BRLCAD_OK,
	    "brep conversion command must create named brep output");
    CHECK(brep_convert_post.calls == 1,
	    "brep conversion command must publish one event transaction");
    CHECK(observed_named_event(brep_convert_post, GED_EVENT_OBJECT_ADDED,
		brep_convert_out),
	    "brep conversion output must emit object-added event");
    CHECK(!observed_named_event(brep_convert_post,
		GED_EVENT_OBJECT_MODIFIED, brep_convert_out),
	    "brep conversion output must not report final object as modified");
    CHECK(db_lookup(gedp->dbip, brep_convert_out, LOOKUP_QUIET) !=
	    RT_DIR_NULL,
	    "brep conversion output must be present");
    CHECK(ged_event_observer_remove(gedp, brep_convert_post_token) == 1,
	    "brep conversion observer removal must succeed");

    const char *brep_bool_l_solid = "_ged_event_brep_bool_left.s";
    const char *brep_bool_r_solid = "_ged_event_brep_bool_right.s";
    const char *brep_bool_l = "_ged_event_brep_bool_left.brep";
    const char *brep_bool_r = "_ged_event_brep_bool_right.brep";
    const char *brep_bool_out = "_ged_event_brep_bool_out.brep";
    const char *brep_bool_l_make_av[4] = {"make", brep_bool_l_solid,
	"sph", NULL};
    CHECK(ged_exec(gedp, 3, brep_bool_l_make_av) == BRLCAD_OK,
	    "brep boolean fixture must create left source sphere");
    const char *brep_bool_r_make_av[4] = {"make", brep_bool_r_solid,
	"ell", NULL};
    CHECK(ged_exec(gedp, 3, brep_bool_r_make_av) == BRLCAD_OK,
	    "brep boolean fixture must create right source ellipsoid");
    const char *brep_bool_l_convert_av[5] = {"brep", brep_bool_l_solid,
	"brep", brep_bool_l, NULL};
    CHECK(ged_exec(gedp, 4, brep_bool_l_convert_av) == BRLCAD_OK,
	    "brep boolean fixture must convert left source to brep");
    const char *brep_bool_r_convert_av[5] = {"brep", brep_bool_r_solid,
	"brep", brep_bool_r, NULL};
    CHECK(ged_exec(gedp, 4, brep_bool_r_convert_av) == BRLCAD_OK,
	    "brep boolean fixture must convert right source to brep");
    event_order_observer brep_bool_post;
    ged_event_observer_token brep_bool_post_token =
	ged_event_observer_add(gedp, GED_EVENT_OBSERVER_POST_RECONCILE,
		event_order_cb, &brep_bool_post);
    CHECK(brep_bool_post_token != 0,
	    "brep boolean observer must register");
    const char *brep_bool_av[7] = {"brep", brep_bool_l, "bool", "u",
	brep_bool_r, brep_bool_out, NULL};
    CHECK(ged_exec(gedp, 6, brep_bool_av) == BRLCAD_OK,
	    "brep boolean command must create named output");
    CHECK(brep_bool_post.calls == 1,
	    "brep boolean command must publish one event transaction");
    CHECK(observed_named_event(brep_bool_post, GED_EVENT_OBJECT_ADDED,
		brep_bool_out),
	    "brep boolean output must emit object-added event");
    CHECK(db_lookup(gedp->dbip, brep_bool_out, LOOKUP_QUIET) !=
	    RT_DIR_NULL,
	    "brep boolean output must be present");
    CHECK(std::find(brep_bool_post.all_kinds.begin(),
	    brep_bool_post.all_kinds.end(), GED_EVENT_OBJECT_REMOVED) ==
	    brep_bool_post.all_kinds.end(),
	    "brep boolean output fixture must not publish removals");
    CHECK(ged_event_observer_remove(gedp, brep_bool_post_token) == 1,
	    "brep boolean observer removal must succeed");

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

    const char *brep_edit_solid = "_ged_event_brep_edit_source.s";
    const char *brep_edit = "_ged_event_brep_edit_source.brep";
    const char *brep_edit_make_av[4] = {"make", brep_edit_solid, "sph",
	NULL};
    CHECK(ged_exec(gedp, 3, brep_edit_make_av) == BRLCAD_OK,
	    "brep edit fixture must create source sphere");
    const char *brep_edit_convert_av[5] = {"brep", brep_edit_solid,
	"brep", brep_edit, NULL};
    CHECK(ged_exec(gedp, 4, brep_edit_convert_av) == BRLCAD_OK,
	    "brep edit fixture must convert sphere to brep");

    event_order_observer brep_geo_post;
    ged_event_observer_token brep_geo_post_token =
	ged_event_observer_add(gedp, GED_EVENT_OBSERVER_POST_RECONCILE,
		event_order_cb, &brep_geo_post);
    CHECK(brep_geo_post_token != 0,
	    "brep geo observer must register");
    const char *brep_geo_av[8] = {"brep", brep_edit, "geo",
	"v_create", "3", "0", "0", NULL};
    CHECK(ged_exec(gedp, 7, brep_geo_av) == BRLCAD_OK,
	    "brep geo v_create command must publish object-modified event");
    CHECK(brep_geo_post.calls == 1,
	    "brep geo v_create command must publish one event transaction");
    CHECK(observed_named_event(brep_geo_post, GED_EVENT_OBJECT_MODIFIED,
		brep_edit),
	    "brep geo v_create command must emit object-modified event");
    CHECK(!observed_named_event(brep_geo_post, GED_EVENT_OBJECT_ADDED,
		brep_edit),
	    "brep geo v_create command must not report in-place edit as add");
    CHECK(ged_event_observer_remove(gedp, brep_geo_post_token) == 1,
	    "brep geo observer removal must succeed");

    event_order_observer brep_topo_post;
    ged_event_observer_token brep_topo_post_token =
	ged_event_observer_add(gedp, GED_EVENT_OBSERVER_POST_RECONCILE,
		event_order_cb, &brep_topo_post);
    CHECK(brep_topo_post_token != 0,
	    "brep topo observer must register");
    const char *brep_topo_av[6] = {"brep", brep_edit, "topo", "f_rev",
	"0", NULL};
    CHECK(ged_exec(gedp, 5, brep_topo_av) == BRLCAD_OK,
	    "brep topo f_rev command must publish object-modified event");
    CHECK(brep_topo_post.calls == 1,
	    "brep topo f_rev command must publish one event transaction");
    CHECK(observed_named_event(brep_topo_post, GED_EVENT_OBJECT_MODIFIED,
		brep_edit),
	    "brep topo f_rev command must emit object-modified event");
    CHECK(!observed_named_event(brep_topo_post, GED_EVENT_OBJECT_ADDED,
		brep_edit),
	    "brep topo f_rev command must not report in-place edit as add");
    CHECK(ged_event_observer_remove(gedp, brep_topo_post_token) == 1,
	    "brep topo observer removal must succeed");

    event_order_observer brep_flip_post;
    ged_event_observer_token brep_flip_post_token =
	ged_event_observer_add(gedp, GED_EVENT_OBSERVER_POST_RECONCILE,
		event_order_cb, &brep_flip_post);
    CHECK(brep_flip_post_token != 0,
	    "brep flip observer must register");
    const char *brep_flip_av[4] = {"brep", brep_edit, "flip", NULL};
    CHECK(ged_exec(gedp, 3, brep_flip_av) == BRLCAD_OK,
	    "brep flip command must publish object-modified event");
    CHECK(brep_flip_post.calls == 1,
	    "brep flip command must publish one event transaction");
    CHECK(observed_named_event(brep_flip_post, GED_EVENT_OBJECT_MODIFIED,
		brep_edit),
	    "brep flip command must emit object-modified event");
    CHECK(!observed_named_event(brep_flip_post, GED_EVENT_OBJECT_ADDED,
		brep_edit),
	    "brep flip command must not report in-place edit as add");
    CHECK(ged_event_observer_remove(gedp, brep_flip_post_token) == 1,
	    "brep flip observer removal must succeed");

    event_order_observer brep_shrink_post;
    ged_event_observer_token brep_shrink_post_token =
	ged_event_observer_add(gedp, GED_EVENT_OBSERVER_POST_RECONCILE,
		event_order_cb, &brep_shrink_post);
    CHECK(brep_shrink_post_token != 0,
	    "brep shrink observer must register");
    const char *brep_shrink_av[4] = {"brep", brep_edit,
	"shrink_surfaces", NULL};
    CHECK(ged_exec(gedp, 3, brep_shrink_av) == BRLCAD_OK,
	    "brep shrink_surfaces command must publish object-modified event");
    CHECK(brep_shrink_post.calls == 1,
	    "brep shrink_surfaces command must publish one event transaction");
    CHECK(observed_named_event(brep_shrink_post, GED_EVENT_OBJECT_MODIFIED,
		brep_edit),
	    "brep shrink_surfaces command must emit object-modified event");
    CHECK(!observed_named_event(brep_shrink_post, GED_EVENT_OBJECT_ADDED,
		brep_edit),
	    "brep shrink_surfaces command must not report in-place edit as add");
    CHECK(ged_event_observer_remove(gedp, brep_shrink_post_token) == 1,
	    "brep shrink observer removal must succeed");

    const char *brep_selection_name = "_ged_event_brep_sel";
    const char *brep_selection_append_av[12] = {"brep", brep_edit,
	"selection", "append", brep_selection_name, "0", "0", "0",
	"1", "0", "0", NULL};
    CHECK(ged_exec(gedp, 11, brep_selection_append_av) == BRLCAD_OK,
	    "brep selection fixture must append a selectable control vertex");
    struct rt_selection_set *brep_selection_set =
	ged_get_selection_set(gedp, brep_edit, brep_selection_name);
    CHECK(brep_selection_set != NULL &&
	    BU_PTBL_LEN(&brep_selection_set->selections) > 0,
	    "brep selection fixture must store a selection");
    event_order_observer brep_selection_post;
    ged_event_observer_token brep_selection_post_token =
	ged_event_observer_add(gedp, GED_EVENT_OBSERVER_POST_RECONCILE,
		event_order_cb, &brep_selection_post);
    CHECK(brep_selection_post_token != 0,
	    "brep selection observer must register");
    const char *brep_selection_translate_av[9] = {"brep", brep_edit,
	"selection", "translate", brep_selection_name, "0.1", "0", "0",
	NULL};
    CHECK(ged_exec(gedp, 8, brep_selection_translate_av) == BRLCAD_OK,
	    "brep selection translate must publish object-modified event");
    CHECK(brep_selection_post.calls == 1,
	    "brep selection translate must publish one event transaction");
    CHECK(observed_named_event(brep_selection_post,
		GED_EVENT_OBJECT_MODIFIED, brep_edit),
	    "brep selection translate must emit object-modified event");
    CHECK(!observed_named_event(brep_selection_post,
		GED_EVENT_OBJECT_ADDED, brep_edit),
	    "brep selection translate must not report in-place edit as add");
    CHECK(ged_event_observer_remove(gedp, brep_selection_post_token) == 1,
	    "brep selection observer removal must succeed");

    const char *brep_csg_solid = "_ged_event_brep_csg_source.s";
    const char *brep_csg_source = "_ged_event_brep_csg_source.brep";
    const char *brep_csg_group = "_ged_event_brep_csg_group.c";
    const char *brep_csg_top = "csg__ged_event_brep_csg_group.c";
    const char *brep_csg_make_av[4] = {"make", brep_csg_solid, "sph",
	NULL};
    CHECK(ged_exec(gedp, 3, brep_csg_make_av) == BRLCAD_OK,
	    "brep csg fixture must create source sphere");
    const char *brep_csg_convert_av[5] = {"brep", brep_csg_solid,
	"brep", brep_csg_source, NULL};
    CHECK(ged_exec(gedp, 4, brep_csg_convert_av) == BRLCAD_OK,
	    "brep csg fixture must convert sphere to named brep");
    const char *brep_csg_group_av[4] = {"g", brep_csg_group,
	brep_csg_source, NULL};
    CHECK(ged_exec(gedp, 3, brep_csg_group_av) == BRLCAD_OK,
	    "brep csg fixture must create source comb");
    event_order_observer brep_csg_post;
    ged_event_observer_token brep_csg_post_token =
	ged_event_observer_add(gedp, GED_EVENT_OBSERVER_POST_RECONCILE,
		event_order_cb, &brep_csg_post);
    CHECK(brep_csg_post_token != 0,
	    "brep csg observer must register");
    const char *brep_csg_av[4] = {"brep", brep_csg_group, "csg", NULL};
    CHECK(ged_exec(gedp, 3, brep_csg_av) == BRLCAD_OK,
	    "brep csg command must create CSG output");
    CHECK(observed_named_event(brep_csg_post, GED_EVENT_OBJECT_ADDED,
		brep_csg_top),
	    "brep csg top-level output must emit object-added event");
    CHECK(db_lookup(gedp->dbip, brep_csg_top, LOOKUP_QUIET) != RT_DIR_NULL,
	    "brep csg top-level output must be present");
    CHECK(ged_event_observer_remove(gedp, brep_csg_post_token) == 1,
	    "brep csg observer removal must succeed");

    const char *brep_csg_direct_solid = "_ged_event_brep_csg_direct.s";
    const char *brep_csg_direct = "_ged_event_brep_csg_direct.brep";
    const char *brep_csg_direct_top = "csg__ged_event_brep_csg_direct.c";
    const char *brep_csg_direct_make_av[4] = {"make",
	brep_csg_direct_solid, "rcc", NULL};
    CHECK(ged_exec(gedp, 3, brep_csg_direct_make_av) == BRLCAD_OK,
	    "brep direct csg fixture must create source cylinder");
    const char *brep_csg_direct_convert_av[5] = {"brep",
	brep_csg_direct_solid, "brep", brep_csg_direct, NULL};
    CHECK(ged_exec(gedp, 4, brep_csg_direct_convert_av) == BRLCAD_OK,
	    "brep direct csg fixture must convert cylinder to named brep");
    event_order_observer brep_csg_direct_post;
    ged_event_observer_token brep_csg_direct_post_token =
	ged_event_observer_add(gedp, GED_EVENT_OBSERVER_POST_RECONCILE,
		event_order_cb, &brep_csg_direct_post);
    CHECK(brep_csg_direct_post_token != 0,
	    "brep direct csg observer must register");
    const char *brep_csg_direct_av[4] = {"brep", brep_csg_direct, "csg",
	NULL};
    CHECK(ged_exec(gedp, 3, brep_csg_direct_av) == BRLCAD_OK,
	    "brep direct csg command must create CSG output");
    CHECK(brep_csg_direct_post.calls == 1,
	    "brep direct csg command must publish one event transaction");
    CHECK(observed_named_event(brep_csg_direct_post,
		GED_EVENT_OBJECT_ADDED, brep_csg_direct_top),
	    "brep direct csg top-level output must emit object-added event");
    CHECK(db_lookup(gedp->dbip, brep_csg_direct_top, LOOKUP_QUIET) !=
	    RT_DIR_NULL,
	    "brep direct csg top-level output must be present");
    CHECK(std::find(brep_csg_direct_post.all_kinds.begin(),
	    brep_csg_direct_post.all_kinds.end(), GED_EVENT_OBJECT_REMOVED) ==
	    brep_csg_direct_post.all_kinds.end(),
	    "brep direct csg fixture must not publish removals");
    CHECK(ged_event_observer_remove(gedp, brep_csg_direct_post_token) == 1,
	    "brep direct csg observer removal must succeed");

    const char *joint2_selection_name = "_ged_event_joint2_sel";
    const char *joint2_replace_av[12] = {"joint2", brep_csg_source,
	"selection", "replace", joint2_selection_name, "0", "0", "0",
	"1", "0", "0", NULL};
    CHECK(ged_exec(gedp, 11, joint2_replace_av) == BRLCAD_OK,
	    "joint2 selection fixture must create a selectable BREP control vertex");
    struct rt_selection_set *joint2_selection_set =
	ged_get_selection_set(gedp, brep_csg_source, joint2_selection_name);
    CHECK(joint2_selection_set != NULL &&
	    BU_PTBL_LEN(&joint2_selection_set->selections) > 0,
	    "joint2 selection fixture must store a selection");
    event_order_observer joint2_translate_post;
    ged_event_observer_token joint2_translate_post_token =
	ged_event_observer_add(gedp, GED_EVENT_OBSERVER_POST_RECONCILE,
		event_order_cb, &joint2_translate_post);
    CHECK(joint2_translate_post_token != 0,
	    "joint2 selection translate observer must register");
    const char *joint2_translate_av[9] = {"joint2", brep_csg_source,
	"selection", "translate", joint2_selection_name, "0.1", "0", "0",
	NULL};
    CHECK(ged_exec(gedp, 8, joint2_translate_av) == BRLCAD_OK,
	    "joint2 selection translate must publish object-modified event");
    CHECK(joint2_translate_post.calls == 1,
	    "joint2 selection translate must publish one event transaction");
    CHECK(observed_named_event(joint2_translate_post,
		GED_EVENT_OBJECT_MODIFIED, brep_csg_source),
	    "joint2 selection translate must emit object-modified event");
    CHECK(!observed_named_event(joint2_translate_post,
		GED_EVENT_OBJECT_ADDED, brep_csg_source),
	    "joint2 selection translate must not report in-place edit as add");
    CHECK(ged_event_observer_remove(gedp, joint2_translate_post_token) == 1,
	    "joint2 selection translate observer removal must succeed");

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

    const char *material_prop_obj = "_ged_event_material_prop";
    const char *material_create_av[5] = {"material", "create",
	material_prop_obj, "MaterialProp", NULL};
    CHECK(ged_exec(gedp, 4, material_create_av) == BRLCAD_OK,
	    "material property fixture must create material object");
    event_order_observer material_set_post;
    ged_event_observer_token material_set_post_token =
	ged_event_observer_add(gedp, GED_EVENT_OBSERVER_POST_RECONCILE,
		event_order_cb, &material_set_post);
    CHECK(material_set_post_token != 0,
	    "material set observer must register");
    const char *material_set_av[7] = {"material", "set", material_prop_obj,
	"physical", "density", "1.25", NULL};
    CHECK(ged_exec(gedp, 6, material_set_av) == BRLCAD_OK,
	    "material set command must publish material event");
    CHECK(material_set_post.calls == 1,
	    "material set command must publish one event transaction");
    CHECK(observed_named_event(material_set_post,
		GED_EVENT_MATERIAL_CHANGED, material_prop_obj),
	    "material set command must emit material-changed event");
    CHECK(std::find(material_set_post.all_kinds.begin(),
	    material_set_post.all_kinds.end(),
	    GED_EVENT_OBJECT_MODIFIED) == material_set_post.all_kinds.end(),
	    "material set semantic event must cover raw object fallback");
    CHECK(ged_event_observer_remove(gedp, material_set_post_token) == 1,
	    "material set observer removal must succeed");

    const char *mater_density_obj = "_DENSITIES";
    event_order_observer mater_density_set_post;
    ged_event_observer_token mater_density_set_post_token =
	ged_event_observer_add(gedp, GED_EVENT_OBSERVER_POST_RECONCILE,
		event_order_cb, &mater_density_set_post);
    CHECK(mater_density_set_post_token != 0,
	    "mater density set observer must register");
    uint64_t mater_density_set_revision_before =
	ged_draw_material_revision(gedp);
    const char *mater_density_set_av[5] = {"mater", "-d", "set",
	"42,1.0,ModernMapMaterial", NULL};
    CHECK(ged_exec(gedp, 4, mater_density_set_av) == BRLCAD_OK,
	    "mater density setup must create in-database density data");
    CHECK(mater_density_set_post.calls == 1,
	    "mater density set command must publish one event transaction");
    CHECK(observed_named_event(mater_density_set_post,
		GED_EVENT_OBJECT_ADDED, mater_density_obj),
	    "mater density set command must emit density object-added event");
    CHECK(observed_named_event(mater_density_set_post,
		GED_EVENT_OBJECT_VISIBILITY_CHANGED, mater_density_obj),
	    "mater density set command must emit density visibility event");
    CHECK(std::find(mater_density_set_post.all_kinds.begin(),
	    mater_density_set_post.all_kinds.end(),
	    GED_EVENT_MATERIAL_CHANGED) !=
	    mater_density_set_post.all_kinds.end(),
	    "mater density set command must emit global material-changed event");
    CHECK(ged_draw_material_revision(gedp) >
	    mater_density_set_revision_before,
	    "mater density set command must bump draw material revision");
    struct directory *mater_density_set_dp = db_lookup(gedp->dbip,
	    mater_density_obj, LOOKUP_QUIET);
    CHECK(mater_density_set_dp != RT_DIR_NULL &&
	    (mater_density_set_dp->d_flags & RT_DIR_HIDDEN),
	    "mater density set command must leave density object hidden");
    CHECK(ged_event_observer_remove(gedp,
		mater_density_set_post_token) == 1,
	    "mater density set observer removal must succeed");

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

    char mater_import_file[MAXPATHLEN] = {0};
    FILE *mater_import_fp = bu_temp_file(mater_import_file, MAXPATHLEN);
    CHECK(mater_import_fp != NULL,
	    "mater density import test must create temp input file");
    if (mater_import_fp) {
	std::fprintf(mater_import_fp, "77\t2500\tImportedDensity\n");
	std::fclose(mater_import_fp);
	event_order_observer mater_import_post;
	ged_event_observer_token mater_import_post_token =
	    ged_event_observer_add(gedp, GED_EVENT_OBSERVER_POST_RECONCILE,
		    event_order_cb, &mater_import_post);
	CHECK(mater_import_post_token != 0,
		"mater density import observer must register");
	const char *mater_import_av[5] = {"mater", "-d", "import",
	    mater_import_file, NULL};
	CHECK(ged_exec(gedp, 4, mater_import_av) == BRLCAD_OK,
		"mater density import command must publish density events");
	CHECK(mater_import_post.calls == 1,
		"mater density import command must publish one event transaction");
	CHECK(observed_named_event(mater_import_post,
		    GED_EVENT_OBJECT_REMOVED, mater_density_obj),
		"mater density import command must emit replaced density removal");
	CHECK(observed_named_event(mater_import_post,
		    GED_EVENT_OBJECT_ADDED, mater_density_obj),
		"mater density import command must emit density object-added event");
	CHECK(std::find(mater_import_post.all_kinds.begin(),
		mater_import_post.all_kinds.end(),
		GED_EVENT_MATERIAL_CHANGED) != mater_import_post.all_kinds.end(),
		"mater density import command must emit material-changed event");
	CHECK(ged_event_observer_remove(gedp, mater_import_post_token) == 1,
		"mater density import observer removal must succeed");
	bu_file_delete(mater_import_file);
    }

    event_order_observer mater_clear_post;
    ged_event_observer_token mater_clear_post_token =
	ged_event_observer_add(gedp, GED_EVENT_OBSERVER_POST_RECONCILE,
		event_order_cb, &mater_clear_post);
    CHECK(mater_clear_post_token != 0,
	    "mater density clear observer must register");
    const char *mater_clear_av[4] = {"mater", "-d", "clear", NULL};
    CHECK(ged_exec(gedp, 3, mater_clear_av) == BRLCAD_OK,
	    "mater density clear command must publish density removal event");
    CHECK(mater_clear_post.calls == 1,
	    "mater density clear command must publish one event transaction");
    CHECK(observed_named_event(mater_clear_post, GED_EVENT_OBJECT_REMOVED,
		mater_density_obj),
	    "mater density clear command must emit density object-removed event");
    CHECK(std::find(mater_clear_post.all_kinds.begin(),
	    mater_clear_post.all_kinds.end(),
	    GED_EVENT_MATERIAL_CHANGED) != mater_clear_post.all_kinds.end(),
	    "mater density clear command must emit material-changed event");
    CHECK(!observed_named_event(mater_clear_post, GED_EVENT_OBJECT_ADDED,
		mater_density_obj),
	    "mater density clear command must not emit density object add");
    CHECK(db_lookup(gedp->dbip, mater_density_obj, LOOKUP_QUIET) ==
	    RT_DIR_NULL,
	    "mater density clear command must remove density object");
    CHECK(ged_event_observer_remove(gedp, mater_clear_post_token) == 1,
	    "mater density clear observer removal must succeed");

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
    const char *concat_import_src = "_ged_concat_import_source.s";
    const char *concat_import_dst = "_ged_concat__ged_concat_import_source.s";
    struct ged *concat_src = ged_open("db", concat_src_g, 0);
    CHECK(concat_src != NULL,
	    "concat color-table source database must be created");
    if (concat_src) {
	const char *source_color_av[7] = {"color", "900", "901",
	    "11", "22", "33", NULL};
	CHECK(ged_exec(concat_src, 6, source_color_av) == BRLCAD_OK,
		"concat color-table source must define region color table");
	const char *source_in_av[8] = {"in", concat_import_src, "sph",
	    "0", "0", "0", "1", NULL};
	CHECK(ged_exec(concat_src, 7, source_in_av) == BRLCAD_OK,
		"concat source must define importable primitive");
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
    CHECK(observed_named_event(concat_post, GED_EVENT_OBJECT_ADDED,
		concat_import_dst),
	    "concat command must emit object-added event for imported objects");
    CHECK(concat_post.all_affected_names.find(DB5_GLOBAL_OBJECT_NAME) !=
	    std::string::npos,
	    "concat -c material event must identify _GLOBAL metadata");
    CHECK(ged_event_observer_remove(gedp, concat_post_token) == 1,
	    "concat color-table observer removal must succeed");
    bu_file_delete(concat_src_g);

    const char *concat_overwrite_name = "_ged_concat_overwrite.s";
    const char *concat_overwrite_target_av[8] = {"in", concat_overwrite_name,
	"sph", "0", "0", "0", "1", NULL};
    CHECK(ged_exec(gedp, 7, concat_overwrite_target_av) == BRLCAD_OK,
	    "concat overwrite target must be created");
    char concat_overwrite_src_g[MAXPATHLEN] = {0};
    bu_dir(concat_overwrite_src_g, MAXPATHLEN, BU_DIR_CURR,
	    "ged_state_services_concat_overwrite_source.g", NULL);
    bu_file_delete(concat_overwrite_src_g);
    struct ged *concat_overwrite_src = ged_open("db", concat_overwrite_src_g,
	    0);
    CHECK(concat_overwrite_src != NULL,
	    "concat overwrite source database must be created");
    if (concat_overwrite_src) {
	const char *overwrite_source_in_av[8] = {"in", concat_overwrite_name,
	    "sph", "1", "0", "0", "2", NULL};
	CHECK(ged_exec(concat_overwrite_src, 7, overwrite_source_in_av) ==
		BRLCAD_OK,
		"concat overwrite source must define replacement primitive");
	db_sync(concat_overwrite_src->dbip);
	ged_close(concat_overwrite_src);
    }
    event_order_observer concat_overwrite_post;
    ged_event_observer_token concat_overwrite_post_token =
	ged_event_observer_add(gedp, GED_EVENT_OBSERVER_POST_RECONCILE,
		event_order_cb, &concat_overwrite_post);
    CHECK(concat_overwrite_post_token != 0,
	    "concat overwrite observer must register");
    const char *concat_overwrite_av[4] = {"concat", "-O",
	concat_overwrite_src_g, NULL};
    CHECK(concat_overwrite_src &&
	    ged_exec(gedp, 3, concat_overwrite_av) == BRLCAD_OK,
	    "concat -O command must publish final modified event");
    CHECK(concat_overwrite_post.calls == 1,
	    "concat -O command must publish one event transaction");
    CHECK(observed_named_event(concat_overwrite_post,
		GED_EVENT_OBJECT_MODIFIED, concat_overwrite_name),
	    "concat -O command must emit object-modified event for overwritten object");
    CHECK(!observed_named_event(concat_overwrite_post,
		GED_EVENT_OBJECT_ADDED, concat_overwrite_name),
	    "concat -O overwrite must not expose final object as an add");
    CHECK(std::find(concat_overwrite_post.all_kinds.begin(),
	    concat_overwrite_post.all_kinds.end(),
	    GED_EVENT_OBJECT_REMOVED) == concat_overwrite_post.all_kinds.end(),
	    "concat -O overwrite must hide temporary backup removal");
    CHECK(concat_overwrite_post.all_affected_names.find(".bak") ==
	    std::string::npos,
	    "concat -O overwrite must not expose temporary backup names");
    CHECK(ged_event_observer_remove(gedp, concat_overwrite_post_token) == 1,
	    "concat overwrite observer removal must succeed");
    bu_file_delete(concat_overwrite_src_g);

    struct rt_wdb *wdbp = wdb_dbopen(gedp->dbip, RT_WDB_TYPE_DB_DEFAULT);
    CHECK(wdbp != RT_WDB_NULL,
	    "comb command event fixture must create writer");
    const char *comb_event_child = "_ged_event_comb_child.s";
    const char *group_event_dst = "_ged_event_group.c";
    const char *comb_event_region = "_ged_event_comb_region.c";
    const char *comb_event_region_b = "_ged_event_comb_region_b.c";
    const char *comb_std_event_dst = "_ged_event_comb_std.c";
    const char *adjust_event_comb = "_ged_event_adjust_comb.c";
    const char *adjust_event_prim = "_ged_event_adjust_prim.s";
    const char *put_comb_event_comb = "_ged_event_put_comb.c";
    const char *put_event_dst = "_ged_event_put.s";
    const char *in_direct_event_dst = "_ged_event_in_direct.s";
    const char *in_common_event_dst = "_ged_event_in_common.s";
    const char *make_event_dst = "_ged_event_make.s";
    const char *metaball_event_dst = "_ged_event_metaball.s";
    const char *bo_event_dst = "_ged_event_bo.bin";
    const char *threeptarb_event_dst = "_ged_event_3ptarb.s";
    const char *cc_event_dst = "_ged_event_constraint";
    const char *inside_event_src = "_ged_event_inside_outer.s";
    const char *inside_event_dst = "_ged_event_inside_inner.s";
    const char *heal_event_bot = "_ged_event_heal.bot";
    const char *matrix_event_child = "_ged_event_matrix_child.s";
    const char *matrix_event_source = "_ged_event_matrix_source.c";
    const char *matrix_event_dest = "_ged_event_matrix_dest.c";
    const char *combmem_event_child_a = "_ged_event_combmem_child_a.s";
    const char *combmem_event_child_b = "_ged_event_combmem_child_b.s";
    const char *combmem_event_comb = "_ged_event_combmem.c";
    const char *edit_perturb_event_sph = "_ged_event_edit_perturb.s";
    const char *pipe_event_obj = "_ged_event_pipe.s";
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
    const char *bev_event_child = "_ged_event_bev_child.s";
    const char *bev_event_region = "_ged_event_bev_region.r";
    const char *bev_event_dst = "_ged_event_bev.nmg";
    const char *polyclip_event_sketch = "_ged_event_polyclip.sketch";
    const char *analyze_pnts_src = "_ged_event_analyze_src.pnts";
    const char *analyze_vol_src = "_ged_event_analyze_vol.s";
    const char *analyze_pnts_out = "_ged_event_analyze_out.pnts";
    const char *pnts_read_basic_obj = "_ged_event_pnts_read_basic.pnts";
    const char *pnts_read_full_obj = "_ged_event_pnts_read_full.pnts";
    const char *bb_event_dst = "_ged_event_bb_bbox.s";
    const char *rfarb_event_dst = "_ged_event_rfarb.s";
    const char *arb_create_event_dst = "_ged_event_arb_create.s";
    const char *track_event_group = "_ged_event_track";
    const char *coil_event_group = "_ged_event_coil";
    const char *human_event_top = "_ged_event_human.r";
    const char *voxelize_event_child = "_ged_event_voxelize_child.s";
    const char *voxelize_event_region = "_ged_event_voxelize_region.r";
    const char *voxelize_event_dst = "_ged_event_voxelize.c";
    const char *voxelize_event_box = "_ged_event_voxelize.c.x0y0z0.s";
    const char *arb_repair_event_dst = "_ged_event_arb_repair.s";
    const char *bot_split_event_src = "_ged_event_bot_split.bot";
    const char *bot_pca_event_dst = "_ged_event_bot_pca.bot";
    const char *bot_condense_event_dst = "_ged_event_bot_condense.bot";
    const char *bot_vertex_fuse_event_dst =
	"_ged_event_bot_vertex_fuse.bot";
    const char *bot_face_fuse_event_dst = "_ged_event_bot_face_fuse.bot";
    const char *bot_smooth_event_src = "_ged_event_bot_smooth_src.bot";
    const char *bot_smooth_event_dst = "_ged_event_bot_smooth.bot";
    const char *bot_cpp_event_src = "_ged_event_bot_cpp_src.bot";
    const char *bot_chull_event_dst = "_ged_event_bot_chull.bot";
    const char *bot_decimate_cpp_event_dst =
	"_ged_event_bot_decimate_cpp.bot";
    const char *bot_subd_event_dst = "_ged_event_bot_subd.bot";
    const char *bot_smooth_cpp_event_dst =
	"_ged_event_bot_smooth_cpp.bot";
    const char *bot_repair_cpp_event_src =
	"_ged_event_bot_repair_cpp_src.bot";
    const char *bot_repair_cpp_event_dst =
	"_ged_event_bot_repair_cpp.bot";
    const char *bot_remesh_event_src = "_ged_event_bot_remesh_src.bot";
    const char *bot_remesh_event_dst = "_ged_event_bot_remesh.bot";
    const char *bot_extrude_event_src =
	"_ged_event_bot_extrude_src.bot";
    const char *bot_extrude_event_dst = "_ged_event_bot_extrude.c";
    const char *bot_exterior_event_src =
	"_ged_event_bot_exterior_src.bot";
    const char *bot_exterior_event_dst =
	"_ged_event_bot_exterior.bot";
    const char *bot_decimate_event_dst = "_ged_event_bot_decimate.bot";
    const char *bot_merge_event_dst = "_ged_event_bot_merge.bot";
    const char *bot_split_event_dst_a = "_ged_event_bot_split.bot.0";
    const char *bot_split_event_dst_b = "_ged_event_bot_split.bot.1";
    const char *lint_bad_bot = "_ged_event_lint_bad.bot";
    const char *lint_group = "_ged_event_lint_group.c";
    const char *shells_event_nmg = "_ged_event_shells.nmg";
    const char *decompose_event_nmg = "_ged_event_decompose.nmg";
    const char *fracture_event_nmg = "_ged_event_fracture.nmg";
    const char *nmg_collapse_event_src = "_ged_event_nmg_collapse.nmg";
    const char *nmg_collapse_event_dst = "_ged_event_nmg_collapse_out.nmg";
    const char *nmg_mm_event_dst = "_ged_event_nmg_mm.nmg";
    const char *nmg_inplace_event_obj = "_ged_event_nmg_inplace.nmg";
    const char *nmg_simplify_event_src = "_ged_event_nmg_simplify.nmg";
    const char *nmg_simplify_event_dst =
	"_ged_event_nmg_simplify_out.s";
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
	point_t inside_center = {347.0, 0.0, 0.0};
	CHECK(mk_sph(wdbp, inside_event_src, inside_center, 2.0) == 0,
		"inside command event fixture primitive must be created");

	point_t voxelize_center = {347.0, 8.0, 0.0};
	CHECK(mk_sph(wdbp, voxelize_event_child, voxelize_center, 1.0) == 0,
		"voxelize command event fixture child must be created");
	struct wmember voxelize_wm;
	BU_LIST_INIT(&voxelize_wm.l);
	CHECK(mk_addmember(voxelize_event_child, &voxelize_wm.l, NULL,
		    WMOP_UNION) != NULL,
		"voxelize command event fixture must add child");
	CHECK(mk_comb(wdbp, voxelize_event_region, &voxelize_wm.l, 1,
		    NULL, NULL, NULL, 0, 0, 0, 0, 0, 0, 0) == 0,
		"voxelize command event fixture region must be created");

	point_t analyze_vol_center = {348.0, 0.0, 0.0};
	CHECK(mk_sph(wdbp, analyze_vol_src, analyze_vol_center, 2.0) == 0,
		"analyze PNTS fixture volume must be created");
	fastf_t analyze_pnts_vertices[6] = {
	    348.0, 0.0, 0.0,
	    360.0, 0.0, 0.0
	};
	CHECK(mk_pnts(wdbp, analyze_pnts_src, RT_PNT_TYPE_PNT, 1.0, 2,
		    analyze_pnts_vertices, NULL, NULL, NULL) == 0,
		"analyze PNTS fixture point cloud must be created");

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

	point_t combmem_a_center = {352.0, 0.0, 0.0};
	point_t combmem_b_center = {353.0, 0.0, 0.0};
	CHECK(mk_sph(wdbp, combmem_event_child_a, combmem_a_center, 1.0)
		== 0,
		"combmem command event fixture first child must be created");
	CHECK(mk_sph(wdbp, combmem_event_child_b, combmem_b_center, 1.0)
		== 0,
		"combmem command event fixture second child must be created");
	struct wmember combmem_wm;
	BU_LIST_INIT(&combmem_wm.l);
	CHECK(mk_addmember(combmem_event_child_a, &combmem_wm.l, NULL,
		    WMOP_UNION) != NULL,
		"combmem command event fixture must add first child");
	CHECK(mk_comb(wdbp, combmem_event_comb, &combmem_wm.l, 0,
		    NULL, NULL, NULL, 0, 0, 0, 0, 0, 0, 0) == 0,
		"combmem command event fixture comb must be created");

	point_t edit_perturb_center = {354.0, 0.0, 0.0};
	CHECK(mk_sph(wdbp, edit_perturb_event_sph, edit_perturb_center, 1.0)
		== 0,
		"edit perturb command event fixture primitive must be created");

	struct bu_list pipe_head;
	point_t pipe_p0 = {354.0, 2.0, 0.0};
	point_t pipe_p1 = {355.0, 2.0, 0.0};
	point_t pipe_p2 = {356.0, 2.0, 0.0};
	mk_pipe_init(&pipe_head);
	mk_add_pipe_pnt(&pipe_head, pipe_p0, 0.5, 0.0, 0.5);
	mk_add_pipe_pnt(&pipe_head, pipe_p1, 0.5, 0.0, 0.5);
	mk_add_pipe_pnt(&pipe_head, pipe_p2, 0.5, 0.0, 0.5);
	CHECK(mk_pipe(wdbp, pipe_event_obj, &pipe_head) == 0,
		"pipe command event fixture primitive must be created");
	mk_pipe_free(&pipe_head);

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

	point_t bev_child_center = {418.0, 0.0, 0.0};
	CHECK(mk_sph(wdbp, bev_event_child, bev_child_center, 1.0) == 0,
		"bev command event fixture child must be created");
	struct wmember bev_wm;
	BU_LIST_INIT(&bev_wm.l);
	CHECK(mk_addmember(bev_event_child, &bev_wm.l, NULL,
		    WMOP_UNION) != NULL,
		"bev command event fixture must add child");
	CHECK(mk_comb(wdbp, bev_event_region, &bev_wm.l, 1, NULL, NULL,
		    NULL, 0, 0, 0, 0, 0, 0, 0) == 0,
		"bev command event fixture region must be created");

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

	CHECK(mk_bot(wdbp, bot_smooth_event_src, RT_BOT_SOLID,
		    RT_BOT_CCW, 0, 4, 4, heal_vertices, heal_faces, NULL,
		    NULL) == 0,
		"bot_smooth command event fixture BoT must be created");
	CHECK(mk_bot(wdbp, bot_cpp_event_src, RT_BOT_SOLID,
		    RT_BOT_CCW, 0, 4, 4, heal_vertices, heal_faces, NULL,
		    NULL) == 0,
		"C++ bot subcommand event fixture BoT must be created");
	CHECK(mk_bot(wdbp, bot_remesh_event_src, RT_BOT_SOLID,
		    RT_BOT_CCW, 0, 4, 4, heal_vertices, heal_faces, NULL,
		    NULL) == 0,
		"bot remesh command event fixture BoT must be created");
	CHECK(mk_bot(wdbp, bot_exterior_event_src, RT_BOT_SOLID,
		    RT_BOT_CCW, 0, 4, 4, heal_vertices, heal_faces, NULL,
		    NULL) == 0,
		"bot exterior command event fixture BoT must be created");

	int bot_repair_faces[9] = {
	    0, 2, 1,
	    0, 3, 2,
	    0, 1, 3
	};
	CHECK(mk_bot(wdbp, bot_repair_cpp_event_src, RT_BOT_SOLID,
		    RT_BOT_CCW, 0, 4, 3, heal_vertices, bot_repair_faces,
		    NULL, NULL) == 0,
		"bot repair command event fixture BoT must be created");

	fastf_t bot_extrude_vertices[9] = {
	    0.0, 0.0, 0.0,
	    1.0, 0.0, 0.0,
	    0.0, 1.0, 0.0
	};
	int bot_extrude_faces[3] = {0, 1, 2};
	fastf_t bot_extrude_thickness[1] = {0.2};
	CHECK(mk_bot(wdbp, bot_extrude_event_src, RT_BOT_PLATE,
		    RT_BOT_CCW, 0, 3, 1, bot_extrude_vertices,
		    bot_extrude_faces, bot_extrude_thickness, NULL) == 0,
		"bot extrude command event fixture BoT must be created");

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

	fastf_t lint_bad_vertices[9] = {
	    0.0, 0.0, 0.0,
	    1.0, 0.0, 0.0,
	    0.0, 1.0, 0.0
	};
	int lint_bad_faces[3] = {0, 1, 2};
	CHECK(mk_bot(wdbp, lint_bad_bot, RT_BOT_SOLID, RT_BOT_CCW, 0,
		    3, 1, lint_bad_vertices, lint_bad_faces, NULL, NULL) == 0,
		"lint diagnostic comb fixture BoT must be created");

	CHECK(make_event_nmg_tet(wdbp, fracture_event_nmg) == 0,
		"fracture command event fixture NMG must be created");
	CHECK(make_event_nmg_tet(wdbp, shells_event_nmg) == 0,
		"shells command event fixture NMG must be created");
	CHECK(make_event_nmg_tet(wdbp, decompose_event_nmg) == 0,
		"decompose command event fixture NMG must be created");
	CHECK(make_event_nmg_tet(wdbp, nmg_collapse_event_src) == 0,
		"nmg_collapse command event fixture NMG must be created");
	CHECK(make_event_nmg_box(wdbp, nmg_simplify_event_src) == 0,
		"nmg_simplify command event fixture NMG must be created");
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

    event_order_observer comb_std_post;
    ged_event_observer_token comb_std_post_token =
	ged_event_observer_add(gedp, GED_EVENT_OBSERVER_POST_RECONCILE,
		event_order_cb, &comb_std_post);
    CHECK(comb_std_post_token != 0,
	    "c command observer must register");
    const char *comb_std_av[4] = {"c", comb_std_event_dst,
	comb_event_child, NULL};
    CHECK(ged_exec(gedp, 3, comb_std_av) == BRLCAD_OK,
	    "c command must publish object creation event");
    CHECK(comb_std_post.calls == 1,
	    "c command must publish one event transaction");
    CHECK(observed_named_event(comb_std_post, GED_EVENT_OBJECT_ADDED,
		comb_std_event_dst),
	    "c command must emit object-added event for created comb");
    CHECK(std::find(comb_std_post.all_kinds.begin(),
	    comb_std_post.all_kinds.end(), GED_EVENT_OBJECT_REMOVED) ==
	    comb_std_post.all_kinds.end(),
	    "c command creation fixture must not publish removals");
    CHECK(ged_event_observer_remove(gedp, comb_std_post_token) == 1,
	    "c command observer removal must succeed");

    event_order_observer comb_std_region_post;
    ged_event_observer_token comb_std_region_post_token =
	ged_event_observer_add(gedp, GED_EVENT_OBSERVER_POST_RECONCILE,
		event_order_cb, &comb_std_region_post);
    CHECK(comb_std_region_post_token != 0,
	    "c -r command observer must register");
    const char *comb_std_region_av[4] = {"c", "-r", comb_std_event_dst,
	NULL};
    CHECK(ged_exec(gedp, 3, comb_std_region_av) == BRLCAD_OK,
	    "c -r command must publish region flag event");
    CHECK(comb_std_region_post.calls == 1,
	    "c -r command must publish one event transaction");
    CHECK(observed_named_event(comb_std_region_post,
		GED_EVENT_ATTRIBUTE_CHANGED, comb_std_event_dst),
	    "c -r command must emit attribute-changed event");
    CHECK(!observed_named_event(comb_std_region_post,
		GED_EVENT_OBJECT_MODIFIED, comb_std_event_dst),
	    "c -r semantic event must cover raw object fallback");
    CHECK(ged_event_observer_remove(gedp, comb_std_region_post_token) == 1,
	    "c -r command observer removal must succeed");

    event_order_observer combmem_post;
    ged_event_observer_token combmem_post_token =
	ged_event_observer_add(gedp, GED_EVENT_OBSERVER_POST_RECONCILE,
		event_order_cb, &combmem_post);
    CHECK(combmem_post_token != 0,
	    "combmem observer must register");
    const char *combmem_av[10] = {"combmem", "-r", "5",
	combmem_event_comb, "u", combmem_event_child_b, "1", "2", "3",
	NULL};
    CHECK(ged_exec(gedp, 9, combmem_av) == BRLCAD_OK,
	    "combmem command must publish comb-tree event");
    CHECK(combmem_post.calls == 1,
	    "combmem command must publish one event transaction");
    CHECK(combmem_post.coalesced_count == 1,
	    "combmem command must coalesce raw fallback with semantic event");
    CHECK(observed_named_event(combmem_post,
		GED_EVENT_COMB_TREE_CHANGED, combmem_event_comb),
	    "combmem command must emit comb-tree event for edited comb");
    CHECK(!observed_named_event(combmem_post,
		GED_EVENT_OBJECT_MODIFIED, combmem_event_comb),
	    "combmem semantic event must cover raw object fallback");
    CHECK(!combmem_post.redraws.empty() && combmem_post.redraws[0] == 1,
	    "combmem semantic event must request redraw");
    CHECK(combmem_post.all_affected_names.find(combmem_event_comb) !=
	    std::string::npos,
	    "combmem command must identify affected comb");
    CHECK(ged_event_observer_remove(gedp, combmem_post_token) == 1,
	    "combmem observer removal must succeed");

    event_order_observer edit_perturb_post;
    ged_event_observer_token edit_perturb_post_token =
	ged_event_observer_add(gedp, GED_EVENT_OBSERVER_POST_RECONCILE,
		event_order_cb, &edit_perturb_post);
    CHECK(edit_perturb_post_token != 0,
	    "edit perturb observer must register");
    const char *edit_perturb_av[5] = {"edit", edit_perturb_event_sph,
	"perturb", "0.1", NULL};
    CHECK(ged_exec(gedp, 4, edit_perturb_av) == BRLCAD_OK,
	    "edit perturb command must publish object-modified event");
    CHECK(edit_perturb_post.calls == 1,
	    "edit perturb command must publish one event transaction");
    CHECK(observed_named_event(edit_perturb_post,
		GED_EVENT_OBJECT_MODIFIED, edit_perturb_event_sph),
	    "edit perturb command must emit object-modified event");
    CHECK(!observed_named_event(edit_perturb_post,
		GED_EVENT_OBJECT_ADDED, edit_perturb_event_sph),
	    "edit perturb command must not expose delete/re-add as object add");
    CHECK(std::find(edit_perturb_post.all_kinds.begin(),
	    edit_perturb_post.all_kinds.end(), GED_EVENT_OBJECT_REMOVED) ==
	    edit_perturb_post.all_kinds.end(),
	    "edit perturb command must hide temporary delete/re-add removal");
    CHECK(ged_event_observer_remove(gedp, edit_perturb_post_token) == 1,
	    "edit perturb observer removal must succeed");

    event_order_observer pipe_delete_post;
    ged_event_observer_token pipe_delete_post_token =
	ged_event_observer_add(gedp, GED_EVENT_OBSERVER_POST_RECONCILE,
		event_order_cb, &pipe_delete_post);
    CHECK(pipe_delete_post_token != 0,
	    "pipe_delete_pnt observer must register");
    const char *pipe_delete_av[4] = {"pipe_delete_pnt", pipe_event_obj,
	"1", NULL};
    CHECK(ged_exec(gedp, 3, pipe_delete_av) == BRLCAD_OK,
	    "pipe_delete_pnt command must publish object-modified event");
    CHECK(pipe_delete_post.calls == 1,
	    "pipe_delete_pnt command must publish one event transaction");
    CHECK(observed_named_event(pipe_delete_post,
		GED_EVENT_OBJECT_MODIFIED, pipe_event_obj),
	    "pipe_delete_pnt command must emit object-modified event");
    CHECK(!observed_named_event(pipe_delete_post,
		GED_EVENT_OBJECT_ADDED, pipe_event_obj),
	    "pipe_delete_pnt command must not report an in-place edit as an add");
    CHECK(std::find(pipe_delete_post.all_kinds.begin(),
	    pipe_delete_post.all_kinds.end(), GED_EVENT_OBJECT_REMOVED) ==
	    pipe_delete_post.all_kinds.end(),
	    "pipe_delete_pnt command must not publish removals");
    CHECK(ged_event_observer_remove(gedp, pipe_delete_post_token) == 1,
	    "pipe_delete_pnt observer removal must succeed");

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

    event_order_observer make_post;
    ged_event_observer_token make_post_token =
	ged_event_observer_add(gedp, GED_EVENT_OBSERVER_POST_RECONCILE,
		event_order_cb, &make_post);
    CHECK(make_post_token != 0,
	    "make observer must register");
    const char *make_av[4] = {"make", make_event_dst, "sph", NULL};
    CHECK(ged_exec(gedp, 3, make_av) == BRLCAD_OK,
	    "make command must publish object creation event");
    CHECK(make_post.calls == 1,
	    "make command must publish one event transaction");
    CHECK(observed_named_event(make_post, GED_EVENT_OBJECT_ADDED,
		make_event_dst),
	    "make command must emit object-added event for created primitive");
    CHECK(std::find(make_post.all_kinds.begin(),
	    make_post.all_kinds.end(), GED_EVENT_OBJECT_REMOVED) ==
	    make_post.all_kinds.end(),
	    "make creation fixture must not publish removals");
    CHECK(ged_event_observer_remove(gedp, make_post_token) == 1,
	    "make observer removal must succeed");

    event_order_observer pscale_post;
    ged_event_observer_token pscale_post_token =
	ged_event_observer_add(gedp, GED_EVENT_OBSERVER_POST_RECONCILE,
		event_order_cb, &pscale_post);
    CHECK(pscale_post_token != 0,
	    "pscale observer must register");
    const char *pscale_av[5] = {"pscale", make_event_dst, "A", "2",
	NULL};
    CHECK(ged_exec(gedp, 4, pscale_av) == BRLCAD_OK,
	    "pscale command must publish object-modified event");
    CHECK(pscale_post.calls == 1,
	    "pscale command must publish one event transaction");
    CHECK(observed_named_event(pscale_post, GED_EVENT_OBJECT_MODIFIED,
		make_event_dst),
	    "pscale command must emit object-modified event");
    CHECK(!observed_named_event(pscale_post, GED_EVENT_OBJECT_ADDED,
		make_event_dst),
	    "pscale command must not report an in-place edit as an add");
    CHECK(ged_event_observer_remove(gedp, pscale_post_token) == 1,
	    "pscale observer removal must succeed");

    event_order_observer otranslate_post;
    ged_event_observer_token otranslate_post_token =
	ged_event_observer_add(gedp, GED_EVENT_OBSERVER_POST_RECONCILE,
		event_order_cb, &otranslate_post);
    CHECK(otranslate_post_token != 0,
	    "otranslate observer must register");
    const char *otranslate_av[6] = {"otranslate", make_event_dst, "1",
	"0", "0", NULL};
    CHECK(ged_exec(gedp, 5, otranslate_av) == BRLCAD_OK,
	    "otranslate command must publish object-modified event");
    CHECK(otranslate_post.calls == 1,
	    "otranslate command must publish one event transaction");
    CHECK(observed_named_event(otranslate_post, GED_EVENT_OBJECT_MODIFIED,
		make_event_dst),
	    "otranslate command must emit object-modified event");
    CHECK(!observed_named_event(otranslate_post, GED_EVENT_OBJECT_ADDED,
		make_event_dst),
	    "otranslate command must not report an in-place edit as an add");
    CHECK(ged_event_observer_remove(gedp, otranslate_post_token) == 1,
	    "otranslate observer removal must succeed");

    event_order_observer oscale_post;
    ged_event_observer_token oscale_post_token =
	ged_event_observer_add(gedp, GED_EVENT_OBSERVER_POST_RECONCILE,
		event_order_cb, &oscale_post);
    CHECK(oscale_post_token != 0,
	    "oscale observer must register");
    const char *oscale_av[4] = {"oscale", make_event_dst, "1.1", NULL};
    CHECK(ged_exec(gedp, 3, oscale_av) == BRLCAD_OK,
	    "oscale command must publish object-modified event");
    CHECK(oscale_post.calls == 1,
	    "oscale command must publish one event transaction");
    CHECK(observed_named_event(oscale_post, GED_EVENT_OBJECT_MODIFIED,
		make_event_dst),
	    "oscale command must emit object-modified event");
    CHECK(!observed_named_event(oscale_post, GED_EVENT_OBJECT_ADDED,
		make_event_dst),
	    "oscale command must not report an in-place edit as an add");
    CHECK(ged_event_observer_remove(gedp, oscale_post_token) == 1,
	    "oscale observer removal must succeed");

    event_order_observer orotate_post;
    ged_event_observer_token orotate_post_token =
	ged_event_observer_add(gedp, GED_EVENT_OBSERVER_POST_RECONCILE,
		event_order_cb, &orotate_post);
    CHECK(orotate_post_token != 0,
	    "orotate observer must register");
    const char *orotate_av[6] = {"orotate", make_event_dst, "0", "0",
	"1", NULL};
    CHECK(ged_exec(gedp, 5, orotate_av) == BRLCAD_OK,
	    "orotate command must publish object-modified event");
    CHECK(orotate_post.calls == 1,
	    "orotate command must publish one event transaction");
    CHECK(observed_named_event(orotate_post, GED_EVENT_OBJECT_MODIFIED,
		make_event_dst),
	    "orotate command must emit object-modified event");
    CHECK(!observed_named_event(orotate_post, GED_EVENT_OBJECT_ADDED,
		make_event_dst),
	    "orotate command must not report an in-place edit as an add");
    CHECK(ged_event_observer_remove(gedp, orotate_post_token) == 1,
	    "orotate observer removal must succeed");

    event_order_observer ocenter_post;
    ged_event_observer_token ocenter_post_token =
	ged_event_observer_add(gedp, GED_EVENT_OBSERVER_POST_RECONCILE,
		event_order_cb, &ocenter_post);
    CHECK(ocenter_post_token != 0,
	    "ocenter observer must register");
    const char *ocenter_av[6] = {"ocenter", make_event_dst, "0", "0",
	"0", NULL};
    CHECK(ged_exec(gedp, 5, ocenter_av) == BRLCAD_OK,
	    "ocenter command must publish object-modified event");
    CHECK(ocenter_post.calls == 1,
	    "ocenter command must publish one event transaction");
    CHECK(observed_named_event(ocenter_post, GED_EVENT_OBJECT_MODIFIED,
		make_event_dst),
	    "ocenter command must emit object-modified event");
    CHECK(!observed_named_event(ocenter_post, GED_EVENT_OBJECT_ADDED,
		make_event_dst),
	    "ocenter command must not report an in-place edit as an add");
    CHECK(ged_event_observer_remove(gedp, ocenter_post_token) == 1,
	    "ocenter observer removal must succeed");

    const char *metaball_make_av[4] = {"make", metaball_event_dst,
	"metaball", NULL};
    CHECK(ged_exec(gedp, 3, metaball_make_av) == BRLCAD_OK,
	    "metaball command event fixture must be created");

    event_order_observer pset_post;
    ged_event_observer_token pset_post_token =
	ged_event_observer_add(gedp, GED_EVENT_OBSERVER_POST_RECONCILE,
		event_order_cb, &pset_post);
    CHECK(pset_post_token != 0,
	    "pset observer must register");
    const char *pset_av[5] = {"pset", metaball_event_dst, "t", "2",
	NULL};
    CHECK(ged_exec(gedp, 4, pset_av) == BRLCAD_OK,
	    "pset command must publish object-modified event");
    CHECK(pset_post.calls == 1,
	    "pset command must publish one event transaction");
    CHECK(observed_named_event(pset_post, GED_EVENT_OBJECT_MODIFIED,
		metaball_event_dst),
	    "pset command must emit object-modified event");
    CHECK(!observed_named_event(pset_post, GED_EVENT_OBJECT_ADDED,
		metaball_event_dst),
	    "pset command must not report an in-place edit as an add");
    CHECK(ged_event_observer_remove(gedp, pset_post_token) == 1,
	    "pset observer removal must succeed");

    event_order_observer metaball_add_post;
    ged_event_observer_token metaball_add_post_token =
	ged_event_observer_add(gedp, GED_EVENT_OBSERVER_POST_RECONCILE,
		event_order_cb, &metaball_add_post);
    CHECK(metaball_add_post_token != 0,
	    "mouse_add_metaball_pnt observer must register");
    const char *metaball_add_av[4] = {"mouse_add_metaball_pnt",
	metaball_event_dst, "0 0 0", NULL};
    CHECK(ged_exec(gedp, 3, metaball_add_av) == BRLCAD_OK,
	    "mouse_add_metaball_pnt command must publish object-modified event");
    CHECK(metaball_add_post.calls == 1,
	    "mouse_add_metaball_pnt command must publish one event transaction");
    CHECK(observed_named_event(metaball_add_post,
		GED_EVENT_OBJECT_MODIFIED, metaball_event_dst),
	    "mouse_add_metaball_pnt command must emit object-modified event");
    CHECK(!observed_named_event(metaball_add_post,
		GED_EVENT_OBJECT_ADDED, metaball_event_dst),
	    "mouse_add_metaball_pnt command must not report an in-place edit as an add");
    CHECK(ged_event_observer_remove(gedp, metaball_add_post_token) == 1,
	    "mouse_add_metaball_pnt observer removal must succeed");

    event_order_observer metaball_move_post;
    ged_event_observer_token metaball_move_post_token =
	ged_event_observer_add(gedp, GED_EVENT_OBSERVER_POST_RECONCILE,
		event_order_cb, &metaball_move_post);
    CHECK(metaball_move_post_token != 0,
	    "metaball_move_pnt observer must register");
    const char *metaball_move_av[5] = {"metaball_move_pnt",
	metaball_event_dst, "0", "0 0 1", NULL};
    CHECK(ged_exec(gedp, 4, metaball_move_av) == BRLCAD_OK,
	    "metaball_move_pnt command must publish object-modified event");
    CHECK(metaball_move_post.calls == 1,
	    "metaball_move_pnt command must publish one event transaction");
    CHECK(observed_named_event(metaball_move_post,
		GED_EVENT_OBJECT_MODIFIED, metaball_event_dst),
	    "metaball_move_pnt command must emit object-modified event");
    CHECK(!observed_named_event(metaball_move_post,
		GED_EVENT_OBJECT_ADDED, metaball_event_dst),
	    "metaball_move_pnt command must not report an in-place edit as an add");
    CHECK(ged_event_observer_remove(gedp, metaball_move_post_token) == 1,
	    "metaball_move_pnt observer removal must succeed");

    event_order_observer metaball_delete_post;
    ged_event_observer_token metaball_delete_post_token =
	ged_event_observer_add(gedp, GED_EVENT_OBSERVER_POST_RECONCILE,
		event_order_cb, &metaball_delete_post);
    CHECK(metaball_delete_post_token != 0,
	    "metaball_delete_pnt observer must register");
    const char *metaball_delete_av[4] = {"metaball_delete_pnt",
	metaball_event_dst, "2", NULL};
    CHECK(ged_exec(gedp, 3, metaball_delete_av) == BRLCAD_OK,
	    "metaball_delete_pnt command must publish object-modified event");
    CHECK(metaball_delete_post.calls == 1,
	    "metaball_delete_pnt command must publish one event transaction");
    CHECK(observed_named_event(metaball_delete_post,
		GED_EVENT_OBJECT_MODIFIED, metaball_event_dst),
	    "metaball_delete_pnt command must emit object-modified event");
    CHECK(!observed_named_event(metaball_delete_post,
		GED_EVENT_OBJECT_ADDED, metaball_event_dst),
	    "metaball_delete_pnt command must not report an in-place edit as an add");
    CHECK(ged_event_observer_remove(gedp, metaball_delete_post_token) == 1,
	    "metaball_delete_pnt observer removal must succeed");

    char bo_event_file[MAXPATHLEN] = {0};
    FILE *bo_fp = bu_temp_file(bo_event_file, MAXPATHLEN);
    CHECK(bo_fp != NULL,
	    "bo command event fixture temp file must be created");
    if (bo_fp) {
	const unsigned char bo_data[4] = {1, 2, 3, 4};
	CHECK(fwrite(bo_data, sizeof(bo_data), 1, bo_fp) == 1,
		"bo command event fixture temp file must be writable");
	fclose(bo_fp);
    }
    event_order_observer bo_post;
    ged_event_observer_token bo_post_token =
	ged_event_observer_add(gedp, GED_EVENT_OBSERVER_POST_RECONCILE,
		event_order_cb, &bo_post);
    CHECK(bo_post_token != 0,
	    "bo observer must register");
    const char *bo_av[7] = {"bo", "-i", "u", "c", bo_event_dst,
	bo_event_file, NULL};
    CHECK(bo_fp && ged_exec(gedp, 6, bo_av) == BRLCAD_OK,
	    "bo import command must publish object creation event");
    CHECK(bo_post.calls == 1,
	    "bo import command must publish one event transaction");
    CHECK(observed_named_event(bo_post, GED_EVENT_OBJECT_ADDED,
		bo_event_dst),
	    "bo import command must emit object-added event");
    CHECK(std::find(bo_post.all_kinds.begin(),
	    bo_post.all_kinds.end(), GED_EVENT_OBJECT_REMOVED) ==
	    bo_post.all_kinds.end(),
	    "bo import fixture must not publish removals");
    CHECK(ged_event_observer_remove(gedp, bo_post_token) == 1,
	    "bo observer removal must succeed");
    if (bo_event_file[0] != '\0')
	bu_file_delete(bo_event_file);

    event_order_observer threeptarb_post;
    ged_event_observer_token threeptarb_post_token =
	ged_event_observer_add(gedp, GED_EVENT_OBSERVER_POST_RECONCILE,
		event_order_cb, &threeptarb_post);
    CHECK(threeptarb_post_token != 0,
	    "3ptarb observer must register");
    const char *threeptarb_av[16] = {"3ptarb", threeptarb_event_dst,
	"0", "0", "0", "1", "0", "0", "0", "1", "0", "z", "1", "1",
	"1", NULL};
    CHECK(ged_exec(gedp, 15, threeptarb_av) == BRLCAD_OK,
	    "3ptarb command must publish object creation event");
    CHECK(threeptarb_post.calls == 1,
	    "3ptarb command must publish one event transaction");
    CHECK(observed_named_event(threeptarb_post, GED_EVENT_OBJECT_ADDED,
		threeptarb_event_dst),
	    "3ptarb command must emit object-added event");
    CHECK(std::find(threeptarb_post.all_kinds.begin(),
	    threeptarb_post.all_kinds.end(), GED_EVENT_OBJECT_REMOVED) ==
	    threeptarb_post.all_kinds.end(),
	    "3ptarb fixture must not publish removals");
    CHECK(ged_event_observer_remove(gedp, threeptarb_post_token) == 1,
	    "3ptarb observer removal must succeed");

    event_order_observer cc_post;
    ged_event_observer_token cc_post_token =
	ged_event_observer_add(gedp, GED_EVENT_OBSERVER_POST_RECONCILE,
		event_order_cb, &cc_post);
    CHECK(cc_post_token != 0,
	    "cc observer must register");
    const char *cc_av[4] = {"cc", cc_event_dst,
	"_ged_event_constraint_expr", NULL};
    CHECK(ged_exec(gedp, 3, cc_av) == BRLCAD_OK,
	    "cc command must publish object creation event");
    CHECK(cc_post.calls == 1,
	    "cc command must publish one event transaction");
    CHECK(observed_named_event(cc_post, GED_EVENT_OBJECT_ADDED,
		cc_event_dst),
	    "cc command must emit object-added event");
    CHECK(std::find(cc_post.all_kinds.begin(),
	    cc_post.all_kinds.end(), GED_EVENT_OBJECT_REMOVED) ==
	    cc_post.all_kinds.end(),
	    "cc fixture must not publish removals");
    CHECK(ged_event_observer_remove(gedp, cc_post_token) == 1,
	    "cc observer removal must succeed");

    event_order_observer inside_post;
    ged_event_observer_token inside_post_token =
	ged_event_observer_add(gedp, GED_EVENT_OBSERVER_POST_RECONCILE,
		event_order_cb, &inside_post);
    CHECK(inside_post_token != 0,
	    "inside observer must register");
    const char *inside_av[5] = {"inside", inside_event_src,
	inside_event_dst, "0.25", NULL};
    CHECK(ged_exec(gedp, 4, inside_av) == BRLCAD_OK,
	    "inside command must publish object creation event");
    CHECK(inside_post.calls == 1,
	    "inside command must publish one event transaction");
    CHECK(observed_named_event(inside_post, GED_EVENT_OBJECT_ADDED,
		inside_event_dst),
	    "inside command must emit object-added event for created object");
    CHECK(std::find(inside_post.all_kinds.begin(),
	    inside_post.all_kinds.end(), GED_EVENT_OBJECT_REMOVED) ==
	    inside_post.all_kinds.end(),
	    "inside creation fixture must not publish removals");
    CHECK(ged_event_observer_remove(gedp, inside_post_token) == 1,
	    "inside observer removal must succeed");

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

    event_order_observer ptranslate_post;
    ged_event_observer_token ptranslate_post_token =
	ged_event_observer_add(gedp, GED_EVENT_OBSERVER_POST_RECONCILE,
		event_order_cb, &ptranslate_post);
    CHECK(ptranslate_post_token != 0,
	    "ptranslate observer must register");
    const char *ptranslate_av[6] = {"ptranslate", "-r", cpi_event_src,
	"H", "0 0 1", NULL};
    CHECK(ged_exec(gedp, 5, ptranslate_av) == BRLCAD_OK,
	    "ptranslate command must publish object-modified event");
    CHECK(ptranslate_post.calls == 1,
	    "ptranslate command must publish one event transaction");
    CHECK(observed_named_event(ptranslate_post, GED_EVENT_OBJECT_MODIFIED,
		cpi_event_src),
	    "ptranslate command must emit object-modified event");
    CHECK(!observed_named_event(ptranslate_post, GED_EVENT_OBJECT_ADDED,
		cpi_event_src),
	    "ptranslate command must not report an in-place edit as an add");
    CHECK(ged_event_observer_remove(gedp, ptranslate_post_token) == 1,
	    "ptranslate observer removal must succeed");

    event_order_observer protate_post;
    ged_event_observer_token protate_post_token =
	ged_event_observer_add(gedp, GED_EVENT_OBSERVER_POST_RECONCILE,
		event_order_cb, &protate_post);
    CHECK(protate_post_token != 0,
	    "protate observer must register");
    const char *protate_av[5] = {"protate", cpi_event_src, "H", "1 0 0",
	NULL};
    CHECK(ged_exec(gedp, 4, protate_av) == BRLCAD_OK,
	    "protate command must publish object-modified event");
    CHECK(protate_post.calls == 1,
	    "protate command must publish one event transaction");
    CHECK(observed_named_event(protate_post, GED_EVENT_OBJECT_MODIFIED,
		cpi_event_src),
	    "protate command must emit object-modified event");
    CHECK(!observed_named_event(protate_post, GED_EVENT_OBJECT_ADDED,
		cpi_event_src),
	    "protate command must not report an in-place edit as an add");
    CHECK(ged_event_observer_remove(gedp, protate_post_token) == 1,
	    "protate observer removal must succeed");

    event_order_observer bev_post;
    ged_event_observer_token bev_post_token =
	ged_event_observer_add(gedp, GED_EVENT_OBSERVER_POST_RECONCILE,
		event_order_cb, &bev_post);
    CHECK(bev_post_token != 0,
	    "bev observer must register");
    const char *bev_av[4] = {"bev", bev_event_dst, bev_event_region, NULL};
    CHECK(ged_exec(gedp, 3, bev_av) == BRLCAD_OK,
	    "bev command must publish object creation event");
    CHECK(bev_post.calls == 1,
	    "bev command must publish one event transaction");
    CHECK(observed_named_event(bev_post, GED_EVENT_OBJECT_ADDED,
		bev_event_dst),
	    "bev command must emit object-added event");
    CHECK(std::find(bev_post.all_kinds.begin(),
	    bev_post.all_kinds.end(), GED_EVENT_OBJECT_REMOVED) ==
	    bev_post.all_kinds.end(),
	    "bev fixture must not publish removals");
    CHECK(ged_event_observer_remove(gedp, bev_post_token) == 1,
	    "bev observer removal must succeed");

    bsg_data_polygon_state polyclip_state;
    std::memset(&polyclip_state, 0, sizeof(polyclip_state));
    MAT_IDN(polyclip_state.gdps_rotation);
    MAT_IDN(polyclip_state.gdps_model2view);
    MAT_IDN(polyclip_state.gdps_view2model);
    polyclip_state.gdps_scale = 1.0;
    VSET(polyclip_state.gdps_origin, 0.0, 0.0, 0.0);
    polyclip_state.gdps_data_vZ = 0.0;
    int polyclip_hole = 0;
    point_t polyclip_points[4];
    VSET(polyclip_points[0], 0.0, 0.0, 0.0);
    VSET(polyclip_points[1], 1.0, 0.0, 0.0);
    VSET(polyclip_points[2], 1.0, 1.0, 0.0);
    VSET(polyclip_points[3], 0.0, 1.0, 0.0);
    struct bg_poly_contour polyclip_contour;
    std::memset(&polyclip_contour, 0, sizeof(polyclip_contour));
    polyclip_contour.num_points = 4;
    polyclip_contour.point = polyclip_points;
    struct bg_polygon polyclip_polygon;
    std::memset(&polyclip_polygon, 0, sizeof(polyclip_polygon));
    polyclip_polygon.num_contours = 1;
    polyclip_polygon.hole = &polyclip_hole;
    polyclip_polygon.contour = &polyclip_contour;
    polyclip_state.gdps_polygons.num_polygons = 1;
    polyclip_state.gdps_polygons.polygon = &polyclip_polygon;

    event_order_observer polyclip_post;
    ged_event_observer_token polyclip_post_token =
	ged_event_observer_add(gedp, GED_EVENT_OBSERVER_POST_RECONCILE,
		event_order_cb, &polyclip_post);
    CHECK(polyclip_post_token != 0,
	    "polyclip export observer must register");
    CHECK(ged_export_polygon(gedp, &polyclip_state, 0,
		polyclip_event_sketch) == BRLCAD_OK,
	    "polyclip export helper must publish object creation event");
    CHECK(polyclip_post.calls == 1,
	    "polyclip export helper must publish one event transaction");
    CHECK(observed_named_event(polyclip_post, GED_EVENT_OBJECT_ADDED,
		polyclip_event_sketch),
	    "polyclip export helper must emit object-added event");
    CHECK(db_lookup(gedp->dbip, polyclip_event_sketch, LOOKUP_QUIET) !=
	    RT_DIR_NULL,
	    "polyclip export helper must create sketch object");
    CHECK(std::find(polyclip_post.all_kinds.begin(),
	    polyclip_post.all_kinds.end(), GED_EVENT_OBJECT_REMOVED) ==
	    polyclip_post.all_kinds.end(),
	    "polyclip export helper fixture must not publish removals");
    CHECK(ged_event_observer_remove(gedp, polyclip_post_token) == 1,
	    "polyclip export observer removal must succeed");

    event_order_observer analyze_pnts_post;
    ged_event_observer_token analyze_pnts_post_token =
	ged_event_observer_add(gedp, GED_EVENT_OBSERVER_POST_RECONCILE,
		event_order_cb, &analyze_pnts_post);
    CHECK(analyze_pnts_post_token != 0,
	    "analyze PNTS output observer must register");
    const char *analyze_pnts_av[7] = {"analyze", "intersect", "-o",
	analyze_pnts_out, analyze_pnts_src, analyze_vol_src, NULL};
    int analyze_pnts_ret = ged_exec(gedp, 6, analyze_pnts_av);
    if (analyze_pnts_ret != BRLCAD_OK)
	bu_log("analyze PNTS result: %s\n", bu_vls_cstr(gedp->ged_result_str));
    CHECK(analyze_pnts_ret == BRLCAD_OK,
	    "analyze PNTS/volume output command must succeed");
    CHECK(analyze_pnts_post.calls == 1,
	    "analyze PNTS/volume output must publish one event transaction");
    CHECK(observed_named_event(analyze_pnts_post, GED_EVENT_OBJECT_ADDED,
		analyze_pnts_out),
	    "analyze PNTS/volume output must emit object-added event");
    CHECK(db_lookup(gedp->dbip, analyze_pnts_out, LOOKUP_QUIET) !=
	    RT_DIR_NULL,
	    "analyze PNTS/volume output object must be present");
    CHECK(std::find(analyze_pnts_post.all_kinds.begin(),
	    analyze_pnts_post.all_kinds.end(), GED_EVENT_OBJECT_RENAMED) ==
	    analyze_pnts_post.all_kinds.end(),
	    "analyze PNTS direct output must not publish temporary rename");
    CHECK(std::find(analyze_pnts_post.all_kinds.begin(),
	    analyze_pnts_post.all_kinds.end(), GED_EVENT_OBJECT_REMOVED) ==
	    analyze_pnts_post.all_kinds.end(),
	    "analyze PNTS direct output must not publish temporary removal");
    CHECK(ged_event_observer_remove(gedp, analyze_pnts_post_token) == 1,
	    "analyze PNTS output observer removal must succeed");

    char pnts_read_basic_file[MAXPATHLEN] = {0};
    FILE *pnts_read_basic_fp = bu_temp_file(pnts_read_basic_file, MAXPATHLEN);
    if (pnts_read_basic_fp) {
	CHECK(fprintf(pnts_read_basic_fp, "1 2 3\n") > 0,
		"pnts read basic fixture must write point data");
	fclose(pnts_read_basic_fp);
    }
    event_order_observer pnts_read_basic_post;
    ged_event_observer_token pnts_read_basic_post_token =
	ged_event_observer_add(gedp, GED_EVENT_OBSERVER_POST_RECONCILE,
		event_order_cb, &pnts_read_basic_post);
    CHECK(pnts_read_basic_post_token != 0,
	    "pnts read basic observer must register");
    const char *pnts_read_basic_av[7] = {"pnts", "read", "-f", "xyz",
	pnts_read_basic_file, pnts_read_basic_obj, NULL};
    CHECK(pnts_read_basic_fp &&
	    ged_exec(gedp, 6, pnts_read_basic_av) == BRLCAD_OK,
	    "pnts read basic command must publish object creation event");
    CHECK(pnts_read_basic_post.calls == 1,
	    "pnts read basic command must publish one event transaction");
    CHECK(observed_named_event(pnts_read_basic_post, GED_EVENT_OBJECT_ADDED,
		pnts_read_basic_obj),
	    "pnts read basic command must emit object-added event");
    CHECK(db_lookup(gedp->dbip, pnts_read_basic_obj, LOOKUP_QUIET) !=
	    RT_DIR_NULL,
	    "pnts read basic command must create PNTS object");
    CHECK(ged_event_observer_remove(gedp, pnts_read_basic_post_token) == 1,
	    "pnts read basic observer removal must succeed");
    bu_file_delete(pnts_read_basic_file);

    char pnts_read_full_file[MAXPATHLEN] = {0};
    FILE *pnts_read_full_fp = bu_temp_file(pnts_read_full_file, MAXPATHLEN);
    if (pnts_read_full_fp) {
	CHECK(fprintf(pnts_read_full_fp,
		    "4 5 6 0 0 1 0.5 255 128 0\n") > 0,
		"pnts read full fixture must write point data");
	fclose(pnts_read_full_fp);
    }
    event_order_observer pnts_read_full_post;
    ged_event_observer_token pnts_read_full_post_token =
	ged_event_observer_add(gedp, GED_EVENT_OBSERVER_POST_RECONCILE,
		event_order_cb, &pnts_read_full_post);
    CHECK(pnts_read_full_post_token != 0,
	    "pnts read full observer must register");
    const char *pnts_read_full_av[7] = {"pnts", "read", "-f",
	"xyzijksrgb", pnts_read_full_file, pnts_read_full_obj, NULL};
    CHECK(pnts_read_full_fp &&
	    ged_exec(gedp, 6, pnts_read_full_av) == BRLCAD_OK,
	    "pnts read full command must publish object creation event");
    CHECK(pnts_read_full_post.calls == 1,
	    "pnts read full command must publish one event transaction");
    CHECK(observed_named_event(pnts_read_full_post, GED_EVENT_OBJECT_ADDED,
		pnts_read_full_obj),
	    "pnts read full command must emit object-added event");
    CHECK(db_lookup(gedp->dbip, pnts_read_full_obj, LOOKUP_QUIET) !=
	    RT_DIR_NULL,
	    "pnts read full command must create PNTS object");
    CHECK(ged_event_observer_remove(gedp, pnts_read_full_post_token) == 1,
	    "pnts read full observer removal must succeed");
    bu_file_delete(pnts_read_full_file);

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

    event_order_observer arb_create_post;
    ged_event_observer_token arb_create_post_token =
	ged_event_observer_add(gedp, GED_EVENT_OBSERVER_POST_RECONCILE,
		event_order_cb, &arb_create_post);
    CHECK(arb_create_post_token != 0,
	    "arb create observer must register");
    const char *arb_create_av[6] = {"arb", "create", arb_create_event_dst,
	"0", "0", NULL};
    CHECK(ged_exec(gedp, 5, arb_create_av) == BRLCAD_OK,
	    "arb create command must publish object creation event");
    CHECK(arb_create_post.calls == 1,
	    "arb create command must publish one event transaction");
    CHECK(observed_named_event(arb_create_post, GED_EVENT_OBJECT_ADDED,
		arb_create_event_dst),
	    "arb create command must emit object-added event");
    CHECK(std::find(arb_create_post.all_kinds.begin(),
	    arb_create_post.all_kinds.end(), GED_EVENT_OBJECT_REMOVED) ==
	    arb_create_post.all_kinds.end(),
	    "arb create fixture must not publish removals");
    CHECK(ged_event_observer_remove(gedp, arb_create_post_token) == 1,
	    "arb create observer removal must succeed");

    event_order_observer rotate_arb_face_post;
    ged_event_observer_token rotate_arb_face_post_token =
	ged_event_observer_add(gedp, GED_EVENT_OBSERVER_POST_RECONCILE,
		event_order_cb, &rotate_arb_face_post);
    CHECK(rotate_arb_face_post_token != 0,
	    "rotate_arb_face observer must register");
    const char *rotate_arb_face_av[6] = {"rotate_arb_face",
	arb_create_event_dst, "1", "1", "0 0 1", NULL};
    CHECK(ged_exec(gedp, 5, rotate_arb_face_av) == BRLCAD_OK,
	    "rotate_arb_face command must publish object-modified event");
    CHECK(rotate_arb_face_post.calls == 1,
	    "rotate_arb_face command must publish one event transaction");
    CHECK(observed_named_event(rotate_arb_face_post,
		GED_EVENT_OBJECT_MODIFIED, arb_create_event_dst),
	    "rotate_arb_face command must emit object-modified event");
    CHECK(!observed_named_event(rotate_arb_face_post,
		GED_EVENT_OBJECT_ADDED, arb_create_event_dst),
	    "rotate_arb_face command must not report an in-place edit as an add");
    CHECK(ged_event_observer_remove(gedp, rotate_arb_face_post_token) == 1,
	    "rotate_arb_face observer removal must succeed");

    event_order_observer edarb_extrude_post;
    ged_event_observer_token edarb_extrude_post_token =
	ged_event_observer_add(gedp, GED_EVENT_OBSERVER_POST_RECONCILE,
		event_order_cb, &edarb_extrude_post);
    CHECK(edarb_extrude_post_token != 0,
	    "edarb extrude observer must register");
    const char *edarb_extrude_av[6] = {"edarb", "extrude",
	arb_create_event_dst, "1234", "1", NULL};
    CHECK(ged_exec(gedp, 5, edarb_extrude_av) == BRLCAD_OK,
	    "edarb extrude command must publish object-modified event");
    CHECK(edarb_extrude_post.calls == 1,
	    "edarb extrude command must publish one event transaction");
    CHECK(observed_named_event(edarb_extrude_post,
		GED_EVENT_OBJECT_MODIFIED, arb_create_event_dst),
	    "edarb extrude command must emit object-modified event");
    CHECK(!observed_named_event(edarb_extrude_post,
		GED_EVENT_OBJECT_ADDED, arb_create_event_dst),
	    "edarb extrude command must not report an in-place edit as an add");
    CHECK(ged_event_observer_remove(gedp, edarb_extrude_post_token) == 1,
	    "edarb extrude observer removal must succeed");

    event_order_observer move_arb_face_post;
    ged_event_observer_token move_arb_face_post_token =
	ged_event_observer_add(gedp, GED_EVENT_OBSERVER_POST_RECONCILE,
		event_order_cb, &move_arb_face_post);
    CHECK(move_arb_face_post_token != 0,
	    "move_arb_face observer must register");
    const char *move_arb_face_av[6] = {"move_arb_face", "-r",
	arb_create_event_dst, "1", "0 0 1", NULL};
    CHECK(ged_exec(gedp, 5, move_arb_face_av) == BRLCAD_OK,
	    "move_arb_face command must publish object-modified event");
    CHECK(move_arb_face_post.calls == 1,
	    "move_arb_face command must publish one event transaction");
    CHECK(observed_named_event(move_arb_face_post,
		GED_EVENT_OBJECT_MODIFIED, arb_create_event_dst),
	    "move_arb_face command must emit object-modified event");
    CHECK(!observed_named_event(move_arb_face_post,
		GED_EVENT_OBJECT_ADDED, arb_create_event_dst),
	    "move_arb_face command must not report an in-place edit as an add");
    CHECK(ged_event_observer_remove(gedp, move_arb_face_post_token) == 1,
	    "move_arb_face observer removal must succeed");

    event_order_observer move_arb_edge_post;
    ged_event_observer_token move_arb_edge_post_token =
	ged_event_observer_add(gedp, GED_EVENT_OBSERVER_POST_RECONCILE,
		event_order_cb, &move_arb_edge_post);
    CHECK(move_arb_edge_post_token != 0,
	    "move_arb_edge observer must register");
    const char *move_arb_edge_av[6] = {"move_arb_edge", "-r",
	arb_create_event_dst, "1", "0 0 1", NULL};
    CHECK(ged_exec(gedp, 5, move_arb_edge_av) == BRLCAD_OK,
	    "move_arb_edge command must publish object-modified event");
    CHECK(move_arb_edge_post.calls == 1,
	    "move_arb_edge command must publish one event transaction");
    CHECK(observed_named_event(move_arb_edge_post,
		GED_EVENT_OBJECT_MODIFIED, arb_create_event_dst),
	    "move_arb_edge command must emit object-modified event");
    CHECK(!observed_named_event(move_arb_edge_post,
		GED_EVENT_OBJECT_ADDED, arb_create_event_dst),
	    "move_arb_edge command must not report an in-place edit as an add");
    CHECK(ged_event_observer_remove(gedp, move_arb_edge_post_token) == 1,
	    "move_arb_edge observer removal must succeed");

    event_order_observer track_post;
    ged_event_observer_token track_post_token =
	ged_event_observer_add(gedp, GED_EVENT_OBSERVER_POST_RECONCILE,
		event_order_cb, &track_post);
    CHECK(track_post_token != 0,
	    "track observer must register");
    const char *track_av[16] = {"track", track_event_group, "10", "0",
	"0", "1", "-3", "0", "1", "13", "0", "1", "-1", "1", "0.2",
	NULL};
    CHECK(ged_exec(gedp, 15, track_av) == BRLCAD_OK,
	    "track command must publish object creation events");
    CHECK(track_post.calls == 1,
	    "track command must publish one event transaction");
    CHECK(observed_named_event(track_post, GED_EVENT_OBJECT_ADDED,
		track_event_group),
	    "track command must emit object-added event for final group");
    CHECK(observed_named_event(track_post, GED_EVENT_OBJECT_ADDED,
		"_ged_event_track.s.0"),
	    "track command must emit object-added event for generated solids");
    CHECK(std::find(track_post.all_kinds.begin(),
	    track_post.all_kinds.end(), GED_EVENT_OBJECT_REMOVED) ==
	    track_post.all_kinds.end(),
	    "track command fixture must not publish removals");
    CHECK(ged_event_observer_remove(gedp, track_post_token) == 1,
	    "track observer removal must succeed");

    event_order_observer coil_post;
    ged_event_observer_token coil_post_token =
	ged_event_observer_add(gedp, GED_EVENT_OBSERVER_POST_RECONCILE,
		event_order_cb, &coil_post);
    CHECK(coil_post_token != 0,
	    "coil observer must register");
    const char *coil_av[3] = {"coil", coil_event_group, NULL};
    CHECK(ged_exec(gedp, 2, coil_av) == BRLCAD_OK,
	    "coil command must publish object creation events");
    CHECK(coil_post.calls == 1,
	    "coil command must publish one event transaction");
    CHECK(observed_named_event(coil_post, GED_EVENT_OBJECT_ADDED,
		coil_event_group),
	    "coil command must emit object-added event for final comb");
    CHECK(observed_named_event(coil_post, GED_EVENT_OBJECT_ADDED,
		"_ged_event_coil_core.s"),
	    "coil command must emit object-added event for generated core");
    CHECK(std::find(coil_post.all_kinds.begin(),
	    coil_post.all_kinds.end(), GED_EVENT_OBJECT_REMOVED) ==
	    coil_post.all_kinds.end(),
	    "coil command fixture must not publish removals");
    CHECK(ged_event_observer_remove(gedp, coil_post_token) == 1,
	    "coil observer removal must succeed");

    event_order_observer human_post;
    ged_event_observer_token human_post_token =
	ged_event_observer_add(gedp, GED_EVENT_OBSERVER_POST_RECONCILE,
		event_order_cb, &human_post);
    CHECK(human_post_token != 0,
	    "human observer must register");
    const char *human_av[4] = {"human", "-n", human_event_top, NULL};
    CHECK(ged_exec(gedp, 3, human_av) == BRLCAD_OK,
	    "human command must publish object creation events");
    CHECK(human_post.calls == 1,
	    "human command must publish one event transaction");
    CHECK(observed_named_event(human_post, GED_EVENT_OBJECT_ADDED,
		human_event_top),
	    "human command must emit object-added event for final body comb");
    CHECK(observed_named_event(human_post, GED_EVENT_OBJECT_ADDED,
		"Head.s"),
	    "human command must emit object-added event for generated solids");
    CHECK(std::find(human_post.all_kinds.begin(), human_post.all_kinds.end(),
	    GED_EVENT_DATABASE_METADATA_CHANGED) != human_post.all_kinds.end(),
	    "human command must emit database metadata change event");
    CHECK(std::find(human_post.all_kinds.begin(), human_post.all_kinds.end(),
	    GED_EVENT_OBJECT_REMOVED) == human_post.all_kinds.end(),
	    "human command fixture must not publish removals");
    CHECK(db_lookup(gedp->dbip, human_event_top, LOOKUP_QUIET) != RT_DIR_NULL,
	    "human command must create final body comb");
    CHECK(ged_event_observer_remove(gedp, human_post_token) == 1,
	    "human observer removal must succeed");

    event_order_observer voxelize_post;
    ged_event_observer_token voxelize_post_token =
	ged_event_observer_add(gedp, GED_EVENT_OBSERVER_POST_RECONCILE,
		event_order_cb, &voxelize_post);
    CHECK(voxelize_post_token != 0,
	    "voxelize observer must register");
    const char *voxelize_av[10] = {"voxelize", "-s", "2 2 2", "-d", "1",
	"-t", "0.5", voxelize_event_dst, voxelize_event_region, NULL};
    CHECK(ged_exec(gedp, 9, voxelize_av) == BRLCAD_OK,
	    "voxelize command must publish object creation events");
    CHECK(voxelize_post.calls == 1,
	    "voxelize command must publish one event transaction");
    CHECK(observed_named_event(voxelize_post, GED_EVENT_OBJECT_ADDED,
		voxelize_event_dst),
	    "voxelize command must emit object-added event for final comb");
    CHECK(observed_named_event(voxelize_post, GED_EVENT_OBJECT_ADDED,
		voxelize_event_box),
	    "voxelize command must emit object-added event for generated voxel");
    CHECK(db_lookup(gedp->dbip, voxelize_event_dst, LOOKUP_QUIET) !=
	    RT_DIR_NULL,
	    "voxelize command must create final comb");
    CHECK(db_lookup(gedp->dbip, voxelize_event_box, LOOKUP_QUIET) !=
	    RT_DIR_NULL,
	    "voxelize command must create generated voxel solid");
    CHECK(std::find(voxelize_post.all_kinds.begin(),
	    voxelize_post.all_kinds.end(), GED_EVENT_OBJECT_REMOVED) ==
	    voxelize_post.all_kinds.end(),
	    "voxelize command fixture must not publish removals");
    CHECK(ged_event_observer_remove(gedp, voxelize_post_token) == 1,
	    "voxelize observer removal must succeed");

    event_order_observer arb_repair_post;
    ged_event_observer_token arb_repair_post_token =
	ged_event_observer_add(gedp, GED_EVENT_OBSERVER_POST_RECONCILE,
		event_order_cb, &arb_repair_post);
    CHECK(arb_repair_post_token != 0,
	    "arb repair observer must register");
    const char *arb_repair_av[6] = {"arb", "repair", "-o",
	arb_repair_event_dst, rfarb_event_dst, NULL};
    CHECK(ged_exec(gedp, 5, arb_repair_av) == BRLCAD_OK,
	    "arb repair output command must publish object creation event");
    CHECK(arb_repair_post.calls == 1,
	    "arb repair output command must publish one event transaction");
    CHECK(observed_named_event(arb_repair_post, GED_EVENT_OBJECT_ADDED,
		arb_repair_event_dst),
	    "arb repair output command must emit object-added event");
    CHECK(std::find(arb_repair_post.all_kinds.begin(),
	    arb_repair_post.all_kinds.end(), GED_EVENT_OBJECT_REMOVED) ==
	    arb_repair_post.all_kinds.end(),
	    "arb repair output fixture must not publish removals");
    CHECK(ged_event_observer_remove(gedp, arb_repair_post_token) == 1,
	    "arb repair observer removal must succeed");

    event_order_observer bot_flip_post;
    ged_event_observer_token bot_flip_post_token =
	ged_event_observer_add(gedp, GED_EVENT_OBSERVER_POST_RECONCILE,
		event_order_cb, &bot_flip_post);
    CHECK(bot_flip_post_token != 0,
	    "bot flip observer must register");
    const char *bot_flip_av[4] = {"bot", "flip", bot_split_event_src, NULL};
    CHECK(ged_exec(gedp, 3, bot_flip_av) == BRLCAD_OK,
	    "bot flip command must publish object-modified event");
    CHECK(bot_flip_post.calls == 1,
	    "bot flip command must publish one event transaction");
    CHECK(observed_named_event(bot_flip_post, GED_EVENT_OBJECT_MODIFIED,
		bot_split_event_src),
	    "bot flip command must emit object-modified event");
    CHECK(!observed_named_event(bot_flip_post, GED_EVENT_OBJECT_ADDED,
		bot_split_event_src),
	    "bot flip command must not report an in-place edit as an add");
    CHECK(ged_event_observer_remove(gedp, bot_flip_post_token) == 1,
	    "bot flip observer removal must succeed");

    event_order_observer bot_pca_post;
    ged_event_observer_token bot_pca_post_token =
	ged_event_observer_add(gedp, GED_EVENT_OBSERVER_POST_RECONCILE,
		event_order_cb, &bot_pca_post);
    CHECK(bot_pca_post_token != 0,
	    "bot pca observer must register");
    const char *bot_pca_av[5] = {"bot", "pca", bot_split_event_src,
	bot_pca_event_dst, NULL};
    CHECK(ged_exec(gedp, 4, bot_pca_av) == BRLCAD_OK,
	    "bot pca output command must publish object creation event");
    CHECK(bot_pca_post.calls == 1,
	    "bot pca output command must publish one event transaction");
    CHECK(observed_named_event(bot_pca_post, GED_EVENT_OBJECT_ADDED,
		bot_pca_event_dst),
	    "bot pca output command must emit object-added event");
    CHECK(std::find(bot_pca_post.all_kinds.begin(),
	    bot_pca_post.all_kinds.end(), GED_EVENT_OBJECT_REMOVED) ==
	    bot_pca_post.all_kinds.end(),
	    "bot pca output fixture must not publish removals");
    CHECK(ged_event_observer_remove(gedp, bot_pca_post_token) == 1,
	    "bot pca observer removal must succeed");

    event_order_observer bot_set_post;
    ged_event_observer_token bot_set_post_token =
	ged_event_observer_add(gedp, GED_EVENT_OBSERVER_POST_RECONCILE,
		event_order_cb, &bot_set_post);
    CHECK(bot_set_post_token != 0,
	    "bot set observer must register");
    const char *bot_set_av[6] = {"bot", "set", "orientation",
	bot_cpp_event_src, "cw", NULL};
    CHECK(ged_exec(gedp, 5, bot_set_av) == BRLCAD_OK,
	    "bot set command must publish object-modified event");
    CHECK(bot_set_post.calls == 1,
	    "bot set command must publish one event transaction");
    CHECK(observed_named_event(bot_set_post, GED_EVENT_OBJECT_MODIFIED,
		bot_cpp_event_src),
	    "bot set command must emit object-modified event");
    CHECK(!observed_named_event(bot_set_post, GED_EVENT_OBJECT_ADDED,
		bot_cpp_event_src),
	    "bot set command must not report an in-place edit as an add");
    CHECK(ged_event_observer_remove(gedp, bot_set_post_token) == 1,
	    "bot set observer removal must succeed");

    event_order_observer bot_sync_post;
    ged_event_observer_token bot_sync_post_token =
	ged_event_observer_add(gedp, GED_EVENT_OBSERVER_POST_RECONCILE,
		event_order_cb, &bot_sync_post);
    CHECK(bot_sync_post_token != 0,
	    "bot sync observer must register");
    const char *bot_sync_av[4] = {"bot", "sync", bot_cpp_event_src, NULL};
    CHECK(ged_exec(gedp, 3, bot_sync_av) == BRLCAD_OK,
	    "bot sync command must publish object-modified event");
    CHECK(bot_sync_post.calls == 1,
	    "bot sync command must publish one event transaction");
    CHECK(observed_named_event(bot_sync_post, GED_EVENT_OBJECT_MODIFIED,
		bot_cpp_event_src),
	    "bot sync command must emit object-modified event");
    CHECK(!observed_named_event(bot_sync_post, GED_EVENT_OBJECT_ADDED,
		bot_cpp_event_src),
	    "bot sync command must not report an in-place edit as an add");
    CHECK(ged_event_observer_remove(gedp, bot_sync_post_token) == 1,
	    "bot sync observer removal must succeed");

    event_order_observer bot_chull_post;
    ged_event_observer_token bot_chull_post_token =
	ged_event_observer_add(gedp, GED_EVENT_OBSERVER_POST_RECONCILE,
		event_order_cb, &bot_chull_post);
    CHECK(bot_chull_post_token != 0,
	    "bot chull observer must register");
    const char *bot_chull_av[5] = {"bot", "chull", bot_cpp_event_src,
	bot_chull_event_dst, NULL};
    CHECK(ged_exec(gedp, 4, bot_chull_av) == BRLCAD_OK,
	    "bot chull output command must publish object creation event");
    CHECK(bot_chull_post.calls == 1,
	    "bot chull output command must publish one event transaction");
    CHECK(observed_named_event(bot_chull_post, GED_EVENT_OBJECT_ADDED,
		bot_chull_event_dst),
	    "bot chull output command must emit object-added event");
    CHECK(db_lookup(gedp->dbip, bot_chull_event_dst, LOOKUP_QUIET) !=
	    RT_DIR_NULL,
	    "bot chull output object must exist after command");
    CHECK(std::find(bot_chull_post.all_kinds.begin(),
	    bot_chull_post.all_kinds.end(), GED_EVENT_OBJECT_REMOVED) ==
	    bot_chull_post.all_kinds.end(),
	    "bot chull output fixture must not publish removals");
    CHECK(ged_event_observer_remove(gedp, bot_chull_post_token) == 1,
	    "bot chull observer removal must succeed");

    event_order_observer bot_decimate_cpp_post;
    ged_event_observer_token bot_decimate_cpp_post_token =
	ged_event_observer_add(gedp, GED_EVENT_OBSERVER_POST_RECONCILE,
		event_order_cb, &bot_decimate_cpp_post);
    CHECK(bot_decimate_cpp_post_token != 0,
	    "bot decimate observer must register");
    const char *bot_decimate_cpp_av[7] = {"bot", "decimate", "-t",
	"0.001", bot_cpp_event_src, bot_decimate_cpp_event_dst, NULL};
    CHECK(ged_exec(gedp, 6, bot_decimate_cpp_av) == BRLCAD_OK,
	    "bot decimate output command must publish object creation event");
    CHECK(bot_decimate_cpp_post.calls == 1,
	    "bot decimate output command must publish one event transaction");
    CHECK(observed_named_event(bot_decimate_cpp_post,
		GED_EVENT_OBJECT_ADDED, bot_decimate_cpp_event_dst),
	    "bot decimate output command must emit object-added event");
    CHECK(db_lookup(gedp->dbip, bot_decimate_cpp_event_dst, LOOKUP_QUIET) !=
	    RT_DIR_NULL,
	    "bot decimate output object must exist after command");
    CHECK(std::find(bot_decimate_cpp_post.all_kinds.begin(),
	    bot_decimate_cpp_post.all_kinds.end(), GED_EVENT_OBJECT_REMOVED) ==
	    bot_decimate_cpp_post.all_kinds.end(),
	    "bot decimate output fixture must not publish removals");
    CHECK(ged_event_observer_remove(gedp, bot_decimate_cpp_post_token) == 1,
	    "bot decimate observer removal must succeed");

    event_order_observer bot_subd_post;
    ged_event_observer_token bot_subd_post_token =
	ged_event_observer_add(gedp, GED_EVENT_OBSERVER_POST_RECONCILE,
		event_order_cb, &bot_subd_post);
    CHECK(bot_subd_post_token != 0,
	    "bot subd observer must register");
    const char *bot_subd_av[7] = {"bot", "subd", "-l", "1",
	bot_cpp_event_src, bot_subd_event_dst, NULL};
    CHECK(ged_exec(gedp, 6, bot_subd_av) == BRLCAD_OK,
	    "bot subd output command must publish object creation event");
    CHECK(bot_subd_post.calls == 1,
	    "bot subd output command must publish one event transaction");
    CHECK(observed_named_event(bot_subd_post, GED_EVENT_OBJECT_ADDED,
		bot_subd_event_dst),
	    "bot subd output command must emit object-added event");
    CHECK(db_lookup(gedp->dbip, bot_subd_event_dst, LOOKUP_QUIET) !=
	    RT_DIR_NULL,
	    "bot subd output object must exist after command");
    CHECK(std::find(bot_subd_post.all_kinds.begin(),
	    bot_subd_post.all_kinds.end(), GED_EVENT_OBJECT_REMOVED) ==
	    bot_subd_post.all_kinds.end(),
	    "bot subd output fixture must not publish removals");
    CHECK(ged_event_observer_remove(gedp, bot_subd_post_token) == 1,
	    "bot subd observer removal must succeed");

    event_order_observer bot_smooth_cpp_post;
    ged_event_observer_token bot_smooth_cpp_post_token =
	ged_event_observer_add(gedp, GED_EVENT_OBSERVER_POST_RECONCILE,
		event_order_cb, &bot_smooth_cpp_post);
    CHECK(bot_smooth_cpp_post_token != 0,
	    "bot smooth observer must register");
    const char *bot_smooth_cpp_av[7] = {"bot", "smooth", "-I", "1",
	bot_cpp_event_src, bot_smooth_cpp_event_dst, NULL};
    CHECK(ged_exec(gedp, 6, bot_smooth_cpp_av) == BRLCAD_OK,
	    "bot smooth output command must publish object creation event");
    CHECK(bot_smooth_cpp_post.calls == 1,
	    "bot smooth output command must publish one event transaction");
    CHECK(observed_named_event(bot_smooth_cpp_post, GED_EVENT_OBJECT_ADDED,
		bot_smooth_cpp_event_dst),
	    "bot smooth output command must emit object-added event");
    CHECK(db_lookup(gedp->dbip, bot_smooth_cpp_event_dst, LOOKUP_QUIET) !=
	    RT_DIR_NULL,
	    "bot smooth output object must exist after command");
    CHECK(std::find(bot_smooth_cpp_post.all_kinds.begin(),
	    bot_smooth_cpp_post.all_kinds.end(), GED_EVENT_OBJECT_REMOVED) ==
	    bot_smooth_cpp_post.all_kinds.end(),
	    "bot smooth output fixture must not publish removals");
    CHECK(ged_event_observer_remove(gedp, bot_smooth_cpp_post_token) == 1,
	    "bot smooth observer removal must succeed");

    event_order_observer bot_repair_cpp_post;
    ged_event_observer_token bot_repair_cpp_post_token =
	ged_event_observer_add(gedp, GED_EVENT_OBSERVER_POST_RECONCILE,
		event_order_cb, &bot_repair_cpp_post);
    CHECK(bot_repair_cpp_post_token != 0,
	    "bot repair observer must register");
    const char *bot_repair_cpp_av[8] = {"bot", "repair", "-p", "100",
	"-o", bot_repair_cpp_event_dst, bot_repair_cpp_event_src, NULL};
    CHECK(ged_exec(gedp, 7, bot_repair_cpp_av) == BRLCAD_OK,
	    "bot repair output command must publish object creation event");
    CHECK(bot_repair_cpp_post.calls == 1,
	    "bot repair output command must publish one event transaction");
    CHECK(observed_named_event(bot_repair_cpp_post, GED_EVENT_OBJECT_ADDED,
		bot_repair_cpp_event_dst),
	    "bot repair output command must emit object-added event");
    CHECK(db_lookup(gedp->dbip, bot_repair_cpp_event_dst, LOOKUP_QUIET) !=
	    RT_DIR_NULL,
	    "bot repair output object must exist after command");
    CHECK(std::find(bot_repair_cpp_post.all_kinds.begin(),
	    bot_repair_cpp_post.all_kinds.end(), GED_EVENT_OBJECT_REMOVED) ==
	    bot_repair_cpp_post.all_kinds.end(),
	    "bot repair output fixture must not publish removals");
    CHECK(ged_event_observer_remove(gedp, bot_repair_cpp_post_token) == 1,
	    "bot repair observer removal must succeed");

    event_order_observer bot_remesh_post;
    ged_event_observer_token bot_remesh_post_token =
	ged_event_observer_add(gedp, GED_EVENT_OBSERVER_POST_RECONCILE,
		event_order_cb, &bot_remesh_post);
    CHECK(bot_remesh_post_token != 0,
	    "bot remesh observer must register");
    const char *bot_remesh_av[5] = {"bot", "remesh", bot_remesh_event_src,
	bot_remesh_event_dst, NULL};
    CHECK(ged_exec(gedp, 4, bot_remesh_av) == BRLCAD_OK,
	    "bot remesh output command must publish object creation event");
    CHECK(bot_remesh_post.calls == 1,
	    "bot remesh output command must publish one event transaction");
    CHECK(observed_named_event(bot_remesh_post, GED_EVENT_OBJECT_ADDED,
		bot_remesh_event_dst),
	    "bot remesh output command must emit object-added event");
    CHECK(db_lookup(gedp->dbip, bot_remesh_event_dst, LOOKUP_QUIET) !=
	    RT_DIR_NULL,
	    "bot remesh output object must exist after command");
    CHECK(std::find(bot_remesh_post.all_kinds.begin(),
	    bot_remesh_post.all_kinds.end(), GED_EVENT_OBJECT_REMOVED) ==
	    bot_remesh_post.all_kinds.end(),
	    "bot remesh output fixture must not publish removals");
    CHECK(ged_event_observer_remove(gedp, bot_remesh_post_token) == 1,
	    "bot remesh observer removal must succeed");

    event_order_observer bot_extrude_post;
    ged_event_observer_token bot_extrude_post_token =
	ged_event_observer_add(gedp, GED_EVENT_OBSERVER_POST_RECONCILE,
		event_order_cb, &bot_extrude_post);
    CHECK(bot_extrude_post_token != 0,
	    "bot extrude observer must register");
    const char *bot_extrude_av[7] = {"bot", "extrude", "-q", "-C",
	bot_extrude_event_src, bot_extrude_event_dst, NULL};
    CHECK(ged_exec(gedp, 6, bot_extrude_av) == BRLCAD_OK,
	    "bot extrude CSG command must publish object creation events");
    CHECK(bot_extrude_post.calls == 1,
	    "bot extrude CSG command must publish one event transaction");
    CHECK(observed_named_event(bot_extrude_post, GED_EVENT_OBJECT_ADDED,
		bot_extrude_event_dst),
	    "bot extrude CSG command must emit output object-added event");
    CHECK(db_lookup(gedp->dbip, bot_extrude_event_dst, LOOKUP_QUIET) !=
	    RT_DIR_NULL,
	    "bot extrude CSG output object must exist after command");
    CHECK(std::find(bot_extrude_post.all_kinds.begin(),
	    bot_extrude_post.all_kinds.end(), GED_EVENT_OBJECT_REMOVED) ==
	    bot_extrude_post.all_kinds.end(),
	    "bot extrude CSG fixture must not publish removals");
    CHECK(ged_event_observer_remove(gedp, bot_extrude_post_token) == 1,
	    "bot extrude observer removal must succeed");

    event_order_observer bot_exterior_post;
    ged_event_observer_token bot_exterior_post_token =
	ged_event_observer_add(gedp, GED_EVENT_OBSERVER_POST_RECONCILE,
		event_order_cb, &bot_exterior_post);
    CHECK(bot_exterior_post_token != 0,
	    "bot exterior observer must register");
    const char *bot_exterior_av[7] = {"bot", "exterior", "-c", "100",
	bot_exterior_event_src, bot_exterior_event_dst, NULL};
    CHECK(ged_exec(gedp, 6, bot_exterior_av) == BRLCAD_OK,
	    "bot exterior output command must publish object creation event");
    CHECK(bot_exterior_post.calls == 1,
	    "bot exterior output command must publish one event transaction");
    CHECK(observed_named_event(bot_exterior_post, GED_EVENT_OBJECT_ADDED,
		bot_exterior_event_dst),
	    "bot exterior output command must emit object-added event");
    CHECK(db_lookup(gedp->dbip, bot_exterior_event_dst, LOOKUP_QUIET) !=
	    RT_DIR_NULL,
	    "bot exterior output object must exist after command");
    CHECK(std::find(bot_exterior_post.all_kinds.begin(),
	    bot_exterior_post.all_kinds.end(), GED_EVENT_OBJECT_REMOVED) ==
	    bot_exterior_post.all_kinds.end(),
	    "bot exterior output fixture must not publish removals");
    CHECK(ged_event_observer_remove(gedp, bot_exterior_post_token) == 1,
	    "bot exterior observer removal must succeed");

    event_order_observer bot_flip_legacy_post;
    ged_event_observer_token bot_flip_legacy_post_token =
	ged_event_observer_add(gedp, GED_EVENT_OBSERVER_POST_RECONCILE,
		event_order_cb, &bot_flip_legacy_post);
    CHECK(bot_flip_legacy_post_token != 0,
	    "legacy bot_flip observer must register");
    const char *bot_flip_legacy_av[3] = {"bot_flip", bot_pca_event_dst, NULL};
    CHECK(ged_exec(gedp, 2, bot_flip_legacy_av) == BRLCAD_OK,
	    "legacy bot_flip command must publish object-modified event");
    CHECK(bot_flip_legacy_post.calls == 1,
	    "legacy bot_flip command must publish one event transaction");
    CHECK(observed_named_event(bot_flip_legacy_post,
		GED_EVENT_OBJECT_MODIFIED, bot_pca_event_dst),
	    "legacy bot_flip command must emit object-modified event");
    CHECK(!observed_named_event(bot_flip_legacy_post,
		GED_EVENT_OBJECT_ADDED, bot_pca_event_dst),
	    "legacy bot_flip command must not report an in-place edit as an add");
    CHECK(ged_event_observer_remove(gedp, bot_flip_legacy_post_token) == 1,
	    "legacy bot_flip observer removal must succeed");

    event_order_observer bot_condense_post;
    ged_event_observer_token bot_condense_post_token =
	ged_event_observer_add(gedp, GED_EVENT_OBSERVER_POST_RECONCILE,
		event_order_cb, &bot_condense_post);
    CHECK(bot_condense_post_token != 0,
	    "bot_condense observer must register");
    const char *bot_condense_av[4] = {"bot_condense",
	bot_condense_event_dst, bot_pca_event_dst, NULL};
    CHECK(ged_exec(gedp, 3, bot_condense_av) == BRLCAD_OK,
	    "bot_condense command must publish object creation event");
    CHECK(bot_condense_post.calls == 1,
	    "bot_condense command must publish one event transaction");
    CHECK(observed_named_event(bot_condense_post, GED_EVENT_OBJECT_ADDED,
		bot_condense_event_dst),
	    "bot_condense command must emit object-added event");
    CHECK(ged_event_observer_remove(gedp, bot_condense_post_token) == 1,
	    "bot_condense observer removal must succeed");

    event_order_observer bot_vertex_fuse_post;
    ged_event_observer_token bot_vertex_fuse_post_token =
	ged_event_observer_add(gedp, GED_EVENT_OBSERVER_POST_RECONCILE,
		event_order_cb, &bot_vertex_fuse_post);
    CHECK(bot_vertex_fuse_post_token != 0,
	    "bot_vertex_fuse observer must register");
    const char *bot_vertex_fuse_av[4] = {"bot_vertex_fuse",
	bot_vertex_fuse_event_dst, bot_condense_event_dst, NULL};
    CHECK(ged_exec(gedp, 3, bot_vertex_fuse_av) == BRLCAD_OK,
	    "bot_vertex_fuse command must publish object creation event");
    CHECK(bot_vertex_fuse_post.calls == 1,
	    "bot_vertex_fuse command must publish one event transaction");
    CHECK(observed_named_event(bot_vertex_fuse_post,
		GED_EVENT_OBJECT_ADDED, bot_vertex_fuse_event_dst),
	    "bot_vertex_fuse command must emit object-added event");
    CHECK(ged_event_observer_remove(gedp, bot_vertex_fuse_post_token) == 1,
	    "bot_vertex_fuse observer removal must succeed");

    event_order_observer bot_face_fuse_post;
    ged_event_observer_token bot_face_fuse_post_token =
	ged_event_observer_add(gedp, GED_EVENT_OBSERVER_POST_RECONCILE,
		event_order_cb, &bot_face_fuse_post);
    CHECK(bot_face_fuse_post_token != 0,
	    "bot_face_fuse observer must register");
    const char *bot_face_fuse_av[4] = {"bot_face_fuse",
	bot_face_fuse_event_dst, bot_vertex_fuse_event_dst, NULL};
    CHECK(ged_exec(gedp, 3, bot_face_fuse_av) == BRLCAD_OK,
	    "bot_face_fuse command must publish object creation event");
    CHECK(bot_face_fuse_post.calls == 1,
	    "bot_face_fuse command must publish one event transaction");
    CHECK(observed_named_event(bot_face_fuse_post,
		GED_EVENT_OBJECT_ADDED, bot_face_fuse_event_dst),
	    "bot_face_fuse command must emit object-added event");
    CHECK(ged_event_observer_remove(gedp, bot_face_fuse_post_token) == 1,
	    "bot_face_fuse observer removal must succeed");

    event_order_observer bot_face_sort_post;
    ged_event_observer_token bot_face_sort_post_token =
	ged_event_observer_add(gedp, GED_EVENT_OBSERVER_POST_RECONCILE,
		event_order_cb, &bot_face_sort_post);
    CHECK(bot_face_sort_post_token != 0,
	    "bot_face_sort observer must register");
    const char *bot_face_sort_av[4] = {"bot_face_sort", "1",
	bot_face_fuse_event_dst, NULL};
    CHECK(ged_exec(gedp, 3, bot_face_sort_av) == BRLCAD_OK,
	    "bot_face_sort command must publish object-modified event");
    CHECK(bot_face_sort_post.calls == 1,
	    "bot_face_sort command must publish one event transaction");
    CHECK(observed_named_event(bot_face_sort_post,
		GED_EVENT_OBJECT_MODIFIED, bot_face_fuse_event_dst),
	    "bot_face_sort command must emit object-modified event");
    CHECK(!observed_named_event(bot_face_sort_post,
		GED_EVENT_OBJECT_ADDED, bot_face_fuse_event_dst),
	    "bot_face_sort command must not report an in-place edit as an add");
    CHECK(ged_event_observer_remove(gedp, bot_face_sort_post_token) == 1,
	    "bot_face_sort observer removal must succeed");

    event_order_observer bot_sync_legacy_post;
    ged_event_observer_token bot_sync_legacy_post_token =
	ged_event_observer_add(gedp, GED_EVENT_OBSERVER_POST_RECONCILE,
		event_order_cb, &bot_sync_legacy_post);
    CHECK(bot_sync_legacy_post_token != 0,
	    "legacy bot_sync observer must register");
    const char *bot_sync_legacy_av[3] = {"bot_sync", bot_face_fuse_event_dst,
	NULL};
    CHECK(ged_exec(gedp, 2, bot_sync_legacy_av) == BRLCAD_OK,
	    "legacy bot_sync command must publish object-modified event");
    CHECK(bot_sync_legacy_post.calls == 1,
	    "legacy bot_sync command must publish one event transaction");
    CHECK(observed_named_event(bot_sync_legacy_post,
		GED_EVENT_OBJECT_MODIFIED, bot_face_fuse_event_dst),
	    "legacy bot_sync command must emit object-modified event");
    CHECK(!observed_named_event(bot_sync_legacy_post,
		GED_EVENT_OBJECT_ADDED, bot_face_fuse_event_dst),
	    "legacy bot_sync command must not report an in-place edit as an add");
    CHECK(ged_event_observer_remove(gedp, bot_sync_legacy_post_token) == 1,
	    "legacy bot_sync observer removal must succeed");

    event_order_observer bot_smooth_post;
    ged_event_observer_token bot_smooth_post_token =
	ged_event_observer_add(gedp, GED_EVENT_OBSERVER_POST_RECONCILE,
		event_order_cb, &bot_smooth_post);
    CHECK(bot_smooth_post_token != 0,
	    "legacy bot_smooth observer must register");
    const char *bot_smooth_av[4] = {"bot_smooth", bot_smooth_event_dst,
	bot_smooth_event_src, NULL};
    CHECK(ged_exec(gedp, 3, bot_smooth_av) == BRLCAD_OK,
	    "legacy bot_smooth command must publish object creation event");
    CHECK(bot_smooth_post.calls == 1,
	    "legacy bot_smooth command must publish one event transaction");
    CHECK(observed_named_event(bot_smooth_post, GED_EVENT_OBJECT_ADDED,
		bot_smooth_event_dst),
	    "legacy bot_smooth command must emit object-added event");
    CHECK(ged_event_observer_remove(gedp, bot_smooth_post_token) == 1,
	    "legacy bot_smooth observer removal must succeed");

    event_order_observer bot_decimate_post;
    ged_event_observer_token bot_decimate_post_token =
	ged_event_observer_add(gedp, GED_EVENT_OBSERVER_POST_RECONCILE,
		event_order_cb, &bot_decimate_post);
    CHECK(bot_decimate_post_token != 0,
	    "bot_decimate observer must register");
    const char *bot_decimate_av[6] = {"bot_decimate", "-f", "0",
	bot_decimate_event_dst, bot_face_fuse_event_dst, NULL};
    CHECK(ged_exec(gedp, 5, bot_decimate_av) == BRLCAD_OK,
	    "bot_decimate command must publish object creation event");
    CHECK(bot_decimate_post.calls == 1,
	    "bot_decimate command must publish one event transaction");
    CHECK(observed_named_event(bot_decimate_post, GED_EVENT_OBJECT_ADDED,
		bot_decimate_event_dst),
	    "bot_decimate command must emit object-added event");
    CHECK(ged_event_observer_remove(gedp, bot_decimate_post_token) == 1,
	    "bot_decimate observer removal must succeed");

    event_order_observer bot_merge_post;
    ged_event_observer_token bot_merge_post_token =
	ged_event_observer_add(gedp, GED_EVENT_OBSERVER_POST_RECONCILE,
		event_order_cb, &bot_merge_post);
    CHECK(bot_merge_post_token != 0,
	    "bot_merge observer must register");
    const char *bot_merge_av[5] = {"bot_merge", bot_merge_event_dst,
	bot_pca_event_dst, bot_decimate_event_dst, NULL};
    CHECK(ged_exec(gedp, 4, bot_merge_av) == BRLCAD_OK,
	    "bot_merge command must publish object creation event");
    CHECK(bot_merge_post.calls == 1,
	    "bot_merge command must publish one event transaction");
    CHECK(observed_named_event(bot_merge_post, GED_EVENT_OBJECT_ADDED,
		bot_merge_event_dst),
	    "bot_merge command must emit object-added event");
    CHECK(ged_event_observer_remove(gedp, bot_merge_post_token) == 1,
	    "bot_merge observer removal must succeed");

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

    event_order_observer lint_group_post;
    ged_event_observer_token lint_group_post_token =
	ged_event_observer_add(gedp, GED_EVENT_OBSERVER_POST_RECONCILE,
		event_order_cb, &lint_group_post);
    CHECK(lint_group_post_token != 0,
	    "lint diagnostic group observer must register");
    const char *lint_group_av[7] = {"lint", "-I", "bot:not_solid", "-g",
	lint_group, lint_bad_bot, NULL};
    CHECK(ged_exec(gedp, 6, lint_group_av) == BRLCAD_OK,
	    "lint diagnostic group command must publish object creation event");
    CHECK(lint_group_post.calls == 1,
	    "lint diagnostic group command must publish one event transaction");
    CHECK(observed_named_event(lint_group_post, GED_EVENT_OBJECT_ADDED,
		lint_group),
	    "lint diagnostic group command must emit object-added event");
    CHECK(db_lookup(gedp->dbip, lint_group, LOOKUP_QUIET) != RT_DIR_NULL,
	    "lint diagnostic group command must create output comb");
    CHECK(std::find(lint_group_post.all_kinds.begin(),
	    lint_group_post.all_kinds.end(), GED_EVENT_OBJECT_REMOVED) ==
	    lint_group_post.all_kinds.end(),
	    "lint diagnostic group fixture must not publish removals");
    CHECK(ged_event_observer_remove(gedp, lint_group_post_token) == 1,
	    "lint diagnostic group observer removal must succeed");

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

    event_order_observer nmg_collapse_post;
    ged_event_observer_token nmg_collapse_post_token =
	ged_event_observer_add(gedp, GED_EVENT_OBSERVER_POST_RECONCILE,
		event_order_cb, &nmg_collapse_post);
    CHECK(nmg_collapse_post_token != 0,
	    "nmg_collapse observer must register");
    const char *nmg_collapse_av[5] = {"nmg_collapse",
	nmg_collapse_event_src, nmg_collapse_event_dst, "0.01", NULL};
    CHECK(ged_exec(gedp, 4, nmg_collapse_av) == BRLCAD_OK,
	    "nmg_collapse command must publish object-added event");
    CHECK(nmg_collapse_post.calls == 1,
	    "nmg_collapse command must publish one event transaction");
    CHECK(observed_named_event(nmg_collapse_post, GED_EVENT_OBJECT_ADDED,
		nmg_collapse_event_dst),
	    "nmg_collapse command must emit object-added event");
    CHECK(std::find(nmg_collapse_post.all_kinds.begin(),
	    nmg_collapse_post.all_kinds.end(), GED_EVENT_OBJECT_REMOVED) ==
	    nmg_collapse_post.all_kinds.end(),
	    "nmg_collapse creation fixture must not publish removals");
    CHECK(ged_event_observer_remove(gedp, nmg_collapse_post_token) == 1,
	    "nmg_collapse observer removal must succeed");

    event_order_observer nmg_simplify_post;
    ged_event_observer_token nmg_simplify_post_token =
	ged_event_observer_add(gedp, GED_EVENT_OBSERVER_POST_RECONCILE,
		event_order_cb, &nmg_simplify_post);
    CHECK(nmg_simplify_post_token != 0,
	    "nmg_simplify observer must register");
    const char *nmg_simplify_av[5] = {"nmg_simplify", "arb",
	nmg_simplify_event_dst, nmg_simplify_event_src, NULL};
    CHECK(ged_exec(gedp, 4, nmg_simplify_av) == BRLCAD_OK,
	    "nmg_simplify command must publish object-added event");
    CHECK(nmg_simplify_post.calls == 1,
	    "nmg_simplify command must publish one event transaction");
    CHECK(observed_named_event(nmg_simplify_post, GED_EVENT_OBJECT_ADDED,
		nmg_simplify_event_dst),
	    "nmg_simplify command must emit object-added event");
    CHECK(std::find(nmg_simplify_post.all_kinds.begin(),
	    nmg_simplify_post.all_kinds.end(), GED_EVENT_OBJECT_REMOVED) ==
	    nmg_simplify_post.all_kinds.end(),
	    "nmg_simplify creation fixture must not publish removals");
    CHECK(ged_event_observer_remove(gedp, nmg_simplify_post_token) == 1,
	    "nmg_simplify observer removal must succeed");

    event_order_observer nmg_mm_post;
    ged_event_observer_token nmg_mm_post_token =
	ged_event_observer_add(gedp, GED_EVENT_OBSERVER_POST_RECONCILE,
		event_order_cb, &nmg_mm_post);
    CHECK(nmg_mm_post_token != 0,
	    "nmg_mm observer must register");
    const char *nmg_mm_av[3] = {"nmg_mm", nmg_mm_event_dst, NULL};
    CHECK(ged_exec(gedp, 2, nmg_mm_av) == BRLCAD_OK,
	    "nmg_mm command must publish object-added event");
    CHECK(nmg_mm_post.calls == 1,
	    "nmg_mm command must publish one event transaction");
    CHECK(observed_named_event(nmg_mm_post, GED_EVENT_OBJECT_ADDED,
		nmg_mm_event_dst),
	    "nmg_mm command must emit object-added event");
    CHECK(std::find(nmg_mm_post.all_kinds.begin(),
	    nmg_mm_post.all_kinds.end(), GED_EVENT_OBJECT_REMOVED) ==
	    nmg_mm_post.all_kinds.end(),
	    "nmg_mm creation fixture must not publish removals");
    CHECK(ged_event_observer_remove(gedp, nmg_mm_post_token) == 1,
	    "nmg_mm observer removal must succeed");

    const char *nmg_inplace_mm_av[3] = {"nmg_mm", nmg_inplace_event_obj,
	NULL};
    CHECK(ged_exec(gedp, 2, nmg_inplace_mm_av) == BRLCAD_OK,
	    "nmg in-place fixture must create an empty NMG");

    event_order_observer nmg_cmface_post;
    ged_event_observer_token nmg_cmface_post_token =
	ged_event_observer_add(gedp, GED_EVENT_OBSERVER_POST_RECONCILE,
		event_order_cb, &nmg_cmface_post);
    CHECK(nmg_cmface_post_token != 0,
	    "nmg cmface observer must register");
    const char *nmg_cmface_av[13] = {"nmg", nmg_inplace_event_obj,
	"cmface", "0", "0", "0", "1", "0", "0", "0", "1", "0",
	NULL};
    CHECK(ged_exec(gedp, 12, nmg_cmface_av) == BRLCAD_OK,
	    "nmg cmface command must publish object-modified event");
    CHECK(nmg_cmface_post.calls == 1,
	    "nmg cmface command must publish one event transaction");
    CHECK(observed_named_event(nmg_cmface_post,
		GED_EVENT_OBJECT_MODIFIED, nmg_inplace_event_obj),
	    "nmg cmface command must emit object-modified event");
    CHECK(!observed_named_event(nmg_cmface_post,
		GED_EVENT_OBJECT_ADDED, nmg_inplace_event_obj),
	    "nmg cmface command must not report in-place edit as add");
    CHECK(ged_event_observer_remove(gedp, nmg_cmface_post_token) == 1,
	    "nmg cmface observer removal must succeed");

    event_order_observer nmg_make_v_post;
    ged_event_observer_token nmg_make_v_post_token =
	ged_event_observer_add(gedp, GED_EVENT_OBSERVER_POST_RECONCILE,
		event_order_cb, &nmg_make_v_post);
    CHECK(nmg_make_v_post_token != 0,
	    "nmg make V observer must register");
    const char *nmg_make_v_av[8] = {"nmg", nmg_inplace_event_obj,
	"make", "V", "2", "0", "0", NULL};
    CHECK(ged_exec(gedp, 7, nmg_make_v_av) == BRLCAD_OK,
	    "nmg make V command must publish object-modified event");
    CHECK(nmg_make_v_post.calls == 1,
	    "nmg make V command must publish one event transaction");
    CHECK(observed_named_event(nmg_make_v_post,
		GED_EVENT_OBJECT_MODIFIED, nmg_inplace_event_obj),
	    "nmg make V command must emit object-modified event");
    CHECK(!observed_named_event(nmg_make_v_post,
		GED_EVENT_OBJECT_ADDED, nmg_inplace_event_obj),
	    "nmg make V command must not report in-place edit as add");
    CHECK(ged_event_observer_remove(gedp, nmg_make_v_post_token) == 1,
	    "nmg make V observer removal must succeed");

    event_order_observer nmg_move_v_post;
    ged_event_observer_token nmg_move_v_post_token =
	ged_event_observer_add(gedp, GED_EVENT_OBSERVER_POST_RECONCILE,
		event_order_cb, &nmg_move_v_post);
    CHECK(nmg_move_v_post_token != 0,
	    "nmg move V observer must register");
    const char *nmg_move_v_av[11] = {"nmg", nmg_inplace_event_obj,
	"move", "V", "2", "0", "0", "2", "1", "0", NULL};
    CHECK(ged_exec(gedp, 10, nmg_move_v_av) == BRLCAD_OK,
	    "nmg move V command must publish object-modified event");
    CHECK(nmg_move_v_post.calls == 1,
	    "nmg move V command must publish one event transaction");
    CHECK(observed_named_event(nmg_move_v_post,
		GED_EVENT_OBJECT_MODIFIED, nmg_inplace_event_obj),
	    "nmg move V command must emit object-modified event");
    CHECK(!observed_named_event(nmg_move_v_post,
		GED_EVENT_OBJECT_ADDED, nmg_inplace_event_obj),
	    "nmg move V command must not report in-place edit as add");
    CHECK(ged_event_observer_remove(gedp, nmg_move_v_post_token) == 1,
	    "nmg move V observer removal must succeed");

    event_order_observer nmg_kill_v_post;
    ged_event_observer_token nmg_kill_v_post_token =
	ged_event_observer_add(gedp, GED_EVENT_OBSERVER_POST_RECONCILE,
		event_order_cb, &nmg_kill_v_post);
    CHECK(nmg_kill_v_post_token != 0,
	    "nmg kill V observer must register");
    const char *nmg_kill_v_av[8] = {"nmg", nmg_inplace_event_obj,
	"kill", "V", "2", "1", "0", NULL};
    CHECK(ged_exec(gedp, 7, nmg_kill_v_av) == BRLCAD_OK,
	    "nmg kill V command must publish object-modified event");
    CHECK(nmg_kill_v_post.calls == 1,
	    "nmg kill V command must publish one event transaction");
    CHECK(observed_named_event(nmg_kill_v_post,
		GED_EVENT_OBJECT_MODIFIED, nmg_inplace_event_obj),
	    "nmg kill V command must emit object-modified event");
    CHECK(!observed_named_event(nmg_kill_v_post,
		GED_EVENT_OBJECT_ADDED, nmg_inplace_event_obj),
	    "nmg kill V command must not report in-place edit as add");
    CHECK(ged_event_observer_remove(gedp, nmg_kill_v_post_token) == 1,
	    "nmg kill V observer removal must succeed");

    long nmg_kill_face_index =
	first_nmg_face_index(gedp, nmg_inplace_event_obj);
    CHECK(nmg_kill_face_index >= 0,
	    "nmg kill F fixture must find an existing face");
    char nmg_kill_face_arg[64] = {0};
    std::snprintf(nmg_kill_face_arg, sizeof(nmg_kill_face_arg), "%ld",
	    nmg_kill_face_index);
    event_order_observer nmg_kill_f_post;
    ged_event_observer_token nmg_kill_f_post_token =
	ged_event_observer_add(gedp, GED_EVENT_OBSERVER_POST_RECONCILE,
		event_order_cb, &nmg_kill_f_post);
    CHECK(nmg_kill_f_post_token != 0,
	    "nmg kill F observer must register");
    const char *nmg_kill_f_av[6] = {"nmg", nmg_inplace_event_obj,
	"kill", "F", nmg_kill_face_arg, NULL};
    CHECK(nmg_kill_face_index >= 0 &&
	    ged_exec(gedp, 5, nmg_kill_f_av) == BRLCAD_OK,
	    "nmg kill F command must publish object-modified event");
    CHECK(nmg_kill_f_post.calls == 1,
	    "nmg kill F command must publish one event transaction");
    CHECK(observed_named_event(nmg_kill_f_post,
		GED_EVENT_OBJECT_MODIFIED, nmg_inplace_event_obj),
	    "nmg kill F command must emit object-modified event");
    CHECK(!observed_named_event(nmg_kill_f_post,
		GED_EVENT_OBJECT_ADDED, nmg_inplace_event_obj),
	    "nmg kill F command must not report in-place edit as add");
    CHECK(ged_event_observer_remove(gedp, nmg_kill_f_post_token) == 1,
	    "nmg kill F observer removal must succeed");

    event_order_observer group_post;
    ged_event_observer_token group_post_token =
	ged_event_observer_add(gedp, GED_EVENT_OBSERVER_POST_RECONCILE,
		event_order_cb, &group_post);
    CHECK(group_post_token != 0,
	    "group command observer must register");
    const char *group_av[4] = {"g", group_event_dst, comb_event_child,
	NULL};
    CHECK(ged_exec(gedp, 3, group_av) == BRLCAD_OK,
	    "group command creation must publish object-added event");
    CHECK(group_post.calls == 1,
	    "group command creation must publish one event transaction");
    CHECK(observed_named_event(group_post, GED_EVENT_OBJECT_ADDED,
		group_event_dst),
	    "group command creation must emit object-added event");
    CHECK(!observed_named_event(group_post, GED_EVENT_COMB_TREE_CHANGED,
		group_event_dst),
	    "group command creation must not report new comb as tree edit");
    CHECK(ged_event_observer_remove(gedp, group_post_token) == 1,
	    "group command observer removal must succeed");

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

    const char *facet_modern_child = "_ged_event_facetize_modern_child.s";
    const char *facet_modern_region = "_ged_event_facetize_modern_region.r";
    const char *facet_modern_output = "_ged_event_facetize_modern_output.c";
    if (wdbp) {
	point_t facet_modern_center = {625.0, 0.0, 0.0};
	struct wmember facet_modern_wm;
	BU_LIST_INIT(&facet_modern_wm.l);
	CHECK(mk_sph(wdbp, facet_modern_child, facet_modern_center, 1.0) == 0,
		"facetize modern event fixture child must be created");
	CHECK(mk_addmember(facet_modern_child, &facet_modern_wm.l, NULL,
		    WMOP_UNION) != NULL,
		"facetize modern event fixture must add child");
	CHECK(mk_comb(wdbp, facet_modern_region, &facet_modern_wm.l, 1,
		    NULL, NULL, NULL, 0, 0, 0, 0, 0, 0, 0) == 0,
		"facetize modern event fixture region must be created");
    }

    event_order_observer facet_modern_post;
    ged_event_observer_token facet_modern_post_token =
	ged_event_observer_add(gedp, GED_EVENT_OBSERVER_POST_RECONCILE,
		event_order_cb, &facet_modern_post);
    CHECK(facet_modern_post_token != 0,
	    "facetize modern observer must register");
    const char *facet_modern_av[8] = {"facetize", "-q", "-r",
	"--methods", "NMG", facet_modern_region, facet_modern_output, NULL};
    int facet_modern_ret = ged_exec(gedp, 7, facet_modern_av);
    if (facet_modern_ret != BRLCAD_OK)
	bu_log("facetize modern result: %s\n",
		bu_vls_cstr(gedp->ged_result_str));
    CHECK(facet_modern_ret == BRLCAD_OK,
	    "facetize modern region copy must succeed");
    CHECK(observed_named_event(facet_modern_post, GED_EVENT_OBJECT_ADDED,
		facet_modern_output),
	    "facetize modern output comb must emit object-added event");
    CHECK(facet_modern_post.calls == 1,
	    "facetize modern import and output comb must be one transaction");
    CHECK(facet_modern_post.all_affected_names.find(facet_modern_output) !=
	    std::string::npos,
	    "facetize modern event must identify output comb");
    CHECK(db_lookup(gedp->dbip, facet_modern_output, LOOKUP_QUIET) !=
	    RT_DIR_NULL,
	    "facetize modern output comb must be present");
    CHECK(ged_event_observer_remove(gedp, facet_modern_post_token) == 1,
	    "facetize modern observer removal must succeed");

    const char *facet_nmg_output = "_ged_event_facetize_nmg_output.nmg";
    event_order_observer facet_nmg_post;
    ged_event_observer_token facet_nmg_post_token =
	ged_event_observer_add(gedp, GED_EVENT_OBSERVER_POST_RECONCILE,
		event_order_cb, &facet_nmg_post);
    CHECK(facet_nmg_post_token != 0,
	    "facetize NMG output observer must register");
    const char *facet_nmg_av[6] = {"facetize", "-q", "-n",
	facet_modern_child, facet_nmg_output, NULL};
    int facet_nmg_ret = ged_exec(gedp, 5, facet_nmg_av);
    if (facet_nmg_ret != BRLCAD_OK)
	bu_log("facetize NMG result: %s\n",
		bu_vls_cstr(gedp->ged_result_str));
    CHECK(facet_nmg_ret == BRLCAD_OK,
	    "facetize NMG output command must succeed");
    CHECK(facet_nmg_post.calls == 1,
	    "facetize NMG output command must publish one event transaction");
    CHECK(observed_named_event(facet_nmg_post, GED_EVENT_OBJECT_ADDED,
		facet_nmg_output),
	    "facetize NMG output command must emit object-added event");
    CHECK(db_lookup(gedp->dbip, facet_nmg_output, LOOKUP_QUIET) !=
	    RT_DIR_NULL,
	    "facetize NMG output object must be present");
    CHECK(ged_event_observer_remove(gedp, facet_nmg_post_token) == 1,
	    "facetize NMG output observer removal must succeed");

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
