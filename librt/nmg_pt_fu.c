/*
 *
 *
 */
#include <stdio.h>
#include "machine.h"
#include "vmath.h"
#include "nmg.h"
#include "raytrace.h"
#include "./nmg_rt.h"

struct fu_pt_info {
	long		magic;
	CONST struct rt_tol	*tol;
	CONST struct faceuse	*fu_p;
	plane_t		norm;		/* surface normal for face(use) */
	pointp_t	pt;		/* pt in plane of face to classify */
	int		pt_class;	/* current point classification */
	void		(*eu_func)();	/* call w/eu when pt on edgeuse */
	void		(*vu_func)();	/* call w/vu when pt on vertexuse */
	int		onehit;		/* return after first "on" condition*/
	fastf_t		dist_in_plane;	/* dist in plane (elem -> plane_pt) */
	long		*closest;	/* ptr to elem w/ min(dist_in_plane)*/
	int		PCA_loc;	/* is PCA at an edge-span,
					 *  edge-vertex, or vertex?
					 */
}

#define NMG_FPI_MAGIC 0x66706900 /* fpi\0 */
#define NMG_CK_FPI(_fpi)	NMG_CKMAG(_fpi, NMG_FPI_MAGIC, "fu_pt_info")

nmg_class_pt_eu(fpi, eu)
struct fu_pt_info	*fpi;
struct edgeuse	*eu;
{
	fastf_t	dist=-1.0;
	point_t	pca;
	int	status;

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
		fpi->closest = &eu->l.magic;
		fpi->PCA_loc = NMG_PCA_EDGE;
		fpi->pt_class = NMG_CLASS_AonBshared;
		if (rt_g.NMG_debug & DEBUG_RT_ISECT)
			rt_log(	"\tplane_pt on eu, (new dist: %g)\n",
				fpi->dist_in_plane);
		if (fpi->eu_func)
			fpi->eu_func(eu, fpi);
		break;
	case 1:	/* within tolerance of endpoint at eu->vu_p.
		   store vertexuse ptr in closest,
		   set dist_in_plane = 0.0 */
		if (dist < fpi->dist_in_plane) {
			fpi->dist_in_plane = 0.0;
			fpi->closest = &eu->vu_p->l.magic;
			fpi->PCA_loc = NMG_PCA_EDGE_VERTEX;
			fpi->pt_class = NMG_CLASS_AonBshared;
			if (rt_g.NMG_debug & DEBUG_RT_ISECT)
				rt_log("\tplane_pt on vu (new dist %g)\n",
					fpi->dist_in_plane);
		} else {
			if (rt_g.NMG_debug & DEBUG_RT_ISECT)
				rt_log("\tplane_pt on vu(dist %g) keeping old dist %g)\n",
					dist, fpi->dist_in_plane);
		}
		if (fpi->vu_func)
			fpi->vu_func(eu->vu_p, fpi);

		break;
	case 2:	/* within tolerance of endpoint at eu->eumate_p
		   store vertexuse ptr (eu->next) in closest,
		   set dist_in_plane = 0.0 */
		if (dist < fpi->dist_in_plane) {
			eu = RT_LIST_PNEXT_CIRC(edgeuse, &(eu->l));
			fpi->dist_in_plane = 0.0;
			fpi->closest = &eu->vu_p->l.magic;
			fpi->PCA_loc = NMG_PCA_EDGE_VERTEX;
			fpi->pt_class = NMG_CLASS_AonBshared;
			if (rt_g.NMG_debug & DEBUG_RT_ISECT)
				rt_log("\tplane_pt on next(eu)->vu (new dist %g)\n",
					fpi->dist_in_plane);
		} else {
			if (rt_g.NMG_debug & DEBUG_RT_ISECT)
				rt_log("\tplane_pt on next(eu)->vu(dist %g) keeping old dist %g)\n",
					dist, fpi->dist_in_plane);
		}
		if (fpi->vu_func)
			fpi->vu_func(eu->vu_p, fpi);

		break;
	case 3: /* PCA of pt on line is "before" eu->vu_p of seg */
		if (dist < fpi->dist_in_plane) {
			fpi->dist_in_plane = dist;
			fpi->closest = &eu->vu_p->l.magic;
			fpi->PCA_loc = NMG_PCA_EDGE_VERTEX;
			if (rt_g.NMG_debug & DEBUG_RT_ISECT)
				rt_log("\vu of eu is new \"closest to plane_pt\" (new dist %g)\n",
					fpi->dist_in_plane);
		} else {
			if (rt_g.NMG_debug & DEBUG_RT_ISECT)
				rt_log("\vu of eu is PCA (dist %g).  keeping old dist %g\n",
					dist, fpi->dist_in_plane);
		}
		break;
	case 4: /* PCA of pt on line is "after" eu->eumate_p->vu_p of seg */
		if (dist < fpi->dist_in_plane) {
			eu = RT_LIST_PNEXT_CIRC(edgeuse, &(eu->l));
			fpi->dist_in_plane = dist;
			fpi->closest = &eu->vu_p->l.magic;
			fpi->PCA_loc = NMG_PCA_EDGE_VERTEX;
			if (rt_g.NMG_debug & DEBUG_RT_ISECT)
				rt_log("\tvu of next(eu) is new \"closest to plane_pt\" (new dist %g)\n",
					fpi->dist_in_plane);
		} else {
			if (rt_g.NMG_debug & DEBUG_RT_ISECT)
				rt_log("\vu of next(eu) is PCA (dist %g).  keeping old dist %g\n",
					dist, fpi->dist_in_plane);
		}
		break;
	case 5: /* PCA is along length of edge, but point is NOT on edge.
		 *  if edge is closer to plane_pt than any previous item,
		 *  store edgeuse ptr in closest and set dist_in_plane
		 */
		if (dist < fpi->dist_in_plane) {
			fpi->dist_in_plane = dist;
			fpi->closest = &eu->l.magic;
			fpi->PCA_loc = NMG_PCA_EDGE;
			if (rt_g.NMG_debug & DEBUG_RT_ISECT)
				rt_log("\teu is new \"closest to plane_pt\" (new dist %g)\n",
					fpi->dist_in_plane);
		} else {
			if (rt_g.NMG_debug & DEBUG_RT_ISECT)
				rt_log("\teu dist is %g, keeping old dist %g)\n",
					dist, fpi->dist_in_plane);
		}
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
	struct edgeuse	*eu;
	struct loop_g	*lg;

	NMG_CK_LOOPUSE(lu);
	NMG_CK_LOOP(lu->l_p);
	lg = lu->l_p->lg_p;
	NMG_CK_LOOP_G(lg);
	NMG_CK_FPI(fpi);

	if (rt_g.NMG_debug & DEBUG_CLASSIFY)
		VPRINT("nmg_class_pt_lu\tPt:", fpi->pt);

	if (lu->up.fu_p != fpi->fu_p)
		return;

	if( !V3PT_IN_RPP_TOL( fpi->pt, lg->min_pt, lg->max_pt, fpi->tol ) )  {
		if (rt_g.NMG_debug & DEBUG_CLASSIFY)
			rt_log("\tPoint is outside loop RPP\n");
		return;
	}
	if (RT_LIST_FIRST_MAGIC(&lu->down_hd) == NMG_EDGEUSE_MAGIC) {
		for (RT_LIST_FOR(eu, edgeuse, &lu->down_hd)) {
			
			nmg_class_pt_eu(fpi, eu);

			/* If point lies ON edge, we are done */
			if( fpi->onehit && fpi->pt_class==NMG_CLASS_AonBshared )
				 break;
		}
	} else if (RT_LIST_FIRST_MAGIC(&lu->down_hd) == NMG_VERTEXUSE_MAGIC) {
		register struct vertexuse *vu;

		vu = RT_LIST_FIRST(vertexuse, &lu->down_hd);
		lu_pt = vu->v_p->vg_p->coord;

		VSUB2(delta, fpi->pt, lu_pt);

		if ( (dist = MAGNITUDE(delta)) < fpi->dist_in_plane) {
			if (lu->orientation == OT_OPPOSITE) {
				fpi->pt_class = NMG_CLASS_AoutB;
			} else if (lu->orientation == OT_SAME) {
				fpi->pt_class = NMG_CLASS_AonBshared;
			} else {
				nmg_pr_orient(lu->orientation, "\t");
				rt_bomb("nmg_class_pt_lu: bad orientation for face loopuse(vertex)\n");
			}
			if (rt_g.NMG_debug & DEBUG_CLASSIFY)
				rt_log("\t\t closer to loop pt (%g, %g, %g)\n",
					V3ARGS(lu_pt) );

			fpi->PCA_loc = NMG_PCA_VERTEX;
			fpi->closest = &vu->l.magic;
			if (dist < fpi->tol->dist) {
				fpi->dist_in_plane = 0.0;
				if (fpi->vu_func)
					fpi->vu_func(vu, fpi);
			} else
				fpi->dist_in_plane = dist;
				
		}
	} else {
		rt_bomb("nmg_class_pt_lu() bad child of loopuse\n");
	}
	if (rt_g.NMG_debug & DEBUG_CLASSIFY)
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
		    fu == fpi->fu_p || fu->fumate == fpi->fu_p) {

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

	/* compute in/out based upon closest element */
	if (*fpi->closest == NMG_EDGEUSE_MAGIC) {
		/* PCA along span of edgeuse */
		eu = (struct edgeuse *)fpi->closest;

		if( nmg_find_eu_leftvec( left, eu ) < 0 ) {
			rt_log("%s[line:%d]: bad LEFT vector\n",
				__FILE__,__LINE__);
			rt_bomb("");
		}
		/* form vector "vupt" from vu to pt, if VDOT(vupt, left)
		 * is less than 0, pt is on "outside" of edge.
		 */
		VSUB2(vupt_v, pt, eu->vu_p->v_p->vg_p->coord);
		if (VDOT(vupt_v, left) >= 0.0)
			fpi->pt_class = NMG_CLASS_AinB;
		else
			fpi->pt_class = NMG_CLASS_AoutB;

	} else if (*fpi->closest == NMG_VERTEXUSE_MAGIC) {
		vu_p = (struct vertexuse *)fpi->closest;
		if (vu_p->up.magic_p == NMG_EDGEUSE_MAGIC) {
			/* point is off one end or the other of edge
			 * XXX
			 */
			Tanenbaum_patch(fpi, vu);

		} else if (vu_p->up.magic_p == NMG_LOOPUSE_MAGIC) {
			lu = (struct loopuse *)vu_p->up.magic_p;
			if (lu->orentation == OT_OPPOSITE)
				fpi->pt_class = NMG_CLASS_AinB;
			else if (lu->orentation == OT_SAME)
				fpi->pt_class = NMG_CLASS_AoutB;
			else {
				rt_log("%s[line:%d] Bad orientation "%s"for loopuse(vertex)\n",
					__FILE__, __LINE__,
					nmg_orientation(lu->orentation));
				rt_bomb("");
			}
		} else if (vu_p->up.magic_p == NMG_SHELL_MAGIC) {
			rt_log("%s[line:%d] vertexuse parent is SHELL???\nnmg_class_pt_fu_except(%g %g %g)\n",
				__FILE__, __LINE__,
				V3ARGS(pt));
			rt_bomb("");
		} else {
			rt_log("%s[line:%d] Bad vertexuse parent?\nnmg_class_pt_fu_except(%g %g %g)\n",
				__FILE__, __LINE__,
				V3ARGS(pt));
			rt_bomb("");
		}
	}
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
 */
struct fu_pt_info *
nmg_class_pt_fu_except(pt, fu, ignore_lu, eu_func, vu_func, onehit, tol)
point_t			pt;
CONST struct faceuse	*fu;
CONST struct loopuse	*ignore_lu;
void		(*eu_func)();	/* func to call when pt on edgeuse */
void		(*vu_func)();	/* func to call when pt on vertexuse*/
CONST int		onehit;		/* return after finding first hit */
CONST struct rt_tol	*tol;
{
	struct vertexuse	*vu_p;
	struct edgeuse		*eu;
	struct loopuse		*lu;
	struct fu_pt_info	*fpi;
	fastf_t			dist;
	vect_t			vupt_v;

	NMG_CK_FACEUSE(fu);
	NMG_CK_FACE(fu->f_p);
	NMG_CK_FACE_G(fu->f_p->fg_p);
	if(ignore_lu) NMG_CK_LOOPUSE(ignore_lu);
	RT_CK_TOL(tol);


	fpi = (struct fu_pt_info *)rt_malloc(sizeof(*fpi), "struct fu_pt_info");
	/* Validate distance from point to plane */
	NMG_GET_FU_PLANE( fpi->norm, fu );
	if( (dist=fabs(DIST_PT_PLANE( pt, fpi->norm ))) > tol->dist )  {
		rt_log("nmg_class_pt_f() ERROR, point (%g,%g,%g) not on face, dist=%g\n",
			V3ARGS(pt), dist );
	}


	fpi->tol = tol;
	fpi->fu_p = fu;
	fpi->pt = pt;
	fpi->pt_class = NMG_CLASS_Unknown;
	fpi->eu_func = eu_func;
	fpi->vu_func = vu_func;
	fpi->onehit = onehit;
	fpi->dist_in_plane = MAX_FASTF;
	fpi->closest = (long *)NULL;
	fpi->magic = NMG_FPI_MAGIC;

	for (RT_LIST_FOR(lu, loopuse, &fu->lu_hd)) {
		if( ignore_lu && (ignore_lu==lu || ignore_lu==lu->lumate_p) )
			continue;

		class_pt_lu(fpi, lu);

		/* If point lies ON loop edge, we are done */
		if( onehit && fpi->pt_class != NMG_CLASS_Unknown )  break;
	}


	if (fpi->pt_class == NMG_CLASS_AinB ||
	    fpi->pt_class == NMG_CLASS_AoutB ||
	    fpi->pt_class == NMG_CLASS_AonBshared)
		return fpi;

	if (fpi->pt_class != NMG_CLASS_Unknown) {
		rt_log("%s[line:$d]: bad classification "%s" for point in face?\n",
			__FILE__, __LINE__, nmg_class_name(fpi->pt_class));
		rt_bomb("");
	}

	deduce_pt_class(fpi);

	return fpi;
}
