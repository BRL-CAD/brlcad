/*
 *			N M G _ I N T E R . C
 *
 *	Routines to intersect two NMG regions.
 *	There are no "user interface" routines in here.
 *
 *  Authors -
 *	Lee A. Butler
 *	Michael John Muuss
 *  
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1990 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include <stdio.h>
#include <math.h>
#include "externs.h"
#include "machine.h"
#include "vmath.h"
#include "nmg.h"
#include "raytrace.h"

/* Was nmg_boolstruct, but that name has appeared in nmg.h */
struct nmg_inter_struct {
	struct nmg_ptbl	*l1;		/* vertexuses on the line of */
	struct nmg_ptbl *l2;		/* intersection between planes */
	struct rt_tol	tol;
	point_t		pt;		/* line of intersection */
	vect_t		dir;
	int		coplanar;
};

/*	V E R T E X _ O N _ F A C E
 *
 *	Perform a topological check to
 *	determine if the given vertex can be found in the given faceuse.
 *	If it can, return a pointer to the vertexuse which was found in the
 *	faceuse.
 */
static struct vertexuse *vertex_on_face(v, fu)
struct vertex *v;
struct faceuse *fu;
{
	struct vertexuse *vu;
	struct edgeuse *eu;
	struct loopuse *lu;

#define CKLU_FOR_FU(_lu, _fu, _vu) \
	if (*_lu->up.magic_p == NMG_FACEUSE_MAGIC && _lu->up.fu_p == _fu) \
		return(_vu)

	NMG_CK_VERTEX(v);

	for( RT_LIST_FOR( vu, vertexuse, &v->vu_hd ) )  {
		NMG_CK_VERTEXUSE(vu);
		if (*vu->up.magic_p == NMG_EDGEUSE_MAGIC) {
			eu = vu->up.eu_p;
			if (*eu->up.magic_p == NMG_LOOPUSE_MAGIC) {
				lu = eu->up.lu_p;
				CKLU_FOR_FU(lu, fu, vu);
			}
		} else if (*vu->up.magic_p == NMG_LOOPUSE_MAGIC) {
				lu = vu->up.lu_p;
				CKLU_FOR_FU(lu, fu, vu);
		}
	}

	return((struct vertexuse *)NULL);
}

/*	I S E C T _ V E R T E X _ F A C E
 *
 *	intersect a vertex with a face (primarily for intersecting
 *	loops of a single vertex with a face).
 */
static void isect_vertex_face(bs, vu, fu)
struct nmg_inter_struct *bs;
struct vertexuse *vu;
struct faceuse *fu;
{
	struct loopuse *plu;
	struct vertexuse *vup;
	pointp_t pt;
	fastf_t dist;

	/* check the topology first */	
	if (vup=vertex_on_face(vu->v_p, fu)) {
		(void)nmg_tbl(bs->l1, TBL_INS_UNIQUE, &vu->l.magic);
		(void)nmg_tbl(bs->l2, TBL_INS_UNIQUE, &vup->l.magic);
		return;
	}


	/* since the topology didn't tell us anything, we need to check with
	 * the geometry
	 */
	pt = vu->v_p->vg_p->coord;
	dist = NMG_DIST_PT_PLANE(pt, fu->f_p->fg_p->N);

	if ( !NEAR_ZERO(dist, bs->tol.dist) )  return;

	if (nmg_tbl(bs->l1, TBL_INS_UNIQUE, &vu->l.magic) < 0) {
		if (rt_g.NMG_debug & DEBUG_POLYSECT)
		    	VPRINT("Making vertexloop", vu->v_p->vg_p->coord);

		plu = nmg_mlv(&fu->l.magic, vu->v_p, OT_UNSPEC);
	    	(void)nmg_tbl(bs->l2, TBL_INS_UNIQUE,
			&RT_LIST_FIRST_MAGIC(&plu->down_hd) );
	}
}

/*	I S E C T _ E D G E _ F A C E
 *
 *	Intersect an edge with a face
 *
 * This code currently assumes that an edge can only intersect a face at one
 * point.  This is probably a bad assumption for the future
 */
static void isect_edge_face(bs, eu, fu)
struct nmg_inter_struct *bs;
struct edgeuse *eu;
struct faceuse *fu;
{
	struct vertexuse *vu_other;
	struct vertex	*v1;
	struct vertex	*v1mate;
	point_t		hit_pt;
	vect_t		edge_vect;
	fastf_t		edge_len;	/* MAGNITUDE(edge_vect) */
	fastf_t		dist;		/* parametric dist to hit point */
	fastf_t		dist_to_plane;	/* distance to hit point, in mm */
	int		status;
	struct loopuse	*plu;
	fastf_t		dist1, dist2;
	vect_t		start_pt;

	NMG_CK_EDGEUSE(eu);
	NMG_CK_VERTEXUSE(eu->vu_p);
	v1 = eu->vu_p->v_p;
	NMG_CK_VERTEX(v1);
	NMG_CK_VERTEX_G(v1->vg_p);

	NMG_CK_EDGEUSE(eu->eumate_p);
	NMG_CK_VERTEXUSE(eu->eumate_p->vu_p);
	v1mate = eu->eumate_p->vu_p->v_p;
	NMG_CK_VERTEX(v1mate);
	NMG_CK_VERTEX_G(v1mate->vg_p);

	if (*eu->up.magic_p == NMG_LOOPUSE_MAGIC) {
		struct edgeuse	*eunext;
		struct edgeuse	*eulast;
		/* some edge sanity checking */
		eunext = RT_LIST_PNEXT_CIRC(edgeuse, &eu->l);
		eulast = RT_LIST_PLAST_CIRC(edgeuse, &eu->l);
		NMG_CK_EDGEUSE(eunext);
		NMG_CK_EDGEUSE(eulast);
		if (v1 != eulast->eumate_p->vu_p->v_p) {
			VPRINT("unshared vertex (mine): ",
				v1->vg_p->coord);
			VPRINT("\t\t (last->eumate_p): ",
				eulast->eumate_p->vu_p->v_p->vg_p->coord);
			rt_bomb("isect_edge_face() discontiuous edgeloop\n");
		}
		if( eunext->vu_p->v_p != v1mate) {
			VPRINT("unshared vertex (my mate): ",
				v1mate->vg_p->coord);
			VPRINT("\t\t (next): ",
				eunext->vu_p->v_p->vg_p->coord);
			rt_bomb("isect_edge_face() discontinuous edgeloop\n");
		}
	}

	/*
	 * First check the topology.  If the topology says that either
	 * vertex of this edgeuse is on the other face, enter the
	 * vertexuses in the list and return.
	 */
	if (vu_other=vertex_on_face(v1, fu)) {
		if (rt_g.NMG_debug & DEBUG_POLYSECT) {
			register pointp_t	p1, p2;
			p1 = v1->vg_p->coord;
			p2 = v1mate->vg_p->coord;
			rt_log("Edgeuse %g, %g, %g -> %g, %g, %g\n",
				V3ARGS(p1), V3ARGS(p2) );
			rt_log("\tvertex topologically on intersection plane\n");
		}

		(void)nmg_tbl(bs->l1, TBL_INS_UNIQUE, &eu->vu_p->l.magic);
		(void)nmg_tbl(bs->l2, TBL_INS_UNIQUE, &vu_other->l.magic);
		return;
	}
	if (vu_other=vertex_on_face(v1mate, fu)) {
		if (rt_g.NMG_debug & DEBUG_POLYSECT) {
			register pointp_t	p1, p2;
			p1 = v1->vg_p->coord;
			p2 = v1mate->vg_p->coord;
			rt_log("Edgeuse %g, %g, %g -> %g, %g, %g\n",
				V3ARGS(p1), V3ARGS(p2) );
			rt_log("\tMATE vertex topologically on intersection plane. skipping edgeuse\n");
		}
#if 0
		/* For some reason, this currently causes trouble */
		(void)nmg_tbl(bs->l1, TBL_INS_UNIQUE, &eu->eumate_p->vu_p->l.magic);
		(void)nmg_tbl(bs->l2, TBL_INS_UNIQUE, &vu_other->l.magic);
#endif
		return;
	}

	/*
	 *  Neither vertex lies on the face according to topology.
	 *  Form a ray that starts at one vertex of the edgeuse
	 *  and points to the other vertex.
	 */
	VSUB2(edge_vect, v1mate->vg_p->coord, v1->vg_p->coord);
	edge_len = MAGNITUDE(edge_vect);
#if 0
	if( eu->e_p->eg_p )
#else
	if(0)
#endif
	{
		/* Use ray in edge geometry, for repeatability */
		register struct edge_g	*eg = eu->e_p->eg_p;
		fastf_t		dot;
		NMG_CK_EDGE_G(eg);
		if( MAGSQ( eg->e_dir ) < SQRT_SMALL_FASTF )  {
			rt_log("zero length edge_g\n");
			nmg_pr_eg(eg, "");
			goto calc;
		}
		if( (dot=VDOT( edge_vect, eg->e_dir )) < 0 )  {
			VREVERSE( edge_vect, eg->e_dir );
			if( dot > -0.95 )  {
				rt_log("edge/ray dot=%g!\n", dot);
				goto calc;
			}
		} else {
			VMOVE( edge_vect, eg->e_dir );
			if( dot < 0.95 )  {
				rt_log("edge/ray dot=%g!\n", dot);
				goto calc;
			}
		}
		VMOVE( start_pt, eg->e_pt );
#define VSUBDOT(_pt2, _pt, _dir)	( \
	((_pt2)[X] - (_pt)[X]) * (_dir)[X] + \
	((_pt2)[Y] - (_pt)[Y]) * (_dir)[Y] + \
	((_pt2)[Z] - (_pt)[Z]) * (_dir)[Z] )
		dist1 = VSUBDOT( v1->vg_p->coord, start_pt, edge_vect ) / edge_len;
		dist2 = VSUBDOT( v1mate->vg_p->coord, start_pt, edge_vect ) / edge_len;
rt_log("A dist1=%g, dist2=%g\n", dist1, dist2);
	} else {
calc:
		VMOVE( start_pt, v1->vg_p->coord );
		dist1 = 0;
		dist2 = edge_len;
rt_log("B dist1=%g, dist2=%g\n", dist1, dist2);
	}

	if (rt_g.NMG_debug & DEBUG_POLYSECT)  {
		rt_log("Testing %g, %g, %g -> %g, %g, %g\n",
			V3ARGS(v1->vg_p->coord), V3ARGS(v1mate->vg_p->coord) );
	}

	status = rt_isect_ray_plane(&dist, start_pt, edge_vect, fu->f_p->fg_p->N);

	if (rt_g.NMG_debug & DEBUG_POLYSECT) {
		if (status >= 0)
			rt_log("\tStatus of rt_isect_ray_plane: %d dist: %g\n",
				status, dist);
		else
			rt_log("\tBoring status of rt_isect_ray_plane: %d dist: %g\n",
				status, dist);
	}
	if (status < 0)
		return;		/* No geometric intersection */

	/* The ray defined by the edgeuse intersects the plane 
	 * of the other face.  Check to see if the distance to
         * intersection is between limits of the endpoints of
	 * this edge(use).
	 * The edge exists over values of 0 <= dist <= 1, ie,
	 * over values of 0 <= dist_to_plane <= edge_len.
	 * The tolerance, an absolute distance, can only be compared
	 * to other absolute distances like dist_to_plane & edge_len.
	 * The vertices are "fattened" by +/- bs->tol units.
	 */
	VJOIN1( hit_pt, start_pt, dist, edge_vect );
	dist_to_plane = edge_len * dist;

	if (rt_g.NMG_debug & DEBUG_POLYSECT)
		rt_log("\tedge_len=%g, dist=%g, dist_to_plane=%g\n",
			edge_len, dist, dist_to_plane);

	if ( dist_to_plane < dist1-(bs->tol.dist) )  {
		/* Hit is behind first point */
		if (rt_g.NMG_debug & DEBUG_POLYSECT)
			rt_log("\tplane behind first point\n");
		return;
	}

	/*
	 * If the vertex on the other end of this edgeuse is on the face,
	 * then make a linkage to an existing face vertex (if found),
	 * and give up on this edge, knowing that we'll pick up the
	 * intersection of the next edgeuse with the face later.
	 */
	if ( dist_to_plane < dist1+(bs->tol.dist) )  {
		/* First point is on plane of face, by geometry */
		if (rt_g.NMG_debug & DEBUG_POLYSECT)
			rt_log("\tedge starts at plane intersect\n");
		(void)nmg_tbl(bs->l1, TBL_INS_UNIQUE, &eu->vu_p->l.magic);

		vu_other = nmg_find_vu_in_face(v1->vg_p->coord, fu, &(bs->tol));
		if (vu_other) {
			register pointp_t	p3;
			/* Face has a very similar vertex.  Add to list */
			(void)nmg_tbl(bs->l2, TBL_INS_UNIQUE, &vu_other->l.magic);
			/* Make new coordinates be the midpoint */
			p3 = vu_other->v_p->vg_p->coord;
			VADD2SCALE(v1->vg_p->coord, v1->vg_p->coord, p3, 0.5);
			/* Combine the two vertices */
			nmg_jv(v1, vu_other->v_p);
		} else {
			/* Insert copy of this vertex into face */
			if (rt_g.NMG_debug & DEBUG_POLYSECT)
			    	VPRINT("Making vertexloop",
			    		v1->vg_p->coord);

			plu = nmg_mlv(&fu->l.magic, v1, OT_UNSPEC);
			vu_other = RT_LIST_FIRST( vertexuse, &plu->down_hd );
			NMG_CK_VERTEXUSE(vu_other);
			(void)nmg_tbl(bs->l2, TBL_INS_UNIQUE, &vu_other->l.magic);
		}
		return;
	}
	if ( dist_to_plane < dist2 - bs->tol.dist) {
		struct edgeuse	*euforw;

		/* Intersection is between first and second vertex points.
		 * Insert new vertex at intersection point.
		 */
		if (rt_g.NMG_debug & DEBUG_POLYSECT)  {
			rt_log("Splitting %g, %g, %g <-> %g, %g, %g\n",
				V3ARGS(v1->vg_p->coord), V3ARGS(v1mate->vg_p->coord) );
			VPRINT("\tPoint of intersection", hit_pt);
		}

		/* if we can't find the appropriate vertex in the
		 * other face, we'll build a new vertex.  Otherwise
		 * we re-use an old one.
		 */
		vu_other = nmg_find_vu_in_face(hit_pt, fu, &(bs->tol));
		if (vu_other) {
			/* the other face has a convenient vertex for us */

			if (rt_g.NMG_debug & DEBUG_POLYSECT)
				rt_log("re-using vertex from other face\n");

/*			(void)nmg_esplit(vu_other->v_p, eu->e_p); */
			(void)nmg_ebreak(vu_other->v_p, eu->e_p);
			(void)nmg_tbl(bs->l2, TBL_INS_UNIQUE, &vu_other->l.magic);
		} else {
			if (rt_g.NMG_debug & DEBUG_POLYSECT)
				rt_log("Making new vertex\n");

/*			(void)nmg_esplit((struct vertex *)NULL, eu->e_p); */
			(void)nmg_ebreak((struct vertex *)NULL, eu->e_p);

			/* given the trouble that nmg_ebreak (nmg_esplit)
			 * went to create, we're going to check this to the
			 * limit.
			 */
			NMG_CK_EDGEUSE(eu);
			NMG_CK_EDGEUSE(eu->eumate_p);

			/* since we just split eu, the "next" edgeuse
			 * from eu CAN'T (in a working [as opposed to broken]
			 * system) be the list head.
			 */
			euforw = RT_LIST_PNEXT(edgeuse, eu);

			NMG_CK_EDGEUSE(euforw);
			NMG_CK_EDGEUSE(euforw->eumate_p);

			NMG_CK_VERTEXUSE(eu->vu_p);
			NMG_CK_VERTEXUSE(eu->eumate_p->vu_p);
			NMG_CK_VERTEXUSE(euforw->vu_p);
			NMG_CK_VERTEXUSE(euforw->eumate_p->vu_p);

			NMG_CK_VERTEX(eu->vu_p->v_p);
			NMG_CK_VERTEX(eu->eumate_p->vu_p->v_p);
			NMG_CK_VERTEX(euforw->vu_p->v_p);
			NMG_CK_VERTEX(euforw->eumate_p->vu_p->v_p);

			nmg_ck_lueu(eu->up.lu_p, "isect_edge_face" );

			/* check to make sure we know the right place to
			 * stick the geometry
			 */
			if (eu->eumate_p->vu_p->v_p->vg_p != 
			    (struct vertex_g *)NULL) {
				VPRINT("where'd this geometry come from?",
					eu->eumate_p->vu_p->v_p->vg_p->coord);
			}
			nmg_vertex_gv(eu->eumate_p->vu_p->v_p, hit_pt);

			NMG_CK_VERTEX_G(eu->vu_p->v_p->vg_p);
			NMG_CK_VERTEX_G(eu->eumate_p->vu_p->v_p->vg_p);
			NMG_CK_VERTEX_G(euforw->vu_p->v_p->vg_p);
			NMG_CK_VERTEX_G(euforw->eumate_p->vu_p->v_p->vg_p);
						
			if (euforw->vu_p->v_p != eu->eumate_p->vu_p->v_p)
				rt_bomb("I was supposed to share verticies!\n");

			if (rt_g.NMG_debug & DEBUG_POLYSECT) {
				register pointp_t	p1, p2;
				p1 = eu->vu_p->v_p->vg_p->coord;
				p2 = eu->eumate_p->vu_p->v_p->vg_p->coord;
				rt_log("Just split %g, %g, %g -> %g, %g, %g\n",
					V3ARGS(p1), V3ARGS(p2) );
				p1 = euforw->vu_p->v_p->vg_p->coord;
				p2 = euforw->eumate_p->vu_p->v_p->vg_p->coord;
				rt_log("\t\t\t%g, %g, %g -> %g, %g, %g\n",
					V3ARGS(p1), V3ARGS(p2) );
			}
			/* stick this vertex in the other shell
			 * and make sure it is in the other shell's
			 * list of vertices on the instersect line
			 */
			plu = nmg_mlv(&fu->l.magic,
				eu->eumate_p->vu_p->v_p, OT_SAME);
			vu_other = RT_LIST_FIRST( vertexuse, &plu->down_hd );
			NMG_CK_VERTEXUSE(vu_other);

			if (rt_g.NMG_debug & DEBUG_POLYSECT) {
			    	VPRINT("Making vertexloop",
					vu_other->v_p->vg_p->coord);
				if (RT_LIST_FIRST_MAGIC(&plu->down_hd) !=
					NMG_VERTEXUSE_MAGIC)
					rt_bomb("bad plu\n");
				if (RT_LIST_FIRST_MAGIC(&plu->lumate_p->down_hd) !=
					NMG_VERTEXUSE_MAGIC)
					rt_bomb("bad plumate\n");

			}
			(void)nmg_tbl(bs->l2, TBL_INS_UNIQUE, &vu_other->l.magic);
		}

		euforw = RT_LIST_PNEXT_CIRC(edgeuse, eu);
		if (rt_g.NMG_debug & DEBUG_POLYSECT) {
			register pointp_t	p1, p2;
			p1 = eu->vu_p->v_p->vg_p->coord;
			p2 = eu->eumate_p->vu_p->v_p->vg_p->coord;
			rt_log("\tNow %g, %g, %g <-> %g, %g, %g\n",
				V3ARGS(p1), V3ARGS(p2) );
			p1 = euforw->vu_p->v_p->vg_p->coord;
			p2 = euforw->eumate_p->vu_p->v_p->vg_p->coord;
			rt_log("\tand %g, %g, %g <-> %g, %g, %g\n\n",
				V3ARGS(p1), V3ARGS(p2) );
		}
		(void)nmg_tbl(bs->l1, TBL_INS_UNIQUE, &euforw->vu_p->l.magic);
		return;
	}
	if ( dist_to_plane < dist2 + bs->tol.dist) {
		/* Second point is on plane of face, by geometry */
		if (rt_g.NMG_debug & DEBUG_POLYSECT)
			rt_log("\tedge ends at plane intersect\n");

		if( RT_LIST_PNEXT_CIRC(edgeuse,eu)->vu_p->v_p != v1mate )
			rt_bomb("isect_edge_face: discontinuous eu loop\n");

#if 0
		/* Adding these guys causes bool.g Test7.r to die */
		(void)nmg_tbl(bs->l1, TBL_INS_UNIQUE, &eu->vu_p->l.magic);
#endif

		vu_other = nmg_find_vu_in_face(v1mate->vg_p->coord, fu, &(bs->tol));
		if (vu_other) {
			register pointp_t	p3;
			/* Face has a very similar vertex.  Add to list */
#if 0
			(void)nmg_tbl(bs->l2, TBL_INS_UNIQUE, &vu_other->l.magic);
#endif
			/* Make new coordinates be the midpoint */
			p3 = vu_other->v_p->vg_p->coord;
			VADD2SCALE(v1mate->vg_p->coord, v1mate->vg_p->coord, p3, 0.5);
			/* Combine the two vertices */
			nmg_jv(v1mate, vu_other->v_p);
		} else {
			/* Insert copy of this vertex into face */
			if (rt_g.NMG_debug & DEBUG_POLYSECT)
			    	VPRINT("Making vertexloop",
			    		v1mate->vg_p->coord);

			plu = nmg_mlv(&fu->l.magic, v1mate, OT_UNSPEC);
			vu_other = RT_LIST_FIRST( vertexuse, &plu->down_hd );
			NMG_CK_VERTEXUSE(vu_other);
#if 0
			(void)nmg_tbl(bs->l2, TBL_INS_UNIQUE, &vu_other->l.magic);
#endif
		}
		return;
	}

	if (rt_g.NMG_debug & DEBUG_POLYSECT)
		rt_log("\tdist to plane X: X > MAGNITUDE(edge)\n");


}
/*	I S E C T _ L O O P _ F A C E
 *
 *	Intersect a single loop with another face
 */
static void isect_loop_face(bs, lu, fu)
struct nmg_inter_struct *bs;
struct loopuse *lu;
struct faceuse *fu;
{
	struct edgeuse	*eu;
	long		magic1;

	if (rt_g.NMG_debug & DEBUG_POLYSECT)
		rt_log("isect_loop_faces\n");

	NMG_CK_LOOPUSE(lu);
	NMG_CK_FACEUSE(fu);

	/* loop overlaps intersection face? */
	magic1 = RT_LIST_FIRST_MAGIC( &lu->down_hd );
	if (magic1 == NMG_VERTEXUSE_MAGIC) {
		/* this is most likely a loop inserted when we split
		 * up fu2 wrt fu1 (we're now splitting fu1 wrt fu2)
		 */
		isect_vertex_face(bs,
			RT_LIST_FIRST(vertexuse,&lu->down_hd), fu);
	} else if (magic1 == NMG_EDGEUSE_MAGIC) {
		/*
		 *  Process a loop consisting of a list of edgeuses.
		 *
		 * By going backwards around the list we avoid
		 * re-processing an edgeuse that was just created
		 * by isect_edge_face.  This is because the edgeuses
		 * point in the "next" direction, and when one of
		 * them is split, it inserts a new edge AHEAD or
		 * "nextward" of the current edgeuse.
		 */ 
		for( eu = RT_LIST_LAST(edgeuse, &lu->down_hd );
		     RT_LIST_NOT_HEAD(eu,&lu->down_hd);
		     eu = RT_LIST_PLAST(edgeuse,eu) )  {
			NMG_CK_EDGEUSE(eu);

			if (eu->up.magic_p != &lu->l.magic) {
				rt_bomb("edge does not share loop\n");
			}

			isect_edge_face(bs, eu, fu);
			nmg_ck_lueu(lu, "isect_loop_face");
		 }
	} else {
		rt_bomb("isect_loop_face() Unknown type of NMG loopuse\n");
	}
}

/*
 *			N M G _ I S E C T _ 2 F A C E _ L O O P S
 *
 *	Intersect loops of face 1 with the entirety of face 2
 */
static void nmg_isect_2face_loops(bs, fu1, fu2)
struct nmg_inter_struct *bs;
struct faceuse	*fu1;
struct faceuse	*fu2;
{
	struct loopuse	*lu;

	NMG_CK_FACE_G(fu2->f_p->fg_p);
	NMG_CK_FACE_G(fu1->f_p->fg_p);

	/* process each loop in face 1 */
	for( RT_LIST_FOR( lu, loopuse, &fu1->lu_hd ) )  {

		if (rt_g.NMG_debug & DEBUG_POLYSECT)
			rt_log("\nLoop %8x\n", lu);

		if (lu->up.fu_p != fu1) {
			rt_bomb("nmg_isect_2face_loops() Child loop doesn't share parent!\n");
		}

		isect_loop_face(bs, lu, fu2);
	}
}

/*
 *			N M G _ I S E C T _ 2 F A C E S
 *
 *	Intersect a pair of faces
 */
static void nmg_isect_2faces(fu1, fu2, tol)
struct faceuse		*fu1, *fu2;
CONST struct rt_tol	*tol;
{
	struct nmg_ptbl vert_list1, vert_list2;
	struct nmg_inter_struct	bs;
	int		i;
	fastf_t		*pl1, *pl2;
	struct face	*f1;
	struct face	*f2;
	point_t		min_pt;
	union {
		struct vertexuse **vu;
		long **magic_p;
	} p;
	int		status;

	NMG_CK_FACEUSE(fu1);
	f1 = fu1->f_p;
	NMG_CK_FACE(f1);
	NMG_CK_FACE_G(f1->fg_p);

	NMG_CK_FACEUSE(fu2);
	f2 = fu2->f_p;
	NMG_CK_FACE(f2);
	NMG_CK_FACE_G(f2->fg_p);

	if (rt_g.NMG_debug & DEBUG_POLYSECT) {
		pl1 = f1->fg_p->N;
		pl2 = f2->fg_p->N;

		rt_log("\nPlanes\t%gx + %gy + %gz = %g\n\t%gx + %gy + %gz = %g\n",
			pl1[0], pl1[1], pl1[2], pl1[3],
			pl2[0], pl2[1], pl2[2], pl2[3]);
	}

	if ( !NMG_EXTENT_OVERLAP(f2->fg_p->min_pt, f2->fg_p->max_pt,
	    f1->fg_p->min_pt, f1->fg_p->max_pt) )  return;

	/* Extents of face1 overlap face2 */
	VMOVE(min_pt, f1->fg_p->min_pt);
	VMIN(min_pt, f2->fg_p->min_pt);
	status = rt_isect_2planes( bs.pt, bs.dir, f1->fg_p->N, f2->fg_p->N,
		min_pt, tol );
	switch( status )  {
	case 0:
		/* All is well */
		bs.coplanar = 0;
		break;
	case -1:
		/* co-planar */
		rt_log("co-planar faces?\n");
		bs.coplanar = 1;
		return;
	case -2:
		/* parallel and distinct, no intersection */
		return;
	default:
		/* internal error */
		rt_log("ERROR nmg_isect_2faces() unable to find plane intersection\n");
		return;
	}

	(void)nmg_tbl(&vert_list1, TBL_INIT,(long *)NULL);
	(void)nmg_tbl(&vert_list2, TBL_INIT,(long *)NULL);

    	bs.l1 = &vert_list1;
    	bs.l2 = &vert_list2;
    	bs.tol = *tol;		/* struct copy */

    	if (rt_g.NMG_debug & (DEBUG_POLYSECT|DEBUG_COMBINE|DEBUG_MESH)
    	    && rt_g.NMG_debug & DEBUG_PLOTEM) {
    		static int fno=1;
    	    	nmg_pl_2fu( "Isect_faces%d.pl", fno++, fu1, fu2, 0 );
    	}

	nmg_isect_2face_loops(&bs, fu1, fu2);

    	if (rt_g.NMG_debug & DEBUG_COMBINE) {
	    	p.magic_p = vert_list1.buffer;
	    	rt_log("vert_list1\n");
		for (i=0 ; i < vert_list1.end ; ++i) {
			rt_log("%d\t%g, %g, %g\t", i,
				p.vu[i]->v_p->vg_p->coord[X],
				p.vu[i]->v_p->vg_p->coord[Y],
				p.vu[i]->v_p->vg_p->coord[Z]);
			if (*p.vu[i]->up.magic_p == NMG_EDGEUSE_MAGIC) {
				rt_log("EDGEUSE\n");
			} else if (*p.vu[i]->up.magic_p == NMG_LOOPUSE_MAGIC){
				rt_log("LOOPUSE\n");
	if ((struct vertexuse *)p.vu[i]->up.lu_p->down_hd.forw != p.vu[i]) {
		rt_log("vertexuse's parent disowns us!\n");
		if (((struct vertexuse *)(p.vu[i]->up.lu_p->lumate_p->down_hd.forw))->l.magic == NMG_VERTEXUSE_MAGIC)
			rt_bomb("lumate has vertexuse\n");
		else
			rt_bomb("lumate has garbage\n");
	}
			} else
				rt_log("UNKNOWN\n");
		}
    	}

    	bs.l2 = &vert_list1;
    	bs.l1 = &vert_list2;
	nmg_isect_2face_loops(&bs, fu2, fu1);

    	if (vert_list1.end == 0) {
    		/* there were no intersections */
		(void)nmg_tbl(&vert_list1, TBL_FREE, (long *)NULL);
		(void)nmg_tbl(&vert_list2, TBL_FREE, (long *)NULL);
    		return;
    	}

	nmg_face_combine(&vert_list1, fu1, fu2, bs.pt, bs.dir);
	nmg_face_combine(&vert_list2, fu2, fu1, bs.pt, bs.dir);

	/* When two faces are intersected
	 * with each other, they should
	 * share the same edge(s) of
	 * intersection. 
	 */
    	if (rt_g.NMG_debug & DEBUG_MESH &&
    	    rt_g.NMG_debug & DEBUG_PLOTEM) {
    		static int fnum=1;
    	    	nmg_pl_2fu( "Before_mesh%d.pl", fnum++, fu1, fu2, 1 );
    	}

	nmg_mesh_faces(fu1, fu2);

    	if (rt_g.NMG_debug & DEBUG_MESH &&
    	    rt_g.NMG_debug & DEBUG_PLOTEM) {
    		static int fno=1;
    	    	nmg_pl_2fu( "After_mesh%d.pl", fno++, fu1, fu2, 1 );
    	}


	(void)nmg_tbl(&vert_list1, TBL_FREE, (long *)NULL);
	(void)nmg_tbl(&vert_list2, TBL_FREE, (long *)NULL);
}

/*
 *			N M G _ C R A C K S H E L L S
 *
 *	Split the components of two shells wherever they may intersect,
 *	in preparation for performing boolean operations on the shells.
 */
void
nmg_crackshells(s1, s2, tol)
struct shell		*s1;
struct shell		*s2;
CONST struct rt_tol	*tol;
{
	struct face	*f1;
	struct faceuse	*fu1, *fu2;
	struct shell_a	*sa1, *sa2;
	long		*flags;

	NMG_CK_SHELL(s1);
	sa1 = s1->sa_p;
	NMG_CK_SHELL_A(sa1);

	NMG_CK_SHELL(s2);
	sa2 = s2->sa_p;
	NMG_CK_SHELL_A(sa2);

	/* XXX this isn't true for non-manifold geometry! */
	if( RT_LIST_IS_EMPTY( &s1->fu_hd ) ||
	    RT_LIST_IS_EMPTY( &s2->fu_hd ) )  {
		rt_log("ERROR:shells must contain faces for boolean operations.");
		return;
	}

	/* See if shells overlap */
	if ( ! NMG_EXTENT_OVERLAP(sa1->min_pt, sa1->max_pt,
	    sa2->min_pt, sa2->max_pt) )
		return;

	flags = (long *)rt_calloc( s1->r_p->m_p->maxindex, sizeof(long),
		"nmg_crackshells flags[]" );

	/*
	 *  Check each of the faces in shell 1 to see
	 *  if they overlap the extent of shell 2
	 */
	for( RT_LIST_FOR( fu1, faceuse, &s1->fu_hd ) )  {
		NMG_CK_FACEUSE(fu1);
		f1 = fu1->f_p;
		NMG_CK_FACE(f1);

		if( NMG_INDEX_IS_SET(flags, f1) )  continue;
		NMG_CK_FACE_G(f1->fg_p);

		/* See if face f1 overlaps shell2 */
		if( ! NMG_EXTENT_OVERLAP(sa2->min_pt, sa2->max_pt,
		    f1->fg_p->min_pt, f1->fg_p->max_pt) )
			continue;

		/*
		 *  Now, check the face f1 from shell 1
		 *  against each of the faces of shell 2
		 */
	    	for( RT_LIST_FOR( fu2, faceuse, &s2->fu_hd ) )  {
	    		NMG_CK_FACEUSE(fu2);
	    		NMG_CK_FACE(fu2->f_p);

	    		/* See if face f1 overlaps face 2 */
			if( ! NMG_EXTENT_OVERLAP(
			    fu2->f_p->fg_p->min_pt, fu2->f_p->fg_p->max_pt,
			    f1->fg_p->min_pt, f1->fg_p->max_pt) )
				continue;

			nmg_isect_2faces(fu1, fu2, tol);

			/* try not to process redundant faceuses (mates) */
			if( RT_LIST_NOT_HEAD( fu2, &s2->fu_hd ) )  {
				register struct faceuse	*nextfu;
				nextfu = RT_LIST_PNEXT(faceuse, fu2 );
				if( nextfu->f_p == fu2->f_p )
					fu2 = nextfu;
			}
	    	}
	    	NMG_INDEX_SET(flags, f1);
	}
	rt_free( (char *)flags, "nmg_crackshells flags[]" );
}
