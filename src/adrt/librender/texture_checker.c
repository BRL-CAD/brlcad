/*                     T E X T U R E _ C H E C K E R . C
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
/** @file librender/texture_checker.c
 *
 *  Comments -
 *      Texture Library - Checker pattern with tile parameter
 *
 */

#include "texture.h"
#include <stdlib.h>
#include "adrt_struct.h"

#include "bu.h"

void
texture_checker_init(struct texture_s *texture, int tile) {
    struct texture_checker_s   *td;

    texture->data = bu_malloc(sizeof(struct texture_checker_s), "checker data");
    texture->free = texture_checker_free;
    texture->work = (texture_work_t *)texture_checker_work;

    td = (struct texture_checker_s *)texture->data;
    td->tile = tile;
}

void
texture_checker_free(struct texture_s *texture) {
    bu_free(texture->data, "checker data");
}

void
texture_checker_work(struct texture_s *texture, void *mesh, struct tie_ray_s *UNUSED(ray), struct tie_id_s *id, vect_t *pixel) {
    struct texture_checker_s	*td;
    vect_t			pt;
    int			u, v;


    td = (struct texture_checker_s *)texture->data;

    /* Transform the Point */
    MATH_VEC_TRANSFORM(pt, id->pos, ADRT_MESH(mesh)->matinv);

    if (pt[0]+TIE_PREC > ADRT_MESH(mesh)->max[0]) pt[0] = ADRT_MESH(mesh)->max[0];
    if (pt[1]+TIE_PREC > ADRT_MESH(mesh)->max[1]) pt[1] = ADRT_MESH(mesh)->max[1];
    u = ADRT_MESH(mesh)->max[0] - ADRT_MESH(mesh)->min[0] > 0 ? (int)((pt[0] - ADRT_MESH(mesh)->min[0]) / ((ADRT_MESH(mesh)->max[0] - ADRT_MESH(mesh)->min[0])/td->tile))%2 : 0;
    v = ADRT_MESH(mesh)->max[1] - ADRT_MESH(mesh)->min[1] > 0 ? (int)((pt[1] - ADRT_MESH(mesh)->min[1]) / ((ADRT_MESH(mesh)->max[1] - ADRT_MESH(mesh)->min[1])/td->tile))%2 : 0;

    *pixel[0] = *pixel[1] = *pixel[2] = u ^ v ? 1.0 : 0.0;
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
