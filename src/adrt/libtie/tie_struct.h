/*
 *  tie_struct.h
 *
 *  RCSid:          $Id$
*/

#ifndef	_TIE_STRUCT_H
#define _TIE_STRUCT_H

#include "tie_define.h"
#include <inttypes.h>

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
  int tri_num;
} tie_geom_t;

typedef struct tie_stack_s {
  tie_kdtree_t *node;
  tfloat near;
  tfloat far;
} tie_stack_t;

typedef struct tie_s {
  uint64_t rays_fired;
  tie_kdtree_t *kdtree;
  int max_depth;	/* Maximum depth allowed for given geometry */
  TIE_3 min, max;
  unsigned int tri_num;
  unsigned int tri_num_alloc;
  tie_tri_t *tri_list;
  int stat;		/* used for testing various statistics */
} tie_t;

#endif
