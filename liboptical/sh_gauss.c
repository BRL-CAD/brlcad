/*
 *	S H _ G A U S S . C
 *
 *  To add a new shader to the "rt" program:
 *
 *	1) Copy this file to sh_shadername.c
 *	2) edit sh_shadername.c:
 *		change "G A U S S" to "S H A D E R N A M E"
 *		change "gauss"   to "shadername"
 *		Set a new number for the gauss_MAGIC define
 *		define shader specific structure and defaults
 *		edit/build parse table for structparse from gauss_parse
 *		edit/build shader_mfuncs tables from gauss_mfuncs for
 *			each shader name being built.
 *		edit the gauss_setup function to do shader-specific setup
 *		edit the gauss_render function to do the actual rendering
 *	3) Edit view.c to add extern for gauss_mfuncs and call to mlib_add
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
 *  By setting the "base address" to zero in the rt_structparse call,
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


/* XXX God this is a hack, we should have some way of asking this of the
 * solid rather than pirating it's private parts.  Talk about castrating
 * the packaging....
 */
struct ell_specific {
	vect_t	ell_V;		/* Vector to center of ellipsoid */
	vect_t	ell_Au;		/* unit-length A vector */
	vect_t	ell_Bu;
	vect_t	ell_Cu;
	vect_t	ell_invsq;	/* [ 1/(|A|**2), 1/(|B|**2), 1/(|C|**2) ] */
	mat_t	ell_SoR;	/* Scale(Rot(vect)) */
	mat_t	ell_invRSSR;	/* invRot(Scale(Scale(Rot(vect)))) */
};

#define gauss_MAGIC 0x6ao5    /* make this a unique number for each shader */
#define CK_gauss_SP(_p) RT_CKMAG(_p, gauss_MAGIC, "gauss_specific")

/*
 * the shader specific structure contains all variables which are unique
 * to any particular use of the shader.
 */
struct gauss_specific {
	long	magic;	/* magic # for memory validity check, must come 1st */
	double	gauss_val;	/* variables for shader ... */
	vect_t	gauss_delta;
	mat_t	gauss_m_to_sh;	/* model to shader space matrix */
	struct ell_specific ell_sp;
};

/* The default values for the variables in the shader specific structure */
CONST static
struct gauss_specific gauss_defaults = {
	gauss_MAGIC,
	1.0,
	{ 1.0, 1.0, 1.0 },
	{	0.0, 0.0, 0.0, 0.0,
		0.0, 0.0, 0.0, 0.0,
		0.0, 0.0, 0.0, 0.0,
		0.0, 0.0, 0.0, 0.0 }
	};

#define SHDR_NULL	((struct gauss_specific *)0)
#define SHDR_O(m)	offsetof(struct gauss_specific, m)
#define SHDR_AO(m)	offsetofarray(struct gauss_specific, m)


/* description of how to parse/print the arguments to the shader
 * There is at least one line here for each variable in the shader specific
 * structure above
 */
struct structparse gauss_print_tab[] = {
	{"%f",  1, "val",		SHDR_O(gauss_val),	FUNC_NULL },
	{"%f",  3, "delta",		SHDR_AO(gauss_delta),	FUNC_NULL },
	{"",	0, (char *)0,		0,			FUNC_NULL }

};
struct structparse gauss_parse_tab[] = {
	{"i",	byteoffset(gauss_print_tab[0]), "gauss_print_tab", 0, FUNC_NULL },
	{"%f",  1, "v",			SHDR_O(gauss_val),	FUNC_NULL },
	{"%f",  3, "d",			SHDR_AO(gauss_delta),	FUNC_NULL },
	{"",	0, (char *)0,		0,			FUNC_NULL }
};

HIDDEN int	gauss_setup(), gauss_render();
HIDDEN void	gauss_print(), gauss_free();

/* The "mfuncs" structure defines the external interface to the shader.
 * Note that more than one shader "name" can be associated with a given
 * shader by defining more than one mfuncs struct in this array.
 * See sh_phong.c for an example of building more than one shader "name"
 * from a set of source functions.  There you will find that "glass" "mirror"
 * and "plastic" are all names for the same shader with different default
 * values for the parameters.
 */
struct mfuncs gauss_mfuncs[] = {
	{"gauss",	0,	0,		MFI_NORMAL|MFI_HIT|MFI_UV,	0,
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
	struct directory *dp;
	struct rt_db_internal ip;
	union tree *tree_p;
	mat_t	reg_mat;
	mat_t	tmp;
	vect_t	bb_min, bb_max, v_tmp;

	/* check the arguments */
	RT_CHECK_RTI(rtip);
	RT_VLS_CHECK( matparm );
	RT_CK_REGION(rp);


	if( rdebug&RDEBUG_SHADE)
		rt_log("gauss_setup(%s)\n", rp->reg_name);

	/* Get memory for the shader parameters and shader-specific data */
	GETSTRUCT( gauss_sp, gauss_specific );
	*dpp = (char *)gauss_sp;

	/* initialize the default values for the shader */
	memcpy(gauss_sp, &gauss_defaults, sizeof(struct gauss_specific) );

	/* parse the user's arguments for this use of the shader. */
	if( rt_structparse( matparm, gauss_parse_tab, (char *)gauss_sp ) < 0 )
		return(-1);

	/* We have to pick up the parameters for the gaussian puff now.
	 * They won't be available later.  So what we do is sneak a peak
	 * inside the region, make sure the first item in it is an ellipsoid
	 * solid, and if it is go steal a peek at its balls ... er ... make
	 * that definition/parameters.
	 */

	if (rp->reg_treetop->tr_op != OP_SOLID)
		/* bitch and moan about non-solid and abort */
		rt_log("%s: non-solid first region element op(%d) s/b (%d)",
			rp->reg_name, rp->reg_treetop->tr_op, OP_SOLID);
		rt_bomb("gauss-shaded region needs ellipsoid as first element").
	}

	if (rp->reg_treetop->tree_leaf.tu_stp->st_id != ID_ELL) {
		/* bitch and moan, bitch and moan */
		rt_log("%s: non-ellipsoid first solid in region id(%d) s/b (%d)",
			rp->reg_name,
			rp->reg_treetop->tree_leaf.tu_stp->st_id,
			ID_ELL);
		rt_bomb("gauss-shaded region needs ellipsoid as first element").
	}

	memcpy(	gauss_sp->ell_sp,
		rp->reg_treetop->tree_leaf.tu_stp->st_specific,
		sizeof(struct ell_specific));

	/* XXX If this puppy isn't axis-aligned, we should come up with a
	 * matrix to rotate it into alignment.  We're going to have to do
	 * computation in the space defined by this ellipsoid.
	 */

/*	db_shader_mat(gauss_sp->gauss_m_to_sh, rtip->rti_dbip, rp); */


	if( rdebug&RDEBUG_SHADE) {
		rt_structprint( " Parameters:", gauss_print_tab, (char *)gauss_sp );
		mat_print( "m_to_sh", gauss_sp->gauss_m_to_sh );
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
	rt_structprint( rp->reg_name, gauss_print_tab, (char *)dp );
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
 *	This is called (from viewshade() in shade.c) once for each hit point
 *	to be shaded.  The purpose here is to fill in values in the shadework
 *	structure.
 */
int
gauss_render( ap, pp, swp, dp )
struct application	*ap;
struct partition	*pp;
struct shadework	*swp;	/* defined in material.h */
char			*dp;	/* ptr to the shader-specific struct */
{
	register struct gauss_specific *gauss_sp =
		(struct gauss_specific *)dp;
	point_t pt;

	/* check the validity of the arguments we got */
	RT_AP_CHECK(ap);
	RT_CHECK_PT(pp);
	CK_gauss_SP(gauss_sp);

	if( rdebug&RDEBUG_SHADE)
		rt_structprint( "gauss_render Parameters:", gauss_print_tab, (char *)gauss_sp );

	/* If we are performing the shading in "region" space, we must 
	 * transform the hit point from "model" space to "region" space.
	 * See the call to db_region_mat in gauss_setup().
	 */
	MAT4X3PNT(pt, gauss_sp->gauss_m_to_sh, swp->sw_hit.hit_point);

	if( rdebug&RDEBUG_SHADE) {
		rt_log("gauss_render()  model:(%g %g %g) shader:(%g %g %g)\n", 
		V3ARGS(swp->sw_hit.hit_point),
		V3ARGS(pt) );
	}

	/* perform shading operations here */

	/* First of all, we need to rotate the ray and ellipse into an
	 * axis-aligned state
	 */

	/* Now we need to evaluate the following function at various points
	 * along the solid/ray intersection partition:
							      2		2	 2
						      / (x-Ux)	  (y-Uy)   (z-Uz)  \
		   1.0				  -.5|   ----   + ------ + ------   |
---------------------------------------------- * e    \ sigma_x   sigma_y  sigma_z /
        (3/2)
   (2*PI)       * sigma_x * sigma_y * sigma_z)


	*/



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
