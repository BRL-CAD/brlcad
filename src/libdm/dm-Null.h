/*                          D M - N U L L . H
 * BRL-CAD
 *
 * Copyright (c) 1993-2016 United States Government as represented by
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
/** @addtogroup libdm */
/** @{ */
/** @file dm-Null.h
 *
 */
#ifndef DM_NULL_H
#define DM_NULL_H

#include "common.h"

#include "dm.h"

__BEGIN_DECLS

DM_EXPORT extern dm dm_null;


DM_EXPORT extern int
null_close(dm *dmp);


DM_EXPORT extern int
null_drawBegin(dm *dmp);


DM_EXPORT extern int
null_drawEnd(dm *dmp);


DM_EXPORT extern int
null_normal(dm *dmp);


DM_EXPORT extern int
null_loadMatrix(dm *dmp, fastf_t *mat, int which_eye);


DM_EXPORT extern int
null_loadPMatrix(dm *dmp, fastf_t *mat);


DM_EXPORT extern int
null_drawString2D(dm *dmp, const char *str, fastf_t x, fastf_t y, int size, int use_aspect);


DM_EXPORT extern int
null_drawLine2D(dm *dmp, fastf_t x_1, fastf_t y_1, fastf_t x_2, fastf_t y_2);


DM_EXPORT extern int
null_drawLine3D(dm *dmp, point_t pt1, point_t pt2);


DM_EXPORT extern int
null_drawLines3D(dm *dmp, int npoints, point_t *points, int sflag);


DM_EXPORT extern int
null_drawPoint2D(dm *dmp, fastf_t x, fastf_t y);


DM_EXPORT extern int
null_drawPoint3D(dm *dmp, point_t point);


DM_EXPORT extern int
null_drawPoints3D(dm *dmp, int npoints, point_t *points);


DM_EXPORT extern int
null_drawVList(dm *dmp, struct bn_vlist *vp);


DM_EXPORT extern int
null_drawVListHiddenLine(dm *dmp, struct bn_vlist *vp);


DM_EXPORT extern int
null_draw(dm *dmp, struct bn_vlist *(*callback_function)(void *), void **data);


DM_EXPORT extern int
null_setFGColor(dm *dmp, unsigned char r, unsigned char g, unsigned char b, int strict, fastf_t transparency);


DM_EXPORT extern int
null_setBGColor(dm *dmp, unsigned char r, unsigned char g, unsigned char b);


DM_EXPORT extern int
null_setLineAttr(dm *dmp, int width, int style);


DM_EXPORT extern int
null_configureWin(dm *dmp, int force);


DM_EXPORT extern int
null_setWinBounds(dm *dmp, fastf_t *w);


DM_EXPORT extern int
null_setLight(dm *dmp, int light_on);


DM_EXPORT extern int
null_setTransparency(dm *dmp, int transparency);


DM_EXPORT extern int
null_setDepthMask(dm *dmp, int mask);


DM_EXPORT extern int
null_setZBuffer(dm *dmp, int zbuffer_on);


DM_EXPORT extern int
null_debug(dm *dmp, int lvl);


DM_EXPORT extern int
null_beginDList(dm *dmp, unsigned int list);


DM_EXPORT extern int
null_endDList(dm *dmp);


DM_EXPORT extern void
null_drawDList(unsigned int list);


DM_EXPORT extern int
null_freeDLists(dm *dmp, unsigned int list, int range);


DM_EXPORT extern int
null_genDLists(dm *dmp, size_t range);


DM_EXPORT extern int
null_getDisplayImage(dm *dmp, unsigned char **image);


DM_EXPORT extern int
null_draw(dm *dmp, struct bn_vlist *(*callback_function)(void *), void **data);


DM_EXPORT extern int
null_fg(dm *dmp, unsigned char r, unsigned char g, unsigned char b, int strict, fastf_t transparency);


DM_EXPORT extern int
null_bg(dm *dmp, unsigned char r, unsigned char g, unsigned char b);


DM_EXPORT extern void
null_reshape(dm *dmp, int width, int height);


DM_EXPORT extern int
null_makeCurrent(dm *dmp);


DM_EXPORT extern void
null_processEvents(dm *dmp);


DM_EXPORT extern int
null_openFb(dm *dmp);

__END_DECLS

#endif  /* DM_NULL_H */

/** @} */
/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
