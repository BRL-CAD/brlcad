/*		N U R B _ B E Z I E R . C
 *
 *  Function-
 *  	Convet a NURB surface into Bezier form, with no internal knots.
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
 *     This software is Copyright (C) 1991 by the United States Army.
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
rt_nurb_bezier( bezier_hd, orig_surf )
struct bu_list		*bezier_hd;
CONST struct face_g_snurb	*orig_surf;
{
	struct face_g_snurb	*s;
	int		dir;
	struct bu_list	todo;

	NMG_CK_SNURB(orig_surf);

	if( (dir = rt_bez_check( orig_surf )) == -1)  {
		s = rt_nurb_scopy( orig_surf );
		BU_LIST_APPEND( bezier_hd, &s->l );
		return 1;	/* Was already Bezier, nothing done */
	}

	BU_LIST_INIT( &todo );
	rt_nurb_s_split( &todo, orig_surf, dir );

	while( BU_LIST_WHILE( s, face_g_snurb, &todo ) )  {
		if( (dir = rt_bez_check(s)) == -1)  {
			/* This snurb is now a Bezier */
			BU_LIST_DEQUEUE( &s->l );
			BU_LIST_APPEND( bezier_hd, &s->l );
		} else {
			/* Split, and keep going */
			BU_LIST_DEQUEUE( &s->l );
			rt_nurb_s_split( &todo, s, dir );
			rt_nurb_free_snurb(s);
		}
	}
	return 0;		/* Bezier snurbs on bezier_hd list */
}

int
rt_bez_check( srf )
CONST struct face_g_snurb * srf;
{
	NMG_CK_SNURB(srf);

	if( srf->u.k_size > (2.0 * srf->order[0]))
		return 0;
	if( srf->v.k_size > (2.0 * srf->order[1]))
		return 1;

	return -1;
}
