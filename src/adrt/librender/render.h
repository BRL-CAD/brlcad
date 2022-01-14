/*                        R E N D E R . H
 * BRL-CAD / ADRT
 *
 * Copyright (c) 2007-2022 United States Government as represented by
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
/** @file librender/render.h
 *
 */

#ifndef ADRT_LIBRENDER_RENDER_H
#define ADRT_LIBRENDER_RENDER_H

#include "render_internal.h"

RENDER_EXPORT extern int render_component_init(render_t *, const char *);
RENDER_EXPORT extern int render_cut_init(render_t *, const char *);
RENDER_EXPORT extern int render_depth_init(render_t *, const char *);
RENDER_EXPORT extern int render_flat_init(render_t *, const char *);
RENDER_EXPORT extern int render_flos_init(render_t *, const char *);
RENDER_EXPORT extern int render_grid_init(render_t *, const char *);
RENDER_EXPORT extern int render_normal_init(render_t *, const char *);
RENDER_EXPORT extern int render_path_init(render_t *, const char *);
RENDER_EXPORT extern int render_phong_init(render_t *, const char *);
RENDER_EXPORT extern int render_spall_init(render_t *, const char *);
RENDER_EXPORT extern int render_surfel_init(render_t *, const char *);

RENDER_EXPORT void* render_hit(struct tie_ray_s *ray, struct tie_id_s *id, struct tie_tri_s *tri, void *ptr);

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
