/*                          D M - O G L . H
 * BRL-CAD
 *
 * Copyright (c) 1993-2014 United States Government as represented by
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
/** @file dm-ogl.h
 *
 */

#ifndef DM_OGL_H
#define DM_OGL_H

#include "common.h"

#ifdef HAVE_GL_GLX_H
#  include <GL/glx.h>
#endif
#ifdef HAVE_GL_GL_H
#  include <GL/gl.h>
#endif

#include "bu/vls.h"

#define CMAP_BASE 40

/* Map +/-2048 GED space into -1.0..+1.0 :: x/2048*/
#define GED2IRIS(x)	(((float)(x))*0.00048828125)


struct ogl_vars {
    GLXContext glxc;
    GLdouble faceplate_mat[16];
    int face_flag;
    int *perspective_mode;
    int fontOffset;
    int ovec;		/* Old color map entry number */
    char is_direct;
    GLclampf r, g, b;
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
