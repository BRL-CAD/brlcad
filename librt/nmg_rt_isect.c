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
#include <math.h>
#include "machine.h"
#include "vmath.h"
#include "nmg.h"
#include "raytrace.h"
#include "./nmg_rt.h"

static void 	vertex_neighborhood RT_ARGS((struct ray_data *rd, struct vertexuse *vu_p, struct hitmiss *myhit));
RT_EXTERN(void	nmg_isect_ray_model, (struct ray_data *rd));

/* Plot a faceuse and a line between pt and plane_pt */
static void
plfu( fu, pt, plane_pt )
struct faceuse *fu;
point_t pt;
point_t plane_pt;
{
	static int file_number=0;
	FILE *fd;
	char name[25];
	char buf[80];
	long *b;
	struct loopuse *lu;
	struct edgeuse *eu;
	struct vertexuse *vu;

	NMG_CK_FACEUSE(fu);
	
	sprintf(name, "ray%02d.pl", file_number++);
	if ((fd=fopen(name, "w")) == (FILE *)NULL) {
		perror(name);
		abort();
	}

	rt_log("\tplotting %s\n", name);
	b = (long *)rt_calloc( fu->s_p->r_p->m_p->maxindex,
		sizeof(long), "bit vec"),

	pl_erase(fd);
	pd_3space(fd,
		fu->f_p->min_pt[0]-1.0,
		fu->f_p->min_pt[1]-1.0,
		fu->f_p->min_pt[2]-1.0,
		fu->f_p->max_pt[0]+1.0,
		fu->f_p->max_pt[1]+1.0,
		fu->f_p->max_pt[2]+1.0);
	
	nmg_pl_fu(fd, fu, b, 255, 255, 255);

	pl_color(fd, 255, 50, 50);
	pdv_3line(fd, pt, plane_pt);

	rt_free((char *)b, "bit vec");
	fclose(fd);
}


nmg_rt_print_hitmiss(a_hit)
struct hitmiss *a_hit;
{
	NMG_CK_HITMISS(a_hit);
	rt_log("   dist:%12g pt(%g %g %g) %s state:",
		a_hit->hit.hit_dist,
		a_hit->hit.hit_point[0],
		a_hit->hit.hit_point[1],
		a_hit->hit.hit_point[2],
		rt_identify_magic(*(int*)a_hit->hit.hit_private) );

	switch(a_hit->in_out) {
	case HMG_HIT_IN_IN:
		rt_log("IN_IN"); break;
	case HMG_HIT_IN_OUT:
		rt_log("IN_OUT"); break;
	case HMG_HIT_OUT_IN:
		rt_log("OUT_IN"); break;
	case HMG_HIT_OUT_OUT:
		rt_log("OUT_OUT"); break;
	case HMG_HIT_IN_ON:
		rt_log("IN_ON"); break;
	case HMG_HIT_ON_IN:
		rt_log("ON_IN"); break;
	case HMG_HIT_OUT_ON:
		rt_log("OUT_ON"); break;
	case HMG_HIT_ON_OUT:
		rt_log("ON_OUT"); break;
	default:
		rt_log("?_?\n"); break;
	}

	switch (a_hit->start_stop) {
	case NMG_HITMISS_SEG_IN:	rt_log(" SEG_START"); break;
	case NMG_HITMISS_SEG_OUT:	rt_log(" SEG_STOP"); break;
	}
	rt_log("\n");

}
static void print_hitlist(hl)
struct hitmiss *hl;
{
	struct hitmiss *a_hit;

	if (!(rt_g.NMG_debug & DEBUG_RT_ISECT))
		return;

	rt_log("nmg/ray hit list\n\n");
f
	for (RT_LIST_FOR(a_hit, hitmiss, &(hl->l))) {
		nmg_rt_print_hitmiss(a_hit);
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
 *
 *	N       R
 *	^      ^
 *	|     /
 *	|    / |
 *	|   /  | -NdotR (N)
 *	|  /   |
 *	| /    |
 *	|/     V
 *	.----->Q
 *
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
	vect_t		n;
	struct nmg_ptbl ftbl;
	double cos_angle;
	hvect_t norm;
	point_t	North_plane_pt, South_plane_pt, South_Pole, North_Pole;

	NMG_CK_VERTEXUSE(vu_p);
	NMG_CK_VERTEX(vu_p->v_p);
	NMG_CK_VERTEX_G(vu_p->v_p->vg_p);

	(void)nmg_tbl( &ftbl, TBL_INIT, (long *)NULL );

	/* for every use of this vertex */
	for ( RT_LIST_FOR(vu, vertexuse, &vu_p->v_p->vu_hd) ) {
		/* if the parent use is an (edge/loop)use of an 
		 * OT_SAME faceuse that we haven't already processed...
		 */
		if (*vu->up.magic_p == NMG_EDGEUSE_MAGIC &&
		    *vu->up.eu_p->up.magic_p == NMG_LOOPUSE_MAGIC &&
		    *vu->up.eu_p->up.lu_p->up.magic_p == NMG_FACEUSE_MAGIC &&
		    vu->up.eu_p->up.lu_p->up.fu_p->orientation == OT_SAME &&
		    nmg_tbl(&ftbl, TBL_INS_UNIQUE,
		    &vu->up.eu_p->up.lu_p->up.fu_p->l.magic) < 0 ) {

		    	/* There is a conceptual sphere around the vertex
		    	 * The point where the ray enters this sphere is
		    	 *  called the North Pole, and the point where it
		    	 *  exits is called the South Pole.
		    	 *
		    	 * The distance from each "Pole Point" to the faceuse
		    	 *  is the commodity in which we are interested.
		    	 * This is either the distance to the plane (if the
		    	 * projection of the point into the plane lies within
		    	 * the faceuse), or the magnitude of the sum of the
		    	 * vectors point->plane and plane_pt->faceuse boundary
		    	 */


			fu = vu->up.eu_p->up.lu_p->up.fu_p;
			/* check this face */
			NMG_CK_FACE(fu->f_p);
			NMG_CK_FACE_G(fu->f_p->fg_p);
		    	NMG_GET_FU_NORMAL( norm, fu );



		    	/* compute the "north pole" points
		    	 *
		    	 * project the point a unit distance BACK along the
		    	 * ray and then project the point "back down" onto
		    	 * the plane.
		    	 */
			VJOIN1(North_Pole, vu_p->v_p->vg_p->coord,
				-1.0, rd->rp->r_dir);
		    	VJOIN1(North_plane_pt, North_Pole, cos_angle, norm);


		    	/* compute the "south pole" points
		    	 *
		    	 * project the point a unit distance FORWARD along the
		    	 * ray, and then project the point down "back up"
		    	 * onto the plane.
		    	 */
			VADD2(South_Pole, vu_p->v_p->vg_p->coord,
				rd->rp->r_dir);
		    	VJOIN1(South_plane_pt, South_Pole, -cos_angle, norm);

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

	NMG_CK_HITMISS(myhit);

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
 *	Compute the "ray state" before and after the ray encounters a
 *	hit-point on an edge.
 */
static int
edge_hit_ray_state(rd, eu)
struct ray_data *rd;
struct edgeuse *eu;
{
	double cos_angle;
	double min_cos_angle = 2.0;
	double max_cos_angle = -1.0;
	struct faceuse *fu;
	struct faceuse *min_fu;
	struct faceuse *max_fu;
	int in_out = 0;
	struct edgeuse *eu_p;
	vect_t edge_left;
	vect_t norm;

	if (rt_g.NMG_debug & DEBUG_RT_ISECT) {
		eu_p = RT_LIST_PNEXT_CIRC(edgeuse, eu);
		rt_log("edge_hit_ray_state(%g %g %g -> %g %g %g _vs_ %g %g %g)\n",
			eu->vu_p->v_p->vg_p->coord[0],
			eu->vu_p->v_p->vg_p->coord[1],
			eu->vu_p->v_p->vg_p->coord[2],
			eu_p->vu_p->v_p->vg_p->coord[0],
			eu_p->vu_p->v_p->vg_p->coord[1],
			eu_p->vu_p->v_p->vg_p->coord[2],
			rd->rp->r_dir[0],
			rd->rp->r_dir[1],
			rd->rp->r_dir[2]);
	}

	eu_p = eu->e_p->eu_p;
	do {
		if (fu=nmg_find_fu_of_eu(eu_p)) {
			if (fu->orientation == OT_OPPOSITE &&
			    fu->fumate_p->orientation == OT_SAME)
				fu = fu->fumate_p;

			if (fu->orientation != OT_SAME) {
				rt_log("%s[%d]: I can't seem to find an OT_SAME faceuse\nThis must be a `dangling' face  I'll skip it\n", __FILE__, __LINE__);
				goto next_edgeuse;
			}

			if (nmg_find_eu_leftvec(edge_left, eu_p)) {
				rt_log("edgeuse not part of faceuse");
				goto next_edgeuse;
			}

			if (! (NMG_3MANIFOLD & 
			       NMG_MANIFOLDS(rd->rd_m->manifolds, fu->f_p)) ){
				rt_log("This is not a 3-Manifold face.  I'll skip it\n");
				goto next_edgeuse;
			}

			cos_angle = VDOT(edge_left, rd->rp->r_dir);

			if (rt_g.NMG_debug & DEBUG_RT_ISECT)
				rt_log("left_vect:(%g %g %g)  cos_angle:%g\n",
					edge_left[0], edge_left[1],
					edge_left[2], cos_angle);

			if (cos_angle < min_cos_angle) {
				min_cos_angle = cos_angle;
				min_fu = fu;
				rt_log("New min cos_angle %g\n", min_cos_angle);
			}
			if (cos_angle > max_cos_angle) {
				max_cos_angle = cos_angle;
				max_fu = fu;
				rt_log("New max cos_angle %g\n", max_cos_angle);
			}
		}
next_edgeuse:	eu_p = eu_p->eumate_p->radial_p;
	} while (eu_p != eu->e_p->eu_p);

	/* min_fu is closest to ray on outbound side
	 * max_fu is closest to ray on inbound side
	 */

	/* Compute the ray state on the inbound side */
	NMG_GET_FU_NORMAL(norm, min_fu);
	cos_angle = VDOT(norm, rd->rp->r_dir);

	if (rt_g.NMG_debug & DEBUG_RT_ISECT) {
		VPRINT("\nmin face normal", norm);
		rt_log("cos_angle wrt ray direction: %g\n", cos_angle);
	}

	if (RT_VECT_ARE_PERP(cos_angle, rd->tol))
		in_out |= NMG_RAY_STATE_ON << 4;
	else if (cos_angle < 0.0)
		in_out |= NMG_RAY_STATE_OUTSIDE << 4;
	else /* (cos_angle > 0.0) */
		in_out |= NMG_RAY_STATE_INSIDE << 4;


	/* Compute the ray state on the outbound side */
	NMG_GET_FU_NORMAL(norm, max_fu);
	cos_angle = VDOT(norm, rd->rp->r_dir);

	if (rt_g.NMG_debug & DEBUG_RT_ISECT) {
		VPRINT("\nmax face normal", norm);
		rt_log("cos_angle wrt ray direction: %g\n", cos_angle);
	}

	if (RT_VECT_ARE_PERP(cos_angle, rd->tol))
		in_out |= NMG_RAY_STATE_ON;
	else if (cos_angle > 0.0)
		in_out |= NMG_RAY_STATE_OUTSIDE;
	else /* (cos_angle < 0.0) */
		in_out |= NMG_RAY_STATE_INSIDE;


	if (rt_g.NMG_debug & DEBUG_RT_ISECT) {
		rt_log("in_out: 0x%02x/", in_out);
		switch(in_out) {
		case HMG_HIT_IN_IN:
			rt_log("IN_IN\n"); break;
		case HMG_HIT_IN_OUT:
			rt_log("IN_OUT\n"); break;
		case HMG_HIT_OUT_IN:
			rt_log("OUT_IN\n"); break;
		case HMG_HIT_OUT_OUT:
			rt_log("OUT_OUT\n"); break;
		case HMG_HIT_IN_ON:
			rt_log("IN_ON\n"); break;
		case HMG_HIT_ON_IN:
			rt_log("ON_IN\n"); break;
		case HMG_HIT_OUT_ON:
			rt_log("OUT_ON\n"); break;
		case HMG_HIT_ON_OUT:
			rt_log("ON_OUT\n"); break;
		default:
			rt_log("?_?\n"); break;
		}
	}
	return in_out;
}

/*
 *
 * record a hit on an edge.
 *
 */
static void
ray_hit_edge(rd, eu_p, dist_along_ray, pt)
struct ray_data *rd;
struct edgeuse *eu_p;
double dist_along_ray;
point_t pt;
{
	struct hitmiss *myhit;
	ray_miss_vertex(rd, eu_p->vu_p);
	ray_miss_vertex(rd, eu_p->eumate_p->vu_p);

	if (rt_g.NMG_debug & DEBUG_RT_ISECT) rt_log("\t - HIT edge 0x%08x\n", eu_p->e_p);

	/* create hit structure for this edge */
	GET_HITMISS(myhit);
	NMG_INDEX_ASSIGN(rd->hitmiss, eu_p->e_p, myhit);

	/* build the hit structure */
	myhit->hit.hit_dist = dist_along_ray;
	VMOVE(myhit->hit.hit_point, pt)
	myhit->hit.hit_private = (genptr_t) eu_p->e_p;

	myhit->in_out = edge_hit_ray_state(rd, eu_p);

	hit_ins(rd, myhit);
	NMG_CK_HITMISS(myhit);
}

/*	I S E C T _ R A Y _ E D G E U S E
 *
 *	Intersect ray with edgeuse.  If they pass within tolerance, a hit
 *	is generated.
 *
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

	if (eu_p->e_p != eu_p->eumate_p->e_p)
		rt_bomb("edgeuse mate has step-father\n");

	rt_log("\tLooking for previous hit on edge 0x%08x ...\n", eu_p->e_p);
	
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

	rt_log("\t No previous hit\n");

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
		{
			point_t pt;

			VJOIN1(pt, rd->rp->r_pt, dist_along_ray, rd->rp->r_dir);

			ray_hit_edge(rd, eu_p, dist_along_ray, pt);

			break;
		}
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
		for (RT_LIST_FOR(eu_p, edgeuse, &lu_p->down_hd)) {
			isect_ray_edgeuse(rd, eu_p);
		}
		return;

	} else if (RT_LIST_FIRST_MAGIC(&lu_p->down_hd)!=NMG_VERTEXUSE_MAGIC) {
		rt_log("in %s at %d", __FILE__, __LINE__);
		nmg_rt_bomb(rd, " bad loopuse child magic");
	}

	/* loopuse child is vertexuse */

	(void) isect_ray_vertexuse(rd,
		RT_LIST_FIRST(vertexuse, &lu_p->down_hd));
}



#define FACE_MISS(rd, f_p) {\
	struct hitmiss *myhit; \
	GET_HITMISS(myhit); \
	NMG_INDEX_ASSIGN(rd->hitmiss, f_p, myhit); \
	RT_LIST_MAGIC_SET(&myhit->l, NMG_RT_HIT_SUB_MAGIC); \
	RT_LIST_INSERT(&rd->rd_miss, &myhit->l); }




static void
eu_touch_func(eu, fpi)
struct edgeuse *eu;
struct fu_pt_info *fpi;
{
	struct edgeuse *eu_next;
	struct ray_data *rd;
	struct hitmiss *myhit;

	NMG_CK_EDGEUSE(eu);
	NMG_CK_EDGEUSE(eu->eumate_p);
	NMG_CK_EDGE(eu->e_p);
	NMG_CK_VERTEXUSE(eu->vu_p);
	NMG_CK_VERTEXUSE(eu->eumate_p->vu_p);
	NMG_CK_VERTEX(eu->vu_p->v_p);
	NMG_CK_VERTEX(eu->eumate_p->vu_p->v_p);

	eu_next = RT_LIST_PNEXT_CIRC(edgeuse, eu);

	rt_log("eu_touch(%g %g %g -> %g %g %g\n",
		eu->vu_p->v_p->vg_p->coord[0],
		eu->vu_p->v_p->vg_p->coord[1],
		eu->vu_p->v_p->vg_p->coord[2],
		eu_next->vu_p->v_p->vg_p->coord[0],
		eu_next->vu_p->v_p->vg_p->coord[1],
		eu_next->vu_p->v_p->vg_p->coord[2]);

	rd = (struct ray_data *)fpi->priv;
	rd->face_subhit = 1;


	rt_log("\tLooking for previous hit on edge 0x%08x ...\n",
		eu->e_p);

	if (myhit = NMG_INDEX_GET(rd->hitmiss, eu->e_p)) {
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

	rt_log("\t No previous hit\n");

	plfu(fpi->fu_p, rd->rp->r_pt, fpi->pt);

	ray_hit_edge(rd, eu, rd->ray_dist_to_plane, fpi->pt);
}


static void
vu_touch_func(vu, fpi)
struct vertexuse *vu;
struct fu_pt_info *fpi;
{
	struct ray_data *rd;

	rt_log("vu_touch(%g %g %g)\n",
		vu->v_p->vg_p->coord[0],
		vu->v_p->vg_p->coord[1],
		vu->v_p->vg_p->coord[2]);

	rd = (struct ray_data *)fpi->priv;
	plfu(fpi->fu_p, rd->rp->r_pt, fpi->pt);

	rd->face_subhit = 1;
	ray_hit_vertex(rd, vu, NMG_VERT_UNKNOWN);
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
	fastf_t			dist;
	struct loopuse		*lu_p;
	point_t			plane_pt;
	struct hitmiss		*myhit;
	int			code;
	plane_t			norm;
	struct fu_pt_info	*fpi;
	double			cos_angle;

	if (rt_g.NMG_debug & DEBUG_RT_ISECT)
		rt_log("isect_ray_faceuse(0x%08x, faceuse:0x%08x/face:0x%08x)\n",
			rd, fu_p, fu_p->f_p);

	NMG_CK_FACEUSE(fu_p);
	NMG_CK_FACEUSE(fu_p->fumate_p);
	NMG_CK_FACE(fu_p->f_p);
	NMG_CK_FACE_G(fu_p->f_p->fg_p);

	/* if this face already processed, we are done. */
	if (myhit = NMG_INDEX_GET(rd->hitmiss, fu_p->f_p)) {
		if (RT_LIST_MAGIC_OK((struct rt_list *)myhit,
		    NMG_RT_HIT_MAGIC)) {
			if (rt_g.NMG_debug & DEBUG_RT_ISECT)
				rt_log(" previously hit\n");
		} else if (RT_LIST_MAGIC_OK((struct rt_list *)myhit,
		    NMG_RT_HIT_SUB_MAGIC)) {
			if (rt_g.NMG_debug & DEBUG_RT_ISECT)
				rt_log(" previously hit sub-element\n");
		} else if (RT_LIST_MAGIC_OK((struct rt_list *)myhit,
		    NMG_RT_MISS_MAGIC)) {
			if (rt_g.NMG_debug & DEBUG_RT_ISECT)
				rt_log(" previously missed\n");
		} else {
			rt_log("%s %d:\n\tBad magic %ld (0x%08x) for hitmiss struct for faceuse 0x%08x\n",
				__FILE__, __LINE__,
				myhit->l.magic, myhit->l.magic, fu_p);
			nmg_rt_bomb(rd, "Was I hit or not?\n");
		}
		return;
	}


	/* bounding box intersection */
	if (!rt_in_rpp(rd->rp, rd->rd_invdir,
	    fu_p->f_p->min_pt, fu_p->f_p->max_pt) ) {
		GET_HITMISS(myhit);
		NMG_INDEX_ASSIGN(rd->hitmiss, fu_p->f_p, myhit);
		RT_LIST_MAGIC_SET(&myhit->l, NMG_RT_MISS_MAGIC);
		RT_LIST_INSERT(&rd->rd_miss, &myhit->l);
		if (rt_g.NMG_debug & DEBUG_RT_ISECT)
			rt_log("missed bounding box\n");
		return;
	}

	if (rt_g.NMG_debug & DEBUG_RT_ISECT) rt_log(" hit bounding box \n");

	/* perform the geometric intersection of the ray with the plane 
	 * of the face.
	 */
	NMG_GET_FU_PLANE( norm, fu_p );
	code = rt_isect_line3_plane(&dist, rd->rp->r_pt, rd->rp->r_dir,
			norm, rd->tol);

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


	/* ray hits plane:
	 *
	 * Compute the ray/plane intersection point.
	 * Then compute whether this point lies within the area of the face.
	 */

	VJOIN1(plane_pt, rd->rp->r_pt, dist, rd->rp->r_dir);

	if (rt_g.NMG_debug & DEBUG_RT_ISECT) {
		rt_log("\tray (%g %g %g) (-> %g %g %g)\n",
			rd->rp->r_pt[0],
			rd->rp->r_pt[1],
			rd->rp->r_pt[2],
			rd->rp->r_dir[0],
			rd->rp->r_dir[1],
			rd->rp->r_dir[2]);

		VPRINT("\tplane/ray intersection point", plane_pt);
		rt_log("\tdistance along ray to intersection point %g\n", dist);
	}


	/* determine if the plane point is in or out of the face, and
	 * if it is within tolerance of any of the elements of the faceuse.
	 *
	 * The value of "rd->face_subhit" will be set non-zero if either
	 * eu_touch_func or vu_touch_func is called.  They will be called
	 * when an edge/vertex of the face is within tolerance of plane_pt.
	 */
	rd->face_subhit = 0;
	rd->ray_dist_to_plane = dist;
	fpi = nmg_class_pt_fu_except(plane_pt, fu_p, (struct loopuse *)NULL,
		eu_touch_func, vu_touch_func, (char *)rd, 1, rd->tol);


	GET_HITMISS(myhit);
	NMG_INDEX_ASSIGN(rd->hitmiss, fu_p->f_p, myhit);
	myhit->hit.hit_private = (genptr_t)fu_p->f_p;

	switch (fpi->pt_class) {
	case NMG_CLASS_Unknown	:
		rt_log("%s[line:%d] ray/plane intercept point cannot be classified wrt face\n",
			__FILE__, __LINE__);
		rt_bomb("bombing");
		break;
	case NMG_CLASS_AinB	:
	case NMG_CLASS_AonBshared :
		/* if a sub-element of the face was within tolerance of the
		 * plane intercept, then a hit has already been recorded for
		 * that element, and we do NOT need to generate one for the
		 * face.
		 */
		if (rd->face_subhit) {
			RT_LIST_MAGIC_SET(&myhit->l, NMG_RT_HIT_SUB_MAGIC);
			VMOVE(myhit->hit.hit_point, plane_pt);
			/* also rd->ray_dist_to_plane */
			myhit->hit.hit_dist = dist; 

			myhit->hit.hit_private = (genptr_t)fu_p->f_p;
			RT_LIST_INSERT(&rd->rd_miss, &myhit->l);
		} else {

			/* The plane_pt was NOT within tolerance of a 
			 * sub-element, but it WAS within the area of the 
			 * face.  We need to record a hit on the face
			 */
			RT_LIST_MAGIC_SET(&myhit->l, NMG_RT_HIT_MAGIC);

			VMOVE(myhit->hit.hit_point, plane_pt);

			/* also rd->ray_dist_to_plane */
			myhit->hit.hit_dist = dist; 

			myhit->hit.hit_private = (genptr_t)fu_p->f_p;


			/* compute what the ray-state is before and after this
			 * encountering this hit point.
			 */

			cos_angle = VDOT(norm, rd->rp->r_dir);

			if (fu_p->orientation == OT_OPPOSITE) {
				if (RT_VECT_ARE_PERP(cos_angle, rd->tol)) {
					/* perpendicular? */
					rt_log("%s[%d]: Ray is in plane of face?\n",
						__FILE__, __LINE__);
					rt_bomb("I quit\n");
				} else if (cos_angle > 0.0)
					myhit->in_out = HMG_HIT_IN_OUT;
				else /* (cos_angle < 0.0) */
					myhit->in_out = HMG_HIT_OUT_IN;

			} else if (fu_p->orientation == OT_SAME) {
				if (RT_VECT_ARE_PERP(cos_angle, rd->tol)) {
					/* perpendicular? */
					rt_log("%s[%d]: Ray is in plane of face?\n",
						__FILE__, __LINE__);
					rt_bomb("I quit\n");
				} else if (cos_angle > 0.0)
					myhit->in_out = HMG_HIT_OUT_IN;
				else
					myhit->in_out = HMG_HIT_IN_OUT;
			}


			hit_ins(rd, myhit);
			NMG_CK_HITMISS(myhit);
		}
		break;
	case NMG_CLASS_AoutB	:
		RT_LIST_MAGIC_SET(&myhit->l, NMG_RT_MISS_MAGIC);
		RT_LIST_INSERT(&rd->rd_miss, &myhit->l);
		break;
	default	:
		rt_log("%s[line:%d] BIZZARE ray/plane intercept point classification\n",
			__FILE__, __LINE__);
		rt_bomb("bombing");
	}

	/* intersect the ray with the edges/verticies of the face */
	for ( RT_LIST_FOR(lu_p, loopuse, &fu_p->lu_hd) )
		isect_ray_loopuse(rd, lu_p);
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


	rt_g.NMG_debug |= DEBUG_RT_ISECT;
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

	if (rt_g.NMG_debug & DEBUG_RT_ISECT) {
		if (RT_LIST_IS_EMPTY(&rd->rd_hit)) {
			if (rt_g.NMG_debug & DEBUG_RT_ISECT)
				rt_log("ray missed NMG\n");
		} else
			print_hitlist(&rd->rd_hit);
	}
}
