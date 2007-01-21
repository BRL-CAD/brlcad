/*                     T E X T U R E _ I N T E R N A L . H
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
/** @file texture_internal.h
 *
 *  Comments -
 *      Texture Library - Internal texture include
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

#ifndef _TEXTURE_INTERNAL_H
#define _TEXTURE_INTERNAL_H


#include "tie.h"
#include "adrt_common.h"

struct texture_s;
struct mesh_s;
typedef void texture_init_t(struct texture_s *texture);
typedef void texture_free_t(struct texture_s *texture);
typedef void texture_work_t(struct texture_s *texture, struct mesh_s *mesh, tie_ray_t *ray, tie_id_t *id, TIE_3 *pixel);


typedef struct texture_s {
  texture_free_t *free;
  texture_work_t *work;
  void *data;
} texture_t;


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
