/*		N U R B _ N O R M . C
 *
 *  Function-
 *  	Calulate and return the normal of a surface given the
 *	U,V parametric values.
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
#include "raytrace.h"
#include "nurb.h"

/* If the order of the surface is linear either direction
 * than approximate it.
 */

void
rt_nurb_s_norm(srf, u, v, norm)
struct face_g_snurb * srf;
fastf_t u, v;
fastf_t * norm;
{
	struct face_g_snurb *usrf, *vsrf;
	point_t uvec, vvec;
	fastf_t p;
	fastf_t se[4], ue[4], ve[4];
	int i;

	/* Case (linear, lienar) find the normal from the polygon */
	if( srf->order[0] == 2 && srf->order[1] == 2 )
	{
		/* Find the correct span to get the normal */
		rt_nurb_s_eval( srf, u, v, se);
		
		p = 0.0;
		for( i = 0; i < srf->u.k_size -1; i++)
		{
			if ( srf->u.knots[i] <= u 
				&& u < srf->u.knots[i+1] )
			{
				p = srf->u.knots[i];

				if (u == p)
					p = srf->u.knots[i+1];
				if ( u == p && i > 1)
					p = srf->u.knots[i-1];
			}
		}

		rt_nurb_s_eval( srf, p, v, ue);

		p = 0.0;
		for( i = 0; i < srf->v.k_size -1; i++)
		{
			if ( srf->v.knots[i] < v 
				&& srf->v.knots[i+1] )
			{
				p = srf->v.knots[i];
				if (v == p)
					p = srf->v.knots[i+1];
				if ( v == p && i > 1)
					p = srf->v.knots[i-1];
			}
		}

		rt_nurb_s_eval( srf, u, p, ve);		
	
		if( RT_NURB_IS_PT_RATIONAL(srf->pt_type))
		{
			ue[0] = ue[0] / ue[3];
			ue[1] = ue[1] / ue[3];
			ue[2] = ue[2] / ue[3];
			ue[3] = ue[3] / ue[3];

			ve[0] = ve[0] / ve[3];
			ve[1] = ve[1] / ve[3];
			ve[2] = ve[2] / ve[3];
			ve[3] = ve[3] / ve[3];

		}

		VSUB2(uvec, se, ue);
		VSUB2(vvec, se, ve);

		VCROSS( norm, uvec, vvec);
		VUNITIZE( norm );

		return;		

	}
	/* Case (linear, > linear) Use the linear direction to approximate
	 * the tangent to the surface 
	 */
	if( srf->order[0] == 2 && srf->order[1] > 2 )
	{
		rt_nurb_s_eval( srf, u, v, se);
		
		p = 0.0;
		for( i = 0; i < srf->u.k_size -1; i++)
		{
			if ( srf->u.knots[i] <= u 
				&& u < srf->u.knots[i+1] )
			{
				p = srf->u.knots[i];

				if (u == p)
					p = srf->u.knots[i+1];
				if ( u == p && i > 1)
					p = srf->u.knots[i-1];
			}
		}

		rt_nurb_s_eval( srf, p, v, ue);
		
		vsrf = (struct face_g_snurb *) rt_nurb_s_diff(srf, RT_NURB_SPLIT_COL);

		rt_nurb_s_eval(vsrf, u, v, ve);

		if( RT_NURB_IS_PT_RATIONAL(srf->pt_type) )
		{
			fastf_t w, inv_w;
			
			w = se[3];
			inv_w = 1.0 / w;

			ve[0] = (inv_w * ve[0]) -
				ve[3] / (w * w) * se[0];
			ve[1] = (inv_w * ve[1]) -
				ve[3] / (w * w) * se[1];
			ve[2] = (inv_w * ve[2]) -
				ve[3] / (w * w) * se[2];

			ue[0] = ue[0] / ue[3];
			ue[1] = ue[1] / ue[3];
			ue[2] = ue[2] / ue[3];
			ue[3] = ue[3] / ue[3];

			se[0] = se[0] / se[3];
			se[1] = se[1] / se[3];
			se[2] = se[2] / se[3];
			se[3] = se[3] / se[3];
		}
		
		VSUB2(uvec, se, ue);
		
		VCROSS(norm, uvec, ve);
		VUNITIZE(norm);

		rt_nurb_free_snurb(vsrf, (struct resource *)NULL);
		return;
	}
	if( srf->order[1] == 2 && srf->order[0] > 2 )
	{
		rt_nurb_s_eval( srf, u, v, se);
		
		p = 0.0;
		for( i = 0; i < srf->v.k_size -1; i++)
		{
			if ( srf->v.knots[i] <= v 
				&& v < srf->v.knots[i+1] )
			{
				p = srf->v.knots[i];

				if (v == p)
					p = srf->u.knots[i+1];
				if ( v == p && i > 1)
					p = srf->v.knots[i-1];
			}
		}

		rt_nurb_s_eval( srf, u, p, ve);

		usrf = (struct face_g_snurb *) rt_nurb_s_diff(srf, RT_NURB_SPLIT_ROW);
		
		rt_nurb_s_eval(usrf, u, v, ue);

		if( RT_NURB_IS_PT_RATIONAL(srf->pt_type) )
		{
			fastf_t w, inv_w;
			
			w = se[3];
			inv_w = 1.0 / w;

			ue[0] = (inv_w * ue[0]) -
				ue[3] / (w * w) * se[0];
			ue[1] = (inv_w * ue[1]) -
				ue[3] / (w * w) * se[1];
			ue[2] = (inv_w * ue[2]) -
				ue[3] / (w * w) * se[2];

			ve[0] = ve[0] / ve[3];
			ve[1] = ve[1] / ve[3];
			ve[2] = ve[2] / ve[3];
			ve[3] = ve[3] / ve[3];

			se[0] = se[0] / se[3];
			se[1] = se[1] / se[3];
			se[2] = se[2] / se[3];
			se[3] = se[3] / se[3];
		}

		VSUB2(vvec, se, ve);
		
		VCROSS(norm, ue, vvec);
		VUNITIZE(norm);		

		rt_nurb_free_snurb(usrf, (struct resource *)NULL);
		return;
	}
	
	/* Case Non Rational (order > 2, order > 2) */
	if( !RT_NURB_IS_PT_RATIONAL(srf->pt_type))
	{

		usrf = (struct face_g_snurb *) rt_nurb_s_diff( srf, RT_NURB_SPLIT_ROW);
		vsrf = (struct face_g_snurb *) rt_nurb_s_diff( srf, RT_NURB_SPLIT_COL);

		rt_nurb_s_eval(usrf, u,v, ue);
		rt_nurb_s_eval(vsrf, u,v, ve);
		
		VCROSS( norm, ue, ve);
		VUNITIZE( norm);

		rt_nurb_free_snurb(usrf, (struct resource *)NULL);
		rt_nurb_free_snurb(vsrf, (struct resource *)NULL);
		
		return;
	}

	/* Case Rational (order > 2, order > 2) */
	if( RT_NURB_IS_PT_RATIONAL(srf->pt_type))
	{
		fastf_t w, inv_w;
		vect_t unorm, vnorm;
		int i;

		rt_nurb_s_eval(srf, u, v, se);

		usrf = (struct face_g_snurb *) rt_nurb_s_diff( srf, RT_NURB_SPLIT_ROW);
		vsrf = (struct face_g_snurb *) rt_nurb_s_diff( srf, RT_NURB_SPLIT_COL);

		rt_nurb_s_eval(usrf, u,v, ue);

		rt_nurb_s_eval(vsrf, u,v, ve);
		
		w = se[3];
		inv_w = 1.0 / w;

		for(i = 0; i < 3; i++)
		{
			unorm[i] = (inv_w * ue[i]) -
				ue[3] / (w*w) * se[i];
			vnorm[i] = (inv_w * ve[i]) -
				ve[3] / (w*w) * se[i];
		}

		VCROSS( norm, unorm, vnorm);
		VUNITIZE( norm);

		rt_nurb_free_snurb(usrf, (struct resource *)NULL);
		rt_nurb_free_snurb(vsrf, (struct resource *)NULL);
		
		return;
	}
	return;
}
