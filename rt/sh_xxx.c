/*
 *	S H _ X X X . C
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


#define xxx_MAGIC 0x18364	/* XXX change this number for each shader */
struct xxx_specific {
	long	magic;
	double	val;
	vect_t	delta;
	mat_t	xform;
};
#define CK_xxx_SP(_p) RT_CKMAG(_p, xxx_MAGIC, "xxx_specific")

static struct xxx_specific xxx_defaults = {
	xxx_MAGIC,
	1.0,
	{ 1.0, 1.0, 1.0 },
	};

#define SHDR_NULL	((struct xxx_specific *)0)
#define SHDR_O(m)	offsetof(struct xxx_specific, m)
#define SHDR_AO(m)	offsetofarray(struct xxx_specific, m)

struct structparse xxx_parse[] = {
	{"%f",  1, "val",		SHDR_O(val),		FUNC_NULL },
	{"%f",  1, "v",			SHDR_O(val),		FUNC_NULL },
	{"%f",  3, "scale",		SHDR_AO(scale),		FUNC_NULL },
	{"%f",  3, "s",			SHDR_AO(scale),		FUNC_NULL },
	{"",	0, (char *)0,		0,			FUNC_NULL }
};

HIDDEN int	xxx_setup(), xxx_render();
HIDDEN void	xxx_print(), xxx_free();

struct mfuncs xxx_mfuncs[] = {
	{"xxx",	0,	0,		MFI_NORMAL|MFI_HIT|MFI_UV,
	xxx_setup,	xxx_render,	xxx_print,	xxx_free },

	{(char *)0,	0,		0,		0,
	0,		0,		0,		0 }
};


/*	X X X _ S E T U P
 *
 *	This routine is called (at prep time)
 *	once for each region which uses this shader.
 *	Any shader-specific initialization should be done here.
 */
HIDDEN int
xxx_setup( rp, matparm, dpp, mfp, rtip)
register struct region	*rp;
struct rt_vls		*matparm;
char			**dpp;	/* pointer to reg_udata in *rp */
struct mfuncs		*mfp;
struct rt_i		*rtip;	/* New since 4.4 release */
{
	register struct xxx_specific	*xxx_sp;
	mat_t	tmp;

	RT_CHECK_RTI(rtip);
	RT_VLS_CHECK( matparm );
	RT_CK_REGION(rp);
	GETSTRUCT( xxx_sp, xxx_specific );
	*dpp = (char *)xxx_sp;

	memcpy(xxx_sp, &xxx_defaults, sizeof(struct xxx_specific) );

	if( rt_structparse( matparm, xxx_parse, (char *)xxx_sp ) < 0 )
		return(-1);

	/* Optional:  get the matrix which maps model space into
	 *  "region" or "shader" space
	 */
	db_region_mat(xxx_sp->xform, rtip->rti_dbip, rp->reg_name);

	/* Add any translation within shader/region space */
	mat_idn(tmp);
	tmp[MDX] = xxx_sp->delta[0];
	tmp[MDY] = xxx_sp->delta[1];
	tmp[MDZ] = xxx_sp->delta[2];
	mat_mul2(tmp, xxx_sp->xform);


	if( rdebug&RDEBUG_SHADE) {
		rt_structprint( rp->reg_name, xxx_parse, (char *)xxx_sp );
		mat_print( "xform", xxx->xform );
	}

	return(1);
}

/*
 *	X X X _ P R I N T
 */
HIDDEN void
xxx_print( rp, dp )
register struct region *rp;
char	*dp;
{
	rt_structprint( rp->reg_name, xxx_parse, (char *)dp );
}

/*
 *	X X X _ F R E E
 */
HIDDEN void
xxx_free( cp )
char *cp;
{
	rt_free( cp, "xxx_specific" );
}

/*
 *	X X X _ R E N D E R
 *
 *	This is called (from viewshade() in shade.c)
 *	once for each hit point to be shaded.
 */
int
xxx_render( ap, pp, swp, dp )
struct application	*ap;
struct partition	*pp;
struct shadework	*swp;
char	*dp;
{
	register struct xxx_specific *xxx_sp =
		(struct xxx_specific *)dp;
	point_t pt;

	RT_AP_CHECK(ap);
	RT_CHECK_PT(pp);
	CK_xxx_SP(xxx_sp);

	if( rdebug&RDEBUG_SHADE)
		rt_structprint( "foo", xxx_parse, (char *)xxx_sp );

	/* Optional: transform hit point into "shader-space coordinates" */
	MAT4X3PNT(pt, xxx_sp->xform, swp->sw_hit.hit_point);



	/* XXX perform shading operations here */

	/* caller will perform transmission/reflection calculations
	 * based upon the values of swp->sw_transmit and swp->sw_reflect
	 *
	 * 0 < swp->sw_transmit <= 1 causes transmission computations
	 * 0 < swp->sw_reflect <= 1 causes reflection computations
	 */

	return(1);
}
