/*                       C O L O R . C P P
 * BRL-CAD
 *
 * Copyright (c) 1997-2026 United States Government as represented by
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

#include "common.h"

#include <stdlib.h>
#include <math.h>
#include <cmath>
#include <ctype.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include <random>
#include "bio.h"

#include "bu/color.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "bu/assert.h"
#include "bu/str.h"
#include "bu/vls.h"


#define COMMA ','

/*
 * The string grammar, named colors, and RGB/HSL/HSV conversion formulas
 * below are adapted from TinyColor:
 *
 * https://github.com/bgrins/TinyColor
 *
 * Copyright (c), Brian Grinstead, http://briangrinstead.com
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

struct bu_color_name {
    const char *name;
    unsigned int rgb;
};

static const struct bu_color_name _bu_color_names[] = {
    {"aliceblue", 0xf0f8ff},
    {"antiquewhite", 0xfaebd7},
    {"aqua", 0x00ffff},
    {"aquamarine", 0x7fffd4},
    {"azure", 0xf0ffff},
    {"beige", 0xf5f5dc},
    {"bisque", 0xffe4c4},
    {"black", 0x000000},
    {"blanchedalmond", 0xffebcd},
    {"blue", 0x0000ff},
    {"blueviolet", 0x8a2be2},
    {"brown", 0xa52a2a},
    {"burlywood", 0xdeb887},
    {"cadetblue", 0x5f9ea0},
    {"chartreuse", 0x7fff00},
    {"chocolate", 0xd2691e},
    {"coral", 0xff7f50},
    {"cornflowerblue", 0x6495ed},
    {"cornsilk", 0xfff8dc},
    {"crimson", 0xdc143c},
    {"cyan", 0x00ffff},
    {"darkblue", 0x00008b},
    {"darkcyan", 0x008b8b},
    {"darkgoldenrod", 0xb8860b},
    {"darkgray", 0xa9a9a9},
    {"darkgreen", 0x006400},
    {"darkgrey", 0xa9a9a9},
    {"darkkhaki", 0xbdb76b},
    {"darkmagenta", 0x8b008b},
    {"darkolivegreen", 0x556b2f},
    {"darkorange", 0xff8c00},
    {"darkorchid", 0x9932cc},
    {"darkred", 0x8b0000},
    {"darksalmon", 0xe9967a},
    {"darkseagreen", 0x8fbc8f},
    {"darkslateblue", 0x483d8b},
    {"darkslategray", 0x2f4f4f},
    {"darkslategrey", 0x2f4f4f},
    {"darkturquoise", 0x00ced1},
    {"darkviolet", 0x9400d3},
    {"deeppink", 0xff1493},
    {"deepskyblue", 0x00bfff},
    {"dimgray", 0x696969},
    {"dimgrey", 0x696969},
    {"dodgerblue", 0x1e90ff},
    {"firebrick", 0xb22222},
    {"floralwhite", 0xfffaf0},
    {"forestgreen", 0x228b22},
    {"fuchsia", 0xff00ff},
    {"gainsboro", 0xdcdcdc},
    {"ghostwhite", 0xf8f8ff},
    {"gold", 0xffd700},
    {"goldenrod", 0xdaa520},
    {"gray", 0x808080},
    {"green", 0x008000}, /* ugh */
    {"greenyellow", 0xadff2f},
    {"grey", 0x808080},
    {"honeydew", 0xf0fff0},
    {"hotpink", 0xff69b4},
    {"indianred", 0xcd5c5c},
    {"indigo", 0x4b0082},
    {"ivory", 0xfffff0},
    {"khaki", 0xf0e68c},
    {"lavender", 0xe6e6fa},
    {"lavenderblush", 0xfff0f5},
    {"lawngreen", 0x7cfc00},
    {"lemonchiffon", 0xfffacd},
    {"lightblue", 0xadd8e6},
    {"lightcoral", 0xf08080},
    {"lightcyan", 0xe0ffff},
    {"lightgoldenrodyellow", 0xfafad2},
    {"lightgray", 0xd3d3d3},
    {"lightgreen", 0x90ee90},
    {"lightgrey", 0xd3d3d3},
    {"lightpink", 0xffb6c1},
    {"lightsalmon", 0xffa07a},
    {"lightseagreen", 0x20b2aa},
    {"lightskyblue", 0x87cefa},
    {"lightslategray", 0x778899},
    {"lightslategrey", 0x778899},
    {"lightsteelblue", 0xb0c4de},
    {"lightyellow", 0xffffe0},
    {"lime", 0x00ff00}, /* ugh */
    {"limegreen", 0x32cd32},
    {"linen", 0xfaf0e6},
    {"magenta", 0xff00ff},
    {"maroon", 0x800000},
    {"mediumaquamarine", 0x66cdaa},
    {"mediumblue", 0x0000cd},
    {"mediumorchid", 0xba55d3},
    {"mediumpurple", 0x9370db},
    {"mediumseagreen", 0x3cb371},
    {"mediumslateblue", 0x7b68ee},
    {"mediumspringgreen", 0x00fa9a},
    {"mediumturquoise", 0x48d1cc},
    {"mediumvioletred", 0xc71585},
    {"midnightblue", 0x191970},
    {"mintcream", 0xf5fffa},
    {"mistyrose", 0xffe4e1},
    {"moccasin", 0xffe4b5},
    {"navajowhite", 0xffdead},
    {"navy", 0x000080},
    {"oldlace", 0xfdf5e6},
    {"olive", 0x808000},
    {"olivedrab", 0x6b8e23},
    {"orange", 0xffa500},
    {"orangered", 0xff4500},
    {"orchid", 0xda70d6},
    {"palegoldenrod", 0xeee8aa},
    {"palegreen", 0x98fb98},
    {"paleturquoise", 0xafeeee},
    {"palevioletred", 0xdb7093},
    {"papayawhip", 0xffefd5},
    {"peachpuff", 0xffdab9},
    {"peru", 0xcd853f},
    {"pink", 0xffc0cb},
    {"plum", 0xdda0dd},
    {"powderblue", 0xb0e0e6},
    {"purple", 0x800080},
    {"rebeccapurple", 0x663399},
    {"red", 0xff0000},
    {"rosybrown", 0xbc8f8f},
    {"royalblue", 0x4169e1},
    {"saddlebrown", 0x8b4513},
    {"salmon", 0xfa8072},
    {"sandybrown", 0xf4a460},
    {"seagreen", 0x2e8b57},
    {"seashell", 0xfff5ee},
    {"sienna", 0xa0522d},
    {"silver", 0xc0c0c0},
    {"skyblue", 0x87ceeb},
    {"slateblue", 0x6a5acd},
    {"slategray", 0x708090},
    {"slategrey", 0x708090},
    {"snow", 0xfffafa},
    {"springgreen", 0x00ff7f},
    {"steelblue", 0x4682b4},
    {"tan", 0xd2b48c},
    {"teal", 0x008080},
    {"thistle", 0xd8bfd8},
    {"tomato", 0xff6347},
    {"turquoise", 0x40e0d0},
    {"violet", 0xee82ee},
    {"wheat", 0xf5deb3},
    {"white", 0xffffff},
    {"whitesmoke", 0xf5f5f5},
    {"yellow", 0xffff00},
    {"yellowgreen", 0x9acd32}
};

static int
_bu_color_in_unit(double value)
{
    return std::isfinite(value) && value >= 0.0 && value <= 1.0;
}


static double
_bu_color_hue(double hue)
{
    hue = fmod(hue, 360.0);
    return (hue < 0.0) ? hue + 360.0 : hue;
}


static void
_bu_color_hsl_to_rgb(const double hsl[4], double rgb[4])
{
    double h = _bu_color_hue(hsl[0]) / 360.0;
    double s = hsl[1];
    double l = hsl[2];

    if (s == 0.0) {
	rgb[0] = rgb[1] = rgb[2] = l;
    } else {
	double q = (l < 0.5) ? l * (1.0 + s) : l + s - l * s;
	double p = 2.0 * l - q;
	double t[3] = {h + 1.0 / 3.0, h, h - 1.0 / 3.0};
	int i;

	for (i = 0; i < 3; i++) {
	    if (t[i] < 0.0)
		t[i] += 1.0;
	    if (t[i] > 1.0)
		t[i] -= 1.0;
	    if (t[i] < 1.0 / 6.0)
		rgb[i] = p + (q - p) * 6.0 * t[i];
	    else if (t[i] < 0.5)
		rgb[i] = q;
	    else if (t[i] < 2.0 / 3.0)
		rgb[i] = p + (q - p) * (2.0 / 3.0 - t[i]) * 6.0;
	    else
		rgb[i] = p;
	}
    }
    rgb[3] = hsl[3];
}


static void
_bu_color_hsv_to_rgb(const double hsv[4], double rgb[4])
{
    double h = _bu_color_hue(hsv[0]) / 60.0;
    double s = hsv[1];
    double v = hsv[2];
    long sector = (long)floor(h);
    double fraction = h - sector;
    double p = v * (1.0 - s);
    double q = v * (1.0 - fraction * s);
    double t = v * (1.0 - (1.0 - fraction) * s);

    switch (sector % 6) {
	case 0: rgb[0] = v; rgb[1] = t; rgb[2] = p; break;
	case 1: rgb[0] = q; rgb[1] = v; rgb[2] = p; break;
	case 2: rgb[0] = p; rgb[1] = v; rgb[2] = t; break;
	case 3: rgb[0] = p; rgb[1] = q; rgb[2] = v; break;
	case 4: rgb[0] = t; rgb[1] = p; rgb[2] = v; break;
	default: rgb[0] = v; rgb[1] = p; rgb[2] = q; break;
    }
    rgb[3] = hsv[3];
}


static void
_bu_color_rgb_to_hsl(const double rgb[4], double hsl[4])
{
    double max = FMAX(rgb[0], FMAX(rgb[1], rgb[2]));
    double min = FMIN(rgb[0], FMIN(rgb[1], rgb[2]));
    double delta = max - min;

    hsl[0] = 0.0;
    hsl[1] = 0.0;
    hsl[2] = (max + min) / 2.0;
    hsl[3] = rgb[3];

    if (delta == 0.0)
	return;

    hsl[1] = (hsl[2] > 0.5) ? delta / (2.0 - max - min) : delta / (max + min);
    if (rgb[0] >= rgb[1] && rgb[0] >= rgb[2])
	hsl[0] = (rgb[1] - rgb[2]) / delta + ((rgb[1] < rgb[2]) ? 6.0 : 0.0);
    else if (rgb[1] >= rgb[2])
	hsl[0] = (rgb[2] - rgb[0]) / delta + 2.0;
    else
	hsl[0] = (rgb[0] - rgb[1]) / delta + 4.0;
    hsl[0] *= 60.0;
}


static void
_bu_color_rgb_to_hsv(const double rgb[4], double hsv[4])
{
    double max = FMAX(rgb[0], FMAX(rgb[1], rgb[2]));
    double min = FMIN(rgb[0], FMIN(rgb[1], rgb[2]));
    double delta = max - min;

    hsv[0] = 0.0;
    hsv[1] = (max == 0.0) ? 0.0 : delta / max;
    hsv[2] = max;
    hsv[3] = rgb[3];

    if (delta == 0.0)
	return;

    if (rgb[0] >= rgb[1] && rgb[0] >= rgb[2])
	hsv[0] = (rgb[1] - rgb[2]) / delta + ((rgb[1] < rgb[2]) ? 6.0 : 0.0);
    else if (rgb[1] >= rgb[2])
	hsv[0] = (rgb[2] - rgb[0]) / delta + 2.0;
    else
	hsv[0] = (rgb[0] - rgb[1]) / delta + 4.0;
    hsv[0] *= 60.0;
}


int
bu_color_convert(const double in[4], bu_color_space_t in_space,
		 bu_color_space_t out_space, double out[4])
{
    double rgb[4];
    double result[4];

    if (UNLIKELY(!in || !out))
	return 0;
    if (in_space < BU_COLOR_SPACE_RGB || in_space > BU_COLOR_SPACE_HSV
	|| out_space < BU_COLOR_SPACE_RGB || out_space > BU_COLOR_SPACE_HSV)
	return 0;
    if (!std::isfinite(in[0]) || !_bu_color_in_unit(in[1])
	|| !_bu_color_in_unit(in[2]) || !_bu_color_in_unit(in[3]))
	return 0;
    if (in_space == BU_COLOR_SPACE_RGB && !_bu_color_in_unit(in[0]))
	return 0;

    switch (in_space) {
	case BU_COLOR_SPACE_RGB:
	    HMOVE(rgb, in);
	    break;
	case BU_COLOR_SPACE_HSL:
	    _bu_color_hsl_to_rgb(in, rgb);
	    break;
	case BU_COLOR_SPACE_HSV:
	    _bu_color_hsv_to_rgb(in, rgb);
	    break;
	default:
	    return 0;
    }

    switch (out_space) {
	case BU_COLOR_SPACE_RGB:
	    HMOVE(result, rgb);
	    break;
	case BU_COLOR_SPACE_HSL:
	    _bu_color_rgb_to_hsl(rgb, result);
	    break;
	case BU_COLOR_SPACE_HSV:
	    _bu_color_rgb_to_hsv(rgb, result);
	    break;
	default:
	    return 0;
    }

    HMOVE(out, result);
    return 1;
}

static int
_bu_hsv_to_float_rgb(fastf_t *rgb, const fastf_t *hsv)
{
    fastf_t float_rgb[3] = { 0.0, 0.0, 0.0 };
    fastf_t hue, sat, val;
    fastf_t hue_frac;
    fastf_t p, q, t;
    long int hue_int;


    if (!rgb || !hsv) {
	return -1;
    }

    hue = FMAX(hsv[HUE], 0.0);
    hue = FMIN(hue, 360.0);
    sat = FMAX(hsv[SAT], 0.0);
    sat = FMIN(sat, 1.0);
    val = FMAX(hsv[VAL], 0.0);
    val = FMIN(val, 1.0);

    if (NEAR_ZERO(sat, SMALL_FASTF)) {
	/* hue is achromatic, so just set constant value */
	VSETALL(float_rgb, val);
    } else {
	if (NEAR_ZERO(hue - 360.0, SMALL_FASTF))
	    hue = 0.0;
	hue /= 60.0;
	hue_int = lrint(floor((double)hue));
	hue_frac = hue - hue_int;
	p = val * (1.0 - sat);
	q = val * (1.0 - (sat * hue_frac));
	t = val * (1.0 - (sat * (1.0 - hue_frac)));
	switch (hue_int) {
	    case 0: VSET(float_rgb, val, t, p); break;
	    case 1: VSET(float_rgb, q, val, p); break;
	    case 2: VSET(float_rgb, p, val, t); break;
	    case 3: VSET(float_rgb, p, q, val); break;
	    case 4: VSET(float_rgb, t, p, val); break;
	    case 5: VSET(float_rgb, val, p, q); break;
	    default:
		    bu_log("%s:%d: This shouldn't happen\n", __FILE__, __LINE__);
		    bu_bomb("unexpected condition encountered in bu_hsv_to_rgb\n");
	}
    }

    rgb[0] = float_rgb[0];
    rgb[1] = float_rgb[1];
    rgb[2] = float_rgb[2];

    return 0;
}

int
bu_color_rand(struct bu_color *c, bu_color_rand_t type)
{
    if (!c) {
	return -1;
    }

    if (type == BU_COLOR_RANDOM) {
	// https://stackoverflow.com/q/21102105
	std::uniform_real_distribution<double> g_rand(0, 1);
	std::random_device rdev;
	std::default_random_engine engine(rdev());
	c->buc_rgb[RED] = (fastf_t)g_rand(engine);
	c->buc_rgb[GRN] = (fastf_t)g_rand(engine);
	c->buc_rgb[BLU] = (fastf_t)g_rand(engine);
	return 0;
    }

    if (type == BU_COLOR_RANDOM_LIGHTENED) {
	/* golden ratio */
	static fastf_t hsv[3] = { 0.0, 0.5, 0.95 };
	static double golden_ratio_conjugate = 0.618033988749895;
	std::uniform_real_distribution<double> g_rand(0, 1);
	std::random_device rdev;
	std::default_random_engine engine(rdev());
	fastf_t h = (fastf_t)g_rand(engine);
	h = fmod(h+golden_ratio_conjugate,1.0);
	*hsv = h * 360.0;

	fastf_t float_rgb[3] = { 0.0, 0.0, 0.0 };
	if (_bu_hsv_to_float_rgb((fastf_t *)float_rgb, hsv) < 0) {
	    return -1;
	}
	c->buc_rgb[RED] = float_rgb[RED];
	c->buc_rgb[GRN] = float_rgb[GRN];
	c->buc_rgb[BLU] = float_rgb[BLU];

	return 0;
    }

    return -1;
}

void
bu_rgb_to_hsv(const unsigned char *rgb, fastf_t *hsv)
{
    fastf_t red, grn, blu;
    fastf_t *hue = &hsv[HUE];
    fastf_t *sat = &hsv[SAT];
    fastf_t *val = &hsv[VAL];
    fastf_t max, min;
    fastf_t chroma;

    /*
     * Compute value
     */
    max = min = red = ((fastf_t)rgb[RED]) / 255.0;

    grn = ((fastf_t)rgb[GRN]) / 255.0;
    if (grn < min)
	min = grn;
    else if (grn > max)
	max = grn;

    blu = ((fastf_t)rgb[BLU]) / 255.0;
    if (blu < min)
	min = blu;
    else if (blu > max)
	max = blu;

    *val = max;

    /*
     * Compute saturation
     */
    chroma = max - min;
    if (max > 0.0)
	*sat = chroma / max;
    else
	*sat = 0.0;

    /*
     * Compute hue
     */
    if (NEAR_ZERO(*sat, SMALL_FASTF)) {
	*hue = 0.0; /* achromatic */
    } else {
	if (NEAR_ZERO(red - max, SMALL_FASTF))      /* red == max */
	    *hue = (grn - blu) / chroma;
	else if (NEAR_ZERO(grn - max, SMALL_FASTF)) /* grn == max */
	    *hue = 2.0 + (blu - red) / chroma;
	else if (NEAR_ZERO(blu - max, SMALL_FASTF)) /* blu == max */
	    *hue = 4.0 + (red - grn) / chroma;

	/*
	 * Convert hue to degrees
	 */
	*hue *= 60.0;
	if (*hue < 0.0)
	    *hue += 360.0;
    }
}


int
bu_hsv_to_rgb(const fastf_t *hsv, unsigned char *rgb)
{
    fastf_t float_rgb[3] = { 0.0, 0.0, 0.0 };

    if (_bu_hsv_to_float_rgb((fastf_t *)float_rgb, hsv) < 0) {
	return -1;
    }

    rgb[RED] = (unsigned char)lrint(float_rgb[RED] * 255.0);
    rgb[GRN] = (unsigned char)lrint(float_rgb[GRN] * 255.0);
    rgb[BLU] = (unsigned char)lrint(float_rgb[BLU] * 255.0);

    return 1;
}


int
bu_color_to_rgb_chars(const struct bu_color *cp, unsigned char *rgb)
{
    if (UNLIKELY(!cp || !rgb)) {
	return 0;
    }
    BU_ASSERT(!(cp->buc_rgb[RED] < 0.0 || cp->buc_rgb[GRN] < 0.0 || cp->buc_rgb[BLU] < 0.0));
    BU_ASSERT(!(cp->buc_rgb[RED] > 1.0 || cp->buc_rgb[GRN] > 1.0 || cp->buc_rgb[BLU] > 1.0));

    rgb[RED] = (unsigned char)lrint(cp->buc_rgb[RED] * 255.0);
    rgb[GRN] = (unsigned char)lrint(cp->buc_rgb[GRN] * 255.0);
    rgb[BLU] = (unsigned char)lrint(cp->buc_rgb[BLU] * 255.0);

    return 1;
}

int
bu_color_to_rgb_ints(const struct bu_color *cp, int *r, int *g, int *b)
{
    if (UNLIKELY(!cp || !r || !g || !b)) {
	return 0;
    }
    BU_ASSERT(!(cp->buc_rgb[RED] < 0.0 || cp->buc_rgb[GRN] < 0.0 || cp->buc_rgb[BLU] < 0.0));
    BU_ASSERT(!(cp->buc_rgb[RED] > 1.0 || cp->buc_rgb[GRN] > 1.0 || cp->buc_rgb[BLU] > 1.0));

    (*r) = (int)lrint(cp->buc_rgb[RED] * 255.0);
    (*g) = (int)lrint(cp->buc_rgb[GRN] * 255.0);
    (*b) = (int)lrint(cp->buc_rgb[BLU] * 255.0);

    return 1;
}

int
bu_color_from_rgb_chars(struct bu_color *cp, const unsigned char *rgb)
{
    if (UNLIKELY(!cp || !rgb)) {
	return 0;
    }

    cp->buc_rgb[RED] = (fastf_t)rgb[RED] / 255.0;
    cp->buc_rgb[GRN] = (fastf_t)rgb[GRN] / 255.0;
    cp->buc_rgb[BLU] = (fastf_t)rgb[BLU] / 255.0;

    return 1;
}


int
bu_color_to_rgb_floats(const struct bu_color *cp, fastf_t *rgb)
{
    if (UNLIKELY(!cp || !rgb)) {
	return 0;
    }
    BU_ASSERT(!(cp->buc_rgb[RED] < 0.0 || cp->buc_rgb[GRN] < 0.0 || cp->buc_rgb[BLU] < 0.0));
    BU_ASSERT(!(cp->buc_rgb[RED] > 1.0 || cp->buc_rgb[GRN] > 1.0 || cp->buc_rgb[BLU] > 1.0));

    VMOVE(rgb, cp->buc_rgb);

    return 1;
}


int
bu_color_from_rgb_floats(struct bu_color *cp, const fastf_t *rgb)
{
    if (UNLIKELY(!cp || !rgb)) {
	return 0;
    }
    if (rgb[RED] > 1.0 || rgb[GRN] > 1.0 || rgb[BLU] > 1.0)
	return 0;

    VMOVE(cp->buc_rgb, rgb);

    return 1;
}


struct bu_color_components {
    double value[4];
    int percent[4];
    int fractional[4];
    size_t count;
};


static int
_bu_color_components(const char *begin, const char *end,
		     struct bu_color_components *components)
{
    const char *p = begin;

    memset(components, 0, sizeof(*components));
    while (p < end) {
	char *number_end = NULL;
	const char *scan;
	size_t index;
	int separated = 0;

	while (p < end && (isspace((unsigned char)*p) || *p == ',' || *p == '/'))
	    p++;
	if (p == end)
	    break;
	if (components->count == 4)
	    return 0;

	index = components->count;
	errno = 0;
	components->value[index] = strtod(p, &number_end);
	if (number_end == p || errno == ERANGE || number_end > end
	    || !std::isfinite(components->value[index]))
	    return 0;
	for (scan = p; scan < number_end; scan++) {
	    if (*scan == '.' || *scan == 'e' || *scan == 'E') {
		components->fractional[index] = 1;
		break;
	    }
	}
	p = number_end;
	while (p < end && isspace((unsigned char)*p)) {
	    separated = 1;
	    p++;
	}
	if (p < end && *p == '%') {
	    components->percent[index] = 1;
	    p++;
	    while (p < end && isspace((unsigned char)*p)) {
		separated = 1;
		p++;
	    }
	}
	if (p < end && !separated && *p != ',' && *p != '/')
	    return 0;
	components->count++;
    }

    return components->count > 0;
}


static int
_bu_color_alpha(const struct bu_color_components *components, size_t index,
		double *alpha)
{
    double value = components->value[index];

    if (components->percent[index])
	value /= 100.0;
    if (!_bu_color_in_unit(value))
	return 0;
    *alpha = value;
    return 1;
}


static int
_bu_color_rgb_components(const struct bu_color_components *components,
			 size_t count, double rgb[4])
{
    int normalized;
    size_t i;

    if (components->count != count)
	return 0;
    normalized = components->fractional[0];
    for (i = 0; i < 3; i++) {
	double value = components->value[i];
	if (components->percent[i])
	    value /= 100.0;
	else if (!normalized)
	    value /= 255.0;
	if (!_bu_color_in_unit(value))
	    return 0;
	rgb[i] = value;
    }
    rgb[3] = 1.0;
    if (count == 4 && !_bu_color_alpha(components, 3, &rgb[3]))
	return 0;
    return 1;
}


static int
_bu_color_model_components(const struct bu_color_components *components,
			   size_t count, double model[4])
{
    size_t i;

    if (components->count != count)
	return 0;
    model[0] = components->value[0];
    if (components->percent[0])
	model[0] *= 3.6;
    for (i = 1; i < 3; i++) {
	model[i] = components->value[i];
	if (components->percent[i] || model[i] > 1.0)
	    model[i] /= 100.0;
	if (!_bu_color_in_unit(model[i]))
	    return 0;
    }
    model[3] = 1.0;
    if (count == 4 && !_bu_color_alpha(components, 3, &model[3]))
	return 0;
    return std::isfinite(model[0]);
}


static int
_bu_color_hex(const char *str, double rgba[4])
{
    size_t len = strlen(str);
    unsigned int value[4] = {0, 0, 0, 255};
    size_t count;
    size_t i;

    if (len == 3 || len == 4)
	count = len;
    else if (len == 6 || len == 8)
	count = len / 2;
    else
	return 0;

    for (i = 0; i < len; i++) {
	if (!isxdigit((unsigned char)str[i]))
	    return 0;
    }
    for (i = 0; i < count; i++) {
	char digits[3] = {0, 0, 0};
	if (len <= 4) {
	    digits[0] = digits[1] = str[i];
	} else {
	    digits[0] = str[i * 2];
	    digits[1] = str[i * 2 + 1];
	}
	if (sscanf(digits, "%x", &value[i]) != 1)
	    return 0;
    }
    for (i = 0; i < 4; i++)
	rgba[i] = (double)value[i] / 255.0;
    return 1;
}


static int
_bu_color_name(const char *str, double rgba[4])
{
    size_t i;

    if (BU_STR_EQUIV(str, "transparent")) {
	rgba[0] = rgba[1] = rgba[2] = rgba[3] = 0.0;
	return 1;
    }

    for (i = 0; i < sizeof(_bu_color_names) / sizeof(_bu_color_names[0]); i++) {
	if (BU_STR_EQUIV(str, _bu_color_names[i].name)) {
	    unsigned int value = _bu_color_names[i].rgb;
	    rgba[0] = (double)((value >> 16) & 0xff) / 255.0;
	    rgba[1] = (double)((value >> 8) & 0xff) / 255.0;
	    rgba[2] = (double)(value & 0xff) / 255.0;
	    rgba[3] = 1.0;
	    return 1;
	}
    }
    return 0;
}


static int
_bu_color_function(const char *str, double rgba[4])
{
    const char *left = strchr(str, '(');
    const char *right = str + strlen(str) - 1;
    struct bu_color_components components;
    char model[8] = {0};
    size_t model_len;
    double input[4];

    if (!left || right <= left || *right != ')')
	return 0;
    model_len = (size_t)(left - str);
    while (model_len > 0 && isspace((unsigned char)str[model_len - 1]))
	model_len--;
    if (model_len == 0 || model_len >= sizeof(model))
	return 0;
    memcpy(model, str, model_len);
    if (!_bu_color_components(left + 1, right, &components))
	return 0;

    if (BU_STR_EQUIV(model, "rgb"))
	return _bu_color_rgb_components(&components, 3, rgba);
    if (BU_STR_EQUIV(model, "rgba"))
	return _bu_color_rgb_components(&components, 4, rgba);
    if (BU_STR_EQUIV(model, "hsl") || BU_STR_EQUIV(model, "hsla")) {
	size_t count = BU_STR_EQUIV(model, "hsla") ? 4 : 3;
	if (!_bu_color_model_components(&components, count, input))
	    return 0;
	return bu_color_convert(input, BU_COLOR_SPACE_HSL, BU_COLOR_SPACE_RGB, rgba);
    }
    if (BU_STR_EQUIV(model, "hsv") || BU_STR_EQUIV(model, "hsva")) {
	size_t count = BU_STR_EQUIV(model, "hsva") ? 4 : 3;
	if (!_bu_color_model_components(&components, count, input))
	    return 0;
	return bu_color_convert(input, BU_COLOR_SPACE_HSV, BU_COLOR_SPACE_RGB, rgba);
    }
    return 0;
}


int
bu_color_parse(const char *str, struct bu_color *color)
{
    struct bu_vls trimmed = BU_VLS_INIT_ZERO;
    struct bu_color_components components;
    struct bu_color parsed = BU_COLOR_INIT_ZERO;
    double rgba[4] = {0.0, 0.0, 0.0, 1.0};
    const char *spec;
    int valid = 0;
    size_t i;

    if (UNLIKELY(!str || !color))
	return 0;

    bu_vls_strcpy(&trimmed, str);
    bu_vls_trimspace(&trimmed);
    spec = bu_vls_cstr(&trimmed);
    if (!spec[0])
	goto done;

    if (spec[0] == '#') {
	valid = _bu_color_hex(spec + 1, rgba);
    } else if (strchr(spec, '(')) {
	valid = _bu_color_function(spec, rgba);
    } else if (isdigit((unsigned char)spec[0]) || spec[0] == '.'
	       || spec[0] == '+' || spec[0] == '-') {
	valid = _bu_color_components(spec, spec + strlen(spec), &components)
	    && _bu_color_rgb_components(&components, 3, rgba);
    } else {
	valid = _bu_color_name(spec, rgba);
    }

    if (valid) {
	for (i = 0; i < 4; i++)
	    parsed.buc_rgb[i] = (fastf_t)rgba[i];
	HMOVE(color->buc_rgb, parsed.buc_rgb);
    }

done:
    bu_vls_free(&trimmed);
    return valid;
}


static unsigned int
_bu_color_byte(double value)
{
    return (unsigned int)lrint(value * 255.0);
}


static const char *
_bu_color_format_name(const double rgba[4])
{
    unsigned int value;
    size_t i;

    if (HZERO(rgba))
	return "transparent";
    if (!EQUAL(rgba[3], 1.0))
	return NULL;

    value = (_bu_color_byte(rgba[0]) << 16)
	| (_bu_color_byte(rgba[1]) << 8)
	| _bu_color_byte(rgba[2]);
    for (i = 0; i < sizeof(_bu_color_names) / sizeof(_bu_color_names[0]); i++) {
	if (_bu_color_names[i].rgb == value)
	    return _bu_color_names[i].name;
    }
    return NULL;
}


int
bu_color_format(const struct bu_color *color, bu_color_format_t format,
		struct bu_vls *output)
{
    struct bu_vls formatted = BU_VLS_INIT_ZERO;
    double rgb[4];
    double converted[4];
    unsigned int r, g, b, a;
    const char *name;
    int valid = 1;
    size_t i;

    if (UNLIKELY(!color || !output))
	return 0;
    for (i = 0; i < 4; i++) {
	rgb[i] = color->buc_rgb[i];
	if (!_bu_color_in_unit(rgb[i]))
	    return 0;
    }

    r = _bu_color_byte(rgb[0]);
    g = _bu_color_byte(rgb[1]);
    b = _bu_color_byte(rgb[2]);
    a = _bu_color_byte(rgb[3]);

    switch (format) {
	case BU_COLOR_FORMAT_RGB:
	    bu_vls_printf(&formatted, "rgb(%u, %u, %u)", r, g, b);
	    break;
	case BU_COLOR_FORMAT_RGBA:
	    bu_vls_printf(&formatted, "rgba(%u, %u, %u, %.15g)", r, g, b, rgb[3]);
	    break;
	case BU_COLOR_FORMAT_HEX:
	    bu_vls_printf(&formatted, "#%02x%02x%02x", r, g, b);
	    break;
	case BU_COLOR_FORMAT_HEXA:
	    bu_vls_printf(&formatted, "#%02x%02x%02x%02x", r, g, b, a);
	    break;
	case BU_COLOR_FORMAT_HSL:
	case BU_COLOR_FORMAT_HSLA:
	    if (!bu_color_convert(rgb, BU_COLOR_SPACE_RGB, BU_COLOR_SPACE_HSL, converted)) {
		valid = 0;
		break;
	    }
	    if (format == BU_COLOR_FORMAT_HSL)
		bu_vls_printf(&formatted, "hsl(%.15g, %.15g%%, %.15g%%)",
			      converted[0], converted[1] * 100.0, converted[2] * 100.0);
	    else
		bu_vls_printf(&formatted, "hsla(%.15g, %.15g%%, %.15g%%, %.15g)",
			      converted[0], converted[1] * 100.0,
			      converted[2] * 100.0, converted[3]);
	    break;
	case BU_COLOR_FORMAT_HSV:
	case BU_COLOR_FORMAT_HSVA:
	    if (!bu_color_convert(rgb, BU_COLOR_SPACE_RGB, BU_COLOR_SPACE_HSV, converted)) {
		valid = 0;
		break;
	    }
	    if (format == BU_COLOR_FORMAT_HSV)
		bu_vls_printf(&formatted, "hsv(%.15g, %.15g%%, %.15g%%)",
			      converted[0], converted[1] * 100.0, converted[2] * 100.0);
	    else
		bu_vls_printf(&formatted, "hsva(%.15g, %.15g%%, %.15g%%, %.15g)",
			      converted[0], converted[1] * 100.0,
			      converted[2] * 100.0, converted[3]);
	    break;
	case BU_COLOR_FORMAT_NAME:
	    name = _bu_color_format_name(rgb);
	    if (name)
		bu_vls_strcat(&formatted, name);
	    else
		valid = 0;
	    break;
	default:
	    valid = 0;
	    break;
    }

    if (valid)
	bu_vls_vlscat(output, &formatted);
    bu_vls_free(&formatted);
    return valid;
}


int
bu_color_from_str(struct bu_color *color, const char *str)
{
    struct bu_color parsed = BU_COLOR_INIT_ZERO;

    if (!bu_color_parse(str, &parsed))
	return 0;

    /* The legacy function has never assigned the alpha channel. */
    VMOVE(color->buc_rgb, parsed.buc_rgb);

    return 1;
}


int
bu_str_to_rgb(const char *str, unsigned char *rgb)
{
    struct bu_color color = BU_COLOR_INIT_ZERO;

    if (!bu_color_from_str(&color, str))
	return 0;
    if (!bu_color_to_rgb_chars(&color, rgb))
	return 0;

    return 1;
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
