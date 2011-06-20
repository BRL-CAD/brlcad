/*                     T E X T U R E _ C L O U D S . C
 * BRL-CAD / ADRT
 *
 * Copyright (c) 2002-2011 United States Government as represented by
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
/** @file librender/texture_clouds.c
 *
 *  Comments -
 *      Texture Library - Perlin Clouds
 *
 */

#include "texture.h"
#include <stdlib.h>
#include "adrt_struct.h"

#include "bu.h"

void
texture_clouds_init(struct texture_s *texture, tfloat size, int octaves, int absolute, vect_t scale, vect_t translate) {
    struct texture_clouds_s *td;

    texture->data = bu_malloc(sizeof(struct texture_clouds_s), "cloud data");
    texture->free = texture_clouds_free;
    texture->work = (texture_work_t *)texture_clouds_work;

    td = (struct texture_clouds_s*)texture->data;
    td->size = size;
    td->octaves = octaves;
    td->absolute = absolute;
    VMOVE(td->scale, scale);
    VMOVE(td->translate, translate);

    texture_perlin_init(&td->perlin);
}

void
texture_clouds_free(struct texture_s *texture) {
    struct texture_clouds_s *td;

    td = (struct texture_clouds_s*)texture->data;
    texture_perlin_free(&td->perlin);
    bu_free(texture->data, "cloud data");
}

void
texture_clouds_work(struct texture_s *texture, void *mesh, struct tie_ray_s *UNUSED(ray), struct tie_id_s *id, vect_t *pixel) {
    struct texture_clouds_s *td;
    vect_t p, pt;


    td = (struct texture_clouds_s*)texture->data;

    /* Transform the Point */
    if (td->absolute) {
	p[0] = id->pos[0] * td->scale[0] + td->translate[0];
	p[1] = id->pos[1] * td->scale[1] + td->translate[1];
	p[2] = id->pos[2] * td->scale[2] + td->translate[2];
    } else {
	MATH_VEC_TRANSFORM(pt, id->pos, ADRT_MESH(mesh)->matinv);
	p[0] = (ADRT_MESH(mesh)->max[0] - ADRT_MESH(mesh)->min[0] > TIE_PREC ? (pt[0] - ADRT_MESH(mesh)->min[0]) / (ADRT_MESH(mesh)->max[0] - ADRT_MESH(mesh)->min[0]) : 0.0) * td->scale[0] + td->translate[0];
	p[1] = (ADRT_MESH(mesh)->max[1] - ADRT_MESH(mesh)->min[1] > TIE_PREC ? (pt[1] - ADRT_MESH(mesh)->min[1]) / (ADRT_MESH(mesh)->max[1] - ADRT_MESH(mesh)->min[1]) : 0.0) * td->scale[1] + td->translate[1];
	p[2] = (ADRT_MESH(mesh)->max[2] - ADRT_MESH(mesh)->min[2] > TIE_PREC ? (pt[2] - ADRT_MESH(mesh)->min[2]) / (ADRT_MESH(mesh)->max[2] - ADRT_MESH(mesh)->min[2]) : 0.0) * td->scale[2] + td->translate[2];
    }

    *pixel[0] = fabs(0.5*texture_perlin_noise3(&td->perlin, p, td->size*1.0, td->octaves) + 0.5);
    *pixel[1] = fabs(0.5*texture_perlin_noise3(&td->perlin, p, td->size*1.0, td->octaves) + 0.5);
    *pixel[2] = fabs(0.5*texture_perlin_noise3(&td->perlin, p, td->size*1.0, td->octaves) + 0.5);
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
