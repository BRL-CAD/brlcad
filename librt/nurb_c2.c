/*	N U R B  _ C 2 . C
 *
 *  Function -
 *	Given parametric u,v values, return the curvature of the
 *	surface.
 *
 *  Author -
 *	Paul Randal Stay
 * 
 *  Source -
 * 	SECAD/VLD Computing Consortium, Bldg 394
 *	The U.S. Army Ballistic Research Laboratory
 * 	Aberdeen Proving Ground, Maryland 21005
 *
 *  Copyright Notice -
 *	This software is Copyright (C) 1990 by the United States Army.
 *	All rights reserved.
 */

#include "conf.h"

#include <stdio.h>
#include <math.h>

#include "machine.h"
#include "vmath.h"
#include "raytrace.h"
#include "nurb.h"

void
rt_nurb_curvature(cvp, srf, u, v)
struct curvature *cvp;
CONST struct face_g_snurb * srf;
fastf_t	u;
fastf_t v;
{
	struct face_g_snurb * us, *vs, * uus, * vvs, *uvs;
	fastf_t ue[4], ve[4], uue[4], vve[4], uve[4], se[4];
        fastf_t         E, F, G;                /* First Fundamental Form */
        fastf_t         L, M, N;                /* Second Fundamental form */
        fastf_t         denom;
        fastf_t         wein[4];                /*Weingarten matrix */
        fastf_t         evec[3];
        fastf_t         mean, gauss, discrim;
        vect_t          norm;
	int 		i;

	us = rt_nurb_s_diff(srf, RT_NURB_SPLIT_ROW);
	vs = rt_nurb_s_diff(srf, RT_NURB_SPLIT_COL);
	uus = rt_nurb_s_diff(us, RT_NURB_SPLIT_ROW);
	vvs = rt_nurb_s_diff(vs, RT_NURB_SPLIT_COL);
	uvs = rt_nurb_s_diff(vs, RT_NURB_SPLIT_ROW);
	
	rt_nurb_s_eval(srf, u, v, se);
	rt_nurb_s_eval(us, u,v, ue);
	rt_nurb_s_eval(vs, u,v, ve);
	rt_nurb_s_eval(uus, u,v, uue);
	rt_nurb_s_eval(vvs, u,v, uve);
	rt_nurb_s_eval(uvs, u,v, uve);

	rt_nurb_free_snurb( us, (struct resource *)NULL);
	rt_nurb_free_snurb( vs, (struct resource *)NULL);
	rt_nurb_free_snurb( uus, (struct resource *)NULL);
	rt_nurb_free_snurb( vvs, (struct resource *)NULL);
	rt_nurb_free_snurb( uvs, (struct resource *)NULL);

	if( RT_NURB_IS_PT_RATIONAL( srf->pt_type ))
	{
		for( i = 0; i < 3; i++)
		{
			ue[i] = (1.0 / se[3] * ue[i]) -
				(ue[3]/se[3]) * se[0]/se[3];
			ve[i] = (1.0 / se[3] * ve[i]) -
				(ve[3]/se[3]) * se[0]/se[3];
		}
		VCROSS(norm, ue, ve);
		VUNITIZE(norm);
		E = VDOT( ue, ue);
		F = VDOT( ue, ve);
		G = VDOT( ve, ve);
		
		for( i = 0; i < 3; i++)
		{
			uue[i] = (1.0 / se[3] * uue[i]) -
				2 * (uue[3]/se[3]) * uue[i] -
				uue[3]/se[3] * (se[i]/se[3]);

			vve[i] = (1.0 / se[3] * vve[i]) -
				2 * (vve[3]/se[3]) * vve[i] -
				vve[3]/se[3] * (se[i]/se[3]);

			 uve[i] = 1.0 / se[3] * uve[i] +
	                        (-1.0 / (se[3] * se[3])) *
        	                (ve[3] * ue[i] + ue[3] * ve[i] +
                	         uve[3] * se[i]) + 
				(-2.0 / (se[3] * se[3] * se[3])) *
	                        (ve[3] * ue[3] * se[i]);
		}

		L = VDOT( norm, uue);
		M = VDOT( norm, uve);
		N = VDOT( norm, vve);
		
	} else
	{
		VCROSS( norm, ue, ve);
		VUNITIZE( norm );
		E = VDOT( ue, ue);
		F = VDOT( ue, ve);
		G = VDOT( ve, ve);
		
		L = VDOT( norm, uue);
		M = VDOT( norm, uve);
		N = VDOT( norm, vve);
	}

	if( srf->order[0] <= 2 && srf->order[1] <= 2)
	{
		cvp->crv_c1 = cvp->crv_c2 = 0;
		vec_ortho(cvp->crv_pdir, norm);
		return;
	}

	denom = ( (E*G) - (F*F) );
	gauss = (L * N - M *M)/denom;
	mean = (G * L + E * N - 2 * F * M) / (2 * denom);
	discrim = sqrt( mean * mean - gauss);
	
	cvp->crv_c1 = mean - discrim;
	cvp->crv_c2 = mean + discrim;

	if( fabs( E*G - F*F) < 0.0001 )		/* XXX */
	{
		bu_log("rt_nurb_curvature: first fundamental form is singular E = %g F= %g G = %g\n",
			E,F,G);
		vec_ortho(cvp->crv_pdir, norm);	/* sanity */
		return;
	}

        wein[0] = ( (G * L) - (F * M))/ (denom);
        wein[1] = ( (G * M) - (F * N))/ (denom);
        wein[2] = ( (E * M) - (F * L))/ (denom);
        wein[3] = ( (E * N) - (F * M))/ (denom);

	if( fabs(wein[1]) < 0.0001 && fabs( wein[3] - cvp->crv_c1 ) < 0.0001 )
        {
                evec[0] = 0.0; evec[1] = 1.0;
        } else
        {
                evec[0] = 1.0;
                if( fabs( wein[1] ) > fabs( wein[3] - cvp->crv_c1) )
                {
                        evec[1] = (cvp->crv_c1 - wein[0]) / wein[1];
                } else
                {
                        evec[1] = wein[2] / ( cvp->crv_c1 - wein[3] );
                }
        }

	cvp->crv_pdir[0] = evec[0] * ue[0] + evec[1] * ve[0];
        cvp->crv_pdir[1] = evec[0] * ue[1] + evec[1] * ve[1];
        cvp->crv_pdir[2] = evec[0] * ue[2] + evec[1] * ve[2];
	VUNITIZE( cvp->crv_pdir);
}
