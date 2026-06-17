/*                   A P P E A R A N C E . C
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
/** @file libbsg/appearance.c
 *
 * BSG node appearance accessors.
 */

#include "common.h"

#include "bsg/appearance.h"
#include "bsg/util.h"
#include "appearance_private.h"
#include "bsg_private.h"
#include "field_private.h"


static void
_appearance_state_init_defaults(struct bsg_node_appearance_state *state)
{
    if (!state)
	return;

    struct bsg_appearance_settings defaults = BSG_APPEARANCE_SETTINGS_INIT;
    state->settings = defaults;
    state->force_draw = 0;
    state->inherit_settings = 0;
    state->uses_default_color = 0;
    state->work_flag = 0;
    state->user_color = 0;
    state->default_color = 0;
    state->basecolor[0] = 0;
    state->basecolor[1] = 0;
    state->basecolor[2] = 0;
    state->eval_flag = 0;
    state->region_id = 0;
    state->valid = 1;
}


static struct bsg_node_appearance_state *
_appearance_state_ensure(bsg_node *node)
{
    if (!node)
	return NULL;
    struct bsg_node_internal *ni = bsg_node_internal_ensure(node);
    if (!ni)
	return NULL;
    if (!ni->appearance.valid)
	_appearance_state_init_defaults(&ni->appearance);
    return &ni->appearance;
}


static void
_appearance_state_update_numeric_from_fields(bsg_node *node,
					     struct bsg_node_appearance_state *state)
{
    if (!node || !state)
	return;
    (void)bsg_field_get_double(bsg_node_field_ref(node, BSG_FIELD_MATERIAL_ALPHA),
	    &state->settings.transparency);
    (void)bsg_field_get_enum(bsg_node_field_ref(node, BSG_FIELD_DRAW_FILL_MODE),
	    &state->settings.draw_mode);
    (void)bsg_field_get_int(bsg_node_field_ref(node, BSG_FIELD_DRAW_LINE_WIDTH),
	    &state->settings.s_line_width);
    (void)bsg_field_get_double(bsg_node_field_ref(node, BSG_FIELD_DRAW_ARROW_TIP_LENGTH),
	    &state->settings.s_arrow_tip_length);
    (void)bsg_field_get_double(bsg_node_field_ref(node, BSG_FIELD_DRAW_ARROW_TIP_WIDTH),
	    &state->settings.s_arrow_tip_width);
}


static const struct bsg_node_appearance_state *
_appearance_state_const(const bsg_node *node,
			struct bsg_node_appearance_state *scratch)
{
    if (!node)
	return NULL;
    if (node->i && node->i->appearance.valid)
	return &node->i->appearance;
    _appearance_state_init_defaults(scratch);
    return scratch;
}


static void
_appearance_sync_settings_from_fields(bsg_node *node)
{
    if (!node)
	return;
    struct bsg_node_appearance_state *state = _appearance_state_ensure(node);
    if (state)
	_appearance_state_update_numeric_from_fields(node, state);
}


void
bsg_appearance_settings_for_node(const bsg_node *node,
				 struct bsg_appearance_settings *settings)
{
    if (!settings)
	return;

    struct bsg_appearance_settings defaults = BSG_APPEARANCE_SETTINGS_INIT;
    *settings = defaults;

    struct bsg_node_appearance_state scratch;
    const struct bsg_node_appearance_state *state =
	_appearance_state_const(node, &scratch);
    if (state)
	*settings = state->settings;
    if (!node)
	return;

    (void)bsg_field_get_double(bsg_node_field_ref((bsg_node *)node, BSG_FIELD_MATERIAL_ALPHA),
	    &settings->transparency);
    (void)bsg_field_get_enum(bsg_node_field_ref((bsg_node *)node, BSG_FIELD_DRAW_FILL_MODE),
	    &settings->draw_mode);
    (void)bsg_field_get_int(bsg_node_field_ref((bsg_node *)node, BSG_FIELD_DRAW_LINE_WIDTH),
	    &settings->s_line_width);
    (void)bsg_field_get_double(bsg_node_field_ref((bsg_node *)node, BSG_FIELD_DRAW_ARROW_TIP_LENGTH),
	    &settings->s_arrow_tip_length);
    (void)bsg_field_get_double(bsg_node_field_ref((bsg_node *)node, BSG_FIELD_DRAW_ARROW_TIP_WIDTH),
	    &settings->s_arrow_tip_width);
}


void
bsg_appearance_set_visible(bsg_node *node, int visible)
{
    if (!node)
	return;
    (void)bsg_field_set_bool(bsg_node_field_ref(node, BSG_FIELD_VISIBILITY), visible ? 1 : 0);
}


int
bsg_appearance_visible(const bsg_node *node)
{
    if (!node)
	return 0;
    int visible = 0;
    (void)bsg_field_get_bool(bsg_node_field_ref((bsg_node *)node, BSG_FIELD_VISIBILITY), &visible);
    return visible ? 1 : 0;
}


void
bsg_appearance_set_force_draw(bsg_node *node, int force_draw)
{
    if (!node)
	return;
    struct bsg_node_appearance_state *state = _appearance_state_ensure(node);
    if (state)
	state->force_draw = force_draw ? 1 : 0;
}


int
bsg_appearance_force_draw(const bsg_node *node)
{
    if (!node)
	return 0;
    struct bsg_node_appearance_state scratch;
    const struct bsg_node_appearance_state *state =
	_appearance_state_const(node, &scratch);
    return (state && state->force_draw) ? 1 : 0;
}


void
bsg_appearance_set_line_style(bsg_node *node, int dashed)
{
    if (!node)
	return;
    (void)bsg_field_set_int(bsg_node_field_ref(node, BSG_FIELD_DRAW_LINE_STYLE),
	    dashed ? 1 : 0);
}


int
bsg_appearance_line_style(const bsg_node *node)
{
    if (!node)
	return 0;
    int line_style = 0;
    (void)bsg_field_get_int(bsg_node_field_ref((bsg_node *)node, BSG_FIELD_DRAW_LINE_STYLE),
	    &line_style);
    return line_style ? 1 : 0;
}


void
bsg_scene_set_line_style(bsg_scene_ref ref, int dashed)
{
    bsg_appearance_set_line_style((bsg_node *)ref.opaque, dashed);
}


int
bsg_scene_line_style(bsg_scene_ref ref)
{
    return bsg_appearance_line_style((const bsg_node *)ref.opaque);
}


void
bsg_appearance_set_line_width(bsg_node *node, int line_width)
{
    if (!node)
	return;
    if (line_width < 0)
	line_width = 0;
    (void)bsg_field_set_int(bsg_node_field_ref(node, BSG_FIELD_DRAW_LINE_WIDTH),
	    line_width);
}


int
bsg_appearance_line_width(const bsg_node *node)
{
    if (!node)
	return 0;
    int line_width = 0;
    (void)bsg_field_get_int(bsg_node_field_ref((bsg_node *)node, BSG_FIELD_DRAW_LINE_WIDTH),
	    &line_width);
    return line_width;
}


void
bsg_scene_set_line_width(bsg_scene_ref ref, int line_width)
{
    bsg_appearance_set_line_width((bsg_node *)ref.opaque, line_width);
}


int
bsg_scene_line_width(bsg_scene_ref ref)
{
    return bsg_appearance_line_width((const bsg_node *)ref.opaque);
}


int
bsg_appearance_apply_settings(bsg_node *node, const struct bsg_appearance_settings *settings)
{
    if (!node || !settings)
	return 0;
    struct bsg_node_appearance_state *state = _appearance_state_ensure(node);
    if (!state)
	return 0;
    int changed = bsg_appearance_settings_sync(&state->settings, settings);
    (void)bsg_field_set_double(bsg_node_field_ref(node, BSG_FIELD_MATERIAL_ALPHA),
	    settings->transparency);
    (void)bsg_field_set_enum(bsg_node_field_ref(node, BSG_FIELD_DRAW_FILL_MODE),
	    settings->draw_mode);
    (void)bsg_field_set_int(bsg_node_field_ref(node, BSG_FIELD_DRAW_LINE_WIDTH),
	    settings->s_line_width);
    (void)bsg_field_set_double(bsg_node_field_ref(node, BSG_FIELD_DRAW_ARROW_TIP_LENGTH),
	    settings->s_arrow_tip_length);
    (void)bsg_field_set_double(bsg_node_field_ref(node, BSG_FIELD_DRAW_ARROW_TIP_WIDTH),
	    settings->s_arrow_tip_width);
    _appearance_sync_settings_from_fields(node);
    return changed;
}


int
bsg_scene_apply_appearance_settings(bsg_scene_ref ref,
				    const struct bsg_appearance_settings *settings)
{
    return bsg_appearance_apply_settings((bsg_node *)ref.opaque, settings);
}


void
bsg_appearance_set_inherited_by_children(bsg_node *node, int inherited)
{
    if (!node)
	return;
    struct bsg_node_appearance_state *state = _appearance_state_ensure(node);
    if (state)
	state->inherit_settings = inherited ? 1 : 0;
}


int
bsg_appearance_inherited_by_children(const bsg_node *node)
{
    if (!node)
	return 0;
    struct bsg_node_appearance_state scratch;
    const struct bsg_node_appearance_state *state =
	_appearance_state_const(node, &scratch);
    return (state && state->inherit_settings) ? 1 : 0;
}


int
bsg_appearance_color_override(const bsg_node *node, unsigned char color[3])
{
    if (!node)
	return 0;
    struct bsg_appearance_settings settings;
    bsg_appearance_settings_for_node(node, &settings);
    if (!settings.color_override)
	return 0;
    if (color) {
	color[0] = settings.color[0];
	color[1] = settings.color[1];
	color[2] = settings.color[2];
    }
    return 1;
}


void
bsg_appearance_set_color_override(bsg_node *node, const unsigned char color[3], int enabled)
{
    if (!node)
	return;
    struct bsg_node_appearance_state *state = _appearance_state_ensure(node);
    if (!state)
	return;
    state->settings.color_override = enabled ? 1 : 0;
    if (color) {
	state->settings.color[0] = color[0];
	state->settings.color[1] = color[1];
	state->settings.color[2] = color[2];
    }
    _appearance_sync_settings_from_fields(node);
}


int
bsg_appearance_uses_default_color(const bsg_node *node)
{
    if (!node)
	return 0;
    struct bsg_node_appearance_state scratch;
    const struct bsg_node_appearance_state *state =
	_appearance_state_const(node, &scratch);
    return (state && state->uses_default_color) ? 1 : 0;
}


void
bsg_appearance_set_work_flag(bsg_node *node, int wflag)
{
    if (!node)
	return;
    struct bsg_node_appearance_state *state = _appearance_state_ensure(node);
    if (state)
	state->work_flag = wflag;
}


void
bsg_scene_set_work_flag(bsg_scene_ref ref, int wflag)
{
    bsg_appearance_set_work_flag((bsg_node *)ref.opaque, wflag);
}


void
bsg_appearance_set_legacy_color_info(bsg_node *node,
				     const unsigned char basecolor[3],
				     int user_color,
				     int default_color)
{
    if (!node)
	return;
    struct bsg_node_appearance_state *state = _appearance_state_ensure(node);
    if (state) {
	state->user_color = user_color ? 1 : 0;
	state->default_color = default_color ? 1 : 0;
	if (basecolor) {
	    state->basecolor[0] = basecolor[0];
	    state->basecolor[1] = basecolor[1];
	    state->basecolor[2] = basecolor[2];
	}
    }
}


void
bsg_scene_set_legacy_color_info(bsg_scene_ref ref,
				const unsigned char basecolor[3],
				int user_color,
				int default_color)
{
    bsg_appearance_set_legacy_color_info((bsg_node *)ref.opaque, basecolor,
	    user_color, default_color);
}


void
bsg_appearance_legacy_basecolor(const bsg_node *node, unsigned char basecolor[3])
{
    if (!basecolor)
	return;
    basecolor[0] = 0;
    basecolor[1] = 0;
    basecolor[2] = 0;
    if (!node)
	return;
    struct bsg_node_appearance_state scratch;
    const struct bsg_node_appearance_state *state =
	_appearance_state_const(node, &scratch);
    if (!state)
	return;
    basecolor[0] = state->basecolor[0];
    basecolor[1] = state->basecolor[1];
    basecolor[2] = state->basecolor[2];
}


void
bsg_scene_legacy_basecolor(bsg_scene_ref ref, unsigned char basecolor[3])
{
    bsg_appearance_legacy_basecolor((const bsg_node *)ref.opaque, basecolor);
}


int
bsg_appearance_legacy_user_color(const bsg_node *node)
{
    if (!node)
	return 0;
    struct bsg_node_appearance_state scratch;
    const struct bsg_node_appearance_state *state =
	_appearance_state_const(node, &scratch);
    return (state && state->user_color) ? 1 : 0;
}


int
bsg_scene_legacy_user_color(bsg_scene_ref ref)
{
    return bsg_appearance_legacy_user_color((const bsg_node *)ref.opaque);
}


int
bsg_appearance_legacy_default_color(const bsg_node *node)
{
    if (!node)
	return 0;
    struct bsg_node_appearance_state scratch;
    const struct bsg_node_appearance_state *state =
	_appearance_state_const(node, &scratch);
    return (state && state->default_color) ? 1 : 0;
}


int
bsg_scene_legacy_default_color(bsg_scene_ref ref)
{
    return bsg_appearance_legacy_default_color((const bsg_node *)ref.opaque);
}


void
bsg_appearance_set_legacy_uses_default_color(bsg_node *node, int default_color)
{
    if (!node)
	return;
    struct bsg_node_appearance_state *state = _appearance_state_ensure(node);
    if (state)
	state->uses_default_color = default_color ? 1 : 0;
}


void
bsg_scene_set_legacy_uses_default_color(bsg_scene_ref ref, int default_color)
{
    bsg_appearance_set_legacy_uses_default_color((bsg_node *)ref.opaque,
	    default_color);
}


void
bsg_appearance_set_legacy_eval_flag(bsg_node *node, int eflag)
{
    if (!node)
	return;
    struct bsg_node_appearance_state *state = _appearance_state_ensure(node);
    if (state)
	state->eval_flag = eflag ? 1 : 0;
}


void
bsg_scene_set_legacy_eval_flag(bsg_scene_ref ref, int eflag)
{
    bsg_appearance_set_legacy_eval_flag((bsg_node *)ref.opaque, eflag);
}


int
bsg_appearance_legacy_eval_flag(const bsg_node *node)
{
    if (!node)
	return 0;
    struct bsg_node_appearance_state scratch;
    const struct bsg_node_appearance_state *state =
	_appearance_state_const(node, &scratch);
    return (state && state->eval_flag) ? 1 : 0;
}


int
bsg_scene_legacy_eval_flag(bsg_scene_ref ref)
{
    return bsg_appearance_legacy_eval_flag((const bsg_node *)ref.opaque);
}


void
bsg_appearance_set_legacy_region_id(bsg_node *node, int region_id)
{
    if (!node)
	return;
    struct bsg_node_appearance_state *state = _appearance_state_ensure(node);
    if (state)
	state->region_id = region_id;
}


void
bsg_scene_set_legacy_region_id(bsg_scene_ref ref, int region_id)
{
    bsg_appearance_set_legacy_region_id((bsg_node *)ref.opaque, region_id);
}


int
bsg_appearance_legacy_region_id(const bsg_node *node)
{
    if (!node)
	return 0;
    struct bsg_node_appearance_state scratch;
    const struct bsg_node_appearance_state *state =
	_appearance_state_const(node, &scratch);
    return state ? state->region_id : 0;
}


int
bsg_scene_legacy_region_id(bsg_scene_ref ref)
{
    return bsg_appearance_legacy_region_id((const bsg_node *)ref.opaque);
}


void
bsg_appearance_set_highlighted(bsg_node *node, int highlighted)
{
    bsg_appearance_set_highlight_state(node, highlighted ? UP : DOWN);
}


void
bsg_scene_set_highlighted(bsg_scene_ref ref, int highlighted)
{
    bsg_appearance_set_highlighted((bsg_node *)ref.opaque, highlighted);
}


int
bsg_appearance_is_highlighted(const bsg_node *node)
{
    return (bsg_appearance_highlight_state(node) == UP) ? 1 : 0;
}


int
bsg_scene_highlighted(bsg_scene_ref ref)
{
    return bsg_appearance_is_highlighted((const bsg_node *)ref.opaque);
}


void
bsg_appearance_set_highlight_state(bsg_node *node, char state)
{
    if (!node)
	return;
    bsg_node_internal_ensure(node)->interaction.highlight_state =
	(state == UP) ? UP : DOWN;
}


char
bsg_appearance_highlight_state(const bsg_node *node)
{
    if (!node || !node->i)
	return DOWN;
    return (node->i->interaction.highlight_state == UP) ? UP : DOWN;
}


void
bsg_appearance_set_changed(bsg_node *node, int changed)
{
    if (!node)
	return;
    bsg_node_internal_ensure(node)->changed = changed ? 1 : 0;
}


void
bsg_scene_set_changed(bsg_scene_ref ref, int changed)
{
    bsg_appearance_set_changed((bsg_node *)ref.opaque, changed);
}


int
bsg_appearance_get_changed(const bsg_node *node)
{
    if (!node || !node->i)
	return 0;
    return node->i->changed ? 1 : 0;
}


int
bsg_scene_changed(bsg_scene_ref ref)
{
    return bsg_appearance_get_changed((const bsg_node *)ref.opaque);
}


uint64_t
bsg_appearance_drawn_rev(const bsg_node *node)
{
    if (!node || !node->i)
	return 0;
    return node->i->drawn_revision;
}


uint64_t
bsg_scene_drawn_rev(bsg_scene_ref ref)
{
    return bsg_appearance_drawn_rev((const bsg_node *)ref.opaque);
}


int
bsg_appearance_dmode(const bsg_node *node)
{
    if (!node)
	return 0;
    int draw_mode = 0;
    (void)bsg_field_get_enum(bsg_node_field_ref((bsg_node *)node, BSG_FIELD_DRAW_FILL_MODE),
	    &draw_mode);
    return draw_mode;
}


int
bsg_scene_dmode(bsg_scene_ref ref)
{
    return bsg_appearance_dmode((const bsg_node *)ref.opaque);
}


void
bsg_appearance_set_dmode(bsg_node *node, int draw_mode)
{
    if (!node)
	return;
    (void)bsg_field_set_enum(bsg_node_field_ref(node, BSG_FIELD_DRAW_FILL_MODE),
	    draw_mode);
}


void
bsg_scene_set_dmode(bsg_scene_ref ref, int draw_mode)
{
    bsg_appearance_set_dmode((bsg_node *)ref.opaque, draw_mode);
}


int
bsg_appearance_strict_fallback(const bsg_node *node)
{
    if (!node)
	return 0;
    struct bsg_node_appearance_state scratch;
    const struct bsg_node_appearance_state *state =
	_appearance_state_const(node, &scratch);
    return (state && state->settings.strict_fallback) ? 1 : 0;
}


void
bsg_appearance_set_strict_fallback(bsg_node *node, int strict_fallback)
{
    if (!node)
	return;
    struct bsg_node_appearance_state *state = _appearance_state_ensure(node);
    if (state)
	state->settings.strict_fallback = strict_fallback ? 1 : 0;
}


int
bsg_scene_strict_fallback(bsg_scene_ref ref)
{
    return bsg_appearance_strict_fallback((const bsg_node *)ref.opaque);
}


void
bsg_scene_set_strict_fallback(bsg_scene_ref ref, int strict_fallback)
{
    bsg_appearance_set_strict_fallback((bsg_node *)ref.opaque, strict_fallback);
}


fastf_t
bsg_appearance_transparency(const bsg_node *node)
{
    if (!node)
	return 1.0;
    double transparency = 1.0;
    (void)bsg_field_get_double(bsg_node_field_ref((bsg_node *)node, BSG_FIELD_MATERIAL_ALPHA),
	    &transparency);
    return transparency;
}


fastf_t
bsg_scene_transparency(bsg_scene_ref ref)
{
    return bsg_appearance_transparency((const bsg_node *)ref.opaque);
}


void
bsg_appearance_set_transparency(bsg_node *node, fastf_t t)
{
    if (!node)
	return;
    (void)bsg_field_set_double(bsg_node_field_ref(node, BSG_FIELD_MATERIAL_ALPHA),
	    t);
}


void
bsg_scene_set_transparency(bsg_scene_ref ref, fastf_t t)
{
    bsg_appearance_set_transparency((bsg_node *)ref.opaque, t);
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
