/*
 *	S H _ G R A S S . C
 *
 *  This shader mimics certain characteristics of grass.
 *
 *  XXX At the moment, this shader assumes that the solid is
 *  a slab from 0.0 to .5m in height.
 *
 *  Parameters:
 *	h/height  space scaling factor for noise call to get height of grass
 *	f/func	  one of either "fbm" or "tur" for the noise function for 
 *			grass height.
 *	t/tilt	  space scaling factor for noise call to get tilt of blades
 *	s/size	  space scaling factor for grid (how small are the cells
 *			between grass blades.
 *	r/radius  size/width of a blade of grass
 *
 *
 *
 *	The usual noise parameters:
 *	   Lacunarity, octaves, h_val, 
 *
 *	Present, but unused at the moment:
 *	   delta, max, min
 *
 *
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

#define PREV_X	3
#define PREV_Y	7
#define PREV_NONE 0

#define grass_MAGIC 0x9    /* make this a unique number for each shader */
#define CK_grass_SP(_p) RT_CKMAG(_p, grass_MAGIC, "grass_specific")

struct grass_ray {
	long		magic;
	struct xray	r;
	struct bn_tol	*tol;
	fastf_t		radius;
	fastf_t		diverge;
};

struct grass_hit {
	struct hit	h;
	vect_t		stalk;
	point_t		root;
	vect_t		normal;
	double		len;
	double		alt;
	double		noise;
};

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
	double	tilt_scale;		/* lean/tilt noise scaling */
	double	height_scale;	/* grass length noise scaling */
	double	grass_radius;	/* "diameter" of grass blades */
	vect_t	grass_delta;	/* offset into noise space */
	point_t grass_min;
	point_t grass_max;
	char	hfunc[4];	/* func to call for noise */
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
	3.141592653,			/* tilt_scale */
	1.0,				/* height_scale */
	1.0,				/* grass_radius */
	{ 1.0, 1.0, 1.0 },		/* grass_delta */
	{ 0.0, 0.0, 0.0 },		/* grass_min */
	{ 0.0, 0.0, 0.0 },		/* grass_max */
	"fbm",				/* func */
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
	{"%f",  1, "tilt",		SHDR_O(tilt_scale),	FUNC_NULL },
	{"%f",  1, "height",		SHDR_O(height_scale),	FUNC_NULL },
	{"%f",  1, "radius",		SHDR_O(grass_radius),	FUNC_NULL },
	{"%f",  3, "delta",		SHDR_AO(grass_delta),	FUNC_NULL },
	{"%f",  3, "max",		SHDR_AO(grass_max),	FUNC_NULL },
	{"%f",  3, "min",		SHDR_AO(grass_min),	FUNC_NULL },
	{"%f",  3, "delta",		SHDR_AO(grass_delta),	FUNC_NULL },
	{"%s",	4, "func",		SHDR_AO(hfunc),		FUNC_NULL },
	{"%f",  16, "m_to_r",		SHDR_AO(m_to_r),	FUNC_NULL },
	{"",	0, (char *)0,		0,			FUNC_NULL }

};
struct bu_structparse grass_parse_tab[] = {
	{"i",	bu_byteoffset(grass_print_tab[0]), "grass_print_tab", 0, FUNC_NULL },
	{"%f",	1, "l",			SHDR_O(lacunarity),	FUNC_NULL },
	{"%f",	1, "o", 		SHDR_O(octaves),	FUNC_NULL },
	{"%f",  1, "s",			SHDR_O(size),	FUNC_NULL },
	{"%f",  1, "t",			SHDR_O(tilt_scale),	FUNC_NULL },
	{"%f",  1, "r",			SHDR_O(grass_radius),	FUNC_NULL },
	{"%f",  1, "h",			SHDR_O(height_scale),	FUNC_NULL },
	{"%f",  3, "d",			SHDR_AO(grass_delta),	FUNC_NULL },
	{"%s",	4, "f",			SHDR_AO(hfunc),		FUNC_NULL },
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

	/* check the arguments */
	RT_CHECK_RTI(rtip);
	RT_VLS_CHECK( matparm );
	RT_CK_REGION(rp);


	if( rdebug&RDEBUG_SHADE)
		bu_log("grass_setup(%s)\n", rp->reg_name);

	/* Get memory for the shader parameters and shader-specific data */
	GETSTRUCT( grass_sp, grass_specific );
	*dpp = (char *)grass_sp;

	/* initialize the default values for the shader */
	memcpy(grass_sp, &grass_defaults, sizeof(struct grass_specific) );

	if (rp->reg_aircode == 0) {
		bu_log("%s\n%s\n",
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
	db_region_mat(grass_sp->m_to_r, rtip->rti_dbip, rp->reg_name);

	grass_sp->inv_size = 1.0 / grass_sp->size;

	mat_inv(grass_sp->r_to_m, grass_sp->m_to_r);

	grass_sp->grass_radius *= grass_sp->size;

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



/* Make sure the Z value of a vector is larger than X and Y */
#define VECT_UP(v) { register double t; \
	if (v[X] > v[Y]) { \
		if (v[X] > v[Z]) { \
			t = v[X]; v[X] = v[Z]; v[Z] = t; \
		} \
	} else { \
		if (v[Y] > v[Z]) { \
			t = v[Y]; v[Y] = v[Z]; v[Z] = t; \
		} \
	} \
}

/*
 *	Intersect ray with grass at one grid crossing.
 *
 *	Return:
 *		0	No hit, no changes
 *		1	Hit grass, struct "h" filled in.
 */
static int
grass_isect(gr, grid_pt, t, grass_sp, h)
struct grass_ray *gr;
point_t grid_pt;	/* grid location to intersect */
double t;		/* distance from "in" along "dir" to curr grid crossing */
struct grass_specific *grass_sp;
struct grass_hit *h;		/* return information */
{
	point_t	tmp;
	point_t	npt;	/* new point for stalk origin */
	vect_t	stalk;
	int 	cond;
	double	dist;
	double	val;
	double	len;
	double	ldist[2];
	point_t	PCA_grass;
	point_t	PCA_ray;
	double	PCA_ray_radius;

	/* compute tilting from vertical for this stalk */
	VSCALE(tmp, grid_pt, grass_sp->tilt_scale);
	bn_noise_vec(tmp, stalk);
	VECT_UP(stalk);
	stalk[Z] += 2.0;
	VUNITIZE(stalk);

	/* Adding a perturbation of the grid point to break up
	 * some of the "grid" structure of the stalk locations.
	 */
	npt[X] = grid_pt[X] - stalk[X];
	npt[Y] = grid_pt[Y] - stalk[Y];
	npt[Z] = grid_pt[Z] = 0.0;

	/* intersect ray with vertical stalk
	 * XXX we really should take advantage of the tolerance here
	 */
	cond = rt_dist_line3_line3(ldist, gr->r.r_pt, gr->r.r_dir,
		npt, stalk, gr->tol);


	if (cond < 0) {
		if (rdebug&RDEBUG_SHADE) bu_log("grass colinear w/ray\n");
		if (cond < -1) return 0;

		/* XXX Lines are parallel and collinear */
		return 0;
	}

	if (ldist[0] >= gr->r.r_max /* beyond out point */ ||
	    ldist[1] < 0.0 /* under ground */)
	    	return 0;


	/* Noise function to define height of grass blades */
	VSCALE(tmp, npt, grass_sp->height_scale);

	if (!strncmp(grass_sp->hfunc, "fbm", 3)) {
		val = bn_noise_fbm(tmp, grass_sp->h_val,
			grass_sp->lacunarity, grass_sp->octaves);
	} else if (!strncmp(grass_sp->hfunc, "tur", 3)) {
		val = bn_noise_turb(tmp, grass_sp->h_val,
			grass_sp->lacunarity, grass_sp->octaves) - .5;
	} else {
		bu_log("unkown noise func \"%s\". Using fbm\n", grass_sp->hfunc);
		val = bn_noise_fbm(tmp, grass_sp->h_val,
			grass_sp->lacunarity, grass_sp->octaves);
	}

#define HEIGHT_OF_SOLID (500.0 * grass_sp->size) /* XXX height of solid in mm */
	len = val * HEIGHT_OF_SOLID;
	if (len < 0.0 /* negative grass length */ ||
	    ldist[1] > len  /* Isect point off end of grass */)
	    	return 0;

	/* compute Pt of closest approach along ray */
	VJOIN1(PCA_ray, gr->r.r_pt, ldist[0], gr->r.r_dir);
	PCA_ray_radius = gr->radius + ldist[0] * gr->diverge;

	/* if the grass is too short, we miss and march onward */
	if ( (PCA_ray[Z] - PCA_ray_radius) >= len) {
		if (rdebug&RDEBUG_SHADE) bu_log("grass short\n");
		return 0;
	}

	VJOIN1(PCA_grass, npt, ldist[1], stalk);
	VSUB2(tmp, PCA_grass, PCA_ray);
	dist = MAGNITUDE(tmp);
	if (dist > (PCA_ray_radius+grass_sp->grass_radius)) {
		if (rdebug&RDEBUG_SHADE) bu_log("missed aside\n");
		return 0;
	}

	/* At this point we've got a hit */
	h->h.hit_dist = ldist[0];
	VMOVE(h->h.hit_point, PCA_ray);
	VMOVE(h->stalk, stalk);
	VMOVE(h->root, npt);
	h->len = len;
	h->alt = ldist[1];
	h->noise = val;

	return 1;
}


/*
 *	March through the grass, intersecting with each grid crossing.
 *
 *	Return:
 *		0	Missed everything
 *		1	Hit some grass, h modified
 */
static int
grass_march(gr, grass_sp, h)
struct grass_ray *gr;
struct grass_specific *grass_sp;
struct grass_hit *h;
{
	double	tDX;		/* dist along ray to span 1 cell in X dir */
	double	tDY;		/* dist along ray to span 1 cell in Y dir */
	double	tX, tY;	/* dist along ray from hit pt. to next cell boundary */
	double	which_x;
	double	which_y;
	point_t	next_pt;
	point_t grid_pt;
	int		step_prev;

	grid_pt[Z] = 0.0;

	/* tDX is the distance along the ray we have to travel
	 * to traverse a cell (travel a unit distance) along the
	 * X axis of the grid.
	 *
	 * tX is the distance along the ray from the initial hit point
	 * to the first cell boundary in the X direction
	 */
	if (gr->r.r_dir[X] < 0.0) {
		tDX = -1.0 / gr->r.r_dir[X];
		tX = (gr->r.r_pt[X] - ((int)gr->r.r_pt[X])) / gr->r.r_dir[X];
		which_x = 0.0;
	} else {
		tDX = 1.0 / gr->r.r_dir[X];
		if (gr->r.r_dir[X] > 0.0)
			tX = (1.0 - (gr->r.r_pt[X] - ((int)gr->r.r_pt[X]))) / gr->r.r_dir[X];
		else
			tX = MAX_FASTF;
		which_x = 1.0;
	}
	if (tX > gr->r.r_max) tX = gr->r.r_max;


	/* tDY is the distance along the ray we have to travel
	 * to traverse a cell (travel a unit distance) along the
	 * Y axis of the grid
	 *
	 * tY is the distance along the ray from the initial hit point
	 * to the first cell boundary in the Y direction
	 */
	if (gr->r.r_dir[Y] < 0.0) {
		tDY = -1.0 / gr->r.r_dir[Y];
		tY = (gr->r.r_pt[Y] - ((int)gr->r.r_pt[Y])) / gr->r.r_dir[Y];
		which_y = 0.0;
	} else {
		tDY = 1.0 / gr->r.r_dir[Y];
		if (gr->r.r_dir[Y] > 0.0)
			tY = (1.0 - (gr->r.r_pt[Y] - ((int)gr->r.r_pt[Y]))) / gr->r.r_dir[Y];
		else
			tY = MAX_FASTF;
		which_y = 1.0;
	}
	if (tY > gr->r.r_max) tY = gr->r.r_max;

	/* time to go marching through the noise space */
	if( rdebug&RDEBUG_SHADE) {
		bu_log("tX %g  tDX %g\n", tX, tDX);
		bu_log("tY %g  tDY %g\n", tY, tDY);
	}


	/* March through the grid, cell by cell.  At each cell boundary,
	 * we intersect the ray with cell corners.  By keeping track of whether
	 * the previous boundary was the same axis as the current one, we can
	 * avoid intersecting any cell corner more than once. 
	 */
	while (tX < gr->r.r_max || tY < gr->r.r_max) {
		if (tX < tY) {
			/* one step in X direction */
			VJOIN1(next_pt, gr->r.r_pt, tX, gr->r.r_dir);
			grid_pt[X] = floor(next_pt[X]);

			if (step_prev != PREV_Y) {
				/* check (int)y and (int)y+1.0 */
				grid_pt[Y] = floor(next_pt[Y]);

				if( rdebug&RDEBUG_SHADE)
					bu_log("tX(%g) pt %g %g %g (check 2)\n\tgrid_pt=%g,%g,%g\n",
						tX, V3ARGS(next_pt), V3ARGS(grid_pt));

				if (grass_isect(gr, grid_pt, tX, grass_sp, h))
					return 1;


				grid_pt[Y] += 1.0;
				if( rdebug&RDEBUG_SHADE)
					bu_log("\tgrid_pt=%g,%g,%g\n", V3ARGS(grid_pt));

				if (grass_isect(gr, grid_pt, tX, grass_sp, h))
					return 1;

			} else {
				/* check only the rayward one */
				grid_pt[Y] = floor(next_pt[Y]+which_y);

				if( rdebug&RDEBUG_SHADE)
					bu_log("tX(%g) pt %g %g %g (check 1)\n\tgrid_pt=%g,%g,%g\n",
						tX, V3ARGS(next_pt), V3ARGS(grid_pt));

				if (grass_isect(gr, grid_pt, tX, grass_sp, h))
				  	return 1;
			}
			step_prev = PREV_X;
			tX += tDX;
		} else {
			/* one step in Y gr->r.r_direction */
			VJOIN1(next_pt, gr->r.r_pt, tY, gr->r.r_dir);
			grid_pt[Y] = floor(next_pt[Y]);

			if (step_prev != PREV_X) {
				/* check (int)x and (int)x+1.0 */
				grid_pt[X] = floor(next_pt[X]);

				if( rdebug&RDEBUG_SHADE)
					bu_log("tY(%g) pt %g %g %g (check 2)\n\tgrid_pt=%g,%g,%g\n",
						tY, V3ARGS(next_pt),
						V3ARGS(grid_pt));

				if (grass_isect(gr, grid_pt, tY, grass_sp, h))
						return 1;

				grid_pt[X] += 1.0;
				if( rdebug&RDEBUG_SHADE)
					bu_log("\tgrid_pt=%g,%g,%g\n", V3ARGS(grid_pt));

				if (grass_isect(gr, grid_pt, tY, grass_sp, h))
						return 1;
			} else {
				/* check only the rayward one */
				grid_pt[X] = floor(next_pt[X]+which_x);

				if( rdebug&RDEBUG_SHADE)
					bu_log("tY(%g) pt %g %g %g (check 1)\n\tgrid_pt=%g,%g,%g\n",
						tY, V3ARGS(next_pt), V3ARGS(grid_pt));

				if (grass_isect(gr, grid_pt, tY, grass_sp, h))
						return 1;
			}
			step_prev = PREV_Y;
			tY += tDY;
		}
	}
	return 0;
}




static void
do_hit(swp, ap, h, grass_sp)
struct shadework *swp;
struct application	*ap;
struct grass_hit *h;
struct grass_specific *grass_sp;
{
	CONST static vect_t up = {0.0, 0.0, 1.0};
	vect_t left;
	vect_t tmp;
	double colorscale;

	/* re-scale shader hit point/distance back into model space */
	swp->sw_hit.hit_dist += grass_sp->inv_size * h->h.hit_dist;

	VSCALE(	swp->sw_hit.hit_point, h->h.hit_point, grass_sp->inv_size);

	swp->sw_transmit = 0.0;

	/* Need to pick a normal */
	VCROSS(left, h->stalk, up);
	VUNITIZE(left);
	VCROSS(swp->sw_hit.hit_normal, left, h->stalk);
	VUNITIZE(swp->sw_hit.hit_normal);
	if( rdebug&RDEBUG_SHADE) {
		bu_log("Hit grass blade\n");
		bu_log("grass root (%g %g %g) stalk (%g %g %g) %g\n",
			V3ARGS(h->root), V3ARGS(h->stalk), MAGNITUDE(h->stalk));
		bu_log("left (%g %g %g) %g\n", V3ARGS(left), MAGNITUDE(left));
		bu_log("Original grass normal (%g %g %g)\n",
			V3ARGS(swp->sw_hit.hit_normal));
	}

	if (VDOT(swp->sw_hit.hit_normal, ap->a_ray.r_dir) > 0.0) {
		VREVERSE(swp->sw_hit.hit_normal,
			swp->sw_hit.hit_normal);
	}
	if (swp->sw_hit.hit_normal[Z] < 0.0)
		swp->sw_hit.hit_normal[Z] *= -1.0;


	colorscale = h->alt/h->len;
	if (colorscale < 0.0 || colorscale > 1.0) {
		bu_log("bad:%g = %g/%g\n", colorscale, h->alt, h->len);
	} else {
		bu_log("good:%g = %g/%g\n", colorscale, h->alt, h->len);
	}
#if 0

	colorscale = (h->alt/h->len) * 0.75 + 0.25;
	VSCALE(swp->sw_color, swp->sw_basecolor, colorscale);
#endif
	if( rdebug&RDEBUG_SHADE) {

		bu_log("Hit pt (%g %g %g) (dist:%g)\n",
			V3ARGS(swp->sw_hit.hit_point),
			swp->sw_hit.hit_dist);

		bu_log("Normal %g %g %g\n",
			V3ARGS(swp->sw_hit.hit_normal));
	}
}




/*
 *	G R A S S _ R E N D E R
 *
 *	This is called (from viewshade() in shade.c) once for each hit point
 *	to be shaded.  The purpose here is to fill in values in the shadework
 *	structure.
 *
 *	Because grass doesn't tend to move around from place to place
 *	in the scene or get scaled up and down, we opt to work in region space.
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
	struct grass_ray gr;
	point_t 	in_pt, out_pt;
	double		in_pt_radius, out_pt_radius;
	double		out_dist;
	struct grass_hit	h;
	int			status;

	/* check the validity of the arguments we got */
	RT_AP_CHECK(ap);
	RT_CHECK_PT(pp);
	CK_grass_SP(grass_sp);

	if( rdebug&RDEBUG_SHADE) bu_log("grass_render\n");

	/* figure out the in/out points, and the radius of the beam
	 * We can work in model space since grass isn't likely to be moving
	 * around the scene.
	 *
	 * XXX We really should at least operate in Region space.
	 * MAT4X3PNT(in_pt, grass_sp->m_to_r, swp->sw_hit.hit_point);
	 */
	VJOIN1(in_pt, ap->a_ray.r_pt, swp->sw_hit.hit_dist, ap->a_ray.r_dir);
	in_pt_radius = ap->a_rbeam + swp->sw_hit.hit_dist * ap->a_diverge;

	VJOIN1(out_pt, ap->a_ray.r_pt, pp->pt_outhit->hit_dist, ap->a_ray.r_dir);
	out_pt_radius = ap->a_rbeam + pp->pt_outhit->hit_dist * ap->a_diverge;

	/* We usually can't afford to evaluate the shader every mm so we allow
	 * the user to scale space by a constant to reduce the density of
	 * the grass and the number of computations needed.
	 *
	 * We get a new "in" and "out" points and a new radius at the "in"
	 * point in this new coordiante system.
	 */
	VSCALE(in_pt, in_pt, grass_sp->size);
	VSCALE(out_pt, out_pt, grass_sp->size);
 	in_pt_radius *= grass_sp->size;

	/* distance to out point from "in_pt" point (not ray start point). */
	out_dist = (pp->pt_outhit->hit_dist - swp->sw_hit.hit_dist) *
			 grass_sp->size;

	if( rdebug&RDEBUG_SHADE) {
		bu_log("\tPnt: %g %g %g\n", V3ARGS(ap->a_ray.r_pt));
		bu_log("\tDir: %g %g %g (diverge:%g)\n", V3ARGS(ap->a_ray.r_dir), ap->a_diverge);
		bu_log("\tHit: %g %g %g  (dist:%g) (radius:%g)\n",
			V3ARGS(swp->sw_hit.hit_point), swp->sw_hit.hit_dist,
			ap->a_rbeam + swp->sw_hit.hit_dist * ap->a_diverge);
		bu_log("\tOut: %g %g %g  (dist:%g) (radius:%g)\n",
			V3ARGS(out_pt), pp->pt_outhit->hit_dist,
			out_pt_radius);

		bu_log("\t in: %g %g %g (dist:%g) r:%g\n", V3ARGS(in_pt), 
			swp->sw_hit.hit_dist * grass_sp->size, in_pt_radius);

		bu_log("\tout: %g %g %g (dist%g)\n", V3ARGS(out_pt), out_dist);

		bu_struct_print("Parameters:", grass_print_tab, (char *)grass_sp );
	 	if( swp->sw_xmitonly ) bu_log("xmit only\n");

	}

	VMOVE(gr.r.r_pt, in_pt);
	VMOVE(gr.r.r_dir, ap->a_ray.r_dir);
	gr.r.r_max = out_dist;
	gr.diverge = ap->a_diverge;
	gr.radius = in_pt_radius;
	gr.tol = &ap->a_rt_i->rti_tol;
	
	status = grass_march(&gr, grass_sp, &h);
	if (status) {
		do_hit(swp, ap, &h, grass_sp);

		/* Tell stacker to shade with plastic */

		return 1;
	}


	if( rdebug&RDEBUG_SHADE)
		bu_log("Missed grass blades\n");


	/* Missed everything */
	/* setting basecolor to 1.0 prevents "filterglass" effect */
	VSETALL(swp->sw_basecolor, 1.0); 
	swp->sw_transmit = 1.0;
	(void)rr_render( ap, pp, swp );

	/* Tell stacker to abort shading */
	return 0;

}
