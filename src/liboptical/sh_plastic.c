/*                    S H _ P L A S T I C . C
 * BRL-CAD
 *
 * Copyright (c) 1998-2020 United States Government as represented by
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
/** @file liboptical/sh_plastic.c
 *
 * Notes -
 * The normals on all surfaces point OUT of the solid.
 * The incoming light rays point IN.  Thus the sign change.
 *
 */

#include "common.h"

#include <stddef.h>
#include <stdio.h>
#include <math.h>

#include "vmath.h"
#include "raytrace.h"
#include "optical.h"
#include "optical/light.h"
#include "optical/plastic.h"
#include "photonmap.h"



/* Fast approximation to specular term */
#define PHAST_PHONG 1	/* See Graphics Gems IV pg 387 */

/* from view.c */
extern double AmbientIntensity;


struct bu_structparse phong_parse[] = {
    {"%d",	1, "shine",		PL_O(shine),		BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%d",	1, "sh",		PL_O(shine),		BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%g",	1, "specular",		PL_O(wgt_specular),	BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%g",	1, "sp",		PL_O(wgt_specular),	BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%g",	1, "diffuse",		PL_O(wgt_diffuse),	BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%g",	1, "di",		PL_O(wgt_diffuse),	BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%g",	1, "transmit",		PL_O(transmit),		BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%g",	1, "tr",		PL_O(transmit),		BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%g",	1, "reflect",		PL_O(reflect),		BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%g",	1, "re",		PL_O(reflect),		BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%g",	1, "ri",		PL_O(refrac_index),	BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%g",	1, "extinction_per_meter", PL_O(extinction),	BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%g",	1, "extinction",	PL_O(extinction),	BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%g",	1, "ex",		PL_O(extinction),	BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%g",	3, "emission",		PL_O(emission),		BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%g",	3, "em",		PL_O(emission),		BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"",	0, (char *)0,		0,			BU_STRUCTPARSE_FUNC_NULL, NULL, NULL }
};


HIDDEN int phong_setup(register struct region *rp, struct bu_vls *matparm, void **dpp, const struct mfuncs *mfp, struct rt_i *rtip);
HIDDEN int mirror_setup(register struct region *rp, struct bu_vls *matparm, void **dpp, const struct mfuncs *mfp, struct rt_i *rtip);
HIDDEN int glass_setup(register struct region *rp, struct bu_vls *matparm, void **dpp, const struct mfuncs *mfp, struct rt_i *rtip);
HIDDEN int phong_render(register struct application *ap, const struct partition *pp, struct shadework *swp, void *dp);
HIDDEN void phong_print(register struct region *rp, void *dp);
HIDDEN void phong_free(void *cp);

/* This can't be const, so the forward link can be written later */
struct mfuncs phg_mfuncs[] = {
    {MF_MAGIC,	"default",	0,		MFI_NORMAL,	0,     phong_setup,	phong_render,	phong_print,	phong_free },
    {MF_MAGIC,	"phong",	0,		MFI_NORMAL,	0,     phong_setup,	phong_render,	phong_print,	phong_free },
    {MF_MAGIC,	"plastic",	0,		MFI_NORMAL,	0,     phong_setup,	phong_render,	phong_print,	phong_free },
    {MF_MAGIC,	"mirror",	0,		MFI_NORMAL,	0,     mirror_setup,	phong_render,	phong_print,	phong_free },
    {MF_MAGIC,	"glass",	0,		MFI_NORMAL,	0,     glass_setup,	phong_render,	phong_print,	phong_free },
    {0,		(char *)0,	0,		0,	0,     0,		0,		0,		0 }
};


#ifndef PHAST_PHONG
extern double phg_ipow();
#endif

#define RI_AIR 1.0    /* Refractive index of air.		*/

HIDDEN int
phong_setup(register struct region *rp, struct bu_vls *matparm, void **dpp, const struct mfuncs *mfp, struct rt_i *UNUSED(rtip))
{
    register struct phong_specific *pp;

    BU_CK_VLS(matparm);
    BU_GET(pp, struct phong_specific);
    *dpp = pp;

    pp->magic = PL_MAGIC;
    pp->shine = 10;
    pp->wgt_specular = 0.7;
    pp->wgt_diffuse = 0.3;
    pp->transmit = 0.0;
    pp->reflect = 0.0;
    pp->refrac_index = RI_AIR;
    pp->extinction = 0.0;
    pp->mfp = mfp;

    if (bu_struct_parse(matparm, phong_parse, (char *)pp, NULL) < 0) {
	BU_PUT(pp, struct phong_specific);
	return -1;
    }

    if (pp->transmit > 0)
	rp->reg_transmit = 1;
    return 1;
}


HIDDEN int
mirror_setup(register struct region *rp, struct bu_vls *matparm, void **dpp, const struct mfuncs *mfp, struct rt_i *UNUSED(rtip))
{
    register struct phong_specific *pp;

    BU_CK_VLS(matparm);
    BU_GET(pp, struct phong_specific);
    *dpp = pp;

    pp->magic = PL_MAGIC;
    pp->shine = 4;
    pp->wgt_specular = 0.6;
    pp->wgt_diffuse = 0.4;
    pp->transmit = 0.0;
    pp->reflect = 0.75;
    pp->refrac_index = 1.65;
    pp->extinction = 0.0;
    pp->mfp = mfp;

    if (bu_struct_parse(matparm, phong_parse, (char *)pp, NULL) < 0) {
	BU_PUT(pp, struct phong_specific);
	return -1;
    }

    if (pp->transmit > 0)
	rp->reg_transmit = 1;
    return 1;
}


HIDDEN int
glass_setup(register struct region *rp, struct bu_vls *matparm, void **dpp, const struct mfuncs *mfp, struct rt_i *UNUSED(rtip))
{
    register struct phong_specific *pp;

    BU_CK_VLS(matparm);
    BU_GET(pp, struct phong_specific);
    *dpp = pp;

    pp->magic = PL_MAGIC;
    pp->shine = 4;
    pp->wgt_specular = 0.7;
    pp->wgt_diffuse = 0.3;
    pp->transmit = 0.8;
    pp->reflect = 0.1;
    /* leaving 0.1 for diffuse/specular */
    pp->refrac_index = 1.65;
    pp->extinction = 0.0;
    pp->mfp = mfp;

    if (bu_struct_parse(matparm, phong_parse, (char *)pp, NULL) < 0) {
	BU_PUT(pp, struct phong_specific);
	return -1;
    }

    if (pp->transmit > 0)
	rp->reg_transmit = 1;
    return 1;
}


HIDDEN void
phong_print(register struct region *rp, void *dp)
{
    bu_struct_print(rp->reg_name, phong_parse, (char *)dp);
}


HIDDEN void
phong_free(void *cp)
{
    BU_PUT(cp, struct phong_specific);
}


/*
 Color pixel based on the energy of a point light source (Eps)
 plus some diffuse illumination (Epd) reflected from the point
 <x, y> :

 E = Epd + Eps (1)

 The energy reflected from diffuse illumination is the product
 of the reflectance coefficient at point P (Rp) and the diffuse
 illumination (Id) :

 Epd = Rp * Id (2)

 The energy reflected from the point light source is calculated
 by the sum of the diffuse reflectance (Rd) and the specular
 reflectance (Rs), multiplied by the intensity of the light
 source (Ips) :

 Eps = (Rd + Rs) * Ips (3)

 The diffuse reflectance is calculated by the product of the
 reflectance coefficient (Rp) and the cosine of the angle of
 incidence (I) :

 Rd = Rp * cos(I)	(4)

 The specular reflectance is calculated by the product of the
 specular reflectance coefficient and (the cosine of the angle (S)
 raised to the nth power) :

 Rs = W(I) * cos(S)**n (5)

 Where,
 I is the angle of incidence.
 S is the angle between the reflected ray and the observer.
 W returns the specular reflection coefficient as a function
 of the angle of incidence.
 n (roughly 1 to 10) represents the shininess of the surface.
 *
 This is the heart of the lighting model which is based on a model
 developed by Bui-Tuong Phong, [see Wm M. Newman and R. F. Sproull,
 "Principles of Interactive Computer Graphics", 	McGraw-Hill, 1979]

 Er = Ra(m)*cos(Ia) + Rd(m)*cos(I1) + W(I1, m)*cos(s)^^n
 where,

 Er	is the energy reflected in the observer's direction.
 Ra	is the diffuse reflectance coefficient at the point
 of intersection due to ambient lighting.
 Ia	is the angle of incidence associated with the ambient
 light source (angle between ray direction (negated) and
 surface normal).
 Rd	is the diffuse reflectance coefficient at the point
 of intersection due to primary lighting.
 I1	is the angle of incidence associated with the primary
 light source (angle between light source direction and
 surface normal).
 m	is the material identification code.
 W	is the specular reflectance coefficient,
 a function of the angle of incidence, range 0.0 to 1.0,
 for the material.
 s	is the angle between the reflected ray and the observer.
 `	n	'Shininess' of the material,  range 1 to 10.
*/
HIDDEN int
phong_render(register struct application *ap, const struct partition *pp, struct shadework *swp, void *dp)
{
    struct light_specific *lp;
    fastf_t *intensity;
    fastf_t dist;
    point_t pt;
    vect_t color;
    fastf_t *to_light;
    fastf_t cosine;
    fastf_t refl;
    int i;
    vect_t reflected;
    vect_t work;

    point_t matcolor;		/* Material color */
    struct phong_specific *ps =
	(struct phong_specific *)dp;

    if (!ps || ps->magic != PL_MAGIC)
	bu_bomb("phong_render: bad magic\n");

    if (pp == NULL)
	bu_bomb("phong_render: bad partition\n");

    if (optical_debug&OPTICAL_DEBUG_SHADE)
	bu_struct_print("phong_render", phong_parse, (char *)ps);

    swp->sw_transmit = ps->transmit;
    swp->sw_reflect = ps->reflect;
    swp->sw_refrac_index = ps->refrac_index;
    swp->sw_extinction = ps->extinction;

    if (swp->sw_xmitonly) {
	if (swp->sw_xmitonly > 1)
	    return 1;	/* done -- wanted parameters only */
	if (swp->sw_reflect > 0 || swp->sw_transmit > 0) {
	    if (optical_debug&OPTICAL_DEBUG_SHADE)
		bu_log("calling rr_render from phong, sw_xmitonly\n");
	    (void)rr_render(ap, pp, swp);
	}
	return 1;	/* done */
    }


    VMOVE(matcolor, swp->sw_color);

    /* Photon Mapping */
    color[0]= swp->sw_color[0];
    color[1]= swp->sw_color[1];
    color[2]= swp->sw_color[2];

    if (!PM_Visualize)
    {
	/* Diffuse reflectance from "Ambient" light source (at eye) */
	if ((cosine = -VDOT(swp->sw_hit.hit_normal, ap->a_ray.r_dir)) > 0.0) {
	    if (cosine > 1.00001) {
		bu_log("cosAmb=1+%g %s surfno=%d (x%d, y%d, lvl%d)\n",
		       cosine-1,
		       pp->pt_inseg->seg_stp->st_dp->d_namep,
		       swp->sw_hit.hit_surfno,
		       ap->a_x, ap->a_y, ap->a_level);
		VPRINT(" normal", swp->sw_hit.hit_normal);
		VPRINT(" r_dir ", ap->a_ray.r_dir);
		cosine = 1;
	    }
	    cosine *= AmbientIntensity;

	    VSCALE(swp->sw_color, matcolor, cosine);
	} else {
	    VSETALL(swp->sw_color, 0);
	}

	/* Emission.  0..1 is normal range, -1..0 sucks light out, like OpenGL */
	VADD2(swp->sw_color, swp->sw_color, ps->emission);

	/* With the advent of procedural shaders, the caller can no longer
	 * provide us reliable light visibility information.  The hit point
	 * may have been changed by another shader in a stack.  There is no
	 * way that anyone else can tell us whether lights are visible.
	 */
	light_obs(ap, swp, ps->mfp->mf_inputs);

	/* Consider effects of each light source */
	for (i=ap->a_rt_i->rti_nlights-1; i>=0; i--) {

	    if ((lp = (struct light_specific *)swp->sw_visible[i]) == LIGHT_NULL)
		continue;

	    if (optical_debug & OPTICAL_DEBUG_LIGHT) {
		bu_log("phong_render light=%s lightfract=%g\n",
		       lp->lt_name, swp->sw_lightfract[i]);
	    }

	    /* Light is not shadowed -- add this contribution */
	    intensity = swp->sw_intensity+3*i;
	    to_light = swp->sw_tolight+3*i;

	    /* Diffuse reflectance from this light source. */
	    if ((cosine=VDOT(swp->sw_hit.hit_normal, to_light)) > 0.0) {
		if (cosine > 1.00001) {
		    bu_log("cosI=1+%g (x%d, y%d, lvl%d)\n", cosine-1,
			   ap->a_x, ap->a_y, ap->a_level);
		    cosine = 1;
		}
		/* Get Obj Hit Point For Attenuation */
		if (PM_Activated) {
		    VJOIN1(pt, ap->a_ray.r_pt, pp->pt_inhit->hit_dist, ap->a_ray.r_dir);
		    dist= sqrt((pt[0]-lp->lt_pos[0])*(pt[0]-lp->lt_pos[0]) + (pt[1]-lp->lt_pos[1])*(pt[1]-lp->lt_pos[1]) + (pt[2]-lp->lt_pos[2])*(pt[2]-lp->lt_pos[2]))/1000.0;
		    dist= (1.0/(0.1 + 1.0*dist + 0.01*dist*dist));
		    refl= dist * ps->wgt_diffuse * cosine * swp->sw_lightfract[i] * lp->lt_intensity;
		    /* bu_log("pt: [%.3f][%.3f, %.3f, %.3f]\n", dist, pt[0], pt[1], pt[2]);*/
		} else
		{
		    refl= ps->wgt_diffuse * swp->sw_lightfract[i] * cosine * lp->lt_fraction;
		}

		VELMUL3(work, matcolor, lp->lt_color, intensity);
		VJOIN1(swp->sw_color, swp->sw_color, refl, work);
	    }

	    /* Calculate specular reflectance.
	     * Reflected ray = (2 * cos(i) * Normal) - Incident ray.
	     * Cos(s) = Reflected ray DOT Incident ray.
	     */
	    cosine *= 2;
	    VSCALE(work, swp->sw_hit.hit_normal, cosine);
	    VSUB2(reflected, work, to_light);
	    if ((cosine = -VDOT(reflected, ap->a_ray.r_dir)) > 0) {
		if (cosine > 1.00001) {
		    bu_log("cosS=1+%g (x%d, y%d, lvl%d)\n", cosine-1,
			   ap->a_x, ap->a_y, ap->a_level);
		    cosine = 1;
		}
		refl = ps->wgt_specular * swp->sw_lightfract[i] *
		    lp->lt_fraction *
#ifdef PHAST_PHONG
		    /* It is unnecessary to compute the actual
		     * exponential here since phong is just a
		     * gross hack.  We approximate re:
		     * Graphics Gems IV "A Fast Alternative to
		     * Phong's Specular Model" Pg 385
		     */
		    cosine /
		    (ps->shine - ps->shine*cosine + cosine);
#else
		phg_ipow(cosine, ps->shine);
#endif /* PHAST_PHONG */
		VELMUL(work, lp->lt_color, intensity);
		VJOIN1(swp->sw_color, swp->sw_color, refl, work);
	    }
	}

	if (PM_Activated) {
	    IrradianceEstimate(ap, work, swp->sw_hit.hit_point, swp->sw_hit.hit_normal);
	    VELMUL(work, work, color);
	    VADD2(swp->sw_color, work, swp->sw_color);
	    if (swp->sw_color[0] > 1.0) swp->sw_color[0]= 1.0;
	    if (swp->sw_color[1] > 1.0) swp->sw_color[1]= 1.0;
	    if (swp->sw_color[2] > 1.0) swp->sw_color[2]= 1.0;
	}

    } else {

	if (PM_Activated) {
	    /* IrradianceEstimate(work, swp->sw_hit.hit_point, swp->sw_hit.hit_normal);
	       VELMUL(swp->sw_color, work, color);*/
	    IrradianceEstimate(ap, swp->sw_color, swp->sw_hit.hit_point, swp->sw_hit.hit_normal);
	    if (swp->sw_color[0] > 1.0) swp->sw_color[0]= 1.0;
	    if (swp->sw_color[1] > 1.0) swp->sw_color[1]= 1.0;
	    if (swp->sw_color[2] > 1.0) swp->sw_color[2]= 1.0;
	}
    }


    if (swp->sw_reflect > 0 || swp->sw_transmit > 0)
	(void)rr_render(ap, pp, swp);

    return 1;
}


#ifndef PHAST_PHONG
/*
 * Raise a floating point number to an integer power
 */
double
phg_ipow(d, cnt)
    double d;
    register int cnt;
{
    fastf_t input, result;

    if ((input=d) < 1e-8) return 0.0;
    if (cnt < 0 || cnt > 200) {
	bu_log("phg_ipow(%g, %d) bad\n", d, cnt);
	return d;
    }
    result = 1;
    while (cnt-- > 0)
	result *= input;
    return result;
}
#endif

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
