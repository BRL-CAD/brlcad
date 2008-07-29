/*                   A D R T _ S T R U C T . H
 * BRL-CAD
 *
 * Copyright (c) 2008 United States Government as represented by
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
/** @file adrt_struct.h
 *
 * Brief description
 *
 */

#ifndef _ADRT_STRUCT_H
#define _ADRT_STRUCT_H

#ifdef TIE_PRECISION
# if TIE_PRECISION != TIE_SINGLE_PRECISION
#  error "Need single floating point precision out of tie"
# endif
#else
# error "Need TIE_PRECISION set to 0 in the CFLAGS"
#endif

#include "tie.h"
#include "texture_internal.h"
#include "render.h"

#define COMMON_BIT_DEPTH_24	0
#define	COMMON_BIT_DEPTH_128	1

/* Attributes */
typedef struct adrt_mesh_attributes_s {
    TIE_3 color; /* base color of the material */
    tfloat density; /* density of the material, x-ray/vulnerability stuff */
    tfloat gloss; /* smoothness of the surface, ability to reflect */
    tfloat emission; /* emission, power of light source */
    tfloat ior; /* index of refraction */
} adrt_mesh_attributes_t;


/* Mesh */
typedef struct adrt_mesh_s {
    int flags;
    char name[256];
    TIE_3 min, max;
    tfloat matrix[16];
    tfloat matinv[16];
    adrt_mesh_attributes_t *attributes;
    struct texture_s *texture;
} adrt_mesh_t;

struct adrt_load_info {
    unsigned char op;
    unsigned int fmt;
    unsigned int wid;
    unsigned int pid;
    char dbnam[64];	/* yes, that extra 'e' is too much typing. */
};

#define ADRT_MESH(_m) ((adrt_mesh_t *)_m)

#endif

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
