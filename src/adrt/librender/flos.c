/*                        F L O S . C
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
/** @file librender/flos.c
 *
 */

#ifndef TIE_PRECISION
# define TIE_PRECISION 0
#endif

#include "adrt_struct.h"

#include <stdio.h>
#include <stdlib.h>

#include "bu.h"

struct render_flos_s {
    point_t frag_pos;
};


void
render_flos_free(render_t *UNUSED(render)) {
}

void
render_flos_work(render_t *render, struct tie_s *tie, struct tie_ray_s *ray, vect_t *pixel) {
    struct tie_id_s id, tid;
    adrt_mesh_t *mesh;
    vect_t vec;
    fastf_t angle;
    struct render_flos_s *rd;

    rd = (struct render_flos_s *)render->data;

    if ((mesh = (adrt_mesh_t *)tie_work(tie, ray, &id, render_hit, NULL))) {
	VSET(*pixel, 0.0, 0.5, 0.0);
    } else {
	return;
    }

    VSUB2(vec,  ray->pos,  id.pos);
    VUNITIZE(vec);
    angle = VDOT( vec,  id.norm);

    /* Determine if direct line of sight to fragment */
    VMOVE(ray->pos, rd->frag_pos);
    VSUB2(ray->dir,  id.pos,  rd->frag_pos);
    VUNITIZE(ray->dir);

    if (tie_work(tie, ray, &tid, render_hit, NULL))
	if (fabs (id.pos[0] - tid.pos[0]) < TIE_PREC &&
		fabs (id.pos[1] - tid.pos[1]) < TIE_PREC &&
		fabs (id.pos[2] - tid.pos[2]) < TIE_PREC)
	    VSET(*pixel, 1.0, 0.0, 0.0);

    VSCALE(*pixel, *pixel, (0.5+angle*0.5));
}

int
render_flos_init(render_t *render, const char *frag_pos)
{
    struct render_flos_s *d;

    if(frag_pos == NULL)
	return -1;

    render->work = render_flos_work;
    render->free = render_flos_free;
    render->data = (struct render_flos_s *)bu_malloc(sizeof(struct render_flos_s), "render_flos_init");
    d = (struct render_flos_s *)render->data;
    sscanf(frag_pos, "#(%lf %lf %lf)", &d->frag_pos[0], &d->frag_pos[1],  &d->frag_pos[2]);
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
