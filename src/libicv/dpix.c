/*                          D P I X . C
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
/** @file dpix.c
 *
 * this contains read/write routines for dpix format.
 *
 */

#include "common.h"

#include <sys/stat.h>  /* for file mode infor in WRMODE */
#include <fcntl.h>

#include "bio.h"
#include "bu.h"
#include "icv.h"
#include "vmath.h"

#define WRMODE S_IRUSR|S_IRGRP|S_IROTH

/* 
 * This function normalizes the data array of the input image.
 * This performs the normalization when the input image has data
 * enteries less than 0.0 or greater than 1.0
 */
HIDDEN icv_image_t *
icv_normalize(icv_image_t *bif)
{	
    double *data;
    double max, min;
    double m, b;
    size_t size;
    unsigned long int i;

    if (bif == NULL) {
    	bu_log("icv_normalize : trying to normalize a NULL bif\n");
    	return bif;
    }

    data = bif->data;
    size = bif->height*bif->width*bif->channels;

    min = INFINITY;
    max = -INFINITY;

    for (i = 0; i<size; i++) {
        V_MIN(min, *data);
        V_MAX(max, *data);
        data++;
    }
    if(max <= 1.0 || min >= 0.0) 
        return bif;

    data = bif->data;
    m = 1/(max-min);
    b = -min/(max-min);

    for (i =0; i<size; i++) {
        *data = m*(*data) + b;
        data++;
    }

    return bif;
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
