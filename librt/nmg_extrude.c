/*		N M G _ E X T R U D E . C
 *	Routines for extruding nmg's. Currently, just a single
 *	face extruder into an nmg solid.
 *
 *  Authors -
 *	Michael Markowski
 *  
 *  Source -
 *      The US Army Research Laboratory
 *      Aberdeen Proving Ground, Maryland  21005-5066
 *
 *  Copyright Notice -
 *	This software is Copyright (C) 1993 by the United States Army.
 *	All rights reserved.
 */
#include <stdio.h>
#include <math.h>
#include <string.h>
#include "machine.h"
#include "vmath.h"
#include "db.h"
#include "nmg.h"
#include "raytrace.h"
#include "rtgeom.h"
#include "rtlist.h"
#include "./debug.h"

/*
 *	E x t r u d e _ N M G _ F a c e
 *
 *	Duplicate a given NMG face, move it by specified vector,
 *	and create a solid bounded by these faces.
 */
nmg_extrude_face(fu, Vec, tol)
struct faceuse	*fu;	/* Face to extrude. */
vect_t		Vec;	/* Magnitude and direction of extrusion. */
struct rt_tol	*tol;	/* NMG tolerances. */
{
	fastf_t		cosang;
	int		cnt, i, j, nfaces;
	struct edgeuse	*eu;
	struct faceuse	*back, *front, *fu2, *nmg_dup_face(), **outfaceuses;
	struct loopuse	*lu, *lu2;
	struct shell	*s;
	struct vertex	*v, *vertlist[4], **verts, **verts2;

#define MIKE_TOL 0.0001

	j = 0;

	/* Duplicate face. */
	fu2 = nmg_dup_face(fu, fu->s_p);

	/* Figure out which face to flip. */
	cosang = VDOT(Vec, fu->f_p->fg_p->N);
	front = fu;
	back = fu2;
	if (NEAR_ZERO(cosang, MIKE_TOL)) {
		rt_bomb("extrude_nmg_face: extrusion cannot be parallel to face\n");
	} else if (cosang > 0.) {
		flip_nmg_face(back, tol);
		translate_nmg_face(front, Vec, tol);
	} else if (cosang < 0.) {
		flip_nmg_face(back, tol);
		translate_nmg_face(back, Vec, tol);
	}

	lu = (struct loopuse *)((&front->lu_hd)->forw);
	lu2 = (struct loopuse *)((&back->lu_hd)->forw);
	nfaces = verts_in_nmg_face(front);
	outfaceuses = (struct faceuse **)
		rt_malloc((nfaces+2) * sizeof(struct faceuse *), "faces");

	do {
		cnt = verts_in_nmg_loop(lu);
		if (cnt < 3)
			rt_bomb("extrude_nmg_face: need at least 3 points\n");
		verts = (struct vertex **)
			rt_malloc((cnt+1)*sizeof(struct vertex *), "verts");
		verts2 = (struct vertex **)
			rt_malloc((cnt+1)*sizeof(struct vertex *), "verts");

		/* Collect vertex structures from 1st face. */
		i = 0;
		NMG_CK_LOOPUSE(lu);
		if (RT_LIST_FIRST_MAGIC(&lu->down_hd) == NMG_EDGEUSE_MAGIC) {
			for (RT_LIST_FOR(eu, edgeuse, &lu->down_hd)) {
				verts[i++] = eu->vu_p->v_p;
			}
		} else
			rt_bomb("extrude_nmg_face: bad loopuse (1)\n");

		/* Collect vertex structures from 2nd face. */
		i = 0;
		NMG_CK_LOOPUSE(lu2);
		if (RT_LIST_FIRST_MAGIC(&lu2->down_hd) == NMG_EDGEUSE_MAGIC) {
			for (RT_LIST_FOR(eu, edgeuse, &lu2->down_hd)) {
				verts2[cnt-i-1] = eu->vu_p->v_p;
				i++;
			}
		} else
			rt_bomb("extrude_nmg_face: bad loopuse (2)\n");

		verts[cnt] = verts[0];
		verts2[cnt] = verts2[0];

		for (i = 0; i < cnt; i++) {
			/* Generate connecting faces. */
			vertlist[0] = verts[i];
			vertlist[1] = verts2[i];
			vertlist[2] = verts2[i+1];
			vertlist[3] = verts[i+1];
			outfaceuses[2+i+j] = nmg_cface(fu->s_p, vertlist, 4);
		}
		j += cnt;

		/* Free memory. */
		rt_free((char *)verts, "verts");
		rt_free((char *)verts2, "verts");

		/* On to next loopuse. */
		lu = (struct loopuse *)((struct rt_list *)(lu))->forw;
		lu2 = (struct loopuse *)((struct rt_list *)(lu2))->forw;

	} while (lu != (struct loopuse *)(&fu->lu_hd));

	outfaceuses[0] = fu;
	outfaceuses[1] = fu2;

	/* Associate the face geometry. */
	for (i = 0; i < nfaces+2; i++) {
		if (nmg_fu_planeeqn(outfaceuses[i], tol) < 0)
			return(-1);	/* FAIL */
	}

	/* Glue the edges of different outward pointing face uses together. */
	nmg_gluefaces(outfaceuses, nfaces+2);

	/* Compute geometry for region and shell. */
	nmg_region_a(fu->s_p->r_p);

	/* Free memory. */
	rt_free((char *)outfaceuses, "faces");
}

/*
 *	F l i p _ N M G _ F a c e
 *
 *	Given a pointer to a faceuse, flip the face by reversing the
 *	order of vertex pointers in each loopuse.
 */
flip_nmg_face(fu, tol)
struct faceuse	*fu;
struct rt_tol	*tol;
{
	int		cnt,		/* Number of vertices in face. */
			i;
	struct vertex	**verts;	/* List of verts in face. */
	struct edgeuse	*eu;
	struct loopuse	*lu, *lu2;
	struct vertex	*v;

	/* Go through each loop and flip it. */
	for (RT_LIST_FOR(lu, loopuse, &fu->lu_hd)) {
		cnt = verts_in_nmg_loop(lu);	/* # of vertices in loop. */
		verts = (struct vertex **)
			rt_malloc(cnt * sizeof(struct vertex *), "verts");

		/* Collect vertex structure pointers from current loop. */
		i = 0;
		NMG_CK_LOOPUSE(lu);
		if (RT_LIST_FIRST_MAGIC(&lu->down_hd) == NMG_EDGEUSE_MAGIC) {
			for (RT_LIST_FOR(eu, edgeuse, &lu->down_hd)) {
				verts[i++] = eu->vu_p->v_p;
			}
		} else if (RT_LIST_FIRST_MAGIC(&lu->down_hd)
			== NMG_VERTEXUSE_MAGIC) {
			v = RT_LIST_PNEXT(vertexuse, &lu->down_hd)->v_p;
			verts[i++] = v;
		} else
			rt_bomb("extrude_nmg_face: loopuse mess up!\n");

		/* Reverse order of vertex structures in current loop. */
		i = 0;
		if (RT_LIST_FIRST_MAGIC(&lu->down_hd)
			== NMG_EDGEUSE_MAGIC) {
			for (RT_LIST_FOR(eu, edgeuse, &lu->down_hd)) {
				eu->vu_p->v_p = verts[cnt-i-1];
				i++;
			}
		} else if (RT_LIST_FIRST_MAGIC(&lu->down_hd)
			== NMG_VERTEXUSE_MAGIC) {
			RT_LIST_PNEXT(vertexuse, &lu->down_hd)->v_p
				= verts[cnt-i-1];
			i++;
		} else
			rt_bomb("extrude_nmg_face: loopuse mess up!\n");

		rt_free((char *)verts, "verts");
	}

	nmg_fu_planeeqn(fu, tol);
}

/*
 *	V e r t s _ i n _ N M G _ L o o p
 *
 *	Count number of vertices in an NMG loop.
 */
int
verts_in_nmg_loop(lu)
struct loopuse	*lu;
{
	int		cnt;
	struct edgeuse	*eu;
	struct vertex	*v;

	/* Count number of vertices in loop. */
	cnt = 0;
	NMG_CK_LOOPUSE(lu);
	if (RT_LIST_FIRST_MAGIC(&lu->down_hd) == NMG_EDGEUSE_MAGIC) {
		for (RT_LIST_FOR(eu, edgeuse, &lu->down_hd)) {
			NMG_CK_EDGEUSE(eu);
			NMG_CK_EDGE(eu->e_p);
			NMG_CK_VERTEXUSE(eu->vu_p);
			NMG_CK_VERTEX(eu->vu_p->v_p);
			cnt++;
		}
	} else if (RT_LIST_FIRST_MAGIC(&lu->down_hd) == NMG_VERTEXUSE_MAGIC) {
		v = RT_LIST_PNEXT(vertexuse, &lu->down_hd)->v_p;
		NMG_CK_VERTEX(v);
		cnt++;
	} else
		rt_bomb("verts_in_nmg_loop: bad loopuse\n");
	return(cnt);
}

/*
 *	V e r t s _ i n _ N M G _ F a c e
 *
 *	Count number of vertices in an NMG face.
 */
int
verts_in_nmg_face(fu)
struct faceuse	*fu;
{
	int		cnt;
	struct loopuse	*lu;

	cnt = 0;
	for (RT_LIST_FOR(lu, loopuse, &fu->lu_hd))
		cnt += verts_in_nmg_loop(lu);
	return(cnt);
}

/*
 *	T r a n s l a t e _ N M G _ F a c e
 *
 *	Translate a face using a vector's magnitude and direction.
 */
translate_nmg_face(fu, Vec, tol)
struct faceuse	*fu;
vect_t		Vec;
struct rt_tol	*tol;
{
	int		cnt,		/* Number of vertices in face. */
			cur,
			i,
			in_there;
	struct vertex	**verts;	/* List of verts in face. */
	struct edgeuse	*eu;
	struct loopuse	*lu;
	struct vertex	*v;

	cur = 0;
	cnt = verts_in_nmg_face(fu);
	verts = (struct vertex **)
		rt_malloc(cnt * sizeof(struct vertex *), "verts");
	for (i = 0; i < cnt; i++)
		verts[i] = NULL;

	/* Go through each loop and translate it. */
	for (RT_LIST_FOR(lu, loopuse, &fu->lu_hd)) {
		NMG_CK_LOOPUSE(lu);
		if (RT_LIST_FIRST_MAGIC(&lu->down_hd) == NMG_EDGEUSE_MAGIC) {
			for (RT_LIST_FOR(eu, edgeuse, &lu->down_hd)) {
				in_there = 0;
				for (i = 0; i < cur && !in_there; i++)
					if (verts[i] == eu->vu_p->v_p)
						in_there = 1;
				if (!in_there) {
					verts[cur++] = eu->vu_p->v_p;
					VADD2(eu->vu_p->v_p->vg_p->coord,
						eu->vu_p->v_p->vg_p->coord,
						Vec);
				}
			}
		} else if (RT_LIST_FIRST_MAGIC(&lu->down_hd)
			== NMG_VERTEXUSE_MAGIC) {
			VADD2(v->vg_p->coord, v->vg_p->coord, Vec);
		} else
			rt_bomb("translate_nmg_face: bad loopuse\n");
	}

	nmg_fu_planeeqn(fu, tol);
	rt_free((char *)verts, "verts");
}
