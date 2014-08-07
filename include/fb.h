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

#include "bu/bu_tcl.h"
#include "bu/magic.h"
#include "bu/vls.h"

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
/* FIXME: ColorMap is same as RLEColorMap defined in 'orle.h' */
typedef struct {
    unsigned short cm_red[256];
    unsigned short cm_green[256];
    unsigned short cm_blue[256];
} ColorMap;


#define PIXEL_NULL (unsigned char *) 0
#define RGBPIXEL_NULL (unsigned char *) 0
#define COLORMAP_NULL (ColorMap *) 0

/* Use a typedef to hide the details of the framebuffer structure */
typedef struct fb fb_s;
#define FB_NULL (fb_s *) 0

/**
 * assert the integrity of an FBIO struct.
 */
#define FB_CK_FB(_p) BU_CKMAG(_p, FB_MAGIC, "FB")

/* declare all the possible interfaces */
#ifdef IF_REMOTE
FB_EXPORT extern fb_s remote_interface;	/* not in list[] */
#endif

#ifdef IF_OGL
FB_EXPORT extern fb_s ogl_interface;
#endif

#ifdef IF_WGL
FB_EXPORT extern fb_s wgl_interface;
#endif

#ifdef IF_X
FB_EXPORT extern fb_s X24_interface;
FB_EXPORT extern fb_s X_interface;
#endif

#ifdef IF_TK
FB_EXPORT extern fb_s tk_interface;
#endif

#ifdef IF_QT
FB_EXPORT extern fb_s qt_interface;
#endif

/* Always included */
FB_EXPORT extern fb_s debug_interface, disk_interface, stk_interface;
FB_EXPORT extern fb_s memory_interface, null_interface;


__BEGIN_DECLS

/* Library entry points */

FB_EXPORT fb_s *fb_get();
FB_EXPORT void  fb_put(fb_s *ifp);
FB_EXPORT extern char *fb_gettype(fb_s *ifp);
FB_EXPORT extern int fb_get_max_width(fb_s *ifp);
FB_EXPORT extern int fb_get_max_height(fb_s *ifp);
FB_EXPORT extern int fb_getwidth(fb_s *ifp);
FB_EXPORT extern int fb_getheight(fb_s *ifp);
FB_EXPORT extern int fb_poll(fb_s *ifp);
FB_EXPORT extern int fb_help(fb_s *ifp);
FB_EXPORT extern int fb_free(fb_s *ifp);
FB_EXPORT extern int fb_clear(fb_s *ifp, unsigned char *pp);
FB_EXPORT extern ssize_t fb_read(fb_s *ifp, int x, int y, unsigned char *pp, size_t count);
FB_EXPORT extern ssize_t fb_write(fb_s *ifp, int x, int y, const unsigned char *pp, size_t count);
FB_EXPORT extern int fb_rmap(fb_s *ifp, ColorMap *cmap);
FB_EXPORT extern int fb_wmap(fb_s *ifp, const ColorMap *cmap);
FB_EXPORT extern int fb_view(fb_s *ifp, int xcenter, int ycenter, int xzoom, int yzoom);
FB_EXPORT extern int fb_getview(fb_s *ifp, int *xcenter, int *ycenter, int *xzoom, int *yzoom);
FB_EXPORT extern int fb_setcursor(fb_s *ifp, const unsigned char *bits, int xb, int yb, int xo, int yo);
FB_EXPORT extern int fb_cursor(fb_s *ifp, int mode, int x, int y);
FB_EXPORT extern int fb_getcursor(fb_s *ifp, int *mode, int *x, int *y);
FB_EXPORT extern int fb_readrect(fb_s *ifp, int xmin, int ymin, int width, int height, unsigned char *pp);
FB_EXPORT extern int fb_writerect(fb_s *ifp, int xmin, int ymin, int width, int height, const unsigned char *pp);
FB_EXPORT extern int fb_bwreadrect(fb_s *ifp, int xmin, int ymin, int width, int height, unsigned char *pp);
FB_EXPORT extern int fb_bwwriterect(fb_s *ifp, int xmin, int ymin, int width, int height, const unsigned char *pp);

FB_EXPORT extern void fb_configureWindow(fb_s *, int, int);
FB_EXPORT extern fb_s *fb_open(const char *file, int _width, int _height);
FB_EXPORT extern int fb_close(fb_s *ifp);
FB_EXPORT extern int fb_close_existing(fb_s *ifp);
FB_EXPORT extern int fb_genhelp(void);
FB_EXPORT extern int fb_ioinit(fb_s *ifp);
FB_EXPORT extern int fb_seek(fb_s *ifp, int x, int y);
FB_EXPORT extern int fb_tell(fb_s *ifp, int *xp, int *yp);
FB_EXPORT extern int fb_rpixel(fb_s *ifp, unsigned char *pp);
FB_EXPORT extern int fb_wpixel(fb_s *ifp, unsigned char *pp);
FB_EXPORT extern int fb_flush(fb_s *ifp);
#if !defined(_WIN32) || defined(__CYGWIN__)
FB_EXPORT extern void fb_log(const char *fmt, ...) _BU_ATTR_PRINTF12;
#endif
FB_EXPORT extern int fb_null(fb_s *ifp);
FB_EXPORT extern int fb_null_setcursor(fb_s *ifp, const unsigned char *bits, int xbits, int ybits, int xorig, int yorig);

/* utility functions */
FB_EXPORT extern int fb_common_file_size(size_t *widthp, size_t *heightp, const char *filename, int pixel_size);
FB_EXPORT extern int fb_common_image_size(size_t *widthp, size_t *heightp, size_t npixels);
FB_EXPORT extern int fb_common_name_size(size_t *widthp, size_t *heightp, const char *name);
FB_EXPORT extern int fb_write_fp(fb_s *ifp, FILE *fp, int req_width, int req_height, int crunch, int inverse, struct bu_vls *result);
FB_EXPORT extern int fb_read_fd(fb_s *ifp, int fd,  int file_width, int file_height, int file_xoff, int file_yoff, int scr_width, int scr_height, int scr_xoff, int scr_yoff, int fileinput, char *file_name, int one_line_only, int multiple_lines, int autosize, int inverse, int clear, int zoom, struct bu_vls *result);

FB_EXPORT extern void fb_set_interface(fb_s *ifp, fb_s *interface);
FB_EXPORT extern void fb_set_name(fb_s *ifp, const char *name);
FB_EXPORT extern char *fb_get_name(fb_s *ifp);
FB_EXPORT extern void fb_set_magic(fb_s *ifp, uint32_t magic);
FB_EXPORT extern long fb_get_pagebuffer_pixel_size(fb_s *ifp);

FB_EXPORT extern int fb_is_set_fd(fb_s *ifp, fd_set *infds);
FB_EXPORT extern int fb_set_fd(fb_s *ifp, fd_set *select_list);
FB_EXPORT extern int fb_clear_fd(fb_s *ifp, fd_set *select_list);

/* color mapping */
FB_EXPORT extern int fb_is_linear_cmap(const ColorMap *cmap);
FB_EXPORT extern void fb_make_linear_cmap(ColorMap *cmap);

/* backward compatibility hacks */
FB_EXPORT extern int fb_reset(fb_s *ifp);
FB_EXPORT extern int fb_viewport(fb_s *ifp, int left, int top, int right, int bottom);
FB_EXPORT extern int fb_window(fb_s *ifp, int xcenter, int ycenter);
FB_EXPORT extern int fb_zoom(fb_s *ifp, int xzoom, int yzoom);
FB_EXPORT extern int fb_scursor(fb_s *ifp, int mode, int x, int y);

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

/* FIXME:  these IF_* sections need to die.  they don't belong in a public header. */

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
FB_EXPORT extern int fb_refresh(fb_s *ifp, int x, int y, int w, int h);


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
