/*                     T E X T U R E _ I M A G E . H
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
/**
 * @file texture_image.h
 *
 *  Comments -
 *      Texture Library - Image Header
 *
 */

#ifndef _TEXTURE_IMAGE_H
#define _TEXTURE_IMAGE_H

#if 0

#include "common.h"

#endif

#include "texture.h"


typedef struct texture_image_s {
    short	w;
    short	h;
    unsigned char *image;
} texture_image_t;

void texture_image_init(texture_t *texture, short w, short h, unsigned char *image);
void texture_image_free(texture_t *texture);
void texture_image_work(__TEXTURE_WORK_PROTOTYPE__);

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
