/*                           H I T . C
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
/** @file hit.c
 *
 */

#include "hit.h"
#include "adrt_struct.h"


void* render_hit(tie_ray_t *ray, tie_id_t *id, tie_tri_t *tri, void *ptr) {
    /* Flip normal to face ray origin (via dot product check) */
    if (ray->dir.v[0] * id->norm.v[0] + ray->dir.v[1] * id->norm.v[1] + ray->dir.v[2] * id->norm.v[2] > 0)
	VSCALE(id->norm.v,  id->norm.v,  -1.0);

    return((adrt_mesh_t *)(tri->ptr));
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
