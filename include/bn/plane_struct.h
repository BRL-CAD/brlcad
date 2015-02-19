/*                        P L A N E _ S T R U C T . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2014 United States Government as represented by
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

/*----------------------------------------------------------------------*/
/* @file plane_struct.h */
/** @addtogroup plane */
/** @{ */

/**
 * @brief
 * Plane structures (from src/librt/plane.h)
 * Holds all the required info for geometric planes.
 */

#ifndef BN_PLANE_STRUCT_H
#define BN_PLANE_STRUCT_H

#include "common.h"
#include "vmath.h"
#include "bn/defines.h"

__BEGIN_DECLS

#define MAXPTS 4			/* All we need are 4 points */
#define pl_A pl_points[0]		/* Synonym for A point */

struct plane_specific  {
    size_t pl_npts;			/* number of points on plane */
    point_t pl_points[MAXPTS];		/* Actual points on plane */
    vect_t pl_Xbasis;			/* X (B-A) vector (for 2d coords) */
    vect_t pl_Ybasis;			/* Y (C-A) vector (for 2d coords) */
    vect_t pl_N;			/* Unit-length Normal (outward) */
    fastf_t pl_NdotA;			/* Normal dot A */
    fastf_t pl_2d_x[MAXPTS];		/* X 2d-projection of points */
    fastf_t pl_2d_y[MAXPTS];		/* Y 2d-projection of points */
    fastf_t pl_2d_com[MAXPTS];		/* pre-computed common-term */
    struct plane_specific *pl_forw;	/* Forward link */
    char pl_code[MAXPTS+1];		/* Face code string.  Decorative. */
};

/*
 * Describe the tri_specific structure.
 */
struct tri_specific  {
    point_t tri_A;			/* triangle vertex (A) */
    vect_t tri_BA;			/* B - A (second point) */
    vect_t tri_CA;			/* C - A (third point) */
    vect_t tri_wn;			/* facet normal (non-unit) */
    vect_t tri_N;			/* unit normal vector */
    fastf_t *tri_normals;		/* unit vertex normals A, B, C  (this is malloced storage) */
    int tri_surfno;			/* solid specific surface number */
    struct tri_specific *tri_forw;	/* Next facet */
};

typedef struct tri_specific tri_specific_double;

/*
 * A more memory conservative version
 */
struct tri_float_specific  {
    float tri_A[3];			/* triangle vertex (A) */
    float tri_BA[3];			/* B - A (second point) */
    float tri_CA[3];			/* C - A (third point) */
    float tri_wn[3];			/* facet normal (non-unit) */
    float tri_N[3];			/* unit normal vector */
    signed char *tri_normals;		/* unit vertex normals A, B, C  (this is malloced storage) */
    int tri_surfno;			/* solid specific surface number */
    struct tri_float_specific *tri_forw;/* Next facet */
};

typedef struct tri_float_specific tri_specific_float;

__END_DECLS

#endif  /* BN_PLANE_STRUCT_H */
/** @} */
/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
