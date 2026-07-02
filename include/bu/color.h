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

/* forward declaration; full definition in bu/vls.h */
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

#define BU_COLOR_RED 0
#define BU_COLOR_GREEN 1
#define BU_COLOR_BLUE 2
#define BU_COLOR_ALPHA 3

#define HUE 0
#define SAT 1
#define VAL 2

#define BU_COLOR_HUE 0
#define BU_COLOR_SATURATION 1
#define BU_COLOR_VALUE 2

/**
 * A single color value.
 *
 * The canonical representation is a red/green/blue/alpha quadruple of
 * floating point channels (buc_rgb[RED], buc_rgb[GRN], buc_rgb[BLU],
 * buc_rgb[ALP]).  Channels are normally in the [0.0, 1.0] range, but
 * the floating point storage is deliberately capable of holding the
 * out-of-gamut and greater-than-one values needed for future high
 * dynamic range (HDR) support without any change to this structure.
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
 * Utility functions to convert between various containers
 * for color handling.
 *
 * FIXME: inconsistent input/output parameters!
 */
BU_EXPORT extern int bu_color_from_rgb_floats(struct bu_color *cp, const fastf_t *rgb);
BU_EXPORT extern int bu_color_from_rgb_chars(struct bu_color *cp, const unsigned char *rgb);

/**
 * Set a color from a human-readable color specification string.
 *
 * This is the single canonical entry point for turning any of the
 * color notations used throughout BRL-CAD into a normalized
 * struct bu_color.  Leading and trailing whitespace is ignored and
 * matching is case-insensitive.  The following notations are accepted:
 *
 *   - integer triplet, 0 to 255 per channel, separated by any of
 *     '/', ',', or whitespace, e.g. "255/0/0", "255,0,0", "255 0 0"
 *   - floating point triplet, 0.0 to 1.0 per channel, same separators,
 *     e.g. "1.0/0.0/0.0"
 *   - hexadecimal "#rgb", "#rgba", "#rrggbb", or "#rrggbbaa" (the
 *     three and four digit forms expand each nibble, so "#f00" is red);
 *     the "a" forms additionally set the alpha channel
 *   - a named color from the CSS/SVG set, e.g. "red", "navy",
 *     "cornflowerblue", "transparent"
 *   - functional notation "model(c0, c1, ...)" where model is one of
 *     rgb, rgba, hsv, hsl, cmyk, gray (or grey).  Channels may be
 *     separated by '/', ',', or whitespace and any channel may be
 *     given as a percentage with a trailing '%'.  An extra trailing
 *     channel is interpreted as alpha.  Examples: "rgb(255, 0, 0)",
 *     "hsv(120, 100%, 50%)", "cmyk(0, 1, 1, 0)", "gray(50%)".
 *
 * Returns 1 on success (color is set) and 0 on failure (color is left
 * unchanged).  Channels are clamped to the [0.0, 1.0] gamut; a
 * specification with an out-of-range channel is rejected.  The alpha
 * channel is only written when the specification supplies one.
 */
BU_EXPORT extern int bu_color_from_str(struct bu_color *cp, const char *str);

/**
 * Serialize a color into a human-readable string appended to the
 * provided vls.
 *
 * The 'format' argument selects the notation and may be NULL or "" for
 * the canonical "R/G/B" integer triplet.  Recognized formats mirror
 * the notations accepted by bu_color_from_str() so the output can be
 * round-tripped:  "rgb" ("R/G/B"), "rgba" ("R/G/B/A"), "hex"
 * ("#rrggbb"), "hexa" ("#rrggbbaa"), "float" ("r/g/b" in [0,1]),
 * "hsv" ("hsv(h,s,v)"), "hsl" ("hsl(h,s,l)"), and "cmyk"
 * ("cmyk(c,m,y,k)").
 *
 * Returns 1 on success and 0 on failure (unknown format or NULL args).
 */
BU_EXPORT extern int bu_color_to_str(struct bu_vls *str, const struct bu_color *cp, const char *format);

BU_EXPORT extern int bu_str_to_rgb(const char *str, unsigned char *rgb);  /* inconsistent, deprecate */

BU_EXPORT extern int bu_color_to_rgb_floats(const struct bu_color *cp, fastf_t *rgb); /* bu_color_as_rgb_3fv */
BU_EXPORT extern int bu_color_to_rgb_chars(const struct bu_color *cp, unsigned char *rgb); /* bu_color_as_rgb */
BU_EXPORT extern int bu_color_to_rgb_ints(const struct bu_color *cp, int *r, int *g, int *b); /* bu_color_as_rgb_3i */


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
