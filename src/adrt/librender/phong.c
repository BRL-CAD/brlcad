/*                         P H O N G . C
 * BRL-CAD
 *
 * Copyright (c) 2007 United States Government as represented by
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
/** @file phong.c
 *
 *  Author -
 *      Justin L. Shumaker
 *
 */

#include "phong.h"
#include "hit.h"
#include "adrt_common.h"
#include <stdio.h>


void render_phong_init(render_t *render) {
  render->work = render_phong_work;
  render->free = render_phong_free;
}


void render_phong_free(render_t *render) {
}


void render_phong_work(render_t *render, tie_t *tie, tie_ray_t *ray, TIE_3 *pixel) {
  tie_id_t		id;
  common_mesh_t		*m;
  TIE_3			vec;
  TFLOAT		angle;

  if((m = (common_mesh_t*)tie_work(tie, ray, &id, render_hit, NULL))) {
    *pixel = m->prop->color;
    if(m->texture)
      m->texture->work(m->texture, m, ray, &id, pixel);
  } else {
    return;
  }

  MATH_VEC_SUB(vec, ray->pos, id.pos);
  MATH_VEC_UNITIZE(vec);
  MATH_VEC_DOT(angle, vec, id.norm);
  MATH_VEC_MUL_SCALAR((*pixel), (*pixel), angle);
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
