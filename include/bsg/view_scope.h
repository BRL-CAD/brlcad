/*                 V I E W _ S C O P E . H
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
 * View-scope scene-ref API.
 *
 * A view-scope node is a container whose children are skipped during BSG
 * render traversal for any view that does not match the node's owner metadata.
 * When the owner is NULL the scope is "shared" and its children are visible
 * to all views.
 *
 * Design:
 *   - Per-view scope:  bsg_scene_view_scope_create(v, name) with v != NULL.
 *                      Only the view v will see the children.
 *   - Shared scope:    NULL owner metadata is supported by the traversal
 *                      predicate; the NULL-owner allocation path is reserved
 *                      for a future phase.
 *
 * Producers use view-scope nodes for per-view and shared view-local scene
 * branches.
 */
/** @{ */
/* @file bsg/view_scope.h */

#ifndef BSG_VIEW_SCOPE_H
#define BSG_VIEW_SCOPE_H

#include "common.h"
#include "bsg/defines.h"

__BEGIN_DECLS

/* Public callers create view scopes through bsg_scene_view_scope_create() in
 * bsg/scene_builder.h and operate on opaque scene refs. */

__END_DECLS

#endif /* BSG_VIEW_SCOPE_H */

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
