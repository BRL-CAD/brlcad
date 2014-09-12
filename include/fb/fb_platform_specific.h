/*          F B _ P L A T F O R M _ S P E C I F I C . H
 * BRL-CAD
 *
 * Copyright (c) 2014 United States Government as represented by
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
/** @addtogroup if */
/** @{*/
/** @file fb_platform_specific.h
 *
 * Structure holding information necessary for embedding a
 * framebuffer in an X11 parent window.  This is NOT public API
 * for libfb, and is not guaranteed to be stable from one
 * release to the next.
 *
 */
/** @} */

#ifdef FB_USE_INTERNAL_API
#ifdef IF_X

#include "common.h"
#include <X11/X.h>

struct X24_fb_info {
    Display *dpy;
    Window win;
    Window cwinp;
    Colormap cmap;
    XVisualInfo *vip;
    GC gc;
};

#endif /* IF_X */

#ifdef IF_OGL
#  ifdef HAVE_X11_XLIB_H
#    include <X11/Xlib.h>
#    include <X11/Xutil.h>
#  endif
/* glx.h on Mac OS X (and perhaps elsewhere) defines a slew of
 *  *  * parameter names that shadow system symbols.  protect the system
 *   *   * symbols by redefining the parameters prior to header inclusion.
 *    *    */
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

struct ogl_fb_info {
    Display *dpy;
    Window win;
    Colormap cmap;
    XVisualInfo *vip;
    GLXContext glxc;
    int double_buffer;
    int soft_cmap;
};
#endif /* IF_OGL */


#ifdef IF_WGL
#  include <windows.h>
/* The wgl interface as currently implemented uses some
 * X11 types, supplied by Tk. */
#  include <tk.h>
#  ifdef HAVE_GL_GL_H
#    include <GL/gl.h>
#  endif

struct wgl_fb_info {
    Display *dpy;
    Window win;
    Colormap cmap;
    PIXELFORMATDESCRIPTOR *vip;
    HDC hdc;
    HGLRC glxc;
    int double_buffer;
    int soft_cmap;
};
#endif /* IF_WGL */

#ifdef IF_QT
struct qt_fb_info {
    void *qapp;
    void *qwin;
    void *qpainter;
    void *draw;
    void *qimg;
};
#endif /* IF_QT */

#endif /* FB_USE_INTERNAL_API */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
