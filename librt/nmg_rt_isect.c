/*
 *			N M G _ R T . C
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


static void print_hitlist(hl)
struct hitmiss *hl;
{
	struct hitmiss *a_hit;

	if (!(rt_g.NMG_debug & DEBUG_NMGRT))
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

	if (rt_g.NMG_debug & DEBUG_NMGRT)
		rt_log("hit_ins()\n");

	for (RT_LIST_FOR(a_hit, hitmiss, &rd->hitmiss[NMG_HIT_LIST]->l)) {
		if (newhit->hit.hit_dist < a_hit->hit.hit_dist)
			break;
		if (rt_g.NMG_debug & DEBUG_NMGRT)
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
	 
	if (RT_LIST_NOT_HEAD(a_hit, &rd->hitmiss[NMG_HIT_LIST]->l)) {
		if (rt_g.NMG_debug & DEBUG_NMGRT)
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
		if (rt_g.NMG_debug & DEBUG_NMGRT)
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
 *  Once it has been decided that the ray hits the vertex, this routine takes
 *  care of filling in the hitmiss structure and adding it to the list 
 *  of hit points.
 */
static struct hitmiss *
build_vertex_hit(rd, vu_p, myhit)
struct ray_data *rd;
struct vertexuse *vu_p;
struct hitmiss *myhit;
{
	vect_t v;

	if (rt_g.NMG_debug & DEBUG_NMGRT)
		rt_log("build_vertex_hit\n");

	if (rd->plane_pt) {
		/* hitting a vertex in a face/plane.  Get the distance
		 * in the plane to the intersection point.
		 */
		if (myhit->dist_in_plane < 0.0) {
			VSUB2(v, rd->plane_pt, vu_p->v_p->vg_p->coord);
			myhit->dist_in_plane = MAGNITUDE(v);
		}


		/* build the hit structure */
		myhit->hit.hit_dist = rd->ray_dist_to_plane;
		VMOVE(myhit->hit.hit_point, vu_p->v_p->vg_p->coord);
		myhit->hit.hit_private = (genptr_t) vu_p->v_p;

		RT_LIST_MAGIC_SET(&myhit->l, NMG_RT_HIT_MAGIC);
		hit_ins(rd, myhit);

		if (myhit->dist_in_plane < rd->plane_dist) {
			/* this is the closest element so far */
			if (rt_g.NMG_debug & DEBUG_NMGRT)
				rt_log("build_vertex_hit() found closer approach of %g\n", myhit->dist_in_plane);
			rd->plane_dist = myhit->dist_in_plane;
			rd->plane_closest = (long *)vu_p->v_p;
		}
		return myhit;
	}

	/* v = vector from ray point to hit vertex */
	VSUB2( v, rd->rp->r_pt, vu_p->v_p->vg_p->coord);
	myhit->hit.hit_dist = MAGNITUDE(v);	/* distance along ray */
	VMOVE(myhit->hit.hit_point, vu_p->v_p->vg_p->coord);
	myhit->hit.hit_private = (genptr_t) vu_p->v_p;

	RT_LIST_MAGIC_SET(&myhit->l, NMG_RT_HIT_MAGIC);
	hit_ins(rd, myhit);

	return myhit;
}

static struct hitmiss *
build_vertex_miss(rd, vu_p, myhit)
struct ray_data *rd;
struct vertexuse *vu_p;
struct hitmiss *myhit;
{
	vect_t v;

	if (rt_g.NMG_debug & DEBUG_NMGRT)
		rt_log("build_vertex_miss()\n");

	/* build up the hit structures */
	if (rd->plane_pt) {
		if (myhit->dist_in_plane < 0.0) {
		    	VSUB2(v, rd->plane_pt, vu_p->v_p->vg_p->coord);
		    	myhit->dist_in_plane = MAGNITUDE(v);
		}

		if (myhit->dist_in_plane < rd->plane_dist) {
			/* this is the closest hit so far */
			rd->plane_dist = myhit->dist_in_plane;
			rd->plane_closest = (long *)vu_p->v_p;

			if (rt_g.NMG_debug & DEBUG_NMGRT) {
				rt_log("build_vertex_miss() found closer approach of %g\n", myhit->dist_in_plane);
				rt_log("\tvertex %g %g %g is new \"closest to plane_pt\"\n",
					vu_p->v_p->vg_p->coord[0],
					vu_p->v_p->vg_p->coord[1],
					vu_p->v_p->vg_p->coord[2]);
			}
		}
	} else {
		if (rt_g.NMG_debug & DEBUG_NMGRT)
			rt_log("\tno plane point\n");
	}

	/* add myhit to the list of misses */
	RT_LIST_MAGIC_SET(&myhit->l, NMG_RT_MISS_MAGIC);
	RT_LIST_INSERT(&rd->hitmiss[NMG_MISS_LIST]->l, &myhit->l);

	return myhit;
}

/*
 *  The ray missed this vertex.  Build the appropriate miss structure, and
 *  calculate the distance from the ray to the vertex if needed.
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

	if (rt_g.NMG_debug & DEBUG_NMGRT) {
		rt_log("ray_miss_vertex(%g %g %g)\n",
			vu_p->v_p->vg_p->coord[0],
			vu_p->v_p->vg_p->coord[1],
			vu_p->v_p->vg_p->coord[2]);
	}

	if (myhit = NMG_INDEX_GET(rd->hitmiss, vu_p->v_p) ) {
		if (rt_g.NMG_debug & DEBUG_NMGRT)
			rt_log("ray_miss_vertex( vertex previously processed )\n");
		return(myhit);
	}

	GET_HITMISS(myhit);
	NMG_INDEX_ASSIGN(rd->hitmiss, vu_p->v_p, myhit);

	/* get build_vertex_miss() to compute this */
	myhit->dist_in_plane = -1.0;

	return (build_vertex_miss(rd, vu_p, myhit));
}



/*
 *  The ray hits the vertex.  Whether you know it or not, it really did hit
 *  this vertex.
 */
static struct hitmiss *
ray_hit_vertex(rd, vu_p)
struct ray_data *rd;
struct vertexuse *vu_p;
{
	struct hitmiss *myhit;

	if (rt_g.NMG_debug & DEBUG_NMGRT)
		rt_log("ray_hit_vertex\n");

	if (myhit = NMG_INDEX_GET(rd->hitmiss, vu_p->v_p) ) {
		if (RT_LIST_MAGIC_OK(&myhit->l, NMG_RT_HIT_MAGIC))
			return(myhit);

		/* oops, we have to change a MISS into a HIT */
		RT_LIST_DEQUEUE(&myhit->l);
	} else {
		GET_HITMISS(myhit);
		NMG_INDEX_ASSIGN(rd->hitmiss, vu_p->v_p, myhit);
	}

	/* build up the hit structures */
	return (build_vertex_hit(rd, vu_p, myhit));
}


/*	I S E C T _ R A Y _ V E R T E X U S E
 *
 *	Called in one of the following situations:
 *		1)	Zero length edge
 *		2)	Vertexuse child of Loopuse
 *		3)	Vertexuse child of Shell
 *
 *	return:
 *		pointer to struct hitlist associated with this vertex
 *
 */
static struct hitmiss *
isect_ray_vertexuse(rd, vu_p)
struct ray_data *rd;
struct vertexuse *vu_p;
{
	struct hitmiss *myhit;
	vect_t	v;

	if (rt_g.NMG_debug & DEBUG_NMGRT)
		rt_log("isect_ray_vertexuse\n");

	NMG_CK_VERTEXUSE(vu_p);
	NMG_CK_VERTEX(vu_p->v_p);
	NMG_CK_VERTEX_G(vu_p->v_p->vg_p);

	if (rt_g.NMG_debug & DEBUG_NMGRT)
		rt_log("nmg_isect_ray_vertexuse %g %g %g", 
			vu_p->v_p->vg_p->coord[0], 
			vu_p->v_p->vg_p->coord[1], 
			vu_p->v_p->vg_p->coord[2]);

	if (myhit = NMG_INDEX_GET(rd->hitmiss, vu_p->v_p) ) {
		/* we've previously processed this vertex */

		if (rt_g.NMG_debug & DEBUG_NMGRT)  {
			if (RT_LIST_MAGIC_OK(&myhit->l, NMG_RT_HIT_MAGIC))
				rt_log(" previously hit\n");
			else if (RT_LIST_MAGIC_OK(&myhit->l, NMG_RT_MISS_MAGIC))
				rt_log(" previously missed\n");
		}
		if (rd->plane_pt && myhit->dist_in_plane < rd->plane_dist) {
			/* this is a closer hit than anything else so far */
			if (rt_g.NMG_debug & DEBUG_NMGRT)
				rt_log("isect_ray_vertexuse() found closer approach of %g\n", myhit->dist_in_plane);
			rd->plane_dist = myhit->dist_in_plane;
			rd->plane_closest = (long *)vu_p->v_p;
		}
		return(myhit); /* previously hit vertex */
	}

	GET_HITMISS(myhit);
	NMG_INDEX_ASSIGN(rd->hitmiss, vu_p->v_p, myhit);

	if (rd->plane_pt) {
		/* we're intersecting a point in a face/plane */
	    	VSUB2(v, rd->plane_pt, vu_p->v_p->vg_p->coord);
	    	myhit->dist_in_plane = MAGNITUDE(v);
		if (myhit->dist_in_plane > rd->tol->dist ||
		    myhit->dist_in_plane < (0 - rd->tol->dist) ) {
			/* a hit on the vertex */
		    	build_vertex_hit(rd, vu_p, myhit);
		} else {
			/* a miss on the vertex */
			build_vertex_miss(rd, vu_p, myhit);
		}

		return(myhit);
	}

	/* we're intersecting a lone or wire vertex  */
	if (myhit->dist_in_plane < 0.0)
		myhit->dist_in_plane =
			rt_dist_line_point(rd->rp->r_pt, rd->rp->r_dir,
						vu_p->v_p->vg_p->coord);

	if (myhit->dist_in_plane > rd->tol->dist ||
	    myhit->dist_in_plane < (0-rd->tol->dist) ) {
		/* ray misses vertex */

	    	build_vertex_miss(rd, vu_p, myhit);
		return(myhit);
	}

	/* ray hits vertex, build the appropriate hit structure. */

	/* signal build_vertex_hit() to fill this in */
	myhit->dist_in_plane = -1.0; 

	build_vertex_hit(rd, vu_p, myhit);

	if (rt_g.NMG_debug & DEBUG_NMGRT) {
		rt_log(" Ray hits vertex, dist %g (%d/%d)\n",
			myhit->hit.hit_dist,
			(int)myhit->hit.hit_private,
			vu_p->v_p->magic);

		print_hitlist(rd->hitmiss[NMG_HIT_LIST]);
	}

	return(myhit);
}




static struct hitmiss *
colinear_edge_ray(rd, eu_p)
struct ray_data *rd;
struct edgeuse *eu_p;
{
	struct hitmiss *vhit1, *vhit2, *myhit;

	if (rt_g.NMG_debug & DEBUG_NMGRT)
		rt_log("\t - colinear_edge_ray\n");

	ray_hit_vertex(rd, eu_p->vu_p);
	ray_hit_vertex(rd, eu_p->eumate_p->vu_p);


	vhit1 = NMG_INDEX_GET(rd->hitmiss, eu_p->vu_p->v_p);
	vhit2 = NMG_INDEX_GET(rd->hitmiss, eu_p->eumate_p->vu_p->v_p);

	/* let's remember that this should be an in/out segment */
	if (vhit1->hit.hit_dist > vhit2->hit.hit_dist) {
		vhit1->in_out = NMG_HITMISS_SEG_OUT;
		vhit2->in_out = NMG_HITMISS_SEG_IN;
	} else {
		vhit1->in_out = NMG_HITMISS_SEG_IN;
		vhit2->in_out = NMG_HITMISS_SEG_OUT;
	}
	vhit1->other = vhit2;
	vhit2->other = vhit1;


	GET_HITMISS(myhit);
	NMG_INDEX_ASSIGN(rd->hitmiss, eu_p->e_p, myhit);

	if (rd->plane_pt) {
		if (vhit1->dist_in_plane > vhit2->dist_in_plane)
			myhit->dist_in_plane = vhit2->dist_in_plane;
		else
			myhit->dist_in_plane = vhit1->dist_in_plane;
	}

	RT_LIST_MAGIC_SET(&myhit->l, NMG_RT_HIT_SUB_MAGIC);
	RT_LIST_INSERT(&rd->hitmiss[NMG_MISS_LIST]->l, &myhit->l);
	return(myhit);
}

/*
 *  When a vertex at an end of an edge gets hit by the ray, this routine
 *  is called to build the hit structures for the vertex and the edge.
 */
static struct hitmiss *
hit_edge_vertex(rd, eu_p, vu_p)
struct ray_data *rd;
struct edgeuse *eu_p;
struct vertexuse *vu_p;
{
	struct hitmiss *vhit, *myhit;

	if (rt_g.NMG_debug & DEBUG_NMGRT)
		rt_log("hit_edge_vertex\n");

	vhit = ray_hit_vertex(rd, vu_p);

	GET_HITMISS(myhit);
	NMG_INDEX_ASSIGN(rd->hitmiss, eu_p->e_p, myhit);

	RT_LIST_MAGIC_SET(&myhit->l, NMG_RT_HIT_SUB_MAGIC);
	RT_LIST_INSERT(&rd->hitmiss[NMG_MISS_LIST]->l, &myhit->l);
	return(myhit);
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
	double dist_along_ray;
	struct hitmiss *myhit, *vhit1, *vhit2;

	NMG_CK_EDGEUSE(eu_p);
	NMG_CK_EDGEUSE(eu_p->eumate_p);
	NMG_CK_EDGE(eu_p->e_p);
	NMG_CK_VERTEXUSE(eu_p->vu_p);
	NMG_CK_VERTEXUSE(eu_p->eumate_p->vu_p);
	NMG_CK_VERTEX(eu_p->vu_p->v_p);
	NMG_CK_VERTEX(eu_p->eumate_p->vu_p->v_p);

	if (rt_g.NMG_debug & DEBUG_NMGRT) {
		rt_log("isect_ray_edgeuse (%g %g %g) -> (%g %g %g)\n",
			eu_p->vu_p->v_p->vg_p->coord[0],
			eu_p->vu_p->v_p->vg_p->coord[1],
			eu_p->vu_p->v_p->vg_p->coord[2],
			eu_p->eumate_p->vu_p->v_p->vg_p->coord[0],
			eu_p->eumate_p->vu_p->v_p->vg_p->coord[1],
			eu_p->eumate_p->vu_p->v_p->vg_p->coord[2]);
		rt_log("\tplane_pt (%g %g %g) plane_dist %g\n", 
			rd->plane_pt[0],
			rd->plane_pt[1],
			rd->plane_pt[2],
			rd->plane_dist); 
	}

	if ( myhit = NMG_INDEX_GET(rd->hitmiss, eu_p->e_p) ) {
		/* previously processed vertex or edge */
		if (rt_g.NMG_debug & DEBUG_NMGRT)
			if (RT_LIST_MAGIC_OK(&myhit->l, NMG_RT_HIT_MAGIC) )
				rt_log("\tedge previously hit\n");
			else
				rt_log("\tedge previously missed\n");
		return;
	}


	status = rt_isect_line_lseg( &dist_along_ray,
			rd->rp->r_pt, rd->rp->r_dir, 
			eu_p->vu_p->v_p->vg_p->coord,
			eu_p->eumate_p->vu_p->v_p->vg_p->coord,
			rd->tol);

	switch (status) {
	case -4 :
		/* Zero length edge.  The routine rt_isect_line_lseg() can't
		 * help us.  Intersect the ray with a vertex.  If it hits, 
		 * then this edge is a "sub-hit".  If it misses then both 
		 * the vertex and the edge are missed by the ray.
		 *
		 * Note.  There is no need to see if this edge (its verticies
		 * really) was the closest item to the hit point in the face.
		 * That will be handled by isect_ray_vertexuse() in this case.
		 */

		if (rt_g.NMG_debug & DEBUG_NMGRT)
			rt_log("\t - Zero length edge\n");

		/* create hit structure for this edge */
		GET_HITMISS(myhit);
		NMG_INDEX_ASSIGN(rd->hitmiss, eu_p->e_p, myhit);

		vhit1 = isect_ray_vertexuse(rd->rp, eu_p->vu_p);
		vhit2 = isect_ray_vertexuse(rd->rp, eu_p->eumate_p->vu_p);
		if (rd->plane_pt) {
			if (vhit1->dist_in_plane < vhit2->dist_in_plane)
				myhit->dist_in_plane = vhit1->dist_in_plane;
			else
				myhit->dist_in_plane = vhit2->dist_in_plane;
		}

		if (RT_LIST_MAGIC_OK(&vhit1->l, NMG_RT_HIT_MAGIC) ||
		    RT_LIST_MAGIC_OK(&vhit2->l, NMG_RT_HIT_MAGIC) ) {
			/* we hit the vertex */
			RT_LIST_MAGIC_SET(&myhit->l, NMG_RT_HIT_SUB_MAGIC);
		} else {
			/* both vertecies were missed, so edge is missed */
			RT_LIST_MAGIC_SET(&myhit->l, NMG_RT_MISS_MAGIC);
		}
		RT_LIST_INSERT(&rd->hitmiss[NMG_MISS_LIST]->l, &myhit->l);
		break;
	case -3 :	/* fallthrough */
	case -2 :
		/* The ray misses the edge segment, but hits the infinite
		 * line of the edge to one end of the edge segment.  This
		 * is an exercise in tabulating the nature of the miss.
		 */

		if (rt_g.NMG_debug & DEBUG_NMGRT)
			rt_log("\t - Miss edge, \"hit\" line\n");
		/* record the fact that we missed each vertex */
		(void)ray_miss_vertex(rd, eu_p->vu_p);
		(void)ray_miss_vertex(rd, eu_p->eumate_p->vu_p);

		/* record the fact that we missed the edge */
		GET_HITMISS(myhit);
		NMG_INDEX_ASSIGN(rd->hitmiss, eu_p->e_p, myhit);

		RT_LIST_MAGIC_SET(&myhit->l, NMG_RT_MISS_MAGIC);
		RT_LIST_INSERT(&rd->hitmiss[NMG_MISS_LIST]->l, &myhit->l);

		break;
	case -1 :
		if (rt_g.NMG_debug & DEBUG_NMGRT)
			rt_log("\t - Miss edge/line\n");

		ray_miss_vertex(rd, eu_p->vu_p);
		ray_miss_vertex(rd, eu_p->eumate_p->vu_p);

		GET_HITMISS(myhit);
		NMG_INDEX_ASSIGN(rd->hitmiss, eu_p->e_p, myhit);

		RT_LIST_MAGIC_SET(&myhit->l, NMG_RT_MISS_MAGIC);
		RT_LIST_INSERT(&rd->hitmiss[NMG_MISS_LIST]->l, &myhit->l);

		break;
	case 0 :  /* oh joy.  Lines are co-linear */
		myhit = colinear_edge_ray(rd, eu_p);
		break;
	case 1 :
		myhit = hit_edge_vertex(rd, eu_p, eu_p->vu_p);
		break;
	case 2 :
		myhit = hit_edge_vertex(rd, eu_p, eu_p->eumate_p->vu_p);
		break;
	case 3 : /* a hit on an edge */
		if (rt_g.NMG_debug & DEBUG_NMGRT)
			rt_log("\t - HIT edge\n");

		/* create hit structure for this edge */
		GET_HITMISS(myhit);
		NMG_INDEX_ASSIGN(rd->hitmiss, eu_p->e_p, myhit);

		/* Since this is a ray/edge intersection, we don't consider
		 * this hit in the "nearest hit" calculations for a faceuse
		 * hit.  That comes later with the edge/hitpoint
		 * consideration.
		 */
		myhit->dist_in_plane = 0.0;

		/* build the hit structure */
		myhit->hit.hit_dist = dist_along_ray;
		VJOIN1(myhit->hit.hit_point, 
				rd->rp->r_pt, dist_along_ray, rd->rp->r_dir);
		myhit->hit.hit_private = (genptr_t) eu_p->e_p;

		hit_ins(rd, myhit);
		break;
	}
}

static void
dist_vu_plane_pt(rd, vu_p)
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
		if (rt_g.NMG_debug & DEBUG_NMGRT)
			rt_log("Vertex (%g %g %g) is new closest\n",
				vu_p->v_p->vg_p->coord[0],
				vu_p->v_p->vg_p->coord[1],
				vu_p->v_p->vg_p->coord[2]);
		rd->plane_dist = dist;
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

	if (rt_g.NMG_debug & DEBUG_NMGRT) {
		rt_log("dist_plane_pt_eu((%g %g %g), 0x%08x(%g %g %g -> %g %g %g)\n",
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
		rt_log("\tplane_dist = %g\n", rd->plane_dist);
	}
	/* Determine if the plane intercept is closer to this edgeuse than
	 * the previously closest edge/vertex
	 */
	status = rt_dist_pt3_lseg3(&dist, pca,
		eu_p->vu_p->v_p->vg_p->coord,
		eu_p->eumate_p->vu_p->v_p->vg_p->coord, rd->plane_pt,
		rd->tol);

	switch (status) {
	case 0: /* we hit the edge(use) with the plane_pt,
		  store edgeuse ptr in plane_closest, set plane_dist = 0.0 */
		if (dist < rd->plane_dist) {
			rd->plane_dist = 0.0;
			rd->plane_closest = &eu_p->l.magic;
			if (rt_g.NMG_debug & DEBUG_NMGRT)
				rt_log("\teu is new \"closest to plane_pt\" (new dist: %g)\n",
					dist);
		}
		/* make sure we have a hit on this edge */
		break;
	case 1:	/* within tolerance of endpoint at eu_p->vu_p.
		   store vertexuse ptr in plane_closest,
		   set plane_dist = 0.0 */
		if (dist < rd->plane_dist) {
			rd->plane_dist = 0.0;
			rd->plane_closest = &eu_p->vu_p->l.magic;
			if (rt_g.NMG_debug & DEBUG_NMGRT)
				rt_log("\tvu of eu is new \"closest to plane_pt\" (new dist %g)\n", dist);
		}
		/* make sure we have a hit on the vertex */
		break;
	case 2:	/* within tolerance of endpoint at eu_p->eumate_p
		   store vertexuse ptr (eu->next) in plane_closest,
		   set plane_dist = 0.0 */
		if (dist < rd->plane_dist) {
			eu = RT_LIST_PNEXT_CIRC(edgeuse, &(eu_p->l));
			rd->plane_dist = 0.0;
			rd->plane_closest = &eu->vu_p->l.magic;
			if (rt_g.NMG_debug & DEBUG_NMGRT)
				rt_log("\tvu of next(eu) is new \"closest to plane_pt\" (new dist %g)\n", dist);
		}
		/* make sure we have a hit on the vertex */
		break;
	case 3: /* PCA of pt on line is "before" eu_p->vu_p of seg */
		if (dist < rd->plane_dist) {
			rd->plane_dist = dist;
			rd->plane_closest = &eu_p->vu_p->l.magic;
			if (rt_g.NMG_debug & DEBUG_NMGRT)
				rt_log("\tvu of eu is new \"closest to plane_pt\" (new dist %g)\n", dist);
		}
		break;
	case 4: /* PCA of pt on line is "after" eu_p->eumate_p->vu_p of seg */
		if (dist < rd->plane_dist) {
			eu = RT_LIST_PNEXT_CIRC(edgeuse, &(eu_p->l));
			rd->plane_dist = dist;
			rd->plane_closest = &eu->vu_p->l.magic;
			if (rt_g.NMG_debug & DEBUG_NMGRT)
				rt_log("\tvu of next(eu) is new \"closest to plane_pt\" (new dist %g)\n", dist);
		}
		break;
	case 5: /* if edge is closer to plane_pt than any previous item,
		   store edgeuse ptr in plane_closest and set plane_dist */
		if (dist < rd->plane_dist) {
			rd->plane_dist = dist;
			rd->plane_closest = &eu_p->l.magic;
			if (rt_g.NMG_debug & DEBUG_NMGRT)
				rt_log("\teu is new \"closest to plane_pt\" (new dist %g)\n", dist);
		}
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

	if (rt_g.NMG_debug & DEBUG_NMGRT)
		rt_log("isect_ray_loopuse( 0x%08x, loop:0x%08x)\n", rd, lu_p->l_p);

	NMG_CK_LOOPUSE(lu_p);
	NMG_CK_LOOP(lu_p->l_p);
	NMG_CK_LOOP_G(lu_p->l_p->lg_p);

	if (RT_LIST_FIRST_MAGIC(&lu_p->down_hd) == NMG_EDGEUSE_MAGIC) {
		if (rd->plane_pt) {
			for (RT_LIST_FOR(eu_p, edgeuse, &lu_p->down_hd)) {
				isect_ray_edgeuse(rd, eu_p);
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
		rt_bomb(" bad loopuse child magic");
	}

	/* loopuse child is vertexuse */

	(void) isect_ray_vertexuse(rd,
		RT_LIST_FIRST(vertexuse, &lu_p->down_hd));
	if (rd->plane_pt)
		dist_vu_plane_pt(rd,
			RT_LIST_FIRST(vertexuse, &lu_p->down_hd));
}


static void
face_closest_is_eu(rd, fu_p, eu)
struct ray_data *rd;
struct faceuse *fu_p;
struct edgeuse *eu;
{
	struct edgeuse *eu_p;
	struct faceuse *nmg_find_fu_of_eu();
	struct hitmiss *hit;
	int mate_rad = 0;
	vect_t eu_v, left, pt_v;

	if (rt_g.NMG_debug & DEBUG_NMGRT)
		rt_log("face_closest_is_eu()\n");

	NMG_CK_EDGEUSE(eu);

	/* if there is a hit on the edge which is "closest" to the plane
	 * intercept point, then we do not add a hit point for the face
	 */
	hit = NMG_INDEX_GET(rd->hitmiss, eu->e_p);
	if (RT_LIST_MAGIC_OK(&hit->l, NMG_RT_HIT_MAGIC)) return;


	/* There is no hit on the closest edge, so we must determine
	 * whether or not the hit point is "inside" or "outside" the surface
	 * area of the face
	 */
	if (fu_p->orientation == OT_OPPOSITE) {
		if (rt_g.NMG_debug & DEBUG_NMGRT)
			rt_log("switching faceuses\n");
		fu_p = fu_p->fumate_p;
	}

	/* if eu isn't in this faceuse, go find a use of this edge
	 * that *IS* in this faceuse
	 */
	eu_p = eu;
	while (nmg_find_fu_of_eu(eu_p) != fu_p) {

		if (rt_g.NMG_debug & DEBUG_NMGRT)
			rt_log("face_closest_is_eu( edgeuse 0x%08x wasn't in faceuse )\n", eu_p);

		if (mate_rad) eu_p = eu_p->eumate_p;
		else eu_p = eu_p->radial_p;
		mate_rad = !mate_rad;

		if (eu_p == eu) {
			rt_bomb("couldn't find faceuse as parent of edge\n");
		}
	}

	/* if plane intersect is on the face-surface side of the edge then
	 * we need a hit point on the face.
	 */

	/* XXX */

	GET_HITMISS(hit);
	NMG_INDEX_ASSIGN(rd->hitmiss, fu_p->f_p, hit);


	VSUB2(eu_v,eu_p->eumate_p->vu_p->v_p->vg_p->coord,
		eu_p->vu_p->v_p->vg_p->coord);

	if (rt_g.NMG_debug & DEBUG_NMGRT) {
		VPRINT("\teu", eu_p->vu_p->v_p->vg_p->coord);
		VPRINT("\teumate", eu_p->eumate_p->vu_p->v_p->vg_p->coord);
		VPRINT("\tpt", rd->plane_pt);
		HPRINT("\tN", fu_p->f_p->fg_p->N);
		VPRINT("\teu_v", eu_v);
	}
	VCROSS(left, fu_p->f_p->fg_p->N, eu_v);
	VSUB2(pt_v, rd->plane_pt, eu_p->vu_p->v_p->vg_p->coord);

	if (rt_g.NMG_debug & DEBUG_NMGRT) {
		VPRINT("\tleft", left);
		VPRINT("\tpt_v", pt_v);
	}

	if (VDOT(left, pt_v) < 0.0) {
		if (rt_g.NMG_debug & DEBUG_NMGRT)
			rt_log("face_closest_is_eu( plane hit outside face )\n");
		RT_LIST_MAGIC_SET(&hit->l, NMG_RT_MISS_MAGIC);
		RT_LIST_INSERT(&rd->hitmiss[NMG_MISS_LIST]->l, &hit->l);
		return;
	}

	if (rt_g.NMG_debug & DEBUG_NMGRT)
		rt_log("face_closest_is_eu( New hit on face )\n");

	hit->dist_in_plane = 0.0;

	VMOVE(hit->hit.hit_point, rd->plane_pt);
	hit->hit.hit_dist = rd->ray_dist_to_plane;
	hit->hit.hit_private = (genptr_t) fu_p->f_p;

	/* add hit to list of hit points */
	hit_ins(rd, hit);
}

static void
face_closest_is_vu(rd, fu_p, vu)
struct ray_data *rd;
struct faceuse *fu_p;
struct vertexuse *vu;
{
	struct vertexuse *vu_p;
	struct faceuse *nmg_find_fu_of_vu();
	struct hitmiss *hit;
	struct faceuse *fu;

	if (rt_g.NMG_debug & DEBUG_NMGRT)
		rt_log("face_closest_is_vu\n");

	hit = NMG_INDEX_GET(rd->hitmiss, vu->v_p);
	if (RT_LIST_MAGIC_OK(&hit->l, NMG_RT_HIT_MAGIC)) return;

	/* if vu isn't in this faceuse, go find a use of this vertex
	 * that *IS* in this faceuse.
	 */
	fu = nmg_find_fu_of_vu(vu);		
	if (fu != fu_p) {
		for (RT_LIST_FOR(vu_p, vertexuse, &vu->v_p->vu_hd)) {
			if (nmg_find_fu_of_vu(vu) == fu_p)
				goto got_fu;
		}
		rt_bomb("didn't find faceuse from plane_closest vertex\n");
		got_fu:
		vu = vu_p;
	}

	/* XXX */

	/* if parent is OT_SAME loopuse, we've missed face */
	/* if parent is OT_OPPOSITE loopuse, we've hit face */
	/* if parent is edgeuse, do left/right computation */
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

	if (rt_g.NMG_debug & DEBUG_NMGRT)
		rt_log("isect_ray_faceuse(0x%08x, face:0x%08x)", rd, fu_p->f_p);

	NMG_CK_FACEUSE(fu_p);
	NMG_CK_FACEUSE(fu_p->fumate_p);
	NMG_CK_FACE(fu_p->f_p);
	NMG_CK_FACE_G(fu_p->f_p->fg_p);

	/* if this face already processed, we are done. */
	if (myhit = NMG_INDEX_GET(rd->hitmiss, fu_p->f_p) ) {
		if (rt_g.NMG_debug & DEBUG_NMGRT)
			rt_log(" previously processed \n");
		return;
	}


	/* bounding box intersection */
	if (!rt_in_rpp(rd->rp, rd->invdir,
	    fu_p->f_p->fg_p->min_pt, fu_p->f_p->fg_p->max_pt) ) {
		GET_HITMISS(myhit);
		NMG_INDEX_ASSIGN(rd->hitmiss, fu_p->f_p, myhit);
		RT_LIST_MAGIC_SET(&myhit->l, NMG_RT_MISS_MAGIC);
		RT_LIST_INSERT(&rd->hitmiss[NMG_MISS_LIST]->l, &myhit->l);
		if (rt_g.NMG_debug & DEBUG_NMGRT)
			rt_log("missed bounding box\n");
		return;
	}

	if (rt_g.NMG_debug & DEBUG_NMGRT) rt_log(" hit bounding box \n");

	code = rt_isect_line3_plane(&dist, rd->rp->r_pt, rd->rp->r_dir,
			fu_p->f_p->fg_p->N, rd->tol);

	if (code < 0) {
		/* ray misses halfspace, but intersected bounding box. */
		if (rt_g.NMG_debug & DEBUG_NMGRT)
			rt_log("\tray misses halfspace of plane\n");

		GET_HITMISS(myhit);
		NMG_INDEX_ASSIGN(rd->hitmiss, fu_p->f_p, myhit);
		RT_LIST_MAGIC_SET(&myhit->l, NMG_RT_MISS_MAGIC);
		RT_LIST_INSERT(&rd->hitmiss[NMG_MISS_LIST]->l, &myhit->l);
		return;
	} else if (code == 0) {
		/* XXX gack!  ray lies in plane.  
		 * In leiu of doing 2D intersection we define such rays
		 * as "missing" the face.  We rely on other faces to generate
		 * hit points.
		 */
		rt_log("\tWarning:  Ignoring ray in plane of face\n");
		GET_HITMISS(myhit);
		NMG_INDEX_ASSIGN(rd->hitmiss, fu_p->f_p, myhit);
		RT_LIST_MAGIC_SET(&myhit->l, NMG_RT_MISS_MAGIC);
		RT_LIST_INSERT(&rd->hitmiss[NMG_MISS_LIST]->l, &myhit->l);
		return;
	}

	/* ray hits plane */

	/* project point into plane of face */
	VJOIN1(plane_pt, rd->rp->r_pt, dist, rd->rp->r_dir);
	rd->plane_pt = plane_pt;
	rd->plane_dist = MAX_FASTF;
	rd->ray_dist_to_plane = dist;

	if (rt_g.NMG_debug & DEBUG_NMGRT) {
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
	


	/* check each loopuse in the faceuse to determine hit/miss */
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
		rt_bomb("Hello.  My $NAME is Indigo Montoya.  You killed my process.  Prepare to vi!\n");
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

	if (rt_g.NMG_debug & DEBUG_NMGRT)
		rt_log("nmg_isect_ray_shell( 0x%08x, 0x%08x)\n", rd, s_p);

	NMG_CK_SHELL(s_p);
	NMG_CK_SHELL_A(s_p->sa_p);

	/* does ray isect shell rpp ?
	 * if not, we can just return.  there is no need to record the
	 * miss for the shell, as there is only one "use" of a shell.
	 */
	if (!rt_in_rpp(rd->rp, rd->invdir,
	    s_p->sa_p->min_pt, s_p->sa_p->max_pt) ) {
		if (rt_g.NMG_debug & DEBUG_NMGRT)
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

	if (rt_g.NMG_debug & DEBUG_NMGRT)
		rt_log("nmg_isect_ray_shell( done )\n", rd, s_p);
}


/*	N M G _ I S E C T _ R A Y
 *
 */
struct hitmiss *
nmg_isect_ray_model(rp, invdir, m, tol)
struct xray *rp;
vect_t invdir;
struct model *m;
struct rt_tol	*tol;
{
	struct nmgregion *r_p;
	struct shell *s_p;
	struct hitmiss *hm;
	struct ray_data rd;

	if (rt_g.NMG_debug & DEBUG_NMGRT)
		rt_log("isect_ray_nmg: Pnt(%g %g %g) Dir(%g %g %g)\n", 
			rp->r_pt[0],
			rp->r_pt[1],
			rp->r_pt[2],
			rp->r_dir[0],
			rp->r_dir[1],
			rp->r_dir[2]);

	NMG_CK_MODEL(m);

	/* create a table to keep track of which elements have been
	 * processed before and which haven't.  Elements in this table
	 * will either be:
	 *		(NULL)		item not previously processed
	 *		hitmiss ptr	item previously processed
	 *
	 * the 0th item in the array is a pointer to the head of the "hit"
	 * list.  The 1th item in the array is a pointer to the head of the
	 * "miss" list.
	 */
	rd.hitmiss = (struct hitmiss **)rt_calloc( m->maxindex, sizeof(struct hitmiss *),
		"nmg geom hit list");

	/* allocate the head of the linked lists for hit and miss lists */
	hm = (struct hitmiss *)rt_calloc(2, sizeof(struct hitmiss),
		"hitlist calloc isect ray nmg");

	RT_LIST_INIT(&(hm->l));
	rd.hitmiss[NMG_HIT_LIST] = hm++;

	RT_LIST_INIT(&(hm->l));
	rd.hitmiss[NMG_MISS_LIST] = hm;
	VMOVE(rd.invdir, invdir);
	rd.tol = tol;

	rd.rp = rp;

	/* ray intersects model, check ray for intersecion with rpp's of
	 * nmgregion's
	 */
	for ( RT_LIST_FOR(r_p, nmgregion, &m->r_hd) ) {
		NMG_CK_REGION(r_p);
		NMG_CK_REGION_A(r_p->ra_p);

		/* does ray intersect nmgregion rpp? */
		if (! rt_in_rpp(rd.rp, rd.invdir,
		    r_p->ra_p->min_pt, r_p->ra_p->max_pt) )
			continue;

		/* ray intersects region, check shell intersection */
		for (RT_LIST_FOR(s_p, shell, &r_p->s_hd)) {
			nmg_isect_ray_shell(&rd, s_p);
		}
	}

	/* free the list of missed elements */
	while (RT_LIST_WHILE(hm, hitmiss, &rd.hitmiss[NMG_MISS_LIST]->l)) {
		RT_LIST_DEQUEUE( &(hm->l) );
		rt_free((char *)hm, "missed hitmiss structure");
	}

	/* if there weren't any hit points then 
	 *   free the list head and return a NULL pointer
	 */
	hm = rd.hitmiss[NMG_HIT_LIST];
	if (RT_LIST_IS_EMPTY(&hm->l)) {
		if (rt_g.NMG_debug & DEBUG_NMGRT)
			rt_log("isect_ray_nmg: No elements in hitlist\n");

		rt_free((char *)rd.hitmiss[NMG_HIT_LIST],
			"free empty hit list");
		hm = (struct hitmiss *)NULL;
	}

	rt_free((char *)rd.hitmiss, "free nmg geom hit list");

	return(hm);
}
