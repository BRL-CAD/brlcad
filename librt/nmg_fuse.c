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

#include <stdio.h>
#include <math.h>
#include "externs.h"
#include "machine.h"
#include "vmath.h"
#include "nmg.h"
#include "raytrace.h"
#include "./debug.h"

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
	int	count = 0;
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
 *			N M G _ R E G I O N _ S E L F _ V F U S E
 *
 *  Working from the end to the front, scan for geometric duplications
 *  within a single list of vertex structures.
 *
 *  Exists primarily as a support routine for nmg_two_region_vertex_fuse().
 */
int
nmg_region_self_vfuse( t, tol )
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
if(count)rt_log("nmg_region_self_vfuse() %d\n", count);
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
if(count)rt_log("nmg_region_both_vfuse() %d\n", count);
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

	total = nmg_region_self_vfuse( &t1, tol );
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

	total = nmg_region_self_vfuse( &t1, tol );

	nmg_tbl( &t1, TBL_FREE, 0 );

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
rt_log("nmg_model_edge_fuse(): %d edges fused\n", total);
	return total;
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
		register struct face_g	*fg1;
		f1 = (struct face *)NMG_TBL_GET(&ftab, i);
		NMG_CK_FACE(f1);
		fg1 = f1->fg_p;
		if( !fg1 )  continue;
		NMG_CK_FACE_G(fg1);

		for( j = i-1; j >= 0; j-- )  {
			register struct face	*f2;
			register struct face_g	*fg2;
			FAST fastf_t		dist;
			struct nmg_ptbl		vtab;
			int			flip2 = 0;
			int			k;

			f2 = (struct face *)NMG_TBL_GET(&ftab, j);
			NMG_CK_FACE(f2);
			fg2 = f2->fg_p;
			if( !fg2 )  continue;
			NMG_CK_FACE_G(fg2);

			if( fg1 == fg2 )  continue;	/* Already shared */

			/* Compare distances from origin */
			dist = fg1->N[3] - fg2->N[3];
			if( !NEAR_ZERO(dist, tol->dist) )  {
				/* How about with reversed normal? */
				dist = fg1->N[3] + fg2->N[3];
				if( !NEAR_ZERO(dist, tol->dist) )
					continue;
				/* Dist matches, how about direction?
				 * Dot will be -1 if dirs are opposite.
				 */
				dist = -VDOT( fg1->N, fg2->N );
				if( !(dist >= tol->para) )  continue;

				/* Geometric match, with flipped signs */
				flip2 = 1;
			} else {
				/* Dist matches, how about direction?
				 * Dot will be +1 if dirs are the same.
				 */
				dist = VDOT( fg1->N, fg2->N );
				if( !(dist >= tol->para) )  continue;

				/* Geometric match, same sign */
			}

			/*
			 *  Plane equations match, within tol.
			 *  Before conducting a merge, verify that
			 *  all the verts in f2 are within tol->dist
			 *  of fg1's plane equation.
			 */
			nmg_vertex_tabulate( &vtab, &f2->fu_p->l.magic );

			for( k = NMG_TBL_END(&vtab)-1; k >= 0; k-- )  {
				register struct vertex		*v;
				register struct vertex_g	*vg;
				v = (struct vertex *)NMG_TBL_GET(&vtab, k);
				NMG_CK_VERTEX(v);
				vg = v->vg_p;
				if( !vg )  rt_bomb("nmg_model_face_fuse: vertex with no geometry?\n");

				dist = DIST_PT_PLANE(vg->coord, fg1->N);
				if( dist > tol->dist )  {
					if (rt_g.NMG_debug & DEBUG_MESH)  {
						rt_log("nmg_model_face_fuse: plane eqns equal, v x%x off plane by %e, skipping (f1=x%x, f2=x%x)\n", v, dist, f1, f2);
						VPRINT(" pt", vg->coord);
						PLPRINT(" fg1", fg1->N);
						PLPRINT(" fg2", fg2->N);
					}
					goto next_face;
				}
			}
			/* All points are on the plane, it's OK to fuse */
			if( flip2 == 0 )  {
				if (rt_g.NMG_debug & DEBUG_MESH)  {
					rt_log("joining face geometry (same dir) f1=x%x, f2=x%x\n", f1, f2);
					PLPRINT(" fg1", fg1->N);
					PLPRINT(" fg2", fg2->N);
				}
				nmg_jfg( f1, f2 );
				total++;
			} else {
				register struct face	*fn;

				if (rt_g.NMG_debug & DEBUG_MESH)  {
					rt_log("joining face geometry (opposite dirs)\n");
				}
				/* Flip flags of faces using fg2, first! */
				for( RT_LIST_FOR( fn, face, &fg2->f_hd ) )  {
					NMG_CK_FACE(fn);
					fn->flip = !fn->flip;
				}
				nmg_jfg( f1, f2 );
				total++;
			}

next_face:		
			nmg_tbl( &vtab, TBL_FREE, 0 );
		}
	}
	nmg_tbl( &ftab, TBL_FREE, 0 );
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

rt_log("nmg_model_fuse(): %d entities fused\n", total);
	return total;
}
