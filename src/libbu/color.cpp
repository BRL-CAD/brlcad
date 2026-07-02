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


/* named colors from the CSS Level 3 / SVG color keyword set.  This is
 * the same list resolved by the Tk and Qt user interfaces used
 * elsewhere in BRL-CAD, and is a superset of the historical X11
 * names.  Values are packed 8-bit 0xRRGGBB.
 */
static const struct {
    const char *name;
    unsigned int hex;
} _bu_color_names[] = {
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
    {"green", 0x008000},
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
    {"lime", 0x00ff00},
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
    {"yellowgreen", 0x9acd32},
    {NULL, 0}
};


/* Convert an HSL triple (hue in degrees, saturation and lightness in
 * [0,1]) to a floating point RGB triple in [0,1].
 */
static void
_bu_hsl_to_float_rgb(fastf_t *rgb, double h, double s, double l)
{
    double c, x, m;
    double r = 0.0, g = 0.0, b = 0.0;

    h = fmod(h, 360.0);
    if (h < 0.0)
	h += 360.0;
    s = FMAX(0.0, FMIN(1.0, s));
    l = FMAX(0.0, FMIN(1.0, l));

    c = (1.0 - fabs(2.0 * l - 1.0)) * s;
    x = c * (1.0 - fabs(fmod(h / 60.0, 2.0) - 1.0));
    m = l - c / 2.0;

    switch ((int)(h / 60.0) % 6) {
	case 0: r = c; g = x; b = 0.0; break;
	case 1: r = x; g = c; b = 0.0; break;
	case 2: r = 0.0; g = c; b = x; break;
	case 3: r = 0.0; g = x; b = c; break;
	case 4: r = x; g = 0.0; b = c; break;
	default: r = c; g = 0.0; b = x; break;
    }

    rgb[RED] = r + m;
    rgb[GRN] = g + m;
    rgb[BLU] = b + m;
}


/* Convert a CMYK quadruple in [0,1] to a floating point RGB triple in
 * [0,1].
 */
static void
_bu_cmyk_to_float_rgb(fastf_t *rgb, double c, double m, double y, double k)
{
    c = FMAX(0.0, FMIN(1.0, c));
    m = FMAX(0.0, FMIN(1.0, m));
    y = FMAX(0.0, FMIN(1.0, y));
    k = FMAX(0.0, FMIN(1.0, k));

    rgb[RED] = (1.0 - c) * (1.0 - k);
    rgb[GRN] = (1.0 - m) * (1.0 - k);
    rgb[BLU] = (1.0 - y) * (1.0 - k);
}


/* Split a channel list (separated by '/', ',', or whitespace) into up
 * to 'maxn' numeric values.  For each value, records whether it was
 * given as a percentage (trailing '%') and whether it contained a
 * decimal point.  Returns the count parsed, or -1 on a malformed token.
 */
static int
_bu_color_channels(const char *str, double *vals, int *pcts, int *dots, int maxn)
{
    int n = 0;
    size_t ac, i;
    char *cpy;
    char *p;
    char **av;

    cpy = bu_strdup(str);
    for (p = cpy; *p; p++) {
	if (*p == '/' || *p == ',' || *p == '\t')
	    *p = ' ';
    }

    av = (char **)bu_calloc((size_t)maxn + 1, sizeof(char *), "color channel argv");
    ac = bu_argv_from_string(av, (size_t)maxn, cpy);

    for (i = 0; i < ac; i++) {
	char *tok = av[i];
	char *end = NULL;
	double v;
	size_t len = strlen(tok);

	pcts[n] = 0;
	dots[n] = (strchr(tok, '.') != NULL);
	if (len > 0 && tok[len - 1] == '%') {
	    pcts[n] = 1;
	    tok[len - 1] = '\0';
	}

	errno = 0;
	v = strtod(tok, &end);
	if (end == tok || (end && *end != '\0')) {
	    n = -1;
	    break;
	}
	vals[n] = v;
	n++;
    }

    bu_free(av, "color channel argv");
    bu_free(cpy, "color channel copy");
    return n;
}


/* Normalize a channel expressed in the 0-255 / 0.0-1.0 / percentage
 * conventions (as used by rgb() and gray()) to a [0,1] fraction.
 */
static double
_bu_color_norm_rgb(double v, int pct, int dot)
{
    if (pct)
	return v / 100.0;
    if (dot)
	return v;
    return v / 255.0;
}


/* Normalize a channel expressed in the 0.0-1.0 / 0-100 / percentage
 * conventions (as used by saturation, value, lightness, and alpha) to
 * a [0,1] fraction.
 */
static double
_bu_color_norm_unit(double v, int pct)
{
    if (pct)
	return v / 100.0;
    if (v > 1.0)
	return v / 100.0;
    return v;
}


/* Parse hexadecimal notation (without the leading '#'):  3, 4, 6, or 8
 * hex digits, where the 4 and 8 digit forms also set alpha.
 */
static int
_bu_color_from_hex(struct bu_color *color, const char *str)
{
    size_t i, len = strlen(str);
    unsigned int chan[4] = {0, 0, 0, 255};
    int nchan;
    int shorthand;

    if (len == 3) {
	nchan = 3; shorthand = 1;
    } else if (len == 4) {
	nchan = 4; shorthand = 1;
    } else if (len == 6) {
	nchan = 3; shorthand = 0;
    } else if (len == 8) {
	nchan = 4; shorthand = 0;
    } else {
	return 0;
    }

    for (i = 0; i < len; i++) {
	if (!isxdigit((unsigned char)str[i]))
	    return 0;
    }

    for (i = 0; i < (size_t)nchan; i++) {
	char buf[3];
	unsigned int val = 0;
	if (shorthand) {
	    buf[0] = buf[1] = str[i];
	} else {
	    buf[0] = str[2 * i];
	    buf[1] = str[2 * i + 1];
	}
	buf[2] = '\0';
	if (sscanf(buf, "%x", &val) != 1)
	    return 0;
	chan[i] = val;
    }

    color->buc_rgb[RED] = (fastf_t)chan[RED] / 255.0;
    color->buc_rgb[GRN] = (fastf_t)chan[GRN] / 255.0;
    color->buc_rgb[BLU] = (fastf_t)chan[BLU] / 255.0;
    if (nchan == 4)
	color->buc_rgb[ALP] = (fastf_t)chan[3] / 255.0;

    return 1;
}


/* Look up a CSS/SVG named color (case-insensitive). */
static int
_bu_color_from_name(struct bu_color *color, const char *str)
{
    size_t i;

    if (BU_STR_EQUIV(str, "transparent")) {
	color->buc_rgb[RED] = 0.0;
	color->buc_rgb[GRN] = 0.0;
	color->buc_rgb[BLU] = 0.0;
	color->buc_rgb[ALP] = 0.0;
	return 1;
    }

    for (i = 0; _bu_color_names[i].name; i++) {
	if (BU_STR_EQUIV(str, _bu_color_names[i].name)) {
	    unsigned int hex = _bu_color_names[i].hex;
	    color->buc_rgb[RED] = (fastf_t)((hex >> 16) & 0xff) / 255.0;
	    color->buc_rgb[GRN] = (fastf_t)((hex >> 8) & 0xff) / 255.0;
	    color->buc_rgb[BLU] = (fastf_t)(hex & 0xff) / 255.0;
	    return 1;
	}
    }

    return 0;
}


/* Parse the legacy integer (0-255) or float (0.0-1.0) triplet forms,
 * preserving the historical behavior exactly.
 */
static int
_bu_color_from_triplet(struct bu_color *color, const char *str)
{
    size_t i;
    int mode;

    /* determine the format - 0 = RGB, 1 = FLOAT, 2 = UNKNOWN */
    for (mode = 0; mode < 3; ++mode) {
	const char * const allowed_separators = "/, ";
	const char *endptr;
	long lr = -1;
	double dr = -1.0;

	errno = 0;

	switch (mode) {
	    case 0: /* RGB */
		lr = strtol(str, (char **)&endptr, 10);
		break;
	    case 1: /* FLOAT */
		dr = strtod(str, (char **)&endptr);
		break;
	    default:
		return 0;
	}

	if ((mode == 0 && lr < 0) || (mode == 1 && dr < 0.0)) {
	    /* negative means string conversion failed */
	    return 0;
	}

	if (endptr != str && strchr(allowed_separators, *endptr)) {
	    break;
	}
    }

    /* iterate over RGB */
    for (i = 0; i < 3; ++i) {
	const char *endptr;

	errno = 0;

	switch (mode) {
	    case 0: /* RGB */
		color->buc_rgb[i] = strtol(str, (char **)&endptr, 10) / 255.0;
		break;
	    case 1: /* FLOAT */
		color->buc_rgb[i] = strtod(str, (char **)&endptr);
		break;
	    default:
		return 0;
	}

	if ((NEAR_ZERO(color->buc_rgb[i], 0.0) && errno)
	    || endptr == str
	    || (i != 2 && *endptr == '\0')
	    || !(color->buc_rgb[i] >= 0.0 && color->buc_rgb[i] <= 1.0))
	{
	    return 0;
	}

	str = endptr + 1;
    }

    return 1;
}


/* Parse functional notation "model(c0, c1, ...)" for the rgb, rgba,
 * hsv, hsl, cmyk, and gray/grey color models.
 */
static int
_bu_color_from_functional(struct bu_color *color, const char *str)
{
    const char *lp = strchr(str, '(');
    const char *rp = strrchr(str, ')');
    struct bu_vls model = BU_VLS_INIT_ZERO;
    double vals[5] = {0.0, 0.0, 0.0, 0.0, 0.0};
    int pcts[5] = {0, 0, 0, 0, 0};
    int dots[5] = {0, 0, 0, 0, 0};
    fastf_t rgb[3] = {0.0, 0.0, 0.0};
    const char *m;
    int n;
    int base = 0;
    int ret = 0;

    if (!lp || !rp || rp <= lp)
	return 0;

    bu_vls_strncpy(&model, str, (size_t)(lp - str));
    bu_vls_trimspace(&model);
    m = bu_vls_cstr(&model);

    {
	struct bu_vls args = BU_VLS_INIT_ZERO;
	bu_vls_strncpy(&args, lp + 1, (size_t)(rp - (lp + 1)));
	n = _bu_color_channels(bu_vls_cstr(&args), vals, pcts, dots, 5);
	bu_vls_free(&args);
    }

    if (n < 1) {
	bu_vls_free(&model);
	return 0;
    }

    if (BU_STR_EQUIV(m, "rgb") || BU_STR_EQUIV(m, "rgba")) {
	base = 3;
	if (n == 3 || n == 4) {
	    rgb[RED] = _bu_color_norm_rgb(vals[0], pcts[0], dots[0]);
	    rgb[GRN] = _bu_color_norm_rgb(vals[1], pcts[1], dots[1]);
	    rgb[BLU] = _bu_color_norm_rgb(vals[2], pcts[2], dots[2]);
	    ret = 1;
	}
    } else if (BU_STR_EQUIV(m, "hsv") || BU_STR_EQUIV(m, "hsva")
	       || BU_STR_EQUIV(m, "hsb")) {
	base = 3;
	if (n == 3 || n == 4) {
	    fastf_t hsv[3];
	    hsv[HUE] = vals[0];
	    hsv[SAT] = _bu_color_norm_unit(vals[1], pcts[1]);
	    hsv[VAL] = _bu_color_norm_unit(vals[2], pcts[2]);
	    if (_bu_hsv_to_float_rgb(rgb, hsv) == 0)
		ret = 1;
	}
    } else if (BU_STR_EQUIV(m, "hsl") || BU_STR_EQUIV(m, "hsla")) {
	base = 3;
	if (n == 3 || n == 4) {
	    _bu_hsl_to_float_rgb(rgb, vals[0],
				 _bu_color_norm_unit(vals[1], pcts[1]),
				 _bu_color_norm_unit(vals[2], pcts[2]));
	    ret = 1;
	}
    } else if (BU_STR_EQUIV(m, "cmyk")) {
	base = 4;
	if (n == 4 || n == 5) {
	    _bu_cmyk_to_float_rgb(rgb,
				  _bu_color_norm_unit(vals[0], pcts[0]),
				  _bu_color_norm_unit(vals[1], pcts[1]),
				  _bu_color_norm_unit(vals[2], pcts[2]),
				  _bu_color_norm_unit(vals[3], pcts[3]));
	    ret = 1;
	}
    } else if (BU_STR_EQUIV(m, "gray") || BU_STR_EQUIV(m, "grey")) {
	base = 1;
	if (n == 1 || n == 2) {
	    fastf_t v = _bu_color_norm_rgb(vals[0], pcts[0], dots[0]);
	    rgb[RED] = v;
	    rgb[GRN] = v;
	    rgb[BLU] = v;
	    ret = 1;
	}
    }

    bu_vls_free(&model);

    if (!ret)
	return 0;

    /* reject out-of-gamut results to protect downstream consumers */
    if (rgb[RED] < 0.0 || rgb[GRN] < 0.0 || rgb[BLU] < 0.0
	|| rgb[RED] > 1.0 || rgb[GRN] > 1.0 || rgb[BLU] > 1.0) {
	return 0;
    }

    color->buc_rgb[RED] = rgb[RED];
    color->buc_rgb[GRN] = rgb[GRN];
    color->buc_rgb[BLU] = rgb[BLU];

    /* a single trailing channel beyond the model's base is alpha */
    if (n == base + 1) {
	double a = _bu_color_norm_unit(vals[n - 1], pcts[n - 1]);
	if (a < 0.0 || a > 1.0)
	    return 0;
	color->buc_rgb[ALP] = a;
    }

    return 1;
}


int
bu_color_from_str(struct bu_color *color, const char *str)
{
    struct bu_color newcolor = BU_COLOR_INIT_ZERO;
    struct bu_vls s = BU_VLS_INIT_ZERO;
    const char *c;
    int ret = 0;

    if (UNLIKELY(!color || !str)) {
	return 0;
    }

    /* work on a whitespace-trimmed copy */
    bu_vls_strcpy(&s, str);
    bu_vls_trimspace(&s);
    c = bu_vls_cstr(&s);
    if (strlen(c) == 0) {
	bu_vls_free(&s);
	return 0;
    }

    /* Alpha starts "unset" (negative) so it is only written when a
     * specification actually supplies one, preserving the historical
     * behavior of leaving the caller's alpha channel untouched.
     */
    newcolor.buc_rgb[ALP] = -1.0;

    if (*c == '#') {
	ret = _bu_color_from_hex(&newcolor, c + 1);
    } else if (strchr(c, '(')) {
	ret = _bu_color_from_functional(&newcolor, c);
    } else if (isdigit((unsigned char)*c) || *c == '.' || *c == '+' || *c == '-') {
	ret = _bu_color_from_triplet(&newcolor, c);
    } else {
	ret = _bu_color_from_name(&newcolor, c);
    }

    if (ret) {
	color->buc_rgb[RED] = newcolor.buc_rgb[RED];
	color->buc_rgb[GRN] = newcolor.buc_rgb[GRN];
	color->buc_rgb[BLU] = newcolor.buc_rgb[BLU];
	if (newcolor.buc_rgb[ALP] >= 0.0)
	    color->buc_rgb[ALP] = newcolor.buc_rgb[ALP];
    }

    bu_vls_free(&s);
    return ret;
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


/* Clamp a floating point channel to [0,1] and quantize to 8-bit. */
static unsigned char
_bu_color_to_uchar(fastf_t v)
{
    if (v < 0.0)
	v = 0.0;
    if (v > 1.0)
	v = 1.0;
    return (unsigned char)lrint(v * 255.0);
}


/* Convert a floating point RGB triple in [0,1] to HSL (hue in degrees,
 * saturation and lightness in [0,1]).
 */
static void
_bu_float_rgb_to_hsl(const fastf_t *rgb, double *h, double *s, double *l)
{
    double r = rgb[RED], g = rgb[GRN], b = rgb[BLU];
    double mx = FMAX(r, FMAX(g, b));
    double mn = FMIN(r, FMIN(g, b));
    double chroma = mx - mn;
    double hh = 0.0;

    *l = (mx + mn) / 2.0;

    if (NEAR_ZERO(chroma, SMALL_FASTF)) {
	*h = 0.0;
	*s = 0.0;
	return;
    }

    *s = chroma / (1.0 - fabs(2.0 * (*l) - 1.0));

    if (NEAR_EQUAL(mx, r, SMALL_FASTF))
	hh = fmod((g - b) / chroma, 6.0);
    else if (NEAR_EQUAL(mx, g, SMALL_FASTF))
	hh = ((b - r) / chroma) + 2.0;
    else
	hh = ((r - g) / chroma) + 4.0;

    hh *= 60.0;
    if (hh < 0.0)
	hh += 360.0;
    *h = hh;
}


/* Convert a floating point RGB triple in [0,1] to CMYK (all in [0,1]). */
static void
_bu_float_rgb_to_cmyk(const fastf_t *rgb, double *c, double *m, double *y, double *k)
{
    double r = rgb[RED], g = rgb[GRN], b = rgb[BLU];
    double mx = FMAX(r, FMAX(g, b));

    *k = 1.0 - mx;
    if (NEAR_ZERO(mx, SMALL_FASTF)) {
	*c = 0.0;
	*m = 0.0;
	*y = 0.0;
	return;
    }
    *c = (mx - r) / mx;
    *m = (mx - g) / mx;
    *y = (mx - b) / mx;
}


int
bu_color_to_str(struct bu_vls *str, const struct bu_color *color, const char *format)
{
    unsigned char r, g, b, a;

    if (UNLIKELY(!str || !color)) {
	return 0;
    }

    r = _bu_color_to_uchar(color->buc_rgb[RED]);
    g = _bu_color_to_uchar(color->buc_rgb[GRN]);
    b = _bu_color_to_uchar(color->buc_rgb[BLU]);
    a = _bu_color_to_uchar(color->buc_rgb[ALP]);

    if (!format || format[0] == '\0' || BU_STR_EQUIV(format, "rgb")) {
	bu_vls_printf(str, "%d/%d/%d", (int)r, (int)g, (int)b);
    } else if (BU_STR_EQUIV(format, "rgba")) {
	bu_vls_printf(str, "%d/%d/%d/%d", (int)r, (int)g, (int)b, (int)a);
    } else if (BU_STR_EQUIV(format, "hex")) {
	bu_vls_printf(str, "#%02x%02x%02x", r, g, b);
    } else if (BU_STR_EQUIV(format, "hexa")) {
	bu_vls_printf(str, "#%02x%02x%02x%02x", r, g, b, a);
    } else if (BU_STR_EQUIV(format, "float")) {
	bu_vls_printf(str, "%g/%g/%g", color->buc_rgb[RED],
		      color->buc_rgb[GRN], color->buc_rgb[BLU]);
    } else if (BU_STR_EQUIV(format, "hsv") || BU_STR_EQUIV(format, "hsb")) {
	fastf_t hsv[3];
	unsigned char rgbc[3];
	rgbc[RED] = r;
	rgbc[GRN] = g;
	rgbc[BLU] = b;
	bu_rgb_to_hsv(rgbc, hsv);
	bu_vls_printf(str, "hsv(%g,%g,%g)", hsv[HUE], hsv[SAT], hsv[VAL]);
    } else if (BU_STR_EQUIV(format, "hsl")) {
	double h, s, l;
	_bu_float_rgb_to_hsl(color->buc_rgb, &h, &s, &l);
	bu_vls_printf(str, "hsl(%g,%g,%g)", h, s, l);
    } else if (BU_STR_EQUIV(format, "cmyk")) {
	double cc, mm, yy, kk;
	_bu_float_rgb_to_cmyk(color->buc_rgb, &cc, &mm, &yy, &kk);
	bu_vls_printf(str, "cmyk(%g,%g,%g,%g)", cc, mm, yy, kk);
    } else {
	return 0;
    }

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
