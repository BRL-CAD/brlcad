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

	nmg_region_vertex_list( &t, r1 );

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

	nmg_region_vertex_list( &t1, r1 );
	nmg_region_vertex_list( &t2, r2 );

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

	nmg_model_vertex_list( &t1, m );

	total = nmg_region_self_vfuse( &t1, tol );

	nmg_tbl( &t1, TBL_FREE, 0 );

	return total;
}
