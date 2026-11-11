/* S E L E C T I O N . H
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License version 2.1.
 */
/** @file ged/selection.h */

#ifndef GED_SELECTION_H
#define GED_SELECTION_H

#include "ged/db_index.h"

struct bu_vls;

/** Stable hash collections derived from one semantic selection set. */
enum ged_selection_hash_kind {
    GED_SELECTION_HASH_SELECTED_PATH = 1,
    GED_SELECTION_HASH_ACTIVE_PATH,
    GED_SELECTION_HASH_ACTIVE_PARENT_PATH,
    GED_SELECTION_HASH_IMMEDIATE_PARENT_OBJECT,
    GED_SELECTION_HASH_GRAND_PARENT_OBJECT
};

__BEGIN_DECLS

/** Return non-zero when @p gedp owns an initialized selection service. */
GED_EXPORT extern int
ged_selection_state_available(struct ged *gedp);

/**
 * Begin a nestable semantic-selection mutation batch.
 *
 * Selection mutations made inside the outermost batch are accumulated.  Any
 * application presentation adapter is notified once when
 * ged_selection_batch_end() is called.
 */
GED_EXPORT extern int
ged_selection_batch_begin(struct ged *gedp);

/** End a selection batch and notify its optional presentation adapter. */
GED_EXPORT extern int
ged_selection_batch_end(struct ged *gedp);

/** Return the number of explicitly selected paths in @p set_name. */
GED_EXPORT extern size_t
ged_selection_count(struct ged *gedp, const char *set_name);

/** Append one non-default selection-set name per line to @p out. */
GED_EXPORT extern int
ged_selection_list_set_names(struct ged *gedp, struct bu_vls *out);

/**
 * Append selected paths from sets matching @p set_pattern to @p out.
 *
 * A NULL or empty pattern identifies the default set.  The return value is
 * the number of matching sets.
 */
GED_EXPORT extern int
ged_selection_list_paths(struct ged *gedp, const char *set_pattern,
	struct bu_vls *out);

/** Return the number of sets matching @p set_pattern. */
GED_EXPORT extern int
ged_selection_set_match_count(struct ged *gedp, const char *set_pattern);

/** Return a deterministic hash of the selected paths in @p set_name. */
GED_EXPORT extern unsigned long long
ged_selection_state_hash(struct ged *gedp, const char *set_name);

/**
 * Copy a renderer-neutral selection identity collection.
 *
 * The return value is the total number of identifiers in @p kind.  If
 * @p hashes is non-NULL, at most @p capacity identifiers are copied.  A
 * caller may therefore query the required capacity with a NULL buffer and
 * then copy the collection without parsing or resolving path strings.
 */
GED_EXPORT extern size_t
ged_selection_hashes(struct ged *gedp, const char *set_name,
	enum ged_selection_hash_kind kind, ged_db_index_id *hashes,
	size_t capacity);

/** Clear one exact semantic selection set. */
GED_EXPORT extern int
ged_selection_clear(struct ged *gedp, const char *set_name);

/** Clear every semantic selection set matching @p set_pattern. */
GED_EXPORT extern int
ged_selection_clear_matching(struct ged *gedp, const char *set_pattern);

/** Select one canonical database path in an exact selection set. */
GED_EXPORT extern int
ged_selection_select_path(struct ged *gedp, const char *set_name,
	const char *path, int recompute_hierarchy);

/** Atomically add multiple canonical database paths to one selection set. */
GED_EXPORT extern int
ged_selection_select_paths(struct ged *gedp, const char *set_name,
	const char *const *paths, size_t path_count, int recompute_hierarchy);

/** Select one path in the single set matching @p set_pattern. */
GED_EXPORT extern int
ged_selection_select_path_matching(struct ged *gedp,
	const char *set_pattern, const char *path, int recompute_hierarchy);

/** Deselect one canonical database path from an exact selection set. */
GED_EXPORT extern int
ged_selection_deselect_path(struct ged *gedp, const char *set_name,
	const char *path, int recompute_hierarchy);

/** Atomically remove multiple paths and their selected descendants. */
GED_EXPORT extern int
ged_selection_deselect_paths(struct ged *gedp, const char *set_name,
	const char *const *paths, size_t path_count, int recompute_hierarchy);

/** Deselect one path from the single set matching @p set_pattern. */
GED_EXPORT extern int
ged_selection_deselect_path_matching(struct ged *gedp,
	const char *set_pattern, const char *path, int recompute_hierarchy);

/** Select one path represented by stable database-index identifiers. */
GED_EXPORT extern int
ged_selection_select_path_ids(struct ged *gedp, const char *set_name,
	const ged_db_index_id *path, size_t path_len, int recompute_hierarchy);

/** Deselect one path represented by stable database-index identifiers. */
GED_EXPORT extern int
ged_selection_deselect_path_ids(struct ged *gedp, const char *set_name,
	const ged_db_index_id *path, size_t path_len, int recompute_hierarchy);

/** Invalidate and lazily recompute hierarchy indexes for one selection set. */
GED_EXPORT extern int
ged_selection_recompute(struct ged *gedp, const char *set_name);

/** Invalidate hierarchy indexes for sets matching @p set_pattern. */
GED_EXPORT extern int
ged_selection_recompute_matching(struct ged *gedp,
	const char *set_pattern);

/** Expand selected combination paths to their terminal descendants. */
GED_EXPORT extern int
ged_selection_expand_matching(struct ged *gedp, const char *set_pattern);

/** Collapse complete sibling selections to their common parent paths. */
GED_EXPORT extern int
ged_selection_collapse_matching(struct ged *gedp, const char *set_pattern);

/** Return non-zero when @p path_hash is explicitly selected. */
GED_EXPORT extern int
ged_selection_is_path_selected(struct ged *gedp, const char *set_name,
	ged_db_index_id path_hash);

/** Return non-zero when @p path_hash is selected or below a selected path. */
GED_EXPORT extern int
ged_selection_is_path_active(struct ged *gedp, const char *set_name,
	ged_db_index_id path_hash);

/** Return non-zero when @p path_hash is an ancestor of a selected path. */
GED_EXPORT extern int
ged_selection_is_path_active_parent(struct ged *gedp,
	const char *set_name, ged_db_index_id path_hash);

/** Return non-zero when @p object_id is any parent of a selected object. */
GED_EXPORT extern int
ged_selection_is_object_parent(struct ged *gedp, const char *set_name,
	ged_db_index_id object_id);

/** Return non-zero when @p object_id immediately parents a selection. */
GED_EXPORT extern int
ged_selection_is_object_immediate_parent(struct ged *gedp,
	const char *set_name, ged_db_index_id object_id);

/** Return non-zero when @p object_id is a non-immediate selection ancestor. */
GED_EXPORT extern int
ged_selection_is_object_grand_parent(struct ged *gedp,
	const char *set_name, ged_db_index_id object_id);

__END_DECLS

#endif /* GED_SELECTION_H */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
