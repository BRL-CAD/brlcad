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

#include <cstring>
#include <string>
#include <vector>

#include "bu/app.h"
#include "ged.h"

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
    for (size_t i = 0; i < event_count; i++)
	state->names.push_back(events[i].name ? events[i].name : "");
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
