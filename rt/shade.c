/*
 *			S H A D E . C
 *
 *	Ray Tracing program, lighting model shader interface.
 *
 *  Notes -
 *	The normals on all surfaces point OUT of the solid.
 *	The incomming light rays point IN.
 *
 *  Authors -
 *	Michael John Muuss
 *	Phil Dykstra
 *  
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1989 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static char RCSview[] = "@(#)$Header$ (BRL)";
#endif

#include "conf.h"

#include <stdio.h>
#include <math.h>

#include "machine.h"
#include "vmath.h"
#include "rtlist.h"
#include "raytrace.h"
#include "./ext.h"
#include "./rdebug.h"
#include "./material.h"
#include "./mathtab.h"
#include "./light.h"

extern int	light_hit(), light_miss();	/* in light.c */

HIDDEN void	shade_inputs();


/*
 *			V I E W S H A D E
 *
 *  Call the material-specific shading function, after making certain
 *  that all shadework fields desired have been provided.
 *
 *  Returns -
 *	0 on failure
 *	1 on success
 *
 *	But of course, nobody cares what this returns.
 *	Everyone calls us as (void)viewshade()
 */
int
viewshade( ap, pp, swp )
struct application *ap;
register CONST struct partition *pp;
register struct shadework *swp;
{
	register struct mfuncs *mfp;
	register struct region *rp;
	register struct light_specific *lp;
	register int	want;

	RT_AP_CHECK(ap);
	RT_CK_RTI(ap->a_rt_i);

	swp->sw_hit = *(pp->pt_inhit);		/* struct copy */

	if( (mfp = (struct mfuncs *)pp->pt_regionp->reg_mfuncs) == MF_NULL )  {
		rt_log("viewshade:  reg_mfuncs NULL\n");
		return(0);
	}
	if( mfp->mf_magic != MF_MAGIC )  {
		rt_log("viewshade:  reg_mfuncs bad magic, %x != %x\n",
			mfp->mf_magic, MF_MAGIC );
		return(0);
	}

	rp = pp->pt_regionp;
	RT_CK_REGION(rp);

	/* Default color is white (uncolored) */
	if( rp->reg_mater.ma_override )  {
		VMOVE( swp->sw_color, rp->reg_mater.ma_color );
	}
	VMOVE( swp->sw_basecolor, swp->sw_color );

	if( swp->sw_hit.hit_dist < 0.0 )
		swp->sw_hit.hit_dist = 0.0;	/* Eye inside solid */
	ap->a_cumlen += swp->sw_hit.hit_dist;

	want = mfp->mf_inputs;

	/* If light information is not needed, set the light
	 * array to "safe" values,
	 * and claim that the light is visible, in case they are used.
	 */
	if( swp->sw_xmitonly )  want &= ~MFI_LIGHT;
	if( !(want & MFI_LIGHT) )  {
		register int	i;

		/* sanity */
		i=0;
		for( RT_LIST_FOR( lp, light_specific, &(LightHead.l) ) )  {
			RT_CK_LIGHT(lp);
			swp->sw_visible[i++] = (char *)lp;
		}
		for( ; i < SW_NLIGHTS; i++ )  {
			swp->sw_visible[i] = (char *)NULL;
		}
	}

	/* If optional inputs are required, have them computed */
	if( want & (MFI_HIT|MFI_NORMAL|MFI_LIGHT|MFI_UV) )  {
		VJOIN1( swp->sw_hit.hit_point, ap->a_ray.r_pt,
			swp->sw_hit.hit_dist, ap->a_ray.r_dir );
		swp->sw_inputs |= MFI_HIT;
	}
	if( (swp->sw_inputs & want) != want )  {
		shade_inputs( ap, pp, swp, want );
	} else if( !(want & MFI_LIGHT) )  {
		register int	i;

		/* sanity */
		for( i = SW_NLIGHTS-1; i >= 0; i-- )  {
			swp->sw_visible[i] = (char *)NULL;
		}
	}

	if( rdebug&RDEBUG_SHADE ) {
		rt_log("About to shade %s: using \"%s\" shader\n",
			rp->reg_name, mfp->mf_name);
		pr_shadework( "before mf_render", swp );
	}


	/* Invoke the actual shader (may be a tree of them) */
	(void)mfp->mf_render( ap, pp, swp, rp->reg_udata );

	if( rdebug&RDEBUG_SHADE ) {
		pr_shadework( "after mf_render", swp );
		rt_log("\n");
	}

	return(1);
}





/*
 *			S H A D E _ I N P U T S
 *
 *  Compute the necessary fields in the shadework structure.
 *
 *  Note that only hit_dist is valid in pp_inhit.
 *  Must calculate it if hit_norm is needed,
 *  after which pt_inflip must be handled.
 *  RT_HIT_UVCOORD() must have hit_point computed
 *  in advance.
 *
 *  If MFI_LIGHT is not on, the presumption is that the sw_visible[]
 *  array is not needed, or has been handled elsewhere.
 */
HIDDEN void
shade_inputs( ap, pp, swp, want )
struct application *ap;
register struct partition *pp;
register struct shadework *swp;
register int	want;
{
	register int	have;

	/* These calcuations all have MFI_HIT as a pre-requisite */
	if( want & (MFI_NORMAL|MFI_LIGHT|MFI_UV) )
		want |= MFI_HIT;

	have = swp->sw_inputs;
	want &= ~have;		/* we don't want what we already have */

	if( want & MFI_HIT )  {
		VJOIN1( swp->sw_hit.hit_point, ap->a_ray.r_pt,
			swp->sw_hit.hit_dist, ap->a_ray.r_dir );
		have |= MFI_HIT;
	}

	if( want & MFI_NORMAL )  {
		if( pp->pt_inhit->hit_dist < 0.0 )  {
			/* Eye inside solid, orthoview */
			VREVERSE( swp->sw_hit.hit_normal, ap->a_ray.r_dir );
		} else {
			FAST fastf_t f;
			/* Get surface normal for hit point */
			/* Stupid SysV CPP needs this on one line */
			RT_HIT_NORMAL( swp->sw_hit.hit_normal, &(swp->sw_hit), pp->pt_inseg->seg_stp, &(ap->a_ray), pp->pt_inflip );

#ifdef never
			if( swp->sw_hit.hit_normal[X] < -1.01 || swp->sw_hit.hit_normal[X] > 1.01 ||
			    swp->sw_hit.hit_normal[Y] < -1.01 || swp->sw_hit.hit_normal[Y] > 1.01 ||
			    swp->sw_hit.hit_normal[Z] < -1.01 || swp->sw_hit.hit_normal[Z] > 1.01 )  {
			    	VPRINT("shade_inputs: N", swp->sw_hit.hit_normal);
				VSET( swp->sw_color, 9, 9, 0 );	/* Yellow */
				return;
			}
#endif
			/* Check to make sure normals are OK */
			if( (f=VDOT( ap->a_ray.r_dir, swp->sw_hit.hit_normal )) > 0 )  {
				rt_log("shade_inputs(%s) flip N xy=%d,%d %s surf=%d dot=%g\n",
					pp->pt_inseg->seg_stp->st_name,
					ap->a_x, ap->a_y,
					rt_functab[pp->pt_inseg->seg_stp->st_id].ft_name,
					swp->sw_hit.hit_surfno, f);
				if( rdebug&RDEBUG_SHADE ) {
					VPRINT("Dir ", ap->a_ray.r_dir);
					VPRINT("Norm", swp->sw_hit.hit_normal);
				}
			}
		}
		have |= MFI_NORMAL;
	}
	if( want & MFI_UV )  {
		if( pp->pt_inhit->hit_dist < 0.0 )  {
			/* Eye inside solid, orthoview */
			swp->sw_uv.uv_u = swp->sw_uv.uv_v = 0.5;
			swp->sw_uv.uv_du = swp->sw_uv.uv_dv = 0;
		} else {
			RT_HIT_UVCOORD(	ap, pp->pt_inseg->seg_stp,
				&(swp->sw_hit), &(swp->sw_uv) );
		}
		if( swp->sw_uv.uv_u < 0 || swp->sw_uv.uv_u > 1 ||
		    swp->sw_uv.uv_v < 0 || swp->sw_uv.uv_v > 1 )  {
			rt_log("shade_inputs:  bad u,v=%e,%e du,dv=%g,%g seg=%s %s surf=%d. xy=%d,%d Making green.\n",
				swp->sw_uv.uv_u, swp->sw_uv.uv_v,
				swp->sw_uv.uv_du, swp->sw_uv.uv_dv,
				pp->pt_inseg->seg_stp->st_name,
		    		rt_functab[pp->pt_inseg->seg_stp->st_id].ft_name,
		    		pp->pt_inhit->hit_surfno,
				ap->a_x, ap->a_y );
			VSET( swp->sw_color, 0, 9, 0 );	/* Green */
			return;
		}
		have |= MFI_UV;
	}
	if( want & MFI_LIGHT )  {
		light_visibility(ap, swp, have);
		have |= MFI_LIGHT;
	}

	/* Record which fields were filled in */
	swp->sw_inputs = have;

	if( (want & have) != want )
		rt_log("shade_inputs:  unable to satisfy request for x%x\n", want);
}


