/*                     T E X T U R E _ M I X . C
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
/** @file texture_mix.c
 *
 *  Comments -
 *      Texture Library - Mix two textures
 *
 */

#include "texture.h"
#include <stdlib.h>
#include "adrt_struct.h"

#include "bu.h"

void texture_mix_init(struct texture_s *texture, struct texture_s *texture1, struct texture_s *texture2, tfloat coef) {
    struct texture_mix_s *td;

    texture->data = bu_malloc(sizeof(struct texture_mix_s), "texture data");
    texture->free = texture_mix_free;
    texture->work = (struct texture_work_s *)texture_mix_work;

    td = (struct texture_mix_s *)texture->data;
    td->texture1 = texture1;
    td->texture2 = texture2;
    td->coef = coef;
}


void texture_mix_free(struct texture_s *texture) {
    bu_free(texture->data, "texture data");
}


void texture_mix_work(__TEXTURE_WORK_PROTOTYPE__) {
    struct texture_mix_s *td;
    TIE_3 t;

    td = (struct texture_mix_s *)texture->data;

    td->texture1->work(td->texture1, ADRT_MESH(mesh), ray, id, pixel);
    td->texture2->work(td->texture2, ADRT_MESH(mesh), ray, id, &t);
    VSCALE((*pixel).v,  (*pixel).v,  td->coef);
    VSCALE(t.v,  t.v,  (1.0 - td->coef));
    VADD2((*pixel).v,  (*pixel).v,  t.v);
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
