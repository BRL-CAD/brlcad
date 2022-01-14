/*                          D M - O G L . H
 * BRL-CAD
 *
 * Copyright (c) 1993-2022 United States Government as represented by
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
/** @file dm-qtgl.h
 *
 */

#ifndef DM_OGL_H
#define DM_OGL_H

#include "common.h"

#include "bio.h" /* for windows.h */

// OSX has deprecated OpenGL since version 10.14
#define GL_SILENCE_DEPRECATION 1

#ifdef HAVE_GL_GL_H
#  include <GL/gl.h>
#endif

#ifdef __cplusplus
#include <QtWidgets/QOpenGLWidget>
#endif

extern "C" {
/* For portable text in OpenGL, use fontstash */
#if defined(__GNUC__) && !defined(__clang__)
#  pragma GCC diagnostic push
#endif
#if defined(__clang__)
#  pragma clang diagnostic push
#endif
#if defined(__GNUC__) && !defined(__clang__)
#  pragma GCC diagnostic ignored "-Wfloat-equal"
#endif
#if defined(__clang__)
#  pragma clang diagnostic ignored "-Wfloat-equal"
#endif

#define FONTSTASH_IMPLEMENTATION
#include "../fontstash/fontstash.h"
#define GLFONTSTASH_IMPLEMENTATION
#include "../fontstash/glfontstash.h"

#if defined(__GNUC__) && !defined(__clang__)
#  pragma GCC diagnostic pop
#endif
#if defined(__clang__)
#  pragma clang diagnostic pop
#endif
}

extern struct dm dm_qtgl;

struct qtgl_vars {
#ifdef __cplusplus
    QOpenGLWidget *qw;
#endif
    struct FONScontext *fs;
    int fontNormal;
    int fontOffset;
    int *perspective_mode;
    int ovec;		/* Old color map entry number */
    char is_direct;
};

struct dm_qtvars {
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
