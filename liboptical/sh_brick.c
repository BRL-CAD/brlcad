/*
 *	S H _ B R I C K . C
 *
 *  To add a new shader to the "rt" program's LIBOPTICAL library:
 *
 *	1) Copy this file to sh_shadername.c
 *	2) edit sh_shadername.c:
 *		change "B R I C K" to "S H A D E R N A M E"
 *		change "brick"   to "shadername"
 *		Set a new number for the brick_MAGIC define
 *		define shader specific structure and defaults
 *		edit/build parse table for bu_structparse from brick_parse
 *		edit/build shader_mfuncs tables from brick_mfuncs for
 *			each shader name being built.
 *		edit the brick_setup function to do shader-specific setup
 *		edit the brick_render function to do the actual rendering
 *	3) Edit init.c to add extern for brick_mfuncs and 
 *		a call to mlib_add_shader().
 *	4) Edit Cakefile to add shader file to "FILES" macro (without the .o)
 *	5) replace this list with a description of the shader, its parameters
 *		and use.
 *	6) Edit shaders.tcl and comb.tcl in the ../tclscripts/mged directory to
 *		add a new gui for this shader.
 */
#include "conf.h"

#include <stdio.h>
#include <math.h>
#include "machine.h"
#include "vmath.h"
#include "raytrace.h"
#include "shadefuncs.h"
#include "shadework.h"
#include "../rt/rdebug.h"

#define brick_MAGIC 0x1834    /* make this a unique number for each shader */
#define CK_brick_SP(_p) BU_CKMAG(_p, brick_MAGIC, "brick_specific")

#define BRICKWIDTH 0.25
#define BRICKHEIGHT 0.08
#define MORTARTHICKNESS 0.01
#define BMWIDTH (BRICKWIDTH + MORTARTHICKNESS)
#define BMHEIGHT (BRICKHEIGHT + MORTARTHICKNESS)
#define MWF (MORTARTHICKNESS*0.5/BMWIDTH)
#define MHF (MORTARTHICKNESS*0.5/BMHEIGHT)

fastf_t brick_color[3] = {0.5,0.15, 0.14};
fastf_t mortar_color[3] = {0.5, 0.5, 0.5};
 

float step(float a, float x)
{
  return (float)(x>=a);
}

/*
 * the shader specific structure contains all variables which are unique
 * to any particular use of the shader.
 */
struct brick_specific {
	long	magic;	/* magic # for memory validity check, must come 1st */
	double	brick_val;	/* variables for shader ... */
	double	brick_dist;
	vect_t	brick_delta;
	point_t brick_min;
	point_t brick_max;
	mat_t	brick_m_to_sh;	/* model to shader space matrix */
	mat_t	brick_m_to_r;	/* model to shader space matrix */
};

/* The default values for the variables in the shader specific structure */
CONST static
struct brick_specific brick_defaults = {
	brick_MAGIC,
	1.0,				/* brick_val */
	0.0,				/* brick_dist */
	{ 1.0, 1.0, 1.0 },		/* brick_delta */
	{ 0.0, 0.0, 0.0 },		/* brick_min */
	{ 0.0, 0.0, 0.0 },		/* brick_max */
	{	0.0, 0.0, 0.0, 0.0,	/* brick_m_to_sh */
		0.0, 0.0, 0.0, 0.0,
		0.0, 0.0, 0.0, 0.0,
		0.0, 0.0, 0.0, 0.0 },
	{	0.0, 0.0, 0.0, 0.0,	/* brick_m_to_r */
		0.0, 0.0, 0.0, 0.0,
		0.0, 0.0, 0.0, 0.0,
		0.0, 0.0, 0.0, 0.0 }
	};

#define SHDR_NULL	((struct brick_specific *)0)
#define SHDR_O(m)	offsetof(struct brick_specific, m)
#define SHDR_AO(m)	bu_offsetofarray(struct brick_specific, m)


/* description of how to parse/print the arguments to the shader
 * There is at least one line here for each variable in the shader specific
 * structure above
 */
struct bu_structparse brick_print_tab[] = {
	{"%f",  1, "val",		SHDR_O(brick_val),	BU_STRUCTPARSE_FUNC_NULL },
	{"%f",  1, "dist",		SHDR_O(brick_dist),	BU_STRUCTPARSE_FUNC_NULL },
	{"%f",  3, "delta",		SHDR_AO(brick_delta),	BU_STRUCTPARSE_FUNC_NULL },
	{"%f",  3, "max",		SHDR_AO(brick_max),	BU_STRUCTPARSE_FUNC_NULL },
	{"%f",  3, "min",		SHDR_AO(brick_min),	BU_STRUCTPARSE_FUNC_NULL },
	{"",	0, (char *)0,		0,			BU_STRUCTPARSE_FUNC_NULL }

};
struct bu_structparse brick_parse_tab[] = {
	{"i",	bu_byteoffset(brick_print_tab[0]), "brick_print_tab", 0, BU_STRUCTPARSE_FUNC_NULL },
	{"%f",  1, "v",		SHDR_O(brick_val),	BU_STRUCTPARSE_FUNC_NULL },
	{"%f",  1, "dist",	SHDR_O(brick_dist),	bu_mm_cvt },
	{"%f",  3, "d",		SHDR_AO(brick_delta),	BU_STRUCTPARSE_FUNC_NULL },
	{"",	0, (char *)0,	0,			BU_STRUCTPARSE_FUNC_NULL }
};

HIDDEN int	brick_setup(), mybrick_render();
HIDDEN void	brick_print(), brick_free();

/* The "mfuncs" structure defines the external interface to the shader.
 * Note that more than one shader "name" can be associated with a given
 * shader by defining more than one mfuncs struct in this array.
 * See sh_phong.c for an example of building more than one shader "name"
 * from a set of source functions.  There you will find that "glass" "mirror"
 * and "plastic" are all names for the same shader with different default
 * values for the parameters.
 */
struct mfuncs brick_mfuncs[] = {
	{MF_MAGIC,	"brick",		0,		MFI_NORMAL|MFI_HIT|MFI_UV,	0,
	brick_setup,	mybrick_render,	brick_print,	brick_free },

	{0,		(char *)0,	0,		0,		0,
	0,		0,		0,		0 }
};


/*	B R I C K _ S E T U P
 *
 *	This routine is called (at prep time)
 *	once for each region which uses this shader.
 *	Any shader-specific initialization should be done here.
 */
HIDDEN int
brick_setup( rp, matparm, dpp, mfp, rtip)
register struct region	*rp;
struct bu_vls		*matparm;
char			**dpp;	/* pointer to reg_udata in *rp */
struct mfuncs		*mfp;
struct rt_i		*rtip;	/* New since 4.4 release */
{
	register struct brick_specific	*brick_sp;
	mat_t	tmp;
	vect_t	bb_min, bb_max, v_tmp;


	/* check the arguments */
	RT_CHECK_RTI(rtip);
	BU_CK_VLS( matparm );
	RT_CK_REGION(rp);


	if( rdebug&RDEBUG_SHADE)
		bu_log("brick_setup(%s)\n", rp->reg_name);

	/* Get memory for the shader parameters and shader-specific data */
	BU_GETSTRUCT( brick_sp, brick_specific );
	*dpp = (char *)brick_sp;

	/* initialize the default values for the shader */
	memcpy(brick_sp, &brick_defaults, sizeof(struct brick_specific) );

	/* parse the user's arguments for this use of the shader. */
	if( bu_struct_parse( matparm, brick_parse_tab, (char *)brick_sp ) < 0 )
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
	 *
*	db_shader_mat(brick_sp->brick_m_to_sh, rtip, rp, brick_sp->brick_min,
*		brick_sp->brick_max);
	 *
	 * Alternatively, shading may be done in "region coordinates"
	 * if desired:
	 *
*	db_region_mat(brick_sp->brick_m_to_r, rtip->rti_dbip, rp->reg_name);
	 *
	 */

	if( rdebug&RDEBUG_SHADE) {
		bu_struct_print( " Parameters:", brick_print_tab, (char *)brick_sp );
		bn_mat_print( "m_to_sh", brick_sp->brick_m_to_sh );
	}

	return(1);
}

/*
 *	B R I C K _ P R I N T
 */
HIDDEN void
brick_print( rp, dp )
register struct region *rp;
char	*dp;
{
	bu_struct_print( rp->reg_name, brick_print_tab, (char *)dp );
}

/*
 *	B R I C K _ F R E E
 */
HIDDEN void
brick_free( cp )
char *cp;
{
	bu_free( cp, "brick_specific" );
}

/*
 *	B R I C K _ R E N D E R
 *
 *	This is called (from viewshade() in shade.c) once for each hit point
 *	to be shaded.  The purpose here is to fill in values in the shadework
 *	structure.
 */
int
mybrick_render( ap, pp, swp, dp )
struct application	*ap;
struct partition	*pp;
struct shadework	*swp;	/* defined in material.h */
char			*dp;	/* ptr to the shader-specific struct */
{
	register struct brick_specific *brick_sp =
		(struct brick_specific *)dp;
	point_t pt;
	static int first = 1;
        vect_t incident_ray;
        vect_t surface_normal;
        fastf_t scoord, tcoord, ss ,tt, sbrick, tbrick, w, h;
       
	/* check the validity of the arguments we got */
	RT_AP_CHECK(ap);
	RT_CHECK_PT(pp);
	CK_brick_SP(brick_sp);

	if( rdebug&RDEBUG_SHADE)
		bu_struct_print( "brick_render Parameters:", brick_print_tab, (char *)brick_sp );

	/* If we are performing the shading in "region" space, we must 
	 * transform the hit point from "model" space to "region" space.
	 * See the call to db_region_mat in brick_setup().
	MAT4X3PNT(pt, brick_sp->brick_m_to_sh, swp->sw_hit.hit_point);
	MAT4X3PNT(pt, brick_sp->brick_m_to_r, swp->sw_hit.hit_point);
	 */

	if( rdebug&RDEBUG_SHADE) {
		bu_log("brick_render()  model:(%g %g %g) shader:(%g %g %g)\n", 
		V3ARGS(swp->sw_hit.hit_point),
		V3ARGS(pt) );
	}

	/* XXX perform shading operations here */
	VMOVE(swp->sw_color, pt);

	if ( first )
	  {
	    bu_log ( "view point: (%g %g %g)\n", V3ARGS(ap->a_ray.r_pt) );
	    first = 0;
	  }
      VSUB2(incident_ray,  swp->sw_hit.hit_point, ap->a_ray.r_pt);
      if(VDOT( swp->sw_hit.hit_normal, incident_ray)>0)
        {
         surface_normal[0] = -swp->sw_hit.hit_normal[0];       
         surface_normal[1] = -swp->sw_hit.hit_normal[1];                
         surface_normal[2] = -swp->sw_hit.hit_normal[2];       
	}
      else
	{
         surface_normal[0] = swp->sw_hit.hit_normal[0];       
         surface_normal[1] = swp->sw_hit.hit_normal[1];                
         surface_normal[2] = swp->sw_hit.hit_normal[2];  

	}
         VUNITIZE(surface_normal);
       ss= swp->sw_uv.uv_u/BMWIDTH;
       tt = swp->sw_uv.uv_v/BMHEIGHT;
       
       if(fmod(tt*0.5, 1.0)>0.5)
        ss += 0.5;
      
       sbrick= floor(ss);
       tbrick= floor(tt);

       ss-=sbrick;
       tt-=tbrick;
     
       w=step(MWF, ss)- step(1-MWF,ss);
       h=step(MHF, tt)- step(1-MHF,tt);

       swp->sw_color[0]= mortar_color[0]*(1.0-(w*h))+(w*h*brick_color[0]);
       swp->sw_color[1]= mortar_color[1]*(1.0-(w*h))+(w*h*brick_color[1]);
       swp->sw_color[2]= mortar_color[2]*(1.0-(w*h))+(w*h*brick_color[2]);
	
        /* shader must perform transmission/reflection calculations
	 *
	 * 0 < swp->sw_transmit <= 1 causes transmission computations
	 * 0 < swp->sw_reflect <= 1 causes reflection computations
	 */
	if( swp->sw_reflect > 0 || swp->sw_transmit > 0 )
	{
		bu_log("calling rr_render\n");
		(void)rr_render( ap, pp, swp );
	}
 
	return(1);
}
