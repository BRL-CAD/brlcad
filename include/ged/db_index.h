/*                       D B _ I N D E X . H
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
 * @brief Explicit GED database hierarchy/index query API.
 *
 * The `.g` database remains authoritative.  The GED database index is a
 * derived, rebuildable service owned by a `struct ged` context and used by
 * drawing, selection, editing, and application adapters for explicit
 * object/instance/path questions.
 *
 * This interface deliberately exposes records and queries, not implementation
 * containers.  Callers must not rely on private maps, Qt item storage, or any
 * particular backend layout.
 *
 * Complexity expectations:
 *   - object lookup by id is expected O(1);
 *   - direct child count/row lookup is expected O(1) after the parent object
 *     has been indexed;
 *   - direct object-use count/row lookup is expected O(1) after the child
 *     object has been indexed;
 *   - affected-path queries are expected O(number of returned affected paths
 *     plus their depth), using reverse object-use records;
 *   - top-object listing is O(number of top objects), with cyclic extras
 *     requiring bounded traversal of known comb-child records;
 *   - path printing and hashing are O(path depth);
 *   - APIs here must not require expansion of every possible full path.
 */
/** @{ */
/** @file ged/db_index.h */

#ifndef GED_DB_INDEX_H
#define GED_DB_INDEX_H

#include "common.h"

#include <stddef.h>

#include "bu/vls.h"
#include "ged/defines.h"

struct directory;

typedef unsigned long long ged_db_index_id;

#define GED_DB_INDEX_REFRESH_DB_CHANGE   0x01
#define GED_DB_INDEX_REFRESH_VIEW_CHANGE 0x02

#define GED_DB_INDEX_OBJECT_CHANGED 0
#define GED_DB_INDEX_OBJECT_ADDED   1
#define GED_DB_INDEX_OBJECT_REMOVED 2

struct ged_db_index_record {
    ged_db_index_id id;        /**< object or unique child-instance id */
    ged_db_index_id object_id; /**< canonical object id for duplicate instances */
    const char *name;          /**< borrowed display/name string */
    struct directory *dp;      /**< borrowed directory pointer when valid */
    int valid;                 /**< non-zero when id is known to the index */
    int is_instance;           /**< non-zero when id names a duplicate child instance */
    int is_comb;               /**< non-zero when the canonical object is a comb */
    size_t child_count;        /**< direct child instances, 0 for leaves/invalid refs */
};

struct ged_db_index_child {
    struct ged_db_index_record record;
    int bool_op;               /**< DB_OP_* value from raytrace.h/db_op_t */
    size_t row;                /**< row in index-presented child order */
};

struct ged_db_index_use {
    struct ged_db_index_record parent; /**< direct comb parent using child */
    struct ged_db_index_record child;  /**< child instance record at parent row */
    int bool_op;                       /**< DB_OP_* value from raytrace.h/db_op_t */
    size_t child_row;                  /**< row of child in parent */
};

__BEGIN_DECLS

/**
 * Return non-zero when @p gedp has an initialized database-index service.
 *
 * Callers should use this only as a capability check, not to infer backend
 * layout.
 */
GED_EXPORT extern int
ged_db_index_available(struct ged *gedp);

/**
 * Reconcile/rebuild the derived index from the current database state.
 *
 * Return value is backend-specific non-negative work/revision information, or
 * 0 when no backend/index is available.
 */
GED_EXPORT extern unsigned long long
ged_db_index_refresh(struct ged *gedp);

/**
 * Reconcile/rebuild the derived index and return backend change flags.
 *
 * This is for migration callers that need refresh classification such as
 * GED_DB_INDEX_REFRESH_VIEW_CHANGE.  A zero return means the refresh found no
 * observable change flags or no backend/index is available.
 */
GED_EXPORT extern unsigned long long
ged_db_index_refresh_flags(struct ged *gedp);

/**
 * Queue a database object change for the next index refresh.
 *
 * This is the public migration bridge for librt dbi_changed_t callbacks.
 * It invalidates derived per-object caches and records the changed object
 * using one of GED_DB_INDEX_OBJECT_CHANGED, GED_DB_INDEX_OBJECT_ADDED, or
 * GED_DB_INDEX_OBJECT_REMOVED.  The actual database-index reconciliation and
 * nref update happen when ged_db_index_refresh() or
 * ged_db_index_refresh_flags() is called.
 *
 * Returns 1 when the change was queued, 0 when no index backend/object was
 * available or @p change_kind was not recognized.
 */
GED_EXPORT extern int
ged_db_index_note_object_change(struct ged *gedp,
				struct directory *dp,
				int change_kind);

/**
 * Queue a database object change by name for the next index refresh.
 *
 * This is used by semantic command events when a live directory pointer is not
 * available, especially after removal or rename.  For changed/added objects the
 * current directory entry is looked up and queued through
 * ged_db_index_note_object_change(); removed names are recorded directly.
 */
GED_EXPORT extern int
ged_db_index_note_object_name_change(struct ged *gedp,
				     const char *name,
				     int change_kind);

GED_EXPORT extern int
ged_db_index_record_get(struct ged *gedp,
			ged_db_index_id id,
			struct ged_db_index_record *record);

GED_EXPORT extern size_t
ged_db_index_tops(struct ged *gedp,
		  int include_cyclic,
		  ged_db_index_id *ids,
		  size_t capacity);

GED_EXPORT extern size_t
ged_db_index_child_count(struct ged *gedp,
			 ged_db_index_id parent_id);

GED_EXPORT extern int
ged_db_index_child_at(struct ged *gedp,
		      ged_db_index_id parent_id,
		      size_t row,
		      struct ged_db_index_child *child);

GED_EXPORT extern size_t
ged_db_index_object_use_count(struct ged *gedp,
			      ged_db_index_id object_id);

GED_EXPORT extern int
ged_db_index_object_use_at(struct ged *gedp,
			   ged_db_index_id object_id,
			   size_t row,
			   struct ged_db_index_use *use);

/**
 * Count indexed hierarchy paths affected by a change to @p object_id.
 *
 * The count is derived from reverse object-use records and includes one row
 * per distinct indexed path ending in @p object_id or one of its duplicate
 * child instance ids.  Passing a duplicate instance id canonicalizes to the
 * underlying object.  @p max_depth bounds returned path depth when non-zero.
 */
GED_EXPORT extern size_t
ged_db_index_affected_path_count(struct ged *gedp,
				 ged_db_index_id object_id,
				 size_t max_depth);

/**
 * Copy one indexed affected path for @p object_id.
 *
 * Returns the full path depth for @p row, or 0 when the row is unavailable.
 * When @p ids is non-NULL, up to @p capacity ids are copied.  Callers may pass
 * NULL/0 first to query the required depth, then call again with storage.
 */
GED_EXPORT extern size_t
ged_db_index_affected_path_at(struct ged *gedp,
			      ged_db_index_id object_id,
			      size_t row,
			      ged_db_index_id *ids,
			      size_t capacity,
			      size_t max_depth);

GED_EXPORT extern int
ged_db_index_valid_id(struct ged *gedp, ged_db_index_id id);

/**
 * Resolve a database path string into database-index path ids.
 *
 * On success, returns the full number of ids in the resolved path.  When
 * @p ids is non-NULL, up to @p capacity ids are copied into it; callers may
 * pass NULL/0 first to query the required length.  Returns 0 when the path is
 * empty, invalid, or no backend is available to resolve indexed paths.
 */
GED_EXPORT extern size_t
ged_db_index_path_resolve(struct ged *gedp,
			  const char *path,
			  ged_db_index_id *ids,
			  size_t capacity);

GED_EXPORT extern int
ged_db_index_path_print(struct ged *gedp,
			struct bu_vls *out,
			const ged_db_index_id *path,
			size_t path_len,
			size_t max_len,
			int verbose);

GED_EXPORT extern ged_db_index_id
ged_db_index_path_hash(struct ged *gedp,
		       const ged_db_index_id *path,
		       size_t path_len,
		       size_t max_len);

__END_DECLS

#endif /* GED_DB_INDEX_H */

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
