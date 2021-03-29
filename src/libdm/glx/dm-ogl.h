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

#include "bu/vls.h"

#define CMAP_BASE 40

/* Map +/-2048 GED space into -1.0..+1.0 :: x/2048*/
#define GED2IRIS(x)	(((float)(x))*0.00048828125)

#define DM_REVERSE_COLOR_BYTE_ORDER(_shift, _mask) {    \
        _shift = 24 - _shift;                           \
        switch (_shift) {                               \
            case 0:                                     \
                _mask >>= 24;                           \
                break;                                  \
            case 8:                                     \
                _mask >>= 8;                            \
                break;                                  \
            case 16:                                    \
                _mask <<= 8;                            \
                break;                                  \
            case 24:                                    \
                _mask <<= 24;                           \
                break;                                  \
        }                                               \
    }

extern struct dm dm_ogl;

#define Ogl_MV_O(_m) offsetof(struct ogl_vars, _m)

struct ogl_internal_vars {
    int faceFlag;
    GLdouble faceplate_mat[16];
    GLclampf r, g, b;
};

struct ogl_vars {
    struct dm *this_dm;
    int cueing_on;
    int zclipping_on;
    int zbuffer_on;
    int lighting_on;
    int transparency_on;
    int fastfog;
    double fogdensity;
    int zbuf;
    int rgb;
    int doublebuffer;
    int depth;
    int debug;
    struct bu_vls log;
    double bound;
    int boundFlag;
    struct ogl_internal_vars i;
};

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

__BEGIN_DECLS

extern void ogl_fogHint();

__END_DECLS

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
