/*                       R E F R A C T . C
 * BRL-CAD
 *
 * Copyright (c) 1985-2011 United States Government as represented by
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
/** @file liboptical/refract.c
 *
 */

#include "common.h"

#include <stdio.h>
#include <string.h>
#include <math.h>

#include "vmath.h"
#include "mater.h"
#include "raytrace.h"
#include "optical.h"
#include "plot3.h"


extern int viewshade(struct application *ap,
		     register const struct partition *pp,
		     register struct shadework *swp);



int max_ireflect = 5;	/* Maximum internal reflection level */
int max_bounces = 5;	/* Maximum recursion level */

#define MSG_PROLOGUE 20		/* # initial messages to see */
#define MSG_INTERVAL 4000		/* message interval thereafter */

#define RI_AIR 1.0		/* Refractive index of air */

#define AIR_GAP_TOL 0.01		/* Max permitted air gap for RI tracking */


#ifdef RT_MULTISPECTRAL
#include "spectrum.h"
extern struct bn_tabdata *background;
#else
extern vect_t background;
#endif

/*
 * R R _ M I S S
 */
HIDDEN int
rr_miss(struct application *ap)
{
    RT_AP_CHECK(ap);
    return 1;	/* treat as escaping ray */
}


/*
 * R R _ H I T
 *
 * This routine is called when an internal reflection ray hits something
 * (which is ordinarily the case).
 *
 * Generally, there will be one or two partitions on the hit list.
 * The values for pt_outhit for the second partition should not be used,
 * as a_onehit was set to 3, getting a maximum of 3 valid hit points.
 *
 * Explicit Returns -
 * 0 dreadful internal error
 * 1 treat as escaping ray & reshoot
 * 2 Proper exit point determined, with Implicit Returns:
 *	a_uvec		exit Point
 *	a_vvec		exit Normal (inward pointing)
 *	a_refrac_index	RI of *next* material
 */
HIDDEN int
rr_hit(struct application *ap, struct partition *PartHeadp, struct seg *UNUSED(segp))
{
    register struct partition *pp;
    register struct hit *hitp;
    register struct soltab *stp;
    struct partition *psave = (struct partition *)NULL;
    struct shadework sw;
    struct application appl;
    int ret;

    RT_AP_CHECK(ap);

    RT_APPLICATION_INIT(&appl);

    for (pp=PartHeadp->pt_forw; pp != PartHeadp; pp = pp->pt_forw)
	if (pp->pt_outhit->hit_dist > 0.0) break;
    if (pp == PartHeadp) {
	if (R_DEBUG&(RDEBUG_SHOWERR|RDEBUG_REFRACT)) {
	    bu_log("rr_hit:  %d, %d no hit out front?\n",
		   ap->a_x, ap->a_y);
	    ret = 0;	/* error */
	    goto out;
	}
	ret = 1;		/* treat as escaping ray */
	goto out;
    }

    /*
     * Ensure that the partition we are given is part of the same
     * region that we started in.  When the internal reflection
     * is happening very near an edge or corner, this is not always
     * the case, and either (a) a small sliver of some other region
     * is found to be in the way, or (b) the ray completely misses the
     * region that it started in, although not by much.
     */
    psave = pp;
    if (R_DEBUG&RDEBUG_REFRACT) bu_log("rr_hit(%s)\n", psave->pt_regionp->reg_name);
    for (; pp != PartHeadp; pp = pp->pt_forw)
	if (pp->pt_regionp == (struct region *)(ap->a_uptr)) break;
    if (pp == PartHeadp) {
	if (R_DEBUG&(RDEBUG_SHOWERR|RDEBUG_REFRACT)) {
	    bu_log("rr_hit:  %d, %d Ray internal to %s landed unexpectedly in %s\n",
		   ap->a_x, ap->a_y,
		   ((struct region *)(ap->a_uptr))->reg_name,
		   psave->pt_regionp->reg_name);
	    ret = 0;	/* error */
	    goto out;
	}
	ret = 1;		/* treat as escaping ray */
	goto out;
    }

    /*
     * At one time, this was a check for pp->pt_inhit->hit_dist
     * being NEAR zero.  That was a mistake, because we may have
     * been at the edge of a subtracted out center piece when
     * internal reflection happened, except that floating point
     * error (being right on the surface of the interior solid)
     * prevented us from "seeing" that solid on the next ray,
     * causing our ray endpoints to be quite far from the starting
     * point, yet with the ray still validly inside the glass region.
     *
     * There is a major problem if the entry point
     * is further ahead than the firing point, ie, >0.
     *
     * Because this error has not yet been encountered, it is
     * considered dreadful.  Some recovery may be possible.
     *
     * For now, this seems to happen when a reflected ray starts outside
     * the glass and doesn't even intersect the glass, so treat it as
     * an escaping ray.
     */

    if (pp->pt_inhit->hit_dist > 10) {
	stp = pp->pt_inseg->seg_stp;
	if (R_DEBUG&RDEBUG_REFRACT)
	    bu_log("rr_hit: %d, %d %s inhit %g > 10.0! (treating as escaping ray)\n",
		   ap->a_x, ap->a_y,
		   pp->pt_regionp->reg_name,
		   pp->pt_inhit->hit_dist);
	ret = 1;	/* treat as escaping ray */
	goto out;
    }

    /*
     * If there is a very small crack in the glass, perhaps formed
     * by a small error when taking the Union of two solids,
     * attempt to find the real exit point.
     * NOTE that this is usually taken care of inside librt
     * in the bool_weave code, but it is inexpensive to check for it
     * here.  If this case is detected, push on, and log it.
     * This code is not expected to be needed.
     */
    while (pp->pt_forw != PartHeadp) {
	register fastf_t d;
	d = pp->pt_forw->pt_inhit->hit_dist - pp->pt_outhit->hit_dist;
	if (!NEAR_ZERO(d, AIR_GAP_TOL))
	    break;
	if (pp->pt_forw->pt_regionp != pp->pt_regionp)
	    break;
	if (R_DEBUG&(RDEBUG_SHOWERR|RDEBUG_REFRACT)) bu_log(
	    "rr_hit: %d, %d fusing small crack in glass %s\n",
	    ap->a_x, ap->a_y,
	    pp->pt_regionp->reg_name);
	pp = pp->pt_forw;
    }

    hitp = pp->pt_outhit;
    stp = pp->pt_outseg->seg_stp;
    if (hitp->hit_dist >= INFINITY) {
	bu_log("rr_hit: %d, %d infinite glass (%g, %g) %s\n",
	       ap->a_x, ap->a_y,
	       pp->pt_inhit->hit_dist, hitp->hit_dist,
	       pp->pt_regionp->reg_name);
	ret = 0;		/* dreadful error */
	goto out;
    }
    VJOIN1(hitp->hit_point, ap->a_ray.r_pt,
	   hitp->hit_dist, ap->a_ray.r_dir);
    RT_HIT_NORMAL(ap->a_vvec, hitp, stp, &(ap->a_ray), pp->pt_outflip);

    /* For refraction, want exit normal to point inward. */
    VREVERSE(ap->a_vvec, ap->a_vvec);
    VMOVE(ap->a_uvec, hitp->hit_point);
    ap->a_cumlen += (hitp->hit_dist - pp->pt_inhit->hit_dist);

    ap->a_refrac_index = RI_AIR;			/* Default medium: air */

    /*
     * Look ahead, and see if there is more glass to come.
     * If so, obtain its refractive index, to enable correct
     * calculation of the departing refraction angle.
     */
    if (pp->pt_forw != PartHeadp) {
	register fastf_t d;
	d = pp->pt_forw->pt_inhit->hit_dist - hitp->hit_dist;
	if (NEAR_ZERO(d, AIR_GAP_TOL)) {
	    /*
	     * Make a private copy of the application struct,
	     * because viewshade() may change various fields.
	     */
	    appl = *ap;			/* struct copy */

	    memset((char *)&sw, 0, sizeof(sw));
	    sw.sw_transmit = sw.sw_reflect = 0.0;

	    /* Set default in case shader doesn't fill this in. */
	    sw.sw_refrac_index = RI_AIR;

	    /* Set special flag so that we get only shader
	     * parameters (refractive index, in this case).
	     * We don't even care about transmitted energy.
	     */
	    sw.sw_xmitonly = 2;
	    sw.sw_inputs = 0;		/* no fields filled yet */
#ifdef RT_MULTISPECTRAL
	    sw.msw_color = bn_tabdata_get_constval(1.0, spectrum);
	    sw.msw_basecolor = bn_tabdata_get_constval(1.0, spectrum);
#else
	    VSETALL(sw.sw_color, 1);
	    VSETALL(sw.sw_basecolor, 1);
#endif

	    if (R_DEBUG&(RDEBUG_SHADE|RDEBUG_REFRACT))
		bu_log("rr_hit calling viewshade to discover refractive index\n");

	    (void)viewshade(&appl, pp->pt_forw, &sw);

#ifdef RT_MULTISPECTRAL
	    bu_free(sw.msw_color, "sw.msw_color");
	    bu_free(sw.msw_basecolor, "sw.msw_basecolor");
#endif

	    if (R_DEBUG&(RDEBUG_SHADE|RDEBUG_REFRACT))
		bu_log("rr_hit refractive index = %g\n", sw.sw_refrac_index);

	    if (sw.sw_transmit > 0) {
		ap->a_refrac_index = sw.sw_refrac_index;
		if (R_DEBUG&RDEBUG_SHADE) {
		    bu_log("rr_hit a_refrac_index=%g (trans=%g)\n",
			   ap->a_refrac_index,
			   sw.sw_transmit);
		}
		ret= 3;	/* OK -- more glass follows */
		goto out;
	    }
	}
    }
    ret = 2;				/* OK -- no more glass */
out:
    if (R_DEBUG&RDEBUG_REFRACT) bu_log("rr_hit(%s) return=%d\n",
				       psave ? psave->pt_regionp->reg_name : "",
				       ret);
    return ret;
}


/*
 * R E F R A C T
 *
 * Compute the refracted ray 'v_2' from the incident ray 'v_1' with
 * the refractive indices 'ri_2' and 'ri_1' respectively.
 * Using Schnell's Law:
 *
 * theta_1 = angle of v_1 with surface normal
 * theta_2 = angle of v_2 with reversed surface normal
 * ri_1 * sin(theta_1) = ri_2 * sin(theta_2)
 *
 * sin(theta_2) = ri_1/ri_2 * sin(theta_1)
 *
 * The above condition is undefined for ri_1/ri_2 * sin(theta_1)
 * being greater than 1, and this represents the condition for total
 * reflection, the 'critical angle' is the angle theta_1 for which
 * ri_1/ri_2 * sin(theta_1) equals 1.
 *
 * Returns TRUE if refracted, FALSE if reflected.
 *
 * Note:  output (v_2) can be same storage as an input.
 */
HIDDEN int
rr_refract(vect_t v_1, vect_t norml, double ri_1, double ri_2, vect_t v_2)
{
    vect_t w, u;
    fastf_t beta;

    if (NEAR_ZERO(ri_1, 0.0001) || NEAR_ZERO(ri_2, 0.0001)) {
	bu_log("rr_refract:ri1=%g, ri2=%g\n", ri_1, ri_2);
	beta = 1;
    } else {
	beta = ri_1/ri_2;		/* temp */
	if (beta > 10000) {
	    bu_log("rr_refract:  beta=%g\n", beta);
	    beta = 1000;
	}
    }
    VSCALE(w, v_1, beta);
    VCROSS(u, w, norml);

    /*
     *	|w X norml| = |w||norml| * sin(theta_1)
     *	        |u| = ri_1/ri_2 * sin(theta_1) = sin(theta_2)
     */
    if ((beta = VDOT(u, u)) > 1.0) {
	/* Past critical angle, total reflection.
	 * Calculate reflected (bounced) incident ray.
	 */
	if (R_DEBUG&RDEBUG_REFRACT) bu_log("rr_refract: reflected.  ri1=%g ri2=%g beta=%g\n",
					   ri_1, ri_2, beta);
	VREVERSE(u, v_1);
	beta = 2 * VDOT(u, norml);
	VSCALE(w, norml, beta);
	VSUB2(v_2, w, u);
	return 0;		/* reflected */
    } else {
	/*
	 * 1 - beta = 1 - sin(theta_2)^^2
	 *	    = cos(theta_2)^^2.
	 * beta = -1.0 * cos(theta_2) - Dot(w, norml).
	 */
	if (R_DEBUG&RDEBUG_REFRACT) bu_log("rr_refract: refracted.  ri1=%g ri2=%g beta=%g\n",
					   ri_1, ri_2, beta);
	beta = -sqrt(1.0 - beta) - VDOT(w, norml);
	VSCALE(u, norml, beta);
	VADD2(v_2, w, u);
	return 1;		/* refracted */
    }
    /* NOTREACHED */
}


/*
 * R R _ R E N D E R
 */
int
rr_render(register struct application *ap,
	  const struct partition *pp,
	  struct shadework *swp)
{
    struct application sub_ap;
    vect_t work;
    vect_t incident_dir;
    fastf_t shader_fract;
    fastf_t reflect;
    fastf_t transmit;

#ifdef RT_MULTISPECTRAL
    struct bn_tabdata *ms_filter_color = BN_TABDATA_NULL;
    struct bn_tabdata *ms_shader_color = BN_TABDATA_NULL;
    struct bn_tabdata *ms_reflect_color = BN_TABDATA_NULL;
    struct bn_tabdata *ms_transmit_color = BN_TABDATA_NULL;
#else
    vect_t filter_color;
    vect_t shader_color;
    vect_t reflect_color;
    vect_t transmit_color;
#endif

    fastf_t attenuation;
    vect_t to_eye;
    int code;

    RT_AP_CHECK(ap);

    RT_APPLICATION_INIT(&sub_ap);

#ifdef RT_MULTISPECTRAL
    sub_ap.a_spectrum = BN_TABDATA_NULL;
#endif

    /*
     * sw_xmitonly is set primarily for light visibility rays.
     * Need to compute (partial) transmission through to the light,
     * or procedural shaders won't be able to cast shadows
     * and light won't be able to get through glass
     * (including "stained glass" and "filter glass").
     *
     * On the other hand, light visibility rays shouldn't be refracted,
     * it is pointless to shoot at where the light isn't.
     */
    if (swp->sw_xmitonly) {
	/* Caller wants transmission term only, don't fire reflected rays */
	transmit = swp->sw_transmit + swp->sw_reflect;	/* Don't loose energy */
	reflect = 0;
    } else {
	reflect = swp->sw_reflect;
	transmit = swp->sw_transmit;
    }
    if (R_DEBUG&RDEBUG_REFRACT) {
	bu_log("rr_render(%s) START: lvl=%d reflect=%g, transmit=%g, xmitonly=%d\n",
	       pp->pt_regionp->reg_name,
	       ap->a_level,
	       reflect, transmit,
	       swp->sw_xmitonly);
    }
    if (reflect <= 0 && transmit <= 0)
	goto out;

    if (ap->a_level > max_bounces) {
	/* Nothing more to do for this ray */
	static long count = 0;		/* Not PARALLEL, should be OK */

	if ((R_DEBUG&(RDEBUG_SHOWERR|RDEBUG_REFRACT)) && (
		count++ < MSG_PROLOGUE ||
		(count%MSG_INTERVAL) == 3
		)) {
	    bu_log("rr_render: %d, %d MAX BOUNCES=%d: %s\n",
		   ap->a_x, ap->a_y,
		   ap->a_level,
		   pp->pt_regionp->reg_name);
	}

	/*
	 * Return the basic color of the object, ignoring the
	 * the fact that it is supposed to be
	 * filtering or reflecting light here.
	 * This is much better than returning just black,
	 * but something better might be done.
	 */
#ifdef RT_MULTISPECTRAL
	BN_CK_TABDATA(swp->msw_color);
	BN_CK_TABDATA(swp->msw_basecolor);
	bn_tabdata_copy(swp->msw_color, swp->msw_basecolor);
#else
	VMOVE(swp->sw_color, swp->sw_basecolor);
#endif
	ap->a_cumlen += pp->pt_inhit->hit_dist;
	goto out;
    }
#ifdef RT_MULTISPECTRAL
    BN_CK_TABDATA(swp->msw_basecolor);
    ms_filter_color = bn_tabdata_dup(swp->msw_basecolor);

#else
    VMOVE(filter_color, swp->sw_basecolor);
#endif

    if ((swp->sw_inputs & (MFI_HIT|MFI_NORMAL)) != (MFI_HIT|MFI_NORMAL))
	shade_inputs(ap, pp, swp, MFI_HIT|MFI_NORMAL);

    /*
     * If this ray is being fired from the exit point of
     * an object, and is directly entering another object,
     * (ie, there is no intervening air-gap), and
     * the two refractive indices match, then do not fire a
     * reflected ray -- just take the transmission contribution.
     * This is important, eg, for glass gun tubes projecting
     * through a glass armor plate. :-)
     */
    if (NEAR_ZERO(pp->pt_inhit->hit_dist, AIR_GAP_TOL)
	&& ZERO(ap->a_refrac_index - swp->sw_refrac_index))
    {
	transmit += reflect;
	reflect = 0;
    }

    /*
     * Diminish base color appropriately, and add in
     * contributions from mirror reflection & transparency
     */
    shader_fract = 1 - (reflect + transmit);
    if (shader_fract < 0) {
	shader_fract = 0;
    } else if (shader_fract >= 1) {
	goto out;
    }
    if (R_DEBUG&RDEBUG_REFRACT) {
	bu_log("rr_render: lvl=%d start shader=%g, reflect=%g, transmit=%g %s\n",
	       ap->a_level,
	       shader_fract, reflect, transmit,
	       pp->pt_regionp->reg_name);
    }
#ifdef RT_MULTISPECTRAL
    BN_GET_TABDATA(ms_shader_color, swp->msw_color->table);
    bn_tabdata_scale(ms_shader_color, swp->msw_color, shader_fract);
#else
    VSCALE(shader_color, swp->sw_color, shader_fract);
#endif

    /*
     * Compute transmission through an object.
     * There may be a mirror reflection, which will be handled
     * by the reflection code later
     */
    if (transmit > 0) {
	if (R_DEBUG&RDEBUG_REFRACT) {
	    bu_log("rr_render: lvl=%d transmit=%g.  Calculate refraction at entrance to %s.\n",
		   ap->a_level, transmit,
		   pp->pt_regionp->reg_name);
	}
	/*
	 * Calculate refraction at entrance.
	 */
	sub_ap = *ap;		/* struct copy */
#ifdef RT_MULTISPECTRAL
	sub_ap.a_spectrum = bn_tabdata_dup((struct bn_tabdata *)ap->a_spectrum);
#endif
	sub_ap.a_level = 0;	/* # of internal reflections */
	sub_ap.a_cumlen = 0;	/* distance through the glass */
	sub_ap.a_user = -1;	/* sanity */
	sub_ap.a_rbeam = ap->a_rbeam + swp->sw_hit.hit_dist * ap->a_diverge;
	sub_ap.a_diverge = 0.0;
	sub_ap.a_uptr = (genptr_t)(pp->pt_regionp);
	VMOVE(sub_ap.a_ray.r_pt, swp->sw_hit.hit_point);
	VMOVE(incident_dir, ap->a_ray.r_dir);

	/* If there is an air gap, reset ray's RI to air */
	if (pp->pt_inhit->hit_dist > AIR_GAP_TOL)
	    sub_ap.a_refrac_index = RI_AIR;

	if (!ZERO(sub_ap.a_refrac_index - swp->sw_refrac_index)
	    && !rr_refract(incident_dir,		/* input direction */
			   swp->sw_hit.hit_normal,	/* exit normal */
			   sub_ap.a_refrac_index,	/* current RI */
			   swp->sw_refrac_index,	/* next RI */
			   sub_ap.a_ray.r_dir		/* output direction */
		))
	{
	    /*
	     * Ray was mirror reflected back outside solid.
	     * Just add contribution to reflection,
	     * and quit.
	     */
	    reflect += transmit;
	    transmit = 0;
#ifdef RT_MULTISPECTRAL
	    ms_transmit_color = bn_tabdata_get_constval(0.0, spectrum);
#else
	    VSETALL(transmit_color, 0);
#endif
	    if (R_DEBUG&RDEBUG_REFRACT) {
		bu_log("rr_render: lvl=%d change xmit into reflection %s\n",
		       ap->a_level,
		       pp->pt_regionp->reg_name);
	    }
	    goto do_reflection;
	}
	if (R_DEBUG&RDEBUG_REFRACT) {
	    bu_log("rr_render: lvl=%d begin transmission through %s.\n",
		   ap->a_level,
		   pp->pt_regionp->reg_name);
	}

	/*
	 * Find new exit point from the inside.
	 * We will iterate, but not recurse, due to the special
	 * (non-recursing) hit and miss routines used here for
	 * internal reflection.
	 *
	 * a_onehit is set to 3, so that where possible,
	 * rr_hit() will be given three accurate hit points:
	 * the entry and exit points of this glass region,
	 * and the entry point into the next region.
	 * This permits calculation of the departing
	 * refraction angle based on the RI of the current and
	 * *next* regions along the ray.
	 */
	sub_ap.a_purpose = "rr first glass transmission ray";
	sub_ap.a_flag = 0;
    do_inside:
	sub_ap.a_hit =  rr_hit;
	sub_ap.a_miss = rr_miss;
	sub_ap.a_logoverlap = ap->a_logoverlap;
	sub_ap.a_onehit = 3;
	sub_ap.a_rbeam = ap->a_rbeam + swp->sw_hit.hit_dist * ap->a_diverge;
	sub_ap.a_diverge = 0.0;
	switch (code = rt_shootray(&sub_ap)) {
	    case 3:
		/* More glass to come.
		 * uvec=exit_pt, vvec=N, a_refrac_index = next RI.
		 */
		break;
	    case 2:
		/* No more glass to come.
		 * uvec=exit_pt, vvec=N, a_refrac_index = next RI.
		 */
		break;
	    case 1:
		/* Treat as escaping ray */
		if (R_DEBUG&RDEBUG_REFRACT)
		    bu_log("rr_refract: Treating as escaping ray\n");
		goto do_exit;
	    case 0:
	    default:
		/* Dreadful error */
#ifdef RT_MULTISPECTRAL
		bu_bomb("rr_refract: Stuck in glass. Very green pixel, unsupported in multi-spectral mode\n");
#else
		VSET(swp->sw_color, 0, 99, 0); /* very green */
#endif
		goto out;			/* abandon hope */
	}

	if (R_DEBUG&RDEBUG_REFRACT) {
	    bu_log("rr_render: calculating refraction @ exit from %s (green)\n", pp->pt_regionp->reg_name);
	    bu_log("Start point to exit point:\n\
vdraw open rr;vdraw params c 00ff00; vdraw write n 0 %g %g %g; vdraw wwrite n 1 %g %g %g; vdraw send\n",
		   V3ARGS(sub_ap.a_ray.r_pt),
		   V3ARGS(sub_ap.a_uvec));
	}
	/* NOTE: rr_hit returns EXIT Point in sub_ap.a_uvec,
	 * and returns EXIT Normal in sub_ap.a_vvec,
	 * and returns next RI in sub_ap.a_refrac_index
	 */
	if (R_DEBUG&RDEBUG_RAYWRITE) {
	    wraypts(sub_ap.a_ray.r_pt,
		    sub_ap.a_ray.r_dir,
		    sub_ap.a_uvec,
		    2, ap, stdout);	/* 2 = ?? */
	}
	if (R_DEBUG&RDEBUG_RAYPLOT) {
	    /* plotfp */
	    bu_semaphore_acquire(BU_SEM_SYSCALL);
	    pl_color(stdout, 0, 255, 0);
	    pdv_3line(stdout,
		      sub_ap.a_ray.r_pt,
		      sub_ap.a_uvec);
	    bu_semaphore_release(BU_SEM_SYSCALL);
	}
	/* Advance.  Exit point becomes new start point */
	VMOVE(sub_ap.a_ray.r_pt, sub_ap.a_uvec);
	VMOVE(incident_dir, sub_ap.a_ray.r_dir);

	/*
	 * Calculate refraction at exit point.
	 * Use "look ahead" RI value from rr_hit.
	 */
	if (!ZERO(sub_ap.a_refrac_index - swp->sw_refrac_index)
	    && !rr_refract(incident_dir,		/* input direction */
			   sub_ap.a_vvec,		/* exit normal */
			   swp->sw_refrac_index,	/* current RI */
			   sub_ap.a_refrac_index,	/* next RI */
			   sub_ap.a_ray.r_dir		/* output direction */
		))
	{
	    static long count = 0;		/* not PARALLEL, should be OK */

	    /* Reflected internally -- keep going */
	    if ((++sub_ap.a_level) <= max_ireflect) {
		sub_ap.a_purpose = "rr reflected internal ray, probing for glass exit point";
		sub_ap.a_flag = 0;
		goto do_inside;
	    }

	    /*
	     * Internal Reflection limit exceeded -- just let
	     * the ray escape, continuing on current course.
	     * This will cause some energy from somewhere in the
	     * sceen to be received through this glass,
	     * which is much better than just returning
	     * grey or black, as before.
	     */
	    if ((R_DEBUG&(RDEBUG_SHOWERR|RDEBUG_REFRACT)) && (
		    count++ < MSG_PROLOGUE ||
		    (count%MSG_INTERVAL) == 3
		    )) {
		bu_log("rr_render: %d, %d Int.reflect=%d: %s lvl=%d\n",
		       sub_ap.a_x, sub_ap.a_y,
		       sub_ap.a_level,
		       pp->pt_regionp->reg_name,
		       ap->a_level);
	    }
	    VMOVE(sub_ap.a_ray.r_dir, incident_dir);
	    goto do_exit;
	}
    do_exit:
	/*
	 * Compute internal spectral transmittance.
	 * Bouger's law.  pg 30 of "color science"
	 *
	 * Apply attenuation factor due to thickness of the glass.
	 * sw_extinction is in terms of fraction of light absorbed
	 * per linear meter of glass.  a_cumlen is in mm.
	 */
/* XXX extinction should be a spectral curve, not scalor */
	if (swp->sw_extinction > 0 && sub_ap.a_cumlen > 0) {
	    attenuation = pow(10.0, -1.0e-3 * sub_ap.a_cumlen *
			      swp->sw_extinction);
	} else {
	    attenuation = 1;
	}

	/*
	 * Process the escaping refracted ray.
	 * This is the only place we might recurse dangerously,
	 * so we are careful to use our caller's recursion level+1.
	 * NOTE: point & direction already filled in
	 */
	sub_ap.a_hit =  ap->a_hit;
	sub_ap.a_miss = ap->a_miss;
	sub_ap.a_logoverlap = ap->a_logoverlap;
	sub_ap.a_onehit = ap->a_onehit;
	sub_ap.a_level = ap->a_level+1;
	sub_ap.a_uptr = ap->a_uptr;
	sub_ap.a_rbeam = ap->a_rbeam + swp->sw_hit.hit_dist * ap->a_diverge;
	sub_ap.a_diverge = 0.0;
	if (code == 3) {
	    sub_ap.a_purpose = "rr recurse on next glass";
	    sub_ap.a_flag = 0;
	} else {
	    sub_ap.a_purpose = "rr recurse on escaping internal ray";
	    sub_ap.a_flag = 1;
	    sub_ap.a_onehit = sub_ap.a_onehit > -3 ? -3 : sub_ap.a_onehit;
	}
	/* sub_ap.a_refrac_index was set to RI of next material by rr_hit().
	 */
	sub_ap.a_cumlen = 0;
	(void) rt_shootray(&sub_ap);

	/* a_user has hit/miss flag! */
	if (sub_ap.a_user == 0) {
#ifdef RT_MULTISPECTRAL
	    bn_tabdata_copy(ms_transmit_color, background);
#else
	    VMOVE(transmit_color, background);
#endif
	    sub_ap.a_cumlen = 0;
	} else {
#ifdef RT_MULTISPECTRAL
	    bn_tabdata_copy(ms_transmit_color, sub_ap.a_spectrum);
#else
	    VMOVE(transmit_color, sub_ap.a_color);
#endif
	}
	transmit *= attenuation;
#ifdef RT_MULTISPECTRAL
	bn_tabdata_mul(ms_transmit_color, ms_filter_color, ms_transmit_color);
#else
	VELMUL(transmit_color, filter_color, transmit_color);
#endif
	if (R_DEBUG&RDEBUG_REFRACT) {
	    bu_log("rr_render: lvl=%d end of xmit through %s\n",
		   ap->a_level,
		   pp->pt_regionp->reg_name);
	}
    } else {
#ifdef RT_MULTISPECTRAL
	bn_tabdata_constval(ms_transmit_color, 0.0);
#else
	VSETALL(transmit_color, 0);
#endif
    }

    /*
     * Handle any reflection, including mirror reflections
     * detected by the transmission code, above.
     */
do_reflection:
#ifdef RT_MULTISPECTRAL
    if (sub_ap.a_spectrum) {
	bu_free(sub_ap.a_spectrum, "rr_render: sub_ap.a_spectrum bn_tabdata*");
	sub_ap.a_spectrum = BN_TABDATA_NULL;
    }
#endif
    if (reflect > 0) {
	register fastf_t f;

	/* Mirror reflection */
	if (R_DEBUG&RDEBUG_REFRACT)
	    bu_log("rr_render: calculating mirror reflection off of %s\n", pp->pt_regionp->reg_name);
	sub_ap = *ap;		/* struct copy */
#ifdef RT_MULTISPECTRAL
	sub_ap.a_spectrum = bn_tabdata_dup((struct bn_tabdata *)ap->a_spectrum);
#endif
	sub_ap.a_rbeam = ap->a_rbeam + swp->sw_hit.hit_dist * ap->a_diverge;
	sub_ap.a_diverge = 0.0;
	sub_ap.a_level = ap->a_level+1;
	sub_ap.a_onehit = -1;	/* Require at least one non-air hit */
	VMOVE(sub_ap.a_ray.r_pt, swp->sw_hit.hit_point);
	VREVERSE(to_eye, ap->a_ray.r_dir);
	f = 2 * VDOT(to_eye, swp->sw_hit.hit_normal);
	VSCALE(work, swp->sw_hit.hit_normal, f);
	/* I have been told this has unit length */
	VSUB2(sub_ap.a_ray.r_dir, work, to_eye);
	sub_ap.a_purpose = "rr reflected ray";
	sub_ap.a_flag = 0;

	if (R_DEBUG&(RDEBUG_RAYPLOT|RDEBUG_REFRACT)) {
	    point_t endpt;
	    /* Plot the surface normal -- green/blue */
	    /* plotfp */
	    f = sub_ap.a_rt_i->rti_radius * 0.02;
	    VJOIN1(endpt, sub_ap.a_ray.r_pt,
		   f, swp->sw_hit.hit_normal);
	    if (R_DEBUG&RDEBUG_RAYPLOT) {
		bu_semaphore_acquire(BU_SEM_SYSCALL);
		pl_color(stdout, 0, 255, 255);
		pdv_3line(stdout, sub_ap.a_ray.r_pt, endpt);
		bu_semaphore_release(BU_SEM_SYSCALL);
	    }
	    bu_log("Surface normal for reflection:\n\
vdraw open rrnorm;vdraw params c 00ffff;vdraw write n 0 %g %g %g;vdraw write n 1 %g %g %g;vdraw send\n",
		   V3ARGS(sub_ap.a_ray.r_pt),
		   V3ARGS(endpt));

	}

	(void)rt_shootray(&sub_ap);

	/* a_user has hit/miss flag! */
	if (sub_ap.a_user == 0) {
	    /* MISS */
#ifdef RT_MULTISPECTRAL
	    bn_tabdata_copy(ms_reflect_color, background);
#else
	    VMOVE(reflect_color, background);
#endif
	} else {
	    ap->a_cumlen += sub_ap.a_cumlen;
#ifdef RT_MULTISPECTRAL
	    bn_tabdata_copy(ms_reflect_color, sub_ap.a_spectrum);
#else
	    VMOVE(reflect_color, sub_ap.a_color);
#endif
	}
    } else {
#ifdef RT_MULTISPECTRAL
	bn_tabdata_constval(ms_reflect_color, 0.0);
#else
	VSETALL(reflect_color, 0);
#endif
    }

    /*
     * Collect the contributions to the final color
     */
#ifdef RT_MULTISPECTRAL
    bn_tabdata_join2(swp->msw_color, ms_shader_color,
		     reflect, ms_reflect_color,
		     transmit, ms_transmit_color);
#else
    VJOIN2(swp->sw_color, shader_color,
	   reflect, reflect_color,
	   transmit, transmit_color);
#endif
    if (R_DEBUG&RDEBUG_REFRACT) {
	bu_log("rr_render: lvl=%d end shader=%g reflect=%g, transmit=%g %s\n",
	       ap->a_level,
	       shader_fract, reflect, transmit,
	       pp->pt_regionp->reg_name);
#ifdef RT_MULTISPECTRAL
	{
	    struct bu_vls str;
	    bu_vls_init(&str);
	    bu_vls_strcat(&str, "ms_shader_color: ");
	    bn_tabdata_to_tcl(&str, ms_shader_color);
	    bu_vls_strcat(&str, "\nms_reflect_color: ");
	    bn_tabdata_to_tcl(&str, ms_reflect_color);
	    bu_vls_strcat(&str, "\nms_transmit_color: ");
	    bn_tabdata_to_tcl(&str, ms_transmit_color);
	    bu_log("rr_render: %s\n", bu_vls_addr(&str));
	    bu_vls_free(&str);
	}
#else
	VPRINT("shader  ", shader_color);
	VPRINT("reflect ", reflect_color);
	VPRINT("transmit", transmit_color);
#endif
    }
out:
    if (R_DEBUG&RDEBUG_REFRACT) {
#ifdef RT_MULTISPECTRAL
	{
	    struct bu_vls str;
	    bu_vls_init(&str);
	    bu_vls_strcat(&str, "final swp->msw_color: ");
	    bn_tabdata_to_tcl(&str, swp->msw_color);
	    bu_log("rr_render: %s\n", bu_vls_addr(&str));
	    bu_vls_free(&str);
	}
#else
	VPRINT("final   ", swp->sw_color);
#endif
    }

    /* Release all the dynamic spectral curves */
#ifdef RT_MULTISPECTRAL
    if (ms_filter_color) bu_free(ms_filter_color, "rr_render: ms_filter_color bn_tabdata*");
    if (ms_shader_color) bu_free(ms_shader_color, "rr_render: ms_shader_color bn_tabdata*");
    if (ms_reflect_color) bu_free(ms_reflect_color, "rr_render: ms_reflect_color bn_tabdata*");
    if (sub_ap.a_spectrum) bu_free(sub_ap.a_spectrum, "rr_render: sub_ap.a_spectrum bn_tabdata*");
#endif

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
