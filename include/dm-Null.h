/*                          D M - N U L L . H
 * BRL-CAD
 *
 * Copyright (c) 1993-2013 United States Government as represented by
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
#ifndef __DM_NULL__
#define __DM_NULL__

#include "common.h"

#include "dm.h"

__BEGIN_DECLS

DM_EXPORT extern struct dm dm_null;


DM_EXPORT extern int
null_close(struct dm *dmp);


DM_EXPORT extern int
null_drawBegin(struct dm *dmp);


DM_EXPORT extern int
null_drawEnd(struct dm *dmp);


DM_EXPORT extern int
null_normal(struct dm *dmp);


DM_EXPORT extern int
null_loadMatrix(struct dm *dmp, fastf_t *mat, int which_eye);


DM_EXPORT extern int
null_loadPMatrix(struct dm *dmp, fastf_t *mat);


DM_EXPORT extern int
null_drawString2D(struct dm *dmp, const char *str, fastf_t x, fastf_t y, int size, int use_aspect);


DM_EXPORT extern int
null_drawLine2D(struct dm *dmp, fastf_t x_1, fastf_t y_1, fastf_t x_2, fastf_t y_2);


DM_EXPORT extern int
null_drawLine3D(struct dm *dmp, point_t pt1, point_t pt2);


DM_EXPORT extern int
null_drawLines3D(struct dm *dmp, int npoints, point_t *points, int sflag);


DM_EXPORT extern int
null_drawPoint2D(struct dm *dmp, fastf_t x, fastf_t y);


DM_EXPORT extern int
null_drawPoint3D(struct dm *dmp, point_t point);


DM_EXPORT extern int
null_drawPoints3D(struct dm *dmp, int npoints, point_t *points);


DM_EXPORT extern int
null_drawVList(struct dm *dmp, struct bn_vlist *vp);


DM_EXPORT extern int
null_drawVListHiddenLine(struct dm *dmp, struct bn_vlist *vp);


DM_EXPORT extern int
null_draw(struct dm *dmp, struct bn_vlist *(*callback_function)(void *), genptr_t *data);


DM_EXPORT extern int
null_setFGColor(struct dm *dmp, unsigned char r, unsigned char g, unsigned char b, int strict, fastf_t transparency);


DM_EXPORT extern int
null_setBGColor(struct dm *dmp, unsigned char r, unsigned char g, unsigned char b);


DM_EXPORT extern int
null_setLineAttr(struct dm *dmp, int width, int style);


DM_EXPORT extern int
null_configureWin(struct dm *dmp, int force);


DM_EXPORT extern int
null_setWinBounds(struct dm *dmp, fastf_t *w);


DM_EXPORT extern int
null_setLight(struct dm *dmp, int light_on);


DM_EXPORT extern int
null_setTransparency(struct dm *dmp, int transparency);


DM_EXPORT extern int
null_setDepthMask(struct dm *dmp, int mask);


DM_EXPORT extern int
null_setZBuffer(struct dm *dmp, int zbuffer_on);


DM_EXPORT extern int
null_debug(struct dm *dmp, int lvl);


DM_EXPORT extern int
null_beginDList(struct dm *dmp, unsigned int list);


DM_EXPORT extern int
null_endDList(struct dm *dmp);


DM_EXPORT extern void
null_drawDList(unsigned int list);


DM_EXPORT extern int
null_freeDLists(struct dm *dmp, unsigned int list, int range);


DM_EXPORT extern int
null_genDLists(struct dm *dmp, size_t range);


DM_EXPORT extern int
null_getDisplayImage(struct dm *dmp, unsigned char **image);


DM_EXPORT extern int
null_draw(struct dm *dmp, struct bn_vlist *(*callback_function)(void *), genptr_t *data);


DM_EXPORT extern int
null_fg(struct dm *dmp, unsigned char r, unsigned char g, unsigned char b, int strict, fastf_t transparency);


DM_EXPORT extern int
null_bg(struct dm *dmp, unsigned char r, unsigned char g, unsigned char b);


DM_EXPORT extern void
null_reshape(struct dm *dmp, int width, int height);


DM_EXPORT extern int
null_makeCurrent(struct dm *dmp);


DM_EXPORT extern void
null_processEvents(struct dm *dmp);

__END_DECLS

#endif  /* __DM_NULL__ */

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
