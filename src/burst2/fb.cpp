/*                          F B . C P P
 * BRL-CAD
 *
 * Copyright (c) 2004-2020 United States Government as represented by
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
/** @file burst/fb.cpp
 *
 */

#include "common.h"

#include <stdio.h>
#include <string.h>
#include <memory.h>
#include <signal.h>

#include "vmath.h"

#include "bu/log.h"
#include "bu/str.h"
#include "fb.h"

#include "./burst.h"

/*
  Must be called after gridInit() so that gridsz is setup.
*/
int
openFbDevice(struct burst_state *s, struct bu_vls *fbdev)
{
    bu_log("Opening frame buffer");
    if (s->zoom < 1) {
	bu_log("Device is too small to display image.");
	return 1;
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
	return 1;
    }
    return 0;
}

int
imageInit(struct burst_state *s)
{
    int needopen = 0;
    static char lastfbfile[LNBUFSZ]={0}; /* last fbfile */
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
    else
	if (lastfbfile[0] != '\0' && !BU_STR_EQUAL(bu_vls_cstr(&s->fbfile), lastfbfile))
	    needopen = 1; /* name changed */
    bu_strlcpy(lastfbfile, bu_vls_cstr(&s->fbfile), LNBUFSZ);

    if (needopen) {
	if (!openFbDevice(s, &s->fbfile))
	    return 0;
	paintGridFb(s);
    } else
	if (!(s->firemode & FM_SHOT) || (s->firemode & FM_FILE))
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
