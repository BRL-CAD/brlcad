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
	double	grass_cell[2];	/* size of a cell in Region coordinates */
	double	lacunarity;
	double	h_val;
	double	octaves;
	double	size;
	point_t	vscale;	/* size of noise coordinate space */
	vect_t	delta;
	mat_t	m_to_sh;	/* model to shader space matrix */
/*	mat_t	sh_to_m;	/* shader to model space matrix */
};

/* The default values for the variables in the shader specific structure */
CONST static
struct grass_specific grass_defaults = {
	grass_MAGIC,
	{1.0, 1.0},			/* grass_cell */
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
	{"%f",  2, "cell",		SHDR_AO(grass_cell),	FUNC_NULL },
	{"%f",	1, "lacunarity",	SHDR_O(lacunarity),	FUNC_NULL },
	{"%f",	1, "H", 		SHDR_O(h_val),	FUNC_NULL },
	{"%f",	1, "octaves", 		SHDR_O(octaves),	FUNC_NULL },
	{"%f",  1, "size",		SHDR_O(size),	FUNC_NULL },
	{"",	0, (char *)0,		0,			FUNC_NULL }

};
struct bu_structparse grass_parse_tab[] = {
	{"i",	bu_byteoffset(grass_print_tab[0]), "grass_print_tab", 0, FUNC_NULL },
	{"%f",  2, "c",			SHDR_AO(grass_cell),	FUNC_NULL },
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
	mat_t	tmp;
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

/*  Process a grid cell and any unprocessed adjoining neighbors.
 *
 *  Return:
 *	0	continue grid marching
 *	~0	abort grid marching
 */
static int
process(curr_pt, flags)
point_t curr_pt;
short flags;
{
	point_t grid_loc;

	return 0;
}

/* The flags argument indicates which cells (relative to the central one)
 * have already been processed.  The bits in the flag word are assigned
 * as follows: ABCDEFGHI.  and represent the relative grid positions:
 *
 *	G H I
 *	D E F
 *	A B C
 *
 * So to move one row in the positive Y direction, we shift the bits
 * as follows: DEFGHI000 while to move in the negative Y we shift the
 * bits as in: 000ABCDEF.
 * Moving in X is slightly more tricky.  To move one row in the positive X
 * direction, a bit pattern ABCDEFGHI becomes BC0EF0HI0 and negative X move
 * results in 0AB0DE0GH.
 *
 * To hide the gory details, we have some macros to do the work.
 */
#define FLAGS_X_MOVE(f, s) \
	if (s < 0) f = 0333;	/* ABC DEF GHI -> 0AB 0DE 0GH */\
	else f = 0666		/* ABC DEF GHI -> BC0 EF0 HI0 */

#define	FLAGS_Y_MOVE(f, s) \
	if (s < 0) f = 0077;	/* ABC DEF GHI -> 000 ABC DEF */\
	else f = 0770		/* ABC DEF GHI -> DEF GHI 000 */



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
	double		tX, tDX, tY, tDY;
	double		curr_dist, out_dist;
	double		radius;
	short		flags;

	/* check the validity of the arguments we got */
	RT_AP_CHECK(ap);
	RT_CHECK_PT(pp);
	CK_grass_SP(grass_sp);

	if( rdebug&RDEBUG_SHADE)
		bu_struct_print( "grass_render Parameters:", grass_print_tab, (char *)grass_sp );
	/* figure out the in/out points, and the radius of the beam in
	 * "region" space.
	 */
	MAT4X3PNT(r.r_pt, grass_sp->m_to_sh, ap->a_ray.r_pt);
	MAT4X3VEC(r.r_dir, grass_sp->m_to_sh, ap->a_ray.r_dir);
	VUNITIZE(r.r_dir);

	MAT4X3PNT(in_pt, grass_sp->m_to_sh, swp->sw_hit.hit_point);

	/* The only thing we can really do is get (possibly 3 different)
	 * radii in the Region coordinate system
	 */
	radius = ap->a_rbeam + swp->sw_hit.hit_dist * ap->a_diverge;
	VSETALL(v, radius);
	MAT4X3VEC(in_rad, grass_sp->m_to_sh, v);

	VJOIN1(v, ap->a_ray.r_pt, pp->pt_outhit->hit_dist, ap->a_ray.r_dir);
	MAT4X3VEC(out_pt, grass_sp->m_to_sh, v);
	radius = ap->a_rbeam + pp->pt_outhit->hit_dist * ap->a_diverge;
	VSETALL(v, radius);
	MAT4X3VEC(out_rad, grass_sp->m_to_sh, v);


	VSUB2(v, in_pt, r.r_pt);
	curr_dist = MAGNITUDE(v);
	VSUB2(v, out_pt, in_pt);
	out_dist = curr_dist + MAGNITUDE(v);


	/* tDX is the distance along the ray we have to travel
	 * to traverse a cell (travel a unit distance) along the
	 * X axis of the grid.
	 *
	 * tX is the distance along the ray to the first cell boundary
	 * beyond the hit point in the X direction.
	 */
	tX = in_pt[X] - 
		floor(in_pt[X]/grass_sp->grass_cell[X]) *
			       grass_sp->grass_cell[X];
	if (r.r_dir[X] < 0.0) {
		tDX = -grass_sp->grass_cell[X] / r.r_dir[X];
		tX += curr_dist;
	} else if (r.r_dir[X] > 0.0) {
		tDX = grass_sp->grass_cell[X] / r.r_dir[X];
		tX = grass_sp->grass_cell[X] - tX;
		tX += curr_dist;
	} else {
		tDX = tX = MAX_FASTF;
	}
	if (tX > out_dist) tX = out_dist;

	/* tDY and tY are for Y as tDX and tX are for X */
	tY = in_pt[Y] - 
		floor(in_pt[Y]/grass_sp->grass_cell[Y]) *
		               grass_sp->grass_cell[Y];
	if (r.r_dir[Y] < 0.0) {
		tDY = -grass_sp->grass_cell[Y] / r.r_dir[Y];
		tY += curr_dist;
	} else if (r.r_dir[Y] > 0.0) {
		tDY = grass_sp->grass_cell[Y] / r.r_dir[Y];
		tY = grass_sp->grass_cell[Y] - tY;
		tY += curr_dist;
	} else {
		tDY = tY = MAX_FASTF;
	}
	if (tY > out_dist) tY = out_dist;

	/* Time to go marching across the grid */
	flags = 0;
	VMOVE(curr_pt, in_pt);
	while (curr_dist < out_dist) {

		/* XXX need to get/pass cell #'s and cell widths */
		if (process(curr_pt, flags))
			goto done;

		if (tX < tY) {
			FLAGS_X_MOVE(flags, r.r_dir[X]);
			curr_dist = tX;
			tX += tDX;
			if (tX > out_dist) tX = out_dist; 
		} else {
			FLAGS_Y_MOVE(flags, r.r_dir[Y]);
			curr_dist = tY;
			tY += tDY;
			if (tY > out_dist) tY = out_dist; 
		}
		VJOIN1(curr_pt, in_pt, curr_dist, r.r_dir);
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
