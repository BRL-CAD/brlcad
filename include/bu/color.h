/*                        C O L O R . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2021 United States Government as represented by
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

#ifndef BU_COLOR_H
#define BU_COLOR_H

#include "common.h"

#include "vmath.h"

#include "bu/defines.h"
#include "bu/magic.h"

__BEGIN_DECLS

/** @addtogroup bu_color
 * @brief
 * Support for storing and manipulating color data.
 */
/** @{ */
/** @file bu/color.h */

#define RED 0
#define GRN 1
#define BLU 2
#define ALP 3

#define HUE 0
#define SAT 1
#define VAL 2


/**
 * a single color value, stored as a 0.0 to 1.0 triplet for RGBA
 */
struct bu_color
{
    hvect_t buc_rgb;
};
typedef struct bu_color bu_color_t;
#define BU_COLOR_NULL ((struct bu_color *) 0)

/**
 * initializes a bu_color struct without allocating any memory.
 */
#define BU_COLOR_INIT(_c) { \
	(_c)->buc_rgb[RED] = (_c)->buc_rgb[GRN] = (_c)->buc_rgb[BLU] = 0; (_c)->buc_rgb[ALP]; \
    }

/**
 * macro suitable for declaration statement initialization of a bu_color
 * struct.  does not allocate memory.
 */
#define BU_COLOR_INIT_ZERO {{0, 0, 0, 0}}



/** random color generating methods */
typedef enum {
    BU_COLOR_RANDOM = 0,
    BU_COLOR_RANDOM_LIGHTENED
} bu_color_rand_t;

/**
 * Function to generate random color
 *
 * Refactoring points:
 *   truly random color
 *     3dm-g: src/libgcv/plugins/rhino/rhino_read.cpp
 *   "constrained" random
 *     BRLCADWrapper:getRandomColor(): src/conv/step/BRLCADWrapper.cpp

 */
BU_EXPORT extern int bu_color_rand(struct bu_color *c, bu_color_rand_t type);

#if 0

/**
 * Refactoring points:
 * color command (set specified color)
 *     src/libged/color.c
 *     src/libged/brep.c
 *   get color from string
 *     src/libbu/color.c

* Possible calling syntax:
 @code
 * // draw a purely random color in 0/0/0 to 255/255/255 range
 * bn_color_samples(colors, NULL, COLOR_RANDOM, 1); // problematic in libbu, random is libbn domain
 *
 * // draw a golden ratio distribution random color in 0/0/0 to 255/255/255 range, s=0.5, v=0.95
 * bn_color_samples(colors, NULL, COLOR_RANDOM_LIGHTENED, 1); // problematic in libbu, random is libbn domain
 *
 * // draw bezier interpolated and lightened samples
 * struct bu_color range[4] = {0};
 * bu_color_from_str(&range[0], "#0f0"); // green
 * bu_color_from_str(&range[1], "0.f/0.f/1.f") // blue
 * bu_color_from_str(&range[2], "purple");
 * bn_color_samples(colors, range, COLOR_LINEAR, 10); // 10 dark colors from green to blue to purple
 *
 * // return a standard "heat map" with 18 quantized samples
 * bn_color_samples(colors, NULL, COLOR_STANDARD_HEAT, 18);
 @endcode
 *
 * Need:
 *   way to map from different color specifications to color including
 *     name: "red"
 *     rgbint: 255/0/0
 *     rgbfloat: 1.0f/0f/0f
 *     hexint: #FF0000
 *     hexshort: #F00
 *     hsv: 0/100%/100%
 *     hsl: 0/100%/50%
 *     ignoring YCbCr, YPbPr, YUV, YIQ, CMYK, CIE LAB
 *     ignoring grayscale specification
 */
/**
 * Return a set of sampled colors given a range of zero or more colors
 * (a NULL-terminated list of colors), a sample method, and desired
 * number of color samples to return.
 *
 * Specifying no colors implies full spectrum.  The default sampling
 * method uses a golden ratio distribution to give a "balanced" random
 * distribution that is effective with dark backgrounds and/or text.
 *
 * Returns the number of samples allocated.
 */
size_t bn_color_samples(struct bu_color **samples, const bu_color *colors, enum sampleMethod, size_t numSamples);
#endif


/**
 * Convert between RGB and HSV color models
 *
 * R, G, and B are in {0, 1, ..., 255},
 *
 * H is in [0.0, 360.0), and S and V are in [0.0, 1.0],
 *
 * If S == 0.0, H is achromatic and set to 0.0
 *
 * These two routines are adapted from:
 * pp. 592-3 of J.D. Foley, A. van Dam, S.K. Feiner, and J.F. Hughes,
 * _Computer graphics: principles and practice_, 2nd ed., Addison-Wesley,
 * Reading, MA, 1990.
 */
BU_EXPORT extern void bu_rgb_to_hsv(const unsigned char *rgb, fastf_t *hsv);
BU_EXPORT extern int bu_hsv_to_rgb(const fastf_t *hsv, unsigned char *rgb);


/**
 * Utility functions to convert between various containers
 * for color handling.
 *
 * FIXME: inconsistent input/output parameters!
 * TODO: consider stdarg ... to consolidate all the _from_ functions, e.g.,
 *   // 3 colors
 *   bu_color_create(struct bu_color **colors, "red", "0/255/0", "#0000ff", NULL);
 *
 *   // 2 colors from existing data
 *   struct bu_color *colors = NULL;
 *   bu_color_create(&colors, "%d/%d/%d", rgb[0], rgb[1], rgb[2], "hsv(%lf,0.5,0.95)", hsv, NULL);
 *   bu_color_destroy(colors);
 */
BU_EXPORT extern int bu_color_from_rgb_floats(struct bu_color *cp, const fastf_t *rgb);
BU_EXPORT extern int bu_color_from_rgb_chars(struct bu_color *cp, const unsigned char *rgb);
BU_EXPORT extern int bu_color_from_str(struct bu_color *cp, const char *str);
/* UNIMPLEMENTED: BU_EXPORT extern int bu_color_from_hsv_floats(struct bu_color *cp, fastf_t *hsv); */

BU_EXPORT extern int bu_str_to_rgb(const char *str, unsigned char *rgb);  /* inconsistent, deprecate */

BU_EXPORT extern int bu_color_to_rgb_floats(const struct bu_color *cp, fastf_t *rgb); /* bu_color_as_rgb_3fv */
BU_EXPORT extern int bu_color_to_rgb_chars(const struct bu_color *cp, unsigned char *rgb); /* bu_color_as_rgb */
BU_EXPORT extern int bu_color_to_rgb_ints(const struct bu_color *cp, int *r, int *g, int *b); /* bu_color_as_rgb_3i */
/* UNIMPLEMENTED: BU_EXPORT extern int bu_color_to_hsv_floats(struct bu_color *cp, fastf_t *hsv); */ /* bu_color_as_hsv_3fv */


/** @} */

__END_DECLS

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
