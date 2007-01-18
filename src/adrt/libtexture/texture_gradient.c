/*                     T E X T U R E _ G R A D I E N T . C
 *
 * @file texture_gradient.c
 *
 * BRL-CAD
 *
 * Copyright (c) 2002-2006 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 *
 *  Comments -
 *      Texture Library - Produces Gradient to be used with Blend
 *
 *  Author -
 *      Justin L. Shumaker
 *
 *  Source -
 *      The U. S. Army Research Laboratory
 *      Aberdeen Proving Ground, Maryland  21005-5068  USA
 *
 * $Id$
 */

#include "texture_gradient.h"
#include <stdlib.h>
#include "umath.h"


void texture_gradient_init(texture_t *texture, int axis);
void texture_gradient_free(texture_t *texture);
void texture_gradient_work(texture_t *texture, common_mesh_t *mesh, tie_ray_t *ray, tie_id_t *id, TIE_3 *pixel);


void texture_gradient_init(texture_t *texture, int axis) {
  texture_gradient_t *td;

  texture->data = malloc(sizeof(texture_gradient_t));
  texture->free = texture_gradient_free;
  texture->work = texture_gradient_work;

  td = (texture_gradient_t *)texture->data;
  td->axis = axis;
}


void texture_gradient_free(texture_t *texture) {
  free(texture->data);
}


void texture_gradient_work(texture_t *texture, common_mesh_t *mesh, tie_ray_t *ray, tie_id_t *id, TIE_3 *pixel) {
  texture_gradient_t *td;
  TIE_3 pt;


  td = (texture_gradient_t *)texture->data;

  /* Transform the Point */
  MATH_VEC_TRANSFORM(pt, id->pos, mesh->matinv);

  if(td->axis == 1) {
    pixel->v[0] = pixel->v[1] = pixel->v[2] = mesh->max.v[1] - mesh->min.v[1] > TIE_PREC ? (pt.v[1] - mesh->min.v[1]) / (mesh->max.v[1] - mesh->min.v[1]) : 0.0;
  } else if(td->axis == 2) {
    pixel->v[0] = pixel->v[1] = pixel->v[2] = mesh->max.v[2] - mesh->min.v[2] > TIE_PREC ? (pt.v[2] - mesh->min.v[2]) / (mesh->max.v[2] - mesh->min.v[1]) : 0.0;
  } else {
    pixel->v[0] = pixel->v[1] = pixel->v[2] = mesh->max.v[0] - mesh->min.v[0] > TIE_PREC ? (pt.v[0] - mesh->min.v[0]) / (mesh->max.v[0] - mesh->min.v[1]) : 0.0;
  }
}
