/*                          C R O P . C
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
/** @file libicv/crop.c
 *
 * This file contains functions for cropping images.
 * There are two types of cropping: rectangular and skeyed.
 *
 */

#include "common.h"

#include <math.h>
#include <stdio.h>

#include "bu/malloc.h"
#include "bu/exit.h"
#include "vmath.h"
#include "icv.h"


int
icv_rect(icv_image_t *img, size_t xorig, size_t yorig, size_t xnum, size_t ynum)
{
    size_t row;
    double *p, *in_data, *out_data;
    size_t widthstep_in, widthstep_out, bytes_row; /**<  */
    int errorflag;

    ICV_IMAGE_VAL_INT(img);

    errorflag=0;
    if (xnum < 1) {
	fprintf(stderr,"icv_rect : ERROR: Horizontal Cut Size\n");
	errorflag=1;
    }
    if (ynum < 1) {
	fprintf(stderr,"icv_rect : ERROR: Vertical Cut Size\n");
	errorflag=1;
    }
    if (errorflag) bu_exit(1,NULL);

    errorflag=0;
    if (xnum > img->width || xorig > img->width - xnum) {
	fprintf(stderr,"icv_rect : Cut not possible; input parameters exceed the width.\n");
	errorflag=1;
    }
    if (ynum > img->height || yorig > img->height - ynum) {
	fprintf(stderr,"icv_rect : Cut not possible; input parameters exceed the height.\n");
	errorflag=1;
    }
    if (errorflag) bu_exit(1,NULL);

    /* initialization of variables to insure cropping and copying */
    widthstep_in = img->width*img->channels;
    widthstep_out = xnum*img->channels;
    bytes_row = widthstep_out*sizeof(double);
    out_data = p = (double *)bu_malloc(ynum*bytes_row,"icv_rect : Cropped Image Data");

    in_data = img->data + yorig*widthstep_in + xorig*img->channels;

    for (row = 0; row < ynum ; row++) {
	VMOVEN(p,in_data,widthstep_out);
	in_data += widthstep_in;
	p += widthstep_out;
    }

    bu_free(img->data, "icv image input data");
    img->width = xnum;
    img->height = ynum;
    img->data = out_data;
    return 0;
}

int
icv_crop(icv_image_t *img, size_t ulx, size_t uly, size_t urx, size_t ury, size_t lrx, size_t lry, size_t llx, size_t lly, size_t ynum, size_t xnum)
{
    size_t row, col;
    double *data, *p;

    ICV_IMAGE_VAL_INT(img);

    /* Validate crop dimensions to prevent integer overflow during allocation */
    if (img->width == 0 || img->height == 0 || xnum < 2 || ynum < 2 || xnum > (size_t)-1 / ynum / img->channels / sizeof(double)) {
	bu_log("icv_crop : Invalid crop dimensions (must be at least 2x2 and fit in memory).\n");
	return -1;
    }

    /* Allocates output data and assigns to image*/
    data = img->data;
    img->data = p = (double *)bu_malloc(ynum*xnum*img->channels*sizeof(double), "icv_crop: Out Image");

    for (row = 0; row < ynum; row++) {
	double row_t = (double)row / (double)(ynum - 1);
	double x_1 = (double)llx + ((double)ulx - (double)llx) * row_t;
	double y_1 = (double)lly + ((double)uly - (double)lly) * row_t;
	double x_2 = (double)lrx + ((double)urx - (double)lrx) * row_t;
	double y_2 = (double)lry + ((double)ury - (double)lry) * row_t;

	for (col = 0; col < xnum; col++) {
	    size_t c;
	    double col_t = (double)col / (double)(xnum - 1);
	    double sx = x_1 + (x_2 - x_1) * col_t;
	    double sy = y_1 + (y_2 - y_1) * col_t;
	    size_t x0, y0, x1, y1;
	    double dx, dy, sx_floor, sy_floor;

	    if (sx < 0.0) sx = 0.0;
	    if (sy < 0.0) sy = 0.0;
	    if (sx > (double)(img->width - 1)) sx = (double)(img->width - 1);
	    if (sy > (double)(img->height - 1)) sy = (double)(img->height - 1);

	    sx_floor = floor(sx);
	    sy_floor = floor(sy);
	    x0 = (size_t)sx_floor;
	    y0 = (size_t)sy_floor;
	    x1 = x0 + 1;
	    y1 = y0 + 1;
	    if (x1 >= img->width) x1 = img->width - 1;
	    if (y1 >= img->height) y1 = img->height - 1;
	    dx = sx - sx_floor;
	    dy = sy - sy_floor;

	    for (c = 0; c < img->channels; c++) {
		double ll = data[(img->width * y0 + x0) * img->channels + c];
		double lr = data[(img->width * y0 + x1) * img->channels + c];
		double ul = data[(img->width * y1 + x0) * img->channels + c];
		double ur = data[(img->width * y1 + x1) * img->channels + c];
		double low = ll + dx * (lr - ll);
		double upp = ul + dx * (ur - ul);
		p[c] = low + dy * (upp - low);
	    }
	    p += img->channels;
	}
    }
    bu_free(data, "icv_crop : frees input image buffer");
    img->width = xnum;
    img->height = ynum;
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
