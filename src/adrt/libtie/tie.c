/*                     T I E . C
 *
 * @file tie.c
 *
 * BRL-CAD
 *
 * Copyright (c) 2002-2006 United States Government as represented by
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
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "struct.h"

#ifdef TIE_SSE
# include <xmmintrin.h>
#endif

/*************************************************************
 **************** PRIVATE FUNCTIONS **************************
 *************************************************************/

static void tie_tri_prep(tie_t *tie) {
  TIE_3 v1, v2, u, v;
  int i, i1, i2;
  tie_tri_t *tri;

  for(i = 0; i < tie->tri_num; i++) {
    tri = &tie->tri_list[i];

    v1 = tri->data[1];
    v2 = tri->data[2];

    /* Compute Normal */
    MATH_VEC_SUB(u, tri->data[1], tri->data[0]);
    MATH_VEC_SUB(v, tri->data[2], tri->data[0]);
    MATH_VEC_CROSS(tri->data[1], u, v);
    MATH_VEC_UNITIZE(tri->data[1]);

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
    tri->v[0] = v1.v[i2] - tri->data[0].v[i2];
    tri->v[1] = v2.v[i2] - tri->data[0].v[i2];

    if(i1 == 0 && i2 == 1) {
      tri->v = (tfloat *)((intptr_t)(tri->v) + 2);
    } else if (i1 == 0) {
      tri->v = (tfloat *)((intptr_t)(tri->v) + 1);
    }

    /* Compute DotVN */
    MATH_VEC_MUL_SCALAR(v1, tri->data[0], -1.0);
    MATH_VEC_DOT(tri->data[2].v[0], v1, tri->data[1]);
  }
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
void tie_init(tie_t *tie, unsigned int tri_num) {
  tie->kdtree = NULL;
  tie->tri_num = 0;
  tie->tri_num_alloc = tri_num;
  tie->tri_list = (tie_tri_t *)malloc(sizeof(tie_tri_t) * tri_num);
  tie->stat = 0;
  tie->rays_fired = 0;
}

/**
 * Free up all the stuff associated with libtie
 *
 * All of the KDTREE nodes and triangles that we have allocated need to
 * be freed in a controlled manner.  This routine does that.
 *
 * @param tie pointer to a struct tie_t
 * @return void
 */
void tie_free(tie_t *tie) {
  int i;

  /* Free Triangle Data */
  for(i = 0; i < tie->tri_num; i++)
    free( (void *)((intptr_t)(tie->tri_list[i].v) & ~0x7L) );
  free(tie->tri_list);

  /* Free KDTREE Nodes */
  tie_kdtree_free(tie);
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
  /* Build the kd-tree */
  tie_kdtree_prep(tie);

  /* Prep all the triangles */
  tie_tri_prep(tie);
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
  split = ((intptr_t)(((tie_kdtree_t *)((intptr_t)tie->kdtree & ~0x7L))->data)) & 0x3;

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
    node_aligned = (tie_kdtree_t *)((intptr_t)stack[stack_ind].node & ~0x7L);
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
    while(((intptr_t)(node_aligned->data)) & 0x4) {
      ray->kdtree_depth++;

      /* Retreive the splitting plane */
      split = ((intptr_t)(node_aligned->data)) & 0x3;

      /* Calculate the projected 1d distance to splitting axis */
      dist = (node_aligned->axis - ray->pos.v[split]) * dirinv[split];

      temp[0] = &((tie_kdtree_t *)(node_aligned->data))[ab[split]];
      temp[1] = &((tie_kdtree_t *)(node_aligned->data))[1-ab[split]];

      i = near >= dist; /* Node B Only? */
      node_aligned = (tie_kdtree_t *)((intptr_t)(temp[i]) & ~0x7L);

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
    data = (tie_geom_t *)(node_aligned->data);
    if(data->tri_num == 0)
      continue;

    hit_count = 0;
    for(i = 0; i < data->tri_num; i++) {
      /*
      * Triangle Intersection Code
      */
      tfloat u0, v0, *v;
      int i1, i2;

      tri = data->tri_list[i];
      MATH_VEC_DOT(u0, tri->data[1], ray->pos);
      MATH_VEC_DOT(v0, tri->data[1], ray->dir);
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
      MATH_VEC_MUL_SCALAR(t.pos, ray->dir, t.dist);
      MATH_VEC_ADD(t.pos, ray->pos, t.pos);

      /* Extract i1 and i2 indices from lower bits of the v pointer */
      v = (tfloat *)((intptr_t)(tri->v) & ~0x7L);

      i1 = TIE_TAB1[((intptr_t)(tri->v) & 0x7)];
      i2 = TIE_TAB1[3 + ((intptr_t)(tri->v) & 0x7)];

      /* Compute U and V */
      u0 = t.pos.v[i1] - tri->data[0].v[i1];
      v0 = t.pos.v[i2] - tri->data[0].v[i2];

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
      if(hit_count < 0xff) {
        hit_list[hit_count] = tri;
        id_list[hit_count] = t;
        hit_count++;
      }


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
 * @param tlist is an array of TIE_3 vertice triplets (v0, v1, v2) that form each triangle.
 * @param tnum is the number of triangles (tlist = 3 * tnum of TIE_3's).
 * @param plist is a list of pointer data that gets assigned to the ptr of each triangle.
 * This will typically be 4-byte (32-bit architecture) spaced array of pointers that
 * associate the triangle pointer with your arbitrary structure, i.e a mesh.
 * @param pstride is the number of bytes to increment the pointer list as it assigns
 * a pointer to each mesh, typically a value of 4 (for 32-bit machines).  If you have
 * a single pointer that groups all triangles to a common structure then you can use
 * a value of 0 for pstride.  This will give the pointer of all triangles the pointer
 * address of plist.
 * @return void
 */
void tie_push(tie_t *tie, TIE_3 *tlist, int tnum, void *plist, int pstride) {
  int i;

  if(tnum + tie->tri_num > tie->tri_num_alloc) {
    tie->tri_list = (tie_tri_t *)realloc(tie->tri_list, sizeof(tie_tri_t) * (tie->tri_num + tnum));
    tie->tri_num_alloc += tnum;
  }

  for(i = 0; i < tnum; i++) {
    tie->tri_list[tie->tri_num].data[0] = tlist[i*3+0];
    tie->tri_list[tie->tri_num].data[1] = tlist[i*3+1];
    tie->tri_list[tie->tri_num].data[2] = tlist[i*3+2];
    if(plist) {
      tie->tri_list[tie->tri_num].ptr = plist;
      plist = (void *)((intptr_t)plist + pstride);
    } else {
      tie->tri_list[tie->tri_num].ptr = NULL;
    }
    tie->tri_list[tie->tri_num].v = (tfloat *)malloc(2*sizeof(tfloat));
    tie->tri_num++;
  }
}

/** @} */
