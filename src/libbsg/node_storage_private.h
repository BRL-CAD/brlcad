/*              N O D E _ S T O R A G E _ P R I V A T E . H
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
/** @file node_storage_private.h
 *
 * Private node storage accessors that remain while legacy producers finish
 * moving to semantic records and payload-level construction.
 */

#ifndef BSG_NODE_STORAGE_PRIVATE_H
#define BSG_NODE_STORAGE_PRIVATE_H

#include "common.h"
#include "node_fwd_private.h"
#include "bsg/feature.h"
#include "bsg/node.h"

__BEGIN_DECLS

BSG_EXPORT extern void
bsg_node_set_source_flags(bsg_node *node, unsigned int source_flags);

extern unsigned int
bsg_node_source_flags(const bsg_node *node);

BSG_EXPORT extern int
bsg_node_is_database_source(const bsg_node *node);

BSG_EXPORT extern int
bsg_node_is_view_source(const bsg_node *node);

BSG_EXPORT extern int
bsg_node_is_local_source(const bsg_node *node);

BSG_EXPORT extern int
bsg_node_is_internal_child_source(const bsg_node *node);

extern void
bsg_node_set_geometry_roles(bsg_node *node, unsigned int geometry_roles);

extern void
bsg_node_add_geometry_roles(bsg_node *node, unsigned int geometry_roles);

extern unsigned int
bsg_node_geometry_roles(const bsg_node *node);

BSG_EXPORT extern int
bsg_node_has_geometry_role(const bsg_node *node, unsigned int geometry_role);

extern enum bsg_feature_family
bsg_feature_node_family(const bsg_node *node);

extern uint64_t
bsg_node_revision(const bsg_node *node);

extern void
bsg_node_touch(bsg_node *node);

__END_DECLS

#endif /* BSG_NODE_STORAGE_PRIVATE_H */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
