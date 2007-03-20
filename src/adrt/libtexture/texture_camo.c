/*                     T E X T U R E _ C A M O . C
 * BRL-CAD / ADRT
 *
 * Copyright (c) 2002-2007 United States Government as represented by
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
/** @file texture_camo.h
 *
 *  Comments -
 *      Texture Library - Applies a 3 color camoflauge
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

#include "texture_camo.h"
#include <stdlib.h>
#include "umath.h"


void texture_camo_init(texture_t *texture, TFLOAT size, int octaves, int absolute, TIE_3 color1, TIE_3 color2, TIE_3 color3);
void texture_camo_free(texture_t *texture);
void texture_camo_work(texture_t *texture, common_mesh_t *mesh, tie_ray_t *ray, tie_id_t *id, TIE_3 *pixel);


void texture_camo_init(texture_t *texture, TFLOAT size, int octaves, int absolute, TIE_3 color1, TIE_3 color2, TIE_3 color3) {
  texture_camo_t   *sd;

  texture->data = malloc(sizeof(texture_camo_t));
  if (!texture->data) {
      perror("texture->data");
      exit(1);
  }
  texture->free = texture_camo_free;
  texture->work = texture_camo_work;

  sd = (texture_camo_t *)texture->data;
  sd->size = size;
  sd->octaves = octaves;
  sd->absolute = absolute;
  sd->color1 = color1;
  sd->color2 = color2;
  sd->color3 = color3;

  texture_perlin_init(&sd->perlin);
}


void texture_camo_free(texture_t *texture) {
  texture_camo_t *td;

  td = (texture_camo_t *)texture->data;
  texture_perlin_free(&td->perlin);
  free(texture->data);
}


void texture_camo_work(texture_t *texture, common_mesh_t *mesh, tie_ray_t *ray, tie_id_t *id, TIE_3 *pixel) {
  texture_camo_t *td;
  TIE_3 p, pt;
  TFLOAT sum1, sum2;


  td = (texture_camo_t *)texture->data;

  /* Transform the Point */
  MATH_VEC_TRANSFORM(pt, id->pos, mesh->matinv);
  if(td->absolute) {
    p = id->pos;
  } else {
    p.v[0] = mesh->max.v[0] - mesh->min.v[0] > TIE_PREC ? (pt.v[0] - mesh->min.v[0]) / (mesh->max.v[0] - mesh->min.v[0]) : 0.0;
    p.v[1] = mesh->max.v[1] - mesh->min.v[1] > TIE_PREC ? (pt.v[1] - mesh->min.v[1]) / (mesh->max.v[1] - mesh->min.v[1]) : 0.0;
    p.v[2] = mesh->max.v[2] - mesh->min.v[2] > TIE_PREC ? (pt.v[2] - mesh->min.v[2]) / (mesh->max.v[2] - mesh->min.v[2]) : 0.0;
  }

  sum1 = fabs(texture_perlin_noise3(&td->perlin, p, td->size*1.0, td->octaves));
  sum2 = fabs(texture_perlin_noise3(&td->perlin, p, td->size*0.8, td->octaves+1));

  if(sum1 < 0.3) {
    *pixel = td->color1;
  } else {
    *pixel = td->color2;
  }

  if(sum2 < 0.3)
    *pixel = td->color3;
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
