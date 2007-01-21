/*                     T E X T U R E _ B L E N D . C
 * BRL-CAD
 *
 * Copyright (c) 2002-2007 United States Government as represented by
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
 */
/** @file texture_blend.c
 *
 *  Comments -
 *      Texture Library - Uses the R and B channels to blend 2 colors
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

#include "texture_blend.h"
#include <stdlib.h>


void texture_blend_init(texture_t *texture, TIE_3 color1, TIE_3 color2);
void texture_blend_free(texture_t *texture);
void texture_blend_work(texture_t *texture, common_mesh_t *mesh, tie_ray_t *ray, tie_id_t *id, TIE_3 *pixel);


void texture_blend_init(texture_t *texture, TIE_3 color1, TIE_3 color2) {
  texture_blend_t *sd;

  texture->data = malloc(sizeof(texture_blend_t));
  texture->free = texture_blend_free;
  texture->work = texture_blend_work;

  sd = (texture_blend_t *)texture->data;
  sd->color1 = color1;
  sd->color2 = color2;
}


void texture_blend_free(texture_t *texture) {
  free(texture->data);
}


void texture_blend_work(texture_t *texture, common_mesh_t *mesh, tie_ray_t *ray, tie_id_t *id, TIE_3 *pixel) {
  texture_blend_t *sd;
  tfloat coef;

  sd = (texture_blend_t *)texture->data;

  coef = pixel->v[0];
  pixel->v[0] = (1.0 - coef)*sd->color1.v[0] + coef*sd->color2.v[0];
  pixel->v[1] = (1.0 - coef)*sd->color1.v[1] + coef*sd->color2.v[1];
  pixel->v[2] = (1.0 - coef)*sd->color1.v[2] + coef*sd->color2.v[2];
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
