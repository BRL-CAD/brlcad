/*                           T I E . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2014 United States Government as represented by
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
/** @file primitives/bot/tie.c
 *  tie.c
 *
 */

#include "tie.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#ifdef TIE_SSE
# include <xmmintrin.h>
#endif

#ifdef HAVE_STDINT_H
# include <stdint.h>
#endif

#include "bio.h"
#include "bu.h"

#include "tieprivate.h"

#define	TIE_TAB1		"\1\0\0\2\2\1"	/* Triangle Index Table */

#define TIE_DEGENERATE_THRESHOLD 0.0001

/*************************************************************
 **************** PRIVATE FUNCTIONS **************************
 *************************************************************/

static void
TIE_VAL(tie_tri_prep)(struct tie_s *tie)
{
    TIE_3 v1, v2, u, v;
    unsigned int i, i1, i2;
    struct tie_tri_s *tri;
    fastf_t mag_sq;

    for (i = 0; i < tie->tri_num; i++) {
	tri = &tie->tri_list[i];

	v1 = tri->data[1];
	v2 = tri->data[2];

	/* Compute Normal */
	VSUB2(u.v,  tri->data[1].v,  tri->data[0].v);
	VSUB2(v.v,  tri->data[2].v,  tri->data[0].v);
	VCROSS(tri->data[1].v,  u.v,  v.v);

	/* Unitize Normal */
	mag_sq = MAGSQ(tri->data[1].v);
	if (UNLIKELY(mag_sq < SMALL_FASTF)) {
	    /* Can not unitize normal, most likely we have a zero area
	     * triangle (i.e. degenerate) so skip it.
	     */
	    continue;
	}
	VSCALE(tri->data[1].v, tri->data[1].v, 1.0/sqrt(mag_sq));

	/* Compute i1 and i2 */
	u.v[0] = fabs(tri->data[1].v[0]);
	u.v[1] = fabs(tri->data[1].v[1]);
	u.v[2] = fabs(tri->data[1].v[2]);

	if (u.v[2] > u.v[1] && u.v[2] > u.v[0]) {
	    i1 = 0;
	    i2 = 1;
	} else if (u.v[1] > u.v[2] && u.v[1] > u.v[0]) {
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

	/* looks like this is intentionally injected to give the
	 * compiler something to do if/while v1 and tri->data are
	 * preloaded, instead of just getting set in the preceding
	 * conditional above.
	 */
	if (i1 == 0 && i2 == 1)
	    tri->b = tri->b + 2;
	else if (i1 == 0)
	    tri->b = tri->b + 1;

	/* Compute DotVN */
	VSCALE(v1.v,  tri->data[0].v,  -1.0);
	tri->data[2].v[0] = VDOT( v1.v,  tri->data[1].v);
    }
}


/*************************************************************
 **************** EXPORTED FUNCTIONS *************************
 *************************************************************/

/**
 * Initialize a struct tie_s data structure
 *
 * This needs to be called before any other libtie data structures are called.
 *
 * @param tie pointer to a struct tie_t
 * @param tri_num initial number of triangles to allocate for. tie_push may
 *		    expand the buffer, if needed
 * @param kdmethod Either TIE_KDTREE_FAST or TIE_KDTREE_OPTIMAL
 * @return void
 */
void TIE_VAL(tie_init)(struct tie_s *tie, unsigned int tri_num, unsigned int kdmethod)
{
    tie->kdtree = NULL;
    tie->kdmethod = kdmethod;
    tie->tri_num = 0;
    tie->tri_num_alloc = tri_num;
    /* set to NULL if tri_num == 0. bu_malloc(0) causes issues. */
    tie->tri_list = tri_num?(struct tie_tri_s *)bu_calloc(tri_num, sizeof(struct tie_tri_s), "tie_init"):NULL;
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
void TIE_VAL(tie_free)(struct tie_s *tie)
{
    bu_free(tie->tri_list, "tie_free");

    /* Free KDTREE Nodes */
    tie_kdtree_free(tie);
}


/**
 * Get ready to shoot rays at triangles
 *
 * Build the KDTREE tree for the triangles we have
 *
 * @param tie pointer to a struct struct tie_s which now has all the triangles in it
 * @return void
 */
void TIE_VAL(tie_prep)(struct tie_s *tie)
{
    /* Build the kd-tree */
    tie_kdtree_prep (tie);

    /* Prep all the triangles */
    TIE_VAL(tie_tri_prep) (tie);
}


/**
 * Shoot a ray at some triangles
 *
 * The user-provided hitfunc is called at each ray/triangle intersection.
 * Calls are guaranteed to be made in the ray-intersection order.
 * The last argument (void *ptr) is passed to the hitfunc as-is, to allow
 * application specific data to be passed to the hitfunc.
 *
 * @param tie a struct struct tie_s universe
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
 * @retval !0 the value returned from the last invocation of hitfunc()
 */
void* TIE_VAL(tie_work)(struct tie_s *tie, struct tie_ray_s *ray, struct tie_id_s *id, void *(*hitfunc)(struct tie_ray_s*, struct tie_id_s*, struct tie_tri_s*, void *ptr), void *ptr)
{
    struct tie_stack_s stack[40];
    struct tie_id_s t = {{0,0,0},{0,0,0},0,0,0}, id_list[256];
    struct tie_tri_s *hit_list[256], *tri;
    struct tie_geom_s *data;
    struct tie_kdtree_s *node, *temp[2];
    tfloat near, far, dirinv[3], dist;
    unsigned int i, n;
    uint8_t hit_count;
    int ab[3], split, stack_ind;
    void *result;

    if (!tie->kdtree)
	return NULL;

    ray->kdtree_depth = 0;

    /*
     * Precompute direction inverse since it's used in a bunch of divides,
     * this allows those divides to become fast multiplies.
     */
    for (i = 0; i < 3; i++) {
	if (ZERO(ray->dir[i]))
	    ray->dir[i] = TIE_PREC;
	dirinv[i] = 1.0 / ray->dir[i];
	ab[i] = dirinv[i] < 0.0 ? 1.0 : 0.0;
    }

    /* Extracting value of splitting plane from the kdtree */
    split = tie->kdtree->b & (uint32_t)0x3L;

    /* Initialize ray segment */
    if (ray->dir[split] < 0.0)
	far = (tie->min[split] - ray->pos[split]) * dirinv[split];
    else
	far = (tie->max[split] - ray->pos[split]) * dirinv[split];

    stack_ind = 0;
    stack[0].node = tie->kdtree;
    stack[0].near = 0;
    stack[0].far = far;

    /* Process items on the stack */
    do {
	near = stack[stack_ind].near;
	far = stack[stack_ind].far;
	node = stack[stack_ind].node;
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
	while (node && node->data && TIE_HAS_CHILDREN(node->b)) {
	    ray->kdtree_depth++;

	    /* Retrieve the splitting plane */
	    split = node->b & (uint32_t)0x3L;

	    /* Calculate the projected 1d distance to splitting axis */
	    dist = (node->axis - ray->pos[split]) * dirinv[split];

	    temp[0] = &((struct tie_kdtree_s *)(node->data))[ab[split]];
	    temp[1] = &((struct tie_kdtree_s *)(node->data))[1-ab[split]];

	    i = near >= dist; /* Node B Only? */
	    node = temp[i];

	    if (far < dist || i)
		continue;

	    /* Nearest Node and Push Furthest */
	    stack_ind++;
	    stack[stack_ind].node = temp[1];
	    stack[stack_ind].near = dist;
	    stack[stack_ind].far = far;
	    far = dist;
	}

	if (!node || !node->data) {
	    /* bu_log("INTERNAL ERROR: Unexpected TIE tree traversal encountered\n"); */
	    continue;
	}

	/*
	 * RAY/TRIANGLE INTERSECTION - Only gets executed on geometry nodes.
	 * This part of the function is being executed because the KDTREE Traversal is Complete.
	 */

	hit_count = 0;
	data = (struct tie_geom_s *)(node->data);

	for (i = 0; i < data->tri_num; i++) {
	    /*
	     * Triangle Intersection Code
	     */
	    tfloat u0, v0, *v;
	    int i1, i2;

	    tri = data->tri_list[i];
	    u0 = VDOT( tri->data[1].v,  ray->pos);
	    v0 = VDOT( tri->data[1].v,  ray->dir);
	    t.dist = -(tri->data[2].v[0] + u0) / v0;

	    /*
	     * Intersection point on triangle must lie within the kdtree node or it is rejected
	     * Apply TIE_PREC to near and far such that triangles that lie on orthogonal planes
	     * aren't in a precision fuzz boundary, thus missing something they should actually
	     * have hit.
	     */
	    if (t.dist < near-TIE_PREC || t.dist > far+TIE_PREC)
		continue;

	    /* Compute Intersection Point (P = O + Dt) */
	    VSCALE(t.pos,  ray->dir,  t.dist);
	    VADD2(t.pos,  ray->pos,  t.pos);

	    /* Extract i1 and i2 indices from the 'b' field */
	    v = tri->v;

	    i1 = TIE_TAB1[tri->b & (uint32_t)0x7L];
	    i2 = TIE_TAB1[3 + (tri->b & (uint32_t)0x7L)];

	    /* Compute U and V */
	    u0 = t.pos[i1] - tri->data[0].v[i1];
	    v0 = t.pos[i2] - tri->data[0].v[i2];

	    /*
	     * Compute the barycentric coordinates, and make sure the coordinates
	     * fall within the boundaries of the triangle plane.
	     */
	    if (fabs(tri->data[2].v[1]) <= TIE_PREC) {
		t.beta = u0 / tri->data[2].v[2];
		if (t.beta < 0 || t.beta > 1)
		    continue;
		t.alpha = (v0 - t.beta*v[1]) / v[0];
	    } else {
		t.beta = (v0*tri->data[2].v[1] - u0*v[0]) / (v[1]*tri->data[2].v[1] - tri->data[2].v[2]*v[0]);
		if (t.beta < 0 || t.beta > 1)
		    continue;
		t.alpha = (u0 - t.beta*tri->data[2].v[2]) / tri->data[2].v[1];
	    }

	    if (t.alpha < 0 || (t.alpha + t.beta) > 1.0)
		continue;

	    /* Triangle Intersected, append it in the list */
	    if (hit_count < 0xFF) {
		hit_list[hit_count] = tri;
		id_list[hit_count] = t;
		hit_count++;
	    }
	}


	/* If we hit something, then sort the hit triangles on demand */
	for (i = 0; i < hit_count; i++) {
	    /* Sort the list so that HitList and IDList [n] is in order wrt [i] */
	    for (n = i; n < hit_count; n++) {
		if (id_list[n].dist < id_list[i].dist) {
		    /* Swap */
		    tri = hit_list[i];
		    t = id_list[i];
		    hit_list[i] = hit_list[n];
		    id_list[i] = id_list[n];
		    hit_list[n] = tri;
		    id_list[n] = t;
		}
	    }

	    VMOVE(id_list[i].norm, hit_list[i]->data[1].v);
	    result = hitfunc(ray, &id_list[i], hit_list[i], ptr);

	    if (result) {
		*id = id_list[i];
		return result;
	    }
	}
    } while (stack_ind >= 0);

    return NULL;
}


/**
 * Add a triangle
 *
 * Add a new triangle to the universe to be raytraced.
 *
 * @param tie the universe
 * @param tlist is an array of TIE_3 vertex triplets (v0, v1, v2) that form each triangle.
 * @param tnum is the number of triangles (tlist = 3 * tnum of TIE_3's).
 * @param plist is a list of pointer data that gets assigned to the ptr of each triangle.
 * This will typically be 4-byte (32-bit architecture) spaced array of pointers that
 * associate the triangle pointer with your arbitrary structure, i.e. a mesh.
 * @param pstride is the number of bytes to increment the pointer list as it assigns
 * a pointer to each mesh, typically a value of 4 (for 32-bit machines).  If you have
 * a single pointer that groups all triangles to a common structure then you can use
 * a value of 0 for pstride.  This will give the pointer of all triangles the pointer
 * address of plist.
 * @return void
 */
void TIE_VAL(tie_push)(struct tie_s *tie, TIE_3 **tlist, unsigned int tnum, void *plist, unsigned int pstride)
{
    unsigned int i;

    /* expand the tri buffer if needed */
    if (tnum + tie->tri_num > tie->tri_num_alloc) {
	tie->tri_list = (struct tie_tri_s *)bu_realloc( tie->tri_list, sizeof(struct tie_tri_s) * (tie->tri_num + tnum), "tri_list during tie_push");
	tie->tri_num_alloc += tnum;
    }

    for (i = 0; i < tnum; i++) {

	if (tie_check_degenerate) {
	    TIE_3 u, v, w;

	    VSUB2(u.v,  (*tlist[i*3+1]).v,  (*tlist[i*3+0]).v);
	    VSUB2(v.v,  (*tlist[i*3+2]).v,  (*tlist[i*3+0]).v);
	    VCROSS(w.v,  u.v,  v.v);

	    if (MAGNITUDE(w.v) < 0.0001 * 0.0001) {
		bu_log("WARNING: degenerate triangle found: %f %f %f | %f %f %f | %f %f %f\n",
			V3ARGS((*tlist[i*3+0]).v),  V3ARGS((*tlist[i*3+1]).v), V3ARGS((*tlist[i*3+2]).v));
		continue;
	    }
	}

	/* pack pack pack */
	tie->tri_list[tie->tri_num].data[0] = *tlist[i*3+0];
	tie->tri_list[tie->tri_num].data[1] = *tlist[i*3+1];
	tie->tri_list[tie->tri_num].data[2] = *tlist[i*3+2];

	/* set the association pointer */
	tie->tri_list[tie->tri_num].ptr = plist;
	if (plist)
	    plist = (void *)((intptr_t)plist + pstride);

	V2SETALL(tie->tri_list[tie->tri_num].v, 0.0);
	tie->tri_list[tie->tri_num].b = 0;
	tie->tri_num++;
    }
    return;
}

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
