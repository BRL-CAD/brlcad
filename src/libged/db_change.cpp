/*                   D B _ C H A N G E . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 */
/** @file libged/db_change.cpp
 *
 * Public value-owned access to GED database change batches.
 */

#include "common.h"

#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "ged/database.h"

#include "dbi.h"

struct ged_db_change_set {
    uint64_t revision = 0;
    unsigned int flags = 0;
    std::vector<std::string> added;
    std::vector<std::string> modified;
    std::vector<std::string> removed;
    std::vector<std::pair<std::string, std::string>> renamed;
    std::vector<std::string> paths;
};

static DbiState *
ged_db_change_state(struct ged *gedp)
{
    return (gedp && gedp->dbi_state) ?
	static_cast<DbiState *>(gedp->dbi_state) : nullptr;
}

static const std::vector<std::string> *
ged_db_change_objects(const struct ged_db_change_set *changes,
	enum ged_db_change_object_kind kind)
{
    if (!changes)
	return nullptr;

    switch (kind) {
	case GED_DB_CHANGE_OBJECT_ADDED:
	    return &changes->added;
	case GED_DB_CHANGE_OBJECT_MODIFIED:
	    return &changes->modified;
	case GED_DB_CHANGE_OBJECT_REMOVED:
	    return &changes->removed;
	default:
	    return nullptr;
    }
}

int
ged_db_change_available(struct ged *gedp)
{
    return ged_db_change_state(gedp) ? 1 : 0;
}

struct ged_db_change_set *
ged_db_change_set_create(struct ged *gedp, uint64_t after_revision)
{
    DbiState *state = ged_db_change_state(gedp);
    if (!state)
	return nullptr;

    struct ged_db_change_set *changes = new ged_db_change_set;
    std::set<std::string> added;
    std::set<std::string> modified;
    std::set<std::string> removed;
    std::map<std::string, std::string> renamed;
    std::set<std::string> paths;

    for (const DbiStateChange &change :
	state->change_snapshots(after_revision)) {
	changes->revision = change.revision;
	if (change.structural)
	    changes->flags |= GED_DB_CHANGE_STRUCTURAL;
	if (change.nref_dirty)
	    changes->flags |= GED_DB_CHANGE_NREF_DIRTY;
	if (change.rebuild)
	    changes->flags |= GED_DB_CHANGE_REBUILD;
	added.insert(change.added.begin(), change.added.end());
	modified.insert(change.modified.begin(), change.modified.end());
	removed.insert(change.removed.begin(), change.removed.end());
	for (const auto &rename : change.renamed)
	    renamed[rename.first] = rename.second;
	paths.insert(change.paths.begin(), change.paths.end());
    }

    changes->added.assign(added.begin(), added.end());
    changes->modified.assign(modified.begin(), modified.end());
    changes->removed.assign(removed.begin(), removed.end());
    changes->renamed.assign(renamed.begin(), renamed.end());
    changes->paths.assign(paths.begin(), paths.end());
    return changes;
}

void
ged_db_change_set_destroy(struct ged_db_change_set *changes)
{
    delete changes;
}

uint64_t
ged_db_change_set_revision(const struct ged_db_change_set *changes)
{
    return changes ? changes->revision : 0;
}

unsigned int
ged_db_change_set_flags(const struct ged_db_change_set *changes)
{
    return changes ? changes->flags : 0;
}

size_t
ged_db_change_set_object_count(const struct ged_db_change_set *changes,
	enum ged_db_change_object_kind kind)
{
    const std::vector<std::string> *objects =
	ged_db_change_objects(changes, kind);
    return objects ? objects->size() : 0;
}

const char *
ged_db_change_set_object_at(const struct ged_db_change_set *changes,
	enum ged_db_change_object_kind kind, size_t index)
{
    const std::vector<std::string> *objects =
	ged_db_change_objects(changes, kind);
    return (objects && index < objects->size()) ?
	(*objects)[index].c_str() : nullptr;
}

size_t
ged_db_change_set_rename_count(const struct ged_db_change_set *changes)
{
    return changes ? changes->renamed.size() : 0;
}

const char *
ged_db_change_set_rename_old_at(const struct ged_db_change_set *changes,
	size_t index)
{
    return (changes && index < changes->renamed.size()) ?
	changes->renamed[index].first.c_str() : nullptr;
}

const char *
ged_db_change_set_rename_new_at(const struct ged_db_change_set *changes,
	size_t index)
{
    return (changes && index < changes->renamed.size()) ?
	changes->renamed[index].second.c_str() : nullptr;
}

size_t
ged_db_change_set_path_count(const struct ged_db_change_set *changes)
{
    return changes ? changes->paths.size() : 0;
}

const char *
ged_db_change_set_path_at(const struct ged_db_change_set *changes,
	size_t index)
{
    return (changes && index < changes->paths.size()) ?
	changes->paths[index].c_str() : nullptr;
}

unsigned int
ged_db_change_reconcile(struct ged *gedp)
{
    DbiState *state = ged_db_change_state(gedp);
    if (!state)
	return GED_DB_CHANGE_EFFECT_NONE;

    const unsigned long long legacy_effects = state->reconcile_changes();
    unsigned int effects = GED_DB_CHANGE_EFFECT_NONE;
    if (legacy_effects & GED_DBISTATE_DB_CHANGE)
	effects |= GED_DB_CHANGE_EFFECT_DATABASE;
    if (legacy_effects & GED_DBISTATE_VIEW_CHANGE)
	effects |= GED_DB_CHANGE_EFFECT_VIEW;
    return effects;
}

int
ged_db_change_acknowledge(struct ged *gedp, uint64_t revision)
{
    DbiState *state = ged_db_change_state(gedp);
    if (!state)
	return 0;
    state->acknowledge_changes(revision);
    return 1;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
