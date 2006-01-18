/*                          B I G E . C
 * BRL-CAD
 *
 * Copyright (c) 1997-2006 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */

/** \addtogroup librt */
/*@{*/
/** @file bigE.c
 * This module implements the 'E' command.
 *
 *  Author -
 *	John Anderson
 *
 *  Modifications -
 *	Bob Parker - modified to live in librt's drawable geometry object.
 *
 *  Source -
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *
 */
/*@}*/

#ifndef lint
static const char RCSid[] = "";
#endif

#include "common.h"

#include <stdio.h>
#include <signal.h>
#ifdef HAVE_STRING_H
#  include <string.h>
#else
#  include <strings.h>
#endif
#include <errno.h>
#include <time.h>

#include "machine.h"
#include "bu.h"
#include "vmath.h"
#include "nmg.h"
#include "rtgeom.h"
#include "raytrace.h"
#include "solid.h"
#include <ctype.h>
/* #define debug 1 */

struct dg_client_data {
	struct dg_obj		*dgop;
	Tcl_Interp		*interp;
	int			wireframe_color_override;
	int			wireframe_color[3];
	struct application	*ap;
	struct bu_ptbl		leaf_list;
	struct rt_i		*rtip;
	time_t			start_time;
	time_t			etime;
	long			nvectors;
	int			do_polysolids;
	int			num_halfs;
};

union E_tree *build_etree(union tree *tp, struct dg_client_data *dgcdp);

/* segment types (stored in the "seg_stp" field of the (struct seg) */
#define	ON_SURF	(struct soltab *)0x1
#define IN_SOL  (struct soltab *)0x2
#define ON_INT	(struct soltab *)0x3

#define NOT_SEG_OVERLAP( _a, _b )	((_a->seg_out.hit_dist <= _b->seg_in.hit_dist) || (_b->seg_out.hit_dist <= _a->seg_in.hit_dist))

/* RT_FREE_SEG_LIST assumed list head is a "struct seg" */
#define MY_FREE_SEG_LIST( _segheadp, _res )	{ \
	register struct seg *_a; \
	while( BU_LIST_WHILE( _a, seg, (_segheadp) ) )  { \
		BU_LIST_DEQUEUE( &(_a->l) ); \
		RT_FREE_SEG( _a, _res ); \
	} }

/* stolen from g_half.c */
struct half_specific  {
        plane_t half_eqn;               /* Plane equation, outward normal */
        vect_t  half_Xbase;             /* "X" basis direction */
        vect_t  half_Ybase;             /* "Y" basis direction */
};
#define HALF_NULL       ((struct half_specific *)0)

/* structures for building a tree corresponding to the region to be drawn
 * uses the same "op" values as "union tree"
 */
union E_tree
{
	long magic;

	struct E_node	/* the operator nodes */
	{
		long magic;
		int op;
		union E_tree *left;
		union E_tree *right;
	} n;

	struct E_leaf	/* the leaf nodes */
	{
		long magic;
		int op;
		struct model *m;		/* NMG version of this leaf solid */
		struct bu_list seghead;		/* head of list of segments for this leaf solid */
		struct bu_ptbl edge_list;	/* list of edges from above NMG */
		struct soltab *stp;		/* the usual soltab pointer */
		unsigned char do_not_free_model; /* A flag indicating that the NMG model pointer is a reference to the
						 * NMG model in the soltab structure */
	} l;
};

#define	E_TREE_MAGIC		0x45545245
#define	CK_ETREE(_p)		BU_CKMAG( _p, E_TREE_MAGIC, "struct E_tree" )

void
Edrawtree(int dp)
{
	return;
}

HIDDEN union E_tree *
#if 1
add_solid(const struct directory	*dp,
	  matp_t			mat,
	  struct dg_client_data		*dgcdp)
#else
add_solid(dp, mat, dgcdp)
struct directory *dp;
matp_t mat;
struct dg_client_data *dgcdp;
#endif
{
	union E_tree *eptr;
	struct nmgregion *r;
	struct rt_db_internal intern;
	int id;
	int solid_is_plate_mode_bot=0;

	BU_GETUNION( eptr, E_tree );
	eptr->magic = E_TREE_MAGIC;

	id = rt_db_get_internal( &intern, dp, dgcdp->dgop->dgo_wdbp->dbip, mat, &rt_uniresource );
	if( id < 0 )
	{
		Tcl_AppendResult(dgcdp->interp, "Failed to get internal form of ",
			dp->d_namep, "\n", (char *)NULL );
		eptr->l.m = (struct model *)NULL;
		return( eptr );
	}
	if( id == ID_COMBINATION )
	{
		/* do explicit expansion of referenced combinations */

		struct rt_comb_internal *comb;

		bu_free( (char *)eptr, "eptr" );

		comb = (struct rt_comb_internal *)intern.idb_ptr;
		RT_CK_COMB( comb );

		eptr = build_etree( comb->tree, dgcdp );
		rt_db_free_internal( &intern, &rt_uniresource );
		return( eptr );
	}
#if 0
	if( id == ID_BOT )
	{
		struct rt_bot_internal *bot = (struct rt_bot_internal *)intern.idb_ptr;

		/* if this is a plate mode BOT, lie to the tesselator to get
		 * an approximation
		 */

		RT_BOT_CK_MAGIC( bot );

		if( bot->mode == RT_BOT_PLATE || bot->mode == RT_BOT_PLATE_NOCOS )
		{
			solid_is_plate_mode_bot = 1;
			bot->mode = RT_BOT_SOLID;
		}
	}
#endif
	if( id == ID_HALF )
	{
		eptr->l.m = NULL;
		dgcdp->num_halfs++;
	}
	else if( id == ID_NMG )
	{
		/* steal the nmg model */
		eptr->l.m = (struct model *)intern.idb_ptr;
		eptr->l.do_not_free_model = 1;
	}
	else
	{
		/* create the NMG version of this solid */
		eptr->l.m = nmg_mm();
		if (rt_functab[id].ft_tessellate( &r, eptr->l.m, &intern,
			&dgcdp->dgop->dgo_wdbp->wdb_ttol, &dgcdp->dgop->dgo_wdbp->wdb_tol) < 0)
		{
			nmg_km( eptr->l.m );
			eptr->l.m = NULL;
		}
	}

	/* get the soltab stuff */
	BU_GETSTRUCT( eptr->l.stp, soltab );
	eptr->l.stp->l.magic = RT_SOLTAB_MAGIC;
	eptr->l.stp->l2.magic = RT_SOLTAB2_MAGIC;
	eptr->l.stp->st_dp = dp;
	eptr->l.stp->st_matp = mat;

	{
		struct rt_bot_internal *bot;
		struct rt_db_internal intern2;

		if( dgcdp->do_polysolids )
		{
			struct shell *s=(struct shell *)NULL;
			struct nmgregion *r=(struct nmgregion *)NULL;

			/* create and prep a BoT version of this solid */
			if( eptr->l.m ) {
				r = BU_LIST_FIRST( nmgregion, &eptr->l.m->r_hd );
				s = BU_LIST_FIRST( shell, &r->s_hd );
			}

			if( solid_is_plate_mode_bot ||
			    !eptr->l.m ||
			    (bot=nmg_bot( s, &dgcdp->dgop->dgo_wdbp->wdb_tol ) ) == (struct rt_bot_internal *)NULL )
			{
				eptr->l.stp->st_id = id;
				eptr->l.stp->st_meth = &rt_functab[id];
				if( rt_functab[id].ft_prep( eptr->l.stp, &intern, dgcdp->rtip ) < 0 )
					Tcl_AppendResult(dgcdp->interp, "Prep failure for solid '", dp->d_namep,
						"'\n", (char *)NULL );
			}
			else
			{
				RT_INIT_DB_INTERNAL( &intern2 );
				intern2.idb_major_type = DB5_MAJORTYPE_BRLCAD;
				intern2.idb_type = ID_BOT;
				intern2.idb_meth = &rt_functab[ID_BOT];
				intern2.idb_ptr = (genptr_t)bot;
				eptr->l.stp->st_id = ID_BOT;
				eptr->l.stp->st_meth = &rt_functab[ID_BOT];
				if (rt_functab[ID_BOT].ft_prep( eptr->l.stp, &intern2, dgcdp->rtip ) < 0 )
				{
					Tcl_AppendResult(dgcdp->interp, "Prep failure for solid '", dp->d_namep,
							 "'\n", (char *)NULL );
				}

				rt_db_free_internal( &intern2, &rt_uniresource );
			}
		}
		else
		{
			/* prep this solid */

			eptr->l.stp->st_id = id;
			eptr->l.stp->st_meth = &rt_functab[id];
			if( rt_functab[id].ft_prep( eptr->l.stp, &intern, dgcdp->rtip ) < 0 )
				Tcl_AppendResult(dgcdp->interp, "Prep failure for solid '", dp->d_namep,
					"'\n", (char *)NULL );
		}
	}

	if( id != ID_NMG )
		rt_db_free_internal( &intern, &rt_uniresource );

	/* add this leaf to the leaf list */
	bu_ptbl_ins( &dgcdp->leaf_list, (long *)eptr );

	return( eptr );
}

/* build an E_tree corresponding to the region tree (tp) */
union E_tree *
build_etree(union tree			*tp,
	    struct dg_client_data	*dgcdp)
{
	union E_tree *eptr = NULL;
	struct soltab *stp;
	struct directory *dp;

	RT_CK_TREE( tp );

	switch( tp->tr_op )
	{
		case OP_UNION:
		case OP_SUBTRACT:
		case OP_INTERSECT:
			BU_GETUNION( eptr, E_tree );
			eptr->magic = E_TREE_MAGIC;
			eptr->n.op = tp->tr_op;
			eptr->n.left = build_etree( tp->tr_b.tb_left, dgcdp );
			eptr->n.right = build_etree( tp->tr_b.tb_right, dgcdp );
			break;
		case OP_SOLID:
			stp = tp->tr_a.tu_stp;
			eptr = add_solid( stp->st_dp, stp->st_matp, dgcdp );
			eptr->l.op = tp->tr_op;
			BU_LIST_INIT( &eptr->l.seghead );
			break;
		case OP_DB_LEAF:
			if( (dp=db_lookup( dgcdp->dgop->dgo_wdbp->dbip, tp->tr_l.tl_name, LOOKUP_NOISY )) == DIR_NULL )
			{
				eptr->l.m = (struct model *)NULL;
				break;
			}
			eptr = add_solid( dp, tp->tr_l.tl_mat, dgcdp );
			eptr->l.op = tp->tr_op;
			BU_LIST_INIT( &eptr->l.seghead );
			break;
		case OP_NOP:
			/* add a NULL solid  */
			BU_GETUNION( eptr, E_tree );
			eptr->magic = E_TREE_MAGIC;
			eptr->l.m = (struct model *)NULL;
			break;
		default:
			bu_bomb("build_etree() Unknown tr_op\n");
	}
	return( eptr );
}

/* a handy routine (for debugging) that prints asegment list */
void
show_seg(struct bu_list *seg, int str)
{
	struct seg *ptr;

	if( !seg )
		bu_log( "%s - NULL seg list\n", str );
	else
	{
		if( BU_LIST_IS_EMPTY( seg ) )
			bu_log( "%s - empty\n", str );
		else
		{
			bu_log( "%s:\n", str );
			for( BU_LIST_FOR( ptr, seg, seg ) )
			{
				if( ptr->seg_stp == ON_SURF )
					bu_log( "\t %g to %g (ON_SURF)\n", ptr->seg_in.hit_dist, ptr->seg_out.hit_dist );
				else if( ptr->seg_stp == ON_INT )
					bu_log( "\t %g to %g (ON_INT)\n", ptr->seg_in.hit_dist, ptr->seg_out.hit_dist );
				else if( ptr->seg_stp == IN_SOL )
					bu_log( "\t %g to %g (IN)\n", ptr->seg_in.hit_dist, ptr->seg_out.hit_dist );
				else
					bu_log( "\t %g to %g (?)\n", ptr->seg_in.hit_dist, ptr->seg_out.hit_dist );
				bu_log( "\t\t( %g %g %g) <-> (%g %g %g)\n", V3ARGS( ptr->seg_in.hit_point),
					V3ARGS( ptr->seg_out.hit_point ) );
			}
		}
	}
}

/* given a segment list, eliminate any overlaps in the segments */
HIDDEN  void
eliminate_overlaps(struct bu_list		*seghead,
		   struct dg_client_data	*dgcdp)
{
	struct seg *a, *b, *nextb;

	a = BU_LIST_FIRST( seg, seghead );
	while( BU_LIST_NOT_HEAD( &a->l, seghead ) )
	{
		b = BU_LIST_PNEXT( seg, &a->l );
		if( BU_LIST_IS_HEAD( &b->l, seghead ) )
			break;

		while( BU_LIST_NOT_HEAD( &b->l, seghead ) )
		{
			nextb = BU_LIST_PNEXT( seg, &b->l );
			if( NOT_SEG_OVERLAP( a, b ) )
				break;

			if( b->seg_in.hit_dist < a->seg_out.hit_dist )
			{
				if( b->seg_out.hit_dist > a->seg_out.hit_dist )
					a->seg_out.hit_dist = b->seg_out.hit_dist;

				BU_LIST_DEQUEUE( &b->l );
				RT_FREE_SEG( b, dgcdp->ap->a_resource );
				b = nextb;
				continue;
			}

			b = nextb;
		}

		a = BU_LIST_PNEXT(seg, &a->l );
	}
}

/* perform the intersection of two segments
 * the result is assigned the provided type
 */
HIDDEN void
do_intersect(struct seg			*A,
	     struct seg			*B,
	     struct bu_list		*seghead,
	     struct soltab		*type,
	     struct dg_client_data	*dgcdp)
{
	struct seg *tmp=(struct seg *)NULL;

	if( NOT_SEG_OVERLAP( A, B ) )
		return;

	RT_GET_SEG( tmp, dgcdp->ap->a_resource );
	if( A->seg_in.hit_dist <= B->seg_in.hit_dist )
	{
		if( B->seg_out.hit_dist <= A->seg_out.hit_dist )
		{
			*tmp = *B;
			tmp->seg_stp = type;
		}
		else
		{
			tmp->seg_in.hit_dist = B->seg_in.hit_dist;
			tmp->seg_out.hit_dist = A->seg_out.hit_dist;
			tmp->seg_stp = type;
		}
	}
	else
	{
		if( B->seg_out.hit_dist >= A->seg_out.hit_dist )
		{
			*tmp = *A;
			tmp->seg_stp = type;
		}
		else
		{
			tmp->seg_in.hit_dist = A->seg_in.hit_dist;
			tmp->seg_out.hit_dist = B->seg_out.hit_dist;
			tmp->seg_stp = type;
		}
	}
	if( tmp )
		BU_LIST_INSERT( seghead, &tmp->l )
	return;
}

/* perform the subtraction of one segment from another
 * the result is assigned the type from segment A
 */
HIDDEN void
do_subtract(struct seg			*A,
	    struct seg			*B,
	    struct bu_list		*seghead,
	    struct dg_client_data	*dgcdp)
{
	struct seg *tmp=(struct seg *)NULL;


	if( NOT_SEG_OVERLAP( A, B ) )
	{
		RT_GET_SEG( tmp, dgcdp->ap->a_resource )
		*tmp = *A;
		BU_LIST_INSERT( seghead, &tmp->l )
		return;
	}

	if( B->seg_in.hit_dist<= A->seg_in.hit_dist )
	{
		if( B->seg_out.hit_dist < A->seg_out.hit_dist )
		{
			RT_GET_SEG( tmp, dgcdp->ap->a_resource )
			*tmp = *A;
			tmp->seg_in.hit_dist = B->seg_out.hit_dist;
			BU_LIST_INSERT( seghead, &tmp->l )
			return;
		}
		else
			return;
	}
	else
	{
		if( B->seg_out.hit_dist >= A->seg_out.hit_dist )
		{
			RT_GET_SEG( tmp, dgcdp->ap->a_resource )
			*tmp = *A;
			tmp->seg_out.hit_dist = B->seg_in.hit_dist;
			BU_LIST_INSERT( seghead, &tmp->l )
			return;
		}
		else
		{
			RT_GET_SEG( tmp, dgcdp->ap->a_resource );
			tmp->seg_in.hit_dist = A->seg_in.hit_dist;
			tmp->seg_out.hit_dist = B->seg_in.hit_dist;
			tmp->seg_stp = A->seg_stp;
			BU_LIST_INSERT( seghead, &tmp->l )
			RT_GET_SEG( tmp, dgcdp->ap->a_resource );
			tmp->seg_in.hit_dist = B->seg_out.hit_dist;
			tmp->seg_out.hit_dist = A->seg_out.hit_dist;
			tmp->seg_stp = A->seg_stp;
			BU_LIST_INSERT( seghead, &tmp->l )
			return;
		}
	}
}

/* perform the union of two segments
 * the types of A and B should be the same
 */
HIDDEN void
do_union(struct seg		*A,
	 struct seg		*B,
	 struct bu_list		*seghead,
	 struct dg_client_data	*dgcdp)
{
	struct seg *tmp;

	RT_GET_SEG( tmp, dgcdp->ap->a_resource )

	if( NOT_SEG_OVERLAP( A, B ) )
	{
		if( A->seg_in.hit_dist <= B->seg_in.hit_dist )
		{
			*tmp = *A;
			BU_LIST_INSERT( seghead, &tmp->l )
			RT_GET_SEG( tmp, dgcdp->ap->a_resource )
			*tmp = *B;
			BU_LIST_INSERT( seghead, &tmp->l )
		}
		else
		{
			*tmp = *B;
			BU_LIST_INSERT( seghead, &tmp->l )
			RT_GET_SEG( tmp, dgcdp->ap->a_resource )
			*tmp = *A;
			BU_LIST_INSERT( seghead, &tmp->l )
		}
		return;
	}

	if( A->seg_in.hit_dist <= B->seg_in.hit_dist )
	{
		*tmp = *A;
		if( B->seg_out.hit_dist > A->seg_out.hit_dist )
			tmp->seg_out.hit_dist = B->seg_out.hit_dist;
	}
	else
	{
		*tmp = *B;
		if( A->seg_out.hit_dist > B->seg_out.hit_dist )
			tmp->seg_out.hit_dist = B->seg_out.hit_dist;
	}

	BU_LIST_INSERT( seghead, &tmp->l )
}

HIDDEN void
promote_ints(struct bu_list		*head,
	     struct dg_client_data	*dgcdp)
{
	struct seg *a, *b, *tmp;

	#ifdef debug
	bu_log( "In promote_ints():\n" );
	show_seg( head, "SEGS" );
	for( BU_LIST_FOR( a, seg, head ) )
	{
		b = BU_LIST_PNEXT( seg, &a->l );
		if( BU_LIST_IS_HEAD( &b->l, head ) )
			break;

		if( b->seg_in.hit_dist < a->seg_in.hit_dist )
			bu_log( "\tsegments out of order:\n" );
	}
	#endif

	a = BU_LIST_FIRST( seg, head );
	while( BU_LIST_NOT_HEAD( &a->l, head ) )
	{
		b = BU_LIST_PNEXT( seg, &a->l );
		while( BU_LIST_NOT_HEAD( &b->l, head ) )
		{
			if( a->seg_stp == ON_INT && b->seg_stp == ON_SURF )
			{
				if( NOT_SEG_OVERLAP( a, b ) )
				{
					b = BU_LIST_PNEXT( seg, &b->l );
					continue;
				}

				if( a->seg_in.hit_dist == b->seg_in.hit_dist &&
				    a->seg_out.hit_dist == b->seg_out.hit_dist )
				{
					a->seg_stp = ON_SURF;
					tmp = b;
					b = BU_LIST_PNEXT( seg, &b->l );
					BU_LIST_DEQUEUE( &tmp->l )
					RT_FREE_SEG( tmp, dgcdp->ap->a_resource )
					continue;;
				}

				if( a->seg_out.hit_dist == b->seg_out.hit_dist )
					a->seg_out.hit_dist = b->seg_in.hit_dist;
				else if( a->seg_out.hit_dist < b->seg_out.hit_dist )
				{
					if( b->seg_in.hit_dist > a->seg_in.hit_dist )
						a->seg_out.hit_dist = b->seg_in.hit_dist;
					else
					{
						tmp = a;
						a  = BU_LIST_PLAST( seg, &a->l );
						BU_LIST_DEQUEUE( &tmp->l )
						RT_FREE_SEG( tmp, dgcdp->ap->a_resource )
						break;
					}
				}
				else if( a->seg_in.hit_dist == b->seg_in.hit_dist )
				{
					fastf_t tmp_dist;

					tmp_dist = a->seg_out.hit_dist;
					a->seg_out.hit_dist = b->seg_out.hit_dist;
					b->seg_in.hit_dist = a->seg_out.hit_dist;
					b->seg_out.hit_dist = tmp_dist;
					a->seg_stp = ON_SURF;
					b->seg_stp = ON_INT;
				}
				else
				{
					RT_GET_SEG( tmp, dgcdp->ap->a_resource )
					*tmp = *a;
					tmp->seg_in.hit_dist = b->seg_out.hit_dist;
					a->seg_out.hit_dist = b->seg_in.hit_dist;
					BU_LIST_APPEND( &b->l, &tmp->l )
				}
			}
			else if( b->seg_stp == ON_INT && a->seg_stp == ON_SURF )
			{
				if( NOT_SEG_OVERLAP( b, a ) )
				{
					b = BU_LIST_PNEXT( seg, &b->l );
					continue;
				}

				if( b->seg_in.hit_dist == a->seg_in.hit_dist &&
				    b->seg_out.hit_dist == a->seg_out.hit_dist )
				{
					b->seg_stp = ON_SURF;
					tmp = a;
					a = BU_LIST_PLAST( seg, &a->l );
					BU_LIST_DEQUEUE( &tmp->l )
					RT_FREE_SEG( tmp, dgcdp->ap->a_resource )
					break;
				}

				if( b->seg_out.hit_dist == a->seg_out.hit_dist )
				{
					tmp = b;
					b = BU_LIST_PNEXT( seg, &b->l );
					BU_LIST_DEQUEUE( &tmp->l )
					RT_FREE_SEG( tmp, dgcdp->ap->a_resource )
				}
				else if( b->seg_out.hit_dist < a->seg_out.hit_dist )
				{
					if( a->seg_in.hit_dist > b->seg_in.hit_dist )
						b->seg_out.hit_dist = a->seg_in.hit_dist;
					else
					{
						tmp = b;
						b = BU_LIST_PNEXT( seg, &b->l );
						BU_LIST_DEQUEUE( &tmp->l )
						RT_FREE_SEG( tmp, dgcdp->ap->a_resource )
						continue;
					}
				}
				else if( b->seg_in.hit_dist == a->seg_in.hit_dist )
					b->seg_in.hit_dist = a->seg_out.hit_dist;
				else
				{
					RT_GET_SEG( tmp, dgcdp->ap->a_resource )
					*tmp = *b;
					tmp->seg_in.hit_dist = a->seg_out.hit_dist;
					b->seg_out.hit_dist = a->seg_in.hit_dist;
					BU_LIST_APPEND( &a->l, &tmp->l )
				}
			}

			if( (a->seg_stp != ON_INT) || (b->seg_stp != ON_INT) )
			{
				b = BU_LIST_PNEXT( seg, &b->l );
				continue;
			}

			if( NOT_SEG_OVERLAP( a, b ) )
			{
				b = BU_LIST_PNEXT( seg, &b->l );
				continue;
			}

			#ifdef debug
			bu_log( "\tfound overlapping ON_INT segs:\n" );
			#endif

			if( a->seg_in.hit_dist == b->seg_in.hit_dist &&
			    a->seg_out.hit_dist == b->seg_out.hit_dist )
			{
				#ifdef debug
				bu_log( "Promoting A, eliminating B\n" );
				#endif

				a->seg_stp = ON_SURF;
				BU_LIST_DEQUEUE( &b->l )
				RT_FREE_SEG( b, dgcdp->ap->a_resource )
				break;
			}

			if( a->seg_out.hit_dist == b->seg_out.hit_dist )
			{
				b->seg_stp = ON_SURF;
				a->seg_out.hit_dist = b->seg_in.hit_dist;

				#ifdef debug
				bu_log( "Promoting B, reducing A:\n" );
				#endif
			}
			else if( a->seg_out.hit_dist < b->seg_out.hit_dist )
			{
				if( b->seg_in.hit_dist > a->seg_in.hit_dist )
				{
					RT_GET_SEG( tmp, dgcdp->ap->a_resource )
					tmp->seg_stp = ON_SURF;
					tmp->seg_in.hit_dist = b->seg_in.hit_dist;
					tmp->seg_out.hit_dist = a->seg_out.hit_dist;
					b->seg_in.hit_dist = a->seg_out.hit_dist;
					a->seg_out.hit_dist = tmp->seg_in.hit_dist;
					BU_LIST_INSERT( &b->l, &tmp->l )

					#ifdef debug
					bu_log( "--==__ overlap\n" );
					#endif
				}
				else
				{
					b->seg_in.hit_dist = a->seg_out.hit_dist;
					a->seg_stp = ON_SURF;

					#ifdef debug
					bu_log( "A within B\n" );
					#endif
				}
			}
			else
			{
				if( a->seg_in.hit_dist == b->seg_in.hit_dist )
				{
					fastf_t tmp_dist;

					tmp_dist = a->seg_out.hit_dist;
					a->seg_out.hit_dist = b->seg_out.hit_dist;
					a->seg_stp = ON_SURF;
					b->seg_in.hit_dist = a->seg_out.hit_dist;
					b->seg_out.hit_dist = tmp_dist;
				}
				else
				{
					RT_GET_SEG( tmp, dgcdp->ap->a_resource )
					*tmp = *a;
					tmp->seg_in.hit_dist = b->seg_out.hit_dist;
					a->seg_out.hit_dist = b->seg_in.hit_dist;
					b->seg_stp = ON_SURF;
					BU_LIST_APPEND( &b->l, &tmp->l )

					#ifdef debug
					bu_log( "B within A:\n" );
					#endif
				}
			}
			b = BU_LIST_PNEXT( seg, &b->l );
		}
		a = BU_LIST_PNEXT( seg, &a->l );
	}

	#ifdef debug
	bu_log( "Results of promote_ints()\n" );
	show_seg( head, "SEGS" );
	#endif
}

/* Evaluate an operation on the operands (segment lists) */
HIDDEN struct bu_list *
eval_op(struct bu_list		*A,
	int			op,
	struct bu_list		*B,
	struct dg_client_data	*dgcdp)
{
	struct seg *sega, *segb, *tmp, *next;
	struct bu_list ret, ons, ins;
	int inserted;

	BU_LIST_INIT( &ret );

	#ifdef debug
	bu_log( "In eval_op:\n" );
	show_seg( A, "\tA:" );
	show_seg( B, "\tB:" );
	#endif

	switch( op )
	{
		case OP_SUBTRACT:

			#ifdef debug
			bu_log( "\t\tSUBTACT\n" );
			#endif

			if( BU_LIST_IS_EMPTY( A ) )
			{
				MY_FREE_SEG_LIST( B, dgcdp->ap->a_resource );
				bu_free( (char *)B, "bu_list" );

				#ifdef debug
				show_seg( A, "Returning" );
				#endif

				return( A );
			}
			else if( BU_LIST_IS_EMPTY( B ) )
			{
				bu_free( (char *)B, "bu_list" );

				#ifdef debug
				show_seg( A, "Returning" );
				#endif

				return( A );
			}

			/* A - B:
			 *	keep segments:
			 *			ON_A - IN_B
			 * 			ON_A - ON_B
			 *			ON_B + IN_A
			 *			IN_A - IN_B
			 */
			for( BU_LIST_FOR( sega, seg, A ) )
			{
				for( BU_LIST_FOR( segb, seg, B ) )
				{
					if( sega->seg_stp == ON_INT && segb->seg_stp == ON_INT )
						do_intersect( sega, segb, &ret, ON_SURF, dgcdp );
					else if( sega->seg_stp == ON_SURF || sega->seg_stp == ON_INT )
					{
						do_subtract( sega, segb, &ret, dgcdp );
					}
					else if( segb->seg_stp == ON_SURF ||  segb->seg_stp == ON_INT )
						do_intersect( segb, sega, &ret, segb->seg_stp, dgcdp );
				}
			}
			MY_FREE_SEG_LIST( B, dgcdp->ap->a_resource );
			bu_free( (char *)B, "bu_list" );
			MY_FREE_SEG_LIST( A, dgcdp->ap->a_resource );
			BU_LIST_INSERT_LIST( A, &ret )

			#ifdef debug
			show_seg( A, "Returning" );
			#endif

			return( A );
		case OP_INTERSECT:

			#ifdef debug
			bu_log( "\t\tINTERSECT\n" );
			#endif

			if( BU_LIST_IS_EMPTY( A ) || BU_LIST_IS_EMPTY( B ) )
			{
				MY_FREE_SEG_LIST( A, dgcdp->ap->a_resource );
				MY_FREE_SEG_LIST( B, dgcdp->ap->a_resource );
				bu_free( (char *)B, "bu_list" );

				#ifdef debug
				show_seg( A, "Returning" );
				#endif

				return( A );
			}
			/* A + B
			 *	This is merely the intersection of segments from A with those from B
			 *	The two different calls to "do_intersect" get the types (IN, ON) right
			 */
			for( BU_LIST_FOR( sega, seg, A ) )
			{
				for( BU_LIST_FOR( segb, seg, B ) )
				{
					if( sega->seg_stp == ON_INT && segb->seg_stp == ON_INT )
						do_intersect( sega, segb, &ret, ON_SURF, dgcdp );
					else if( sega->seg_stp == ON_SURF || sega->seg_stp == ON_INT )
						do_intersect( sega, segb, &ret, sega->seg_stp, dgcdp );
					else
						do_intersect( segb, sega, &ret, segb->seg_stp, dgcdp );
				}
			}
			MY_FREE_SEG_LIST( B, dgcdp->ap->a_resource );
			bu_free( (char *)B, "bu_list" );
			MY_FREE_SEG_LIST( A, dgcdp->ap->a_resource );
			BU_LIST_INSERT_LIST( A, &ret )

			#ifdef debug
			show_seg( A, "Returning" );
			#endif

			return( A );
		case OP_UNION:

			#ifdef debug
			bu_log( "\t\tUNION\n" );
			#endif

			if( BU_LIST_IS_EMPTY( A ) )
			{
				bu_free( (char *)A, "bu_list" );

				#ifdef debug
				show_seg( B, "Returning B (A is empty)" );
				#endif

				return( B );
			}
			if( BU_LIST_IS_EMPTY( B ) )
			{
				bu_free( (char *)B, "bu_list" );

				#ifdef debug
				show_seg( A, "Returning A (B is empty)" );
				#endif

				return( A );
			}
			/* A u B:
			 *	keep segments:
			 *		ON_A - IN_B (ON)
			 *		IN_B + ON_A (IN )
			 *		ON_B - IN_A (ON)
			 *		IN_A + ON_B (IN)
			 * 		all remaining unique ON or IN segments
			 */

			/* create two new lists, one with all the ON segments,
			 * the other with all the IN segments
			 */
			BU_LIST_INIT( &ons )
			BU_LIST_INIT( &ins )

			/* Put the A operand segments on the lists */
			while( BU_LIST_WHILE( sega, seg, A ) )
			{
				BU_LIST_DEQUEUE( &sega->l )

				if( sega->seg_stp == ON_SURF || sega->seg_stp == ON_INT )
					BU_LIST_INSERT( &ons, &sega->l )
				else
					BU_LIST_INSERT( &ins, &sega->l )
			}

			/* insert the B operand segments in the lists (maintaining order from smaller starting
			 * hit distance to larger
			 */
			while( BU_LIST_WHILE( segb, seg, B ) )
			{
				int inserted;
				BU_LIST_DEQUEUE( &segb->l )

				if( segb->seg_stp == IN_SOL )
				{
					inserted = 0;
					for( BU_LIST_FOR( tmp, seg, &ins ) )
					{
						if( tmp->seg_in.hit_dist >= segb->seg_in.hit_dist )
						{
							inserted = 1;
							BU_LIST_INSERT( &tmp->l, &segb->l )
							break;
						}
					}
					if( !inserted )
						BU_LIST_INSERT( &ins, &segb->l )
				}
				else
				{
					inserted = 0;
					for( BU_LIST_FOR( tmp, seg, &ons ) )
					{
						if( tmp->seg_in.hit_dist >= segb->seg_in.hit_dist )
						{
							inserted = 1;
							BU_LIST_INSERT( &tmp->l, &segb->l )
							break;
						}
					}
					if( !inserted )
						BU_LIST_INSERT( &ons, &segb->l )
				}
			}

			/* promote intersecting ON_INT's to ON_SURF */
			promote_ints( &ons, dgcdp );

			/* make sure the segments are unique */
			eliminate_overlaps( &ins, dgcdp );
			eliminate_overlaps( &ons, dgcdp );

			#ifdef debug
			show_seg( &ons, "ONS" );
			show_seg( &ins, "INS" );
			#endif

			/* subtract INS from ONS */
			#ifdef debug
			bu_log( "doing subtraction:\n" );
			#endif
			sega = BU_LIST_FIRST( seg, &ons );
			while( BU_LIST_NOT_HEAD( &sega->l, &ons ) )
			{
				next = BU_LIST_PNEXT( seg, &sega->l );

				#ifdef debug
				bu_log( "A is %g to %g:\n", sega->seg_in.hit_dist, sega->seg_out.hit_dist );
				#endif

				for( BU_LIST_FOR( segb, seg, &ins ) )
				{
					#ifdef debug
					bu_log( "\tcomparing to B %g to %g\n", segb->seg_in.hit_dist, segb->seg_out.hit_dist );
					#endif

					if( NOT_SEG_OVERLAP( sega, segb ) )
					{
						#ifdef debug
						bu_log( "\t\tNo overlap!!\n" );
						#endif

						continue;
					}

					if( segb->seg_in.hit_dist <= sega->seg_in.hit_dist &&
					    segb->seg_out.hit_dist >= sega->seg_out.hit_dist )
					{
						#ifdef debug
						bu_log( "\t\teliminating A\n" );
						#endif

						/* eliminate sega */
						BU_LIST_DEQUEUE( &sega->l )
						RT_FREE_SEG( sega, dgcdp->ap->a_resource )
						sega = next;
						break;
					}

					if( segb->seg_in.hit_dist > sega->seg_in.hit_dist &&
					    segb->seg_out.hit_dist < sega->seg_out.hit_dist )
					{
						/* split sega */
						RT_GET_SEG( tmp, dgcdp->ap->a_resource )
						*tmp = *sega;
						tmp->seg_in.hit_dist = segb->seg_out.hit_dist;
						sega->seg_out.hit_dist = segb->seg_in.hit_dist;
						BU_LIST_APPEND( &sega->l, &tmp->l )
						next = tmp;

						#ifdef debug
						bu_log( "\t\tsplit A into: %g to %g and %g to %g\n", sega->seg_in.hit_dist, sega->seg_out.hit_dist, tmp->seg_in.hit_dist, tmp->seg_out.hit_dist );
						#endif
					}
					else
					{
						/* subtract edges */
						if( segb->seg_in.hit_dist > sega->seg_in.hit_dist )
							sega->seg_out.hit_dist = segb->seg_in.hit_dist;
						if( segb->seg_out.hit_dist < sega->seg_out.hit_dist )
							sega->seg_in.hit_dist = segb->seg_out.hit_dist;

						#ifdef debug
						bu_log( "\t\tsubtracted A down to %g to %g\n", sega->seg_in.hit_dist, sega->seg_out.hit_dist );
						#endif
					}
				}
				sega = next;
			}

			/* put the resuling ONS list on the result list */
			BU_LIST_INSERT_LIST( &ret, &ons )

			/* add INS to the return list (maintain order) */
			while( BU_LIST_WHILE( sega, seg, &ins ) )
			{
				BU_LIST_DEQUEUE( &sega->l )

				inserted = 0;
				for( BU_LIST_FOR( segb, seg, &ret ) )
				{
					if( sega->seg_in.hit_dist < segb->seg_in.hit_dist )
					{
						BU_LIST_INSERT( &segb->l, &sega->l )
						inserted = 1;
						break;
					}
				}

				if( !inserted )
					BU_LIST_INSERT( &ret, &sega->l )
			}

			MY_FREE_SEG_LIST( B, dgcdp->ap->a_resource );
			bu_free( (char *)B, "bu_list" );
			MY_FREE_SEG_LIST( A, dgcdp->ap->a_resource );
			BU_LIST_INSERT_LIST( A, &ret )

			#ifdef debug
			show_seg( A, "Returning" );
			#endif

			return( A );
	}

	/* should never get here */
	MY_FREE_SEG_LIST( A, dgcdp->ap->a_resource );
	MY_FREE_SEG_LIST( B, dgcdp->ap->a_resource );
	bu_free( (char *)B, "bu_list" );

	#ifdef debug
	show_seg( A, "Returning (default)" );
	#endif

	return( A );

}

/* evaluate an E-tree */
HIDDEN struct bu_list *
eval_etree(union E_tree			*eptr,
	   struct dg_client_data	*dgcdp)

{
	struct bu_list *A, *B;

	CK_ETREE( eptr );

	#ifdef debug
	bu_log( "In eval_etree:\n" );
	#endif

	switch( eptr->l.op )
	{
		case OP_DB_LEAF:
		case OP_SOLID:
			A = (struct bu_list *)bu_malloc( sizeof( struct bu_list ), "bu_list" );
			BU_LIST_INIT( A );
			BU_LIST_INSERT_LIST( A, &eptr->l.seghead )

			#ifdef debug
			show_seg( A, "LEAF:" );
			#endif

			return( A );
		case OP_SUBTRACT:
		case OP_INTERSECT:
		case OP_UNION:
			#ifdef debug
			bu_log( "Evaluating subtrees\n" );
			#endif

			A = eval_etree( eptr->n.left, dgcdp );
			B = eval_etree( eptr->n.right, dgcdp );
			return( eval_op( A, eptr->n.op, B, dgcdp ) );
	}

	/* should never get here */
	return( (struct bu_list *)NULL );	/* for the compilers */
}

HIDDEN void
inverse_dir( vect_t dir, vect_t inv_dir )
{
	/* Compute the inverse of the direction cosines */
	if( !NEAR_ZERO( dir[X], SQRT_SMALL_FASTF ) )  {
		inv_dir[X]=1.0/dir[X];
	} else {
		inv_dir[X] = INFINITY;
		dir[X] = 0.0;
	}
	if( !NEAR_ZERO( dir[Y], SQRT_SMALL_FASTF ) )  {
		inv_dir[Y]=1.0/dir[Y];
	} else {
		inv_dir[Y] = INFINITY;
		dir[Y] = 0.0;
	}
	if( !NEAR_ZERO( dir[Z], SQRT_SMALL_FASTF ) )  {
		inv_dir[Z]=1.0/dir[Z];
	} else {
		inv_dir[Z] = INFINITY;
		dir[Z] = 0.0;
	}
}

HIDDEN struct soltab *
classify_seg( struct seg *seg, struct soltab *shoot, struct xray *rp, struct dg_client_data *dgcdp )
{
	fastf_t mid_dist;
	struct xray new_rp;
	struct ray_data rd;
	struct soltab *ret = IN_SOL;

	bzero( &rd, sizeof( struct ray_data ) );

	BU_GETSTRUCT( rd.seghead, seg );
	BU_LIST_INIT( &rd.seghead->l );

	mid_dist = (seg->seg_in.hit_dist + seg->seg_out.hit_dist) / 2.0;
	VJOIN1( new_rp.r_pt, rp->r_pt, mid_dist, rp->r_dir );
#ifdef debug
	bu_log( "Classifying segment with mid_pt (%g %g %g) with respct to %s\n", V3ARGS( new_rp.r_pt ), shoot->st_dp->d_namep );
#endif

	bn_vec_ortho( new_rp.r_dir, rp->r_dir );
	inverse_dir( new_rp.r_dir, rd.rd_invdir );

	/* set up "ray_data" structure for nmg raytrace */
	rd.rp = &new_rp;
	rd.tol = &dgcdp->dgop->dgo_wdbp->wdb_tol;
	rd.ap = dgcdp->ap;
	rd.magic = NMG_RAY_DATA_MAGIC;
	rd.classifying_ray = 0;
	rd.hitmiss = (struct hitmiss **)NULL;
	rd.stp = shoot;

	if( rt_functab[shoot->st_id].ft_shot( shoot, &new_rp, dgcdp->ap, rd.seghead ) ) {
		struct seg *seg;

		while( BU_LIST_WHILE( seg, seg, &rd.seghead->l ) ) {
			BU_LIST_DEQUEUE( &seg->l );
#ifdef debug
			bu_log( "dist = %g and %g\n", seg->seg_in.hit_dist,
				seg->seg_out.hit_dist );
#endif
			if( ret != ON_SURF ) {
				if( NEAR_ZERO( seg->seg_in.hit_dist, rd.tol->dist ) ) {
					ret = ON_SURF;
				}
				if( NEAR_ZERO( seg->seg_out.hit_dist, rd.tol->dist ) ) {
					ret = ON_SURF;
				}
			}
			RT_FREE_SEG( seg, dgcdp->ap->a_resource );
		}
	}

	if( ret != ON_SURF ) {
		vect_t new_dir;

		VCROSS( new_dir, new_rp.r_dir, rp->r_dir );
		VMOVE( new_rp.r_dir, new_dir );
		inverse_dir( new_rp.r_dir, rd.rd_invdir );
		if( rt_functab[shoot->st_id].ft_shot( shoot, &new_rp, dgcdp->ap, rd.seghead ) ) {
			struct seg *seg;

			while( BU_LIST_WHILE( seg, seg, &rd.seghead->l ) ) {
				BU_LIST_DEQUEUE( &seg->l );
#ifdef debug
				bu_log( "dist = %g and %g\n", seg->seg_in.hit_dist,
					seg->seg_out.hit_dist );
#endif
				if( ret != ON_SURF ) {
					if( NEAR_ZERO( seg->seg_in.hit_dist, rd.tol->dist ) ) {
						ret = ON_SURF;
					}
					if( NEAR_ZERO( seg->seg_out.hit_dist, rd.tol->dist ) ) {
						ret = ON_SURF;
					}
				}
				RT_FREE_SEG( seg, dgcdp->ap->a_resource );
			}
		}
	}
#ifdef debug
	bu_log( "\t x%x\n", ret );
#endif
	return ret;
}

/* Shoot rays (corresponding to possible edges in the result)
 * at the solids, put the results in the E-tree leaves as type IN_SOL.
 * Call eval_etree() and plot the results
 */
HIDDEN void
shoot_and_plot(point_t			start_pt,
	       vect_t			dir,
	       struct bu_list		*vhead,
	       fastf_t			edge_len,
	       int			skip_leaf1,
	       int			skip_leaf2,
	       union E_tree		*eptr,
	       struct soltab		*type,
	       struct dg_client_data	*dgcdp)
{
	struct xray rp;
	struct ray_data rd;
	int shoot_leaf;
	struct bu_list *final_segs;

	if( bu_debug&BU_DEBUG_MEM_CHECK && bu_mem_barriercheck() )
		bu_log( "Error at start of shoot_and_plot()\n" );

	CK_ETREE( eptr );

	bzero( &rd, sizeof( struct ray_data ) );

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
	rd.tol = &dgcdp->dgop->dgo_wdbp->wdb_tol;
	rd.ap = dgcdp->ap;
	rd.magic = NMG_RAY_DATA_MAGIC;
	rd.classifying_ray = 0;
	rd.hitmiss = (struct hitmiss **)NULL;

	/* shoot this ray at every leaf solid except the one this edge came from
	 * (or the two that this intersection line came from
	 */
	for( shoot_leaf=0 ; shoot_leaf < BU_PTBL_END( &dgcdp->leaf_list ) ; shoot_leaf++ )
	{
		union E_tree *shoot;
		int dont_shoot=0;

		shoot = (union E_tree *)BU_PTBL_GET( &dgcdp->leaf_list, shoot_leaf );

		if( BU_LIST_NON_EMPTY( &shoot->l.seghead ) )
		{
			MY_FREE_SEG_LIST( &shoot->l.seghead, dgcdp->ap->a_resource );
		}
		BU_LIST_INIT( &shoot->l.seghead );

		/* don't shoot rays at the leaves that were the source of this possible edge */
		if( shoot_leaf == skip_leaf1 || shoot_leaf == skip_leaf2 )
			dont_shoot = 1;
		else
		{
			/* don't shoot at duplicate solids either */
			union E_tree *leaf;

			if( skip_leaf1 >= 0 )
			{
				leaf = (union E_tree *)BU_PTBL_GET( &dgcdp->leaf_list, skip_leaf1 );
				if( leaf->l.stp->st_dp == shoot->l.stp->st_dp )
				{
					if( !leaf->l.stp->st_matp && !shoot->l.stp->st_matp )
						dont_shoot = 1;
					else if( !leaf->l.stp->st_matp &&
						 bn_mat_is_equal( shoot->l.stp->st_matp,
								  bn_mat_identity,
								  &dgcdp->dgop->dgo_wdbp->wdb_tol ) )
						dont_shoot = 1;
					else if( !shoot->l.stp->st_matp &&
						 bn_mat_is_equal( leaf->l.stp->st_matp,
								  bn_mat_identity,
								  &dgcdp->dgop->dgo_wdbp->wdb_tol ) )
						dont_shoot = 1;
					else if( leaf->l.stp->st_matp &&
						 shoot->l.stp->st_matp &&
						 bn_mat_is_equal( leaf->l.stp->st_matp,
								  shoot->l.stp->st_matp,
								  &dgcdp->dgop->dgo_wdbp->wdb_tol ) )
						dont_shoot = 1;
				}
			}
			if( !dont_shoot && skip_leaf2 >= 0 )
			{
				leaf = (union E_tree *)BU_PTBL_GET( &dgcdp->leaf_list, skip_leaf2 );
				if( leaf->l.stp->st_dp == shoot->l.stp->st_dp )
				{
					if( !leaf->l.stp->st_matp && !shoot->l.stp->st_matp )
						dont_shoot = 1;
					else if( !leaf->l.stp->st_matp &&
						 bn_mat_is_equal( shoot->l.stp->st_matp,
								  bn_mat_identity,
								  &dgcdp->dgop->dgo_wdbp->wdb_tol ) )
						dont_shoot = 1;
					else if( !shoot->l.stp->st_matp &&
						 bn_mat_is_equal( leaf->l.stp->st_matp,
								  bn_mat_identity,
								  &dgcdp->dgop->dgo_wdbp->wdb_tol ) )
						dont_shoot = 1;
					else if( leaf->l.stp->st_matp &&
						 shoot->l.stp->st_matp &&
						 bn_mat_is_equal( leaf->l.stp->st_matp,
								  shoot->l.stp->st_matp,
								  &dgcdp->dgop->dgo_wdbp->wdb_tol ) )
						dont_shoot = 1;
				}
			}
		}

		if( dont_shoot )
		{
			struct seg *seg;

			/* put entire edge in seg list and mark it as ON the surface */
			RT_GET_SEG( seg, dgcdp->ap->a_resource );
			seg->l.magic = RT_SEG_MAGIC;
			seg->seg_in.hit_dist = 0.0;
			seg->seg_out.hit_dist = edge_len;
			seg->seg_stp = type;
			BU_LIST_INSERT( &shoot->l.seghead, &seg->l );
			continue;
		}

		/* initialize the lists of things that have been hit/missed */
		rd.rd_m = shoot->l.m;
		BU_LIST_INIT(&rd.rd_hit);
		BU_LIST_INIT(&rd.rd_miss);

		rd.stp = shoot->l.stp;

		/* actually shoot the ray, assign segments to the leaf, and mark them as IN_SOL */
		if( rt_in_rpp( &rp, rd.rd_invdir, shoot->l.stp->st_min, shoot->l.stp->st_max ) )
		{
			if( rt_functab[shoot->l.stp->st_id].ft_shot( shoot->l.stp, &rp, dgcdp->ap, rd.seghead ) )
			{
				struct seg *seg;

				/* put the segments in the lead solid structure */
				while( BU_LIST_WHILE( seg, seg, &rd.seghead->l ) )
				{
					BU_LIST_DEQUEUE( &seg->l )
					/* clip segments to the edge being considered */
					if( seg->seg_in.hit_dist >= edge_len || seg->seg_out.hit_dist <= 0 )
						RT_FREE_SEG( seg, dgcdp->ap->a_resource )
					else
					{
						if( seg->seg_in.hit_dist < 0.0 )
							seg->seg_in.hit_dist = 0.0;
						if( seg->seg_out.hit_dist > edge_len )
							seg->seg_out.hit_dist = edge_len;
						seg->seg_stp = classify_seg( seg, shoot->l.stp, &rp, dgcdp );
						BU_LIST_INSERT(  &shoot->l.seghead, &seg->l )
					}
				}
			}
		}
	}

	/* Evaluate the Boolean tree to get the "final" segments
	 * which are to be plotted.
	 */
	#ifdef debug
	bu_log( "EVALUATING ETREE:\n" );
	bu_log( "ray start (%g %g %g), dir=(%g %g %g)\n", V3ARGS( start_pt ), V3ARGS( dir ) );
	#endif

	final_segs = eval_etree( eptr, dgcdp );

	#ifdef debug
	show_seg( final_segs, "DRAWING" );
	#endif

	if( final_segs )
	{
		struct seg *seg;

		/* add the segemnts to the VLIST */
		for( BU_LIST_FOR( seg, seg, final_segs ) )
		{
			point_t pt;

			/* only plot the resulting segments that are ON the SURFace */
			if( seg->seg_stp != ON_SURF )
				continue;

			dgcdp->nvectors++;
			VJOIN1( pt, rp.r_pt, seg->seg_in.hit_dist, rp.r_dir )

			#ifdef debug
			bu_log( "\t\tDRAW (%g %g %g)", V3ARGS( pt ) );
			#endif

			RT_ADD_VLIST( vhead, pt, RT_VLIST_LINE_MOVE );
			VJOIN1( pt, rp.r_pt, seg->seg_out.hit_dist, rp.r_dir )

			#ifdef debug
			bu_log( "<->(%g %g %g)\n", V3ARGS( pt ) );
			#endif

			RT_ADD_VLIST( vhead, pt, RT_VLIST_LINE_DRAW );
		}

	}

	if( final_segs )
		MY_FREE_SEG_LIST( final_segs, dgcdp->ap->a_resource );
	bu_free( (char *)final_segs, "bu_list" );

	if( bu_debug&BU_DEBUG_MEM_CHECK && bu_mem_barriercheck() )
		bu_log( "Error at end of shoot_and_plot()\n" );

}

#define HITS_BLOCK	20

HIDDEN void
Eplot(union E_tree		*eptr,
      struct bu_list		*vhead,
      struct dg_client_data	*dgcdp)
{
	point_t start_pt;
	int leaf_no;
	union E_tree *leaf_ptr;
	int hit_count1=0, hit_count2=0;
	point_t *hits1=NULL, *hits2=NULL;
	int hits_avail1=0, hits_avail2=0;
	int i;
	struct bu_list *result;
	struct bn_tol *tol;

	tol = &dgcdp->dgop->dgo_wdbp->wdb_tol;

	CK_ETREE( eptr );

	/* create an edge list for each leaf solid */
	for( leaf_no=0 ; leaf_no < BU_PTBL_END( &dgcdp->leaf_list ) ; leaf_no++ )
	{
		leaf_ptr = (union E_tree *)BU_PTBL_GET( &dgcdp->leaf_list, leaf_no );
		CK_ETREE( leaf_ptr );
		if( leaf_ptr->l.op != OP_DB_LEAF && leaf_ptr->l.op != OP_SOLID )
		{
			Tcl_AppendResult(dgcdp->interp, "Eplot: Bad leaf node!!!\n", (char *)NULL );
			return;
		}

		if( leaf_ptr->l.m )
			nmg_edge_tabulate( &leaf_ptr->l.edge_list, &leaf_ptr->l.m->magic );
		else
			bu_ptbl_init( &leaf_ptr->l.edge_list, 1, "edge_list" );
	}

	/* now plot appropriate parts of each solid */

	/* loop through every leaf solid */
	for( leaf_no=0 ; leaf_no < BU_PTBL_END( &dgcdp->leaf_list ) ; leaf_no++ )
	{
		int edge_no;

		leaf_ptr = (union E_tree *)BU_PTBL_GET( &dgcdp->leaf_list, leaf_no );

		if( !leaf_ptr->l.m )
			continue;

		/* do each edge of the current leaf solid */
		for( edge_no=0 ; edge_no < BU_PTBL_END( &leaf_ptr->l.edge_list ) ; edge_no++ )
		{
			struct edge *e;
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
			if( edge_len < tol->dist )
				continue;
			inv_len = 1.0/edge_len;
			VSCALE( dir, dir, inv_len );
			shoot_and_plot( vg->coord, dir, vhead, edge_len, leaf_no, -1, eptr, ON_SURF, dgcdp );

		}
	}

	hits1 = (point_t *)bu_calloc( HITS_BLOCK, sizeof( point_t ), "hits" );
	hits_avail1 = HITS_BLOCK;
	hits2 = (point_t *)bu_calloc( HITS_BLOCK, sizeof( point_t ), "hits" );
	hits_avail2 = HITS_BLOCK;

	/* Now draw solid intersection lines */
	for( leaf_no=0 ; leaf_no < BU_PTBL_END( &dgcdp->leaf_list ) ; leaf_no++ )
	{
		int leaf2;

		leaf_ptr = (union E_tree *)BU_PTBL_GET( &dgcdp->leaf_list, leaf_no );
		if( !leaf_ptr->l.m )
			continue;

		for( leaf2=leaf_no+1 ; leaf2 < BU_PTBL_END( &dgcdp->leaf_list ) ; leaf2++ )
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
			struct bu_list *A, *B;

			leaf2_ptr = (union E_tree *)BU_PTBL_GET( &dgcdp->leaf_list, leaf2 );
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
				NMG_GET_FU_PLANE( pl1, fu1 );

				for( BU_LIST_FOR( fu2, faceuse, &s2->fu_hd ) )
				{
					fastf_t dist;
					vect_t dir;
					vect_t diff;
					fastf_t *dists1, *dists2;
					fastf_t min_dist, max_dist;
					int	min_hit, max_hit;
					int done;
					struct seg *aseg;

					if( fu2->orientation != OT_SAME )
						continue;

					f2 = fu2->f_p;

					if ( !V3RPP_OVERLAP_TOL(f2->min_pt, f2->max_pt,
								f1->min_pt, f1->max_pt,
								tol) )
							continue;

					NMG_GET_FU_PLANE( pl2, fu2 );

					if( bn_coplanar( pl1, pl2, tol ) ) {
						continue;
					}

					hit_count1=0;
					hit_count2=0;
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

							if( bn_isect_line3_plane( &dist, vg1a->coord,
										  dir, pl2,
										  tol ) < 1 )
								continue;

							if( dist < -tol->dist || dist > 1.0 + tol->dist )
								continue;

							if( hit_count1 >= hits_avail1 ) {
								hits_avail1 += HITS_BLOCK;
								hits1 = (point_t *)bu_realloc( hits1,
									 hits_avail1 * sizeof( point_t ), "hits1" );
							}
							VJOIN1( hits1[hit_count1], vg1a->coord, dist, dir );
							hit_count1++;
						}
					}
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

							if( bn_isect_line3_plane( &dist, vg2a->coord,
										  dir, pl1,
										  tol ) < 1 )
								continue;

							if( dist < -tol->dist || dist > 1.0 + tol->dist )
								continue;

							if( hit_count2 >= hits_avail2 ) {
								hits_avail2 += HITS_BLOCK;
								hits2 = (point_t *)bu_realloc( hits2,
									 hits_avail2 * sizeof( point_t ), "hits2" );
							}
							VJOIN1( hits2[hit_count2], vg2a->coord, dist, dir );
							hit_count2++;
						}
					}

					if( hit_count1 < 2 || hit_count2 < 2 ) {
						/* nothing to plot */
						continue;
					}

					/* sort the hits on face 1 */
					dists1 = (fastf_t *)bu_calloc( hit_count1,
							 sizeof( fastf_t ), "dists1" );
					dists2 = (fastf_t *)bu_calloc( hit_count2,
							 sizeof( fastf_t ), "dists2" );
					VMOVE( start_pt, hits1[0] );
					dists1[0] = 0.0;
					min_dist = 0.0;
					min_hit = 0;
					VSUB2( dir, hits1[1], hits1[0] );
					dists1[1] = MAGNITUDE( dir );
					VUNITIZE( dir );
					max_dist = dists1[1];
					max_hit = 1;
					for( i=2 ; i<hit_count1 ; i++ ) {
						VSUB2( diff, hits1[i], start_pt );
						dists1[i] = MAGNITUDE( diff );
						if( VDOT( dir, diff ) < 0.0 )
							dists1[i] = -dists1[i];
						if( dists1[i] > max_dist ) {
							max_dist = dists1[i];
							max_hit = i;
						}
						if( dists1[i] < min_dist ) {
							min_dist = dists1[i];
							min_hit = i;
						}
					}

					/* recalculate dir */
					VSUB2( dir, hits1[max_hit], hits1[min_hit] );
					VUNITIZE( dir );

					done = 0;
					while( !done ) {
						done = 1;
						for( i=1 ; i<hit_count1 ; i++ ) {
							if( dists1[i-1] > dists1[i] ) {
								fastf_t tmp;
								point_t tmp_pt;

								done = 0;
								tmp = dists1[i];
								VMOVE( tmp_pt, hits1[i] );
								dists1[i] = dists1[i-1];
								VMOVE( hits1[i], hits1[i-1] );
								dists1[i-1] = tmp;
								VMOVE( hits1[i-1], tmp_pt );
							}
						}
					}

					/* sort the hits on face 2 */
					min_dist = MAX_FASTF;
					min_hit = -1;
					max_dist = -min_dist;
					max_hit = -1;
					for( i=0 ; i<hit_count2 ; i++ ) {
						VSUB2( diff, hits2[i], start_pt );
						dists2[i] = MAGNITUDE( diff );
						if( VDOT( dir, diff ) < 0.0 )
							dists2[i] = -dists2[i];
						if( dists2[i] > max_dist ) {
							max_dist = dists2[i];
							max_hit = i;
						}
						if( dists2[i] < min_dist ) {
							min_dist = dists2[i];
							min_hit = i;
						}
					}

					done = 0;
					while( !done ) {
						done = 1;
						for( i=1 ; i<hit_count2 ; i++ ) {
							if( dists2[i-1] > dists2[i] ) {
								fastf_t tmp;
								point_t tmp_pt;

								done = 0;
								tmp = dists2[i];
								VMOVE( tmp_pt, hits2[i] );
								dists2[i] = dists2[i-1];
								VMOVE( hits2[i], hits2[i-1] );
								dists2[i-1] = tmp;
								VMOVE( hits2[i-1], tmp_pt );
							}
						}
					}

					/* build a segment list for each solid */
					A = (struct bu_list *)bu_calloc( 1, sizeof( struct bu_list ), "A" );
					B = (struct bu_list *)bu_calloc( 1, sizeof( struct bu_list ), "B" );
					BU_LIST_INIT( A );
					BU_LIST_INIT( B );

					for( i=1 ; i<hit_count1 ; i += 2 ) {
						fastf_t diff;

						diff = dists1[i] - dists1[i-1];
						if( NEAR_ZERO( diff, tol->dist )) {
							continue;
						}
						RT_GET_SEG( aseg, dgcdp->ap->a_resource );
						aseg->l.magic = RT_SEG_MAGIC;
						aseg->seg_stp = ON_INT;
						VMOVE( aseg->seg_in.hit_point, hits1[i-1] );
						aseg->seg_in.hit_dist = dists1[i-1];
						VMOVE( aseg->seg_out.hit_point, hits1[i] );
						aseg->seg_out.hit_dist = dists1[i];

						BU_LIST_APPEND( A, &aseg->l );
					}

					for( i=1 ; i<hit_count2 ; i += 2 ) {
						fastf_t diff;

						diff = dists2[i] - dists2[i-1];
						if( NEAR_ZERO( diff, tol->dist )) {
							continue;
						}
						RT_GET_SEG( aseg, dgcdp->ap->a_resource );
						aseg->l.magic = RT_SEG_MAGIC;
						aseg->seg_stp = ON_INT;
						VMOVE( aseg->seg_in.hit_point, hits2[i-1] );
						aseg->seg_in.hit_dist = dists2[i-1];
						VMOVE( aseg->seg_out.hit_point, hits2[i] );
						aseg->seg_out.hit_dist = dists2[i];

						BU_LIST_APPEND( B, &aseg->l );
					}

					result = eval_op( A, OP_INTERSECT, B, dgcdp );

					for( BU_LIST_FOR( aseg, seg, result ) ) {
						point_t ray_start;

						VJOIN1( ray_start, start_pt, aseg->seg_in.hit_dist, dir );
						shoot_and_plot( ray_start, dir, vhead,
								aseg->seg_out.hit_dist - aseg->seg_in.hit_dist,
								leaf_no, leaf2, eptr, ON_INT, dgcdp );
					}
					MY_FREE_SEG_LIST( result, dgcdp->ap->a_resource );

					bu_free( (char *)dists1, "dists1" );
					bu_free( (char *)dists2, "dists2" );
				}
			}
		}
	}

	bu_free( (char *)hits1, "hits1" );
	bu_free( (char *)hits2, "hits2" );
}

HIDDEN void
free_etree(union E_tree			*eptr,
	   struct dg_client_data	*dgcdp)
{
	CK_ETREE( eptr );

	switch( eptr->l.op )
	{
		case OP_UNION:
		case OP_SUBTRACT:
		case OP_INTERSECT:
			free_etree( eptr->n.left, dgcdp );
			free_etree( eptr->n.right, dgcdp );
			bu_free( (char *)eptr, "node pointer" );
			break;
		case OP_DB_LEAF:
		case OP_SOLID:
			if( eptr->l.m && !eptr->l.do_not_free_model )
			{
				nmg_km( eptr->l.m );
				eptr->l.m = (struct model *)NULL;
			}
			if( BU_LIST_NON_EMPTY( &eptr->l.seghead ) )
			{
				MY_FREE_SEG_LIST( &eptr->l.seghead, dgcdp->ap->a_resource );
			}
			if( BU_LIST_NON_EMPTY( &eptr->l.edge_list.l ) )
			{
				bu_ptbl_free( &eptr->l.edge_list );
			}
			if( eptr->l.stp )
			{
				if( eptr->l.stp->st_specific )
					rt_functab[eptr->l.stp->st_id].ft_free( eptr->l.stp );
				bu_free( (char *)eptr->l.stp, "struct soltab" );
			}

			bu_free( (char *)eptr, "leaf pointer" );
			break;
	}
}

/* convert all "half" solids to polysolids */
HIDDEN void
fix_halfs(struct dg_client_data	*dgcdp)
{
	point_t max, min;
	int i, count=0;
	struct bn_tol *tol;

	tol = &dgcdp->dgop->dgo_wdbp->wdb_tol;

	VSETALL( max, -MAX_FASTF )
	VSETALL( min, MAX_FASTF )

	for( i=0 ; i<BU_PTBL_END( &dgcdp->leaf_list ) ; i++ )
	{
		union E_tree *tp;

		tp = (union E_tree *)BU_PTBL_GET( &dgcdp->leaf_list, i );

		if( tp->l.stp->st_id == ID_HALF )
			continue;

		VMINMAX( min, max, tp->l.stp->st_min )
		VMINMAX( min, max, tp->l.stp->st_max )
		count++;
	}

	if( !count )
	{
		Tcl_AppendResult(dgcdp->interp, "A 'half' solid is the only solid in a region (ignored)\n", (char *)NULL );
		return;
	}

	for( i=0 ; i<BU_PTBL_END( &dgcdp->leaf_list ) ; i++ )
	{
		union E_tree *tp;
		struct vertex *v[8];
		struct vertex **vp[4];
		struct nmgregion *r;
		struct shell *s;
		struct rt_pg_internal *pg;
		struct faceuse *fu;
		plane_t haf_pl;
		struct half_specific *hp;
		int j;

		tp = (union E_tree *)BU_PTBL_GET( &dgcdp->leaf_list, i );

		if( tp->l.stp->st_id != ID_HALF )
			continue;

		hp = (struct half_specific *)tp->l.stp->st_specific;

		HMOVE( haf_pl, hp->half_eqn )

		if( DIST_PT_PLANE( max, haf_pl ) >= -tol->dist &&
		    DIST_PT_PLANE( min, haf_pl ) >= -tol->dist )
			continue;

		/* make an NMG the size of our model bounding box */
		tp->l.m = nmg_mm();
		r = nmg_mrsv( tp->l.m );
		s = BU_LIST_FIRST( shell, &r->s_hd );

		for( j=0 ; j<8 ; j++ )
			v[j] = (struct vertex *)NULL;

		vp[0] = &v[0];
		vp[1] = &v[1];
		vp[2] = &v[2];
		vp[3] = &v[3];
		fu = nmg_cmface( s, vp, 4 );
		nmg_vertex_g( v[0], max[X], min[Y], min[Z] );
		nmg_vertex_g( v[1], max[X], max[Y], min[Z] );
		nmg_vertex_g( v[2], max[X], max[Y], max[Z] );
		nmg_vertex_g( v[3], max[X], min[Y], max[Z] );
		nmg_calc_face_g( fu );

		vp[0] = &v[4];
		vp[1] = &v[5];
		vp[2] = &v[6];
		vp[3] = &v[7];
		fu = nmg_cmface( s, vp, 4 );
		nmg_vertex_g( v[4], min[X], min[Y], min[Z] );
		nmg_vertex_g( v[5], min[X], min[Y], max[Z] );
		nmg_vertex_g( v[6], min[X], max[Y], max[Z] );
		nmg_vertex_g( v[7], min[X], max[Y], min[Z] );
		nmg_calc_face_g( fu );

		vp[0] = &v[0];
		vp[1] = &v[3];
		vp[2] = &v[5];
		vp[3] = &v[4];
		fu = nmg_cmface( s, vp, 4 );
		nmg_calc_face_g( fu );

		vp[0] = &v[1];
		vp[1] = &v[7];
		vp[2] = &v[6];
		vp[3] = &v[2];
		fu = nmg_cmface( s, vp, 4 );
		nmg_calc_face_g( fu );

		vp[0] = &v[3];
		vp[1] = &v[2];
		vp[2] = &v[6];
		vp[3] = &v[5];
		fu = nmg_cmface( s, vp, 4 );
		nmg_calc_face_g( fu );

		vp[0] = &v[1];
		vp[1] = &v[0];
		vp[2] = &v[4];
		vp[3] = &v[7];
		fu = nmg_cmface( s, vp, 4 );
		nmg_calc_face_g( fu );

		nmg_region_a( r, tol );

		for( BU_LIST_FOR( fu, faceuse, &s->fu_hd ) )
		{
			struct edgeuse *eu, *new_eu;
			struct loopuse *lu, *new_lu;
			plane_t pl;
			int count;
			struct vertexuse *vcut[2];
			point_t pt[2];
			struct edgeuse *eu_split[2];

			if( fu->orientation != OT_SAME )
				continue;

			NMG_GET_FU_PLANE( pl, fu );

			if( bn_coplanar( pl, haf_pl, tol ) > 0 )
				continue;

			lu = BU_LIST_FIRST( loopuse, &fu->lu_hd );

			count = 0;
			for( BU_LIST_FOR( eu, edgeuse, &lu->down_hd ) )
			{
				vect_t dir;
				struct vertex_g *v1g, *v2g;
				fastf_t dist;

				v1g = eu->vu_p->v_p->vg_p;
				v2g = eu->eumate_p->vu_p->v_p->vg_p;

				VSUB2( dir, v2g->coord, v1g->coord )

				if( bn_isect_line3_plane( &dist, v1g->coord, dir, haf_pl, tol ) < 1 )
					continue;

				if( dist < 0.0 || dist >=1.0 )
					continue;

				VJOIN1( pt[count], v1g->coord, dist, dir )
				eu_split[count] = eu;

				count++;
				if( count == 2 )
					break;
			}

			if( count != 2 )
				continue;

			new_eu = nmg_eusplit( (struct vertex *)NULL, eu_split[0], 1 );
			vcut[0] = new_eu->vu_p;
			nmg_vertex_gv( vcut[0]->v_p, pt[0] );

			new_eu = nmg_eusplit( (struct vertex *)NULL, eu_split[1], 1 );
			vcut[1] = new_eu->vu_p;
			nmg_vertex_gv( vcut[1]->v_p, pt[1] );

			new_lu = nmg_cut_loop( vcut[0], vcut[1] );
			nmg_lu_reorient( lu );
			nmg_lu_reorient( new_lu );

			for( BU_LIST_FOR( eu, edgeuse, &lu->down_hd ) )
			{
				if( eu->vu_p->v_p == vcut[0]->v_p || eu->vu_p->v_p == vcut[1]->v_p )
					continue;

				if( DIST_PT_PLANE( eu->vu_p->v_p->vg_p->coord, haf_pl ) > tol->dist )
				{
					nmg_klu( lu );
					break;
				}
				else
				{
					nmg_klu( new_lu );
					break;
				}
			}
		}

		/* kill any faces outside the half */
		fu = BU_LIST_FIRST( faceuse, &s->fu_hd );
		if( fu->orientation != OT_SAME )
			fu = BU_LIST_PNEXT( faceuse, &fu->l );
		while( BU_LIST_NOT_HEAD( &fu->l, &s->fu_hd ) )
		{
			struct faceuse *next_fu;
			struct loopuse *lu;
			int killfu=0;

			next_fu = BU_LIST_PNEXT( faceuse, &fu->l );
			if( fu->fumate_p == next_fu )
				next_fu = BU_LIST_PNEXT( faceuse, &next_fu->l );

			if( fu->orientation != OT_SAME )
			{
				fu = next_fu;
				continue;
			}

			lu = BU_LIST_FIRST( loopuse, &fu->lu_hd );
			while( BU_LIST_NOT_HEAD( &lu->l, &fu->lu_hd ) )
			{
				struct loopuse *next_lu;
				struct edgeuse *eu;
				int killit;

				next_lu = BU_LIST_PNEXT( loopuse, &lu->l );

				killit = 0;
				for( BU_LIST_FOR( eu, edgeuse, &lu->down_hd ) )
				{
					struct vertex_g *vg;

					vg = eu->vu_p->v_p->vg_p;

					if( DIST_PT_PLANE( vg->coord, haf_pl ) > tol->dist )
					{
						killit = 1;
						break;
					}
				}

				if( killit )
				{
					if( nmg_klu( lu ) )
					{
						killfu = 1;
						break;
					}
				}
				lu = next_lu;
			}

			if( killfu )
				nmg_kfu( fu );

			fu = next_fu;
		}

		nmg_rebound( tp->l.m, tol );
		nmg_model_fuse( tp->l.m, tol );
		nmg_close_shell( s, tol );
		nmg_rebound( tp->l.m, tol );

		BU_GETSTRUCT( pg, rt_pg_internal );

		if( !nmg_to_poly( tp->l.m, pg, tol ) )
		{
			bu_free( (char *)pg, "rt_pg_internal" );
			Tcl_AppendResult(dgcdp->interp, "Prep failure for solid '", tp->l.stp->st_dp->d_namep,
				"'\n", (char *)NULL );
		}
		else
		{
			struct rt_db_internal intern2;

			RT_INIT_DB_INTERNAL( &intern2 );
			intern2.idb_major_type = DB5_MAJORTYPE_BRLCAD;
			intern2.idb_type = ID_POLY;
			intern2.idb_meth = &rt_functab[ID_POLY];
			intern2.idb_ptr = (genptr_t)pg;
			rt_functab[tp->l.stp->st_id].ft_free( tp->l.stp );
			tp->l.stp->st_specific = NULL;
			tp->l.stp->st_id = ID_POLY;
			VSETALL( tp->l.stp->st_max, -INFINITY );
			VSETALL( tp->l.stp->st_min,  INFINITY );
			if (rt_functab[ID_POLY].ft_prep( tp->l.stp, &intern2, dgcdp->rtip ) < 0 )
			{
				Tcl_AppendResult(dgcdp->interp, "Prep failure for polysolid version of solid '", tp->l.stp->st_dp->d_namep,
					"'\n", (char *)NULL );
			}

			rt_db_free_internal( &intern2, &rt_uniresource );
		}
	}
}

int
dgo_E_cmd(struct dg_obj	*dgop,
	 Tcl_Interp	*interp,
	 int		argc,
	 char 		**argv)
{
	register int		c;
	char			perf_message[128];
	struct dg_client_data	*dgcdp;

	if (argc < 2) {
		struct bu_vls vls;

		bu_vls_init(&vls);
		bu_vls_printf(&vls, "help E");
		Tcl_Eval(interp, bu_vls_addr(&vls));
		bu_vls_free(&vls);
		return TCL_ERROR;
	}

	if (bu_debug&BU_DEBUG_MEM_CHECK && bu_mem_barriercheck())
		bu_log("Error at start of 'E'\n");

	BU_GETSTRUCT(dgcdp, dg_client_data);
	dgcdp->dgop = dgop;
	dgcdp->interp = interp;
	dgcdp->do_polysolids = 0;
	dgcdp->wireframe_color_override = 0;

	/* Parse options. */
	bu_optind = 1;          /* re-init bu_getopt() */
	while((c=bu_getopt(argc,argv,"sC:")) != EOF) {
		switch (c) {
		case 'C':
			{
				int		r,g,b;
				register char	*cp = bu_optarg;

				r = atoi(cp);
				while((*cp >= '0' && *cp <= '9'))  cp++;
				while(*cp && (*cp < '0' || *cp > '9')) cp++;
				g = atoi(cp);
				while((*cp >= '0' && *cp <= '9'))  cp++;
				while(*cp && (*cp < '0' || *cp > '9')) cp++;
				b = atoi(cp);

				if (r < 0 || r > 255)  r = 255;
				if (g < 0 || g > 255)  g = 255;
				if (b < 0 || b > 255)  b = 255;

				dgcdp->wireframe_color_override = 1;
				dgcdp->wireframe_color[0] = r;
				dgcdp->wireframe_color[1] = g;
				dgcdp->wireframe_color[2] = b;
			}
			break;
		case 's':
			dgcdp->do_polysolids = 1;
			break;
		default:
			{
				struct bu_vls vls;

				bu_vls_init(&vls);
				bu_vls_printf(&vls, "help %s", argv[0]);
				Tcl_Eval(interp, bu_vls_addr(&vls));
				bu_vls_free(&vls);

				return TCL_ERROR;
			}
		}
	}
	argc -= bu_optind;
	argv += bu_optind;

	dgo_eraseobjpath(dgop, interp, argc, argv, LOOKUP_QUIET, 0);

#if 0
	dgop->dgo_wdbp->wdb_ttol.magic = RT_TESS_TOL_MAGIC;
	dgop->dgo_wdbp->wdb_ttol.rel = 0.01;
#endif

	dgcdp->ap = (struct application *)bu_malloc(sizeof(struct application), "Big E app");
	RT_APPLICATION_INIT(dgcdp->ap);
	dgcdp->ap->a_resource = &rt_uniresource;
	rt_uniresource.re_magic = RESOURCE_MAGIC;
	if( BU_LIST_UNINITIALIZED( &rt_uniresource.re_nmgfree ) )
		BU_LIST_INIT( &rt_uniresource.re_nmgfree );

	bu_ptbl_init( &dgcdp->leaf_list, 8, "leaf_list" );

	dgcdp->rtip = rt_new_rti( dgop->dgo_wdbp->dbip );
	dgcdp->rtip->rti_tol = dgop->dgo_wdbp->wdb_tol;	/* struct copy */
	dgcdp->rtip->useair = 1;
	dgcdp->ap->a_rt_i = dgcdp->rtip;

	dgcdp->nvectors = 0;
	(void)time( &dgcdp->start_time );

	if( rt_gettrees( dgcdp->rtip, argc, (const char **)argv, 1 ) )
	{
		bu_ptbl_free( &dgcdp->leaf_list );

		/* do not do an rt_free_rti() (closes the database!!!!) */
		rt_clean( dgcdp->rtip );

		bu_free( (char *)dgcdp->rtip, "rt_i structure for 'E'" );
		bu_free(dgcdp, "dgcdp");

		Tcl_AppendResult(interp, "Failed to get objects\n", (char *)NULL);
		return TCL_ERROR;
	}
	{
		struct region *rp;
		union E_tree *eptr;
		struct bu_list vhead;
		struct db_tree_state ts;
		struct db_full_path path;

		BU_LIST_INIT( &vhead );

		for( BU_LIST_FOR( rp, region, &(dgcdp->rtip->HeadRegion) ) )  {
			dgcdp->num_halfs = 0;
			eptr = build_etree( rp->reg_treetop, dgcdp );

			if( dgcdp->num_halfs )
				fix_halfs(dgcdp);

			Eplot( eptr, &vhead, dgcdp );
			free_etree( eptr, dgcdp );
			bu_ptbl_reset( &dgcdp->leaf_list );
			ts.ts_mater = rp->reg_mater;
			db_string_to_path( &path, dgop->dgo_wdbp->dbip, rp->reg_name );
			dgo_drawH_part2( 0, &vhead, &path, &ts, SOLID_NULL, dgcdp );
			db_free_full_path( &path );
		}
		/* do not do an rt_free_rti() (closes the database!!!!) */
		rt_clean( dgcdp->rtip );

		bu_free( (char *)dgcdp->rtip, "rt_i structure for 'E'" );
	}

	dgo_color_soltab(&dgop->dgo_headSolid);
	(void)time( &dgcdp->etime );

	/* free leaf_list */
	bu_ptbl_free( &dgcdp->leaf_list );

	sprintf(perf_message, "E: %ld vectors in %ld sec\n", dgcdp->nvectors, (long)(dgcdp->etime - dgcdp->start_time) );
	Tcl_AppendResult(interp, perf_message, (char *)NULL);

	return TCL_OK;
}

/*
 *			F _ E V E D I T
 *
 *  The "Big E" command.
 *  Evaluated Edit something (add to visible display)
 *  Usage: E object(s)
 */
int
dgo_E_tcl(ClientData	clientData,
	  Tcl_Interp	*interp,
	  int		argc,
	  char		**argv)
{
	struct dg_obj		*dgop = (struct dg_obj *)clientData;

	return dgo_E_cmd(dgop, interp, argc-1, argv+1);
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
