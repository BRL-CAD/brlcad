/* 
 *	N U R B _ T R I M _ U T I L . C
 *
 * nurb_trim.c - trimming curve Utilities.
 * 
 * Author:  Paul R. Stay
 * Source
 * 	SECAD/VLD Computing Consortium, Bldg 394
 * 	The US Army Ballistic Research Laboratory
 * 	Aberdeen Proving Ground, Maryland 21005
 * 
 * Date: Mon July 3, 1995
 * 
 * Copyright Notice - 
 * 	This software is Copyright (C) 1990-2004 by the United States Army.
 * 	All rights reserved.
 * 
 */
#ifndef lint
static const char rcs_ident[] = "$Header$";
#endif

#include "conf.h"

#include <stdio.h>
#include <math.h>

#include "machine.h"
#include "vmath.h"
#include "raytrace.h"
#include "nurb.h"

/* Check to see if the curve conmtrol polygon 
 * wonders outside the parametric range given.
 * This is usefull if a trimming curve control polygon is outside
 * but the evaluated curve is not. We will want to refine the 
 * curve so that is lies within the range;
 * otherwise it breaks the surface evaluation 
 */

int
rt_nurb_crv_in_range(struct edge_g_cnurb *crv, fastf_t u_min, fastf_t u_max, fastf_t v_min, fastf_t v_max)
{
	point_t eval;
	fastf_t *pts;
	int	coords = RT_NURB_EXTRACT_COORDS( crv->pt_type );
	int 	rat = RT_NURB_IS_PT_RATIONAL(crv->pt_type);
	int	i;

	pts = &crv->ctl_points[0];
	
	for( i = 0; i < crv->c_size; i++)
	{
		if (rat)
		{
			eval[0] = pts[0] / pts[2];
			eval[1] = pts[1] / pts[2];
			eval[2] = 1;
		} else	{
			eval[0] = pts[0];
			eval[1] = pts[1];
			eval[2] = 1;
		}

		if( eval[0] < u_min || eval[0] > u_max )
			return 0;

		if( eval[1] < v_min || eval[1] > v_max )
			return 0;

		pts += coords;
	}
	return 1;	
}
