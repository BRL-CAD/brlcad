/*
 *			C L O U D . C
 *
 * An attempt at 2D Geoffrey Gardner style cloud texture map
 *
 *
 *  Author -
 *	Phillip Dykstra
 *  
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1985 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static char RCScloud[] = "@(#)$Header$ (BRL)";
#endif

#include "conf.h"

#include <stdio.h>
#include <math.h>
#include "machine.h"
#include "vmath.h"
#include "raytrace.h"
#include "./material.h"
#include "./mathtab.h"
#include "./rdebug.h"

struct cloud_specific {
	fastf_t	cl_thresh;
	fastf_t	cl_range;
};
#define CL_NULL	((struct cloud_specific *)0)
#define CL_O(m)	offsetof(struct cloud_specific, m)

struct bu_structparse cloud_parse[] = {
	{"%f",	1, "thresh",	CL_O(cl_thresh),	FUNC_NULL },
	{"%f",	1, "range",	CL_O(cl_range),		FUNC_NULL },
	{"",	0, (char *)0,	0,			FUNC_NULL }
};

HIDDEN int	cloud_setup(), cloud_render();
HIDDEN void	cloud_print(), cloud_free();

struct mfuncs cloud_mfuncs[] = {
	{"cloud",	0,		0,		MFI_UV,		0,
	cloud_setup,	cloud_render,	cloud_print,	cloud_free },

	{(char *)0,	0,		0,		0,		0,
	0,		0,		0,		0 }
};

#define	NUMSINES	4

/*
 *			C L O U D _ T E X T U R E
 *
 * Returns the texture value for a plane point
 */
double
cloud_texture(x,y,Contrast,initFx,initFy)
register fastf_t x, y;
fastf_t Contrast, initFx, initFy;
{
	register int	i;
	FAST fastf_t	Px, Py, Fx, Fy, C;
	FAST fastf_t	t1, t2, k;

	t1 = t2 = 0;

	/*
	 * Compute initial Phases and Frequencies
	 * Freq "1" goes through 2Pi as x or y go thru 0.0 -> 1.0
	 */
	Fx = rt_twopi * initFx;
	Fy = rt_twopi * initFy;
	Px = rt_halfpi * tab_sin( 0.5 * Fy * y );
	Py = rt_halfpi * tab_sin( 0.5 * Fx * x );
	C = 1.0;	/* ??? */

	for( i = 0; i < NUMSINES; i++ ) {
		/*
		 * Compute one term of each summation.
		 */
		t1 += C * tab_sin( Fx * x + Px ) + Contrast;
		t2 += C * tab_sin( Fy * y + Py ) + Contrast;

		/*
		 * Compute the new phases and frequencies.
		 * N.B. The phases shouldn't vary the same way!
		 */
		Px = rt_halfpi * tab_sin( Fy * y );
		Py = rt_halfpi * tab_sin( Fx * x );
		Fx *= 2.0;
		Fy *= 2.0;
		C  *= 0.707;
	}

	/* Choose a magic k! */
	/* Compute max possible summation */
	k =  NUMSINES * 2 * NUMSINES;

	return( t1 * t2 / k );
}

/*
 *			C L O U D _ S E T U P
 */
HIDDEN int
cloud_setup( rp, matparm, dpp, mfp, rtip )
register struct region *rp;
struct rt_vls	*matparm;
char	**dpp;
struct mfuncs	*mfp;
struct rt_i	*rtip;
{
	register struct cloud_specific *cp;

	RT_VLS_CHECK( matparm );
	GETSTRUCT( cp, cloud_specific );
	*dpp = (char *)cp;

	cp->cl_thresh = 0.35;
	cp->cl_range = 0.3;
	if( bu_struct_parse( matparm, cloud_parse, (char *)cp ) < 0 )
		return(-1);

	return(1);
}

/*
 *			C L O U D _ P R I N T
 */
HIDDEN void
cloud_print( rp, dp )
register struct region *rp;
char	*dp;
{
	bu_struct_print( rp->reg_name, cloud_parse, (char *)dp );
}

/*
 *			C L O U D _ F R E E
 */
HIDDEN void
cloud_free( cp )
char *cp;
{
	rt_free( cp, "cloud_specific" );
}

/*
 *			C L O U D _ R E N D E R
 *
 * Return a sky color with translucency control.
 *  Threshold is the intensity below which it is completely translucent.
 *  Range in the range on intensities over which translucence varies
 *   from 0 to 1.
 *  thresh=0.35, range=0.3 for decent clouds.
 */
int
cloud_render( ap, pp, swp, dp )
struct application	*ap;
struct partition	*pp;
struct shadework	*swp;
char	*dp;
{
	register struct cloud_specific *cp =
		(struct cloud_specific *)dp;
	double intensity;
	FAST fastf_t	TR;

	intensity = cloud_texture( swp->sw_uv.uv_u, swp->sw_uv.uv_v,
		1.0, 2.0, 1.0 );

	/* Intensity is normalized - check bounds */
	if( intensity > 1.0 )
		intensity = 1.0;
	else if( intensity < 0.0 )
		intensity = 0.0;

	/* Compute Translucency Function */
	TR = 1.0 - ( intensity - cp->cl_thresh ) / cp->cl_range;
	if (TR < 0.0)
		TR = 0.0;
	else if (TR > 1.0)
		TR = 1.0;

	swp->sw_color[0] = ((1-TR) * intensity + (TR * .31));	/* Red */
	swp->sw_color[1] = ((1-TR) * intensity + (TR * .31));	/* Green */
	swp->sw_color[2] = ((1-TR) * intensity + (TR * .78));	/* Blue */
	return(1);
}
