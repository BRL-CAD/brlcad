/*                     C O M P O N E N T . C
 * BRL-CAD / ADRT
 *
 * Copyright (c) 2007-2011 United States Government as represented by
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
/** @file librender/component.c
 *
 */

#include <stdio.h>

#include "adrt.h"
#include "adrt_struct.h"

void
render_component_free(render_t *UNUSED(render))
{
    return;
}

static void *
component_hit(struct tie_ray_s *ray, struct tie_id_s *UNUSED(id), struct tie_tri_s *tri, void *UNUSED(ptr))
{
    adrt_mesh_t *mesh = (adrt_mesh_t *)(tri->ptr);

    ray->depth++;
    return (mesh->flags & (ADRT_MESH_SELECT|ADRT_MESH_HIT)) ? mesh : NULL;
}

void
render_component_work(render_t *UNUSED(render), struct tie_s *tie, struct tie_ray_s *ray, vect_t *pixel)
{
    struct tie_id_s id;
    adrt_mesh_t *mesh;
    vect_t vec;

    if ((mesh = (adrt_mesh_t *)tie_work(tie, ray, &id, component_hit, NULL))) {
	/* Flip normal to face ray origin (via dot product check) */
	if (ray->dir[0] * id.norm[0] + ray->dir[1] * id.norm[1] + ray->dir[2] * id.norm[2] > 0)
	    VSCALE(id.norm,  id.norm,  -1.0);

	/* shade solid */
	*pixel[0] = mesh->flags & ADRT_MESH_HIT ? 0.8 : 0.2;
	*pixel[1] = (tfloat)0.2;
	*pixel[2] = mesh->flags & ADRT_MESH_SELECT ? 0.8 : 0.2;
	VSUB2(vec,  ray->pos,  id.pos);
	VUNITIZE(vec);
	VSCALE((*pixel), (*pixel), VDOT(vec, id.norm) * 0.8);
    } else if (ray->depth) {
	*pixel[0] += (tfloat)0.2;
	*pixel[1] += (tfloat)0.2;
	*pixel[2] += (tfloat)0.2;
    }
}

int
render_component_init(render_t *render, const char *UNUSED(usr)) {
    render->work = render_component_work;
    render->free = render_component_free;
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
