/*                     T E X T U R E _ B U M P . C
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
/** @file texture_bump.c
 *
 *  Comments -
 *      Texture Library - Bump Mapping maps R,G,Z to surface normal X,Y,Z
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

#include "texture_bump.h"
#include <stdlib.h>
#include "umath.h"


void texture_bump_init(texture_t *texture, TIE_3 coef);
void texture_bump_free(texture_t *texture);
void texture_bump_work(texture_t *texture, common_mesh_t *mesh, tie_ray_t *ray, tie_id_t *id, TIE_3 *pixel);


void texture_bump_init(texture_t *texture, TIE_3 coef) {
  texture_bump_t *sd;

  texture->data = malloc(sizeof(texture_bump_t));
  texture->free = texture_bump_free;
  texture->work = texture_bump_work;

  sd = (texture_bump_t *)texture->data;
  sd->coef = coef;
}


void texture_bump_free(texture_t *texture) {
  free(texture->data);
}


void texture_bump_work(texture_t *texture, common_mesh_t *mesh, tie_ray_t *ray, tie_id_t *id, TIE_3 *pixel) {
  texture_bump_t *sd;
  TIE_3 n;
  TFLOAT d;


  sd = (texture_bump_t *)texture->data;


  n.v[0] = id->norm.v[0] + sd->coef.v[0]*(2*pixel->v[0]-1.0);
  n.v[1] = id->norm.v[1] + sd->coef.v[1]*(2*pixel->v[1]-1.0);
  n.v[2] = id->norm.v[2] + sd->coef.v[2]*(2*pixel->v[2]-1.0);
  MATH_VEC_UNITIZE(n);

  MATH_VEC_DOT(d, n, id->norm);
  if(d < 0)
    MATH_VEC_MUL_SCALAR(n, n, -1.0);
  id->norm = n;
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
