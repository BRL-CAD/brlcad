/*                     D E F I N E S . H
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
/** @addtogroup libdm */
/** @{ */
/** @file dm/defines.h
 *
 */

#ifndef DM_DEFINES_H
#define DM_DEFINES_H

#include "common.h"

#ifndef DM_EXPORT
#  if defined(DM_DLL_EXPORTS) && defined(DM_DLL_IMPORTS)
#    error "Only DM_DLL_EXPORTS or DM_DLL_IMPORTS can be defined, not both."
#  elif defined(DM_DLL_EXPORTS)
#    define DM_EXPORT COMPILER_DLLEXPORT
#  elif defined(DM_DLL_IMPORTS)
#    define DM_EXPORT COMPILER_DLLIMPORT
#  else
#    define DM_EXPORT
#  endif
#endif

#ifndef FB_EXPORT
#  if defined(FB_DLL_EXPORTS) && defined(FB_DLL_IMPORTS)
#    error "Only FB_DLL_EXPORTS or FB_DLL_IMPORTS can be defined, not both."
#  elif defined(FB_DLL_EXPORTS)
#    define FB_EXPORT COMPILER_DLLEXPORT
#  elif defined(FB_DLL_IMPORTS)
#    define FB_EXPORT COMPILER_DLLIMPORT
#  else
#    define FB_EXPORT
#  endif
#endif

/* The internals of the dm structure are hidden using the PImpl pattern*/
struct dm_impl;
struct dm {
    uint32_t magic;
    struct dm_impl *i;
};

struct dm_plugin {
    uint32_t api_version; /* must be first in struct */
    const struct dm * const p;
};

/* The internals of the framebuffer structure are hidden using the PImpl pattern */
struct fb_impl;
struct fb {
    struct fb_impl *i;
};

struct fb_plugin {
    const struct fb * const p;
};

#endif /* DM_DEFINES_H */

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
