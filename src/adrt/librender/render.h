/*                        R E N D E R . H
 * BRL-CAD / ADRT
 *
 * Copyright (c) 2007-2010 United States Government as represented by
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
/** @file render.h
 *
 */

#ifndef _RENDER_H
#define _RENDER_H

#include "render_internal.h"

RENDER_SHADER(component);
RENDER_SHADER(cut);
RENDER_SHADER(depth);
RENDER_SHADER(flat);
RENDER_SHADER(flos);
RENDER_SHADER(grid);
RENDER_SHADER(normal);
RENDER_SHADER(path);
RENDER_SHADER(phong);
RENDER_SHADER(spall);
RENDER_SHADER(surfel);

void* render_hit(tie_ray_t *ray, tie_id_t *id, tie_tri_t *tri, void *ptr);

#endif

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
