/*
 *			N M G _ C L A S S . C
 *
 *	NMG classifier code
 *
 *
 *  Author -
 *	Lee A. Butler
 *  
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1989 by the United States Army.
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
#include "rtlist.h"
#include "raytrace.h"

#define INSIDE	32
#define ON_SURF	64
#define OUTSIDE	128

/*	Structure for keeping track of how close a point/vertex is to
 *	its potential neighbors.
 */
struct neighbor {
	union {
		struct vertexuse *vu;
		struct edgeuse *eu;
	} p;
	fastf_t dist;	/* distance from point to neighbor */
	int inout;	/* what the neighbor thought about the point */
};

static void	joint_hitmiss2 RT_ARGS( (struct edgeuse	*eu, point_t pt,
			struct neighbor *closest, long *novote) );
static void	pt_hitmis_e RT_ARGS( (point_t pt, struct edgeuse *eu,
			struct neighbor	*closest, CONST struct rt_tol *tol,
			long *novote) );
static void	pt_hitmis_l RT_ARGS( (point_t pt, struct loopuse *lu,
			struct neighbor	*closest, CONST struct rt_tol *tol,
			long *novote) );
RT_EXTERN(int		nmg_pt_hitmis_f, (point_t pt, struct faceuse *fu,
			CONST struct rt_tol *tol, long *novote) );
static int	pt_inout_s RT_ARGS( (point_t pt, struct shell *s,
			CONST struct rt_tol *tol) );
static int	class_vu_vs_s RT_ARGS( (struct vertexuse *vu, struct shell *sB,
			long *classlist[4], CONST struct rt_tol	*tol) );
static int	class_eu_vs_s RT_ARGS( (struct edgeuse *eu, struct shell *s,
			long *classlist[4], CONST struct rt_tol	*tol) );
static int	class_lu_vs_s RT_ARGS( (struct loopuse *lu, struct shell *s,
			long *classlist[4], CONST struct rt_tol	*tol) );
static void	class_fu_vs_s RT_ARGS( (struct faceuse *fu, struct shell *s,
			long *classlist[4], CONST struct rt_tol	*tol) );

static vect_t projection_dir = { 1.0, 0.0, 0.0 };

#define FACE_HIT 256
#define FACE_MISS 512

void
nmg_pr_class_status( prefix, status )
char	*prefix;
int	status;
{
	char	*str;

	switch(status)  {
	case INSIDE:
		str = "INSIDE";
		break;
	case OUTSIDE:
		str = "OUTSIDE";
		break;
	case ON_SURF:
		str = "ON_SURF";
		break;
	default:
		str = "??unknown_code??";
		break;
	}
	rt_log("%s has classification status %s\n", prefix, str);
}

/*
 *			J O I N T _ H I T M I S S 2
 *
 *	The ray hit an edge.  We have to decide whether it hit the
 *	edge, or missed it.
 */
static void joint_hitmiss2(eu, pt, closest, novote)
struct edgeuse	*eu;
point_t		pt;
struct neighbor *closest;
long		*novote;
{
	struct edgeuse *eu_rinf, *eu_r, *mate_r;
	struct edgeuse	*p;
	fastf_t	*N1, *N2;
	fastf_t PdotN1, PdotN2;

	eu_rinf = nmg_faceradial(eu);

	if (rt_g.NMG_debug & DEBUG_CLASSIFY) {
		EUPRINT("Easy question time", eu);
		VPRINT("Point", pt);
	}
	if( eu_rinf == eu )  {
		rt_bomb("joint_hitmiss2: radial eu is me?\n");
	}
	/* If eu_rinf == eu->eumate_p, thats OK, this is a dangling face,
	 * or a face that has not been fully hooked up yet.
	 * It's OK as long the the orientations both match.
	 */
	if( eu->up.lu_p->orientation == eu_rinf->up.lu_p->orientation ) {
		if (eu->up.lu_p->orientation == OT_SAME) {
			closest->dist = 0.0;
			closest->p.eu = eu;
			closest->inout = FACE_HIT;
		} else if (eu->up.lu_p->orientation == OT_OPPOSITE) {
			closest->dist = 0.0;
			closest->p.eu = eu;
			closest->inout = FACE_MISS;
		} else rt_bomb("joint_hitmiss2: bad loop orientation\n");
	    	return;
	}

	if (rt_g.NMG_debug & (DEBUG_CLASSIFY|DEBUG_NMGRT) )
		EUPRINT("Hard question time", eu);

	rt_log("nmg intersection: assuming miss\n");
	rt_log(" eu_rinf=x%x, eu->eumate_p=x%x, eu=x%x\n", eu_rinf, eu->eumate_p, eu);
	rt_log(" eu lu orient=%s, eu_rinf lu orient=%s\n",
		nmg_orientation(eu->up.lu_p->orientation),
		nmg_orientation(eu_rinf->up.lu_p->orientation) );
}

/*
 *			P T _ H I T M I S _ E
 *
 *	Given a point and an edgeuse, determine if the point is
 *	closer to this edgeuse than anything it's been compared with
 *	before.  If it is, record how close the point is to the edgeuse
 *	and whether it is on the inside of the face area bounded by the
 *	edgeuse or on the outside of the face area.
 *
 */
static void pt_hitmis_e(pt, eu, closest, tol, novote)
point_t		pt;
struct edgeuse	*eu;
struct neighbor	*closest;
CONST struct rt_tol	*tol;
long		*novote;
{
	vect_t	N,	/* plane normal */
		euvect,	/* vector of edgeuse */
		plvec,	/* vector into face from edge */
		ptvec;	/* vector from lseg to pt */
	pointp_t eupt, matept;
	point_t pca;	/* point of closest approach from pt to edge lseg */
	fastf_t dist;	/* distance from pca to pt */
	fastf_t dot, mag;

	NMG_CK_EDGEUSE(eu);
	NMG_CK_EDGEUSE(eu->eumate_p);
	NMG_CK_VERTEX_G(eu->vu_p->v_p->vg_p);
	NMG_CK_VERTEX_G(eu->eumate_p->vu_p->v_p->vg_p);

	eupt = eu->vu_p->v_p->vg_p->coord;
	matept = eu->eumate_p->vu_p->v_p->vg_p->coord;

	if (rt_g.NMG_debug & DEBUG_CLASSIFY) {
		VPRINT("pt_hitmis_e\tProjected pt", pt);
		EUPRINT("          \tVS. eu", eu);
	}
	dist = rt_dist_pt_lseg(pca, eupt, matept, pt, tol);
	if( dist < 0.0 )  rt_log("pt_hitmis_e: neg dist=%g?\n", dist);

	if (rt_g.NMG_debug & DEBUG_CLASSIFY) {
		rt_log("          \tdist: %g\n", dist);
		VPRINT("          \tpca:", pca);
	}

	if (dist < closest->dist) {

		if (rt_g.NMG_debug & DEBUG_CLASSIFY) {
				EUPRINT("\t\tcloser to edgeuse", eu);
				rt_log("\t\tdistance: %g\n", dist);
		}

		if (*eu->up.magic_p == NMG_LOOPUSE_MAGIC &&
		    *eu->up.lu_p->up.magic_p == NMG_FACEUSE_MAGIC) {

		    	if (NEAR_ZERO(dist, tol->dist)) {
		    		/* The ray has hit the edge! */
		    		if (rt_g.NMG_debug & DEBUG_CLASSIFY) {
			    		VPRINT("Vertex", pt);
			    		EUPRINT("hits edge, calling Joint_HitMiss", eu);
			    		rt_log("distance: %g   tol: %g\n", dist, tol->dist);
		    		}

		    		joint_hitmiss2(eu, pt, closest, novote);
		    		return;
		    	} else {
		    		if (rt_g.NMG_debug & DEBUG_CLASSIFY) {
			    		VPRINT("Vertex", pt);
			    		EUPRINT("Misses edge", eu);
			    		rt_log("distance: %g   tol: %g\n", dist, tol->dist);
		    		}
		    	}

			/* calculate in/out */
			VMOVE(N, eu->up.lu_p->up.fu_p->f_p->fg_p->N);
		    	if (eu->up.lu_p->up.fu_p->orientation != OT_SAME) {
				VREVERSE(N,N);
		    	}
			VSUB2(euvect, matept, eupt);

			mag = MAGSQ(euvect);
			while (mag < 1.0 && mag > 0.0) {
				VSCALE(euvect, euvect, 10.0);
				mag = MAGSQ(euvect);
			}

		    	/* Get vector which lies on the plane, and points
		    	 * left, towards the interior of the CCW loop.
		    	 */
		    	VCROSS( plvec, N, euvect );
			VSUB2(ptvec, pt, pca);

			mag = MAGSQ(ptvec);
			while (mag < 1.0 && mag > 0.0) {
				VSCALE(ptvec, ptvec, 10.0);
				mag = MAGSQ(ptvec);
			}

			dot = VDOT(plvec, ptvec);
			if (dot >= 0.0) {
				closest->dist = dist;
				closest->p.eu = eu;
				closest->inout = FACE_HIT;
			    	if (rt_g.NMG_debug & DEBUG_CLASSIFY)
			    		rt_log("\tHIT\n");
			} else if (dot < 0.0) {
				closest->dist = dist;
				closest->p.eu = eu;
				closest->inout = FACE_MISS;
			    	if (rt_g.NMG_debug & DEBUG_CLASSIFY)
			    		rt_log("\tMISS\n");
			}
		} else {
			if (rt_g.NMG_debug & DEBUG_CLASSIFY)
				rt_log("Trying to classify a pt (%g, %g, %g)\n\tvs a wire edge? (%g, %g, %g -> %g, %g, %g)\n",
					pt[0], pt[1], pt[2],
					eu->vu_p->v_p->vg_p->coord[0],
					eu->vu_p->v_p->vg_p->coord[1],
					eu->vu_p->v_p->vg_p->coord[2],
					eu->eumate_p->vu_p->v_p->vg_p->coord[0],
					eu->eumate_p->vu_p->v_p->vg_p->coord[1],
					eu->eumate_p->vu_p->v_p->vg_p->coord[2]
				);
		}

	}
}


/*
 *			P T _ H I T M I S _ L
 *
 *	Given a point on the plane of the loopuse, determine if the point
 *	is in, or out of the area of the loop
 */
static void pt_hitmis_l(pt, lu, closest, tol, novote)
point_t		pt;
struct loopuse	*lu;
struct neighbor	*closest;
CONST struct rt_tol	*tol;
long		*novote;
{
	vect_t	delta;
	pointp_t lu_pt;
	fastf_t dist;
	struct edgeuse *eu;
	char *name;

	NMG_CK_LOOPUSE(lu);

	if (rt_g.NMG_debug & DEBUG_CLASSIFY)
		VPRINT("pt_hitmis_loop\tProjected Pt:", pt);

	if (*lu->up.magic_p != NMG_FACEUSE_MAGIC)
		return;

	
	if (RT_LIST_FIRST_MAGIC(&lu->down_hd) == NMG_EDGEUSE_MAGIC) {
		for (RT_LIST_FOR(eu, edgeuse, &lu->down_hd)) {
			pt_hitmis_e(pt, eu, closest, tol, novote);
		}

	} else if (RT_LIST_FIRST_MAGIC(&lu->down_hd) == NMG_VERTEXUSE_MAGIC) {
		register struct vertexuse *vu;
		vu = RT_LIST_FIRST(vertexuse, &lu->down_hd);

		lu_pt = vu->v_p->vg_p->coord;
		VSUB2(delta, pt, lu_pt);
		dist = MAGNITUDE(delta);

		if (dist < closest->dist) {

			if (lu->orientation == OT_OPPOSITE) {
				closest->inout = FACE_HIT; name = "HIT";
			} else if (lu->orientation == OT_SAME) {
				closest->inout = FACE_MISS; name = "MISS";
			} else {
				rt_log("bad orientation for face loopuse\n");
				nmg_pr_orient(lu->orientation, "\t");
				rt_bomb("pt_hitmis_l: BAD NMG\n");
			}

			if (rt_g.NMG_debug & DEBUG_CLASSIFY)
				rt_log("point (%g, %g, %g) closer to lupt (%g, %g, %g)\n\tdist %g %s\n",
					pt[0], pt[1], pt[2],
					lu_pt[0], lu_pt[1], lu_pt[2],
					dist, name);

			closest->dist = dist;
			closest->p.vu = vu;
		}
	} else {
		rt_log("%s at %d bad child of loopuse\n", __FILE__,
			__LINE__);
		rt_bomb("pt_hitmis_l\n");
	}
}

/*
 *			P T _ H I T M I S _ F
 *
 *	Given a face and a point on the plane of the face, determine if
 *	the point is in or out of the area bounded by the face.
 *
 *	Explicit Return
 *		1	point is ON or IN the area of the face
 *		0	point is outside the area of the face
 */
int
nmg_pt_hitmis_f(pt, fu, tol, novote)
point_t		pt;
struct faceuse	*fu;
CONST struct rt_tol	*tol;
long		*novote;
{
	struct loopuse *lu;
	struct neighbor closest;

	/* if this is a non-manifold (dangling) face of the shell, don't
	 * count any potential hit
	 */

	if (rt_g.NMG_debug & DEBUG_CLASSIFY) {
		VPRINT("nmg_pt_hitmis_f\tProjected Pt:", pt);
	}


	/* find the closest approach in this face to the point */
	closest.dist = MAX_FASTF;
	closest.p.eu = (struct edgeuse *)NULL;

	for (RT_LIST_FOR(lu, loopuse, &fu->lu_hd)) {
		pt_hitmis_l(pt, lu, &closest, tol, novote);
	}

	if (closest.inout == FACE_HIT) return(1);
	return(0);
}


/*
 *			P T _ H I T M I S _ S
 *
 *	returns status (inside/outside/on_surface) of pt WRT shell.
 */
static pt_inout_s(pt, s, tol)
point_t pt;
struct shell *s;
CONST struct rt_tol	*tol;
{
	int		hitcount = 0;
	int		stat;
	fastf_t 	dist;
	point_t 	plane_pt;
	struct faceuse	*fu;
	struct model	*m;
	long		*novote; /* faces that can't vote in a hit list */

	RT_CK_TOL(tol);

	if (rt_g.NMG_debug & DEBUG_CLASSIFY)
		VPRINT("pt_inout_s: ", pt);

	if (! NMG_EXTENT_OVERLAP(s->sa_p->min_pt,s->sa_p->max_pt,pt,pt) )  {
		if (rt_g.NMG_debug & DEBUG_CLASSIFY)
			rt_log("	OUTSIDE, extents don't overlap\n");

		return(OUTSIDE);
	}

	m = s->r_p->m_p;
	NMG_CK_MODEL(m);
	novote = (long *)rt_calloc( m->maxindex, sizeof(long), "pt_inout_s novote[]" );

	if (rt_g.NMG_debug & DEBUG_CLASSIFY)
		VPRINT("\tFiring vector:", projection_dir);

	fu = RT_LIST_FIRST(faceuse, &s->fu_hd);
	while (RT_LIST_NOT_HEAD(fu, &s->fu_hd)) {

		/* catch some (but not all) non-voters before they reach
		 * the polling booth
		 */
		if (nmg_dangling_face(fu)) {
			NMG_INDEX_SET(novote, fu->f_p);
/***			(void)nmg_tbl(&novote, TBL_INS, &fu->f_p->magic);**/

			if (RT_LIST_PNEXT(faceuse, fu) == fu->fumate_p)
				fu = RT_LIST_PNEXT_PNEXT(faceuse, fu);
			else
				fu = RT_LIST_PNEXT(faceuse, fu);

			continue;
		}

		/* since the above block of code isn't the only place that
		 * faces get put in the list, we need this here too
		 */
/***		if (nmg_tbl(&novote, TBL_LOC, &fu->f_p->magic) >= 0)  ***/
		if( NMG_INDEX_TEST( novote, fu->f_p ) )  {
			if (RT_LIST_PNEXT(faceuse, fu) == fu->fumate_p)
				fu = RT_LIST_PNEXT_PNEXT(faceuse, fu);
			else
				fu = RT_LIST_PNEXT(faceuse, fu);

			continue;
		}

		stat = rt_isect_ray_plane(&dist, pt, projection_dir,
			fu->f_p->fg_p->N);

		if (rt_g.NMG_debug & DEBUG_CLASSIFY) {
			rt_log("\tray/plane: stat:%d dist:%g\n", stat, dist);
			PLPRINT("\tplane", fu->f_p->fg_p->N);
		}

		if (stat >= 0 && dist >= 0) {
			/* compare extent of face to projection of pt on plane
			*/
			if (pt[Y] >= fu->f_p->fg_p->min_pt[Y] &&
			    pt[Y] <= fu->f_p->fg_p->max_pt[Y] &&
			    pt[Z] >= fu->f_p->fg_p->min_pt[Z] &&
			    pt[Z] <= fu->f_p->fg_p->max_pt[Z]) {
			    	/* translate point into plane of face and
			    	 * determine hit.
			    	 */
			    	VJOIN1(plane_pt, pt, dist,projection_dir);

				if (!nmg_dangling_face(fu)) {
					hitcount += nmg_pt_hitmis_f(plane_pt,
						fu, tol, novote);
				} else {
					if (rt_g.NMG_debug & DEBUG_CLASSIFY)
						rt_log("\tnon-manifold face\n");
					return(0);
				}


			}

		}

		if (RT_LIST_PNEXT(faceuse, fu) == fu->fumate_p)
			fu = RT_LIST_PNEXT_PNEXT(faceuse, fu);
		else
			fu = RT_LIST_PNEXT(faceuse, fu);
	}

	rt_free( (char *)novote, "pt_inout_s novote[]" );

	/* if the hitcount is even, we're outside.  if it's odd, we're inside
	 */
	if (hitcount & 1) {
		if (rt_g.NMG_debug & DEBUG_CLASSIFY)
			rt_log("	INSIDE face / pt (%g, %g, %g) hitcount: %d\n",
			pt[0], pt[1], pt[2], hitcount);

		return(INSIDE);
	}

	if (rt_g.NMG_debug & DEBUG_CLASSIFY)
		rt_log("	OUTSIDE face / pt (%g, %g, %g) hitcount: %d\n",
			pt[0], pt[1], pt[2], hitcount);

	return(OUTSIDE);
}


/*
 *			C L A S S _ V U _ V S _ S
 *
 *	Classify a loopuse/vertexuse from shell A WRT shell B.
 */
static int class_vu_vs_s(vu, sB, classlist, tol)
struct vertexuse	*vu;
struct shell		*sB;
long			*classlist[4];
CONST struct rt_tol	*tol;
{
	struct vertexuse *vup;
	pointp_t pt;
	int status;

	NMG_CK_VERTEXUSE(vu);
	NMG_CK_SHELL(sB);
	RT_CK_TOL(tol);

	pt = vu->v_p->vg_p->coord;

	if (rt_g.NMG_debug & DEBUG_CLASSIFY)
		VPRINT("class_vu_vs_s ", pt);

	/* check for vertex presence in class list */
	if( NMG_INDEX_TEST(classlist[NMG_CLASS_AinB], vu->v_p) )
		return(INSIDE);

	if( NMG_INDEX_TEST(classlist[NMG_CLASS_AonBshared], vu->v_p) )
		return(ON_SURF);

	if( NMG_INDEX_TEST(classlist[NMG_CLASS_AinB], vu->v_p) )
		return(OUTSIDE);

	/* we use topology to determing if the vertex is "ON" the
	 * other shell.
	 */
	for(RT_LIST_FOR(vup, vertexuse, &vu->v_p->vu_hd)) {

		if (*vup->up.magic_p == NMG_LOOPUSE_MAGIC &&
		    nmg_lups(vup->up.lu_p) == sB) {

		    	/* it's ON_SURF */
		    	NMG_INDEX_SET(classlist[NMG_CLASS_AonBshared], 
		    		vu->v_p );
			if (rt_g.NMG_debug & DEBUG_CLASSIFY)
		    		VPRINT("\tVertex is ON_SURF", pt);

			return(ON_SURF);
		}
		else if (*vup->up.magic_p == NMG_EDGEUSE_MAGIC &&
		    nmg_eups(vup->up.eu_p) == sB) {

		    	NMG_INDEX_SET(classlist[NMG_CLASS_AonBshared],
		    		vu->v_p );
			if (rt_g.NMG_debug & DEBUG_CLASSIFY)
		    		VPRINT("\tVertex is ON_SURF", pt);

			return(ON_SURF);
		}
	}

	if (rt_g.NMG_debug & DEBUG_CLASSIFY)
		rt_log("\tCan't classify vertex via topology\n");

	/* The topology doesn't tell us about the vertex being "on" the shell
	 * so now it's time to look at the geometry to determine the vertex
	 * relationsship to the shell.
	 *
	 *  We know that the vertex doesn't lie ON any of the faces or
	 * edges, since the intersection algorithm would have combined the
	 * topology if that had been the case.
	 */

	status = pt_inout_s(pt, sB, tol);
	
	if (status == OUTSIDE)  {
		NMG_INDEX_SET(classlist[NMG_CLASS_AoutB], vu->v_p);
	}  else if (status == INSIDE)  {
		NMG_INDEX_SET(classlist[NMG_CLASS_AinB], vu->v_p);
	}  else  {
		rt_log("status=%d\n", status);
		VPRINT("pt", pt);
		rt_bomb("class_vu_vs_s: Why wasn't this point in or out?\n");
	}

	if (rt_g.NMG_debug & DEBUG_CLASSIFY) {
		if (status == OUTSIDE)
			VPRINT("Vertex is OUTSIDE ", pt);
		else if (status == INSIDE)
			VPRINT("Vertex is INSIDE ", pt);
	}

	return(status);
}

/*
 *			C L A S S _ E U _ V S _ S
 */
static int class_eu_vs_s(eu, s, classlist, tol)
struct edgeuse	*eu;
struct shell	*s;
long		*classlist[4];
CONST struct rt_tol	*tol;
{
	int euv_cl, matev_cl, status;
	struct edgeuse *eup;
	point_t pt;
	pointp_t eupt, matept;

	if (rt_g.NMG_debug & DEBUG_CLASSIFY)
		EUPRINT("class_eu_vs_s\t\t", eu);

	NMG_CK_EDGEUSE(eu);	
	NMG_CK_SHELL(s);	
	
	/* check to see if edge is already in one of the lists */
	if( NMG_INDEX_TEST(classlist[NMG_CLASS_AinB], eu->e_p) )
		return(INSIDE);

	if( NMG_INDEX_TEST(classlist[NMG_CLASS_AonBshared], eu->e_p) )
		return(ON_SURF);

	if( NMG_INDEX_TEST(classlist[NMG_CLASS_AinB], eu->e_p) )
		return(OUTSIDE);

	euv_cl = class_vu_vs_s(eu->vu_p, s, classlist, tol);
	matev_cl = class_vu_vs_s(eu->eumate_p->vu_p, s, classlist, tol);
	
	/* sanity check */
	if ((euv_cl == INSIDE && matev_cl == OUTSIDE) ||
	    (euv_cl == OUTSIDE && matev_cl == INSIDE)) {
	    	nmg_pr_class_status("eu vu", euv_cl);
	    	nmg_pr_class_status("eumate vu", matev_cl);
	    	/* Do them over, so we can watch */
		rt_g.NMG_debug |= DEBUG_CLASSIFY;
		(void)class_vu_vs_s(eu->vu_p, s, classlist, tol);
		(void)class_vu_vs_s(eu->eumate_p->vu_p, s, classlist, tol);

	    	EUPRINT("didn't this edge get cut?", eu);
	    	nmg_pr_eu(eu, "  ");
		rt_bomb("class_eu_vs_s\n");
	    }

	if (euv_cl == ON_SURF && matev_cl == ON_SURF) {
		/* check for radial uses of this edge by the shell */

		eup = eu->radial_p->eumate_p;
		do {
			NMG_CK_EDGEUSE(eup);
			if (nmg_eups(eup) == s) {
				NMG_INDEX_SET(classlist[NMG_CLASS_AonBshared],
					eu->e_p );
				return(ON_SURF);
			}

			eup = eup->radial_p->eumate_p;
		} while (eup != eu->radial_p->eumate_p);

		/* although the two endpoints are "on" the shell,
		 * the edge would appear to be either "inside" or "outside",
		 * since there are no uses of this edge in the shell.
		 *
		 * So we classify the midpoint of the line WRT the shell.
		 */
		
		eupt = eu->vu_p->v_p->vg_p->coord;
		matept = eu->eumate_p->vu_p->v_p->vg_p->coord;

		VADD2SCALE(pt, eupt, matept, 0.5);

		if (rt_g.NMG_debug & DEBUG_CLASSIFY)
			VPRINT("Classifying midpoint of edge", pt);

		status = pt_inout_s(pt, s, tol);
	
		if (status == OUTSIDE)  {
			NMG_INDEX_SET(classlist[NMG_CLASS_AoutB], eu->e_p);
		}  else if (status == INSIDE)  {
			NMG_INDEX_SET(classlist[NMG_CLASS_AinB], eu->e_p);
		}  else  {
			EUPRINT("Why wasn't this edge in or out?", eu);
			rt_bomb("class_eu_vs_s");
		}

		return(status);
	}

	if (euv_cl == OUTSIDE || matev_cl == OUTSIDE) {
		NMG_INDEX_SET(classlist[NMG_CLASS_AoutB], eu->e_p);
		return(OUTSIDE);
	}
	NMG_INDEX_SET(classlist[NMG_CLASS_AinB], eu->e_p);
	return(INSIDE);
}

/*
 *			C L A S S _ L U _ V S _ S
 */
static int class_lu_vs_s(lu, s, classlist, tol)
struct loopuse		*lu;
struct shell		*s;
long			*classlist[4];
CONST struct rt_tol	*tol;
{
	int class;
	unsigned in, out, on;
	struct edgeuse *eu, *p, *q;
	struct loopuse *q_lu;
	struct vertexuse *vu;
	long		magic1;

	NMG_CK_LOOPUSE(lu);
	NMG_CK_SHELL(s);

	/* check to see if loop is already in one of the lists */
	if( NMG_INDEX_TEST(classlist[NMG_CLASS_AinB], lu->l_p) )
		return(INSIDE);

	if( NMG_INDEX_TEST(classlist[NMG_CLASS_AonBshared], lu->l_p) )
		return(ON_SURF);

	if( NMG_INDEX_TEST(classlist[NMG_CLASS_AinB], lu->l_p) )
		return(OUTSIDE);

	magic1 = RT_LIST_FIRST_MAGIC( &lu->down_hd );
	if (magic1 == NMG_VERTEXUSE_MAGIC) {
		vu = RT_LIST_PNEXT( vertexuse, &lu->down_hd );
		NMG_CK_VERTEXUSE(vu);
		class = class_vu_vs_s(vu, s, classlist, tol);
		switch (class) {
		case INSIDE:
			NMG_INDEX_SET(classlist[NMG_CLASS_AinB], lu->l_p);
			break;
		case OUTSIDE:
			NMG_INDEX_SET(classlist[NMG_CLASS_AoutB], lu->l_p);
			 break;
		case ON_SURF:
			NMG_INDEX_SET(classlist[NMG_CLASS_AonBshared], lu->l_p);
			break;
		default:
			rt_bomb("class_lu_vs_s: bad vertexloop classification\n");
		}
		return(class);
	} else if (magic1 != NMG_EDGEUSE_MAGIC) {
		rt_bomb("class_lu_vs_s: bad child of loopuse\n");
	}

	/* loop is collection of edgeuses */
	in = out = on = 0;
	for (RT_LIST_FOR(eu, edgeuse, &lu->down_hd)) {

		class = class_eu_vs_s(eu, s, classlist, tol);
		switch (class) {
		case INSIDE	: ++in; 
				if (rt_g.NMG_debug & DEBUG_CLASSIFY)
					EUPRINT("Edge is INSIDE", eu);
				break;
		case OUTSIDE	: ++out;
				if (rt_g.NMG_debug & DEBUG_CLASSIFY)
					EUPRINT("Edge is OUTSIDE", eu);
				break;
		case ON_SURF	: ++on;
				if (rt_g.NMG_debug & DEBUG_CLASSIFY)
					EUPRINT("Edge is ON_SURF", eu);
				break;
		default		: rt_bomb("class_lu_vs_s: bad class for edgeuse\n");
		}
	}

	if (rt_g.NMG_debug & DEBUG_CLASSIFY)
		rt_log("Loopuse edges in:%d on:%d out:%d\n", in, on, out);

	if (in > 0 && out > 0) {
		rt_log("Loopuse edges in:%d on:%d out:%d\n", in, on, out);
		for(RT_LIST_FOR(eu, edgeuse, &lu->down_hd)) {
			EUPRINT("edgeuse", eu);
		}

		rt_bomb("class_lu_vs_s: loop crosses face?\n");
	}
	if (out > 0) {
		NMG_INDEX_SET(classlist[NMG_CLASS_AoutB], lu->l_p);
		return(OUTSIDE);
	} else if (in > 0) {
		NMG_INDEX_SET(classlist[NMG_CLASS_AinB], lu->l_p);
		return(INSIDE);
	} else if (on == 0)
		rt_bomb("class_lu_vs_s: alright, who's the wiseguy that stole my edgeuses?\n");


	if (rt_g.NMG_debug & DEBUG_CLASSIFY)
		rt_log("\tAll edgeuses of loop are ON");

	/* since all of the edgeuses of this loop are "on" the other shell,
	 * we need to see if this loop is "on" the other shell
	 *
	 * if we're a wire edge loop, simply having all edges "on" is
	 *	sufficient.
	 *
	 * foreach radial edgeuse
	 * 	if edgeuse vertex is same and edgeuse parent shell is the one
	 *	 	desired, then....
	 *
	 *		p = edgeuse, q = radial edgeuse
	 *		while p's vertex equals q's vertex and we
	 *			haven't come full circle
	 *			move p and q forward
	 *		if we made it full circle, loop is on
	 */

	if (*lu->up.magic_p == NMG_SHELL_MAGIC) {
		NMG_INDEX_SET(classlist[NMG_CLASS_AonBshared], lu->l_p);
		return(ON_SURF);
	}

	NMG_CK_FACEUSE(lu->up.fu_p);

	eu = RT_LIST_FIRST(edgeuse, &lu->down_hd);
	eu = eu->radial_p->eumate_p;
	do {
		/* if the radial edge is a part of a loop which is part of
		 * a face, then it's one that we might be "on"
		 */
		if (*eu->up.magic_p == NMG_LOOPUSE_MAGIC && 
		    *eu->up.lu_p->up.magic_p == NMG_FACEUSE_MAGIC && 
		    eu->up.lu_p->up.fu_p->s_p == s ) {

			p = RT_LIST_FIRST(edgeuse, &lu->down_hd);
			q = eu;
			q_lu = q->up.lu_p;

			/* get the starting vertex of each edgeuse to be the
			 * same.
			 */
			if (q->vu_p->v_p != p->vu_p->v_p) {
				q = eu->eumate_p;
				if (q->vu_p->v_p != p->vu_p->v_p)
					rt_bomb("class_lu_vu_s: radial edgeuse doesn't share verticies\n");
			
			}

		    	/* march around the two loops to see if they 
		    	 * are the same all the way around.
		    	 */
		    	while (p->vu_p->v_p == q->vu_p->v_p &&
			    q->up.lu_p == q_lu && /* sanity check */
		    	    RT_LIST_NOT_HEAD(p, &lu->down_hd) ) {
				q = RT_LIST_PNEXT_CIRC(edgeuse, &q->l);
				p = RT_LIST_PNEXT(edgeuse, p);
			}

			if (!RT_LIST_NOT_HEAD(p, &lu->down_hd)) {

				/* the two loops are "on" each other.  All
				 * that remains is to determine
				 * shared/anit-shared status.
				 */
				NMG_CK_FACE_G(q_lu->up.fu_p->f_p->fg_p);
				if (VDOT(q_lu->up.fu_p->f_p->fg_p->N,
				    lu->up.fu_p->f_p->fg_p->N) >= 0) {
				    	NMG_INDEX_SET(classlist[NMG_CLASS_AonBshared],
				    		lu->l_p );
					if (rt_g.NMG_debug & DEBUG_CLASSIFY)
						rt_log("Loop is on-shared\n");
				} else {
					NMG_INDEX_SET(classlist[NMG_CLASS_AonBanti],
						lu->l_p );
					if (rt_g.NMG_debug & DEBUG_CLASSIFY)
						rt_log("Loop is on-antishared\n");
				}

				return(ON_SURF);
			}

		}
		eu = eu->radial_p->eumate_p;
	} while (eu != RT_LIST_FIRST(edgeuse, &lu->down_hd) );



	/* OK, the edgeuses are all "on", but the loop isn't.  Time to
	 * decide if the loop is "out" or "in".  To do this, we look for
	 * an edgeuse radial to one of the edgeuses in the loop which is
	 * a part of a face in the other shell.  If/when we find such a
	 * radial edge, we check the direction (in/out) of the faceuse normal.
	 * if the faceuse normal is pointing out of the shell, we are outside.
	 * if the faceuse normal is pointing into the shell, we are inside.
	 */

	for (RT_LIST_FOR(eu, edgeuse, &lu->down_hd)) {

		p = eu->radial_p;
		do {
			if (*p->up.magic_p == NMG_LOOPUSE_MAGIC &&
			    *p->up.lu_p->up.magic_p == NMG_FACEUSE_MAGIC &&
			    p->up.lu_p->up.fu_p->s_p == s) {

			    	if (p->up.lu_p->up.fu_p->orientation == OT_OPPOSITE) {
			    		NMG_INDEX_SET(classlist[NMG_CLASS_AinB],
			    			lu->l_p );
					if (rt_g.NMG_debug & DEBUG_CLASSIFY)
						rt_log("Loop is INSIDE\n");
					return(INSIDE);
			    	} else if (p->up.lu_p->up.fu_p->orientation == OT_SAME) {
			    		NMG_INDEX_SET(classlist[NMG_CLASS_AoutB],
			    			lu->l_p );
					if (rt_g.NMG_debug & DEBUG_CLASSIFY)
						rt_log("Loop is OUTSIDE\n");
					return(OUTSIDE);
			    	}

			}
			p = p->eumate_p->radial_p;
		} while (p != eu->eumate_p);

	}

	if (rt_g.NMG_debug & DEBUG_CLASSIFY)
		rt_log("Loop is OUTSIDE 'cause it isn't anything else\n");
	

	/* Since we didn't find any radial faces to classify ourselves against
	 * and we already know that the edges are all "on" that must mean that
	 * the loopuse is "on" a wireframe portion of the shell.
	 */
	NMG_INDEX_SET( classlist[NMG_CLASS_AoutB], lu->l_p );
	return(OUTSIDE);
}

/*
 *			C L A S S _ F U _ V S _ S
 */
static void class_fu_vs_s(fu, s, classlist, tol)
struct faceuse		*fu;
struct shell		*s;
long			*classlist[4];
CONST struct rt_tol	*tol;
{
	struct loopuse *lu;
	
	NMG_CK_FACEUSE(fu);
	NMG_CK_SHELL(s);

	if (rt_g.NMG_debug & DEBUG_CLASSIFY)
        	PLPRINT("\nclass_fu_vs_s plane equation:", fu->f_p->fg_p->N);

	for (RT_LIST_FOR(lu, loopuse, &fu->lu_hd))
		(void)class_lu_vs_s(lu, s, classlist, tol);

}

/*
 *			N M G _ C L A S S _ S H E L L S
 *
 *	Classify one shell WRT the other shell
 */
void
nmg_class_shells(sA, sB, classlist, tol)
struct shell	*sA;
struct shell	*sB;
long		*classlist[4];
CONST struct rt_tol	*tol;
{
	struct faceuse *fu;
	struct loopuse *lu;
	struct edgeuse *eu;

	RT_CK_TOL(tol);

	if (rt_g.NMG_debug & DEBUG_CLASSIFY &&
	    RT_LIST_NON_EMPTY(&sA->fu_hd))
		rt_log("class_shells - doing faces\n");

	fu = RT_LIST_FIRST(faceuse, &sA->fu_hd);
	while (RT_LIST_NOT_HEAD(fu, &sA->fu_hd)) {

		class_fu_vs_s(fu, sB, classlist, tol);

		if (RT_LIST_PNEXT(faceuse, fu) == fu->fumate_p)
			fu = RT_LIST_PNEXT_PNEXT(faceuse, fu);
		else
			fu = RT_LIST_PNEXT(faceuse, fu);
	}
	
	if (rt_g.NMG_debug & DEBUG_CLASSIFY &&
	    RT_LIST_NON_EMPTY(&sA->lu_hd))
		rt_log("class_shells - doing loops\n");

	lu = RT_LIST_FIRST(loopuse, &sA->lu_hd);
	while (RT_LIST_NOT_HEAD(lu, &sA->lu_hd)) {

		(void)class_lu_vs_s(lu, sB, classlist, tol);

		if (RT_LIST_PNEXT(loopuse, lu) == lu->lumate_p)
			lu = RT_LIST_PNEXT_PNEXT(loopuse, lu);
		else
			lu = RT_LIST_PNEXT(loopuse, lu);
	}

	if (rt_g.NMG_debug & DEBUG_CLASSIFY &&
	    RT_LIST_NON_EMPTY(&sA->eu_hd))
		rt_log("class_shells - doing edges\n");

	eu = RT_LIST_FIRST(edgeuse, &sA->eu_hd);
	while (RT_LIST_NOT_HEAD(eu, &sA->eu_hd)) {

		(void)class_eu_vs_s(eu, sB, classlist, tol);

		if (RT_LIST_PNEXT(edgeuse, eu) == eu->eumate_p)
			eu = RT_LIST_PNEXT_PNEXT(edgeuse, eu);
		else
			eu = RT_LIST_PNEXT(edgeuse, eu);
	}

	if (sA->vu_p) {
		if (rt_g.NMG_debug)
			rt_log("class_shells - doing vertex\n");
		(void)class_vu_vs_s(sA->vu_p, sB, classlist, tol);
	}
}
