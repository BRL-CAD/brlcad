/*                       P A I N T . C P P
 * BRL-CAD
 *
 * Copyright (c) 2004-2021 United States Government as represented by
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
 *
 */
/** @file burst/paint.cpp
 *
 */

#include "common.h"

#include <assert.h>
#include <memory.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>

#include "vmath.h"
#include "bu/log.h"
#include "bu/str.h"
#include "raytrace.h"
#include "dm.h"

#include "./burst.h"

static int roundToInt(fastf_t f)
{
    int a = (int)f;
    return (f - a >= 0.5) ? a + 1 : a;
}

/* Offset frame buffer coordinates to center of cell from grid lines. */
#define CenterCell(b)	(b += roundToInt((fastf_t) s->zoom / 2.0))

/* Compare one RGB pixel to another. */
#define SAMERGB(a, b)	((a)[RED] == (b)[RED] &&\
			 (a)[GRN] == (b)[GRN] &&\
			 (a)[BLU] == (b)[BLU])

static unsigned char pixbuf[MAXDEVWID][3];
static int gridxmargin;
static int gridymargin;

void
gridToFb(struct burst_state *s, int gx, int gy, int *fxp, int *fyp)
{
    /* map grid offsets to frame buffer coordinates */
    *fxp = (gx - s->gridxorg)*s->zoom + gridxmargin;
    *fyp = (gy - s->gridyorg)*s->zoom + gridymargin;
    return;
}

void
paintGridFb(struct burst_state *s)
{
    int fx, fy;
    int fxbeg, fybeg;
    int fxfin, fyfin;
    int fxorg, fyorg;
    int fgridwid;
    if (fb_clear(s->fbiop, s->pixbkgr) == -1)
	return;
    gridxmargin = (s->devwid - (s->gridwidth*s->zoom)) / 2.0;
    gridymargin = (s->devhgt - (s->gridheight*s->zoom)) / 2.0;
    gridToFb(s, s->gridxorg, s->gridyorg, &fxbeg, &fybeg);
    gridToFb(s, s->gridxfin+1, s->gridyfin+1, &fxfin, &fyfin);
    gridToFb(s, 0, 0, &fxorg, &fyorg);
    CenterCell(fxorg); /* center of cell */
    CenterCell(fyorg);
    if (s->zoom == 1) {
	fgridwid = s->gridwidth + 2;
	fxfin++;
    } else {
	fgridwid = s->gridwidth * s->zoom + 1;
    }

    /* draw vertical lines */
    for (fx = 1; fx < fgridwid; fx++)
	COPYRGB(&pixbuf[fx][0], s->pixbkgr);
    for (fx = fxbeg; fx <= fxfin; fx += s->zoom)
	COPYRGB(&pixbuf[fx-fxbeg][0], &s->pixgrid[0]);
    for (fy = fybeg; fy <= fyfin; fy++)
	(void) fb_write(s->fbiop, fxbeg, fy, (unsigned char *)pixbuf, fgridwid);
    for (fy = 0; fy < s->devwid; fy++)
	(void) fb_write(s->fbiop, fxorg, fy, s->pixaxis, 1);

    /* draw horizontal lines */
    if (s->zoom > 1) {
	for (fy = fybeg; fy <= fyfin; fy += s->zoom) {
	    (void) fb_write(s->fbiop, fxbeg, fy, s->pixgrid, fgridwid);
	}
    }
    for (fx = 0; fx < s->devwid; fx++) {
	COPYRGB(&pixbuf[fx][0], s->pixaxis);
    }
    (void) fb_write(s->fbiop, 0, fyorg, (unsigned char *)pixbuf, s->devwid);
    return;
}


void
paintCellFb(struct application *ap, unsigned char *pixpaint, unsigned char *pixexpendable)
{
    struct burst_state *s = (struct burst_state *)ap->a_uptr;
    int gx, gy;
    int gyfin, gxfin;
    int gxorg, gyorg;
    int x, y;
    int cnt;
    gridToFb(s, ap->a_x, ap->a_y, &gx, &gy);
    gxorg = gx+1;
    gyorg = gy+1;
    gxfin = s->zoom == 1 ? gx+s->zoom+1 : gx+s->zoom;
    gyfin = s->zoom == 1 ? gy+s->zoom+1 : gy+s->zoom;
    cnt = gxfin - gxorg;
    for (y = gyorg; y < gyfin; y++) {
	if (s->zoom != 1 && (y - gy) % s->zoom == 0) {
	    continue;
	}
	bu_semaphore_acquire(BU_SEM_GENERAL);
	(void) fb_read(s->fbiop, gxorg, y, (unsigned char *)pixbuf, cnt);
	bu_semaphore_release(BU_SEM_GENERAL);
	for (x = gxorg; x < gxfin; x++) {
	    if (SAMERGB(&pixbuf[x-gxorg][0], pixexpendable)) {
		COPYRGB(&pixbuf[x-gxorg][0], pixpaint);
	    }
	}
	bu_semaphore_acquire(BU_SEM_GENERAL);
	(void) fb_write(s->fbiop, gxorg, y, (unsigned char *)pixbuf, cnt);
	bu_semaphore_release(BU_SEM_GENERAL);
    }
    return;
}


void
paintSpallFb(struct application *ap)
{
    struct burst_state *s = (struct burst_state *)ap->a_uptr;
    unsigned char pixel[3];
    int x, y;
    int err;
    fastf_t celldist;
    pixel[RED] = ap->a_color[RED] * 255;
    pixel[GRN] = ap->a_color[GRN] * 255;
    pixel[BLU] = ap->a_color[BLU] * 255;
    gridToFb(s, ap->a_x, ap->a_y, &x, &y);
    CenterCell(x);	/* center of cell */
    CenterCell(y);
    celldist = ap->a_cumlen/s->cellsz * s->zoom;
    x = roundToInt(x + VDOT(ap->a_ray.r_dir, s->gridhor) * celldist);
    y = roundToInt(y + VDOT(ap->a_ray.r_dir, s->gridver) * celldist);
    bu_semaphore_acquire(BU_SEM_GENERAL);
    err = fb_write(s->fbiop, x, y, pixel, 1);
    bu_semaphore_release(BU_SEM_GENERAL);
    if (err == -1)
	brst_log(s, MSG_OUT, "Write failed to pixel <%d, %d> from cell <%d, %d>.\n", x, y, ap->a_x, ap->a_y);
    return;
}

/*
  Must be called after gridInit() so that gridsz is setup.
*/
int
openFbDevice(struct burst_state *s, struct bu_vls *fbdev)
{
    brst_log(s, MSG_LOG, "Opening frame buffer\n");
    if (s->zoom < 1) {
	brst_log(s, MSG_OUT, "Device is too small to display image.\n");
	return 0;
    }
    if (((s->fbiop != FB_NULL && fb_getwidth(s->fbiop) != s->devwid) || s->pixgrid == NULL)
	&& (s->pixgrid = (unsigned char *) calloc(s->devwid*3, sizeof(unsigned char))) == (unsigned char *) NULL) {
	bu_exit(EXIT_FAILURE, "Memory allocation of %ld bytes failed.", sizeof(unsigned char)*s->devwid);
    }
    (void) memset((char *) s->pixgrid, 0, sizeof(unsigned char)*s->devwid*3);
    if (s->fbiop != FB_NULL && fb_close(s->fbiop) == -1) {
	bu_exit(EXIT_FAILURE, "failed to close fb");
    }
    s->fbiop = fb_open(bu_vls_cstr(fbdev), s->devwid, s->devhgt);
    if (s->fbiop == NULL) {
	return 0;
    } else if (fb_clear(s->fbiop, s->pixblack) == -1
	       || (fb_zoom(s->fbiop, 1, 1) == -1)
	       || (fb_window(s->fbiop, s->devwid/2, s->devhgt/2) == -1)
	      ) {
	return 0;
    }

    return 1;
}

int
imageInit(struct burst_state *s)
{
    int needopen = 0;
    static char lastfbfile[LNBUFSZ]= {0}; /* last fbfile */
    s->devwid = 512;
    s->devhgt = 512;
    if (!bu_vls_strlen(&s->fbfile))
	return 1;
    s->zoom = s->devwid / s->gridsz - 1;
    while (s->zoom < 3  && s->devwid < MAXDEVWID) {
	s->devwid *= 2;
	s->devhgt *= 2;
	s->zoom = s->devwid / s->gridsz;
    }
    if (s->zoom * s->gridsz == s->devwid)
	s->zoom--;
    V_MAX(s->zoom, 1);

    /* Determine whether it is necessary to open fbfile. */
    if (s->fbiop == FB_NULL || fb_getwidth(s->fbiop) != s->devwid)
	needopen = 1; /* not currently open or size changed */
    else if (lastfbfile[0] != '\0' && !BU_STR_EQUAL(bu_vls_cstr(&s->fbfile), lastfbfile))
	needopen = 1; /* name changed */
    bu_strlcpy(lastfbfile, bu_vls_cstr(&s->fbfile), LNBUFSZ);

    if (needopen) {
	if (!openFbDevice(s, &s->fbfile))
	    return 0;
	paintGridFb(s);
    } else if (!(s->firemode & FM_SHOT) || (s->firemode & FM_FILE))
	paintGridFb(s);
    return 1;
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
