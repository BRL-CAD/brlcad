#define NEWLINE 1	/* "New" algorithm:  isect line with 2 faces in 2d */

/*
 *			N M G _ I N T E R . C
 *
 *  Routines to intersect two NMG regions.  When complete, all loops
 *  in each region have a single classification w.r.t. the other region,
 *  i.e. all geometric intersections of the two regions have explicit
 *  topological representations.
 *
 *  The intersector makes sure that all geometric intersections gets
 *  recorded with explicit geometry and topology that is shared between both
 *  regions. Primary examples of this are (a) the line of intersection
 *  between two planes (faces), and (b) the point of intersection where two
 *  edges cross.
 *
 *  Entities of one region that are INSIDE, but not ON the other region
 *  do not become shared during the intersection process.
 *
 *  Method -
 *
 *	Find all the points of intersection between the two regions, and
 *	insert vertices at those points, breaking edges on those new
 *	vertices as appropriate.
 *
 *	Call the face cutter to construct and delete edges and loops
 *	along the line of intersection, as appropriate.
 *
 *	There are no "user interface" routines in here.
 *
 *  Authors -
 *	Michael John Muuss
 *	Lee A. Butler
 *  
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5068  USA
 *  
 *  Distribution Notice -
 *	Re-distribution of this software is restricted, as described in
 *	your "Statement of Terms and Conditions for the Release of
 *	The BRL-CAD Pacakge" agreement.
 *
 *  Copyright Notice -
 *	This software is Copyright (C) 1994 by the United States Army
 *	in all countries except the USA.  All rights reserved.
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (ARL)";
#endif

#include <stdio.h>
#include <math.h>
#include "externs.h"
#include "machine.h"
#include "vmath.h"
#include "nmg.h"
#include "raytrace.h"

#define ISECT_NONE	0
#define ISECT_SHARED_V	1
#define ISECT_SPLIT1	2
#define ISECT_SPLIT2	4

struct nmg_inter_struct {
	long		magic;
	struct nmg_ptbl	*l1;		/* vertexuses on the line of */
	struct nmg_ptbl *l2;		/* intersection between planes */
	struct shell	*s1;
	struct shell	*s2;
	struct faceuse	*fu1;		/* null if l1 comes from a wire */
	struct faceuse	*fu2;		/* null if l2 comes from a wire */
	struct rt_tol	tol;
	int		coplanar;	/* a flag */
	point_t		pt;		/* 3D line of intersection */
	vect_t		dir;
	point_t		pt2d;		/* 2D projection of isect line */
	vect_t		dir2d;
	fastf_t		*vert2d;	/* Array of 2d vertex projections [index] */
	int		maxindex;	/* size of vert2d[] */
	mat_t		proj;		/* Matrix to project onto XY plane */
	struct face	*face;		/* face of 2d projection plane */
};
#define NMG_INTER_STRUCT_MAGIC	0x99912120
#define NMG_CK_INTER_STRUCT(_p)	NMG_CKMAG(_p, NMG_INTER_STRUCT_MAGIC, "nmg_inter_struct")

static CONST struct nmg_visit_handlers nmg_null_handlers;	/* null filled */

struct ee_2d_state {
	struct nmg_inter_struct	*is;
	struct edgeuse	*eu;
	point_t	start;
	point_t	end;
	vect_t	dir;
};

RT_EXTERN(void		nmg_isect2d_prep, (struct nmg_inter_struct *is, struct face *f1));
RT_EXTERN(CONST struct vertexuse *nmg_loop_touches_self, (CONST struct loopuse *lu));

static int	nmg_isect_edge2p_face2p RT_ARGS((struct nmg_inter_struct *is,
			struct edgeuse *eu, struct faceuse *fu,
			struct faceuse *eu_fu));
struct edgeuse *	nmg_break_eu_on_v RT_ARGS((struct edgeuse *eu1,
			struct vertex *v2, struct faceuse *fu,
			struct nmg_inter_struct *is));
RT_EXTERN(struct vertexuse *	nmg_enlist_vu, (struct nmg_inter_struct	*is,
				CONST struct vertexuse *vu,
				struct vertexuse *dualvu));
RT_EXTERN(void			nmg_isect2d_cleanup, (struct nmg_inter_struct *is));


static struct nmg_inter_struct	*nmg_hack_last_is;	/* see nmg_isect2d_final_cleanup() */


/*
 *			N M G _ E N L I S T _ V U
 *
 *  Given a vu which represents a point of intersection between shells
 *  s1 and s2, insert it and it's dual into lists l1 and l2.
 *  First, determine whether the vu came from s1 or s2, and insert in
 *  the corresponding list.
 *
 *  Second, try and find a dual of that vertex
 *  in the other shell's faceuse (fu1 or fu2)
 *  (if the entity in the other shell is not a wire), and enlist the dual.
 *  If there is no dual, make a self-loop over there, and enlist that.
 *
 *  If 'dualvu' is provided, don't search, just use that.
 *
 *  While it is true that in most cases the calling routine will know
 *  which shell the vu came from, it's cheap to re-determine it here.
 *  This "all in one" packaging, which handles both lists automaticly
 *  is *vastly* superior to the previous version, which pushed 10-20
 *  lines of bookkeeping up into *every* place an intersection vu was
 *  created.
 *
 *  Returns a pointer to vu's dual.
 *
 *  "Join the Army, young vertexuse".
 */
struct vertexuse *
nmg_enlist_vu( is, vu, dualvu )
struct nmg_inter_struct	*is;
CONST struct vertexuse	*vu;
struct vertexuse	*dualvu;		/* vu's dual in other shell.  May be NULL */
{
	struct shell		*sv;		/* shell of vu */
	struct loopuse		*lu;		/* lu of new self-loop */
	struct faceuse		*dualfu;	/* faceuse of vu's dual */
	struct shell		*duals;		/* shell of vu's dual */
	struct faceuse		*fuv;		/* faceuse of vu */

	NMG_CK_INTER_STRUCT(is);
	NMG_CK_VERTEXUSE(vu);
	if(dualvu) NMG_CK_VERTEXUSE(dualvu);

	sv = nmg_find_s_of_vu( vu );
	fuv = nmg_find_fu_of_vu( vu );

	/* First step:  add vu to corresponding list */
	if( sv == is->s1 )  {
		nmg_tbl( is->l1, TBL_INS_UNIQUE, (long *)&vu->l.magic );
		duals = is->s2;		/* other shell */
		dualfu = is->fu2;
		if( is->fu1 && is->fu1->s_p != is->s1 ) rt_bomb("fu1/s1 mismatch\n");
		if( fuv != is->fu1 )  {
			rt_log("fuv=x%x, fu1=x%x, fu2=x%x\n", fuv, is->fu1, is->fu2);
			rt_bomb("nmg_enlist_vu() vu/fu1 mis-match\n");
		}
	} else if( sv == is->s2 )  {
		nmg_tbl( is->l2, TBL_INS_UNIQUE, (long *)&vu->l.magic );
		duals = is->s1;		/* other shell */
		dualfu = is->fu1;
		if( is->fu2 && is->fu2->s_p != is->s2 ) rt_bomb("fu2/s2 mismatch\n");
		if( fuv != is->fu2 )  {
			rt_log("fuv=x%x, fu1=x%x, fu2=x%x\n", fuv, is->fu1, is->fu2);
			rt_bomb("nmg_enlist_vu() vu/fu2 mis-match\n");
		}
	} else {
		rt_log("nmg_enlist_vu(vu=x%x) sv=x%x, s1=x%x, s2=x%x\n",
			vu, sv, is->s1, is->s2 );
		rt_bomb("nmg_enlist_vu: vu is not in s1 or s2\n");
	}

	if( dualvu )  {
		if( vu->v_p != dualvu->v_p )  rt_bomb("dual vu has different vertex\n");
		if( nmg_find_s_of_vu(dualvu) != duals )  rt_bomb("dual vu shell mis-match\n");
		if( dualfu && nmg_find_fu_of_vu(dualvu) != dualfu) rt_bomb("dual vu has wrong fu\n");
	}

	/* Second, search for vu's dual */
	if( dualfu )  {
		NMG_CK_FACEUSE(dualfu);
		if( dualfu->s_p != duals )  rt_bomb("dual fu's shell is not dual's shell?\n");
		if( !dualvu && !(dualvu = nmg_find_v_in_face( vu->v_p, dualfu )) )  {
			/* Not found, make self-loop in dualfu */
			lu = nmg_mlv( &dualfu->l.magic, vu->v_p, OT_BOOLPLACE );
			nmg_loop_g( lu->l_p, &(is->tol) );
			dualvu = RT_LIST_FIRST( vertexuse, &lu->down_hd );
		} else {
			if( rt_g.NMG_debug & DEBUG_POLYSECT )  {
				rt_log("nmg_enlist_vu(vu=x%x) re-using dualvu=x%x from dualfu=x%x\n",
					vu,
					dualvu, dualfu);
			}
		}
	} else {
		/* Must have come from a wire in other shell, make wire loop */
		rt_log("\tvu=x%x, %s, fu1=x%x, fu2=x%x\n", vu, (sv==is->s1)?"shell 1":"shell 2", is->fu1, is->fu2);
		rt_log("nmg_enlist_vu(): QUESTION: What do I search for wire intersections?  Making self-loop\n");
		if( !dualvu && !(dualvu = nmg_find_v_in_shell( vu->v_p, duals, 0 )) )  {
			/* Not found, make self-loop in dual shell */
			lu = nmg_mlv( &duals->l.magic, vu->v_p, OT_BOOLPLACE );
			nmg_loop_g( lu->l_p, &(is->tol) );
			dualvu = RT_LIST_FIRST( vertexuse, &lu->down_hd );
		} else {
			if( rt_g.NMG_debug & DEBUG_POLYSECT )  {
				rt_log("nmg_enlist_vu(vu=x%x) re-using dualvu=x%x from dualshell=x%x\n",
					vu,
					dualvu, duals);
			}
		}
	}
	NMG_CK_VERTEXUSE(dualvu);

	/* Enlist the dual onto the other list */
	if( sv == is->s1 )  {
		nmg_tbl( is->l2, TBL_INS_UNIQUE, (long *)&dualvu->l.magic );
	} else {
		nmg_tbl( is->l1, TBL_INS_UNIQUE, (long *)&dualvu->l.magic );
	}

	if( rt_g.NMG_debug & DEBUG_POLYSECT )  {
		rt_log("nmg_enlist_vu(vu=x%x) v=x%x, dualvu=x%x (%s)\n",
			vu, vu->v_p, dualvu,
			(sv == is->s1) ? "shell 1" : "shell 2" );
	}

	/* Some (expensive) centralized sanity checking */
	if( (rt_g.NMG_debug & DEBUG_VERIFY) && is->fu1 && is->fu2 )  {
		nmg_ck_v_in_2fus(vu->v_p, is->fu1, is->fu2, &(is->tol));
	}
	return dualvu;
}

/*
 *			N M G _ G E T _ 2 D _ V E R T E X
 *
 *  A "lazy evaluator" to obtain the 2D projection of a vertex.
 *  The lazy approach is not a luxury, since new (3D) vertices are created
 *  as the edge/edge intersection proceeds, and their 2D coordinates may
 *  be needed later on in the calculation.
 *  The alternative would be to store the 2D projection each time a
 *  new vertex is created, but that is likely to be a lot of bothersome
 *  code, where one omission would be deadly.
 *
 *  The return is a 3-tuple, with the Z coordinate set to 0.0 for safety.
 *  This is especially useful when the projected value is printed using
 *  one of the 3D print routines.
 */
static void
nmg_get_2d_vertex( v2d, v, is, fu )
point_t		v2d;		/* a 3-tuple */
struct vertex	*v;
struct nmg_inter_struct	*is;
CONST struct faceuse	*fu;	/* for plane equation */
{
	register fastf_t	*pt2d;
	point_t			pt;
	struct vertex_g		*vg;

	NMG_CK_INTER_STRUCT(is);
	NMG_CK_VERTEX(v);

	/* If 2D preparations have not been made yet, do it now */
	if( !is->vert2d )  {
		nmg_isect2d_prep( is, fu->f_p );
	}

	if( fu->f_p != is->face )  {
		rt_log("nmg_get_2d_vertex(,fu=%x) f=x%x, is->face=%x\n",
			fu, fu->f_p, is->face);
		rt_bomb("nmg_get_2d_vertex:  face mis-match\n");
	}
	NMG_CK_FACEUSE(fu);

	if( !v->vg_p )  {
		rt_log("nmg_get_2d_vertex: v=x%x, fu=x%x, null vg_p\n", v, fu);
		rt_bomb("nmg_get_2d_vertex:  vertex with no geometry!\n");
	}
	vg = v->vg_p;
	NMG_CK_VERTEX_G(vg);
	if( v->index >= is->maxindex )  {
		struct model	*m;
		int		oldmax;
		register int	i;

		oldmax = is->maxindex;
		m = nmg_find_model(&v->magic);
		NMG_CK_MODEL(m);
		rt_log("nmg_get_2d_vertex:  v=x%x, v->index=%d, is->maxindex=%d, m->maxindex=%d\n",
			v, v->index, is->maxindex, m->maxindex );
		if( v->index >= m->maxindex )  {
			/* Really off the end */
			VPRINT("3d vertex", vg->coord);
			rt_bomb("nmg_get_2d_vertex:  array overrun\n");
		}
		/* Need to extend array, it's grown. */
		is->maxindex = m->maxindex * 4;
		if (rt_g.NMG_debug & DEBUG_POLYSECT)  {
			rt_log("nmg_get_2d_vertex() extending vert2d array from %d to %d points (m max=%d)\n",
				oldmax, is->maxindex, m->maxindex);
		}
		is->vert2d = (fastf_t *)rt_realloc( (char *)is->vert2d,
			is->maxindex * 3 * sizeof(fastf_t), "vert2d[]");

		/* Clear out the new part of the 2D vertex array, setting flag in [2] to -1 */
		for( i = (3*is->maxindex)-1-2; i >= oldmax*3; i -= 3 )  {
			VSET( &is->vert2d[i], 0, 0, -1 );
		}
	}
	pt2d = &is->vert2d[v->index*3];
	if( pt2d[2] == 0 )  {
		/* Flag set.  Conversion is done.  Been here before */
		v2d[0] = pt2d[0];
		v2d[1] = pt2d[1];
		v2d[2] = 0;
		return;
	}

	MAT4X3PNT( pt, is->proj, vg->coord );
	v2d[0] = pt2d[0] = pt[0];
	v2d[1] = pt2d[1] = pt[1];
	v2d[2] = pt2d[2] = 0;		/* flag */

	if( !NEAR_ZERO( pt[2], is->tol.dist ) )  {
		plane_t	n;
		fastf_t	dist;
		NMG_GET_FU_PLANE( n, fu );
		dist = DIST_PT_PLANE(vg->coord, n);
		rt_log("nmg_get_2d_vertex ERROR #%d (%g %g %g) becomes (%g,%g)\n\t%g != zero, dist3d=%g, %g*tol\n",
			v->index, V3ARGS(vg->coord), V3ARGS(pt),
			dist, dist/is->tol.dist );
		if( !NEAR_ZERO( dist, is->tol.dist ) &&
		    !NEAR_ZERO( pt[2], 10*is->tol.dist ) )  {
			rt_log("nmg_get_2d_vertex(,fu=%x) f=x%x, is->face=%x\n",
				fu, fu->f_p, is->face);
			PLPRINT("is->face N", is->face->fg_p->N);
			PLPRINT("fu->f_p N", n);
			rt_bomb("3D->2D point projection error\n");
		}
	}

	if (rt_g.NMG_debug & DEBUG_POLYSECT)  {
		rt_log("2d #%d (%g %g %g) becomes (%g,%g) %g\n",
			v->index, V3ARGS(vg->coord), V3ARGS(pt) );
	}
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
/* XXX Set this up so that it can take either an edge pointer
 * or a face pointer.  In case of edge, make edge_g->dir unit, and
 * rotate that to +X axis.  Make edge_g->pt be the origin.
 * This will allow the 2D routines to operate on wires.
 * f1 & is->face should become a (long *) to track geom associativity.
 */
void
nmg_isect2d_prep( is, f1 )
struct nmg_inter_struct	*is;
struct face		*f1;
{
	struct model	*m;
	struct face_g	*fg;
	vect_t		to;
	point_t		centroid;
	point_t		centroid_proj;
	plane_t		n;
	register int	i;

	NMG_CK_INTER_STRUCT(is);
	NMG_CK_FACE(f1);

	nmg_isect2d_cleanup(is);
	nmg_hack_last_is = is;

	fg = f1->fg_p;
	NMG_CK_FACE_G(fg);
	is->face = f1;
	if( f1->flip )  {
		VREVERSE( n, fg->N );
		n[3] = -fg->N[3];
	} else {
		HMOVE( n, fg->N );
	}
	if (rt_g.NMG_debug & DEBUG_POLYSECT)  {
		rt_log("nmg_isect2d_prep(f=x%x) flip=%d\n", f1, f1->flip);
		PLPRINT("N", n);
	}

	m = nmg_find_model( &f1->l.magic );

	is->maxindex = ( 2 * m->maxindex );
	is->vert2d = (fastf_t *)rt_malloc( is->maxindex * 3 * sizeof(fastf_t), "vert2d[]");

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
	mat_fromto( is->proj, n, to );
	VADD2SCALE( centroid, f1->max_pt, f1->min_pt, 0.5 );
	MAT4X3PNT( centroid_proj, is->proj, centroid );
	centroid_proj[Z] = n[3];	/* pull dist from origin off newZ */
	MAT_DELTAS_VEC_NEG( is->proj, centroid_proj );

	/* Clear out the 2D vertex array, setting flag in [2] to -1 */
	for( i = (3*is->maxindex)-1-2; i >= 0; i -= 3 )  {
		VSET( &is->vert2d[i], 0, 0, -1 );
	}
}

/*
 *			N M G _ I S E C T 2 D _ C L E A N U P.
 *
 *  Common routine to zap 2d vertex cache, and release dynamic storage.
 */
void
nmg_isect2d_cleanup(is)
struct nmg_inter_struct	*is;
{
	NMG_CK_INTER_STRUCT(is);

	nmg_hack_last_is = (struct nmg_inter_struct *)NULL;

	if( !is->vert2d )  return;
	rt_free( (char *)is->vert2d, "vert2d");
	is->vert2d = (fastf_t *)NULL;
	is->face = (struct face *)NULL;
}

/*
 *			N M G _ I S E C T 2 D _ F I N A L _ C L E A N U P
 *
 *  XXX Hack routine used for storage reclamation by G-JACK for
 *  XXX calculation of the reportcard without gobbling lots of memory
 *  XXX on rt_bomb() longjmp()s.
 *  Can be called by the longjmp handler with impunity.
 *  If a pointer to busy dynamic memory is still handy, it will be freed.
 *  If not, no harm done.
 */
void
nmg_isect2d_final_cleanup()
{
	if( nmg_hack_last_is && nmg_hack_last_is->magic == NMG_INTER_STRUCT_MAGIC )
		nmg_isect2d_cleanup( nmg_hack_last_is );
}

/*
 *			N M G _ I S E C T _ E D G E 2 P _ V E R T 2 P
 *
 *  Intersect an edge and a vertex known to lie in the same plane.
 *
 *  This is a separate routine because it's used more than once.
 *
 *  Returns -
 *	0	if there was no intersection
 *	1	if the vertex was an endpoint of the edge.
 *	2	if edge was split (in the middle) at vertex.
 *
 * XXX YYY ZZZ replace this with nmg_isect_vu_on_eu().
 */
static int
nmg_isect_edge2p_vert2p( is, eu, vu )
struct nmg_inter_struct *is;
struct edgeuse		*eu;
struct vertexuse	*vu;
{
	fastf_t		dist;
	int		status;
	point_t		endpt;
	int		ret;

	NMG_CK_INTER_STRUCT(is);
	NMG_CK_EDGEUSE(eu);
	NMG_CK_VERTEXUSE(vu);

	/* Topology check */
	if( vu->v_p == eu->vu_p->v_p || vu->v_p == eu->eumate_p->vu_p->v_p )  {
		ret = 1;
		goto out;
	}

	/* XXX a 2d formulation would be better! */
#if 0
	/* Has to wait for non-faceuse based versions of the 2d proj */
	nmg_get_2d_vertex( is->pt, eu->vu_p->v_p, is, fu );
	nmg_get_2d_vertex( endpt, eu->eumate_p->vu_p->v_p, is, fu );
	VSUB2_2D( is->dir, endpt, is->pt );
	/* rt_isect_pt2_lseg2() */
#endif

	status = rt_isect_pt_lseg( &dist,
		eu->vu_p->v_p->vg_p->coord,
		eu->eumate_p->vu_p->v_p->vg_p->coord,
		vu->v_p->vg_p->coord, &is->tol );
	switch( status )  {
	default:
		ret = 0;
		break;
	case 1:
		/* pt is at start of edge */
		/* The vertex assumes the coords of the edge vertex */
		nmg_jv( eu->vu_p->v_p, vu->v_p );
		ret = 1;
		break;
	case 2:
		/* pt is at end of edge */
		/* The vertex assumes the coords of the edge vertex */
		nmg_jv( vu->v_p, eu->eumate_p->vu_p->v_p );
		ret = 1;
		break;
	case 3:
		/* pt is in interior of edge, break edge */
		(void)nmg_ebreak( vu->v_p, eu );
		ret = 2;
		break;
	}
out:
	if (rt_g.NMG_debug & DEBUG_POLYSECT)
		rt_log("nmg_isect_edge2p_vert2p(, eu=x%x, vu=x%x) ret=%d\n", eu, vu, ret);
	return ret;
}

/*
 *			N M G _ I S E C T _ V E R T 2 P _ F A C E 2 P
 *
 *  Handle the complete intersection of a vertex which lies on the
 *  plane of a face.
 *  If already part of the topology of the face, do nothing more.
 *  It it intersects one of the edges of the face, break the edge there.
 *  Otherwise, add a self-loop into the face as a marker.
 *
 *  Since this is an intersection of a vertex, no loop cut/join action
 *  is needed, since the only possible actions are fusing vertices
 *  and breaking edges into two edges about this vertex.
 *  No vertexuses need to be added to the intersect list.
 *
 *  Returns -
 *	vu2	dual of vu1.  vu2 is in fu2.
 */
struct vertexuse *
nmg_isect_vert2p_face2p(is, vu1, fu2)
struct nmg_inter_struct *is;
struct vertexuse	*vu1;
struct faceuse		*fu2;
{
	struct vertexuse	*vu2;
	struct loopuse	 *lu2;
	pointp_t	pt;
	fastf_t		dist;

	if (rt_g.NMG_debug & DEBUG_POLYSECT)
		rt_log("nmg_isect_vert2p_face2p(, vu1=x%x, fu2=x%x)\n", vu1, fu2);
	NMG_CK_INTER_STRUCT(is);
	NMG_CK_VERTEXUSE(vu1);
	NMG_CK_FACEUSE(fu2);

	/* check the topology first */	
	if( vu2 = nmg_find_v_in_face(vu1->v_p, fu2) )  return vu2;

	/* topology didn't say anything, check with the geometry. */
	pt = vu1->v_p->vg_p->coord;

	/* For every edge and vert in face, check geometric intersection */
	for( RT_LIST_FOR( lu2, loopuse, &fu2->lu_hd ) )  {
		struct edgeuse	*eu2;

		NMG_CK_LOOPUSE(lu2);
		if( RT_LIST_FIRST_MAGIC( &lu2->down_hd ) == NMG_VERTEXUSE_MAGIC )  {
			vu2 = RT_LIST_FIRST( vertexuse, &lu2->down_hd );
			if( vu1->v_p == vu2->v_p )  return vu2;
			/* Perhaps a 2d routine here? */
			if( rt_pt3_pt3_equal( pt, vu2->v_p->vg_p->coord, &is->tol ) )  {
				/* Fuse the two verts together */
				nmg_jv( vu1->v_p, vu2->v_p );
				nmg_ck_v_in_2fus(vu1->v_p, nmg_find_fu_of_vu(vu1), fu2, &is->tol);
				return vu2;
			}
			continue;
		}
		for( RT_LIST_FOR( eu2, edgeuse, &lu2->down_hd ) )  {
			struct edgeuse	*new_eu;
			/* If there is an intersection, we are done */
			if( new_eu = nmg_break_eu_on_v( eu2, vu1->v_p, fu2, is ) ) {
				return new_eu->vu_p;
			}
		}
	}

	/* The vertex lies in the face, but touches nothing.  Place marker */
	if (rt_g.NMG_debug & DEBUG_POLYSECT)
	    	VPRINT("Making vertexloop", pt);

	lu2 = nmg_mlv(&fu2->l.magic, vu1->v_p, OT_BOOLPLACE);
	nmg_loop_g( lu2->l_p, &is->tol );
	return RT_LIST_FIRST( vertexuse, &lu2->down_hd );
}

/*
 *			N M G _ I S E C T _ 3 V E R T E X _ 3 F A C E
 *
 *	intersect a vertex with a face (primarily for intersecting
 *	loops of a single vertex with a face).
 *
 *  XXX It would be useful to have one of the new vu's in fu returned
 *  XXX as a flag, so that nmg_find_v_in_face() wouldn't have to be called
 *  XXX to re-determine what was just done.
 */
static void
nmg_isect_3vertex_3face(is, vu, fu)
struct nmg_inter_struct *is;
struct vertexuse *vu;
struct faceuse *fu;
{
	struct loopuse *plu;
	struct vertexuse *vup;
	pointp_t pt;
	fastf_t dist;
	plane_t	n;

	NMG_CK_INTER_STRUCT(is);
	NMG_CK_VERTEXUSE(vu);
	NMG_CK_VERTEX(vu->v_p);
	NMG_CK_FACEUSE(fu);

	if (rt_g.NMG_debug & DEBUG_POLYSECT)
		rt_log("nmg_isect_3vertex_3face(, vu=x%x, fu=x%x) v=x%x\n", vu, fu, vu->v_p);

	/* check the topology first */	
	if (vup=nmg_find_v_in_face(vu->v_p, fu)) {
		if (rt_g.NMG_debug & DEBUG_POLYSECT) rt_log("\tvu lies in face (topology 1)\n");
		(void)nmg_tbl(is->l1, TBL_INS_UNIQUE, &vu->l.magic);
		(void)nmg_tbl(is->l2, TBL_INS_UNIQUE, &vup->l.magic);
		return;
	}


	/* since the topology didn't tell us anything, we need to check with
	 * the geometry
	 */
	pt = vu->v_p->vg_p->coord;
	NMG_GET_FU_PLANE( n, fu );
	dist = DIST_PT_PLANE(pt, n);

	if ( !NEAR_ZERO(dist, is->tol.dist) )  {
		if (rt_g.NMG_debug & DEBUG_POLYSECT) rt_log("\tvu not on face (geometry)\n");
		return;
	}

	/*
	 *  The point lies on the plane of the face, by geometry.
	 *  This is now a 2-D problem.
	 *
	 *  Intersect this point with all the edges of the face.
	 *  Note that no nmg_tbl() calls will be done during vert2p_face2p,
	 *  which is exactly what is needed here.
	 */
	vup = nmg_isect_vert2p_face2p( is, vu, fu );
	NMG_CK_VERTEXUSE(vup);

	nmg_enlist_vu( is, vu, vup );
}

/*
 *			N M G _ B R E A K _ 3 E D G E _ A T _ P L A N E
 *
 *	Having decided that an edge(use) crosses a plane of intersection,
 *	stick a vertex at the point of intersection along the edge.
 *
 *  vu1_final in fu1 is RT_LIST_PNEXT_CIRC(edgeuse,eu1)->vu_p after return.
 *  vu2_final is the returned value, and is in fu2.
 *
 */
static struct vertexuse *
nmg_break_3edge_at_plane(hit_pt, fu2, is, eu1)
CONST point_t		hit_pt;
struct faceuse		*fu2;		/* The face that eu intersects */
struct nmg_inter_struct *is;
struct edgeuse		*eu1;		/* Edge to be broken (in fu1) */
{
	struct vertexuse *vu1_final;
	struct vertexuse *vu2_final;	/* hit_pt's vu in fu2 */
	struct vertex	*v2;
	struct loopuse	*plu2;		/* "point" loopuse */
	struct edgeuse	*eu1forw;	/* New eu, after break, forw of eu1 */
	struct vertex	*v1;
	struct vertex	*v1mate;
	fastf_t		dist;

	NMG_CK_INTER_STRUCT(is);
	NMG_CK_EDGEUSE(eu1);

	v1 = eu1->vu_p->v_p;
	NMG_CK_VERTEX(v1);
	v1mate = eu1->eumate_p->vu_p->v_p;
	NMG_CK_VERTEX(v1mate);

	/* Intersection is between first and second vertex points.
	 * Insert new vertex at intersection point.
	 */
	if (rt_g.NMG_debug & DEBUG_POLYSECT)  {
		rt_log("nmg_break_3edge_at_plane() Splitting %g, %g, %g <-> %g, %g, %g\n",
			V3ARGS(v1->vg_p->coord),
			V3ARGS(v1mate->vg_p->coord) );
		VPRINT("\tAt point of intersection", hit_pt);
	}

	/* Double check for bad behavior */
	if( rt_pt3_pt3_equal( hit_pt, v1->vg_p->coord, &is->tol ) )
		rt_bomb("nmg_break_3edge_at_plane() hit_pt equal to v1\n");
	if( rt_pt3_pt3_equal( hit_pt, v1mate->vg_p->coord, &is->tol ) )
		rt_bomb("nmg_break_3edge_at_plane() hit_pt equal to v1mate\n");

	{
		vect_t	va, vb;
		VSUB2( va, hit_pt, eu1->vu_p->v_p->vg_p->coord  );
		VSUB2( vb, eu1->eumate_p->vu_p->v_p->vg_p->coord, hit_pt );
		VUNITIZE(va);
		VUNITIZE(vb);
		if( VDOT( va, vb ) <= 0.7071 )  {
			rt_bomb("nmg_break_3edge_at_plane() eu1 changes direction?\n");
		}
	}
	{
		struct rt_tol	t2;
		t2 = is->tol;	/* Struct copy */

		t2.dist = is->tol.dist * 4;
		t2.dist_sq = t2.dist * t2.dist;
		dist = DIST_PT_PT(hit_pt, v1->vg_p->coord);
		if( rt_pt3_pt3_equal( hit_pt, v1->vg_p->coord, &t2 ) )
			rt_log("NOTICE: nmg_break_3edge_at_plane() hit_pt nearly equal to v1 %g*tol\n", dist/is->tol.dist);
		dist = DIST_PT_PT(hit_pt, v1mate->vg_p->coord);
		if( rt_pt3_pt3_equal( hit_pt, v1mate->vg_p->coord, &t2 ) )
			rt_log("NOTICE: nmg_break_3edge_at_plane() hit_pt nearly equal to v1mate %g*tol\n", dist/is->tol.dist);
	}

	/* if we can't find the appropriate vertex in the
	 * other face by a geometry search, build a new vertex.
	 * Otherwise, re-use the existing one.
	 * Can't just search other face, might miss relevant vert.
	 */
	v2 = nmg_find_pt_in_model(fu2->s_p->r_p->m_p, hit_pt, &(is->tol));
	if (v2) {
		/* the other face has a convenient vertex for us */
		if (rt_g.NMG_debug & DEBUG_POLYSECT)
			rt_log("re-using vertex v=x%x from other shell\n", v2);

		eu1forw = nmg_ebreak(v2, eu1);
		vu1_final = eu1forw->vu_p;
		vu2_final = nmg_enlist_vu( is, vu1_final, 0 );
	} else {
		/* The other face has no vertex in this vicinity */
		/* If hit_pt falls outside all the loops in fu2,
		 * then there is no need to break this edge.
		 * XXX It is probably cheaper to call nmg_isect_3vertex_3face()
		 * XXX here first, causing any ON cases to be resolved into
		 * XXX shared topology first (and also cutting fu2 edges NOW),
		 * XXX and then run the classifier to answer IN/OUT.
		 * This is expensive.  For getting started, tolerate it.
		 */
		int	class;
		class = nmg_class_pt_f( hit_pt, fu2, &is->tol );
		if( class == NMG_CLASS_AoutB )  {
			/* point outside face loop, no need to break eu1 */
#if 0
rt_log("%%%%%% point is outside face loop, no need to break eu1?\n");
			return (struct vertexuse *)NULL;
#endif
			/* Can't optimize this break out -- need to have
			 * the new vertexuse on the line of intersection,
			 * to drive the state machine of the face cutter!
			 */
		}

		eu1forw = nmg_ebreak((struct vertex *)NULL, eu1);
		vu1_final = eu1forw->vu_p;
		nmg_vertex_gv(vu1_final->v_p, hit_pt);
		if (rt_g.NMG_debug & DEBUG_POLYSECT)
			rt_log("Made new vertex vu=x%x, v=x%x\n", vu1_final, vu1_final->v_p);

		NMG_CK_VERTEX_G(eu1->vu_p->v_p->vg_p);
		NMG_CK_VERTEX_G(eu1->eumate_p->vu_p->v_p->vg_p);
		NMG_CK_VERTEX_G(eu1forw->vu_p->v_p->vg_p);
		NMG_CK_VERTEX_G(eu1forw->eumate_p->vu_p->v_p->vg_p);

		if (rt_g.NMG_debug & DEBUG_POLYSECT) {
			register pointp_t p1 = eu1->vu_p->v_p->vg_p->coord;
			register pointp_t p2 = eu1->eumate_p->vu_p->v_p->vg_p->coord;

			rt_log("After split eu1 x%x= %g, %g, %g -> %g, %g, %g\n",
				eu1,
				V3ARGS(p1), V3ARGS(p2) );
			p1 = eu1forw->vu_p->v_p->vg_p->coord;
			p2 = eu1forw->eumate_p->vu_p->v_p->vg_p->coord;
			rt_log("\teu1forw x%x = %g, %g, %g -> %g, %g, %g\n",
				eu1forw,
				V3ARGS(p1), V3ARGS(p2) );
		}

		switch(class)  {
		case NMG_CLASS_AinB:
			/* point inside a face loop, break edge */
			break;
		case NMG_CLASS_AonBshared:
			/* point is on a loop boundary.  Break fu2 loop too? */
			if (rt_g.NMG_debug & DEBUG_POLYSECT)
				rt_log("%%%%%% point is on loop boundary.  Break fu2 loop too?\n");
			nmg_isect_3vertex_3face( is, vu1_final, fu2 );
			/* XXX should get new vu2 from isect_3vertex_3face! */
			vu2_final = nmg_find_v_in_face( vu1_final->v_p, fu2 );
			if( !vu2_final ) rt_bomb("%%%%%% missed!\n");
			NMG_CK_VERTEXUSE(vu2_final);
			nmg_enlist_vu( is, vu1_final, vu2_final );
			return vu2_final;
		case NMG_CLASS_AoutB:
			/* Can't optimize this, break edge anyway. */
			break;
		default:
			rt_bomb("nmg_break_3edge_at_plane() bad classification return from nmg_class_pt_f()\n");
		}

		/* stick this vertex in the other shell
		 * and make sure it is in the other shell's
		 * list of vertices on the intersect line
		 */
		plu2 = nmg_mlv(&fu2->l.magic, vu1_final->v_p, OT_BOOLPLACE);
		vu2_final = RT_LIST_FIRST( vertexuse, &plu2->down_hd );
		NMG_CK_VERTEXUSE(vu2_final);
		nmg_loop_g(plu2->l_p, &is->tol);

		if (rt_g.NMG_debug & DEBUG_POLYSECT) {
			rt_log("Made vertexloop in other face. lu=x%x vu=x%x on v=x%x\n",
				plu2, 
				vu2_final, vu2_final->v_p);
		}
		vu2_final = nmg_enlist_vu( is, vu1_final, vu2_final );
	}

	if (rt_g.NMG_debug & DEBUG_POLYSECT) {
		register pointp_t	p1, p2;
		p1 = eu1->vu_p->v_p->vg_p->coord;
		p2 = eu1->eumate_p->vu_p->v_p->vg_p->coord;
		rt_log("\tNow %g, %g, %g <-> %g, %g, %g\n",
			V3ARGS(p1), V3ARGS(p2) );
		p1 = eu1forw->vu_p->v_p->vg_p->coord;
		p2 = eu1forw->eumate_p->vu_p->v_p->vg_p->coord;
		rt_log("\tand %g, %g, %g <-> %g, %g, %g\n\n",
			V3ARGS(p1), V3ARGS(p2) );
	}
	return vu2_final;
}

/*
 *			N M G _ B R E A K _ E U _ O N _ V
 *
 *  The vertex 'v2' is known to lie in the plane of eu1's face.
 *  If v2 lies between the two endpoints of eu1, break eu1 and
 *  return the new edgeuse pointer.
 *
 *  If an edgeuse vertex is joined with v2, v2 remains as the survivor,
 *  as the caller is working on it explicitly, and the edgeuse vertices
 *  are dealt with implicitly (by dereferencing the eu pointers).
 *  Otherwise, we will invalidate our caller's v2 pointer.
 *
 *  Note that no "intersection line" stuff is done, the goal here is
 *  just to get the edge appropriately broken.
 *
 *  Either faceuse can be passed in, but it needs to be consistent with the
 *  faceuse used to establish the 2d vertex cache.
 *
 *  Returns -
 *	new_eu	if edge is broken
 *	0	otherwise
 */
struct edgeuse *
nmg_break_eu_on_v( eu1, v2, fu, is )
struct edgeuse		*eu1;
struct vertex		*v2;
struct faceuse		*fu;	/* for plane equation of (either) face */
struct nmg_inter_struct	*is;
{
	point_t		a;
	point_t		b;
	point_t		p;
	int		code;
	fastf_t		dist;
	struct vertex	*v1a;
	struct vertex	*v1b;
	struct edgeuse		*new_eu = (struct edgeuse *)0;

	NMG_CK_EDGEUSE(eu1);
	NMG_CK_VERTEX(v2);
	NMG_CK_FACEUSE(fu);
	NMG_CK_INTER_STRUCT(is);

	v1a = eu1->vu_p->v_p;
	v1b = RT_LIST_PNEXT_CIRC( edgeuse, eu1 )->vu_p->v_p;

	/* Check for already shared topology */
	if( v1a == v2 || v1b == v2 )  {
		goto out;
	}

	/* Map to 2d */
	nmg_get_2d_vertex( a, v1a, is, fu );
	nmg_get_2d_vertex( b, v1b, is, fu );
	nmg_get_2d_vertex( p, v2, is, fu );

	dist = -INFINITY;
	code = rt_isect_pt2_lseg2( &dist, a, b, p, &(is->tol) );

	switch(code)  {
	case -2:
		/* P outside AB */
		break;
	default:
	case -1:
		/* P not on line */
#if 0
		/* This can happen when v2 is a long way from the lseg */
		V2PRINT("a", a);
		V2PRINT("p", p);
		V2PRINT("b", b);
		VPRINT("A", v1a->vg_p->coord);
		VPRINT("P", v2->vg_p->coord);
		VPRINT("B", v1b->vg_p->coord);
		rt_bomb("nmg_break_eu_on_v() P not on line?\n");
#endif
		break;
	case 1:
		/* P is at A */
		nmg_jv( v2, v1a );	/* v2 must be surviving vertex */
		break;
	case 2:
		/* P is at B */
		nmg_jv( v2, v1b );	/* v2 must be surviving vertex */
		break;
	case 3:
		/* P is in the middle, break edge */
		new_eu = nmg_ebreak( v2, eu1 );
		if (rt_g.NMG_debug & DEBUG_POLYSECT) {
			rt_log("nmg_break_eu_on_v() breaking eu=x%x on v=x%x, new_eu=x%x\n",
				eu1, v2, new_eu );
		}
		break;
	}

out:
	return new_eu;
}

/*
 *			N M G _ I S E C T _ 2 C O L I N E A R _ E D G E 2 P
 *
 *  Perform edge mutual breaking only on two colinear edgeuses.
 *  This can result in 2 new edgeuses showing up in either loop (case A & D).
 *  The vertexuse lists are updated to have all participating vu's and
 *  their duals.
 *
 *  Two colinear line segments (eu1 and eu2, or just "1" and "2" in the
 *  diagram) can overlap each other in one of 9 configurations,
 *  labeled A through I:
 *
 *	A	B	C	D	E	F	G	H	I
 *
 *  vu1b,vu2b
 *	*	*	  *	  *	*	  *	*	  *	*=*
 *	1	1	  2	  2	1	  2	1	  2	1 2
 *	1=*	1	  2	*=2	1=*	*=2	*	  *	1 2
 *	1 2	*=*	*=*	1 2	1 2	1 2			1 2
 *	1 2	  2	1	1 2	1 2	1 2	  *	*	1 2
 *	1=*	  2	1	*=2	*=2	1=*	  2	1	1 2
 *	1	  *	*	  2	  2	1	  *	*	1 2
 *	*			  *	  *	*			*=*
 *   vu1a,vu2a
 *
 *  To ensure nothing is missed, break every edgeuse on all 4 vertices.
 *  If a new edgeuse is created, add it to the list of edgeuses still to be
 *  broken.
 *  Brute force, but *certain* not to miss anything.
 *
 *  Returns the number of edgeuses that resulted,
 *  which is always at least the original 2.
 *
 */
int
nmg_isect_2colinear_edge2p( eu1, eu2, fu, is )
struct edgeuse	*eu1;
struct edgeuse	*eu2;
struct faceuse		*fu;	/* for plane equation of (either) face */
struct nmg_inter_struct	*is;
{
	struct edgeuse	*eu[10];
	struct vertexuse *vu[4];
	register int	i;
	register int	j;
	int		neu;	/* Number of edgeuses */

	NMG_CK_EDGEUSE(eu1);
	NMG_CK_EDGEUSE(eu2);
	NMG_CK_FACEUSE(fu);	/* Don't check it, just pass it on down. */
	NMG_CK_INTER_STRUCT(is);

	vu[0] = eu1->vu_p;
	vu[1] = RT_LIST_PNEXT_CIRC( edgeuse, eu1 )->vu_p;
	vu[2] = eu2->vu_p;
	vu[3] = RT_LIST_PNEXT_CIRC( edgeuse, eu2 )->vu_p;

	eu[0] = eu1;
	eu[1] = eu2;
	neu = 2;

	for( i=0; i < neu; i++ )  {
		for( j=0; j<4; j++ )  {
			if( eu[neu] = nmg_break_eu_on_v(eu[i],vu[j]->v_p,fu,is) )  {
				nmg_enlist_vu( is, eu[neu]->vu_p, vu[j] );
				neu++;
			}
		}
	}

	/* Now join 'em up */
	for( i=0; i < neu-1; i++ )  {
		for( j=i+1; j < neu; j++ )  {
			if( !NMG_ARE_EUS_ADJACENT(eu[i],eu[j]) )  continue;
			nmg_radial_join_eu( eu[i], eu[j], &(is->tol) );
		}
	}

	/* Enlist all four of the original endpoints */
	for( i=0; i < 4; i++ )  {
		for( j=0; j < 4; j++ )  {
			if( i==j )  continue;
			if( vu[i]->v_p == vu[j]->v_p )  {
				nmg_enlist_vu( is, vu[i], vu[j] );
				goto next_i;
			}
		}
		/* No match, let subroutine hunt for dual */
		nmg_enlist_vu( is, vu[i], 0 );
next_i:		;
	}

	if (rt_g.NMG_debug & DEBUG_POLYSECT) {
		rt_log("nmg_isect_2colinear_edge2p(eu1=x%x, eu2=x%x) #eu=%d\n",
			eu1, eu2, neu);
	}
	return neu;
}

/*
 *			N M G _ I S E C T _ E D G E 2 P _ E D G E 2 P
 *
 *  Actual 2d edge/edge intersector
 *
 *  One or both of the edges may be wire edges, i.e.
 *  either or both of the fu1 and fu2 args may be null.
 *  If so, the vert_list's are unimportant.
 *
 *  Returns a bit vector -
 *	ISECT_NONE	no intersection
 *	ISECT_SHARED_V	intersection was at (at least one) shared vertex
 *	ISECT_SPLIT1	eu1 was split at (geometric) intersection.
 *	ISECT_SPLIT2	eu2 was split at (geometric) intersection.
 */
int
nmg_isect_edge2p_edge2p( is, eu1, eu2, fu1, fu2 )
struct nmg_inter_struct	*is;
struct edgeuse		*eu1;
struct edgeuse		*eu2;
struct faceuse		*fu1;		/* fu of eu1, for plane equation */
struct faceuse		*fu2;		/* fu of eu2, for error checks */
{
	point_t		eu1_start;
	point_t		eu1_end;
	vect_t		eu1_dir;
	point_t		eu2_start;
	point_t		eu2_end;
	vect_t		eu2_dir;
	vect_t		dir3d;
	fastf_t		dist[2];
	int		status;
	point_t		hit_pt;
	struct vertexuse	*vu;
	struct vertexuse	*vu1a, *vu1b;
	struct vertexuse	*vu2a, *vu2b;
	struct model		*m;
	int		ret = 0;
	int		wire = 1;	/* at least one arg is a wire */
	long		*magic1;
	long		*magic2;

	NMG_CK_INTER_STRUCT(is);
	NMG_CK_EDGEUSE(eu1);
	NMG_CK_EDGEUSE(eu2);
	m = nmg_find_model(&eu1->l.magic);
	NMG_CK_MODEL(m);
	/*
	 * Important note:  don't use eu1->eumate_p->vu_p here,
	 * because that vu is in the opposite orientation faceuse.
	 * Putting those vu's on the intersection line makes for big trouble.
	 */
	vu1a = eu1->vu_p;
	vu1b = RT_LIST_PNEXT_CIRC( edgeuse, eu1 )->vu_p;
	vu2a = eu2->vu_p;
	vu2b = RT_LIST_PNEXT_CIRC( edgeuse, eu2 )->vu_p;
	NMG_CK_VERTEXUSE(vu1a);
	NMG_CK_VERTEXUSE(vu1b);
	NMG_CK_VERTEXUSE(vu2a);
	NMG_CK_VERTEXUSE(vu2b);

	if( fu1 && fu2 )  wire = 0;	/* No wire edges */

	/* Note what parent to make self-loops in:  faceuse, or shell */
	if( fu1 )
		magic1 = &fu1->l.magic;
	else
		magic1 = &nmg_find_s_of_eu(eu1)->l.magic;
	if( fu2 )
		magic2 = &fu2->l.magic;
	else
		magic2 = &nmg_find_s_of_eu(eu2)->l.magic;

	if (rt_g.NMG_debug & DEBUG_POLYSECT)  {
		rt_log("nmg_isect_edge2p_edge2p(eu1=x%x, eu2=x%x) START\n\tfu1=x%x, fu2=x%x\n\tvu1a=%x vu1b=%x, vu2a=%x vu2b=%x\n\tv1a=%x v1b=%x,   v2a=%x v2b=%x\n",
			eu1, eu2,
			fu1, fu2,
			vu1a, vu1b, vu2a, vu2b,
			vu1a->v_p, vu1b->v_p, vu2a->v_p, vu2b->v_p );
	}

	/*
	 *  Topology check.
	 *  If both endpoints of both edges match, this is a trivial accept.
	 */
	if( vu1a->v_p == vu2a->v_p && vu1b->v_p == vu2b->v_p )  {
		if (rt_g.NMG_debug & DEBUG_POLYSECT)
			rt_log("nmg_isect_edge2p_edge2p: shared edge topology, both ends\n");
    		nmg_radial_join_eu(eu1, eu2, &is->tol );
	    	nmg_enlist_vu( is, vu1a, vu2a );
	    	nmg_enlist_vu( is, vu1b, vu2b );
	    	ret = ISECT_SHARED_V;
		goto out;		/* vu1a, vu1b already listed */
	}
	if( vu1a->v_p == vu2b->v_p && vu1b->v_p == vu2a->v_p )  {
		if (rt_g.NMG_debug & DEBUG_POLYSECT)
			rt_log("nmg_isect_edge2p_edge2p: shared edge topology, both ends, reversed.\n");
    		nmg_radial_join_eu(eu1, eu2, &is->tol );
	    	nmg_enlist_vu( is, vu1a, vu2b );
	    	nmg_enlist_vu( is, vu1b, vu2a );
	    	ret = ISECT_SHARED_V;
		goto out;		/* vu1a, vu1b already listed */
	}

	/*
	 *  The 3D line in is->pt and is->dir is prepared by the caller.
	 *  is->pt is *not* one of the endpoints of this edge.
	 *
	 *  IMPORTANT NOTE:  The edge-ray used for the edge intersection
	 *  calculations is colinear with the "intersection line",
	 *  but the edge-ray starts at vu1a and points to vu1b,
	 *  while the intersection line has to satisfy different constraints.
	 *  Don't confuse the two!
	 */
	nmg_get_2d_vertex( eu1_start, vu1a->v_p, is, fu2 );	/* 2D line */
	nmg_get_2d_vertex( eu1_end, vu1b->v_p, is, fu2 );
	VSUB2_2D( eu1_dir, eu1_end, eu1_start );

	nmg_get_2d_vertex( eu2_start, vu2a->v_p, is, fu2 );
	nmg_get_2d_vertex( eu2_end, vu2b->v_p, is, fu2 );
	VSUB2_2D( eu2_dir, eu2_end, eu2_start );

	dist[0] = dist[1] = 0;	/* for clean prints, below */

	/* The "proper" thing to do is intersect two line segments.
	 * However, this means that none of the intersections of edge "line"
	 * with the exterior of the loop are computed, and that
	 * violates the strategy assumptions of the face-cutter.
	 */
	/* To pick up ALL intersection points, the source edge is a line */
	status = rt_isect_line2_lseg2( dist, eu1_start, eu1_dir,
			eu2_start, eu2_dir, &is->tol );

	if (rt_g.NMG_debug & DEBUG_POLYSECT) {
		rt_log("\trt_isect_line2_lseg2()=%d, dist: %g, %g\n",
			status, dist[0], dist[1] );
	}

	/*
	 *  Whether geometry hits or misses, as long as not colinear, check topo.
	 *  If one endpoint matches, and edges are not colinear,
	 *  then accept the one shared vertex as the intersection point.
	 *  Can't do this before geometry check, or we might miss the
	 *  colinear condition, and not do the mutual intersection.
	 */
	if( status != 0 &&
	    (vu1a->v_p == vu2a->v_p || vu1a->v_p == vu2b->v_p ||
	    vu1b->v_p == vu2a->v_p || vu1b->v_p == vu2b->v_p )
	)  {
		if (rt_g.NMG_debug & DEBUG_POLYSECT)
			rt_log("edge2p_edge2p: non-colinear edges share one vertex (topology)\n");
		if( vu1a->v_p == vu2a->v_p )
			nmg_enlist_vu( is, vu1a, vu2a );
		else if( vu1a->v_p == vu2b->v_p )
			nmg_enlist_vu( is, vu1a, vu2b );

		if( vu1b->v_p == vu2a->v_p )
			nmg_enlist_vu( is, vu1b, vu2a );
		else if( vu1b->v_p == vu2b->v_p )
			nmg_enlist_vu( is, vu1b, vu2b );

		ret = ISECT_SHARED_V;
		goto out;		/* vu1a, vu1b already listed */
	}

	if (status < 0)  {
		ret = ISECT_NONE;	/* No geometric intersection */
		goto topo;		/* Still need to list vu1a, vu2b */
	}

	if( status == 0 )  {
		/* Lines are co-linear and on line of intersection. */
		/* Perform full mutual intersection, and vu enlisting. */
		if( nmg_isect_2colinear_edge2p( eu1, eu2, fu2, is ) > 2 )  {
			/* Can't tell which edgeuse(s) got split */
			ret = ISECT_SPLIT1 | ISECT_SPLIT2;
		} else {
			/* XXX Can't tell if some sharing ensued.  Does it matter? */
			/* No, not for the one place we are called. */
			ret = ISECT_NONE;
		}
		goto out;		/* vu1a, vu1b listed by nmg_isect_2colinear_edge2p */
	}

	/* There is only one intersect point.  Break one or both edges. */


	/* The ray defined by the edgeuse line eu1 intersects the lseg eu2.
	 * Tolerances have already been factored in.
	 * The edge exists over values of 0 <= dist <= 1.
	 */
	VSUB2( dir3d, vu1b->v_p->vg_p->coord, vu1a->v_p->vg_p->coord );
	VJOIN1( hit_pt, vu1a->v_p->vg_p->coord, dist[0], dir3d );

	if ( dist[0] == 0 )  {
		/* First point of eu1 is on eu2, by geometry */
		if (rt_g.NMG_debug & DEBUG_POLYSECT)
			rt_log("\tvu=x%x vu1a is intersect point\n", vu1a);
		if( dist[1] < 0 || dist[1] > 1 )  {
			if (rt_g.NMG_debug & DEBUG_POLYSECT)
				rt_log("\teu1 line intersects eu2 outside vu2a...vu2b range, ignore.\n");
			ret = ISECT_NONE;
			goto topo;
		}

		/* Edges not colinear. Either join up with a matching vertex,
		 * or break eu2 on our vert.
		 */
		if( dist[1] == 0 )  {
			if (rt_g.NMG_debug & DEBUG_POLYSECT)
				rt_log("\tvu2a matches vu1a\n");
			nmg_jv(vu1a->v_p, vu2a->v_p);
			nmg_enlist_vu( is, vu1a, vu2a );
			ret = ISECT_SHARED_V;
			goto topo;
		}
		if( dist[1] == 1 )  {
			if (rt_g.NMG_debug & DEBUG_POLYSECT)
				rt_log("\tsecond point of eu2 matches vu1a\n");
			nmg_jv(vu1a->v_p, vu2b->v_p);
			nmg_enlist_vu( is, vu1a, vu2b );
			ret = ISECT_SHARED_V;
			goto topo;
		}
		/* Break eu2 on our first vertex */
		if (rt_g.NMG_debug & DEBUG_POLYSECT)
			rt_log("\tbreaking eu2 on vu1a\n");
		vu = nmg_ebreak( vu1a->v_p, eu2 )->vu_p;
		nmg_enlist_vu( is, vu1a, vu );
		ret = ISECT_SPLIT2;	/* eu1 not broken, just touched */
		goto topo;
	}

	if ( dist[0] == 1 )  {
		/* Second point of eu1 is on eu2, by geometry */
		if (rt_g.NMG_debug & DEBUG_POLYSECT)
			rt_log("\tvu=x%x vu1b is intersect point\n", vu1b);
		if( dist[1] < 0 || dist[1] > 1 )  {
			if (rt_g.NMG_debug & DEBUG_POLYSECT)
				rt_log("\teu1 line intersects eu2 outside vu2a...vu2b range, ignore.\n");
			ret = ISECT_NONE;
			goto topo;
		}

		/* Edges not colinear. Either join up with a matching vertex,
		 * or break eu2 on our vert.
		 */
		if( dist[1] == 0 )  {
			if (rt_g.NMG_debug & DEBUG_POLYSECT)
				rt_log("\tvu2a matches vu1b\n");
			nmg_jv(vu1b->v_p, vu2a->v_p);
			nmg_enlist_vu( is, vu1b, vu2a );
			ret = ISECT_SHARED_V;
			goto topo;
		}
		if( dist[1] == 1 )  {
			if (rt_g.NMG_debug & DEBUG_POLYSECT)
				rt_log("\tsecond point of eu2 matches vu1b\n");
			nmg_jv(vu1b->v_p, vu2b->v_p);
			nmg_enlist_vu( is, vu1b, vu2b );
			ret = ISECT_SHARED_V;
			goto topo;
		}
		/* Break eu2 on our second vertex */
		if (rt_g.NMG_debug & DEBUG_POLYSECT)
			rt_log("\tbreaking eu2 on vu1b\n");
		vu = nmg_ebreak( vu1b->v_p, eu2 )->vu_p;
		nmg_enlist_vu( is, vu1b, vu );
		ret = ISECT_SPLIT2;	/* eu1 not broken, just touched */
		goto topo;
	}

	/*  eu2 intersect point is on eu1 line, but not between vertices.
	 *  Since it crosses the line of intersection, it must be broken.
	 */
	if( dist[0] < 0 || dist[0] > 1 )  {
		struct loopuse *plu;
		if (rt_g.NMG_debug & DEBUG_POLYSECT)
			rt_log("\tIntersect point on eu2 is outside vu1a...vu1b.  Break eu2 anyway.\n");

		if( dist[1] == 0 )  {
			nmg_enlist_vu( is, vu2a, 0 );
			ret = ISECT_SHARED_V;		/* eu1 was not broken */
			goto topo;
		} else if( dist[1] == 1 )  {
			nmg_enlist_vu( is, vu2b, 0 );
			ret = ISECT_SHARED_V;		/* eu1 was not broken */
			goto topo;
		} else if( dist[1] > 0 && dist[1] < 1 )  {
			/* Break eu2 somewhere in the middle */
			struct vertexuse	*new_vu2;
			struct vertex		*new_v2;
			if (rt_g.NMG_debug & DEBUG_POLYSECT)
			    	VPRINT("\t\tBreaking eu2 at intersect point", hit_pt);
			new_v2 = nmg_find_pt_in_model(m, hit_pt, &(is->tol) );
			new_vu2 = nmg_ebreak( new_v2, eu2 )->vu_p;
			if( !new_v2 )  {
				/* A new vertex was created, assign geom */
				nmg_vertex_gv( new_vu2->v_p, hit_pt );	/* 3d geom */
			}
			nmg_enlist_vu( is, new_vu2, 0 );
			ret = ISECT_SPLIT2;	/* eu1 was not broken */
			goto topo;
		}

		/* Hit point not on either eu1 or eu2, nothing to do */
		ret = ISECT_NONE;
		goto topo;
	}

	/* Intersection is in the middle of the reference edge (eu1) */
	/* dist[0] >= 0 && dist[0] <= 1 ) */
	if (rt_g.NMG_debug & DEBUG_POLYSECT)
		rt_log("\tintersect is in middle of eu1, breaking it\n");

	/* Edges not colinear. Either join up with a matching vertex,
	 * or break eu2 on our vert.
	 */
	if( dist[1] == 0 )  {
		if (rt_g.NMG_debug & DEBUG_POLYSECT)
			rt_log("\t\tintersect point is vu2a\n");
		vu = nmg_ebreak( vu2a->v_p, eu1 )->vu_p;
		nmg_enlist_vu( is, vu2a, vu );
		ret |= ISECT_SPLIT1;
		goto topo;
	} else if( dist[1] == 1 )  {
		if (rt_g.NMG_debug & DEBUG_POLYSECT)
			rt_log("\t\tintersect point is vu2b\n");
		vu = nmg_ebreak( vu2b->v_p, eu1 )->vu_p;
		nmg_enlist_vu( is, vu2b, vu );
		ret |= ISECT_SPLIT1;
		goto topo;
	} else if( dist[1] > 0 && dist[1] < 1 )  {
		/* Intersection is in the middle of both, split edge */
		struct vertex	*new_v;
		if (rt_g.NMG_debug & DEBUG_POLYSECT)
		    	VPRINT("\t\tBreaking both edges at intersect point", hit_pt);
		ret = ISECT_SPLIT1 | ISECT_SPLIT2;
		new_v = nmg_e2break( eu1, eu2 );
		nmg_vertex_gv( new_v, hit_pt );	/* 3d geometry */

		/* new_v is at far end of eu1 and eu2 */
		if( eu1->eumate_p->vu_p->v_p != new_v ) rt_bomb("new_v 1\n");
		if( eu2->eumate_p->vu_p->v_p != new_v ) rt_bomb("new_v 2\n");
		/* Can't use eumate_p here, it's in wrong orientation face */
		nmg_enlist_vu( is, RT_LIST_PNEXT_CIRC(edgeuse,eu1)->vu_p,
			RT_LIST_PNEXT_CIRC(edgeuse,eu2)->vu_p );
		goto topo;
	} else {
		/* Intersection is in middle of eu1, which lies on the
		 * line of intersection being computed, but is outside
		 * the endpoints of eu2.  There is no point in breaking
		 * eu1 here -- it does not connnect up with anything.
		 */
		ret = ISECT_NONE;
		goto topo;
	}

topo:
	/*
	 *  Listing of any vu's from eu2 will have been done above.
	 *
	 *  The *original* vu1a and vu1b (and their duals) MUST be
	 *  forcibly listed on
	 *  the intersection line, since eu1 lies ON the line!
	 *
	 *  This is done last, so that the intersection code (above) has
	 *  the opportunity to create the duals.
	 *  vu1a and vu1b don't have to have anything to do with eu2,
	 *  hence the 2nd vu argument is unspecified (0).
	 *  For our purposes here, we will be satisfied with *any* use
	 *  of the same vertex in the other face.
	 */
	nmg_enlist_vu( is, vu1a, 0 );
	nmg_enlist_vu( is, vu1b, 0 );
out:
	/* By here, vu1a and vu1b MUST have been enlisted */
	if (rt_g.NMG_debug & DEBUG_POLYSECT)  {
		rt_log("nmg_isect_edge2p_edge2p(eu1=x%x, eu2=x%x) END, ret=%d %s%s%s\n",
			eu1, eu2, ret,
			(ret&ISECT_SHARED_V)? "SHARED_V|" :
				((ret==0) ? "NONE" : ""),
			(ret&ISECT_SPLIT1)? "SPLIT1|" : "",
			(ret&ISECT_SPLIT2)? "SPLIT2" : ""
		);
	}

	return ret;
}

/*
 *			N M G _ I S E C T _ E D G E 3 P _ F A C E 3 P
 *
 *  Intersect an edge eu1 with a faceuse fu2.
 *  eu1 may belong to fu1, or it may be a wire edge.
 *
 *  XXX It is not clear whether we need the caller to provide the
 *  line equation, or if we should just create it here.
 *  If done here, the start pt needs to be outside fu2 (fu1 also?)
 *
 *  Returns -
 *	0	If everything went well
 *	1	If vu[] list along the intersection line needs to be re-done.
 */
static int
nmg_isect_edge3p_face3p(is, eu1, fu2)
struct nmg_inter_struct *is;
struct edgeuse		*eu1;
struct faceuse		*fu2;
{
	struct vertexuse *vu1_final = (struct vertexuse *)NULL;
	struct vertexuse *vu2_final = (struct vertexuse *)NULL;
	struct vertex	*v1a;		/* vertex at start of eu1 */
	struct vertex	*v1b;		/* vertex at end of eu1 */
	struct vertex	*v2;
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
	struct faceuse	*fu1;		/* fu that contains eu1 */
	plane_t		n2;
	int		ret = 0;

	if (rt_g.NMG_debug & DEBUG_POLYSECT)
		rt_log("nmg_isect_edge3p_face3p(, eu1=x%x, fu2=x%x) START\n", eu1, fu2);

	NMG_CK_INTER_STRUCT(is);
	NMG_CK_EDGEUSE(eu1);
	NMG_CK_VERTEXUSE(eu1->vu_p);
	v1a = eu1->vu_p->v_p;
	NMG_CK_VERTEX(v1a);
	NMG_CK_VERTEX_G(v1a->vg_p);

	NMG_CK_EDGEUSE(eu1->eumate_p);
	NMG_CK_VERTEXUSE(eu1->eumate_p->vu_p);
	v1b = eu1->eumate_p->vu_p->v_p;
	NMG_CK_VERTEX(v1b);
	NMG_CK_VERTEX_G(v1b->vg_p);

	NMG_CK_FACEUSE(fu2);
	if( fu2->orientation != OT_SAME )  rt_bomb("nmg_isect_edge3p_face3p() fu2 not OT_SAME\n");
	fu1 = nmg_find_fu_of_eu(eu1);	/* May be NULL */

	/*
	 *  Form a ray that starts at one vertex of the edgeuse
	 *  and points to the other vertex.
	 */
	VSUB2(edge_vect, v1b->vg_p->coord, v1a->vg_p->coord);
	edge_len = MAGNITUDE(edge_vect);

	VMOVE( start_pt, v1a->vg_p->coord );

	{
		/* XXX HACK */
		double	dot;
		dot = fabs( VDOT( is->dir, edge_vect ) / edge_len ) - 1;
		if( !NEAR_ZERO( dot, .01 ) )  {
			rt_log("HACK HACK cough cough.  Resetting is->pt, is->dir\n");
			VPRINT("old is->pt ", is->pt);
			VPRINT("old is->dir", is->dir);
			VMOVE( is->pt, start_pt );
			VMOVE( is->dir, edge_vect );
			VUNITIZE(is->dir);
			VPRINT("new is->pt ", is->pt);
			VPRINT("new is->dir", is->dir);
		}
	}

	NMG_GET_FU_PLANE( n2, fu2 );
	if (rt_g.NMG_debug & DEBUG_POLYSECT)  {
		rt_log("Testing (%g, %g, %g) -> (%g, %g, %g) dir=(%g, %g, %g)\n",
			V3ARGS(start_pt),
			V3ARGS(v1b->vg_p->coord),
			V3ARGS(edge_vect) );
		PLPRINT("\t", n2);
	}

	status = rt_isect_line3_plane(&dist, start_pt, edge_vect,
		n2, &is->tol);

	if (rt_g.NMG_debug & DEBUG_POLYSECT) {
	    if (status >= 0)
		rt_log("\tHit. rt_isect_line3_plane=%d, dist=%g (%e)\n",
			status, dist, dist);
	    else
		rt_log("\tMiss. Boring status of rt_isect_line3_plane: %d\n",
			status);
	}
	if( status == 0 )  {
		struct nmg_inter_struct	is2;

		/*
		 *  Edge (ray) lies in the plane of the other face,
		 *  by geometry.  Drop into 2D code to handle all
		 *  possible intersections (there may be many),
		 *  and any cut/joins, then resume with the previous work.
		 */
		if (rt_g.NMG_debug & DEBUG_POLYSECT)  {
			rt_log("nmg_isect_edge3p_face3p: edge lies ON face, using 2D code\n@ @ @ @ @ @ @ @ @ @ 2D CODE, START\n");
			rt_log("  The status of the face/face intersect line, before 2d:\n");
			nmg_pr_ptbl_vert_list( "l1", is->l1 );
			nmg_pr_ptbl_vert_list( "l2", is->l2 );
		}

		is2 = *is;	/* make private copy */
		is2.vert2d = 0;	/* Don't use previously initialized stuff */

		ret = nmg_isect_edge2p_face2p( &is2, eu1, fu2, fu1 );

		nmg_isect2d_cleanup( &is2 );

		/*
		 *  Because nmg_isect_edge2p_face2p() calls the face cutter,
		 *  vu's in lone lu's that are listed in the current l1 or
		 *  l2 lists may have been destroyed.  It's ret is ours.
		 */

		/* Only do this if list is still OK */
		if (rt_g.NMG_debug & DEBUG_POLYSECT && ret == 0)  {
			rt_log("nmg_isect_edge3p_face3p: @ @ @ @ @ @ @ @ @ @ 2D CODE, END, resume 3d problem.\n");
			rt_log("  The status of the face/face intersect line, so far:\n");
			nmg_pr_ptbl_vert_list( "l1", is->l1 );
			nmg_pr_ptbl_vert_list( "l2", is->l2 );
		}

		/* See if start vertex is now shared */
		if (vu2_final=nmg_find_v_in_face(eu1->vu_p->v_p, fu2)) {
			if (rt_g.NMG_debug & DEBUG_POLYSECT)
				rt_log("\tEdge start vertex lies on other face (2d topology).\n");
			vu1_final = eu1->vu_p;
			(void)nmg_tbl(is->l1, TBL_INS_UNIQUE, &vu1_final->l.magic);
			(void)nmg_tbl(is->l2, TBL_INS_UNIQUE, &vu2_final->l.magic);
		}
vu1_final = vu1_final = (struct vertexuse *)NULL;	/* XXX HACK HACK -- shut off error checking */
		goto out;
	}

	/*
	 *  We now know that the the edge does not lie +in+ the other face,
	 *  so it will intersect the face in at most one point.
	 *  Before looking at the results of the geometric calculation,
	 *  check the topology.  If the topology says that starting vertex
	 *  of this edgeuse is on the other face, that is the hit point.
	 *  Enter the two vertexuses of that starting vertex in the list,
	 *  and return.
	 *
	 *  XXX Lee wonders if there might be a benefit to violating the
	 *  XXX "only ask geom question once" rule, and doing a geom
	 *  XXX calculation here before the topology check.
	 */
	if (vu2_final=nmg_find_v_in_face(v1a, fu2)) {
		vu1_final = eu1->vu_p;
		if (rt_g.NMG_debug & DEBUG_POLYSECT) {
			rt_log("\tEdge start vertex lies on other face (topology).\n\tAdding vu1_final=x%x (v=x%x), vu2_final=x%x (v=x%x)\n",
				vu1_final, vu1_final->v_p,
				vu2_final, vu2_final->v_p);
		}
		(void)nmg_tbl(is->l1, TBL_INS_UNIQUE, &vu1_final->l.magic);
		(void)nmg_tbl(is->l2, TBL_INS_UNIQUE, &vu2_final->l.magic);
		goto out;
	}

	if (status < 0)  {
		/*  Ray does not strike plane.
		 *  See if start point lies on plane.
		 */
		dist = VDOT( start_pt, n2 ) - n2[3];
		if( !NEAR_ZERO( dist, is->tol.dist ) )
			goto out;		/* No geometric intersection */

		/* XXX Does this ever happen, now that geom calc is done
		 * XXX above, and there is 2D handling as well?  Lets find out.
		 */
		rt_bomb("nmg_isect_edge3p_face3p: Edge start vertex lies on other face (geometry)\n");

		/* Start point lies on plane of other face */
		if (rt_g.NMG_debug & DEBUG_POLYSECT)
			rt_log("\tEdge start vertex lies on other face (geometry)\n");
		dist = VSUB2DOT( v1a->vg_p->coord, start_pt, edge_vect )
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
	 * The vertices are "fattened" by +/- is->tol units.
	 */
	dist_to_plane = edge_len * dist;

	if (rt_g.NMG_debug & DEBUG_POLYSECT)
		rt_log("\tedge_len=%g, dist=%g, dist_to_plane=%g\n",
			edge_len, dist, dist_to_plane);

	if ( dist_to_plane < -is->tol.dist )  {
		/* Hit is behind first point */
		if (rt_g.NMG_debug & DEBUG_POLYSECT)
			rt_log("\tplane behind first point\n");
		goto out;
	}

	if ( dist_to_plane > edge_len + is->tol.dist) {
		if (rt_g.NMG_debug & DEBUG_POLYSECT)
			rt_log("\tplane beyond second point\n");
		goto out;
	}

	VJOIN1( hit_pt, start_pt, dist, edge_vect );

	/* Check hit_pt against face/face intersection line */
	{
		fastf_t	ff_dist;
		ff_dist = rt_dist_line_point( is->pt, is->dir, hit_pt );
		if( ff_dist > is->tol.dist )  {
			rt_log("WARNING nmg_isect_edge3p_face3p() hit_pt off f/f line %g*tol (%e, tol=%e)\n",
				ff_dist/is->tol.dist,
				ff_dist, is->tol.dist);
			/* XXX now what? */
		}
	}

	/*
	 * If the vertex on the other end of this edgeuse is on the face,
	 * then make a linkage to an existing face vertex (if found),
	 * and give up on this edge, knowing that we'll pick up the
	 * intersection of the next edgeuse with the face later.
	 */
	if ( dist_to_plane < is->tol.dist )  {
		/* First point is on plane of face, by geometry */
		if (rt_g.NMG_debug & DEBUG_POLYSECT)
			rt_log("\tedge starts at plane intersect\n");
		vu1_final = eu1->vu_p;
		vu2_final = nmg_enlist_vu( is, vu1_final, 0 );
		goto out;
	}

	if ( dist_to_plane < edge_len - is->tol.dist) {
		/* Intersection is between first and second vertex points.
		 * Insert new vertex at intersection point.
		 */
		vu2_final = nmg_break_3edge_at_plane(hit_pt, fu2, is, eu1);
		if( vu2_final )
			vu1_final = RT_LIST_PNEXT_CIRC(edgeuse,eu1)->vu_p;
		goto out;
	}

#if 0
	if ( dist_to_plane <= edge_len + is->tol.dist)
#endif
	{
		/* Second point is on plane of face, by geometry */
		if (rt_g.NMG_debug & DEBUG_POLYSECT)
			rt_log("\tedge ends at plane intersect\n");

		eunext = RT_LIST_PNEXT_CIRC(edgeuse,eu1);
		NMG_CK_EDGEUSE(eunext);
		if( eunext->vu_p->v_p != v1b )
			rt_bomb("nmg_isect_edge3p_face3p: discontinuous eu loop\n");

		vu1_final = eunext->vu_p;
		vu2_final = nmg_enlist_vu( is, vu1_final, 0 );
		goto out;
	}

out:
	/* If vu's were added to list, run some quick checks here */
	if( vu1_final && vu2_final )  {
		fastf_t	dist;

		if( vu1_final->v_p != vu2_final->v_p )  rt_bomb("nmg_isect_edge3p_face3p() vertex mis-match\n");

		dist = rt_dist_line_point( is->pt, is->dir,
			vu1_final->v_p->vg_p->coord );
		if( dist > 100*is->tol.dist )  {
			rt_log("ERROR nmg_isect_edge3p_face3p() vu1=x%x point off line by %g > 100*dist_tol (%g)\n",
				vu1_final, dist, 100*is->tol.dist);
			VPRINT("is->pt|", is->pt);
			VPRINT("is->dir", is->dir);
			VPRINT(" coord ", vu1_final->v_p->vg_p->coord );
			rt_bomb("nmg_isect_edge3p_face3p()\n");
		}
		if( dist > is->tol.dist )  {
			rt_log("WARNING nmg_isect_edge3p_face3p() vu1=x%x pt off line %g*tol (%e, tol=%e)\n",
				vu1_final, dist/is->tol.dist,
				dist, is->tol.dist);
		}
	}

	if (rt_g.NMG_debug & DEBUG_POLYSECT)
		rt_log("nmg_isect_edge3p_face3p(, eu1=x%x, fu2=x%x) ret=%d END\n", eu1, fu2, ret);
	return ret;
}

/*
 *			N M G _ I S E C T _ L O O P 3 P _ F A C E 3 P
 *
 *	Intersect a single loop with another face.
 *	Note that it may be a wire loop.
 *
 *  Returns -
 *	 0	everything is ok
 *	>0	vu[] list along intersection line needs to be re-done.
 */
static int
nmg_isect_loop3p_face3p(bs, lu, fu)
struct nmg_inter_struct *bs;
struct loopuse *lu;
struct faceuse *fu;
{
	struct edgeuse	*eu;
	long		magic1;
	int		discards = 0;

	if (rt_g.NMG_debug & DEBUG_POLYSECT) {
		plane_t		n;
		rt_log("nmg_isect_loop3p_face3p(, lu=x%x, fu=x%x) START\n", lu, fu);
		NMG_GET_FU_PLANE( n, fu );
		HPRINT("  fg N", n);
	}

	NMG_CK_INTER_STRUCT(bs);
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
		nmg_isect_3vertex_3face(bs, vu, fu);
		return 0;
	} else if (magic1 != NMG_EDGEUSE_MAGIC) {
		rt_bomb("nmg_isect_loop3p_face3p() Unknown type of NMG loopuse\n");
	}

	/*  Process loop consisting of a list of edgeuses.
	 *
	 * By going backwards around the list we avoid
	 * re-processing an edgeuse that was just created
	 * by nmg_isect_edge3p_face3p.  This is because the edgeuses
	 * point in the "next" direction, and when one of
	 * them is split, it inserts a new edge AHEAD or
	 * "nextward" of the current edgeuse.
	 */ 
	for( eu = RT_LIST_LAST(edgeuse, &lu->down_hd );
	     RT_LIST_NOT_HEAD(eu,&lu->down_hd);
	     eu = RT_LIST_PLAST(edgeuse,eu) )  {
		NMG_CK_EDGEUSE(eu);

		if (eu->up.magic_p != &lu->l.magic) {
			rt_bomb("nmg_isect_loop3p_face3p: edge does not share loop\n");
		}

		discards += nmg_isect_edge3p_face3p(bs, eu, fu);

		nmg_ck_lueu(lu, "nmg_isect_loop3p_face3p");
	}

	if (rt_g.NMG_debug & DEBUG_POLYSECT)  {
		rt_log("nmg_isect_loop3p_face3p(, lu=x%x, fu=x%x) END, discards=%d\n", lu, fu, discards);
	}
	return discards;
}

/*
 *			N M G _ I S E C T _ E D G E 2 P _ F A C E 2 P
 *
 *  Given one (2D) edge (eu1) lying in the plane of another face (fu2),
 *  intersect with all the other edges of that face.
 *  This edge represents a line of intersection, and so a
 *  cutjoin/mesh pass will be needed for each one.
 *
 *  XXX eu1 may be a wire edge, in which case there is no face!
 *
 *  Note that this routine completely conducts the
 *  intersection operation, so that edges may come and go, loops
 *  may join or split, each time it is called.
 *  This imposes special requirements on handling the march through
 *  the linked lists in this routine.
 *
 *  This also means that much of argument "is" is changed each call.
 *
 *  It further means that vu's in lone lu's found along the edge
 *  "intersection line" here may get merged in, causing the lu to
 *  be killed, and the vu, which is listed in the 3D (calling)
 *  routine's l1/l2 list, is now invalid.
 *
 *  NOTE-
 *  Since this routine calls the face cutter, *all* points of intersection
 *  along the line, for *both* faces, need to be found.
 *  Otherwise, the parity requirements of the face cutter will be violated.
 *  This means that eu1 needs to be intersected with all of fu1 also,
 *  including itself (so that the vu's at the ends of eu1 are listed).
 *
 *  Returns -
 *	0	Topology is completely shared (or no sharing).  l1/l2 valid.
 *	>0	Caller needs to invalidate his l1/l2 list.
 */
static int
nmg_isect_edge2p_face2p( is, eu1, fu2, fu1 )
struct nmg_inter_struct	*is;
struct edgeuse		*eu1;		/* edge to be intersected w/fu2 */
struct faceuse		*fu2;		/* face to be intersected w/eu1 */
struct faceuse		*fu1;		/* fu that eu1 is from */
{
	struct nmg_ptbl vert_list1, vert_list2;
	struct loopuse		*lu;
	struct vertexuse	*vu1;
	struct vertexuse	*vu2;
	struct edgeuse		*fu2_eu;	/* use of edge in fu2 */
	struct xray		line;
	point_t			min_pt;
	point_t			max_pt;
	vect_t			invdir;
	int			another_pass;
	int			total_splits = 0;
	int			ret = 0;

	NMG_CK_INTER_STRUCT(is);
	NMG_CK_EDGEUSE(eu1);
	NMG_CK_FACEUSE(fu2);
	/* fu1 may be null */

	if (rt_g.NMG_debug & DEBUG_POLYSECT)
		rt_log("nmg_isect_edge2p_face2p(eu1=x%x, fu2=x%x, fu1=x%x) START\n", eu1, fu2, fu1);

	if( fu2->orientation != OT_SAME )  rt_bomb("nmg_isect_edge2p_face2p() fu2 not OT_SAME\n");
	if( fu1 && fu1->orientation != OT_SAME )  rt_bomb("nmg_isect_edge2p_face2p() fu1 not OT_SAME\n");

	/*  See if an edge exists in other face that connects these 2 verts */
	fu2_eu = nmg_find_eu_in_face( eu1->vu_p->v_p, eu1->eumate_p->vu_p->v_p,
	    fu2, (CONST struct edgeuse *)NULL, 0 );
	if( fu2_eu != (struct edgeuse *)NULL )  {
		/* There is an edge in other face that joins these 2 verts. */
		NMG_CK_EDGEUSE(fu2_eu);
		if( fu2_eu->e_p != eu1->e_p )  {
			/* Not the same edge, fuse! */
			rt_log("nmg_isect_edge2p_face2p() fusing unshared shared edge\n");
			nmg_radial_join_eu( eu1, fu2_eu, &is->tol );
		}
		/* Topology is completely shared */
		if (rt_g.NMG_debug & DEBUG_POLYSECT)
			rt_log("nmg_isect_edge2p_face2p() topology is shared\n");
		ret = 0;
		goto do_ret;
	}

	/* Zap 2d cache, we could be switching faces now */
	nmg_isect2d_cleanup(is);

	(void)nmg_tbl(&vert_list1, TBL_INIT,(long *)NULL);
	(void)nmg_tbl(&vert_list2, TBL_INIT,(long *)NULL);

    	is->l1 = &vert_list1;
    	is->l2 = &vert_list2;
	is->s1 = nmg_find_s_of_eu(eu1);		/* may be wire edge */
	is->s2 = fu2->s_p;
	is->fu1 = fu1;
	is->fu2 = fu2;

    	if ( fu1 && rt_g.NMG_debug & (DEBUG_POLYSECT|DEBUG_FCUT|DEBUG_MESH)
    	    && rt_g.NMG_debug & DEBUG_PLOTEM) {
    	    	nmg_pl_2fu( "Iface%d.pl", 0, fu2, fu1, 0 );
    	}

	/*
	 *  Construct the ray which contains the line of intersection,
	 *  i.e. the line that contains the edge "eu1".
	 *
	 *  See the comment in nmg_isect_two_generic_faces() for details
	 *  on the constraints on this ray, and the algorithm.
	 */
	vu1 = eu1->vu_p;
	vu2 = RT_LIST_PNEXT_CIRC( edgeuse, eu1 )->vu_p;
	if( vu1->v_p == vu2->v_p )  {
		if (rt_g.NMG_debug & DEBUG_POLYSECT)
			rt_log("nmg_isect_edge2p_face2p(eu1=x%x) skipping 0-len edge (topology)\n", eu1);
		goto out;
	}
	VMOVE( line.r_pt, vu1->v_p->vg_p->coord );		/* 3D line */
	VSUB2( line.r_dir, vu2->v_p->vg_p->coord, line.r_pt );
	VUNITIZE( line.r_dir );
	VINVDIR( invdir, line.r_dir );

	/* nmg_loop_g() makes sure there are no 0-thickness faces */
	VMOVE( min_pt, fu2->f_p->min_pt );
	VMOVE( max_pt, fu2->f_p->max_pt );

	if( !rt_in_rpp( &line, invdir, min_pt, max_pt ) )  {
		/* The edge ray missed the face RPP, nothing to do. */
		if (rt_g.NMG_debug & DEBUG_POLYSECT)  {
			VPRINT("r_pt ", line.r_pt);
			VPRINT("r_dir", line.r_dir);
			VPRINT("fu2 min", fu2->f_p->min_pt);
			VPRINT("fu2 max", fu2->f_p->max_pt);
			VPRINT("min_pt", min_pt);
			VPRINT("max_pt", max_pt);
			rt_log("r_min=%g, r_max=%g\n", line.r_min, line.r_max);
			rt_log("nmg_isect_edge2p_face2p() edge ray missed face bounding RPP\n");
		}
		goto out;
	}
	if (rt_g.NMG_debug & DEBUG_POLYSECT)  {
		VPRINT("fu2 min", fu2->f_p->min_pt);
		VPRINT("fu2 max", fu2->f_p->max_pt);
		rt_log("r_min=%g, r_max=%g\n", line.r_min, line.r_max);
	}
	/* Start point will lie at min or max dist, outside of face RPP */
	VJOIN1( is->pt, line.r_pt, line.r_min, line.r_dir );
	if( line.r_min > line.r_max )  {
		/* Direction is heading the wrong way, flip it */
		VREVERSE( is->dir, line.r_dir );
		if (rt_g.NMG_debug & DEBUG_POLYSECT)
			rt_log("flipping dir\n");
	} else {
		VMOVE( is->dir, line.r_dir );
	}
	if (rt_g.NMG_debug & DEBUG_POLYSECT)  {
		VPRINT("r_pt ", line.r_pt);
		VPRINT("r_dir", line.r_dir);
		VPRINT("->pt ", is->pt);
		VPRINT("->dir", is->dir);
	}

	if( rt_g.NMG_debug & DEBUG_VERIFY )  {
		nmg_fu_touchingloops(fu2);
		if(fu1)nmg_fu_touchingloops(fu1);
		nmg_region_v_unique( is->s1->r_p, &is->tol );
		nmg_region_v_unique( is->s2->r_p, &is->tol );
	}

	/* The use of an infinite line through fu2 is essential! */
	/* Put eu1's vu's on the list.  They are the hardest to get
	 * right by geometry. (Needed to fix Test18) */
	/* XXX What about join operations on colinear edgeuses? */
/* XXX YYY ZZZ What about proper breaking of colinear edgeuses, too! */
/* XXX Can I just enlist them, and let the face cutter deal with it? */
/* XXX Should get list of all eu's on edge_g, and enlist all their vu's! */
	nmg_enlist_vu( is, eu1->vu_p, 0 );
	nmg_enlist_vu( is, RT_LIST_PNEXT_CIRC(edgeuse,eu1)->vu_p, 0 );

	/* Run eu1's line through fu2 */
	total_splits = nmg_isect_line2_face2p( is, &vert_list2, fu2, fu1 );
	/* Now, run line through fu1, if eu1 is not wire */
	if( fu1 )  {
		total_splits += nmg_isect_line2_face2p( is,
			&vert_list1, fu1, fu2 );
	}
	if (rt_g.NMG_debug & DEBUG_POLYSECT )
		rt_log("nmg_isect_edge2p_face2p(): total_splits=%d\n", lu, total_splits);
	/* XXX Should modify caller to tally all eu's sharing this edge_g */


	if( total_splits <= 0 )  goto out;

    	if (rt_g.NMG_debug & DEBUG_FCUT) {
	    	rt_log("nmg_isect_edge2p_face2p(eu1=x%x, fu2=x%x) vert_lists C:\n", eu1, fu2 );
    		nmg_pr_ptbl_vert_list( "vert_list1", &vert_list1 );
    		nmg_pr_ptbl_vert_list( "vert_list2", &vert_list2 );
    	}

	nmg_purge_unwanted_intersection_points(&vert_list1, fu2, &is->tol);
	if(fu1)nmg_purge_unwanted_intersection_points(&vert_list2, fu1, &is->tol);

    	if (rt_g.NMG_debug & DEBUG_FCUT) {
	    	rt_log("nmg_isect_edge2p_face2p(eu1=x%x, fu2=x%x) vert_lists D:\n", eu1, fu2 );
    		nmg_pr_ptbl_vert_list( "vert_list1", &vert_list1 );
    		nmg_pr_ptbl_vert_list( "vert_list2", &vert_list2 );
    	}

	if (vert_list1.end == 0 && vert_list2.end == 0) goto out;

	/* Invoke the face cutter to snip and join loops along isect line */
	nmg_face_cutjoin(&vert_list1, &vert_list2, fu1, fu2, is->pt, is->dir, &is->tol);
	ret = 1;		/* face cutter was called. */

out:
	(void)nmg_tbl(&vert_list1, TBL_FREE, (long *)NULL);
	(void)nmg_tbl(&vert_list2, TBL_FREE, (long *)NULL);

do_ret:
	if (rt_g.NMG_debug & DEBUG_POLYSECT)  {
		rt_log("nmg_isect_edge2p_face2p(eu1=x%x, fu2=x%x) ret=%d\n",
			eu1, fu2, ret);
	}
	return ret;
}

/*
 *			N M G _ I S E C T _ T W O _ F A C E 2 P
 *
 *  Manage the mutual intersection of two 3-D coplanar planar faces.
 */
static void
nmg_isect_two_face2p( is, fu1, fu2 )
struct nmg_inter_struct	*is;
struct faceuse		*fu1, *fu2;
{
	struct loopuse	*lu;
	struct edgeuse	*eu;
	struct vertexuse	*vu;

	NMG_CK_INTER_STRUCT(is);
	NMG_CK_FACEUSE(fu1);
	NMG_CK_FACEUSE(fu2);
	is->fu1 = fu1;
	is->fu2 = fu2;

#if 0
	/* r71, r23 are useful demonstrations */
	/* Turn these flags on only for early code debugging */
	rt_g.NMG_debug |= DEBUG_POLYSECT;
	rt_g.NMG_debug |= DEBUG_FCUT;
	rt_g.NMG_debug |= DEBUG_VU_SORT;
	rt_g.NMG_debug |= DEBUG_PLOTEM;
	rt_g.NMG_debug |= DEBUG_INS;
	rt_g.NMG_debug |= DEBUG_FCUT;
#endif

	if (rt_g.NMG_debug & DEBUG_POLYSECT)
		rt_log("nmg_isect_two_face2p(fu1=x%x, fu2=x%x) START\n", fu1, fu2);

	/* For every edge in f1, intersect with f2, incl. cutjoin */
f1_again:
	if( rt_g.NMG_debug & DEBUG_VERIFY )  {
nmg_fu_touchingloops(fu1);
nmg_fu_touchingloops(fu2);
nmg_region_v_unique( fu1->s_p->r_p, &is->tol );
nmg_region_v_unique( fu2->s_p->r_p, &is->tol );
	}
	for( RT_LIST_FOR( lu, loopuse, &fu1->lu_hd ) )  {
		NMG_CK_LOOPUSE(lu);
		if( RT_LIST_FIRST_MAGIC( &lu->down_hd ) == NMG_VERTEXUSE_MAGIC )  {
			vu = RT_LIST_FIRST( vertexuse, &lu->down_hd );
			nmg_isect_vert2p_face2p( is, vu, fu2 );
			continue;
		}
		for( RT_LIST_FOR( eu, edgeuse, &lu->down_hd ) )  {
			NMG_CK_EDGEUSE(eu);

			if( nmg_isect_edge2p_face2p( is, eu, fu2, fu1 ) )  {
				/* Face topologies have changed */
				goto f1_again;
			}
		}
	}

	/* Zap 2d cache, we are switching faces now */
	nmg_isect2d_cleanup(is);

	/* For every edge in f2, intersect with f1, incl. cutjoin */
f2_again:
	if( rt_g.NMG_debug & DEBUG_VERIFY )  {
nmg_fu_touchingloops(fu1);
nmg_fu_touchingloops(fu2);
nmg_region_v_unique( fu1->s_p->r_p, &is->tol );
nmg_region_v_unique( fu2->s_p->r_p, &is->tol );
	}
	for( RT_LIST_FOR( lu, loopuse, &fu2->lu_hd ) )  {
		NMG_CK_LOOPUSE(lu);
		if( RT_LIST_FIRST_MAGIC( &lu->down_hd ) == NMG_VERTEXUSE_MAGIC )  {
			vu = RT_LIST_FIRST( vertexuse, &lu->down_hd );
			nmg_isect_vert2p_face2p( is, vu, fu1 );
			continue;
		}
		for( RT_LIST_FOR( eu, edgeuse, &lu->down_hd ) )  {
			NMG_CK_EDGEUSE(eu);

			if( nmg_isect_edge2p_face2p( is, eu, fu1, fu2 ) )  {
				/* Face topologies have changed */
				goto f2_again;
			}
		}
	}
	if( rt_g.NMG_debug & DEBUG_VERIFY )  {
nmg_fu_touchingloops(fu1);
nmg_fu_touchingloops(fu2);
nmg_region_v_unique( fu1->s_p->r_p, &is->tol );
nmg_region_v_unique( fu2->s_p->r_p, &is->tol );
	}
	if (rt_g.NMG_debug & DEBUG_POLYSECT)
		rt_log("nmg_isect_two_face2p(fu1=x%x, fu2=x%x) END\n", fu1, fu2);
}

/*
 * NEWLINE!
 *			N M G _ I S E C T _ L I N E 2 _ E D G E 2 P
 *
 *  A parallel to nmg_isect_edge2p_edge2p().
 *
 *  Intersect the line with eu1, from fu1.
 *  The resulting vu's are added to "list", not is->l1 or is->l2.
 *  fu2 is the "other" face on this intersect line, and is used only
 *  when searching for existing vertex structs suitable for re-use.
 *
 *  Returns -
 *	Number of times edge is broken (0 or 1).
 */
int
nmg_isect_line2_edge2p( is, list, eu1, fu1, fu2 )
struct nmg_inter_struct	*is;
struct nmg_ptbl		*list;
struct edgeuse		*eu1;
struct faceuse		*fu1;
struct faceuse		*fu2;
{
	point_t		eu1_start;	/* 2D */
	point_t		eu1_end;	/* 2D */
	vect_t		eu1_dir;	/* 2D */
	fastf_t		dist[2];
	int		status;
	point_t		hit_pt;		/* 3D */
	struct vertexuse	*vu;
	struct vertexuse	*vu1a, *vu1b;
	int			ret = 0;

	NMG_CK_INTER_STRUCT(is);
	NMG_CK_PTBL(list);
	NMG_CK_EDGEUSE(eu1);
	NMG_CK_FACEUSE(fu1);
	NMG_CK_FACEUSE(fu2);

	/*
	 * Important note:  don't use eu1->eumate_p->vu_p here,
	 * because that vu is in the opposite orientation faceuse.
	 * Putting those vu's on the intersection line makes for big trouble.
	 */
	vu1a = eu1->vu_p;
	vu1b = RT_LIST_PNEXT_CIRC( edgeuse, eu1 )->vu_p;
	NMG_CK_VERTEXUSE(vu1a);
	NMG_CK_VERTEXUSE(vu1b);

	if (rt_g.NMG_debug & DEBUG_POLYSECT)  {
		rt_log("nmg_isect_line2_edge2p(eu1=x%x, fu1=x%x)\n\tvu1a=%x vu1b=%x\n\tv2a=%x v2b=%x\n",
			eu1, fu1,
			vu1a, vu1b,
			vu1a->v_p, vu1b->v_p );
	}

	/*
	 *  The 3D line in is->pt and is->dir is prepared by the caller.
	 */
	nmg_get_2d_vertex( eu1_start, vu1a->v_p, is, fu1 );
	nmg_get_2d_vertex( eu1_end, vu1b->v_p, is, fu1 );
	VSUB2_2D( eu1_dir, eu1_end, eu1_start );

	dist[0] = dist[1] = 0;	/* for clean prints, below */

	/* Intersect the line with the edge, in 2D */
	status = rt_isect_line2_lseg2( dist, is->pt2d, is->dir2d,
			eu1_start, eu1_dir, &is->tol );

	if (rt_g.NMG_debug & DEBUG_POLYSECT) {
		rt_log("\trt_isect_line2_lseg2()=%d, dist: %g, %g\n",
			status, dist[0], dist[1] );
	}

	if (status < 0)  goto out;	/* No geometric intersection */

	if( status == 0 )  {
		/*
		 *  The edge is colinear with the line.
		 *  List both vertexuse structures, and return.
		 */
		if (rt_g.NMG_debug & DEBUG_POLYSECT)
			rt_log("\t\tedge colinear with isect line.  Listing vu1a, vu1b\n");
		nmg_enlist_vu(is, vu1a, 0);
		nmg_enlist_vu(is, vu1b, 0);
		ret = 0;
		goto out;
	}

	/* There is only one intersect point.  Break the edge there. */

	VJOIN1( hit_pt, is->pt, dist[0], is->dir );	/* 3D hit */

	/* Edges not colinear. Either list a vertex,
	 * or break eu1.
	 */
	if( status == 1 || dist[1] == 0 )  {
		if (rt_g.NMG_debug & DEBUG_POLYSECT)
			rt_log("\t\tintersect point is vu1a\n");
		if( !rt_pt3_pt3_equal(hit_pt, vu1a->v_p->vg_p->coord, &(is->tol) ) )
			rt_bomb("vu1a does not match calculated point\n");
		nmg_enlist_vu(is, vu1a, 0);
		ret = 0;
	} else if( status == 2 || dist[1] == 1 )  {
		if (rt_g.NMG_debug & DEBUG_POLYSECT)
			rt_log("\t\tintersect point is vu1b\n");
		if( !rt_pt3_pt3_equal(hit_pt, vu1b->v_p->vg_p->coord, &(is->tol) ) )
			rt_bomb("vu1b does not match calculated point\n");
		nmg_enlist_vu(is, vu1b, 0);
		ret = 0;
	} else {
		/* Intersection is in the middle of eu1, split edge */
		struct vertexuse	*vu1_final;
		struct vertex		*new_v;
		if (rt_g.NMG_debug & DEBUG_POLYSECT)  {
			fastf_t	dist;
			int	code;
			rt_log("\t2D: pt2d=(%g, %g), dir2d=(%g, %g)\n",
				is->pt2d[X], is->pt2d[Y],
				is->dir2d[X], is->dir2d[Y] );
			rt_log("\t2D: eu1_start=(%g, %g), eu1_dir=(%g, %g)\n",
				eu1_start[X], eu1_start[Y],
				eu1_dir[X], eu1_dir[Y] );
			VPRINT("\t3D: is->pt ", is->pt);
			VPRINT("\t3D: is->dir", is->dir);
			rt_log("\t2d: Breaking eu1 at isect.\n");
			VPRINT("  vu1a", vu1a->v_p->vg_p->coord);
			VPRINT("hit_pt", hit_pt);
			VPRINT("  vu1b", vu1b->v_p->vg_p->coord);
			/* XXX Perform a (not-so) quick check */
			code = rt_isect_pt_lseg( &dist, vu1a->v_p->vg_p->coord,
				vu1b->v_p->vg_p->coord,
				hit_pt, &(is->tol) );
			rt_log("\trt_isect_pt_lseg() dist=%g, ret=%d\n", dist, code);
			if( code < 0 )  rt_bomb("3D point not on 3D lseg\n");

			/* Ensure that the 3D hit_pt is between the end pts */
if( !rt_between(vu1a->v_p->vg_p->coord[X], hit_pt[X], vu1b->v_p->vg_p->coord[X], &(is->tol)) ||
    !rt_between(vu1a->v_p->vg_p->coord[Y], hit_pt[Y], vu1b->v_p->vg_p->coord[Y], &(is->tol)) ||
    !rt_between(vu1a->v_p->vg_p->coord[Z], hit_pt[Z], vu1b->v_p->vg_p->coord[Z], &(is->tol)) )  {
    	VPRINT("vu1a", vu1a->v_p->vg_p->coord);
    	VPRINT("hitp", hit_pt);
    	VPRINT("vu1b", vu1b->v_p->vg_p->coord);
    	rt_bomb("nmg_isect_line2_edge2p() hit point not between edge verts!\n");
}

		}

		/* if we can't find the appropriate vertex
		 * by a geometry search, build a new vertex.
		 * Otherwise, re-use the existing one.
		 * Can't just search other face, might miss relevant vert.
		 */
		new_v = nmg_find_pt_in_model(fu2->s_p->r_p->m_p, hit_pt, &(is->tol));
		vu1_final = nmg_ebreak(new_v, eu1)->vu_p;
		ret = 1;
		if( !new_v )  {
			nmg_vertex_gv( vu1_final->v_p, hit_pt );	/* 3d geom */
			if (rt_g.NMG_debug & DEBUG_POLYSECT)
				rt_log("\t\tmaking new vertex vu=x%x v=x%x\n",
					vu1_final, vu1_final->v_p);
		} else {
			if (rt_g.NMG_debug & DEBUG_POLYSECT)
				rt_log("\t\tre-using vertex v=x%x vu=x%x\n", new_v, vu1_final);
		}
		nmg_enlist_vu(is, vu1_final, 0);

		nmg_ck_face_worthless_edges( fu1 );
	}

out:
	if (rt_g.NMG_debug & DEBUG_POLYSECT)
		rt_log("nmg_isect_line2_edge2p(eu1=x%x, fu1=x%x) END ret=%d\n", eu1, fu1, ret);
	return ret;
}

/*
 *			N M G _ I S E C T _ L I N E 2 _ V E R T E X 2
 *
 *  If this lone vertex lies along the intersect line, then add it to
 *  the lists.
 */
void
nmg_isect_line2_vertex2( is, list1, vu1, fu1, fu2 )
struct nmg_inter_struct	*is;
struct nmg_ptbl		*list1;
struct vertexuse	*vu1;
struct faceuse		*fu1;
struct faceuse		*fu2;
{
	point_t		v2d;

	NMG_CK_INTER_STRUCT(is);
	NMG_CK_PTBL(list1);
	NMG_CK_VERTEXUSE(vu1);
	NMG_CK_FACEUSE(fu1);
	NMG_CK_FACEUSE(fu2);

	if (rt_g.NMG_debug & DEBUG_POLYSECT)
		rt_log("nmg_isect_line2_vertex2(vu=x%x)\n", vu1);

	nmg_get_2d_vertex( v2d, vu1->v_p, is, fu1 );

	if( rt_distsq_line2_point2( is->pt2d, is->dir2d, v2d ) > is->tol.dist_sq )
		return;

	if (rt_g.NMG_debug & DEBUG_POLYSECT)
		rt_log("nmg_isect_line2_vertex2(vu=x%x) line hits vertex v=x%x\n", vu1, vu1->v_p);

	nmg_enlist_vu( is, vu1, 0 );
}

/*
 *  NEWLINE2
 *
 *			N M G _ I S E C T _ L I N E _ E D G E G E O M 2 P
 *
 *  Given an edgeuse eu1 in fu1 with edge geometry eg1,
 *  and a (2 face intersection) line which lies in the plane of fu1,
 *  calculate the intersection of eg1 with the line,
 *  and apply that result to all edgeuses which share eg1.
 *
 *  This routine replaces the incremental approach, which tended to
 *  degenerate when a long edge was broken very near an edgeuse.
 *  When the angle from the edge to the line was small, that close
 *  extra vertexuse would also get listed on the line of intersection,
 *  playing havoc with the state transition mechanism of the face cutter.
 *
 *  In this routine, two non-colinear lines will intersect in exactly one pt.
 *
 *  Returns -
 *	Number of times an edge is broken (0 or >=1).
 */
int
nmg_isect_line2_edgegeom2p( is, list, eu1, fu1, fu2 )
struct nmg_inter_struct	*is;
struct nmg_ptbl		*list;
struct edgeuse		*eu1;
struct faceuse		*fu1;
struct faceuse		*fu2;
{
	point_t		eu1_pt2d;	/* 2D */
	point_t		eu1_end2d;	/* 2D */
	fastf_t		dist[2];
	int		status;
	point_t		hit3d;
	point_t		hit2d;
	struct vertexuse	*vu;
	struct vertexuse	*vu1a, *vu1b;
	int			ret = 0;
	struct edge_g		*eg1;
	struct nmg_ptbl		eutab;
	point_t		eg_pt2d;	/* 2D */
	vect_t		eg_dir2d;	/* 2D */
	struct edgeuse	**eup;
	struct vertex	*hit_v;
	int		code;
	int		n_colinear;
	fastf_t		min_dist_sq;

	eutab.magic = 0;

	NMG_CK_INTER_STRUCT(is);
	NMG_CK_PTBL(list);
	NMG_CK_EDGEUSE(eu1);
	NMG_CK_FACEUSE(fu1);
	NMG_CK_FACEUSE(fu2);

	eg1 = eu1->e_p->eg_p;
	/* Might this be null? */
	NMG_CK_EDGE_G(eg1);

	if (rt_g.NMG_debug & DEBUG_POLYSECT)  {
		rt_log("nmg_isect_line2_edgegeom2p(eg1=x%x, fu1=x%x) eu1=x%x START\n",
			eg1, fu1, eu1);
	}

	/*  The 3D line in is->pt and is->dir is prepared by our caller. */
	/*  So is the 2D line in is->pt2d and is->dir2d. */
	MAT4X3PNT( eg_pt2d, is->proj, eg1->e_pt );
	MAT4X3VEC( eg_dir2d, is->proj, eg1->e_dir );

	/*
	 *  Determine if eg1 is colinear with the line of intersection. 2D
	 */
	dist[0] = dist[1] = -INFINITY;
	code = rt_isect_line2_line2( dist, is->pt2d, is->dir2d,
		eg_pt2d, eg_dir2d, &(is->tol) );
	if( code < 0 )  {
		/* Lines are parallel, no intersection, nothing to do. */
		ret = 0;
		goto out;
	}

	/* Assemble the list of eu's in fu1 that use eg1 */
	nmg_edgeuse_with_eg_tabulate( &eutab, &fu1->l.magic, eg1 );
	if (rt_g.NMG_debug & DEBUG_POLYSECT)  {
		rt_log(" code=%d, dist[0]=%g\n", code, dist[0]);
		nmg_pr_ptbl("Edgeuse table", &eutab, 1);
	}

	if( fabs(VDOT_2D(is->dir2d, eg_dir2d)) < is->tol.para )
		goto not_colinear;

	/*
	 *  Even though rt_isect_line2_line2() might have said "non-colinear",
	 *  the angles are close enough that it's worth searching properly.
	 *
	 *  eg1 is potentially colinear with line of intersection.
	 *  Test each edgeuse on eg1 individually, and count all those where
	 *  both vertices are within tolerance of the line.
	 *  The 3D version of the comparison is used here, for
	 *  consistency with similar checks in other routines.
	 */
	n_colinear = 0;
	for( eup = (struct edgeuse **)NMG_TBL_LASTADDR(&eutab);
	     eup >= (struct edgeuse **)NMG_TBL_BASEADDR(&eutab); eup--
	)  {
		FAST fastf_t		a, b;

		NMG_CK_EDGEUSE(*eup);

		vu1a = (*eup)->vu_p;
		vu1b = RT_LIST_PNEXT_CIRC( edgeuse, (*eup) )->vu_p;
		NMG_CK_VERTEXUSE(vu1a);
		NMG_CK_VERTEXUSE(vu1b);

		a = rt_distsq_line3_pt3( is->pt, is->dir, vu1a->v_p->vg_p->coord );
		b = rt_distsq_line3_pt3( is->pt, is->dir, vu1b->v_p->vg_p->coord );
		if (rt_g.NMG_debug & DEBUG_POLYSECT)  {
			rt_log("\tColinearity check *eup=x%x, a=%g, b=%g\n",
				*eup, a, b);
		}
		if( a > is->tol.dist_sq || b > is->tol.dist_sq )
			continue;
		n_colinear++;
	}
	if( n_colinear >= NMG_TBL_END(&eutab) )  {
		/* All edgeuses are colinear!  Enlist them and return. */
		if (rt_g.NMG_debug & DEBUG_POLYSECT)  {
			rt_log("\tAll colinear.  Enlisting all edgeuse vu's\n");
		}
		for( eup = (struct edgeuse **)NMG_TBL_LASTADDR(&eutab);
		     eup >= (struct edgeuse **)NMG_TBL_BASEADDR(&eutab); eup--
		)  {
			vu1a = (*eup)->vu_p;
			vu1b = RT_LIST_PNEXT_CIRC( edgeuse, (*eup) )->vu_p;
			nmg_enlist_vu(is, vu1a, 0);
			nmg_enlist_vu(is, vu1b, 0);
		}
		ret = 0;
		goto out;
	}
	if( n_colinear > 0 )  {
		/*
		 *  Only *some* of the edgeuses are colinear, yet they all
		 *  came from the same original edge, and share edge geometry.
		 *  Now what?
		 */
		if( code >= 1 )  {
			if (rt_g.NMG_debug & DEBUG_POLYSECT)  {
				rt_log("\t%d of %d eus claim colinearity.  Using isect point.\n",
					n_colinear, NMG_TBL_END(&eutab));
			}
			/* Don't enlist ANY of the eu's due to colinearity */
			goto not_colinear;
		}
		rt_log("\tOnly %d of %d edgeuses claim colinearity.\n",
			n_colinear, NMG_TBL_END(&eutab));
		rt_bomb("partially colinear edge geometry?\n");
	}
	/*  If rt_isect_line2_line2() says colinear, we are done. */
	if( code == 0 )  {
		/*
		 *  The edges themselves *all* disagree that they are colinear.
		 *  Since we don't have a hit distance to use,
		 *  call the 2 lines colinear and parallel.  No intersects.
		 */
		if (rt_g.NMG_debug & DEBUG_POLYSECT)  {
			rt_log("\tNo colinear edgeuses, assuming parallel lines.  Miss.\n");
		}
		ret = 0;
		goto out;
	}

	/*
	 *  Found the ONE point in 2D where the two lines intersect.
	 *  Before we believe that the rt_isect_line2_line2()'s hit point
	 *  is the place to break our edge, search for the vertex
	 *  closest to the line.  If it's within tolerance, use *that*
	 *  rather than breaking the edge.
	 */
not_colinear:
	min_dist_sq = INFINITY;
	for( eup = (struct edgeuse **)NMG_TBL_LASTADDR(&eutab);
	     eup >= (struct edgeuse **)NMG_TBL_BASEADDR(&eutab); eup--
	)  {
		FAST fastf_t		d;

		NMG_CK_EDGEUSE(*eup);

		vu1a = (*eup)->vu_p;
		vu1b = RT_LIST_PNEXT_CIRC( edgeuse, (*eup) )->vu_p;
		NMG_CK_VERTEXUSE(vu1a);
		NMG_CK_VERTEXUSE(vu1b);

		d = rt_distsq_line3_pt3( is->pt, is->dir, vu1a->v_p->vg_p->coord );
		if( d < min_dist_sq )  {
			min_dist_sq = d;
			hit_v = vu1a->v_p;
		}
		d = rt_distsq_line3_pt3( is->pt, is->dir, vu1b->v_p->vg_p->coord );
		if( d < min_dist_sq )  {
			min_dist_sq = d;
			hit_v = vu1b->v_p;
		}
	}
	if( min_dist_sq < is->tol.dist_sq )  {
		/* Found the vertex.  Enlist all uses of it on this edge_g. */
		if (rt_g.NMG_debug & DEBUG_POLYSECT)  {
			rt_log("\tExisting v=x%x is intersect point, dist=%g\n",
				hit_v, sqrt(min_dist_sq) );
		}
		for( eup = (struct edgeuse **)NMG_TBL_LASTADDR(&eutab);
		     eup >= (struct edgeuse **)NMG_TBL_BASEADDR(&eutab); eup--
		)  {
			NMG_CK_EDGEUSE(*eup);
			vu1a = (*eup)->vu_p;
			vu1b = RT_LIST_PNEXT_CIRC( edgeuse, (*eup) )->vu_p;
			NMG_CK_VERTEXUSE(vu1a);
			NMG_CK_VERTEXUSE(vu1b);
			if( vu1a->v_p == hit_v )
				nmg_enlist_vu(is, vu1a, 0);
			if( vu1b->v_p == hit_v )
				nmg_enlist_vu(is, vu1b, 0);
		}
		ret = 0;
		goto out;
	}

	VJOIN1( hit3d, is->pt, dist[0], is->dir );
	if (rt_g.NMG_debug & DEBUG_POLYSECT)  {
		VPRINT("\t2 lines intersect at", hit3d);
	}
	if( !V3PT_IN_RPP( hit3d, fu1->f_p->min_pt, fu1->f_p->max_pt ) )  {
		/* Lines intersect outside bounds of this face.  Done. */
		ret = 0;
		goto out;
	}

	VJOIN1_2D( hit2d, is->pt2d, dist[0], is->dir2d );
	hit_v = (struct vertex *)NULL;
	ret = 0;

	/*
	 *  Because of the possibility of "acordian pleat" loops that
	 *  run back and forth, it is possible that there might be
	 *  multiple vertexuses to list.
	 */
	for( eup = (struct edgeuse **)NMG_TBL_LASTADDR(&eutab);
	     eup >= (struct edgeuse **)NMG_TBL_BASEADDR(&eutab); eup--
	)  {
		fastf_t			ldist;
		struct vertexuse	*vu1_final;

		NMG_CK_EDGEUSE(*eup);
		if (rt_g.NMG_debug & DEBUG_POLYSECT)  {
			rt_log("\tConsidering *eup=x%x\n", *eup);
		}

		vu1a = (*eup)->vu_p;
		vu1b = RT_LIST_PNEXT_CIRC( edgeuse, (*eup) )->vu_p;
		NMG_CK_VERTEXUSE(vu1a);
		NMG_CK_VERTEXUSE(vu1b);

		/* First, a topology check of both endpoints */
		if( vu1a->v_p == hit_v )  {
hit_a:
			if (rt_g.NMG_debug & DEBUG_POLYSECT)  {
				rt_log("\tlisting intersect point at vu1a=x%x\n", vu1a);
			}
			nmg_enlist_vu(is, vu1a, 0);
		}
		if( vu1b->v_p == hit_v )  {
hit_b:
			if (rt_g.NMG_debug & DEBUG_POLYSECT)  {
				rt_log("\tlisting intersect point at vu1b=x%x\n", vu1b);
			}
			nmg_enlist_vu(is, vu1b, 0);
		}
		if( vu1a->v_p == hit_v || vu1b->v_p == hit_v )  continue;

		/*  Second, a geometry check on the edgeuse ENDPOINTS
		 *  -vs- the line segment.  This is 3D, for consistency
		 *  with comparisons elsewhere.
		 */
		if( rt_distsq_line3_pt3( is->pt, is->dir, vu1a->v_p->vg_p->coord ) <= is->tol.dist_sq  )  {
			hit_v = vu1a->v_p;
			goto hit_a;
		}
		if( rt_distsq_line3_pt3( is->pt, is->dir, vu1b->v_p->vg_p->coord ) <= is->tol.dist_sq )  {
			hit_v = vu1b->v_p;
			goto hit_b;
		}

		/* Third, a geometry check of the HITPT -vs- the line segment */
		nmg_get_2d_vertex( eu1_pt2d, vu1a->v_p, is, fu1 );
		nmg_get_2d_vertex( eu1_end2d, vu1b->v_p, is, fu1 );
		ldist = 0;
		code = rt_isect_pt2_lseg2( &ldist, eu1_pt2d, eu1_end2d, hit2d, &(is->tol) );
		if (rt_g.NMG_debug & DEBUG_POLYSECT)  {
			rt_log("\trt_isect_pt2_lseg2() returned %d, ldist=%g\n", code, ldist);
		}
		switch(code)  {
		case -2:
			continue;	/* outside lseg AB pts */
		default:
		case -1:
#if 0
			rt_log("rt_isect_pt2_lseg2() returned %d, dist[0]=%e\n", code, dist[0]);
			VPRINT("  pt", is->pt);
			VPRINT(" dir", is->dir);
			VPRINT("  A3", vu1a->v_p->vg_p->coord);
			VPRINT("Hit3", hit3d);
			VPRINT("  B3", vu1b->v_p->vg_p->coord);
			V2PRINT("\n pt2", is->pt2d);
			V2PRINT("dir2", is->dir2d);
			V2PRINT("  A2", eu1_pt2d );
			V2PRINT("Hit2", hit2d);
			V2PRINT("  B2", eu1_end2d );
			rt_log("not on line of AB within tolerance, continuing\n");
#endif
			continue;	/* Point not on lseg */
		case 1:
			/* Point is at A (vu1a) by geometry */
			hit_v = vu1a->v_p;
			if (rt_g.NMG_debug & DEBUG_POLYSECT) rt_log("\thit_v = x%x (vu1a)\n", hit_v);
			goto hit_a;
		case 2:
			/* Point is at B (vu1b) by geometry */
			hit_v = vu1b->v_p;
			if (rt_g.NMG_debug & DEBUG_POLYSECT) rt_log("\thit_v = x%x (vu1b)\n", hit_v);
			goto hit_b;
		case 3:
			/* Point hits the line segment amidships!  Split edge!
			 * If we don't have a hit vertex yet,
			 * search for one in whole model.
			 */
			if( !hit_v )  {
				hit_v = nmg_find_pt_in_model(fu1->s_p->r_p->m_p,
					hit3d, &(is->tol));
				if( hit_v == vu1a->v_p || hit_v == vu1b->v_p )
					rt_bomb("About to make 0-length edge!\n");
			}
			vu1_final = nmg_ebreak(hit_v, *eup)->vu_p;
			ret++;		/* tally # of edges broken */
			if( !hit_v )  {
				hit_v = vu1_final->v_p;
				nmg_vertex_gv( hit_v, hit3d );
				if (rt_g.NMG_debug & DEBUG_POLYSECT)  {
					rt_log("\tmaking new vertex vu=x%x hit_v=x%x\n",
						vu1_final, hit_v);
				}
			} else {
				if (rt_g.NMG_debug & DEBUG_POLYSECT)  {
					rt_log("\tre-using hit_v=x%x, vu=x%x\n", hit_v, vu1_final);
				}
				if( hit_v != vu1_final->v_p )  rt_bomb("hit_v changed?\n");
			}
			nmg_enlist_vu(is, vu1_final, 0);
			/* Neither old nor new edgeuse need further handling */
			break;
		}
	}

out:
	if(eutab.magic)  nmg_tbl( &eutab, TBL_FREE, (long *)0 );
	if (rt_g.NMG_debug & DEBUG_POLYSECT)  {
		rt_log("nmg_isect_line2_edgegeom2p(eg1=x%x, fu1=x%x) ret=%d END\n",
			eg1, fu1, ret);
	}
	return ret;
}

/*
 * NEWLINE!
 *			N M G _ I S E C T _ L I N E 2 _ F A C E 2 P
 *
 *  A parallel to nmg_isect_edge2p_face2p().
 *
 *  The line is provided by the caller, and lies in the plane of both
 *  face fu1 and face fu2.
 *  We are concerned only with intersection with fu1 here.
 *  See the comment in nmg_isect_two_generic_faces() for details
 *  on the constraints on this ray, and the algorithm.
 *
 *  Return -
 *	number of edges broken
 */
int
nmg_isect_line2_face2p(is, list, fu1, fu2)
struct nmg_inter_struct	*is;
struct nmg_ptbl		*list;
struct faceuse		*fu1;
struct faceuse		*fu2;
{
	struct loopuse		*lu1;
	int			nbreak = 0;
	struct model		*m;
	unsigned char		*tags;

	NMG_CK_INTER_STRUCT(is);
	NMG_CK_PTBL(list);
	NMG_CK_FACEUSE(fu1);
	NMG_CK_FACEUSE(fu2);
	m = fu1->s_p->r_p->m_p;
	NMG_CK_MODEL(m);

	if (rt_g.NMG_debug & DEBUG_POLYSECT)
		rt_log("nmg_isect_line2_face2p(, fu1=x%x)\n", fu1);

	/* Allocate map of edgegeom's visited */
	tags = (unsigned char *)rt_calloc( 2*m->maxindex+1, 1, "nmg_isect_line2_face2p() tags[]" );

	if( fu1->orientation != OT_SAME )  rt_bomb("nmg_isect_line2_face2p() fu1 not OT_SAME\n");

	/* XXX Check that line lies in plane of face */
	/* XXX Distance from start point to plane within tol? */

    	if (rt_g.NMG_debug & (DEBUG_POLYSECT|DEBUG_FCUT)
    	    && rt_g.NMG_debug & DEBUG_PLOTEM) {
    	    	/* XXX This routine is a real weakie */
    	    	nmg_plot_ray_face("face", is->pt, is->dir, fu1);
    	}

	/* Zap 2d cache, we should be switching faces now */
	nmg_isect2d_cleanup(is);

	/* Project the intersect line into 2D.  Build matrix first. */
	nmg_isect2d_prep( is, fu1->f_p );
	MAT4X3PNT( is->pt2d, is->proj, is->pt );
	MAT4X3VEC( is->dir2d, is->proj, is->dir );

nmg_ck_face_worthless_edges( fu1 );
	if( rt_g.NMG_debug & DEBUG_VERIFY )  {
nmg_fu_touchingloops(fu1);
nmg_region_v_unique( fu1->s_p->r_p, &is->tol );
	}
	/* Split all edges that cross the line of intersection */
	for( RT_LIST_FOR( lu1, loopuse, &fu1->lu_hd ) )  {
		struct edgeuse	*eu1;

		if( RT_LIST_FIRST_MAGIC( &lu1->down_hd ) == NMG_VERTEXUSE_MAGIC )  {
			struct vertexuse	*vu1;
			/* Intersect line with lone vertex vu1 */
			vu1 = RT_LIST_FIRST( vertexuse, &lu1->down_hd );
			NMG_CK_VERTEXUSE(vu1);
			nmg_isect_line2_vertex2( is, list, vu1, fu1, fu2 );
			/* nbreak is not incremented for a vert on the line */
			continue;
		}
		for( RT_LIST_FOR( eu1, edgeuse, &lu1->down_hd ) )  {
			struct edge_g	*eg1;
			/* intersect line with eu1 */
			eg1 = eu1->e_p->eg_p;
			/* If this eu's eg has been seen before, nothing more to do */
			if( eg1 && !NMG_INDEX_FIRST_TIME(tags, eg1) )  continue;

			nbreak += nmg_isect_line2_edgegeom2p( is, list, eu1, fu1, fu2 );

			if( rt_g.NMG_debug & DEBUG_VERIFY )  {
				nmg_fu_touchingloops(fu1);
				nmg_region_v_unique( fu1->s_p->r_p, &is->tol );
			}
		}
	}

	if( rt_g.NMG_debug & DEBUG_VERIFY )  {
nmg_fu_touchingloops(fu1);
nmg_region_v_unique( fu1->s_p->r_p, &is->tol );
	}

do_ret:
	rt_free( (char *)tags, "nmg_isect_line2_face2p() tags[]");
	if (rt_g.NMG_debug & DEBUG_POLYSECT)  {
		rt_log("nmg_isect_line2_face2p(fu1=x%x) END nbreak=%d\n", fu1, nbreak);
	}
	return nbreak;
}

/*
 * NEWLINE!
 *			N M G _ I S E C T _ T W O _ F A C E 3 P
 *
 *  Handle the complete mutual intersection of
 *  two 3-D non-coplanar planar faces,
 *  including cutjoin and meshing.
 *
 *  The line of intersection has already been computed.
 *  Handle as two 2-D line/face intersection problems
 */
static void
nmg_isect_two_face3pNEW( is, fu1, fu2 )
struct nmg_inter_struct	*is;
struct faceuse		*fu1, *fu2;
{
	struct nmg_ptbl vert_list1, vert_list2;
	int	again;		/* Need to do it again? */
	int	trips;		/* Number of trips through loop */

	NMG_CK_INTER_STRUCT(is);
	NMG_CK_FACEUSE(fu1);
	NMG_CK_FACEUSE(fu2);

	if (rt_g.NMG_debug & DEBUG_POLYSECT) {
		rt_log("nmg_isect_two_face3pNEW( fu1=x%x, fu2=x%x )  START12\n", fu1, fu2);
		VPRINT("isect ray is->pt ", is->pt);
		VPRINT("isect ray is->dir", is->dir);
	}

	if( rt_g.NMG_debug & DEBUG_VERIFY )  {
		nmg_vfu( &fu1->s_p->fu_hd, fu1->s_p );
		nmg_vfu( &fu2->s_p->fu_hd, fu2->s_p );
nmg_fu_touchingloops(fu1);
nmg_fu_touchingloops(fu2);
	}

	/* Verify that line is within tolerance of both planes */
	/* XXX */

	(void)nmg_tbl(&vert_list1, TBL_INIT,(long *)NULL);
	(void)nmg_tbl(&vert_list2, TBL_INIT,(long *)NULL);

    	is->l1 = &vert_list1;
    	is->l2 = &vert_list2;
	is->s1 = fu1->s_p;
	is->s2 = fu2->s_p;
	is->fu1 = fu1;
	is->fu2 = fu2;

    	if (rt_g.NMG_debug & (DEBUG_POLYSECT|DEBUG_FCUT|DEBUG_MESH)
    	    && rt_g.NMG_debug & DEBUG_PLOTEM) {
    	    	nmg_pl_2fu( "Iface%d.pl", 0, fu1, fu2, 0 );
    	}

	/* XXX Is an optimization based upon the return code here possible? */
	(void)nmg_isect_line2_face2p(is, &vert_list1, fu1, fu2);

	if( rt_g.NMG_debug & DEBUG_VERIFY )  {
nmg_fu_touchingloops(fu1);
nmg_fu_touchingloops(fu2);
		nmg_vfu( &fu1->s_p->fu_hd, fu1->s_p );
		nmg_vfu( &fu2->s_p->fu_hd, fu2->s_p );
	}

	/*
	 *  Now intersect the line with the other face.
	 */
    	if (rt_g.NMG_debug & DEBUG_FCUT) {
	    	rt_log("nmg_isect_two_face3pNEW(fu1=x%x, fu2=x%x) vert_lists A:\n", fu1, fu2);
    		nmg_pr_ptbl_vert_list( "vert_list1", &vert_list1 );
    		nmg_pr_ptbl_vert_list( "vert_list2", &vert_list2 );
    	}

	if (rt_g.NMG_debug & DEBUG_POLYSECT) {
		rt_log("nmg_isect_two_face3pNEW( fu1=x%x, fu2=x%x )  START21\n", fu1, fu2);
	}

    	is->l2 = &vert_list1;
    	is->l1 = &vert_list2;
	is->s2 = fu1->s_p;
	is->s1 = fu2->s_p;
	is->fu2 = fu1;
	is->fu1 = fu2;
	(void)nmg_isect_line2_face2p(is, &vert_list2, fu2, fu1);

	if( rt_g.NMG_debug & DEBUG_VERIFY )  {
nmg_fu_touchingloops(fu1);
nmg_fu_touchingloops(fu2);
		nmg_vfu( &fu1->s_p->fu_hd, fu1->s_p );
		nmg_vfu( &fu2->s_p->fu_hd, fu2->s_p );
	}

	nmg_purge_unwanted_intersection_points(&vert_list1, fu2, &is->tol);
	nmg_purge_unwanted_intersection_points(&vert_list2, fu1, &is->tol);

    	if (rt_g.NMG_debug & DEBUG_FCUT) {
	    	rt_log("nmg_isect_two_face3pNEW(fu1=x%x, fu2=x%x) vert_lists B:\n", fu1, fu2);
    		nmg_pr_ptbl_vert_list( "vert_list1", &vert_list1 );
    		nmg_pr_ptbl_vert_list( "vert_list2", &vert_list2 );
    	}

    	if (vert_list1.end == 0 && vert_list2.end == 0) {
    		/* there were no intersections */
    		goto out;
    	}

	if (rt_g.NMG_debug & DEBUG_POLYSECT) {
		rt_log("nmg_isect_two_face3pNEW( fu1=x%x, fu2=x%x )  MIDDLE\n", fu1, fu2);
	}

	nmg_face_cutjoin(&vert_list1, &vert_list2, fu1, fu2, is->pt, is->dir, &is->tol);

	if( rt_g.NMG_debug & DEBUG_VERIFY )  {
nmg_fu_touchingloops(fu1);
nmg_fu_touchingloops(fu2);
nmg_region_v_unique( fu1->s_p->r_p, &is->tol );
nmg_region_v_unique( fu2->s_p->r_p, &is->tol );
		nmg_vfu( &fu1->s_p->fu_hd, fu1->s_p );
		nmg_vfu( &fu2->s_p->fu_hd, fu2->s_p );
	}

	nmg_mesh_faces(fu1, fu2, &is->tol);
	if( rt_g.NMG_debug & DEBUG_VERIFY )  {
nmg_fu_touchingloops(fu1);
nmg_fu_touchingloops(fu2);
	}

#if 0
	show_broken_stuff((long *)fu1, (long **)NULL, 1, 0);
	show_broken_stuff((long *)fu2, (long **)NULL, 1, 0);
#endif

out:
	(void)nmg_tbl(&vert_list1, TBL_FREE, (long *)NULL);
	(void)nmg_tbl(&vert_list2, TBL_FREE, (long *)NULL);

	if( rt_g.NMG_debug & DEBUG_VERIFY )  {
		nmg_vfu( &fu1->s_p->fu_hd, fu1->s_p );
		nmg_vfu( &fu2->s_p->fu_hd, fu2->s_p );
	}
	if (rt_g.NMG_debug & DEBUG_POLYSECT) {
		rt_log("nmg_isect_two_face3pNEW( fu1=x%x, fu2=x%x )  END\n", fu1, fu2);
		VPRINT("isect ray is->pt ", is->pt);
		VPRINT("isect ray is->dir", is->dir);
	}
}

/*
 *			N M G _ I S E C T _ T W O _ G E N E R I C _ F A C E S
 *
 *	Intersect a pair of faces
 */
void
nmg_isect_two_generic_faces(fu1, fu2, tol)
struct faceuse		*fu1, *fu2;
CONST struct rt_tol	*tol;
{
	struct nmg_inter_struct	bs;
	int		i;
	plane_t		pl1, pl2;
	struct face	*f1;
	struct face	*f2;
	point_t		min_pt;
	int		status;

	RT_CK_TOL(tol);
	bs.magic = NMG_INTER_STRUCT_MAGIC;
	bs.vert2d = (fastf_t *)NULL;
	bs.tol = *tol;		/* struct copy */

	NMG_CK_FACEUSE(fu1);
	f1 = fu1->f_p;
	NMG_CK_FACE(f1);
	NMG_CK_FACE_G(f1->fg_p);

	NMG_CK_FACEUSE(fu2);
	f2 = fu2->f_p;
	NMG_CK_FACE(f2);
	NMG_CK_FACE_G(f2->fg_p);

	NMG_GET_FU_PLANE( pl1, fu1 );
	NMG_GET_FU_PLANE( pl2, fu2 );

	if (rt_g.NMG_debug & DEBUG_POLYSECT) {
		rt_log("\nnmg_isect_two_generic_faces(fu1=x%x, fu2=x%x)\n", fu1, fu2);

		rt_log("Planes\t%gx + %gy + %gz = %g\n\t%gx + %gy + %gz = %g\n",
			pl1[0], pl1[1], pl1[2], pl1[3],
			pl2[0], pl2[1], pl2[2], pl2[3]);
		rt_log( "Cosine of angle between planes = %g\n" , VDOT( pl1 , pl2 ) );
		rt_log( "fu1:\n" );
		nmg_pr_fu_briefly( fu1 , "\t" );
		rt_log( "fu2:\n" );
		nmg_pr_fu_briefly( fu2 , "\t" );
nmg_fu_touchingloops(fu1);
nmg_fu_touchingloops(fu2);
	}

	if( f1->fg_p == f2->fg_p )  {
		if (rt_g.NMG_debug & DEBUG_POLYSECT) {
			rt_log("co-planar faces (shared fg)\n");
		}
		goto coplanar;
	}

	if ( !V3RPP_OVERLAP_TOL(f2->min_pt, f2->max_pt,
	    f1->min_pt, f1->max_pt, &bs.tol) )  return;

	/*
	 *  The extents of face1 overlap the extents of face2.
	 *  Construct a ray which contains the line of intersection.
	 *  There are two choices for direction, and an infinite number
	 *  of candidate points.
	 *
	 *  The correct choice of this ray is very important, so that:
	 *	1)  All intersections are at positive distances on the ray,
	 *	2)  dir cross N will point "left".
	 *
	 *  These two conditions can be satisfied by intersecting the
	 *  line with the face's bounding RPP.  This will give two
	 *  points A and B, where A is closer to the min point of the RPP
	 *  and B is closer to the max point of the RPP.
	 *  Let bs.pt be A, and let bs.dir point from A towards B.
	 *  This choice will satisfy both constraints, above.
	 *
	 *  NOTE:  These conditions must be enforced in the 2D code, also.
	 */
	VMOVE(min_pt, f1->min_pt);
	VMIN(min_pt, f2->min_pt);
	status = rt_isect_2planes( bs.pt, bs.dir, pl1, pl2,
		min_pt, tol );

	if (rt_g.NMG_debug & DEBUG_POLYSECT) {
		rt_log( "\tnmg_isect_two_generic_faces: intersect ray start (%f , %f , %f )\n\t\tin direction (%f , %f , %f )\n",
			bs.pt[X],
			bs.pt[Y],
			bs.pt[Z],
			bs.dir[X],
			bs.dir[Y],
			bs.dir[Z] );
	}

	switch( status )  {
	case 0:
		if( fu1->f_p->fg_p == fu2->f_p->fg_p )  {
			rt_bomb("nmg_isect_two_generic_faces: co-planar faces not detected\n");
		}
		/* All is well */
		bs.coplanar = 0;
		nmg_isect_two_face3pNEW( &bs, fu1, fu2 );
		break;
	case -1:
		/* co-planar faces */
		rt_log("co-planar faces (rt_isect_2planes)  WARNING: faces not shared.\n");
coplanar:
		bs.coplanar = 1;
		nmg_isect_two_face2p( &bs, fu1, fu2 );
		break;
	case -2:
		/* parallel and distinct, no intersection */
		break;
	default:
		/* internal error */
		rt_log("ERROR nmg_isect_two_generic_faces() unable to find plane intersection\n");
		break;
	}

	nmg_isect2d_cleanup( &bs );

	/* Eliminate any OT_BOOLPLACE self-loops now. */
	nmg_sanitize_fu( fu1 );
	nmg_sanitize_fu( fu2 );

	/* Eliminate stray vertices that were added along edges in this step */
	(void)nmg_unbreak_region_edges( &fu1->l.magic );
	(void)nmg_unbreak_region_edges( &fu2->l.magic );

    	if ( fu1 && rt_g.NMG_debug & (DEBUG_POLYSECT|DEBUG_FCUT|DEBUG_MESH)
    	    && rt_g.NMG_debug & DEBUG_PLOTEM) {
    	    	static int nshell = 1;
    	    	char	name[32];
    	    	FILE	*fp;
    	    	struct model	*m = nmg_find_model( &fu1->l.magic );

    	    	/* Both at once */
    	    	nmg_pl_2fu( "Iface%d.pl", 0, fu2, fu1, 0 );

		/* Each in it's own file */
    	    	nmg_face_plot( fu1 );
    	    	nmg_face_plot( fu2 );

    	    	sprintf(name, "shellA%d.pl", nshell);
    	    	if( (fp = fopen(name, "w")) != NULL )  {
    	    		rt_log("Plotting to %s\n", name);
    	    		nmg_pl_s( fp, fu1->s_p );
    	    		fclose(fp);
    	    	}

    	    	sprintf(name, "shellB%d.pl", nshell);
    	    	if( (fp = fopen(name, "w")) != NULL )  {
    	    		rt_log("Plotting to %s\n", name);
    	    		nmg_pl_s( fp, fu2->s_p );
    	    		fclose(fp);
    	    	}

    	    	sprintf(name, "model%d.g", nshell);
		nmg_stash_model_to_file( name, m, "After 2d isect" );
    	    	nshell++;
    	}

	if( rt_g.NMG_debug & DEBUG_VERIFY )  {
nmg_region_v_unique( fu1->s_p->r_p, &bs.tol );
nmg_region_v_unique( fu2->s_p->r_p, &bs.tol );
nmg_fu_touchingloops(fu1);
nmg_fu_touchingloops(fu2);
nmg_ck_face_worthless_edges( fu1 );
nmg_ck_face_worthless_edges( fu2 );
	}
}

/*
 *			N M G _ I S E C T _ E D G E 3 P _ E D G E 3 P
 *
 *  Intersect one edge with another.  At least one is a wire edge;
 *  thus there is no face context or intersection line.
 *  If the edges are non-colinear, there will be at most one point of isect.
 *  If the edges are colinear, there may be two.
 */
static void
nmg_isect_edge3p_edge3p( is, eu1, eu2 )
struct nmg_inter_struct	*is;
struct edgeuse		*eu1;
struct edgeuse		*eu2;
{
	struct vertexuse	*vu1a;
	struct vertexuse	*vu1b;
	struct vertexuse	*vu2a;
	struct vertexuse	*vu2b;
	vect_t			eu1_dir;
	vect_t			eu2_dir;
	fastf_t			dist[2];
	int			status;
	struct vertex		*new_v;
	point_t			hit_pt;

	NMG_CK_INTER_STRUCT(is);
	NMG_CK_EDGEUSE(eu1);
	NMG_CK_EDGEUSE(eu2);

top:
	vu1a = eu1->vu_p;
	vu1b = RT_LIST_PNEXT_CIRC( edgeuse, eu1 )->vu_p;
	vu2a = eu2->vu_p;
	vu2b = RT_LIST_PNEXT_CIRC( edgeuse, eu2 )->vu_p;
	NMG_CK_VERTEXUSE(vu1a);
	NMG_CK_VERTEXUSE(vu1b);
	NMG_CK_VERTEXUSE(vu2a);
	NMG_CK_VERTEXUSE(vu2b);

	if (rt_g.NMG_debug & DEBUG_POLYSECT)
		rt_log("nmg_isect_edge3p_edge3p(eu1=x%x, eu2=x%x)\n\tvu1a=%x vu1b=%x, vu2a=%x vu2b=%x\n\tv1a=%x v1b=%x,   v2a=%x v2b=%x\n",
			eu1, eu2,
			vu1a, vu1b, vu2a, vu2b,
			vu1a->v_p, vu1b->v_p, vu2a->v_p, vu2b->v_p );

	/*
	 *  Topology check.
	 *  If both endpoints of both edges match, this is a trivial accept.
	 */
	if( (vu1a->v_p == vu2a->v_p && vu1b->v_p == vu2b->v_p) ||
	    (vu1a->v_p == vu2b->v_p && vu1b->v_p == vu2a->v_p) )  {
		if (rt_g.NMG_debug & DEBUG_POLYSECT)
			rt_log("nmg_isect_edge3p_edge3p: shared edge topology, both ends\n");
	    	if( eu1->e_p != eu2->e_p )
	    		nmg_radial_join_eu(eu1, eu2, &is->tol );
	    	return;
	}
	VSUB2( eu1_dir, vu1b->v_p->vg_p->coord, vu1a->v_p->vg_p->coord );
	VSUB2( eu2_dir, vu2b->v_p->vg_p->coord, vu2a->v_p->vg_p->coord );

	dist[0] = dist[1] = 0;	/* for clean prints, below */

	status = rt_isect_lseg3_lseg3( dist,
			vu1a->v_p->vg_p->coord, eu1_dir,
			vu2a->v_p->vg_p->coord, eu2_dir, &is->tol );

	if (rt_g.NMG_debug & DEBUG_POLYSECT) {
		rt_log("\trt_isect_line3_lseg3()=%d, dist: %g, %g\n",
			status, dist[0], dist[1] );
	}

	if( status < 0 )  {
		/* missed */
		return;
	}

	if( status == 0 )  {
		int	nbreak;
		/* lines are colinear */
		rt_bomb("nmg_isect_edge3p_edge3p() colinear case.  Write some code here.\n");
		/* Initialize 2D vertex cache with EDGE info. */
		nmg_isect2d_prep( is, (struct face *)eu1->e_p );
		(void)nmg_isect_2colinear_edge2p( eu1, eu2, eu1->e_p, is );
		/* XXX The support for this won't quite work yet */
		return;
	}

	/* XXX There is an intersection point.  This could just be
	 * reformulated as a 2D problem here, and passed off to
	 * the 2D routine that this was based on.
	 */

	/* dist[0] is distance along eu1 */
	if( dist[0] == 0 )  {
		/* Hit is at vu1a */
		if( dist[1] == 0 )  {
			/* Hit is at vu2a */
			nmg_jv( vu1a->v_p, vu2a->v_p );
			return;
		} else if( dist[1] == 1 )  {
			/* Hit is at vu2b */
			nmg_jv( vu1a->v_p, vu2b->v_p );
			return;
		}
		/* Break eu2 on vu1a */
		nmg_ebreak( vu1a->v_p, eu2 );
		return;
	} else if( dist[0] == 1 )  {
		/* Hit is at vu1b */
		if( dist[1] == 0 )  {
			/* Hit is at vu2a */
			nmg_jv( vu1b->v_p, vu2a->v_p );
			return;
		} else if( dist[1] == 1 )  {
			/* Hit is at vu2b */
			nmg_jv( vu1b->v_p, vu2b->v_p );
			return;
		}
		/* Break eu2 on vu1b */
		nmg_ebreak( vu1b->v_p, eu2 );
		return;
	} else {
		/* Hit on eu1 is between vu1a and vu1b */
		if( dist[1] < 0 || dist[1] > 1 )  return;	/* Don't bother breaking eu1, it doesn't touch eu2. */

		if( dist[1] == 0 )  {
			/* Hit is at vu2a */
			nmg_ebreak( vu2a->v_p, eu1 );
			return;
		} else if( dist[1] == 1 )  {
			/* Hit is at vu2b */
			nmg_ebreak( vu2b->v_p, eu1 );
			return;
		}
		/* Hit is amidships on both eu1 and eu2. */
		new_v = nmg_e2break( eu1, eu2 );

		VJOIN1( hit_pt, vu2a->v_p->vg_p->coord, dist[1], eu2_dir );
		nmg_vertex_gv(new_v, hit_pt);
	}
}

/*
 *			N M G _ I S E C T _ V E R T E X 3 _ E D G E 3 P
 *
 *  Intersect a lone vertex from s1 with a single edge from s2.
 */
static void
nmg_isect_vertex3_edge3p( is, vu1, eu2 )
struct nmg_inter_struct		*is;
struct vertexuse	*vu1;
struct edgeuse		*eu2;
{
	fastf_t		dist;
	int		code;
	struct vertexuse	*vu2;

	NMG_CK_INTER_STRUCT(is);
	NMG_CK_VERTEXUSE(vu1);
	NMG_CK_EDGEUSE(eu2);

	code = rt_isect_pt_lseg( &dist, eu2->vu_p->v_p->vg_p->coord,
		eu2->vu_p->v_p->vg_p->coord,
		vu1->v_p->vg_p->coord, &is->tol );

	if( code < 0 )  return;		/* Not on line */
	switch( code )  {
	case 1:
		/* Hit is at A */
		vu2 = eu2->vu_p;
		break;
	case 2:
		/* Hit is at B */
		vu2 = RT_LIST_NEXT( edgeuse, &eu2->l)->vu_p;
		break;
	case 3:
		/* Hit is in the span AB somewhere, break edge */
		vu2 = nmg_ebreak( vu1->v_p, eu2 )->vu_p;
		break;
	default:
		rt_bomb("nmg_isect_vertex3_edge3p()\n");
	}
	/* Make sure verts are shared at hit point. They _should_ already be. */
	nmg_jv( vu1->v_p, vu2->v_p );
	(void)nmg_tbl(is->l1, TBL_INS_UNIQUE, &vu1->l.magic);
	(void)nmg_tbl(is->l2, TBL_INS_UNIQUE, &vu2->l.magic);
}

/*
 *			N M G _ I S E C T _ E D G E 3 P _ S H E L L
 *
 *  Intersect one edge with all of another shell.
 *  There is no face context for this edge, because
 *
 *  At present, this routine is used for only two purposes:
 *	1)  Handling wire edge -vs- shell intersection,
 *	2)  From nmg_isect_face3p_shell_int(), for "straddling" edges.
 *
 *  The edge will be fully intersected with the shell, potentially
 *  getting trimmed down in the process as crossings of s2 are found.
 *  The caller is responsible for re-calling with the extra edgeuses.
 *
 *  If both vertices of eu1 are on s2 (the other shell), and
 *  there is no edge in s2 between them, we need to determine
 *  whether this is an interior or exterior edge, and
 *  perhaps add a loop into s2 connecting those two verts.
 *
 *  We can't use the face cutter, because s2 has no
 *  appropriate face containing this edge.
 *
 *  If this edge is split, we have to
 *  trust nmg_ebreak() to insert new eu's ahead in the eu list,
 *  so caller will see them.
 *
 *  Lots of junk will be put on the vert_list's in 'is';  the caller
 *  should just free the lists without using them.
 */
static void
nmg_isect_edge3p_shell( is, eu1, s2 )
struct nmg_inter_struct		*is;
struct edgeuse		*eu1;
struct shell		*s2;
{
	struct shell	*s1;
	struct faceuse	*fu2;
	struct loopuse	*lu2;
	struct edgeuse	*eu2;
	struct vertexuse *vu2;
	point_t		midpt;

	NMG_CK_INTER_STRUCT(is);
	NMG_CK_EDGEUSE(eu1);
	NMG_CK_SHELL(s2);

	s1 = nmg_find_s_of_eu(eu1);

	if (rt_g.NMG_debug & DEBUG_POLYSECT) {
		rt_log("nmg_isect_edge3p_shell(, eu1=x%x, s2=x%x) START\n",
			eu1, s2 );
	}

	if( eu2 = nmg_find_matching_eu_in_s( eu1, s2 ) )  {
		/* XXX Is the fact that s2 has a corresponding edge good enough? */
		nmg_radial_join_eu( eu1, eu2, &is->tol );
		return;
	}

	/* Note the ray that contains this edge.  For debug in nmg_isect_edge3p_face3p() */
	VMOVE( is->pt, eu1->vu_p->v_p->vg_p->coord );
	VSUB2( is->dir, eu1->eumate_p->vu_p->v_p->vg_p->coord, is->pt );
	VUNITIZE( is->dir );

	/* Check eu1 of s1 against all faces in s2 */
	for( RT_LIST_FOR( fu2, faceuse, &s2->fu_hd ) )  {
		NMG_CK_FACEUSE(fu2);
		if( fu2->orientation != OT_SAME )  continue;
		is->fu2 = fu2;

		/* We aren't interested in the vert_list's, ignore return */
		(void)nmg_isect_edge3p_face3p( is, eu1, fu2 );
	}

	/* Check eu1 of s1 against all wire loops in s2 */
	is->fu2 = (struct faceuse *)NULL;
	for( RT_LIST_FOR( lu2, loopuse, &s2->lu_hd ) )  {
		NMG_CK_LOOPUSE(lu2);
		/* Really, it's just a bunch of wire edges, in a loop. */
		if( RT_LIST_FIRST_MAGIC( &lu2->down_hd ) == NMG_VERTEXUSE_MAGIC)  {
			/* XXX Can there be lone-vertex wire loops here? */
			vu2 = RT_LIST_FIRST( vertexuse, &lu2->down_hd );
			NMG_CK_VERTEXUSE(vu2);
			nmg_isect_vertex3_edge3p( is, vu2, eu1 );
			continue;
		}
		for( RT_LIST_FOR( eu2, edgeuse, &lu2->down_hd ) )  {
			NMG_CK_EDGEUSE(eu2);
			nmg_isect_edge3p_edge3p( is, eu1, eu2 );
		}
	}

	/* Check eu1 of s1 against all wire edges in s2 */
	for( RT_LIST_FOR( eu2, edgeuse, &s2->eu_hd ) )  {
		NMG_CK_EDGEUSE(eu2);
		nmg_isect_edge3p_edge3p( is, eu1, eu2 );
	}

	/* Check eu1 of s1 against vert of s2 */
	if( s2->vu_p )  {
		nmg_isect_vertex3_edge3p( is, s2->vu_p, eu1 );
	}

	/*
	 *  The edge has been fully intersected with the other shell.
	 *  It may have been trimmed in the process;  the caller is
	 *  responsible for re-calling us with the extra edgeuses.
	 *  If both vertices of eu1 are on s2 (the other shell), and
	 *  there is no edge in s2 between them, we need to determine
	 *  whether this is an interior or exterior edge, and
	 *  perhaps add a loop into s2 connecting those two verts.
	 */
	if( eu2 = nmg_find_matching_eu_in_s( eu1, s2 ) )  {
		/* We can't fuse wire edges */
		goto out;
	}
	/*  Can't use the face cutter, because s2 has no associated face!
	 *  Call the geometric classifier on the midpoint.
	 *  If it's INSIDE or ON the other shell, add a wire loop
	 *  that connects the two vertices.
	 */
	VADD2SCALE( midpt, eu1->vu_p->v_p->vg_p->coord,
		eu1->eumate_p->vu_p->v_p->vg_p->coord,  0.5 );
	if( nmg_class_pt_s( midpt, s2, &is->tol ) == NMG_CLASS_AoutB )
		goto out;		/* Nothing more to do */

	/* Add a wire loop in s2 connecting the two vertices */
	lu2 = nmg_mlv( &s2->l.magic, eu1->vu_p->v_p, OT_UNSPEC );
	NMG_CK_LOOPUSE(lu2);
	{
		struct edgeuse	*neu1, *neu2;

		neu1 = nmg_meonvu( RT_LIST_FIRST( vertexuse, &lu2->down_hd ) );
		neu2 = nmg_eusplit( eu1->eumate_p->vu_p->v_p, neu1 );
		NMG_CK_EDGEUSE(eu1);
		/* Attach new edge in s2 to original edge in s1 */
		nmg_moveeu( eu1, neu2 );
		nmg_moveeu( eu1, neu1 );
	}
	nmg_loop_g(lu2->l_p, &is->tol);
	if (rt_g.NMG_debug & DEBUG_POLYSECT) {
		rt_log("nmg_isect_edge3p_shell(, eu1=x%x, s2=x%x) Added wire lu=x%x\n",
			eu1, s2, lu2 );
	}

out:
	if (rt_g.NMG_debug & DEBUG_POLYSECT) {
		rt_log("nmg_isect_edge3p_shell(, eu1=x%x, s2=x%x) END\n",
			eu1, s2 );
	}
	return;
}

/*
 *			N M G _ I S E C T _ F A C E 3 P _ S H E L L _ I N T
 *
 *  Intersect all the edges in fu1 that don't lie on any of the faces
 *  of shell s2 with s2, i.e. "interior" edges, where the
 *  endpoints lie on s2, but the edge is not shared with a face of s2.
 *  Such edges wouldn't have been processed by
 *  the NEWLINE version of nmg_isect_two_generic_faces(), so
 *  intersections need to be looked for here.
 *  Fortunately, it's easy to reject everything except edges that need
 *  processing using only the topology structures.
 *
 *  The "_int" at the end of the name is to signify that this routine
 *  does only "interior" edges, and is not a general face/shell intersector.
 */
void
nmg_isect_face3p_shell_int( is, fu1, s2 )
struct nmg_inter_struct	*is;
struct faceuse	*fu1;
struct shell	*s2;
{
	struct shell	*s1;
	struct loopuse	*lu1;
	struct edgeuse	*eu1;

	NMG_CK_INTER_STRUCT(is);
	NMG_CK_FACEUSE(fu1);
	NMG_CK_SHELL(s2);
	s1 = fu1->s_p;
	NMG_CK_SHELL(s1);

	if (rt_g.NMG_debug & DEBUG_POLYSECT)
		rt_log("nmg_isect_face3p_shell_int(, fu1=x%x, s2=x%x) START\n", fu1, s2 );

	for( RT_LIST_FOR( lu1, loopuse, &fu1->lu_hd ) )  {
		NMG_CK_LOOPUSE(lu1);
		if( RT_LIST_FIRST_MAGIC( &lu1->down_hd ) == NMG_VERTEXUSE_MAGIC)
			continue;
		for( RT_LIST_FOR( eu1, edgeuse, &lu1->down_hd ) )  {
			struct edgeuse		*eu2;

			eu2 = nmg_find_matching_eu_in_s( eu1, s2 );
			if( eu2	)  {
rt_log("nmg_isect_face3p_shell_int() eu1=x%x, e1=x%x, eu2=x%x, e2=x%x (nothing to do)\n", eu1, eu1->e_p, eu2, eu2->e_p);
				/*  Whether the edgeuse is in a face, or a
				 *  wire edgeuse, the other guys will isect it.
				 */
				continue;
			}
			/*  vu2a and vu2b are in shell s2, but there is no
			 *  edge running between them in shell s2.
			 *  Create a line of intersection, and go to it!.
			 */
rt_log("nmg_isect_face3p_shell_int(, s2=x%x) eu1=x%x, no eu2\n", s2, eu1);
			nmg_isect_edge3p_shell( is, eu1, s2 );
		}
	}

	if (rt_g.NMG_debug & DEBUG_POLYSECT)
		rt_log("nmg_isect_face3p_shell_int(, fu1=x%x, s2=x%x) END\n", fu1, s2 );
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
	struct nmg_ptbl		vert_list1, vert_list2;
	struct nmg_inter_struct is;
	struct shell_a	*sa1, *sa2;
	struct face	*f1;
	struct faceuse	*fu1, *fu2;
	struct loopuse	*lu1;
	struct loopuse	*lu2;
	struct edgeuse	*eu1;
	struct edgeuse	*eu2;
	long		*flags;

	if (rt_g.NMG_debug & DEBUG_POLYSECT)
		rt_log("nmg_crackshells(s1=x%x, s2=x%x)\n", s1, s2);

	RT_CK_TOL(tol);
	NMG_CK_SHELL(s1);
	sa1 = s1->sa_p;
	NMG_CK_SHELL_A(sa1);

	NMG_CK_SHELL(s2);
	sa2 = s2->sa_p;
	NMG_CK_SHELL_A(sa2);

nmg_ck_vs_in_region( s1->r_p, tol );
nmg_ck_vs_in_region( s2->r_p, tol );

	/* All the non-face/face isect subroutines need are tol, l1, and l2 */
	is.magic = NMG_INTER_STRUCT_MAGIC;
	is.vert2d = (fastf_t *)NULL;
	is.tol = *tol;		/* struct copy */
	is.l1 = &vert_list1;
	is.l2 = &vert_list2;
	is.s1 = s1;
	is.s2 = s2;
	is.fu1 = (struct faceuse *)NULL;
	is.fu2 = (struct faceuse *)NULL;
	(void)nmg_tbl(&vert_list1, TBL_INIT, (long *)NULL);
	(void)nmg_tbl(&vert_list2, TBL_INIT, (long *)NULL);

	if( rt_g.NMG_debug & DEBUG_VERIFY )  {
		nmg_vshell( &s1->r_p->s_hd, s1->r_p );
		nmg_vshell( &s2->r_p->s_hd, s2->r_p );
	}

	/* XXX this isn't true for non-3-manifold geometry! */
	if( RT_LIST_IS_EMPTY( &s1->fu_hd ) ||
	    RT_LIST_IS_EMPTY( &s2->fu_hd ) )  {
		rt_log("ERROR:shells must contain faces for boolean operations.");
		return;
	}

	/* See if shells overlap */
	if ( ! V3RPP_OVERLAP_TOL(sa1->min_pt, sa1->max_pt,
	    sa2->min_pt, sa2->max_pt, tol) )
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

		if( fu1->orientation != OT_SAME )  continue;
		if( NMG_INDEX_IS_SET(flags, f1) )  continue;
		NMG_CK_FACE_G(f1->fg_p);

		/* See if face f1 overlaps shell2 */
		if( ! V3RPP_OVERLAP_TOL(sa2->min_pt, sa2->max_pt,
		    f1->min_pt, f1->max_pt, tol) )
			continue;

		is.fu1 = fu1;

		/*
		 *  Now, check the face f1 from shell 1
		 *  against each of the faces of shell 2
		 */
	    	for( RT_LIST_FOR( fu2, faceuse, &s2->fu_hd ) )  {
	    		NMG_CK_FACEUSE(fu2);
	    		NMG_CK_FACE(fu2->f_p);
			if( fu2->orientation != OT_SAME )  continue;

	    		is.fu2 = fu2;
			nmg_isect_two_generic_faces(fu1, fu2, tol);
	    	}

		/*
		 *  Because the rest of the shell elements are wires,
		 *  there is no need to invoke the face cutter;
		 *  calculating the intersection points (vertices)
		 *  is sufficient.
		 *  XXX Is this true?  What about a wire edge cutting
		 *  XXX clean across fu1?  fu1 ought to be cut!
		 */
		is.fu2 = (struct faceuse *)NULL;

		/* Check f1 from s1 against wire loops of s2 */
		for( RT_LIST_FOR( lu2, loopuse, &s2->lu_hd ) )  {
			NMG_CK_LOOPUSE(lu2);
			/* Not interested in vert_list here */
			(void)nmg_isect_loop3p_face3p( &is, lu2, fu1 );
		}

		/* Check f1 from s1 against wire edges of s2 */
		for( RT_LIST_FOR( eu2, edgeuse, &s2->eu_hd ) )  {
			NMG_CK_EDGEUSE(eu2);

			nmg_isect_edge3p_face3p( &is, eu2, fu1 );
		}

		/* Check f1 from s1 against lone vert of s2 */
		if( s2->vu_p )  {
			nmg_isect_3vertex_3face( &is, s2->vu_p, fu1 );
		}

	    	NMG_INDEX_SET(flags, f1);

		if( rt_g.NMG_debug & DEBUG_VERIFY )  {
			nmg_vshell( &s1->r_p->s_hd, s1->r_p );
			nmg_vshell( &s2->r_p->s_hd, s2->r_p );
		}
	}

	/*  Check each wire loop of shell 1 against non-faces of shell 2. */
	is.fu1 = (struct faceuse *)NULL;
	is.fu2 = (struct faceuse *)NULL;
	for( RT_LIST_FOR( lu1, loopuse, &s1->lu_hd ) )  {
		NMG_CK_LOOPUSE( lu1 );
		/* Really, it's just a bunch of wire edges, in a loop. */
		/* XXX Can there be lone-vertex loops here? */
		for( RT_LIST_FOR( eu1, edgeuse, &lu1->down_hd ) )  {
			NMG_CK_EDGEUSE(eu1);
			/* Check eu1 against all of shell 2 */
			nmg_isect_edge3p_shell( &is, eu1, s2 );
		}
	}

	/*  Check each wire edge of shell 1 against all of shell 2. */
	for( RT_LIST_FOR( eu1, edgeuse, &s1->eu_hd ) )  {
		NMG_CK_EDGEUSE( eu1 );
		nmg_isect_edge3p_shell( &is, eu1, s2 );
	}

	/* Check each lone vert of s1 against shell 2 */
	if( s1->vu_p )  {
		/* Check vert of s1 against all faceuses in s2 */
	    	for( RT_LIST_FOR( fu2, faceuse, &s2->fu_hd ) )  {
	    		NMG_CK_FACEUSE(fu2);
			if( fu2->orientation != OT_SAME )  continue;
	    		nmg_isect_3vertex_3face( &is, s1->vu_p, fu2 );
	    	}
		/* Check vert of s1 against all wire loops of s2 */
		for( RT_LIST_FOR( lu2, loopuse, &s2->lu_hd ) )  {
			NMG_CK_LOOPUSE(lu2);
			/* Really, it's just a bunch of wire edges, in a loop. */
			/* XXX Can there be lone-vertex loops here? */
			for( RT_LIST_FOR( eu2, edgeuse, &lu2->down_hd ) )  {
				NMG_CK_EDGEUSE(eu2);
				nmg_isect_vertex3_edge3p( &is, s1->vu_p, eu2 );
			}
		}
		/* Check vert of s1 against all wire edges of s2 */
		for( RT_LIST_FOR( eu2, edgeuse, &s2->eu_hd ) )  {
			NMG_CK_EDGEUSE(eu2);
			nmg_isect_vertex3_edge3p( &is, s1->vu_p, eu2 );
		}

		/* Check vert of s1 against vert of s2 */
		/* Unnecessary: already done by vertex fuser */
	}

	/* Release storage from bogus isect line */
	(void)nmg_tbl(&vert_list1, TBL_FREE, (long *)NULL);
	(void)nmg_tbl(&vert_list2, TBL_FREE, (long *)NULL);

	rt_free( (char *)flags, "nmg_crackshells flags[]" );

	/* Eliminate stray vertices that were added along edges in this step */
	(void)nmg_unbreak_region_edges( &s1->l.magic );
	(void)nmg_unbreak_region_edges( &s2->l.magic );

	/* clean things up now that the intersections have been built */
	nmg_sanitize_s_lv(s1, OT_BOOLPLACE);
	nmg_sanitize_s_lv(s2, OT_BOOLPLACE);

	nmg_isect2d_cleanup(&is);

	if( rt_g.NMG_debug & DEBUG_VERIFY )  {
		nmg_vshell( &s1->r_p->s_hd, s1->r_p );
		nmg_vshell( &s2->r_p->s_hd, s2->r_p );
nmg_ck_vs_in_region( s1->r_p, tol );
nmg_ck_vs_in_region( s2->r_p, tol );
	}
}

/*
 *			N M G _ F U _ T O U C H I N G L O O P S
 */
int
nmg_fu_touchingloops(fu)
CONST struct faceuse	*fu;
{
	CONST struct loopuse	*lu;
	CONST struct vertexuse	*vu;

	NMG_CK_FACEUSE(fu);
	for( RT_LIST_FOR( lu, loopuse, &fu->lu_hd ) )  {
		NMG_CK_LOOPUSE(lu);
		if( vu = nmg_loop_touches_self( lu ) )  {
#if 0
			/* Right now, this routine is used for debugging ONLY,
			 * so if this condition exists, die.
			 * However, note that this condition happens a lot
			 * for valid reasons, too.
			 */
			rt_log("nmg_fu_touchingloops(lu=x%x, vu=x%x, v=x%x)\n",
				lu, vu, vu->v_p );
			nmg_pr_lu_briefly(lu,0);
			rt_bomb("nmg_fu_touchingloops()\n");
#else
			/* Perhaps log something here? */
#endif
			return 1;
		}
	}
	return 0;
}
