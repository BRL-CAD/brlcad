/*                       D E F I N E S . H
 * BRL-CAD
 *
 * Copyright (c) 2015-2021 United States Government as represented by
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

#ifndef OPTICAL_DEFINES_H
#define OPTICAL_DEFINES_H

#include "common.h"

#include "bu/vls.h"
#include "bn/tabdata.h"
#include "rt/region.h"

/** @addtogroup liboptical
 *
 * @brief
 *  Definitions for the BRL-CAD Optical Library, LIBOPTICAL.
 *
 */
/** @{ */
/** @file optical/defines.h */

__BEGIN_DECLS

#ifndef OPTICAL_EXPORT
#  if defined(OPTICAL_DLL_EXPORTS) && defined(OPTICAL_DLL_IMPORTS)
#    error "Only OPTICAL_DLL_EXPORTS or OPTICAL_DLL_IMPORTS can be defined, not both."
#  elif defined(OPTICAL_DLL_EXPORTS)
#    define OPTICAL_EXPORT COMPILER_DLLEXPORT
#  elif defined(OPTICAL_DLL_IMPORTS)
#    define OPTICAL_EXPORT COMPILER_DLLIMPORT
#  else
#    define OPTICAL_EXPORT
#  endif
#endif

/* for liboptical */
OPTICAL_EXPORT extern double AmbientIntensity;
OPTICAL_EXPORT extern vect_t background;

/* defined in sh_text.c */
OPTICAL_EXPORT extern struct region env_region; /* environment map region */

/* defined in refract.c */
OPTICAL_EXPORT extern int max_bounces;
OPTICAL_EXPORT extern int max_ireflect;

struct floatpixel {
    double	ff_dist;		/**< @brief range to ff_hitpt[], <-INFINITY for miss */
    float	ff_hitpt[3];
    struct region *ff_regp;
    int	ff_frame;		/**< @brief >= 0 means pixel was reprojected */
    short	ff_x;			/**< @brief screen x, y coords of first location */
    short	ff_y;
    char	ff_color[3];
};

__END_DECLS

#endif /* OPTICAL_H */

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
