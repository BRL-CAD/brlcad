/*		This module implimenst the 'E' command
 *
 *  Author -
 *	John Anderson
 *  
 *  Source -
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1997 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "conf.h"

#include <stdio.h>
#include <signal.h>
#ifdef USE_STRING_H
#	include <string.h>
#else
#	include <strings.h>
#endif
#include <errno.h>

#include "machine.h"
#include "bu.h"
#include "vmath.h"
#include "externs.h"
#include "nmg.h"
#include "raytrace.h"
#include "rtgeom.h"
#include "./ged.h"
#include "./mged_solid.h"
#include "./mged_dm.h"
#include <ctype.h>

extern struct bn_tol		mged_tol;
extern struct rt_tess_tol	mged_ttol;
static struct application	ap;
static time_t			start_time, etime;
static struct bu_ptbl		leaf_list;
static long			nvectors;
static struct rt_i		*rtip;
static int			do_polysolids;

/*
 * The tree walker neds to have an initial state.  We could
 * steal it from doview.c but there is no real reason.
 */

static struct db_tree_state E_initial_tree_state = {
	0,			/* ts_dbip */
	0,			/* ts_sofar */
	0,0,0,			/* region, air, gmater */
	100,			/* GIFT los */
#if __STDC__
	{
#endif
		/* struct mater_info ts_mater */
		1.0, 0.0, 0.0,	/* color, RGB */
		0,		/* override */
		0,		/* color inherit */
		0,		/* mater inherit */
#if 0
		""		/* shader */
#else
		NULL		/* shader */
#endif
#if __STDC__
	}
#endif
	,
	1.0, 0.0, 0.0, 0.0,
	0.0, 1.0, 0.0, 0.0,
	0.0, 0.0, 1.0, 0.0,
	0.0, 0.0, 0.0, 1.0,
};


union E_tree
{
	long magic;

	struct E_node
	{
		long magic;
		int op;
		union E_tree *left;
		union E_tree *right;
	} n;

	struct E_leaf
	{
		long magic;
		int op;
		int the_edge;
		struct model *m;
		struct seg seghead;
		struct bu_ptbl edge_list;
		struct soltab *stp;
	} l;
};

#define	E_TREE_MAGIC		0x45545245
#define	CK_ETREE(_p)		BU_CKMAG( _p, E_TREE_MAGIC, "struct E_tree" )

void
Edrawtree( dp )
{
	return;
}

union E_tree *
build_etree( tp )
union tree *tp;
{
	union E_tree *eptr;
	struct nmgregion *r;
	struct directory *dp;
	struct rt_db_internal intern;
	int id;

	RT_CK_TREE( tp );

	BU_GETUNION( eptr, E_tree );
	eptr->magic = E_TREE_MAGIC;

	switch( tp->tr_op )
	{
		case OP_UNION:
		case OP_SUBTRACT:
		case OP_INTERSECT:
			eptr->n.op = tp->tr_op;
			eptr->n.left = build_etree( tp->tr_b.tb_left );
			eptr->n.right = build_etree( tp->tr_b.tb_right );
			break;
		case OP_DB_LEAF:
			eptr->l.op = tp->tr_op;
			eptr->l.the_edge = 0;
			BU_LIST_INIT( &eptr->l.seghead.l );
			if( (dp=db_lookup( dbip, tp->tr_l.tl_name, LOOKUP_NOISY )) == DIR_NULL )
			{
				eptr->l.m = (struct model *)NULL;
				break;
			}
			id = rt_db_get_internal( &intern, dp, dbip, tp->tr_l.tl_mat );
			if( id < 0 )
			{
				Tcl_AppendResult(interp, "Failed to get internal form of ",
					dp->d_namep, "\n", (char *)NULL );
				eptr->l.m = (struct model *)NULL;
				break;
			}
			if( id == ID_COMBINATION )
			{
				struct rt_comb_internal *comb;

				bu_free( (char *)eptr, "eptr" );

				comb = (struct rt_comb_internal *)intern.idb_ptr;
				RT_CK_COMB( comb );

				eptr = build_etree( comb->tree );
				rt_db_free_internal( &intern );
				break;
			}

			eptr->l.m = nmg_mm();
			if (rt_functab[id].ft_tessellate( &r, eptr->l.m, &intern,
				&mged_ttol, &mged_tol) < 0)
			{
				Tcl_AppendResult(interp, "Tessellation failed for ", dp->d_namep,
					"\n", (char *)NULL );
			}

			BU_GETSTRUCT( eptr->l.stp, soltab );
			eptr->l.stp->st_dp = dp;
			eptr->l.stp->st_matp = tp->tr_l.tl_mat;

			{
				struct rt_pg_internal *pg;
				struct rt_db_internal intern2;

				if( do_polysolids )
				{
					eptr->l.stp->st_id = ID_POLY;
					BU_GETSTRUCT( pg, rt_pg_internal );

					if( !nmg_to_poly( eptr->l.m, pg, &mged_tol ) )
					{
						Tcl_AppendResult(interp, "Failed to convert '",
							dp->d_namep, "' to a polysolid\n", (char *)NULL );
					}
					RT_INIT_DB_INTERNAL( &intern2 );
					intern2.idb_type = ID_POLY;
					intern2.idb_ptr = (genptr_t)pg;
					if (rt_functab[ID_POLY].ft_prep( eptr->l.stp, &intern2, rtip ) < 0 )
					{
						Tcl_AppendResult(interp, "Prep failure for solid '", dp->d_namep,
							"'\n", (char *)NULL );
					}

					rt_db_free_internal( &intern2 );
				}
				else
				{
					eptr->l.stp->st_id = id;
					if( rt_functab[id].ft_prep( eptr->l.stp, &intern, rtip ) < 0 )
						Tcl_AppendResult(interp, "Prep failure for solid '", dp->d_namep,
							"'\n", (char *)NULL );
				}
			}

			rt_db_free_internal( &intern );

			/* add this leaf to the leaf list */
			bu_ptbl_ins( &leaf_list, (long *)eptr );
			break;
	}
	return( eptr );
}

void
show_seg( seg, str )
struct seg *seg;
{
	struct seg *ptr;

	if( !seg )
		bu_log( "%s - NULL seg list\n", str );
	else
	{
		if( BU_LIST_IS_EMPTY( &seg->l ) )
			bu_log( "%s - empty\n" );
		else
		{
			bu_log( "%s:\n", str );
			for( BU_LIST_FOR( ptr, seg, &seg->l ) )
			{
				bu_log( "\t %g to %g\n", ptr->seg_in.hit_dist, ptr->seg_out.hit_dist );
			}
		}
	}
}

HIDDEN struct seg *
eval_op( A, op, B )
struct seg *A;
struct seg *B;
int op;
{
	struct seg *sega, *segb;
	struct seg *ptr;
	struct seg *final=(struct seg *)NULL;

	/* handle special cases for "THE EDGE" */
	if( A && B && A->seg_stp && B->seg_stp )
	{
		/* this wil only happen while plotting solid intersection lines */
		if( op != OP_INTERSECT )
			return( eval_op( A, OP_INTERSECT, B ) );
	}
	else
	{

		if( B && op == OP_SUBTRACT && B->seg_stp )
		{
			return( eval_op( A, OP_INTERSECT, B ) );
		}

		if( A && op == OP_UNION && A->seg_stp )
		{
			return( eval_op( A, OP_SUBTRACT, B ) );
		}

		if( B && op == OP_UNION && B->seg_stp )
		{
			return( eval_op( B, OP_SUBTRACT, A ) );
		}
	}

	if(  (A && A->seg_stp) || (B && B->seg_stp) )
	{
		if( A )
			A->seg_stp = (struct soltab *)0x1;
		if( B )
			B->seg_stp = (struct soltab *)0x1;
	}

	if( op == OP_INTERSECT )
	{
		if( !B )
			return( A );
		if( !A )
		{
			RT_FREE_SEG_LIST( B, ap.a_resource );
			return( B );
		}

		BU_GETSTRUCT( final, seg );
		BU_LIST_INIT( &final->l );

		if( A->seg_stp || B->seg_stp )
			final->seg_stp = (struct soltab *)0x1;
		else
			final->seg_stp = (struct soltab *)NULL;

		for( BU_LIST_FOR( sega, seg, &A->l ) )
		{
			for( BU_LIST_FOR( segb, seg, &B->l ) )
			{
				if( segb->seg_out.hit_dist <= sega->seg_in.hit_dist )
					continue;
				if( segb->seg_in.hit_dist >= sega->seg_out.hit_dist )
					continue;

				RT_GET_SEG( ptr, ap.a_resource );
				if( sega->seg_in.hit_dist > segb->seg_in.hit_dist )
					ptr->seg_in.hit_dist = sega->seg_in.hit_dist;
				else
					ptr->seg_in.hit_dist = segb->seg_in.hit_dist;

				if( sega->seg_out.hit_dist < segb->seg_out.hit_dist )
					ptr->seg_out.hit_dist = sega->seg_out.hit_dist;
				else
					ptr->seg_out.hit_dist = segb->seg_out.hit_dist;

				BU_LIST_INSERT( &final->l, &ptr->l );
			}
		}
		RT_FREE_SEG_LIST( A, ap.a_resource );
		RT_FREE_SEG_LIST( B, ap.a_resource );
		BU_LIST_INSERT_LIST( &A->l, &final->l );
		bu_free( (char *)final, "final" );
		return( A );
	}

	if( op == OP_SUBTRACT )
	{
		if( !A )
		{
			RT_FREE_SEG_LIST( B, ap.a_resource );
			return( B );
		}
		if( !B )
			return( A );

		for( BU_LIST_FOR( sega, seg, &A->l ) )
		{
			for( BU_LIST_FOR( segb, seg, &B->l ) )
			{
				if( segb->seg_out.hit_dist <= sega->seg_in.hit_dist )
					continue;
				if( segb->seg_in.hit_dist >= sega->seg_out.hit_dist )
					continue;

				if( sega->seg_in.hit_dist >= segb->seg_in.hit_dist &&
				    sega->seg_out.hit_dist <= segb->seg_out.hit_dist )
				{
					struct seg *tmp;

					/* remove entire segment */
					tmp = sega;
					sega = BU_LIST_PREV( seg, &sega->l );
					BU_LIST_DEQUEUE( &tmp->l );
					RT_FREE_SEG( tmp, ap.a_resource );
					continue;
				}

				if( segb->seg_in.hit_dist > sega->seg_in.hit_dist &&
				    segb->seg_out.hit_dist < sega->seg_out.hit_dist )
				{
					/* splits segment into two */
					RT_GET_SEG( ptr, ap.a_resource );
					ptr->seg_in.hit_dist = sega->seg_in.hit_dist;
					ptr->seg_out.hit_dist = segb->seg_in.hit_dist;
					sega->seg_in.hit_dist = segb->seg_out.hit_dist;
					BU_LIST_INSERT( &sega->l, &ptr->l );
				}
				else
				{
					if( segb->seg_in.hit_dist > sega->seg_in.hit_dist )
						sega->seg_out.hit_dist = segb->seg_in.hit_dist;
					if( segb->seg_out.hit_dist < sega->seg_out.hit_dist )
						sega->seg_in.hit_dist = segb->seg_out.hit_dist;
				}
			}
		}
		RT_FREE_SEG_LIST( B, ap.a_resource );
		return( A );
	}

	if( op == OP_UNION )
	{
		if( !A && !B )
			return( (struct seg *)NULL );
		if( !A )
			return( B );
		if( !B )
			return( A );

		while( BU_LIST_WHILE( segb, seg, &B->l ) )
		{
			int found=0;

			for( BU_LIST_FOR( sega, seg, &A->l ) )
			{
				if( segb->seg_in.hit_dist > sega->seg_out.hit_dist )
					continue;

				found = 1;
				if( segb->seg_out.hit_dist <= sega->seg_out.hit_dist &&
				    segb->seg_in.hit_dist >= sega->seg_in.hit_dist )
				{
					/* nothing to do */
					BU_LIST_DEQUEUE( &segb->l );
					bu_free( (char *)segb, "segb" );
					break;
				}

				if( segb->seg_out.hit_dist < sega->seg_in.hit_dist )
				{
					/* insert B segment before A segment */
					BU_LIST_DEQUEUE( &segb->l );
					BU_LIST_INSERT( &sega->l, &segb->l );
					break;
				}

				/* segments overlap */
				if( segb->seg_in.hit_dist < sega->seg_in.hit_dist )
					sega->seg_in.hit_dist = segb->seg_in.hit_dist;
				if( segb->seg_out.hit_dist > sega->seg_out.hit_dist )
					sega->seg_out.hit_dist = segb->seg_out.hit_dist;

				BU_LIST_DEQUEUE( &segb->l );
				bu_free( (char *)segb, "segb" );
				break;
			}

			if( !found )
			{
				/* segment from B gets added to end of A */
				BU_LIST_DEQUEUE( &segb->l )
				BU_LIST_INSERT( &A->l, &segb->l );
			}
		}
		return( A );
	}
}

HIDDEN struct seg *
eval_etree( eptr )
union E_tree *eptr;
{
	struct seg *A, *B;

	CK_ETREE( eptr );

	switch( eptr->l.op )
	{
		case OP_DB_LEAF:
			if( eptr->l.the_edge )
			{
				eptr->l.seghead.seg_stp = (struct soltab *)0x1;
			}
			else
			{
				eptr->l.seghead.seg_stp = (struct soltab *)NULL;
			}
			return( &eptr->l.seghead );
		case OP_SUBTRACT:
		case OP_INTERSECT:
		case OP_UNION:
			A = eval_etree( eptr->n.left );
			B = eval_etree( eptr->n.right );
			return( eval_op( A, eptr->n.op, B ) );
	}
}

HIDDEN void
shoot_and_plot( start_pt, dir, vhead, edge_len, skip_leaf1, skip_leaf2, eptr )
point_t start_pt;
vect_t dir;
struct bu_list *vhead;
fastf_t edge_len;
int skip_leaf1, skip_leaf2;
union E_tree *eptr;
{
	struct xray rp;
	struct ray_data rd;
	int shoot_leaf;
	struct seg *final_segs;

	if( bu_debug&BU_DEBUG_MEM_CHECK && bu_mem_barriercheck() )
		bu_log( "Error at start of shoot_and_plot()\n" );

	CK_ETREE( eptr );

	bzero( &rd, sizeof( struct ray_data ) );
	bzero( &ap, sizeof( struct application ) );

	ap.a_resource = &rt_uniresource;
	rt_uniresource.re_magic = RESOURCE_MAGIC;
	ap.a_rt_i = rtip;

	if( BU_LIST_UNINITIALIZED( &rt_uniresource.re_nmgfree ) )
		BU_LIST_INIT( &rt_uniresource.re_nmgfree );

	BU_GETSTRUCT( rd.seghead, seg );
	BU_LIST_INIT( &rd.seghead->l );

	VMOVE( rp.r_pt, start_pt )
	VMOVE( rp.r_dir, dir )
	/* Compute the inverse of the direction cosines */
	if( !NEAR_ZERO( rp.r_dir[X], SQRT_SMALL_FASTF ) )  {
		rd.rd_invdir[X]=1.0/rp.r_dir[X];
	} else {
		rd.rd_invdir[X] = INFINITY;
		rp.r_dir[X] = 0.0;
	}
	if( !NEAR_ZERO( rp.r_dir[Y], SQRT_SMALL_FASTF ) )  {
		rd.rd_invdir[Y]=1.0/rp.r_dir[Y];
	} else {
		rd.rd_invdir[Y] = INFINITY;
		rp.r_dir[Y] = 0.0;
	}
	if( !NEAR_ZERO( rp.r_dir[Z], SQRT_SMALL_FASTF ) )  {
		rd.rd_invdir[Z]=1.0/rp.r_dir[Z];
	} else {
		rd.rd_invdir[Z] = INFINITY;
		rp.r_dir[Z] = 0.0;
	}

	/* set up "ray_data" structure for nmg raytrace */
	rd.rp = &rp;
	rd.tol = &mged_tol;
	rd.ap = &ap;
	rd.magic = NMG_RAY_DATA_MAGIC;
	rd.classifying_ray = 0;
	rd.hitmiss = (struct hitmiss **)NULL;

	/* shoot this ray at every leaf solid except the current one */
	for( shoot_leaf=0 ; shoot_leaf < BU_PTBL_END( &leaf_list ) ; shoot_leaf++ )
	{
		union E_tree *shoot;
		int dont_shoot=0;

		shoot = (union E_tree *)BU_PTBL_GET( &leaf_list, shoot_leaf );

		if( BU_LIST_NON_EMPTY( &shoot->l.seghead.l ) )
		{
			RT_FREE_SEG_LIST( &shoot->l.seghead, ap.a_resource );
		}
		BU_LIST_INIT( &shoot->l.seghead.l );

		if( shoot_leaf == skip_leaf1 || shoot_leaf == skip_leaf2 )
			dont_shoot = 1;
		else
		{
			union E_tree *leaf;

			if( skip_leaf1 >= 0 )
			{
				leaf = (union E_tree *)BU_PTBL_GET( &leaf_list, skip_leaf1 );
				if( leaf->l.stp->st_dp == shoot->l.stp->st_dp )
				{
					if( !leaf->l.stp->st_matp && !shoot->l.stp->st_matp )
						dont_shoot = 1;
					else if( bn_mat_is_equal( leaf->l.stp->st_matp, shoot->l.stp->st_matp, &mged_tol ) )
						dont_shoot = 1;
				}
			}
			if( !dont_shoot && skip_leaf2 >= 0 )
			{
				leaf = (union E_tree *)BU_PTBL_GET( &leaf_list, skip_leaf2 );
				if( leaf->l.stp->st_dp == shoot->l.stp->st_dp )
				{
					if( !leaf->l.stp->st_matp && !shoot->l.stp->st_matp )
						dont_shoot = 1;
					else if( bn_mat_is_equal( leaf->l.stp->st_matp, shoot->l.stp->st_matp, &mged_tol ) )
						dont_shoot = 1;
				}
			}
		}

		if( dont_shoot )
		{
			struct seg *seg;

			/* put entire edge in seg list */
			RT_GET_SEG( seg, ap.a_resource );
			seg->l.magic = RT_SEG_MAGIC;
			seg->seg_in.hit_dist = 0.0;
			seg->seg_out.hit_dist = edge_len;
			BU_LIST_INSERT( &shoot->l.seghead.l, &seg->l );
			if( shoot_leaf == skip_leaf1 || shoot_leaf == skip_leaf2 )
				shoot->l.the_edge = 1;
			else
				shoot->l.the_edge = 0;
			continue;
		}

		shoot->l.the_edge = 0;

		/* initialize the lists of things that have been hit/missed */
		rd.rd_m = shoot->l.m;
		BU_LIST_INIT(&rd.rd_hit);
		BU_LIST_INIT(&rd.rd_miss);

		rd.stp = shoot->l.stp;

		/* make sure this solid is identified as not "THE EDGE" */
		shoot->l.the_edge = 0;

		/* actually shoot the ray */
		if( rt_in_rpp( &rp, rd.rd_invdir, shoot->l.stp->st_min, shoot->l.stp->st_max ) )
		{
			if( rt_functab[shoot->l.stp->st_id].ft_shot( shoot->l.stp, &rp, &ap, rd.seghead ) )
			{
				struct seg *seg;

				seg = BU_LIST_FIRST( seg, &rd.seghead->l );
				/* put the segments in the lead solid structure */
				BU_LIST_INSERT_LIST( &shoot->l.seghead.l, &rd.seghead->l );
				shoot->l.seghead.seg_stp = shoot->l.stp;
			}
		}
	}

	/* Evaluate the Boolean tree to get the "final" segments
	 * which are to be plotted.
	 */
	final_segs = eval_etree( eptr );

	if( final_segs )
	{
		struct seg *seg;

		/* add the segemnts to the VLIST */
		for( BU_LIST_FOR( seg, seg, &final_segs->l ) )
		{
			point_t pt;
			nvectors++;
			VJOIN1( pt, rp.r_pt, seg->seg_in.hit_dist, rp.r_dir )
			RT_ADD_VLIST( vhead, pt, RT_VLIST_LINE_MOVE );
			VJOIN1( pt, rp.r_pt, seg->seg_out.hit_dist, rp.r_dir )
			RT_ADD_VLIST( vhead, pt, RT_VLIST_LINE_DRAW );
		}
		
	}

	if( final_segs )
		RT_FREE_SEG_LIST( final_segs, ap.a_resource );

	if( bu_debug&BU_DEBUG_MEM_CHECK && bu_mem_barriercheck() )
		bu_log( "Error at end of shoot_and_plot()\n" );

}

HIDDEN void
Eplot( eptr, vhead )
union E_tree *eptr;
struct bu_list *vhead;
{
	int leaf_no;
	union E_tree *leaf_ptr;
	struct ray_data rd;
	int hit_count=0;

	if( bu_debug&BU_DEBUG_MEM_CHECK && bu_mem_barriercheck() )
		bu_log( "Error at start of Eplot()\n" );

	CK_ETREE( eptr );

	/* create an edge list for each leaf solid */
	for( leaf_no=0 ; leaf_no < BU_PTBL_END( &leaf_list ) ; leaf_no++ )
	{
		leaf_ptr = (union E_tree *)BU_PTBL_GET( &leaf_list, leaf_no );
		CK_ETREE( leaf_ptr );
		if( leaf_ptr->l.op != OP_DB_LEAF )
		{
			Tcl_AppendResult(interp, "Eplot: Bad leaf node!!!\n", (char *)NULL );
			return;
		}

		if( leaf_ptr->l.m )
			nmg_edge_tabulate( &leaf_ptr->l.edge_list, &leaf_ptr->l.m->magic );
		else
			bu_ptbl_init( &leaf_ptr->l.edge_list, 1, "edge_list" );
	}

	/* now plot appropriate parts of each solid */

	/* loop through every leaf solid */
	for( leaf_no=0 ; leaf_no < BU_PTBL_END( &leaf_list ) ; leaf_no++ )
	{
		int edge_no;

		leaf_ptr = (union E_tree *)BU_PTBL_GET( &leaf_list, leaf_no );

		if( !leaf_ptr->l.m )
			continue;

		leaf_ptr->l.the_edge = 1;

		/* do each edge of the current leaf solid */
		for( edge_no=0 ; edge_no < BU_PTBL_END( &leaf_ptr->l.edge_list ) ; edge_no++ )
		{
			struct edge *e;
			struct edgeuse *eu;
			struct vertex_g *vg;
			struct vertex_g *vg2;
			vect_t dir;
			fastf_t edge_len;
			fastf_t inv_len;

			e = (struct edge *)BU_PTBL_GET( &leaf_ptr->l.edge_list, edge_no );
			NMG_CK_EDGE( e );
			vg = e->eu_p->vu_p->v_p->vg_p;
			NMG_CK_VERTEX_G( vg );

			vg2 = e->eu_p->eumate_p->vu_p->v_p->vg_p;
			NMG_CK_VERTEX_G( vg2 );

			/* set up a ray from vg towards vg2 */
			VSUB2( dir, vg2->coord, vg->coord );
			edge_len = MAGNITUDE( dir );
			if( edge_len < mged_tol.dist )
				continue;
			inv_len = 1.0/edge_len;
			VSCALE( dir, dir, inv_len );

			shoot_and_plot( vg->coord, dir, vhead, edge_len, leaf_no, -1, eptr );

		}
	}

	/* Now draw solid intersection lines */

	for( leaf_no=0 ; leaf_no < BU_PTBL_END( &leaf_list ) ; leaf_no++ )
	{
		int leaf2;

		leaf_ptr = (union E_tree *)BU_PTBL_GET( &leaf_list, leaf_no );
		if( !leaf_ptr->l.m )
			continue;

		for( leaf2=leaf_no+1 ; leaf2 < BU_PTBL_END( &leaf_list ) ; leaf2++ )
		{
			union E_tree *leaf2_ptr;
			struct nmgregion *r1, *r2;
			struct shell *s1, *s2;
			struct faceuse *fu1, *fu2;
			struct face *f1, *f2;
			plane_t pl1, pl2;
			struct loopuse *lu1, *lu2;
			struct edgeuse *eu1, *eu2;
			struct vertex_g *vg1a, *vg1b, *vg2a, *vg2b;

			leaf2_ptr = (union E_tree *)BU_PTBL_GET( &leaf_list, leaf2 );
			if( !leaf2_ptr->l.m )
				continue;

			/* find intersection lines between these two NMG's */

			r1 = BU_LIST_FIRST( nmgregion, &leaf_ptr->l.m->r_hd );
			s1 = BU_LIST_FIRST( shell, &r1->s_hd );
			r2 = BU_LIST_FIRST( nmgregion, &leaf2_ptr->l.m->r_hd );
			s2 = BU_LIST_FIRST( shell, &r2->s_hd );

			for( BU_LIST_FOR( fu1, faceuse, &s1->fu_hd ) )
			{
				if( fu1->orientation != OT_SAME )
					continue;

				f1 = fu1->f_p;

				for( BU_LIST_FOR( fu2, faceuse, &s2->fu_hd ) )
				{
					fastf_t dist;
					fastf_t old_dist;
					fastf_t len,start_len;
					point_t hits[4];
					point_t start_pt;
					vect_t dir;
					vect_t to_hit;
					fastf_t inv_len;
					fastf_t lena, lenb;

					if( fu2->orientation != OT_SAME )
						continue;

					f2 = fu2->f_p;

					if ( !V3RPP_OVERLAP_TOL(f2->min_pt, f2->max_pt,
						f1->min_pt, f1->max_pt, &mged_tol) )
							continue;

					NMG_GET_FU_PLANE( pl1, fu1 );
					NMG_GET_FU_PLANE( pl2, fu2 );

					if( bn_coplanar( pl1, pl2, &mged_tol ) )
						continue;

					hit_count=0;
					old_dist = MAX_FASTF;
					for( BU_LIST_FOR( lu1, loopuse, &fu1->lu_hd ) )
					{
						if( BU_LIST_FIRST_MAGIC( &lu1->down_hd ) != NMG_EDGEUSE_MAGIC )
							continue;

						for( BU_LIST_FOR( eu1, edgeuse, &lu1->down_hd ) )
						{
							vg1a = eu1->vu_p->v_p->vg_p;
							vg1b = eu1->eumate_p->vu_p->v_p->vg_p;
							VSUB2( dir, vg1b->coord, vg1a->coord );

							/* find intersection of this edge with fu2 */

							if( bn_isect_line3_plane( &dist, vg1a->coord, dir, pl2, &mged_tol ) < 1 )
								continue;

							if( dist < 0.0 || dist >= 1.0 )
								continue;

							if( NEAR_ZERO( old_dist - dist, mged_tol.dist ) )
								continue;

							old_dist = dist;
							VJOIN1( hits[hit_count], vg1a->coord, dist, dir )
							hit_count++;
							if( hit_count == 2 )
								break;;
						}
						if( hit_count == 2 )
							break;;
					}
					old_dist = MAX_FASTF;
					for( BU_LIST_FOR( lu2, loopuse, &fu2->lu_hd ) )
					{
						if( BU_LIST_FIRST_MAGIC( &lu2->down_hd ) != NMG_EDGEUSE_MAGIC )
							continue;

						for( BU_LIST_FOR( eu2, edgeuse, &lu2->down_hd ) )
						{
							vg2a = eu2->vu_p->v_p->vg_p;
							vg2b = eu2->eumate_p->vu_p->v_p->vg_p;
							VSUB2( dir, vg2b->coord, vg2a->coord );

							/* find intersection of this edge with fu1 */

							if( bn_isect_line3_plane( &dist, vg2a->coord, dir, pl1, &mged_tol ) < 1 )
								continue;

							if( dist < 0.0 || dist >= 1.0 )
								continue;

							if( NEAR_ZERO( old_dist - dist, mged_tol.dist ) )
								continue;

							old_dist = dist;
							VJOIN1( hits[hit_count], vg2a->coord, dist, dir )
							hit_count++;
							if( hit_count == 4 )
								break;;
						}
						if( hit_count == 4 )
							break;;
					}

					/* now should have 4 hit points along line of intersection */
					if( hit_count != 4 )
						continue;

					VSUB2( dir, hits[1], hits[0] )
					len = MAGNITUDE( dir );
					if( len < mged_tol.dist )
						continue;
					inv_len = 1.0/len;
					VSCALE( dir, dir, inv_len )
					VSUB2( to_hit, hits[2], hits[0] )
					lena = VDOT( to_hit, dir );
					VSUB2( to_hit, hits[3], hits[0] )
					lenb = VDOT( to_hit, dir );
					if( lena > lenb )
					{
						dist = lena;
						lena = lenb;
						lenb = dist;
						VMOVE( start_pt, hits[3] )
						VMOVE( hits[2], hits[3] )
						VMOVE( hits[3], start_pt )
					}

					if( lena > len )
						continue;
					if( lenb < 0.0 )
						continue;

					if( lena > 0.0 )
					{
						VMOVE( start_pt, hits[2] )
						start_len = lena;
					}
					else
					{
						VMOVE( start_pt, hits[0] )
						start_len = 0.0;
					}

					if( lenb < len )
						len = lenb;

					shoot_and_plot( start_pt, dir, vhead, len-start_len, leaf_no, leaf2, eptr );
				}
			}
		}
	}

	if( bu_debug&BU_DEBUG_MEM_CHECK && bu_mem_barriercheck() )
		bu_log( "Error at end of Eplot()\n" );

}

HIDDEN void
free_etree( eptr )
union E_tree *eptr;
{
	CK_ETREE( eptr );

	switch( eptr->l.op )
	{
		case OP_UNION:
		case OP_SUBTRACT:
		case OP_INTERSECT:
			free_etree( eptr->n.left );
			free_etree( eptr->n.right );
			bu_free( (char *)eptr, "node pointer" );
			break;
		case OP_DB_LEAF:
			if( eptr->l.m )
			{
				nmg_km( eptr->l.m );
				eptr->l.m = (struct model *)NULL;
			}
			if( BU_LIST_NON_EMPTY( &eptr->l.seghead.l ) )
			{
				RT_FREE_SEG_LIST( &eptr->l.seghead, ap.a_resource );
			}
			if( BU_LIST_NON_EMPTY( &eptr->l.edge_list.l ) )
			{
				bu_ptbl_free( &eptr->l.edge_list );
			}
			if( eptr->l.stp )
			{
				rt_functab[eptr->l.stp->st_id].ft_free( eptr->l.stp );
				bu_free( (char *)eptr->l.stp, "struct soltab" );
			}

			bu_free( (char *)eptr, "leaf pointer" );
			break;
	}
}

HIDDEN union tree *
E_region_end( tsp, pathp, curtree )
register struct db_tree_state *tsp;
struct db_full_path     *pathp;
union tree              *curtree;
{
	struct directory *dp;
	struct bu_vls vls;
	struct rt_db_internal intern;
	struct rt_comb_internal *comb;
	union E_tree *etree;
	union tree *ret_tree;
	struct bu_list vhead;
	int id;
	int simple_solid=0;

	if( bu_debug&BU_DEBUG_MEM_CHECK && bu_mem_barriercheck() )
		bu_log( "Error at start of E_region_end()\n" );

	RT_CK_TESS_TOL(tsp->ts_ttol);
	BN_CK_TOL(tsp->ts_tol);
	RT_CK_FULL_PATH(pathp)

	bu_vls_init( &vls );

	dp = DB_FULL_PATH_CUR_DIR( pathp );
	RT_CK_DIR( dp );

	BU_LIST_INIT( &vhead );

	if( dp->d_flags & DIR_SOLID )
		simple_solid = 1;

	id = rt_db_get_internal( &intern, dp, dbip, tsp->ts_mat );
	if( id < 0 )
	{
		Tcl_AppendResult(interp, "Unable to get internal form of ", dp->d_namep,
			"\n", (char *)NULL );
		return( curtree );
	}

	if( simple_solid )
	{
		struct model *m;
		struct nmgregion *r;

		m = nmg_mm();
		if (rt_functab[id].ft_tessellate( &r, m, &intern,
			&mged_ttol, &mged_tol) < 0)
		{
			Tcl_AppendResult(interp, "Tessellation failed for ", dp->d_namep,
				"\n", (char *)NULL );
		}

		nmg_m_to_vlist( &vhead, m, 0 );
		rt_db_free_internal( &intern );

		db_free_tree( curtree );
		ret_tree =  (union tree *)NULL;
	}
	else
	{
		comb = (struct rt_comb_internal *)intern.idb_ptr;
		RT_CK_COMB( comb );

		if( !comb->tree )
		{
			rt_db_free_internal( &intern );
			return( curtree );
		}

		etree = build_etree( comb->tree );

		Eplot( etree, &vhead );
		rt_db_free_internal( &intern );

		/* Free etree */
		free_etree( etree );

		/* reset leaf_list */
		bu_ptbl_reset( &leaf_list );

		ret_tree = curtree;
	}

	drawH_part2( 0, &vhead, pathp, tsp, SOLID_NULL );

	if( bu_debug&BU_DEBUG_MEM_CHECK && bu_mem_barriercheck() )
		bu_log( "Error at end of E_region_end()\n" );

	return( ret_tree );
}

HIDDEN union tree *
E_solid_end( tsp, pathp, ep, id )
struct db_tree_state    *tsp;
struct db_full_path     *pathp;
struct bu_external      *ep;
int                     id;
{
	union tree *curtree;

	db_free_external( ep );
	BU_GETUNION( curtree, tree );
	curtree->magic = RT_TREE_MAGIC;
	curtree->tr_op = OP_NOP;
	return( curtree );
}
/*
 *			F _ E V E D I T
 *
 *  The "Big E" command.
 *  Evaluated Edit something (add to visible display)
 *  Usage: E object(s)
 */
int
f_evedit(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int	argc;
char	**argv;
{
	int i;
	struct directory *dp;
	int	initial_blank_screen;
	int start_argc=1;
	char perf_message[128];

	if(argc < 2 || MAXARGS < argc){
	  struct bu_vls vls;

	  bu_vls_init(&vls);
	  bu_vls_printf(&vls, "help E");
	  Tcl_Eval(interp, bu_vls_addr(&vls));
	  bu_vls_free(&vls);
	  return TCL_ERROR;
	}

/*	bu_debug = BU_DEBUG_MEM_CHECK; */
	if( bu_debug&BU_DEBUG_MEM_CHECK && bu_mem_barriercheck() )
		bu_log( "Error at start of 'E'\n" );

	do_polysolids = 0;
	if( *argv[1] == '-' )
	{
		do_polysolids = 1;
		start_argc++;
	}

	initial_blank_screen = BU_LIST_IS_EMPTY(&HeadSolid.l);

	for( i=start_argc ; i<argc ; i++ )
	{
		if( (dp = db_lookup( dbip,  argv[i], LOOKUP_QUIET )) != DIR_NULL )
		{
			eraseobj( dp );
			no_memory = 0;
		}
	}

	if( dmp->dm_displaylist )
	{
		/* Force displaylist update before starting new drawing */
		update_views = 1;
		refresh();
	}


	E_initial_tree_state.ts_ttol = &mged_ttol;
	E_initial_tree_state.ts_tol  = &mged_tol;
	E_initial_tree_state.ts_stop_at_regions = 1;

	mged_ttol.magic = RT_TESS_TOL_MAGIC;
	mged_ttol.abs = mged_abs_tol;
	mged_ttol.rel = mged_rel_tol;
	mged_ttol.norm = mged_nrm_tol;

	bu_ptbl_init( &leaf_list, 8, "leaf_list" );

	if( setjmp( jmp_env ) == 0 )
		(void)signal( SIGINT, sig3 ); /* allow interupts */
	else
	{
		bu_ptbl_free( &leaf_list );
		return( TCL_OK );
	}

	rtip = rt_new_rti( dbip );
	rtip->rti_tol = mged_tol;	/* struct copy */

	nvectors = 0;
	(void)time( &start_time );

	(void)db_walk_tree( dbip, argc-start_argc, (CONST char **)(argv+start_argc), 1, &E_initial_tree_state, 0,
		E_region_end, E_solid_end );

	(void)time( &etime );

	(void)signal( SIGINT, SIG_IGN );

	/* free leaf_list */
	bu_ptbl_free( &leaf_list );

      /* If we went from blank screen to non-blank, resize */
      if (mged_variables.autosize  && initial_blank_screen &&
	  BU_LIST_NON_EMPTY(&HeadSolid.l)) {
	size_reset();
	new_mats();
	(void)mged_svbase();
      }

	color_soltab();

	sprintf(perf_message, "E: %ld vectors in %ld sec\n", nvectors, etime - start_time );
	Tcl_AppendResult(interp, perf_message, (char *)NULL);

	return TCL_OK;
}
