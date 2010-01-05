/*                        N O R M A L . C
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
/** @file normal.c
 *
 */

#include "normal.h"
#include "hit.h"
#include "adrt_struct.h"
#include <stdio.h>


void
render_normal_init(render_t *render, char *usr) {
    render->work = render_normal_work;
    render->free = render_normal_free;
    return;
}


void
render_normal_free(render_t *render) {
    return;
}


static void *
normal_hit(tie_ray_t *ray, tie_id_t *id, tie_tri_t *tri, void *ptr) {
    return((adrt_mesh_t *)(tri->ptr));
}


void
render_normal_work(render_t *render, tie_t *tie, tie_ray_t *ray, TIE_3 *pixel) {
    tie_id_t	id;
    float	one[3] = { 1, 1, 1 };

    if (tie_work(tie, ray, &id, normal_hit, NULL))
	VADD2SCALE(pixel->v, id.norm.v, one, 0.5);
    return;
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
