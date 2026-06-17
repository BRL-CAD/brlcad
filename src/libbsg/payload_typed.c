/*             P A Y L O A D _ T Y P E D . C
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
/** @file libbsg/payload_typed.c
 *
 * Typed payload object model.
 *
 * Implements the typed payload lifecycle helpers plus the concrete payload
 * builders used by retained drawing records.
 */

#include "common.h"

#include <string.h>

#include "bg/polygon.h"
#include "bu/list.h"
#include "bu/malloc.h"
#include "bu/str.h"
#include "bu/vls.h"
#include "bsg/defines.h"
#include "bsg/draw_source.h"
#include "bsg/faceplate.h"
#include "bsg/geometry.h"
#include "bsg/lod.h"
#include "bsg/pick.h"
#include "bsg/polygon.h"
#include "payload_typed_private.h"
#include "bsg_private.h"
#include "draw_source_private.h"
#include "node_private.h"
#include "payload_private.h"
#include "polygon_private.h"

static unsigned long long
_typed_payload_flags(bsg_payload_type type)
{
    switch (type) {
	case BSG_PL_LINE_SET:
	case BSG_PL_TEXT:
	case BSG_PL_HUD_TEXT:
	case BSG_PL_POLYGON:
	case BSG_PL_IMAGE:
	case BSG_PL_FRAMEBUFFER:
	case BSG_PL_AXES:
	case BSG_PL_GRID:
	case BSG_PL_ANNOTATION:
	    return BSG_PAYLOAD_VLIST;
	case BSG_PL_SKETCH:
	case BSG_PL_EDIT_PREVIEW:
	    return BSG_PAYLOAD_VLIST;
	case BSG_PL_MESH:
	    return BSG_PAYLOAD_MESH;
	case BSG_PL_CSG:
	    return BSG_PAYLOAD_CSG;
	case BSG_PL_BREP:
	    return BSG_PAYLOAD_BREP;
	default:
	    return 0;
    }
}

static int
_no_bounds(struct bsg_payload *UNUSED(pl), point_t *UNUSED(bmin), point_t *UNUSED(bmax))
{
    return 0;
}

static int
_no_export(struct bsg_payload *UNUSED(pl), struct bu_vls *UNUSED(out))
{
    return 0;
}

static int
_no_backend_prepare(struct bsg_payload *UNUSED(pl), void *UNUSED(backend_ctx))
{
    return 0;
}

static void
_payload_realization_init(struct bsg_payload *pl)
{
    if (!pl)
	return;
    memset(&pl->realization, 0, sizeof(pl->realization));
    pl->realization.kind = bsg_payload_default_realization_kind(pl->pl_type);
    pl->realization.status = BSG_REALIZE_STATUS_CURRENT;
    pl->realization.stale_reason = BSG_PAYLOAD_STALE_NONE;
    pl->realization.generated_revision = pl->pl_revision;
}

static void
_payload_defaults(struct bsg_payload *pl)
{
    if (!pl)
	return;
    pl->pl_bounds = _no_bounds;
    pl->pl_export = _no_export;
    pl->pl_backend_prepare = _no_backend_prepare;
}


/* -----------------------------------------------------------------------
 * Core payload lifecycle
 * ----------------------------------------------------------------------- */

struct bsg_payload *
bsg_payload_create(bsg_payload_type type)
{
    struct bsg_payload *pl;
    BU_GET(pl, struct bsg_payload);
    memset(pl, 0, sizeof(*pl));
    pl->pl_type     = type;
    pl->pl_revision = 0;
    _payload_realization_init(pl);
    _payload_defaults(pl);
    return pl;
}


void
bsg_payload_free(struct bsg_payload *pl)
{
    if (!pl)
	return;
    if (pl->pl_free)
	pl->pl_free(pl);
    else
	BU_PUT(pl, struct bsg_payload);
}


void
bsg_payload_bump_revision(struct bsg_payload *pl)
{
    if (!pl)
	return;
    pl->pl_revision++;
    pl->realization.generated_revision = pl->pl_revision;
}


bsg_payload_realization_kind
bsg_payload_default_realization_kind(bsg_payload_type type)
{
    switch (type) {
	case BSG_PL_LINE_SET:
	case BSG_PL_TEXT:
	case BSG_PL_HUD_TEXT:
	case BSG_PL_AXES:
	case BSG_PL_GRID:
	case BSG_PL_ANNOTATION:
	    return BSG_REALIZE_WIREFRAME;
	case BSG_PL_POLYGON:
	    return BSG_REALIZE_POLYGON;
	case BSG_PL_MESH:
	    return BSG_REALIZE_LOD_MESH;
	case BSG_PL_CSG:
	    return BSG_REALIZE_ADAPTIVE_WIREFRAME;
	case BSG_PL_BREP:
	    return BSG_REALIZE_BREP_DISPLAY;
	case BSG_PL_SKETCH:
	case BSG_PL_EDIT_PREVIEW:
	    return BSG_REALIZE_EDIT_PREVIEW;
	case BSG_PL_IMAGE:
	case BSG_PL_FRAMEBUFFER:
	    return BSG_REALIZE_WIREFRAME;
	case BSG_PL_NONE:
	default:
	    return BSG_REALIZE_NONE;
    }
}


const char *
bsg_payload_realization_kind_name(bsg_payload_realization_kind kind)
{
    switch (kind) {
	case BSG_REALIZE_NONE: return "none";
	case BSG_REALIZE_WIREFRAME: return "wireframe";
	case BSG_REALIZE_EVALUATED_WIREFRAME: return "evaluated-wireframe";
	case BSG_REALIZE_SHADED_MESH: return "shaded-mesh";
	case BSG_REALIZE_ADAPTIVE_WIREFRAME: return "adaptive-wireframe";
	case BSG_REALIZE_LOD_MESH: return "lod-mesh";
	case BSG_REALIZE_BREP_DISPLAY: return "brep-display";
	case BSG_REALIZE_POLYGON: return "polygon";
	case BSG_REALIZE_EDIT_PREVIEW: return "edit-preview";
	default: return "unknown";
    }
}


const char *
bsg_payload_realization_status_name(bsg_payload_realization_status status)
{
    switch (status) {
	case BSG_REALIZE_STATUS_CURRENT: return "current";
	case BSG_REALIZE_STATUS_STALE: return "stale";
	case BSG_REALIZE_STATUS_FAILED: return "failed";
	default: return "unknown";
    }
}


const char *
bsg_payload_stale_reason_name(bsg_payload_stale_reason reason)
{
    switch (reason) {
	case BSG_PAYLOAD_STALE_NONE: return "current";
	case BSG_PAYLOAD_STALE_SOURCE_CHANGED: return "source-changed";
	case BSG_PAYLOAD_STALE_VIEW_INPUT_CHANGED: return "view-input-changed";
	case BSG_PAYLOAD_STALE_SETTINGS_CHANGED: return "settings-changed";
	case BSG_PAYLOAD_STALE_FORCED: return "forced";
	case BSG_PAYLOAD_STALE_UPDATE_FAILED: return "update-failed";
	default: return "unknown";
    }
}


void
bsg_payload_mark_source_revision(struct bsg_payload *pl,
				 uint64_t source_revision,
				 bsg_payload_stale_reason reason)
{
    if (!pl)
	return;
    pl->realization.source_revision = source_revision;
    if (source_revision != pl->realization.realized_source_revision)
	bsg_payload_mark_stale(pl,
		reason ? reason : BSG_PAYLOAD_STALE_SOURCE_CHANGED,
		NULL);
}


void
bsg_payload_mark_stale(struct bsg_payload *pl,
		       bsg_payload_stale_reason reason,
		       const char *explanation)
{
    if (!pl)
	return;
    pl->realization.status = (reason == BSG_PAYLOAD_STALE_UPDATE_FAILED) ?
	BSG_REALIZE_STATUS_FAILED : BSG_REALIZE_STATUS_STALE;
    pl->realization.stale_reason = reason ? reason : BSG_PAYLOAD_STALE_FORCED;
    if (explanation && explanation[0]) {
	bu_strlcpy(pl->realization.failure_reason, explanation,
		sizeof(pl->realization.failure_reason));
    } else {
	pl->realization.failure_reason[0] = '\0';
    }
}


const struct bsg_payload_realization *
bsg_payload_realization_state(const struct bsg_payload *pl)
{
    return pl ? &pl->realization : NULL;
}


int
bsg_payload_is_stale(const struct bsg_payload *pl)
{
    if (!pl)
	return 0;
    return pl->realization.status != BSG_REALIZE_STATUS_CURRENT ||
	pl->realization.source_revision != pl->realization.realized_source_revision ||
	pl->realization.inputs_revision != pl->realization.realized_inputs_revision;
}


int
bsg_payload_realize(struct bsg_payload *pl, struct bsg_view *v)
{
    if (!pl)
	return -1;

    uint64_t before = pl->pl_revision;
    int rc = 0;

    if (pl->pl_type == BSG_PL_EDIT_PREVIEW) {
	rc = bsg_payload_edit_preview_realize(pl, v);
	if (bsg_payload_edit_preview_stale_reason(pl) == BSG_EDIT_PREVIEW_STALE_UPDATE_FAILED) {
	    bsg_payload_mark_stale(pl, BSG_PAYLOAD_STALE_UPDATE_FAILED,
		    "edit-preview update failed");
	    return -1;
	}
	pl->realization.source_revision =
	    bsg_payload_edit_preview_get_data(pl)->source_revision;
	pl->realization.inputs_revision =
	    bsg_payload_edit_preview_get_data(pl)->inputs_revision;
    } else if (pl->pl_type == BSG_PL_SKETCH) {
	rc = bsg_payload_sketch_realize(pl, v);
    } else if (pl->pl_update) {
	pl->pl_update(pl, v);
	rc = (pl->pl_revision != before) ? 1 : 0;
    }

    if (rc < 0) {
	bsg_payload_mark_stale(pl, BSG_PAYLOAD_STALE_UPDATE_FAILED,
		"payload realization failed");
	return -1;
    }

    if ((pl->pl_type == BSG_PL_CSG || pl->pl_type == BSG_PL_BREP ||
		pl->pl_type == BSG_PL_NONE) &&
	    !pl->pl_update && bsg_payload_is_stale(pl))
	return 0;

    pl->realization.generated_revision = pl->pl_revision;
    pl->realization.realized_source_revision = pl->realization.source_revision;
    pl->realization.realized_inputs_revision = pl->realization.inputs_revision;
    pl->realization.status = BSG_REALIZE_STATUS_CURRENT;
    pl->realization.stale_reason = BSG_PAYLOAD_STALE_NONE;
    pl->realization.failure_reason[0] = '\0';
    pl->realization.has_bounds = 0;
    if (pl->pl_bounds && pl->pl_bounds(pl, &pl->realization.bmin, &pl->realization.bmax))
	pl->realization.has_bounds = 1;

    return (pl->pl_revision != before || rc > 0) ? 1 : 0;
}


/* -----------------------------------------------------------------------
 * Node ↔ payload binding
 * ----------------------------------------------------------------------- */

void
bsg_node_set_payload(bsg_node *node, struct bsg_payload *pl)
{
    if (!node)
	return;
    /* Free any existing payload */
    struct bsg_node_internal *ni = bsg_node_internal_ensure(node);
    if (ni->payload) {
	ni->payload->pl_owner = NULL;
	bsg_payload_free(ni->payload);
    }

    ni->payload = pl;
    bsg_node_set_payload_type(node, 0);
    if (pl) {
	pl->pl_owner = node;
	bsg_node_set_payload_type(node, _typed_payload_flags(pl->pl_type));
    }
}


struct bsg_payload *
bsg_node_get_payload(const bsg_node *node)
{
    if (!node)
	return NULL;
    return node->i ? node->i->payload : NULL;
}


void
bsg_scene_set_payload(bsg_scene_ref ref, struct bsg_payload *pl)
{
    bsg_node_set_payload((bsg_node *)ref.opaque, pl);
}


struct bsg_payload *
bsg_scene_payload(bsg_scene_ref ref)
{
    return bsg_node_get_payload((const bsg_node *)ref.opaque);
}


void
bsg_scene_payload_update(bsg_scene_ref ref, struct bsg_view *v)
{
    (void)bsg_payload_realize(bsg_scene_payload(ref), v);
}


/* -----------------------------------------------------------------------
 * TEXT payload
 * ----------------------------------------------------------------------- */

static void
_text_payload_free(struct bsg_payload *pl)
{
    if (!pl)
	return;
    struct bsg_label *label = pl->pl.text;
    if (label) {
	bu_vls_free(&label->label);
	BU_PUT(label, struct bsg_label);
    }
    BU_PUT(pl, struct bsg_payload);
}


struct bsg_payload *
bsg_payload_text_create(struct bsg_label *label)
{
    struct bsg_payload *pl = bsg_payload_create(BSG_PL_TEXT);
    if (!pl)
	return NULL;
    pl->pl.text  = label;
    pl->pl_free  = _text_payload_free;
    return pl;
}


struct bsg_label *
bsg_payload_text_get(struct bsg_payload *payload)
{
    if (!payload || payload->pl_type != BSG_PL_TEXT)
	return NULL;
    return payload->pl.text;
}


/* -----------------------------------------------------------------------
 * HUD_TEXT payload
 * ----------------------------------------------------------------------- */

struct bsg_payload *
bsg_payload_hud_text_create(struct bsg_label *label)
{
    struct bsg_payload *pl = bsg_payload_text_create(label);
    if (!pl)
	return NULL;
    pl->pl_type = BSG_PL_HUD_TEXT;
    pl->pl.hud_text = label;
    return pl;
}

struct bsg_label *
bsg_payload_hud_text_get(struct bsg_payload *payload)
{
    if (!payload || payload->pl_type != BSG_PL_HUD_TEXT)
	return NULL;
    return payload->pl.hud_text;
}


/* -----------------------------------------------------------------------
 * LINE_SET payload
 * ----------------------------------------------------------------------- */

static void
_line_set_payload_free(struct bsg_payload *pl)
{
    if (!pl)
	return;
    struct bsg_payload_line_set *ls = pl->pl.line_set;
    if (ls) {
	if (ls->points)
	    bu_free(ls->points, "payload line-set points");
	if (ls->cmds)
	    bu_free(ls->cmds, "payload line-set cmds");
	BU_PUT(ls, struct bsg_payload_line_set);
    }
    BU_PUT(pl, struct bsg_payload);
}

static int
_line_set_payload_bounds(struct bsg_payload *pl, point_t *bmin, point_t *bmax)
{
    if (!pl || !pl->pl.line_set || !pl->pl.line_set->point_cnt)
	return 0;
    VSETALL((*bmin), INFINITY);
    VSETALL((*bmax), -INFINITY);
    for (size_t i = 0; i < pl->pl.line_set->point_cnt; i++) {
	VMINMAX((*bmin), (*bmax), pl->pl.line_set->points[i]);
    }
    return 1;
}

struct bsg_payload *
bsg_payload_line_set_create(point_t *points, const int *cmds, size_t point_cnt)
{
    struct bsg_payload *pl = bsg_payload_create(BSG_PL_LINE_SET);
    if (!pl)
	return NULL;

    struct bsg_payload_line_set *ls;
    BU_GET(ls, struct bsg_payload_line_set);
    memset(ls, 0, sizeof(*ls));
    ls->point_cnt = point_cnt;
    if (point_cnt) {
	ls->points = (point_t *)bu_calloc(point_cnt, sizeof(point_t), "payload line-set points");
	ls->cmds = (int *)bu_calloc(point_cnt, sizeof(int), "payload line-set cmds");
	for (size_t i = 0; i < point_cnt; i++) {
	    if (points)
		VMOVE(ls->points[i], points[i]);
	    ls->cmds[i] = cmds ? cmds[i] : ((i % 2) ? BSG_GEOMETRY_LINE_DRAW : BSG_GEOMETRY_LINE_MOVE);
	}
    }

    pl->pl.line_set = ls;
    pl->pl_free = _line_set_payload_free;
    pl->pl_bounds = _line_set_payload_bounds;
    return pl;
}

struct bsg_payload_line_set *
bsg_payload_line_set_get(struct bsg_payload *payload)
{
    if (!payload || payload->pl_type != BSG_PL_LINE_SET)
	return NULL;
    return payload->pl.line_set;
}


int
bsg_payload_line_set_append_segments(struct bsg_payload *payload,
	const point_t *points, const int *cmds, size_t add_cnt)
{
    if (!payload || payload->pl_type != BSG_PL_LINE_SET || !add_cnt)
	return 0;
    struct bsg_payload_line_set *ls = payload->pl.line_set;
    if (!ls)
	return 0;
    size_t old_cnt = ls->point_cnt;
    size_t new_cnt = old_cnt + add_cnt;
    point_t *new_pts;
    int *new_cmds;
    if (ls->points) {
	new_pts = (point_t *)bu_realloc(ls->points, new_cnt * sizeof(point_t), "line-set append points");
    } else {
	new_pts = (point_t *)bu_calloc(new_cnt, sizeof(point_t), "line-set append points");
    }
    if (ls->cmds) {
	new_cmds = (int *)bu_realloc(ls->cmds, new_cnt * sizeof(int), "line-set append cmds");
    } else {
	new_cmds = (int *)bu_calloc(new_cnt, sizeof(int), "line-set append cmds");
    }
    if (!new_pts || !new_cmds) {
	if (new_pts) bu_free(new_pts, "line-set append points");
	if (new_cmds) bu_free(new_cmds, "line-set append cmds");
	return 0;
    }
    ls->points = new_pts;
    ls->cmds = new_cmds;
    for (size_t i = 0; i < add_cnt; i++) {
	if (points)
	    VMOVE(ls->points[old_cnt + i], points[i]);
	ls->cmds[old_cnt + i] = cmds ? cmds[i] : BSG_GEOMETRY_LINE_DRAW;
    }
    ls->point_cnt = new_cnt;
    bsg_payload_bump_revision(payload);
    return 1;
}

int
bsg_payload_line_set_replace(struct bsg_payload *payload,
	const point_t *points, const int *cmds, size_t point_cnt)
{
    if (!payload || payload->pl_type != BSG_PL_LINE_SET)
	return 0;
    struct bsg_payload_line_set *ls = payload->pl.line_set;
    if (!ls)
	return 0;
    if (ls->points)
	bu_free(ls->points, "line-set replace points");
    if (ls->cmds)
	bu_free(ls->cmds, "line-set replace cmds");
    ls->points = NULL;
    ls->cmds = NULL;
    ls->point_cnt = 0;
    if (point_cnt) {
	ls->points = (point_t *)bu_calloc(point_cnt, sizeof(point_t), "line-set replace points");
	ls->cmds = (int *)bu_calloc(point_cnt, sizeof(int), "line-set replace cmds");
	for (size_t i = 0; i < point_cnt; i++) {
	    if (points)
		VMOVE(ls->points[i], points[i]);
	    ls->cmds[i] = cmds ? cmds[i] : ((i % 2) ? BSG_GEOMETRY_LINE_DRAW : BSG_GEOMETRY_LINE_MOVE);
	}
	ls->point_cnt = point_cnt;
    }
    bsg_payload_bump_revision(payload);
    return 1;
}

int
bsg_payload_line_set_clear(struct bsg_payload *payload)
{
    if (!payload || payload->pl_type != BSG_PL_LINE_SET)
	return 0;
    struct bsg_payload_line_set *ls = payload->pl.line_set;
    if (!ls)
	return 0;
    if (ls->points)
	bu_free(ls->points, "line-set clear points");
    if (ls->cmds)
	bu_free(ls->cmds, "line-set clear cmds");
    ls->points = NULL;
    ls->cmds = NULL;
    ls->point_cnt = 0;
    bsg_payload_bump_revision(payload);
    return 1;
}

size_t
bsg_payload_line_set_point_count(const struct bsg_payload *payload)
{
    if (!payload || payload->pl_type != BSG_PL_LINE_SET || !payload->pl.line_set)
	return 0;
    return payload->pl.line_set->point_cnt;
}

int
bsg_payload_line_set_cmd_at(const struct bsg_payload *payload, size_t idx)
{
    if (!payload || payload->pl_type != BSG_PL_LINE_SET || !payload->pl.line_set)
	return -1;
    struct bsg_payload_line_set *ls = payload->pl.line_set;
    if (idx >= ls->point_cnt || !ls->cmds)
	return -1;
    return ls->cmds[idx];
}


/* -----------------------------------------------------------------------
 * POLYGON payload
 * ----------------------------------------------------------------------- */

static void
_polygon_payload_free(struct bsg_payload *pl)
{
    if (!pl)
	return;
    struct bsg_polygon *poly = pl->pl.polygon;
    if (poly) {
	bg_polygon_free(&poly->polygon);
	BU_PUT(poly, struct bsg_polygon);
    }
    BU_PUT(pl, struct bsg_payload);
}

static int
_polygon_payload_bounds(struct bsg_payload *pl, point_t *bmin, point_t *bmax)
{
    if (!pl || !pl->pl.polygon || !pl->pl.polygon->polygon.num_contours)
	return 0;
    VSETALL((*bmin), INFINITY);
    VSETALL((*bmax), -INFINITY);
    struct bsg_polygon *poly = pl->pl.polygon;
    for (size_t i = 0; i < poly->polygon.num_contours; i++) {
	struct bg_poly_contour *c = &poly->polygon.contour[i];
	for (size_t j = 0; j < c->num_points; j++) {
	    VMINMAX((*bmin), (*bmax), c->point[j]);
	}
    }
    return 1;
}

static void
_polygon_payload_update(struct bsg_payload *pl, struct bsg_view *v)
{
    if (!pl || !pl->pl_owner)
	return;
    bsg_node *owner_node = (bsg_node *)pl->pl_owner;
    struct bsg_view *owner = bsg_node_get_view(owner_node);
    (void)bsg_update_polygon(owner_node, owner ? owner : v, 0);
}

struct bsg_payload *
bsg_payload_polygon_create(struct bsg_polygon *polygon)
{
    struct bsg_payload *pl = bsg_payload_create(BSG_PL_POLYGON);
    if (!pl)
	return NULL;
    pl->pl.polygon = polygon;
    pl->pl_free = _polygon_payload_free;
    pl->pl_update = _polygon_payload_update;
    pl->pl_bounds = _polygon_payload_bounds;
    return pl;
}

struct bsg_polygon *
bsg_payload_polygon_get(struct bsg_payload *payload)
{
    if (!payload || payload->pl_type != BSG_PL_POLYGON)
	return NULL;
    return payload->pl.polygon;
}


/* -----------------------------------------------------------------------
 * MESH / CSG / BREP payloads
 * ----------------------------------------------------------------------- */

static void
_mesh_payload_free(struct bsg_payload *pl)
{
    if (!pl)
	return;
    if (pl->pl.mesh)
	bsg_mesh_lod_destroy(pl->pl.mesh);
    BU_PUT(pl, struct bsg_payload);
}

struct bsg_payload *
bsg_payload_mesh_create(struct bsg_mesh_lod *mesh)
{
    struct bsg_payload *pl = bsg_payload_create(BSG_PL_MESH);
    if (!pl)
	return NULL;
    pl->pl.mesh = mesh;
    pl->pl_free = _mesh_payload_free;
    return pl;
}

struct bsg_mesh_lod *
bsg_payload_mesh_get(struct bsg_payload *payload)
{
    if (!payload || payload->pl_type != BSG_PL_MESH)
	return NULL;
    return payload->pl.mesh;
}

struct bsg_payload *
bsg_payload_csg_create(void *opaque)
{
    struct bsg_payload *pl = bsg_payload_create(BSG_PL_CSG);
    if (!pl)
	return NULL;
    pl->pl.csg = opaque;
    return pl;
}

struct bsg_payload *
bsg_payload_brep_create(void *opaque)
{
    struct bsg_payload *pl = bsg_payload_create(BSG_PL_BREP);
    if (!pl)
	return NULL;
    pl->pl.brep = opaque;
    return pl;
}

/* -----------------------------------------------------------------------
 * IMAGE / FRAMEBUFFER payloads
 * ----------------------------------------------------------------------- */

static void
_image_payload_free(struct bsg_payload *pl)
{
    if (!pl)
	return;
    struct bsg_payload_image *img = pl->pl.image;
    if (img) {
	if (img->pixels)
	    bu_free(img->pixels, "payload image pixels");
	BU_PUT(img, struct bsg_payload_image);
    }
    BU_PUT(pl, struct bsg_payload);
}

struct bsg_payload *
bsg_payload_image_create(size_t width, size_t height, size_t channels, const unsigned char *pixels)
{
    struct bsg_payload *pl = bsg_payload_create(BSG_PL_IMAGE);
    if (!pl)
	return NULL;

    struct bsg_payload_image *img;
    BU_GET(img, struct bsg_payload_image);
    memset(img, 0, sizeof(*img));
    img->width = width;
    img->height = height;
    img->channels = channels;
    if (width && height && channels && pixels) {
	size_t psize = width * height * channels;
	img->pixels = (unsigned char *)bu_malloc(psize, "payload image pixels");
	memcpy(img->pixels, pixels, psize);
    }

    pl->pl.image = img;
    pl->pl_free = _image_payload_free;
    return pl;
}

struct bsg_payload_image *
bsg_payload_image_get(struct bsg_payload *payload)
{
    if (!payload || payload->pl_type != BSG_PL_IMAGE)
	return NULL;
    return payload->pl.image;
}

static void
_framebuffer_payload_free(struct bsg_payload *pl)
{
    if (!pl)
	return;
    if (pl->pl.framebuffer)
	BU_PUT(pl->pl.framebuffer, struct bsg_payload_framebuffer);
    BU_PUT(pl, struct bsg_payload);
}

struct bsg_payload *
bsg_payload_framebuffer_create(struct fb *fbp, int mode)
{
    struct bsg_payload *pl = bsg_payload_create(BSG_PL_FRAMEBUFFER);
    if (!pl)
	return NULL;
    struct bsg_payload_framebuffer *fbpl;
    BU_GET(fbpl, struct bsg_payload_framebuffer);
    fbpl->fbp = fbp;
    fbpl->mode = mode;
    pl->pl.framebuffer = fbpl;
    pl->pl_free = _framebuffer_payload_free;
    return pl;
}

struct bsg_payload_framebuffer *
bsg_payload_framebuffer_get(struct bsg_payload *payload)
{
    if (!payload || payload->pl_type != BSG_PL_FRAMEBUFFER)
	return NULL;
    return payload->pl.framebuffer;
}


/* -----------------------------------------------------------------------
 * AXES payload
 * ----------------------------------------------------------------------- */

static void
_axes_payload_free(struct bsg_payload *pl)
{
    if (!pl)
	return;
    struct bsg_axes *axes = pl->pl.axes;
    if (axes)
	BU_PUT(axes, struct bsg_axes);
    BU_PUT(pl, struct bsg_payload);
}


struct bsg_payload *
bsg_payload_axes_create(struct bsg_axes *axes)
{
    struct bsg_payload *pl = bsg_payload_create(BSG_PL_AXES);
    if (!pl)
	return NULL;
    pl->pl.axes  = axes;
    pl->pl_free  = _axes_payload_free;
    return pl;
}


struct bsg_axes *
bsg_payload_axes_get(struct bsg_payload *payload)
{
    if (!payload || payload->pl_type != BSG_PL_AXES)
	return NULL;
    return payload->pl.axes;
}


/* -----------------------------------------------------------------------
 * GRID payload
 * ----------------------------------------------------------------------- */

static void
_grid_payload_free(struct bsg_payload *pl)
{
    if (!pl)
	return;
    if (pl->pl.grid)
	BU_PUT(pl->pl.grid, struct bsg_grid_state);
    BU_PUT(pl, struct bsg_payload);
}

struct bsg_payload *
bsg_payload_grid_create(const struct bsg_grid_state *grid)
{
    struct bsg_payload *pl = bsg_payload_create(BSG_PL_GRID);
    if (!pl)
	return NULL;
    struct bsg_grid_state *g;
    BU_GET(g, struct bsg_grid_state);
    memset(g, 0, sizeof(*g));
    if (grid)
	memcpy(g, grid, sizeof(*g));
    pl->pl.grid = g;
    pl->pl_free = _grid_payload_free;
    return pl;
}

struct bsg_grid_state *
bsg_payload_grid_get(struct bsg_payload *payload)
{
    if (!payload || payload->pl_type != BSG_PL_GRID)
	return NULL;
    return payload->pl.grid;
}


/* -----------------------------------------------------------------------
 * Edit-preview payload
 * ----------------------------------------------------------------------- */

static void *
_edit_preview_ctx(struct bsg_edit_preview_source *d)
{
    if (!d)
	return NULL;
    return (d->preview_ctx) ? d->preview_ctx : d->editor_ctx;
}

static bsg_payload_stale_reason
_edit_preview_payload_stale_reason(bsg_edit_preview_stale_reason reason,
				   bsg_payload_stale_reason default_reason)
{
    switch (reason) {
	case BSG_EDIT_PREVIEW_STALE_NONE:
	    return default_reason;
	case BSG_EDIT_PREVIEW_STALE_SOURCE_CHANGED:
	    return BSG_PAYLOAD_STALE_SOURCE_CHANGED;
	case BSG_EDIT_PREVIEW_STALE_VIEW_INPUT_CHANGED:
	    return BSG_PAYLOAD_STALE_VIEW_INPUT_CHANGED;
	case BSG_EDIT_PREVIEW_STALE_SETTINGS_CHANGED:
	    return BSG_PAYLOAD_STALE_SETTINGS_CHANGED;
	case BSG_EDIT_PREVIEW_STALE_FORCED:
	    return BSG_PAYLOAD_STALE_FORCED;
	case BSG_EDIT_PREVIEW_STALE_UPDATE_FAILED:
	    return BSG_PAYLOAD_STALE_UPDATE_FAILED;
	default:
	    return default_reason ? default_reason : BSG_PAYLOAD_STALE_FORCED;
    }
}

static void
_edit_preview_payload_sync_revisions(struct bsg_payload *pl,
				     const struct bsg_edit_preview_source *d)
{
    if (!pl || !d)
	return;
    pl->realization.source_revision = d->source_revision;
    pl->realization.inputs_revision = d->inputs_revision;
    pl->realization.realized_source_revision =
	d->last_realized_source_revision;
    pl->realization.realized_inputs_revision =
	d->last_realized_inputs_revision;
}

static int
_edit_preview_payload_mark_generic_stale(struct bsg_payload *pl,
					 bsg_payload_stale_reason reason,
					 const char *explanation)
{
    if (!pl)
	return 0;
    bsg_payload_realization_status old_status = pl->realization.status;
    bsg_payload_stale_reason old_reason = pl->realization.stale_reason;
    char old_failure[sizeof(pl->realization.failure_reason)];
    bu_strlcpy(old_failure, pl->realization.failure_reason, sizeof(old_failure));

    bsg_payload_mark_stale(pl, reason, explanation);
    return old_status != pl->realization.status ||
	old_reason != pl->realization.stale_reason ||
	!BU_STR_EQUAL(old_failure, pl->realization.failure_reason);
}

static int
_edit_preview_payload_set_current(struct bsg_payload *pl,
				  const struct bsg_edit_preview_source *d)
{
    if (!pl || !d)
	return 0;

    bsg_payload_realization_status old_status = pl->realization.status;
    bsg_payload_stale_reason old_reason = pl->realization.stale_reason;
    uint64_t old_source = pl->realization.source_revision;
    uint64_t old_inputs = pl->realization.inputs_revision;
    uint64_t old_realized_source =
	pl->realization.realized_source_revision;
    uint64_t old_realized_inputs =
	pl->realization.realized_inputs_revision;
    char old_failure[sizeof(pl->realization.failure_reason)];
    bu_strlcpy(old_failure, pl->realization.failure_reason, sizeof(old_failure));

    _edit_preview_payload_sync_revisions(pl, d);
    pl->realization.status = BSG_REALIZE_STATUS_CURRENT;
    pl->realization.stale_reason = BSG_PAYLOAD_STALE_NONE;
    pl->realization.failure_reason[0] = '\0';

    if (old_status != pl->realization.status ||
	    old_reason != pl->realization.stale_reason ||
	    old_source != pl->realization.source_revision ||
	    old_inputs != pl->realization.inputs_revision ||
	    old_realized_source != pl->realization.realized_source_revision ||
	    old_realized_inputs != pl->realization.realized_inputs_revision ||
	    !BU_STR_EQUAL(old_failure, pl->realization.failure_reason)) {
	bsg_payload_bump_revision(pl);
	return 1;
    }

    pl->realization.generated_revision = pl->pl_revision;
    return 0;
}

static void
_edit_preview_payload_free(struct bsg_payload *pl)
{
    if (!pl)
	return;
    struct bsg_edit_preview_source *d = pl->pl.edit_preview;
    if (d) {
	if (d->owns_preview_ctx && d->free_cb && d->preview_ctx)
	    d->free_cb(d->preview_ctx);
	BU_PUT(d, struct bsg_edit_preview_source);
    }
    BU_PUT(pl, struct bsg_payload);
}

static void
_edit_preview_payload_update(struct bsg_payload *pl, struct bsg_view *v)
{
    if (!pl || pl->pl_type != BSG_PL_EDIT_PREVIEW || !pl->pl.edit_preview)
	return;

    struct bsg_edit_preview_source *d = pl->pl.edit_preview;
    void *ctx = _edit_preview_ctx(d);
    if (!ctx) {
	if (!d->update_cb && !d->revision_cb && !bsg_payload_edit_preview_is_stale(pl))
	    return;
	d->stale_reason = BSG_EDIT_PREVIEW_STALE_UPDATE_FAILED;
	_edit_preview_payload_sync_revisions(pl, d);
	if (_edit_preview_payload_mark_generic_stale(pl,
		BSG_PAYLOAD_STALE_UPDATE_FAILED,
		"edit-preview update failed"))
	    bsg_payload_bump_revision(pl);
	return;
    }

    uint64_t prev_preview_rev = d->last_realized_revision;
    int updated = 0;

    if (d->update_cb)
	updated = d->update_cb(ctx, v);

    uint64_t preview_rev = prev_preview_rev;
    if (d->revision_cb)
	preview_rev = d->revision_cb(ctx);
    else if (updated)
	preview_rev = prev_preview_rev + 1;

    if (updated && preview_rev == prev_preview_rev)
	preview_rev++;

    if (preview_rev != prev_preview_rev) {
	d->last_realized_revision = preview_rev;
	bsg_payload_bump_revision(pl);
    }

    if (updated < 0) {
	d->stale_reason = BSG_EDIT_PREVIEW_STALE_UPDATE_FAILED;
	_edit_preview_payload_sync_revisions(pl, d);
	if (_edit_preview_payload_mark_generic_stale(pl,
		BSG_PAYLOAD_STALE_UPDATE_FAILED,
		"edit-preview update failed"))
	    bsg_payload_bump_revision(pl);
	return;
    }

    d->last_realized_source_revision = d->source_revision;
    d->last_realized_inputs_revision = d->inputs_revision;
    d->stale_reason = BSG_EDIT_PREVIEW_STALE_NONE;
    (void)_edit_preview_payload_set_current(pl, d);
}

static int
_edit_preview_payload_bounds(struct bsg_payload *pl, point_t *bmin, point_t *bmax)
{
    return bsg_payload_edit_preview_bounds(pl, bmin, bmax);
}


struct bsg_payload *
bsg_payload_edit_preview_create(void *editor_ctx, void *aux_ctx)
{
    struct bsg_payload *pl = bsg_payload_create(BSG_PL_EDIT_PREVIEW);
    if (!pl)
	return NULL;

    struct bsg_edit_preview_source *d;
    BU_GET(d, struct bsg_edit_preview_source);
    d->editor_ctx  = editor_ctx;
    d->aux_ctx     = aux_ctx;
    d->preview_ctx = editor_ctx;
    d->owns_preview_ctx = 0;
    d->source_revision = 0;
    d->inputs_revision = 0;
    d->last_realized_revision = 0;
    d->last_realized_source_revision = 0;
    d->last_realized_inputs_revision = 0;
    d->stale_reason = BSG_EDIT_PREVIEW_STALE_NONE;
    d->revision_cb = NULL;
    d->update_cb   = NULL;
    d->bounds_cb   = NULL;
    d->pick_cb     = NULL;
    d->snap_cb     = NULL;
    d->free_cb     = NULL;

    pl->pl.edit_preview = d;
    pl->pl_free = _edit_preview_payload_free;
    pl->pl_update = _edit_preview_payload_update;
    pl->pl_bounds = _edit_preview_payload_bounds;
    return pl;
}


struct bsg_edit_preview_source *
bsg_payload_edit_preview_get_data(struct bsg_payload *payload)
{
    if (!payload || payload->pl_type != BSG_PL_EDIT_PREVIEW)
	return NULL;
    return payload->pl.edit_preview;
}


int
bsg_payload_edit_preview_set_ops(struct bsg_payload *payload,
	void *preview_ctx,
	int owns_preview_ctx,
	bsg_edit_preview_revision_cb_t revision_cb,
	bsg_edit_preview_update_cb_t   update_cb,
	bsg_edit_preview_bounds_cb_t   bounds_cb,
	bsg_edit_preview_pick_cb_t     pick_cb,
	bsg_edit_preview_snap_cb_t     snap_cb,
	bsg_edit_preview_free_cb_t     free_cb)
{
    struct bsg_edit_preview_source *d = bsg_payload_edit_preview_get_data(payload);
    if (!d)
	return 0;

    uint64_t old_pl_revision = payload->pl_revision;
    void *old_preview_ctx = d->preview_ctx;
    int old_owns_preview_ctx = d->owns_preview_ctx;
    bsg_edit_preview_free_cb_t old_free_cb = d->free_cb;

    if (old_owns_preview_ctx && old_free_cb && old_preview_ctx &&
	    old_preview_ctx != preview_ctx)
	old_free_cb(old_preview_ctx);

    d->preview_ctx = preview_ctx;
    d->owns_preview_ctx = owns_preview_ctx;
    d->revision_cb   = revision_cb;
    d->update_cb     = update_cb;
    d->bounds_cb     = bounds_cb;
    d->pick_cb       = pick_cb;
    d->snap_cb       = snap_cb;
    d->free_cb       = free_cb;
    d->last_realized_revision =
	(revision_cb) ? revision_cb(_edit_preview_ctx(d)) : 0;
    d->source_revision = d->last_realized_revision;
    d->last_realized_source_revision = d->source_revision;
    d->last_realized_inputs_revision = d->inputs_revision;
    d->stale_reason = BSG_EDIT_PREVIEW_STALE_NONE;
    (void)_edit_preview_payload_set_current(payload, d);
    if (payload->pl_revision == old_pl_revision)
	bsg_payload_bump_revision(payload);

    return 1;
}


uint64_t
bsg_payload_edit_preview_revision(struct bsg_payload *payload)
{
    struct bsg_edit_preview_source *d = bsg_payload_edit_preview_get_data(payload);
    if (!d)
	return 0;

    if (d->revision_cb)
	return d->revision_cb(_edit_preview_ctx(d));

    return d->last_realized_revision;
}


int
bsg_payload_edit_preview_mark_source_revision(struct bsg_payload *payload,
					      uint64_t source_revision,
					      bsg_edit_preview_stale_reason reason)
{
    struct bsg_edit_preview_source *d = bsg_payload_edit_preview_get_data(payload);
    if (!d)
	return 0;
    uint64_t old_pl_revision = payload->pl_revision;
    uint64_t old_source = payload->realization.source_revision;
    uint64_t old_realized_source =
	payload->realization.realized_source_revision;
    d->source_revision = source_revision;
    _edit_preview_payload_sync_revisions(payload, d);
    int changed = old_source != payload->realization.source_revision ||
	old_realized_source != payload->realization.realized_source_revision;
    if (source_revision != d->last_realized_source_revision) {
	d->stale_reason = reason ? reason : BSG_EDIT_PREVIEW_STALE_SOURCE_CHANGED;
	bsg_payload_stale_reason stale_reason =
	    _edit_preview_payload_stale_reason(d->stale_reason,
		    BSG_PAYLOAD_STALE_SOURCE_CHANGED);
	if (_edit_preview_payload_mark_generic_stale(payload, stale_reason, NULL))
	    changed = 1;
    }
    if (changed)
	bsg_payload_bump_revision(payload);
    return payload->pl_revision != old_pl_revision;
}


int
bsg_payload_edit_preview_mark_inputs_revision(struct bsg_payload *payload,
					      uint64_t inputs_revision,
					      bsg_edit_preview_stale_reason reason)
{
    struct bsg_edit_preview_source *d = bsg_payload_edit_preview_get_data(payload);
    if (!d)
	return 0;
    uint64_t old_pl_revision = payload->pl_revision;
    uint64_t old_inputs = payload->realization.inputs_revision;
    uint64_t old_realized_inputs =
	payload->realization.realized_inputs_revision;
    d->inputs_revision = inputs_revision;
    _edit_preview_payload_sync_revisions(payload, d);
    int changed = old_inputs != payload->realization.inputs_revision ||
	old_realized_inputs != payload->realization.realized_inputs_revision;
    if (inputs_revision != d->last_realized_inputs_revision) {
	d->stale_reason = reason ? reason : BSG_EDIT_PREVIEW_STALE_VIEW_INPUT_CHANGED;
	bsg_payload_stale_reason stale_reason =
	    _edit_preview_payload_stale_reason(d->stale_reason,
		    BSG_PAYLOAD_STALE_VIEW_INPUT_CHANGED);
	if (_edit_preview_payload_mark_generic_stale(payload, stale_reason, NULL))
	    changed = 1;
    }
    if (changed)
	bsg_payload_bump_revision(payload);
    return payload->pl_revision != old_pl_revision;
}


int
bsg_payload_edit_preview_is_stale(struct bsg_payload *payload)
{
    struct bsg_edit_preview_source *d = bsg_payload_edit_preview_get_data(payload);
    if (!d)
	return 0;
    return d->stale_reason != BSG_EDIT_PREVIEW_STALE_NONE ||
	d->source_revision != d->last_realized_source_revision ||
	d->inputs_revision != d->last_realized_inputs_revision;
}


bsg_edit_preview_stale_reason
bsg_payload_edit_preview_stale_reason(struct bsg_payload *payload)
{
    struct bsg_edit_preview_source *d = bsg_payload_edit_preview_get_data(payload);
    if (!d)
	return BSG_EDIT_PREVIEW_STALE_NONE;
    if (d->stale_reason != BSG_EDIT_PREVIEW_STALE_NONE)
	return d->stale_reason;
    if (d->source_revision != d->last_realized_source_revision)
	return BSG_EDIT_PREVIEW_STALE_SOURCE_CHANGED;
    if (d->inputs_revision != d->last_realized_inputs_revision)
	return BSG_EDIT_PREVIEW_STALE_VIEW_INPUT_CHANGED;
    return BSG_EDIT_PREVIEW_STALE_NONE;
}


const char *
bsg_payload_edit_preview_stale_reason_name(bsg_edit_preview_stale_reason reason)
{
    return bsg_edit_preview_stale_reason_name(reason);
}


const char *
bsg_edit_preview_stale_reason_name(bsg_edit_preview_stale_reason reason)
{
    switch (reason) {
	case BSG_EDIT_PREVIEW_STALE_NONE:
	    return "current";
	case BSG_EDIT_PREVIEW_STALE_SOURCE_CHANGED:
	    return "source-changed";
	case BSG_EDIT_PREVIEW_STALE_VIEW_INPUT_CHANGED:
	    return "view-input-changed";
	case BSG_EDIT_PREVIEW_STALE_SETTINGS_CHANGED:
	    return "settings-changed";
	case BSG_EDIT_PREVIEW_STALE_FORCED:
	    return "forced";
	case BSG_EDIT_PREVIEW_STALE_UPDATE_FAILED:
	    return "update-failed";
	default:
	    return "unknown";
    }
}


uint64_t
bsg_payload_edit_preview_last_realized_source_revision(struct bsg_payload *payload)
{
    struct bsg_edit_preview_source *d = bsg_payload_edit_preview_get_data(payload);
    return d ? d->last_realized_source_revision : 0;
}


uint64_t
bsg_payload_edit_preview_last_realized_inputs_revision(struct bsg_payload *payload)
{
    struct bsg_edit_preview_source *d = bsg_payload_edit_preview_get_data(payload);
    return d ? d->last_realized_inputs_revision : 0;
}


int
bsg_payload_edit_preview_realize(struct bsg_payload *payload, struct bsg_view *v)
{
    if (!payload || payload->pl_type != BSG_PL_EDIT_PREVIEW)
	return -1;

    uint64_t rev_before = payload->pl_revision;
    _edit_preview_payload_update(payload, v);
    if (bsg_payload_edit_preview_stale_reason(payload) ==
	    BSG_EDIT_PREVIEW_STALE_UPDATE_FAILED)
	return -1;

    return (payload->pl_revision != rev_before) ? 1 : 0;
}


int
bsg_payload_edit_preview_bounds(struct bsg_payload *payload, point_t *bmin, point_t *bmax)
{
    struct bsg_edit_preview_source *d = bsg_payload_edit_preview_get_data(payload);
    if (!d || !d->bounds_cb || !bmin || !bmax)
	return 0;
    return d->bounds_cb(_edit_preview_ctx(d), bmin, bmax);
}


int
bsg_payload_edit_preview_pick(struct bsg_payload *payload, struct bsg_view *v,
	int x, int y, void *pick_out)
{
    struct bsg_edit_preview_source *d = bsg_payload_edit_preview_get_data(payload);
    if (!d || !d->pick_cb)
	return 0;
    return d->pick_cb(_edit_preview_ctx(d), v, x, y, pick_out);
}


int
bsg_payload_edit_preview_snap(struct bsg_payload *payload, struct bsg_view *v,
	const point_t sample_pt, point_t out_pt)
{
    struct bsg_edit_preview_source *d = bsg_payload_edit_preview_get_data(payload);
    if (!d || !d->snap_cb || !sample_pt || !out_pt)
	return 0;
    return d->snap_cb(_edit_preview_ctx(d), v, sample_pt, out_pt);
}


/* -----------------------------------------------------------------------
 * SKETCH payload
 * ----------------------------------------------------------------------- */

static void
_sketch_payload_free(struct bsg_payload *pl)
{
    if (!pl)
	return;
    struct bsg_sketch_live_data *d = pl->pl.sketch;
    if (d) {
	if (d->owns_live_ctx && d->free_cb && d->live_ctx)
	    d->free_cb(d->live_ctx);
	if (d->geometry)
	    bsg_payload_free(d->geometry);
	BU_PUT(d, struct bsg_sketch_live_data);
    }
    BU_PUT(pl, struct bsg_payload);
}

static void *
_sketch_live_ctx(struct bsg_sketch_live_data *d)
{
    if (!d)
	return NULL;
    return (d->live_ctx) ? d->live_ctx : d->rt_edit_ptr;
}

static void
_sketch_payload_update(struct bsg_payload *pl, struct bsg_view *v)
{
    if (!pl || pl->pl_type != BSG_PL_SKETCH || !pl->pl.sketch)
	return;

    struct bsg_sketch_live_data *d = pl->pl.sketch;
    void *ctx = _sketch_live_ctx(d);
    if (!ctx)
	return;

    uint64_t prev_live_rev = d->last_realized_revision;
    int updated = 0;

    if (d->update_cb)
	updated = d->update_cb(ctx, v);

    uint64_t live_rev = prev_live_rev;
    if (d->revision_cb)
	live_rev = d->revision_cb(ctx);
    else if (updated)
	live_rev = prev_live_rev + 1;

    /* If update_cb reports a change but revision_cb did not advance, force a
     * monotonic increment so payload_revision tracks realized updates. */
    if (updated && live_rev == prev_live_rev)
	live_rev++;

    if (live_rev != prev_live_rev) {
	d->last_realized_revision = live_rev;
	bsg_payload_bump_revision(pl);
    }
}

static int
_sketch_payload_bounds(struct bsg_payload *pl, point_t *bmin, point_t *bmax)
{
    return bsg_payload_sketch_bounds(pl, bmin, bmax);
}


struct bsg_payload *
bsg_payload_sketch_create(void *rt_edit_ptr, void *grid_ptr)
{
    struct bsg_payload *pl = bsg_payload_create(BSG_PL_SKETCH);
    if (!pl)
	return NULL;

    struct bsg_sketch_live_data *d;
    BU_GET(d, struct bsg_sketch_live_data);
    memset(d, 0, sizeof(*d));
    d->rt_edit_ptr = rt_edit_ptr;
    d->grid_ptr    = grid_ptr;
    d->live_ctx = rt_edit_ptr;
    d->owns_live_ctx = 0;
    d->geometry = bsg_payload_line_set_create(NULL, NULL, 0);
    if (!d->geometry) {
	BU_PUT(d, struct bsg_sketch_live_data);
	BU_PUT(pl, struct bsg_payload);
	return NULL;
    }
    d->last_realized_revision = 0;
    d->revision_cb = NULL;
    d->update_cb = NULL;
    d->bounds_cb = NULL;
    d->pick_cb = NULL;
    d->snap_cb = NULL;
    d->free_cb = NULL;

    pl->pl.sketch = d;
    pl->pl_free   = _sketch_payload_free;
    pl->pl_update = _sketch_payload_update;
    pl->pl_bounds = _sketch_payload_bounds;
    return pl;
}


struct bsg_sketch_live_data *
bsg_payload_sketch_get_data(struct bsg_payload *payload)
{
    if (!payload || payload->pl_type != BSG_PL_SKETCH)
	return NULL;
    return payload->pl.sketch;
}

struct bsg_payload *
bsg_payload_sketch_geometry(struct bsg_payload *payload)
{
    struct bsg_sketch_live_data *d = bsg_payload_sketch_get_data(payload);
    return d ? d->geometry : NULL;
}

int
bsg_payload_sketch_set_live_ops(struct bsg_payload *payload,
	void *live_ctx,
	int owns_live_ctx,
	bsg_sketch_live_revision_cb_t revision_cb,
	bsg_sketch_live_update_cb_t update_cb,
	bsg_sketch_live_bounds_cb_t bounds_cb,
	bsg_sketch_live_pick_cb_t pick_cb,
	bsg_sketch_live_snap_cb_t snap_cb,
	bsg_sketch_live_free_cb_t free_cb)
{
    struct bsg_sketch_live_data *d = bsg_payload_sketch_get_data(payload);
    if (!d)
	return 0;

    uint64_t old_pl_revision = payload->pl_revision;
    void *old_live_ctx = d->live_ctx;
    int old_owns_live_ctx = d->owns_live_ctx;
    bsg_sketch_live_free_cb_t old_free_cb = d->free_cb;

    if (old_owns_live_ctx && old_free_cb && old_live_ctx &&
	    old_live_ctx != live_ctx)
	old_free_cb(old_live_ctx);

    d->live_ctx = live_ctx;
    d->owns_live_ctx = owns_live_ctx;
    d->revision_cb = revision_cb;
    d->update_cb = update_cb;
    d->bounds_cb = bounds_cb;
    d->pick_cb = pick_cb;
    d->snap_cb = snap_cb;
    d->free_cb = free_cb;
    d->last_realized_revision = (revision_cb) ? revision_cb(_sketch_live_ctx(d)) : 0;
    if (payload->pl_revision == old_pl_revision)
	bsg_payload_bump_revision(payload);

    return 1;
}

uint64_t
bsg_payload_sketch_revision(struct bsg_payload *payload)
{
    struct bsg_sketch_live_data *d = bsg_payload_sketch_get_data(payload);
    if (!d)
	return 0;

    if (d->revision_cb)
	return d->revision_cb(_sketch_live_ctx(d));

    return d->last_realized_revision;
}

int
bsg_payload_sketch_realize(struct bsg_payload *payload, struct bsg_view *v)
{
    if (!payload || payload->pl_type != BSG_PL_SKETCH)
	return -1;

    uint64_t rev_before = payload->pl_revision;
    _sketch_payload_update(payload, v);

    return (payload->pl_revision != rev_before) ? 1 : 0;
}

int
bsg_payload_sketch_bounds(struct bsg_payload *payload, point_t *bmin, point_t *bmax)
{
    struct bsg_sketch_live_data *d = bsg_payload_sketch_get_data(payload);
    if (!d || !d->bounds_cb || !bmin || !bmax)
	return 0;
    return d->bounds_cb(_sketch_live_ctx(d), bmin, bmax);
}

int
bsg_payload_sketch_pick(struct bsg_payload *payload, struct bsg_view *v, int x, int y, void *pick_out)
{
    struct bsg_sketch_live_data *d = bsg_payload_sketch_get_data(payload);
    if (!d || !d->pick_cb)
	return 0;
    return d->pick_cb(_sketch_live_ctx(d), v, x, y, pick_out);
}

int
bsg_payload_sketch_snap(struct bsg_payload *payload, struct bsg_view *v, const point_t sample_pt, point_t out_pt)
{
    struct bsg_sketch_live_data *d = bsg_payload_sketch_get_data(payload);
    if (!d || !d->snap_cb || !sample_pt || !out_pt)
	return 0;
    return d->snap_cb(_sketch_live_ctx(d), v, sample_pt, out_pt);
}


/* -----------------------------------------------------------------------
 * ANNOTATION payload
 * ----------------------------------------------------------------------- */

static void
_annotation_segment_clear(struct bsg_annotation_segment *seg)
{
    if (!seg)
	return;
    if (seg->kind == BSG_ANNOTATION_SEGMENT_NURB) {
	if (seg->data.nurb.knots)
	    bu_free(seg->data.nurb.knots, "payload annotation nurb knots");
	if (seg->data.nurb.control_points)
	    bu_free(seg->data.nurb.control_points, "payload annotation nurb control points");
	if (seg->data.nurb.weights)
	    bu_free(seg->data.nurb.weights, "payload annotation nurb weights");
    } else if (seg->kind == BSG_ANNOTATION_SEGMENT_BEZIER) {
	if (seg->data.bezier.control_points)
	    bu_free(seg->data.bezier.control_points, "payload annotation bezier control points");
    } else if (seg->kind == BSG_ANNOTATION_SEGMENT_TEXT) {
	if (seg->data.text.text)
	    bu_free(seg->data.text.text, "payload annotation text segment");
    }
    memset(seg, 0, sizeof(*seg));
}

static void
_annotation_segments_clear(struct bsg_annotation_segment *segments, size_t segment_cnt)
{
    if (!segments)
	return;
    for (size_t i = 0; i < segment_cnt; i++)
	_annotation_segment_clear(&segments[i]);
}

static void
_annotation_segment_copy(struct bsg_annotation_segment *dst,
			 const struct bsg_annotation_segment *src)
{
    if (!dst || !src)
	return;
    memset(dst, 0, sizeof(*dst));
    dst->kind = src->kind;
    dst->reverse = src->reverse;
    switch (src->kind) {
	case BSG_ANNOTATION_SEGMENT_LINE:
	    dst->data.line = src->data.line;
	    break;
	case BSG_ANNOTATION_SEGMENT_CARC:
	    dst->data.carc = src->data.carc;
	    break;
	case BSG_ANNOTATION_SEGMENT_NURB:
	    dst->data.nurb.order = src->data.nurb.order;
	    dst->data.nurb.point_type = src->data.nurb.point_type;
	    dst->data.nurb.knot_count = src->data.nurb.knot_count;
	    dst->data.nurb.control_point_count = src->data.nurb.control_point_count;
	    if (src->data.nurb.knot_count && src->data.nurb.knots) {
		dst->data.nurb.knots = (fastf_t *)bu_calloc(src->data.nurb.knot_count,
			sizeof(fastf_t), "payload annotation nurb knots");
		memcpy(dst->data.nurb.knots, src->data.nurb.knots,
			src->data.nurb.knot_count * sizeof(fastf_t));
	    }
	    if (src->data.nurb.control_point_count && src->data.nurb.control_points) {
		dst->data.nurb.control_points = (int *)bu_calloc(
			src->data.nurb.control_point_count, sizeof(int),
			"payload annotation nurb control points");
		memcpy(dst->data.nurb.control_points, src->data.nurb.control_points,
			src->data.nurb.control_point_count * sizeof(int));
	    }
	    if (src->data.nurb.control_point_count && src->data.nurb.weights) {
		dst->data.nurb.weights = (fastf_t *)bu_calloc(
			src->data.nurb.control_point_count, sizeof(fastf_t),
			"payload annotation nurb weights");
		memcpy(dst->data.nurb.weights, src->data.nurb.weights,
			src->data.nurb.control_point_count * sizeof(fastf_t));
	    }
	    break;
	case BSG_ANNOTATION_SEGMENT_BEZIER:
	    dst->data.bezier.degree = src->data.bezier.degree;
	    dst->data.bezier.control_point_count = src->data.bezier.control_point_count;
	    if (src->data.bezier.control_point_count && src->data.bezier.control_points) {
		dst->data.bezier.control_points = (int *)bu_calloc(
			src->data.bezier.control_point_count, sizeof(int),
			"payload annotation bezier control points");
		memcpy(dst->data.bezier.control_points, src->data.bezier.control_points,
			src->data.bezier.control_point_count * sizeof(int));
	    }
	    break;
	case BSG_ANNOTATION_SEGMENT_TEXT:
	    dst->data.text.ref_pt = src->data.text.ref_pt;
	    dst->data.text.relative_position = src->data.text.relative_position;
	    dst->data.text.size = src->data.text.size;
	    dst->data.text.rotation = src->data.text.rotation;
	    if (src->data.text.text)
		dst->data.text.text = bu_strdup(src->data.text.text);
	    break;
	default:
	    break;
    }
}

static void
_annotation_payload_free(struct bsg_payload *pl)
{
    if (!pl)
	return;
    struct bsg_payload_annotation *ann = pl->pl.annotation;
    if (ann) {
	bu_vls_free(&ann->text);
	if (ann->points)
	    bu_free(ann->points, "payload annotation points");
	_annotation_segments_clear(ann->segments, ann->segment_cnt);
	if (ann->segments)
	    bu_free(ann->segments, "payload annotation segments");
	BU_PUT(ann, struct bsg_payload_annotation);
    }
    BU_PUT(pl, struct bsg_payload);
}

static int
_annotation_payload_bounds(struct bsg_payload *pl, point_t *bmin, point_t *bmax)
{
    if (!pl || !pl->pl.annotation)
	return 0;
    const struct bsg_payload_annotation *ann = pl->pl.annotation;
    if (!ann->point_cnt || !ann->points) {
	VMOVE((*bmin), ann->anchor);
	VMOVE((*bmax), ann->anchor);
	return 1;
    }

    int initialized = 0;
    if (ann->space == BSG_ANNOTATION_SPACE_DISPLAY) {
	VMOVE((*bmin), ann->anchor);
	VMOVE((*bmax), ann->anchor);
	initialized = 1;
    }
    for (size_t i = 0; i < pl->pl.annotation->point_cnt; i++) {
	point_t bp;
	if (ann->space == BSG_ANNOTATION_SPACE_DISPLAY) {
	    point_t local;
	    MAT4X3PNT(local, ann->display_mat, ann->points[i]);
	    VADD2(bp, ann->anchor, local);
	} else {
	    MAT4X3PNT(bp, ann->model_mat, ann->points[i]);
	}
	if (!initialized) {
	    VMOVE((*bmin), bp);
	    VMOVE((*bmax), bp);
	    initialized = 1;
	} else {
	    VMINMAX((*bmin), (*bmax), bp);
	}
    }
    return 1;
}

struct bsg_payload *
bsg_payload_annotation_create(const char *text, point_t *points, size_t point_cnt)
{
    point_t anchor = VINIT_ZERO;
    mat_t model_mat, display_mat;
    MAT_IDN(model_mat);
    MAT_IDN(display_mat);
    return bsg_payload_annotation_create_record(text, BSG_ANNOTATION_SPACE_MODEL,
	    anchor, model_mat, display_mat, (const point_t *)points, point_cnt,
	    NULL, 0);
}

struct bsg_payload *
bsg_payload_annotation_create_record(const char *summary,
				     bsg_annotation_space space,
				     const point_t anchor,
				     const mat_t model_mat,
				     const mat_t display_mat,
				     const point_t *points,
				     size_t point_cnt,
				     const struct bsg_annotation_segment *segments,
				     size_t segment_cnt)
{
    struct bsg_payload *pl = bsg_payload_create(BSG_PL_ANNOTATION);
    if (!pl)
	return NULL;

    struct bsg_payload_annotation *ann;
    BU_GET(ann, struct bsg_payload_annotation);
    memset(ann, 0, sizeof(*ann));
    BU_VLS_INIT(&ann->text);
    if (summary)
	bu_vls_sprintf(&ann->text, "%s", summary);
    ann->space = space;
    if (anchor)
	VMOVE(ann->anchor, anchor);
    if (model_mat)
	MAT_COPY(ann->model_mat, model_mat);
    else
	MAT_IDN(ann->model_mat);
    if (display_mat)
	MAT_COPY(ann->display_mat, display_mat);
    else
	MAT_IDN(ann->display_mat);
    ann->point_cnt = point_cnt;
    if (point_cnt && points) {
	ann->points = (point_t *)bu_calloc(point_cnt, sizeof(point_t), "payload annotation points");
	for (size_t i = 0; i < point_cnt; i++)
	    VMOVE(ann->points[i], points[i]);
    }
    ann->segment_cnt = segment_cnt;
    if (segment_cnt && segments) {
	ann->segments = (struct bsg_annotation_segment *)bu_calloc(segment_cnt,
		sizeof(struct bsg_annotation_segment), "payload annotation segments");
	for (size_t i = 0; i < segment_cnt; i++)
	    _annotation_segment_copy(&ann->segments[i], &segments[i]);
    }

    pl->pl.annotation = ann;
    pl->pl_free = _annotation_payload_free;
    pl->pl_bounds = _annotation_payload_bounds;
    return pl;
}

struct bsg_payload_annotation *
bsg_payload_annotation_get(struct bsg_payload *payload)
{
    if (!payload || payload->pl_type != BSG_PL_ANNOTATION)
	return NULL;
    return payload->pl.annotation;
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
