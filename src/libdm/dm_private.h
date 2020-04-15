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

#define DM_MIN (-2048)
#define DM_MAX (2047)

#define DM_O(_m) offsetof(struct dm, _m)

#define GED_MAX 2047.0
#define GED_MIN -2048.0
#define GED_RANGE 4095.0
#define INV_GED 0.00048828125
#define INV_4096 0.000244140625

/*
 * Display coordinate conversion:
 * GED is using -2048..+2048,
 * X is 0..width, 0..height
 */
#define DIVBY4096(x) (((double)(x))*INV_4096)
#define GED_TO_Xx(_dmp, x) ((int)((DIVBY4096(x)+0.5)*_dmp->dm_width))
#define GED_TO_Xy(_dmp, x) ((int)((0.5-DIVBY4096(x))*_dmp->dm_height))
#define Xx_TO_GED(_dmp, x) ((int)(((x)/(double)_dmp->dm_width - 0.5) * GED_RANGE))
#define Xy_TO_GED(_dmp, x) ((int)((0.5 - (x)/(double)_dmp->dm_height) * GED_RANGE))

/* +-2048 to +-1 */
#define GED_TO_PM1(x) (((fastf_t)(x))*INV_GED)

#ifdef IR_KNOBS
#  define NOISE 16		/* Size of dead spot on knob */
#endif

/* Line Styles */
#define DM_SOLID_LINE 0
#define DM_DASHED_LINE 1

/* Colors */
#define DM_COLOR_HI	((short)230)
#define DM_COLOR_LOW	((short)0)
#define DM_BLACK_R	DM_COLOR_LOW
#define DM_BLACK_G	DM_COLOR_LOW
#define DM_BLACK_B	DM_COLOR_LOW
#define DM_RED_R	DM_COLOR_HI
#define DM_RED_G	DM_COLOR_LOW
#define DM_RED_B	DM_COLOR_LOW
#define DM_BLUE_R	DM_COLOR_LOW
#define DM_BLUE_G	DM_COLOR_LOW
#define DM_BLUE_B	DM_COLOR_HI
#define DM_YELLOW_R	DM_COLOR_HI
#define DM_YELLOW_G	DM_COLOR_HI
#define DM_YELLOW_B	DM_COLOR_LOW
#define DM_WHITE_R	DM_COLOR_HI
#define DM_WHITE_G	DM_COLOR_HI
#define DM_WHITE_B	DM_COLOR_HI
#define DM_BLACK	DM_BLACK_R, DM_BLACK_G, DM_BLACK_B
#define DM_RED		DM_RED_R, DM_RED_G, DM_RED_B
#define DM_BLUE		DM_BLUE_R, DM_BLUE_G, DM_BLUE_B
#define DM_YELLOW	DM_YELLOW_R, DM_YELLOW_G, DM_YELLOW_B
#define DM_WHITE	DM_WHITE_R, DM_WHITE_G, DM_WHITE_B

#define DM_COPY_COLOR(_dr, _dg, _db, _sr, _sg, _sb) {\
	(_dr) = (_sr);\
	(_dg) = (_sg);\
	(_db) = (_sb); }
#define DM_SAME_COLOR(_dr, _dg, _db, _sr, _sg, _sb)(\
	(_dr) == (_sr) &&\
	(_dg) == (_sg) &&\
	(_db) == (_sb))
#if defined(DM_X) || defined(DM_OGL)
#define DM_REVERSE_COLOR_BYTE_ORDER(_shift, _mask) {	\
	_shift = 24 - _shift;				\
	switch (_shift) {				\
	    case 0:					\
		_mask >>= 24;				\
		break;					\
	    case 8:					\
		_mask >>= 8;				\
		break;					\
	    case 16:					\
		_mask <<= 8;				\
		break;					\
	    case 24:					\
		_mask <<= 24;				\
		break;					\
	}						\
    }
#else
/* Do nothing */
#define DM_REVERSE_COLOR_BYTE_ORDER(_shift, _mask)
#endif



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
