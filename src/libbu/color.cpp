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


static const char *
bu_rgb_skip_space(const char *str)
{
    while (str && *str && isspace((unsigned char)*str))
	str++;
    return str;
}


static int
bu_rgb_parse_channel(const char **str, unsigned char *channel)
{
    char *end = NULL;
    long value;
    const char *start;

    if (!str || !*str || !channel)
	return 0;
    start = bu_rgb_skip_space(*str);
    if (!start || !*start)
	return 0;
    errno = 0;
    value = strtol(start, &end, 10);
    if (end == start || errno == ERANGE || value < 0 || value > 255)
	return 0;
    *channel = (unsigned char)value;
    *str = end;
    return 1;
}


static int
bu_rgb_parse_packed(unsigned char *rgb, const char *arg)
{
    const char *str = arg;
    const char *after_red = NULL;
    unsigned char parsed[3] = {0, 0, 0};
    char separator = '\0';

    if (!rgb || !arg || !bu_rgb_parse_channel(&str, &parsed[RED]))
	return 0;
    after_red = str;
    str = bu_rgb_skip_space(str);

    if (*str == '/' || *str == ',' || *str == ';') {
	separator = *str++;
	if (!bu_rgb_parse_channel(&str, &parsed[GRN]))
	    return 0;
	str = bu_rgb_skip_space(str);
	if (*str != separator)
	    return 0;
	str++;
	if (!bu_rgb_parse_channel(&str, &parsed[BLU]))
	    return 0;
    } else {
	/* The whitespace-only form needs a real separator after each channel. */
	if (after_red == str)
	    return 0;
	if (!bu_rgb_parse_channel(&str, &parsed[GRN]))
	    return 0;
	after_red = str;
	str = bu_rgb_skip_space(str);
	if (after_red == str)
	    return 0;
	if (!bu_rgb_parse_channel(&str, &parsed[BLU]))
	    return 0;
    }

    if (*bu_rgb_skip_space(str) != '\0')
	return 0;
    VMOVE(rgb, parsed);
    return 1;
}


int
bu_rgb_channel_validate(struct bu_vls *msg, const char *arg)
{
    const char *str = arg;
    unsigned char channel = 0;

    if (bu_rgb_parse_channel(&str, &channel) && *bu_rgb_skip_space(str) == '\0')
	return 0;
    if (msg)
	bu_vls_printf(msg, "RGB channel must be a base-ten integer from 0 through 255");
    return -1;
}


int
bu_rgb_from_argv(unsigned char *rgb, size_t argc, const char * const *argv)
{
    unsigned char parsed[3] = {0, 0, 0};

    if (!rgb || !argv || !argc || !argv[0])
	return 0;
    if (bu_rgb_parse_packed(parsed, argv[0])) {
	VMOVE(rgb, parsed);
	return 1;
    }
    if (argc < 3 || !argv[1] || !argv[2])
	return 0;
    for (size_t i = 0; i < 3; i++) {
	const char *str = argv[i];
	if (!bu_rgb_parse_channel(&str, &parsed[i]) || *bu_rgb_skip_space(str) != '\0')
	    return 0;
    }
    VMOVE(rgb, parsed);
    return 3;
}


int
bu_color_from_str(struct bu_color *color, const char *str)
{
    struct bu_color newcolor = BU_COLOR_INIT_ZERO;
    const char *argv[1] = {str};
    unsigned char rgb[3] = {0, 0, 0};
    size_t i;
    int mode = 0;

    if (UNLIKELY(!color || !str)) {
	return 0;
    }

    /* skip past any leading whitespace */
    while (isspace(*str))
	str++;

    /* hexadecimal color in #FFF or #FFFFFF form */
    if (*str == '#') {
	int ret = 0;
	size_t len;
	unsigned int hex_rgb[3] = {0, 0, 0};

	str++;

	len = strlen(str);
	if (len == 3) {
	    ret = sscanf(str, "%01x%01x%01x", &hex_rgb[RED], &hex_rgb[GRN], &hex_rgb[BLU]);
	    hex_rgb[RED] += hex_rgb[RED] * 16;
	    hex_rgb[GRN] += hex_rgb[GRN] * 16;
	    hex_rgb[BLU] += hex_rgb[BLU] * 16;
	} else if (len == 6) {
	    ret = sscanf(str, "%02x%02x%02x", &hex_rgb[RED], &hex_rgb[GRN], &hex_rgb[BLU]);
	}
	if (ret != 3) {
	    return 0;
	}
	if (hex_rgb[RED] > 255 || hex_rgb[GRN] > 255 || hex_rgb[BLU] > 255)	{
	    return 0;
	}

	color->buc_rgb[RED] = (fastf_t)hex_rgb[RED] / 255.0;
	color->buc_rgb[GRN] = (fastf_t)hex_rgb[GRN] / 255.0;
	color->buc_rgb[BLU] = (fastf_t)hex_rgb[BLU] / 255.0;

	return 1;
    }

    /* Integer RGB triples have one strict, shared grammar.  Keep the
     * historical floating-point form below for callers that need it. */
    if (bu_rgb_from_argv(rgb, 1, argv))
	return bu_color_from_rgb_chars(color, rgb);

    /* determine the format - 0 = RGB, 1 = FLOAT, 2 = UNKNOWN */
    for (mode = 0; mode < 3; ++mode) {
	const char * const allowed_separators = "/,; ";
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

    /* Integer triples are handled exclusively by bu_rgb_from_argv above,
     * which enforces one delimiter grammar and complete consumption. */
    if (mode == 0)
	return 0;

    /* iterate over RGB */
    for (i = 0; i < 3; ++i) {
	const char *endptr;

	errno = 0;

	/* 0 = RGB, 1 = FLOAT, 2 = UNKNOWN */
	switch (mode) {
	    case 0: /*RGB*/
		newcolor.buc_rgb[i] = strtol(str, (char **)&endptr, 10) / 255.0;
		break;

	    case 1: /*FLOAT*/
		newcolor.buc_rgb[i] = strtod(str, (char **)&endptr);
		break;

	    default: /*UNKNOWN*/
		bu_bomb("error");
	}

	if ((NEAR_ZERO(newcolor.buc_rgb[i], 0.0) && errno)
	    || endptr == str
	    || (i != 2 && *endptr == '\0')
	    || (i == 2 && *bu_rgb_skip_space(endptr) != '\0')
	    || !(newcolor.buc_rgb[i] >= 0.0 && newcolor.buc_rgb[i] <= 1.0))
	{
	    return 0;
	}

	str = endptr + 1;
    }
    VMOVE(color->buc_rgb, newcolor.buc_rgb);

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
