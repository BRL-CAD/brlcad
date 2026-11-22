/* BRL-CAD
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

#include "common.h"
#include <stdlib.h>
#include <string.h>
#include "bu/app.h"
#include "bu/log.h"
#include "bv.h"
#define DM_WITH_RT
#include "dm.h"
#include "../include/private.h"

struct fixture {
    size_t rows;
    int smallest, largest;
    fastf_t previous_y;
};

static void
check(int condition, const char *message)
{
    if (!condition) {
	bu_log("status text: %s\n", message);
	exit(EXIT_FAILURE);
    }
}

static int
bounds(struct dm *dmp, vect2d_t *bmin, vect2d_t *bmax, const char *text,
    fastf_t UNUSED(x), fastf_t UNUSED(y), int UNUSED(size), int UNUSED(aspect))
{
    V2SET(*bmin, 0, 0);
    V2SET(*bmax, strlen(text) * dm_get_fontsize(dmp) * 0.5, dm_get_fontsize(dmp));
    return BRLCAD_OK;
}

static int
draw_text(struct dm *dmp, const char *text, fastf_t x, fastf_t y,
    int UNUSED(size), int UNUSED(aspect))
{
    struct fixture *f = (struct fixture *)dm_get_udata(dmp);
    int font_size = dm_get_fontsize(dmp);
    fastf_t left = (x + 1) * dm_get_width(dmp) * 0.5;
    fastf_t bottom = (y + 1) * dm_get_height(dmp) * 0.5;
    check(left >= 0 && left + strlen(text) * font_size * 0.5 <= dm_get_width(dmp), "row exceeds width");
    check(bottom >= font_size * 0.25 && bottom + font_size <= dm_get_height(dmp), "row exceeds height");
    check(!f->rows || y < f->previous_y, "rows not drawn in reading order");
    if (!f->rows || font_size < f->smallest) f->smallest = font_size;
    if (font_size > f->largest) f->largest = font_size;
    f->previous_y = y;
    f->rows++;
    return BRLCAD_OK;
}

int
main(int UNUSED(argc), char **argv)
{
    bu_setprogname(argv[0]);
    struct dm *dmp = dm_open(NULL, NULL, "nu", 0, NULL);
    struct bview view;
    struct fixture f = {0};
    check(dmp != NULL, "could not create null display");
    memset(&view, 0, sizeof(view));
    bv_init(&view, NULL);
    view.dmp = dmp;
    view.gv_s->gv_view_params.draw = 1;
    check(view.gv_s->gv_view_params.font_size == 0, "default is not adaptive");
    dm_set_udata(dmp, &f);
    dmp->i->dm_drawString2D = draw_text;
    dmp->i->dm_String2DBBox = bounds;
    dm_set_width(dmp, 240);
    dm_set_height(dmp, 200);
    dm_set_fontsize(dmp, 22);
    dm_draw_faceplate(&view);
    check(f.rows > 1 && f.smallest >= 10, "narrow status did not wrap readably");
    check(dm_get_fontsize(dmp) == 22, "renderer did not restore font size");
    memset(&f, 0, sizeof(f));
    dm_set_width(dmp, 1600);
    dm_draw_faceplate(&view);
    check(f.rows == 1 && f.largest == 14, "wide status did not use automatic maximum");
    memset(&f, 0, sizeof(f));
    dm_set_width(dmp, 240);
    view.gv_s->gv_view_params.font_size = 18;
    dm_draw_faceplate(&view);
    check(f.rows > 1 && f.smallest == 18, "fixed status size changed or did not wrap");
    memset(&f, 0, sizeof(f));
    dm_set_height(dmp, 50);
    dm_draw_faceplate(&view);
    check(f.rows == 1 && f.smallest == 18, "short viewport lost its first status row");
    bv_free(&view);
    dm_close(dmp);
    return EXIT_SUCCESS;
}
