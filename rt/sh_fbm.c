/*
 *	F B M . C
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

struct fbm_specific {
	double	lacunarity;
	double	h_val;
	double	octaves;
	double	offset;
	double	gain;
	double	distortion;
	point_t	scale;	/* scale coordinate space */
};

static struct fbm_specific fbm_defaults = {
	2.1753974,	/* lacunarity */
	1.0,		/* h_val */
	4,		/* octaves */
	0.0,		/* offset */
	0.0,		/* gain */
	1.0,		/* distortion */
	{ 1.0, 1.0, 1.0 }	/* scale */
	};

#define FBM_NULL	((struct fbm_specific *)0)
#define FBM_O(m)	offsetof(struct fbm_specific, m)
#define FBM_AO(m)	offsetofarray(struct fbm_specific, m)

struct structparse fbm_parse[] = {
	{"%f",	1, "lacunarity",	FBM_O(lacunarity),	FUNC_NULL },
	{"%f",	1, "H", 		FBM_O(h_val),		FUNC_NULL },
	{"%f",	1, "octaves", 		FBM_O(octaves),		FUNC_NULL },
	{"%f",	1, "gain",		FBM_O(gain),		FUNC_NULL },
	{"%f",	1, "distortion",	FBM_O(distortion),	FUNC_NULL },
	{"%f",	1, "l",			FBM_O(lacunarity),	FUNC_NULL },
	{"%d",	1, "o", 		FBM_O(octaves),		FUNC_NULL },
	{"%f",	1, "g",			FBM_O(gain),		FUNC_NULL },
	{"%f",	1, "d",			FBM_O(distortion),	FUNC_NULL },
	{"%f",  3, "scale",		FBM_AO(scale),		FUNC_NULL },
	{"",	0, (char *)0,		0,			FUNC_NULL }
};

HIDDEN int	fbm_setup(), fbm_render();
HIDDEN void	fbm_print(), fbm_free();

struct mfuncs fbm_mfuncs[] = {
	{"bump_fbm",	0,		0,		MFI_NORMAL|MFI_HIT|MFI_UV,
	fbm_setup,	fbm_render,	fbm_print,	fbm_free },

	{(char *)0,	0,		0,		0,
	0,		0,		0,		0 }
};



/*
 *	F B M _ S E T U P
 */
HIDDEN int
fbm_setup( rp, matparm, dpp )
register struct region *rp;
struct rt_vls	*matparm;
char	**dpp;
{
	register struct fbm_specific *fbm;

	RT_VLS_CHECK( matparm );
	GETSTRUCT( fbm, fbm_specific );
	*dpp = (char *)fbm;

	memcpy(fbm, &fbm_defaults, sizeof(struct fbm_specific) );
	if( rdebug&RDEBUG_SHADE)
		rt_log("fbm_setup\n");

	if( rt_structparse( matparm, fbm_parse, (char *)fbm ) < 0 )
		return(-1);

	if( rdebug&RDEBUG_SHADE)
		rt_structprint( rp->reg_name, fbm_parse, (char *)fbm );

	return(1);
}

/*
 *	F B M _ P R I N T
 */
HIDDEN void
fbm_print( rp, dp )
register struct region *rp;
char	*dp;
{
	rt_structprint( rp->reg_name, fbm_parse, (char *)dp );
}

/*
 *	F B M _ F R E E
 */
HIDDEN void
fbm_free( cp )
char *cp;
{
	rt_free( cp, "fbm_specific" );
}

/*
 *	F B M _ R E N D E R
 */
int
fbm_render( ap, pp, swp, dp )
struct application	*ap;
struct partition	*pp;
struct shadework	*swp;
char	*dp;
{
	register struct fbm_specific *fbm_sp =
		(struct fbm_specific *)dp;
	vect_t v_noise;
	point_t pt;

	if( rdebug&RDEBUG_SHADE)
		rt_structprint( "foo", fbm_parse, (char *)fbm_sp );

	pt[0] = swp->sw_hit.hit_point[0] * fbm_sp->scale[0];
	pt[1] = swp->sw_hit.hit_point[1] * fbm_sp->scale[1];
	pt[2] = swp->sw_hit.hit_point[2] * fbm_sp->scale[2];

	noise_vec(pt, v_noise);

	VSCALE(v_noise, v_noise, fbm_sp->distortion);

	if( rdebug&RDEBUG_SHADE)
		rt_log("fbm_render: point (%g %g %g) becomes (%g %g %g)\n\tv_noise (%g %g %g)\n",
			V3ARGS(swp->sw_hit.hit_point),
			V3ARGS(pt),
			V3ARGS(v_noise));

	VADD2(swp->sw_hit.hit_normal, swp->sw_hit.hit_normal, v_noise);
	VUNITIZE(swp->sw_hit.hit_normal);

	return(1);
}
