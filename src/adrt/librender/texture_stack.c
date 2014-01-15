/*                     T E X T U R E _ S T A C K . C
 * BRL-CAD / ADRT
 *
 * Copyright (c) 2002-2014 United States Government as represented by
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
/** @file librender/texture_stack.c
 *
 * Comments -
 * Texture Library - Stack textures to pipe output of one into another
 *
 */

#include "texture.h"
#include <stdlib.h>

#include "bu.h"





void
texture_stack_free(struct texture_s *texture) {
    struct texture_stack_s *td;

    td = (struct texture_stack_s *)texture->data;
    bu_free(td->list, "texture stack");
    bu_free(texture->data, "texture data");
}


void
texture_stack_work(struct texture_s *texture, void *mesh, struct tie_ray_s *ray, struct tie_id_s *id, vect_t *pixel) {
    struct texture_stack_s *td;
    int i;

    td = (struct texture_stack_s *)texture->data;

    for (i = td->num-1; i >= 0; i--)
	td->list[i]->work(td->list[i], mesh, ray, id, pixel);
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
