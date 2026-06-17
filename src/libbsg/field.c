/*                      F I E L D . C
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
/** @file libbsg/field.c
 *
 * Node-owned field storage and public field accessors.
 */

#include "common.h"

#include <math.h>
#include <string.h>

#include "bu/malloc.h"
#include "bu/str.h"
#include "bu/vls.h"
#include "bu/ptbl.h"
#include "vmath.h"

#include "bsg/field.h"
#include "bsg/group.h"
#include "bsg/switch.h"
#include "bsg_private.h"
#include "draw_set_private.h"
#include "field_private.h"
#include "node_private.h"
#include "node_storage_private.h"
#include "object_private.h"
#include "sensor_private.h"

static bsg_field_ref
_field_ref(struct bsg_field *field)
{
    bsg_field_ref ref = BSG_FIELD_REF_NULL_INIT;
    ref.opaque = field;
    return ref;
}

static struct bsg_field *
_field_from_ref(bsg_field_ref ref)
{
    return (struct bsg_field *)ref.opaque;
}

static bsg_node_ref
_node_ref_from_node(bsg_node *node)
{
    bsg_node_ref ref = BSG_NODE_REF_NULL_INIT;
    ref.object = bsg_object_ref_from_node(node);
    return ref;
}

static bsg_node *
_node_from_ref(bsg_node_ref ref)
{
    return bsg_object_ref_node(ref.object);
}

static void
_field_string_set(struct bsg_field *field, const char *value)
{
    if (!field)
	return;
    if (field->value.string_value) {
	bu_free(field->value.string_value, "bsg field string");
	field->value.string_value = NULL;
    }
    field->value.string_value = bu_strdup(value ? value : "");
}

static void
_field_clear_dynamic(struct bsg_field *field)
{
    if (!field)
	return;

    switch (field->value_type) {
	case BSG_FIELD_VALUE_STRING:
	    if (field->value.string_value)
		bu_free(field->value.string_value, "bsg field string");
	    break;
	case BSG_FIELD_VALUE_MULTI_DOUBLE:
	    if (field->value.multi_double_value.values)
		bu_free(field->value.multi_double_value.values, "bsg field multi double");
	    break;
	case BSG_FIELD_VALUE_MULTI_POINT:
	    if (field->value.multi_point_value.values)
		bu_free(field->value.multi_point_value.values, "bsg field multi point");
	    break;
	case BSG_FIELD_VALUE_MULTI_INT:
	    if (field->value.multi_int_value.values)
		bu_free(field->value.multi_int_value.values, "bsg field multi int");
	    break;
	case BSG_FIELD_VALUE_MULTI_COLOR3:
	    if (field->value.multi_color3_value.values)
		bu_free(field->value.multi_color3_value.values, "bsg field multi color");
	    break;
	default:
	    break;
    }
}

static void
_field_init(struct bsg_field *field,
	    bsg_node *owner,
	    const char *name,
	    bsg_field_value_type value_type,
	    bsg_field_id_t id)
{
    if (!field)
	return;
    _field_clear_dynamic(field);
    memset(field, 0, sizeof(*field));
    field->owner = owner;
    field->name = name;
    field->value_type = value_type;
    field->id = id;
}

static struct bsg_node_fields *
_fields_get(const bsg_node *node)
{
    return (node && node->i) ? node->i->fields : NULL;
}

static struct bsg_node_fields *
_fields_ensure(bsg_node *node)
{
    if (!node)
	return NULL;
    struct bsg_node_internal *ni = bsg_node_internal_ensure(node);
    if (!ni->fields) {
	BU_ALLOC(ni->fields, struct bsg_node_fields);
	memset(ni->fields, 0, sizeof(struct bsg_node_fields));
    }
    return ni->fields;
}

static void
_fields_sync_from_legacy(bsg_node *node)
{
    struct bsg_node_fields *fields = _fields_get(node);
    if (!node || !fields)
	return;

    if (fields->name.revision == 0 && !fields->name.value.string_value)
	_field_string_set(&fields->name, "");
    if (fields->visibility.revision == 0)
	fields->visibility.value.bool_value = 1;
    if (fields->color.revision == 0) {
	if (fields->material_diffuse_color.revision != 0) {
	    fields->color.value.color3_value[0] = fields->material_diffuse_color.value.color3_value[0];
	    fields->color.value.color3_value[1] = fields->material_diffuse_color.value.color3_value[1];
	    fields->color.value.color3_value[2] = fields->material_diffuse_color.value.color3_value[2];
	} else {
	    fields->color.value.color3_value[0] = 255;
	    fields->color.value.color3_value[1] = 0;
	    fields->color.value.color3_value[2] = 0;
	}
    }
    if (fields->transform.revision == 0)
	MAT_IDN(fields->transform.value.matrix_value);
    if (fields->bounds.revision == 0) {
	VSETALL(fields->bounds.value.bbox_value.min, 0.0);
	VSETALL(fields->bounds.value.bbox_value.max, 0.0);
	fields->bounds.value.bbox_value.valid = 0;
    }
    if (fields->switch_which_child.revision == 0)
	fields->switch_which_child.value.int_value = BSG_SWITCH_NONE;
    if (fields->material_diffuse_color.revision == 0) {
	fields->material_diffuse_color.value.color3_value[0] = 255;
	fields->material_diffuse_color.value.color3_value[1] = 0;
	fields->material_diffuse_color.value.color3_value[2] = 0;
    }
    if (fields->material_line_color.revision == 0) {
	fields->material_line_color.value.color3_value[0] = 255;
	fields->material_line_color.value.color3_value[1] = 0;
	fields->material_line_color.value.color3_value[2] = 0;
    }
    if (fields->draw_line_width.revision == 0)
	fields->draw_line_width.value.int_value = 1;
    if (fields->draw_line_style.revision == 0)
	fields->draw_line_style.value.int_value = 0;
    if (fields->draw_fill_mode.revision == 0)
	fields->draw_fill_mode.value.enum_value = 0;
    if (fields->material_alpha.revision == 0)
	fields->material_alpha.value.double_value = 1.0;
    if (fields->draw_arrow_tip_length.revision == 0)
	fields->draw_arrow_tip_length.value.double_value = 0.0;
    if (fields->draw_arrow_tip_width.revision == 0)
	fields->draw_arrow_tip_width.value.double_value = 0.0;
}

void
bsg_node_fields_reset(bsg_node *node)
{
    struct bsg_node_fields *fields = _fields_ensure(node);
    if (!fields)
	return;

    _field_init(&fields->name, node, "name", BSG_FIELD_VALUE_STRING, BSG_FIELD_NAME);
    _field_init(&fields->visibility, node, "visibility", BSG_FIELD_VALUE_BOOL, BSG_FIELD_VISIBILITY);
    _field_init(&fields->color, node, "color", BSG_FIELD_VALUE_COLOR3, BSG_FIELD_COLOR);
    _field_init(&fields->transform, node, "transform", BSG_FIELD_VALUE_MATRIX, BSG_FIELD_TRANSFORM);
    _field_init(&fields->bounds, node, "bounds", BSG_FIELD_VALUE_BBOX, BSG_FIELD_BOUNDS);
    _field_init(&fields->switch_which_child, node, "whichChild", BSG_FIELD_VALUE_INT, BSG_FIELD_SWITCH_WHICH);
    _field_init(&fields->children, node, "children", BSG_FIELD_VALUE_MULTI_NODE, BSG_FIELD_CHILDREN);
    _field_init(&fields->material_diffuse_color, node, "diffuseColor", BSG_FIELD_VALUE_COLOR3, BSG_FIELD_MATERIAL_DIFFUSE_COLOR);
    _field_init(&fields->material_specular_color, node, "specularColor", BSG_FIELD_VALUE_COLOR3, BSG_FIELD_MATERIAL_SPECULAR_COLOR);
    _field_init(&fields->material_emissive_color, node, "emissiveColor", BSG_FIELD_VALUE_COLOR3, BSG_FIELD_MATERIAL_EMISSIVE_COLOR);
    _field_init(&fields->material_alpha, node, "alpha", BSG_FIELD_VALUE_DOUBLE, BSG_FIELD_MATERIAL_ALPHA);
    _field_init(&fields->material_shininess, node, "shininess", BSG_FIELD_VALUE_DOUBLE, BSG_FIELD_MATERIAL_SHININESS);
    _field_init(&fields->material_line_color, node, "lineColor", BSG_FIELD_VALUE_COLOR3, BSG_FIELD_MATERIAL_LINE_COLOR);
    _field_init(&fields->draw_line_width, node, "lineWidth", BSG_FIELD_VALUE_INT, BSG_FIELD_DRAW_LINE_WIDTH);
    _field_init(&fields->draw_line_style, node, "lineStyle", BSG_FIELD_VALUE_INT, BSG_FIELD_DRAW_LINE_STYLE);
    _field_init(&fields->draw_fill_mode, node, "fillMode", BSG_FIELD_VALUE_ENUM, BSG_FIELD_DRAW_FILL_MODE);
    _field_init(&fields->draw_point_size, node, "pointSize", BSG_FIELD_VALUE_DOUBLE, BSG_FIELD_DRAW_POINT_SIZE);
    _field_init(&fields->draw_transparency_policy, node, "transparencyPolicy", BSG_FIELD_VALUE_ENUM, BSG_FIELD_DRAW_TRANSPARENCY_POLICY);
    _field_init(&fields->draw_arrow_tip_length, node, "arrowTipLength", BSG_FIELD_VALUE_DOUBLE, BSG_FIELD_DRAW_ARROW_TIP_LENGTH);
    _field_init(&fields->draw_arrow_tip_width, node, "arrowTipWidth", BSG_FIELD_VALUE_DOUBLE, BSG_FIELD_DRAW_ARROW_TIP_WIDTH);
    _field_init(&fields->complexity_value, node, "complexity", BSG_FIELD_VALUE_DOUBLE, BSG_FIELD_COMPLEXITY_VALUE);
    _field_init(&fields->complexity_curve_scale, node, "curveScale", BSG_FIELD_VALUE_DOUBLE, BSG_FIELD_COMPLEXITY_CURVE_SCALE);
    _field_init(&fields->complexity_point_scale, node, "pointScale", BSG_FIELD_VALUE_DOUBLE, BSG_FIELD_COMPLEXITY_POINT_SCALE);
    _field_init(&fields->complexity_lod_policy, node, "lodPolicy", BSG_FIELD_VALUE_ENUM, BSG_FIELD_COMPLEXITY_LOD_POLICY);
    _field_init(&fields->complexity_lod_level, node, "lodLevel", BSG_FIELD_VALUE_INT, BSG_FIELD_COMPLEXITY_LOD_LEVEL);
    _field_init(&fields->camera_projection, node, "projection", BSG_FIELD_VALUE_ENUM, BSG_FIELD_CAMERA_PROJECTION);
    _field_init(&fields->camera_perspective, node, "perspective", BSG_FIELD_VALUE_DOUBLE, BSG_FIELD_CAMERA_PERSPECTIVE);
    _field_init(&fields->camera_position, node, "position", BSG_FIELD_VALUE_VEC3, BSG_FIELD_CAMERA_POSITION);
    _field_init(&fields->camera_orientation, node, "orientation", BSG_FIELD_VALUE_MATRIX, BSG_FIELD_CAMERA_ORIENTATION);
    _field_init(&fields->camera_near_distance, node, "nearDistance", BSG_FIELD_VALUE_DOUBLE, BSG_FIELD_CAMERA_NEAR_DISTANCE);
    _field_init(&fields->camera_far_distance, node, "farDistance", BSG_FIELD_VALUE_DOUBLE, BSG_FIELD_CAMERA_FAR_DISTANCE);
    _field_init(&fields->light_type, node, "lightType", BSG_FIELD_VALUE_ENUM, BSG_FIELD_LIGHT_TYPE);
    _field_init(&fields->light_position, node, "lightPosition", BSG_FIELD_VALUE_VEC3, BSG_FIELD_LIGHT_POSITION);
    _field_init(&fields->light_direction, node, "lightDirection", BSG_FIELD_VALUE_VEC3, BSG_FIELD_LIGHT_DIRECTION);
    _field_init(&fields->light_diffuse_color, node, "lightDiffuseColor", BSG_FIELD_VALUE_COLOR3, BSG_FIELD_LIGHT_DIFFUSE_COLOR);
    _field_init(&fields->light_specular_color, node, "lightSpecularColor", BSG_FIELD_VALUE_COLOR3, BSG_FIELD_LIGHT_SPECULAR_COLOR);
    _field_init(&fields->light_ambient_color, node, "lightAmbientColor", BSG_FIELD_VALUE_COLOR3, BSG_FIELD_LIGHT_AMBIENT_COLOR);
    _field_init(&fields->light_intensity, node, "lightIntensity", BSG_FIELD_VALUE_DOUBLE, BSG_FIELD_LIGHT_INTENSITY);
    _field_init(&fields->light_active, node, "lightActive", BSG_FIELD_VALUE_BOOL, BSG_FIELD_LIGHT_ACTIVE);
    _field_init(&fields->geometry_kind, node, "geometryKind", BSG_FIELD_VALUE_ENUM, BSG_FIELD_GEOMETRY_KIND);
    _field_init(&fields->geometry_coordinates, node, "coordinates", BSG_FIELD_VALUE_MULTI_POINT, BSG_FIELD_GEOMETRY_COORDINATES);
    _field_init(&fields->geometry_normals, node, "normals", BSG_FIELD_VALUE_MULTI_POINT, BSG_FIELD_GEOMETRY_NORMALS);
    _field_init(&fields->geometry_colors, node, "colors", BSG_FIELD_VALUE_MULTI_COLOR3, BSG_FIELD_GEOMETRY_COLORS);
    _field_init(&fields->geometry_indices, node, "indices", BSG_FIELD_VALUE_MULTI_INT, BSG_FIELD_GEOMETRY_INDICES);
    _field_init(&fields->geometry_primitive_sets, node, "primitiveSets", BSG_FIELD_VALUE_MULTI_INT, BSG_FIELD_GEOMETRY_PRIMITIVE_SETS);
    _field_init(&fields->geometry_text, node, "text", BSG_FIELD_VALUE_STRING, BSG_FIELD_GEOMETRY_TEXT);
    _field_init(&fields->geometry_image_width, node, "imageWidth", BSG_FIELD_VALUE_UINT64, BSG_FIELD_GEOMETRY_IMAGE_WIDTH);
    _field_init(&fields->geometry_image_height, node, "imageHeight", BSG_FIELD_VALUE_UINT64, BSG_FIELD_GEOMETRY_IMAGE_HEIGHT);
    _field_init(&fields->geometry_image_channels, node, "imageChannels", BSG_FIELD_VALUE_UINT64, BSG_FIELD_GEOMETRY_IMAGE_CHANNELS);
    _field_init(&fields->geometry_source_revision, node, "geometryRevision", BSG_FIELD_VALUE_UINT64, BSG_FIELD_GEOMETRY_SOURCE_REVISION);
    _field_init(&fields->lod_selection, node, "lodSelection", BSG_FIELD_VALUE_ENUM, BSG_FIELD_LOD_SELECTION);
    _field_init(&fields->lod_active_level, node, "activeLevel", BSG_FIELD_VALUE_INT, BSG_FIELD_LOD_ACTIVE_LEVEL);
    _field_init(&fields->lod_ranges, node, "ranges", BSG_FIELD_VALUE_MULTI_DOUBLE, BSG_FIELD_LOD_RANGES);
    _field_init(&fields->database_source_path, node, "databasePath", BSG_FIELD_VALUE_STRING, BSG_FIELD_DATABASE_SOURCE_PATH);
    _field_init(&fields->database_source_draw_mode, node, "sourceDrawMode", BSG_FIELD_VALUE_ENUM, BSG_FIELD_DATABASE_SOURCE_DRAW_MODE);
    _field_init(&fields->database_source_material_policy, node, "materialPolicy", BSG_FIELD_VALUE_ENUM, BSG_FIELD_DATABASE_SOURCE_MATERIAL_POLICY);
    _field_init(&fields->database_source_revision, node, "sourceRevision", BSG_FIELD_VALUE_UINT64, BSG_FIELD_DATABASE_SOURCE_REVISION);
    _field_init(&fields->database_source_inputs_revision, node, "inputsRevision", BSG_FIELD_VALUE_UINT64, BSG_FIELD_DATABASE_SOURCE_INPUTS_REVISION);
    _field_init(&fields->database_source_realized_revision, node, "realizedSourceRevision", BSG_FIELD_VALUE_UINT64, BSG_FIELD_DATABASE_SOURCE_REALIZED_REVISION);
    _field_init(&fields->database_source_realized_inputs_revision, node, "realizedInputsRevision", BSG_FIELD_VALUE_UINT64, BSG_FIELD_DATABASE_SOURCE_REALIZED_INPUTS_REVISION);
    _field_init(&fields->database_source_stale_reason, node, "staleReason", BSG_FIELD_VALUE_ENUM, BSG_FIELD_DATABASE_SOURCE_STALE_REASON);
    _field_init(&fields->database_source_realization_identity, node, "realizationIdentity", BSG_FIELD_VALUE_UINT64, BSG_FIELD_DATABASE_SOURCE_REALIZATION_IDENTITY);
    _field_init(&fields->database_source_realization_status, node, "realizationStatus", BSG_FIELD_VALUE_ENUM, BSG_FIELD_DATABASE_SOURCE_REALIZATION_STATUS);
    _field_init(&fields->database_source_realization_role_flags, node, "realizationRoleFlags", BSG_FIELD_VALUE_INT, BSG_FIELD_DATABASE_SOURCE_REALIZATION_ROLE_FLAGS);
    _field_init(&fields->database_source_realization_view_dependent, node, "realizationViewDependent", BSG_FIELD_VALUE_BOOL, BSG_FIELD_DATABASE_SOURCE_REALIZATION_VIEW_DEPENDENT);
    _field_init(&fields->database_source_realization_view_scale, node, "realizationViewScale", BSG_FIELD_VALUE_DOUBLE, BSG_FIELD_DATABASE_SOURCE_REALIZATION_VIEW_SCALE);
    _field_init(&fields->database_source_realization_bot_threshold, node, "realizationBotThreshold", BSG_FIELD_VALUE_UINT64, BSG_FIELD_DATABASE_SOURCE_REALIZATION_BOT_THRESHOLD);
    _field_init(&fields->database_source_realization_curve_scale, node, "realizationCurveScale", BSG_FIELD_VALUE_DOUBLE, BSG_FIELD_DATABASE_SOURCE_REALIZATION_CURVE_SCALE);
    _field_init(&fields->database_source_realization_point_scale, node, "realizationPointScale", BSG_FIELD_VALUE_DOUBLE, BSG_FIELD_DATABASE_SOURCE_REALIZATION_POINT_SCALE);
    {
	unsigned char white[3] = {255, 255, 255};
	unsigned char black[3] = {0, 0, 0};
	vect_t light_dir = VINIT_ZERO;
	MAT_IDN(fields->camera_orientation.value.matrix_value);
	fields->material_specular_color.value.color3_value[0] = white[0];
	fields->material_specular_color.value.color3_value[1] = white[1];
	fields->material_specular_color.value.color3_value[2] = white[2];
	fields->material_emissive_color.value.color3_value[0] = black[0];
	fields->material_emissive_color.value.color3_value[1] = black[1];
	fields->material_emissive_color.value.color3_value[2] = black[2];
	fields->material_alpha.value.double_value = 1.0;
	fields->material_shininess.value.double_value = 0.2;
	fields->draw_line_width.value.int_value = 1;
	fields->draw_point_size.value.double_value = 1.0;
	fields->complexity_value.value.double_value = 0.5;
	fields->complexity_curve_scale.value.double_value = 1.0;
	fields->complexity_point_scale.value.double_value = 1.0;
	fields->complexity_lod_level.value.int_value = -1;
	fields->lod_active_level.value.int_value = -1;
	fields->camera_far_distance.value.double_value = INFINITY;
	fields->light_diffuse_color.value.color3_value[0] = white[0];
	fields->light_diffuse_color.value.color3_value[1] = white[1];
	fields->light_diffuse_color.value.color3_value[2] = white[2];
	fields->light_specular_color.value.color3_value[0] = white[0];
	fields->light_specular_color.value.color3_value[1] = white[1];
	fields->light_specular_color.value.color3_value[2] = white[2];
	fields->light_ambient_color.value.color3_value[0] = 51;
	fields->light_ambient_color.value.color3_value[1] = 51;
	fields->light_ambient_color.value.color3_value[2] = 51;
	VSET(light_dir, 0.0, 0.0, -1.0);
	VMOVE(fields->light_direction.value.vec3_value, light_dir);
	fields->light_intensity.value.double_value = 1.0;
	fields->light_active.value.bool_value = 1;
    }
    _fields_sync_from_legacy(node);
}

void
bsg_node_fields_free(bsg_node *node)
{
    struct bsg_node_fields *fields = _fields_get(node);
    if (!node || !fields)
	return;
    _field_clear_dynamic(&fields->name);
    _field_clear_dynamic(&fields->geometry_coordinates);
    _field_clear_dynamic(&fields->geometry_normals);
    _field_clear_dynamic(&fields->geometry_colors);
    _field_clear_dynamic(&fields->geometry_indices);
    _field_clear_dynamic(&fields->geometry_primitive_sets);
    _field_clear_dynamic(&fields->geometry_text);
    _field_clear_dynamic(&fields->lod_ranges);
    _field_clear_dynamic(&fields->database_source_path);
    bu_free(fields, "bsg node fields");
    node->i->fields = NULL;
}

static struct bsg_field *
_node_field(bsg_node *node, bsg_field_id_t fid)
{
    struct bsg_node_fields *fields = _fields_ensure(node);
    if (!fields)
	return NULL;
    if (!fields->name.owner)
	bsg_node_fields_reset(node);
    else
	_fields_sync_from_legacy(node);

    switch (fid) {
	case BSG_FIELD_NAME:
	    return &fields->name;
	case BSG_FIELD_FLAG:
	case BSG_FIELD_VISIBILITY:
	    return &fields->visibility;
	case BSG_FIELD_COLOR:
	    return &fields->color;
	case BSG_FIELD_TRANSFORM:
	    return &fields->transform;
	case BSG_FIELD_BOUNDS:
	    return &fields->bounds;
	case BSG_FIELD_SWITCH_WHICH:
	    return &fields->switch_which_child;
	case BSG_FIELD_CHILDREN:
	    return &fields->children;
	case BSG_FIELD_MATERIAL_DIFFUSE_COLOR:
	    return &fields->material_diffuse_color;
	case BSG_FIELD_MATERIAL_SPECULAR_COLOR:
	    return &fields->material_specular_color;
	case BSG_FIELD_MATERIAL_EMISSIVE_COLOR:
	    return &fields->material_emissive_color;
	case BSG_FIELD_MATERIAL_ALPHA:
	    return &fields->material_alpha;
	case BSG_FIELD_MATERIAL_SHININESS:
	    return &fields->material_shininess;
	case BSG_FIELD_MATERIAL_LINE_COLOR:
	    return &fields->material_line_color;
	case BSG_FIELD_DRAW_LINE_WIDTH:
	    return &fields->draw_line_width;
	case BSG_FIELD_DRAW_LINE_STYLE:
	    return &fields->draw_line_style;
	case BSG_FIELD_DRAW_FILL_MODE:
	    return &fields->draw_fill_mode;
	case BSG_FIELD_DRAW_POINT_SIZE:
	    return &fields->draw_point_size;
	case BSG_FIELD_DRAW_TRANSPARENCY_POLICY:
	    return &fields->draw_transparency_policy;
	case BSG_FIELD_DRAW_ARROW_TIP_LENGTH:
	    return &fields->draw_arrow_tip_length;
	case BSG_FIELD_DRAW_ARROW_TIP_WIDTH:
	    return &fields->draw_arrow_tip_width;
	case BSG_FIELD_COMPLEXITY_VALUE:
	    return &fields->complexity_value;
	case BSG_FIELD_COMPLEXITY_CURVE_SCALE:
	    return &fields->complexity_curve_scale;
	case BSG_FIELD_COMPLEXITY_POINT_SCALE:
	    return &fields->complexity_point_scale;
	case BSG_FIELD_COMPLEXITY_LOD_POLICY:
	    return &fields->complexity_lod_policy;
	case BSG_FIELD_COMPLEXITY_LOD_LEVEL:
	    return &fields->complexity_lod_level;
	case BSG_FIELD_CAMERA_PROJECTION:
	    return &fields->camera_projection;
	case BSG_FIELD_CAMERA_PERSPECTIVE:
	    return &fields->camera_perspective;
	case BSG_FIELD_CAMERA_POSITION:
	    return &fields->camera_position;
	case BSG_FIELD_CAMERA_ORIENTATION:
	    return &fields->camera_orientation;
	case BSG_FIELD_CAMERA_NEAR_DISTANCE:
	    return &fields->camera_near_distance;
	case BSG_FIELD_CAMERA_FAR_DISTANCE:
	    return &fields->camera_far_distance;
	case BSG_FIELD_LIGHT_TYPE:
	    return &fields->light_type;
	case BSG_FIELD_LIGHT_POSITION:
	    return &fields->light_position;
	case BSG_FIELD_LIGHT_DIRECTION:
	    return &fields->light_direction;
	case BSG_FIELD_LIGHT_DIFFUSE_COLOR:
	    return &fields->light_diffuse_color;
	case BSG_FIELD_LIGHT_SPECULAR_COLOR:
	    return &fields->light_specular_color;
	case BSG_FIELD_LIGHT_AMBIENT_COLOR:
	    return &fields->light_ambient_color;
	case BSG_FIELD_LIGHT_INTENSITY:
	    return &fields->light_intensity;
	case BSG_FIELD_LIGHT_ACTIVE:
	    return &fields->light_active;
	case BSG_FIELD_GEOMETRY_KIND:
	    return &fields->geometry_kind;
	case BSG_FIELD_GEOMETRY_COORDINATES:
	    return &fields->geometry_coordinates;
	case BSG_FIELD_GEOMETRY_NORMALS:
	    return &fields->geometry_normals;
	case BSG_FIELD_GEOMETRY_COLORS:
	    return &fields->geometry_colors;
	case BSG_FIELD_GEOMETRY_INDICES:
	    return &fields->geometry_indices;
	case BSG_FIELD_GEOMETRY_PRIMITIVE_SETS:
	    return &fields->geometry_primitive_sets;
	case BSG_FIELD_GEOMETRY_TEXT:
	    return &fields->geometry_text;
	case BSG_FIELD_GEOMETRY_IMAGE_WIDTH:
	    return &fields->geometry_image_width;
	case BSG_FIELD_GEOMETRY_IMAGE_HEIGHT:
	    return &fields->geometry_image_height;
	case BSG_FIELD_GEOMETRY_IMAGE_CHANNELS:
	    return &fields->geometry_image_channels;
	case BSG_FIELD_GEOMETRY_SOURCE_REVISION:
	    return &fields->geometry_source_revision;
	case BSG_FIELD_LOD_SELECTION:
	    return &fields->lod_selection;
	case BSG_FIELD_LOD_ACTIVE_LEVEL:
	    return &fields->lod_active_level;
	case BSG_FIELD_LOD_RANGES:
	    return &fields->lod_ranges;
	case BSG_FIELD_DATABASE_SOURCE_PATH:
	    return &fields->database_source_path;
	case BSG_FIELD_DATABASE_SOURCE_DRAW_MODE:
	    return &fields->database_source_draw_mode;
	case BSG_FIELD_DATABASE_SOURCE_MATERIAL_POLICY:
	    return &fields->database_source_material_policy;
	case BSG_FIELD_DATABASE_SOURCE_REVISION:
	    return &fields->database_source_revision;
	case BSG_FIELD_DATABASE_SOURCE_INPUTS_REVISION:
	    return &fields->database_source_inputs_revision;
	case BSG_FIELD_DATABASE_SOURCE_REALIZED_REVISION:
	    return &fields->database_source_realized_revision;
	case BSG_FIELD_DATABASE_SOURCE_REALIZED_INPUTS_REVISION:
	    return &fields->database_source_realized_inputs_revision;
	case BSG_FIELD_DATABASE_SOURCE_STALE_REASON:
	    return &fields->database_source_stale_reason;
	case BSG_FIELD_DATABASE_SOURCE_REALIZATION_IDENTITY:
	    return &fields->database_source_realization_identity;
	case BSG_FIELD_DATABASE_SOURCE_REALIZATION_STATUS:
	    return &fields->database_source_realization_status;
	case BSG_FIELD_DATABASE_SOURCE_REALIZATION_ROLE_FLAGS:
	    return &fields->database_source_realization_role_flags;
	case BSG_FIELD_DATABASE_SOURCE_REALIZATION_VIEW_DEPENDENT:
	    return &fields->database_source_realization_view_dependent;
	case BSG_FIELD_DATABASE_SOURCE_REALIZATION_VIEW_SCALE:
	    return &fields->database_source_realization_view_scale;
	case BSG_FIELD_DATABASE_SOURCE_REALIZATION_BOT_THRESHOLD:
	    return &fields->database_source_realization_bot_threshold;
	case BSG_FIELD_DATABASE_SOURCE_REALIZATION_CURVE_SCALE:
	    return &fields->database_source_realization_curve_scale;
	case BSG_FIELD_DATABASE_SOURCE_REALIZATION_POINT_SCALE:
	    return &fields->database_source_realization_point_scale;
	default:
	    return NULL;
    }
}

bsg_field_ref
bsg_node_field_ref(bsg_node *node, bsg_field_id_t fid)
{
    return _field_ref(_node_field(node, fid));
}

static struct bsg_field *
_field_source(struct bsg_field *field)
{
    int depth = 0;
    while (field && field->connection && depth++ < 64)
	field = field->connection;
    return field;
}

static void
_field_sync_to_legacy(struct bsg_field *field)
{
    bsg_node *node;
    if (!field || !field->owner)
	return;

    node = field->owner;
    switch (field->id) {
	case BSG_FIELD_COLOR:
	    break;
	case BSG_FIELD_MATERIAL_DIFFUSE_COLOR:
	    bsg_node_internal_ensure(node)->material_revision++;
	    break;
	case BSG_FIELD_MATERIAL_LINE_COLOR:
	    bsg_node_internal_ensure(node)->material_revision++;
	    break;
	case BSG_FIELD_BOUNDS:
	    if (field->value.bbox_value.valid)
		bsg_node_bbox_cache_set(node, field->value.bbox_value.min,
			field->value.bbox_value.max);
	    else
		bsg_node_bbox_cache_clear(node);
	    break;
	default:
	    break;
    }
}

static void
_field_touch(struct bsg_field *field)
{
    if (!field || !field->owner)
	return;
    field->revision++;
    _field_sync_to_legacy(field);
    bsg_node_touch(field->owner);
    bsg_sensor_notify_field_ref(_field_ref(field));
}

static void
_field_touch_if_unset(struct bsg_field *field)
{
    if (field && field->revision == 0)
	_field_touch(field);
}

void
bsg_node_field_touch(bsg_node *node, bsg_field_id_t fid)
{
    struct bsg_field *field = _node_field(node, fid);
    if (!field || !node)
	return;
    _field_touch(field);
}

bsg_field_ref
bsg_field_ref_null(void)
{
    bsg_field_ref ref = BSG_FIELD_REF_NULL_INIT;
    return ref;
}

int
bsg_field_ref_is_null(bsg_field_ref ref)
{
    return ref.opaque ? 0 : 1;
}

int
bsg_field_ref_equal(bsg_field_ref a, bsg_field_ref b)
{
    return a.opaque == b.opaque;
}

bsg_node_ref
bsg_field_owner(bsg_field_ref ref)
{
    struct bsg_field *field = _field_from_ref(ref);
    return _node_ref_from_node(field ? field->owner : NULL);
}

const char *
bsg_field_name(bsg_field_ref ref)
{
    struct bsg_field *field = _field_from_ref(ref);
    return field ? field->name : NULL;
}

bsg_field_value_type
bsg_field_ref_value_type(bsg_field_ref ref)
{
    struct bsg_field *field = _field_from_ref(ref);
    return field ? field->value_type : BSG_FIELD_VALUE_INVALID;
}

uint64_t
bsg_field_revision(bsg_field_ref ref)
{
    struct bsg_field *field = _field_from_ref(ref);
    return field ? field->revision : 0;
}

int
bsg_field_touch(bsg_field_ref ref)
{
    struct bsg_field *field = _field_from_ref(ref);
    if (!field)
	return 0;
    _field_touch(field);
    return 1;
}

int
bsg_field_connect(bsg_field_ref dst_ref, bsg_field_ref src_ref)
{
    struct bsg_field *dst = _field_from_ref(dst_ref);
    struct bsg_field *src = _field_from_ref(src_ref);
    if (!dst || !src || dst == src || dst->value_type != src->value_type)
	return 0;
    dst->connection = src;
    _field_touch(dst);
    return 1;
}

int
bsg_field_disconnect(bsg_field_ref dst_ref)
{
    struct bsg_field *dst = _field_from_ref(dst_ref);
    if (!dst || !dst->connection)
	return 0;
    dst->connection = NULL;
    _field_touch(dst);
    return 1;
}

bsg_field_ref
bsg_field_connection(bsg_field_ref ref)
{
    struct bsg_field *field = _field_from_ref(ref);
    return _field_ref(field ? field->connection : NULL);
}

bsg_field_ref
bsg_node_ref_field(bsg_node_ref node_ref, const char *name)
{
    bsg_node *node = _node_from_ref(node_ref);
    if (!node || !name)
	return bsg_field_ref_null();

    if (BU_STR_EQUAL(name, "name"))
	return bsg_node_field_ref(node, BSG_FIELD_NAME);
    if (BU_STR_EQUAL(name, "visibility") || BU_STR_EQUAL(name, "visible"))
	return bsg_node_field_ref(node, BSG_FIELD_VISIBILITY);
    if (BU_STR_EQUAL(name, "color"))
	return bsg_node_field_ref(node, BSG_FIELD_COLOR);
    if (BU_STR_EQUAL(name, "transform"))
	return bsg_node_field_ref(node, BSG_FIELD_TRANSFORM);
    if (BU_STR_EQUAL(name, "bounds"))
	return bsg_node_field_ref(node, BSG_FIELD_BOUNDS);
    if (BU_STR_EQUAL(name, "whichChild"))
	return bsg_node_field_ref(node, BSG_FIELD_SWITCH_WHICH);
    if (BU_STR_EQUAL(name, "children"))
	return bsg_node_field_ref(node, BSG_FIELD_CHILDREN);
    if (BU_STR_EQUAL(name, "diffuseColor"))
	return bsg_node_field_ref(node, BSG_FIELD_MATERIAL_DIFFUSE_COLOR);
    if (BU_STR_EQUAL(name, "specularColor"))
	return bsg_node_field_ref(node, BSG_FIELD_MATERIAL_SPECULAR_COLOR);
    if (BU_STR_EQUAL(name, "emissiveColor"))
	return bsg_node_field_ref(node, BSG_FIELD_MATERIAL_EMISSIVE_COLOR);
    if (BU_STR_EQUAL(name, "alpha"))
	return bsg_node_field_ref(node, BSG_FIELD_MATERIAL_ALPHA);
    if (BU_STR_EQUAL(name, "shininess"))
	return bsg_node_field_ref(node, BSG_FIELD_MATERIAL_SHININESS);
    if (BU_STR_EQUAL(name, "lineColor"))
	return bsg_node_field_ref(node, BSG_FIELD_MATERIAL_LINE_COLOR);
    if (BU_STR_EQUAL(name, "lineWidth"))
	return bsg_node_field_ref(node, BSG_FIELD_DRAW_LINE_WIDTH);
    if (BU_STR_EQUAL(name, "lineStyle"))
	return bsg_node_field_ref(node, BSG_FIELD_DRAW_LINE_STYLE);
    if (BU_STR_EQUAL(name, "fillMode"))
	return bsg_node_field_ref(node, BSG_FIELD_DRAW_FILL_MODE);
    if (BU_STR_EQUAL(name, "pointSize"))
	return bsg_node_field_ref(node, BSG_FIELD_DRAW_POINT_SIZE);
    if (BU_STR_EQUAL(name, "transparencyPolicy"))
	return bsg_node_field_ref(node, BSG_FIELD_DRAW_TRANSPARENCY_POLICY);
    if (BU_STR_EQUAL(name, "arrowTipLength"))
	return bsg_node_field_ref(node, BSG_FIELD_DRAW_ARROW_TIP_LENGTH);
    if (BU_STR_EQUAL(name, "arrowTipWidth"))
	return bsg_node_field_ref(node, BSG_FIELD_DRAW_ARROW_TIP_WIDTH);
    if (BU_STR_EQUAL(name, "complexity"))
	return bsg_node_field_ref(node, BSG_FIELD_COMPLEXITY_VALUE);
    if (BU_STR_EQUAL(name, "curveScale"))
	return bsg_node_field_ref(node, BSG_FIELD_COMPLEXITY_CURVE_SCALE);
    if (BU_STR_EQUAL(name, "pointScale"))
	return bsg_node_field_ref(node, BSG_FIELD_COMPLEXITY_POINT_SCALE);
    if (BU_STR_EQUAL(name, "lodPolicy"))
	return bsg_node_field_ref(node, BSG_FIELD_COMPLEXITY_LOD_POLICY);
    if (BU_STR_EQUAL(name, "lodLevel"))
	return bsg_node_field_ref(node, BSG_FIELD_COMPLEXITY_LOD_LEVEL);
    if (BU_STR_EQUAL(name, "projection"))
	return bsg_node_field_ref(node, BSG_FIELD_CAMERA_PROJECTION);
    if (BU_STR_EQUAL(name, "perspective"))
	return bsg_node_field_ref(node, BSG_FIELD_CAMERA_PERSPECTIVE);
    if (BU_STR_EQUAL(name, "position"))
	return bsg_node_field_ref(node, BSG_FIELD_CAMERA_POSITION);
    if (BU_STR_EQUAL(name, "orientation"))
	return bsg_node_field_ref(node, BSG_FIELD_CAMERA_ORIENTATION);
    if (BU_STR_EQUAL(name, "nearDistance"))
	return bsg_node_field_ref(node, BSG_FIELD_CAMERA_NEAR_DISTANCE);
    if (BU_STR_EQUAL(name, "farDistance"))
	return bsg_node_field_ref(node, BSG_FIELD_CAMERA_FAR_DISTANCE);
    if (BU_STR_EQUAL(name, "lightType"))
	return bsg_node_field_ref(node, BSG_FIELD_LIGHT_TYPE);
    if (BU_STR_EQUAL(name, "lightPosition"))
	return bsg_node_field_ref(node, BSG_FIELD_LIGHT_POSITION);
    if (BU_STR_EQUAL(name, "lightDirection"))
	return bsg_node_field_ref(node, BSG_FIELD_LIGHT_DIRECTION);
    if (BU_STR_EQUAL(name, "lightDiffuseColor"))
	return bsg_node_field_ref(node, BSG_FIELD_LIGHT_DIFFUSE_COLOR);
    if (BU_STR_EQUAL(name, "lightSpecularColor"))
	return bsg_node_field_ref(node, BSG_FIELD_LIGHT_SPECULAR_COLOR);
    if (BU_STR_EQUAL(name, "lightAmbientColor"))
	return bsg_node_field_ref(node, BSG_FIELD_LIGHT_AMBIENT_COLOR);
    if (BU_STR_EQUAL(name, "lightIntensity"))
	return bsg_node_field_ref(node, BSG_FIELD_LIGHT_INTENSITY);
    if (BU_STR_EQUAL(name, "lightActive"))
	return bsg_node_field_ref(node, BSG_FIELD_LIGHT_ACTIVE);
    if (BU_STR_EQUAL(name, "geometryKind"))
	return bsg_node_field_ref(node, BSG_FIELD_GEOMETRY_KIND);
    if (BU_STR_EQUAL(name, "coordinates"))
	return bsg_node_field_ref(node, BSG_FIELD_GEOMETRY_COORDINATES);
    if (BU_STR_EQUAL(name, "normals"))
	return bsg_node_field_ref(node, BSG_FIELD_GEOMETRY_NORMALS);
    if (BU_STR_EQUAL(name, "colors"))
	return bsg_node_field_ref(node, BSG_FIELD_GEOMETRY_COLORS);
    if (BU_STR_EQUAL(name, "indices"))
	return bsg_node_field_ref(node, BSG_FIELD_GEOMETRY_INDICES);
    if (BU_STR_EQUAL(name, "primitiveSets"))
	return bsg_node_field_ref(node, BSG_FIELD_GEOMETRY_PRIMITIVE_SETS);
    if (BU_STR_EQUAL(name, "text"))
	return bsg_node_field_ref(node, BSG_FIELD_GEOMETRY_TEXT);
    if (BU_STR_EQUAL(name, "imageWidth"))
	return bsg_node_field_ref(node, BSG_FIELD_GEOMETRY_IMAGE_WIDTH);
    if (BU_STR_EQUAL(name, "imageHeight"))
	return bsg_node_field_ref(node, BSG_FIELD_GEOMETRY_IMAGE_HEIGHT);
    if (BU_STR_EQUAL(name, "imageChannels"))
	return bsg_node_field_ref(node, BSG_FIELD_GEOMETRY_IMAGE_CHANNELS);
    if (BU_STR_EQUAL(name, "geometryRevision"))
	return bsg_node_field_ref(node, BSG_FIELD_GEOMETRY_SOURCE_REVISION);
    if (BU_STR_EQUAL(name, "lodSelection"))
	return bsg_node_field_ref(node, BSG_FIELD_LOD_SELECTION);
    if (BU_STR_EQUAL(name, "activeLevel"))
	return bsg_node_field_ref(node, BSG_FIELD_LOD_ACTIVE_LEVEL);
    if (BU_STR_EQUAL(name, "ranges"))
	return bsg_node_field_ref(node, BSG_FIELD_LOD_RANGES);
    if (BU_STR_EQUAL(name, "databasePath"))
	return bsg_node_field_ref(node, BSG_FIELD_DATABASE_SOURCE_PATH);
    if (BU_STR_EQUAL(name, "sourceDrawMode"))
	return bsg_node_field_ref(node, BSG_FIELD_DATABASE_SOURCE_DRAW_MODE);
    if (BU_STR_EQUAL(name, "materialPolicy"))
	return bsg_node_field_ref(node, BSG_FIELD_DATABASE_SOURCE_MATERIAL_POLICY);
    if (BU_STR_EQUAL(name, "sourceRevision"))
	return bsg_node_field_ref(node, BSG_FIELD_DATABASE_SOURCE_REVISION);
    if (BU_STR_EQUAL(name, "inputsRevision"))
	return bsg_node_field_ref(node, BSG_FIELD_DATABASE_SOURCE_INPUTS_REVISION);
    if (BU_STR_EQUAL(name, "realizedSourceRevision"))
	return bsg_node_field_ref(node, BSG_FIELD_DATABASE_SOURCE_REALIZED_REVISION);
    if (BU_STR_EQUAL(name, "realizedInputsRevision"))
	return bsg_node_field_ref(node, BSG_FIELD_DATABASE_SOURCE_REALIZED_INPUTS_REVISION);
    if (BU_STR_EQUAL(name, "staleReason"))
	return bsg_node_field_ref(node, BSG_FIELD_DATABASE_SOURCE_STALE_REASON);
    if (BU_STR_EQUAL(name, "realizationIdentity"))
	return bsg_node_field_ref(node, BSG_FIELD_DATABASE_SOURCE_REALIZATION_IDENTITY);
    if (BU_STR_EQUAL(name, "realizationStatus"))
	return bsg_node_field_ref(node, BSG_FIELD_DATABASE_SOURCE_REALIZATION_STATUS);
    if (BU_STR_EQUAL(name, "realizationRoleFlags"))
	return bsg_node_field_ref(node, BSG_FIELD_DATABASE_SOURCE_REALIZATION_ROLE_FLAGS);
    if (BU_STR_EQUAL(name, "realizationViewDependent"))
	return bsg_node_field_ref(node, BSG_FIELD_DATABASE_SOURCE_REALIZATION_VIEW_DEPENDENT);
    if (BU_STR_EQUAL(name, "realizationViewScale"))
	return bsg_node_field_ref(node, BSG_FIELD_DATABASE_SOURCE_REALIZATION_VIEW_SCALE);
    if (BU_STR_EQUAL(name, "realizationBotThreshold"))
	return bsg_node_field_ref(node, BSG_FIELD_DATABASE_SOURCE_REALIZATION_BOT_THRESHOLD);
    if (BU_STR_EQUAL(name, "realizationCurveScale"))
	return bsg_node_field_ref(node, BSG_FIELD_DATABASE_SOURCE_REALIZATION_CURVE_SCALE);
    if (BU_STR_EQUAL(name, "realizationPointScale"))
	return bsg_node_field_ref(node, BSG_FIELD_DATABASE_SOURCE_REALIZATION_POINT_SCALE);

    return bsg_field_ref_null();
}

bsg_field_ref
bsg_node_ref_name_field(bsg_node_ref node)
{
    return bsg_node_field_ref(_node_from_ref(node), BSG_FIELD_NAME);
}

bsg_field_ref
bsg_node_ref_visibility_field(bsg_node_ref node)
{
    return bsg_node_field_ref(_node_from_ref(node), BSG_FIELD_VISIBILITY);
}

bsg_field_ref
bsg_node_ref_color_field(bsg_node_ref node)
{
    return bsg_node_field_ref(_node_from_ref(node), BSG_FIELD_COLOR);
}

bsg_field_ref
bsg_node_ref_transform_field(bsg_node_ref node)
{
    return bsg_node_field_ref(_node_from_ref(node), BSG_FIELD_TRANSFORM);
}

bsg_field_ref
bsg_node_ref_bounds_field(bsg_node_ref node)
{
    return bsg_node_field_ref(_node_from_ref(node), BSG_FIELD_BOUNDS);
}

bsg_field_ref
bsg_group_ref_children_field(bsg_group_ref group)
{
    if (!bsg_type_is_a(bsg_object_type(group.node.object), bsg_group_type()))
	return bsg_field_ref_null();
    return bsg_node_field_ref(_node_from_ref(group.node), BSG_FIELD_CHILDREN);
}

bsg_field_ref
bsg_switch_ref_which_child_field(bsg_switch_ref sw)
{
    if (!bsg_type_is_a(bsg_object_type(sw.group.node.object), bsg_switch_type()))
	return bsg_field_ref_null();
    return bsg_node_field_ref(_node_from_ref(sw.group.node), BSG_FIELD_SWITCH_WHICH);
}

int
bsg_field_get_bool(bsg_field_ref ref, int *value)
{
    struct bsg_field *field = _field_source(_field_from_ref(ref));
    if (!field || field->value_type != BSG_FIELD_VALUE_BOOL || !value)
	return 0;
    *value = field->value.bool_value ? 1 : 0;
    return 1;
}

int
bsg_field_set_bool(bsg_field_ref ref, int value)
{
    struct bsg_field *field = _field_from_ref(ref);
    if (!field || field->value_type != BSG_FIELD_VALUE_BOOL)
	return 0;
    if (field->value.bool_value == (value ? 1 : 0)) {
	_field_touch_if_unset(field);
	return 1;
    }
    field->value.bool_value = value ? 1 : 0;
    _field_touch(field);
    return 1;
}

int
bsg_field_get_int(bsg_field_ref ref, int *value)
{
    struct bsg_field *field = _field_source(_field_from_ref(ref));
    if (!field || field->value_type != BSG_FIELD_VALUE_INT || !value)
	return 0;
    *value = field->value.int_value;
    return 1;
}

int
bsg_field_set_int(bsg_field_ref ref, int value)
{
    struct bsg_field *field = _field_from_ref(ref);
    if (!field || field->value_type != BSG_FIELD_VALUE_INT)
	return 0;
    if (field->value.int_value == value) {
	_field_touch_if_unset(field);
	return 1;
    }
    field->value.int_value = value;
    _field_touch(field);
    return 1;
}

int
bsg_field_get_uint64(bsg_field_ref ref, uint64_t *value)
{
    struct bsg_field *field = _field_source(_field_from_ref(ref));
    if (!field || field->value_type != BSG_FIELD_VALUE_UINT64 || !value)
	return 0;
    *value = field->value.uint64_value;
    return 1;
}

int
bsg_field_set_uint64(bsg_field_ref ref, uint64_t value)
{
    struct bsg_field *field = _field_from_ref(ref);
    if (!field || field->value_type != BSG_FIELD_VALUE_UINT64)
	return 0;
    if (field->value.uint64_value == value) {
	_field_touch_if_unset(field);
	return 1;
    }
    field->value.uint64_value = value;
    _field_touch(field);
    return 1;
}

int
bsg_field_get_float(bsg_field_ref ref, float *value)
{
    struct bsg_field *field = _field_source(_field_from_ref(ref));
    if (!field || field->value_type != BSG_FIELD_VALUE_FLOAT || !value)
	return 0;
    *value = field->value.float_value;
    return 1;
}

int
bsg_field_set_float(bsg_field_ref ref, float value)
{
    struct bsg_field *field = _field_from_ref(ref);
    if (!field || field->value_type != BSG_FIELD_VALUE_FLOAT)
	return 0;
    if (NEAR_ZERO((fastf_t)field->value.float_value - (fastf_t)value, SMALL_FASTF)) {
	_field_touch_if_unset(field);
	return 1;
    }
    field->value.float_value = value;
    _field_touch(field);
    return 1;
}

int
bsg_field_get_double(bsg_field_ref ref, double *value)
{
    struct bsg_field *field = _field_source(_field_from_ref(ref));
    if (!field || field->value_type != BSG_FIELD_VALUE_DOUBLE || !value)
	return 0;
    *value = field->value.double_value;
    return 1;
}

int
bsg_field_set_double(bsg_field_ref ref, double value)
{
    struct bsg_field *field = _field_from_ref(ref);
    if (!field || field->value_type != BSG_FIELD_VALUE_DOUBLE)
	return 0;
    if (NEAR_ZERO((fastf_t)field->value.double_value - (fastf_t)value, SMALL_FASTF)) {
	_field_touch_if_unset(field);
	return 1;
    }
    field->value.double_value = value;
    _field_touch(field);
    return 1;
}

int
bsg_field_get_enum(bsg_field_ref ref, int *value)
{
    struct bsg_field *field = _field_source(_field_from_ref(ref));
    if (!field || field->value_type != BSG_FIELD_VALUE_ENUM || !value)
	return 0;
    *value = field->value.enum_value;
    return 1;
}

int
bsg_field_set_enum(bsg_field_ref ref, int value)
{
    struct bsg_field *field = _field_from_ref(ref);
    if (!field || field->value_type != BSG_FIELD_VALUE_ENUM)
	return 0;
    if (field->value.enum_value == value) {
	_field_touch_if_unset(field);
	return 1;
    }
    field->value.enum_value = value;
    _field_touch(field);
    return 1;
}

int
bsg_field_get_string(bsg_field_ref ref, const char **value)
{
    struct bsg_field *field = _field_source(_field_from_ref(ref));
    if (!field || field->value_type != BSG_FIELD_VALUE_STRING || !value)
	return 0;
    *value = field->value.string_value ? field->value.string_value : "";
    return 1;
}

int
bsg_field_set_string(bsg_field_ref ref, const char *value)
{
    struct bsg_field *field = _field_from_ref(ref);
    if (!field || field->value_type != BSG_FIELD_VALUE_STRING)
	return 0;
    if (BU_STR_EQUAL(field->value.string_value ? field->value.string_value : "", value ? value : "")) {
	_field_touch_if_unset(field);
	return 1;
    }
    _field_string_set(field, value);
    _field_touch(field);
    return 1;
}

int
bsg_field_get_vec3(bsg_field_ref ref, vect_t value)
{
    struct bsg_field *field = _field_source(_field_from_ref(ref));
    if (!field || field->value_type != BSG_FIELD_VALUE_VEC3 || !value)
	return 0;
    VMOVE(value, field->value.vec3_value);
    return 1;
}

int
bsg_field_set_vec3(bsg_field_ref ref, const vect_t value)
{
    struct bsg_field *field = _field_from_ref(ref);
    if (!field || field->value_type != BSG_FIELD_VALUE_VEC3 || !value)
	return 0;
    if (VNEAR_EQUAL(field->value.vec3_value, value, SMALL_FASTF)) {
	_field_touch_if_unset(field);
	return 1;
    }
    VMOVE(field->value.vec3_value, value);
    _field_touch(field);
    return 1;
}

int
bsg_field_get_color3(bsg_field_ref ref, unsigned char color[3])
{
    struct bsg_field *field = _field_source(_field_from_ref(ref));
    if (!field || field->value_type != BSG_FIELD_VALUE_COLOR3 || !color)
	return 0;
    color[0] = field->value.color3_value[0];
    color[1] = field->value.color3_value[1];
    color[2] = field->value.color3_value[2];
    return 1;
}

int
bsg_field_set_color3(bsg_field_ref ref, const unsigned char color[3])
{
    struct bsg_field *field = _field_from_ref(ref);
    if (!field || field->value_type != BSG_FIELD_VALUE_COLOR3 || !color)
	return 0;
    if (field->value.color3_value[0] == color[0] &&
	    field->value.color3_value[1] == color[1] &&
	    field->value.color3_value[2] == color[2]) {
	_field_touch_if_unset(field);
	return 1;
    }
    field->value.color3_value[0] = color[0];
    field->value.color3_value[1] = color[1];
    field->value.color3_value[2] = color[2];
    _field_touch(field);
    return 1;
}

int
bsg_field_get_matrix(bsg_field_ref ref, mat_t value)
{
    struct bsg_field *field = _field_source(_field_from_ref(ref));
    if (!field || field->value_type != BSG_FIELD_VALUE_MATRIX || !value)
	return 0;
    MAT_COPY(value, field->value.matrix_value);
    return 1;
}

int
bsg_field_set_matrix(bsg_field_ref ref, const mat_t value)
{
    struct bsg_field *field = _field_from_ref(ref);
    if (!field || field->value_type != BSG_FIELD_VALUE_MATRIX || !value)
	return 0;
    if (memcmp(field->value.matrix_value, value, sizeof(mat_t)) == 0) {
	_field_touch_if_unset(field);
	return 1;
    }
    MAT_COPY(field->value.matrix_value, value);
    _field_touch(field);
    return 1;
}

int
bsg_field_get_bbox(bsg_field_ref ref, point_t bmin, point_t bmax, int *valid)
{
    struct bsg_field *field = _field_source(_field_from_ref(ref));
    if (!field || field->value_type != BSG_FIELD_VALUE_BBOX)
	return 0;
    if (valid)
	*valid = field->value.bbox_value.valid ? 1 : 0;
    if (bmin)
	VMOVE(bmin, field->value.bbox_value.min);
    if (bmax)
	VMOVE(bmax, field->value.bbox_value.max);
    return 1;
}

int
bsg_field_set_bbox(bsg_field_ref ref, const point_t bmin, const point_t bmax, int valid)
{
    struct bsg_field *field = _field_from_ref(ref);
    if (!field || field->value_type != BSG_FIELD_VALUE_BBOX)
	return 0;
    if (field->value.bbox_value.valid == (valid ? 1 : 0) &&
	    (!valid || ((bmin && bmax) &&
	    VNEAR_EQUAL(field->value.bbox_value.min, bmin, SMALL_FASTF) &&
	    VNEAR_EQUAL(field->value.bbox_value.max, bmax, SMALL_FASTF)))) {
	_field_touch_if_unset(field);
	return 1;
    }
    field->value.bbox_value.valid = valid ? 1 : 0;
    if (valid && bmin && bmax) {
	VMOVE(field->value.bbox_value.min, bmin);
	VMOVE(field->value.bbox_value.max, bmax);
    }
    _field_touch(field);
    return 1;
}

int
bsg_field_get_object(bsg_field_ref ref, bsg_object_ref *value)
{
    struct bsg_field *field = _field_source(_field_from_ref(ref));
    if (!field || field->value_type != BSG_FIELD_VALUE_OBJECT || !value)
	return 0;
    *value = field->value.object_value;
    return 1;
}

int
bsg_field_set_object(bsg_field_ref ref, bsg_object_ref value)
{
    struct bsg_field *field = _field_from_ref(ref);
    if (!field || field->value_type != BSG_FIELD_VALUE_OBJECT)
	return 0;
    if (bsg_object_ref_equal(field->value.object_value, value)) {
	_field_touch_if_unset(field);
	return 1;
    }
    field->value.object_value = value;
    _field_touch(field);
    return 1;
}

int
bsg_field_get_node(bsg_field_ref ref, bsg_node_ref *value)
{
    struct bsg_field *field = _field_source(_field_from_ref(ref));
    if (!field || field->value_type != BSG_FIELD_VALUE_NODE || !value)
	return 0;
    *value = field->value.node_value;
    return 1;
}

int
bsg_field_set_node(bsg_field_ref ref, bsg_node_ref value)
{
    struct bsg_field *field = _field_from_ref(ref);
    if (!field || field->value_type != BSG_FIELD_VALUE_NODE)
	return 0;
    if (bsg_node_ref_equal(field->value.node_value, value)) {
	_field_touch_if_unset(field);
	return 1;
    }
    field->value.node_value = value;
    _field_touch(field);
    return 1;
}

size_t
bsg_field_multi_count(bsg_field_ref ref)
{
    struct bsg_field *field = _field_source(_field_from_ref(ref));
    if (!field)
	return 0;
    if (field->value_type == BSG_FIELD_VALUE_MULTI_NODE && field->owner)
	return bsg_node_child_count(field->owner);
    if (field->value_type == BSG_FIELD_VALUE_MULTI_DOUBLE)
	return field->value.multi_double_value.count;
    if (field->value_type == BSG_FIELD_VALUE_MULTI_POINT)
	return field->value.multi_point_value.count;
    if (field->value_type == BSG_FIELD_VALUE_MULTI_INT)
	return field->value.multi_int_value.count;
    if (field->value_type == BSG_FIELD_VALUE_MULTI_COLOR3)
	return field->value.multi_color3_value.count;
    return 0;
}

int
bsg_field_multi_clear(bsg_field_ref ref)
{
    struct bsg_field *field = _field_from_ref(ref);
    if (!field)
	return 0;

    switch (field->value_type) {
	case BSG_FIELD_VALUE_MULTI_NODE:
	    if (!field->owner)
		return 0;
	    while (bsg_node_child_count(field->owner) > 0)
		bsg_node_remove_child(field->owner, bsg_node_child_at(field->owner, 0));
	    _field_touch(field);
	    return 1;
	case BSG_FIELD_VALUE_MULTI_DOUBLE:
	    if (field->value.multi_double_value.values)
		bu_free(field->value.multi_double_value.values, "bsg field multi double");
	    field->value.multi_double_value.values = NULL;
	    field->value.multi_double_value.count = 0;
	    _field_touch(field);
	    return 1;
	case BSG_FIELD_VALUE_MULTI_POINT:
	    if (field->value.multi_point_value.values)
		bu_free(field->value.multi_point_value.values, "bsg field multi point");
	    field->value.multi_point_value.values = NULL;
	    field->value.multi_point_value.count = 0;
	    _field_touch(field);
	    return 1;
	case BSG_FIELD_VALUE_MULTI_INT:
	    if (field->value.multi_int_value.values)
		bu_free(field->value.multi_int_value.values, "bsg field multi int");
	    field->value.multi_int_value.values = NULL;
	    field->value.multi_int_value.count = 0;
	    _field_touch(field);
	    return 1;
	case BSG_FIELD_VALUE_MULTI_COLOR3:
	    if (field->value.multi_color3_value.values)
		bu_free(field->value.multi_color3_value.values, "bsg field multi color");
	    field->value.multi_color3_value.values = NULL;
	    field->value.multi_color3_value.count = 0;
	    _field_touch(field);
	    return 1;
	default:
	    return 0;
    }
}

int
bsg_field_multi_node_at(bsg_field_ref ref, size_t idx, bsg_node_ref *value)
{
    struct bsg_field *field = _field_source(_field_from_ref(ref));
    if (!field || field->value_type != BSG_FIELD_VALUE_MULTI_NODE || !field->owner || !value)
	return 0;
    *value = _node_ref_from_node(bsg_node_child_at(field->owner, idx));
    return bsg_node_ref_is_null(*value) ? 0 : 1;
}

int
bsg_field_multi_node_append(bsg_field_ref ref, bsg_node_ref value)
{
    struct bsg_field *field = _field_from_ref(ref);
    bsg_node *child = _node_from_ref(value);
    if (!field || field->value_type != BSG_FIELD_VALUE_MULTI_NODE || !field->owner || !child)
	return 0;
    bsg_node_add_child(field->owner, child);
    return 1;
}

int
bsg_field_multi_node_remove(bsg_field_ref ref, bsg_node_ref value)
{
    struct bsg_field *field = _field_from_ref(ref);
    bsg_node *child = _node_from_ref(value);
    if (!field || field->value_type != BSG_FIELD_VALUE_MULTI_NODE || !field->owner || !child)
	return 0;
    bsg_node_remove_child(field->owner, child);
    return 1;
}

static int
_multi_double_ensure(struct bsg_field *field, size_t count)
{
    double *values;
    if (!field || field->value_type != BSG_FIELD_VALUE_MULTI_DOUBLE)
	return 0;
    if (count <= field->value.multi_double_value.count)
	return 1;
    values = field->value.multi_double_value.values ?
	(double *)bu_realloc(field->value.multi_double_value.values, count * sizeof(double), "bsg field multi double") :
	(double *)bu_calloc(count, sizeof(double), "bsg field multi double");
    if (!values)
	return 0;
    for (size_t i = field->value.multi_double_value.count; i < count; i++)
	values[i] = 0.0;
    field->value.multi_double_value.values = values;
    field->value.multi_double_value.count = count;
    return 1;
}

int
bsg_field_multi_double_at(bsg_field_ref ref, size_t idx, double *value)
{
    struct bsg_field *field = _field_source(_field_from_ref(ref));
    if (!field || field->value_type != BSG_FIELD_VALUE_MULTI_DOUBLE || !value ||
	    idx >= field->value.multi_double_value.count)
	return 0;
    *value = field->value.multi_double_value.values[idx];
    return 1;
}

int
bsg_field_multi_double_set(bsg_field_ref ref, size_t idx, double value)
{
    struct bsg_field *field = _field_from_ref(ref);
    if (!_multi_double_ensure(field, idx + 1))
	return 0;
    if (NEAR_ZERO((fastf_t)field->value.multi_double_value.values[idx] - (fastf_t)value, SMALL_FASTF))
	return 1;
    field->value.multi_double_value.values[idx] = value;
    _field_touch(field);
    return 1;
}

int
bsg_field_multi_double_append(bsg_field_ref ref, double value)
{
    return bsg_field_multi_double_set(ref, bsg_field_multi_count(ref), value);
}

static int
_multi_point_ensure(struct bsg_field *field, size_t count)
{
    point_t *values;
    if (!field || field->value_type != BSG_FIELD_VALUE_MULTI_POINT)
	return 0;
    if (count <= field->value.multi_point_value.count)
	return 1;
    values = field->value.multi_point_value.values ?
	(point_t *)bu_realloc(field->value.multi_point_value.values, count * sizeof(point_t), "bsg field multi point") :
	(point_t *)bu_calloc(count, sizeof(point_t), "bsg field multi point");
    if (!values)
	return 0;
    for (size_t i = field->value.multi_point_value.count; i < count; i++)
	VSETALL(values[i], 0.0);
    field->value.multi_point_value.values = values;
    field->value.multi_point_value.count = count;
    return 1;
}

int
bsg_field_multi_point_at(bsg_field_ref ref, size_t idx, point_t value)
{
    struct bsg_field *field = _field_source(_field_from_ref(ref));
    if (!field || field->value_type != BSG_FIELD_VALUE_MULTI_POINT || !value ||
	    idx >= field->value.multi_point_value.count)
	return 0;
    VMOVE(value, field->value.multi_point_value.values[idx]);
    return 1;
}

int
bsg_field_multi_point_set(bsg_field_ref ref, size_t idx, const point_t value)
{
    struct bsg_field *field = _field_from_ref(ref);
    if (!_multi_point_ensure(field, idx + 1) || !value)
	return 0;
    if (VNEAR_EQUAL(field->value.multi_point_value.values[idx], value, SMALL_FASTF))
	return 1;
    VMOVE(field->value.multi_point_value.values[idx], value);
    _field_touch(field);
    return 1;
}

int
bsg_field_multi_point_append(bsg_field_ref ref, const point_t value)
{
    return bsg_field_multi_point_set(ref, bsg_field_multi_count(ref), value);
}

static int
_multi_int_ensure(struct bsg_field *field, size_t count)
{
    int *values;
    if (!field || field->value_type != BSG_FIELD_VALUE_MULTI_INT)
	return 0;
    if (count <= field->value.multi_int_value.count)
	return 1;
    values = field->value.multi_int_value.values ?
	(int *)bu_realloc(field->value.multi_int_value.values, count * sizeof(int), "bsg field multi int") :
	(int *)bu_calloc(count, sizeof(int), "bsg field multi int");
    if (!values)
	return 0;
    for (size_t i = field->value.multi_int_value.count; i < count; i++)
	values[i] = 0;
    field->value.multi_int_value.values = values;
    field->value.multi_int_value.count = count;
    return 1;
}

int
bsg_field_multi_int_at(bsg_field_ref ref, size_t idx, int *value)
{
    struct bsg_field *field = _field_source(_field_from_ref(ref));
    if (!field || field->value_type != BSG_FIELD_VALUE_MULTI_INT || !value ||
	    idx >= field->value.multi_int_value.count)
	return 0;
    *value = field->value.multi_int_value.values[idx];
    return 1;
}

int
bsg_field_multi_int_set(bsg_field_ref ref, size_t idx, int value)
{
    struct bsg_field *field = _field_from_ref(ref);
    if (!_multi_int_ensure(field, idx + 1))
	return 0;
    if (field->value.multi_int_value.values[idx] == value)
	return 1;
    field->value.multi_int_value.values[idx] = value;
    _field_touch(field);
    return 1;
}

int
bsg_field_multi_int_append(bsg_field_ref ref, int value)
{
    return bsg_field_multi_int_set(ref, bsg_field_multi_count(ref), value);
}

static int
_multi_color3_ensure(struct bsg_field *field, size_t count)
{
    unsigned char (*values)[3];
    if (!field || field->value_type != BSG_FIELD_VALUE_MULTI_COLOR3)
	return 0;
    if (count <= field->value.multi_color3_value.count)
	return 1;
    values = field->value.multi_color3_value.values ?
	(unsigned char (*)[3])bu_realloc(field->value.multi_color3_value.values, count * sizeof(unsigned char[3]), "bsg field multi color") :
	(unsigned char (*)[3])bu_calloc(count, sizeof(unsigned char[3]), "bsg field multi color");
    if (!values)
	return 0;
    for (size_t i = field->value.multi_color3_value.count; i < count; i++) {
	values[i][0] = 0;
	values[i][1] = 0;
	values[i][2] = 0;
    }
    field->value.multi_color3_value.values = values;
    field->value.multi_color3_value.count = count;
    return 1;
}

int
bsg_field_multi_color3_at(bsg_field_ref ref, size_t idx, unsigned char color[3])
{
    struct bsg_field *field = _field_source(_field_from_ref(ref));
    if (!field || field->value_type != BSG_FIELD_VALUE_MULTI_COLOR3 || !color ||
	    idx >= field->value.multi_color3_value.count)
	return 0;
    color[0] = field->value.multi_color3_value.values[idx][0];
    color[1] = field->value.multi_color3_value.values[idx][1];
    color[2] = field->value.multi_color3_value.values[idx][2];
    return 1;
}

int
bsg_field_multi_color3_set(bsg_field_ref ref, size_t idx, const unsigned char color[3])
{
    struct bsg_field *field = _field_from_ref(ref);
    if (!_multi_color3_ensure(field, idx + 1) || !color)
	return 0;
    if (field->value.multi_color3_value.values[idx][0] == color[0] &&
	    field->value.multi_color3_value.values[idx][1] == color[1] &&
	    field->value.multi_color3_value.values[idx][2] == color[2])
	return 1;
    field->value.multi_color3_value.values[idx][0] = color[0];
    field->value.multi_color3_value.values[idx][1] = color[1];
    field->value.multi_color3_value.values[idx][2] = color[2];
    _field_touch(field);
    return 1;
}

int
bsg_field_multi_color3_append(bsg_field_ref ref, const unsigned char color[3])
{
    return bsg_field_multi_color3_set(ref, bsg_field_multi_count(ref), color);
}

void
bsg_node_set_flag(bsg_node *n, int flag)
{
    (void)bsg_field_set_bool(bsg_node_field_ref(n, BSG_FIELD_VISIBILITY), flag ? 1 : 0);
}

int
bsg_node_get_flag(const bsg_node *n)
{
    int value = 0;
    (void)bsg_field_get_bool(bsg_node_field_ref((bsg_node *)n, BSG_FIELD_VISIBILITY), &value);
    return value ? UP : DOWN;
}

void
bsg_node_set_color(bsg_node *n, unsigned char r, unsigned char g, unsigned char b)
{
    unsigned char color[3];
    color[0] = r;
    color[1] = g;
    color[2] = b;
    (void)bsg_field_set_color3(bsg_node_field_ref(n, BSG_FIELD_COLOR), color);
}

void
bsg_node_get_color(const bsg_node *n, unsigned char *r, unsigned char *g, unsigned char *b)
{
    unsigned char color[3] = {0, 0, 0};
    if (!bsg_field_get_color3(bsg_node_field_ref((bsg_node *)n, BSG_FIELD_COLOR), color))
	return;
    if (r)
	*r = color[0];
    if (g)
	*g = color[1];
    if (b)
	*b = color[2];
}

void
bsg_node_set_visible(bsg_node *n, int on)
{
    (void)bsg_field_set_bool(bsg_node_field_ref(n, BSG_FIELD_VISIBILITY), on ? 1 : 0);
}

int
bsg_node_get_visible(const bsg_node *n)
{
    int visible = 0;
    (void)bsg_field_get_bool(bsg_node_field_ref((bsg_node *)n, BSG_FIELD_VISIBILITY), &visible);
    return visible ? 1 : 0;
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
