/* 
 *      N U R B _ T O _P O L Y . C
 *
 * Function -
 *     Returns two polygons from a NURB surface.
 *     Asumes that the surface is flat.
 * 
 * Author -
 *     Paul R. Stay
 *
 * Source -
 *     SECAD/VLD Computing Consortium, Bldg 394
 *     The U.S. Army Ballistic Research Laboratory
 *     Aberdeen Proving Ground, Maryland 21005
 *
 * Copyright Notice -
 *     This software is Copyright (C) 1990-2004 by the United States Army.
 *     All rights reserved.
 */

#include "conf.h"

#include <stdio.h>

#include "machine.h"
#include "vmath.h"
#include "raytrace.h"
#include "nurb.h"

/* Algorithm -
 * From the four corners of the surface, return the two parts split by the
 * diagonal from the first and third corner point making sure Homogeneous
 * points are divided. 
 */

struct rt_nurb_poly *
rt_nurb_to_poly( srf )
struct face_g_snurb *srf;
{
	int	coords = srf->pt_type;
	fastf_t 	 * p1, *p2, *p3, *p4;
	fastf_t 	uv1[2], uv2[2], uv3[2], uv4[2];
	struct rt_nurb_poly *p, *p_head;

	/* Extract the four corners from the mesh */

	p1 = srf->ctl_points;
	p2 = srf->ctl_points + coords * srf->s_size[1];
	p3 = srf->ctl_points + (coords * srf->s_size[1] * 
	    (srf->s_size[0] - 1)) + 
	    ((srf->s_size[1] - 1) * coords);
	p4 = srf->ctl_points + (coords * srf->s_size[1] * 
	    (srf->s_size[0] - 1));

	/* If the point is rational then divide out the w component */
	if ( RT_NURB_IS_PT_RATIONAL(srf->pt_type)) {
		int	w_index;

		if ( RT_NURB_EXTRACT_PT_TYPE( srf->pt_type) == RT_NURB_PT_XY)
			w_index = 2;
		else if ( RT_NURB_EXTRACT_PT_TYPE( srf->pt_type) == RT_NURB_PT_UV)
			w_index = 2;
		else if ( RT_NURB_EXTRACT_PT_TYPE( srf->pt_type) == RT_NURB_PT_XYZ)
			w_index = 3;
		else /* assume the forth coordinate */
			w_index = 3;

		p1[0] = p1[0] / p1[w_index];
		p2[0] = p2[0] / p1[w_index];
		p3[0] = p3[0] / p1[w_index];
		p4[0] = p4[0] / p1[w_index];
	}

	uv1[0] = srf->u.knots[0];
	uv1[1] = srf->v.knots[0];

	uv2[0] = srf->u.knots[srf->u.k_size -1];
	uv2[1] = srf->v.knots[0];

	uv3[0] = srf->u.knots[srf->u.k_size -1];
	uv3[1] = srf->v.knots[srf->v.k_size -1];

	uv4[0] = srf->u.knots[0];
	uv4[1] = srf->v.knots[srf->v.k_size -1];

	p = rt_nurb_mk_poly(p1, p2, p3, uv1, uv2, uv3 ); 
	p_head = p;
	p = rt_nurb_mk_poly(p3, p4, p1, uv3, uv4, uv1 ); 
	p->next = p_head; 
	p_head = p;

	return p_head;
}


struct rt_nurb_poly *
rt_nurb_mk_poly( v1, v2, v3, uv1, uv2, uv3 )
fastf_t * v1, *v2, *v3;
fastf_t uv1[2], uv2[2], uv3[2];
{
	struct rt_nurb_poly *p;

	p = (struct rt_nurb_poly *) bu_malloc( sizeof( struct rt_nurb_poly ),
	    "rt_nurb_mk_poly: rt_nurb_poly struct" );

	p->next = (struct rt_nurb_poly *) 0;

	VMOVE( p->ply[0], v1);
	VMOVE( p->ply[1], v2);
	VMOVE( p->ply[2], v3);

	p->uv[0][0] = uv1[0];
	p->uv[0][1] = uv1[1];
	p->uv[1][0] = uv2[0];
	p->uv[1][1] = uv2[1];
	p->uv[2][0] = uv3[0];
	p->uv[2][1] = uv3[1];

	return p;
}
