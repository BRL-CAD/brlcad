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
#include "nurb.h"
#include "./debug.h"

extern int debug_file_count;

/* XXX Move to raytrace.h */
RT_EXTERN(struct vertexuse 	*nmg_is_vertex_in_face, (CONST struct vertex *v,
				CONST struct face *f));
RT_EXTERN(struct edge_g_lseg	*nmg_pick_best_edge_g, (struct edgeuse *eu1,
				struct edgeuse *eu2, CONST struct bn_tol *tol));
RT_EXTERN(void			nmg_pr_radial_list, (CONST struct bu_list *hd,
				CONST struct bn_tol *tol));
void nmg_pr_radial();

RT_EXTERN( fastf_t mat_determinant, ( mat_t matrix ) );

/* XXX move to nmg_info.c */
/*
 *			N M G _ F I N D _ O T _ S A M E _ E U _ O F _ E
 *
 *  If there is an edgeuse of an OT_SAME faceuse on this edge, return it.
 *  Only return a wire edgeuse if that is all there is.
 *  Useful for selecting a "good" edgeuse to pass to nmg_eu_2vecs_perp().
 */
struct edgeuse *
nmg_find_ot_same_eu_of_e( e )
CONST struct edge	*e;
{
	register struct edgeuse	*eu1;
	register struct edgeuse	*eu;
	struct faceuse		*fu;

	NMG_CK_EDGE(e);
	eu = eu1 = e->eu_p;
	do  {
		fu = nmg_find_fu_of_eu(eu);
		if( fu && fu->orientation == OT_SAME )  return eu;

		fu = nmg_find_fu_of_eu(eu->eumate_p);
		if( fu && fu->orientation == OT_SAME )  return eu->eumate_p;
		eu = eu->radial_p->eumate_p;
	} while( eu != eu1 );
	return eu1;		/* All wire */
}

/* XXX Move to nmg_info.c */
/*
 *			N M G _ I S _ V E R T E X _ I N _ F A C E
 *
 *  Returns -
 *	vu	One use of vertex 'v' in face 'f'.
 *	NULL	If there are no uses of 'v' in 'f'.
 */
struct vertexuse *
nmg_is_vertex_in_face( v, f )
CONST struct vertex	*v;
CONST struct face	*f;
{
	struct vertexuse	*vu;

	NMG_CK_VERTEX(v);
	NMG_CK_FACE(f);

	for( BU_LIST_FOR( vu, vertexuse, &v->vu_hd ) )  {
		register CONST struct edgeuse	*eu;
		register CONST struct loopuse	*lu;
		register CONST struct faceuse	*fu;

		if( *vu->up.magic_p != NMG_EDGEUSE_MAGIC )  continue;
		if( *(eu = vu->up.eu_p)->up.magic_p != NMG_LOOPUSE_MAGIC )  continue;
		lu = eu->up.lu_p;
		if( *lu->up.magic_p != NMG_FACEUSE_MAGIC )  continue;
		fu = lu->up.fu_p;
		if( fu->f_p != f )  continue;
		return vu;
	}
	return (struct vertexuse *)NULL;
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
	for( BU_LIST_FOR( lu1, loopuse, &fu1->lu_hd ) )  {
		if( BU_LIST_FIRST_MAGIC(&lu1->down_hd) == NMG_VERTEXUSE_MAGIC )
			continue;
		nverts = 0;
		nbadv = 0;
		magic1 = (long *)NULL;
		magic2 = (long *)NULL;
		got_three = 0;
		for( BU_LIST_FOR( eu1, edgeuse, &lu1->down_hd ) )  {
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
CONST struct bn_tol	*tol;
{
	int	i;
	int	j;
	struct bu_ptbl	t;

	NMG_CK_REGION(r1);
	BN_CK_TOL(tol);

	nmg_vertex_tabulate( &t, &r1->l.magic );

	for( i = BU_PTBL_END(&t)-1; i >= 0; i-- )  {
		register struct vertex	*vi;
		vi = (struct vertex *)BU_PTBL_GET(&t, i);
		NMG_CK_VERTEX(vi);
		if( !vi->vg_p )  continue;

		for( j = i-1; j >= 0; j-- )  {
			register struct vertex	*vj;
			vj = (struct vertex *)BU_PTBL_GET(&t, j);
			NMG_CK_VERTEX(vj);
			if( !vj->vg_p )  continue;
			if( !bn_pt3_pt3_equal( vi->vg_p->coord, vj->vg_p->coord, tol ) )
				continue;
			/* They are the same */
			bu_log("nmg_region_v_unique():  2 verts are the same, within tolerance\n");
			nmg_pr_v( vi, 0 );
			nmg_pr_v( vj, 0 );
			rt_bomb("nmg_region_v_unique()\n");
		}
	}
	bu_ptbl_free( &t);
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
bu_ptbl_vfuse( t, tol )
struct bu_ptbl		*t;
CONST struct bn_tol	*tol;
{
	int	count = 0;
	int	i;
	int	j;

	for( i = BU_PTBL_END(t)-1; i >= 0; i-- )  {
		register struct vertex	*vi;
		vi = (struct vertex *)BU_PTBL_GET(t, i);
		NMG_CK_VERTEX(vi);
		if( !vi->vg_p )  continue;

		for( j = i-1; j >= 0; j-- )  {
			register struct vertex	*vj;
			vj = (struct vertex *)BU_PTBL_GET(t, j);
			NMG_CK_VERTEX(vj);
			if( !vj->vg_p )  continue;
			if( !bn_pt3_pt3_equal( vi->vg_p->coord, vj->vg_p->coord, tol ) )
				continue;
			/* They are the same, fuse vi into vj */
			nmg_jv( vj, vi );
			bu_ptbl_rm( t, (long *)vi );
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
 *  rather than bothering to do a BU_PTBL_RM, which will re-copy the
 *  list to compress it.
 *
 *  Exists as a support routine for nmg_two_region_vertex_fuse()
 */
int
nmg_region_both_vfuse( t1, t2, tol )
struct bu_ptbl		*t1;
struct bu_ptbl		*t2;
CONST struct bn_tol	*tol;
{
	int	count = 0;
	int	i;
	int	j;

	/* Verify t2 is good to start with */
	for( j = BU_PTBL_END(t2)-1; j >= 0; j-- )  {
		register struct vertex	*vj;
		vj = (struct vertex *)BU_PTBL_GET(t2, j);
		NMG_CK_VERTEX(vj);
	}

	for( i = BU_PTBL_END(t1)-1; i >= 0; i-- )  {
		register struct vertex	*vi;
		vi = (struct vertex *)BU_PTBL_GET(t1, i);
		NMG_CK_VERTEX(vi);
		if( !vi->vg_p )  continue;

		for( j = BU_PTBL_END(t2)-1; j >= 0; j-- )  {
			register struct vertex	*vj;
			vj = (struct vertex *)BU_PTBL_GET(t2, j);
			if(!vj) continue;
			NMG_CK_VERTEX(vj);
			if( !vj->vg_p )  continue;
			if( !bn_pt3_pt3_equal( vi->vg_p->coord, vj->vg_p->coord, tol ) )
				continue;
			/* They are the same, fuse vj into vi */
			nmg_jv( vi, vj );
			BU_PTBL_GET(t2, j) = 0;
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
CONST struct bn_tol	*tol;
{
	struct bu_ptbl	t1;
	struct bu_ptbl	t2;
	int		total = 0;

	NMG_CK_REGION(r1);
	NMG_CK_REGION(r2);
	BN_CK_TOL(tol);

	if( r1->m_p != r2->m_p )  rt_bomb("nmg_two_region_vertex_fuse:  regions not in same model\n");

	nmg_vertex_tabulate( &t1, &r1->l.magic );
	nmg_vertex_tabulate( &t2, &r2->l.magic );

	total = bu_ptbl_vfuse( &t1, tol );
	total += nmg_region_both_vfuse( &t1, &t2, tol );

	bu_ptbl_free( &t1);
	bu_ptbl_free( &t2);

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
CONST struct bn_tol	*tol;
{
	struct bu_ptbl	t1;
	int		total = 0;

	NMG_CK_MODEL(m);
	BN_CK_TOL(tol);

	nmg_vertex_tabulate( &t1, &m->magic );

	total = bu_ptbl_vfuse( &t1, tol );

	bu_ptbl_free( &t1);

	if( rt_g.NMG_debug & DEBUG_BASIC && total > 0 )
		bu_log("nmg_model_vertex_fuse() %d\n", total);
	return total;
}

/*
 *		N M G _ C N U R B _ I S _ L I N E A R
 *
 *	Checks if cnurb is linear
 *
 *	Returns:
 *		1 - cnurb is linear
 *		0 - either cnurb is not linear, or it's not obvious
 */

int
nmg_cnurb_is_linear( cnrb )
CONST struct edge_g_cnurb *cnrb;
{
	int i;
	int coords;
	int last_index;
	int linear=0;

	NMG_CK_EDGE_G_CNURB( cnrb );

	if( rt_g.NMG_debug & DEBUG_MESH )
	{
		bu_log( "nmg_cnurb_is_linear( x%x )\n", cnrb );
		rt_nurb_c_print( cnrb );
	}

	if( cnrb->order <= 0 )
	{
		linear = 1;
		goto out;
	}

	if( cnrb->order == 2 )
	{
		if( cnrb->c_size == 2 )
		{
			linear = 1;
			goto out;
		}
	}

	coords = RT_NURB_EXTRACT_COORDS( cnrb->pt_type );
	last_index = (cnrb->c_size - 1)*coords;

	/* Check if all control points are either the start point or end point */
	for( i=1 ; i<cnrb->c_size-2 ; i++ )
	{
		int index;

		index = i*coords;

		if( VEQUAL( &cnrb->ctl_points[0], &cnrb->ctl_points[i] ) )
			continue;
		if( VEQUAL( &cnrb->ctl_points[last_index], &cnrb->ctl_points[i] ) )
			continue;

		goto out;
	}

	linear = 1;

out:
	if( rt_g.NMG_debug & DEBUG_MESH )
		bu_log( "nmg_cnurb_is_linear( x%x ) returning %d\n", cnrb, linear );

	return( linear );
}

/*
 *			N M G _ S N U R B _ I S _ P L A N A R
 *
 *	Checks if snurb surface is planar
 *
 *	Returns:
 *		0 - surface is not planar
 *		1 - surface is planar (within tolerance)
 */

int
nmg_snurb_is_planar( srf, tol )
CONST struct face_g_snurb *srf;
CONST struct bn_tol *tol;
{
	plane_t pl;
	int i;
	int coords;
	mat_t matrix;
	mat_t inverse;
	vect_t vsum;
	double det;
	double one_over_vertex_count;
	int planar=0;

	NMG_CK_FACE_G_SNURB( srf );
	BN_CK_TOL( tol );

	if( rt_g.NMG_debug & DEBUG_MESH )
	{
		bu_log( "nmg_snurb_is_planar( x%x )\n", srf );
#if 1
		rt_nurb_s_print( "", srf );
#endif
	}

	if( srf->order[0] == 2 && srf->order[1] == 2 )
	{
		if( srf->s_size[0] == 2 && srf->s_size[1] == 2 )
		{
			planar = 1;
			goto out;
		}
	}

	/* build matrix */
	mat_zero( matrix );
	VSET( vsum , 0.0 , 0.0 , 0.0 );

	one_over_vertex_count = 1.0/(double)(srf->s_size[0]*srf->s_size[1]);
	coords = RT_NURB_EXTRACT_COORDS( srf->pt_type );

	/* calculate an average plane for all control points */
	for( i=0 ; i<srf->s_size[0]*srf->s_size[1] ; i++ )
	{
		fastf_t *pt;

		pt = &srf->ctl_points[ i*coords ];

		matrix[0] += pt[X] * pt[X];
		matrix[1] += pt[X] * pt[Y];
		matrix[2] += pt[X] * pt[Z];
		matrix[5] += pt[Y] * pt[Y];
		matrix[6] += pt[Y] * pt[Z];
		matrix[10] += pt[Z] * pt[Z];

		vsum[X] += pt[X];
		vsum[Y] += pt[Y];
		vsum[Z] += pt[Z];
	}
	matrix[4] = matrix[1];
	matrix[8] = matrix[2];
	matrix[9] = matrix[6];
	matrix[15] = 1.0;
		
	/* Check that we don't have a singular matrix */
	det = mat_determinant( matrix );

	if( !NEAR_ZERO( det , SMALL_FASTF ) )
	{
		fastf_t inv_len_pl;

		/* invert matrix */
		mat_inv( inverse , matrix );

		/* get normal vector */
		MAT4X3PNT( pl , inverse , vsum );

		/* unitize direction vector */
		inv_len_pl = 1.0/(MAGNITUDE( pl ));
		HSCALE( pl , pl , inv_len_pl );

		/* get average vertex coordinates */
		VSCALE( vsum, vsum, one_over_vertex_count );

		/* get distance from plane to orgin */
		pl[H] = VDOT( pl , vsum );

	}
	else
	{
		int x_same=1;
		int y_same=1;
		int z_same=1;

		/* singular matrix, may occur if all vertices have the same zero
		 * component.
		 */
		for( i=1 ; i<srf->s_size[0]*srf->s_size[1] ; i++ )
		{
			if( srf->ctl_points[i*coords+X] != srf->ctl_points[X] )
				x_same = 0;
			if( srf->ctl_points[i*coords+Y] != srf->ctl_points[Y] )
				y_same = 0;
			if( srf->ctl_points[i*coords+Z] != srf->ctl_points[Z] )
				z_same = 0;

			if( !x_same && !y_same && !z_same )
				break;
		}

		if( x_same )
		{
			VSET( pl , 1.0 , 0.0 , 0.0 );
		}
		else if( y_same )
		{
			VSET( pl , 0.0 , 1.0 , 0.0 );
		}
		else if( z_same )
		{
			VSET( pl , 0.0 , 0.0 , 1.0 );
		}

		if( x_same || y_same || z_same )
		{
			/* get average vertex coordinates */
			VSCALE( vsum, vsum, one_over_vertex_count );

			/* get distance from plane to orgin */
			pl[H] = VDOT( pl , vsum );

		}
		else
		{
			bu_log( "nmg_snurb_is_plana: Cannot calculate plane for snurb x%x\n" , srf );
			rt_nurb_s_print( "", srf );
			rt_bomb( "nmg_snurb_is_plana: Cannot calculate plane for snurb\n" );
		}
	}

	/* Now verify that every control point is on this plane */
	for( i=0 ; i<srf->s_size[0]*srf->s_size[1] ; i++ )
	{
		fastf_t *pt;
		fastf_t dist;

		pt = &srf->ctl_points[ i*coords ];

		dist = DIST_PT_PLANE( pt, pl );
		if( dist > tol->dist )
			goto out;
	}

	planar = 1;
out:
	if( rt_g.NMG_debug & DEBUG_MESH )
		bu_log( "nmg_snurb_is_planar( x%x ) returning %d\n", srf, planar );

	return( planar );

}

struct pt_list
{
	struct bu_list l;
	point_t xyz;
	fastf_t t;
};

void
nmg_eval_linear_trim_curve( snrb, uvw, xyz )
CONST struct face_g_snurb *snrb;
CONST fastf_t uvw[3];
point_t xyz;
{
	int coords;
	hpoint_t xyz1;

	if( snrb )
	{
		NMG_CK_FACE_G_SNURB( snrb );
		rt_nurb_s_eval( snrb, uvw[0], uvw[1], xyz1 );
		if( RT_NURB_IS_PT_RATIONAL( snrb->pt_type ) )
		{
			fastf_t inverse_weight;

			coords = RT_NURB_EXTRACT_COORDS( snrb->pt_type );
			inverse_weight = 1.0/xyz1[coords-1];

			VSCALE( xyz, xyz1, inverse_weight );
		}
		else
			VMOVE( xyz, xyz1 )
	}
	else
		VMOVE( xyz, uvw )

}

void
nmg_eval_trim_curve( cnrb, snrb, t, xyz )
CONST struct edge_g_cnurb *cnrb;
CONST struct face_g_snurb *snrb;
CONST fastf_t t;
point_t xyz;
{
	hpoint_t uvw;
	hpoint_t xyz1;
	int coords;

	NMG_CK_EDGE_G_CNURB( cnrb );
	if( snrb )
	{
		NMG_CK_FACE_G_SNURB( snrb )
	}

	rt_nurb_c_eval( cnrb, t, uvw );

	if( RT_NURB_IS_PT_RATIONAL( cnrb->pt_type ) )
	{
		fastf_t inverse_weight;

		coords = RT_NURB_EXTRACT_COORDS( cnrb->pt_type );
		inverse_weight = 1.0/uvw[coords-1];

		VSCALE( uvw, uvw, inverse_weight );
	}

	if( snrb )
	{
		rt_nurb_s_eval( snrb, uvw[0], uvw[1], xyz1 );
		if( RT_NURB_IS_PT_RATIONAL( snrb->pt_type ) )
		{
			fastf_t inverse_weight;

			coords = RT_NURB_EXTRACT_COORDS( snrb->pt_type );
			inverse_weight = 1.0/xyz1[coords-1];

			VSCALE( xyz, xyz1, inverse_weight );
		}
		else
			VMOVE( xyz, xyz1 )
	}
	else
		VMOVE( xyz, uvw )

#if 0
	if( rt_g.NMG_debug & DEBUG_MESH )
		bu_log( "nmg_eval_trim_curve returning (%g %g %g )\n", V3ARGS( xyz ) );
#endif

}

void
nmg_split_trim( cnrb, snrb, t, pt0, pt1, tol )
CONST struct edge_g_cnurb *cnrb;
CONST struct face_g_snurb *snrb;
fastf_t t;
struct pt_list *pt0,*pt1;
CONST struct bn_tol *tol;
{
	struct pt_list *pt_new;
	fastf_t t_sub;
	vect_t seg;

	NMG_CK_EDGE_G_CNURB( cnrb );
	NMG_CK_FACE_G_SNURB( snrb );
	BN_CK_TOL( tol );
#if 0
	if( rt_g.NMG_debug & DEBUG_MESH )
		bu_log( "nmg_split_trim( cnrb=x%x, snrb=x%x, t=%g, pt0=x%x, pt1=x%x )START\n",
			cnrb, snrb, t, pt0, pt1 );
#endif
	pt_new = (struct pt_list *)rt_malloc( sizeof( struct pt_list ), "g_split_trim: pt_new" );
	pt_new->t = t;

	if( pt_new->t < pt0->t || pt_new->t > pt1->t )
	{
		bu_log( "nmg_split_trim: split parameter (%g) is not between ends (%g and %g)\n",
			t, pt0->t, pt1->t );
		rt_bomb( "nmg_split_trim: split parameteris not between ends\n" );
	}

	nmg_eval_trim_curve( cnrb, snrb, pt_new->t, pt_new->xyz );

	BU_LIST_INSERT( &pt1->l, &pt_new->l );

	VSUB2( seg, pt0->xyz, pt_new->xyz );
	if( MAGSQ( seg ) > tol->dist_sq )
	{
		t_sub = (pt0->t + pt_new->t)/2.0;
#if 0
		if( rt_g.NMG_debug & DEBUG_MESH )
			bu_log( "nmg_split_trim: recursing at t=%g (%g,%g)\n",
					t_sub, pt0->t, pt_new->t );
#endif
		nmg_split_trim( cnrb, snrb, t_sub, pt0, pt_new, tol );
	}

	VSUB2( seg, pt_new->xyz, pt1->xyz );
	if( MAGSQ( seg ) > tol->dist_sq )
	{
		t_sub = (pt_new->t + pt1->t)/2.0;
#if 0
		if( rt_g.NMG_debug & DEBUG_MESH )
			bu_log( "nmg_split_trim: recursing at t=%g (%g,%g)\n",
					t_sub, pt_new->t, pt1->t );
#endif
		nmg_split_trim( cnrb, snrb, t_sub, pt_new, pt1, tol );
	}
#if 0
	if( rt_g.NMG_debug & DEBUG_MESH )
		bu_log( "nmg_split_trim( cnrb=x%x, snrb=x%x, t=%g, pt0=x%x, pt1=x%x ) END\n",
			cnrb, snrb, t, pt0, pt1 );
#endif

}

void
nmg_eval_trim_to_tol( cnrb, snrb, t0, t1, head, tol )
CONST struct edge_g_cnurb *cnrb;
CONST struct face_g_snurb *snrb;
CONST fastf_t t0,t1;
struct bu_list *head;
CONST struct bn_tol *tol;
{
	fastf_t t;
	int done=0;
	point_t xyz;
	struct pt_list *pt0,*pt1;

	NMG_CK_EDGE_G_CNURB( cnrb );
	NMG_CK_FACE_G_SNURB( snrb );
	BN_CK_TOL( tol );

	if( rt_g.NMG_debug & DEBUG_MESH )
		bu_log( "nmg_eval_trim_to_tol( cnrb=x%x, snrb=x%x, t0=%g, t1=%g ) START\n",
				cnrb, snrb, t0, t1 );

	pt0 = (struct pt_list *)rt_malloc( sizeof( struct pt_list ), "nmg_eval_trim_to_tol: pt0 " );
	pt0->t = t0;
	nmg_eval_trim_curve( cnrb, snrb, pt0->t, pt0->xyz );
	BU_LIST_INSERT( head, &pt0->l );

	pt1 = (struct pt_list *)rt_malloc( sizeof( struct pt_list ), "nmg_eval_trim_to_tol: pt1 " );
	pt1->t = t1;
	nmg_eval_trim_curve( cnrb, snrb, pt1->t, pt1->xyz );
	BU_LIST_INSERT( head, &pt1->l );

	t = (t0 + t1)/2.0;
	nmg_split_trim( cnrb, snrb, t, pt0, pt1, tol );

	if( rt_g.NMG_debug & DEBUG_MESH )
		bu_log( "nmg_eval_trim_to_tol( cnrb=x%x, snrb=x%x, t0=%g, t1=%g ) END\n",
				cnrb, snrb, t0, t1 );
}

void
nmg_split_linear_trim( snrb, uvw1, uvw, uvw2, pt0, pt1, tol )
CONST struct face_g_snurb *snrb;
CONST fastf_t uvw1[3];
CONST fastf_t uvw[3];
CONST fastf_t uvw2[3];
struct pt_list *pt0;
struct pt_list *pt1;
CONST struct bn_tol *tol;
{
	struct pt_list *pt_new;
	fastf_t t_sub;
	fastf_t uvw_sub[3];
	vect_t seg;

	if( snrb )
		NMG_CK_FACE_G_SNURB( snrb );
	BN_CK_TOL( tol );
#if 0
	if( rt_g.NMG_debug & DEBUG_MESH )
		bu_log( "nmg_split_linear_trim( snrb=x%x, pt0=x%x, pt1=x%x )START\n",
			snrb, pt0, pt1 );
#endif
	pt_new = (struct pt_list *)rt_malloc( sizeof( struct pt_list ), "g_split_trim: pt_new" );
	pt_new->t = 0.5*(pt0->t + pt1->t);

	VBLEND2( uvw_sub, 1.0 - pt_new->t, uvw1, pt_new->t, uvw2 );
	nmg_eval_linear_trim_curve( snrb, uvw_sub, pt_new->xyz );

	BU_LIST_INSERT( &pt1->l, &pt_new->l );

#if 0
	if( rt_g.NMG_debug & DEBUG_MESH )
		bu_log( "nmg_split_linear_trim: new segments (%g %g %g) <-> (%g %g %g) <-> (%g %g %g)\n",
			V3ARGS( pt0->xyz ), V3ARGS( pt_new->xyz ), V3ARGS( pt1->xyz ) );
#endif

	VSUB2( seg, pt0->xyz, pt_new->xyz );
	if( MAGSQ( seg ) > tol->dist_sq )
	{
		t_sub = (pt0->t + pt_new->t)/2.0;
		VBLEND2( uvw_sub, 1.0 - t_sub, uvw1, t_sub, uvw2 );
#if 0
		if( rt_g.NMG_debug & DEBUG_MESH )
			bu_log( "nmg_split_linear_trim: recursing at t=%g (%g,%g)\n",
					t_sub, pt0->t, pt_new->t );
#endif
		nmg_split_linear_trim( snrb, uvw1, uvw_sub, uvw2, pt0, pt_new, tol );
	}

	VSUB2( seg, pt_new->xyz, pt1->xyz );
	if( MAGSQ( seg ) > tol->dist_sq )
	{
		t_sub = (pt_new->t + pt1->t)/2.0;
		VBLEND2( uvw_sub, 1.0 - t_sub, uvw1, t_sub, uvw2 );
#if 0
		if( rt_g.NMG_debug & DEBUG_MESH )
			bu_log( "nmg_split_linear_trim: recursing at t=%g (%g,%g)\n",
					t_sub, pt_new->t, pt1->t );
#endif
		nmg_split_linear_trim( snrb, uvw1, uvw_sub, uvw2, pt0, pt_new, tol );
	}
#if 0
	if( rt_g.NMG_debug & DEBUG_MESH )
		bu_log( "nmg_split_linear_trim( snrb=x%x, pt0=x%x, pt1=x%x ) END\n",
			snrb, pt0, pt1 );
#endif

}

void
nmg_eval_linear_trim_to_tol( cnrb, snrb, uvw1, uvw2, head, tol )
CONST struct edge_g_cnurb *cnrb;
CONST struct face_g_snurb *snrb;
CONST fastf_t uvw1[3];
CONST fastf_t uvw2[3];
struct bu_list *head;
CONST struct bn_tol *tol;
{
	int done=0;
	point_t xyz;
	fastf_t uvw[3];
	struct pt_list *pt0,*pt1;

	NMG_CK_EDGE_G_CNURB( cnrb );
	NMG_CK_FACE_G_SNURB( snrb );
	BN_CK_TOL( tol );

	NMG_CK_EDGE_G_CNURB( cnrb );
	NMG_CK_FACE_G_SNURB( snrb );
	BN_CK_TOL( tol );

	if( rt_g.NMG_debug & DEBUG_MESH )
		bu_log( "nmg_eval_linear_trim_to_tol( cnrb=x%x, snrb=x%x, uvw1=( %g %g %g), uvw2=( %g %g %g ) ) START\n",
				cnrb, snrb, V3ARGS( uvw1 ), V3ARGS( uvw2 ) );

	pt0 = (struct pt_list *)rt_malloc( sizeof( struct pt_list ), "nmg_eval_linear_trim_to_tol: pt0 " );
	pt0->t = 0.0;
	nmg_eval_linear_trim_curve( snrb, uvw1, pt0->xyz );
	BU_LIST_INSERT( head, &pt0->l );

	pt1 = (struct pt_list *)rt_malloc( sizeof( struct pt_list ), "nmg_eval_linear_trim_to_tol: pt1 " );
	pt1->t = 1.0;
	nmg_eval_linear_trim_curve( snrb, uvw2, pt1->xyz );
	BU_LIST_INSERT( head, &pt1->l );


	VBLEND2( uvw, 0.5, uvw1, 0.5, uvw2 )
	nmg_split_linear_trim( snrb, uvw1, uvw, uvw2, pt0, pt1, tol );

	if( rt_g.NMG_debug & DEBUG_MESH )
		bu_log( "nmg_eval_linear_trim_to_tol( cnrb=x%x, snrb=x%x ) END\n",
				cnrb, snrb );
}

/* check for coincidence at twenty interior points along a cnurb */
#define		CHECK_NUMBER	20

/*		N M G _ C N U R B _ L S E G _ C O I N C I D E N T
 *
 *	Checks if CNURB is coincident with line segment from pt1 to pt2
 *	by calculating a number of points along the CNURB and checking
 *	if they lie on the line between pt1 and pt2 (within tolerance).
 *		NOTE: eu1 must be the EU referencing cnrb!!!!
 *
 *	Returns:
 *		0 - not coincident
 *		1 - coincident
 */
int
nmg_cnurb_lseg_coincident( eu1, cnrb, snrb, pt1, pt2, tol )
CONST struct edgeuse *eu1;
CONST struct edge_g_cnurb *cnrb;
CONST struct face_g_snurb *snrb;
CONST point_t pt1;
CONST point_t pt2;
CONST struct bn_tol *tol;
{
	fastf_t t0,t1,t;
	fastf_t delt;
	int coincident=0;
	int i;


	NMG_CK_EDGEUSE( eu1 );
	NMG_CK_EDGE_G_CNURB( cnrb );

	if( rt_g.NMG_debug & DEBUG_MESH )
		bu_log( "nmg_cnurb_lseg_coincident( eu1=x%x, cnrb=x%x, snrb=x%x, pt1=(%g %g %g), pt2=(%g %g %g)\n",
			eu1, cnrb, snrb, V3ARGS( pt1 ), V3ARGS( pt2 ) );

	if( eu1->g.cnurb_p != cnrb )
	{
		bu_log( "nmg_cnurb_lseg_coincident: cnrb x%x isn't from eu x%x\n",
			cnrb, eu1 );
		rt_bomb( "nmg_cnurb_lseg_coincident: cnrb and eu1 disagree\n" );
	}

	if( snrb )
		NMG_CK_FACE_G_SNURB( snrb );

	BN_CK_TOL( tol );

	if( cnrb->order <= 0 )
	{
		/* cnrb is linear in parameter space */
		struct vertexuse *vu1;
		struct vertexuse *vu2;
		struct vertexuse_a_cnurb *vua1;
		struct vertexuse_a_cnurb *vua2;

		if( !snrb )
			rt_bomb( "nmg_cnurb_lseg_coincident: No CNURB nor SNURB!!\n" );

		vu1 = eu1->vu_p;
		NMG_CK_VERTEXUSE( vu1 );
		if( !vu1->a.magic_p )
		{
			bu_log( "nmg_cnurb_lseg_coincident: vu (x%x) has no attributes\n",
				vu1 );
			rt_bomb( "nmg_cnurb_lseg_coincident: vu has no attributes\n" );
		}

		if( *vu1->a.magic_p != NMG_VERTEXUSE_A_CNURB_MAGIC )
		{
			bu_log( "nmg_cnurb_lseg_coincident: vu (x%x) from CNURB EU (x%x) is not CNURB\n", 
				vu1, eu1 );
			rt_bomb( "nmg_cnurb_lseg_coincident: vu from CNURB EU is not CNURB\n" );
		}

		vua1 = vu1->a.cnurb_p;
		NMG_CK_VERTEXUSE_A_CNURB( vua1 );

		vu2 = eu1->eumate_p->vu_p;
		NMG_CK_VERTEXUSE( vu2 );
		if( !vu2->a.magic_p )
		{
			bu_log( "nmg_cnurb_lseg_coincident: vu (x%x) has no attributes\n",
				vu2 );
			rt_bomb( "nmg_cnurb_lseg_coincident: vu has no attributes\n" );
		}

		if( *vu2->a.magic_p != NMG_VERTEXUSE_A_CNURB_MAGIC )
		{
			bu_log( "nmg_cnurb_lseg_coincident: vu (x%x) from CNURB EU (x%x) is not CNURB\n", 
				vu2, eu1 );
			rt_bomb( "nmg_cnurb_lseg_coincident: vu from CNURB EU is not CNURB\n" );
		}

		vua2 = vu2->a.cnurb_p;
		NMG_CK_VERTEXUSE_A_CNURB( vua2 );

		coincident = 1;
		for( i=0 ; i<CHECK_NUMBER ; i++ )
		{
			point_t uvw;
			point_t xyz;
			fastf_t blend;
			fastf_t dist;
			point_t pca;

			blend = (double)(i+1)/(double)(CHECK_NUMBER+1);
			VBLEND2( uvw, blend, vua1->param, (1.0-blend), vua2->param );

			nmg_eval_linear_trim_curve( snrb, uvw, xyz );

			if( bn_dist_pt3_lseg3( &dist, pca, pt1, pt2, xyz, tol ) > 2 )
			{
				coincident = 0;
				break;
			}
		}
		if( rt_g.NMG_debug & DEBUG_MESH )
			bu_log( "nmg_cnurb_lseg_coincident returning %d\n", coincident );
		return( coincident );
	}

	t0 = cnrb->k.knots[0];
	t1 = cnrb->k.knots[cnrb->k.k_size-1];
	delt = (t1 - t0)/(double)(CHECK_NUMBER+1);

	coincident = 1;
	for( i=0 ; i<CHECK_NUMBER ; i++ )
	{
		point_t xyz;
		fastf_t dist;
		point_t pca;

		t = t0 + (double)(i+1)*delt;

		nmg_eval_trim_curve( cnrb, snrb, t, xyz );

		if( bn_dist_pt3_lseg3( &dist, pca, pt1, pt2, xyz, tol ) > 2 )
		{
			coincident = 0;
			break;
		}
	}
	if( rt_g.NMG_debug & DEBUG_MESH )
		bu_log( "nmg_cnurb_lseg_coincident returning %d\n", coincident );
	return( coincident );
}

/*		N M G _ C N U R B _ I S _ O N _ C R V
 *
 *	Checks if CNURB eu lies on curve contained in list headed at "head"
 *	"Head" must contain a list of points (struct pt_list) each within
 *	tolerance of the next. (Just checks at "CHECK_NUMBER" points for now).
 *
 *	Returns:
 *		 0 - cnurb is not on curve;
 *		 1 - cnurb is on curve
 */
int
nmg_cnurb_is_on_crv( eu, cnrb, snrb, head, tol )
CONST struct edgeuse *eu;
CONST struct edge_g_cnurb *cnrb;
CONST struct face_g_snurb *snrb;
CONST struct bu_list *head;
CONST struct bn_tol *tol;
{
	int i;
	int coincident;
	fastf_t t, t0, t1;
	fastf_t delt;

	NMG_CK_EDGEUSE( eu );
	NMG_CK_EDGE_G_CNURB( cnrb );
	if( snrb )
		NMG_CK_FACE_G_SNURB( snrb );
	BU_CK_LIST_HEAD( head );
	BN_CK_TOL( tol );

	if( rt_g.NMG_debug & DEBUG_MESH )
		bu_log( "nmg_cnurb_is_on_crv( eu=x%x, cnrb=x%x, snrb=x%x, head=x%x )\n",
			eu, cnrb, snrb, head );
	if( cnrb->order <= 0 )
	{
		struct vertexuse *vu1,*vu2;
		struct vertexuse_a_cnurb *vu1a,*vu2a;
		fastf_t blend;
		point_t uvw;
		point_t xyz;

		/* cnurb is linear in parameter space */

		vu1 = eu->vu_p;
		NMG_CK_VERTEXUSE( vu1 );
		vu2 = eu->eumate_p->vu_p;
		NMG_CK_VERTEXUSE( vu2 );

		if( !vu1->a.magic_p )
		{
			bu_log( "nmg_cnurb_is_on_crv(): vu (x%x) on CNURB EU (x%x) has no attributes\n",
				vu1, eu );
			rt_bomb( "nmg_cnurb_is_on_crv(): vu on CNURB EU has no attributes\n" );
		}
		if( *vu1->a.magic_p != NMG_VERTEXUSE_A_CNURB_MAGIC )
		{
			bu_log( "nmg_cnurb_is_on_crv(): vu (x%x) on CNURB EU (x%x) is not CNURB\n",
				vu1, eu );
			rt_bomb( "nmg_cnurb_is_on_crv(): vu on CNURB EU is not CNURB\n" );
		}
		vu1a = vu1->a.cnurb_p;
		NMG_CK_VERTEXUSE_A_CNURB( vu1a );

		if( !vu2->a.magic_p )
		{
			bu_log( "nmg_cnurb_is_on_crv(): vu (x%x) on CNURB EU (x%x) has no attributes\n",
				vu2, eu->eumate_p );
			rt_bomb( "nmg_cnurb_is_on_crv(): vu on CNURB EU has no attributes\n" );
		}
		if( *vu2->a.magic_p != NMG_VERTEXUSE_A_CNURB_MAGIC )
		{
			bu_log( "nmg_cnurb_is_on_crv(): vu (x%x) on CNURB EU (x%x) is not CNURB\n",
				vu2, eu->eumate_p );
			rt_bomb( "nmg_cnurb_is_on_crv(): vu on CNURB EU is not CNURB\n" );
		}
		vu2a = vu2->a.cnurb_p;
		NMG_CK_VERTEXUSE_A_CNURB( vu2a );

		coincident = 1;
		for( i=0 ; i<CHECK_NUMBER ; i++ )
		{
			struct pt_list *pt;
			int found=0;

			blend = (double)(i+1)/(double)(CHECK_NUMBER+1);

			VBLEND2( uvw, blend, vu1a->param, (1.0-blend), vu2a->param );

			nmg_eval_linear_trim_curve( snrb, uvw, xyz );

			for( BU_LIST_FOR( pt, pt_list, head ) )
			{
				vect_t diff;

				VSUB2( diff, xyz, pt->xyz );
				if( MAGSQ( diff ) <= tol->dist_sq )
				{
					found = 1;
					break;
				}
			}
			if( !found )
			{
				coincident = 0;
				break;
			}
		}
		if( rt_g.NMG_debug & DEBUG_MESH )
			bu_log( "nmg_cnurb_is_on_crv() returning %d\n", coincident );
		return( coincident );
	}

	coincident = 1;
	t0 = cnrb->k.knots[0];
	t1 = cnrb->k.knots[cnrb->k.k_size-1];
	delt = (t1 - t0)/(double)(CHECK_NUMBER+1);
	for( i=0 ; i<CHECK_NUMBER ; i++ )
	{
		point_t xyz;
		struct pt_list *pt;
		int found;

		t = t0 + (double)(i+1)*delt;

		nmg_eval_trim_curve( cnrb, snrb, t, xyz );

		found = 0;
		for( BU_LIST_FOR( pt, pt_list, head ) )
		{
			vect_t diff;

			VSUB2( diff, xyz, pt->xyz );
			if( MAGSQ( diff ) <= tol->dist_sq )
			{
				found = 1;
				break;
			}
		}
		if( !found )
		{
			coincident = 0;
			break;
		}
	}

	if( rt_g.NMG_debug & DEBUG_MESH )
		bu_log( "nmg_cnurb_is_on_crv returning %d\n", coincident );
	return( coincident );
}

/*
 *			N M G _ M O D E L _ E D G E _ F U S E
 */
#if 0
int
nmg_model_edge_fuse( m, tol )
struct model		*m;
CONST struct bn_tol	*tol;
{
	struct bu_ptbl	eutab;
	int		total = 0;
	int		non_lseg=0;
	register int	i,j;

	NMG_CK_MODEL(m);
	BN_CK_TOL(tol);

	/* Make a list of all the edgeuse structs in the model */
again:
	total = 0;
	nmg_edgeuse_tabulate( &eutab, &m->magic );

	for( i = BU_PTBL_END(&eutab)-1; i >= 0; i-- )  {
		struct edgeuse		*eu1;
		struct edge		*e1;
		register struct vertex	*v1a, *v1b;
		int			lseg1=0;
		int lseg2;

		eu1 = (struct edgeuse *)BU_PTBL_GET(&eutab, i);
		NMG_CK_EDGEUSE(eu1);
		e1 = eu1->e_p;
		NMG_CK_EDGE(e1);
		NMG_CK_EDGE_G_EITHER(eu1->g.magic_p);

		v1a = eu1->vu_p->v_p;
		v1b = eu1->eumate_p->vu_p->v_p;
		NMG_CK_VERTEX(v1a);
		NMG_CK_VERTEX(v1b);

		if( v1a == v1b )  {
			if( *eu1->g.magic_p == NMG_EDGE_G_LSEG_MAGIC )  {
				/* 0-length lseg edge.  Eliminate. */
				/* These should't happen, but need fixing. */
				if( nmg_keu( eu1 ) )  {
					bu_log("nmg_model_edge_fuse() WARNING:  deletion of 0-length edgeuse=x%x caused parent to go empty.\n", eu1);
				}
				bu_log("nmg_model_edge_fuse() 0-length edgeuse=x%x deleted\n", eu1);
				/* XXX no way to know where edgeuse mate will be */
				bu_ptbl_free( &eutab);
				goto again;
			}
			/* For g_cnurb edges, could check for distinct param values on either end of edge */
		}

		if( !eu1->g.magic_p )
		{
			bu_log( "nmg_model_edge_fuse() WARNING: eu (x%x) has no geometry\n" , eu1 );
			continue;
		}

		/* Check if eu1 is a line segment */
		if( *eu1->g.magic_p == NMG_EDGE_G_LSEG_MAGIC )
			lseg1 = 1;
		else if( *eu1->g.magic_p == NMG_EDGE_G_CNURB_MAGIC )
		{
			non_lseg++;
			continue;
		}
		else

		{
			bu_log( "nmg_model_edge_fuse() WARNING: eu (x%x) has unknown geometry\n" , eu1 );
			continue;
		}

		/* For performance, don't recheck pointers here */
		for( j = i-1; j >= 0; j-- )  {
			register struct edgeuse	*eu2;
			register struct vertex	*v2a, *v2b;
			int eus_are_coincident=0;


			eu2 = (struct edgeuse *)BU_PTBL_GET(&eutab,j);

			if( rt_g.NMG_debug & DEBUG_MESH )
				bu_log( "nmg_mode_edge_fuse: checking eus x%x and x%x\n", eu1, eu2 );

			/* Do this vertex test first, to reduce memory loads */
			v2a = eu2->vu_p->v_p;
			if( v2a != v1a && v2a != v1b )  continue;

			if( e1 == eu2->e_p )  continue;	/* Already shared */

			v2b = eu2->eumate_p->vu_p->v_p;
			if( (v2a != v1a || v2b != v1b) &&
			    (v2b != v1a || v2a != v1b) )
				continue;

			if( !eu2->g.magic_p )
			{
				bu_log( "nmg_model_edge_fuse() WARNING: eu (x%x) has no geometry\n" , eu2 );
				continue;
			}

			/* check if eu2 is a line segment */
			lseg2 = 0;
			if( *eu2->g.magic_p == NMG_EDGE_G_LSEG_MAGIC )
				lseg2 = 1;
			else if( *eu2->g.magic_p == NMG_EDGE_G_CNURB_MAGIC )
				continue;
			else
			{
				bu_log( "nmg_model_edge_fuse() WARNING: eu (x%x) has unknown geometry\n" , eu2 );
				continue;
			}

			/* EU's share endpoints */
			if( lseg1 && lseg2 )
			{
				/* both are line segments */
				nmg_radial_join_eu(eu1, eu2, tol);
			     	total++;
			}
		}

	}
	if( rt_g.NMG_debug & DEBUG_BASIC && total > 0 )
		bu_log("nmg_model_edge_fuse(): %d edges fused\n", total);
	bu_ptbl_free( &eutab);

	if( non_lseg )
		bu_log( "Warning: CNURB edges not fused!!!\n" );

	return total;
}
#else
int
nmg_model_edge_fuse( m, tol )
struct model *m;
struct bn_tol *tol;
{
	struct bu_ptbl edges;
	int i, j;
	int count=0;

	NMG_CK_MODEL( m );
	BN_CK_TOL( tol );

	nmg_edge_tabulate( &edges, &m->magic );

	for( i=0 ; i<BU_PTBL_END( &edges )-1 ; i++ )
	{
		struct edge *e1;
		struct edgeuse *eu1;

		e1 = (struct edge *)BU_PTBL_GET( &edges, i );
		eu1 = e1->eu_p;
		if( !eu1 )
			continue;
		if( *eu1->g.magic_p != NMG_EDGE_G_LSEG_MAGIC )
			continue;

		for( j=i+1 ; j<BU_PTBL_END( &edges ) ; j++ )
		{
			struct edge *e2;
			struct edgeuse *eu2;

			e2 = (struct edge *)BU_PTBL_GET( &edges, j );
			eu2 = e2->eu_p;
			if( !eu2 )
				continue;
			if( *eu2->g.magic_p != NMG_EDGE_G_LSEG_MAGIC )
				continue;

			if( NMG_ARE_EUS_ADJACENT( eu1, eu2 ) )
			{
				count++;
				nmg_radial_join_eu(eu1, eu2, tol);
			}
		}
	}

	bu_ptbl_free( &edges );

	return( count );
}
#endif

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
CONST struct bn_tol	*tol;
{
	struct edge_g_lseg	*eg1;
	struct edge_g_lseg	*eg2;

	NMG_CK_EDGEUSE(eu1);
	NMG_CK_EDGEUSE(eu2);
	BN_CK_TOL(tol);

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
	if( bn_distsq_line3_pt3( eg1->e_pt, eg1->e_dir,
	    eu2->vu_p->v_p->vg_p->coord ) > tol->dist_sq )  goto trouble;
	if( bn_distsq_line3_pt3( eg1->e_pt, eg1->e_dir,
	    eu2->eumate_p->vu_p->v_p->vg_p->coord ) > tol->dist_sq )  goto trouble;

	/* Ensure that vertices of both edges are within tol of other eg */
	if( bn_distsq_line3_pt3( eg2->e_pt, eg2->e_dir,
	    eu1->vu_p->v_p->vg_p->coord ) > tol->dist_sq )  goto trouble;
	if( bn_distsq_line3_pt3( eg2->e_pt, eg2->e_dir,
	    eu1->eumate_p->vu_p->v_p->vg_p->coord ) > tol->dist_sq )  goto trouble;

	/* Perhaps check for ultra-short edges (< 10*tol->dist)? */

	/* Do not use bn_isect_line3_line3() -- it's MUCH too strict */

	return 1;
trouble:
	if( !bn_2line3_colinear( eg1->e_pt, eg1->e_dir, eg2->e_pt, eg2->e_dir,
	     1e5, tol ) )
		return 0;

	/* XXX debug */
	nmg_pr_eg( &eg1->l.magic, 0 );
	nmg_pr_eg( &eg2->l.magic, 0 );
	bu_log("nmg_2edgeuse_g_coincident() lines colinear, vertex check fails, calling colinear anyway.\n");
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
CONST struct bn_tol	*tol;
{
	struct bu_ptbl	etab;
	int		total = 0;
	register int	i,j;

	NMG_CK_MODEL(m);
	BN_CK_TOL(tol);

	/* Make a list of all the edge geometry structs in the model */
	nmg_edge_g_tabulate( &etab, &m->magic );

	for( i = BU_PTBL_END(&etab)-1; i >= 0; i-- )  {
		struct edge_g_lseg		*eg1;
		struct edgeuse			*eu1;

		eg1 = (struct edge_g_lseg *)BU_PTBL_GET(&etab, i);
		NMG_CK_EDGE_G_EITHER(eg1);

		/* XXX Need routine to compare two cnurbs geometricly */
		if( eg1->l.magic == NMG_EDGE_G_CNURB_MAGIC )  {
			continue;
		}

		NMG_CK_EDGE_G_LSEG(eg1);
		eu1 = BU_LIST_MAIN_PTR( edgeuse, BU_LIST_FIRST( rt_list, &eg1->eu_hd2 ), l2 );
		NMG_CK_EDGEUSE(eu1);

		for( j = i-1; j >= 0; j-- )  {
			struct edge_g_lseg	*eg2;
			struct edgeuse		*eu2;

			eg2 = (struct edge_g_lseg *)BU_PTBL_GET(&etab,j);
			NMG_CK_EDGE_G_EITHER(eg2);
			if( eg2->l.magic == NMG_EDGE_G_CNURB_MAGIC )  continue;
			NMG_CK_EDGE_G_LSEG(eg2);
			eu2 = BU_LIST_MAIN_PTR( edgeuse, BU_LIST_FIRST( rt_list, &eg2->eu_hd2 ), l2 );
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
			BU_PTBL_GET(&etab,i) = (long *)NULL;
			break;
		}
	}
	bu_ptbl_free( &etab);
	if( rt_g.NMG_debug & DEBUG_BASIC && total > 0 )
		bu_log("nmg_model_edge_g_fuse(): %d edge_g_lseg's fused\n", total);
	return total;
}

#define TOL_MULTIPLES	1.0
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
 *	0	All is well, or all verts are within TOL_MULTIPLES*tol->dist of fu2
 *	count	Number of verts *not* on fu2's surface when at least one is
 *		more than TOL_MULTIPLES*tol->dist from fu2.
 *
 *  XXX It would be more efficient to use nmg_vist() for this.
 */
int
nmg_ck_fu_verts( fu1, f2, tol )
struct faceuse	*fu1;
struct face	*f2;
CONST struct bn_tol	*tol;
{
	CONST struct face_g_plane	*fg2;
	struct bu_ptbl		vtab;
	FAST fastf_t		dist;
	fastf_t			worst = 0;
	int			k;
	int			count = 0;

	NMG_CK_FACEUSE( fu1 );
	NMG_CK_FACE( f2 );
	BN_CK_TOL(tol);

	fg2 = f2->g.plane_p;
	NMG_CK_FACE_G_PLANE(fg2);

	nmg_vertex_tabulate( &vtab, &fu1->l.magic );

	for( k = BU_PTBL_END(&vtab)-1; k >= 0; k-- )  {
		register struct vertex		*v;
		register struct vertex_g	*vg;

		v = (struct vertex *)BU_PTBL_GET(&vtab, k);
		NMG_CK_VERTEX(v);
		vg = v->vg_p;
		if( !vg )  rt_bomb("nmg_ck_fu_verts(): vertex with no geometry?\n");
		NMG_CK_VERTEX_G(vg);

		/* Geometry check */
		dist = DIST_PT_PLANE(vg->coord, fg2->N);
		if( dist > tol->dist || dist < (-tol->dist) )  {
			if (rt_g.NMG_debug & DEBUG_MESH)  {
				bu_log("nmg_ck_fu_verts(x%x, x%x) v x%x off face by %e\n",
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
	bu_ptbl_free( &vtab);

	if ( count && (rt_g.NMG_debug & DEBUG_BASIC))  {
		bu_log("nmg_ck_fu_verts(fu1=x%x, f2=x%x, tol=%g) f1=x%x, ret=%d, worst=%gmm (%e)\n",
			fu1, f2, tol->dist, fu1->f_p,
			count, worst, worst );
	}

	if( worst > TOL_MULTIPLES*tol->dist )
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
CONST struct bn_tol *tol;
{
	struct face_g_plane *fg1;
	struct faceuse *fu;
	struct face *f;
	int count=0;

	NMG_CK_FACEUSE( fu1 );
	NMG_CK_FACE( f2 );
	BN_CK_TOL( tol );

	NMG_CK_FACE( fu1->f_p );
	fg1 = fu1->f_p->g.plane_p;
	NMG_CK_FACE_G_PLANE( fg1 );

	for( BU_LIST_FOR( f , face , &fg1->f_hd ) )
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
CONST struct bn_tol	*tol;
{
	register struct face_g_plane	*fg1;
	register struct face_g_plane	*fg2;
	int			flip2 = 0;
	int			code;

	NMG_CK_FACE(f1);
	NMG_CK_FACE(f2);
	BN_CK_TOL(tol);

	fg1 = f1->g.plane_p;
	fg2 = f2->g.plane_p;

	if( !fg1 || !fg2 )  {
		if (rt_g.NMG_debug & DEBUG_MESH)  {
			bu_log("nmg_two_face_fuse(x%x, x%x) null fg fg1=x%x, fg2=x%x\n",
				f1, f2, fg1, fg2);
		}
		return 0;
	}

	NMG_CK_FACE_G_PLANE(fg1);
	NMG_CK_FACE_G_PLANE(fg2);

	if( fg1 == fg2 )  {
		if (rt_g.NMG_debug & DEBUG_MESH)  {
			bu_log("nmg_two_face_fuse(x%x, x%x) fg already shared\n",
				f1, f2);
		}
		return 0;	/* Already shared */
	}
#if 0
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
			bu_log("nmg_two_face_fuse(x%x, x%x) faces have a common loop, they MUST be fused.  flip2=%d\n",
				f1, f2, flip2);
		}
		goto must_fuse;
	}

	/* See if faces are coplanar */
	code = bn_coplanar( fg1->N, fg2->N, tol );
	if( code <= 0 )  {
		if (rt_g.NMG_debug & DEBUG_MESH)  {
			bu_log("nmg_two_face_fuse(x%x, x%x) faces non-coplanar\n",
				f1, f2);
		}
		return 0;
	}
	if( code == 1 )
		flip2 = 0;
	else
		flip2 = 1;

	if (rt_g.NMG_debug & DEBUG_MESH)  {
		bu_log("nmg_two_face_fuse(x%x, x%x) coplanar faces, bn_coplanar code=%d, flip2=%d\n",
			f1, f2, code, flip2);
	}
#else
	if( VDOT( fg1->N, fg2->N ) > 0.0 )
		flip2 = 0;
	else
		flip2 = 1;

	if( nmg_ck_fg_verts( f1->fu_p, f2, tol ) != 0 )  {
		if (rt_g.NMG_debug & DEBUG_MESH)  {
			bu_log("nmg_two_face_fuse: f1 verts not within tol of f2's surface, can't fuse\n");
		}
		return 0;
	}
#endif

	/*
	 *  Plane equations match, within tol.
	 *  Before conducting a merge, verify that
	 *  all the verts in f2 are within tol->dist
	 *  of f1's surface.
	 */
	if( nmg_ck_fg_verts( f2->fu_p, f1, tol ) != 0 )  {
		if (rt_g.NMG_debug & DEBUG_MESH)  {
			bu_log("nmg_two_face_fuse: f2 verts not within tol of f1's surface, can't fuse\n");
		}
		return 0;
	}

must_fuse:
	/* All points are on the plane, it's OK to fuse */
	if( flip2 == 0 )  {
		if (rt_g.NMG_debug & DEBUG_MESH)  {
			bu_log("joining face geometry (same dir) f1=x%x, f2=x%x\n", f1, f2);
			PLPRINT(" fg1", fg1->N);
			PLPRINT(" fg2", fg2->N);
		}
		nmg_jfg( f1, f2 );
	} else {
		register struct face	*fn;
		if (rt_g.NMG_debug & DEBUG_MESH)  {
			bu_log("joining face geometry (opposite dirs)\n");

			bu_log(" f1=x%x, flip=%d", f1, f1->flip);
			PLPRINT(" fg1", fg1->N);

			bu_log(" f2=x%x, flip=%d", f2, f2->flip);
			PLPRINT(" fg2", fg2->N);
		}
		/* Flip flags of faces using fg2, first! */
		for( BU_LIST_FOR( fn, face, &fg2->f_hd ) )  {
			NMG_CK_FACE(fn);
			fn->flip = !fn->flip;
			if (rt_g.NMG_debug & DEBUG_MESH)  {
				bu_log("f=x%x, new flip=%d\n", fn, fn->flip);
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
CONST struct bn_tol	*tol;
{
	struct bu_ptbl	ftab;
	int		total = 0;
	register int	i,j;

	NMG_CK_MODEL(m);
	BN_CK_TOL(tol);

	/* Make a list of all the face structs in the model */
	nmg_face_tabulate( &ftab, &m->magic );

	for( i = BU_PTBL_END(&ftab)-1; i >= 0; i-- )  {
		register struct face	*f1;
		register struct face_g_plane	*fg1;
		f1 = (struct face *)BU_PTBL_GET(&ftab, i);
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

			f2 = (struct face *)BU_PTBL_GET(&ftab, j);
			NMG_CK_FACE(f2);
			fg2 = f2->g.plane_p;
			if( !fg2 )  continue;
			NMG_CK_FACE_G_PLANE(fg2);

			if( fg1 == fg2 )  continue;	/* Already shared */

			if( nmg_two_face_fuse( f1, f2, tol ) > 0 )
				total++;
		}
	}
	bu_ptbl_free( &ftab);
	if( rt_g.NMG_debug & DEBUG_BASIC && total > 0 )
		bu_log("nmg_model_face_fuse: %d faces fused\n", total);
	return total;
}

int
nmg_break_all_es_on_v( magic_p, v, tol )
long *magic_p;
struct vertex *v;
CONST struct bn_tol *tol;
{
	struct bu_ptbl eus;
	int i;
	int count=0;
	CONST char *magic_type;

	if( rt_g.NMG_debug & DEBUG_BOOL )
		bu_log( "nmg_break_all_es_on_v( magic=x%x, v=x%x )\n", magic_p, v );

	NMG_CK_VERTEX( v );	
	BN_CK_TOL( tol );

	magic_type = bu_identify_magic( *magic_p );
	if( !strcmp( magic_type, "NULL" ), !strcmp( magic_type, "Unknown_Magic" ) )
	{
		bu_log( "Bad magic pointer passed to nmg_break_all_es_on_v (%s)\n", magic_type );
		rt_bomb( "Bad magic pointer passed to nmg_break_all_es_on_v()\n" );
	}

	nmg_edgeuse_tabulate( &eus, magic_p );

	for( i=0 ; i<BU_PTBL_END( &eus ) ; i++ )
	{
		struct edgeuse *eu;
		struct edgeuse *eu_next, *eu_prev;
		struct vertex *va;
		struct vertex *vb;
		fastf_t dist;
		int code;

		eu = (struct edgeuse *)BU_PTBL_GET( &eus, i );
		NMG_CK_EDGEUSE( eu );

		if( eu->g.magic_p && *eu->g.magic_p == NMG_EDGE_G_CNURB_MAGIC )
			continue;
		va = eu->vu_p->v_p;
		vb = eu->eumate_p->vu_p->v_p;
		NMG_CK_VERTEX(va);
		NMG_CK_VERTEX(vb);

		if( va == v ) continue;
		if( vb == v ) continue;

		eu_next = BU_LIST_PNEXT_CIRC( edgeuse, &eu->l );
		eu_prev = BU_LIST_PPREV_CIRC( edgeuse, &eu->l );

		if( eu_prev->vu_p->v_p == v )
			continue;

		if( eu_next->eumate_p->vu_p->v_p == v )
			continue;

		code = bn_isect_pt_lseg( &dist, va->vg_p->coord, vb->vg_p->coord,
			v->vg_p->coord, tol );
		if( code < 1 )  continue;	/* missed */
		if( code == 1 || code == 2 )  {
			bu_log("nmg_break_all_es_on_v() code=%d, why wasn't this vertex fused?\n", code);
			bu_log( "\teu=x%x, v=x%x\n", eu, v );
			continue;
		}
		/* Break edge on vertex, but don't fuse yet. */

		if( rt_g.NMG_debug & DEBUG_BOOL )
			bu_log( "\tnmg_break_all_es_on_v: breaking eu x%x on v x%x\n", eu, v );

		(void)nmg_ebreak( v, eu );
		count++;
	}
	bu_ptbl_free( &eus);
	return( count );
}

/*
 *			N M G _ M O D E L _ B R E A K _ E _ O N _ V
 *
 *  As the first step in evaluating a boolean formula,
 *  before starting to do face/face intersections, compare every
 *  edge in the model with every vertex in the model.
 *  If the vertex is within tolerance of the edge, break the edge,
 *  and enrole the new edge on a list of edges still to be processed.
 *
 *  A list of edges and a list of vertices are built, and then processed.
 *
 *  Space partitioning could improve the performance of this algorithm.
 *  For the moment, a brute-force approach is used.
 *
 *  Returns -
 *	Number of edges broken.
 */
int
nmg_model_break_e_on_v( m, tol )
struct model			*m;
CONST struct bn_tol		*tol;
{
	int		count = 0;
	struct bu_ptbl	verts;
	struct bu_ptbl	edgeuses;
	struct bu_ptbl	new_edgeuses;
	register struct edgeuse	**eup;

	NMG_CK_MODEL(m);
	BN_CK_TOL(tol);

	nmg_e_and_v_tabulate( &edgeuses, &verts, &m->magic );

	/* Repeat the process until no new edgeuses are created */
	while( BU_PTBL_LEN( &edgeuses ) > 0 )  {
		(void)bu_ptbl_init( &new_edgeuses, 64, " &new_edgeuses");

		for( eup = (struct edgeuse **)BU_PTBL_LASTADDR(&edgeuses);
		     eup >= (struct edgeuse **)BU_PTBL_BASEADDR(&edgeuses);
		     eup--
		)  {
			register struct edgeuse	*eu;
			register struct vertex	*va;
			register struct vertex	*vb;
			register struct vertex	**vp;

			eu = *eup;
			NMG_CK_EDGEUSE(eu);
			if( eu->g.magic_p && *eu->g.magic_p == NMG_EDGE_G_CNURB_MAGIC )
				continue;
			va = eu->vu_p->v_p;
			vb = eu->eumate_p->vu_p->v_p;
			NMG_CK_VERTEX(va);
			NMG_CK_VERTEX(vb);
			for( vp = (struct vertex **)BU_PTBL_LASTADDR(&verts);
			     vp >= (struct vertex **)BU_PTBL_BASEADDR(&verts);
			     vp--
			)  {
				register struct vertex	*v;
				fastf_t			dist;
				int			code;
				struct edgeuse		*new_eu;

				v = *vp;
				NMG_CK_VERTEX(v);
				if( va == v )  continue;
				if( vb == v )  continue;

				/* A good candidate for inline expansion */
				code = bn_isect_pt_lseg( &dist,
					va->vg_p->coord,
					vb->vg_p->coord,
					v->vg_p->coord, tol );
				if( code < 1 )  continue;	/* missed */
				if( code == 1 || code == 2 )  {
					bu_log("nmg_model_break_e_on_v() code=%d, why wasn't this vertex fused?\n", code);
					continue;
				}

				if (rt_g.NMG_debug & (DEBUG_BOOL|DEBUG_BASIC) )
					bu_log( "nmg_model_break_e_on_v(): breaking eu x%x (e=x%x) at vertex x%x\n", eu,eu->e_p, v );

				/* Break edge on vertex, but don't fuse yet. */
				new_eu = nmg_ebreak( v, eu );
				/* Put new edges into follow-on list */
				bu_ptbl_ins( &new_edgeuses, &new_eu->l.magic );

				/* reset vertex vb */
				vb = eu->eumate_p->vu_p->v_p;
				count++;
			}
		}
		bu_ptbl_free( &edgeuses);
		edgeuses = new_edgeuses;		/* struct copy */
	}
	bu_ptbl_free( &edgeuses);
	bu_ptbl_free( &verts);
	if (rt_g.NMG_debug & (DEBUG_BOOL|DEBUG_BASIC) )
		bu_log("nmg_model_break_e_on_v() broke %d edges\n", count);
	return count;
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
CONST struct bn_tol	*tol;
{
	int	total = 0;

	NMG_CK_MODEL(m);
	BN_CK_TOL(tol);

	/* XXXX vertex fusing and edge breaking can produce vertices that are
	 * not within tolerance of their face. Edge breaking needs to be moved
	 * to step 1.5, then a routine to make sure all vertices are within
	 * tolerance of owning face must be called if "total" is greater than zero.
	 * This routine may have to triangulate the face if an appropriate plane
	 * cannot be calculated.
	 */

	/* Step 1 -- the vertices. */
	if( rt_g.NMG_debug & DEBUG_BASIC )
		bu_log( "nmg_model_fuse: vertices\n" );
	total += nmg_model_vertex_fuse( m, tol );

	/* Step 1.5 -- break edges on vertices, before fusing edges */
	if( rt_g.NMG_debug & DEBUG_BASIC )
		bu_log( "nmg_model_fuse: break edges\n" );
	total += nmg_model_break_e_on_v( m, tol );

	if( total )
	{
		struct nmgregion *r;
		struct shell *s;

		/* vertices and/or edges have been moved,
		 * may have created out-of-tolerance faces
		 */

		for( BU_LIST_FOR( r, nmgregion, &m->r_hd ) )
		{
			for( BU_LIST_FOR( s, shell, &r->s_hd ) )
				nmg_make_faces_within_tol( s, tol );
		}
	}

	/* Step 2 -- the face geometry */
	if( rt_g.NMG_debug & DEBUG_BASIC )
		bu_log( "nmg_model_fuse: faces\n" );
	total += nmg_model_face_fuse( m, tol );

	/* Step 3 -- edges */
	if( rt_g.NMG_debug & DEBUG_BASIC )
		bu_log( "nmg_model_fuse: edges\n" );
	total += nmg_model_edge_fuse( m, tol );

	/* Step 4 -- edge geometry */
	if( rt_g.NMG_debug & DEBUG_BASIC )
		bu_log( "nmg_model_fuse: edge geometries\n" );
	total += nmg_model_edge_g_fuse( m, tol );

	if( rt_g.NMG_debug & DEBUG_BASIC && total > 0 )
		bu_log("nmg_model_fuse(): %d entities fused\n", total);
	return total;
}


/* -------------------- RADIAL -------------------- */

struct nmg_radial {
	struct bu_list	l;
	struct edgeuse	*eu;
	struct faceuse	*fu;		/* Derrived from eu */
	struct shell	*s;		/* Derrived from eu */
	int		existing_flag;	/* !0 if this eu exists on dest edge */
	int		is_crack;	/* This eu is part of a crack. */
	int		is_outie;	/* This crack is an "outie" */
	int		needs_flip;	/* Insert eumate, not eu */
	fastf_t		ang;		/* angle, in radians.  0 to 2pi */
};
#define NMG_RADIAL_MAGIC	0x52614421	/* RaD! */
#define NMG_CK_RADIAL(_p)	NMG_CKMAG(_p, NMG_RADIAL_MAGIC, "nmg_radial")

/*
 *			N M G _ R A D I A L _ S O R T E D _ L I S T _ I N S E R T
 *
 *  Build sorted list, with 'ang' running from zero to 2*pi.
 *  New edgeuses with same angle as an edgeuse already on the list
 *  are added AFTER the last existing one, for lack of any better way
 *  to break the tie.
 */
void
nmg_radial_sorted_list_insert( hd, rad )
struct bu_list		*hd;
struct nmg_radial	*rad;
{
	struct nmg_radial	*cur;
	register fastf_t	rad_ang;

	BU_CK_LIST_HEAD(hd);
	NMG_CK_RADIAL(rad);

	if(BU_LIST_IS_EMPTY(hd))  {
		BU_LIST_APPEND( hd, &rad->l );
		return;
	}

	/* Put wires at the front */
	if( rad->fu == (struct faceuse *)NULL )  {
		/* Add before first element */
		BU_LIST_APPEND( hd, &rad->l );
		return;
	}

	rad_ang = rad->ang;

	/*  Check for trivial append at end of list.
	 *  This is a very common case, when input list is sorted.
	 */
	cur = BU_LIST_PREV(nmg_radial, hd);
	if( cur->fu && rad_ang >= cur->ang )  {
		BU_LIST_INSERT( hd, &rad->l );
		return;
	}

	/* Brute force search through hd's list, going backwards */
	for( BU_LIST_FOR_BACKWARDS( cur, nmg_radial, hd ) )  {
		if( cur->fu == (struct faceuse *)NULL )  continue;
		if( rad_ang >= cur->ang )  {
			BU_LIST_APPEND( &cur->l, &rad->l );
			return;
		}
	}

	/* Add before first element */
	BU_LIST_APPEND( hd, &rad->l );
}

/*
 *  Not only verity that list is monotone increasing, but that
 *  pointer integrity still exists.
 */
void
nmg_radial_verify_pointers( hd, tol )
CONST struct bu_list	*hd;
CONST struct bn_tol	*tol;
{
	register struct nmg_radial	*rad;
	register fastf_t		amin = -64;
	register struct nmg_radial	*prev;
	register struct nmg_radial	*next;

	BU_CK_LIST_HEAD(hd);
	BN_CK_TOL(tol);

	/* Verify pointers increasing */
	for( BU_LIST_FOR( rad, nmg_radial, hd ) )  {
		/* Verify pointer integrity */
		prev = BU_LIST_PPREV_CIRC(nmg_radial, rad);
		next = BU_LIST_PNEXT_CIRC(nmg_radial, rad);
		if( rad->eu != prev->eu->radial_p->eumate_p )
			rt_bomb("nmg_radial_verify_pointers() eu not radial+mate forw from prev\n");
		if( rad->eu->eumate_p != prev->eu->radial_p )
			rt_bomb("nmg_radial_verify_pointers() eumate not radial from prev\n");
		if( rad->eu != next->eu->eumate_p->radial_p )
			rt_bomb("nmg_radial_verify_pointers() eu not mate+radial back from next\n");
		if( rad->eu->eumate_p != next->eu->eumate_p->radial_p->eumate_p )
			rt_bomb("nmg_radial_verify_pointers() eumate not mate+radial+mate back from next\n");

		if( rad->fu == (struct faceuse *)NULL )  continue;
		if( rad->ang < amin )  {
			nmg_pr_radial_list( hd, tol );
			bu_log(" previous angle=%g > current=%g\n",
				amin*bn_radtodeg, rad->ang*bn_radtodeg);
			rt_bomb("nmg_radial_verify_pointers() not monotone increasing\n");
		}
		amin = rad->ang;
	}
}

/*
 *
 *  Verify that the angles are monotone increasing.
 *  Wire edgeuses are ignored.
 */
void
nmg_radial_verify_monotone( hd, tol )
CONST struct bu_list	*hd;
CONST struct bn_tol	*tol;
{
	register struct nmg_radial	*rad;
	register fastf_t		amin = -64;

	BU_CK_LIST_HEAD(hd);
	BN_CK_TOL(tol);

	/* Verify monotone increasing */
	for( BU_LIST_FOR( rad, nmg_radial, hd ) )  {
		if( rad->fu == (struct faceuse *)NULL )  continue;
		if( rad->ang < amin )  {
			nmg_pr_radial_list( hd, tol );
			bu_log(" previous angle=%g > current=%g\n",
				amin*bn_radtodeg, rad->ang*bn_radtodeg);
			rt_bomb("nmg_radial_verify_monotone() not monotone increasing\n");
		}
		amin = rad->ang;
	}
}

/*		N M G _ I N S U R E _ L I S T _ I S _ I N C R E A S I N G
 *
 *	Check if the passed rt_list is in increasing order. If not,
 *	reverse the order of the list.
 * XXX Isn't the word "ensure"?
 */
void
nmg_insure_radial_list_is_increasing( hd, amin, amax )
struct bu_list	*hd;
fastf_t amin;
fastf_t amax;
{
	struct nmg_radial *rad;
	fastf_t cur_value=(-MAX_FASTF);
	int increasing=1;

	BU_CK_LIST_HEAD( hd );

	/* if we don't have more than 3 entries, it doesn't matter */

	if( bu_list_len( hd ) < 3 )
		return;

	for( BU_LIST_FOR( rad, nmg_radial, hd ) )
	{
		/* skip wire edges */
		if( rad->fu == (struct faceuse *)NULL )
			continue;

		/* if increasing, just keep checking */
		if( rad->ang >= cur_value )
		{
			cur_value = rad->ang;
			continue;
		}

		/* angle decreases, is it going from max to min?? */
		if( rad->ang == amin && cur_value == amax )
		{
			/* O.K., just went from max to min */
			cur_value = rad->ang;
			continue;
		}

		/* if we get here, this list is not increasing!!! */
		increasing = 0;
		break;
	}

	if( increasing )	/* all is well */
		return;

	/* reverse order of the list */
	bu_list_reverse( hd );

	/* Need to exchange eu with eu->eumate_p for each eu on the list */
	for( BU_LIST_FOR( rad, nmg_radial, hd ) )
	{
		struct faceuse *fu;

		rad->eu = rad->eu->eumate_p;
		rad->fu = nmg_find_fu_of_eu( rad->eu );
	}
}

/*
 *			N M G _ R A D I A L _ B U I L D _ L I S T
 *
 *  The coordinate system is expected to have been chosen in such a
 *  way that the radial list of faces around this edge are circularly
 *  increasing (CCW) in their angle.  Put them in the list in exactly
 *  the order they occur around the edge.  Then, at the end, move the
 *  list head to lie between the maximum and minimum angles, so that the
 *  list head is crossed as the angle goes around through zero.
 *  Now the list is monotone increasing.
 *
 *  The edgeuse's radial pointer takes us in the CCW direction.
 *
 *  If the list contains nmg_radial structures r1, r2, r3, r4,
 *  then going CCW around the edge we will encounter:
 *
 *			(from i-1)			(from i+1)
 *  r1->eu->eumate_p	r4->eu->radial_p		r2->eu->eumate_p->radial_p->eumate_p
 *  r1->eu		r4->eu->radial_p->eumate_p	r2->eu->eumate_p->radial_p
 *  r2->eu->eumate_p	r1->eu->radial_p		r3->eu->eumate_p->radial_p->eumate_p
 *  r2->eu		r1->eu->radial_p->eumate_p	r3->eu->eumate_p->radial_p
 *  r3->eu->eumate_p	r2->eu->radial_p		r4->eu->eumate_p->radial_p->eumate_p
 *  r3->eu		r2->eu->radial_p->eumate_p	r4->eu->eumate_p->radial_p
 *  r4->eu->eumate_p	r3->eu->radial_p		r1->eu->eumate_p->radial_p->eumate_p
 *  r4->eu		r3->eu->radial_p->eumate_p	r1->eu->eumate_p->radial_p
 */
void
nmg_radial_build_list( hd, shell_tbl, existing, eu, xvec, yvec, zvec, tol )
struct bu_list		*hd;
struct bu_ptbl		*shell_tbl;	/* may be null */
int			existing;
struct edgeuse		*eu;
CONST vect_t		xvec;
CONST vect_t		yvec;
CONST vect_t		zvec;
CONST struct bn_tol	*tol;		/* for printing */
{
	fastf_t			angle;
	struct edgeuse		*teu;
	struct nmg_radial	*rad;
	fastf_t			amin;
	fastf_t			amax;
	int			non_wire_edges=0;
	int			edge_count;
	struct nmg_radial	*rmin = (struct nmg_radial *)NULL;
	struct nmg_radial	*rmax = (struct nmg_radial *)NULL;
	struct nmg_radial	*first;

	BU_CK_LIST_HEAD(hd);
	if(shell_tbl) BU_CK_PTBL(shell_tbl);
	NMG_CK_EDGEUSE(eu);
	BN_CK_TOL(tol);

	if( rt_g.NMG_debug & DEBUG_BASIC || rt_g.NMG_debug & DEBUG_MESH_EU )
		bu_log("nmg_radial_build_list( existing=%d, eu=x%x )\n", existing, eu );

	amin = 64;
	amax = -64;

	teu = eu;
	for(;;)  {
		BU_GETSTRUCT( rad, nmg_radial );
		rad->l.magic = NMG_RADIAL_MAGIC;
		rad->eu = teu;
		rad->fu = nmg_find_fu_of_eu( teu );
		if( rad->fu )  {
			/* We depend on ang being strictly in the range 0..2pi */
			rad->ang = nmg_measure_fu_angle( teu, xvec, yvec, zvec );
			non_wire_edges++;

			if( rad->ang < amin )  {
				amin = rad->ang;
				rmin = rad;
			}
			if( rad->ang > amax )  {
				amax = rad->ang;
				rmax = rad;
			}
		} else {
			/* Wire edge.  Set a preposterous angle */
			rad->ang = -bn_pi;	/* -180 */
		}
		rad->s = nmg_find_s_of_eu( teu );
		rad->existing_flag = existing;
		rad->needs_flip = 0;	/* not yet determined */
		rad->is_crack = 0;	/* not yet determined */
		rad->is_outie = 0;	/* not yet determined */

		if( rt_g.NMG_debug & DEBUG_MESH_EU )
			bu_log( "\trad->eu = %x, rad->ang = %g\n", rad->eu, rad->ang );

		/* Just append.  Should already be properly sorted. */
		BU_LIST_INSERT( hd, &(rad->l) );

		/* Add to list of all shells involved at this edge */
		if(shell_tbl) bu_ptbl_ins_unique( shell_tbl, &(rad->s->l.magic) );

		/* Advance to next edgeuse pair */
		teu = teu->radial_p->eumate_p;
		if( teu == eu )  break;
	}

	nmg_insure_radial_list_is_increasing( hd, amin, amax );

	/* Increasing, with min or max value possibly repeated */
	if( !rmin )  {
		/* Nothing but wire edgeuses, done. */
		return;
	}

	/* At least one non-wire edgeuse was found */
	if( non_wire_edges < 2 )
	{
		/* only one non wire entry in list */
		return;
	}

	if( rt_g.NMG_debug & DEBUG_MESH_EU )
	{
		struct nmg_radial *next;

		bu_log("amin=%g min_eu=x%x, amax=%g max_eu=x%x\n",
		rmin->ang * bn_radtodeg, rmin->eu,
		rmax->ang * bn_radtodeg, rmax->eu );

		for( BU_LIST_FOR( next, nmg_radial, hd ) )
			bu_log( "%x: eu=%x, fu=%x, ang=%g\n" , next, next->eu, next->fu, next->ang );
	}

	/* Skip to extremal repeated max&min.  Ignore wires */
	first = rmax;
	for(;;)  {
		struct nmg_radial	*next;
		next = rmax;
		do {
			next = BU_LIST_PNEXT_CIRC(nmg_radial, next);
		} while( next->fu == (struct faceuse *)NULL );
		if( next->ang >= amax )
		{
			rmax = next;		/* a repeated max */
			if( rmax == first )	/* we have gone all the way around (All angles are same ) */
				break;
		}
		else
			break;
	}
	/* wires before min establish new rmin */
	first = rmin;
	for(;;)  {
		struct nmg_radial	*next;

		while( (next = BU_LIST_PPREV_CIRC(nmg_radial, rmin))->fu == (struct faceuse *)NULL )
			rmin = next;
		next = BU_LIST_PPREV_CIRC(nmg_radial, rmin);
		if( next->ang <= amin )
		{
			rmin = next;		/* a repeated min */
			if( rmin == first )	/* all the way round again (All angles are same ) */
			{
				/* set rmin to next entry after rmax */
				rmin = BU_LIST_PNEXT_CIRC( nmg_radial, rmax );
				break;
			}
		}
		else
			break;
	}

	/* Move list head so that it is inbetween min and max entries. */
	if( BU_LIST_PNEXT_CIRC(nmg_radial, rmax) == rmin )  {
		/* Maximum entry is followed by minimum.  Ascending --> CCW */
		BU_LIST_DEQUEUE( hd );
		/* Append head after maximum, before minimum */
		BU_LIST_APPEND( &(rmax->l), hd );
		nmg_radial_verify_pointers( hd, tol );
	} else {
		bu_log("  %f %f %f --- %f %f %f\n",
			V3ARGS(eu->vu_p->v_p->vg_p->coord),
			V3ARGS(eu->eumate_p->vu_p->v_p->vg_p->coord) );
		bu_log("amin=%g min_eu=x%x, amax=%g max_eu=x%x B\n",
			rmin->ang * bn_radtodeg, rmin->eu,
			rmax->ang * bn_radtodeg, rmax->eu );
		nmg_pr_radial_list( hd, tol );
		nmg_pr_fu_around_eu_vecs( eu, xvec, yvec, zvec, tol );
		rt_bomb("nmg_radial_build_list() min and max angle not adjacent in list (or list not monotone increasing)\n");
	}
}

/*
 *			N M G _ R A D I A L _ M E R G E _ L I S T S
 *
 *  Merge all of the src list into the dest list, sorting by angles.
 */
void
nmg_radial_merge_lists( dest, src, tol )
struct bu_list		*dest;
struct bu_list		*src;
CONST struct bn_tol	*tol;
{
	struct nmg_radial	*rad;

	BU_CK_LIST_HEAD(dest);
	BU_CK_LIST_HEAD(src);
	BN_CK_TOL(tol);

	while( BU_LIST_WHILE( rad, nmg_radial, src ) )  {
		BU_LIST_DEQUEUE( &rad->l );
		nmg_radial_sorted_list_insert( dest, rad );
	}
}

/*
 *			N M G _ I S _ C R A C K _ O U T I E
 *
 *  If there is more than one edgeuse of a loopuse along an edge, then
 *  it is a "topological crack".  There are two kinds, an "innie",
 *  where the crack is a null-area incursion into the interior of the loop,
 *  and an "outie", where the crack is a null-area protrusion outside
 *  of the interior of the loop.
 *
 *			 "Outie"		 "Innie"
 *			*-------*		*-------*
 *			|       ^		|       ^
 *			v       |		v       |
 *		*<------*       |		*--->*  |
 *		*---M-->*       |		*<-M-*  |
 *			|       |		|       |
 *			v       |		v       |
 *			*------>*		*------>*
 *
 *  The algorithm used is to compute the geometric midpoint of the crack
 *  edge, "delete" that edge from the loop, and then classify the midpoint
 *  ("M") against the remainder of the loop.  If the edge midpoint
 *  is inside the remains of the loop, then the crack is an "innie",
 *  otherwise it is an "outie".
 *
 *  When there are an odd number of edgeuses along the crack, then the
 *  situation is "nasty":
 *
 *			 "Nasty"
 *			*-------*
 *			|       ^
 *			v       |
 *		*<------*       |
 *		*------>*       |
 *		*<------*       |
 *		*------>*       |
 *		*<------*       |
 *		|		|
 *		|		|
 *		v		|
 *		*------------->*
 *
 *  The caller is responsible for making sure that the edgeuse is not
 *  a wire edgeuse (i.e. that the edgeuse is part of a loop).
 *
 *  In the "Nasty" case, all the edgeuse pairs are "outies" except for
 *  the last lone edgeuse, which should be handled as a "non-crack".
 *  Walk the loopuse's edgeuses list in edgeuse order to see which one
 *  is the last (non-crack) repeated edgeuse.
 *  For efficiency, detecting and dealing with this condition is left
 *  up to the caller, and is not checked for here.
 */
int
nmg_is_crack_outie( eu, tol )
CONST struct edgeuse	*eu;
CONST struct bn_tol	*tol;
{
	CONST struct loopuse	*lu;
	CONST struct edge	*e;
	point_t			midpt;
	vect_t			diff;
	fastf_t			diff_len;
	CONST fastf_t		*a, *b;
	int			class;

	NMG_CK_EDGEUSE(eu);
	BN_CK_TOL(tol);

	lu = eu->up.lu_p;
	NMG_CK_LOOPUSE(lu);
	e = eu->e_p;
	NMG_CK_EDGE(e);

	/* If ENTIRE loop is a crack, there is no surface area, it's an outie */
	if( nmg_loop_is_a_crack( lu ) )  return 1;

	a = eu->vu_p->v_p->vg_p->coord;
	b = eu->eumate_p->vu_p->v_p->vg_p->coord;
	VADD2SCALE( midpt, a, b, 0.5 );

	/* Ensure edge is long enough so midpoint is not within tol of verts */
	VSUB2( diff, a, b );
	diff_len = MAGNITUDE(diff);
	{
#if 1
		/* all we want here is a classification of the midpoint,
		 * so let's create a temporary tolerance that will work!!! */

		struct bn_tol tmp_tol;
		struct faceuse		*fu;
		plane_t			pl;
		fastf_t			dist;

		tmp_tol = (*tol);
		if( *lu->up.magic_p != NMG_FACEUSE_MAGIC )
			rt_bomb( "Nmg_is_crack_outie called with non-face loop" );

		fu = lu->up.fu_p;
		NMG_CK_FACEUSE( fu );
		NMG_GET_FU_PLANE( pl, fu );
		dist = DIST_PT_PLANE( midpt, pl );
		VJOIN1( midpt, midpt , -dist, pl )
		dist = fabs( DIST_PT_PLANE( midpt, pl ) );
		if( dist > SQRT_SMALL_FASTF )
		{
			tmp_tol.dist = dist*2.0;
			tmp_tol.dist_sq = tmp_tol.dist * tmp_tol.dist;
		}
		else
		{
			tmp_tol.dist = SQRT_SMALL_FASTF;
			tmp_tol.dist_sq = SMALL_FASTF;
		}
		class = nmg_class_pt_lu_except( midpt, lu, e, &tmp_tol );
#else
		rt_pr_tol(tol);
		bu_log(" eu=x%x, len=%g\n", eu, MAGNITUDE(diff) );
		rt_bomb("nmg_is_crack_outie() edge is too short to bisect.  Increase tolerance and re-run.\n");
#endif
	}
	if( rt_g.NMG_debug & DEBUG_BASIC )  {
		bu_log("nmg_is_crack_outie(eu=x%x) lu=x%x, e=x%x, class=%s\n",
			eu, lu, e, nmg_class_name(class) );
	}

	if( lu->orientation == OT_SAME )  {
		if( class == NMG_CLASS_AinB || class == NMG_CLASS_AonBshared )
			return 0;		/* an "innie" */
		if( class == NMG_CLASS_AoutB )
			return 1;		/* an "outie" */
	} else {
		/* It's a hole loop, things work backwards. */
		if( class == NMG_CLASS_AinB || class == NMG_CLASS_AonBshared )
			return 1;		/* an "outie" */
		if( class == NMG_CLASS_AoutB )
			return 0;		/* an "innie" */
	}

	/* Other classifications "shouldn't happen". */
	bu_log("nmg_is_crack_outie(eu=x%x), lu=x%x(%s)\n  midpt_class=%s, midpt=(%g, %g, %g)\n",
		eu,
		lu, nmg_orientation(lu->orientation),
		nmg_class_name(class),
		V3ARGS(midpt) );
	nmg_pr_lu_briefly( lu, 0 );
	rt_bomb("nmg_is_crack_outie() got unexpected midpt classification from nmg_class_pt_lu_except()\n");

	return( -1 ); /* make the compiler happy */
}

/*
 *			N M G _ F I N D _ R A D I A L _ E U
 */
struct nmg_radial *
nmg_find_radial_eu( hd, eu )
CONST struct bu_list	*hd;
CONST struct edgeuse	*eu;
{
	register struct nmg_radial	*rad;

	BU_CK_LIST_HEAD(hd);
	NMG_CK_EDGEUSE(eu);

	for( BU_LIST_FOR( rad, nmg_radial, hd ) )  {
		if( rad->eu == eu )  return rad;
		if( rad->eu->eumate_p == eu )  return rad;
	}
	bu_log("nmg_find_radial_eu() eu=x%x\n", eu);
	rt_bomb("nmg_find_radial_eu() given edgeuse not found on list\n");

	return( (struct nmg_radial *)NULL );
}

/*
 *			N M G _ F I N D _ N E X T _ U S E _ O F _ 2 E _ I N _ L U
 *
 *  Find the next use of either of two edges in the loopuse.
 *  The second edge pointer may be NULL.
 */
CONST struct edgeuse *
nmg_find_next_use_of_2e_in_lu( eu, e1, e2 )
CONST struct edgeuse	*eu;
CONST struct edge	*e1;
CONST struct edge	*e2;		/* may be NULL */
{
	register CONST struct edgeuse	*neu;
	register CONST struct edge	*e;

	NMG_CK_EDGEUSE(eu);
	NMG_CK_LOOPUSE(eu->up.lu_p);	/* sanity */
	NMG_CK_EDGE(e1);
	if(e2) NMG_CK_EDGE(e2);

	neu = eu;
	do  {
		neu = BU_LIST_PNEXT_CIRC( edgeuse, &neu->l );
	} while( neu->e_p != e1 && neu->e_p != e2 );
	return neu;

}

/*
 *			N M G _ R A D I A L _ M A R K _ C R A C K S
 *
 *  For every edgeuse, if there are other edgeuses around this edge
 *  from the same face, then mark them all as part of a "crack".
 *
 *  To be a crack the two edgeuses must be from the same loopuse.
 *
 *  If the count of repeated ("crack") edgeuses is even, then
 *  classify the entire crack as an "innie" or an "outie".
 *  If the count is odd, this is a "Nasty" --
 *  all but one edgeuse are marked as "outies",
 *  and the remaining one is marked as a non-crack.
 *  The "outie" edgeuses are marked off in pairs,
 *  in the loopuses's edgeuse order.
 */
void
nmg_radial_mark_cracks( hd, e1, e2, tol )
struct bu_list		*hd;
CONST struct edge	*e1;
CONST struct edge	*e2;		/* may be NULL */
CONST struct bn_tol	*tol;
{
	struct nmg_radial	*rad;
	struct nmg_radial	*other;
	CONST struct loopuse	*lu;
	CONST struct edgeuse	*eu;
	register int		uses;
	int			outie;

	BU_CK_LIST_HEAD(hd);
	NMG_CK_EDGE(e1);
	if(e2) NMG_CK_EDGE(e2);
	BN_CK_TOL(tol);

	for( BU_LIST_FOR( rad, nmg_radial, hd ) )  {
		NMG_CK_RADIAL(rad);
		if( rad->is_crack )  continue;
		if( !rad->fu ) continue;		/* skip wire edges */
		lu = rad->eu->up.lu_p;
		uses = 0;

		/* Search the remainder of the list for other uses */
		for( other = BU_LIST_PNEXT( nmg_radial, rad );
		     BU_LIST_NOT_HEAD( other, hd );
		     other = BU_LIST_PNEXT( nmg_radial, other )
		)  {
			if( !other->fu ) continue;	/* skip wire edges */
			/* Only consider edgeuses from the same loopuse */
			if( other->eu->up.lu_p != lu &&
			    other->eu->eumate_p->up.lu_p != lu )
				continue;
			uses++;
		}
		if( uses <= 0 )  {
			/* The main search continues to end of list */
			continue;		/* not a crack */
		}
		uses++;		/* account for first use too */

		/* OK, we have a crack. Which kind? */
		if( (uses & 1) == 0 )  {
			/* Even number of edgeuses. */
			outie = nmg_is_crack_outie( rad->eu, tol );
			rad->is_crack = 1;
			rad->is_outie = outie;
			if (rt_g.NMG_debug & DEBUG_MESH_EU )  {
				bu_log( "nmg_radial_mark_cracks() EVEN crack eu=x%x, uses=%d, outie=%d\n",
					rad->eu, uses, outie );
			}
			/* Mark all the rest of them the same way */
			for( other = BU_LIST_PNEXT( nmg_radial, rad );
			     BU_LIST_NOT_HEAD( other, hd );
			     other = BU_LIST_PNEXT( nmg_radial, other )
			)  {
				if( !other->fu ) continue;	/* skip wire edges */
				/* Only consider edgeuses from the same loopuse */
				if( other->eu->up.lu_p != lu &&
				    other->eu->eumate_p->up.lu_p != lu )
					continue;
				other->is_crack = 1;
				other->is_outie = outie;
			}
			if (rt_g.NMG_debug & DEBUG_MESH_EU )  {
				bu_log("Printing loopuse and resulting radial list:\n");
				nmg_pr_lu_briefly( lu, 0 );
				nmg_pr_radial_list( hd, tol );
			}
			continue;
		}
		/*
		 *  Odd number of edgeuses.  Traverse in loopuse order.
		 *  All but the last one are "outies", last one is "innie"
		 */
		if (rt_g.NMG_debug & DEBUG_MESH_EU )  {
			bu_log( "nmg_radial_mark_cracks() ODD crack eu=x%x, uses=%d, outie=%d\n",
				rad->eu, uses, outie );
		}
		/* Mark off pairs of edgeuses, one per trip through loop. */
		eu = rad->eu;
		for( ; uses >= 2; uses-- )  {
			eu = nmg_find_next_use_of_2e_in_lu( eu, e1, e2 );
			if (rt_g.NMG_debug & DEBUG_MESH_EU )  {
				bu_log("rad->eu=x%x, eu=x%x, uses=%d\n",
					rad->eu, eu, uses);
			}
			if( eu == rad->eu )  {
				nmg_pr_lu_briefly( lu, 0 );
				nmg_pr_radial_list( hd, tol );
				rt_bomb("nmg_radial_mark_cracks() loop too short!\n");
			}

			other = nmg_find_radial_eu( hd, eu );
			/* Mark 'em as "outies" */
			other->is_crack = 1;
			other->is_outie = 1;
		}

		/* Should only be one left, this one is an "innie":  it borders surface area */
		eu = nmg_find_next_use_of_2e_in_lu( eu, e1, e2 );
		if( eu != rad->eu )  {
			nmg_pr_lu_briefly( lu, 0 );
			nmg_pr_radial_list( hd, tol );
			rt_bomb("nmg_radial_mark_cracks() loop didn't return to start\n");
		}

		rad->is_crack = 1;
		rad->is_outie = 0;		/* "innie" */
	}
}

/*
 *			N M G _ R A D I A L _ F I N D _ A N _ O R I G I N A L
 *
 *  Returns -
 *	NULL		No edgeuses from indicated shell on this list
 *	nmg_radial*	An original, else first newbie, else a newbie crack.
 */
struct nmg_radial *
nmg_radial_find_an_original( hd, s, tol )
struct bu_list		*hd;
CONST struct shell	*s;
CONST struct bn_tol	*tol;
{
	register struct nmg_radial	*rad;
	struct nmg_radial	*fallback = (struct nmg_radial *)NULL;
	struct nmg_radial	*wire = (struct nmg_radial *)NULL;
	int			seen_shell = 0;

	BU_CK_LIST_HEAD(hd);
	NMG_CK_SHELL(s);

	/* First choice:  find an original, non-crack, non-wire */
	for( BU_LIST_FOR( rad, nmg_radial, hd ) )  {
		NMG_CK_RADIAL(rad);
		if( rad->s != s )  continue;
		seen_shell++;
		if( rad->is_outie )  {
			fallback = rad;
			continue;	/* skip "outie" cracks */
		}
		if( !rad->fu )  continue;	/* skip wires */
		if( rad->existing_flag )  return rad;
	}
	if( !seen_shell )  return (struct nmg_radial *)NULL;	/* No edgeuses from that shell, at all! */

	/* Next, an original crack would be OK */
	if(fallback) return fallback;

	/* If there were no originals, find first newbie */
	for( BU_LIST_FOR( rad, nmg_radial, hd ) )  {
		if( rad->s != s )  continue;
		if( rad->is_outie )  {
			fallback = rad;
			continue;	/* skip "outie" cracks */
		}
		if( !rad->fu )  {
			continue;	/* skip wires */
		}
		return rad;
	}
	/* If no ordinary newbies, provide a newbie crack */
	if(fallback) return fallback;

	/* No ordinary newbiew or newbie cracks, any wires? */
	for( BU_LIST_FOR( rad, nmg_radial, hd ) )  {
		if( rad->s != s )  continue;
		if( rad->is_outie )  {
			continue;	/* skip "outie" cracks */
		}
		if( !rad->fu )  {
			fallback = rad;
			continue;	/* skip wires */
		}
		return rad;
	}
	if(fallback) return fallback;

	bu_log("nmg_radial_find_an_original() shell=x%x\n", s);
	nmg_pr_radial_list( hd, tol );
	rt_bomb("nmg_radial_find_an_original() No entries from indicated shell\n");

	return( (struct nmg_radial *)NULL );
}

/*
 *			N M G _ R A D I A L _ M A R K _ F L I P S
 *
 *  For a given shell, find an original edgeuse from that shell,
 *  and then mark parity violators with a "flip" flag.
 */
int
nmg_radial_mark_flips( hd, s, tol )
struct bu_list		*hd;
CONST struct shell	*s;
CONST struct bn_tol	*tol;
{
	struct nmg_radial	*rad;
	struct nmg_radial	*orig;
	register int		expected_ot;
	int			count = 0;
	int			nflip = 0;

	BU_CK_LIST_HEAD(hd);
	NMG_CK_SHELL(s);
	BN_CK_TOL(tol);

	orig = nmg_radial_find_an_original( hd, s, tol );
	NMG_CK_RADIAL(orig);
	if( orig->is_outie )  {
		/* Only originals were "outie" cracks.  No flipping */
		return 0;
	}

	if( !orig->fu )
	{
		/* nothing but wires */
		return( 0 );
	}
	if( !orig->existing_flag )  {
		/* There were no originals.  Do something sensible to check the newbies */
		if( !orig->fu )  {
			/* Nothing but wires */
			return 0;
		}
#if 0
		/*  Given that there were no existing edgeuses
		 *  from this shell, make our first addition
		 *  oppose the previous radial faceuse, just to be nice.
		 */
		rad = BU_LIST_PPREV_CIRC( nmg_radial, orig );
		/* Hopefully it isn't a wire */
		if( rad->eu->vu_p->v_p == orig->eu->vu_p->v_p )  {
			/* Flip everything, for general compatibility */
			for( BU_LIST_FOR( rad, nmg_radial, hd ) )  {
				if( rad->s != s )  continue;
				if( !rad->fu )  continue;	/* skip wires */
				rad->needs_flip = !rad->needs_flip;
			}
		}
#endif
	}

	expected_ot = !(orig->fu->orientation == OT_SAME);
	if( !orig->is_outie ) count++;	/* Don't count orig if "outie" crack */

	for( BU_LIST_FOR_CIRC( rad, nmg_radial, orig ) )  {
		if( rad->s != s )  continue;
		if( !rad->fu )  continue;	/* skip wires */
		if( rad->is_outie ) continue;	/* skip "outie" cracks */
		count++;
		if( expected_ot == (rad->fu->orientation == OT_SAME) )  {
			expected_ot = !expected_ot;
			continue;
		}
		/* Mis-match detected */
		if (rt_g.NMG_debug & DEBUG_MESH_EU ) {
			bu_log("nmg_radial_mark_flips() Mis-match detected, setting flip flag eu=x%x\n", rad->eu);
		}
		rad->needs_flip = !rad->needs_flip;
		nflip++;
		/* With this one flipped, set expectation for next */
		expected_ot = !expected_ot;
	}

	if( count & 1 )  {
#if 0
		bu_log("nmg_radial_mark_flips() NOTICE dangling fu=x%x detected at eu=x%x in shell=x%x, proceeding.\n  %g %g %g --- %g %g %g\n",
			orig->fu, orig->eu, s,
			V3ARGS(orig->eu->vu_p->v_p->vg_p->coord),
			V3ARGS(orig->eu->eumate_p->vu_p->v_p->vg_p->coord)
		);
		nmg_pr_radial_list( hd, tol );
#endif
		return count;
	}

	if( expected_ot == (orig->fu->orientation == OT_SAME) )
		return nflip;

	bu_log("nmg_radial_mark_flips() unable to establish proper orientation parity.\n  eu count=%d, shell=x%x, expectation=%d\n",
		count, s, expected_ot);
	nmg_pr_radial_list( hd, tol );
	rt_bomb("nmg_radial_mark_flips() unable to establish proper orientation parity.\n");

	return( 0 ); /* for compiler */
}

/*
 *			N M G _ R A D I A L _ C H E C K _ P A R I T Y
 *
 *  For each shell, check orientation parity of edgeuses within that shell.
 */
int
nmg_radial_check_parity( hd, shells, tol )
CONST struct bu_list	*hd;
CONST struct bu_ptbl	*shells;
CONST struct bn_tol	*tol;
{
	struct nmg_radial	*rad;
	struct nmg_radial	*other;
	struct shell		**sp;
	struct nmg_radial	*orig;
	register int		expected_ot;
	int			count = 0;

	BU_CK_LIST_HEAD(hd);
	BU_CK_PTBL(shells);
	BN_CK_TOL(tol);

	for( sp = (struct shell **)BU_PTBL_LASTADDR(shells);
 	     sp >= (struct shell **)BU_PTBL_BASEADDR(shells); sp-- 
	)  {

		NMG_CK_SHELL(*sp);
		orig = nmg_radial_find_an_original( hd, *sp, tol );
		if( !orig )  continue;
		NMG_CK_RADIAL(orig);
		if( !orig->existing_flag )  {
			/* There were no originals.  Do something sensible to check the newbies */
			if( !orig->fu )  continue;	/* Nothing but wires */
		}
		if( orig->is_outie )  continue;	/* Loop was nothing but outies */
		expected_ot = !(orig->fu->orientation == OT_SAME);

		for( BU_LIST_FOR_CIRC( rad, nmg_radial, orig ) )  {
			if( rad->s != *sp )  continue;
			if( !rad->fu )  continue;	/* skip wires */
			if( rad->is_outie ) continue;	/* skip "outie" cracks */
			if( expected_ot == (rad->fu->orientation == OT_SAME) )  {
				expected_ot = !expected_ot;
				continue;
			}
			/* Mis-match detected */
			bu_log("nmg_radial_check_parity() bad parity eu=x%x, s=x%x\n",
				rad->eu, *sp);
			count++;
			/* Set expectation for next */
			expected_ot = !expected_ot;
		}
		if( expected_ot == (orig->fu->orientation == OT_SAME) )
			continue;
		bu_log("nmg_radial_check_parity() bad parity at END eu=x%x, s=x%x\n",
			rad->eu, *sp);
		count++;
 	}
	return count;
}

/*
 *			N M G _ R A D I A L _ I M P L E M E N T _ D E C I S I O N S
 *
 *  For all non-original edgeuses in the list, place them in the proper
 *  place around the destination edge.
 */
void
nmg_radial_implement_decisions( hd, tol, eu1, xvec, yvec, zvec )
struct bu_list		*hd;
CONST struct bn_tol	*tol;		/* for printing */
struct edgeuse		*eu1;	/* temp */
vect_t			xvec, yvec, zvec;	/*** temp ***/
{
	struct nmg_radial	*rad;
	struct nmg_radial	*prev;
	int			skipped;

	BU_CK_LIST_HEAD(hd);
	BN_CK_TOL(tol);

	if( rt_g.NMG_debug & DEBUG_BASIC )
		bu_log("nmg_radial_implement_decisions() BEGIN\n");

again:
	skipped = 0;
	for( BU_LIST_FOR( rad, nmg_radial, hd ) )  {
		struct edgeuse	*dest;
		struct edgeuse	*src;

		if( rad->existing_flag )  continue;
		prev = BU_LIST_PPREV_CIRC( nmg_radial, rad );
		if( !prev->existing_flag )  {
			/* Previous eu isn't in place yet, can't do this one until next pass. */
			skipped++;
			continue;
		}

		/*
		 *  Insert "rad" CCW radial from "prev".
		 *
		 */
		if (rt_g.NMG_debug & DEBUG_MESH_EU ) {
			bu_log("Before -- ");
			nmg_pr_fu_around_eu_vecs( eu1, xvec, yvec, zvec, tol );
			nmg_pr_radial("prev:", (CONST struct nmg_radial *)prev);
			nmg_pr_radial(" rad:", (CONST struct nmg_radial *)rad);
		}
		dest = prev->eu;
		if( rad->needs_flip )  {
			struct edgeuse	*other_eu = rad->eu->eumate_p;

			nmg_je( dest, rad->eu );
			rad->eu = other_eu;
			rad->fu = nmg_find_fu_of_eu( other_eu );
			rad->needs_flip = 0;
		} else {
			nmg_je( dest, rad->eu->eumate_p );
		}
		rad->existing_flag = 1;
		if (rt_g.NMG_debug & DEBUG_MESH_EU ) {
			bu_log("After -- ");
			nmg_pr_fu_around_eu_vecs( eu1, xvec, yvec, zvec, tol );
		}
	}
	if( skipped )  {
		if( rt_g.NMG_debug & DEBUG_BASIC )
			bu_log("nmg_radial_implement_decisions() %d remaining, go again\n", skipped);
		goto again;
	}

	if( rt_g.NMG_debug & DEBUG_BASIC )
		bu_log("nmg_radial_implement_decisions() END\n");
}

/*
 *			N M G _ P R _ R A D I A L
 */
void
nmg_pr_radial( title, rad )
CONST char		*title;
CONST struct nmg_radial	*rad;
{
	struct face		*f;
	int			orient;

	NMG_CK_RADIAL(rad);
	if( !rad->fu )  {
		f = (struct face *)NULL;
		orient = 'W';
	} else {
		f = rad->fu->f_p;
		orient = nmg_orientation(rad->fu->orientation)[3];
	}
	bu_log("%s%8.8x, mate of \\/\n",
		title,
		rad->eu->eumate_p
	);
	bu_log("%s%8.8x, f=%8.8x, fu=%8.8x=%c, s=%8.8x %s %c%c%c %g deg\n",
		title,
		rad->eu,
		f, rad->fu, orient,
		rad->s,
		rad->existing_flag ? "old" : "new",
		rad->needs_flip ? 'F' : '/',
		rad->is_crack ? 'C' : '/',
		rad->is_outie ? 'O' : (rad->is_crack ? 'I' : '/'),
		rad->ang * bn_radtodeg
	);
}

/*
 *			N M G _ P R _ R A D I A L _ L I S T
 *
 *  Patterned after nmg_pr_fu_around_eu_vecs(), with similar format.
 */
void
nmg_pr_radial_list( hd, tol )
CONST struct bu_list	*hd;
CONST struct bn_tol	*tol;		/* for printing */
{
	struct nmg_radial	*rad;

	BU_CK_LIST_HEAD(hd);
	BN_CK_TOL(tol);

	bu_log("nmg_pr_radial_list( hd=x%x )\n", hd);

	for( BU_LIST_FOR( rad, nmg_radial, hd ) )  {
		NMG_CK_RADIAL(rad);
		nmg_pr_radial(" ", rad);
	}
}


/*		N M G _ D O _ R A D I A L _ F L I P S
 *
 *	This routine looks for nmg_radial structures with the same angle,
 *	and sorts them to match the order of nmg_radial structures that
 *	are not at that same angle
 */
void
nmg_do_radial_flips( hd )
struct bu_list *hd;
{
	struct nmg_radial	*start_same;
	struct bn_tol	tol;

		tol.magic = BN_TOL_MAGIC;
		tol.dist = 0.005;
		tol.dist_sq = tol.dist * tol.dist;
		tol.perp = 1e-6;
		tol.para = 1 - tol.perp;

	BU_CK_LIST_HEAD( hd );

	start_same = BU_LIST_FIRST( nmg_radial, hd );
	while( BU_LIST_NOT_HEAD( &start_same->l, hd ) )
	{
		struct nmg_radial	*next_after_same;
		struct nmg_radial	*end_same;
		struct nmg_radial	*same;
		struct nmg_radial	*check;

		if( !start_same->fu )
		{
			start_same = BU_LIST_PNEXT( nmg_radial, &start_same->l );
			continue;
		}

		end_same = BU_LIST_PNEXT_CIRC( nmg_radial, &start_same->l );
		while( (end_same->ang == start_same->ang && end_same != start_same)
			|| !end_same->fu )
			end_same = BU_LIST_PNEXT_CIRC( nmg_radial, &end_same->l );
		end_same = BU_LIST_PPREV_CIRC( nmg_radial, &end_same->l );

		if( start_same == end_same )
		{
			start_same = BU_LIST_PNEXT( nmg_radial, &start_same->l );
			continue;
		}

		/* more than one eu at same angle, sort them according to shell */
		next_after_same = BU_LIST_PNEXT_CIRC( nmg_radial, &end_same->l );
		while( !next_after_same->fu && next_after_same != start_same )
			next_after_same = BU_LIST_PNEXT_CIRC( nmg_radial, &next_after_same->l );

		if( next_after_same == start_same )
		{
			/* no other radials with faces */
			return;
		}

		check = next_after_same;
		while( start_same != end_same && check != start_same )
		{
			same = end_same;
			while( same->s != check->s && same != start_same )
				same = BU_LIST_PPREV_CIRC( nmg_radial, &same->l );

			if( same->s != check->s )
			{
				/* couldn't find any other radial from shell "same->s"
				 * so put look at next radial
				 */

				check = BU_LIST_PNEXT_CIRC( nmg_radial, &check->l );
				continue;
			}

			/* same->s matches check->s, so move "same" to right after end_same */
			if( same == start_same )
			{
				/* starting radial matches, need to move it and
				 * set pointer to new start_same
				 */
				start_same = BU_LIST_PNEXT_CIRC( nmg_radial, &start_same->l );
				BU_LIST_DEQUEUE( &same->l );
				BU_LIST_APPEND( &end_same->l, &same->l );
			}
			else if( same == end_same )
			{
				/* already in correct place, just move end_same */
				end_same = BU_LIST_PPREV_CIRC( nmg_radial, &end_same->l );
			}
			else
			{
				BU_LIST_DEQUEUE( &same->l );
				BU_LIST_APPEND( &end_same->l, &same->l );
			}

			check = BU_LIST_PNEXT_CIRC( nmg_radial, &check->l );
		}

		start_same = BU_LIST_PNEXT( nmg_radial, &end_same->l );
	}
}

/*              N M G _ D O _ R A D I A L _ J O I N
 *
 *      Perform radial join of edges in list "hd" based on direction with respect
 *      to "eu1ref"
 */

void
nmg_do_radial_join( hd, eu1ref, xvec, yvec, zvec, tol )
struct bu_list *hd;
struct edgeuse *eu1ref;
vect_t xvec, yvec, zvec;
CONST struct bn_tol *tol;
{
	struct nmg_radial	*rad;
	struct nmg_radial	*prev;
	vect_t			ref_dir;
	int			skipped;

	BU_CK_LIST_HEAD( hd );
	NMG_CK_EDGEUSE( eu1ref );
	BN_CK_TOL( tol );

	if (rt_g.NMG_debug & DEBUG_MESH_EU )
		bu_log( "nmg_do_radial_join() START\n" );

	nmg_do_radial_flips( hd );

	VSUB2( ref_dir, eu1ref->eumate_p->vu_p->v_p->vg_p->coord, eu1ref->vu_p->v_p->vg_p->coord );

	if (rt_g.NMG_debug & DEBUG_MESH_EU )
		bu_log( "ref_dir = ( %g %g %g )\n", V3ARGS( ref_dir ) );

top:

	if (rt_g.NMG_debug & DEBUG_MESH_EU )
	{
		bu_log( "At top of nmg_do_radial_join:\n" );
		nmg_pr_radial_list( hd, tol );
	}

	skipped = 0;
	for( BU_LIST_FOR( rad, nmg_radial, hd ) )
	{
		struct edgeuse *dest;
		struct edgeuse *src;
		vect_t src_dir;
		vect_t dest_dir;

		if( rad->existing_flag )
			continue;

		prev = BU_LIST_PPREV_CIRC( nmg_radial, rad );
		if( !prev->existing_flag )
		{
			skipped++;
			continue;
		}

		VSUB2( dest_dir, prev->eu->eumate_p->vu_p->v_p->vg_p->coord, prev->eu->vu_p->v_p->vg_p->coord );
		VSUB2( src_dir, rad->eu->eumate_p->vu_p->v_p->vg_p->coord, rad->eu->vu_p->v_p->vg_p->coord );

		if( !prev->fu || !rad->fu )
		{
			nmg_je( prev->eu, rad->eu );
			continue;
		}

		if( VDOT( dest_dir, ref_dir ) < 0.0 )
			dest = prev->eu->eumate_p;
		else
			dest = prev->eu;

		if( VDOT( src_dir, ref_dir ) > 0.0 )
			src = rad->eu->eumate_p;
		else
			src = rad->eu;

		if (rt_g.NMG_debug & DEBUG_MESH_EU )
		{
			bu_log("Before -- ");
			nmg_pr_fu_around_eu_vecs( eu1ref, xvec, yvec, zvec, tol );
			nmg_pr_radial("prev:", prev);
			nmg_pr_radial(" rad:", rad);

			if( VDOT( dest_dir, ref_dir ) < 0.0 )
				bu_log( "dest_dir disagrees with eu1ref\n" );
			else
				bu_log( "dest_dir agrees with eu1ref\n" );
			
			if( VDOT( src_dir, ref_dir ) < 0.0 )
				bu_log( "src_dir disagrees with eu1ref\n" );
			else
				bu_log( "src_dir agrees with eu1ref\n" );

			bu_log( "Joining dest_eu=x%x to src_eu=x%x\n", dest, src );
		}

		nmg_je( dest, src );
		rad->existing_flag = 1;
		if (rt_g.NMG_debug & DEBUG_MESH_EU )
		{
			bu_log("After -- ");
			nmg_pr_fu_around_eu_vecs( eu1ref, xvec, yvec, zvec, tol );
		}
	}

	if( skipped )
		goto top;

	if (rt_g.NMG_debug & DEBUG_MESH_EU )
		bu_log( "nmg_do_radial_join() DONE\n\n" );
}

/*
 *			N M G _ R A D I A L _ J O I N _ E U
 *
 *  A new routine, that uses "global information" about the edge
 *  to plan the operations to be performed.
 */
void
nmg_radial_join_eu_NEW(eu1, eu2, tol)
struct edgeuse		*eu1;
struct edgeuse		*eu2;
CONST struct bn_tol	*tol;
{
	struct edgeuse		*eu1ref;		/* reference eu for eu1 */
	struct edgeuse		*eu2ref;
	struct edge_g_lseg	*best_eg;
	struct faceuse		*fu1;
	struct faceuse		*fu2;
	struct nmg_radial	*rad;
	vect_t			xvec, yvec, zvec;
	struct bu_list		list1;
	struct bu_list		list2;
	struct bu_ptbl		shell_tbl;
	struct shell		**sp;
	int			count1, count2;

	NMG_CK_EDGEUSE(eu1);
	NMG_CK_EDGEUSE(eu2);
	BN_CK_TOL(tol);

	if( eu1->e_p == eu2->e_p )  return;

	if( !NMG_ARE_EUS_ADJACENT(eu1, eu2) )
		rt_bomb("nmg_radial_join_eu_NEW() edgeuses don't share vertices.\n");

	if( eu1->vu_p->v_p == eu1->eumate_p->vu_p->v_p )  rt_bomb("nmg_radial_join_eu_NEW(): 0 length edge (topology)\n");

	if( bn_pt3_pt3_equal( eu1->vu_p->v_p->vg_p->coord,
	    eu1->eumate_p->vu_p->v_p->vg_p->coord, tol ) )
		rt_bomb("nmg_radial_join_eu_NEW(): 0 length edge (geometry)\n");

	/* Ensure faces are of same orientation, if both eu's have faces */
	fu1 = nmg_find_fu_of_eu(eu1);
	fu2 = nmg_find_fu_of_eu(eu2);
	if( fu1 && fu2 )  {
		if( fu1->orientation != fu2->orientation ){
			eu2 = eu2->eumate_p;
			fu2 = nmg_find_fu_of_eu(eu2);
			if( fu1->orientation != fu2->orientation )
				rt_bomb( "nmg_radial_join_eu_NEW(): Cannot find matching orientations for faceuses\n" );
		}
	}

	if( eu1->eumate_p->radial_p == eu1 && eu2->eumate_p->radial_p == eu2 &&
		nmg_find_s_of_eu( eu1 ) == nmg_find_s_of_eu( eu2 ) )
	{
		/* Only joining two edges, let's keep it simple */
		nmg_je( eu1, eu2 );
		if( eu1->g.magic_p && eu2->g.magic_p )
		{
			if( eu1->g.magic_p != eu2->g.magic_p )
				nmg_jeg( eu1->g.lseg_p, eu2->g.lseg_p );
		}
		else if( eu1->g.magic_p && !eu2->g.magic_p )
			(void)nmg_use_edge_g( eu2, eu1->g.magic_p );
		else if( !eu1->g.magic_p && eu2->g.magic_p )
			(void)nmg_use_edge_g( eu1, eu2->g.magic_p );
		else
		{
			nmg_edge_g( eu1 );
			nmg_use_edge_g( eu2, eu1->g.magic_p );
		}
		return;
	}

	/* XXX This angle-based algorithm can't yet handle snurb faces! */
	if( fu1 && fu1->f_p->g.magic_p && *fu1->f_p->g.magic_p == NMG_FACE_G_SNURB_MAGIC )  return;
	if( fu2 && fu2->f_p->g.magic_p && *fu2->f_p->g.magic_p == NMG_FACE_G_SNURB_MAGIC )  return;

	/*  Construct local coordinate system for this edge,
	 *  so all angles can be measured relative to a common reference.
	 */
	if( fu1 )  {
		if( fu1->orientation == OT_SAME )
			eu1ref = eu1;
		else
			eu1ref = eu1->eumate_p;
	} else {
		/* eu1 is a wire, find a non-wire, if there are any */
		eu1ref = nmg_find_ot_same_eu_of_e(eu1->e_p);
	}
	if( eu1ref->vu_p->v_p == eu2->vu_p->v_p )  {
		eu2ref = eu2;
	} else {
		eu2ref = eu2->eumate_p;
	}
	nmg_eu_2vecs_perp( xvec, yvec, zvec, eu1ref, tol );

	/* Build the primary lists that describe the situation */
	BU_LIST_INIT( &list1 );
	BU_LIST_INIT( &list2 );
	bu_ptbl_init( &shell_tbl, 64, " &shell_tbl");

	nmg_radial_build_list( &list1, &shell_tbl, 1, eu1ref, xvec, yvec, zvec, tol );
	nmg_radial_build_list( &list2, &shell_tbl, 0, eu2ref, xvec, yvec, zvec, tol );

#if 0
	/* OK so far? */
	nmg_pr_radial_list( &list1, tol );
	nmg_pr_radial_list( &list2, tol );
	nmg_pr_ptbl( "Participating shells", &shell_tbl, 1 );
#endif

	if (rt_g.NMG_debug & DEBUG_MESH_EU ) {
		bu_log("nmg_radial_join_eu_NEW(eu1=x%x, eu2=x%x) e1=x%x, e2=x%x\n",
			eu1, eu2,
			eu1->e_p, eu2->e_p);
		nmg_euprint("\tJoining", eu1);
		bu_log( "Faces around eu1:\n" );
		nmg_pr_fu_around_eu_vecs( eu1ref, xvec, yvec, zvec, tol );
		nmg_pr_radial_list( &list1, tol );

		bu_log( "Faces around eu2:\n" );
		nmg_pr_fu_around_eu_vecs( eu2ref, xvec, yvec, zvec, tol );
		nmg_pr_radial_list( &list2, tol );
		nmg_pr_ptbl( "Participating shells", &shell_tbl, 1 );
	}

#if 0
	count1 = nmg_radial_check_parity( &list1, &shell_tbl, tol );
	count2 = nmg_radial_check_parity( &list2, &shell_tbl, tol );
	if( count1 || count2 ) bu_log("nmg_radial_join_eu_NEW() bad parity at the outset, %d, %d\n", count1, count2);

	best_eg = nmg_pick_best_edge_g( eu1, eu2, tol );
#else
	best_eg = eu1->g.lseg_p;
#endif

	/* Merge the two lists, sorting by angles */
	nmg_radial_merge_lists( &list1, &list2, tol );
	nmg_radial_verify_monotone( &list1, tol );

#if 1
	if (rt_g.NMG_debug & DEBUG_MESH_EU )
	{
		bu_log( "Before nmg_do_radial_join():\n" );
		bu_log( "xvec=(%g %g %g), yvec=(%g %g %g), zvec=(%g %g %g)\n" , V3ARGS( xvec ), V3ARGS( yvec ), V3ARGS( zvec ) );
		nmg_pr_fu_around_eu_vecs( eu2ref, xvec, yvec, zvec, tol );
	}
	nmg_do_radial_join( &list1, eu1ref, xvec, yvec, zvec, tol );

	/* Clean up */
	bu_ptbl_free( &shell_tbl);
	while( BU_LIST_WHILE( rad, nmg_radial, &list1 ) )  {
		BU_LIST_DEQUEUE( &rad->l );
		rt_free( (char *)rad, "nmg_radial" );
	}
	return;
#else

	nmg_radial_mark_cracks( &list1, eu1->e_p, eu2->e_p, tol );

	for( sp = (struct shell **)BU_PTBL_LASTADDR(&shell_tbl);
 	     sp >= (struct shell **)BU_PTBL_BASEADDR(&shell_tbl); sp-- 
	)  {
		nmg_radial_mark_flips( &list1, *sp, tol );
	}

	if (rt_g.NMG_debug & DEBUG_MESH_EU ) {
		bu_log("marked list:\n");
		bu_log("  edge: %g %g %g -> %g %g %g\n",
			V3ARGS(eu1ref->vu_p->v_p->vg_p->coord),
			V3ARGS(eu1ref->eumate_p->vu_p->v_p->vg_p->coord) );
		nmg_pr_radial_list( &list1, tol );
	}

	nmg_radial_implement_decisions( &list1, tol, eu1ref, xvec, yvec, zvec );

	if (rt_g.NMG_debug & DEBUG_MESH_EU ) {
		/* How did it come out? */
		nmg_pr_radial_list( &list1, tol );
		nmg_pr_fu_around_eu_vecs( eu1ref, xvec, yvec, zvec, tol );
	}
	count1 = nmg_radial_check_parity( &list1, &shell_tbl, tol );
	if( count1 )  bu_log("nmg_radial_join_eu_NEW() bad parity at completion, %d\n", count1);
	nmg_radial_verify_pointers( &list1, tol );

	nmg_eu_radial_check( eu1ref, nmg_find_s_of_eu(eu1), tol );	/* expensive */
#endif
}

/*
 *			N M G _ R A D I A L _ E X C H A N G E _ M A R K E D
 *
 *  Exchange eu and eu->eumate_p on the radial list, where marked.
 */
void
nmg_radial_exchange_marked( hd, tol )
struct bu_list		*hd;
CONST struct bn_tol	*tol;		/* for printing */
{
	struct nmg_radial	*rad;
	struct nmg_radial	*prev;
	int			skipped;

	BU_CK_LIST_HEAD(hd);
	BN_CK_TOL(tol);

	for( BU_LIST_FOR( rad, nmg_radial, hd ) )  {
		struct edgeuse	*eu;
		struct edgeuse	*eumate;
		struct edgeuse	*before;
		struct edgeuse	*after;

		if( !rad->needs_flip )  continue;

		/*
		 *  Initial sequencing is:
		 *    before(radial), eu, eumate, after(radial)
		 */
		eu = rad->eu;
		eumate = eu->eumate_p;
		before = eu->radial_p;
		after = eumate->radial_p;

		/*
		 *  Rearrange order to be:
		 *	before, eumate, eu, after.
		 */
		before->radial_p = eumate;
		eumate->radial_p = before;

		after->radial_p = eu;
		eu->radial_p = after;

		rad->eu = eumate;
		rad->fu = nmg_find_fu_of_eu( rad->eu );
		rad->needs_flip = 0;
	}
}

/*
 *			N M G _ S _ R A D I A L _ H A R M O N I Z E
 *
 *  Visit each edge in this shell exactly once.
 *  Where the radial edgeuse parity has become disrupted
 *  due to a boolean operation or whatever, fix it.
 */
void
nmg_s_radial_harmonize( s, tol )
struct shell		*s;
CONST struct bn_tol	*tol;
{
	struct bu_ptbl	edges;
	struct edgeuse	*eu;
	struct bu_list	list;
	vect_t		xvec, yvec, zvec;
	struct edge	**ep;

	NMG_CK_SHELL(s);
	BN_CK_TOL(tol);

	if( rt_g.NMG_debug & DEBUG_BASIC )
		bu_log("nmg_s_radial_harmonize( s=x%x ) BEGIN\n", s);

	nmg_edge_tabulate( &edges, &s->l.magic );
	for( ep = (struct edge **)BU_PTBL_LASTADDR(&edges);
	     ep >= (struct edge **)BU_PTBL_BASEADDR(&edges); ep--
	)  {
		struct nmg_radial	*rad;
		int	nflip;

		NMG_CK_EDGE(*ep);
		eu = nmg_find_ot_same_eu_of_e( *ep );
		nmg_eu_2vecs_perp( xvec, yvec, zvec, eu, tol );

		BU_LIST_INIT( &list );

		nmg_radial_build_list( &list, (struct bu_ptbl *)NULL, 1, eu, xvec, yvec, zvec, tol );
		nflip = nmg_radial_mark_flips( &list, s, tol );
		if( nflip )  {
			if (rt_g.NMG_debug & DEBUG_MESH_EU ) {
				bu_log("Flips needed:\n");
				nmg_pr_radial_list( &list, tol );
			}
			/* Now, do the flips */
			nmg_radial_exchange_marked( &list, tol );
			if (rt_g.NMG_debug & DEBUG_MESH_EU ) {
				nmg_pr_fu_around_eu_vecs( eu, xvec, yvec, zvec, tol );
			}
		}
		/* Release the storage */
		while( BU_LIST_WHILE( rad, nmg_radial, &list ) )  {
			BU_LIST_DEQUEUE( &rad->l );
			rt_free( (char *)rad, "nmg_radial" );
		}
	}

	bu_ptbl_free( &edges);

	if( rt_g.NMG_debug & DEBUG_BASIC )
		bu_log("nmg_s_radial_harmonize( s=x%x ) END\n", s);
}

/*
 *			N M G  _ E U _ R A D I A L _ C H E C K
 *
 *  Where the radial edgeuse parity has become disrupted, note it.
 *
 *  Returns -
 *	0	OK
 *	!0	Radial parity problem detected
 */
int
nmg_eu_radial_check( eu, s, tol )
struct edgeuse		*eu;
struct shell		*s;
CONST struct bn_tol	*tol;
{
	struct bu_list	list;
	vect_t		xvec, yvec, zvec;
	struct nmg_radial	*rad;
	int	nflip;
	struct faceuse	*fu;
#if 1
	return( 0 );
#else

	NMG_CK_EDGEUSE(eu);
	BN_CK_TOL(tol);

	if (rt_g.NMG_debug & DEBUG_BASIC)  {
		bu_log("nmg_eu_radial_check(eu=x%x, s=x%x)\n", eu, s);
	}

	fu = nmg_find_fu_of_eu(eu);
	if( fu && fu->orientation != OT_SAME )  eu = eu->eumate_p;
	nmg_eu_2vecs_perp( xvec, yvec, zvec, eu, tol );

	BU_LIST_INIT( &list );

	/* In bad cases, this routine may rt_bomb() */
	nmg_radial_build_list( &list, NULL, 1, eu, xvec, yvec, zvec, tol );

	nmg_radial_mark_cracks( &list, eu->e_p, NULL, tol );

	nflip = nmg_radial_mark_flips( &list, s, tol );
	if( nflip )  {
		struct nmg_radial *rad;
		bu_log("nmg_eu_radial_check(x%x) %d flips needed\n  %g %g %g --- %g %g %g\n",
			s, nflip,
			V3ARGS(eu->vu_p->v_p->vg_p->coord),
			V3ARGS(eu->eumate_p->vu_p->v_p->vg_p->coord) );
		nmg_pr_radial_list( &list, tol );
		if (rt_g.NMG_debug & DEBUG_MESH_EU ) {
			nmg_pr_fu_around_eu_vecs( eu, xvec, yvec, zvec, tol );
		}

		if (rt_g.NMG_debug)
		{
			char tmp_name[256];

			sprintf( tmp_name, "radial_check_%d.g", debug_file_count );
			nmg_stash_model_to_file( tmp_name, nmg_find_model( &eu->l.magic ), "error" );
			for( BU_LIST_FOR( rad, nmg_radial, &list ) )
				nmg_pr_fu_briefly( rad->fu, "" );
		}
	}

	/* Release the storage */
	while( BU_LIST_WHILE( rad, nmg_radial, &list ) )  {
		BU_LIST_DEQUEUE( &rad->l );
		rt_free( (char *)rad, "nmg_radial" );
	}
	return nflip;
#endif
}

/*
 *			N M G _ S _ R A D I A L _ C H E C K
 *
 *  Visit each edge in this shell exactly once, and check it.
 */
void
nmg_s_radial_check( s, tol )
struct shell		*s;
CONST struct bn_tol	*tol;
{
	struct bu_ptbl	edges;
	struct edgeuse	*eu;
	struct bu_list	list;
	vect_t		xvec, yvec, zvec;
	struct edge	**ep;

	NMG_CK_SHELL(s);
	BN_CK_TOL(tol);

	if( rt_g.NMG_debug & DEBUG_BASIC )
		bu_log("nmg_s_radial_check( s=x%x ) BEGIN\n", s);

	nmg_edge_tabulate( &edges, &s->l.magic );
	for( ep = (struct edge **)BU_PTBL_LASTADDR(&edges);
	     ep >= (struct edge **)BU_PTBL_BASEADDR(&edges); ep--
	)  {
		NMG_CK_EDGE(*ep);
		eu = nmg_find_ot_same_eu_of_e( *ep );
		nmg_eu_radial_check( eu, s, tol );
	}

	bu_ptbl_free( &edges);

	if( rt_g.NMG_debug & DEBUG_BASIC )
		bu_log("nmg_s_radial_check( s=x%x ) END\n", s);
}

/*
 *			N M G _ R _ R A D I A L _ C H E C K
 */
void
nmg_r_radial_check( r, tol )
CONST struct nmgregion	*r;
CONST struct bn_tol	*tol;
{
	struct shell	*s;

	NMG_CK_REGION(r);
	BN_CK_TOL(tol);

	if( rt_g.NMG_debug & DEBUG_BASIC )
		bu_log("nmg_r_radial_check( r=x%x )\n", r);

	for( BU_LIST_FOR( s, shell, &r->s_hd ) )  {
		NMG_CK_SHELL(s);
		nmg_s_radial_check( s, tol );
	}
}
