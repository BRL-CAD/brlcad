/*
 *	S H _ A I R . C
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


#define air_MAGIC 0x41697200	/* "Air" */
struct air_specific {
	long	magic;
	double	d_p_mm;	/* density per unit mm */
	double	B;
	double	color[3];
};
#define CK_air_SP(_p) RT_CKMAG(_p, air_MAGIC, "air_specific")

static struct air_specific air_defaults = {
	air_MAGIC,
	1e-7,		/* d_p_pp */	
	1.0,
	{ .25, .25, .8 }
	};

#define SHDR_NULL	((struct air_specific *)0)
#define SHDR_O(m)	offsetof(struct air_specific, m)
#define SHDR_AO(m)	offsetofarray(struct air_specific, m)

struct structparse air_parse[] = {
	{"%f",  1, "dpmm",		SHDR_O(d_p_mm),		FUNC_NULL },
	{"%f",  1, "B",			SHDR_O(B),		FUNC_NULL },
	{"",	0, (char *)0,		0,			FUNC_NULL }
};

HIDDEN int	air_setup(), air_render(), fog_render(), emist_render();
HIDDEN void	air_print(), air_free();

struct mfuncs air_mfuncs[] = {
	{"air",	0,	0,		MFI_NORMAL|MFI_HIT|MFI_UV,
	air_setup,	air_render,	air_print,	air_free },

	{"fog",	0,	0,		MFI_NORMAL|MFI_HIT|MFI_UV,
	air_setup,	fog_render,	air_print,	air_free },

	{"emist",0,	0,		MFI_NORMAL|MFI_HIT|MFI_UV,
	air_setup,	emist_render,	air_print,	air_free },

	{(char *)0,	0,		0,		0,
	0,		0,		0,		0 }
};


/*	A I R _ S E T U P
 *
 *	This routine is called (at prep time)
 *	once for each region which uses this shader.
 *	Any shader-specific initialization should be done here.
 */
HIDDEN int
air_setup( rp, matparm, dpp, mfp, rtip)
register struct region	*rp;
struct rt_vls		*matparm;
char			**dpp;	/* pointer to reg_udata in *rp */
struct mfuncs		*mfp;
struct rt_i		*rtip;	/* New since 4.4 release */
{
	register struct air_specific	*air_sp;
	mat_t	tmp;

	if( rdebug&RDEBUG_SHADE) rt_log("air_setup\n");

	RT_CHECK_RTI(rtip);
	RT_VLS_CHECK( matparm );
	RT_CK_REGION(rp);
	GETSTRUCT( air_sp, air_specific );
	*dpp = (char *)air_sp;

	memcpy(air_sp, &air_defaults, sizeof(struct air_specific) );

#if 0
	rp->reg_regionid = -1;
	rp->reg_aircode = 1;
#endif

	rt_log("\"%s\"\n", RT_VLS_ADDR(matparm) );
	if( rt_structparse( matparm, air_parse, (char *)air_sp ) < 0 )
		return(-1);

	return(1);
}

/*
 *	A I R _ P R I N T
 */
HIDDEN void
air_print( rp, dp )
register struct region *rp;
char	*dp;
{
	rt_structprint( rp->reg_name, air_parse, (char *)dp );
}

/*
 *	A I R _ F R E E
 */
HIDDEN void
air_free( cp )
char *cp;
{
	if( rdebug&RDEBUG_SHADE)
		rt_log("air_free(%s:%d)\n", __FILE__, __LINE__);
	rt_free( cp, "air_specific" );
}

/*
 *	A I R _ R E N D E R
 *
 *	This is called (from viewshade() in shade.c)
 *	once for each hit point to be shaded.
 */
int
air_render( ap, pp, swp, dp )
struct application	*ap;
struct partition	*pp;
struct shadework	*swp;
char	*dp;
{
	register struct air_specific *air_sp =
		(struct air_specific *)dp;
	point_t pt;

	RT_AP_CHECK(ap);
	RT_CHECK_PT(pp);
	CK_air_SP(air_sp);

	if( rdebug&RDEBUG_SHADE) {
		rt_structprint( "air_specific", air_parse, (char *)air_sp );

		rt_log("air in(%g) out%g)\n",
			pp->pt_inhit->hit_dist,
			pp->pt_outhit->hit_dist);
	}

	return(1);
}
/*
 *	F O G _ R E N D E R
 *
 *	This is called (from viewshade() in shade.c)
 *	once for each hit point to be shaded.
 */
int
fog_render( ap, pp, swp, dp )
struct application	*ap;
struct partition	*pp;
struct shadework	*swp;
char	*dp;
{
	register struct air_specific *air_sp =
		(struct air_specific *)dp;
	point_t pt;
	double tau;
	double dist;

	RT_AP_CHECK(ap);
	RT_CHECK_PT(pp);
	CK_air_SP(air_sp);

	if( rdebug&RDEBUG_SHADE) {
		rt_structprint( "fog_specific", air_parse, (char *)air_sp );
		rt_log("fog in(%g) out(%g) r_pt(%g %g %g)\n",
			pp->pt_inhit->hit_dist,
			pp->pt_outhit->hit_dist,
			V3ARGS(ap->a_ray.r_pt));
	}

	/* We can't trust the reflect/refract support in rt to do the right
	 * thing so we have to take over here.
	 */
#if 0
	swp->sw_transmit = 1.0;
	return(1);
#endif	

	/* Beer's Law Homogeneous Fog */

	/* tau = optical path depth = density per mm * distance */
	if (pp->pt_inhit->hit_dist < 0.0) dist = pp->pt_outhit->hit_dist;
	else dist = (pp->pt_outhit->hit_dist - pp->pt_inhit->hit_dist);

	tau = air_sp->d_p_mm * dist;

	/* transmission = e^(-tau) */
	swp->sw_transmit = exp(-tau);

	if ( swp->sw_transmit > 1.0) swp->sw_transmit = 1.0;
	else if ( swp->sw_transmit < 0.0 ) swp->sw_transmit = 0.0;

	/* extinction = 1. - transmission.  Extinguished part replaced by
	 * color of the air
	 */
	VMOVE(swp->sw_color, air_sp->color);
	VMOVE(swp->sw_basecolor, air_sp->color);

	if( rdebug&RDEBUG_SHADE)
		rt_log("fog o dist:%gmm tau:%g transmit:%g color(%g %g %g)\n",
			dist, tau, swp->sw_transmit, V3ARGS(swp->sw_color) );

	return(1);
}



/*
 *	E M I S T _ R E N D E R
 *
 *	This is called (from viewshade() in shade.c)
 *	once for each hit point to be shaded.
 */
int
emist_render( ap, pp, swp, dp )
struct application	*ap;
struct partition	*pp;
struct shadework	*swp;
char	*dp;
{
	register struct air_specific *air_sp =
		(struct air_specific *)dp;
	point_t pt;
	double tau;
	double Zo, Ze, Zd, te;

	RT_AP_CHECK(ap);
	RT_CHECK_PT(pp);
	CK_air_SP(air_sp);

	if( rdebug&RDEBUG_SHADE) {
		rt_structprint( "emist_specific", air_parse, (char *)air_sp );

		rt_log("air in(%g) out%g)\n",
			pp->pt_inhit->hit_dist,
			pp->pt_outhit->hit_dist);
	}

	/* Exponential Mist */

	/* d_p_mm = overall fog density
	 * B = density falloff with altitude
	 * Zo = elevation at ray start
	 * Ze = elevation at ray end
	 * Zd = Z component of normalized ray vector 
	 * te = dist from pt to end of ray (out hit point)
	 */
	Zd = ap->a_ray.r_dir[Z];
	Zo = pp->pt_inhit->hit_point[Z];
	Ze = pp->pt_outhit->hit_point[Z];
	te = pp->pt_outhit->hit_dist;

	if (Zd < SQRT_SMALL_FASTF)
		tau = air_sp->d_p_mm * te * exp( -air_sp->B * Zo);
	else
		tau = ((air_sp->d_p_mm * te) / (air_sp->B * Zd)) * 
			(exp( -(Zo+Zd*te) ) - exp(-Zo) );

	/* extinction = 1. - transmission.  Extinguished part replaced by
	 * color of the air
	 */
	 VMOVE(swp->sw_color, air_sp->color);


	return(1);
}
