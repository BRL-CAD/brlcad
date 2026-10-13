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
/** @file libdm/tests/faceplate.c
 * Exercise layout, clipping, dispatch and registration without a windowing
 * toolkit or graphics context.  Record drawing through a null DM's call table.
 */
#include "common.h"
#include <stdlib.h>
#include "bu/app.h"
#include "bu/log.h"
#include "bu/str.h"
#include "dm.h"
#include "dm/faceplate.h"
#include "../include/private.h"

#define MAX_TEXT 32
struct text_record {
    const char *label;
    fastf_t x, y;
};
struct fixture {
    struct text_record text[MAX_TEXT];
    size_t texts, lines, probes, actions, row;
    fastf_t value;
    int unregister;
    struct dm *dmp;
};

static void
check(int condition, const char *message)
{
    if (!condition) {
        bu_log("faceplate: %s\n", message);
        exit(EXIT_FAILURE);
    }
}

static int
record_text(struct dm *dmp, const char *label, fastf_t x, fastf_t y,
    int UNUSED(size), int UNUSED(aspect))
{
    struct fixture *f = (struct fixture *)dm_get_udata(dmp);
    check(f->texts < MAX_TEXT, "too many text calls");
    f->text[f->texts].label = label;
    f->text[f->texts].x = x;
    f->text[f->texts++].y = y;
    return BRLCAD_OK;
}

static int
record_line(struct dm *dmp, fastf_t UNUSED(x1), fastf_t UNUSED(y1),
    fastf_t UNUSED(x2), fastf_t UNUSED(y2))
{
    struct fixture *f = (struct fixture *)dm_get_udata(dmp);
    f->lines++;
    return BRLCAD_OK;
}

static int
record_color(struct dm *dmp, unsigned char r, unsigned char g, unsigned char b,
    int UNUSED(strict), fastf_t UNUSED(alpha))
{
    VSET(dmp->i->dm_fg, r, g, b);
    return BRLCAD_OK;
}

static int
record_line_attr(struct dm *dmp, int width, int style)
{
    dm_set_linewidth(dmp, width);
    dm_set_linestyle(dmp, style);
    return BRLCAD_OK;
}

static int
get_item(void *UNUSED(data), size_t row, struct dm_faceplate_item *item)
{
    static const char *labels[] = {"header", "first", "second", "last"};
    static const fastf_t values[] = {-0.5, 0, 0.5, 0};
    if (row >= sizeof(labels)/sizeof(labels[0])) return 0;
    item->label = labels[row];
    item->value = values[row];
    item->selected = row == 1;
    VSET(item->color, 255, 255, 0);
    return 1;
}

static void
select_item(void *data, size_t row, fastf_t value, int activate)
{
    struct fixture *f = (struct fixture *)data;
    f->row = row;
    f->value = value;
    if (activate) f->actions++;
    else f->probes++;
    if (activate && f->unregister) dm_set_faceplate_gui(f->dmp, NULL);
}

static int
custom_label(struct dm *dmp, void *UNUSED(data), size_t row, fastf_t x, fastf_t y)
{
    if (row != 2) return 0;
    dm_draw_string_2d(dmp, "custom", x, y, 0, 0);
    return 1;
}

int
main(int UNUSED(argc), char **argv)
{
    const char *attach = "attach";
    struct fixture f = {0};
    struct dm_faceplate_controls menu, sliders;
    struct dm_faceplate_gui gui;
    struct dm *other;
    fastf_t edge;
    bu_setprogname(argv[0]);
    f.dmp = dm_open(NULL, NULL, "nu", 1, &attach);
    other = dm_open(NULL, NULL, "nu", 1, &attach);
    check(f.dmp && other, "cannot open null DMs");
    dm_set_udata(f.dmp, &f);
    f.dmp->i->dm_drawString2D = record_text;
    f.dmp->i->dm_drawLine2D = record_line;
    f.dmp->i->dm_setFGColor = record_color;
    f.dmp->i->dm_setLineAttr = record_line_attr;
    dm_set_fg(f.dmp, 12, 34, 56, 1, 1);
    dm_set_line_attr(f.dmp, 3, 1);
    dm_set_fontsize(f.dmp, 14);

    dm_faceplate_controls_init(&menu, DM_FACEPLATE_MENU);
    menu.top = 0.875;
    menu.bottom = 0.2;
    menu.row_height = 0.25;
    menu.font_size = 18;
    menu.data = &f;
    menu.item = get_item;
    menu.select = select_item;
    menu.draw_label = custom_label;
    edge = dm_faceplate_controls_draw(f.dmp, &menu);
    check(EQUAL(edge, 0.125) && f.texts == 4 && f.lines == 5,
        "menu clipping or border layout changed");
    check(BU_STR_EQUAL(f.text[2].label, "==>") && BU_STR_EQUAL(f.text[3].label, "custom"),
        "selection arrow or custom label missing");
    check(EQUAL(f.text[0].y, 0.75 + menu.text_offset), "menu baseline changed");
    check(dm_get_fg(f.dmp)[0] == 12 && dm_get_fg(f.dmp)[1] == 34 &&
        dm_get_fg(f.dmp)[2] == 56 && dm_get_linewidth(f.dmp) == 3 &&
        dm_get_linestyle(f.dmp) == 1 && dm_get_fontsize(f.dmp) == 14,
        "drawing did not restore DM state");
    check(dm_faceplate_controls_select(&menu, menu.label_x, 0.5, 0) &&
        f.probes == 1 && !f.actions && f.row == 1, "menu probe activated an action");
    check(dm_faceplate_controls_select(&menu, menu.label_x, 0.5, 1) &&
        f.actions == 1 && f.row == 1, "menu action selected the wrong row");
    check(!dm_faceplate_controls_select(&menu, menu.label_x, 0, 1) &&
        !dm_faceplate_controls_select(&menu, 0, 0.5, 1) &&
        !dm_faceplate_controls_select(&menu, menu.label_x, 1, 1),
        "hidden or outside menu rows were selectable");

    dm_faceplate_controls_init(&sliders, DM_FACEPLATE_SLIDER);
    sliders.left = -0.625;
    sliders.right = 1;
    sliders.top = 1;
    sliders.bottom = 0.5;
    sliders.row_height = 0.125;
    sliders.data = &f;
    sliders.item = get_item;
    sliders.select = select_item;
    f.texts = f.lines = 0;
    edge = dm_faceplate_controls_draw(f.dmp, &sliders);
    check(EQUAL(edge, 0.5) && f.texts == 4 && f.lines == 5,
        "slider layout changed");
    check(EQUAL(f.text[0].x, (-0.5 - sliders.dead_zone) * -sliders.left) &&
        ZERO(f.text[1].x) && EQUAL(f.text[2].x, 0.5 + sliders.dead_zone),
        "slider label did not account for asymmetric widths and dead zone");
    check(dm_faceplate_controls_select(&sliders, -0.3125, 0.8125, 1) &&
        f.row == 1 && EQUAL(f.value, -0.5), "negative slider mapping changed");
    check(dm_faceplate_controls_select(&sliders, 0.5, 0.8125, 1) &&
        EQUAL(f.value, 0.5), "positive slider mapping changed");
    check(dm_faceplate_controls_select(&sliders, 0, 0.8125, 1) &&
        ZERO(f.value), "slider dead-zone center changed");

    check(!dm_faceplate_gui_select(f.dmp, 0, 0.8, 1), "faceplate enabled by default");
    gui.menu = &menu;
    gui.sliders = &sliders;
    dm_set_faceplate_gui(f.dmp, &gui);
    check(dm_faceplate_gui_select(f.dmp, 0, 0.8125, 0) == DM_FACEPLATE_SLIDER_HIT,
        "registered slider dispatch failed");
    check(dm_faceplate_gui_select(f.dmp, menu.label_x, 0.5, 0) == DM_FACEPLATE_MENU_HIT,
        "registered menu dispatch failed");
    check(!dm_faceplate_gui_select(other, 0, 0.8125, 1), "registration leaked to another DM");
    f.texts = f.lines = 0;
    dm_draw_faceplate_gui(f.dmp);
    check(f.texts == 8, "registered controls were not drawn");
    f.unregister = 1;
    check(dm_faceplate_gui_select(f.dmp, 0, 0.8125, 1) == DM_FACEPLATE_SLIDER_HIT,
        "an action could not unregister its own GUI");
    f.texts = 0;
    dm_draw_faceplate_gui(f.dmp);
    check(!f.texts && !dm_faceplate_gui_select(f.dmp, 0, 0.8125, 1),
        "unregistered controls remained active");
    menu.row_height = 0;
    check(!dm_faceplate_controls_select(&menu, menu.label_x, 0.5, 1) &&
        EQUAL(dm_faceplate_controls_draw(f.dmp, &menu), menu.top),
        "invalid layout was accepted");
    check(!dm_faceplate_controls_select(NULL, 0, 0, 1), "NULL controls accepted input");
    dm_draw_faceplate_gui(NULL);
    dm_set_faceplate_gui(NULL, NULL);
    dm_close(other);
    dm_close(f.dmp);
    return EXIT_SUCCESS;
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
