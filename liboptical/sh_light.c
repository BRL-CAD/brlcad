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

#include <stdio.h>
#include <math.h>
#include "machine.h"
#include "vmath.h"
#include "raytrace.h"
#include "./material.h"
#include "./mathtab.h"
#include "./light.h"
#include "./rdebug.h"


struct matparse light_parse[] = {
	"inten",	(mp_off_ty)&(LIGHT_NULL->lt_intensity),	"%f",
	"fract",	(mp_off_ty)&(LIGHT_NULL->lt_fraction),	"%f",
	(char *)0,	(mp_off_ty)0,				(char *)0
};

struct light_specific *LightHeadp = LIGHT_NULL;		/* Linked list of lights */

extern double AmbientIntensity;

HIDDEN int light_setup(), light_render(), light_print();
int light_free();

struct mfuncs light_mfuncs[] = {
	"light",	0,		0,
	light_setup,	light_render,	light_print,	light_free,

	(char *)0,	0,		0,
	0,		0,		0,		0
};

/*
 *			L I G H T _ R E N D E R
 *
 *  If we have a direct view of the light, return it's color.
 */
HIDDEN int
light_render( ap, pp )
register struct application *ap;
register struct partition *pp;
{
	register struct light_specific *lp =
		(struct light_specific *)pp->pt_regionp->reg_udata;

	VSCALE( ap->a_color, lp->lt_color, lp->lt_fraction );
}

/*
 *			L I G H T _ S E T U P
 *
 *  Called once for each light-emitting region.
 */
HIDDEN int
light_setup( rp )
register struct region *rp;
{
	register struct light_specific *lp;

	GETSTRUCT( lp, light_specific );
	rp->reg_udata = (char *)lp;

	lp->lt_intensity = 1000.0;	/* Lumens */
	lp->lt_fraction = -1.0;		/* Recomputed later */
	lp->lt_explicit = 1;		/* explicitly modeled */
	mlib_parse( rp->reg_mater.ma_matparm, light_parse, (mp_off_ty)lp );

	/* Determine position and size */
	if( rp->reg_treetop->tr_op == OP_SOLID )  {
		register struct soltab *stp;

		stp = rp->reg_treetop->tr_a.tu_stp;
		VMOVE( lp->lt_pos, stp->st_center );
		lp->lt_radius = stp->st_aradius;
	} else {
		vect_t	min_rpp, max_rpp, avg, rad;

		VSETALL( min_rpp,  INFINITY );
		VSETALL( max_rpp, -INFINITY );
		rt_rpp_tree( rp->reg_treetop, min_rpp, max_rpp );
		VADD2( avg, min_rpp, max_rpp );
		VSCALE( avg, avg, 0.5 );
		VMOVE( lp->lt_pos, avg );
		VSUB2( rad, max_rpp, avg );
		lp->lt_radius = MAGNITUDE( rad );
	}
	if( rp->reg_mater.ma_override )  {
		VSET( lp->lt_color, rp->reg_mater.ma_rgb[0]/255.,
			rp->reg_mater.ma_rgb[1]/255.,
			rp->reg_mater.ma_rgb[2]/255. );
	} else {
		VSETALL( lp->lt_color, 1 );
	}
	VPRINT( "Light at", lp->lt_pos );
	VMOVE( lp->lt_vec, lp->lt_pos );
	VUNITIZE( lp->lt_vec );

	/* Add to linked list of lights */
	lp->lt_forw = LightHeadp;
	LightHeadp = lp;

	return(1);
}

/*
 *			L I G H T _ P R I N T
 */
HIDDEN int
light_print( rp )
register struct region *rp;
{
	mlib_print(rp->reg_name, light_parse, (mp_off_ty)rp->reg_udata);
}

/*
 *			L I G H T _ F R E E
 */
int
light_free( cp )
char *cp;
{

	if( LightHeadp == LIGHT_NULL )  {
		rt_log("light_free(x%x), list is null\n", cp);
		return;
	}
	if( LightHeadp == (struct light_specific *)cp )  {
		LightHeadp = LightHeadp->lt_forw;
	} else {
		register struct light_specific *lp;	/* current light */
		register struct light_specific **llp;	/* last light lt_forw */

		llp = &LightHeadp;
		lp = LightHeadp->lt_forw;
		while( lp != LIGHT_NULL )  {
			if( lp == (struct light_specific *)cp )  {
				*llp = lp->lt_forw;
				goto found;
			}
			llp = &(lp->lt_forw);
			lp = lp->lt_forw;
		}
		rt_log("light_free:  unable to find light in list\n");
	}
found:
	rt_free( cp, "light_specific" );
}

/*
 *			L I G H T _ M A K E R
 *
 *  Special hook called by view_2init to build 1 or 3 debugging lights.
 */
light_maker(num, v2m)
int	num;
mat_t	v2m;
{
	register struct light_specific *lp;
	register int i;
	vect_t	temp;
	vect_t	color;

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
		VMOVE( lp->lt_color, color );
		MAT4X3VEC( lp->lt_pos, v2m, temp );
		VMOVE( lp->lt_vec, lp->lt_pos );
		VUNITIZE( lp->lt_vec );
		lp->lt_intensity = 1000.0;
		lp->lt_radius = 10.0;		/* 10 mm */
		lp->lt_explicit = 0;		/* NOT explicitly modeled */
		lp->lt_forw = LightHeadp;
		LightHeadp = lp;
	}
}

/*
 *			L I G H T _ I N I T
 *
 *  Special routine called by view_2init() to determine the relative
 *  intensities of each light source.
 */
light_init()
{
	register struct light_specific *lp;
	int nlights = 0;
	fastf_t	inten = 0.0;

	for( lp = LightHeadp; lp; lp = lp->lt_forw )  {
		nlights++;
		if( lp->lt_fraction > 0 )  continue;	/* overridden */
		if( lp->lt_intensity <= 0 )
			lp->lt_intensity = 1;		/* keep non-neg */
		inten += lp->lt_intensity;
	}

	/* Compute total emitted energy, including ambient */
/**	inten *= (1 + AmbientIntensity); **/
	/* This is non-physical and risky, but gives nicer pictures for now */
	inten *= (1 + AmbientIntensity*0.5);

	for( lp = LightHeadp; lp; lp = lp->lt_forw )  {
		if( lp->lt_fraction > 0 )  continue;	/* overridden */
		lp->lt_fraction = lp->lt_intensity / inten;
	}
}
