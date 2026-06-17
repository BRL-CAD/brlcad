/*                      V I E W _ S E T . H
 * BRL-CAD
 *
 * Copyright (c) 1993-2026 United States Government as represented by
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
/** @addtogroup bsg_view_set
 *
 * In applications with multiple views, those views typically share common
 * scene objects and memory.  To manage this sharing, we define view sets.
 *
 * Canonical home; bv/view_sets.h is a backward-compatibility bridge.
 */
/** @{ */
/** @file bsg/view_set.h */

#ifndef BSG_VIEW_SET_H
#define BSG_VIEW_SET_H

#include "common.h"
#include "bsg/defines.h"

__BEGIN_DECLS

/**
 * Initialize an empty view set
 */
BSG_EXPORT void
bsg_set_init(struct bsg_view_set *s);

/**
 * Free view set
 */
BSG_EXPORT void
bsg_set_free(struct bsg_view_set *s);

/**
 * Add view v to set s, handling shared memory assignments.
 */
BSG_EXPORT void
bsg_set_add_view(struct bsg_view_set *s, struct bsg_view *v);

/**
 * Remove view v from set s.  If v == NULL, all views
 * are removed from the set.
 */
BSG_EXPORT void
bsg_set_rm_view(struct bsg_view_set *s, struct bsg_view *v);

/**
 * Return a bu_ptbl holding pointers to all views in set s
 */
BSG_EXPORT struct bu_ptbl *
bsg_set_views(struct bsg_view_set *s);

/**
 * Return a pointer to the view with name vname, if it is present in s.  If not
 * found, returns NULL
 */
BSG_EXPORT struct bsg_view *
bsg_set_find_view(struct bsg_view_set *s, const char *vname);


/* Opaque implementation recycling handle for draw contexts. */
BSG_EXPORT void *
bsg_set_fsos(struct bsg_view_set *s);


__END_DECLS

/** @} */

#endif /* BSG_VIEW_SET_H */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
