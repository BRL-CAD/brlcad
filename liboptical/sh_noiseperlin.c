/*
 *	S H _ N P . C
 *
 *  To add a new shader to the "rt" program's LIBOPTICAL library:
 *
 *	1) Copy this file to sh_shadername.c
 *	2) edit sh_shadername.c:
 *		change "N P" to "S H A D E R N A M E"
 *		change "noiseperlin"   to "shadername"
 *		Set a new number for the noiseperlin_MAGIC define
 *		define shader specific structure and defaults
 *		edit/build parse table for bu_structparse from noiseperlin_parse
 *		edit/build shader_mfuncs tables from noiseperlin_mfuncs for
 *			each shader name being built.
 *		edit the noiseperlin_setup function to do shader-specific setup
 *		edit the noiseperlin_render function to do the actual rendering
 *	3) Edit init.c to add extern for noiseperlin_mfuncs and 
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

#define noiseperlin_MAGIC 0x1834    /* make this a unique number for each shader */
#define CK_noiseperlin_SP(_p) BU_CKMAG(_p, noiseperlin_MAGIC, "noiseperlin_specific")

#define NOISESIZE 575
float noiseTable[NOISESIZE][NOISESIZE];

float linearNoise( float u, float v);
float turbulence( float u, float v );

/*
 * the shader specific structure contains all variables which are unique
 * to any particular use of the shader.
 */
struct noiseperlin_specific {
	long	magic;	/* magic # for memory validity check, must come 1st */
	double	noiseperlin_val;	/* variables for shader ... */
	double	noiseperlin_dist;
	vect_t	noiseperlin_delta;
	point_t noiseperlin_min;
	point_t noiseperlin_max;
	mat_t	noiseperlin_m_to_sh;	/* model to shader space matrix */
	mat_t	noiseperlin_m_to_r;	/* model to shader space matrix */
};

/* The default values for the variables in the shader specific structure */
CONST static
struct noiseperlin_specific noiseperlin_defaults = {
	noiseperlin_MAGIC,
	1.0,				/* noiseperlin_val */
	0.0,				/* noiseperlin_dist */
	{ 1.0, 1.0, 1.0 },		/* noiseperlin_delta */
	{ 0.0, 0.0, 0.0 },		/* noiseperlin_min */
	{ 0.0, 0.0, 0.0 },		/* noiseperlin_max */
	{	0.0, 0.0, 0.0, 0.0,	/* noiseperlin_m_to_sh */
		0.0, 0.0, 0.0, 0.0,
		0.0, 0.0, 0.0, 0.0,
		0.0, 0.0, 0.0, 0.0 },
	{	0.0, 0.0, 0.0, 0.0,	/* noiseperlin_m_to_r */
		0.0, 0.0, 0.0, 0.0,
		0.0, 0.0, 0.0, 0.0,
		0.0, 0.0, 0.0, 0.0 }
	};

#define SHDR_NULL	((struct noiseperlin_specific *)0)
#define SHDR_O(m)	offsetof(struct noiseperlin_specific, m)
#define SHDR_AO(m)	bu_offsetofarray(struct noiseperlin_specific, m)


/* description of how to parse/print the arguments to the shader
 * There is at least one line here for each variable in the shader specific
 * structure above
 */
struct bu_structparse noiseperlin_print_tab[] = {
	{"%f",  1, "val",		SHDR_O(noiseperlin_val),	BU_STRUCTPARSE_FUNC_NULL },
	{"%f",  1, "dist",		SHDR_O(noiseperlin_dist),	BU_STRUCTPARSE_FUNC_NULL },
	{"%f",  3, "delta",		SHDR_AO(noiseperlin_delta),	BU_STRUCTPARSE_FUNC_NULL },
	{"%f",  3, "max",		SHDR_AO(noiseperlin_max),	BU_STRUCTPARSE_FUNC_NULL },
	{"%f",  3, "min",		SHDR_AO(noiseperlin_min),	BU_STRUCTPARSE_FUNC_NULL },
	{"",	0, (char *)0,		0,			BU_STRUCTPARSE_FUNC_NULL }

};
struct bu_structparse noiseperlin_parse_tab[] = {
	{"i",	bu_byteoffset(noiseperlin_print_tab[0]), "noiseperlin_print_tab", 0, BU_STRUCTPARSE_FUNC_NULL },
	{"%f",  1, "v",		SHDR_O(noiseperlin_val),	BU_STRUCTPARSE_FUNC_NULL },
	{"%f",  1, "dist",	SHDR_O(noiseperlin_dist),	bu_mm_cvt },
	{"%f",  3, "d",		SHDR_AO(noiseperlin_delta),	BU_STRUCTPARSE_FUNC_NULL },
	{"",	0, (char *)0,	0,			BU_STRUCTPARSE_FUNC_NULL }
};

HIDDEN int	noiseperlin_setup(), noiseperlin_render();
HIDDEN void	noiseperlin_print(), noiseperlin_free();

/* The "mfuncs" structure defines the external interface to the shader.
 * Note that more than one shader "name" can be associated with a given
 * shader by defining more than one mfuncs struct in this array.
 * See sh_phong.c for an example of building more than one shader "name"
 * from a set of source functions.  There you will find that "glass" "mirror"
 * and "plastic" are all names for the same shader with different default
 * values for the parameters.
 */
struct mfuncs noiseperlin_mfuncs[] = {
	{MF_MAGIC,	"noiseperlin",		0,		MFI_NORMAL|MFI_HIT|MFI_UV,	0,
	noiseperlin_setup,	noiseperlin_render,	noiseperlin_print,	noiseperlin_free },

	{0,		(char *)0,	0,		0,		0,
	0,		0,		0,		0 }
};


/*	N P _ S E T U P
 *
 *	This routine is called (at prep time)
 *	once for each region which uses this shader.
 *	Any shader-specific initialization should be done here.
 */
HIDDEN int
noiseperlin_setup( rp, matparm, dpp, mfp, rtip)
register struct region	*rp;
struct bu_vls		*matparm;
char			**dpp;	/* pointer to reg_udata in *rp */
struct mfuncs		*mfp;
struct rt_i		*rtip;	/* New since 4.4 release */
{
	register struct noiseperlin_specific	*noiseperlin_sp;
	mat_t	tmp;
	vect_t	bb_min, bb_max, v_tmp;
   long i, j;

	/* check the arguments */
	RT_CHECK_RTI(rtip);
	BU_CK_VLS( matparm );
	RT_CK_REGION(rp);

   bu_log("setting up noise\n");
	if( rdebug&RDEBUG_SHADE)
		bu_log("noiseperlin_setup(%s)\n", rp->reg_name);

	/* Get memory for the shader parameters and shader-specific data */
	BU_GETSTRUCT( noiseperlin_sp, noiseperlin_specific );
	*dpp = (char *)noiseperlin_sp;

	/* initialize the default values for the shader */
	memcpy(noiseperlin_sp, &noiseperlin_defaults, sizeof(struct noiseperlin_specific) );

	/* parse the user's arguments for this use of the shader. */
	if( bu_struct_parse( matparm, noiseperlin_parse_tab, (char *)noiseperlin_sp ) < 0 )
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
*	db_shader_mat(noiseperlin_sp->noiseperlin_m_to_sh, rtip, rp, noiseperlin_sp->noiseperlin_min,
*		noiseperlin_sp->noiseperlin_max);
	 *
	 * Alternatively, shading may be done in "region coordinates"
	 * if desired:
	 *
*	db_region_mat(noiseperlin_sp->noiseperlin_m_to_r, rtip->rti_dbip, rp->reg_name);
	 *
	 */

   srand(time(NULL));
   for ( i = 0; i < NOISESIZE; i++ )
      for ( j = 0; j < NOISESIZE; j++ )
         noiseTable[i][j] = (float)rand() / RAND_MAX;

	if( rdebug&RDEBUG_SHADE) {
		bu_struct_print( " Parameters:", noiseperlin_print_tab, (char *)noiseperlin_sp );
		bn_mat_print( "m_to_sh", noiseperlin_sp->noiseperlin_m_to_sh );
	}

	return(1);
}

/*
 *	N P _ P R I N T
 */
HIDDEN void
noiseperlin_print( rp, dp )
register struct region *rp;
char	*dp;
{
	bu_struct_print( rp->reg_name, noiseperlin_print_tab, (char *)dp );
}

/*
 *	N P _ F R E E
 */
HIDDEN void
noiseperlin_free( cp )
char *cp;
{
	bu_free( cp, "noiseperlin_specific" );
}

/*
 *	N P _ R E N D E R
 *
 *	This is called (from viewshade() in shade.c) once for each hit point
 *	to be shaded.  The purpose here is to fill in values in the shadework
 *	structure.
 */
int
noiseperlin_render( ap, pp, swp, dp )
struct application	*ap;
struct partition	*pp;
struct shadework	*swp;	/* defined in material.h */
char			*dp;	/* ptr to the shader-specific struct */
{
	register struct noiseperlin_specific *noiseperlin_sp =
		(struct noiseperlin_specific *)dp;
	point_t pt;

	/* check the validity of the arguments we got */
	RT_AP_CHECK(ap);
	RT_CHECK_PT(pp);
	CK_noiseperlin_SP(noiseperlin_sp);

	if( rdebug&RDEBUG_SHADE)
		bu_struct_print( "noiseperlin_render Parameters:", noiseperlin_print_tab, (char *)noiseperlin_sp );

	/* If we are performing the shading in "region" space, we must 
	 * transform the hit point from "model" space to "region" space.
	 * See the call to db_region_mat in noiseperlin_setup().
	MAT4X3PNT(pt, noiseperlin_sp->noiseperlin_m_to_sh, swp->sw_hit.hit_point);
	MAT4X3PNT(pt, noiseperlin_sp->noiseperlin_m_to_r, swp->sw_hit.hit_point);
	 */

	if( rdebug&RDEBUG_SHADE) {
		bu_log("noiseperlin_render()  model:(%g %g %g) shader:(%g %g %g)\n", 
		V3ARGS(swp->sw_hit.hit_point),
		V3ARGS(pt) );
	}

	/* NOISEPERLIN perform shading operations here */
	VMOVE(swp->sw_color, pt);

   /*   swp->sw_color[0] = turbulence( swp->sw_uv.uv_u, swp->sw_uv.uv_v ); */
   swp->sw_color[0] = linearNoise( swp->sw_uv.uv_u, swp->sw_uv.uv_v ); 
   swp->sw_color[1] = swp->sw_color[0];
   swp->sw_color[2] = swp->sw_color[0];
	/* shader must perform transmission/reflection calculations
	 *
	 * 0 < swp->sw_transmit <= 1 causes transmission computations
	 * 0 < swp->sw_reflect <= 1 causes reflection computations
	 */
	if( swp->sw_reflect > 0 || swp->sw_transmit > 0 )
		(void)rr_render( ap, pp, swp );

	return(1);
}

float linearNoise( float u, float v )
{
   int iu, iv, ip, iq;
   float du, dv, bot, top;

   iu = u * NOISESIZE;
   iv = v * NOISESIZE;
   du = (u*NOISESIZE) - iu;
   dv = (v*NOISESIZE) - iv;

   iu = iu % NOISESIZE;
   iv = iv % NOISESIZE;
   ip = ( iu+1) % NOISESIZE;
   iq = (iv+1) % NOISESIZE;
   bot = noiseTable[iu][iv] + du * ( noiseTable[ip][iv]-noiseTable[iu][iv]);
   top = noiseTable[iu][iq] + du*(noiseTable[ip][iq]-noiseTable[iu][iq]);
   
   return ( bot + dv * ( top-bot) );
}

float turbulence ( float u, float v )
{
   float t = 0.0;
   float scale = 1.0;
   float pixel_size = .10;

   while ( scale > pixel_size )
   {
      t += linearNoise( u/scale, v/scale)*scale;
      scale /= 2.0;
   }
   
   return t;
}
