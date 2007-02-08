/*                     K D T R E E . C
 * BRL-CAD / ADRT
 *
 * Copyright (c) 2002-2007 United States Government as represented by
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
/** @addtogroup libtie */
/** @{ */
/** @file tie.c
 *
 *  @brief support routines for shooting at triangles
 *  Comments -
 *      KD-Tree Routines
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

#include "kdtree.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

TFLOAT TIE_PREC;

/*************************************************************
 **************** PRIVATE FUNCTIONS **************************
 *************************************************************/


static void tie_kdtree_free_node(tie_kdtree_t *node) {
  tie_kdtree_t *node_aligned = (tie_kdtree_t *)((intptr_t)node & ~0x7L);

  if(((intptr_t)(node_aligned->data)) & 0x4) {
    /* Node Data is KDTREE Children, Recurse */
    tie_kdtree_free_node(&((tie_kdtree_t *)(((intptr_t)(node_aligned->data)) & ~0x7L))[0]);
    tie_kdtree_free_node(&((tie_kdtree_t *)(((intptr_t)(node_aligned->data)) & ~0x7L))[1]);
    free((void*)((intptr_t)(node_aligned->data) & ~0x7L));
  } else {
    /* This node points to a geometry node, free it */
    free(((tie_geom_t *)((intptr_t)(node_aligned->data) & ~0x7L))->tri_list);
    free((void *)((intptr_t)(node_aligned->data) & ~0x7L));
  }
}


static void tie_kdtree_cache_free_node(tie_t *tie, tie_kdtree_t *node, void **cache) {
  tie_kdtree_t *node_aligned = (tie_kdtree_t *)((intptr_t)node & ~0x7L);
  unsigned int size, mem, tri_num, i, tri_ind;
  unsigned char type, split;

  memcpy(&size, *cache, sizeof(unsigned int));
  memcpy(&mem, &((char *)*cache)[sizeof(unsigned int)], sizeof(unsigned int));
  /*
  * If the available size for this cache is under 1MB, then grow it 4MB larger.
  * This reduces the number of realloc's required.  Makes things much much faster
  * on systems like FreeBSD that do a full copy for each realloc.
  */
  if(mem - size < 1<<20) {
    mem += 1<<23;
    memcpy(&((char *)*cache)[sizeof(unsigned int)], &mem, sizeof(unsigned int));
    *cache = realloc(*cache, mem);
  }

  if(((intptr_t)(node_aligned->data)) & 0x4) {
    /* Create a KD-Tree Node in the cache */
    type = 0;
    memcpy(&((char *)*cache)[size], &type, 1);
    size += 1;
    memcpy(&((char *)*cache)[size], &(node_aligned->axis), sizeof(TFLOAT));
    size += sizeof(TFLOAT);
    split = ((intptr_t)(node_aligned->data)) & 0x3;
    memcpy(&((char *)*cache)[size], &split, 1);
    size += 1;

    /* Update size of cache */
    memcpy(*cache, &size, sizeof(unsigned int));

    /* Node Data is KDTREE Children, Recurse */
    tie_kdtree_cache_free_node(tie, &((tie_kdtree_t *)(((intptr_t)(node_aligned->data)) & ~0x7L))[0], cache);
    tie_kdtree_cache_free_node(tie, &((tie_kdtree_t *)(((intptr_t)(node_aligned->data)) & ~0x7L))[1], cache);
    free((void *)((intptr_t)(node_aligned->data) & ~0x7L));
  } else {
    tri_num = ((tie_geom_t *)((intptr_t)(node_aligned->data) & ~0x7L))->tri_num;
    type = 1;
    memcpy(&((char *)*cache)[size], &type, 1);
    size += 1;
    memcpy(&((char *)*cache)[size], &tri_num, sizeof(unsigned int));

    size += sizeof(unsigned int);

    for(i = 0; i < tri_num; i++) {
      /*
      * Pointer subtraction gives us the index of the triangle since the block of memory
      * that the triangle exists in is contiguous memory.
      */
      tri_ind = ((tie_geom_t *)((intptr_t)(node_aligned->data) & ~0x7L))->tri_list[i] - &tie->tri_list[0];
      memcpy(&((char *)*cache)[size], &tri_ind, sizeof(unsigned int));
      size += sizeof(unsigned int);
    }

    /* Update size of cache */
    memcpy(*cache, &size, sizeof(unsigned int));

    /* This node points to a geometry node, free it */
    free(((tie_geom_t *)((intptr_t)(node_aligned->data) & ~0x7L))->tri_list);
    free((void *)((intptr_t)(node_aligned->data) & ~0x7L));
  }
}


static void tie_kdtree_prep_head(tie_t *tie, tie_tri_t *tri_list, int tri_num) {
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

    MATH_BBOX(tie->min, tie->max, tri_list[0].data[0], tri_list[0].data[1], tri_list[0].data[2]);

    g->tri_list = (tie_tri_t **)malloc(sizeof(tie_tri_t *) * tri_num);

    /* form bounding box of scene */
    for(i = 0; i < tri_num; i++) {
      g->tri_list[i] = &tri_list[i];

      /* Get Bounding Box of Triangle */
      MATH_BBOX(min, max, tri_list[i].data[0], tri_list[i].data[1], tri_list[i].data[2]);

      /* Check to see if defines a new Max or Min point */
      MATH_VEC_MIN(tie->min, min);
      MATH_VEC_MAX(tie->max, max);
      /* printf("Box: [%.3f, %.3f, %.3f] [%.3f, %.3f, %.3f]\n", tie->min.v[0], tie->min.v[1], tie->min.v[2], tie->max.v[0], tie->max.v[1], tie->max.v[2]); */
    }

    ((tie_geom_t *)(tie->kdtree->data))->tri_num = tri_num;
  }
}


static int tie_kdtree_tri_box_overlap(TIE_3 *center, TIE_3 *half_size, TIE_3 triverts[3]) {
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
  TFLOAT min, max, d, t, rad;

  /* move everything so that the boxcenter is in (0,0,0) */
  MATH_VEC_SUB(v0, triverts[0], (*center));
  MATH_VEC_SUB(v1, triverts[1], (*center));
  MATH_VEC_SUB(v2, triverts[2], (*center));

  /*
  * First test overlap in the {x,y,z}-directions
  * find min, max of the triangle each direction, and test for overlap in
  * that direction -- this is equivalent to testing a minimal AABB around
  * the triangle against the AABB
  */

  /* test in X-direction */
  MATH_MIN3(min, v0.v[0], v1.v[0], v2.v[0]);
  MATH_MAX3(max, v0.v[0], v1.v[0], v2.v[0]);
  if(min > half_size->v[0] || max < -half_size->v[0])
    return 0;

  /* test in Y-direction */
  MATH_MIN3(min, v0.v[1], v1.v[1], v2.v[1]);
  MATH_MAX3(max, v0.v[1], v1.v[1], v2.v[1]);
  if(min > half_size->v[1] || max < -half_size->v[1])
    return 0;

  /* test in Z-direction */
  MATH_MIN3(min, v0.v[2], v1.v[2], v2.v[2]);
  MATH_MAX3(max, v0.v[2], v1.v[2], v2.v[2]);
  if(min > half_size->v[2] || max < -half_size->v[2])
    return 0;

  /* compute triangle edges */
  MATH_VEC_SUB(e0, v1, v0); /* tri edge 0 */
  MATH_VEC_SUB(e1, v2, v1); /* tri edge 1 */
  MATH_VEC_SUB(e2, v0, v2); /* tri edge 2 */

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
  MATH_VEC_CROSS(normal, e0, e1);
  MATH_VEC_DOT(d, normal, v0);  /* plane eq: normal . x + d = 0 */

  p.v[0] = normal.v[0] > 0 ? half_size->v[0] : -half_size->v[0];
  p.v[1] = normal.v[1] > 0 ? half_size->v[1] : -half_size->v[1];
  p.v[2] = normal.v[2] > 0 ? half_size->v[2] : -half_size->v[2];

  MATH_VEC_DOT(t, normal, p);
  return t >= d ? 1 : 0;
}


static void tie_kdtree_build(tie_t *tie, tie_kdtree_t *node, int depth, TIE_3 min, TIE_3 max, int node_a, int node_b) {
  tie_geom_t *child[2], *node_gd = (tie_geom_t *)(node->data);
  TIE_3 cmin[2], cmax[2], center[2], half_size[2];
  int i, j, n, split, cnt[2];

#if 0
  /*  if(depth >= 26) */
    printf("%f %f %f %f %f %f\n", min.v[0], min.v[1], min.v[2], max.v[0], max.v[1], max.v[2]);
#endif

  /* Terminating criteria for KDTREE subdivision */
  if(node_gd->tri_num <= TIE_KDTREE_NODE_MAX || depth > tie->max_depth) {
    /*    tie->stat++; */
    tie->stat += node_gd->tri_num;
#if 0
    if(node_gd->tri_num > tie->stat)
      tie->stat = node_gd->tri_num;
    if(node_gd->tri_num > tie->stat) {
      tie->stat = node_gd->tri_num;
      printf("depth: %d, tris: %d\n", depth, node_gd->tri_num);
      printf("%f %f %f %f %f %f\n", min.v[0], min.v[1], min.v[2], max.v[0], max.v[1], max.v[2]);
    }
    exit(0);
#endif
    return;
  }

#if 0
{
  /**********************
  * MID-SPLIT ALGORITHM *
  ***********************/
  TIE_3 vec;

  /* Left Child */
  cmin[0] = min;
  cmax[0] = max;

  /* Right Child */
  cmin[1] = min;
  cmax[1] = max;

  MATH_VEC_ADD(center[0], max, min);
  MATH_VEC_MUL_SCALAR(center[0], center[0], 0.5);

  /* Split along largest Axis to keep node sizes relatively cube-like (Naive) */
  MATH_VEC_SUB(vec, max, min);

  /* Determine the largest Axis */
  if(vec.v[0] >= vec.v[1] && vec.v[0] >= vec.v[2]) {
    cmax[0].v[0] = center[0].v[0];
    cmin[1].v[0] = center[0].v[0];
    node->axis = center[0].v[0];
    split = 0;
  } else if(vec.v[1] >= vec.v[0] && vec.v[1] >= vec.v[2]) {
    cmax[0].v[1] = center[0].v[1];
    cmin[1].v[1] = center[0].v[1];
    node->axis = center[0].v[1];
    split = 1;
  } else {
    cmax[0].v[2] = center[0].v[2];
    cmin[1].v[2] = center[0].v[2];
    node->axis = center[0].v[2];
    split = 2;
  }
}
#else
{
  /****************************************
  * Justin's Aggressive KD-Tree Algorithm *
  *****************************************/
  int slice[3][MAX_SLICES+MIN_SLICES], gap[3][2], active, split_slice;
  int side[3][MAX_SLICES+MIN_SLICES][2], d, s, k, smax[3], smin, slice_num;
  TFLOAT coef[3][MAX_SLICES+MIN_SLICES], split_coef, beg, end, d_min, d_max;
  tie_tri_t *tri;

  /*
  * Calculate number of slices to use as a function of triangle density.
  * Slices as a function of relative node size does not work so well.
  */
  slice_num = MIN_SLICES + MAX_SLICES * ((TFLOAT)node_gd->tri_num / (TFLOAT)tie->tri_num);

  for(d = 0; d < 3; d++) {
    /*
    * Optimization: Walk each triangle and find the min and max for the given dimension
    * of the complete triangle list.  This will tell us what slices we needn't bother
    * doing any computations for.
    */
    for(i = 0; i < node_gd->tri_num; i++) {
      tri = node_gd->tri_list[i];
      /* Set min anx max */
      MATH_MIN3(tri->v[0], tri->data[0].v[d], tri->data[1].v[d], tri->data[2].v[d]);
      MATH_MAX3(tri->v[1], tri->data[0].v[d], tri->data[1].v[d], tri->data[2].v[d]);

      /* Clamp to node AABB */
      if(tri->v[0] < min.v[d])
	tri->v[0] = min.v[d];
      if(tri->v[1] > max.v[d])
	tri->v[1] = max.v[d];

      if(i == 0 || tri->v[0] < d_min)
	d_min = tri->v[0];

      if(i == 0 || tri->v[1] > d_max)
	d_max = tri->v[1];
    }

    for(k = 0; k < slice_num; k++) {
      slice[d][k] = 0;
      side[d][k][0] = 0;
      side[d][k][1] = 0;

      /* Left Child */
      cmin[0] = min;
      cmax[0] = max;

      /* Right Child */
      cmin[1] = min;
      cmax[1] = max;

      /* construct slices so as not to use the boundaries as slices */
      coef[d][k] = ((TFLOAT)k / (TFLOAT)(slice_num-1)) * (TFLOAT)(slice_num-2) / (TFLOAT)slice_num + (TFLOAT)1 / (TFLOAT)slice_num;
      cmax[0].v[d] = min.v[d]*(1.0-coef[d][k]) + max.v[d]*coef[d][k];
      cmin[1].v[d] = cmax[0].v[d];

      if(cmax[0].v[d] < d_min || cmax[0].v[d] > d_max)
	continue;

      for(n = 0; n < 2; n++) {
	MATH_VEC_ADD(center[n], cmax[n], cmin[n]);
	MATH_VEC_MUL_SCALAR(center[n], center[n], 0.5);
	MATH_VEC_SUB(half_size[n], cmax[n], cmin[n]);
	MATH_VEC_MUL_SCALAR(half_size[n], half_size[n], 0.5);
      }

      for(i = 0; i < node_gd->tri_num; i++) {
	/*
	* Optimization: If the points for the triangle of the dimension being tested
	* do not span the cutting plane, then do not bother with the next test.
	*/
	if((node_gd->tri_list[i]->data[0].v[d] > cmax[0].v[d] &&
	    node_gd->tri_list[i]->data[1].v[d] > cmax[0].v[d] &&
	    node_gd->tri_list[i]->data[2].v[d] > cmax[0].v[d])||
	   (node_gd->tri_list[i]->data[0].v[d] < cmax[0].v[d] &&
	    node_gd->tri_list[i]->data[1].v[d] < cmax[0].v[d] &&
	    node_gd->tri_list[i]->data[2].v[d] < cmax[0].v[d]))
	  continue;

	/* Check that the triangle is in both node A and B for it to span. */
	s = 0;
	for(n = 0; n < 2; n++) {
	  /*
	  * Check to see if any triangle points are inside of the node before
	  * spending alot of cycles on the full blown triangle box overlap
	  */
	  for(j = 0; j < 3; j++)
	    if(node_gd->tri_list[i]->data[j].v[0] > cmin[n].v[0] &&
	       node_gd->tri_list[i]->data[j].v[0] < cmax[n].v[0] &&
	       node_gd->tri_list[i]->data[j].v[1] > cmin[n].v[1] &&
	       node_gd->tri_list[i]->data[j].v[1] < cmax[n].v[1] &&
	       node_gd->tri_list[i]->data[j].v[2] > cmin[n].v[2] &&
	       node_gd->tri_list[i]->data[j].v[2] < cmax[n].v[2]) {
	       j = 4;
	    }

	  if(j == 5) {
	    s++;
	    side[d][k][n]++;
	  } else {
	    if(tie_kdtree_tri_box_overlap(&center[n], &half_size[n], node_gd->tri_list[i]->data)) {
	      s++;
	      side[d][k][n]++;
	    }
	  }
	}

	if(s == 2)
	  slice[d][k]++;
      }
    }
  }

  /* Store the max value from each of the 3 Slice arrays */
  for(d = 0; d < 3; d++) {
    smax[d] = 0;
    for(k = 0; k < slice_num; k++) {
      if(slice[d][k] > smax[d])
	smax[d] = slice[d][k];
    }
  }

  /*
  * To eliminate "empty" areas, build a list of spans where geometric complexity is
  * less than MIN_SPAN of the overal nodes size and then selecting the splitting plane
  * the corresponds to the span slice domain nearest the center to bias towards a balanced tree
  */

  for(d = 0; d < 3; d++) {
    gap[d][0] = 0;
    gap[d][1] = 0;
    beg = 0;
    end = 0;
    active = 0;

    for(k = 0; k < slice_num; k++) {
/*      printf("slice[%d][%d]: %d < %d\n", d, k, slice[d][k], (int)(MIN_DENSITY * (TFLOAT)smax[d])); */
      if(slice[d][k] < (int)(MIN_DENSITY * (TFLOAT)smax[d])) {
	if(!active) {
	  active = 1;
	  beg = k;
	  end = k;
	} else {
	  end = k;
	}
      } else {
	if(active) {
	  if(end - beg > gap[d][1] - gap[d][0]) {
	    gap[d][0] = beg;
	    gap[d][1] = end;
	  }
	}
	active = 0;
      }
    }

    if(active)
      if(end - beg > gap[d][1] - gap[d][0]) {
	gap[d][0] = beg;
	gap[d][1] = end;
      }
  }

#if 0
  printf("gap_x: %d->%d = %d\n", gap[0][0], gap[0][1], gap[0][1]-gap[0][0]);
  printf("gap_y: %d->%d = %d\n", gap[1][0], gap[1][1], gap[1][1]-gap[1][0]);
  printf("gap_z: %d->%d = %d\n", gap[2][0], gap[2][1], gap[2][1]-gap[2][0]);
#endif

  /*
  * If there is a gap atleast MIN_SPAN in side wrt the nodes dimension size
  * then use the nearest edge of the gap to 0.5 as the splitting plane,
  * Use the the gap with the largest span.
  * If no gaps are found meeting the criteria then weight the span values to
  * bias towards a balanced kd-tree and choose the minima of that weighted curve.
  */

  /* Largest gap */
  d = 0;
  if(gap[1][1] - gap[1][0] > gap[d][1] - gap[d][0])
    d = 1;
  if(gap[2][1] - gap[2][0] > gap[d][1] - gap[d][0])
    d = 2;

  /*
  * Largest gap found must meet MIN_SPAN requirements
  * There must be atleast 500 triangles or we don't bother.
  * Lower triangle numbers means there is a higher probability that
  * triangles lack any sort of coherent structure.
  */
  if((TFLOAT)(gap[d][1] - gap[d][0]) / (TFLOAT)slice_num > MIN_SPAN && node_gd->tri_num > 500) {
/*  printf("choosing slice[%d]: %d->%d :: %d tris\n", d, gap[d][0], gap[d][1], node_gd->tri_num); */
    split = d;
    if(abs(gap[d][0] - slice_num/2) < abs(gap[d][1] - slice_num/2)) {
      /* choose gap[d][0] as splitting plane */
      split_coef = ((TFLOAT)gap[d][0] / (TFLOAT)(slice_num-1)) * (TFLOAT)(slice_num-2) / (TFLOAT)slice_num + (TFLOAT)1 / (TFLOAT)slice_num;
      split_slice = gap[d][0];
    } else {
      /* choose gap[d][1] as splitting plane */
      split_coef = ((TFLOAT)gap[d][1] / (TFLOAT)(slice_num-1)) * (TFLOAT)(slice_num-2) / (TFLOAT)slice_num + (TFLOAT)1 / (TFLOAT)slice_num;
      split_slice = gap[d][1];
    }
  } else {
    /*
    * Weight the slices based on a heuristic driven linear scaling function to bias values
    * towards the center as more desirable.  This solves the case of a partially linear graph
    * to prevent marching to determine a desirable splitting point.  If this section of code
    * is being executed it's typically because most 'empty space' has now been eliminated
    * and/or the resulting geometry is now losing structure as the smaller cells are being
    * created, i.e dividing a fraction of a wing-nut instead of an engine-block.
    */
    for(d = 0; d < 3; d++) {
      for(k = 0; k < slice_num; k++) {
	slice[d][k] += fabs(coef[d][k]-0.5) * SCALE_COEF * smax[d];
/*        printf("%.3f %d\n", coef[d][k], slice[d][k]); */
      }
    }

    /* Choose the slice with the graphs minima as the splitting plane. */
    split = 0;
    smin = tie->tri_num;
    split_coef = 0.5;
    for(d = 0; d < 3; d++) {
      for(k = 0; k < slice_num; k++) {
	if(slice[d][k] < smin) {
	  split_coef = coef[d][k];
	  split = d;
	  split_slice = k;
	  smin = slice[d][k];
	}
      }
    }

    /*
    * If the dimension chosen to split along has a value of 0 for the maximum value
    * then the geometry was aligned such that it fell undetectable between the slices
    * and therefore was not picked up by the marching slices.  In the event that this
    * happens, choose to naively split along the middle as this last ditch decision
    * will give better results than the algorithm naively picking the first of the
    * the slices forming these irregular, short followed by a long box, splits.
    */
    if(smax[split] == 0) {
      split_coef = coef[split][slice_num / 2];
      split_slice = slice_num / 2;
    }
  }

  /*
  * Lastly, after we have supposedly chosen the most ideal splitting point,
  * check to see that the subdivision that is about to take place is worth
  * doing.  In other words, if both children have the same number of triangles
  * as the parent does then stop.
  */
  if(side[split][split_slice][0] == node_gd->tri_num && side[split][split_slice][1] == node_gd->tri_num) {
    tie->stat += node_gd->tri_num;
    return;
  }

#if 0
  if(side[split][split_slice][0] == node_a && side[split][split_slice][1] == node_b) {
    if(node_gd->tri_num < 10)
      return;
    /*      printf("%f %f %f %f %f %f\n", min.v[0], min.v[1], min.v[2], max.v[0], max.v[1], max.v[2]); */
    /*      printf("moo: %d - %d\n", depth, node_gd->tri_num); */
  }
#endif


#if 0
  printf("winner: depth: %d, dim = %d, smin = %d, coef: %.3f\n", depth, split, smin, split_coef);
  printf("winner: min: %.3f %.3f %.3f, max: %.3f %.3f %.3f, tris: %d\n", min.v[0], min.v[1], min.v[2], max.v[0], max.v[1], max.v[2], node_gd->tri_num);
#endif

  /* Based on the winner, construct the two child nodes */
  /* Left Child */
  cmin[0] = min;
  cmax[0] = max;

  /* Right Child */
  cmin[1] = min;
  cmax[1] = max;

  cmax[0].v[split] = min.v[split]*(1.0-split_coef) + max.v[split]*split_coef;
  cmin[1].v[split] = cmax[0].v[split];
  node->axis = cmax[0].v[split];
}
#endif


  /* Allocate 2 children nodes for the parent node */
  node->data = (void *)malloc(2 * sizeof(tie_kdtree_t));

  ((tie_kdtree_t *)(node->data))[0].data = malloc(sizeof(tie_geom_t));
  ((tie_kdtree_t *)(node->data))[1].data = malloc(sizeof(tie_geom_t));

  /* Initialize Triangle List */
  child[0] = ((tie_geom_t *)(((tie_kdtree_t *)(node->data))[0].data));
  child[1] = ((tie_geom_t *)(((tie_kdtree_t *)(node->data))[1].data));

  child[0]->tri_list = (tie_tri_t **)malloc(sizeof(tie_tri_t *) * node_gd->tri_num);
  child[0]->tri_num = 0;

  child[1]->tri_list = (tie_tri_t **)malloc(sizeof(tie_tri_t *) * node_gd->tri_num);
  child[1]->tri_num = 0;


  /*
  * Determine if the triangles touch either of the two children nodes,
  * if it does insert it into them respectively.
  */
  for(n = 0; n < 2; n++) {
    cnt[n] = 0;

    MATH_VEC_ADD(center[n], cmax[n], cmin[n]);
    MATH_VEC_MUL_SCALAR(center[n], center[n], 0.5);
    MATH_VEC_SUB(half_size[n], cmax[n], cmin[n]);
    MATH_VEC_MUL_SCALAR(half_size[n], half_size[n], 0.5);

    for(i = 0; i < node_gd->tri_num; i++) {
      /*
      * Check to see if any triangle points are inside of the node before
      * spending alot of cycles on the full blown triangle box overlap
      */
      for(j = 0; j < 3; j++)
	if(node_gd->tri_list[i]->data[j].v[0] > cmin[n].v[0] &&
	   node_gd->tri_list[i]->data[j].v[0] < cmax[n].v[0] &&
	   node_gd->tri_list[i]->data[j].v[1] > cmin[n].v[1] &&
	   node_gd->tri_list[i]->data[j].v[1] < cmax[n].v[1] &&
	   node_gd->tri_list[i]->data[j].v[2] > cmin[n].v[2] &&
	   node_gd->tri_list[i]->data[j].v[2] < cmax[n].v[2]) {
	   j = 4;
	}

      if(j == 5) {
	child[n]->tri_list[child[n]->tri_num++] = node_gd->tri_list[i];
	cnt[n]++;
      } else {
	if(tie_kdtree_tri_box_overlap(&center[n], &half_size[n], node_gd->tri_list[i]->data)) {
	  child[n]->tri_list[child[n]->tri_num++] = node_gd->tri_list[i];
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
  node_gd->tri_num = 0;
  free(node_gd->tri_list);
  free(node_gd);

  /* Push each child through the same process. */
  tie_kdtree_build(tie, &((tie_kdtree_t *)(node->data))[0], depth+1, cmin[0], cmax[0], cnt[0], cnt[1]);
  tie_kdtree_build(tie, &((tie_kdtree_t *)(node->data))[1], depth+1, cmin[1], cmax[1], cnt[0], cnt[1]);

  /* Assign the splitting dimension to the node */
  /* If we've come this far then YES, this node DOES have child nodes, MARK it as so. */
  node->data = (void *)((intptr_t)(node->data) + split + 4);
}


/*************************************************************
 **************** EXPORTED FUNCTIONS *************************
 *************************************************************/

/**
 * Free up all the stuff associated with the kdtree
 *
 * All of the KDTREE nodes and triangles that we have allocated need to
 * be freed in a controlled manner.  This routine does that.
 *
 * @param tie pointer to a struct tie_t
 * @return void
 */
void tie_kdtree_free(tie_t *tie) {
  /* Free KDTREE Nodes */
  /* prevent tie from crashing when a tie_free() is called right after a tie_init() */
  if(tie->kdtree)
    tie_kdtree_free_node(tie->kdtree);
  free(tie->kdtree);
}


/**
 * Free up all the stuff associated with the kdtree and build a
 * cache as the data is freed.  Building the cache while the data
 * is freed allows the peak memory not to go any higher than it
 * already is.  If there were seprarate cache and free functions
 * then the cache would exist in memory while the triangles and
 * kd-tree were in memory thus severly limiting optimal memory
 * usage.
 *
 * All of the KDTREE nodes and triangles that we have allocated need to
 * be freed in a controlled manner.  This routine does that.
 *
 * @param tie pointer to a struct tie_t and void **cache to store data
 * @return void
 */
void tie_kdtree_cache_free(tie_t *tie, void **cache) {
  unsigned int size;

  /*
  * Free KDTREE Node
  * Prevent tie from crashing when a tie_free() is called right after a tie_init()
  */
  if(tie->kdtree) {
    *cache = malloc(2 * sizeof(unsigned int));
    size = 2*sizeof(unsigned int);
    memcpy(*cache, &size, sizeof(unsigned int));
    memcpy(&((char *)*cache)[sizeof(unsigned int)], &size, sizeof(unsigned int));

    /* Build the cache */
    tie_kdtree_cache_free_node(tie, tie->kdtree, cache);

    /* Resize the array back to it's real value */
    memcpy(&size, *cache, sizeof(unsigned int));
    *cache = realloc(*cache, size);
  }
  free(tie->kdtree);
}


void tie_kdtree_cache_load(tie_t *tie, void *cache) {
  tie_kdtree_t *node, *temp_node, *stack[64];
  tie_geom_t *geom;
  TIE_3 min, max;
  unsigned int i, size, index, tri_ind, stack_ind;
  char type, split;


  if(!cache)
    return;

  memcpy(&size, cache, sizeof(unsigned int));
  /* Advance past the first (2) unsigned ints to the actualy data */
  index = 2*sizeof(unsigned int);
  stack_ind = 0;

  while(index < size) {
    memcpy(&type, &((char *)cache)[index], 1);
    index += 1;

    if(type) {
      /* Geometry Node - Allocate a tie_geom_t and assign to node->data. */
      node->data = malloc(sizeof(tie_geom_t));
      geom = (tie_geom_t *)node->data;

      memcpy(&(geom->tri_num), &((char *)cache)[index], sizeof(unsigned int));
      index += sizeof(unsigned int);

      geom->tri_list = (tie_tri_t **)malloc(geom->tri_num * sizeof(tie_tri_t *));
      for(i = 0; i < geom->tri_num; i++) {
	memcpy(&tri_ind, &((char *)cache)[index], sizeof(unsigned int));
	index += sizeof(unsigned int);

	/* Translate the numerical index to a pointer index into tie->tri_list. */
	geom->tri_list[i] = &tie->tri_list[0] + tri_ind;
      }

      if(stack_ind) {
	stack_ind--;
	node = stack[stack_ind];
      }
    } else {
      /* KD-Tree Node */
      if(!tie->kdtree) {
	tie->kdtree = (tie_kdtree_t *)malloc(sizeof(tie_kdtree_t));
	node = tie->kdtree;
      }

      /* Assign splitting axis value */
      memcpy(&node->axis, &((char *)cache)[index], sizeof(TFLOAT));
      index += sizeof(TFLOAT);

      /* Get splitting plane */
      memcpy(&split, &((char *)cache)[index], 1);
      index += 1;

      /* Allocate memory for 2 child nodes */
      node->data = malloc(2 * sizeof(tie_kdtree_t));

      /* Push B on the stack and Process A */
      stack[stack_ind] = &((tie_kdtree_t *)node->data)[1];
      stack_ind++;

      /* Set the new current node */
      temp_node = node;
      node = &((tie_kdtree_t *)node->data)[0];

      /*
      * Mask the splitting plane and mark it as a kdtree node
      * using the lower bits of the ptr.
      */
      temp_node->data = (void *)((intptr_t)(temp_node->data) + split + 4);
    }
  }

  /* form bounding box of scene */
  MATH_BBOX(tie->min, tie->max, tie->tri_list[0].data[0], tie->tri_list[0].data[1], tie->tri_list[0].data[2]);
  for(i = 0; i < tie->tri_num; i++) {
    /* Get Bounding Box of Triangle */
    MATH_BBOX(min, max, tie->tri_list[i].data[0], tie->tri_list[i].data[1], tie->tri_list[i].data[2]);

    /* Check to see if defines a new Max or Min point */
    MATH_VEC_MIN(tie->min, min);
    MATH_VEC_MAX(tie->max, max);
  }
}


/**
 * Get ready to shoot rays at triangles
 *
 * Build the KDTREE tree for the triangles we have
 *
 * @param tie pointer to a struct tie_t which now has all the triangles in it
 * @return void
 */
void tie_kdtree_prep(tie_t *tie) {
  TIE_3 delta;
  int already_built;


  already_built = tie->kdtree ? 1 : 0;

  /* Set bounding volume and make head node a geometry node */
  if(!already_built)
    tie_kdtree_prep_head(tie, tie->tri_list, tie->tri_num);

  if(!tie->kdtree)
    return;

  /* Trim KDTREE to number of actual triangles if it's not that size already. */
  if(!already_built)
    ((tie_geom_t *)(tie->kdtree->data))->tri_list = (tie_tri_t **)realloc(((tie_geom_t *)(tie->kdtree->data))->tri_list, sizeof(tie_tri_t *) * ((tie_geom_t *)(tie->kdtree->data))->tri_num);

  /*
  * Compute Floating Fuzz Precision Value
  * For now, take largest dimension as basis for TIE_PREC
  */
  MATH_VEC_SUB(delta, tie->max, tie->min);
  MATH_MAX3(TIE_PREC, delta.v[0], delta.v[1], delta.v[2]);
#if TIE_SINGLE_PREC
  TIE_PREC *= 0.000001;
#else
  TIE_PREC *= 0.000000000001;
#endif

  /* Grow the head node to avoid floating point fuzz in the building process with edges */
  MATH_VEC_MUL_SCALAR(delta, delta, 1.0); /* XXX */
  MATH_VEC_SUB(tie->min, tie->min, delta);
  MATH_VEC_ADD(tie->max, tie->max, delta);

  /* Compute Max Depth to allow the KD-Tree to grow to */
  tie->max_depth = (int)(TIE_KDTREE_DEPTH_K1 * (log(tie->tri_num) / log(2)) + TIE_KDTREE_DEPTH_K2);
  printf("max_depth: %d\n", tie->max_depth);

  /* Build the KDTREE */
  if(!already_built)
    tie_kdtree_build(tie, tie->kdtree, 0, tie->min, tie->max, 0, 0);

  printf("stat: %d\n", tie->stat);
  tie->stat = 0;

/*  exit(0); */ /* uncomment to profile prep phase only */
}

/** @} */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
