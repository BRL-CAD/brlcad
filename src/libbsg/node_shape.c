/*                   N O D E _ S H A P E . C
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
/** @file libbsg/node_shape.c
 *
 * BSG_NODE_SHAPE lifecycle.
 * A shape node is a drawable leaf that carries typed geometry or source
 * metadata.
 */

#include "common.h"

#include "bu/list.h"
#include "bsg/database_source.h"
#include "bsg/defines.h"
#include "bsg/draw_source.h"
#include "bsg/field.h"
#include "bsg/node.h"
#include "payload_typed_private.h"
#include "bsg/util.h"
#include "bsg_private.h"
#include "node_private.h"
#include "draw_source_private.h"
#include "field_private.h"
#include "node_storage_private.h"
#include "object_private.h"


bsg_node *
bsg_shape_create(struct bsg_view *v)
{
    if (!v)
	return NULL;

    bsg_node *node = bsg_node_create(v, BSG_NODE_SHAPE);
    bsg_node_set_object_type(node, bsg_shape_type());
    return node;
}


void
bsg_shape_mark_db_object(bsg_node *shape)
{
    if (!shape)
	return;
    bsg_node_set_source_flags(shape, bsg_node_source_flags(shape) | BSG_SOURCE_DB);
    bsg_node_set_object_type(shape, bsg_geometry_type());
    bsg_node_set_geometry_roles(shape, BSG_GEOMETRY_DB_OBJECT);
}


void
bsg_scene_mark_db_object(bsg_scene_ref shape)
{
    bsg_shape_mark_db_object((bsg_node *)shape.opaque);
}


int
bsg_shape_is_non_database_source(const bsg_node *shape)
{
    if (!shape)
	return 0;
    if (shape->i && shape->i->non_database_source_valid)
	return shape->i->non_database_source ? 1 : 0;
    return 0;
}


void
bsg_shape_set_non_database_source(bsg_node *shape, int non_database_source)
{
    if (!shape)
	return;
    struct bsg_node_internal *ni = bsg_node_internal_ensure(shape);
    if (ni) {
	ni->non_database_source = non_database_source ? 1 : 0;
	ni->non_database_source_valid = 1;
    }
}


void
bsg_scene_set_non_database_source(bsg_scene_ref shape, int non_database_source)
{
    bsg_shape_set_non_database_source((bsg_node *)shape.opaque,
	    non_database_source);
}


int
bsg_shape_is_mesh(const bsg_node *shape)
{
    if (!shape)
	return 0;
    bsg_node_ref node = bsg_node_ref_from_object(
	    bsg_object_ref_from_node((bsg_node *)shape));
    bsg_database_source_ref source = bsg_database_source_ref_from_node(node);
    int flags = bsg_database_source_ref_realization_role_flags(source);
    if (flags & BSG_DATABASE_SOURCE_REALIZATION_ROLE_MESH)
	return 1;
    return bsg_node_source_realization_is_mesh(shape);
}


int
bsg_shape_arrows_enabled(const bsg_node *shape)
{
    if (!shape)
	return 0;
    if (!shape->i)
	return 0;
    return shape->i->arrows_enabled ? 1 : 0;
}


void
bsg_shape_set_arrows_enabled(bsg_node *shape, int enabled)
{
    if (!shape)
	return;
    bsg_node_internal_ensure(shape)->arrows_enabled = enabled ? 1 : 0;
}


fastf_t
bsg_shape_arrow_tip_length(const bsg_node *shape)
{
    if (!shape)
	return 0.0;
    double length = 0.0;
    if (!bsg_field_get_double(bsg_node_field_ref((bsg_node *)shape,
		    BSG_FIELD_DRAW_ARROW_TIP_LENGTH), &length))
	return 0.0;
    return length;
}


void
bsg_shape_set_arrow_tip_length(bsg_node *shape, fastf_t length)
{
    if (!shape)
	return;
    (void)bsg_field_set_double(bsg_node_field_ref(shape,
		BSG_FIELD_DRAW_ARROW_TIP_LENGTH), length);
}


fastf_t
bsg_shape_arrow_tip_width(const bsg_node *shape)
{
    if (!shape)
	return 0.0;
    double width = 0.0;
    if (!bsg_field_get_double(bsg_node_field_ref((bsg_node *)shape,
		    BSG_FIELD_DRAW_ARROW_TIP_WIDTH), &width))
	return 0.0;
    return width;
}


void
bsg_shape_set_arrow_tip_width(bsg_node *shape, fastf_t width)
{
    if (!shape)
	return;
    (void)bsg_field_set_double(bsg_node_field_ref(shape,
		BSG_FIELD_DRAW_ARROW_TIP_WIDTH), width);
}


void
bsg_shape_destroy(bsg_node *shape)
{
    if (!shape)
	return;

    bsg_node_destroy(shape);
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
