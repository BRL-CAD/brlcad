/*
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
 *  
 *  Distribution Notice -
 *	Re-distribution of this software is restricted, as described in
 *	your "Statement of Terms and Conditions for the Release of
 *	The BRL-CAD Package" license agreement.
 *
 *  Copyright Notice -
 *	This software is Copyright (C) 1998 by the United States Army
 *	in all countries except the USA.  All rights reserved.
 */
#ifndef lint
static char RCSsh_light[] = "@(#)$Header$ (ARL)";
#endif

#include "conf.h"

#include <stdio.h>
#include <math.h>
#include "machine.h"
#include "vmath.h"
#include "bu.h"
#include "raytrace.h"
#include "shadefuncs.h"
#include "shadework.h"
#include "../rt/rdebug.h"
#include "../rt/light.h"

#if !defined(M_PI)
#define M_PI            3.14159265358979323846
#define	M_PI_2		1.57079632679489661923
#endif


#define LIGHT_O(m)	offsetof(struct light_specific, m)
#define LIGHT_OA(m)	bu_offsetofarray(struct light_specific, m)

RT_EXTERN(HIDDEN void	aim_set, (CONST struct bu_structparse *sdp, CONST char *name,
CONST char *base, char *value));

/***********************************************************************
 *
 *  light_cvt_visible()
 *
 *  Convert "visible" flag to "invisible" variable 
 */
void
light_cvt_visible( sdp, name, base, value )
register CONST struct bu_structparse	*sdp;	/* structure description */
register CONST char			*name;	/* struct member name */
char					*base;	/* begining of structure */
CONST char				*value;	/* string containing value */
{
	int *p = (int *)(base+sdp->sp_offset);
	struct light_specific *lp = (struct light_specific *)base;

	switch (sdp->sp_offset) {
	case LIGHT_O(lt_invisible):
		lp->lt_visible = !lp->lt_invisible;
		break;
	case LIGHT_O(lt_visible):
		lp->lt_invisible = !lp->lt_visible;
		break;
	}
	/* reconvert with optional units */
	*p = !*p;
}


struct bu_structparse light_print_tab[] = {
{"%f",	1, "bright",	LIGHT_O(lt_intensity),	BU_STRUCTPARSE_FUNC_NULL },
{"%f",	1, "angle",	LIGHT_O(lt_angle),	BU_STRUCTPARSE_FUNC_NULL },
{"%f",	1, "fract",	LIGHT_O(lt_fraction),	BU_STRUCTPARSE_FUNC_NULL },
{"%f",	3, "dir",	LIGHT_OA(lt_dir),	aim_set },
{"%d",	1, "shadows",	LIGHT_O(lt_shadows),	BU_STRUCTPARSE_FUNC_NULL },
{"%d",	1, "infinite",	LIGHT_O(lt_infinite),	BU_STRUCTPARSE_FUNC_NULL },
{"%d",	1, "visible",	LIGHT_O(lt_visible),	BU_STRUCTPARSE_FUNC_NULL },
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

{"%f",	3, "dir",	LIGHT_OA(lt_dir),	aim_set },
{"%f",	3, "d",		LIGHT_OA(lt_dir),	aim_set },
{"%f",	3, "aim",	LIGHT_OA(lt_dir),	aim_set },

{"%d",	1, "shadows",	LIGHT_O(lt_shadows),	BU_STRUCTPARSE_FUNC_NULL },
{"%d",	1, "s",		LIGHT_O(lt_shadows),	BU_STRUCTPARSE_FUNC_NULL },

{"%d",	1, "infinite",	LIGHT_O(lt_infinite),	BU_STRUCTPARSE_FUNC_NULL },
{"%d",	1, "i",		LIGHT_O(lt_infinite),	BU_STRUCTPARSE_FUNC_NULL },

{"%d",	1, "visible",	LIGHT_O(lt_visible),	light_cvt_visible },
{"%d",  1, "invisible",	LIGHT_O(lt_invisible),	light_cvt_visible },
{"%d",	1, "v",		LIGHT_O(lt_visible),	light_cvt_visible },

{"",	0, (char *)0,	0,			BU_STRUCTPARSE_FUNC_NULL }
};


struct light_specific	LightHead;	/* Heads linked list of lights */

extern double AmbientIntensity;

#if RT_MULTISPECTRAL
extern CONST struct bn_table	*spectrum;	/* from rttherm/viewtherm.c */
#endif

HIDDEN int	light_setup(), light_render();
HIDDEN void	light_print();
void		light_free();

struct mfuncs light_mfuncs[] = {
	{MF_MAGIC,	"light",	0,		MFI_NORMAL,	0,
	light_setup,	light_render,	light_print,	light_free },
	{0,		(char *)0,	0,		0,		0,
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
	if ((f = -VDOT( swp->sw_hit.hit_normal, ap->a_ray.r_dir )*0.5) < 0 )
		f = 0;

	/* See if surface normal falls in light beam direction */
	if (VDOT( lp->lt_aim, swp->sw_hit.hit_normal) < lp->lt_cosangle )  {
		/* dark, outside of light beam area */
		f *= lp->lt_fraction;
	} else {
		/* within beam area */
		f = (f+0.5) * lp->lt_fraction;
	}
#if RT_MULTISPECTRAL
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
		bn_tabdata_scale( swp->msw_color, lp->lt_spectrum, f );
	}
#else
	VSCALE( swp->sw_color, lp->lt_color, f );
#endif
	return(1);
}

/*
 *			L I G H T _ S E T U P
 *
 *  Called once for each light-emitting region.
 */
HIDDEN int
light_setup( rp, matparm, dpp, mfp, rtip )
register struct region *rp;
struct bu_vls	*matparm;
genptr_t	*dpp;
struct mfuncs           *mfp;
struct rt_i             *rtip;  /* New since 4.4 release */
{
	register struct light_specific *lp;
	register struct soltab *stp;
	vect_t	work;
	fastf_t	f;

	BU_CK_VLS( matparm );
	BU_GETSTRUCT( lp, light_specific );

	BU_LIST_MAGIC_SET( &(lp->l), LIGHT_MAGIC );
	lp->lt_intensity = 1000.0;	/* Lumens */
	lp->lt_fraction = -1.0;		/* Recomputed later */
	lp->lt_visible = 1;		/* explicitly modeled */
	lp->lt_invisible = 0;		/* explicitly modeled */
	lp->lt_shadows = 1;		/* by default, casts shadows */
	lp->lt_angle = 180;		/* spherical emission by default */
	lp->lt_exaim = 0;		/* use default aiming mechanism */
	lp->lt_infinite = 0;
	lp->lt_rp = rp;
	lp->lt_name = bu_strdup( rp->reg_name );
	if (bu_struct_parse( matparm, light_parse, (char *)lp ) < 0 )  {
		bu_free( (char *)lp, "light_specific" );
		return(-1);
	}

	if (lp->lt_angle > 180 )  lp->lt_angle = 180;
	lp->lt_cosangle = cos( (double) lp->lt_angle * 0.0174532925199433 );

	/* Determine position and size */
	if (rp->reg_treetop->tr_op == OP_SOLID )  {

		stp = rp->reg_treetop->tr_a.tu_stp;
		VMOVE( lp->lt_pos, stp->st_center );
		lp->lt_radius = stp->st_aradius;
	} else {
		vect_t	min_rpp, max_rpp;
		vect_t	rad;
		register union tree *tp;

		if (rt_bound_tree( rp->reg_treetop, min_rpp, max_rpp ) < 0 )
			return(-1);

		if (max_rpp[X] >= INFINITY )  {
			bu_log("light_setup(%s) Infinitely large light sources not supported\n",
			    lp->lt_name );
			return(-1);
		}
		VADD2SCALE( lp->lt_pos, min_rpp, max_rpp, 0.5 );
		VSUB2( rad, max_rpp, lp->lt_pos );
		/* Use smallest radius from center to max as light radius */
		/* Having the radius too large can give very poor lighting */
		if (rad[X] < rad[Y] )
			lp->lt_radius = rad[X];
		else
			lp->lt_radius = rad[Y];
		if (rad[Z] < lp->lt_radius )
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
		if ((matp = stp->st_matp) == (matp_t)0 )
			matp = (matp_t)bn_mat_identity;
		if (lp->lt_exaim) {
			VSUB2 (work, lp->lt_dir, lp->lt_pos);
			VUNITIZE (work);
		}
		else VSET( work, 0, 0, -1 );
		MAT4X3VEC( lp->lt_aim, matp, work );
		VUNITIZE( lp->lt_aim );
	}

#if RT_MULTISPECTRAL
	BN_GET_TABDATA(lp->lt_spectrum, spectrum);
	if (rp->reg_mater.ma_temperature > 0 )  {
		rt_spect_black_body( lp->lt_spectrum,
			rp->reg_mater.ma_temperature, 5 );
		if (rdebug & RDEBUG_LIGHT )  {
			bu_log("Light %s temp is %g degK, emission is pure black-body\n",
				rp->reg_name, rp->reg_mater.ma_temperature);
		}
	} else if (rp->reg_mater.ma_color_valid )  {
		rt_spect_reflectance_rgb( lp->lt_spectrum, rp->reg_mater.ma_color );
		/* XXX Need to convert units of lumens (candela-sr) to ?? mw/sr?  Use any old numbers to get started. */
		bn_tabdata_scale( lp->lt_spectrum, lp->lt_spectrum,
			lp->lt_intensity * 0.001 ); /* XXX */
	} else {
		/* Default: Perfectly even emission across whole spectrum */
		bn_tabdata_constval( lp->lt_spectrum, 0.001 );
	}
#else
	if (rp->reg_mater.ma_color_valid )  {
		VMOVE( lp->lt_color, rp->reg_mater.ma_color );
	} else {
		VSETALL( lp->lt_color, 1 );
	}
#endif

	VMOVE( lp->lt_vec, lp->lt_pos );
	f = MAGNITUDE( lp->lt_vec );
	if (f < SQRT_SMALL_FASTF ) {
		/* light at the origin, make its direction vector up */
		VSET( lp->lt_vec, 0, 0, 1 );
	} else {
		VSCALE( lp->lt_vec, lp->lt_vec, f );
	}

	/* Add to linked list of lights */
	if (BU_LIST_UNINITIALIZED( &(LightHead.l ) ) )  {
		BU_LIST_INIT( &(LightHead.l) );
	}
	BU_LIST_INSERT( &(LightHead.l), &(lp->l) );

	if (lp->lt_invisible )  {
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
	bu_struct_print(rp->reg_name, light_print_tab, (char *)dp);
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
	BU_LIST_DEQUEUE( &(light->l) );
	if (light->lt_name )  {
		bu_free( light->lt_name, "light name" );
		light->lt_name = (char *)0;
	}
	light->l.magic = 0;	/* sanity */
	bu_free( (char *)light, "light_specific" );
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
		BU_GETSTRUCT( lp, light_specific );
		lp->l.magic = LIGHT_MAGIC;
#if RT_MULTISPECTRAL
		BN_GET_TABDATA(lp->lt_spectrum, spectrum);
		rt_spect_reflectance_rgb( lp->lt_spectrum, color );
		bn_tabdata_scale( lp->lt_spectrum, lp->lt_spectrum, 1000.0 );
#else
		VMOVE( lp->lt_color, color );
#endif
		MAT4X3VEC( lp->lt_pos, v2m, temp );
		VMOVE( lp->lt_vec, lp->lt_pos );
		VUNITIZE( lp->lt_vec );

		sprintf(name, "Implicit light %d", i);
		lp->lt_name = bu_strdup(name);

		VSET( lp->lt_aim, 0, 0, -1 );	/* any direction: spherical */
		lp->lt_intensity = 1000.0;
		lp->lt_radius = 0.1;		/* mm, "point" source */
		lp->lt_visible = 0;		/* NOT explicitly modeled */
		lp->lt_invisible = 1;		/* NOT explicitly modeled */
		lp->lt_shadows = 0;		/* no shadows for speed */
		lp->lt_angle = 180;		/* spherical emission */
		lp->lt_cosangle = -1;		/* cos(180) */
		lp->lt_infinite = 0;
		lp->lt_rp = REGION_NULL;
		if (BU_LIST_UNINITIALIZED( &(LightHead.l ) ) )  {
			BU_LIST_INIT( &(LightHead.l) );
		}
		BU_LIST_INSERT( &(LightHead.l), &(lp->l) );
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

	if (BU_LIST_UNINITIALIZED( &(LightHead.l ) ) )  {
		BU_LIST_INIT( &(LightHead.l) );
	}
	for( BU_LIST_FOR( lp, light_specific, &(LightHead.l) ) )  {
		nlights++;
		if (lp->lt_fraction > 0 )  continue;	/* overridden */
		if (lp->lt_intensity <= 0 )
			lp->lt_intensity = 1;		/* keep non-neg */
		if (lp->lt_intensity > inten )
			inten = lp->lt_intensity;
	}

	/* Compute total emitted energy, including ambient */
	/**	inten *= (1 + AmbientIntensity); **/
	/* This is non-physical and risky, but gives nicer pictures for now */
	inten *= (1 + AmbientIntensity*0.5);

	for( BU_LIST_FOR( lp, light_specific, &(LightHead.l) ) )  {
		RT_CK_LIGHT(lp);
		if (lp->lt_fraction > 0 )  continue;	/* overridden */
#if RT_MULTISPECTRAL
		lp->lt_fraction = 1.0;	/* always use honest intensity values */
#else
		lp->lt_fraction = lp->lt_intensity / inten;
#endif
	}
	if (rt_verbosity & VERBOSE_LIGHTINFO) {
		bu_log("Lighting: Ambient = %d%%\n", (int)(AmbientIntensity*100));

		for( BU_LIST_FOR( lp, light_specific, &(LightHead.l) ) )  {
			RT_CK_LIGHT(lp);
			bu_log( "  %s: (%g, %g, %g), aimed at (%g, %g, %g)\n",
			    lp->lt_name,
			    lp->lt_pos[X], lp->lt_pos[Y], lp->lt_pos[Z],
			    lp->lt_aim[X], lp->lt_aim[Y], lp->lt_aim[Z] );
			bu_log( "  %s: %s, %s, %g lumens (%d%%), halfang=%g\n",
			    lp->lt_name,
			    lp->lt_visible ? "visible":"invisible",
			    lp->lt_shadows ? "casts shadows":"no shadows",
			    lp->lt_intensity,
			    (int)(lp->lt_fraction*100),
			    lp->lt_angle );
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
light_cleanup()
{
	register struct light_specific *lp, *zaplp;

	if (BU_LIST_UNINITIALIZED( &(LightHead.l ) ) )  {
		BU_LIST_INIT( &(LightHead.l) );
		return;
	}
	for( BU_LIST_FOR( lp, light_specific, &(LightHead.l) ) )  {
		RT_CK_LIGHT(lp);
		if (lp->lt_rp != REGION_NULL && lp->lt_visible )  {
			/* Will be cleaned up by mlib_free() */
			continue;
		}
		zaplp = lp;
		lp = BU_LIST_PREV( light_specific, &(lp->l) );
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
#if RT_MULTISPECTRAL
	struct bn_tabdata	*ms_filter_color = BN_TABDATA_NULL;
#else
	vect_t	filter_color;
#endif
	int	light_visible;
	int	air_sols_seen = 0;
	char	*reason = "???";

#if RT_MULTISPECTRAL
	sub_ap.a_spectrum = BN_TABDATA_NULL;	/* sanity */
	BN_CK_TABDATA(ap->a_spectrum);
#endif

	BU_CK_LIST_HEAD(&finished_segs->l);

	lp = (struct light_specific *)(ap->a_uptr);
	RT_CK_LIGHT(lp);

#if RT_MULTISPECTRAL
	ms_filter_color = bn_tabdata_get_constval( 1.0, spectrum );
	BN_GET_TABDATA( sw.msw_color, spectrum );
	BN_GET_TABDATA( sw.msw_basecolor, spectrum );
#else
	VSETALL( filter_color, 1 );
#endif

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
#if RT_MULTISPECTRAL
			bn_tabdata_constval( sw.msw_color, 1.0 );
			bn_tabdata_constval( sw.msw_basecolor, 1.0 );
#else
			VSETALL( sw.sw_color, 1 );
			VSETALL( sw.sw_basecolor, 1 );
#endif

			(void)viewshade( ap, pp, &sw );
			/* sw_transmit is only return */

			/* XXX Clouds don't yet attenuate differently based on freq */
#if RT_MULTISPECTRAL
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
		pp=PartHeadp->pt_forw;
		RT_CK_PT(pp);

		if (lp->lt_invisible || lp->lt_infinite )  {
			light_visible = 1;
#if RT_MULTISPECTRAL
			bn_tabdata_copy( ap->a_spectrum, ms_filter_color );
#else
			VMOVE( ap->a_color, filter_color );
#endif
			reason = "Unobstructed invisible/infinite light";
			goto out;
		}

		if (air_sols_seen > 0 )  {
			light_visible = 1;
#if RT_MULTISPECTRAL
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
#if RT_MULTISPECTRAL
			sub_ap.a_spectrum = bn_tabdata_dup( ap->a_spectrum );
#endif
			/* pt_outhit->hit_point has not been calculated */
			VJOIN1(sub_ap.a_ray.r_pt, ap->a_ray.r_pt,
			    pp->pt_outhit->hit_dist, ap->a_ray.r_dir);

			retval = rt_shootray( &sub_ap );

			ap->a_user = sub_ap.a_user;
			ap->a_uptr = sub_ap.a_uptr;
#if RT_MULTISPECTRAL
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
	if (lp->lt_rp == regp )  {
#if RT_MULTISPECTRAL
		bn_tabdata_copy( ap->a_spectrum, ms_filter_color );
#else
		VMOVE( ap->a_color, filter_color );
#endif
		light_visible = 1;
		reason = "hit light";
		goto out;
	}

	/* or something futher away than a finite invisible light */
	if (lp->lt_invisible && !(lp->lt_infinite) ) {
		vect_t	tolight;
		VSUB2( tolight, lp->lt_pos, ap->a_ray.r_pt );
		if (pp->pt_inhit->hit_dist >= MAGNITUDE(tolight) ) {
#if RT_MULTISPECTRAL
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
	if (pp->pt_outhit->hit_dist >= INFINITY ||
	    (regp->reg_transmit == 0 /* XXX && Not procedural shader */) )  {
#if RT_MULTISPECTRAL
		bn_tabdata_constval( ap->a_spectrum, 0.0 );
#else
		VSETALL( ap->a_color, 0 );
#endif
		light_visible = 0;
		reason = "hit opaque object";
		goto out;
	}

#if RT_MULTISPECTRAL
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
#if RT_MULTISPECTRAL
	bn_tabdata_constval( sw.msw_color, 1.0 );
	bn_tabdata_constval( sw.msw_basecolor, 1.0 );
#else
	VSETALL( sw.sw_color, 1 );
	VSETALL( sw.sw_basecolor, 1 );
#endif

	(void)viewshade( ap, pp, &sw );
	/* sw_transmit is output */

#if RT_MULTISPECTRAL
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
#if RT_MULTISPECTRAL
	sub_ap.a_spectrum = bn_tabdata_dup( ap->a_spectrum );
#endif
	{
		register fastf_t f;
		f = pp->pt_outhit->hit_dist + ap->a_rt_i->rti_tol.dist;
		VJOIN1(sub_ap.a_ray.r_pt, ap->a_ray.r_pt, f, ap->a_ray.r_dir);
	}
	sub_ap.a_purpose = "light transmission after filtering";
	light_visible = rt_shootray( &sub_ap );

#if RT_MULTISPECTRAL
	bn_tabdata_mul( ap->a_spectrum, sub_ap.a_spectrum, ms_filter_color );
#else
	VELMUL( ap->a_color, sub_ap.a_color, filter_color );
#endif
	reason = "after filtering";
out:
#if RT_MULTISPECTRAL
	if (ms_filter_color ) bn_tabdata_free( ms_filter_color );
	if (sw.msw_color )  bn_tabdata_free( sw.msw_color );
	if (sw.msw_basecolor ) bn_tabdata_free( sw.msw_basecolor );
	if (sub_ap.a_spectrum )  bn_tabdata_free( sub_ap.a_spectrum );
	if (rdebug & RDEBUG_LIGHT )  {
		bu_log("light vis=%d %s %s %s  ",
		    light_visible,
		    lp->lt_name,
		    reason,
		    regp ? regp->reg_name : "" );
		bn_pr_tabdata("light spectrum", ap->a_spectrum);
	}
#else
	if (rdebug & RDEBUG_LIGHT ) bu_log("light vis=%d %s (%4.2f, %4.2f, %4.2f) %s %s\n",
	    light_visible,
	    lp->lt_name,
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
light_miss(ap)
register struct application *ap;
{
	struct light_specific *lp = (struct light_specific *)(ap->a_uptr);

	RT_CK_LIGHT(lp);
	if (lp->lt_invisible || lp->lt_infinite ) {
		VSETALL( ap->a_color, 1 );
		if (rdebug & RDEBUG_LIGHT ) bu_log("light_miss vis=1\n");
		return(1);		/* light_visible = 1 */
	}

	bu_log("light ray missed non-infinite, visible light source\n");
	bu_log("on pixel: %d %d\n", ap->a_x, ap->a_y);

	/* Missed light, either via blockage or dither.  Return black */
	VSETALL( ap->a_color, 0 );
	if (rdebug & RDEBUG_LIGHT ) bu_log("light_miss vis=0\n");
	return(0);			/* light_visible = 0 */
}

struct light_obs_stuff {
	struct application *ap;
	struct shadework *swp;
	struct light_specific *lp;
	int *rand_idx;

#if RT_MULTISPECTRAL
	struct bn_tabdata **inten;
#else
	fastf_t *inten;
#endif
	int iter;
	vect_t to_light_center;	/* coordinate system on light */
	vect_t light_x;
	vect_t light_y;
};

/***********************************************************************
 *
 *	light_vis
 *
 *	Compute 1 light visibility ray from a hit point to the light.
 *
 */
static int 
light_vis(los)
struct light_obs_stuff *los;
{
	struct application sub_ap;
	double radius, angle, cos_angle, x, y; 
	point_t shoot_pt;
	vect_t shoot_dir;

	if (rdebug & RDEBUG_LIGHT ) bu_log("light_vis\n");

	/* compute the light direction */
	if (los->lp->lt_infinite ) {
		/* Infinite lights are point sources, no fuzzy penumbra */
		VMOVE( shoot_dir, los->lp->lt_vec );

	} else {

		/* We're going to shoot at a point on the apporximating
		 * sphere for the light source.  We pick a point on the 
		 * circle (presented area) for the light source from this
		 * angle.  This is done by picking random radius and angle 
		 * values on the disc.
		 */
		radius = los->lp->lt_radius * 
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


		VJOIN2(shoot_pt, los->lp->lt_pos, 
		       x, los->light_x,
		       y, los->light_y);

		VSUB2(shoot_dir, shoot_pt, los->swp->sw_hit.hit_point);
	}

	if (rdebug & RDEBUG_LIGHT) {
		VPRINT("shoot_dir", shoot_dir);
	}


	if (rdebug& RDEBUG_RAYPLOT) {
		point_t ray_endpt;

 /* Yelow -- light visibility ray */
		pl_color(stdout, 200, 200, 0);
		VADD2(ray_endpt, los->swp->sw_hit.hit_point, shoot_dir);
		pdv_3line(stdout, los->swp->sw_hit.hit_point, ray_endpt);
	}

	VUNITIZE( shoot_dir ); /* xxx We should just reuse to_light */



	/*
	 * See if ray from hit point to light lies within light beam
	 * Note: this is should always be true for infinite lights!
	 */
	if (-VDOT(shoot_dir, los->lp->lt_aim) < los->lp->lt_cosangle )  {
		/* dark (outside of light beam) */
		if (rdebug & RDEBUG_LIGHT)
			bu_log("point outside beam, obscured: %s\n",
			       los->lp->lt_name);
		return 0;
	}


	if (!(los->lp->lt_shadows) )  {
	       /* "fill light" in beam, don't care about shadows */
		if (rdebug & RDEBUG_LIGHT)
			bu_log("fill light, no shadow, visible: %s\n",
			       los->lp->lt_name);
#if RT_MULTISPECTRAL
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
	sub_ap.a_uptr = (genptr_t)los->lp;	/* so we can tell.. */
	sub_ap.a_level = 0;
	/* Will need entry & exit pts, for filter glass ==> 2 */
	/* Continue going through air ==> negative */
	sub_ap.a_onehit = -2;

	VSETALL( sub_ap.a_color, 1 );	/* vis intens so far */
	sub_ap.a_purpose = los->lp->lt_name;	/* name of light shot at */

	RT_CK_LIGHT((struct light_specific *)(sub_ap.a_uptr));
	RT_CK_AP(&sub_ap);

	if (rt_shootray( &sub_ap ) )  {
		/* light visible */
		if (rdebug & RDEBUG_LIGHT)
			bu_log("light visible: %s\n", los->lp->lt_name);

#if RT_MULTISPECTRAL
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
		bu_log("light obscured: %s\n", los->lp->lt_name);

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
light_obs(ap, swp, have)
struct application *ap;
struct shadework *swp;
int have;
{
	register struct light_specific *lp;
	register int	i;
	register fastf_t *tl_p;
	int vis_ray;
	int tot_vis_rays;
	int visibility;
	struct light_obs_stuff los;
	static int rand_idx;

	if (rdebug & RDEBUG_LIGHT )
		bu_log("computing Light obscuration: start\n");

	RT_CK_AP(ap);
	los.rand_idx = &rand_idx;
	los.ap = ap;
	los.swp = swp;

	/*
	 *  Determine light visibility
	 *
	 *  The sw_intensity field does NOT include the light's
	 *  emission spectrum (color), only path attenuation.
	 *  sw_intensity=(1,1,1) for no attenuation.
	 */
	tl_p = swp->sw_tolight;

	i = 0;
	for( BU_LIST_FOR( lp, light_specific, &(LightHead.l) ) )  {
		RT_CK_LIGHT(lp);

		if (rdebug & RDEBUG_LIGHT)
			bu_log("computing for light %d\n", i);
		swp->sw_lightfract[i] = 0.0;

		if (lp->lt_infinite || lp->lt_shadows == 0) tot_vis_rays = 1;
		else			tot_vis_rays = lp->lt_shadows;

		los.lp = lp;
#if RT_MULTISPECTRAL
		if(swp->msw_intensity[i]) BN_CK_TABDATA(swp->msw_intensity[i]);
		los.inten = &swp->msw_intensity[i];
#else
		los.inten = &swp->sw_intensity[3*i];
#endif

		/* create a coordinate system about the light center
		 * with the hitpoint->light ray as one of the axes
		 */
		VSUB2(los.to_light_center, lp->lt_pos, swp->sw_hit.hit_point);

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
					   lp->lt_name);
				continue;
			}
		}

		visibility = 0;
		for (vis_ray = 0 ; vis_ray < tot_vis_rays ; vis_ray ++) {
			los.iter = vis_ray;

			switch (light_vis(&los)) {
			case 1:
				/* remember the last ray that hit */
				VMOVE(tl_p, los.to_light_center);
				visibility++;
				break;
			case -1:
				/* this is our clue to give up on
				 * this light source.  Probably an infinite
				 * point light source.
				 */
				VMOVE(tl_p, los.to_light_center);
				visibility = vis_ray = tot_vis_rays;
				break;
			case -2:
				visibility = 0;
				vis_ray = tot_vis_rays;
				break;
			}
		}
		if (visibility) {
			swp->sw_visible[i] = (char *)lp;
			swp->sw_lightfract[i] =
				(fastf_t)visibility / (fastf_t)tot_vis_rays;
		} else {
			swp->sw_visible[i] = (char *)0;
		}

		/* Advance to next light */
		tl_p += 3;
		i++;
	}

	if (rdebug & RDEBUG_LIGHT ) bu_log("computing Light obscruration: end\n");
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
#if !RT_MULTISPECTRAL
	register fastf_t *intensity;
#endif
	register fastf_t *tolight;
	register fastf_t f;
	struct application sub_ap;

	if (rdebug & RDEBUG_LIGHT ) bu_log("computing Light visibility: start\n");

	/*
	 *  Determine light visibility
	 *
	 *  The sw_intensity field does NOT include the light's
	 *  emission spectrum (color), only path attenuation.
	 *  sw_intensity=(1,1,1) for no attenuation.
	 */
	i = 0;
#if !RT_MULTISPECTRAL
	intensity = swp->sw_intensity;
#endif
	tolight = swp->sw_tolight;
	for( BU_LIST_FOR( lp, light_specific, &(LightHead.l) ) )  {
		RT_CK_LIGHT(lp);
		/* compute the light direction */
		if (lp->lt_infinite ) {
			/* Infinite lights are point sources, no fuzzy penumbra */
			VMOVE( tolight, lp->lt_vec );
		} else {
			VSUB2(tolight, lp->lt_pos,
			    swp->sw_hit.hit_point);
#if 1
			/*
			 *  Dither light pos for penumbra by +/- 0.5 light 
			 *  radius; this presently makes a cubical light 
			 *  source distribution.
			 */
			f = lp->lt_radius * 0.7;
			tolight[X] = lp->lt_pos[X] +
			    bn_rand_half(ap->a_resource->re_randptr)*f -
			    swp->sw_hit.hit_point[X];
			tolight[Y] = lp->lt_pos[Y] +
			    bn_rand_half(ap->a_resource->re_randptr)*f -
			    swp->sw_hit.hit_point[Y];
			tolight[Z] = lp->lt_pos[Z] +
			    bn_rand_half(ap->a_resource->re_randptr)*f -
			    swp->sw_hit.hit_point[Z];
#endif

		}

		/*
		 *  If we have a normal, test against light direction
		 */
		if ((have & MFI_NORMAL) && (swp->sw_transmit <= 0) )  {
			if (VDOT(swp->sw_hit.hit_normal,tolight) < 0 ) {
				/* backfacing, opaque */
				if (rdebug & RDEBUG_LIGHT)
					bu_log("normal backfacing, opaque surface: %s\n", lp->lt_name);
				swp->sw_visible[i] = (char *)0;
				goto next;
			}
		}

		if (rdebug& RDEBUG_RAYPLOT) {
			point_t ray_endpt;

			/* Yelow -- light visibility ray */
			pl_color(stdout, 200, 200, 0);
			VADD2(ray_endpt, swp->sw_hit.hit_point,
			    tolight);
			pdv_3line(stdout, swp->sw_hit.hit_point,
			    ray_endpt);
		}
		VUNITIZE( tolight );

		/*
		 * See if ray from hit point to light lies within light beam
		 * Note: this is should always be true for infinite lights!
		 */
		if (-VDOT(tolight, lp->lt_aim) < lp->lt_cosangle )  {
			/* dark (outside of light beam) */
			if (rdebug & RDEBUG_LIGHT)
				bu_log("point outside beam, obscured: %s\n", lp->lt_name);

			swp->sw_visible[i] = (char *)0;
			goto next;
		}
		if (!(lp->lt_shadows) )  {
			/* "fill light" in beam, don't care about shadows */
			if (rdebug & RDEBUG_LIGHT)
				bu_log("fill light, no shadow, visible: %s\n", lp->lt_name);
			swp->sw_visible[i] = (char *)lp;
#if RT_MULTISPECTRAL
			/* XXX Need a power level for this! */
			bn_tabdata_constval( swp->msw_intensity[i], 1.0 );
#else
			VSETALL( intensity, 1 );
#endif
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
		sub_ap.a_rbeam = ap->a_rbeam + swp->sw_hit.hit_dist * ap->a_diverge;
		sub_ap.a_diverge = ap->a_diverge;

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


		if (rt_shootray( &sub_ap ) )  {
			/* light visible */
			if (rdebug & RDEBUG_LIGHT)
				bu_log("light visible: %s\n", lp->lt_name);
			swp->sw_visible[i] = (char *)lp;
#if RT_MULTISPECTRAL
			if (swp->msw_intensity[i] == BN_TABDATA_NULL) {
				swp->msw_intensity[i] = sub_ap.a_spectrum;
			} else {
				bu_bomb("Why are we multisampling?");

				bn_tabdata_add(swp->msw_intensity[i],
					       swp->msw_intensity[i],
					       sub_ap.a_spectrum);

				bn_tabdata_free(sub_ap.a_spectrum);
			}
			sub_ap.a_spectrum = BN_TABDATA_NULL;

#else
			VMOVE( intensity, sub_ap.a_color );
#endif
		} else {
			/* dark (light obscured) */
			if (rdebug & RDEBUG_LIGHT)
				bu_log("light obscured: %s\n", lp->lt_name);
			swp->sw_visible[i] = (char *)0;
		}
next:
		/* Advance to next light */
		i++;
#if RT_MULTISPECTRAL
		/* Release sub_ap? */
#else
		intensity += 3;
#endif
		tolight += 3;
	}

	if (rdebug & RDEBUG_LIGHT ) bu_log("computing Light visibility: end\n");
}


