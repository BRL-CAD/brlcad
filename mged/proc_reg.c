/*		This module impliments the 'E' command
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
static const char RCSid[] = "@(#)$Header$ (BRL)";
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
#include "rtgeom.h"
#include "raytrace.h"
#include "./ged.h"
#include "./mged_solid.h"
#include "./mged_dm.h"
#include <ctype.h>
/* #define debug 1 */

extern struct bn_tol		mged_tol;
extern struct rt_tess_tol	mged_ttol;
extern int			mged_wireframe_color_override;
extern int			mged_wireframe_color[3];
static struct application	*ap=(struct application *)NULL;
static time_t			start_time, etime;
static struct bu_ptbl		leaf_list;
static long			nvectors;
static struct rt_i		*rtip;
static int			num_halfs;
static int			do_polysolids; /* if this is non-zero, convert all the solids to polysolids
						* for the raytracing (otherwise just raytrace the solids
						* themselves). Raytracing polysolids is slower, but produces
						* a better plot, since the polysolids exactly match the NMGs
						* that provide the starting edges.
						*/
union E_tree *build_etree();

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
Edrawtree( dp )
{
	return;
}

HIDDEN union E_tree *
add_solid( dp, mat )
struct directory *dp;
matp_t mat;
{
	union E_tree *eptr;
	struct nmgregion *r;
	struct rt_db_internal intern;
	int id;
	int solid_is_plate_mode_bot=0;

	BU_GETUNION( eptr, E_tree );
	eptr->magic = E_TREE_MAGIC;

	id = rt_db_get_internal( &intern, dp, dbip, mat, &rt_uniresource );
	if( id < 0 )
	{
		Tcl_AppendResult(interp, "Failed to get internal form of ",
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

		eptr = build_etree( comb->tree );
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
		num_halfs++;
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
			&mged_ttol, &mged_tol) < 0)
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
		struct rt_pg_internal *pg;
		struct rt_db_internal intern2;

		if( do_polysolids )
		{
			/* create and prep a polysolid version of this solid */

			BU_GETSTRUCT( pg, rt_pg_internal );

			if( !solid_is_plate_mode_bot || !eptr->l.m || !nmg_to_poly( eptr->l.m, pg, &mged_tol ) )
			{
				bu_free( (char *)pg, "rt_pg_internal" );
				eptr->l.stp->st_id = id;
				if( rt_functab[id].ft_prep( eptr->l.stp, &intern, rtip ) < 0 )
					Tcl_AppendResult(interp, "Prep failure for solid '", dp->d_namep,
						"'\n", (char *)NULL );
			}
			else
			{
				RT_INIT_DB_INTERNAL( &intern2 );
				intern2.idb_type = ID_POLY;
				intern2.idb_meth = &rt_functab[ID_POLY];
				intern2.idb_ptr = (genptr_t)pg;
				eptr->l.stp->st_id = ID_POLY;
				if (rt_functab[ID_POLY].ft_prep( eptr->l.stp, &intern2, rtip ) < 0 )
				{
					Tcl_AppendResult(interp, "Prep failure for solid '", dp->d_namep,
						"'\n", (char *)NULL );
				}

				rt_db_free_internal( &intern2, &rt_uniresource );
			}
		}
		else
		{
			/* prep this solid */

			eptr->l.stp->st_id = id;
			if( rt_functab[id].ft_prep( eptr->l.stp, &intern, rtip ) < 0 )
				Tcl_AppendResult(interp, "Prep failure for solid '", dp->d_namep,
					"'\n", (char *)NULL );
		}
	}

	if( id != ID_NMG )
		rt_db_free_internal( &intern, &rt_uniresource );

	/* add this leaf to the leaf list */
	bu_ptbl_ins( &leaf_list, (long *)eptr );

	return( eptr );
}

/* build an E_tree corresponding to the region tree (tp) */
union E_tree *
build_etree( tp )
union tree *tp;
{
	union E_tree *eptr;
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
			eptr->n.left = build_etree( tp->tr_b.tb_left );
			eptr->n.right = build_etree( tp->tr_b.tb_right );
			break;
		case OP_SOLID:
			stp = tp->tr_a.tu_stp;
			eptr = add_solid( stp->st_dp, stp->st_matp );
			eptr->l.op = tp->tr_op;
			BU_LIST_INIT( &eptr->l.seghead );
			break;
		case OP_DB_LEAF:
			if( (dp=db_lookup( dbip, tp->tr_l.tl_name, LOOKUP_NOISY )) == DIR_NULL )
			{
				eptr->l.m = (struct model *)NULL;
				break;
			}
			eptr = add_solid( dp, tp->tr_l.tl_mat );
			eptr->l.op = tp->tr_op;
			BU_LIST_INIT( &eptr->l.seghead );
			break;
		case OP_NOP:
			/* add a NULL solid  */
			BU_GETUNION( eptr, E_tree );
			eptr->magic = E_TREE_MAGIC;
			eptr->l.m = (struct model *)NULL;
			break;
	}
	return( eptr );
}

/* a handy routine (for debugging) that prints asegment list */
void
show_seg( seg, str )
struct bu_list *seg;
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
					bu_log( "\t %g to %g (??)\n", ptr->seg_in.hit_dist, ptr->seg_out.hit_dist );
			}
		}
	}
}

/* given a segment list, eliminate any overlaps in the segments */
HIDDEN  void
eliminate_overlaps( seghead )
struct bu_list *seghead;
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
				RT_FREE_SEG( b, ap->a_resource );
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
do_intersect( A, B, seghead, type )
struct seg *A;
struct seg *B;
struct bu_list *seghead;
struct soltab *type;
{
	struct seg *tmp=(struct seg *)NULL;

	if( NOT_SEG_OVERLAP( A, B ) )
		return;

	RT_GET_SEG( tmp, ap->a_resource );
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
do_subtract( A, B, seghead )
struct seg *A;
struct seg *B;
struct bu_list *seghead;
{
	struct seg *tmp=(struct seg *)NULL;


	if( NOT_SEG_OVERLAP( A, B ) )
	{
		RT_GET_SEG( tmp, ap->a_resource )
		*tmp = *A;
		BU_LIST_INSERT( seghead, &tmp->l )
		return;
	}

	if( B->seg_in.hit_dist<= A->seg_in.hit_dist )
	{
		if( B->seg_out.hit_dist < A->seg_out.hit_dist )
		{
			RT_GET_SEG( tmp, ap->a_resource )
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
			RT_GET_SEG( tmp, ap->a_resource )
			*tmp = *A;
			tmp->seg_out.hit_dist = B->seg_in.hit_dist;
			BU_LIST_INSERT( seghead, &tmp->l )
			return;
		}
		else
		{
			RT_GET_SEG( tmp, ap->a_resource );
			tmp->seg_in.hit_dist = A->seg_in.hit_dist;
			tmp->seg_out.hit_dist = B->seg_in.hit_dist;
			tmp->seg_stp = A->seg_stp;
			BU_LIST_INSERT( seghead, &tmp->l )
			RT_GET_SEG( tmp, ap->a_resource );
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
do_union( A, B, seghead )
struct seg *A;
struct seg *B;
struct bu_list *seghead;
{
	struct seg *tmp;

	RT_GET_SEG( tmp, ap->a_resource )

	if( NOT_SEG_OVERLAP( A, B ) )
	{
		if( A->seg_in.hit_dist <= B->seg_in.hit_dist )
		{
			*tmp = *A;
			BU_LIST_INSERT( seghead, &tmp->l )
			RT_GET_SEG( tmp, ap->a_resource )
			*tmp = *B;
			BU_LIST_INSERT( seghead, &tmp->l )
		}
		else
		{
			*tmp = *B;
			BU_LIST_INSERT( seghead, &tmp->l )
			RT_GET_SEG( tmp, ap->a_resource )
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
promote_ints( head )
struct bu_list *head;
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
					RT_FREE_SEG( tmp, ap->a_resource )
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
						RT_FREE_SEG( tmp, ap->a_resource )
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
					RT_GET_SEG( tmp, ap->a_resource )
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
					RT_FREE_SEG( tmp, ap->a_resource )
					break;
				}

				if( b->seg_out.hit_dist == a->seg_out.hit_dist )
				{
					tmp = b;
					b = BU_LIST_PNEXT( seg, &b->l );
					BU_LIST_DEQUEUE( &tmp->l )
					RT_FREE_SEG( tmp, ap->a_resource )
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
						RT_FREE_SEG( tmp, ap->a_resource )
						continue;
					}
				}
				else if( b->seg_in.hit_dist == a->seg_in.hit_dist )
					b->seg_in.hit_dist = a->seg_out.hit_dist;
				else
				{
					RT_GET_SEG( tmp, ap->a_resource )
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
				RT_FREE_SEG( b, ap->a_resource )
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
					RT_GET_SEG( tmp, ap->a_resource )
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
					RT_GET_SEG( tmp, ap->a_resource )
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
eval_op( A, op, B )
struct bu_list *A;
struct bu_list *B;
int op;
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
				MY_FREE_SEG_LIST( B, ap->a_resource );
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
			 * 			ON_A + ON_B
			 *			ON_B + IN_A
			 *			IN_A - IN_B
			 */
			for( BU_LIST_FOR( sega, seg, A ) )
			{
				for( BU_LIST_FOR( segb, seg, B ) )
				{
					if( sega->seg_stp == ON_INT && segb->seg_stp == ON_INT )
						do_intersect( sega, segb, &ret, ON_SURF );
					else if( sega->seg_stp == ON_SURF || sega->seg_stp == ON_INT )
					{
						if( segb->seg_stp == IN_SOL )
							do_subtract( sega, segb, &ret );
						else
							do_intersect( sega, segb, &ret, sega->seg_stp );
					}
					else if( segb->seg_stp == ON_SURF ||  segb->seg_stp == ON_INT )
						do_intersect( segb, sega, &ret, segb->seg_stp );
					else
						do_subtract( sega, segb, &ret );
				}
			}
			MY_FREE_SEG_LIST( B, ap->a_resource );
			bu_free( (char *)B, "bu_list" );
			MY_FREE_SEG_LIST( A, ap->a_resource );
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
				MY_FREE_SEG_LIST( A, ap->a_resource );
				MY_FREE_SEG_LIST( B, ap->a_resource );
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
						do_intersect( sega, segb, &ret, ON_SURF );
					else if( sega->seg_stp == ON_SURF || sega->seg_stp == ON_INT )
						do_intersect( sega, segb, &ret, sega->seg_stp );
					else
						do_intersect( segb, sega, &ret, segb->seg_stp );
				}
			}
			MY_FREE_SEG_LIST( B, ap->a_resource );
			bu_free( (char *)B, "bu_list" );
			MY_FREE_SEG_LIST( A, ap->a_resource );
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
			promote_ints( &ons );

			/* make sure the segments are unique */
			eliminate_overlaps( &ins );
			eliminate_overlaps( &ons );

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
						RT_FREE_SEG( sega, ap->a_resource )
						sega = next;
						break;
					}

					if( segb->seg_in.hit_dist > sega->seg_in.hit_dist &&
					    segb->seg_out.hit_dist < sega->seg_out.hit_dist )
					{
						/* split sega */
						RT_GET_SEG( tmp, ap->a_resource )
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

			MY_FREE_SEG_LIST( B, ap->a_resource );
			bu_free( (char *)B, "bu_list" );
			MY_FREE_SEG_LIST( A, ap->a_resource );
			BU_LIST_INSERT_LIST( A, &ret )

			#ifdef debug
			show_seg( A, "Returning" );
			#endif

			return( A );
	}

	/* should never get here */
	MY_FREE_SEG_LIST( A, ap->a_resource );
	MY_FREE_SEG_LIST( B, ap->a_resource );
	bu_free( (char *)B, "bu_list" );

	#ifdef debug
	show_seg( A, "Returning (default)" );
	#endif

	return( A );
	
}

/* evaluate an E-tree */
HIDDEN struct bu_list *
eval_etree( eptr )
union E_tree *eptr;
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

			A = eval_etree( eptr->n.left );
			B = eval_etree( eptr->n.right );
			return( eval_op( A, eptr->n.op, B ) );
	}

	/* should never get here */
	return( (struct bu_list *)NULL );	/* for the compilers */
}

/* Shoot rays (corresponding to possible edges in the result)
 * at the solids, put the results in the E-tree leaves as type IN_SOL.
 * Call eval_etree() and plot the results
 */
HIDDEN void
shoot_and_plot( start_pt, dir, vhead, edge_len, skip_leaf1, skip_leaf2, eptr, type )
point_t start_pt;
vect_t dir;
struct bu_list *vhead;
fastf_t edge_len;
int skip_leaf1, skip_leaf2;
union E_tree *eptr;
struct soltab *type;
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
	rd.tol = &mged_tol;
	rd.ap = ap;
	rd.magic = NMG_RAY_DATA_MAGIC;
	rd.classifying_ray = 0;
	rd.hitmiss = (struct hitmiss **)NULL;

	/* shoot this ray at every leaf solid except the one this edge came from
	 * (or the two that this intersection line came from
	 */
	for( shoot_leaf=0 ; shoot_leaf < BU_PTBL_END( &leaf_list ) ; shoot_leaf++ )
	{
		union E_tree *shoot;
		int dont_shoot=0;

		shoot = (union E_tree *)BU_PTBL_GET( &leaf_list, shoot_leaf );

		if( BU_LIST_NON_EMPTY( &shoot->l.seghead ) )
		{
			MY_FREE_SEG_LIST( &shoot->l.seghead, ap->a_resource );
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

			/* put entire edge in seg list and mark it as ON the surface */
			RT_GET_SEG( seg, ap->a_resource );
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
			if( rt_functab[shoot->l.stp->st_id].ft_shot( shoot->l.stp, &rp, ap, rd.seghead ) )
			{
				struct seg *seg;

				/* put the segments in the lead solid structure */
				while( BU_LIST_WHILE( seg, seg, &rd.seghead->l ) )
				{
					BU_LIST_DEQUEUE( &seg->l )
					/* clip segments to the edge being considered */
					if( seg->seg_in.hit_dist >= edge_len || seg->seg_out.hit_dist <= 0 )
						RT_FREE_SEG( seg, ap->a_resource )
					else
					{
						if( seg->seg_in.hit_dist < 0.0 )
							seg->seg_in.hit_dist = 0.0;
						if( seg->seg_out.hit_dist > edge_len )
							seg->seg_out.hit_dist = edge_len;
						seg->seg_stp = IN_SOL;
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

	final_segs = eval_etree( eptr );

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

			nvectors++;
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
		MY_FREE_SEG_LIST( final_segs, ap->a_resource );
	bu_free( (char *)final_segs, "bu_list" );

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
	int hit_count=0;

	if( bu_debug&BU_DEBUG_MEM_CHECK && bu_mem_barriercheck() )
		bu_log( "Error at start of Eplot()\n" );

	CK_ETREE( eptr );

	/* create an edge list for each leaf solid */
	for( leaf_no=0 ; leaf_no < BU_PTBL_END( &leaf_list ) ; leaf_no++ )
	{
		leaf_ptr = (union E_tree *)BU_PTBL_GET( &leaf_list, leaf_no );
		CK_ETREE( leaf_ptr );
		if( leaf_ptr->l.op != OP_DB_LEAF && leaf_ptr->l.op != OP_SOLID )
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
			if( edge_len < mged_tol.dist )
				continue;
			inv_len = 1.0/edge_len;
			VSCALE( dir, dir, inv_len );
			shoot_and_plot( vg->coord, dir, vhead, edge_len, leaf_no, -1, eptr, ON_SURF );

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
				NMG_GET_FU_PLANE( pl1, fu1 );

				for( BU_LIST_FOR( fu2, faceuse, &s2->fu_hd ) )
				{
					fastf_t dist;
					fastf_t len,start_len;
					point_t hits[4];
					point_t start_pt;
					vect_t dir;
					vect_t to_hit;
					fastf_t inv_len;
					fastf_t lena, lenb;
					vect_t diff;

					if( fu2->orientation != OT_SAME )
						continue;

					f2 = fu2->f_p;

					if ( !V3RPP_OVERLAP_TOL(f2->min_pt, f2->max_pt,
						f1->min_pt, f1->max_pt, &mged_tol) )
							continue;

					NMG_GET_FU_PLANE( pl2, fu2 );

					if( bn_coplanar( pl1, pl2, &mged_tol ) )
						continue;

					hit_count=0;
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
							if( dist < 0.0 || dist > 1.0 )
								continue;

							VJOIN1( hits[hit_count], vg1a->coord, dist, dir )
							if( hit_count == 1 )
							{
								VSUB2( diff, hits[hit_count], hits[hit_count-1] )
								len = MAGSQ( diff );
								if ( NEAR_ZERO( len, mged_tol.dist_sq ) )
									continue;
							}
							hit_count++;
							if( hit_count == 2 )
								break;
						}
						if( hit_count == 2 )
							break;
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

							if( bn_isect_line3_plane( &dist, vg2a->coord, dir, pl1, &mged_tol ) < 1 )
								continue;

							if( dist < 0.0 || dist > 1.0 )
								continue;

							VJOIN1( hits[hit_count], vg2a->coord, dist, dir )
							if( hit_count == 3 )
							{
								VSUB2( diff, hits[hit_count], hits[hit_count-1] )
								len = MAGSQ( diff );
								if ( NEAR_ZERO( len, mged_tol.dist_sq ) )
									continue;
							}
							hit_count++;
							if( hit_count == 4 )
								break;
						}
						if( hit_count == 4 )
							break;
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

					shoot_and_plot( start_pt, dir, vhead, len-start_len, leaf_no, leaf2, eptr, ON_INT );
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
		case OP_SOLID:
			if( eptr->l.m && !eptr->l.do_not_free_model )
			{
				nmg_km( eptr->l.m );
				eptr->l.m = (struct model *)NULL;
			}
			if( BU_LIST_NON_EMPTY( &eptr->l.seghead ) )
			{
				MY_FREE_SEG_LIST( &eptr->l.seghead, ap->a_resource );
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
fix_halfs()
{
	point_t max, min;
	int i, count=0;

	VSETALL( max, -MAX_FASTF )
	VSETALL( min, MAX_FASTF )

	for( i=0 ; i<BU_PTBL_END( &leaf_list ) ; i++ )
	{
		union E_tree *tp;

		tp = (union E_tree *)BU_PTBL_GET( &leaf_list, i );

		if( tp->l.stp->st_id == ID_HALF )
			continue;

		VMINMAX( min, max, tp->l.stp->st_min )
		VMINMAX( min, max, tp->l.stp->st_max )
		count++;
	}

	if( !count )
	{
		Tcl_AppendResult(interp, "A 'half' solid is the only solid in a region (ignored)\n", (char *)NULL );
		return;
	}

	for( i=0 ; i<BU_PTBL_END( &leaf_list ) ; i++ )
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

		tp = (union E_tree *)BU_PTBL_GET( &leaf_list, i );

		if( tp->l.stp->st_id != ID_HALF )
			continue;

		hp = (struct half_specific *)tp->l.stp->st_specific;

		HMOVE( haf_pl, hp->half_eqn )

		if( DIST_PT_PLANE( max, haf_pl ) >= -mged_tol.dist && DIST_PT_PLANE( min, haf_pl ) >= -mged_tol.dist )
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

		nmg_region_a( r, &mged_tol );

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

			if( bn_coplanar( pl, haf_pl, &mged_tol ) > 0 )
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

				if( bn_isect_line3_plane( &dist, v1g->coord, dir, haf_pl, &mged_tol ) < 1 )
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

				if( DIST_PT_PLANE( eu->vu_p->v_p->vg_p->coord, haf_pl ) > mged_tol.dist )
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

					if( DIST_PT_PLANE( vg->coord, haf_pl ) > mged_tol.dist )
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

		nmg_rebound( tp->l.m, &mged_tol );
		nmg_model_fuse( tp->l.m, &mged_tol );
		nmg_close_shell( s, &mged_tol );
		nmg_rebound( tp->l.m, &mged_tol );

		BU_GETSTRUCT( pg, rt_pg_internal );

		if( !nmg_to_poly( tp->l.m, pg, &mged_tol ) )
		{
			bu_free( (char *)pg, "rt_pg_internal" );
			Tcl_AppendResult(interp, "Prep failure for solid '", tp->l.stp->st_dp->d_namep,
				"'\n", (char *)NULL );
		}
		else
		{
			struct rt_db_internal intern2;

			RT_INIT_DB_INTERNAL( &intern2 );
			intern2.idb_type = ID_POLY;
			intern2.idb_meth = &rt_functab[ID_POLY];
			intern2.idb_ptr = (genptr_t)pg;
			rt_functab[tp->l.stp->st_id].ft_free( tp->l.stp );
			tp->l.stp->st_specific = NULL;
			tp->l.stp->st_id = ID_POLY;
			VSETALL( tp->l.stp->st_max, -INFINITY );
			VSETALL( tp->l.stp->st_min,  INFINITY );
			if (rt_functab[ID_POLY].ft_prep( tp->l.stp, &intern2, rtip ) < 0 )
			{
				Tcl_AppendResult(interp, "Prep failure for polysolid version of solid '", tp->l.stp->st_dp->d_namep,
					"'\n", (char *)NULL );
			}

			rt_db_free_internal( &intern2, &rt_uniresource );
		}
	}
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
	int	initial_blank_screen;
	char perf_message[128];
	register int    c;
	register struct dm_list *dmlp;
	register struct dm_list *save_dmlp;
	register struct cmd_list *save_cmd_list;

	if(argc < 2){
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
	mged_wireframe_color_override = 0;

	/* Parse options. */
	bu_optind = 1;          /* re-init bu_getopt() */
	while( (c=bu_getopt(argc,argv,"sC:")) != EOF ) {
		switch(c) {
		case 'C':
			{
				int		r,g,b;
				register char	*cp = bu_optarg;

				r = atoi(cp);
				while( (*cp >= '0' && *cp <= '9') )  cp++;
				while( *cp && (*cp < '0' || *cp > '9') ) cp++;
				g = atoi(cp);
				while( (*cp >= '0' && *cp <= '9') )  cp++;
				while( *cp && (*cp < '0' || *cp > '9') ) cp++;
				b = atoi(cp);

				if( r < 0 || r > 255 )  r = 255;
				if( g < 0 || g > 255 )  g = 255;
				if( b < 0 || b > 255 )  b = 255;

				mged_wireframe_color_override = 1;
				mged_wireframe_color[0] = r;
				mged_wireframe_color[1] = g;
				mged_wireframe_color[2] = b;
			}
			break;
		case 's':
			do_polysolids = 1;
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

	initial_blank_screen = BU_LIST_IS_EMPTY(&HeadSolid.l);
	eraseobjpath(interp, argc, argv, LOOKUP_QUIET, 0);

	mged_ttol.magic = RT_TESS_TOL_MAGIC;
	mged_ttol.abs = mged_abs_tol;
	mged_ttol.rel = mged_rel_tol;
	mged_ttol.norm = mged_nrm_tol;

	if( !ap )
	{
		ap = (struct application *)bu_calloc( 1, sizeof( struct application ), "Big E app" );
		ap->a_resource = &rt_uniresource;
		rt_uniresource.re_magic = RESOURCE_MAGIC;
		if( BU_LIST_UNINITIALIZED( &rt_uniresource.re_nmgfree ) )
			BU_LIST_INIT( &rt_uniresource.re_nmgfree );
	}

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
	ap->a_rt_i = rtip;

	nvectors = 0;
	(void)time( &start_time );

	if( rt_gettrees( rtip, argc, (const char **)argv, 1 ) )
	{
		bu_ptbl_free( &leaf_list );
		(void)signal( SIGINT, SIG_IGN );

		/* do not do an rt_free_rti() (closes the database!!!!) */
		rt_clean( rtip );

		bu_free( (char *)rtip, "rt_i structure for 'E'" );

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

		for( BU_LIST_FOR( rp, region, &(rtip->HeadRegion) ) )  {
			num_halfs = 0;
			eptr = build_etree( rp->reg_treetop );

			if( num_halfs )
				fix_halfs();

			Eplot( eptr, &vhead );
			free_etree( eptr );
			bu_ptbl_reset( &leaf_list );
			ts.ts_mater = rp->reg_mater;
			db_string_to_path( &path, dbip, rp->reg_name );
			drawH_part2( 0, &vhead, &path, &ts, SOLID_NULL );
			db_free_full_path( &path );
		}
		/* do not do an rt_free_rti() (closes the database!!!!) */
		rt_clean( rtip );

		bu_free( (char *)rtip, "rt_i structure for 'E'" );
	}

	(void)time( &etime );

	(void)signal( SIGINT, SIG_IGN );

	/* free leaf_list */
	bu_ptbl_free( &leaf_list );

	save_dmlp = curr_dm_list;
	save_cmd_list = curr_cmd_list;
	FOR_ALL_DISPLAYS(dmlp, &head_dm_list.l){
	  curr_dm_list = dmlp;
	  if(curr_dm_list->dml_tie)
	    curr_cmd_list = curr_dm_list->dml_tie;
	  else
	    curr_cmd_list = &head_cmd_list;

	  /* If we went from blank screen to non-blank, resize */
	  if (mged_variables->mv_autosize  && initial_blank_screen &&
	      BU_LIST_NON_EMPTY(&HeadSolid.l)) {
	    struct view_ring *vrp;

	    size_reset();
	    new_mats();
	    (void)mged_svbase();

	    for(BU_LIST_FOR(vrp, view_ring, &view_state->vs_headView.l))
	      vrp->vr_scale = view_state->vs_Viewscale;
	  }

	  color_soltab();
	}

	curr_dm_list = save_dmlp;
	curr_cmd_list = save_cmd_list;

	sprintf(perf_message, "E: %ld vectors in %ld sec\n", nvectors, (long)(etime - start_time) );
	Tcl_AppendResult(interp, perf_message, (char *)NULL);

	update_views = 1;
	return TCL_OK;
}
