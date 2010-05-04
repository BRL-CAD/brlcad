/*                    T I E _ S T R U C T . H
 * BRL-CAD
 *
 * Copyright (c) 2008-2010 United States Government as represented by
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
/** @file tie_struct.h
 *
 * Brief description
 *
 */

#ifndef	_TIE_STRUCT_H
#define _TIE_STRUCT_H

#include "tie_define.h"

#ifdef _WIN32
# undef near
# undef far
#endif

typedef struct TIE_3_s {
    tfloat v[3];
} TIE_3;

typedef struct tie_ray_s {
    TIE_3 pos;    /* Position */
    TIE_3 dir;    /* Direction */
    short depth;  /* Depth */
    short kdtree_depth;
} tie_ray_t;

typedef struct tie_id_s {
    TIE_3 pos;    /* Point */
    TIE_3 norm;   /* Normal */
    tfloat dist;   /* Distance */
    tfloat alpha;	/* Barycentric Coordinate Alpha */
    tfloat beta;	/* Barycentric Coordinate Beta */
} tie_id_t;

typedef struct tie_tri_s {
    TIE_3 data[3];	/* 36-bytes, Data[0] = Point, Data[1] = Normal, Data[2] = DotProduct, VectorU, VectorV */
    tfloat *v;		/* 8-bytes */
    void *ptr;		/* 4-bytes */
} tie_tri_t;

typedef struct tie_kdtree_s {
    tfloat axis;
    void *data;
} tie_kdtree_t;

typedef struct tie_geom_s {
    tie_tri_t **tri_list;
    unsigned int tri_num;
} tie_geom_t;

typedef struct tie_stack_s {
    tie_kdtree_t *node;
    tfloat near;
    tfloat far;
} tie_stack_t;

typedef struct tie_s {
    uint64_t rays_fired;
    tie_kdtree_t *kdtree;
    unsigned int max_depth;	/* Maximum depth allowed for given geometry */
    unsigned int tri_num;
    unsigned int tri_num_alloc;
    tie_tri_t *tri_list;
    int stat;		/* used for testing various statistics */
    unsigned int kdmethod;		/* Optimal or Fast */
    /* all tfloat altered stuff should be at the end. */
    TIE_3 min, max;
    vect_t mid;
    fastf_t radius;
#if TIE_PRECISION == 0
    tfloat _pad[6];	/* so both float and double variants are the same size. */
#endif
} tie_t;

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
