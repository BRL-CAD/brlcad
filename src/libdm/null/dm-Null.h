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

#ifndef DM_NULL_EXPORT
#  if defined(DM_DLL_EXPORTS) && defined(DM_DLL_IMPORTS)
#    error "Only DM_DLL_EXPORTS or DM_DLL_IMPORTS can be defined, not both."
#  elif defined(DM_DLL_EXPORTS)
#    define DM_NULL_EXPORT COMPILER_DLLEXPORT
#  elif defined(DM_DLL_IMPORTS)
#    define DM_NULL_EXPORT COMPILER_DLLIMPORT
#  else
#    define DM_NULL_EXPORT
#  endif
#endif

__BEGIN_DECLS

DM_NULL_EXPORT extern struct dm dm_null;


DM_NULL_EXPORT extern int
null_close(struct dm *dmp);


DM_NULL_EXPORT extern int
null_drawBegin(struct dm *dmp);


DM_NULL_EXPORT extern int
null_drawEnd(struct dm *dmp);


DM_NULL_EXPORT extern int
null_normal(struct dm *dmp);


DM_NULL_EXPORT extern int
null_loadMatrix(struct dm *dmp, fastf_t *mat, int which_eye);


DM_NULL_EXPORT extern int
null_loadPMatrix(struct dm *dmp, fastf_t *mat);


DM_NULL_EXPORT extern int
null_drawString2D(struct dm *dmp, const char *str, fastf_t x, fastf_t y, int size, int use_aspect);


DM_NULL_EXPORT extern int
null_drawLine2D(struct dm *dmp, fastf_t x_1, fastf_t y_1, fastf_t x_2, fastf_t y_2);


DM_NULL_EXPORT extern int
null_drawLine3D(struct dm *dmp, point_t pt1, point_t pt2);


DM_NULL_EXPORT extern int
null_drawLines3D(struct dm *dmp, int npoints, point_t *points, int sflag);


DM_NULL_EXPORT extern int
null_drawPoint2D(struct dm *dmp, fastf_t x, fastf_t y);


DM_NULL_EXPORT extern int
null_drawPoint3D(struct dm *dmp, point_t point);


DM_NULL_EXPORT extern int
null_drawPoints3D(struct dm *dmp, int npoints, point_t *points);


DM_NULL_EXPORT extern int
null_drawVList(struct dm *dmp, struct bn_vlist *vp);


DM_NULL_EXPORT extern int
null_drawVListHiddenLine(struct dm *dmp, struct bn_vlist *vp);


DM_NULL_EXPORT extern int
null_draw(struct dm *dmp, struct bn_vlist *(*callback_function)(void *), void **data);


DM_NULL_EXPORT extern int
null_setFGColor(struct dm *dmp, unsigned char r, unsigned char g, unsigned char b, int strict, fastf_t transparency);


DM_NULL_EXPORT extern int
null_setBGColor(struct dm *dmp, unsigned char r, unsigned char g, unsigned char b);


DM_NULL_EXPORT extern int
null_setLineAttr(struct dm *dmp, int width, int style);


DM_NULL_EXPORT extern int
null_configureWin(struct dm *dmp, int force);


DM_NULL_EXPORT extern int
null_setWinBounds(struct dm *dmp, fastf_t *w);


DM_NULL_EXPORT extern int
null_setLight(struct dm *dmp, int light_on);


DM_NULL_EXPORT extern int
null_setTransparency(struct dm *dmp, int transparency);


DM_NULL_EXPORT extern int
null_setDepthMask(struct dm *dmp, int mask);


DM_NULL_EXPORT extern int
null_setZBuffer(struct dm *dmp, int zbuffer_on);


DM_NULL_EXPORT extern int
null_debug(struct dm *dmp, int lvl);


DM_NULL_EXPORT extern int
null_beginDList(struct dm *dmp, unsigned int list);


DM_NULL_EXPORT extern int
null_endDList(struct dm *dmp);


DM_NULL_EXPORT extern int
null_drawDList(unsigned int list);


DM_NULL_EXPORT extern int
null_freeDLists(struct dm *dmp, unsigned int list, int range);


DM_NULL_EXPORT extern int
null_genDLists(struct dm *dmp, size_t range);


DM_NULL_EXPORT extern int
null_getDisplayImage(struct dm *dmp, unsigned char **image);


DM_NULL_EXPORT extern int
null_draw(struct dm *dmp, struct bn_vlist *(*callback_function)(void *), void **data);


DM_NULL_EXPORT extern int
null_fg(struct dm *dmp, unsigned char r, unsigned char g, unsigned char b, int strict, fastf_t transparency);


DM_NULL_EXPORT extern int
null_bg(struct dm *dmp, unsigned char r, unsigned char g, unsigned char b);


DM_NULL_EXPORT extern int
null_reshape(struct dm *dmp, int width, int height);


DM_NULL_EXPORT extern int
null_makeCurrent(struct dm *dmp);


DM_NULL_EXPORT extern int
null_doevent(struct dm *dmp, void *clientData, void *eventPtr);


DM_NULL_EXPORT extern void
null_processEvents(struct dm *dmp);


DM_NULL_EXPORT extern int
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
