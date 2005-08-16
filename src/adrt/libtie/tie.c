/*                     T I E . C
 *
 * @file tie.c
 *
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
 *
 *  @brief support routines for shooting at triangles
 *  Comments -
 *      Triangle Intersection Engine
 *
 *      The calling sequence is as follows:
 *        - tie_init()	initialize the data structure
 *        - tie_push()	add triangles to the universe to be raytraced
 *        - tie_prep()	build the KDTREE for the triangles
 *        - tie_work()	shoot some ray
 *        - tie_work()	shoot some ray
 *        - tie_work()	shoot some ray
 *        - tie_work()	shoot some ray
 *        - tie_free()	Free up all the memory
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
/** @addtogroup libtie
 *
 * The calling sequence is as follows:
 *	@li tie_init()	initialize the data structure
 *	@li tie_push()	add triangles to the universe to be raytraced
 *	@li tie_prep()	build the KDTREE for the triangles
 *	@li tie_work()	shoot some ray
 *	@li tie_work()	shoot some ray
 *	@li tie_work()	shoot some ray
 *	@li tie_work()	shoot some ray
 *	@li tie_work()	shoot some ray
 *	@li tie_free()	Free up all the memory
 * @{
 */

#ifdef HAVE_CONFIG_H
# include "brlcad_config.h"
#endif

#include "tie.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "struct.h"

#ifdef TIE_SSE
# include <xmmintrin.h>
#endif

#ifndef HUGE
# ifdef HUGE_VAL
#  define HUGE HUGE_VAL
# else
#  define HUGE 3e23
# endif
#endif

tfloat TIE_PREC;

/*************************************************************
 **************** PRIVATE FUNCTIONS **************************
 *************************************************************/


static void tie_free_node(tie_kdtree_t *node) {
  tie_kdtree_t	*node_aligned = (tie_kdtree_t*)((TIE_PTR_CAST)node & ~0x7L);

  if(((TIE_PTR_CAST)(node_aligned->data)) & 0x4) {
    /* Node Data is KDTREE Children, Recurse */
    tie_free_node(&((tie_kdtree_t*)(node_aligned->data))[0]);
    tie_free_node(&((tie_kdtree_t*)(node_aligned->data))[1]);
    free((void*)((TIE_PTR_CAST)(node_aligned->data) & ~0x7L));
  } else {
    /* This node points to a geometry node, free it */
    free(((tie_geom_t*)((TIE_PTR_CAST)(node_aligned->data) & ~0x7L))->tri_list);
    free((void*)((TIE_PTR_CAST)(node_aligned->data) & ~0x7L));
  }
}


static void tie_insert(tie_t *tie, tie_tri_t *tri_list, int tri_num) {
  tie_geom_t *g;
  TIE_3 min, max;
  int i;


  if(!tri_num)
    return;

  /* Insert all triangles into the Head Node */
  if(!tie->kdtree) {
    tie->kdtree = (tie_kdtree_t *)malloc(sizeof(tie_kdtree_t));
    tie->kdtree->data = (void *)malloc(sizeof(tie_geom_t));
    g = ((tie_geom_t *)(tie->kdtree->data));
    g->tri_num = 0;

    math_bbox(tie->min, tie->max, tri_list[0].data[0], tri_list[0].data[1], tri_list[0].data[2]);

    g->tri_list = (tie_tri_t **)malloc(sizeof(tie_tri_t *) * tri_num);

    /* form bounding box of scene */
    for(i = 0; i < tri_num; i++) {
      g->tri_list[i] = &tri_list[i];

      /* Get Bounding Box of Triangle */
      math_bbox(min, max, tri_list[i].data[0], tri_list[i].data[1], tri_list[i].data[2]);

      /* Check to see if defines a new Max or Min point */
      math_vec_min(tie->min, min);
      math_vec_max(tie->max, max);
      /* printf("Box: [%.3f, %.3f, %.3f] [%.3f, %.3f, %.3f]\n", tie->min.v[0], tie->min.v[1], tie->min.v[2], tie->max.v[0], tie->max.v[1], tie->max.v[2]); */
    }

    ((tie_geom_t *)(tie->kdtree->data))->tri_num = tri_num;
  }
}


static int tie_tri_box_overlap(TIE_3 *center, TIE_3 *half_size, TIE_3 triverts[3]) {
  /*
   * use separating axis theorem to test overlap between triangle and box
   * need to test for overlap in these directions:
   * 1) the {x,y,z}-directions (actually, since we use the AABB of the triangle
   *    we do not even need to test these)
   * 2) normal of the triangle
   * 3) crossproduct(edge from tri, {x,y,z}-directin)
   *    this gives 3x3=9 more tests
   */
  TIE_3 v0, v1, v2, normal, e0, e1, e2, fe, p;
  tfloat min, max, d, t, rad;

  /* move everything so that the boxcenter is in (0,0,0) */
  math_vec_sub(v0, triverts[0], (*center));
  math_vec_sub(v1, triverts[1], (*center));
  math_vec_sub(v2, triverts[2], (*center));

  /*
  * First test overlap in the {x,y,z}-directions
  * find min, max of the triangle each direction, and test for overlap in
  * that direction -- this is equivalent to testing a minimal AABB around
  * the triangle against the AABB
  */

  /* test in X-direction */
  math_min3(min, v0.v[0], v1.v[0], v2.v[0]);
  math_max3(max, v0.v[0], v1.v[0], v2.v[0]);
  if(min > half_size->v[0] || max < -half_size->v[0])
    return 0;

  /* test in Y-direction */
  math_min3(min, v0.v[1], v1.v[1], v2.v[1]);
  math_max3(max, v0.v[1], v1.v[1], v2.v[1]);
  if(min > half_size->v[1] || max < -half_size->v[1])
    return 0;

  /* test in Z-direction */
  math_min3(min, v0.v[2], v1.v[2], v2.v[2]);
  math_max3(max, v0.v[2], v1.v[2], v2.v[2]);
  if(min > half_size->v[2] || max < -half_size->v[2])
    return 0;

  /* compute triangle edges */
  math_vec_sub(e0, v1, v0); /* tri edge 0 */
  math_vec_sub(e1, v2, v1); /* tri edge 1 */
  math_vec_sub(e2, v0, v2); /* tri edge 2 */

  /* Perform the 9 tests */
  fe.v[0] = fabs(e0.v[0]);
  fe.v[1] = fabs(e0.v[1]);
  fe.v[2] = fabs(e0.v[2]);

  AXISTEST_X01(e0.v[2], e0.v[1], fe.v[2], fe.v[1]);
  AXISTEST_Y02(e0.v[2], e0.v[0], fe.v[2], fe.v[0]);
  AXISTEST_Z12(e0.v[1], e0.v[0], fe.v[1], fe.v[0]);

  fe.v[0] = fabs(e1.v[0]);
  fe.v[1] = fabs(e1.v[1]);
  fe.v[2] = fabs(e1.v[2]);

  AXISTEST_X01(e1.v[2], e1.v[1], fe.v[2], fe.v[1]);
  AXISTEST_Y02(e1.v[2], e1.v[0], fe.v[2], fe.v[0]);
  AXISTEST_Z0(e1.v[1], e1.v[0], fe.v[1], fe.v[0]);

  fe.v[0] = fabs(e2.v[0]);
  fe.v[1] = fabs(e2.v[1]);
  fe.v[2] = fabs(e2.v[2]);

  AXISTEST_X2(e2.v[2], e2.v[1], fe.v[2], fe.v[1]);
  AXISTEST_Y1(e2.v[2], e2.v[0], fe.v[2], fe.v[0]);
  AXISTEST_Z12(e2.v[1], e2.v[0], fe.v[1], fe.v[0]);

  /*
  * Test if the box intersects the plane of the triangle
  * compute plane equation of triangle: normal*x+d=0
  */
  math_vec_cross(normal, e0, e1);
  math_vec_dot(d, normal, v0);  /* plane eq: normal . x + d = 0 */

  p.v[0] = normal.v[0] > 0 ? half_size->v[0] : -half_size->v[0];
  p.v[1] = normal.v[1] > 0 ? half_size->v[1] : -half_size->v[1];
  p.v[2] = normal.v[2] > 0 ? half_size->v[2] : -half_size->v[2];

  math_vec_dot(t, normal, p);
  return t >= d ? 1 : 0;
}


static void tie_tri_prep(tie_t *tie) {
  TIE_3 v1, v2, u, v;
  int i, i1, i2;
  tie_tri_t *tri;

  for(i = 0; i < tie->tri_num; i++) {
    tri = &tie->tri_list[i];

    v1 = tri->data[1];
    v2 = tri->data[2];

    /* Compute Normal */
    math_vec_sub(u, tri->data[1], tri->data[0]);
    math_vec_sub(v, tri->data[2], tri->data[0]);
    math_vec_cross(tri->data[1], u, v);
    math_vec_unitize(tri->data[1]);

    /* Compute i1 and i2 */
    u.v[0] = fabs(tri->data[1].v[0]);
    u.v[1] = fabs(tri->data[1].v[1]);
    u.v[2] = fabs(tri->data[1].v[2]);

    if(u.v[2] > u.v[1] && u.v[2] > u.v[0]) {
      i1 = 0;
      i2 = 1;
    } else if(u.v[1] > u.v[2] && u.v[1] > u.v[0]) {
      i1 = 0;
      i2 = 2;
    } else {
      i1 = 1;
      i2 = 2;
    }

    /* compute u1, v2, u2, v2 */
    tri->data[2].v[1] = v1.v[i1] - tri->data[0].v[i1];
    tri->data[2].v[2] = v2.v[i1] - tri->data[0].v[i1];
    tri->v12[0] = v1.v[i2] - tri->data[0].v[i2];
    tri->v12[1] = v2.v[i2] - tri->data[0].v[i2];

    if(i1 == 0 && i2 == 1) {
      tri->v12 = (tfloat *)((TIE_PTR_CAST)(tri->v12) + 2);
    } else if (i1 == 0) {
      tri->v12 = (tfloat *)((TIE_PTR_CAST)(tri->v12) + 1);
    }

    /* Compute DotVN */
    math_vec_mul_scalar(v1, tri->data[0], -1.0);
    math_vec_dot(tri->data[2].v[0], v1, tri->data[1]);
  }
}


static void tie_build_tree(tie_t *tie, tie_kdtree_t *node, int depth, TIE_3 min, TIE_3 max, tfloat cost) {
  tie_geom_t	*child[2], *node_geom_data = (tie_geom_t*)(node->data);
  TIE_3		cmin[2], cmax[2], center, half_size, vec;
  int		i, j, n, split, cnt[2];
  tfloat	n1cost, n2cost, a, b;

/*  printf("%f %f %f %f %f %f\n", min.v[0], min.v[1], min.v[2], max.v[0], max.v[1], max.v[2]); */

  /* Terminating criteria for KDTREE subdivision */
  if(node_geom_data->tri_num <= TIE_KDTREE_NODE_MAX || depth > tie->max_depth) {
    if(node_geom_data->tri_num > tie->stat) {
      tie->stat = node_geom_data->tri_num;
    }

/*    printf("num: %d, depth: %d\n", node_geom_data->tri_num, depth); */
/*    if(node_geom_data->tri_num > tie->stat) */
/*      tie->stat++; */
    return;
  }

  /* If node area is to small with respect to scene node area then terminate */
  a = (max.v[0]-min.v[0])*(max.v[1]-min.v[1]) +
      (max.v[0]-min.v[0])*(max.v[2]-min.v[2]) + 
      (max.v[1]-min.v[1])*(max.v[2]-min.v[2]);

  b = (tie->max.v[0]-tie->min.v[0])*(tie->max.v[1]-tie->min.v[1]) +
      (tie->max.v[0]-tie->min.v[0])*(tie->max.v[2]-tie->min.v[2]) +
      (tie->max.v[1]-tie->min.v[1])*(tie->max.v[2]-tie->min.v[2]);

  /* If ratio of areas is too small then don't bother splitting */
  if(a / b <= TIE_KDTREE_MIN_AREA) {
    if(node_geom_data->tri_num > tie->stat) {
      tie->stat = node_geom_data->tri_num;
    }
    return;
  }

#if 1
  /* Left Child */
  cmin[0] = min;
  cmax[0] = max;

  /* Right Child */
  cmin[1] = min;
  cmax[1] = max;

  math_vec_add(center, max, min);
  math_vec_mul_scalar(center, center, 0.5);

  /* Split along largest Axis to keep node sizes relatively cube-like (Naive) */
  math_vec_sub(vec, max, min);

  /* Determine the largest Axis */
  if(vec.v[0] >= vec.v[1] && vec.v[0] >= vec.v[2]) {
    cmax[0].v[0] = center.v[0];
    cmin[1].v[0] = center.v[0];
    node->axis = center.v[0];
    split = 0;
  } else if(vec.v[1] >= vec.v[0] && vec.v[1] >= vec.v[2]) {
    cmax[0].v[1] = center.v[1];
    cmin[1].v[1] = center.v[1];
    node->axis = center.v[1];
    split = 1;
  } else {
    cmax[0].v[2] = center.v[2];
    cmin[1].v[2] = center.v[2];
    node->axis = center.v[2];
    split = 2;
  }
#else
..
{
int d, s;
tfloat sa, cost_test[3][7][2];
TIE_3 vec1, vec2;

  /*
  * Test all 3 splitting planes
  * Test 7 possible cuts for each splitting plane test
  */
  for(d = 0; d < 3; d++) {
    /* Eight Splitting plane tests */
    for(s = 0; s < 7; s++) {
      /* Generate box sizes */

      /* Left Child */
      cmin[0] = min;
      cmax[0] = max;

      /* Right Child */
      cmin[1] = min;
      cmax[1] = max;

      math_vec_add(center, max, min);
      math_vec_mul_scalar(vec, min, ((tfloat)(s+1)));
      math_vec_mul_scalar(center, max, ((tfloat)(7-s)));
      math_vec_add(center, center, vec);
      math_vec_mul_scalar(center, center, 0.125);

      cmax[0].v[d] = center.v[d];
      cmin[1].v[d] = center.v[d];

      /* Compute cost by examining surface area of all triangles for both nodes */
      for(n = 0; n < 2; n++) {
        cnt[n] = 0;

        math_vec_add(center, cmax[n], cmin[n]);
        math_vec_mul_scalar(center, center, 0.5);
        math_vec_sub(half_size, cmax[n], cmin[n]);
        math_vec_mul_scalar(half_size, half_size, 0.5);

        /*
        * compute surface area
        * surface area of 3d triangle:
        *  1/2 (V1-V0) x (V2 - V0)
        *  where 'x' is cross product.
        */
        sa = 0;
        for(i = 0; i < node_geom_data->tri_num; i++) {

        }
        sa /= (tfloat)node->geom_data->tri_num;
      }



    }
  }

}
#endif

  /* Allocate 2 children nodes for the parent node */
  node->data = (void *)malloc(2 * sizeof(tie_kdtree_t));

  ((tie_kdtree_t *)(node->data))[0].data = malloc(sizeof(tie_geom_t));
  ((tie_kdtree_t *)(node->data))[1].data = malloc(sizeof(tie_geom_t));

  /* Initialize Triangle List */
  child[0] = ((tie_geom_t *)(((tie_kdtree_t *)(node->data))[0].data));
  child[1] = ((tie_geom_t *)(((tie_kdtree_t *)(node->data))[1].data));

  child[0]->tri_list = (tie_tri_t **)malloc(sizeof(tie_tri_t *) * node_geom_data->tri_num);
  child[0]->tri_num = 0;

  child[1]->tri_list = (tie_tri_t **)malloc(sizeof(tie_tri_t *) * node_geom_data->tri_num);
  child[1]->tri_num = 0;


  /* 
  * Determine if the triangles touch either of the two children nodes,
  * if it does insert it into them respectively.
  */
  for(n = 0; n < 2; n++) {
    cnt[n] = 0;

    math_vec_add(center, cmax[n], cmin[n]);
    math_vec_mul_scalar(center, center, 0.5);
    math_vec_sub(half_size, cmax[n], cmin[n]);
    math_vec_mul_scalar(half_size, half_size, 0.5);

    for(i = 0; i < node_geom_data->tri_num; i++) {
      /*
      * Check to see if any triangle points are inside of the node before
      * spending alot of cycles on the full blown triangle box overlap
      */
      for(j = 0; j < 3; j++)
        if(node_geom_data->tri_list[i]->data[j].v[0] > cmin[n].v[0] &&
           node_geom_data->tri_list[i]->data[j].v[0] < cmax[n].v[0] &&
           node_geom_data->tri_list[i]->data[j].v[1] > cmin[n].v[1] &&
           node_geom_data->tri_list[i]->data[j].v[1] < cmax[n].v[1] &&
           node_geom_data->tri_list[i]->data[j].v[2] > cmin[n].v[2] &&
           node_geom_data->tri_list[i]->data[j].v[2] < cmax[n].v[2]) {
           j = 4;
        }

      if(j == 5) {
        child[n]->tri_list[child[n]->tri_num++] = node_geom_data->tri_list[i];
        cnt[n]++;
      } else {
        if(tie_tri_box_overlap(&center, &half_size, node_geom_data->tri_list[i]->data)) {
          child[n]->tri_list[child[n]->tri_num++] = node_geom_data->tri_list[i];
          cnt[n]++;
        }
      }
    }

    /* Resize Tri List to actual ammount of memory used */
    child[n]->tri_list = (tie_tri_t **)realloc(child[n]->tri_list, sizeof(tie_tri_t *)*child[n]->tri_num);
  }

  /*
  * Now that the triangles have been propogated to the appropriate child nodes,
  * free the triangle list on this node.
  */
  node_geom_data->tri_num = 0;
  free(node_geom_data->tri_list);
  free(node_geom_data);

  /* Push each child through the same process. */
  tie_build_tree(tie, &((tie_kdtree_t *)(node->data))[0], depth+1, cmin[0], cmax[0], n1cost);
  tie_build_tree(tie, &((tie_kdtree_t *)(node->data))[1], depth+1, cmin[1], cmax[1], n2cost);
 
  /* Assign the splitting dimension to the node */
  /* If we've come this far then YES, this node DOES have child nodes, MARK it as so. */
  node->data = (void*)((TIE_PTR_CAST)(node->data) + split + 4);
}


/*************************************************************
 **************** EXPORTED FUNCTIONS *************************
 *************************************************************/

/**
 * Initialize a tie_t data structure
 *
 * This needs to be called before any other libtie data structures are called.
 *
 * @param tie pointer to a struct tie_t
 * @return void
 */
void tie_init(tie_t *tie, int tri_num) {
  tie->kdtree = NULL;
  tie->tri_num = 0;
  tie->tri_list = (tie_tri_t *)malloc(sizeof(tie_tri_t) * tri_num);
  tie->stat = 0;
  tie->rays_fired = 0;
}

/**
 * Free up all the stuff associate with libtie
 *
 * All of the KDTREE nodes and triangles that we have allocated need to
 * be freed in a controlled manner.  This routine does that.
 *
 * @param tie pointer to a struct tie_t
 * @return void
 */
void tie_free(tie_t *tie) {
  int			i;

  /* Free Triangle Data */
  for(i = 0; i < tie->tri_num; i++)
    free( (void*)((TIE_PTR_CAST)(tie->tri_list[i].v12) & ~0x7L) );
  free(tie->tri_list);

  /* Free KDTREE Nodes */
  if(tie->kdtree) /* prevent tie from crashing when a tie_free() is called right after a tie_init() */
    tie_free_node(tie->kdtree);
  free(tie->kdtree);
}


/**
 * Get ready to shoot rays at triangles
 *
 * Build the KDTREE tree for the triangles we have
 *
 * @param tie pointer to a struct tie_t which now has all the triangles in it
 * @return void
 */
void tie_prep(tie_t *tie) {
  TIE_3		delta;

  tie_insert(tie, tie->tri_list, tie->tri_num);

  if(!tie->kdtree)
    return;

  /* Trim KDTREE back from power of 2 size (set during insert phase) to number of actual triangles */
  ((tie_geom_t *)(tie->kdtree->data))->tri_list = (tie_tri_t **)realloc(((tie_geom_t *)(tie->kdtree->data))->tri_list, sizeof(tie_tri_t *) * ((tie_geom_t *)(tie->kdtree->data))->tri_num);

  /*
  * Compute Floating Fuzz Precision Value
  * For now, take largest dimension as basis for TIE_PREC
  */
  math_vec_sub(delta, tie->max, tie->min);
  math_max3(TIE_PREC, delta.v[0], delta.v[1], delta.v[2]);
#if TIE_SINGLE_PREC
  TIE_PREC *= 0.000001;
#else
  TIE_PREC *= 0.000000000001;
#endif

  /* Grow the head node a little bit to avoid floating point fuzz in the building process */
  math_vec_mul_scalar(delta, delta, 0.01);
  math_vec_sub(tie->min, tie->min, delta);
  math_vec_add(tie->max, tie->max, delta);

  /* Compute Max Depth to allow the KD-Tree to grow to */
  tie->max_depth = (int)(TIE_KDTREE_DEPTH_K1 * (log(tie->tri_num) / log(2)) + TIE_KDTREE_DEPTH_K2);
  printf("max_depth: %d\n", tie->max_depth);

  /* Build the KDTREE */
  tie_build_tree(tie, tie->kdtree, 0, tie->min, tie->max, ((tie_geom_t *)(tie->kdtree->data))->tri_num);

  /* Prep all the triangles */
  tie_tri_prep(tie);

  printf("stat: %d\n", tie->stat);
/*  exit(0); */ /* uncomment to profile prep phase only */
  tie->stat = 0;
}


/**
 * Shoot a ray at some triangles
 *
 * The user-provided hitfunc is called at each ray/triangle intersection.
 * Calls are guaranteed to be made in the ray-intersection order.  
 * The last argument (void *ptr) is passed to the hitfunc as-is, to allow
 * application specific data to be passed to the hitfunc.
 *
 * @param tie a struct tie_t universe
 * @param ray the ray to be intersected with the geometry
 * @param id the intersection data for each intersection
 * @param hitfunc the application routine to be called upon ray/triangle intersection.
 * This function should return 0 if the ray is to continue propagating through the geometry,
 * or non-zero if ray intersection should cease.  
 * @param ptr a pointer to be passed to the hitfunc when it is called.
 *
 * @return the return value from the user hitfunc() is used.
 * In the event that the ray does not hit anything, or the ray exits the geometry space, a null value will be returned.
 * @retval 0 ray did not hit anything, or ray was propagated through the geometry completely.
 * @retval !0 the value returned from the last invokation of hitfunc()
 */
void* tie_work(tie_t *tie, tie_ray_t *ray, tie_id_t *id, void *(*hitfunc)(tie_ray_t*, tie_id_t*, tie_tri_t*, void *ptr), void *ptr) {
  tie_stack_t stack[40];
  tie_id_t t, id_list[256];
  tie_tri_t *hit_list[256], *tri;
  tie_geom_t *data;
  tie_kdtree_t *node_aligned, *temp[2];
  tfloat near, far, dirinv[3], dist;
  int i, n, ab[3], split, stack_ind, hit_count;
  void *result;


  if(!tie->kdtree)
    return(NULL);

  ray->kdtree_depth = 0;

  /*
  * Precompute direction inverse since it's used in a bunch of divides,
  * this allows those divides to become fast multiplies.
  */
  for(i = 0; i < 3; i++) {
    if(ray->dir.v[i] == 0)
      ray->dir.v[i] = TIE_PREC;
    dirinv[i] = 1.0 / ray->dir.v[i];
    ab[i] = dirinv[i] < 0 ? 1 : 0;
  }

  /* Extracting value of splitting plane from tie->kdtree pointer */
  split = ((TIE_PTR_CAST)(((tie_kdtree_t *)((TIE_PTR_CAST)tie->kdtree & ~0x7L))->data)) & 0x3;

  /* Initialize ray segment */
  if(ray->dir.v[split] < 0) {
    far = (tie->min.v[split] - ray->pos.v[split]) * dirinv[split];
  } else {
    far = (tie->max.v[split] - ray->pos.v[split]) * dirinv[split];
  }

  stack_ind = 0;
  stack[0].node = tie->kdtree;
  stack[0].near = 0;
  stack[0].far = far;

  /* Process items on the stack */
  while(stack_ind >= 0) {
    near = stack[stack_ind].near;
    far = stack[stack_ind].far;

    /*
    * Take the pointer from stack[stack_ind] and remove lower pts bits used to store data to
    * give a valid ptr address.
    */
    node_aligned = (tie_kdtree_t *)((TIE_PTR_CAST)stack[stack_ind].node & ~0x7L);
    stack_ind--;

    /*
    * KDTREE TRAVERSAL
    *
    * 3 conditions can happen here:
    *   - Ray only intersects the nearest node
    *   - Ray only intersects the furthest node
    *   - Ray intersects both nodes, pushing the furthest onto the stack
    *
    * Gordon Stoll's Mantra - Rays are Measured in Millions :-)
    */
    while(((TIE_PTR_CAST)(node_aligned->data)) & 0x4) {
      ray->kdtree_depth++;

      /* Retreive the splitting plane */
      split = ((TIE_PTR_CAST)(node_aligned->data)) & 0x3;

      /* Calculate the projected 1d distance to splitting axis */
      dist = (node_aligned->axis - ray->pos.v[split]) * dirinv[split];

      temp[0] = &((tie_kdtree_t *)(node_aligned->data))[ab[split]];
      temp[1] = &((tie_kdtree_t *)(node_aligned->data))[1-ab[split]];

      i = near >= dist; // Node B Only?
      node_aligned = (tie_kdtree_t *)((TIE_PTR_CAST)(temp[i]) & ~0x7L);

      if(far < dist || i)
        continue;

      /* Nearest Node and Push Furthest */
      stack_ind++;
      stack[stack_ind].node = temp[1];
      stack[stack_ind].near = dist;
      stack[stack_ind].far = far;
      far = dist;
    }


    /*
    * RAY/TRIANGLE INTERSECTION - Only gets executed on geometry nodes.
    * This part of the function is being executed because the KDTREE Traversal is Complete.
    */
    hit_count = 0;

    data = (tie_geom_t *)(node_aligned->data);

    for(i = 0; i < data->tri_num; i++) {
      /*
      * Triangle Intersection Code
      */
      tfloat u0, v0, *v;
      int i1, i2;

      tri = data->tri_list[i];
      math_vec_dot(u0, tri->data[1], ray->pos);
      math_vec_dot(v0, tri->data[1], ray->dir);
      t.dist = -(tri->data[2].v[0] + u0) / v0;

      /*
      * Intersection point on triangle must lie within the kdtree node or it is rejected
      * Apply TIE_PREC to near and far such that triangles that lie on orthogonal planes
      * aren't in a precision fuzz boundary, thus missing something they should actualy
      * have hit.
      */
      if(t.dist < near-TIE_PREC || t.dist > far+TIE_PREC)
        continue;

      /* Compute Intersection Point (P = O + Dt) */
      math_vec_mul_scalar(t.pos, ray->dir, t.dist);
      math_vec_add(t.pos, ray->pos, t.pos);

      /* Extract i1 and i2 indices from lower bits of the v12 pointer */
      i1 = TIE_TAB1[((TIE_PTR_CAST)(tri->v12) & 0x7)];
      i2 = TIE_TAB1[3 + ((TIE_PTR_CAST)(tri->v12) & 0x7)];

      /* Compute U and V */
      u0 = t.pos.v[i1] - tri->data[0].v[i1];
      v0 = t.pos.v[i2] - tri->data[0].v[i2];

      v = (tfloat *)((TIE_PTR_CAST)(tri->v12) & ~0x7L);

      /*
      * Compute the barycentric coordinates, and make sure the coordinates
      * fall within the boundaries of the triangle plane.
      */
      if(fabs(tri->data[2].v[1]) <= TIE_PREC) {
        t.beta = u0 / tri->data[2].v[2];
        if(t.beta < 0 || t.beta > 1)
          continue;
        t.alpha = (v0 - t.beta*v[1]) / v[0];
      } else {
        t.beta = (v0*tri->data[2].v[1] - u0*v[0]) / (v[1]*tri->data[2].v[1] - tri->data[2].v[2]*v[0]);
        if(t.beta < 0 || t.beta > 1)
          continue;
        t.alpha = (u0 - t.beta*tri->data[2].v[2]) / tri->data[2].v[1];
      }

      if(t.alpha < 0 || (t.alpha + t.beta) > 1.0)
        continue;

      /* Triangle Intersected, append it in the list */
      hit_list[hit_count] = tri;
      id_list[hit_count] = t;
      hit_count++;
    }


    /* If we hit something, then sort the hit triangles on demand */
    for(i = 0; i < hit_count; i++) {
      /* Sort the list so that HitList and IDList [n] is in order wrt [i] */
      for(n = i; n < hit_count; n++) {
        if(id_list[n].dist < id_list[i].dist) {
          /* Swap */
          tri = hit_list[i];
          t = id_list[i];
          hit_list[i] = hit_list[n];
          id_list[i] = id_list[n];
          hit_list[n] = tri;
          id_list[n] = t;
        }
      }

      id_list[i].norm = hit_list[i]->data[1];
      result = hitfunc(ray, &id_list[i], hit_list[i], ptr);

      if(result) {
        *id = id_list[i];
        return(result);
      }
    }
  }

  return(NULL);
}


/**
 * Add a triangle
 *
 * Add a new triangle to the universe to be raytraced.
 *
 * @param tie the universe
 * @param v0 vertex of triangle
 * @param v1 vertex of triangle
 * @param v2 vertex of triangle
 * @param ptr value to be stashed in triangle data structure for reference by the hitfunc() routine.
 * Typically, this is a pointer to a mesh data structure.  It can be anything the application hitfunc() routine might
 * want to get at from the triangle data structure.
 * @return void
 */
void tie_push(tie_t *tie, TIE_3 *tlist, int tnum, void *plist, int pstride) {
  int i, ind, step;
  void *mem;

  step = 2 * sizeof(tfloat);
  mem = malloc(tnum * step);
  ind = 0;
  for(i = 0; i < tnum; i++) {
    tie->tri_list[tie->tri_num].data[0] = tlist[i*3+0];
    tie->tri_list[tie->tri_num].data[1] = tlist[i*3+1];
    tie->tri_list[tie->tri_num].data[2] = tlist[i*3+2];
    if(plist) {
      tie->tri_list[tie->tri_num].ptr = plist;
      plist += pstride;
    } else {
      tie->tri_list[tie->tri_num].ptr = NULL;
    }
    tie->tri_list[tie->tri_num].v12 = (tfloat *)&((char *)mem)[ind];
    ind += step;
    tie->tri_num++;
  }
}

/** @} */
