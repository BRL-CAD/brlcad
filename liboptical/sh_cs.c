/*
 *	S H _ C S . C
 *
 *	Color Square shader.  Maps the shader space RPP onto the unit
 *	color cube.
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


#define cs_MAGIC 0x1834    /* make this a unique number for each shader */
#define CK_cs_SP(_p) RT_CKMAG(_p, cs_MAGIC, "cs_specific")

/*
 * the shader specific structure contains all variables which are unique
 * to any particular use of the shader.
 */
struct cs_specific {
	long	magic;	/* magic # for memory validity check, must come 1st */
	double	val;	/* variables for shader ... */
	vect_t	delta;
	mat_t	m_to_r;	/* model to region matrix */
	char	*name;
};

/* The default values for the variables in the shader specific structure */
CONST static
struct cs_specific cs_defaults = {
	cs_MAGIC,
	1.0,
	{ 1.0, 1.0, 1.0 },
	{	0.0, 0.0, 0.0, 0.0,
		0.0, 0.0, 0.0, 0.0,
		0.0, 0.0, 0.0, 0.0,
		0.0, 0.0, 0.0, 0.0 }
	};

#define SHDR_NULL	((struct cs_specific *)0)
#define SHDR_O(m)	offsetof(struct cs_specific, m)
#define SHDR_AO(m)	offsetofarray(struct cs_specific, m)

/* description of how to parse/print the arguments to the shader
 * There is at least one line here for each variable in the shader specific
 * structure above
 */
struct structparse cs_print_tab[] = {
	{"%f",  1, "val",		SHDR_O(val),		FUNC_NULL },
	{"%f",  3, "delta",		SHDR_AO(delta),		FUNC_NULL },
	{"",	0, (char *)0,		0,			FUNC_NULL }

};
struct structparse cs_parse_tab[] = {
	{"i",	(long)(cs_print_tab), (char *)0, 0,		FUNC_NULL },
	{"%f",  1, "v",			SHDR_O(val),		FUNC_NULL },
	{"%f",  3, "d",			SHDR_AO(delta),		FUNC_NULL },
	{"",	0, (char *)0,		0,			FUNC_NULL }
};

HIDDEN int	cs_setup(), cs_render();
HIDDEN void	cs_print(), cs_free();

/* The "mfuncs" structure defines the external interface to the shader.
 * Note that more than one shader "name" can be associated with a given
 * shader by defining more than one mfuncs struct in this array.
 * See sh_phong.c for an example of building more than one shader "name"
 * from a set of source functions.  There you will find that "glass" "mirror"
 * and "plastic" are all names for the same shader with different default
 * values for the parameters.
 */
struct mfuncs cs_mfuncs[] = {
	{"cs",	0,	0,		MFI_NORMAL|MFI_HIT|MFI_UV,	0,
	cs_setup,	cs_render,	cs_print,	cs_free },

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
cs_setup( rp, matparm, dpp, mfp, rtip)
register struct region	*rp;
struct rt_vls		*matparm;
char			**dpp;	/* pointer to reg_udata in *rp */
struct mfuncs		*mfp;
struct rt_i		*rtip;	/* New since 4.4 release */
{
	register struct cs_specific	*cs_sp;
	mat_t	tmp;
	vect_t	bb_min, bb_max, v_tmp;

	/* check the arguments */
	RT_CHECK_RTI(rtip);
	RT_VLS_CHECK( matparm );
	RT_CK_REGION(rp);


	if( rdebug&RDEBUG_SHADE)
		rt_log("cs_setup(%s)\n", rp->reg_name);

	/* Get memory for the shader parameters and shader-specific data */
	GETSTRUCT( cs_sp, cs_specific );
	*dpp = (char *)cs_sp;

	/* initialize the default values for the shader */
	memcpy(cs_sp, &cs_defaults, sizeof(struct cs_specific) );

	/* parse the user's arguments for this use of the shader. */
	if( rt_structparse( matparm, cs_parse_tab, (char *)cs_sp ) < 0 )
		return(-1);


#if 0
	/* Optional:  get the matrix which maps model space into
	 *  "region" or "shader" space.
	 *
	 * If the shader needs to operate in a coordinate system which stays
	 * fixed on the region when the region is moved (as in animation)
	 * we need to get a matrix to perform the appropriate transform(s).
	 */

	db_region_mat(cs_sp->m_to_r, rtip->rti_dbip, rp->reg_name);

	/* 
	 * scale space so that the range 0..1 in shader space covers 
	 * the region bounding box exactly
	 */

	VSETALL(bb_max, -INFINITY);
	VSETALL(bb_min, INFINITY);
	rt_bound_tree(rp->reg_treetop, bb_min, bb_max);

	if( rdebug&RDEBUG_SHADE)
		rt_log("bb_min(%g %g %g)   bb_max(%g %g %g)\n",
			V3ARGS(bb_min),		V3ARGS(bb_max) );

	VSUB2(v_tmp, bb_max, bb_min);
	VINVDIR(v_tmp, v_tmp);
	mat_idn(tmp);
	MAT_SCALE_VEC(tmp, v_tmp);
	mat_mul2(tmp, cs_sp->m_to_r);
#else
	db_shader_mat(cs_sp->m_to_r, rtip->rti_dbip, rp);
#endif
	/* 
	 * cs_sp->m_to_r now maps model-space coordinates
	 * into region-space coordinates
	 */

	if( rdebug&RDEBUG_SHADE) {
		rt_structprint( " Parameters:", cs_print_tab, (char *)cs_sp );
		mat_print( "m_to_r", cs_sp->m_to_r );
	}

	return(1);
}

/*
 *	X X X _ P R I N T
 */
HIDDEN void
cs_print( rp, dp )
register struct region *rp;
char	*dp;
{
	rt_structprint( rp->reg_name, cs_print_tab, (char *)dp );
}

/*
 *	X X X _ F R E E
 */
HIDDEN void
cs_free( cp )
char *cp;
{
	rt_free( cp, "cs_specific" );
}

/*
 *	X X X _ R E N D E R
 *
 *	This is called (from viewshade() in shade.c) once for each hit point
 *	to be shaded.  The purpose here is to fill in values in the shadework
 *	structure.
 */
int
cs_render( ap, pp, swp, dp )
struct application	*ap;
struct partition	*pp;
struct shadework	*swp;	/* defined in material.h */
char			*dp;	/* ptr to the shader-specific struct */
{
	register struct cs_specific *cs_sp =
		(struct cs_specific *)dp;
	point_t pt;

	/* check the validity of the arguments we got */
	RT_AP_CHECK(ap);
	RT_CHECK_PT(pp);
	CK_cs_SP(cs_sp);

	if( rdebug&RDEBUG_SHADE)
		rt_structprint( "cs", cs_print_tab, (char *)cs_sp );

	/* If we are performing the shading in "region" space, we must 
	 * transform the hit point from "model" space to "region" space.
	 * See the call to db_region_mat in cs_setup().
	 */
	MAT4X3PNT(pt, cs_sp->m_to_r, swp->sw_hit.hit_point);


	if( rdebug&RDEBUG_SHADE)
		rt_log("cs_render(%s)  model:(%g %g %g)\ncs_render(%s) region:(%g %g %g)\n", 
		cs_sp->name,
		V3ARGS(swp->sw_hit.hit_point),
		cs_sp->name,
		V3ARGS(pt) );

	/* XXX perform shading operations here */


	/* caller will perform transmission/reflection calculations
	 * based upon the values of swp->sw_transmit and swp->sw_reflect
	 *
	 * 0 < swp->sw_transmit <= 1 causes transmission computations
	 * 0 < swp->sw_reflect <= 1 causes reflection computations
	 */

	VMOVE(swp->sw_color, pt);

	return(1);
}
