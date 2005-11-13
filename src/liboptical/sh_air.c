/*                        S H _ A I R . C
 * BRL-CAD
 *
 * Copyright (C) 2004-2005 United States Government as represented by
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
/** @file sh_air.c
 *
 */
#include "common.h"



#include <stdio.h>
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#include <math.h>
#include "machine.h"
#include "vmath.h"
#include "raytrace.h"
#include "rtprivate.h"

extern int rr_render(struct application	*ap,
		     struct partition	*pp,
		     struct shadework   *swp);

#define AIR_MAGIC 0x41697200	/* "Air" */
struct air_specific {
	long	magic;
	double	d_p_mm;	/* density per unit millimeter (specified in m)*/
	double	scale;	/* only used in emist */
	double	delta;	/* only used in emist */
	char	*name;	/* name of "ground" object for emist_terrain_render */
};
#define CK_AIR_SP(_p) BU_CKMAG(_p, AIR_MAGIC, "air_specific")

static struct air_specific air_defaults = {
	AIR_MAGIC,
	.1,		/* d_p_mm */
	.01,		/* scale */
	0.0,		/* delta */
	};

#define SHDR_NULL	((struct air_specific *)0)
#define SHDR_O(m)	offsetof(struct air_specific, m)
#define SHDR_AO(m)	bu_offsetofarray(struct air_specific, m)

static void dpm_hook(register const struct bu_structparse *sdp, register const char *name, char *base, const char *value);

struct bu_structparse air_parse[] = {
	{"%f",  1, "dpm",		SHDR_O(d_p_mm),		dpm_hook },
	{"%f",  1, "scale",		SHDR_O(scale),		BU_STRUCTPARSE_FUNC_NULL },
	{"%f",  1, "s",			SHDR_O(scale),		BU_STRUCTPARSE_FUNC_NULL },
	{"%f",  1, "delta",		SHDR_O(delta),		bu_mm_cvt },
	{"%f",  1, "d",			SHDR_O(delta),		bu_mm_cvt },
	{"",	0, (char *)0,		0,			BU_STRUCTPARSE_FUNC_NULL }
};

HIDDEN int	air_setup(register struct region *rp, struct bu_vls *matparm, char **dpp, struct mfuncs *mfp, struct rt_i *rtip), airtest_render(struct application *ap, struct partition *pp, struct shadework *swp, char *dp), air_render(struct application *ap, struct partition *pp, struct shadework *swp, char *dp), emist_render(struct application *ap, struct partition *pp, struct shadework *swp, char *dp), tmist_render(struct application *ap, struct partition *pp, struct shadework *swp, char *dp);
HIDDEN void	air_print(register struct region *rp, char *dp), air_free(char *cp);

struct mfuncs air_mfuncs[] = {
	{MF_MAGIC,	"airtest",	0,		MFI_HIT, MFF_PROC,
	air_setup,	airtest_render,	air_print,	air_free },

	{MF_MAGIC,	"air",		0,		MFI_HIT, MFF_PROC,
	air_setup,	air_render,	air_print,	air_free },

	{MF_MAGIC,	"fog",		0,		MFI_HIT, MFF_PROC,
	air_setup,	air_render,	air_print,	air_free },

	{MF_MAGIC,	"emist",	0,		MFI_HIT, MFF_PROC,
	air_setup,	emist_render,	air_print,	air_free },

	{MF_MAGIC,	"tmist",	0,		MFI_HIT, MFF_PROC,
	air_setup,	tmist_render,	air_print,	air_free },

	{0,		(char *)0,	0,		0,	0,
	0,		0,		0,		0 }
};
static void
dpm_hook(register const struct bu_structparse *sdp, register const char *name, char *base, const char *value)
                                    	     	/* structure description */
                   			      	/* struct member name */
    					      	/* begining of structure */
          				       	/* string containing value */
{
#define meters_to_millimeters 0.001
	struct air_specific *air_sp = (struct air_specific *)base;

	air_sp->d_p_mm *= meters_to_millimeters;
}
/*	A I R _ S E T U P
 *
 *	This routine is called (at prep time)
 *	once for each region which uses this shader.
 *	Any shader-specific initialization should be done here.
 */
HIDDEN int
air_setup(register struct region *rp, struct bu_vls *matparm, char **dpp, struct mfuncs *mfp, struct rt_i *rtip)


    			      	/* pointer to reg_udata in *rp */

           		      	/* New since 4.4 release */
{
	register struct air_specific	*air_sp;

	if (rdebug&RDEBUG_SHADE) bu_log("air_setup\n");

	RT_CHECK_RTI(rtip);
	BU_CK_VLS( matparm );
	RT_CK_REGION(rp);
	BU_GETSTRUCT( air_sp, air_specific );
	*dpp = (char *)air_sp;

	memcpy(air_sp, &air_defaults, sizeof(struct air_specific) );

	if (rp->reg_aircode == 0) {
		bu_log("WARNING(%s): air shader '%s' applied to non-air region.\n%s\n",
			rp->reg_name,
			mfp->mf_name,
			"  Set air flag with \"edcodes\" in mged");
		bu_bomb("");
	}

	if (rdebug&RDEBUG_SHADE) bu_log("\"%s\"\n", bu_vls_addr(matparm) );
	if (bu_struct_parse( matparm, air_parse, (char *)air_sp ) < 0 )
		return(-1);

	if (rdebug&RDEBUG_SHADE) air_print(rp, (char *)air_sp);

	return(1);
}

/*
 *	A I R _ P R I N T
 */
HIDDEN void
air_print(register struct region *rp, char *dp)
{
	bu_struct_print( rp->reg_name, air_parse, (char *)dp );
}

/*
 *	A I R _ F R E E
 */
HIDDEN void
air_free(char *cp)
{
	if (rdebug&RDEBUG_SHADE)
		bu_log("air_free(%s:%d)\n", __FILE__, __LINE__);
	bu_free( cp, "air_specific" );
}

/*
 *	A I R T E S T _ R E N D E R
 *
 *	This is called (from viewshade() in shade.c)
 *	once for each hit point to be shaded.
 */
int
airtest_render(struct application *ap, struct partition *pp, struct shadework *swp, char *dp)
{
	register struct air_specific *air_sp =
		(struct air_specific *)dp;

	RT_AP_CHECK(ap);
	RT_CHECK_PT(pp);
	CK_AIR_SP(air_sp);

	if (rdebug&RDEBUG_SHADE) {
		bu_struct_print( "air_specific", air_parse, (char *)air_sp );

		bu_log("air in(%g) out%g)\n",
			pp->pt_inhit->hit_dist,
			pp->pt_outhit->hit_dist);
	}

	return(1);
}
/*
 *	A I R _ R E N D E R
 *
 *	This is called (from viewshade() in shade.c)
 *	once for each hit point to be shaded.
 *
 *
 *	This implements Beer's law homogeneous Fog/haze
 *
 *	Tau = optical path depth = density_per_unit_distance * distance
 *
 *	transmission = e^(-Tau)
 */
int
air_render(struct application *ap, struct partition *pp, struct shadework *swp, char *dp)
{
	register struct air_specific *air_sp =
		(struct air_specific *)dp;
	double tau;
	double dist;

	RT_AP_CHECK(ap);
	RT_CHECK_PT(pp);
	CK_AIR_SP(air_sp);

	if (rdebug&RDEBUG_SHADE) {
		bu_struct_print( "air_specific", air_parse, (char *)air_sp );
		bu_log("air in(%g) out(%g) r_pt(%g %g %g)\n",
			pp->pt_inhit->hit_dist,
			pp->pt_outhit->hit_dist,
			V3ARGS(ap->a_ray.r_pt));
	}

	/* Beer's Law Homogeneous Fog */

	/* get the path length right */
	if (pp->pt_inhit->hit_dist < 0.0)
		dist = pp->pt_outhit->hit_dist;
	else
		dist = (pp->pt_outhit->hit_dist - pp->pt_inhit->hit_dist);

	/* tau = optical path depth = density per mm * distance in mm */
	tau = air_sp->d_p_mm * dist;

	/* transmission = e^(-tau) */
	swp->sw_transmit = exp(-tau);

	if ( swp->sw_transmit > 1.0) swp->sw_transmit = 1.0;
	else if ( swp->sw_transmit < 0.0 ) swp->sw_transmit = 0.0;

	/* extinction = 1. - transmission.  Extinguished part replaced by
	 * the "color of the air".
	 */

	if (swp->sw_reflect > 0 || swp->sw_transmit > 0 )
		(void)rr_render( ap, pp, swp );

	if (rdebug&RDEBUG_SHADE)
		bu_log("air o dist:%gmm tau:%g transmit:%g\n",
			dist, tau, swp->sw_transmit);

	return(1);
}

int
tmist_hit(register struct application *ap, struct partition *PartHeadp, struct seg *segHeadp)
{
	/* go looking for the object named in
	 * ((struct air_specific *)ap->a_uptr)->name
	 * in the partition list,
	 * set ap->a_dist to distance to that object
	 */
	return 0;
}
int
tmist_miss(register struct application *ap)
{
	/* we missed?!  This is bogus!
	 * but set ap->a_dist to something big
	 */
	return 0;
}

/*
 *	T M I S T _ R E N D E R
 *
 * Use height above named terrain object
 *
 *
 *	This is called (from viewshade() in shade.c)
 *	once for each hit point to be shaded.
 */
int
tmist_render(struct application *ap, struct partition *pp, struct shadework *swp, char *dp)
{
	register struct air_specific *air_sp =
		(struct air_specific *)dp;
	point_t in_pt, out_pt;
	vect_t vapor_path;
	fastf_t dist;
	fastf_t step_dist;
	fastf_t tau;
	fastf_t dt;
	struct application my_ap;
	long meters;

	RT_AP_CHECK(ap);
	RT_CHECK_PT(pp);
	CK_AIR_SP(air_sp);

	/* Get entry point */
	if (pp->pt_inhit->hit_dist < 0.0) {
		VMOVE(in_pt, ap->a_ray.r_pt);
	} else {
		VJOIN1(in_pt, ap->a_ray.r_pt,
			pp->pt_inhit->hit_dist, ap->a_ray.r_dir);
	}

	/* Get exit point */
	VJOIN1(out_pt,
		ap->a_ray.r_pt, pp->pt_outhit->hit_dist, ap->a_ray.r_dir);

	VSUB2(vapor_path, out_pt, in_pt);

	dist = MAGNITUDE(vapor_path);
	meters = (long)dist;
	meters = meters >> 3;

	if (meters < 1) step_dist = dist;
	else step_dist = dist / (fastf_t)meters;

	for (dt=0.0 ; dt <= dist ; dt += step_dist) {
		memcpy((char *)&my_ap, (char *)ap, sizeof(struct application));
		VJOIN1(my_ap.a_ray.r_pt, in_pt, dt, my_ap.a_ray.r_dir);
		VSET(my_ap.a_ray.r_dir, 0.0, 0.0, -1.0);
		my_ap.a_hit = tmist_hit;
		my_ap.a_miss = tmist_miss;
		my_ap.a_onehit = 0;
		my_ap.a_uptr = (genptr_t)air_sp;
		rt_shootray( &my_ap );

		/* XXX check my_ap.a_dist for distance to ground */

		/* XXX compute optical density at this altitude */

		/* XXX integrate in the effects of this meter of air */

	}
	tau = 42;


	swp->sw_transmit = exp(-tau);

	if (swp->sw_transmit > 1.0) swp->sw_transmit = 1.0;
	else if (swp->sw_transmit < 0.0) swp->sw_transmit = 0.0;

	if (rdebug&RDEBUG_SHADE)
		bu_log("tmist transmit = %g\n", swp->sw_transmit);

	return(1);
}
/*
 *	E M I S T _ R E N D E R
 *
 *
 *
 *
 * te = dist from pt to end of ray (out hit point)
 * Zo = elevation at ray start
 * Ze = elevation at ray end
 * Zd = Z component of normalized ray vector
 * d_p_mm = overall fog density
 * B = density falloff with altitude
 *
 *
 *	delta = height at which fog starts
 *	scale = stretches exponential decay zone
 *	d_p_mm = maximum density @ ground
 *
 *	This is called (from viewshade() in shade.c)
 *	once for each hit point to be shaded.
 */
int
emist_render(struct application *ap, struct partition *pp, struct shadework *swp, char *dp)
{
	register struct air_specific *air_sp =
		(struct air_specific *)dp;
	point_t in_pt, out_pt;
	double tau;
	double Zo, Ze, Zd, te;

	RT_AP_CHECK(ap);
	RT_CHECK_PT(pp);
	CK_AIR_SP(air_sp);

	/* compute hit point & length of ray */
	if (pp->pt_inhit->hit_dist < 0.0) {
		VMOVE(in_pt, ap->a_ray.r_pt);
		te = pp->pt_outhit->hit_dist;
	} else {
		VJOIN1(in_pt, ap->a_ray.r_pt,
			pp->pt_inhit->hit_dist, ap->a_ray.r_dir);
		te = pp->pt_outhit->hit_dist - pp->pt_inhit->hit_dist;
	}

	/* get exit point */
	VJOIN1(out_pt,
		ap->a_ray.r_pt, pp->pt_outhit->hit_dist, ap->a_ray.r_dir);

	Zo = (air_sp->delta + in_pt[Z]) * air_sp->scale;
	Ze = (air_sp->delta + out_pt[Z]) * air_sp->scale;
	Zd = ap->a_ray.r_dir[Z];

	if ( NEAR_ZERO( Zd, SQRT_SMALL_FASTF ) )
		tau = air_sp->d_p_mm * te * exp( -Zo);
	else
		tau = (( air_sp->d_p_mm * te) /  Zd) * ( exp(-Zo) - exp(-Ze) );

/*	XXX future
	tau *= bn_noise_fbm(pt);
*/

	swp->sw_transmit = exp(-tau);

	if (swp->sw_transmit > 1.0) swp->sw_transmit = 1.0;
	else if (swp->sw_transmit < 0.0) swp->sw_transmit = 0.0;

	if (rdebug&RDEBUG_SHADE)
		bu_log("emist transmit = %g\n", swp->sw_transmit);

	return(1);
}
/*
 *	F B M _ E M I S T _ R E N D E R
 *
 *
 *
 *
 * te = dist from pt to end of ray (out hit point)
 * Zo = elevation at ray start
 * Ze = elevation at ray end
 * Zd = Z component of normalized ray vector
 * d_p_mm = overall fog density
 * B = density falloff with altitude
 *
 *
 *	delta = height at which fog starts
 *	scale = stretches exponential decay zone
 *	d_p_mm = maximum density @ ground
 *
 *	This is called (from viewshade() in shade.c)
 *	once for each hit point to be shaded.
 */
int
emist_fbm_render(struct application *ap, struct partition *pp, struct shadework *swp, char *dp)
{
#ifndef NO_MAGIC_CHECKING
	register struct air_specific *air_sp =
	   (struct air_specific *)dp;
#endif
	point_t in_pt, out_pt;
	vect_t dist_v;
	double dist, delta;

#ifndef NO_MAGIC_CHECKING
	RT_AP_CHECK(ap);
	RT_CHECK_PT(pp);
	CK_AIR_SP(air_sp);
#endif

	/* compute hit point & length of ray */
	if (pp->pt_inhit->hit_dist < 0.0) {
		VMOVE(in_pt, ap->a_ray.r_pt);
	} else {
		VJOIN1(in_pt, ap->a_ray.r_pt,
			pp->pt_inhit->hit_dist, ap->a_ray.r_dir);
	}

	/* get exit point */
	VJOIN1(out_pt,
		ap->a_ray.r_pt, pp->pt_outhit->hit_dist, ap->a_ray.r_dir);


	/* we march along the ray, evaluating the atmospheric function
	 * at each step.
	 */

	VSUB2(dist_v, out_pt, in_pt);
	dist = MAGNITUDE(dist_v);

	for (delta=0 ; delta < dist ; delta += 1.0 ) {
		/* compute the current point in space */

		/* Shoot a ray down the -Z axis to find our current height
		 * above the local terrain.
		 */

	}



	return(1);
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
