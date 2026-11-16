/*                      E N C O D I N G . C
 * BRL-CAD
 *
 * Copyright (c) 2013-2026 United States Government as represented by
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
/** @file encoding.c
 *
 * Contains encoding conversion supports for various file formats.
 *
 */

#include "icv.h"
#include "vmath.h"
#include "bu/magic.h"
#include "bu/malloc.h"
#include "bn.h"

double *
icv_uchar2double(unsigned char *data, size_t size)
{
    double *double_data, *double_p;
    unsigned char *char_p;

    if (size == 0)
	return NULL;

    char_p = data;
    double_p = double_data = (double *) bu_malloc(size*sizeof(double), "uchar2data : double data");

    while (size--) {
	*double_p = ICV_CONV_8BIT(*char_p);
	double_p++;
	char_p++;
    }

    return double_data;
}


/* Clamp before integer conversion: valid HDR samples may exceed lrint's
 * range. Display formats map NaN to black and infinities to their limits. */
static double
icv_display_sample(double value)
{
    if (isnan(value) || value < 0.0)
	return 0.0;
    return value > 1.0 ? 1.0 : value;
}


unsigned char *
icv_data2uchar(const icv_image_t *bif)
{
    size_t size;
    unsigned char *uchar_data, *char_p;
    double *double_p;

    if (!bif) {
	return NULL;
    }

    ICV_IMAGE_VAL_PTR(bif);

    size = bif->height*bif->width*bif->channels;
    char_p = uchar_data = (unsigned char *) bu_malloc((size_t)size, "data2uchar : unsigned char data");

    if (!char_p) {
	return NULL;
    }

    double_p = bif->data;

    if (ZERO(bif->gamma_corr)) {
	while (size--) {
	    *char_p = (unsigned char)lrint(icv_display_sample(*double_p)*255.0);

	    char_p++;
	    double_p++;
	}

    } else {
	float *rand_p = NULL;
	double ex = 1.0/bif->gamma_corr;
	bn_rand_init(rand_p, 0);

	while (size--) {
	    double val = icv_display_sample(*double_p);

	    long longval = lrint(pow(val, ex)*255.0 + (double) bn_rand0to1(rand_p));
	    if (longval > 255)
		*char_p = 255;
	    else if (longval < 0)
		*char_p = 0;
	    else
		*char_p = (unsigned char)longval;

	    char_p++;
	    double_p++;
	}
    }

    return uchar_data;
}


/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
