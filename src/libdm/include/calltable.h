/*                    C A L L T A B L E . H
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

#include "png.h"

#include "bu/parse.h"
#include "bu/vls.h"
#include "dm.h"
#include "brlcad_version.h"

__BEGIN_DECLS

#define DM_API (1*1000000 + (BRLCAD_VERSION_MAJOR*10000) + (BRLCAD_VERSION_MINOR*100) + BRLCAD_VERSION_PATCH)

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
struct dm_impl {
    struct dm *(*dm_open)(void *ctx, void *interp, int argc, const char *argv[]);
    int (*dm_close)(struct dm *dmp);
    int (*dm_viable)(const char *dpy_string);
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
    int (*dm_drawVListHiddenLine)(struct dm *dmp, struct bn_vlist *vp);
    int (*dm_draw_data_axes)(struct dm *dmp, fastf_t sf, struct bview_data_axes_state *bndasp);
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
    int (*dm_getDisplayImage)(struct dm *dmp, unsigned char **image, int flip);  /**< @brief (0,0) is upper left pixel */
    int (*dm_reshape)(struct dm *dmp, int width, int height);
    int (*dm_makeCurrent)(struct dm *dmp);
    int (*dm_SwapBuffers)(struct dm *dmp);
    int (*dm_doevent)(struct dm *dmp, void *clientData, void *eventPtr);
    int (*dm_openFb)(struct dm *dmp);
    int (*dm_get_internal)(struct dm *dmp);
    int (*dm_put_internal)(struct dm *dmp);
    int (*dm_geometry_request)(struct dm *dmp, int width, int height);
    int (*dm_internal_var)(struct bu_vls *result, struct dm *dmp, const char *key);
    int (*dm_write_image)(struct bu_vls *msgs, FILE *fp, struct dm *dmp);
    void (*dm_flush)(struct dm *dmp);
    void (*dm_sync)(struct dm *dmp);
    int (*dm_event_cmp)(struct dm *dmp, dm_event_t type, int event);
    void (*dm_fogHint)(struct dm *dmp, int fastfog);
    int (*dm_share_dlist)(struct dm *dmp1, struct dm *dmp2);
    unsigned long dm_id;          /**< @brief window id */
    int dm_graphical;		/**< @brief !0 means device supports interactive graphics */
    const char *graphics_system; /**< @brief String identifying the drawing layer assumed */
    int dm_displaylist;		/**< @brief !0 means device has displaylist */
    int dm_stereo;                /**< @brief stereo flag */
    double dm_bound;		/**< @brief zoom-in limit */
    int dm_boundFlag;
    const char *dm_name;		/**< @brief short name of device */
    const char *dm_lname;		/**< @brief long name of device */
    int dm_top;                   /**< @brief !0 means toplevel window */
    int dm_width;
    int dm_height;
    int dm_dirty;
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
    struct bu_vls dm_log;   /**< @brief !NULL && !empty means log debug output to the file */
    unsigned char dm_bg[3];	/**< @brief background color */
    unsigned char dm_fg[3];	/**< @brief foreground color */
    vect_t dm_clipmin;		/**< @brief minimum clipping vector */
    vect_t dm_clipmax;		/**< @brief maximum clipping vector */
    int dm_debugLevel;		/**< @brief !0 means debugging */
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

struct fb_impl {
    uint32_t if_magic;
    uint32_t type_magic;
    /* Static information: per device TYPE.     */
    int (*if_open)(struct fb *ifp, const char *file, int _width, int _height);                       /**< @brief open device */
    int (*if_open_existing)(struct fb *ifp, int width, int height, struct fb_platform_specific *fb_p);                       /**< @brief open device */
    int (*if_close_existing)(struct fb *ifp);                       /**< @brief close embedded struct fb */
    struct fb_platform_specific *(*if_existing_get)(uint32_t magic);                       /**< @brief allocate memory for platform specific container*/
    void                         (*if_existing_put)(struct fb_platform_specific *fb_p);                       /**< @brief free memory for platform specific container */
    int (*if_close)(struct fb *ifp);                                                                 /**< @brief close device */
    int (*if_clear)(struct fb *ifp, unsigned char *pp);                                              /**< @brief clear device */
    ssize_t (*if_read)(struct fb *ifp, int x, int y, unsigned char *pp, size_t count);               /**< @brief read pixels */
    ssize_t (*if_write)(struct fb *ifp, int x, int y, const unsigned char *pp, size_t count);        /**< @brief write pixels */
    int (*if_rmap)(struct fb *ifp, ColorMap *cmap);                                                  /**< @brief read colormap */
    int (*if_wmap)(struct fb *ifp, const ColorMap *cmap);                                            /**< @brief write colormap */
    int (*if_view)(struct fb *ifp, int xcent, int ycent, int xzoom, int yzoom);                      /**< @brief set view */
    int (*if_getview)(struct fb *ifp, int *xcent, int *ycent, int *xzoom, int *yzoom);               /**< @brief get view */
    int (*if_setcursor)(struct fb *ifp, const unsigned char *bits, int xb, int yb, int xo, int yo);  /**< @brief define cursor */
    int (*if_cursor)(struct fb *ifp, int mode, int x, int y);                                        /**< @brief set cursor */
    int (*if_getcursor)(struct fb *ifp, int *mode, int *x, int *y);                                  /**< @brief get cursor */
    int (*if_readrect)(struct fb *ifp, int xmin, int ymin, int _width, int _height, unsigned char *pp);              /**< @brief read rectangle */
    int (*if_writerect)(struct fb *ifp, int xmin, int ymin, int _width, int _height, const unsigned char *pp);       /**< @brief write rectangle */
    int (*if_bwreadrect)(struct fb *ifp, int xmin, int ymin, int _width, int _height, unsigned char *pp);            /**< @brief read monochrome rectangle */
    int (*if_bwwriterect)(struct fb *ifp, int xmin, int ymin, int _width, int _height, const unsigned char *pp);     /**< @brief write rectangle */
    int (*if_configure_window)(struct fb *ifp, int width, int height);         /**< @brief configure window */
    int (*if_refresh)(struct fb *ifp, int x, int y, int w, int h);         /**< @brief refresh window */
    int (*if_poll)(struct fb *ifp);          /**< @brief handle events */
    int (*if_flush)(struct fb *ifp);         /**< @brief flush output */
    int (*if_free)(struct fb *ifp);          /**< @brief free resources */
    int (*if_help)(struct fb *ifp);          /**< @brief print useful info */
    char *if_type;      /**< @brief what "open" calls it */
    int if_max_width;   /**< @brief max device width */
    int if_max_height;  /**< @brief max device height */
    /* Dynamic information: per device INSTANCE. */
    char *if_name;      /**< @brief what the user called it */
    int if_width;       /**< @brief current values */
    int if_height;
    int if_selfd;       /**< @brief select(fd) for input events if >= 0 */
    /* Internal information: needed by the library.     */
    int if_fd;          /**< @brief internal file descriptor */
    int if_xzoom;       /**< @brief zoom factors */
    int if_yzoom;
    int if_xcenter;     /**< @brief pan position */
    int if_ycenter;
    int if_cursmode;    /**< @brief cursor on/off */
    int if_xcurs;       /**< @brief cursor position */
    int if_ycurs;
    unsigned char *if_pbase;/**< @brief Address of malloc()ed page buffer.      */
    unsigned char *if_pcurp;/**< @brief Current pointer into page buffer.       */
    unsigned char *if_pendp;/**< @brief End of page buffer.                     */
    int if_pno;         /**< @brief Current "page" in memory.           */
    int if_pdirty;      /**< @brief Page modified flag.                 */
    long if_pixcur;     /**< @brief Current pixel number in framebuffer. */
    long if_ppixels;    /**< @brief Sizeof page buffer (pixels).                */
    int if_debug;       /**< @brief Buffered IO debug flag.             */
    long if_poll_refresh_rate; /**< @brief Recommended polling rate for interactive framebuffers in microseconds. */
    /* State variables for individual interface modules */
    union {
        char *p;
        size_t l;
    } u1, u2, u3, u4, u5, u6;
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
