/*                    D M _ P R I V A T E . H
 * BRL-CAD
 *
 * Copyright (c) 2014-2016 United States Government as represented by
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

struct dm_vars {
    void *pub_vars;
    void *priv_vars;
};

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

/**
 * Interface to a specific Display Manager
 */
struct dm_internal {
    int (*dm_close)(struct dm_internal *dmp);
    int (*dm_drawBegin)(struct dm_internal *dmp);	/**< @brief formerly dmr_prolog */
    int (*dm_drawEnd)(struct dm_internal *dmp);		/**< @brief formerly dmr_epilog */
    int (*dm_normal)(struct dm_internal *dmp);
    int (*dm_loadMatrix)(struct dm_internal *dmp, fastf_t *mat, int which_eye);
    int (*dm_loadPMatrix)(struct dm_internal *dmp, fastf_t *mat);
    int (*dm_drawString2D)(struct dm_internal *dmp, const char *str, fastf_t x, fastf_t y, int size, int use_aspect);	/**< @brief formerly dmr_puts */
    int (*dm_drawLine2D)(struct dm_internal *dmp, fastf_t x_1, fastf_t y_1, fastf_t x_2, fastf_t y_2);	/**< @brief formerly dmr_2d_line */
    int (*dm_drawLine3D)(struct dm_internal *dmp, point_t pt1, point_t pt2);
    int (*dm_drawLines3D)(struct dm_internal *dmp, int npoints, point_t *points, int sflag);
    int (*dm_drawPoint2D)(struct dm_internal *dmp, fastf_t x, fastf_t y);
    int (*dm_drawPoint3D)(struct dm_internal *dmp, point_t point);
    int (*dm_drawPoints3D)(struct dm_internal *dmp, int npoints, point_t *points);
    int (*dm_drawVList)(struct dm_internal *dmp, struct bn_vlist *vp);
    int (*dm_drawVListHiddenLine)(struct dm_internal *dmp, register struct bn_vlist *vp);
    int (*dm_draw)(struct dm_internal *dmp, struct bn_vlist *(*callback_function)(void *), void **data);	/**< @brief formerly dmr_object */
    int (*dm_setFGColor)(struct dm_internal *dmp, unsigned char r, unsigned char g, unsigned char b, int strict, fastf_t transparency);
    int (*dm_setBGColor)(struct dm_internal *, unsigned char, unsigned char, unsigned char);
    int (*dm_setLineAttr)(struct dm_internal *dmp, int width, int style);	/**< @brief currently - linewidth, (not-)dashed */
    int (*dm_configureWin)(struct dm_internal *dmp, int force);
    int (*dm_setWinBounds)(struct dm_internal *dmp, fastf_t *w);
    int (*dm_setLight)(struct dm_internal *dmp, int light_on);
    int (*dm_setTransparency)(struct dm_internal *dmp, int transparency_on);
    int (*dm_setDepthMask)(struct dm_internal *dmp, int depthMask_on);
    int (*dm_setZBuffer)(struct dm_internal *dmp, int zbuffer_on);
    int (*dm_debug)(struct dm_internal *dmp, int lvl);		/**< @brief Set DM debug level */
    int (*dm_logfile)(struct dm_internal *dmp, const char *filename); /**< @brief Set DM log file */
    int (*dm_beginDList)(struct dm_internal *dmp, unsigned int list);
    int (*dm_endDList)(struct dm_internal *dmp);
    void (*dm_drawDList)(unsigned int list);
    int (*dm_freeDLists)(struct dm_internal *dmp, unsigned int list, int range);
    int (*dm_genDLists)(struct dm_internal *dmp, size_t range);
    int (*dm_draw_obj)(struct dm_internal *dmp, struct display_list *obj);
    int (*dm_getDisplayImage)(struct dm_internal *dmp, unsigned char **image);  /**< @brief (0,0) is upper left pixel */
    void (*dm_reshape)(struct dm_internal *dmp, int width, int height);
    int (*dm_makeCurrent)(struct dm_internal *dmp);
    int (*dm_openFb)(struct dm_internal *dmp);
    void (*dm_get_internal)(struct dm_internal *dmp);
    void (*dm_put_internal)(struct dm_internal *dmp);
    unsigned long dm_id;          /**< @brief window id */
    int dm_displaylist;		/**< @brief !0 means device has displaylist */
    int dm_stereo;                /**< @brief stereo flag */
    double dm_bound;		/**< @brief zoom-in limit */
    int dm_boundFlag;
    const char *dm_name;		/**< @brief short name of device */
    const char *dm_lname;		/**< @brief long name of device */
    int dm_type;			/**< @brief display manager type */
    int dm_top;                   /**< @brief !0 means toplevel window */
    int dm_width;
    int dm_height;
    int dm_bytes_per_pixel;
    int dm_bits_per_channel;  /* bits per color channel */
    int dm_lineWidth;
    int dm_lineStyle;
    fastf_t dm_aspect;
    fastf_t *dm_vp;		/**< @brief (FIXME: ogl still depends on this) Viewscale pointer */
    struct dm_vars dm_vars;	/**< @brief display manager dependent variables */
    void *m_vars;
    void *p_vars;
    struct bu_vls dm_pathName;	/**< @brief full Tcl/Tk name of drawing window */
    struct bu_vls dm_tkName;	/**< @brief short Tcl/Tk name of drawing window */
    struct bu_vls dm_dName;	/**< @brief Display name */
    unsigned char dm_bg[3];	/**< @brief background color */
    unsigned char dm_fg[3];	/**< @brief foreground color */
    vect_t dm_clipmin;		/**< @brief minimum clipping vector */
    vect_t dm_clipmax;		/**< @brief maximum clipping vector */
    int dm_debugLevel;		/**< @brief !0 means debugging */
    struct bu_vls dm_log;   /**< @brief !NULL && !empty means log debug output to the file */
    int dm_perspective;		/**< @brief !0 means perspective on */
    int dm_light;			/**< @brief !0 means lighting on */
    int dm_transparency;		/**< @brief !0 means transparency on */
    int dm_depthMask;		/**< @brief !0 means depth buffer is writable */
    int dm_zbuffer;		/**< @brief !0 means zbuffer on */
    int dm_zclip;			/**< @brief !0 means zclipping */
    int dm_clearBufferAfter;	/**< @brief 1 means clear back buffer after drawing and swap */
    int dm_fontsize;		/**< @brief !0 override's the auto font size */
    struct bu_structparse *vparse;    /**< @brief Table listing settable variables */
    fb *fbp;                    /**< @brief Framebuffer associated with this display instance */
    Tcl_Interp *dm_interp;	/**< @brief Tcl interpreter */
};

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
