/*                            F B . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2014 United States Government as represented by
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
/** @addtogroup libfb */
/** @{ */
/** @file fb.h
 *
 * "Generic" Framebuffer Library Interface Defines.
 *
 */

#ifndef FB_H
#define FB_H

#include "common.h"

#include <limits.h>  /* For INT_MAX */
#include <stdlib.h>

/*
 * Needed for fd_set, avoid including sys/select.h outright since it
 * conflicts on some systems (e.g. freebsd4).
 *
 * FIXME: would be nice to decouple this interface from fd_set as it's
 * only used in one place right now.
 */
#if defined(HAVE_SYS_TYPES_H)
#  include <sys/types.h>
#endif
#if defined(HAVE_SYS_TIME_H)
#  include <sys/time.h>
#endif

#ifndef FB_EXPORT
#  if defined(FB_DLL_EXPORTS) && defined(FB_DLL_IMPORTS)
#    error "Only FB_DLL_EXPORTS or FB_DLL_IMPORTS can be defined, not both."
#  elif defined(FB_DLL_EXPORTS)
#    define FB_EXPORT __declspec(dllexport)
#  elif defined(FB_DLL_IMPORTS)
#    define FB_EXPORT __declspec(dllimport)
#  else
#    define FB_EXPORT
#  endif
#endif

/**
 * Format of disk pixels is .pix raw image files.  Formerly used as
 * arguments to many of the library routines, but has fallen into
 * disuse due to the difficulties with ANSI function prototypes, and
 * the fact that arrays are not real types in C.  The most notable
 * problem is that of passing a pointer to an array of RGBpixel.  It
 * looks doubly dimensioned, but isn't.
 */
typedef unsigned char RGBpixel[3];


/**
 * These generic color maps have up to 16 bits of significance,
 * left-justified in a short.  Think of this as fixed-point values
 * from 0 to 1.
 */
typedef struct {
    unsigned short cm_red[256];
    unsigned short cm_green[256];
    unsigned short cm_blue[256];
} ColorMap;

#define PIXEL_NULL (unsigned char *) 0
#define RGBPIXEL_NULL (unsigned char *) 0
#define COLORMAP_NULL (ColorMap *) 0
#define FB_NULL (struct fb *) 0

struct fbserv_listener {
    int fbsl_fd;                        /**< @brief socket to listen for connections */
#if defined(_WIN32) && !defined(__CYGWIN__)
    Tcl_Channel fbsl_chan;
#endif
    int fbsl_port;                      /**< @brief port number to listen on */
    int fbsl_listen;                    /**< @brief !0 means listen for connections */
    struct fbserv_obj *fbsl_fbsp;       /**< @brief points to its fbserv object */
};


struct fbserv_client {
    int fbsc_fd;
#if defined(_WIN32) && !defined(__CYGWIN__)
    Tcl_Channel fbsc_chan;
    Tcl_FileProc *fbsc_handler;
#endif
    struct pkg_conn *fbsc_pkg;
    struct fbserv_obj *fbsc_fbsp;       /**< @brief points to its fbserv object */
};


/**
 *@brief
 * A frame-buffer container.
 *
 * One of these is allocated for each active framebuffer.  A pointer
 * to this structure is the first argument to all the library
 * routines.
 */
struct fb {
    uint32_t fb_magic;
    int fb_type;
    char *handle;      /**< @brief what the user called it */
    void *fb_canvas;
    int is_embedded;
    int width;       /**< @brief current values */
    int height;
    int dirty_flag;      /**< @brief Page modified flag. */
    struct bu_attribute_value_set *extra_settings;  /**< @brief All settings (generic and DMTYPE specific) listed here. */
    /* Support for import/export of data via network */
    struct fbserv_listener fbs_listener;                /**< @brief data for listening */
    struct fbserv_client fbs_clients[MAX_CLIENTS];      /**< @brief connected clients */
    /* With or without active network listeners/clients, have a callback function for possible parents */
    void (*fb_callback)(void *clientData);             /**< @brief callback function */
    void *fb_clientData;
};

/**
 * assert the integrity of an FBIO struct.
 */
#define FB_CK_FBIO(_p) BU_CKMAG(_p, FB_MAGIC, "FBIO")

/* Frame buffer structure will (hopefully) be internal to libfb, so use a typedef */
typedef struct fb fb_s;

    /* Static information: per device TYPE.     */
    int (*if_open)(struct FBIO_ *ifp, const char *file, int _width, int _height);                       /**< @brief open device */
    int (*if_close)(struct FBIO_ *ifp);                                                                 /**< @brief close device */
    int (*if_clear)(struct FBIO_ *ifp, unsigned char *pp);                                              /**< @brief clear device */
    ssize_t (*if_read)(struct FBIO_ *ifp, int x, int y, unsigned char *pp, size_t count);               /**< @brief read pixels */
    ssize_t (*if_write)(struct FBIO_ *ifp, int x, int y, const unsigned char *pp, size_t count);        /**< @brief write pixels */
    int (*if_rmap)(struct FBIO_ *ifp, ColorMap *cmap);                                                  /**< @brief read colormap */
    int (*if_wmap)(struct FBIO_ *ifp, const ColorMap *cmap);                                            /**< @brief write colormap */
    int (*if_view)(struct FBIO_ *ifp, int xcent, int ycent, int xzoom, int yzoom);                      /**< @brief set view */
    int (*if_getview)(struct FBIO_ *ifp, int *xcent, int *ycent, int *xzoom, int *yzoom);               /**< @brief get view */
    int (*if_setcursor)(struct FBIO_ *ifp, const unsigned char *bits, int xb, int yb, int xo, int yo);  /**< @brief define cursor */
    int (*if_cursor)(struct FBIO_ *ifp, int mode, int x, int y);                                        /**< @brief set cursor */
    int (*if_getcursor)(struct FBIO_ *ifp, int *mode, int *x, int *y);                                  /**< @brief get cursor */
    int (*if_readrect)(struct FBIO_ *ifp, int xmin, int ymin, int _width, int _height, unsigned char *pp);              /**< @brief read rectangle */
    int (*if_writerect)(struct FBIO_ *ifp, int xmin, int ymin, int _width, int _height, const unsigned char *pp);       /**< @brief write rectangle */
    int (*if_bwreadrect)(struct FBIO_ *ifp, int xmin, int ymin, int _width, int _height, unsigned char *pp);            /**< @brief read monochrome rectangle */
    int (*if_bwwriterect)(struct FBIO_ *ifp, int xmin, int ymin, int _width, int _height, const unsigned char *pp);     /**< @brief write rectangle */
    int (*if_poll)(struct FBIO_ *ifp);          /**< @brief handle events */
    int (*if_flush)(struct FBIO_ *ifp);         /**< @brief flush output */
    int (*if_free)(struct FBIO_ *ifp);          /**< @brief free resources */
    int (*if_help)(struct FBIO_ *ifp);          /**< @brief print useful info */
    const char *if_type;        /**< @brief what "open" calls it */
    int if_max_width;   /**< @brief max device width */
    int if_max_height;  /**< @brief max device height */


FB_EXPORT int fb_get_type(fb_s *fbp);
FB_EXPORT int fb_set_type(fb_s *fbp, int fb_t);

FB_EXPORT int fb_get_width(fb_s *fbp);
FB_EXPORT int fb_set_width(fb_s *fbp, int width);

FB_EXPORT int fb_get_height(fb_s *fbp);
FB_EXPORT int fb_set_height(fb_s *fbp, int height);

FB_EXPORT COLORMAP_TYPE *fb_get_colormap(fb_s *fbp);
FB_EXPORT void           fb_set_colormap(fb_s *fbp, COLORMAP_TYPE *map);

FB_EXPORT ssize_t fb_get_pixels(fb_s *fbp, int x, int y, unsigned char *pp, size_t count);               /**< @brief read pixels into pp */
FB_EXPORT ssize_t fb_set_pixels(fb_s *fbp, int x, int y, const unsigned char *pp, size_t count);   /**< @brief write pixels from pp*/
FB_EXPORT ssize_t fb_get_rect(fb_s *fbp, int xmin, int ymin, int _width, int _height, unsigned char *pp); /**< @brief read rectangle into pp */
FB_EXPORT ssize_t fb_set_rect(fb_s *fbp, int xmin, int ymin, int _width, int _height, unsigned char *pp);   /**< @brief write rectangle from pp*/


FB_EXPORT extern int   fb_init(fb_s *fbp, int fb_t, );  /* TODO - probably need an actual public struct to hold parent info */
FB_EXPORT extern int   fb_close(fb_s *fbp);
FB_EXPORT extern int   fb_refresh(fb_s *fbp);
FB_EXPORT extern void *fb_canvas(fb_s *fbp);  /* Exposes the low level drawing object (X window, OpenGL context, etc.) for custom drawing */
FB_EXPORT extern int   fb_get_type(fb_s *fbp);
FB_EXPORT extern int   fb_set_type(fb_s *fbp, int fb_t);
FB_EXPORT extern int   fb_get_image(fb_s *fbp, icv_image_t *image);
FB_EXPORT extern int   fb_get_obj_image(fb_s *fbp, const char *handle, icv_image_t *image);




/* Library entry points which are macros.
 *
 * FIXME: turn these into proper functions so we can appropriately
 * avoid dereferencing a NULL _ifp or calling an invalid callback.
 */
#define fb_gettype(_ifp)		(_ifp->if_type)
#define fb_getwidth(_ifp)		(_ifp->if_width)
#define fb_getheight(_ifp)		(_ifp->if_height)
#define fb_poll(_ifp)			(*_ifp->if_poll)(_ifp)
#define fb_help(_ifp)			(*_ifp->if_help)(_ifp)
#define fb_free(_ifp)			(*_ifp->if_free)(_ifp)
#define fb_clear(_ifp, _pp)		(*_ifp->if_clear)(_ifp, _pp)
#define fb_read(_ifp, _x, _y, _pp, _ct)		(*_ifp->if_read)(_ifp, _x, _y, _pp, _ct)
#define fb_write(_ifp, _x, _y, _pp, _ct)	(*_ifp->if_write)(_ifp, _x, _y, _pp, _ct)
#define fb_rmap(_ifp, _cmap)			(*_ifp->if_rmap)(_ifp, _cmap)
#define fb_wmap(_ifp, _cmap)			(*_ifp->if_wmap)(_ifp, _cmap)
#define fb_view(_ifp, _xc, _yc, _xz, _yz)	(*_ifp->if_view)(_ifp, _xc, _yc, _xz, _yz)
#define fb_getview(_ifp, _xcp, _ycp, _xzp, _yzp)	(*_ifp->if_getview)(_ifp, _xcp, _ycp, _xzp, _yzp)
#define fb_setcursor(_ifp, _bits, _xb, _yb, _xo, _yo) 	(*_ifp->if_setcursor)(_ifp, _bits, _xb, _yb, _xo, _yo)
#define fb_cursor(_ifp, _mode, _x, _y)			(*_ifp->if_cursor)(_ifp, _mode, _x, _y)
#define fb_getcursor(_ifp, _modep, _xp, _yp)		(*_ifp->if_getcursor)(_ifp, _modep, _xp, _yp)
#define fb_readrect(_ifp, _xmin, _ymin, _width, _height, _pp)		(*_ifp->if_readrect)(_ifp, _xmin, _ymin, _width, _height, _pp)
#define fb_writerect(_ifp, _xmin, _ymin, _width, _height, _pp)		(*_ifp->if_writerect)(_ifp, _xmin, _ymin, _width, _height, _pp)
#define fb_bwreadrect(_ifp, _xmin, _ymin, _width, _height, _pp) 	(*_ifp->if_bwreadrect)(_ifp, _xmin, _ymin, _width, _height, _pp)
#define fb_bwwriterect(_ifp, _xmin, _ymin, _width, _height, _pp)	(*_ifp->if_bwwriterect)(_ifp, _xmin, _ymin, _width, _height, _pp)

__BEGIN_DECLS

/* Library entry points which are true functions. */
FB_EXPORT extern void fb_configureWindow(FBIO *, int, int);
FB_EXPORT extern FBIO *fb_open(const char *file, int _width, int _height);
FB_EXPORT extern int fb_close(FBIO *ifp);
FB_EXPORT extern int fb_close_existing(FBIO *ifp);
FB_EXPORT extern int fb_genhelp(void);
FB_EXPORT extern int fb_ioinit(FBIO *ifp);
FB_EXPORT extern int fb_seek(FBIO *ifp, int x, int y);
FB_EXPORT extern int fb_tell(FBIO *ifp, int *xp, int *yp);
FB_EXPORT extern int fb_rpixel(FBIO *ifp, unsigned char *pp);
FB_EXPORT extern int fb_wpixel(FBIO *ifp, unsigned char *pp);
FB_EXPORT extern int fb_flush(FBIO *ifp);
#if !defined(_WIN32) || defined(__CYGWIN__)
FB_EXPORT extern void fb_log(const char *fmt, ...) _BU_ATTR_PRINTF12;
#endif
FB_EXPORT extern int fb_null(FBIO *ifp);
FB_EXPORT extern int fb_null_setcursor(FBIO *ifp, const unsigned char *bits, int xbits, int ybits, int xorig, int yorig);

/* utility functions */
FB_EXPORT extern int fb_common_file_size(size_t *widthp, size_t *heightp, const char *filename, int pixel_size);
FB_EXPORT extern int fb_common_image_size(size_t *widthp, size_t *heightp, size_t npixels);
FB_EXPORT extern int fb_common_name_size(size_t *widthp, size_t *heightp, const char *name);
FB_EXPORT extern int fb_write_fp(FBIO *ifp, FILE *fp, int req_width, int req_height, int crunch, int inverse, struct bu_vls *result);
FB_EXPORT extern int fb_read_fd(FBIO *ifp, int fd,  int file_width, int file_height, int file_xoff, int file_yoff, int scr_width, int scr_height, int scr_xoff, int scr_yoff, int fileinput, char *file_name, int one_line_only, int multiple_lines, int autosize, int inverse, int clear, int zoom, struct bu_vls *result);

/* color mapping */
FB_EXPORT extern int fb_is_linear_cmap(const ColorMap *cmap);
FB_EXPORT extern void fb_make_linear_cmap(ColorMap *cmap);

/* backward compatibility hacks */
FB_EXPORT extern int fb_reset(FBIO *ifp);
FB_EXPORT extern int fb_viewport(FBIO *ifp, int left, int top, int right, int bottom);
FB_EXPORT extern int fb_window(FBIO *ifp, int xcenter, int ycenter);
FB_EXPORT extern int fb_zoom(FBIO *ifp, int xzoom, int yzoom);
FB_EXPORT extern int fb_scursor(FBIO *ifp, int mode, int x, int y);

/*
 * Some functions and variables we couldn't hide.
 * Not for general consumption.
 */
FB_EXPORT extern int _fb_disk_enable;
FB_EXPORT extern int fb_sim_readrect(FBIO *ifp, int xmin, int ymin, int _width, int _height, unsigned char *pp);
FB_EXPORT extern int fb_sim_writerect(FBIO *ifp, int xmin, int ymin, int _width, int _height, const unsigned char *pp);
FB_EXPORT extern int fb_sim_bwreadrect(FBIO *ifp, int xmin, int ymin, int _width, int _height, unsigned char *pp);
FB_EXPORT extern int fb_sim_bwwriterect(FBIO *ifp, int xmin, int ymin, int _width, int _height, const unsigned char *pp);
FB_EXPORT extern int fb_sim_view(FBIO *ifp, int xcenter, int ycenter, int xzoom, int yzoom);
FB_EXPORT extern int fb_sim_getview(FBIO *ifp, int *xcenter, int *ycenter, int *xzoom, int *yzoom);
FB_EXPORT extern int fb_sim_cursor(FBIO *ifp, int mode, int x, int y);
FB_EXPORT extern int fb_sim_getcursor(FBIO *ifp, int *mode, int *x, int *y);

/* FIXME:  these IF_* sections need to die.  they don't belong in a public header. */

#ifdef IF_X
#  ifdef HAVE_X11_XLIB_H
#    include <X11/Xlib.h>
#    include <X11/Xutil.h>
#  endif
FB_EXPORT extern int _X24_open_existing(FBIO *ifp, Display *dpy, Window win, Window cwinp, Colormap cmap, XVisualInfo *vip, int width, int height, GC gc);
#endif

#ifdef IF_OGL
#  ifdef HAVE_X11_XLIB_H
#    include <X11/Xlib.h>
#    include <X11/Xutil.h>
#  endif
/* glx.h on Mac OS X (and perhaps elsewhere) defines a slew of
 * parameter names that shadow system symbols.  protect the system
 * symbols by redefining the parameters prior to header inclusion.
 */
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
FB_EXPORT extern int _ogl_open_existing(FBIO *ifp, Display *dpy, Window win, Colormap cmap, XVisualInfo *vip, int width, int height, GLXContext glxc, int double_buffer, int soft_cmap);
#endif

/*TODO - openscenegraph stuff needed here? */
#ifdef IF_OSG
#  ifdef __cplusplus
extern "C++" {
#include <osg/GraphicsContext>
}
#include "tk.h"
FB_EXPORT extern int _osg_open_existing(FBIO *ifp, Display *dpy, Window win, Colormap cmap, int width, int height, osg::ref_ptr<osg::GraphicsContext> graphicsContext);
#  endif
#endif /*IF_OSG*/

#ifdef IF_WGL
#  include <windows.h>
#  include <tk.h>
#  ifdef HAVE_GL_GL_H
#    include <GL/gl.h>
#  endif
FB_EXPORT extern int _wgl_open_existing(FBIO *ifp, Display *dpy, Window win, Colormap cmap, PIXELFORMATDESCRIPTOR *vip, HDC hdc, int width, int height, HGLRC glxc, int double_buffer, int soft_cmap);
#endif

#ifdef IF_QT
FB_EXPORT extern int _qt_open_existing(FBIO *ifp, int width, int height, void *qapp, void *qwin, void *qpainter, void *draw, void **qimg);
#endif

/*
 * Copy one RGB pixel to another.
 */
#define COPYRGB(to, from) { (to)[RED]=(from)[RED];\
			   (to)[GRN]=(from)[GRN];\
			   (to)[BLU]=(from)[BLU]; }

/* Debug Bitvector Definition */
#define FB_DEBUG_BIO 1	/* Buffered io calls (less r/wpixel) */
#define FB_DEBUG_CMAP 2	/* Contents of colormaps */
#define FB_DEBUG_RW 4	/* Contents of reads and writes */
#define FB_DEBUG_BRW 8	/* Buffered IO rpixel and wpixel */

/* tcl.c */
/* The presence of Tcl_Interp as an arg prevents giving arg list */
FB_EXPORT extern void fb_tcl_setup(void);
FB_EXPORT extern int Fb_Init(Tcl_Interp *interp);
FB_EXPORT extern int fb_refresh(FBIO *ifp, int x, int y, int w, int h);


/**
 * report version information about LIBFB
 */
FB_EXPORT extern const char *fb_version(void);

__END_DECLS

#endif /* FB_H */

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
