/*                      F B _ W G L . H
 * BRL-CAD
 *
 * Copyright (c) 2014-2022 United States Government as represented by
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
/** @{*/
/** @file fb_wgl.h
 *
 * Structure holding information necessary for embedding a
 * framebuffer in a Windows Tk parent window.  This is NOT public API
 * for libfb, and is not guaranteed to be stable from one
 * release to the next.
 *
 */
/** @} */

#ifdef FB_USE_INTERNAL_API
#  include "common.h"

#  include "bio.h"

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
