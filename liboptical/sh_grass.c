/*
 *	S H _ G R A S S . C
 *
 *	A procedural shader to produce grass
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

#define grass_MAGIC 0x1834    /* make this a unique number for each shader */
#define CK_grass_SP(_p) RT_CKMAG(_p, grass_MAGIC, "grass_specific")

/*
 * the shader specific structure contains all variables which are unique
 * to any particular use of the shader.
 */
struct grass_specific {
	long	magic;	/* magic # for memory validity check, must come 1st */
	double	cell[2];	/* size of a cell in Region coordinates */
	double	lacunarity;
	double	h_val;
	double	octaves;
	double	size;
	point_t	vscale;	/* size of noise coordinate space */
	vect_t	delta;
	mat_t	m_to_sh;	/* model to shader space matrix */
};

/* The default values for the variables in the shader specific structure */
CONST static
struct grass_specific grass_defaults = {
	grass_MAGIC,
	{1.0, 1.0},			/* cell */
	2.1753974,			/* lacunarity */
	1.0,				/* h_val */
	4.0,				/* octaves */
	1.0,				/* size */
	{ 1.0, 1.0, 1.0 },		/* vscale */
	{ 1000.0, 1000.0, 1000.0 },	/* delta into noise space */
	{	0.0, 0.0, 0.0, 0.0,	/* m_to_sh */
		0.0, 0.0, 0.0, 0.0,
		0.0, 0.0, 0.0, 0.0,
		0.0, 0.0, 0.0, 0.0 }
	};

#define SHDR_NULL	((struct grass_specific *)0)
#define SHDR_O(m)	offsetof(struct grass_specific, m)
#define SHDR_AO(m)	offsetofarray(struct grass_specific, m)

/* description of how to parse/print the arguments to the shader
 * There is at least one line here for each variable in the shader specific
 * structure above
 */
struct bu_structparse grass_print_tab[] = {
	{"%f",  2, "cell",		SHDR_AO(cell),	FUNC_NULL },
	{"%f",	1, "lacunarity",	SHDR_O(lacunarity),	FUNC_NULL },
	{"%f",	1, "H", 		SHDR_O(h_val),	FUNC_NULL },
	{"%f",	1, "octaves", 		SHDR_O(octaves),	FUNC_NULL },
	{"%f",  1, "size",		SHDR_O(size),	FUNC_NULL },
	{"",	0, (char *)0,		0,			FUNC_NULL }

};
struct bu_structparse grass_parse_tab[] = {
	{"i",	bu_byteoffset(grass_print_tab[0]), "grass_print_tab", 0, FUNC_NULL },
	{"%f",  2, "c",			SHDR_AO(cell),	FUNC_NULL },
	{"%f",	1, "l",			SHDR_O(lacunarity),	FUNC_NULL },
	{"%f",	1, "o", 		SHDR_O(octaves),	FUNC_NULL },
	{"%f",  1, "s",			SHDR_O(size),	FUNC_NULL },
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
	{"grass",	0,	0,		MFI_NORMAL|MFI_HIT|MFI_UV,	0,
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
		rt_log("grass_setup(%s)\n", rp->reg_name);

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

	/* The shader needs to operate in a coordinate system which stays
	 * fixed on the region when the region is moved (as in animation).
	 * We need to get a matrix to perform the appropriate transform(s).
	 */
	db_region_mat(grass_sp->m_to_sh, rtip->rti_dbip, rp->reg_name);

	if( rdebug&RDEBUG_SHADE) {
		bu_struct_print( " Parameters:", grass_print_tab, (char *)grass_sp );
		mat_print( "m_to_sh", grass_sp->m_to_sh );
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

/*	I S E C T _ C E L L
 *
 *  Intersects a region-space ray with a grid cell of grass.
 *
 *  Return:
 *	0	continue grid marching
 *	~0	abort grid marching
 */
static int
isect_cell(cell, r, swp, out_dist, grass_sp)
long			cell[3];
struct xray		*r;
double 			out_dist;
struct shadework	*swp;
struct grass_specific	*grass_sp;
{
	if( rdebug&RDEBUG_SHADE)
		bu_log("isect_cell(%ld,%ld)\n", V2ARGS(cell));

	return 0;
}



/*  Process a grid cell and any unprocessed adjoining neighbors.
 *
 * The flags argument indicates which cells (relative to the central
 * one) need to be processed.  The bit values for the relative
 * positions are:
 *
 *  pos   bit	 pos  bit   pos  bit
 *------------------------------------
 * -1, 1  0100	0, 1 0200  1, 1 0400
 * -1, 0  0010  0, 0 0020  1, 0 0040
 * -1,-1  0001  0,-1 0002  1,-1 0004
 *
 *
 *  Return:
 *	0	continue grid marching
 *	~0	abort grid marching
 */
static int
do_cells(cell_num, r, flags, swp, out_dist, grass_sp)
long			cell_num[3];
struct xray		*r;
short 			flags;
double 			out_dist;
struct shadework	*swp;	/* defined in material.h */
struct grass_specific	*grass_sp;
{
	int x, y;
	long cell[3];

#define FLAG(x,y) ((1 << ((y+1)*3)) << (x+1))
#define ISDONE(x,y,flags) ( ! (FLAG(x,y) & flags))

	if( rdebug&RDEBUG_SHADE)
		bu_log("do_cells(%ld,%ld)\n", V2ARGS(cell_num));

	for (y=-1; y < 2 ; y++) {
		for (x=-1; x < 2 ; x++) {
#if 0
			short f;
			f = FLAG(x,y);
			if( rdebug&RDEBUG_SHADE)
				bu_log("\n0%03o &\n0%03o = %d ",
					flags, f, (flags & f) != 0 );
#endif
			if ( ISDONE(x,y,flags) ) continue;

			cell[X] = cell_num[X] + x;
			cell[Y] = cell_num[Y] + y;

			if( rdebug&RDEBUG_SHADE)
				bu_log("checking relative cell %2d,%2d at(%d,%d)\n",
					x, y, V2ARGS(cell));
			if (isect_cell(cell, r, swp, out_dist, grass_sp))
				return 1;

		}
	}
	return 0;
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
	struct xray	r;
	vect_t		v;
	vect_t		in_rad, out_rad;
	point_t 	in_pt, out_pt, curr_pt;
	double		curr_dist, out_dist;
	double		radius;
	short		flags;
	double		t[2], tD[2];
	int		n;
	double		t_orig[2];
	long		tD_iter[2];
	long		cell_num[3];	/* cell number */



	/* check the validity of the arguments we got */
	RT_AP_CHECK(ap);
	RT_CHECK_PT(pp);
	CK_grass_SP(grass_sp);

	if( rdebug&RDEBUG_SHADE)
		bu_struct_print( "grass_render Parameters:", grass_print_tab, (char *)grass_sp );

	/* Convert everything over to "Region" space.
	 * First the ray and its radius then
	 * the in/out points
	 */
	MAT4X3PNT(r.r_pt, grass_sp->m_to_sh, ap->a_ray.r_pt);
	MAT4X3VEC(r.r_dir, grass_sp->m_to_sh, ap->a_ray.r_dir);
	VUNITIZE(r.r_dir);

	/* In Hit point */
	MAT4X3PNT(in_pt, grass_sp->m_to_sh, swp->sw_hit.hit_point);

	/* The only thing we can really do is get (possibly 3 different)
	 * radii in the Region coordinate system
	 */
	radius = ap->a_rbeam + swp->sw_hit.hit_dist * ap->a_diverge;
	VSETALL(v, radius);
	MAT4X3VEC(in_rad, grass_sp->m_to_sh, v);

	/* Out point */
	VJOIN1(v, ap->a_ray.r_pt, pp->pt_outhit->hit_dist, ap->a_ray.r_dir);
	MAT4X3VEC(out_pt, grass_sp->m_to_sh, v);
	radius = ap->a_rbeam + pp->pt_outhit->hit_dist * ap->a_diverge;
	VSETALL(v, radius);
	MAT4X3VEC(out_rad, grass_sp->m_to_sh, v);

	/* set up a DDA grid to march through. */
	VSUB2(v, in_pt, r.r_pt);
	curr_dist = MAGNITUDE(v);
	VSUB2(v, out_pt, r.r_pt);
	out_dist = MAGNITUDE(v);

	if( rdebug&RDEBUG_SHADE) {
		bu_log("Pt: (%g %g %g)\nRPt:(%g %g %g)\n", V3ARGS(ap->a_ray.r_pt),
			V3ARGS(r.r_pt));
		bu_log("Dir: (%g %g %g)\nRDir:(%g %g %g)\n", V3ARGS(ap->a_ray.r_dir),
			V3ARGS(r.r_dir));

		bu_log("Hit Pt:(%g %g %g) d:%g r:  %g\nRegHPt:(%g %g %g) d:%g Rr:(%g %g %g)\n",
			V3ARGS(swp->sw_hit.hit_point), swp->sw_hit.hit_dist,
			ap->a_rbeam + swp->sw_hit.hit_dist * ap->a_diverge,
			V3ARGS(in_pt), curr_dist, V3ARGS(in_rad));

		VJOIN1(v, ap->a_ray.r_pt, pp->pt_outhit->hit_dist, ap->a_ray.r_dir);
		bu_log("Out Pt:(%g %g %g) d:%g r:   %g\nRegOPt:(%g %g %g) d:%g ROr:(%g %g %g)\n",
			V3ARGS(v), pp->pt_outhit->hit_dist, radius,
			V3ARGS(out_pt), out_dist, V3ARGS(out_rad));
	}


	/* tD[X] is the distance along the ray we have to travel
	 * to traverse a cell (travel a unit distance) along the
	 * X axis of the grid.
	 *
	 * t[X] is the distance along the ray to the first cell boundary
	 * beyond the hit point in the X direction.
	 *
	 * t_orig[X] is the same as t[X] but won't get changed as we
	 * march across the grid.  At each X grid line crossing, we recompute
	 * a new t[X] from t_orig[X], tD[X], tD_iter[X].  The tD_iter[X] is 
	 * the number of times we've made a full step in the X direction.
	 */
	for (n=X ; n < Z ; n++) {
		char *s;
		if (n == X) s="X";
		else s="Y";

		/* compute distance from cell origin to in_pt */
		t[n] = in_pt[n] - 
			floor(in_pt[n]/grass_sp->cell[n]) *
				       grass_sp->cell[n];
		if( rdebug&RDEBUG_SHADE)
			bu_log("t[%s]:%g = in_pt[%s]:%g - floor():%g * cell[%s]%g\n",
				s, t[n], s, in_pt[n],
				floor(in_pt[n]/grass_sp->cell[n]),
				s, grass_sp->cell[n]);

		if (r.r_dir[n] < -SMALL_FASTF) {
			tD[n] = -grass_sp->cell[n] / r.r_dir[n];
			t[n] = t[n] / -r.r_dir[n];
			t[n] += curr_dist;
		} else if (r.r_dir[n] > SMALL_FASTF) {
			tD[n] = grass_sp->cell[n] / r.r_dir[n];
			t[n] = grass_sp->cell[n] - t[n];
			t[n] = t[n] / r.r_dir[n];
			t[n] += curr_dist;
		} else {
			tD[n] = t[n] = MAX_FASTF;
		}
		if (t[n] > out_dist) t[n] = out_dist;
		t_orig[n] = t[n];
		tD_iter[n] = 0;
	}
	

	if( rdebug&RDEBUG_SHADE) {
		bu_log("t[X]:%g tD[X]:%g\n", t[X], tD[X]);
		bu_log("t[Y]:%g tD[Y]:%g\n\n", t[Y], tD[Y]);
	}

	/* The flags argument indicates which cells (relative to the central
	 * one) have already been processed.  The bit values for the relative
	 * positions are:
	 *
	 *  pos   bit	 pos  bit   pos  bit
	 *------------------------------------
	 * -1, 1  0100	0, 1 0200  1, 1 0400
	 * -1, 0  0010  0, 0 0020  1, 0 0040
	 * -1,-1  0001  0,-1 0002  1,-1 0004
	 *
	 */
	flags = 0777;
	VMOVE(curr_pt, in_pt);
	cell_num[X] = (long)(in_pt[X] / grass_sp->cell[X]);
	cell_num[Y] = (long)(in_pt[Y] / grass_sp->cell[Y]);
	cell_num[Z] = 0.0;

	/* Time to go marching across the grid */
	while (curr_dist < out_dist) {


		if( rdebug&RDEBUG_SHADE) {
			point_t	cell_pos; /* cell origin position */

			if( rdebug&RDEBUG_SHADE) {
				bu_log("dist:%g (%g %g %g)\n", curr_dist,
					V3ARGS(curr_pt));
				bu_log("cell num: %d %d\n", V2ARGS(cell_num));
			}
			cell_pos[X] = cell_num[X] * grass_sp->cell[X];
			cell_pos[Y] = cell_num[Y] * grass_sp->cell[Y];

			if( rdebug&RDEBUG_SHADE)
				bu_log("cell pos: %g %g\n", V2ARGS(cell_pos));
		}

		if (do_cells(cell_num, &r, flags,
		    swp, out_dist, grass_sp))
			goto done;

		if (t[X] < t[Y]) {
			if( rdebug&RDEBUG_SHADE)
				bu_log("stepping X %le\n", t[X]);
			if (r.r_dir[X] < 0.0) {
				flags = 0111;
				cell_num[X]--;
			} else {
				flags = 0444;
				cell_num[X]++;
			}
			n = X;
		} else {
			if( rdebug&RDEBUG_SHADE)
				bu_log("stepping Y %le\n", t[Y]);
			if (r.r_dir[Y] < 0.0) {
				flags = 0007;
				cell_num[Y]--;
			} else {
				flags = 0700;
				cell_num[Y]++;
			}
			n = Y;
		}
		curr_dist = t[n];
		t[n] = t_orig[n] + tD[n] * ++tD_iter[n];

		if (t[n] > out_dist) t[n] = out_dist; 

		VJOIN1(curr_pt, r.r_pt, curr_dist, r.r_dir);
		curr_pt[n] = ((long) ((curr_pt[n] / grass_sp->cell[n]) + .1))
			 * grass_sp->cell[n];
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


done:
	/* Tell stacker to shade with plastic */
	return 1;


}
