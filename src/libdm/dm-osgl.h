/*                        D M -  O S G . H
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
/** @addtogroup libdm */
/** @{ */
/** @file dm-osgl.h
 *
 */

#ifndef DM_OSGL_H
#define DM_OSGL_H

#ifdef DM_OSGL

#include "common.h"

#include "bu/vls.h"

#ifdef __cplusplus
#include <iostream>

#include <osg/GraphicsContext>
#include <osg/Timer>
#endif /* __cplusplus */

extern "C" {
/* For portable text in OpenGL, use fontstash */
#define FONTSTASH_IMPLEMENTATION
#include "./fontstash/fontstash.h"

#define GLFONTSTASH_IMPLEMENTATION
#include "./fontstash/glfontstash.h"
}

#define CMAP_BASE 40

/* Map +/-2048 GED space into -1.0..+1.0 :: x/2048*/
#define GED2IRIS(x)	(((float)(x))*0.00048828125)

#define Osgl_MV_O(_m) offsetof(struct modifiable_osgl_vars, _m)

struct modifiable_osgl_vars {
    dm *this_dm;
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

struct osgl_vars {
#ifdef __cplusplus
    GLdouble faceplate_mat[16];
#endif
    int face_flag;
    int *perspective_mode;
    struct FONScontext *fs;
    int fontNormal;
    int ovec;		/* Old color map entry number */
#ifdef __cplusplus
    GLclampf r, g, b;
#endif
    struct modifiable_osgl_vars mvars;
#if defined(_WIN32)
    HGLRC glxc; /* Need to figure out what OSGL needs on Win32 */
#endif
#ifdef __cplusplus
    osg::ref_ptr<osg::GraphicsContext> graphicsContext;
    osg::ref_ptr<osg::GraphicsContext::Traits> traits;
    osg::Timer *timer;
    int last_update_time;
#endif
};

__BEGIN_DECLS

#ifdef __cplusplus
extern "C" {
#endif
void osgl_fogHint(dm *dmp, int fastfog);
#ifdef __cplusplus
}
#endif

__END_DECLS

#endif /* DM_OSGL */
#endif /* DM_OSGL_H */

/** @} */
/*
 * Local Variables:
 * mode: C++
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
