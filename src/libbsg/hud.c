/*                         H U D . C
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
/** @file libbsg/hud.c
 *
 * Per-view HUD root and typed HUD payload realization.
 */

#include "common.h"

#include <math.h>
#include <string.h>

#include "bn.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "bu/ptbl.h"
#include "bu/units.h"
#include "bu/vls.h"

#include "bsg/defines.h"
#include "bsg/faceplate.h"
#include "bsg/geometry.h"
#include "bsg/hud.h"
#include "bsg/node.h"
#include "bsg/overlay.h"
#include "bsg/payload.h"
#include "payload_typed_private.h"
#include "bsg/util.h"
#include "bsg/view_state.h"
#include "bsg/scene_object.h"
#include "appearance_private.h"
#include "bsg_private.h"
#include "material_private.h"
#include "node_private.h"
#include "node_storage_private.h"
#include "object_private.h"
#include "geometry_private.h"
#include "overlay_private.h"
#include "payload_private.h"
#include "scene_object_private.h"
#include "view_state_private.h"

struct hud_feature_desc {
    bsg_hud_feature_type  type;
    bsg_hud_coord         coord_space;
    bsg_overlay_role      role;
    bsg_overlay_class     overlay_class;
    bsg_overlay_lifecycle lifecycle;
    const char           *name;
};

static const struct hud_feature_desc _hud_features[BSG_HUD_FEATURE_COUNT] = {
    { BSG_HUD_FEATURE_CENTER_DOT,  BSG_HUD_COORD_NDC,            BSG_OVERLAY_ROLE_SCREEN, BSG_OVERLAY_CLASS_FACEPLATE,             BSG_OVERLAY_LC_PERSISTENT,      "_hud_center_dot"  },
    { BSG_HUD_FEATURE_MODEL_AXES,  BSG_HUD_COORD_MODEL_ANCHORED, BSG_OVERLAY_ROLE_SCREEN, BSG_OVERLAY_CLASS_DIAGNOSTIC,            BSG_OVERLAY_LC_PER_VIEW,        "_hud_model_axes"  },
    { BSG_HUD_FEATURE_VIEW_AXES,   BSG_HUD_COORD_VIEW_PLANE,     BSG_OVERLAY_ROLE_SCREEN, BSG_OVERLAY_CLASS_DIAGNOSTIC,            BSG_OVERLAY_LC_PER_VIEW,        "_hud_view_axes"   },
    { BSG_HUD_FEATURE_VIEW_SCALE,  BSG_HUD_COORD_NDC,            BSG_OVERLAY_ROLE_SCREEN, BSG_OVERLAY_CLASS_DIAGNOSTIC,            BSG_OVERLAY_LC_PERSISTENT,      "_hud_view_scale"  },
    { BSG_HUD_FEATURE_ADC,         BSG_HUD_COORD_VIEW_PLANE,     BSG_OVERLAY_ROLE_SCREEN, BSG_OVERLAY_CLASS_DIAGNOSTIC,            BSG_OVERLAY_LC_PER_VIEW,        "_hud_adc"         },
    { BSG_HUD_FEATURE_GRID,        BSG_HUD_COORD_MODEL_ANCHORED, BSG_OVERLAY_ROLE_MODEL,  BSG_OVERLAY_CLASS_DIAGNOSTIC,            BSG_OVERLAY_LC_PER_VIEW,        "_hud_grid"        },
    { BSG_HUD_FEATURE_RECT,        BSG_HUD_COORD_SCREEN_PX,      BSG_OVERLAY_ROLE_SCREEN, BSG_OVERLAY_CLASS_SELECTION_RUBBER_BAND, BSG_OVERLAY_LC_PER_FRAME,       "_hud_rect"        },
    { BSG_HUD_FEATURE_VIEW_PARAMS, BSG_HUD_COORD_NDC,            BSG_OVERLAY_ROLE_SCREEN, BSG_OVERLAY_CLASS_DIAGNOSTIC,            BSG_OVERLAY_LC_PER_FRAME,       "_hud_view_params" },
    { BSG_HUD_FEATURE_FRAMEBUFFER, BSG_HUD_COORD_SCREEN_PX,      BSG_OVERLAY_ROLE_SCREEN, BSG_OVERLAY_CLASS_DIAGNOSTIC,            BSG_OVERLAY_LC_SHARED_VIEW_SET, "_hud_framebuffer" }
};

static fastf_t
_ged_to_ndc(int val)
{
    return ((fastf_t)val) / (fastf_t)BSG_VIEW_MAX;
}

static struct bsg_hud_node_meta *
_meta_alloc(int i, int sort_order)
{
    struct bsg_hud_node_meta *m;
    BU_ALLOC(m, struct bsg_hud_node_meta);
    m->feature_type = _hud_features[i].type;
    m->coord_space = _hud_features[i].coord_space;
    m->role = _hud_features[i].role;
    m->overlay_class = _hud_features[i].overlay_class;
    m->lifecycle = _hud_features[i].lifecycle;
    m->sort_order = sort_order;
    return m;
}

static struct bsg_hud_payload *
_payload_alloc(int i)
{
    struct bsg_hud_payload *p;
    BU_ALLOC(p, struct bsg_hud_payload);
    memset(p, 0, sizeof(*p));
    p->feature_type = _hud_features[i].type;
    return p;
}

static struct bsg_hud_node_meta *
_hud_meta(bsg_node *node)
{
    return (node && node->i) ? node->i->hud_meta : NULL;
}

static struct bsg_hud_payload *
_hud_payload(bsg_node *node)
{
    return (node && node->i) ? node->i->hud_payload : NULL;
}

static void
_hud_set_meta_payload(bsg_node *node, struct bsg_hud_node_meta *meta, struct bsg_hud_payload *payload)
{
    if (!node)
	return;
    struct bsg_node_internal *ni = bsg_node_internal_ensure(node);
    ni->hud_meta = meta;
    ni->hud_payload = payload;
}

static void
_meta_free_cb(struct bsg_node *node)
{
    if (!node)
return;
    struct bsg_hud_node_meta *m = _hud_meta(node);
    if (m) {
bu_free(m, "bsg_hud_node_meta");
node->i->hud_meta = NULL;
    }
    struct bsg_hud_payload *p = _hud_payload(node);
    if (p) {
bu_free(p, "bsg_hud_payload");
node->i->hud_payload = NULL;
    }
}

static void
_set_feature_style(bsg_node *node, int r, int g, int b, int line_width)
{
    if (!node)
return;
    bsg_material_set_rgb(node, (unsigned char)r, (unsigned char)g, (unsigned char)b);
    bsg_appearance_set_line_width(node, line_width);
}

static void
_clear_feature_children(bsg_node *feature)
{
    if (!feature)
return;
    while (BU_PTBL_LEN(&feature->children) > 0) {
bsg_node *child = (bsg_node *)BU_PTBL_GET(&feature->children, 0);
if (!child)
    break;
bsg_node_destroy(child);
    }
}

static bsg_node *
_make_feature_child(struct bsg_view *v, bsg_node *parent, int feature_idx,
    const char *suffix, int sort_order)
{
    if (!v || !parent)
return NULL;
    bsg_node *child = bsg_node_create(v, BSG_NODE_SHAPE);
    if (!child)
return NULL;
    bsg_node_set_object_type(child, bsg_shape_type());
    bsg_node_set_visible_state(child, 1);
    _hud_set_meta_payload(child, _meta_alloc(feature_idx, sort_order), _payload_alloc(feature_idx));
    bsg_node_set_release_callback(child, _meta_free_cb);
    {
struct bu_vls name = BU_VLS_INIT_ZERO;
bu_vls_sprintf(&name, "%s_%s", _hud_features[feature_idx].name, suffix);
bsg_node_set_name(child, bu_vls_cstr(&name));
bu_vls_free(&name);
    }
    bsg_overlay_register_owner(child, v,
_hud_features[feature_idx].role,
_hud_features[feature_idx].overlay_class,
_hud_features[feature_idx].lifecycle,
BSG_OVERLAY_ORDER_SCREEN,
NULL,
sort_order);
    bsg_node_add_child(parent, child);
    return child;
}

static void
_attach_line_set_payload(bsg_node *node, point_t *pts, const int *cmds, size_t cnt)
{
    if (!node)
return;
    if (!pts || !cmds || cnt == 0) {
bsg_geometry_node_clear_private_realization(node);
bsg_node_set_payload_type(node, BSG_PAYLOAD_OVERLAY);
return;
    }
    (void)bsg_geometry_node_set_line_set(node, (const point_t *)pts, cmds, cnt);
    bsg_node_set_payload_type(node, BSG_PAYLOAD_OVERLAY);
}

static void
_attach_hud_text_payload(bsg_node *node, const char *str, fastf_t x, fastf_t y, int size)
{
    if (!node || !str || strlen(str) == 0) {
	if (node) {
	    bsg_geometry_node_clear_private_realization(node);
	    bsg_node_set_payload_type(node, BSG_PAYLOAD_OVERLAY);
	}
	return;
    }
    (void)bsg_geometry_node_set_hud_text(node, str, x, y, size);
    bsg_node_set_payload_type(node, BSG_PAYLOAD_OVERLAY);
}

static void
_attach_axes_payload(bsg_node *node, const struct bsg_axes *src)
{
    if (!node || !src) {
	if (node)
	    bsg_geometry_node_clear_private_realization(node);
	return;
    }
    (void)bsg_geometry_node_set_axes_overlay(node, src);
}

static void
_attach_grid_payload(bsg_node *node, const struct bsg_grid_state *grid)
{
    if (!node || !grid) {
	if (node)
	    bsg_geometry_node_clear_private_realization(node);
	return;
    }
    (void)bsg_geometry_node_set_grid_overlay(node, grid);
}

static void
_attach_framebuffer_payload(bsg_node *node, int mode)
{
    if (!node || mode == 0) {
if (node) {
    bsg_geometry_node_clear_private_realization(node);
    bsg_node_set_payload_type(node, BSG_PAYLOAD_OVERLAY);
}
return;
    }
    (void)bsg_geometry_node_set_framebuffer_mode(node, mode);
    bsg_node_set_payload_type(node, BSG_PAYLOAD_OVERLAY);
}

static void
_hud_params_string(struct bsg_view *v, const struct bsg_params_state *ps, struct bu_vls *vls)
{
    point_t center;
    char *ustr = (char *)bu_units_string(v->gv_local2base);
    MAT_DELTAS_GET_NEG(center, v->gv_center);
    VSCALE(center, center, v->gv_base2local);

    if (ps->draw_size) {
if (bu_vls_strlen(vls) > 0)
    bu_vls_putc(vls, ' ');
bu_vls_printf(vls, "size[%s]: %.2f", ustr, v->gv_size * v->gv_base2local);
    }
    if (ps->draw_center) {
if (bu_vls_strlen(vls) > 0)
    bu_vls_putc(vls, ' ');
bu_vls_printf(vls, "center[%s]: (%.2f, %.2f, %.2f)", ustr, V3ARGS(center));
    }
    if (ps->draw_az) {
if (bu_vls_strlen(vls) > 0)
    bu_vls_putc(vls, ' ');
bu_vls_printf(vls, "az:%.2f", v->gv_aet[0]);
    }
    if (ps->draw_el) {
if (bu_vls_strlen(vls) > 0)
    bu_vls_putc(vls, ' ');
bu_vls_printf(vls, "el:%.2f", v->gv_aet[1]);
    }
    if (ps->draw_tw) {
if (bu_vls_strlen(vls) > 0)
    bu_vls_putc(vls, ' ');
bu_vls_printf(vls, "tw:%.2f", v->gv_aet[2]);
    }
    fastf_t frametime = bsg_view_frametime(v);
    if (ps->draw_fps && frametime > SMALL_FASTF) {
if (bu_vls_strlen(vls) > 0)
    bu_vls_putc(vls, ' ');
bu_vls_printf(vls, "FPS:%.2f", 1.0 / frametime);
    }
}

static void
_realize_center_dot(struct bsg_node *feature)
{
    point_t pts[4];
    int cmds[4] = {BSG_GEOMETRY_LINE_MOVE, BSG_GEOMETRY_LINE_DRAW, BSG_GEOMETRY_LINE_MOVE, BSG_GEOMETRY_LINE_DRAW};
    VSET(pts[0], -0.01, 0.0, 0.0);
    VSET(pts[1],  0.01, 0.0, 0.0);
    VSET(pts[2], 0.0, -0.01, 0.0);
    VSET(pts[3], 0.0,  0.01, 0.0);
    _attach_line_set_payload(feature, pts, cmds, 4);
}

static void
_realize_view_scale(struct bsg_view *v, bsg_node *feature, const struct bsg_other_state *state)
{
    point_t pts[6];
    int cmds[6] = {BSG_GEOMETRY_LINE_MOVE, BSG_GEOMETRY_LINE_DRAW, BSG_GEOMETRY_LINE_MOVE, BSG_GEOMETRY_LINE_DRAW, BSG_GEOMETRY_LINE_MOVE, BSG_GEOMETRY_LINE_DRAW};
    struct bu_vls scale = BU_VLS_INIT_ZERO;
    int soffset = 0;
    VSET(pts[0], -0.5, -0.8, 0.0);
    VSET(pts[1],  0.5, -0.8, 0.0);
    VSET(pts[2], -0.5, -0.79, 0.0);
    VSET(pts[3], -0.5, -0.81, 0.0);
    VSET(pts[4],  0.5, -0.79, 0.0);
    VSET(pts[5],  0.5, -0.81, 0.0);
    _attach_line_set_payload(feature, pts, cmds, 6);

    bsg_node *zero = _make_feature_child(v, feature, BSG_HUD_FEATURE_VIEW_SCALE, "zero", 31);
    bsg_node *label = _make_feature_child(v, feature, BSG_HUD_FEATURE_VIEW_SCALE, "value", 32);
    if (zero) {
	_attach_hud_text_payload(zero, "0", -0.505, -0.78, state->gos_font_size);
	_set_feature_style(zero, state->gos_line_color[0], state->gos_line_color[1], state->gos_line_color[2], 1);
    }
    if (label) {
bu_vls_printf(&scale, "%g%s", v->gv_size * 0.5 * v->gv_base2local,
bu_units_string(1 / v->gv_base2local));
soffset = (int)(strlen(bu_vls_cstr(&scale)) * 0.5);
_attach_hud_text_payload(label, bu_vls_cstr(&scale), 0.5 - (soffset * 0.015), -0.78, state->gos_font_size);
_set_feature_style(label, state->gos_line_color[0], state->gos_line_color[1], state->gos_line_color[2], 1);
    }
    bu_vls_free(&scale);
}

static void
_realize_adc(struct bsg_view *v, bsg_node *feature, const struct bsg_adc_state *adc)
{
    point_t pts[12];
    int cmds[12] = {
BSG_GEOMETRY_LINE_MOVE, BSG_GEOMETRY_LINE_DRAW,
BSG_GEOMETRY_LINE_MOVE, BSG_GEOMETRY_LINE_DRAW,
BSG_GEOMETRY_LINE_MOVE, BSG_GEOMETRY_LINE_DRAW,
BSG_GEOMETRY_LINE_MOVE, BSG_GEOMETRY_LINE_DRAW,
BSG_GEOMETRY_LINE_MOVE, BSG_GEOMETRY_LINE_DRAW,
BSG_GEOMETRY_LINE_MOVE, BSG_GEOMETRY_LINE_DRAW
    };
    fastf_t x = _ged_to_ndc(adc->dv_x);
    fastf_t y = _ged_to_ndc(adc->dv_y);
    double a1 = adc->a1 * DEG2RAD;
    double a2 = adc->a2 * DEG2RAD;
    fastf_t len = 2.0;
    VSET(pts[0], -1.0, y, 0.0);
    VSET(pts[1],  1.0, y, 0.0);
    VSET(pts[2], x, -1.0, 0.0);
    VSET(pts[3], x,  1.0, 0.0);
    VSET(pts[4], x, y, 0.0);
    VSET(pts[5], x + cos(a1) * len, y + sin(a1) * len, 0.0);
    VSET(pts[6], x, y, 0.0);
    VSET(pts[7], x - cos(a1) * len, y - sin(a1) * len, 0.0);
    VSET(pts[8], x, y, 0.0);
    VSET(pts[9], x + cos(a2) * len, y + sin(a2) * len, 0.0);
    VSET(pts[10], x, y, 0.0);
    VSET(pts[11], x - cos(a2) * len, y - sin(a2) * len, 0.0);
    _attach_line_set_payload(feature, pts, cmds, 12);

    struct bu_vls label = BU_VLS_INIT_ZERO;
    bu_vls_printf(&label, "a1:%.2f a2:%.2f dst:%.2f", adc->a1, adc->a2, adc->dst);
    bsg_node *text = _make_feature_child(v, feature, BSG_HUD_FEATURE_ADC, "text", 41);
    if (text) {
_attach_hud_text_payload(text, bu_vls_cstr(&label), x + 0.02, y + 0.02, 1);
_set_feature_style(text, adc->tick_color[0], adc->tick_color[1], adc->tick_color[2], 1);
    }
    bu_vls_free(&label);
}

static void
_realize_rect(struct bsg_node *feature, const struct bsg_interactive_rect_state *rect)
{
    point_t pts[5];
    int cmds[5] = {BSG_GEOMETRY_LINE_MOVE, BSG_GEOMETRY_LINE_DRAW, BSG_GEOMETRY_LINE_DRAW, BSG_GEOMETRY_LINE_DRAW, BSG_GEOMETRY_LINE_DRAW};
    VSET(pts[0], rect->x, rect->y, 0.0);
    VSET(pts[1], rect->x + rect->width, rect->y, 0.0);
    VSET(pts[2], rect->x + rect->width, rect->y + rect->height, 0.0);
    VSET(pts[3], rect->x, rect->y + rect->height, 0.0);
    VSET(pts[4], rect->x, rect->y, 0.0);
    _attach_line_set_payload(feature, pts, cmds, 5);
}

static void
_realize_view_params(struct bsg_view *v, bsg_node *feature, const struct bsg_params_state *params)
{
    struct bu_vls label = BU_VLS_INIT_ZERO;
    _hud_params_string(v, params, &label);
    _attach_hud_text_payload(feature, bu_vls_cstr(&label), -0.98, -0.965, params->font_size);
    bu_vls_free(&label);
}

BSG_EXPORT bsg_node *
bsg_hud_root_create(struct bsg_view *v)
{
    if (!v)
return NULL;
    if (v->gv_hud_root)
return (bsg_node *)v->gv_hud_root;

    bsg_node *root = bsg_scene_node_create_detached(v, BSG_SOURCE_CHILD);
    if (!root)
return NULL;

    bsg_node_set_object_type(root, bsg_group_type());
    bsg_node_set_visible_state(root, 1);
    root->parent = NULL;
    bsg_node_set_name(root, "_hud_root");

    for (int i = 0; i < BSG_HUD_FEATURE_COUNT; i++) {
bsg_node *child = bsg_node_create(v, BSG_NODE_SHAPE);
if (!child) {
    bu_log("bsg_hud_root_create: failed to allocate feature node %d\n", i);
    continue;
}
bsg_node_set_object_type(child, bsg_shape_type());
bsg_node_set_visible_state(child, 0);
bsg_node_set_name(child, _hud_features[i].name);
_hud_set_meta_payload(child, _meta_alloc(i, i), _payload_alloc(i));
bsg_node_set_release_callback(child, _meta_free_cb);
bsg_overlay_register_owner(child, v,
    _hud_features[i].role,
    _hud_features[i].overlay_class,
    _hud_features[i].lifecycle,
    (i == BSG_HUD_FEATURE_FRAMEBUFFER) ? BSG_OVERLAY_ORDER_POST_TRANSPARENT : BSG_OVERLAY_ORDER_SCREEN,
    NULL,
    i);
bsg_node_add_child(root, child);
    }

    v->gv_hud_root = root;
    return root;
}

BSG_EXPORT bsg_node *
bsg_hud_root_get(struct bsg_view *v)
{
    if (!v)
return NULL;
    return (bsg_node *)v->gv_hud_root;
}

int
bsg_hud_ensure(struct bsg_view *v)
{
    return bsg_hud_root_create(v) ? 0 : -1;
}

void
bsg_hud_root_destroy(struct bsg_view *v)
{
    if (!v || !v->gv_hud_root)
return;

    bsg_node *root = (bsg_node *)v->gv_hud_root;
    v->gv_hud_root = NULL;
    while (BU_PTBL_LEN(&root->children) > 0) {
bsg_node *child = (bsg_node *)BU_PTBL_GET(&root->children, 0);
if (!child)
    break;
bsg_node_destroy(child);
    }
    bsg_scene_node_release(root);
}

int
bsg_hud_sync(struct bsg_view *v)
{
    if (!v)
return -1;
    if (!v->gv_hud_root && !bsg_hud_root_create(v))
return -1;

    bsg_node *root = (bsg_node *)v->gv_hud_root;
    if (BU_PTBL_LEN(&root->children) < BSG_HUD_FEATURE_COUNT)
return -1;

    struct bsg_other_state *center_dot = bsg_view_center_dot(v);
    struct bsg_axes *model_axes = bsg_view_model_axes(v);
    struct bsg_axes *view_axes = bsg_view_view_axes(v);
    struct bsg_other_state *view_scale = bsg_view_scale_state(v);
    struct bsg_adc_state *adc = bsg_view_adc(v);
    struct bsg_grid_state *grid = bsg_view_grid(v);
    struct bsg_interactive_rect_state *rect = bsg_view_interactive_rect(v);
    struct bsg_params_state *params = bsg_view_params(v);
    int fb_mode = bsg_view_framebuffer_mode(v);

    for (int i = 0; i < BSG_HUD_FEATURE_COUNT; i++) {
bsg_node *child = (bsg_node *)BU_PTBL_GET(&root->children, (size_t)i);
struct bsg_hud_node_meta *m;
struct bsg_hud_payload *p;
int enabled = 0;
if (!child)
    continue;
m = _hud_meta(child);
p = _hud_payload(child);
if (!m || !p)
    continue;

_clear_feature_children(child);
bsg_overlay_register_owner(child, v,
    _hud_features[i].role,
    _hud_features[i].overlay_class,
    _hud_features[i].lifecycle,
    (i == BSG_HUD_FEATURE_FRAMEBUFFER && fb_mode == 1) ? BSG_OVERLAY_ORDER_MODEL : BSG_OVERLAY_ORDER_POST_TRANSPARENT,
    NULL,
    i);

switch (m->feature_type) {
    case BSG_HUD_FEATURE_CENTER_DOT:
enabled = center_dot->gos_draw;
memcpy(&p->data.other, center_dot, sizeof(struct bsg_other_state));
_set_feature_style(child, center_dot->gos_line_color[0], center_dot->gos_line_color[1], center_dot->gos_line_color[2], 1);
if (enabled)
    _realize_center_dot(child);
break;
    case BSG_HUD_FEATURE_MODEL_AXES:
enabled = model_axes->draw;
memcpy(&p->data.axes, model_axes, sizeof(struct bsg_axes));
_set_feature_style(child, model_axes->axes_color[0], model_axes->axes_color[1], model_axes->axes_color[2], model_axes->line_width);
if (enabled)
    _attach_axes_payload(child, model_axes);
break;
    case BSG_HUD_FEATURE_VIEW_AXES:
enabled = view_axes->draw;
memcpy(&p->data.axes, view_axes, sizeof(struct bsg_axes));
_set_feature_style(child, view_axes->axes_color[0], view_axes->axes_color[1], view_axes->axes_color[2], view_axes->line_width);
if (enabled)
    _attach_axes_payload(child, view_axes);
break;
    case BSG_HUD_FEATURE_VIEW_SCALE:
enabled = view_scale->gos_draw;
memcpy(&p->data.other, view_scale, sizeof(struct bsg_other_state));
_set_feature_style(child, view_scale->gos_line_color[0], view_scale->gos_line_color[1], view_scale->gos_line_color[2], 1);
if (enabled)
    _realize_view_scale(v, child, view_scale);
break;
    case BSG_HUD_FEATURE_ADC:
enabled = adc->draw;
memcpy(&p->data.adc, adc, sizeof(struct bsg_adc_state));
_set_feature_style(child, adc->line_color[0], adc->line_color[1], adc->line_color[2], adc->line_width);
if (enabled)
    _realize_adc(v, child, adc);
break;
    case BSG_HUD_FEATURE_GRID:
enabled = grid->draw;
memcpy(&p->data.grid, grid, sizeof(struct bsg_grid_state));
_set_feature_style(child, grid->color[0], grid->color[1], grid->color[2], 1);
if (enabled)
    _attach_grid_payload(child, grid);
break;
    case BSG_HUD_FEATURE_RECT:
enabled = rect->draw;
memcpy(&p->data.rect, rect, sizeof(struct bsg_interactive_rect_state));
_set_feature_style(child, rect->color[0], rect->color[1], rect->color[2], (rect->line_width > 0) ? rect->line_width : 1);
if (enabled)
    _realize_rect(child, rect);
break;
    case BSG_HUD_FEATURE_VIEW_PARAMS:
enabled = params->draw;
memcpy(&p->data.params, params, sizeof(struct bsg_params_state));
_set_feature_style(child, params->color[0], params->color[1], params->color[2], 1);
if (enabled)
    _realize_view_params(v, child, params);
break;
    case BSG_HUD_FEATURE_FRAMEBUFFER:
enabled = (fb_mode != 0);
p->data.framebuffer.mode = fb_mode;
_set_feature_style(child, 255, 255, 255, 1);
if (enabled)
    _attach_framebuffer_payload(child, fb_mode);
break;
    default:
break;
}

if (!enabled)
    bsg_node_set_payload(child, NULL);
bsg_node_set_visible_state(child, enabled ? 1 : 0);
    }

    return 0;
}

BSG_EXPORT struct bsg_hud_node_meta *
bsg_hud_node_get_meta(bsg_node *node)
{
    if (!node)
return NULL;
    if (!(bsg_node_get_payload_type(node) & BSG_PAYLOAD_OVERLAY))
return NULL;
    return _hud_meta(node);
}

BSG_EXPORT const struct bsg_hud_payload *
bsg_hud_node_get_payload(bsg_node *node)
{
    if (!node)
return NULL;
    if (!(bsg_node_get_payload_type(node) & BSG_PAYLOAD_OVERLAY))
return NULL;
    return _hud_payload(node);
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
