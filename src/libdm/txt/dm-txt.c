/*                       D M - T X T . C
 * BRL-CAD
 *
 * Copyright (c) 2013-2021 United States Government as represented by
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
/** @file libdm/dm-txt.c
 *
 */

#include "common.h"

#ifdef HAVE_SYS_TIME_H
#  include <sys/time.h>
#endif

#include "tcl.h"
#include "vmath.h"
#include "dm.h"
#include "../include/private.h"

extern struct dm dm_txt;

struct dm *
txt_open(void *UNUSED(ctx), void *interp, int argc, const char **argv)
{
    struct dm *dmp;

    if (argc < 0 || !argv)
	return DM_NULL;

    BU_ALLOC(dmp, struct dm);
    dmp->magic = DM_MAGIC;
    dmp->start_time = 0;

    BU_ALLOC(dmp->i, struct dm_impl);

    *dmp->i = *dm_txt.i;
    dmp->i->dm_interp = interp;

    bu_log("open called\n");

    return dmp;
}


HIDDEN int
txt_close(struct dm *dmp)
{
    bu_log("close called\n");
    bu_free(dmp->i, "dmp impl");
    bu_free(dmp, "dmp");
    return 0;
}

int
txt_viable(const char *UNUSED(dpy_string))
{
    return 1;
}


HIDDEN int
txt_drawBegin(struct dm *UNUSED(dmp))
{
    bu_log("drawBegin called\n");
    return 0;
}


HIDDEN int
txt_drawEnd(struct dm *UNUSED(dmp))
{
    bu_log("drawEnd called\n");
    return 0;
}


HIDDEN int
txt_hud_begin(struct dm *UNUSED(dmp))
{
    bu_log("hud_begin called\n");
    return 0;
}

HIDDEN int
txt_hud_end(struct dm *UNUSED(dmp))
{
    bu_log("hud_end called\n");
    return 0;
}

HIDDEN int
txt_loadMatrix(struct dm *UNUSED(dmp), fastf_t *UNUSED(mat), int UNUSED(which_eye))
{
    bu_log("loadMatrix called\n");
    return 0;
}


HIDDEN int
txt_loadPMatrix(struct dm *UNUSED(dmp), fastf_t *UNUSED(mat))
{
    bu_log("loadPMatrix called\n");
    return 0;
}


HIDDEN int
txt_drawString2D(struct dm *UNUSED(dmp), const char *UNUSED(str), fastf_t UNUSED(x), fastf_t UNUSED(y), int UNUSED(size), int UNUSED(use_aspect))
{
    bu_log("drawString2D called\n");
    return 0;
}


HIDDEN int
txt_String2DBBox(struct dm *UNUSED(dmp), vect2d_t *UNUSED(bmin), vect2d_t *UNUSED(bmax), const char *UNUSED(str), fastf_t UNUSED(x), fastf_t UNUSED(y), int UNUSED(size), int UNUSED(use_aspect))
{
    bu_log("String2DBBox called\n");
    return 0;
}


HIDDEN int
txt_drawLine2D(struct dm *UNUSED(dmp), fastf_t UNUSED(x_1), fastf_t UNUSED(y_1), fastf_t UNUSED(x_2), fastf_t UNUSED(y_2))
{
    bu_log("drawLine2D called\n");
    return 0;
}


HIDDEN int
txt_drawLine3D(struct dm *UNUSED(dmp), point_t UNUSED(pt1), point_t UNUSED(pt2))
{
    bu_log("drawLine3D called\n");
    return 0;
}


HIDDEN int
txt_drawLines3D(struct dm *UNUSED(dmp), int UNUSED(npoints), point_t *UNUSED(points), int UNUSED(sflag))
{
    bu_log("drawLines3D called\n");
    return 0;
}


HIDDEN int
txt_drawPoint2D(struct dm *UNUSED(dmp), fastf_t UNUSED(x), fastf_t UNUSED(y))
{
    bu_log("drawPoint2D called\n");
    return 0;
}


HIDDEN int
txt_drawPoint3D(struct dm *UNUSED(dmp), point_t UNUSED(point))
{
    bu_log("drawPoint3D called\n");
    return 0;
}


HIDDEN int
txt_drawPoints3D(struct dm *UNUSED(dmp), int UNUSED(npoints), point_t *UNUSED(points))
{
    bu_log("drawPoints3D called\n");
    return 0;
}


HIDDEN int
txt_drawVList(struct dm *UNUSED(dmp), struct bv_vlist *UNUSED(vp))
{
    bu_log("drawVList called\n");
    return 0;
}


HIDDEN int
txt_drawVListHiddenLine(struct dm *UNUSED(dmp), struct bv_vlist *UNUSED(vp))
{
    bu_log("drawVListHiddenLine called\n");
    return 0;
}


HIDDEN int
txt_draw(struct dm *dmp, struct bv_vlist *(*callback_function)(void *), void **data)
{
    bu_log("draw called\n");
    return dmp == NULL && callback_function == NULL && data == NULL;
}


HIDDEN int
txt_setFGColor(struct dm *UNUSED(dmp), unsigned char UNUSED(r), unsigned char UNUSED(g), unsigned char UNUSED(b), int UNUSED(strict), fastf_t UNUSED(transparency))
{
    bu_log("setFGColor called\n");
    return 0;
}


HIDDEN int
txt_setBGColor(struct dm *UNUSED(dmp), unsigned char UNUSED(r), unsigned char UNUSED(g), unsigned char UNUSED(b))
{
    bu_log("setBGColor called\n");
    return 0;
}


HIDDEN int
txt_setLineAttr(struct dm *UNUSED(dmp), int UNUSED(width), int UNUSED(style))
{
    bu_log("setLineAttr called\n");
    return 0;
}


HIDDEN int
txt_configureWin(struct dm *UNUSED(dmp), int UNUSED(force))
{
    bu_log("configureWin called\n");
    return 0;
}


HIDDEN int
txt_setWinBounds(struct dm *UNUSED(dmp), fastf_t *UNUSED(w))
{
    bu_log("setWinBounds called\n");
    return 0;
}


HIDDEN int
txt_setLight(struct dm *UNUSED(dmp), int UNUSED(light_on))
{
    bu_log("setLight called\n");
    return 0;
}


HIDDEN int
txt_setTransparency(struct dm *UNUSED(dmp), int UNUSED(transparency))
{
    bu_log("setTransparency called\n");
    return 0;
}


HIDDEN int
txt_setDepthMask(struct dm *UNUSED(dmp), int UNUSED(mask))
{
    bu_log("setDepthMask called\n");
    return 0;
}


HIDDEN int
txt_setZBuffer(struct dm *UNUSED(dmp), int UNUSED(zbuffer_on))
{
    bu_log("setZBuffer called\n");
    return 0;
}


HIDDEN int
txt_debug(struct dm *UNUSED(dmp), int UNUSED(lvl))
{
    bu_log("debug called\n");
    return 0;
}


HIDDEN int
txt_logfile(struct dm *UNUSED(dmp), const char *UNUSED(filename))
{
    bu_log("logfile called\n");
    return 0;
}


HIDDEN int
txt_beginDList(struct dm *UNUSED(dmp), unsigned int UNUSED(list))
{
    bu_log("beginDList called\n");
    return 0;
}


HIDDEN int
txt_endDList(struct dm *UNUSED(dmp))
{
    bu_log("endDList called\n");
    return 0;
}


HIDDEN int
txt_drawDList(unsigned int UNUSED(list))
{
    bu_log("drawDList called\n");
    return 0;
}


HIDDEN int
txt_freeDLists(struct dm *UNUSED(dmp), unsigned int UNUSED(list), int UNUSED(range))
{
    bu_log("freeDList called\n");
    return 0;
}


HIDDEN int
txt_genDLists(struct dm *UNUSED(dmp), size_t UNUSED(range))
{
    bu_log("genDLists called\n");
    return 0;
}


HIDDEN int
txt_getDisplayImage(struct dm *UNUSED(dmp), unsigned char **UNUSED(image), int flip, int alpha)
{
    bu_log("getDisplayImage called (flip: %d, alpha: %d)\n", flip, alpha);
    return 0;
}


HIDDEN int
txt_reshape(struct dm *UNUSED(dmp), int UNUSED(width), int UNUSED(height))
{
    bu_log("reshape called\n");
    return 0;
}


HIDDEN int
txt_makeCurrent(struct dm *UNUSED(dmp))
{
    bu_log("makeCurrent called\n");
    return 0;
}

HIDDEN int
txt_SwapBuffers(struct dm *UNUSED(dmp))
{
    bu_log("SwapBuffers called\n");
    return 0;
}

HIDDEN int
txt_doevent(struct dm *UNUSED(dmp), void *UNUSED(vclientData), void *UNUSED(veventPtr))
{
    bu_log("doevent called\n");
    return 0;
}

HIDDEN int
txt_openFb(struct dm *UNUSED(dmp))
{
    bu_log("openFb called\n");
    return 0;
}


struct dm_impl dm_txt_impl = {
    txt_open,
    txt_close,
    txt_viable,
    txt_drawBegin,
    txt_drawEnd,
    txt_hud_begin,
    txt_hud_end,
    txt_loadMatrix,
    txt_loadPMatrix,
    txt_drawString2D,
    txt_String2DBBox,
    txt_drawLine2D,
    txt_drawLine3D,
    txt_drawLines3D,
    txt_drawPoint2D,
    txt_drawPoint3D,
    txt_drawPoints3D,
    txt_drawVList,
    txt_drawVListHiddenLine,
    NULL,
    txt_draw,
    txt_setFGColor,
    txt_setBGColor,
    txt_setLineAttr,
    txt_configureWin,
    txt_setWinBounds,
    txt_setLight,
    txt_setTransparency,
    txt_setDepthMask,
    txt_setZBuffer,
    txt_debug,
    txt_logfile,
    txt_beginDList,
    txt_endDList,
    txt_drawDList,
    txt_freeDLists,
    txt_genDLists,
    NULL,
    txt_getDisplayImage,
    txt_reshape,
    txt_makeCurrent,
    txt_SwapBuffers,
    txt_doevent,
    txt_openFb,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    0,
    0,				/* not graphical */
    NULL,                       /* not graphical */
    0,				/* no displaylist */
    0,				/* no stereo */
    0.0,			/* zoom-in limit */
    1,				/* bound flag */
    "txt",
    "Text Display",
    0,/* top */
    0,/* width */
    0,/* height */
    0,/* dirty */
    0,/* bytes per pixel */
    0,/* bits per channel */
    0,
    0,
    0,
    0,
    {0, 0},
    NULL,
    NULL,
    BU_VLS_INIT_ZERO,		/* bu_vls path name*/
    BU_VLS_INIT_ZERO,		/* bu_vls full name drawing window */
    BU_VLS_INIT_ZERO,		/* bu_vls short name drawing window */
    BU_VLS_INIT_ZERO,		/* bu_vls logfile */
    {0, 0, 0},			/* bg color */
    {0, 0, 0},			/* fg color */
    {0.0, 0.0, 0.0},		/* clipmin */
    {0.0, 0.0, 0.0},		/* clipmax */
    0,				/* no debugging */
    0,				/* no perspective */
    0,				/* no lighting */
    0,				/* no transparency */
    0,				/* depth buffer is not writable */
    0,				/* no zbuffer */
    0,				/* no zclipping */
    1,                          /* clear back buffer after drawing and swap */
    0,                          /* not overriding the auto font size */
    BU_STRUCTPARSE_NULL,
    FB_NULL,
    0,				/* Tcl interpreter */
    NULL,                       /* Drawing context */
    NULL                        /* App data */
};

struct dm dm_txt = { DM_MAGIC, &dm_txt_impl, 0 };

#ifdef DM_PLUGIN
const struct dm_plugin pinfo = { DM_API, &dm_txt };

COMPILER_DLLEXPORT const struct dm_plugin *dm_plugin_info()
{
    return &pinfo;
}
#endif

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
