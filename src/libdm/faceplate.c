/*                    F A C E P L A T E . C
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
/** @file libdm/faceplate.c */

#include "common.h"
#include <math.h>
#include <string.h>
#include "dm.h"
#include "dm/faceplate.h"
#include "include/private.h"

/* Keep the classic layout in one place, converting its historical GED
 * coordinates at the API boundary rather than in application renderers. */
#define FACEPLATE_MENU_RIGHT (-1250.0 * INV_BV)
#define FACEPLATE_MENU_LABEL ((-2048.0 + 115.0) * INV_BV)
#define FACEPLATE_MENU_TOP (1780.0 * INV_BV)
#define FACEPLATE_MENU_HEIGHT (104.0 * INV_BV)
#define FACEPLATE_SLIDER_HEIGHT (100.0 * INV_BV)
#define FACEPLATE_BOTTOM (-1920.0 * INV_BV)
#define FACEPLATE_TEXT_OFFSET (-15.0 * INV_BV)
#define FACEPLATE_DEAD_ZONE (64.0 * INV_BV)

void
dm_faceplate_controls_init(struct dm_faceplate_controls *c, enum dm_faceplate_control_type type)
{
    if (!c) return;
    memset(c, 0, sizeof(*c));
    c->type = type;
    c->bottom = FACEPLATE_BOTTOM;
    c->line_width = 1;
    VSETALL(c->line_color, 255);
    VSET(c->arrow_color, 255, 255, 0);
    if (type == DM_FACEPLATE_SLIDER) {
	c->left = FACEPLATE_MENU_RIGHT;
	c->right = BV_MAX * INV_BV;
	c->top = BV_MAX * INV_BV;
	c->row_height = FACEPLATE_SLIDER_HEIGHT;
	c->dead_zone = FACEPLATE_DEAD_ZONE;
    } else {
	c->left = -1.0;
	c->right = FACEPLATE_MENU_RIGHT;
	c->row_height = FACEPLATE_MENU_HEIGHT;
	c->top = FACEPLATE_MENU_TOP + c->row_height * 0.5;
	c->label_x = FACEPLATE_MENU_LABEL;
	c->text_offset = FACEPLATE_TEXT_OFFSET;
    }
}

static int
controls_valid(const struct dm_faceplate_controls *c)
{
    return c && c->item &&
	(c->type == DM_FACEPLATE_MENU || c->type == DM_FACEPLATE_SLIDER) &&
	isfinite(c->left) && isfinite(c->right) &&
	isfinite(c->top) && isfinite(c->bottom) &&
	isfinite(c->label_x) && isfinite(c->text_offset) &&
	isfinite(c->dead_zone) && c->dead_zone >= 0 && c->dead_zone < 1 &&
	isfinite(c->row_height) && c->row_height > SMALL_FASTF &&
	c->top - c->row_height < c->top &&
	c->left < c->right && c->top > c->bottom &&
	(c->type != DM_FACEPLATE_SLIDER || (c->left < 0 && c->right > 0));
}

static int
get_item(const struct dm_faceplate_controls *c, size_t row, struct dm_faceplate_item *item)
{
    memset(item, 0, sizeof(*item));
    return c->item(c->data, row, item) && item->label;
}

static void
set_color(struct dm *dmp, const int *color)
{
    dm_set_fg(dmp, color[0], color[1], color[2], 1, 1.0);
}

fastf_t
dm_faceplate_controls_draw(struct dm *dmp, const struct dm_faceplate_controls *c)
{
    size_t row = 0;
    fastf_t edge;
    int color[3], width, style, font;
    unsigned char *fg;
    if (!dmp || !controls_valid(c)) return c ? c->top : 0;

    fg = dm_get_fg(dmp);
    VMOVE(color, fg);
    width = dm_get_linewidth(dmp);
    style = dm_get_linestyle(dmp);
    font = dm_get_fontsize(dmp);
    dm_set_line_attr(dmp, c->line_width, 0);
    if (c->font_size > 0) dm_set_fontsize(dmp, c->font_size);

    edge = c->top;
    while (edge - c->row_height * 0.5 > c->bottom) {
	struct dm_faceplate_item item;
	fastf_t x = c->label_x;
	fastf_t y = edge - c->row_height * 0.5 + c->text_offset;
	if (!get_item(c, row, &item)) break;
	if (!row && c->type == DM_FACEPLATE_MENU) {
	    set_color(dmp, c->line_color);
	    dm_draw_line_2d(dmp, c->left, edge, c->right, edge);
	}
	if (c->type == DM_FACEPLATE_SLIDER) {
	    if (!isfinite(item.value)) item.value = 0;
	    if (item.value > 0) x = (item.value + c->dead_zone) * c->right;
	    else if (item.value < 0) x = (item.value - c->dead_zone) * -c->left;
	    else x = 0;
	}
	set_color(dmp, item.color);
	if (!c->draw_label || !c->draw_label(dmp, c->data, row, x, y))
	    dm_draw_string_2d(dmp, item.label, x, y, 0, 0);
	if (c->type == DM_FACEPLATE_MENU && item.selected) {
	    set_color(dmp, c->arrow_color);
	    dm_draw_string_2d(dmp, "==>", c->left, y, 0, 0);
	}
	edge -= c->row_height;
	set_color(dmp, c->line_color);
	dm_draw_line_2d(dmp, c->left, edge, c->right, edge);
	row++;
    }
    if (row) {
	fastf_t x = c->type == DM_FACEPLATE_MENU ? c->right : c->left;
	set_color(dmp, c->line_color);
	dm_draw_line_2d(dmp, x, c->top, x, edge);
    }
    dm_set_fontsize(dmp, font);
    dm_set_line_attr(dmp, width, style);
    set_color(dmp, color);
    return edge;
}

int
dm_faceplate_controls_select(const struct dm_faceplate_controls *c, fastf_t x, fastf_t y, int activate)
{
    size_t row = 0;
    fastf_t edge;
    if (!controls_valid(c) || !isfinite(x) || !isfinite(y) ||
	x < c->left || x > c->right || y > c->top) return 0;

    edge = c->top;
    while (edge - c->row_height * 0.5 > c->bottom) {
	struct dm_faceplate_item item;
	if (!get_item(c, row, &item)) return 0;
	edge -= c->row_height;
	if (y > edge || (c->type == DM_FACEPLATE_SLIDER && y >= edge)) {
	    fastf_t value = 0;
	    if (c->type == DM_FACEPLATE_SLIDER)
		value = x >= 0 ? x / c->right : x / -c->left;
	    if (c->select) c->select(c->data, row, value, activate);
	    return 1;
	}
	row++;
    }
    return 0;
}

void
dm_set_faceplate_gui(struct dm *dmp, const struct dm_faceplate_gui *gui)
{
    if (dmp) {
	dmp->i->dm_faceplate_gui = gui;
	dm_set_dirty(dmp, 1);
    }
}

void
dm_draw_faceplate_gui(struct dm *dmp)
{
    const struct dm_faceplate_gui *gui = dmp ? dmp->i->dm_faceplate_gui : NULL;
    if (!gui) return;
    dm_faceplate_controls_draw(dmp, gui->sliders);
    dm_faceplate_controls_draw(dmp, gui->menu);
}

int
dm_faceplate_gui_select(struct dm *dmp, fastf_t x, fastf_t y, int activate)
{
    const struct dm_faceplate_gui *gui = dmp ? dmp->i->dm_faceplate_gui : NULL;
    if (!gui) return 0;
    if (dm_faceplate_controls_select(gui->sliders, x, y, activate)) return DM_FACEPLATE_SLIDER_HIT;
    return dm_faceplate_controls_select(gui->menu, x, y, activate);
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
