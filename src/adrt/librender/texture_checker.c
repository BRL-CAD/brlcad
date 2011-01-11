/*                     T E X T U R E _ C H E C K E R . C
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
/** @file texture_checker.c
 *
 *  Comments -
 *      Texture Library - Checker pattern with tile parameter
 *
 */

#include "texture.h"
#include <stdlib.h>
#include "adrt_struct.h"

#include "bu.h"

void texture_checker_init(struct texture_s *texture, int tile) {
    struct texture_checker_s   *td;

    texture->data = bu_malloc(sizeof(struct texture_checker_s), "checker data");
    texture->free = texture_checker_free;
    texture->work = (struct texture_work_s *)texture_checker_work;

    td = (struct texture_checker_s *)texture->data;
    td->tile = tile;
}


void texture_checker_free(struct texture_s *texture) {
    bu_free(texture->data, "checker data");
}


void texture_checker_work(__TEXTURE_WORK_PROTOTYPE__) {
    struct texture_checker_s	*td;
    TIE_3			pt;
    int			u, v;


    td = (struct texture_checker_s *)texture->data;

    /* Transform the Point */
    MATH_VEC_TRANSFORM(pt, id->pos, ADRT_MESH(mesh)->matinv);

    if (pt.v[0]+TIE_PREC > ADRT_MESH(mesh)->max.v[0]) pt.v[0] = ADRT_MESH(mesh)->max.v[0];
    if (pt.v[1]+TIE_PREC > ADRT_MESH(mesh)->max.v[1]) pt.v[1] = ADRT_MESH(mesh)->max.v[1];
    u = ADRT_MESH(mesh)->max.v[0] - ADRT_MESH(mesh)->min.v[0] > 0 ? (int)((pt.v[0] - ADRT_MESH(mesh)->min.v[0]) / ((ADRT_MESH(mesh)->max.v[0] - ADRT_MESH(mesh)->min.v[0])/td->tile))%2 : 0;
    v = ADRT_MESH(mesh)->max.v[1] - ADRT_MESH(mesh)->min.v[1] > 0 ? (int)((pt.v[1] - ADRT_MESH(mesh)->min.v[1]) / ((ADRT_MESH(mesh)->max.v[1] - ADRT_MESH(mesh)->min.v[1])/td->tile))%2 : 0;

    pixel->v[0] = pixel->v[1] = pixel->v[2] = u ^ v ? 1.0 : 0.0;
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
