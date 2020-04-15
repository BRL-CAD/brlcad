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
    struct dm *this_dm;
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
drawLine3D(struct dm *dmp, point_t pt1, point_t pt2, const char *log_bu, float *wireColor);

int
drawLines3D(struct dm *dmp, int npoints, point_t *points, int sflag, const char *log_bu, float *wireColor);

int
drawLine2D(struct dm *dmp, fastf_t X1, fastf_t Y1, fastf_t X2, fastf_t Y2, const char *log_bu);

int
draw_Line3D(struct dm *dmp, point_t pt1, point_t pt2);

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
    HIDDEN int _dmtype##_close(struct dm *dmp); \
    HIDDEN int _dmtype##_drawBegin(struct dm *dmp); \
    HIDDEN int _dmtype##_drawEnd(struct dm *dmp); \
    HIDDEN int _dmtype##_normal(struct dm *dmp); \
    HIDDEN int _dmtype##_loadMatrix(struct dm *dmp, fastf_t *mat, int which_eye); \
    HIDDEN int _dmtype##_drawString2D(struct dm *dmp, char *str, fastf_t x, fastf_t y, int size, int use_aspect); \
    HIDDEN int _dmtype##_drawLine2D(struct dm *dmp, fastf_t x_1, fastf_t y_1, fastf_t x_2, fastf_t y_2); \
    HIDDEN int _dmtype##_drawLine3D(struct dm *dmp, point_t pt1, point_t pt2); \
    HIDDEN int _dmtype##_drawLines3D(struct dm *dmp, int npoints, point_t *points, int sflag); \
    HIDDEN int _dmtype##_drawPoint2D(struct dm *dmp, fastf_t x, fastf_t y); \
    HIDDEN int _dmtype##_drawPoint3D(struct dm *dmp, point_t point); \
    HIDDEN int _dmtype##_drawPoints3D(struct dm *dmp, int npoints, point_t *points); \
    HIDDEN int _dmtype##_drawVList(struct dm *dmp, struct bn_vlist *vp); \
    HIDDEN int _dmtype##_draw(struct dm *dmp, struct bn_vlist *(*callback_function)(void *), void **data); \
    HIDDEN int _dmtype##_setFGColor(struct dm *dmp, unsigned char r, unsigned char g, unsigned char b, int strict, fastf_t transparency); \
    HIDDEN int _dmtype##_setBGColor(struct dm *dmp, unsigned char r, unsigned char g, unsigned char b); \
    HIDDEN int _dmtype##_setLineAttr(struct dm *dmp, int width, int style); \
    HIDDEN int _dmtype##_configureWin_guts(struct dm *dmp, int force); \
    HIDDEN int _dmtype##_configureWin(struct dm *dmp, int force);                    \
    HIDDEN int _dmtype##_setLight(struct dm *dmp, int lighting_on); \
    HIDDEN int _dmtype##_setTransparency(struct dm *dmp, int transparency_on); \
    HIDDEN int _dmtype##_setDepthMask(struct dm *dmp, int depthMask_on); \
    HIDDEN int _dmtype##_setZBuffer(struct dm *dmp, int zbuffer_on); \
    HIDDEN int _dmtype##_setWinBounds(struct dm *dmp, fastf_t *w); \
    HIDDEN int _dmtype##_debug(struct dm *dmp, int lvl); \
    HIDDEN int _dmtype##_beginDList(struct dm *dmp, unsigned int list); \
    HIDDEN int _dmtype##_endDList(struct dm *dmp); \
    HIDDEN int _dmtype##_drawDList(struct dm *dmp, unsigned int list); \
    HIDDEN int _dmtype##_freeDLists(struct dm *dmp, unsigned int list, int range); \
    HIDDEN int _dmtype##_getDisplayImage(struct dm *dmp, unsigned char **image);

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
