/*
 *	S H _ N O I S E . C
 *
 *	Shaders:
 *	gravel		turbulence noise applied to color and surface normal
 *	turbump		turbulence noise applied to surface normal
 *	turcolor	turbulence noise applied to color
 *	fbmbump		fbm noise applied to surface normal
 *	fbmcolor	fbm noise applied to color
 *
 *  Author -
 *	Lee A. Butler
 *  
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5068  USA
 *  
 *  Distribution Notice -
 *	Re-distribution of this software is restricted, as described in
 *	your "Statement of Terms and Conditions for the Release of
 *	The BRL-CAD Package" license agreement.
 *
 *  Copyright Notice -
 *	This software is Copyright (C) 1998 by the United States Army
 *	in all countries except the USA.  All rights reserved.
 */
#ifndef lint
static const char RCSid[] = "@(#)$Header$ (ARL)";
#endif

#include "conf.h"

#include <stdio.h>
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#include <math.h>
#include "machine.h"
#include "bu.h"
#include "vmath.h"
#include "bn.h"
#include "raytrace.h"
#include "shadefuncs.h"
#include "shadework.h"
#include "rtprivate.h"

extern int rr_render(struct application	*ap,
		     struct partition	*pp,
		     struct shadework   *swp);
#define noise_MAGIC 0x1847
#define CK_noise_SP(_p) BU_CKMAG(_p, noise_MAGIC, "noise_specific")

/* This allows us to specify the "size" parameter as values like ".5m"
 * or "27in" rather than using mm all the time.
 */
void
noise_cvt_parse( sdp, name, base, value )
register const struct bu_structparse	*sdp;	/* structure description */
register const char			*name;	/* struct member name */
char					*base;	/* begining of structure */
const char				*value;	/* string containing value */
{
	double *p = (double *)(base+sdp->sp_offset);

	if (rdebug&RDEBUG_SHADE)
		bu_log("%s value %s ", name, value);
	/* reconvert with optional units */
	*p = rt_mm_value(value);

	if (rdebug&RDEBUG_SHADE)
		bu_log(" %g\n", *p);

}

void
noise_deg_to_rad( sdp, name, base, value )
register const struct bu_structparse	*sdp;	/* structure description */
register const char			*name;	/* struct member name */
char					*base;	/* begining of structure */
const char				*value;	/* string containing value */
{
	double *p = (double *)(base+sdp->sp_offset);

	/* reconvert with optional units */
	*p = *p * (bn_pi / 180.0);
}

/*
 * the shader specific structure contains all variables which are unique
 * to any particular use of the shader.
 */
struct noise_specific {
	long	magic;	/* magic # for memory validity check, must come 1st */
	double	lacunarity;
	double	h_val;
	double	octaves;
	double	size;
	double	max_angle;
	point_t	vscale;	/* size of noise coordinate space */
	vect_t	delta;
	mat_t	m_to_sh;	/* model to shader space matrix */
	mat_t	sh_to_m;	/* shader to model space matrix */
	double	max_delta;
	double	nsd;
	double	minval;		/* don't use noise value less than this */
	int	shader_number;
};

/* The default values for the variables in the shader specific structure */
static const
struct noise_specific noise_defaults = {
	noise_MAGIC,
	2.1753974,	/* lacunarity */
	1.0,		/* h_val */
	4.0,		/* octaves */
	1.0,		/* size */
	1.57079632679489661923,		/* max_angle M_PI_2 */
	{ 1.0, 1.0, 1.0 },		/* vscale */
	{ 1000.0, 1000.0, 1000.0 },	/* delta into noise space */
	{	0.0, 0.0, 0.0, 0.0,	/* m_to_sh */
		0.0, 0.0, 0.0, 0.0,
		0.0, 0.0, 0.0, 0.0,
		0.0, 0.0, 0.0, 0.0 },
	{	0.0, 0.0, 0.0, 0.0,	/* sh_to_m */
		0.0, 0.0, 0.0, 0.0,
		0.0, 0.0, 0.0, 0.0,
		0.0, 0.0, 0.0, 0.0 },
	0.0,				/* max_delta */
	0.0,				/* nsd */
	0.0,				/* minval */
	0				/* shader_number */
	};

#define SHDR_NULL	((struct noise_specific *)0)
#define SHDR_O(m)	offsetof(struct noise_specific, m)
#define SHDR_AO(m)	bu_offsetofarray(struct noise_specific, m)


/* description of how to parse/print the arguments to the shader
 * There is at least one line here for each variable in the shader specific
 * structure above
 */
struct bu_structparse noise_print_tab[] = {
	{"%f",	1, "lacunarity",	SHDR_O(lacunarity),	BU_STRUCTPARSE_FUNC_NULL },
	{"%f",	1, "H", 		SHDR_O(h_val),		BU_STRUCTPARSE_FUNC_NULL },
	{"%f",	1, "octaves", 		SHDR_O(octaves),	BU_STRUCTPARSE_FUNC_NULL },
	{"%f",  3, "delta",		SHDR_AO(delta),		BU_STRUCTPARSE_FUNC_NULL },
	{"%f",  1, "size",		SHDR_O(size),		bu_mm_cvt },
	{"%f",  1, "angle",		SHDR_O(max_angle),	noise_deg_to_rad },
	{"%f",  3, "vscale",		SHDR_AO(vscale),	BU_STRUCTPARSE_FUNC_NULL },
	{"%f",  1, "min",		SHDR_O(minval),		BU_STRUCTPARSE_FUNC_NULL },
	{"",	0, (char *)0,		0,			BU_STRUCTPARSE_FUNC_NULL }
};

struct bu_structparse noise_parse_tab[] = {
	{"i",	bu_byteoffset(noise_print_tab[0]), "noise_print_tab", 0, BU_STRUCTPARSE_FUNC_NULL },
	{"%f",	1, "lacunarity",	SHDR_O(lacunarity),	BU_STRUCTPARSE_FUNC_NULL },
	{"%f",	1, "l",			SHDR_O(lacunarity),	BU_STRUCTPARSE_FUNC_NULL },
	{"%f",	1, "H", 		SHDR_O(h_val),		BU_STRUCTPARSE_FUNC_NULL },
	{"%f",	1, "octaves", 		SHDR_O(octaves),	BU_STRUCTPARSE_FUNC_NULL },
	{"%f",	1, "o", 		SHDR_O(octaves),	BU_STRUCTPARSE_FUNC_NULL },
	{"%f",  3, "delta",		SHDR_AO(delta),		BU_STRUCTPARSE_FUNC_NULL },
	{"%f",  3, "d",			SHDR_AO(delta),		BU_STRUCTPARSE_FUNC_NULL },
	{"%f",  1, "size",		SHDR_O(size),		bu_mm_cvt },
	{"%f",  1, "s",			SHDR_O(size),		bu_mm_cvt },
	{"%f",  1, "angle",		SHDR_O(max_angle),	noise_deg_to_rad },
	{"%f",  1, "ang",		SHDR_O(max_angle),	noise_deg_to_rad },
	{"%f",  1, "a",			SHDR_O(max_angle),	noise_deg_to_rad },
	{"%f",  3, "vscale",		SHDR_AO(vscale),	BU_STRUCTPARSE_FUNC_NULL },
	{"%f",  3, "vs",		SHDR_AO(vscale),	BU_STRUCTPARSE_FUNC_NULL },
	{"%f",  3, "v",			SHDR_AO(vscale),	BU_STRUCTPARSE_FUNC_NULL },
	{"%f",  1, "min",		SHDR_O(minval),		BU_STRUCTPARSE_FUNC_NULL },
	{"",	0, (char *)0,		0,			BU_STRUCTPARSE_FUNC_NULL }
};

HIDDEN int	noise_setup(), noise_render(),
		fbmbump_render(), turbump_render(),
		fbmcolor_render(), turcolor_render(),
		fractal_render();
HIDDEN void	noise_print(), noise_free();

/* The "mfuncs" structure defines the external interface to the shader.
 * Note that more than one shader "name" can be associated with a given
 * shader by defining more than one mfuncs struct in this array.
 * See sh_phong.c for an example of building more than one shader "name"
 * from a set of source functions.  There you will find that "glass" "mirror"
 * and "plastic" are all names for the same shader with different default
 * values for the parameters.
 *
 * WARNING:  The order of this table is critical for these shaders.
 */
struct mfuncs noise_mfuncs[] = {
	{MF_MAGIC,	"gravel",	0,	MFI_NORMAL|MFI_HIT|MFI_UV, 0,
	noise_setup,	fractal_render,	noise_print,	noise_free },

	{MF_MAGIC,	"fbmbump",	0,	MFI_NORMAL|MFI_HIT|MFI_UV, 0,
	noise_setup,	fractal_render,	noise_print,	noise_free },

	{MF_MAGIC,	"turbump",	0,	MFI_NORMAL|MFI_HIT|MFI_UV, 0,
	noise_setup,	fractal_render, noise_print,	noise_free },

	{MF_MAGIC,	"fbmcolor",	0,	MFI_NORMAL|MFI_HIT|MFI_UV, 0,
	noise_setup,	fractal_render, noise_print,	noise_free },

	{MF_MAGIC,	"turcolor",	0,	MFI_NORMAL|MFI_HIT|MFI_UV, 0,
	noise_setup,	fractal_render, noise_print,	noise_free },

	{MF_MAGIC,	"grunge",	0,	MFI_NORMAL|MFI_HIT|MFI_UV, 0,
	noise_setup,	fractal_render, noise_print,	noise_free },

	{MF_MAGIC,	"turcombo",	0,	MFI_NORMAL|MFI_HIT|MFI_UV, 0,
	noise_setup,	fractal_render, noise_print,	noise_free },

	{MF_MAGIC,	"fbmcombo",	0,	MFI_NORMAL|MFI_HIT|MFI_UV, 0,
	noise_setup,	fractal_render,	noise_print,	noise_free },

	{MF_MAGIC,	"flash",	0,	MFI_NORMAL|MFI_HIT|MFI_UV, 0,
	noise_setup,	fractal_render, noise_print,	noise_free },

	{0,		(char *)0,	0,	0,		0,
	0,		0,		0,		0 }
};


/*	G R A V E L _ S E T U P
 *
 *	This routine is called (at prep time)
 *	once for each region which uses this shader.
 *	Any shader-specific initialization should be done here.
 */
HIDDEN int
noise_setup( rp, matparm, dpp, mfp, rtip)
register struct region	*rp;
struct bu_vls		*matparm;
char			**dpp;	/* pointer to reg_udata in *rp */
struct mfuncs		*mfp;
struct rt_i		*rtip;	/* New since 4.4 release */
{
	register struct noise_specific	*noise_sp;
	mat_t	tmp;
	mat_t model_to_region;
	int i;

	/* check the arguments */
	RT_CHECK_RTI(rtip);
	BU_CK_VLS( matparm );
	RT_CK_REGION(rp);


	if (rdebug&RDEBUG_SHADE)
		bu_log("noise_setup(%s, %s) (%s)\n",
			rp->reg_name, bu_vls_addr(matparm),
			rp->reg_mater.ma_shader);

	/* Get memory for the shader parameters and shader-specific data */
	BU_GETSTRUCT( noise_sp, noise_specific );
	*dpp = (char *)noise_sp;

	/* initialize the default values for the shader */
	memcpy(noise_sp, &noise_defaults, sizeof(struct noise_specific) );

	/* parse the user's arguments for this use of the shader. */
	if (bu_struct_parse( matparm, noise_parse_tab, (char *)noise_sp ) < 0 )
		return(-1);

	/* figure out which shader is really being called */
	for (i = 0 ; noise_mfuncs[i].mf_name ; i++ ) {
		if (!strcmp(noise_mfuncs[i].mf_name, mfp->mf_name))
			goto found;
	}
	bu_log("shader name \"%s\" not recognized, assuming \"%s\"\n",
		mfp->mf_name, noise_mfuncs[0].mf_name);
	i = 0;
found:
	noise_sp->shader_number = i;

	db_region_mat(model_to_region, rtip->rti_dbip, rp->reg_name, &rt_uniresource);

	MAT_IDN(tmp);
	if (noise_sp->size != 1.0) {
		/* the user sets "size" to the size of the biggest
		 * noise-space blob in model coordinates
		 */
		tmp[0] = tmp[5] = tmp[10] = 1.0/noise_sp->size;
	} else {
		tmp[0] = 1.0/noise_sp->vscale[0];
		tmp[5] = 1.0/noise_sp->vscale[1];
		tmp[10] = 1.0/noise_sp->vscale[2];
	}

	bn_mat_mul(noise_sp->m_to_sh, tmp, model_to_region);

	/* Add any translation within shader/region space */
	MAT_IDN(tmp);
	tmp[MDX] = noise_sp->delta[0];
	tmp[MDY] = noise_sp->delta[1];
	tmp[MDZ] = noise_sp->delta[2];
	bn_mat_mul2(tmp, noise_sp->m_to_sh);

	bn_mat_inv(noise_sp->sh_to_m, noise_sp->m_to_sh);

	noise_sp->nsd = 1.0 / 
		pow(noise_sp->lacunarity, noise_sp->octaves);

	if (rdebug&RDEBUG_SHADE) {
		bu_struct_print( " Parameters:", noise_print_tab, (char *)noise_sp );
		bn_mat_print( "m_to_sh", noise_sp->m_to_sh );
	}

	return(1);
}

/*
 *	G R A V E L _ P R I N T
 */
HIDDEN void
noise_print( rp, dp )
register struct region *rp;
char	*dp;
{
	bu_struct_print( rp->reg_name, noise_print_tab, (char *)dp );
}

/*
 *	G R A V E L _ F R E E
 */
HIDDEN void
noise_free( cp )
char *cp;
{
	bu_free( cp, "noise_specific" );
}
#define RESCALE_NOISE(n) n += 1.0

/*
 *	N O R M _ N O I S E
 *
 *	Apply a noise function to the surface normal
 */
static void
norm_noise(pt, val, noise_sp, func, swp, rescale)
point_t pt;
double val;
struct noise_specific *noise_sp;
double (*func)();
struct shadework	*swp;	/* defined in material.h */
int rescale;
{
	vect_t N, tmp;
	point_t u_pt, v_pt;
	vect_t u_vec, v_vec;
	double u_val, v_val;
	mat_t u_mat, v_mat;

	/* dork the normal around
	 * Convert the normal to shader space, get u,v coordinate system
	 */

	if (rdebug&RDEBUG_SHADE) {
		VPRINT("Model space Normal", swp->sw_hit.hit_normal);
	}
	MAT4X3VEC(N, noise_sp->m_to_sh, swp->sw_hit.hit_normal);
	VUNITIZE(N);
	if (rdebug&RDEBUG_SHADE) {
		VPRINT("Shader space Normal", N);
	}

	/* construct coordinate system from vectors perpendicular to normal */
	bn_vec_perp(u_vec, N);
	VCROSS(v_vec, N, u_vec);

	/* compute noise function at position slightly off pt in both
	 * U and V directions to get change in values
	 */
	VJOIN1(u_pt, pt, noise_sp->nsd, u_vec);
	u_val = func(u_pt, noise_sp->h_val, noise_sp->lacunarity,
		noise_sp->octaves);

	if (rescale) RESCALE_NOISE(u_val);

	VJOIN1(v_pt, pt, noise_sp->nsd, v_vec);
	v_val = func(v_pt, noise_sp->h_val, noise_sp->lacunarity,
		noise_sp->octaves);

	if (rescale) RESCALE_NOISE(v_val);

	/* construct normal rotation about U and V vectors based upon 
	 * variation in surface in each direction.  Apply the result to
	 * the surface normal.
	 */
	bn_mat_arb_rot(u_mat, pt, u_vec, (val - v_val) * noise_sp->max_angle);
	MAT4X3VEC(tmp, u_mat, N);

	bn_mat_arb_rot(v_mat, pt, v_vec, (val - u_val) * noise_sp->max_angle);

	MAT4X3VEC(N, v_mat, tmp);

	if (rdebug&RDEBUG_SHADE) {
		VPRINT("old normal", swp->sw_hit.hit_normal);
	}

	MAT4X3VEC(swp->sw_hit.hit_normal, noise_sp->sh_to_m, N);
	VUNITIZE(swp->sw_hit.hit_normal);
	if (rdebug&RDEBUG_SHADE) {
		VPRINT("new normal", swp->sw_hit.hit_normal);
	}
}

/*
 *	F R A C T A L _ R E N D E R
 *
 *	This is called (from viewshade() in shade.c) once for each hit point
 *	to be shaded.  The purpose here is to fill in values in the shadework
 *	structure.
 */
int
fractal_render( ap, pp, swp, dp )
struct application	*ap;
struct partition	*pp;
struct shadework	*swp;	/* defined in material.h */
char			*dp;	/* ptr to the shader-specific struct */
{
	register struct noise_specific *noise_sp =
		(struct noise_specific *)dp;
	point_t pt;
	double val;

	/* check the validity of the arguments we got */
	RT_AP_CHECK(ap);
	RT_CHECK_PT(pp);
	CK_noise_SP(noise_sp);

	if (rdebug&RDEBUG_SHADE)
		bu_struct_print( "noise_render Parameters:", noise_print_tab, (char *)noise_sp );

	/* If we are performing the shading in "region" space, we must 
	 * transform the hit point from "model" space to "region" space.
	 * See the call to db_region_mat in noise_setup().
	 */
	MAT4X3PNT(pt, noise_sp->m_to_sh, swp->sw_hit.hit_point);

	if (rdebug&RDEBUG_SHADE) {
		bu_log("%s:%d noise_render(%s)  model:(%g %g %g) shader:(%g %g %g)\n", 
		__FILE__, __LINE__,
		noise_mfuncs[noise_sp->shader_number].mf_name,
		V3ARGS(swp->sw_hit.hit_point),
		V3ARGS(pt) );
	}

	switch (noise_sp->shader_number) {
	case 0:	/* gravel */
	case 6: /* turcombo */
		val = bn_noise_turb(pt, noise_sp->h_val,
			noise_sp->lacunarity, noise_sp->octaves);
		if (val < noise_sp->minval )  val = noise_sp->minval;
#if RT_MULTISPECTRAL
		swp->sw_temperature *= val;
#else
		VSCALE(swp->sw_color, swp->sw_color, val);
#endif
		norm_noise(pt, val, noise_sp, bn_noise_turb, swp, 0);
		break;
	case 1:	/* fbmbump */
		val = bn_noise_fbm(pt, noise_sp->h_val,
			noise_sp->lacunarity, noise_sp->octaves);
		RESCALE_NOISE(val);
		norm_noise(pt, val, noise_sp, bn_noise_fbm, swp, 1);
		break;
	case 2:	/* turbump */
		val = bn_noise_turb(pt, noise_sp->h_val,
			noise_sp->lacunarity, noise_sp->octaves);

		norm_noise(pt, val, noise_sp, bn_noise_turb, swp, 0);
		break;
	case 3:	/* fbmcolor */
		val = bn_noise_fbm(pt, noise_sp->h_val,
			noise_sp->lacunarity, noise_sp->octaves);
		RESCALE_NOISE(val);
		if (val < noise_sp->minval )  val = noise_sp->minval;
#if RT_MULTISPECTRAL
		swp->sw_temperature *= val;
#else
		VSCALE(swp->sw_color, swp->sw_color, val);
#endif
		break;
	case 4:	/* turcolor */
		val = bn_noise_turb(pt, noise_sp->h_val,
			noise_sp->lacunarity, noise_sp->octaves);
		if (val < noise_sp->minval )  val = noise_sp->minval;
#if RT_MULTISPECTRAL
		swp->sw_temperature *= val;
#else
		VSCALE(swp->sw_color, swp->sw_color, val);
#endif
		break;
	case 5: /* grunge */
	case 7: /* fbmcombo */
		val = bn_noise_fbm(pt, noise_sp->h_val,
			noise_sp->lacunarity, noise_sp->octaves);
		RESCALE_NOISE(val);
		if (val < noise_sp->minval )  val = noise_sp->minval;
#if RT_MULTISPECTRAL
		swp->sw_temperature *= val;
#else
		VSCALE(swp->sw_color, swp->sw_color, val);
#endif
		norm_noise(pt, val, noise_sp, bn_noise_fbm, swp, 1);
		break;

	case 8: /* flash */
		val = bn_noise_fbm(pt, noise_sp->h_val,
			noise_sp->lacunarity, noise_sp->octaves);

		val = 1.0 - val;
		if (val < noise_sp->minval )  val = noise_sp->minval;

#if RT_MULTISPECTRAL
		swp->sw_temperature *= val;
#else
		VSCALE(swp->sw_color, swp->sw_color, val);
#endif
		swp->sw_temperature = val * 2000.0;
		break;
	}


	/* shader must perform transmission/reflection calculations
	 *
	 * 0 < swp->sw_transmit <= 1 causes transmission computations
	 * 0 < swp->sw_reflect <= 1 causes reflection computations
	 */
	if (swp->sw_reflect > 0 || swp->sw_transmit > 0 )
		(void)rr_render( ap, pp, swp );

	return(1);
}
