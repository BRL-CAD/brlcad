/*		N U R B _ B E Z I E R . C
 *
 *  Function-
 *  	Convet the NURB surfaces into their bezier form 
 *	(no internal knots)
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

#include <stdio.h>
#include "machine.h"
#include "vmath.h"
#include "nurb.h"

struct snurb *
rt_nurb_bezier( srf )
struct snurb * srf;
{

	struct snurb * s_list, *s, *split;
	int dir;

	s_list = (struct snurb *)0;

	if( (dir = rt_bez_check( srf )) == -1)
		return (struct snurb * ) srf;

	s = (struct snurb *) rt_nurb_s_split( srf, dir);

	while( s != (struct snurb *)0)
	{
		struct snurb * bez;

		bez = s;
		s = s->next;

		if( (dir = rt_bez_check(bez)) == -1)
		{
			bez->next = s_list;
			s_list = bez;
		} else
		{
			split = (struct snurb *) rt_nurb_s_split(bez, dir);
			split->next->next = s;
			s = split;
			rt_nurb_free_snurb(bez);
		}
	}
	return (struct snurb *) s_list;	
}

int
rt_bez_check( srf )
struct snurb * srf;
{
	if( srf->u_knots.k_size > (2.0 * srf->order[0]))
		return 0;
	if( srf->v_knots.k_size > (2.0 * srf->order[1]))
		return 1;

	return -1;
}
