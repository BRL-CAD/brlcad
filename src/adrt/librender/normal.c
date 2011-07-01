/*                        N O R M A L . C
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
/** @file librender/normal.c
 *
 */

#include <stdio.h>
#include "adrt_struct.h"

void
render_normal_free(render_t *UNUSED(render)) {
    return;
}

static void *
normal_hit(struct tie_ray_s *UNUSED(ray), struct tie_id_s *UNUSED(id), struct tie_tri_s *tri, void *UNUSED(ptr)) {
    return (adrt_mesh_t *)(tri->ptr);
}

void
render_normal_work(render_t *UNUSED(render), struct tie_s *tie, struct tie_ray_s *ray, vect_t *pixel) {
    struct tie_id_s	id;
    float	one[3] = { 1, 1, 1 };

    if (tie_work(tie, ray, &id, normal_hit, NULL))
	VADD2SCALE(*pixel, id.norm, one, 0.5);
    return;
}

int
render_normal_init(render_t *render, const char *UNUSED(usr)) {
    render->work = render_normal_work;
    render->free = render_normal_free;
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
