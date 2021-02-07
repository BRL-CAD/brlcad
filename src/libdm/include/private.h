/*                    D M _ P R I V A T E . H
 * BRL-CAD
 *
 * Copyright (c) 2014-2021 United States Government as represented by
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
/** @file private.h
 *
 * Internal header for the display manager library.
 *
 */

#include "common.h"

#ifndef DM_PRIVATE_H
#define DM_PRIVATE_H

/* Opaque containers that hold the set of available libdm
 * backend implementations.  This is initialized at startup
 * and it's contents are cleared at the end by dm_init.cpp */
extern void *dm_backends;
extern void *fb_backends;

#include <limits.h> /* For INT_MAX */
#include <stdlib.h>

#include "vmath.h"
#include "dm.h"

#include "./calltable.h"

#define DM_MIN (-2048)
#define DM_MAX (2047)

#define DM_O(_m) offsetof(struct dm_impl, _m)

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
#define GED_TO_Xx(_dmp, x) ((int)((DIVBY4096(x)+0.5)*_dmp->i->dm_width))
#define GED_TO_Xy(_dmp, x) ((int)((0.5-DIVBY4096(x))*_dmp->i->dm_height))
#define Xx_TO_GED(_dmp, x) ((int)(((x)/(double)_dmp->i->dm_width - 0.5) * GED_RANGE))
#define Xy_TO_GED(_dmp, x) ((int)((0.5 - (x)/(double)_dmp->i->dm_height) * GED_RANGE))

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

__BEGIN_DECLS

int
drawLine2D(struct dm *dmp, fastf_t X1, fastf_t Y1, fastf_t X2, fastf_t Y2, const char *log_bu);

DM_EXPORT extern struct fb remote_interface; /* not in list[] */

/* Always included */
DM_EXPORT extern struct fb debug_interface, disk_interface, stk_interface;
DM_EXPORT extern struct fb memory_interface, fb_null_interface;


/* Shared memory (shmget et. al.) key common to multiple framebuffers */
#define SHMEM_KEY 42

/* Maximum memory buffer allocation.
 *
 * Care must be taken as this can result in a large default memory
 * allocation that can have an impact on performance or minimum system
 * requirements.  For example, 20*1024 results in a 20480x20480 buffer
 * and a 1.6GB allocation.  Using 32*1024 results in a 4GB allocation.
 */
#define FB_XMAXSCREEN 20*1024 /* 1.6GB */
#define FB_YMAXSCREEN 20*1024 /* 1.6GB */

/* setting to 1 turns on general intrface debugging for all fb types */
#define FB_DEBUG 0

/*
 * Structure of color map in shared memory region.  Has exactly the
 * same format as the SGI hardware "gammaramp" map Note that only the
 * lower 8 bits are significant.
 */
struct fb_cmap {
    short cmr[256];
    short cmg[256];
    short cmb[256];
};


/*
 * This defines the format of the in-memory framebuffer copy.  The
 * alpha component and reverse order are maintained for compatibility
 * with /dev/sgi
 */
struct fb_pixel {
    unsigned char blue;
    unsigned char green;
    unsigned char red;
    unsigned char alpha;
};


/* Clipping structure for zoom/pan operations */
struct fb_clip {
    int xpixmin;	/* view clipping planes clipped to pixel memory space*/
    int xpixmax;
    int ypixmin;
    int ypixmax;
    int xscrmin;	/* view clipping planes */
    int xscrmax;
    int yscrmin;
    int yscrmax;
    double oleft;	/* glOrtho parameters */
    double oright;
    double otop;
    double obottom;
};

DM_EXPORT extern int fb_sim_writerect(struct fb *ifp, int xmin, int ymin, int _width, int _height, const unsigned char *pp);
DM_EXPORT extern int fb_sim_bwwriterect(struct fb *ifp, int xmin, int ymin, int _width, int _height, const unsigned char *pp);

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
