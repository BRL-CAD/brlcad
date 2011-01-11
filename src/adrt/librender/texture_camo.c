/*                     T E X T U R E _ C A M O . C
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
/** @file texture.h
 *
 *  Comments -
 *      Texture Library - Applies a 3 color camoflauge
 *
 */

#include "texture.h"
#include <stdlib.h>
#include "adrt_struct.h"

#include "bu.h"

void texture_camo_init(struct texture_s *texture, tfloat size, int octaves, int absolute, TIE_3 color1, TIE_3 color2, TIE_3 color3) {
    struct texture_camo_s   *sd;

    texture->data = bu_malloc(sizeof(struct texture_camo_s), "camo data");
    texture->free = texture_camo_free;
    texture->work = (struct texture_work_s *)texture_camo_work;

    sd = (struct texture_camo_s *)texture->data;
    sd->size = size;
    sd->octaves = octaves;
    sd->absolute = absolute;
    sd->color1 = color1;
    sd->color2 = color2;
    sd->color3 = color3;

    texture_perlin_init(&sd->perlin);
}


void texture_camo_free(struct texture_s *texture) {
    struct texture_camo_s *td;

    td = (struct texture_camo_s *)texture->data;
    texture_perlin_free(&td->perlin);
    bu_free(texture->data, "camo data");
}


void texture_camo_work(__TEXTURE_WORK_PROTOTYPE__) {
    struct texture_camo_s *td;
    TIE_3 p, pt;
    tfloat sum1, sum2;


    td = (struct texture_camo_s *)texture->data;

    /* Transform the Point */
    MATH_VEC_TRANSFORM(pt, id->pos, ADRT_MESH(mesh)->matinv);
    if (td->absolute)
	p = id->pos;
    else {
	p.v[0] = ADRT_MESH(mesh)->max.v[0] - ADRT_MESH(mesh)->min.v[0] > TIE_PREC ? (pt.v[0] - ADRT_MESH(mesh)->min.v[0]) / (ADRT_MESH(mesh)->max.v[0] - ADRT_MESH(mesh)->min.v[0]) : 0.0;
	p.v[1] = ADRT_MESH(mesh)->max.v[1] - ADRT_MESH(mesh)->min.v[1] > TIE_PREC ? (pt.v[1] - ADRT_MESH(mesh)->min.v[1]) / (ADRT_MESH(mesh)->max.v[1] - ADRT_MESH(mesh)->min.v[1]) : 0.0;
	p.v[2] = ADRT_MESH(mesh)->max.v[2] - ADRT_MESH(mesh)->min.v[2] > TIE_PREC ? (pt.v[2] - ADRT_MESH(mesh)->min.v[2]) / (ADRT_MESH(mesh)->max.v[2] - ADRT_MESH(mesh)->min.v[2]) : 0.0;
    }

    sum1 = fabs(texture_perlin_noise3(&td->perlin, p, td->size*1.0, td->octaves));
    sum2 = fabs(texture_perlin_noise3(&td->perlin, p, td->size*0.8, td->octaves+1));

    if (sum1 < 0.3)
	*pixel = td->color1;
    else
	*pixel = td->color2;

    if (sum2 < 0.3)
	*pixel = td->color3;
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
