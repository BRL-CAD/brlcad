/*                           H I T . C
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
/** @file librender/hit.c
 *
 */

#include "adrt_struct.h"


void* render_hit(struct tie_ray_s *ray, struct tie_id_s *id, struct tie_tri_s *tri, void *UNUSED(ptr)) {
    /* Flip normal to face ray origin (via dot product check) */
    if (ray->dir[0] * id->norm[0] + ray->dir[1] * id->norm[1] + ray->dir[2] * id->norm[2] > 0)
	VSCALE(id->norm,  id->norm,  -1.0);

    return (adrt_mesh_t *)(tri->ptr);
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
