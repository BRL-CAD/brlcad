/*                    O P E R A T I O N S . C
 * BRL-CAD
 *
 * Copyright (c) 2013-2023 United States Government as represented by
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

#include "icv.h"

#include "bio.h"
#include "bu/log.h"
#include "bu/magic.h"
#include "bu/malloc.h"
#include "bn/tol.h"
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
    size_t size;

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

int
icv_diff(
	int *matching, int *off_by_1, int *off_by_many,
	icv_image_t *img1, icv_image_t *img2
	)
{
    if (!img1 || !img2)
	return -1;

    int ret = 0;

    // Have images
    unsigned char *d1 = icv_data2uchar(img1);
    unsigned char *d2 = icv_data2uchar(img2);
    size_t s1 = img1->width * img1->height;
    size_t s2 = img2->width * img2->height;
    size_t smin = (s1 < s2) ? s1 : s2;
    size_t smax = (s1 > s2) ? s1 : s2;
    for (size_t i = 0; i < smin; i++) {
	int r1 = d1[i*3+0];
	int g1 = d1[i*3+1];
	int b1 = d1[i*3+2];
	int r2 = d2[i*3+0];
	int g2 = d2[i*3+1];
	int b2 = d2[i*3+2];
	int dcnt = 0;
	dcnt += (r1 != r2) ? 1 : 0;
	dcnt += (g1 != g2) ? 1 : 0;
	dcnt += (b1 != b2) ? 1 : 0;
	switch (dcnt) {
	    case 0:
		if (matching)
		    (*matching)++;
		break;
	    case 1:
		ret = 1;
		if (off_by_1)
		    (*off_by_1)++;
		break;
	    default:
		ret = 1;
		if (off_by_many)
		    (*off_by_many)++;
	}
    }
    if (smin != smax) {
	ret = 1;
	if (off_by_many) {
	    (*off_by_many) += (int)(smax - smin);
	}
    }
    bu_free(d1, "image 1 rgb");
    bu_free(d2, "image 2 rgb");

    return ret;
}

int
icv_fit(icv_image_t *img, struct bu_vls *msg, size_t o_width_req, size_t o_height_req, fastf_t sf)
{
    if (!img)
	return BRLCAD_ERROR;

    size_t i_w, i_n;
    size_t o_w_used, o_n_used;
    size_t x_offset, y_offset;
    fastf_t ar_w;

    i_w = img->width;
    i_n = img->height;
    ar_w = o_width_req / (fastf_t)i_w;

    o_w_used = (size_t)(i_w * ar_w * sf);
    o_n_used = (size_t)(i_n * ar_w * sf);

    if (icv_resize(img, ICV_RESIZE_BINTERP, o_w_used, o_n_used, 1) < 0) {
	if (msg)
	    bu_vls_printf(msg, "icv_resize failed");
	return BRLCAD_ERROR;
    }

    if (NEAR_EQUAL(sf, 1.0, BN_TOL_DIST)) {
	fastf_t ar_n = o_height_req / (fastf_t)i_n;

	if (ar_w > ar_n && !NEAR_EQUAL(ar_w, ar_n, BN_TOL_DIST)) {
	    /* need to crop final image height so that we keep the center of the image */
	    size_t x_orig = 0;
	    size_t y_orig = (o_n_used - o_height_req) * 0.5;

	    if (icv_rect(img, x_orig, y_orig, o_width_req, o_height_req) < 0) {
		if (msg)
		    bu_vls_printf(msg, "icv_rect failed");
		return BRLCAD_ERROR;
	    }

	    x_offset = 0;
	    y_offset = 0;
	} else {
	    /* user needs to offset final image in the window */
	    x_offset = 0;
	    y_offset = (size_t)((o_height_req - o_n_used) * 0.5);
	}
    } else {
	if (sf > 1.0) {
	    size_t x_orig = (o_w_used - o_width_req) * 0.5;
	    size_t y_orig = (o_n_used - o_height_req) * 0.5;

	    if (icv_rect(img, x_orig, y_orig, o_width_req, o_height_req) < 0) {
		if (msg)
		    bu_vls_printf(msg, "icv_rect failed");
		return BRLCAD_ERROR;
	    }

	    x_offset = 0;
	    y_offset = 0;
	} else {
	    /* user needs to offset final image in the window */
	    x_offset = (size_t)((o_width_req - o_w_used) * 0.5);
	    y_offset = (size_t)((o_height_req - o_n_used) * 0.5);
	}
    }

    if (msg)
	bu_vls_printf(msg, "%zu %zu %zu %zu", img->width, img->height, x_offset, y_offset);

    return BRLCAD_OK;
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
