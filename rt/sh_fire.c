/*
 *	S H _ F I R E . C
 *
 *  To add a new shader to the "rt" program:
 *
 *	1) Copy this file to sh_shadername.c
 *	2) edit sh_shadername.c:
 *		change "F I R E" to "S H A D E R N A M E"
 *		change "fire"   to "shadername"
 *		Set a new number for the fire_MAGIC define
 *		define shader specific structure and defaults
 *		edit/build parse table for structparse from fire_parse
 *		edit/build shader_mfuncs tables from fire_mfuncs for
 *			each shader name being built.
 *		edit the fire_setup function to do shader-specific setup
 *		edit the fire_render function to do the actual rendering
 *	3) Edit view.c to add extern for fire_mfuncs and call to mlib_add
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




#define fire_MAGIC 0x46697265   /* ``Fire'' */
#define CK_fire_SP(_p) RT_CKMAG(_p, fire_MAGIC, "fire_specific")

/*
 * the shader specific structure contains all variables which are unique
 * to any particular use of the shader.
 */
struct fire_specific {
	long	magic;	/* magic # for memory validity check, must come 1st */
	int	fire_debug;
	double	fire_stretch;
	double	noise_lacunarity;
	double	noise_h_val;
	double	noise_octaves;
	vect_t	noise_scale;
	vect_t	noise_delta;

	mat_t	fire_m_to_sh;		/* model to shader space matrix */
	mat_t	fire_sh_to_noise;	/* shader to noise space matrix */
	mat_t	fire_colorspline_mat;
};

/* The default values for the variables in the shader specific structure */
CONST static
struct fire_specific fire_defaults = {
	fire_MAGIC,
	0,			/* fire_debug */
	0.0,			/* fire_stretch */
	2.1753974,		/* noise_lacunarity */
	1.0,			/* noise_h_val */
	2.0,			/* noise_octaves */
	{ 10.0, 10.0, 10.0 },	/* noise_scale */
	{ 0.0, 0.0, 0.0 },	/* noise_delta */

	{	/* fire_m_to_sh */
		1.0, 0.0, 0.0, 0.0,
		0.0, 1.0, 0.0, 0.0,
		0.0, 0.0, 1.0, 0.0,
		0.0, 0.0, 0.0, 1.0 },
	{	/* fire_sh_to_noise */
		1.0, 0.0, 0.0, 0.0,
		0.0, 1.0, 0.0, 0.0,
		0.0, 0.0, 1.0, 0.0,
		0.0, 0.0, 0.0, 1.0 },
	{	/* fire_colorspline_mat */
		0.0, 0.0, 0.0, 0.0,
		0.0, 0.0, 0.0, 0.0,
		0.0, 0.0, 0.0, 0.0,
		0.0, 0.0, 0.0, 0.0 }
	};

#define SHDR_NULL	((struct fire_specific *)0)
#define SHDR_O(m)	offsetof(struct fire_specific, m)
#define SHDR_AO(m)	offsetofarray(struct fire_specific, m)



/* description of how to parse/print the arguments to the shader
 * There is at least one line here for each variable in the shader specific
 * structure above
 */
struct structparse fire_print_tab[] = {
	{"%d",  1, "debug",	SHDR_O(fire_debug),		FUNC_NULL },
	{"%f",  1, "stretch",	SHDR_O(fire_stretch),		FUNC_NULL },
	{"%f",	1, "lacunarity", SHDR_O(noise_lacunarity),	FUNC_NULL },
	{"%f",	1, "H", 	SHDR_O(noise_h_val),		FUNC_NULL },
	{"%f",	1, "octaves", 	SHDR_O(noise_octaves),		FUNC_NULL },
	{"%f",  1, "scale",	SHDR_O(noise_scale),		FUNC_NULL },
	{"%f",  3, "delta",	SHDR_AO(noise_delta),		FUNC_NULL },
	{"",	0, (char *)0,		0,			FUNC_NULL }

};
struct structparse fire_parse_tab[] = {
	{"i",	byteoffset(fire_print_tab[0]), "fire_print_tab", 0, FUNC_NULL },
	{"%f",  1, "st",	SHDR_O(fire_stretch),		FUNC_NULL },
	{"%f",	1, "l",		SHDR_O(noise_lacunarity),	FUNC_NULL },
	{"%f",	1, "H", 	SHDR_O(noise_h_val),		FUNC_NULL },
	{"%f",	1, "o", 	SHDR_O(noise_octaves),		FUNC_NULL },
	{"%f",  1, "sc",	SHDR_AO(noise_scale),		FUNC_NULL },
	{"%f",  3, "d",		SHDR_AO(noise_delta),		FUNC_NULL },
	{"",	0, (char *)0,		0,			FUNC_NULL }
};

HIDDEN int	fire_setup(), fire_render();
HIDDEN void	fire_print(), fire_free();

/* The "mfuncs" structure defines the external interface to the shader.
 * Note that more than one shader "name" can be associated with a given
 * shader by defining more than one mfuncs struct in this array.
 * See sh_phong.c for an example of building more than one shader "name"
 * from a set of source functions.  There you will find that "glass" "mirror"
 * and "plastic" are all names for the same shader with different default
 * values for the parameters.
 */
struct mfuncs fire_mfuncs[] = {
	{"fire",	0,	0,		MFI_HIT,	0,
	fire_setup,	fire_render,	fire_print,	fire_free },

	{(char *)0,	0,		0,		0,		0,
	0,		0,		0,		0 }
};

CONST double flame_colors[18][3] = {
	0.0, 0.0, 0.0,
	0.0, 0.0, 0.0,
	0.106, 0.0, 0.0,
	0.212, 0.0, 0.0,
	0.318, 0.0, 0.0,
	0.427, 0.0, 0.0,
	0.533, 0.0, 0.0,
	0.651, 0.02, 0.0,
	0.741, 0.118, 0.0,
	0.827, 0.235, 0.0,
	0.906, 0.353, 0.0,
	0.933, 0.500, 0.0,
	0.957, 0.635, 0.047,
	0.973, 0.733, 0.227,
	0.984, 0.820, 0.451,
	0.990, 0.925, 0.824,
	1.0, 0.945, 0.902,
	1.0, 0.945, 0.902
};

/*	F I R E _ S E T U P
 *
 *	This routine is called (at prep time)
 *	once for each region which uses this shader.
 *	Any shader-specific initialization should be done here.
 */
HIDDEN int
fire_setup( rp, matparm, dpp, mfp, rtip)
register struct region	*rp;
struct rt_vls		*matparm;
char			**dpp;	/* pointer to reg_udata in *rp */
struct mfuncs		*mfp;
struct rt_i		*rtip;	/* New since 4.4 release */
{
	register struct fire_specific	*fire_sp;
	mat_t	tmp;
	vect_t	bb_min, bb_max, v_tmp;

	/* check the arguments */
	RT_CHECK_RTI(rtip);
	RT_VLS_CHECK( matparm );
	RT_CK_REGION(rp);


	if( rdebug&RDEBUG_SHADE)
		rt_log("fire_setup(%s)\n", rp->reg_name);

	/* Get memory for the shader parameters and shader-specific data */
	GETSTRUCT( fire_sp, fire_specific );
	*dpp = (char *)fire_sp;

	/* initialize the default values for the shader */
	memcpy(fire_sp, &fire_defaults, sizeof(struct fire_specific) );

	/* parse the user's arguments for this use of the shader. */
	if( rt_structparse( matparm, fire_parse_tab, (char *)fire_sp ) < 0 )
		return(-1);

	/* Optional:
	 *
	 * If the shader needs to operate in a coordinate system which stays
	 * fixed on the region when the region is moved (as in animation)
	 * we need to get a matrix to perform the appropriate transform(s).
	 */

	db_shader_mat(fire_sp->fire_m_to_sh, rtip->rti_dbip, rp);

	/* XXX build matrix to map shader space to noise space */
	mat_idn(fire_sp->fire_sh_to_noise);
	MAT_DELTAS_VEC(fire_sp->fire_sh_to_noise, fire_sp->noise_delta);
	MAT_SCALE_VEC(fire_sp->fire_sh_to_noise, fire_sp->noise_scale);

	/* get matrix for performing spline of fire colors */
	rt_dspline_matrix(fire_sp->fire_colorspline_mat, "Catmull", 0.5);



	if( rdebug&RDEBUG_SHADE || fire_sp->fire_debug ) {
		rt_structprint( " FIRE Parameters:", fire_print_tab, (char *)fire_sp );
		mat_print( "m_to_sh", fire_sp->fire_m_to_sh );
		mat_print( "sh_to_noise", fire_sp->fire_sh_to_noise );
		mat_print( "colorspline", fire_sp->fire_colorspline_mat );
	}

	return(1);
}

/*
 *	F I R E _ P R I N T
 */
HIDDEN void
fire_print( rp, dp )
register struct region *rp;
char	*dp;
{
	rt_structprint( rp->reg_name, fire_print_tab, (char *)dp );
}

/*
 *	F I R E _ F R E E
 */
HIDDEN void
fire_free( cp )
char *cp;
{
	rt_free( cp, "fire_specific" );
}

/*
 *	F I R E _ R E N D E R
 *
 *	This is called (from viewshade() in shade.c) once for each hit point
 *	to be shaded.  The purpose here is to fill in values in the shadework
 *	structure.
 */
int
fire_render( ap, pp, swp, dp )
struct application	*ap;
struct partition	*pp;
struct shadework	*swp;	/* defined in material.h */
char			*dp;	/* ptr to the shader-specific struct */
{
	register struct fire_specific *fire_sp =
		(struct fire_specific *)dp;
	point_t t_i_pt;
	point_t t_o_pt;
	point_t	m_i_pt, m_o_pt;	/* model space in/out points */
	point_t sh_i_pt, sh_o_pt;	/* shader space in/out points */
	point_t noise_i_pt, noise_o_pt;	/* shader space in/out points */
	point_t noise_pt;
	point_t	color;
	vect_t	noise_r_dir;
	double	noise_r_thick;
	int	i;
	double	samples_per_unit_noise;
	double	noise_dist_per_sample;
	point_t	shader_pt;
	vect_t	shader_r_dir;
	double	shader_r_thick;
	double	shader_dist_per_sample;

	int	samples;
	double	dist;
	double	noise_val;
	double	lumens;

	/* check the validity of the arguments we got */
	RT_AP_CHECK(ap);
	RT_CHECK_PT(pp);
	CK_fire_SP(fire_sp);

	if( rdebug&RDEBUG_SHADE || fire_sp->fire_debug ) {
/*		rt_structprint( "fire_render Parameters:", fire_print_tab, (char *)fire_sp ); */
		rt_log("fire_render()\n");
	}
	/* If we are performing the shading in "region" space, we must 
	 * transform the hit point from "model" space to "region" space.
	 * See the call to db_region_mat in fire_setup().
	 */

	/*
	 * Compute the ray/solid in and out points,
	 */
	VMOVE(m_i_pt, swp->sw_hit.hit_point);
	VJOIN1(m_o_pt, ap->a_ray.r_pt, pp->pt_outhit->hit_dist, ap->a_ray.r_dir);

	if( rdebug&RDEBUG_SHADE || fire_sp->fire_debug ) {
		rt_log("fire_render() model space \n");
		rt_log("fire_render() m_i_pt(%g %g %g)\n", V3ARGS(m_i_pt) );
		rt_log("fire_render() m_o_pt(%g %g %g)\n", V3ARGS(m_o_pt) );
	}

	/* map points into shader space */
	MAT4X3PNT(sh_i_pt, fire_sp->fire_m_to_sh, m_i_pt);
	MAT4X3PNT(sh_o_pt, fire_sp->fire_m_to_sh, m_o_pt);

	if( rdebug&RDEBUG_SHADE || fire_sp->fire_debug ) {
		rt_log("fire_render() shader space \n");
		rt_log("fire_render() sh_i_pt(%g %g %g)\n", V3ARGS(sh_i_pt) );
		rt_log("fire_render() sh_o_pt(%g %g %g)\n", V3ARGS(sh_o_pt) );
	}

	/* apply exponential stretch to shader space */
	VMOVE(t_i_pt, sh_i_pt);
	VMOVE(t_o_pt, sh_o_pt);
	if ( ! NEAR_ZERO(fire_sp->fire_stretch, SQRT_SMALL_FASTF) ) {
		t_i_pt[Z] = exp( (sh_i_pt[Z]+0.125) * -fire_sp->fire_stretch );
		t_o_pt[Z] = exp( (sh_o_pt[Z]+0.125) * -fire_sp->fire_stretch );
	}

	/* map shader space into noise space */
	MAT4X3PNT(noise_i_pt, fire_sp->fire_sh_to_noise, sh_i_pt);
	MAT4X3PNT(noise_o_pt, fire_sp->fire_sh_to_noise, sh_o_pt);

	if( rdebug&RDEBUG_SHADE || fire_sp->fire_debug ) {
		rt_log("fire_render() noise space \n");
		rt_log("fire_render() noise_i_pt(%g %g %g)\n", V3ARGS(noise_i_pt) );
		rt_log("fire_render() noise_o_pt(%g %g %g)\n", V3ARGS(noise_o_pt) );
	}

	/* map points into shader space (again)*/
	MAT4X3PNT(sh_i_pt, fire_sp->fire_m_to_sh, m_i_pt);
	MAT4X3PNT(sh_o_pt, fire_sp->fire_m_to_sh, m_o_pt);

	VSUB2(noise_r_dir, noise_o_pt, noise_i_pt);

	noise_r_thick = MAGNITUDE(noise_r_dir);

	if( rdebug&RDEBUG_SHADE || fire_sp->fire_debug ) {
		rt_log("fire_render() noise_r_dir (%g %g %g)\n",
			V3ARGS(noise_r_dir) );
		rt_log("fire_render() noise_r_thick %g\n", noise_r_thick);
	}


	/* compute number of samples per unit length in noise space.
	 *
	 * The noise field used by the noise_turb and noise_fbm routines
	 * has a maximum frequency of about 1 cycle per integer step in
	 * noise space.  Each octave increases this frequency by the
	 * "lacunarity" factor.  To sample this space adequately, nyquist
	 * tells us we need at least 4 samples/cycle at the highest octave
	 * rate.
	 */

	samples_per_unit_noise =
		pow(fire_sp->noise_lacunarity, fire_sp->noise_octaves-1) * 4.0;

	noise_dist_per_sample = 1.0 / samples_per_unit_noise;

	samples = samples_per_unit_noise * noise_r_thick;

	if (samples < 1) samples = 1;

	if( rdebug&RDEBUG_SHADE || fire_sp->fire_debug ) {
		rt_log("samples:%d\n", samples);
		rt_log("samples_per_unit_noise %g\n", samples_per_unit_noise);
		rt_log("noise_dist_per_sample %g\n", noise_dist_per_sample);
	}

	/* To do the exponential stretch and decay properly we need to 
	 * do the computations in shader space, and convert the points
	 * to noise space.  Performance pig.
	 *
	 */

	VSUB2(shader_r_dir, sh_o_pt, sh_i_pt);
	shader_r_thick = MAGNITUDE(shader_r_dir);
	VUNITIZE(shader_r_dir);

	shader_dist_per_sample = shader_r_thick / samples;

	lumens = 0.0;
	for (i = 0 ; i < samples ; i++) {
		dist = (double)i * shader_dist_per_sample;
		VJOIN1(shader_pt, sh_i_pt, dist, shader_r_dir);
		
		VMOVE(t_i_pt, shader_pt);
		if ( ! NEAR_ZERO(fire_sp->fire_stretch, SQRT_SMALL_FASTF) ) {
			t_i_pt[Z] = exp( (t_i_pt[Z]+0.125) * -fire_sp->fire_stretch );
		}
		
		/* map shader space into noise space */
		MAT4X3PNT(noise_pt, fire_sp->fire_sh_to_noise, t_i_pt);

		noise_val = noise_turb(noise_pt, fire_sp->noise_h_val,
			fire_sp->noise_lacunarity, fire_sp->noise_octaves);

		if( rdebug&RDEBUG_SHADE || fire_sp->fire_debug )
			rt_log("noise_turb(%g %g %g) = %g\n",
				V3ARGS(noise_pt),
				noise_val);

		/* XXX
		 * When doing the exponential stretch, we scale the noise
		 * value by the height in shader space
		 */

		if ( NEAR_ZERO(fire_sp->fire_stretch, SQRT_SMALL_FASTF) )
			lumens += noise_val * 0.025;
		else {
			register double t;
			t = lumens;
			lumens += noise_val * 0.025 *  (1.0 -shader_pt[Z]);
			if( rdebug&RDEBUG_SHADE || fire_sp->fire_debug )
				rt_log("lumens:%g = %g + %g * %g\n",
					lumens, t, noise_val,
					0.025 * (1.0 - shader_pt[Z]) );

		}
		if (lumens >= 1.0) {
			lumens = 1.0;
			if( rdebug&RDEBUG_SHADE || fire_sp->fire_debug )
				rt_log("early exit from lumens loop\n");
			break;
		}

	}
	
	if( rdebug&RDEBUG_SHADE || fire_sp->fire_debug )
		rt_log("lumens = %g\n", lumens);

	if (lumens < 0.0) lumens = 0.0;
	else if (lumens > 1.0) lumens = 1.0;

	swp->sw_transmit = 1.0 - lumens;

	rt_dspline_n(color, fire_sp->fire_colorspline_mat, flame_colors,
		18, 3, lumens);

	VMOVE(swp->sw_color, color);
	VSETALL(swp->sw_basecolor, 1.0);

	/* caller will perform transmission/reflection calculations
	 * based upon the values of swp->sw_transmit and swp->sw_reflect
	 *
	 * 0 < swp->sw_transmit <= 1 causes transmission computations
	 * 0 < swp->sw_reflect <= 1 causes reflection computations
	 */

	return(1);
}
