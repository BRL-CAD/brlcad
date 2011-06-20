/*                     T E X T U R E _ C A M O . C
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
/** @file librender/texture.h
 *
 *  Comments -
 *      Texture Library - Applies a 3 color camoflauge
 *
 */

#include "texture.h"
#include <stdlib.h>
#include "adrt_struct.h"

#include "bu.h"

void
texture_camo_init(struct texture_s *texture, fastf_t size, int octaves, int absolute, vect_t color1, vect_t color2, vect_t color3) {
    struct texture_camo_s   *sd;

    texture->data = bu_malloc(sizeof(struct texture_camo_s), "camo data");
    texture->free = texture_camo_free;
    texture->work = (texture_work_t *)texture_camo_work;

    sd = (struct texture_camo_s *)texture->data;
    sd->size = size;
    sd->octaves = octaves;
    sd->absolute = absolute;
    VMOVE(sd->color1, color1);
    VMOVE(sd->color2, color2);
    VMOVE(sd->color3, color3);

    texture_perlin_init(&sd->perlin);
}

void
texture_camo_free(struct texture_s *texture) {
    struct texture_camo_s *td;

    td = (struct texture_camo_s *)texture->data;
    texture_perlin_free(&td->perlin);
    bu_free(texture->data, "camo data");
}

void
texture_camo_work(struct texture_s *texture, void *mesh, struct tie_ray_s *UNUSED(ray), struct tie_id_s *id, vect_t *pixel) {
    struct texture_camo_s *td;
    vect_t p, pt;
    tfloat sum1, sum2;


    td = (struct texture_camo_s *)texture->data;

    /* Transform the Point */
    MATH_VEC_TRANSFORM(pt, id->pos, ADRT_MESH(mesh)->matinv);
    if (td->absolute) {
	VMOVE(p, id->pos);
    } else {
	p[0] = ADRT_MESH(mesh)->max[0] - ADRT_MESH(mesh)->min[0] > TIE_PREC ? (pt[0] - ADRT_MESH(mesh)->min[0]) / (ADRT_MESH(mesh)->max[0] - ADRT_MESH(mesh)->min[0]) : 0.0;
	p[1] = ADRT_MESH(mesh)->max[1] - ADRT_MESH(mesh)->min[1] > TIE_PREC ? (pt[1] - ADRT_MESH(mesh)->min[1]) / (ADRT_MESH(mesh)->max[1] - ADRT_MESH(mesh)->min[1]) : 0.0;
	p[2] = ADRT_MESH(mesh)->max[2] - ADRT_MESH(mesh)->min[2] > TIE_PREC ? (pt[2] - ADRT_MESH(mesh)->min[2]) / (ADRT_MESH(mesh)->max[2] - ADRT_MESH(mesh)->min[2]) : 0.0;
    }

    sum1 = fabs(texture_perlin_noise3(&td->perlin, p, td->size*1.0, td->octaves));
    sum2 = fabs(texture_perlin_noise3(&td->perlin, p, td->size*0.8, td->octaves+1));

    if (sum1 < 0.3) {
	VMOVE(*pixel, td->color1);
    } else {
	VMOVE(*pixel, td->color2);
    }

    if (sum2 < 0.3)
	VMOVE(*pixel, td->color3);
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
