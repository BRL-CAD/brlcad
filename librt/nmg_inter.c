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
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1993 by the United States Army.
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
	long		magic;
	struct nmg_ptbl	*l1;		/* vertexuses on the line of */
	struct nmg_ptbl *l2;		/* intersection between planes */
	struct rt_tol	tol;
	point_t		pt;		/* line of intersection */
	vect_t		dir;
	int		coplanar;
	fastf_t		*vert2d;	/* Array of 2d vertex projections [index] */
	mat_t		proj;		/* Matrix to project onto XY plane */
};
#define NMG_INTER_STRUCT_MAGIC	0x99912120
#define NMG_CK_INTER_STRUCT(_p)	NMG_CKMAG(_p, NMG_INTER_STRUCT_MAGIC, "nmg_inter_struct")

static struct nmg_visit_handlers nmg_isect2d_handlers;	/* null fill, to start */

/*
 */
void
nmg_isect2d_vis_vertex( magicp, state, flag )
long		*magicp;
genptr_t	state;
int		flag;
{
	struct vertex	*v = (struct vertex *)magicp;
	struct nmg_inter_struct	*is = (struct nmg_inter_struct *)state;
	fastf_t		*opt;
	point_t		pt;
	register fastf_t	*pt2d;

	NMG_CK_INTER_STRUCT(is);
	NMG_CK_VERTEX(v);

	if( !v->vg_p )  rt_bomb("nmg_isect2d_vis_vertex:  vertex with no geometry!\n");
	pt2d = &is->vert2d[v->index*3];
	if( pt2d[2] == 0 )  {
		/* Flag set.  Been here before */
		return;
	}

	opt = v->vg_p->coord;
	MAT4X3PNT( pt, is->proj, opt );
	pt2d[0] = pt[0];
	pt2d[1] = pt[1];
	pt2d[2] = 0;		/* flag */
rt_log("%d (%g,%g,%g) becomes (%g,%g) %g\n", v->index, V3ARGS(opt), V3ARGS(pt) );
}

/*
 *			N M G _ I S E C T 2 D _ P R E P
 *
 *  To intersect two co-planar faces, project all vertices from those
 *  faces into 2D.
 *  At the moment, use a memory intensive strategy which allocates a
 *  (3d) point_t for each "index" item, and subscripts the resulting
 *  array by the vertices index number.
 *  Since additional vertices can be created as the intersection process
 *  operates, 2*maxindex items are originall allocated, as a (generous)
 *  upper bound on the amount of intersecting that might happen.
 *
 *  In the array, the third double of each projected vertex is set to -1 when
 *  that slot has not been filled yet, and 0 when it has been.
 */
void
nmg_isect2d_prep( is, f1, f2 )
struct nmg_inter_struct	*is;
struct face		*f1;
struct face		*f2;
{
	struct model	*m;
	struct face_g	*fg;
	int		words;
	vect_t		to;
	point_t		centroid;
	point_t		centroid_proj;
	register int	i;

	NMG_CK_INTER_STRUCT(is);
	NMG_CK_FACE(f1);
	NMG_CK_FACE(f2);
	fg = f1->fg_p;
	NMG_CK_FACE_G(fg);

	m = nmg_find_model( &f1->magic );

	words = 3 * ( 2 * m->maxindex );
	is->vert2d = (fastf_t *)rt_malloc( words * sizeof(fastf_t), "vert2d[]");

	/*
	 *  Rotate so that f1's N vector points up +Z.
	 *  This places all 2D calcuations in the XY plane.
	 *  Translate so that f1's centroid becomes the 2D origin.
	 *  Reasoning:  no vertex should be favored by putting it at
	 *  the origin.  The "desirable" floating point space in the
	 *  vicinity of the origin should be used to best advantage,
	 *  by centering calculations around it.
	 */
	VSET( to, 0, 0, 1 );
HPRINT("fg->N", fg->N);
	mat_fromto( is->proj, fg->N, to );
	VADD2SCALE( centroid, fg->max_pt, fg->min_pt, 0.5 );
	MAT4X3PNT( centroid_proj, is->proj, centroid );
VPRINT("centroid", centroid);
VPRINT("centroid_proj", centroid_proj);
	centroid_proj[Z] = fg->N[3];	/* pull dist from origin off newZ */
	MAT_DELTAS_VEC_NEG( is->proj, centroid_proj );
mat_print("3d->2d xform matrix", is->proj);

	/* Clear out the 2D vertex array, setting flag in [2] to -1 */
	for( i = words-1-2; i >= 0; i -= 3 )  {
		VSET( &is->vert2d[i], 0, 0, -1 );
	}

	/* Project all the vertices in both faces */
	nmg_isect2d_handlers.vis_vertex = nmg_isect2d_vis_vertex;
rt_log("projecting face1:\n");
	nmg_visit( f1->fu_p, &nmg_isect2d_handlers, (genptr_t)is );
rt_log("projecting face2:\n");
	nmg_visit( f2->fu_p, &nmg_isect2d_handlers, (genptr_t)is );
}

/*
 *			N M G _ F I N D _ 3 V E R T E X _ O N _ 3 F A C E
 *
 *	Perform a topological check to
 *	determine if the given vertex can be found in the given faceuse.
 *	If it can, return a pointer to the vertexuse which was found in the
 *	faceuse.
 */
static struct vertexuse *
nmg_find_3vertex_on_3face(v, fu)
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

static void
nmg_isect_2vertex_2face(bs, vu, fu)
struct nmg_inter_struct *bs;
struct vertexuse *vu;
struct faceuse *fu;
{
	rt_bomb("2vertex_2face\n");
}

/*
 *			N M G _ I S E C T _ 3 V E R T E X _ 3 F A C E
 *
 *	intersect a vertex with a face (primarily for intersecting
 *	loops of a single vertex with a face).
 */
static void
nmg_isect_3vertex_3face(bs, vu, fu)
struct nmg_inter_struct *bs;
struct vertexuse *vu;
struct faceuse *fu;
{
	struct loopuse *plu;
	struct vertexuse *vup;
	pointp_t pt;
	fastf_t dist;

	if (rt_g.NMG_debug & DEBUG_POLYSECT)
		rt_log("nmg_isect_3vertex_3face(, vu=x%x, fu=x%x)\n", vu, fu);

	/* check the topology first */	
	if (vup=nmg_find_3vertex_on_3face(vu->v_p, fu)) {
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
		nmg_loop_g(plu->l_p);
	    	(void)nmg_tbl(bs->l2, TBL_INS_UNIQUE,
			&RT_LIST_FIRST_MAGIC(&plu->down_hd) );
	}
}

/*	M E G A _ C H E C K _ E B R E A K _ R E S U L T
 *
 *	Given all the trouble that nmg_ebreak() and nmg_esplit() were
 *	to develop, this routine exists to assist isect_edge_face() in
 *	checking the results of a call to these routines.
 */
static struct edgeuse *
mega_check_ebreak_result(eu)
struct edgeuse *eu;
{
	struct edgeuse *euforw;

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

	if (euforw->vu_p->v_p != eu->eumate_p->vu_p->v_p)
		rt_bomb("I was supposed to share verticies!\n");

	/* Make sure there is no geometry at the place we're about
	 * to stick the new geometry
	 */
	if (eu->eumate_p->vu_p->v_p->vg_p != (struct vertex_g *)NULL) {
		VPRINT("where'd this geometry come from?",
			eu->eumate_p->vu_p->v_p->vg_p->coord);
		rt_bomb("I didn't order this\n");
	}

	return(euforw);
}

/*
 *			N M G _ B R E A K _ 3 E D G E _ A T _ P L A N E
 *
 *	Having decided that an edge(use) crosses a plane of intersection,
 *	stick a vertex at the point of intersection along the edge.
 */
static void
nmg_break_3edge_at_plane(hit_pt, fu, bs, eu, v1, v1mate)
point_t	hit_pt;
struct faceuse *fu;
struct nmg_inter_struct *bs;
struct edgeuse *eu;
struct vertex	*v1;
struct vertex	*v1mate;
{
	struct vertexuse *vu_other;
	struct loopuse	*plu;
	struct edgeuse *euforw;

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

		(void)nmg_ebreak(vu_other->v_p, eu->e_p);
		(void)nmg_tbl(bs->l2, TBL_INS_UNIQUE, &vu_other->l.magic);
	} else {
		if (rt_g.NMG_debug & DEBUG_POLYSECT)
			rt_log("Making new vertex\n");

		(void)nmg_ebreak((struct vertex *)NULL, eu->e_p);

		(void)mega_check_ebreak_result(eu);
		euforw = RT_LIST_PNEXT(edgeuse, eu);

		if (euforw->vu_p->v_p != eu->eumate_p->vu_p->v_p)
			rt_bomb("I thought you said I was sharing verticies!\n");

		nmg_vertex_gv(eu->eumate_p->vu_p->v_p, hit_pt);

		NMG_CK_VERTEX_G(eu->vu_p->v_p->vg_p);
		NMG_CK_VERTEX_G(eu->eumate_p->vu_p->v_p->vg_p);
		NMG_CK_VERTEX_G(euforw->vu_p->v_p->vg_p);
		NMG_CK_VERTEX_G(euforw->eumate_p->vu_p->v_p->vg_p);

		if (euforw->vu_p->v_p != eu->eumate_p->vu_p->v_p)
			rt_bomb("I thought I was sharing verticies!\n");

		if (rt_g.NMG_debug & DEBUG_POLYSECT) {
			register pointp_t p1 = eu->vu_p->v_p->vg_p->coord;
			register pointp_t p2 = eu->eumate_p->vu_p->v_p->vg_p->coord;

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
		 *
		 * XXX Is this really a good idea?
		 */
		plu = nmg_mlv(&fu->l.magic, eu->eumate_p->vu_p->v_p, OT_UNSPEC);
		vu_other = RT_LIST_FIRST( vertexuse, &plu->down_hd );
		NMG_CK_VERTEXUSE(vu_other);
		nmg_loop_g(plu->l_p);

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
}

static void
nmg_isect_2edge_2face(bs, eu, fu)
struct nmg_inter_struct *bs;
struct edgeuse *eu;
struct faceuse *fu;
{
	rt_bomb("XXX YYY ZZZ\n");
}

/*
 *			N M G _ I S E C T _ 3 E D G E _ 3 F A C E
 *
 *	Intersect an edge with a face
 *
 * This code currently assumes that an edge can only intersect a face at one
 * point.  This is probably a bad assumption for the future
 */
static void
nmg_isect_3edge_3face(bs, eu, fu)
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
	fastf_t		dist2;
	vect_t		start_pt;
	struct edgeuse	*euforw;
	struct edgeuse	*eunext;
	struct edgeuse	*eulast;

	if (rt_g.NMG_debug & DEBUG_POLYSECT)
		rt_log("nmg_isect_3edge_3face(, eu=x%x, fu=x%x)\n", eu, fu);

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
			rt_bomb("nmg_isect_3edge_3face() discontiuous edgeloop\n");
		}
		if( eunext->vu_p->v_p != v1mate) {
			VPRINT("unshared vertex (my mate): ",
				v1mate->vg_p->coord);
			VPRINT("\t\t (next): ",
				eunext->vu_p->v_p->vg_p->coord);
			rt_bomb("nmg_isect_3edge_3face() discontinuous edgeloop\n");
		}
	}

	/*
	 * First check the topology.  If the topology says that starting
	 * vertex of this edgeuse is on the other face, enter the
	 * two vertexuses of it in the list and return.
	 */
	if (vu_other=nmg_find_3vertex_on_3face(v1, fu)) {
		if (rt_g.NMG_debug & DEBUG_POLYSECT) {
			register pointp_t p1 = v1->vg_p->coord;
			register pointp_t p2 = v1mate->vg_p->coord;
			rt_log("Edgeuse %g, %g, %g -> %g, %g, %g\n",
				V3ARGS(p1), V3ARGS(p2) );
			rt_log("\tvertex topologically on isect plane.\n\tAdding vu1=x%x (v=x%x), vu_other=x%x (v=x%x)\n",
				eu->vu_p, eu->vu_p->v_p,
				vu_other, vu_other->v_p);
		}

		(void)nmg_tbl(bs->l1, TBL_INS_UNIQUE, &eu->vu_p->l.magic);
		(void)nmg_tbl(bs->l2, TBL_INS_UNIQUE, &vu_other->l.magic);
		return;
	}

	/*
	 *  Starting vertex does not lie on the face according to topology.
	 *  Form a ray that starts at one vertex of the edgeuse
	 *  and points to the other vertex.
	 */
	VSUB2(edge_vect, v1mate->vg_p->coord, v1->vg_p->coord);
	edge_len = MAGNITUDE(edge_vect);

	VMOVE( start_pt, v1->vg_p->coord );

	if (rt_g.NMG_debug & DEBUG_POLYSECT)  {
		rt_log("Testing (%g, %g, %g) -> (%g, %g, %g) dir=(%g, %g, %g)\n",
			V3ARGS(start_pt),
			V3ARGS(v1mate->vg_p->coord),
			V3ARGS(edge_vect) );
	}

	status = rt_isect_ray_plane(&dist, start_pt, edge_vect, fu->f_p->fg_p->N);

	if (rt_g.NMG_debug & DEBUG_POLYSECT) {
	    if (status >= 0)
		rt_log("\tHit. Status of rt_isect_ray_plane: %d dist: %g\n",
				status, dist);
	    else
		rt_log("\tMiss. Boring status of rt_isect_ray_plane: %d dist: %g\n",
				status, dist);
	}
	if (status < 0)  {
		/*  Ray does not strike plane.
		 *  See if start point lies on plane.
		 */
		dist = VDOT( start_pt, fu->f_p->fg_p->N ) - fu->f_p->fg_p->N[3];
		if( !NEAR_ZERO( dist, bs->tol.dist ) )
			return;		/* No geometric intersection */

		/* Start point lies on plane of other face */
		if (rt_g.NMG_debug & DEBUG_POLYSECT)
			rt_log("\tStart point lies on plane of other face\n");
		dist = VSUB2DOT( v1->vg_p->coord, start_pt, edge_vect )
				/ edge_len;
	}

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

	if ( dist_to_plane < -bs->tol.dist )  {
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
	if ( dist_to_plane < bs->tol.dist )  {
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
			nmg_loop_g(plu->l_p);
			vu_other = RT_LIST_FIRST( vertexuse, &plu->down_hd );
			NMG_CK_VERTEXUSE(vu_other);
			(void)nmg_tbl(bs->l2, TBL_INS_UNIQUE, &vu_other->l.magic);
		}
		return;
	}

	if ( dist_to_plane < edge_len - bs->tol.dist) {
		/* Intersection is between first and second vertex points.
		 * Insert new vertex at intersection point.
		 */
		nmg_break_3edge_at_plane(hit_pt, fu, bs, eu, v1, v1mate);
		return;
	}

	if ( dist_to_plane < edge_len + bs->tol.dist) {
		/* Second point is on plane of face, by geometry */
		if (rt_g.NMG_debug & DEBUG_POLYSECT)
			rt_log("\tedge ends at plane intersect\n");

		eunext = RT_LIST_PNEXT_CIRC(edgeuse,eu);
		NMG_CK_EDGEUSE(eunext);
		if( eunext->vu_p->v_p != v1mate )
			rt_bomb("nmg_isect_3edge_3face: discontinuous eu loop\n");

		(void)nmg_tbl(bs->l1, TBL_INS_UNIQUE, &eunext->vu_p->l.magic);

		vu_other = nmg_find_vu_in_face(v1mate->vg_p->coord, fu, &(bs->tol));
		if (vu_other) {
			register pointp_t	p3;
			/* Face has a very similar vertex.  Add to list */
			(void)nmg_tbl(bs->l2, TBL_INS_UNIQUE, &vu_other->l.magic);
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
			nmg_loop_g(plu->l_p);
			vu_other = RT_LIST_FIRST( vertexuse, &plu->down_hd );
			NMG_CK_VERTEXUSE(vu_other);
			(void)nmg_tbl(bs->l2, TBL_INS_UNIQUE, &vu_other->l.magic);
		}
		return;
	}

	if (rt_g.NMG_debug & DEBUG_POLYSECT)
		rt_log("\tdist to plane X: X > MAGNITUDE(edge)\n");


}

/*
 *			N M G _ I S E C T _ P L A N E _ L O O P _ F A C E
 *
 *	Intersect a single loop with another face
 *	Handles both 3D and 2D cases.
 */
static void
nmg_isect_plane_loop_face(bs, lu, fu)
struct nmg_inter_struct *bs;
struct loopuse *lu;
struct faceuse *fu;
{
	struct edgeuse	*eu;
	long		magic1;
	struct loopuse	*fulu;

	if (rt_g.NMG_debug & DEBUG_POLYSECT) {
		rt_log("nmg_isect_plane_loop_face(, lu=x%x, fu=x%x)\n", lu, fu);
		HPRINT("  fg N", fu->f_p->fg_p->N);
	}

	NMG_CK_LOOPUSE(lu);
	NMG_CK_LOOP(lu->l_p);
	NMG_CK_LOOP_G(lu->l_p->lg_p);

	NMG_CK_FACEUSE(fu);

	magic1 = RT_LIST_FIRST_MAGIC( &lu->down_hd );
	if (magic1 == NMG_VERTEXUSE_MAGIC) {
		struct vertexuse	*vu = RT_LIST_FIRST(vertexuse,&lu->down_hd);
		/* this is most likely a loop inserted when we split
		 * up fu2 wrt fu1 (we're now splitting fu1 wrt fu2)
		 */
	     	if( bs->coplanar )
			nmg_isect_2vertex_2face(bs, vu, fu);
		else
			nmg_isect_3vertex_3face(bs, vu, fu);
		return;
	} else if (magic1 != NMG_EDGEUSE_MAGIC) {
		rt_bomb("nmg_isect_plane_loop_face() Unknown type of NMG loopuse\n");
	}

	/*  Process loop consisting of a list of edgeuses.
	 *
	 * By going backwards around the list we avoid
	 * re-processing an edgeuse that was just created
	 * by nmg_isect_3edge_3face.  This is because the edgeuses
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

	     	if( bs->coplanar )
			nmg_isect_2edge_2face(bs, eu, fu);
	     	else
			nmg_isect_3edge_3face(bs, eu, fu);

		nmg_ck_lueu(lu, "nmg_isect_plane_loop_face");
	}

}

/*
 *			N M G _ I S E C T _ T W O _ P L A N A R _ F A C E S
 *
 *	Intersect loops of face 1 with the entirety of face 2
 */
static void
nmg_isect_two_planar_faces(bs, fu1, fu2)
struct nmg_inter_struct *bs;
struct faceuse	*fu1;
struct faceuse	*fu2;
{
	struct loopuse	*lu, *fu2lu;
	struct loop_g	*lg;

	if (rt_g.NMG_debug & DEBUG_POLYSECT)
		rt_log("nmg_isect_two_planar_faces(, fu1=x%x, fu2=x%x) START ++++++++++\n", fu1, fu2);

	NMG_CK_FACE_G(fu2->f_p->fg_p);
	NMG_CK_FACE_G(fu1->f_p->fg_p);

	/* process each loop in face 1 */
	for( RT_LIST_FOR( lu, loopuse, &fu1->lu_hd ) )  {
		NMG_CK_LOOPUSE(lu);
		if (lu->up.fu_p != fu1) {
			rt_bomb("nmg_isect_two_planar_faces() Child loop doesn't share parent!\n");
		}
		NMG_CK_LOOP(lu->l_p);
		lg = lu->l_p->lg_p;
		NMG_CK_LOOP_G(lg);

		/* If the bounding box of a loop doesn't intersect the
		 * bounding box of a loop in the other face, it doesn't need
		 * to get cut.
		 */
		for (RT_LIST_FOR(fu2lu, loopuse, &fu2->lu_hd )){
			struct loop_g *fu2lg;

			NMG_CK_LOOPUSE(fu2lu);
			NMG_CK_LOOP(fu2lu->l_p);
			fu2lg = fu2lu->l_p->lg_p;
			NMG_CK_LOOP_G(fu2lg);

			if (NMG_EXTENT_OVERLAP( fu2lg->min_pt, fu2lg->max_pt,
			    lg->min_pt, lg->max_pt)) {
				nmg_isect_plane_loop_face(bs, lu, fu2);
			}
		}
	}
	if (rt_g.NMG_debug & DEBUG_POLYSECT)
		rt_log("nmg_isect_two_planar_faces(, fu1=x%x, fu2=x%x) RETURN ++++++++++\n\n", fu1, fu2);
}

/*
 *			N M G _ P R _ V E R T _ L I S T
 */
static void
nmg_pr_vert_list( str, tbl )
char		*str;
struct nmg_ptbl	*tbl;
{
	int			i;
	struct vertexuse	**vup;
	struct vertexuse	*vu;
	struct vertex		*v;
	struct vertex_g		*vg;

    	rt_log("nmg_pr_vert_list(%s):\n", str);

	vup = (struct vertexuse **)tbl->buffer;
	for (i=0 ; i < tbl->end ; ++i) {
		vu = vup[i];
		NMG_CK_VERTEXUSE(vu);
		v = vu->v_p;
		NMG_CK_VERTEX(v);
		vg = v->vg_p;
		NMG_CK_VERTEX_G(vg);
		rt_log("%d\t%g, %g, %g\t", i, V3ARGS(vg->coord) );
		if (*vu->up.magic_p == NMG_EDGEUSE_MAGIC) {
			rt_log("EDGEUSE\n");
		} else if (*vu->up.magic_p == NMG_LOOPUSE_MAGIC) {
			rt_log("LOOPUSE\n");
			if ((struct vertexuse *)vu->up.lu_p->down_hd.forw != vu) {
				rt_log("ERROR vertexuse's parent disowns us!\n");
				if (((struct vertexuse *)(vu->up.lu_p->lumate_p->down_hd.forw))->l.magic == NMG_VERTEXUSE_MAGIC)
					rt_bomb("lumate has vertexuse\n");
				else
					rt_bomb("lumate has garbage\n");
			}
		} else {
			rt_log("UNKNOWN\n");
		}
	}
}

/*
 *			N M G _ I S E C T _ T W O _ G E N E R I C _ F A C E S
 *
 *	Intersect a pair of faces
 */
static void
nmg_isect_two_generic_faces(fu1, fu2, tol)
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
	int		status;

	bs.magic = NMG_INTER_STRUCT_MAGIC;
	bs.vert2d = (fastf_t *)NULL;

	NMG_CK_FACEUSE(fu1);
	f1 = fu1->f_p;
	NMG_CK_FACE(f1);
	NMG_CK_FACE_G(f1->fg_p);

	NMG_CK_FACEUSE(fu2);
	f2 = fu2->f_p;
	NMG_CK_FACE(f2);
	NMG_CK_FACE_G(f2->fg_p);

	if (rt_g.NMG_debug & DEBUG_POLYSECT) {
		rt_log("\nnmg_isect_two_generic_faces(fu1=x%x, fu2=x%x)\n", fu1, fu2);
		pl1 = f1->fg_p->N;
		pl2 = f2->fg_p->N;

		rt_log("Planes\t%gx + %gy + %gz = %g\n\t%gx + %gy + %gz = %g\n",
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
		rt_log("co-planar faces.\n");
		bs.coplanar = 1;
		nmg_isect2d_prep( &bs, f1, f2 );
		rt_log("Skipping, for now.\n");
		return;	/* XXX break */
	case -2:
		/* parallel and distinct, no intersection */
		return;
	default:
		/* internal error */
		rt_log("ERROR nmg_isect_two_generic_faces() unable to find plane intersection\n");
		return;
	}

	if (rt_g.NMG_debug & DEBUG_POLYSECT) {
		VPRINT("isect ray bs.pt ", bs.pt);
		VPRINT("isect ray bs.dir", bs.dir);
	}

	(void)nmg_tbl(&vert_list1, TBL_INIT,(long *)NULL);
	(void)nmg_tbl(&vert_list2, TBL_INIT,(long *)NULL);

    	bs.l1 = &vert_list1;
    	bs.l2 = &vert_list2;
    	bs.tol = *tol;		/* struct copy */

    	if (rt_g.NMG_debug & (DEBUG_POLYSECT|DEBUG_FCUT|DEBUG_MESH)
    	    && rt_g.NMG_debug & DEBUG_PLOTEM) {
    		static int fno=1;
    	    	nmg_pl_2fu( "Isect_faces%d.pl", fno++, fu1, fu2, 0 );
    	}

	nmg_isect_two_planar_faces(&bs, fu1, fu2);

    	if (rt_g.NMG_debug & DEBUG_FCUT) {
	    	rt_log("nmg_isect_two_generic_faces(fu1=x%x, fu2=x%x) vert_lists A:\n", fu1, fu2);
    		nmg_pr_vert_list( "vert_list1", &vert_list1 );
    		nmg_pr_vert_list( "vert_list2", &vert_list2 );
    	}

    	bs.l2 = &vert_list1;
    	bs.l1 = &vert_list2;
	nmg_isect_two_planar_faces(&bs, fu2, fu1);

	nmg_purge_unwanted_intersection_points(&vert_list1, fu2);
	nmg_purge_unwanted_intersection_points(&vert_list2, fu1);


    	if (rt_g.NMG_debug & DEBUG_FCUT) {
	    	rt_log("nmg_isect_two_generic_faces(fu1=x%x, fu2=x%x) vert_lists B:\n", fu1, fu2);
    		nmg_pr_vert_list( "vert_list1", &vert_list1 );
    		nmg_pr_vert_list( "vert_list2", &vert_list2 );
    	}

    	if (vert_list1.end == 0) {
    		/* there were no intersections */
    		goto out;
    	}

	nmg_face_cutjoin(&vert_list1, &vert_list2, fu1, fu2, bs.pt, bs.dir, tol);

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

#if 0
	show_broken_stuff((long *)fu1, (long **)NULL, 1, 0);
	show_broken_stuff((long *)fu2, (long **)NULL, 1, 0);
#endif

out:
	(void)nmg_tbl(&vert_list1, TBL_FREE, (long *)NULL);
	(void)nmg_tbl(&vert_list2, TBL_FREE, (long *)NULL);
	if(bs.vert2d)  rt_free( (char *)bs.vert2d, "vert2d" );
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

	if (rt_g.NMG_debug & DEBUG_POLYSECT)
		rt_log("nmg_crackshells(s1=x%x, s2=x%x)\n", s1, s2);

	NMG_CK_SHELL(s1);
	sa1 = s1->sa_p;
	NMG_CK_SHELL_A(sa1);

	NMG_CK_SHELL(s2);
	sa2 = s2->sa_p;
	NMG_CK_SHELL_A(sa2);

	/* XXX this isn't true for non-3-manifold geometry! */
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

			nmg_isect_two_generic_faces(fu1, fu2, tol);

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
