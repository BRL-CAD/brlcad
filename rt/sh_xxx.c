/*
 *	S H _ X X X . C
 *
 *  To add a new shader to the "rt" program:
 *
 *	1) Copy this file to sh_shadername.c
 *	2) edit sh_shadername.c:
 *		change "X X X" to "S H A D E R N A M E"
 *		change "xxx"   to "shadername"
 *		Set a new number for the xxx_MAGIC define
 *		define shader specific structure and defaults
 *		edit/build parse table for bu_structparse from xxx_parse
 *		edit/build shader_mfuncs tables from xxx_mfuncs for
 *			each shader name being built.
 *		edit the xxx_setup function to do shader-specific setup
 *		edit the xxx_render function to do the actual rendering
 *	3) Edit view.c to add extern for xxx_mfuncs and call to mlib_add
 *		to function view_init()
 *	4) Edit Cakefile to add shader file to "FILES" and "RT_OBJ" macros.
 *	5) replace this list with a description of the shader, its parameters
 *		and use.
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

/*
 *  Generic settable parameters.
 *  By setting the "base address" to zero in the bu_structparse call,
 *  the actual memory address is given here as the structure offset.
 *
 *  Strictly speaking, the C language only permits initializers of the
 *  form: address +- constant, where here the intent is to measure the
 *  byte address of the indicated variable.
 *  Matching compensation code for the CRAY is located in librt/parse.c
 */
#if CRAY
#	define byteoffset(_i)	(((int)&(_i)))	/* actually a word offset */
#else
#  if IRIX > 5
#	define byteoffset(_i)	((long)((char *)&(_i)))
#  else
#    if sgi || __convexc__ || ultrix || _HPUX_SOURCE
	/* "Lazy" way.  Works on reasonable machines with byte addressing */
#	define byteoffset(_i)	((int)((char *)&(_i)))
#    else
	/* "Conservative" way of finding # bytes as diff of 2 char ptrs */
#	define byteoffset(_i)	((int)(((char *)&(_i))-((char *)0)))
#    endif
#  endif
#endif




#define xxx_MAGIC 0x1834    /* make this a unique number for each shader */
#define CK_xxx_SP(_p) RT_CKMAG(_p, xxx_MAGIC, "xxx_specific")

/*
 * the shader specific structure contains all variables which are unique
 * to any particular use of the shader.
 */
struct xxx_specific {
	long	magic;	/* magic # for memory validity check, must come 1st */
	double	xxx_val;	/* variables for shader ... */
	vect_t	xxx_delta;
	mat_t	xxx_m_to_sh;	/* model to shader space matrix */
};

/* The default values for the variables in the shader specific structure */
CONST static
struct xxx_specific xxx_defaults = {
	xxx_MAGIC,
	1.0,
	{ 1.0, 1.0, 1.0 },
	{	0.0, 0.0, 0.0, 0.0,
		0.0, 0.0, 0.0, 0.0,
		0.0, 0.0, 0.0, 0.0,
		0.0, 0.0, 0.0, 0.0 }
	};

#define SHDR_NULL	((struct xxx_specific *)0)
#define SHDR_O(m)	offsetof(struct xxx_specific, m)
#define SHDR_AO(m)	offsetofarray(struct xxx_specific, m)


/* description of how to parse/print the arguments to the shader
 * There is at least one line here for each variable in the shader specific
 * structure above
 */
struct bu_structparse xxx_print_tab[] = {
	{"%f",  1, "val",		SHDR_O(xxx_val),	FUNC_NULL },
	{"%f",  3, "delta",		SHDR_AO(xxx_delta),	FUNC_NULL },
	{"",	0, (char *)0,		0,			FUNC_NULL }

};
struct bu_structparse xxx_parse_tab[] = {
	{"i",	byteoffset(xxx_print_tab[0]), "xxx_print_tab", 0, FUNC_NULL },
	{"%f",  1, "v",			SHDR_O(xxx_val),	FUNC_NULL },
	{"%f",  3, "d",			SHDR_AO(xxx_delta),	FUNC_NULL },
	{"",	0, (char *)0,		0,			FUNC_NULL }
};

HIDDEN int	xxx_setup(), xxx_render();
HIDDEN void	xxx_print(), xxx_free();

/* The "mfuncs" structure defines the external interface to the shader.
 * Note that more than one shader "name" can be associated with a given
 * shader by defining more than one mfuncs struct in this array.
 * See sh_phong.c for an example of building more than one shader "name"
 * from a set of source functions.  There you will find that "glass" "mirror"
 * and "plastic" are all names for the same shader with different default
 * values for the parameters.
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
	vect_t	bb_min, bb_max, v_tmp;

	/* check the arguments */
	RT_CHECK_RTI(rtip);
	RT_VLS_CHECK( matparm );
	RT_CK_REGION(rp);


	if( rdebug&RDEBUG_SHADE)
		rt_log("xxx_setup(%s)\n", rp->reg_name);

	/* Get memory for the shader parameters and shader-specific data */
	GETSTRUCT( xxx_sp, xxx_specific );
	*dpp = (char *)xxx_sp;

	/* initialize the default values for the shader */
	memcpy(xxx_sp, &xxx_defaults, sizeof(struct xxx_specific) );

	/* parse the user's arguments for this use of the shader. */
	if( bu_struct_parse( matparm, xxx_parse_tab, (char *)xxx_sp ) < 0 )
		return(-1);

	/* Optional:
	 *
	 * If the shader needs to operate in a coordinate system which stays
	 * fixed on the region when the region is moved (as in animation)
	 * we need to get a matrix to perform the appropriate transform(s).
	 *
	 * db_shader_mat returns a matrix which maps points on/in the region
	 * into the unit cube.  This unit cube is formed by first mapping from
	 * world coordinates into "region coordinates" (the coordinate system
	 * in which the region is defined).  Then the bounding box of the 
	 * region is used to establish a mapping to the unit cube
	 */

	db_shader_mat(xxx_sp->xxx_m_to_sh, rtip, rp);


	if( rdebug&RDEBUG_SHADE) {
		bu_struct_print( " Parameters:", xxx_print_tab, (char *)xxx_sp );
		mat_print( "m_to_sh", xxx_sp->xxx_m_to_sh );
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
	bu_struct_print( rp->reg_name, xxx_print_tab, (char *)dp );
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
 *	This is called (from viewshade() in shade.c) once for each hit point
 *	to be shaded.  The purpose here is to fill in values in the shadework
 *	structure.
 */
int
xxx_render( ap, pp, swp, dp )
struct application	*ap;
struct partition	*pp;
struct shadework	*swp;	/* defined in material.h */
char			*dp;	/* ptr to the shader-specific struct */
{
	register struct xxx_specific *xxx_sp =
		(struct xxx_specific *)dp;
	point_t pt;

	/* check the validity of the arguments we got */
	RT_AP_CHECK(ap);
	RT_CHECK_PT(pp);
	CK_xxx_SP(xxx_sp);

	if( rdebug&RDEBUG_SHADE)
		bu_struct_print( "xxx_render Parameters:", xxx_print_tab, (char *)xxx_sp );

	/* If we are performing the shading in "region" space, we must 
	 * transform the hit point from "model" space to "region" space.
	 * See the call to db_region_mat in xxx_setup().
	 */
	MAT4X3PNT(pt, xxx_sp->xxx_m_to_sh, swp->sw_hit.hit_point);

	if( rdebug&RDEBUG_SHADE) {
		rt_log("xxx_render()  model:(%g %g %g) shader:(%g %g %g)\n", 
		V3ARGS(swp->sw_hit.hit_point),
		V3ARGS(pt) );
	}

	/* XXX perform shading operations here */
	VMOVE(swp->sw_color, pt);

	/* shader must perform transmission/reflection calculations
	 *
	 * 0 < swp->sw_transmit <= 1 causes transmission computations
	 * 0 < swp->sw_reflect <= 1 causes reflection computations
	 */
	if( swp->sw_reflect > 0 || swp->sw_transmit > 0 )
		(void)rr_render( ap, pp, swp );

	return(1);
}
