/*                    D M _ P R I V A T E . H
 * BRL-CAD
 *
 * Copyright (c) 2014-2020 United States Government as represented by
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
/** @file dm_private.h
 *
 * Internal header for the display manager library.
 *
 */

#include "common.h"

#ifndef DM_PRIVATE_H
#define DM_PRIVATE_H

#include "vmath.h"
#include "dm.h"

#include "dm/calltable.h"

#if defined(DM_OGL) || defined(DM_WGL)
#define Ogl_MV_O(_m) offsetof(struct modifiable_ogl_vars, _m)

struct modifiable_ogl_vars {
    dm *this_dm;
    int cueing_on;
    int zclipping_on;
    int zbuffer_on;
    int lighting_on;
    int transparency_on;
    int fastfog;
    double fogdensity;
    int zbuf;
    int rgb;
    int doublebuffer;
    int depth;
    int debug;
    struct bu_vls log;
    double bound;
    int boundFlag;
};
#endif


__BEGIN_DECLS

int
drawLine3D(struct dm_internal *dmp, point_t pt1, point_t pt2, const char *log_bu, float *wireColor);

int
drawLines3D(struct dm_internal *dmp, int npoints, point_t *points, int sflag, const char *log_bu, float *wireColor);

int
drawLine2D(struct dm_internal *dmp, fastf_t X1, fastf_t Y1, fastf_t X2, fastf_t Y2, const char *log_bu);

int
draw_Line3D(struct dm_internal *dmp, point_t pt1, point_t pt2);

void
flip_display_image_vertically(unsigned char *image, size_t width, size_t height);

void
dm_generic_hook(const struct bu_structparse *sdp,
	const char *name,
	void *base,
	const char *value,
	void *data);

__END_DECLS

/************************************************/
/* dm-*.c macros for autogenerating common code */
/************************************************/

#define HIDDEN_DM_FUNCTION_PROTOTYPES(_dmtype) \
    HIDDEN int _dmtype##_close(dm *dmp); \
    HIDDEN int _dmtype##_drawBegin(dm *dmp); \
    HIDDEN int _dmtype##_drawEnd(dm *dmp); \
    HIDDEN int _dmtype##_normal(dm *dmp); \
    HIDDEN int _dmtype##_loadMatrix(dm *dmp, fastf_t *mat, int which_eye); \
    HIDDEN int _dmtype##_drawString2D(dm *dmp, char *str, fastf_t x, fastf_t y, int size, int use_aspect); \
    HIDDEN int _dmtype##_drawLine2D(dm *dmp, fastf_t x_1, fastf_t y_1, fastf_t x_2, fastf_t y_2); \
    HIDDEN int _dmtype##_drawLine3D(dm *dmp, point_t pt1, point_t pt2); \
    HIDDEN int _dmtype##_drawLines3D(dm *dmp, int npoints, point_t *points, int sflag); \
    HIDDEN int _dmtype##_drawPoint2D(dm *dmp, fastf_t x, fastf_t y); \
    HIDDEN int _dmtype##_drawPoint3D(dm *dmp, point_t point); \
    HIDDEN int _dmtype##_drawPoints3D(dm *dmp, int npoints, point_t *points); \
    HIDDEN int _dmtype##_drawVList(dm *dmp, struct bn_vlist *vp); \
    HIDDEN int _dmtype##_draw(dm *dmp, struct bn_vlist *(*callback_function)(void *), void **data); \
    HIDDEN int _dmtype##_setFGColor(dm *dmp, unsigned char r, unsigned char g, unsigned char b, int strict, fastf_t transparency); \
    HIDDEN int _dmtype##_setBGColor(dm *dmp, unsigned char r, unsigned char g, unsigned char b); \
    HIDDEN int _dmtype##_setLineAttr(dm *dmp, int width, int style); \
    HIDDEN int _dmtype##_configureWin_guts(dm *dmp, int force); \
    HIDDEN int _dmtype##_configureWin(dm *dmp, int force);                    \
    HIDDEN int _dmtype##_setLight(dm *dmp, int lighting_on); \
    HIDDEN int _dmtype##_setTransparency(dm *dmp, int transparency_on); \
    HIDDEN int _dmtype##_setDepthMask(dm *dmp, int depthMask_on); \
    HIDDEN int _dmtype##_setZBuffer(dm *dmp, int zbuffer_on); \
    HIDDEN int _dmtype##_setWinBounds(dm *dmp, fastf_t *w); \
    HIDDEN int _dmtype##_debug(dm *dmp, int lvl); \
    HIDDEN int _dmtype##_beginDList(dm *dmp, unsigned int list); \
    HIDDEN int _dmtype##_endDList(dm *dmp); \
    HIDDEN int _dmtype##_drawDList(dm *dmp, unsigned int list); \
    HIDDEN int _dmtype##_freeDLists(dm *dmp, unsigned int list, int range); \
    HIDDEN int _dmtype##_getDisplayImage(dm *dmp, unsigned char **image);

#endif /* DM_PRIVATE_H */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
