/*                        F I L T E R . C
 * BRL-CAD
 *
 * Copyright (c) 2013 United States Government as represented by
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

#include "bu.h"
#include "icv.h"

#include "vmath.h"

#define KERN_DEFAULT 3

/* private functions */

HIDDEN void
get_kernel(ICV_FILTER filter_type, double *kern, double *offset)
{
    switch (filter_type) {
	case ICV_FILTER_LOW_PASS :
	    kern[0] = 3.0/42.0; kern[1] = 5.0/42.0; kern[2] = 3.0/42.0;
	    kern[3] = 5.0/42.0; kern[4] = 10.0/42.0; kern[5] = 5.0/42.0;
	    kern[6] = 3.0/42.0; kern[7] = 5.0/42.0; kern[8] = 3.0/42.0;
	    *offset = 0;
	    break;
	case ICV_FILTER_LAPLACIAN :
	    kern[0] = -1.0/16.0; kern[1] = -1.0/16.0; kern[2] = -1.0/16.0;
	    kern[3] = -1.0/16.0; kern[4] = 8.0/16.0; kern[5] = -1.0/16.0;
	    kern[6] = -1.0/16.0; kern[7] = -1.0/16.0; kern[8] = -1.0/16.0;
	    *offset = 0.5;
	    break;
	case ICV_FILTER_HORIZONTAL_GRAD :
	    kern[0] = 1.0/6.0; kern[1] = 0; kern[2] = -1.0/6.0;
	    kern[3] = 1.0/6.0; kern[4] = 0; kern[5] = -1.0/6.0;
	    kern[6] = 1.0/6.0; kern[7] = 0; kern[8] = -1.0/6.0;
	    *offset = 0.5;
	    break;
	case ICV_FILTER_VERTICAL_GRAD :
	    kern[0] = 1.0/6.0; kern[1] = 1.0/6.0; kern[2] = 1.0/6.0;
	    kern[3] = 0; kern[4] = 0; kern[5] = 0;
	    kern[6] = -1.0/6.0; kern[7] = -1.0/6.0; kern[8] = -1.0/6.0;
	    *offset = 0.5;
	    break;
	case ICV_FILTER_HIGH_PASS :
	    kern[0] = -1.0; kern[1] = -2.0; kern[2] = -1.0;
	    kern[3] = -2.0; kern[4] = 13.0; kern[5] = -2.0;
	    kern[6] = -1.0; kern[7] = -2.0; kern[8] = -1.0;
	    *offset = 0;
	    break;
	case ICV_FILTER_BOXCAR_AVERAGE :
	    kern[0] = 1.0/9; kern[1] = 1.0/9; kern[2] = 1.0/9;
	    kern[3] = 1.0/9; kern[4] = 1.0/9; kern[5] = 1.0/9;
	    kern[6] = 1.0/9; kern[7] = 1.0/9; kern[8] = 1.0/9;
	    *offset = 0;
	    break;
	case ICV_FILTER_NULL :
	    kern[0] = 0; kern[1] = 0; kern[2] = 0;
	    kern[3] = 0; kern[4] = 0; kern[5] = 0;
	    kern[6] = 0; kern[7] = 0; kern[8] = 0;
	    *offset = 0;
	    break;
	default :
	    bu_log("Filter Type not Implemented.\n");
	    bu_free(kern, "Freeing Kernel, Wrong filter");
	    kern = NULL;
    }
    return;
}

HIDDEN void
get_kernel3(ICV_FILTER3 filter_type, double *kern, double *offset)
{
    switch (filter_type) {
	case ICV_FILTER3_LOW_PASS :
	    kern[0] = 1.0/84; kern[1] = 3.0/84; kern[2] = 1.0/84;
	    kern[3] = 3.0/84; kern[4] = 5.0/84; kern[5] = 3.0/84;
	    kern[6] = 1.0/84; kern[7] = 3.0/84; kern[8] = 1.0/84;
	    kern[9] = 3.0/84; kern[10] = 5.0/84; kern[11] = 3.0/84;
	    kern[12] = 5.0/84; kern[13] = 10.0/84; kern[14] = 5.0/84;
	    kern[15] = 3.0/84; kern[16] = 5.0/84; kern[17] = 3.0/84;
	    kern[18] = 1.0/84; kern[19] = 3.0/84; kern[20] = 1.0/84;
	    kern[21] = 3.0/84; kern[22] = 5.0/84; kern[23] = 3.0/84;
	    kern[24] = 1.0/84; kern[25] = 3.0/84; kern[26] = 1.0/84;
	    *offset = 0;
	    break;
	case ICV_FILTER3_HIGH_PASS :
	    kern[0] = -1.0; kern[1] = -2.0; kern[2] = -1.0;
	    kern[3] = -2.0; kern[4] = -4.0; kern[5] = -2.0;
	    kern[6] = -1.0; kern[7] = -2.0; kern[8] = -1.0;
	    kern[9] = -2.0; kern[10] = -4.0; kern[11] = -2.0;
	    kern[12] = -4.0; kern[13] = 56.0; kern[14] = -4.0;
	    kern[15] = -2.0; kern[16] = -4.0; kern[17] = -2.0;
	    kern[18] = -1.0; kern[19] = -2.0; kern[20] = -1.0;
	    kern[21] = -2.0; kern[22] = -4.0; kern[23] = -2.0;
	    kern[24] = -1.0; kern[25] = -2.0; kern[26] = -1.0;
	    *offset = 0;
	    break;
	case ICV_FILTER3_BOXCAR_AVERAGE :
	    kern[0] = 1.0/53; kern[1] = 1.0/53; kern[2] = 1.0/53;
	    kern[3] = 1.0/53; kern[4] = 1.0/53; kern[5] = 1.0/53;
	    kern[6] = 1.0/53; kern[7] = 1.0/53; kern[8] = 1.0/53;
	    kern[9] = 1.0/53; kern[10] = 1.0/53; kern[11] = 1.0/53;
	    kern[12] = 1.0/53; kern[13] = 27.0/53; kern[14] = 1.0/53;
	    kern[15] = 1.0/53; kern[16] = 1.0/53; kern[17] = 1.0/53;
	    kern[18] = 1.0/53; kern[19] = 1.0/53; kern[20] = 1.0/53;
	    kern[21] = 1.0/53; kern[22] = 1.0/53; kern[23] = 1.0/53;
	    kern[24] = 1.0/53; kern[25] = 1.0/53; kern[26] = 1.0/53;
	    *offset = 0;
	    break;
	case ICV_FILTER3_ANIMATION_SMEAR :
	    kern[0] = 1.0/69; kern[1] = 1.0/69; kern[2] = 1.0/69;
	    kern[3] = 1.0/69; kern[4] = 1.0/69; kern[5] = 1.0/69;
	    kern[6] = 1.0/69; kern[7] = 1.0/69; kern[8] = 1.0/69;
	    kern[9] = 1.0/69; kern[10] = 1.0/69; kern[11] = 1.0/69;
	    kern[12] = 1.0/69; kern[13] = 1.0/69; kern[14] = 1.0/69;
	    kern[15] = 1.0/69; kern[16] = 1.0/69; kern[17] = 1.0/69;
	    kern[18] = 2.0/69; kern[19] = 2.0/69; kern[20] = 2.0/69;
	    kern[21] = 2.0/69; kern[22] = 35.0/69; kern[23] = 2.0/69;
	    kern[24] = 2.0/69; kern[25] = 2.0/69; kern[26] = 2.0/69;
	    *offset = 0;
	    break;
	case ICV_FILTER3_NULL :
	    kern[0] = 0; kern[1] = 0; kern[2] = 0;
	    kern[3] = 0; kern[4] = 0; kern[5] = 0;
	    kern[6] = 0; kern[7] = 0; kern[8] = 0;
	    kern[9] = 0; kern[10] = 0; kern[11] = 0;
	    kern[12] = 0; kern[13] = 0; kern[14] = 0;
	    kern[15] = 0; kern[16] = 0; kern[17] = 0;
	    kern[18] = 0; kern[19] = 0; kern[20] = 0;
	    kern[21] = 0; kern[22] = 0; kern[23] = 0;
	    kern[24] = 0; kern[25] = 0; kern[26] = 0;
	    *offset = 0;
	    break;
	default :
	    bu_log("Filter Type not Implemented.\n");
	    bu_free(kern, "Freeing Kernel, Wrong filter");
	    kern = NULL;
    }
    return;
}

/* end of private functions */

/* begin public functions */

int
icv_filter(icv_image_t *img, ICV_FILTER filter_type)
{
    double *kern=NULL, *kern_p=NULL;
    double c_val;
    double *out_data, *in_data, *data_p;
    double offset = 0;
    int k_dim = KERN_DEFAULT, k_dim_half, k_dim_half_ceil;
    long int size;
    long int h, w, k, i;
    long int widthstep;
    long int index, n_index; /**< index is the index of the pixel in
			      * out image and n_index corresponds to
			      * the nearby pixel in input image
			      */

    /* TODO A new Functionality. Update the get_kernel function to
     * accommodate the generalized kernel length. This can be based
     * upon a library of filters or closed form definitions.
     */

    ICV_IMAGE_VAL_INT(img);

    kern = (double *)bu_malloc(k_dim*k_dim*sizeof(double), "icv_filter : Kernel Allocation");
    get_kernel(filter_type, kern, &offset);

    if (!kern)
	return -1;

    widthstep = img->width*img->channels;

    in_data = img->data;
    size = img->height*img->width*img->channels;
    /* Replaces data pointer in place */
    img->data = out_data = (double*)bu_malloc(size*sizeof(double), "icv_filter : out_image_data");

    index = -1;
    /* Kernel Dimension is always considered to be odd*/
    k_dim_half = k_dim/2*img->channels;
    k_dim_half_ceil = (k_dim - k_dim/2)*img->channels;
    index = 0;
    for (h = 0; h < img->height; h++) {
	VMOVEN(out_data, in_data+index, k_dim_half);
	out_data+=k_dim_half;

	for (w = 0; w < widthstep - k_dim*img->channels; w++) {
	    c_val = 0;
	    kern_p = kern;

	    for (k = -k_dim/2; k<=k_dim/2; k++) {
		n_index = index + k*widthstep;
		data_p = in_data + n_index;
		for (i = 0; i<=k_dim; i++) {
		    /* Ensures that the arguments are given a zero value for
		     * out of bound pixels. Thus behaves similar to zero padding
		     */
		    if (n_index >= 0 && n_index < size) {
			c_val += (*kern_p++)*(*data_p);
			data_p += img->channels;
			/* Ensures out bound in image */
			n_index += img->channels;
		    }
		}
	    }
	    *out_data = c_val + offset;
	    out_data++;
	    index++;
	}
	index = (h+1)*widthstep;
	VMOVEN(out_data,in_data+index -k_dim_half_ceil*img->channels,k_dim_half_ceil*img->channels );
	out_data+= k_dim_half_ceil;
    }
    bu_free(in_data, "icv:filter Input Image Data");
    return 0;
}

icv_image_t *
icv_filter3(icv_image_t *old_img, icv_image_t *curr_img, icv_image_t *new_img, ICV_FILTER3 filter_type)
{
    icv_image_t *out_img;
    double *kern = NULL;
    double *kern_old, *kern_curr, *kern_new;
    double c_val;
    double *out_data;
    double *old_data, *curr_data, *new_data;
    double *old_data_p, *curr_data_p, *new_data_p;
    double offset = 0;
    int k_dim = KERN_DEFAULT;
    long int size;
    long int s, k, i;
    long int widthstep;
    long int index, n_index; /**< index is the index of the pixel in
			      * out image and n_index corresponds to
			      * the nearby pixel in input image
			      */

    ICV_IMAGE_VAL_PTR(old_img);
    ICV_IMAGE_VAL_PTR(curr_img);
    ICV_IMAGE_VAL_PTR(new_img);

    if ((old_img->width == curr_img->width && curr_img->width == new_img->width) && \
	(old_img->height == curr_img->height && curr_img->height == new_img->height) && \
	(old_img->channels == curr_img->channels && curr_img->channels == new_img->channels)) {
	bu_log("icv_filter3 : Image Parameters not Equal");
	return NULL;
    }

    kern = (double *)bu_malloc(k_dim*k_dim*3*sizeof(double), "icv_filter3 : Kernel Allocation");
    get_kernel3(filter_type, kern, &offset);

    if (!kern)
	return NULL;

    widthstep = old_img->width*old_img->channels;

    old_data = old_img->data;
    curr_data = curr_img->data;
    new_data = new_img->data;

    size = old_img->height*old_img->width*old_img->channels;

    out_img = icv_create(old_img->width, old_img->height, old_img->color_space);

    out_data = out_img->data;

    index = -1;

    for (s = 0; s <= size; s++) {
	index++;
	c_val = 0;
	kern_old = kern;
	kern_curr = kern + k_dim*k_dim-1;
	kern_new = kern + 2*k_dim*k_dim-1;
	for (k = -k_dim/2; k<=k_dim/2; k++) {
	    n_index = index + k*widthstep;
	    old_data_p = old_data + n_index;
	    curr_data_p = curr_data + n_index;
	    new_data_p = new_data + n_index;
	    for (i = 0; i<=k_dim; i++) {
		/* Ensures that the arguments are given a zero value for
		   out of bound pixels. Thus behaves similar to zero padding */
		if (n_index >= 0 && n_index < size) {
		    c_val += (*kern_old++)*(*old_data_p);
		    c_val += (*kern_curr++)*(*curr_data_p);
		    c_val += (*kern_new++)*(*new_data_p);
		    old_data_p += old_img->channels;
		    curr_data_p += old_img->channels;
		    new_data_p += old_img->channels;
		    n_index += old_img->channels;
		}
	    }
	}
	*out_data++ = c_val + offset;
    }
    return 0;
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

    if (fraction<0) {
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
