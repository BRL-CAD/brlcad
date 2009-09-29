/*                     T E X T U R E _ B L E N D . C
 * BRL-CAD / ADRT
 *
 * Copyright (c) 2002-2009 United States Government as represented by
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
/** @file texture_blend.c
 *
 *  Comments -
 *      Texture Library - Uses the R and B channels to blend 2 colors
 *
 */

#include "texture.h"
#include <stdlib.h>

#include "bu.h"

void texture_blend_init(texture_t *texture, TIE_3 color1, TIE_3 color2) {
    texture_blend_t *sd;

    texture->data = bu_malloc(sizeof(texture_blend_t), "texture data");
    texture->free = texture_blend_free;
    texture->work = (texture_work_t *)texture_blend_work;

    sd = (texture_blend_t *)texture->data;
    sd->color1 = color1;
    sd->color2 = color2;
}


void texture_blend_free(texture_t *texture) {
    bu_free(texture->data, "texture data");
}


void texture_blend_work(__TEXTURE_WORK_PROTOTYPE__) {
    texture_blend_t *sd;
    tfloat coef;

    sd = (texture_blend_t *)texture->data;

    coef = pixel->v[0];
    pixel->v[0] = (1.0 - coef)*sd->color1.v[0] + coef*sd->color2.v[0];
    pixel->v[1] = (1.0 - coef)*sd->color1.v[1] + coef*sd->color2.v[1];
    pixel->v[2] = (1.0 - coef)*sd->color1.v[2] + coef*sd->color2.v[2];
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
