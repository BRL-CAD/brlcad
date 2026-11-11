/*                   T E S T _ D B _ I N D E X . C P P
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
 * You should have received a copy of the GNU Lesser General Public License
 * along with this file; see the file named COPYING for more information.
 */
/** @file test_db_index.cpp */

#include "common.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <set>
#include <string>
#include <vector>

#include "bu/app.h"
#include "bu/file.h"
#include "bu/hash.h"
#include "ged.h"
#include "ged/db_index.h"
#include "ged/event.h"
#include "ged/selection.h"
#include "wdb.h"

static int failures = 0;

#define CHECK(_condition, _message) do { \
	if (!(_condition)) { \
	    bu_log("FAIL [%s:%d] %s\n", __FILE__, __LINE__, (_message)); \
	    failures++; \
	} \
    } while (0)

static ged_db_index_id
name_id(const char *name)
{
    return bu_data_hash(name, std::strlen(name));
}

static int
create_fixture(const char *path)
{
    struct rt_wdb *wdbp = wdb_fopen(path);
    if (!wdbp)
	return 0;

    point_t center = VINIT_ZERO;
    struct wmember inner_members;
    struct wmember outer_members;
    mat_t first_matrix;
    mat_t second_matrix;
    BU_LIST_INIT(&inner_members.l);
    BU_LIST_INIT(&outer_members.l);
    MAT_IDN(first_matrix);
    MAT_IDN(second_matrix);
    MAT_DELTAS(first_matrix, 1.0, 2.0, 3.0);
    MAT_DELTAS(second_matrix, 4.0, 5.0, 6.0);

    int ok = (mk_sph(wdbp, "leaf.s", center, 1.0) == 0);
    ok = ok && (mk_addmember("leaf.s", &inner_members.l, first_matrix,
	WMOP_UNION) != NULL);
    ok = ok && (mk_addmember("leaf.s", &inner_members.l, second_matrix,
	WMOP_SUBTRACT) != NULL);
    ok = ok && (mk_addmember("missing.s", &inner_members.l, NULL,
	WMOP_UNION) != NULL);
    ok = ok && (mk_comb(wdbp, "inner.g", &inner_members.l, 0, NULL, NULL,
	NULL, 0, 0, 0, 0, 0, 0, 0) == 0);
    ok = ok && (mk_addmember("inner.g", &outer_members.l, NULL,
	WMOP_UNION) != NULL);
    ok = ok && (mk_addmember("inner.g", &outer_members.l, NULL,
	WMOP_UNION) != NULL);
    ok = ok && (mk_comb(wdbp, "outer.g", &outer_members.l, 0, NULL, NULL,
	NULL, 0, 0, 0, 0, 0, 0, 0) == 0);

    db_close(wdbp->dbip);
    return ok;
}

static void
check_children(struct ged *gedp)
{
    ged_db_index_id inner_id = name_id("inner.g");
    ged_db_index_id leaf_id = name_id("leaf.s");
    ged_db_index_id duplicate_leaf_id = 0;
    struct ged_db_index_record inner = {};
    CHECK(ged_db_index_record_get(gedp, inner_id, &inner) == 1,
	"inner combination record is available");
    CHECK(inner.is_comb == 1 && inner.child_count == 3,
	"inner combination reports all direct occurrences");

    for (size_t row = 0; row < inner.child_count; ++row) {
	struct ged_db_index_child child = {};
	CHECK(ged_db_index_child_at(gedp, inner_id, row, &child) == 1,
	    "child row is available");
	if (child.record.id == leaf_id) {
	    CHECK(!child.record.is_instance,
		"first child use retains the canonical id");
	    CHECK(child.matrix_valid && EQUAL(child.matrix[MDX], 1.0) &&
		EQUAL(child.matrix[MDY], 2.0) &&
		EQUAL(child.matrix[MDZ], 3.0),
		"first child use retains its matrix");
	}
	if (child.record.object_id == leaf_id && child.record.is_instance) {
	    duplicate_leaf_id = child.record.id;
	    CHECK(child.record.name && BU_STR_EQUAL(child.record.name,
		"leaf.s@1"), "duplicate child has an occurrence name");
	    CHECK(child.bool_op == DB_OP_SUBTRACT,
		"duplicate child retains its boolean operation");
	    CHECK(child.matrix_valid && EQUAL(child.matrix[MDX], 4.0) &&
		EQUAL(child.matrix[MDY], 5.0) &&
		EQUAL(child.matrix[MDZ], 6.0),
		"duplicate child retains its matrix");
	}
	if (child.record.name && BU_STR_EQUAL(child.record.name, "missing.s")) {
	    CHECK(child.record.valid && !child.record.dp,
		"invalid references remain queryable");
	}
    }
    CHECK(duplicate_leaf_id != 0,
	"duplicate child receives a distinct occurrence id");
    CHECK(ged_db_index_object_use_count(gedp, leaf_id) == 2,
	"reverse uses include both child occurrences");
    CHECK(ged_db_index_object_use_count(gedp, duplicate_leaf_id) == 2,
	"reverse use queries canonicalize occurrence ids");
}

static void
check_paths(struct ged *gedp)
{
    ged_db_index_id leaf_id = name_id("leaf.s");
    const char *path = "outer.g/inner.g@1/leaf.s@1";
    ged_db_index_id ids[3] = {0, 0, 0};
    CHECK(ged_db_index_path_resolve(gedp, path, NULL, 0) == 3,
	"occurrence-qualified path reports its depth");
    CHECK(ged_db_index_path_resolve(gedp, path, ids, 3) == 3,
	"occurrence-qualified path resolves to ids");
    CHECK(ids[1] != name_id("inner.g") && ids[2] != leaf_id,
	"resolved path preserves duplicate occurrence identity");

    struct bu_vls printed = BU_VLS_INIT_ZERO;
    CHECK(ged_db_index_path_print(gedp, &printed, ids, 3, 0, 0) == 1,
	"resolved path is printable");
    CHECK(BU_STR_EQUAL(bu_vls_cstr(&printed), path),
	"printed path preserves occurrence qualifiers");
    bu_vls_free(&printed);

    CHECK(ged_db_index_affected_path_count(gedp, leaf_id, 0) == 4,
	"affected paths include every nested duplicate occurrence");
    std::set<std::string> affected;
    for (size_t row = 0; row < 4; ++row) {
	size_t path_len = ged_db_index_affected_path_at(gedp, leaf_id, row,
	    NULL, 0, 0);
	std::vector<ged_db_index_id> affected_ids(path_len);
	CHECK(ged_db_index_affected_path_at(gedp, leaf_id, row,
	    affected_ids.data(), affected_ids.size(), 0) == path_len,
	    "affected path ids are available");
	struct bu_vls affected_path = BU_VLS_INIT_ZERO;
	CHECK(ged_db_index_path_print(gedp, &affected_path,
	    affected_ids.data(), affected_ids.size(), 0, 0) == 1,
	    "affected path is printable");
	affected.insert(bu_vls_cstr(&affected_path));
	bu_vls_free(&affected_path);
    }
    CHECK(affected.size() == 4,
	"affected paths retain independent duplicate identities");
}

static void
check_removed_record(struct ged *gedp, const struct ged_event *events,
	size_t count, const struct ged_event_result *UNUSED(result),
	void *UNUSED(client_data))
{
    for (size_t i = 0; i < count; i++) {
	if (events[i].kind != GED_EVENT_OBJECT_REMOVED)
	    continue;

	struct ged_db_index_record record = {};
	CHECK(ged_db_index_record_get(gedp, name_id(events[i].name), &record)
	    && !record.dp, "removal observer sees an invalid reference");
    }
}


static void
check_updates(struct ged *gedp)
{
    struct rt_wdb *wdbp = wdb_dbopen(gedp->dbip, RT_WDB_TYPE_DB_DISK);
    CHECK(wdbp != RT_WDB_NULL, "database writer is available");
    if (!wdbp)
	return;

    point_t center = {10.0, 0.0, 0.0};
    CHECK(mk_sph(wdbp, "added.s", center, 2.0) == 0,
	"direct librt addition succeeds");
    CHECK(ged_db_index_refresh_flags(gedp) & GED_DB_INDEX_REFRESH_DB_CHANGE,
	"GED events invalidate the hierarchy index after addition");
    struct ged_db_index_record added = {};
    CHECK(ged_db_index_record_get(gedp, name_id("added.s"), &added) == 1,
	"added object is visible after refresh");

    struct directory *added_dp = db_lookup(gedp->dbip, "added.s", LOOKUP_QUIET);
    CHECK(added_dp != RT_DIR_NULL &&
	db_rename(gedp->dbip, added_dp, "renamed.s") == 0,
	"direct librt rename succeeds");
    CHECK(ged_db_index_refresh_flags(gedp) & GED_DB_INDEX_REFRESH_DB_CHANGE,
	"rename bridge invalidates the hierarchy index");
    struct ged_db_index_record renamed = {};
    CHECK(ged_db_index_record_get(gedp, name_id("renamed.s"), &renamed) == 1,
	"renamed object is visible after refresh");
    CHECK(ged_db_index_record_get(gedp, name_id("added.s"), &added) == 1 &&
	!added.dp && added.name && BU_STR_EQUAL(added.name, "added.s"),
	"obsolete object id remains available as an invalid reference");

    const auto observer = ged_event_observer_add(gedp,
	GED_EVENT_OBSERVER_POST_RECONCILE, check_removed_record, nullptr);
    CHECK(observer != 0, "index removal observer registers");
    CHECK(renamed.dp && db_dirdelete(gedp->dbip, renamed.dp) == 0,
	"direct librt removal succeeds");
    CHECK(ged_db_index_record_get(gedp, name_id("renamed.s"), &renamed)
	&& !renamed.dp, "observer query cannot cache a removed directory entry");
    ged_event_observer_remove(gedp, observer);

    const auto revision = ged_db_index_refresh(gedp);
    CHECK(ged_db_index_path_resolve(gedp, "unknown.s", NULL, 0) == 0 &&
	ged_db_index_refresh(gedp) == revision,
	"a path miss does not rebuild a current index");
}

static void
check_inmem_writes(struct ged *gedp)
{
    struct rt_wdb *writer = wdb_dbopen(gedp->dbip, RT_WDB_TYPE_DB_INMEM);
    CHECK(writer != RT_WDB_NULL, "in-memory writer is available");
    if (!writer)
	return;

    const char *combination = "inmem.c";
    const char *children[] = {"leaf.s", "inner.g"};
    for (const char *child : children) {
	struct wmember members;
	BU_LIST_INIT(&members.l);
	CHECK(mk_addmember(child, &members.l, NULL, WMOP_UNION) != NULL &&
	    mk_lcomb(writer, combination, &members, 0, NULL, NULL, NULL, 0) == 0,
	    "in-memory combination write succeeds");
	struct ged_db_index_child record = {};
	CHECK(ged_db_index_child_at(gedp, name_id(combination), 0, &record) &&
	    record.record.object_id == name_id(child),
	    "index observes new in-memory combination contents");
    }
}


static void
check_primitive_reference_tops(struct ged *gedp)
{
    const char *sketch = "profile.skt";
    const char *extrusion = "profile_extrusion.s";
    const char *make[] = {"make", sketch, "sketch"};
    struct rt_wdb *writer = wdb_dbopen(gedp->dbip, RT_WDB_TYPE_DB_DISK);
    const point_t base = VINIT_ZERO;
    const vect_t height = {0.0, 0.0, 1.0};
    const vect_t u = {1.0, 0.0, 0.0};
    const vect_t v = {0.0, 1.0, 0.0};
    CHECK(writer && ged_exec(gedp, 3, make) == BRLCAD_OK &&
	mk_extrusion(writer, extrusion, sketch, base, height, u, v, 0) == 0,
	"create an extrusion with a sketch reference");
    std::vector<ged_db_index_id> tops(ged_db_index_tops(gedp, 0, NULL, 0));
    ged_db_index_tops(gedp, 0, tops.data(), tops.size());
    CHECK(std::find(tops.begin(), tops.end(), name_id(extrusion)) != tops.end() &&
	std::find(tops.begin(), tops.end(), name_id(sketch)) == tops.end(),
	"index tops account for non-combination references");
}


static void
check_index_lifecycle(struct ged *gedp, const char *path)
{
    struct ged_db_index_record record = {};
    CHECK(ged_db_index_record_get(gedp, name_id("outer.g"), &record) &&
	record.dp, "index contains a live directory entry before close");

    const char *close[] = {"closedb"};
    if (ged_exec(gedp, 1, close) != BRLCAD_OK) {
	CHECK(false, "database closes");
	return;
    }

    CHECK(!ged_db_index_available(gedp) &&
	!ged_db_index_record_get(gedp, name_id("outer.g"), &record) &&
	!record.dp && !record.name, "closed database exposes no stale index record");

    const char *open[] = {"opendb", path};
    CHECK(ged_exec(gedp, 2, open) == BRLCAD_OK, "database reopens");
    CHECK(!ged_db_index_record_get(gedp, name_id("renamed.s"), &record),
	"previous database's removed-name records are discarded");
}

int
main(int argc, char **argv)
{
    bu_setprogname(argv[0]);
    if (argc != 1)
	return 1;

    char path[MAXPATHLEN] = {0};
    FILE *temp = bu_temp_file(path, MAXPATHLEN);
    if (!temp)
	return 1;
    std::fclose(temp);
    if (!create_fixture(path)) {
	bu_file_delete(path);
	return 1;
    }

    struct ged *gedp = ged_open("db", path, 1);
    CHECK(gedp != NULL, "fixture opens in a GED context");
    if (gedp) {
	CHECK(ged_db_index_available(gedp) == 1,
	    "public database index is available");
	CHECK(ged_db_index_refresh(gedp) > 0,
	    "database index refresh reports a revision");
	std::vector<ged_db_index_id> tops(
	    ged_db_index_tops(gedp, 0, NULL, 0));
	CHECK(ged_db_index_tops(gedp, 0, tops.data(), tops.size()) == 1 &&
	    tops[0] == name_id("outer.g"),
	    "outer combination is the sole hierarchy top");
	check_children(gedp);
	check_paths(gedp);
	check_updates(gedp);
	check_inmem_writes(gedp);
	check_primitive_reference_tops(gedp);
	check_index_lifecycle(gedp, path);
	ged_close(gedp);
    }

    bu_file_delete(path);
    return failures ? 1 : 0;
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
