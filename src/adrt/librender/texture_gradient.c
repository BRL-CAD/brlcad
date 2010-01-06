/*                     T E X T U R E _ G R A D I E N T . C
 * BRL-CAD / ADRT
 *
 * Copyright (c) 2002-2010 United States Government as represented by
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
/** @file texture_gradient.c
 *
 *  Comments -
 *      Texture Library - Produces Gradient to be used with Blend
 *
 */

#include "texture.h"
#include <stdlib.h>
#include "adrt_struct.h"

#include "bu.h"

void texture_gradient_init(texture_t *texture, int axis) {
    texture_gradient_t *td;

    texture->data = bu_malloc(sizeof(texture_gradient_t), "gradient data");
    texture->free = texture_gradient_free;
    texture->work = (texture_work_t *)texture_gradient_work;

    td = (texture_gradient_t *)texture->data;
    td->axis = axis;
}


void texture_gradient_free(texture_t *texture) {
    bu_free(texture->data, "gradient data");
}


void texture_gradient_work(__TEXTURE_WORK_PROTOTYPE__) {
    texture_gradient_t *td;
    TIE_3 pt;


    td = (texture_gradient_t *)texture->data;

    /* Transform the Point */
    MATH_VEC_TRANSFORM(pt, id->pos, ADRT_MESH(mesh)->matinv);

    if (td->axis == 1) {
	pixel->v[0] = pixel->v[1] = pixel->v[2] = ADRT_MESH(mesh)->max.v[1] - ADRT_MESH(mesh)->min.v[1] > TIE_PREC ? (pt.v[1] - ADRT_MESH(mesh)->min.v[1]) / (ADRT_MESH(mesh)->max.v[1] - ADRT_MESH(mesh)->min.v[1]) : 0.0;
    } else if (td->axis == 2) {
	pixel->v[0] = pixel->v[1] = pixel->v[2] = ADRT_MESH(mesh)->max.v[2] - ADRT_MESH(mesh)->min.v[2] > TIE_PREC ? (pt.v[2] - ADRT_MESH(mesh)->min.v[2]) / (ADRT_MESH(mesh)->max.v[2] - ADRT_MESH(mesh)->min.v[1]) : 0.0;
    } else {
	pixel->v[0] = pixel->v[1] = pixel->v[2] = ADRT_MESH(mesh)->max.v[0] - ADRT_MESH(mesh)->min.v[0] > TIE_PREC ? (pt.v[0] - ADRT_MESH(mesh)->min.v[0]) / (ADRT_MESH(mesh)->max.v[0] - ADRT_MESH(mesh)->min.v[1]) : 0.0;
    }
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
