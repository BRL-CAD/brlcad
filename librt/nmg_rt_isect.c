/*
 *			N M G _ R T _ I S E C T . C
 *
 *	Support routines for raytracing an NMG.
 *
 *  Author -
 *	Lee A. Butler
 *  
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066  USA
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1993 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include <stdio.h>
#include "machine.h"
#include "vmath.h"
#include "nmg.h"
#include "raytrace.h"
#include "./nmg_rt.h"

static void 	vertex_neighborhood RT_ARGS((struct ray_data *rd, struct vertexuse *vu_p, struct hitmiss *myhit));
RT_EXTERN(void	nmg_isect_ray_model, (struct ray_data *rd));

static void print_hitlist(hl)
struct hitmiss *hl;
{
	struct hitmiss *a_hit;

	if (!(rt_g.NMG_debug & DEBUG_RT_ISECT))
		return;

	rt_log("nmg/ray hit list\n");

	for (RT_LIST_FOR(a_hit, hitmiss, &(hl->l))) {
		rt_log("\tray_hit_distance %g   pt(%g %g %g) struct %s\n",
			a_hit->hit.hit_dist,
			a_hit->hit.hit_point[0],
			a_hit->hit.hit_point[1],
			a_hit->hit.hit_point[2],
			rt_identify_magic(*(int*)a_hit->hit.hit_private) );
	}
}




/*
 *	H I T _ I N S
 *
 *	insert the new hit point in the correct place in the list of hits
 *	so that the list is always in sorted order
 */
static void
hit_ins(rd, newhit)
struct ray_data *rd;
struct hitmiss *newhit;
{
	struct hitmiss *a_hit;

	if (rt_g.NMG_debug & DEBUG_RT_ISECT)
		rt_log("hit_ins()\n");

	for (RT_LIST_FOR(a_hit, hitmiss, &rd->rd_hit)) {
		if (newhit->hit.hit_dist < a_hit->hit.hit_dist)
			break;
		if (rt_g.NMG_debug & DEBUG_RT_ISECT)
			rt_log("\tahit: 0x%08x dist:%g pt:(%g %g %g)\n",
				a_hit,
				a_hit->hit.hit_dist,
				a_hit->hit.hit_point[0],
				a_hit->hit.hit_point[1],
				a_hit->hit.hit_point[2]);
	}

	/* a_hit now points to the item before which we should insert this
	 * hit in the hit list.
	 */
	 
	if (RT_LIST_NOT_HEAD(a_hit, &rd->rd_hit)) {
		if (rt_g.NMG_debug & DEBUG_RT_ISECT)
		    rt_log ("\tinserting newhit 0x%08x dist:%g pt:(%g %g %g)\n\tprior to 0x%08x dist:%g pt:(%g %g %g)\n",
			newhit,
			newhit->hit.hit_dist,
			newhit->hit.hit_point[0],
			newhit->hit.hit_point[1],
			newhit->hit.hit_point[2],
			a_hit,
			a_hit->hit.hit_dist,
			a_hit->hit.hit_point[0],
			a_hit->hit.hit_point[1],
			a_hit->hit.hit_point[2]);
	} else {
		if (rt_g.NMG_debug & DEBUG_RT_ISECT)
		    rt_log ("\tinserting newhit 0x%08x dist:%g pt:(%g %g %g)\n\tat end of list\n",
			newhit,
			newhit->hit.hit_dist,
			newhit->hit.hit_point[0],
			newhit->hit.hit_point[1],
			newhit->hit.hit_point[2]);
	}
	RT_LIST_INSERT(&a_hit->l, &newhit->l);
	RT_LIST_MAGIC_SET(&newhit->l, NMG_RT_HIT_MAGIC);
}


/*
 *  The ray missed this vertex.  Build the appropriate miss structure.
 */
static struct hitmiss *
ray_miss_vertex(rd, vu_p)
struct ray_data *rd;
struct vertexuse *vu_p;
{
	struct hitmiss *myhit;

	NMG_CK_VERTEXUSE(vu_p);
	NMG_CK_VERTEX(vu_p->v_p);
	NMG_CK_VERTEX_G(vu_p->v_p->vg_p);

	if (rt_g.NMG_debug & DEBUG_RT_ISECT) {
		rt_log("ray_miss_vertex(%g %g %g)\n",
			vu_p->v_p->vg_p->coord[0],
			vu_p->v_p->vg_p->coord[1],
			vu_p->v_p->vg_p->coord[2]);
	}

	if (myhit=NMG_INDEX_GET(rd->hitmiss, vu_p->v_p)) {
		/* vertex previously processed */
		if (RT_LIST_MAGIC_OK((struct rt_list *)myhit, NMG_RT_HIT_MAGIC)) {
			if (rt_g.NMG_debug & DEBUG_RT_ISECT)
				rt_log("ray_miss_vertex( vertex previously HIT!!!! )\n");
		} else if (RT_LIST_MAGIC_OK((struct rt_list *)myhit, NMG_RT_HIT_SUB_MAGIC)) {
			if (rt_g.NMG_debug & DEBUG_RT_ISECT)
				rt_log("ray_miss_vertex( vertex previously HIT_SUB!?!? )\n");
		}
		return(myhit);
	}
	if (myhit=NMG_INDEX_GET(rd->hitmiss, vu_p->v_p)) {
		if (rt_g.NMG_debug & DEBUG_RT_ISECT)
			rt_log("ray_miss_vertex( vertex previously missed )\n");
		return(myhit);
	}

	GET_HITMISS(myhit);
	NMG_INDEX_ASSIGN(rd->hitmiss, vu_p->v_p, myhit);

	/* get build_vertex_miss() to compute this */
	myhit->dist_in_plane = -1.0;

	/* add myhit to the list of misses */
	RT_LIST_MAGIC_SET(&myhit->l, NMG_RT_MISS_MAGIC);
	RT_LIST_INSERT(&rd->rd_miss, &myhit->l);

	return myhit;
}

/*
 *	Examine the vertex neighborhood to classify the ray as IN/ON/OUT of
 *	the NMG both before and after hitting the vertex.

N       R
^      ^
|     /
|    / |
|   /  | -NdotR (N)
|  /   |
| /    |
|/     V
.----->Q


 */
static void
vertex_neighborhood(rd, vu_p, myhit)
struct ray_data *rd;
struct vertexuse *vu_p;
struct hitmiss *myhit;
{
	struct vertexuse *vu;
	struct faceuse *fu;
	double NdotR;
	point_t q;
	vectp_t N;
	struct nmg_ptbl ftbl;


	NMG_CK_VERTEXUSE(vu_p);
	NMG_CK_VERTEX(vu_p->v_p);
	NMG_CK_VERTEX_G(vu_p->v_p->vg_p);

	(void)nmg_tbl( &ftbl, TBL_INIT, (long *)NULL );

	/* for every use of this vertex */
	for ( RT_LIST_FOR(vu, vertexuse, &vu_p->v_p->vu_hd) ) {
		/* if the parent use is an (edge/loop)use of an 
		 * OT_SAME faceuse...
		 */
		if (*vu->up.magic_p == NMG_EDGEUSE_MAGIC &&
		    *vu->up.eu_p->up.magic_p == NMG_LOOPUSE_MAGIC &&
		    *vu->up.eu_p->up.lu_p->up.magic_p == NMG_FACEUSE_MAGIC &&
		    vu->up.eu_p->up.lu_p->up.fu_p->orientation == OT_SAME) {


			fu = vu->up.eu_p->up.lu_p->up.fu_p;

		    	/* if we've already done this face,
		    	 * skip on to the next
		    	 */
			if (nmg_tbl(&ftbl, TBL_INS_UNIQUE, &fu->l.magic) >= 0)
				continue;

			/* check this face */
			NMG_CK_FACE(fu->f_p);
			NMG_CK_FACE_G(fu->f_p->fg_p);
			N = fu->f_p->fg_p->N;

			NdotR = VDOT(N, rd->rp->r_dir);
			VADD2(q, vu_p->v_p->vg_p->coord, rd->rp->r_dir);
			VJOIN1(q, q, NdotR, N);

			/* pt "q" should be on plane of the face.
			 * determine if it is in/on/out of the face boundary
			 * XXX
			 */
		}
	}
	(void)nmg_tbl( &ftbl, TBL_FREE, (long *)NULL );
}

/*
 *  Once it has been decided that the ray hits the vertex, 
 *  this routine takes care of recording that fact.
 */
static struct hitmiss *
ray_hit_vertex(rd, vu_p, status)
struct ray_data *rd;
struct vertexuse *vu_p;
int status;
{
	struct hitmiss *myhit;
	vect_t v;

	if (rt_g.NMG_debug & DEBUG_RT_ISECT)
		rt_log("ray_hit_vertex\n");

	if (myhit = NMG_INDEX_GET(rd->hitmiss, vu_p->v_p)) {
		if (RT_LIST_MAGIC_OK((struct rt_list *)myhit, NMG_RT_HIT_MAGIC))
			return(myhit);
		/* oops, we have to change a MISS into a HIT */
		RT_LIST_DEQUEUE(&myhit->l);
	} else {
		GET_HITMISS(myhit);
		NMG_INDEX_ASSIGN(rd->hitmiss, vu_p->v_p, myhit);
	}

	/* v = vector from ray point to hit vertex */
	VSUB2( v, rd->rp->r_pt, vu_p->v_p->vg_p->coord);
	myhit->hit.hit_dist = MAGNITUDE(v);	/* distance along ray */
	VMOVE(myhit->hit.hit_point, vu_p->v_p->vg_p->coord);
	myhit->hit.hit_private = (genptr_t) vu_p->v_p;

	RT_LIST_MAGIC_SET(&myhit->l, NMG_RT_HIT_MAGIC);

	/* XXX need to compute neighborhood of vertex so that ray
	 * can be classified as in/on/out before and after the vertex.
	 */
	vertex_neighborhood(rd, vu_p, myhit);

	hit_ins(rd, myhit);

	return myhit;
}


/*	I S E C T _ R A Y _ V E R T E X U S E
 *
 *	Called in one of the following situations:
 *		1)	Zero length edge
 *		2)	Vertexuse child of Loopuse
 *		3)	Vertexuse child of Shell
 *
 *	return:
 *		1 vertex was hit
 *		0 vertex was missed
 */
static int
isect_ray_vertexuse(rd, vu_p)
struct ray_data *rd;
struct vertexuse *vu_p;
{
	struct hitmiss *myhit;
	vect_t	v;
	double ray_vu_dist;

	if (rt_g.NMG_debug & DEBUG_RT_ISECT)
		rt_log("isect_ray_vertexuse\n");

	NMG_CK_VERTEXUSE(vu_p);
	NMG_CK_VERTEX(vu_p->v_p);
	NMG_CK_VERTEX_G(vu_p->v_p->vg_p);

	if (rt_g.NMG_debug & DEBUG_RT_ISECT)
		rt_log("nmg_isect_ray_vertexuse %g %g %g", 
			vu_p->v_p->vg_p->coord[0], 
			vu_p->v_p->vg_p->coord[1], 
			vu_p->v_p->vg_p->coord[2]);

	if (myhit = NMG_INDEX_GET(rd->hitmiss, vu_p->v_p)) {
		if (RT_LIST_MAGIC_OK((struct rt_list *)myhit, NMG_RT_HIT_MAGIC)) {
			/* we've previously hit this vertex */
			if (rt_g.NMG_debug & DEBUG_RT_ISECT)
				rt_log(" previously hit\n");
			return(1);
		} else {
			/* we've previously missed this vertex */
			if (rt_g.NMG_debug & DEBUG_RT_ISECT)
				rt_log(" previously missed\n");
			return 0;
		}
	}

	/* intersect ray with vertex */
	ray_vu_dist = rt_dist_line_point(rd->rp->r_pt, rd->rp->r_dir,
					 vu_p->v_p->vg_p->coord);

	if (ray_vu_dist > rd->tol->dist) {
		/* ray misses vertex */
		ray_miss_vertex(rd, vu_p);
		return 0;
	}

	/* ray hits vertex */
	ray_hit_vertex(rd, vu_p, NMG_VERT_UNKNOWN);

	if (rt_g.NMG_debug & DEBUG_RT_ISECT) {
		rt_log(" Ray hits vertex, dist %g (%d/%d)\n",
			myhit->hit.hit_dist,
			(int)myhit->hit.hit_private,
			vu_p->v_p->magic);

		print_hitlist(rd->hitmiss[NMG_HIT_LIST]);
	}

	return 1;
}


/*
 * As the name implies, this routine is called when the ray and an edge are
 * colinear.  It handles marking the verticies as hit, remembering that this
 * is a seg_in/seg_out pair, and builds the hit on the edge.
 *
 * XXX Is there something we could remember for the "closest hit" algorithm?
 *
 */
static void
colinear_edge_ray(rd, eu_p)
struct ray_data *rd;
struct edgeuse *eu_p;
{
	struct hitmiss *vhit1, *vhit2, *myhit;


	if (rt_g.NMG_debug & DEBUG_RT_ISECT)
		rt_log("\t - colinear_edge_ray\n");

	vhit1 = NMG_INDEX_GET(rd->hitmiss, eu_p->vu_p->v_p);
	vhit2 = NMG_INDEX_GET(rd->hitmiss, eu_p->eumate_p->vu_p->v_p);

	/* record the hit on each vertex, and remember that these two hits
	 * should be kept together.
	 */
	if (vhit1->hit.hit_dist > vhit2->hit.hit_dist) {
		ray_hit_vertex(rd, eu_p->vu_p, NMG_VERT_ENTER);
		vhit1->start_stop = NMG_HITMISS_SEG_OUT;

		ray_hit_vertex(rd, eu_p->eumate_p->vu_p, NMG_VERT_LEAVE);
		vhit2->start_stop = NMG_HITMISS_SEG_IN;
	} else {
		ray_hit_vertex(rd, eu_p->vu_p, NMG_VERT_LEAVE);
		vhit1->start_stop = NMG_HITMISS_SEG_IN;

		ray_hit_vertex(rd, eu_p->eumate_p->vu_p, NMG_VERT_ENTER);
		vhit2->start_stop = NMG_HITMISS_SEG_OUT;
	}
	vhit1->other = vhit2;
	vhit2->other = vhit1;

	GET_HITMISS(myhit);
	NMG_INDEX_ASSIGN(rd->hitmiss, eu_p->e_p, myhit);
	RT_LIST_MAGIC_SET(&myhit->l, NMG_RT_HIT_SUB_MAGIC);
	RT_LIST_INSERT(&rd->rd_miss, &myhit->l);
	return;
}

/*
 *  When a vertex at an end of an edge gets hit by the ray, this macro
 *  is used to build the hit structures for the vertex and the edge.
 */
#define HIT_EDGE_VERTEX(rd, eu_p, vu_p) {\
	if (rt_g.NMG_debug & DEBUG_RT_ISECT) rt_log("hit_edge_vertex\n"); \
	if (*eu_p->up.magic_p == NMG_SHELL_MAGIC || \
	    (*eu_p->up.magic_p == NMG_LOOPUSE_MAGIC && \
	     *eu_p->up.lu_p->up.magic_p == NMG_SHELL_MAGIC)) \
		ray_hit_vertex(rd, vu_p, NMG_VERT_ENTER_LEAVE); \
	else \
		ray_hit_vertex(rd, vu_p, NMG_VERT_UNKNOWN); \
	GET_HITMISS(myhit); \
	NMG_INDEX_ASSIGN(rd->hitmiss, eu_p->e_p, myhit); \
	RT_LIST_MAGIC_SET(&myhit->l, NMG_RT_HIT_SUB_MAGIC); \
	RT_LIST_INSERT(&rd->rd_miss, &myhit->l); }

/*
 *
 * record a hit on an edge.
 *
 */
static void
ray_hit_edge(rd, eu_p, dist_along_ray)
struct ray_data *rd;
struct edgeuse *eu_p;
double dist_along_ray;
{
	struct hitmiss *myhit;
	ray_miss_vertex(rd, eu_p->vu_p);
	ray_miss_vertex(rd, eu_p->eumate_p->vu_p);

	if (rt_g.NMG_debug & DEBUG_RT_ISECT) rt_log("\t - HIT edge\n");

	/* create hit structure for this edge */
	GET_HITMISS(myhit);
	NMG_INDEX_ASSIGN(rd->hitmiss, eu_p->e_p, myhit);

	/* Since this is a ray/edge intersection, we don't consider
	 * this hit in the "nearest hit" calculations for a faceuse
	 * hit.  That comes later with the edge/hitpoint
	 * consideration.
	 */
	myhit->dist_in_plane = 0.0;


	/* XXX Need to compute IN/OUT status for the ray wrt the faces about
	 * the edge and fill in the in_out element in the hitmiss structure.
	 */

	/* build the hit structure */
	myhit->hit.hit_dist = dist_along_ray;
	VJOIN1(myhit->hit.hit_point, 
		rd->rp->r_pt, dist_along_ray, rd->rp->r_dir);
	myhit->hit.hit_private = (genptr_t) eu_p->e_p;

	hit_ins(rd, myhit);
}

/*	I S E C T _ R A Y _ E D G E U S E
 *
 *	Intersect ray with edgeuse.  If they pass within tolerance, a hit
 *	is generated.
 *
 *	If the edgeuse is part of a face, (plane_pt is set) then we also
 *	compute the distance from the edge to the plane_pt.  If this is
 *	less than the previous "closest" value then record this edgeuse as
 *	the "plane_closest" object.
 */
static void
isect_ray_edgeuse(rd, eu_p)
struct ray_data *rd;
struct edgeuse *eu_p;
{
	int status;
	struct hitmiss *myhit;
	double dist_along_ray;
	int vhit1, vhit2;

	NMG_CK_EDGEUSE(eu_p);
	NMG_CK_EDGEUSE(eu_p->eumate_p);
	NMG_CK_EDGE(eu_p->e_p);
	NMG_CK_VERTEXUSE(eu_p->vu_p);
	NMG_CK_VERTEXUSE(eu_p->eumate_p->vu_p);
	NMG_CK_VERTEX(eu_p->vu_p->v_p);
	NMG_CK_VERTEX(eu_p->eumate_p->vu_p->v_p);

	if (rt_g.NMG_debug & DEBUG_RT_ISECT) {
		rt_log("isect_ray_edgeuse (%g %g %g) -> (%g %g %g)",
			eu_p->vu_p->v_p->vg_p->coord[0],
			eu_p->vu_p->v_p->vg_p->coord[1],
			eu_p->vu_p->v_p->vg_p->coord[2],
			eu_p->eumate_p->vu_p->v_p->vg_p->coord[0],
			eu_p->eumate_p->vu_p->v_p->vg_p->coord[1],
			eu_p->eumate_p->vu_p->v_p->vg_p->coord[2]);
	}

	if (myhit = NMG_INDEX_GET(rd->hitmiss, eu_p->e_p)) {
		if (RT_LIST_MAGIC_OK((struct rt_list *)myhit, NMG_RT_HIT_MAGIC)) {
			/* previously hit vertex or edge */
			if (rt_g.NMG_debug & DEBUG_RT_ISECT)
				rt_log("\tedge previously hit\n");
			return;
		} else if (RT_LIST_MAGIC_OK((struct rt_list *)myhit, NMG_RT_HIT_SUB_MAGIC)) {
			if (rt_g.NMG_debug & DEBUG_RT_ISECT)
				rt_log("\tvertex of edge previously hit\n");
			return;
		} else if (RT_LIST_MAGIC_OK((struct rt_list *)myhit, NMG_RT_MISS_MAGIC)) {
			if (rt_g.NMG_debug & DEBUG_RT_ISECT)
				rt_log("\tedge previously missed\n");
			return;
		} else {
			nmg_rt_bomb(rd, "what happened?\n");
		}
	}


	status = rt_isect_line_lseg( &dist_along_ray,
			rd->rp->r_pt, rd->rp->r_dir, 
			eu_p->vu_p->v_p->vg_p->coord,
			eu_p->eumate_p->vu_p->v_p->vg_p->coord,
			rd->tol);

	switch (status) {
	case -4 :
		/* Zero length edge.  The routine rt_isect_line_lseg() can't
		 * help us.  Intersect the ray with each vertex.  If either
		 * vertex is hit, then record that the edge has sub-elements
		 * which where hit.  Otherwise, record the edge as being
		 * missed.
		 */

		if (rt_g.NMG_debug & DEBUG_RT_ISECT)
			rt_log("\t - Zero length edge\n");

		vhit1 = isect_ray_vertexuse(rd->rp, eu_p->vu_p);
		vhit2 = isect_ray_vertexuse(rd->rp, eu_p->eumate_p->vu_p);

		GET_HITMISS(myhit);
		NMG_INDEX_ASSIGN(rd->hitmiss, eu_p->e_p, myhit);

		if (vhit1 || vhit2) {
			/* we hit the vertex */
			RT_LIST_MAGIC_SET(&myhit->l, NMG_RT_HIT_SUB_MAGIC);
		} else {
			/* both vertecies were missed, so edge is missed */
			RT_LIST_MAGIC_SET(&myhit->l, NMG_RT_MISS_MAGIC);
		}
		RT_LIST_INSERT(&rd->rd_miss, &myhit->l);
		break;
	case -3 :	/* fallthrough */
	case -2 :
		/* The ray misses the edge segment, but hits the infinite
		 * line of the edge to one end of the edge segment.  This
		 * is an exercise in tabulating the nature of the miss.
		 */

		if (rt_g.NMG_debug & DEBUG_RT_ISECT)
			rt_log("\t - Miss edge, \"hit\" line\n");
		/* record the fact that we missed each vertex */
		(void)ray_miss_vertex(rd, eu_p->vu_p);
		(void)ray_miss_vertex(rd, eu_p->eumate_p->vu_p);

		/* record the fact that we missed the edge */
		GET_HITMISS(myhit);
		NMG_INDEX_ASSIGN(rd->hitmiss, eu_p->e_p, myhit);

		RT_LIST_MAGIC_SET(&myhit->l, NMG_RT_MISS_MAGIC);
		RT_LIST_INSERT(&rd->rd_miss, &myhit->l);

		break;
	case -1 : /* just plain missed the edge/line */
		if (rt_g.NMG_debug & DEBUG_RT_ISECT)
			rt_log("\t - Miss edge/line\n");

		ray_miss_vertex(rd, eu_p->vu_p);
		ray_miss_vertex(rd, eu_p->eumate_p->vu_p);

		GET_HITMISS(myhit);
		NMG_INDEX_ASSIGN(rd->hitmiss, eu_p->e_p, myhit);

		RT_LIST_MAGIC_SET(&myhit->l, NMG_RT_MISS_MAGIC);
		RT_LIST_INSERT(&rd->rd_miss, &myhit->l);

		break;
	case 0 :  /* oh joy.  Lines are co-linear */
		colinear_edge_ray(rd, eu_p);
		break;
	case 1 :
		HIT_EDGE_VERTEX(rd, eu_p, eu_p->vu_p);
		break;
	case 2 :
		HIT_EDGE_VERTEX(rd, eu_p, eu_p->eumate_p->vu_p);
		break;
	case 3 : /* a hit on an edge */
		ray_hit_edge(rd, eu_p, dist_along_ray);
		break;
	}
}






static void
dist_plane_pt_vu(rd, vu_p)
struct ray_data *rd;
struct vertexuse *vu_p;
{
	vect_t v;
	fastf_t dist;
    	VSUB2(v, rd->plane_pt, vu_p->v_p->vg_p->coord);	
	dist = MAGNITUDE(v);

	if (dist < rd->tol->dist) {
		/* make sure there is a hit on the vertex */
	}

	if (dist < rd->plane_dist) {
		if (rt_g.NMG_debug & DEBUG_RT_ISECT)
			rt_log("Vertex (%g %g %g) is new closest\n",
				vu_p->v_p->vg_p->coord[0],
				vu_p->v_p->vg_p->coord[1],
				vu_p->v_p->vg_p->coord[2]);
		rd->plane_dist = dist;
		rd->plane_dist_type = NMG_PCA_VERTEX;
		rd->plane_closest = (long *)vu_p->v_p;
	}
}
static void
dist_plane_pt_eu(rd, eu_p)
struct ray_data *rd;
struct edgeuse *eu_p;
{
	fastf_t dist;
	point_t pca;
	int status;
	struct edgeuse *eu;

	if (rt_g.NMG_debug & DEBUG_RT_ISECT) {
		rt_log(
		"dist_plane_pt_eu((%g %g %g), 0x%08x(%g %g %g -> %g %g %g)\n",
				rd->plane_pt[0],
				rd->plane_pt[1],
				rd->plane_pt[2],
				eu_p,
				eu_p->vu_p->v_p->vg_p->coord[0],
				eu_p->vu_p->v_p->vg_p->coord[1],
				eu_p->vu_p->v_p->vg_p->coord[2],
				eu_p->eumate_p->vu_p->v_p->vg_p->coord[0],
				eu_p->eumate_p->vu_p->v_p->vg_p->coord[1],
				eu_p->eumate_p->vu_p->v_p->vg_p->coord[2]);
		rt_log("\told plane_dist = %g\n", rd->plane_dist);
		switch(rd->plane_dist_type) {
		case NMG_PCA_EDGE	:
			rt_log("\told plane_dist_type NMG_PCA_EDGE\n"); break;
		case NMG_PCA_EDGE_VERTEX :
			rt_log("\told plane_dist_type NMG_PCA_EDGE_VERTEX\n"); break;
		case NMG_PCA_VERTEX	:
			rt_log("\told plane_dist_type NMG_PCA_VERTEX\n"); break;
		default:
			rt_log("\told plane_dist_type UNDEFINED\n"); break;
		}
	}
	/* Determine if the plane intercept is closer to this edgeuse than
	 * the previously closest edge/vertex
	 */
	dist = 0.0;
	status = rt_dist_pt3_lseg3(&dist, pca,
		eu_p->vu_p->v_p->vg_p->coord,
		eu_p->eumate_p->vu_p->v_p->vg_p->coord, rd->plane_pt,
		rd->tol);

	switch (status) {
	case 0: /* we hit the edge(use) with the plane_pt,
		 * store edgeuse ptr in plane_closest,
		 * set plane_dist = 0.0
		 *
		 * dist is the parametric distance along the edge where
		 * the PCA occurrs!
		 */

		rd->plane_dist = 0.0;
		rd->plane_closest = &eu_p->l.magic;
		rd->plane_dist_type = NMG_PCA_EDGE;
		if (rt_g.NMG_debug & DEBUG_RT_ISECT)
			rt_log(	"\tplane_pt on eu, (new dist: %g)\n",
				rd->plane_dist);
		/* XXX make sure we have a hit on this edge? */
		break;
	case 1:	/* within tolerance of endpoint at eu_p->vu_p.
		   store vertexuse ptr in plane_closest,
		   set plane_dist = 0.0 */
		if (dist < rd->plane_dist) {
			rd->plane_dist = 0.0;
			rd->plane_closest = &eu_p->vu_p->l.magic;
			rd->plane_dist_type = NMG_PCA_EDGE_VERTEX;
			if (rt_g.NMG_debug & DEBUG_RT_ISECT)
				rt_log("\tplane_pt on vu (new dist %g)\n",
					rd->plane_dist);
		} else {
			if (rt_g.NMG_debug & DEBUG_RT_ISECT)
				rt_log("\tplane_pt on vu(dist %g) keeping old dist %g)\n",
					dist, rd->plane_dist);
		}
		/* XXX make sure we have a hit on the vertex? */
		break;
	case 2:	/* within tolerance of endpoint at eu_p->eumate_p
		   store vertexuse ptr (eu->next) in plane_closest,
		   set plane_dist = 0.0 */
		if (dist < rd->plane_dist) {
			eu = RT_LIST_PNEXT_CIRC(edgeuse, &(eu_p->l));
			rd->plane_dist = 0.0;
			rd->plane_closest = &eu->vu_p->l.magic;
			rd->plane_dist_type = NMG_PCA_EDGE_VERTEX;
			if (rt_g.NMG_debug & DEBUG_RT_ISECT)
				rt_log("\tplane_pt on next(eu_p)->vu (new dist %g)\n",
					rd->plane_dist);
		} else {
			if (rt_g.NMG_debug & DEBUG_RT_ISECT)
				rt_log("\tplane_pt on next(eu_p)->vu(dist %g) keeping old dist %g)\n",
					dist, rd->plane_dist);
		}
		/* XXX make sure we have a hit on the vertex? */
		break;
	case 3: /* PCA of pt on line is "before" eu_p->vu_p of seg */
		if (dist < rd->plane_dist) {
			rd->plane_dist = dist;
			rd->plane_closest = &eu_p->vu_p->l.magic;
			rd->plane_dist_type = NMG_PCA_EDGE_VERTEX;
			if (rt_g.NMG_debug & DEBUG_RT_ISECT)
				rt_log("\vu of eu is new \"closest to plane_pt\" (new dist %g)\n", rd->plane_dist);
		} else {
			if (rt_g.NMG_debug & DEBUG_RT_ISECT)
				rt_log("\vu of eu is PCA (dist %g).  keeping old dist %g\n", dist, rd->plane_dist);
		}
		break;
	case 4: /* PCA of pt on line is "after" eu_p->eumate_p->vu_p of seg */
		if (dist < rd->plane_dist) {
			eu = RT_LIST_PNEXT_CIRC(edgeuse, &(eu_p->l));
			rd->plane_dist = dist;
			rd->plane_closest = &eu->vu_p->l.magic;
			rd->plane_dist_type = NMG_PCA_EDGE_VERTEX;
			if (rt_g.NMG_debug & DEBUG_RT_ISECT)
				rt_log("\tvu of next(eu) is new \"closest to plane_pt\" (new dist %g)\n", rd->plane_dist);
		} else {
			if (rt_g.NMG_debug & DEBUG_RT_ISECT)
				rt_log("\vu of next(eu) is PCA (dist %g).  keeping old dist %g\n", dist, rd->plane_dist);
		}
		break;
	case 5: /* PCA is along length of edge, but point is NOT on edge.
		 *  if edge is closer to plane_pt than any previous item,
		 *  store edgeuse ptr in plane_closest and set plane_dist
		 */
		if (dist < rd->plane_dist) {
			rd->plane_dist = dist;
			rd->plane_closest = &eu_p->l.magic;
			rd->plane_dist_type = NMG_PCA_EDGE;
			if (rt_g.NMG_debug & DEBUG_RT_ISECT)
				rt_log("\teu is new \"closest to plane_pt\" (new dist %g)\n", rd->plane_dist);
		} else {
			if (rt_g.NMG_debug & DEBUG_RT_ISECT)
				rt_log("\teu dist is %g, keeping old dist %g)\n", dist, rd->plane_dist);
		}
		break;
	default :
		nmg_rt_bomb(rd, "Look, there has to be SOMETHING about this edge/plane_pt\n");
		break;
	}
}

/*	I S E C T _ R A Y _ L O O P U S E
 *
 */
static void
isect_ray_loopuse(rd, lu_p)
struct ray_data *rd;
struct loopuse *lu_p;
{
	struct edgeuse *eu_p;

	if (rt_g.NMG_debug & DEBUG_RT_ISECT)
		rt_log("isect_ray_loopuse( 0x%08x, loop:0x%08x)\n", rd, lu_p->l_p);

	NMG_CK_LOOPUSE(lu_p);
	NMG_CK_LOOP(lu_p->l_p);
	NMG_CK_LOOP_G(lu_p->l_p->lg_p);

	if (RT_LIST_FIRST_MAGIC(&lu_p->down_hd) == NMG_EDGEUSE_MAGIC) {
		if (rd->plane_pt) {
			for (RT_LIST_FOR(eu_p, edgeuse, &lu_p->down_hd)) {
				/* first we determine if the ray hits
				 * the edge
				 */
				isect_ray_edgeuse(rd, eu_p);

				/* now we determine if the distance of the
				 * ray-plane intercept to the edge(use)
				 */
				dist_plane_pt_eu(rd, eu_p);
			}
		} else {
			for (RT_LIST_FOR(eu_p, edgeuse, &lu_p->down_hd)) {
				isect_ray_edgeuse(rd, eu_p);
			}
		}
		return;

	} else if (RT_LIST_FIRST_MAGIC(&lu_p->down_hd)!=NMG_VERTEXUSE_MAGIC) {
		rt_log("in %s at %d", __FILE__, __LINE__);
		nmg_rt_bomb(rd, " bad loopuse child magic");
	}

	/* loopuse child is vertexuse */

	(void) isect_ray_vertexuse(rd,
		RT_LIST_FIRST(vertexuse, &lu_p->down_hd));
	if (rd->plane_pt)
		dist_plane_pt_vu(rd,
			RT_LIST_FIRST(vertexuse, &lu_p->down_hd));
}



#define FACE_MISS(rd, f_p) {\
	struct hitmiss *myhit; \
	GET_HITMISS(myhit); \
	NMG_INDEX_ASSIGN(rd->hitmiss, f_p, myhit); \
	RT_LIST_MAGIC_SET(&myhit->l, NMG_RT_HIT_SUB_MAGIC); \
	RT_LIST_INSERT(&rd->rd_miss, &myhit->l); }

static void
face_closest_is_eu(rd, fu_p, eu)
struct ray_data *rd;
struct faceuse *fu_p;
struct edgeuse *eu;
{
	struct edgeuse *eu_p;
	struct faceuse *nmg_find_fu_of_eu();
	struct hitmiss *myhit;
	int mate_rad = 0;
	vect_t eu_v, left, pt_v;

	if (rt_g.NMG_debug & DEBUG_RT_ISECT)
		rt_log("face_closest_is_eu()\n");

	NMG_CK_EDGEUSE(eu);

	/* if there is a hit on the edge which is "closest" to the plane
	 * intercept point, then we do not add a hit point for the face
	 */
	if (myhit = NMG_INDEX_GET(rd->hitmiss, eu->e_p)) {
		if (RT_LIST_MAGIC_OK((struct rt_list *)myhit, NMG_RT_HIT_MAGIC)) {
			if (rt_g.NMG_debug & DEBUG_RT_ISECT)
				rt_log("\tedge hit, no face hit here\n");
			FACE_MISS(rd, fu_p->f_p);
			return;
		} else if (RT_LIST_MAGIC_OK((struct rt_list *)myhit, NMG_RT_HIT_SUB_MAGIC)) {
			if (rt_g.NMG_debug & DEBUG_RT_ISECT)
				rt_log("\tedge vertex hit, no face hit here\n");
			FACE_MISS(rd, fu_p->f_p);
			return;
		}
	}

	/* There is no hit on the closest edge, so we must determine
	 * whether or not the hit point is "inside" or "outside" the surface
	 * area of the face
	 */
	if (fu_p->orientation == OT_OPPOSITE) {
		if (rt_g.NMG_debug & DEBUG_RT_ISECT)
			rt_log("switching faceuses\n");
		fu_p = fu_p->fumate_p;
	}

	/* if eu isn't in this faceuse, go find a use of this edge
	 * that *IS* in this faceuse
	 */
	eu_p = eu;
	while (nmg_find_fu_of_eu(eu_p) != fu_p) {

		if (rt_g.NMG_debug & DEBUG_RT_ISECT)
			rt_log("face_closest_is_eu( edgeuse 0x%08x wasn't in faceuse )\n", eu_p);

		if (mate_rad) eu_p = eu_p->eumate_p;
		else eu_p = eu_p->radial_p;
		mate_rad = !mate_rad;

		if (eu_p == eu) {
			nmg_rt_bomb(rd, "couldn't find faceuse as parent of edge\n");
		}
	}


	/* XXX  The Paul Tanenbaum "patch"
	 *	If the vertex of the edge is closer to the point than the
	 *	span of the edge, we must pick the edge leaving the vertex
	 *	which has the smallest value of VDOT(eu->left, vp) where vp
	 *	is the normalized vector from V to P and eu->left is the
	 *	normalized "face" vector of the edge.  This handles the case
	 *	diagramed below, where either e1 OR e2 could be the "closest"
	 *	edge based upon edge-point distances.
	 *
	 *
	 *	    \	    /
	 *		     o P
	 *	      \   /
	 *
	 *	      V o
	 *	       /-\
	 *	      /---\
	 *	     /-   -\
	 *	    /-	   -\
	 *	e1 /-	    -\	e2
	 *
	 */

	if (rd->plane_dist_type == NMG_PCA_EDGE_VERTEX &&
	    rd->plane_dist != 0.0) {
	    	/* find the appropriate "closest edge" use of this vertex. */
	    	vect_t vp;
	    	vect_t fv;
		double vdot_min = 2.0;
		double newvdot;
	    	struct edgeuse *closest_eu;
	    	struct faceuse *fu;
	    	struct vertexuse *vu;

	    	/* First we form the unit vector PV */
	    	VSUB2(vp, rd->plane_pt, eu_p->vu_p->v_p->vg_p->coord);
	    	VUNITIZE(vp);

	    	/* for each edge in this face about the vertex, determine
	    	 * the angle between PV and the face vector.  Classify the
		 * plane_pt WRT the edge for which VDOT(left, P) with P
	    	 */
	    	for (RT_LIST_FOR(vu, vertexuse, &eu->vu_p->v_p->vu_hd)) {
	    		/* if there is a faceuse ancestor of this vertexuse
	    		 * which is associated with this face, then perform
	    		 * the VDOT and save the results
	    		 */
			if (*vu->up.magic_p == NMG_EDGEUSE_MAGIC &&
			    (nmg_find_fu_of_vu(vu))->f_p == fu_p->f_p) {

				VCROSS(fv, fu_p->f_p->fg_p->N, vu->up.eu_p->e_p->eg_p->e_dir)
				/* since eg_p->dir is not unit length, we must
				 * unitize the result
				 */
				VUNITIZE(fv);
				newvdot = VDOT(fv, vp);
			    	/* if the face vector of eu is "more opposed"
			    	 * to the vector vp than the previous edge,
			    	 * remember this edge as the "closest" one
			    	 */
				if (newvdot < vdot_min) {
					closest_eu = vu->up.eu_p;
					vdot_min - newvdot;
				}
			}
	    	}
	    	fu = nmg_find_fu_of_eu(closest_eu);
	    	if (fu == fu_p) {
		    	eu_p = closest_eu;
		} else if (fu == fu_p->fumate_p) {
		    	eu_p = closest_eu->eumate_p;
		} else {
			nmg_rt_bomb(rd, "why can't i find this edge in this face?\n");
		}
	}

	/* if plane intersect is on the face-surface side of the edge then
	 * we need a hit point on the face.
	 */

	/* XXX */

	GET_HITMISS(myhit);
	NMG_INDEX_ASSIGN(rd->hitmiss, fu_p->f_p, myhit);


	VSUB2(eu_v,eu_p->eumate_p->vu_p->v_p->vg_p->coord,
		eu_p->vu_p->v_p->vg_p->coord);

	if (rt_g.NMG_debug & DEBUG_RT_ISECT) {
		VPRINT("\teu", eu_p->vu_p->v_p->vg_p->coord);
		VPRINT("\teumate", eu_p->eumate_p->vu_p->v_p->vg_p->coord);
		VPRINT("\tpt", rd->plane_pt);
		HPRINT("\tN", fu_p->f_p->fg_p->N);
		VPRINT("\teu_v", eu_v);
	}
	VCROSS(left, fu_p->f_p->fg_p->N, eu_v);
	VSUB2(pt_v, rd->plane_pt, eu_p->vu_p->v_p->vg_p->coord);

	if (rt_g.NMG_debug & DEBUG_RT_ISECT) {
		VPRINT("\tleft", left);
		VPRINT("\tpt_v", pt_v);
	}

	if (VDOT(left, pt_v) < 0.0) {
		if (rt_g.NMG_debug & DEBUG_RT_ISECT)
			rt_log("face_closest_is_eu( plane hit outside face )\n");
		RT_LIST_MAGIC_SET(&myhit->l, NMG_RT_MISS_MAGIC);
		RT_LIST_INSERT(&rd->rd_miss, &myhit->l);
		return;
	}

	if (rt_g.NMG_debug & DEBUG_RT_ISECT)
		rt_log("face_closest_is_eu( New hit on face )\n");

	myhit->dist_in_plane = 0.0;

	VMOVE(myhit->hit.hit_point, rd->plane_pt);
	myhit->hit.hit_dist = rd->ray_dist_to_plane;
	myhit->hit.hit_private = (genptr_t) fu_p->f_p;

	/* add hit to list of hit points */
	hit_ins(rd, myhit);
}

/* 
 * The element which was closest to the plane intercept was a vertex(use).
 *
 */
static void
face_closest_is_vu(rd, fu_p, vu)
struct ray_data *rd;
struct faceuse *fu_p;
struct vertexuse *vu;
{
	struct vertexuse *vu_p;
	struct faceuse *nmg_find_fu_of_vu();
	struct hitmiss *myhit;
	struct faceuse *fu;

	if (rt_g.NMG_debug & DEBUG_RT_ISECT)
		rt_log("face_closest_is_vu\n");

	if (myhit = NMG_INDEX_GET(rd->hitmiss, vu->v_p)) {
		if (RT_LIST_MAGIC_OK((struct rt_list *)myhit, NMG_RT_HIT_MAGIC)) {
			if (rt_g.NMG_debug & DEBUG_RT_ISECT)
				rt_log("\tvertex hit, no face hit here\n");
			FACE_MISS(rd, fu_p->f_p);
			return;
		}
	}

	/* if vu isn't in this faceuse, go find a use of this vertex
	 * that *IS* in this faceuse.
	 */
	fu = nmg_find_fu_of_vu(vu);		
	if (fu != fu_p) {
		for (RT_LIST_FOR(vu_p, vertexuse, &vu->v_p->vu_hd)) {
			if (nmg_find_fu_of_vu(vu) == fu_p)
				goto got_fu;
		}
		nmg_rt_bomb(rd, "didn't find faceuse from plane_closest vertex\n");
		got_fu:
		vu = vu_p;
	}

	GET_HITMISS(myhit);
	NMG_INDEX_ASSIGN(rd->hitmiss, fu_p->f_p, myhit);

	/* XXX */
	if (*vu->up.magic_p == NMG_LOOPUSE_MAGIC) {
		struct loopuse *lu = vu->up.lu_p;
		if (lu->orientation == OT_OPPOSITE) {
			/* if parent is OT_OPPOSITE loopuse, we've hit face */
		} else if (lu->orientation == OT_SAME) {
			/* if parent is OT_SAME loopuse, we've missed face */
			RT_LIST_MAGIC_SET(&myhit->l, NMG_RT_MISS_MAGIC);
			RT_LIST_INSERT(&rd->rd_miss, &myhit->l);
		} else {
			rt_log("non-oriented loopuse parent at: %s %d\n",
				__FILE__, __LINE__);
			nmg_rt_bomb(rd, "goodnight\n");
		}
	} else if (*vu->up.magic_p == NMG_EDGEUSE_MAGIC) {
		/* if parent is edgeuse, do left/right computation */
	} else {
		rt_log("vertexuse has strange parent at: %s %d\n",
			__FILE__, __LINE__);
		nmg_rt_bomb(rd, "arrivaderchi\n");
	}

}

/*	I S E C T _ R A Y _ F A C E U S E
 *
 *	check to see if ray hits face.
 */
static void
isect_ray_faceuse(rd, fu_p)
struct ray_data *rd;
struct faceuse *fu_p;
{
	fastf_t dist;
	struct loopuse *lu_p;
	point_t plane_pt;
	struct hitmiss *myhit;
	int code;

	if (rt_g.NMG_debug & DEBUG_RT_ISECT)
		rt_log("isect_ray_faceuse(0x%08x, face:0x%08x)", rd, fu_p->f_p);

	NMG_CK_FACEUSE(fu_p);
	NMG_CK_FACEUSE(fu_p->fumate_p);
	NMG_CK_FACE(fu_p->f_p);
	NMG_CK_FACE_G(fu_p->f_p->fg_p);

	/* if this face already processed, we are done. */
	if (myhit = NMG_INDEX_GET(rd->hitmiss, fu_p->f_p)) {
		if (RT_LIST_MAGIC_OK((struct rt_list *)myhit, NMG_RT_HIT_MAGIC)) {
			if (rt_g.NMG_debug & DEBUG_RT_ISECT)
				rt_log(" previously hit\n");
		} else if (RT_LIST_MAGIC_OK((struct rt_list *)myhit, NMG_RT_HIT_SUB_MAGIC)) {
			if (rt_g.NMG_debug & DEBUG_RT_ISECT)
				rt_log(" previously hit sub-element\n");
		} else if (RT_LIST_MAGIC_OK((struct rt_list *)myhit, NMG_RT_MISS_MAGIC)) {
			if (rt_g.NMG_debug & DEBUG_RT_ISECT)
				rt_log(" previously missed\n");
		} else {
			nmg_rt_bomb(rd, "Was I hit or not?\n");
		}
		return;
	}


	/* bounding box intersection */
	if (!rt_in_rpp(rd->rp, rd->rd_invdir,
	    fu_p->f_p->fg_p->min_pt, fu_p->f_p->fg_p->max_pt) ) {
		GET_HITMISS(myhit);
		NMG_INDEX_ASSIGN(rd->hitmiss, fu_p->f_p, myhit);
		RT_LIST_MAGIC_SET(&myhit->l, NMG_RT_MISS_MAGIC);
		RT_LIST_INSERT(&rd->rd_miss, &myhit->l);
		if (rt_g.NMG_debug & DEBUG_RT_ISECT)
			rt_log("missed bounding box\n");
		return;
	}

	if (rt_g.NMG_debug & DEBUG_RT_ISECT) rt_log(" hit bounding box \n");

	code = rt_isect_line3_plane(&dist, rd->rp->r_pt, rd->rp->r_dir,
			fu_p->f_p->fg_p->N, rd->tol);

	if (code < 0) {
		/* ray is parallel to halfspace and (-1)inside or (-2)outside
		 * the halfspace.
		 */
		if (rt_g.NMG_debug & DEBUG_RT_ISECT)
			rt_log("\tray misses halfspace of plane\n");

		GET_HITMISS(myhit);
		NMG_INDEX_ASSIGN(rd->hitmiss, fu_p->f_p, myhit);
		RT_LIST_MAGIC_SET(&myhit->l, NMG_RT_MISS_MAGIC);
		RT_LIST_INSERT(&rd->rd_miss, &myhit->l);
		return;
	} else if (code == 0) {
		/* XXX gack!  ray lies in plane.  
		 * In leiu of doing 2D intersection we define such rays
		 * as "missing" the face.  We rely on other faces to generate
		 * hit points.
		 */
		rt_log("\tWarning:  Ignoring ray in plane of face (NOW A MISS) XXX\n");
		GET_HITMISS(myhit);
		NMG_INDEX_ASSIGN(rd->hitmiss, fu_p->f_p, myhit);
		RT_LIST_MAGIC_SET(&myhit->l, NMG_RT_MISS_MAGIC);
		RT_LIST_INSERT(&rd->rd_miss, &myhit->l);
		return;
	}

	/* ray hits plane */

	/* project point into plane of face */
	VJOIN1(plane_pt, rd->rp->r_pt, dist, rd->rp->r_dir);
	rd->plane_pt = plane_pt;
	rd->plane_dist = MAX_FASTF;
	rd->ray_dist_to_plane = dist;

	if (rt_g.NMG_debug & DEBUG_RT_ISECT) {
		rt_log("\tray (%g %g %g) (-> %g %g %g)\n",
			rd->rp->r_pt[0],
			rd->rp->r_pt[1],
			rd->rp->r_pt[2],
			rd->rp->r_dir[0],
			rd->rp->r_dir[1],
			rd->rp->r_dir[2]);
			
		VPRINT("\tplane/ray intersection point", plane_pt);

		rt_log("\tdistance along ray to intersection point %g\n",
			dist);
	}


	/* check each loopuse in the faceuse to determine hit/miss and
	 * find the object which is "closest" to the ray/plane intercept.
	 */
	for ( RT_LIST_FOR(lu_p, loopuse, &fu_p->lu_hd) )
		isect_ray_loopuse(rd, lu_p);


	/* find the sub-element that was closest to the point of intersection.
	 * If this element was hit, we don't need to generate a hit point on
	 * the surface of the face.  If it was a miss, we may need to generate
	 * a hit on the surface of the face.
	 */
	switch (*rd->plane_closest) {
	case NMG_VERTEXUSE_MAGIC:
		face_closest_is_vu(rd, fu_p,
			(struct vertexuse *)rd->plane_closest);
		break;
	case NMG_EDGEUSE_MAGIC:
		face_closest_is_eu(rd, fu_p,
			(struct edgeuse *)rd->plane_closest);
		break;
	default:
		rt_log("%s %d Bogus magic (0x%08x/%dfor element closest to plane/face intercept\n",
			__FILE__, __LINE__,
			*rd->plane_closest, *rd->plane_closest);
		nmg_rt_bomb(rd, "Hello.  My $NAME is Indigo Montoya.  You killed my process.  Prepare to vi!\n");
	}

	rd->plane_pt = (pointp_t)NULL;
}


/*	I S E C T _ R A Y _ S H E L L
 *
 *	Implicit return:
 *		adds hit points to the hit-list "hl"
 */
static void
nmg_isect_ray_shell(rd, s_p)
struct ray_data *rd;
struct shell *s_p;
{
	struct faceuse *fu_p;
	struct loopuse *lu_p;
	struct edgeuse *eu_p;

	if (rt_g.NMG_debug & DEBUG_RT_ISECT)
		rt_log("nmg_isect_ray_shell( 0x%08x, 0x%08x)\n", rd, s_p);

	NMG_CK_SHELL(s_p);
	NMG_CK_SHELL_A(s_p->sa_p);

	/* does ray isect shell rpp ?
	 * if not, we can just return.  there is no need to record the
	 * miss for the shell, as there is only one "use" of a shell.
	 */
	if (!rt_in_rpp(rd->rp, rd->rd_invdir,
	    s_p->sa_p->min_pt, s_p->sa_p->max_pt) ) {
		if (rt_g.NMG_debug & DEBUG_RT_ISECT)
			rt_log("nmg_isect_ray_shell( no RPP overlap)\n",
				 rd, s_p);
		return;
	}

	/* ray intersects shell, check sub-objects */

	for (RT_LIST_FOR(fu_p, faceuse, &(s_p->fu_hd)) )
		isect_ray_faceuse(rd, fu_p);

	for (RT_LIST_FOR(lu_p, loopuse, &(s_p->lu_hd)) )
		isect_ray_loopuse(rd, lu_p);

	for (RT_LIST_FOR(eu_p, edgeuse, &(s_p->eu_hd)) )
		isect_ray_edgeuse(rd, eu_p);

	if (s_p->vu_p)
		(void)isect_ray_vertexuse(rd, s_p->vu_p);

	if (rt_g.NMG_debug & DEBUG_RT_ISECT)
		rt_log("nmg_isect_ray_shell( done )\n", rd, s_p);
}


/*	N M G _ I S E C T _ R A Y _ M O D E L
 *
 */
void
nmg_isect_ray_model(rd)
struct ray_data *rd;
{
	struct nmgregion *r_p;
	struct shell *s_p;
	struct hitmiss *hm;


	if (rt_g.NMG_debug & DEBUG_RT_ISECT)
		rt_log("isect_ray_nmg: Pnt(%g %g %g) Dir(%g %g %g)\n", 
			rd->rp->r_pt[0],
			rd->rp->r_pt[1],
			rd->rp->r_pt[2],
			rd->rp->r_dir[0],
			rd->rp->r_dir[1],
			rd->rp->r_dir[2]);

	NMG_CK_MODEL(rd->rd_m);

	/* Caller has assured us that the ray intersects the nmg model,
	 * check ray for intersecion with rpp's of nmgregion's
	 */
	for ( RT_LIST_FOR(r_p, nmgregion, &rd->rd_m->r_hd) ) {
		NMG_CK_REGION(r_p);
		NMG_CK_REGION_A(r_p->ra_p);

		/* does ray intersect nmgregion rpp? */
		if (! rt_in_rpp(rd->rp, rd->rd_invdir,
		    r_p->ra_p->min_pt, r_p->ra_p->max_pt) )
			continue;

		/* ray intersects region, check shell intersection */
		for (RT_LIST_FOR(s_p, shell, &r_p->s_hd)) {
			nmg_isect_ray_shell(rd, s_p);
		}
	}
}
