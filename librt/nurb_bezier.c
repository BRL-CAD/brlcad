/*		N U R B _ B E Z I E R . C
 *
 *  Function-
 *  	Convert a NURB surface or curve into Bezier form, with no internal knots.
 *
 *  Author -
 *	Paul R. Stay
 *
 *  Source -
 *     SECAD/VLD Computing Consortium, Bldg 394
 *     The U.S. Army Ballistic Research Laboratory
 *     Aberdeen Proving Ground, Maryland 21005
 *
 * Copyright Notice -
 *     This software is Copyright (C) 1991-2004 by the United States Army.
 *     All rights reserved.
 */

#include "conf.h"

#include <stdio.h>

#include "machine.h"
#include "vmath.h"
#include "nmg.h"
#include "raytrace.h"
#include "nurb.h"

/*
 *			R T _ N U R B _ B E Z I E R
 *
 *  Given a single snurb, if it is in Bezier form,
 *  duplicate the snurb, and enqueue it on the bezier_hd list.
 *  If the original snurb is NOT in Bezier form,
 *  subdivide it a set of snurbs which are,
 *  each of which are enqueued on the bezier_hd list.
 *
 *  In either case, the original surface remains untouched.
 *
 *  Returns -
 *	0	Surface splitting was done.
 *	1	Original surface was Bezier, only a copy was done.
 */
int
rt_nurb_bezier(struct bu_list *bezier_hd, const struct face_g_snurb *orig_surf, struct resource *res)
{
	struct face_g_snurb	*s;
	int		dir;
	struct bu_list	todo;

	NMG_CK_SNURB(orig_surf);

	if( (dir = rt_bez_check( orig_surf )) == -1)  {
		s = rt_nurb_scopy( orig_surf, res );
		BU_LIST_APPEND( bezier_hd, &s->l );
		return 1;	/* Was already Bezier, nothing done */
	}

	BU_LIST_INIT( &todo );
	rt_nurb_s_split( &todo, orig_surf, dir, res );

	while( BU_LIST_WHILE( s, face_g_snurb, &todo ) )  {
		if( (dir = rt_bez_check(s)) == -1)  {
			/* This snurb is now a Bezier */
			BU_LIST_DEQUEUE( &s->l );
			BU_LIST_APPEND( bezier_hd, &s->l );
		} else {
			/* Split, and keep going */
			BU_LIST_DEQUEUE( &s->l );
			rt_nurb_s_split( &todo, s, dir, res );
			rt_nurb_free_snurb(s, res);
		}
	}
	return 0;		/* Bezier snurbs on bezier_hd list */
}

int
rt_bez_check(const struct face_g_snurb *srf)
{
	NMG_CK_SNURB(srf);

	if( srf->u.k_size > (2.0 * srf->order[0]))
		return 0;
	if( srf->v.k_size > (2.0 * srf->order[1]))
		return 1;

	return -1;
}

/*		N U R B _ C R V _ I S _ B E Z I E R
 *
 * Check if a NURB curve is in Bezier form.
 *
 * returns:
 *	1 - curve is Bezier
 *	0 - curve is not Bezier
 */

int
nurb_crv_is_bezier(const struct edge_g_cnurb *crv)
{
	int i;
	fastf_t knot_min, knot_max;
	int bezier=1;

	knot_min = crv->k.knots[0];
	knot_max = crv->k.knots[crv->k.k_size-1];

	for( i=1 ; i<crv->k.k_size-1 ; i++ )
	{
		if( crv->k.knots[i] != knot_min && crv->k.knots[i] != knot_max )
		{
			bezier = 0;
			break;
		}
	}

	return( bezier );
}

/*	N U R B _ C _ T O _ B E Z I E R
 *
 * Split NURB curve into list of Bezier curves.
 *
 * If curve is already Bezier, return NULL
 */

void
nurb_c_to_bezier(struct bu_list *clist, struct edge_g_cnurb *crv)
{
	fastf_t knot_min, knot_max;
	int i;
	struct edge_g_cnurb *crv1, *crv_copy;
	int done;

	/* make a copy of original curve */
	crv_copy = rt_nurb_crv_copy( crv );

	/* split curve at each knot value */
	done = 0;
	while( !done )
	{
		fastf_t split;

		knot_min = crv_copy->k.knots[0];
		knot_max = crv_copy->k.knots[crv_copy->k.k_size-1];

		split = MAX_FASTF;
		for( i=1 ; i<crv_copy->k.k_size-1 ; i++ )
		{
			if( crv_copy->k.knots[i] != knot_min && crv_copy->k.knots[i] != knot_max )
			{
				split = crv_copy->k.knots[i];
				break;
			}
		}

		if( split == MAX_FASTF )
		{
			done = 1;
			BU_LIST_APPEND( clist, &crv_copy->l );
			break;
		}

		crv1 = rt_nurb_c_xsplit( crv_copy, split );

		rt_nurb_free_cnurb( crv_copy );
		crv_copy = BU_LIST_PNEXT( edge_g_cnurb, &crv1->l );
		BU_LIST_DEQUEUE( &crv_copy->l );

		BU_LIST_APPEND( clist, &crv1->l );
	}
}
