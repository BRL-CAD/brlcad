/*                       D M - T K . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2020 United States Government as represented by
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
/** @file libdm/dm-tk.c
 *
 */

#include "common.h"

#ifdef HAVE_SYS_TIME_H
#  include <sys/time.h>
#endif

#include "tcl.h"
#include "vmath.h"
#include "dm.h"
#include "dm-Null.h"
#include "dm_private.h"


int
tk_close(struct dm_internal *UNUSED(dmp))
{
    return 0;
}


int
tk_drawBegin(struct dm_internal *UNUSED(dmp))
{
    return 0;
}


int
tk_drawEnd(struct dm_internal *UNUSED(dmp))
{
    return 0;
}


int
tk_normal(struct dm_internal *UNUSED(dmp))
{
    return 0;
}


int
tk_loadMatrix(struct dm_internal *UNUSED(dmp), fastf_t *UNUSED(mat), int UNUSED(which_eye))
{
    return 0;
}


int
tk_loadPMatrix(struct dm_internal *UNUSED(dmp), fastf_t *UNUSED(mat))
{
    return 0;
}


int
tk_drawString2D(struct dm_internal *UNUSED(dmp), const char *UNUSED(str), fastf_t UNUSED(x), fastf_t UNUSED(y), int UNUSED(size), int UNUSED(use_aspect))
{
    return 0;
}


int
tk_drawLine2D(struct dm_internal *UNUSED(dmp), fastf_t UNUSED(x_1), fastf_t UNUSED(y_1), fastf_t UNUSED(x_2), fastf_t UNUSED(y_2))
{
    return 0;
}


int
tk_drawLine3D(struct dm_internal *UNUSED(dmp), point_t UNUSED(pt1), point_t UNUSED(pt2))
{
    return 0;
}


int
tk_drawLines3D(struct dm_internal *UNUSED(dmp), int UNUSED(npoints), point_t *UNUSED(points), int UNUSED(sflag))
{
    return 0;
}


int
tk_drawPoint2D(struct dm_internal *UNUSED(dmp), fastf_t UNUSED(x), fastf_t UNUSED(y))
{
    return 0;
}


int
tk_drawPoint3D(struct dm_internal *UNUSED(dmp), point_t UNUSED(point))
{
    return 0;
}


int
tk_drawPoints3D(struct dm_internal *UNUSED(dmp), int UNUSED(npoints), point_t *UNUSED(points))
{
    return 0;
}


int
tk_drawVList(struct dm_internal *UNUSED(dmp), struct bn_vlist *UNUSED(vp))
{
    return 0;
}


int
tk_drawVListHiddenLine(struct dm_internal *UNUSED(dmp), struct bn_vlist *UNUSED(vp))
{
    return 0;
}


int
tk_draw(struct dm_internal *dmp, struct bn_vlist *(*callback_function)(void *), void **data)
{
    return dmp == NULL && callback_function == NULL && data == NULL;
}


int
tk_setFGColor(struct dm_internal *UNUSED(dmp), unsigned char UNUSED(r), unsigned char UNUSED(g), unsigned char UNUSED(b), int UNUSED(strict), fastf_t UNUSED(transparency))
{
    return 0;
}


int
tk_setBGColor(struct dm_internal *UNUSED(dmp), unsigned char UNUSED(r), unsigned char UNUSED(g), unsigned char UNUSED(b))
{
    return 0;
}


int
tk_setLineAttr(struct dm_internal *UNUSED(dmp), int UNUSED(width), int UNUSED(style))
{
    return 0;
}


int
tk_configureWin(struct dm_internal *UNUSED(dmp), int UNUSED(force))
{
    return 0;
}


int
tk_setWinBounds(struct dm_internal *UNUSED(dmp), fastf_t *UNUSED(w))
{
    return 0;
}


int
tk_setLight(struct dm_internal *UNUSED(dmp), int UNUSED(light_on))
{
    return 0;
}


int
tk_setTransparency(struct dm_internal *UNUSED(dmp), int UNUSED(transparency))
{
    return 0;
}


int
tk_setDepthMask(struct dm_internal *UNUSED(dmp), int UNUSED(mask))
{
    return 0;
}


int
tk_setZBuffer(struct dm_internal *UNUSED(dmp), int UNUSED(zbuffer_on))
{
    return 0;
}


int
tk_debug(struct dm_internal *UNUSED(dmp), int UNUSED(lvl))
{
    return 0;
}

int
tk_logfile(struct dm_internal *UNUSED(dmp), const char *UNUSED(filename))
{
    return 0;
}

int
tk_beginDList(struct dm_internal *UNUSED(dmp), unsigned int UNUSED(list))
{
    return 0;
}


int
tk_endDList(struct dm_internal *UNUSED(dmp))
{
    return 0;
}


int
tk_drawDList(unsigned int UNUSED(list))
{
    return 0;
}


int
tk_freeDLists(struct dm_internal *UNUSED(dmp), unsigned int UNUSED(list), int UNUSED(range))
{
    return 0;
}


int
tk_genDLists(struct dm_internal *UNUSED(dmp), size_t UNUSED(range))
{
    return 0;
}


int
tk_getDisplayImage(struct dm_internal *UNUSED(dmp), unsigned char **UNUSED(image))
{
    return 0;
}


int
tk_reshape(struct dm_internal *UNUSED(dmp), int UNUSED(width), int UNUSED(height))
{
    return 0;
}


int
tk_makeCurrent(struct dm_internal *UNUSED(dmp))
{
    return 0;
}


int
tk_openFb(struct dm_internal *UNUSED(dmp))
{
    return 0;
}


struct dm_internal dm_tk = {
    tk_close,
    tk_drawBegin,
    tk_drawEnd,
    tk_normal,
    tk_loadMatrix,
    tk_loadPMatrix,
    tk_drawString2D,
    tk_drawLine2D,
    tk_drawLine3D,
    tk_drawLines3D,
    tk_drawPoint2D,
    tk_drawPoint3D,
    tk_drawPoints3D,
    tk_drawVList,
    tk_drawVListHiddenLine,
    tk_draw,
    tk_setFGColor,
    tk_setBGColor,
    tk_setLineAttr,
    tk_configureWin,
    tk_setWinBounds,
    tk_setLight,
    tk_setTransparency,
    tk_setDepthMask,
    tk_setZBuffer,
    tk_debug,
    tk_logfile,
    tk_beginDList,
    tk_endDList,
    tk_drawDList,
    tk_freeDLists,
    tk_genDLists,
    NULL,
    tk_getDisplayImage,
    tk_reshape,
    tk_makeCurrent,
    tk_openFb,
    NULL,
    NULL,
    0,
    0,				/* no displaylist */
    0,				/* no stereo */
    0.0,			/* zoom-in limit */
    1,				/* bound flag */
    "nu",
    "Tk Display",
    DM_TYPE_NULL,
    0,/* top */
    0,/* width */
    0,/* height */
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
    {0, 0, 0},			/* bg color */
    {0, 0, 0},			/* fg color */
    {0.0, 0.0, 0.0},		/* clipmin */
    {0.0, 0.0, 0.0},		/* clipmax */
    0,				/* no debugging */
    BU_VLS_INIT_ZERO,		/* bu_vls logfile */
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
    0				/* Tcl interpreter */
};

struct dm_internal *tk_open_dm(Tcl_Interp *interp, int argc, char **argv);

struct dm_internal *
tk_open_dm(Tcl_Interp *UNUSED(interp), int UNUSED(argc), char **UNUSED(argv))
{
    return DM_NULL;
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
