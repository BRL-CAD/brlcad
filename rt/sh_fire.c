/*
 *	S H _ F I R E . C
 *
 *	A 3D "solid" cloud shader
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
#define M_PI            3.14159265358979323846

#define CLAMP(_x,_a,_b)	(_x < _a ? _a : (_x > _b ? _b : _x))
#define FLOOR(x)	(  (int)(x) - (  (x) < 0 && (x) != (int)(x)  )  )
#define CEIL(x)		(  (int)(x) + (  (x) > 0 && (x) != (int)(x)  )  )
#define SMOOTHSTEP(x)   (  (x) * (x) * (3 - 2*(x))  )

struct fire_specific {
	double	lacunarity;
	double	h_val;
	double	octaves;
	double	scale;	/* scale coordinate space */
	vect_t	delta;	/* xlatd in noise space (where interesting noise is)*/
	int	exp_use;
	double	flame_height;
	mat_t	xform;
};

static struct fire_specific fire_defaults = {
	2.1753974,	/* lacunarity */
	1.0,		/* h_val */
	4.0,		/* octaves */
	1.0,		/* scale */
	{ 0.0, 0.0, 0.0 },	/* delta */
	0.0,			/* exp_use */
	500.0		/* flame height */
	};

#define SHDR_NULL	((struct fire_specific *)0)
#define SHDR_O(m)	offsetof(struct fire_specific, m)
#define SHDR_AO(m)	offsetofarray(struct fire_specific, m)

struct structparse fire_pr[] = {
	{"%f",	1, "lacunarity",	SHDR_O(lacunarity),	FUNC_NULL },
	{"%f",	1, "H", 		SHDR_O(h_val),	FUNC_NULL },
	{"%f",	1, "octaves", 		SHDR_O(octaves),	FUNC_NULL },
	{"%f",  1, "scale",		SHDR_O(scale),	FUNC_NULL },
	{"%f",  1, "height",		SHDR_O(flame_height),	FUNC_NULL },
	{"%f",  3, "delta",		SHDR_AO(delta),	FUNC_NULL },
	{"%d",  1, "exp_use",		SHDR_O(exp_use),	FUNC_NULL },
	{"",	0, (char *)0,		0,			FUNC_NULL }
};
struct structparse fire_parse[] = {
	{"%f",	1, "lacunarity",	SHDR_O(lacunarity),	FUNC_NULL },
	{"%f",	1, "H", 		SHDR_O(h_val),	FUNC_NULL },
	{"%f",	1, "octaves", 		SHDR_O(octaves),	FUNC_NULL },
	{"%f",  1, "scale",		SHDR_O(scale),	FUNC_NULL },
	{"%f",  3, "delta",		SHDR_AO(delta),	FUNC_NULL },
	{"%d",  1, "exp_use",		SHDR_O(exp_use),	FUNC_NULL },
	{"%f",  1, "height",		SHDR_O(flame_height),	FUNC_NULL },
	{"%f",	1, "l",			SHDR_O(lacunarity),	FUNC_NULL },
	{"%f",	1, "o", 		SHDR_O(octaves),	FUNC_NULL },
	{"%f",  1, "s",			SHDR_O(scale),	FUNC_NULL },
	{"%f",  3, "d",			SHDR_AO(delta),	FUNC_NULL },
	{"%d",  1, "e",			SHDR_O(exp_use),	FUNC_NULL },
	{"%f",  1, "h",			SHDR_O(flame_height),	FUNC_NULL },
	{"",	0, (char *)0,		0,			FUNC_NULL }
};

HIDDEN int	fire_setup(), fire_render();
HIDDEN void	fire_print(), fire_free();

struct mfuncs fire_mfuncs[] = {
	{"fire",	0,		0,	MFI_NORMAL|MFI_HIT|MFI_UV, 0,
	fire_setup,	fire_render,	fire_print,	fire_free },

	{(char *)0,	0,		0,		0,
	0,		0,		0,		0 }
};



/*
 *	F I R E _ S E T U P
 */
HIDDEN int
fire_setup( rp, matparm, dpp, mfp, rtip )
register struct region *rp;
struct rt_vls	*matparm;
char	**dpp;	/* pointer to reg_udata in *rp */
struct mfuncs		*mfp;
struct rt_i		*rtip;
{
	register struct fire_specific *fire;
	struct db_full_path full_path;
	mat_t	region_to_model;
	mat_t	model_to_region;
	mat_t	tmp;

	RT_VLS_CHECK( matparm );
	GETSTRUCT( fire, fire_specific );
	*dpp = (char *)fire;

	memcpy(fire, &fire_defaults, sizeof(struct fire_specific) );
	if( rdebug&RDEBUG_SHADE)
		rt_log("fire_setup\n");

	if( rt_structparse( matparm, fire_parse, (char *)fire ) < 0 )
		return(-1);

	if( rdebug&RDEBUG_SHADE)
		rt_structprint( rp->reg_name, fire_parse, (char *)fire );

	/* get transformation between world and "region" coordinates */
	if (db_string_to_path( &full_path, rtip->rti_dbip, rp->reg_name) ) {
		/* bad thing */
		rt_bomb("db_string_to_path() error");
	}
	if(! db_path_to_mat(rtip->rti_dbip, &full_path, region_to_model, 0)) {
		/* bad thing */
		rt_bomb("db_path_to_mat() error");
	}

	/* get matrix to map points from model (world) space
	 * to "region" space
	 */
	mat_inv(model_to_region, region_to_model);


	/* add the noise-space scaling */
	mat_idn(tmp);
	tmp[0] = 1. / fire->scale;
	tmp[5] = 1. / fire->scale;
	tmp[10] =  1. / fire->scale;

	mat_mul(fire->xform, tmp, model_to_region);

	/* add the translation within noise space */
	mat_idn(tmp);
	tmp[MDX] = fire->delta[0];
	tmp[MDY] = fire->delta[1];
	tmp[MDZ] = fire->delta[2];
	mat_mul2(tmp, fire->xform);

	return(1);
}

/*
 *	F I R E _ P R I N T
 */
HIDDEN void
fire_print( rp, dp )
register struct region *rp;
char	*dp;
{
	rt_structprint( rp->reg_name, fire_pr, (char *)dp );
}

/*
 *	F I R E _ F R E E
 */
HIDDEN void
fire_free( cp )
char *cp;
{
	rt_free( cp, "fire_specific" );
}

/*
 *	F I R E _ R E N D E R
 */
int
fire_render( ap, pp, swp, dp )
struct application	*ap;
struct partition	*pp;
struct shadework	*swp;
char	*dp;
{
	register struct fire_specific *fire_sp =
		(struct fire_specific *)dp;
	point_t in_pt;	/* point where ray enters fire solid */
	point_t out_pt; /* point where ray leaves fire solid */
	point_t pt;
	vect_t	v_cloud;/* vector representing ray/solid intersection */
	double	thickness; /* magnitude of v_cloud (distance through solid) */
	int	steps;	   /* # of samples along ray/solid intersection */
	double	step_delta;/* distance between sample points */
	int	i;
	double  val;
	double	trans;
	double	density;

	RT_CHECK_PT(pp);
	RT_AP_CHECK(ap);
	RT_CK_REGION(pp->pt_regionp);

	/* compute the ray/solid in and out points,
	 * and transform them into "shader space" coordinates 
	 */
	VJOIN1(pt, ap->a_ray.r_pt, pp->pt_inhit->hit_dist, ap->a_ray.r_dir);
	MAT4X3PNT(in_pt, fire_sp->xform, pt);

	VJOIN1(pt, ap->a_ray.r_pt, pp->pt_outhit->hit_dist, ap->a_ray.r_dir);
	MAT4X3PNT(out_pt, fire_sp->xform, pt);

	if (fire_sp->exp_use) {
		/* map flame height into the range 0 <= fh <= 10
		 * map that exponentially into range 0 < fh < 1
		 */
		in_pt[Z] =  1 - exp( - in_pt[Z] / fire_sp->flame_height);
		out_pt[Z] = 1 - exp( -out_pt[Z] / fire_sp->flame_height);
	}

	/* get ray/solid intersection vector (in noise space)
	 * and compute thickness of solid (in noise space) along ray path
	 */
	VSUB2(v_cloud, out_pt, in_pt);
	thickness = MAGNITUDE(v_cloud);


	/* The noise field used by the noise_turb and noise_fbm routines
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

	steps = pow(fire_sp->lacunarity, fire_sp->octaves-1) * 4;
	step_delta = thickness / (double)steps;

	VUNITIZE(v_cloud);
	VMOVE(pt, in_pt);
	trans = 1.0;
	for (i=0 ; i < steps ; i++ ) {
		/* compute the next point in the cloud space */
		VJOIN1(pt, in_pt, i*step_delta, v_cloud);

		val = noise_turb(pt, fire_sp->h_val,
			fire_sp->lacunarity, fire_sp->octaves );

		val *= 2.;
		val = CLAMP(val, 0.0, 1.0);
		val = 1 - val;

		/* trans *= exp( - density_per_mm * dist )
		 *
		 * density_per_mm
		 *	near zero when Z close to 1
		 *	near total opacity when Z = 0
		 */

		density = val * (1 - SMOOTHSTEP(pt[Z])) * 5.0;

		trans *=  exp( - density * step_delta);
	}
	
	swp->sw_transmit = trans;
/*	VSET(swp->sw_color, 0.0, 0.0, 0.0); */
	VSET(swp->sw_basecolor, 1.0, 1.0, 1.0);

	return(1);
}
