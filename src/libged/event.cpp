/*                        E V E N T . C P P
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

#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

#include "ged/event.h"
#include "ged_private.h"

struct ged_event_owned {
    enum ged_event_kind kind = GED_EVENT_NONE;
    std::string name;
    std::string new_name;
    std::string parent_name;
    std::string child_name;
    std::string path;
    int redraw = 0;
};

struct ged_event_observer_entry {
    ged_event_observer_token token = 0;
    enum ged_event_observer_phase phase = GED_EVENT_OBSERVER_INTERNAL;
    ged_event_observer_func_t func = nullptr;
    void *client_data = nullptr;
    bool removed = false;
};

struct ged_event_result {
    enum ged_event_status status = GED_EVENT_OK;
    size_t event_count = 0;
    size_t coalesced_count = 0;
    std::vector<std::string> names;
    std::vector<std::string> paths;
    std::string diagnostic;
};

struct ged_event_service {
    struct ged *gedp = nullptr;
    struct db_i *callback_dbip = nullptr;
    ged_event_observer_token next_token = 1;
    int batch_depth = 0;
    int dispatch_depth = 0;
    size_t queued_count = 0;
    std::vector<ged_event_owned> queued_events;
    std::vector<ged_event_owned> followup_events;
    std::vector<ged_event_observer_entry> observers;
    ged_event_dispatch_func_t dispatch = nullptr;
    void *dispatch_data = nullptr;
};

static struct ged_event_service *
ged_event_service_get(struct ged *gedp)
{
    return (gedp && gedp->i) ? gedp->i->ged_event_servicep : nullptr;
}

static const char *
ged_event_string(const std::string &value)
{
    return value.empty() ? nullptr : value.c_str();
}

static struct ged_event
ged_event_view(const ged_event_owned &owned)
{
    struct ged_event event;
    event.kind = owned.kind;
    event.name = ged_event_string(owned.name);
    event.new_name = ged_event_string(owned.new_name);
    event.parent_name = ged_event_string(owned.parent_name);
    event.child_name = ged_event_string(owned.child_name);
    event.path = ged_event_string(owned.path);
    event.redraw = owned.redraw;
    return event;
}

static ged_event_owned
ged_event_own(const struct ged_event &event)
{
    ged_event_owned owned;
    owned.kind = event.kind;
    if (event.name)
	owned.name = event.name;
    if (event.new_name)
	owned.new_name = event.new_name;
    if (event.parent_name)
	owned.parent_name = event.parent_name;
    if (event.child_name)
	owned.child_name = event.child_name;
    if (event.path)
	owned.path = event.path;
    owned.redraw = event.redraw ? 1 : 0;
    return owned;
}

static size_t
ged_event_queue(std::vector<ged_event_owned> &queue,
		const struct ged_event *events, size_t event_count)
{
    size_t added = 0;
    queue.reserve(queue.size() + event_count);
    for (size_t i = 0; i < event_count; i++) {
	if (events[i].kind == GED_EVENT_NONE)
	    continue;
	queue.push_back(ged_event_own(events[i]));
	added++;
    }
    return added;
}

static bool
ged_event_same(const ged_event_owned &left, const ged_event_owned &right)
{
    return left.kind == right.kind &&
	left.name == right.name &&
	left.new_name == right.new_name &&
	left.parent_name == right.parent_name &&
	left.child_name == right.child_name &&
	left.path == right.path &&
	left.redraw == right.redraw;
}

static std::vector<ged_event_owned>
ged_event_coalesce(const std::vector<ged_event_owned> &events)
{
    std::vector<ged_event_owned> coalesced;
    coalesced.reserve(events.size());
    for (const ged_event_owned &event : events) {
	if (std::find_if(coalesced.begin(), coalesced.end(),
		[&event](const ged_event_owned &candidate) {
		    return ged_event_same(event, candidate);
		}) == coalesced.end())
	    coalesced.push_back(event);
    }
    return coalesced;
}

static void
ged_event_add_unique(std::vector<std::string> &values,
		     const std::string &value)
{
    if (!value.empty() &&
	std::find(values.begin(), values.end(), value) == values.end())
	values.push_back(value);
}

static void
ged_event_result_set(struct ged_event_result *result,
		     const std::vector<ged_event_owned> &events,
		     size_t input_count)
{
    if (!result)
	return;
    ged_event_result_clear(result);
    result->event_count = input_count;
    result->coalesced_count = events.size();
    for (const ged_event_owned &event : events) {
	ged_event_add_unique(result->names, event.name);
	ged_event_add_unique(result->names, event.new_name);
	ged_event_add_unique(result->names, event.parent_name);
	ged_event_add_unique(result->names, event.child_name);
	ged_event_add_unique(result->paths, event.path);
    }
}

static void
ged_event_prune_observers(struct ged_event_service *service)
{
    service->observers.erase(
	std::remove_if(service->observers.begin(), service->observers.end(),
	    [](const ged_event_observer_entry &entry) {
		return entry.removed;
	    }),
	service->observers.end());
}

static ged_event_observer_entry *
ged_event_observer_find(struct ged_event_service *service,
			ged_event_observer_token token)
{
    for (ged_event_observer_entry &entry : service->observers) {
	if (entry.token == token)
	    return &entry;
    }
    return nullptr;
}

static void
ged_event_dispatch_phase(struct ged_event_service *service,
			 enum ged_event_observer_phase phase,
			 const std::vector<struct ged_event> &events,
			 const struct ged_event_result *result)
{
    std::vector<ged_event_observer_token> tokens;
    for (const ged_event_observer_entry &entry : service->observers) {
	if (!entry.removed && entry.phase == phase)
	    tokens.push_back(entry.token);
    }

    for (ged_event_observer_token token : tokens) {
	ged_event_observer_entry *entry = ged_event_observer_find(service, token);
	if (!entry || entry->removed)
	    continue;
	ged_event_observer_func_t func = entry->func;
	void *client_data = entry->client_data;
	func(service->gedp, events.data(), events.size(), result, client_data);
    }
}

struct ged_event_process_context {
    struct ged_event_service *service;
    const std::vector<ged_event_owned> *events;
    size_t input_count;
    struct ged_event_result *result;
    bool completed;
};

static void
ged_event_process_work(void *work_data)
{
    ged_event_process_context *context =
	static_cast<ged_event_process_context *>(work_data);
    struct ged_event_service *service = context->service;
    std::vector<ged_event_owned> coalesced =
	ged_event_coalesce(*context->events);
    ged_event_result_set(context->result, coalesced, context->input_count);

    std::vector<struct ged_event> views;
    views.reserve(coalesced.size());
    for (const ged_event_owned &event : coalesced)
	views.push_back(ged_event_view(event));

    service->dispatch_depth++;
    ged_event_dispatch_phase(service, GED_EVENT_OBSERVER_INTERNAL, views,
	context->result);
    ged_event_dispatch_phase(service, GED_EVENT_OBSERVER_POST_RECONCILE, views,
	context->result);
    service->dispatch_depth--;
    if (!service->dispatch_depth)
	ged_event_prune_observers(service);
    context->completed = true;
}

static enum ged_event_status
ged_event_process(struct ged_event_service *service,
		  const std::vector<ged_event_owned> &events,
		  size_t input_count, struct ged_event_result *result)
{
    struct ged_event_result local_result;
    struct ged_event_result *dispatch_result = result ? result : &local_result;
    ged_event_process_context context = {
	service, &events, input_count, dispatch_result, false
    };
    int dispatch_status = 0;
    if (service->dispatch)
	dispatch_status = service->dispatch(service->gedp,
	    ged_event_process_work, &context, service->dispatch_data);
    if (service->dispatch && (dispatch_status != 0 || !context.completed)) {
	ged_event_result_clear(dispatch_result);
	dispatch_result->status = GED_EVENT_ERROR;
	dispatch_result->diagnostic = "owner-context event dispatch failed";
	return GED_EVENT_ERROR;
    }
    if (!service->dispatch)
	ged_event_process_work(&context);
    return GED_EVENT_OK;
}

static enum ged_event_status
ged_event_drain(struct ged_event_service *service,
		std::vector<ged_event_owned> &events, size_t input_count,
		struct ged_event_result *result)
{
    enum ged_event_status status =
	ged_event_process(service, events, input_count, result);
    while (status == GED_EVENT_OK && !service->followup_events.empty()) {
	std::vector<ged_event_owned> followups;
	followups.swap(service->followup_events);
	status = ged_event_process(service, followups, followups.size(), nullptr);
    }
    return status;
}

static void
ged_event_librt_changed(struct db_i *UNUSED(dbip), struct directory *dp,
		       int mode, void *client_data)
{
    struct ged *gedp = static_cast<struct ged *>(client_data);
    if (!gedp || !dp || !dp->d_namep)
	return;

    struct ged_event event;
    ged_event_init(&event);
    event.name = dp->d_namep;
    if (BU_STR_EQUAL(event.name, DB5_GLOBAL_OBJECT_NAME)) {
	event.kind = GED_EVENT_DATABASE_METADATA_CHANGED;
    } else if (mode == 0) {
	event.kind = (dp->d_flags & RT_DIR_COMB) ?
	    GED_EVENT_COMB_TREE_CHANGED : GED_EVENT_OBJECT_MODIFIED;
    } else if (mode == 1) {
	event.kind = GED_EVENT_OBJECT_ADDED;
    } else if (mode == 2) {
	event.kind = GED_EVENT_OBJECT_REMOVED;
    } else {
	return;
    }
    (void)ged_event_publish(gedp, &event, 1, nullptr);
}

struct ged_event_service *
ged_event_service_create(struct ged *gedp)
{
    struct ged_event_service *service = new ged_event_service;
    service->gedp = gedp;
    return service;
}

void
ged_event_service_destroy(struct ged_event_service *service)
{
    if (service && service->callback_dbip)
	db_rm_changed_clbk(service->callback_dbip, ged_event_librt_changed,
	    service->gedp);
    delete service;
}

void
ged_event_database_set_private(struct ged *gedp, struct db_i *dbip)
{
    struct ged_event_service *service = ged_event_service_get(gedp);
    if (!service || service->callback_dbip == dbip)
	return;
    if (service->callback_dbip)
	db_rm_changed_clbk(service->callback_dbip, ged_event_librt_changed, gedp);
    service->callback_dbip = nullptr;
    if (dbip && db_add_changed_clbk(dbip, ged_event_librt_changed, gedp) == 0)
	service->callback_dbip = dbip;
}

void
ged_event_init(struct ged_event *event)
{
    if (event)
	std::memset(event, 0, sizeof(*event));
}

struct ged_event_result *
ged_event_result_create(void)
{
    return new ged_event_result;
}

void
ged_event_result_clear(struct ged_event_result *result)
{
    if (!result)
	return;
    result->status = GED_EVENT_OK;
    result->event_count = 0;
    result->coalesced_count = 0;
    result->names.clear();
    result->paths.clear();
    result->diagnostic.clear();
}

void
ged_event_result_destroy(struct ged_event_result *result)
{
    delete result;
}

enum ged_event_status
ged_event_result_status(const struct ged_event_result *result)
{
    return result ? result->status : GED_EVENT_INVALID;
}

size_t
ged_event_result_event_count(const struct ged_event_result *result)
{
    return result ? result->event_count : 0;
}

size_t
ged_event_result_coalesced_count(const struct ged_event_result *result)
{
    return result ? result->coalesced_count : 0;
}

size_t
ged_event_result_name_count(const struct ged_event_result *result)
{
    return result ? result->names.size() : 0;
}

const char *
ged_event_result_name_at(const struct ged_event_result *result, size_t index)
{
    return (result && index < result->names.size()) ?
	result->names[index].c_str() : nullptr;
}

size_t
ged_event_result_path_count(const struct ged_event_result *result)
{
    return result ? result->paths.size() : 0;
}

const char *
ged_event_result_path_at(const struct ged_event_result *result, size_t index)
{
    return (result && index < result->paths.size()) ?
	result->paths[index].c_str() : nullptr;
}

const char *
ged_event_result_diagnostic(const struct ged_event_result *result)
{
    return result ? result->diagnostic.c_str() : "";
}

enum ged_event_status
ged_event_dispatch_set(struct ged *gedp, ged_event_dispatch_func_t dispatch,
		       void *client_data)
{
    struct ged_event_service *service = ged_event_service_get(gedp);
    if (!service)
	return GED_EVENT_UNAVAILABLE;
    if (service->dispatch_depth || service->batch_depth)
	return GED_EVENT_INVALID;
    service->dispatch = dispatch;
    service->dispatch_data = client_data;
    return GED_EVENT_OK;
}

enum ged_event_status
ged_event_batch_begin(struct ged *gedp)
{
    struct ged_event_service *service = ged_event_service_get(gedp);
    if (!service)
	return GED_EVENT_UNAVAILABLE;
    service->batch_depth++;
    return GED_EVENT_OK;
}

enum ged_event_status
ged_event_batch_end(struct ged *gedp, struct ged_event_result *result)
{
    struct ged_event_service *service = ged_event_service_get(gedp);
    if (!service)
	return GED_EVENT_UNAVAILABLE;
    if (service->batch_depth <= 0)
	return GED_EVENT_INVALID;
    service->batch_depth--;
    if (service->batch_depth)
	return GED_EVENT_OK;

    std::vector<ged_event_owned> events;
    events.swap(service->queued_events);
    size_t input_count = service->queued_count;
    service->queued_count = 0;
    if (events.empty()) {
	ged_event_result_clear(result);
	return GED_EVENT_OK;
    }
    return ged_event_drain(service, events, input_count, result);
}

enum ged_event_status
ged_event_publish(struct ged *gedp, const struct ged_event *events,
		  size_t event_count, struct ged_event_result *result)
{
    struct ged_event_service *service = ged_event_service_get(gedp);
    if (!service)
	return GED_EVENT_UNAVAILABLE;
    if (!events || !event_count)
	return GED_EVENT_INVALID;

    if (service->dispatch_depth) {
	ged_event_queue(service->followup_events, events, event_count);
	return GED_EVENT_OK;
    }
    if (service->batch_depth) {
	service->queued_count +=
	    ged_event_queue(service->queued_events, events, event_count);
	return GED_EVENT_OK;
    }

    std::vector<ged_event_owned> owned;
    size_t input_count = ged_event_queue(owned, events, event_count);
    if (owned.empty()) {
	ged_event_result_clear(result);
	return GED_EVENT_OK;
    }
    return ged_event_drain(service, owned, input_count, result);
}

ged_event_observer_token
ged_event_observer_add(struct ged *gedp,
		       enum ged_event_observer_phase phase,
		       ged_event_observer_func_t func,
		       void *client_data)
{
    struct ged_event_service *service = ged_event_service_get(gedp);
    if (!service || !func ||
	(phase != GED_EVENT_OBSERVER_INTERNAL &&
	 phase != GED_EVENT_OBSERVER_POST_RECONCILE))
	return 0;

    ged_event_observer_entry entry;
    entry.token = service->next_token++;
    if (!entry.token)
	entry.token = service->next_token++;
    entry.phase = phase;
    entry.func = func;
    entry.client_data = client_data;
    service->observers.push_back(entry);
    return entry.token;
}

int
ged_event_observer_remove(struct ged *gedp, ged_event_observer_token token)
{
    struct ged_event_service *service = ged_event_service_get(gedp);
    if (!service || !token)
	return 0;
    ged_event_observer_entry *entry = ged_event_observer_find(service, token);
    if (!entry || entry->removed)
	return 0;
    entry->removed = true;
    if (!service->dispatch_depth)
	ged_event_prune_observers(service);
    return 1;
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
