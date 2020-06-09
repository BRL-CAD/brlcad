/*                          D M - N U L L . H
 * BRL-CAD
 *
 * Copyright (c) 1993-2020 United States Government as represented by
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
/** @addtogroup libstruct dm */
/** @{ */
/** @file dm-Null.h
 *
 */
#ifndef DM_NULL_H
#define DM_NULL_H

#include "common.h"

#include "dm.h"

__BEGIN_DECLS

extern struct dm dm_null;


extern int
null_close(struct dm *dmp);


extern int
null_drawBegin(struct dm *dmp);


extern int
null_drawEnd(struct dm *dmp);


extern int
null_normal(struct dm *dmp);


extern int
null_loadMatrix(struct dm *dmp, fastf_t *mat, int which_eye);


extern int
null_loadPMatrix(struct dm *dmp, fastf_t *mat);


extern int
null_drawString2D(struct dm *dmp, const char *str, fastf_t x, fastf_t y, int size, int use_aspect);


extern int
null_drawLine2D(struct dm *dmp, fastf_t x_1, fastf_t y_1, fastf_t x_2, fastf_t y_2);


extern int
null_drawLine3D(struct dm *dmp, point_t pt1, point_t pt2);


extern int
null_drawLines3D(struct dm *dmp, int npoints, point_t *points, int sflag);


extern int
null_drawPoint2D(struct dm *dmp, fastf_t x, fastf_t y);


extern int
null_drawPoint3D(struct dm *dmp, point_t point);


extern int
null_drawPoints3D(struct dm *dmp, int npoints, point_t *points);


extern int
null_drawVList(struct dm *dmp, struct bn_vlist *vp);


extern int
null_drawVListHiddenLine(struct dm *dmp, struct bn_vlist *vp);


extern int
null_draw(struct dm *dmp, struct bn_vlist *(*callback_function)(void *), void **data);


extern int
null_setFGColor(struct dm *dmp, unsigned char r, unsigned char g, unsigned char b, int strict, fastf_t transparency);


extern int
null_setBGColor(struct dm *dmp, unsigned char r, unsigned char g, unsigned char b);


extern int
null_setLineAttr(struct dm *dmp, int width, int style);


extern int
null_configureWin(struct dm *dmp, int force);


extern int
null_setWinBounds(struct dm *dmp, fastf_t *w);


extern int
null_setLight(struct dm *dmp, int light_on);


extern int
null_setTransparency(struct dm *dmp, int transparency);


extern int
null_setDepthMask(struct dm *dmp, int mask);


extern int
null_setZBuffer(struct dm *dmp, int zbuffer_on);


extern int
null_debug(struct dm *dmp, int lvl);


extern int
null_beginDList(struct dm *dmp, unsigned int list);


extern int
null_endDList(struct dm *dmp);


extern int
null_drawDList(unsigned int list);


extern int
null_freeDLists(struct dm *dmp, unsigned int list, int range);


extern int
null_genDLists(struct dm *dmp, size_t range);


extern int
null_getDisplayImage(struct dm *dmp, unsigned char **image);


extern int
null_draw(struct dm *dmp, struct bn_vlist *(*callback_function)(void *), void **data);


extern int
null_fg(struct dm *dmp, unsigned char r, unsigned char g, unsigned char b, int strict, fastf_t transparency);


extern int
null_bg(struct dm *dmp, unsigned char r, unsigned char g, unsigned char b);


extern int
null_reshape(struct dm *dmp, int width, int height);


extern int
null_makeCurrent(struct dm *dmp);


extern void
null_processEvents(struct dm *dmp);


extern int
null_openFb(struct dm *dmp);

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
