/*                          D M - O G L . H
 * BRL-CAD
 *
 * Copyright (c) 1993-2021 United States Government as represented by
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
/** @addtogroup libstruct dm */
/** @{ */
/** @file dm-ogl.h
 *
 */

#ifndef DM_OGL_H
#define DM_OGL_H

#include "common.h"

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#ifdef HAVE_GL_GLX_H
#  include <GL/glx.h>
#endif
#ifdef HAVE_GL_GL_H
#  include <GL/gl.h>
#endif

#include "tk.h"
#define HAVE_X11_TYPES 1

extern struct dm dm_ogl;

/* Private, platform specific OpenGL variables */
struct pogl_vars {
    GLXContext glxc;
    int fontOffset;
    int ovec;		/* Old color map entry number */
    char is_direct;
};

struct dm_glxvars {
    Display *dpy;
    Window win;
    Tk_Window top;
    Tk_Window xtkwin;
    int depth;
    Colormap cmap;
    XVisualInfo *vip;
    XFontStruct *fontstruct;
    int devmotionnotify;
    int devbuttonpress;
    int devbuttonrelease;
};

#endif /* DM_OGL_H */

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
