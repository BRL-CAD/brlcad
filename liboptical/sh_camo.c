/*
 *	S H _ C A M O . C
 *
 *	A camoflage shader
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

struct camo_specific {
	double	lacunarity;
	double	h_val;
	double	octaves;
	double	t1;
	double	t2;
	point_t	scale;	/* scale coordinate space */
	point_t c1;
	point_t c2;
	point_t c3;
	vect_t	delta;
	mat_t	xform;
};

static struct camo_specific camo_defaults = {
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

#define CAMO_NULL	((struct camo_specific *)0)
#define CAMO_O(m)	offsetof(struct camo_specific, m)
#define CAMO_AO(m)	offsetofarray(struct camo_specific, m)

struct structparse camo_parse[] = {
	{"%f",	1, "lacunarity",	CAMO_O(lacunarity),	FUNC_NULL },
	{"%f",	1, "H", 		CAMO_O(h_val),		FUNC_NULL },
	{"%f",	1, "octaves", 		CAMO_O(octaves),		FUNC_NULL },
	{"%f",	1, "t1",		CAMO_O(t1),		FUNC_NULL },
	{"%f",	1, "t2",		CAMO_O(t2),		FUNC_NULL },
	{"%f",  3, "scale",		CAMO_AO(scale),		FUNC_NULL },
	{"%f",  3, "c1",		CAMO_AO(c1),		FUNC_NULL },
	{"%f",  3, "c2",		CAMO_AO(c2),		FUNC_NULL },
	{"%f",  3, "c3",		CAMO_AO(c3),		FUNC_NULL },
	{"%f",  3, "delta",		CAMO_AO(delta),		FUNC_NULL },
	{"%f",	1, "l",			CAMO_O(lacunarity),	FUNC_NULL },
	{"%d",	1, "o", 		CAMO_O(octaves),		FUNC_NULL },
	{"%f",  3, "s",			CAMO_AO(scale),		FUNC_NULL },
	{"%f",  3, "d",			CAMO_AO(delta),		FUNC_NULL },

	{"",	0, (char *)0,		0,			FUNC_NULL }
};

HIDDEN int	camo_setup(), camo_render();
HIDDEN void	camo_print(), camo_free();

struct mfuncs camo_mfuncs[] = {
	{"camo",	0,		0,		MFI_NORMAL|MFI_HIT|MFI_UV,
	camo_setup,	camo_render,	camo_print,	camo_free },

	{(char *)0,	0,		0,		0,
	0,		0,		0,		0 }
};



/*
 *	C A M O _ S E T U P
 */
HIDDEN int
camo_setup( rp, matparm, dpp, mfp, rtip)
register struct region	*rp;
struct rt_vls		*matparm;
char			**dpp;	/* pointer to reg_udata in *rp */
struct mfuncs		*mfp;
struct rt_i		*rtip;
{
	register struct camo_specific *camo;
	struct db_full_path full_path;
	mat_t	region_to_model;
	mat_t	model_to_region;
	mat_t	tmp;

	RT_CHECK_RTI(rtip);
	RT_VLS_CHECK( matparm );
	RT_CK_REGION(rp);
	GETSTRUCT( camo, camo_specific );
	*dpp = (char *)camo;

	memcpy(camo, &camo_defaults, sizeof(struct camo_specific) );
	if( rdebug&RDEBUG_SHADE)
		rt_log("camo_setup\n");

	if( rt_structparse( matparm, camo_parse, (char *)camo ) < 0 )
		return(-1);

	if( rdebug&RDEBUG_SHADE)
		rt_structprint( rp->reg_name, camo_parse, (char *)camo );

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
	tmp[0] = camo->scale[0];
	tmp[5] = camo->scale[1];
	tmp[10] = camo->scale[2];

	mat_mul(camo->xform, tmp2, model_to_region);

	/* add the translation within noise space */
	mat_idn(tmp);
	tmp[MDX] = camo->delta[0];
	tmp[MDY] = camo->delta[1];
	tmp[MDZ] = camo->delta[2];
	mat_mul2(tmp, camo->xform);

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
	vect_t v_noise;
	point_t pt;
	double  val;

/*	pp->pt_inseg->seg_stp	/* struct soltab */
/*	pp->pt_regionp		/* region */

/*
 *	region rpp in ray-trace coordinates (changes as solid moves)
 *  rt_bound_tree(pp->pt_regionp->reg_treetop, reg_rpp_min, reg_rpp_max)
 */

	if( rdebug&RDEBUG_SHADE)
		rt_structprint( "foo", camo_parse, (char *)camo_sp );

	/* transform point into "noise-space coordinates" */
	MAT4X3PNT(pt, camo_sp->xform, swp->sw_hit.hit_point);

	val = noise_fbm(pt, camo_sp->h_val, camo_sp->lacunarity, camo_sp->octaves );

	if( rdebug&RDEBUG_SHADE)
		rt_log("camo_render: point (%g %g %g) %g\n\tRGB(%g %g %g) -> ",
			V3ARGS(swp->sw_hit.hit_point), val,
			V3ARGS(swp->sw_color));

	if (val < camo_sp->t1) {
		VMOVE(swp->sw_color, camo_sp->c1);
	} else if (val < camo_sp->t2 ) {
		VMOVE(swp->sw_color, camo_sp->c2);
	} else {
		VMOVE(swp->sw_color, camo_sp->c3);
	}

/*	swp->sw_color[1] += val * camo_sp->distortion;
	swp->sw_basecolor[1] += val * camo_sp->distortion; */

	if( rdebug&RDEBUG_SHADE)
		rt_log("RGB(%g %g %g)\n", V3ARGS(swp->sw_color));

	return(1);
}
