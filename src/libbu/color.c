/*                         C O L O R . C
 * BRL-CAD
 *
 * Copyright (c) 1997-2018 United States Government as represented by
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
#include "bio.h"

#include "bu/color.h"
#include "bu/log.h"
#include "bu/malloc.h"


#define COMMA ','


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
    fastf_t hue, sat, val;
    fastf_t hue_frac;
    fastf_t p, q, t;
    long int hue_int;

    hue = FMAX(hsv[HUE], 0.0);
    hue = FMIN(hsv[HUE], 360.0);
    sat = FMAX(hsv[SAT], 0.0);
    sat = FMIN(hsv[SAT], 1.0);
    val = FMAX(hsv[VAL], 0.0);
    val = FMIN(hsv[VAL], 1.0);

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
		bu_log("%s:%d: This shouldn't happen\n",
		       __FILE__, __LINE__);
		bu_bomb("unexpected condition encountered in bu_hsv_to_rgb\n");
	}
    }

    rgb[RED] = (unsigned char)lrint(float_rgb[RED] * 255.0);
    rgb[GRN] = (unsigned char)lrint(float_rgb[GRN] * 255.0);
    rgb[BLU] = (unsigned char)lrint(float_rgb[BLU] * 255.0);

    return 1;
}


int
bu_str_to_rgb(const char *str, unsigned char *rgb)
{
    int num;
    unsigned int r = 0;
    unsigned int g = 0;
    unsigned int b = 0;

    if (UNLIKELY(!str || !rgb)) {
	return 0;
    }

    while (isspace((int)(*str)))
	++str;

    if (*str == '#') {
	if (strlen(++str) != 6)
	    return 0;
	sscanf(str, "%02x%02x%02x", (unsigned int *)&r, (unsigned int *)&g, (unsigned int *)&b);
    } else if (isdigit((int)(*str))) {
	num = sscanf(str, "%u/%u/%u", &r, &g, &b);
	if (num == 1) {
	    num = sscanf(str, "%u %u %u", &r, &g, &b);
	    if (num != 3)
		return 0;
	}
	if (r > 255)
	    r = 255;
	if (g > 255)
	    g = 255;
	if (b > 255)
	    b = 255;
    } else {
	return 0;
    }

    VSET(rgb, (fastf_t)r, (fastf_t)g, (fastf_t)b);

    return 1;
}


int
bu_color_to_rgb_chars(const struct bu_color *cp, unsigned char *rgb)
{
    unsigned int r, g, b;
    if (UNLIKELY(!cp || !rgb)) {
	return 0;
    }
    r = (unsigned int)cp->buc_rgb[RED];
    g = (unsigned int)cp->buc_rgb[GRN];
    b = (unsigned int)cp->buc_rgb[BLU];

    rgb[0] = (unsigned char)r;
    rgb[1] = (unsigned char)g;
    rgb[2] = (unsigned char)b;

    return 1;
}


int
bu_color_from_rgb_chars(struct bu_color *cp, const unsigned char *rgb)
{
    unsigned int r, g, b;
    if (UNLIKELY(!cp || !rgb)) {
	return 0;
    }

    r = (unsigned int)rgb[RED];
    g = (unsigned int)rgb[GRN];
    b = (unsigned int)rgb[BLU];


    cp->buc_rgb[RED] = (fastf_t)r;
    cp->buc_rgb[GRN] = (fastf_t)g;
    cp->buc_rgb[BLU] = (fastf_t)b;

    return 1;
}


int
bu_color_to_rgb_floats(const struct bu_color *cp, fastf_t *rgb)
{
    if (UNLIKELY(!cp || !rgb)) {
	return 0;
    }

    rgb[0] = cp->buc_rgb[RED];
    rgb[1] = cp->buc_rgb[GRN];
    rgb[2] = cp->buc_rgb[BLU];

    return 1;
}


int
bu_color_from_rgb_floats(struct bu_color *cp, const fastf_t *rgb)
{
    if (UNLIKELY(!cp || !rgb)) {
	return 0;
    }

    cp->buc_rgb[RED] = rgb[0];
    cp->buc_rgb[GRN] = rgb[1];
    cp->buc_rgb[BLU] = rgb[2];

    return 1;
}


int
bu_color_from_str(struct bu_color *color, const char *str)
{
    size_t i;
    char separator = '\0';
    int mode = 0;

    BU_COLOR_INIT(color);

    /* determine the format - 0 = RGB, 1 = FLOAT, 2 = UNKNOWN */
    for (mode = 0; mode <= 2; ++mode) {
	const char * const allowed_separators = "/" CPP_XSTR(COMMA);
	const char *endptr;
	float result;

	errno = 0;

	switch (mode) {
	    case 0: /*RGB*/
		result = strtol(str, (char **)&endptr, 10);
		break;

	    case 1: /*FLOAT*/
		result = strtod(str, (char **)&endptr);
		break;

	    default:
		return 0;
	}

	if (!((NEAR_ZERO(result, 0.0) && errno) || endptr == str
	      || !strchr(allowed_separators, *endptr))) {
	    separator = *endptr;
	    break;
	}
    }

    /* 0 = RGB, 1 = FLOAT, 2 = UNKNOWN */
    for (i = 0; i < 3; ++i) {
	const char expected_char = i == 2 ? '\0' : separator;
	const char *endptr;

	errno = 0;

	switch (mode) {
	    case 0: /*RGB*/
		color->buc_rgb[i] = strtol(str, (char **)&endptr, 10) / 255.0;
		break;

	    case 1: /*FLOAT*/
		color->buc_rgb[i] = strtod(str, (char **)&endptr);
		break;

	    default: /*UNKNOWN*/
		bu_bomb("error");
	}

	if ((NEAR_ZERO(color->buc_rgb[i], 0.0) && errno) || endptr == str || *endptr != expected_char
	    || !(0.0 <= color->buc_rgb[i] && color->buc_rgb[i] <= 1.0)) {
	    VSETALL(color->buc_rgb, 0.0);
	    return 0;
	}

	str = endptr + 1;
    }

    return 1;
}


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
