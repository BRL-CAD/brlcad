/*
 *			L I G H T . C
 *
 *  Implement simple isotropic light sources as a material property.
 *
 *  Author -
 *	Michael John Muuss
 *  
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1987 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static char RCSlight[] = "@(#)$Header$ (BRL)";
#endif

#include "conf.h"

#include <stdio.h>
#include <math.h>
#include "machine.h"
#include "vmath.h"
#include "rtlist.h"
#include "raytrace.h"
#include "./material.h"
#include "./mathtab.h"
#include "./light.h"
#include "./rdebug.h"

#define LIGHT_O(m)	offsetof(struct light_specific, m)
#define LIGHT_OA(m)	offsetofarray(struct light_specific, m)

RT_EXTERN(HIDDEN void	aim_set, (CONST struct bu_structparse *sdp, CONST char *name,
			CONST char *base, char *value));

struct bu_structparse light_parse[] = {
	{"%f",	1, "inten",	LIGHT_O(lt_intensity),	FUNC_NULL },
	{"%f",	1, "angle",	LIGHT_O(lt_angle),	FUNC_NULL },
	{"%f",	1, "fract",	LIGHT_O(lt_fraction),	FUNC_NULL },
	{"%f",	3, "aim",	LIGHT_OA(lt_dir),	aim_set },
	{"%d",	1, "shadows",	LIGHT_O(lt_shadows),	FUNC_NULL },
	{"%d",	1, "infinite",	LIGHT_O(lt_infinite),	FUNC_NULL },
	{"%d",	1, "invisible",	LIGHT_O(lt_invisible),	FUNC_NULL },
	{"",	0, (char *)0,	0,			FUNC_NULL }
};

struct light_specific	LightHead;	/* Heads linked list of lights */

extern double AmbientIntensity;

HIDDEN int	light_setup(), light_render();
HIDDEN void	light_print();
void		light_free();

struct mfuncs light_mfuncs[] = {
	{"light",	0,		0,		MFI_NORMAL,	0,
	light_setup,	light_render,	light_print,	light_free },

	{(char *)0,	0,		0,		0,		0,
	0,		0,		0,		0 }
};

/*
 *			A I M _ S E T
 *
 *  This routine is called by bu_struct_parse() if the "aim"
 *  qualifier is encountered, and causes lt_exaim to be set.
 */

HIDDEN void aim_set (sdp, name, base, value)
CONST struct bu_structparse *sdp;
CONST char *name;
CONST char *base;
char *value;
{
	register struct light_specific *lp =
		(struct light_specific *)base;

	lp->lt_exaim = 1;
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
light_render( ap, pp, swp, dp )
struct application	*ap;
struct partition	*pp;
struct shadework	*swp;
char	*dp;
{
	register struct light_specific *lp =
		(struct light_specific *)dp;
	register fastf_t f;

	RT_CK_LIGHT(lp);

	/* Provide cosine/2 shading, to make light look round */
	if( (f = -VDOT( swp->sw_hit.hit_normal, ap->a_ray.r_dir )*0.5) < 0 )
		f = 0;

	/* See if surface normal falls in light beam direction */
	if( VDOT( lp->lt_aim, swp->sw_hit.hit_normal) < lp->lt_cosangle )  {
		/* dark, outside of light beam area */
		f *= lp->lt_fraction;
	} else {
		/* within beam area */
		f = (f+0.5) * lp->lt_fraction;
	}
	VSCALE( swp->sw_color, lp->lt_color, f );
	return(1);
}

/*
 *			L I G H T _ S E T U P
 *
 *  Called once for each light-emitting region.
 */
HIDDEN int
light_setup( rp, matparm, dpp )
register struct region *rp;
struct rt_vls	*matparm;
genptr_t	*dpp;
{
	register struct light_specific *lp;
	register struct soltab *stp;
	vect_t	work;
	fastf_t	f;

	RT_VLS_CHECK( matparm );
	GETSTRUCT( lp, light_specific );

	RT_LIST_MAGIC_SET( &(lp->l), LIGHT_MAGIC );
	lp->lt_intensity = 1000.0;	/* Lumens */
	lp->lt_fraction = -1.0;		/* Recomputed later */
	lp->lt_invisible = 0;		/* explicitly modeled */
	lp->lt_shadows = 1;		/* by default, casts shadows */
	lp->lt_angle = 180;		/* spherical emission by default */
	lp->lt_exaim = 0;		/* use default aiming mechanism */
	lp->lt_infinite = 0;
	lp->lt_rp = rp;
	lp->lt_name = rt_strdup( rp->reg_name );
	if( bu_struct_parse( matparm, light_parse, (char *)lp ) < 0 )  {
		rt_free( (char *)lp, "light_specific" );
		return(-1);
	}

	if( lp->lt_angle > 180 )  lp->lt_angle = 180;
	lp->lt_cosangle = cos( (double) lp->lt_angle * 0.0174532925199433 );

	/* Determine position and size */
	if( rp->reg_treetop->tr_op == OP_SOLID )  {

		stp = rp->reg_treetop->tr_a.tu_stp;
		VMOVE( lp->lt_pos, stp->st_center );
		lp->lt_radius = stp->st_aradius;
	} else {
		vect_t	min_rpp, max_rpp;
		vect_t	rad;
		register union tree *tp;

		if( rt_bound_tree( rp->reg_treetop, min_rpp, max_rpp ) < 0 )
			return(-1);

		if( max_rpp[X] >= INFINITY )  {
			rt_log("light_setup(%s) Infinitely large light sources not supported\n",
				lp->lt_name );
			return(-1);
		}
		VADD2SCALE( lp->lt_pos, min_rpp, max_rpp, 0.5 );
		VSUB2( rad, max_rpp, lp->lt_pos );
		/* Use smallest radius from center to max as light radius */
		/* Having the radius too large can give very poor lighting */
		if( rad[X] < rad[Y] )
			lp->lt_radius = rad[X];
		else
			lp->lt_radius = rad[Y];
		if( rad[Z] < lp->lt_radius )
			lp->lt_radius = rad[Z];

		/* Find first leaf node on left of tree */
		tp = rp->reg_treetop;
		while( tp->tr_op != OP_SOLID )
			tp = tp->tr_b.tb_left;
		stp = tp->tr_a.tu_stp;
	}

	/* Light is aimed down -Z in it's local coordinate system */
	{
		register matp_t	matp;
		if( (matp = stp->st_matp) == (matp_t)0 )
			matp = (matp_t)rt_identity;
		if (lp->lt_exaim) {
			VSUB2 (work, lp->lt_dir, lp->lt_pos);
			VUNITIZE (work);
			}
		   else VSET( work, 0, 0, -1 );
		MAT4X3VEC( lp->lt_aim, matp, work );
		VUNITIZE( lp->lt_aim );
	}

	if( rp->reg_mater.ma_override )  {
		VMOVE( lp->lt_color, rp->reg_mater.ma_color );
	} else {
		VSETALL( lp->lt_color, 1 );
	}

	VMOVE( lp->lt_vec, lp->lt_pos );
	f = MAGNITUDE( lp->lt_vec );
	if( f < SQRT_SMALL_FASTF ) {
		/* light at the origin, make its direction vector up */
		VSET( lp->lt_vec, 0, 0, 1 );
	} else {
		VSCALE( lp->lt_vec, lp->lt_vec, f );
	}

	/* Add to linked list of lights */
	if( RT_LIST_UNINITIALIZED( &(LightHead.l ) ) )  {
		RT_LIST_INIT( &(LightHead.l) );
	}
	RT_LIST_INSERT( &(LightHead.l), &(lp->l) );

	if( lp->lt_invisible )  {
		lp->lt_rp = REGION_NULL;
		/* Note that *dpp (reg_udata) is left null */
		return(0);	/* don't show light, destroy it */
	}

	*dpp = (genptr_t)lp;	/* Associate lp with reg_udata */
	return(1);
}

/*
 *			L I G H T _ P R I N T
 */
HIDDEN void
light_print( rp, dp )
register struct region *rp;
char	*dp;
{
	bu_struct_print(rp->reg_name, light_parse, (char *)dp);
}

/*
 *			L I G H T _ F R E E
 */
void
light_free( cp )
char *cp;
{
	register struct light_specific *light =
		(struct light_specific *)cp;

	RT_CK_LIGHT(light);
	RT_LIST_DEQUEUE( &(light->l) );
	if( light->lt_name )  {
		rt_free( light->lt_name, "light name" );
		light->lt_name = (char *)0;
	}
	light->l.magic = 0;	/* sanity */
	rt_free( (char *)light, "light_specific" );
}

/*
 *			L I G H T _ M A K E R
 *
 *  Special hook called by view_2init to build 1 or 3 debugging lights.
 */
void
light_maker(num, v2m)
int	num;
mat_t	v2m;
{
	register struct light_specific *lp;
	register int i;
	vect_t	temp;
	vect_t	color;
	char	name[64];

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
			VSET( color,  1, .1, .1 );	/* Red-ish */
			VSET( temp, 1, 0, 1 );
			break;

		case 2:
			/* 2:  Behind, and overhead */
			VSET( color, .1, .1,  1 );	/* Blue-ish */
			VSET( temp, 0, 1, -0.5 );
			break;

		default:
			return;
		}
		GETSTRUCT( lp, light_specific );
		lp->l.magic = LIGHT_MAGIC;
		VMOVE( lp->lt_color, color );
		MAT4X3VEC( lp->lt_pos, v2m, temp );
		VMOVE( lp->lt_vec, lp->lt_pos );
		VUNITIZE( lp->lt_vec );

		sprintf(name, "Implicit light %d", i);
		lp->lt_name = rt_strdup(name);

		VSET( lp->lt_aim, 0, 0, -1 );	/* any direction: spherical */
		lp->lt_intensity = 1000.0;
		lp->lt_radius = 0.1;		/* mm, "point" source */
		lp->lt_invisible = 1;		/* NOT explicitly modeled */
		lp->lt_shadows = 0;		/* no shadows for speed */
		lp->lt_angle = 180;		/* spherical emission */
		lp->lt_cosangle = -1;		/* cos(180) */
		lp->lt_infinite = 0;
		lp->lt_rp = REGION_NULL;
		if( RT_LIST_UNINITIALIZED( &(LightHead.l ) ) )  {
			RT_LIST_INIT( &(LightHead.l) );
		}
		RT_LIST_INSERT( &(LightHead.l), &(lp->l) );
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
light_init()
{
	register struct light_specific *lp;
	register int		nlights = 0;
	register fastf_t	inten = 0.0;

	if( RT_LIST_UNINITIALIZED( &(LightHead.l ) ) )  {
		RT_LIST_INIT( &(LightHead.l) );
	}
	for( RT_LIST_FOR( lp, light_specific, &(LightHead.l) ) )  {
		nlights++;
		if( lp->lt_fraction > 0 )  continue;	/* overridden */
		if( lp->lt_intensity <= 0 )
			lp->lt_intensity = 1;		/* keep non-neg */
		if( lp->lt_intensity > inten )
			inten = lp->lt_intensity;
	}

	/* Compute total emitted energy, including ambient */
/**	inten *= (1 + AmbientIntensity); **/
	/* This is non-physical and risky, but gives nicer pictures for now */
	inten *= (1 + AmbientIntensity*0.5);

	for( RT_LIST_FOR( lp, light_specific, &(LightHead.l) ) )  {
		RT_CK_LIGHT(lp);
		if( lp->lt_fraction > 0 )  continue;	/* overridden */
		lp->lt_fraction = lp->lt_intensity / inten;
	}
	rt_log("Lighting: Ambient = %d%%\n", (int)(AmbientIntensity*100));
	for( RT_LIST_FOR( lp, light_specific, &(LightHead.l) ) )  {
		RT_CK_LIGHT(lp);
		rt_log( "  %s: (%g, %g, %g), aimed at (%g, %g, %g)\n",
			lp->lt_name,
			lp->lt_pos[X], lp->lt_pos[Y], lp->lt_pos[Z],
			lp->lt_aim[X], lp->lt_aim[Y], lp->lt_aim[Z] );
		rt_log( "  %s: %s, %s, %g lumens (%d%%), halfang=%g\n",
			lp->lt_name,
			lp->lt_invisible ? "invisible":"visible",
			lp->lt_shadows ? "casts shadows":"no shadows",
			lp->lt_intensity,
			(int)(lp->lt_fraction*100),
			lp->lt_angle );
	}
	if( nlights > SW_NLIGHTS )  {
		rt_log("Number of lights limited to %d\n", SW_NLIGHTS);
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
light_cleanup()
{
	register struct light_specific *lp, *zaplp;

	if( RT_LIST_UNINITIALIZED( &(LightHead.l ) ) )  {
		RT_LIST_INIT( &(LightHead.l) );
		return;
	}
	for( RT_LIST_FOR( lp, light_specific, &(LightHead.l) ) )  {
		RT_CK_LIGHT(lp);
		if( lp->lt_rp != REGION_NULL && lp->lt_invisible == 0 )  {
			/* Will be cleaned up by mlib_free() */
			continue;
		}
		zaplp = lp;
		lp = RT_LIST_PREV( light_specific, &(lp->l) );
		light_free( (genptr_t)zaplp );
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
 *  These shadow functions return a boolean "light_visible".
 * 
 *  This is a simplified algorithm, and could be improved.
 *  Reflected light can't be dealt with at all.
 *
 *  Would also be nice to return an actual energy level, rather than
 *  a boolean, which could account for distance, etc.
 */

int
light_hit(ap, PartHeadp, finished_segs )
struct application *ap;
struct partition *PartHeadp;
struct seg *finished_segs;
{
	register struct partition *pp;
	register struct region	*regp = NULL;
	struct application	sub_ap;
	struct shadework	sw;
	CONST struct light_specific	*lp;
	extern int	light_render();
	vect_t	filter_color;
	int	light_visible;
	int	air_sols_seen = 0;
	char	*reason = "???";

	RT_CK_LIST_HEAD(&finished_segs->l);

	lp = (struct light_specific *)(ap->a_uptr);
	RT_CK_LIGHT(lp);

	VSETALL( filter_color, 1 );

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
		if( pp->pt_regionp->reg_aircode != 0 )  {
			/* Accumulate transmission through each air lump */
			air_sols_seen++;

			/* Obtain opacity of this region, multiply */
			sw.sw_inputs = 0;
			sw.sw_transmit = sw.sw_reflect = 0.0;
			sw.sw_refrac_index = 1.0;
			sw.sw_xmitonly = 1;	/* only want sw_transmit */
			sw.sw_segs = finished_segs;
			VSETALL( sw.sw_color, 1 );
			VSETALL( sw.sw_basecolor, 1 );

			(void)viewshade( ap, pp, &sw );
			/* sw_transmit is only return */

			/* Clouds don't yet attenuate differently based on freq */
			VSCALE( filter_color, filter_color, sw.sw_transmit );
			continue;
		}
		if( pp->pt_inhit->hit_dist >= ap->a_rt_i->rti_tol.dist )
			break;
		if( pp->pt_outhit->hit_dist >= ap->a_rt_i->rti_tol.dist*10 )
			break;
	}
	if( pp == PartHeadp )  {
		pp=PartHeadp->pt_forw;
		RT_CK_PT(pp);

		if( lp->lt_invisible || lp->lt_infinite )  {
			light_visible = 1;
			VMOVE( ap->a_color, filter_color );
			reason = "Unobstructed invisible/infinite light";
			goto out;
		}

		if( air_sols_seen > 0 )  {
			light_visible = 1;
			VMOVE( ap->a_color, filter_color );
/* XXXXXXX This seems to happen with *every* light vis ray through air */
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
			/* pt_outhit->hit_point has not been calculated */
			VJOIN1(sub_ap.a_ray.r_pt, ap->a_ray.r_pt,
				pp->pt_outhit->hit_dist, ap->a_ray.r_dir);

			retval = rt_shootray( &sub_ap );

			ap->a_user = sub_ap.a_user;
			ap->a_uptr = sub_ap.a_uptr;
			ap->a_color[0] = sub_ap.a_color[0];
			ap->a_color[1] = sub_ap.a_color[1];
			ap->a_color[2] = sub_ap.a_color[2];
			VMOVE(ap->a_uvec, sub_ap.a_uvec);
			VMOVE(ap->a_vvec, sub_ap.a_vvec);
			ap->a_refrac_index = sub_ap.a_refrac_index;
			ap->a_cumlen = sub_ap.a_cumlen;
			ap->a_return = sub_ap.a_return;

			light_visible = retval;
			reason = "pressed on past start point";
			goto out;
		}


		rt_log("light_hit:  ERROR, nothing hit, sxy=%d,%d, dtol=%e\n",
			ap->a_x, ap->a_y,
			ap->a_rt_i->rti_tol.dist);
		rt_pr_partitions(ap->a_rt_i, PartHeadp, "light_hit pt list");
		light_visible = 0;
		reason = "error, nothing hit";
		goto out;
	}
	regp = pp->pt_regionp;

	/* Check to see if we hit the light source */
	if( lp->lt_rp == regp )  {
		VMOVE( ap->a_color, filter_color );
		light_visible = 1;
		reason = "hit light";
		goto out;
	}

	/* or something futher away than a finite invisible light */
	if( lp->lt_invisible && !(lp->lt_infinite) ) {
		vect_t	tolight;
		VSUB2( tolight, lp->lt_pos, ap->a_ray.r_pt );
		if( pp->pt_inhit->hit_dist >= MAGNITUDE(tolight) ) {
			VMOVE( ap->a_color, filter_color );
			light_visible = 1;
			reason = "hit behind invisible light ==> hit light";
			goto out;
		}
	}

	/* If we hit an entirely opaque object, this light is invisible */
	if( pp->pt_outhit->hit_dist >= INFINITY ||
	    (regp->reg_transmit == 0 /* XXX && Not procedural shader */) )  {
		VSETALL( ap->a_color, 0 );
		light_visible = 0;
	    	reason = "hit opaque object";
		goto out;
	}

	/*  See if any further contributions will mater */
	if( ap->a_color[0] + ap->a_color[1] + ap->a_color[2] < 0.01 )  {
	    	/* Any light energy is "fully" attenuated by here */
		VSETALL( ap->a_color, 0 );
		light_visible = 0;
		reason = "light fully attenuated before shading";
		goto out;
	}

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
	VSETALL( sw.sw_color, 1 );
	VSETALL( sw.sw_basecolor, 1 );

	(void)viewshade( ap, pp, &sw );
	/* sw_transmit is output */

	VSCALE( filter_color, filter_color, sw.sw_transmit );
	if( filter_color[0] + filter_color[1] + filter_color[2] < 0.01 )  {
	    	/* Any recursion won't be significant */
		VSETALL( ap->a_color, 0 );
		light_visible = 0;
		reason = "light fully attenuated after shading";
		goto out;
	}

	/*
	 * Push on to exit point, and trace on from there.
	 * Transmission so far is passed along in sub_ap.a_color[];
	 * Don't even think of trying to refract, or we will miss the light!
	 */
	sub_ap = *ap;			/* struct copy */
	sub_ap.a_level = ap->a_level+1;
	{
		register fastf_t f;
		f = pp->pt_outhit->hit_dist + ap->a_rt_i->rti_tol.dist;
		VJOIN1(sub_ap.a_ray.r_pt, ap->a_ray.r_pt, f, ap->a_ray.r_dir);
	}
	sub_ap.a_purpose = "light transmission after filtering";
	light_visible = rt_shootray( &sub_ap );

	VELMUL( ap->a_color, sub_ap.a_color, filter_color );
	reason = "after filtering";
out:
	if( rdebug & RDEBUG_LIGHT ) bu_log("light vis=%d %s (%4.2f, %4.2f, %4.2f) %s %s\n",
		light_visible,
		lp->lt_name,
		V3ARGS(ap->a_color), reason,
		regp ? regp->reg_name : "" );
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
light_miss(ap, PartHeadp)
register struct application *ap;
struct partition *PartHeadp;
{
	struct light_specific *lp = (struct light_specific *)(ap->a_uptr);

	RT_CK_LIGHT(lp);
	if( lp->lt_invisible || lp->lt_infinite ) {
		VSETALL( ap->a_color, 1 );
		if( rdebug & RDEBUG_LIGHT ) rt_log("light_miss vis=1\n");
		return(1);		/* light_visible = 1 */
	}
	/* Missed light, either via blockage or dither.  Return black */
	VSETALL( ap->a_color, 0 );
	if( rdebug & RDEBUG_LIGHT ) rt_log("light_miss vis=0\n");
	return(0);			/* light_visible = 0 */
}

/*
 *			L I G H T _ V I S I B I L I T Y
 *
 *	Determine the visibility of each light source in the scene from a
 *	particular location.
 */
void
light_visibility(ap, swp, have)
struct application *ap;
struct shadework *swp;
int have;
{
		register struct light_specific *lp;
		register int	i;
		register fastf_t *intensity, *tolight;
		register fastf_t f;
		struct application sub_ap;

		/*
		 *  Determine light visibility
		 *
		 *  The sw_intensity field does NOT include the light's
		 *  emission spectrum (color), only path attenuation.
		 *  sw_intensity=(1,1,1) for no attenuation.
		 */
		i = 0;
		intensity = swp->sw_intensity;
		tolight = swp->sw_tolight;
		for( RT_LIST_FOR( lp, light_specific, &(LightHead.l) ) )  {
			RT_CK_LIGHT(lp);
			/* compute the light direction */
			if( lp->lt_infinite ) {
				/* Infinite lights are point sources, no fuzzy penumbra */
				VMOVE( tolight, lp->lt_vec );
			} else {
				VSUB2(tolight, lp->lt_pos,
					swp->sw_hit.hit_point);
#if 0
		/*
		 *  Dither light pos for penumbra by +/- 0.5 light radius;
		 *  this presently makes a cubical light source distribution.
		 */
		f = lp->lt_radius * 0.9;
		tolight[X] = lp->lt_pos[X] +
			rand_half(ap->a_resource->re_randptr)*f -
			swp->sw_hit.hit_point[X];
		tolight[Y] = lp->lt_pos[Y] +
			rand_half(ap->a_resource->re_randptr)*f -
			swp->sw_hit.hit_point[Y];
		tolight[Z] = lp->lt_pos[Z] +
			rand_half(ap->a_resource->re_randptr)*f -
			swp->sw_hit.hit_point[Z];
#endif

			}

			/*
			 *  If we have a normal, test against light direction
			 */
			if( (have & MFI_NORMAL) && (swp->sw_transmit <= 0) )  {
				if( VDOT(swp->sw_hit.hit_normal,tolight) < 0 ) {
					/* backfacing, opaque */
					swp->sw_visible[i] = (char *)0;
					goto next;
				}
			}
			VUNITIZE( tolight );

			/*
			 * See if ray from hit point to light lies within light beam
			 * Note: this is should always be true for infinite lights!
			 */
			if( -VDOT(tolight, lp->lt_aim) < lp->lt_cosangle )  {
				/* dark (outside of light beam) */
				swp->sw_visible[i] = (char *)0;
				goto next;
			}
			if( !(lp->lt_shadows) )  {
				/* "fill light" in beam, don't care about shadows */
				swp->sw_visible[i] = (char *)lp;
				VSETALL( intensity, 1 );
				goto next;
			}

			/*
			 *  Fire ray at light source to check for shadowing.
			 *  (This SHOULD actually return an energy spectrum).
			 *  Advance start point slightly off surface.
			 */
			sub_ap = *ap;			/* struct copy */
			VMOVE( sub_ap.a_ray.r_dir, tolight );
			{
				register fastf_t f;
				f = ap->a_rt_i->rti_tol.dist;
				VJOIN1( sub_ap.a_ray.r_pt,
					swp->sw_hit.hit_point,
					f, tolight );
			}
			sub_ap.a_hit = light_hit;
			sub_ap.a_miss = light_miss;
			sub_ap.a_user = -1;		/* sanity */
			sub_ap.a_uptr = (genptr_t)lp;	/* so we can tell.. */
			sub_ap.a_level = 0;
			/* Will need entry & exit pts, for filter glass ==> 2 */
			/* Continue going through air ==> negative */
			sub_ap.a_onehit = -2;

			VSETALL( sub_ap.a_color, 1 );	/* vis intens so far */
			sub_ap.a_purpose = lp->lt_name;	/* name of light shot at */

			RT_CK_LIGHT((struct light_specific *)(sub_ap.a_uptr));			


			if( rt_shootray( &sub_ap ) )  {
				/* light visible */
				swp->sw_visible[i] = (char *)lp;
				VMOVE( intensity, sub_ap.a_color );
			} else {
				/* dark (light obscured) */
				swp->sw_visible[i] = (char *)0;
			}
next:
			/* Advance to next light */
			i++;
			intensity += 3;
			tolight += 3;
		}
}
