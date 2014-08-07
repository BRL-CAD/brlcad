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
FB_EXPORT extern fb_s X_interface;
FB_EXPORT extern fb_s X24_interface;
#endif
#ifdef IF_OGL
FB_EXPORT extern fb_s ogl_interface;
#endif
#ifdef IF_WGL
FB_EXPORT extern fb_s wgl_interface;
#endif
#ifdef IF_TK
FB_EXPORT extern fb_s tk_interface;
#endif
#ifdef IF_QT
FB_EXPORT extern fb_s qt_interface;
#endif
#ifdef IF_REMOTE
FB_EXPORT extern fb_s remote_interface; /* not in list[] */
#endif

/* Always included */
FB_EXPORT extern fb_s debug_interface, disk_interface, stk_interface;
FB_EXPORT extern fb_s memory_interface, null_interface;

__BEGIN_DECLS

/**
 *@brief
 * A frame-buffer IO structure.
 *
 * One of these is allocated for each active framebuffer.  A pointer
 * to this structure is the first argument to all the library
 * routines.  TODO - see if this can move to a private header.
 */
struct fb {
    uint32_t if_magic;
    /* Static information: per device TYPE.     */
    int (*if_open)(struct fb *ifp, const char *file, int _width, int _height);                       /**< @brief open device */
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
    /* State variables for individual interface modules */
    union {
        char *p;
        size_t l;
    } u1, u2, u3, u4, u5, u6;
};


#ifdef IF_X
#  ifdef HAVE_X11_XLIB_H
#    include <X11/Xlib.h>
#    include <X11/Xutil.h>
#  endif
FB_EXPORT extern int _X24_open_existing(fb_s *ifp, Display *dpy, Window win, Window cwinp, Colormap cmap, XVisualInfo *vip, int width, int height, GC gc);
#endif

#ifdef IF_OGL
#  ifdef HAVE_X11_XLIB_H
#    include <X11/Xlib.h>
#    include <X11/Xutil.h>
#  endif
/* glx.h on Mac OS X (and perhaps elsewhere) defines a slew of
 *  * parameter names that shadow system symbols.  protect the system
 *   * symbols by redefining the parameters prior to header inclusion.
 *    */
#  define j1 J1
#  define y1 Y1
#  define read rd
#  define index idx
#  define access acs
#  define remainder rem
#  ifdef HAVE_GL_GLX_H
#    include <GL/glx.h>
#  endif
#  undef remainder
#  undef access
#  undef index
#  undef read
#  undef y1
#  undef j1
#  ifdef HAVE_GL_GL_H
#    include <GL/gl.h>
#  endif
FB_EXPORT extern int _ogl_open_existing(fb_s *ifp, Display *dpy, Window win, Colormap cmap, XVisualInfo *vip, int width, int height, GLXContext glxc, int double_buffer, int soft_cmap);
#endif

#ifdef IF_WGL
#  include <windows.h>
#  include <tk.h>
#  ifdef HAVE_GL_GL_H
#    include <GL/gl.h>
#  endif
FB_EXPORT extern int _wgl_open_existing(fb_s *ifp, Display *dpy, Window win, Colormap cmap, PIXELFORMATDESCRIPTOR *vip, HDC hdc, int width, int height, HGLRC glxc, int double_buffer, int soft_cmap);
#endif

#ifdef IF_QT
FB_EXPORT extern int _qt_open_existing(fb_s *ifp, int width, int height, void *qapp, void *qwin, void *qpainter, void *draw, void **qimg);
#endif

/*
 * Some functions and variables we couldn't hide.
 * Not for general consumption.
 */
FB_EXPORT extern int _fb_disk_enable;
FB_EXPORT extern int fb_sim_readrect(fb_s *ifp, int xmin, int ymin, int _width, int _height, unsigned char *pp);
FB_EXPORT extern int fb_sim_writerect(fb_s *ifp, int xmin, int ymin, int _width, int _height, const unsigned char *pp);
FB_EXPORT extern int fb_sim_bwreadrect(fb_s *ifp, int xmin, int ymin, int _width, int _height, unsigned char *pp);
FB_EXPORT extern int fb_sim_bwwriterect(fb_s *ifp, int xmin, int ymin, int _width, int _height, const unsigned char *pp);
FB_EXPORT extern int fb_sim_view(fb_s *ifp, int xcenter, int ycenter, int xzoom, int yzoom);
FB_EXPORT extern int fb_sim_getview(fb_s *ifp, int *xcenter, int *ycenter, int *xzoom, int *yzoom);
FB_EXPORT extern int fb_sim_cursor(fb_s *ifp, int mode, int x, int y);
FB_EXPORT extern int fb_sim_getcursor(fb_s *ifp, int *mode, int *x, int *y);

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
