/*                     T E X T U R E _ C L O U D S . C
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
/** @file texture_clouds.c
 *
 *  Comments -
 *      Texture Library - Perlin Clouds
 *
 */

#include "texture.h"
#include <stdlib.h>
#include "adrt_struct.h"

#include "bu.h"

void texture_clouds_init(texture_t *texture, tfloat size, int octaves, int absolute, TIE_3 scale, TIE_3 translate) {
    texture_clouds_t *td;

    texture->data = bu_malloc(sizeof(texture_clouds_t), "cloud data");
    texture->free = texture_clouds_free;
    texture->work = (texture_work_t *)texture_clouds_work;

    td = (texture_clouds_t*)texture->data;
    td->size = size;
    td->octaves = octaves;
    td->absolute = absolute;
    td->scale = scale;
    td->translate = translate;

    texture_perlin_init(&td->perlin);
}


void texture_clouds_free(texture_t *texture) {
    texture_clouds_t *td;

    td = (texture_clouds_t*)texture->data;
    texture_perlin_free(&td->perlin);
    bu_free(texture->data, "cloud data");
}


void texture_clouds_work(__TEXTURE_WORK_PROTOTYPE__) {
    texture_clouds_t *td;
    TIE_3 p, pt;


    td = (texture_clouds_t*)texture->data;

    /* Transform the Point */
    if (td->absolute) {
	p.v[0] = id->pos.v[0] * td->scale.v[0] + td->translate.v[0];
	p.v[1] = id->pos.v[1] * td->scale.v[1] + td->translate.v[1];
	p.v[2] = id->pos.v[2] * td->scale.v[2] + td->translate.v[2];
    } else {
	MATH_VEC_TRANSFORM(pt, id->pos, ADRT_MESH(mesh)->matinv);
	p.v[0] = (ADRT_MESH(mesh)->max.v[0] - ADRT_MESH(mesh)->min.v[0] > TIE_PREC ? (pt.v[0] - ADRT_MESH(mesh)->min.v[0]) / (ADRT_MESH(mesh)->max.v[0] - ADRT_MESH(mesh)->min.v[0]) : 0.0) * td->scale.v[0] + td->translate.v[0];
	p.v[1] = (ADRT_MESH(mesh)->max.v[1] - ADRT_MESH(mesh)->min.v[1] > TIE_PREC ? (pt.v[1] - ADRT_MESH(mesh)->min.v[1]) / (ADRT_MESH(mesh)->max.v[1] - ADRT_MESH(mesh)->min.v[1]) : 0.0) * td->scale.v[1] + td->translate.v[1];
	p.v[2] = (ADRT_MESH(mesh)->max.v[2] - ADRT_MESH(mesh)->min.v[2] > TIE_PREC ? (pt.v[2] - ADRT_MESH(mesh)->min.v[2]) / (ADRT_MESH(mesh)->max.v[2] - ADRT_MESH(mesh)->min.v[2]) : 0.0) * td->scale.v[2] + td->translate.v[2];
    }

    pixel->v[0] = fabs(0.5*texture_perlin_noise3(&td->perlin, p, td->size*1.0, td->octaves) + 0.5);
    pixel->v[1] = fabs(0.5*texture_perlin_noise3(&td->perlin, p, td->size*1.0, td->octaves) + 0.5);
    pixel->v[2] = fabs(0.5*texture_perlin_noise3(&td->perlin, p, td->size*1.0, td->octaves) + 0.5);
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
