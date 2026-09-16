/*                        C O L O R . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2026 United States Government as represented by
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

struct bu_vls;

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

#define BU_COLOR_INDEX_RED 0
#define BU_COLOR_INDEX_GREEN 1
#define BU_COLOR_INDEX_BLUE 2
#define BU_COLOR_INDEX_ALPHA 3

#define HUE 0
#define SAT 1
#define VAL 2

#define BU_COLOR_INDEX_HUE 0
#define BU_COLOR_INDEX_SATURATION 1
#define BU_COLOR_INDEX_VALUE 2

/**
 * A single color value, stored as a normalized RGBA quadruple.
 */
struct bu_color
{
    hvect_t buc_rgb;
};
typedef struct bu_color bu_color_t;
#define BU_COLOR_NULL ((struct bu_color *) 0)

/** Color spaces supported by bu_color_convert(). */
typedef enum bu_color_space {
    BU_COLOR_SPACE_RGB = 0,
    BU_COLOR_SPACE_HSL,
    BU_COLOR_SPACE_HSV
} bu_color_space_t;

/** Text representations supported by bu_color_format(). */
typedef enum bu_color_format {
    BU_COLOR_FORMAT_RGB = 0,
    BU_COLOR_FORMAT_RGBA,
    BU_COLOR_FORMAT_HEX,
    BU_COLOR_FORMAT_HEXA,
    BU_COLOR_FORMAT_HSL,
    BU_COLOR_FORMAT_HSLA,
    BU_COLOR_FORMAT_HSV,
    BU_COLOR_FORMAT_HSVA,
    BU_COLOR_FORMAT_NAME
} bu_color_format_t;

/**
 * initializes a bu_color struct without allocating any memory.
 */
#define BU_COLOR_INIT(_c) { \
	(_c)->buc_rgb[RED] = (_c)->buc_rgb[GRN] = (_c)->buc_rgb[BLU] = 0; (_c)->buc_rgb[ALP] = 0; \
    }

/**
 * Check whether two colors are equal within a tolerance.
 */
#define BU_COLOR_NEAR_EQUAL(_c1, _c2, _tol) \
    HNEAR_EQUAL(_c1.buc_rgb, _c2.buc_rgb, _tol)


/**
 * macro suitable for declaration statement initialization of a bu_color
 * struct.  does not allocate memory.
 */
#define BU_COLOR_INIT_ZERO {{0, 0, 0, 0}}

/* Initializers for commonly used colors */
#define BU_COLOR_BLUE   {{0, 0, 1, 0}}
#define BU_COLOR_CYAN   {{0, 1, 1, 0}}
#define BU_COLOR_GREEN  {{0, 1, 0, 0}}
#define BU_COLOR_PURPLE {{1, 0, 1, 0}}
#define BU_COLOR_RED    {{1, 0, 0, 0}}
#define BU_COLOR_WHITE  {{1, 1, 1, 0}}
#define BU_COLOR_YELLOW {{1, 1, 0, 0}}

/**
 * Copy a bu_color
 */
#define BU_COLOR_CPY(_dest, _src) {\
    (_dest)->buc_rgb[RED] = (_src)->buc_rgb[RED]; \
    (_dest)->buc_rgb[GRN] = (_src)->buc_rgb[GRN]; \
    (_dest)->buc_rgb[BLU] = (_src)->buc_rgb[BLU]; \
    (_dest)->buc_rgb[ALP] = (_src)->buc_rgb[ALP]; \
}


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
 * Parse a human-readable color specification.
 *
 * Accepted representations include the legacy BRL-CAD integer and
 * normalized floating point RGB triplets, hexadecimal RGB and RGBA,
 * CSS named colors, and RGB(A), HSL(A), and HSV(A) functional notation.
 * RGB and alpha are normalized to [0.0, 1.0] in color.  An omitted alpha
 * channel is set to 1.0 (opaque).
 *
 * Returns 1 on success and 0 on failure.  On failure, color is unchanged.
 */
BU_EXPORT extern int bu_color_parse(const char *str, struct bu_color *color);

/**
 * Append a human-readable representation of color to output.
 *
 * RGB(A) uses functional notation with 8-bit RGB channels and normalized
 * alpha.  HSL(A) and HSV(A) use hue in degrees, percentages for the other
 * model channels, and normalized alpha.  Hexadecimal output uses CSS RGBA
 * channel order.  Named output fails when color has no exact CSS name.
 *
 * Returns 1 on success and 0 on failure.  On failure, output is unchanged.
 */
BU_EXPORT extern int bu_color_format(const struct bu_color *color,
				     bu_color_format_t format,
				     struct bu_vls *output);

/**
 * Convert a numeric color between RGB, HSL, and HSV.
 *
 * RGB channels, saturation, lightness, value, and alpha use [0.0, 1.0].
 * Hue is expressed in degrees and is wrapped to [0.0, 360.0).  Alpha is
 * copied without conversion.  All four input components must be finite.
 *
 * Returns 1 on success and 0 on failure.  On failure, out is unchanged.
 */
BU_EXPORT extern int bu_color_convert(const double in[4],
				      bu_color_space_t in_space,
				      bu_color_space_t out_space,
				      double out[4]);


/**
 * Legacy utility functions for converting RGB storage containers.
 * New color-space and text conversions should use bu_color_convert(),
 * bu_color_parse(), and bu_color_format().
 */
BU_EXPORT extern int bu_color_from_rgb_floats(struct bu_color *cp, const fastf_t *rgb);
BU_EXPORT extern int bu_color_from_rgb_chars(struct bu_color *cp, const unsigned char *rgb);
/** Compatibility parser equivalent to bu_color_parse(), except that cp's
 * alpha channel is left unchanged.
 */
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
