/*                        N O R M A L . C
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
/** @file normal.c
 *
 *  Author -
 *      Justin L. Shumaker
 *
 */

#include "normal.h"
#include "hit.h"
#include "adrt_common.h"
#include <stdio.h>


void render_normal_init(render_t *render) {
  render->work = render_normal_work;
  render->free = render_normal_free;
}


void render_normal_free(render_t *render) {
}


static void* normal_hit(tie_ray_t *ray, tie_id_t *id, tie_tri_t *tri, void *ptr) {
  common_triangle_t *t = ((common_triangle_t *)(tri->ptr));

  if(t->normals) {
    id->norm.v[0] = (1.0 - (id->alpha + id->beta)) * t->normals[0] + id->alpha * t->normals[3] + id->beta * t->normals[6];
    id->norm.v[1] = (1.0 - (id->alpha + id->beta)) * t->normals[1] + id->alpha * t->normals[4] + id->beta * t->normals[7];
    id->norm.v[2] = (1.0 - (id->alpha + id->beta)) * t->normals[2] + id->alpha * t->normals[5] + id->beta * t->normals[8];
  }

  return( t->mesh );
}


void render_normal_work(render_t *render, tie_t *tie, tie_ray_t *ray, TIE_3 *pixel) {
  tie_id_t	id;
  common_mesh_t	*m;

  if((m = (common_mesh_t *)tie_work(tie, ray, &id, normal_hit, NULL))) {
    pixel->v[0] = (id.norm.v[0]+1) * 0.5;
    pixel->v[1] = (id.norm.v[1]+1) * 0.5;
    pixel->v[2] = (id.norm.v[2]+1) * 0.5;
  }
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
