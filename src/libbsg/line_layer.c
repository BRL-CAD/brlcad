/*                    L I N E _ L A Y E R . C
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
/** @file libbsg/line_layer.c
 *
 * Typed multi-color line layer builder.
 */

#include "common.h"

#include <string.h>

#include "bu/malloc.h"
#include "bu/vls.h"
#include "bsg/feature.h"
#include "bsg/geometry.h"


struct bsg_line_layer {
    long rgb;
    char *name;
    point_t *points;
    int *commands;
    size_t point_count;
    size_t point_capacity;
};

struct bsg_line_layer_builder {
    struct bsg_line_layer *layers;
    size_t layer_count;
    size_t layer_capacity;
};


static long
_line_layer_rgb(int r, int g, int b)
{
    return ((r & 0xFF) << 16) | ((g & 0xFF) << 8) | (b & 0xFF);
}


static int
_line_layer_geometry_cmd_valid(int command)
{
    switch (command) {
	case BSG_GEOMETRY_LINE_MOVE:
	case BSG_GEOMETRY_LINE_DRAW:
	case BSG_GEOMETRY_POINT_DRAW:
	    return 1;
	default:
	    break;
    }
    return 0;
}


static void
_line_layer_free(struct bsg_line_layer *layer)
{
    if (!layer)
	return;

    if (layer->name)
	bu_free(layer->name, "bsg line layer name");
    if (layer->points)
	bu_free(layer->points, "bsg line layer points");
    if (layer->commands)
	bu_free(layer->commands, "bsg line layer commands");
    memset(layer, 0, sizeof(*layer));
}


static int
_line_layer_reserve_points(struct bsg_line_layer *layer, size_t needed)
{
    if (!layer)
	return 0;
    if (needed <= layer->point_capacity)
	return 1;

    size_t capacity = layer->point_capacity ? layer->point_capacity : 64;
    while (capacity < needed)
	capacity *= 2;

    layer->points = (point_t *)bu_realloc(layer->points, capacity * sizeof(point_t),
	    "bsg line layer points");
    layer->commands = (int *)bu_realloc(layer->commands, capacity * sizeof(int),
	    "bsg line layer commands");
    layer->point_capacity = capacity;
    return 1;
}


static struct bsg_line_layer *
_line_layer_find_or_create(struct bsg_line_layer_builder *builder, int r, int g, int b)
{
    if (!builder)
	return NULL;

    long rgb = _line_layer_rgb(r, g, b);
    for (size_t i = 0; i < builder->layer_count; i++) {
	if (builder->layers[i].rgb == rgb)
	    return &builder->layers[i];
    }

    if (builder->layer_count == builder->layer_capacity) {
	size_t capacity = builder->layer_capacity ? builder->layer_capacity * 2 : 8;
	builder->layers = (struct bsg_line_layer *)bu_realloc(builder->layers,
		capacity * sizeof(struct bsg_line_layer),
		"bsg line layer builder layers");
	for (size_t i = builder->layer_capacity; i < capacity; i++)
	    memset(&builder->layers[i], 0, sizeof(builder->layers[i]));
	builder->layer_capacity = capacity;
    }

    struct bsg_line_layer *layer = &builder->layers[builder->layer_count++];
    memset(layer, 0, sizeof(*layer));
    layer->rgb = rgb;

    struct bu_vls lname = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&lname, "rgb_%03ld_%03ld_%03ld",
	    (rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, rgb & 0xFF);
    layer->name = bu_strdup(bu_vls_cstr(&lname));
    bu_vls_free(&lname);

    return layer;
}


struct bsg_line_layer_builder *
bsg_line_layer_builder_create(void)
{
    struct bsg_line_layer_builder *builder = NULL;
    BU_ALLOC(builder, struct bsg_line_layer_builder);
    return builder;
}


void
bsg_line_layer_builder_clear(struct bsg_line_layer_builder *builder)
{
    if (!builder)
	return;

    for (size_t i = 0; i < builder->layer_count; i++)
	_line_layer_free(&builder->layers[i]);
    builder->layer_count = 0;
}


void
bsg_line_layer_builder_free(struct bsg_line_layer_builder *builder)
{
    if (!builder)
	return;

    bsg_line_layer_builder_clear(builder);
    if (builder->layers)
	bu_free(builder->layers, "bsg line layer builder layers");
    bu_free(builder, "bsg line layer builder");
}


size_t
bsg_line_layer_builder_layer_count(const struct bsg_line_layer_builder *builder)
{
    return builder ? builder->layer_count : 0;
}


size_t
bsg_line_layer_builder_point_count(const struct bsg_line_layer_builder *builder)
{
    size_t count = 0;
    if (!builder)
	return 0;

    for (size_t i = 0; i < builder->layer_count; i++)
	count += builder->layers[i].point_count;
    return count;
}


const struct bsg_line_layer *
bsg_line_layer_builder_layer_at(const struct bsg_line_layer_builder *builder,
				size_t idx)
{
    if (!builder || idx >= builder->layer_count)
	return NULL;
    return &builder->layers[idx];
}


struct bsg_line_layer *
bsg_line_layer_builder_find(struct bsg_line_layer_builder *builder,
			    int r,
			    int g,
			    int b)
{
    return _line_layer_find_or_create(builder, r, g, b);
}


int
bsg_line_layer_add(struct bsg_line_layer *layer,
		   const point_t point,
		   int command)
{
    if (!layer || !point || !_line_layer_geometry_cmd_valid(command))
	return 0;

    if (!_line_layer_reserve_points(layer, layer->point_count + 1))
	return 0;

    VMOVE(layer->points[layer->point_count], point);
    layer->commands[layer->point_count] = command;
    layer->point_count++;
    return 1;
}


int
bsg_line_layer_builder_add(struct bsg_line_layer_builder *builder,
			   int r,
			   int g,
			   int b,
			   const point_t point,
			   int command)
{
    struct bsg_line_layer *layer = bsg_line_layer_builder_find(builder, r, g, b);
    return bsg_line_layer_add(layer, point, command);
}


int
bsg_line_layer_color(const struct bsg_line_layer *layer,
		     unsigned char *r,
		     unsigned char *g,
		     unsigned char *b)
{
    if (!layer)
	return 0;
    if (r)
	*r = (unsigned char)((layer->rgb >> 16) & 0xFF);
    if (g)
	*g = (unsigned char)((layer->rgb >> 8) & 0xFF);
    if (b)
	*b = (unsigned char)(layer->rgb & 0xFF);
    return 1;
}


size_t
bsg_line_layer_point_count(const struct bsg_line_layer *layer)
{
    return layer ? layer->point_count : 0;
}


int
bsg_line_layer_point_at(const struct bsg_line_layer *layer,
			size_t idx,
			point_t point)
{
    if (!layer || !point || idx >= layer->point_count)
	return 0;
    VMOVE(point, layer->points[idx]);
    return 1;
}


int
bsg_line_layer_command_at(const struct bsg_line_layer *layer,
			  size_t idx,
			  int *command)
{
    if (!layer || !command || idx >= layer->point_count)
	return 0;
    *command = layer->commands[idx];
    return 1;
}


bsg_feature_ref
bsg_feature_replace_line_layer_builder(struct bsg_view *v,
				       const char *name,
				       int local,
				       const struct bsg_line_layer_builder *builder,
				       const struct bsg_feature_style *style)
{
    bsg_feature_ref null_ref = BSG_FEATURE_REF_NULL_INIT;
    if (!v || !name)
	return null_ref;

    if (!builder || !builder->layer_count)
	return bsg_feature_replace_line_layers(v, name, local, NULL, 0, style);

    struct bsg_feature_line_layer *layers = (struct bsg_feature_line_layer *)bu_calloc(
	    builder->layer_count, sizeof(struct bsg_feature_line_layer), "bsg feature line layers");

    for (size_t i = 0; i < builder->layer_count; i++) {
	const struct bsg_line_layer *src = &builder->layers[i];
	struct bsg_feature_line_layer init = BSG_FEATURE_LINE_LAYER_INIT;
	layers[i] = init;
	layers[i].name = src->name;
	layers[i].points = (const point_t *)src->points;
	layers[i].commands = src->commands;
	layers[i].point_count = src->point_count;
	layers[i].style.color_valid = 1;
	layers[i].style.color[0] = (unsigned char)((src->rgb >> 16) & 0xFF);
	layers[i].style.color[1] = (unsigned char)((src->rgb >> 8) & 0xFF);
	layers[i].style.color[2] = (unsigned char)(src->rgb & 0xFF);
    }

    bsg_feature_ref ref = bsg_feature_replace_line_layers(v, name, local,
	    (const struct bsg_feature_line_layer *)layers, builder->layer_count, style);
    bu_free(layers, "bsg feature line layers");
    return ref;
}
