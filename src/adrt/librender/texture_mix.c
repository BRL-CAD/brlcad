/*                     T E X T U R E _ M I X . C
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
/** @file librender/texture_mix.c
 *
 *  Comments -
 *      Texture Library - Mix two textures
 *
 */

#include "texture.h"
#include <stdlib.h>
#include "adrt_struct.h"

#include "bu.h"

void
texture_mix_init(struct texture_s *texture, struct texture_s *texture1, struct texture_s *texture2, fastf_t coef) {
    struct texture_mix_s *td;

    texture->data = bu_malloc(sizeof(struct texture_mix_s), "texture data");
    texture->free = texture_mix_free;
    texture->work = (texture_work_t *)texture_mix_work;

    td = (struct texture_mix_s *)texture->data;
    td->texture1 = texture1;
    td->texture2 = texture2;
    td->coef = coef;
}

void
texture_mix_free(struct texture_s *texture) {
    bu_free(texture->data, "texture data");
}

void
texture_mix_work(struct texture_s *texture, void *mesh, struct tie_ray_s *ray, struct tie_id_s *id, vect_t *pixel) {
    struct texture_mix_s *td;
    vect_t t;

    td = (struct texture_mix_s *)texture->data;

    td->texture1->work(td->texture1, ADRT_MESH(mesh), ray, id, pixel);
    td->texture2->work(td->texture2, ADRT_MESH(mesh), ray, id, &t);
    VSCALE((*pixel),  (*pixel),  td->coef);
    VSCALE(t,  t,  (1.0 - td->coef));
    VADD2((*pixel),  (*pixel),  t);
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
