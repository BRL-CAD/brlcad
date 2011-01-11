/*                        F L O S . C
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
/** @file flos.c
 *
 */

#ifndef TIE_PRECISION
# define TIE_PRECISION 0
#endif

#include "adrt_struct.h"

#include <stdio.h>
#include <stdlib.h>

#include "bu.h"

typedef struct render_flos_s {
    TIE_3 frag_pos;
} render_flos_t;


void render_flos_free(render_t *render) {
}

void render_flos_work(render_t *render, tie_t *tie, tie_ray_t *ray, TIE_3 *pixel) {
    tie_id_t id, tid;
    adrt_mesh_t *mesh;
    TIE_3 vec;
    tfloat angle;
    render_flos_t *rd;

    rd = (render_flos_t *)render->data;

    if ((mesh = (adrt_mesh_t *)tie_work(tie, ray, &id, render_hit, NULL))) {
	VSET((*pixel).v, 0.0, 0.5, 0.0);
    } else {
	return;
    }

    VSUB2(vec.v,  ray->pos.v,  id.pos.v);
    VUNITIZE(vec.v);
    angle = VDOT( vec.v,  id.norm.v);

    /* Determine if direct line of sight to fragment */
    ray->pos = rd->frag_pos;
    VSUB2(ray->dir.v,  id.pos.v,  rd->frag_pos.v);
    VUNITIZE(ray->dir.v);

    if (tie_work(tie, ray, &tid, render_hit, NULL)) {
	if (fabs (id.pos.v[0] - tid.pos.v[0]) < TIE_PREC &&
	    fabs (id.pos.v[1] - tid.pos.v[1]) < TIE_PREC &&
	    fabs (id.pos.v[2] - tid.pos.v[2]) < TIE_PREC)
	{
	    VSET((*pixel).v, 1.0, 0.0, 0.0);
	}
    }

    VSCALE((*pixel).v,  (*pixel).v,  (0.5+angle*0.5));
}

int
render_flos_init(render_t *render, const char *frag_pos)
{
    render_flos_t *d;

    if(frag_pos == NULL)
	    return -1;

    render->work = render_flos_work;
    render->free = render_flos_free;
    render->data = (render_flos_t *)bu_malloc(sizeof(render_flos_t), "render_flos_init");
    d = (render_flos_t *)render->data;
    sscanf(frag_pos, "#(%f %f %f)", &d->frag_pos.v[0], &d->frag_pos.v[1],  &d->frag_pos.v[2]);
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
