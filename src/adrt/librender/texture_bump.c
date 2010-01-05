/*                     T E X T U R E _ B U M P . C
 * BRL-CAD / ADRT
 *
 * Copyright (c) 2002-2010 United States Government as represented by
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
/** @file texture_bump.c
 *
 *  Comments -
 *      Texture Library - Bump Mapping maps R, G, Z to surface normal X, Y, Z
 *
 */

#include "texture.h"
#include <stdlib.h>

#include "bu.h"

void texture_bump_init(texture_t *texture, TIE_3 coef) {
    texture_bump_t *sd;

    texture->data = bu_malloc(sizeof(texture_bump_t), "texture data");
    texture->free = texture_bump_free;
    texture->work = (texture_work_t *)texture_bump_work;

    sd = (texture_bump_t *)texture->data;
    sd->coef = coef;
}


void texture_bump_free(texture_t *texture) {
    bu_free(texture->data, "texture data");
}


void texture_bump_work(__TEXTURE_WORK_PROTOTYPE__) {
    texture_bump_t *sd;
    TIE_3 n;
    tfloat d;


    sd = (texture_bump_t *)texture->data;


    n.v[0] = id->norm.v[0] + sd->coef.v[0]*(2*pixel->v[0]-1.0);
    n.v[1] = id->norm.v[1] + sd->coef.v[1]*(2*pixel->v[1]-1.0);
    n.v[2] = id->norm.v[2] + sd->coef.v[2]*(2*pixel->v[2]-1.0);
    VUNITIZE(n.v);

    d = VDOT( n.v,  id->norm.v);
    if (d < 0)
	VSCALE(n.v,  n.v,  -1.0);
    id->norm = n;
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
