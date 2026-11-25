/*                        H I S T O R Y . H
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
/** @file ged/history.h
 * Typed snapshots of database history state.
 */

#ifndef GED_HISTORY_H
#define GED_HISTORY_H

#include <stddef.h>
#include <stdint.h>

#include "ged/defines.h"

struct bu_vls;
struct db_i;
struct ged;

enum ged_history_mode {
    GED_HISTORY_DISABLED = 0,
    GED_HISTORY_SESSION,
    GED_HISTORY_EMBEDDED,
    GED_HISTORY_WORKSPACE
};

enum ged_history_state {
    GED_HISTORY_INACTIVE = 0,
    GED_HISTORY_CURRENT,
    GED_HISTORY_STALE,
    GED_HISTORY_EXTERNAL_EDITS,
    GED_HISTORY_MALFORMED,
    GED_HISTORY_UNSUPPORTED,
    GED_HISTORY_UNAVAILABLE
};

struct ged_history_choice {
    const char *revision;
    const char *label;
    int preferred;
};

struct ged_history_status {
    enum ged_history_mode mode;
    enum ged_history_state state;
    int writable;        /**< The backing store accepts writes. */
    int edit_active;     /**< An edit is still being staged. */
    int staged_conflict;
    int retention_boundary;
    const char *selected_revision;
    const char *oldest_revision;   /**< Session history. */
    const char *workspace_head;    /**< Most recently observed workspace head. */
    const char *exported_revision; /**< Most recently recorded export. */
    uint64_t generation;           /**< Workspace generation of the open view. */
    uint64_t workspace_generation; /**< Most recently observed workspace generation. */
    size_t session_bytes;
    size_t session_limit;
    struct ged_history_choice undo;
    size_t redo_count;
    const struct ged_history_choice *redo;
};

__BEGIN_DECLS

typedef int (*ged_history_prepare_func_t)(struct ged *, void *);
typedef void (*ged_history_replaced_func_t)(struct ged *, struct db_i *, void *);

/**
 * Register the host's database replacement handlers. An optional prepare
 * handler returns BRLCAD_OK when replacement can proceed, or writes a
 * diagnostic to ged_result_str and returns BRLCAD_ERROR. The replaced
 * handler runs after publication and view reconciliation, while the
 * retired database is still valid. Release or rebind host aliases there.
 * Both handlers run on the GED owner's thread.
 */
GED_EXPORT extern void ged_history_set_replacement_handlers(struct ged *gedp,
    ged_history_prepare_func_t prepare, ged_history_replaced_func_t replaced,
    void *data);

/**
 * Read history state on the GED owner's thread. The returned status owns
 * its strings and choices and remains valid after the document changes.
 * Null strings denote unavailable revisions. Decimal session IDs and
 * hexadecimal persistent IDs use the same forms accepted by GED.
 * Return NULL on error and optionally populate @p message.
 */
GED_EXPORT extern const struct ged_history_status *
ged_history_status_create(struct ged *gedp, struct bu_vls *message);

/** Release an owned status. Accepts NULL. */
GED_EXPORT extern void ged_history_status_destroy(const struct ged_history_status *status);

__END_DECLS

#endif /* GED_HISTORY_H */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
