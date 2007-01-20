/*                       S H _ C A M O . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2007 United States Government as represented by
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
/** @file sh_camo.c
 *
 *	A shader to apply a crude camoflage color pattern to an object
 *	using a fractional Brownian motion of 3 colors
 *
 *	At each hit point, the shader evaluate the fbm to obtain a "Noise"
 *	value between -1 and 1.  The color produced is:
 *
 *	Noise value		Color
 *	-1 <= N < t1		 c1
 *	t1 <= N < t2		 c2
 *	t2 <= N <= 1		 c3
 */
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

#ifdef RT_MULTISPECTRAL
#  include "spectrum.h"
extern const struct bn_table	*spectrum;
#endif

#if !defined(M_PI)
#  define M_PI            3.14159265358979323846
#endif

#define SMOOTHSTEP(x)  ((x)*(x)*(3 - 2*(x)))


#define camo_MAGIC 0x18364	/* XXX change this number for each shader */
struct camo_specific {
	long	magic;
	double	noise_lacunarity;
	double	noise_h_val;
	double	noise_octaves;
	double	noise_size;
	point_t	noise_vscale;	/* size of noise coordinate space */
	vect_t	noise_delta;	/* a delta in noise space to be applied to pts */

	double	t1;	/* noise threshold for color1 */
	double	t2;	/* noise threshold for color2 */
	point_t c1;	/* color 1 */
	point_t c2;	/* color 2 */
	point_t c3;	/* color 3 */
	mat_t	xform;	/* model->region coord sys matrix */
};
#define CK_camo_SP(_p) BU_CKMAG(_p, camo_MAGIC, "camo_specific")

/* This allows us to specify the "size" parameter as values like ".5m"
 * or "27in" rather than using mm all the time.
 */
void
camo_cvt_parse(register const struct bu_structparse *sdp, register const char *name, char *base, const char *value)
                                    	     	/* structure description */
                   			      	/* struct member name */
    					      	/* begining of structure */
          				       	/* string containing value */
{
	double *p = (double *)(base+sdp->sp_offset);

	/* reconvert with optional units */
	*p = rt_mm_value(value);
}


static struct camo_specific camo_defaults = {
	camo_MAGIC,
	2.1753974,		/* noise_lacunarity */
	1.0,			/* noise_h_val */
	4.0,			/* noise_octaves */
	1.0,			/* noise_size */
	{ 0.0125, 0.0125, 0.0125 },	/* noise_vscale */
	{ 1000.0, 1000.0, 1000.0 },	/* delta into noise space */
	-0.25,			/* t1 */
	0.25,			/* t2 */
	{ .38, .29, .16 },	/* darker color c1 (97/74/41) */
	{ .1, .30, .04 },	/* basic color c2 (26/77/10) */
	{ .15, .15, .15 }	/* dark black (38/38/38) */
	};

static struct camo_specific marble_defaults = {
	camo_MAGIC,
	2.1753974,		/* noise_lacunarity */
	1.0,			/* noise_h_val */
	4.0,			/* noise_octaves */
	1.0,			/* noise_size */
	{ 1.0, 1.0, 1.0 },	/* noise_vscale */
	{ 1000.0, 1000.0, 1000.0 },	/* delta into noise space */
	0.25,			/* t1 */
	0.5,			/* t2 */
	{ .8, .2, .16 },	/* darker color c1 (97/74/41) */
	{ .9, .9, .8 },	/* basic color c2 (26/77/10) */
	{ .15, .15, .15 }	/* dark black (38/38/38) */
	};

#define SHDR_NULL	((struct camo_specific *)0)
#define SHDR_O(m)	bu_offsetof(struct camo_specific, m)
#define SHDR_AO(m)	bu_offsetofarray(struct camo_specific, m)

void color_fix(register const struct bu_structparse *sdp, register const char *name, char *base, const char *value);

struct bu_structparse camo_print_tab[] = {
	{"%f",	1, "lacunarity",	SHDR_O(noise_lacunarity),	BU_STRUCTPARSE_FUNC_NULL },
	{"%f",	1, "H", 		SHDR_O(noise_h_val),		BU_STRUCTPARSE_FUNC_NULL },
	{"%f",	1, "octaves", 		SHDR_O(noise_octaves),	BU_STRUCTPARSE_FUNC_NULL },
	{"%f",  1, "size",		SHDR_O(noise_size),		bu_mm_cvt },
	{"%f",  3, "vscale",		SHDR_AO(noise_vscale),	BU_STRUCTPARSE_FUNC_NULL },
	{"%f",	1, "thresh1",		SHDR_O(t1),		BU_STRUCTPARSE_FUNC_NULL },
	{"%f",	1, "thresh2",		SHDR_O(t2),		BU_STRUCTPARSE_FUNC_NULL },
	{"%f",  3, "color1",		SHDR_AO(c1),		color_fix },
	{"%f",  3, "color2",		SHDR_AO(c2),		color_fix },
	{"%f",  3, "color3",		SHDR_AO(c3),		color_fix },
	{"%f",  3, "delta",		SHDR_AO(noise_delta),	BU_STRUCTPARSE_FUNC_NULL },
	{"",	0, (char *)0,		0,			BU_STRUCTPARSE_FUNC_NULL }
};

struct bu_structparse camo_parse[] = {
	{"%f",	1, "lacunarity",	SHDR_O(noise_lacunarity),	BU_STRUCTPARSE_FUNC_NULL },
	{"%f",	1, "l",			SHDR_O(noise_lacunarity),	BU_STRUCTPARSE_FUNC_NULL },
	{"%f",	1, "H", 		SHDR_O(noise_h_val),		BU_STRUCTPARSE_FUNC_NULL },
	{"%f",	1, "octaves", 		SHDR_O(noise_octaves),	BU_STRUCTPARSE_FUNC_NULL },
	{"%f",	1, "o", 		SHDR_O(noise_octaves),	BU_STRUCTPARSE_FUNC_NULL },
	{"%f",	1, "t1",		SHDR_O(t1),		BU_STRUCTPARSE_FUNC_NULL },
	{"%f",	1, "t2",		SHDR_O(t2),		BU_STRUCTPARSE_FUNC_NULL },
	{"%f",  1, "size",		SHDR_O(noise_size),		bu_mm_cvt },
	{"%f",  1, "s",			SHDR_O(noise_size),		bu_mm_cvt },
	{"%f",  3, "vscale",		SHDR_AO(noise_vscale),	BU_STRUCTPARSE_FUNC_NULL },
	{"%f",  3, "vs",		SHDR_AO(noise_vscale),	BU_STRUCTPARSE_FUNC_NULL },
	{"%f",  3, "v",			SHDR_AO(noise_vscale),	BU_STRUCTPARSE_FUNC_NULL },
	{"%f",  3, "c1",		SHDR_AO(c1),		color_fix },
	{"%f",  3, "c2",		SHDR_AO(c2),		color_fix },
	{"%f",  3, "c3",		SHDR_AO(c3),		color_fix },
	{"%f",  3, "delta",		SHDR_AO(noise_delta),		BU_STRUCTPARSE_FUNC_NULL },
	{"%f",  3, "d",			SHDR_AO(noise_delta),		BU_STRUCTPARSE_FUNC_NULL },
	{"",	0, (char *)0,		0,			BU_STRUCTPARSE_FUNC_NULL }
};

HIDDEN int	marble_setup(register struct region *rp, struct bu_vls *matparm, char **dpp, struct mfuncs *mfp, struct rt_i *rtip), marble_render(struct application *ap, struct partition *pp, struct shadework *swp, char *dp);
HIDDEN int	camo_setup(register struct region *rp, struct bu_vls *matparm, char **dpp, struct mfuncs *mfp, struct rt_i *rtip), camo_render(struct application *ap, struct partition *pp, struct shadework *swp, char *dp);
HIDDEN void	camo_print(register struct region *rp, char *dp), camo_free(char *cp);

struct mfuncs camo_mfuncs[] = {
	{MF_MAGIC,	"camo",		0,		MFI_HIT,	0,
	camo_setup,	camo_render,	camo_print,	camo_free },

	{MF_MAGIC,	"marble",		0,		MFI_HIT,	0,
	marble_setup,	marble_render,	camo_print,	camo_free },

	{0,		(char *)0,	0,		0,		0,
	0,		0,		0,		0 }
};

/*  color_fix
 *
 *  Used as a hooked function for input of color values
 */
void
color_fix(register const struct bu_structparse *sdp, register const char *name, char *base, const char *value)
                                    	     	/* structure description */
                   			      	/* struct member name */
    					      	/* begining of structure */
          				       	/* string containing value */
{
	register double *p = (double *)(base+sdp->sp_offset);
	register int i;
	int ok;

	/* if all the values are in the range [0..1] there's nothing to do */
	for (ok=1, i=0 ; i < sdp->sp_count ; i++, p++) {
		if (*p > 1.0) ok = 0;
	}
	if (ok) return;

	/* user specified colors in the range [0..255] so we need to
	 * map those into [0..1]
	 */
	p = (double *)(base+sdp->sp_offset);
	for (i=0 ; i < sdp->sp_count ; i++, p++) {
		*p /= 255.0;
	}
}




/*	C A M O _ S E T U P
 *
 *	This routine is called (at prep time)
 *	once for each region which uses this shader.
 *	Any shader-specific initialization should be done here.
 */
HIDDEN int
camo_setup(register struct region *rp, struct bu_vls *matparm, char **dpp, struct mfuncs *mfp, struct rt_i *rtip)


    			      	/* pointer to reg_udata in *rp */

           		      	/* New since 4.4 release */
{
	register struct camo_specific	*camo_sp;
	mat_t	model_to_region;
	mat_t	tmp;

	RT_CHECK_RTI(rtip);
	BU_CK_VLS( matparm );
	RT_CK_REGION(rp);
	BU_GETSTRUCT( camo_sp, camo_specific );
	*dpp = (char *)camo_sp;

	if (rdebug&RDEBUG_SHADE) {
		bu_log("camouflage parameters = '%s'\n", bu_vls_addr(matparm));
	}
	memcpy(camo_sp, &camo_defaults, sizeof(struct camo_specific) );

	if (bu_struct_parse( matparm, camo_parse, (char *)camo_sp ) < 0 )
		return(-1);

	/* Optional:  get the matrix which maps model space into
	 *  "region" or "shader" space
	 */
	db_region_mat(model_to_region, rtip->rti_dbip, rp->reg_name, &rt_uniresource);

	/* add the noise-space scaling */
	MAT_IDN(tmp);
	if (camo_sp->noise_size != 1.0) {
		/* the user sets "noise_size" to the size of the biggest
		 * noise-space blob in model coordinates
		 */
		tmp[0] = tmp[5] = tmp[10] = 1.0/camo_sp->noise_size;
	} else {
		tmp[0] = 1.0/camo_sp->noise_vscale[0];
		tmp[5] = 1.0/camo_sp->noise_vscale[1];
		tmp[10] = 1.0/camo_sp->noise_vscale[2];
	}

	bn_mat_mul(camo_sp->xform, tmp, model_to_region);

	/* Add any translation within shader/region space */
	MAT_IDN(tmp);
	tmp[MDX] = camo_sp->noise_delta[0];
	tmp[MDY] = camo_sp->noise_delta[1];
	tmp[MDZ] = camo_sp->noise_delta[2];
	bn_mat_mul2(tmp, camo_sp->xform);

	if (rdebug&RDEBUG_SHADE) {
		bu_struct_print( rp->reg_name, camo_print_tab,
			(char *)camo_sp );
		bn_mat_print( "xform", camo_sp->xform );
	}

	return(1);
}

/*
 *	C A M O _ P R I N T
 */
HIDDEN void
camo_print(register struct region *rp, char *dp)
{
	bu_struct_print( rp->reg_name, camo_print_tab, (char *)dp );
}

/*
 *	C A M O _ F R E E
 */
HIDDEN void
camo_free(char *cp)
{
	bu_free( cp, "camo_specific" );
}

/*
 *	C A M O _ R E N D E R
 *
 *	This is called (from viewshade() in shade.c)
 *	once for each hit point to be shaded.
 */
int
camo_render(struct application *ap, struct partition *pp, struct shadework *swp, char *dp)
{
	register struct camo_specific *camo_sp =
		(struct camo_specific *)dp;
	point_t pt;
	double val;
#ifdef RT_MULTISPECTRAL
	float fcolor[3];
#endif

	RT_AP_CHECK(ap);
	RT_CHECK_PT(pp);
	CK_camo_SP(camo_sp);

	if (rdebug&RDEBUG_SHADE)
		bu_struct_print( "foo", camo_parse, (char *)camo_sp );

	/* Optional: transform hit point into "shader-space coordinates" */
	MAT4X3PNT(pt, camo_sp->xform, swp->sw_hit.hit_point);


	/* bn_noise_fbm returns a value in the approximate range of
	 *	-1.0 ~<= bn_noise_fbm() ~<= 1.0
	 */
	val = bn_noise_fbm(pt, camo_sp->noise_h_val,
		camo_sp->noise_lacunarity, camo_sp->noise_octaves );

#ifdef RT_MULTISPECTRAL
	BN_CK_TABDATA(swp->msw_color);
	if (val < camo_sp->t1) {
		VMOVE(fcolor, camo_sp->c1 );
		rt_spect_reflectance_rgb( swp->msw_color, fcolor);
	} else if (val < camo_sp->t2 ) {
		VMOVE(fcolor, camo_sp->c2 );
		rt_spect_reflectance_rgb( swp->msw_color, fcolor);
	} else {
		VMOVE(fcolor, camo_sp->c3 );
		rt_spect_reflectance_rgb( swp->msw_color, fcolor);
	}
#else
	if (val < camo_sp->t1) {
		VMOVE(swp->sw_color, camo_sp->c1);
	} else if (val < camo_sp->t2 ) {
		VMOVE(swp->sw_color, camo_sp->c2);
	} else {
		VMOVE(swp->sw_color, camo_sp->c3);
	}
#endif

	return(1);
}
/*	M A R B L E 2 _ S E T U P
 *
 *	This routine is called (at prep time)
 *	once for each region which uses this shader.
 *	Any shader-specific initialization should be done here.
 */
HIDDEN int
marble_setup(register struct region *rp, struct bu_vls *matparm, char **dpp, struct mfuncs *mfp, struct rt_i *rtip)


    			      	/* pointer to reg_udata in *rp */

           		      	/* New since 4.4 release */
{
	register struct camo_specific	*camo_sp;
	mat_t	model_to_region;
	mat_t	tmp;

	RT_CHECK_RTI(rtip);
	BU_CK_VLS( matparm );
	RT_CK_REGION(rp);
	BU_GETSTRUCT( camo_sp, camo_specific );
	*dpp = (char *)camo_sp;

	if (rdebug&RDEBUG_SHADE) {
		bu_log("marble parameters = '%s'\n", bu_vls_addr(matparm));
	}
	memcpy(camo_sp, &marble_defaults, sizeof(struct camo_specific) );

	if (bu_struct_parse( matparm, camo_parse, (char *)camo_sp ) < 0 )
		return(-1);

	/* Optional:  get the matrix which maps model space into
	 *  "region" or "shader" space
	 */
	db_region_mat(model_to_region, rtip->rti_dbip, rp->reg_name, &rt_uniresource);

	/* add the noise-space scaling */
	MAT_IDN(tmp);
	if (camo_sp->noise_size != 1.0) {
		/* the user sets "noise_size" to the size of the biggest
		 * noise-space blob in model coordinates
		 */
		tmp[0] = tmp[5] = tmp[10] = 1.0/camo_sp->noise_size;
	} else {
		tmp[0] = 1.0/camo_sp->noise_vscale[0];
		tmp[5] = 1.0/camo_sp->noise_vscale[1];
		tmp[10] = 1.0/camo_sp->noise_vscale[2];
	}

	bn_mat_mul(camo_sp->xform, tmp, model_to_region);

	/* Add any translation within shader/region space */
	MAT_IDN(tmp);
	tmp[MDX] = camo_sp->noise_delta[0];
	tmp[MDY] = camo_sp->noise_delta[1];
	tmp[MDZ] = camo_sp->noise_delta[2];
	bn_mat_mul2(tmp, camo_sp->xform);

	if (rdebug&RDEBUG_SHADE) {
		bu_struct_print( rp->reg_name, camo_print_tab,
			(char *)camo_sp );
		bn_mat_print( "xform", camo_sp->xform );
	}

	return(1);
}



/*
 *	M A R B L E 2 _ R E N D E R
 *
 *	This is called (from viewshade() in shade.c)
 *	once for each hit point to be shaded.
 */
int
marble_render(struct application *ap, struct partition *pp, struct shadework *swp, char *dp)
{
	register struct camo_specific *camo_sp =
		(struct camo_specific *)dp;
	point_t pt;
	double val, inv_val;
#ifdef RT_MULTISPECTRAL
	float fcolor[3];
#endif

	RT_AP_CHECK(ap);
	RT_CHECK_PT(pp);
	CK_camo_SP(camo_sp);

	if (rdebug&RDEBUG_SHADE)
		bu_struct_print( "foo", camo_parse, (char *)camo_sp );

	/* Optional: transform hit point into "shader-space coordinates" */
	MAT4X3PNT(pt, camo_sp->xform, swp->sw_hit.hit_point);


	/* bn_noise_turb returns a value in the approximate range of
	 *	0.0 ~<= bn_noise_turb() ~<= 1.0
	 */
	val = bn_noise_turb(pt, camo_sp->noise_h_val,
		camo_sp->noise_lacunarity, camo_sp->noise_octaves );

#if 0
	val *= 2.0;
	CLAMP(val, 0.0, 1.0);
#else
	val = sin(val*M_PI);

#endif
	inv_val = 1.0 - val;

#ifdef RT_MULTISPECTRAL
	{
		struct bn_tabdata *tcolor;

		BN_CK_TABDATA(swp->msw_color);
		BN_GET_TABDATA( tcolor, spectrum );

		VMOVE(fcolor, camo_sp->c2 );

		rt_spect_reflectance_rgb( tcolor, fcolor );
		bn_tabdata_blend2( swp->msw_color, val, swp->msw_color,
			inv_val, tcolor );
		bn_tabdata_free( tcolor );
	}
#else
	VCOMB2(swp->sw_color, val, swp->sw_color, inv_val, camo_sp->c2);
#endif

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
