/*                      S H _ L I G H T . C
 * BRL-CAD
 *
 * Copyright (c) 1998-2007 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file sh_light.c
 *			L I G H T . C
 *
 *  Implement simple isotropic light sources as a material property.
 *
 *  Author -
 *	Michael John Muuss
 *
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5068  USA
 */
#ifndef lint
static const char RCSsh_light[] = "@(#)$Header$ (ARL)";
#endif

#include "common.h"

#include <stddef.h>
#include <stdio.h>
#ifdef HAVE_STRING_H
#  include <string.h>
#else
#  include <strings.h>
#endif
#include <math.h>

#include "machine.h"
#include "vmath.h"
#include "raytrace.h"
#include "rtprivate.h"
#include "plot3.h"
#include "light.h"
#include "photonmap.h"

#ifdef RT_MULTISPECTRAL
#  include "spectrum.h"
extern const struct bn_table	*spectrum;
#endif


#if !defined(M_PI)
#define M_PI            3.14159265358979323846
#define	M_PI_2		1.57079632679489661923
#endif


#define LIGHT_O(m)	bu_offsetof(struct light_specific, m)
#define LIGHT_OA(m)	bu_offsetofarray(struct light_specific, m)

BU_EXTERN(void	aim_set, (const struct bu_structparse *sdp, const char *name,
			  const char *base, char *value));

/***********************************************************************
 *
 *  light_cvt_visible()
 *
 *  Convert "visible" flag to "invisible" variable
 */
void
light_cvt_visible(register const struct bu_structparse *sdp, register const char *name, char *base, const char *value)
     /* structure description */
     /* struct member name */
     /* begining of structure */
     /* string containing value */
{
    struct light_specific *lsp = (struct light_specific *)base;

    if (rdebug & RDEBUG_LIGHT) {
	bu_log("light_cvt_visible(%s, %d)\n", name, sdp->sp_offset);
	bu_log("visible: %d invisible: %d\n",
	       LIGHT_O(lt_visible),
	       LIGHT_O(lt_invisible));
    }
    switch (sdp->sp_offset) {
    case LIGHT_O(lt_invisible):
	lsp->lt_visible = !lsp->lt_invisible;
	break;
    case LIGHT_O(lt_visible):
	lsp->lt_invisible = !lsp->lt_visible;
	break;
    }
}

/*
 *
 */
static void
light_pt_set(register const struct bu_structparse *sdp, register const char *name, char *base, const char *value)
     /* structure description */
     /* struct member name */
     /* begining of structure */
     /* string containing value */
{
    struct light_specific *lsp = (struct light_specific *)base;
    fastf_t *p = (fastf_t *)(base+sdp->sp_offset);

    /* make sure we have enough room, allocate in batches of SOME_LIGHT_SAMPLES */
    if ( lsp->lt_pt_count % SOME_LIGHT_SAMPLES == 0) {
	if (lsp->lt_pt_count == 0) {
	    /* assumes initialized to NULL */
	    if (lsp->lt_sample_pts) {
		bu_free(lsp->lt_sample_pts, "free light samples array");
	    }
	    lsp->lt_sample_pts = (struct light_pt *)bu_calloc(lsp->lt_pt_count + SOME_LIGHT_SAMPLES, sizeof(struct light_pt), "callocate light sample points");
	} else {
	    lsp->lt_sample_pts = (struct light_pt *)bu_realloc(lsp->lt_sample_pts, (lsp->lt_pt_count + SOME_LIGHT_SAMPLES) * sizeof(struct light_pt), "reallocate light sample points");
	}
    }

    if (! strcmp("pt", name) ) {
	/* user just specified point, set normal to zeros */
	p[3] = p[4] = p[5] = 0.0;
    } else if ( strcmp("pn", name) ) {
	bu_log("*********** unknown option in light_pt_set %s:%d\n", __FILE__, __LINE__);
	return;
    }

    memcpy( &lsp->lt_sample_pts[ lsp->lt_pt_count++ ], p, sizeof( struct light_pt ) );

    if (rdebug & RDEBUG_LIGHT ) {
	bu_log("set light point %g %g %g   N %g %g %g\n", p[0], p[1], p[2], p[3], p[4], p[5]);
    }
}

struct bu_structparse light_print_tab[] = {
    {"%f",	1, "bright",	LIGHT_O(lt_intensity),	BU_STRUCTPARSE_FUNC_NULL },
    {"%f",	1, "angle",	LIGHT_O(lt_angle),	BU_STRUCTPARSE_FUNC_NULL },
    {"%f",	1, "fract",	LIGHT_O(lt_fraction),	BU_STRUCTPARSE_FUNC_NULL },
    {"%f",	3, "target",	LIGHT_OA(lt_target),	aim_set },
    {"%d",	1, "shadows",	LIGHT_O(lt_shadows),	BU_STRUCTPARSE_FUNC_NULL },
    {"%d",	1, "infinite",	LIGHT_O(lt_infinite),	BU_STRUCTPARSE_FUNC_NULL },
    {"%d",	1, "visible",	LIGHT_O(lt_visible),	BU_STRUCTPARSE_FUNC_NULL },
    {"%d",	1, "invisible",	LIGHT_O(lt_invisible),	BU_STRUCTPARSE_FUNC_NULL },
    {"",	0, (char *)0,	0,			BU_STRUCTPARSE_FUNC_NULL }
};
struct bu_structparse light_parse[] = {

    {"%f",	1, "bright",	LIGHT_O(lt_intensity),	BU_STRUCTPARSE_FUNC_NULL },
    {"%f",	1, "b",		LIGHT_O(lt_intensity),	BU_STRUCTPARSE_FUNC_NULL },
    {"%f",	1, "inten",	LIGHT_O(lt_intensity),	BU_STRUCTPARSE_FUNC_NULL },

    {"%f",	1, "angle",	LIGHT_O(lt_angle),	BU_STRUCTPARSE_FUNC_NULL },
    {"%f",	1, "a",		LIGHT_O(lt_angle),	BU_STRUCTPARSE_FUNC_NULL },

    {"%f",	1, "fract",	LIGHT_O(lt_fraction),	BU_STRUCTPARSE_FUNC_NULL },
    {"%f",	1, "f",		LIGHT_O(lt_fraction),	BU_STRUCTPARSE_FUNC_NULL },

    {"%f",	3, "target",	LIGHT_OA(lt_target),	aim_set },
    {"%f",	3, "t",		LIGHT_OA(lt_target),	aim_set },
    {"%f",	3, "aim",	LIGHT_OA(lt_target),	aim_set },

    {"%d",	1, "shadows",	LIGHT_O(lt_shadows),	BU_STRUCTPARSE_FUNC_NULL },
    {"%d",	1, "s",		LIGHT_O(lt_shadows),	BU_STRUCTPARSE_FUNC_NULL },

    {"%d",	1, "infinite",	LIGHT_O(lt_infinite),	BU_STRUCTPARSE_FUNC_NULL },
    {"%d",	1, "i",		LIGHT_O(lt_infinite),	BU_STRUCTPARSE_FUNC_NULL },

    {"%d",	1, "visible",	LIGHT_O(lt_visible),	light_cvt_visible },
    {"%d",  1, "invisible",	LIGHT_O(lt_invisible),	light_cvt_visible },
    {"%d",	1, "v",		LIGHT_O(lt_visible),	light_cvt_visible },

    {"%f",	3, "pt",	LIGHT_OA(lt_parse_pt), light_pt_set },
    {"%f",	6, "pn",	LIGHT_OA(lt_parse_pt), light_pt_set },

    {"",	0, (char *)0,	0,			BU_STRUCTPARSE_FUNC_NULL }
};


struct light_specific	LightHead;	/* Heads linked list of lights */


HIDDEN int	light_setup(register struct region *rp, struct bu_vls *matparm, genptr_t *dpp, struct mfuncs *mfp, struct rt_i *rtip), light_render(struct application *ap, struct partition *pp, struct shadework *swp, char *dp);
HIDDEN void	light_print(register struct region *rp, char *dp);
void		light_free(char *cp);

struct mfuncs light_mfuncs[] = {
    {MF_MAGIC,	"light",	0,		MFI_NORMAL,	0,
     light_setup,	light_render,	light_print,	light_free },
    {0,		(char *)0,	0,		0,		0,
     0,		0,		0,		0 }
};


/*
 * This is a container for all the stuff that must be carried around when
 * doing the light obscuration/visibility calculations.
 */
struct light_obs_stuff {
    struct application *ap;
    struct shadework *swp;
    struct light_specific *lsp;
    int *rand_idx;

#ifdef RT_MULTISPECTRAL
    struct bn_tabdata **inten;
#else
    fastf_t *inten;
#endif
    int iter;
    vect_t to_light_center;	/* coordinate system on light */
    vect_t light_x;
    vect_t light_y;
};


/*
 *			A I M _ S E T
 *
 *  This routine is called by bu_struct_parse() if the "aim"
 *  qualifier is encountered, and causes lt_exaim to be set.
 */

HIDDEN void aim_set (const struct bu_structparse *sdp, const char *name, const char *base, char *value)
{
    register struct light_specific *lsp = (struct light_specific *)base;
    if (rdebug & RDEBUG_LIGHT )  {
	VPRINT("lt_target: ", lsp->lt_target);
    }
    lsp->lt_exaim = 1;
}

/*
 *			L I G H T _ R E N D E R
 *
 *  If we have a direct view of the light, return it's color.
 *  A cosine term is needed in the shading of the light source,
 *  to make it have dimension and shape.  However, just a simple
 *  cosine of the angle between the normal and the direction vector
 *  leads to a pretty dim looking light.  Therefore, a cos/2 + 0.5
 *  term is used when the viewer is within the beam, and a cos/2 term
 *  when the beam points away.
 */
HIDDEN int
light_render(struct application *ap, struct partition *pp, struct shadework *swp, char *dp)
{
    register struct light_specific *lsp = (struct light_specific *)dp;
    register fastf_t f;

    RT_CK_LIGHT(lsp);

    /* Provide cosine/2 shading, to make light look round */
    if ((f = -VDOT( swp->sw_hit.hit_normal, ap->a_ray.r_dir )*0.5) < 0 )
	f = 0;

    /* See if surface normal falls in light beam direction */
    if (VDOT( lsp->lt_aim, swp->sw_hit.hit_normal) < lsp->lt_cosangle )  {
	/* dark, outside of light beam area */
	f *= lsp->lt_fraction;
    } else {
	/* within beam area */
	f = (f+0.5) * lsp->lt_fraction;
    }
#ifdef RT_MULTISPECTRAL
    /* Support a shader having modified the temperature of the source */
    if (swp->sw_temperature > 0 )  {
	rt_spect_black_body( swp->msw_color, swp->sw_temperature, 5 );
	bn_tabdata_scale( swp->msw_color, swp->msw_color, f );
	if (rdebug & RDEBUG_LIGHT )  {
	    bu_log("light %s xy=%d,%d temp=%g\n",
		   pp->pt_regionp->reg_name, ap->a_x, ap->a_y,
		   swp->sw_temperature );
	}
    } else {
	bn_tabdata_scale( swp->msw_color, lsp->lt_spectrum, f );
    }
#else
    if (!PM_Activated) {
	VSCALE( swp->sw_color, lsp->lt_color, f );
    }
#endif
    return(1);
}

/*
 * When shooting grids of rays to generate the points on the light, we
 * add the hit point(s) to the list of points on the light.
 *
 */
static int
gen_hit(register struct application *ap,
	struct partition	    *PartHeadp,
	struct seg		    *sp)
{
    struct light_specific *lsp = (struct light_specific *)ap->a_uptr;
    struct soltab *stp;
    struct light_pt *lpt;
    struct partition *pp, *prev, *next;

    if ((pp=PartHeadp->pt_forw) == PartHeadp) return 0;

    for( ; pp != PartHeadp; pp = pp->pt_forw )  {

	if (pp->pt_regionp != lsp->lt_rp) continue;

	prev = pp->pt_back;
	/* check to make sure the light hit point isn't against some
	 * other object
	 */
	if (prev != PartHeadp) {
	    double delta;
	    delta = prev->pt_outhit->hit_dist -
		pp->pt_inhit->hit_dist;

	    /* XXX This really should compare to see if adj
	     * object is air
	     */
	    if (delta < 5.0 && delta > -5.0) {
		continue;
	    }
	}

	/* The inbound point is not against another object, so
	 * light will be emitted in this direction
	 */
	lpt = &lsp->lt_sample_pts[lsp->lt_pt_count++];

	stp = pp->pt_inseg->seg_stp;

	VJOIN1( lpt->lp_pt, ap->a_ray.r_pt,
		pp->pt_inhit->hit_dist, ap->a_ray.r_dir );

	RT_HIT_NORMAL( lpt->lp_norm, pp->pt_inhit, stp,
		       &(ap->a_ray), pp->pt_inflip );

	/* check to make sure the light out hit point isn't against
	 * some other object
	 */
	next = pp->pt_forw;
	if (next != PartHeadp) {
	    double delta;
	    delta = next->pt_inhit->hit_dist -
		pp->pt_outhit->hit_dist;

	    /* XXX This really should compare to see if adj
	     * object is air
	     */
	    if (delta < 5.0 && delta > -5.0) {
		continue;
	    }
	}
	/* The out point isn't against another object, so light
	 * will be emitted in this direction
	 */
	lpt = &lsp->lt_sample_pts[lsp->lt_pt_count++];

	stp = pp->pt_outseg->seg_stp;

	VJOIN1( lpt->lp_pt, ap->a_ray.r_pt,
		pp->pt_outhit->hit_dist, ap->a_ray.r_dir );

	RT_HIT_NORMAL( lpt->lp_norm, pp->pt_outhit, stp,
		       &(ap->a_ray), pp->pt_outflip );
    }
    return 1;
}

/*
 * When shooting the grids for building light pts, if we miss the light just
 * return;
 */
static int
gen_miss(register struct application *ap)
{
    return 0;
}

#if 0
/*
 *
 *  Shoot rays in each of the axis directions looking for hit points
 *  on a light.
 */
static void
shoot_grids(struct application *ap,
	    vect_t step,
	    point_t tree_min,
	    point_t tree_max)
{
    double x, y, z;
    struct light_specific *lsp = (struct light_specific *)ap->a_uptr;

    if (rdebug & RDEBUG_LIGHT )
	bu_log("shoot_grids Z, step=(%g, %g, %g)\n", V3ARGS(step) );

    /* shoot through the X,Y plane */
#define MIN_GRID_STEP_MM	0.1		/* had been 10.0 */

    if (step[Y] > MIN_GRID_STEP_MM && step[X] > MIN_GRID_STEP_MM) {
	z = tree_min[Z];

	VSET(ap->a_ray.r_dir, 0.0, 0.0, 1.0);
	z = tree_min[Z];

	for (y = tree_min[Y] + step[Y] * 0.5;
	     y < tree_max[Y] ; y += step[Y])

	    for (x = tree_min[X] + step[X] * 0.5;
		 x < tree_max[X] ; x += step[X]) {
		VSET(ap->a_ray.r_pt, x, y, z);
		(void)rt_shootray( ap );
	    }
    }

    if (rdebug & RDEBUG_LIGHT )
	bu_log("shoot_grids Y\n");

    /* shoot through the X, Z plane */
    if (step[Z] > MIN_GRID_STEP_MM && step[X] > MIN_GRID_STEP_MM) {
	y = tree_min[Y];

	VSET(ap->a_ray.r_dir, 0.0, 1.0, 0.0);
	z = tree_min[Z];

	for (z = tree_min[Z] + step[Z] * 0.5;
	     z < tree_max[Z] ; z += step[Z])

	    for (x = tree_min[X] + step[X] * 0.5;
		 x < tree_max[X] ; x += step[X]) {
		VSET(ap->a_ray.r_pt, x, y, z);
		(void)rt_shootray( ap );
	    }
    }

    /* shoot through the Y, Z plane */
    if (rdebug & RDEBUG_LIGHT )
	bu_log("shoot_grids X\n");
    if (step[Z] > MIN_GRID_STEP_MM && step[Y] > MIN_GRID_STEP_MM) {
	VSET(ap->a_ray.r_dir, 1.0, 0.0, 0.0);
	x = tree_min[X];

	for (z = tree_min[Z] + step[Z] * 0.5;
	     z < tree_max[Z] ; z += step[Z])

	    for (y = tree_min[Y] + step[Y] * 0.5;
		 y < tree_max[Y] ; y += step[Y]) {
		VSET(ap->a_ray.r_pt, x, y, z);
		(void)rt_shootray( ap );
	    }
    }

}
#endif
static void
ray_setup(struct application *ap,
	  point_t tree_min,
	  point_t tree_max,
	  point_t span)
{
    int face;
    point_t pt;
    static int idx = 0;

    /* pick a face of the bounding RPP at which we will start the ray */
    face = BN_RANDOM(idx) * 2.9999;

    switch (face) {
    case 0: /* XMIN */
	VSET(ap->a_ray.r_pt,
	     tree_min[X] - 10.0,
	     tree_min[Y] + BN_RANDOM(idx) * span[Y],
	     tree_min[Z] + BN_RANDOM(idx) * span[Z]);
	VSET(pt,
	     tree_max[X],
	     tree_min[Y] + BN_RANDOM(idx) * span[Y],
	     tree_min[Z] + BN_RANDOM(idx) * span[Z]);
	break;

    case 1: /* YMIN */
	VSET(ap->a_ray.r_pt,
	     tree_min[X] + BN_RANDOM(idx) * span[X],
	     tree_min[Y] - 10.0,
	     tree_min[Z] + BN_RANDOM(idx) * span[Z]);
	VSET(pt,
	     tree_min[X] + BN_RANDOM(idx) * span[X],
	     tree_max[Y],
	     tree_min[Z] + BN_RANDOM(idx) * span[Z]);
	break;

    case 2: /* ZMIN */
	VSET(ap->a_ray.r_pt,
	     tree_min[X] +
	     BN_RANDOM(idx) * span[X],

	     tree_min[Y] +
	     BN_RANDOM(idx) * span[Y],

	     tree_min[Z] - 10.0);
	VSET(pt,
	     tree_min[X] +
	     BN_RANDOM(idx) * span[X],

	     tree_min[Y] +
	     BN_RANDOM(idx) * span[Y],

	     tree_max[Z]);
	break;
    }
    VSUB2(ap->a_ray.r_dir, pt, ap->a_ray.r_pt);
    VUNITIZE(ap->a_ray.r_dir);

}


/*	L I G H T _ G E N _ S A M P L E _ P T S
 *
 *  Generate a set of sample points on the surface of the light with surface
 *  normals.
 */
void
light_gen_sample_pts(struct application    *upap,
		     struct light_specific  *lsp)
{
    struct application ap;
    point_t tree_min;
    point_t tree_max;
    vect_t  span;

    RT_CK_LIGHT(lsp);

    if (rdebug & RDEBUG_LIGHT )
	bu_log("light_gen_sample_pts(%s)\n", lsp->lt_name);


    memset(&ap, 0, sizeof(ap));
    ap.a_rt_i = upap->a_rt_i;
    ap.a_onehit = 0;
    ap.a_hit = gen_hit;
    ap.a_miss = gen_miss;
    ap.a_uptr = (genptr_t)lsp;

    /* get the bounding box of the light source */
    rt_bound_tree(lsp->lt_rp->reg_treetop, tree_min, tree_max);


    if (rdebug & RDEBUG_LIGHT ) {
	bu_log("\tlight bb (%g %g %g), (%g %g %g)\n",
	       V3ARGS(tree_min), V3ARGS(tree_max) );
    }


    /* if there is no space occupied by the light source, then
     * just give up
     */
    VSUB2(span, tree_max, tree_min);
    if (rdebug & RDEBUG_LIGHT ) {
	bu_log("\tspan %g %g %g\n", V3ARGS(span));
    }
    if (span[X] <= 0.0 && span[Y] <= 0.0 && span[Z] <= 0.0) {
	bu_log("\tSmall light. (treating as point source)\n");
	return;
    }

    while ( lsp->lt_pt_count < SOME_LIGHT_SAMPLES ) {

	ray_setup(&ap, tree_min, tree_max, span);
	(void)rt_shootray( &ap );
    }
#if 0
    if (rdebug & RDEBUG_LIGHT ) {
	int l;
	point_t p;
	struct light_pt *lpt = &lsp->lt_sample_pts[0];

	bu_log("\t%d light sample points\n", lsp->lt_pt_count);

	for (l=0 ; l < lsp->lt_pt_count ; l++, lpt++) {

	    VJOIN1(p, lpt->lp_pt, 100.0, lpt->lp_norm);

	    bu_log("\tV %g %g %g  %g %g %g\n",
		   V3ARGS(lpt->lp_pt), V3ARGS(p));
	}
    }
#endif
}

/*
 *			L I G H T _ S E T U P
 *
 *  Called once for each light-emitting region.
 */
HIDDEN int
light_setup(register struct region *rp,
	    struct bu_vls	   *matparm,
	    genptr_t		   *dpp,
	    struct mfuncs          *mfp,
	    struct rt_i            *rtip)  /* New since 4.4 release */
{
    register struct light_specific *lsp;
    register struct soltab *stp;
    vect_t	work;
    fastf_t	f;

    BU_CK_VLS( matparm );
    BU_GETSTRUCT( lsp, light_specific );

    BU_LIST_MAGIC_SET( &(lsp->l), LIGHT_MAGIC );
    lsp->lt_intensity = 1.0;	/* Lumens */
    lsp->lt_fraction = -1.0;		/* Recomputed later */
    lsp->lt_visible = 1;		/* explicitly modeled */
    lsp->lt_invisible = 0;		/* explicitly modeled */
    lsp->lt_shadows = 1;		/* by default, casts shadows */
    lsp->lt_angle = 180;		/* spherical emission by default */
    lsp->lt_exaim = 0;		/* use default aiming mechanism */
    lsp->lt_infinite = 0;
    lsp->lt_rp = rp;
    lsp->lt_pt_count = 0;
    lsp->lt_sample_pts = (struct light_pt *)NULL;
    lsp->lt_name = bu_strdup( rp->reg_name );

    if (bu_struct_parse( matparm, light_parse, (char *)lsp ) < 0 )  {
	light_free((char *)lsp);
	return(-1);
    }

    if (lsp->lt_angle > 180 )  lsp->lt_angle = 180;
    lsp->lt_cosangle = cos( (double) lsp->lt_angle * 0.0174532925199433 );

    /* Determine position and size */
    if (rp->reg_treetop->tr_op == OP_SOLID )  {

	stp = rp->reg_treetop->tr_a.tu_stp;
	VMOVE( lsp->lt_pos, stp->st_center );
	lsp->lt_radius = stp->st_aradius;
    } else {
	vect_t	min_rpp, max_rpp;
	vect_t	rad;
	register union tree *tp;

	if (rt_bound_tree( rp->reg_treetop, min_rpp, max_rpp ) < 0 )
	    return(-1);

	if (max_rpp[X] >= INFINITY )  {
	    bu_log("light_setup(%s) Infinitely large light sources not supported\n",
		   lsp->lt_name );
	    return(-1);
	}
	VADD2SCALE( lsp->lt_pos, min_rpp, max_rpp, 0.5 );
	VSUB2( rad, max_rpp, lsp->lt_pos );
	/* Use smallest radius from center to max as light radius */
	/* Having the radius too large can give very poor lighting */
	if (rad[X] < rad[Y] )
	    lsp->lt_radius = rad[X];
	else
	    lsp->lt_radius = rad[Y];
	if (rad[Z] < lsp->lt_radius )
	    lsp->lt_radius = rad[Z];

	/* Find first leaf node on left of tree */
	tp = rp->reg_treetop;
	while( tp->tr_op != OP_SOLID )
	    tp = tp->tr_b.tb_left;
	stp = tp->tr_a.tu_stp;
    }

    /* Light is aimed down -Z in it's local coordinate system */
    {
	register matp_t	matp;
	if ((matp = stp->st_matp) == (matp_t)0 )
	    matp = (matp_t)bn_mat_identity;
	if (lsp->lt_exaim) {
	    VSUB2 (work, lsp->lt_target, lsp->lt_pos);
	    VUNITIZE (work);
	}
	else VSET( work, 0, 0, -1 );
	MAT4X3VEC( lsp->lt_aim, matp, work );
	VUNITIZE( lsp->lt_aim );
    }

#ifdef RT_MULTISPECTRAL
    BN_GET_TABDATA(lsp->lt_spectrum, spectrum);
    if (rp->reg_mater.ma_temperature > 0 )  {
	rt_spect_black_body( lsp->lt_spectrum,
			     rp->reg_mater.ma_temperature, 5 );
	if (rdebug & RDEBUG_LIGHT )  {
	    bu_log("Light %s temp is %g degK, emission is pure black-body\n",
		   rp->reg_name, rp->reg_mater.ma_temperature);
	}
    } else if (rp->reg_mater.ma_color_valid )  {
	rt_spect_reflectance_rgb( lsp->lt_spectrum,
				  rp->reg_mater.ma_color );
	/* XXX Need to convert units of lumens (candela-sr) to ??
	 * mw/sr?  Use any old numbers to get started.
	 */
	bn_tabdata_scale( lsp->lt_spectrum, lsp->lt_spectrum,
			  lsp->lt_intensity * 0.001 ); /* XXX */
    } else {
	/* Default: Perfectly even emission across whole spectrum */
	bn_tabdata_constval( lsp->lt_spectrum, 0.001 );
    }
#else
    if (rp->reg_mater.ma_color_valid )  {
	VMOVE( lsp->lt_color, rp->reg_mater.ma_color );
    } else {
	VSETALL( lsp->lt_color, 1 );
    }
#endif

    VMOVE( lsp->lt_vec, lsp->lt_pos );
    f = MAGNITUDE( lsp->lt_vec );
    if (f < SQRT_SMALL_FASTF ) {
	/* light at the origin, make its direction vector up */
	VSET( lsp->lt_vec, 0, 0, 1 );
    } else {
	VSCALE( lsp->lt_vec, lsp->lt_vec, f );
    }

    /* Add to linked list of lights */
    if (BU_LIST_UNINITIALIZED( &(LightHead.l ) ) )  {
	BU_LIST_INIT( &(LightHead.l) );
    }
    BU_LIST_INSERT( &(LightHead.l), &(lsp->l) );

    if (rdebug&RDEBUG_LIGHT) {
	light_print(rp, (char *)lsp);
    }
    if (lsp->lt_invisible )  {
	return(2);	/* don't show light, destroy it later */
    }

    *dpp = (genptr_t)lsp;	/* Associate lsp with reg_udata */
    return(1);
}

/*
 *			L I G H T _ P R I N T
 */
HIDDEN void
light_print(register struct region *rp, char *dp)
{
    bu_struct_print(rp->reg_name, light_print_tab, (char *)dp);
}

/*
 *			L I G H T _ F R E E
 */
void
light_free(char *cp)
{
    register struct light_specific *lsp = (struct light_specific *)cp;

    RT_CK_LIGHT(lsp);
    BU_LIST_DEQUEUE( &(lsp->l) );
    if (lsp->lt_name )  {
	bu_free( lsp->lt_name, "light name" );
	lsp->lt_name = (char *)0;
    }
    if (lsp->lt_sample_pts) {
	bu_free(lsp->lt_sample_pts, "free light samples array");
    }
    lsp->l.magic = 0;	/* sanity */
    bu_free( (char *)lsp, "light_specific" );
}

/*
 *			L I G H T _ M A K E R
 *
 *  Special hook called by view_2init to build 1 or 3 debugging lights.
 */
void
light_maker(int num, mat_t v2m)
{
    register struct light_specific *lsp;
    register int i;
    vect_t	temp;
    vect_t	color;
    char	name[64];
#ifdef RT_MULTISPECTRAL
    float	fcolor[3];
#endif
    /* Determine the Light location(s) in view space */
    for( i=0; i<num; i++ )  {
	switch(i)  {
	case 0:
	    /* 0:  At left edge, 1/2 high */
	    VSET( color, 1,  1,  1 );	/* White */
	    VSET( temp, -1, 0, 1 );
	    break;

	case 1:
	    /* 1: At right edge, 1/2 high */
	    VSET( color,  1, 1, 1 );
	    VSET( temp, 1, 0, 1 );
	    break;

	case 2:
	    /* 2:  Behind, and overhead */
	    VSET( color, 1, 1,  1 );
	    VSET( temp, 0, 1, -0.5 );
	    break;

	default:
	    return;
	}
#if 0
	{  /* debugging ascii plot commands drawing from point location to origin */
	    vect_t tmp;
	    static vect_t pt = {0.0, 0.0, 0.0};
	    MAT4X3PNT(tmp, v2m, temp); bu_log("C 0 255 255\nO %g %g %g\n", V3ARGS(tmp));
	    MAT4X3PNT(tmp, v2m, pt); bu_log("Q %g %g %g\n", V3ARGS(tmp));
	}
#endif

	BU_GETSTRUCT( lsp, light_specific );
	lsp->l.magic = LIGHT_MAGIC;
#ifdef RT_MULTISPECTRAL

	BN_GET_TABDATA(lsp->lt_spectrum, spectrum);
	VMOVE(fcolor, color);
	rt_spect_reflectance_rgb( lsp->lt_spectrum, fcolor );
	bn_tabdata_scale( lsp->lt_spectrum, lsp->lt_spectrum, 1000.0 );

#else
	VMOVE( lsp->lt_color, color );
#endif
	MAT4X3PNT( lsp->lt_pos, v2m, temp );
	VMOVE( lsp->lt_vec, lsp->lt_pos );
	VUNITIZE( lsp->lt_vec );

	sprintf(name, "Implicit light %d", i);
	lsp->lt_name = bu_strdup(name);

	/* XXX Is it bogus to set lt_aim? */
	VSET( lsp->lt_aim, 0, 0, -1 );	/* any direction: spherical */
	lsp->lt_intensity = 1.0;
	lsp->lt_radius = 0.1;		/* mm, "point" source */
	lsp->lt_visible = 0;		/* NOT explicitly modeled */
	lsp->lt_invisible = 1;		/* NOT explicitly modeled */
	lsp->lt_shadows = 0;		/* no shadows for speed */
	lsp->lt_angle = 180;		/* spherical emission */
	lsp->lt_cosangle = -1;		/* cos(180) */
	lsp->lt_infinite = 0;
	lsp->lt_rp = REGION_NULL;
	if (BU_LIST_UNINITIALIZED( &(LightHead.l ) ) )  {
	    BU_LIST_INIT( &(LightHead.l) );
	}
	BU_LIST_INSERT( &(LightHead.l), &(lsp->l) );
    }
}

/*
 *			L I G H T _ I N I T
 *
 *  Special routine called by view_2init() to determine the relative
 *  intensities of each light source.
 *
 *  Because of the limited dynamic range of RGB space (0..255),
 *  the strategy used here is a bit risky.  We find the brightest
 *  single light source in the model, and assume that the energy from
 *  multiple lights will not shine on a single location in such a way
 *  as to add up to an overload condition.
 *  We then account for the effect of ambient light, because it always
 *  adds it's contribution.  Even here we only expect 50% of the ambient
 *  intensity, to keep the pictures reasonably bright.
 */
int
light_init(struct application *ap)
{
    register struct light_specific *lsp;
    register int		nlights = 0;
    register fastf_t	inten = 0.0;

    if (BU_LIST_UNINITIALIZED( &(LightHead.l ) ) )  {
	BU_LIST_INIT( &(LightHead.l) );
    }


    for( BU_LIST_FOR( lsp, light_specific, &(LightHead.l) ) )  {
	nlights++;
	if (lsp->lt_fraction > 0 )  continue;	/* overridden */
	if (lsp->lt_intensity <= 0 )
	    lsp->lt_intensity = 1;		/* keep non-neg */
	if (lsp->lt_intensity > inten )
	    inten = lsp->lt_intensity;
    }

    /* Compute total emitted energy, including ambient */
    /**	inten *= (1 + AmbientIntensity); **/
    /* This is non-physical and risky, but gives nicer pictures for now */
    inten *= (1 + AmbientIntensity*0.5);

    for( BU_LIST_FOR( lsp, light_specific, &(LightHead.l) ) )  {
	RT_CK_LIGHT(lsp);
	if (lsp->lt_fraction > 0 )  continue;	/* overridden */
#ifdef RT_MULTISPECTRAL
	lsp->lt_fraction = 1.0;	/* always use honest intensity values */
#else
	lsp->lt_fraction = lsp->lt_intensity / inten;
#endif
    }

    /*
     * Make sure we have sample points for all light sources in the scene
     */
    for( BU_LIST_FOR( lsp, light_specific, &(LightHead.l) ) )  {
	RT_CK_LIGHT(lsp);
	if (lsp->lt_shadows > 1 && ! lsp->lt_infinite && lsp->lt_pt_count < 1)
	    light_gen_sample_pts(ap, lsp);
    }


    if (R_DEBUG) {
	bu_log("Lighting: Ambient = %d%%\n",
	       (int)(AmbientIntensity*100));

	for( BU_LIST_FOR( lsp, light_specific, &(LightHead.l) ) )  {
	    RT_CK_LIGHT(lsp);
	    bu_log( "  %s: (%g, %g, %g), aimed at (%g, %g, %g)\n",
		    lsp->lt_name,
		    lsp->lt_pos[X], lsp->lt_pos[Y], lsp->lt_pos[Z],
		    lsp->lt_aim[X], lsp->lt_aim[Y], lsp->lt_aim[Z] );
	    bu_log( "  %s: %s, %s, %g lumens (%d%%), halfang=%g\n",
		    lsp->lt_name,
		    lsp->lt_visible ? "visible":"invisible",
		    lsp->lt_shadows ? "casts shadows":"no shadows",
		    lsp->lt_intensity,
		    (int)(lsp->lt_fraction*100),
		    lsp->lt_angle );

	    if ( lsp->lt_pt_count > 0) {
		int samp;

		bu_log( "  %d sample points\n", lsp->lt_pt_count);
		for (samp = 0 ; samp < lsp->lt_pt_count ; samp++) {
		    bu_log("     pt %g %g %g N %g %g %g\n",
			   V3ARGS(lsp->lt_sample_pts[samp].lp_pt),
			   V3ARGS(lsp->lt_sample_pts[samp].lp_norm) );
		}
	    }
	}
    }
    if (nlights > SW_NLIGHTS )  {
	bu_log("Number of lights limited to %d\n", SW_NLIGHTS);
	nlights = SW_NLIGHTS;
    }
    return(nlights);
}


/*
 *			L I G H T _ C L E A N U P
 *
 *  Called from view_end().
 *  Take care of releasing storage for any lights which will not
 *  be cleaned up by mlib_free():
 *	implicitly created lights, because they have no associated region, and
 *	invisible lights, because their region was destroyed.
 */
void
light_cleanup(void)
{
    register struct light_specific *lsp, *zaplsp;

    if (BU_LIST_UNINITIALIZED( &(LightHead.l ) ) )  {
	BU_LIST_INIT( &(LightHead.l) );
	return;
    }
    for( BU_LIST_FOR( lsp, light_specific, &(LightHead.l) ) )  {
	RT_CK_LIGHT(lsp);
	if (lsp->lt_rp != REGION_NULL && lsp->lt_visible )  {
	    /* Will be cleaned up by mlib_free() */
	    continue;
	}
	zaplsp = lsp;
	lsp = BU_LIST_PREV( light_specific, &(lsp->l) );
	light_free( (genptr_t)zaplsp );
    }
}
/**********************************************************************/
/*
 *			L I G H T _ H I T
 *
 *  A light visibility test ray hit something.  Determine what this means.
 *
 *  Input -
 *	a_color[] contains the fraction of a the light that will be
 *	propagated back along the ray, so far.  If this gets too small,
 *	recursion through lots of glass ought to stop.
 *  Output -
 *	a_color[] contains the fraction of light that can be seen.
 *	RGB transmissions are separately indicated, to allow simplistic
 *	colored glass (with apologies to Roy Hall).
 *
 *  a_spectrum is used in place of a_color for multispectral renderings.
 *
 *  These shadow functions return a boolean "light_visible".
 *
 *  This is a simplified algorithm, and could be improved.
 *  Reflected light can't be dealt with at all.
 *
 *  Would also be nice to return an actual energy level, rather than
 *  a boolean, which could account for distance, etc.
 */

int
light_hit(struct application *ap, struct partition *PartHeadp, struct seg *finished_segs)
{
    register struct partition *pp;
    register struct region	*regp = NULL;
    struct application	sub_ap;
    struct shadework	sw;
    const struct light_specific	*lsp;
    extern int	light_render(struct application *ap, struct partition *pp, struct shadework *swp, char *dp);
#ifdef RT_MULTISPECTRAL
    struct bn_tabdata	*ms_filter_color = BN_TABDATA_NULL;
#else
    vect_t	filter_color;
#endif
    int	light_visible = 0;
    int	air_sols_seen = 0;
    int 	is_proc;
    char	*reason = "???";

    RT_CK_PT_HD(PartHeadp);

    memset(&sw, 0, sizeof(sw));		/* make sure nothing nasty on the stack */
    if (rdebug&RDEBUG_LIGHT)
	bu_log("light_hit level %d %d\n", ap->a_level, __LINE__);

#ifdef RT_MULTISPECTRAL
    sub_ap.a_spectrum = BN_TABDATA_NULL;	/* sanity */
    BN_CK_TABDATA(ap->a_spectrum);
#endif

    BU_CK_LIST_HEAD(&finished_segs->l);

    lsp = (struct light_specific *)(ap->a_uptr);
    RT_CK_LIGHT(lsp);

#ifdef RT_MULTISPECTRAL
    ms_filter_color = bn_tabdata_get_constval( 1.0, spectrum );
    BN_GET_TABDATA( sw.msw_color, spectrum );
    BN_GET_TABDATA( sw.msw_basecolor, spectrum );
#else
    VSETALL( filter_color, 1 );
#endif

    /* anything to do? */
    if (PartHeadp->pt_forw == PartHeadp) {
	bu_log("light_hit:  ERROR, EMPTY PARTITION sxy=(%d, %d)\n", ap->a_x, ap->a_y);
	light_visible = 0;
	reason = "ERROR: EMPTY PARTITION";
	goto out;
    }

    /*XXX Bogus with Air.  We should check to see if it is the same
     * surface.
     *
     *  Since the light visibility ray started at the surface of a solid,
     *  it is likely that the solid will be the first partition on
     *  the list, with pt_outhit->hit_dist being roughly zero.
     *  Don't start using partitions until pt_inhit->hit_dist is
     *  slightly larger than zero, i.e., that the partition is not
     *  including the start point.
     *  The outhit distance needs to be checked too, so that if the
     *  partition is heading through the solid toward the light
     *  e.g. (-1,+50), then the fact that the light is obscured will
     *  not be missed.
     */
    for( pp=PartHeadp->pt_forw; pp != PartHeadp; pp = pp->pt_forw )  {
	if (pp->pt_regionp->reg_aircode != 0 )  {
	    /* Accumulate transmission through each air lump */
	    air_sols_seen++;

	    /* Obtain opacity of this region, multiply */
	    sw.sw_inputs = 0;
	    sw.sw_transmit = sw.sw_reflect = 0.0;
	    sw.sw_refrac_index = 1.0;
	    sw.sw_xmitonly = 1;	/* only want sw_transmit */
	    sw.sw_segs = finished_segs;
#ifdef RT_MULTISPECTRAL
	    bn_tabdata_constval( sw.msw_color, 1.0 );
	    bn_tabdata_constval( sw.msw_basecolor, 1.0 );
#else
	    VSETALL( sw.sw_color, 1 );
	    VSETALL( sw.sw_basecolor, 1 );
#endif
	    if (rdebug&RDEBUG_LIGHT) bu_log("calling viewshade\n");
	    (void)viewshade( ap, pp, &sw );
	    if (rdebug&RDEBUG_LIGHT) bu_log("viewshade returns\n");
	    /* sw_transmit is only return */

	    /* XXX Clouds don't yet attenuate differently based on freq */
#ifdef RT_MULTISPECTRAL
	    bn_tabdata_scale( ms_filter_color, ms_filter_color,
			      sw.sw_transmit );
#else
	    VSCALE( filter_color, filter_color, sw.sw_transmit );
#endif
	    continue;
	}
	if (pp->pt_inhit->hit_dist >= ap->a_rt_i->rti_tol.dist )
	    break;
	if (pp->pt_outhit->hit_dist >= ap->a_rt_i->rti_tol.dist*10 )
	    break;
    }


    if (pp == PartHeadp )  {
	if (rdebug&RDEBUG_LIGHT) bu_log("pp == PartHeadp\n");

	pp=PartHeadp->pt_forw;
	RT_CK_PT(pp);

	if (lsp->lt_invisible || lsp->lt_infinite )  {
	    light_visible = 1;
#ifdef RT_MULTISPECTRAL
	    bn_tabdata_copy( ap->a_spectrum, ms_filter_color );
#else
	    VMOVE( ap->a_color, filter_color );
#endif
	    reason = "Unobstructed invisible/infinite light";
	    goto out;
	}

	if (air_sols_seen > 0 )  {
	    light_visible = 1;
#ifdef RT_MULTISPECTRAL
	    bn_tabdata_copy( ap->a_spectrum, ms_filter_color );
#else
	    VMOVE( ap->a_color, filter_color );
#endif
	    /* XXXXXXX This seems to happen with *every*
	     * light vis ray through air
	     */
	    reason = "Off end of partition list, air was seen";
	    goto out;
	}

	if (pp->pt_inhit->hit_dist <= ap->a_rt_i->rti_tol.dist) {
	    int retval;
	    /* XXX This is bogus if air is being used */
	    /* What has probably happened is that the shadow ray
	     * has produced an Out-hit from the current solid
	     * which looks valid, but is in fact an intersection
	     * with the current hit point.
	     */

	    sub_ap = *ap;	/* struct copy */
	    sub_ap.a_level++;
#ifdef RT_MULTISPECTRAL
	    sub_ap.a_spectrum = bn_tabdata_dup( ap->a_spectrum );
#endif
	    /* pt_outhit->hit_point has not been calculated */
	    VJOIN1(sub_ap.a_ray.r_pt, ap->a_ray.r_pt,
		   pp->pt_outhit->hit_dist, ap->a_ray.r_dir);

	    if (rdebug&RDEBUG_LIGHT) bu_log("hit_dist < tol\n");
	    retval = rt_shootray( &sub_ap );

	    ap->a_user = sub_ap.a_user;
	    ap->a_uptr = sub_ap.a_uptr;
#ifdef RT_MULTISPECTRAL
	    bn_tabdata_copy( ap->a_spectrum, sub_ap.a_spectrum );
#else
	    ap->a_color[0] = sub_ap.a_color[0];
	    ap->a_color[1] = sub_ap.a_color[1];
	    ap->a_color[2] = sub_ap.a_color[2];
#endif
	    VMOVE(ap->a_uvec, sub_ap.a_uvec);
	    VMOVE(ap->a_vvec, sub_ap.a_vvec);
	    ap->a_refrac_index = sub_ap.a_refrac_index;
	    ap->a_cumlen = sub_ap.a_cumlen;
	    ap->a_return = sub_ap.a_return;

	    light_visible = retval;
	    reason = "pressed on past start point";
	    goto out;
	}


	bu_log("light_hit:  ERROR, nothing hit, sxy=%d,%d, dtol=%e\n",
	       ap->a_x, ap->a_y,
	       ap->a_rt_i->rti_tol.dist);
	rt_pr_partitions(ap->a_rt_i, PartHeadp, "light_hit pt list");
	light_visible = 0;
	reason = "error, nothing hit";
	goto out;
    }

    regp = pp->pt_regionp;

    /* Check to see if we hit the light source */
    if (lsp->lt_rp == regp )  {
#ifdef RT_MULTISPECTRAL
	bn_tabdata_copy( ap->a_spectrum, ms_filter_color );
#else
	VMOVE( ap->a_color, filter_color );
#endif
	light_visible = 1;
	reason = "hit light";
	goto out;
    }

    /* if the region we hit is a light source be generous */
#if 1
    {
	struct light_specific *lsp;
	for( BU_LIST_FOR( lsp, light_specific, &(LightHead.l) ) )  {
	    if (lsp->lt_rp == regp) {
#ifdef RT_MULTISPECTRAL
		bn_tabdata_copy( ap->a_spectrum, ms_filter_color );
#else
		VMOVE( ap->a_color, filter_color );
#endif
		light_visible = 1;
		reason = "hit light";
		goto out;

	    }
	}
    }
#endif
    /* or something futher away than a finite invisible light */
    if (lsp->lt_invisible && !(lsp->lt_infinite) ) {
	vect_t	tolight;
	VSUB2( tolight, lsp->lt_pos, ap->a_ray.r_pt );
	if (pp->pt_inhit->hit_dist >= MAGNITUDE(tolight) ) {
#ifdef RT_MULTISPECTRAL
	    bn_tabdata_copy( ap->a_spectrum, ms_filter_color );
#else
	    VMOVE( ap->a_color, filter_color );
#endif
	    light_visible = 1;
	    reason = "hit behind invisible light ==> hit light";
	    goto out;
	}
    }

    /* If we hit an entirely opaque object, this light is invisible */
    is_proc = ((struct mfuncs *)regp->reg_mfuncs)->mf_flags|MFF_PROC;


    if (pp->pt_outhit->hit_dist >= INFINITY ||
	(regp->reg_transmit == 0 &&
	 ! is_proc /* procedural shader */) ) {

#ifdef RT_MULTISPECTRAL
	bn_tabdata_constval( ap->a_spectrum, 0.0 );
#else
	VSETALL( ap->a_color, 0 );
#endif
	light_visible = 0;
	reason = "hit opaque object";
	goto out;
    }

#ifdef RT_MULTISPECTRAL
    /* XXX Check area under spectral curve?  What power level for thresh? */
#else
    /*  See if any further contributions will mater */
    if (ap->a_color[0] + ap->a_color[1] + ap->a_color[2] < 0.01 )  {
	/* Any light energy is "fully" attenuated by here */
	VSETALL( ap->a_color, 0 );
	light_visible = 0;
	reason = "light fully attenuated before shading";
	goto out;
    }
#endif

    /*
     *  Determine transparency parameters of this object.
     *  All we really need here is the opacity information;
     *  full shading is not required.
     */
    sw.sw_inputs = 0;
    sw.sw_transmit = sw.sw_reflect = 0.0;
    sw.sw_refrac_index = 1.0;
    sw.sw_xmitonly = 1;		/* only want sw_transmit */
    sw.sw_segs = finished_segs;
#ifdef RT_MULTISPECTRAL
    bn_tabdata_constval( sw.msw_color, 1.0 );
    bn_tabdata_constval( sw.msw_basecolor, 1.0 );
#else
    VSETALL( sw.sw_color, 1 );
    VSETALL( sw.sw_basecolor, 1 );
#endif

    if (rdebug&RDEBUG_LIGHT) bu_log("calling viewshade\n");
    (void)viewshade( ap, pp, &sw );
    if (rdebug&RDEBUG_LIGHT) bu_log("viewshade back\n");
    /* sw_transmit is output */

#ifdef RT_MULTISPECTRAL
    bn_tabdata_scale( ms_filter_color, ms_filter_color, sw.sw_transmit );
    /* XXX Power level check again? */
#else
    VSCALE( filter_color, filter_color, sw.sw_transmit );
    if (filter_color[0] + filter_color[1] + filter_color[2] < 0.01 )  {
	/* Any recursion won't be significant */
	VSETALL( ap->a_color, 0 );
	light_visible = 0;
	reason = "light fully attenuated after shading";
	goto out;
    }
#endif
    /*
     * Push on to exit point, and trace on from there.
     * Transmission so far is passed along in sub_ap.a_color[];
     * Don't even think of trying to refract, or we will miss the light!
     */
    sub_ap = *ap;			/* struct copy */
    sub_ap.a_level = ap->a_level+1;
#ifdef RT_MULTISPECTRAL
    sub_ap.a_spectrum = bn_tabdata_dup( ap->a_spectrum );
#endif
    {
	register fastf_t f;
	f = pp->pt_outhit->hit_dist + ap->a_rt_i->rti_tol.dist;
	VJOIN1(sub_ap.a_ray.r_pt, ap->a_ray.r_pt, f, ap->a_ray.r_dir);
    }
    sub_ap.a_purpose = "light transmission after filtering";
    if (rdebug&RDEBUG_LIGHT)
	bu_log("shooting level %d from %d\n",
	       sub_ap.a_level, __LINE__);
    light_visible = rt_shootray( &sub_ap );
    if (rdebug&RDEBUG_LIGHT)
	if (light_visible < 0)
	    bu_log("%s:%d\n", __FILE__, __LINE__);

#ifdef RT_MULTISPECTRAL
    bn_tabdata_mul( ap->a_spectrum, sub_ap.a_spectrum, ms_filter_color );
#else
    VELMUL( ap->a_color, sub_ap.a_color, filter_color );
#endif
    reason = "after filtering";
 out:

#ifdef RT_MULTISPECTRAL
    if (ms_filter_color ) bn_tabdata_free( ms_filter_color );
    if (sw.msw_color )  bn_tabdata_free( sw.msw_color );
    if (sw.msw_basecolor ) bn_tabdata_free( sw.msw_basecolor );
    if (sub_ap.a_spectrum )  bn_tabdata_free( sub_ap.a_spectrum );
    if (rdebug & RDEBUG_LIGHT )  {
	bu_log("light vis=%d %s %s %s  ",
	       light_visible,
	       lsp->lt_name,
	       reason,
	       regp ? regp->reg_name : "" );
	bn_pr_tabdata("light spectrum", ap->a_spectrum);
    }
#else
    if (rdebug & RDEBUG_LIGHT ) bu_log("light vis=%d %s (%4.2f, %4.2f, %4.2f) %s %s\n",
				       light_visible,
				       lsp->lt_name,
				       V3ARGS(ap->a_color), reason,
				       regp ? regp->reg_name : "" );
#endif
    return(light_visible);
}

/*
 *  			L I G H T _ M I S S
 *
 *  If there is no explicit light solid in the model, we will always "miss"
 *  the light, so return light_visible = TRUE.
 */
/* ARGSUSED */
int
light_miss(register struct application *ap)
{
    struct light_specific *lsp = (struct light_specific *)(ap->a_uptr);

    RT_CK_LIGHT(lsp);
    if (lsp->lt_invisible || lsp->lt_infinite ) {
	VSETALL( ap->a_color, 1 );
	if (rdebug & RDEBUG_LIGHT ) bu_log("light_miss vis=1\n");
	return(1);		/* light_visible = 1 */
    }

    if (rdebug & RDEBUG_LIGHT) {
	bu_log("light ray missed non-infinite, visible light source\n");
	bu_log("on pixel: %d %d\n", ap->a_x, ap->a_y);
	bu_log("ray: (%g %g %g) -> %g %g %g\n", V3ARGS(ap->a_ray.r_pt), V3ARGS(ap->a_ray.r_dir) );
	bu_log("a_level: %d\n", ap->a_level);
    }

    /* Missed light, either via blockage or dither.  Return black */
    VSETALL( ap->a_color, 0 );
    if (rdebug & RDEBUG_LIGHT ) bu_log("light_miss vis=0\n");
    return(-1);			/* light_visible = 0 */
}

#define VF_SEEN 1
#define VF_BACKFACE 2
/***********************************************************************
 *
 *	light_vis
 *
 *	Compute 1 light visibility ray from a hit point to the light.
 *
 */
static int
light_vis(struct light_obs_stuff *los, char *flags)
{
    struct application sub_ap;
    double radius = 0.0;
    double angle = 0.0;
    double cos_angle, x, y;
    point_t shoot_pt;
    vect_t shoot_dir;
    int shot_status;
    vect_t dir, rdir;
    int idx;
    int k = 0;
    struct light_pt *lpt;
    int tryagain = 0;
    double VisRayvsLightN;
    double VisRayvsSurfN;

    if (rdebug & RDEBUG_LIGHT ) bu_log("light_vis\n");

 retry:

    /* compute the light direction */
    if (los->lsp->lt_infinite ) {
	/* Infinite lights are point sources, no fuzzy penumbra */
	VMOVE( shoot_dir, los->lsp->lt_vec );

    } else if (los->lsp->lt_pt_count > 0) {
	static const vect_t zero = { 0.0, 0.0, 0.0 };

	/* pick a point at random from the list of points on
	 * the surface of the light.  If the normals indicate
	 * inter-visibility, then shoot at that point
	 */

	idx = los->lsp->lt_pt_count *
	    fabs(bn_rand_half(los->ap->a_resource->re_randptr)) *
	    2.0;
	if (idx == los->lsp->lt_pt_count) idx--;

    reusept:

	for (k=idx ; ((k+1) % los->lsp->lt_pt_count) != idx ;
	     k = (k+1) % los->lsp->lt_pt_count) {
	    if (rdebug & RDEBUG_LIGHT )
		bu_log("checking sample pt %d\n", k);

	    if (flags[k] & VF_SEEN ) continue;
	    if (flags[k] & VF_BACKFACE ) continue;

	    /* we've got a candidate, check for backfacing */
	    if (rdebug & RDEBUG_LIGHT )
		bu_log("\tpossible sample pt %d\n", k);

	    lpt = &los->lsp->lt_sample_pts[k];

	    VSUB2(dir, lpt->lp_pt, los->swp->sw_hit.hit_point);
	    VUNITIZE(dir);
	    VREVERSE(rdir, dir);


	    /* if the surface normals of the light and hit point
	     * indicate that light could pass between the two
	     * points, then we have a good choice
	     *
	     * If the light point has no surface normal, then
	     * this is a general point usable from any angle
	     * so again we can shoot at this point
	     *
	     * We tolerance this down so that light points which
	     * are in the plane of the hit point are not candidates
	     * (since the light on the surface from such would be
	     * very small).  We also tolerance the normal on the
	     * light to the visibility ray so that points on the
	     * perimiter of the presented area of the light source
	     * are not chosen.  This helps avoid shooting at points
	     * on the light source which machine floating-point
	     * inaccuracies would cause the ray to miss.
	     */
#define COSINE89_99DEG 0.0001745329

	    VisRayvsSurfN
		= VDOT(los->swp->sw_hit.hit_normal, dir);

	    if ( VEQUAL(lpt->lp_norm, zero) ) {
		VisRayvsLightN = 1.0;
	    } else {
		VisRayvsLightN = VDOT(lpt->lp_norm, rdir);
	    }


	    if ( VisRayvsLightN > COSINE89_99DEG &&
		   VisRayvsSurfN > COSINE89_99DEG ) {

		/* ok, we can shoot at this sample point */
		if (rdebug & RDEBUG_LIGHT )
		    bu_log("\tPt %d selected... OK normal %g %g %g\n",
			   k, V3ARGS(lpt->lp_norm));

		flags[k] |= VF_SEEN;

		goto done;
	    }
#undef COSINE89DEG
	    if (rdebug & RDEBUG_LIGHT ) {
		bu_log("\tbackfacing\n");
		bu_log("VisRayvsLightN %g\n", VisRayvsLightN);
		bu_log("VisRayvsSurfN %g\n", VisRayvsSurfN);
		VPRINT("norm", lpt->lp_norm);
		VPRINT("zero", zero);
	    }
	    /* the sample point is backfacing to the location
	     * we want to test from
	     */
	    flags[k] |= VF_BACKFACE;
	}

	/* if we get here, then everything is used or backfacing */

	if (rdebug & RDEBUG_LIGHT ) {
	    bu_log("all light sample pts used.  trying to recycle\n");
	}

	tryagain = 0;
	for (k=0 ; k < los->lsp->lt_pt_count ; k++) {
	    if (flags[k] & VF_SEEN ) {
		/* this one was used, we can re-use it */
		tryagain = 1;
		flags[k] &= VF_BACKFACE;
	    }
	}
	if (tryagain) {
	    if (rdebug & RDEBUG_LIGHT ) {
		bu_log("recycling\n");
	    }
	    goto reusept;
	}
	/* at this point, we have no candidate points available to
	 * shoot at
	 */
	if (rdebug & RDEBUG_LIGHT ) {
	    bu_log("can't find point to shoot at\n");
	}
	return 0;
    done:
	/* we've got a point on the surface of the light to shoot at */
	VMOVE(shoot_pt, lpt->lp_pt);
	VSUB2(shoot_dir, shoot_pt, los->swp->sw_hit.hit_point);

    } else {

	if (rdebug & RDEBUG_LIGHT )
	    bu_log("shooting at approximating sphere\n");
	/* We're going to shoot at a point on the apporximating
	 * sphere for the light source.  We pick a point on the
	 * circle (presented area) for the light source from this
	 * angle.  This is done by picking random radius and angle
	 * values on the disc.
	 */
	radius = los->lsp->lt_radius *
	    /*			drand48(); */
	    fabs(bn_rand_half(los->ap->a_resource->re_randptr)
		 * 2.0);
	angle =  M_PI * 2.0 *
	    /*			drand48(); */
	    (bn_rand_half(los->ap->a_resource->re_randptr) + 0.5);

	y = radius * bn_tab_sin(angle);

	/* by adding 90 degrees to the angle, the sin of the new
	 * angle becomes the cosine of the old angle.  Thus we
	 * can use the sine table to compute the value, and avoid
	 * the expensive actual computation.  So the next 3 lines
	 * replace:
	 *		x = radius * cos(angle);
	 */
	cos_angle = M_PI_2 + angle;
	if (cos_angle > (2.0*M_PI)) cos_angle -= (2.0*M_PI);
	x = radius * bn_tab_sin(cos_angle);

	VJOIN2(shoot_pt, los->lsp->lt_pos,
	       x, los->light_x,
	       y, los->light_y);

	if (rdebug & RDEBUG_LIGHT) {
	    bu_log("light at (%g %g %g) radius %g\n",
		   V3ARGS(los->lsp->lt_pos),
		   los->lsp->lt_radius);

	    bu_log("\tshooting at radius %g\n", radius);

	    bu_log("\ttarget light point %g %g %g\n",
		   V3ARGS(shoot_pt));
	}
	VSUB2(shoot_dir, shoot_pt, los->swp->sw_hit.hit_point);

    }

    if (rdebug & RDEBUG_LIGHT) {
	VPRINT("shoot_dir", shoot_dir);
    }


    if (rdebug& RDEBUG_RAYPLOT) {
	point_t ray_endpt;

	/* Yelow -- light visibility ray */
	VADD2(ray_endpt, los->swp->sw_hit.hit_point, shoot_dir);
	bu_semaphore_acquire( BU_SEM_SYSCALL );
	pl_color(stdout, 200, 200, 0);
	pdv_3line(stdout, los->swp->sw_hit.hit_point, ray_endpt);
	bu_semaphore_release( BU_SEM_SYSCALL );
    }

    VUNITIZE( shoot_dir );


    /*
     * See if ray from hit point to light lies within light beam
     * Note: this is should always be true for infinite lights!
     */
    if (-VDOT(shoot_dir, los->lsp->lt_aim) < los->lsp->lt_cosangle )  {
	/* dark (outside of light beam) */
	if (rdebug & RDEBUG_LIGHT)
	    bu_log("point outside beam, obscured: %s\n",
		   los->lsp->lt_name);
	return 0;
    }


    if (!(los->lsp->lt_shadows) )  {
	/* "fill light" in beam, don't care about shadows */
	if (rdebug & RDEBUG_LIGHT)
	    bu_log("fill light, no shadow, visible: %s\n",
		   los->lsp->lt_name);
#ifdef RT_MULTISPECTRAL
	/* XXX Need a power level for this! */
	bn_tabdata_constval( ((struct bn_tabdata *)los->inten), 1.0);
#else
	VSETALL( ((vectp_t)los->inten), 1 );
#endif

	return -1;
    }


    /*
     *  Fire ray at light source to check for shadowing.
     *  (This SHOULD actually return an energy spectrum).
     *  Advance start point slightly off surface.
     */
    sub_ap = *los->ap;			/* struct copy */
    RT_CK_AP(&sub_ap);

    VMOVE( sub_ap.a_ray.r_dir, shoot_dir );
    {
	register fastf_t f;
	f = los->ap->a_rt_i->rti_tol.dist;
	VJOIN1( sub_ap.a_ray.r_pt, los->swp->sw_hit.hit_point, f,
		shoot_dir);
    }
    sub_ap.a_rbeam = los->ap->a_rbeam +
	los->swp->sw_hit.hit_dist *
	los->ap->a_diverge;
    sub_ap.a_diverge = los->ap->a_diverge;

    sub_ap.a_hit = light_hit;
    sub_ap.a_miss = light_miss;
    sub_ap.a_user = -1;		/* sanity */
    sub_ap.a_uptr = (genptr_t)los->lsp;	/* so we can tell.. */
    sub_ap.a_level = 0;
    /* Will need entry & exit pts, for filter glass ==> 2 */
    /* Continue going through air ==> negative */
    sub_ap.a_onehit = -2;

    VSETALL( sub_ap.a_color, 1 );	/* vis intens so far */
    sub_ap.a_purpose = los->lsp->lt_name;	/* name of light shot at */

    RT_CK_LIGHT((struct light_specific *)(sub_ap.a_uptr));
    RT_CK_AP(&sub_ap);

    if (rdebug & RDEBUG_LIGHT)
	bu_log("shooting level %d from %d\n",
	       sub_ap.a_level, __LINE__);
    shot_status = rt_shootray( &sub_ap );
    if (rdebug & RDEBUG_LIGHT)
	bu_log("shot_status: %d\n", shot_status);

    if (shot_status < 0) {
	if (los->lsp->lt_infinite) {
	}  else if (los->lsp->lt_pt_count > 0) {
	    if (rdebug & RDEBUG_LIGHT) {
		bu_log("was pt %d\n (%g %g %g) normal %g %g %g\n", k,
		       V3ARGS(los->lsp->lt_sample_pts[k].lp_pt),
		       V3ARGS(los->lsp->lt_sample_pts[k].lp_norm) );
	    }
	} else {
	    if (rdebug & RDEBUG_LIGHT) {
		bu_log("was radius: %g of (%g) angle: %g\n",
		       radius, los->lsp->lt_radius, angle);

		bu_log("re-shooting\n");
	    }
	    goto retry;
	}
    }

    if (shot_status > 0 )  {
	/* light visible */
	if (rdebug & RDEBUG_LIGHT)
	    bu_log("light visible: %s\n", los->lsp->lt_name);

#ifdef RT_MULTISPECTRAL
	BN_CK_TABDATA(sub_ap.a_spectrum);
	if (*(los->inten) == BN_TABDATA_NULL) {
	    *(los->inten) = sub_ap.a_spectrum;
	} else {
	    BN_CK_TABDATA(*(los->inten));
	    bn_tabdata_add(*(los->inten),
			   *(los->inten),
			   sub_ap.a_spectrum);

	    bn_tabdata_free(sub_ap.a_spectrum);
	}
	sub_ap.a_spectrum = BN_TABDATA_NULL;
#else
	VMOVE( los->inten, sub_ap.a_color );
#endif
	return 1;
    }
    /* dark (light obscured) */
    if (rdebug & RDEBUG_LIGHT)
	bu_log("light obscured: %s\n", los->lsp->lt_name);

    return 0;
}

/*
 *			L I G H T _ O B S C U R A T I O N
 *
 *	Determine the visibility of each light source in the scene from a
 *	particular location.
 *	It is up to the caller to apply sw_lightfract[] to lp_color, etc.
 *
 *	Sets
 *	swp:	sw_tolight[]
 *		sw_intensity[]  or msw_intensity[]
 *		sw_visible[]
 *		sw_lightfract[]
 *
 *	References
 *	ap:	a_resource
 *		a_rti_i->rti_tol
 *		a_rbeam
 *		a_diverge
 */
void
light_obs(struct application *ap, struct shadework *swp, int have)
{
    register struct light_specific *lsp;
    register int	i;
    register fastf_t *tl_p;
    int vis_ray;
    int tot_vis_rays;
    int visibility;
    struct light_obs_stuff los;
    static int rand_idx;
    int flag_size = 0;

    /* use a constant buffer to minimize number of malloc/free calls per ray */
    char static_flags[SOME_LIGHT_SAMPLES] = {0};
    char *flags = static_flags;

    if (rdebug & RDEBUG_LIGHT ) {
	bu_log("computing Light obscuration: start\n");
    }

    RT_CK_AP(ap);
    los.rand_idx = &rand_idx;
    los.ap = ap;
    los.swp = swp;

    /* find largest sampled light */
    for( BU_LIST_FOR( lsp, light_specific, &(LightHead.l) ) )  {
	if (lsp->lt_pt_count > flag_size) {
	    flag_size = lsp->lt_pt_count;
	}
    }
    if (flag_size > SOME_LIGHT_SAMPLES) {
	flags = (char *)bu_calloc(flag_size, sizeof(char), "callocate flags array");
    }

    /*
     *  Determine light visibility
     *
     *  The sw_intensity field does NOT include the light's
     *  emission spectrum (color), only path attenuation.
     *  sw_intensity=(1,1,1) for no attenuation.
     */
    tl_p = swp->sw_tolight;

    i = 0;
    for( BU_LIST_FOR( lsp, light_specific, &(LightHead.l) ) )  {
	RT_CK_LIGHT(lsp);

	if (rdebug & RDEBUG_LIGHT)
	    bu_log("computing for light %d\n", i);
	swp->sw_lightfract[i] = 0.0;

	if (lsp->lt_infinite || lsp->lt_shadows == 0) tot_vis_rays = 1;
	else			tot_vis_rays = lsp->lt_shadows;

	los.lsp = lsp;
#ifdef RT_MULTISPECTRAL
	if(swp->msw_intensity[i]) BN_CK_TABDATA(swp->msw_intensity[i]);
	los.inten = &swp->msw_intensity[i];
#else
	los.inten = &swp->sw_intensity[3*i];
#endif

	/* create a coordinate system about the light center
	 * with the hitpoint->light ray as one of the axes
	 */
	if (lsp->lt_infinite) {
	    VMOVE(los.to_light_center, lsp->lt_vec);
	} else {
	    VSUB2(los.to_light_center, lsp->lt_pos, swp->sw_hit.hit_point);
	}
	VUNITIZE(los.to_light_center);
	bn_vec_ortho( los.light_x, los.to_light_center);
	VCROSS(los.light_y, los.to_light_center, los.light_x);

	/*
	 *  If we have a normal, test against light direction
	 */
	if ((have & MFI_NORMAL) && (swp->sw_transmit <= 0) )  {
	    if (VDOT(swp->sw_hit.hit_normal,
		     los.to_light_center)      < 0 ) {
		/* backfacing, opaque */
		if (rdebug & RDEBUG_LIGHT)
		    bu_log("norm backfacing, opaque surf:%s\n",
			   lsp->lt_name);
		continue;
	    }
	}

	visibility = 0;
	if (flag_size > 0) {
	    memset(flags, 0, flag_size * sizeof(char));
	}
	for (vis_ray = 0 ; vis_ray < tot_vis_rays ; vis_ray ++) {
	    int lv;
	    los.iter = vis_ray;

	    if (rdebug & RDEBUG_LIGHT)
		bu_log("----------vis_ray %d---------\n",
		       vis_ray);

	    switch (lv = light_vis(&los, flags)) {
	    case 1:
		/* remember the last ray that hit */
		VMOVE(tl_p, los.to_light_center);
		visibility++;
		break;
	    case -1:
		/* this is our clue to give up on
		 * this light source.
		 */
		VMOVE(tl_p, los.to_light_center);
		visibility = vis_ray = tot_vis_rays;
		break;
	    case 0:	/* light not visible */
		if (rdebug & RDEBUG_LIGHT)
		    bu_log("light not visible\n");
		break;
	    default:
		bu_log("light_vis = %d\n", lv);
	    }
	}
	if (visibility) {
	    swp->sw_visible[i] = lsp;
	    swp->sw_lightfract[i] =
		(fastf_t)visibility / (fastf_t)tot_vis_rays;
	} else {
	    swp->sw_visible[i] = (struct light_specific *)NULL;
	}

	/* Advance to next light */
	tl_p += 3;
	i++;
    }
    if (flags && flags != static_flags) {
	bu_free(flags, "free flags array");
    }

    if (rdebug & RDEBUG_LIGHT ) bu_log("computing Light obscruration: end\n");
}


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
