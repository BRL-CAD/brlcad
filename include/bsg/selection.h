/*                   S E L E C T I O N . H
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
/** @addtogroup libbsg
 *
 * @brief
 * First-class selection model for BSG interaction records.
 *
 * A `bsg_selection` is an opaque set of cloned interaction records.  The
 * records carry feature refs, source paths, subelement ids, and payload
 * hit ids; graph-node pointers are not selection identity.
 */
/** @{ */
/* @file bsg/selection.h */

#ifndef BSG_SELECTION_H
#define BSG_SELECTION_H

#include "common.h"
#include "bu/ptbl.h"
#include "bsg/defines.h"

__BEGIN_DECLS

/** Opaque selection-set handle */
struct bsg_selection;
struct bsg_interaction_record;

/**
 * Allocate and initialise an empty selection set.
 * Returns NULL on allocation failure.
 */
BSG_EXPORT extern struct bsg_selection *
bsg_selection_create(void);

/**
 * Release a selection set.
 * Source objects referenced by the set are NOT destroyed; only the set itself
 * is freed.
 * No-op if @p sel is NULL.
 */
BSG_EXPORT extern void
bsg_selection_destroy(struct bsg_selection *sel);

BSG_EXPORT extern void
bsg_selection_add_record(struct bsg_selection *sel,
			 const struct bsg_interaction_record *record);

BSG_EXPORT extern void
bsg_selection_remove_record(struct bsg_selection *sel,
			    const struct bsg_interaction_record *record);

/**
 * Remove all records from @p sel.
 * No-op if @p sel is NULL.
 */
BSG_EXPORT extern void
bsg_selection_clear(struct bsg_selection *sel);

BSG_EXPORT extern int
bsg_selection_contains_record(const struct bsg_selection *sel,
			      const struct bsg_interaction_record *record);

/**
 * Return the number of interaction records currently in @p sel.
 * Returns 0 if @p sel is NULL.
 */
BSG_EXPORT extern size_t
bsg_selection_count(const struct bsg_selection *sel);

BSG_EXPORT extern const struct bsg_interaction_record *
bsg_selection_record(const struct bsg_selection *sel, size_t idx);

__END_DECLS

#endif /* BSG_SELECTION_H */

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
