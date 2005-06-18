/*                     T E X T U R E _ C L O U D S . H
 *
 * @file texture_clouds.h
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
 *      Texture Library - Perlin Clouds Header
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

#ifndef _TEXTURE_CLOUDS_H
#define _TEXTURE_CLOUDS_H


#include "texture.h"
#include "texture_perlin.h"


typedef struct texture_clouds_s {
  tfloat size;
  int octaves;
  int absolute;
  TIE_3	scale;
  TIE_3 translate;
  texture_perlin_t perlin;
} texture_clouds_t;


extern	void	texture_clouds_init(texture_t *texture, tfloat size, int octaves, int absolute, TIE_3 scale, TIE_3 translate);
extern	void	texture_clouds_free(texture_t *texture);
extern	void	texture_clouds_work(texture_t *texture, common_mesh_t *mesh, tie_ray_t *ray, tie_id_t *id, TIE_3 *pixel);


#endif
