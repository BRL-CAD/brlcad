/*                         U I _ F B . C
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file ui_fb.c
 *
 * Windowed front end for the BRL-CAD launcher, drawn entirely with libfb (no
 * Tcl/Tk or Qt).  A rendered splash image is blitted with libicv/libfb, menu
 * entries are drawn as clickable panels labeled with the VFONT text helper, and
 * mouse/keyboard interaction is driven through the libfb input-event API
 * (fb_set_interactive / fb_next_event).
 *
 */

#include "common.h"

#include <stdio.h>
#include <string.h>

#include "bu.h"
#include "bu/color.h"
#include "bu/mime.h"
#include "bu/snooze.h"
#include "icv.h"
#include "dm.h"

#include "launcher.h"
#include "fbtext.h"

#define QUIT_ACTION (-1)

/* Palette (R,G,B). */
static const RGBpixel col_bg        = { 26, 28, 34 };
static const RGBpixel col_panel     = { 52, 58, 70 };
static const RGBpixel col_panel_off = { 38, 40, 46 };
static const RGBpixel col_hover     = { 178, 66, 42 };
static const RGBpixel col_border    = { 84, 92, 108 };
static const RGBpixel col_accent    = { 200, 80, 50 };
static const RGBpixel col_text      = { 235, 238, 242 };
static const RGBpixel col_text_dim  = { 150, 156, 168 };
static const RGBpixel col_text_off  = { 96, 100, 110 };

struct button {
    int x, yb, w, h;	/* framebuffer coords (origin lower-left) */
    int action;		/* registry index, or QUIT_ACTION */
};


static void
fill_rect(struct fb *fbp, int x, int yb, int w, int h, const RGBpixel c)
{
    int W = fb_getwidth(fbp);
    int H = fb_getheight(fbp);
    unsigned char *row;
    int i, yy;

    if (x < 0) { w += x; x = 0; }
    if (yb < 0) { h += yb; yb = 0; }
    if (x + w > W) w = W - x;
    if (yb + h > H) h = H - yb;
    if (w <= 0 || h <= 0)
	return;

    row = (unsigned char *)bu_malloc(w * 3, "fill_rect row");
    for (i = 0; i < w; i++) {
	row[i * 3 + RED] = c[RED];
	row[i * 3 + GRN] = c[GRN];
	row[i * 3 + BLU] = c[BLU];
    }
    for (yy = yb; yy < yb + h; yy++)
	(void)fb_write(fbp, x, yy, row, w);
    bu_free(row, "fill_rect row");
}


static void
draw_border(struct fb *fbp, int x, int yb, int w, int h, const RGBpixel c)
{
    fill_rect(fbp, x, yb, w, 1, c);
    fill_rect(fbp, x, yb + h - 1, w, 1, c);
    fill_rect(fbp, x, yb, 1, h, c);
    fill_rect(fbp, x + w - 1, yb, 1, h, c);
}


/*
 * Blit an icv image upright with its lower-left corner at (sx, sy) in
 * framebuffer coordinates.  libicv's uchar row buffer is bottom-up (row 0 is
 * the bottom of the image), matching libfb's bottom-up rows, so the copy is a
 * straight 1:1 mapping.  Doing this directly (rather than via fb_read_icv)
 * keeps the orientation consistent with the rest of the fb_write drawing below.
 */
static void
blit_icv(struct fb *fbp, icv_image_t *img, int sx, int sy)
{
    unsigned char *rgb;
    int iw, ih, i;
    int H = fb_getheight(fbp);

    if (!img)
	return;
    rgb = icv_data2uchar(img);
    if (!rgb)
	return;
    iw = (int)img->width;
    ih = (int)img->height;
    for (i = 0; i < ih; i++) {
	int fy = sy + i;
	if (fy < 0 || fy >= H)
	    continue;
	(void)fb_write(fbp, sx, fy, rgb + (size_t)i * iw * 3, iw);
    }
    bu_free(rgb, "blit_icv rgb");
}


/*
 * Load and scale the splash image.  Prefers a rendered splash asset, falling
 * back to the bundled BRL-CAD logo.  Returns NULL if neither is present.
 */
static icv_image_t *
load_splash(int maxw, int maxh)
{
    char path[MAXPATHLEN] = {0};
    icv_image_t *img = NULL;
    double sc;

    bu_dir(path, sizeof(path), BU_DIR_DATA, "launcher", "splash.png", NULL);
    if (bu_file_exists(path, NULL))
	img = icv_read(path, BU_MIME_IMAGE_AUTO, 0, 0);

    if (!img) {
	bu_dir(path, sizeof(path), BU_DIR_DATA, "images", "brlLogo-nobg.png", NULL);
	if (bu_file_exists(path, NULL))
	    img = icv_read(path, BU_MIME_IMAGE_AUTO, 0, 0);
    }

    if (!img)
	return NULL;

    sc = 1.0;
    if ((int)img->width > maxw)
	sc = (double)maxw / (double)img->width;
    if ((double)img->height * sc > (double)maxh)
	sc = (double)maxh / (double)img->height;

    if (sc < 1.0) {
	size_t nw = (size_t)((double)img->width * sc);
	size_t nh = (size_t)((double)img->height * sc);
	if (nw > 0 && nh > 0)
	    (void)icv_resize(img, ICV_RESIZE_BINTERP, nw, nh, 0);
    }

    return img;
}


static void
redraw(struct fb *fbp, struct fbtext *ft, struct app_registry *r,
       icv_image_t *splash, struct button *btns, int nbtns, int hover)
{
    int W = fb_getwidth(fbp);
    int H = fb_getheight(fbp);
    int i;
    int lh = fbtext_line_height(ft);

    fill_rect(fbp, 0, 0, W, H, col_bg);

    /* Splash across the top, centered. */
    if (splash) {
	int iw = (int)splash->width;
	int ih = (int)splash->height;
	int sx = (W - iw) / 2;
	int sy = H - 24 - ih;		/* 24px top margin, image top-aligned */
	if (sx < 0) sx = 0;
	if (sy < 0) sy = 0;
	blit_icv(fbp, splash, sx, sy);
	/* Accent rule under the splash. */
	fill_rect(fbp, 40, sy - 12, W - 80, 2, col_accent);
    }

    /* Menu buttons. */
    for (i = 0; i < nbtns; i++) {
	struct button *b = &btns[i];
	int avail = 1;
	const char *name = "Quit";
	const char *desc = "Exit the launcher";
	const RGBpixel *tcol = &col_text;
	const RGBpixel *dcol = &col_text_dim;

	if (b->action != QUIT_ACTION) {
	    struct app_entry *e = &r->apps[b->action];
	    name = e->name;
	    desc = e->description;
	    avail = e->available;
	}

	if (i == hover)
	    fill_rect(fbp, b->x, b->yb, b->w, b->h, col_hover);
	else
	    fill_rect(fbp, b->x, b->yb, b->w, b->h, avail ? col_panel : col_panel_off);
	draw_border(fbp, b->x, b->yb, b->w, b->h, col_border);

	if (!avail) {
	    tcol = &col_text_off;
	    dcol = &col_text_off;
	}

	/* Name at the left, description further right, vertically centered. */
	fbtext_draw(fbp, ft, b->x + 18, b->yb + (b->h - lh) / 2, name, *tcol);
	if (desc && desc[0]) {
	    char label[APP_TEXT_LEN + 8];
	    if (b->action != QUIT_ACTION && !avail)
		snprintf(label, sizeof(label), "%s  (not installed)", desc);
	    else
		bu_strlcpy(label, desc, sizeof(label));
	    fbtext_draw(fbp, ft, b->x + 260, b->yb + (b->h - lh) / 2, label, *dcol);
	}
    }

    /* Footer hint. */
    fbtext_draw(fbp, ft, 44, 18,
		"Click an entry, or press its number.  Press q or Esc to quit.",
		col_text_dim);

    fb_flush(fbp);
}


static int
hit_test(struct button *btns, int nbtns, int x, int y)
{
    int i;
    for (i = 0; i < nbtns; i++) {
	struct button *b = &btns[i];
	if (x >= b->x && x < b->x + b->w && y >= b->yb && y < b->yb + b->h)
	    return i;
    }
    return -1;
}


/* Perform the action for a button; sets *running to 0 to exit. */
static void
activate(struct app_registry *r, struct button *b, int *running)
{
    if (b->action == QUIT_ACTION) {
	*running = 0;
	return;
    }
    {
	struct app_entry *e = &r->apps[b->action];
	if (e->available)
	    (void)app_launch(e);
    }
}


int
ui_fb_run(struct app_registry *r)
{
    struct fb *fbp;
    struct fbtext ft;
    icv_image_t *splash;
    struct button *btns;
    int nbtns;
    int W, H;
    int i;
    int running = 1;
    int hover = -1;
    int need_redraw = 1;
    long rate;

    const int win_w = 820;
    const int win_h = 780;
    int bw;
    const int bh = 48;
    const int gap = 10;

    if (!r || r->count == 0)
	return -1;

    fbp = fb_open(NULL, win_w, win_h);
    if (fbp == FB_NULL)
	return -1;

    /* If we did not get a real window (e.g. the null device), fall back. */
    {
	const char *nm = fb_get_name(fbp);
	if (nm && (bu_strncmp(nm, "/dev/null", 9) == 0 || strstr(nm, "null") != NULL)) {
	    fb_close(fbp);
	    return -1;
	}
    }

    W = fb_getwidth(fbp);
    H = fb_getheight(fbp);

    if (fbtext_open(&ft, NULL) != 0) {
	/* No font available -- not usable as a graphical menu. */
	fb_close(fbp);
	return -1;
    }

    fb_set_interactive(fbp, 1);

    splash = load_splash(W - 40, 300);

    /* Build the button list: one per registry entry, plus Quit. */
    bw = W - 60;
    nbtns = r->count + 1;
    btns = (struct button *)bu_calloc(nbtns, sizeof(struct button), "buttons");
    {
	int splash_h = splash ? (int)splash->height : 0;
	int menu_top = 24 + splash_h + 28;	/* top-down y of first button */
	int bx = (W - bw) / 2;
	for (i = 0; i < nbtns; i++) {
	    int ty = menu_top + i * (bh + gap);
	    btns[i].x = bx;
	    btns[i].w = bw;
	    btns[i].h = bh;
	    btns[i].yb = H - (ty + bh);	/* convert top-down to fb bottom */
	    btns[i].action = (i < r->count) ? i : QUIT_ACTION;
	}
    }

    rate = fb_poll_rate(fbp);
    if (rate <= 0)
	rate = 15000;

    while (running) {
	struct fb_event e;

	if (need_redraw) {
	    redraw(fbp, &ft, r, splash, btns, nbtns, hover);
	    need_redraw = 0;
	}

	if (fb_next_event(fbp, &e)) {
	    switch (e.type) {
		case FB_EVENT_CLOSE:
		    running = 0;
		    break;
		case FB_EVENT_MOTION:
		    {
			int h = hit_test(btns, nbtns, e.x, e.y);
			if (h != hover) {
			    hover = h;
			    need_redraw = 1;
			}
		    }
		    break;
		case FB_EVENT_BUTTON_RELEASE:
		    if (e.button == 1) {
			int h = hit_test(btns, nbtns, e.x, e.y);
			if (h >= 0)
			    activate(r, &btns[h], &running);
			need_redraw = 1;
		    }
		    break;
		case FB_EVENT_KEY_PRESS:
		    {
			int k = e.keycode;
			if (k == 'q' || k == 'Q' || k == 27 /* Esc */) {
			    running = 0;
			} else if (k >= '1' && k <= '9') {
			    int idx = k - '1';
			    if (idx < nbtns) {
				activate(r, &btns[idx], &running);
				need_redraw = 1;
			    }
			} else if (k == '\r' || k == '\n') {
			    if (hover >= 0) {
				activate(r, &btns[hover], &running);
				need_redraw = 1;
			    }
			}
		    }
		    break;
		case FB_EVENT_EXPOSE:
		case FB_EVENT_RESIZE:
		    need_redraw = 1;
		    break;
		default:
		    break;
	    }
	} else {
	    bu_snooze(rate);
	}
    }

    if (splash)
	icv_destroy(splash);
    bu_free(btns, "buttons");
    fbtext_close(&ft);
    fb_close(fbp);
    return 0;
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
