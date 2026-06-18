/*                    E V E N T _ T X N . H
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
 * You should have received a copy of the GNU Lesser General Public License
 * along with this file; see the file named COPYING for more information.
 */
/** @addtogroup ged_database
 *
 * @brief GED semantic database mutation/reconciliation events.
 *
 * Commands publish post-mutation semantic database events here instead of
 * performing draw, model, selection, or edit repair directly.  The event
 * layer batches and coalesces local mutations, reconciles core GED services,
 * then notifies application observers with stable post-reconcile state.
 *
 * Reconciliation delegates draw repair to the canonical ged_draw_transaction
 * API.  Callers should treat these event records as semantic database events,
 * not as draw transactions themselves.
 */
/** @{ */
/** @file ged/event_txn.h */

#ifndef GED_EVENT_TXN_H
#define GED_EVENT_TXN_H

#include "common.h"

#include <stddef.h>
#include <stdint.h>

#include "ged/defines.h"

typedef enum ged_event_kind {
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
} ged_event_kind;

typedef enum ged_event_observer_phase {
    GED_EVENT_OBSERVER_INTERNAL = 0,
    GED_EVENT_OBSERVER_POST_RECONCILE = 1
} ged_event_observer_phase;

typedef uintptr_t ged_event_observer_token;

struct ged_event {
    ged_event_kind kind;
    const char *name;        /**< primary object/path name, borrowed */
    const char *new_name;    /**< replacement object/path name for rename, borrowed */
    const char *parent_name; /**< parent comb/object for instance events, borrowed */
    const char *child_name;  /**< child object/reference for instance events, borrowed */
    const char *path;        /**< full affected instance path when known, borrowed */
    int redraw;              /**< non-zero when modified sources should redraw now */
};

struct ged_event_txn_result {
    int status;                       /**< >=0 success/count, <0 error */
    size_t event_count;               /**< input event count */
    size_t coalesced_event_count;     /**< events after exact duplicate coalescing */
    int draw_status;                  /**< aggregate draw reconciliation status */
    int db_index_status;              /**< aggregate database-index status */
    int selection_status;             /**< aggregate selection reconciliation status */
    uint64_t draw_scene_revision_before;
    uint64_t draw_scene_revision_after;
    struct bu_vls affected_names;     /**< affected user-facing names/paths */
    struct bu_vls errors;             /**< diagnostic details */
};

typedef void (*ged_event_observer_func_t)(
	struct ged *gedp,
	const struct ged_event *events,
	size_t event_count,
	const struct ged_event_txn_result *result,
	void *client_data);

__BEGIN_DECLS

GED_EXPORT extern int
ged_event_txn_available(struct ged *gedp);

GED_EXPORT extern int
ged_event_txn_disable(struct ged *gedp);

GED_EXPORT extern int
ged_event_txn_enable(struct ged *gedp);

/**
 * Begin/end a bulk mutation/import transaction.  While a bulk transaction is
 * active, per-command batches and published events are treated as evidence of
 * database mutation, but interactive reconciliation is deferred.  The outermost
 * end publishes one batch-rebuild event only when a live GED consumer is
 * present.
 */
GED_EXPORT extern int
ged_event_bulk_begin(struct ged *gedp);

GED_EXPORT extern int
ged_event_bulk_end(struct ged *gedp, struct ged_event_txn_result *result);

GED_EXPORT extern int
ged_event_bulk_active(struct ged *gedp);

GED_EXPORT extern int
ged_event_txn_has_live_consumers(struct ged *gedp);

/**
 * Install/remove the GED-owned bridge from low-level librt database changed
 * callbacks into GedEventTxn.  The bridge is a fallback source for database
 * mutations that do not publish richer command semantic events directly.
 */
GED_EXPORT extern int
ged_event_librt_callbacks_enable(struct ged *gedp);

GED_EXPORT extern int
ged_event_librt_callbacks_disable(struct ged *gedp);

GED_EXPORT extern void
ged_event_txn_result_init(struct ged_event_txn_result *result);

GED_EXPORT extern void
ged_event_txn_result_free(struct ged_event_txn_result *result);

/**
 * Begin/end a semantic event batch.  Events published while a batch is open
 * are copied by the GED context and reconciled when the outermost batch ends.
 */
GED_EXPORT extern int
ged_event_batch_begin(struct ged *gedp);

GED_EXPORT extern int
ged_event_batch_end(struct ged *gedp, struct ged_event_txn_result *result);

GED_EXPORT extern int
ged_event_publish(struct ged *gedp,
		  const struct ged_event *events,
		  size_t event_count,
		  struct ged_event_txn_result *result);

GED_EXPORT extern int
ged_event_notify_object_added(struct ged *gedp,
			      const char *name,
			      struct ged_event_txn_result *result);

GED_EXPORT extern int
ged_event_notify_object_removed(struct ged *gedp,
				const char *name,
				struct ged_event_txn_result *result);

GED_EXPORT extern int
ged_event_notify_object_renamed(struct ged *gedp,
				const char *old_name,
				const char *new_name,
				struct ged_event_txn_result *result);

GED_EXPORT extern int
ged_event_notify_object_modified(struct ged *gedp,
				 const char *name,
				 int redraw,
				 struct ged_event_txn_result *result);

GED_EXPORT extern int
ged_event_notify_object_visibility_changed(struct ged *gedp,
					   const char *name,
					   struct ged_event_txn_result *result);

GED_EXPORT extern int
ged_event_notify_comb_tree_changed(struct ged *gedp,
				   const char *name,
				   int redraw,
				   struct ged_event_txn_result *result);

GED_EXPORT extern int
ged_event_notify_comb_instance_removed(struct ged *gedp,
				       const char *parent_name,
				       const char *child_name,
				       const char *path,
				       struct ged_event_txn_result *result);

GED_EXPORT extern int
ged_event_notify_object_references_removed(struct ged *gedp,
					   const char *name,
					   struct ged_event_txn_result *result);

GED_EXPORT extern int
ged_event_notify_object_reference_removed_from_parent(struct ged *gedp,
						      const char *name,
						      const char *parent_name,
						      const char *path,
						      struct ged_event_txn_result *result);

GED_EXPORT extern int
ged_event_notify_attribute_changed(struct ged *gedp,
				   const char *name,
				   int redraw,
				   struct ged_event_txn_result *result);

GED_EXPORT extern int
ged_event_notify_material_changed(struct ged *gedp,
				  struct ged_event_txn_result *result);

GED_EXPORT extern int
ged_event_notify_object_material_changed(struct ged *gedp,
					 const char *name,
					 struct ged_event_txn_result *result);

GED_EXPORT extern int
ged_event_notify_batch_rebuild(struct ged *gedp,
			       struct ged_event_txn_result *result);

GED_EXPORT extern int
ged_event_notify_database_metadata_changed(struct ged *gedp,
					   struct ged_event_txn_result *result);

GED_EXPORT extern ged_event_observer_token
ged_event_observer_add(struct ged *gedp,
		       ged_event_observer_phase phase,
		       ged_event_observer_func_t func,
		       void *client_data);

GED_EXPORT extern int
ged_event_observer_remove(struct ged *gedp, ged_event_observer_token token);

__END_DECLS

#endif /* GED_EVENT_TXN_H */

/** @} */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
