/*                     T E X T U R E _ S T A C K . H
 * BRL-CAD / ADRT
 *
 * Copyright (c) 2002-2008 United States Government as represented by
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
/** @file texture_stack.h
 *
 *  Comments -
 *      Texture Library - Stack Header
 *
 */

#ifndef _TEXTURE_STACK_H
#define _TEXTURE_STACK_H


#include "texture.h"


typedef struct texture_stack_s {
    int			num;
    texture_t		**list;
} texture_stack_t;


extern	void	texture_stack_init(texture_t *texture);
extern	void	texture_stack_free(texture_t *texture);
extern	void	texture_stack_work(__TEXTURE_WORK_PROTOTYPE__);
extern	void	texture_stack_push(texture_t *texture, texture_t *texture_new);


#endif

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
