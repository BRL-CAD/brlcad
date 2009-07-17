/*                       T E X T U R E . H
 * BRL-CAD / ADRT
 *
 * Copyright (c) 2002-2009 United States Government as represented by
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
/** @file texture.h
 *
 *  Comments -
 *      Texture Library - Main Include
 *
 */

#ifndef _TEXTURE_H
#define _TEXTURE_H

#include "texture_internal.h"

#define TEXTURE_BLEND		0x0200
#define TEXTURE_BUMP		0x0201
#define TEXTURE_CAMO		0x0202
#define TEXTURE_CHECKER		0x0203
#define TEXTURE_CLOUDS		0x0204
#define TEXTURE_GRADIENT	0x0205
#define TEXTURE_IMAGE		0x0206
#define TEXTURE_MIX		0x0207
#define TEXTURE_REFLECT		0x0208
#define TEXTURE_STACK		0x0209

typedef struct texture_perlin_s {
    int *PV;
    TIE_3 *RV;
} texture_perlin_t;

extern	void	texture_perlin_init(texture_perlin_t *P);
extern	void	texture_perlin_free(texture_perlin_t *P);
extern	tfloat	texture_perlin_noise3(texture_perlin_t *P, TIE_3 V, tfloat Size, int Depth);


typedef struct texture_blend_s {
    TIE_3 color1;
    TIE_3 color2;
} texture_blend_t;

extern void texture_blend_init(texture_t *texture, TIE_3 color1, TIE_3 color2);
extern void texture_blend_free(texture_t *texture);
extern void texture_blend_work(__TEXTURE_WORK_PROTOTYPE__);




typedef struct texture_bump_s {
    TIE_3 coef;
} texture_bump_t;


extern	void	texture_bump_init(texture_t *texture, TIE_3 rgb);
extern	void	texture_bump_free(texture_t *texture);
extern	void	texture_bump_work(__TEXTURE_WORK_PROTOTYPE__);



typedef struct texture_camo_s {
    tfloat size;
    int octaves;
    int absolute;
    TIE_3 color1;
    TIE_3 color2;
    TIE_3 color3;
    texture_perlin_t perlin;
} texture_camo_t;


void texture_camo_init(texture_t *texture, tfloat size, int octaves, int absolute, TIE_3 color1, TIE_3 color2, TIE_3 color3);
void texture_camo_free(texture_t *texture);
void texture_camo_work(__TEXTURE_WORK_PROTOTYPE__);


typedef struct texture_checker_s {
    int tile;
} texture_checker_t;

extern	void	texture_checker_init(texture_t *texture, int checker);
extern	void	texture_checker_free(texture_t *texture);
extern	void	texture_checker_work(__TEXTURE_WORK_PROTOTYPE__);


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


typedef struct texture_gradient_s {
    int		axis;
} texture_gradient_t;

void texture_gradient_init(texture_t *texture, int axis);
void texture_gradient_free(texture_t *texture);
void texture_gradient_work(__TEXTURE_WORK_PROTOTYPE__);


typedef struct texture_image_s {
    short	w;
    short	h;
    unsigned char *image;
} texture_image_t;

void texture_image_init(texture_t *texture, short w, short h, unsigned char *image);
void texture_image_free(texture_t *texture);
void texture_image_work(__TEXTURE_WORK_PROTOTYPE__);

typedef struct texture_mix_s {
    texture_t *texture1;
    texture_t *texture2;
    tfloat coef;
} texture_mix_t;


extern	void	texture_mix_init(texture_t *texture, texture_t *texture1, texture_t *texture2, tfloat coef);
extern	void	texture_mix_free(texture_t *texture);
extern	void	texture_mix_work(__TEXTURE_WORK_PROTOTYPE__);


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
