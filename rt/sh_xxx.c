/*
 *	S H _ X X X . C
 *
 *  To add a new shader to the "rt" program:
 *
 *	1) Copy this file to sh_shadername.c
 *	2) edit sh_shadername.c:
 *		define shader specific structure and defaults
 *		edit/build parse table for structparse from xxx_parse
 *		edit/build shader_mfuncs tables from xxx_mfuncs for
 *			each shader name being built.
 *		edit the xxx_setup function to do shader-specific setup
 *		edit the xxx_render function to do the actual rendering
 *	3) Edit view.c to add extern for shader_mfuncs and call to mlib_add
 *	   to function view_init()
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


#define xxx_MAGIC 0x18364    /* make this a unique number for each shader */
#define CK_xxx_SP(_p) RT_CKMAG(_p, xxx_MAGIC, "xxx_specific")

/*
 * the shader specific structure contains all variables which unique to any
 * particular use of the shader.
 */
struct xxx_specific {
	long	magic;	/* magic # for memory validity check, must come 1st */
	double	val;	/* variables for shader ... */
	vect_t	delta;
	mat_t	xform;
};

/* The default values for the variables in the shader specific structure */
CONST static
struct xxx_specific xxx_defaults = {
	xxx_MAGIC,
	1.0,
	{ 1.0, 1.0, 1.0 },
	{	1.0, 0.0, 0.0, 0.0,
		0.0, 1.0, 0.0, 0.0,
		0.0, 0.0, 1.0, 0.0,
		0.0, 0.0, 0.0, 1.0 }
	};

#define SHDR_NULL	((struct xxx_specific *)0)
#define SHDR_O(m)	offsetof(struct xxx_specific, m)
#define SHDR_AO(m)	offsetofarray(struct xxx_specific, m)

/* description of how to parse/print the arguments to the shader */
struct structparse xxx_parse[] = {
	{"%f",  1, "val",		SHDR_O(val),		FUNC_NULL },
	{"%f",  1, "v",			SHDR_O(val),		FUNC_NULL },
	{"%f",  3, "delta",		SHDR_AO(delta),		FUNC_NULL },
	{"%f",  3, "d",			SHDR_AO(delta),		FUNC_NULL },
	{"",	0, (char *)0,		0,			FUNC_NULL }
};

HIDDEN int	xxx_setup(), xxx_render();
HIDDEN void	xxx_print(), xxx_free();

/* The "mfuncs" structure defines the external interface to the shader.
 * Note that more than one shader "name" can be associated with a given
 * shader by defining more than one mfuncs struct in this array.
 */
struct mfuncs xxx_mfuncs[] = {
	{"xxx",	0,	0,		MFI_NORMAL|MFI_HIT|MFI_UV,	0,
	xxx_setup,	xxx_render,	xxx_print,	xxx_free },

	{(char *)0,	0,		0,		0,		0,
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

	/* initialize the default values for the shader */
	memcpy(xxx_sp, &xxx_defaults, sizeof(struct xxx_specific) );

	/* parse the user's arguments for this use of the shader. */
	if( rt_structparse( matparm, xxx_parse, (char *)xxx_sp ) < 0 )
		return(-1);

	/* Optional:  get the matrix which maps model space into
	 *  "region" or "shader" space.
	 *
	 * If the shader needs to operate in a coordinate system which stays
	 * fixed on the region when the region is moved (as in animation)
	 * we need to get a matrix to perform the appropriate transform(s).
	 */
	db_region_mat(xxx_sp->xform, rtip->rti_dbip, rp->reg_name);

	/* Example:  append parameter translations to transformation
	 * 	from "model" to "region" space.
	 */
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
		rt_structprint( "xxx", xxx_parse, (char *)xxx_sp );

	/* If we are performing the shading in "region" space, we must 
	 * transform the hit point from "model" space to "region" space.
	 * See the call to db_region_mat in xxx_setup().
	 */
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
