/*                     T E X T U R E _ S T A C K . C
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
/** @file texture_stack.c
 *
 *  Comments -
 *      Texture Library - Stack textures to pipe output of one into another
 *
 */

#include "texture.h"
#include <stdlib.h>

#include "bu.h"


void texture_stack_init(texture_t *texture) {
    texture_stack_t *td;

    texture->data = bu_malloc(sizeof(texture_stack_t), "texture data");
    texture->free = texture_stack_free;
    texture->work = (texture_work_t *)texture_stack_work;

    td = (texture_stack_t *)texture->data;
    td->num = 0;
    td->list = NULL;
}


void texture_stack_free(texture_t *texture) {
    texture_stack_t *td;

    td = (texture_stack_t *)texture->data;
    bu_free(td->list, "texture stack");
    bu_free(texture->data, "texture data");
}


void texture_stack_work(__TEXTURE_WORK_PROTOTYPE__) {
    texture_stack_t *td;
    int i;

    td = (texture_stack_t *)texture->data;

    for (i = td->num-1; i >= 0; i--)
	td->list[i]->work(td->list[i], mesh, ray, id, pixel);
}


void texture_stack_push(texture_t *texture, texture_t *texture_new) {
    texture_stack_t *td;

    td = (texture_stack_t *)texture->data;

    td->list = (texture_t **)bu_realloc(td->list, sizeof(texture_t *)*(td->num+1), "texture data");
    td->list[td->num++] = texture_new;
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
