/*                   F B _ P R I V A T E . H
 * BRL-CAD
 *
 * Copyright (c) 2008-2014 United States Government as represented by
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
/** @file fb_private.h
 *
 * Private header for libfb.
 *
 */

#ifndef LIBFB_FB_PRIVATE_H
#define LIBFB_FB_PRIVATE_H

#include "common.h"

#include <limits.h> /* For INT_MAX */
#include <stdlib.h>

#include "fb.h"

/* declare all the possible interfaces */
#ifdef IF_X
FB_EXPORT extern fb X24_interface;
#endif
#ifdef IF_OGL
FB_EXPORT extern fb ogl_interface;
#endif
#ifdef IF_OSGL
FB_EXPORT extern fb osgl_interface;
#endif
#ifdef IF_WGL
FB_EXPORT extern fb wgl_interface;
#endif
#ifdef IF_TK
FB_EXPORT extern fb tk_interface;
#endif
#ifdef IF_QT
FB_EXPORT extern fb qt_interface;
#endif
#ifdef IF_REMOTE
FB_EXPORT extern fb remote_interface; /* not in list[] */
#endif

/* Always included */
FB_EXPORT extern fb debug_interface, disk_interface, stk_interface;
FB_EXPORT extern fb memory_interface, null_interface;


/* Shared memory (shmget et. al.) key common to multiple framebuffers */
#define SHMEM_KEY 42


__BEGIN_DECLS

/**
 *@brief
 * A frame-buffer IO structure.
 *
 * One of these is allocated for each active framebuffer.  A pointer
 * to this structure is the first argument to all the library
 * routines. The details of the structure are hidden behind function
 * calls in the fb.h API - no code external to libfb should work
 * directly with structure members.
 */
struct fb_internal {
    uint32_t if_magic;
    uint32_t type_magic;
    /* Static information: per device TYPE.     */
    int (*if_open)(struct fb_internal *ifp, const char *file, size_t _width, size_t _height);                       /**< @brief open device */
    int (*if_open_existing)(struct fb_internal *ifp, size_t width, size_t height, struct fb_platform_specific *fb_p);                       /**< @brief open device */
    int (*if_close_existing)(struct fb_internal *ifp);                       /**< @brief close embedded fb */
    struct fb_platform_specific *(*if_existing_get)(uint32_t magic);                       /**< @brief allocate memory for platform specific container*/
    void                         (*if_existing_put)(struct fb_platform_specific *fb_p);                       /**< @brief free memory for platform specific container */
    int (*if_close)(struct fb_internal *ifp);                                                                 /**< @brief close device */
    int (*if_clear)(struct fb_internal *ifp, unsigned char *pp);                                              /**< @brief clear device */
    ssize_t (*if_read)(struct fb_internal *ifp, size_t x, size_t y, unsigned char *pp, size_t count);               /**< @brief read pixels */
    ssize_t (*if_write)(struct fb_internal *ifp, size_t x, size_t y, const unsigned char *pp, size_t count);        /**< @brief write pixels */
    int (*if_rmap)(struct fb_internal *ifp, ColorMap *cmap);                                                  /**< @brief read colormap */
    int (*if_wmap)(struct fb_internal *ifp, const ColorMap *cmap);                                            /**< @brief write colormap */
    int (*if_view)(struct fb_internal *ifp, size_t xcent, size_t ycent, size_t xzoom, size_t yzoom);                      /**< @brief set view */
    int (*if_getview)(struct fb_internal *ifp, size_t *xcent, size_t *ycent, size_t *xzoom, size_t *yzoom);               /**< @brief get view */
    int (*if_setcursor)(struct fb_internal *ifp, const unsigned char *bits, size_t xb, size_t yb, size_t xo, size_t yo);  /**< @brief define cursor */
    int (*if_cursor)(struct fb_internal *ifp, int mode, size_t x, size_t y);                                        /**< @brief set cursor */
    int (*if_getcursor)(struct fb_internal *ifp, int *mode, size_t *x, size_t *y);                                  /**< @brief get cursor */
    int (*if_readrect)(struct fb_internal *ifp, size_t xmin, size_t ymin, size_t _width, size_t _height, unsigned char *pp);              /**< @brief read rectangle */
    int (*if_writerect)(struct fb_internal *ifp, size_t xmin, size_t ymin, size_t _width, size_t _height, const unsigned char *pp);       /**< @brief write rectangle */
    int (*if_bwreadrect)(struct fb_internal *ifp, size_t xmin, size_t ymin, size_t _width, size_t _height, unsigned char *pp);            /**< @brief read monochrome rectangle */
    int (*if_bwwriterect)(struct fb_internal *ifp, size_t xmin, size_t ymin, size_t _width, size_t _height, const unsigned char *pp);     /**< @brief write rectangle */
    int (*if_configure_window)(struct fb_internal *ifp, size_t width, size_t height);         /**< @brief configure window */
    int (*if_refresh)(struct fb_internal *ifp, size_t x, size_t y, size_t w, size_t h);         /**< @brief refresh window */
    int (*if_poll)(struct fb_internal *ifp);          /**< @brief handle events */
    int (*if_flush)(struct fb_internal *ifp);         /**< @brief flush output */
    int (*if_free)(struct fb_internal *ifp);          /**< @brief free resources */
    int (*if_help)(struct fb_internal *ifp);          /**< @brief print useful info */
    char *if_type;      /**< @brief what "open" calls it */
    size_t if_max_width;   /**< @brief max device width */
    size_t if_max_height;  /**< @brief max device height */
    /* Dynamic information: per device INSTANCE. */
    char *if_name;      /**< @brief what the user called it */
    size_t if_width;       /**< @brief current values */
    size_t if_height;
    int if_selfd;       /**< @brief select(fd) for input events if >= 0 */
    /* Internal information: needed by the library.     */
    int if_fd;          /**< @brief internal file descriptor */
    size_t if_xzoom;       /**< @brief zoom factors */
    size_t if_yzoom;
    size_t if_xcenter;     /**< @brief pan position */
    size_t if_ycenter;
    int if_cursmode;    /**< @brief cursor on/off */
    size_t if_xcurs;       /**< @brief cursor position */
    size_t if_ycurs;
    unsigned char *if_pbase;/**< @brief Address of malloc()ed page buffer.      */
    unsigned char *if_pcurp;/**< @brief Current pointer into page buffer.       */
    unsigned char *if_pendp;/**< @brief End of page buffer.                     */
    int if_pno;         /**< @brief Current "page" in memory.           */
    int if_pdirty;      /**< @brief Page modified flag.                 */
    size_t if_pixcur;     /**< @brief Current pixel number in framebuffer. */
    size_t if_ppixels;    /**< @brief Sizeof page buffer (pixels).                */
    int if_debug;       /**< @brief Buffered IO debug flag.             */
    size_t if_poll_refresh_rate; /**< @brief Recommended polling rate for interactive framebuffers in microseconds. */
    /* State variables for individual interface modules */
    union {
        char *p;
        size_t l;
    } u1, u2, u3, u4, u5, u6;
};

/*
 * Some functions and variables we couldn't hide.
 * Not for general consumption.
 */
FB_EXPORT extern int _fb_disk_enable;
FB_EXPORT extern int fb_sim_readrect(fb *ifp, size_t xmin, size_t ymin, size_t _width, size_t _height, unsigned char *pp);
FB_EXPORT extern int fb_sim_writerect(fb *ifp, size_t xmin, size_t ymin, size_t _width, size_t _height, const unsigned char *pp);
FB_EXPORT extern int fb_sim_bwreadrect(fb *ifp, size_t xmin, size_t ymin, size_t _width, size_t _height, unsigned char *pp);
FB_EXPORT extern int fb_sim_bwwriterect(fb *ifp, size_t xmin, size_t ymin, size_t _width, size_t _height, const unsigned char *pp);
FB_EXPORT extern int fb_sim_view(fb *ifp, size_t xcenter, size_t ycenter, size_t xzoom, size_t yzoom);
FB_EXPORT extern int fb_sim_getview(fb *ifp, size_t *xcenter, size_t *ycenter, size_t *xzoom, size_t *yzoom);
FB_EXPORT extern int fb_sim_cursor(fb *ifp, int mode, size_t x, size_t y);
FB_EXPORT extern int fb_sim_getcursor(fb *ifp, int *mode, size_t *x, size_t *y);

__END_DECLS

#endif /* LIBFB_FB_PRIVATE_H */

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
