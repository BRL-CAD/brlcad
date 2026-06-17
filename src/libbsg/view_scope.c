/*               V I E W _ S C O P E . C
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
/** @file libbsg/view_scope.c
 *
 * BSG_NODE_VIEW_SCOPE lifecycle — create / visible / destroy.
 *
 * A view-scope node is a non-drawable container whose children are skipped
 * during render traversal for views that do not own this scope.  A NULL owner
 * is shared and visible to all views.
 */

#include "common.h"

#include "bu/ptbl.h"
#include "bsg/defines.h"
#include "bsg/node.h"
#include "bsg/object.h"
#include "bsg/util.h"
#include "bsg/scene_object.h"
#include "bsg/view_scope.h"
#include "draw_source_private.h"
#include "node_private.h"
#include "object_private.h"
#include "view_scope_private.h"


bsg_node *
bsg_view_scope_create(struct bsg_view *v)
{
    if (!v)
	return NULL;

    bsg_node *s = bsg_node_create(v, BSG_NODE_VIEW_SCOPE);
    if (!s)
	return NULL;

    bsg_node_set_object_type(s, bsg_view_scope_type());
    bsg_node_set_view(s, v);

    return (bsg_node *)s;
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
