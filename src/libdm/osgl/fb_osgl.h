/*                      F B _ O S G L . H
 * BRL-CAD
 *
 * Copyright (c) 2014-2021 United States Government as represented by
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
/** @file fb_osgl.h
 *
 * Structure holding information necessary for embedding a
 * framebuffer in an X11 parent window.  This is NOT public API
 * for libfb, and is not guaranteed to be stable from one
 * release to the next.
 *
 */
/** @} */

#ifdef FB_USE_INTERNAL_API
#include "common.h"

#ifdef HAVE_SIGNAL_H
#  include <signal.h>
#endif
#ifdef HAVE_SYS_TYPES_H
#  include <sys/types.h>
#endif
#ifdef HAVE_SYS_SHM_H
#  include <sys/shm.h>
#endif
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#ifdef HAVE_SYS_IPC_H
#  include <sys/ipc.h>
#endif
#include <errno.h>

#ifdef HAVE_SYS_WAIT_H
#  include <sys/wait.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif
#include "bu/color.h"
#include "bu/file.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "bu/parallel.h"
#include "bu/str.h"
#include "dm.h"
#ifdef __cplusplus
}
#endif

#if defined(__GNUC__) && !defined(__clang__)
#  pragma GCC diagnostic push
#endif
#if defined(__clang__)
#  pragma clang diagnostic push
#endif
#if defined(__GNUC__) && !defined(__clang__)
#  pragma GCC diagnostic ignored "-Wfloat-equal"
#  pragma GCC diagnostic ignored "-Wdeprecated-copy"
#endif
#if defined(__clang__)
#  pragma clang diagnostic ignored "-Wfloat-equal"
#  pragma clang diagnostic ignored "-Wdeprecated-copy"
#endif

#ifdef __cplusplus
#include <osg/GraphicsContext>
#include <osgViewer/Viewer>
#include <osgViewer/GraphicsWindow>
#include <osgViewer/ViewerEventHandlers>
#include <osgViewer/config/SingleWindow>

#include <osgDB/Registry>

#include <osgGA/StateSetManipulator>

#include <osg/ImageUtils>
#include <osg/TextureRectangle>
#include <osg/Geode>
#include <osg/Geometry>
#include <osg/DrawPixels>
#include <osg/StateSet>
#include <osg/Timer>

#include <iostream>
#endif

#if defined(__GNUC__) && !defined(__clang__)
#  pragma GCC diagnostic pop
#endif
#if defined(__clang__)
#  pragma clang diagnostic pop
#endif



struct osgl_fb_info {
    void *glc;
    void *traits;
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
