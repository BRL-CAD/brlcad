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
	fastf_t		tol;
	point_t		pt;		/* line of intersection */
	vect_t		dir;
	int		coplanar;
};



/*
 *	T B L _ V S O R T
 *	sort list of vertices in fu1 on plane of fu2 
 *
 *	W A R N I N G:
 *		This function makes gross assumptions about the contents
 *	and structure of an nmg_ptbl list!  If I could figure a way of folding
 *	this into the nmg_tbl routine, I would do it.
 */
static void tbl_vsort(b, fu1, fu2, pt)
struct nmg_ptbl *b;		/* table of vertexuses on intercept line */
struct faceuse	*fu1, *fu2;
point_t		pt;
{
	point_t		min_pt;
	vect_t		vect;
	union {
		struct vertexuse **vu;
		long **magic_p;
	} p;
	struct vertexuse *tvu;
	fastf_t *mag, tmag;
	int i, j;

	mag = (fastf_t *)rt_calloc(b->end, sizeof(fastf_t),
					"vector magnitudes for sort");

	p.magic_p = b->buffer;
	/* check vertexuses and compute distance from start of line */
	for(i = 0 ; i < b->end ; ++i) {
		NMG_CK_VERTEXUSE(p.vu[i]);

		VSUB2(vect, pt, p.vu[i]->v_p->vg_p->coord);
		mag[i] = MAGNITUDE(vect);
	}

	/* a trashy bubble-head sort, because I hope this list is never
	 * very long.
	 */
	for(i=0 ; i < b->end - 1 ; ++i) {
		for (j=i+1; j < b->end ; ++j) {
			if (mag[i] > mag[j]) {
				tvu = p.vu[i];
				p.vu[i] = p.vu[j];
				p.vu[j] = tvu;

				tmag = mag[i];
				mag[i] = mag[j];
				mag[j] = tmag;
			}
		}
	}
	/*
	 * We should do something here to "properly"
	 * order vertexuses which share a vertex
	 * or whose coordinates are equal.
	 *
	 * Just what should be done & how is not
	 * clear to me at this hour of the night.
	 * for (i=0 ; i < b->end - 1 ; ++i) {
	 *	if (p.vu[i]->v_p == p.vu[i+1]->v_p ||
	 *	    VAPPROXEQUAL(p.vu[i]->v_p->vg_p->coord,
	 *	    p.vu[i+1]->v_p->vg_p->coord, VDIVIDE_TOL) ) {
	 *	}
	 * }
	 */
	rt_free((char *)mag, "vector magnitudes");
}


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

	for( NMG_LIST( vu, vertexuse, &v->vu_hd ) )  {
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

	if (NEAR_ZERO(dist, bs->tol) &&
	    nmg_tbl(bs->l1, TBL_INS_UNIQUE, &vu->l.magic) < 0) {
		if (rt_g.NMG_debug & DEBUG_POLYSECT)
		    	VPRINT("Making vertexloop", vu->v_p->vg_p->coord);

		plu = nmg_mlv(&fu->l.magic, vu->v_p, OT_UNSPEC);
	    	/* XXX should this be TBL_INS_UNIQUE? */
	    	(void)nmg_tbl(bs->l2, TBL_INS,
			&NMG_LIST_FIRST_MAGIC(&plu->down_hd) );
	}
}

/*	I S E C T _ E D G E _ F A C E
 *
 *	Intersect an edge with a face
 */
static void isect_edge_face(bs, eu, fu)
struct nmg_inter_struct *bs;
struct edgeuse *eu;
struct faceuse *fu;
{
	struct vertexuse *vu;
	point_t		hit_pt;
	vect_t		edge_vect;
	fastf_t		edge_len;	/* MAGNITUDE(edge_vect) */
	pointp_t	p1, p2;
	fastf_t		dist;		/* parametric dist to hit point */
	fastf_t		dist_to_plane;	/* distance to hit point, in mm */
	int		status;
	struct loopuse	*plu;

	NMG_CK_EDGEUSE(eu);
	NMG_CK_VERTEXUSE(eu->vu_p);
	NMG_CK_VERTEX(eu->vu_p->v_p);
	NMG_CK_VERTEX_G(eu->vu_p->v_p->vg_p);

	if (*eu->up.magic_p == NMG_LOOPUSE_MAGIC) {
		struct edgeuse	*eunext;
		struct edgeuse	*eulast;
		/* some edge sanity checking */
		eunext = NMG_LIST_PNEXT_CIRC(edgeuse, &eu->l);
		eulast = NMG_LIST_PLAST_CIRC(edgeuse, &eu->l);
		NMG_CK_EDGEUSE(eunext);
		NMG_CK_EDGEUSE(eulast);
		if (eu->vu_p->v_p != eulast->eumate_p->vu_p->v_p) {
			VPRINT("unshared vertex (mine): ",
				eu->vu_p->v_p->vg_p->coord);
			VPRINT("\t\t (last->eumate_p): ",
				eulast->eumate_p->vu_p->v_p->vg_p->coord);
			rt_bomb("isect_edge_face() discontiuous edgeloop\n");
		}
		if( eunext->vu_p->v_p != eu->eumate_p->vu_p->v_p) {
			VPRINT("unshared vertex (my mate): ",
				eu->eumate_p->vu_p->v_p->vg_p->coord);
			VPRINT("\t\t (next): ",
				eunext->vu_p->v_p->vg_p->coord);
			rt_bomb("isect_edge_face() discontiuous edgeloop\n");
		}
	}

	/* We check to see if the edge crosses the plane of face fu.
	 * First we check the topology.  If the topology says that the start
	 * vertex of this edgeuse is on the other face, we enter the
	 * vertexuses in the list and it's all over.
	 *
	 * If the vertex on the other end of this edgeuse is on the face,
	 * then make a linkage to an existing face vertex (if found),
	 * and give up on this edge, knowing that we'll pick up the
	 * intersection of the next edgeuse with the face later.
	 *
	 * This all assumes that an edge can only intersect a face at one
	 * point.  This is probably a bad assumption for the future
	 */
	if (vu=vertex_on_face(eu->vu_p->v_p, fu)) {

		if (rt_g.NMG_debug & DEBUG_POLYSECT) {
			p1 = eu->vu_p->v_p->vg_p->coord;
			NMG_CK_EDGEUSE(eu->eumate_p);
			NMG_CK_VERTEXUSE(eu->eumate_p->vu_p);
			NMG_CK_VERTEX(eu->eumate_p->vu_p->v_p);
			NMG_CK_VERTEX_G(eu->eumate_p->vu_p->v_p->vg_p);
			p2 = eu->eumate_p->vu_p->v_p->vg_p->coord;
			rt_log("Edgeuse %g, %g, %g -> %g, %g, %g\n",
				p1[0], p1[1], p1[2], p2[0], p2[1], p2[2]);
			rt_log("\tvertex topologically on intersection plane\n");
		}

		(void)nmg_tbl(bs->l1, TBL_INS_UNIQUE, &eu->vu_p->l.magic);
		(void)nmg_tbl(bs->l2, TBL_INS_UNIQUE, &vu->l.magic);
		return;
	} else if (vu=vertex_on_face(eu->eumate_p->vu_p->v_p, fu)) {

		if (rt_g.NMG_debug & DEBUG_POLYSECT) {
			p1 = eu->vu_p->v_p->vg_p->coord;
			NMG_CK_EDGEUSE(eu->eumate_p);
			NMG_CK_VERTEXUSE(eu->eumate_p->vu_p);
			NMG_CK_VERTEX(eu->eumate_p->vu_p->v_p);
			NMG_CK_VERTEX_G(eu->eumate_p->vu_p->v_p->vg_p);
			p2 = eu->eumate_p->vu_p->v_p->vg_p->coord;
			rt_log("Edgeuse %g, %g, %g -> %g, %g, %g\n",
				p1[0], p1[1], p1[2], p2[0], p2[1], p2[2]);
			rt_log("\tMATE vertex topologically on intersection plane. skipping edgeuse\n");
		}
		return;
	}

	/*
	 *  Neither vertex lies on the face according to topology.
	 *  Form a ray that starts at one vertex of the edgeuse
	 *  and points to the other vertex.
	 */
	p1 = eu->vu_p->v_p->vg_p->coord;
	p2 = eu->eumate_p->vu_p->v_p->vg_p->coord;
	VSUB2(edge_vect, p2, p1);

	if (rt_g.NMG_debug & DEBUG_POLYSECT)
		rt_log("Testing %g, %g, %g -> %g, %g, %g\n",
			p1[X], p1[Y], p1[Z], p2[X], p2[Y], p2[Z]);


	status = rt_isect_ray_plane(&dist, p1, edge_vect, fu->f_p->fg_p->N);

	if (rt_g.NMG_debug & DEBUG_POLYSECT) {
		if (status >= 0)
			rt_log("\tStatus of rt_isect_ray_plane: %d dist: %g\n",
				status, dist);
		else
			rt_log("\tBoring status of rt_isect_ray_plane: %d dist: %g\n",
				status, dist);
	}

	/* if this edge doesn't intersect the other face by geometry,
	 * we're done.
	 */
	if (status < 0)
		return;

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
	VJOIN1( hit_pt, p1, dist, edge_vect );
	edge_len = MAGNITUDE(edge_vect);
	dist_to_plane = edge_len * dist;

	if (rt_g.NMG_debug & DEBUG_POLYSECT)
		rt_log("\tedge_len=%g, dist=%g, dist_to_plane=%g\n",
			edge_len, dist, dist_to_plane);

	if ( dist_to_plane < -(bs->tol) )  {
		/* Hit is behind first point */
		if (rt_g.NMG_debug & DEBUG_POLYSECT)
			rt_log("\tplane behind first point\n");
		return;
	}

	if ( dist_to_plane < bs->tol )  {
		/* First point is on plane of face, by geometry */
		if (rt_g.NMG_debug & DEBUG_POLYSECT)
			rt_log("\tedge starts at plane intersect\n");

		/* Add to list of verts on intersection line */
		(void)nmg_tbl(bs->l1, TBL_INS_UNIQUE, &eu->vu_p->l.magic);

		/* If face doesn't have a "very similar" point, give it one */
		vu = nmg_find_vu_in_face(p1, fu, bs->tol);
		if (vu) {
			register pointp_t	p3;
			/* Face has a vertex very similar to this one.
			 * Add vertex to face's list of vertices on
			 * intersection line.
			 */
			(void)nmg_tbl(bs->l2, TBL_INS_UNIQUE, &vu->l.magic);

			/* new coordinates are the midpoint between
			 * the two existing coordinates
			 */
			p3 = vu->v_p->vg_p->coord;
			VADD2SCALE(p1, p1, p3, 0.5);
			/* Combine the two vertices */
			nmg_jv(eu->vu_p->v_p, vu->v_p);
		} else {
			/* Since the other face doesn't have a vertex quite
			 * like this one, we make a copy of this one and make.
			 * sure it's in the other face's list of intersect
			 * verticies.
			 */

			if (rt_g.NMG_debug & DEBUG_POLYSECT)
			    	VPRINT("Making vertexloop",
			    		eu->vu_p->v_p->vg_p->coord);

			plu = nmg_mlv(&fu->l.magic, eu->vu_p->v_p, OT_UNSPEC);

			/* make sure this vertex is in other face's list of
			 * points to deal with
			 */
			/* Should this be TBL_INS_UNIQUE? */
			(void)nmg_tbl(bs->l2, TBL_INS,
				&NMG_LIST_FIRST_MAGIC(&plu->down_hd) );
		}
		return;
	}
	if ( dist_to_plane < edge_len - bs->tol) {
		struct edgeuse	*euforw;

		/* Intersection is between first and second vertex points.
		 * Insert new vertex at intersection point.
		 */
		if (rt_g.NMG_debug & DEBUG_POLYSECT)  {
			rt_log("Splitting %g, %g, %g <-> %g, %g, %g\n",
			p1[X], p1[Y], p1[Z], p2[X], p2[Y], p2[Z]);
			VPRINT("\tPoint of intersection", hit_pt);
		}

		/* if we can't find the appropriate vertex in the
		 * other face, we'll build a new vertex.  Otherwise
		 * we re-use an old one.
		 */
		vu = nmg_find_vu_in_face(hit_pt, fu, bs->tol);
		if (vu) {
			/* the other face has a convenient vertex for us */

			if (rt_g.NMG_debug & DEBUG_POLYSECT)
				rt_log("re-using vertex from other face\n");

			(void)nmg_esplit(vu->v_p, eu->e_p);
			(void)nmg_tbl(bs->l2, TBL_INS_UNIQUE, &vu->l.magic);
		} else {
			if (rt_g.NMG_debug & DEBUG_POLYSECT)
				rt_log("Making new vertex\n");

			(void)nmg_esplit((struct vertex *)NULL, eu->e_p);

			/* given the trouble that nmg_esplit was to create,
			 * we're going to check this to the limit
			 */
			NMG_CK_EDGEUSE(eu);
			NMG_CK_EDGEUSE(eu->eumate_p);

			/* since we just split eu, the "next" edgeuse
			 * from eu CAN'T (in a working [as opposed to broken]
			 * system) be the list head.  
			 */
			euforw = NMG_LIST_PNEXT(edgeuse, eu);

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

			if (rt_g.NMG_debug & DEBUG_POLYSECT) {
			    	VPRINT("Making vertexloop",
				NMG_LIST_PNEXT(vertexuse,&plu->down_hd)->
					v_p->vg_p->coord);
				if (NMG_LIST_FIRST_MAGIC(&plu->down_hd) !=
					NMG_VERTEXUSE_MAGIC)
					rt_bomb("bad plu\n");
				if (NMG_LIST_FIRST_MAGIC(&plu->lumate_p->down_hd) !=
					NMG_VERTEXUSE_MAGIC)
					rt_bomb("bad plumate\n");

			}
			(void)nmg_tbl(bs->l2, TBL_INS,
				&NMG_LIST_FIRST_MAGIC(&plu->down_hd) );
		}

		euforw = NMG_LIST_PNEXT_CIRC(edgeuse, eu);
		if (rt_g.NMG_debug & DEBUG_POLYSECT) {
			p1 = eu->vu_p->v_p->vg_p->coord;
			p2 = eu->eumate_p->vu_p->v_p->vg_p->coord;
			rt_log("\tNow %g, %g, %g <-> %g, %g, %g\n",
				p1[X], p1[Y], p1[Z], p2[X], p2[Y], p2[Z]);
			p1 = euforw->vu_p->v_p->vg_p->coord;
			p2 = euforw->eumate_p->vu_p->v_p->vg_p->coord;
			rt_log("\tand %g, %g, %g <-> %g, %g, %g\n\n",
				p1[X], p1[Y], p1[Z], p2[X], p2[Y], p2[Z]);
		}

		(void)nmg_tbl(bs->l1, TBL_INS, &euforw->vu_p->l.magic);
		return;
	}
	if ( dist_to_plane < edge_len + bs->tol) {
		/* Second point is on plane of face, by geometry */
		/* Make no entries in intersection lists,
		 * because it will be handled on the next call.
		 */
		if (rt_g.NMG_debug & DEBUG_POLYSECT)
			rt_log("\tedge ends at plane intersect\n");

		if( NMG_LIST_PNEXT_CIRC(edgeuse,eu)->vu_p->v_p !=
		    eu->eumate_p->vu_p->v_p )
			rt_bomb("isect_edge_face: discontinuous eu loop\n");

		/* If face has a "very similar" point, connect up with it */
		vu = nmg_find_vu_in_face(p2, fu, bs->tol);
		if (vu) {
			register pointp_t	p3;
			/* new coordinates are the midpoint between
			 * the two existing coordinates
			 */
			p3 = vu->v_p->vg_p->coord;
			VADD2SCALE(p2, p2, p3, 0.5);
			/* Combine the two vertices */
			nmg_jv(eu->eumate_p->vu_p->v_p, vu->v_p);
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
	magic1 = NMG_LIST_FIRST_MAGIC( &lu->down_hd );
	if (magic1 == NMG_VERTEXUSE_MAGIC) {
		/* this is most likely a loop inserted when we split
		 * up fu2 wrt fu1 (we're now splitting fu1 wrt fu2)
		 */
		isect_vertex_face(bs,
			NMG_LIST_FIRST(vertexuse,&lu->down_hd), fu);
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
		for( eu = NMG_LIST_LAST(edgeuse, &lu->down_hd );
		     NMG_LIST_MORE(eu,edgeuse,&lu->down_hd);
		     eu = NMG_LIST_PLAST(edgeuse,eu) )  {
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
	for( NMG_LIST( lu, loopuse, &fu1->lu_hd ) )  {

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
struct faceuse *fu1, *fu2;
fastf_t tol;
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
	extern void nmg_face_combine();

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
	status = rt_isect_2planes( bs.pt, bs.dir, f1->fg_p->N, f2->fg_p->N, min_pt );
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
    	bs.tol = tol/50.0;

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

	tbl_vsort(&vert_list1, fu1, fu2, bs.pt);
	nmg_face_combine(&vert_list1, fu1, fu2);

	tbl_vsort(&vert_list2, fu2, fu1, bs.pt);
	nmg_face_combine(&vert_list2, fu2, fu1);

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
void nmg_crackshells(s1, s2, tol)
struct shell *s1, *s2;
fastf_t tol;
{
	struct faceuse *fu1, *fu2;
	struct nmg_ptbl faces;

	NMG_CK_SHELL(s2);
	NMG_CK_SHELL_A(s2->sa_p);
	NMG_CK_SHELL(s1);
	NMG_CK_SHELL_A(s1->sa_p);

	/* XXX this isn't true for non-manifold geometry! */
	if( NMG_LIST_IS_EMPTY( &s1->fu_hd ) ||
	    NMG_LIST_IS_EMPTY( &s2->fu_hd ) )  {
		rt_log("ERROR:shells must contain faces for boolean operations.");
		return;
	}

	if ( ! NMG_EXTENT_OVERLAP(s1->sa_p->min_pt, s1->sa_p->max_pt,
	    s2->sa_p->min_pt, s2->sa_p->max_pt) )
		return;

	(void)nmg_tbl(&faces, TBL_INIT, (long *)NULL);

	/* shells overlap */
	for( NMG_LIST( fu1, faceuse, &s1->fu_hd ) )  {
		/* check each of the faces in shell 1 to see
		 * if they overlap the extent of shell 2
		 */
		NMG_CK_FACEUSE(fu1);
		NMG_CK_FACE(fu1->f_p);
		NMG_CK_FACE_G(fu1->f_p->fg_p);

		if (nmg_tbl(&faces, TBL_LOC, &fu1->f_p->magic) < 0 &&
		    NMG_EXTENT_OVERLAP(s2->sa_p->min_pt,
		    s2->sa_p->max_pt, fu1->f_p->fg_p->min_pt,
		    fu1->f_p->fg_p->max_pt) ) {

			/* poly1 overlaps shell2 */
		    	for( NMG_LIST( fu2, faceuse, &s2->fu_hd ) )  {
		    		NMG_CK_FACEUSE(fu2);
		    		NMG_CK_FACE(fu2->f_p);
				/* now check the face of shell 1
				 * against each of the faces of shell 2
	 			 */
				nmg_isect_2faces(fu1, fu2, tol);

				/* try not to process redundant faceuses (mates) */
				if( NMG_LIST_MORE( fu2, faceuse, &s2->fu_hd ) )  {
					register struct faceuse	*nextfu;
					nextfu = NMG_LIST_PNEXT(faceuse, fu2 );
					if( nextfu->f_p == fu2->f_p )
						fu2 = nextfu;
				}
		    	}
			(void)nmg_tbl(&faces, TBL_INS, &fu1->f_p->magic);
		}

		/* try not to process redundant faceuses (mates) */
		if( NMG_LIST_MORE( fu1, faceuse, &s1->fu_hd ) )  {
			register struct faceuse	*nextfu;
			nextfu = NMG_LIST_PNEXT(faceuse, fu1 );
			if( nextfu->f_p == fu1->f_p )
				fu1 = nextfu;
		}
	}

	(void)nmg_tbl(&faces, TBL_FREE, (long *)NULL );
}
