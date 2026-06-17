/*                     I D E N T I T Y . C
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
/** @file libbsg/identity.c
 *
 * BSG node identity and revision model accessors.
 */

#include "common.h"

#include <limits.h>

#include "bsg/defines.h"
#include "bsg/draw_set.h"
#include "bsg/identity.h"
#include "bsg_private.h"
#include "field_private.h"
#include "identity_private.h"
#include "node_private.h"


void
bsg_node_identity_set_name(bsg_node *node, const char *name)
{
    if (!node)
	return;

    bsg_node_set_name(node, name ? name : "");
}


const char *
bsg_node_identity_name(const bsg_node *node)
{
    const char *name = NULL;
    if (!node)
	return NULL;
    if (!bsg_field_get_string(bsg_node_field_ref((bsg_node *)node, BSG_FIELD_NAME), &name))
	return "";
    return name ? name : "";
}


uint64_t
bsg_node_revision_get(const bsg_node *node)
{
    if (!node || !node->i)
	return 0;
    return node->i->revision;
}


void
bsg_node_revision_set(bsg_node *node, uint64_t revision)
{
    if (!node)
	return;
    bsg_node_internal_ensure(node)->revision = revision;
}


uint64_t
bsg_node_revision_bump(bsg_node *node)
{
    if (!node)
	return 0;

    bsg_node_internal_ensure(node)->revision++;
    bsg_bump_rev_node(node);
    return node->i->revision;
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
