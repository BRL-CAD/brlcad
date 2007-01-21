/*                     T E X T U R E _ S T A C K . C
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
/** @file texture_stack.c
 *
 *  Comments -
 *      Texture Library - Stack textures to pipe output of one into another
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

#include "texture_stack.h"
#include <stdlib.h>


void texture_stack_init(texture_t *texture);
void texture_stack_free(texture_t *texture);
void texture_stack_work(texture_t *texture, common_mesh_t *mesh, tie_ray_t *ray, tie_id_t *id, TIE_3 *pixel);
void texture_stack_push(texture_t *texture, texture_t *texture_new);


void texture_stack_init(texture_t *texture) {
  texture_stack_t *td;

  texture->data = malloc(sizeof(texture_stack_t));
  texture->free = texture_stack_free;
  texture->work = texture_stack_work;

  td = (texture_stack_t *)texture->data;
  td->num = 0;
  td->list = NULL;
}


void texture_stack_free(texture_t *texture) {
  texture_stack_t *td;

  td = (texture_stack_t *)texture->data;
  free(td->list);
  free(texture->data);
}


void texture_stack_work(texture_t *texture, common_mesh_t *mesh, tie_ray_t *ray, tie_id_t *id, TIE_3 *pixel) {
  texture_stack_t *td;
  int i;

  td = (texture_stack_t *)texture->data;

  for(i = td->num-1; i >= 0; i--)
    td->list[i]->work(td->list[i], mesh, ray, id, pixel);
}


void texture_stack_push(texture_t *texture, texture_t *texture_new) {
  texture_stack_t *td;

  td = (texture_stack_t *)texture->data;

  td->list = (texture_t **)realloc(td->list, sizeof(texture_t *)*(td->num+1));
  td->list[td->num++] = texture_new;
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
