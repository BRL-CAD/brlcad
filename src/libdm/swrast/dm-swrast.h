/*                    D M - S W R A S T . H
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
/** @file dm-swrast.h
 *
 */

#ifndef DM_SWRAST_H
#define DM_SWRAST_H

#include "common.h"

extern "C" {
#include "OSMesa/gl.h"
#include "OSMesa/osmesa.h"

#include "bv.h"

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

extern struct dm dm_swrast;

struct swrast_vars {
    struct bv *v;
    OSMesaContext ctx;
    void *os_b;
    struct FONScontext *fs;
    int fontNormal;
    int fontOffset;
    int *perspective_mode;
    int ovec;		/* Old color map entry number */
    char is_direct;
};

struct dm_swvars {
    int devmotionnotify;
    int devbuttonpress;
    int devbuttonrelease;
};

#endif /* DM_SWRAST_H */

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
