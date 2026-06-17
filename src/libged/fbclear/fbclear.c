/*                        F B C L E A R . C
 * BRL-CAD
 *
 * Copyright (c) 1986-2026 United States Government as represented by
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
/** @file libged/fbclear.c
 *
 * Clear a framebuffer.
 *
 */

#include "common.h"

#include <stdlib.h>
#ifdef HAVE_SYS_TYPES_H
#  include <sys/types.h>
#endif
#ifdef HAVE_SYS_STAT_H
#  include <sys/stat.h>
#endif
#ifdef HAVE_WINSOCK_H
#  include <winsock.h>
#endif

#include "dm.h"
#include "ged.h"
#include "bu/str.h"
#include "bsg/view_state.h"


#define FB_CONSTRAIN(_v, _a, _b) \
    (((_v) > (_a)) ? ((_v) < (_b) ? (_v) : (_b)) : (_a))

int
ged_fbclear_core(struct ged *gedp, int argc, const char *argv[])
{
    static const char usage[] = "\nUsage: fbclear [-m] [rgb]\n  -m   also reset gv_fb_mode to 0 (turn the framebuffer underlay off)";

    int ret;
    int reset_mode = 0;
    unsigned char clearColor[3] = {0.0, 0.0 ,0.0};

    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    if (!gedp->ged_gvp || !gedp->ged_gvp->dmp) {
	bu_vls_printf(gedp->ged_result_str, "no display manager currently active");
	return BRLCAD_ERROR;
    }

    struct dm *dmp = (struct dm *)gedp->ged_gvp->dmp;
    struct fb *fbp = dm_get_fb(dmp);

    if (!fbp) {
	bu_vls_printf(gedp->ged_result_str, "display manager does not have a framebuffer");
	return BRLCAD_ERROR;
    }

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* Phase B2 (ert reliability): optional "-m" flag that also resets
     * gv_fb_mode to 0 so the user can wipe the framebuffer image *and*
     * restore the plain 3D view in a single command.  Without this the
     * faceplate-driven underlay pasted on top of the 3D scene by ert's
     * implicit `gv_fb_mode = 2` would persist (showing a black canvas
     * after `fbclear`) until the user separately runs the `view fb`
     * faceplate command. */
    int arg_start = 1;
    if (argc >= 2 && BU_STR_EQUAL(argv[1], "-m")) {
	reset_mode = 1;
	arg_start = 2;
    }

    if (argc - arg_start == 1) {
	int r, g, b;

	if (sscanf(argv[arg_start], "%d %d %d", &r, &g, &b) != 3) {
	    bu_log("fb_clear: bad color spec - %s", argv[arg_start]);
	    return BRLCAD_ERROR;
	}

	clearColor[RED] = FB_CONSTRAIN(r, 0, 255);
	clearColor[GRN] = FB_CONSTRAIN(g, 0, 255);
	clearColor[BLU] = FB_CONSTRAIN(b, 0, 255);

    } else if (argc - arg_start > 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    ret = fb_clear(fbp, clearColor);

    if (ret)
	return BRLCAD_ERROR;

    if (reset_mode)
	bsg_view_set_framebuffer_mode(gedp->ged_gvp, 0);

    (void)dm_draw_begin(dmp);
    fb_refresh(fbp, 0, 0, fb_getwidth(fbp), fb_getheight(fbp));
    (void)dm_draw_end(dmp);

    dm_set_native_repaint_pending(dmp, 1);

    return BRLCAD_OK;
}

#include "../include/plugin.h"

#define GED_FBCLEAR_COMMANDS(X, XID) \
    X(fbclear, ged_fbclear_core, GED_CMD_DEFAULT) \

GED_DECLARE_COMMAND_SET(GED_FBCLEAR_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST("libged_fbclear", 1, GED_FBCLEAR_COMMANDS)

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
