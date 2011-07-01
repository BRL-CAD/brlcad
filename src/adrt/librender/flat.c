/*                          F L A T . C
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
/** @file librender/flat.c
 *
 */

#include "adrt_struct.h"
#include <stdio.h>


void
render_flat_free(render_t *UNUSED(render))
{
}

void
render_flat_work(render_t *UNUSED(render), struct tie_s *tie, struct tie_ray_s *ray, vect_t *pixel)
{
    struct tie_id_s id;
    adrt_mesh_t *mesh;

    if ((mesh = (adrt_mesh_t *)tie_work(tie, ray, &id, render_hit, NULL))) {
	VMOVE(*pixel, mesh->attributes->color.v);
	if (mesh->texture)
	    mesh->texture->work(mesh->texture, mesh, ray, &id, pixel);
    }
}

int
render_flat_init(render_t *render, const char *UNUSED(usr))
{
    render->work = render_flat_work;
    render->free = render_flat_free;
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
