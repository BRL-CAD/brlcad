/*                     T E X T U R E _ G R A D I E N T . C
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
/** @file librender/texture_gradient.c
 *
 *  Comments -
 *      Texture Library - Produces Gradient to be used with Blend
 *
 */

#include "texture.h"
#include <stdlib.h>
#include "adrt_struct.h"

#include "bu.h"

void
texture_gradient_init(struct texture_s *texture, int axis) {
    struct texture_gradient_s *td;

    texture->data = bu_malloc(sizeof(struct texture_gradient_s), "gradient data");
    texture->free = texture_gradient_free;
    texture->work = (texture_work_t *)texture_gradient_work;

    td = (struct texture_gradient_s *)texture->data;
    td->axis = axis;
}

void
texture_gradient_free(struct texture_s *texture) {
    bu_free(texture->data, "gradient data");
}

void
texture_gradient_work(struct texture_s *texture, void *mesh, struct tie_ray_s *UNUSED(ray), struct tie_id_s *id, vect_t *pixel) {
    struct texture_gradient_s *td;
    vect_t pt;

    td = (struct texture_gradient_s *)texture->data;

    /* Transform the Point */
    MATH_VEC_TRANSFORM(pt, id->pos, ADRT_MESH(mesh)->matinv);

    if (td->axis == 1) {
	*pixel[0] = *pixel[1] = *pixel[2] = ADRT_MESH(mesh)->max[1] - ADRT_MESH(mesh)->min[1] > TIE_PREC ? (pt[1] - ADRT_MESH(mesh)->min[1]) / (ADRT_MESH(mesh)->max[1] - ADRT_MESH(mesh)->min[1]) : 0.0;
    } else if (td->axis == 2) {
	*pixel[0] = *pixel[1] = *pixel[2] = ADRT_MESH(mesh)->max[2] - ADRT_MESH(mesh)->min[2] > TIE_PREC ? (pt[2] - ADRT_MESH(mesh)->min[2]) / (ADRT_MESH(mesh)->max[2] - ADRT_MESH(mesh)->min[1]) : 0.0;
    } else {
	*pixel[0] = *pixel[1] = *pixel[2] = ADRT_MESH(mesh)->max[0] - ADRT_MESH(mesh)->min[0] > TIE_PREC ? (pt[0] - ADRT_MESH(mesh)->min[0]) / (ADRT_MESH(mesh)->max[0] - ADRT_MESH(mesh)->min[1]) : 0.0;
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
