/*                     S T R U C T. H
 * BRL-CAD
 *
 * Copyright (C) 2002-2005 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file struct.h
 *                     S T R U C T . H
 *
 *  Triangle Intersection Engine Structures
 *
 *  Author -
 *      Justin L. Shumaker
 *
 *  Source -
 *      The U. S. Army Research Laboratory
 *      Aberdeen Proving Ground, Maryland  21005-5068  USA
 *
 * $Id$
 */
/** @addtogroup libtie */ /** @{ */
  
#ifndef	_TIE_STRUCT_H
#define _TIE_STRUCT_H

#include "define.h"
#include <inttypes.h>
#if TIE_RAY_ACCOUNTING
# include <pthread.h>
#endif

/**
 * @struct TIE_3_s struct.h
 * This is a 3-tuple
 */
typedef struct TIE_3_s {
  tfloat		v[3];
} TIE_3;

/** 
 * @struct tie_ray_s struct.h
 * All the information about a ray.
 *
 * includes position, direction and depth
 */
typedef struct tie_ray_s {
  TIE_3		pos;    /* Position */
  TIE_3		dir;    /* Direction */
  short		depth;  /* Depth */
  short		kdtree_depth;
} tie_ray_t;

/** 
 * @struct tie_id_s struct.h
 *
 * Ray intersection data (id)
 *
 * point, normal, distance, and barycentric coordinates
 */
typedef struct tie_id_s {
  TIE_3		pos;    /* Point */
  TIE_3		norm;   /* Normal */
  tfloat	dist;   /* Distance */
  tfloat	alpha;	/* Barycentric Coordinate Alpha */
  tfloat	beta;	/* Barycentric Coordinate Beta */
} tie_id_t;

/** 
 * @struct tie_tri_s struct.h
 *
 * Everything you need to know about a triangle
 */
typedef struct tie_tri_s {
  TIE_3		data[3];	/* 36-bytes, Data[0] = Point, Data[1] = Normal, Data[2] = DotProduct, VectorU, VectorV */
  tfloat	*v12;		/* 8-bytes */
  void		*ptr;		/* 4-bytes */
} tie_tri_t;

/** 
 * @struct tie_kdtree_s struct.h
 *
 * The binary space partitioning tree
 */
typedef struct tie_kdtree_s {
  tfloat	axis;
  void		*data;
} tie_kdtree_t;

/** 
 *
 */
typedef struct tie_geom_s {
  tie_tri_t	**tri_list;
  int		tri_num;
} tie_geom_t;

/** 
 *
 */
typedef struct tie_stack_s {
  tie_kdtree_t	*node;
  tfloat	near;
  tfloat	far;
} tie_stack_t;

/** 
 *
 */
typedef struct tie_s {
  uint64_t	rays_fired;
  tie_kdtree_t	*kdtree;
  int		max_depth;	/* Maximum depth allowed for given geometry */
  TIE_3		min, max;
  int		tri_num;
  tie_tri_t	*tri_list;
  int		max_tri;
} tie_t;

#endif

/** @} */
