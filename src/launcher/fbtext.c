/*                       F B T E X T . C
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
/** @file fbtext.c
 *
 * Anti-aliased text rendering into a framebuffer using BRL-CAD's Berkeley
 * VFONT facility (bu/vfont.h).  This is the same font machinery used by the
 * "fblabel" tool; the glyph rasterization here is adapted from it so the
 * launcher can label its menu without any GUI toolkit.
 *
 */

#include "common.h"

#include <stdio.h>
#include <string.h>

#include "bu/color.h"
#include "bu/vfont.h"
#include "dm.h"

#include "fbtext.h"

#define FONTBUFSZ 200

/* Gaussian anti-aliasing filter weights (from fblabel). */
#define CNTR_WT 0.3011592441
#define MID_WT 0.1238102667
#define CRNR_WT 0.0508999223

static int filterbuf[FONTBUFSZ][FONTBUFSZ];
static float resbuf[FONTBUFSZ];
static RGBpixel fbline[FONTBUFSZ];


static void
squash(int *buf0, int *buf1, int *buf2, float *ret_buf, int n)
{
    int j;
    for (j = 1; j < n - 1; j++) {
	ret_buf[j] =
	    (float)(buf2[j - 1] * CRNR_WT + buf2[j] * MID_WT + buf2[j + 1] * CRNR_WT +
		    buf1[j - 1] * MID_WT + buf1[j] * CNTR_WT + buf1[j + 1] * MID_WT +
		    buf0[j - 1] * CRNR_WT + buf0[j] * MID_WT + buf0[j + 1] * CRNR_WT);
    }
}


static int
bitx(char *bitstring, int posn)
{
    for (; posn >= 8; posn -= 8, bitstring++);
    return (int)(*bitstring) & (1 << posn);
}


static void
fill_buf(int wid, int *buf, char *bitrow)
{
    int j;
    for (j = 0; j < wid; j++)
	buf[j + 2] = bitx(bitrow, (j & ~7) + (7 - (j & 7))) ? 1 : 0;
    buf[0] = buf[1] = buf[wid + 2] = buf[wid + 3] = 0;
}


static void
draw_char(struct fb *fbp, struct vfont *vfp, struct vfont_dispatch *vdp,
	  int x, int y, const RGBpixel color, int cw, int ch)
{
    int i, j, base, ln;
    int totwid = cw;
    int bytes_wide = (cw + 7) >> 3;

    if (cw <= 0 || ch <= 0 || cw + 4 >= FONTBUFSZ || ch + 4 >= FONTBUFSZ)
	return;

    for (i = 0; i < 2; i++)
	memset((char *)&filterbuf[i][0], 0, (totwid + 4) * sizeof(int));

    for (ln = 0, i = ch + 1; i >= 2; i--, ln++)
	fill_buf(cw, &filterbuf[i][0], &vfp->vf_bits[vdp->vd_addr + bytes_wide * ln]);

    for (i = ch + 2; i < ch + 4; i++)
	memset((char *)&filterbuf[i][0], 0, (totwid + 4) * sizeof(int));

    base = (vdp->vd_down % 2) ? 1 : 2;

    for (i = ch + base; i >= base; i--) {
	squash(filterbuf[i - 1], filterbuf[i], filterbuf[i + 1], resbuf, totwid + 4);
	fb_read(fbp, x, y - vdp->vd_down + i, (unsigned char *)fbline, totwid + 3);
	for (j = 0; j < (totwid + 3) - 1; j++) {
	    int tmp;
	    tmp = fbline[j][RED] & 0377;
	    fbline[j][RED] = (int)(color[RED] * resbuf[j] + (1 - resbuf[j]) * tmp) & 0377;
	    tmp = fbline[j][GRN] & 0377;
	    fbline[j][GRN] = (int)(color[GRN] * resbuf[j] + (1 - resbuf[j]) * tmp) & 0377;
	    tmp = fbline[j][BLU] & 0377;
	    fbline[j][BLU] = (int)(color[BLU] * resbuf[j] + (1 - resbuf[j]) * tmp) & 0377;
	}
	(void)fb_write(fbp, x, y - vdp->vd_down + i, (unsigned char *)fbline, totwid + 3);
    }
}


int
fbtext_open(struct fbtext *t, const char *fontname)
{
    if (!t)
	return -1;
    t->vfp = vfont_get((char *)fontname);
    return (t->vfp == VFONT_NULL) ? -1 : 0;
}


void
fbtext_close(struct fbtext *t)
{
    if (t && t->vfp) {
	vfont_free(t->vfp);
	t->vfp = NULL;
    }
}


int
fbtext_line_height(struct fbtext *t)
{
    if (!t || !t->vfp)
	return 12;
    return (t->vfp->vf_maxy > 0) ? t->vfp->vf_maxy : 12;
}


int
fbtext_string_width(struct fbtext *t, const char *s)
{
    int w = 0;
    const char *p;

    if (!t || !t->vfp || !s)
	return 0;

    for (p = s; *p; p++) {
	int c = (int)*p & 0377;
	struct vfont_dispatch *vdp = &t->vfp->vf_dispatch[c];
	int cw = vdp->vd_left + vdp->vd_right;
	if (cw <= 1) {
	    w += 8;	/* blank space */
	    continue;
	}
	w += vdp->vd_width + 2;
    }
    return w;
}


void
fbtext_draw(struct fb *fbp, struct fbtext *t, int x, int y, const char *s, const RGBpixel color)
{
    int currx = x;
    const char *p;

    if (!fbp || !t || !t->vfp || !s)
	return;

    for (p = s; *p; p++) {
	int c = (int)*p & 0377;
	struct vfont_dispatch *vdp = &t->vfp->vf_dispatch[c];
	int cw = vdp->vd_left + vdp->vd_right;
	int ch = vdp->vd_up + vdp->vd_down;

	if (cw <= 1) {
	    currx += 8;
	    continue;
	}
	if (currx + cw > fb_getwidth(fbp) - 1)
	    break;

	draw_char(fbp, t->vfp, vdp, currx, y, color, cw, ch);
	currx += vdp->vd_width + 2;
    }
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
