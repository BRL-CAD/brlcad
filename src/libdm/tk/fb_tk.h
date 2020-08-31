/*                      F B _ T K . H
 * BRL-CAD
 *
 * Copyright (c) 2014-2020 United States Government as represented by
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
/** @file fb_tk.h
 *
 * Structure holding information necessary for embedding a
 * framebuffer in an X OpenGL parent window.  This is NOT public API
 * for libfb, and is not guaranteed to be stable from one
 * release to the next.
 *
 */
/** @} */

#ifdef FB_USE_INTERNAL_API
#include "common.h"
#include "tk.h"
#include "OSMesa/gl.h"
#include "OSMesa/osmesa.h"

struct tk_fb_info {
    Display *dpy;
    Window win;
    OSMesaContext glxc;
    void *buf;
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
