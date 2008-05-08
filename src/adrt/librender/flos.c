/*                        F L O S . C
 * BRL-CAD / ADRT
 *
 * Copyright (c) 2007-2008 United States Government as represented by
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

#include "component.h"
#include "cut.h"
#include "depth.h"
#include "flat.h"
#include "flos.h"


#include "flos.h"
#include "hit.h"
#include "adrt_struct.h"

#include <stdio.h>
#include <stdlib.h>

#include "bu.h"

void render_flos_init(render_t *render, TIE_3 frag_pos) {
    render_flos_t *d;

    render->work = render_flos_work;
    render->free = render_flos_free;
    render->data = (render_flos_t *)bu_malloc(sizeof(render_flos_t), "render_flos_init");
    d = (render_flos_t *)render->data;
    d->frag_pos = frag_pos;
}


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
	MATH_VEC_SET((*pixel), 0.0, 0.5, 0.0);
    } else {
	return;
    }

    MATH_VEC_SUB(vec, ray->pos, id.pos);
    MATH_VEC_UNITIZE(vec);
    MATH_VEC_DOT(angle, vec, id.norm);

    /* Determine if direct line of sight to fragment */
    ray->pos = rd->frag_pos;
    MATH_VEC_SUB(ray->dir, id.pos, rd->frag_pos);
    MATH_VEC_UNITIZE(ray->dir);

    if (tie_work(tie, ray, &tid, render_hit, NULL)) {
	if (fabs (id.pos.v[0] - tid.pos.v[0]) < TIE_PREC &&
	    fabs (id.pos.v[1] - tid.pos.v[1]) < TIE_PREC &&
	    fabs (id.pos.v[2] - tid.pos.v[2]) < TIE_PREC)
	{
	    MATH_VEC_SET((*pixel), 1.0, 0.0, 0.0);
	}
    }

    MATH_VEC_MUL_SCALAR((*pixel), (*pixel), (0.5+angle*0.5));
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
