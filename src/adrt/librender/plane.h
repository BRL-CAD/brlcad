/*                         P L A N E . H
 * BRL-CAD
 *
 * Copyright (c) 2007 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation.
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
/** @file plane.h
 *
 *  Author -
 *      Justin L. Shumaker
 *
 */

#ifndef _RENDER_PLANE_H
#define _RENDER_PLANE_H

#include "render_internal.h"

typedef struct render_plane_s {
  TIE_3 ray_pos;
  TIE_3 ray_dir;
  tfloat plane[4];
  tie_t tie;
} render_plane_t;

void render_plane_init(render_t *render, TIE_3 ray_pos, TIE_3 ray_dir);
void render_plane_free(render_t *render);
void render_plane_work(render_t *render, tie_t *tie, tie_ray_t *ray, TIE_3 *pixel);

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
