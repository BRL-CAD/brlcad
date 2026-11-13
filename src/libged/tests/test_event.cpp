/*                    T E S T _ E V E N T . C P P
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

#include <atomic>
#include <cstdio>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

#include "bu/app.h"
#include "bu/file.h"
#include "bv/lod.h"
#include "ged.h"

#include "../dbi.h"

static int failures = 0;

#define CHECK(expression, message) do { \
    if (!(expression)) { \
	bu_log("FAIL: %s\n", (message)); \
	failures++; \
    } \
} while (0)

struct observation {
    std::vector<std::string> calls;
    std::vector<std::string> names;
    std::vector<std::string> new_names;
    std::vector<enum ged_event_kind> kinds;
    ged_event_observer_token self = 0;
    ged_event_observer_token other = 0;
    bool publish_followup = false;
};

static void
record_events(struct ged *UNUSED(gedp), const struct ged_event *events,
	      size_t event_count, const struct ged_event_result *result,
	      void *client_data)
{
    observation *state = static_cast<observation *>(client_data);
    state->calls.push_back("record");
    CHECK(ged_event_result_coalesced_count(result) == event_count,
	"observer result describes delivered events");
    for (size_t i = 0; i < event_count; i++) {
	state->names.push_back(events[i].name ? events[i].name : "");
	state->new_names.push_back(events[i].new_name ? events[i].new_name : "");
	state->kinds.push_back(events[i].kind);
    }
}

static bool
observed_rename(const observation &state, const char *old_name,
	const char *new_name)
{
    for (size_t i = 0; i < state.kinds.size(); i++) {
	if (state.kinds[i] == GED_EVENT_OBJECT_RENAMED &&
		state.names[i] == old_name && state.new_names[i] == new_name)
	    return true;
    }
    return false;
}

static void
record_internal(struct ged *UNUSED(gedp),
		const struct ged_event *UNUSED(events),
		size_t UNUSED(event_count),
		const struct ged_event_result *UNUSED(result),
		void *client_data)
{
    observation *state = static_cast<observation *>(client_data);
    state->calls.push_back("internal");
}

static void
record_post(struct ged *UNUSED(gedp),
	    const struct ged_event *UNUSED(events),
	    size_t UNUSED(event_count),
	    const struct ged_event_result *UNUSED(result),
	    void *client_data)
{
    observation *state = static_cast<observation *>(client_data);
    state->calls.push_back("post");
}

static void
remove_observers(struct ged *gedp,
		 const struct ged_event *UNUSED(events),
		 size_t UNUSED(event_count),
		 const struct ged_event_result *UNUSED(result),
		 void *client_data)
{
    observation *state = static_cast<observation *>(client_data);
    state->calls.push_back("remove");
    CHECK(ged_event_observer_remove(gedp, state->self) == 1,
	"observer removes itself during dispatch");
    CHECK(ged_event_observer_remove(gedp, state->other) == 1,
	"observer removes a later observer during dispatch");
}

static void
publish_followup(struct ged *gedp, const struct ged_event *events,
		 size_t event_count,
		 const struct ged_event_result *UNUSED(result),
		 void *client_data)
{
    observation *state = static_cast<observation *>(client_data);
    state->calls.push_back(events[0].name ? events[0].name : "");
    if (!state->publish_followup || event_count != 1 ||
	!events[0].name || !BU_STR_EQUAL(events[0].name, "first"))
	return;
    state->publish_followup = false;
    struct ged_event event;
    ged_event_init(&event);
    event.kind = GED_EVENT_OBJECT_MODIFIED;
    event.name = "second";
    CHECK(ged_event_publish(gedp, &event, 1, nullptr) == GED_EVENT_OK,
	"reentrant publication is accepted");
}

static void
publish_in_batch(struct ged *gedp, const struct ged_event *events,
	size_t count, const struct ged_event_result *UNUSED(result),
	void *UNUSED(client_data))
{
    if (count != 1 || !BU_STR_EQUAL(events[0].name, "start"))
	return;

    CHECK(ged_event_batch_begin(gedp) == GED_EVENT_OK,
	"event observer can open a batch");
    struct ged_event event = {};
    event.kind = GED_EVENT_OBJECT_MODIFIED;
    event.name = "batched";
    CHECK(ged_event_publish(gedp, &event, 1, nullptr) == GED_EVENT_OK,
	"observer can publish into an open batch");
}


static void
test_reentrant_batch(struct ged *gedp)
{
    observation state;
    const auto publisher = ged_event_observer_add(gedp, GED_EVENT_OBSERVER_INTERNAL,
	publish_in_batch, nullptr);
    const auto recorder = ged_event_observer_add(gedp, GED_EVENT_OBSERVER_POST_RECONCILE,
	record_events, &state);

    struct ged_event event = {};
    event.kind = GED_EVENT_OBJECT_MODIFIED;
    event.name = "start";
    CHECK(ged_event_publish(gedp, &event, 1, nullptr) == GED_EVENT_OK &&
	state.names.size() == 1 && state.names.front() == "start",
	"a batch opened during dispatch defers its events");
    CHECK(ged_event_batch_end(gedp, nullptr) == GED_EVENT_OK &&
	state.names.size() == 2 && state.names.back() == "batched",
	"deferred events dispatch when the outer batch closes");

    ged_event_observer_remove(gedp, recorder);
    ged_event_observer_remove(gedp, publisher);
}


struct dispatch_state {
    int calls = 0;
    bool work_ran = false;
};

static int
owner_dispatch(struct ged *UNUSED(gedp), ged_event_work_func_t work,
	       void *work_data, void *client_data)
{
    dispatch_state *state = static_cast<dispatch_state *>(client_data);
    state->calls++;
    work(work_data);
    state->work_ran = true;
    return 0;
}

static int
incomplete_dispatch(struct ged *UNUSED(gedp),
		    ged_event_work_func_t UNUSED(work),
		    void *UNUSED(work_data), void *UNUSED(client_data))
{
    return 0;
}

static void
test_batching_and_payloads(struct ged *gedp)
{
    observation state;
    ged_event_observer_token observer = ged_event_observer_add(gedp,
	GED_EVENT_OBSERVER_POST_RECONCILE, record_events, &state);
    CHECK(observer != 0, "post-reconcile observer is registered");

    struct ged_event_result *result = ged_event_result_create();
    char copied_name[] = "alpha";
    struct ged_event events[3];
    for (size_t i = 0; i < 3; i++)
	ged_event_init(&events[i]);
    events[0].kind = GED_EVENT_OBJECT_MODIFIED;
    events[0].name = copied_name;
    events[1] = events[0];
    events[2].kind = GED_EVENT_COMB_INSTANCE_REMOVED;
    events[2].parent_name = "comb";
    events[2].child_name = "alpha";
    events[2].path = "comb/alpha";

    CHECK(ged_event_batch_begin(gedp) == GED_EVENT_OK,
	"outer batch begins");
    CHECK(ged_event_batch_begin(gedp) == GED_EVENT_OK,
	"nested batch begins");
    CHECK(ged_event_publish(gedp, events, 3, result) == GED_EVENT_OK,
	"batched events are accepted");
    bu_strlcpy(copied_name, "other", sizeof(copied_name));
    CHECK(state.calls.empty(), "nested batch does not dispatch early");
    CHECK(ged_event_batch_end(gedp, result) == GED_EVENT_OK,
	"nested batch ends");
    CHECK(state.calls.empty(), "inner batch end does not dispatch");
    CHECK(ged_event_batch_end(gedp, result) == GED_EVENT_OK,
	"outer batch ends");

    CHECK(state.calls.size() == 1, "batch dispatches once");
    CHECK(state.names.size() == 2 && state.names[0] == "alpha",
	"publisher payloads are copied before dispatch");
    CHECK(ged_event_result_event_count(result) == 3,
	"result records valid input count");
    CHECK(ged_event_result_coalesced_count(result) == 2,
	"exact duplicate events are coalesced");
    CHECK(ged_event_result_name_count(result) == 2,
	"result collects unique affected names");
    CHECK(ged_event_result_path_count(result) == 1 &&
	BU_STR_EQUAL(ged_event_result_path_at(result, 0), "comb/alpha"),
	"result reports affected paths");

    CHECK(ged_event_observer_remove(gedp, observer) == 1,
	"observer is removed");
    ged_event_result_destroy(result);
}

static void
test_phase_order_and_removal(struct ged *gedp)
{
    observation phases;
    ged_event_observer_token internal = ged_event_observer_add(gedp,
	GED_EVENT_OBSERVER_INTERNAL, record_internal, &phases);
    ged_event_observer_token post = ged_event_observer_add(gedp,
	GED_EVENT_OBSERVER_POST_RECONCILE, record_post, &phases);

    struct ged_event event;
    ged_event_init(&event);
    event.kind = GED_EVENT_OBJECT_ADDED;
    event.name = "ordered";
    CHECK(ged_event_publish(gedp, &event, 1, nullptr) == GED_EVENT_OK,
	"ordered event publishes");
    CHECK(phases.calls.size() == 2 && phases.calls[0] == "internal" &&
	phases.calls[1] == "post", "internal observers precede post observers");
    ged_event_observer_remove(gedp, internal);
    ged_event_observer_remove(gedp, post);

    observation removal;
    removal.self = ged_event_observer_add(gedp,
	GED_EVENT_OBSERVER_INTERNAL, remove_observers, &removal);
    removal.other = ged_event_observer_add(gedp,
	GED_EVENT_OBSERVER_INTERNAL, record_internal, &removal);
    CHECK(ged_event_publish(gedp, &event, 1, nullptr) == GED_EVENT_OK,
	"removal event publishes");
    CHECK(removal.calls.size() == 1 && removal.calls[0] == "remove",
	"removed later observer is skipped in current dispatch");
    CHECK(ged_event_publish(gedp, &event, 1, nullptr) == GED_EVENT_OK,
	"event publishes after observer removal");
    CHECK(removal.calls.size() == 1,
	"self-removed observer does not run again");
}

static void
test_reentrancy_and_owner_dispatch(struct ged *gedp)
{
    observation reentrant;
    reentrant.publish_followup = true;
    ged_event_observer_token observer = ged_event_observer_add(gedp,
	GED_EVENT_OBSERVER_POST_RECONCILE, publish_followup, &reentrant);
    struct ged_event event;
    ged_event_init(&event);
    event.kind = GED_EVENT_OBJECT_MODIFIED;
    event.name = "first";
    CHECK(ged_event_publish(gedp, &event, 1, nullptr) == GED_EVENT_OK,
	"initial reentrant event publishes");
    CHECK(reentrant.calls.size() == 2 && reentrant.calls[0] == "first" &&
	reentrant.calls[1] == "second",
	"reentrant event is deferred until current dispatch completes");
    ged_event_observer_remove(gedp, observer);

    dispatch_state dispatch;
    CHECK(ged_event_dispatch_set(gedp, owner_dispatch, &dispatch) ==
	GED_EVENT_OK, "owner dispatcher is installed");
    CHECK(ged_event_publish(gedp, &event, 1, nullptr) == GED_EVENT_OK,
	"owner-dispatched event publishes");
    CHECK(dispatch.calls == 1 && dispatch.work_ran,
	"event work uses the synchronous owner dispatcher");
    CHECK(ged_event_dispatch_set(gedp, nullptr, nullptr) == GED_EVENT_OK,
	"owner dispatcher is cleared");

    struct ged_event_result *result = ged_event_result_create();
    CHECK(ged_event_dispatch_set(gedp, incomplete_dispatch, nullptr) ==
	GED_EVENT_OK, "incomplete dispatcher is installed");
    CHECK(ged_event_publish(gedp, &event, 1, result) == GED_EVENT_ERROR,
	"dispatcher must complete work before returning");
    CHECK(ged_event_result_status(result) == GED_EVENT_ERROR &&
	std::strlen(ged_event_result_diagnostic(result)) > 0,
	"dispatcher failure records a diagnostic");
    CHECK(ged_event_dispatch_set(gedp, nullptr, nullptr) == GED_EVENT_OK,
	"incomplete dispatcher is cleared");
    ged_event_result_destroy(result);
}

static void
check_directory_removal(struct ged *gedp,
    const struct ged_event *events, size_t event_count,
    const struct ged_event_result *result, void *client_data)
{
    record_events(gedp, events, event_count, result, client_data);
    for (size_t i = 0; i < event_count; i++) {
	if (events[i].kind == GED_EVENT_OBJECT_REMOVED)
	    CHECK(db_lookup(gedp->dbip, events[i].name, LOOKUP_QUIET) == RT_DIR_NULL,
		"removal observers see the unlinked directory entry");
    }
}


static void
test_librt_bridge(void)
{
    struct db_i *dbip = db_open_inmem();
    CHECK(dbip != DBI_NULL, "in-memory database opens");
    if (dbip == DBI_NULL)
	return;

    struct bu_vls address = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&address, "%p", (void *)dbip);
    struct ged *gedp = ged_open("inmem", bu_vls_cstr(&address), 0);
    bu_vls_free(&address);
    CHECK(gedp != GED_NULL, "GED context adopts in-memory database");
    if (gedp == GED_NULL) {
	db_close(dbip);
	return;
    }

    DbiState *dbis = new DbiState(gedp);
    gedp->dbi_state = dbis;

    observation state;
    ged_event_observer_token observer = ged_event_observer_add(gedp,
	GED_EVENT_OBSERVER_POST_RECONCILE, check_directory_removal, &state);
    int type = ID_SPH;
    struct directory *dp = db_diradd(gedp->dbip, "direct.s",
	RT_DIR_PHONY_ADDR, 0, RT_DIR_SOLID, &type);
    CHECK(dp != RT_DIR_NULL, "direct librt directory addition succeeds");
    CHECK(state.kinds.size() == 1 &&
	state.kinds[0] == GED_EVENT_OBJECT_ADDED &&
	state.names[0] == "direct.s",
	"direct librt addition publishes a GED event");

    if (dp != RT_DIR_NULL)
	CHECK(db_dirdelete(gedp->dbip, dp) == 0,
	    "direct librt directory removal succeeds");
    CHECK(state.kinds.size() == 2 &&
	state.kinds[1] == GED_EVENT_OBJECT_REMOVED &&
	state.names[1] == "direct.s",
	"direct librt removal publishes a GED event");

    std::vector<DbiStateChange> changes = dbis->change_snapshots();
    CHECK(changes.size() == 2 &&
	changes[0].added.count("direct.s") == 1 &&
	changes[1].removed.count("direct.s") == 1,
	"DbiState retains value-based add and removal snapshots");
    CHECK(dbis->reconcile_changes() == GED_DBISTATE_DB_CHANGE,
	"pending DbiState snapshots reconcile explicitly");
    CHECK(dbis->reconcile_changes() == 0,
	"each DbiState revision reconciles only once");

    dbis->acknowledge_changes(changes[0].revision);
    changes = dbis->change_snapshots();
    CHECK(changes.size() == 1 &&
	changes[0].removed.count("direct.s") == 1,
	"acknowledging one revision preserves later snapshots");
    dbis->acknowledge_changes(changes[0].revision);
    CHECK(dbis->change_snapshots().empty(),
	"acknowledging the latest reconciled revision clears history");

    struct ged_event rebuild = {};
    rebuild.kind = GED_EVENT_BATCH_REBUILD;
    CHECK(ged_event_publish(gedp, &rebuild, 1, nullptr) == GED_EVENT_OK &&
	dbis->reconcile_changes() == GED_DBISTATE_DB_CHANGE,
	"an inventory rebuild reconciles without individual object events");

    ged_event_observer_remove(gedp, observer);
    gedp->dbi_state = nullptr;
    delete dbis;
    ged_close(gedp);
}

static void
test_dbi_change_inventory(void)
{
    struct db_i *dbip = db_open_inmem();
    CHECK(dbip != DBI_NULL, "inventory database opens");
    if (dbip == DBI_NULL)
	return;

    struct bu_vls address = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&address, "%p", (void *)dbip);
    struct ged *gedp = ged_open("inmem", bu_vls_cstr(&address), 0);
    bu_vls_free(&address);
    CHECK(gedp != GED_NULL, "inventory GED context opens");
    if (gedp == GED_NULL) {
	db_close(dbip);
	return;
    }

    DbiState *dbis = new DbiState(gedp);
    gedp->dbi_state = dbis;
    gedp->ged_lod = bv_mesh_lod_context_create("event_inventory.g");
    CHECK(gedp->ged_lod != nullptr, "inventory LoD context opens");
    constexpr unsigned long long added_lod_key = 1001;
    constexpr unsigned long long changed_lod_key = 1002;
    constexpr unsigned long long removed_lod_key = 1003;
    constexpr unsigned long long renamed_lod_key = 1004;
    if (gedp->ged_lod) {
	bv_mesh_lod_key_put(gedp->ged_lod, "added.s", added_lod_key);
	bv_mesh_lod_key_put(gedp->ged_lod, "changed.s", changed_lod_key);
	bv_mesh_lod_key_put(gedp->ged_lod, "removed.s", removed_lod_key);
	bv_mesh_lod_key_put(gedp->ged_lod, "old.s", renamed_lod_key);
    }

    struct ged_event events[5];
    for (size_t i = 0; i < 5; i++)
	ged_event_init(&events[i]);
    events[0].kind = GED_EVENT_OBJECT_ADDED;
    events[0].name = "added.s";
    events[1].kind = GED_EVENT_OBJECT_MODIFIED;
    events[1].name = "changed.s";
    events[2].kind = GED_EVENT_OBJECT_REMOVED;
    events[2].name = "removed.s";
    events[3].kind = GED_EVENT_COMB_INSTANCE_REMOVED;
    events[3].parent_name = "parent.c";
    events[3].child_name = "child.s";
    events[3].path = "parent.c/child.s";
    events[4].kind = GED_EVENT_OBJECT_RENAMED;
    events[4].name = "old.s";
    events[4].new_name = "new.s";

    CHECK(ged_event_publish(gedp, events, 5, nullptr) == GED_EVENT_OK,
	"mixed mutation batch publishes");
    std::vector<DbiStateChange> changes = dbis->change_snapshots();
    CHECK(changes.size() == 1, "one event batch creates one revision");
    if (changes.size() == 1) {
	const DbiStateChange &change = changes[0];
	CHECK(change.added.count("added.s") == 1 &&
	    change.modified.count("changed.s") == 1 &&
	    change.modified.count("parent.c") == 1 &&
	    change.removed.count("removed.s") == 1 &&
	    change.renamed.at("old.s") == "new.s",
	    "snapshot classifies object mutations");
	CHECK(change.paths.count("parent.c/child.s") == 1 &&
	    change.structural && change.nref_dirty,
	    "snapshot preserves paths and structural state");
	dbis->acknowledge_changes(change.revision);
	CHECK(dbis->change_snapshots().size() == 1,
	    "unreconciled revisions cannot be acknowledged");
    }

    if (gedp->ged_lod) {
	CHECK(bv_mesh_lod_key_get(gedp->ged_lod, "changed.s") ==
	    changed_lod_key,
	    "recording changes does not invalidate LoD data prematurely");
    }

    dbis->reconcile_changes();
    if (gedp->ged_lod) {
	CHECK(bv_mesh_lod_key_get(gedp->ged_lod, "added.s") == 0 &&
	    bv_mesh_lod_key_get(gedp->ged_lod, "changed.s") == 0 &&
	    bv_mesh_lod_key_get(gedp->ged_lod, "removed.s") == 0 &&
	    bv_mesh_lod_key_get(gedp->ged_lod, "old.s") == 0,
	    "reconciliation invalidates LoD names for every mutation class");
    }
    changes = dbis->change_snapshots();
    if (!changes.empty())
	dbis->acknowledge_changes(changes.back().revision);
    CHECK(dbis->change_snapshots().empty(),
	"reconciled inventory can be acknowledged");

    observation followup;
    followup.publish_followup = true;
    ged_event_observer_token observer = ged_event_observer_add(gedp,
	GED_EVENT_OBSERVER_INTERNAL, publish_followup, &followup);
    struct ged_event first;
    ged_event_init(&first);
    first.kind = GED_EVENT_OBJECT_MODIFIED;
    first.name = "first";
    CHECK(ged_event_publish(gedp, &first, 1, nullptr) == GED_EVENT_OK,
	"followup inventory events publish");
    changes = dbis->change_snapshots();
    CHECK(changes.size() == 2 &&
	changes[0].modified.count("first") == 1 &&
	changes[1].modified.count("second") == 1,
	"changes during dispatch become a later revision");
    dbis->update();
    CHECK(dbis->change_snapshots().empty(),
	"legacy update consumes reconciled snapshot history");
    ged_event_observer_remove(gedp, observer);

    gedp->dbi_state = nullptr;
    delete dbis;
    ged_close(gedp);
}

static void
test_dbi_duplicate_instances(void)
{
    struct db_i *dbip = db_open_inmem();
    CHECK(dbip != DBI_NULL, "duplicate instance database opens");
    if (dbip == DBI_NULL)
	return;

    struct bu_vls address = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&address, "%p", (void *)dbip);
    struct ged *gedp = ged_open("inmem", bu_vls_cstr(&address), 0);
    bu_vls_free(&address);
    CHECK(gedp != GED_NULL, "duplicate instance GED context opens");
    if (gedp == GED_NULL) {
	db_close(dbip);
	return;
    }

    DbiState *dbis = new DbiState(gedp);
    gedp->dbi_state = dbis;
    const char *make_args[] = {"make", "duplicate.s", "sph"};
    CHECK(ged_exec(gedp, 3, make_args) == BRLCAD_OK,
	"duplicate instance solid is created");
    const char *group_args[] = {
	"g", "duplicate.g", "duplicate.s", "duplicate.s"
    };
    CHECK(ged_exec(gedp, 4, group_args) == BRLCAD_OK,
	"combination with duplicate members is created");
    dbis->reconcile_changes();

    unsigned long long group_hash = bu_data_hash("duplicate.g",
	std::strlen("duplicate.g"));
    auto children_it = dbis->p_v.find(group_hash);
    CHECK(children_it != dbis->p_v.end() && children_it->second.size() == 2,
	"DbiState records both duplicate members");
    if (children_it != dbis->p_v.end()) {
	for (unsigned long long child_hash : children_it->second)
	    CHECK(dbis->valid_hash(child_hash),
		"duplicate member hash remains resolvable");
    }
    CHECK(dbis->i_map.size() == 1 && dbis->i_str.size() == 1,
	"used duplicate instance metadata survives garbage collection");

    gedp->dbi_state = nullptr;
    delete dbis;
    ged_close(gedp);
}

static void
test_move_all_events(void)
{
    struct db_i *dbip = db_open_inmem();
    CHECK(dbip != DBI_NULL, "move_all event database opens");
    if (dbip == DBI_NULL)
	return;

    struct bu_vls address = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&address, "%p", (void *)dbip);
    struct ged *gedp = ged_open("inmem", bu_vls_cstr(&address), 0);
    bu_vls_free(&address);
    CHECK(gedp != GED_NULL, "move_all event GED context opens");
    if (gedp == GED_NULL) {
	db_close(dbip);
	return;
    }

    const char *direct_old = "move_all_direct_old.s";
    const char *direct_new = "move_all_direct_new.s";
    const char *make_direct[] = {"make", direct_old, "sph"};
    CHECK(ged_exec(gedp, 3, make_direct) == BRLCAD_OK,
	"move_all direct source is created");

    observation direct;
    ged_event_observer_token direct_observer = ged_event_observer_add(gedp,
	GED_EVENT_OBSERVER_POST_RECONCILE, record_events, &direct);
    CHECK(direct_observer != 0, "move_all direct observer registers");
    const char *move_direct[] = {"move_all", direct_old, direct_new};
    CHECK(ged_exec(gedp, 3, move_direct) == BRLCAD_OK,
	"move_all direct rename succeeds");
    CHECK(direct.calls.size() == 1,
	"move_all direct rename dispatches one event batch");
    CHECK(observed_rename(direct, direct_old, direct_new),
	"move_all direct rename publishes old and new identities");
    CHECK(db_lookup(gedp->dbip, direct_new, LOOKUP_QUIET) != RT_DIR_NULL,
	"move_all direct rename updates the database directory");
    CHECK(ged_event_observer_remove(gedp, direct_observer) == 1,
	"move_all direct observer is removed");

    const char *file_old_a = "move_all_file_old_a.s";
    const char *file_old_b = "move_all_file_old_b.s";
    const char *file_new_a = "move_all_file_new_a.s";
    const char *file_new_b = "move_all_file_new_b.s";
    const char *make_file_a[] = {"make", file_old_a, "sph"};
    const char *make_file_b[] = {"make", file_old_b, "sph"};
    CHECK(ged_exec(gedp, 3, make_file_a) == BRLCAD_OK &&
	ged_exec(gedp, 3, make_file_b) == BRLCAD_OK,
	"move_all file sources are created");

    char mapping_path[MAXPATHLEN] = {0};
    FILE *mapping = bu_temp_file(mapping_path, MAXPATHLEN);
    CHECK(mapping != NULL, "move_all mapping file is created");
    if (mapping) {
	std::fprintf(mapping, "%s %s\n", file_old_a, file_new_a);
	std::fprintf(mapping, "%s %s\n", file_old_b, file_new_b);
	std::fclose(mapping);

	observation grouped;
	ged_event_observer_token grouped_observer = ged_event_observer_add(gedp,
	    GED_EVENT_OBSERVER_POST_RECONCILE, record_events, &grouped);
	CHECK(grouped_observer != 0, "move_all file observer registers");
	const char *move_file[] = {"move_all", "-f", mapping_path};
	CHECK(ged_exec(gedp, 3, move_file) == BRLCAD_OK,
	    "move_all file renames succeed");
	CHECK(grouped.calls.size() == 1,
	    "move_all file renames dispatch one event batch");
	CHECK(observed_rename(grouped, file_old_a, file_new_a) &&
	    observed_rename(grouped, file_old_b, file_new_b),
	    "move_all file batch publishes both rename identities");
	CHECK(ged_event_observer_remove(gedp, grouped_observer) == 1,
	    "move_all file observer is removed");
	(void)bu_file_delete(mapping_path);
    }

    ged_close(gedp);
}

static void
test_concurrent_publication(struct ged *gedp)
{
    constexpr size_t thread_count = 4;
    static constexpr size_t events_per_thread = 25;
    std::atomic<size_t> publish_failures(0);
    observation state;
    ged_event_observer_token observer = ged_event_observer_add(gedp,
	GED_EVENT_OBSERVER_POST_RECONCILE, record_events, &state);

    std::vector<std::thread> publishers;
    for (size_t thread_index = 0; thread_index < thread_count;
	 thread_index++) {
	publishers.emplace_back([gedp, thread_index, &publish_failures]() {
	    for (size_t event_index = 0; event_index < events_per_thread;
		 event_index++) {
		std::string name = "parallel_" +
		    std::to_string(thread_index) + "_" +
		    std::to_string(event_index);
		struct ged_event event;
		ged_event_init(&event);
		event.kind = GED_EVENT_OBJECT_MODIFIED;
		event.name = name.c_str();
		if (ged_event_publish(gedp, &event, 1, nullptr) != GED_EVENT_OK)
		    publish_failures++;
	    }
	});
    }
    for (std::thread &publisher : publishers)
	publisher.join();

    CHECK(publish_failures == 0, "concurrent events publish successfully");
    CHECK(state.kinds.size() == thread_count * events_per_thread,
	"concurrent publications are serialized without loss");
    ged_event_observer_remove(gedp, observer);
}

int
main(int argc, char **argv)
{
    bu_setprogname(argv[0]);
    CHECK(argc == 1, "test takes no arguments");

    struct ged context;
    ged_init(&context);
    test_batching_and_payloads(&context);
    test_phase_order_and_removal(&context);
    test_reentrancy_and_owner_dispatch(&context);
    test_concurrent_publication(&context);
    test_reentrant_batch(&context);
    test_librt_bridge();
    test_dbi_change_inventory();
    test_dbi_duplicate_instances();
    test_move_all_events();

    CHECK(ged_event_batch_end(&context, nullptr) == GED_EVENT_INVALID,
	"ending a nonexistent batch is rejected");
    CHECK(ged_event_publish(&context, nullptr, 0, nullptr) ==
	GED_EVENT_INVALID, "empty publication is rejected");
    ged_free(&context);

    if (failures)
	bu_log("%d event test failure(s)\n", failures);
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
