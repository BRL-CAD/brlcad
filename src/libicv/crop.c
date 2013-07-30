/*                          C R O P . C
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
/** @file libicv/crop.c
 *
 * This file contains functions for cropping images.
 * There are two types of cropping rectangular and skeyed.
 *
 */
#include "bu.h"
#include "icv.h"
#include "vmath.h"

int
icv_rect(icv_image_t *img, int xorig, int yorig, int xnum, int ynum)
{
    int row;
    double *p, *in_data, *out_data;
    int widthstep_in, widthstep_out, bytes_row; /**<  */

    if (!ICV_IMAGE_IS_INITIALIZED(img)) {
	    bu_log("ERROR: trying to crop a null image\n");
	    return -1;
    }

    if (xorig < 0)
	xorig = 0;

    if (yorig < 0)
	yorig = 0;

    if (xnum <= 0)
	bu_exit(1, "icv_rect : ERROR: Horizontal Cut Size\n");

    if (ynum <= 0)
	bu_exit(1, "icv_rect : ERROR: Horizontal Cut Size\n");

    if (xorig+xnum > img->width)
	bu_exit(1, "icv_rect : Cut not possible, Input parameters exceeds the width\n");

    if (yorig+ynum > img->height)
	bu_exit(1, "icv_rect : Cut not possible, Input parameters exceeds the height\n");

    /* initialization of variables to insure cropping and copying */
    widthstep_in = img->width*img->channels;
    widthstep_out = xnum*img->channels;
    bytes_row = widthstep_out*sizeof(double);
    out_data = p = bu_malloc(ynum*bytes_row,"icv_rect : Cropped Image Data" );

    /* Hopes to the initial point to be extracted on the first line */
    in_data = img->data + xorig*img->channels;

    for (row = yorig; row < yorig+ynum ;row++) {
	    VMOVEN(p,in_data,widthstep_out);
	    in_data	+= widthstep_in;
	    p += widthstep_out;
    }

    bu_free(img->data, "icv image input data");
    img->width = xnum;
    img->height = ynum;
    img->data = out_data;
    return 0;
}
