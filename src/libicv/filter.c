/*                        F I L T E R . C
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
/** @file libicv/filter.c
 *
 * This file contains routines for image filtering. This is done
 * mainly using the convolution of images. Both Gray Scale and RGB
 * images are taken care.
 */

#include "bu/log.h"
#include "bu/malloc.h"
#include "icv_private.h"

#include "vmath.h"

#define KERN_DEFAULT 3

/* private functions */

static int
get_kernel(ICV_FILTER filter_type, double *kern, double *offset)
{
    switch (filter_type) {
	case ICV_FILTER_LOW_PASS :
	    kern[0] = 3.0/42.0;
	    kern[1] = 5.0/42.0;
	    kern[2] = 3.0/42.0;
	    kern[3] = 5.0/42.0;
	    kern[4] = 10.0/42.0;
	    kern[5] = 5.0/42.0;
	    kern[6] = 3.0/42.0;
	    kern[7] = 5.0/42.0;
	    kern[8] = 3.0/42.0;
	    *offset = 0;
	    break;
	case ICV_FILTER_LAPLACIAN :
	    kern[0] = -1.0/16.0;
	    kern[1] = -1.0/16.0;
	    kern[2] = -1.0/16.0;
	    kern[3] = -1.0/16.0;
	    kern[4] = 8.0/16.0;
	    kern[5] = -1.0/16.0;
	    kern[6] = -1.0/16.0;
	    kern[7] = -1.0/16.0;
	    kern[8] = -1.0/16.0;
	    *offset = 0.5;
	    break;
	case ICV_FILTER_HORIZONTAL_GRAD :
	    kern[0] = 1.0/6.0;
	    kern[1] = 0;
	    kern[2] = -1.0/6.0;
	    kern[3] = 1.0/6.0;
	    kern[4] = 0;
	    kern[5] = -1.0/6.0;
	    kern[6] = 1.0/6.0;
	    kern[7] = 0;
	    kern[8] = -1.0/6.0;
	    *offset = 0.5;
	    break;
	case ICV_FILTER_VERTICAL_GRAD :
	    kern[0] = 1.0/6.0;
	    kern[1] = 1.0/6.0;
	    kern[2] = 1.0/6.0;
	    kern[3] = 0;
	    kern[4] = 0;
	    kern[5] = 0;
	    kern[6] = -1.0/6.0;
	    kern[7] = -1.0/6.0;
	    kern[8] = -1.0/6.0;
	    *offset = 0.5;
	    break;
	case ICV_FILTER_HIGH_PASS :
	    kern[0] = -1.0;
	    kern[1] = -2.0;
	    kern[2] = -1.0;
	    kern[3] = -2.0;
	    kern[4] = 13.0;
	    kern[5] = -2.0;
	    kern[6] = -1.0;
	    kern[7] = -2.0;
	    kern[8] = -1.0;
	    *offset = 0;
	    break;
	case ICV_FILTER_BOXCAR_AVERAGE :
	    kern[0] = 1.0/9;
	    kern[1] = 1.0/9;
	    kern[2] = 1.0/9;
	    kern[3] = 1.0/9;
	    kern[4] = 1.0/9;
	    kern[5] = 1.0/9;
	    kern[6] = 1.0/9;
	    kern[7] = 1.0/9;
	    kern[8] = 1.0/9;
	    *offset = 0;
	    break;
	case ICV_FILTER_NULL :
	    kern[0] = 0;
	    kern[1] = 0;
	    kern[2] = 0;
	    kern[3] = 0;
	    kern[4] = 1;
	    kern[5] = 0;
	    kern[6] = 0;
	    kern[7] = 0;
	    kern[8] = 0;
	    *offset = 0;
	    break;
	default :
	    bu_log("Filter Type not Implemented.\n");
	    return -1;
    }
    return 0;
}

static int
get_kernel3(ICV_FILTER3 filter_type, double *kern, double *offset)
{
    switch (filter_type) {
	case ICV_FILTER3_LOW_PASS :
	    kern[0] = 1.0/84;
	    kern[1] = 3.0/84;
	    kern[2] = 1.0/84;
	    kern[3] = 3.0/84;
	    kern[4] = 5.0/84;
	    kern[5] = 3.0/84;
	    kern[6] = 1.0/84;
	    kern[7] = 3.0/84;
	    kern[8] = 1.0/84;
	    kern[9] = 3.0/84;
	    kern[10] = 5.0/84;
	    kern[11] = 3.0/84;
	    kern[12] = 5.0/84;
	    kern[13] = 10.0/84;
	    kern[14] = 5.0/84;
	    kern[15] = 3.0/84;
	    kern[16] = 5.0/84;
	    kern[17] = 3.0/84;
	    kern[18] = 1.0/84;
	    kern[19] = 3.0/84;
	    kern[20] = 1.0/84;
	    kern[21] = 3.0/84;
	    kern[22] = 5.0/84;
	    kern[23] = 3.0/84;
	    kern[24] = 1.0/84;
	    kern[25] = 3.0/84;
	    kern[26] = 1.0/84;
	    *offset = 0;
	    break;
	case ICV_FILTER3_HIGH_PASS :
	    kern[0] = -1.0;
	    kern[1] = -2.0;
	    kern[2] = -1.0;
	    kern[3] = -2.0;
	    kern[4] = -4.0;
	    kern[5] = -2.0;
	    kern[6] = -1.0;
	    kern[7] = -2.0;
	    kern[8] = -1.0;
	    kern[9] = -2.0;
	    kern[10] = -4.0;
	    kern[11] = -2.0;
	    kern[12] = -4.0;
	    kern[13] = 56.0;
	    kern[14] = -4.0;
	    kern[15] = -2.0;
	    kern[16] = -4.0;
	    kern[17] = -2.0;
	    kern[18] = -1.0;
	    kern[19] = -2.0;
	    kern[20] = -1.0;
	    kern[21] = -2.0;
	    kern[22] = -4.0;
	    kern[23] = -2.0;
	    kern[24] = -1.0;
	    kern[25] = -2.0;
	    kern[26] = -1.0;
	    *offset = 0;
	    break;
	case ICV_FILTER3_BOXCAR_AVERAGE :
	    kern[0] = 1.0/53;
	    kern[1] = 1.0/53;
	    kern[2] = 1.0/53;
	    kern[3] = 1.0/53;
	    kern[4] = 1.0/53;
	    kern[5] = 1.0/53;
	    kern[6] = 1.0/53;
	    kern[7] = 1.0/53;
	    kern[8] = 1.0/53;
	    kern[9] = 1.0/53;
	    kern[10] = 1.0/53;
	    kern[11] = 1.0/53;
	    kern[12] = 1.0/53;
	    kern[13] = 27.0/53;
	    kern[14] = 1.0/53;
	    kern[15] = 1.0/53;
	    kern[16] = 1.0/53;
	    kern[17] = 1.0/53;
	    kern[18] = 1.0/53;
	    kern[19] = 1.0/53;
	    kern[20] = 1.0/53;
	    kern[21] = 1.0/53;
	    kern[22] = 1.0/53;
	    kern[23] = 1.0/53;
	    kern[24] = 1.0/53;
	    kern[25] = 1.0/53;
	    kern[26] = 1.0/53;
	    *offset = 0;
	    break;
	case ICV_FILTER3_ANIMATION_SMEAR :
	    kern[0] = 1.0/69;
	    kern[1] = 1.0/69;
	    kern[2] = 1.0/69;
	    kern[3] = 1.0/69;
	    kern[4] = 1.0/69;
	    kern[5] = 1.0/69;
	    kern[6] = 1.0/69;
	    kern[7] = 1.0/69;
	    kern[8] = 1.0/69;
	    kern[9] = 1.0/69;
	    kern[10] = 1.0/69;
	    kern[11] = 1.0/69;
	    kern[12] = 1.0/69;
	    kern[13] = 1.0/69;
	    kern[14] = 1.0/69;
	    kern[15] = 1.0/69;
	    kern[16] = 1.0/69;
	    kern[17] = 1.0/69;
	    kern[18] = 2.0/69;
	    kern[19] = 2.0/69;
	    kern[20] = 2.0/69;
	    kern[21] = 2.0/69;
	    kern[22] = 35.0/69;
	    kern[23] = 2.0/69;
	    kern[24] = 2.0/69;
	    kern[25] = 2.0/69;
	    kern[26] = 2.0/69;
	    *offset = 0;
	    break;
	case ICV_FILTER3_NULL :
	    kern[0] = 0;
	    kern[1] = 0;
	    kern[2] = 0;
	    kern[3] = 0;
	    kern[4] = 0;
	    kern[5] = 0;
	    kern[6] = 0;
	    kern[7] = 0;
	    kern[8] = 0;
	    kern[9] = 0;
	    kern[10] = 0;
	    kern[11] = 0;
	    kern[12] = 0;
	    kern[13] = 1;
	    kern[14] = 0;
	    kern[15] = 0;
	    kern[16] = 0;
	    kern[17] = 0;
	    kern[18] = 0;
	    kern[19] = 0;
	    kern[20] = 0;
	    kern[21] = 0;
	    kern[22] = 0;
	    kern[23] = 0;
	    kern[24] = 0;
	    kern[25] = 0;
	    kern[26] = 0;
	    *offset = 0;
	    break;
	default :
	    bu_log("Filter Type not Implemented.\n");
	    return -1;
    }
    return 0;
}

/* end of private functions */

static size_t
clamped_index(ptrdiff_t coord, size_t limit)
{
    if (coord < 0)
	return 0;
    if ((size_t)coord >= limit)
	return limit - 1;
    return (size_t)coord;
}

/* begin public functions */

int
icv_filter(icv_image_t *img, ICV_FILTER filter_type)
{
    double *kern = NULL;
    double *out_data, *in_data;
    double offset = 0;
    size_t k_dim = KERN_DEFAULT;
    size_t size;
    size_t y, x, c, ky, kx;

    /* TODO A new Functionality. Update the get_kernel function to
     * accommodate the generalized kernel length. This can be based
     * upon a library of filters or closed form definitions.
     */

    ICV_IMAGE_VAL_INT(img);

    kern = (double *)bu_malloc(k_dim*k_dim*sizeof(double), "icv_filter : Kernel Allocation");
    if (get_kernel(filter_type, kern, &offset) < 0) {
	bu_free(kern, "icv_filter : Kernel Allocation");
	return -1;
    }

    if (!kern)
	return -1;

    in_data = img->data;
    size = img->height*img->width*img->channels;

    if (size == 0) {
	bu_free(kern, "icv_filter : Kernel Allocation");
	return -1;
    }

    if (img->width > 0 && img->height > (size_t)-1 / img->width / img->channels / sizeof(double)) {
	bu_free(kern, "icv_filter : Kernel Allocation");
	return -1;
    }

    out_data = (double*)bu_malloc(size*sizeof(double), "icv_filter : out_image_data");

    /* Convolve in pixel coordinates and clamp border samples to the closest
     * valid edge pixel.  This avoids scalar row-wrap artifacts and keeps
     * normalized kernels, such as boxcar and low-pass, normalized on image
     * borders and on images smaller than the kernel. */
    for (y = 0; y < img->height; y++) {
	for (x = 0; x < img->width; x++) {
	    for (c = 0; c < img->channels; c++) {
		double c_val = 0.0;
		for (ky = 0; ky < k_dim; ky++) {
		    size_t sy = clamped_index((ptrdiff_t)y + (ptrdiff_t)ky - (ptrdiff_t)(k_dim / 2), img->height);
		    for (kx = 0; kx < k_dim; kx++) {
			size_t sx = clamped_index((ptrdiff_t)x + (ptrdiff_t)kx - (ptrdiff_t)(k_dim / 2), img->width);
			size_t in_idx = (sy * img->width + sx) * img->channels + c;
			c_val += kern[ky * k_dim + kx] * in_data[in_idx];
		    }
		}
		out_data[(y * img->width + x) * img->channels + c] = c_val + offset;
	    }
	}
    }
    bu_free(kern, "icv_filter : Kernel Allocation");
    icv_image_data_free(img, "icv:filter Input Image Data");
    icv_image_data_set_bu(img, out_data);
    return 0;
}

icv_image_t *
icv_filter3(icv_image_t *old_img, icv_image_t *curr_img, icv_image_t *new_img, ICV_FILTER3 filter_type)
{
    icv_image_t *out_img;
    double *kern = NULL;
    double *out_data;
    double *old_data, *curr_data, *new_data;
    double offset = 0;
    size_t k_dim = KERN_DEFAULT;
    size_t size;
    size_t y, x, c, ky, kx;

    ICV_IMAGE_VAL_PTR(old_img);
    ICV_IMAGE_VAL_PTR(curr_img);
    ICV_IMAGE_VAL_PTR(new_img);

    if (old_img->width != curr_img->width || curr_img->width != new_img->width || \
	old_img->height != curr_img->height || curr_img->height != new_img->height || \
	old_img->channels != curr_img->channels || curr_img->channels != new_img->channels) {
	bu_log("icv_filter3 : Image Parameters not Equal");
	return NULL;
    }
    if (old_img->color_space != curr_img->color_space || curr_img->color_space != new_img->color_space) {
	bu_log("icv_filter3 : Image Color Spaces not Equal");
	return NULL;
    }

    kern = (double *)bu_malloc(k_dim*k_dim*3*sizeof(double), "icv_filter3 : Kernel Allocation");
    if (get_kernel3(filter_type, kern, &offset) < 0) {
	bu_free(kern, "icv_filter3 : Kernel Allocation");
	return NULL;
    }

    if (!kern)
	return NULL;

    old_data = old_img->data;
    curr_data = curr_img->data;
    new_data = new_img->data;

    size = old_img->height*old_img->width*old_img->channels;
    if (size == 0) {
	bu_free(kern, "icv_filter3 : Kernel Allocation");
	return NULL;
    }

    out_img = icv_create_with_channels(curr_img->width, curr_img->height, curr_img->color_space, curr_img->channels);
    if (!out_img) {
	bu_free(kern, "icv_filter3 : Kernel Allocation");
	return NULL;
    }

    out_data = out_img->data;

    /* Temporal filtering uses the same edge policy as icv_filter for each
     * frame.  Kernels are laid out as old/current/new 3x3 planes. */
    for (y = 0; y < curr_img->height; y++) {
	for (x = 0; x < curr_img->width; x++) {
	    for (c = 0; c < curr_img->channels; c++) {
		double c_val = 0.0;
		for (ky = 0; ky < k_dim; ky++) {
		    size_t sy = clamped_index((ptrdiff_t)y + (ptrdiff_t)ky - (ptrdiff_t)(k_dim / 2), curr_img->height);
		    for (kx = 0; kx < k_dim; kx++) {
			size_t sx = clamped_index((ptrdiff_t)x + (ptrdiff_t)kx - (ptrdiff_t)(k_dim / 2), curr_img->width);
			size_t pidx = (sy * curr_img->width + sx) * curr_img->channels + c;
			size_t kidx = ky * k_dim + kx;
			c_val += kern[kidx] * old_data[pidx];
			c_val += kern[k_dim * k_dim + kidx] * curr_data[pidx];
			c_val += kern[2 * k_dim * k_dim + kidx] * new_data[pidx];
		    }
		}
		out_data[(y * curr_img->width + x) * curr_img->channels + c] = c_val + offset;
	    }
	}
    }
    bu_free(kern, "icv_filter3 : Kernel Allocation");
    return out_img;
}


int
icv_fade(icv_image_t *img, double fraction)
{
    size_t size;
    double *data;

    ICV_IMAGE_VAL_INT(img);

    size= img->height*img->width*img->channels;

    if (size == 0)
	return -1;

    if (fraction < 0.0 || fraction > 1.0) {
	bu_log("ERROR : Multiplier invalid. Image not Faded.");
	return -1;
    }

    data = img->data;

    while (size--) {
	*data = *data*fraction;
	if (*data > 1)
	    *data= 1.0;
	data++;
    }
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
