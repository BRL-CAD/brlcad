/*                     S H _ S C L O U D . C
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
/** @file sh_scloud.c
 *
 *	A 3D "solid" cloud shader
 *
 *  Author -
 *	Lee A. Butler
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
#ifdef HAVE_STRING_H
#  include <string.h>
#else
#  include <strings.h>
#endif
#include <math.h>

#include "machine.h"
#include "vmath.h"
#include "raytrace.h"
#include "rtprivate.h"
#include "light.h"

extern int rr_render(struct application	*ap,
		     struct partition	*pp,
		     struct shadework   *swp);
#if !defined(M_PI)
#define M_PI            3.14159265358979323846
#endif

#define FLOOR(x)	(  (int)(x) - (  (x) < 0 && (x) != (int)(x)  )  )
#define CEIL(x)		(  (int)(x) + (  (x) > 0 && (x) != (int)(x)  )  )

struct scloud_specific {
	double	lacunarity;
	double	h_val;
	double	octaves;
	double	scale;	/* scale coordinate space */
	point_t	vscale;
	vect_t	delta;	/* xlatd in noise space (where interesting noise is)*/
	double	max_d_p_mm;	/* maximum density per millimeter */
	double	min_d_p_mm;	/* background density per millimeter */
	mat_t	mtos;		/* model to shader */
	mat_t	stom;		/* shader to model */
};

static struct scloud_specific scloud_defaults = {
	2.1753974,	/* lacunarity */
	1.0,		/* h_val */
	4.0,		/* octaves */
	1.0,		/* scale */
	{ 1.0, 1.0, 1.0 },	/* vscale */
	{ 1000.0, 1200.0, 2100.0 },	/* delta */
	0.01,			/* max_d_p_mm */
	0.0
	};

#define SHDR_NULL	((struct scloud_specific *)0)
#define SHDR_O(m)	bu_offsetof(struct scloud_specific, m)
#define SHDR_AO(m)	bu_offsetofarray(struct scloud_specific, m)

struct bu_structparse scloud_pr[] = {
	{"%f",	1, "lacunarity",	SHDR_O(lacunarity),	BU_STRUCTPARSE_FUNC_NULL },
	{"%f",	1, "H", 		SHDR_O(h_val),		BU_STRUCTPARSE_FUNC_NULL },
	{"%f",	1, "octaves", 		SHDR_O(octaves),	BU_STRUCTPARSE_FUNC_NULL },
	{"%f",  1, "scale",		SHDR_O(scale),		BU_STRUCTPARSE_FUNC_NULL },
	{"%f",  3, "vscale",		SHDR_AO(vscale),	BU_STRUCTPARSE_FUNC_NULL },
	{"%f",  3, "delta",		SHDR_AO(delta),		BU_STRUCTPARSE_FUNC_NULL },
	{"%f",	1, "Max", 		SHDR_O(max_d_p_mm),	BU_STRUCTPARSE_FUNC_NULL },
	{"%f",	1, "min", 		SHDR_O(min_d_p_mm),	BU_STRUCTPARSE_FUNC_NULL },
	{"",	0, (char *)0,		0,			BU_STRUCTPARSE_FUNC_NULL }
};
struct bu_structparse scloud_parse[] = {
	{"%f",	1, "lacunarity",	SHDR_O(lacunarity),	BU_STRUCTPARSE_FUNC_NULL },
	{"%f",	1, "H", 		SHDR_O(h_val),		BU_STRUCTPARSE_FUNC_NULL },
	{"%f",	1, "octaves", 		SHDR_O(octaves),	BU_STRUCTPARSE_FUNC_NULL },
	{"%f",  1, "scale",		SHDR_O(scale),		BU_STRUCTPARSE_FUNC_NULL },
	{"%f",  3, "delta",		SHDR_AO(delta),		BU_STRUCTPARSE_FUNC_NULL },
	{"%f",	1, "l",			SHDR_O(lacunarity),	BU_STRUCTPARSE_FUNC_NULL },
	{"%f",	1, "M", 		SHDR_O(max_d_p_mm),	BU_STRUCTPARSE_FUNC_NULL },
	{"%f",	1, "Max", 		SHDR_O(max_d_p_mm),	BU_STRUCTPARSE_FUNC_NULL },
	{"%f",	1, "m", 		SHDR_O(min_d_p_mm),	BU_STRUCTPARSE_FUNC_NULL },
	{"%f",	1, "min", 		SHDR_O(min_d_p_mm),	BU_STRUCTPARSE_FUNC_NULL },
	{"%f",	1, "o", 		SHDR_O(octaves),	BU_STRUCTPARSE_FUNC_NULL },
	{"%f",  1, "s",			SHDR_O(scale),		BU_STRUCTPARSE_FUNC_NULL },
	{"%f",  3, "vs",		SHDR_AO(vscale),	BU_STRUCTPARSE_FUNC_NULL },
	{"%f",  3, "d",			SHDR_AO(delta),		BU_STRUCTPARSE_FUNC_NULL },
	{"",	0, (char *)0,		0,			BU_STRUCTPARSE_FUNC_NULL }
};

HIDDEN int	scloud_setup(register struct region *rp, struct bu_vls *matparm, char **dpp, struct mfuncs *mfp, struct rt_i *rtip), scloud_render(struct application *ap, struct partition *pp, struct shadework *swp, char *dp), tsplat_render(struct application *ap, struct partition *pp, struct shadework *swp, char *dp);
HIDDEN void	scloud_print(register struct region *rp, char *dp), scloud_free(char *cp);

struct mfuncs scloud_mfuncs[] = {
	{MF_MAGIC,	"scloud",	0,	MFI_HIT, MFF_PROC,
	scloud_setup,	scloud_render,	scloud_print,	scloud_free },

	{MF_MAGIC,	"tsplat",	0,	MFI_HIT, MFF_PROC,
	scloud_setup,	tsplat_render,	scloud_print,	scloud_free },

	{0,		(char *)0,	0,		0, 0,
	0,		0,		0,		0 }
};


/*
 *	S C L O U D _ S E T U P
 */
HIDDEN int
scloud_setup(register struct region *rp, struct bu_vls *matparm, char **dpp, struct mfuncs *mfp, struct rt_i *rtip)


		/* pointer to reg_udata in *rp */


{
	register struct scloud_specific *scloud;
	struct db_full_path full_path;
	mat_t	region_to_model;
	mat_t	model_to_region;
	mat_t	tmp;

	BU_CK_VLS( matparm );
	BU_GETSTRUCT( scloud, scloud_specific );
	*dpp = (char *)scloud;

	if (rp->reg_aircode == 0) {
		bu_log("WARNING(%s): air shader '%s' applied to non-air region.\n%s\n",
			rp->reg_name,
			mfp->mf_name,
			"  Set air flag with \"edcodes\" in mged");
		bu_bomb("");
	}

	memcpy(scloud, &scloud_defaults, sizeof(struct scloud_specific) );
	if (rdebug&RDEBUG_SHADE)
		bu_log("scloud_setup\n");

	if (bu_struct_parse( matparm, scloud_parse, (char *)scloud ) < 0 )
		return(-1);

	if (rdebug&RDEBUG_SHADE)
		(void)bu_struct_print( rp->reg_name, scloud_parse, (char *)scloud );

	/* get transformation between world and "region" coordinates */
	if (db_string_to_path( &full_path, rtip->rti_dbip, rp->reg_name) ) {
		/* bad thing */
		rt_bomb("db_string_to_path() error");
	}
	if(! db_path_to_mat(rtip->rti_dbip, &full_path, region_to_model, 0, &rt_uniresource)) {
		/* bad thing */
		rt_bomb("db_path_to_mat() error");
	}

	/* get matrix to map points from model space to "region" space */
	bn_mat_inv(model_to_region, region_to_model);

	/* add the noise-space scaling */
	MAT_IDN(tmp);
	if (scloud->scale != 1.0) {
		tmp[0] = tmp[5] = tmp[10] = 1.0 / scloud->scale;
	} else {
		tmp[0] = 1.0 / (scloud->vscale[0]);
		tmp[5] = 1.0 / (scloud->vscale[1]);
		tmp[10] = 1.0 / (scloud->vscale[2]);
	}

	bn_mat_mul(scloud->mtos, tmp, model_to_region);

	/* add the translation within noise space */
	MAT_IDN(tmp);
	tmp[MDX] = scloud->delta[0];
	tmp[MDY] = scloud->delta[1];
	tmp[MDZ] = scloud->delta[2];
	bn_mat_mul2(tmp, scloud->mtos);
	bn_mat_inv(scloud->stom, scloud->mtos);

	return(1);
}

/*
 *	S C L O U D _ P R I N T
 */
HIDDEN void
scloud_print(register struct region *rp, char *dp)
{
	(void)bu_struct_print( rp->reg_name, scloud_pr, (char *)dp );
}

/*
 *	S C L O U D _ F R E E
 */
HIDDEN void
scloud_free(char *cp)
{
	bu_free( cp, "scloud_specific" );
}


/*
 *	T S P L A T _ R E N D E R
 *
 *	Sort of a surface spot transparency shader.  Picks transparency
 *	based upon noise value of surface spot.
 */
int
tsplat_render(struct application *ap, struct partition *pp, struct shadework *swp, char *dp)
{
	register struct scloud_specific *scloud_sp =
		(struct scloud_specific *)dp;
	point_t in_pt;	/* point where ray enters scloud solid */
	double  val;

	RT_CHECK_PT(pp);
	RT_AP_CHECK(ap);
	RT_CK_REGION(pp->pt_regionp);


	/* just shade the surface with a transparency */
	MAT4X3PNT(in_pt, scloud_sp->mtos, swp->sw_hit.hit_point);
	val = bn_noise_fbm(in_pt, scloud_sp->h_val,
			scloud_sp->lacunarity, scloud_sp->octaves );
	CLAMP(val, 0.0, 1.0);
	swp->sw_transmit = 1.0 - val;


	if (swp->sw_reflect > 0 || swp->sw_transmit > 0 )
		(void)rr_render( ap, pp, swp );

	return 1;
}


/*
 *	S C L O U D _ R E N D E R
 */
int
scloud_render(struct application *ap, struct partition *pp, struct shadework *swp, char *dp)
{
	register struct scloud_specific *scloud_sp =
		(struct scloud_specific *)dp;
	point_t in_pt;	/* point where ray enters scloud solid */
	point_t out_pt; /* point where ray leaves scloud solid */
	point_t pt;
	vect_t	v_cloud;/* vector representing ray/solid intersection */
	double	thickness; /* magnitude of v_cloud (distance through solid) */
	int	steps;	   /* # of samples along ray/solid intersection */
	double	step_delta;/* distance between sample points, texture space */
	int	i;
	double  val;
	double	trans;
	point_t	incident_light;
	double	delta_dpmm;
	double density;
	struct shadework sub_sw;
	struct light_specific *lp;

	RT_CHECK_PT(pp);
	RT_AP_CHECK(ap);
	RT_CK_REGION(pp->pt_regionp);

	/* compute the ray/solid in and out points,
	 * and transform them into "shader space" coordinates
	 */
	VJOIN1(pt, ap->a_ray.r_pt, pp->pt_inhit->hit_dist, ap->a_ray.r_dir);
	MAT4X3PNT(in_pt, scloud_sp->mtos, pt);

	VJOIN1(pt, ap->a_ray.r_pt, pp->pt_outhit->hit_dist, ap->a_ray.r_dir);
	MAT4X3PNT(out_pt, scloud_sp->mtos, pt);


	/* get ray/solid intersection vector (in noise space)
	 * and compute thickness of solid (in noise space) along ray path
	 */
	VSUB2(v_cloud, out_pt, in_pt);
	thickness = MAGNITUDE(v_cloud);

	/* The noise field used by the bn_noise_turb and bn_noise_fbm routines
	 * has a maximum frequency of about 1 cycle per integer step in
	 * noise space.  Each octave increases this frequency by the
	 * "lacunarity" factor.  To sample this space adequately we need
	 *
	 *	4 samples per integer step for the first octave,
	 *	lacunarity * 4 samples/step for the second octave,
	 * 	lacunarity^2 * 4 samples/step for the third octave,
	 * 	lacunarity^3 * 4 samples/step for the forth octave,
	 *
	 * so for a computation with 4 octaves we need something on the
	 * order of lacunarity^3 * 4 samples per integer step in noise space.
	 */

	steps = pow(scloud_sp->lacunarity, scloud_sp->octaves-1) * 4;
	step_delta = thickness / (double)steps;

	if (rdebug&RDEBUG_SHADE)
		bu_log("steps=%d  delta=%g  thickness=%g\n",
			steps, step_delta, thickness);

	VUNITIZE(v_cloud);
	VMOVE(pt, in_pt);
	trans = 1.0;

	delta_dpmm = scloud_sp->max_d_p_mm - scloud_sp->min_d_p_mm;

	sub_sw = *swp; /* struct copy */
	sub_sw.sw_inputs = MFI_HIT;

	for (i=0 ; i < steps ; i++ ) {
		/* compute the next point in the cloud space */
		VJOIN1(pt, in_pt, i*step_delta, v_cloud);

		/* get turbulence value (0 .. 1) */
		val = bn_noise_turb(pt, scloud_sp->h_val,
			scloud_sp->lacunarity, scloud_sp->octaves );

		density = scloud_sp->min_d_p_mm + val * delta_dpmm;

		val = exp( - density * step_delta);
		trans *= val;

		if (swp->sw_xmitonly) continue;

		/* need to set the hit in our fake shadework structure */
		MAT4X3PNT(sub_sw.sw_hit.hit_point, scloud_sp->stom, pt);
		sub_sw.sw_transmit = trans;

		sub_sw.sw_inputs = MFI_HIT;

		light_obs( ap, &sub_sw, swp->sw_inputs );
		/* now we know how much light has arrived from each
		 * light source to this point
		 */
		for (i=ap->a_rt_i->rti_nlights-1  ; i >= 0 ; i--) {
			lp = (struct light_specific *)swp->sw_visible[i];
			if (lp == LIGHT_NULL ) continue;

			/* compute how much light has arrived at
			 * this location
			 */
			incident_light[0] += sub_sw.sw_intensity[3*i+0] *
				lp->lt_color[0] * sub_sw.sw_lightfract[i];
			incident_light[1] += sub_sw.sw_intensity[3*i+1] *
				lp->lt_color[1] * sub_sw.sw_lightfract[i];
			incident_light[2] += sub_sw.sw_intensity[3*i+2] *
				lp->lt_color[2] * sub_sw.sw_lightfract[i];
		}

		VSCALE(incident_light, incident_light, trans);


	}

	/* scloud is basically a white object with partial transparency */
	swp->sw_transmit = trans;
	if (swp->sw_xmitonly )  return 1;


	/*
	 *  At the point of maximum opacity, check light visibility
	 *  for light color and cloud shadowing.
	 *  OOPS:  Don't use an interior point, or light_visibility()
	 *  will see an attenuated light source.
	 */
	swp->sw_hit.hit_dist = pp->pt_inhit->hit_dist;
	VJOIN1(swp->sw_hit.hit_point, ap->a_ray.r_pt, swp->sw_hit.hit_dist,
		ap->a_ray.r_dir);
	VREVERSE( swp->sw_hit.hit_normal, ap->a_ray.r_dir );
	swp->sw_inputs |= MFI_HIT | MFI_NORMAL;
	light_obs( ap, swp, swp->sw_inputs );
	VSETALL(incident_light, 0 );
	for( i=ap->a_rt_i->rti_nlights-1; i>=0; i-- )  {
		struct light_specific	*lp;
		if ((lp = (struct light_specific *)swp->sw_visible[i]) == LIGHT_NULL )
			continue;
		/* XXX don't have a macro for this */
		incident_light[0] += swp->sw_intensity[3*i+0] * lp->lt_color[0];
		incident_light[1] += swp->sw_intensity[3*i+1] * lp->lt_color[1];
		incident_light[2] += swp->sw_intensity[3*i+2] * lp->lt_color[2];
	}
	VELMUL( swp->sw_color, swp->sw_color, incident_light );


	if (rdebug&RDEBUG_SHADE ) {
		pr_shadework( "scloud: after light vis, before rr_render", swp);
	}

	if (swp->sw_reflect > 0 || swp->sw_transmit > 0 )
		(void)rr_render( ap, pp, swp );

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
