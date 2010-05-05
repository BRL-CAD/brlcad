/*                          F L O S . H
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
/** @file flos.h
 *
 * Brief description
 *
 */

#ifndef _RENDER_FLOS_H
#define _RENDER_FLOS_H

#include "render_internal.h"

typedef struct render_flos_s {
    TIE_3 frag_pos;
} render_flos_t;

BU_EXPORT BU_EXTERN(void render_flos_init, (render_t *render, char *frag_pos));
BU_EXPORT BU_EXTERN(void render_flos_free, (render_t *render));
BU_EXPORT BU_EXTERN(void render_flos_work, (render_t *render, tie_t *tie, tie_ray_t *ray, TIE_3 *pixel));

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
