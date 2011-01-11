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

#include "vmath.h"

#ifdef __cplusplus
extern "C" {
#endif

#define TIE_SINGLE_PRECISION 0
#define TIE_DOUBLE_PRECISION 1

#ifndef TIE_EXPORT
#  if defined(_WIN32) && !defined(__CYGWIN__) && defined(BRLCAD_DLL)
#    ifdef TIE_EXPORT_DLL
#      define TIE_EXPORT __declspec(dllexport)
#    else
#      define TIE_EXPORT __declspec(dllimport)
#    endif
#  else
#    define TIE_EXPORT
#  endif
#endif


/*
 * define which precision to use, 0 is 'float' and 1 is 'double'.
 * Default to double precision to protect those not familiar.
 * Horrible macros wrap functions and values to build a library
 * capable of doing either without recompilation.
 */
#ifndef TIE_PRECISION
# define TIE_PRECISION TIE_SINGLE_PRECISION
#endif

#define TIE_CHECK_DEGENERATE	1

#define TIE_KDTREE_FAST		0x0
#define TIE_KDTREE_OPTIMAL	0x1

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

TIE_EXPORT BU_EXTERN(void TIE_VAL(tie_kdtree_free), (tie_t *tie));
TIE_EXPORT BU_EXTERN(uint32_t TIE_VAL(tie_kdtree_cache_free), (tie_t *tie, void **cache));
TIE_EXPORT BU_EXTERN(void TIE_VAL(tie_kdtree_cache_load), (tie_t *tie, void *cache, uint32_t size));
TIE_EXPORT BU_EXTERN(void TIE_VAL(tie_kdtree_prep), (tie_t *tie));
TIE_EXPORT extern tfloat TIE_VAL(TIE_PREC);

/* compatability macros */
#define tie_kdtree_free TIE_VAL(tie_kdtree_free)
#define tie_kdtree_cache_free TIE_VAL(tie_kdtree_cache_free)
#define tie_kdtree_cache_load TIE_VAL(tie_kdtree_cache_load)
#define tie_kdtree_prep TIE_VAL(tie_kdtree_prep)
#define TIE_PREC TIE_VAL(TIE_PREC)

TIE_EXPORT BU_EXTERN(int tie_check_degenerate,);

TIE_EXPORT BU_EXTERN(void TIE_VAL(tie_init), (tie_t *tie, unsigned int tri_num, unsigned int kdmethod));
TIE_EXPORT BU_EXTERN(void TIE_VAL(tie_free), (tie_t *tie));
TIE_EXPORT BU_EXTERN(void TIE_VAL(tie_prep), (tie_t *tie));
TIE_EXPORT BU_EXTERN(void* TIE_VAL(tie_work), (tie_t *tie, tie_ray_t *ray, tie_id_t *id, void *(*hitfunc)(tie_ray_t*, tie_id_t*, tie_tri_t*, void *ptr), void *ptr));
TIE_EXPORT BU_EXTERN(void TIE_VAL(tie_push), (tie_t *tie, TIE_3 **tlist, unsigned int tnum, void *plist, unsigned int pstride));

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
