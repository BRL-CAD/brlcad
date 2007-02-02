/*                     T E X T U R E _ M I X . H
 * BRL-CAD
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
/** @file texture_mix.h
 *
 *  Comments -
 *      Texture Library - Mix Header
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

#ifndef _TEXTURE_MIX_H
#define _TEXTURE_MIX_H


#include "texture.h"


typedef struct texture_mix_s {
  texture_t *texture1;
  texture_t *texture2;
  TFLOAT coef;
} texture_mix_t;


extern	void	texture_mix_init(texture_t *texture, texture_t *texture1, texture_t *texture2, TFLOAT coef);
extern	void	texture_mix_free(texture_t *texture);
extern	void	texture_mix_work(texture_t *texture, common_mesh_t *mesh, tie_ray_t *ray, tie_id_t *id, TIE_3 *pixel);


#endif

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
