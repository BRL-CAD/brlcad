/*
 *			N M G _ F U S E . C
 *
 *  Routines to "fuse" entities together that are geometrically identical
 *  (within tolerance) into entities that share underlying geometry
 *  structures, so that the relationship is explicit.
 *
 *  Author -
 *	Michael John Muuss
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
 *	This software is Copyright (C) 1993 by the United States Army
 *	in all countries except the USA.  All rights reserved.
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (ARL)";
#endif

#include "conf.h"
#include <stdio.h>
#include <math.h>
#include "machine.h"
#include "externs.h"
#include "vmath.h"
#include "nmg.h"
#include "raytrace.h"
#include "./debug.h"

/* XXX Move to raytrace.h */
RT_EXTERN(int	 	nmg_is_vertex_in_face, (CONST struct vertex *v,
			CONST struct face *f));
/* XXX Move to nmg_info.c */
/*
 *			N M G _ I S _ V E R T E X _ I N _ F A C E
 *
 *  Returns -
 *	!0	If there is at least one use of vertex 'v' in face 'f'.
 *	0	If there are no uses of 'v' in 'f'.
 */
int
nmg_is_vertex_in_face( v, f )
CONST struct vertex	*v;
CONST struct face	*f;
{
	CONST struct vertexuse	*vu;
	CONST struct loopuse	*lu;
	CONST struct faceuse	*fu;

	NMG_CK_VERTEX(v);
	NMG_CK_FACE(f);

	for( RT_LIST_FOR( vu, vertexuse, &v->vu_hd ) )  {
		if( *vu->up.magic_p != NMG_EDGEUSE_MAGIC )  continue;
		if( *vu->up.eu_p->up.magic_p != NMG_LOOPUSE_MAGIC )  continue;
		lu = vu->up.eu_p->up.lu_p;
		if( *lu->up.magic_p != NMG_FACEUSE_MAGIC )  continue;
		fu = lu->up.fu_p;
		NMG_CK_FACEUSE(fu);
		if( fu->f_p != f )  continue;
		return 1;
	}
	return 0;
}

/*
 *			N M G _ I S _ C O M M O N _ B I G L O O P
 *
 *  Do two faces share by topology at least one loop of 3 or more vertices?
 *
 *  Require that at least three distinct edge geometries be involved.
 *
 * XXX Won't catch sharing of faces with only self-loops and no edge loops.
 */
int
nmg_is_common_bigloop( f1, f2 )
CONST struct face	*f1;
CONST struct face	*f2;
{
	CONST struct faceuse	*fu1;
	CONST struct loopuse	*lu1;
	CONST struct edgeuse	*eu1;
	CONST long		*magic1 = (long *)NULL;
	CONST long		*magic2 = (long *)NULL;
	int	nverts;
	int	nbadv;
	int	got_three;

	NMG_CK_FACE(f1);
	NMG_CK_FACE(f2);

	fu1 = f1->fu_p;
	NMG_CK_FACEUSE(fu1);

	/* For all loopuses in fu1 */
	for( RT_LIST_FOR( lu1, loopuse, &fu1->lu_hd ) )  {
		if( RT_LIST_FIRST_MAGIC(&lu1->down_hd) == NMG_VERTEXUSE_MAGIC )
			continue;
		nverts = 0;
		nbadv = 0;
		magic1 = (long *)NULL;
		magic2 = (long *)NULL;
		got_three = 0;
		for( RT_LIST_FOR( eu1, edgeuse, &lu1->down_hd ) )  {
			nverts++;
			NMG_CK_EDGE_G_LSEG(eu1->g.lseg_p);
			if( !magic1 )  {
				magic1 = eu1->g.magic_p;
			} else if( !magic2 )  {
				if( eu1->g.magic_p != magic1 )  {
					magic2 = eu1->g.magic_p;
				}
			} else {
				if( eu1->g.magic_p != magic1 &&
				    eu1->g.magic_p != magic2 )  {
					got_three = 1;
				}
			}
			if( nmg_is_vertex_in_face( eu1->vu_p->v_p, f2 ) )
				continue;
			nbadv++;
			break;
		}
		if( nbadv <= 0 && nverts >= 3 && got_three )  return 1;
	}
	return 0;
}

/*
 *			N M G _ R E G I O N _ V _ U N I Q U E
 *
 *  Ensure that all the vertices in r1 are still geometricaly unique.
 *  This will be true after nmg_region_both_vfuse() has been called,
 *  and should remain true throughout the intersection process.
 */
void
nmg_region_v_unique( r1, tol )
struct nmgregion	*r1;
CONST struct rt_tol	*tol;
{
	int	i;
	int	j;
	struct nmg_ptbl	t;

	NMG_CK_REGION(r1);
	RT_CK_TOL(tol);

	nmg_vertex_tabulate( &t, &r1->l.magic );

	for( i = NMG_TBL_END(&t)-1; i >= 0; i-- )  {
		register struct vertex	*vi;
		vi = (struct vertex *)NMG_TBL_GET(&t, i);
		NMG_CK_VERTEX(vi);
		if( !vi->vg_p )  continue;

		for( j = i-1; j >= 0; j-- )  {
			register struct vertex	*vj;
			vj = (struct vertex *)NMG_TBL_GET(&t, j);
			NMG_CK_VERTEX(vj);
			if( !vj->vg_p )  continue;
			if( !rt_pt3_pt3_equal( vi->vg_p->coord, vj->vg_p->coord, tol ) )
				continue;
			/* They are the same */
			rt_log("nmg_region_v_unique():  2 verts are the same, within tolerance\n");
			nmg_pr_v( vi, 0 );
			nmg_pr_v( vj, 0 );
			rt_bomb("nmg_region_v_unique()\n");
		}
	}
	nmg_tbl( &t, TBL_FREE, 0 );
}

/*
 *			N M G _ P T B L _ V F U S E
 *
 *  Working from the end to the front, scan for geometric duplications
 *  within a single list of vertex structures.
 *
 *  Exists primarily as a support routine for nmg_model_vertex_fuse().
 */
int
nmg_ptbl_vfuse( t, tol )
struct nmg_ptbl		*t;
CONST struct rt_tol	*tol;
{
	int	count = 0;
	int	i;
	int	j;

	for( i = NMG_TBL_END(t)-1; i >= 0; i-- )  {
		register struct vertex	*vi;
		vi = (struct vertex *)NMG_TBL_GET(t, i);
		NMG_CK_VERTEX(vi);
		if( !vi->vg_p )  continue;

		for( j = i-1; j >= 0; j-- )  {
			register struct vertex	*vj;
			vj = (struct vertex *)NMG_TBL_GET(t, j);
			NMG_CK_VERTEX(vj);
			if( !vj->vg_p )  continue;
			if( !rt_pt3_pt3_equal( vi->vg_p->coord, vj->vg_p->coord, tol ) )
				continue;
			/* They are the same, fuse vi into vj */
			nmg_jv( vj, vi );
			nmg_tbl( t, TBL_RM, (long *)vi );
			count++;
			break;
		}
	}
	return count;
}

/*
 *			N M G _ R E G I O N _ B O T H _ V F U S E
 *
 *  For every element in t1, scan t2 for geometric duplications.
 *
 *  Deleted elements in t2 are marked by a null vertex pointer,
 *  rather than bothering to do a TBL_RM, which will re-copy the
 *  list to compress it.
 *
 *  Exists as a support routine for nmg_two_region_vertex_fuse()
 */
int
nmg_region_both_vfuse( t1, t2, tol )
struct nmg_ptbl		*t1;
struct nmg_ptbl		*t2;
CONST struct rt_tol	*tol;
{
	int	count = 0;
	int	i;
	int	j;

	/* Verify t2 is good to start with */
	for( j = NMG_TBL_END(t2)-1; j >= 0; j-- )  {
		register struct vertex	*vj;
		vj = (struct vertex *)NMG_TBL_GET(t2, j);
		NMG_CK_VERTEX(vj);
	}

	for( i = NMG_TBL_END(t1)-1; i >= 0; i-- )  {
		register struct vertex	*vi;
		vi = (struct vertex *)NMG_TBL_GET(t1, i);
		NMG_CK_VERTEX(vi);
		if( !vi->vg_p )  continue;

		for( j = NMG_TBL_END(t2)-1; j >= 0; j-- )  {
			register struct vertex	*vj;
			vj = (struct vertex *)NMG_TBL_GET(t2, j);
			if(!vj) continue;
			NMG_CK_VERTEX(vj);
			if( !vj->vg_p )  continue;
			if( !rt_pt3_pt3_equal( vi->vg_p->coord, vj->vg_p->coord, tol ) )
				continue;
			/* They are the same, fuse vj into vi */
			nmg_jv( vi, vj );
			NMG_TBL_GET(t2, j) = 0;
			count++;
		}
	}
	return count;
}

#if 0
/*
 *			N M G _ T W O _ R E G I O N _ V E R T E X _ F U S E
 *
 *  This routine should not be used.  Instead, call nmg_model_vertex_fuse().
 */
int
nmg_two_region_vertex_fuse( r1, r2, tol )
struct nmgregion	*r1;
struct nmgregion	*r2;
CONST struct rt_tol	*tol;
{
	struct nmg_ptbl	t1;
	struct nmg_ptbl	t2;
	int		total = 0;

	NMG_CK_REGION(r1);
	NMG_CK_REGION(r2);
	RT_CK_TOL(tol);

	if( r1->m_p != r2->m_p )  rt_bomb("nmg_two_region_vertex_fuse:  regions not in same model\n");

	nmg_vertex_tabulate( &t1, &r1->l.magic );
	nmg_vertex_tabulate( &t2, &r2->l.magic );

	total = nmg_ptbl_vfuse( &t1, tol );
	total += nmg_region_both_vfuse( &t1, &t2, tol );

	nmg_tbl( &t1, TBL_FREE, 0 );
	nmg_tbl( &t2, TBL_FREE, 0 );

	return total;
}
#endif

/*
 *			N M G _ M O D E L _ V E R T E X _ F U S E
 *
 *  Fuse together any vertices in the nmgmodel that are geometricly
 *  identical, within the tolerance.
 */
int
nmg_model_vertex_fuse( m, tol )
struct model		*m;
CONST struct rt_tol	*tol;
{
	struct nmg_ptbl	t1;
	int		total = 0;

	NMG_CK_MODEL(m);
	RT_CK_TOL(tol);

	nmg_vertex_tabulate( &t1, &m->magic );

	total = nmg_ptbl_vfuse( &t1, tol );

	nmg_tbl( &t1, TBL_FREE, 0 );

	if( rt_g.NMG_debug & DEBUG_BASIC && total > 0 )
		rt_log("nmg_model_vertex_fuse() %d\n", total);
	return total;
}

/*
 *			N M G _ M O D E L _ E D G E _ F U S E
 */
int
nmg_model_edge_fuse( m, tol )
struct model		*m;
CONST struct rt_tol	*tol;
{
	struct nmg_ptbl	eutab;
	int		total = 0;
	register int	i,j;

	NMG_CK_MODEL(m);
	RT_CK_TOL(tol);

	/* Make a list of all the edgeuse structs in the model */
again:
	total = 0;
	nmg_edgeuse_tabulate( &eutab, &m->magic );

	for( i = NMG_TBL_END(&eutab)-1; i >= 0; i-- )  {
		register struct edgeuse	*eu1;
		register struct edge	*e1;
		struct vertex		*v1a, *v1b;

		eu1 = (struct edgeuse *)NMG_TBL_GET(&eutab, i);
		NMG_CK_EDGEUSE(eu1);
		e1 = eu1->e_p;
		NMG_CK_EDGE(e1);

		v1a = eu1->vu_p->v_p;
		v1b = eu1->eumate_p->vu_p->v_p;
		NMG_CK_VERTEX(v1a);
		NMG_CK_VERTEX(v1b);

		if( v1a == v1b )  {
			/* 0-length edge.  Eliminate. */
			/* These should't happen, but need fixing. */
			if( nmg_keu( eu1 ) )  {
				rt_log("nmg_model_edge_fuse() WARNING:  deletion of 0-length edgeuse=x%x caused parent to go empty.\n", eu1);
			}
			rt_log("nmg_model_edge_fuse() 0-length edgeuse=x%x deleted\n", eu1);
			/* XXX no way to know where edgeuse mate will be */
			nmg_tbl( &eutab, TBL_FREE, 0 );
			goto again;
		}

		for( j = i-1; j >= 0; j-- )  {
			register struct edgeuse	*eu2;
			register struct edge	*e2;

			eu2 = (struct edgeuse *)NMG_TBL_GET(&eutab,j);
			NMG_CK_EDGEUSE(eu2);
			e2 = eu2->e_p;
			NMG_CK_EDGE(e2);

			if( e1 == e2 )  continue;	/* Already shared */
			if( (eu2->vu_p->v_p == v1a &&
			     eu2->eumate_p->vu_p->v_p == v1b) ||
			    (eu2->eumate_p->vu_p->v_p == v1a &&
			     eu2->vu_p->v_p == v1b) )  {
				nmg_radial_join_eu(eu1, eu2, tol);
			     	total++;
			 }
		}
	}
	if( rt_g.NMG_debug & DEBUG_BASIC && total > 0 )
		rt_log("nmg_model_edge_fuse(): %d edges fused\n", total);
	nmg_tbl( &eutab, TBL_FREE, 0 );
	return total;
}

/* XXX move to nmg_info.c */
/*
 *			N M G _ 2 E D G E U S E _ G _ C O I N C I D E N T
 *
 *  Given two edgeuses, determine if they share the same edge geometry,
 *  either topologically, or within tolerance.
 *
 *  Returns -
 *	0	two edge geometries are not coincident
 *	1	edges geometries are everywhere coincident.
 *		(For linear edge_g_lseg, the 2 are the same line, within tol.)
 */
int
nmg_2edgeuse_g_coincident( eu1, eu2, tol )
CONST struct edgeuse	*eu1;
CONST struct edgeuse	*eu2;
CONST struct rt_tol	*tol;
{
	struct edge_g_lseg	*eg1;
	struct edge_g_lseg	*eg2;

	NMG_CK_EDGEUSE(eu1);
	NMG_CK_EDGEUSE(eu2);
	RT_CK_TOL(tol);

	eg1 = eu1->g.lseg_p;
	eg2 = eu2->g.lseg_p;
	NMG_CK_EDGE_G_LSEG(eg1);
	NMG_CK_EDGE_G_LSEG(eg2);

	if( eg1 == eg2 )  return 1;

	/* Ensure direction vectors are generally parallel */
	/* These are not unit vectors */
	/* tol->para and RT_DOT_TOL are too tight a tolerance.  0.1 is 5 degrees */
	if( fabs(VDOT(eg1->e_dir, eg2->e_dir)) <
	    0.9 * MAGNITUDE(eg1->e_dir) * MAGNITUDE(eg2->e_dir)  )  return 0;

	/* Ensure that vertices on edge 2 are within tol of e1 */
	if( rt_distsq_line3_pt3( eg1->e_pt, eg1->e_dir,
	    eu2->vu_p->v_p->vg_p->coord ) > tol->dist_sq )  goto trouble;
	if( rt_distsq_line3_pt3( eg1->e_pt, eg1->e_dir,
	    eu2->eumate_p->vu_p->v_p->vg_p->coord ) > tol->dist_sq )  goto trouble;

	/* Ensure that vertices of both edges are within tol of other eg */
	if( rt_distsq_line3_pt3( eg2->e_pt, eg2->e_dir,
	    eu1->vu_p->v_p->vg_p->coord ) > tol->dist_sq )  goto trouble;
	if( rt_distsq_line3_pt3( eg2->e_pt, eg2->e_dir,
	    eu1->eumate_p->vu_p->v_p->vg_p->coord ) > tol->dist_sq )  goto trouble;

	/* Perhaps check for ultra-short edges (< 10*tol->dist)? */

	/* Do not use rt_isect_line3_line3() -- it's MUCH too strict */

	return 1;
trouble:
	if( !rt_2line3_colinear( eg1->e_pt, eg1->e_dir, eg2->e_pt, eg2->e_dir,
	     1e5, tol ) )
		return 0;

	/* XXX debug */
	nmg_pr_eg( &eg1->magic, 0 );
	nmg_pr_eg( &eg2->magic, 0 );
	rt_log("nmg_2edgeuse_g_coincident() lines colinear, vertex check fails, calling colinear anyway.\n");
	return 1;
}

/*
 *			N M G _ M O D E L _ E D G E _ G _ F U S E
 *
 *  The present algorithm is a consequence of the old edge geom ptr structure.
 *  XXX This might be better formulated by generating a list of all
 *  edge_g structs in the model, and comparing *them* pairwise.
 */
int
nmg_model_edge_g_fuse( m, tol )
struct model		*m;
CONST struct rt_tol	*tol;
{
	struct nmg_ptbl	etab;
	int		total = 0;
	register int	i,j;

	NMG_CK_MODEL(m);
	RT_CK_TOL(tol);

	/* Make a list of all the edge geometry structs in the model */
	nmg_edge_g_tabulate( &etab, &m->magic );

	for( i = NMG_TBL_END(&etab)-1; i >= 0; i-- )  {
		struct edge_g_lseg		*eg1;
		struct edgeuse			*eu1;

		eg1 = (struct edge_g_lseg *)NMG_TBL_GET(&etab, i);
		NMG_CK_EDGE_G_EITHER(eg1);

		/* XXX Need routine to compare two cnurbs geometricly */
		if( eg1->magic == NMG_EDGE_G_CNURB_MAGIC )  {
			continue;
		}

		NMG_CK_EDGE_G_LSEG(eg1);
		eu1 = RT_LIST_MAIN_PTR( edgeuse, RT_LIST_FIRST( rt_list, &eg1->eu_hd2 ), l2 );
		NMG_CK_EDGEUSE(eu1);

		for( j = i-1; j >= 0; j-- )  {
			struct edge_g_lseg	*eg2;
			struct edgeuse		*eu2;

			eg2 = (struct edge_g_lseg *)NMG_TBL_GET(&etab,j);
			NMG_CK_EDGE_G_EITHER(eg2);
			if( eg2->magic == NMG_EDGE_G_CNURB_MAGIC )  continue;
			NMG_CK_EDGE_G_LSEG(eg2);
			eu2 = RT_LIST_MAIN_PTR( edgeuse, RT_LIST_FIRST( rt_list, &eg2->eu_hd2 ), l2 );
			NMG_CK_EDGEUSE(eu2);

			if( eg1 == eg2 )  rt_bomb("nmg_model_edge_g_fuse() edge_g listed twice in ptbl?\n");

			if( !nmg_2edgeuse_g_coincident( eu1, eu2, tol ) )  continue;

			/* Comitted to fusing two edge_g_lseg's.
			 * Make all instances of eg1 become eg2.
			 * XXX really should check ALL edges using eg1
			 * XXX against ALL edges using eg2 for coincidence.
			 */
		     	total++;
			nmg_jeg( eg2, eg1 );
			NMG_TBL_GET(&etab,i) = (long *)NULL;
			break;
		}
	}
	nmg_tbl( &etab, TBL_FREE, (long *)0 );
	if( rt_g.NMG_debug & DEBUG_BASIC && total > 0 )
		rt_log("nmg_model_edge_g_fuse(): %d edge_g_lseg's fused\n", total);
	return total;
}

/*
 *			N M G _ C K _ F U _ V E R T S
 *
 *  Check that all the vertices in fu1 are within tol->dist of fu2's surface.
 *  fu1 and fu2 may be the same face, or different.
 *
 *  This is intended to be a geometric check only, not a topology check.
 *  Topology may have become inappropriately shared.
 *
 *  Returns -
 *	0	All is well, or all verts are within 10*tol->dist of fu2
 *	count	Number of verts *not* on fu2's surface when at least one is
 *		more than 10*tol->dist from fu2.
 *
 *  XXX It would be more efficient to use nmg_vist() for this.
 */
int
nmg_ck_fu_verts( fu1, f2, tol )
struct faceuse	*fu1;
struct face	*f2;
CONST struct rt_tol	*tol;
{
	CONST struct face_g_plane	*fg2;
	struct nmg_ptbl		vtab;
	FAST fastf_t		dist;
	fastf_t			worst = 0;
	int			k;
	int			count = 0;

	NMG_CK_FACEUSE( fu1 );
	NMG_CK_FACE( f2 );
	RT_CK_TOL(tol);

	fg2 = f2->g.plane_p;
	NMG_CK_FACE_G_PLANE(fg2);

	nmg_vertex_tabulate( &vtab, &fu1->l.magic );

	for( k = NMG_TBL_END(&vtab)-1; k >= 0; k-- )  {
		register struct vertex		*v;
		register struct vertex_g	*vg;

		v = (struct vertex *)NMG_TBL_GET(&vtab, k);
		NMG_CK_VERTEX(v);
		vg = v->vg_p;
		if( !vg )  rt_bomb("nmg_ck_fu_verts(): vertex with no geometry?\n");
		NMG_CK_VERTEX_G(vg);

		/* Geometry check */
		dist = DIST_PT_PLANE(vg->coord, fg2->N);
		if( dist > tol->dist || dist < (-tol->dist) )  {
			if (rt_g.NMG_debug & DEBUG_MESH)  {
				rt_log("nmg_ck_fu_verts(x%x, x%x) v x%x off face by %e\n",
					fu1, f2,
					v, dist );
				VPRINT(" pt", vg->coord);
				PLPRINT(" fg2", fg2->N);
			}
			count++;
			if( dist < 0.0 )
				dist = (-dist);
			if( dist > worst )  worst = dist;
		}
	}
	nmg_tbl( &vtab, TBL_FREE, 0 );

	if ( count != 0 || rt_g.NMG_debug & DEBUG_BASIC)  {
		rt_log("nmg_ck_fu_verts(fu1=x%x, f2=x%x, tol=%g) f1=x%x, ret=%d, worst=%gmm (%e)\n",
			fu1, f2, tol->dist, fu1->f_p,
			count, worst, worst );
	}

	if( worst > 10.0*tol->dist )
		return count;
	else
		return( 0 );
}

/*			N M G _ C K _ F G _ V E R T S
 *
 * Similar to nmg_ck_fu_verts, but checks all vertices that use the same
 * face geometry as fu1
 *  fu1 and f2 may be the same face, or different.
 *
 *  This is intended to be a geometric check only, not a topology check.
 *  Topology may have become inappropriately shared.
 *
 *  Returns -
 *	0	All is well.
 *	count	Number of verts *not* on fu2's surface.
 *
 */
int
nmg_ck_fg_verts( fu1 , f2 , tol )
struct faceuse *fu1;
struct face *f2;
CONST struct rt_tol *tol;
{
	struct face_g_plane *fg1;
	struct faceuse *fu;
	struct face *f;
	int count=0;

	NMG_CK_FACEUSE( fu1 );
	NMG_CK_FACE( f2 );
	RT_CK_TOL( tol );

	NMG_CK_FACE( fu1->f_p );
	fg1 = fu1->f_p->g.plane_p;
	NMG_CK_FACE_G_PLANE( fg1 );

	for( RT_LIST_FOR( f , face , &fg1->f_hd ) )
	{
		NMG_CK_FACE( f );

		fu = f->fu_p;
		NMG_CK_FACEUSE( fu );

		count += nmg_ck_fu_verts( fu , f2 , tol );
	}

	return( count );
}

/*
 *			N M G _ T W O _ F A C E _ F U S E
 *
 *  XXX A better algorithm would be to compare loop by loop.
 *  If the two faces share all the verts of at least one loop of 3 or more
 *  vertices, then they should be shared.  Otherwise it will be awkward
 *  having shared loop(s) on non-shared faces!!
 *
 *  Compare the geometry of two faces, and fuse them if they are the
 *  same within tolerance.
 *  First compare the plane equations.  If they are "similar" (within tol),
 *  then check all verts in f2 to make sure that they are within tol->dist
 *  of f1's geometry.  If they are, then fuse the face geometry.
 *
 *  Returns -
 *	0	Faces were not fused.
 *	>0	Faces were successfully fused.
 */
int
nmg_two_face_fuse( f1, f2, tol )
struct face	*f1;
struct face	*f2;
CONST struct rt_tol	*tol;
{
	register struct face_g_plane	*fg1;
	register struct face_g_plane	*fg2;
	int			flip2 = 0;
	int			code;

	NMG_CK_FACE(f1);
	NMG_CK_FACE(f2);
	RT_CK_TOL(tol);

	fg1 = f1->g.plane_p;
	fg2 = f2->g.plane_p;

	if( !fg1 || !fg2 )  {
		if (rt_g.NMG_debug & DEBUG_MESH)  {
			rt_log("nmg_two_face_fuse(x%x, x%x) null fg fg1=x%x, fg2=x%x\n",
				f1, f2, fg1, fg2);
		}
		return 0;
	}

	NMG_CK_FACE_G_PLANE(fg1);
	NMG_CK_FACE_G_PLANE(fg2);

	if( fg1 == fg2 )  {
		if (rt_g.NMG_debug & DEBUG_MESH)  {
			rt_log("nmg_two_face_fuse(x%x, x%x) fg already shared\n",
				f1, f2);
		}
		return 0;	/* Already shared */
	}

	/*
	 *  First, a topology check.
	 *  If the two faces share one entire loop (of at least 3 verts)
	 *  according to topology, then by all rights the faces MUST be
	 *  shared.
	 */

	if( fabs(VDOT(fg1->N, fg2->N)) >= 0.99  &&
	    fabs(fg1->N[3]) - fabs(fg2->N[3]) < 100 * tol->dist  &&
	    nmg_is_common_bigloop( f1, f2 ) )  {
		if( VDOT( fg1->N, fg2->N ) < 0 )  flip2 = 1;
		if (rt_g.NMG_debug & DEBUG_MESH)  {
			rt_log("nmg_two_face_fuse(x%x, x%x) faces have a common loop, they MUST be fused.  flip2=%d\n",
				f1, f2, flip2);
		}
		goto must_fuse;
	}

	/* See if faces are coplanar */
	code = rt_coplanar( fg1->N, fg2->N, tol );
	if( code <= 0 )  {
		if (rt_g.NMG_debug & DEBUG_MESH)  {
			rt_log("nmg_two_face_fuse(x%x, x%x) faces non-coplanar\n",
				f1, f2);
		}
		return 0;
	}
	if( code == 1 )
		flip2 = 0;
	else
		flip2 = 1;

	if (rt_g.NMG_debug & DEBUG_MESH)  {
		rt_log("nmg_two_face_fuse(x%x, x%x) coplanar faces, rt_coplanar code=%d, flip2=%d\n",
			f1, f2, code, flip2);
	}

	/*
	 *  Plane equations match, within tol.
	 *  Before conducting a merge, verify that
	 *  all the verts in f2 are within tol->dist
	 *  of f1's surface.
	 */
	if( nmg_ck_fg_verts( f2->fu_p, f1, tol ) != 0 )  {
		if (rt_g.NMG_debug & DEBUG_MESH)  {
			rt_log("nmg_two_face_fuse: f2 verts not within tol of f1's surface, can't fuse\n");
		}
		return 0;
	}

must_fuse:
	/* All points are on the plane, it's OK to fuse */
	if( flip2 == 0 )  {
		if (rt_g.NMG_debug & DEBUG_MESH)  {
			rt_log("joining face geometry (same dir) f1=x%x, f2=x%x\n", f1, f2);
			PLPRINT(" fg1", fg1->N);
			PLPRINT(" fg2", fg2->N);
		}
		nmg_jfg( f1, f2 );
	} else {
		register struct face	*fn;
		if (rt_g.NMG_debug & DEBUG_MESH)  {
			rt_log("joining face geometry (opposite dirs)\n");

			rt_log(" f1=x%x, flip=%d", f1, f1->flip);
			PLPRINT(" fg1", fg1->N);

			rt_log(" f2=x%x, flip=%d", f2, f2->flip);
			PLPRINT(" fg2", fg2->N);
		}
		/* Flip flags of faces using fg2, first! */
		for( RT_LIST_FOR( fn, face, &fg2->f_hd ) )  {
			NMG_CK_FACE(fn);
			fn->flip = !fn->flip;
			if (rt_g.NMG_debug & DEBUG_MESH)  {
				rt_log("f=x%x, new flip=%d\n", fn, fn->flip);
			}
		}
		nmg_jfg( f1, f2 );
	}
	return 1;
}

/*
 *			N M G _ M O D E L _ F A C E _ F U S E
 *
 *  A routine to find all face geometry structures in an nmg model that
 *  have the same plane equation, and have them share face geometry.
 *  (See also nmg_shell_coplanar_face_merge(), which actually moves
 *  the loops into one face).
 *
 *  The criteria for two face geometry structs being the "same" are:
 *	1) The plane equations must be the same, within tolerance.
 *	2) All the vertices on the 2nd face must lie within the
 *	   distance tolerance of the 1st face's plane equation.
 */
int
nmg_model_face_fuse( m, tol )
struct model		*m;
CONST struct rt_tol	*tol;
{
	struct nmg_ptbl	ftab;
	int		total = 0;
	register int	i,j;

	NMG_CK_MODEL(m);
	RT_CK_TOL(tol);

	/* Make a list of all the face structs in the model */
	nmg_face_tabulate( &ftab, &m->magic );

	for( i = NMG_TBL_END(&ftab)-1; i >= 0; i-- )  {
		register struct face	*f1;
		register struct face_g_plane	*fg1;
		f1 = (struct face *)NMG_TBL_GET(&ftab, i);
		NMG_CK_FACE(f1);
		NMG_CK_FACE_G_EITHER(f1->g.magic_p);

		if( *f1->g.magic_p == NMG_FACE_G_SNURB_MAGIC )  {
			/* XXX Need routine to compare 2 snurbs for equality here */
			continue;
		}

		fg1 = f1->g.plane_p;
		NMG_CK_FACE_G_PLANE(fg1);

		/* Check that all the verts of f1 are within tol of face */
		if( nmg_ck_fu_verts( f1->fu_p, f1, tol ) != 0 )  {
			PLPRINT(" f1", f1->g.plane_p->N);
			nmg_pr_fu_briefly(f1->fu_p, 0);
			rt_bomb("nmg_model_face_fuse(): verts not within tol of containing face\n");
		}

		for( j = i-1; j >= 0; j-- )  {
			register struct face	*f2;
			register struct face_g_plane	*fg2;

			f2 = (struct face *)NMG_TBL_GET(&ftab, j);
			NMG_CK_FACE(f2);
			fg2 = f2->g.plane_p;
			if( !fg2 )  continue;
			NMG_CK_FACE_G_PLANE(fg2);

			if( fg1 == fg2 )  continue;	/* Already shared */

			if( nmg_two_face_fuse( f1, f2, tol ) > 0 )
				total++;
		}
	}
	nmg_tbl( &ftab, TBL_FREE, 0 );
	if( rt_g.NMG_debug & DEBUG_BASIC && total > 0 )
		rt_log("nmg_model_face_fuse: %d faces fused\n", total);
	return total;
}

/*
 *			N M G _ M O D E L _ F U S E
 *
 *  This is the primary application interface to the geometry fusing support.
 *  Fuse together all data structures that are equal to each other,
 *  within tolerance.
 *
 *  The algorithm is three part:
 *	1)  Fuse together all vertices.
 *	2)  Fuse together all face geometry, where appropriate.
 *	3)  Fuse together all edges.
 *
 *  Edge fusing is handled last, because the difficult part there is
 *  sorting faces radially around the edge.
 *  It is important to know whether faces are shared or not
 *  at that point.
 *
 *  XXX It would be more efficient to build all the ptbl's at once,
 *  XXX with a single traversal of the model.
 */
int
nmg_model_fuse( m, tol )
struct model		*m;
CONST struct rt_tol	*tol;
{
	int	total = 0;

	NMG_CK_MODEL(m);
	RT_CK_TOL(tol);

	/* Step 1 -- the vertices. */
	total += nmg_model_vertex_fuse( m, tol );

	/* Step 2 -- the face geometry */
	total += nmg_model_face_fuse( m, tol );

	/* Step 3 -- edges */
	total += nmg_model_edge_fuse( m, tol );

	/* Step 4 -- edge geometry */
	total += nmg_model_edge_g_fuse( m, tol );

	if( rt_g.NMG_debug & DEBUG_BASIC && total > 0 )
		rt_log("nmg_model_fuse(): %d entities fused\n", total);
	return total;
}
