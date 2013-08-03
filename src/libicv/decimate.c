/*                      D E C I M A T E . C
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
/** @file decimate.c
 *
 * This file contains routines to scale down an image to a lower
 * resolution.
 *
 */

#include "bu.h"
#include "icv.h"

HIDDEN void shrink_image(icv_image_t* bif, int factor)
{
    double *data_p, *res_p; /**< input and output pointers */
    double *p;
    int facsq,x,y,py,px,c;
    size_t widthstep =  bif->width*bif->channels;

    facsq = factor*factor,c;
    res_p = bif->data;
    p = bu_malloc(bif->channels*sizeof(double), "shrink_image : Pixel Values Temp Buffer");

    for (y=0; y<bif->height; y+=factor)
        for (x=0; x<bif->width; x+=factor) {

            for (c=0; c<bif->channels; c++) {
                p[c]= 0;
            }

            for (py = 0; py < factor; py++) {
                data_p = bif->data + (y+py)*widthstep;
                for (px = 0; px < factor; px++) {
                    for (c=0; c<bif->channels; c++) {
                        p[c] += *data_p++;
                    }
                }
            }

            for (c=0; c<bif->channels; c++)
                *res_p++ = p[c]/facsq;
        }

    bif->width = (int) bif->width/factor;
    bif->height = (int) bif->height/factor;
    bif->data = bu_realloc(bif->data, (size_t) (bif->width*bif->height*bif->channels), "shrink_image : Reallocation");

    return;

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
