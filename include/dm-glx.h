/*                          D M - G L X . H
 * BRL-CAD
 *
 * Copyright (c) 1993-2011 United States Government as represented by
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
/** @addtogroup libdm */
/** @{ */
/** @file dm-glx.h
 *
 */

#ifndef __DM_GLX__
#define __DM_GLX__

/* Map +/-2048 GED space into -1.0..+1.0 :: x/2048*/
#define GED2IRIS(x)	(((float)(x))*0.00048828125)

#define Glx_MV_O(_m) offsetof(struct modifiable_glx_vars, _m)

struct modifiable_glx_vars {
    int cueing_on;
    int zclipping_on;
    int zbuffer_on;
    int lighting_on;
    int debug;
    int zbuf;
    int rgb;
    int doublebuffer;
    int min_scr_z;       /* based on getgdesc(GD_ZMIN) */
    int max_scr_z;       /* based on getgdesc(GD_ZMAX) */
};

struct glx_vars {
    struct bu_list l;
    Display *dpy;
    Window win;
    Tk_Window top;
    Tk_Window xtkwin;
    int depth;
    int omx, omy;
    unsigned int mb_mask;
    Colormap cmap;
    XVisualInfo *vip;
    int devmotionnotify;
    int devbuttonpress;
    int devbuttonrelease;
    int knobs[8];
    int stereo_is_on;
    int is_gt;
    struct modifiable_glx_vars mvars;
};

extern void glx_clearToBlack();
extern struct glx_vars head_glx_vars;

#endif /* __DM_GLX__ */

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
