/*                    E D I T _ B U F . C P P
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
/** @file edit_buf.cpp
 *
 * Temporary edit buffer API.
 *
 * Maintains a per-GED registry of independently identified edit sessions.
 * Paths describe transaction targets but do not impose session uniqueness.
 * The command layer separately selects one default session per path and target
 * for multi-step CLI edits.
 *
 * The buffer is stored inside Ged_Internal (ged_private.h) and is destroyed
 * (entries abandoned) when the Ged_Internal destructor runs.
 */

#include "common.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstring>
#include <string>
#include <vector>

#include "raytrace.h"
#include "rt/db_fullpath.h"
#include "rt/edit.h"
#include "rt/func.h"
#include "rt/functab.h"

#include "ged/edit.h"

#include "./ged_private.h"
#include "./dbi.h"

static void _edit_database_changed(struct db_i *, struct directory *, int,
    void *);

/* ------------------------------------------------------------------ *
 * Ged_Internal destructor — abandons all pending edits on close.
 * ------------------------------------------------------------------ */

Ged_Internal::~Ged_Internal()
{
    if (edit_dbip)
	(void)db_rm_changed_clbk(edit_dbip, _edit_database_changed, this);
    for (auto &kv : edit_buf) {
	rt_edit_destroy(kv.second.s);
	db_free_full_path(&kv.second.dfp);
    }
    edit_buf.clear();
}


/* ------------------------------------------------------------------ *
 * Internal helper: compute a canonical path from a db_full_path.
 * Uses the full path string so there is zero collision risk.
 * ------------------------------------------------------------------ */

static std::string
_edit_buf_key(const struct db_full_path *dfp)
{
    char *pstr = db_path_to_string(dfp);
    std::string key(pstr);
    bu_free(pstr, "edit_buf path key");
    return key;
}

static std::string
_edit_default_key(const std::string &path, enum ged_edit_session_target target)
{
    return path + "\x1f" + std::to_string((int)target);
}

static std::unordered_map<std::string,
    Ged_Internal::ged_edit_buf_entry>::iterator
_edit_find_default(Ged_Internal *internal, const std::string &path,
	enum ged_edit_session_target target)
{
    if (!internal)
	return std::unordered_map<std::string,
	    Ged_Internal::ged_edit_buf_entry>::iterator();
    auto selected = internal->edit_defaults.find(_edit_default_key(path,
	target));
    if (selected == internal->edit_defaults.end())
	return internal->edit_buf.end();
    auto entry = internal->edit_buf.find(selected->second);
    if (entry == internal->edit_buf.end())
	internal->edit_defaults.erase(selected);
    return entry;
}

static void
_edit_forget_defaults(Ged_Internal *internal, const std::string &storage_key)
{
    for (auto selected = internal->edit_defaults.begin();
	selected != internal->edit_defaults.end();) {
	if (selected->second == storage_key)
	    selected = internal->edit_defaults.erase(selected);
	else
	    ++selected;
    }
}

static uint64_t
_edit_owner(Ged_Internal *internal)
{
    static std::atomic<uint64_t> next_owner(1);
    if (internal && !internal->edit_owner) {
	internal->edit_owner = next_owner.fetch_add(1);
	if (!internal->edit_owner)
	    internal->edit_owner = next_owner.fetch_add(1);
    }
    return internal ? internal->edit_owner : 0;
}

static ged_edit_session_ref
_edit_ref(Ged_Internal *internal,
	const Ged_Internal::ged_edit_buf_entry &entry)
{
    ged_edit_session_ref ref = {_edit_owner(internal), entry.id,
	entry.generation};
    return ref;
}

static void
_edit_notify(struct ged *gedp, enum ged_edit_session_event_kind kind,
	const std::string &path,
	const Ged_Internal::ged_edit_buf_entry &entry,
	const std::string *replacement_path = nullptr,
	enum ged_edit_session_invalidation_reason invalidation_reason =
	    GED_EDIT_INVALIDATION_NONE)
{
    if (!gedp || !gedp->i || !gedp->i->i)
	return;

    Ged_Internal *internal = gedp->i->i;
    Ged_Internal::ged_edit_pending_event pending;
    pending.kind = kind;
    pending.session = _edit_ref(internal, entry);
    pending.path = path;
    if (replacement_path)
	pending.replacement_path = *replacement_path;
    pending.invalidation_reason = invalidation_reason;
    pending.revision = entry.revision;
    pending.command_id = entry.last_cmd;
    internal->edit_pending_events.push_back(pending);

    if (internal->edit_dispatch_depth)
	return;

    internal->edit_dispatch_depth++;
    while (!internal->edit_pending_events.empty()) {
	Ged_Internal::ged_edit_pending_event current =
	    internal->edit_pending_events.front();
	internal->edit_pending_events.erase(internal->edit_pending_events.begin());

	std::vector<ged_edit_observer_token> observers;
	observers.reserve(internal->edit_observers.size());
	for (const auto &observer : internal->edit_observers)
	    observers.push_back(observer.first);

	struct ged_edit_session_event event;
	event.kind = current.kind;
	event.session = current.session;
	event.path = current.path.c_str();
	event.replacement_path = current.replacement_path.empty() ? nullptr :
	    current.replacement_path.c_str();
	event.invalidation_reason = current.invalidation_reason;
	event.revision = current.revision;
	event.command_id = current.command_id;

	for (ged_edit_observer_token token : observers) {
	    auto observer = internal->edit_observers.find(token);
	    if (observer == internal->edit_observers.end() ||
		!observer->second.first)
		continue;
	    observer->second.first(gedp, &event, observer->second.second);
	}
    }
    internal->edit_dispatch_depth--;
}

static std::unordered_map<std::string,
    Ged_Internal::ged_edit_buf_entry>::iterator
_edit_find_ref(Ged_Internal *internal, ged_edit_session_ref ref)
{
    if (!internal || ref.owner != _edit_owner(internal) ||
	!ref.id || !ref.generation)
	return internal ? internal->edit_buf.end() :
	    std::unordered_map<std::string,
		Ged_Internal::ged_edit_buf_entry>::iterator();
    for (auto entry = internal->edit_buf.begin();
	entry != internal->edit_buf.end(); ++entry) {
	if (entry->second.id == ref.id &&
	    entry->second.generation == ref.generation)
	    return entry;
    }
    return internal->edit_buf.end();
}

enum edit_database_change {
    EDIT_DATABASE_MODIFIED = 0,
    EDIT_DATABASE_ADDED = 1,
    EDIT_DATABASE_REMOVED = 2
};

static std::string
_edit_renamed_path(const Ged_Internal::ged_edit_buf_entry &entry,
	size_t renamed_index, const char *new_name)
{
    std::string path;
    for (size_t i = 0; i < entry.path_names.size(); ++i) {
	path.push_back('/');
	path.append(i == renamed_index ? new_name : entry.path_names[i]);
	if (i < entry.path_instances.size() && entry.path_instances[i]) {
	    path.push_back('@');
	    path.append(std::to_string(entry.path_instances[i]));
	}
    }
    return path;
}

static void
_edit_release_key(struct ged *gedp, const std::string &key,
	enum ged_edit_session_event_kind kind,
	const std::string *replacement_path = nullptr,
	enum ged_edit_session_invalidation_reason invalidation_reason =
	    GED_EDIT_INVALIDATION_NONE)
{
    if (!gedp || !gedp->i || !gedp->i->i)
	return;
    Ged_Internal *internal = gedp->i->i;
    auto entry = internal->edit_buf.find(key);
    if (entry == internal->edit_buf.end())
	return;

    const Ged_Internal::ged_edit_buf_entry final_entry = entry->second;
    rt_edit_destroy(entry->second.s);
    db_free_full_path(&entry->second.dfp);
    _edit_forget_defaults(internal, key);
    internal->edit_buf.erase(entry);
    _edit_notify(gedp, kind, final_entry.path, final_entry, replacement_path,
	invalidation_reason);
}

struct edit_invalidation {
    std::string key;
    std::string replacement_path;
    enum ged_edit_session_invalidation_reason reason =
	GED_EDIT_INVALIDATION_NONE;
};

static void
_edit_database_changed(struct db_i *dbip, struct directory *dp, int mode,
	void *client_data)
{
    Ged_Internal *internal = static_cast<Ged_Internal *>(client_data);
    if (!internal || !internal->gedp || internal->edit_dbip != dbip || !dp ||
	mode == EDIT_DATABASE_ADDED || internal->edit_buf.empty())
	return;

    std::vector<edit_invalidation> invalidations;
    for (const auto &session : internal->edit_buf) {
	const Ged_Internal::ged_edit_buf_entry &entry = session.second;
	if (entry.committing)
	    continue;

	size_t changed_index = entry.dfp.fp_len;
	for (size_t i = 0; i < entry.dfp.fp_len; ++i) {
	    if (entry.dfp.fp_names[i] == dp) {
		changed_index = i;
		break;
	    }
	}
	if (changed_index == entry.dfp.fp_len)
	    continue;

	edit_invalidation invalidation;
	invalidation.key = session.first;
	const bool terminal = changed_index + 1 == entry.dfp.fp_len;
	if (mode == EDIT_DATABASE_REMOVED) {
	    invalidation.reason = terminal ?
		GED_EDIT_INVALIDATION_SOURCE_REMOVED :
		GED_EDIT_INVALIDATION_PATH_REMOVED;
	} else if (changed_index < entry.path_names.size() && dp->d_namep &&
	    entry.path_names[changed_index] != dp->d_namep) {
	    invalidation.reason = GED_EDIT_INVALIDATION_SOURCE_RENAMED;
	    invalidation.replacement_path = _edit_renamed_path(entry,
		changed_index, dp->d_namep);
	} else {
	    int bool_op = 0;
	    const bool path_exists = terminal || db_comb_has_instance(&bool_op,
		dbip, dp, entry.dfp.fp_names[changed_index + 1],
		entry.path_instances[changed_index + 1]);
	    invalidation.reason = path_exists ?
		GED_EDIT_INVALIDATION_SOURCE_CHANGED :
		GED_EDIT_INVALIDATION_PATH_REMOVED;
	}
	invalidations.push_back(invalidation);
    }

    if (invalidations.empty())
	return;
    const bool was_closing = internal->edit_closing;
    internal->edit_closing = true;
    for (const edit_invalidation &invalidation : invalidations) {
	const std::string *replacement = invalidation.replacement_path.empty() ?
	    nullptr : &invalidation.replacement_path;
	_edit_release_key(internal->gedp, invalidation.key,
	    GED_EDIT_SESSION_INVALIDATE, replacement, invalidation.reason);
    }
    internal->edit_closing = was_closing;
}

static void
_edit_database_observer_ensure(struct ged *gedp)
{
    if (!gedp || !gedp->dbip || !gedp->i || !gedp->i->i)
	return;
    Ged_Internal *internal = gedp->i->i;
    internal->gedp = gedp;
    if (internal->edit_dbip == gedp->dbip)
	return;
    if (internal->edit_dbip) {
	(void)db_rm_changed_clbk(internal->edit_dbip,
	    _edit_database_changed, internal);
	internal->edit_dbip = nullptr;
    }
    if (db_add_changed_clbk(gedp->dbip, _edit_database_changed, internal) == 0)
	internal->edit_dbip = gedp->dbip;
}

static bool
_edit_resolve_key(std::string &key, struct ged *gedp, const char *path,
	struct db_full_path *resolved)
{
    if (!gedp || !gedp->dbip || !path || !path[0])
	return false;
    struct db_full_path local_path;
    db_full_path_init(&local_path);
    if (db_string_to_path(&local_path, gedp->dbip, path) != 0) {
	db_free_full_path(&local_path);
	return false;
    }
    key = _edit_buf_key(&local_path);
    if (resolved)
	db_dup_full_path(resolved, &local_path);
    db_free_full_path(&local_path);
    return true;
}

static std::unordered_map<std::string,
    Ged_Internal::ged_edit_buf_entry>::iterator
_edit_store(struct ged *gedp, const struct db_full_path *dfp,
	struct rt_edit *state, enum ged_edit_session_target target,
	const std::string &path, struct directory *write_dp,
	const char *occurrence_name, int occurrence_index, bool make_default)
{
    Ged_Internal *internal = gedp->i->i;
    Ged_Internal::ged_edit_buf_entry entry;
    entry.path = path;
    db_full_path_init(&entry.dfp);
    db_dup_full_path(&entry.dfp, dfp);
    entry.s = state;
    entry.write_dp = write_dp;
    entry.target = target;
    if (occurrence_name)
	entry.occurrence_name = occurrence_name;
    entry.occurrence_index = occurrence_index;
    entry.path_names.reserve(dfp->fp_len);
    entry.path_instances.reserve(dfp->fp_len);
    for (size_t i = 0; i < dfp->fp_len; ++i) {
	entry.path_names.emplace_back(dfp->fp_names[i]->d_namep);
	entry.path_instances.push_back(dfp->fp_cinst ? dfp->fp_cinst[i] : 0);
    }
    entry.id = internal->edit_next_id++;
    if (!entry.id)
	entry.id = internal->edit_next_id++;
    const std::string storage_key = std::to_string(entry.id);
    auto stored = internal->edit_buf.emplace(storage_key, entry).first;
    if (make_default)
	internal->edit_defaults[_edit_default_key(path, target)] = storage_key;
    return stored;
}


/* ------------------------------------------------------------------ *
 * Public API
 * ------------------------------------------------------------------ */

struct rt_edit *
ged_edit_buf_get(struct ged *gedp, const struct db_full_path *dfp)
{
    if (!gedp || !dfp || !gedp->i || !gedp->i->i)
	return NULL;

    Ged_Internal *gi = gedp->i->i;
    std::string key = _edit_buf_key(dfp);
    auto it = _edit_find_default(gi, key, GED_EDIT_TARGET_OBJECT);
    if (it == gi->edit_buf.end())
	return NULL;
    return it->second.s;
}


void
ged_edit_buf_set(struct ged *gedp, const struct db_full_path *dfp, struct rt_edit *s)
{
    if (!gedp || !dfp || !s || !gedp->i || !gedp->i->i)
	return;
    _edit_database_observer_ensure(gedp);
    Ged_Internal *gi = gedp->i->i;
    std::string key = _edit_buf_key(dfp);
    auto it = _edit_find_default(gi, key, GED_EDIT_TARGET_OBJECT);
    if (it != gi->edit_buf.end()) {
	const std::string storage_key = it->first;
	rt_edit_destroy(it->second.s);
	db_free_full_path(&it->second.dfp);
	_edit_forget_defaults(gi, storage_key);
	gi->edit_buf.erase(it);
    }
    (void)_edit_store(gedp, dfp, s, GED_EDIT_TARGET_OBJECT, key,
	DB_FULL_PATH_CUR_DIR(dfp), NULL, 0, true);
}


static void
_edit_buf_notify_update(struct ged *gedp, const struct db_full_path *dfp,
	int command_id, bool geometry_changed)
{
    if (!gedp || !dfp || !gedp->i || !gedp->i->i)
	return;
    Ged_Internal *internal = gedp->i->i;
    auto entry = _edit_find_default(internal, _edit_buf_key(dfp),
	GED_EDIT_TARGET_OBJECT);
    if (entry == internal->edit_buf.end())
	return;
    entry->second.revision++;
    entry->second.last_cmd = command_id;
    if (geometry_changed)
	entry->second.dirty = true;
    _edit_notify(gedp, GED_EDIT_SESSION_UPDATE, entry->second.path,
	entry->second);
}


void
ged_edit_buf_notify_update(struct ged *gedp, const struct db_full_path *dfp,
	int command_id)
{
    _edit_buf_notify_update(gedp, dfp, command_id, true);
}


void
ged_edit_buf_notify_checkpoint(struct ged *gedp,
	const struct db_full_path *dfp)
{
    if (!gedp || !dfp || !gedp->i || !gedp->i->i)
	return;
    Ged_Internal *internal = gedp->i->i;
    auto entry = _edit_find_default(internal, _edit_buf_key(dfp),
	GED_EDIT_TARGET_OBJECT);
    if (entry != internal->edit_buf.end()) {
	entry->second.checkpoint_dirty = entry->second.dirty;
	_edit_notify(gedp, GED_EDIT_SESSION_CHECKPOINT, entry->second.path,
	    entry->second);
    }
}


void
ged_edit_buf_notify_revert(struct ged *gedp,
	const struct db_full_path *dfp)
{
    if (!gedp || !dfp || !gedp->i || !gedp->i->i)
	return;
    Ged_Internal *internal = gedp->i->i;
    auto entry = _edit_find_default(internal, _edit_buf_key(dfp),
	GED_EDIT_TARGET_OBJECT);
    if (entry == internal->edit_buf.end())
	return;
    entry->second.revision++;
    entry->second.last_cmd = 0;
    entry->second.dirty = entry->second.checkpoint_dirty;
    _edit_notify(gedp, GED_EDIT_SESSION_REVERT, entry->second.path,
	entry->second);
}


int
ged_edit_buf_promote(struct ged *gedp, const struct db_full_path *dfp)
{
    if (!gedp || !dfp || !gedp->i || !gedp->i->i)
	return BRLCAD_ERROR;

    Ged_Internal *gi = gedp->i->i;
    std::string key = _edit_buf_key(dfp);
    auto it = _edit_find_default(gi, key, GED_EDIT_TARGET_OBJECT);
    if (it == gi->edit_buf.end())
	return BRLCAD_ERROR;

    const Ged_Internal::ged_edit_buf_entry final_entry = it->second;
    struct rt_edit *s = it->second.s;
    struct directory *dp = it->second.write_dp;

    int ret = BRLCAD_ERROR;
    if (dp && gedp->dbip) {
	/* rt_db_put_internal frees es_int internals and re-initializes it
	 * via RT_DB_INTERNAL_INIT, so the subsequent rt_edit_destroy is safe. */
	it->second.committing = true;
	int put_ret = rt_db_put_internal(dp, gedp->dbip, &s->es_int);
	it->second.committing = false;
	ret = (put_ret < 0) ? BRLCAD_ERROR : BRLCAD_OK;
    }

    rt_edit_destroy(s);
    db_free_full_path(&it->second.dfp);
    const std::string storage_key = it->first;
    _edit_forget_defaults(gi, storage_key);
    gi->edit_buf.erase(it);

    if (ret == BRLCAD_OK)
	_edit_notify(gedp, GED_EDIT_SESSION_COMMIT, final_entry.path,
	    final_entry);

    /* Notify DbiState of the change */
    if (ret == BRLCAD_OK && gedp->dbi_state) {
	DbiState *dbis = (DbiState *)gedp->dbi_state;
	dbis->update();
    }

    return ret;
}


void
ged_edit_buf_abandon(struct ged *gedp, const struct db_full_path *dfp)
{
    if (!gedp || !dfp || !gedp->i || !gedp->i->i)
	return;

    Ged_Internal *gi = gedp->i->i;
    std::string key = _edit_buf_key(dfp);
    auto it = _edit_find_default(gi, key, GED_EDIT_TARGET_OBJECT);
    if (it == gi->edit_buf.end())
	return;

    _edit_release_key(gedp, it->first, GED_EDIT_SESSION_CANCEL);
}


void
ged_edit_buf_flush(struct ged *gedp)
{
    if (!gedp || !gedp->i || !gedp->i->i)
	return;

    Ged_Internal *gi = gedp->i->i;
    if (gi->edit_buf.empty())
	return;

    /* Collect keys up front to avoid iterator invalidation during erase */
    std::vector<std::string> keys;
    keys.reserve(gi->edit_defaults.size());
    for (const auto &selected : gi->edit_defaults) {
	if (std::find(keys.begin(), keys.end(), selected.second) == keys.end())
	    keys.push_back(selected.second);
    }

    bool any_written = false;
    for (const std::string &key : keys) {
	auto it = gi->edit_buf.find(key);
	if (it == gi->edit_buf.end())
	    continue;

	const Ged_Internal::ged_edit_buf_entry final_entry = it->second;
	struct rt_edit *s = it->second.s;
	struct directory *dp = it->second.write_dp;

	bool written = false;
	if (dp && gedp->dbip) {
	    it->second.committing = true;
	    written = rt_db_put_internal(dp, gedp->dbip, &s->es_int) >= 0;
	    it->second.committing = false;
	    any_written = any_written || written;
	}

	rt_edit_destroy(s);
	db_free_full_path(&it->second.dfp);
	_edit_forget_defaults(gi, key);
	gi->edit_buf.erase(it);
	if (written)
	    _edit_notify(gedp, GED_EDIT_SESSION_COMMIT, final_entry.path,
		final_entry);
    }

    if (any_written && gedp->dbi_state) {
	DbiState *dbis = (DbiState *)gedp->dbi_state;
	dbis->update();
    }
}


static void
_edit_sessions_close(struct ged *gedp, bool permanent)
{
    if (!gedp || !gedp->i || !gedp->i->i)
	return;
    Ged_Internal *internal = gedp->i->i;
    internal->edit_closing = true;
    if (internal->edit_dbip) {
	(void)db_rm_changed_clbk(internal->edit_dbip,
	    _edit_database_changed, internal);
	internal->edit_dbip = nullptr;
    }

    std::vector<std::string> keys;
    keys.reserve(internal->edit_buf.size());
    for (const auto &entry : internal->edit_buf)
	keys.push_back(entry.first);
    for (const std::string &key : keys)
	_edit_release_key(gedp, key, GED_EDIT_SESSION_INVALIDATE, nullptr,
	    GED_EDIT_INVALIDATION_DATABASE_CLOSED);

    if (!permanent)
	internal->edit_closing = false;
}


void
ged_edit_sessions_database_close_private(struct ged *gedp)
{
    _edit_sessions_close(gedp, false);
}


void
ged_edit_sessions_close_private(struct ged *gedp)
{
    _edit_sessions_close(gedp, true);
}


int
ged_edit_session_ref_is_null(ged_edit_session_ref session)
{
    return !session.owner && !session.id && !session.generation;
}


enum ged_edit_status
ged_edit_sessions_get(struct ged *gedp, const char *path,
	enum ged_edit_session_target target, ged_edit_session_ref *sessions,
	size_t capacity, size_t *count)
{
    if (count)
	*count = 0;
    if (!gedp || !path || !path[0] || !count ||
	(capacity && !sessions) || !gedp->i || !gedp->i->i ||
	(target != GED_EDIT_TARGET_OBJECT &&
	 target != GED_EDIT_TARGET_OCCURRENCE))
	return GED_EDIT_INVALID;

    std::string key;
    if (!_edit_resolve_key(key, gedp, path, NULL))
	return GED_EDIT_NOT_FOUND;
    Ged_Internal *internal = gedp->i->i;
    for (const auto &entry : internal->edit_buf) {
	if (entry.second.path != key || entry.second.target != target)
	    continue;
	if (*count < capacity)
	    sessions[*count] = _edit_ref(internal, entry.second);
	(*count)++;
    }
    return GED_EDIT_OK;
}


enum ged_edit_status
ged_edit_session_default_get(struct ged *gedp, const char *path,
	enum ged_edit_session_target target, int create,
	ged_edit_session_ref *session)
{
    if (session)
	*session = GED_EDIT_SESSION_REF_NULL;
    if (!gedp || !path || !path[0] || !session || !gedp->i || !gedp->i->i)
	return GED_EDIT_INVALID;
    std::string key;
    if (!_edit_resolve_key(key, gedp, path, NULL))
	return GED_EDIT_NOT_FOUND;
    Ged_Internal *internal = gedp->i->i;
    auto entry = _edit_find_default(internal, key, target);
    if (entry != internal->edit_buf.end()) {
	*session = _edit_ref(internal, entry->second);
	return GED_EDIT_OK;
    }
    if (!create)
	return GED_EDIT_NOT_FOUND;
    enum ged_edit_status status = ged_edit_session_begin(gedp, path, target,
	session);
    if (status != GED_EDIT_OK)
	return status;
    entry = _edit_find_ref(internal, *session);
    if (entry == internal->edit_buf.end())
	return GED_EDIT_ERROR;
    internal->edit_defaults[_edit_default_key(entry->second.path, target)] =
	entry->first;
    return GED_EDIT_OK;
}


enum ged_edit_status
ged_edit_session_begin(struct ged *gedp, const char *path,
	enum ged_edit_session_target target, ged_edit_session_ref *session)
{
    if (session)
	*session = GED_EDIT_SESSION_REF_NULL;
    if (!gedp || !path || !path[0] || !session || !gedp->i ||
	!gedp->i->i || !gedp->dbip ||
	(target != GED_EDIT_TARGET_OBJECT &&
	 target != GED_EDIT_TARGET_OCCURRENCE))
	return GED_EDIT_INVALID;

    if (gedp->i->i->edit_closing)
	return GED_EDIT_REJECTED;

    struct db_full_path full_path;
    db_full_path_init(&full_path);
    std::string key;
    if (!_edit_resolve_key(key, gedp, path, &full_path))
	return GED_EDIT_NOT_FOUND;

    Ged_Internal *internal = gedp->i->i;
    struct db_full_path edit_path;
    db_full_path_init(&edit_path);
    struct directory *write_dp = DB_FULL_PATH_CUR_DIR(&full_path);
    const char *occurrence_name = NULL;
    int occurrence_index = 0;
    if (target == GED_EDIT_TARGET_OCCURRENCE) {
	if (full_path.fp_len < 2) {
	    db_free_full_path(&full_path);
	    return GED_EDIT_INVALID;
	}
	occurrence_name = DB_FULL_PATH_CUR_DIR(&full_path)->d_namep;
	occurrence_index = DB_FULL_PATH_CUR_COMB_INST(&full_path);
	full_path.fp_len--;
	char *parent_string = db_path_to_string(&full_path);
	full_path.fp_len++;
	if (!parent_string || db_string_to_path(&edit_path, gedp->dbip,
		parent_string) != 0) {
	    if (parent_string)
		bu_free(parent_string, "occurrence parent path");
	    db_free_full_path(&full_path);
	    return GED_EDIT_ERROR;
	}
	bu_free(parent_string, "occurrence parent path");
	write_dp = DB_FULL_PATH_CUR_DIR(&edit_path);
	if (!(write_dp->d_flags & RT_DIR_COMB)) {
	    db_free_full_path(&edit_path);
	    db_free_full_path(&full_path);
	    return GED_EDIT_INVALID;
	}
    } else {
	db_dup_full_path(&edit_path, &full_path);
    }
    struct bn_tol tolerance = BN_TOL_INIT_TOL;
    struct rt_edit *state = rt_edit_create(&edit_path, gedp->dbip,
	&tolerance, NULL);
    if (!state) {
	db_free_full_path(&edit_path);
	db_free_full_path(&full_path);
	return GED_EDIT_ERROR;
    }
    if (target == GED_EDIT_TARGET_OCCURRENCE) {
	mat_t matrix;
	if (rt_edit_comb_instance_matrix_get(state, occurrence_name,
		occurrence_index, matrix) != BRLCAD_OK) {
	    rt_edit_destroy(state);
	    db_free_full_path(&edit_path);
	    db_free_full_path(&full_path);
	    return GED_EDIT_NOT_FOUND;
	}
    }
    _edit_database_observer_ensure(gedp);
    auto added = _edit_store(gedp, &full_path, state, target, key, write_dp,
	occurrence_name, occurrence_index, false);
    if (added == internal->edit_buf.end()) {
	rt_edit_destroy(state);
	db_free_full_path(&edit_path);
	db_free_full_path(&full_path);
	return GED_EDIT_ERROR;
    }
    *session = _edit_ref(internal, added->second);
    _edit_notify(gedp, GED_EDIT_SESSION_BEGIN, key, added->second);
    db_free_full_path(&edit_path);
    db_free_full_path(&full_path);
    return GED_EDIT_OK;
}


enum ged_edit_status
ged_edit_session_info_get(struct ged *gedp, ged_edit_session_ref session,
	struct ged_edit_session_info *info)
{
    if (!gedp || !info || !gedp->i || !gedp->i->i)
	return GED_EDIT_INVALID;
    Ged_Internal *internal = gedp->i->i;
    auto entry = _edit_find_ref(internal, session);
    if (entry == internal->edit_buf.end())
	return session.owner == _edit_owner(internal) ? GED_EDIT_STALE :
	    GED_EDIT_INVALID;
    info->session = session;
    info->target = entry->second.target;
    info->revision = entry->second.revision;
    info->command_id = entry->second.last_cmd;
    info->primitive_type = entry->second.s ?
	entry->second.s->es_int.idb_type : -1;
    info->dirty = entry->second.dirty ? 1 : 0;
    return GED_EDIT_OK;
}


enum ged_edit_status
ged_edit_session_path_get(struct ged *gedp, ged_edit_session_ref session,
	struct bu_vls *path)
{
    if (!gedp || !path || !gedp->i || !gedp->i->i)
	return GED_EDIT_INVALID;
    Ged_Internal *internal = gedp->i->i;
    auto entry = _edit_find_ref(internal, session);
    if (entry == internal->edit_buf.end())
	return session.owner == _edit_owner(internal) ? GED_EDIT_STALE :
	    GED_EDIT_INVALID;
    bu_vls_strcat(path, entry->second.path.c_str());
    return GED_EDIT_OK;
}


enum ged_edit_status
ged_edit_session_descriptor_get(struct ged *gedp,
	ged_edit_session_ref session,
	const struct rt_edit_prim_desc **descriptor)
{
    if (descriptor)
	*descriptor = NULL;
    if (!gedp || !descriptor || !gedp->i || !gedp->i->i)
	return GED_EDIT_INVALID;
    Ged_Internal *internal = gedp->i->i;
    auto entry = _edit_find_ref(internal, session);
    if (entry == internal->edit_buf.end())
	return session.owner == _edit_owner(internal) ? GED_EDIT_STALE :
	    GED_EDIT_INVALID;
    if (entry->second.target != GED_EDIT_TARGET_OBJECT)
	return GED_EDIT_UNSUPPORTED;
    const int type = entry->second.s ? entry->second.s->es_int.idb_type : -1;
    if (type < 0 || EDOBJ[type].magic != RT_FUNCTAB_MAGIC ||
	!EDOBJ[type].ft_edit_desc)
	return GED_EDIT_UNSUPPORTED;
    *descriptor = EDOBJ[type].ft_edit_desc();
    return *descriptor ? GED_EDIT_OK : GED_EDIT_UNSUPPORTED;
}


enum ged_edit_status
ged_edit_session_command_values_get(struct ged *gedp,
	ged_edit_session_ref session, int command_id,
	struct rt_edit_cmd_values *result)
{
    if (result)
	rt_edit_cmd_values_init(result);
    if (!gedp || !result || !gedp->i || !gedp->i->i)
	return GED_EDIT_INVALID;
    Ged_Internal *internal = gedp->i->i;
    auto entry = _edit_find_ref(internal, session);
    if (entry == internal->edit_buf.end())
	return session.owner == _edit_owner(internal) ? GED_EDIT_STALE :
	    GED_EDIT_INVALID;
    if (entry->second.target != GED_EDIT_TARGET_OBJECT)
	return GED_EDIT_UNSUPPORTED;
    const enum rt_edit_value_status status = rt_edit_cmd_values_get(
	entry->second.s, command_id, result);
    if (status == RT_EDIT_VALUE_ERROR)
	return GED_EDIT_ERROR;
    return status == RT_EDIT_VALUE_OK ? GED_EDIT_OK : GED_EDIT_UNSUPPORTED;
}


enum ged_edit_status
ged_edit_session_parameter_bounds_get(struct ged *gedp,
	ged_edit_session_ref session, int command_id, int parameter_index,
	struct rt_edit_param_bounds *bounds)
{
    if (bounds)
	memset(bounds, 0, sizeof(*bounds));
    if (!gedp || !bounds || !gedp->i || !gedp->i->i)
	return GED_EDIT_INVALID;
    Ged_Internal *internal = gedp->i->i;
    auto entry = _edit_find_ref(internal, session);
    if (entry == internal->edit_buf.end())
	return session.owner == _edit_owner(internal) ? GED_EDIT_STALE :
	    GED_EDIT_INVALID;
    if (entry->second.target != GED_EDIT_TARGET_OBJECT)
	return GED_EDIT_UNSUPPORTED;
    return rt_edit_param_bounds_get(entry->second.s, command_id,
	parameter_index, bounds) == BRLCAD_OK ? GED_EDIT_OK :
	GED_EDIT_UNSUPPORTED;
}


static const struct rt_edit_cmd_desc *
_edit_command(const struct rt_edit_prim_desc *descriptor, int command_id)
{
    if (!descriptor)
	return NULL;
    for (int i = 0; i < descriptor->ncmd; ++i) {
	if (descriptor->cmds[i].cmd_id == command_id)
	    return &descriptor->cmds[i];
    }
    return NULL;
}


static size_t
_edit_parameter_width(const struct rt_edit_param_desc *parameter)
{
    if (!parameter)
	return 0;
    switch (parameter->type) {
	case RT_EDIT_PARAM_SCALAR:
	case RT_EDIT_PARAM_INTEGER:
	case RT_EDIT_PARAM_BOOLEAN:
	case RT_EDIT_PARAM_ENUM:
	    return 1;
	case RT_EDIT_PARAM_SCALAR_LIST:
	case RT_EDIT_PARAM_INTEGER_LIST:
	    return (size_t)parameter->nenum;
	case RT_EDIT_PARAM_POINT2:
	case RT_EDIT_PARAM_VECTOR2:
	    return 2;
	case RT_EDIT_PARAM_POINT:
	case RT_EDIT_PARAM_VECTOR:
	case RT_EDIT_PARAM_COLOR:
	    return 3;
	case RT_EDIT_PARAM_MATRIX:
	    return 16;
	default:
	    return 0;
    }
}


static bool
_edit_integer_parameter(int type)
{
    return type == RT_EDIT_PARAM_INTEGER || type == RT_EDIT_PARAM_BOOLEAN ||
	type == RT_EDIT_PARAM_ENUM || type == RT_EDIT_PARAM_INTEGER_LIST;
}


static bool
_edit_enum_value_valid(const struct rt_edit_param_desc *parameter,
	fastf_t value)
{
    if (!parameter || parameter->type != RT_EDIT_PARAM_ENUM ||
	parameter->nenum <= 0 || !parameter->enum_ids)
	return true;
    const int enum_value = (int)nearbyint(value);
    for (int i = 0; i < parameter->nenum; ++i) {
	if (parameter->enum_ids[i] == enum_value)
	    return true;
    }
    return false;
}


static bool
_edit_command_input_valid(const struct rt_edit *state,
	const struct rt_edit_cmd_desc *command,
	const struct ged_edit_command_input *input)
{
    if (!state || !command || !input || command->nparam < 0 ||
	(command->nparam && !command->params) ||
	input->value_count > RT_EDIT_MAXPARA ||
	input->string_count > RT_EDIT_MAXSTR)
	return false;

    size_t required_values = 0;
    size_t required_strings = 0;
    for (int i = 0; i < command->nparam; ++i) {
	const struct rt_edit_param_desc *parameter = &command->params[i];
	if (parameter->index < 0)
	    return false;
	if (parameter->type == RT_EDIT_PARAM_STRING) {
	    const size_t index = (size_t)parameter->index;
	    required_strings = std::max(required_strings, index + 1);
	    if (!input->strings || index >= input->string_count ||
		!input->strings[index] ||
		strlen(input->strings[index]) >= RT_EDIT_MAXSTR_LEN)
		return false;
	    continue;
	}

	const bool list_parameter =
	    parameter->type == RT_EDIT_PARAM_SCALAR_LIST ||
	    parameter->type == RT_EDIT_PARAM_INTEGER_LIST;
	const size_t index = (size_t)parameter->index;
	if (list_parameter && index > input->value_count)
	    return false;
	const size_t width = list_parameter ?
	    input->value_count - index :
	    _edit_parameter_width(parameter);
	if (!width || index + width > RT_EDIT_MAXPARA ||
	    (list_parameter && width < (size_t)parameter->nenum))
	    return false;
	required_values = std::max(required_values, index + width);
	if (!input->values || index + width > input->value_count)
	    return false;

	struct rt_edit_param_bounds bounds = {};
	if (rt_edit_param_bounds_get(state, command->cmd_id, i, &bounds) !=
		BRLCAD_OK)
	    return false;
	for (size_t value_index = index; value_index < index + width;
	    ++value_index) {
	    const fastf_t value = input->values[value_index];
	    if (!std::isfinite(value) ||
		(bounds.has_minimum && value < bounds.minimum) ||
		(bounds.has_maximum && value > bounds.maximum) ||
		(_edit_integer_parameter(parameter->type) &&
		 fabs(value - nearbyint(value)) > SMALL_FASTF) ||
		(parameter->type == RT_EDIT_PARAM_BOOLEAN &&
		 (value < 0.0 || value > 1.0)) ||
		!_edit_enum_value_valid(parameter, value))
		return false;
	}
    }
    return input->value_count == required_values &&
	input->string_count == required_strings;
}


enum ged_edit_status
ged_edit_session_apply(struct ged *gedp, ged_edit_session_ref session,
	const struct ged_edit_command_input *input)
{
    if (!gedp || !input || !gedp->i || !gedp->i->i)
	return GED_EDIT_INVALID;
    Ged_Internal *internal = gedp->i->i;
    auto entry = _edit_find_ref(internal, session);
    if (entry == internal->edit_buf.end())
	return session.owner == _edit_owner(internal) ? GED_EDIT_STALE :
	    GED_EDIT_INVALID;
    if (entry->second.target != GED_EDIT_TARGET_OBJECT)
	return GED_EDIT_UNSUPPORTED;

    struct rt_edit *state = entry->second.s;
    const int type = state ? state->es_int.idb_type : -1;
    if (type < 0 || EDOBJ[type].magic != RT_FUNCTAB_MAGIC ||
	!EDOBJ[type].ft_edit_desc)
	return GED_EDIT_UNSUPPORTED;
    const struct rt_edit_prim_desc *descriptor = EDOBJ[type].ft_edit_desc();
    const struct rt_edit_cmd_desc *command = _edit_command(descriptor,
	input->command_id);
    if (!command)
	return GED_EDIT_UNSUPPORTED;
    if (!_edit_command_input_valid(state, command, input))
	return GED_EDIT_INVALID;

    memset(state->e_para, 0, sizeof(state->e_para));
    memset(state->e_str, 0, sizeof(state->e_str));
    state->e_inpara = (int)input->value_count;
    state->e_nstr = 0;
    if (input->value_count)
	memcpy(state->e_para, input->values,
	    input->value_count * sizeof(fastf_t));
    for (int i = 0; i < command->nparam; ++i) {
	const struct rt_edit_param_desc *parameter = &command->params[i];
	if (parameter->type == RT_EDIT_PARAM_STRING)
	    rt_edit_set_str(state, parameter->index,
		input->strings[parameter->index]);
    }

    rt_edit_set_edflag(state, command->cmd_id);
    const int process_result = rt_edit_process(state);
    if (process_result != BRLCAD_OK) {
	if (state->log_str && bu_vls_strlen(state->log_str) &&
	    gedp->ged_result_str)
	    bu_vls_printf(gedp->ged_result_str, "%s",
		bu_vls_cstr(state->log_str));
	if (state->log_str)
	    bu_vls_trunc(state->log_str, 0);
	return GED_EDIT_REJECTED;
    }

    entry->second.revision++;
    entry->second.last_cmd = command->cmd_id;
    if (!command->category || !BU_STR_EQUAL(command->category, "selection"))
	entry->second.dirty = true;
    _edit_notify(gedp, GED_EDIT_SESSION_UPDATE, entry->second.path,
	entry->second);
    return GED_EDIT_OK;
}


enum ged_edit_status
ged_edit_session_occurrence_matrix_get(struct ged *gedp,
	ged_edit_session_ref session, mat_t matrix)
{
    if (!gedp || !matrix || !gedp->i || !gedp->i->i)
	return GED_EDIT_INVALID;
    Ged_Internal *internal = gedp->i->i;
    auto entry = _edit_find_ref(internal, session);
    if (entry == internal->edit_buf.end())
	return session.owner == _edit_owner(internal) ? GED_EDIT_STALE :
	    GED_EDIT_INVALID;
    if (entry->second.target != GED_EDIT_TARGET_OCCURRENCE)
	return GED_EDIT_UNSUPPORTED;
    return rt_edit_comb_instance_matrix_get(entry->second.s,
	entry->second.occurrence_name.c_str(), entry->second.occurrence_index,
	matrix) == BRLCAD_OK ? GED_EDIT_OK : GED_EDIT_ERROR;
}


enum ged_edit_status
ged_edit_session_occurrence_matrix_set(struct ged *gedp,
	ged_edit_session_ref session, const mat_t matrix)
{
    if (!gedp || !matrix || !gedp->i || !gedp->i->i)
	return GED_EDIT_INVALID;
    for (size_t i = 0; i < ELEMENTS_PER_MAT; ++i) {
	if (!std::isfinite(matrix[i]))
	    return GED_EDIT_INVALID;
    }
    Ged_Internal *internal = gedp->i->i;
    auto entry = _edit_find_ref(internal, session);
    if (entry == internal->edit_buf.end())
	return session.owner == _edit_owner(internal) ? GED_EDIT_STALE :
	    GED_EDIT_INVALID;
    if (entry->second.target != GED_EDIT_TARGET_OCCURRENCE)
	return GED_EDIT_UNSUPPORTED;
    if (rt_edit_comb_instance_matrix_set(entry->second.s,
	    entry->second.occurrence_name.c_str(), entry->second.occurrence_index,
	    matrix) != BRLCAD_OK)
	return GED_EDIT_ERROR;
    entry->second.revision++;
    entry->second.dirty = true;
    entry->second.last_cmd = 0;
    _edit_notify(gedp, GED_EDIT_SESSION_UPDATE, entry->second.path,
	entry->second);
    return GED_EDIT_OK;
}


enum ged_edit_status
ged_edit_session_checkpoint(struct ged *gedp, ged_edit_session_ref session)
{
    if (!gedp || !gedp->i || !gedp->i->i)
	return GED_EDIT_INVALID;
    Ged_Internal *internal = gedp->i->i;
    auto entry = _edit_find_ref(internal, session);
    if (entry == internal->edit_buf.end())
	return session.owner == _edit_owner(internal) ? GED_EDIT_STALE :
	    GED_EDIT_INVALID;
    if (rt_edit_checkpoint(entry->second.s) != BRLCAD_OK)
	return GED_EDIT_ERROR;
    entry->second.checkpoint_dirty = entry->second.dirty;
    _edit_notify(gedp, GED_EDIT_SESSION_CHECKPOINT, entry->second.path,
	entry->second);
    return GED_EDIT_OK;
}


enum ged_edit_status
ged_edit_session_revert(struct ged *gedp, ged_edit_session_ref session)
{
    if (!gedp || !gedp->i || !gedp->i->i)
	return GED_EDIT_INVALID;
    Ged_Internal *internal = gedp->i->i;
    auto entry = _edit_find_ref(internal, session);
    if (entry == internal->edit_buf.end())
	return session.owner == _edit_owner(internal) ? GED_EDIT_STALE :
	    GED_EDIT_INVALID;
    if (rt_edit_revert(entry->second.s) != BRLCAD_OK)
	return GED_EDIT_ERROR;
    entry->second.revision++;
    entry->second.last_cmd = 0;
    entry->second.dirty = entry->second.checkpoint_dirty;
    _edit_notify(gedp, GED_EDIT_SESSION_REVERT, entry->second.path,
	entry->second);
    return GED_EDIT_OK;
}


enum ged_edit_status
ged_edit_session_commit(struct ged *gedp, ged_edit_session_ref session)
{
    if (!gedp || !gedp->i || !gedp->i->i)
	return GED_EDIT_INVALID;
    Ged_Internal *internal = gedp->i->i;
    auto entry = _edit_find_ref(internal, session);
    if (entry == internal->edit_buf.end())
	return session.owner == _edit_owner(internal) ? GED_EDIT_STALE :
	    GED_EDIT_INVALID;
    if (!entry->second.dirty) {
	const std::string key = entry->first;
	const Ged_Internal::ged_edit_buf_entry final_entry = entry->second;
	rt_edit_destroy(entry->second.s);
	db_free_full_path(&entry->second.dfp);
	_edit_forget_defaults(internal, key);
	internal->edit_buf.erase(entry);
	_edit_notify(gedp, GED_EDIT_SESSION_COMMIT, final_entry.path,
	    final_entry);
	return GED_EDIT_OK;
    }
    const std::string key = entry->first;
    const Ged_Internal::ged_edit_buf_entry final_entry = entry->second;
    entry->second.committing = true;
    const int result = rt_db_put_internal(entry->second.write_dp, gedp->dbip,
	&entry->second.s->es_int);
    entry->second.committing = false;
    rt_edit_destroy(entry->second.s);
    db_free_full_path(&entry->second.dfp);
    _edit_forget_defaults(internal, key);
    internal->edit_buf.erase(entry);
    if (result < 0)
	return GED_EDIT_ERROR;
    _edit_notify(gedp, GED_EDIT_SESSION_COMMIT, final_entry.path,
	final_entry);
    if (gedp->dbi_state)
	((DbiState *)gedp->dbi_state)->update();
    return GED_EDIT_OK;
}


enum ged_edit_status
ged_edit_session_cancel(struct ged *gedp, ged_edit_session_ref session)
{
    if (!gedp || !gedp->i || !gedp->i->i)
	return GED_EDIT_INVALID;
    Ged_Internal *internal = gedp->i->i;
    auto entry = _edit_find_ref(internal, session);
    if (entry == internal->edit_buf.end())
	return session.owner == _edit_owner(internal) ? GED_EDIT_STALE :
	    GED_EDIT_INVALID;
    _edit_release_key(gedp, entry->first, GED_EDIT_SESSION_CANCEL);
    return GED_EDIT_OK;
}


enum ged_edit_status
ged_edit_session_internal_copy(struct ged *gedp,
	ged_edit_session_ref session, struct rt_db_internal *intern)
{
    if (!gedp || !intern || !gedp->i || !gedp->i->i)
	return GED_EDIT_INVALID;
    Ged_Internal *internal = gedp->i->i;
    auto entry = _edit_find_ref(internal, session);
    if (entry == internal->edit_buf.end())
	return session.owner == _edit_owner(internal) ? GED_EDIT_STALE :
	    GED_EDIT_INVALID;

    if (entry->second.target != GED_EDIT_TARGET_OBJECT)
	return GED_EDIT_UNSUPPORTED;

    struct bu_external external;
    BU_EXTERNAL_INIT(&external);
    if (rt_obj_export(&external, &entry->second.s->es_int, 1.0,
	entry->second.s->dbip) < 0)
	return GED_EDIT_ERROR;
    RT_DB_INTERNAL_INIT(intern);
    intern->idb_minor_type = entry->second.s->es_int.idb_type;
    mat_t identity;
    MAT_IDN(identity);
    const int result = rt_obj_import(intern, &external, identity,
	entry->second.s->dbip);
    bu_free_external(&external);
    return result < 0 ? GED_EDIT_ERROR : GED_EDIT_OK;
}


ged_edit_observer_token
ged_edit_observer_add(struct ged *gedp, ged_edit_observer_func_t callback,
	void *client_data)
{
    if (!gedp || !callback || !gedp->i || !gedp->i->i)
	return 0;
    Ged_Internal *internal = gedp->i->i;
    ged_edit_observer_token token = internal->edit_next_observer++;
    if (!token)
	token = internal->edit_next_observer++;
    internal->edit_observers[token] = std::make_pair(callback, client_data);
    return token;
}


int
ged_edit_observer_remove(struct ged *gedp, ged_edit_observer_token token)
{
    if (!gedp || !token || !gedp->i || !gedp->i->i)
	return 0;
    return gedp->i->i->edit_observers.erase(token) ? 1 : 0;
}


/*
 * Local Variables:
 * tab-width: 8
 * mode: C++
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
