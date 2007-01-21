/*                          G R I D . C
 * BRL-CAD
 *
 * Copyright (c) 2007 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation.
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
 *  Author -
 *      Justin L. Shumaker
 *
 */

#include "grid.h"
#include "hit.h"
#include "adrt_common.h"
#include <stdio.h>

#define GRID 5.0
#define LINE 0.4
void render_grid_init(render_t *render) {
  render->work = render_grid_work;
  render->free = render_grid_free;
}


void render_grid_free(render_t *render) {
}

void render_grid_work(render_t *render, tie_t *tie, tie_ray_t *ray, TIE_3 *pixel) {
  tie_id_t id;
  common_mesh_t *m;
  TIE_3 vec;
  tfloat angle;


  if((m = (common_mesh_t *)tie_work(tie, ray, &id, render_hit, NULL))) {
    /* if X or Y lie in the grid paint it white else make it gray */
    if(fabs(GRID*id.pos.v[0] - (int)(GRID*id.pos.v[0])) < 0.2*LINE || fabs(GRID*id.pos.v[1] - (int)(GRID*id.pos.v[1])) < 0.2*LINE) {
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

  MATH_VEC_SUB(vec, ray->pos, id.pos);
  MATH_VEC_UNITIZE(vec);
  MATH_VEC_DOT(angle, vec, id.norm);
  MATH_VEC_MUL_SCALAR((*pixel), (*pixel), (angle*0.9));

  pixel->v[0] += 0.1;
  pixel->v[1] += 0.1;
  pixel->v[2] += 0.1;
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
