/*
 *	S H _ C A M O . C
 *
 *	A shader to apply a crude camoflage color pattern to an object
 *	using a fractional Brownian motion of 3 colors
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

#define camo_MAGIC 0x18364	/* XXX change this number for each shader */
struct camo_specific {
	long	magic;
	double	lacunarity;
	double	h_val;
	double	octaves;
	double	t1;	/* color threshold 1 */
	double	t2;	/* color threshold 2 */
	point_t	scale;	/* scale of noise coordinate space */
	point_t c1;	/* color 1 */
	point_t c2;	/* color 2 */
	point_t c3;	/* color 3 */
	vect_t	delta;	/* a delta in noise space to be applied to pts */
	mat_t	xform;	/* model->region coord sys matrix */
};
#define CK_camo_SP(_p) RT_CKMAG(_p, camo_MAGIC, "camo_specific")

static struct camo_specific camo_defaults = {
	camo_MAGIC,
	2.1753974,	/* lacunarity */
	1.0,		/* h_val */
	4.0,		/* octaves */
	-0.5,		/* t1 */
	0.5,		/* t2 */
	{ 1.0, 1.0, 1.0 },	/* scale */
	{ .38, .29, .16 },	/* darker color c1 */
	{ .125, .35, .04 },	/* basic color c2 */
	{ .815, .635, .35 },	/* brighter color c3 */
	{ 1000.0, 1000.0, 1000.0 }	/* delta into noise space */
	};

#define SHDR_NULL	((struct camo_specific *)0)
#define SHDR_O(m)	offsetof(struct camo_specific, m)
#define SHDR_AO(m)	offsetofarray(struct camo_specific, m)

struct structparse camo_parse[] = {
	{"%f",	1, "lacunarity",	SHDR_O(lacunarity),	FUNC_NULL },
	{"%f",	1, "l",			SHDR_O(lacunarity),	FUNC_NULL },
	{"%f",	1, "H", 		SHDR_O(h_val),		FUNC_NULL },
	{"%f",	1, "octaves", 		SHDR_O(octaves),	FUNC_NULL },
	{"%f",	1, "o", 		SHDR_O(octaves),	FUNC_NULL },
	{"%f",	1, "t1",		SHDR_O(t1),		FUNC_NULL },
	{"%f",	1, "t2",		SHDR_O(t2),		FUNC_NULL },
	{"%f",  3, "scale",		SHDR_AO(scale),		FUNC_NULL },
	{"%f",  3, "s",			SHDR_AO(scale),		FUNC_NULL },
	{"%f",  3, "c1",		SHDR_AO(c1),		FUNC_NULL },
	{"%f",  3, "c2",		SHDR_AO(c2),		FUNC_NULL },
	{"%f",  3, "c3",		SHDR_AO(c3),		FUNC_NULL },
	{"%f",  3, "delta",		SHDR_AO(delta),		FUNC_NULL },
	{"%f",  3, "d",			SHDR_AO(delta),		FUNC_NULL },
	{"",	0, (char *)0,		0,			FUNC_NULL }
};

HIDDEN int	camo_setup(), camo_render();
HIDDEN void	camo_print(), camo_free();

struct mfuncs camo_mfuncs[] = {
	{"camo",	0,		0,		MFI_HIT,	0,
	camo_setup,	camo_render,	camo_print,	camo_free },

	{(char *)0,	0,		0,		0,		0,
	0,		0,		0,		0 }
};


/*	C A M O _ S E T U P
 *
 *	This routine is called (at prep time)
 *	once for each region which uses this shader.
 *	Any shader-specific initialization should be done here.
 */
HIDDEN int
camo_setup( rp, matparm, dpp, mfp, rtip)
register struct region	*rp;
struct rt_vls		*matparm;
char			**dpp;	/* pointer to reg_udata in *rp */
struct mfuncs		*mfp;
struct rt_i		*rtip;	/* New since 4.4 release */
{
	register struct camo_specific	*camo_sp;
	mat_t	model_to_region;
	mat_t	tmp;

	RT_CHECK_RTI(rtip);
	RT_VLS_CHECK( matparm );
	RT_CK_REGION(rp);
	GETSTRUCT( camo_sp, camo_specific );
	*dpp = (char *)camo_sp;

	memcpy(camo_sp, &camo_defaults, sizeof(struct camo_specific) );

	if( rt_structparse( matparm, camo_parse, (char *)camo_sp ) < 0 )
		return(-1);

	/* Optional:  get the matrix which maps model space into
	 *  "region" or "shader" space
	 */
	db_region_mat(model_to_region, rtip->rti_dbip, rp->reg_name);

	/* add the noise-space scaling */
	mat_idn(tmp);
	tmp[0] = camo_sp->scale[0];
	tmp[5] = camo_sp->scale[1];
	tmp[10] = camo_sp->scale[2];

	mat_mul(camo_sp->xform, tmp, model_to_region);

	/* Add any translation within shader/region space */
	mat_idn(tmp);
	tmp[MDX] = camo_sp->delta[0];
	tmp[MDY] = camo_sp->delta[1];
	tmp[MDZ] = camo_sp->delta[2];
	mat_mul2(tmp, camo_sp->xform);

	if( rdebug&RDEBUG_SHADE) {
		rt_structprint( rp->reg_name, camo_parse, (char *)camo_sp );
		mat_print( "xform", camo_sp->xform );
	}

	return(1);
}

/*
 *	C A M O _ P R I N T
 */
HIDDEN void
camo_print( rp, dp )
register struct region *rp;
char	*dp;
{
	rt_structprint( rp->reg_name, camo_parse, (char *)dp );
}

/*
 *	C A M O _ F R E E
 */
HIDDEN void
camo_free( cp )
char *cp;
{
	rt_free( cp, "camo_specific" );
}

/*
 *	C A M O _ R E N D E R
 *
 *	This is called (from viewshade() in shade.c)
 *	once for each hit point to be shaded.
 */
int
camo_render( ap, pp, swp, dp )
struct application	*ap;
struct partition	*pp;
struct shadework	*swp;
char	*dp;
{
	register struct camo_specific *camo_sp =
		(struct camo_specific *)dp;
	point_t pt;
	double val;

	RT_AP_CHECK(ap);
	RT_CHECK_PT(pp);
	CK_camo_SP(camo_sp);

	if( rdebug&RDEBUG_SHADE)
		rt_structprint( "foo", camo_parse, (char *)camo_sp );

	/* Optional: transform hit point into "shader-space coordinates" */
	MAT4X3PNT(pt, camo_sp->xform, swp->sw_hit.hit_point);


	/* noise_fbm returns a value in the approximate range of
	 *	-1.0 ~<= noise_fbm() ~<= 1.0
	 */
	val = noise_fbm(pt, camo_sp->h_val,
		camo_sp->lacunarity, camo_sp->octaves );

	if (val < camo_sp->t1) {
		VMOVE(swp->sw_color, camo_sp->c1);
	} else if (val < camo_sp->t2 ) {
		VMOVE(swp->sw_color, camo_sp->c2);
	} else {
		VMOVE(swp->sw_color, camo_sp->c3);
	}

	return(1);
}
