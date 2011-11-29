/*                           T I E . H
 * BRL-CAD
 *
 * Copyright (c) 2008-2011 United States Government as represented by
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

#ifndef RT_EXPORT
#  if defined(RT_DLL_EXPORTS) && defined(RT_DLL_IMPORTS)
#    error "Only RT_DLL_EXPORTS or RT_DLL_IMPORTS can be defined, not both."
#  elif defined(RT_DLL_EXPORTS)
#    define RT_EXPORT __declspec(dllexport)
#  elif defined(RT_DLL_IMPORTS)
#    define RT_EXPORT __declspec(dllimport)
#  else
#    define RT_EXPORT
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

struct tie_ray_s {
    point_t pos;    /* Position */
    vect_t dir;    /* Direction */
    short depth;  /* Depth */
    short kdtree_depth;
};

struct tie_id_s {
    point_t pos;    /* Point */
    vect_t norm;   /* Normal */
    fastf_t dist;   /* Distance */
    fastf_t alpha;	/* Barycentric Coordinate Alpha */
    fastf_t beta;	/* Barycentric Coordinate Beta */
};

struct tie_tri_s {
    TIE_3 data[3];	/* 36-bytes, Data[0] = Point, Data[1] = Normal, Data[2] = DotProduct, VectorU, VectorV */
    tfloat *v;		/* 8-bytes */
    void *ptr;		/* 4-bytes */
};

struct tie_kdtree_s {
    fastf_t axis;
    void *data;
};


struct tie_s {
    uint64_t rays_fired;
    struct tie_kdtree_s *kdtree;
    unsigned int max_depth;	/* Maximum depth allowed for given geometry */
    unsigned int tri_num;
    unsigned int tri_num_alloc;
    struct tie_tri_s *tri_list;
    int stat;		/* used for testing various statistics */
    unsigned int kdmethod;		/* Optimal or Fast */
    point_t min, max;
    vect_t amin, amax, mid;
    fastf_t radius;
};

RT_EXPORT extern void TIE_VAL(tie_kdtree_free)(struct tie_s *tie);
RT_EXPORT extern void TIE_VAL(tie_kdtree_prep)(struct tie_s *tie);
RT_EXPORT extern fastf_t TIE_PREC;

/* compatability macros */
#define tie_kdtree_free TIE_VAL(tie_kdtree_free)
#define tie_kdtree_prep TIE_VAL(tie_kdtree_prep)

RT_EXPORT extern int tie_check_degenerate;

RT_EXPORT extern void TIE_VAL(tie_init)(struct tie_s *tie, unsigned int tri_num, unsigned int kdmethod);
RT_EXPORT extern void TIE_VAL(tie_free)(struct tie_s *tie);
RT_EXPORT extern void TIE_VAL(tie_prep)(struct tie_s *tie);
RT_EXPORT extern void* TIE_VAL(tie_work)(struct tie_s *tie, struct tie_ray_s *ray, struct tie_id_s *id, void *(*hitfunc)(struct tie_ray_s*, struct tie_id_s*, struct tie_tri_s*, void *ptr), void *ptr);
RT_EXPORT extern void TIE_VAL(tie_push)(struct tie_s *tie, TIE_3 **tlist, unsigned int tnum, void *plist, unsigned int pstride);

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
