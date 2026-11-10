/*                         E V E N T . H
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 */
/** @file ged/event.h
 *
 * Renderer-neutral notification of semantic database changes.
 *
 * Publishers describe completed changes.  The GED context copies event
 * payloads, coalesces exact duplicates, and delivers each batch first to
 * internal reconcilers and then to application observers.  Publication and
 * observer registration are serialized per GED context.
 */

#ifndef GED_EVENT_H
#define GED_EVENT_H

#include <stddef.h>
#include <stdint.h>

#include "ged/defines.h"

struct ged_event_result;

/** Semantic database changes visible to GED clients. */
enum ged_event_kind {
    GED_EVENT_NONE = 0,
    GED_EVENT_OBJECT_ADDED,
    GED_EVENT_OBJECT_REMOVED,
    GED_EVENT_OBJECT_RENAMED,
    GED_EVENT_OBJECT_MODIFIED,
    GED_EVENT_OBJECT_VISIBILITY_CHANGED,
    GED_EVENT_COMB_TREE_CHANGED,
    GED_EVENT_COMB_INSTANCE_REMOVED,
    GED_EVENT_OBJECT_REFERENCES_REMOVED,
    GED_EVENT_ATTRIBUTE_CHANGED,
    GED_EVENT_MATERIAL_CHANGED,
    GED_EVENT_BATCH_REBUILD,
    GED_EVENT_DATABASE_METADATA_CHANGED
};

/** Observer point relative to core GED reconciliation. */
enum ged_event_observer_phase {
    GED_EVENT_OBSERVER_INTERNAL = 0,
    GED_EVENT_OBSERVER_POST_RECONCILE
};

/** Status returned by event operations. */
enum ged_event_status {
    GED_EVENT_OK = 0,
    GED_EVENT_INVALID,
    GED_EVENT_UNAVAILABLE,
    GED_EVENT_ERROR
};

typedef uintptr_t ged_event_observer_token;

/**
 * Borrowed input describing one completed database change.
 *
 * Strings need remain valid only for the publishing call.  NULL indicates
 * that an identity is unknown or does not apply to the event kind.
 */
struct ged_event {
    enum ged_event_kind kind;
    const char *name;
    const char *new_name;
    const char *parent_name;
    const char *child_name;
    const char *path;
    int redraw;
};

/** Callback receiving one immutable, reconciled event batch. */
typedef void (*ged_event_observer_func_t)(
    struct ged *gedp,
    const struct ged_event *events,
    size_t event_count,
    const struct ged_event_result *result,
    void *client_data);

/** Unit of event work passed to an application owner dispatcher. */
typedef void (*ged_event_work_func_t)(void *work_data);

/**
 * Synchronously run event work in the GED owner's execution context.
 *
 * A dispatcher must not return before @p work has completed.  Return zero on
 * success.  Applications that do not install a dispatcher receive events in
 * the publishing context.
 */
typedef int (*ged_event_dispatch_func_t)(
    struct ged *gedp,
    ged_event_work_func_t work,
    void *work_data,
    void *client_data);

__BEGIN_DECLS

/** Initialize a caller-owned event descriptor to GED_EVENT_NONE. */
GED_EXPORT extern void ged_event_init(struct ged_event *event);

/** Allocate an empty event result. */
GED_EXPORT extern struct ged_event_result *ged_event_result_create(void);

/** Clear a reusable event result while retaining its allocation. */
GED_EXPORT extern void ged_event_result_clear(struct ged_event_result *result);

/** Destroy an event result allocated by ged_event_result_create(). */
GED_EXPORT extern void ged_event_result_destroy(struct ged_event_result *result);

/** Return the operation status stored in an event result. */
GED_EXPORT extern enum ged_event_status
ged_event_result_status(const struct ged_event_result *result);

/** Return the number of published events represented by a result. */
GED_EXPORT extern size_t
ged_event_result_event_count(const struct ged_event_result *result);

/** Return the number of events remaining after exact coalescing. */
GED_EXPORT extern size_t
ged_event_result_coalesced_count(const struct ged_event_result *result);

/** Return the number of unique object names represented by a result. */
GED_EXPORT extern size_t
ged_event_result_name_count(const struct ged_event_result *result);

/** Return a result-owned object name, or NULL for an invalid index. */
GED_EXPORT extern const char *
ged_event_result_name_at(const struct ged_event_result *result, size_t index);

/** Return the number of unique paths represented by a result. */
GED_EXPORT extern size_t
ged_event_result_path_count(const struct ged_event_result *result);

/** Return a result-owned path, or NULL for an invalid index. */
GED_EXPORT extern const char *
ged_event_result_path_at(const struct ged_event_result *result, size_t index);

/** Return a result-owned diagnostic, or an empty string when none exists. */
GED_EXPORT extern const char *
ged_event_result_diagnostic(const struct ged_event_result *result);

/** Install or clear the synchronous owner-context dispatcher. */
GED_EXPORT extern enum ged_event_status
ged_event_dispatch_set(struct ged *gedp, ged_event_dispatch_func_t dispatch,
		       void *client_data);

/** Begin a nestable event batch. */
GED_EXPORT extern enum ged_event_status ged_event_batch_begin(struct ged *gedp);

/** End a batch and dispatch at its outermost boundary. */
GED_EXPORT extern enum ged_event_status
ged_event_batch_end(struct ged *gedp, struct ged_event_result *result);

/**
 * Publish and copy a sequence of semantic database events.
 *
 * Reentrant or concurrent publications join the active dispatch queue.  In
 * that case @p result is not populated because another publishing call owns
 * the eventual dispatch result.
 */
GED_EXPORT extern enum ged_event_status
ged_event_publish(struct ged *gedp, const struct ged_event *events,
		  size_t event_count, struct ged_event_result *result);

/** Register an observer. */
GED_EXPORT extern ged_event_observer_token
ged_event_observer_add(struct ged *gedp,
		       enum ged_event_observer_phase phase,
		       ged_event_observer_func_t func,
		       void *client_data);

/** Remove an observer; removal during dispatch is supported. */
GED_EXPORT extern int
ged_event_observer_remove(struct ged *gedp, ged_event_observer_token token);

__END_DECLS

#endif /* GED_EVENT_H */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
