/*                        C O L O R . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2014 United States Government as represented by
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

/** @file color.h
  */
#ifndef BU_COLOR_H
#define BU_COLOR_H

#include "common.h"

#include "bu/defines.h"
#include "bu/magic.h"

/*----------------------------------------------------------------------*/

/** @addtogroup color */
/** @{ */
/** @file libbu/color.c */

#define RED 0
#define GRN 1
#define BLU 2

#define HUE 0
#define SAT 1
#define VAL 2

#define ACHROMATIC	-1.0

struct bu_color
{
    uint32_t buc_magic;
    fastf_t buc_rgb[3];
};
typedef struct bu_color bu_color_t;
#define BU_COLOR_NULL ((struct bu_color *) 0)

/**
 * asserts the integrity of a bu_color struct.
 */
#define BU_CK_COLOR(_c) BU_CKMAG(_c, BU_COLOR_MAGIC, "bu_color")

/**
 * initializes a bu_bitv struct without allocating any memory.
 */
#define BU_COLOR_INIT(_c) { \
	(_c)->buc_magic = BU_COLOR_MAGIC; \
	(_c)->buc_rgb[0] = (_c)->buc_rgb[1] = (_c)->buc_rgb[2] = 0; \
    }

/**
 * macro suitable for declaration statement initialization of a bu_color
 * struct.  does not allocate memory.
 */
#define BU_COLOR_INIT_ZERO { BU_COLOR_MAGIC, {0, 0, 0} }

/**
 * returns truthfully whether a bu_color has been initialized
 */
#define BU_COLOR_IS_INITIALIZED(_c) (((struct bu_color *)(_c) != BU_COLOR_NULL) && LIKELY((_c)->magic == BU_COLOR_MAGIC))

/**
 * Convert between RGB and HSV color models
 *
 * R, G, and B are in {0, 1, ..., 255},
 *
 * H is in [0.0, 360.0), and S and V are in [0.0, 1.0],
 *
 * unless S = 0.0, in which case H = ACHROMATIC.
 *
 * These two routines are adapted from:
 * pp. 592-3 of J.D. Foley, A. van Dam, S.K. Feiner, and J.F. Hughes,
 * _Computer graphics: principles and practice_, 2nd ed., Addison-Wesley,
 * Reading, MA, 1990.
 */

/* color.c */
BU_EXPORT extern void bu_rgb_to_hsv(unsigned char *rgb, fastf_t *hsv);
BU_EXPORT extern int bu_hsv_to_rgb(fastf_t *hsv, unsigned char *rgb);
BU_EXPORT extern int bu_str_to_rgb(char *str, unsigned char *rgb);
BU_EXPORT extern int bu_color_from_rgb_floats(struct bu_color *cp, fastf_t *rgb);
BU_EXPORT extern int bu_color_to_rgb_floats(struct bu_color *cp, fastf_t *rgb);

/* UNIMPLEMENTED
 *
 * BU_EXPORT export void bu_color_from_rgb_chars(struct bu_color *cp, unsigned char *rgb);
 * BU_EXPORT export int bu_color_to_rgb_chars(struct bu_color *cp, unsigned char *rgb);
 * BU_EXPORT export int bu_color_from_hsv_floats(struct bu_color *cp, fastf_t *hsv);
 * BU_EXPORT export int bu_color_to_hsv_floats(struct bu_color *cp, fastf_t *hsv);
 */


/** @} */


#endif  /* BU_COLOR_H */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
