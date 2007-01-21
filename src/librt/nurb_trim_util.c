/*                N U R B _ T R I M _ U T I L . C
 * BRL-CAD
 *
 * Copyright (c) 1990-2007 United States Government as represented by
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
/** @addtogroup nurb */
/** @{ */
/** @file nurb_trim_util.c
 *
 * Trimming curve Utilities.
 *
 * Author:  Paul R. Stay
 * Source
 * 	SECAD/VLD Computing Consortium, Bldg 394
 * 	The US Army Ballistic Research Laboratory
 * 	Aberdeen Proving Ground, Maryland 21005
 *
 * Date: Mon July 3, 1995
 */
/** @} */

#ifndef lint
static const char rcs_ident[] = "$Header$";
#endif

#include "common.h"



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

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
