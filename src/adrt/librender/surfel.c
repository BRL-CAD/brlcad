/*                        S U R F E L . C
 * BRL-CAD
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

#include <stdio.h>
#include <stdlib.h>

#include "bu.h"

#include "render.h"
#include "adrt_struct.h"

typedef struct render_surfel_pt_s {
    point_t pos;
    fastf_t radius;
    vect_t color;
} render_surfel_pt_t;

typedef struct render_surfel_s {
    uint32_t num;
    render_surfel_pt_t *list;
} render_surfel_t;


void
render_surfel_free(render_t *render)
{
    render_surfel_t *d;

    d = (render_surfel_t *)render->data;
    bu_free(d->list, "data list");
    bu_free(render->data, "render data");
}


void
render_surfel_work(render_t *render, struct tie_s *tie, struct tie_ray_s *ray, vect_t *pixel)
{
    render_surfel_t *d;
    struct tie_id_s id;
    adrt_mesh_t *mesh;
    uint32_t i;
    fastf_t dist_sq;

    d = (render_surfel_t *)render->data;

    if ((mesh = (adrt_mesh_t *)tie_work(tie, ray, &id, render_hit, NULL))) {
	for (i = 0; i < d->num; i++) {
	    dist_sq = (d->list[i].pos[0]-id.pos[0]) * (d->list[i].pos[0]-id.pos[0]) +
		(d->list[i].pos[1]-id.pos[1]) * (d->list[i].pos[1]-id.pos[1]) +
		(d->list[i].pos[2]-id.pos[2]) * (d->list[i].pos[2]-id.pos[2]);

	    if (dist_sq < d->list[i].radius*d->list[i].radius) {
		VMOVE(*pixel, d->list[i].color);
		break;
	    }
	}

	VSET(*pixel, 0.8, 0.8, 0.8);
    }
}

int
render_surfel_init(render_t *render, const char *buf)
{
    render_surfel_t *d;

    if(buf == NULL)
	return -1;

    render->work = render_surfel_work;
    render->free = render_surfel_free;
    render->data = (render_surfel_t *)bu_malloc(sizeof(render_surfel_t), "render data");
    d = (render_surfel_t *)render->data;
    d->num = 0;
    d->list = NULL;
    /*
       d->list = (render_surfel_pt_t *)bu_malloc(d->num * sizeof(render_surfel_pt_t), "data list");
       */
    /* do something to extract num and list from buf */
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
