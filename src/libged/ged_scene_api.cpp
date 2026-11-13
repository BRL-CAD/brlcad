/*                  G E D _ S C E N E _ A P I . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License version 2.1.
 */
/** @file ged_scene_api.cpp */

#include "common.h"

#include <climits>
#include <new>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "ged.h"
#include "ged/scene.h"

#include "./dbi.h"
#include "./ged_private.h"

struct ged_scene_result {
    enum ged_scene_status status = GED_SCENE_OK;
    int changed = 0;
    size_t group_count = 0;
    size_t shape_count = 0;
    size_t conflict_count = 0;
    uint64_t revision_before = 0;
    uint64_t revision_after = 0;
    std::vector<std::string> paths;
    std::string diagnostic;
};

struct ged_scene_delta {
    enum ged_scene_delta_kind kind = GED_SCENE_DELTA_UNKNOWN;
    ged_view_context *view = NULL;
    std::vector<std::string> paths;
    int affects_all = 0;
    size_t group_count = 0;
    size_t shape_count = 0;
    int presentation_only = 0;
    uint64_t revision_before = 0;
    uint64_t revision_after = 0;
    std::string diagnostic;
};

static BViewState *
ged_scene_view_state(struct ged *gedp, ged_view_context *view)
{
    if (!gedp || !gedp->dbi_state || !view)
	return NULL;
    DbiState *dbis = static_cast<DbiState *>(gedp->dbi_state);
    return dbis->get_view_state(view);
}

static void
ged_scene_result_reset(struct ged_scene_result *result)
{
    if (!result)
	return;
    *result = ged_scene_result();
}

static void
ged_scene_result_fail(struct ged_scene_result *result,
	enum ged_scene_status status, const char *diagnostic)
{
    if (!result)
	return;
    result->status = status;
    result->diagnostic = diagnostic ? diagnostic : "";
}

static int
ged_scene_request_supported(const struct ged_scene_draw_request *request)
{
    if (!request)
	return 0;
    const struct ged_scene_style *style = &request->style;
    const struct ged_scene_realization_policy *realization =
	&request->realization;
    return style->draw_mode == GED_SCENE_DRAW_DEFAULT &&
	EQUAL(style->opacity, 1.0) && !style->color_override &&
	style->line_width == 1 && !style->mixed_modes && !style->flags &&
	realization->mode == GED_SCENE_REALIZE_AUTO && !realization->strict;
}

static void
ged_scene_publish(struct ged *gedp, enum ged_scene_delta_kind kind,
	ged_view_context *view, const std::vector<std::string> &paths,
	uint64_t revision_before, uint64_t revision_after)
{
    if (!gedp || !gedp->i || !gedp->i->i ||
	(paths.empty() && kind != GED_SCENE_DELTA_REDRAW))
	return;

    struct ged_scene_delta delta;
    delta.kind = kind;
    delta.view = view;
    delta.paths = paths;
    delta.affects_all = paths.empty() ? 1 : 0;
    delta.revision_before = revision_before;
    delta.revision_after = revision_after;

    Ged_Internal *internal = gedp->i->i;
    std::vector<ged_scene_observer_token> observers;
    observers.reserve(internal->scene_observers.size());
    for (const auto &observer : internal->scene_observers)
	observers.push_back(observer.first);

    for (ged_scene_observer_token token : observers) {
	auto observer = internal->scene_observers.find(token);
	if (observer != internal->scene_observers.end() &&
	    observer->second.first)
	    observer->second.first(gedp, &delta, observer->second.second);
    }
}

void
ged_scene_style_init(struct ged_scene_style *style)
{
    if (!style)
	return;
    style->draw_mode = GED_SCENE_DRAW_DEFAULT;
    style->opacity = 1.0;
    style->color[0] = 255;
    style->color[1] = 0;
    style->color[2] = 0;
    style->color_override = 0;
    style->line_width = 1;
    style->mixed_modes = 0;
    style->flags = 0;
}

void
ged_scene_realization_policy_init(struct ged_scene_realization_policy *policy)
{
    if (!policy)
	return;
    policy->mode = GED_SCENE_REALIZE_AUTO;
    policy->strict = 0;
}

void
ged_scene_draw_request_init(struct ged_scene_draw_request *request)
{
    if (!request)
	return;
    request->view = NULL;
    request->paths = NULL;
    request->path_count = 0;
    ged_scene_style_init(&request->style);
    ged_scene_realization_policy_init(&request->realization);
    request->autoview = 0;
}

void
ged_scene_erase_request_init(struct ged_scene_erase_request *request)
{
    if (!request)
	return;
    request->view = NULL;
    request->path = NULL;
    request->match = GED_SCENE_ERASE_EXACT;
    request->draw_mode = GED_SCENE_DRAW_DEFAULT;
}

void
ged_scene_redraw_request_init(struct ged_scene_redraw_request *request)
{
    if (!request)
	return;
    request->view = NULL;
    request->paths = NULL;
    request->path_count = 0;
}

struct ged_scene_result *
ged_scene_result_create(void)
{
    return new (std::nothrow) struct ged_scene_result;
}

void
ged_scene_result_clear(struct ged_scene_result *result)
{
    ged_scene_result_reset(result);
}

void
ged_scene_result_destroy(struct ged_scene_result *result)
{
    delete result;
}

enum ged_scene_status
ged_scene_result_status(const struct ged_scene_result *result)
{
    return result ? result->status : GED_SCENE_INVALID;
}

int
ged_scene_result_changed(const struct ged_scene_result *result)
{
    return result ? result->changed : 0;
}

size_t
ged_scene_result_path_count(const struct ged_scene_result *result)
{
    return result ? result->paths.size() : 0;
}

const char *
ged_scene_result_path_at(const struct ged_scene_result *result, size_t index)
{
    return result && index < result->paths.size() ?
	result->paths[index].c_str() : NULL;
}

size_t
ged_scene_result_group_count(const struct ged_scene_result *result)
{
    return result ? result->group_count : 0;
}

size_t
ged_scene_result_shape_count(const struct ged_scene_result *result)
{
    return result ? result->shape_count : 0;
}

size_t
ged_scene_result_conflict_count(const struct ged_scene_result *result)
{
    return result ? result->conflict_count : 0;
}

uint64_t
ged_scene_result_revision_before(const struct ged_scene_result *result)
{
    return result ? result->revision_before : 0;
}

uint64_t
ged_scene_result_revision_after(const struct ged_scene_result *result)
{
    return result ? result->revision_after : 0;
}

const char *
ged_scene_result_diagnostic(const struct ged_scene_result *result)
{
    return result ? result->diagnostic.c_str() : "";
}

int
ged_scene_available(const struct ged *gedp)
{
    return gedp && gedp->dbi_state;
}

enum ged_scene_path_state
ged_scene_path_state_ids(struct ged *gedp, ged_view_context *view,
	const unsigned long long *ids, size_t count,
	enum ged_scene_draw_mode mode)
{
    BViewState *view_state = ged_scene_view_state(gedp, view);
    if (!view_state || !ids || !count || mode < GED_SCENE_DRAW_DEFAULT ||
	mode > GED_SCENE_DRAW_EVALUATED_POINTS)
	return GED_SCENE_PATH_NOT_DRAWN;

    DbiState *dbis = static_cast<DbiState *>(gedp->dbi_state);
    std::vector<unsigned long long> path_ids(ids, ids + count);
    const unsigned long long path_hash = dbis->path_hash(path_ids, 0);
    const int state = view_state->is_hdrawn(static_cast<int>(mode), path_hash);
    if (state == 1)
	return GED_SCENE_PATH_DRAWN;
    if (state == 2)
	return GED_SCENE_PATH_PARTIALLY_DRAWN;
    return GED_SCENE_PATH_NOT_DRAWN;
}

enum ged_scene_path_state
ged_scene_path_state_get(struct ged *gedp, ged_view_context *view,
	const char *path, enum ged_scene_draw_mode mode)
{
    if (!gedp || !gedp->dbi_state || !path || !path[0])
	return GED_SCENE_PATH_NOT_DRAWN;
    DbiState *dbis = static_cast<DbiState *>(gedp->dbi_state);
    std::vector<unsigned long long> ids = dbis->digest_path(path);
    return ids.empty() ? GED_SCENE_PATH_NOT_DRAWN :
	ged_scene_path_state_ids(gedp, view, ids.data(), ids.size(), mode);
}

enum ged_scene_status
ged_scene_draw(struct ged *gedp, const struct ged_scene_draw_request *request,
	struct ged_scene_result *result)
{
    ged_scene_result_reset(result);
    if (!ged_scene_available(gedp) || !request || !request->view ||
	!request->paths || !request->path_count ||
	request->path_count > static_cast<size_t>(INT_MAX) ||
	!ged_scene_request_supported(request)) {
	ged_scene_result_fail(result, GED_SCENE_INVALID,
	    "unsupported or invalid draw request");
	return GED_SCENE_INVALID;
    }

    const uint64_t revision_before = ged_scene_revision(gedp);
    if (result)
	result->revision_before = revision_before;
    std::vector<enum ged_scene_path_state> before(request->path_count);
    std::vector<const char *> argv;
    argv.reserve(request->path_count + 2);
    argv.push_back("draw");
    if (!request->autoview)
	argv.push_back("-R");
    for (size_t i = 0; i < request->path_count; i++) {
	if (!request->paths[i] || !request->paths[i][0]) {
	    ged_scene_result_fail(result, GED_SCENE_INVALID,
		"draw path is empty");
	    return GED_SCENE_INVALID;
	}
	DbiState *dbis = static_cast<DbiState *>(gedp->dbi_state);
	if (dbis->digest_path(request->paths[i]).empty()) {
	    ged_scene_result_fail(result, GED_SCENE_NOT_FOUND,
		"draw path was not found");
	    return GED_SCENE_NOT_FOUND;
	}
	before[i] = ged_scene_path_state_get(gedp, request->view,
	    request->paths[i], GED_SCENE_DRAW_DEFAULT);
	argv.push_back(request->paths[i]);
    }

    struct bview *active_view = gedp->ged_gvp;
    gedp->ged_gvp = request->view;
    const int ret = ged_exec_draw(gedp, static_cast<int>(argv.size()),
	argv.data());
    gedp->ged_gvp = active_view;
    if (ret != BRLCAD_OK) {
	ged_scene_result_fail(result, GED_SCENE_ERROR, "draw command failed");
	return GED_SCENE_ERROR;
    }

    std::vector<std::string> changed_paths;
    for (size_t i = 0; i < request->path_count; i++) {
	if (ged_scene_path_state_get(gedp, request->view, request->paths[i],
		GED_SCENE_DRAW_DEFAULT) != before[i])
	    changed_paths.emplace_back(request->paths[i]);
    }
    const uint64_t revision_after = changed_paths.empty() ? revision_before :
	++gedp->i->i->scene_revision;
    if (result) {
	result->changed = changed_paths.empty() ? 0 : 1;
	result->paths = changed_paths;
	result->revision_after = revision_after;
    }
    ged_scene_publish(gedp, GED_SCENE_DELTA_DRAW, request->view,
	changed_paths, revision_before, revision_after);
    return GED_SCENE_OK;
}

enum ged_scene_status
ged_scene_erase(struct ged *gedp,
	const struct ged_scene_erase_request *request,
	struct ged_scene_result *result)
{
    ged_scene_result_reset(result);
    if (!ged_scene_available(gedp) || !request || !request->view ||
	!request->path || !request->path[0] ||
	request->match != GED_SCENE_ERASE_EXACT ||
	request->draw_mode != GED_SCENE_DRAW_DEFAULT) {
	ged_scene_result_fail(result, GED_SCENE_INVALID,
	    "unsupported or invalid erase request");
	return GED_SCENE_INVALID;
    }

    DbiState *dbis = static_cast<DbiState *>(gedp->dbi_state);
    if (dbis->digest_path(request->path).empty()) {
	ged_scene_result_fail(result, GED_SCENE_NOT_FOUND,
	    "erase path was not found");
	return GED_SCENE_NOT_FOUND;
    }
    const uint64_t revision_before = ged_scene_revision(gedp);
    if (result)
	result->revision_before = revision_before;
    const enum ged_scene_path_state before = ged_scene_path_state_get(gedp,
	request->view, request->path, GED_SCENE_DRAW_DEFAULT);

    const char *argv[] = {"erase", request->path};
    struct bview *active_view = gedp->ged_gvp;
    gedp->ged_gvp = request->view;
    const int ret = ged_exec_erase(gedp, 2, argv);
    gedp->ged_gvp = active_view;
    if (ret != BRLCAD_OK) {
	ged_scene_result_fail(result, GED_SCENE_ERROR, "erase command failed");
	return GED_SCENE_ERROR;
    }

    std::vector<std::string> changed_paths;
    if (ged_scene_path_state_get(gedp, request->view, request->path,
	    GED_SCENE_DRAW_DEFAULT) != before)
	changed_paths.emplace_back(request->path);
    const uint64_t revision_after = changed_paths.empty() ? revision_before :
	++gedp->i->i->scene_revision;
    if (result) {
	result->changed = changed_paths.empty() ? 0 : 1;
	result->paths = changed_paths;
	result->revision_after = revision_after;
    }
    ged_scene_publish(gedp, GED_SCENE_DELTA_ERASE, request->view,
	changed_paths, revision_before, revision_after);
    return GED_SCENE_OK;
}

enum ged_scene_status
ged_scene_redraw(struct ged *gedp,
	const struct ged_scene_redraw_request *request,
	struct ged_scene_result *result)
{
    ged_scene_result_reset(result);
    if (!ged_scene_available(gedp) || !request) {
	ged_scene_result_fail(result, GED_SCENE_INVALID,
	    "scene unavailable or redraw request invalid");
	return GED_SCENE_INVALID;
    }
    if (request->path_count) {
	ged_scene_result_fail(result, GED_SCENE_INVALID,
	    "legacy scene redraw does not support path filtering");
	return GED_SCENE_INVALID;
    }

    DbiState *dbis = static_cast<DbiState *>(gedp->dbi_state);
    BViewState *requested_state = request->view ?
	dbis->get_view_state(request->view) : NULL;
    if (request->view && !requested_state) {
	ged_scene_result_fail(result, GED_SCENE_NOT_FOUND,
	    "redraw view was not found");
	return GED_SCENE_NOT_FOUND;
    }

    std::unordered_map<BViewState *, std::unordered_set<struct bview *>>
	view_groups;
    struct bu_ptbl *views = bv_set_views(&gedp->ged_views);
    for (size_t i = 0; i < BU_PTBL_LEN(views); i++) {
	struct bview *view = reinterpret_cast<struct bview *>(
	    BU_PTBL_GET(views, i));
	BViewState *view_state = dbis->get_view_state(view);
	if (!view_state || (requested_state && view_state != requested_state))
	    continue;
	view_groups[view_state].insert(view);
    }

    const uint64_t revision_before = ged_scene_revision(gedp);
    int changed = 0;
    for (auto &view_group : view_groups) {
	if (view_group.first->redraw(NULL, view_group.second, 1) &
		GED_DBISTATE_VIEW_CHANGE)
	    changed = 1;
    }
    const uint64_t revision_after = changed ?
	++gedp->i->i->scene_revision : revision_before;
    if (result) {
	result->changed = changed;
	result->group_count = view_groups.size();
	result->revision_before = revision_before;
	result->revision_after = revision_after;
    }
    if (changed) {
	const std::vector<std::string> paths;
	ged_scene_publish(gedp, GED_SCENE_DELTA_REDRAW, request->view, paths,
	    revision_before, revision_after);
    }
    return GED_SCENE_OK;
}

uint64_t
ged_scene_revision(const struct ged *gedp)
{
    return gedp && gedp->i && gedp->i->i ? gedp->i->i->scene_revision : 0;
}

enum ged_scene_delta_kind
ged_scene_delta_kind(const struct ged_scene_delta *delta)
{
    return delta ? delta->kind : GED_SCENE_DELTA_UNKNOWN;
}

ged_view_context *
ged_scene_delta_view(const struct ged_scene_delta *delta)
{
    return delta ? delta->view : NULL;
}

size_t
ged_scene_delta_path_count(const struct ged_scene_delta *delta)
{
    return delta ? delta->paths.size() : 0;
}

const char *
ged_scene_delta_path_at(const struct ged_scene_delta *delta, size_t index)
{
    return delta && index < delta->paths.size() ?
	delta->paths[index].c_str() : NULL;
}

int
ged_scene_delta_affects_all(const struct ged_scene_delta *delta)
{
    return delta ? delta->affects_all : 0;
}

size_t
ged_scene_delta_group_count(const struct ged_scene_delta *delta)
{
    return delta ? delta->group_count : 0;
}

size_t
ged_scene_delta_shape_count(const struct ged_scene_delta *delta)
{
    return delta ? delta->shape_count : 0;
}

int
ged_scene_delta_is_presentation_only(const struct ged_scene_delta *delta)
{
    return delta ? delta->presentation_only : 0;
}

uint64_t
ged_scene_delta_revision_before(const struct ged_scene_delta *delta)
{
    return delta ? delta->revision_before : 0;
}

uint64_t
ged_scene_delta_revision_after(const struct ged_scene_delta *delta)
{
    return delta ? delta->revision_after : 0;
}

const char *
ged_scene_delta_diagnostic(const struct ged_scene_delta *delta)
{
    return delta ? delta->diagnostic.c_str() : "";
}

ged_scene_observer_token
ged_scene_observer_add(struct ged *gedp, ged_scene_observer_func_t callback,
	void *client_data)
{
    if (!gedp || !gedp->i || !gedp->i->i || !callback)
	return 0;
    Ged_Internal *internal = gedp->i->i;
    const ged_scene_observer_token token = internal->scene_next_observer++;
    internal->scene_observers[token] = std::make_pair(callback, client_data);
    return token;
}

int
ged_scene_observer_remove(struct ged *gedp, ged_scene_observer_token token)
{
    if (!gedp || !gedp->i || !gedp->i->i || !token)
	return 0;
    return gedp->i->i->scene_observers.erase(token) ? 1 : 0;
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
