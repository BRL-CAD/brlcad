/*                          G R I D . C
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
/** @file grid.c
 *
 */

#include "grid.h"
#include "hit.h"
#include "adrt_struct.h"
#include <stdio.h>

#define GRID 5.0
#define LINE 0.4
void render_grid_init(render_t *render, char *usr) {
    render->work = render_grid_work;
    render->free = render_grid_free;
}


void render_grid_free(render_t *render) {
}

void render_grid_work(render_t *render, tie_t *tie, tie_ray_t *ray, TIE_3 *pixel) {
    tie_id_t id;
    adrt_mesh_t *m;
    TIE_3 vec;
    tfloat angle;


    if ((m = (adrt_mesh_t *)tie_work(tie, ray, &id, render_hit, NULL))) {
	/* if X or Y lie in the grid paint it white else make it gray */
	if (fabs(GRID*id.pos.v[0] - (int)(GRID*id.pos.v[0])) < 0.2*LINE || fabs(GRID*id.pos.v[1] - (int)(GRID*id.pos.v[1])) < 0.2*LINE) {
	    pixel->v[0] = 0.9;
	    pixel->v[1] = 0.9;
	    pixel->v[2] = 0.9;
	} else {
	    pixel->v[0] = 0.1;
	    pixel->v[1] = 0.1;
	    pixel->v[2] = 0.1;
	}
    } else {
	return;
    }

    VSUB2(vec.v,  ray->pos.v,  id.pos.v);
    VUNITIZE(vec.v);
    angle = VDOT( vec.v,  id.norm.v);
    VSCALE((*pixel).v,  (*pixel).v,  (angle*0.9));

    pixel->v[0] += 0.1;
    pixel->v[1] += 0.1;
    pixel->v[2] += 0.1;
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
