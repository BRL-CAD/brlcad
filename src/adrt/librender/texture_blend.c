/*                     T E X T U R E _ B L E N D . C
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
/** @file librender/texture_blend.c
 *
 *  Comments -
 *      Texture Library - Uses the R and B channels to blend 2 colors
 *
 */

#include "texture.h"
#include <stdlib.h>

#include "bu.h"

void
texture_blend_init(struct texture_s *texture, vect_t color1, vect_t color2) {
    struct texture_blend_s *sd;

    texture->data = bu_malloc(sizeof(struct texture_blend_s), "texture data");
    texture->free = texture_blend_free;
    texture->work = (texture_work_t *)texture_blend_work;

    sd = (struct texture_blend_s *)texture->data;
    VMOVE(sd->color1, color1);
    VMOVE(sd->color2, color2);
}

void
texture_blend_free(struct texture_s *texture) {
    bu_free(texture->data, "texture data");
}

void
texture_blend_work(struct texture_s *texture, void *UNUSED(mesh), struct tie_ray_s *UNUSED(ray), struct tie_id_s *UNUSED(id), vect_t *pixel) {
    struct texture_blend_s *sd;
    fastf_t coef;

    sd = (struct texture_blend_s *)texture->data;

    coef = *pixel[0];
    *pixel[0] = (1.0 - coef)*sd->color1[0] + coef*sd->color2[0];
    *pixel[1] = (1.0 - coef)*sd->color1[1] + coef*sd->color2[1];
    *pixel[2] = (1.0 - coef)*sd->color1[2] + coef*sd->color2[2];
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
