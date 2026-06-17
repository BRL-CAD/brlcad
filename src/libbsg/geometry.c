/*                    G E O M E T R Y . C
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 */
/** @file libbsg/geometry.c
 *
 * Field-backed typed geometry nodes.
 */

#include "common.h"

#include <limits.h>
#include <string.h>

#include "bu/malloc.h"
#include "bu/str.h"
#include "bu/vls.h"
#include "vmath.h"

#include "bsg/geometry.h"
#include "bsg/faceplate.h"
#include "payload_typed_private.h"
#include "field_private.h"
#include "geometry_private.h"
#include "node_private.h"
#include "node_storage_private.h"
#include "object_private.h"
#include "payload_private.h"

static bsg_type_id
_geometry_subtype(const char *name)
{
    return bsg_type_register(name, bsg_geometry_type());
}

bsg_type_id bsg_line_set_type(void) { return _geometry_subtype("BSGLineSet"); }
bsg_type_id bsg_indexed_line_set_type(void) { return _geometry_subtype("BSGIndexedLineSet"); }
bsg_type_id bsg_point_set_type(void) { return _geometry_subtype("BSGPointSet"); }
bsg_type_id bsg_indexed_face_set_type(void) { return _geometry_subtype("BSGIndexedFaceSet"); }
bsg_type_id bsg_mesh_type(void) { return _geometry_subtype("BSGMesh"); }
bsg_type_id bsg_text_type(void) { return _geometry_subtype("BSGText"); }
bsg_type_id bsg_image_type(void) { return _geometry_subtype("BSGImage"); }
bsg_type_id bsg_framebuffer_type(void) { return _geometry_subtype("BSGFramebuffer"); }
bsg_type_id bsg_overlay_geometry_type(void) { return _geometry_subtype("BSGOverlayGeometry"); }
bsg_type_id bsg_hud_geometry_type(void) { return _geometry_subtype("BSGHUDGeometry"); }
bsg_type_id bsg_annotation_type(void) { return _geometry_subtype("BSGAnnotation"); }
bsg_type_id bsg_csg_proxy_type(void) { return _geometry_subtype("BSGCSGProxy"); }
bsg_type_id bsg_brep_proxy_type(void) { return _geometry_subtype("BSGBRepProxy"); }
bsg_type_id bsg_edit_preview_type(void) { return _geometry_subtype("BSGEditPreview"); }

static bsg_node *
_geometry_node_from_ref(bsg_node_ref ref)
{
    return bsg_object_ref_node(ref.object);
}

static bsg_node_ref
_geometry_node_ref_from_node(bsg_node *node)
{
    bsg_node_ref ref = BSG_NODE_REF_NULL_INIT;
    ref.object = bsg_object_ref_from_node(node);
    return ref;
}

static bsg_geometry_ref
_geometry_ref_from_node(bsg_node *node)
{
    bsg_geometry_ref ref = BSG_GEOMETRY_REF_NULL_INIT;
    ref.shape.node = _geometry_node_ref_from_node(node);
    return ref;
}

static int
_geometry_ref_valid(bsg_geometry_ref ref)
{
    return bsg_node_is_a(ref.shape.node, bsg_geometry_type());
}

static int
_set_node_type_and_kind(bsg_node *node, bsg_type_id type, bsg_geometry_node_kind kind)
{
    if (!node)
	return 0;
    bsg_node_set_object_type(node, type);
    (void)bsg_field_set_enum(bsg_node_field_ref(node, BSG_FIELD_GEOMETRY_KIND), (int)kind);
    return 1;
}

static bsg_geometry_ref
_geometry_create(struct bsg_view *v, const char *name, bsg_type_id type, bsg_geometry_node_kind kind)
{
    bsg_geometry_ref ref = bsg_geometry_ref_create(v, name);
    bsg_node *node = _geometry_node_from_ref(ref.shape.node);
    (void)_set_node_type_and_kind(node, type, kind);
    return ref;
}

#define GEOMETRY_CREATE_FN(_ret, _init, _fn, _type_fn, _kind) \
_ret \
_fn(struct bsg_view *v, const char *name) \
{ \
    _ret ref = _init; \
    ref.geometry = _geometry_create(v, name, _type_fn(), _kind); \
    return ref; \
}

GEOMETRY_CREATE_FN(bsg_line_set_ref, BSG_LINE_SET_REF_NULL_INIT, bsg_line_set_ref_create, bsg_line_set_type, BSG_GEOMETRY_NODE_LINE_SET)
GEOMETRY_CREATE_FN(bsg_indexed_line_set_ref, BSG_INDEXED_LINE_SET_REF_NULL_INIT, bsg_indexed_line_set_ref_create, bsg_indexed_line_set_type, BSG_GEOMETRY_NODE_INDEXED_LINE_SET)
GEOMETRY_CREATE_FN(bsg_point_set_ref, BSG_POINT_SET_REF_NULL_INIT, bsg_point_set_ref_create, bsg_point_set_type, BSG_GEOMETRY_NODE_POINT_SET)
GEOMETRY_CREATE_FN(bsg_indexed_face_set_ref, BSG_INDEXED_FACE_SET_REF_NULL_INIT, bsg_indexed_face_set_ref_create, bsg_indexed_face_set_type, BSG_GEOMETRY_NODE_INDEXED_FACE_SET)
GEOMETRY_CREATE_FN(bsg_mesh_ref, BSG_MESH_REF_NULL_INIT, bsg_mesh_ref_create, bsg_mesh_type, BSG_GEOMETRY_NODE_MESH)
GEOMETRY_CREATE_FN(bsg_text_ref, BSG_TEXT_REF_NULL_INIT, bsg_text_ref_create, bsg_text_type, BSG_GEOMETRY_NODE_TEXT)
GEOMETRY_CREATE_FN(bsg_image_ref, BSG_IMAGE_REF_NULL_INIT, bsg_image_ref_create, bsg_image_type, BSG_GEOMETRY_NODE_IMAGE)
GEOMETRY_CREATE_FN(bsg_framebuffer_ref, BSG_FRAMEBUFFER_REF_NULL_INIT, bsg_framebuffer_ref_create, bsg_framebuffer_type, BSG_GEOMETRY_NODE_FRAMEBUFFER)
GEOMETRY_CREATE_FN(bsg_overlay_geometry_ref, BSG_OVERLAY_GEOMETRY_REF_NULL_INIT, bsg_overlay_geometry_ref_create, bsg_overlay_geometry_type, BSG_GEOMETRY_NODE_OVERLAY)
GEOMETRY_CREATE_FN(bsg_hud_geometry_ref, BSG_HUD_GEOMETRY_REF_NULL_INIT, bsg_hud_geometry_ref_create, bsg_hud_geometry_type, BSG_GEOMETRY_NODE_HUD)
GEOMETRY_CREATE_FN(bsg_annotation_ref, BSG_ANNOTATION_REF_NULL_INIT, bsg_annotation_ref_create, bsg_annotation_type, BSG_GEOMETRY_NODE_ANNOTATION)
GEOMETRY_CREATE_FN(bsg_csg_proxy_ref, BSG_CSG_PROXY_REF_NULL_INIT, bsg_csg_proxy_ref_create, bsg_csg_proxy_type, BSG_GEOMETRY_NODE_CSG_PROXY)
GEOMETRY_CREATE_FN(bsg_brep_proxy_ref, BSG_BREP_PROXY_REF_NULL_INIT, bsg_brep_proxy_ref_create, bsg_brep_proxy_type, BSG_GEOMETRY_NODE_BREP_PROXY)
GEOMETRY_CREATE_FN(bsg_edit_preview_ref, BSG_EDIT_PREVIEW_REF_NULL_INIT, bsg_edit_preview_ref_create, bsg_edit_preview_type, BSG_GEOMETRY_NODE_EDIT_PREVIEW)

#undef GEOMETRY_CREATE_FN

bsg_node_ref bsg_line_set_ref_as_node(bsg_line_set_ref ref) { return bsg_geometry_ref_as_node(ref.geometry); }
bsg_node_ref bsg_indexed_line_set_ref_as_node(bsg_indexed_line_set_ref ref) { return bsg_geometry_ref_as_node(ref.geometry); }
bsg_node_ref bsg_point_set_ref_as_node(bsg_point_set_ref ref) { return bsg_geometry_ref_as_node(ref.geometry); }
bsg_node_ref bsg_indexed_face_set_ref_as_node(bsg_indexed_face_set_ref ref) { return bsg_geometry_ref_as_node(ref.geometry); }
bsg_node_ref bsg_mesh_ref_as_node(bsg_mesh_ref ref) { return bsg_geometry_ref_as_node(ref.geometry); }
bsg_node_ref bsg_text_ref_as_node(bsg_text_ref ref) { return bsg_geometry_ref_as_node(ref.geometry); }
bsg_node_ref bsg_image_ref_as_node(bsg_image_ref ref) { return bsg_geometry_ref_as_node(ref.geometry); }
bsg_node_ref bsg_framebuffer_ref_as_node(bsg_framebuffer_ref ref) { return bsg_geometry_ref_as_node(ref.geometry); }
bsg_node_ref bsg_overlay_geometry_ref_as_node(bsg_overlay_geometry_ref ref) { return bsg_geometry_ref_as_node(ref.geometry); }
bsg_node_ref bsg_hud_geometry_ref_as_node(bsg_hud_geometry_ref ref) { return bsg_geometry_ref_as_node(ref.geometry); }
bsg_node_ref bsg_annotation_ref_as_node(bsg_annotation_ref ref) { return bsg_geometry_ref_as_node(ref.geometry); }
bsg_node_ref bsg_csg_proxy_ref_as_node(bsg_csg_proxy_ref ref) { return bsg_geometry_ref_as_node(ref.geometry); }
bsg_node_ref bsg_brep_proxy_ref_as_node(bsg_brep_proxy_ref ref) { return bsg_geometry_ref_as_node(ref.geometry); }
bsg_node_ref bsg_edit_preview_ref_as_node(bsg_edit_preview_ref ref) { return bsg_geometry_ref_as_node(ref.geometry); }

bsg_geometry_ref bsg_line_set_ref_as_geometry(bsg_line_set_ref ref) { return ref.geometry; }
bsg_geometry_ref bsg_indexed_line_set_ref_as_geometry(bsg_indexed_line_set_ref ref) { return ref.geometry; }
bsg_geometry_ref bsg_point_set_ref_as_geometry(bsg_point_set_ref ref) { return ref.geometry; }
bsg_geometry_ref bsg_indexed_face_set_ref_as_geometry(bsg_indexed_face_set_ref ref) { return ref.geometry; }
bsg_geometry_ref bsg_mesh_ref_as_geometry(bsg_mesh_ref ref) { return ref.geometry; }
bsg_geometry_ref bsg_text_ref_as_geometry(bsg_text_ref ref) { return ref.geometry; }
bsg_geometry_ref bsg_image_ref_as_geometry(bsg_image_ref ref) { return ref.geometry; }
bsg_geometry_ref bsg_framebuffer_ref_as_geometry(bsg_framebuffer_ref ref) { return ref.geometry; }
bsg_geometry_ref bsg_overlay_geometry_ref_as_geometry(bsg_overlay_geometry_ref ref) { return ref.geometry; }
bsg_geometry_ref bsg_hud_geometry_ref_as_geometry(bsg_hud_geometry_ref ref) { return ref.geometry; }
bsg_geometry_ref bsg_annotation_ref_as_geometry(bsg_annotation_ref ref) { return ref.geometry; }
bsg_geometry_ref bsg_csg_proxy_ref_as_geometry(bsg_csg_proxy_ref ref) { return ref.geometry; }
bsg_geometry_ref bsg_brep_proxy_ref_as_geometry(bsg_brep_proxy_ref ref) { return ref.geometry; }
bsg_geometry_ref bsg_edit_preview_ref_as_geometry(bsg_edit_preview_ref ref) { return ref.geometry; }

bsg_line_set_ref
bsg_node_ref_as_line_set(bsg_node_ref ref)
{
    bsg_line_set_ref typed = BSG_LINE_SET_REF_NULL_INIT;
    if (bsg_node_is_a(ref, bsg_line_set_type()))
	typed.geometry.shape.node = ref;
    return typed;
}

bsg_indexed_line_set_ref
bsg_node_ref_as_indexed_line_set(bsg_node_ref ref)
{
    bsg_indexed_line_set_ref typed = BSG_INDEXED_LINE_SET_REF_NULL_INIT;
    if (bsg_node_is_a(ref, bsg_indexed_line_set_type()))
	typed.geometry.shape.node = ref;
    return typed;
}

bsg_point_set_ref
bsg_node_ref_as_point_set(bsg_node_ref ref)
{
    bsg_point_set_ref typed = BSG_POINT_SET_REF_NULL_INIT;
    if (bsg_node_is_a(ref, bsg_point_set_type()))
	typed.geometry.shape.node = ref;
    return typed;
}

bsg_indexed_face_set_ref
bsg_node_ref_as_indexed_face_set(bsg_node_ref ref)
{
    bsg_indexed_face_set_ref typed = BSG_INDEXED_FACE_SET_REF_NULL_INIT;
    if (bsg_node_is_a(ref, bsg_indexed_face_set_type()))
	typed.geometry.shape.node = ref;
    return typed;
}

bsg_mesh_ref
bsg_node_ref_as_mesh(bsg_node_ref ref)
{
    bsg_mesh_ref typed = BSG_MESH_REF_NULL_INIT;
    if (bsg_node_is_a(ref, bsg_mesh_type()))
	typed.geometry.shape.node = ref;
    return typed;
}

bsg_text_ref
bsg_node_ref_as_text(bsg_node_ref ref)
{
    bsg_text_ref typed = BSG_TEXT_REF_NULL_INIT;
    if (bsg_node_is_a(ref, bsg_text_type()))
	typed.geometry.shape.node = ref;
    return typed;
}

bsg_image_ref
bsg_node_ref_as_image(bsg_node_ref ref)
{
    bsg_image_ref typed = BSG_IMAGE_REF_NULL_INIT;
    if (bsg_node_is_a(ref, bsg_image_type()))
	typed.geometry.shape.node = ref;
    return typed;
}

bsg_framebuffer_ref
bsg_node_ref_as_framebuffer(bsg_node_ref ref)
{
    bsg_framebuffer_ref typed = BSG_FRAMEBUFFER_REF_NULL_INIT;
    if (bsg_node_is_a(ref, bsg_framebuffer_type()))
	typed.geometry.shape.node = ref;
    return typed;
}

bsg_overlay_geometry_ref
bsg_node_ref_as_overlay_geometry(bsg_node_ref ref)
{
    bsg_overlay_geometry_ref typed = BSG_OVERLAY_GEOMETRY_REF_NULL_INIT;
    if (bsg_node_is_a(ref, bsg_overlay_geometry_type()))
	typed.geometry.shape.node = ref;
    return typed;
}

bsg_hud_geometry_ref
bsg_node_ref_as_hud_geometry(bsg_node_ref ref)
{
    bsg_hud_geometry_ref typed = BSG_HUD_GEOMETRY_REF_NULL_INIT;
    if (bsg_node_is_a(ref, bsg_hud_geometry_type()))
	typed.geometry.shape.node = ref;
    return typed;
}

bsg_annotation_ref
bsg_node_ref_as_annotation(bsg_node_ref ref)
{
    bsg_annotation_ref typed = BSG_ANNOTATION_REF_NULL_INIT;
    if (bsg_node_is_a(ref, bsg_annotation_type()))
	typed.geometry.shape.node = ref;
    return typed;
}

bsg_csg_proxy_ref
bsg_node_ref_as_csg_proxy(bsg_node_ref ref)
{
    bsg_csg_proxy_ref typed = BSG_CSG_PROXY_REF_NULL_INIT;
    if (bsg_node_is_a(ref, bsg_csg_proxy_type()))
	typed.geometry.shape.node = ref;
    return typed;
}

bsg_brep_proxy_ref
bsg_node_ref_as_brep_proxy(bsg_node_ref ref)
{
    bsg_brep_proxy_ref typed = BSG_BREP_PROXY_REF_NULL_INIT;
    if (bsg_node_is_a(ref, bsg_brep_proxy_type()))
	typed.geometry.shape.node = ref;
    return typed;
}

bsg_edit_preview_ref
bsg_node_ref_as_edit_preview(bsg_node_ref ref)
{
    bsg_edit_preview_ref typed = BSG_EDIT_PREVIEW_REF_NULL_INIT;
    if (bsg_node_is_a(ref, bsg_edit_preview_type()))
	typed.geometry.shape.node = ref;
    return typed;
}

bsg_geometry_node_kind
bsg_geometry_ref_kind(bsg_geometry_ref ref)
{
    int kind = BSG_GEOMETRY_NODE_NONE;
    if (!bsg_field_get_enum(bsg_geometry_ref_kind_field(ref), &kind))
	return BSG_GEOMETRY_NODE_NONE;
    return (bsg_geometry_node_kind)kind;
}


bsg_geometry_ref
bsg_scene_ref_as_geometry(bsg_scene_ref ref)
{
    bsg_geometry_ref geometry = BSG_GEOMETRY_REF_NULL_INIT;
    if (bsg_scene_ref_type(ref) != BSG_SCENE_ELEMENT_SHAPE)
	return geometry;
    geometry = _geometry_ref_from_node((bsg_node *)ref.opaque);
    return _geometry_ref_valid(geometry) ? geometry : (bsg_geometry_ref)BSG_GEOMETRY_REF_NULL_INIT;
}

bsg_scene_ref
bsg_geometry_ref_as_scene(bsg_geometry_ref ref)
{
    if (!_geometry_ref_valid(ref))
	return (bsg_scene_ref)BSG_SCENE_REF_NULL_INIT;
    return bsg_scene_ref_from_object_ref(bsg_node_ref_object(ref.shape.node));
}

static bsg_field_ref
_geometry_field(bsg_geometry_ref ref, bsg_field_id_t id)
{
    if (!_geometry_ref_valid(ref))
	return bsg_field_ref_null();
    return bsg_node_field_ref(_geometry_node_from_ref(ref.shape.node), id);
}

bsg_field_ref bsg_geometry_ref_kind_field(bsg_geometry_ref ref) { return _geometry_field(ref, BSG_FIELD_GEOMETRY_KIND); }
bsg_field_ref bsg_geometry_ref_coordinates_field(bsg_geometry_ref ref) { return _geometry_field(ref, BSG_FIELD_GEOMETRY_COORDINATES); }
bsg_field_ref bsg_geometry_ref_normals_field(bsg_geometry_ref ref) { return _geometry_field(ref, BSG_FIELD_GEOMETRY_NORMALS); }
bsg_field_ref bsg_geometry_ref_colors_field(bsg_geometry_ref ref) { return _geometry_field(ref, BSG_FIELD_GEOMETRY_COLORS); }
bsg_field_ref bsg_geometry_ref_indices_field(bsg_geometry_ref ref) { return _geometry_field(ref, BSG_FIELD_GEOMETRY_INDICES); }
bsg_field_ref bsg_geometry_ref_primitive_sets_field(bsg_geometry_ref ref) { return _geometry_field(ref, BSG_FIELD_GEOMETRY_PRIMITIVE_SETS); }
bsg_field_ref bsg_geometry_ref_text_field(bsg_geometry_ref ref) { return _geometry_field(ref, BSG_FIELD_GEOMETRY_TEXT); }
bsg_field_ref bsg_geometry_ref_image_width_field(bsg_geometry_ref ref) { return _geometry_field(ref, BSG_FIELD_GEOMETRY_IMAGE_WIDTH); }
bsg_field_ref bsg_geometry_ref_image_height_field(bsg_geometry_ref ref) { return _geometry_field(ref, BSG_FIELD_GEOMETRY_IMAGE_HEIGHT); }
bsg_field_ref bsg_geometry_ref_image_channels_field(bsg_geometry_ref ref) { return _geometry_field(ref, BSG_FIELD_GEOMETRY_IMAGE_CHANNELS); }
bsg_field_ref bsg_geometry_ref_revision_field(bsg_geometry_ref ref) { return _geometry_field(ref, BSG_FIELD_GEOMETRY_SOURCE_REVISION); }

static void
_geometry_revision_bump(bsg_geometry_ref ref);

uint64_t
bsg_geometry_ref_revision(bsg_geometry_ref ref)
{
    uint64_t rev = 0;
    (void)bsg_field_get_uint64(bsg_geometry_ref_revision_field(ref), &rev);
    return rev;
}

int
bsg_geometry_ref_clear(bsg_geometry_ref ref)
{
    bsg_node *node = _geometry_node_from_ref(ref.shape.node);
    if (!node)
	return 0;
    if (!bsg_geometry_node_set_kind(node, BSG_GEOMETRY_NODE_NONE))
	return 0;
    bsg_node_set_payload(node, NULL);
    (void)bsg_field_multi_clear(bsg_geometry_ref_coordinates_field(ref));
    (void)bsg_field_multi_clear(bsg_geometry_ref_normals_field(ref));
    (void)bsg_field_multi_clear(bsg_geometry_ref_colors_field(ref));
    (void)bsg_field_multi_clear(bsg_geometry_ref_indices_field(ref));
    (void)bsg_field_multi_clear(bsg_geometry_ref_primitive_sets_field(ref));
    point_t bmin = VINIT_ZERO;
    point_t bmax = VINIT_ZERO;
    bsg_node_set_bounds(node, bmin, bmax, 0);
    _geometry_revision_bump(ref);
    return 1;
}

static void
_geometry_revision_bump(bsg_geometry_ref ref)
{
    uint64_t rev = bsg_geometry_ref_revision(ref);
    (void)bsg_field_set_uint64(bsg_geometry_ref_revision_field(ref), rev + 1);
}

static int
_set_point_array(bsg_field_ref field, const point_t *points, size_t count)
{
    if (!bsg_field_multi_clear(field))
	return count == 0 ? 1 : 0;
    for (size_t i = 0; i < count; i++) {
	point_t zero = VINIT_ZERO;
	if (!bsg_field_multi_point_append(field, points ? points[i] : zero))
	    return 0;
    }
    return 1;
}

static int
_set_int_array(bsg_field_ref field, const int *values, size_t count, int default_value)
{
    if (!bsg_field_multi_clear(field))
	return count == 0 ? 1 : 0;
    for (size_t i = 0; i < count; i++) {
	if (!bsg_field_multi_int_append(field, values ? values[i] : default_value))
	    return 0;
    }
    return 1;
}

static int
_set_line_command_array(bsg_field_ref field, const int *values, size_t count)
{
    if (!bsg_field_multi_clear(field))
	return count == 0 ? 1 : 0;
    for (size_t i = 0; i < count; i++) {
	int cmd = values ? values[i] : ((i % 2) ? BSG_GEOMETRY_LINE_DRAW : BSG_GEOMETRY_LINE_MOVE);
	switch (cmd) {
	    case BSG_GEOMETRY_LINE_MOVE:
	    case BSG_GEOMETRY_LINE_DRAW:
	    case BSG_GEOMETRY_POINT_DRAW:
		break;
	    default:
		return 0;
	}
	if (!bsg_field_multi_int_append(field, cmd))
	    return 0;
    }
    return 1;
}

static void
_geometry_update_bounds_from_points(bsg_node *node, const point_t *points, size_t count)
{
    point_t bmin, bmax;

    if (!node)
	return;

    if (!points || !count) {
	VSETALL(bmin, 0.0);
	VSETALL(bmax, 0.0);
	bsg_node_set_bounds(node, bmin, bmax, 0);
	return;
    }

    VMOVE(bmin, points[0]);
    VMOVE(bmax, points[0]);
    for (size_t i = 1; i < count; i++) {
	VMIN(bmin, points[i]);
	VMAX(bmax, points[i]);
    }
    bsg_node_set_bounds(node, bmin, bmax, 1);
}

int
bsg_geometry_node_clear_private_realization(struct bsg_node *node)
{
    if (!node)
	return 0;
    bsg_node_set_payload(node, NULL);
    return 1;
}

static bsg_geometry_node_kind
_geometry_kind_from_payload(const struct bsg_payload *payload)
{
    if (!payload)
	return BSG_GEOMETRY_NODE_NONE;
    switch (payload->pl_type) {
	case BSG_PL_LINE_SET:
	    return BSG_GEOMETRY_NODE_LINE_SET;
	case BSG_PL_TEXT:
	    return BSG_GEOMETRY_NODE_TEXT;
	case BSG_PL_HUD_TEXT:
	    return BSG_GEOMETRY_NODE_HUD;
	case BSG_PL_IMAGE:
	    return BSG_GEOMETRY_NODE_IMAGE;
	case BSG_PL_FRAMEBUFFER:
	    return BSG_GEOMETRY_NODE_FRAMEBUFFER;
	case BSG_PL_AXES:
	case BSG_PL_GRID:
	    return BSG_GEOMETRY_NODE_OVERLAY;
	case BSG_PL_ANNOTATION:
	    return BSG_GEOMETRY_NODE_ANNOTATION;
	case BSG_PL_CSG:
	    return BSG_GEOMETRY_NODE_CSG_PROXY;
	case BSG_PL_MESH:
	    return BSG_GEOMETRY_NODE_MESH;
	case BSG_PL_BREP:
	    return BSG_GEOMETRY_NODE_BREP_PROXY;
	case BSG_PL_SKETCH:
	case BSG_PL_EDIT_PREVIEW:
	    return BSG_GEOMETRY_NODE_EDIT_PREVIEW;
	default:
	    return BSG_GEOMETRY_NODE_NONE;
    }
}

int
bsg_geometry_node_set_private_realization(struct bsg_node *node, struct bsg_payload *payload)
{
    bsg_geometry_ref geometry;
    bsg_geometry_node_kind kind;
    int field_geometry_updated = 0;
    if (!node)
	return 0;
    if (!payload)
	return bsg_geometry_node_clear_private_realization(node);
    kind = _geometry_kind_from_payload(payload);
    if (kind == BSG_GEOMETRY_NODE_LINE_SET) {
	struct bsg_payload_line_set *ls = bsg_payload_line_set_get(payload);
	if (ls) {
	    if (!bsg_geometry_node_set_line_set_fields(node, (const point_t *)ls->points, ls->cmds, ls->point_cnt))
		return 0;
	    field_geometry_updated = 1;
	}
    } else if (kind != BSG_GEOMETRY_NODE_NONE) {
	if (!bsg_geometry_node_set_kind(node, kind))
	    return 0;
    }
    geometry = _geometry_ref_from_node(node);
    bsg_node_set_payload(node, payload);
    if (!field_geometry_updated)
	_geometry_revision_bump(geometry);
    return 1;
}

int
bsg_geometry_node_set_kind(struct bsg_node *node, bsg_geometry_node_kind kind)
{
    bsg_type_id type = bsg_geometry_type();
    switch (kind) {
	case BSG_GEOMETRY_NODE_LINE_SET: type = bsg_line_set_type(); break;
	case BSG_GEOMETRY_NODE_INDEXED_LINE_SET: type = bsg_indexed_line_set_type(); break;
	case BSG_GEOMETRY_NODE_POINT_SET: type = bsg_point_set_type(); break;
	case BSG_GEOMETRY_NODE_INDEXED_FACE_SET: type = bsg_indexed_face_set_type(); break;
	case BSG_GEOMETRY_NODE_MESH: type = bsg_mesh_type(); break;
	case BSG_GEOMETRY_NODE_TEXT: type = bsg_text_type(); break;
	case BSG_GEOMETRY_NODE_IMAGE: type = bsg_image_type(); break;
	case BSG_GEOMETRY_NODE_FRAMEBUFFER: type = bsg_framebuffer_type(); break;
	case BSG_GEOMETRY_NODE_OVERLAY: type = bsg_overlay_geometry_type(); break;
	case BSG_GEOMETRY_NODE_HUD: type = bsg_hud_geometry_type(); break;
	case BSG_GEOMETRY_NODE_ANNOTATION: type = bsg_annotation_type(); break;
	case BSG_GEOMETRY_NODE_CSG_PROXY: type = bsg_csg_proxy_type(); break;
	case BSG_GEOMETRY_NODE_BREP_PROXY: type = bsg_brep_proxy_type(); break;
	case BSG_GEOMETRY_NODE_EDIT_PREVIEW: type = bsg_edit_preview_type(); break;
	default: break;
    }
    return _set_node_type_and_kind(node, type, kind);
}

bsg_geometry_node_kind
bsg_geometry_node_kind_get(const struct bsg_node *node)
{
    int kind = BSG_GEOMETRY_NODE_NONE;
    if (!node)
	return BSG_GEOMETRY_NODE_NONE;
    (void)bsg_field_get_enum(bsg_node_field_ref((bsg_node *)node, BSG_FIELD_GEOMETRY_KIND), &kind);
    return (bsg_geometry_node_kind)kind;
}

uint64_t
bsg_geometry_node_revision(const struct bsg_node *node)
{
    if (!node)
	return 0;
    return bsg_geometry_ref_revision(_geometry_ref_from_node((bsg_node *)node));
}

int
bsg_geometry_node_bump_revision(struct bsg_node *node)
{
    if (!node)
	return 0;
    _geometry_revision_bump(_geometry_ref_from_node(node));
    return 1;
}

int
bsg_line_set_ref_set_points(bsg_line_set_ref ref, const point_t *points, const int *commands, size_t point_count)
{
    bsg_node *node = _geometry_node_from_ref(ref.geometry.shape.node);
    if (!node || !bsg_node_is_a(ref.geometry.shape.node, bsg_line_set_type()))
	return 0;
    bsg_node_add_geometry_roles(node, BSG_GEOMETRY_LINE_SET);
    if (!_set_point_array(bsg_geometry_ref_coordinates_field(ref.geometry), points, point_count))
	return 0;
    if (!_set_line_command_array(bsg_geometry_ref_primitive_sets_field(ref.geometry), commands, point_count))
	return 0;
    bsg_node_set_payload(node, NULL);
    _geometry_update_bounds_from_points(node, points, point_count);
    _geometry_revision_bump(ref.geometry);
    return 1;
}

size_t
bsg_line_set_ref_point_count(bsg_line_set_ref ref)
{
    return bsg_field_multi_count(bsg_geometry_ref_coordinates_field(ref.geometry));
}

int
bsg_line_set_ref_point_at(bsg_line_set_ref ref, size_t idx, point_t value)
{
    return bsg_field_multi_point_at(bsg_geometry_ref_coordinates_field(ref.geometry), idx, value);
}

int
bsg_line_set_ref_command_at(bsg_line_set_ref ref, size_t idx, int *command)
{
    return bsg_field_multi_int_at(bsg_geometry_ref_primitive_sets_field(ref.geometry), idx, command);
}

int
bsg_point_set_ref_set_points(bsg_point_set_ref ref, const point_t *points, size_t point_count)
{
    bsg_node *node = _geometry_node_from_ref(ref.geometry.shape.node);
    int *commands = NULL;
    int ok = 1;
    if (!node || !bsg_node_is_a(ref.geometry.shape.node, bsg_point_set_type()))
	return 0;
    if (point_count)
	commands = (int *)bu_calloc(point_count, sizeof(int), "bsg point-set commands");
    for (size_t i = 0; i < point_count; i++)
	commands[i] = BSG_GEOMETRY_POINT_DRAW;
    ok = _set_point_array(bsg_geometry_ref_coordinates_field(ref.geometry), points, point_count);
    ok = ok && _set_int_array(bsg_geometry_ref_primitive_sets_field(ref.geometry), commands, point_count, BSG_GEOMETRY_POINT_DRAW);
    if (ok) {
	bsg_node_set_payload(node, NULL);
	_geometry_update_bounds_from_points(node, points, point_count);
    }
    if (commands)
	bu_free(commands, "bsg point-set commands");
    if (ok)
	_geometry_revision_bump(ref.geometry);
    return ok;
}

int
bsg_indexed_line_set_ref_set_geometry(bsg_indexed_line_set_ref ref,
	const point_t *points, size_t point_count, const int *indices, size_t index_count)
{
    bsg_node *node = _geometry_node_from_ref(ref.geometry.shape.node);
    size_t segment_count = 0;
    size_t segment_vertices = 0;
    if (!node || !bsg_node_is_a(ref.geometry.shape.node, bsg_indexed_line_set_type()))
	return 0;
    if (!point_count && !index_count) {
	/* Empty indexed-line geometry is a valid clear-to-empty state. */
    } else if (!points || !point_count || !indices || !index_count) {
	return 0;
    }
    for (size_t i = 0; i < index_count; i++) {
	int idx = indices[i];
	if (idx < 0) {
	    if (idx != -1 || segment_vertices < 2)
		return 0;
	    segment_count++;
	    segment_vertices = 0;
	    continue;
	}
	if ((size_t)idx >= point_count)
	    return 0;
	segment_vertices++;
    }
    if (segment_vertices) {
	if (segment_vertices < 2)
	    return 0;
	segment_count++;
    }
    if (point_count && !segment_count)
	return 0;
    bsg_node_add_geometry_roles(node, BSG_GEOMETRY_LINE_SET);
    if (!_set_point_array(bsg_geometry_ref_coordinates_field(ref.geometry), points, point_count))
	return 0;
    if (!_set_int_array(bsg_geometry_ref_indices_field(ref.geometry), indices, index_count, -1))
	return 0;
    bsg_node_set_payload(node, NULL);
    _geometry_update_bounds_from_points(node, points, point_count);
    _geometry_revision_bump(ref.geometry);
    return 1;
}

static int _indexed_face_set_validate(const point_t *points, size_t point_count,
	const vect_t *normals, size_t normal_count,
	const int *indices, size_t index_count,
	size_t *face_count_out);

int
bsg_indexed_face_set_ref_set_geometry(bsg_indexed_face_set_ref ref,
	const point_t *points, size_t point_count, const int *indices, size_t index_count)
{
    bsg_node *node = _geometry_node_from_ref(ref.geometry.shape.node);
    if (!node || !bsg_node_is_a(ref.geometry.shape.node, bsg_indexed_face_set_type()))
	return 0;
    if (!bsg_geometry_ref_set_indexed_face_set_fields(ref.geometry, points,
		point_count, NULL, 0, indices, index_count))
	return 0;
    bsg_node_set_payload(node, NULL);
    return 1;
}

static struct bsg_label *
_label_create(const char *text, const point_t position, int size)
{
    struct bsg_label *label;
    BU_GET(label, struct bsg_label);
    memset(label, 0, sizeof(*label));
    BU_VLS_INIT(&label->label);
    bu_vls_sprintf(&label->label, "%s", text ? text : "");
    label->size = size;
    if (position)
	VMOVE(label->p, position);
    return label;
}

static struct bsg_label *
_label_clone(const struct bsg_label *src)
{
    struct bsg_label *label;
    if (!src)
	return NULL;
    BU_GET(label, struct bsg_label);
    memset(label, 0, sizeof(*label));
    BU_VLS_INIT(&label->label);
    bu_vls_sprintf(&label->label, "%s", BU_VLS_IS_INITIALIZED(&src->label) ? bu_vls_cstr(&src->label) : "");
    label->size = src->size;
    VMOVE(label->p, src->p);
    label->line_flag = src->line_flag;
    VMOVE(label->target, src->target);
    label->anchor = src->anchor;
    label->arrow = src->arrow;
    return label;
}

int
bsg_text_ref_set_text(bsg_text_ref ref, const char *text, const point_t position, int size)
{
    bsg_node *node = _geometry_node_from_ref(ref.geometry.shape.node);
    if (!node || !bsg_node_is_a(ref.geometry.shape.node, bsg_text_type()))
	return 0;
    if (!text || !text[0]) {
	bsg_node_set_payload(node, NULL);
	(void)bsg_field_set_string(bsg_geometry_ref_text_field(ref.geometry), "");
	_geometry_revision_bump(ref.geometry);
	return 1;
    }
    (void)bsg_field_set_string(bsg_geometry_ref_text_field(ref.geometry), text);
    (void)bsg_field_multi_clear(bsg_geometry_ref_coordinates_field(ref.geometry));
    if (position)
	(void)bsg_field_multi_point_append(bsg_geometry_ref_coordinates_field(ref.geometry), position);
    bsg_node_set_payload(node, bsg_payload_text_create(_label_create(text, position, size)));
    _geometry_revision_bump(ref.geometry);
    return 1;
}

int
bsg_hud_geometry_ref_set_text(bsg_hud_geometry_ref ref, const char *text, fastf_t x, fastf_t y, int size)
{
    point_t position;
    bsg_node *node = _geometry_node_from_ref(ref.geometry.shape.node);
    if (!node || !bsg_node_is_a(ref.geometry.shape.node, bsg_hud_geometry_type()))
	return 0;
    VSET(position, x, y, 0.0);
    if (!text || !text[0]) {
	bsg_node_set_payload(node, NULL);
	(void)bsg_field_set_string(bsg_geometry_ref_text_field(ref.geometry), "");
	_geometry_revision_bump(ref.geometry);
	return 1;
    }
    (void)bsg_field_set_string(bsg_geometry_ref_text_field(ref.geometry), text);
    (void)bsg_field_multi_clear(bsg_geometry_ref_coordinates_field(ref.geometry));
    (void)bsg_field_multi_point_append(bsg_geometry_ref_coordinates_field(ref.geometry), position);
    bsg_node_set_payload(node, bsg_payload_hud_text_create(_label_create(text, position, size)));
    bsg_node_set_payload_type(node, BSG_PAYLOAD_OVERLAY);
    _geometry_revision_bump(ref.geometry);
    return 1;
}

int
bsg_geometry_ref_set_annotation(bsg_geometry_ref ref,
				const char *summary,
				bsg_annotation_space space,
				const point_t anchor,
				const mat_t model_mat,
				const mat_t display_mat,
				const point_t *points,
				size_t point_count,
				const struct bsg_annotation_segment *segments,
				size_t segment_count)
{
    bsg_node *node = _geometry_node_from_ref(ref.shape.node);
    point_t local_anchor = VINIT_ZERO;
    point_t bmin = VINIT_ZERO;
    point_t bmax = VINIT_ZERO;
    mat_t local_model_mat, local_display_mat;
    struct bsg_payload *payload;

    if (!node)
	return 0;
    if (anchor)
	VMOVE(local_anchor, anchor);
    MAT_IDN(local_model_mat);
    MAT_IDN(local_display_mat);
    if (model_mat)
	MAT_COPY(local_model_mat, model_mat);
    if (display_mat)
	MAT_COPY(local_display_mat, display_mat);

    if (!bsg_geometry_node_set_kind(node, BSG_GEOMETRY_NODE_ANNOTATION))
	return 0;
    bsg_node_add_geometry_roles(node, BSG_GEOMETRY_TEXT_LABELS);
    bsg_node_set_payload_type(node, BSG_PAYLOAD_OVERLAY);
    (void)bsg_field_set_string(bsg_geometry_ref_text_field(ref), summary ? summary : "");
    if (!_set_point_array(bsg_geometry_ref_coordinates_field(ref), points, point_count))
	return 0;

    payload = bsg_payload_annotation_create_record(summary, space, local_anchor,
	    local_model_mat, local_display_mat, points, point_count, segments,
	    segment_count);
    if (!payload)
	return 0;
    bsg_node_set_payload(node, payload);

    VMOVE(bmin, local_anchor);
    VMOVE(bmax, local_anchor);
    for (size_t i = 0; i < point_count; i++) {
	point_t bp;
	if (space == BSG_ANNOTATION_SPACE_DISPLAY) {
	    point_t local;
	    MAT4X3PNT(local, local_display_mat, points[i]);
	    VADD2(bp, local_anchor, local);
	} else {
	    MAT4X3PNT(bp, local_model_mat, points[i]);
	}
	VMINMAX(bmin, bmax, bp);
    }
    bsg_node_set_bounds(node, bmin, bmax, 1);
    _geometry_revision_bump(ref);
    return 1;
}

int
bsg_annotation_ref_set_record(bsg_annotation_ref ref,
			      const char *summary,
			      bsg_annotation_space space,
			      const point_t anchor,
			      const mat_t model_mat,
			      const mat_t display_mat,
			      const point_t *points,
			      size_t point_count,
			      const struct bsg_annotation_segment *segments,
			      size_t segment_count)
{
    bsg_node *node = _geometry_node_from_ref(ref.geometry.shape.node);
    if (!node || !bsg_node_is_a(ref.geometry.shape.node, bsg_annotation_type()))
	return 0;
    return bsg_geometry_ref_set_annotation(ref.geometry, summary, space, anchor,
	    model_mat, display_mat, points, point_count, segments, segment_count);
}

int
bsg_image_ref_set_metadata(bsg_image_ref ref, size_t width, size_t height, size_t channels)
{
    bsg_node *node = _geometry_node_from_ref(ref.geometry.shape.node);
    if (!node || !bsg_node_is_a(ref.geometry.shape.node, bsg_image_type()))
	return 0;
    (void)bsg_field_set_uint64(bsg_geometry_ref_image_width_field(ref.geometry), (uint64_t)width);
    (void)bsg_field_set_uint64(bsg_geometry_ref_image_height_field(ref.geometry), (uint64_t)height);
    (void)bsg_field_set_uint64(bsg_geometry_ref_image_channels_field(ref.geometry), (uint64_t)channels);
    bsg_node_set_payload(node, bsg_payload_image_create(width, height, channels, NULL));
    _geometry_revision_bump(ref.geometry);
    return 1;
}

int
bsg_framebuffer_ref_set_mode(bsg_framebuffer_ref ref, int mode)
{
    bsg_node *node = _geometry_node_from_ref(ref.geometry.shape.node);
    if (!node || !bsg_node_is_a(ref.geometry.shape.node, bsg_framebuffer_type()))
	return 0;
    (void)bsg_field_set_uint64(bsg_geometry_ref_image_channels_field(ref.geometry), (uint64_t)(mode < 0 ? 0 : mode));
    bsg_node_set_payload(node, mode ? bsg_payload_framebuffer_create(NULL, mode) : NULL);
    bsg_node_set_payload_type(node, BSG_PAYLOAD_OVERLAY);
    _geometry_revision_bump(ref.geometry);
    return 1;
}

int
bsg_geometry_node_set_line_set_fields(struct bsg_node *node, const point_t *points, const int *commands, size_t point_count)
{
    bsg_geometry_ref geometry;
    if (!bsg_geometry_node_set_kind(node, BSG_GEOMETRY_NODE_LINE_SET))
	return 0;
    bsg_node_add_geometry_roles(node, BSG_GEOMETRY_LINE_SET);
    geometry = _geometry_ref_from_node(node);
    if (!_set_point_array(bsg_geometry_ref_coordinates_field(geometry), points, point_count))
	return 0;
    if (!_set_line_command_array(bsg_geometry_ref_primitive_sets_field(geometry), commands, point_count))
	return 0;
    _geometry_update_bounds_from_points(node, points, point_count);
    _geometry_revision_bump(geometry);
    return 1;
}

int
bsg_geometry_node_set_line_set(struct bsg_node *node, const point_t *points, const int *commands, size_t point_count)
{
    if (!bsg_geometry_node_set_line_set_fields(node, points, commands, point_count))
	return 0;
    bsg_node_set_payload(node, bsg_payload_line_set_create((point_t *)points, commands, point_count));
    return 1;
}

int
bsg_geometry_ref_set_line_set(bsg_geometry_ref ref, const point_t *points, const int *commands, size_t point_count)
{
    bsg_node *node = _geometry_node_from_ref(ref.shape.node);
    if (!node)
	return 0;
    if (!bsg_geometry_node_set_line_set_fields(node, points, commands, point_count))
	return 0;
    bsg_node_set_payload(node, NULL);
    return 1;
}

int
bsg_geometry_node_set_point_set_fields(struct bsg_node *node, const point_t *points, size_t point_count)
{
    bsg_geometry_ref geometry;
    int *commands = NULL;
    int ok = 1;
    if (!bsg_geometry_node_set_kind(node, BSG_GEOMETRY_NODE_POINT_SET))
	return 0;
    geometry = _geometry_ref_from_node(node);
    if (point_count)
	commands = (int *)bu_calloc(point_count, sizeof(int), "bsg point-set field commands");
    for (size_t i = 0; i < point_count; i++)
	commands[i] = BSG_GEOMETRY_POINT_DRAW;
    ok = _set_point_array(bsg_geometry_ref_coordinates_field(geometry), points, point_count);
    ok = ok && _set_int_array(bsg_geometry_ref_primitive_sets_field(geometry), commands, point_count, BSG_GEOMETRY_POINT_DRAW);
    if (ok)
	_geometry_update_bounds_from_points(node, points, point_count);
    if (commands)
	bu_free(commands, "bsg point-set field commands");
    if (ok)
	_geometry_revision_bump(geometry);
    return ok;
}

int
bsg_geometry_ref_set_point_set(bsg_geometry_ref ref, const point_t *points, size_t point_count)
{
    bsg_node *node = _geometry_node_from_ref(ref.shape.node);
    if (!node)
	return 0;
    if (!bsg_geometry_node_set_point_set_fields(node, points, point_count))
	return 0;
    bsg_node_set_payload(node, NULL);
    return 1;
}

static int
_indexed_face_set_validate(const point_t *points, size_t point_count,
			   const vect_t *normals, size_t normal_count,
			   const int *indices, size_t index_count,
			   size_t *face_count_out)
{
    size_t face_count = 0;
    size_t face_vertices = 0;
    size_t vertex_index_count = 0;
    unsigned int face_stamp = 1;
    unsigned int *seen = NULL;
    int valid = 0;

    if (face_count_out)
	*face_count_out = 0;

    if (!point_count && !index_count)
	return normal_count ? 0 : 1;
    if (!points || !point_count || !indices || !index_count)
	return 0;
    if (normal_count && !normals)
	return 0;

    if (point_count > ((size_t)-1) / sizeof(unsigned int))
	return 0;
    seen = (unsigned int *)bu_calloc(point_count, sizeof(unsigned int),
	    "bsg indexed-face validation marks");

    for (size_t i = 0; i < index_count; i++) {
	int idx = indices[i];
	if (idx < 0) {
	    if (idx != -1 || face_vertices < 3)
		goto done;
	    face_count++;
	    face_vertices = 0;
	    if (face_stamp == UINT_MAX) {
		memset(seen, 0, point_count * sizeof(unsigned int));
		face_stamp = 1;
	    } else {
		face_stamp++;
	    }
	    continue;
	}
	if ((size_t)idx >= point_count)
	    goto done;
	if (seen[idx] == face_stamp)
	    goto done;
	seen[idx] = face_stamp;
	vertex_index_count++;
	face_vertices++;
    }

    if (face_vertices) {
	if (face_vertices < 3)
	    goto done;
	face_count++;
    }
    if (!face_count)
	goto done;

    if (normal_count &&
	    normal_count != vertex_index_count &&
	    normal_count != point_count &&
	    normal_count != face_count)
	goto done;

    if (face_count_out)
	*face_count_out = face_count;
    valid = 1;

done:
    if (seen)
	bu_free(seen, "bsg indexed-face validation marks");
    return valid;
}

int
bsg_geometry_ref_set_indexed_face_set_fields(bsg_geometry_ref ref,
	const point_t *points, size_t point_count,
	const vect_t *normals, size_t normal_count,
	const int *indices, size_t index_count)
{
    bsg_node *node = _geometry_node_from_ref(ref.shape.node);
    size_t face_count = 0;
    if (!node)
	return 0;
    if (!_indexed_face_set_validate(points, point_count, normals, normal_count,
		indices, index_count, &face_count))
	return 0;
    if (!bsg_geometry_node_set_kind(node, BSG_GEOMETRY_NODE_INDEXED_FACE_SET))
	return 0;
    if (!_set_point_array(bsg_geometry_ref_coordinates_field(ref), points, point_count))
	return 0;
    if (!_set_point_array(bsg_geometry_ref_normals_field(ref), (const point_t *)normals, normal_count))
	return 0;
    if (!_set_int_array(bsg_geometry_ref_indices_field(ref), indices, index_count, -1))
	return 0;
    _geometry_update_bounds_from_points(node, points, point_count);
    _geometry_revision_bump(ref);
    return 1;
}

int
bsg_geometry_ref_set_indexed_face_set(bsg_geometry_ref ref,
	const point_t *points, size_t point_count,
	const vect_t *normals, size_t normal_count,
	const int *indices, size_t index_count)
{
    bsg_node *node = _geometry_node_from_ref(ref.shape.node);
    if (!bsg_geometry_ref_set_indexed_face_set_fields(ref,
	    points, point_count, normals, normal_count, indices, index_count))
	return 0;
    bsg_node_set_payload(node, NULL);
    return 1;
}

int
bsg_geometry_ref_update_indexed_face_set(bsg_geometry_ref ref,
	const point_t *points, size_t point_count,
	const vect_t *normals, size_t normal_count,
	const int *indices, size_t index_count)
{
    return bsg_geometry_ref_set_indexed_face_set_fields(ref,
	    points, point_count, normals, normal_count, indices, index_count);
}

int
bsg_geometry_node_set_indexed_face_set_fields(struct bsg_node *node,
	const point_t *points, size_t point_count,
	const vect_t *normals, size_t normal_count,
	const int *indices, size_t index_count)
{
    if (!node)
	return 0;
    return bsg_geometry_ref_set_indexed_face_set_fields(_geometry_ref_from_node(node),
	    points, point_count, normals, normal_count, indices, index_count);
}

int
bsg_geometry_node_set_indexed_face_set(struct bsg_node *node,
	const point_t *points, size_t point_count,
	const vect_t *normals, size_t normal_count,
	const int *indices, size_t index_count)
{
    if (!node)
	return 0;
    return bsg_geometry_ref_set_indexed_face_set(_geometry_ref_from_node(node),
	    points, point_count, normals, normal_count, indices, index_count);
}

int
bsg_geometry_node_set_hud_text(struct bsg_node *node, const char *text, fastf_t x, fastf_t y, int size)
{
    bsg_hud_geometry_ref ref = BSG_HUD_GEOMETRY_REF_NULL_INIT;
    if (!bsg_geometry_node_set_kind(node, BSG_GEOMETRY_NODE_HUD))
	return 0;
    ref.geometry = _geometry_ref_from_node(node);
    return bsg_hud_geometry_ref_set_text(ref, text, x, y, size);
}

int
bsg_geometry_node_set_text_label(struct bsg_node *node, const struct bsg_label *label)
{
    bsg_geometry_ref geometry;
    if (!node || !label)
	return 0;
    if (!bsg_geometry_node_set_kind(node, BSG_GEOMETRY_NODE_TEXT))
	return 0;
    geometry = _geometry_ref_from_node(node);
    (void)bsg_field_set_string(bsg_geometry_ref_text_field(geometry),
	    BU_VLS_IS_INITIALIZED(&label->label) ? bu_vls_cstr(&label->label) : "");
    (void)bsg_field_multi_clear(bsg_geometry_ref_coordinates_field(geometry));
    (void)bsg_field_multi_point_append(bsg_geometry_ref_coordinates_field(geometry), label->p);
    bsg_node_set_payload(node, bsg_payload_text_create(_label_clone(label)));
    _geometry_revision_bump(geometry);
    return 1;
}

int
bsg_geometry_node_set_axes_overlay(struct bsg_node *node, const struct bsg_axes *src)
{
    struct bsg_axes *axes;
    bsg_geometry_ref geometry;
    if (!node || !src)
	return 0;
    if (!bsg_geometry_node_set_kind(node, BSG_GEOMETRY_NODE_OVERLAY))
	return 0;
    geometry = _geometry_ref_from_node(node);
    BU_GET(axes, struct bsg_axes);
    memcpy(axes, src, sizeof(*axes));
    bsg_node_set_payload(node, bsg_payload_axes_create(axes));
    bsg_node_set_payload_type(node, BSG_PAYLOAD_OVERLAY);
    _geometry_revision_bump(geometry);
    return 1;
}

int
bsg_geometry_node_set_grid_overlay(struct bsg_node *node, const struct bsg_grid_state *grid)
{
    bsg_geometry_ref geometry;
    if (!node || !grid)
	return 0;
    if (!bsg_geometry_node_set_kind(node, BSG_GEOMETRY_NODE_OVERLAY))
	return 0;
    geometry = _geometry_ref_from_node(node);
    bsg_node_set_payload(node, bsg_payload_grid_create(grid));
    bsg_node_set_payload_type(node, BSG_PAYLOAD_OVERLAY);
    _geometry_revision_bump(geometry);
    return 1;
}

int
bsg_geometry_node_set_mesh_realization(struct bsg_node *node, struct bsg_mesh_lod *mesh)
{
    struct bsg_payload *payload;
    if (!node || !mesh)
	return 0;
    payload = bsg_payload_mesh_create(mesh);
    if (!payload)
	return 0;
    if (!bsg_geometry_node_set_private_realization(node, payload)) {
	bsg_payload_free(payload);
	return 0;
    }
    return 1;
}

int
bsg_mesh_ref_set_lod(bsg_mesh_ref ref, struct bsg_mesh_lod *mesh)
{
    bsg_node *node = _geometry_node_from_ref(ref.geometry.shape.node);
    if (!node || !bsg_node_is_a(ref.geometry.shape.node, bsg_mesh_type()))
	return 0;
    return bsg_geometry_node_set_mesh_realization(node, mesh);
}

int
bsg_geometry_node_set_framebuffer_mode(struct bsg_node *node, int mode)
{
    bsg_framebuffer_ref ref = BSG_FRAMEBUFFER_REF_NULL_INIT;
    if (!bsg_geometry_node_set_kind(node, BSG_GEOMETRY_NODE_FRAMEBUFFER))
	return 0;
    ref.geometry = _geometry_ref_from_node(node);
    return bsg_framebuffer_ref_set_mode(ref, mode);
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
