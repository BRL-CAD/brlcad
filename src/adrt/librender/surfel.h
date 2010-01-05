/*                        S U R F E L . H
 * BRL-CAD
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
/** @file surfel.h
 *
 * Brief description
 *
 */

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

#ifndef _RENDER_SURFEL_H
#define _RENDER_SURFEL_H

#include "render_internal.h"

typedef struct render_surfel_pt_s {
    TIE_3 pos;
    tfloat radius;
    TIE_3 color;
} render_surfel_pt_t;


typedef struct render_surfel_s {
    uint32_t num;
    render_surfel_pt_t *list;
} render_surfel_t;


void render_surfel_init(render_t *render, uint32_t num, render_surfel_pt_t *list);
void render_surfel_free(render_t *render);
void render_surfel_work(render_t *render, tie_t *tie, tie_ray_t *ray, TIE_3 *pixel);

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
