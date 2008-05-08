/*                         P H O N G . C
 * BRL-CAD / ADRT
 *
 * Copyright (c) 2007-2008 United States Government as represented by
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
/** @file phong.c
 *
 */

#include "phong.h"
#include "hit.h"
#include "adrt_struct.h"
#include <stdio.h>


void render_phong_init(render_t *render) {
    render->work = render_phong_work;
    render->free = render_phong_free;
}


void render_phong_free(render_t *render) {
}


void render_phong_work(render_t *render, tie_t *tie, tie_ray_t *ray, TIE_3 *pixel) {
    tie_id_t		id;
    adrt_mesh_t		*mesh;
    TIE_3			vec;
    tfloat		angle;

    if ((mesh = (adrt_mesh_t*)tie_work(tie, ray, &id, render_hit, NULL))) {
	*pixel = mesh->attributes->color;
	if (mesh->texture)
	    mesh->texture->work(mesh->texture, mesh, ray, &id, pixel);
    } else {
	return;
    }

    MATH_VEC_SUB(vec, ray->pos, id.pos);
    MATH_VEC_UNITIZE(vec);
    MATH_VEC_DOT(angle, vec, id.norm);
    MATH_VEC_MUL_SCALAR((*pixel), (*pixel), angle);
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
