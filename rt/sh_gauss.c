/*
 *	S H _ G A U S S . C
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


#define gauss_MAGIC 0x1864	/* GAUSS change this number for each shader */
struct gauss_specific {
	long	magic;
	vect_t	stddev;
	vect_t	delta;
	mat_t	xform;
};
#define CK_gauss_SP(_p) RT_CKMAG(_p, gauss_MAGIC, "gauss_specific")

static struct gauss_specific gauss_defaults = {
	gauss_MAGIC,
	{ 1.0, 1.0, 1.0 },
	{ 1.0, 1.0, 1.0 },
	};

#define SHDR_NULL	((struct gauss_specific *)0)
#define SHDR_O(m)	offsetof(struct gauss_specific, m)
#define SHDR_AO(m)	offsetofarray(struct gauss_specific, m)

struct structparse gauss_parse[] = {
	{"%f",  3, "delta",		SHDR_AO(delta),		FUNC_NULL },
	{"%f",  3, "d",			SHDR_AO(delta),		FUNC_NULL },
	{"%f",  3, "stddev",		SHDR_AO(stddev),	FUNC_NULL },
	{"%f",  3, "s",			SHDR_AO(stddev),	FUNC_NULL },
	{"",	0, (char *)0,		0,			FUNC_NULL }
};

HIDDEN int	gauss_setup(), gauss_render();
HIDDEN void	gauss_print(), gauss_free();

struct mfuncs gauss_mfuncs[] = {
	{"gauss",	0,	0,		MFI_NORMAL|MFI_HIT,	0,
	gauss_setup,	gauss_render,	gauss_print,	gauss_free },

	{(char *)0,	0,		0,		0,		0,
	0,		0,		0,		0 }
};


/*	G A U S S _ S E T U P
 *
 *	This routine is called (at prep time)
 *	once for each region which uses this shader.
 *	Any shader-specific initialization should be done here.
 */
HIDDEN int
gauss_setup( rp, matparm, dpp, mfp, rtip)
register struct region	*rp;
struct rt_vls		*matparm;
char			**dpp;	/* pointer to reg_udata in *rp */
struct mfuncs		*mfp;
struct rt_i		*rtip;	/* New since 4.4 release */
{
	register struct gauss_specific	*gauss_sp;
	mat_t	tmp;

	RT_CHECK_RTI(rtip);
	RT_VLS_CHECK( matparm );
	RT_CK_REGION(rp);
	GETSTRUCT( gauss_sp, gauss_specific );
	*dpp = (char *)gauss_sp;

	memcpy(gauss_sp, &gauss_defaults, sizeof(struct gauss_specific) );

	if( rt_structparse( matparm, gauss_parse, (char *)gauss_sp ) < 0 )
		return(-1);

	/* get the matrix which maps model space into
	 *  "region" or "shader" space
	 */
	db_region_mat(gauss_sp->xform, rtip->rti_dbip, rp->reg_name);

	/* Add any translation within shader/region space */
	mat_idn(tmp);
	tmp[MDX] = gauss_sp->delta[0];
	tmp[MDY] = gauss_sp->delta[1];
	tmp[MDZ] = gauss_sp->delta[2];
	mat_mul2(tmp, gauss_sp->xform);


	if( rdebug&RDEBUG_SHADE) {
		rt_structprint( rp->reg_name, gauss_parse, (char *)gauss_sp );
		mat_print( "xform", gauss->xform );
	}

	return(1);
}

/*
 *	G A U S S _ P R I N T
 */
HIDDEN void
gauss_print( rp, dp )
register struct region *rp;
char	*dp;
{
	rt_structprint( rp->reg_name, gauss_parse, (char *)dp );
}

/*
 *	G A U S S _ F R E E
 */
HIDDEN void
gauss_free( cp )
char *cp;
{
	rt_free( cp, "gauss_specific" );
}

/*
 *	G A U S S _ R E N D E R
 *
 *	This is called (from viewshade() in shade.c)
 *	once for each hit point to be shaded.
 */
int
gauss_render( ap, pp, swp, dp )
struct application	*ap;
struct partition	*pp;
struct shadework	*swp;
char	*dp;
{
	register struct gauss_specific *gauss_sp =
		(struct gauss_specific *)dp;
	point_t pt;

	RT_AP_CHECK(ap);
	RT_CHECK_PT(pp);
	CK_gauss_SP(gauss_sp);

	if( rdebug&RDEBUG_SHADE)
		rt_structprint( "foo", gauss_parse, (char *)gauss_sp );


	/* compute the ray/solid in and out points,
	 * and transform them into "shader space" coordinates 
	 */
	VJOIN1(pt, ap->a_ray.r_pt, pp->pt_inhit->hit_dist, ap->a_ray.r_dir);
	MAT4X3PNT(in_pt, sgauss_sp->xform, pt);

	VJOIN1(pt, ap->a_ray.r_pt, pp->pt_outhit->hit_dist, ap->a_ray.r_dir);
	MAT4X3PNT(out_pt, sgauss_sp->xform, pt);

	/* get ray/solid intersection vector (in noise space)
	 * and compute thickness of solid (in noise space) along ray path
	 */
	VSUB2(v_cloud, out_pt, in_pt);
	thickness = MAGNITUDE(v_cloud);

	steps = pow(gauss_sp->lacunarity, gauss_sp->octaves-1) * 4;
	step_delta = thickness / (double)steps;


	VUNITIZE(v_cloud);
	VMOVE(pt, in_pt);
	trans = 1.0;
	for (i=0 ; i < steps ; i++ ) {
		/* compute the next point in the gaus space */
		VJOIN1(pt, in_pt, i*step_delta, v_cloud);

		pt[X] /= gauss_sp->stddev[X];
		pt[Y] /= gauss_sp->stddev[Y];
		pt[Z] /= gauss_sp->stddev[Z];

		/* density has normal distribution
		 *	1 / sqrt(2 * PI) * pow (- n*n/ 2.0)
		 */
		den  = 0.39894228 * pow( - pt[X] * pt[X] * .5 );
		den *= 0.39894228 * pow( - pt[Y] * pt[Y] * .5 );
		den *= 0.39894228 * pow( - pt[Z] * pt[Z] * .5 );

	}




	/* caller will perform transmission/reflection calculations
	 * based upon the values of swp->sw_transmit and swp->sw_reflect
	 *
	 * 0 < swp->sw_transmit <= 1 causes transmission computations
	 * 0 < swp->sw_reflect <= 1 causes reflection computations
	 */

	return(1);
}
