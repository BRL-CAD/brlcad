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
#include "rtgeom.h"
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


#define DBINT_MAGIC 0xDECCA
struct reg_db_internals {
	struct rt_list	l;
	struct rt_db_internal ip;	/* internal rep from rtgeom.h */
	struct soltab	*st_p;
	vect_t one_sigma;
};
#define DBINT_MAGIC 0xDECCA
#define CK_DBINT(_p) RT_CKMAG( _p, DBINT_MAGIC, "struct reg_db_internals" )

struct tree_bark {
	struct db_i	*dbip;
	struct rt_list	*l;
	CONST char	*name;
	struct gauss_specific *gs;
};


#define gauss_MAGIC 0x6a05    /* make this a unique number for each shader */
#define CK_gauss_SP(_p) RT_CKMAG(_p, gauss_MAGIC, "gauss_specific")

/*
 * the shader specific structure contains all variables which are unique
 * to any particular use of the shader.
 */
struct gauss_specific {
	long	magic;	/* magic # for memory validity check, must come 1st */
	double	gauss_sigma;	/* # std dev represented by ell bounds */
	mat_t	gauss_m_to_sh;	/* model to shader space matrix */
	struct	rt_list	dbil;
};


/* The default values for the variables in the shader specific structure */
CONST static
struct gauss_specific gauss_defaults = {
	gauss_MAGIC,
	4.0,
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
	{"%f",  1, "sigma",		SHDR_O(gauss_sigma),	FUNC_NULL },
	{"",	0, (char *)0,		0,			FUNC_NULL }

};
struct structparse gauss_parse_tab[] = {
	{"i",	byteoffset(gauss_print_tab[0]), "gauss_print_tab", 0, FUNC_NULL },
	{"%f",  1, "s",			SHDR_O(gauss_sigma),	FUNC_NULL },
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


static void
tree_solids(tp, tb)
union tree *tp;
struct tree_bark *tb;
{
	RT_CK_TREE(tp);

	switch( tp->tr_op )  {
	case OP_NOP:
		return;

	case OP_SOLID: {
		struct reg_db_internals *dbint;
		matp_t mp;
		long sol_id;
		struct rt_ell_internal *ell_p;

		GETSTRUCT( dbint, reg_db_internals );
		RT_LIST_MAGIC_SET( &(dbint->l), DBINT_MAGIC);

		if (tp->tr_a.tu_stp->st_matp)
			mp = tp->tr_a.tu_stp->st_matp;
		else
			mp = rt_identity;

		/* Get the internal form of this solid & add it to the list */
		rt_db_get_internal(&dbint->ip, tp->tr_a.tu_stp->st_dp,
			tb->dbip, mp);

		RT_CK_DB_INTERNAL(&dbint->ip);
		dbint->st_p = tp->tr_a.tu_stp;

		sol_id = dbint->ip.idb_type;

		if (sol_id < 0 || sol_id > rt_nfunctab ) {
			rt_log("Solid ID %ld out of bounds\n", sol_id);
			rt_bomb("");
		}


		if (sol_id != ID_ELL) {
			rt_log(" got a solid type %d \"%s\".  This solid ain't no ellipse bucko!\n",
				sol_id, rt_functab[sol_id].ft_name);
		} else {
			ell_p = dbint->ip.idb_ptr;

			rt_log(" got a solid type %d \"%s\"\n", sol_id,
				rt_functab[sol_id].ft_name);

			RT_ELL_CK_MAGIC(ell_p);

			VPRINT("point", ell_p->v); 
			VPRINT("a", ell_p->a); 
			VPRINT("b", ell_p->b); 
			VPRINT("c", ell_p->c); 
		}

		/* XXX Only works for axis aligned solids */
		VSET(dbint->one_sigma,
			MAGNITUDE(ell_p->a) / tb->gs->gauss_sigma,
			MAGNITUDE(ell_p->b) / tb->gs->gauss_sigma,
			MAGNITUDE(ell_p->c) / tb->gs->gauss_sigma);


		VPRINT("sigma", dbint->one_sigma);
		RT_LIST_APPEND(tb->l, &(dbint->l) );

		break;
	}
	case OP_UNION:
		tree_solids(tp->tr_b.tb_left, tb);
		tree_solids(tp->tr_b.tb_right, tb);
		break;

	case OP_NOT:
	case OP_GUARD:
	case OP_XNOP:
		rt_log("Warning: Non-union region op in %s!\n", tb->name);
		tree_solids(tp->tr_b.tb_left, tb);
		break;

	case OP_INTERSECT:
	case OP_SUBTRACT:
	case OP_XOR:
		rt_log("Warning: Non-union region op in %s!\n", tb->name);
		tree_solids(tp->tr_b.tb_left, tb);
		tree_solids(tp->tr_b.tb_right, tb);
		return;

	default:
		rt_bomb("rt_tree_region_assign: bad op\n");
	}
}

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
	struct tree_bark tb;

	/* check the arguments */
	RT_CHECK_RTI(rtip);
	RT_VLS_CHECK( matparm );
	RT_CK_REGION(rp);


	if( rdebug&RDEBUG_SHADE)
		rt_log("gauss_setup(%s)\n", rp->reg_name);

	if (! rtip->useair)
		rt_bomb("gauss shader used and useair not set\n");


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

	RT_LIST_INIT( &gauss_sp->dbil );
	tb.l = &gauss_sp->dbil;

	tb.dbip = rtip->rti_dbip;
	tb.name = rp->reg_name;
	tb.gs = gauss_sp;

	tree_solids ( rp->reg_treetop, &tb );


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
	register struct gauss_specific *gauss_sp =
		(struct gauss_specific *)cp;
	struct reg_db_internals *p;

	while (RT_LIST_WHILE(p, reg_db_internals, &gauss_sp->dbil) ) {
		RT_LIST_DEQUEUE( &(p->l) );
		rt_free( p->ip.idb_ptr, "internal ptr" );
		rt_free( (char *)p, "gauss reg_db_internals" );
	}

	rt_free( cp, "gauss_specific" );
}


/*
 *
 * Evaluate the 3-D gaussian "puff" function:
 *
 * 1.0 / ( (2*PI)^(3/2) * sigmaX*sigmaY*sigmaZ )) * 
 *      exp( -0.5 * ( (x-ux)^2/sigmaX + (y-uy)^2/sigmaY + (z-uz)^2/sigmaZ ) )
 *
 * for a given point "pt" where the center of the puff is at {ux,uy,uz} and
 * the size of 1 standard deviation is {sigmaX,sigmaY,sigmaZ}
 */
static double
gauss_eval(pt, ell_center, sigma)
point_t pt;
point_t ell_center;
vect_t sigma;
{
	double term2;
	point_t p;
	double val;

	VSUB2(p, pt, ell_center);
	p[X] *= p[X];
	p[Y] *= p[Y];
	p[Z] *= p[Z];

	term2 = (p[X]/sigma[X]) + (p[Y]/sigma[Y]) + (p[Z]/sigma[Z]);
	term2 *= term2;

	val = exp( -0.5 * term2 );

	rt_log("pt(%g %g %g) term2:%g val:%g\n",
		V3ARGS(pt), term2, val);

	return val;
}

/*
 * Given a seg which participates in the partition we are shading evaluate
 * the transmission on the path
 */
static double
eval_seg(ap, dbint, seg_p)
struct application	*ap;
struct reg_db_internals *dbint;
struct seg *seg_p;
{
	double span;
	point_t pt;
	struct rt_ell_internal *ell_p = dbint->ip.idb_ptr;
	double optical_density = 0.0;
	double step_dist;
	double dist;
	int steps;

	span = seg_p->seg_out.hit_dist - seg_p->seg_in.hit_dist;
	steps = (int)(span / 100.0 + 0.5);
	if ( steps < 2 ) steps = 2;

	step_dist = span / (double)steps;

	rt_log("r_pt(%g %g %g)  r_dir(%g %g %g)\n", V3ARGS(ap->a_ray.r_pt),
		V3ARGS(ap->a_ray.r_dir) );

	rt_log("dist_in:%g dist_out:%g span:%g step_dist:%g steps:%d\n",
		seg_p->seg_in.hit_dist,
		seg_p->seg_out.hit_dist,
		span, step_dist, steps);

#if 1
	for (dist=seg_p->seg_in.hit_dist ; dist < seg_p->seg_out.hit_dist ; dist += step_dist ) {
		VJOIN1(pt, ap->a_ray.r_pt, dist, ap->a_ray.r_dir);
		optical_density += gauss_eval(pt, ell_p->v, dbint->one_sigma);
	}


	return optical_density;
#else
	return gauss_eval(ell_p->v, ell_p->v, dbint->one_sigma);
#endif
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
	struct seg *seg_p;
	struct reg_db_internals *dbint_p;
	double optical_density = 0.0;
	double partition_dist;
	double tau;

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

	/* XXX First of all, we need to rotate the ray and ellipse into an
	 * axis-aligned state
	 */


	RT_CK_LIST_HEAD(&swp->sw_segs->l);
	RT_CK_LIST_HEAD(&gauss_sp->dbil);


	/* look at each segment that participated in the ray */
	for (RT_LIST_FOR(seg_p, seg, &swp->sw_segs->l) ) {

		RT_CK_SEG(seg_p);
		RT_CK_SOLTAB(seg_p->seg_stp);

		/* check to see if the seg/solid is in this partition */
		if (BITTEST(pp->pt_solhit, seg_p->seg_stp->st_bit) ) {

			/* check to see if the solid is in the region */
			for (RT_LIST_FOR(dbint_p, reg_db_internals, &gauss_sp->dbil)){

				CK_DBINT(dbint_p);

				if (dbint_p->st_p == seg_p->seg_stp) {
					optical_density +=
						eval_seg(ap, dbint_p, seg_p);
					break;
				}
			}
		}
	}


	rt_log("Optical Density %g\n", optical_density);

	/* get the path length right */
	if (pp->pt_inhit->hit_dist < 0.0)
		partition_dist = pp->pt_outhit->hit_dist;
	else
		partition_dist = (pp->pt_outhit->hit_dist - pp->pt_inhit->hit_dist);

	tau = optical_density * partition_dist;

/* 	swp->sw_transmit = exp(-tau); */
 	swp->sw_transmit = 1.0 - optical_density;

/*	VMOVE(swp->sw_color, pt);*/

	/* shader must perform transmission/reflection calculations
	 *
	 * 0 < swp->sw_transmit <= 1 causes transmission computations
	 * 0 < swp->sw_reflect <= 1 causes reflection computations
	 */
	if( swp->sw_reflect > 0 || swp->sw_transmit > 0 )
		(void)rr_render( ap, pp, swp );

	return(1);
}



