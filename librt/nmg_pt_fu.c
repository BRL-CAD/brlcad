/*
 *			N M G _ P T _ F U . C
 *
 *  Routines for classifying a point with respect to a faceuse.
 *
 *  Author -
 *	Lee A. Butler
 *  
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5068  USA
 *  
 *  Distribution Notice -
 *	Re-distribution of this software is restricted, as described in
 *	your "Statement of Terms and Conditions for the Release of
 *	The BRL-CAD Package" agreement.
 *
 *  Copyright Notice -
 *	This software is Copyright (C) 1994 by the United States Army
 *	in all countries except the USA.  All rights reserved.
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (ARL)";
#endif

#include "conf.h"

#include <stdio.h>
#include <math.h>
#include "machine.h"
#include "vmath.h"
#include "nmg.h"
#include "raytrace.h"
#include "./nmg_rt.h"




static void
nmg_class_pt_eu(fpi, eu)
struct fu_pt_info	*fpi;
struct edgeuse	*eu;
{
	fastf_t	dist = -1.0;
	point_t	pca;
	int	status;
	struct edgeuse *eunext;

	NMG_CK_FPI(fpi);
	RT_CK_TOL(fpi->tol);
	NMG_CK_EDGEUSE(eu);
	NMG_CK_VERTEXUSE(eu->vu_p);
	NMG_CK_VERTEX(eu->vu_p->v_p);
	NMG_CK_VERTEX_G(eu->vu_p->v_p->vg_p);
	NMG_CK_EDGEUSE(eu->eumate_p);
	NMG_CK_VERTEXUSE(eu->eumate_p->vu_p);
	NMG_CK_VERTEX(eu->eumate_p->vu_p->v_p);
	NMG_CK_VERTEX_G(eu->eumate_p->vu_p->v_p->vg_p);

	eunext = RT_LIST_PNEXT_CIRC(edgeuse, &eu->l);	
	NMG_CK_VERTEXUSE(eunext->vu_p);
	NMG_CK_VERTEX(eunext->vu_p->v_p);
	NMG_CK_VERTEX_G(eunext->vu_p->v_p->vg_p);

	if (rt_g.NMG_debug & DEBUG_PT_FU )
		rt_log("nmg_class_pt_eu( (%g %g %g)->(%g %g %g) )\n",
			V3ARGS(eu->vu_p->v_p->vg_p->coord),
			V3ARGS(eunext->vu_p->v_p->vg_p->coord));

	/* if this edgeuse's vertexuse is on a previously processed & touched
	 * vertex:
	 *	if the user wants to get called for each use,
	 *		make the call for this use.
	 *	mark this edgeuse as processed.
	 *	return
	 */
	if (NMG_INDEX_GET(fpi->tbl, eu->vu_p->v_p) == NMG_FPI_TOUCHED) {
		if (fpi->vu_func && fpi->allhits == NMG_FPI_PERUSE)
			fpi->vu_func(eu->vu_p, fpi);

		NMG_INDEX_ASSIGN(fpi->tbl, eu->e_p, NMG_FPI_TOUCHED);
		if (rt_g.NMG_debug & DEBUG_PT_FU )
			rt_log("\teu->vu->v previously touched\n");

		return;
	}

	/* if the next edgeuse's vertexuse is on a previously 
	 * processed & touched vertex:
	 *	if the user wants to get called for each use,
	 *		make the call for this use.
	 *	mark this edgeuse as processed.
	 *	return
	 */
	if (NMG_INDEX_GET(fpi->tbl, eunext->vu_p->v_p) == NMG_FPI_TOUCHED) {
		if (fpi->vu_func && fpi->allhits == NMG_FPI_PERUSE)
			fpi->vu_func(eunext->vu_p, fpi);

		NMG_INDEX_ASSIGN(fpi->tbl, eu->e_p, NMG_FPI_TOUCHED);
		if (rt_g.NMG_debug & DEBUG_PT_FU )
			rt_log("\teunext->vu->v previously touched\n");

		return;
	}


	/* The verticies at the ends of this edge were missed or hit
	 * previously processed.
	 */
	switch (NMG_INDEX_GET(fpi->tbl, eu->e_p)) {
	case NMG_FPI_TOUCHED:
		/* The edgeuse has been pre-determined to be touching.
		 * This was a touch on the edge span.
		 *
		 * If the user wants to get called for each use, make the
		 * call for this edgeuse.
		 */
		if (rt_g.NMG_debug & DEBUG_PT_FU )
			rt_log ("\teu previously hit\n");

		if (fpi->eu_func && fpi->allhits == NMG_FPI_PERUSE)
			fpi->eu_func(eu, fpi);

		return;
	case NMG_FPI_MISSED:
		if (rt_g.NMG_debug & DEBUG_PT_FU )
			rt_log ("\teu previously missed\n");

		return;
	}


	/* The edgeuse was not previously processed, 
	 * so it's time to do it now.
	 */
	if (rt_g.NMG_debug & DEBUG_PT_FU )
		rt_log ("\tprocessing eu (%g %g %g) -> (%g %g %g)\n",
			V3ARGS(eu->vu_p->v_p->vg_p->coord),
			V3ARGS(eunext->vu_p->v_p->vg_p->coord));

	status = rt_dist_pt3_lseg3(&dist, pca,
		eu->vu_p->v_p->vg_p->coord,
		eu->eumate_p->vu_p->v_p->vg_p->coord,
		fpi->pt,
		fpi->tol);


	switch (status) {
	case 0: /* pt is on the edge(use)
		 * store edgeuse ptr in closest,
		 * set dist_in_plane = 0.0
		 *
		 * dist is the parametric distance along the edge where
		 * the PCA occurrs!
		 */

		fpi->dist_in_plane = 0.0;
		VMOVE(fpi->pca, pca);
		fpi->closest = &eu->l.magic;
		fpi->PCA_loc = NMG_PCA_EDGE;
		fpi->pt_class = NMG_CLASS_AonBshared;
		if (rt_g.NMG_debug & DEBUG_PT_FU )
			rt_log(	"\tplane_pt on eu, (new dist: %g)\n",
				fpi->dist_in_plane);
		if (fpi->eu_func)
			fpi->eu_func(eu, fpi);
		NMG_INDEX_ASSIGN(fpi->tbl, eu->vu_p->v_p, NMG_FPI_MISSED);
		NMG_INDEX_ASSIGN(fpi->tbl, eu->e_p, NMG_FPI_TOUCHED);
		break;
	case 1:	/* within tolerance of endpoint at eu->vu_p.
		   store vertexuse ptr in closest,
		   set dist_in_plane = 0.0 */
		if (dist < fpi->dist_in_plane) {
			fpi->dist_in_plane = 0.0;
			VMOVE(fpi->pca, pca);
			fpi->closest = &eu->vu_p->l.magic;
			fpi->PCA_loc = NMG_PCA_EDGE_VERTEX;
			fpi->pt_class = NMG_CLASS_AonBshared;
			if (rt_g.NMG_debug & DEBUG_PT_FU )
				rt_log("\tplane_pt on vu (new dist %g)\n",
					fpi->dist_in_plane);
		} else {
			if (rt_g.NMG_debug & DEBUG_PT_FU )
				rt_log("\tplane_pt on vu(dist %g) keeping old dist %g)\n",
					dist, fpi->dist_in_plane);
		}
		if (fpi->vu_func)
			fpi->vu_func(eu->vu_p, fpi);

		NMG_INDEX_ASSIGN(fpi->tbl, eu->vu_p->v_p, NMG_FPI_TOUCHED);
		NMG_INDEX_ASSIGN(fpi->tbl, eu->e_p, NMG_FPI_TOUCHED);
		break;
	case 2:	/* within tolerance of endpoint at eu->eumate_p
		   store vertexuse ptr (eu->next) in closest,
		   set dist_in_plane = 0.0 */
		if (dist < fpi->dist_in_plane) {
			fpi->dist_in_plane = 0.0;
			VMOVE(fpi->pca, pca);
			fpi->closest = &eunext->vu_p->l.magic;
			fpi->PCA_loc = NMG_PCA_EDGE_VERTEX;
			fpi->pt_class = NMG_CLASS_AonBshared;
			if (rt_g.NMG_debug & DEBUG_PT_FU )
				rt_log("\tplane_pt on next(eu)->vu (new dist %g)\n",
					fpi->dist_in_plane);
		} else {
			if (rt_g.NMG_debug & DEBUG_PT_FU )
				rt_log("\tplane_pt on next(eu)->vu(dist %g) keeping old dist %g)\n",
					dist, fpi->dist_in_plane);
		}
		if (fpi->vu_func)
			fpi->vu_func(eunext->vu_p, fpi);

		NMG_INDEX_ASSIGN(fpi->tbl, eunext->vu_p->v_p, NMG_FPI_TOUCHED);
		NMG_INDEX_ASSIGN(fpi->tbl, eunext->e_p, NMG_FPI_TOUCHED);
		NMG_INDEX_ASSIGN(fpi->tbl, eu->e_p, NMG_FPI_TOUCHED);

		break;
	case 3: /* PCA of pt on line is "before" eu->vu_p of seg */
		if (dist < fpi->dist_in_plane) {
			fpi->dist_in_plane = dist;
			VMOVE(fpi->pca, pca);
			fpi->closest = &eu->vu_p->l.magic;
			fpi->PCA_loc = NMG_PCA_EDGE_VERTEX;

			if (rt_g.NMG_debug & DEBUG_PT_FU )
				rt_log("\tvu of eu is new \"closest to plane_pt\" (new dist %g)\n",
					fpi->dist_in_plane);
		} else {
			if (rt_g.NMG_debug & DEBUG_PT_FU )
				rt_log("\tvu of eu is PCA (dist %g).  keeping old dist %g\n",
					dist, fpi->dist_in_plane);
		}
		NMG_INDEX_ASSIGN(fpi->tbl, eu->e_p, NMG_FPI_MISSED);
		break;
	case 4: /* PCA of pt on line is "after" eu->eumate_p->vu_p of seg */
		if (dist < fpi->dist_in_plane) {
			eu = RT_LIST_PNEXT_CIRC(edgeuse, &(eu->l));
			fpi->dist_in_plane = dist;
			VMOVE(fpi->pca, pca);
			fpi->closest = &eu->vu_p->l.magic;
			fpi->PCA_loc = NMG_PCA_EDGE_VERTEX;
			if (rt_g.NMG_debug & DEBUG_PT_FU )
				rt_log("\tvu of next(eu) is new \"closest to plane_pt\" (new dist %g)\n",
					fpi->dist_in_plane);
		} else {
			if (rt_g.NMG_debug & DEBUG_PT_FU )
				rt_log("\tvu of next(eu) is PCA (dist %g).  keeping old dist %g\n",
					dist, fpi->dist_in_plane);
		}
		NMG_INDEX_ASSIGN(fpi->tbl, eu->vu_p->v_p, NMG_FPI_MISSED);
		NMG_INDEX_ASSIGN(fpi->tbl, eunext->vu_p->v_p, NMG_FPI_MISSED);
		NMG_INDEX_ASSIGN(fpi->tbl, eu->e_p, NMG_FPI_MISSED);
		break;
	case 5: /* PCA is along length of edge, but point is NOT on edge.
		 *  if edge is closer to plane_pt than any previous item,
		 *  store edgeuse ptr in closest and set dist_in_plane
		 */
		if (dist < fpi->dist_in_plane) {
			fpi->dist_in_plane = dist;
			VMOVE(fpi->pca, pca);
			fpi->closest = &eu->l.magic;
			fpi->PCA_loc = NMG_PCA_EDGE;
			if (rt_g.NMG_debug & DEBUG_PT_FU )
				rt_log("\teu is new \"closest to plane_pt\" (new dist %g)\n",
					fpi->dist_in_plane);
		} else {
			if (rt_g.NMG_debug & DEBUG_PT_FU )
				rt_log("\teu dist is %g, keeping old dist %g)\n",
					dist, fpi->dist_in_plane);
		}
		NMG_INDEX_ASSIGN(fpi->tbl, eu->vu_p->v_p, NMG_FPI_MISSED);
		NMG_INDEX_ASSIGN(fpi->tbl, eunext->vu_p->v_p, NMG_FPI_MISSED);
		NMG_INDEX_ASSIGN(fpi->tbl, eu->e_p, NMG_FPI_MISSED);
		break;
	default :
		rt_log("Look, there has to be SOMETHING about this edge/plane_pt %s %d\n",
			__FILE__, __LINE__);
		rt_bomb("");
		break;
	}
}
/*
 *			N M G _ C L A S S _ P T _ L U
 *
 *  Determine if the point fpi->plane_pt is IN, ON, or OUT of the
 *  area enclosed by the loop.
 *
 *  Implicit Return -
 *	Updated "closest" structure if appropriate.
 */
static void
nmg_class_pt_lu(fpi, lu)
struct fu_pt_info	*fpi;
CONST struct loopuse	*lu;
{
	vect_t		delta;
	pointp_t	lu_pt;
	fastf_t		dist;
	struct loop_g	*lg;

	NMG_CK_LOOPUSE(lu);
	NMG_CK_LOOP(lu->l_p);
	lg = lu->l_p->lg_p;
	NMG_CK_LOOP_G(lg);
	NMG_CK_FPI(fpi);

	if (rt_g.NMG_debug & DEBUG_PT_FU ) {
		rt_log(
		    "nmg_class_pt_lu\tPt: (%g %g %g)\n\tinitial class is %s\n",
		    fpi->pt, nmg_class_name(fpi->pt_class));
	}

	/* if this isn't a loop of a face, or if we've already tested
	 * this loop, we're outta here
	 */
	if (lu->up.fu_p != fpi->fu_p || NMG_INDEX_TEST(fpi->tbl, lu))
		return;
 
	if( !V3PT_IN_RPP_TOL( fpi->pt, lg->min_pt, lg->max_pt, fpi->tol ) )  {
		if (rt_g.NMG_debug & DEBUG_PT_FU )
			rt_log("\tPoint is outside loop RPP\n");
		NMG_INDEX_SET(fpi->tbl, lu);
		NMG_INDEX_SET(fpi->tbl, lu->lumate_p);
		return;
	}

	if (RT_LIST_FIRST_MAGIC(&lu->down_hd) == NMG_EDGEUSE_MAGIC) {
		register struct edgeuse	*eu;
		for (RT_LIST_FOR(eu, edgeuse, &lu->down_hd)) {
			
			nmg_class_pt_eu(fpi, eu);

			/* If point lies ON edge, we might be done */
			if( fpi->allhits == NMG_FPI_FIRST &&
			    fpi->pt_class == NMG_CLASS_AonBshared )
				 break;
		}
	} else if (RT_LIST_FIRST_MAGIC(&lu->down_hd) == NMG_VERTEXUSE_MAGIC) {
		register struct vertexuse *vu;

		vu = RT_LIST_FIRST(vertexuse, &lu->down_hd);

		/* check to see if this vertex was previously processed */
		switch (NMG_INDEX_GET(fpi->tbl, vu->v_p)) {
		case NMG_FPI_TOUCHED:
			if (fpi->vu_func && fpi->allhits == NMG_FPI_PERUSE)
				fpi->vu_func(vu, fpi);
			/* fallthrough */
		case NMG_FPI_MISSED:
			/* if this point were going to be the "closest"
			 * element, it has already been chosen
			 */
			return;
		}

		lu_pt = vu->v_p->vg_p->coord;
		VSUB2(delta, fpi->pt, lu_pt);
		dist = MAGNITUDE(delta);
		
		if (dist < fpi->tol->dist) {
			/* this is a touch */
			dist = 0.0;
			if (fpi->vu_func)
				fpi->vu_func(vu, fpi);

			NMG_INDEX_ASSIGN(fpi->tbl, vu->v_p, NMG_FPI_TOUCHED);
		} else {
			NMG_INDEX_ASSIGN(fpi->tbl, vu->v_p, NMG_FPI_MISSED);
		}

		if (dist < fpi->dist_in_plane) {
			if (lu->orientation == OT_OPPOSITE) {
				fpi->pt_class = NMG_CLASS_AoutB;
			} else if (lu->orientation == OT_SAME) {
				fpi->pt_class = NMG_CLASS_AonBshared;
			} else {
				nmg_pr_orient(lu->orientation, "\t");
				rt_bomb("nmg_class_pt_lu: bad orientation for face loopuse(vertex)\n");
			}
			if (rt_g.NMG_debug & DEBUG_PT_FU )
				rt_log("\t\t closer to loop pt (%g, %g, %g)\n",
					V3ARGS(lu_pt) );

			fpi->PCA_loc = NMG_PCA_VERTEX;
			fpi->closest = &vu->l.magic;
			fpi->dist_in_plane = dist;
			VMOVE(fpi->pca, vu->v_p->vg_p->coord);
		}
	} else {
		rt_bomb("nmg_class_pt_lu() bad child of loopuse\n");
	}

	NMG_INDEX_SET(fpi->tbl, lu);
	NMG_INDEX_SET(fpi->tbl, lu->lumate_p);

	if (rt_g.NMG_debug & DEBUG_PT_FU )
		rt_log("nmg_class_pt_lu\treturning, closest=%g %s\n",
			fpi->dist_in_plane, nmg_class_name(fpi->pt_class) );
}




/*
 *      The Paul Tanenbaum "patch"
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
 */
static void
Tanenbaum_patch(fpi, vu_p)
struct fu_pt_info *fpi;
struct vertexuse *vu_p;
{

    	/* find the appropriate "closest edge" use of this vertex. */
    	vect_t pv;
    	vect_t left;	/* vector pointing "left" from edgeuse into faceuse */
	double vdot_min = 2.0;
	double newvdot;
    	struct edgeuse *closest_eu;
    	struct faceuse *fu;
    	struct vertexuse *vu;
	struct edgeuse *eu;

	if (rt_g.NMG_debug & DEBUG_PT_FU )
		rt_log("Tanenbaum_patch()\n");


    	/* First we form the unit vector PV */
    	VSUB2(pv, fpi->pt, vu_p->v_p->vg_p->coord);
    	VUNITIZE(pv);

    	/* for each edge in this faceuse about the vertex, determine
    	 * the angle between PV and the "left" vector.  Classify the
	 * plane_pt WRT the edge for which VDOT(left, P) with P
    	 */
    	for (RT_LIST_FOR(vu, vertexuse, &vu_p->v_p->vu_hd)) {
		/* if there is a faceuse ancestor of this vertexuse
		 * which is associated with this face, then perform
    		 * the VDOT and save the results.
    		 */
    		fu = nmg_find_fu_of_vu(vu);
		if (*vu->up.magic_p == NMG_EDGEUSE_MAGIC &&
		    fu == fpi->fu_p || fu->fumate_p == fpi->fu_p) {

		    	if( nmg_find_eu_leftvec(left, eu=vu->up.eu_p ) < 0 ) {
		    		rt_log("%s[line:%d]: bad LEFT vector\n",
		    			__FILE__,__LINE__);
		    		rt_bomb("");
		    	}
		    	VUNITIZE(left);

			newvdot = VDOT(left, pv);
		    	/* if the "left" vector of eu is "more opposed"
		    	 * to the vector pv than the previous edge,
		    	 * remember this edge as the "closest" one
		    	 */
			if (newvdot < vdot_min) {
				closest_eu = eu;
				vdot_min = newvdot;
			}
		}
	}
	fu = nmg_find_fu_of_eu(closest_eu);
	if (fu == fpi->fu_p) {
	    	eu = closest_eu;
	} else if (fu == fpi->fu_p->fumate_p) {
	    	eu = closest_eu->eumate_p;
	} else {
		rt_log("%s[line:%d] Why can't I find this edge in this face?\n",
			__FILE__, __LINE__);
		rt_bomb("");
	}

	/* now we know which edgeuse to compare against */
	if (vdot_min < 0.0)
		fpi->pt_class = NMG_CLASS_AoutB;
	else
		fpi->pt_class = NMG_CLASS_AinB;
}





/*	D E D U C E _ P T _ C L A S S
 *
 *	For those occasions when the plane_pt is not within tolerance of
 *	any element of the faceuse, we must deduce the pt classification
 *	based upon the "closest" element to the pt.
 */
static void
deduce_pt_class(fpi)
struct fu_pt_info *fpi;
{
	struct edgeuse		*eu;
	struct vertexuse	*vu;
	struct loopuse		*lu;
	vect_t			left;
	vect_t			vupt_v;

	NMG_CK_FPI(fpi);

	if (rt_g.NMG_debug & DEBUG_PT_FU )
		rt_log("deduce_pt_class()\n");


	/* compute in/out based upon closest element */
	if (*fpi->closest == NMG_EDGEUSE_MAGIC) {
		struct faceuse *fu;
		struct edgeuse *eu1;

		/* PCA along span of edgeuse */
		eu = (struct edgeuse *)fpi->closest;
		fu = nmg_find_fu_of_eu( eu );
		if( !fu )
		{
			rt_log("%s[line:%d]: deduce_pt_class: eu not in a fu\n",
				__FILE__,__LINE__);
			rt_bomb("");
		}

		/* default is point is outside */
		fpi->pt_class = NMG_CLASS_AoutB;

		/* check all uses of this edge in this fu,
		 * if any say point is "in", then it must be "in"
		 */
		eu1 = eu;
		do
		{
			struct edgeuse *next_eu;

			next_eu = eu1->radial_p->eumate_p;
			/* make sure we stay in this fu */
			if( nmg_find_fu_of_eu( eu1 ) != fu )
			{
				if( nmg_find_fu_of_eu( eu1->eumate_p ) == fu )
					eu1 = eu1->eumate_p;
				else
				{
					eu1 = next_eu;
					continue;
				}
			}
			if( nmg_find_eu_leftvec( left, eu1 ) < 0 ) {
				rt_log("%s[line:%d]: bad LEFT vector\n",
					__FILE__,__LINE__);
				rt_bomb("");
			}
			/* form vector "vupt" from vu to pt, if VDOT(vupt, left)
			 * is less than 0, pt is on "outside" of edge.
			 */
			VSUB2(vupt_v, fpi->pt, eu1->vu_p->v_p->vg_p->coord);
			if (VDOT(vupt_v, left) >= 0.0)
			{
				if (rt_g.NMG_debug & DEBUG_PT_FU )
rt_log("vdot w/eu (%g %g %g)->(%g %g %g) left\n\tsuggests NMG_CLASS_AinB\n",
				V3ARGS(eu1->vu_p->v_p->vg_p->coord),
				V3ARGS(eu1->eumate_p->vu_p->v_p->vg_p->coord));
				
				fpi->pt_class = NMG_CLASS_AinB;
				break;
			}

			eu1 = next_eu;
		} while( eu1 != eu && eu1 != eu->eumate_p );

	} else if (*fpi->closest == NMG_VERTEXUSE_MAGIC) {
		vu = (struct vertexuse *)fpi->closest;
		if (*vu->up.magic_p == NMG_EDGEUSE_MAGIC) {
			/* point is off one end or the other of edge
			 */
			Tanenbaum_patch(fpi, vu);

		} else if (*vu->up.magic_p == NMG_LOOPUSE_MAGIC) {
			lu = (struct loopuse *)vu->up.magic_p;
			if (lu->orientation == OT_OPPOSITE)
				fpi->pt_class = NMG_CLASS_AinB;
			else if (lu->orientation == OT_SAME)
				fpi->pt_class = NMG_CLASS_AoutB;
			else {
				rt_log("%s[line:%d] Bad orientation \"%s\"for loopuse(vertex)\n",
					__FILE__, __LINE__,
					nmg_orientation(lu->orientation));
				rt_bomb("");
			}
		} else if (*vu->up.magic_p == NMG_SHELL_MAGIC) {
			rt_log("%s[line:%d] vertexuse parent is SHELL???\nnmg_class_pt_fu_except(%g %g %g)\n",
				__FILE__, __LINE__,
				V3ARGS(fpi->pt));
			rt_bomb("");
		} else {
			rt_log("%s[line:%d] Bad vertexuse parent?\nnmg_class_pt_fu_except(%g %g %g)\n",
				__FILE__, __LINE__,
				V3ARGS(fpi->pt));
			rt_bomb("");
		}
	}
}




static void
pl_pt_fu(fpi, fu)
struct fu_pt_info *fpi;
CONST struct faceuse *fu;
{
	FILE *fd;
	char name[25];
	long *b;
	static int plot_file_number=0;

	NMG_CK_FACEUSE(fu);
	NMG_CK_FPI(fpi);

	
	sprintf(name, "pt_fu%02d.pl", plot_file_number++);
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

	pl_color(fd, 255, 255, 50);
	pdv_3line(fd, fpi->pca, fpi->pt);

	rt_free((char *)b, "bit vec");
	fclose(fd);
}

/*
 *			N M G _ C L A S S _ P T _ F U
 *
 *  This is intended as a general user interface routine.
 *  Given the Cartesian coordinates for a point which is known to
 *  lie in the plane of a faceuse, return the classification for that point
 *  with respect to all the loops on that face.
 *
 *  The algorithm used is to find the edge which the point is closest
 *  to, and classifiy with respect to that.
 *
 *  "ignore_lu" is optional.  When non-null, it points to a loopuse (and it's
 *  mate) which will not be considered in the assessment of this point.
 *  This is used by nmg_lu_reorient() to work on one lu in the face.
 *
 *  The point is "A", and the face is "B".
 *
 *  Returns -
 *	struct fu_pt_info *	The user is responsible for calling rt_free
 * 				On this storage.
 *
 *	fpi->pt_class has one of the following:
 *	NMG_CLASS_AinB		pt is INSIDE the area of the faceuse.
 *	NMG_CLASS_AonBshared	pt is ON a loop boundary.
 *	NMG_CLASS_AoutB		pt is OUTSIDE the area of the faceuse.
 *
 *
 *  Values for "allhits"
 *	0	return after finding first element pt touches
 *	1	find all elements pt touches, call user routine for each geom.
 *	2	find all elements pt touches, call user routine for each use
 */
struct fu_pt_info *
nmg_class_pt_fu_except(pt, fu, ignore_lu, 
			eu_func, vu_func, priv, allhits, tol)
point_t			pt;
CONST struct faceuse	*fu;
CONST struct loopuse	*ignore_lu;
void			(*eu_func)();	/* func to call when pt on edgeuse */
void			(*vu_func)();	/* func to call when pt on vertexuse*/
char			*priv;		/* private data for [ev]u_func */
CONST int		allhits;	/* return after finding first hit */
CONST struct rt_tol	*tol;
{
	struct loopuse		*lu;
	struct fu_pt_info	*fpi;
	fastf_t			dist;
	struct model		*m;

	NMG_CK_FACEUSE(fu);
	NMG_CK_FACE(fu->f_p);
	NMG_CK_FACE_G_PLANE(fu->f_p->g.plane_p);
	if(ignore_lu) NMG_CK_LOOPUSE(ignore_lu);
	RT_CK_TOL(tol);


	if (rt_g.NMG_debug & DEBUG_PT_FU )
		rt_log("nmg_class_pt_fu_except()\n");

	fpi = (struct fu_pt_info *)rt_malloc(sizeof(*fpi), "struct fu_pt_info");

	/* Validate distance from point to plane */
	NMG_GET_FU_PLANE( fpi->norm, fu );
	if( (dist=fabs(DIST_PT_PLANE( pt, fpi->norm ))) > tol->dist )  {
		rt_log("nmg_class_pt_fu_except() ERROR, point (%g,%g,%g)\nnot on face %g %g %g %g,\ndist=%g\n",
			V3ARGS(pt), V4ARGS(fpi->norm), dist );
	}

	m = nmg_find_model((CONST long *)fu);
	fpi->tbl = rt_calloc(m->maxindex, 1, "nmg_class_pt_fu_except() proc tbl");

	fpi->tol = tol;
	fpi->fu_p = fu;
	fpi->pt = pt;
	fpi->pt_class = NMG_CLASS_Unknown;
	fpi->eu_func = eu_func;
	fpi->vu_func = vu_func;
	fpi->allhits = allhits;
	fpi->dist_in_plane = MAX_FASTF;
	VSET(fpi->pca, 0.0, 0.0, 0.0);
	fpi->closest = (long *)NULL;
	fpi->priv = priv;
	fpi->magic = NMG_FPI_MAGIC;


	for (RT_LIST_FOR(lu, loopuse, &fu->lu_hd)) {
		if( ignore_lu && (ignore_lu==lu || ignore_lu==lu->lumate_p) )
			continue;

		nmg_class_pt_lu(fpi, lu);

		/* If point lies ON loop edge, we are done */
		if( allhits == NMG_FPI_FIRST && 
		    fpi->pt_class != NMG_CLASS_Unknown )
			break;
	}


	if (fpi->pt_class == NMG_CLASS_AinB ||
	    fpi->pt_class == NMG_CLASS_AoutB ||
	    fpi->pt_class == NMG_CLASS_AonBshared)
		goto exit_ramp;

	if (fpi->pt_class != NMG_CLASS_Unknown) {
		rt_log("%s[line:$d]: bad classification \"%s\" for point in face?\n",
			__FILE__, __LINE__, nmg_class_name(fpi->pt_class));
		rt_bomb("");
	}

	if (fpi->closest == (long *)NULL) {
		/* The plane point was never within a bounding RPP of a loop.
		 * Therefore, we are decidedly OUTside the area of the face.
		 */
		fpi->pt_class = NMG_CLASS_AoutB;
	} else
		deduce_pt_class(fpi);

exit_ramp:
	if (rt_g.NMG_debug & DEBUG_PT_FU ) {
		if (fpi->pt_class != NMG_CLASS_Unknown)
			pl_pt_fu(fpi, fu);
		else
			rt_log("class = unknown, skipping plot\n");

		rt_log("nmg_class_pt_fu_except(): point is classed %s\n",
			nmg_class_name(fpi->pt_class));
	}
	return fpi;
}


