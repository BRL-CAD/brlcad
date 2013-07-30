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

#include "icv.h"

#define KERN_DEFAULT 3

HIDDEN void
icv_get_kernel(ICV_FILTER filter_type, double *kern, double *offset) {
    switch(filter_type) {
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
