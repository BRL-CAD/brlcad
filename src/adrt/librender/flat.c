/*                          F L A T . C
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
/** @file flat.c
 *
 */

#include "flat.h"
#include "hit.h"
#include "adrt_struct.h"
#include <stdio.h>


void render_flat_init(render_t *render, char *usr) {
    render->work = render_flat_work;
    render->free = render_flat_free;
}


void render_flat_free(render_t *render) {
}


void render_flat_work(render_t *render, tie_t *tie, tie_ray_t *ray, TIE_3 *pixel) {
    tie_id_t id;
    adrt_mesh_t *mesh;

    if ((mesh = (adrt_mesh_t *)tie_work(tie, ray, &id, render_hit, NULL))) {
	*pixel = mesh->attributes->color;
	if (mesh->texture)
	    mesh->texture->work(mesh->texture, mesh, ray, &id, pixel);
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
