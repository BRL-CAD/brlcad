/*                    O P E R A T I O N S . C
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
/** @file libicv/operations.c
 *
 * This file contains routines for operations.
 *
 */

#include "common.h"

#include <math.h>

#include "bu.h"
#include "icv.h"

#include "bio.h"
#include "vmath.h"


int icv_sanitize(icv_image_t* img)
{
    double *data = NULL;
    size_t size;

    ICV_IMAGE_VAL_INT(img);

    data= img->data;
    for (size = img->width*img->height*img->channels; size>0; size--) {
	if (*data>1.0)
	    *data = 1.0;
	else if (*data<0)
	    *data = 0;
	data++;
    }
    img->flags |= ICV_SANITIZED;
    return 0;
}

int icv_add_val(icv_image_t* img, double val)
{
    double *data = NULL;
    size_t size;

    data = img->data;

    ICV_IMAGE_VAL_INT(img);

    for (size = img->width*img->height*img->channels; size>0; size--) {
	*data += val;
	data++;
    }

    if (img->flags & ICV_OPERATIONS_MODE)
	img->flags&=(!ICV_SANITIZED);
    else
	icv_sanitize(img);

    return 0;
}

int icv_multiply_val(icv_image_t* img, double val)
{
    double *data = NULL;
    size_t size;

    ICV_IMAGE_VAL_INT(img);

    data = img->data;

    for (size = img->width*img->height*img->channels; size>0; size--) {
	*data *= val;
	 data++;
    }
    if ((img->flags & ICV_OPERATIONS_MODE))
	img->flags&=(!ICV_SANITIZED);
    else
	icv_sanitize(img);

    return 0;
}

int icv_divide_val(icv_image_t* img, double val)
{
    double *data = NULL;
    size_t size;

    ICV_IMAGE_VAL_INT(img);

    data = img->data;

    /* Since data is double dividing by 0 will result in INF and -INF */

    for (size = img->width*img->height*img->channels; size>0; size--) {
	*data /= val;
	 data++;
     }

    if ((img->flags & ICV_OPERATIONS_MODE))
	img->flags&=(!ICV_SANITIZED);
    else
	icv_sanitize(img);

    return 0;
}

int icv_pow_val(icv_image_t* img, double val)
{
    double *data = NULL;
    size_t size;

    ICV_IMAGE_VAL_INT(img);

    data = img->data;

    for (size = img->width*img->height*img->channels; size>0; size--) {
	*data = pow(*data,val);
	 data++;
    }

    if ((img->flags & ICV_OPERATIONS_MODE))
	img->flags&=(!ICV_SANITIZED);
    else
	icv_sanitize(img);

    return 0;
}

icv_image_t *icv_add(icv_image_t *img1, icv_image_t *img2)
{
    double *data1, *data2, *out_data;
    size_t size;
    icv_image_t *out_img;

    ICV_IMAGE_VAL_PTR(img1);
    ICV_IMAGE_VAL_PTR(img2);

    if ((img1->width != img2->width) || (img1->height != img2->height) || (img1->channels != img2->channels)) {
	bu_log("icv_add : Image Parameters not Equal");
	return NULL;
    }

    data1 =img1->data;
    data2 =img2->data;

    out_img = icv_create(img1->width, img1->height, img1->color_space);

    out_data = out_img->data;

    for (size = img1->width*img1->height*img1->channels; size>0; size--)
	*out_data++ = *data1++ + *data2++;

    icv_sanitize(out_img);

    return out_img;
}

icv_image_t *icv_sub(icv_image_t *img1, icv_image_t *img2)
{
    double *data1, *data2, *out_data;
    size_t size;
    icv_image_t *out_img;

    ICV_IMAGE_VAL_PTR(img1);
    ICV_IMAGE_VAL_PTR(img2);

    if ((img1->width != img2->width) || (img1->height != img2->height) || (img1->channels != img2->channels)) {
	bu_log("icv_add : Image Parameters not Equal");
	return NULL;
    }

    data1 =img1->data;
    data2 =img2->data;

    out_img = icv_create(img1->width, img1->height, img1->color_space);

    out_data = out_img->data;

    for (size = img1->width*img1->height*img1->channels; size>0; size--)
	*out_data++ = *data1++ - *data2++;

    icv_sanitize(out_img);

    return out_img;
}

icv_image_t *icv_multiply(icv_image_t *img1, icv_image_t *img2)
{
    double *data1, *data2, *out_data;
    size_t size;
    icv_image_t *out_img;

    ICV_IMAGE_VAL_PTR(img1);
    ICV_IMAGE_VAL_PTR(img2);

    if ((img1->width != img2->width) || (img1->height != img2->height) || (img1->channels != img2->channels)) {
	bu_log("icv_add : Image Parameters not Equal");
	return NULL;
    }

    data1 =img1->data;
    data2 =img2->data;

    out_img = icv_create(img1->width, img1->height, img1->color_space);

    out_data = out_img->data;

    for (size = img1->width*img1->height*img1->channels; size>0; size--)
	*out_data++ = *data1++ * *data2++;

    icv_sanitize(out_img);

    return out_img;
}


icv_image_t *icv_divide(icv_image_t *img1, icv_image_t *img2)
{
    double *data1, *data2, *out_data;
    size_t size;
    icv_image_t *out_img;

    ICV_IMAGE_VAL_PTR(img1);
    ICV_IMAGE_VAL_PTR(img2);

    if ((img1->width != img2->width) || (img1->height != img2->height) || (img1->channels != img2->channels)) {
	bu_log("icv_add : Image Parameters not Equal");
	return NULL;
    }

    data1 =img1->data;
    data2 =img2->data;

    out_img = icv_create(img1->width, img1->height, img1->color_space);

    out_data = out_img->data;

    for (size = img1->width*img1->height*img1->channels; size>0; size--)
	*out_data++ = *data1++ / (*data2++ + VDIVIDE_TOL);

    icv_sanitize(out_img);

    return out_img;
}

int icv_saturate(icv_image_t* img, double sat)
{
    double *data;
    double bw;			/* monochrome intensity */
    double rwgt, gwgt, bwgt;
    double rt, gt, bt;
    long size;

    ICV_IMAGE_VAL_INT(img);

    if (img == NULL) {
	bu_log("icv_saturate : Trying to Saturate a Null img");
	return -1;
    }

    if (img->color_space != ICV_COLOR_SPACE_RGB) {
	bu_log("icv_saturate : Saturates only RGB Images");
	return -1;
    }

    data = img->data;
    size = img->width*img->height;
    rwgt = 0.31*(1.0-sat);
    gwgt = 0.61*(1.0-sat);
    bwgt = 0.08*(1.0-sat);
    while (size-- > 0) {
	rt = *data;
	gt = *(data+1);
	bt = *(data+2);
	bw = (rwgt*rt + gwgt*gt + bwgt*bt);
	rt = bw + sat*rt;
	gt = bw + sat*gt;
	bt = bw + sat*bt;
	*data++ = rt;
	*data++ = gt;
	*data++ = bt;
    }
    icv_sanitize(img);
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
