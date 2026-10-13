/*                    F A C E P L A T E . H
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
/** @file dm/faceplate.h
 * Toolkit-independent menus and sliders for the display faceplate.
 * Applications supply presentation data and actions; libdm knows nothing
 * about the database or the application's editing state.
 */

#ifndef DM_FACEPLATE_H
#define DM_FACEPLATE_H

#include "common.h"
#include "vmath.h"
#include "dm/defines.h"

__BEGIN_DECLS

enum dm_faceplate_control_type {
    DM_FACEPLATE_MENU,
    DM_FACEPLATE_SLIDER
};

enum dm_faceplate_hit {
    DM_FACEPLATE_MISS,
    DM_FACEPLATE_MENU_HIT,
    DM_FACEPLATE_SLIDER_HIT
};

struct dm_faceplate_item {
    const char *label;              /**< Borrowed until the next provider call. */
    int color[3];
    int selected;                  /**< Draw a selection arrow for menu rows. */
    fastf_t value;                 /**< Slider value, normally in [-1, 1]. */
};

/** Coordinates are HUD coordinates: [-1, 1], with Y increasing upwards.
 * Rows run downwards from top; only rows whose centers exceed bottom are
 * drawn or selectable.  A provider returns 1 for a row, 0 at end of list.
 * It must not modify the controls during drawing or probing.
 */
struct dm_faceplate_controls {
    enum dm_faceplate_control_type type;
    fastf_t left, right, top, bottom;
    fastf_t row_height;
    fastf_t label_x, text_offset;
    fastf_t dead_zone;             /**< Fraction of each slider half-width. */
    int line_color[3], arrow_color[3];
    int line_width, font_size;     /**< Zero font size uses the DM's font. */
    void *data;
    int (*item)(void *data, size_t row, struct dm_faceplate_item *item);
    /** activate is zero for a probe, nonzero for an action.  Slider values
     * include the dead zone, so the application can apply its knob policy.
     * An action may replace menus or unregister the faceplate.
     */
    void (*select)(void *data, size_t row, fastf_t value, int activate);
    /** Optional label renderer. Return 1 if handled, 0 for the default text.
     * Used for application-specific highlighting, without moving that state
     * into libdm. Called inside the HUD with the label's baseline position.
     */
    int (*draw_label)(struct dm *, void *data, size_t row, fastf_t x, fastf_t y);
};

struct dm_faceplate_gui {
    const struct dm_faceplate_controls *menu;
    const struct dm_faceplate_controls *sliders;
};

/** Initialize an empty control list with MGED's classic layout and colors. */
DM_EXPORT extern void dm_faceplate_controls_init(struct dm_faceplate_controls *, enum dm_faceplate_control_type);

/** Draw inside an existing dm_hud_begin/end pair. Return the bottom edge of
 * the last row, or top for an empty/invalid list. Drawing preserves the DM's
 * foreground color, line attributes and font size.
 */
DM_EXPORT extern fastf_t dm_faceplate_controls_draw(struct dm *, const struct dm_faceplate_controls *);

/** Return 1 for a visible row at (x,y), 0 otherwise. A probe also calls select
 * with activate=0, allowing legacy applications to update their highlight.
 */
DM_EXPORT extern int dm_faceplate_controls_select(const struct dm_faceplate_controls *, fastf_t x, fastf_t y, int activate);

/** Register a borrowed GUI description, or NULL to disable it. The caller
 * owns all descriptors and callback data and must keep them alive until
 * unregistered or the DM is closed. dm_draw_faceplate draws registered
 * controls automatically. No GUI is registered by default.
 */
DM_EXPORT extern void dm_set_faceplate_gui(struct dm *, const struct dm_faceplate_gui *);
/** Draw the registered controls inside the current HUD. */
DM_EXPORT extern void dm_draw_faceplate_gui(struct dm *);
/** Dispatch to registered controls. Return 1 for menu, 2 for slider, 0 for
 * neither. Coordinates use the same HUD convention as drawing.
 */
DM_EXPORT extern int dm_faceplate_gui_select(struct dm *, fastf_t x, fastf_t y, int activate);

__END_DECLS
#endif /* DM_FACEPLATE_H */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
