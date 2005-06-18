/*                     T E X T U R E _ M I X . C
 *
 * @file texture_mix.c
 *
 * BRL-CAD
 *
 * Copyright (C) 2002-2005 United States Government as represented by
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
 *      Texture Library - Mix two textures
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

#include "texture_mix.h"
#include <stdlib.h>
#include "umath.h"


void texture_mix_init(texture_t *texture, texture_t *texture1, texture_t *texture2, tfloat coef);
void texture_mix_free(texture_t *texture);
void texture_mix_work(texture_t *texture, common_mesh_t *mesh, tie_ray_t *ray, tie_id_t *id, TIE_3 *pixel);


void texture_mix_init(texture_t *texture, texture_t *texture1, texture_t *texture2, tfloat coef) {
  texture_mix_t *td;

  texture->data = malloc(sizeof(texture_mix_t));
  texture->free = texture_mix_free;
  texture->work = texture_mix_work;

  td = (texture_mix_t *)texture->data;
  td->texture1 = texture1;
  td->texture2 = texture2;
  td->coef = coef;
}


void texture_mix_free(texture_t *texture) {
  free(texture->data);
}


void texture_mix_work(texture_t *texture, common_mesh_t *mesh, tie_ray_t *ray, tie_id_t *id, TIE_3 *pixel) {
  texture_mix_t *td;
  TIE_3 t;
  int i;


  td = (texture_mix_t *)texture->data;

  td->texture1->work(td->texture1, mesh, ray, id, pixel);
  td->texture2->work(td->texture2, mesh, ray, id, &t);
  math_vec_mul_scalar((*pixel), (*pixel), td->coef);
  math_vec_mul_scalar(t, t, (1.0 - td->coef));
  math_vec_add((*pixel), (*pixel), t);
}
