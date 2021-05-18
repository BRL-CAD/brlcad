/*                          D M - W G L . H
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
/** @file dm-wgl.h
 *
 */

#ifndef DM_WGL_H
#define DM_WGL_H

#include "common.h"

#ifdef HAVE_GL_GL_H
#  include <GL/gl.h>
#endif

#include "tk.h"
#define HAVE_X11_TYPES 1
#include "tkWinInt.h"

#include "bu/vls.h"
#include "pkg.h"
#include "dm/fbserv.h"

__BEGIN_DECLS

#define CMAP_BASE 40

/* Map +/-2048 GED space into -1.0..+1.0 :: x/2048*/
#define GED2IRIS(x)	(((float)(x))*0.00048828125)

extern struct dm dm_wgl;

#define Ogl_MV_O(_m) offsetof(struct modifiable_ogl_vars, _m)

struct modifiable_ogl_vars {
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
};

struct dm_wglvars {
    Display *dpy;
    Window win;
    Tk_Window top;
    Tk_Window xtkwin;
    int depth;
    Colormap cmap;
    PIXELFORMATDESCRIPTOR *vip;
    HFONT fontstruct;
    HDC  hdc;      /* device context of device that OpenGL calls are to be drawn on */
    int devmotionnotify;
    int devbuttonpress;
    int devbuttonrelease;
};

struct wgl_vars {
    HGLRC glxc;
    GLdouble faceplate_mat[16];
    int face_flag;
    int *perspective_mode;
    int fontOffset;
    int ovec;		/* Old color map entry number */
    char is_direct;
    GLclampf r, g, b;
};

extern void wgl_fogHint();

// fbserv callbacks
extern int wgl_is_listening(struct fbserv_obj *fbsp);
extern int wgl_listen_on_port(struct fbserv_obj *fbsp, int available_port);
extern void wgl_open_server_handler(struct fbserv_obj *fbsp);
extern void wgl_close_server_handler(struct fbserv_obj *fbsp);
extern void wgl_open_client_handler(struct fbserv_obj *fbsp, int i, void *data);
extern void wgl_close_client_handler(struct fbserv_obj *fbsp, int sub);

__END_DECLS

#endif /* DM_WGL_H */

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
