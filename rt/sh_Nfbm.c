/*
 *	S H _ N F B M . C
 *
 */
#include "conf.h"

#include <stdio.h>
#include <math.h>
#include "machine.h"
#include "vmath.h"
#include "raytrace.h"
#include "./material.h"
#include "./mathtab.h"
#include "./rdebug.h"


#define Nfbm_MAGIC 0x18364	/* XXX change this number for each shader */
struct Nfbm_specific {
	long	magic;
	double	lacunarity;
	double	h_val;
	double	octaves;
	double	max_angle;
	double	size;
	point_t vsize;
	vect_t	delta;
	mat_t	xform;
};
#define CK_Nfbm_SP(_p) RT_CKMAG(_p, Nfbm_MAGIC, "Nfbm_specific")

static struct Nfbm_specific Nfbm_defaults = {
	Nfbm_MAGIC,
	2.1753974,			/* lacunarity */
	1.0,				/* h_val */
	4,				/* octaves */
	22.5,				/* Angle  */
	1.0,				/* size */
	{ 1.0, 1.0, 1.0 },		/* Vector Size */
	{ 1000.0, 1000.0, 1000.0 }	/* delta */
	};

#define SHDR_NULL	((struct Nfbm_specific *)0)
#define SHDR_O(m)	offsetof(struct Nfbm_specific, m)
#define SHDR_AO(m)	offsetofarray(struct Nfbm_specific, m)

void
cvt_parse( sdp, name, base, value )
register CONST struct bu_structparse	*sdp;	/* structure description */
register CONST char			*name;	/* struct member name */
char					*base;	/* begining of structure */
CONST char				*value;	/* string containing value */
{
	double v;
	double *p = (double *)(base+sdp->sp_offset);

	/* reconvert with optional units */
	*p = rt_mm_value(value);
}

struct bu_structparse Nfbm_parse[] = {
	{"%f",	1, "lacunarity",	SHDR_O(lacunarity),	FUNC_NULL },
	{"%f",	1, "l",			SHDR_O(lacunarity),	FUNC_NULL },
	{"%f",	1, "H", 		SHDR_O(h_val),		FUNC_NULL },
	{"%f",	1, "octaves", 		SHDR_O(octaves),	FUNC_NULL },
	{"%f",	1, "o", 		SHDR_O(octaves),	FUNC_NULL },
	{"%f",	1, "angle",		SHDR_O(max_angle),	FUNC_NULL },
	{"%f",	1, "a", 		SHDR_O(max_angle),	FUNC_NULL },
	{"%f",  1, "size",		SHDR_O(size),		cvt_parse },
	{"%f",  1, "s",			SHDR_O(size),		cvt_parse },
	{"%f",  3, "vsize",		SHDR_AO(vsize),		FUNC_NULL },
	{"%f",  3, "v",			SHDR_AO(vsize),		FUNC_NULL },
	{"%f",  3, "delta",		SHDR_AO(delta),		FUNC_NULL },
	{"%f",  3, "d",			SHDR_AO(delta),		FUNC_NULL },
	
	{"",	0, (char *)0,		0,			FUNC_NULL }
};

HIDDEN int	Nfbm_setup(), Nfbm_render();
HIDDEN void	Nfbm_print(), Nfbm_free();

struct mfuncs Nfbm_mfuncs[] = {
	{"Nfbm",	0,	0,		MFI_NORMAL,		0,
	Nfbm_setup,	Nfbm_render,	Nfbm_print,	Nfbm_free },

	{(char *)0,	0,		0,		0,		0,
	0,		0,		0,		0 }
};


/*	N F B M _ S E T U P
 *
 *	This routine is called (at prep time)
 *	once for each region which uses this shader.
 *	Any shader-specific initialization should be done here.
 */
HIDDEN int
Nfbm_setup( rp, matparm, dpp, mfp, rtip)
register struct region	*rp;
struct rt_vls		*matparm;
char			**dpp;	/* pointer to reg_udata in *rp */
struct mfuncs		*mfp;
struct rt_i		*rtip;	/* New since 4.4 release */
{
	register struct Nfbm_specific	*Nfbm_sp;
	mat_t	model_to_region;
	mat_t	tmp;

	RT_CHECK_RTI(rtip);
	RT_VLS_CHECK( matparm );
	RT_CK_REGION(rp);
	GETSTRUCT( Nfbm_sp, Nfbm_specific );
	*dpp = (char *)Nfbm_sp;

	memcpy(Nfbm_sp, &Nfbm_defaults, sizeof(struct Nfbm_specific) );

	if( bu_struct_parse( matparm, Nfbm_parse, (char *)Nfbm_sp ) < 0 )
		return(-1);

	/* Optional:  get the matrix which maps model space into
	 *  "region" or "shader" space
	 */
	db_region_mat(model_to_region, rtip->rti_dbip, rp->reg_name);

	/* add the noise-space scaling */
	mat_idn(tmp);
	if (Nfbm_sp->size != 1.0) {
		tmp[0] = tmp[5] = tmp[10] = 1.0/Nfbm_sp->size;
	} else {
		tmp[0] = 1.0/Nfbm_sp->vsize[0];
		tmp[5] = 1.0/Nfbm_sp->vsize[1];
		tmp[10] = 1.0/Nfbm_sp->vsize[2];
	}

	mat_mul(Nfbm_sp->xform, tmp, model_to_region);

	/* Add any translation within shader/region space */
	mat_idn(tmp);
	tmp[MDX] = Nfbm_sp->delta[0];
	tmp[MDY] = Nfbm_sp->delta[1];
	tmp[MDZ] = Nfbm_sp->delta[2];
	mat_mul2(tmp, Nfbm_sp->xform);

	if( rdebug&RDEBUG_SHADE) {
		bu_struct_print( rp->reg_name, Nfbm_parse, (char *)Nfbm_sp );
		mat_print( "xform", Nfbm_sp->xform );
	}

	return(1);
}

/*
 *	N F B M _ P R I N T
 */
HIDDEN void
Nfbm_print( rp, dp )
register struct region *rp;
char	*dp;
{
	bu_struct_print( rp->reg_name, Nfbm_parse, (char *)dp );
}

/*
 *	N F B M _ F R E E
 */
HIDDEN void
Nfbm_free( cp )
char *cp;
{
	rt_free( cp, "Nfbm_specific" );
}

/*	N F B M _ R E N D E R
 *
 *	This is called (from viewshade() in shade.c)
 *	once for each hit point to be shaded.
 */
int
Nfbm_render( ap, pp, swp, dp )
struct application	*ap;
struct partition	*pp;
struct shadework	*swp;
char	*dp;
{
	register struct Nfbm_specific *Nfbm_sp =
		(struct Nfbm_specific *)dp;

	point_t pt;
	vect_t	v_noise;	/* noise vector for each octave */
	vect_t	freq_v;
	double	cos_angle;
	double	angle;
	quat_t	q;
	static CONST vect_t up = { 0.0, 0.0, 1.0 };
	vect_t	N;
	mat_t	mat;
	int	i;
	double	freq;
	point_t	p;

	RT_AP_CHECK(ap);
	RT_CHECK_PT(pp);
	CK_Nfbm_SP(Nfbm_sp);

	if( rdebug&RDEBUG_SHADE)
		bu_struct_print( "foo", Nfbm_parse, (char *)Nfbm_sp );


	/* transform hit point into "shader-space coordinates" */
	MAT4X3PNT(pt, Nfbm_sp->xform, swp->sw_hit.hit_point);


	/* Compute random vector from point */
	VSETALL(v_noise, 0.0);
	freq = 1.0;
	for (i=0 ; i < Nfbm_sp->octaves ; i++ ) {

#if 0
		noise_vec(pt, freq_v);
#else
		freq_v[X] = noise_perlin(pt);
		VSCALE(p, pt, Nfbm_sp->lacunarity);
		freq_v[Y] = noise_perlin(p);
		VSCALE(p, pt, Nfbm_sp->lacunarity*Nfbm_sp->lacunarity);
		freq_v[Z] = noise_perlin(p);
#endif
		VJOIN1(v_noise, v_noise, 1.0/freq, freq_v);
		VSCALE(pt, pt, Nfbm_sp->lacunarity);
		freq *= Nfbm_sp->lacunarity;
	}

	VUNITIZE(v_noise);
	cos_angle = VDOT(v_noise, up);
	angle = acos(cos_angle);
	
	angle *= Nfbm_sp->max_angle / 180.0;


	/* form quaternion to rotate Normal to new location */
	VCROSS(q, up, v_noise);

	QUAT_FROM_VROT_RAD(q, angle, q);
	quat_quat2mat(mat, q);
	VMOVE(N, swp->sw_hit.hit_normal);
	MAT4X3VEC(swp->sw_hit.hit_normal, mat, N);
	
	return(1);
}
#if 0

/* compute the footprint of the ray intersect point */
radius = ap->a_rbeam + ap->a_diverge * swp->sw_hit.hit_dist;


	/* Compute random vector from point */
	VSET(v_noise, 0.0, 0.0, 0.0);
	VSET(N, 0.0, 0.0, 0.0);
	freq = 1.0;
	for (i=0 ; i < Nfbm_sp->octaves ; i++ ) {
		noise_vec(pt, v_noise);
		VJOIN1(N, N, 1.0/freq, v_noise);
		VSCALE(pt, pt, Nfbm_sp->lacunarity);
		freq *= Nfbm_sp->lacunarity;
	}

	/* XXX This is bad */
	while ( MAGSQ(N) > .125) {
		VSCALE(N, N, 0.5);
	}

	VADD2(swp->sw_hit.hit_normal, swp->sw_hit.hit_normal, N);
	VUNITIZE(swp->sw_hit.hit_normal);

	new_cos_angle = VDOT(swp->sw_hit.hit_normal, ap->a_ray.r_dir) ;

	/* preserve the normal vs ray orientation */
	if (cos_angle <= 0.0 && new_cos_angle > 0.0 ||
	    cos_angle > 0.0 && new_cos_angle <= 0.0) {
		VREVERSE(swp->sw_hit.hit_normal, swp->sw_hit.hit_normal);
	}
#endif
