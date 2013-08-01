/*                    O P E R A T I O N S . C
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

void icv_sanitize(icv_image_t* img)
{
    double *data = NULL;
    size_t size;
    data= img->data;
    for (size = img->width*img->height*img->channels; size>0; size--) {
	if(*data>1.0)
	    *data = 1.0;
	else if (*data<0)
	    *data = 0;
	data++;
    }
    img->flags |= ICV_SANITIZED;
}

void icv_add_val(icv_image_t* img, double val)
{
    double *data = NULL;
    size_t size;

    data = img->data;

    for (size = img->width*img->height*img->channels; size>0; size--) {
	*data += val;
	data++;
    }

    if(img->flags && ICV_OPERATIONS_MODE)
	img->flags&=(!ICV_SANITIZED);
    else
	icv_sanitize(img);
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
