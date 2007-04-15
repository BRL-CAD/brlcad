/*                    S H _ P L A S T I C . C
 * BRL-CAD
 *
 * Copyright (c) 1998-2007 United States Government as represented by
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
/** @file sh_plastic.c
 *			P L A S T I C
 *
 *  Notes -
 *	The normals on all surfaces point OUT of the solid.
 *	The incomming light rays point IN.  Thus the sign change.
 *
 *  Authors -
 *	Michael John Muuss
 *	Gary S. Moss
 *
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5068  USA
 */
#ifndef lint
static const char RCSid[] = "@(#)$Header$ (ARL)";
#endif

#include "common.h"

#include <stddef.h>
#include <stdio.h>
#include <math.h>

#include "machine.h"
#include "vmath.h"
#include "mater.h"
#include "raytrace.h"
#include "rtprivate.h"
#include "light.h"
#include "plastic.h"
#include "photonmap.h"

#ifdef RT_MULTISPECTRAL
#  include "spectrum.h"
extern const struct bn_table	*spectrum;
#endif


extern int rr_render(struct application	*ap,
		     struct partition	*pp,
		     struct shadework   *swp);
/* Fast approximation to specular term */
#define PHAST_PHONG 1	/* See Graphics Gems IV pg 387 */

/* from view.c */
extern double AmbientIntensity;


struct bu_structparse phong_parse[] = {
    {"%d",	1, "shine",		PL_O(shine),		BU_STRUCTPARSE_FUNC_NULL },
    {"%d",	1, "sh",		PL_O(shine),		BU_STRUCTPARSE_FUNC_NULL },
    {"%f",	1, "specular",		PL_O(wgt_specular),	BU_STRUCTPARSE_FUNC_NULL },
    {"%f",	1, "sp",		PL_O(wgt_specular),	BU_STRUCTPARSE_FUNC_NULL },
    {"%f",	1, "diffuse",		PL_O(wgt_diffuse),	BU_STRUCTPARSE_FUNC_NULL },
    {"%f",	1, "di",		PL_O(wgt_diffuse),	BU_STRUCTPARSE_FUNC_NULL },
    {"%f",	1, "transmit",		PL_O(transmit),		BU_STRUCTPARSE_FUNC_NULL },
    {"%f",	1, "tr",		PL_O(transmit),		BU_STRUCTPARSE_FUNC_NULL },
    {"%f",	1, "reflect",		PL_O(reflect),		BU_STRUCTPARSE_FUNC_NULL },
    {"%f",	1, "re",		PL_O(reflect),		BU_STRUCTPARSE_FUNC_NULL },
    {"%f",	1, "ri",		PL_O(refrac_index),	BU_STRUCTPARSE_FUNC_NULL },
    {"%f",	1, "extinction_per_meter", PL_O(extinction),	BU_STRUCTPARSE_FUNC_NULL },
    {"%f",	1, "extinction",	PL_O(extinction),	BU_STRUCTPARSE_FUNC_NULL },
    {"%f",	1, "ex",		PL_O(extinction),	BU_STRUCTPARSE_FUNC_NULL },
    {"%f",	3, "emission",		PL_O(emission),		BU_STRUCTPARSE_FUNC_NULL },
    {"%f",	3, "em",		PL_O(emission),		BU_STRUCTPARSE_FUNC_NULL },
    {"",	0, (char *)0,		0,			BU_STRUCTPARSE_FUNC_NULL }
};

HIDDEN int phong_setup(register struct region *rp, struct bu_vls *matparm, char **dpp, struct mfuncs *mfp, struct rt_i *rtip), mirror_setup(register struct region *rp, struct bu_vls *matparm, char **dpp, struct mfuncs *mfp, struct rt_i *rtip), glass_setup(register struct region *rp, struct bu_vls *matparm, char **dpp, struct mfuncs *mfp, struct rt_i *rtip);
HIDDEN int phong_render(register struct application *ap, struct partition *pp, struct shadework *swp, char *dp);
HIDDEN void	phong_print(register struct region *rp, char *dp);
HIDDEN void	phong_free(char *cp);

/* This can't be const, so the forward link can be written later */
struct mfuncs phg_mfuncs[] = {
    {MF_MAGIC,	"default",	0,		MFI_NORMAL,	0,
     phong_setup,	phong_render,	phong_print,	phong_free },

    {MF_MAGIC,	"phong",	0,		MFI_NORMAL,	0,
     phong_setup,	phong_render,	phong_print,	phong_free },

    {MF_MAGIC,	"plastic",	0,		MFI_NORMAL,	0,
     phong_setup,	phong_render,	phong_print,	phong_free },

    {MF_MAGIC,	"mirror",	0,		MFI_NORMAL,	0,
     mirror_setup,	phong_render,	phong_print,	phong_free },

    {MF_MAGIC,	"glass",	0,		MFI_NORMAL,	0,
     glass_setup,	phong_render,	phong_print,	phong_free },

    {0,		(char *)0,	0,		0,	0,
     0,		0,		0,		0 }
};

#ifndef PHAST_PHONG
extern double phg_ipow();
#endif

#define RI_AIR		1.0    /* Refractive index of air.		*/

/*
 *			P H O N G _ S E T U P
 */
HIDDEN int
phong_setup(register struct region *rp, struct bu_vls *matparm, char **dpp, struct mfuncs *mfp, struct rt_i *rtip)
{
    register struct phong_specific *pp;

    BU_CK_VLS( matparm );
    BU_GETSTRUCT( pp, phong_specific );
    *dpp = (char *)pp;

    pp->magic = PL_MAGIC;
    pp->shine = 10;
    pp->wgt_specular = 0.7;
    pp->wgt_diffuse = 0.3;
    pp->transmit = 0.0;
    pp->reflect = 0.0;
    pp->refrac_index = RI_AIR;
    pp->extinction = 0.0;
    pp->mfp = mfp;

    if (bu_struct_parse( matparm, phong_parse, (char *)pp ) < 0 )  {
	bu_free( (char *)pp, "phong_specific" );
	return(-1);
    }

    if (pp->transmit > 0 )
	rp->reg_transmit = 1;
    return(1);
}

/*
 *			M I R R O R _ S E T U P
 */
HIDDEN int
mirror_setup(register struct region *rp, struct bu_vls *matparm, char **dpp, struct mfuncs *mfp, struct rt_i *rtip)
{
    register struct phong_specific *pp;

    BU_CK_VLS( matparm );
    BU_GETSTRUCT( pp, phong_specific );
    *dpp = (char *)pp;

    pp->magic = PL_MAGIC;
    pp->shine = 4;
    pp->wgt_specular = 0.6;
    pp->wgt_diffuse = 0.4;
    pp->transmit = 0.0;
    pp->reflect = 0.75;
    pp->refrac_index = 1.65;
    pp->extinction = 0.0;
    pp->mfp = mfp;

    if (bu_struct_parse( matparm, phong_parse, (char *)pp ) < 0 )  {
	bu_free( (char *)pp, "phong_specific" );
	return(-1);
    }

    if (pp->transmit > 0 )
	rp->reg_transmit = 1;
    return(1);
}

/*
 *			G L A S S _ S E T U P
 */
HIDDEN int
glass_setup(register struct region *rp, struct bu_vls *matparm, char **dpp, struct mfuncs *mfp, struct rt_i *rtip)
{
    register struct phong_specific *pp;

    BU_CK_VLS( matparm );
    BU_GETSTRUCT( pp, phong_specific );
    *dpp = (char *)pp;

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

    if (bu_struct_parse( matparm, phong_parse, (char *)pp ) < 0 )  {
	bu_free( (char *)pp, "phong_specific" );
	return(-1);
    }

    if (pp->transmit > 0 )
	rp->reg_transmit = 1;
    return(1);
}

/*
 *			P H O N G _ P R I N T
 */
HIDDEN void
phong_print(register struct region *rp, char *dp)
{
    bu_struct_print(rp->reg_name, phong_parse, (char *)dp);
}

/*
 *			P H O N G _ F R E E
 */
HIDDEN void
phong_free(char *cp)
{
    bu_free( cp, "phong_specific" );
}


/*
 *			P H O N G _ R E N D E R
 *
 Color pixel based on the energy of a point light source (Eps)
 plus some diffuse illumination (Epd) reflected from the point
 <x,y> :

 E = Epd + Eps		(1)

 The energy reflected from diffuse illumination is the product
 of the reflectance coefficient at point P (Rp) and the diffuse
 illumination (Id) :

 Epd = Rp * Id		(2)

 The energy reflected from the point light source is calculated
 by the sum of the diffuse reflectance (Rd) and the specular
 reflectance (Rs), multiplied by the intensity of the light
 source (Ips) :

 Eps = (Rd + Rs) * Ips	(3)

 The diffuse reflectance is calculated by the product of the
 reflectance coefficient (Rp) and the cosine of the angle of
 incidence (I) :

 Rd = Rp * cos(I)	(4)

 The specular reflectance is calculated by the product of the
 specular reflectance coeffient and (the cosine of the angle (S)
 raised to the nth power) :

 Rs = W(I) * cos(S)**n	(5)

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

 Er = Ra(m)*cos(Ia) + Rd(m)*cos(I1) + W(I1,m)*cos(s)^^n
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
phong_render(register struct application *ap, struct partition *pp, struct shadework *swp, char *dp)
{
    register struct light_specific *lp;
#ifndef RT_MULTISPECTRAL
    register	fastf_t	*intensity;
#endif
    register	fastf_t	refl;
    register	fastf_t	*to_light;
    register	int	i;
    register	fastf_t	cosine;
    vect_t			work,color;
    vect_t			reflected;
    point_t			pt;
    fastf_t			dist;

#ifdef RT_MULTISPECTRAL
    struct bn_tabdata	*ms_matcolor = BN_TABDATA_NULL;
#else
    point_t	matcolor;		/* Material color */
#endif
    struct phong_specific *ps =
	(struct phong_specific *)dp;

    if (ps->magic != PL_MAGIC )  bu_log("phong_render: bad magic\n");

    if (rdebug&RDEBUG_SHADE)
	bu_struct_print( "phong_render", phong_parse, (char *)ps );

    swp->sw_transmit = ps->transmit;
    swp->sw_reflect = ps->reflect;
    swp->sw_refrac_index = ps->refrac_index;
    swp->sw_extinction = ps->extinction;
#if SW_SET_TRANSMIT
    if (swp->sw_phong_set_vector & SW_SET_TRANSMIT) swp->sw_transmit = swp->sw_phong_transmit;
    if (swp->sw_phong_set_vector & SW_SET_REFLECT) swp->sw_reflect = swp->sw_phong_reflect;
    if (swp->sw_phong_set_vector & SW_SET_REFRAC_INDEX) swp->sw_refrac_index = swp->sw_phong_ri;
    if (swp->sw_phong_set_vector & SW_SET_EXTINCTION) swp->sw_extinction = swp->sw_phong_extinction;
#endif /* SW_SET_TRANSMIT */
    if (swp->sw_xmitonly ) {
	if (swp->sw_xmitonly > 1 )
	    return(1);	/* done -- wanted parameters only */
	if (swp->sw_reflect > 0 || swp->sw_transmit > 0 ) {
	    if (rdebug&RDEBUG_SHADE)
		bu_log("calling rr_render from phong, sw_xmitonly\n");
	    (void)rr_render( ap, pp, swp );
	}
	return(1);	/* done */
    }


#ifdef RT_MULTISPECTRAL
    ms_matcolor = bn_tabdata_dup( swp->msw_color );
#else
    VMOVE( matcolor, swp->sw_color );
#endif

    /* Photon Mapping */
#ifndef RT_MULTISPECTRAL
    color[0]= swp->sw_color[0];
    color[1]= swp->sw_color[1];
    color[2]= swp->sw_color[2];
#endif

#ifndef RT_MULTISPECTRAL
    if (!PM_Visualize)
#endif
	{
	    /* Diffuse reflectance from "Ambient" light source (at eye) */
	    if ((cosine = -VDOT( swp->sw_hit.hit_normal, ap->a_ray.r_dir )) > 0.0 )  {
		if (cosine > 1.00001 )  {
		    bu_log("cosAmb=1+%g %s surfno=%d (x%d,y%d,lvl%d)\n",
			   cosine-1,
			   pp->pt_inseg->seg_stp->st_dp->d_namep,
			   swp->sw_hit.hit_surfno,
			   ap->a_x, ap->a_y, ap->a_level);
		    VPRINT(" normal", swp->sw_hit.hit_normal);
		    VPRINT(" r_dir ", ap->a_ray.r_dir);
		    cosine = 1;
		}
#if SW_SET_TRANSMIT
		if (swp->sw_phong_set_vector & SW_SET_AMBIENT) {
		    cosine *= swp->sw_phong_ambient;
		} else {
		    cosine *= AmbientIntensity;
		}
#else
		cosine *= AmbientIntensity;
#endif
#ifdef RT_MULTISPECTRAL
		bn_tabdata_scale( swp->msw_color, ms_matcolor, cosine );
#else
		VSCALE( swp->sw_color, matcolor, cosine );
#endif
	    } else {
#ifdef RT_MULTISPECTRAL
		bn_tabdata_constval( swp->msw_color, 0.0 );
#else
		VSETALL( swp->sw_color, 0 );
#endif
	    }

	    /* Emission.  0..1 is normal range, -1..0 sucks light out, like OpenGL */
#ifdef RT_MULTISPECTRAL
	    {
		float emission[3];
		struct bn_tabdata	*ms_emission = BN_TABDATA_NULL;
		VMOVE(emission,ps->emission);
#if SW_SET_TRANSMIT
		if (swp->sw_phong_set_vector & SW_SET_EMISSION) {
		    VSETALL(emission, swp->sw_phong_emission);
		}
#endif
		/* XXX Really should get a curve at prep, not expand RGB samples */
		BN_GET_TABDATA( ms_emission, spectrum );
		rt_spect_reflectance_rgb( ms_emission, emission );
		bn_tabdata_add( swp->msw_color, swp->msw_color, ms_emission );
		bn_tabdata_free( ms_emission );
	    }
#else
#if SW_SET_TRANSMIT
	    if (swp->sw_phong_set_vector & SW_SET_EMISSION) {
		vect_t tmp;
		VSETALL(tmp,swp->sw_phong_emission);
		VADD2( swp->sw_color, swp->sw_color, tmp);
	    } else {
		VADD2( swp->sw_color, swp->sw_color, ps->emission );
	    }
#else
	    VADD2( swp->sw_color, swp->sw_color, ps->emission );
#endif /* SW_SET_TRANSMIT */
#endif

	    /* With the advent of procedural shaders, the caller can no longer
	     * provide us reliable light visibility information.  The hit point
	     * may have been changed by another shader in a stack.  There is no
	     * way that anyone else can tell us whether lights are visible.
	     */
	    light_obs(ap, swp, ps->mfp->mf_inputs);

	    /* Consider effects of each light source */
	    for( i=ap->a_rt_i->rti_nlights-1; i>=0; i-- )  {

		if ((lp = (struct light_specific *)swp->sw_visible[i]) == LIGHT_NULL )
		    continue;

		if( rdebug & RDEBUG_LIGHT )  {
		    bu_log("phong_render light=%s lightfract=%g\n",
			   lp->lt_name, swp->sw_lightfract[i] );
		}

		/* Light is not shadowed -- add this contribution */
#ifndef RT_MULTISPECTRAL
		intensity = swp->sw_intensity+3*i;
#endif
		to_light = swp->sw_tolight+3*i;

		/* Diffuse reflectance from this light source. */
		if ((cosine=VDOT(swp->sw_hit.hit_normal, to_light)) > 0.0 )  {
		    if (cosine > 1.00001 )  {
			bu_log("cosI=1+%g (x%d,y%d,lvl%d)\n", cosine-1,
			       ap->a_x, ap->a_y, ap->a_level);
			cosine = 1;
		    }
		    /* Get Obj Hit Point For Attenuation */
#ifndef RT_MULTISPECTRAL
		    if (pp && PM_Activated) {
			VJOIN1(pt, ap->a_ray.r_pt, pp->pt_inhit->hit_dist, ap->a_ray.r_dir)
			    dist= sqrt((pt[0]-lp->lt_pos[0])*(pt[0]-lp->lt_pos[0]) + (pt[1]-lp->lt_pos[1])*(pt[1]-lp->lt_pos[1]) + (pt[2]-lp->lt_pos[2])*(pt[2]-lp->lt_pos[2]))/1000.0;
			dist= (1.0/(0.1 + 1.0*dist + 0.01*dist*dist));
			refl= dist * ps->wgt_diffuse * cosine * swp->sw_lightfract[i] * lp->lt_intensity;
			/*				bu_log("pt: [%.3f][%.3f,%.3f,%.3f]\n",dist,pt[0],pt[1],pt[2]);*/
		    } else
#endif
			{
			    refl= ps->wgt_diffuse * swp->sw_lightfract[i] * cosine * lp->lt_fraction;
			}

#ifdef RT_MULTISPECTRAL
		    bn_tabdata_incr_mul3_scale( swp->msw_color,
						lp->lt_spectrum,
						swp->msw_intensity[i],
						ms_matcolor,
						refl );
#else
		    VELMUL3( work, matcolor, lp->lt_color, intensity );
		    VJOIN1( swp->sw_color, swp->sw_color, refl, work );
#endif
		}

		/* Calculate specular reflectance.
		 *	Reflected ray = (2 * cos(i) * Normal) - Incident ray.
		 * 	Cos(s) = Reflected ray DOT Incident ray.
		 */
		cosine *= 2;
		VSCALE( work, swp->sw_hit.hit_normal, cosine );
		VSUB2( reflected, work, to_light );
		if ((cosine = -VDOT( reflected, ap->a_ray.r_dir )) > 0 )  {
		    if (cosine > 1.00001 )  {
			bu_log("cosS=1+%g (x%d,y%d,lvl%d)\n", cosine-1,
			       ap->a_x, ap->a_y, ap->a_level);
			cosine = 1;
		    }
		    refl = ps->wgt_specular * swp->sw_lightfract[i] *
			lp->lt_fraction *
#ifdef PHAST_PHONG
			/* It is unnecessary to compute the actual
			 * exponential here since phong is just a
			 * gross hack.  We approximate re:
			 *  Graphics Gems IV "A Fast Alternative to
			 *  Phong's Specular Model" Pg 385
			 */
			cosine /
			(ps->shine - ps->shine*cosine + cosine);
#else
		    phg_ipow(cosine, ps->shine);
#endif /* PHAST_PHONG */
#ifdef RT_MULTISPECTRAL
		    bn_tabdata_incr_mul2_scale( swp->msw_color,
						lp->lt_spectrum,
						swp->msw_intensity[i],
						refl );
#else
		    VELMUL( work, lp->lt_color, intensity );
		    VJOIN1( swp->sw_color, swp->sw_color, refl, work );
#endif
		}
	    }

#ifndef RT_MULTISPECTRAL
	    if (PM_Activated) {
		IrradianceEstimate(ap, work, swp->sw_hit.hit_point, swp->sw_hit.hit_normal, 100, 100);
		VELMUL(work, work, color);
		VADD2(swp->sw_color, work, swp->sw_color);
		if (swp->sw_color[0] > 1.0) swp->sw_color[0]= 1.0;
		if (swp->sw_color[1] > 1.0) swp->sw_color[1]= 1.0;
		if (swp->sw_color[2] > 1.0) swp->sw_color[2]= 1.0;
	    }

	} else {

	    if (PM_Activated) {
		/*  IrradianceEstimate(work, swp->sw_hit.hit_point, swp->sw_hit.hit_normal, 100, 100);
		    VELMUL(swp->sw_color, work, color);*/
		IrradianceEstimate(ap, swp->sw_color, swp->sw_hit.hit_point, swp->sw_hit.hit_normal, 100, 100);
		if (swp->sw_color[0] > 1.0) swp->sw_color[0]= 1.0;
		if (swp->sw_color[1] > 1.0) swp->sw_color[1]= 1.0;
		if (swp->sw_color[2] > 1.0) swp->sw_color[2]= 1.0;
	    }
#endif
	}


    if (swp->sw_reflect > 0 || swp->sw_transmit > 0 )
	(void)rr_render( ap, pp, swp );

#ifdef RT_MULTISPECTRAL
    bn_tabdata_free(ms_matcolor);
#endif
    return(1);
}


#ifndef PHAST_PHONG
/*
 *  			I P O W
 *
 *  Raise a floating point number to an integer power
 */
double
phg_ipow( d, cnt )
     double d;
     register int cnt;
{
    FAST fastf_t input, result;

    if ((input=d) < 1e-8 )  return(0.0);
    if (cnt < 0 || cnt > 200 )  {
	bu_log("phg_ipow(%g,%d) bad\n", d, cnt);
	return(d);
    }
    result = 1;
    while( cnt-- > 0 )
	result *= input;
    return( result );
}
#endif

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
