/*                         D E P T H . C
 * BRL-CAD / ADRT
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
/** @file depth.c
 *
 *  Author -
 *      Justin L. Shumaker
 *
 */

#include "depth.h"
#include "hit.h"
#include "adrt_common.h"
#include <stdio.h>


void render_depth_init(render_t *render) {
  render->work = render_depth_work;
  render->free = render_depth_free;
}


void render_depth_free(render_t *render) {
}


void render_depth_work(render_t *render, tie_t *tie, tie_ray_t *ray, TIE_3 *pixel) {
  tie_id_t id;
  common_mesh_t *mesh;

  /* Visualize ray depth, must put ray->depth++ hack into bsp for this to be of any use */
  if((mesh = (common_mesh_t *)tie_work(tie, ray, &id, render_hit, NULL)))
    pixel->v[0] = 0.0075 * ray->kdtree_depth;
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
