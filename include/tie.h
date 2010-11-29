/*                           T I E . H
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
/** @file tie.h
 *
 * Brief description
 *
 */

#ifndef _TIE_H
#define _TIE_H

#include "common.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _WIN32
# undef near
# undef far
#endif

#include <math.h>
#ifdef HAVE_STDINT_H
#  include <stdint.h>
#endif

#include "vmath.h"

#define TIE_SINGLE_PRECISION 0
#define TIE_DOUBLE_PRECISION 1

/*
 * define which precision to use, 0 is 'float' and 1 is 'double'.
 * Default to double precision to protect those not familiar.
 * Horrible macros wrap functions and values to build a library
 * capable of doing either without recompilation.
 */
#ifndef TIE_PRECISION
# define TIE_PRECISION TIE_SINGLE_PRECISION
#endif

#define	TIE_TAB1		"\1\0\0\2\2\1"	/* Triangle Index Table */
#define	TIE_KDTREE_NODE_MAX	4		/* Maximum number of triangles that can reside in a given node until it should be split */
#define	TIE_KDTREE_DEPTH_K1	1.4		/* K1 Depth Constant Coefficient */
#define	TIE_KDTREE_DEPTH_K2	1		/* K2 Contant */
#define TIE_CHECK_DEGENERATE	1

#define TIE_KDTREE_FAST		0x0
#define TIE_KDTREE_OPTIMAL	0x1

#define MAX_SLICES	100
#define MIN_SLICES	35
#define MIN_DENSITY	0.01
#define MIN_SPAN	0.15
#define SCALE_COEF	1.80

/* Type to use for floating precision */
#if TIE_PRECISION == TIE_SINGLE_PRECISION
typedef float tfloat;
# define TIE_VAL(x) x##0
#elif TIE_PRECISION == TIE_DOUBLE_PRECISION
typedef double tfloat;
# define TIE_VAL(x) x##1
#else
# error "Unknown precision"
#endif

/* TCOPY(type, source base, source offset, dest base, dest offset) */
#define TCOPY(_t, _fv, _fi, _tv, _ti) { \
	*(_t *)&((uint8_t *)_tv)[_ti] = *(_t *)&((uint8_t *)_fv)[_fi]; }

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
    vect_t amin, amax, mid;
    fastf_t radius;
#if TIE_PRECISION == 0
    tfloat _pad[6];	/* so both float and double variants are the same size. */
#endif
} tie_t;

BU_EXPORT BU_EXTERN(void TIE_VAL(tie_kdtree_free), (tie_t *tie));
BU_EXPORT BU_EXTERN(uint32_t TIE_VAL(tie_kdtree_cache_free), (tie_t *tie, void **cache));
BU_EXPORT BU_EXTERN(void TIE_VAL(tie_kdtree_cache_load), (tie_t *tie, void *cache, uint32_t size));
BU_EXPORT BU_EXTERN(void TIE_VAL(tie_kdtree_prep), (tie_t *tie));
BU_EXPORT extern tfloat TIE_VAL(TIE_PREC);

/* compatability macros */
#define tie_kdtree_free TIE_VAL(tie_kdtree_free)
#define tie_kdtree_cache_free TIE_VAL(tie_kdtree_cache_free)
#define tie_kdtree_cache_load TIE_VAL(tie_kdtree_cache_load)
#define tie_kdtree_prep TIE_VAL(tie_kdtree_prep)
#define TIE_PREC TIE_VAL(TIE_PREC)

BU_EXPORT extern TIE_VAL(int tie_check_degenerate);
#define tie_check_degenerate TIE_VAL(tie_check_degenerate)

BU_EXPORT BU_EXTERN(void TIE_VAL(tie_init), (tie_t *tie, unsigned int tri_num, unsigned int kdmethod));
BU_EXPORT BU_EXTERN(void TIE_VAL(tie_free), (tie_t *tie));
BU_EXPORT BU_EXTERN(void TIE_VAL(tie_prep), (tie_t *tie));
BU_EXPORT BU_EXTERN(void* TIE_VAL(tie_work), (tie_t *tie, tie_ray_t *ray, tie_id_t *id, void *(*hitfunc)(tie_ray_t*, tie_id_t*, tie_tri_t*, void *ptr), void *ptr));
BU_EXPORT BU_EXTERN(void TIE_VAL(tie_push), (tie_t *tie, TIE_3 **tlist, unsigned int tnum, void *plist, unsigned int pstride));

/* backwards compatible macros */
#define tie_init TIE_VAL(tie_init)
#define tie_free TIE_VAL(tie_free)
#define tie_prep TIE_VAL(tie_prep)
#define tie_work TIE_VAL(tie_work)
#define tie_push TIE_VAL(tie_push)

#ifdef __cplusplus
}
#endif

#endif /* _TIE_H */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
