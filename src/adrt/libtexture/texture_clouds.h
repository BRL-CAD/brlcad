/*                     T E X T U R E _ C L O U D S . H
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
/** @file texture_clouds.h
 *
 *  Comments -
 *      Texture Library - Perlin Clouds Header
 *
 */

#ifndef _TEXTURE_CLOUDS_H
#define _TEXTURE_CLOUDS_H


#include "texture.h"
#include "texture_perlin.h"


typedef struct texture_clouds_s {
    tfloat size;
    int octaves;
    int absolute;
    TIE_3	scale;
    TIE_3 translate;
    texture_perlin_t perlin;
} texture_clouds_t;

void texture_clouds_init(texture_t *texture, tfloat size, int octaves, int absolute, TIE_3 scale, TIE_3 translate);
void texture_clouds_free(texture_t *texture);
void texture_clouds_work(__TEXTURE_WORK_PROTOTYPE__);

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
