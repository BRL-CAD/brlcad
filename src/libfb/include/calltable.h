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
 * Internal call table structure for the frame manager library.
 *
 * This API is exposed only to support supplying framebuffer backends as
 * plugins - calling codes should *NOT* depend on this API to remain stable.
 * From their perspective it is considered a private implementation detail.
 *
 * Instead, they should use the fb_*() functions in fb.h
 *
 */

#include "common.h"

#ifndef FB_CALLTABLE_H
#define FB_CALLTABLE_H

#include "fb.h"

__BEGIN_DECLS

/**
 * Interface to a specific Display Manager
 *
 * TODO - see how much of this can be set up as private, backend specific
 * variables behind the vparse.  The txt backend, for example, doesn't need
 * Tk information...
 */

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

#endif /* FB_CALLTABLE_H */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
