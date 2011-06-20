/*                       T E X T U R E . H
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
/** @file librender/texture.h
 *
 *  Comments -
 *      Texture Library - Main Include
 *
 */

#ifndef _TEXTURE_H
#define _TEXTURE_H

#include "texture_internal.h"

#define TEXTURE_BLEND  0x0200
#define TEXTURE_BUMP  0x0201
#define TEXTURE_CAMO  0x0202
#define TEXTURE_CHECKER  0x0203
#define TEXTURE_CLOUDS  0x0204
#define TEXTURE_GRADIENT 0x0205
#define TEXTURE_IMAGE  0x0206
#define TEXTURE_MIX  0x0207
#define TEXTURE_REFLECT  0x0208
#define TEXTURE_STACK  0x0209

struct texture_perlin_s {
    int *PV;
    vect_t *RV;
};

BU_EXPORT extern void texture_perlin_init(struct texture_perlin_s *P);
BU_EXPORT extern void texture_perlin_free(struct texture_perlin_s *P);
BU_EXPORT extern fastf_t texture_perlin_noise3(struct texture_perlin_s *P, vect_t V, fastf_t Size, int Depth);


struct texture_blend_s {
    vect_t color1;
    vect_t color2;
};

BU_EXPORT extern void texture_blend_init(struct texture_s *texture, vect_t color1, vect_t color2);
BU_EXPORT extern void texture_blend_free(struct texture_s *texture);
BU_EXPORT extern void texture_blend_work(__TEXTURE_WORK_PROTOTYPE__);

struct texture_bump_s {
    vect_t coef;
};

BU_EXPORT extern void texture_bump_init(struct texture_s *texture, vect_t rgb);
BU_EXPORT extern void texture_bump_free(struct texture_s *texture);
BU_EXPORT extern void texture_bump_work(__TEXTURE_WORK_PROTOTYPE__);



struct texture_camo_s {
    fastf_t size;
    int octaves;
    int absolute;
    vect_t color1;
    vect_t color2;
    vect_t color3;
    struct texture_perlin_s perlin;
};


BU_EXPORT extern void texture_camo_init(struct texture_s *texture, fastf_t size, int octaves, int absolute, vect_t color1, vect_t color2, vect_t color3);
BU_EXPORT extern void texture_camo_free(struct texture_s *texture);
BU_EXPORT extern void texture_camo_work(__TEXTURE_WORK_PROTOTYPE__);


struct texture_checker_s {
    int tile;
};

BU_EXPORT extern void texture_checker_init(struct texture_s *texture, int checker);
BU_EXPORT extern void texture_checker_free(struct texture_s *texture);
BU_EXPORT extern void texture_checker_work(__TEXTURE_WORK_PROTOTYPE__);


struct texture_clouds_s {
    fastf_t size;
    int octaves;
    int absolute;
    vect_t scale;
    vect_t translate;
    struct texture_perlin_s perlin;
};

BU_EXPORT extern void texture_cloudts_inis(struct texture_s *texture, fastf_t size, int octaves, int absolute, vect_t scale, vect_t translate);
BU_EXPORT extern void texture_clouds_free(struct texture_s *texture);
BU_EXPORT extern void texture_clouds_work(__TEXTURE_WORK_PROTOTYPE__);


struct texture_gradient_s {
    int  axis;
};

BU_EXPORT extern void texture_gradient_init(struct texture_s *texture, int axis);
BU_EXPORT extern void texture_gradient_free(struct texture_s *texture);
BU_EXPORT extern void texture_gradient_work(__TEXTURE_WORK_PROTOTYPE__);


struct texture_image_s {
    short w;
    short h;
    unsigned char *image;
};

BU_EXPORT extern void texture_image_init(struct texture_s *texture, short w, short h, unsigned char *image);
BU_EXPORT extern void texture_image_free(struct texture_s *texture);
BU_EXPORT extern void texture_image_work(__TEXTURE_WORK_PROTOTYPE__);

struct texture_mix_s {
    struct texture_s *texture1;
    struct texture_s *texture2;
    fastf_t coef;
};


BU_EXPORT extern void texture_mix_init(struct texture_s *texture, struct texture_s *texture1, struct texture_s *texture2, fastf_t coef);
BU_EXPORT extern void texture_mix_free(struct texture_s *texture);
BU_EXPORT extern void texture_mix_work(__TEXTURE_WORK_PROTOTYPE__);


struct texture_stack_s {
    int   num;
    struct texture_s  **list;
};


BU_EXPORT extern void texture_stack_init(struct texture_s *texture);
BU_EXPORT extern void texture_stack_free(struct texture_s *texture);
BU_EXPORT extern void texture_stack_work(__TEXTURE_WORK_PROTOTYPE__);
BU_EXPORT extern void texture_stack_push(struct texture_s *texture, struct texture_s *texture_new);


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
