/*                         S P A L L . H
 * BRL-CAD
 *
 * Copyright (c) 2007 United States Government as represented by
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
/** @file spall.h
 *
 *  Author -
 *      Justin L. Shumaker
 *
 */

#ifndef _RENDER_SPALL_H
#define _RENDER_SPALL_H

#include "render_internal.h"

typedef struct render_spall_s {
  TIE_3 ray_pos;
  TIE_3 ray_dir;
  TFLOAT plane[4];
  TFLOAT angle;
  tie_t tie;
} render_spall_t;

void render_spall_init(render_t *render, TIE_3 ray_pos, TIE_3 ray_dir, TFLOAT angle);
void render_spall_free(render_t *render);
void render_spall_work(render_t *render, tie_t *tie, tie_ray_t *ray, TIE_3 *pixel);

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
