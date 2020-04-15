/*                    C A L L T A B L E . H
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
/** @file calltable.h
 *
 * Internal call table structure for the display manager library.
 *
 * This API is exposed only to support supplying display manager backends as
 * plugins - calling codes should *NOT* depend on this API to remain stable.
 * From their perspective it is considered a private implementation detail.
 *
 * Instead, they should use the dm_*() functions in dm.h
 *
 */

#include "common.h"

#ifndef DM_CALLTABLE_H
#define DM_CALLTABLE_H

#include "bu/parse.h"
#include "bu/vls.h"
#include "dm.h"

__BEGIN_DECLS

struct dm_vars {
    void *pub_vars;
    void *priv_vars;
};

/**
 * Interface to a specific Display Manager
 *
 * TODO - see how much of this can be set up as private, backend specific
 * variables behind the vparse.  The txt backend, for example, doesn't need
 * Tk information...
 */
struct dm {
    int (*dm_close)(struct dm *dmp);
    int (*dm_drawBegin)(struct dm *dmp);	/**< @brief formerly dmr_prolog */
    int (*dm_drawEnd)(struct dm *dmp);		/**< @brief formerly dmr_epilog */
    int (*dm_normal)(struct dm *dmp);
    int (*dm_loadMatrix)(struct dm *dmp, fastf_t *mat, int which_eye);
    int (*dm_loadPMatrix)(struct dm *dmp, fastf_t *mat);
    int (*dm_drawString2D)(struct dm *dmp, const char *str, fastf_t x, fastf_t y, int size, int use_aspect);	/**< @brief formerly dmr_puts */
    int (*dm_drawLine2D)(struct dm *dmp, fastf_t x_1, fastf_t y_1, fastf_t x_2, fastf_t y_2);	/**< @brief formerly dmr_2d_line */
    int (*dm_drawLine3D)(struct dm *dmp, point_t pt1, point_t pt2);
    int (*dm_drawLines3D)(struct dm *dmp, int npoints, point_t *points, int sflag);
    int (*dm_drawPoint2D)(struct dm *dmp, fastf_t x, fastf_t y);
    int (*dm_drawPoint3D)(struct dm *dmp, point_t point);
    int (*dm_drawPoints3D)(struct dm *dmp, int npoints, point_t *points);
    int (*dm_drawVList)(struct dm *dmp, struct bn_vlist *vp);
    int (*dm_drawVListHiddenLine)(struct dm *dmp, register struct bn_vlist *vp);
    int (*dm_draw)(struct dm *dmp, struct bn_vlist *(*callback_function)(void *), void **data);	/**< @brief formerly dmr_object */
    int (*dm_setFGColor)(struct dm *dmp, unsigned char r, unsigned char g, unsigned char b, int strict, fastf_t transparency);
    int (*dm_setBGColor)(struct dm *, unsigned char, unsigned char, unsigned char);
    int (*dm_setLineAttr)(struct dm *dmp, int width, int style);	/**< @brief currently - linewidth, (not-)dashed */
    int (*dm_configureWin)(struct dm *dmp, int force);
    int (*dm_setWinBounds)(struct dm *dmp, fastf_t *w);
    int (*dm_setLight)(struct dm *dmp, int light_on);
    int (*dm_setTransparency)(struct dm *dmp, int transparency_on);
    int (*dm_setDepthMask)(struct dm *dmp, int depthMask_on);
    int (*dm_setZBuffer)(struct dm *dmp, int zbuffer_on);
    int (*dm_debug)(struct dm *dmp, int lvl);		/**< @brief Set DM debug level */
    int (*dm_logfile)(struct dm *dmp, const char *filename); /**< @brief Set DM log file */
    int (*dm_beginDList)(struct dm *dmp, unsigned int list);
    int (*dm_endDList)(struct dm *dmp);
    int (*dm_drawDList)(unsigned int list);
    int (*dm_freeDLists)(struct dm *dmp, unsigned int list, int range);
    int (*dm_genDLists)(struct dm *dmp, size_t range);
    int (*dm_draw_obj)(struct dm *dmp, struct display_list *obj);
    int (*dm_getDisplayImage)(struct dm *dmp, unsigned char **image);  /**< @brief (0,0) is upper left pixel */
    int (*dm_reshape)(struct dm *dmp, int width, int height);
    int (*dm_makeCurrent)(struct dm *dmp);
    int (*dm_openFb)(struct dm *dmp);
    int (*dm_get_internal)(struct dm *dmp);
    int (*dm_put_internal)(struct dm *dmp);
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
    struct fb *fbp;                    /**< @brief Framebuffer associated with this display instance */
    void *dm_interp;		/**< @brief interpreter */
};


__END_DECLS

#endif /* DM_CALLTABLE_H */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
