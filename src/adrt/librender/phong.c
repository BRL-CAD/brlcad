/*                         P H O N G . C
 * BRL-CAD / ADRT
 *
 * Copyright (c) 2007-2010 United States Government as represented by
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

#include <stdio.h>

#include "vmath.h"

#include "adrt_struct.h"

void
render_phong_free(render_t *render)
{
    return;
}

void
render_phong_work(render_t *render, tie_t *tie, tie_ray_t *ray, TIE_3 *pixel)
{
    tie_id_t		id;
    adrt_mesh_t		*mesh;

    if ((mesh = (adrt_mesh_t*)tie_work(tie, ray, &id, render_hit, NULL)) != NULL) {
	TIE_3		vec;

	*pixel = mesh->attributes->color;

	if (mesh->texture)
	    mesh->texture->work(mesh->texture, mesh, ray, &id, pixel);

	VSUB2(vec.v,  ray->pos.v,  id.pos.v);
	VUNITIZE(vec.v);
	VSCALE((*pixel).v, (*pixel).v, VDOT( vec.v,  id.norm.v));
    }
    return;
}

int
render_phong_init(render_t *render, char *usr)
{
    render->work = render_phong_work;
    render->free = render_phong_free;
    return 0;
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
