/*                     M A T E R I A L . C
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
/** @file libbsg/material.c
 *
 * BSG node material accessors.
 */

#include "common.h"

#include "bsg/material.h"
#include "bsg_private.h"
#include "field_private.h"
#include "material_private.h"
#include "node_private.h"
#include "object_private.h"

static bsg_node *
_material_node(bsg_material_ref material)
{
    if (!bsg_type_is_a(bsg_object_type(material.node.object), bsg_material_type()))
	return NULL;
    return bsg_object_ref_node(material.node.object);
}

bsg_field_ref
bsg_material_ref_diffuse_color_field(bsg_material_ref material)
{
    return bsg_node_field_ref(_material_node(material), BSG_FIELD_MATERIAL_DIFFUSE_COLOR);
}

bsg_field_ref
bsg_material_ref_specular_color_field(bsg_material_ref material)
{
    return bsg_node_field_ref(_material_node(material), BSG_FIELD_MATERIAL_SPECULAR_COLOR);
}

bsg_field_ref
bsg_material_ref_emissive_color_field(bsg_material_ref material)
{
    return bsg_node_field_ref(_material_node(material), BSG_FIELD_MATERIAL_EMISSIVE_COLOR);
}

bsg_field_ref
bsg_material_ref_alpha_field(bsg_material_ref material)
{
    return bsg_node_field_ref(_material_node(material), BSG_FIELD_MATERIAL_ALPHA);
}

bsg_field_ref
bsg_material_ref_shininess_field(bsg_material_ref material)
{
    return bsg_node_field_ref(_material_node(material), BSG_FIELD_MATERIAL_SHININESS);
}

bsg_field_ref
bsg_material_ref_line_color_field(bsg_material_ref material)
{
    return bsg_node_field_ref(_material_node(material), BSG_FIELD_MATERIAL_LINE_COLOR);
}

int
bsg_material_ref_set_diffuse_color(bsg_material_ref material, const unsigned char color[3])
{
    return bsg_field_set_color3(bsg_material_ref_diffuse_color_field(material), color);
}

int
bsg_material_ref_diffuse_color(bsg_material_ref material, unsigned char color[3])
{
    return bsg_field_get_color3(bsg_material_ref_diffuse_color_field(material), color);
}

int
bsg_material_ref_set_specular_color(bsg_material_ref material, const unsigned char color[3])
{
    return bsg_field_set_color3(bsg_material_ref_specular_color_field(material), color);
}

int
bsg_material_ref_specular_color(bsg_material_ref material, unsigned char color[3])
{
    return bsg_field_get_color3(bsg_material_ref_specular_color_field(material), color);
}

int
bsg_material_ref_set_emissive_color(bsg_material_ref material, const unsigned char color[3])
{
    return bsg_field_set_color3(bsg_material_ref_emissive_color_field(material), color);
}

int
bsg_material_ref_emissive_color(bsg_material_ref material, unsigned char color[3])
{
    return bsg_field_get_color3(bsg_material_ref_emissive_color_field(material), color);
}

int
bsg_material_ref_set_alpha(bsg_material_ref material, double alpha)
{
    return bsg_field_set_double(bsg_material_ref_alpha_field(material), alpha);
}

double
bsg_material_ref_alpha(bsg_material_ref material)
{
    double alpha = 1.0;
    (void)bsg_field_get_double(bsg_material_ref_alpha_field(material), &alpha);
    return alpha;
}

int
bsg_material_ref_set_shininess(bsg_material_ref material, double shininess)
{
    return bsg_field_set_double(bsg_material_ref_shininess_field(material), shininess);
}

double
bsg_material_ref_shininess(bsg_material_ref material)
{
    double shininess = 0.0;
    (void)bsg_field_get_double(bsg_material_ref_shininess_field(material), &shininess);
    return shininess;
}

int
bsg_material_ref_set_line_color(bsg_material_ref material, const unsigned char color[3])
{
    return bsg_field_set_color3(bsg_material_ref_line_color_field(material), color);
}

int
bsg_material_ref_line_color(bsg_material_ref material, unsigned char color[3])
{
    return bsg_field_get_color3(bsg_material_ref_line_color_field(material), color);
}


void
bsg_material_set_rgb(bsg_node *node, unsigned char r, unsigned char g, unsigned char b)
{
    if (!node)
	return;

    unsigned char color[3];
    color[0] = r;
    color[1] = g;
    color[2] = b;
    (void)bsg_field_set_color3(bsg_node_field_ref(node, BSG_FIELD_COLOR), color);
}


void
bsg_scene_material_set_rgb(bsg_scene_ref ref, unsigned char r, unsigned char g, unsigned char b)
{
    bsg_material_set_rgb((bsg_node *)ref.opaque, r, g, b);
}


void
bsg_material_get_rgb(const bsg_node *node, unsigned char *r, unsigned char *g, unsigned char *b)
{
    if (!node)
	return;

    unsigned char color[3] = {0, 0, 0};
    if (!bsg_field_get_color3(bsg_node_field_ref((bsg_node *)node, BSG_FIELD_COLOR), color))
	return;
    if (r)
	*r = color[0];
    if (g)
	*g = color[1];
    if (b)
	*b = color[2];
}


void
bsg_scene_material_get_rgb(bsg_scene_ref ref, unsigned char *r, unsigned char *g, unsigned char *b)
{
    bsg_material_get_rgb((const bsg_node *)ref.opaque, r, g, b);
}


void
bsg_material_set_revision(bsg_node *node, uint32_t revision)
{
    if (!node)
	return;
    bsg_node_internal_ensure(node)->material_revision = revision;
}


void
bsg_scene_material_set_revision(bsg_scene_ref ref, uint32_t revision)
{
    bsg_material_set_revision((bsg_node *)ref.opaque, revision);
}


uint32_t
bsg_material_revision(const bsg_node *node)
{
    if (!node || !node->i)
	return 0;
    return node->i->material_revision;
}


uint32_t
bsg_scene_material_revision(bsg_scene_ref ref)
{
    return bsg_material_revision((const bsg_node *)ref.opaque);
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
