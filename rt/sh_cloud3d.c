/*
 *	S H _ C L O U D 3 D . C
 *
 *	A 3D cloud shader
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

struct cloud3d_specific {
	double	lacunarity;
	double	h_val;
	double	octaves;
	double	thresh;
	vect_t	delta;
	point_t	scale;	/* scale coordinate space */
	mat_t	xform;
};

static struct cloud3d_specific cloud3d_defaults = {
	2.1753974,	/* lacunarity */
	1.0,		/* h_val */
	4.0,		/* octaves */
	1.0,		/* threshold for opacity */
	{ 0.0, 0.0, 0.0 },	/* delta */
	{ 1.0, 1.0, 1.0 },	/* scale */
	};

#define CLOUD3D_NULL	((struct cloud3d_specific *)0)
#define CLOUD3D_O(m)	offsetof(struct cloud3d_specific, m)
#define CLOUD3D_AO(m)	offsetofarray(struct cloud3d_specific, m)

struct structparse cloud3d_pr[] = {
	{"%f",	1, "lacunarity",	CLOUD3D_O(lacunarity),	FUNC_NULL },
	{"%f",	1, "H", 		CLOUD3D_O(h_val),	FUNC_NULL },
	{"%f",	1, "octaves", 		CLOUD3D_O(octaves),	FUNC_NULL },
	{"%f",	1, "thresh",		CLOUD3D_O(thresh),	FUNC_NULL },
	{"%f",  3, "scale",		CLOUD3D_AO(scale),	FUNC_NULL },
	{"%f",  3, "delta",		CLOUD3D_AO(delta),	FUNC_NULL },
	{"",	0, (char *)0,		0,			FUNC_NULL }
};
struct structparse cloud3d_parse[] = {
	{"%f",	1, "lacunarity",	CLOUD3D_O(lacunarity),	FUNC_NULL },
	{"%f",	1, "H", 		CLOUD3D_O(h_val),	FUNC_NULL },
	{"%f",	1, "octaves", 		CLOUD3D_O(octaves),	FUNC_NULL },
	{"%f",	1, "thresh",		CLOUD3D_O(thresh),	FUNC_NULL },
	{"%f",  3, "scale",		CLOUD3D_AO(scale),	FUNC_NULL },
	{"%f",  3, "delta",		CLOUD3D_AO(delta),	FUNC_NULL },
	{"%f",	1, "l",			CLOUD3D_O(lacunarity),	FUNC_NULL },
	{"%f",	1, "o", 		CLOUD3D_O(octaves),	FUNC_NULL },
	{"%f",	1, "t",			CLOUD3D_O(thresh),	FUNC_NULL },
	{"%f",  3, "s",			CLOUD3D_AO(scale),	FUNC_NULL },
	{"%f",  3, "d",			CLOUD3D_AO(delta),	FUNC_NULL },
	{"",	0, (char *)0,		0,			FUNC_NULL }
};

HIDDEN int	cloud3d_setup(), cloud3d_render();
HIDDEN void	cloud3d_print(), cloud3d_free();

struct mfuncs cloud3d_mfuncs[] = {
	{"cloud3d",	0,		0,		MFI_NORMAL|MFI_HIT|MFI_UV,
	cloud3d_setup,	cloud3d_render,	cloud3d_print,	cloud3d_free },

	{(char *)0,	0,		0,		0,
	0,		0,		0,		0 }
};



/*
 *	C L O U D 3 D _ S E T U P
 */
HIDDEN int
cloud3d_setup( rp, matparm, dpp, mfp, rtip )
register struct region *rp;
struct rt_vls	*matparm;
char	**dpp;	/* pointer to reg_udata in *rp */
struct mfuncs		*mfp;
struct rt_i		*rtip;
{
	register struct cloud3d_specific *cloud3d;
	struct db_full_path full_path;
	mat_t	region_to_model;
	mat_t	model_to_region;
	mat_t	tmp;

	RT_VLS_CHECK( matparm );
	GETSTRUCT( cloud3d, cloud3d_specific );
	*dpp = (char *)cloud3d;

	memcpy(cloud3d, &cloud3d_defaults, sizeof(struct cloud3d_specific) );
	if( rdebug&RDEBUG_SHADE)
		rt_log("cloud3d_setup\n");

	if( rt_structparse( matparm, cloud3d_parse, (char *)cloud3d ) < 0 )
		return(-1);

	if( rdebug&RDEBUG_SHADE)
		rt_structprint( rp->reg_name, cloud3d_parse, (char *)cloud3d );

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
	tmp[0] = 1. / cloud3d->scale[0];
	tmp[5] = 1. / cloud3d->scale[1];
	tmp[10] =  1. / cloud3d->scale[2];

	mat_mul(cloud3d->xform, tmp, model_to_region);

	/* add the translation within noise space */
	mat_idn(tmp);
	tmp[MDX] = cloud3d->delta[0];
	tmp[MDY] = cloud3d->delta[1];
	tmp[MDZ] = cloud3d->delta[2];
	mat_mul2(tmp, cloud3d->xform);

	return(1);
}

/*
 *	C L O U D 3 D _ P R I N T
 */
HIDDEN void
cloud3d_print( rp, dp )
register struct region *rp;
char	*dp;
{
	rt_structprint( rp->reg_name, cloud3d_pr, (char *)dp );
}

/*
 *	C L O U D 3 D _ F R E E
 */
HIDDEN void
cloud3d_free( cp )
char *cp;
{
	rt_free( cp, "cloud3d_specific" );
}

/*
 *	C L O U D 3 D _ R E N D E R
 */
int
cloud3d_render( ap, pp, swp, dp )
struct application	*ap;
struct partition	*pp;
struct shadework	*swp;
char	*dp;
{
	register struct cloud3d_specific *cloud3d_sp =
		(struct cloud3d_specific *)dp;
	point_t in_pt, out_pt, pt;
	vect_t	v_cloud;
	double	thickness;
	int	steps;
	double	step_delta;
	int	i;
	double  val;
	double	transmission;

	RT_CHECK_PT(pp);
	RT_AP_CHECK(ap);
	RT_CK_REGION(pp->pt_regionp);

#if 0
	MAT4X3PNT(in_pt, cloud3d_sp->xform, swp->sw_hit.hit_point);
		val = noise_fbm(in_pt, cloud3d_sp->h_val, 
			cloud3d_sp->lacunarity, cloud3d_sp->octaves );
 	VSET(swp->sw_color, val, val, val);
	return 1;
#endif
	VJOIN1(in_pt, ap->a_ray.r_pt, pp->pt_inhit->hit_dist, ap->a_ray.r_dir);
	VJOIN1(out_pt, ap->a_ray.r_pt, pp->pt_outhit->hit_dist, ap->a_ray.r_dir);

	/* transform point into "noise-space coordinates" */
	MAT4X3PNT(in_pt, cloud3d_sp->xform, in_pt);
	MAT4X3PNT(out_pt, cloud3d_sp->xform, out_pt);

	VSUB2(v_cloud, out_pt, in_pt);
	thickness = MAGNITUDE(v_cloud);
	steps = (int)(thickness * 3.0);

	step_delta = thickness / (double)steps;

	VUNITIZE(v_cloud);

	VMOVE(pt, in_pt);
	transmission = 1.0;
	for (i=0 ; i < steps ; i++ ) {
		/* compute the next point in the cloud space */
		VJOIN1(pt, in_pt, i*step_delta, v_cloud);

		val = noise_fbm(pt, cloud3d_sp->h_val, 
			cloud3d_sp->lacunarity, cloud3d_sp->octaves );

		val = 1.0 - fabs(val*cloud3d_sp->thresh*.5 + .5);

		transmission *= val;
		if (transmission < 0.000001) {
			transmission = 0.;
			break;
		}
	}

	transmission = CLAMP(transmission, 0.0, 1.0);

	swp->sw_transmit = transmission;

/* 	VSET(swp->sw_color, transmission, transmission, transmission); */

	return(1);
}
