/*                         D E P T H . H
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
/** @file depth.h
 *
 *  Author -
 *      Justin L. Shumaker
 *
 */

#ifndef _RENDER_DEPTH_H
#define _RENDER_DEPTH_H

#include "render_internal.h"

void render_depth_init(render_t *render);
void render_depth_free(render_t *render);
void render_depth_work(render_t *render, tie_t *tie, tie_ray_t *ray, TIE_3 *pixel);

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
