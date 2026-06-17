/*                  S E L E C T I O N _ S T A T E . H
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
/** @addtogroup ged_view
 *
 * @brief GED semantic selection-state API.
 *
 * Selection state is owned by the GED context, not by Qt model items or view
 * widgets.  Applications identify selected targets using database-index path
 * ids and query semantic state such as selected paths, active ancestor paths,
 * and parent-object highlighting.
 *
 * This service owns selection state directly.  Callers must not depend on
 * private migration backends.
 */
/** @{ */
/** @file ged/selection_state.h */

#ifndef GED_SELECTION_STATE_H
#define GED_SELECTION_STATE_H

#include "common.h"

#include <stddef.h>

#include "ged/db_index.h"

struct bu_vls;

__BEGIN_DECLS

/**
 * Return non-zero when the GED context has an initialized selection service.
 */
GED_EXPORT extern int
ged_selection_state_available(struct ged *gedp);

/**
 * Return the number of explicitly selected paths in @p set_name.
 *
 * A NULL or empty set name means the default selection set.
 */
GED_EXPORT extern size_t
ged_selection_count(struct ged *gedp, const char *set_name);

/**
 * Append one selection-set name per line to @p out.
 *
 * The default set is implicit and is not included in the named-set listing.
 * Returns the number of names appended.
 */
GED_EXPORT extern int
ged_selection_list_set_names(struct ged *gedp, struct bu_vls *out);

/**
 * Append one selected database path per line for all sets matching
 * @p set_pattern.  NULL or empty pattern means the default set.
 *
 * Returns the number of matching sets.
 */
GED_EXPORT extern int
ged_selection_list_paths(struct ged *gedp,
			 const char *set_pattern,
			 struct bu_vls *out);

GED_EXPORT extern int
ged_selection_set_match_count(struct ged *gedp, const char *set_pattern);

GED_EXPORT extern unsigned long long
ged_selection_state_hash(struct ged *gedp, const char *set_name);

GED_EXPORT extern int
ged_selection_clear(struct ged *gedp, const char *set_name);

GED_EXPORT extern int
ged_selection_clear_matching(struct ged *gedp, const char *set_pattern);

GED_EXPORT extern int
ged_selection_select_path(struct ged *gedp,
			  const char *set_name,
			  const char *path,
			  int recompute_hierarchy);

GED_EXPORT extern int
ged_selection_select_path_matching(struct ged *gedp,
				   const char *set_pattern,
				   const char *path,
				   int recompute_hierarchy);

GED_EXPORT extern int
ged_selection_deselect_path(struct ged *gedp,
			    const char *set_name,
			    const char *path,
			    int recompute_hierarchy);

GED_EXPORT extern int
ged_selection_deselect_path_matching(struct ged *gedp,
				     const char *set_pattern,
				     const char *path,
				     int recompute_hierarchy);

GED_EXPORT extern int
ged_selection_select_path_ids(struct ged *gedp,
			      const char *set_name,
			      const ged_db_index_id *path,
			      size_t path_len,
			      int recompute_hierarchy);

GED_EXPORT extern int
ged_selection_deselect_path_ids(struct ged *gedp,
				const char *set_name,
				const ged_db_index_id *path,
				size_t path_len,
				int recompute_hierarchy);

GED_EXPORT extern int
ged_selection_recompute(struct ged *gedp, const char *set_name);

GED_EXPORT extern int
ged_selection_recompute_matching(struct ged *gedp, const char *set_pattern);

GED_EXPORT extern int
ged_selection_expand_matching(struct ged *gedp, const char *set_pattern);

GED_EXPORT extern int
ged_selection_collapse_matching(struct ged *gedp, const char *set_pattern);

/**
 * Synchronize semantic selection state into retained draw/view selection
 * records.  Returns non-zero when draw/view selection state changed.
 */
GED_EXPORT extern int
ged_selection_draw_sync(struct ged *gedp, const char *set_name);

GED_EXPORT extern int
ged_selection_draw_sync_matching(struct ged *gedp, const char *set_pattern);

GED_EXPORT extern int
ged_selection_is_path_selected(struct ged *gedp,
			       const char *set_name,
			       ged_db_index_id path_hash);

GED_EXPORT extern int
ged_selection_is_path_active(struct ged *gedp,
			     const char *set_name,
			     ged_db_index_id path_hash);

GED_EXPORT extern int
ged_selection_is_path_active_parent(struct ged *gedp,
				    const char *set_name,
				    ged_db_index_id path_hash);

GED_EXPORT extern int
ged_selection_is_object_parent(struct ged *gedp,
			       const char *set_name,
			       ged_db_index_id object_id);

GED_EXPORT extern int
ged_selection_is_object_immediate_parent(struct ged *gedp,
					 const char *set_name,
					 ged_db_index_id object_id);

GED_EXPORT extern int
ged_selection_is_object_grand_parent(struct ged *gedp,
				     const char *set_name,
				     ged_db_index_id object_id);

__END_DECLS

#endif /* GED_SELECTION_STATE_H */

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
