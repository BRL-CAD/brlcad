/*                   C O L O R _ S P A C E . C
 * BRL-CAD
 *
 * Copyright (c) 2013-2014 United States Government as represented by
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
/** @file libicv/color_space.c
 *
 * This file contains color_space conversion routines.
 *
 */
#include "common.h"

#include <stdlib.h>
#include <string.h>

#include "vmath.h"
#include "bio.h"
#include "bu.h"
#include "icv.h"

int
icv_gray2rgb(icv_image_t *img)
{
    double *out_data, *op;
    double *in_data;
    long int size;
    long int i = 0;

    ICV_IMAGE_VAL_INT(img);

    /* This is a true condition i.e the image is already RGB*/
    if (img->color_space == ICV_COLOR_SPACE_RGB) {
	return 0;
    }
    else if (img->color_space != ICV_COLOR_SPACE_GRAY) {
	bu_log("ERROR : color_space error");
	return -1;
    }

    size = img->height*img->width;
    op = out_data = (double *)bu_malloc(size*3*sizeof(double), "Out Image Data");
    in_data = img->data;
    for (i =0 ; i < size; i++) {
	*(out_data) = *in_data;
	*(out_data+1) = *in_data;
	*(out_data+2) = *in_data;
	out_data+=3;
	in_data++;
    }

    bu_free(img->data, "icv_gray2rgb : gray image data");
    img->data = op;
    img->color_space = ICV_COLOR_SPACE_RGB;
    img->channels = 3;

    return 0;
}

int
icv_rgb2gray(icv_image_t *img, ICV_COLOR color, double rweight, double gweight, double bweight)
{
    double *out_data, *in_data;
    size_t in, out, size;
    int multiple_colors = 0;
    int num_color_planes = 3;

    double value;
    int red, green, blue;
    red = green = blue = 0;

    ICV_IMAGE_VAL_INT(img);

    /* This is a true condition i.e the image is already GRAY*/
    if (img->color_space == ICV_COLOR_SPACE_GRAY)
	return 0;
    else if (img->color_space != ICV_COLOR_SPACE_RGB) {
	bu_log("ERROR : color_space error");
	return -1;
    }

    switch (color) {
	case ICV_COLOR_R :
	    red = 1;
	    bweight = 0;
	    gweight = 0;
	    multiple_colors = 0;
	    break;
	case ICV_COLOR_G :
	    green = 1;
	    rweight = 0;
	    bweight = 0;
	    multiple_colors = 0;
	    break;
	case ICV_COLOR_B :
	    blue = 1;
	    rweight = 0;
	    gweight = 0;
	    multiple_colors = 0;
	    break;
	case ICV_COLOR_RG :
	    red = 1;
	    green = 1;
	    bweight = 0;
	    multiple_colors = 1;
	    break;
	case ICV_COLOR_RB :
	    blue = 1;
	    red = 1;
	    gweight = 0;
	    multiple_colors = 1;
	    break;
	case ICV_COLOR_BG :
	    blue = 1;
	    green = 1;
	    rweight = 0;
	    multiple_colors = 1;
	    break;
	case ICV_COLOR_RGB :
	    red = 1;
	    green = 1;
	    blue = 1;
	    multiple_colors = 1;
	    break;
	default :
	    bu_log("ERROR: Wrong Arguments for Color");
	    return -1;
    }

    /* Gets number of planes according to the status of arguments
       check */
    num_color_planes = red + green + blue;
    in_data = img->data;


    /* If function is called with zero for weight of respective plane
       then divide the weight equally among all the planes */
    if (red != 0 && ZERO(rweight))
	rweight = 1.0 / (double)num_color_planes;
    if (green != 0 && ZERO(gweight))
	gweight = 1.0 / (double)num_color_planes;
    if (blue != 0 && ZERO(bweight))
	bweight = 1.0 / (double)num_color_planes;

    size = img->height*img->width;
    out_data = (double*) bu_malloc(size*sizeof(double), "Out Image Data");
    if (multiple_colors) {
	for (in = out = 0; out < size; out++, in += 3) {
	    value = rweight*in_data[in] + gweight*in_data[in+1] + bweight*in_data[in+2];
	    if (value > 1.0) {
		out_data[out] = 1.0;
	    } else if (value < 0.0) {
		out_data[out] = 0.0;
	    } else
		out_data[out] = value;
	}
    } else if (red) {
	for (in = out = 0; out < size; out++, in += 3)
	    out_data[out] = in_data[in];
    } else if (green) {
	for (in = out = 0; out < size; out++, in += 3)
	    out_data[out] = in_data[in+1];
    } else if (blue) {
	for (in = out = 0; out < size; out++, in += 3)
	    out_data[out] = in_data[in+2];
    } else {
	/* uniform weight */
	for (in = out = 0; out < size; out++, in += 3)
	    out_data[out] = (in_data[in] + in_data[in+1] + in_data[in+2]) / 3.0;
    }
    bu_free(img->data, "icv_image_rgb2gray : rgb image data");
    img->data = out_data;
    img->color_space = ICV_COLOR_SPACE_GRAY;
    img->channels = 1;

    return 0;
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
