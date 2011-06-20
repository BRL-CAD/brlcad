/*                     T E X T U R E _ B U M P . C
 * BRL-CAD / ADRT
 *
 * Copyright (c) 2002-2011 United States Government as represented by
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
/** @file librender/texture_bump.c
 *
 *  Comments -
 *      Texture Library - Bump Mapping maps R, G, Z to surface normal X, Y, Z
 *
 */

#include "texture.h"
#include <stdlib.h>

#include "bu.h"

void
texture_bump_init(struct texture_s *texture, vect_t coef) {
    struct texture_bump_s *sd;

    texture->data = bu_malloc(sizeof(struct texture_bump_s), "texture data");
    texture->free = texture_bump_free;
    texture->work = (texture_work_t *)texture_bump_work;

    sd = (struct texture_bump_s *)texture->data;
    VMOVE(sd->coef, coef);
}

void
texture_bump_free(struct texture_s *texture) {
    bu_free(texture->data, "texture data");
}

void
texture_bump_work(struct texture_s *texture, void *UNUSED(mesh), struct tie_ray_s *UNUSED(ray), struct tie_id_s *id, vect_t *pixel) {
    struct texture_bump_s *sd;
    vect_t n;
    fastf_t d;

    sd = (struct texture_bump_s *)texture->data;

    n[0] = id->norm[0] + sd->coef[0]*(2* *pixel[0]-1.0);
    n[1] = id->norm[1] + sd->coef[1]*(2* *pixel[1]-1.0);
    n[2] = id->norm[2] + sd->coef[2]*(2* *pixel[2]-1.0);
    VUNITIZE(n);

    d = VDOT( n,  id->norm);
    if (d < 0)
	VSCALE(n,  n,  -1.0);
    VMOVE(id->norm, n);
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
