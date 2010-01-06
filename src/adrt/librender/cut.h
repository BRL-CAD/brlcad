/*                        C U T . H
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
/** @file cut.h
 *
 */

#ifndef _RENDER_CUT_H
#define _RENDER_CUT_H

#include "render_internal.h"

typedef struct render_cut_s {
    TIE_3 ray_pos;
    TIE_3 ray_dir;
    tfloat plane[4];
    tie_t tie;
} render_cut_t;

void render_cut_init(render_t *render, char *buf);	/* requires two vectors; pos and dir */
void render_cut_free(render_t *render);
void render_cut_work(render_t *render, tie_t *tie, tie_ray_t *ray, TIE_3 *pixel);

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
