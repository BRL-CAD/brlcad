/*                   V I S I B I L I T Y . C
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
/** @file libbsg/visibility.c
 *
 * Graph-native visibility predicates.
 */

#include "common.h"

#include "bsg/defines.h"
#include "bsg/object.h"
#include "bsg/visibility.h"
#include "draw_source_private.h"
#include "node_private.h"
#include "object_private.h"
#include "visibility_private.h"


BSG_EXPORT int
bsg_visibility_local(const bsg_node *node)
{
    if (!node)
	return 0;
    return bsg_node_visible(node);
}


BSG_EXPORT int
bsg_visibility_scope_allows_view(const bsg_node *node,
				 const struct bsg_view *v)
{
    const bsg_node *n = (const bsg_node *)node;

    if (!n)
	return 0;

    if (!bsg_node_type_is_a(n, bsg_view_scope_type()))
	return 1;

    struct bsg_view *owner = bsg_node_get_view(n);
    if (owner == NULL)
	return 1;

    return (owner == (struct bsg_view *)v) ? 1 : 0;
}


BSG_EXPORT int
bsg_visibility_inherited_visible(const bsg_node *node,
				 const struct bsg_view *v,
				 int inherited_visible)
{
    if (!node)
	return 0;

    if (!inherited_visible)
	return 0;

    if (!bsg_visibility_local(node))
	return 0;

    return bsg_visibility_scope_allows_view(node, v);
}


BSG_EXPORT int
bsg_visibility_visible_in_view(const bsg_node *node,
			       const struct bsg_view *v)
{
    const bsg_node *n = (const bsg_node *)node;

    if (!n)
	return 0;

    int visible = 1;
    while (n) {
	visible = bsg_visibility_inherited_visible(n, v, visible);
	if (!visible)
	    return 0;
	n = n->parent;
    }

    return 1;
}


BSG_EXPORT int
bsg_visibility_hidden_in_view(const bsg_node *node,
			      const struct bsg_view *v)
{
    return bsg_visibility_visible_in_view(node, v) ? 0 : 1;
}

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
