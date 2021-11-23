/*                       D M - N U L L . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2021 United States Government as represented by
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
/** @file libdm/dm-Null.c
 *
 */

#include "common.h"

#ifdef HAVE_SYS_TIME_H
#  include <sys/time.h>
#endif

#include "vmath.h"
#include "bu/malloc.h"
#include "dm.h"
#include "../null/dm-Null.h"
#include "../include/private.h"

struct dm *
null_open(void *UNUSED(ctx), void *interp, int UNUSED(argc), const char **UNUSED(argv))
{
    struct dm *dmp = DM_NULL;

    BU_ALLOC(dmp, struct dm);
    dmp->magic = DM_MAGIC;
    dmp->start_time = 0;

    BU_ALLOC(dmp->i, struct dm_impl);

    *dmp->i = *dm_null.i;
    dmp->i->dm_interp = interp;

    return dmp;
}

int
null_close(struct dm *UNUSED(dmp))
{
    return 0;
}

int
null_viable(const char *UNUSED(dpy_string))
{
    return 1;
}

int
null_drawBegin(struct dm *UNUSED(dmp))
{
    return 0;
}


int
null_drawEnd(struct dm *UNUSED(dmp))
{
    return 0;
}


int
null_hud_begin(struct dm *UNUSED(dmp))
{
    return 0;
}

int
null_hud_end(struct dm *UNUSED(dmp))
{
    return 0;
}

int
null_loadMatrix(struct dm *UNUSED(dmp), fastf_t *UNUSED(mat), int UNUSED(which_eye))
{
    return 0;
}


int
null_loadPMatrix(struct dm *UNUSED(dmp), fastf_t *UNUSED(mat))
{
    return 0;
}


int
null_drawString2D(struct dm *UNUSED(dmp), const char *UNUSED(str), fastf_t UNUSED(x), fastf_t UNUSED(y), int UNUSED(size), int UNUSED(use_aspect))
{
    return 0;
}

int
null_String2DBBox(struct dm *UNUSED(dmp), vect2d_t *UNUSED(bmin), vect2d_t *UNUSED(bmax), const char *UNUSED(str), fastf_t UNUSED(x), fastf_t UNUSED(y), int UNUSED(size), int UNUSED(use_aspect))
{
    return 0;
}

int
null_drawLine2D(struct dm *UNUSED(dmp), fastf_t UNUSED(x_1), fastf_t UNUSED(y_1), fastf_t UNUSED(x_2), fastf_t UNUSED(y_2))
{
    return 0;
}


int
null_drawLine3D(struct dm *UNUSED(dmp), point_t UNUSED(pt1), point_t UNUSED(pt2))
{
    return 0;
}


int
null_drawLines3D(struct dm *UNUSED(dmp), int UNUSED(npoints), point_t *UNUSED(points), int UNUSED(sflag))
{
    return 0;
}


int
null_drawPoint2D(struct dm *UNUSED(dmp), fastf_t UNUSED(x), fastf_t UNUSED(y))
{
    return 0;
}


int
null_drawPoint3D(struct dm *UNUSED(dmp), point_t UNUSED(point))
{
    return 0;
}


int
null_drawPoints3D(struct dm *UNUSED(dmp), int UNUSED(npoints), point_t *UNUSED(points))
{
    return 0;
}


int
null_drawVList(struct dm *UNUSED(dmp), struct bv_vlist *UNUSED(vp))
{
    return 0;
}


int
null_drawVListHiddenLine(struct dm *UNUSED(dmp), struct bv_vlist *UNUSED(vp))
{
    return 0;
}


int
null_draw(struct dm *dmp, struct bv_vlist *(*callback_function)(void *), void **data)
{
    return dmp == NULL && callback_function == NULL && data == NULL;
}


int
null_setFGColor(struct dm *UNUSED(dmp), unsigned char UNUSED(r), unsigned char UNUSED(g), unsigned char UNUSED(b), int UNUSED(strict), fastf_t UNUSED(transparency))
{
    return 0;
}


int
null_setBGColor(struct dm *UNUSED(dmp), unsigned char UNUSED(r), unsigned char UNUSED(g), unsigned char UNUSED(b))
{
    return 0;
}


int
null_setLineAttr(struct dm *UNUSED(dmp), int UNUSED(width), int UNUSED(style))
{
    return 0;
}


int
null_configureWin(struct dm *UNUSED(dmp), int UNUSED(force))
{
    return 0;
}


int
null_setWinBounds(struct dm *UNUSED(dmp), fastf_t *UNUSED(w))
{
    return 0;
}


int
null_setLight(struct dm *UNUSED(dmp), int UNUSED(light_on))
{
    return 0;
}


int
null_setTransparency(struct dm *UNUSED(dmp), int UNUSED(transparency))
{
    return 0;
}


int
null_setDepthMask(struct dm *UNUSED(dmp), int UNUSED(mask))
{
    return 0;
}


int
null_setZBuffer(struct dm *UNUSED(dmp), int UNUSED(zbuffer_on))
{
    return 0;
}


int
null_debug(struct dm *UNUSED(dmp), int UNUSED(lvl))
{
    return 0;
}

int
null_logfile(struct dm *UNUSED(dmp), const char *UNUSED(filename))
{
    return 0;
}

int
null_beginDList(struct dm *UNUSED(dmp), unsigned int UNUSED(list))
{
    return 0;
}


int
null_endDList(struct dm *UNUSED(dmp))
{
    return 0;
}


int
null_drawDList(unsigned int UNUSED(list))
{
    return 0;
}


int
null_freeDLists(struct dm *UNUSED(dmp), unsigned int UNUSED(list), int UNUSED(range))
{
    return 0;
}


int
null_genDLists(struct dm *UNUSED(dmp), size_t UNUSED(range))
{
    return 0;
}


int
null_getDisplayImage(struct dm *UNUSED(dmp), unsigned char **UNUSED(image), int UNUSED(flip), int UNUSED(alpha))
{
    return 0;
}


int
null_reshape(struct dm *UNUSED(dmp), int UNUSED(width), int UNUSED(height))
{
    return 0;
}


int
null_makeCurrent(struct dm *UNUSED(dmp))
{
    return 0;
}

int
null_SwapBuffers(struct dm *UNUSED(dmp))
{
    return 0;
}

int
null_doevent(struct dm *UNUSED(dmp), void *UNUSED(clientData), void *UNUSED(eventPtr))
{
    return 0;
}

int
null_openFb(struct dm *UNUSED(dmp))
{
    return 0;
}


struct dm_impl dm_null_impl = {
    null_open,
    null_close,
    null_viable,
    null_drawBegin,
    null_drawEnd,
    null_hud_begin,
    null_hud_end,
    null_loadMatrix,
    null_loadPMatrix,
    null_drawString2D,
    null_String2DBBox,
    null_drawLine2D,
    null_drawLine3D,
    null_drawLines3D,
    null_drawPoint2D,
    null_drawPoint3D,
    null_drawPoints3D,
    null_drawVList,
    null_drawVListHiddenLine,
    NULL,
    null_draw,
    null_setFGColor,
    null_setBGColor,
    null_setLineAttr,
    null_configureWin,
    null_setWinBounds,
    null_setLight,
    null_setTransparency,
    null_setDepthMask,
    null_setZBuffer,
    null_debug,
    null_logfile,
    null_beginDList,
    null_endDList,
    null_drawDList,
    null_freeDLists,
    null_genDLists,
    NULL,
    null_getDisplayImage,
    null_reshape,
    null_makeCurrent,
    null_SwapBuffers,
    null_doevent,
    null_openFb,
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
    "nu",
    "Null Display",
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

struct dm dm_null = { DM_MAGIC, &dm_null_impl, 0 };

#ifdef DM_PLUGIN
const struct dm_plugin pinfo = { &dm_null };

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
