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

struct ged_history_revision {
    const char *id;
    const char *parent; /**< NULL at a root or retention boundary. */
    const char *label;
    int64_t authored;   /**< Seconds since the Unix epoch. */
    int selected;
    int retention_boundary;
};

enum ged_history_change_type {
    GED_HISTORY_CREATE = 0,
    GED_HISTORY_DELETE,
    GED_HISTORY_UPDATE,
    GED_HISTORY_RENAME
};

struct ged_history_change {
    enum ged_history_change_type type;
    const char *name;
    const char *new_name; /**< Rename destination, or NULL. */
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
 * Rebuild the legacy drawable from the new database during a registered
 * history replacement callback. The retired database is kept alive
 * through this call. Run it on the thread that owns the display.
 */
GED_EXPORT extern void ged_history_rebuild_legacy_display(struct ged *gedp, struct db_i *retired);

typedef int (*ged_history_edit_func_t)(struct ged *, void *);

/**
 * Complete a modeling edit on the GED owner's thread. The callback uses
 * gedp->dbip as its writable database and returns BRLCAD_OK to commit.
 * With history enabled, it joins the current edit or publishes one new
 * revision. On a plain writable document it uses the live database.
 */
GED_EXPORT extern int ged_history_apply_edit(struct ged *gedp, const char *label,
    ged_history_edit_func_t apply, void *data);

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

typedef int (*ged_history_revision_func_t)(const struct ged_history_revision *, void *);
typedef int (*ged_history_checkpoint_func_t)(const char *, const char *, void *);
typedef int (*ged_history_change_func_t)(const struct ged_history_change *, void *);

/**
 * Visit newest revisions first, optionally filtering on the exact changed
 * object name. When unfiltered, the optional checkpoint visitor receives
 * the checkpoint name followed by the revision ID.
 */
GED_EXPORT extern int ged_history_visit(struct ged *gedp, const char *object,
    ged_history_revision_func_t revision, ged_history_checkpoint_func_t checkpoint,
    void *data, struct bu_vls *message);

/**
 * Resolve a revision ID or checkpoint name and visit its summary and
 * changes. Either visitor may be NULL. Rename hints are followed by the
 * underlying creates, deletes and updates.
 *
 * Both visit APIs run on the GED owner's thread. Callback values are valid
 * for the duration of the callback. Visitors may inspect their values and
 * must leave GED state alone. Return BRLCAD_OK to continue or BRLCAD_ERROR
 * to stop. A failed lookup can populate @p message.
 */
GED_EXPORT extern int ged_history_visit_revision(struct ged *gedp, const char *id,
    ged_history_revision_func_t revision, ged_history_change_func_t change,
    void *data, struct bu_vls *message);

/**
 * Open an independent, read-only librt view of a retained revision.
 * Decimal session IDs, persistent IDs and checkpoint names use the same
 * forms as db history show. The caller releases the view with db_close.
 * The working revision and preferred redo path remain as they were.
 */
GED_EXPORT extern struct db_i *ged_history_open_revision(struct ged *gedp,
    const char *id, struct bu_vls *message);

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
