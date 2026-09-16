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
 * Maintains a per-gedp map of (path_string -> rt_edit *) that persists across
 * command invocations within a session.  Allows multi-step CLI edits without
 * a disk write per step.
 *
 * The buffer is stored inside Ged_Internal (ged_private.h) and is destroyed
 * (entries abandoned) when the Ged_Internal destructor runs.
 */

#include "common.h"

#include <string>
#include <vector>

#include "raytrace.h"
#include "rt/db_fullpath.h"
#include "rt/edit.h"

#include "./ged_private.h"
#include "./dbi.h"


/* ------------------------------------------------------------------ *
 * Ged_Internal destructor — abandons all pending edits on close.
 * ------------------------------------------------------------------ */

Ged_Internal::~Ged_Internal()
{
    for (auto &kv : edit_buf) {
	rt_edit_destroy(kv.second.s);
	db_free_full_path(&kv.second.dfp);
    }
    edit_buf.clear();
}


/* ------------------------------------------------------------------ *
 * Internal helper: compute the canonical path key from a db_full_path.
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
    auto it = gi->edit_buf.find(key);
    if (it == gi->edit_buf.end())
	return NULL;
    return it->second.s;
}


void
ged_edit_buf_set(struct ged *gedp, const struct db_full_path *dfp, struct rt_edit *s)
{
    if (!gedp || !dfp || !s || !gedp->i || !gedp->i->i)
	return;

    Ged_Internal *gi = gedp->i->i;
    std::string key = _edit_buf_key(dfp);

    /* If an entry already exists, free it first */
    auto it = gi->edit_buf.find(key);
    if (it != gi->edit_buf.end()) {
	rt_edit_destroy(it->second.s);
	db_free_full_path(&it->second.dfp);
	gi->edit_buf.erase(it);
    }

    Ged_Internal::ged_edit_buf_entry entry;
    db_full_path_init(&entry.dfp);
    db_dup_full_path(&entry.dfp, dfp);
    entry.s = s;
    gi->edit_buf[key] = entry;
}


int
ged_edit_buf_promote(struct ged *gedp, const struct db_full_path *dfp)
{
    if (!gedp || !dfp || !gedp->i || !gedp->i->i)
	return BRLCAD_ERROR;

    Ged_Internal *gi = gedp->i->i;
    std::string key = _edit_buf_key(dfp);
    auto it = gi->edit_buf.find(key);
    if (it == gi->edit_buf.end())
	return BRLCAD_ERROR;

    struct rt_edit *s = it->second.s;
    struct directory *dp = DB_FULL_PATH_CUR_DIR(&it->second.dfp);

    int ret = BRLCAD_ERROR;
    if (dp && gedp->dbip) {
	/* rt_db_put_internal frees es_int internals and re-initializes it
	 * via RT_DB_INTERNAL_INIT, so the subsequent rt_edit_destroy is safe. */
	int put_ret = rt_db_put_internal(dp, gedp->dbip, &s->es_int);
	ret = (put_ret < 0) ? BRLCAD_ERROR : BRLCAD_OK;
    }

    rt_edit_destroy(s);
    db_free_full_path(&it->second.dfp);
    gi->edit_buf.erase(it);

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
    auto it = gi->edit_buf.find(key);
    if (it == gi->edit_buf.end())
	return;

    rt_edit_destroy(it->second.s);
    db_free_full_path(&it->second.dfp);
    gi->edit_buf.erase(it);
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
    keys.reserve(gi->edit_buf.size());
    for (auto &kv : gi->edit_buf)
	keys.push_back(kv.first);

    bool any_written = false;
    for (const std::string &key : keys) {
	auto it = gi->edit_buf.find(key);
	if (it == gi->edit_buf.end())
	    continue;

	struct rt_edit *s = it->second.s;
	struct directory *dp = DB_FULL_PATH_CUR_DIR(&it->second.dfp);

	if (dp && gedp->dbip) {
	    rt_db_put_internal(dp, gedp->dbip, &s->es_int);
	    any_written = true;
	}

	rt_edit_destroy(s);
	db_free_full_path(&it->second.dfp);
	gi->edit_buf.erase(it);
    }

    if (any_written && gedp->dbi_state) {
	DbiState *dbis = (DbiState *)gedp->dbi_state;
	dbis->update();
    }
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
