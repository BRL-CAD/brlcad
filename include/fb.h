/*                            F B . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2011 United States Government as represented by
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

#ifndef __FB_H__
#define __FB_H__

#include "common.h"

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

#include "fbio.h"
#include "bu.h"


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

/* Library entry points which are true functions. */
FB_EXPORT extern void fb_configureWindow(FBIO *, int, int);
FB_EXPORT extern FBIO *fb_open(char *file, int _width, int _height);
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
FB_EXPORT extern void fb_log(const char *fmt, ...) __BU_ATTR_FORMAT12;
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

#ifdef IF_WGL
#  include <windows.h>
#  include <tk.h>
#  ifdef HAVE_GL_GL_H
#    include <GL/gl.h>
#  endif
FB_EXPORT extern int _wgl_open_existing(FBIO *ifp, Display *dpy, Window win, Colormap cmap, PIXELFORMATDESCRIPTOR *vip, HDC hdc, int width, int height, HGLRC glxc, int double_buffer, int soft_cmap);
#endif

/*
 * Copy one RGB pixel to another.
 */
#define COPYRGB(to, from) { (to)[RED]=(from)[RED];\
			   (to)[GRN]=(from)[GRN];\
			   (to)[BLU]=(from)[BLU]; }

/**
 * DEPRECATED: use fb_wpixel() instead.
 */
#define FB_WPIXEL(ifp, pp) fb_wpixel(ifp, pp)

/* Debug Bitvector Definition */
#define FB_DEBUG_BIO 1	/* Buffered io calls (less r/wpixel) */
#define FB_DEBUG_CMAP 2	/* Contents of colormaps */
#define FB_DEBUG_RW 4	/* Contents of reads and writes */
#define FB_DEBUG_BRW 8	/* Buffered IO rpixel and wpixel */

#define FB_CKMAG(_ptr, _magic, _str)	\
	if (!(_ptr)) { \
		fb_log("ERROR: null %s ptr, file %s, line %d\n", \
			_str, __FILE__, __LINE__); \
		abort(); \
	} else if ((uint32_t)(*((uintptr_t *)(_ptr))) != (uint32_t)(_magic)) { \
		fb_log("ERROR: bad %s ptr 0x%x, s/b 0x%x, was %p, file %s, line %d\n", \
		       _str, (uint32_t)(*((uintptr_t *)(_ptr))), (uint32_t)(_magic), \
		       (uint32_t)(*((uintptr_t *)(_ptr))), __FILE__, __LINE__); \
		abort(); \
	}

/* tcl.c */
/* The presence of Tcl_Interp as an arg prevents giving arg list */
FB_EXPORT extern void fb_tcl_setup();
FB_EXPORT extern int Fb_Init();
FB_EXPORT extern int fb_refresh(FBIO *ifp, int x, int y, int w, int h);


/**
 * report version information about LIBFB
 */
FB_EXPORT extern const char *fb_version(void);

#endif /* __FB_H__ */

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
