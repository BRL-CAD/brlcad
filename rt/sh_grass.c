/*
 *	S H _ G R A S S . C
 *
 *  To add a new shader to the "rt" program:
 *
 *	1) Copy this file to sh_shadername.c
 *	2) edit sh_shadername.c:
 *		change "G R A S S" to "S H A D E R N A M E"
 *		change "grass"   to "shadername"
 *		Set a new number for the grass_MAGIC define
 *		define shader specific structure and defaults
 *		edit/build parse table for bu_structparse from grass_parse
 *		edit/build shader_mfuncs tables from grass_mfuncs for
 *			each shader name being built.
 *		edit the grass_setup function to do shader-specific setup
 *		edit the grass_render function to do the actual rendering
 *	3) Edit view.c to add extern for grass_mfuncs and call to mlib_add
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
#include "./light.h"
#include "./mathtab.h"
#include "./rdebug.h"

/* #define SHADOWS 0*/
#define RI_AIR		1.0		/* Refractive index of air */

#define grass_MAGIC 0x9    /* make this a unique number for each shader */
#define CK_grass_SP(_p) RT_CKMAG(_p, grass_MAGIC, "grass_specific")

/*
 * the shader specific structure contains all variables which are unique
 * to any particular use of the shader.
 */
struct grass_specific {
	long	magic;	/* magic # for memory validity check, must come 1st */
	double	lacunarity;
	double	h_val;
	double	octaves;
	double	size;
	point_t	vscale;	/* size of noise coordinate space */
	vect_t	delta;
	double	grass_ls;	/* lean/tilt scaling */
	double	grass_radius;
	double	grass_norm;	/* variables for shader ... */
	double	grass_thresh;	/* variables for shader ... */
	vect_t	grass_delta;	/* offset into noise space */
	point_t grass_min;
	point_t grass_max;
	mat_t	m_to_r;	/* model to region space matrix */
	mat_t	r_to_m;	/* region to model space matrix */
	double	inv_size;
};

/* The default values for the variables in the shader specific structure */
CONST static
struct grass_specific grass_defaults = {
	grass_MAGIC,
	2.1753974,	/* lacunarity */
	1.0,		/* h_val */
	4.0,		/* octaves */
	1.0,		/* size */
	{ 1.0, 1.0, 1.0 },	/* vscale */
	{ 1000.0, 1000.0, 1000.0 },	/* delta into noise space */
	3.141592653,			/* grass_ls */
	1.0,				/* grass_radius */
	0.0,				/* grass_norm */
	1.0,				/* grass_thresh */
	{ 1.0, 1.0, 1.0 },		/* grass_delta */
	{ 0.0, 0.0, 0.0 },		/* grass_min */
	{ 0.0, 0.0, 0.0 },		/* grass_max */
	{	1.0, 0.0, 0.0, 0.0,	/* grass_m_to_sh */
		0.0, 1.0, 0.0, 0.0,
		0.0, 0.0, 1.0, 0.0,
		0.0, 0.0, 0.0, 1.0 }
	};

#define SHDR_NULL	((struct grass_specific *)0)
#define SHDR_O(m)	offsetof(struct grass_specific, m)
#define SHDR_AO(m)	offsetofarray(struct grass_specific, m)


/* description of how to parse/print the arguments to the shader
 * There is at least one line here for each variable in the shader specific
 * structure above
 */
struct bu_structparse grass_print_tab[] = {
	{"%f",	1, "lacunarity",	SHDR_O(lacunarity),	FUNC_NULL },
	{"%f",	1, "H", 		SHDR_O(h_val),	FUNC_NULL },
	{"%f",	1, "octaves", 		SHDR_O(octaves),	FUNC_NULL },
	{"%f",  1, "size",		SHDR_O(size),	FUNC_NULL },
	{"%f",  1, "1/size",		SHDR_O(inv_size),	FUNC_NULL },
	{"%f",  1, "lean",		SHDR_O(grass_ls),	FUNC_NULL },
	{"%f",  1, "radius",		SHDR_O(grass_radius),	FUNC_NULL },
	{"%f",  1, "norm",		SHDR_O(grass_norm),	FUNC_NULL },
	{"%f",  1, "thresh",		SHDR_O(grass_thresh),	FUNC_NULL },
	{"%f",  3, "delta",		SHDR_AO(grass_delta),	FUNC_NULL },
	{"%f",  3, "max",		SHDR_AO(grass_max),	FUNC_NULL },
	{"%f",  3, "min",		SHDR_AO(grass_min),	FUNC_NULL },
	{"%f",  3, "delta",		SHDR_AO(grass_delta),	FUNC_NULL },
	{"%f",  16, "m_to_r",		SHDR_AO(m_to_r),	FUNC_NULL },
	{"",	0, (char *)0,		0,			FUNC_NULL }

};
struct bu_structparse grass_parse_tab[] = {
	{"i",	bu_byteoffset(grass_print_tab[0]), "grass_print_tab", 0, FUNC_NULL },
	{"%f",	1, "l",			SHDR_O(lacunarity),	FUNC_NULL },
	{"%f",	1, "o", 		SHDR_O(octaves),	FUNC_NULL },
	{"%f",  1, "s",			SHDR_O(size),	FUNC_NULL },
	{"%f",  1, "ls",		SHDR_O(grass_ls),	FUNC_NULL },
	{"%f",  1, "r",			SHDR_O(grass_radius),	FUNC_NULL },
	{"%f",  1, "n",			SHDR_O(grass_norm),	FUNC_NULL },
	{"%f",  1, "t",			SHDR_O(grass_thresh),	FUNC_NULL },
	{"%f",  3, "d",			SHDR_AO(grass_delta),	FUNC_NULL },
	{"",	0, (char *)0,		0,			FUNC_NULL }
};

HIDDEN int	grass_setup(), grass_render();
HIDDEN void	grass_print(), grass_free();

/* The "mfuncs" structure defines the external interface to the shader.
 * Note that more than one shader "name" can be associated with a given
 * shader by defining more than one mfuncs struct in this array.
 * See sh_phong.c for an example of building more than one shader "name"
 * from a set of source functions.  There you will find that "glass" "mirror"
 * and "plastic" are all names for the same shader with different default
 * values for the parameters.
 */
struct mfuncs grass_mfuncs[] = {
	{"grass",	0,	0, MFI_NORMAL|MFI_HIT|MFI_UV,	MFF_PROC,
	grass_setup,	grass_render,	grass_print,	grass_free },

	{(char *)0,	0,		0,		0,		0,
	0,		0,		0,		0 }
};


/*	G R A S S _ S E T U P
 *
 *	This routine is called (at prep time)
 *	once for each region which uses this shader.
 *	Any shader-specific initialization should be done here.
 */
HIDDEN int
grass_setup( rp, matparm, dpp, mfp, rtip)
register struct region	*rp;
struct rt_vls		*matparm;
char			**dpp;	/* pointer to reg_udata in *rp */
struct mfuncs		*mfp;
struct rt_i		*rtip;	/* New since 4.4 release */
{
	register struct grass_specific	*grass_sp;
	mat_t	tmp, mtr;
	vect_t	bb_min, bb_max, v_tmp;

	/* check the arguments */
	RT_CHECK_RTI(rtip);
	RT_VLS_CHECK( matparm );
	RT_CK_REGION(rp);


	if( rdebug&RDEBUG_SHADE)
		rt_log("grass_setup(%s)\n", rp->reg_name);

	/* Get memory for the shader parameters and shader-specific data */
	GETSTRUCT( grass_sp, grass_specific );
	*dpp = (char *)grass_sp;

	/* initialize the default values for the shader */
	memcpy(grass_sp, &grass_defaults, sizeof(struct grass_specific) );

	if (rp->reg_aircode == 0) {
		rt_log("%s\n%s\n",
		"*** WARNING: grass shader applied to non-air region!!! ***",
		"Set air flag with \"edcodes\" in mged");
		rt_bomb("");
	}

	/* parse the user's arguments for this use of the shader. */
	if( bu_struct_parse( matparm, grass_parse_tab, (char *)grass_sp ) < 0 )
		return(-1);

	/* We're going to operate in Region space so we can extract known
	 * distances and sizes in mm for the shader calculations.
	 */
	db_region_mat(mtr, rtip->rti_dbip, rp->reg_name);	 


#if 0
	mat_idn(tmp);
	if (grass_sp->size != 1.0) {
		/* the user sets "size" to the size of the biggest
		 * noise-space blob in model coordinates
		 */
		tmp[0] = tmp[5] = tmp[10] = 1.0/grass_sp->size;
	} else {
		tmp[0] = 1.0/grass_sp->vscale[0];
		tmp[5] = 1.0/grass_sp->vscale[1];
		tmp[10] = 1.0/grass_sp->vscale[2];
	}

	mat_mul(grass_sp->m_to_r, tmp, mtr);

	/* Add any translation within shader/region space */
	mat_idn(tmp);
	tmp[MDX] = grass_sp->delta[0];
	tmp[MDY] = grass_sp->delta[1];
	tmp[MDZ] = grass_sp->delta[2];
	mat_mul2(tmp, grass_sp->m_to_r);
#else
	mat_copy(grass_sp->m_to_r, mtr);

	grass_sp->inv_size = 1.0 / grass_sp->size;
#endif

	mat_inv(grass_sp->r_to_m, grass_sp->m_to_r);

	if( rdebug&RDEBUG_SHADE) {
		bu_struct_print( " Parameters:", grass_print_tab, (char *)grass_sp );
		mat_print( "m_to_r", grass_sp->m_to_r );
	}

	return(1);
}

/*
 *	G R A S S _ P R I N T
 */
HIDDEN void
grass_print( rp, dp )
register struct region *rp;
char	*dp;
{
	bu_struct_print( rp->reg_name, grass_print_tab, (char *)dp );
}

/*
 *	G R A S S _ F R E E
 */
HIDDEN void
grass_free( cp )
char *cp;
{
	rt_free( cp, "grass_specific" );
}

/*
 *	Actual ray/blade intersection
 *
 */
static int
frob(in, dir, p2, ap, swp, grass_sp, radius, t, out_dist)
point_t in; /* in point in the solid */
vect_t dir; /* ray direction */
point_t p2; /* grid point to test for grass */
struct application *ap;
struct shadework *swp;
struct grass_specific *grass_sp;
double radius;	/* radius of ray at "in" */
double t;	/* distance from "in" along "dir" to curr grid pt */
double out_dist;/* distance to solid exit */
{
	point_t PCA1, PCA2;	/* point of closest approach for each line */
	double ldist[2];	/* dist along each line to PCA */
	vect_t v;
	vect_t stalk;
	double val;
	double alt;
	double radius_at_PCA;
	point_t npt;
	double dist;
	double grass_diameter;
	point_t tmp;
	CONST static vect_t d2 = { 0.0, 0.0, 1.0};
	register struct light_specific *lp;
	extern struct light_specific LightHead;
	vect_t	tolight;	

	grass_diameter = grass_sp->grass_radius /* mm */ * grass_sp->size;

	/* Tilt the vertical stalk by a (repeatable) random amount.  Make sure
	 * that the Z value is the largest.
	 */
	VSCALE(tmp, p2, grass_sp->grass_ls);
	bn_noise_vec(tmp, stalk);
	if( rdebug&RDEBUG_SHADE) {
		bu_log("frob: %g %g %g scaled to %g %g %g\n",
			V3ARGS(p2), V3ARGS(tmp));
		bu_log("became %g %g %g\n", V3ARGS(stalk));
	}

	if (stalk[X] > stalk[Z]) { val = stalk[Z]; stalk[Z] = stalk[X]; stalk[X] = val; }
	if (stalk[Y] > stalk[Z]) { val = stalk[Z]; stalk[Z] = stalk[Y]; stalk[Y] = val; }

	/* The following assures we can make unit vector,
	 * and that the grass doesn't lean over too much
	 */
	stalk[Z] += 2.0;
	VUNITIZE(stalk);


	/*
	 * intersect ray with vertical stalk
	 */
	if (rt_dist_line3_line3(ldist, in, dir, p2, stalk,
	    &ap->a_rt_i->rti_tol) < 0)
		rt_bomb("line/line isect error\n");

#if 1
	/* Noise function to define height of grass blades */
	VSCALE(tmp, p2, grass_sp->grass_thresh);
	val = bn_noise_fbm(tmp, grass_sp->h_val,
		grass_sp->lacunarity, grass_sp->octaves);

#else
	val = .4;
#endif


#define SOLID_HEIGHT 500.0 /* XXX hack.  Known height of solid in mm */
	alt = val * (SOLID_HEIGHT * grass_sp->size);

	/* compute Pt of closest approach along ray */
	VJOIN1(PCA1, in, ldist[0], dir);
	radius_at_PCA = radius + ldist[0] * ap->a_diverge;

	if( rdebug&RDEBUG_SHADE) {
		bu_log("\tgrass_dia: %g (%g)\n", grass_diameter);
		bu_log("\tval:%g  alt:%g\n", val, alt);
		bu_log("\tdist to ray PCA=%g PCA1 %g %g %g radius@PCA:%g\n",
			ldist[0], V3ARGS(PCA1), radius_at_PCA);
	}


	/* if the grass is too short, we miss and march onward */
	if ( (PCA1[Z] - radius_at_PCA) >= alt) {
		if( rdebug&RDEBUG_SHADE)
			bu_log("\tPCA1(%g) > alt(%g) of grass + r@PCA\n", PCA1[Z], alt);
		return 0;
	}

	/* if we hit the grass below the ground, forget it and 
	 * signal caller to quit cell-marching
	 */
	if ( (PCA1[Z] + radius_at_PCA) < 0.0) {
		if( rdebug&RDEBUG_SHADE)
			bu_log("\tPCA1(%g) < ground\n", PCA1[Z]);
		return -1;
	}


	/* compute dist between Points of closest approach */
	VJOIN1(PCA2, p2, ldist[1], stalk);
	VSUB2(v, PCA1, PCA2);
	dist = MAGNITUDE(v);

	if( rdebug&RDEBUG_SHADE) {
		bu_log("\tdist to grass PCA=%g PCA2 %g %g %g\n",
			ldist[1], V3ARGS(PCA2));
		bu_log("\tdelta btw PCA pts:%g  beam radius:%g\n",
			dist, radius);
	}


	/* if the ray radius isn't greater than the distance to the blade then
	 *  we've missed, march onward
	 */
	if (dist > (radius_at_PCA + grass_diameter)  &&
	    (radius_at_PCA + grass_diameter) < .75 )
		return 0;

	/* Ray overlaps blade of grass 
	 * XXX The ray might overlap several blades of grass.  We should do
	 * more here.  Perhaps based upon the area of overlap vs ray footprint
	 * area?
	 */
	swp->sw_transmit = 0.0;

	/* if this is a shadow ray, then just register opacity and quit.
	 * XXX Once again some indication of partial transmittance might be
	 * nice to add.  Grass tends to be translucent at short distances.
	 */
 	if( swp->sw_xmitonly ) return -1;

	/* set the color
	 * We scale the color based upon the height of the hitpoint to
	 * approximate basic shadowing.  Gross hack XXX
	 */
	val = (PCA1[Z]/alt) * 0.5 + 0.5;

	VSCALE(swp->sw_color, swp->sw_basecolor, val );

#if 1
	/* compute normal for stalk */

	for( RT_LIST_FOR( lp, light_specific, &(LightHead.l) ) )  {
		RT_CK_LIGHT(lp);
		/* compute the light direction */
		if( lp->lt_infinite ) {
			/* Infinite lights are point sources, no fuzzy penumbra */
			VMOVE( tolight, lp->lt_vec );
		} else {
			/* convert light position into shader space */
			VSCALE(tmp, lp->lt_pos, grass_sp->size);

			VSUB2(tolight, tmp, 
				swp->sw_hit.hit_point);
		}
		if (lp->lt_fraction > 0.6) break;
	}

	VCROSS(tmp, stalk, d2);
	VCROSS(v, tmp, stalk);
	VUNITIZE(v);	/* tentative normal for stalk */
	VUNITIZE(tolight);
	if (VDOT(v, tolight)  > 0.0) {
		VMOVE(swp->sw_hit.hit_normal, v);
	} else {
		VREVERSE(swp->sw_hit.hit_normal, v);
	}
#else
	/* Compute a normal for the blade of grass based upon
	 * the blade's 2D position
	 */
	PCA2[Z] = 0.0;

	if (grass_sp->grass_norm != 0.0) {
		/* hack for manipulating grass normal */
		VSCALE(PCA2, PCA2, grass_sp->grass_norm);

		do {
			VSCALE(PCA2, PCA2, grass_sp->lacunarity);
			bn_noise_vec(PCA2, v);
		} while (VDOT(v, dir) >= -0.2);

		VUNITIZE(v);

		VSCALE(PCA2, PCA2, grass_sp->lacunarity);
		bn_noise_vec(PCA2, tmp);

		VUNITIZE(tmp);
		VSCALE(tmp, tmp, 0.1736);
		VADD2(v, v, tmp);

		VUNITIZE(v);
		VMOVE(swp->sw_hit.hit_normal, v);
	} else {
		bn_noise_vec(PCA2, v);
		if (VDOT(v, dir) > 0.0) {
			VREVERSE(swp->sw_hit.hit_normal, v);
		} else {
			VMOVE(swp->sw_hit.hit_normal, v);
		}
	}
	VUNITIZE(swp->sw_hit.hit_normal);
#endif

	return 1;
}






/*
 *	G R A S S _ R E N D E R
 *
 *	This is called (from viewshade() in shade.c) once for each hit point
 *	to be shaded.  The purpose here is to fill in values in the shadework
 *	structure.
 */
int
grass_render( ap, pp, swp, dp )
struct application	*ap;
struct partition	*pp;
struct shadework	*swp;	/* defined in material.h */
char			*dp;	/* ptr to the shader-specific struct */
{
	register struct grass_specific *grass_sp =
		(struct grass_specific *)dp;
	point_t in_pt, out_pt;	/* model space in/out points */
	point_t in, out, next_pt;
	double in_pt_radius, out_pt_radius;	/* beam radius, model space */
	double in_radius, out_radius;	/* beam radius, shader space */
	double radius;
	double out_dist;
	double	tDX;		/* dist along ray to span 1 cell in X dir */
	double	tDY;		/* dist along ray to span 1 cell in Y dir */
	double	tX, tY;	/* dist along ray from hit pt. to next cell boundary */
#define PREV_X	3
#define PREV_Y	7
#define PREV_NONE 0
	int 	step_prev = PREV_NONE;
	double	which_x;
	double	which_y;
	double	ldist[2];
	vect_t	dir;
	point_t	p2;
	int i;
	double	dist;
	int hit;
	int iter;

	/* check the validity of the arguments we got */
	RT_AP_CHECK(ap);
	RT_CHECK_PT(pp);
	CK_grass_SP(grass_sp);

	/* set some fast constants */
	VMOVE(dir, ap->a_ray.r_dir);
	p2[Z] = 0.0;


	if( rdebug&RDEBUG_SHADE) bu_log("grass_render\n");

#if !defined(SHADOWS)
	if (swp->sw_xmitonly) {
		/* XXX hack, cast no shadows */
		VSETALL(swp->sw_basecolor, 1.0); 
		swp->sw_transmit = 1.0;

		if( rdebug&RDEBUG_SHADE) bu_log("XXX HACK no grass shadows\n");

		return 0;	/* don't do phong shading on light vis rays */
	}
#endif
	/* figure out the in/out points, and the radius of the beam
	 * We can work in model space since grass isn't likely to be moving
	 * around the scene.
	 */
	VJOIN1(in_pt, ap->a_ray.r_pt, swp->sw_hit.hit_dist, dir);
	in_pt_radius = ap->a_rbeam + swp->sw_hit.hit_dist * ap->a_diverge;

	VJOIN1(out_pt, ap->a_ray.r_pt, pp->pt_outhit->hit_dist, dir);
	out_pt_radius = ap->a_rbeam + pp->pt_outhit->hit_dist * ap->a_diverge;

	/* We usually can't afford to evaluate the shader every mm so we allow
	 * the user to scale space by a constant to reduce the density of
	 * the grass and the number of computations needed.
	 *
	 * We get a new "in" and "out points and a new radius at the "in"
	 * point in this new coordiante system.
	 */
	VSCALE(in, in_pt, grass_sp->size);
	VSCALE(out, out_pt, grass_sp->size);
 	in_radius = in_pt_radius * grass_sp->size;

	/* out distance computed from "in" hit point */
	out_dist = (pp->pt_outhit->hit_dist - swp->sw_hit.hit_dist) *
			 grass_sp->size;

	if( rdebug&RDEBUG_SHADE) {
		bu_log("\tPnt: %g %g %g\n", V3ARGS(ap->a_ray.r_pt));
		bu_log("\tDir: %g %g %g (diverge:%g)\n", V3ARGS(ap->a_ray.r_dir), ap->a_diverge);
		bu_log("\tHit: %g %g %g  (dist:%g) (radius:%g)\n",
			V3ARGS(swp->sw_hit.hit_point), swp->sw_hit.hit_dist,
			in_pt_radius);
		bu_log("\tOut: %g %g %g  (dist:%g) (radius:%g)\n",
			V3ARGS(out_pt), pp->pt_outhit->hit_dist,
			out_pt_radius);
		bu_log("\t in: %g %g %g (dist:%g) r:%g\n", V3ARGS(in), 
			swp->sw_hit.hit_dist * grass_sp->size, in_radius);

		bu_log("\tout: %g %g %g (dist%g)\n", V3ARGS(out), out_dist);

		bu_struct_print("Parameters:", grass_print_tab, (char *)grass_sp );
	 	if( swp->sw_xmitonly ) bu_log("xmit only\n");

	}


	/* tDX is the distance along the ray we have to travel
	 * to traverse a cell (travel a unit distance) along the
	 * X axis of the grid.
	 *
	 * tX is the distance along the ray from the initial hit point
	 * to the first cell boundary in the X direction
	 */
	if (dir[X] < 0.0) {
		tDX = -1.0 / dir[X];
		tX = (in[X] - ((int)in[X])) / dir[X];
		which_x = 0.0;
	} else {
		tDX = 1.0 / dir[X];
		if (dir[X] > 0.0)
			tX = (1.0 - (in[X] - ((int)in[X]))) / dir[X];
		else
			tX = MAX_FASTF;
		which_x = 1.0;
	}
	if (tX > out_dist) tX = out_dist;


	/* tDY is the distance along the ray we have to travel
	 * to traverse a cell (travel a unit distance) along the
	 * Y axis of the grid
	 *
	 * tY is the distance along the ray from the initial hit point
	 * to the first cell boundary in the Y direction
	 */
	if (dir[Y] < 0.0) {
		tDY = -1.0 / dir[Y];
		tY = (in[Y] - ((int)in[Y])) / dir[Y];
		which_y = 0.0;
	} else {
		tDY = 1.0 / dir[Y];
		if (dir[Y] > 0.0)
			tY = (1.0 - (in[Y] - ((int)in[Y]))) / dir[Y];
		else
			tY = MAX_FASTF;
		which_y = 1.0;
	}
	if (tY > out_dist) tY = out_dist;

	/* time to go marching through the noise space */
	if( rdebug&RDEBUG_SHADE) {
		bu_log("tX %g  tDX %g\n", tX, tDX);
		bu_log("tY %g  tDY %g\n", tY, tDY);
	}
	hit = 0;
	iter = 0;
	swp->sw_transmit = 1.0;
	while (! hit && (tX < out_dist || tY < out_dist)) {
		if (tX < tY) {
			/* one step in X direction */
			VJOIN1(next_pt, in, tX, dir);
			p2[X] = floor(next_pt[X]);

			if (step_prev != PREV_Y) {
				/* check (int)y and (int)y+1.0 XXX use floor()*/
				p2[Y] = floor(next_pt[Y]);

				if( rdebug&RDEBUG_SHADE)
					bu_log("tX(%g) pt %g %g %g (check 2)\n\tp2=%g,%g,%g\n",
						tX, V3ARGS(next_pt), V3ARGS(p2));

				hit = frob(in, dir, p2, ap, swp, 
					grass_sp, in_radius, tX, out_dist);
				if (hit) { 
					swp->sw_hit.hit_dist += 
						tX * grass_sp->inv_size;
					VMOVE(swp->sw_hit.hit_point, next_pt);
					break;
				}

				p2[Y] += 1.0;
				if( rdebug&RDEBUG_SHADE)
					bu_log("\tp2=%g,%g,%g\n", V3ARGS(p2));

				hit = frob(in, dir, p2, ap, swp,
					grass_sp, in_radius, tX, out_dist);
				if (hit) { 
					swp->sw_hit.hit_dist += 
						tX * grass_sp->inv_size;
					VMOVE(swp->sw_hit.hit_point, next_pt);
					break;
				}
			} else {
				/* check only the rayward one */
				p2[Y] = floor(next_pt[Y]+which_y);

				if( rdebug&RDEBUG_SHADE)
					bu_log("tX(%g) pt %g %g %g (check 1)\n\tp2=%g,%g,%g\n",
						tX, V3ARGS(next_pt), V3ARGS(p2));

				hit = frob(in, dir, p2, ap, swp,
					grass_sp, in_radius, tX, out_dist);
				if (hit) { 
					swp->sw_hit.hit_dist += 
						tX * grass_sp->inv_size;
					VMOVE(swp->sw_hit.hit_point, next_pt);
					break;
				}
			}
			step_prev = PREV_X;
			tX += tDX;
		} else {
			/* one step in Y direction */
			VJOIN1(next_pt, in, tY, dir);
			p2[Y] = floor(next_pt[Y]);

			if (step_prev != PREV_X) {
				/* check (int)x and (int)x+1.0 */
				p2[X] = floor(next_pt[X]);

				if( rdebug&RDEBUG_SHADE)
					bu_log("tY(%g) pt %g %g %g (check 2)\n\tp2=%g,%g,%g\n",
						tY, V3ARGS(next_pt),
						V3ARGS(p2));

				hit = frob(in, dir, p2, ap, swp, 
					grass_sp, in_radius, tY, out_dist);
				if (hit) { 
					if( rdebug&RDEBUG_SHADE)
						bu_log("A d:%g tY:%g inv:%g\n",
							swp->sw_hit.hit_dist,
							tY, grass_sp->inv_size);
					swp->sw_hit.hit_dist += 
						tY * grass_sp->inv_size;
					VMOVE(swp->sw_hit.hit_point, next_pt);
					break;
				}
				p2[X] += 1.0;
				if( rdebug&RDEBUG_SHADE)
					bu_log("\tp2=%g,%g,%g\n", V3ARGS(p2));

				hit = frob(in, dir, p2, ap, swp, 
					grass_sp, in_radius, tY, out_dist);
				if (hit) { 
					if( rdebug&RDEBUG_SHADE)
						bu_log("B d:%g tY:%g inv:%g\n",
							swp->sw_hit.hit_dist,
							tY, grass_sp->inv_size);
					swp->sw_hit.hit_dist += 
						tY * grass_sp->inv_size;
					VMOVE(swp->sw_hit.hit_point, next_pt);
					break;
				}
			} else {
				/* check only the rayward one */
				p2[X] = floor(next_pt[X]+which_x);

				if( rdebug&RDEBUG_SHADE)
					bu_log("tY(%g) pt %g %g %g (check 1)\n\tp2=%g,%g,%g\n",
						tY, V3ARGS(next_pt), V3ARGS(p2));

				hit = frob(in, dir, p2, ap, swp, 
					grass_sp, in_radius, tY, out_dist);
				if (hit) { 
					if( rdebug&RDEBUG_SHADE)
						bu_log("d:%g tY:%g inv:%g\n",
							swp->sw_hit.hit_dist,
							tY, grass_sp->inv_size);
					swp->sw_hit.hit_dist += 
						tY * grass_sp->inv_size;
					if( rdebug&RDEBUG_SHADE)
						bu_log("new d:%g\n",
							swp->sw_hit.hit_dist);

					VMOVE(swp->sw_hit.hit_point, next_pt);
					break;
				}
			}
			step_prev = PREV_Y;
			tY += tDY;
		}
	}


	if (hit > 0) {
		if( rdebug&RDEBUG_SHADE)
			bu_log("Hit grass blade\n");

		/* RE-SCALE shader hit point and distance back into model
		 * coordinate system.
		 */
		VJOIN1(swp->sw_hit.hit_point, ap->a_ray.r_pt, 
			swp->sw_hit.hit_dist, ap->a_ray.r_dir);

		if( rdebug&RDEBUG_SHADE)
			bu_log("new hit point: %g %g %g (dist:%g)\n",
				V3ARGS(swp->sw_hit.hit_point),
				swp->sw_hit.hit_dist);

		return 1;
	}

#if defined(SHADOWS)
	/* XXX here is where we should do the procedural shadows */
	/* seems to have something to do with shadow rays through grass */
	if (hit < 0) {
		/* hit grass, xmitonly case */
		return 1;
	}
#endif

	/* If we missed everything, then it's time to trace on through */

	if( rdebug&RDEBUG_SHADE)
		bu_log("Missed grass blades\n");


	/* setting basecolor to 1.0 prevents "filterglass" effect */
	VSETALL(swp->sw_basecolor, 1.0); 
	swp->sw_transmit = 1.0;
	(void)rr_render( ap, pp, swp );

	return 0;
}
