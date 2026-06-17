/*                O B J E C T _ P R I V A T E . H
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
/** @file object_private.h
 *
 * Source-private object/type bridge used while scene-ref authoring and raw
 * node internals are migrated to public typed object refs.
 */

#ifndef BSG_OBJECT_PRIVATE_H
#define BSG_OBJECT_PRIVATE_H

#include "common.h"
#include "node_fwd_private.h"
#include "bsg/object.h"
#include "bsg/scene_builder.h"

__BEGIN_DECLS

BSG_EXPORT extern bsg_object_ref
bsg_object_ref_from_node(bsg_node *node);

BSG_EXPORT extern bsg_node *
bsg_object_ref_node(bsg_object_ref object);

BSG_EXPORT extern void
bsg_node_set_object_type(bsg_node *node, bsg_type_id type);

BSG_EXPORT extern bsg_type_id
bsg_node_object_type(const bsg_node *node);

BSG_EXPORT extern int
bsg_node_type_is_a(const bsg_node *node, bsg_type_id type);

BSG_EXPORT extern bsg_object_ref
bsg_object_ref_from_scene_ref(bsg_scene_ref ref);

BSG_EXPORT extern bsg_scene_ref
bsg_scene_ref_from_object_ref(bsg_object_ref object);

__END_DECLS

#endif /* BSG_OBJECT_PRIVATE_H */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
