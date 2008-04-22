/*                     T E X T U R E _ B L E N D . H
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
/** @file texture_blend.h
 *
 *  Comments -
 *      Texture Library - Blend Header
 *
 */

#ifndef _TEXTURE_BLEND_H
#define _TEXTURE_BLEND_H


#include "texture_internal.h"


typedef struct texture_blend_s {
    TIE_3 color1;
    TIE_3 color2;
} texture_blend_t;


extern void texture_blend_init(texture_t *texture, TIE_3 color1, TIE_3 color2);
extern void texture_blend_free(texture_t *texture);
extern void texture_blend_work(__TEXTURE_WORK_PROTOTYPE__);


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
