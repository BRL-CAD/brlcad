/*                   N O D E _ G R O U P . C
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
/** @file libbsg/node_group.c
 *
 * BSG_NODE_GROUP lifecycle (create / add_child / remove_child /
 * destroy).  A group aggregates child nodes without contributing any drawable
 * geometry — analogous to OpenInventor's SoGroup.
 */

#include "common.h"

#include "bu/ptbl.h"
#include "bsg/defines.h"
#include "bsg/node.h"
#include "bsg/util.h"
#include "node_private.h"
#include "object_private.h"


bsg_node *
bsg_group_create(struct bsg_view *v)
{
    if (!v)
	return NULL;

    bsg_node *node = bsg_node_create(v, BSG_NODE_GROUP);
    bsg_node_set_object_type(node, bsg_group_type());
    return node;
}


void
bsg_group_add_child(bsg_node *group, bsg_node *child)
{
    if (!group || !child)
	return;

    bsg_node_add_child(group, child);
}


void
bsg_group_remove_child(bsg_node *group, bsg_node *child)
{
    if (!group || !child)
	return;

    bsg_node_remove_child(group, child);
}


void
bsg_group_destroy(bsg_node *group)
{
    if (!group)
	return;

    bsg_node_destroy(group);
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
