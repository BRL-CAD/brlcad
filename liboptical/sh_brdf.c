/*
 *			B R D F
 *
 *  Simple Isotropic Gaussian model with just one parameter (RMS slope).
 *
 *  Notes -
 *	The normals on all surfaces point OUT of the solid.
 *	The incoming light rays point IN.
 *
 *  Authors -
 *	John R. Anderson
 *  
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *  
 *  Distribution Notice -
 *	Re-distribution of this software is restricted, as described in
 *	your "Statement of Terms and Conditions for the Release of
 *	The BRL-CAD Pacakge" agreement.
 *
 *  Copyright Notice -
 *	This software is Copyright (C) 1996 by the United States Army
 *	in all countries except the USA.  All rights reserved.
 *
 *	Based on the simple Isotropic Gaussian Model presented by Gregory Ward
 *	in "Measuring and Modeling Anisotropic Reflection" (Which also references
 *	earlier work by Beckmann, Torrance, and Cook).
 */
#ifndef lint
static char RCSbrdf[] = "@(#)$Header$ (BRL)";
#endif

#include "conf.h"

#include <stdio.h>
#include <math.h>
#include "machine.h"
#include "vmath.h"
#include "mater.h"
#include "raytrace.h"
#include "./rdebug.h"
#include "./material.h"
#include "./light.h"

/* from view.c */
extern double AmbientIntensity;

/* Local information */
struct brdf_specific {
	int	magic;
	double	specular_refl;	/* specular reflectance */
	double	diffuse_refl;	/* diffuse reflectnace */
	double	rms_slope;	/* Standard deviation (RMS) of surface slope (roughness) */
	double	rms_sq;		/* square of above */
	double	denom;		/* denominator for specular term */
	double	transmit;	/* Moss "transparency" */
	double	reflect;	/* Moss "transmission" */
	double	refrac_index;
	double	extinction;
};
#define BRDF_MAGIC	0xbeef00d
#define BRDF_NULL	((struct brdf_specific *)0)
#define BRDF_O(m)	offsetof(struct brdf_specific, m)

struct bu_structparse brdf_parse[] = {
	{"%f",	1, "specular",		BRDF_O(specular_refl),	FUNC_NULL },
	{"%f",	1, "sp",		BRDF_O(specular_refl),	FUNC_NULL },
	{"%f",	1, "diffuse",		BRDF_O(diffuse_refl),	FUNC_NULL },
	{"%f",	1, "di",		BRDF_O(diffuse_refl),	FUNC_NULL },
	{"%f",	1, "rough",		BRDF_O(rms_slope),	FUNC_NULL },
	{"%f",	1, "rms",		BRDF_O(rms_slope),	FUNC_NULL },
	{"%f",	1, "transmit",		BRDF_O(transmit),	FUNC_NULL },
	{"%f",	1, "tr",		BRDF_O(transmit),	FUNC_NULL },
	{"%f",	1, "reflect",		BRDF_O(reflect),	FUNC_NULL },
	{"%f",	1, "re",		BRDF_O(reflect),	FUNC_NULL },
	{"%f",	1, "ri",		BRDF_O(refrac_index),	FUNC_NULL },
	{"%f",	1, "extinction_per_meter", BRDF_O(extinction),	FUNC_NULL },
	{"%f",	1, "extinction",	BRDF_O(extinction),	FUNC_NULL },
	{"%f",	1, "ex",		BRDF_O(extinction),	FUNC_NULL },
	{"",	0, (char *)0,		0,			FUNC_NULL }
};

HIDDEN int brdf_setup();
HIDDEN int brdf_render();
HIDDEN void	brdf_print();
HIDDEN void	brdf_free();

struct mfuncs brdf_mfuncs[] = {
	{"brdf",	0,		0,		MFI_NORMAL|MFI_LIGHT,	0,
	brdf_setup,	brdf_render,	brdf_print,	brdf_free },

	{(char *)0,	0,		0,		0,	0,
	0,		0,		0,		0 }
};

#define RI_AIR		1.0    /* Refractive index of air.		*/

/*
 *			B R D F _ S E T U P
 */
HIDDEN int
brdf_setup( rp, matparm, dpp, mpf, rtip )
register struct region *rp;
struct rt_vls	*matparm;
char	**dpp;
struct mfuncs	*mfp;
struct rt_i	*rtip;
{
	register struct brdf_specific *pp;

	RT_VLS_CHECK( matparm );
	GETSTRUCT( pp, brdf_specific );
	*dpp = (char *)pp;

	pp->magic = BRDF_MAGIC;
	pp->specular_refl = 0.7;
	pp->diffuse_refl = 0.3;
	pp->transmit = 0.0;
	pp->reflect = 0.0;
	pp->refrac_index = RI_AIR;
	pp->extinction = 0.0;
	pp->rms_slope = 0.05;

	if( bu_struct_parse( matparm, brdf_parse, (char *)pp ) < 0 )  {
		rt_free( (char *)pp, "brdf_specific" );
		return(-1);
	}

	pp->rms_sq = pp->rms_slope * pp->rms_slope;
	pp->denom = 4.0 * rt_pi * pp->rms_sq;

	return(1);
}
/*
 *			B R D F _ P R I N T
 */
HIDDEN void
brdf_print( rp, dp )
register struct region *rp;
char	*dp;
{
	bu_struct_print(rp->reg_name, brdf_parse, (char *)dp);
}

/*
 *			B R D F _ F R E E
 */
HIDDEN void
brdf_free( cp )
char *cp;
{
	rt_free( cp, "brdf_specific" );
}


/*
 *			B R D F _ R E N D E R
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
	incidence (I) and normalized by PI :

				Rd = Rp * cos(I) / PI	(4)

	The specular reflectance is calculated by the product of the
	specular reflectance coeffient and a term dependent on the
	surface roughness :

				Rs = W(I,O) * R(I,O,r)	(5)

	Where,
		I is the angle of incidence.
		O is the angle to the observer.
		r is the standard deviation (RMS) of the surface slope.
		W returns the specular reflection coefficient as a function
	of the angle of incidence, and the viewer angle.
		R is a surface roughness term.

 */
HIDDEN int
brdf_render( ap, pp, swp, dp )
register struct application *ap;
struct partition	*pp;
struct shadework	*swp;
char	*dp;
{
	register struct light_specific *lp;
	register fastf_t *intensity, *to_light;
	register int	i;
	register fastf_t cosi,cosr;
	register fastf_t refl;
	vect_t h_dir;
	vect_t to_eye;
	vect_t	work;
	vect_t	reflected;
	vect_t	cprod;			/* color product */
	point_t	matcolor;		/* Material color */
	struct brdf_specific *ps =
		(struct brdf_specific *)dp;

	if( ps->magic != BRDF_MAGIC )  rt_log("brdf_render: bad magic\n");

	if( rdebug&RDEBUG_SHADE)
		bu_struct_print( "brdf_render", brdf_parse, (char *)ps );

	swp->sw_transmit = ps->transmit;
	swp->sw_reflect = ps->reflect;
	swp->sw_refrac_index = ps->refrac_index;
	swp->sw_extinction = ps->extinction;
	if( swp->sw_xmitonly ) {
		if( swp->sw_reflect > 0 || swp->sw_transmit > 0 )
			(void)rr_render( ap, pp, swp );
		return(1);	/* done */
	}

	VMOVE( matcolor, swp->sw_color );

	/* Diffuse reflectance from "Ambient" light source (at eye) */
	if( (cosr = -VDOT( swp->sw_hit.hit_normal, ap->a_ray.r_dir )) > 0.0 )  {
		if( cosr > 1.00001 )  {
			rt_log("cosAmb=1+%g (x%d,y%d,lvl%d)\n", cosr-1,
				ap->a_x, ap->a_y, ap->a_level);
			cosr = 1;
		}
		refl = cosr * AmbientIntensity;
		VSCALE( swp->sw_color, matcolor, refl );
	} else {
		VSETALL( swp->sw_color, 0 );
	}

	VREVERSE( to_eye, ap->a_ray.r_dir );

	/* Consider effects of each light source */
	for( i=ap->a_rt_i->rti_nlights-1; i>=0; i-- )  {
		fastf_t cos_tmp;
		fastf_t tan_sq;
		double exponent;

		if( (lp = (struct light_specific *)swp->sw_visible[i]) == LIGHT_NULL )
			continue;
	
		/* Light is not shadowed -- add this contribution */
		intensity = swp->sw_intensity+3*i;
		to_light = swp->sw_tolight+3*i;

		if( (cosi = VDOT( swp->sw_hit.hit_normal, to_light )) > 0.0 )  {
			if( cosi > 1.00001 )  {
				rt_log("cosI=1+%g (x%d,y%d,lvl%d)\n", cosi-1,
					ap->a_x, ap->a_y, ap->a_level);
				cosi = 1;
			}

			/* Diffuse reflectance from this light source. */
			refl = cosi * lp->lt_fraction * ps->diffuse_refl;
			VELMUL( work, lp->lt_color,
				intensity );
			VELMUL( cprod, matcolor, work );
			VJOIN1( swp->sw_color, swp->sw_color,
				refl, cprod );

			/* Calculate specular reflectance. */
			if( NEAR_ZERO( ps->rms_sq, SMALL_FASTF ) )
				continue;
			VADD2( h_dir, to_eye, to_light )
			VUNITIZE( h_dir );
			cos_tmp = VDOT( h_dir, swp->sw_hit.hit_normal );
			if( cos_tmp <= 0.0 )
				continue;
			cos_tmp *= cos_tmp;
			if( NEAR_ZERO( cos_tmp, SMALL_FASTF ) )
				continue;

			tan_sq = (1.0-cos_tmp)/cos_tmp;
			exponent = (-tan_sq/ps->rms_sq );
			refl = ps->specular_refl * lp->lt_fraction * exp( exponent ) /
				sqrt( cosi * cosr ) / ps->denom;
			if( refl > 1.0 )
				refl = 1.0;

			VELMUL( work, lp->lt_color, intensity );
			VJOIN1( swp->sw_color, swp->sw_color, refl, work );

		}
	}

	if( swp->sw_reflect > 0 || swp->sw_transmit > 0 )
		(void)rr_render( ap, pp, swp );
	return(1);
}

