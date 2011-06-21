/*                         S H A D E . C
 * BRL-CAD
 *
 * Copyright (c) 1989-2011 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file liboptical/shade.c
 *
 * Ray Tracing program, lighting model shader interface.
 *
 * Notes -
 * The normals on all surfaces point OUT of the solid.
 * The incomming light rays point IN.
 *
 */

#include "common.h"

#include <stdio.h>
#include <math.h>

#include "bu.h"
#include "bn.h"
#include "vmath.h"
#include "raytrace.h"
#include "optical.h"
#include "light.h"
#include "plot3.h"


#ifdef RT_MULTISPECTRAL
#  include "spectrum.h"
#endif


/**
 * P R _ S H A D E W O R K
 *
 * Pretty print a shadework structure.
 */
void
pr_shadework(const char *str, const struct shadework *swp)
{
    int i;

    if (!swp)
	return;

    bu_log("Shadework%s: 0x%x\n", str ? str : "", swp);
    bu_printb(" sw_inputs", swp->sw_inputs, MFI_FORMAT);
    if (swp->sw_inputs & MFI_HIT)
	bu_log(" sw_hit.dist:%g @ sw_hit.point(%g %g %g)\n",
	       swp->sw_hit.hit_dist,
	       V3ARGS(swp->sw_hit.hit_point));
    else
	bu_log(" sw_hit.dist:%g\n", swp->sw_hit.hit_dist);

    if (swp->sw_inputs && MFI_NORMAL)
	bu_log(" sw_hit.normal(%g %g %g)\n",
	       V3ARGS(swp->sw_hit.hit_normal));


    bu_log(" sw_transmit %f\n", swp->sw_transmit);
    bu_log(" sw_reflect %f\n", swp->sw_reflect);
    bu_log(" sw_refract_index %f\n", swp->sw_refrac_index);
    bu_log(" sw_extinction %f\n", swp->sw_extinction);
#ifdef RT_MULTISPECTRAL
    bn_pr_tabdata("msw_color", swp->msw_color);
    bn_pr_tabdata("msw_basecolor", swp->msw_basecolor);
#else
    VPRINT(" sw_color", swp->sw_color);
    VPRINT(" sw_basecolor", swp->sw_basecolor);
#endif
    bu_log(" sw_uv  %f %f\n", swp->sw_uv.uv_u, swp->sw_uv.uv_v);
    bu_log(" sw_dudv  %f %f\n", swp->sw_uv.uv_du, swp->sw_uv.uv_dv);
    bu_log(" sw_xmitonly %d\n", swp->sw_xmitonly);
    bu_log("\n");
    if (swp->sw_inputs & MFI_LIGHT) for (i=0; i < SW_NLIGHTS; i++) {
	    if (swp->sw_visible[i] == (struct light_specific *)NULL) continue;
	    RT_CK_LIGHT(swp->sw_visible[i]);
#ifdef RT_MULTISPECTRAL
	    bu_log("   light %d visible, dir=(%g, %g, %g)\n",
		   i,
		   V3ARGS(&swp->sw_tolight[i*3]));
	    BN_CK_TABDATA(swp->msw_intensity[i]);
	    bn_pr_tabdata("light intensity", swp->msw_intensity[i]);
#else
	    bu_log("   light %d visible, intensity=%g, dir=(%g, %g, %g)\n",
		   i,
		   swp->sw_intensity[i],
		   V3ARGS(&swp->sw_tolight[i*3]));
#endif
	}
}


/**
 * S H A D E _ I N P U T S
 *
 * Compute the necessary fields in the shadework structure.
 *
 * Note that only hit_dist is valid in pp_inhit.  Must calculate it if
 * hit_norm is needed, after which pt_inflip must be handled.
 * RT_HIT_UVCOORD() must have hit_point computed in advance.
 *
 * If MFI_LIGHT is not on, the presumption is that the sw_visible[]
 * array is not needed, or has been handled elsewhere.
 */
void
shade_inputs(struct application *ap, const struct partition *pp, struct shadework *swp, int want)
{
    register int have;

    RT_CK_RAY(swp->sw_hit.hit_rayp);

    /* These calcuations all have MFI_HIT as a pre-requisite */
    if (want & (MFI_NORMAL|MFI_LIGHT|MFI_UV))
	want |= MFI_HIT;

    have = swp->sw_inputs;
    want &= ~have;		/* we don't want what we already have */

    if (want & MFI_HIT) {
	VJOIN1(swp->sw_hit.hit_point, ap->a_ray.r_pt,
	       swp->sw_hit.hit_dist, ap->a_ray.r_dir);
	have |= MFI_HIT;
    }

    if (want & MFI_NORMAL) {
	if (pp->pt_inhit->hit_dist < 0.0) {
	    /* Eye inside solid, orthoview */
	    VREVERSE(swp->sw_hit.hit_normal, ap->a_ray.r_dir);
	} else {
	    register fastf_t f;
	    /* Get surface normal for hit point */
	    RT_HIT_NORMAL(swp->sw_hit.hit_normal, &(swp->sw_hit), pp->pt_inseg->seg_stp, &(ap->a_ray), pp->pt_inflip);

#ifdef never
	    if (swp->sw_hit.hit_normal[X] < -1.01 || swp->sw_hit.hit_normal[X] > 1.01 ||
		swp->sw_hit.hit_normal[Y] < -1.01 || swp->sw_hit.hit_normal[Y] > 1.01 ||
		swp->sw_hit.hit_normal[Z] < -1.01 || swp->sw_hit.hit_normal[Z] > 1.01) {
		VPRINT("shade_inputs: N", swp->sw_hit.hit_normal);
		VSET(swp->sw_color, 9, 9, 0);	/* Yellow */
		return;
	    }
#endif
	    /* Check to make sure normals are OK */
	    f = VDOT(ap->a_ray.r_dir, swp->sw_hit.hit_normal);
	    if (f > 0.0 &&
		!BN_VECT_ARE_PERP(f, &(ap->a_rt_i->rti_tol))) {
		static int counter = 0;
		if (counter++ < 100 || (R_DEBUG&RDEBUG_SHADE)) {
		    bu_log("shade_inputs(%s) flip N xy=%d, %d %s surf=%d dot=%g\n",
			   pp->pt_inseg->seg_stp->st_name,
			   ap->a_x, ap->a_y,
			   rt_functab[pp->pt_inseg->seg_stp->st_id].ft_name,
			   swp->sw_hit.hit_surfno, f);
		} else {
		    if (counter++ == 101) {
			bu_log("shade_inputs(%s) flipped normals detected, additional reporting suppressed\n",
			       pp->pt_inseg->seg_stp->st_name);
		    }
		}
		if (R_DEBUG&RDEBUG_SHADE) {
		    VPRINT("Dir ", ap->a_ray.r_dir);
		    VPRINT("Norm", swp->sw_hit.hit_normal);
		}
		/* reverse the normal so it's lit */
		VREVERSE(swp->sw_hit.hit_normal, swp->sw_hit.hit_normal);
	    }
	}
	if (R_DEBUG&(RDEBUG_RAYPLOT|RDEBUG_SHADE)) {
	    point_t endpt;
	    fastf_t f;
	    /* Plot the surface normal -- green/blue */
	    f = ap->a_rt_i->rti_radius * 0.02;
	    VJOIN1(endpt, swp->sw_hit.hit_point,
		   f, swp->sw_hit.hit_normal);
	    if (R_DEBUG&RDEBUG_RAYPLOT) {
		bu_semaphore_acquire(BU_SEM_SYSCALL);
		pl_color(stdout, 0, 255, 255);
		pdv_3line(stdout, swp->sw_hit.hit_point, endpt);
		bu_semaphore_release(BU_SEM_SYSCALL);
	    }
	    bu_log("Surface normal for shader:\n\
hit pt: %g %g %g end pt: %g %g %g\n",
		   V3ARGS(swp->sw_hit.hit_point),
		   V3ARGS(endpt));

	}
	have |= MFI_NORMAL;
    }
    if (want & MFI_UV) {
	if (pp->pt_inhit->hit_dist < 0.0) {
	    /* Eye inside solid, orthoview */
	    swp->sw_uv.uv_u = swp->sw_uv.uv_v = 0.5;
	    swp->sw_uv.uv_du = swp->sw_uv.uv_dv = 0;
	} else {
	    RT_HIT_UVCOORD(ap, pp->pt_inseg->seg_stp,
			   &(swp->sw_hit), &(swp->sw_uv));
	}
	if (swp->sw_uv.uv_u < 0 || swp->sw_uv.uv_u > 1 ||
	    swp->sw_uv.uv_v < 0 || swp->sw_uv.uv_v > 1) {
	    bu_log("shade_inputs:  bad u, v=%e, %e du, dv=%g, %g seg=%s %s surf=%d. xy=%d, %d Making green.\n",
		   swp->sw_uv.uv_u, swp->sw_uv.uv_v,
		   swp->sw_uv.uv_du, swp->sw_uv.uv_dv,
		   pp->pt_inseg->seg_stp->st_name,
		   rt_functab[pp->pt_inseg->seg_stp->st_id].ft_name,
		   pp->pt_inhit->hit_surfno,
		   ap->a_x, ap->a_y);
#ifdef RT_MULTISPECTRAL
	    {
		static const float green[3] = {0.0f, 9.0f, 0.0f};
		rt_spect_reflectance_rgb(swp->msw_color, green);
	    }
#else
	    VSET(swp->sw_color, 0, 9, 0);	/* Hyper-Green */
#endif

	    return;
	}
	have |= MFI_UV;
    }
    /* NOTE:  Lee has changed the shaders to do light themselves now. */
    /* This isn't where light visibility is determined any more. */
    if (want & MFI_LIGHT) {
	light_obs(ap, swp, have);
	have |= MFI_LIGHT;
    }

    /* Record which fields were filled in */
    swp->sw_inputs = have;

    if ((want & have) != want)
	bu_log("shade_inputs:  unable to satisfy request for x%x\n", want);
}


/**
 * V I E W S H A D E
 *
 * Call the material-specific shading function, after making certain
 * that all shadework fields desired have been provided.
 *
 * Returns -
 * 0 on failure
 * 1 on success
 *
 * But of course, nobody cares what this returns.  Everyone calls us
 * as (void)viewshade()
 */
int
viewshade(struct application *ap, const struct partition *pp, struct shadework *swp)
{
    register const struct mfuncs *mfp;
    register const struct region *rp;
    struct light_specific *lp;
    register int want;

    RT_AP_CHECK(ap);
    RT_CK_RTI(ap->a_rt_i);
    RT_CK_PT(pp);
    RT_CK_HIT(pp->pt_inhit);
    RT_CK_RAY(pp->pt_inhit->hit_rayp);
    rp = pp->pt_regionp;
    RT_CK_REGION(rp);
    mfp = (struct mfuncs *)pp->pt_regionp->reg_mfuncs;
    RT_CK_MF(mfp);

    if (!swp || !mfp) {
	if (R_DEBUG&RDEBUG_SHADE) {
	    bu_log("ERROR: NULL shadework or mfuncs structure encountered\n");
	}
	return 0;
    }

    want = mfp->mf_inputs;

    if (R_DEBUG&RDEBUG_SHADE) {
	bu_log("viewshade(%s)\n Using \"%s\" shader, ",
	       rp->reg_name, mfp->mf_name);
	bu_printb("mfp_inputs", want, MFI_FORMAT);
	bu_log("\n");
    }

    swp->sw_hit = *(pp->pt_inhit);		/* struct copy */

#ifdef RT_MULTISPECTRAL
    /* XXX where does region get reflectance?  Default temperature? */
    BN_CK_TABDATA(swp->msw_color);
    BN_CK_TABDATA(swp->msw_basecolor);
    if (rp->reg_mater.ma_color_valid) {
	rt_spect_reflectance_rgb(swp->msw_color, rp->reg_mater.ma_color);
    }
    bn_tabdata_copy(swp->msw_basecolor, swp->msw_color);
#else
    /* Default color is white (uncolored) */
    if (rp->reg_mater.ma_color_valid) {
	VMOVE(swp->sw_color, rp->reg_mater.ma_color);
    }
    VMOVE(swp->sw_basecolor, swp->sw_color);
#endif

    if (swp->sw_hit.hit_dist < 0.0)
	swp->sw_hit.hit_dist = 0.0;	/* Eye inside solid */
    ap->a_cumlen += swp->sw_hit.hit_dist;

    /* If light information is not needed, set the light array to
     * "safe" values, and claim that the light is visible, in case
     * they are used.
     */
    if (swp->sw_xmitonly) want &= ~MFI_LIGHT;
    if (!(want & MFI_LIGHT)) {
	register int i;

	/* sanity */
	i=0;
	for (BU_LIST_FOR(lp, light_specific, &(LightHead.l))) {
	    RT_CK_LIGHT(lp);
	    swp->sw_visible[i++] = lp;
	}
	for (; i < SW_NLIGHTS; i++) {
	    swp->sw_visible[i] = (struct light_specific *)NULL;
	}
    }

    /* If optional inputs are required, have them computed */
    if (want & (MFI_HIT|MFI_NORMAL|MFI_LIGHT|MFI_UV)) {
	VJOIN1(swp->sw_hit.hit_point, ap->a_ray.r_pt,
	       swp->sw_hit.hit_dist, ap->a_ray.r_dir);
	swp->sw_inputs |= MFI_HIT;
    }
    if ((swp->sw_inputs & want) != want) {
	shade_inputs(ap, pp, swp, want);
    } else if (!(want & MFI_LIGHT)) {
	register int i;

	/* sanity */
	for (i = SW_NLIGHTS-1; i >= 0; i--) {
	    swp->sw_visible[i] = (struct light_specific *)NULL;
	}
    }

    if (R_DEBUG&RDEBUG_SHADE) {
	pr_shadework("before mf_render", swp);
    }


    /* Invoke the actual shader (may be a tree of them) */
    if (mfp && mfp->mf_render)
	(void)mfp->mf_render(ap, pp, swp, rp->reg_udata);

    if (R_DEBUG&RDEBUG_SHADE) {
	pr_shadework("after mf_render", swp);
	bu_log("\n");
    }

    return 1;
}


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
