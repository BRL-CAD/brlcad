/*                     T E X T U R E _ I M A G E . C
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
/**
 * @file librender/texture_image.c
 *
 *  Comments -
 *      Texture Library - Projects an Image onto a Surface
 *
 */

#include "texture.h"

#include <stdlib.h>
#include <string.h>
#include "adrt_struct.h"

#include "bu.h"

void
texture_image_init(struct texture_s *texture, short w, short h, unsigned char *image) {
    struct texture_image_s *td;

    texture->data = bu_malloc(sizeof(struct texture_image_s), "texture data");
    texture->free = texture_image_free;
    texture->work = (texture_work_t *)texture_image_work;

    td = (struct texture_image_s *)texture->data;
    td->w = w;
    td->h = h;
    td->image = (unsigned char *)bu_malloc(3*w*h, "texture image");
    memcpy(td->image, image, 3*w*h);
}

void
texture_image_free(struct texture_s *texture) {
    struct texture_image_s *td;

    td = (struct texture_image_s *)texture->data;
    bu_free(td->image, "texture image");
    bu_free(texture->data, "texture data");
}

void
texture_image_work(struct texture_s *texture, void *mesh, struct tie_ray_s *UNUSED(ray), struct tie_id_s *id, vect_t *pixel) {
    struct texture_image_s *td;
    vect_t pt;
    fastf_t u, v;
    int ind;


    td = (struct texture_image_s *)texture->data;


    /* Transform the Point */
    MATH_VEC_TRANSFORM(pt, id->pos, ADRT_MESH(mesh)->matinv);
    u = ADRT_MESH(mesh)->max[0] - ADRT_MESH(mesh)->min[0] > TIE_PREC ? (pt[0] - ADRT_MESH(mesh)->min[0]) / (ADRT_MESH(mesh)->max[0] - ADRT_MESH(mesh)->min[0]) : 0.0;
    v = ADRT_MESH(mesh)->max[1] - ADRT_MESH(mesh)->min[1] > TIE_PREC ? (pt[1] - ADRT_MESH(mesh)->min[1]) / (ADRT_MESH(mesh)->max[1] - ADRT_MESH(mesh)->min[1]) : 0.0;

    ind = 3*((int)((1.0 - v)*td->h)*td->w + (int)(u*td->w));

    *pixel[0] = td->image[ind+2] / 255.0;
    *pixel[1] = td->image[ind+1] / 255.0;
    *pixel[2] = td->image[ind+0] / 255.0;
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
