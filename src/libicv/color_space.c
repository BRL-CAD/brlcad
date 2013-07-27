/*                   C O L O R _ S P A C E . C
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
icv_image_gray2rgb(icv_image_t *img)
{
    double *out_data,*op;    
    double *in_data;
    long int size;
    long int i = 0;
     if (!ICV_IMAGE_IS_INITIALIZED(img)) {
	bu_log("icv_image_gray2rgb : Unitialized Image argument\n");
	return -1;
    }
    
    if (img->color_space == ICV_COLOR_SPACE_RGB) {
        bu_log("icv_image_gray2rgb : already RGB");    
        return 0;
    }    
    else if (img->color_space != ICV_COLOR_SPACE_GRAY) {
	bu_log("icv_image_gray2rgb : color_space error");    
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

    bu_free(img->data, "icv_image_gray2rgb : gray image data");
    img->data = op;
    img->color_space = ICV_COLOR_SPACE_RGB;
    img->channels = 3;
    
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
